/*========================== begin_copyright_notice ============================

Copyright (C) 2017-2021 Intel Corporation

SPDX-License-Identifier: MIT

============================= end_copyright_notice ===========================*/

#pragma once

#include "IGC/common/StringMacros.hpp"
#include "usc.h"
#include "usc_gen7.h"
#include "usc_gen9.h"
#include "common/Stats.hpp"
#include "common/Types.hpp"
#include "common/allocator.h"
#include "common/igc_resourceDimTypes.h"
// hack
#include "common/debug/Debug.hpp"
#include "common/debug/Dump.hpp"
#include <set>
#include <string.h>
#include <sstream>
#include "Compiler/CISACodeGen/ShaderUnits.hpp"
#include "Compiler/CISACodeGen/Platform.hpp"
#include "Compiler/CISACodeGen/DriverInfo.hpp"
#include "Compiler/CISACodeGen/helper.h"
#include "Compiler/MetaDataApi/MetaDataApi.h"
#include "Compiler/MetaDataApi/IGCMetaDataHelper.h"
#include "Compiler/CodeGenContextWrapper.hpp"
#include "visa/include/RelocationInfo.h"
#include "ZEBinWriter/zebin/source/autogen/ZEInfo.hpp"

#include "../AdaptorOCL/OCL/sp/spp_g8.h"
#include "../GenISAIntrinsics/GenIntrinsics.h"
#include "../GenISAIntrinsics/GenIntrinsicInst.h"
#include "common/LLVMWarningsPush.hpp"
#include <llvm/IR/LLVMContext.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/IRBuilder.h>
#include "llvm/IR/Function.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/IR/AssemblyAnnotationWriter.h"
#include "common/LLVMWarningsPop.hpp"
#include "CodeGenPublicEnums.h"
#include "AdaptorOCL/TranslationBlock.h"
#include "common/MDFrameWork.h"
#include "CompilerStats.h"
#include <unordered_set>
#include "Probe/Assertion.h"
#include <optional>
#include <Metrics/IGCMetric.h>

/************************************************************************
This file contains the interface structure and functions to communicate
between front ends and code generator
************************************************************************/

namespace llvm
{
    class Module;
    class Function;
}

#define MAX_VSHADER_INPUT_REGISTERS_PACKAGEABLE 32
static const unsigned int g_c_Max_PS_attributes = 32;

namespace IGC
{
    class CodeGenContext;

    struct SProgramOutput
    {
    public:
        typedef std::vector<vISA::ZESymEntry> SymbolListTy;
        typedef std::vector<vISA::ZERelocEntry> RelocListTy;
        typedef std::vector<vISA::ZEFuncAttribEntry> FuncAttrListTy;
        // function scope symbols
        struct ZEBinFuncSymbolTable {
            SymbolListTy function;          // function symbols
            SymbolListTy sampler;           // sampler symbols
            SymbolListTy local;             // local symbols
        };
        // function scope gtpin info
        struct ZEBinFuncGTPinInfo {
            std::string name;
            void* buffer = nullptr;
            unsigned bufferSize = 0;
        };
        typedef std::vector<ZEBinFuncGTPinInfo> FuncGTPinInfoListTy;

    public:
        void* m_programBin = nullptr;     //<! Must be 16 byte aligned, and padded to a 64 byte boundary
        unsigned int    m_programSize = 0;    //<! Number of bytes of program data (including padding)
        unsigned int    m_unpaddedProgramSize = 0;      //<! program size without padding used for binary linking
        unsigned int    m_startReg = 0;                 //<! Which GRF to start with
        unsigned int    m_scratchSpaceUsedBySpills = 0; //<! amount of scratch space needed for shader spilling
        unsigned int    m_scratchSpaceUsedByShader = 0; //<! amount of scratch space needed by shader
        unsigned int    m_scratchSpaceUsedByGtpin = 0; //<! amount of scratch space used by gtpin
        void*           m_debugData = nullptr;      //<! elf file containing debug information for the kernel (source->genIsa)
        unsigned int    m_debugDataSize = 0;        //<! size of the elf file containing debug information
        // TODO: m_debugDataGenISA and m_debugDataGenISASize
        // are not really needed, consider removal
        void* m_debugDataGenISA = nullptr;          //<! GenISA debug data (VISA -> GenISA)
        unsigned int    m_debugDataGenISASize = 0;      //<! Number of bytes of GenISA debug data
        unsigned int    m_InstructionCount = 0;
        unsigned int    m_BasicBlockCount = 0;
        void* m_gtpinBuffer = nullptr;              // Will be populated by VISA only when special switch is passed by gtpin
        unsigned int    m_gtpinBufferSize = 0;
        FuncGTPinInfoListTy m_FuncGTPinInfoList;
        void* m_funcSymbolTable = nullptr;
        unsigned int    m_funcSymbolTableSize = 0;
        unsigned int    m_funcSymbolTableEntries = 0;
        ZEBinFuncSymbolTable m_symbols;           // duplicated information of m_funcSymbolTable, for zebin
        void* m_funcRelocationTable = nullptr;
        unsigned int    m_funcRelocationTableSize = 0;
        unsigned int    m_funcRelocationTableEntries = 0;
        RelocListTy     m_relocs;                  // duplicated information of m_funcRelocationTable, for zebin
        void* m_funcAttributeTable = nullptr;
        unsigned int    m_funcAttributeTableSize = 0;
        unsigned int    m_funcAttributeTableEntries = 0;
        FuncAttrListTy  m_funcAttrs;               // duplicated information of m_funcAttributeTable, for zebin
        void* m_globalHostAccessTable = nullptr;
        unsigned int    m_globalHostAccessTableSize = 0;
        unsigned int    m_globalHostAccessTableEntries = 0;
        unsigned int    m_offsetToSkipPerThreadDataLoad = 0;
        uint32_t        m_offsetToSkipSetFFIDGP = 0;
        bool            m_roundPower2KBytes = false;
        bool            m_UseScratchSpacePrivateMemory = true;
        bool            m_SeparatingSpillAndPrivateScratchMemorySpace = false;
        unsigned int m_scratchSpaceSizeLimit = 0;
        unsigned int m_numGRFTotal = 128;
        unsigned int m_numGRFSpillFill = 0;
        using NamedVISAAsm = std::pair<std::string, std::string>; // Pair of name for the section (1st elem) and VISA asm text (2nd elem).
        std::vector<NamedVISAAsm> m_VISAAsm;

