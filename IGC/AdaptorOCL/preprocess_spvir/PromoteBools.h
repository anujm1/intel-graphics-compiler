/*========================== begin_copyright_notice ============================

Copyright (C) 2022 Intel Corporation

SPDX-License-Identifier: MIT

============================= end_copyright_notice ===========================*/

#pragma once

#include "common/LLVMWarningsPush.hpp"
#include "llvmWrapper/IR/Module.h"
#include <llvm/Pass.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/IR/IRBuilder.h>
#include "common/LLVMWarningsPop.hpp"

#include "Compiler/MetaDataUtilsWrapper.h"

#include <string>
#include <queue>

#define VISIT_FUNCTION_IMPL(InstructionType) \
    void visit##InstructionType(llvm::InstructionType& instruction) \
    { \
        changed |= !wasPromoted(instruction.getFunction()) \
                   && getOrCreatePromotedValue(&instruction) != &instruction; \
    }

namespace IGC
{
    class PromoteBools : public llvm::ModulePass, public llvm::InstVisitor<PromoteBools>
    {
    public:
        static char ID;

        PromoteBools();
        ~PromoteBools() {}

        virtual llvm::StringRef getPassName() const override
        {
            return "PromoteBools";
        }

        virtual bool runOnModule(llvm::Module& module) override;

        VISIT_FUNCTION_IMPL(AllocaInst)
        VISIT_FUNCTION_IMPL(CallInst)
        VISIT_FUNCTION_IMPL(GetElementPtrInst)
        VISIT_FUNCTION_IMPL(LoadInst)
        VISIT_FUNCTION_IMPL(StoreInst)

    private:
        bool changed;

        llvm::Value* createZextIfNeeded(llvm::Value* argument, llvm::Instruction* insertBefore);
        void cleanUp(llvm::Module& module);

        // Checking if type needs promotion
        bool typeNeedsPromotion(llvm::Type* type, llvm::DenseSet<llvm::Type*> visitedTypes = {});

        // Promoting types
        llvm::DenseMap<llvm::Type*, llvm::Type*> promotedTypesCache;
        llvm::Type* getOrCreatePromotedType(llvm::Type* type);

        // Promoting values
        llvm::DenseMap<llvm::Value*, llvm::Value*> promotedValuesCache;
        llvm::DenseSet<llvm::Value*> doNotRemove;
        std::queue<llvm::Value*> promotionQueue;
        llvm::Value* getOrCreatePromotedValue(llvm::Value* value);
        bool wasPromoted(llvm::Value* value);
        llvm::Function* promoteFunction(llvm::Function* function);
        llvm::GlobalVariable* promoteGlobalVariable(llvm::GlobalVariable* globalVariable);
        llvm::Constant* promoteConstant(llvm::Constant* constant);
        llvm::AllocaInst* promoteAlloca(llvm::AllocaInst* alloca);
        llvm::AddrSpaceCastInst* promoteAddrSpaceCast(llvm::AddrSpaceCastInst* addrSpaceCast);
        llvm::Value* promoteBitCast(llvm::BitCastInst* bitcast);
        llvm::CallInst* promoteCall(llvm::CallInst* call);
        llvm::ExtractValueInst* promoteExtractValue(llvm::ExtractValueInst* extractValue);
        llvm::GetElementPtrInst* promoteGetElementPtr(llvm::GetElementPtrInst* getElementPtr);
        llvm::ICmpInst* promoteICmp(llvm::ICmpInst* icmp);
        llvm::InsertValueInst* promoteInsertValue(llvm::InsertValueInst* insertValue);
        llvm::LoadInst* promoteLoad(llvm::LoadInst* load);
        llvm::PHINode* promotePHI(llvm::PHINode* phi);
        llvm::StoreInst* promoteStore(llvm::StoreInst* store);

        // Promoting values - helping vars
        llvm::DenseSet<llvm::PHINode*> visitedPHINodes;
    };
}
