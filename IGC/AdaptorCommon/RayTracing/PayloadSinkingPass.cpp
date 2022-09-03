/*========================== begin_copyright_notice ============================

Copyright (C) 2021 Intel Corporation

SPDX-License-Identifier: MIT

============================= end_copyright_notice ===========================*/

//===----------------------------------------------------------------------===//
///
/// This pass will attempt to sink writes to the payload in closest-hit and
/// miss shaders into the inlined continuation (if it was inlined).
///
/// The goal is to eliminate loads in the inlined continuation that we already
/// know the value of. For example:
///
/// [shader("raygeneration")]
/// void MyRaygenShader()
/// {
///   RayPayload payload = { float4(0, 0, 0, 0) };
///   TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);
///   RenderTarget[DispatchRaysIndex().xy] = payload.color;
/// }
///
/// [shader("miss")]
/// void MyMissShader(inout RayPayload payload)
/// {
///   payload.color = float4(0, 0, 0, 1);
/// }
///
/// If we know the MaxTraceRecursionDepth == 1, we don't need to load the
/// payload pointer argument in the miss shader; we just need to find where
/// the payload is located in the SWStack of the raygen shader.
///
/// After inlining the continuation:
///
/// [shader("miss")]
/// void MyMissShader(inout RayPayload payload)
/// {
///   payload.color = float4(0, 0, 0, 1);
///   RenderTarget[DispatchRaysIndex().xy] = payload.color;
/// }
///
/// Which means we can just do:
///
/// [shader("miss")]
/// void MyMissShader(inout RayPayload payload)
/// {
///   payload.color = float4(0, 0, 0, 1);
///   RenderTarget[DispatchRaysIndex().xy] = float4(0, 0, 0, 1);
/// }
///
/// Later in compilation, it may even be possible to eliminate the payload.color
/// write if there is a StackIDRelease after it.
//===----------------------------------------------------------------------===//

#include "RTBuilder.h"
#include "Compiler/IGCPassSupport.h"
#include "iStdLib/utility.h"
#include "common/LLVMUtils.h"
#include "common/LLVMWarningsPush.hpp"
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/CFG.h>
#include <llvm/ADT/SmallPtrSet.h>
#include "common/LLVMWarningsPop.hpp"
#include "Probe/Assertion.h"
#include "ShaderProperties.h"
#include "Utils.h"

using namespace llvm;
using namespace IGC;
using namespace ShaderProperties;

class PayloadSinkingPass : public FunctionPass
{
public:
    PayloadSinkingPass() : FunctionPass(ID)
    {
        initializePayloadSinkingPassPass(*PassRegistry::getPassRegistry());
    }

    bool runOnFunction(Function &F) override;
    StringRef getPassName() const override
    {
        return "PayloadSinkingPass";
    }

    void getAnalysisUsage(llvm::AnalysisUsage &AU) const override
    {
        AU.setPreservesCFG();
        AU.addRequired<DominatorTreeWrapperPass>();
        AU.addRequired<CodeGenContextWrapper>();
    }

    static char ID;
private:
    bool canSink(const CodeGenContext &Ctx, Function& F) const;
};

char PayloadSinkingPass::ID = 0;

// Register pass to igc-opt
#define PASS_FLAG "payload-sinking"
#define PASS_DESCRIPTION "Sink payload stores into continuations"
#define PASS_CFG_ONLY false
#define PASS_ANALYSIS false
IGC_INITIALIZE_PASS_BEGIN(PayloadSinkingPass, PASS_FLAG, PASS_DESCRIPTION, PASS_CFG_ONLY, PASS_ANALYSIS)
IGC_INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
IGC_INITIALIZE_PASS_DEPENDENCY(CodeGenContextWrapper)
IGC_INITIALIZE_PASS_END(PayloadSinkingPass, PASS_FLAG, PASS_DESCRIPTION, PASS_CFG_ONLY, PASS_ANALYSIS)

// Given a starting block, return true if all paths must hit at least one
// `StopBB` on the way to the exit.
static bool mustExecute(
    const BasicBlock* From,
    const SmallPtrSetImpl<const BasicBlock*>& StopBBs)
{
    SmallPtrSet<const BasicBlock*, 32> Visited;
    SmallVector<const BasicBlock*, 8> WorkList{ From };

    do {
        auto* BB = WorkList.pop_back_val();
        if (!Visited.insert(BB).second)
            continue;
        if (StopBBs.count(BB) != 0)
            continue;
        if (isa<ReturnInst>(BB->getTerminator()))
            return false;
        WorkList.append(succ_begin(BB), succ_end(BB));
    } while (!WorkList.empty());

    return true;
}

// Conservatively find stores there are guaranteed to not alias each other.
// While we could opt for a heavier weight alias analysis if needed in the
// future, let's conservatively only sink stores that have constant offsets.
static SmallVector<StoreInst*, 4>
getNonAliasingStores(ArrayRef<PayloadUse> Uses, const DominatorTree *DT)
{
    SmallVector<StoreInst*, 4> Stores;
    for (auto& Use : Uses)
    {
        if (auto* SI = dyn_cast<StoreInst>(Use.I))
        {
            if (!std::any_of(Uses.begin(), Uses.end(), [&](const PayloadUse& A) {
                if (A.I == SI)
                    return false;

                if (!overlap(A.MemInterval, Use.MemInterval))
                    return false;

                return isPotentiallyReachable(SI, A.I, nullptr, DT);
            }))
            {
                Stores.push_back(SI);
            }
        }
    }

    return Stores;
}