        // Optional statistics
        std::optional<uint64_t> m_NumGRFSpill;
        std::optional<uint64_t> m_NumGRFFill;
        std::optional<uint64_t> m_NumSends;
        std::optional<uint64_t> m_NumCycles;
        std::optional<uint64_t> m_NumSendStallCycles;

        unsigned int m_numThreads = 0;

        void Destroy()
        {
            if (m_programBin)
            {
                IGC::aligned_free(m_programBin);
            }
            if (m_debugData)
            {
                IGC::aligned_free(m_debugData);
            }
            if (m_debugDataGenISA)
            {
                IGC::aligned_free(m_debugDataGenISA);
            }
            if (m_funcAttributeTable)
            {
                IGC::aligned_free(m_funcAttributeTable);
            }
            if (m_funcSymbolTable)
            {
                free(m_funcSymbolTable);
            }
            if (m_funcRelocationTable)
            {
                free(m_funcRelocationTable);
            }
            if (m_globalHostAccessTable)
            {
                free(m_globalHostAccessTable);
            }
        }

        void init(bool roundPower2KBytes, unsigned int scratchSpaceSizeLimitT, bool useScratchSpacePrivateMemory, bool SepSpillPvtSS)
        {
            m_roundPower2KBytes = roundPower2KBytes;
            m_scratchSpaceSizeLimit = scratchSpaceSizeLimitT;
            m_UseScratchSpacePrivateMemory = useScratchSpacePrivateMemory;
            m_SeparatingSpillAndPrivateScratchMemorySpace = SepSpillPvtSS;
        }

        // if IGC needs scratch for private memory, we use slot0 for private
        // if IGC does not need scratch for private, slot0 is used for spill
        // if we want to use both private and spill in single slot, we need
        // to add them together
        unsigned int getScratchSpaceUsageInSlot0() const
        {
            unsigned int result = (m_UseScratchSpacePrivateMemory ? m_scratchSpaceUsedByShader : 0);
            if (result == 0)
            {
                result = (m_scratchSpaceUsedBySpills + m_scratchSpaceUsedByGtpin);
            }
            else if (!m_SeparatingSpillAndPrivateScratchMemorySpace)
            {
                result += (m_scratchSpaceUsedBySpills + m_scratchSpaceUsedByGtpin);
            }
            else
            {
                // \TODO: doubts about driver-compiler interface, conservatively set the size
                // to the max of two slots
                result = std::max(result, m_scratchSpaceUsedBySpills + m_scratchSpaceUsedByGtpin);
            }
            result = roundSize(result);
            IGC_ASSERT(result <= m_scratchSpaceSizeLimit);
            return result;
        }
        // slot1 is used for spilling only when m_SeparatingSpillAndPrivateScratchMemorySpace is on
        // and Slot0 is used for IGC private memory
        unsigned int getScratchSpaceUsageInSlot1() const
        {
            unsigned int slot0_offset = (m_UseScratchSpacePrivateMemory ? m_scratchSpaceUsedByShader : 0);
            unsigned int result = 0;
            if (m_SeparatingSpillAndPrivateScratchMemorySpace && slot0_offset > 0)
            {
                // \TODO: doubts about driver-compiler interface, conservatively set the size
                // to the max of two slots
                result = std::max(slot0_offset, m_scratchSpaceUsedBySpills + m_scratchSpaceUsedByGtpin);
            }
            result = roundSize(result);
            IGC_ASSERT(result <= m_scratchSpaceSizeLimit);
            return result;
        }

        unsigned int getScratchSpaceUsageInStateless() const
        {
            return roundSize(!m_UseScratchSpacePrivateMemory ? m_scratchSpaceUsedByShader : 0);
        }

        void setScratchSpaceUsedByShader(unsigned int scratchSpaceUsedByShader)
        {
            m_scratchSpaceUsedByShader = scratchSpaceUsedByShader;
        }
    private:
        unsigned int roundSize(unsigned int size) const
        {
            if (m_roundPower2KBytes)
            {
                size = roundPower2KBbyte(size);
            }
            else
            {
                size = roundPower2Byte(size);
            }
            return size;
        }

        unsigned int roundPower2KBbyte(unsigned int size) const
        {
            return (size ? iSTD::RoundPower2(iSTD::Max(int_cast<DWORD>(size), static_cast<DWORD>(sizeof(KILOBYTE)))) : 0);
        }

        // XeHP_SDV+ : we round to one of values: pow(2, (0, 6, 7, 8...18))
        unsigned int roundPower2Byte(unsigned int size) const
        {
            unsigned int ret = (size ? iSTD::RoundPower2(int_cast<DWORD>(size)) : 0);
            //round any value in (0,32] to 64 BYTEs
            ret = ((ret > 0 && ret <= 32) ? 64 : ret);
            return ret;
        }
    };

    enum InstrStatTypes
    {
        SROA_PROMOTED,
        LICM_STAT,
        TOTAL_TYPES
    };
    enum InstrStatStage
    {
        BEGIN,
        END,
        EXCEED_THRESHOLD,
        TOTAL_STAGE
    };

