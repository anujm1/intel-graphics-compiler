/*========================== begin_copyright_notice ============================

Copyright (c) 2000-2021 Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

============================= end_copyright_notice ===========================*/

#ifndef TARGET_GENX_H
#define TARGET_GENX_H
#include "visa_igc_common_header.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/GenXIntrinsics/GenXIntrinsics.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"
#include <string>

namespace llvm {

class BasicBlock;
class CallInst;
class Constant;
class DebugLoc;
class DominatorTree;
class formatted_raw_ostream;
class Function;
class FunctionGroup;
class FunctionGroupPass;
class FunctionPass;
class GenXSubtarget;
class Instruction;
class MDNode;
class ModulePass;
class ShuffleVectorInst;
class TargetOptions;
class Twine;
class Value;
class raw_ostream;
class raw_pwrite_stream;

enum BalingKind {
  BK_Legalization, // build baling info for legalization
  BK_CodeGen,      // build baling info for the final vISA emission
  BK_Analysis,     // build baling info for analysis (register pressure)
};

FunctionPass *createGenXPrinterPass(raw_ostream &O, const std::string &Banner);
FunctionGroupPass *createGenXGroupPrinterPass(raw_ostream &O, const std::string &Banner);
FunctionPass *createGenXAnalysisDumperPass(FunctionPass *Analysis, const char *Suffix);
FunctionGroupPass *createGenXGroupAnalysisDumperPass(FunctionGroupPass *Analysis, const char *Suffix);

FunctionPass *createGenXCFSimplificationPass();
ModulePass *createGenXEarlySimdCFConformancePass();
FunctionPass *createGenXReduceIntSizePass();
FunctionPass *createGenXInstCombineCleanup();
FunctionPass *createGenXInlineAsmLoweringPass();
FunctionPass *createGenXLoweringPass();
FunctionPass *createGenXLowerAggrCopiesPass();
FunctionPass *createGenXLowerJmpTableSwitchPass();
FunctionPass *createGenXGEPLoweringPass();
FunctionPass *createGenXRegionCollapsingPass();
FunctionPass *createGenXExtractVectorizerPass();
FunctionPass *createGenXRawSendRipperPass();
FunctionPass *createGenXFuncBalingPass(BalingKind Kind, GenXSubtarget *ST);
FunctionPass *createGenXPrologEpilogInsertionPass();
FunctionPass *createGenXLegalizationPass();
ModulePass *createGenXEmulatePass();
ModulePass *createGenXEmulationImportPass();
FunctionPass *createGenXDeadVectorRemovalPass();
FunctionPass *createGenXPatternMatchPass(const TargetOptions *Options);
FunctionPass *createGenXPostLegalizationPass();
FunctionPass *createTransformPrivMemPass();
ModulePass *createGenXThreadPrivateMemoryPass();
FunctionPass *createGenXPromotePredicatePass();
FunctionPass *createGenXIMadPostLegalizationPass();
FunctionPass *createGenXAggregatePseudoLoweringPass();
ModulePass *createGenXModulePass();
FunctionGroupPass *createGenXLateSimdCFConformancePass();
FunctionGroupPass *createGenXLivenessPass();
ModulePass *createGenXFunctionPointersLoweringPass();
FunctionGroupPass *createGenXCategoryPass();
FunctionGroupPass *createGenXGroupBalingPass(BalingKind Kind, GenXSubtarget *ST);
FunctionGroupPass *createGenXUnbalingPass();
FunctionGroupPass *createGenXDepressurizerPass();
FunctionGroupPass *createGenXLateLegalizationPass();
FunctionGroupPass *createGenXNumberingPass();
FunctionGroupPass *createGenXLiveRangesPass();
FunctionGroupPass *createGenXRematerializationPass();
FunctionGroupPass *createGenXCoalescingPass();
FunctionGroupPass *createGenXAddressCommoningPass();
FunctionGroupPass *createGenXArgIndirectionPass();
FunctionPass *createGenXTidyControlFlowPass();
FunctionGroupPass *createGenXVisaRegAllocPass();
FunctionGroupPass *createGenXCisaBuilderPass();
ModulePass *createGenXFinalizerPass(raw_pwrite_stream &o);
ModulePass *createGenXDebugInfoPass();
ModulePass *createGenXImportBiFPass();
ModulePass *createGenXGlobalVariableLoweringPass();

namespace genx {

// A local encoding (not part of vISA or GenX) of whether an operand should be signed.
enum Signedness {
  DONTCARESIGNED = 3, SIGNED = 1, UNSIGNED = 2
};

constexpr unsigned BoolBits  = 1;
constexpr unsigned ByteBits  = 8;
constexpr unsigned WordBits  = 16;
constexpr unsigned DWordBits = 32;
constexpr unsigned QWordBits = 64;
constexpr unsigned OWordBits = 128;

constexpr unsigned ByteBytes = ByteBits / ByteBits;
constexpr unsigned WordBytes = WordBits / ByteBits;
constexpr unsigned DWordBytes = DWordBits / ByteBits;
constexpr unsigned QWordBytes = QWordBits / ByteBits;
constexpr unsigned OWordBytes = OWordBits / ByteBits;

constexpr unsigned SurfaceElementBytes = 4;
constexpr unsigned SamplerElementBytes = 4;

// Currently EM determines behavior of 32 lanes.
// Probably that should be moved to subtarget if
// different targets will support different EM sizes.
constexpr unsigned TotalEMSize = 32;

// vISA allows [-512,511] for operation to be baled as offset
// for rdregion, copied from visa
constexpr int G4_MAX_ADDR_IMM = 511;
constexpr int G4_MIN_ADDR_IMM = -512;

// Default GRF Width if subtarget is not available
constexpr unsigned defaultGRFWidth = 32;

// describe integer vector immediate (V, UV)
enum ImmIntVec : int8_t {
  Width = 8, // num elem in vector
  ElemSize = 4, // in bits
  MaxUInt = (1 << ElemSize) - 1,
  MinUInt = 0,
  MaxSInt = (1 << (ElemSize - 1)) - 1,
  MinSInt = -(1 << (ElemSize - 1))
};


} // End genx namespace
} // End llvm namespace

#endif