// copies `SI` and all of its pointer arithmetic from the payload pointer
// base right after `Payload`. `Payload` is attached to be the new base of the
// pointer arithmetic.
static void sinkStore(StoreInst* SI, Instruction *Payload)
{
    Instruction* I = cast<Instruction>(SI->getPointerOperand());
    Instruction* CurI   = nullptr;
    Instruction* FirstI = Payload;
    while (!isa<PayloadPtrIntrinsic>(I))
    {
        CurI = I->clone();
        if (FirstI == Payload)
            FirstI = CurI;
        CurI->insertAfter(Payload);
        CurI->setName(VALUE_NAME(Twine(I->getName()) + ".sink"));
        I = cast<Instruction>(CurI->getOperand(0));
    }

    if (CurI)
        CurI->setOperand(0, Payload);

    auto* NewSI = cast<StoreInst>(SI->clone());
    NewSI->insertAfter(FirstI);
    NewSI->setOperand(1, FirstI);
}

// Determines whether stores in the function are eligible for sinking to
// continuations.
bool PayloadSinkingPass::canSink(
    const CodeGenContext &Ctx, Function& F) const
{
    ModuleMetaData* modMD = Ctx.getModuleMetaData();
    auto &FuncMD = modMD->FuncMD;

    auto MD = FuncMD.find(&F);
    if (MD == FuncMD.end())
        return false;

    auto& rtInfo = MD->second.rtInfo;
    auto ShaderTy = rtInfo.callableShaderType;

    // If this shader returns to a continuation, this guarantees that all the
    // inlined continuations collectively post dominate all payload writes
    // in the current shader.
    return (shaderReturnsToContinuation(ShaderTy) || ShaderTy == AnyHit) &&
            !rtInfo.isContinuation &&
            // Don't sink in callable since we don't know what the recursion
            // limit is. If there is 1, that is safe.
            (ShaderTy != Callable || modMD->rtInfo.NumContinuations == 1);
}

bool PayloadSinkingPass::runOnFunction(Function &F)
{
    auto *CGCtx = getAnalysis<CodeGenContextWrapper>().getCodeGenContext();

    if (!canSink(*CGCtx, F))
        return false;

    SmallVector<PayloadPtrIntrinsic*, 1> PayloadPtrs;
    SmallVector<ContinuationSignpostIntrinsic*, 4> Signposts;

    for (auto& I : instructions(F))
    {
        if (auto* PI = dyn_cast<PayloadPtrIntrinsic>(&I))
            PayloadPtrs.push_back(PI);
        else if (auto* SPI = dyn_cast<ContinuationSignpostIntrinsic>(&I))
            Signposts.push_back(SPI);
    }

    if (PayloadPtrs.size() != 1)
    {
        IGC_ASSERT_MESSAGE(PayloadPtrs.empty(), "this shouldn't happen!");
        return false;
    }

    if (Signposts.empty())
        return false;

    auto& DL = F.getParent()->getDataLayout();

    SmallVector<PayloadUse, 4> PayloadUses;
    if (!collectAnalyzablePayloadUses(PayloadPtrs[0], DL, PayloadUses, 0))
        return false;

    auto *DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
    auto Stores = getNonAliasingStores(PayloadUses, DT);

    bool Changed = false;
    IRBuilder<> IRB(F.getContext());

    for (auto* SI : Stores)
    {
        SmallVector<ContinuationSignpostIntrinsic*, 4> SinkLocs;
        SmallPtrSet<const BasicBlock*, 4> StopBBs;
        for (auto* I : Signposts)
        {
            if (DT->dominates(SI, I))
            {
                SinkLocs.push_back(I);
                StopBBs.insert(I->getParent());
            }
        }

        if (!mustExecute(SI->getParent(), StopBBs))
            continue;

        Changed = true;

        // Note: if we move toward a hybrid approach of inlining some
        // continuations and BTD to others, we'll need to tweak this to sink
        // stores to the BTDs as well.
        for (auto* Location : SinkLocs)
        {
            // Compute new payload pointer: FrameAddr + offset
            uint32_t Addrspace = Location->getType()->getPointerAddressSpace();
            IRB.SetInsertPoint(Location->getNextNode());
            Value* NewPayload =
                IRB.CreateBitCast(Location, IRB.getInt8PtrTy(Addrspace));

            NewPayload = IRB.CreateGEP(IRB.getInt8Ty(), NewPayload, Location->getOffset());
            NewPayload = IRB.CreateBitCast(NewPayload, PayloadPtrs[0]->getType());

            sinkStore(SI, cast<Instruction>(NewPayload));
        }

        SI->eraseFromParent();
    }

    return Changed;
}

namespace IGC
{

Pass* createPayloadSinkingPass(void)
{
    return new PayloadSinkingPass();
}

} // namespace IGC