    struct SInstrTypes
    {
        bool CorrelatedValuePropagationEnable;
        bool hasMultipleBB;
        bool hasCmp;
        bool hasSwitch;
        bool hasPhi;
        bool hasLoadStore;
        bool hasIndirectCall;
        bool hasInlineAsm;
        bool hasInlineAsmPointerAccess;
        bool hasIndirectBranch;
        bool hasFunctionAddressTaken;
        bool hasSel;
        bool hasPointer;
        bool hasLocalLoadStore;
        bool hasGlobalLoad; // has (stateless) loads from global addresspace
        bool hasGlobalStore; // has (stateless) stores to global addresspace
        bool hasStorageBufferLoad; // has (stateful) loads from storage buffers (UAV/SSBO)
        bool hasStorageBufferStore; // has (stateful) stores to storage buffers (UAV/SSBO)
        bool hasSubroutines;
        bool hasPrimitiveAlloca;
        bool hasNonPrimitiveAlloca;
        bool hasReadOnlyArray;
        bool hasBuiltin;
        bool hasFRem;
        bool psHasSideEffect;     //<! only relevant to pixel shader, has other memory writes besides RTWrite
        bool hasGenericAddressSpacePointers;
        bool hasDebugInfo;        //<! true only if module contains debug info !llvm.dbg.cu
        bool hasAtomics;
        bool hasDiscard;
        bool hasTypedRead;
        bool hasTypedwrite;
        bool mayHaveIndirectOperands;  //<! true if code may have indirect operands like r5[a0].
        // true if shader may have indirect texture or buffer.
        // Note: does not check for indirect sampler
        bool mayHaveIndirectResources;
        bool hasUniformAssumptions;
        bool hasPullBary;
        bool sampleCmpToDiscardOptimizationPossible;
        bool hasRuntimeValueVector;
        unsigned int numCall;
        unsigned int numBarrier;
        unsigned int numLoadStore;
        unsigned int numWaveIntrinsics;
        unsigned int numAtomics;
        unsigned int numTypedReadWrite;
        unsigned int numAllInsts;
        unsigned int sampleCmpToDiscardOptimizationSlot;
        unsigned int numSample;
        unsigned int numBB;
        unsigned int numLoopInsts;
        unsigned int numOfLoop;
        unsigned int numInsts;    //<! measured after optimization, used as a compiler heuristic
        unsigned int numAllocaInsts;
        unsigned int numPsInputs;
        bool hasDynamicGenericLoadStore;
        bool hasUnmaskedRegion;
        unsigned int numGlobalInsts;
        unsigned int numLocalInsts;
    };

    struct SSimplePushInfo
    {
        // Constant buffer Binding Table Index or Surface State Offset.
        // Valid only if 'isStateless' is false.
        // If 'isBindless' is false then 'm_cbIdx' contains a Binding Table
        // Index otherwise it contains a Surface State Offset in 64-byte units.
        uint m_cbIdx = 0;
        // m_pushableAddressGrfOffset and m_pushableOffsetGrfOffset are GRF
        // offsets (in DWORDS) in the runtime data pushed to the shader. These
        // fields are valid only if greater or equal to 0. If a field is valid
        // it means that the runtime data from the GRF offset was used in
        // the buffer address calculation.
        // These fields must contain values provided by frontend in
        // pushInfo.pushableAddresses metadata.
        // m_pushableAddressGrfOffset is only valid when isStateless is true.
        // m_pushableOffsetGrfOffset is only valid when isStateless or
        // isBindless is true.
        // When isStateless is true runtime data at m_pushableAddressGrfOffset
        // contains a 64bit canonicalized address. Data starting at
        // m_pushableOffsetGrfOffset contains 32bit offset relative to the 64bit
        // starting address.
        // PushAnalysiss pass matches the following pattern:
        //   uint8_t* pShaderRuntimeData ={...}; // to be pushed
        //   uint64_t pushableAddress =
        //     *(uint64_t*)(pShaderRuntimeData + 4*pushableAddressGrfOffset);
        //   if (pushableOffsetGrfOffset >=0) {
        //     pushableAddress +=
        //       *(uint32_t*)(pShaderRuntimeData + 4*pushableOffsetGrfOffset);
        //   }
        //   pushableAddress += m_offset;
        //
        // m_pushableOffsetGrfOffset is also used when isBindless is true and
        // contains the GRF offset that was used to calculate the Surface State
        // Offset of the buffer. It must contain one of the values provided by
        // frontend in pushInfo.bindlessPushInfo metadata.
        int m_pushableAddressGrfOffset = -1;
        int m_pushableOffsetGrfOffset = -1;
        // Immediate offset in bytes add to the start of the simple push region.
        uint m_offset = 0;
        // Data size in bytes, must be a multiple of GRF size
        uint m_size = 0;
        bool isStateless = false;
        bool isBindless = false;
    };

    struct ConstantPayloadInfo
    {
        int  DerivedConstantsOffset = -1;
    };


    struct SResInfoFoldingOutput
    {
        uint32_t textureID;
        bool value[4];
    };

    enum SIMDInfoBit
    {
        SIMD_SELECTED,       // 0: if the SIMD is selected. If 1, all the other bits are ignored.
        SIMD_RETRY,          // 1: is a retry
        SIMD_SKIP_HW,        // 2: skip this SIMD due to HW restriction / WA.
        SIMD_SKIP_REGPRES,   // 3: skip this SIMD due to register pressure early out.
        SIMD_SKIP_SPILL,     // 4: skip this SIMD due to spill or high chance of spilling.
        SIMD_SKIP_STALL,     // 5: skip this SIMD due to stall cycle or thread occupancy heuristic.
        SIMD_SKIP_THGRPSIZE, // 6: skip due to threadGroupSize heuristic(CS / OCL only).
        SIMD_SKIP_PERF       // 7: skip this SIMD due to performance concern (dx12 + discard, MRT, etc) or other reasons.
    };

    enum SIMDInfoOffset
    {
        SIMD8_OFFSET = 0,
        SIMD16_OFFSET = 8,
        SIMD32_OFFSET = 16,
    };

    struct SKernelProgram
    {
        SProgramOutput simd1;
        SProgramOutput simd8;
        SProgramOutput simd16;
        SProgramOutput simd32;
        unsigned int bindingTableEntryCount = 0;

        char* gatherMap = nullptr;
        unsigned int gatherMapSize = 0;
        unsigned int ConstantBufferLength = 0;
        unsigned int ConstantBufferMask   = 0;
        unsigned int MaxNumberOfThreads   = 0;
        bool         isMessageTargetDataCacheDataPort = false;

        unsigned int NOSBufferSize = 0;
        unsigned int ConstantBufferLoaded = 0;
        uint64_t     UavLoaded = 0;
        unsigned int ShaderResourceLoaded[4];
        unsigned int RenderTargetLoaded = 0;

        bool         hasControlFlow = false;
        unsigned int bufferSlot = 0;
        unsigned int statelessCBPushedSize = 0;

        bool         hasEvalSampler = false;
        std::vector<SResInfoFoldingOutput> m_ResInfoFoldingOutput;
        // GenUpdateCB outputs
        void*       m_ConstantBufferReplaceShaderPatterns = nullptr;
        uint        m_ConstantBufferReplaceShaderPatternsSize = 0;
        uint        m_ConstantBufferUsageMask = 0;
        uint        m_ConstantBufferReplaceSize = 0;

        SSimplePushInfo simplePushInfoArr[g_c_maxNumberOfBufferPushed];

        uint64_t    SIMDInfo;
        void* m_StagingCtx;
        bool m_RequestStage2;
    };

    /// Gen10+, corresponds to 3DSTATE_VF_SGVS_2 as described below
    struct SVertexFetchSGVExtendedParameters
    {
        struct
        {
            bool enabled = false;      //<! XPn Enable = XPn Source Select = (*)
            unsigned int location = 0; //<! Linear offset of the 32bit component in VUE
        } extendedParameters[3] = {};  //<! Order of elements: XP0, XP1, XP2
    };

    // XeHPC/XeHPG Task/Mesh Extended Parameters: XP0 (DrawID), XP1, XP2
    struct SMeshExtendedParameters
    {
        static constexpr size_t             drawIdXPIndex = 0;
        bool                                enabled[3] = {};
    };

    struct SBindlessProgram : SKernelProgram
    {
        SProgramOutput simdProgram;
        USC::GFXMEDIA_GPUWALKER_SIMD SimdWidth;
        std::string name;
        uint32_t ShaderStackSize = 0;
        CallableShaderTypeMD ShaderType = NumberOfCallableShaderTypes;
        bool isContinuation = false;
        // if 'isContinuation' is true, this will contain the name of the
        // original shader.
        std::string ParentName;
        // if 'isContinuation' is true, this may contain the slot num for the
        // shader identifier it has been promoted to.
        std::optional<uint32_t> SlotNum;
        uint64_t ShaderHash = 0;

        // raygen specific fields
        // TODO: need to separate out bindless and raygen into two structs
        // for both DX and Vulkan.

        void*         ThreadPayloadData = nullptr;
        unsigned int  TotalDataLength   = 0;
        // dynamically select between the 1D and 2D layout at runtime based
        // on the size of the dispatch.
        uint32_t      DimX1D            = 0;
        uint32_t      DimY1D            = 0;
        uint32_t      DimX2D            = 0;
        uint32_t      DimY2D            = 0;

        // Shaders that satisfy `isPrimaryShaderIdentifier()` can also have
        // a collection of other names that they go by.
        std::vector<std::string> Aliases;

        // We maintain this information to provide to GTPin. These are all
        // offsets in bytes from the base of GRF.
        uint32_t GlobalPtrOffset = 0; // pointer to RTGlobals
        uint32_t LocalPtrOffset  = 0; // pointer to local root sig (except for raygen!)
        uint32_t StackIDsOffset  = 0; // stack ID vector base

        // Shader has LSC store messages with non-default L1 cache control
        bool HasLscStoresWithNonDefaultL1CacheControls = false;
    };

    struct SRayTracingShadersGroup
    {
        // This is the default shader that is executed when the RTUnit
        // encounters a null shader. It is optional because there is
        // no need to compile it for collection state objects.
        llvm::Optional<SBindlessProgram> callStackHandler;
        // These are the raygen shaders
        llvm::SmallVector<SBindlessProgram, 4> m_DispatchPrograms;
        // Non raygen shaders
        llvm::SmallVector<SBindlessProgram, 8> m_CallableShaders;
        // Continuation shaders
        llvm::SmallVector<SBindlessProgram, 8> m_Continuations;
    };

    struct SRayTracingPipelineConfig
    {
        unsigned int maxTraceRecursionDepth = 0;
        unsigned int pipelineFlags = 0;
    };

    struct SRayTracingShaderConfig
    {
        unsigned MaxPayloadSizeInBytes = 0;
        unsigned MaxAttributeSizeInBytes = 0;
    };

    struct SOpenCLKernelInfo
    {
        struct SResourceInfo
        {
            enum { RES_UAV, RES_SRV, RES_OTHER } Type;
            int Index;
        };

        SOpenCLKernelInfo() {};

        std::string m_kernelName = {};
        QWORD       m_ShaderHashCode = {};

        std::vector<std::unique_ptr<iOpenCL::PointerInputAnnotation>>       m_pointerInput;
        std::vector<std::shared_ptr<iOpenCL::PointerArgumentAnnotation>>    m_pointerArgument;
        std::vector<std::unique_ptr<iOpenCL::LocalArgumentAnnotation>>      m_localPointerArgument;
        std::vector<std::unique_ptr<iOpenCL::SamplerInputAnnotation>>       m_samplerInput;
        std::vector<std::unique_ptr<iOpenCL::SamplerArgumentAnnotation>>    m_samplerArgument;
        std::vector<std::unique_ptr<iOpenCL::ConstantInputAnnotation>>      m_constantInputAnnotation;
        std::vector<std::unique_ptr<iOpenCL::ConstantArgumentAnnotation>>   m_constantArgumentAnnotation;
        std::vector<std::unique_ptr<iOpenCL::ImageArgumentAnnotation>>      m_imageInputAnnotations;
        std::vector<std::unique_ptr<iOpenCL::KernelArgumentInfoAnnotation>> m_kernelArgInfo;
        std::vector<std::unique_ptr<iOpenCL::PrintfStringAnnotation>>       m_printfStringAnnotations;

        std::unique_ptr<iOpenCL::PrintfBufferAnnotation>    m_printfBufferAnnotation = nullptr;
        std::unique_ptr<iOpenCL::SyncBufferAnnotation>      m_syncBufferAnnotation = nullptr;
        std::unique_ptr<iOpenCL::RTGlobalBufferAnnotation>  m_rtGlobalBufferAnnotation = nullptr;
        std::unique_ptr<iOpenCL::StartGASAnnotation>        m_startGAS = nullptr;
        std::unique_ptr<iOpenCL::WindowSizeGASAnnotation>   m_WindowSizeGAS = nullptr;
        std::unique_ptr<iOpenCL::PrivateMemSizeAnnotation>  m_PrivateMemSize = nullptr;
        std::string                                         m_kernelAttributeInfo = {};

        bool                                                m_HasInlineVmeSamplers = false;

        // This maps argument numbers to BTI and sampler indices
        // (e.g. kernel argument 3, which is is an image_2d, may be mapped to BTI 6)
        std::map<DWORD, unsigned int> m_argIndexMap = {};

        std::map<unsigned int, std::shared_ptr<iOpenCL::PointerArgumentAnnotation>> m_argOffsetMap = {};

        iOpenCL::ThreadPayload        m_threadPayload = {};

        iOpenCL::ExecutionEnvironment m_executionEnvironment = {};

        iOpenCL::KernelTypeProgramBinaryInfo m_kernelTypeInfo = {};

        SKernelProgram                m_kernelProgram = {};

        // ----- Information for zebin ----- //
        // Cross-thread payload arguments
        zebin::PayloadArgumentsTy m_zePayloadArgs;
        // BTI information for payload arguments
        zebin::BindingTableIndicesTy m_zeBTIArgs;
        // Kernel attributes. zeinfo's user_attributes of kernels
        zebin::zeInfoUserAttribute m_zeUserAttributes;
        // Kernel args info
        zebin::ArgsInfoTy m_zeKernelArgsInfo;
        // Inline samplers
        zebin::InlineSamplersTy m_zeInlineSamplers;

        // Analysis result of if there are non-kernel-argument ld/st in the kernel
        // If all false, we can avoid expensive memory setting of each kernel during runtime
        int m_hasNonKernelArgLoad = -1;
        int m_hasNonKernelArgStore = -1;
        int m_hasNonKernelArgAtomic = -1;
    };

    struct SOpenCLProgramInfo
    {
        struct ZEBinRelocTable
        {
            std::vector<vISA::ZERelocEntry> globalReloc;
            std::vector<vISA::ZERelocEntry> globalConstReloc;
        };
        // program scope symbols
        struct ZEBinProgramSymbolTable
        {
            using SymbolSeq = std::vector<vISA::ZESymEntry>;
            SymbolSeq global;            // global symbols
            SymbolSeq globalConst;       // global constant symbols
            SymbolSeq globalStringConst; // global string constant symbols
        };
        struct LegacySymbolTable
        {
            void* m_buffer = nullptr;
            unsigned int m_size = 0;
            unsigned int m_entries = 0;
        };

        typedef std::vector<vISA::ZEHostAccessEntry> ZEBinGlobalHostAccessTable;

        std::unique_ptr<iOpenCL::InitConstantAnnotation> m_initConstantAnnotation;
        std::unique_ptr<iOpenCL::InitConstantAnnotation> m_initConstantStringAnnotation;
        std::unique_ptr<iOpenCL::InitGlobalAnnotation> m_initGlobalAnnotation;
        std::vector<std::unique_ptr<iOpenCL::ConstantPointerAnnotation> > m_initConstantPointerAnnotation;
        std::vector<std::unique_ptr<iOpenCL::GlobalPointerAnnotation> > m_initGlobalPointerAnnotation;
        std::vector<std::unique_ptr<iOpenCL::KernelTypeProgramBinaryInfo> > m_initKernelTypeAnnotation;

        ZEBinRelocTable m_GlobalPointerAddressRelocAnnotation;
        ZEBinProgramSymbolTable m_zebinSymbolTable;
        LegacySymbolTable m_legacySymbolTable;
        ZEBinGlobalHostAccessTable m_zebinGlobalHostAccessTable;
        bool m_hasCrossThreadOffsetRelocations = false;
    };

    class CBTILayout
    {
    public:
        unsigned int GetSystemThreadBindingTableIndex(void) const;
        unsigned int GetBindingTableEntryCount(void) const;
        unsigned int GetTextureIndex(unsigned int index) const;
        unsigned int GetUavIndex(unsigned int index) const;
        unsigned int GetRenderTargetIndex(unsigned int index) const;
        unsigned int GetConstantBufferIndex(unsigned int index) const;
        unsigned int GetTextureIndexSize() const { return m_pLayout->maxResourceIdx - m_pLayout->minResourceIdx; }
        unsigned int GetUavIndexSize() const { return m_pLayout->maxUAVIdx - m_pLayout->minUAVIdx; }
        unsigned int GetRenderTargetIndexSize() const { return m_pLayout->maxColorBufferIdx - m_pLayout->minColorBufferIdx; }
        unsigned int GetConstantBufferIndexSize() const { return m_pLayout->maxConstantBufferIdx - m_pLayout->minConstantBufferIdx; }
        unsigned int GetNullSurfaceIdx() const;
        unsigned int GetTGSMIndex() const;
        unsigned int GetScratchSurfaceBindingTableIndex() const;
        unsigned int GetStatelessBindingTableIndex() const;
        unsigned int GetImmediateConstantBufferOffset() const;
        unsigned int GetDrawIndirectBufferIndex() const;
        const USC::SShaderStageBTLayout* GetBtLayout() const { return m_pLayout; };
        const std::vector<unsigned char>& GetColorBufferMappingTable() const { return m_ColorBufferMappings; }

        CBTILayout(const USC::SShaderStageBTLayout* pLayout) : m_pLayout(pLayout)
        {}

        CBTILayout(
            const USC::SShaderStageBTLayout* pLayout,
            const std::vector<unsigned char>& colorBufferMappings) :
            m_pLayout(pLayout),
            m_ColorBufferMappings(colorBufferMappings)
        {}

    protected:
        const USC::SShaderStageBTLayout* m_pLayout;

        // Vulkan front end provides a separate vector with color buffer mappings.
        const std::vector<unsigned char> m_ColorBufferMappings;
    };

    // This is insanely ugly, but it's the pretties solution we could
    // think of that preserves the GFX code.
    // This is temporary and will go away once image access between
    // OCL and GFX is unified.
    // This happens because in GFX the layout comes from the driver and is
    // immutable, while in OCL we need to change the layout mid-codegen.
    class COCLBTILayout : public CBTILayout
    {
    public:
        COCLBTILayout(const USC::SShaderStageBTLayout* pLayout) : CBTILayout(pLayout)
        {}

        USC::SShaderStageBTLayout* getModifiableLayout();
    };

    class RetryManager
    {
    public:
        RetryManager();
        ~RetryManager();

        bool AdvanceState();
        bool AllowLICM() const;
        bool AllowPromotePrivateMemory() const;
        bool AllowPreRAScheduler() const;
        bool AllowVISAPreRAScheduler() const;
        bool AllowCodeSinking() const;
        bool AllowAddressArithmeticSinking() const;
        bool AllowSimd32Slicing() const;
        bool AllowLargeURBWrite() const;
        bool AllowConstantCoalescing() const;
        bool AllowLargeGRF() const;
        bool AllowLoadSinking() const;
        void SetFirstStateId(int id);
        bool IsFirstTry() const;
        bool IsLastTry() const;
        unsigned GetRetryId() const;

        void Enable();
        void Disable();

        void SetSpillSize(unsigned int spillSize);
        unsigned int GetLastSpillSize() const;

        unsigned int numInstructions = 0;
        // For OCL the retry manager will work on per-kernel basis, that means
        // Disable() will disable only specific kernel. Other kernels still can
        // be retried. To keep the old behavior for other shader types, Disable()
        // will check the field and keep the old behavior. If other shader
        // types want to follow OCL this has to be set, see CodeGenContext
        // constructor.
        bool perKernel;
        /// the set of OCL kernels that need to recompile
        std::set<std::string> kernelSet;
        /// the set of OCL kernels that need to skip recompilation
        std::set<std::string> kernelSkip;

        void ClearSpillParams();
        // save entry for given SIMD mode, to avoid recompile for next retry.
        void SaveSIMDEntry(SIMDMode simdMode, CShader* shader);
        CShader* GetSIMDEntry(SIMDMode simdMode);
        bool AnyKernelSpills() const;

        // Try to pickup the simd mode & kernel based on heuristics and fill
        // programOutput.  If returning true, then stop the further retry.
        bool PickupKernels(CodeGenContext* cgCtx);

    private:
        unsigned stateId;
        // For debugging purposes, it can be useful to start on a particular
        // ID rather than id 0.
        unsigned firstStateId;

        // internal knob to disable retry manager.
        bool enabled;

        unsigned lastSpillSize = 0;

        // cache the compiled kernel during retry
        struct CacheEntry
        {
            SIMDMode simdMode;
            CShader* shader;
        };

        CacheEntry cache[3] = {
            {SIMDMode::SIMD8, nullptr},
            {SIMDMode::SIMD16, nullptr},
            {SIMDMode::SIMD32, nullptr},
        };

        CacheEntry* GetCacheEntry(SIMDMode simdMode);

    };

    /// this class adds intrinsic cache to LLVM context
    class LLVMContextWrapper : public llvm::LLVMContext
    {
        LLVMContextWrapper(LLVMContextWrapper&) = delete;
        LLVMContextWrapper& operator =(LLVMContextWrapper&) = delete;

    public:
        LLVMContextWrapper(bool createResourceDimTypes = true);
        /// ref count the LLVMContext as now CodeGenContext owns it
        unsigned int refCount = 0;
        /// IntrinsicIDCache - Cache of intrinsic pointer to numeric ID mappings
        /// requested in this context
        typedef llvm::ValueMap<const llvm::Function*, unsigned> SafeIntrinsicIDCacheTy;
        SafeIntrinsicIDCacheTy m_SafeIntrinsicIDCache;
        void AddRef();
        void Release();
    };

    struct RoutingIndex
    {
        unsigned int resourceRangeID;
        unsigned int indexIntoRange;
        unsigned int routeTo;
        unsigned int lscCacheCtrl;
    };

    class CodeGenContext
    {
    private:
        // For assigning a unique Function ID within CodeGenContext.
        std::unordered_map<std::string, int> m_functionIDs;
        bool m_enableDumpUseShorterName = false;
    public:
        /// input: hash key
        ShaderHash    hash;
        ShaderType    type;
        /// input: Platform features supported
        const CPlatform& platform;
        /// input: binding table layout used by the driver
        const CBTILayout& btiLayout;
        /// information about the driver
        const CDriverInfo& m_DriverInfo;
        /// output: driver instrumentation
        TimeStats* m_compilerTimeStats = nullptr;
        ShaderStats* m_sumShaderStats = nullptr;
        /// output: list of buffer IDs which are promoted to direct AS
        // Map of promoted buffer ids with their respective buffer offsets if needed. Buffer offset will be -1 if no need of buffer offset
        std::map<unsigned, int> m_buffersPromotedToDirectAS;
        // float 16, float32 and float64 denorm mode
        Float_DenormMode    m_floatDenormMode16 = FLOAT_DENORM_FLUSH_TO_ZERO;
        Float_DenormMode    m_floatDenormMode32 = FLOAT_DENORM_FLUSH_TO_ZERO;
        Float_DenormMode    m_floatDenormMode64 = FLOAT_DENORM_FLUSH_TO_ZERO;

        PushConstantMode m_pushConstantMode = PushConstantMode::DEFAULT;

        SInstrTypes m_instrTypes;
        SInstrTypes m_instrTypesAfterOpts;
        // The module contains global variables with private address space.
        // When this is true, the flag "ForceGlobalMemoryAllocation" is enabled as a WA
        bool m_hasGlobalInPrivateAddressSpace = false;

        /////  used for instruction statistic before/after pass
        int instrStat[TOTAL_TYPES][TOTAL_STAGE];

        // Module flag for subroutines/stackcalls enabled
        bool m_enableSubroutine = false;
        // Module flag for function pointers enabled
        bool m_enableFunctionPointer = false;
        // Module flag for when we need to compile multiple SIMD sizes to support SIMD variants
        bool m_enableSimdVariantCompilation = false;
        // Module flag to indicate if non-inlinable stack functions are present
        bool m_hasStackCalls = false;
        // Flag to determine if early Z culling should be called for certain patterns
        bool m_ForceEarlyZMathCheck = false;
        // Adding multiversioning to partially redundant samples, if AIL is on.
        bool m_enableSampleMultiversioning = false;

        bool m_src1RemovedForBlendOpt = false;
        llvm::AssemblyAnnotationWriter* annotater = nullptr;

        RetryManager m_retryManager;

        IGCMetrics::IGCMetric metrics;

        // shader stat for opt customization
        uint32_t     m_tempCount = 0;
        uint32_t     m_sampler = 0;
        uint32_t     m_inputCount = 0;
        uint32_t     m_dxbcCount = 0;
        uint32_t     m_ConstantBufferCount = 0;
        uint32_t     m_numGradientSinked = 0;
        std::vector<unsigned> m_indexableTempSize;
        bool         m_highPsRegisterPressure = 0;

        // Record previous simd for code patching
        CShader* m_prevShader = nullptr;

        // For IR dump after pass
        unsigned     m_numPasses = 0;
        bool m_threadCombiningOptDone = false;

        void* m_ConstantBufferReplaceShaderPatterns = nullptr;
        uint m_ConstantBufferReplaceShaderPatternsSize = 0;
        uint m_ConstantBufferUsageMask = 0;
        uint m_ConstantBufferReplaceSize = 0;
        // tracking next available GRF offset for constants payload
        unsigned int        m_constantPayloadNextAvailableGRFOffset = 0;
        ConstantPayloadInfo m_constantPayloadOffsets;

        void* gtpin_init = nullptr;
        bool m_hasLegacyDebugInfo = false;
        bool m_hasEmu64BitInsts = false;
        bool m_hasDPDivSqrtEmu = false;

        CompilerStats m_Stats;
        // Flag for staged compilation
        CG_FLAG_t m_CgFlag = FLAG_CG_ALL_SIMDS;
        // Staging context passing from Stage 1 for compile continuation
        CG_CTX_t* m_StagingCtx = nullptr;
        // We determine whether generating SIMD32 based on SIMD16's result
        // For staged compilation, we record if SIMD32 will be generated in Stage1, and
        // pass it to Stage2.
        bool m_doSimd32Stage2 = false;
        bool m_doSimd16Stage2 = false;
        std::string m_savedBitcodeString;
        SInstrTypes m_savedInstrTypes;

        bool m_hasVendorExtension = false;

        std::vector<int> m_hsIdxMap;
        std::vector<int> m_dsIdxMap;
        std::vector<int> m_gsIdxMap;
        std::vector<int> m_hsNonDefaultIdxMap;
        std::vector<int> m_dsNonDefaultIdxMap;
        std::vector<int> m_gsNonDefaultIdxMap;
        std::vector<int> m_psIdxMap;
        DWORD LtoUsedMask = 0;
        uint64_t m_SIMDInfo;
        uint32_t HdcEnableIndexSize = 0;
        std::vector<RoutingIndex> HdcEnableIndexValues;
        uint32_t m_NumGRFPerThread = 0;

        // Flag per function/kernel informing about if it has
        // expensive loops and needs trigger retry compilation
        std::unordered_map<llvm::Function*, bool> m_FuncHasExpensiveLoops;

        bool HasFuncExpensiveLoop(llvm::Function* pFunc);

        // Raytracing (any shader type)
        // If provided, the BVH has been constructed such that the root node
        // is at a constant offset from the start of the BVH. This allows
        // us to skip loading the offset at BVH::rootNodeOffset.
        std::optional<size_t> BVHFixedOffset;
    private:
        //For storing error message
        std::stringstream oclErrorMessage;
        //For storing warning message
        std::stringstream oclWarningMessage;

    protected:
        // Objects pointed to by these pointers are owned by this class.
        LLVMContextWrapper* llvmCtxWrapper;
        /// input: LLVM module
        IGCLLVM::Module* module = nullptr;
        /// input: IGC MetaData Utils
        IGC::IGCMD::MetaDataUtils* m_pMdUtils = nullptr;
        IGC::ModuleMetaData* modMD = nullptr;

        virtual void setFlagsPerCtx();
    public:
        CodeGenContext(
            ShaderType          _type,      ///< shader type
            const CBTILayout& _bitLayout, ///< binding table layout to be used in code gen
            const CPlatform& _platform,  ///< IGC HW platform description
            const CDriverInfo& driverInfo, ///< Queries to know runtime features support
            const bool          createResourceDimTypes = true,
            LLVMContextWrapper* LLVMContext = nullptr)///< LLVM context to use, if null a new one will be created
            : type(_type), platform(_platform), btiLayout(_bitLayout), m_DriverInfo(driverInfo),
            llvmCtxWrapper(LLVMContext), m_SIMDInfo(0)
        {
            if (llvmCtxWrapper == nullptr)
            {
                initLLVMContextWrapper(createResourceDimTypes);
            }
            else
            {
                llvmCtxWrapper->AddRef();
            }

            m_indexableTempSize.resize(64);

            for (uint i = 0; i < TOTAL_TYPES; i++)
            {
                for (uint j = 0; j < TOTAL_STAGE; j++)
                {
                    instrStat[i][j] = 0;
                }
            }

            // Per context flag adjustment
            setFlagsPerCtx();

            // Set retry behavor for Disable()
            m_retryManager.perKernel = (type == ShaderType::OPENCL_SHADER);
        }

        CodeGenContext(CodeGenContext&) = delete;
        CodeGenContext& operator =(CodeGenContext&) = delete;

        void initLLVMContextWrapper(bool createResourceDimTypes = true);
        llvm::LLVMContext* getLLVMContext() const;
        IGC::IGCMD::MetaDataUtils* getMetaDataUtils() const;
        IGCLLVM::Module* getModule() const;

        void setModule(llvm::Module* m);
        // Several clients explicitly delete module without resetting module to null.
        // This causes the issue later when the dtor is invoked (trying to delete a
        // dangling pointer again). This function is used to replace any explicit
        // delete in order to prevent deleting dangling pointers happening.
        void deleteModule();
        IGC::ModuleMetaData* getModuleMetaData() const;
        unsigned int getRegisterPointerSizeInBits(unsigned int AS) const;
        bool enableFunctionCall() const;
        void CheckEnableSubroutine(llvm::Module& M);
        virtual void InitVarMetaData();
        virtual ~CodeGenContext();
        void clear();
        void clearMD();
        void EmitError(std::ostream &OS, const char* errorstr, const llvm::Value *context) const;
        void EmitError(const char* errorstr, const llvm::Value *context);
        void EmitWarning(const char* warningstr);
        inline bool HasError() const { return !this->oclErrorMessage.str().empty(); }
        inline bool HasWarning() const { return !this->oclWarningMessage.str().empty(); }
        inline const std::string GetWarning() { return this->oclWarningMessage.str(); }
        inline const std::string GetError() { return this->oclErrorMessage.str(); }
        inline const std::string GetErrorAndWarning() { return GetWarning() + GetError(); }

        CompOptions& getCompilerOption();
        virtual void resetOnRetry();
        virtual uint32_t getNumThreadsPerEU() const;
        virtual uint32_t getNumGRFPerThread(bool returnDefault = true);
        virtual void setNumGRFPerThread(uint32_t value) { m_NumGRFPerThread = value; }
        virtual bool forceGlobalMemoryAllocation() const;
        virtual bool allocatePrivateAsGlobalBuffer() const;
        virtual bool noLocalToGenericOptionEnabled() const;
        virtual bool enableTakeGlobalAddress() const;
        virtual int16_t getVectorCoalescingControl() const;
        virtual uint32_t getPrivateMemoryMinimalSizePerThread() const;
        virtual uint32_t getIntelScratchSpacePrivateMemoryMinimalSizePerThread() const;
        virtual bool enableZEBinary() const;
        bool isPOSH() const;

        CompilerStats& Stats()
        {
            return m_Stats;
        }

        unsigned int GetSIMDInfoOffset(SIMDMode simd, ShaderDispatchMode mode)
        {
            unsigned int offset = 0;

            switch (mode) {
            case ShaderDispatchMode::NOT_APPLICABLE:
                switch (simd) {
                case SIMDMode::SIMD8:
                    offset = SIMD8_OFFSET;
                    break;
                case SIMDMode::SIMD16:
                    offset = SIMD16_OFFSET;
                    break;
                case SIMDMode::SIMD32:
                    offset = SIMD32_OFFSET;
                    break;
                default:
                    break;
                }
                break;

            default:
                break;
            }
            return offset;
        }

        void SetSIMDInfo(SIMDInfoBit bit, SIMDMode simd, ShaderDispatchMode mode)
        {
            unsigned int offset = GetSIMDInfoOffset(simd, mode);
            m_SIMDInfo |= (uint64_t)1 << (bit + offset);
        }

        void ClearSIMDInfo(SIMDMode simd, ShaderDispatchMode mode)
        {
            unsigned int offset = GetSIMDInfoOffset(simd, mode);
            m_SIMDInfo &= ~(0xff << offset);
        }

        uint64_t GetSIMDInfo() { return m_SIMDInfo; }

        virtual llvm::Optional<SIMDMode> knownSIMDSize() const {
            return llvm::None;
        }

        // This can be paired with `EncodeAS4GFXResource()` to get a unique
        // index.
        uint32_t getUniqueIndirectIdx()
        {
            return getModuleMetaData()->CurUniqueIndirectIdx++;
        }

        // Frontends may elect to compute indices in their own way. If so,
        // they should call this at the end to mark the max index they have
        // reserved so that later passes can ensure that `getUniqueIndirectIdx()`
        // won't collide with any indices from the frontend.
        void setUniqueIndirectIdx(uint32_t NewVal)
        {
            uint32_t &CurVal = getModuleMetaData()->CurUniqueIndirectIdx;
            CurVal = std::max(CurVal, NewVal);
        }

        // Use this when you want to know about a particular function's
        // rayquery usage.
        bool hasSyncRTCalls(llvm::Function *F) const
        {
            auto* MMD = getModuleMetaData();
            auto funcMDItr = MMD->FuncMD.find(F);
            bool hasRQCall =
                (funcMDItr != MMD->FuncMD.end() && funcMDItr->second.hasSyncRTCalls);

            return hasRQCall;
        }

        // Use this to determine if any shaders in the module use rayquery.
        bool hasSyncRTCalls() const
        {
            return (getModuleMetaData()->rtInfo.RayQueryAllocSizeInBytes != 0);
        }

        // For creating internal names with function IDs.
        void createFunctionIDs();
        int getFunctionID(llvm::Function* F);
        std::string getFunctionDumpName(int functionId);
        bool dumpUseShorterName() const { return m_enableDumpUseShorterName; }
    };

    struct SComputeShaderSecondCompileInput
    {
        bool secondCompile;
        bool isRowMajor;
        int numChannelsUsed;
        int runtimeVal_LoopCount;
        int runtimeVal_ResWidthOrHeight;
        int runtimeVal_ConstBufferSize;

        SComputeShaderSecondCompileInput()
            : secondCompile(false)
            , isRowMajor(false)
            , numChannelsUsed(0)
            , runtimeVal_LoopCount(0)
            , runtimeVal_ResWidthOrHeight(0)
            , runtimeVal_ConstBufferSize(0)
        {}
    };

    void OptimizeIR(CodeGenContext* ctx);

    /**
     * Fold derived constants.  Load CB data from CBptr with index & offset,
     * calculate the new data based on LLVM bitcode and store results to pNewCB.
     * Then driver will push pNewCB to thread payload.
     */
    void FoldDerivedConstant(char* bitcode, uint bitcodeSize, void* CBptr[15],
        std::function<void(uint[4], uint, uint, bool)> getResInfoCB, uint* pNewCB);
} // end IGC namespace
