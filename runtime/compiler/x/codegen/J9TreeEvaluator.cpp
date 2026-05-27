/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "j9port.h"
#include "locknursery.h"
#include "thrdsup.h"
#include "thrtypes.h"
#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/ScratchRegisterManager.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/TRResolvedMethod.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CompilerEnv.hpp"
#include "env/CHTable.hpp"
#include "env/IO.hpp"
#include "env/j9method.h"
#include "env/jittypes.h"
#include "env/PersistentCHTable.hpp"
#include "env/VMJ9.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/SimpleRegex.hpp"
#include "OMR/Bytes.hpp"
#include "x/codegen/AllocPrefetchSnippet.hpp"
#include "x/codegen/CheckFailureSnippet.hpp"
#include "x/codegen/CompareAnalyser.hpp"
#include "x/codegen/ForceRecompilationSnippet.hpp"
#include "x/codegen/FPTreeEvaluator.hpp"
#include "x/codegen/J9X86Instruction.hpp"
#include "x/codegen/MonitorSnippet.hpp"
#include "x/codegen/OutlinedInstructions.hpp"
#include "x/codegen/HelperCallSnippet.hpp"
#include "env/CompilerEnv.hpp"
#include "ras/Logger.hpp"
#include "runtime/J9Runtime.hpp"
#include "codegen/J9WatchedStaticFieldSnippet.hpp"
#include "codegen/X86FPConversionSnippet.hpp"

#ifdef TR_TARGET_64BIT
#include "codegen/AMD64PrivateLinkage.hpp"
#endif
#ifdef TR_TARGET_32BIT
#include "codegen/IA32PrivateLinkage.hpp"
#include "codegen/IA32LinkageUtils.hpp"
#endif

#ifdef LINUX
#include <time.h>

#endif

#define NUM_PICS 3

// Minimum number of words for zero-initialization via REP OP::STOSD
//
#define MIN_REPSTOSD_WORDS 64
static int32_t minRepstosdWords = 0;

// Maximum number of words per loop iteration for loop zero-initialization.
//
#define MAX_ZERO_INIT_WORDS_PER_ITERATION 4
static int32_t maxZeroInitWordsPerIteration = 0;

static bool getNodeIs64Bit(TR::Node *node, TR::CodeGenerator *cg);
static TR::Register *intOrLongClobberEvaluate(TR::Node *node, bool nodeIs64Bit, TR::CodeGenerator *cg);

static uint32_t logBase2(uintptr_t n)
{
    // Could use leadingZeroes, except we can't call it from here
    //
    uint32_t result = 8 * sizeof(n) - 1;
    uintptr_t mask = ((uintptr_t)1) << result;
    while (mask && !(mask & n)) {
        mask >>= 1;
        result--;
    }
    return result;
}

// ----------------------------------------------------------------------------
inline void generateLoadJ9Class(TR::Node *node, TR::Register *j9class, TR::Register *object, TR::CodeGenerator *cg)
{
    bool needsNULLCHK = false;
    TR::ILOpCodes opValue = node->getOpCodeValue();

    if (node->getOpCode().isReadBar() || node->getOpCode().isWrtBar())
        needsNULLCHK = true;
    else {
        switch (opValue) {
            case TR::monent:
            case TR::monexit:
                TR_ASSERT_FATAL(TR::Compiler->om.areValueTypesEnabled()
                        || TR::Compiler->om.areValueBasedMonitorChecksEnabled(),
                    "monent and monexit are expected for generateLoadJ9Class only when value type or when value based "
                    "monitor check is enabled");
            case TR::checkcastAndNULLCHK:
                needsNULLCHK = true;
                break;
            case TR::icall: // TR_checkAssignable
                return; // j9class register already holds j9class
            case TR::checkcast:
            case TR:: instanceof:
                break;
            default:
                TR_ASSERT_FATAL(false, "Unexpected opCode for generateLoadJ9Class %s.", node->getOpCode().getName());
                break;
        }
    }

    auto use64BitClasses = cg->comp()->target().is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders();
    auto instr = Inst_RegMem(OP::LRegMem(use64BitClasses), node, j9class,
        MRef_Bdisp32(object, TR::Compiler->om.offsetOfObjectVftField(), cg), cg);
    if (needsNULLCHK) {
        cg->setImplicitExceptionPoint(instr);
        instr->setNeedsGCMap(0xFF00FFFF);
        if (opValue == TR::checkcastAndNULLCHK)
            instr->setNode(cg->comp()->findNullChkInfo(node));
    }

    auto mask = TR::Compiler->om.maskOfObjectVftField();
    if (~mask != 0) {
        Inst_RegImm(~mask <= 127 ? OP::ANDRegImms(use64BitClasses) : OP::ANDRegImm4(use64BitClasses), node, j9class,
            mask, cg);
    }
}

static TR_OutlinedInstructions *generateArrayletReference(TR::Node *node, TR::Node *loadOrStoreOrArrayElementNode,
    TR::Instruction *checkInstruction, TR::LabelSymbol *arrayletRefLabel, TR::LabelSymbol *restartLabel,
    TR::Register *baseArrayReg, TR::Register *loadOrStoreReg, TR::Register *indexReg, int32_t indexValue,
    TR::Register *valueReg, bool needsBoundCheck, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

    TR::Register *scratchReg = cg->allocateRegister();

    TR_OutlinedInstructions *arrayletRef = new (cg->trHeapMemory()) TR_OutlinedInstructions(arrayletRefLabel, cg);
    arrayletRef->setRestartLabel(restartLabel);

    if (needsBoundCheck) {
        // The current block is required for exception handling and anchoring
        // the GC map.
        //
        arrayletRef->setBlock(cg->getCurrentEvaluationBlock());
        arrayletRef->setCallNode(node);
    }

    cg->getOutlinedInstructionsList().push_front(arrayletRef);

    arrayletRef->swapInstructionListsWithCompilation();

    Inst_Label(NULL, OP::label, arrayletRefLabel, cg)->setNode(node);

    // TODO: REMOVE THIS!
    //
    // This merely indicates that this OOL sequence should be assigned with the non-linear
    // assigner, and should go away when the non-linear assigner handles all OOL sequences.
    //
    arrayletRefLabel->setNonLinear();

    static char *forceArrayletInt = feGetEnv("TR_forceArrayletInt");
    if (forceArrayletInt) {
        Inst(OP::INT3, node, cg);
    }

    // -----------------------------------------------------------------------------------
    // Track all virtual register use within the arraylet path.  This info will be used
    // to adjust the virtual register use counts within the mainline path for more precise
    // register assignment.
    // -----------------------------------------------------------------------------------

    cg->startRecordingRegisterUsage();

    if (needsBoundCheck) {
        // -------------------------------------------------------------------------
        // Check if the base array has a spine.  If not, this is a real AIOB.
        // -------------------------------------------------------------------------

        TR::MemoryReference *arraySizeMR = MRef_Bdisp32(baseArrayReg, fej9->getOffsetOfContiguousArraySizeField(), cg);

        Inst_MemImm(OP::CMP4MemImms, node, arraySizeMR, 0, cg);

        TR::LabelSymbol *boundCheckFailureLabel = generateLabelSymbol(cg);

        checkInstruction = Inst_Label(OP::JNE4, node, boundCheckFailureLabel, cg);

        cg->addSnippet(new (cg->trHeapMemory()) TR::X86CheckFailureSnippet(cg, node->getSymbolReference(),
            boundCheckFailureLabel, checkInstruction, false));

        // -------------------------------------------------------------------------
        // The array has a spine.  Do a bound check on its true length.
        // -------------------------------------------------------------------------

        arraySizeMR = MRef_Bdisp32(baseArrayReg, fej9->getOffsetOfDiscontiguousArraySizeField(), cg);

        if (!indexReg) {
            OP::Mnemonic op = (indexValue >= -128 && indexValue <= 127) ? OP::CMP4MemImms : OP::CMP4MemImm4;
            Inst_MemImm(op, node, arraySizeMR, indexValue, cg);
        } else {
            Inst_MemReg(OP::CMP4MemReg, node, arraySizeMR, indexReg, cg);
        }

        boundCheckFailureLabel = generateLabelSymbol(cg);
        checkInstruction = Inst_Label(OP::JBE4, node, boundCheckFailureLabel, cg);

        cg->addSnippet(new (cg->trHeapMemory()) TR::X86CheckFailureSnippet(cg, node->getSymbolReference(),
            boundCheckFailureLabel, checkInstruction, false));
    }

    // -------------------------------------------------------------------------
    // Determine if a load needs to be decompressed.
    // -------------------------------------------------------------------------

    bool seenCompressionSequence = false;
    bool loadNeedsDecompression = false;

    if (loadOrStoreOrArrayElementNode->getOpCodeValue() == TR::l2a
        || (((loadOrStoreOrArrayElementNode->getOpCodeValue() == TR::aload
                 || loadOrStoreOrArrayElementNode->getOpCodeValue() == TR::aRegLoad)
                && node->isSpineCheckWithArrayElementChild())
            && comp->target().is64Bit() && comp->useCompressedPointers()))
        loadNeedsDecompression = true;

    TR::Node *actualLoadOrStoreOrArrayElementNode = loadOrStoreOrArrayElementNode;
    while ((loadNeedsDecompression && actualLoadOrStoreOrArrayElementNode->getOpCode().isConversion())
        || actualLoadOrStoreOrArrayElementNode->containsCompressionSequence()) {
        if (actualLoadOrStoreOrArrayElementNode->containsCompressionSequence())
            seenCompressionSequence = true;

        actualLoadOrStoreOrArrayElementNode = actualLoadOrStoreOrArrayElementNode->getFirstChild();
    }

    // -------------------------------------------------------------------------
    // Do the load, store, or array address calculation
    // -------------------------------------------------------------------------

    TR::DataType dt = actualLoadOrStoreOrArrayElementNode->getDataType();
    int32_t elementSize;

    if (dt == TR::Address) {
        elementSize = TR::Compiler->om.sizeofReferenceField();
    } else {
        elementSize = TR::Symbol::convertTypeToSize(dt);
    }

    int32_t spinePointerSize = (comp->target().is64Bit() && !comp->useCompressedPointers()) ? 8 : 4;
    int32_t arrayHeaderSize = TR::Compiler->om.discontiguousArrayHeaderSizeInBytes();
    int32_t arrayletMask = fej9->getArrayletMask(elementSize);

    TR::MemoryReference *spineMR;

    // Load the arraylet from the spine.
    //
    if (indexReg) {
        OP::Mnemonic op = comp->target().is64Bit() ? OP::MOVSXReg8Reg4 : OP::MOVRegReg();
        Inst_RegReg(op, node, scratchReg, indexReg, cg);

        int32_t spineShift = fej9->getArraySpineShift(elementSize);
        Inst_RegImm(OP::SARRegImm1(), node, scratchReg, spineShift, cg);

        spineMR = MRef_BISdisp32(baseArrayReg, scratchReg,
            TR::MemoryReference::convertMultiplierToStride(spinePointerSize), arrayHeaderSize, cg);
    } else {
        int32_t spineIndex = fej9->getArrayletLeafIndex(indexValue, elementSize);
        int32_t spineDisp32 = (spineIndex * spinePointerSize) + arrayHeaderSize;

        spineMR = MRef_Bdisp32(baseArrayReg, spineDisp32, cg);
    }

    OP::Mnemonic op = (spinePointerSize == 8) ? OP::L8RegMem : OP::L4RegMem;
    Inst_RegMem(op, node, scratchReg, spineMR, cg);

    // Decompress the arraylet pointer from the spine.
    int32_t shiftOffset = 0;

    if (comp->target().is64Bit() && comp->useCompressedPointers()) {
        shiftOffset = TR::Compiler->om.compressedReferenceShiftOffset();
        if (shiftOffset > 0) {
            Inst_RegImm(OP::SHL8RegImm1, node, scratchReg, shiftOffset, cg);
        }
    }

    TR::MemoryReference *arrayletMR;

    // Calculate the offset with the arraylet for the index.
    //
    if (indexReg) {
        TR::Register *scratchReg2 = cg->allocateRegister();

        Inst_RegReg(OP::MOVRegReg(), node, scratchReg2, indexReg, cg);
        Inst_RegImm(OP::ANDRegImm4(), node, scratchReg2, arrayletMask, cg);
        arrayletMR = MRef_BIS(scratchReg, scratchReg2, TR::MemoryReference::convertMultiplierToStride(elementSize), cg);

        cg->stopUsingRegister(scratchReg2);
    } else {
        int32_t arrayletIndex = ((TR_J9VMBase *)fej9)->getLeafElementIndex(indexValue, elementSize);
        arrayletMR = MRef_Bdisp32(scratchReg, arrayletIndex * elementSize, cg);
    }

    cg->stopUsingRegister(scratchReg);

    if (!actualLoadOrStoreOrArrayElementNode->getOpCode().isStore()) {
        OP::Mnemonic op;

        TR::MemoryReference *highArrayletMR = NULL;
        TR::Register *highRegister = NULL;

        // If we're not loading an array shadow then this must be an effective
        // address computation on the array element (for a write barrier).
        //
        if ((!actualLoadOrStoreOrArrayElementNode->getOpCode().hasSymbolReference()
                || !actualLoadOrStoreOrArrayElementNode->getSymbolReference()->getSymbol()->isArrayShadowSymbol())
            && !node->isSpineCheckWithArrayElementChild()) {
            op = OP::LEARegMem();
        } else {
            switch (dt) {
                case TR::Int8:
                    op = OP::L1RegMem;
                    break;
                case TR::Int16:
                    op = OP::L2RegMem;
                    break;
                case TR::Int32:
                    op = OP::L4RegMem;
                    break;
                case TR::Int64:
                    if (comp->target().is64Bit())
                        op = OP::L8RegMem;
                    else {
                        TR_ASSERT(loadOrStoreReg->getRegisterPair(), "expecting a register pair");

                        op = OP::L4RegMem;
                        highArrayletMR = MRef_MRefOff(*arrayletMR, 4, cg);
                        highRegister = loadOrStoreReg->getHighOrder();
                        loadOrStoreReg = loadOrStoreReg->getLowOrder();
                    }
                    break;

                case TR::Float:
                    op = OP::MOVSSRegMem;
                    break;
                case TR::Double:
                    op = OP::MOVSDRegMem;
                    break;

                case TR::Address:
                    if (comp->target().is32Bit() || comp->useCompressedPointers())
                        op = OP::L4RegMem;
                    else
                        op = OP::L8RegMem;
                    break;

                default:
                    TR_ASSERT(0, "unsupported array element load type");
                    op = OP::bad;
            }
        }

        Inst_RegMem(op, node, loadOrStoreReg, arrayletMR, cg);

        if (highArrayletMR) {
            Inst_RegMem(op, node, highRegister, highArrayletMR, cg);
        }

        // Decompress the loaded address if necessary.
        //
        if (loadNeedsDecompression) {
            if (comp->target().is64Bit() && comp->useCompressedPointers()) {
                if (shiftOffset > 0) {
                    Inst_RegImm(OP::SHL8RegImm1, node, loadOrStoreReg, shiftOffset, cg);
                }
            }
        }
    } else {
        if (dt != TR::Address) {
            // movE [S + S2], value
            //
            OP::Mnemonic op;
            bool needStore = true;

            switch (dt) {
                case TR::Int8:
                    op = valueReg ? OP::S1MemReg : OP::S1MemImm1;
                    break;
                case TR::Int16:
                    op = valueReg ? OP::S2MemReg : OP::S2MemImm2;
                    break;
                case TR::Int32:
                    op = valueReg ? OP::S4MemReg : OP::S4MemImm4;
                    break;
                case TR::Int64:
                    if (comp->target().is64Bit()) {
                        // The range of the immediate must be verified before this function to
                        // fall within a signed 32-bit integer.
                        //
                        op = valueReg ? OP::S8MemReg : OP::S8MemImm4;
                    } else {
                        if (valueReg) {
                            TR_ASSERT(valueReg->getRegisterPair(), "value must be a register pair");
                            Inst_MemReg(OP::S4MemReg, node, arrayletMR, valueReg->getLowOrder(), cg);
                            Inst_MemReg(OP::S4MemReg, node, MRef_MRefOff(*arrayletMR, 4, cg), valueReg->getHighOrder(),
                                cg);
                        } else {
                            TR::Node *valueChild = actualLoadOrStoreOrArrayElementNode->getSecondChild();
                            TR_ASSERT(valueChild->getOpCode().isLoadConst(), "expecting a long constant child");

                            Inst_MemImm(OP::S4MemImm4, node, arrayletMR, valueChild->getLongIntLow(), cg);
                            Inst_MemImm(OP::S4MemImm4, node, MRef_MRefOff(*arrayletMR, 4, cg),
                                valueChild->getLongIntHigh(), cg);
                        }

                        needStore = false;
                    }
                    break;

                case TR::Float:
                    op = OP::MOVSSMemReg;
                    break;
                case TR::Double:
                    op = OP::MOVSDMemReg;
                    break;

                default:
                    TR_ASSERT(0, "unsupported array element store type");
                    op = OP::bad;
            }

            if (needStore) {
                if (valueReg)
                    Inst_MemReg(op, node, arrayletMR, valueReg, cg);
                else {
                    int32_t value = actualLoadOrStoreOrArrayElementNode->getSecondChild()->getInt();
                    Inst_MemImm(op, node, arrayletMR, value, cg);
                }
            }
        } else {
            // lea S, [S+S2]
            TR_ASSERT(0, "OOL reference stores not supported yet");
        }
    }

    Inst_Label(OP::JMP4, node, restartLabel, cg);

    // -----------------------------------------------------------------------------------
    // Stop tracking virtual register usage.
    // -----------------------------------------------------------------------------------

    arrayletRef->setOutlinedPathRegisterUsageList(cg->stopRecordingRegisterUsage());

    arrayletRef->swapInstructionListsWithCompilation();

    return arrayletRef;
}

// 32-bit float/double convert to long
//
TR::Register *J9::X86::TreeEvaluator::fpConvertToLong(TR::Node *node, TR::SymbolReference *helperSymRef,
    TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR_ASSERT_FATAL(comp->target().is32Bit(), "AMD64 doesn't use this logic");

    TR::Node *child = node->getFirstChild();

    if (child->getOpCode().isDouble()) {
        TR::RegisterDependencyConditions *deps;

        TR::Register *doubleReg = cg->evaluate(child);
        TR::Register *lowReg = cg->allocateRegister(TR_GPR);
        TR::Register *highReg = cg->allocateRegister(TR_GPR);
        TR::RealRegister *espReal = cg->machine()->getRealRegister(TR::RealRegister::esp);

        deps = RegDeps((uint8_t)0, 3, cg);
        deps->addPostCondition(lowReg, TR::RealRegister::NoReg, cg);
        deps->addPostCondition(highReg, TR::RealRegister::NoReg, cg);
        deps->addPostCondition(doubleReg, TR::RealRegister::NoReg, cg);
        deps->stopAddingConditions();

        TR::LabelSymbol *reStartLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg); // exit routine label
        TR::LabelSymbol *CallLabel
            = TR::LabelSymbol::create(cg->trHeapMemory(), cg); // label where long (64-bit) conversion will start
        TR::LabelSymbol *StartLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);

        StartLabel->setStartInternalControlFlow();
        reStartLabel->setEndInternalControlFlow();

        // Attempt to convert a double in an XMM register to an integer using CVTTSD2SI.
        // If the conversion succeeds, put the integer in lowReg and sign-extend it to highReg.
        // If the conversion fails (the double is too large), call the helper.
        Inst_RegReg(OP::CVTTSD2SIReg4Reg, node, lowReg, doubleReg, cg);
        Inst_RegImm(OP::CMP4RegImm4, node, lowReg, 0x80000000, cg);

        Inst_Label(OP::label, node, StartLabel, cg);
        Inst_Label(OP::JE4, node, CallLabel, cg);

        Inst_RegReg(OP::MOV4RegReg, node, highReg, lowReg, cg);
        Inst_RegImm(OP::SAR4RegImm1, node, highReg, 31, cg);

        Inst_Label(OP::label, node, reStartLabel, deps, cg);

        TR::Register *targetRegister = cg->allocateRegisterPair(lowReg, highReg);
        TR::SymbolReference *d2l = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_IA32double2LongSSE);
        d2l->getSymbol()->getMethodSymbol()->setLinkage(TR_Helper);
        TR::Node::recreate(node, TR::lcall);
        node->setSymbolReference(d2l);
        TR_OutlinedInstructions *outlinedHelperCall = new (cg->trHeapMemory())
            TR_OutlinedInstructions(node, TR::lcall, targetRegister, CallLabel, reStartLabel, cg);
        cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);

        cg->decReferenceCount(child);
        node->setRegister(targetRegister);

        return targetRegister;
    } else {
        TR::Register *accReg = NULL;
        TR::Register *lowReg = cg->allocateRegister(TR_GPR);
        TR::Register *highReg = cg->allocateRegister(TR_GPR);
        TR::Register *floatReg = cg->evaluate(child);

        TR::LabelSymbol *snippetLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
        TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
        TR::LabelSymbol *reStartLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);

        startLabel->setStartInternalControlFlow();
        reStartLabel->setEndInternalControlFlow();

        Inst_Label(OP::label, node, startLabel, cg);

        // These instructions must be set appropriately prior to the creation
        // of the snippet near the end of this method. Also see warnings below.
        //
        TR::X86RegMemInstruction *loadHighInstr; // loads the high dword of the converted long
        TR::X86RegMemInstruction *loadLowInstr; // loads the low dword of the converted long

        TR::MemoryReference *tempMR = cg->machine()->getDummyLocalMR(TR::Float);
        Inst_MemReg(OP::MOVSSMemReg, node, tempMR, floatReg, cg);
        Inst_Mem(OP::FLDMem, node, MRef_MRefOff(*tempMR, 0, cg), cg);

        Inst(OP::FLDDUP, node, cg);

        // For slow conversion only, change the rounding mode on the FPU via its control word register.
        //
        TR::MemoryReference *convertedLongMR = (cg->machine())->getDummyLocalMR(TR::Int64);

        if (cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_X86_SSE3)) {
            Inst_Mem(OP::FLSTTPMem, node, convertedLongMR, cg);
        } else {
            int16_t fpcw = comp->getJittedMethodSymbol()->usesSinglePrecisionMode() ? SINGLE_PRECISION_ROUND_TO_ZERO
                                                                                    : DOUBLE_PRECISION_ROUND_TO_ZERO;
            Inst_Mem(OP::LDCWMem, node, MRef_const(cg->findOrCreate2ByteConstant(node, fpcw), cg), cg);
            Inst_Mem(OP::FLSTPMem, node, convertedLongMR, cg);

            fpcw = comp->getJittedMethodSymbol()->usesSinglePrecisionMode() ? SINGLE_PRECISION_ROUND_TO_NEAREST
                                                                            : DOUBLE_PRECISION_ROUND_TO_NEAREST;

            Inst_Mem(OP::LDCWMem, node, MRef_const(cg->findOrCreate2ByteConstant(node, fpcw), cg), cg);
        }

        // WARNING:
        //
        // The following load instructions are dissected in the snippet to determine the target registers.
        // If they or their format is changed, you may need to change the snippet also.
        //
        loadHighInstr = Inst_RegMem(OP::L4RegMem, node, highReg, MRef_MRefOff(*convertedLongMR, 4, cg), cg);

        loadLowInstr = Inst_RegMem(OP::L4RegMem, node, lowReg, MRef_MRefOff(*convertedLongMR, 0, cg), cg);

        // Jump to the snippet if the converted value is an indefinite integer; otherwise continue.
        //
        Inst_RegImm(OP::CMP4RegImm4, node, highReg, INT_MIN, cg);
        Inst_Label(OP::JNE4, node, reStartLabel, cg);
        Inst_RegReg(OP::TEST4RegReg, node, lowReg, lowReg, cg);
        Inst_Label(OP::JE4, node, snippetLabel, cg);

        // Create the conversion snippet.
        //
        cg->addSnippet(new (cg->trHeapMemory()) TR::X86FPConvertToLongSnippet(reStartLabel, snippetLabel, helperSymRef,
            node, loadHighInstr, loadLowInstr, cg));

        TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, accReg ? 3 : 2, cg);

        // Make sure the high and low long registers are assigned to something.
        //
        if (accReg) {
            deps->addPostCondition(accReg, TR::RealRegister::eax, cg);
        }

        deps->addPostCondition(lowReg, TR::RealRegister::NoReg, cg);
        deps->addPostCondition(highReg, TR::RealRegister::NoReg, cg);

        Inst_Label(OP::label, node, reStartLabel, deps, cg);

        cg->decReferenceCount(child);
        Inst(OP::FSTPST0, node, cg);

        TR::Register *targetRegister = cg->allocateRegisterPair(lowReg, highReg);
        node->setRegister(targetRegister);
        return targetRegister;
    }
}

// On AMD64, all four [fd]2[il] conversions are handled here
// On IA32, both [fd]2i conversions are handled here
TR::Register *J9::X86::TreeEvaluator::f2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    bool doubleSource;
    bool longTarget;
    OP::Mnemonic cvttOpCode;

    switch (node->getOpCodeValue()) {
        case TR::f2i:
            cvttOpCode = OP::CVTTSS2SIReg4Reg;
            doubleSource = false;
            longTarget = false;
            break;
        case TR::f2l:
            cvttOpCode = OP::CVTTSS2SIReg8Reg;
            doubleSource = false;
            longTarget = true;
            break;
        case TR::d2i:
            cvttOpCode = OP::CVTTSD2SIReg4Reg;
            doubleSource = true;
            longTarget = false;
            break;
        case TR::d2l:
            cvttOpCode = OP::CVTTSD2SIReg8Reg;
            doubleSource = true;
            longTarget = true;
            break;
        default:
            TR_ASSERT_FATAL(0, "Unknown opcode value in f2iEvaluator");
            break;
    }
    TR_ASSERT_FATAL(cg->comp()->target().is64Bit() || !longTarget, "Incorrect opcode value in f2iEvaluator");

    TR::Node *child = node->getFirstChild();
    TR::Register *sourceRegister = NULL;
    TR::Register *targetRegister = cg->allocateRegister(TR_GPR);
    TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    TR::LabelSymbol *endLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    TR::LabelSymbol *exceptionLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);

    sourceRegister = cg->evaluate(child);
    Inst_RegReg(cvttOpCode, node, targetRegister, sourceRegister, cg);

    startLabel->setStartInternalControlFlow();
    endLabel->setEndInternalControlFlow();

    Inst_Label(OP::label, node, startLabel, cg);

    if (longTarget) {
        TR_ASSERT_FATAL(cg->comp()->target().is64Bit(), "We should only get here on AMD64");
        // We can't compare with 0x8000000000000000.
        // Instead, rotate left 1 bit and compare with 0x0000000000000001.
        Inst_Reg(OP::ROL8Reg1, node, targetRegister, cg);
        Inst_RegImm(OP::CMP8RegImms, node, targetRegister, 1, cg);
        Inst_Label(OP::JE4, node, exceptionLabel, cg);
    } else {
        Inst_RegImm(OP::CMP4RegImm4, node, targetRegister, INT_MIN, cg);
        Inst_Label(OP::JE4, node, exceptionLabel, cg);
    }

    // TODO: (omr issue #4969): Remove once support for spills in OOL paths is added
    TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, (uint8_t)2, cg);
    deps->addPostCondition(targetRegister, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(sourceRegister, TR::RealRegister::NoReg, cg);

    {
        TR_OutlinedInstructionsGenerator og(exceptionLabel, node, cg);
        // at this point, target is set to -INF and there can only be THREE possible results: -INF, +INF, NaN
        // compare source with ZERO
        Inst_RegMem(doubleSource ? OP::UCOMISDRegMem : OP::UCOMISSRegMem, node, sourceRegister,
            MRef_const(doubleSource ? cg->findOrCreate8ByteConstant(node, 0) : cg->findOrCreate4ByteConstant(node, 0),
                cg),
            cg);
        // load max int if source is positive, note that for long case, LLONG_MAX << 1 is loaded as it will be shifted
        // right
        Inst_RegMem(OP::CMOVARegMem(longTarget), node, targetRegister,
            MRef_const(longTarget ? cg->findOrCreate8ByteConstant(node, LLONG_MAX << 1)
                                  : cg->findOrCreate4ByteConstant(node, INT_MAX),
                cg),
            cg);
        // load zero if source is NaN
        Inst_RegMem(OP::CMOVPRegMem(longTarget), node, targetRegister,
            MRef_const(longTarget ? cg->findOrCreate8ByteConstant(node, 0) : cg->findOrCreate4ByteConstant(node, 0),
                cg),
            cg);

        Inst_Label(OP::JMP4, node, endLabel, cg);
        og.endOutlinedInstructionSequence();
    }

    Inst_Label(OP::label, node, endLabel, deps, cg);
    if (longTarget) {
        Inst_Reg(OP::ROR8Reg1, node, targetRegister, cg);
    }

    node->setRegister(targetRegister);
    cg->decReferenceCount(child);
    return targetRegister;
}

TR::Register *J9::X86::TreeEvaluator::f2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    TR_ASSERT_FATAL(cg->comp()->target().is32Bit(), "AMD64 uses f2iEvaluator for this");
    return TR::TreeEvaluator::fpConvertToLong(node, cg->symRefTab()->findOrCreateRuntimeHelper(TR_IA32floatToLong), cg);
}

TR::Register *J9::X86::TreeEvaluator::d2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    TR_ASSERT_FATAL(cg->comp()->target().is32Bit(), "AMD64 uses f2iEvaluator for this");

    return TR::TreeEvaluator::fpConvertToLong(node, cg->symRefTab()->findOrCreateRuntimeHelper(TR_IA32doubleToLong),
        cg);
}

/*
 * J9 X86 specific tree evaluator table overrides
 */
extern void TEMPORARY_initJ9X86TreeEvaluatorTable(TR::CodeGenerator *cg)
{
    OMR::TreeEvaluatorFunctionPointerTable tet = cg->getTreeEvaluatorTable();

    tet[TR::f2i] = TR::TreeEvaluator::f2iEvaluator;
    tet[TR::f2iu] = TR::TreeEvaluator::f2iEvaluator;
    tet[TR::f2l] = TR::TreeEvaluator::f2iEvaluator;
    tet[TR::f2lu] = TR::TreeEvaluator::f2iEvaluator;
    tet[TR::d2i] = TR::TreeEvaluator::f2iEvaluator;
    tet[TR::d2iu] = TR::TreeEvaluator::f2iEvaluator;
    tet[TR::d2l] = TR::TreeEvaluator::f2iEvaluator;
    tet[TR::d2lu] = TR::TreeEvaluator::f2iEvaluator;
    tet[TR::monent] = TR::TreeEvaluator::monentEvaluator;
    tet[TR::monexit] = TR::TreeEvaluator::monexitEvaluator;
    tet[TR::monexitfence] = TR::TreeEvaluator::monexitfenceEvaluator;
    tet[TR::asynccheck] = TR::TreeEvaluator::asynccheckEvaluator;
    tet[TR:: instanceof ] = TR::TreeEvaluator::checkcastinstanceofEvaluator;
    tet[TR::checkcast] = TR::TreeEvaluator::checkcastinstanceofEvaluator;
    tet[TR::checkcastAndNULLCHK] = TR::TreeEvaluator::checkcastinstanceofEvaluator;
    tet[TR::New] = TR::TreeEvaluator::newEvaluator;
    tet[TR::newarray] = TR::TreeEvaluator::newEvaluator;
    tet[TR::anewarray] = TR::TreeEvaluator::newEvaluator;
    tet[TR::variableNew] = TR::TreeEvaluator::newEvaluator;
    tet[TR::variableNewArray] = TR::TreeEvaluator::newEvaluator;
    tet[TR::multianewarray] = TR::TreeEvaluator::multianewArrayEvaluator;
    tet[TR::arraylength] = TR::TreeEvaluator::arraylengthEvaluator;
    tet[TR::lookup] = TR::TreeEvaluator::lookupEvaluator;
    tet[TR::exceptionRangeFence] = TR::TreeEvaluator::exceptionRangeFenceEvaluator;
    tet[TR::NULLCHK] = TR::TreeEvaluator::NULLCHKEvaluator;
    tet[TR::ZEROCHK] = TR::TreeEvaluator::ZEROCHKEvaluator;
    tet[TR::ResolveCHK] = TR::TreeEvaluator::resolveCHKEvaluator;
    tet[TR::ResolveAndNULLCHK] = TR::TreeEvaluator::resolveAndNULLCHKEvaluator;
    tet[TR::DIVCHK] = TR::TreeEvaluator::DIVCHKEvaluator;
    tet[TR::BNDCHK] = TR::TreeEvaluator::BNDCHKEvaluator;
    tet[TR::ArrayCopyBNDCHK] = TR::TreeEvaluator::ArrayCopyBNDCHKEvaluator;
    tet[TR::BNDCHKwithSpineCHK] = TR::TreeEvaluator::BNDCHKwithSpineCHKEvaluator;
    tet[TR::SpineCHK] = TR::TreeEvaluator::BNDCHKwithSpineCHKEvaluator;
    tet[TR::ArrayStoreCHK] = TR::TreeEvaluator::ArrayStoreCHKEvaluator;
    tet[TR::ArrayCHK] = TR::TreeEvaluator::ArrayCHKEvaluator;
    tet[TR::MethodEnterHook] = TR::TreeEvaluator::conditionalHelperEvaluator;
    tet[TR::MethodExitHook] = TR::TreeEvaluator::conditionalHelperEvaluator;
    tet[TR::allocationFence] = TR::TreeEvaluator::NOPEvaluator;
    tet[TR::loadFence] = TR::TreeEvaluator::barrierFenceEvaluator;
    tet[TR::storeFence] = TR::TreeEvaluator::barrierFenceEvaluator;
    tet[TR::fullFence] = TR::TreeEvaluator::barrierFenceEvaluator;
    tet[TR::ihbit] = TR::TreeEvaluator::integerHighestOneBit;
    tet[TR::ilbit] = TR::TreeEvaluator::integerLowestOneBit;
    tet[TR::inolz] = TR::TreeEvaluator::integerNumberOfLeadingZeros;
    tet[TR::inotz] = TR::TreeEvaluator::integerNumberOfTrailingZeros;
    tet[TR::lhbit] = TR::TreeEvaluator::longHighestOneBit;
    tet[TR::llbit] = TR::TreeEvaluator::longLowestOneBit;
    tet[TR::lnolz] = TR::TreeEvaluator::longNumberOfLeadingZeros;
    tet[TR::lnotz] = TR::TreeEvaluator::longNumberOfTrailingZeros;
    tet[TR::tstart] = TR::TreeEvaluator::tstartEvaluator;
    tet[TR::tfinish] = TR::TreeEvaluator::tfinishEvaluator;
    tet[TR::tabort] = TR::TreeEvaluator::tabortEvaluator;

#if defined(TR_TARGET_32BIT)
    // 32-bit overrides
    tet[TR::f2l] = TR::TreeEvaluator::f2lEvaluator;
    tet[TR::f2lu] = TR::TreeEvaluator::f2lEvaluator;
    tet[TR::d2l] = TR::TreeEvaluator::d2lEvaluator;
    tet[TR::d2lu] = TR::TreeEvaluator::d2lEvaluator;
    tet[TR::ldiv] = TR::TreeEvaluator::integerPairDivEvaluator;
    tet[TR::lrem] = TR::TreeEvaluator::integerPairRemEvaluator;
#endif
}

static void generateCommonLockNurseryCodes(TR::Node *node, TR::CodeGenerator *cg,
    bool monent, // true for VMmonentEvaluator, false for VMmonexitEvaluator
    TR::LabelSymbol *monitorLookupCacheLabel, TR::LabelSymbol *fallThruFromMonitorLookupCacheLabel,
    TR::LabelSymbol *snippetLabel, uint32_t &numDeps, int &lwOffset, TR::Register *objectClassReg,
    TR::Register *&lookupOffsetReg, TR::Register *vmThreadReg, TR::Register *objectReg)
{
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
    if (comp->getOption(TR_EnableMonitorCacheLookup)) {
        if (monent)
            lwOffset = 0;
        Inst_Label(OP::JLE4, node, monitorLookupCacheLabel, cg);
        Inst_Label(OP::JMP4, node, fallThruFromMonitorLookupCacheLabel, cg);

        Inst_Label(OP::label, node, monitorLookupCacheLabel, cg);

        lookupOffsetReg = cg->allocateRegister();
        numDeps++;

        int32_t offsetOfMonitorLookupCache = offsetof(J9VMThread, objectMonitorLookupCache);

        // Inst_RegMem(OP::LRegMem(), node, objectClassReg,
        // MRef_Bdisp32(vmThreadReg, offsetOfMonitorLookupCache, cg), cg);
        Inst_RegReg(OP::MOVRegReg(), node, lookupOffsetReg, objectReg, cg);

        Inst_RegImm(OP::SARRegImm1(comp->target().is64Bit()), node, lookupOffsetReg,
            trailingZeroes(TR::Compiler->om.getObjectAlignmentInBytes()), cg);

        J9JavaVM *jvm = fej9->getJ9JITConfig()->javaVM;
        Inst_RegImm(OP::ANDRegImms(), node, lookupOffsetReg, J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE - 1, cg);
        Inst_RegImm(OP::SHLRegImm1(), node, lookupOffsetReg, trailingZeroes(TR::Compiler->om.sizeofReferenceField()),
            cg);
        Inst_RegMem((comp->target().is64Bit() && fej9->generateCompressedLockWord()) ? OP::L4RegMem : OP::LRegMem(),
            node, objectClassReg, MRef_BISdisp32(vmThreadReg, lookupOffsetReg, 0, offsetOfMonitorLookupCache, cg), cg);

        Inst_RegReg(OP::TESTRegReg(), node, objectClassReg, objectClassReg, cg);
        Inst_Label(OP::JE4, node, snippetLabel, cg);

        int32_t offsetOfMonitor = offsetof(J9ObjectMonitor, monitor);
        Inst_RegMem(OP::LRegMem(), node, lookupOffsetReg, MRef_Bdisp32(objectClassReg, offsetOfMonitor, cg), cg);

        int32_t offsetOfUserData = offsetof(J9ThreadAbstractMonitor, userData);
        Inst_RegMem(OP::LRegMem(), node, lookupOffsetReg, MRef_Bdisp32(lookupOffsetReg, offsetOfUserData, cg), cg);

        Inst_RegReg(OP::CMPRegReg(), node, lookupOffsetReg, objectReg, cg);
        Inst_Label(OP::JNE4, node, snippetLabel, cg);

        int32_t offsetOfAlternateLockWord = offsetof(J9ObjectMonitor, alternateLockword);
        // Inst_RegMem(OP::LRegMem(), node, lookupOffsetReg,
        // MRef_Bdisp32(objectClassReg, offsetOfAlternateLockWord, cg), cg);
        Inst_RegImm(OP::ADDRegImms(), node, objectClassReg, offsetOfAlternateLockWord, cg);
        // Inst_RegReg(OP::ADDRegReg(), node, objectClassReg, lookupOffsetReg, cg);
        Inst_RegReg(OP::SUBRegReg(), node, objectClassReg, objectReg, cg);

        Inst_Label(OP::label, node, fallThruFromMonitorLookupCacheLabel, cg);
    } else
        Inst_Label(OP::JLE4, node, snippetLabel, cg);
}

#ifdef TR_TARGET_32BIT
TR::Register *J9::X86::I386::TreeEvaluator::conditionalHelperEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // used by asynccheck, methodEnterhook, and methodExitHook

    // Decrement the reference count on the constant placeholder parameter to
    // the MethodEnterHook call.  An evaluation isn't necessary because the
    // constant value isn't used here.
    //
    if (node->getOpCodeValue() == TR::MethodEnterHook) {
        if (node->getSecondChild()->getOpCode().isCall() && node->getSecondChild()->getNumChildren() > 1) {
            cg->decReferenceCount(node->getSecondChild()->getFirstChild());
        }
    }

    // The child contains an inline test.
    //
    TR::Node *testNode = node->getFirstChild();
    TR::Node *secondChild = testNode->getSecondChild();
    if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL) {
        int32_t value = secondChild->getInt();
        TR::Node *firstChild = testNode->getFirstChild();
        OP::Mnemonic opCode;
        if (value >= -128 && value <= 127)
            opCode = OP::CMP4MemImms;
        else
            opCode = OP::CMP4MemImm4;
        TR::MemoryReference *memRef = MRef_node(firstChild, cg);
        Inst_MemImm(opCode, node, memRef, value, cg);
        memRef->decNodeReferenceCounts(cg);
        cg->decReferenceCount(secondChild);
    } else {
        TR_X86CompareAnalyser temp(cg);
        temp.integerCompareAnalyser(testNode, OP::CMP4RegReg, OP::CMP4RegMem, OP::CMP4MemReg);
    }

    TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    TR::LabelSymbol *reStartLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    TR::LabelSymbol *snippetLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    startLabel->setStartInternalControlFlow();
    reStartLabel->setEndInternalControlFlow();
    Inst_Label(OP::label, node, startLabel, cg);
    Inst_Label(testNode->getOpCodeValue() == TR::icmpeq ? OP::JE4 : OP::JNE4, node, snippetLabel, cg);

    TR::Snippet *snippet;
    if (node->getNumChildren() == 2)
        snippet
            = new (cg->trHeapMemory()) TR::X86HelperCallSnippet(cg, reStartLabel, snippetLabel, node->getSecondChild());
    else
        snippet = new (cg->trHeapMemory())
            TR::X86HelperCallSnippet(cg, node, reStartLabel, snippetLabel, node->getSymbolReference());

    cg->addSnippet(snippet);

    Inst_Label(OP::label, node, reStartLabel, cg);
    cg->decReferenceCount(testNode);
    return NULL;
}
#endif

#ifdef TR_TARGET_64BIT
TR::Register *J9::X86::AMD64::TreeEvaluator::conditionalHelperEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // TODO:AMD64: Try to common this with the IA32 version

    // used by asynccheck, methodEnterHook, and methodExitHook

    // The trees for TR::MethodEnterHook are expected to look like one of the following only:
    //
    // (1)   Static Method
    //
    //       TR::MethodEnterHook
    //          icmpne
    //             iload eventFlags (VM Thread)
    //             iconst 0
    //          vcall (jitReportMethodEnter)
    //             aconst (RAM method)
    //
    // (2)   Virtual Method
    //
    //       TR::MethodEnterHook
    //          icmpne
    //             iload eventFlags (VM Thread)
    //             iconst 0
    //          vcall (jitReportMethodEnter)
    //             aload (receiver parameter)
    //             aconst (RAM method)
    //
    //
    // The tree for TR::MethodExitHook is expected to look like the following:
    //
    //    TR::MethodExitHook
    //       icmpne
    //          iload (MethodExitHook table entry)
    //          iconst 0
    //       vcall (jitReportMethodExit)
    //          aconst (RAM method)
    //

    // The child contains an inline test.
    //
    TR::Node *testNode = node->getFirstChild();
    TR::Node *secondChild = testNode->getSecondChild();
    bool testIs64Bit = TR::TreeEvaluator::getNodeIs64Bit(secondChild, cg);
    bool testIsEQ = testNode->getOpCodeValue() == TR::icmpeq || testNode->getOpCodeValue() == TR::lcmpeq;

    TR::Register *thisReg = NULL;
    TR::Register *ramMethodReg = NULL;

    // The receiver and RAM method parameters must be evaluated outside of the internal control flow region if it is
    // commoned, and their registers added to the post dependency condition on the merge label.
    //
    // The reference counts will be decremented when the call node is evaluated.
    //
    if (node->getOpCodeValue() == TR::MethodEnterHook || node->getOpCodeValue() == TR::MethodExitHook) {
        TR::Node *callNode = node->getSecondChild();

        if (callNode->getNumChildren() > 1) {
            if (callNode->getFirstChild()->getReferenceCount() > 1)
                thisReg = cg->evaluate(callNode->getFirstChild());

            if (callNode->getSecondChild()->getReferenceCount() > 1)
                ramMethodReg = cg->evaluate(callNode->getSecondChild());
        } else {
            if (callNode->getFirstChild()->getReferenceCount() > 1)
                ramMethodReg = cg->evaluate(callNode->getFirstChild());
        }
    }

    if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL
        && (!testIs64Bit || IS_32BIT_SIGNED(secondChild->getLongInt()))) {
        // Try to compare memory directly with immediate
        //
        TR::MemoryReference *memRef = MRef_node(testNode->getFirstChild(), cg);
        OP::Mnemonic op;

        if (testIs64Bit) {
            int64_t value = secondChild->getLongInt();
            op = IS_8BIT_SIGNED(value) ? OP::CMP8MemImms : OP::CMP8MemImm4;
            Inst_MemImm(op, node, memRef, value, cg);
        } else {
            int32_t value = secondChild->getInt();
            op = IS_8BIT_SIGNED(value) ? OP::CMP4MemImms : OP::CMP4MemImm4;
            Inst_MemImm(op, node, memRef, value, cg);
        }

        memRef->decNodeReferenceCounts(cg);
        cg->decReferenceCount(secondChild);
    } else {
        TR_X86CompareAnalyser temp(cg);
        temp.integerCompareAnalyser(testNode, OP::CMPRegReg(testIs64Bit), OP::CMPRegMem(testIs64Bit),
            OP::CMPMemReg(testIs64Bit));
    }

    TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    TR::LabelSymbol *reStartLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    TR::LabelSymbol *snippetLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    startLabel->setStartInternalControlFlow();
    reStartLabel->setEndInternalControlFlow();

    TR::Instruction *startInstruction = Inst_Label(OP::label, node, startLabel, cg);

    if (node->getOpCodeValue() == TR::MethodEnterHook || node->getOpCodeValue() == TR::MethodExitHook) {
        TR::Node *callNode = node->getSecondChild();

        // Generate an inverted jump around the call.  This is necessary because we want to do the call inline rather
        // than through the snippet.
        //
        Inst_Label(testIsEQ ? OP::JNE4 : OP::JE4, node, reStartLabel, cg);
        TR::TreeEvaluator::performCall(callNode, false, false, cg);

        // Collect postconditions from the internal control flow region and put
        // them on the restart label to prevent spills in the internal control
        // flow region.
        // TODO:AMD64: This would be a useful general facility to have.
        //
        TR::Machine *machine = cg->machine();
        TR::RegisterDependencyConditions *postConditions = new (cg->trHeapMemory())
            TR::RegisterDependencyConditions((uint8_t)0, TR::RealRegister::NumRegisters, cg->trMemory());
        if (thisReg)
            postConditions->addPostCondition(thisReg, TR::RealRegister::NoReg, cg);

        if (ramMethodReg)
            postConditions->addPostCondition(ramMethodReg, TR::RealRegister::NoReg, cg);

        for (TR::Instruction *cursor = cg->getAppendInstruction(); cursor != startInstruction;
             cursor = cursor->getPrev()) {
            TR::RegisterDependencyConditions *cursorDeps = cursor->getDependencyConditions();
            if (cursorDeps && cursor->getOpCodeValue() != OP::assocreg) {
                for (int32_t i = 0; i < cursorDeps->getNumPostConditions(); i++) {
                    TR::RegisterDependency *cursorPostCondition
                        = cursorDeps->getPostConditions()->getRegisterDependency(i);
                    postConditions->unionPostCondition(cursorPostCondition->getRegister(),
                        cursorPostCondition->getRealRegister(), cg);
                }
            }
        }
        postConditions->stopAddingPostConditions();

        Inst_Label(OP::label, node, reStartLabel, postConditions, cg);
    } else {
        Inst_Label(testIsEQ ? OP::JE4 : OP::JNE4, node, snippetLabel, cg);

        TR::Snippet *snippet;
        if (node->getNumChildren() == 2)
            snippet = new (cg->trHeapMemory())
                TR::X86HelperCallSnippet(cg, reStartLabel, snippetLabel, node->getSecondChild());
        else
            snippet = new (cg->trHeapMemory())
                TR::X86HelperCallSnippet(cg, node, reStartLabel, snippetLabel, node->getSymbolReference());

        cg->addSnippet(snippet);
        Inst_Label(OP::label, node, reStartLabel, cg);
    }

    cg->decReferenceCount(testNode);
    return NULL;
}
#endif

TR::Register *J9::X86::TreeEvaluator::performHeapLoadWithReadBarrier(TR::Node *node, TR::CodeGenerator *cg)
{
#ifndef OMR_GC_CONCURRENT_SCAVENGER
    TR_ASSERT_FATAL(0, "Concurrent Scavenger not supported.");
    return NULL;
#else
    TR::Compilation *comp = cg->comp();
    bool use64BitClasses = comp->target().is64Bit() && !comp->useCompressedPointers();

    TR::MemoryReference *sourceMR = MRef_node(node, cg);
    TR::Register *address
        = TR::TreeEvaluator::loadMemory(node, sourceMR, TR_RematerializableLoadEffectiveAddress, false, cg);
    address->setMemRef(sourceMR);
    sourceMR->decNodeReferenceCounts(cg);

    TR::Register *object = cg->allocateRegister();
    TR::Instruction *load = Inst_RegMem(OP::LRegMem(use64BitClasses), node, object, MRef_Bdisp32(address, 0, cg), cg);
    cg->setImplicitExceptionPoint(load);

    switch (TR::Compiler->om.readBarrierType()) {
        case gc_modron_readbar_none:
            TR_ASSERT(false, "This path should only be reached when a read barrier is required.");
            break;
        case gc_modron_readbar_always:
            Inst_MemReg(OP::SMemReg(), node,
                MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, floatTemp1), cg), address, cg);
            Inst_HelperCall(node, TR_softwareReadBarrier, NULL, cg);
            Inst_RegMem(OP::LRegMem(use64BitClasses), node, object, MRef_Bdisp32(address, 0, cg), cg);
            break;
        case gc_modron_readbar_range_check: {
            TR::LabelSymbol *begLabel = generateLabelSymbol(cg);
            TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
            TR::LabelSymbol *rdbarLabel = generateLabelSymbol(cg);
            begLabel->setStartInternalControlFlow();
            endLabel->setEndInternalControlFlow();

            TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)2, 2, cg);
            deps->addPreCondition(object, TR::RealRegister::NoReg, cg);
            deps->addPreCondition(address, TR::RealRegister::NoReg, cg);
            deps->addPostCondition(object, TR::RealRegister::NoReg, cg);
            deps->addPostCondition(address, TR::RealRegister::NoReg, cg);

            Inst_Label(OP::label, node, begLabel, cg);
            Inst_RegMem(OP::CMPRegMem(use64BitClasses), node, object,
                MRef_Bdisp32(cg->getVMThreadRegister(), comp->fej9()->thisThreadGetEvacuateBaseAddressOffset(), cg),
                cg);
            Inst_Label(OP::JAE4, node, rdbarLabel, cg);
            {
                TR_OutlinedInstructionsGenerator og(rdbarLabel, node, cg);
                Inst_RegMem(OP::CMPRegMem(use64BitClasses), node, object,
                    MRef_Bdisp32(cg->getVMThreadRegister(), comp->fej9()->thisThreadGetEvacuateTopAddressOffset(), cg),
                    cg);
                Inst_Label(OP::JA4, node, endLabel, cg);
                Inst_MemReg(OP::SMemReg(), node,
                    MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, floatTemp1), cg), address, cg);
                Inst_HelperCall(node, TR_softwareReadBarrier, NULL, cg);
                Inst_RegMem(OP::LRegMem(use64BitClasses), node, object, MRef_Bdisp32(address, 0, cg), cg);
                Inst_Label(OP::JMP4, node, endLabel, cg);
                og.endOutlinedInstructionSequence();
            }
            Inst_Label(OP::label, node, endLabel, deps, cg);
        } break;
        default:
            TR_ASSERT(false, "Unsupported Read Barrier Type.");
            break;
    }
    cg->stopUsingRegister(address);
    return object;
#endif
}

// Should only be called for pure TR::awrtbar and TR::awrtbari nodes.
//
TR::Register *J9::X86::TreeEvaluator::writeBarrierEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::MemoryReference *storeMR = MRef_node(node, cg);
    TR::Node *destOwningObject;
    TR::Node *sourceObject;
    TR::Compilation *comp = cg->comp();
    bool usingCompressedPointers = false;
    bool useShiftedOffsets = (TR::Compiler->om.compressedReferenceShiftOffset() != 0);

    if (node->getOpCodeValue() == TR::awrtbari) {
        destOwningObject = node->getChild(2);
        sourceObject = node->getSecondChild();
        if (comp->useCompressedPointers() && (node->getSymbolReference()->getSymbol()->getDataType() == TR::Address)
            && (node->getSecondChild()->getDataType() != TR::Address)) {
            usingCompressedPointers = true;

            if (useShiftedOffsets) {
                while ((sourceObject->getNumChildren() > 0) && (sourceObject->getOpCodeValue() != TR::a2l))
                    sourceObject = sourceObject->getFirstChild();
                if (sourceObject->getOpCodeValue() == TR::a2l)
                    sourceObject = sourceObject->getFirstChild();
                // this is required so that different registers are
                // allocated for the actual store and translated values
                sourceObject->incReferenceCount();
            }
        }
    } else {
        TR_ASSERT((node->getOpCodeValue() == TR::awrtbar), "expecting a TR::wrtbar");
        destOwningObject = node->getSecondChild();
        sourceObject = node->getFirstChild();
    }

    TR_X86ScratchRegisterManager *scratchRegisterManager
        = cg->generateScratchRegisterManager(comp->target().is64Bit() ? 15 : 7);

    TR::TreeEvaluator::VMwrtbarWithStoreEvaluator(node, storeMR, scratchRegisterManager, destOwningObject, sourceObject,
        (node->getOpCodeValue() == TR::awrtbari) ? true : false, cg, false);

    if (comp->useAnchors() && (node->getOpCodeValue() == TR::awrtbari))
        node->setStoreAlreadyEvaluated(true);

    if (usingCompressedPointers)
        cg->decReferenceCount(node->getSecondChild());

    return NULL;
}

TR::Register *J9::X86::TreeEvaluator::monentEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    if (cg->enableRematerialisation() && cg->supportsStaticMemoryRematerialization())
        TR::TreeEvaluator::removeLiveDiscardableStatics(cg);

    return TR::TreeEvaluator::VMmonentEvaluator(node, cg);
}

TR::Register *J9::X86::TreeEvaluator::monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    if (cg->enableRematerialisation() && cg->supportsStaticMemoryRematerialization())
        TR::TreeEvaluator::removeLiveDiscardableStatics(cg);

    return TR::TreeEvaluator::VMmonexitEvaluator(node, cg);
}

TR::Register *J9::X86::TreeEvaluator::asynccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // Generate the test and branch for async message processing.
    //
    TR::Node *compareNode = node->getFirstChild();
    TR::Node *secondChild = compareNode->getSecondChild();
    TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
    TR::Compilation *comp = cg->comp();

    if (comp->getOption(TR_RTGCMapCheck)) {
        TR::TreeEvaluator::asyncGCMapCheckPatching(node, cg, snippetLabel);
    } else {
        TR_ASSERT_FATAL(secondChild->getOpCode().isLoadConst(),
            "unrecognized asynccheck test: special async check value is not a constant");

        TR::MemoryReference *mr = MRef_node(compareNode->getFirstChild(), cg);
        if ((secondChild->getRegister() != NULL)
            || (comp->target().is64Bit() && !IS_32BIT_SIGNED(secondChild->getLongInt()))) {
            TR::Register *valueReg = cg->evaluate(secondChild);
            TR::X86CheckAsyncMessagesMemRegInstruction *ins
                = Inst_CheckAsyncMessages(node, OP::CMPMemReg(), mr, valueReg, cg);
        } else {
            int32_t value = secondChild->getInt();
            OP::Mnemonic op = (value < 127 && value >= -128) ? OP::CMPMemImms() : OP::CMPMemImm4();
            TR::X86CheckAsyncMessagesMemImmInstruction *ins = Inst_CheckAsyncMessages(node, op, mr, value, cg);
        }

        mr->decNodeReferenceCounts(cg);
        cg->decReferenceCount(secondChild);
    }

    TR::LabelSymbol *startControlFlowLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *endControlFlowLabel = generateLabelSymbol(cg);

    bool testIsEqual = compareNode->getOpCodeValue() == TR::icmpeq || compareNode->getOpCodeValue() == TR::lcmpeq;

    TR_ASSERT(testIsEqual, "unrecognized asynccheck test: test is not equal");

    startControlFlowLabel->setStartInternalControlFlow();
    Inst_Label(OP::label, node, startControlFlowLabel, cg);

    Inst_Label(testIsEqual ? OP::JE4 : OP::JNE4, node, snippetLabel, cg);

    {
        TR_OutlinedInstructionsGenerator og(snippetLabel, node, cg);
        Inst_ImmSym(OP::CALLImm4, node, (uintptr_t)node->getSymbolReference()->getMethodAddress(),
            node->getSymbolReference(), cg)
            ->setNeedsGCMap(0xFF00FFFF);
        Inst_Label(OP::JMP4, node, endControlFlowLabel, cg);
        og.endOutlinedInstructionSequence();
    }

    endControlFlowLabel->setEndInternalControlFlow();
    Inst_Label(OP::label, node, endControlFlowLabel, cg);

    cg->decReferenceCount(compareNode);

    return NULL;
}

// Handles newObject, newArray, anewArray
//
TR::Register *J9::X86::TreeEvaluator::newEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR::Register *targetRegister = NULL;

    if (TR::TreeEvaluator::requireHelperCallValueTypeAllocation(node, cg)) {
        TR_OpaqueClassBlock *classInfo;
        bool spillFPRegs = comp->canAllocateInlineOnStack(node, classInfo) <= 0;
        return TR::TreeEvaluator::performHelperCall(node, NULL, TR::acall, spillFPRegs, cg);
    }

    targetRegister = TR::TreeEvaluator::VMnewEvaluator(node, cg);
    if (!targetRegister) {
        // Inline object allocation wasn't generated, just generate a call to the helper.
        // If we know that the class is fully initialized, we don't have to spill
        // the FP registers.
        //
        TR_OpaqueClassBlock *classInfo;
        bool spillFPRegs = (comp->canAllocateInlineOnStack(node, classInfo) <= 0);
        targetRegister = TR::TreeEvaluator::performHelperCall(node, NULL, TR::acall, spillFPRegs, cg);
    }

    return targetRegister;
}

/**
 * @brief Generate inline code for allocating a 2D array (multianewarray with 2 dimensions).
 *
 * This implementation allocates a two-dimensional array in a single contiguous memory block.
 *
 * Memory Layout (from low to high addresses):
 *   1. Spine array header
 *   2. Spine array slots (m references to leaf arrays)
 *   3. First leaf array header
 *   4. First leaf array elements (n elements)
 *   5. ... (repeat for all m leaf arrays)
 *   6. Last leaf array header
 *   7. Last leaf array elements (n elements)
 *
 * The inline code runs as follows:
 *   @code
 *   if (m == 0) {
 *     totalSize = zero array size
 *   } else if (m < 0) { // negative array size
 *     goto OOL VM helper call
 *   } else {
 *     spineSize = spineArrayHeaderSize + m * referenceSize
 *     if (n == 0) {
 *       leafSize = zero array size
 *     } else if (n < 0) { // negative array size
 *       goto OOL VM helper
 *     } else {
 *       leafSize = leafArrayHeaderSize + n * leafArrayElementSize
 *       leafBlockSize = m * leafSize
 *     }
 *     totalSize = spineSize + leafBlockSize
 *   }
 *   spinePtr = vmThread->heapAlloc
 *   allocEnd = spinePtr + totalSize
 *   if (allocEnd > vmThread->heapTop) { // heap overflow
 *     goto OOL VM helper
 *   }
 *   vmThread->heapAlloc = allocEnd
 *   spinePtr[classOffset] = spineArrayClass
 *   if (m == 0) {
 *     goto OOL zero size array init
 *   }
 *   spinePtr[sizeOffset] = m
 *   memset(spinePtr + spineArrayHeaderSize, 0, leafBlockSize)
 *   leafPtr = allocEnd - leafSize
 *   if (n == 0 && offheap enabled) {
 *     goto OOL zero size array init loop
 *   }
 *   for (i = m - 1; i >= 0; i--) { // iterate backwards
 *     leafPtr[classOffset] = leafArrayClass
 *     leafPtr[sizeOffset] = n
 *     spinePtr[spineArrayHeaderSize + i * referenceSize] = leafPtr
 *     leafPtr -= leafSize
 *   }
 *   done:
 *   @endcode
 *
 * There are three OOL paths:
 * - OOL VM helper: call the VM helper to allocate the array
 *   @code
 *   spinePtr = jitAMultiANewArray()
 *   goto done
 *   @endcode
 * - OOL zero size array init: initialize the spine array to zero
 *   @code
 *   spinePtr[sizeOffset] = 0
 *   spinePtr[mustBeZeroOffset] = 0
 *   goto done
 *   @endcode
 * - OOL zero size array init loop: initialize the leaf arrays to zero
 *   @code
 *   for (i = m - 1; i >= 0; i--) { // iterate backwards
 *     leafPtr[classOffset] = leafArrayClass
 *     spinePtr[spineArrayHeaderSize + i * referenceSize] = leafPtr
 *     leafPtr -= leafSize
 *   }
 *   goto done
 *   @endcode
 *
 * @note Must only be used for arrays of exactly two dimensions
 * @note Falls back to VM helper for error cases (negative dimensions, heap overflow)
 *
 * @param node                   The multianewarray IL node
 * @param cg                     The code generator instance
 * @param leafArrayElementSize   Size in bytes of each element in the leaf arrays
 * @return                       Register containing pointer to the allocated spine array
 */
static TR::Register *generate2DArrayWithInlineAllocators(TR::Node *node, TR::CodeGenerator *cg,
    int32_t leafArrayElementSize)
{
    TR::Compilation *comp = cg->comp();

    TR::Node *dimsPtrNode = node->getFirstChild(); // ptr to array of sizes, one for each dimension. Array construction
                                                   // stops at the outermost zero size, if any
    TR::Node *nDimsNode
        = node->getSecondChild(); // number of dimensions - this is fixed in the bytecode, so compile time constant 2
    TR::Node *classNode = node->getThirdChild(); // class of the outermost dimension

    TR_J9VMBase *fej9 = comp->fej9();

    /*
     * For a new m*n array of X we allocate the first dimension (m) array spine followed
     * by the second dimension (n) array leaves, inserting padding to align the arrays.
     * This looks like the following:
     *
     *    |-------------------------| allocation end (vmThread->heapAlloc after)
     *    | n empty X array slots   | \
     *    |-------------------------|  |
     *    | mth leaf array header   |  |
     *    |-------------------------|  |
     *    | padding for alignment   |  |
     *    |-------------------------|  | leaf arrays
     *                ...              | (m * (header size + n * X's size + leaf padding)) bytes
     *    |-------------------------|  |
     *    | n empty X array slots   |  |
     *    |-------------------------|  |
     *    | 2nd leaf array header   |  |
     *    |-------------------------|  |
     *    | padding for alignment   |  |
     *    |-------------------------|  |
     *    | n empty X array slots   |  |
     *    |-------------------------|  |
     *    | 1st leaf array header   | /
     *    |-------------------------|
     *    | padding for alignment   | \
     *    |-------------------------|  | spine array
     *    | m reference array slots |  | (spine padding + header size + m * reference size) bytes
     *    |-------------------------|  |
     *    | spine array header      | /
     *    |-------------------------| allocation start (vmThread->heapAlloc before)
     *
     */

    // alignment requirement
    int32_t alignmentInBytes = TR::Compiler->om.getObjectAlignmentInBytes();
    // a length>0 array object would *not* require alignment if both a single element
    // and the header are already the exact multiple of alignment; otherwise alignment is needed
    uintptr_t contiguousArrayHeaderSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
    bool needsAlignLeaf = (OMR::align(leafArrayElementSize, alignmentInBytes) != leafArrayElementSize);
    bool needsAlignHeader = (contiguousArrayHeaderSize != OMR::align(contiguousArrayHeaderSize, alignmentInBytes));
    uintptr_t referenceSize = TR::Compiler->om.sizeofReferenceField();
    bool needsAlignSpine = (OMR::align(referenceSize, alignmentInBytes) != referenceSize) || needsAlignHeader;

    bool use64BitClasses = !TR::Compiler->om.generateCompressedObjectHeaders();

    uintptr_t classOffset = TR::Compiler->om.offsetOfObjectVftField();
    uintptr_t sizeOffset = fej9->getOffsetOfContiguousArraySizeField();

#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
    bool isOffHeapAllocationEnabled = TR::Compiler->om.isOffHeapAllocationEnabled();
#endif /* defined(J9VM_GC_SPARSE_HEAP_ALLOCATION) */

    // Generate OOL snippet to call vm helper for error cases
    // Snippet will return to doneLabel with result in spinePtrReg
    TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *helperLabel = generateLabelSymbol(cg);
    doneLabel->setEndInternalControlFlow();
    TR::Register *spinePtrReg = cg->allocateRegister();
    TR_OutlinedInstructions *outlinedHelperCall
        = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::acall, spinePtrReg, helperLabel, doneLabel, cg);
    cg->generateDebugCounter(outlinedHelperCall->getFirstInstruction(),
        TR::DebugCounter::debugCounterName(comp, "helperCalls/%s/(%s)/%d/%d", node->getOpCode().getName(),
            comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
        1, TR::DebugCounter::Cheap);
    cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);

    // load dimensions and class
    TR::Register *dimsPtrReg = cg->evaluate(dimsPtrNode);
    TR::Register *classReg = cg->evaluate(classNode);

    /*
     * Although the number of dimensions remains constant and is not utilized through a register in the mainline ICF,
     * it is referenced in the out-of-line (OOL) code section to set up call parameters. If the node has multiple
     * reference counts, and it is not explicitly attached to the mainline dependency conditions, there is a risk
     * that it may be spilled in the OOL path and reverse-spilled in another path. This can lead to unexpected behavior.
     * To mitigate this, ensure that the dimension node is evaluated and explicitly attached to the mainline dependency.
     */
    TR::Register *nDimsReg = cg->evaluate(nDimsNode);

    // Calculate spine array size

    TR::Register *spineSizeReg = cg->allocateRegister();
    Inst_RegImm(OP::MOV8RegImm4, node, spineSizeReg, contiguousArrayHeaderSize, cg);

    int32_t zeroArraySizeAligned = OMR::align(TR::Compiler->om.discontiguousArrayHeaderSizeInBytes(), alignmentInBytes);
    TR::Register *tempReg = cg->allocateRegister();
    Inst_RegImm(OP::MOV8RegImm4, node, tempReg, zeroArraySizeAligned, cg);

    TR::Register *firstDimReg = cg->allocateRegister();
    Inst_RegMem(OP::MOVSXReg8Mem4, node, firstDimReg, MRef_Bdisp32(dimsPtrReg, 4, cg), cg);

    // if first dim = 0 load the zero array size and skip over calculating the leaf block size
    Inst_RegReg(OP::TEST8RegReg, node, firstDimReg, firstDimReg, cg);
    Inst_RegReg(OP::CMOVE8RegReg, node, spineSizeReg, tempReg, cg);
    TR::LabelSymbol *startControlFlow = generateLabelSymbol(cg);
    startControlFlow->setStartInternalControlFlow();
    Inst_Label(OP::label, node, startControlFlow, cg);
    TR::LabelSymbol *allocateLabel = generateLabelSymbol(cg);
    Inst_Label(OP::JE4, node, allocateLabel, cg);

    // if first dim < 0 go to OOL helper
    Inst_Label(OP::JL4, node, helperLabel, cg);

    // spine size += first dim * reference size
    Inst_RegMem(OP::LEA8RegMem, node, spineSizeReg,
        MRef_BISdisp32(spineSizeReg, firstDimReg, trailingZeroes((int32_t)referenceSize), 0, cg), cg);

    // pad spine size so leaf arrays will be aligned
    if (needsAlignSpine) {
        Inst_RegImm(OP::ADD8RegImm4, node, spineSizeReg, alignmentInBytes - 1, cg);
        Inst_RegImm(OP::AND8RegImm4, node, spineSizeReg, -alignmentInBytes, cg);
    }

    // Calculate leaf array block size

    TR::Register *leafSizeReg = cg->allocateRegister();
    Inst_RegImm(OP::MOV8RegImm4, node, leafSizeReg, contiguousArrayHeaderSize, cg);

    // if second dim = 0 load the zero array size and skip over calculating the leaf size
    TR::Register *secondDimReg = cg->allocateRegister();
    Inst_RegMem(OP::MOVSXReg8Mem4, node, secondDimReg, MRef_Bdisp32(dimsPtrReg, 0, cg), cg);

    Inst_RegReg(OP::TEST8RegReg, node, secondDimReg, secondDimReg, cg);
    Inst_RegReg(OP::CMOVE8RegReg, node, leafSizeReg, tempReg, cg);
    TR::LabelSymbol *calculateLeafBlockSize = generateLabelSymbol(cg);
    Inst_Label(OP::JE4, node, calculateLeafBlockSize, cg);

    // if second dim < 0 go to OOL helper
    Inst_Label(OP::JL4, node, helperLabel, cg);

    // leaf size = header size + second dim * leaf element size
    Inst_RegMem(OP::LEA8RegMem, node, leafSizeReg,
        MRef_BISdisp32(leafSizeReg, secondDimReg, trailingZeroes(leafArrayElementSize), 0, cg), cg);

    // pad leafSize for alignment
    if (needsAlignLeaf) {
        Inst_RegImm(OP::ADD8RegImm4, node, leafSizeReg, alignmentInBytes - 1, cg);
        Inst_RegImm(OP::AND8RegImm4, node, leafSizeReg, -alignmentInBytes, cg);
    }

    Inst_Label(OP::label, node, calculateLeafBlockSize, cg);

    Inst_RegReg(OP::MOV8RegReg, node, tempReg, firstDimReg, cg);
    Inst_RegReg(OP::IMUL8RegReg, node, tempReg, leafSizeReg, cg);

    // spineSize += leafBlockSize
    Inst_RegReg(OP::ADD8RegReg, node, spineSizeReg, tempReg, cg);

    Inst_Label(OP::label, node, allocateLabel, cg);

    // spinePtrReg = vmThread->heapAlloc
    TR::Register *vmThreadReg = cg->getVMThreadRegister();
    Inst_RegMem(OP::L8RegMem, node, spinePtrReg, MRef_Bdisp32(vmThreadReg, offsetof(J9VMThread, heapAlloc), cg), cg);

    // allocEnd = spinePtr + spineSize + leafBlockSize
    TR::Register *leafPtrReg = cg->allocateRegister();
    Inst_RegMem(OP::LEA8RegMem, node, leafPtrReg, MRef_BISdisp32(spinePtrReg, spineSizeReg, 0, 0, cg), cg);

    // if allocEnd > vmThread->heapTop go to helper
    Inst_RegMem(OP::CMP8RegMem, node, leafPtrReg, MRef_Bdisp32(vmThreadReg, offsetof(J9VMThread, heapTop), cg), cg);
    Inst_Label(OP::JA4, node, helperLabel, cg);

    // bump vmThread->heapAlloc to allocate the memory needed
    Inst_MemReg(OP::S8MemReg, node, MRef_Bdisp32(vmThreadReg, offsetof(J9VMThread, heapAlloc), cg), leafPtrReg, cg);

    // load class into spine header
    Inst_MemReg(OP::SMemReg(use64BitClasses), node, MRef_Bdisp32(spinePtrReg, classOffset, cg), classReg, cg);

    // if first dim = 0 goto initialise zero length spine
    TR::LabelSymbol *initZeroLengthLabel = generateLabelSymbol(cg);
    Inst_RegReg(OP::TEST8RegReg, node, firstDimReg, firstDimReg, cg);
    Inst_Label(OP::JE4, node, initZeroLengthLabel, cg);

    // initialise zero length array
    TR_OutlinedInstructionsGenerator zeroLengthOOL(initZeroLengthLabel, node, cg);

    // init size and mustBeZero ('0') fields to 0
    Inst_MemImm(OP::S4MemImm4, node, MRef_Bdisp32(spinePtrReg, fej9->getOffsetOfContiguousArraySizeField(), cg), 0, cg);
    Inst_MemImm(OP::S4MemImm4, node, MRef_Bdisp32(spinePtrReg, fej9->getOffsetOfDiscontiguousArraySizeField(), cg), 0,
        cg);

#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
    if (isOffHeapAllocationEnabled) {
        // Init 1st dim dataAddr slot to 0
        Inst_MemImm(OP::S8MemImm4, node, MRef_Bdisp32(spinePtrReg, fej9->getOffsetOfDiscontiguousDataAddrField(), cg),
            0, cg);
    }
#endif /* J9VM_GC_SPARSE_HEAP_ALLOCATION */

    Inst_Label(OP::JMP4, node, doneLabel, cg);
    zeroLengthOOL.endOutlinedInstructionSequence();

    // otherwise load first dim into spine header
    Inst_MemReg(OP::S4MemReg, node, MRef_Bdisp32(spinePtrReg, sizeOffset, cg), firstDimReg, cg);

    // zero out the leaf portion of the allocation if necessary
    bool clearAllocation = !fej9->tlhHasBeenCleared();
    if (clearAllocation) {
        // adjust leafPtr to the beginning of the leaf array block
        Inst_RegReg(OP::SUB8RegReg, node, leafPtrReg, tempReg, cg);

        // clear leafBlockSize bytes starting at leafPtr
        // leafBlockSize = m * (contiguousArrayHeaderSize + n * leafArrayElementSize + padding)
        // if alignment is needed, then leafBlockSize is a multiple of alignmentInBytes
        // otherwise leafBlockSize is a multiple of the min of contiguousArrayHeaderSize and leafArrayElementSize
        // (assuming both are powers of 2) the largest size we can handle per rep stos iteration is 8 bytes
        int32_t sizeShift = trailingZeroes(8
            | static_cast<int32_t>(
                needsAlignHeader ? alignmentInBytes : (contiguousArrayHeaderSize | leafArrayElementSize)));

        static const OP::Mnemonic xorOpCode[] = { OP::XOR1RegReg, OP::XOR2RegReg, OP::XOR4RegReg, OP::XOR8RegReg };

        Inst_RegReg(xorOpCode[sizeShift], node, spineSizeReg, spineSizeReg, cg);

        if (sizeShift != 0)
            Inst_RegImm(OP::SHRRegImm1(), node, tempReg, sizeShift, cg);

        static const OP::Mnemonic repstosOpCode[] = { OP::REPSTOSB, OP::REPSTOSW, OP::REPSTOSD, OP::REPSTOSQ };

        Inst(repstosOpCode[sizeShift], node, cg);
        // leafPtrReg is pointing to the end of the allocation again
    }

    // load element class
    Inst_RegMem(OP::LRegMem(use64BitClasses), node, tempReg,
        MRef_Bdisp32(classReg, offsetof(J9ArrayClass, componentType), cg), cg);

#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
    if (isOffHeapAllocationEnabled) {
        /* Populate dataAddr slot of spine array. Arrays of non-zero size
         * use contiguous header layout while zero size arrays use discontiguous header layout.
         */
        Inst_RegMem(OP::LEARegMem(), node, spineSizeReg,
            MRef_Bdisp32(spinePtrReg, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg), cg);
        Inst_MemReg(OP::SMemReg(), node, MRef_Bdisp32(spinePtrReg, fej9->getOffsetOfContiguousDataAddrField(), cg),
            spineSizeReg, cg);
    }
#endif /* J9VM_GC_SPARSE_HEAP_ALLOCATION */

    // adjust leafPtr to prepare for loop
    Inst_RegReg(OP::SUB8RegReg, node, leafPtrReg, leafSizeReg, cg);

#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
    // for zero-length offheap arrays, the work to initialize zero length arrays is sufficiently different that a
    // separate loop body is required for leaf arrays
    if (isOffHeapAllocationEnabled) {
        // if second dimension = 0, use OOL loop for initializing zero length arrays
        TR::LabelSymbol *initZeroLengthLoopLabel = generateLabelSymbol(cg);
        Inst_RegReg(OP::TEST8RegReg, node, secondDimReg, secondDimReg, cg);
        Inst_Label(OP::JE4, node, initZeroLengthLoopLabel, cg);

        // initialise zero length arrays
        TR_OutlinedInstructionsGenerator zeroLengthLoopOOL(initZeroLengthLoopLabel, node, cg);
        TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);
        Inst_Label(OP::label, node, loopLabel, cg);

        // initialise leaf array class
        Inst_MemReg(OP::SMemReg(use64BitClasses), node, MRef_Bdisp32(leafPtrReg, classOffset, cg), tempReg, cg);
        // length, mustBeZero, and dataAddr fields are already set to zero since the allocation is zeroed

        // insert leaf array reference into spine array
        // spinePtr[first dim * reference size + (header size - reference size)] = leafPtr
        // subtract reference size to account for the off by one value of first dim
        TR::MemoryReference *spineSlotMemRef = MRef_BISdisp32(spinePtrReg, firstDimReg,
            trailingZeroes((int32_t)referenceSize), contiguousArrayHeaderSize - referenceSize, cg);
        if (comp->useCompressedPointers()) {
            int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();
            Inst_RegReg(OP::MOVRegReg(), node, spineSizeReg, leafPtrReg, cg);
            if (shiftAmount != 0) {
                Inst_RegImm(OP::SHRRegImm1(), node, spineSizeReg, shiftAmount, cg);
            }
            Inst_MemReg(OP::S4MemReg, node, spineSlotMemRef, spineSizeReg, cg);
        } else {
            Inst_MemReg(OP::S8MemReg, node, spineSlotMemRef, leafPtrReg, cg);
        }

        // decrement firstDim and leafPtr and loop back
        Inst_RegReg(OP::SUB8RegReg, node, leafPtrReg, leafSizeReg, cg);
        Inst_Reg(OP::DEC8Reg, node, firstDimReg, cg);
        Inst_Label(OP::JG4, node, loopLabel, cg);

        Inst_Label(OP::JMP4, node, doneLabel, cg);
        zeroLengthLoopOOL.endOutlinedInstructionSequence();
    }
#endif /* J9VM_GC_SPARSE_HEAP_ALLOCATION */

    // check if we can optimize by combining class and size into a single 8-byte write
    // this is possible when using compressed headers and the fields are adjacent
    bool arrayHeaderFitsInGPR = !use64BitClasses && ((classOffset + 4) == sizeOffset);

    if (arrayHeaderFitsInGPR) {
        Inst_RegImm(OP::SHL8RegImm1, node, secondDimReg, 32, cg);
        Inst_RegReg(OP::OR8RegReg, node, secondDimReg, tempReg, cg);
    }

    TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);
    Inst_Label(OP::label, node, loopLabel, cg);

    // initialise leaf array
    if (arrayHeaderFitsInGPR) {
        Inst_MemReg(OP::S8MemReg, node, MRef_Bdisp32(leafPtrReg, classOffset, cg), secondDimReg, cg);
    } else {
        Inst_MemReg(OP::SMemReg(use64BitClasses), node, MRef_Bdisp32(leafPtrReg, classOffset, cg), tempReg, cg);
        Inst_MemReg(OP::S4MemReg, node, MRef_Bdisp32(leafPtrReg, sizeOffset, cg), secondDimReg, cg);
    }

#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
    if (isOffHeapAllocationEnabled) {
        /* Populate dataAddr slot of leaf array. Arrays of non-zero size
         * use contiguous header layout while zero size arrays use discontiguous header layout.
         */
        Inst_RegMem(OP::LEARegMem(), node, spineSizeReg,
            MRef_Bdisp32(leafPtrReg, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg), cg);
        Inst_MemReg(OP::SMemReg(), node, MRef_Bdisp32(leafPtrReg, fej9->getOffsetOfContiguousDataAddrField(), cg),
            spineSizeReg, cg);
    }
#endif /* J9VM_GC_SPARSE_HEAP_ALLOCATION */

    // insert leaf array reference into spine array
    // spinePtr[first dim * reference size + (header size - reference size)] = leafPtr
    // subtract reference size to account for the off by one value of first dim
    TR::MemoryReference *spineSlotMemRef = MRef_BISdisp32(spinePtrReg, firstDimReg,
        trailingZeroes((int32_t)referenceSize), contiguousArrayHeaderSize - referenceSize, cg);
    if (comp->useCompressedPointers()) {
        int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();
        Inst_RegReg(OP::MOVRegReg(), node, spineSizeReg, leafPtrReg, cg);
        if (shiftAmount != 0) {
            Inst_RegImm(OP::SHRRegImm1(), node, spineSizeReg, shiftAmount, cg);
        }
        Inst_MemReg(OP::S4MemReg, node, spineSlotMemRef, spineSizeReg, cg);
    } else {
        Inst_MemReg(OP::S8MemReg, node, spineSlotMemRef, leafPtrReg, cg);
    }

    // decrement firstDim and leafPtr and loop back
    Inst_RegReg(OP::SUB8RegReg, node, leafPtrReg, leafSizeReg, cg);
    Inst_Reg(OP::DEC8Reg, node, firstDimReg, cg);
    Inst_Label(OP::JG4, node, loopLabel, cg);

    // done, OOL helper will return to this point
    TR::RegisterDependencyConditions *deps = RegDeps(0, 14, cg);
    deps->addPostCondition(spinePtrReg, TR::RealRegister::NoReg, cg);

    deps->addPostCondition(nDimsReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(dimsPtrReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(classReg, TR::RealRegister::NoReg, cg);

    deps->addPostCondition(firstDimReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(leafSizeReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(secondDimReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(vmThreadReg, TR::RealRegister::ebp, cg);

    if (clearAllocation) {
        deps->addPostCondition(tempReg, TR::RealRegister::ecx, cg);
        deps->addPostCondition(leafPtrReg, TR::RealRegister::edi, cg);
        deps->addPostCondition(spineSizeReg, TR::RealRegister::eax, cg);
    } else {
        deps->addPostCondition(tempReg, TR::RealRegister::NoReg, cg);
        deps->addPostCondition(leafPtrReg, TR::RealRegister::NoReg, cg);
        deps->addPostCondition(spineSizeReg, TR::RealRegister::NoReg, cg);
    }

    TR::Node *callNode = outlinedHelperCall->getCallNode();
    TR::Register *reg;

    if (callNode->getFirstChild() == node->getFirstChild()) {
        reg = callNode->getFirstChild()->getRegister();
        if (reg)
            deps->unionPostCondition(reg, TR::RealRegister::NoReg, cg);
    }

    if (callNode->getSecondChild() == node->getSecondChild()) {
        reg = callNode->getSecondChild()->getRegister();
        if (reg)
            deps->unionPostCondition(reg, TR::RealRegister::NoReg, cg);
    }

    if (callNode->getThirdChild() == node->getThirdChild()) {
        reg = callNode->getThirdChild()->getRegister();
        if (reg)
            deps->unionPostCondition(reg, TR::RealRegister::NoReg, cg);
    }

    deps->stopAddingConditions();
    Inst_Label(OP::label, node, doneLabel, deps, cg);

    cg->stopUsingRegister(spineSizeReg);
    cg->stopUsingRegister(firstDimReg);
    cg->stopUsingRegister(leafSizeReg);
    cg->stopUsingRegister(secondDimReg);
    cg->stopUsingRegister(leafPtrReg);
    cg->stopUsingRegister(tempReg);

    // now that the array is properly allocated, move into a collected reference register
    TR::Register *returnReg = cg->allocateCollectedReferenceRegister();
    Inst_RegReg(OP::MOV8RegReg, node, returnReg, spinePtrReg, cg);

    cg->stopUsingRegister(spinePtrReg);

    cg->decReferenceCount(dimsPtrNode);
    cg->decReferenceCount(nDimsNode);
    cg->decReferenceCount(classNode);

    node->setRegister(returnReg);
    return returnReg;
}

/**
 * Generate code for multianewarray
 *
 * Includes inline allocation for arrays where the size of the first or second dimension is 0.
 *
 * NB Must only be used for arrays of at least two dimensions
 */
static TR::Register *generate2DZeroLengthArrayWithInlineAllocators(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();

    TR::Node *firstChild = node->getFirstChild(); // ptr to array of sizes, one for each dimension. Array construction
                                                  // stops at the outermost zero size
    TR::Node *secondChild
        = node->getSecondChild(); // Number of dimensions - this is fixed in the bytecode, so compile time constant
    TR::Node *thirdChild = node->getThirdChild(); // class of the outermost dimension

    // 2-dimensional MultiANewArray
    TR_J9VMBase *fej9 = comp->fej9();

    TR::Register *dimsPtrReg = NULL;
    TR::Register *dimReg = NULL;
    TR::Register *classReg = NULL;
    TR::Register *firstDimLenReg = NULL;
    TR::Register *secondDimLenReg = NULL;
    TR::Register *targetReg = NULL;
    TR::Register *temp1Reg = NULL;
    TR::Register *temp2Reg = NULL;
    TR::Register *temp3Reg = NULL;
    TR::Register *componentClassReg = NULL;
    TR::Register *vmThreadReg = cg->getVMThreadRegister();

    targetReg = cg->allocateRegister();
    firstDimLenReg = cg->allocateRegister();
    secondDimLenReg = cg->allocateRegister();
    temp1Reg = cg->allocateRegister();
    temp2Reg = cg->allocateRegister();
    temp3Reg = cg->allocateRegister();
    componentClassReg = cg->allocateRegister();

    TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *nonZeroFirstDimLabel = generateLabelSymbol(cg);
#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
    bool isOffHeapAllocationEnabled = TR::Compiler->om.isOffHeapAllocationEnabled();
#endif /* defined(J9VM_GC_SPARSE_HEAP_ALLOCATION) */

    startLabel->setStartInternalControlFlow();
    doneLabel->setEndInternalControlFlow();

    TR::LabelSymbol *oolFailLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *oolJumpPoint = generateLabelSymbol(cg);

    Inst_Label(OP::label, node, startLabel, cg);

    // Generate the heap allocation, and the snippet that will handle heap overflow.
    TR_OutlinedInstructions *outlinedHelperCall
        = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::acall, targetReg, oolFailLabel, doneLabel, cg);
    cg->generateDebugCounter(outlinedHelperCall->getFirstInstruction(),
        TR::DebugCounter::debugCounterName(comp, "multianewarray/dynamic/zero-dim/helper/(%s)/%d/%d", comp->signature(),
            node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
        1, TR::DebugCounter::Cheap);
    cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);

    dimReg = cg->evaluate(secondChild);
    dimsPtrReg = cg->evaluate(firstChild);
    classReg = cg->evaluate(thirdChild);

    // inlined code for allocating zero length arrays where the zero len is in either the first or second dimension

    Inst_RegMem(OP::L4RegMem, node, secondDimLenReg, MRef_Bdisp32(dimsPtrReg, 0, cg), cg);
    // Load the 32-bit length value as a 64-bit value so that the top half of the register
    // can be zeroed out. This will allow us to treat the value as 64-bit when performing
    // calculations later on.
    Inst_RegMem(OP::MOVSXReg8Mem4, node, firstDimLenReg, MRef_Bdisp32(dimsPtrReg, 4, cg), cg);

    Inst_RegImm(OP::CMP4RegImm4, node, secondDimLenReg, 0, cg);

    Inst_Label(OP::JNE4, node, oolJumpPoint, cg);
    // Second Dim length is 0

    Inst_RegImm(OP::CMP4RegImm4, node, firstDimLenReg, 0, cg);
    Inst_Label(OP::JNE4, node, nonZeroFirstDimLabel, cg);

    // First Dim zero, only allocate 1 zero-length object array
    Inst_RegMem(OP::LRegMem(), node, targetReg, MRef_Bdisp32(vmThreadReg, offsetof(J9VMThread, heapAlloc), cg), cg);

    // Take into account alignment requirements for the size of the zero-length array header
    int32_t zeroArraySizeAligned = OMR::align(TR::Compiler->om.discontiguousArrayHeaderSizeInBytes(),
        TR::Compiler->om.getObjectAlignmentInBytes());
    Inst_RegMem(OP::LEARegMem(), node, temp1Reg, MRef_Bdisp32(targetReg, zeroArraySizeAligned, cg), cg);

    Inst_RegMem(OP::CMPRegMem(), node, temp1Reg, MRef_Bdisp32(vmThreadReg, offsetof(J9VMThread, heapTop), cg), cg);
    Inst_Label(OP::JA4, node, oolJumpPoint, cg);
    Inst_MemReg(OP::SMemReg(), node, MRef_Bdisp32(vmThreadReg, offsetof(J9VMThread, heapAlloc), cg), temp1Reg, cg);

    // Init class
    bool use64BitClasses = comp->target().is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders();
    Inst_MemReg(OP::SMemReg(use64BitClasses), node,
        MRef_Bdisp32(targetReg, TR::Compiler->om.offsetOfObjectVftField(), cg), classReg, cg);

    // Init size and mustBeZero ('0') fields to 0
    Inst_MemImm(OP::S4MemImm4, node, MRef_Bdisp32(targetReg, fej9->getOffsetOfContiguousArraySizeField(), cg), 0, cg);
    Inst_MemImm(OP::S4MemImm4, node, MRef_Bdisp32(targetReg, fej9->getOffsetOfDiscontiguousArraySizeField(), cg), 0,
        cg);

#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
    if (isOffHeapAllocationEnabled) {
        // Init 1st dim dataAddr slot to 0
        Inst_MemImm(OP::S8MemImm4, node, MRef_Bdisp32(targetReg, fej9->getOffsetOfDiscontiguousDataAddrField(), cg), 0,
            cg);
    }
#endif /* J9VM_GC_SPARSE_HEAP_ALLOCATION */
    Inst_Label(OP::JMP4, node, doneLabel, cg);

    // First dim length not 0
    Inst_Label(OP::label, node, nonZeroFirstDimLabel, cg);

    Inst_RegMem(OP::LRegMem(), node, componentClassReg,
        MRef_Bdisp32(classReg, offsetof(J9ArrayClass, componentType), cg), cg);

    int32_t elementSize = TR::Compiler->om.sizeofReferenceField();

    uintptr_t maxObjectSize = cg->getMaxObjectSizeGuaranteedNotToOverflow();
    uintptr_t maxObjectSizeInElements = maxObjectSize / elementSize;
    Inst_RegImm(OP::CMPRegImm4(), node, firstDimLenReg, static_cast<int32_t>(maxObjectSizeInElements), cg);

    // Must be an unsigned comparison on sizes.
    Inst_Label(OP::JAE4, node, oolJumpPoint, cg);

    Inst_RegReg(OP::MOVRegReg(), node, temp1Reg, firstDimLenReg, cg);

    int32_t elementSizeAligned = OMR::align(elementSize, TR::Compiler->om.getObjectAlignmentInBytes());
    int32_t alignmentCompensation = (elementSize == elementSizeAligned) ? 0 : elementSizeAligned - 1;

    TR_ASSERT_FATAL(elementSize <= 8, "multianewArrayEvaluator - elementSize cannot be greater than 8!");
    Inst_RegImm(OP::SHLRegImm1(), node, temp1Reg, TR::MemoryReference::convertMultiplierToStride(elementSize), cg);
    Inst_RegImm(OP::ADDRegImm4(), node, temp1Reg,
        TR::Compiler->om.contiguousArrayHeaderSizeInBytes() + alignmentCompensation, cg);

    if (alignmentCompensation != 0) {
        Inst_RegImm(OP::ANDRegImm4(), node, temp1Reg, -elementSizeAligned, cg);
    }

    TR_ASSERT_FATAL(zeroArraySizeAligned >= 0 && zeroArraySizeAligned <= 127,
        "discontiguousArrayHeaderSizeInBytes cannot be > 127 for IMulRegRegImms instruction");
    Inst_RegRegImm(OP::IMULRegRegImm4(), node, temp2Reg, firstDimLenReg, zeroArraySizeAligned, cg);

    // temp2Reg = temp2Reg + temp1Reg
    Inst_RegReg(OP::ADDRegReg(), node, temp2Reg, temp1Reg, cg);

    Inst_RegMem(OP::LRegMem(), node, targetReg, MRef_Bdisp32(vmThreadReg, offsetof(J9VMThread, heapAlloc), cg), cg);
    // temp2Reg = temp2Reg + J9VMThread->heapAlloc
    Inst_RegReg(OP::ADDRegReg(), node, temp2Reg, targetReg, cg);

    Inst_RegMem(OP::CMPRegMem(), node, temp2Reg, MRef_Bdisp32(vmThreadReg, offsetof(J9VMThread, heapTop), cg), cg);
    Inst_Label(OP::JA4, node, oolJumpPoint, cg);
    Inst_MemReg(OP::SMemReg(), node, MRef_Bdisp32(vmThreadReg, offsetof(J9VMThread, heapAlloc), cg), temp2Reg, cg);

    // init 1st dim array class field
    Inst_MemReg(OP::SMemReg(use64BitClasses), node,
        MRef_Bdisp32(targetReg, TR::Compiler->om.offsetOfObjectVftField(), cg), classReg, cg);
    // Init 1st dim array size field
    Inst_MemReg(OP::S4MemReg, node, MRef_Bdisp32(targetReg, fej9->getOffsetOfContiguousArraySizeField(), cg),
        firstDimLenReg, cg);
#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
    if (isOffHeapAllocationEnabled) {
        /* Populate dataAddr slot of 1st dimension array. Arrays of non-zero size
         * use contiguous header layout while zero size arrays use discontiguous header layout.
         */
        Inst_RegMem(OP::LEARegMem(), node, temp3Reg,
            MRef_Bdisp32(targetReg, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg), cg);
        Inst_MemReg(OP::SMemReg(), node, MRef_Bdisp32(targetReg, fej9->getOffsetOfContiguousDataAddrField(), cg),
            temp3Reg, cg);
    }
#endif /* J9VM_GC_SPARSE_HEAP_ALLOCATION */

    // temp2 point to end of 1st dim array i.e. start of 2nd dim
    Inst_RegReg(OP::MOVRegReg(), node, temp2Reg, targetReg, cg);
    Inst_RegReg(OP::ADDRegReg(), node, temp2Reg, temp1Reg, cg);
    // temp1 points to 1st dim array past header
    Inst_RegMem(OP::LEARegMem(), node, temp1Reg,
        MRef_Bdisp32(targetReg, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg), cg);

    // loop start
    Inst_Label(OP::label, node, loopLabel, cg);
    // Init 2nd dim element's class
    Inst_MemReg(OP::SMemReg(use64BitClasses), node,
        MRef_Bdisp32(temp2Reg, TR::Compiler->om.offsetOfObjectVftField(), cg), componentClassReg, cg);
    // Init 2nd dim element's size and mustBeZero ('0') fields to 0
    Inst_MemImm(OP::S4MemImm4, node, MRef_Bdisp32(temp2Reg, fej9->getOffsetOfContiguousArraySizeField(), cg), 0, cg);
    Inst_MemImm(OP::S4MemImm4, node, MRef_Bdisp32(temp2Reg, fej9->getOffsetOfDiscontiguousArraySizeField(), cg), 0, cg);

#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
    if (isOffHeapAllocationEnabled) {
        // Populate dataAddr slot for 2nd dimension zero size array.
        Inst_MemImm(OP::S8MemImm4, node, MRef_Bdisp32(temp2Reg, fej9->getOffsetOfDiscontiguousDataAddrField(), cg), 0,
            cg);
    }
#endif /* J9VM_GC_SPARSE_HEAP_ALLOCATION */

    // Store 2nd dim element into 1st dim array slot, compress temp2 if needed
    if (comp->target().is64Bit() && comp->useCompressedPointers()) {
        int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();
        Inst_RegReg(OP::MOVRegReg(), node, temp3Reg, temp2Reg, cg);
        if (shiftAmount != 0) {
            Inst_RegImm(OP::SHRRegImm1(), node, temp3Reg, shiftAmount, cg);
        }
        Inst_MemReg(OP::S4MemReg, node, MRef_Bdisp32(temp1Reg, 0, cg), temp3Reg, cg);
    } else {
        Inst_MemReg(OP::SMemReg(), node, MRef_Bdisp32(temp1Reg, 0, cg), temp2Reg, cg);
    }

    // Advance cursors temp1 and temp2
    Inst_RegImm(OP::ADDRegImms(), node, temp2Reg, zeroArraySizeAligned, cg);
    Inst_RegImm(OP::ADDRegImms(), node, temp1Reg, elementSize, cg);

    Inst_Reg(OP::DEC4Reg, node, firstDimLenReg, cg);
    Inst_Label(OP::JA4, node, loopLabel, cg);

    Inst_Label(OP::JMP4, node, doneLabel, cg);

    TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, 13, cg);

    deps->addPostCondition(dimsPtrReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(dimReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(classReg, TR::RealRegister::NoReg, cg);

    deps->addPostCondition(firstDimLenReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(secondDimLenReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(temp1Reg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(temp2Reg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(temp3Reg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(componentClassReg, TR::RealRegister::NoReg, cg);

    deps->addPostCondition(targetReg, TR::RealRegister::eax, cg);
    deps->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);

    TR::Node *callNode = outlinedHelperCall->getCallNode();
    TR::Register *reg;

    if (callNode->getFirstChild() == node->getFirstChild()) {
        reg = callNode->getFirstChild()->getRegister();
        if (reg)
            deps->unionPostCondition(reg, TR::RealRegister::NoReg, cg);
    }

    if (callNode->getSecondChild() == node->getSecondChild()) {
        reg = callNode->getSecondChild()->getRegister();
        if (reg)
            deps->unionPostCondition(reg, TR::RealRegister::NoReg, cg);
    }

    if (callNode->getThirdChild() == node->getThirdChild()) {
        reg = callNode->getThirdChild()->getRegister();
        if (reg)
            deps->unionPostCondition(reg, TR::RealRegister::NoReg, cg);
    }

    deps->stopAddingConditions();

    Inst_Label(OP::label, node, oolJumpPoint, cg);
    Inst_Label(OP::JMP4, node, oolFailLabel, cg);

    Inst_Label(OP::label, node, doneLabel, deps, cg);

    // Copy the newly allocated object into a collected reference register now that it is a valid object.
    //
    TR::Register *targetReg2 = cg->allocateCollectedReferenceRegister();
    TR::RegisterDependencyConditions *deps2 = RegDeps(0, 1, cg);
    deps2->addPostCondition(targetReg2, TR::RealRegister::eax, cg);
    Inst_RegReg(OP::MOVRegReg(), node, targetReg2, targetReg, deps2, cg);
    cg->stopUsingRegister(targetReg);
    targetReg = targetReg2;

    cg->stopUsingRegister(firstDimLenReg);
    cg->stopUsingRegister(secondDimLenReg);
    cg->stopUsingRegister(temp1Reg);
    cg->stopUsingRegister(temp2Reg);
    cg->stopUsingRegister(temp3Reg);
    cg->stopUsingRegister(componentClassReg);

    // Decrement use counts on the children
    //
    cg->decReferenceCount(node->getFirstChild());
    cg->decReferenceCount(node->getSecondChild());
    cg->decReferenceCount(node->getThirdChild());

    node->setRegister(targetReg);
    return targetReg;
}

/**
 * Generate code for multianewarray
 *
 * Checks the number of dimensions. For 1 dimensional arrays call the helper, for >1 call
 * generate2DArrayWithInlineAllocators or generate2DZeroLengthArrayWithInlineAllocators.
 */
TR::Register *J9::X86::TreeEvaluator::multianewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR_ASSERT_FATAL(comp->target().is64Bit(), "multianewArrayEvaluator is only supported on 64-bit JVMs!");

    TR::Node *secondChild
        = node->getSecondChild(); // Number of dimensions - this is fixed in the bytecode, so compile time constant

    // The number of dimensions should always be an iconst
    TR_ASSERT_FATAL(secondChild->getOpCodeValue() == TR::iconst, "dims of multianewarray must be iconst");

    // Anything with more than 2 dimensions will be replaced by a direct call when lowering trees.
    uint32_t nDims = secondChild->get32bitIntegralValue();
    TR_ASSERT_FATAL(nDims <= 2,
        "multianewarray with dimension >2 should have been lowered to direct call before instruction selection");

    TR::Node *classNode = node->getThirdChild(); // class of the outermost dimension

    if (nDims == 2) {
        static const bool disable = feGetEnv("TR_Disable2DArrayWithInlineAllocators") != NULL;
        int32_t leafArrayElementSize = TR::Compiler->om.getTwoDimensionalArrayComponentSize(classNode);
        if (!disable && (leafArrayElementSize != -1) && !comp->getOptions()->realTimeGC()) {
            return generate2DArrayWithInlineAllocators(node, cg, leafArrayElementSize);
        } else {
            return generate2DZeroLengthArrayWithInlineAllocators(node, cg);
        }
    } else {
        logprintf(comp->getOption(TR_TraceCG), comp->log(),
            "Disabling inline allocations for multianewarray of dim %d\n", nDims);

        TR::ILOpCodes opCode = node->getOpCodeValue();
        TR::Node::recreate(node, TR::acall);
        TR::Register *targetRegister = directCallEvaluator(node, cg);
        TR::Node::recreate(node, opCode);
        return targetRegister;
    }
}

TR::Register *J9::X86::TreeEvaluator::arraycopyEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();

    if (!node->isReferenceArrayCopy()) {
        return OMR::TreeEvaluatorConnector::arraycopyEvaluator(node, cg);
    }

    auto srcObjReg = cg->evaluate(node->getChild(0));
    auto dstObjReg = cg->evaluate(node->getChild(1));
    auto srcReg = cg->evaluate(node->getChild(2));
    auto dstReg = cg->evaluate(node->getChild(3));
    auto sizeReg = cg->evaluate(node->getChild(4));

#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
    if (TR::Compiler->om.isOffHeapAllocationEnabled())
        // For correct card-marking calculation, the dstObjNode should be the baseObj not the dataAddrPointer
        TR_ASSERT_FATAL(!node->getChild(1)->isDataAddrPointer(),
            "The byteDstObjNode child of arraycopy cannot be a dataAddrPointer");
#endif /* defined(J9VM_GC_SPARSE_HEAP_ALLOCATION) */

    if (comp->target().is64Bit() && !TR::TreeEvaluator::getNodeIs64Bit(node->getChild(4), cg)) {
        Inst_RegReg(OP::MOVZXReg8Reg4, node, sizeReg, sizeReg, cg);
    }

    if (!node->isNoArrayStoreCheckArrayCopy()) {
        // Nothing to optimize, simply call jitReferenceArrayCopy helper
        auto deps = RegDeps((uint8_t)3, 3, cg);
        deps->addPreCondition(srcReg, TR::RealRegister::esi, cg);
        deps->addPreCondition(dstReg, TR::RealRegister::edi, cg);
        deps->addPreCondition(sizeReg, TR::RealRegister::ecx, cg);
        deps->addPostCondition(srcReg, TR::RealRegister::esi, cg);
        deps->addPostCondition(dstReg, TR::RealRegister::edi, cg);
        deps->addPostCondition(sizeReg, TR::RealRegister::ecx, cg);

        Inst_MemReg(OP::SMemReg(), node, MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, floatTemp1), cg),
            srcObjReg, cg);
        Inst_MemReg(OP::SMemReg(), node, MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, floatTemp2), cg),
            dstObjReg, cg);
        Inst_HelperCall(node, TR_referenceArrayCopy, deps, cg)->setNeedsGCMap(0xFF00FFFF);

        auto snippetLabel = generateLabelSymbol(cg);
        auto instr = Inst_Label(OP::JNE4, node, snippetLabel,
            cg); // ReferenceArrayCopy set ZF when succeed.
        auto snippet = new (cg->trHeapMemory()) TR::X86CheckFailureSnippet(cg,
            cg->symRefTab()->findOrCreateRuntimeHelper(TR_arrayStoreException), snippetLabel, instr, false);
        cg->addSnippet(snippet);
    } else {
        bool use64BitClasses = comp->target().is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders();

        auto RSI = cg->allocateRegister();
        auto RDI = cg->allocateRegister();
        auto RCX = cg->allocateRegister();

        TR::Register *tmpReg1 = NULL;
        TR::Register *tmpReg2 = NULL;
        TR::Register *tmpXmmYmmReg1 = NULL;
        TR::Register *tmpXmmYmmReg2 = NULL;

        static bool disableReferenceArrayCopyInlineSmallSizeWithoutREPMOVS
            = feGetEnv("TR_DisableReferenceArrayCopyInlineSmallSizeWithoutREPMOVS") != NULL;

        bool enableInlineForSmallSize = !disableReferenceArrayCopyInlineSmallSizeWithoutREPMOVS
            && !comp->getOption(TR_DisableReferenceArrayCopyInlineSmallSizeWithoutREPMOVS)
            && comp->target().cpu.supportsAVX() && comp->target().is64Bit();

        int32_t repMovsThresholdBytes = 32;
        int32_t newThreshold = comp->getOptions()->getArraycopyRepMovsReferenceArrayThreshold();

        if ((repMovsThresholdBytes < newThreshold) && ((newThreshold == 64) || (newThreshold == 128))) {
            // If the CPU doesn't support AVX512, reduce the threshold to 64 bytes
            repMovsThresholdBytes = ((newThreshold == 128) && cg->getMaxPreferredVectorLength() != TR::VectorLength512)
                ? 64
                : newThreshold;
        }

        if (enableInlineForSmallSize) {
            tmpReg1 = cg->allocateRegister(TR_GPR);
            tmpReg2 = cg->allocateRegister(TR_GPR);
            tmpXmmYmmReg1 = cg->allocateRegister(TR_VRF);
            tmpXmmYmmReg2 = cg->allocateRegister(TR_VRF);
        }

        Inst_RegReg(OP::MOVRegReg(), node, RSI, srcReg, cg);
        Inst_RegReg(OP::MOVRegReg(), node, RDI, dstReg, cg);
        Inst_RegReg(OP::MOVRegReg(), node, RCX, sizeReg, cg);

        int8_t numDeps = enableInlineForSmallSize ? 9 : 5;
        TR::RegisterDependencyConditions *deps = RegDeps(numDeps, numDeps, cg);

        deps->addPreCondition(RSI, TR::RealRegister::esi, cg);
        deps->addPreCondition(RDI, TR::RealRegister::edi, cg);
        deps->addPreCondition(RCX, TR::RealRegister::ecx, cg);
        deps->addPreCondition(srcObjReg, TR::RealRegister::NoReg, cg);
        deps->addPreCondition(dstObjReg, TR::RealRegister::NoReg, cg);
        deps->addPostCondition(RSI, TR::RealRegister::esi, cg);
        deps->addPostCondition(RDI, TR::RealRegister::edi, cg);
        deps->addPostCondition(RCX, TR::RealRegister::ecx, cg);
        deps->addPostCondition(srcObjReg, TR::RealRegister::NoReg, cg);
        deps->addPostCondition(dstObjReg, TR::RealRegister::NoReg, cg);

        if (enableInlineForSmallSize) {
            deps->addPreCondition(tmpReg1, TR::RealRegister::NoReg, cg);
            deps->addPreCondition(tmpReg2, TR::RealRegister::NoReg, cg);
            deps->addPreCondition(tmpXmmYmmReg1, TR::RealRegister::NoReg, cg);
            deps->addPreCondition(tmpXmmYmmReg2, TR::RealRegister::NoReg, cg);

            deps->addPostCondition(tmpReg1, TR::RealRegister::NoReg, cg);
            deps->addPostCondition(tmpReg2, TR::RealRegister::NoReg, cg);
            deps->addPostCondition(tmpXmmYmmReg1, TR::RealRegister::NoReg, cg);
            deps->addPostCondition(tmpXmmYmmReg2, TR::RealRegister::NoReg, cg);
        }

        auto begLabel = generateLabelSymbol(cg);
        auto endLabel = generateLabelSymbol(cg);
        begLabel->setStartInternalControlFlow();
        endLabel->setEndInternalControlFlow();

        Inst_Label(OP::label, node, begLabel, cg);

        if (TR::Compiler->om.readBarrierType() != gc_modron_readbar_none) {
            bool use64BitClasses = comp->target().is64Bit() && !comp->useCompressedPointers();

            TR::LabelSymbol *rdbarLabel = generateLabelSymbol(cg);
            // EvacuateTopAddress == 0 means Concurrent Scavenge is inactive
            Inst_MemImm(OP::CMPMemImms(use64BitClasses), node,
                MRef_Bdisp32(cg->getVMThreadRegister(), comp->fej9()->thisThreadGetEvacuateTopAddressOffset(), cg), 0,
                cg);
            Inst_Label(OP::JNE4, node, rdbarLabel, cg);

            TR_OutlinedInstructionsGenerator og(rdbarLabel, node, cg);
            Inst_MemReg(OP::SMemReg(), node,
                MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, floatTemp1), cg), srcObjReg, cg);
            Inst_MemReg(OP::SMemReg(), node,
                MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, floatTemp2), cg), dstObjReg, cg);
            Inst_HelperCall(node, TR_referenceArrayCopy, NULL, cg)->setNeedsGCMap(0xFF00FFFF);
            Inst_Label(OP::JMP4, node, endLabel, cg);
            og.endOutlinedInstructionSequence();
        }

        if (enableInlineForSmallSize) {
            TR::LabelSymbol *repMovsLabel = generateLabelSymbol(cg);

            if (use64BitClasses) {
                OMR::TreeEvaluatorConnector::arrayCopy64BitPrimitiveInlineSmallSizeWithoutREPMOVSImplRoot16(node,
                    RDI /* dstReg */, RSI /* srcReg */, RCX /* sizeReg */, tmpReg1, tmpReg2, tmpXmmYmmReg1,
                    tmpXmmYmmReg2, cg, repMovsThresholdBytes, repMovsLabel, endLabel);
            } else {
                OMR::TreeEvaluatorConnector::arrayCopy32BitPrimitiveInlineSmallSizeWithoutREPMOVSImplRoot16(node,
                    RDI /* dstReg */, RSI /* srcReg */, RCX /* sizeReg */, tmpReg1, tmpReg2, tmpXmmYmmReg1,
                    tmpXmmYmmReg2, cg, repMovsThresholdBytes, repMovsLabel, endLabel);
            }

            Inst_Label(OP::label, node, repMovsLabel, cg);
        }

        if (!node->isForwardArrayCopy()) {
            TR::LabelSymbol *backwardLabel = generateLabelSymbol(cg);

            Inst_RegReg(OP::SUBRegReg(), node, RDI, RSI, cg); // dst = dst - src
            Inst_RegReg(OP::CMPRegReg(), node, RDI, RCX, cg); // cmp dst, size
            Inst_RegMem(OP::LEARegMem(), node, RDI, MRef_BIS(RDI, RSI, 0, cg),
                cg); // dst = dst + src
            Inst_Label(OP::JB4, node, backwardLabel, cg); // jb, skip backward copy setup

            TR_OutlinedInstructionsGenerator og(backwardLabel, node, cg);
            Inst_RegMem(OP::LEARegMem(), node, RSI,
                MRef_BISdisp32(RSI, RCX, 0, -TR::Compiler->om.sizeofReferenceField(), cg), cg);
            Inst_RegMem(OP::LEARegMem(), node, RDI,
                MRef_BISdisp32(RDI, RCX, 0, -TR::Compiler->om.sizeofReferenceField(), cg), cg);
            Inst_RegImm(OP::SHRRegImm1(), node, RCX, use64BitClasses ? 3 : 2, cg);
            Inst(OP::STD, node, cg);
            Inst(use64BitClasses ? OP::REPMOVSQ : OP::REPMOVSD, node, cg);
            Inst(OP::CLD, node, cg);
            Inst_Label(OP::JMP4, node, endLabel, cg);
            og.endOutlinedInstructionSequence();
        }

        Inst_RegImm(OP::SHRRegImm1(), node, RCX, use64BitClasses ? 3 : 2, cg);
        Inst(use64BitClasses ? OP::REPMOVSQ : OP::REPMOVSD, node, cg);

        Inst_Label(OP::label, node, endLabel, deps, cg);

        cg->stopUsingRegister(RSI);
        cg->stopUsingRegister(RDI);
        cg->stopUsingRegister(RCX);

        if (enableInlineForSmallSize) {
            cg->stopUsingRegister(tmpReg1);
            cg->stopUsingRegister(tmpReg2);
            cg->stopUsingRegister(tmpXmmYmmReg1);
            cg->stopUsingRegister(tmpXmmYmmReg2);
        }

        TR::TreeEvaluator::VMwrtbarWithoutStoreEvaluator(node, node->getChild(1), NULL, NULL,
            cg->generateScratchRegisterManager(), cg);
    }

    for (int32_t i = 0; i < node->getNumChildren(); i++) {
        cg->decReferenceCount(node->getChild(i));
    }
    return NULL;
}

TR::Register *J9::X86::TreeEvaluator::arraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
    // MOV R, [B + contiguousSize]
    // TEST R, R
    // CMOVE R, [B + discontiguousSize]
    //
    TR::Register *objectReg = cg->evaluate(node->getFirstChild());
    TR::Register *lengthReg = cg->allocateRegister();

    TR::MemoryReference *contiguousArraySizeMR
        = MRef_Bdisp32(objectReg, fej9->getOffsetOfContiguousArraySizeField(), cg);

    TR::MemoryReference *discontiguousArraySizeMR
        = MRef_Bdisp32(objectReg, fej9->getOffsetOfDiscontiguousArraySizeField(), cg);

    Inst_RegMem(OP::L4RegMem, node, lengthReg, contiguousArraySizeMR, cg);
    Inst_RegReg(OP::TEST4RegReg, node, lengthReg, lengthReg, cg);
    Inst_RegMem(OP::CMOVE4RegMem, node, lengthReg, discontiguousArraySizeMR, cg);

    cg->decReferenceCount(node->getFirstChild());
    node->setRegister(lengthReg);
    return lengthReg;
}

TR::Register *J9::X86::TreeEvaluator::exceptionRangeFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    Inst_Fence(OP::fence, node, node, cg);
    return NULL;
}

TR::Register *J9::X86::TreeEvaluator::evaluateNULLCHKWithPossibleResolve(TR::Node *node, bool needResolution,
    TR::CodeGenerator *cg)
{
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
    static bool disableBranchlessPassThroughNULLCHK = feGetEnv("TR_disableBranchlessPassThroughNULLCHK") != NULL;
    // NOTE:
    //
    // If no code is generated for the null check, just evaluate the
    // child and decrement its use count UNLESS the child is a pass-through node
    // in which case some kind of explicit test or indirect load must be generated
    // to force the null check at this point.
    //
    TR::Node *firstChild = node->getFirstChild();
    TR::Node *reference = NULL;
    TR::Compilation *comp = cg->comp();

    bool usingCompressedPointers = false;

    if (comp->useCompressedPointers() && firstChild->getOpCodeValue() == TR::l2a) {
        // pattern match the sequence under the l2a
        // NULLCHK        NULLCHK                     <- node
        //    aloadi f      l2a
        //       aload O       ladd
        //                       lshl
        //                          i2l
        //                            iloadi/irdbari f <- firstChild
        //                               aload O        <- reference
        //                          iconst shftKonst
        //                       lconst HB
        //
        usingCompressedPointers = true;

        TR::ILOpCodes loadOp = comp->il.opCodeForIndirectLoad(TR::Int32);
        TR::ILOpCodes rdbarOp = comp->il.opCodeForIndirectReadBarrier(TR::Int32);
        while (firstChild->getOpCodeValue() != loadOp && firstChild->getOpCodeValue() != rdbarOp)
            firstChild = firstChild->getFirstChild();
        reference = firstChild->getFirstChild();
    } else
        reference = node->getNullCheckReference();

    TR::ILOpCode &opCode = firstChild->getOpCode();

    // Skip the NULLCHK for TR::loadaddr nodes.
    //
    if (reference->getOpCodeValue() == TR::loadaddr) {
        if (usingCompressedPointers)
            firstChild = node->getFirstChild();
        cg->evaluate(firstChild);
        cg->decReferenceCount(firstChild);
        return NULL;
    }

    bool needExplicitCheck = true;
    bool needLateEvaluation = true;

    // Add the explicit check after this instruction
    //
    TR::Instruction *appendTo = 0;

    if (opCode.isLoadVar() || (comp->target().is64Bit() && opCode.getOpCodeValue() == TR::l2i)) {
        TR::SymbolReference *symRef = NULL;

        if (opCode.getOpCodeValue() == TR::l2i) {
            symRef = firstChild->getFirstChild()->getSymbolReference();
        } else
            symRef = firstChild->getSymbolReference();

        if (symRef && (symRef->getSymbol()->getOffset() + symRef->getOffset() < cg->getNumberBytesReadInaccessible())) {
            needExplicitCheck = false;

            // If the child is an arraylength which has been reduced to an iloadi,
            // and is only going to be used immediately in a bound check then combine the checks.
            //
            TR::TreeTop *nextTreeTop = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();
            if (firstChild->getReferenceCount() == 2 && nextTreeTop) {
                TR::Node *nextTopNode = nextTreeTop->getNode();

                if (nextTopNode) {
                    if (nextTopNode->getOpCode().isBndCheck() || nextTopNode->getOpCode().isSpineCheck()) {
                        bool doIt = false;

                        if (nextTopNode->getOpCodeValue() == TR::SpineCHK) {
                            // Implicit NULLCHKs and SpineCHKs can be merged if the base array
                            // is the same.
                            //
                            if (firstChild->getOpCode().isIndirect() && firstChild->getOpCode().isLoadVar()) {
                                if (nextTopNode->getChild(1) == firstChild->getFirstChild())
                                    doIt = true;
                            }
                        } else {
                            int32_t arrayLengthChildNum
                                = (nextTopNode->getOpCodeValue() == TR::BNDCHKwithSpineCHK) ? 2 : 0;

                            if (nextTopNode->getChild(arrayLengthChildNum) == firstChild)
                                doIt = true;
                        }

                        if (doIt
                            && performTransformation(comp,
                                "\nMerging NULLCHK [" POINTER_PRINTF_FORMAT
                                "] and BNDCHK/SpineCHK [" POINTER_PRINTF_FORMAT
                                "] of load child [" POINTER_PRINTF_FORMAT "]\n",
                                node, nextTopNode, firstChild)) {
                            needLateEvaluation = false;
                            nextTopNode->setHasFoldedImplicitNULLCHK(true);
                        }
                    } else if (nextTopNode->getOpCode().isIf() && nextTopNode->isNonoverriddenGuard()
                        && nextTopNode->getFirstChild() == firstChild) {
                        needLateEvaluation = false;
                        needExplicitCheck = true;
                        reference->incReferenceCount(); // will be decremented again later
                    }
                }
            }
        } else if (firstChild->getReferenceCount() == 1 && !firstChild->getSymbolReference()->isUnresolved()) {
            // If the child is only used here, we don't need to evaluate it
            // since all we need is the grandchild which will be evaluated by
            // the generation of the explicit check below.
            //
            needLateEvaluation = false;

            // at this point, firstChild is the raw iloadi (created by lowerTrees) and
            // reference is the aload of the object. node->getFirstChild is the
            // l2a sequence; as a result, firstChild's refCount will always be 1
            // and node->getFirstChild's refCount will be at least 2 (one under the nullchk
            // and the other under the translate treetop)
            //
            if (usingCompressedPointers && node->getFirstChild()->getReferenceCount() >= 2)
                needLateEvaluation = true;
        }
    } else if (opCode.isStore()) {
        TR::SymbolReference *symRef = firstChild->getSymbolReference();
        if (symRef && symRef->getSymbol()->getOffset() + symRef->getOffset() < cg->getNumberBytesWriteInaccessible()) {
            needExplicitCheck = false;
        }
    } else if (opCode.isCall() && opCode.isIndirect()
        && cg->getNumberBytesReadInaccessible() > TR::Compiler->om.offsetOfObjectVftField()) {
        needExplicitCheck = false;
    } else if (opCode.getOpCodeValue() == TR::monent || opCode.getOpCodeValue() == TR::monexit) {
        // The child may generate inline code that provides an implicit null check
        // but we won't know until the child is evaluated.
        //
        reference->incReferenceCount(); // will be decremented again later
        needLateEvaluation = false;
        cg->evaluate(reference);
        appendTo = cg->getAppendInstruction();
        cg->evaluate(firstChild);

        // TODO: this shouldn't be getOffsetOfContiguousArraySizeField
        //
        if (cg->getImplicitExceptionPoint()
            && cg->getNumberBytesReadInaccessible() > fej9->getOffsetOfContiguousArraySizeField()) {
            needExplicitCheck = false;
            cg->decReferenceCount(reference);
        }
    } else if (!disableBranchlessPassThroughNULLCHK && opCode.getOpCodeValue() == TR::PassThrough && !needResolution
        && cg->getHasResumableTrapHandler()) {
        TR::Register *refRegister = cg->evaluate(firstChild);
        needLateEvaluation = false;

        if (refRegister) {
            if (!appendTo)
                appendTo = cg->getAppendInstruction();
            if (cg->getNumberBytesReadInaccessible() > 0) {
                needExplicitCheck = false;
                TR::MemoryReference *memRef = NULL;
                if (TR::Compiler->om.compressedReferenceShift() > 0 && firstChild->getType() == TR::Address
                    && firstChild->getOpCode().hasSymbolReference()
                    && firstChild->getSymbol()->isCollectedReference()) {
                    memRef = MRef_BISdisp32(NULL, refRegister, TR::Compiler->om.compressedReferenceShift(), 0, cg);
                } else {
                    memRef = MRef_Bdisp32(refRegister, 0, cg);
                }
                appendTo = Inst_MemImm(appendTo, OP::TEST1MemImm1, memRef, 0, cg);
                cg->setImplicitExceptionPoint(appendTo);
            }
        }
    }

    // Generate the code for the null check.
    //
    if (needExplicitCheck) {
        // TODO - If a resolve check is needed as well, the resolve must be done
        // before the null check, so that exceptions are handled in the correct
        // order.
        //
        ///// if (needResolution)
        /////    {
        /////    ...
        /////    }

        // Avoid loading the grandchild into a register if it is not going to be used again.
        //
        if (opCode.getOpCodeValue() == TR::PassThrough && reference->getOpCode().isLoadVar()
            && reference->getRegister() == NULL && reference->getReferenceCount() == 1) {
            TR::MemoryReference *tempMR = MRef_node(reference, cg);

            if (!appendTo)
                appendTo = cg->getAppendInstruction();

            OP::Mnemonic op = OP::CMPMemImms();
            appendTo = Inst_MemImm(appendTo, op, tempMR, NULLVALUE, cg);
            tempMR->decNodeReferenceCounts(cg);
            needLateEvaluation = false;
        } else {
            TR::Register *targetRegister = cg->evaluate(reference);

            if (!appendTo)
                appendTo = cg->getAppendInstruction();

            appendTo = Inst_RegReg(appendTo, OP::TESTRegReg(), targetRegister, targetRegister, cg);
        }

        TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
        appendTo = Inst_Label(appendTo, OP::JE4, snippetLabel, cg);
        // the _node field should point to the current node
        appendTo->setNode(node);
        appendTo->setLiveLocals(cg->getLiveLocals());

        TR::Snippet *snippet;
        if (opCode.isCall() || !needResolution
            || comp->target().is64Bit()) // TODO:AMD64: Implement the "withresolve" version
        {
            snippet = new (cg->trHeapMemory())
                TR::X86CheckFailureSnippet(cg, node->getSymbolReference(), snippetLabel, appendTo);
        } else {
            TR_RuntimeHelper resolverCall;
            TR::Machine *machine = cg->machine();
            TR::Symbol *firstChildSym = firstChild->getSymbolReference()->getSymbol();

            if (firstChildSym->isShadow()) {
                resolverCall = opCode.isStore() ? TR_X86interpreterUnresolvedFieldSetterGlue
                                                : TR_X86interpreterUnresolvedFieldGlue;
            } else if (firstChildSym->isClassObject()) {
                resolverCall = firstChildSym->addressIsCPIndexOfStatic()
                    ? TR_X86interpreterUnresolvedClassFromStaticFieldGlue
                    : TR_X86interpreterUnresolvedClassGlue;
            } else if (firstChildSym->isConstString()) {
                resolverCall = TR_X86interpreterUnresolvedStringGlue;
            } else if (firstChildSym->isConstMethodType()) {
                resolverCall = TR_interpreterUnresolvedMethodTypeGlue;
            } else if (firstChildSym->isConstMethodHandle()) {
                resolverCall = TR_interpreterUnresolvedMethodHandleGlue;
            } else if (firstChildSym->isCallSiteTableEntry()) {
                resolverCall = TR_interpreterUnresolvedCallSiteTableEntryGlue;
            } else if (firstChildSym->isMethodTypeTableEntry()) {
                resolverCall = TR_interpreterUnresolvedMethodTypeTableEntryGlue;
            } else {
                resolverCall = opCode.isStore() ? TR_X86interpreterUnresolvedStaticFieldSetterGlue
                                                : TR_X86interpreterUnresolvedStaticFieldGlue;
            }

            snippet = new (cg->trHeapMemory()) TR::X86CheckFailureSnippetWithResolve(cg, node->getSymbolReference(),
                firstChild->getSymbolReference(), resolverCall, snippetLabel, appendTo);

            ((TR::X86CheckFailureSnippetWithResolve *)(snippet))
                ->setNumLiveX87Registers(machine->fpGetNumberOfLiveFPRs());
            ((TR::X86CheckFailureSnippetWithResolve *)(snippet))->setHasLiveXMMRs();
        }

        cg->addSnippet(snippet);
    }

    // If we need to evaluate the child, do so. Otherwise, if we have
    // evaluated the reference node, then decrement its use count.
    // The use count of the child is decremented when we are done
    // evaluating the NULLCHK.
    //
    if (needLateEvaluation) {
        cg->evaluate(node->getFirstChild());
    } else if (needExplicitCheck) {
        cg->decReferenceCount(reference);
    }

    if (comp->useCompressedPointers())
        cg->decReferenceCount(node->getFirstChild());
    else
        cg->decReferenceCount(firstChild);

    // If an explicit check has not been generated for the null check, there is
    // an instruction that will cause a hardware trap if the exception is to be
    // taken. If this method may catch the exception, a GC stack map must be
    // created for this instruction. All registers are valid at this GC point
    // TODO - if the method may not catch the exception we still need to note
    // that the GC point exists, since maps before this point and after it cannot
    // be merged.
    //
    if (!needExplicitCheck) {
        TR::Instruction *faultingInstruction = cg->getImplicitExceptionPoint();
        if (faultingInstruction) {
            faultingInstruction->setNeedsGCMap(0xFF00FFFF);
            faultingInstruction->setNode(node);
        }
    }

    TR::Node *n = NULL;
    if (comp->useCompressedPointers() && reference->getOpCodeValue() == TR::l2a) {
        reference->setIsNonNull(true);
        n = reference->getFirstChild();
        TR::ILOpCodes loadOp = comp->il.opCodeForIndirectLoad(TR::Int32);
        TR::ILOpCodes rdbarOp = comp->il.opCodeForIndirectReadBarrier(TR::Int32);
        while (n->getOpCodeValue() != loadOp && n->getOpCodeValue() != rdbarOp) {
            n->setIsNonZero(true);
            n = n->getFirstChild();
        }
        n->setIsNonZero(true);
    }

    reference->setIsNonNull(true);

    return NULL;
}

TR::Register *J9::X86::TreeEvaluator::NULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    return TR::TreeEvaluator::evaluateNULLCHKWithPossibleResolve(node, false, cg);
}

TR::Register *J9::X86::TreeEvaluator::resolveAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    return TR::TreeEvaluator::evaluateNULLCHKWithPossibleResolve(node, true, cg);
}

// Generate explicit checks for division by zero and division
// overflow (i.e. 0x80000000 / 0xFFFFFFFF), if necessary.
//
TR::Register *J9::X86::TreeEvaluator::DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    bool hasConversion;
    TR::Node *divisionNode = node->getFirstChild();
    TR::Compilation *comp = cg->comp();

    TR::ILOpCodes op = divisionNode->getOpCodeValue();

    if (op == TR::iu2l || op == TR::bu2i || op == TR::bu2l || op == TR::bu2s || op == TR::su2i || op == TR::su2l) {
        divisionNode = divisionNode->getFirstChild();
        hasConversion = true;
    } else
        hasConversion = false;

    bool use64BitRegisters = comp->target().is64Bit() && divisionNode->getOpCode().isLong();
    bool useRegisterPairs = comp->target().is32Bit() && divisionNode->getOpCode().isLong();

    // Not all targets support implicit division checks, so we generate explicit
    // tests and snippets to jump to.
    //
    bool platformNeedsExplicitCheck = !cg->enableImplicitDivideCheck();

    // Only do this for TR::ldiv/TR::lrem and TR::idiv/TR::irem by non-constant
    // divisors, or by a constant of zero.
    // Other constant divisors are optimized in signedIntegerDivOrRemAnalyser,
    // and do not cause hardware exceptions.
    //
    bool operationNeedsCheck = (divisionNode->getOpCode().isInt()
        && (!divisionNode->getSecondChild()->getOpCode().isLoadConst()
            || divisionNode->getSecondChild()->getInt() == 0));
    if (use64BitRegisters) {
        operationNeedsCheck = operationNeedsCheck
            | ((!divisionNode->getSecondChild()->getOpCode().isLoadConst()
                || divisionNode->getSecondChild()->getLongInt() == 0));
    } else {
        operationNeedsCheck = operationNeedsCheck | useRegisterPairs;
    }

    if (platformNeedsExplicitCheck && operationNeedsCheck) {
        TR::Register *dividendReg = cg->evaluate(divisionNode->getFirstChild());
        TR::Register *divisorReg = cg->evaluate(divisionNode->getSecondChild());

        TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
        TR::LabelSymbol *divisionLabel = generateLabelSymbol(cg);
        TR::LabelSymbol *divideByZeroSnippetLabel = generateLabelSymbol(cg);
        TR::LabelSymbol *restartLabel = generateLabelSymbol(cg);

        // These instructions are dissected in the divide check snippet to determine
        // the source registers. If they or their format are changed, you may need to
        // change the snippet(s) also.
        //
        TR::X86RegRegInstruction *lowDivisorTestInstr;
        TR::X86RegRegInstruction *highDivisorTestInstr;

        startLabel->setStartInternalControlFlow();
        restartLabel->setEndInternalControlFlow();

        Inst_Label(OP::label, node, startLabel, cg);

        if (useRegisterPairs) {
            TR::Register *tempReg = cg->allocateRegister(TR_GPR);
            lowDivisorTestInstr = Inst_RegReg(OP::MOV4RegReg, node, tempReg, divisorReg->getLowOrder(), cg);
            highDivisorTestInstr = Inst_RegReg(OP::OR4RegReg, node, tempReg, divisorReg->getHighOrder(), cg);
            Inst_RegReg(OP::TEST4RegReg, node, tempReg, tempReg, cg);
            cg->stopUsingRegister(tempReg);
        } else
            lowDivisorTestInstr = Inst_RegReg(OP::TESTRegReg(use64BitRegisters), node, divisorReg, divisorReg, cg);

        Inst_Label(OP::JE4, node, divideByZeroSnippetLabel, cg);

        cg->addSnippet(new (cg->trHeapMemory()) TR::X86CheckFailureSnippet(cg, node->getSymbolReference(),
            divideByZeroSnippetLabel, cg->getAppendInstruction()));

        Inst_Label(OP::label, node, divisionLabel, cg);

        TR::Register *resultRegister = cg->evaluate(divisionNode);

        if (!hasConversion)
            cg->decReferenceCount(divisionNode);

        // We need to make sure that any spilling occurs only after restartLabel,
        // otherwise the divide check snippet may store into the wrong register.
        //
        TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, 2, cg);
        TR::Register *scratchRegister;

        if (useRegisterPairs) {
            deps->addPostCondition(resultRegister->getLowOrder(), TR::RealRegister::eax, cg);
            deps->addPostCondition(resultRegister->getHighOrder(), TR::RealRegister::edx, cg);
        } else
            switch (divisionNode->getOpCodeValue()) {
                case TR::idiv:
                case TR::ldiv:
                    deps->addPostCondition(resultRegister, TR::RealRegister::eax, cg);
                    scratchRegister = cg->allocateRegister(TR_GPR);
                    deps->addPostCondition(scratchRegister, TR::RealRegister::edx, cg);
                    cg->stopUsingRegister(scratchRegister);
                    break;

                case TR::irem:
                case TR::lrem:
                    deps->addPostCondition(resultRegister, TR::RealRegister::edx, cg);
                    scratchRegister = cg->allocateRegister(TR_GPR);
                    deps->addPostCondition(scratchRegister, TR::RealRegister::eax, cg);
                    cg->stopUsingRegister(scratchRegister);
                    break;

                default:
                    TR_ASSERT(0, "bad division opcode for DIVCHK\n");
            }

        Inst_Label(OP::label, node, restartLabel, deps, cg);

        if (hasConversion) {
            cg->evaluate(node->getFirstChild());
            cg->decReferenceCount(node->getFirstChild());
        }
    } else {
        cg->evaluate(node->getFirstChild());
        cg->decReferenceCount(node->getFirstChild());

        // There may be an instruction that will cause a hardware trap if an exception
        // is to be taken.
        // If this method may catch the exception, a GC stack map must be created for
        // this instruction. All registers are valid at this GC point
        //
        // TODO: if the method may not catch the exception we still need to note
        // that the GC point exists, since maps before this point and after it cannot
        // be merged.
        //
        TR::Instruction *faultingInstruction = cg->getImplicitExceptionPoint();
        if (faultingInstruction)
            faultingInstruction->setNeedsGCMap(0xFF00FFFF);
    }

    return NULL;
}

static bool isInteger(TR::ILOpCode &op, TR::CodeGenerator *cg)
{
    if (cg->comp()->target().is64Bit())
        return op.isIntegerOrAddress();
    else
        return op.isIntegerOrAddress() && (op.getSize() <= 4);
}

static OP::Mnemonic branchOpCodeForCompare(TR::ILOpCode &op, bool opposite = false)
{
    int32_t index = 0;
    if (op.isCompareTrueIfLess())
        index += 1;
    if (op.isCompareTrueIfGreater())
        index += 2;
    if (op.isCompareTrueIfEqual())
        index += 4;
    if (op.isUnsignedCompare())
        index += 8;

    if (opposite)
        index ^= 7;

    static const OP::Mnemonic opTable[] = {
        OP::bad,
        OP::JL4,
        OP::JG4,
        OP::JNE4,
        OP::JE4,
        OP::JLE4,
        OP::JGE4,
        OP::bad,
        OP::bad,
        OP::JB4,
        OP::JA4,
        OP::JNE4,
        OP::JE4,
        OP::JBE4,
        OP::JAE4,
        OP::bad,
    };
    return opTable[index];
}

TR::Register *J9::X86::TreeEvaluator::ZEROCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // NOTE: ZEROCHK is intended to be general and straightforward.  If you're
    // thinking of adding special code for specific situations in here, consider
    // whether you want to add your own CHK opcode instead.  If you feel the
    // need for special handling here, you may also want special handling in the
    // optimizer, in which case a separate opcode may be more suitable.
    //
    // On the other hand, if the improvements you're adding could benefit other
    // users of ZEROCHK, please go ahead and add them!
    //
    // If in doubt, discuss your design with your team lead.

    TR::LabelSymbol *slowPathLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    TR::LabelSymbol *restartLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    slowPathLabel->setStartInternalControlFlow();
    restartLabel->setEndInternalControlFlow();
    TR::Compilation *comp = cg->comp();

    // Temporarily hide the first child so it doesn't appear in the outlined call
    //
    node->rotateChildren(node->getNumChildren() - 1, 0);
    node->setNumChildren(node->getNumChildren() - 1);

    // Outlined instructions for check failure
    // Note: we don't pass the restartLabel in here because we don't want a
    // restart branch.
    //
    TR_OutlinedInstructions *outlinedHelperCall
        = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::call, NULL, slowPathLabel, NULL, cg);
    cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);
    cg->generateDebugCounter(outlinedHelperCall->getFirstInstruction(),
        TR::DebugCounter::debugCounterName(comp, "helperCalls/%s/(%s)/%d/%d", node->getOpCode().getName(),
            comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
        1, TR::DebugCounter::Cheap);

    // Restore the first child
    //
    node->setNumChildren(node->getNumChildren() + 1);
    node->rotateChildren(0, node->getNumChildren() - 1);

    // Children other than the first are only for the outlined path; we don't need them here
    //
    for (int32_t i = 1; i < node->getNumChildren(); i++)
        cg->recursivelyDecReferenceCount(node->getChild(i));

    // In-line instructions for the check
    //
    TR::Node *valueToCheck = node->getFirstChild();
    if (valueToCheck->getOpCode().isBooleanCompare() && isInteger(valueToCheck->getChild(0)->getOpCode(), cg)
        && isInteger(valueToCheck->getChild(1)->getOpCode(), cg)
        && performTransformation(comp, "O^O CODEGEN Optimizing ZEROCHK+%s %s\n", valueToCheck->getOpCode().getName(),
            valueToCheck->getName(cg->getDebug()))) {
        if (valueToCheck->getOpCode().isCompareForOrder()) {
            TR::TreeEvaluator::compareIntegersForOrder(valueToCheck, cg);
        } else {
            TR_ASSERT(valueToCheck->getOpCode().isCompareForEquality(),
                "Compare opcode must either be compare for order or for equality");
            TR::TreeEvaluator::compareIntegersForEquality(valueToCheck, cg);
        }
        Inst_Label(branchOpCodeForCompare(valueToCheck->getOpCode(), true), node, slowPathLabel, cg);
    } else {
        TR::Register *value = cg->evaluate(node->getFirstChild());
        Inst_RegReg(OP::TEST4RegReg, node, value, value, cg);
        cg->decReferenceCount(node->getFirstChild());
        Inst_Label(OP::JE4, node, slowPathLabel, cg);
    }
    Inst_Label(OP::label, node, restartLabel, cg);

    return NULL;
}

bool isConditionCodeSetForCompare(TR::Node *node, bool *jumpOnOppositeCondition, TR::Compilation *comp)
{
    // Disable.  Need to re-think how we handle overflow cases.
    //
    static char *disableNoCompareEFlags = feGetEnv("TR_disableNoCompareEFlags");
    if (disableNoCompareEFlags)
        return false;

    // See if there is a previous instruction that has set the condition flags
    // properly for this node's register
    //
    TR::Register *firstChildReg = node->getFirstChild()->getRegister();
    TR::Register *secondChildReg = node->getSecondChild()->getRegister();

    if (!firstChildReg || !secondChildReg)
        return false;

    // Find the last instruction that either
    //    1) sets the appropriate condition flags, or
    //    2) modifies the register to be tested
    // (and that hopefully does both)
    //
    TR::Instruction *prevInstr;
    for (prevInstr = comp->cg()->getAppendInstruction(); prevInstr; prevInstr = prevInstr->getPrev()) {
        if (prevInstr->getOpCodeValue() == OP::CMP4RegReg) {
            TR::Register *prevInstrTargetRegister = prevInstr->getTargetRegister();
            TR::Register *prevInstrSourceRegister = prevInstr->getSourceRegister();

            if (prevInstrTargetRegister && prevInstrSourceRegister
                && (((prevInstrSourceRegister == firstChildReg) && (prevInstrTargetRegister == secondChildReg))
                    || ((prevInstrSourceRegister == secondChildReg) && (prevInstrTargetRegister == firstChildReg)))) {
                if (performTransformation(comp, "O^O SKIP BOUND CHECK COMPARISON at node %p\n", node)) {
                    if (prevInstrTargetRegister == secondChildReg)
                        *jumpOnOppositeCondition = true;
                    return true;
                } else
                    return false;
            }
        }

        if (prevInstr->getOpCodeValue() == OP::label) {
            // This instruction is a possible branch target.
            return false;
        }

        if (prevInstr->getOpCode().modifiesSomeArithmeticFlags()) {
            // This instruction overwrites the condition flags.
            return false;
        }
    }

    return false;
}

void setImplicitNULLCHKExceptionInfo(TR::Node *node, TR::CodeGenerator *cg)
{
    TR_ASSERT_FATAL(node->hasFoldedImplicitNULLCHK(),
        "Attempt to set exception info on BNDCHK without implicit NULLCHK");

    TR::Compilation *comp = cg->comp();
    bool isTraceCG = comp->getOption(TR_TraceCG);

    TR::Instruction *faultingInstruction = cg->getImplicitExceptionPoint();
    if (faultingInstruction) {
        // Check and correctly set the implicit exception point
        //
        // The compare analyzer may have generated a secondary load to
        // resolved a compressed pointer and incorrectly set the exception point.
        // If that is the case, correctly set the exception point on
        // the comparison that actually throws the null pointer exception.
        //
        // The last instruction is a branch, the comparison is before.
        TR::Instruction *cmpInstruction = cg->getAppendInstruction()->getPrev();
        OP::Mnemonic mnemonic = cmpInstruction->getOpCodeValue();
        bool isComparisonMemForm = mnemonic == OP::CMP4MemReg || mnemonic == OP::CMP4RegMem;
        if (comp->useCompressedPointers() && faultingInstruction != cmpInstruction && isComparisonMemForm) {
            logprintf(isTraceCG, comp->log(), "Faulting instruction (previously %p) updated to %p\n",
                faultingInstruction, cmpInstruction);

            faultingInstruction = cmpInstruction;
            cg->setImplicitExceptionPoint(faultingInstruction);
        }

        faultingInstruction->setNeedsGCMap(0xFF00FFFF);
        faultingInstruction->setNode(node);
    }

    logprintf(isTraceCG, comp->log(), "Node %p has foldedimplicitNULLCHK, and a faulting instruction of %p\n", node,
        faultingInstruction);
}

TR::Register *J9::X86::TreeEvaluator::BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Node *firstChild = node->getFirstChild();
    TR::Node *secondChild = node->getSecondChild();

    // Perform a bound check.
    //
    // Value propagation or profile-directed optimization may have determined
    // that the array bound is a constant, and lowered TR::arraylength into an
    // iconst. In this case, make sure that the constant is the second child.
    //
    TR::LabelSymbol *boundCheckFailureLabel = generateLabelSymbol(cg);
    TR::Instruction *instr;
    TR::Compilation *comp = cg->comp();

    bool skippedComparison = false;
    bool jumpOnOppositeCondition = false;
    if (firstChild->getOpCode().isLoadConst()) {
        if (secondChild->getOpCode().isLoadConst() && firstChild->getInt() <= secondChild->getInt()) {
            instr = Inst_Label(OP::JMP4, node, boundCheckFailureLabel, cg);
            cg->decReferenceCount(firstChild);
            cg->decReferenceCount(secondChild);
        } else {
            if (!isConditionCodeSetForCompare(node, &jumpOnOppositeCondition, cg->comp())) {
                node->swapChildren();
                TR::TreeEvaluator::compareIntegersForOrder(node, cg);
                node->swapChildren();
                instr = Inst_Label(OP::JAE4, node, boundCheckFailureLabel, cg);
            } else
                skippedComparison = true;
        }
    } else {
        if (!isConditionCodeSetForCompare(node, &jumpOnOppositeCondition, cg->comp())) {
            TR::TreeEvaluator::compareIntegersForOrder(node, cg);
            instr = Inst_Label(OP::JBE4, node, boundCheckFailureLabel, cg);
        } else
            skippedComparison = true;
    }

    if (skippedComparison) {
        if (jumpOnOppositeCondition)
            instr = Inst_Label(OP::JAE4, node, boundCheckFailureLabel, cg);
        else
            instr = Inst_Label(OP::JBE4, node, boundCheckFailureLabel, cg);

        cg->decReferenceCount(firstChild);
        cg->decReferenceCount(secondChild);
    }

    cg->addSnippet(new (cg->trHeapMemory())
            TR::X86CheckFailureSnippet(cg, node->getSymbolReference(), boundCheckFailureLabel, instr, false));
    if (node->hasFoldedImplicitNULLCHK())
        setImplicitNULLCHKExceptionInfo(node, cg);

    firstChild->setIsNonNegative(true);
    secondChild->setIsNonNegative(true);

    return NULL;
}

TR::Register *J9::X86::TreeEvaluator::ArrayCopyBNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // Check that first child >= second child
    //
    // If the first child is a constant and the second isn't, swap the children.
    //
    TR::Node *firstChild = node->getFirstChild();
    TR::Node *secondChild = node->getSecondChild();
    TR::LabelSymbol *boundCheckFailureLabel = generateLabelSymbol(cg);
    TR::Instruction *instr;

    if (firstChild->getOpCode().isLoadConst()) {
        if (secondChild->getOpCode().isLoadConst()) {
            if (firstChild->getInt() < secondChild->getInt()) {
                // Check will always fail, just jump to failure snippet
                //
                instr = Inst_Label(OP::JMP4, node, boundCheckFailureLabel, cg);
            } else {
                // Check will always succeed, no need for an instruction
                //
                instr = NULL;
            }
            cg->decReferenceCount(firstChild);
            cg->decReferenceCount(secondChild);
        } else {
            node->swapChildren();
            TR::TreeEvaluator::compareIntegersForOrder(node, cg);
            node->swapChildren();
            instr = Inst_Label(OP::JG4, node, boundCheckFailureLabel, cg);
        }
    } else {
        TR::TreeEvaluator::compareIntegersForOrder(node, cg);
        instr = Inst_Label(OP::JL4, node, boundCheckFailureLabel, cg);
    }

    if (instr)
        cg->addSnippet(new (cg->trHeapMemory())
                TR::X86CheckFailureSnippet(cg, node->getSymbolReference(), boundCheckFailureLabel, instr, false));

    return NULL;
}

TR::Register *J9::X86::TreeEvaluator::ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
    TR::Instruction *prevInstr = cg->getAppendInstruction();
    TR::LabelSymbol *startLabel, *startOfWrtbarLabel, *doNullStoreLabel, *doneLabel;

    // skipStoreNullCheck
    // skipJLOCheck
    // skipSuperClassCheck
    // cannotSkipWriteBarrier

    flags16_t actions;

    TR::Node *firstChild = node->getFirstChild();
    TR::Node *sourceChild = firstChild->getSecondChild();

    static bool isRealTimeGC = comp->getOptions()->realTimeGC();
    auto gcMode = TR::Compiler->om.writeBarrierType();

    bool isNonRTWriteBarrierRequired = (gcMode != gc_modron_wrtbar_none && !firstChild->skipWrtBar()) ? true : false;
    bool generateWriteBarrier = isRealTimeGC || isNonRTWriteBarrierRequired;
    bool nopASC = (node->getArrayStoreClassInNode() && comp->performVirtualGuardNOPing()
                      && !fej9->classHasBeenExtended(node->getArrayStoreClassInNode()))
        ? true
        : false;

    // OffHeap runs defer destination evaluation after GC point.
    static char *disableDeferDestinationEvaluation = feGetEnv("TR_DisableDeferDestinationEvaluation");
    bool deferDestinationEvaluation
        = TR::Compiler->om.isOffHeapAllocationEnabled() && !disableDeferDestinationEvaluation;

    doneLabel = generateLabelSymbol(cg);
    doneLabel->setEndInternalControlFlow();

    if (generateWriteBarrier) {
        startOfWrtbarLabel = generateLabelSymbol(cg);
        // For OffHeap we use mainline store for null stores to consolidate store paths and defer
        // destination evaluation. OffHeap will perform redundant wrtbar on null stores.
        doNullStoreLabel = deferDestinationEvaluation ? startOfWrtbarLabel : generateLabelSymbol(cg);
    } else {
        startOfWrtbarLabel = doneLabel;
        doNullStoreLabel = doneLabel;
    }

    bool usingCompressedPointers = false;
    bool usingLowMemHeap = false;
    bool useShiftedOffsets = (TR::Compiler->om.compressedReferenceShiftOffset() != 0);

    if (comp->useCompressedPointers() && firstChild->getOpCode().isIndirect()) {
        usingLowMemHeap = true;
        usingCompressedPointers = true;

        if (useShiftedOffsets) {
            while ((sourceChild->getNumChildren() > 0) && (sourceChild->getOpCodeValue() != TR::a2l))
                sourceChild = sourceChild->getFirstChild();
            if (sourceChild->getOpCodeValue() == TR::a2l)
                sourceChild = sourceChild->getFirstChild();
            // this is required so that different registers are
            // allocated for the actual store and translated values
            sourceChild->incReferenceCount();
        }
    }

    // -------------------------------------------------------------------------
    //
    // Evaluate all of the children here to avoid issues with internal control
    // flow and outlined instructions.
    //
    // -------------------------------------------------------------------------

    TR::MemoryReference *tempMR = NULL;
    TR::Node *dstArrayNode, *offsetNode = NULL;

    if (generateWriteBarrier) {
        if (!deferDestinationEvaluation) {
            tempMR = MRef_node(firstChild, cg);
        } else {
            /* Evaluate destination subtrees
             * ArrayStoreCHK
             *    awrtbari  // firstChild
             *      aloadi  <contiguousArrayDataAddrField>
             *        aload     // dstArrayNode
             *      ...
             * OR
             * ArrayStoreCHK
             *    awrtbari  // firstChild
             *      aladd (internalPtr )
             *        aloadi  <contiguousArrayDataAddrField>
             *          aload     // dstArrayNode
             *        <offset>  // offsetNode
             *      ...
             */
            if (firstChild->getFirstChild()->isDataAddrPointer())
                dstArrayNode = firstChild->getFirstChild()->getFirstChild();
            else if (firstChild->getFirstChild()->getOpCodeValue() == TR::aladd
                && firstChild->getFirstChild()->getFirstChild()->isDataAddrPointer()) {
                dstArrayNode = firstChild->getFirstChild()->getFirstChild()->getFirstChild();
                offsetNode = firstChild->getFirstChild()->getSecondChild();
            } else {
                TR_ASSERT_FATAL(false, "Unexpected array access tree shape for OffHeap in ArrayStoreCHKEvaluator");
            }

            cg->evaluate(dstArrayNode);
            if (offsetNode
                && !(offsetNode->getOpCode().isLoadConst() && offsetNode->getLongInt() >= TR::getMinSigned<TR::Int32>()
                    && offsetNode->getLongInt() <= TR::getMaxSigned<TR::Int32>()))
                cg->evaluate(offsetNode);
        }
    }

    TR::Node *destinationChild = firstChild->getChild(2);
    TR::Register *destinationRegister = cg->evaluate(destinationChild);
    TR::Register *sourceRegister = cg->evaluate(sourceChild);

    TR_X86ScratchRegisterManager *scratchRegisterManager
        = cg->generateScratchRegisterManager(comp->target().is64Bit() ? 15 : 7);

    TR::Register *compressedRegister = NULL;
    if (usingCompressedPointers) {
        if (usingLowMemHeap && !useShiftedOffsets)
            compressedRegister = sourceRegister;
        else {
            // valid for useShiftedOffsets
            compressedRegister = cg->evaluate(firstChild->getSecondChild());
            if (!usingLowMemHeap) {
                Inst_RegReg(OP::TESTRegReg(), firstChild, sourceRegister, sourceRegister, cg);
                Inst_RegReg(OP::CMOVERegReg(), firstChild, compressedRegister, sourceRegister, cg);
            }
        }
    }

    // -------------------------------------------------------------------------
    //
    // If the source reference is NULL, the array store checks and the write
    // barrier can be bypassed.  Generate the NULL store in an outlined sequence.
    // For realtime GC we must still do the barrier.  If we are not generating
    // a write barrier then the store will happen inline.
    //
    // -------------------------------------------------------------------------

    startLabel = generateLabelSymbol(cg);
    startLabel->setStartInternalControlFlow();
    Inst_Label(OP::label, node, startLabel, cg);

    Inst_RegReg(OP::TESTRegReg(), node, sourceRegister, sourceRegister, cg);

    TR::LabelSymbol *nullTargetLabel = isRealTimeGC ? startOfWrtbarLabel : doNullStoreLabel;

    Inst_Label(OP::JE4, node, nullTargetLabel, cg);

    // -------------------------------------------------------------------------
    //
    // Generate up-front array store checks to avoid calling out to the helper.
    //
    // -------------------------------------------------------------------------

    TR::LabelSymbol *postASCLabel = NULL;
    if (nopASC) {
        // Speculatively NOP the array store check if VP is able to prove that the ASC
        // would always succeed given the current state of the class hierarchy.
        //
        TR::Node *helperCallNode
            = TR::Node::createWithSymRef(TR::call, 2, 2, sourceChild, destinationChild, node->getSymbolReference());
        helperCallNode->copyByteCodeInfo(node);

        TR::LabelSymbol *oolASCLabel = generateLabelSymbol(cg);
        TR::LabelSymbol *restartLabel;

        if (generateWriteBarrier) {
            restartLabel = startOfWrtbarLabel;
        } else {
            restartLabel = postASCLabel = generateLabelSymbol(cg);
        }

        TR_OutlinedInstructions *outlinedASCHelperCall = new (cg->trHeapMemory())
            TR_OutlinedInstructions(helperCallNode, TR::call, NULL, oolASCLabel, restartLabel, cg);
        cg->getOutlinedInstructionsList().push_front(outlinedASCHelperCall);

        static char *alwaysDoOOLASCc = feGetEnv("TR_doOOLASC");
        static bool alwaysDoOOLASC = alwaysDoOOLASCc ? true : false;

        if (!alwaysDoOOLASC) {
            TR_VirtualGuard *virtualGuard
                = TR_VirtualGuard::createArrayStoreCheckGuard(comp, node, node->getArrayStoreClassInNode());
            Inst_VirtualGuardNOP(node, virtualGuard->addNOPSite(), NULL, oolASCLabel, cg);
        } else {
            Inst_Label(OP::JMP4, node, oolASCLabel, cg);
        }

        // Restore the reference counts of the children created for the temporary vacll node above.
        //
        sourceChild->decReferenceCount();
        destinationChild->decReferenceCount();
    } else {
        TR::TreeEvaluator::VMarrayStoreCHKEvaluator(node, sourceChild, destinationChild, scratchRegisterManager,
            startOfWrtbarLabel, prevInstr, cg);
    }

    // -------------------------------------------------------------------------
    //
    // Generate write barrier.
    //
    // -------------------------------------------------------------------------

    bool isSourceNonNull = sourceChild->isNonNull();

    if (generateWriteBarrier) {
        Inst_Label(OP::label, node, startOfWrtbarLabel, cg);

        if (!isRealTimeGC) {
            // HACK: set the nullness property on the source so that the write barrier
            //       doesn't do the same test.
            //
            sourceChild->setIsNonNull(true);
        }

        if (deferDestinationEvaluation) {
            // Perform deferred destination evaluation
            tempMR = MRef_node(firstChild, cg);
        }

        TR::TreeEvaluator::VMwrtbarWithStoreEvaluator(node, tempMR, scratchRegisterManager, destinationChild,
            sourceChild, true, cg, true);
    } else if (postASCLabel) {
        // Lay down a arestart label for OOL ASC if the write barrier was skipped
        //
        Inst_Label(OP::label, node, postASCLabel, cg);
    }

    // -------------------------------------------------------------------------
    //
    // Either do the bypassed NULL store out of line or the reference store
    // inline if the write barrier was omitted.
    //
    // -------------------------------------------------------------------------

    TR::MemoryReference *tempMR2 = NULL;

    TR::Instruction *dependencyAnchorInstruction = NULL;

    if (!isRealTimeGC) {
        // OffHeap uses the already generated mainline VMwrtbarWithStoreEvaluator for null stores
        if (generateWriteBarrier && !deferDestinationEvaluation) {
            assert(isNonRTWriteBarrierRequired);
            assert(tempMR);

            // HACK: reset the nullness property on the source.
            //
            sourceChild->setIsNonNull(isSourceNonNull);

            // Perform the NULL store that was bypassed earlier by the write barrier.
            //
            TR_OutlinedInstructionsGenerator og(nullTargetLabel, node, cg);

            tempMR2 = MRef_MRefOff(*tempMR, 0, cg);

            if (usingCompressedPointers)
                Inst_MemReg(OP::S4MemReg, node, tempMR2, compressedRegister, cg);
            else
                Inst_MemReg(OP::SMemReg(), node, tempMR2, sourceRegister, cg);

            Inst_Label(OP::JMP4, node, doneLabel, cg);
            og.endOutlinedInstructionSequence();
        } else if (!generateWriteBarrier) {
            // No write barrier emitted.  Evaluate the store here.
            //
            assert(!isNonRTWriteBarrierRequired);
            assert(doneLabel == nullTargetLabel);

            // This is where the dependency condition will eventually go.
            //
            dependencyAnchorInstruction = cg->getAppendInstruction();

            tempMR = MRef_node(firstChild, cg);

            TR::X86MemRegInstruction *storeInstr;

            if (usingCompressedPointers)
                storeInstr = Inst_MemReg(OP::S4MemReg, node, tempMR, compressedRegister, cg);
            else
                storeInstr = Inst_MemReg(OP::SMemReg(), node, tempMR, sourceRegister, cg);

            cg->setImplicitExceptionPoint(storeInstr);

            if (!usingLowMemHeap || useShiftedOffsets)
                cg->decReferenceCount(sourceChild);
            cg->decReferenceCount(destinationChild);
            tempMR->decNodeReferenceCounts(cg);
        }
    }

    // -------------------------------------------------------------------------
    //
    // Generate outermost register dependencies
    //
    // -------------------------------------------------------------------------

    TR::RegisterDependencyConditions *deps = RegDeps(13, 13, cg);
    deps->unionPostCondition(destinationRegister, TR::RealRegister::NoReg, cg);
    deps->unionPostCondition(sourceRegister, TR::RealRegister::NoReg, cg);

    scratchRegisterManager->addScratchRegistersToDependencyList(deps);

    if (usingCompressedPointers && (!usingLowMemHeap || useShiftedOffsets)) {
        deps->unionPostCondition(compressedRegister, TR::RealRegister::NoReg, cg);
    }

    if (generateWriteBarrier) {
        // Memory reference is not live in an internal control flow region.
        //
        if (tempMR->getBaseRegister() && tempMR->getBaseRegister() != destinationRegister) {
            deps->unionPostCondition(tempMR->getBaseRegister(), TR::RealRegister::NoReg, cg);
        }

        if (tempMR->getIndexRegister() && tempMR->getIndexRegister() != destinationRegister) {
            deps->unionPostCondition(tempMR->getIndexRegister(), TR::RealRegister::NoReg, cg);
        }

        if (deferDestinationEvaluation && dstArrayNode->getRegister() != destinationRegister) {
            // For OffHeap tempMR->getBaseRegister() would be the dataAddrPtr not the baseArray.
            deps->unionPostCondition(dstArrayNode->getRegister(), TR::RealRegister::NoReg, cg);
        }

        if (comp->target().is64Bit()) {
            TR::Register *addressRegister = tempMR->getAddressRegister();
            if (addressRegister && addressRegister != destinationRegister) {
                deps->unionPostCondition(addressRegister, TR::RealRegister::NoReg, cg);
            }
        }
    }

    if (tempMR2 && comp->target().is64Bit()) {
        TR::Register *addressRegister = tempMR2->getAddressRegister();
        if (addressRegister && addressRegister != destinationRegister)
            deps->unionPostCondition(addressRegister, TR::RealRegister::NoReg, cg);
    }

    deps->unionPostCondition(cg->getVMThreadRegister(),
        (TR::RealRegister::RegNum)cg->getVMThreadRegister()->getAssociation(), cg);

    deps->stopAddingConditions();

    scratchRegisterManager->stopUsingRegisters();

    if (dependencyAnchorInstruction) {
        Inst_Label(dependencyAnchorInstruction, OP::label, doneLabel, deps, cg);
    } else {
        Inst_Label(OP::label, node, doneLabel, deps, cg);
    }

    if (usingCompressedPointers) {
        cg->decReferenceCount(firstChild->getSecondChild());
        cg->decReferenceCount(firstChild);
    }

    if (comp->useAnchors() && firstChild->getOpCode().isIndirect())
        firstChild->setStoreAlreadyEvaluated(true);

    return NULL;
}

TR::Register *J9::X86::TreeEvaluator::ArrayCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    return TR::TreeEvaluator::VMarrayCheckEvaluator(node, cg);
}

// Handles both BNDCHKwithSpineCHK and SpineCHK nodes.
//
TR::Register *J9::X86::TreeEvaluator::BNDCHKwithSpineCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
    bool needsBoundCheck = (node->getOpCodeValue() == TR::BNDCHKwithSpineCHK) ? true : false;

    TR::Node *loadOrStoreChild = node->getFirstChild();
    TR::Node *baseArrayChild = node->getSecondChild();
    TR::Node *arrayLengthChild;
    TR::Node *indexChild;

    if (needsBoundCheck) {
        arrayLengthChild = node->getChild(2);
        indexChild = node->getChild(3);
    } else {
        arrayLengthChild = NULL;
        indexChild = node->getChild(2);
    }

    // Perform a bound check.
    //
    // Value propagation or profile-directed optimization may have determined
    // that the array bound is a constant, and lowered TR::arraylength into an
    // iconst.  In this case, make sure that the constant is the second child.
    //
    OP::Mnemonic branchOpCode;

    // For primitive stores anchored under the check node, we must evaluate the source node
    // before the bound check branch so that its available to the snippet.  We can make
    // an exception for constant values that could be folded directly into a immediate
    // store instruction.
    //
    if (loadOrStoreChild->getOpCode().isStore() && loadOrStoreChild->getReferenceCount() <= 1) {
        TR::Node *valueChild = loadOrStoreChild->getSecondChild();

        if (!valueChild->getOpCode().isLoadConst()
            || (valueChild->getOpCode().isLoadConst()
                && ((valueChild->getDataType() == TR::Float) || (valueChild->getDataType() == TR::Double)
                    || (comp->target().is64Bit() && !IS_32BIT_SIGNED(valueChild->getLongInt()))))) {
            cg->evaluate(valueChild);
        }
    }

    TR::Register *baseArrayReg = cg->evaluate(baseArrayChild);

    TR::TreeEvaluator::preEvaluateEscapingNodesForSpineCheck(node, cg);

    TR::Instruction *faultingInstruction = NULL;

    TR::LabelSymbol *boundCheckFailureLabel = generateLabelSymbol(cg);
    TR::X86LabelInstruction *checkInstr = NULL;

    if (needsBoundCheck) {
        if (arrayLengthChild->getOpCode().isLoadConst()) {
            if (indexChild->getOpCode().isLoadConst() && arrayLengthChild->getInt() <= indexChild->getInt()) {
                // Create real check failure snippet if we can prove the
                // bound check will always fail.
                //
                branchOpCode = OP::JMP4;
                cg->decReferenceCount(arrayLengthChild);
                cg->decReferenceCount(indexChild);
            } else {
                TR::DataType dt = loadOrStoreChild->getDataType();
                int32_t elementSize
                    = (dt == TR::Address) ? TR::Compiler->om.sizeofReferenceField() : TR::Symbol::convertTypeToSize(dt);

                if (TR::Compiler->om.isDiscontiguousArray(arrayLengthChild->getInt(), elementSize)) {
                    // Create real check failure snippet if we can prove the spine check
                    // will always fail
                    //
                    branchOpCode = OP::JMP4;
                    cg->decReferenceCount(arrayLengthChild);
                    if (!indexChild->getOpCode().isLoadConst()) {
                        cg->evaluate(indexChild);
                    }

                    cg->decReferenceCount(indexChild);

                    faultingInstruction = cg->getImplicitExceptionPoint();
                } else {
                    // Check the bounds.
                    //
                    TR::TreeEvaluator::compareIntegersForOrder(node, indexChild, arrayLengthChild, cg);
                    branchOpCode = OP::JAE4;
                    faultingInstruction = cg->getImplicitExceptionPoint();
                }
            }
        } else {
            // Check the bounds.
            //
            TR::TreeEvaluator::compareIntegersForOrder(node, arrayLengthChild, indexChild, cg);
            branchOpCode = OP::JBE4;
            faultingInstruction = cg->getImplicitExceptionPoint();
        }

        static char *forceArraylet = feGetEnv("TR_forceArraylet");
        if (forceArraylet) {
            branchOpCode = OP::JMP4;
        }

        checkInstr = Inst_Label(branchOpCode, node, boundCheckFailureLabel, cg);
    } else {
        // -------------------------------------------------------------------------
        // Check if the base array has a spine.  If so, process out of line.
        // -------------------------------------------------------------------------

        if (!indexChild->getOpCode().isLoadConst()) {
            cg->evaluate(indexChild);
        }

        TR::MemoryReference *arraySizeMR = MRef_Bdisp32(baseArrayReg, fej9->getOffsetOfContiguousArraySizeField(), cg);

        Inst_MemImm(OP::CMP4MemImms, node, arraySizeMR, 0, cg);
        Inst_Label(OP::JE4, node, boundCheckFailureLabel, cg);
    }

    // -----------------------------------------------------------------------------------
    // Track all virtual register use within the mainline path.  This info will be used
    // to adjust the virtual register use counts within the outlined path for more precise
    // register assignment.
    // -----------------------------------------------------------------------------------

    cg->startRecordingRegisterUsage();

    TR::Register *loadOrStoreReg = NULL;
    TR::Register *valueReg = NULL;

    int32_t indexValue;

    // For reference stores, only evaluate the array element address because the store cannot
    // happen here (it must be done via the array store check).
    //
    // For primitive stores, evaluate them now.
    //
    // For loads, evaluate them now.
    //
    if (loadOrStoreChild->getOpCode().isStore()) {
        if (loadOrStoreChild->getReferenceCount() > 1) {
            TR_ASSERT(loadOrStoreChild->getOpCode().isWrtBar(), "Opcode must be wrtbar");
            loadOrStoreReg = cg->evaluate(loadOrStoreChild->getFirstChild());
            cg->decReferenceCount(loadOrStoreChild->getFirstChild());
        } else {
            // If the store is not commoned then it must be a primitive store.
            //
            loadOrStoreReg = cg->evaluate(loadOrStoreChild);
            valueReg = loadOrStoreChild->getSecondChild()->getRegister();

            if (!valueReg) {
                // If the immediate value was not evaluated then it must have been folded
                // into the instruction.
                //
                TR_ASSERT(loadOrStoreChild->getSecondChild()->getOpCode().isLoadConst(),
                    "unevaluated, non-constant value child");
                TR_ASSERT(IS_32BIT_SIGNED(loadOrStoreChild->getSecondChild()->getInt()),
                    "immediate value too wide for instruction");
            }
        }
    } else {
        loadOrStoreReg = cg->evaluate(loadOrStoreChild);
    }

    // -----------------------------------------------------------------------------------
    // Stop tracking virtual register usage.
    // -----------------------------------------------------------------------------------

    TR::list<OMR::RegisterUsage *> *mainlineRUL = cg->stopRecordingRegisterUsage();

    TR::Register *indexReg = indexChild->getRegister();

    // Index register must be in a register or a constant.
    //
    TR_ASSERT((indexReg || indexChild->getOpCode().isLoadConst()),
        "index child is not evaluated or constant: indexReg=%p, indexChild=%p", indexReg, indexChild);

    if (indexReg) {
        indexValue = -1;
    } else {
        indexValue = indexChild->getInt();
    }

    // TODO: don't always require the VM thread
    //
    TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, 1, cg);

    deps->addPostCondition(cg->getVMThreadRegister(),
        (TR::RealRegister::RegNum)cg->getVMThreadRegister()->getAssociation(), cg);

    deps->stopAddingConditions();

    TR::LabelSymbol *mergeLabel = generateLabelSymbol(cg);
    mergeLabel->setInternalControlFlowMerge();
    TR::X86LabelInstruction *restartInstr = Inst_Label(OP::label, node, mergeLabel, deps, cg);

    TR_OutlinedInstructions *arrayletOI
        = generateArrayletReference(node, loadOrStoreChild, checkInstr, boundCheckFailureLabel, mergeLabel,
            baseArrayReg, loadOrStoreReg, indexReg, indexValue, valueReg, needsBoundCheck, cg);

    arrayletOI->setMainlinePathRegisterUsageList(mainlineRUL);

    if (node->hasFoldedImplicitNULLCHK()) {
        if (faultingInstruction) {
            faultingInstruction->setNeedsGCMap(0xFF00FFFF);
            faultingInstruction->setNode(node);
        }
    }

    if (arrayLengthChild)
        arrayLengthChild->setIsNonNegative(true);

    indexChild->setIsNonNegative(true);

    cg->decReferenceCount(loadOrStoreChild);
    cg->decReferenceCount(baseArrayChild);

    if (!needsBoundCheck) {
        // Spine checks must decrement the reference count on the index explicitly.
        //
        cg->decReferenceCount(indexChild);
    }

    return NULL;
}

/*
 * this evaluator is used specifically for evaluate the following three nodes
 *
 *  storFence
 *  loadFence
 *  storeFence
 *
 * Since Java specification for loadfence and storefenc is stronger
 * than the intel specification, a full mfence instruction have to
 * be used for all three of them
 *
 * Due to performance penalty of mfence, a faster lockor on RSP is used
 * it achieve the same functionality but runs faster.
 */
TR::Register *J9::X86::TreeEvaluator::barrierFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::ILOpCodes opCode = node->getOpCodeValue();
    if (opCode == TR::fullFence && node->canOmitSync()) {
        Inst_Label(OP::label, node, generateLabelSymbol(cg), cg);
    } else if (cg->comp()->getOption(TR_X86UseMFENCE)) {
        Inst(OP::MFENCE, node, cg);
    } else {
        TR::RealRegister *stackReg = cg->machine()->getRealRegister(TR::RealRegister::esp);
        TR::MemoryReference *mr = MRef_Bdisp32(stackReg, intptr_t(0), cg);

        mr->setRequiresLockPrefix();
        Inst_MemImm(OP::OR4MemImms, node, mr, 0, cg);
        cg->stopUsingRegister(stackReg);
    }
    return NULL;
}

TR::Register *J9::X86::TreeEvaluator::readbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    TR_ASSERT(node->getNumChildren() == 1, "readbar node should have one child");
    TR::Node *handleNode = node->getChild(0);

    TR::Compilation *comp = cg->comp();

    bool needBranchAroundForNULL = !node->hasFoldedImplicitNULLCHK() && !node->isNonNull();

    if (comp->getOption(TR_TraceCG)) {
        OMR::Logger *log = comp->log();
        log->printf("\nnode %p has folded implicit nullchk: %d\n", node, node->hasFoldedImplicitNULLCHK());
        log->printf("node %p is nonnull: %d\n", node, node->isNonNull());
        log->printf("node %p needs branchAround: %d\n", node, needBranchAroundForNULL);
    }

    TR::LabelSymbol *startLabel = NULL;
    TR::LabelSymbol *doneLabel = NULL;
    if (needBranchAroundForNULL) {
        startLabel = generateLabelSymbol(cg);
        doneLabel = generateLabelSymbol(cg);

        Inst_Label(OP::label, node, startLabel, cg);
        startLabel->setStartInternalControlFlow();
    }

    TR::Register *handleRegister = cg->intClobberEvaluate(handleNode);

    if (needBranchAroundForNULL) {
        // if handle is NULL, then just branch around the redirection
        Inst_RegReg(OP::TESTRegReg(), node, handleRegister, handleRegister, cg);
        Inst_Label(OP::JE4, handleNode, doneLabel, cg);
    }

    // handle is not NULL or we're an implicit nullcheck, so go through forwarding pointer to get object
    TR::MemoryReference *handleMR = MRef_Bdisp32(handleRegister, node->getSymbolReference()->getOffset(), cg);
    TR::Instruction *forwardingInstr = Inst_RegMem(OP::L4RegMem, handleNode, handleRegister, handleMR, cg);
    cg->setImplicitExceptionPoint(forwardingInstr);

    if (needBranchAroundForNULL) {
        TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, 1, cg);
        deps->addPostCondition(handleRegister, TR::RealRegister::NoReg, cg);

        // and we're done
        Inst_Label(OP::label, node, doneLabel, deps, cg);

        doneLabel->setEndInternalControlFlow();
    }

    node->setRegister(handleRegister);
    cg->decReferenceCount(handleNode);

    return handleRegister;
}

static TR::Register *highestOneBit(TR::Node *node, TR::CodeGenerator *cg, TR::Register *reg, bool is64Bit)
{
    // xor r1, r1
    // bsr r2, reg
    // setne r1
    // shl r1, r2
    TR::Register *scratchReg = cg->allocateRegister();
    TR::Register *bsrReg = cg->allocateRegister();
    Inst_RegReg(OP::XOR4RegReg, node, scratchReg, scratchReg, cg);
    Inst_RegReg(OP::BSRRegReg(is64Bit), node, bsrReg, reg, cg);
    Inst_Reg(OP::SETNE1Reg, node, scratchReg, cg);
    TR::RegisterDependencyConditions *shiftDependencies = RegDeps((uint8_t)1, 1, cg);
    shiftDependencies->addPreCondition(bsrReg, TR::RealRegister::ecx, cg);
    shiftDependencies->addPostCondition(bsrReg, TR::RealRegister::ecx, cg);
    shiftDependencies->stopAddingConditions();
    Inst_RegReg(OP::SHLRegCL(is64Bit), node, scratchReg, bsrReg, shiftDependencies, cg);
    cg->stopUsingRegister(bsrReg);
    return scratchReg;
}

TR::Register *J9::X86::TreeEvaluator::integerHighestOneBit(TR::Node *node, TR::CodeGenerator *cg)
{
    TR_ASSERT(node->getNumChildren() == 1, "Node has a wrong number of children (i.e. !=1 )! ");
    TR::Node *child = node->getFirstChild();
    TR::Register *inputReg = cg->evaluate(child);
    TR::Register *resultReg = highestOneBit(node, cg, inputReg, cg->comp()->target().is64Bit());
    cg->decReferenceCount(child);
    node->setRegister(resultReg);
    return resultReg;
}

TR::Register *J9::X86::TreeEvaluator::longHighestOneBit(TR::Node *node, TR::CodeGenerator *cg)
{
    TR_ASSERT(node->getNumChildren() == 1, "Node has a wrong number of children (i.e. !=1 )! ");
    TR::Node *child = node->getFirstChild();
    TR::Register *inputReg = cg->evaluate(child);
    TR::Register *resultReg = NULL;
    if (cg->comp()->target().is64Bit()) {
        resultReg = highestOneBit(node, cg, inputReg, true);
    } else {
        // mask out low part result if high part is not 0
        // xor r1 r1
        // cmp inputHigh, 0
        // setne r1
        // dec r1
        // and resultLow, r1
        // ret resultHigh:resultLow
        TR::Register *inputLow = inputReg->getLowOrder();
        TR::Register *inputHigh = inputReg->getHighOrder();
        TR::Register *maskReg = cg->allocateRegister();
        TR::Register *resultHigh = highestOneBit(node, cg, inputHigh, false);
        TR::Register *resultLow = highestOneBit(node, cg, inputLow, false);
        Inst_RegReg(OP::XOR4RegReg, node, maskReg, maskReg, cg);
        Inst_RegImm(OP::CMP4RegImm4, node, inputHigh, 0, cg);
        Inst_Reg(OP::SETNE1Reg, node, maskReg, cg);
        Inst_Reg(OP::DEC4Reg, node, maskReg, cg);
        Inst_RegReg(OP::AND4RegReg, node, resultLow, maskReg, cg);
        resultReg = cg->allocateRegisterPair(resultLow, resultHigh);
        cg->stopUsingRegister(maskReg);
    }
    cg->decReferenceCount(child);
    node->setRegister(resultReg);
    return resultReg;
}

static TR::Register *lowestOneBit(TR::Node *node, TR::CodeGenerator *cg, TR::Register *reg, bool is64Bit)
{
    TR::Register *resultReg = cg->allocateRegister();
    Inst_RegReg(OP::MOVRegReg(is64Bit), node, resultReg, reg, cg);
    Inst_Reg(OP::NEGReg(is64Bit), node, resultReg, cg);
    Inst_RegReg(OP::ANDRegReg(is64Bit), node, resultReg, reg, cg);
    return resultReg;
}

TR::Register *J9::X86::TreeEvaluator::integerLowestOneBit(TR::Node *node, TR::CodeGenerator *cg)
{
    TR_ASSERT(node->getNumChildren() == 1, "Node has a wrong number of children (i.e. !=1 )! ");
    TR::Node *child = node->getFirstChild();
    TR::Register *inputReg = cg->evaluate(child);
    TR::Register *reg = lowestOneBit(node, cg, inputReg, cg->comp()->target().is64Bit());
    node->setRegister(reg);
    cg->decReferenceCount(child);
    return reg;
}

TR::Register *J9::X86::TreeEvaluator::longLowestOneBit(TR::Node *node, TR::CodeGenerator *cg)
{
    TR_ASSERT(node->getNumChildren() == 1, "Node has a wrong number of children (i.e. !=1 )! ");
    TR::Node *child = node->getFirstChild();
    TR::Register *inputReg = cg->evaluate(child);
    TR::Register *resultReg = NULL;
    if (cg->comp()->target().is64Bit()) {
        resultReg = lowestOneBit(node, cg, inputReg, true);
    } else {
        // mask out high part if low part is not 0
        // xor r1, r1
        // get low result
        // setne r1
        // dec   r1
        // and   r1, inputHigh
        // get high result
        // return resultHigh:resultLow
        TR::Register *inputHigh = inputReg->getHighOrder();
        TR::Register *inputLow = inputReg->getLowOrder();
        TR::Register *scratchReg = cg->allocateRegister();
        Inst_RegReg(OP::XOR4RegReg, node, scratchReg, scratchReg, cg);
        TR::Register *resultLow = lowestOneBit(node, cg, inputLow, false);
        Inst_Reg(OP::SETNE1Reg, node, scratchReg, cg);
        Inst_Reg(OP::DEC4Reg, node, scratchReg, cg);
        Inst_RegReg(OP::AND4RegReg, node, scratchReg, inputHigh, cg);
        TR::Register *resultHigh = lowestOneBit(node, cg, scratchReg, false);
        cg->stopUsingRegister(scratchReg);
        resultReg = cg->allocateRegisterPair(resultLow, resultHigh);
    }
    node->setRegister(resultReg);
    cg->decReferenceCount(child);
    return resultReg;
}

static TR::Register *numberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg, TR::Register *reg, bool is64Bit,
    bool isLong)
{
    // xor r1, r1
    // bsr r2, reg
    // sete r1
    // dec r1
    // inc r2
    // and r2, r1
    // mov r1, is64Bit? 64: 32
    // sub r1, r2
    // ret r1
    TR::Register *maskReg = cg->allocateRegister();
    TR::Register *bsrReg = cg->allocateRegister();
    Inst_RegReg(OP::XOR4RegReg, node, maskReg, maskReg, cg);
    Inst_RegReg(OP::BSRRegReg(is64Bit), node, bsrReg, reg, cg);
    Inst_Reg(OP::SETE1Reg, node, maskReg, cg);
    Inst_Reg(OP::DECReg(is64Bit), node, maskReg, cg);
    Inst_Reg(OP::INCReg(is64Bit), node, bsrReg, cg);
    Inst_RegReg(OP::ANDRegReg(is64Bit), node, bsrReg, maskReg, cg);
    Inst_RegImm(OP::MOVRegImm4(is64Bit), node, maskReg, isLong ? 64 : 32, cg);
    Inst_RegReg(OP::SUBRegReg(is64Bit), node, maskReg, bsrReg, cg);
    cg->stopUsingRegister(bsrReg);
    return maskReg;
}

TR::Register *J9::X86::TreeEvaluator::integerNumberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg)
{
    TR_ASSERT(node->getNumChildren() == 1, "Node has a wrong number of children (i.e. !=1 )! ");
    TR::Node *child = node->getFirstChild();
    TR::Register *inputReg = cg->evaluate(child);
    TR::Register *resultReg = numberOfLeadingZeros(node, cg, inputReg, false, false);
    node->setRegister(resultReg);
    cg->decReferenceCount(child);
    return resultReg;
}

TR::Register *J9::X86::TreeEvaluator::longNumberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg)
{
    TR_ASSERT(node->getNumChildren() == 1, "Node has a wrong number of children (i.e. !=1 )! ");
    TR::Node *child = node->getFirstChild();
    TR::Register *inputReg = cg->evaluate(child);
    TR::Register *resultReg = NULL;
    if (cg->comp()->target().is64Bit()) {
        resultReg = numberOfLeadingZeros(node, cg, inputReg, true, true);
    } else {
        // keep low part if high part is 0
        // xor r1, r1
        // cmp inputHigh, 0
        // setne r1
        // dec r1
        // and resultLow, r1
        // add resultHigh, resultLow
        // return resultHigh
        TR::Register *inputHigh = inputReg->getHighOrder();
        TR::Register *inputLow = inputReg->getLowOrder();
        TR::Register *resultHigh = numberOfLeadingZeros(node, cg, inputHigh, false, false);
        TR::Register *resultLow = numberOfLeadingZeros(node, cg, inputLow, false, false);
        TR::Register *maskReg = cg->allocateRegister();
        Inst_RegReg(OP::XOR4RegReg, node, maskReg, maskReg, cg);
        Inst_RegImm(OP::CMP4RegImm4, node, inputHigh, 0, cg);
        Inst_Reg(OP::SETNE1Reg, node, maskReg, cg);
        Inst_Reg(OP::DEC4Reg, node, maskReg, cg);
        Inst_RegReg(OP::AND4RegReg, node, resultLow, maskReg, cg);
        Inst_RegReg(OP::ADD4RegReg, node, resultHigh, resultLow, cg);
        cg->stopUsingRegister(resultLow);
        cg->stopUsingRegister(maskReg);
        resultReg = resultHigh;
    }
    node->setRegister(resultReg);
    cg->decReferenceCount(child);
    return resultReg;
}

static TR::Register *numberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg, TR::Register *reg, bool is64Bit,
    bool isLong)
{
    // r1 is shift amount, r3 is the mask
    // xor r1, r1
    // bsf r2, reg
    // sete r1
    // mov r3, r1
    // dec r3
    // shl r1, is64Bit ? 6 : 5
    // and r2, r3
    // add r2, r1
    // return r2
    TR::Register *bsfReg = cg->allocateRegister();
    TR::Register *tempReg = cg->allocateRegister();
    TR::Register *maskReg = cg->allocateRegister();
    Inst_RegReg(OP::XOR4RegReg, node, tempReg, tempReg, cg);
    Inst_RegReg(OP::BSFRegReg(is64Bit), node, bsfReg, reg, cg);
    Inst_Reg(OP::SETE1Reg, node, tempReg, cg);
    Inst_RegReg(OP::MOVRegReg(is64Bit), node, maskReg, tempReg, cg);
    Inst_Reg(OP::DECReg(is64Bit), node, maskReg, cg);
    Inst_RegImm(OP::SHLRegImm1(is64Bit), node, tempReg, isLong ? 6 : 5, cg);
    Inst_RegReg(OP::ANDRegReg(is64Bit), node, bsfReg, maskReg, cg);
    Inst_RegReg(OP::ADDRegReg(is64Bit), node, bsfReg, tempReg, cg);
    cg->stopUsingRegister(tempReg);
    cg->stopUsingRegister(maskReg);
    return bsfReg;
}

TR::Register *J9::X86::TreeEvaluator::integerNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg)
{
    TR_ASSERT(node->getNumChildren() == 1, "Node has a wrong number of children (i.e. !=1 )! ");
    TR::Node *child = node->getFirstChild();
    TR::Register *inputReg = cg->evaluate(child);
    TR::Register *resultReg = numberOfTrailingZeros(node, cg, inputReg, cg->comp()->target().is64Bit(), false);
    node->setRegister(resultReg);
    cg->decReferenceCount(child);
    return resultReg;
}

TR::Register *J9::X86::TreeEvaluator::longNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg)
{
    TR_ASSERT(node->getNumChildren() == 1, "Node has a wrong number of children (i.e. !=1 )! ");
    TR::Node *child = node->getFirstChild();
    TR::Register *inputReg = cg->evaluate(child);
    TR::Register *resultReg = NULL;
    if (cg->comp()->target().is64Bit()) {
        resultReg = numberOfTrailingZeros(node, cg, inputReg, true, true);
    } else {
        // mask out result of high part if low part is not 32
        // xor r1, r1
        // cmp resultLow, 32
        // setne r1
        // dec r1
        // and r1, resultHigh
        // and resultLow, r1
        // return resultLow
        TR::Register *inputLow = inputReg->getLowOrder();
        TR::Register *inputHigh = inputReg->getHighOrder();
        TR::Register *maskReg = cg->allocateRegister();
        TR::Register *resultLow = numberOfTrailingZeros(node, cg, inputLow, false, false);
        TR::Register *resultHigh = numberOfTrailingZeros(node, cg, inputHigh, false, false);
        Inst_RegReg(OP::XOR4RegReg, node, maskReg, maskReg, cg);
        Inst_RegImm(OP::CMP4RegImm4, node, resultLow, 32, cg);
        Inst_Reg(OP::SETNE1Reg, node, maskReg, cg);
        Inst_Reg(OP::DEC4Reg, node, maskReg, cg);
        Inst_RegReg(OP::AND4RegReg, node, maskReg, resultHigh, cg);
        Inst_RegReg(OP::ADD4RegReg, node, resultLow, maskReg, cg);
        cg->stopUsingRegister(resultHigh);
        cg->stopUsingRegister(maskReg);
        resultReg = resultLow;
    }
    node->setRegister(resultReg);
    cg->decReferenceCount(child);
    return resultReg;
}

inline void generateInlinedCheckCastForDynamicCastClass(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    bool use64BitClasses = comp->target().is64Bit()
        && (!TR::Compiler->om.generateCompressedObjectHeaders()
            || (comp->compileRelocatableCode() && comp->getOption(TR_UseSymbolValidationManager)));
    TR::Register *ObjReg = cg->evaluate(node->getFirstChild());
    TR::Register *castClassReg = cg->evaluate(node->getSecondChild());
    TR::Register *temp1Reg = cg->allocateRegister();
    TR::Register *temp2Reg = cg->allocateRegister();
    TR::Register *objClassReg = cg->allocateRegister();

    bool isCheckCastAndNullCheck = (node->getOpCodeValue() == TR::checkcastAndNULLCHK);

    TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *fallThruLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *outlinedCallLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *throwLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *isClassLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *iTableLoopLabel = generateLabelSymbol(cg);
    startLabel->setStartInternalControlFlow();
    fallThruLabel->setEndInternalControlFlow();

    Inst_Label(OP::label, node, startLabel, cg);

    TR_OutlinedInstructions *outlinedHelperCall
        = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::call, NULL, outlinedCallLabel, fallThruLabel, cg);
    cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);

    // objClassReg holds object class also serves as null check
    if (isCheckCastAndNullCheck)
        generateLoadJ9Class(node, objClassReg, ObjReg, cg);

    // temp2Reg holds romClass of cast class, for testing array, interface class type
    Inst_RegMem(OP::LRegMem(), node, temp2Reg, MRef_Bdisp32(castClassReg, offsetof(J9Class, romClass), cg), cg);

    // If cast class is array, call out of line helper
    Inst_MemImm(OP::TEST4MemImm4, node, MRef_Bdisp32(temp2Reg, offsetof(J9ROMClass, modifiers), cg), J9AccClassArray,
        cg);
    Inst_Label(OP::JNE4, node, outlinedCallLabel, cg);

    // objClassReg holds object class
    if (!isCheckCastAndNullCheck) {
        Inst_RegReg(OP::TESTRegReg(), node, ObjReg, ObjReg, cg);
        Inst_Label(OP::JE4, node, fallThruLabel, cg);
        generateLoadJ9Class(node, objClassReg, ObjReg, cg);
    }

    // Object not array, inline checks
    // Check cast class is interface
    Inst_MemImm(OP::TEST4MemImm4, node, MRef_Bdisp32(temp2Reg, offsetof(J9ROMClass, modifiers), cg), J9AccInterface,
        cg);
    Inst_Label(OP::JE4, node, isClassLabel, cg);

    // Obtain I-Table
    // temp1Reg holds head of J9Class->iTable of obj class
    Inst_RegMem(OP::LRegMem(), node, temp1Reg, MRef_Bdisp32(objClassReg, offsetof(J9Class, iTable), cg), cg);
    // Loop through I-Table
    // temp1Reg holds iTable list element through the loop
    Inst_Label(OP::label, node, iTableLoopLabel, cg);
    Inst_RegReg(OP::TESTRegReg(), node, temp1Reg, temp1Reg, cg);
    Inst_Label(OP::JE4, node, throwLabel, cg);
    auto interfaceMR = MRef_Bdisp32(temp1Reg, offsetof(J9ITable, interfaceClass), cg);
    Inst_MemReg(OP::CMPMemReg(), node, interfaceMR, castClassReg, cg);
    Inst_RegMem(OP::LRegMem(), node, temp1Reg, MRef_Bdisp32(temp1Reg, offsetof(J9ITable, next), cg), cg);
    Inst_Label(OP::JNE4, node, iTableLoopLabel, cg);

    // Found from I-Table
    Inst_Label(OP::JMP4, node, fallThruLabel, cg);

    // cast class is non-interface class
    Inst_Label(OP::label, node, isClassLabel, cg);
    // equality test
    Inst_RegReg(OP::CMPRegReg(use64BitClasses), node, objClassReg, castClassReg, cg);
    Inst_Label(OP::JE4, node, fallThruLabel, cg);

    // class not equal
    // temp2 holds cast class depth
    // class depth mask must be low 16 bits to safely load without the mask.
    static_assert(J9AccClassDepthMask == 0xffff, "J9_JAVA_CLASS_DEPTH_MASK must be 0xffff");
    Inst_RegMem(comp->target().is64Bit() ? OP::MOVZXReg8Mem2 : OP::MOVZXReg4Mem2, node, temp2Reg,
        MRef_Bdisp32(castClassReg, offsetof(J9Class, classDepthAndFlags), cg), cg);

    // cast class depth >= obj class depth, throw
    Inst_RegMem(OP::CMP2RegMem, node, temp2Reg, MRef_Bdisp32(objClassReg, offsetof(J9Class, classDepthAndFlags), cg),
        cg);
    Inst_Label(OP::JAE4, node, throwLabel, cg);

    // check obj class's super class array entry
    // temp1Reg holds superClasses array of obj class
    // An alternative sequences requiring one less register may be:
    // SHL temp2Reg, 3 for 64-bit or 2 for 32-bit
    // ADD temp2Reg, [temp3Reg, superclasses offset]
    // CMP classClassReg, [temp2Reg]
    // On 64 bit, the extra reg isn't likely to cause significant register pressure.
    // On 32 bit, it could put more register pressure due to limited number of regs.
    // Since 64-bit is more prevalent, we opt to optimize for 64bit in this case
    Inst_RegMem(OP::LRegMem(), node, temp1Reg, MRef_Bdisp32(objClassReg, offsetof(J9Class, superclasses), cg), cg);
    Inst_RegMem(OP::CMPRegMem(use64BitClasses), node, castClassReg,
        MRef_BIS(temp1Reg, temp2Reg, comp->target().is64Bit() ? 3 : 2, cg), cg);
    Inst_Label(OP::JNE4, node, throwLabel, cg);

    // throw classCastException
    {
        TR_OutlinedInstructionsGenerator og(throwLabel, node, cg);
        Inst_Reg(OP::PUSHReg, node, objClassReg, cg);
        Inst_Reg(OP::PUSHReg, node, castClassReg, cg);
        auto call = Inst_HelperCall(node, TR_throwClassCastException, NULL, cg);
        call->setNeedsGCMap(0xFF00FFFF);
        call->setAdjustsFramePointerBy(-2 * (int32_t)sizeof(J9Class *));
        og.endOutlinedInstructionSequence();
    }

    TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, 8, cg);

    deps->addPostCondition(ObjReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(castClassReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(temp1Reg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(temp2Reg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(objClassReg, TR::RealRegister::NoReg, cg);

    TR::Node *callNode = outlinedHelperCall->getCallNode();
    TR::Register *reg;

    if (callNode->getFirstChild() == node->getFirstChild()) {
        reg = callNode->getFirstChild()->getRegister();
        if (reg)
            deps->unionPostCondition(reg, TR::RealRegister::NoReg, cg);
    }

    if (callNode->getSecondChild() == node->getSecondChild()) {
        reg = callNode->getSecondChild()->getRegister();
        if (reg)
            deps->unionPostCondition(reg, TR::RealRegister::NoReg, cg);
    }

    deps->stopAddingConditions();

    Inst_Label(OP::label, node, fallThruLabel, deps, cg);

    cg->stopUsingRegister(temp1Reg);
    cg->stopUsingRegister(temp2Reg);
    cg->stopUsingRegister(objClassReg);

    // Decrement use counts on the children
    //
    cg->decReferenceCount(node->getFirstChild());
    cg->decReferenceCount(node->getSecondChild());
}

/**
 * @brief Inline a checkcast, instanceof, or Class.isAssignableFrom() when the
 *     cast class is a [Ljava/lang/Object known at compile-time
 *
 * @details
 *
 * if (objectRef == NULL) {
 *     instanceof/isAssignableFrom : 0
 *     checkcast : no action
 * }
 *
 * if (objectRef is a non-primitive array) {
 *     instanceof/isAssignableFrom : 1
 *     checkcast : no action
 * } else {
 *     instanceof/isAssignableFrom : 0
 *     checkcast : throw OOL
 * }
 *
 * @param[in] node : \c TR::Node being evaluated
 * @param[in] clazz : compile-time \c J9Class address
 * @param[in] isCheckCast : true if a checkcast operation; false otherwise
 * @param[in] cg : \c CodeGenerator object
 *
 */
static void inlineCheckCastOrInstanceOfObjectArrayCastClass(TR::Node *node, TR_OpaqueClassBlock *clazz,
    bool isCheckCast, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    bool isAssignableFrom = (node->getOpCodeValue() == TR::icall);

    logprintf(comp->getOption(TR_TraceCG), comp->log(), "Inline %s for [jlO : node=%p, castClass=%p\n",
        isCheckCast ? "checkcast" : (isAssignableFrom ? "isAssignableFrom" : "instanceof"), node, clazz);

    TR::Node *objectNode = node->getFirstChild();
    TR::Node *castClassNode = node->getSecondChild();
    TR::Register *objectReg = cg->evaluate(objectNode);

    // The first child of a call to isAssignableFrom is already the class object
    //
    TR::Register *objectClassReg = isAssignableFrom ? objectReg : cg->allocateRegister();
    TR::Register *scratchReg = cg->allocateRegister();
    TR::Register *resultReg = isCheckCast ? NULL : cg->allocateRegister();

    TR::LabelSymbol *outlinedHelperCallLabel = NULL;
    TR_OutlinedInstructions *outlinedHelperCall = NULL;

    TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *fallThruLabel = generateLabelSymbol(cg);

    startLabel->setStartInternalControlFlow();
    fallThruLabel->setEndInternalControlFlow();

    if (isCheckCast) {
        outlinedHelperCallLabel = generateLabelSymbol(cg);
        outlinedHelperCall = new (cg->trHeapMemory())
            TR_OutlinedInstructions(node, TR::call, NULL, outlinedHelperCallLabel, fallThruLabel, cg);
        cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);
    }

    Inst_Label(OP::label, node, startLabel, cg);

    static char *breakOnInlineObjectArrayCheck = feGetEnv("TR_BreakOnInlineObjectArrayCheck");
    if (breakOnInlineObjectArrayCheck)
        Inst(OP::INT3, node, cg);

    if (!isCheckCast) {
        Inst_RegReg(OP::XOR4RegReg, node, resultReg, resultReg, cg);
    }

    // -----------------------------------------------------------------------
    // If the object is NULL, no exception is thrown for a checkcast and a 0
    // is returned for an instanceof.
    //
    // A NULL class passed to Class.isAssignableFrom() will be handled by a
    // NULLCHK node inserted before this call node.
    // -----------------------------------------------------------------------

    if (!objectNode->isNonNull()) {
        Inst_RegReg(OP::TESTRegReg(), node, objectReg, objectReg, cg);
        Inst_Label(OP::JE4, node, fallThruLabel, cg);
    }

    // The cast class is an array of j/l/Objects. Check if the
    // object class is a non-primitive array
    //
    generateLoadJ9Class(node, objectClassReg, objectReg, cg);

    // Aliases for code readability only
    //
    TR::Register *romClassReg = scratchReg, *componentClassReg = scratchReg;

    TR::LabelSymbol *notCastableLabel = isCheckCast ? outlinedHelperCallLabel : fallThruLabel;

    // Check if romClass is an array
    //
    Inst_RegMem(OP::LRegMem(), node, romClassReg, MRef_Bdisp32(objectClassReg, offsetof(J9Class, romClass), cg), cg);
    Inst_MemImm(OP::TEST4MemImm4, node, MRef_Bdisp32(romClassReg, offsetof(J9ROMClass, modifiers), cg), J9AccClassArray,
        cg);
    Inst_Label(OP::JE4, node, notCastableLabel, cg);

    // Check if object class is a primitive array
    //
    Inst_RegMem(OP::LRegMem(), node, componentClassReg,
        MRef_Bdisp32(objectClassReg, offsetof(J9ArrayClass, componentType), cg), cg);
    Inst_RegMem(OP::LRegMem(), node, romClassReg, MRef_Bdisp32(componentClassReg, offsetof(J9Class, romClass), cg), cg);
    Inst_MemImm(OP::TEST4MemImm4, node, MRef_Bdisp32(romClassReg, offsetof(J9ROMClass, modifiers), cg),
        J9AccClassInternalPrimitiveType, cg);
    Inst_Label(OP::JNE4, node, notCastableLabel, cg);

    if (!isCheckCast) {
        Inst_RegImm(OP::MOV4RegImm4, node, resultReg, 1, cg);
    }

    // clang-format off
    int32_t numRegDeps =
          1   // objectReg
        + ((objectReg != objectClassReg) ? 1 : 0)
        + (resultReg ? 1 : 0)
        + 1   // scratchReg
        + (outlinedHelperCall ? 2 : 0);  // 2 helper args: objectRef + castClass
    // clang-format on

    TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, numRegDeps, cg);

    deps->addPostCondition(objectReg, TR::RealRegister::NoReg, cg);

    if (objectReg != objectClassReg)
        deps->addPostCondition(objectClassReg, TR::RealRegister::NoReg, cg);

    if (resultReg)
        deps->addPostCondition(resultReg, TR::RealRegister::NoReg, cg);

    deps->addPostCondition(scratchReg, TR::RealRegister::NoReg, cg);

    if (outlinedHelperCall) {
        TR::Node *callNode = outlinedHelperCall->getCallNode();
        TR::Register *reg;

        if (callNode->getFirstChild() == node->getFirstChild()) {
            reg = callNode->getFirstChild()->getRegister();
            if (reg)
                deps->unionPostCondition(reg, TR::RealRegister::NoReg, cg);
        }

        if (callNode->getSecondChild() == node->getSecondChild()) {
            reg = callNode->getSecondChild()->getRegister();
            if (reg)
                deps->unionPostCondition(reg, TR::RealRegister::NoReg, cg);
        }
    }

    deps->stopAddingConditions();
    Inst_Label(OP::label, node, fallThruLabel, deps, cg);

    cg->stopUsingRegister(scratchReg);
    if (objectReg != objectClassReg)
        cg->stopUsingRegister(objectClassReg);

    if (!isCheckCast) {
        node->setRegister(resultReg);
    }

    cg->decReferenceCount(objectNode);
    cg->decReferenceCount(castClassNode);

    return;
}

/**
 * @brief Inline a checkcast, instanceof, or Class.isAssignableFrom() when the
 *     cast class is an array known at compile-time with a final leaf type
 *
 * @details
 *
 * if (objectRef == NULL) {
 *     instanceof/isAssignableFrom : 0
 *     checkcast : no action
 * }
 *
 * if (objectRef.class == castClass) {
 *     instanceof/isAssignableFrom : 1
 *     checkcast : no action
 * } else {
 *     instanceof/isAssignableFrom : 0
 *     checkcast : throw OOL
 * }
 *
 * @param[in] node : \c TR::Node being evaluated
 * @param[in] clazz : compile-time \c J9Class address
 * @param[in] isCheckCast : true if a checkcast operation; false otherwise
 * @param[in] cg : \c CodeGenerator object
 *
 */
static void inlineCheckCastOrInstanceOfFinalArrayCastClass(TR::Node *node, TR_OpaqueClassBlock *clazz, bool isCheckCast,
    TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    bool isAssignableFrom = (node->getOpCodeValue() == TR::icall);

    logprintf(comp->getOption(TR_TraceCG), comp->log(),
        "Inline %s for const final cast class array : node=%p, castClass=%p\n",
        isCheckCast ? "checkcast" : (isAssignableFrom ? "isAssignableFrom" : "instanceof"), node, clazz);

    TR::Node *objectNode = node->getFirstChild();
    TR::Node *castClassNode = node->getSecondChild();
    TR::Register *objectReg = cg->evaluate(objectNode);

    // The first child of a call to isAssignableFrom is already the class object
    //
    TR::Register *objectClassReg = isAssignableFrom ? objectReg : cg->allocateRegister();
    TR::Register *resultReg = isCheckCast ? NULL : cg->allocateRegister();
    TR::Register *scratchReg = NULL;

    TR_OutlinedInstructions *outlinedHelperCall = NULL;

    TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *fallThruLabel = generateLabelSymbol(cg);
    startLabel->setStartInternalControlFlow();
    fallThruLabel->setEndInternalControlFlow();

    J9Class *castClassLeafJ9Class = (J9Class *)cg->fe()->getLeafComponentClassFromArrayClass(clazz);
    SVM_ASSERT_NONFATAL(castClassLeafJ9Class, "Could not find j9leaf class from array class, %p", clazz);

    TR_OpaqueClassBlock *castClassLeafClass = TR::Compiler->cls.convertClassPtrToClassOffset(castClassLeafJ9Class);

    static char *breakOnInlineFinalArrayCastClass = feGetEnv("TR_BreakOnInlineFinalArrayCastClass");
    if (breakOnInlineFinalArrayCastClass)
        Inst(OP::INT3, node, cg);

    Inst_Label(OP::label, node, startLabel, cg);

    if (!isCheckCast) {
        Inst_RegReg(OP::XOR4RegReg, node, resultReg, resultReg, cg);
    }

    // -----------------------------------------------------------------------
    // If the object is NULL, no exception is thrown for a checkcast and a 0
    // is returned for an instanceof.
    //
    // A NULL class passed to Class.isAssignableFrom() will be handled by a
    // NULLCHK node inserted before this call node.
    // -----------------------------------------------------------------------

    if (!objectNode->isNonNull()) {
        Inst_RegReg(OP::TEST8RegReg, node, objectReg, objectReg, cg);

        // checkcast leaves the operand stack unaffected
        // instanceof returns 0 if the objectRef is null
        //
        Inst_Label(OP::JE4, node, fallThruLabel, cg);
    }

    // -----------------------------------------------------------------------
    // For a cast class array with a final leaf component class, perform a
    // trivial check whether objectClass is the same as the castClass.
    // -----------------------------------------------------------------------

    /*
     * A quick note on relocation type. We use TR_ClassPointer, rather than TR_ClassAddress
     * or TR_SymbolFromManager for the relocation types, we use TR_ClassPointer since
     * the addMetaData* methods do not fully support those two relo types. In the future,
     * after that support is added, the relocation type should be changed to TR_ClassAddress
     */

    generateLoadJ9Class(node, objectClassReg, objectReg, cg);
    uintptr_t clazzAddress = (uintptr_t)clazz;

    if (IS_32BIT_SIGNED(clazzAddress) && !comp->compileRelocatableCode()) {
        Inst_RegImm(OP::CMP8RegImm4, node, objectClassReg, (int32_t)clazzAddress, cg);
    } else {
        scratchReg = cg->allocateRegister();
        Inst_RegImm64(OP::MOV8RegImm64, node, scratchReg, clazzAddress, cg, TR_ClassPointer);
        Inst_RegReg(OP::CMP8RegReg, node, objectClassReg, scratchReg, cg);
    }

    if (isCheckCast) {
        // Unsuccessful checkcast cast jumps directly to the OOL helper to
        // throw the CastClassException
        //
        TR::LabelSymbol *outlinedHelperCallLabel = generateLabelSymbol(cg);
        outlinedHelperCall = new (cg->trHeapMemory())
            TR_OutlinedInstructions(node, TR::call, NULL, outlinedHelperCallLabel, fallThruLabel, cg);
        cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);

        Inst_Label(OP::JNE4, node, outlinedHelperCallLabel, cg);
    } else {
        Inst_Reg(OP::SETE1Reg, node, resultReg, cg);
    }

    // ----------------------------------------------------------------------
    // Collect register dependencies for fallThruLabel
    // ----------------------------------------------------------------------

    // clang-format off
    int32_t numRegDeps =
          1  // objectReg
        + ((objectReg != objectClassReg) ? 1 : 0)
        + (outlinedHelperCall ? 2 : 0)  // 2 helper args: objectRef + castClass
        + (resultReg ? 1 : 0)
        + (scratchReg ? 1 : 0);
    // clang-format on

    TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, numRegDeps, cg);

    deps->addPostCondition(objectReg, TR::RealRegister::NoReg, cg);

    if (objectReg != objectClassReg)
        deps->addPostCondition(objectClassReg, TR::RealRegister::NoReg, cg);

    if (resultReg)
        deps->addPostCondition(resultReg, TR::RealRegister::NoReg, cg);

    if (scratchReg)
        deps->addPostCondition(scratchReg, TR::RealRegister::NoReg, cg);

    if (outlinedHelperCall) {
        TR::Node *callNode = outlinedHelperCall->getCallNode();
        TR::Register *reg;

        if (callNode->getFirstChild() == node->getFirstChild()) {
            reg = callNode->getFirstChild()->getRegister();
            if (reg)
                deps->unionPostCondition(reg, TR::RealRegister::NoReg, cg);
        }

        if (callNode->getSecondChild() == node->getSecondChild()) {
            reg = callNode->getSecondChild()->getRegister();
            if (reg)
                deps->unionPostCondition(reg, TR::RealRegister::NoReg, cg);
        }
    }

    deps->stopAddingConditions();
    Inst_Label(OP::label, node, fallThruLabel, deps, cg);

    if (scratchReg)
        cg->stopUsingRegister(scratchReg);

    if (objectReg != objectClassReg)
        cg->stopUsingRegister(objectClassReg);

    cg->decReferenceCount(objectNode);
    cg->decReferenceCount(castClassNode);

    if (!isCheckCast) {
        node->setRegister(resultReg);
    }

    return;
}

/**
 * @brief
 *    Generate instructions to update the castClassCache field of a J9Class object.
 *    Handles class addresses of different sizes.
 *
 * @param[in] objectClassReg : register containing the destination object class
 * @param[in] clazzAddress : the class address to update in the cache
 * @param[in] use64BitClasses : whether 64 bit classes are active
 * @param[in,out] scratchReg : a secondary scratch register allocated if needed in this function
 * @param[in] node : the cast check TR::Node
 * @param[in] cg : the CodeGenerator object
 */
static void generateCastClassCacheUpdate(TR::Register *objectClassReg, uintptr_t clazzAddress, bool use64BitClasses,
    TR::Register *&scratchReg, TR::Node *node, TR::CodeGenerator *cg)
{
    static const char *dontUpdateCastClassCache = feGetEnv("TR_DontUpdateCastClassCache");
    if (dontUpdateCastClassCache)
        return;

    TR::MemoryReference *castClassMR = MRef_Bdisp32(objectClassReg, offsetof(J9Class, castClassCache), cg);

    bool use32Bit = (!cg->comp()->compileRelocatableCode())
        && (!use64BitClasses || (use64BitClasses && IS_32BIT_SIGNED(clazzAddress)));

    if (use32Bit) {
        Inst_MemImm(OP::S8MemImm4, node, castClassMR, (int32_t)clazzAddress, cg);
    } else {
        if (!scratchReg)
            scratchReg = cg->allocateRegister();
        Inst_RegImm64(OP::MOV8RegImm64, node, scratchReg, clazzAddress, cg, TR_ClassPointer);
        Inst_MemReg(OP::S8MemReg, node, castClassMR, scratchReg, cg);
    }
}

/**
 * @brief Inline a checkcast, instanceof, or Class.isAssignableFrom() when the
 *     cast class is an array known at compile-time
 *
 * @details
 *
 * This case inlines the checkcast/instanceof/isAssignableFrom() sequence
 * implemented by the VM in `inlineCheckCast()` in VMHelpers.hpp, but
 * specializes it for when the cast class is an array known at compile-time.
 *
 * It implements the following logic:
 *
 * if (objectRef == NULL) {
 *     result:
 *         instanceof/isAssignableFrom : 0
 *         checkcast : no action
 * }
 *
 * objectClass = objectRef.class
 *
 * if (objectClass == castClass) {
 *     result:
 *         instanceof/isAssignableFrom : 1
 *         checkcast : no action
 * }
 *
 * if (castClass is found in objectClass->castClassCache) {
 *     result:
 *         instanceof/isAssignableFrom : 1 if castable; 0 otherwise
 *         checkcast : no action if castable; throw via OOL helper otherwise
 * }
 *
 * if (castClass->leaf is a non-primitive class) {
 *
 *     if (objectClass is not an array) {
 *         goto notCastableUpdateCache
 *     }
 *
 *     if (objectClass->arity != castClass->arity) {
 *         handle all cast checks OOL
 *     }
 *
 * #ifdef J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES
 *     if (objectClass is not null restricted && castClass is null restricted) {
 *         goto notCastableUpdateCache
 *     }
 * #endif
 *
 *     if (objectClass->leaf is not a mixed class) {
 *         goto notCastableUpdateCache
 *     }
 *
 *     if (objectClass->leaf == castClass->leaf) {
 *         goto castableAndUpdateCache
 *     }
 *
 *     if (castClass->leaf is final) {
 *         if ((objectClass->leaf->depth <= castClass->leaf->depth) {
 *             if (castClass->leaf is an interface) {
 *                 handle all cast checks OOL
 *             } else {
 *                 goto notCastableUpdateCache
 *             }
 *         }
 *
 *         if (objectClass->leaf->superclasses[castClass->leaf->depth] == castClass->leaf)) {
 *             goto castableAndUpdateCache
 *         }
 *     }
 * }
 *
 * notCastableUpdateCache:
 *     update objectClass->castClassCache
 *     result:
 *         instanceof/isAssignableFrom : 0
 *         checkcast : throw OOL
 *
 * castableAndUpdateCache:
 *     update objectClass->castClassCache
 *     result:
 *         instanceof/isAssignableFrom : 1
 *         checkcast : no action
 *
 * @param[in] node : \c TR::Node being evaluated
 * @param[in] clazz : compile-time \c J9Class address
 * @param[in] isCheckCast : true if a checkcast operation; false otherwise
 * @param[in] cg : \c CodeGenerator object
 */
static void inlineCheckCastOrInstanceOfKnownArrayCastClass(TR::Node *node, TR_OpaqueClassBlock *clazz, bool isCheckCast,
    TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
    bool isAssignableFrom = (node->getOpCodeValue() == TR::icall);

    logprintf(comp->getOption(TR_TraceCG), comp->log(), "Inline %s for const cast class array: node=%p, castClass=%p\n",
        isCheckCast ? "checkcast" : (isAssignableFrom ? "isAssignableFrom" : "instanceof"), node, clazz);

    const int32_t CAST_CLASS_CACHE_MASK = ~1;
    const int32_t CAST_CLASS_CACHE_UNCASTABLE = 1;

    TR::Node *objectNode = node->getFirstChild();
    TR::Node *castClassNode = node->getSecondChild();
    TR::Register *objectReg = cg->evaluate(objectNode);

    // The first child of a call to isAssignableFrom is already the class object
    //
    TR::Register *objectClassReg = isAssignableFrom ? objectReg : cg->allocateRegister();
    TR::Register *resultReg = isCheckCast ? NULL : cg->allocateRegister();
    TR::Register *scratchReg = NULL;
    TR::Register *scratchReg2 = NULL;
    TR::Register *scratchReg3 = NULL;

    bool use64BitClasses = !TR::Compiler->om.generateCompressedObjectHeaders() || comp->compileRelocatableCode();

    TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *fallThruLabel = generateLabelSymbol(cg);
    startLabel->setStartInternalControlFlow();
    fallThruLabel->setEndInternalControlFlow();

    TR::LabelSymbol *oolHelperCallTrampolineLabel = isCheckCast ? generateLabelSymbol(cg) : NULL;
    TR_OutlinedInstructions *outlinedHelperCall = NULL;

    J9Class *castClassLeafJ9Class = (J9Class *)fej9->getLeafComponentClassFromArrayClass((TR_OpaqueClassBlock *)clazz);
    SVM_ASSERT_NONFATAL(castClassLeafJ9Class, "Could not find j9leaf class from array class, %p", clazz);

    TR_OpaqueClassBlock *castClassLeafClass = TR::Compiler->cls.convertClassPtrToClassOffset(castClassLeafJ9Class);

    static char *breakOnInlineArrayCastClass = feGetEnv("TR_BreakOnInlineArrayCastClass");
    if (breakOnInlineArrayCastClass)
        Inst(OP::INT3, node, cg);

    TR::LabelSymbol *castableDoNotCacheLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *castableAndUpdateCacheLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *notCastableDoNotCacheLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *notCastableUpdateCacheLabel = generateLabelSymbol(cg);

    Inst_Label(OP::label, node, startLabel, cg);

    // -----------------------------------------------------------------------
    // If the object is NULL, no exception is thrown for a checkcast and a 0
    // is returned for an instanceof.
    //
    // A NULL class passed to Class.isAssignableFrom() will be handled by a
    // NULLCHK node inserted before this call node.
    // -----------------------------------------------------------------------

    if (!objectNode->isNonNull()) {
        Inst_RegReg(OP::TEST8RegReg, node, objectReg, objectReg, cg);

        // checkcast leaves the operand stack unaffected
        // instanceof returns 0 if the objectRef is null
        //
        TR::LabelSymbol *nullTargetLabel = isCheckCast ? fallThruLabel : notCastableDoNotCacheLabel;
        Inst_Label(OP::JE4, node, nullTargetLabel, cg);
    }

    // -----------------------------------------------------------------------
    // Perform trivial check whether objectClass is the same as the castClass.
    // The castClass is known to be an array (all array classes are implicitly
    // final), so no subclass test is needed.
    //
    // If the trivial check reveals a successful cast, do not cache the
    // result to avoiding polluting the cast class cache.
    // -----------------------------------------------------------------------

    generateLoadJ9Class(node, objectClassReg, objectReg, cg);
    uintptr_t clazzAddress = (uintptr_t)clazz;

    if (IS_32BIT_SIGNED(clazzAddress) && !comp->compileRelocatableCode()) {
        Inst_RegImm(OP::CMP8RegImm4, node, objectClassReg, (int32_t)clazzAddress, cg);
    } else {
        scratchReg = cg->allocateRegister();
        Inst_RegImm64(OP::MOV8RegImm64, node, scratchReg, clazzAddress, cg, TR_ClassPointer);
        Inst_RegReg(OP::CMP8RegReg, node, objectClassReg, scratchReg, cg);
    }

    Inst_Label(OP::JE4, node, castableDoNotCacheLabel, cg);

    // ----------------------------------------------------------------------
    // Next, check for a hit in the object's classCastCache
    // ----------------------------------------------------------------------

    if (!scratchReg)
        scratchReg = cg->allocateRegister();

    Inst_RegMem(OP::L8RegMem, node, scratchReg, MRef_Bdisp32(objectClassReg, offsetof(J9Class, castClassCache), cg),
        cg);

    if (use64BitClasses) {
        if (IS_32BIT_SIGNED(clazzAddress) && !comp->compileRelocatableCode()) {
            Inst_RegImm(OP::XOR8RegImm4, node, scratchReg, (int32_t)clazzAddress, cg);
        } else {
            if (!scratchReg2)
                scratchReg2 = cg->allocateRegister();
            Inst_RegImm64(OP::MOV8RegImm64, node, scratchReg2, clazzAddress, cg, TR_ClassPointer);
            Inst_RegReg(OP::XOR8RegReg, node, scratchReg, scratchReg2, cg);
        }
    } else {
        Inst_RegImm(OP::XOR4RegImm4, node, scratchReg, (int32_t)clazzAddress, cg);
    }

    Inst_RegImm(OP::TEST8RegImm4, node, scratchReg, CAST_CLASS_CACHE_MASK, cg);

    TR::LabelSymbol *castClassCacheMissLabel = generateLabelSymbol(cg);
    Inst_Label(OP::JNE4, node, castClassCacheMissLabel, cg);

    // ----------------------------------------------------------------------
    // objectClass was found in the cache. Determine whether it was castable
    // or not and exit appropriately.
    // ----------------------------------------------------------------------

    Inst_RegImm(OP::TEST8RegImm4, node, scratchReg, CAST_CLASS_CACHE_UNCASTABLE, cg);
    Inst_Label(OP::JE4, node, castableDoNotCacheLabel, cg);

    // ----------------------------------------------------------------------
    // If the cast class leaf component is not a reference array, the result
    // is not castable. Fall through to update the cache.
    // ----------------------------------------------------------------------
    if (fej9->isClassMixed((TR_OpaqueClassBlock *)castClassLeafJ9Class)) {
        // The JMP is required on this path to complete the above cache check
        //
        Inst_Label(OP::JMP4, node, notCastableDoNotCacheLabel, cg);
        Inst_Label(OP::label, node, castClassCacheMissLabel, cg);

        // ----------------------------------------------------------------------
        // Check if objectClass is an array. Not castable if it is not.
        // ----------------------------------------------------------------------

        Inst_MemImm(IS_8BIT_SIGNED(J9AccClassRAMArray) ? OP::TEST1MemImm1 : OP::TEST4MemImm4, node,
            MRef_Bdisp32(objectClassReg, offsetof(J9Class, classDepthAndFlags), cg), J9AccClassRAMArray, cg);
        Inst_Label(OP::JE4, node, notCastableUpdateCacheLabel, cg);

        // ----------------------------------------------------------------------
        // For an array objectClass, if objectClass->arity != castClass->arity
        // then perform cast check in helper
        // ----------------------------------------------------------------------

        Inst_RegMem(OP::L8RegMem, node, scratchReg, MRef_Bdisp32(objectClassReg, offsetof(J9ArrayClass, arity), cg),
            cg);

        UDATA arity = static_cast<TR_J9VM *>(fej9)->getArityFromArrayClass((TR_OpaqueClassBlock *)clazz);
        Inst_RegImm(OP::CMP8RegImm4, node, scratchReg, arity, cg);

        if (!oolHelperCallTrampolineLabel)
            oolHelperCallTrampolineLabel = generateLabelSymbol(cg);
        Inst_Label(OP::JNE4, node, oolHelperCallTrampolineLabel, cg);

#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
        J9Class *castClassJ9Class = TR::Compiler->cls.convertClassOffsetToClassPtr(clazz);
        if (TR::Compiler->cls.isArrayNullRestricted(cg->comp(), (TR_OpaqueClassBlock *)castClassJ9Class)) {
            static_assert(J9ClassArrayIsNullRestricted == 0x2000000,
                "J9ClassArrayIsNullRestricted must be 0x2000000 for simple bit test");

            Inst_MemImm(IS_8BIT_SIGNED(J9ClassArrayIsNullRestricted) ? OP::TEST1MemImm1 : OP::TEST4MemImm4, node,
                MRef_Bdisp32(objectClassReg, offsetof(J9Class, classFlags), cg), J9ClassArrayIsNullRestricted, cg);

            // Fail, since a nullable array class cannot be cast to a null-restricted class
            //
            Inst_Label(OP::JE4, node, notCastableUpdateCacheLabel, cg);
        }
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */

        // Alias for code readability
        //
        TR::Register *&objectClassLeafReg = scratchReg2;

        if (!objectClassLeafReg)
            objectClassLeafReg = cg->allocateRegister();

        Inst_RegMem(OP::L8RegMem, node, objectClassLeafReg,
            MRef_Bdisp32(objectClassReg, offsetof(J9ArrayClass, leafComponentType), cg), cg);

        // ----------------------------------------------------------------------
        // If objectClassLeaf is not a mixed object (reference) then
        // notCastableUpdateCache
        // ----------------------------------------------------------------------

        Inst_RegMem(OP::L8RegMem, node, scratchReg,
            MRef_Bdisp32(objectClassLeafReg, offsetof(J9Class, classDepthAndFlags), cg), cg);
        Inst_RegImm(OP::AND8RegImm4, node, scratchReg, (OBJECT_HEADER_SHAPE_MASK << J9AccClassRAMShapeShift), cg);
        Inst_RegImm(OP::CMP8RegImm4, node, scratchReg, (OBJECT_HEADER_SHAPE_MIXED << J9AccClassRAMShapeShift), cg);
        Inst_Label(OP::JNE4, node, notCastableUpdateCacheLabel, cg);

        // ----------------------------------------------------------------------
        // If objectClassLeafClass == castClassLeafClass then
        // castableAndUpdateCache
        // ----------------------------------------------------------------------

        uintptr_t componentClazzAddress = (uintptr_t)castClassLeafJ9Class;

        if (IS_32BIT_SIGNED(componentClazzAddress) && !comp->compileRelocatableCode()) {
            Inst_RegImm(OP::CMP8RegImm4, node, objectClassLeafReg, (int32_t)componentClazzAddress, cg);
        } else {
            Inst_RegImm64(OP::MOV8RegImm64, node, scratchReg, componentClazzAddress, cg, TR_ClassPointer);
            Inst_RegReg(OP::CMP8RegReg, node, objectClassLeafReg, scratchReg, cg);
        }

        Inst_Label(OP::JE4, node, castableAndUpdateCacheLabel, cg);

        // ----------------------------------------------------------------------
        // Skip the subclass check if the castClassLeaf is final
        // ----------------------------------------------------------------------

        bool castClassLeafIsInterface
            = J9ROMCLASS_IS_INTERFACE(TR::Compiler->cls.romClassOf((TR_OpaqueClassBlock *)castClassLeafJ9Class));

        if (!fej9->isClassFinal(castClassLeafClass)) {
            // ----------------------------------------------------------------------
            // Is objectClassLeaf a subclass of castClassLeaf?
            // ----------------------------------------------------------------------

            uintptr_t castClassLeafDepth = TR::Compiler->cls.classDepthOf(castClassLeafClass);

            static_assert(J9AccClassDepthMask == 0xffff, "J9AccClassDepthMask must be 0xffff");
            TR::MemoryReference *objectClassLeafDepthMR
                = MRef_Bdisp32(objectClassLeafReg, offsetof(J9Class, classDepthAndFlags), cg);
            Inst_MemImm(OP::CMP2MemImm2, node, objectClassLeafDepthMR, castClassLeafDepth, cg);

            if (castClassLeafIsInterface) {
                // Too complex for inline; perform cast check in helper.
                // Issue #23616 tracks the inlining of interface arrays.
                //
                if (!oolHelperCallTrampolineLabel)
                    oolHelperCallTrampolineLabel = generateLabelSymbol(cg);
                Inst_Label(OP::JBE4, node, oolHelperCallTrampolineLabel, cg);
            } else {
                TR_ASSERT_FATAL(!TR::Compiler->cls.isClassArray(comp, castClassLeafClass),
                    "Expected cast class leaf component to be non-array");
                Inst_Label(OP::JBE4, node, notCastableUpdateCacheLabel, cg);
            }

            Inst_RegMem(OP::L8RegMem, node, scratchReg,
                MRef_Bdisp32(objectClassLeafReg, offsetof(J9Class, superclasses), cg), cg);
            auto offset = castClassLeafDepth * sizeof(J9Class *);
            TR_ASSERT_FATAL(IS_32BIT_SIGNED(offset), "superclass array offset is unreasonably large");

            TR::MemoryReference *superclassMR2 = MRef_Bdisp32(scratchReg, offset, cg);
            if (use64BitClasses) {
                if (IS_32BIT_SIGNED(componentClazzAddress) && !comp->compileRelocatableCode()) {
                    Inst_MemImm(OP::CMP8MemImm4, node, superclassMR2, (int32_t)componentClazzAddress, cg);
                } else {
                    if (!scratchReg3)
                        scratchReg3 = cg->allocateRegister();

                    Inst_RegImm64(OP::MOV8RegImm64, node, scratchReg3, componentClazzAddress, cg, TR_ClassPointer);
                    Inst_MemReg(OP::CMP8MemReg, node, superclassMR2, scratchReg3, cg);
                }
            } else {
                Inst_MemImm(OP::CMP4MemImm4, node, superclassMR2, (int32_t)componentClazzAddress, cg);
            }

            Inst_Label(OP::JE4, node, castableAndUpdateCacheLabel, cg);
        }

        if (castClassLeafIsInterface) {
            if (!oolHelperCallTrampolineLabel)
                oolHelperCallTrampolineLabel = generateLabelSymbol(cg);
            Inst_Label(OP::JMP4, node, oolHelperCallTrampolineLabel, cg);
        } else {
            TR_ASSERT_FATAL(!TR::Compiler->cls.isClassArray(comp, castClassLeafClass),
                "Expected cast class leaf component to be non-array");
        }

        // Generated code will fall through to notCastableUpdateCacheLabel

    } else {
        Inst_Label(OP::label, node, castClassCacheMissLabel, cg);
    }

    Inst_Label(OP::label, node, notCastableUpdateCacheLabel, cg);
    generateCastClassCacheUpdate(objectClassReg, clazzAddress | 1, use64BitClasses, scratchReg, node, cg);

    Inst_Label(OP::label, node, notCastableDoNotCacheLabel, cg);

    if (!isCheckCast) {
        Inst_RegReg(OP::XOR4RegReg, node, resultReg, resultReg, cg);
        Inst_Label(OP::JMP4, node, fallThruLabel, cg);
    } else {
        // The out-of-line helper will throw the CastClassException
        //
        TR_ASSERT_FATAL(oolHelperCallTrampolineLabel, "checkcast requires an OOL label");
    }

    if (oolHelperCallTrampolineLabel) {
        TR::LabelSymbol *outlinedHelperCallLabel = generateLabelSymbol(cg);
        outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, isCheckCast ? TR::call : TR::icall,
            resultReg, outlinedHelperCallLabel, fallThruLabel, cg);
        cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);

        // This code sequence has been designed so that all branches to the out of line helper
        // call go through a single jump.  In practice, this has little impact on performance
        // (other than code size).
        //
        // Although it would be better if instructions branched directly to the out-of-line
        // helper call, the use of this trampoline is required when there are multiple branches
        // to the same out-of-line sequence due to unspecified behaviours with the out-of-line
        // register assigner.  The specific issues with multiple branches to the same OOL sequence
        // have not been fully determined, but until they are fully understood and that OOL
        // design is reworked, use a single branch to reach each unique OOL helper call.
        //
        Inst_Label(OP::label, node, oolHelperCallTrampolineLabel, cg);
        Inst_Label(OP::JMP4, node, outlinedHelperCallLabel, cg);
    }

    Inst_Label(OP::label, node, castableAndUpdateCacheLabel, cg);
    generateCastClassCacheUpdate(objectClassReg, clazzAddress, use64BitClasses, scratchReg, node, cg);

    Inst_Label(OP::label, node, castableDoNotCacheLabel, cg);

    if (!isCheckCast) {
        Inst_RegImm(OP::MOV4RegImm4, node, resultReg, 1, cg);
    }

    // ----------------------------------------------------------------------
    // Collect register dependencies for fallThruLabel
    // ----------------------------------------------------------------------

    // clang-format off
    int32_t numRegDeps =
          1 + // objectReg
        + ((objectReg != objectClassReg) ? 1 : 0)
        + (outlinedHelperCall ? 2 : 0)  // 2 helper args: objectRef + castClass
        + (resultReg ? 1 : 0)
        + (scratchReg ? 1 : 0)
        + (scratchReg2 ? 1 : 0)
        + (scratchReg3 ? 1 : 0);
    // clang-format on

    TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, numRegDeps, cg);

    deps->addPostCondition(objectReg, TR::RealRegister::NoReg, cg);

    if (objectReg != objectClassReg)
        deps->addPostCondition(objectClassReg, TR::RealRegister::NoReg, cg);

    if (resultReg)
        deps->addPostCondition(resultReg, TR::RealRegister::NoReg, cg);

    if (scratchReg)
        deps->addPostCondition(scratchReg, TR::RealRegister::NoReg, cg);

    if (scratchReg2)
        deps->addPostCondition(scratchReg2, TR::RealRegister::NoReg, cg);

    if (scratchReg3)
        deps->addPostCondition(scratchReg3, TR::RealRegister::NoReg, cg);

    if (outlinedHelperCall) {
        TR::Node *callNode = outlinedHelperCall->getCallNode();
        TR::Register *reg;

        if (callNode->getFirstChild() == node->getFirstChild()) {
            reg = callNode->getFirstChild()->getRegister();
            if (reg)
                deps->unionPostCondition(reg, TR::RealRegister::NoReg, cg);
        }

        if (callNode->getSecondChild() == node->getSecondChild()) {
            reg = callNode->getSecondChild()->getRegister();
            if (reg)
                deps->unionPostCondition(reg, TR::RealRegister::NoReg, cg);
        }
    }

    deps->stopAddingConditions();
    Inst_Label(OP::label, node, fallThruLabel, deps, cg);

    if (scratchReg)
        cg->stopUsingRegister(scratchReg);

    if (scratchReg2)
        cg->stopUsingRegister(scratchReg2);

    if (scratchReg3)
        cg->stopUsingRegister(scratchReg3);

    if (objectReg != objectClassReg)
        cg->stopUsingRegister(objectClassReg);

    cg->decReferenceCount(objectNode);
    cg->decReferenceCount(castClassNode);

    if (!isCheckCast) {
        node->setRegister(resultReg);
    }

    return;
}

static void generateInlinedCheckCastOrInstanceOfForArrayClass(TR::Node *node, TR_OpaqueClassBlock *clazz,
    bool isCheckCast, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

    static char *disableInlineObjectArrayCastClass = feGetEnv("TR_DisableInlineObjectArrayCastClass");
    static char *disableInlineFinalArrayCastClass = feGetEnv("TR_DisableInlineFinalArrayClass");
    static char *disableInlineKnownArrayCastClass = feGetEnv("TR_DisableInlineKnownArrayCastClass");

    if (clazz && TR::Compiler->cls.isClassArray(comp, clazz)) {
        TR_OpaqueClassBlock *castClassComponentClass = fej9->getComponentClassFromArrayClass(clazz);

        if (!disableInlineObjectArrayCastClass && fej9->isJavaLangObject(castClassComponentClass)) {
            inlineCheckCastOrInstanceOfObjectArrayCastClass(node, clazz, isCheckCast, cg);
            return;
        } else {
            bool perform = comp->target().is64Bit()
                && (!comp->compileRelocatableCode() || comp->getOption(TR_UseSymbolValidationManager));

            if (perform) {
                /**
                 * The only reason this is disabled for relocatable compiles (AOT and out-of-process
                 * compiles like JitServer) is because the support has not been implemented yet.
                 * Implementing the code relocations and appropriate frontend queries can be done,
                 * but is beyond the scope of this initial implementation. The work is tracked
                 * in Issue #23510.
                 */
                TR_OpaqueClassBlock *castClassLeafClass = fej9->getLeafComponentClassFromArrayClass(clazz);

                if (!castClassLeafClass) {
                    return;
                } else if (!disableInlineFinalArrayCastClass && fej9->isClassFinal(castClassLeafClass)) {
                    inlineCheckCastOrInstanceOfFinalArrayCastClass(node, clazz, isCheckCast, cg);
                    return;
                } else if (!disableInlineKnownArrayCastClass) {
                    inlineCheckCastOrInstanceOfKnownArrayCastClass(node, clazz, isCheckCast, cg);
                    return;
                }
            }
        }
    }

    if (node->getOpCodeValue() == TR::checkcastAndNULLCHK) {
        auto object = cg->evaluate(node->getChild(0));
        // Just touch the memory in case this is a NULL pointer and we need to throw
        // the exception after the checkcast. If the checkcast was combined with nullpointer
        // there's nobody after the checkcast to throw the exception.
        auto instr = Inst_MemImm(OP::TEST1MemImm1, node,
            MRef_Bdisp32(object, TR::Compiler->om.offsetOfObjectVftField(), cg), 0, cg);
        cg->setImplicitExceptionPoint(instr);
        instr->setNeedsGCMap(0xFF00FFFF);
        instr->setNode(comp->findNullChkInfo(node));
    }

    TR::TreeEvaluator::performHelperCall(node, NULL, isCheckCast ? TR::call : TR::icall, false, cg);
}

/**
 * @brief Generate instructions to perform an interface table walk to search
 *        for a given cast class.  Handles both checkcast and instanceof.
 *
 * @param[in] node : \c TR::Node of the current check node
 * @param[in] cg : \c TR::CodeGenerator object
 * @param[in] iTableLookUpFailLabel : \c TR::LabelSymbol to handle the case where the cast class
 *               is not found during the itable walk
 * @param[in] castClass : \c J9::Class cast class address
 * @param[in] castClassReg : \c TR::Register scratch register to hold large cast class address,
 *               if necessary.  NULL means a scratch register is not required for the cast class.
 * @param[in] itableReg : \c TR::Register to use during itable walk
 */
static void inlineInterfaceLookup(TR::Node *node, TR::CodeGenerator *cg, TR::LabelSymbol *iTableLookUpFailLabel,
    TR_OpaqueClassBlock *castClass, TR::Register *castClassReg, TR::Register *itableReg)
{
    TR::LabelSymbol *iTableLoopLabel = generateLabelSymbol(cg);

    if (castClassReg) {
        Inst_RegImm64(OP::MOV8RegImm64, node, castClassReg, reinterpret_cast<uintptr_t>(castClass), cg,
            TR_ClassAddress);
    }

    // Loop through I-Table
    Inst_Label(OP::label, node, iTableLoopLabel, cg);
    Inst_RegReg(OP::TESTRegReg(), node, itableReg, itableReg, cg);
    Inst_Label(OP::JE4, node, iTableLookUpFailLabel, cg);
    auto interfaceMR = MRef_Bdisp32(itableReg, offsetof(J9ITable, interfaceClass), cg);
    if (castClassReg) {
        Inst_MemReg(OP::CMP8MemReg, node, interfaceMR, castClassReg, cg);
    } else {
        Inst_MemImmSym(OP::CMP4MemImm4, node, interfaceMR, reinterpret_cast<uintptr_t>(castClass),
            node->getChild(1)->getSymbolReference(), cg);
    }
    Inst_RegMem(OP::LRegMem(), node, itableReg, MRef_Bdisp32(itableReg, offsetof(J9ITable, next), cg), cg);
    Inst_Label(OP::JNE4, node, iTableLoopLabel, cg);

    // If the loop does not iterate then a match was found
}

inline void generateInlinedCheckCastOrInstanceOfForInterface(TR::Node *node, TR_OpaqueClassBlock *castClass,
    TR::CodeGenerator *cg, bool isCheckCast)
{
    TR::Compilation *comp = cg->comp();
    TR_ASSERT(castClass && TR::Compiler->cls.isInterfaceClass(comp, castClass), "Not a compile-time known Interface.");

    bool use64BitClasses = comp->target().is64Bit()
        && (!TR::Compiler->om.generateCompressedObjectHeaders()
            || (comp->compileRelocatableCode() && comp->getOption(TR_UseSymbolValidationManager)));

    // When running 64 bit compressed refs, if castClass is an address above the 2G boundary then we can't use
    // a push 32bit immediate instruction to pass it on the stack to the jitThrowClassCastException helper
    // as the address gets sign extended. It needs to be stored in a temp register and then push the
    // register to the stack.
    bool highClass = (comp->target().is64Bit() && ((uintptr_t)castClass) > INT_MAX);

    TR::Register *instanceClassReg = cg->allocateRegister();
    TR::Register *castClassReg = (use64BitClasses || highClass) ? cg->allocateRegister() : NULL;

    // Profiled call site cache
    uint8_t numSuccessfulClassChecks = 0;
    uintptr_t guessClass = 0;
    if (!comp->compileRelocatableCode()) {
        TR_OpaqueClassBlock *guessClassArray[NUM_PICS];
        auto num_PICs = TR::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastInfo(cg, node, guessClassArray);
        auto fej9 = static_cast<TR_J9VMBase *>(comp->fe());
        for (uint8_t i = 0; i < num_PICs; i++) {
            if (fej9->instanceOfOrCheckCastNoCacheUpdate((J9Class *)guessClassArray[i], (J9Class *)castClass)) {
                guessClass = reinterpret_cast<uintptr_t>(guessClassArray[i]);
                numSuccessfulClassChecks++;
            }
        }
    }

    /**
     * Only cache the last successful check if profiling strongly suggests this check
     * is monomorphic.  If profiling has seen more than one successful class check then
     * using a cache could cause performance problems as multiple threads compete to
     * update the single cached class.  If no successful class checks have been seen
     * before then do not speculate that a cache would be helpful.
     */
    bool doClassCache = (numSuccessfulClassChecks == 1) ? true : false;

    static bool disableInterfaceCastCache = feGetEnv("TR_forceDisableInterfaceCastCache") != NULL;
    static bool enableInterfaceCastCache = feGetEnv("TR_forceEnableInterfaceCastCache") != NULL;

    TR_ASSERT_FATAL(!(disableInterfaceCastCache && enableInterfaceCastCache),
        "checkcast interface cast cache cannot be both explicitly enabled and disabled");

    doClassCache = !disableInterfaceCastCache && (enableInterfaceCastCache || doClassCache);

    /**
     * A scratch register is required for ClassCastException throws when
     * a cache is not being used for checkcasts.
     */
    TR::Register *scratchReg = (!doClassCache && isCheckCast) ? cg->allocateRegister() : NULL;

    uint8_t numDeps = 1 + (castClassReg != NULL) + (scratchReg != NULL);
    auto deps = RegDeps(numDeps, numDeps, cg);
    deps->addPreCondition(instanceClassReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(instanceClassReg, TR::RealRegister::NoReg, cg);
    if (castClassReg) {
        deps->addPreCondition(castClassReg, TR::RealRegister::NoReg, cg);
        deps->addPostCondition(castClassReg, TR::RealRegister::NoReg, cg);
    }
    if (scratchReg) {
        deps->addPreCondition(scratchReg, TR::RealRegister::NoReg, cg);
        deps->addPostCondition(scratchReg, TR::RealRegister::NoReg, cg);
    }
    deps->stopAddingConditions();

    TR::LabelSymbol *begLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
    begLabel->setStartInternalControlFlow();
    endLabel->setEndInternalControlFlow();

    TR::LabelSymbol *iTableLookUpPathLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *iTableLookUpFailLabel = generateLabelSymbol(cg);

    Inst_RegReg(OP::MOVRegReg(), node, instanceClassReg, node->getChild(0)->getRegister(), cg);
    Inst_Label(OP::label, node, begLabel, cg);

    // Null test
    if (!node->getChild(0)->isNonNull() && node->getOpCodeValue() != TR::checkcastAndNULLCHK) {
        // instanceClassReg contains the object at this point, reusing the register as object is no longer used after
        // this point.
        Inst_RegReg(OP::TESTRegReg(), node, instanceClassReg, instanceClassReg, cg);
        Inst_Label(OP::JE4, node, endLabel, cg);
    }

    // Load J9Class
    generateLoadJ9Class(node, instanceClassReg, instanceClassReg, cg);

    if (doClassCache) {
        // Call site cache
        auto cache = sizeof(J9Class *) == 4 ? cg->create4ByteData(node, (uint32_t)guessClass)
                                            : cg->create8ByteData(node, (uint64_t)guessClass);
        cache->setClassAddress(true);
        Inst_RegMem(OP::CMPRegMem(use64BitClasses), node, instanceClassReg, MRef_const(cache, cg), cg);
        Inst_Label(OP::JNE4, node, iTableLookUpPathLabel, cg);

        // I-Table lookup out-of-line
        {
            TR_OutlinedInstructionsGenerator og(iTableLookUpPathLabel, node, cg);

            // Preserve the instance class before the register is reused
            Inst_Reg(OP::PUSHReg, node, instanceClassReg, cg);

            // Save VFP
            auto vfp = Inst_VFPSave(node, cg);

            // Obtain I-Table
            // Re-use the instanceClassReg register to perform itable lookup
            Inst_RegMem(OP::LRegMem(), node, instanceClassReg,
                MRef_Bdisp32(instanceClassReg, offsetof(J9Class, iTable), cg), cg);

            inlineInterfaceLookup(node, cg, iTableLookUpFailLabel, castClass, castClassReg, instanceClassReg);

            // Found from I-Table

            /**
             * The original implementation of this interface cast cache operated as
             * an LRU cache.  This is suboptimal in the presence of multiple threads
             * executing this code with unique instance classes as it will lead to
             * significant thrashing on the processor caches to maintain coherency.
             * Disable behaving as an LRU cache by default, but leave an environment
             * variable to enable the original behaviour.
             */
            static bool updateCacheSlot = feGetEnv("TR_updateInterfaceCheckCastCacheSlot") != NULL;
            if (updateCacheSlot) {
                Inst_Mem(OP::POPMem, node, MRef_const(cache, cg),
                    cg); // j9class
            } else {
                Inst_Reg(OP::POPReg, node, instanceClassReg, cg); // j9class
            }

            if (!isCheckCast) {
                Inst(OP::STC, node, cg);
            }
            Inst_Label(OP::JMP4, node, endLabel, cg);

            // Not found
            Inst_VFPRestore(vfp, node, cg);

            Inst_Label(OP::label, node, iTableLookUpFailLabel, cg);

            if (isCheckCast) {
                if (castClassReg) {
                    Inst_Reg(OP::PUSHReg, node, castClassReg, cg);
                } else {
                    Inst_Imm(OP::PUSHImm4, node, static_cast<int32_t>(reinterpret_cast<uintptr_t>(castClass)), cg);
                }
                auto call = Inst_HelperCall(node, TR_throwClassCastException, NULL, cg);
                call->setNeedsGCMap(0xFF00FFFF);
                call->setAdjustsFramePointerBy(-2 * (int32_t)sizeof(J9Class *));
            } else {
                Inst_Reg(OP::POPReg, node, instanceClassReg, cg);
                Inst_Label(OP::JMP4, node, endLabel, cg);
            }

            og.endOutlinedInstructionSequence();
        }

        // Succeed
        if (!isCheckCast) {
            Inst(OP::STC, node, cg);
        }
    } else {
        /**
         * Do not use a cache.  Generate the interface lookup inline and the fail
         * path out-of-line.
         */

        /**
         * Use the scratch register if it is available.  Otherwise, re-use the
         * instanceClassReg register to perform itable lookup
         */
        TR::Register *itableReg = scratchReg ? scratchReg : instanceClassReg;

        // Obtain I-Table
        Inst_RegMem(OP::LRegMem(), node, itableReg, MRef_Bdisp32(instanceClassReg, offsetof(J9Class, iTable), cg), cg);

        inlineInterfaceLookup(node, cg, isCheckCast ? iTableLookUpFailLabel : endLabel, castClass, castClassReg,
            itableReg);

        if (!isCheckCast) {
            // Class found in itable
            Inst(OP::STC, node, cg);

            // Fall through to endLabel
        } else {
            // CheckCast iTable fail lookup out-of-line
            TR_OutlinedInstructionsGenerator og(iTableLookUpFailLabel, node, cg);

            Inst_Reg(OP::PUSHReg, node, instanceClassReg, cg);

            if (castClassReg) {
                Inst_Reg(OP::PUSHReg, node, castClassReg, cg);
            } else {
                Inst_Imm(OP::PUSHImm4, node, static_cast<int32_t>(reinterpret_cast<uintptr_t>(castClass)), cg);
            }
            auto call = Inst_HelperCall(node, TR_throwClassCastException, NULL, cg);
            call->setNeedsGCMap(0xFF00FFFF);
            call->setAdjustsFramePointerBy(-2 * (int32_t)sizeof(J9Class *));

            og.endOutlinedInstructionSequence();
        }
    }

    Inst_Label(OP::label, node, endLabel, deps, cg);

    cg->stopUsingRegister(instanceClassReg);

    if (castClassReg) {
        cg->stopUsingRegister(castClassReg);
    }

    if (scratchReg) {
        cg->stopUsingRegister(scratchReg);
    }
}

inline void generateInlinedCheckCastOrInstanceOfForClass(TR::Node *node, TR_OpaqueClassBlock *clazz,
    TR::CodeGenerator *cg, bool isCheckCast)
{
    TR::Compilation *comp = cg->comp();
    auto fej9 = (TR_J9VMBase *)(cg->fe());

    bool use64BitClasses = false;
    if (comp->target().is64Bit()) {
        // When running 64 bit compressed refs, if clazz is an address above the 2G
        // boundary then we can't use a push 32bit immediate instruction to pass it
        // to the helper as the address gets sign extended. So we need to test for
        // this case and switch to the 64bit memory to memory encoding
        // that is used when running 64 bit non-compressed.
        auto highClass = ((uintptr_t)clazz) > INT_MAX;

        use64BitClasses = !TR::Compiler->om.generateCompressedObjectHeaders() || highClass
            || (comp->compileRelocatableCode() && comp->getOption(TR_UseSymbolValidationManager));
    }

    auto clazzData = use64BitClasses ? cg->create8ByteData(node, (uint64_t)(uintptr_t)clazz) : NULL;
    if (clazzData) {
        clazzData->setClassAddress(true);
    }

    auto j9class = cg->allocateRegister();
    auto tmp = cg->allocateRegister();

    auto deps = RegDeps((uint8_t)2, (uint8_t)2, cg);
    deps->addPreCondition(tmp, TR::RealRegister::NoReg, cg);
    deps->addPreCondition(j9class, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(tmp, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(j9class, TR::RealRegister::NoReg, cg);

    auto begLabel = generateLabelSymbol(cg);
    auto endLabel = generateLabelSymbol(cg);
    begLabel->setStartInternalControlFlow();
    endLabel->setEndInternalControlFlow();

    auto successLabel = isCheckCast ? endLabel : generateLabelSymbol(cg);
    auto failLabel = isCheckCast ? generateLabelSymbol(cg) : endLabel;

    Inst_RegReg(OP::MOVRegReg(), node, j9class, node->getChild(0)->getRegister(), cg);
    Inst_Label(OP::label, node, begLabel, cg);

    // Null test
    if (!node->getChild(0)->isNonNull() && node->getOpCodeValue() != TR::checkcastAndNULLCHK) {
        // j9class contains the object at this point, reusing the register as object is no longer used after this point.
        Inst_RegReg(OP::TESTRegReg(), node, j9class, j9class, cg);
        Inst_Label(OP::JE4, node, endLabel, cg);
    }

    // Load J9Class
    generateLoadJ9Class(node, j9class, j9class, cg);

    // Equality test
    if (!fej9->isAbstractClass(clazz) || node->getOpCodeValue() == TR::icall /*TR_checkAssignable*/) {
        // For instanceof and checkcast, LHS is obtained from an instance, which cannot be abstract or interface;
        // therefore, equality test can be safely skipped for instanceof and checkcast when RHS is abstract.
        // However, LHS for TR_checkAssignable may be abstract or interface as it may be an arbitrary class, and
        // hence equality test is always needed.
        if (use64BitClasses) {
            Inst_RegMem(OP::CMP8RegMem, node, j9class, MRef_const(clazzData, cg), cg);
        } else {
            Inst_RegImm(OP::CMP4RegImm4, node, j9class, (uintptr_t)clazz, cg);
        }
        if (!fej9->isClassFinal(clazz)) {
            Inst_Label(OP::JE4, node, successLabel, cg);
        }
    }
    // at this point, ZF == 1 indicates success

    // Superclass test
    if (!fej9->isClassFinal(clazz)) {
        auto depth = TR::Compiler->cls.classDepthOf(clazz);
        if (depth >= comp->getOptions()->_minimumSuperclassArraySize) {
            static_assert(J9AccClassDepthMask == 0xffff, "J9AccClassDepthMask must be 0xffff");
            auto depthMR = MRef_Bdisp32(j9class, offsetof(J9Class, classDepthAndFlags), cg);
            Inst_MemImm(OP::CMP2MemImm2, node, depthMR, depth, cg);
            if (!isCheckCast) {
                // Need ensure CF is cleared before reaching to fail label
                auto outlineLabel = generateLabelSymbol(cg);
                Inst_Label(OP::JBE4, node, outlineLabel, cg);

                TR_OutlinedInstructionsGenerator og(outlineLabel, node, cg);
                Inst(OP::CLC, node, cg);
                Inst_Label(OP::JMP4, node, failLabel, cg);
                og.endOutlinedInstructionSequence();
            } else {
                Inst_Label(OP::JBE4, node, failLabel, cg);
            }
        }

        Inst_RegMem(OP::LRegMem(), node, tmp, MRef_Bdisp32(j9class, offsetof(J9Class, superclasses), cg), cg);
        auto offset = depth * sizeof(J9Class *);
        TR_ASSERT(IS_32BIT_SIGNED(offset), "The offset to superclass is unreasonably large.");
        auto superclass = MRef_Bdisp32(tmp, offset, cg);
        if (use64BitClasses) {
            Inst_RegMem(OP::L8RegMem, node, tmp, superclass, cg);
            Inst_RegMem(OP::CMP8RegMem, node, tmp, MRef_const(clazzData, cg), cg);
        } else {
            Inst_MemImm(OP::CMP4MemImm4, node, superclass, (int32_t)(uintptr_t)clazz, cg);
        }
    }
    // at this point, ZF == 1 indicates success

    // Branch to success/fail path
    if (!isCheckCast) {
        Inst(OP::CLC, node, cg);
    }
    Inst_Label(OP::JNE4, node, failLabel, cg);

    // Set CF to report success
    if (!isCheckCast) {
        Inst_Label(OP::label, node, successLabel, cg);
        Inst(OP::STC, node, cg);
    }

    // Throw exception for CheckCast
    if (isCheckCast) {
        TR_OutlinedInstructionsGenerator og(failLabel, node, cg);

        Inst_Reg(OP::PUSHReg, node, j9class, cg);
        if (use64BitClasses) {
            Inst_Mem(OP::PUSHMem, node, MRef_const(clazzData, cg), cg);
        } else {
            Inst_Imm(OP::PUSHImm4, node, (int32_t)(uintptr_t)clazz, cg);
        }
        auto call = Inst_HelperCall(node, TR_throwClassCastException, NULL, cg);
        call->setNeedsGCMap(0xFF00FFFF);
        call->setAdjustsFramePointerBy(-2 * (int32_t)sizeof(J9Class *));

        og.endOutlinedInstructionSequence();
    }

    // Succeed
    Inst_Label(OP::label, node, endLabel, deps, cg);

    cg->stopUsingRegister(j9class);
    cg->stopUsingRegister(tmp);
}

TR::Register *J9::X86::TreeEvaluator::checkcastinstanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();

    bool isCheckCast = false;
    switch (node->getOpCodeValue()) {
        case TR::checkcast:
        case TR::checkcastAndNULLCHK:
            isCheckCast = true;
            break;
        case TR:: instanceof:
        case TR::icall: // TR_checkAssignable
            break;
        default:
            TR_ASSERT(false, "Incorrect Op Code %d.", node->getOpCodeValue());
            break;
    }
    TR_OpaqueClassBlock *clazz = TR::TreeEvaluator::getCastClassAddress(node->getChild(1));
    if (isCheckCast && !clazz && !comp->getOption(TR_DisableInlineCheckCast)
        && (!comp->compileRelocatableCode() || comp->getOption(TR_UseSymbolValidationManager))) {
        generateInlinedCheckCastForDynamicCastClass(node, cg);
    } else if (clazz && !TR::Compiler->cls.isClassArray(comp, clazz) && // not yet optimized
        (!comp->compileRelocatableCode() || comp->getOption(TR_UseSymbolValidationManager))
        && !comp->getOption(TR_DisableInlineCheckCast) && !comp->getOption(TR_DisableInlineInstanceOf)) {
        cg->evaluate(node->getChild(0));
        if (TR::Compiler->cls.isInterfaceClass(comp, clazz)) {
            generateInlinedCheckCastOrInstanceOfForInterface(node, clazz, cg, isCheckCast);
        } else {
            generateInlinedCheckCastOrInstanceOfForClass(node, clazz, cg, isCheckCast);
        }
        if (!isCheckCast) {
            auto result = cg->allocateRegister();
            Inst_Reg(OP::SETB1Reg, node, result, cg);
            Inst_RegReg(OP::MOVZXReg4Reg1, node, result, result, cg);
            node->setRegister(result);
        }
        cg->decReferenceCount(node->getChild(0));
        cg->recursivelyDecReferenceCount(node->getChild(1));
    } else {
        generateInlinedCheckCastOrInstanceOfForArrayClass(node, clazz, isCheckCast, cg);
    }
    return node->getRegister();
}

static TR::MemoryReference *getMemoryReference(TR::Register *objectClassReg, TR::Register *objectReg, int32_t lwOffset,
    TR::CodeGenerator *cg)
{
    if (objectClassReg)
        return MRef_BIS(objectReg, objectClassReg, 0, cg);
    else
        return MRef_Bdisp32(objectReg, lwOffset, cg);
}

void J9::X86::TreeEvaluator::asyncGCMapCheckPatching(TR::Node *node, TR::CodeGenerator *cg,
    TR::LabelSymbol *snippetLabel)
{
    TR::MemoryReference *SOMmr = MRef_node(node->getFirstChild()->getFirstChild(), cg);
    TR::Compilation *comp = cg->comp();

    if (cg->comp()->target().is64Bit()) {
        // 64 bit sequence
        //
        // Generate a call to the out-of-line patching sequence.
        // This sequence will convert the call back into an asynch message check cmp
        //
        TR::LabelSymbol *gcMapPatchingLabel = generateLabelSymbol(cg);
        TR::LabelSymbol *outlinedStartLabel = generateLabelSymbol(cg);
        TR::LabelSymbol *outlinedEndLabel = generateLabelSymbol(cg);
        TR::LabelSymbol *asyncWithoutPatch = generateLabelSymbol(cg);

        // Start inline patching sequence
        //
        TR::Register *patchableAddrReg = cg->allocateRegister();
        TR::Register *patchValReg = cg->allocateRegister();
        TR::Register *tempReg = cg->allocateRegister();

        outlinedStartLabel->setStartInternalControlFlow();
        outlinedEndLabel->setEndInternalControlFlow();

        // Inst_Label(OP::CALLImm4, node, gcMapPatchingLabel, cg);
        Inst_PatchableCodeAlignment(TR::X86PatchableCodeAlignmentInstruction::CALLImm4AtomicRegions,
            Inst_Label(OP::CALLImm4, node, gcMapPatchingLabel, cg), cg);

        TR_OutlinedInstructionsGenerator og(gcMapPatchingLabel, node, cg);

        Inst_Label(OP::label, node, outlinedStartLabel, cg);
        // Load the address that we are going to patch and clean up the stack
        //
        Inst_Reg(OP::POPReg, node, patchableAddrReg, cg);

        // check if there is already an async even pending
        //
        Inst_MemImm(OP::CMP8MemImm4, node, SOMmr, -1, cg);
        Inst_Label(OP::JE4, node, asyncWithoutPatch, cg);

        // Signal the async event
        //
        static char *d = feGetEnv("TR_GCOnAsyncBREAK");
        if (d)
            Inst(OP::INT3, node, cg);

        Inst_MemImm(OP::S8MemImm4, node,
            MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, stackOverflowMark), cg), -1, cg);
        Inst_RegImm(OP::MOV8RegImm4, node, tempReg, 1 << comp->getPersistentInfo()->getGCMapCheckEventHandle(), cg);
        Inst_MemReg(OP::LOR8MemReg, node,
            MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, asyncEventFlags), cg), tempReg, cg);

        // Populate the code we are going to patch in
        //
        // existing
        // 000007ff`7d340578 e8f4170000      call    000007ff`7d341d71  <------
        // 000007ff`7d34057d 0f84ee1e0000    je      000007ff`7d342471
        //*********
        // patching in
        // 000007ff'7d34056f 48837d50ff      cmp qword ptr [rbp+0x50], 0xffffffffffffffff   <-----
        // 000007ff`7d34057d 0f84ee1e0000    je      000007ff`7d342471

        // Load the original value
        //

        Inst_RegMem(OP::L8RegMem, node, patchValReg, MRef_Bdisp32(patchableAddrReg, -5, cg), cg);
        Inst_RegImm64(OP::MOV8RegImm64, node, tempReg, (uint64_t)0x0, cg);
        Inst_RegReg(OP::OR8RegReg, node, patchValReg, tempReg, cg);
        Inst_RegImm64(OP::MOV8RegImm64, node, tempReg, (uint64_t)0x0, cg);
        Inst_RegReg(OP::AND8RegReg, node, patchValReg, tempReg, cg);

        TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, 4, cg);
        deps->addPostCondition(patchableAddrReg, TR::RealRegister::NoReg, cg);
        deps->addPostCondition(patchValReg, TR::RealRegister::NoReg, cg);
        deps->addPostCondition(tempReg, TR::RealRegister::NoReg, cg);
        deps->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);
        deps->stopAddingConditions();

        Inst_MemReg(OP::S8MemReg, node, MRef_Bdisp32(patchableAddrReg, -5, cg), patchValReg, deps, cg);
        Inst_Label(OP::label, node, asyncWithoutPatch, cg);
        Inst_Label(OP::JMP4, node, snippetLabel, cg);

        cg->stopUsingRegister(patchableAddrReg);
        cg->stopUsingRegister(patchValReg);
        cg->stopUsingRegister(tempReg);
        Inst_Label(OP::label, node, outlinedEndLabel, cg);

        og.endOutlinedInstructionSequence();
    } else {
        // 32 bit sequence
        //

        // Generate a call to the out-of-line patching sequence.
        // This sequence will convert the call back into an asynch message check cmp
        //
        TR::LabelSymbol *gcMapPatchingLabel = generateLabelSymbol(cg);
        TR::LabelSymbol *outlinedStartLabel = generateLabelSymbol(cg);
        TR::LabelSymbol *outlinedEndLabel = generateLabelSymbol(cg);
        TR::LabelSymbol *asyncWithoutPatch = generateLabelSymbol(cg);

        // Start inline patching sequence
        //
        TR::Register *patchableAddrReg = cg->allocateRegister();
        TR::Register *lowPatchValReg = cg->allocateRegister();
        TR::Register *highPatchValReg = cg->allocateRegister();
        TR::Register *lowExistingValReg = cg->allocateRegister();
        TR::Register *highExistingValReg = cg->allocateRegister();

        outlinedStartLabel->setStartInternalControlFlow();
        outlinedEndLabel->setEndInternalControlFlow();

        // Inst_BoundaryAvoidance(TR::X86BoundaryAvoidanceInstruction::CALLImm4AtomicRegions, 8,
        // 8,Inst_Label(OP::CALLImm4, node, gcMapPatchingLabel, cg), cg);
        TR::Instruction *callInst
            = Inst_PatchableCodeAlignment(TR::X86PatchableCodeAlignmentInstruction::CALLImm4AtomicRegions,
                Inst_Label(OP::CALLImm4, node, gcMapPatchingLabel, cg), cg);
        TR::X86VFPSaveInstruction *vfpSaveInst = Inst_VFPSave(callInst->getPrev(), cg);

        TR_OutlinedInstructionsGenerator og(gcMapPatchingLabel, node, cg);

        Inst_Label(OP::label, node, outlinedStartLabel, cg);
        // Load the address that we are going to patch and clean up the stack
        //
        Inst_Reg(OP::POPReg, node, patchableAddrReg, cg);

        // check if there is already an async even pending
        //
        Inst_MemImm(OP::CMP4MemImm4, node, SOMmr, -1, cg);
        Inst_Label(OP::JE4, node, asyncWithoutPatch, cg);

        // Signal the async event
        //
        Inst_MemImm(OP::S4MemImm4, node,
            MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, stackOverflowMark), cg), -1, cg);
        Inst_RegImm(OP::MOV4RegImm4, node, lowPatchValReg, 1 << comp->getPersistentInfo()->getGCMapCheckEventHandle(),
            cg);
        Inst_MemReg(OP::LOR4MemReg, node,
            MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, asyncEventFlags), cg), lowPatchValReg, cg);

        // Populate the registers we are going to use in the lock cmp xchg
        //

        static char *d = feGetEnv("TR_GCOnAsyncBREAK");
        if (d)
            Inst(OP::INT3, node, cg);

        // Populate the existing inline code
        //
        Inst_RegMem(OP::L4RegMem, node, lowExistingValReg, MRef_Bdisp32(patchableAddrReg, -5, cg), cg);
        Inst_RegMem(OP::L4RegMem, node, highExistingValReg, MRef_Bdisp32(patchableAddrReg, -1, cg), cg);

        // Populate the code we are going to patch in
        // 837d28ff        cmp     dword ptr [ebp+28h],0FFFFFFFFh <--- patching in
        // 90              nop
        //*******************
        //                 call imm4                              <---- patching over
        //
        Inst_RegImm(OP::MOV4RegImm4, node, lowPatchValReg, (uint32_t)0x287d8390, cg);
        Inst_RegReg(OP::MOV4RegReg, node, highPatchValReg, highExistingValReg, cg);
        Inst_RegImm(OP::OR4RegImm4, node, highPatchValReg, (uint32_t)0x000000ff, cg);

        TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, 6, cg);

        deps->addPostCondition(patchableAddrReg, TR::RealRegister::edi, cg);
        deps->addPostCondition(lowPatchValReg, TR::RealRegister::ebx, cg);
        deps->addPostCondition(highPatchValReg, TR::RealRegister::ecx, cg);
        deps->addPostCondition(lowExistingValReg, TR::RealRegister::eax, cg);
        deps->addPostCondition(highExistingValReg, TR::RealRegister::edx, cg);
        deps->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);
        deps->stopAddingConditions();
        Inst_Mem(OP::LCMPXCHG8BMem, node, MRef_Bdisp32(patchableAddrReg, -5, cg), deps, cg);
        Inst_Label(OP::label, node, asyncWithoutPatch, cg);
        Inst_VFPRestore(Inst_Label(OP::JMP4, node, snippetLabel, cg), vfpSaveInst, cg);

        cg->stopUsingRegister(patchableAddrReg);
        cg->stopUsingRegister(lowPatchValReg);
        cg->stopUsingRegister(highPatchValReg);
        cg->stopUsingRegister(lowExistingValReg);
        cg->stopUsingRegister(highExistingValReg);
        Inst_Label(OP::label, node, outlinedEndLabel, cg);

        og.endOutlinedInstructionSequence();
    }
}

void J9::X86::TreeEvaluator::inlineRecursiveMonitor(TR::Node *node, TR::CodeGenerator *cg,
    TR::LabelSymbol *fallThruLabel, TR::LabelSymbol *jitMonitorEnterOrExitSnippetLabel,
    TR::LabelSymbol *inlineRecursiveSnippetLabel, TR::Register *objectReg, int lwOffset,
    TR::LabelSymbol *snippetRestartLabel, bool reservingLock)
{
    // Code generated:
    //
    // outlinedStartLabel:
    //    mov lockWordReg, [obj+lwOffset]
    //    add lockWordReg, INC_DEC_VALUE/-INC_DEC_VALUE  ---> lock word with increased recursive count
    //    mov lockWordMaskedReg, NON_INC_DEC_MASK
    //    and lockWordMaskedReg, lockWordReg  ---> lock word masked out counter bits
    //    cmp lockWordMaskedReg, vmThreadReg
    //    jne jitMonitorEnterOrExitSnippetLabel
    //    mov [obj+lwOffset], lockWordReg
    //
    // #if defined(TR_TARGET_64BIT) && (JAVA_SPEC_VERSION >= 19)
    //    inc|dec [vmThreadReg + ownedMonitorCountOffset]
    // #endif
    //
    // snippetRestartLabel:
    //    jmp fallThruLabel
    // outlinedEndLabel:
    //
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
    TR::LabelSymbol *outlinedStartLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *outlinedEndLabel = generateLabelSymbol(cg);

    outlinedStartLabel->setStartInternalControlFlow();
    outlinedEndLabel->setEndInternalControlFlow();

    TR_OutlinedInstructionsGenerator og(inlineRecursiveSnippetLabel, node, cg);

    Inst_Label(OP::label, node, outlinedStartLabel, cg);
    TR::Register *lockWordReg = cg->allocateRegister();
    TR::Register *lockWordMaskedReg = cg->allocateRegister();
    TR::Register *vmThreadReg = cg->getVMThreadRegister();
    bool use64bitOp = cg->comp()->target().is64Bit() && !fej9->generateCompressedLockWord();
    bool isMonitorEnter
        = node->getSymbolReference() == cg->comp()->getSymRefTab()->findOrCreateMethodMonitorEntrySymbolRef(NULL)
        || node->getSymbolReference() == cg->comp()->getSymRefTab()->findOrCreateMonitorEntrySymbolRef(NULL);

    Inst_RegMem(OP::LRegMem(use64bitOp), node, lockWordReg, MRef_Bdisp32(objectReg, lwOffset, cg), cg);
    Inst_RegImm(OP::ADDRegImm4(use64bitOp), node, lockWordReg, isMonitorEnter ? INC_DEC_VALUE : -INC_DEC_VALUE, cg);
    Inst_RegImm(OP::MOVRegImm4(use64bitOp), node, lockWordMaskedReg, NON_INC_DEC_MASK - RES_BIT, cg);
    Inst_RegReg(OP::ANDRegReg(use64bitOp), node, lockWordMaskedReg, lockWordReg, cg);
    Inst_RegReg(OP::CMPRegReg(use64bitOp), node, lockWordMaskedReg, vmThreadReg, cg);

    Inst_Label(OP::JNE4, node, jitMonitorEnterOrExitSnippetLabel, cg);
    Inst_MemReg(OP::SMemReg(use64bitOp), node, MRef_Bdisp32(objectReg, lwOffset, cg), lockWordReg, cg);

#if defined(TR_TARGET_64BIT) && (JAVA_SPEC_VERSION >= 19)
    // Adjust J9VMThread ownedMonitorCount for execution paths that do not
    // go out of line to acquire the lock.  It is safe and efficient to do
    // this unconditionally.  There is no need to check for overflow or
    // underflow.
    //
    Inst_Mem(isMonitorEnter ? OP::INC8Mem : OP::DEC8Mem, node,
        MRef_Bdisp32(vmThreadReg, fej9->thisThreadGetOwnedMonitorCountOffset(), cg), cg);
#endif

    TR::RegisterDependencyConditions *restartDeps = RegDeps((uint8_t)0, 4, cg);
    restartDeps->addPostCondition(objectReg, TR::RealRegister::NoReg, cg);
    restartDeps->addPostCondition(vmThreadReg, TR::RealRegister::ebp, cg);
    restartDeps->addPostCondition(lockWordMaskedReg, TR::RealRegister::NoReg, cg);
    restartDeps->addPostCondition(lockWordReg, TR::RealRegister::NoReg, cg);
    restartDeps->stopAddingConditions();
    Inst_Label(OP::label, node, snippetRestartLabel, restartDeps, cg);

    Inst_Label(OP::JMP4, node, fallThruLabel, cg);

    cg->stopUsingRegister(lockWordReg);
    cg->stopUsingRegister(lockWordMaskedReg);

    TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, 1, cg);
    deps->addPostCondition(vmThreadReg, TR::RealRegister::ebp, cg);
    deps->stopAddingConditions();
    Inst_Label(OP::label, node, outlinedEndLabel, deps, cg);

    og.endOutlinedInstructionSequence();
}

void J9::X86::TreeEvaluator::generateCheckForValueMonitorEnterOrExit(TR::Node *node, int32_t classFlag,
    TR::LabelSymbol *snippetLabel, TR::CodeGenerator *cg)
{
    TR::Register *objectReg = cg->evaluate(node->getFirstChild());
    TR::Register *j9classReg = cg->allocateRegister();
    generateLoadJ9Class(node, j9classReg, objectReg, cg);
    auto fej9 = (TR_J9VMBase *)(cg->fe());
    TR::MemoryReference *classFlagsMR = MRef_Bdisp32(j9classReg, (uintptr_t)(fej9->getOffsetOfClassFlags()), cg);

    OP::Mnemonic testOpCode;
    if ((uint32_t)classFlag <= USHRT_MAX)
        testOpCode = OP::TEST2MemImm2;
    else
        testOpCode = OP::TEST4MemImm4;

    Inst_MemImm(testOpCode, node, classFlagsMR, classFlag, cg);
    Inst_Label(OP::JNE4, node, snippetLabel, cg);
}

TR::Register *J9::X86::TreeEvaluator::VMmonentEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // If there is a NULLCHK above this node it will be expecting us to set
    // up the excepting instruction. If we are not going to inline an
    // appropriate excepting instruction we must make sure to reset the
    // excepting instruction since our children may have set it.
    //
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
    bool is64Bit = comp->target().is64Bit();

    static const char *noInline = feGetEnv("TR_NoInlineMonitor");
    bool reservingLock = false;
    bool normalLockPreservingReservation = false;
    bool dummyMethodMonitor = false;
    TR_YesNoMaybe isMonitorValueBasedOrValueType = cg->isMonitorValueBasedOrValueType(node);

    int lwOffset = fej9->getByteOffsetToLockword((TR_OpaqueClassBlock *)cg->getMonClass(node));
    if (comp->getOption(TR_MimicInterpreterFrameShape)
        || (comp->getOption(TR_FullSpeedDebug) && node->isSyncMethodMonitor()) || noInline
        || (isMonitorValueBasedOrValueType == TR_yes) || comp->getOption(TR_DisableInlineMonEnt)) {
        // Don't inline
        //
        TR::ILOpCodes opCode = node->getOpCodeValue();
        TR::Node::recreate(node, TR::call);
        TR::TreeEvaluator::directCallEvaluator(node, cg);
        TR::Node::recreate(node, opCode);
        cg->setImplicitExceptionPoint(NULL);
        return NULL;
    }

    if (lwOffset > 0 && comp->getOption(TR_ReservingLocks)) {
        bool dummy = false;
        TR::TreeEvaluator::evaluateLockForReservation(node, &reservingLock, &normalLockPreservingReservation, cg);
        TR::TreeEvaluator::isPrimitiveMonitor(node, cg);

        if (node->isPrimitiveLockedRegion() && reservingLock)
            dummyMethodMonitor = TR::TreeEvaluator::isDummyMonitorEnter(node, cg);

        if (reservingLock && !node->isPrimitiveLockedRegion())
            dummyMethodMonitor = false;
    }

    TR::Node *objectRef = node->getFirstChild();

    static const char *disableInlineRecursiveEnv = feGetEnv("TR_DisableInlineRecursiveMonitor");
    bool inlineRecursive = disableInlineRecursiveEnv ? false : true;
    if (lwOffset <= 0)
        inlineRecursive = false;

    // Evaluate the object reference
    //
    TR::Register *objectReg = cg->evaluate(objectRef);
    TR::Register *eaxReal = cg->allocateRegister();
    TR::Register *scratchReg = NULL;
    uint32_t numDeps = 3; // objectReg, eax, ebp

    cg->setImplicitExceptionPoint(NULL);

    TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *fallThruLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *inlinedMonEnterFallThruLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *snippetFallThruLabel = inlineRecursive ? generateLabelSymbol(cg) : fallThruLabel;

    startLabel->setStartInternalControlFlow();
    fallThruLabel->setEndInternalControlFlow();
    Inst_Label(OP::label, node, startLabel, cg);

    TR::Register *vmThreadReg = cg->getVMThreadRegister();

    TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *monitorLookupCacheLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *fallThruFromMonitorLookupCacheLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *exitLabel = NULL;

    TR_OutlinedInstructions *outlinedHelperCall;
    // In the reserving lock case below, we change the symref on the node... Here, we are going to store the original
    // symref, so that we can restore our change.
    TR::SymbolReference *originalNodeSymRef = NULL;

    TR::Node *helperCallNode = node;

    if (isMonitorValueBasedOrValueType == TR_maybe)
        TR::TreeEvaluator::generateCheckForValueMonitorEnterOrExit(node, J9_CLASS_DISALLOWS_LOCKING_FLAGS, snippetLabel,
            cg);

    if (comp->getOption(TR_ReservingLocks)) {
        // About to change the node's symref... store the original.
        originalNodeSymRef = node->getSymbolReference();

        if (reservingLock && node->isPrimitiveLockedRegion() && dummyMethodMonitor) {
            TR_RuntimeHelper monExitHelper = (node->getSymbolReference() == cg->getSymRef(TR_methodMonitorEntry))
                ? TR_AMD64JitMethodMonitorExitReservedPrimitive
                : TR_AMD64JitMonitorExitReservedPrimitive;

            node->setSymbolReference(comp->getSymRefTab()->findOrCreateRuntimeHelper(monExitHelper, true, true, true));

            exitLabel = generateLabelSymbol(cg);
            TR_OutlinedInstructions *outlinedExitHelperCall
                = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::call, NULL, exitLabel, fallThruLabel, cg);
            cg->getOutlinedInstructionsList().push_front(outlinedExitHelperCall);
        }

        TR_RuntimeHelper helper;
        bool success = TR::TreeEvaluator::monEntryExitHelper(true, node, reservingLock, normalLockPreservingReservation,
            helper, cg);
        if (success)
            node->setSymbolReference(comp->getSymRefTab()->findOrCreateRuntimeHelper(helper, true, true, true));

        if (reservingLock) {
            uint32_t reservableLwValue = RES_BIT;
            if (TR::Options::_aggressiveLockReservation)
                reservableLwValue = 0;

            // Make this integer the same size as the lock word. If we always
            // passed a 32-bit value, then on 64-bit with an uncompressed lock
            // word, the helper would have to either zero-extend the value, or
            // rely on the caller having done so even though the calling
            // convention doesn't appear to require it.
            TR::Node *reservableLwNode = NULL;
            if (comp->target().is32Bit() || fej9->generateCompressedLockWord())
                reservableLwNode = TR::Node::iconst(node, reservableLwValue);
            else
                reservableLwNode = TR::Node::lconst(node, reservableLwValue);

            helperCallNode = TR::Node::create(node, TR::call, 2, objectRef, reservableLwNode);

            helperCallNode->setSymbolReference(node->getSymbolReference());
            helperCallNode->incReferenceCount();
        }
    }

    outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(helperCallNode, TR::call, NULL, snippetLabel,
        (exitLabel) ? exitLabel : snippetFallThruLabel, cg);

    if (helperCallNode != node)
        helperCallNode->recursivelyDecReferenceCount();

    cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);
    cg->generateDebugCounter(outlinedHelperCall->getFirstInstruction(),
        TR::DebugCounter::debugCounterName(comp, "helperCalls/%s/(%s)/%d/%d", node->getOpCode().getName(),
            comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
        1, TR::DebugCounter::Cheap);

    // Okay, and we've made it down here and we've successfully generated all outlined snippets, let's restore the
    // node's symref.
    if (comp->getOption(TR_ReservingLocks)) {
        node->setSymbolReference(originalNodeSymRef);
    }

    if (inlineRecursive) {
        TR::LabelSymbol *inlineRecursiveSnippetLabel = generateLabelSymbol(cg);
        TR::LabelSymbol *jitMonitorEnterSnippetLabel = snippetLabel;
        snippetLabel = inlineRecursiveSnippetLabel;
        TR::TreeEvaluator::inlineRecursiveMonitor(node, cg, fallThruLabel, jitMonitorEnterSnippetLabel,
            inlineRecursiveSnippetLabel, objectReg, lwOffset, snippetFallThruLabel, reservingLock);
    }

    // Compare the monitor slot in the object against zero.  If it succeeds
    // we are done.  Else call the helper.
    // Code generated:
    //    xor     eax, eax
    //    cmpxchg monitor(objectReg), ebp
    //    jne     snippet
    //    label   restartLabel
    //
    // Code generated for read monitor enter:
    //    xor     eax, eax
    //    mov     lockedReg, INC_DEC_VALUE (0x04)
    //    cmpxchg monitor(objectReg), lockedReg
    //    jne     snippet
    //    label   restartLabel
    //
    TR::Register *lockedReg = NULL;
    OP::Mnemonic op = OP::bad;

    bool isSMP = comp->target().isSMP();
    if (is64Bit && !fej9->generateCompressedLockWord()) {
        op = isSMP ? OP::LCMPXCHG8MemReg : OP::CMPXCHG8MemReg;
    } else {
        op = isSMP ? OP::LCMPXCHG4MemReg : OP::CMPXCHG4MemReg;
    }

    TR::Register *objectClassReg = NULL;
    TR::Register *lookupOffsetReg = NULL;

    if (lwOffset <= 0) {
        TR::MemoryReference *objectClassMR = MRef_Bdisp32(objectReg, TMP_OFFSETOF_J9OBJECT_CLAZZ, cg);
        objectClassReg = cg->allocateRegister();
        numDeps++;

        OP::Mnemonic loadOp = (TR::Compiler->om.compressObjectReferences()) ? OP::L4RegMem : OP::LRegMem();
        TR::X86RegMemInstruction *instr = Inst_RegMem(loadOp, node, objectClassReg, objectClassMR, cg);

        // This instruction may try to dereference a null memory address
        // add an implicit exception point for it.
        //
        cg->setImplicitExceptionPoint(instr);
        instr->setNeedsGCMap(0xFF00FFFF);

        TR::TreeEvaluator::generateVFTMaskInstruction(node, objectClassReg, cg);
        int32_t offsetOfLockOffset = offsetof(J9Class, lockOffset);
        Inst_RegMem(OP::LRegMem(), node, objectClassReg, MRef_Bdisp32(objectClassReg, offsetOfLockOffset, cg), cg);
        Inst_RegImm(OP::CMPRegImms(), node, objectClassReg, 0, cg);

        generateCommonLockNurseryCodes(node, cg,
            true, // true for VMmonentEvaluator, false for VMmonexitEvaluator
            monitorLookupCacheLabel, fallThruFromMonitorLookupCacheLabel, snippetLabel, numDeps, lwOffset,
            objectClassReg, lookupOffsetReg, vmThreadReg, objectReg);
    }

    if (comp->getOption(TR_ReservingLocks) && reservingLock) {
        TR::LabelSymbol *mismatchLabel
            = TR::Options::_aggressiveLockReservation ? snippetLabel : generateLabelSymbol(cg);

        Inst_RegMem(OP::LEARegMem(), node, eaxReal, MRef_Bdisp32(vmThreadReg, RES_BIT, cg), cg);

        OP::Mnemonic cmpMemRegOp
            = (is64Bit && fej9->generateCompressedLockWord()) ? OP::CMP4MemReg : OP::CMPMemReg(is64Bit);
        TR::X86MemRegInstruction *instr
            = Inst_MemReg(cmpMemRegOp, node, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), eaxReal, cg);

        cg->setImplicitExceptionPoint(instr);
        instr->setNeedsGCMap(0xFF00FFFF);

        Inst_Label(OP::JNE4, node, mismatchLabel, cg);

        if (!node->isPrimitiveLockedRegion()) {
            OP::Mnemonic addMemImmOp
                = (is64Bit && fej9->generateCompressedLockWord()) ? OP::ADD4MemImms : OP::ADDMemImms();
            Inst_MemImm(addMemImmOp, node, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), REC_BIT, cg);
        }

        if (!TR::Options::_aggressiveLockReservation) {
            // Jump over the non-reservable path
            Inst_Label(OP::JMP4, node, inlinedMonEnterFallThruLabel, cg);

            // It's possible that the lock may be available, but not reservable. In
            // that case we should try the usual cmpxchg for non-reserving enter.
            // Otherwise we'll necessarily call the helper.
            Inst_Label(OP::label, node, mismatchLabel, cg);

            OP::Mnemonic cmpMemImmOp
                = (is64Bit && fej9->generateCompressedLockWord()) ? OP::CMP4MemImms : OP::CMPMemImms();

            auto lwMR = getMemoryReference(objectClassReg, objectReg, lwOffset, cg);
            Inst_MemImm(cmpMemImmOp, node, lwMR, 0, cg);
            Inst_Label(OP::JNE4, node, snippetLabel, cg);
            Inst_RegReg(OP::XOR4RegReg, node, eaxReal, eaxReal, cg);
            lwMR = getMemoryReference(objectClassReg, objectReg, lwOffset, cg);
            Inst_MemReg(op, node, lwMR, vmThreadReg, cg);
            Inst_Label(OP::JNE4, node, snippetLabel, cg);
        }
    } else {
        if (TR::Options::_aggressiveLockReservation) {
            if (comp->getOption(TR_ReservingLocks) && normalLockPreservingReservation) {
                OP::Mnemonic cmpMemImmOp
                    = (is64Bit && fej9->generateCompressedLockWord()) ? OP::CMP4MemImms : OP::CMPMemImms();

                TR::X86MemImmInstruction *instr = Inst_MemImm(cmpMemImmOp, node,
                    getMemoryReference(objectClassReg, objectReg, lwOffset, cg), 0, cg);

                cg->setImplicitExceptionPoint(instr);
                instr->setNeedsGCMap(0xFF00FFFF);

                Inst_Label(OP::JNE4, node, snippetLabel, cg);
            }

            Inst_RegReg(OP::XOR4RegReg, node, eaxReal, eaxReal, cg);
        } else if (!comp->getOption(TR_ReservingLocks)) {
            Inst_RegReg(OP::XOR4RegReg, node, eaxReal, eaxReal, cg);
        } else {
            OP::Mnemonic loadOp = OP::LRegMem();
            OP::Mnemonic testOp = OP::TESTRegImm4();
            if (is64Bit && fej9->generateCompressedLockWord()) {
                loadOp = OP::L4RegMem;
                testOp = OP::TEST4RegImm4;
            }

            auto lwMR = getMemoryReference(objectClassReg, objectReg, lwOffset, cg);
            auto instr = Inst_RegMem(loadOp, node, eaxReal, lwMR, cg);
            cg->setImplicitExceptionPoint(instr);
            instr->setNeedsGCMap(0xFF00FFFF);

            Inst_RegImm(testOp, node, eaxReal, (int32_t)~RES_BIT, cg);
            Inst_Label(OP::JNE4, node, snippetLabel, cg);
        }

        if (node->isReadMonitor()) {
            lockedReg = cg->allocateRegister();
            if (is64Bit && fej9->generateCompressedLockWord())
                Inst_RegReg(OP::XOR4RegReg, node, lockedReg, lockedReg,
                    cg); // After lockedReg is allocated zero it out.
            Inst_RegImm(OP::MOVRegImm4(), node, lockedReg, INC_DEC_VALUE, cg);
            ++numDeps;
        } else {
            bool conditionallyReserve = false;
            bool shouldConditionallyReserveForReservableClasses = comp->getOption(TR_ReservingLocks)
                && !TR::Options::_aggressiveLockReservation && lwOffset > 0 && cg->getMonClass(node) != NULL;

            if (shouldConditionallyReserveForReservableClasses) {
                TR_PersistentClassInfo *monClassInfo
                    = comp->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(
                        cg->getMonClass(node), comp);

                if (monClassInfo != NULL && monClassInfo->isReservable())
                    conditionallyReserve = true;
            }

            if (!conditionallyReserve) {
                // we want to write thread reg into lock word
                lockedReg = vmThreadReg;
            } else {
                lockedReg = cg->allocateRegister();
                numDeps++;

                // Compute the value to put into the lock word based on the
                // current value, which is either 0 or RES_BIT ("reservable").
                //
                //    0       ==> vmThreadReg
                //    RES_BIT ==> vmThreadReg | RES_BIT | INC_DEC_VALUE
                //
                // For reservable locks, failure to reserve at this point would
                // prevent any future reservation of the same lock.

                bool b64 = is64Bit && !fej9->generateCompressedLockWord();
                Inst_RegReg(OP::MOVRegReg(b64), node, lockedReg, eaxReal, cg);
                Inst_RegImm(OP::SHRRegImm1(b64), node, lockedReg, RES_BIT_POSITION, cg);
                Inst_Reg(OP::NEGReg(b64), node, lockedReg, cg);
                Inst_RegImm(OP::ANDRegImms(b64), node, lockedReg, RES_BIT | INC_DEC_VALUE, cg);
                Inst_RegReg(OP::ADDRegReg(b64), node, lockedReg, vmThreadReg, cg);
            }
        }

        // try to swap into lock word
        TR::X86MemRegInstruction *instr
            = Inst_MemReg(op, node, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), lockedReg, cg);
        cg->setImplicitExceptionPoint(instr);
        instr->setNeedsGCMap(0xFF00FFFF);

        Inst_Label(OP::JNE4, node, snippetLabel, cg);
    }

    Inst_Label(OP::label, node, inlinedMonEnterFallThruLabel, cg);

#if defined(TR_TARGET_64BIT) && (JAVA_SPEC_VERSION >= 19)
    // Adjust J9VMThread ownedMonitorCount for execution paths that do not
    // go out of line to acquire the lock.  It is safe and efficient to do
    // this unconditionally.  There is no need to check for overflow..
    //
    Inst_Mem(OP::INC8Mem, node, MRef_Bdisp32(vmThreadReg, fej9->thisThreadGetOwnedMonitorCountOffset(), cg), cg);
#endif

    // Create dependencies for the registers used.
    // The dependencies must be in the order:
    //      objectReg, eaxReal, vmThreadReg
    // since the snippet needs to find them to grab the real registers from them.
    //
    TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, (uint8_t)numDeps, cg);
    deps->addPostCondition(objectReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(eaxReal, TR::RealRegister::eax, cg);
    deps->addPostCondition(vmThreadReg, TR::RealRegister::ebp, cg);

    if (scratchReg)
        deps->addPostCondition(scratchReg, TR::RealRegister::NoReg, cg);

    if (lockedReg != NULL && lockedReg != vmThreadReg) {
        deps->addPostCondition(lockedReg, TR::RealRegister::NoReg, cg);
    }

    if (objectClassReg)
        deps->addPostCondition(objectClassReg, TR::RealRegister::NoReg, cg);

    if (lookupOffsetReg)
        deps->addPostCondition(lookupOffsetReg, TR::RealRegister::NoReg, cg);

    deps->stopAddingConditions();

    Inst_Label(OP::label, node, fallThruLabel, deps, cg);

    cg->decReferenceCount(objectRef);
    cg->stopUsingRegister(eaxReal);
    if (scratchReg)
        cg->stopUsingRegister(scratchReg);
    if (objectClassReg)
        cg->stopUsingRegister(objectClassReg);
    if (lookupOffsetReg)
        cg->stopUsingRegister(lookupOffsetReg);

    if (lockedReg != NULL && lockedReg != vmThreadReg) {
        cg->stopUsingRegister(lockedReg);
    }

    return NULL;
}

TR::Register *J9::X86::TreeEvaluator::VMmonexitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // If there is a NULLCHK above this node it will be expecting us to set
    // up the excepting instruction.  If we are not going to inline an
    // appropriate excepting instruction we must make sure to reset the
    // excepting instruction since our children may have set it.
    //
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
    static const char *noInline = feGetEnv("TR_NoInlineMonitor");
    bool reservingLock = false;
    bool normalLockPreservingReservation = false;
    bool dummyMethodMonitor = false;
    bool gen64BitInstr = cg->comp()->target().is64Bit() && !fej9->generateCompressedLockWord();
    int lwOffset = fej9->getByteOffsetToLockword((TR_OpaqueClassBlock *)cg->getMonClass(node));
    TR_YesNoMaybe isMonitorValueBasedOrValueType = cg->isMonitorValueBasedOrValueType(node);

    if (comp->getOption(TR_MimicInterpreterFrameShape) || noInline || (isMonitorValueBasedOrValueType == TR_yes)
        || comp->getOption(TR_DisableInlineMonExit)) {
        // Don't inline
        //
        TR::ILOpCodes opCode = node->getOpCodeValue();
        TR::Node::recreate(node, TR::call);
        TR::TreeEvaluator::directCallEvaluator(node, cg);
        TR::Node::recreate(node, opCode);
        cg->setImplicitExceptionPoint(NULL);
        return NULL;
    }

    if (lwOffset > 0 && comp->getOption(TR_ReservingLocks)) {
        TR::TreeEvaluator::evaluateLockForReservation(node, &reservingLock, &normalLockPreservingReservation, cg);
        if (node->isPrimitiveLockedRegion() && reservingLock)
            dummyMethodMonitor = TR::TreeEvaluator::isDummyMonitorExit(node, cg);

        if (!node->isPrimitiveLockedRegion() && reservingLock)
            dummyMethodMonitor = false;
    }

    if (dummyMethodMonitor) {
        cg->decReferenceCount(node->getFirstChild());
        return NULL;
    }

    static const char *disableInlineRecursiveEnv = feGetEnv("TR_DisableInlineRecursiveMonitor");
    bool inlineRecursive = disableInlineRecursiveEnv ? false : true;
    if (lwOffset <= 0)
        inlineRecursive = false;

    // Evaluate the object reference
    //
    TR::Node *objectRef = node->getFirstChild();
    TR::Register *objectReg = cg->evaluate(objectRef);
    TR::Register *tempReg = NULL;
    uint32_t numDeps = 2; // objectReg, ebp

    cg->setImplicitExceptionPoint(NULL);
    TR::Register *vmThreadReg = cg->getVMThreadRegister();

    TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *fallThruLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *inlinedMonExitFallThruLabel = generateLabelSymbol(cg);
    // Create the monitor exit snippet
    TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);

    if (isMonitorValueBasedOrValueType == TR_maybe)
        TR::TreeEvaluator::generateCheckForValueMonitorEnterOrExit(node, J9_CLASS_DISALLOWS_LOCKING_FLAGS, snippetLabel,
            cg);

    startLabel->setStartInternalControlFlow();
    TR::LabelSymbol *snippetFallThruLabel = inlineRecursive ? generateLabelSymbol(cg) : fallThruLabel;
    fallThruLabel->setEndInternalControlFlow();
    Inst_Label(OP::label, node, startLabel, cg);

    TR::Register *eaxReal = NULL;
    TR::Register *unlockedReg = NULL;
    TR::Register *scratchReg = NULL;

    TR::Register *objectClassReg = NULL;
    TR::Register *lookupOffsetReg = NULL;

    TR::LabelSymbol *monitorLookupCacheLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *fallThruFromMonitorLookupCacheLabel = generateLabelSymbol(cg);

    if (lwOffset <= 0) {
        TR::MemoryReference *objectClassMR = MRef_Bdisp32(objectReg, TMP_OFFSETOF_J9OBJECT_CLAZZ, cg);
        OP::Mnemonic op = TR::Compiler->om.compressObjectReferences() ? OP::L4RegMem : OP::LRegMem();
        objectClassReg = cg->allocateRegister();

        TR::Instruction *instr = Inst_RegMem(op, node, objectClassReg, objectClassMR, cg);

        // this instruction may try to dereference a null memory address
        // add an implicit exception point for it.
        cg->setImplicitExceptionPoint(instr);
        instr->setNeedsGCMap(0xFF00FFFF);

        TR::TreeEvaluator::generateVFTMaskInstruction(node, objectClassReg, cg);
        int32_t offsetOfLockOffset = offsetof(J9Class, lockOffset);
        Inst_RegMem(OP::LRegMem(), node, objectClassReg, MRef_Bdisp32(objectClassReg, offsetOfLockOffset, cg), cg);

        numDeps++;

        Inst_RegImm(OP::CMP4RegImm4, node, objectClassReg, 0, cg);

        generateCommonLockNurseryCodes(node, cg,
            false, // true for VMmonentEvaluator, false for VMmonexitEvaluator
            monitorLookupCacheLabel, fallThruFromMonitorLookupCacheLabel, snippetLabel, numDeps, lwOffset,
            objectClassReg, lookupOffsetReg, vmThreadReg, objectReg);
    }

    // This is a normal inlined monitor exit
    //
    // Compare the monitor slot in the object against the thread register.
    // If it succeeds we are done. Else call the helper.
    //
    // Code generated:
    //    cmp   ebp, monitor(objectReg)
    //    jne   snippet
    //    test  flags(objectReg), FLC-bit     ; Only if FLC in separate word
    //    jne   snippet
    //    mov   monitor(objectReg), 0
    //    label restartLabel
    //
    // Code generated for read monitor:
    //    xor   unlockedReg, unlockedReg
    //    mov   eax, INC_DEC_VALUE
    //    (lock)cmpxchg   monitor(objectReg), unlockedReg
    //    jne   snippet
    //    label restartLabel
    //
    if (comp->getOption(TR_ReservingLocks)) {
        if (reservingLock) {
            tempReg = cg->allocateRegister();
            numDeps++;
        }

        if (reservingLock || normalLockPreservingReservation) {
            TR_RuntimeHelper helper;
            bool success = TR::TreeEvaluator::monEntryExitHelper(false, node, reservingLock,
                normalLockPreservingReservation, helper, cg);

            TR_ASSERT(success == true, "monEntryExitHelper: could not find runtime helper");

            node->setSymbolReference(comp->getSymRefTab()->findOrCreateRuntimeHelper(helper, true, true, true));
        }
    }

    TR_OutlinedInstructions *outlinedHelperCall = new (cg->trHeapMemory())
        TR_OutlinedInstructions(node, TR::call, NULL, snippetLabel, snippetFallThruLabel, cg);
    cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);
    cg->generateDebugCounter(outlinedHelperCall->getFirstInstruction(),
        TR::DebugCounter::debugCounterName(comp, "helperCalls/%s/(%s)/%d/%d", node->getOpCode().getName(),
            comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
        1, TR::DebugCounter::Cheap);

    if (inlineRecursive) {
        TR::LabelSymbol *inlineRecursiveSnippetLabel = generateLabelSymbol(cg);
        TR::LabelSymbol *jitMonitorExitSnippetLabel = snippetLabel;
        snippetLabel = inlineRecursiveSnippetLabel;
        TR::TreeEvaluator::inlineRecursiveMonitor(node, cg, fallThruLabel, jitMonitorExitSnippetLabel,
            inlineRecursiveSnippetLabel, objectReg, lwOffset, snippetFallThruLabel, reservingLock);
    }

    bool reservingDecrementNeeded = false;

    if (node->isReadMonitor()) {
        unlockedReg = cg->allocateRegister();
        eaxReal = cg->allocateRegister();
        Inst_RegReg(OP::XOR4RegReg, node, unlockedReg, unlockedReg, cg);
        Inst_RegImm(OP::MOVRegImm4(), node, eaxReal, INC_DEC_VALUE, cg);

        OP::Mnemonic op
            = cg->comp()->target().isSMP() ? OP::LCMPXCHGMemReg(gen64BitInstr) : OP::CMPXCHGMemReg(gen64BitInstr);
        cg->setImplicitExceptionPoint(
            Inst_MemReg(op, node, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), unlockedReg, cg));
        numDeps += 2;
    } else {
        if (reservingLock) {
            if (node->isPrimitiveLockedRegion()) {
                cg->setImplicitExceptionPoint(Inst_RegMem(OP::LRegMem(gen64BitInstr), node, tempReg,
                    getMemoryReference(objectClassReg, objectReg, lwOffset, cg), cg));

                // Mask out the thread ID and reservation count
                Inst_RegImm(OP::ANDRegImms(), node, tempReg, FLAGS_MASK, cg);
                // If only the RES flag is set and no other we can continue
                Inst_RegImm(OP::XORRegImms(), node, tempReg, RES_BIT, cg);
            } else {
                reservingDecrementNeeded = true;
                Inst_RegMem(OP::LEARegMem(), node, tempReg, MRef_Bdisp32(vmThreadReg, (REC_BIT | RES_BIT), cg), cg);
                cg->setImplicitExceptionPoint(Inst_MemReg(OP::CMPMemReg(gen64BitInstr), node,
                    getMemoryReference(objectClassReg, objectReg, lwOffset, cg), tempReg, cg));
            }
        } else {
            cg->setImplicitExceptionPoint(Inst_RegMem(OP::CMPRegMem(gen64BitInstr), node, vmThreadReg,
                getMemoryReference(objectClassReg, objectReg, lwOffset, cg), cg));
        }
    }

    TR::LabelSymbol *mismatchLabel
        = (reservingLock && !TR::Options::_aggressiveLockReservation) ? generateLabelSymbol(cg) : snippetLabel;

    Inst_Label(OP::JNE4, node, mismatchLabel, cg);

    if (reservingDecrementNeeded) {
        // Subtract the reservation count
        Inst_MemImm(OP::SUBMemImms(gen64BitInstr), node, getMemoryReference(objectClassReg, objectReg, lwOffset, cg),
            REC_BIT,
            cg); // I'm not sure OP::SUB4MemImms will work.
    }

    if (!node->isReadMonitor() && !reservingLock) {
        Inst_MemImm(OP::SMemImm4(gen64BitInstr), node, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), 0,
            cg);
    }

    if (reservingLock && !TR::Options::_aggressiveLockReservation) {
        Inst_Label(OP::JMP4, node, inlinedMonExitFallThruLabel, cg);

        // Avoid the helper for non-recursive exit in case it isn't reserved
        Inst_Label(OP::label, node, mismatchLabel, cg);
        auto lwMR = getMemoryReference(objectClassReg, objectReg, lwOffset, cg);
        Inst_MemReg(OP::CMPMemReg(gen64BitInstr), node, lwMR, vmThreadReg, cg);
        Inst_Label(OP::JNE4, node, snippetLabel, cg);
        lwMR = getMemoryReference(objectClassReg, objectReg, lwOffset, cg);
        Inst_MemImm(OP::SMemImm4(gen64BitInstr), node, lwMR, 0, cg);
    }

    Inst_Label(OP::label, node, inlinedMonExitFallThruLabel, cg);

#if defined(TR_TARGET_64BIT) && (JAVA_SPEC_VERSION >= 19)
    // Adjust J9VMThread ownedMonitorCount for execution paths that do not
    // go out of line to release the lock.  It is safe and efficient to do
    // this unconditionally.  There is no need to check for underflow.
    //
    Inst_Mem(OP::DEC8Mem, node, MRef_Bdisp32(vmThreadReg, fej9->thisThreadGetOwnedMonitorCountOffset(), cg), cg);
#endif

    // Create dependencies for the registers used.
    // The first dependencies must be objectReg, vmThreadReg, tempReg
    // Or, for readmonitors they must be objectReg, vmThreadReg, unlockedReg, eaxReal
    // snippet needs to find them to grab the real registers from them.
    //
    TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, (uint8_t)numDeps, cg);
    deps->addPostCondition(objectReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(vmThreadReg, TR::RealRegister::ebp, cg);

    if (node->isReadMonitor()) {
        deps->addPostCondition(unlockedReg, TR::RealRegister::NoReg, cg);
        deps->addPostCondition(eaxReal, TR::RealRegister::eax, cg);
    }

    if (lookupOffsetReg)
        deps->addPostCondition(lookupOffsetReg, TR::RealRegister::NoReg, cg);

    if (tempReg && !node->isReadMonitor())
        deps->addPostCondition(tempReg, TR::RealRegister::NoReg, cg);
    if (scratchReg)
        deps->addPostCondition(scratchReg, TR::RealRegister::NoReg, cg);
    if (objectClassReg)
        deps->addPostCondition(objectClassReg, TR::RealRegister::NoReg, cg);

    deps->stopAddingConditions();
    Inst_Label(OP::label, node, fallThruLabel, deps, cg);

    if (eaxReal)
        cg->stopUsingRegister(eaxReal);
    if (unlockedReg)
        cg->stopUsingRegister(unlockedReg);

    cg->decReferenceCount(objectRef);
    if (tempReg)
        cg->stopUsingRegister(tempReg);

    if (scratchReg)
        cg->stopUsingRegister(scratchReg);

    if (objectClassReg)
        cg->stopUsingRegister(objectClassReg);

    if (lookupOffsetReg)
        cg->stopUsingRegister(lookupOffsetReg);

    return NULL;
}

bool J9::X86::TreeEvaluator::monEntryExitHelper(bool entry, TR::Node *node, bool reservingLock,
    bool normalLockPreservingReservation, TR_RuntimeHelper &helper, TR::CodeGenerator *cg)
{
    bool methodMonitor = entry ? (node->getSymbolReference() == cg->getSymRef(TR_methodMonitorEntry))
                               : (node->getSymbolReference() == cg->getSymRef(TR_methodMonitorExit));

    if (reservingLock) {
        if (node->isPrimitiveLockedRegion()) {
            static TR_RuntimeHelper helpersCase1[2][2][2] = {
                {   { TR_IA32JitMonitorExitReservedPrimitive, TR_IA32JitMethodMonitorExitReservedPrimitive },
                 { TR_AMD64JitMonitorExitReservedPrimitive, TR_AMD64JitMethodMonitorExitReservedPrimitive }  },
                { { TR_IA32JitMonitorEnterReservedPrimitive, TR_IA32JitMethodMonitorEnterReservedPrimitive },
                 { TR_AMD64JitMonitorEnterReservedPrimitive, TR_AMD64JitMethodMonitorEnterReservedPrimitive } }
            };

            helper = helpersCase1[entry ? 1 : 0][cg->comp()->target().is64Bit() ? 1 : 0][methodMonitor ? 1 : 0];
            return true;
        } else {
            static TR_RuntimeHelper helpersCase2[2][2][2] = {
                {   { TR_IA32JitMonitorExitReserved, TR_IA32JitMethodMonitorExitReserved },
                 { TR_AMD64JitMonitorExitReserved, TR_AMD64JitMethodMonitorExitReserved }  },
                { { TR_IA32JitMonitorEnterReserved, TR_IA32JitMethodMonitorEnterReserved },
                 { TR_AMD64JitMonitorEnterReserved, TR_AMD64JitMethodMonitorEnterReserved } }
            };

            helper = helpersCase2[entry ? 1 : 0][cg->comp()->target().is64Bit() ? 1 : 0][methodMonitor ? 1 : 0];
            return true;
        }
    } else if (normalLockPreservingReservation) {
        static TR_RuntimeHelper helpersCase2[2][2][2] = {
            {   { TR_IA32JitMonitorExitPreservingReservation, TR_IA32JitMethodMonitorExitPreservingReservation },
             { TR_AMD64JitMonitorExitPreservingReservation, TR_AMD64JitMethodMonitorExitPreservingReservation }  },
            { { TR_IA32JitMonitorEnterPreservingReservation, TR_IA32JitMethodMonitorEnterPreservingReservation },
             { TR_AMD64JitMonitorEnterPreservingReservation, TR_AMD64JitMethodMonitorEnterPreservingReservation } }
        };

        helper = helpersCase2[entry ? 1 : 0][cg->comp()->target().is64Bit() ? 1 : 0][methodMonitor ? 1 : 0];
        return true;
    }

    return false;
}

/**
 * @brief Inserts a prefetch instruction at the given offset from a base register.
 *    Each successive prefetch instruction offsets its prefetch by one cache line
 *    (64 bytes) from the previous.
 *
 * @param[in] node : \c TR::Node associated with this prefetch
 * @param[in] numPrefetches : number of consecutive prefetch instructions to insert
 * @param[in] allocationReg : register containing the base address from which
 *               prefetches start
 * @param[in] offset : offset from \c allocationReg to begin prefetch
 * @param[in] cg : \c TR::CodeGenerator object
 */
static void insertAllocationPrefetch(TR::Node *node, int32_t numPrefetches, TR::Register *allocationReg, int32_t offset,
    TR::CodeGenerator *cg)
{
    for (int32_t i = 0; i < numPrefetches; i++) {
        Inst_Mem(OP::PREFETCHNTA, node, MRef_Bdisp32(allocationReg, offset, cg), cg);
        offset += 64;
    }
}

// Generate code to allocate discontiguous arrays or objects and arrays when
// using a realtime GC policy from the object heap.  Returns the register
// containing the address of the allocation.
//
// If the sizeReg is non-null, the allocation is variable length.  In this case
// the elementSize is meaningful and "size" is the extra size to be added.
// Otherwise "size" contains the total size of the allocation.
//
// Also, on return the "segmentReg" register is set to the address of the
// memory segment.
//
static void genHeapAllocForDiscontiguousArraysOrRealtime(TR::Node *node, TR_OpaqueClassBlock *clazz,
    int32_t allocationSizeOrDataOffset, int32_t elementSize, TR::Register *sizeReg, TR::Register *eaxReal,
    TR::Register *segmentReg, TR::Register *tempReg, TR::LabelSymbol *failLabel, TR::CodeGenerator *cg)
{
    // Load the current heap segment and see if there is room in it. Loop if
    // we can't get the lock on the segment.
    //
    TR::Compilation *comp = cg->comp();
    TR::Register *vmThreadReg = cg->getVMThreadRegister();
    bool generateArraylets = comp->generateArraylets();

    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

    if (comp->getOptions()->realTimeGC()) {
#if defined(J9VM_GC_REALTIME)
        // this will be bogus for variable length allocations because it only includes the header size (+ arraylet ptr
        // for arrays)
        UDATA sizeClass = fej9->getObjectSizeClass(allocationSizeOrDataOffset);

        if (comp->getOption(TR_BreakOnNew))
            Inst(OP::INT3, node, cg);

        // heap allocation, so proceed
        if (sizeReg) {
            Inst_RegReg(OP::XOR4RegReg, node, eaxReal, eaxReal, cg);

            // make sure size isn't too big
            // convert max object size to num elements because computing an object size from num elements may overflow
            TR_ASSERT(fej9->getMaxObjectSizeForSizeClass() <= UINT_MAX, "assertion failure");
            Inst_RegImm(OP::CMPRegImm4(), node, sizeReg,
                (fej9->getMaxObjectSizeForSizeClass() - allocationSizeOrDataOffset) / elementSize, cg);
            Inst_Label(OP::JA4, node, failLabel, cg);

            // Hybrid arraylets need a zero length test if the size is unknown.
            //
            if (!generateArraylets) {
                Inst_RegReg(OP::TEST4RegReg, node, sizeReg, sizeReg, cg);
                Inst_Label(OP::JE4, node, failLabel, cg);
            }

            // need to round up to sizeof(UDATA) so we can use it to index into size class index array
            // conservatively just add sizeof(UDATA) bytes and round
            int32_t round = 0;
            if (elementSize < sizeof(UDATA))
                round = sizeof(UDATA) - 1;

            // now compute size of object in bytes
            Inst_RegMem(OP::LEARegMem(), node, segmentReg,
                MRef_BISdisp32(eaxReal, sizeReg, TR::MemoryReference::convertMultiplierToStride(elementSize),
                    allocationSizeOrDataOffset + round, cg),
                cg);

            if (elementSize < sizeof(UDATA))
                Inst_RegImm(OP::ANDRegImms(), node, segmentReg, -(int32_t)sizeof(UDATA), cg);

#ifdef J9VM_INTERP_FLAGS_IN_CLASS_SLOT
            Inst_RegImm(OP::CMPRegImm4(), node, segmentReg, J9_GC_MINIMUM_OBJECT_SIZE, cg);
            TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
            Inst_Label(OP::JAE4, node, doneLabel, cg);
            Inst_RegImm(OP::MOVRegImm4(), node, segmentReg, J9_GC_MINIMUM_OBJECT_SIZE, cg);
            Inst_Label(OP::label, node, doneLabel, cg);
#endif

            // get size class
            Inst_RegMem(OP::LRegMem(), node, tempReg, MRef_Bdisp32(vmThreadReg, fej9->thisThreadJavaVMOffset(), cg),
                cg);
            Inst_RegMem(OP::LRegMem(), node, tempReg, MRef_Bdisp32(tempReg, fej9->getRealtimeSizeClassesOffset(), cg),
                cg);
            Inst_RegMem(OP::LRegMem(), node, tempReg,
                MRef_BISdisp32(tempReg, segmentReg, TR::MemoryReference::convertMultiplierToStride(1),
                    fej9->getSizeClassesIndexOffset(), cg),
                cg);

            // tempReg now holds size class
            TR::MemoryReference *currentMemRef, *topMemRef, *currentMemRefBump;
            if (cg->comp()->target().is64Bit()) {
                TR_ASSERT(sizeof(J9VMGCSegregatedAllocationCacheEntry) == 16,
                    "unexpected J9VMGCSegregatedAllocationCacheEntry size");
                // going to play some games here
                //   need to use tempReg to index into two arrays:
                //        1) allocation caches
                //        2) cell size array
                //   The first one has stride 16, second one stride sizeof(UDATA)
                //   We need a shift instruction to be able to do stride 16
                //   To avoid two shifts, only do one for stride sizeof(UDATA) and use a multiplier in memory ref for 16
                //   64-bit, so shift 3 times for sizeof(UDATA) and use multiplier stride 2 in memory references
                Inst_RegImm(OP::SHLRegImm1(), node, tempReg, 3, cg);
                currentMemRef = MRef_BISdisp32(vmThreadReg, tempReg, TR::MemoryReference::convertMultiplierToStride(2),
                    fej9->thisThreadAllocationCacheCurrentOffset(0), cg);
                topMemRef = MRef_BISdisp32(vmThreadReg, tempReg, TR::MemoryReference::convertMultiplierToStride(2),
                    fej9->thisThreadAllocationCacheTopOffset(0), cg);
                currentMemRefBump
                    = MRef_BISdisp32(vmThreadReg, tempReg, TR::MemoryReference::convertMultiplierToStride(2),
                        fej9->thisThreadAllocationCacheCurrentOffset(0), cg);
            } else {
                // size needs to be 8 or less or it there's no multiplier stride available (would need to use other
                // branch of else)
                TR_ASSERT(sizeof(J9VMGCSegregatedAllocationCacheEntry) <= 8,
                    "unexpected J9VMGCSegregatedAllocationCacheEntry size");

                currentMemRef = MRef_BISdisp32(vmThreadReg, tempReg,
                    TR::MemoryReference::convertMultiplierToStride(sizeof(J9VMGCSegregatedAllocationCacheEntry)),
                    fej9->thisThreadAllocationCacheCurrentOffset(0), cg);
                topMemRef = MRef_BISdisp32(vmThreadReg, tempReg,
                    TR::MemoryReference::convertMultiplierToStride(sizeof(J9VMGCSegregatedAllocationCacheEntry)),
                    fej9->thisThreadAllocationCacheTopOffset(0), cg);
                currentMemRefBump = MRef_BISdisp32(vmThreadReg, tempReg,
                    TR::MemoryReference::convertMultiplierToStride(sizeof(J9VMGCSegregatedAllocationCacheEntry)),
                    fej9->thisThreadAllocationCacheCurrentOffset(0), cg);
            }
            // tempReg now contains size class (32-bit) or size class * sizeof(J9VMGCSegregatedAllocationCacheEntry)
            // (64-bit)

            // get next cell for this size class
            Inst_RegMem(OP::LRegMem(), node, eaxReal, currentMemRef, cg);

            // if null, then no cell available, use slow path
            Inst_RegMem(OP::CMPRegMem(), node, eaxReal, topMemRef, cg);
            Inst_Label(OP::JAE4, node, failLabel, cg);

            // have a valid cell, need to update current cell pointer
            Inst_RegMem(OP::LRegMem(), node, segmentReg, MRef_Bdisp32(vmThreadReg, fej9->thisThreadJavaVMOffset(), cg),
                cg);
            Inst_RegMem(OP::LRegMem(), node, segmentReg,
                MRef_Bdisp32(segmentReg, fej9->getRealtimeSizeClassesOffset(), cg), cg);
            if (cg->comp()->target().is64Bit()) {
                // tempReg already has already been shifted for sizeof(UDATA)
                Inst_RegMem(OP::LRegMem(), node, segmentReg,
                    MRef_BISdisp32(segmentReg, tempReg, TR::MemoryReference::convertMultiplierToStride(1),
                        fej9->getSmallCellSizesOffset(), cg),
                    cg);
            } else {
                // tempReg needs to be shifted for sizeof(UDATA)
                Inst_RegMem(OP::LRegMem(), node, segmentReg,
                    MRef_BISdisp32(segmentReg, tempReg, TR::MemoryReference::convertMultiplierToStride(sizeof(UDATA)),
                        fej9->getSmallCellSizesOffset(), cg),
                    cg);
            }
            // segmentReg now holds cell size

            // update current cell by cell size
            Inst_MemReg(OP::ADDMemReg(), node, currentMemRefBump, segmentReg, cg);
        } else {
            Inst_RegMem(OP::LRegMem(), node, eaxReal,
                MRef_Bdisp32(vmThreadReg, fej9->thisThreadAllocationCacheCurrentOffset(sizeClass), cg), cg);

            Inst_RegMem(OP::CMPRegMem(), node, eaxReal,
                MRef_Bdisp32(vmThreadReg, fej9->thisThreadAllocationCacheTopOffset(sizeClass), cg), cg);

            Inst_Label(OP::JAE4, node, failLabel, cg);

            // we have an object in eaxReal, now bump the current updatepointer
            OP::Mnemonic opcode;
            uint32_t cellSize = fej9->getCellSizeForSizeClass(sizeClass);
            if (cellSize <= 127)
                opcode = OP::ADDMemImms();
            else if (cellSize == 128) {
                opcode = OP::SUBMemImms();
                cellSize = (uint32_t)-128;
            } else
                opcode = OP::ADDMemImm4();

            Inst_MemImm(opcode, node,
                MRef_Bdisp32(vmThreadReg, fej9->thisThreadAllocationCacheCurrentOffset(sizeClass), cg), cellSize, cg);
        }

        // we're done
        return;
#endif
    } else {
        bool isSmallAllocation = false;

        size_t heapAlloc_offset = offsetof(J9VMThread, heapAlloc);
        size_t heapTop_offset = offsetof(J9VMThread, heapTop);
        size_t tlhPrefetchFTA_offset = offsetof(J9VMThread, tlhPrefetchFTA);
#ifdef J9VM_GC_NON_ZERO_TLH
        if (!comp->getOption(TR_DisableDualTLH) && node->canSkipZeroInitialization()) {
            heapAlloc_offset = offsetof(J9VMThread, nonZeroHeapAlloc);
            heapTop_offset = offsetof(J9VMThread, nonZeroHeapTop);
            tlhPrefetchFTA_offset = offsetof(J9VMThread, nonZeroTlhPrefetchFTA);
        }
#endif
        // Load the base of the next available heap storage.  This load is done speculatively on the assumption that the
        // allocation will be inlined.  If the assumption turns out to be false then the performance impact should be
        // minimal because the helper will be called in that case.  It is necessary to insert this load here so that it
        // dominates all control paths through this internal control flow region.
        //
        Inst_RegMem(OP::LRegMem(), node, eaxReal, MRef_Bdisp32(vmThreadReg, heapAlloc_offset, cg), cg);

        bool canSkipOverflowCheck = false;

        // If the array length is constant, check to see if the size of the array will fit in a single arraylet leaf.
        // If the allocation size is too large, call the snippet.
        //
        if (generateArraylets && (node->getOpCodeValue() == TR::anewarray || node->getOpCodeValue() == TR::newarray)) {
            if (comp->getOption(TR_DisableTarokInlineArrayletAllocation))
                Inst_Label(OP::JMP4, node, failLabel, cg);

            if (sizeReg) {
                uint32_t maxContiguousArrayletLeafSizeInBytes
                    = (uint32_t)(TR::Compiler->om.arrayletLeafSize() - TR::Compiler->om.sizeofReferenceAddress());

                int32_t maxArrayletSizeInElements = maxContiguousArrayletLeafSizeInBytes / elementSize;

                // Hybrid arraylets need a zero length test if the size is unknown.
                //
                Inst_RegReg(OP::TEST4RegReg, node, sizeReg, sizeReg, cg);
                Inst_Label(OP::JE4, node, failLabel, cg);

                Inst_RegImm(OP::CMP4RegImm4, node, sizeReg, maxArrayletSizeInElements, cg);
                Inst_Label(OP::JAE4, node, failLabel, cg);

                // If the max arraylet leaf size is less than the amount of free space available on
                // the stack, there is no need to check for an overflow scenario.
                //
                if (maxContiguousArrayletLeafSizeInBytes <= cg->getMaxObjectSizeGuaranteedNotToOverflow())
                    canSkipOverflowCheck = true;
            } else if (TR::Compiler->om.isDiscontiguousArray(allocationSizeOrDataOffset)) {
                // TODO: just call the helper directly and don't generate any
                //       further instructions.
                //
                // Actually, we should never get here because we've already checked
                // constant lengths for discontiguity...
                //
                Inst_Label(OP::JMP4, node, failLabel, cg);
            }
        }

        if (sizeReg && !canSkipOverflowCheck) {
            // Hybrid arraylets need a zero length test if the size is unknown.
            // The length could be zero.
            //
            if (!generateArraylets) {
                Inst_RegReg(OP::TEST4RegReg, node, sizeReg, sizeReg, cg);
                Inst_Label(OP::JE4, node, failLabel, cg);
            }

            // The GC will guarantee that at least 'maxObjectSizeGuaranteedNotToOverflow' bytes
            // of slush will exist between the top of the heap and the end of the address space.
            //
            uintptr_t maxObjectSize = cg->getMaxObjectSizeGuaranteedNotToOverflow();
            uintptr_t maxObjectSizeInElements = maxObjectSize / elementSize;

            if (cg->comp()->target().is64Bit()
                && !(maxObjectSizeInElements > 0 && maxObjectSizeInElements <= (uintptr_t)INT_MAX)) {
                Inst_RegImm64(OP::MOV8RegImm64, node, tempReg, maxObjectSizeInElements, cg);
                Inst_RegReg(OP::CMP8RegReg, node, sizeReg, tempReg, cg);
            } else {
                Inst_RegImm(OP::CMP4RegImm4, node, sizeReg, (int32_t)maxObjectSizeInElements, cg);
            }

            // Must be an unsigned comparison on sizes.
            //
            Inst_Label(OP::JAE4, node, failLabel, cg);
        }

#if !defined(J9VM_GC_THREAD_LOCAL_HEAP)
        // Establish a loop label in case the new heap pointer cannot be committed.
        //
        TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);
        Inst_Label(OP::label, node, loopLabel, cg);
#endif

        if (sizeReg) {
            // calculate variable size, rounding up if necessary to a intptr_t multiple boundary
            //
            int32_t round; // zero indicates no rounding is necessary

            if (!generateArraylets) {
                //            TR_ASSERT(allocationSizeOrDataOffset % fej9->getObjectAlignmentInBytes() == 0, "Array
                //            header size of %d is not a multiple of %d", allocationSizeOrDataOffset,
                //            fej9->getObjectAlignmentInBytes());
            }

            round = (elementSize < TR::Compiler->om.getObjectAlignmentInBytes())
                ? TR::Compiler->om.getObjectAlignmentInBytes()
                : 0;

            int32_t disp32 = round ? (round - 1) : 0;
#ifdef J9VM_INTERP_FLAGS_IN_CLASS_SLOT
            if ((node->getOpCodeValue() == TR::anewarray || node->getOpCodeValue() == TR::newarray)) {
                // All arrays in combo builds will always be at least 12 bytes in size in all specs if
                // dual header shape is enabled and 20 bytes otherwise:
                //
                // Dual header shape is enabled:
                // 1)  class pointer + contig length + one or more elements
                // 2)  class pointer + 0 + 0 (for zero length arrays)
                //
                // Dual header shape is disabled:
                // 1)  class pointer + contig length + dataAddr + one or more elements
                // 2)  class pointer + 0 + 0 (for zero length arrays) + dataAddr
                //
                // Since objects are aligned to 8 bytes then the minimum size for an array must be 16 and 24 after
                // rounding
                TR_ASSERT(J9_GC_MINIMUM_OBJECT_SIZE >= 8,
                    "Expecting a minimum indexable object size >= 8 (actual minimum is %d)\n",
                    J9_GC_MINIMUM_OBJECT_SIZE);

                Inst_RegMem(OP::LEARegMem(), node, tempReg,
                    MRef_BISdisp32(eaxReal, sizeReg, TR::MemoryReference::convertMultiplierToStride(elementSize),
                        allocationSizeOrDataOffset + disp32, cg),
                    cg);

                if (round) {
                    Inst_RegImm(OP::ANDRegImm4(), node, tempReg, -round, cg);
                }
            } else
#endif
            {
#ifdef J9VM_INTERP_FLAGS_IN_CLASS_SLOT
                Inst_RegReg(OP::XOR4RegReg, node, tempReg, tempReg, cg);
#endif

                Inst_RegMem(OP::LEARegMem(), node, tempReg,
                    MRef_BISdisp32(
#ifdef J9VM_INTERP_FLAGS_IN_CLASS_SLOT
                        tempReg,
#else
                        eaxReal,
#endif
                        sizeReg, TR::MemoryReference::convertMultiplierToStride(elementSize),
                        allocationSizeOrDataOffset + disp32, cg),
                    cg);

                if (round) {
                    Inst_RegImm(OP::ANDRegImm4(), node, tempReg, -round, cg);
                }

#ifdef J9VM_INTERP_FLAGS_IN_CLASS_SLOT
                Inst_RegImm(OP::CMPRegImm4(), node, tempReg, J9_GC_MINIMUM_OBJECT_SIZE, cg);
                TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
                Inst_Label(OP::JAE4, node, doneLabel, cg);
                Inst_RegImm(OP::MOVRegImm4(), node, tempReg, J9_GC_MINIMUM_OBJECT_SIZE, cg);
                Inst_Label(OP::label, node, doneLabel, cg);
                Inst_RegReg(OP::ADDRegReg(), node, tempReg, eaxReal, cg);
#endif
            }
        } else {
            isSmallAllocation = allocationSizeOrDataOffset <= 0x40 ? true : false;
            allocationSizeOrDataOffset = (allocationSizeOrDataOffset + TR::Compiler->om.getObjectAlignmentInBytes() - 1)
                & (-TR::Compiler->om.getObjectAlignmentInBytes());

            if ((uint32_t)allocationSizeOrDataOffset > cg->getMaxObjectSizeGuaranteedNotToOverflow()) {
                Inst_RegReg(OP::MOVRegReg(), node, tempReg, eaxReal, cg);
                if (allocationSizeOrDataOffset <= 127)
                    Inst_RegImm(OP::ADDRegImms(), node, tempReg, allocationSizeOrDataOffset, cg);
                else if (allocationSizeOrDataOffset == 128)
                    Inst_RegImm(OP::SUBRegImms(), node, tempReg, (unsigned)-128, cg);
                else
                    Inst_RegImm(OP::ADDRegImm4(), node, tempReg, allocationSizeOrDataOffset, cg);

                // Check for overflow
                Inst_Label(OP::JB4, node, failLabel, cg);
            } else {
                Inst_RegMem(OP::LEARegMem(), node, tempReg, MRef_Bdisp32(eaxReal, allocationSizeOrDataOffset, cg), cg);
            }
        }

        Inst_RegMem(OP::CMPRegMem(), node, tempReg, MRef_Bdisp32(vmThreadReg, heapTop_offset, cg), cg);

        Inst_Label(OP::JA4, node, failLabel, cg);

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)

        // Make sure that the arraylet is aligned properly.
        //
        if (generateArraylets && (node->getOpCodeValue() == TR::anewarray || node->getOpCodeValue() == TR::newarray)) {
            Inst_RegMem(OP::LEARegMem(), node, tempReg,
                MRef_Bdisp32(tempReg, TR::Compiler->om.getObjectAlignmentInBytes() - 1, cg), cg);
            if (cg->comp()->target().is64Bit())
                Inst_RegImm(OP::AND8RegImm4, node, tempReg, -TR::Compiler->om.getObjectAlignmentInBytes(), cg);
            else
                Inst_RegImm(OP::AND4RegImm4, node, tempReg, -TR::Compiler->om.getObjectAlignmentInBytes(), cg);
        }

        Inst_MemReg(OP::SMemReg(), node, MRef_Bdisp32(vmThreadReg, heapAlloc_offset, cg), tempReg, cg);

        if (!isSmallAllocation && cg->enableTLHPrefetching()) {
            TR::LabelSymbol *prefetchSnippetLabel = generateLabelSymbol(cg);
            TR::LabelSymbol *restartLabel = generateLabelSymbol(cg);
            cg->addSnippet(new (cg->trHeapMemory()) TR::X86AllocPrefetchSnippet(cg, node, TR::Options::_TLHPrefetchSize,
                restartLabel, prefetchSnippetLabel,
                (!comp->getOption(TR_DisableDualTLH) && node->canSkipZeroInitialization())));

            bool useDirectPrefetchCall = false;
            bool useSharedCodeCacheSnippet = fej9->supportsCodeCacheSnippets();

            // Generate the prefetch thunk in code cache.  Only generate this once.
            //
            bool prefetchThunkGenerated = (fej9->getAllocationPrefetchCodeSnippetAddress(comp) != 0);
#ifdef J9VM_GC_NON_ZERO_TLH
            if (!comp->getOption(TR_DisableDualTLH) && node->canSkipZeroInitialization()) {
                prefetchThunkGenerated = (fej9->getAllocationNoZeroPrefetchCodeSnippetAddress(comp) != 0);
            }
#endif
            if (useSharedCodeCacheSnippet && prefetchThunkGenerated) {
                useDirectPrefetchCall = true;
            }

            Inst_RegReg(OP::SUB4RegReg, node, tempReg, eaxReal, cg);

            Inst_MemReg(OP::SUB4MemReg, node, MRef_Bdisp32(vmThreadReg, tlhPrefetchFTA_offset, cg), tempReg, cg);
            if (!useDirectPrefetchCall)
                Inst_Label(OP::JLE4, node, prefetchSnippetLabel, cg);
            else {
                Inst_Label(OP::JG4, node, restartLabel, cg);
                TR::SymbolReference *helperSymRef
                    = cg->getSymRefTab()->findOrCreateRuntimeHelper(TR_X86CodeCachePrefetchHelper);
                TR::MethodSymbol *helperSymbol = helperSymRef->getSymbol()->castToMethodSymbol();
#ifdef J9VM_GC_NON_ZERO_TLH
                if (!comp->getOption(TR_DisableDualTLH) && node->canSkipZeroInitialization()) {
                    helperSymbol->setMethodAddress(fej9->getAllocationNoZeroPrefetchCodeSnippetAddress(comp));
                } else {
                    helperSymbol->setMethodAddress(fej9->getAllocationPrefetchCodeSnippetAddress(comp));
                }
#else
                helperSymbol->setMethodAddress(fej9->getAllocationPrefetchCodeSnippetAddress(comp));
#endif
                Inst_ImmSym(OP::CALLImm4, node, (uintptr_t)helperSymbol->getMethodAddress(), helperSymRef, cg);
            }

            Inst_Label(OP::label, node, restartLabel, cg);
        }

#else // J9VM_GC_THREAD_LOCAL_HEAP
        Inst_MemReg(OP::CMPXCHGMemReg(), node, MRef_Bdisp32(vmThreadReg, heapAlloc_offset, cg), tempReg, cg);
        Inst_Label(OP::JNE4, node, loopLabel, cg);
#endif // !J9VM_GC_THREAD_LOCAL_HEAP
    }
}

// ------------------------------------------------------------------------------
// genHeapAllocForObjectOrHybridArraylet
//
// Needs 2TLH support.
// ------------------------------------------------------------------------------

/**
 * @param[in] eaxReal : address of the newly allocated object
 * @param[in] nextTLHAllocReg : the new TLH alloc pointer after the object is allocated
 */

static void genHeapAllocForObjectOrHybridArraylet(TR::Node *node, TR_OpaqueClassBlock *clazz,
    int32_t allocationSizeOrDataOffset, int32_t elementSize, TR::Register *sizeReg, TR::Register *eaxReal,
    TR::Register *nextTLHAllocReg, TR::Register *tempReg, TR::LabelSymbol *failLabel, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR::Register *vmThreadReg = cg->getVMThreadRegister();

    TR_ASSERT_FATAL(!comp->generateArraylets(), "This function can only handle hybrid arraylets");

    bool isTooSmallToPrefetch = false;

    if (sizeReg) {
        // -------------
        // VARIABLE SIZE
        // -------------

        // The GC will guarantee that at least 'maxObjectSizeGuaranteedNotToOverflow' bytes
        // of slush will exist between the top of the heap and the end of the address space.
        //
        uintptr_t maxObjectSize = cg->getMaxObjectSizeGuaranteedNotToOverflow();
        uintptr_t maxObjectSizeInElements = maxObjectSize / elementSize;

        if (comp->target().is64Bit()
            && !(maxObjectSizeInElements > 0 && maxObjectSizeInElements <= (uintptr_t)INT_MAX)) {
            // nextTLHAllocReg can be used as a scratch register until it is defined below
            //
            Inst_RegImm64(OP::MOV8RegImm64, node, nextTLHAllocReg, maxObjectSizeInElements, cg);
            Inst_RegReg(OP::CMP8RegReg, node, sizeReg, nextTLHAllocReg, cg);
        } else {
            Inst_RegImm(OP::CMP4RegImm4, node, sizeReg, (int32_t)maxObjectSizeInElements, cg);
        }

        // Must be an unsigned comparison on sizes.
        //
        Inst_Label(OP::JAE4, node, failLabel, cg);

        Inst_RegMem(OP::LRegMem(), node, eaxReal, MRef_Bdisp32(vmThreadReg, offsetof(J9VMThread, heapAlloc), cg), cg);

        Inst_RegReg(OP::MOV4RegReg, node, nextTLHAllocReg, sizeReg, cg);

        // Artificially adjust the number of elements by 1 if the array is zero length.  This works
        // because either the array is zero length and needs a discontiguous array length field
        // (occupying a slot) or it has at least 1 element which will take up a slot anyway.
        //
        // Native 64-bit array headers do not need this adjustment because the
        // contiguous and discontiguous array headers are the same size.
        //
        if (comp->target().is32Bit() || (comp->target().is64Bit() && comp->useCompressedPointers())) {
            Inst_RegImm(OP::CMP4RegImm4, node, nextTLHAllocReg, 1, cg);
            Inst_RegImm(OP::ADC4RegImm4, node, nextTLHAllocReg, 0, cg);
        }

        uint8_t shiftVal = TR::MemoryReference::convertMultiplierToStride(elementSize);
        if (shiftVal > 0) {
            Inst_RegImm(OP::SHLRegImm1(), node, nextTLHAllocReg, shiftVal, cg);
        }

        // calculate variable size, rounding up if necessary to a intptr_t multiple boundary
        //
        // zero round indicates no rounding is necessary
        //
        int32_t round = (elementSize >= TR::Compiler->om.getObjectAlignmentInBytes())
            ? 0
            : TR::Compiler->om.getObjectAlignmentInBytes();
        int32_t disp32 = round ? (round - 1) : 0;

        Inst_RegImm(OP::ADDRegImm4(), node, nextTLHAllocReg, allocationSizeOrDataOffset + disp32, cg);

        if (round) {
            Inst_RegImm(OP::ANDRegImm4(), node, nextTLHAllocReg, -round, cg);
        }

        // Copy full object size in bytes to RCX for zero init via REP OP::STOSQ
        //
        Inst_RegReg(OP::MOVRegReg(), node, tempReg, nextTLHAllocReg, cg);

        Inst_RegReg(OP::ADDRegReg(), node, nextTLHAllocReg, eaxReal, cg);
    } else {
        // ----------
        // FIXED SIZE
        // ----------

        Inst_RegMem(OP::LRegMem(), node, eaxReal, MRef_Bdisp32(vmThreadReg, offsetof(J9VMThread, heapAlloc), cg), cg);

        if (comp->getOptLevel() < hot)
            isTooSmallToPrefetch = allocationSizeOrDataOffset <= 0x40 ? true : false;

        allocationSizeOrDataOffset = (allocationSizeOrDataOffset + TR::Compiler->om.getObjectAlignmentInBytes() - 1)
            & (-TR::Compiler->om.getObjectAlignmentInBytes());

        if ((uint32_t)allocationSizeOrDataOffset > cg->getMaxObjectSizeGuaranteedNotToOverflow()) {
            Inst_RegReg(OP::MOVRegReg(), node, nextTLHAllocReg, eaxReal, cg);
            if (allocationSizeOrDataOffset <= 127)
                Inst_RegImm(OP::ADDRegImms(), node, nextTLHAllocReg, allocationSizeOrDataOffset, cg);
            else if (allocationSizeOrDataOffset == 128)
                Inst_RegImm(OP::SUBRegImms(), node, nextTLHAllocReg, (unsigned)-128, cg);
            else
                Inst_RegImm(OP::ADDRegImm4(), node, nextTLHAllocReg, allocationSizeOrDataOffset, cg);

            // Check for overflow
            Inst_Label(OP::JB4, node, failLabel, cg);
        } else {
            Inst_RegMem(OP::LEARegMem(), node, nextTLHAllocReg, MRef_Bdisp32(eaxReal, allocationSizeOrDataOffset, cg),
                cg);
        }
    }

    // -----------
    // MERGED PATH
    // -----------

    Inst_RegMem(OP::CMPRegMem(), node, nextTLHAllocReg, MRef_Bdisp32(vmThreadReg, offsetof(J9VMThread, heapTop), cg),
        cg);

    Inst_Label(OP::JA4, node, failLabel, cg);

    if (!isTooSmallToPrefetch && cg->enableTLHPrefetching()) {
        insertAllocationPrefetch(node, 1, nextTLHAllocReg, 0xc0, cg);
    }

    Inst_MemReg(OP::SMemReg(), node, MRef_Bdisp32(vmThreadReg, offsetof(J9VMThread, heapAlloc), cg), nextTLHAllocReg,
        cg);

    if (!isTooSmallToPrefetch && node->getOpCodeValue() != TR::New && cg->enableTLHPrefetching()) {
        insertAllocationPrefetch(node, 3, nextTLHAllocReg, 0x100, cg);
    }
}

// Generate the code to initialize an object header - used for both new and
// array new
//
static void genInitObjectHeader(TR::Node *node, TR_OpaqueClassBlock *clazz, TR::Register *classReg,
    TR::Register *objectReg, TR::Register *tempReg, bool isZeroInitialized, bool isDynamicAllocation,
    TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

    bool use64BitClasses = comp->target().is64Bit()
        && (!TR::Compiler->om.generateCompressedObjectHeaders()
            || (comp->compileRelocatableCode() && comp->getOption(TR_UseSymbolValidationManager)));

    TR_ASSERT((isDynamicAllocation || clazz), "Cannot have a null clazz while not doing dynamic array allocation\n");

    // --------------------------------------------------------------------------------
    //
    // Initialize CLASS field
    //
    // --------------------------------------------------------------------------------
    //
    OP::Mnemonic opSMemReg = OP::SMemReg(use64BitClasses);

    TR::Register *clzReg = classReg;

    // For dynamic array allocation, load the array class from the component class and store into clzReg
    if (isDynamicAllocation) {
        TR_ASSERT((node->getOpCodeValue() == TR::anewarray),
            "Dynamic allocation currently only supports reference arrays");
        TR_ASSERT(classReg, "must have a classReg for dynamic allocation");
        clzReg = tempReg;
        Inst_RegMem(OP::LRegMem(), node, clzReg, MRef_Bdisp32(classReg, offsetof(J9Class, arrayClass), cg), cg);
    }
    // TODO: should be able to use a TR_ClassPointer relocation without this stuff (along with class validation)
    else if (cg->needClassAndMethodPointerRelocations() && !comp->getOption(TR_UseSymbolValidationManager)) {
        TR::Register *vmThreadReg = cg->getVMThreadRegister();
        if (node->getOpCodeValue() == TR::newarray) {
            Inst_RegMem(OP::LRegMem(), node, tempReg, MRef_Bdisp32(vmThreadReg, offsetof(J9VMThread, javaVM), cg), cg);
            Inst_RegMem(OP::LRegMem(), node, tempReg,
                MRef_Bdisp32(tempReg,
                    offsetof(J9JavaVM, booleanArrayClass) + (node->getSecondChild()->getInt() - 4) * sizeof(J9Class *),
                    cg),
                cg);
            // tempReg should contain a 32 bit pointer.
            Inst_MemReg(opSMemReg, node, MRef_Bdisp32(objectReg, TR::Compiler->om.offsetOfObjectVftField(), cg),
                tempReg, cg);
            clzReg = tempReg;
        } else {
            TR_ASSERT_FATAL((node->getOpCodeValue() == TR::New) && classReg,
                "Must have a classReg for TR::New in non-SVM AOT mode");
            clzReg = classReg;
        }
    }

    // For RealTime Code Only.
    int32_t orFlags = 0;
    int32_t orFlagsClass = 0;

    if (!clzReg) {
        TR::Instruction *instr = NULL;
        if (use64BitClasses) {
            if (cg->needClassAndMethodPointerRelocations() && comp->getOption(TR_UseSymbolValidationManager))
                instr = Inst_RegImm64(OP::MOV8RegImm64, node, tempReg, ((intptr_t)clazz | orFlagsClass), cg,
                    TR_ClassPointer);
            else
                instr = Inst_RegImm64(OP::MOV8RegImm64, node, tempReg, ((intptr_t)clazz | orFlagsClass), cg);
            Inst_MemReg(OP::S8MemReg, node, MRef_Bdisp32(objectReg, TR::Compiler->om.offsetOfObjectVftField(), cg),
                tempReg, cg);
        } else {
            instr = Inst_MemImm(OP::S4MemImm4, node,
                MRef_Bdisp32(objectReg, TR::Compiler->om.offsetOfObjectVftField(), cg),
                (int32_t)((uintptr_t)clazz | orFlagsClass), cg);
        }
    } else {
        if (orFlagsClass != 0)
            Inst_RegImm(use64BitClasses ? OP::OR8RegImm4 : OP::OR4RegImm4, node, clzReg, orFlagsClass, cg);
        Inst_MemReg(opSMemReg, node, MRef_Bdisp32(objectReg, TR::Compiler->om.offsetOfObjectVftField(), cg), clzReg,
            cg);
    }

    // --------------------------------------------------------------------------------
    //
    // Initialize FLAGS field
    //
    // --------------------------------------------------------------------------------
    //

    // Collect the flags to be OR'd in that are known at compile time.
    //

#ifndef J9VM_INTERP_FLAGS_IN_CLASS_SLOT
    // Enable macro once GC-Helper is fixed
    J9ROMClass *romClass = TR::Compiler->cls.romClassOf(clazz);
    if (romClass) {
        orFlags |= romClass->instanceShape;
        orFlags |= fej9->getStaticObjectFlags();

#if defined(J9VM_OPT_NEW_OBJECT_HASH)
        // put orFlags or 0 into header if needed
        Inst_MemImm(OP::S4MemImm4, node, MRef_Bdisp32(objectReg, TMP_OFFSETOF_J9OBJECT_FLAGS, cg), orFlags, cg);

#endif /* !J9VM_OPT_NEW_OBJECT_HASH */
    }
#endif /* FLAGS_IN_CLASS_SLOT */

    // --------------------------------------------------------------------------------
    //
    // Initialize MONITOR field
    //
    // --------------------------------------------------------------------------------
    //
    // For dynamic array allocation, in case (very unlikely) the object array has a lock word, we just initialized it to
    // 0 conservatively. In this case, if the original array is reserved, initializing the cloned object's lock word to
    // 0 will force the locking to go to the slow locking path.
    if (isDynamicAllocation) {
        TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
        Inst_RegMem(OP::LRegMem(), node, tempReg, MRef_Bdisp32(clzReg, offsetof(J9ArrayClass, lockOffset), cg), cg);
        Inst_RegImm(OP::CMPRegImm4(), node, tempReg, (int32_t)-1, cg);
        Inst_Label(OP::JE4, node, doneLabel, cg);
        Inst_MemImm(OP::SMemImm4(comp->target().is64Bit() && !fej9->generateCompressedLockWord()), node,
            MRef_BIS(objectReg, tempReg, 0, cg), 0, cg);
        Inst_Label(OP::label, node, doneLabel, cg);
    } else {
        bool initReservable = TR::Compiler->cls.classFlagReservableWordInitValue(clazz);
        if (!isZeroInitialized || initReservable) {
            bool initLw = (node->getOpCodeValue() != TR::New) || initReservable;
            int lwOffset = fej9->getByteOffsetToLockword(clazz);
            if (lwOffset == -1)
                initLw = false;

            if (initLw) {
                int32_t initialLwValue = 0;
                if (initReservable)
                    initialLwValue = OBJECT_HEADER_LOCK_RESERVED;

                Inst_MemImm(OP::SMemImm4(comp->target().is64Bit() && !fej9->generateCompressedLockWord()), node,
                    MRef_Bdisp32(objectReg, lwOffset, cg), initialLwValue, cg);
            }
        }
    }
}

// Generate the code to initialize an array object header
//
static void genInitArrayHeader(TR::Node *node, TR_OpaqueClassBlock *clazz, TR::Register *classReg,
    TR::Register *objectReg, TR::Register *sizeReg, int32_t elementSize, int32_t arrayletDataOffset,
    TR::Register *tempReg, bool isZeroInitialized, bool isDynamicAllocation, bool shouldInitZeroSizedArrayHeader,
    TR::CodeGenerator *cg)
{
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

    // Initialize the object header
    //
    genInitObjectHeader(node, clazz, classReg, objectReg, tempReg, isZeroInitialized, isDynamicAllocation, cg);

    int32_t arraySizeOffset = fej9->getOffsetOfContiguousArraySizeField();

    TR::MemoryReference *arraySizeMR = MRef_Bdisp32(objectReg, arraySizeOffset, cg);
    // Special handling of zero sized arrays.
    // Zero length arrays are discontiguous (i.e. they also need the discontiguous length field to be 0) because
    // they are indistinguishable from non-zero length discontiguous arrays. But instead of explicitly checking
    // for zero sized arrays we unconditionally store 0 in the third dword of the array object header. That is
    // safe because the 3rd dword is either array size of a zero sized array or will contain the first elements
    // of an array:
    // - When dual header shape is enabled zero sized arrays have the following layout:
    // - The smallest array possible is a byte array with 1 element which would have a layout:
    //   #bits per section (compressed refs): | 32 bits |  32 bits   | 32 bits | 32 bits |   32 bits   |   32 bits    |
    //   zero sized arrays:                   |  class  | mustBeZero |  size   | padding |            other           |
    //   smallest contiguous array:           |  class  |    size    | padding |           other                      |
    //
    // - When dual header shape is disabled zero sized arrays have the following layout:
    //   #bits per section (compressed refs): | 32 bits |  32 bits   | 32 bits | 32 bits |   32 bits   |   32 bits    |
    //   zero sized arrays:                   |  class  | mustBeZero |  size   | padding |          dataAddr          |
    //   smallest contiguous array:           |  class  |    size    |      dataAddr     | 1 byte + padding |  other  |
    //
    // Refer to J9IndexableObject(Dis)contiguousCompressed structs runtime/oti/j9nonbuilder.h for more detail
    int32_t arrayDiscontiguousSizeOffset = fej9->getOffsetOfDiscontiguousArraySizeField();
    TR::MemoryReference *arrayDiscontiguousSizeMR = MRef_Bdisp32(objectReg, arrayDiscontiguousSizeOffset, cg);

    TR::Compilation *comp = cg->comp();

    bool canUseFastInlineAllocation = (!comp->getOptions()->realTimeGC() && !comp->generateArraylets()) ? true : false;

    // Initialize the array size
    //
    if (sizeReg) {
        // Variable size
        //
        if (canUseFastInlineAllocation) {
            // Native 64-bit needs to cover the discontiguous size field
            //
            OP::Mnemonic storeOp
                = (comp->target().is64Bit() && !comp->useCompressedPointers()) ? OP::S8MemReg : OP::S4MemReg;
            Inst_MemReg(storeOp, node, arraySizeMR, sizeReg, cg);
        } else {
            Inst_MemReg(OP::S4MemReg, node, arraySizeMR, sizeReg, cg);
        }
        // Take care of zero sized arrays as they are discontiguous and not contiguous
        if (shouldInitZeroSizedArrayHeader) {
            Inst_MemImm(OP::S4MemImm4, node, arrayDiscontiguousSizeMR, 0, cg);
        }
    } else {
        // Fixed size
        //
        int32_t instanceSize = 0;
        if (canUseFastInlineAllocation) {
            // Native 64-bit needs to cover the discontiguous size field
            //
            OP::Mnemonic storeOp
                = (comp->target().is64Bit() && !comp->useCompressedPointers()) ? OP::S8MemImm4 : OP::S4MemImm4;
            instanceSize = node->getFirstChild()->getInt();
            Inst_MemImm(storeOp, node, arraySizeMR, instanceSize, cg);
        } else {
            instanceSize = node->getFirstChild()->getInt();
            Inst_MemImm(OP::S4MemImm4, node, arraySizeMR, instanceSize, cg);
        }
        // Take care of zero sized arrays as they are discontiguous and not contiguous
        if (shouldInitZeroSizedArrayHeader && (instanceSize == 0)) {
            Inst_MemImm(OP::S4MemImm4, node, arrayDiscontiguousSizeMR, 0, cg);
        }
    }

    bool generateArraylets = comp->generateArraylets();

    if (generateArraylets) {
        // write arraylet pointer
        OP::Mnemonic storeOp;

        Inst_RegMem(OP::LEARegMem(), node, tempReg, MRef_Bdisp32(objectReg, arrayletDataOffset, cg), cg);

        if (comp->useCompressedPointers()) {
            storeOp = OP::S4MemReg;

            // Compress the arraylet pointer.
            //
            if (TR::Compiler->om.compressedReferenceShiftOffset() > 0)
                Inst_RegImm(OP::SHR8RegImm1, node, tempReg, TR::Compiler->om.compressedReferenceShiftOffset(), cg);
        } else {
            storeOp = OP::SMemReg();
        }

        TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
        Inst_MemReg(storeOp, node, MRef_Bdisp32(objectReg, fej9->getFirstArrayletPointerOffset(comp), cg), tempReg, cg);
    }
}

// ------------------------------------------------------------------------------
// genZeroInitForEntireObjectOrHybridArraylet
// ------------------------------------------------------------------------------

static bool genZeroInitForEntireObjectOrHybridArraylet(TR::Node *node, int32_t objectSizeInBytes, int32_t elementSize,
    TR::Register *sizeReg, TR::Register *newObjectAddressReg, TR::Register *numBytesToZeroInitReg,
    TR::Register *segmentReg, TR_X86ScratchRegisterManager *srm, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();

    bool isArrayNew = (node->getOpCodeValue() != TR::New);
    auto headerSizeInBytes
        = isArrayNew ? TR::Compiler->om.contiguousArrayHeaderSizeInBytes() : TR::Compiler->om.objectHeaderSizeInBytes();

    // In order for us to successfully initialize the size field of a zero sized array
    // we must set headerSizeInBytes to 8 bytes for compressed refs and 12 bytes for full refs.
    // This can be accomplished by using offset of discontiguous array size field.
    // However, when dealing with compressed refs with dual header shape enabled we don't
    // need to change the headerSizeInBytes because contiguous header size is equal to the offset
    // of discontiguous array size field.
    // This allows rep stosb to initialize the size field of zero sized arrays appropriately.
    //
    // Refer to J9IndexableObject(Dis)contiguousCompressed structs in runtime/oti/j9nonbuilder.h for more detail
    //
    if (!comp->target().is32Bit() && isArrayNew
        && (TR::Compiler->om.isIndexableDataAddrPresent() || !TR::Compiler->om.compressObjectReferences())) {
        TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

        TR_ASSERT_FATAL_WITH_NODE(node,
            (fej9->getOffsetOfDiscontiguousArraySizeField() - fej9->getOffsetOfContiguousArraySizeField()) == 4,
            "Offset of size field in discontiguous array header is expected to be 4 bytes more than contiguous array "
            "header. "
            "But size field offset for contiguous array header was %d bytes and %d bytes for discontiguous array "
            "header.\n",
            fej9->getOffsetOfContiguousArraySizeField(), fej9->getOffsetOfDiscontiguousArraySizeField());

        headerSizeInBytes = static_cast<uint32_t>(fej9->getOffsetOfDiscontiguousArraySizeField());
    }

    TR_ASSERT(headerSizeInBytes >= 4, "Object/Array header must be >= 4.");
    objectSizeInBytes -= headerSizeInBytes;

    if (!minRepstosdWords) {
        static char *p = feGetEnv("TR_MinRepstosdWords");
        minRepstosdWords = p ? atoi(p) : MIN_REPSTOSD_WORDS;
    }

    if (sizeReg || objectSizeInBytes >= minRepstosdWords) {
        // Zero-initialize by using REP OP::STOSB.
        //
        if (sizeReg) {
            // -------------
            // VARIABLE SIZE
            // -------------

            // Subtract off the header size and initialize the remaining slots.
            //
            // When the size is in a register, the numBytesToZeroInitReg contains the
            // rounded object size including the header
            //
            Inst_RegImm(OP::SUBRegImms(), node, numBytesToZeroInitReg, headerSizeInBytes, cg);
        } else {
            // ----------
            // FIXED SIZE
            // ----------

            if (comp->target().is64Bit() && !IS_32BIT_SIGNED(objectSizeInBytes)) {
                Inst_RegImm64(OP::MOV8RegImm64, node, numBytesToZeroInitReg, objectSizeInBytes, cg);
            } else {
                Inst_RegImm(OP::MOVRegImm4(), node, numBytesToZeroInitReg, objectSizeInBytes, cg);
            }
        }

        // If the compile-time size is unknown, generate a runtime length check to
        // determine if REP STOS initialization is more appropriate.
        //
        // On 32-bit, always do REP STOS initialization inline.
        //
        static const char *p = feGetEnv("TR_repStosZeroInitThresholdBytes");
        static int32_t repStosZeroInitThresholdBytes = p ? atoi(p) : 64;
        static bool doInlineRepStosZeroInit = feGetEnv("TR_dontInlineRepStosZeroInit") ? false : true;

        TR::Register *zeroInitScratchReg = NULL;
        if (comp->target().is64Bit()) {
            zeroInitScratchReg = srm->findOrCreateScratchRegister();
        }

#ifdef TR_TARGET_64BIT
        if (sizeReg && doInlineRepStosZeroInit) {
            int32_t repSTOSThresholdAdjustment = 0;
            int32_t startingAddressAdjustment = 0;

            /**
             * When initialization will be done inline with 8-byte stores, if the object
             * header is not a multiple of 8 then adjust the starting address back by
             * four bytes to ensure the stores are aligned on their natural boundary
             * and so that initialization does not run past the end of the newly allocated
             * object.  The REP STOS threshold will also need to be adjusted to account
             * for these four bytes.
             */
            if (headerSizeInBytes % 16 == 12) {
                repSTOSThresholdAdjustment = -4;
                startingAddressAdjustment = -4;
            }

            TR::LabelSymbol *repStosInitLabelSym = generateLabelSymbol(cg);
            TR::LabelSymbol *mergeInitLabelSym = generateLabelSymbol(cg);

            Inst_RegImm(OP::CMPRegImms(), node, numBytesToZeroInitReg,
                repStosZeroInitThresholdBytes + repSTOSThresholdAdjustment, cg);
            Inst_Label(OP::JG4, node, repStosInitLabelSym, cg);

            // Begin zero initialization after the header, adjusting for header size
            // if necessary
            //
            Inst_RegMem(OP::LEARegMem(), node, segmentReg,
                MRef_Bdisp32(newObjectAddressReg, headerSizeInBytes + startingAddressAdjustment, cg), cg);

            Inst_RegReg(OP::XOR4RegReg, node, zeroInitScratchReg, zeroInitScratchReg, cg);

            // Generate mainline zero initialization with stores
            //
            TR::LabelSymbol *zeroInitLoopLabelSym = generateLabelSymbol(cg);
            Inst_Label(OP::label, node, zeroInitLoopLabelSym, cg);
            Inst_MemReg(OP::S8MemReg, node, MRef_Bdisp32(segmentReg, 0, cg), zeroInitScratchReg, cg);
            Inst_RegImm(OP::ADD8RegImms, node, segmentReg, 8, cg);
            Inst_RegImm(OP::SUB8RegImms, node, numBytesToZeroInitReg, 8, cg);
            Inst_Label(OP::JG4, node, zeroInitLoopLabelSym, cg);

            {
                // Generate out-of-line REP STOS initialization
                //
                TR_OutlinedInstructionsGenerator og(repStosInitLabelSym, node, cg);

                // newObjectAddressReg must be in rax
                // segmentReg must be in rdi
                // numBytesToZeroInitReg must be in rcx

                // Begin zero initialization after the header
                //
                Inst_RegMem(OP::LEARegMem(), node, segmentReg, MRef_Bdisp32(newObjectAddressReg, headerSizeInBytes, cg),
                    cg);

                Inst_RegReg(OP::MOVRegReg(), node, zeroInitScratchReg, newObjectAddressReg, cg);
                Inst_RegReg(OP::XOR4RegReg, node, newObjectAddressReg, newObjectAddressReg, cg);
                Inst(OP::REPSTOSB, node, cg);
                Inst_RegReg(OP::MOVRegReg(), node, newObjectAddressReg, zeroInitScratchReg, cg);
                Inst_Label(OP::JMP4, node, mergeInitLabelSym, cg);
                og.endOutlinedInstructionSequence();
            }

            srm->reclaimScratchRegister(zeroInitScratchReg);

            // Merge
            //
            Inst_Label(OP::label, node, mergeInitLabelSym, cg);
        } else {
#endif
            // Begin zero initialization after the header
            //
            Inst_RegMem(OP::LEARegMem(), node, segmentReg, MRef_Bdisp32(newObjectAddressReg, headerSizeInBytes, cg),
                cg);

            if (comp->target().is64Bit()) {
                Inst_RegReg(OP::MOVRegReg(), node, zeroInitScratchReg, newObjectAddressReg, cg);
            } else {
                Inst_Reg(OP::PUSHReg, node, newObjectAddressReg, cg);
            }
            Inst_RegReg(OP::XOR4RegReg, node, newObjectAddressReg, newObjectAddressReg, cg);
            Inst(OP::REPSTOSB, node, cg);
            if (comp->target().is64Bit()) {
                Inst_RegReg(OP::MOVRegReg(), node, newObjectAddressReg, zeroInitScratchReg, cg);
                srm->reclaimScratchRegister(zeroInitScratchReg);
            } else {
                Inst_Reg(OP::POPReg, node, newObjectAddressReg, cg);
            }
#ifdef TR_TARGET_64BIT
        }
#endif

        return true;
    } else if (objectSizeInBytes > 0) {
        if (objectSizeInBytes % 16 == 12) {
            // Zero-out header to avoid a 12-byte residue
            objectSizeInBytes += 4;
            headerSizeInBytes -= 4;
        }

        TR::Register *scratchReg = srm->findOrCreateScratchRegister(TR_FPR);
        Inst_RegReg(OP::PXORRegReg, node, scratchReg, scratchReg, cg);
        int32_t offset = 0;
        while (objectSizeInBytes >= 16) {
            Inst_MemReg(OP::MOVDQUMemReg, node, MRef_Bdisp32(newObjectAddressReg, headerSizeInBytes + offset, cg),
                scratchReg, cg);
            objectSizeInBytes -= 16;
            offset += 16;
        }

        switch (objectSizeInBytes) {
            case 8:
                Inst_MemReg(OP::MOVQMemReg, node, MRef_Bdisp32(newObjectAddressReg, headerSizeInBytes + offset, cg),
                    scratchReg, cg);
                break;
            case 4:
                Inst_MemReg(OP::MOVDMemReg, node, MRef_Bdisp32(newObjectAddressReg, headerSizeInBytes + offset, cg),
                    scratchReg, cg);
                break;
            case 0:
                break;
            default:
                TR_ASSERT(false, "residue size should only be 0, 4 or 8.");
        }

        srm->reclaimScratchRegister(scratchReg);
        return false;
    } else {
        return false;
    }
}

// Generate the code to initialize the data portion of an allocated object.
// Zero-initialize the monitor slot in the header at the same time.
// If "sizeReg"  is non-null it contains the number of array elements and
// "elementSize" contains the size of each element.
// Otherwise the object size is in "objectSize".
//
static bool genZeroInitEntireObject(TR::Node *node, int32_t objectSize, int32_t elementSize, TR::Register *sizeReg,
    TR::Register *targetReg, TR::Register *tempReg, TR::Register *segmentReg, TR_X86ScratchRegisterManager *srm,
    TR::CodeGenerator *cg)
{
    // object header flags now occupy 4bytes on 64-bit
    TR::ILOpCodes opcode = node->getOpCodeValue();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
    TR::Compilation *comp = cg->comp();

    bool isArrayNew = (opcode != TR::New);
    TR_OpaqueClassBlock *clazz = NULL;

    // set up clazz value here
    comp->canAllocateInline(node, clazz);

    int32_t numSlots = 0;
    int32_t startOfZeroInits
        = isArrayNew ? TR::Compiler->om.contiguousArrayHeaderSizeInBytes() : TR::Compiler->om.objectHeaderSizeInBytes();

    if (comp->target().is64Bit()) {
        // round down to the nearest word size
        TR_ASSERT(startOfZeroInits < 0xF8, "expecting start of zero inits to be the size of the header");
        startOfZeroInits &= 0xF8;
    }

    numSlots = (int32_t)((objectSize - startOfZeroInits) / TR::Compiler->om.sizeofReferenceAddress());

    bool generateArraylets = comp->generateArraylets();

    int32_t i;

    // *** old object header ***
    // since i'm always confused,
    // here is the layout of an object
    //
    // #if defined(J9VM_THR_LOCK_NURSERY)
    //
    // on 32-bit
    //     for an indexable object [header = 4 or 3 slots]
    //     #if defined(J9VM_THR_LOCK_NURSERY_FAT_ARRAYS)
    //     --clazz-- --flags-- --monitor-- --size-- <--data-->
    //     #else
    //     --clazz-- --flags-- --size-- <--data-->
    //     #endif
    //
    //     for a non-indexable object (if the object has sync methods, monitor
    //     slot is part of the data slots) [header = 2 slots]
    //     --clazz-- --flags-- <--data-->
    //
    // on 64-bit
    //     for an indexable object [header = 3 or 2 slots]
    //     #if defined(J9VM_THR_LOCK_NURSERY_FAT_ARRAYS)
    //     --clazz-- --flags+size-- --monitor-- <--data-->
    //     #else
    //     --clazz-- --flags+size-- <--data-->
    //     #endif
    //
    //     for a non-indexable object [header = 2 slots]
    //     --clazz-- --flags-- <--data-->
    //
    // #else
    //
    // on 32-bit
    //     for an indexable object [header = 4 slots]
    //     --clazz-- --flags-- --monitor-- --size-- <--data-->
    //
    //     for a non-indexable object [header = 3 slots]
    //     --clazz-- --flags-- --monitor-- <--data-->
    //
    // on 64-bit
    //     for an indexable object [header = 3 slots]
    //     --clazz-- --flags+size-- --monitor-- <--data-->
    //
    //     for a non-indexable object [header = 3 slots]
    //     --clazz-- --flags-- --monitor-- <--data-->
    //
    // #endif
    //
    // Packed Objects adds two more fields,
    //

    if (!minRepstosdWords) {
        static char *p = feGetEnv("TR_MinRepstosdWords");
        if (p)
            minRepstosdWords = atoi(p);
        else
            minRepstosdWords = MIN_REPSTOSD_WORDS; // Use default value
    }

    int32_t alignmentDelta = 0; // for aligning properly to get best performance from REP OP::STOSD/OP::STOSQ

    if (sizeReg || (numSlots + alignmentDelta) >= minRepstosdWords) {
        // Zero-initialize by using REP OP::STOSD/OP::STOSQ.
        //
        // startOffset will be monitorSlot only for arrays

        Inst_RegMem(OP::LEARegMem(), node, segmentReg, MRef_Bdisp32(targetReg, startOfZeroInits, cg), cg);

        if (sizeReg) {
            int32_t additionalSlots = 0;

            if (generateArraylets) {
                additionalSlots++;
                if (elementSize > sizeof(UDATA))
                    additionalSlots++;
            }

            switch (elementSize) {
                // Calculate the number of slots by rounding up to number of words,
                // adding in partialHeaderSize.adding in partialHeaderSize.
                //
                case 1:
                    if (comp->target().is64Bit()) {
                        Inst_RegMem(OP::LEA8RegMem, node, tempReg, MRef_Bdisp32(sizeReg, (additionalSlots * 8) + 7, cg),
                            cg);
                        Inst_RegImm(OP::SHR8RegImm1, node, tempReg, 3, cg);
                    } else {
                        Inst_RegMem(OP::LEA4RegMem, node, tempReg, MRef_Bdisp32(sizeReg, (additionalSlots * 4) + 3, cg),
                            cg);
                        Inst_RegImm(OP::SHR4RegImm1, node, tempReg, 2, cg);
                    }
                    break;
                case 2:
                    if (comp->target().is64Bit()) {
                        Inst_RegMem(OP::LEA8RegMem, node, tempReg, MRef_Bdisp32(sizeReg, (additionalSlots * 4) + 3, cg),
                            cg);
                        Inst_RegImm(OP::SHR8RegImm1, node, tempReg, 2, cg);
                    } else {
                        Inst_RegMem(OP::LEA4RegMem, node, tempReg, MRef_Bdisp32(sizeReg, (additionalSlots * 2) + 1, cg),
                            cg);
                        Inst_RegImm(OP::SHR4RegImm1, node, tempReg, 1, cg);
                    }
                    break;
                case 4:
                    if (comp->target().is64Bit()) {
                        Inst_RegMem(OP::LEA8RegMem, node, tempReg, MRef_Bdisp32(sizeReg, (additionalSlots * 2) + 1, cg),
                            cg);
                        Inst_RegImm(OP::SHR8RegImm1, node, tempReg, 1, cg);
                    } else {
                        Inst_RegMem(OP::LEA4RegMem, node, tempReg, MRef_Bdisp32(sizeReg, additionalSlots, cg), cg);
                    }
                    break;
                case 8:
                    if (comp->target().is64Bit()) {
                        Inst_RegMem(OP::LEA8RegMem, node, tempReg, MRef_Bdisp32(sizeReg, additionalSlots, cg), cg);
                    } else {
                        Inst_RegMem(OP::LEA4RegMem, node, tempReg,
                            MRef_BISdisp32(NULL, sizeReg, TR::MemoryReference::convertMultiplierToStride(2),
                                additionalSlots, cg),
                            cg);
                    }
                    break;
            }
        } else {
            // Fixed size
            //
            Inst_RegImm(OP::MOVRegImm4(), node, tempReg, numSlots + alignmentDelta, cg);
            if (comp->target().is64Bit()) {
                // TODO AMD64: replace both instructions with a LEA tempReg, [disp32]
                //
                Inst_RegReg(OP::MOVSXReg8Reg4, node, tempReg, tempReg, cg);
            }
        }

        TR::Register *scratchReg = NULL;
        if (comp->target().is64Bit()) {
            scratchReg = srm->findOrCreateScratchRegister();
            Inst_RegReg(OP::MOVRegReg(), node, scratchReg, targetReg, cg);
        } else {
            Inst_Reg(OP::PUSHReg, node, targetReg, cg);
        }

        Inst_RegReg(OP::XOR4RegReg, node, targetReg, targetReg, cg);

        // We just pushed targetReg on the stack and zeroed it out. targetReg contained the address of the
        // beginning of the header. We want to use the 0-reg to initialize the monitor slot, so we use
        // segmentReg, which points to targetReg+startOfZeroInits and subtract the extra offset.

        bool initLw = (node->getOpCodeValue() != TR::New);
        int lwOffset = fej9->getByteOffsetToLockword(clazz);
        initLw = false;

        if (initLw) {
            OP::Mnemonic op
                = (comp->target().is64Bit() && fej9->generateCompressedLockWord()) ? OP::S4MemReg : OP::SMemReg();
            Inst_MemReg(op, node, MRef_Bdisp32(segmentReg, lwOffset - startOfZeroInits, cg), targetReg, cg);
        }

        OP::Mnemonic op = comp->target().is64Bit() ? OP::REPSTOSQ : OP::REPSTOSD;
        Inst(op, node, cg);

        if (comp->target().is64Bit()) {
            Inst_RegReg(OP::MOVRegReg(), node, targetReg, scratchReg, cg);
            srm->reclaimScratchRegister(scratchReg);
        } else {
            Inst_Reg(OP::POPReg, node, targetReg, cg);
        }

        return true;
    }

    if (numSlots > 0) {
        Inst_RegReg(OP::XOR4RegReg, node, tempReg, tempReg, cg);

        bool initLw = (node->getOpCodeValue() != TR::New);
        int lwOffset = fej9->getByteOffsetToLockword(clazz);
        initLw = false;

        if (initLw) {
            OP::Mnemonic op
                = (comp->target().is64Bit() && fej9->generateCompressedLockWord()) ? OP::S4MemReg : OP::SMemReg();
            Inst_MemReg(op, node, MRef_Bdisp32(targetReg, lwOffset, cg), tempReg, cg);
        }
    } else {
        bool initLw = (node->getOpCodeValue() != TR::New);
        int lwOffset = fej9->getByteOffsetToLockword(clazz);
        initLw = false;

        if (initLw) {
            OP::Mnemonic op
                = (comp->target().is64Bit() && fej9->generateCompressedLockWord()) ? OP::S4MemImm4 : OP::SMemImm4();
            Inst_MemImm(op, node, MRef_Bdisp32(targetReg, lwOffset, cg), 0, cg);
        }
        return false;
    }

    int32_t numIterations = numSlots / maxZeroInitWordsPerIteration;
    if (numIterations > 1) {
        // Generate the initializations in a loop
        //
        int32_t numLoopSlots = numIterations * maxZeroInitWordsPerIteration;
        int32_t endOffset;

        endOffset = (int32_t)(numLoopSlots * TR::Compiler->om.sizeofReferenceAddress() + startOfZeroInits);

        Inst_RegImm(OP::MOVRegImm4(), node, segmentReg, -((numIterations - 1) * maxZeroInitWordsPerIteration), cg);

        if (comp->target().is64Bit())
            Inst_RegReg(OP::MOVSXReg8Reg4, node, segmentReg, segmentReg, cg);

        TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);
        Inst_Label(OP::label, node, loopLabel, cg);
        for (i = maxZeroInitWordsPerIteration; i > 0; i--) {
            Inst_MemReg(OP::SMemReg(), node,
                MRef_BISdisp32(targetReg, segmentReg,
                    TR::MemoryReference::convertMultiplierToStride((int32_t)TR::Compiler->om.sizeofReferenceAddress()),
                    endOffset - TR::Compiler->om.sizeofReferenceAddress() * i, cg),
                tempReg, cg);
        }
        Inst_RegImm(OP::ADDRegImms(), node, segmentReg, maxZeroInitWordsPerIteration, cg);
        Inst_Label(OP::JLE4, node, loopLabel, cg);

        // Generate the left-over initializations
        //
        for (i = 0; i < numSlots % maxZeroInitWordsPerIteration; i++) {
            Inst_MemReg(OP::SMemReg(), node,
                MRef_Bdisp32(targetReg, endOffset + TR::Compiler->om.sizeofReferenceAddress() * i, cg), tempReg, cg);
        }
    } else {
        // Generate the initializations inline
        //
        for (i = 0; i < numSlots; i++) {
            // Don't bother initializing the array-size slot
            //
            Inst_MemReg(OP::SMemReg(), node,
                MRef_Bdisp32(targetReg, i * TR::Compiler->om.sizeofReferenceAddress() + startOfZeroInits, cg), tempReg,
                cg);
        }
    }

    return false;
}

TR::Register *objectCloneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    /*
     * Commented out Object.clone() code has been removed for code cleanliness.
     * If it needs to be resurrected it can be found in RTC or CMVC.
     */
    return NULL;
}

#ifdef J9VM_GC_SPARSE_HEAP_ALLOCATION
static void handleOffHeapDataForArrays(TR::Node *node, TR::Register *sizeReg, TR::Register *targetReg,
    TR::Register *tempReg, TR_X86ScratchRegisterManager *srm, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
    OMR::Logger *log = comp->log();
    bool trace = comp->getOption(TR_TraceCG);

    /* Here we'll update dataAddr slot for both fixed and variable length arrays. Fixed length arrays are
     * simple as we just need to check first child of the node for array size. For variable length arrays,
     * runtime size checks are needed to determine whether to use contiguous or discontiguous header layout.
     *
     * In both scenarios, arrays of non-zero size use contiguous header layout while zero size arrays use
     * discontiguous header layout. DataAddr field of zero size arrays is intialized to NULL because they
     * don't have any data elements.
     */
    TR::MemoryReference *dataAddrSlotMR = NULL;
    TR::MemoryReference *dataAddrMR = NULL;
    TR::Register *zeroReg = NULL;
    if (TR::Compiler->om.compressObjectReferences() && NULL != sizeReg) {
        /* We need to check sizeReg at runtime to determine correct offset of dataAddr field.
         * Here we deal only with compressed refs because dataAddr field offset for discontiguous
         * and contiguous arrays is the same in full refs.
         */
        logprintf(trace, log, "Node (%p): Dealing with compressed refs variable length array.\n", node);

        TR_ASSERT_FATAL_WITH_NODE(node,
            (fej9->getOffsetOfDiscontiguousDataAddrField() - fej9->getOffsetOfContiguousDataAddrField()) == 8,
            "Offset of dataAddr field in discontiguous array is expected to be 8 bytes more than contiguous array. "
            "But was %d bytes for discontiguous and %d bytes for contiguous array.\n",
            fej9->getOffsetOfDiscontiguousDataAddrField(), fej9->getOffsetOfContiguousDataAddrField());

        TR::Register *discontiguousDataAddrOffsetReg = srm->findOrCreateScratchRegister();
        Inst_RegReg(OP::XOR4RegReg, node, discontiguousDataAddrOffsetReg, discontiguousDataAddrOffsetReg, cg);
        // Since array size is capped at 32 bits, we only need to check lower half (0-31 bits) of sizeReg.
        Inst_RegImm(OP::CMP4RegImm4, node, sizeReg, 1, cg);
        Inst_RegImm(OP::ADCRegImm4(), node, discontiguousDataAddrOffsetReg, 0, cg);

        dataAddrMR = MRef_BISdisp32(targetReg, discontiguousDataAddrOffsetReg, 3,
            TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg);
        dataAddrSlotMR = MRef_BISdisp32(targetReg, discontiguousDataAddrOffsetReg, 3,
            fej9->getOffsetOfContiguousDataAddrField(), cg);
        // Load first data element address
        Inst_RegMem(OP::LEARegMem(), node, tempReg, dataAddrMR, cg);

        // Clear out tempReg if dealing with 0 length array
        zeroReg = srm->findOrCreateScratchRegister();
        Inst_RegReg(OP::XOR4RegReg, node, zeroReg, zeroReg, cg);
        // Since array size is capped at 32 bits, we only need to check lower half (0-31 bits) of sizeReg.
        Inst_RegReg(OP::TEST4RegReg, node, sizeReg, sizeReg, cg);
        Inst_RegReg(OP::CMOVERegReg(), node, tempReg, zeroReg, cg);
        srm->reclaimScratchRegister(zeroReg);

        // Write first data element address to dataAddr slot
        Inst_MemReg(OP::SMemReg(), node, dataAddrSlotMR, tempReg, cg);
        srm->reclaimScratchRegister(discontiguousDataAddrOffsetReg);
    } else if (NULL == sizeReg && node->getFirstChild()->getOpCode().isLoadConst()
        && node->getFirstChild()->getInt() == 0) {
        logprintf(trace, log, "Node (%p): Dealing with full/compressed refs fixed length zero size array.\n", node);

        dataAddrSlotMR = MRef_Bdisp32(targetReg, fej9->getOffsetOfDiscontiguousDataAddrField(), cg);
        Inst_MemImm(OP::SMemImm4(), node, dataAddrSlotMR, 0, cg);
    } else {
        logprintf(trace, log,
            "Node (%p): Dealing with either full/compressed refs fixed length non-zero size array or full refs "
            "variable length array.\n",
            node);

        if (!TR::Compiler->om.compressObjectReferences()) {
            TR_ASSERT_FATAL_WITH_NODE(node,
                fej9->getOffsetOfDiscontiguousDataAddrField() == fej9->getOffsetOfContiguousDataAddrField(),
                "dataAddr field offset is expected to be same for both contiguous and discontiguous arrays in full "
                "refs. "
                "But was %d bytes for discontiguous and %d bytes for contiguous array.\n",
                fej9->getOffsetOfDiscontiguousDataAddrField(), fej9->getOffsetOfContiguousDataAddrField());
        }

        dataAddrMR = MRef_Bdisp32(targetReg, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg);
        dataAddrSlotMR = MRef_Bdisp32(targetReg, fej9->getOffsetOfContiguousDataAddrField(), cg);
        // Load first data element address
        Inst_RegMem(OP::LEARegMem(), node, tempReg, dataAddrMR, cg);

        if (!TR::Compiler->om.compressObjectReferences() && NULL != sizeReg) {
            // Clear out tempReg if dealing with 0 length array
            zeroReg = srm->findOrCreateScratchRegister();
            Inst_RegReg(OP::XORRegReg(), node, zeroReg, zeroReg, cg);
            // Since array size is capped at 32 bits, we only need to check lower half (0-31 bits) of sizeReg.
            Inst_RegReg(OP::TEST4RegReg, node, sizeReg, sizeReg, cg);
            Inst_RegReg(OP::CMOVERegReg(), node, tempReg, zeroReg, cg);
            srm->reclaimScratchRegister(zeroReg);
        }
        // Write first data element address to dataAddr slot
        Inst_MemReg(OP::SMemReg(), node, dataAddrSlotMR, tempReg, cg);
    }
}
#endif /* J9VM_GC_SPARSE_HEAP_ALLOCATION */

static void verifyInlinedAllocation(TR::Node *node, TR_OpaqueClassBlock *clazz, int32_t allocationSize,
    TR::LabelSymbol *failLabel, TR::Instruction *startInstr, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();

    startInstr = startInstr->getNext();
    TR_OpaqueClassBlock *classToValidate = clazz;

    TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(
        sizeof(TR_RelocationRecordInformation), heapAlloc);
    recordInfo->data1 = allocationSize;
    recordInfo->data2 = node->getInlinedSiteIndex();
    recordInfo->data3 = (uintptr_t)failLabel;
    recordInfo->data4 = (uintptr_t)startInstr;

    TR::SymbolReference *classSymRef;
    TR_ExternalRelocationTargetKind reloKind;

    if (node->getOpCodeValue() == TR::New) {
        classSymRef = node->getFirstChild()->getSymbolReference();
        reloKind = TR_VerifyClassObjectForAlloc;
    } else {
        classSymRef = node->getSecondChild()->getSymbolReference();
        reloKind = TR_VerifyRefArrayForAlloc;

        if (comp->getOption(TR_UseSymbolValidationManager)) {
            classToValidate = comp->fej9()->getComponentClassFromArrayClass(classToValidate);
        }
    }

    if (comp->getOption(TR_UseSymbolValidationManager)) {
        TR_ASSERT(classToValidate, "classToValidate should not be NULL, clazz=%p\n", clazz);
        recordInfo->data5 = (uintptr_t)classToValidate;
    }

    cg->addExternalRelocation(new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(startInstr,
                                  (uint8_t *)classSymRef, (uint8_t *)recordInfo, reloKind, cg),
        __FILE__, __LINE__, node);
}

TR::Register *J9::X86::TreeEvaluator::VMnewEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = comp->fej9();
    OMR::Logger *log = comp->log();
    bool trace = comp->getOption(TR_TraceCG);

    if (comp->suppressAllocationInlining())
        return NULL;

    if (comp->getOption(TR_DisableAllocationInlining))
        return NULL;

    // If the helper does not preserve all the registers there will not be
    // enough registers to do the inline allocation.
    // Also, don't do the inline allocation if optimizing for space
    //
    TR::MethodSymbol *helperSym = node->getSymbol()->castToMethodSymbol();
    if (!helperSym->preservesAllRegisters())
        return NULL;

    bool realTimeGC = comp->getOptions()->realTimeGC();
    bool generateArraylets = comp->generateArraylets();

    TR_OpaqueClassBlock *clazz = NULL;
    bool isArrayNew = false;
    int32_t allocationSize = 0;
    int32_t objectSize = 0;
    int32_t elementSize = 0;
    int32_t dataOffset = 0;

    TR_X86ScratchRegisterManager *srm = cg->generateScratchRegisterManager(comp->target().is64Bit() ? 8 : 7);

    TR::Register *classReg = NULL;
    TR::Register *segmentReg = NULL;
    TR::Register *tempReg = NULL;
    TR::Register *targetReg = NULL;
    TR::Register *sizeReg = NULL;

    /**
     * Study of registers used in inline allocation.
     *
     * Result goes to targetReg. Unless outlinedHelperCall is used, which requires an extra register move to targetReg2.
     *    targetReg2 is needed because the result needs to be CollectedReferenceRegister, but only after object is
     * ready.
     *
     * classReg contains the J9Class for the object to be allocated. Not always used; instead, when loadaddr is not
     * evaluated, it is rematerialized like a constant (in which case, clazz contains the known value). When it is
     * rematerialized, there are 'interesting' AOT/HCR patching routines.
     *
     * sizeReg is used for array allocations to hold the number of elements. However...
     *    for packed variable (objectSize==0) arrays, sizeReg behaves like segmentReg should (i.e. contains size in
     * _bytes_): elementSize is set to 1 and sizeReg is result of multiplication of real elementSize vs element count.
     *
     * segmentReg contains the size, _in bytes!_, of the object/array to be allocated. When outlining is used, it will
     * be bound to edi. This must contain the rounding (i.e. 8-aligned, so address will always end in 0x0 or 0x8). When
     * size cannot be known (i.e. dynamic array size) explicit assembly is generated to do rounding (allocationSize is
     * reused to contain the header offset). After tlh-top comparison, this register is reused as a temporary register
     * (i.e. genHeapAlloc in non-outlined path, and inside the outlined codert asm sequences). This size is not
     * available at non-outlined zero-initialization routine and needs to be re-materialized.
     *
     */

    // --------------------------------------------------------------------------------
    //
    // Find the class info and allocation size depending on the node type.
    //
    // Returns:
    //    size of object    includes the size of the array header
    //    -1                cannot allocate inline
    //    0                 variable sized allocation
    //
    // --------------------------------------------------------------------------------

    objectSize = comp->canAllocateInline(node, clazz);
    if (objectSize < 0)
        return NULL;

    // Currently dynamic allocation is only supported on reference array.
    // We are performing dynamic array allocation if both object size and
    // class block cannot be statically determined.
    //
    bool dynamicArrayAllocation = (node->getOpCodeValue() == TR::anewarray) && (objectSize == 0) && (clazz == NULL);

    allocationSize = objectSize;

    static long count = 0;
    if (!performTransformation(comp, "O^O <%3d> Inlining Allocation of %s [0x%p].\n", count++,
            node->getOpCode().getName(), node))
        return NULL;

    if (node->getOpCodeValue() == TR::New) {
        // realtimeGC: cannot inline if object size is too big to get a size class
        if (realTimeGC) {
            if ((uint32_t)objectSize > fej9->getMaxObjectSizeForSizeClass())
                return NULL;
        }

        dataOffset = TR::Compiler->om.objectHeaderSizeInBytes(); // Not used...
        classReg = node->getFirstChild()->getRegister();

        // For Non-SVM AOT, the class has to be in a register
        if (TR::Compiler->om.areValueTypesEnabled() && !classReg && cg->needClassAndMethodPointerRelocations()
            && !comp->getOption(TR_UseSymbolValidationManager)) {
            classReg = cg->evaluate(node->getFirstChild());

            logprintf(trace, log, "%s: evaluate loadaddr: clazz %p classReg %s\n", __FUNCTION__, clazz,
                classReg ? classReg->getRegisterName(comp) : "<none>");
        }

        TR_ASSERT_FATAL_WITH_NODE(node, objectSize > 0, "Object size must be known");
    } else {
        if (node->getOpCodeValue() == TR::newarray) {
            elementSize = TR::Compiler->om.getSizeOfArrayElement(node);
        } else {
            // Must be TR::anewarray
            //
            elementSize = comp->useCompressedPointers() ? TR::Compiler->om.sizeofReferenceField()
                                                        : (int32_t)TR::Compiler->om.sizeofReferenceAddress();

            classReg = node->getSecondChild()->getRegister();

            // For dynamic array allocation, need to evaluate second child
            if (!classReg && dynamicArrayAllocation)
                classReg = cg->evaluate(node->getSecondChild());
        }

        isArrayNew = true;

        dataOffset = generateArraylets ? fej9->getArrayletFirstElementOffset(elementSize, comp)
                                       : TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
    }

    bool enableTLHBatchClearing = fej9->tlhHasBeenCleared();

#ifdef J9VM_GC_NON_ZERO_TLH
    if (node->canSkipZeroInitialization() && (enableTLHBatchClearing || !comp->getOption(TR_DisableDualTLH))
        && !realTimeGC) {
        // Choose appropriate helper call if zero initialization can be skipped
        //
        TR::SymbolReference *noZeroInitSymRef = NULL;
        TR::SymbolReferenceTable *symRefTab = comp->getSymRefTab();

        switch (node->getOpCodeValue()) {
            case TR::New:
                // For value types, it should use the jitNewValue helper call which
                // is set up before codegen
                //
                if (!TR::Compiler->om.areValueTypesEnabled()
                    || (node->getSymbolReference()
                        != symRefTab->findOrCreateNewValueSymbolRef(comp->getMethodSymbol()))) {
                    noZeroInitSymRef = symRefTab->findOrCreateNewObjectNoZeroInitSymbolRef(comp->getMethodSymbol());
                }
                break;

            case TR::newarray:
                noZeroInitSymRef = symRefTab->findOrCreateNewArrayNoZeroInitSymbolRef(comp->getMethodSymbol());
                break;

            case TR::anewarray:
                noZeroInitSymRef = symRefTab->findOrCreateANewArrayNoZeroInitSymbolRef(comp->getMethodSymbol());
                break;

            default:
                break;
        }

        if (noZeroInitSymRef) {
            node->setSymbolReference(noZeroInitSymRef);
        }

        logprintf(trace, log, "SKIPZEROINIT: for %p, change the symbol to %p ", node, node->getSymbolReference());
    } else {
        logprintf(trace, log, "NOSKIPZEROINIT: for %p,  keep symbol as %p ", node, node->getSymbolReference());
    }
#endif

    segmentReg = srm->findOrCreateScratchRegister();
    tempReg = srm->findOrCreateScratchRegister();

    // If the size is variable, evaluate it into a register
    //
    if (objectSize == 0) {
        sizeReg = cg->evaluate(node->getFirstChild());
        allocationSize += dataOffset;
        logprintf(trace, log, "allocationSize %d dataOffset %d\n", allocationSize, dataOffset);
    } else {
        sizeReg = NULL;
    }

    TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
    startLabel->setStartInternalControlFlow();
    Inst_Label(OP::label, node, startLabel, cg);

    // Generate the helper call for out-of-line allocation
    //
    targetReg = srm->findOrCreateScratchRegister();
    TR::LabelSymbol *fallThru = generateLabelSymbol(cg);
    fallThru->setEndInternalControlFlow();
    TR::LabelSymbol *failLabel = generateLabelSymbol(cg);

    TR_OutlinedInstructions *outlinedHelperCall
        = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::acall, targetReg, failLabel, fallThru, cg);
    cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);

    TR::Instruction *startInstr = cg->getAppendInstruction();

    // --------------------------------------------------------------------------------
    // Do the allocation from the TLH and bump pointers.
    //
    // The address of the start of the allocated heap space will be in targetReg.
    // --------------------------------------------------------------------------------

    bool doInlineAllocationForObjectOrHybridArraylet = (!realTimeGC && !generateArraylets) ? true : false;

    if (doInlineAllocationForObjectOrHybridArraylet) {
        genHeapAllocForObjectOrHybridArraylet(node, clazz, allocationSize, elementSize, sizeReg, targetReg, segmentReg,
            tempReg, failLabel, cg);
    } else {
        genHeapAllocForDiscontiguousArraysOrRealtime(node, clazz, allocationSize, elementSize, sizeReg, targetReg,
            segmentReg, tempReg, failLabel, cg);
    }

    // --------------------------------------------------------------------------------
    // Perform zero-initialization on data slots.
    //
    // There may be information about which slots are to be zero-initialized.
    // If there is no information, all slots must be zero-initialized.
    // --------------------------------------------------------------------------------

    bool useRepInstruction;
    bool monitorSlotIsInitialized;

    TR::Register *scratchReg = NULL;
    bool shouldInitZeroSizedArrayHeader = true;

#ifdef J9VM_GC_NON_ZERO_TLH
    if (!enableTLHBatchClearing || realTimeGC) {
#endif
        if (!maxZeroInitWordsPerIteration) {
            static char *p = feGetEnv("TR_MaxZeroInitWordsPerIteration");
            maxZeroInitWordsPerIteration = p ? atoi(p) : MAX_ZERO_INIT_WORDS_PER_ITERATION;
        }

        TR_ExtraInfoForNew *initInfo = node->getSymbolReference()->getExtraInfo();

        if (initInfo && initInfo->zeroInitSlots) {
            // If there are too many words to be individually initialized, initialize
            // them all
            //
            if (initInfo->numZeroInitSlots >= maxZeroInitWordsPerIteration * 2 - 1)
                initInfo->zeroInitSlots = NULL;
        }

        if (initInfo && initInfo->zeroInitSlots) {
            // Zero-initialize by explicit zero stores.
            // Use the supplied bit vector to identify which slots to initialize
            //
            // Zero-initialize the monitor slot in the header at the same time.
            //
            TR_BitVectorIterator bvi(*initInfo->zeroInitSlots);
            static bool UseOldBVI = feGetEnv("TR_UseOldBVI");
            if (UseOldBVI) {
                Inst_RegReg(OP::XOR4RegReg, node, tempReg, tempReg, cg);
                while (bvi.hasMoreElements()) {
                    Inst_MemReg(OP::S4MemReg, node, MRef_Bdisp32(targetReg, bvi.getNextElement() * 4 + dataOffset, cg),
                        tempReg, cg);
                }
            } else {
                int32_t lastElementIndex = -1;
                int32_t nextE = -2;
                int32_t span = 0;
                int32_t lastSpan = -1;
                scratchReg = srm->findOrCreateScratchRegister(TR_FPR);
                Inst_RegReg(OP::PXORRegReg, node, scratchReg, scratchReg, cg);
                while (bvi.hasMoreElements()) {
                    nextE = bvi.getNextElement();
                    if (-1 == lastElementIndex)
                        lastElementIndex = nextE;
                    span = nextE - lastElementIndex;
                    TR_ASSERT(span >= 0, "SPAN < 0");
                    if (span < 3) {
                        lastSpan = span;
                        continue;
                    } else if (span == 3) {
                        Inst_MemReg(OP::MOVDQUMemReg, node,
                            MRef_Bdisp32(targetReg, lastElementIndex * 4 + dataOffset, cg), scratchReg, cg);
                        lastSpan = -1;
                        lastElementIndex = -1;
                    } else if (span > 3) {
                        OP::Mnemonic storeOpCode;
                        if (lastSpan == 0) {
                            storeOpCode = OP::MOVDMemReg;
                        } else if (lastSpan == 1) {
                            storeOpCode = OP::MOVQMemReg;
                        } else {
                            storeOpCode = OP::MOVDQUMemReg;
                        }

                        Inst_MemReg(storeOpCode, node, MRef_Bdisp32(targetReg, lastElementIndex * 4 + dataOffset, cg),
                            scratchReg, cg);

                        lastElementIndex = nextE;
                        lastSpan = 0;
                    }
                }

                int32_t adjustedDataOffset = dataOffset;
                OP::Mnemonic storeOpCode = OP::bad;

                switch (lastSpan) {
                    case 0:
                        storeOpCode = OP::MOVDMemReg;
                        break;
                    case 1:
                        storeOpCode = OP::MOVQMemReg;
                        break;
                    case 2:
                        TR_ASSERT(dataOffset >= 4, "dataOffset must be >= 4.");
                        storeOpCode = OP::MOVDQUMemReg;
                        adjustedDataOffset -= 4;
                        break;
                    default:
                        break;
                }

                if (storeOpCode != OP::bad) {
                    Inst_MemReg(storeOpCode, node,
                        MRef_Bdisp32(targetReg, lastElementIndex * 4 + adjustedDataOffset, cg), scratchReg, cg);
                }

                srm->reclaimScratchRegister(scratchReg);
            }

            useRepInstruction = false;

            J9JavaVM *jvm = fej9->getJ9JITConfig()->javaVM;
            monitorSlotIsInitialized = (jvm->lockwordMode != LOCKNURSERY_ALGORITHM_ALL_INHERIT);
        } else if ((!initInfo || initInfo->numZeroInitSlots > 0) && !node->canSkipZeroInitialization()) {
            // Initialize all slots
            //
            if (doInlineAllocationForObjectOrHybridArraylet) {
                useRepInstruction = genZeroInitForEntireObjectOrHybridArraylet(node, objectSize, elementSize, sizeReg,
                    targetReg, tempReg, segmentReg, srm, cg);
                shouldInitZeroSizedArrayHeader = false;
            } else {
                useRepInstruction = genZeroInitEntireObject(node, objectSize, elementSize, sizeReg, targetReg, tempReg,
                    segmentReg, srm, cg);
            }

            J9JavaVM *jvm = fej9->getJ9JITConfig()->javaVM;
            monitorSlotIsInitialized = (jvm->lockwordMode != LOCKNURSERY_ALGORITHM_ALL_INHERIT);
        } else {
            // Skip data initialization
            //
            if (doInlineAllocationForObjectOrHybridArraylet) {
                // Even though we can skip the data initialization, for arrays of unknown size we still have
                // to initialize at least one slot to cover the discontiguous length field in case the array
                // is zero sized.  This is because the length is not checked at runtime and is only needed
                // for non-native 64-bit targets where the discontiguous length slot is already initialized
                // via the contiguous length slot.
                //
                if (node->getOpCodeValue() != TR::New && (comp->target().is32Bit() || comp->useCompressedPointers())) {
                    Inst_MemImm(OP::SMemImm4(), node,
                        MRef_Bdisp32(targetReg, fej9->getOffsetOfDiscontiguousArraySizeField(), cg), 0, cg);
                    shouldInitZeroSizedArrayHeader = false;
                }
            }

            monitorSlotIsInitialized = false;
            useRepInstruction = false;
        }
#ifdef J9VM_GC_NON_ZERO_TLH
    } else {
        monitorSlotIsInitialized = false;
        useRepInstruction = false;
    }
#endif

    // --------------------------------------------------------------------------------
    // Initialize the header
    // --------------------------------------------------------------------------------

    if (isArrayNew) {
        TR::Register *classArgReg;
        bool isDynamicAllocationArg;

        if ((fej9->inlinedAllocationsMustBeVerified() && !comp->getOption(TR_UseSymbolValidationManager)
                && node->getOpCodeValue() == TR::anewarray)
            || dynamicArrayAllocation) {
            // If dynamic array allocation, must pass in classReg to initialize the array header
            //
            classArgReg = classReg;
            isDynamicAllocationArg = true;
        } else {
            classArgReg = NULL;
            isDynamicAllocationArg = false;
        }

        genInitArrayHeader(node, clazz, classArgReg, targetReg, sizeReg, elementSize, dataOffset, tempReg,
            monitorSlotIsInitialized, isDynamicAllocationArg, shouldInitZeroSizedArrayHeader, cg);
    } else {
        genInitObjectHeader(node, clazz, classReg, targetReg, tempReg, monitorSlotIsInitialized, false, cg);
    }

#ifdef J9VM_GC_SPARSE_HEAP_ALLOCATION
    if (isArrayNew && TR::Compiler->om.isOffHeapAllocationEnabled()) {
        handleOffHeapDataForArrays(node, sizeReg, targetReg, tempReg, srm, cg);
    }
#endif /* J9VM_GC_SPARSE_HEAP_ALLOCATION */

    if (fej9->inlinedAllocationsMustBeVerified()
        && (node->getOpCodeValue() == TR::New || node->getOpCodeValue() == TR::anewarray)) {
        verifyInlinedAllocation(node, clazz, allocationSize, failLabel, startInstr, cg);
    }

    // 1 == vmThread
    int32_t numDeps = 2;

    if (useRepInstruction)
        numDeps += 2;

    if (sizeReg)
        numDeps++;
    if (classReg)
        numDeps++;

    // Outlined helper call
    //
    if (node->getOpCodeValue() == TR::New)
        numDeps++;
    else
        numDeps += 2;

    numDeps += srm->numAvailableRegisters();

    TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, numDeps, cg);

    if (sizeReg)
        deps->addPostCondition(sizeReg, TR::RealRegister::NoReg, cg);
    if (classReg)
        deps->addPostCondition(classReg, TR::RealRegister::NoReg, cg);

    deps->addPostCondition(targetReg, TR::RealRegister::eax, cg);
    deps->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);

    if (useRepInstruction) {
        // These real register dependencies must be added here because the same
        // virtuals may appear in the scratch register manager
        //
        deps->addPostCondition(tempReg, TR::RealRegister::ecx, cg);
        deps->addPostCondition(segmentReg, TR::RealRegister::edi, cg);
    }

    // Outlined helper call
    //
    TR::Node *callNode = outlinedHelperCall->getCallNode();
    TR::Register *reg;

    if (callNode->getFirstChild() == node->getFirstChild()) {
        reg = callNode->getFirstChild()->getRegister();
        if (reg)
            deps->unionPostCondition(reg, TR::RealRegister::NoReg, cg);
    }

    if (node->getOpCodeValue() != TR::New) {
        if (callNode->getSecondChild() == node->getSecondChild()) {
            reg = callNode->getSecondChild()->getRegister();
            if (reg)
                deps->unionPostCondition(reg, TR::RealRegister::NoReg, cg);
        }
    }

    srm->addScratchRegistersToDependencyList(deps);

    deps->stopAddingConditions();

    Inst_Label(OP::label, node, fallThru, deps, cg);

    // Copy the newly allocated object into a collected reference register
    // now that it is a valid object.
    //
    TR::Register *targetReg2 = cg->allocateCollectedReferenceRegister();
    TR::RegisterDependencyConditions *deps2 = RegDeps(0, 1, cg);
    deps2->addPostCondition(targetReg2, TR::RealRegister::eax, cg);
    Inst_RegReg(OP::MOVRegReg(), node, targetReg2, targetReg, deps2, cg);
    cg->stopUsingRegister(targetReg);
    targetReg = targetReg2;

    srm->stopUsingRegisters();

    cg->decReferenceCount(node->getFirstChild());
    if (isArrayNew)
        cg->decReferenceCount(node->getSecondChild());

    node->setRegister(targetReg);
    return targetReg;
}

// Generate instructions to type-check a store into a reference-type array.
// The code sequence determines if the destination is an array of "java/lang/Object" instances,
// or if the source object has the correct type (i.e. equal to the component type of the array).
//
void J9::X86::TreeEvaluator::VMarrayStoreCHKEvaluator(TR::Node *node, TR::Node *sourceChild, TR::Node *destinationChild,
    TR_X86ScratchRegisterManager *scratchRegisterManager, TR::LabelSymbol *wrtbarLabel, TR::Instruction *prevInstr,
    TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
    TR::Register *sourceReg = sourceChild->getRegister();
    TR::Register *destReg = destinationChild->getRegister();
    TR::LabelSymbol *helperCallLabel = generateLabelSymbol(cg);

    static char *disableArrayStoreCheckOpts = feGetEnv("TR_disableArrayStoreCheckOpts");
    if (!disableArrayStoreCheckOpts || !debug("noInlinedArrayStoreCHKs")) {
        // If the component type of the array is equal to the type of the source reference,
        // then the store always succeeds. The component type of the array is stored in a
        // field of the J9ArrayClass that represents the type of the array.
        //

        TR::Register *sourceClassReg = scratchRegisterManager->findOrCreateScratchRegister();
        TR::Register *destComponentClassReg = scratchRegisterManager->findOrCreateScratchRegister();

        TR::Instruction *instr;

        if (TR::Compiler->om.compressObjectReferences()) {
            // FIXME: Add check for hint when doing the arraystore check as below when class pointer compression
            // is enabled.

            TR::MemoryReference *destTypeMR = MRef_Bdisp32(destReg, TR::Compiler->om.offsetOfObjectVftField(), cg);

            Inst_RegMem(OP::L4RegMem, node, destComponentClassReg, destTypeMR,
                cg); // class pointer is 32 bits
            TR::TreeEvaluator::generateVFTMaskInstruction(node, destComponentClassReg, cg);

            // -------------------------------------------------------------------------
            //
            // If the component type is java.lang.Object then the store always succeeds.
            //
            // -------------------------------------------------------------------------

            TR_OpaqueClassBlock *objectClass = fej9->getSystemClassFromClassName("java/lang/Object", 16);

            TR_ASSERT((((uintptr_t)objectClass) >> 32) == 0,
                "TR_OpaqueClassBlock must fit on 32 bits when using class pointer compression");
            instr = Inst_RegImm(OP::CMP4RegImm4, node, destComponentClassReg, (uint32_t)((uint64_t)objectClass), cg);

            Inst_Label(OP::JE4, node, wrtbarLabel, cg);

            // here we may have to convert the TR_OpaqueClassBlock into a J9Class pointer
            // and store it in destComponentClassReg
            // ..

            TR::MemoryReference *destCompTypeMR
                = MRef_Bdisp32(destComponentClassReg, offsetof(J9ArrayClass, componentType), cg);
            Inst_RegMem(OP::LRegMem(), node, destComponentClassReg, destCompTypeMR, cg);

            // here we may have to convert the J9Class pointer from destComponentClassReg into
            // a TR_OpaqueClassBlock and store it back into destComponentClassReg
            // ..

            TR::MemoryReference *sourceRegClassMR
                = MRef_Bdisp32(sourceReg, TR::Compiler->om.offsetOfObjectVftField(), cg);
            Inst_RegMem(OP::L4RegMem, node, sourceClassReg, sourceRegClassMR, cg);
            TR::TreeEvaluator::generateVFTMaskInstruction(node, sourceClassReg, cg);

            Inst_RegReg(OP::CMP4RegReg, node, destComponentClassReg, sourceClassReg,
                cg); // compare only 32 bits
            Inst_Label(OP::JE4, node, wrtbarLabel, cg);

            // -------------------------------------------------------------------------
            //          // Check the source class cast cache
            //
            // -------------------------------------------------------------------------

            Inst_MemReg(OP::CMP4MemReg, node, MRef_Bdisp32(sourceClassReg, offsetof(J9Class, castClassCache), cg),
                destComponentClassReg, cg);
        } else // no class pointer compression
        {
            TR::MemoryReference *sourceClassMR = MRef_Bdisp32(sourceReg, TR::Compiler->om.offsetOfObjectVftField(), cg);
            Inst_RegMem(OP::LRegMem(), node, sourceClassReg, sourceClassMR, cg);
            TR::TreeEvaluator::generateVFTMaskInstruction(node, sourceClassReg, cg);

            TR::MemoryReference *destClassMR = MRef_Bdisp32(destReg, TR::Compiler->om.offsetOfObjectVftField(), cg);
            Inst_RegMem(OP::LRegMem(), node, destComponentClassReg, destClassMR, cg);
            TR::TreeEvaluator::generateVFTMaskInstruction(node, destComponentClassReg, cg);
            TR::MemoryReference *destCompTypeMR
                = MRef_Bdisp32(destComponentClassReg, offsetof(J9ArrayClass, componentType), cg);
            Inst_RegMem(OP::LRegMem(), node, destComponentClassReg, destCompTypeMR, cg);

            Inst_RegReg(OP::CMPRegReg(), node, destComponentClassReg, sourceClassReg, cg);
            Inst_Label(OP::JE4, node, wrtbarLabel, cg);

            // -------------------------------------------------------------------------
            //
            // Check the source class cast cache
            //
            // -------------------------------------------------------------------------

            Inst_MemReg(OP::CMPMemReg(), node, MRef_Bdisp32(sourceClassReg, offsetof(J9Class, castClassCache), cg),
                destComponentClassReg, cg);
        }
        Inst_Label(OP::JE4, node, wrtbarLabel, cg);

        instr = NULL;
        /*
        TR::Instruction *instr;


        // -------------------------------------------------------------------------
        //
        // If the component type is java.lang.Object then the store always succeeds.
        //
        // -------------------------------------------------------------------------

        TR_OpaqueClassBlock *objectClass = fej9->getSystemClassFromClassName("java/lang/Object", 16);

        if (comp->target().is64Bit())
           {
              if (TR::Compiler->om.compressObjectReferences())
                 {
                 TR_ASSERT((((uintptr_t)objectClass) >> 32) == 0, "TR_OpaqueClassBlock must fit on 32 bits when using
        class pointer compression"); instr = Inst_RegImm(OP::CMP4RegImm4, node,
        destComponentClassReg, (uint32_t) ((uint64_t) objectClass), cg);
                 }
              else // 64 bit but no class pointer compression
                 {
                 if ((uintptr_t)objectClass <= (uintptr_t)0x7fffffff)
                    {
                    instr = Inst_RegImm(OP::CMP8RegImm4, node, destComponentClassReg,
        (uintptr_t) objectClass, cg);
                    }
                 else
                    {
                    TR::Register *objectClassReg = scratchRegisterManager->findOrCreateScratchRegister();
                    instr = Inst_RegImm64(OP::MOV8RegImm64, node, objectClassReg, (uintptr_t)
        objectClass, cg); Inst_RegReg(OP::CMP8RegReg, node, destComponentClassReg,
        objectClassReg, cg); scratchRegisterManager->reclaimScratchRegister(objectClassReg);
                    }
                 }
           }
        else
           {
           instr = Inst_RegImm(OP::CMP4RegImm4, node, destComponentClassReg,
        (int32_t)(uintptr_t) objectClass, cg);
           }

        Inst_Label(OP::JE4, node, wrtbarLabel, cg);
        */

        // ---------------------------------------------
        //
        // If isInstanceOf (objectClass,ArrayComponentClass,true,true) was successful and stored during VP, we need to
        // test again the real arrayComponentClass Need to relocate address of arrayComponentClass under aot sharedcache
        // Need to possibility of class unloading.
        // --------------------------------------------

        if (!(comp->getOption(TR_DisableArrayStoreCheckOpts)) && node->getArrayComponentClassInNode()) {
            TR_OpaqueClassBlock *arrayComponentClass = (TR_OpaqueClassBlock *)node->getArrayComponentClassInNode();
            if (comp->target().is64Bit()) {
                if (TR::Compiler->om.compressObjectReferences()) {
                    TR_ASSERT((((uintptr_t)arrayComponentClass) >> 32) == 0,
                        "TR_OpaqueClassBlock must fit on 32 bits when using class pointer compression");
                    instr = Inst_RegImm(OP::CMP4RegImm4, node, destComponentClassReg,
                        (uint32_t)((uint64_t)arrayComponentClass), cg);

                    if (fej9->isUnloadAssumptionRequired(arrayComponentClass, comp->getCurrentMethod()))
                        comp->getStaticPICSites()->push_front(instr);

                } else // 64 bit but no class pointer compression
                {
                    if ((uintptr_t)arrayComponentClass <= (uintptr_t)0x7fffffff) {
                        instr = Inst_RegImm(OP::CMP8RegImm4, node, destComponentClassReg,
                            (uintptr_t)arrayComponentClass, cg);
                        if (fej9->isUnloadAssumptionRequired(arrayComponentClass, comp->getCurrentMethod()))
                            comp->getStaticPICSites()->push_front(instr);

                    } else {
                        TR::Register *arrayComponentClassReg = scratchRegisterManager->findOrCreateScratchRegister();
                        instr = Inst_RegImm64(OP::MOV8RegImm64, node, arrayComponentClassReg,
                            (uintptr_t)arrayComponentClass, cg);
                        Inst_RegReg(OP::CMP8RegReg, node, destComponentClassReg, arrayComponentClassReg, cg);
                        scratchRegisterManager->reclaimScratchRegister(arrayComponentClassReg);
                    }
                }
            } else {
                instr = Inst_RegImm(OP::CMP4RegImm4, node, destComponentClassReg,
                    (int32_t)(uintptr_t)arrayComponentClass, cg);
                if (fej9->isUnloadAssumptionRequired(arrayComponentClass, comp->getCurrentMethod()))
                    comp->getStaticPICSites()->push_front(instr);
            }

            Inst_Label(OP::JE4, node, wrtbarLabel, cg);
        }

        // For compressed references:
        // destComponentClassReg contains the class offset so we may need to generate code
        // to convert from class offset to real J9Class pointer

        // -------------------------------------------------------------------------
        //
        // Compare source and dest class depths
        //
        // -------------------------------------------------------------------------

        // Get the depth of array component type in testerReg
        //
        bool eliminateDepthMask = (J9AccClassDepthMask == 0xffff);
        TR::MemoryReference *destComponentClassDepthMR
            = MRef_Bdisp32(destComponentClassReg, offsetof(J9Class, classDepthAndFlags), cg);

        // DMDM  32-bit only???
        if (comp->target().is32Bit()) {
            scratchRegisterManager->reclaimScratchRegister(destComponentClassReg);
        }

        TR::Register *destComponentClassDepthReg = scratchRegisterManager->findOrCreateScratchRegister();

        if (eliminateDepthMask) {
            if (comp->target().is64Bit())
                Inst_RegMem(OP::MOVZXReg8Mem2, node, destComponentClassDepthReg, destComponentClassDepthMR, cg);
            else
                Inst_RegMem(OP::MOVZXReg4Mem2, node, destComponentClassDepthReg, destComponentClassDepthMR, cg);
        } else {
            Inst_RegMem(OP::LRegMem(), node, destComponentClassDepthReg, destComponentClassDepthMR, cg);
        }

        if (!eliminateDepthMask) {
            if (comp->target().is64Bit()) {
                TR_ASSERT(!(J9AccClassDepthMask & 0x80000000), "AMD64: need to use a second register for AND mask");
                if (!(J9AccClassDepthMask & 0x80000000))
                    Inst_RegImm(OP::AND8RegImm4, node, destComponentClassDepthReg, J9AccClassDepthMask, cg);
            } else {
                Inst_RegImm(OP::AND4RegImm4, node, destComponentClassDepthReg, J9AccClassDepthMask, cg);
            }
        }

        // For compressed references:
        // temp2 contains the class offset so we may need to generate code
        // to convert from class offset to real J9Class pointer

        // Get the depth of type of object being stored into the array in testerReg2
        //

        TR::MemoryReference *mr = MRef_Bdisp32(sourceClassReg, offsetof(J9Class, classDepthAndFlags), cg);

        // There aren't enough registers available on 32-bit across this internal
        // control flow region.  Give one back and manually and force the source
        // class to be rematerialized later.
        //
        if (comp->target().is32Bit()) {
            scratchRegisterManager->reclaimScratchRegister(sourceClassReg);
        }

        TR::Register *sourceClassDepthReg = NULL;
        if (eliminateDepthMask) {
            Inst_MemReg(OP::CMP2MemReg, node, mr, destComponentClassDepthReg, cg);
        } else {
            sourceClassDepthReg = scratchRegisterManager->findOrCreateScratchRegister();
            Inst_RegMem(OP::LRegMem(), node, sourceClassDepthReg, mr, cg);

            if (comp->target().is64Bit()) {
                TR_ASSERT(!(J9AccClassDepthMask & 0x80000000), "AMD64: need to use a second register for AND mask");
                if (!(J9AccClassDepthMask & 0x80000000))
                    Inst_RegImm(OP::AND8RegImm4, node, sourceClassDepthReg, J9AccClassDepthMask, cg);
            } else {
                Inst_RegImm(OP::AND4RegImm4, node, sourceClassDepthReg, J9AccClassDepthMask, cg);
            }
            Inst_RegReg(OP::CMP4RegReg, node, sourceClassDepthReg, destComponentClassDepthReg, cg);
        }

        /*TR::Register *sourceClassDepthReg = scratchRegisterManager->findOrCreateScratchRegister();
        Inst_RegMem(
           OP::LRegMem(),
           node,
           sourceClassDepthReg,
           mr, cg);

        if (comp->target().is64Bit())
           {
           TR_ASSERT(!(J9AccClassDepthMask & 0x80000000), "AMD64: need to use a second register for AND mask");
           if (!(J9AccClassDepthMask & 0x80000000))
              Inst_RegImm(OP::AND8RegImm4, node, sourceClassDepthReg, J9AccClassDepthMask,
        cg);
           }
        else
           {
           Inst_RegImm(OP::AND4RegImm4, node, sourceClassDepthReg, J9AccClassDepthMask, cg);
           }

        Inst_RegReg(OP::CMP4RegReg, node, sourceClassDepthReg, destComponentClassDepthReg,
        cg);*/

        Inst_Label(OP::JBE4, node, helperCallLabel, cg);
        if (sourceClassDepthReg != NULL)
            scratchRegisterManager->reclaimScratchRegister(sourceClassDepthReg);

        // For compressed references:
        // destComponentClassReg contains the class offset so we may need to generate code
        // to convert from class offset to real J9Class pointer

        if (comp->target().is32Bit()) {
            // Rematerialize the source class.
            //
            sourceClassReg = scratchRegisterManager->findOrCreateScratchRegister();
            TR::MemoryReference *sourceClassMR = MRef_Bdisp32(sourceReg, TR::Compiler->om.offsetOfObjectVftField(), cg);
            Inst_RegMem(OP::LRegMem(), node, sourceClassReg, sourceClassMR, cg);
            TR::TreeEvaluator::generateVFTMaskInstruction(node, sourceClassReg, cg);
        }

        TR::MemoryReference *tempMR = MRef_Bdisp32(sourceClassReg, offsetof(J9Class, superclasses), cg);

        if (comp->target().is32Bit()) {
            scratchRegisterManager->reclaimScratchRegister(sourceClassReg);
        }

        TR::Register *sourceSuperClassReg = scratchRegisterManager->findOrCreateScratchRegister();

        Inst_RegMem(OP::LRegMem(), node, sourceSuperClassReg, tempMR, cg);

        TR::MemoryReference *leaMR
            = MRef_BISdisp32(sourceSuperClassReg, destComponentClassDepthReg, logBase2(sizeof(uintptr_t)), 0, cg);

        // For compressed references:
        // leaMR is a memory reference to a J9Class
        // destComponentClassReg contains a TR_OpaqueClassBlock
        // We may need to convert superClass to a class offset before doing the comparison

        if (comp->target().is32Bit()) {
            Inst_RegMem(OP::LRegMem(), node, sourceSuperClassReg, leaMR, cg);

            // Rematerialize destination component class
            //
            TR::MemoryReference *destClassMR = MRef_Bdisp32(destReg, TR::Compiler->om.offsetOfObjectVftField(), cg);

            Inst_RegMem(OP::LRegMem(), node, destComponentClassReg, destClassMR, cg);
            TR::TreeEvaluator::generateVFTMaskInstruction(node, destComponentClassReg, cg);
            TR::MemoryReference *destCompTypeMR
                = MRef_Bdisp32(destComponentClassReg, offsetof(J9ArrayClass, componentType), cg);

            Inst_MemReg(OP::CMPMemReg(), node, destCompTypeMR, sourceSuperClassReg, cg);
        } else {
            Inst_RegMem(OP::CMP4RegMem, node, destComponentClassReg, leaMR, cg);
        }

        scratchRegisterManager->reclaimScratchRegister(destComponentClassReg);
        scratchRegisterManager->reclaimScratchRegister(destComponentClassDepthReg);
        scratchRegisterManager->reclaimScratchRegister(sourceClassReg);
        scratchRegisterManager->reclaimScratchRegister(sourceSuperClassReg);

        Inst_Label(OP::JE4, node, wrtbarLabel, cg);
    }

    // The fast paths failed; execute the type-check helper call.
    //
    TR::LabelSymbol *helperReturnLabel = generateLabelSymbol(cg);
    TR::Node *helperCallNode
        = TR::Node::createWithSymRef(TR::call, 2, 2, sourceChild, destinationChild, node->getSymbolReference());
    helperCallNode->copyByteCodeInfo(node);
    Inst_Label(OP::JMP4, helperCallNode, helperCallLabel, cg);
    TR_OutlinedInstructions *outlinedHelperCall = new (cg->trHeapMemory())
        TR_OutlinedInstructions(helperCallNode, TR::call, NULL, helperCallLabel, helperReturnLabel, cg);
    cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);
    Inst_Label(OP::label, helperCallNode, helperReturnLabel, cg);
    cg->decReferenceCount(sourceChild);
    cg->decReferenceCount(destinationChild);
}

// Check that two objects are compatible for use in an arraycopy operation.
// If not, an ArrayStoreException is thrown.
//
TR::Register *J9::X86::TreeEvaluator::VMarrayCheckEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
    bool use64BitClasses = cg->comp()->target().is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders();

    TR::Node *object1 = node->getFirstChild();
    TR::Node *object2 = node->getSecondChild();
    TR::Register *object1Reg = cg->evaluate(object1);
    TR::Register *object2Reg = cg->evaluate(object2);

    TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *fallThrough = generateLabelSymbol(cg);
    TR::Instruction *instr;
    TR::LabelSymbol *snippetLabel = NULL;
    TR::Snippet *snippet = NULL;
    TR::Register *tempReg = cg->allocateRegister();

    startLabel->setStartInternalControlFlow();
    fallThrough->setEndInternalControlFlow();
    Inst_Label(OP::label, node, startLabel, cg);

    // If the objects are the same and one of them is known to be an array, they
    // are compatible.
    //
    if (node->isArrayChkPrimitiveArray1() || node->isArrayChkReferenceArray1() || node->isArrayChkPrimitiveArray2()
        || node->isArrayChkReferenceArray2()) {
        Inst_RegReg(OP::CMPRegReg(), node, object1Reg, object2Reg, cg);
        Inst_Label(OP::JE4, node, fallThrough, cg);
    }

    else {
        // Neither object is known to be an array
        // Check that object 1 is an array. If not, throw exception.
        //
        OP::Mnemonic testOpCode;
        if ((J9AccClassRAMArray >= CHAR_MIN) && (J9AccClassRAMArray <= CHAR_MAX))
            testOpCode = OP::TEST1MemImm1;
        else
            testOpCode = OP::TEST4MemImm4;

        if (TR::Compiler->om.compressObjectReferences())
            Inst_RegMem(OP::L4RegMem, node, tempReg,
                MRef_Bdisp32(object1Reg, TR::Compiler->om.offsetOfObjectVftField(), cg), cg);
        else
            Inst_RegMem(OP::LRegMem(), node, tempReg,
                MRef_Bdisp32(object1Reg, TR::Compiler->om.offsetOfObjectVftField(), cg), cg);

        TR::TreeEvaluator::generateVFTMaskInstruction(node, tempReg, cg);
        Inst_MemImm(testOpCode, node, MRef_Bdisp32(tempReg, offsetof(J9Class, classDepthAndFlags), cg),
            J9AccClassRAMArray, cg);
        if (!snippetLabel) {
            snippetLabel = generateLabelSymbol(cg);
            instr = Inst_Label(OP::JE4, node, snippetLabel, cg);
            snippet = new (cg->trHeapMemory())
                TR::X86CheckFailureSnippet(cg, node->getSymbolReference(), snippetLabel, instr);
            cg->addSnippet(snippet);
        } else
            Inst_Label(OP::JE4, node, snippetLabel, cg);
    }

    // Test equality of the object classes.
    //
    Inst_RegMem(OP::LRegMem(use64BitClasses), node, tempReg,
        MRef_Bdisp32(object1Reg, TR::Compiler->om.offsetOfObjectVftField(), cg), cg);
    Inst_RegMem(OP::XORRegMem(use64BitClasses), node, tempReg,
        MRef_Bdisp32(object2Reg, TR::Compiler->om.offsetOfObjectVftField(), cg), cg);
    TR::TreeEvaluator::generateVFTMaskInstruction(node, tempReg, cg);

    // If either object is known to be a primitive array, we are done. Either
    // the equality test fails and we throw the exception or it succeeds and
    // we finish.
    //
    if (node->isArrayChkPrimitiveArray1() || node->isArrayChkPrimitiveArray2()) {
        if (!snippetLabel) {
            snippetLabel = generateLabelSymbol(cg);
            instr = Inst_Label(OP::JNE4, node, snippetLabel, cg);
            snippet = new (cg->trHeapMemory())
                TR::X86CheckFailureSnippet(cg, node->getSymbolReference(), snippetLabel, instr);
            cg->addSnippet(snippet);
        } else
            Inst_Label(OP::JNE4, node, snippetLabel, cg);
    }

    // Otherwise, there is more testing to do. If the classes are equal we
    // are done, and branch to the fallThrough label.
    //
    else {
        Inst_Label(OP::JE4, node, fallThrough, cg);

        // If either object is not known to be a reference array type, check it
        // We already know that object1 is an array type but we may have to now
        // check object2.
        //
        if (!node->isArrayChkReferenceArray1()) {
            if (TR::Compiler->om.compressObjectReferences())
                Inst_RegMem(OP::L4RegMem, node, tempReg,
                    MRef_Bdisp32(object1Reg, TR::Compiler->om.offsetOfObjectVftField(), cg), cg);
            else
                Inst_RegMem(OP::LRegMem(), node, tempReg,
                    MRef_Bdisp32(object1Reg, TR::Compiler->om.offsetOfObjectVftField(), cg), cg);

            TR::TreeEvaluator::generateVFTMaskInstruction(node, tempReg, cg);
            Inst_RegMem(OP::LRegMem(), node, tempReg, MRef_Bdisp32(tempReg, offsetof(J9Class, classDepthAndFlags), cg),
                cg);
            // X = (ramclass->ClassDepthAndFlags)>>J9AccClassRAMShapeShift

            // X & OBJECT_HEADER_SHAPE_MASK
            Inst_RegImm(OP::ANDRegImm4(), node, tempReg, (OBJECT_HEADER_SHAPE_MASK << J9AccClassRAMShapeShift), cg);
            Inst_RegImm(OP::CMPRegImm4(), node, tempReg, (OBJECT_HEADER_SHAPE_POINTERS << J9AccClassRAMShapeShift), cg);

            if (!snippetLabel) {
                snippetLabel = generateLabelSymbol(cg);
                instr = Inst_Label(OP::JNE4, node, snippetLabel, cg);
                snippet = new (cg->trHeapMemory())
                    TR::X86CheckFailureSnippet(cg, node->getSymbolReference(), snippetLabel, instr);
                cg->addSnippet(snippet);
            } else
                Inst_Label(OP::JNE4, node, snippetLabel, cg);
        }
        if (!node->isArrayChkReferenceArray2()) {
            // Check that object 2 is an array. If not, throw exception.
            //
            OP::Mnemonic testOpCode;
            if ((J9AccClassRAMArray >= CHAR_MIN) && (J9AccClassRAMArray <= CHAR_MAX))
                testOpCode = OP::TEST1MemImm1;
            else
                testOpCode = OP::TEST4MemImm4;

            // Check that object 2 is an array. If not, throw exception.
            //
            if (TR::Compiler->om.compressObjectReferences())
                Inst_RegMem(OP::L4RegMem, node, tempReg,
                    MRef_Bdisp32(object2Reg, TR::Compiler->om.offsetOfObjectVftField(), cg), cg);
            else
                Inst_RegMem(OP::LRegMem(), node, tempReg,
                    MRef_Bdisp32(object2Reg, TR::Compiler->om.offsetOfObjectVftField(), cg), cg);
            TR::TreeEvaluator::generateVFTMaskInstruction(node, tempReg, cg);
            Inst_MemImm(testOpCode, node, MRef_Bdisp32(tempReg, offsetof(J9Class, classDepthAndFlags), cg),
                J9AccClassRAMArray, cg);
            if (!snippetLabel) {
                snippetLabel = generateLabelSymbol(cg);
                instr = Inst_Label(OP::JE4, node, snippetLabel, cg);
                snippet = new (cg->trHeapMemory())
                    TR::X86CheckFailureSnippet(cg, node->getSymbolReference(), snippetLabel, instr);
                cg->addSnippet(snippet);
            } else
                Inst_Label(OP::JE4, node, snippetLabel, cg);

            Inst_RegMem(OP::LRegMem(), node, tempReg, MRef_Bdisp32(tempReg, offsetof(J9Class, classDepthAndFlags), cg),
                cg);
            Inst_RegImm(OP::ANDRegImm4(), node, tempReg, (OBJECT_HEADER_SHAPE_MASK << J9AccClassRAMShapeShift), cg);
            Inst_RegImm(OP::CMPRegImm4(), node, tempReg, (OBJECT_HEADER_SHAPE_POINTERS << J9AccClassRAMShapeShift), cg);

            Inst_Label(OP::JNE4, node, snippetLabel, cg);
        }

        // Now both objects are known to be reference arrays, so they are
        // compatible for arraycopy.
    }

    // Now generate the fall-through label
    //
    TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, (uint8_t)4, cg);
    deps->addPostCondition(object1Reg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(object2Reg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(tempReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);

    Inst_Label(OP::label, node, fallThrough, deps, cg);

    cg->stopUsingRegister(tempReg);
    cg->decReferenceCount(object1);
    cg->decReferenceCount(object2);

    return NULL;
}

#ifdef LINUX
#if defined(TR_TARGET_32BIT)
static void addFPXMMDependencies(TR::CodeGenerator *cg, TR::RegisterDependencyConditions *dependencies)
{
    TR_LiveRegisters *lr = cg->getLiveRegisters(TR_FPR);
    if (!lr || lr->getNumberOfLiveRegisters() > 0) {
        for (uint8_t regIndex = cg->machine()->getFirstXMMR();
             regIndex <= static_cast<uint8_t>(cg->machine()->getLastXMMR()); regIndex++) {
            TR::Register *dummy = cg->allocateRegister(TR_FPR);
            dummy->setPlaceholderReg();
            dependencies->addPostCondition(dummy, (TR::RealRegister::RegNum)regIndex, cg);
            cg->stopUsingRegister(dummy);
        }
    }
}
#endif

#define J9TIME_NANOSECONDS_PER_SECOND ((I_64)1000000000)
#if defined(TR_TARGET_64BIT)
static bool inlineNanoTime(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

    if (fej9->supportsFastNanoTime()) { // Fully Inlined Version

        // First, evaluate resultAddress if provided.  There's no telling how
        // many regs that address computation needs, so let's get it out of the
        // way before we start using registers for other things.
        //

        TR::Register *resultAddress;
        if (node->getNumChildren() == 1) {
            resultAddress = cg->evaluate(node->getFirstChild());
        } else {
            TR_ASSERT(node->getNumChildren() == 0, "nanoTime must have zero or one children");
            resultAddress = NULL;
        }

        TR::SymbolReference *gtod = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_AMD64clockGetTime);
        TR::Node *timevalNode = TR::Node::createWithSymRef(node, TR::loadaddr, 0, cg->getNanoTimeTemp());
        TR::Node *clockSourceNode = TR::Node::create(node, TR::iconst, 0, CLOCK_MONOTONIC);
        TR::Node *callNode = TR::Node::createWithSymRef(TR::call, 2, 2, clockSourceNode, timevalNode, gtod);
        // TODO: Use performCall
        TR::Linkage *linkage = cg->getLinkage(gtod->getSymbol()->getMethodSymbol()->getLinkageConvention());
        linkage->buildDirectDispatch(callNode, false);

        TR::Register *result = cg->allocateRegister();
        TR::Register *reg = cg->allocateRegister();

        TR::MemoryReference *tv_sec;

        // result = tv_sec * 1,000,000,000 (converts seconds to nanoseconds)

        tv_sec = MRef_node(timevalNode, cg, false);
        Inst_RegMem(OP::L8RegMem, node, result, tv_sec, cg);
        Inst_RegRegImm(OP::IMUL8RegRegImm4, node, result, result, J9TIME_NANOSECONDS_PER_SECOND, cg);

        // reg = tv_usec
        Inst_RegMem(OP::L8RegMem, node, reg, MRef_MRefOff(*tv_sec, offsetof(struct timespec, tv_nsec), cg), cg);

        // result = reg + result
        Inst_RegMem(OP::LEA8RegMem, node, result, MRef_BIS(reg, result, 0, cg), cg);

#if defined(J9VM_OPT_CRIU_SUPPORT)
        if (fej9->isSnapshotModeEnabled()) {
            // result = result - nanoTimeMonotonicClockDelta
            TR::Register *vmThreadReg = cg->getVMThreadRegister();
            Inst_RegMem(OP::L8RegMem, node, reg, MRef_Bdisp32(vmThreadReg, offsetof(J9VMThread, javaVM), cg), cg);
            Inst_RegMem(OP::L8RegMem, node, reg, MRef_Bdisp32(reg, offsetof(J9JavaVM, portLibrary), cg), cg);
            Inst_RegMem(OP::SUB8RegMem, node, result,
                MRef_Bdisp32(reg, offsetof(J9PortLibrary, nanoTimeMonotonicClockDelta), cg), cg);
        }
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

        cg->stopUsingRegister(reg);

        // Store the result to memory if necessary
        if (resultAddress) {
            Inst_MemReg(OP::S8MemReg, node, MRef_Bdisp32(resultAddress, 0, cg), result, cg);

            cg->decReferenceCount(node->getFirstChild());
            if (node->getReferenceCount() == 1
                && cg->getCurrentEvaluationTreeTop()->getNode()->getOpCodeValue() == TR::treetop) {
                // Result is not needed in a register, so free it up
                //
                cg->stopUsingRegister(result);
                result = NULL;
            }
        }

        node->setRegister(result);

        return true;
    } else { // Inlined call to Port Library
        return false;
    }
}
#else // !64bit
static bool inlineNanoTime(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

    TR::RealRegister *espReal = cg->machine()->getRealRegister(TR::RealRegister::esp);
    TR::Register *vmThreadReg = cg->getVMThreadRegister();
    TR::Register *temp2 = 0;

    if (fej9->supportsFastNanoTime()) {
        TR::Register *resultAddress;
        if (node->getNumChildren() == 1) {
            resultAddress = cg->evaluate(node->getFirstChild());
            Inst_Reg(OP::PUSHReg, node, resultAddress, cg);
            Inst_Imm(OP::PUSHImm4, node, CLOCK_MONOTONIC, cg);
        } else {
            // Leave space on the stack for the 64-bit result
            //

            Inst_RegImm(OP::SUB4RegImms, node, espReal, 8, cg);

            resultAddress = cg->allocateRegister();
            Inst_RegReg(OP::MOV4RegReg, node, resultAddress, espReal,
                cg); // save away esp before the push
            Inst_Reg(OP::PUSHReg, node, resultAddress, cg);
            Inst_Imm(OP::PUSHImm4, node, CLOCK_MONOTONIC, cg);
            cg->stopUsingRegister(resultAddress);
            resultAddress = espReal;
        }

        // 64-bit issues on the call instructions below

        // Build register dependencies and call the method in the system library
        // directly. Since this is a "C"-style call, ebx, esi and edi are preserved
        //
        int32_t extraFPDeps = (uint8_t)(cg->machine()->getLastXMMR() - cg->machine()->getFirstXMMR() + 1);
        TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, (uint8_t)4 + extraFPDeps, cg);
        TR::Register *temp1 = cg->allocateRegister();
        deps->addPostCondition(temp1, TR::RealRegister::eax, cg);
        cg->stopUsingRegister(temp1);
        temp1 = cg->allocateRegister();
        deps->addPostCondition(temp1, TR::RealRegister::ecx, cg);
        cg->stopUsingRegister(temp1);
        temp1 = cg->allocateRegister();
        deps->addPostCondition(temp1, TR::RealRegister::edx, cg);
        cg->stopUsingRegister(temp1);
        deps->addPostCondition(cg->getMethodMetaDataRegister(), TR::RealRegister::ebp, cg);

        // add the XMM dependencies
        addFPXMMDependencies(cg, deps);
        deps->stopAddingConditions();

        TR::X86ImmInstruction *callInstr = Inst_Imm(OP::CALLImm4, node, (int32_t)&clock_gettime, deps, cg);

        Inst_RegImm(OP::ADD4RegImms, node, espReal, 8, cg);

        TR::Register *eaxReal = cg->allocateRegister();
        TR::Register *edxReal = cg->allocateRegister();

        // load usec to a register
        TR::Register *reglow = cg->allocateRegister();
        Inst_RegMem(OP::L4RegMem, node, reglow, MRef_Bdisp32(resultAddress, 4, cg), cg);

        TR::RegisterDependencyConditions *dep1 = RegDeps((uint8_t)2, 2, cg);
        dep1->addPreCondition(eaxReal, TR::RealRegister::eax, cg);
        dep1->addPreCondition(edxReal, TR::RealRegister::edx, cg);
        dep1->addPostCondition(eaxReal, TR::RealRegister::eax, cg);
        dep1->addPostCondition(edxReal, TR::RealRegister::edx, cg);

        // load second to eax then multiply by 1,000,000,000

        Inst_RegMem(OP::L4RegMem, node, edxReal, MRef_Bdisp32(resultAddress, 0, cg), cg);
        Inst_RegImm(OP::MOV4RegImm4, node, eaxReal, J9TIME_NANOSECONDS_PER_SECOND, cg);
        Inst_RegReg(OP::IMUL4AccReg, node, eaxReal, edxReal, dep1, cg);

        // add the two parts
        Inst_RegReg(OP::ADD4RegReg, node, eaxReal, reglow, cg);
        Inst_RegImm(OP::ADC4RegImm4, node, edxReal, 0x0, cg);

#if defined(J9VM_OPT_CRIU_SUPPORT)
        if (fej9->isSnapshotModeEnabled()) {
            // subtract nanoTimeMonotonicClockDelta from the result
            TR::Register *vmThreadReg = cg->getVMThreadRegister();
            Inst_RegMem(OP::L4RegMem, node, regLow, MRef_Bdisp32(vmThreadReg, offsetof(J9VMThread, javaVM), cg), cg);
            Inst_RegMem(OP::L4RegMem, node, regLow, MRef_Bdisp32(regLow, offsetof(J9JavaVM, portLibrary), cg), cg);
            Inst_RegMem(OP::SUB4RegMem, node, eaxReal,
                MRef_Bdisp32(regLow, offsetof(J9PortLibrary, nanoTimeMonotonicClockDelta), cg), cg);
            Inst_RegMem(OP::SBB4RegMem, node, edxReal,
                MRef_Bdisp32(regLow, offsetof(J9PortLibrary, nanoTimeMonotonicClockDelta) + 4, cg), cg);
        }
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

        // store it back
        Inst_MemReg(OP::S4MemReg, node, MRef_Bdisp32(resultAddress, 0, cg), eaxReal, cg);
        Inst_MemReg(OP::S4MemReg, node, MRef_Bdisp32(resultAddress, 4, cg), edxReal, cg);

        cg->stopUsingRegister(eaxReal);
        cg->stopUsingRegister(edxReal);
        cg->stopUsingRegister(reglow);

        TR::Register *lowReg = cg->allocateRegister();
        TR::Register *highReg = cg->allocateRegister();

        if (node->getNumChildren() == 1) {
            if (node->getReferenceCount() > 1
                || cg->getCurrentEvaluationTreeTop()->getNode()->getOpCodeValue() != TR::treetop) {
                Inst_RegMem(OP::L4RegMem, node, lowReg, MRef_Bdisp32(resultAddress, 0, cg), cg);
                Inst_RegMem(OP::L4RegMem, node, highReg, MRef_Bdisp32(resultAddress, 4, cg), cg);

                TR::RegisterPair *result = cg->allocateRegisterPair(lowReg, highReg);
                node->setRegister(result);
            }
            cg->decReferenceCount(node->getFirstChild());
        } else {
            // The result of the call is now on the stack. Get it into registers.
            //
            Inst_Reg(OP::POPReg, node, lowReg, cg);
            Inst_Reg(OP::POPReg, node, highReg, cg);
            TR::RegisterPair *result = cg->allocateRegisterPair(lowReg, highReg);
            node->setRegister(result);
        }
    } else {
        // This code is busted. The hires clock is measured in microseconds, not
        // nanoseconds, and this code doesn't correct for that. The above code
        // will be faster anyway, and it should be upgraded to support AOT, so
        // then we'll never need the hires clock version again.
        static char *useHiResClock = feGetEnv("TR_useHiResClock");
        if (!useHiResClock)
            return false;
        // Leave space on the stack for the 64-bit result
        //
        temp2 = cg->allocateRegister();
        Inst_RegMem(OP::L4RegMem, node, temp2, MRef_Bdisp32(vmThreadReg, offsetof(J9VMThread, javaVM), cg), cg);
        Inst_RegMem(OP::L4RegMem, node, temp2, MRef_Bdisp32(temp2, offsetof(J9JavaVM, portLibrary), cg), cg);
        Inst_Reg(OP::PUSHReg, node, espReal, cg);
        Inst_Reg(OP::PUSHReg, node, temp2, cg);

        int32_t extraFPDeps = (uint8_t)(cg->machine()->getLastXMMR() - cg->machine()->getFirstXMMR() + 1);

        // Build register dependencies and call the method in the port library
        // directly. Since this is a "C"-style call, ebx, esi and edi are preserved
        //
        TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, (uint8_t)4 + extraFPDeps, cg);
        TR::Register *temp1 = cg->allocateRegister();
        deps->addPostCondition(temp1, TR::RealRegister::ecx, cg);
        cg->stopUsingRegister(temp1);

        TR::Register *lowReg = cg->allocateRegister();
        deps->addPostCondition(lowReg, TR::RealRegister::eax, cg);

        TR::Register *highReg = cg->allocateRegister();
        deps->addPostCondition(highReg, TR::RealRegister::edx, cg);

        deps->addPostCondition(cg->getMethodMetaDataRegister(), TR::RealRegister::ebp, cg);

        // add the XMM dependencies
        addFPXMMDependencies(cg, deps);
        deps->stopAddingConditions();

        Inst_CallMem(OP::CALLMem, node, MRef_Bdisp32(temp2, offsetof(OMRPortLibrary, time_hires_clock), cg), deps, cg);
        cg->stopUsingRegister(temp2);

        Inst_RegImm(OP::ADD4RegImms, node, espReal, 8, cg);

        TR::RegisterPair *result = cg->allocateRegisterPair(lowReg, highReg);
        node->setRegister(result);
    }

    return true;
}
#endif
#endif // LINUX

TR::Register *J9::X86::TreeEvaluator::inlineMathFma(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Node *firstChild = node->getFirstChild();
    TR::Node *secondChild = node->getSecondChild();
    TR::Node *thirdChild = node->getThirdChild();

    TR::Register *lhsReg = NULL;
    TR::Register *midReg = NULL;
    TR::Register *rhsReg = NULL;
    TR::Register *result = cg->allocateRegister(TR_FPR);

    bool memLoadLhs
        = !firstChild->getRegister() && firstChild->getReferenceCount() == 1 && firstChild->getOpCode().isLoadVar();

    bool memLoadMiddle
        = !secondChild->getRegister() && secondChild->getReferenceCount() == 1 && secondChild->getOpCode().isLoadVar();

    bool memLoadRhs
        = !thirdChild->getRegister() && thirdChild->getReferenceCount() == 1 && thirdChild->getOpCode().isLoadVar();

    bool is64Bit = node->getDataType().isDouble();

    OP::Mnemonic fpMovRegRegOpcode = is64Bit ? OP::MOVSDRegReg : OP::MOVSSRegReg;
    result->setIsSinglePrecision(!is64Bit);

    TR_ASSERT_FATAL(cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_X86_FMA),
        "Cannot generate inline fma implementation without FMA extensions");

    // Choose fma instruction carefully, based on operand form, to reduce number of copies
    if (memLoadLhs) {
        OP::Mnemonic opcode = is64Bit ? OP::VFMADD231SDRegRegMem : OP::VFMADD231SSRegRegMem;
        TR::MemoryReference *lhsMR = MRef_node(firstChild, cg);

        if (memLoadRhs) {
            // a (2) * b (3) + c (1)
            TR::MemoryReference *rhsMR = MRef_node(thirdChild, cg);
            Inst_RegMem(OP::MOVSRegMem(is64Bit), node, result, rhsMR, cg);

            midReg = cg->evaluate(secondChild);
            memLoadMiddle = false; // No choice but to evaluate;
            Inst_RegRegMem(opcode, node, result, midReg, lhsMR, cg);
        } else if (memLoadMiddle) {
            // fma = a (1) * b (3) + c (2)
            opcode = is64Bit ? OP::VFMADD132SDRegRegMem : OP::VFMADD132SSRegRegMem;

            TR::MemoryReference *midMR = MRef_node(secondChild, cg);
            rhsReg = cg->evaluate(thirdChild);

            Inst_RegMem(OP::MOVSRegMem(is64Bit), node, result, lhsMR, cg);
            Inst_RegRegMem(opcode, node, result, rhsReg, midMR, cg);
        } else {
            // fma = a (2) * b (3) + c (1)
            midReg = cg->evaluate(secondChild);
            rhsReg = cg->evaluate(thirdChild);
            Inst_RegReg(fpMovRegRegOpcode, node, result, rhsReg, cg);
            Inst_RegRegMem(opcode, node, result, midReg, lhsMR, cg);
        }
    } else if (memLoadMiddle) {
        TR::MemoryReference *midMR = MRef_node(secondChild, cg);
        lhsReg = cg->evaluate(firstChild);

        if (memLoadRhs) {
            // fma = a (2) * b (1) + c (3)
            OP::Mnemonic opcode = is64Bit ? OP::VFMADD213SDRegRegMem : OP::VFMADD213SSRegRegMem;
            TR::MemoryReference *rhsMR = MRef_node(thirdChild, cg);

            Inst_RegMem(OP::MOVSRegMem(is64Bit), node, result, midMR, cg);
            Inst_RegRegMem(opcode, node, result, lhsReg, rhsMR, cg);
        } else {
            // fma = a (1) * b (3) + c (2)
            OP::Mnemonic opcode = is64Bit ? OP::VFMADD132SDRegRegMem : OP::VFMADD132SSRegRegMem;
            rhsReg = cg->evaluate(thirdChild);

            Inst_RegReg(fpMovRegRegOpcode, node, result, lhsReg, cg);
            Inst_RegRegMem(opcode, node, result, rhsReg, midMR, cg);
        }
    } else if (memLoadRhs) {
        // fma = a (2) * b (1) + c (3)
        OP::Mnemonic opcode = is64Bit ? OP::VFMADD213SDRegRegMem : OP::VFMADD213SSRegRegMem;

        TR::MemoryReference *rhsMR = MRef_node(thirdChild, cg);
        lhsReg = cg->evaluate(firstChild);
        midReg = cg->evaluate(secondChild);

        Inst_RegReg(fpMovRegRegOpcode, node, result, lhsReg, cg);
        Inst_RegRegMem(opcode, node, result, midReg, rhsMR, cg);
    } else {
        // fma = a (2) * b (1) + c (3)
        OP::Mnemonic opcode = is64Bit ? OP::VFMADD213SDRegRegReg : OP::VFMADD213SSRegRegReg;

        lhsReg = cg->evaluate(firstChild);
        midReg = cg->evaluate(secondChild);
        rhsReg = cg->evaluate(thirdChild);

        Inst_RegReg(fpMovRegRegOpcode, node, result, lhsReg, cg);
        Inst_RegRegReg(opcode, node, result, midReg, rhsReg, cg);
    }

    if (memLoadLhs) {
        cg->recursivelyDecReferenceCount(firstChild);
    } else {
        cg->decReferenceCount(firstChild);
    }

    if (memLoadMiddle) {
        cg->recursivelyDecReferenceCount(secondChild);
    } else {
        cg->decReferenceCount(secondChild);
    }

    if (memLoadRhs) {
        cg->recursivelyDecReferenceCount(thirdChild);
    } else {
        cg->decReferenceCount(thirdChild);
    }

    node->setRegister(result);

    return result;
}

// Convert serial String.hashCode computation into vectorization copy and implement with SSE instruction
//
// Conversion process example:
//
//    str[8] = example string representing 8 characters (compressed or decompressed)
//
//    The serial method for creating the hash:
//          hash = 0, offset = 0, count = 8
//          for (int i = offset; i < offset+count; ++i) {
//                hash = (hash << 5) - hash + str[i];
//          }
//
//    Note that ((hash << 5) - hash) is equivalent to hash * 31
//
//    Expanding out the for loop:
//          hash = ((((((((0*31+str[0])*31+str[1])*31+str[2])*31+str[3])*31+str[4])*31+str[5])*31+str[6])*31+str[7])
//
//    Simplified:
//          hash =        (31^7)*str[0] + (31^6)*str[1] + (31^5)*str[2] + (31^4)*str[3]
//                      + (31^3)*str[4] + (31^2)*str[5] + (31^1)*str[6] + (31^0)*str[7]
//
//    Rearranged:
//          hash =        (31^7)*str[0] + (31^3)*str[4]
//                      + (31^6)*str[1] + (31^2)*str[5]
//                      + (31^5)*str[2] + (31^1)*str[6]
//                      + (31^4)*str[3] + (31^0)*str[7]
//
//    Factor out [31^3, 31^2, 31^1, 31^0]:
//          hash =        31^3*((31^4)*str[0] + str[4])           Vector[0]
//                      + 31^2*((31^4)*str[1] + str[5])           Vector[1]
//                      + 31^1*((31^4)*str[2] + str[6])           Vector[2]
//                      + 31^0*((31^4)*str[3] + str[7])           Vector[3]
//
//    Keep factoring out any 31^4 if possible (this example has no such case). If the string was 12 characters long
//    then:
//          31^3*((31^8)*str[0] + (31^4)*str[4] + (31^0)*str[8]) would become 31^3*(31^4((31^4)*str[0] + str[4]) +
//          (31^0)*str[8])
//
//    Vectorization is done by simultaneously calculating the four sums that hash is made of (each -> is a successive
//    step):
//          Vector[0] = str[0] -> multiply 31^4 -> add str[4] -> multiply 31^3
//          Vector[1] = str[1] -> multiply 31^4 -> add str[5] -> multiply 31^2
//          Vector[2] = str[2] -> multiply 31^4 -> add str[6] -> multiply 31^1
//          Vector[3] = str[3] -> multiply 31^4 -> add str[7] -> multiply 1
//
//    Adding these four vectorized values together produces the required hash.
//    If the number of characters in the string is not a multiple of 4, then the remainder of the hash is calculated
//    serially.
//
// Implementation overview:
//
// start_label
// if size < threshold, goto serial_label, current threshold is 4
//    xmm0 = load 16 bytes align constant [923521, 923521, 923521, 923521]
//    xmm1 = 0
// SSEloop
//    xmm2 = decompressed: load 8 byte value in lower 8 bytes.
//           compressed: load 4 byte value in lower 4 bytes
//    xmm1 = xmm1 * xmm0
//    if(isCompressed)
//          movzxbd xmm2, xmm2
//    else
//          movzxwd xmm2, xmm2
//    xmm1 = xmm1 + xmm2
//    i = i + 4;
//    cmp i, end -3
//    jge SSEloop
// xmm0 = load 16 bytes align [31^3, 31^2, 31, 1]
// xmm1 = xmm1 * xmm0      value contains [a0, a1, a2, a3]
// xmm0 = xmm1
// xmm0 = xmm0 >> 64 bits
// xmm1 = xmm1 + xmm0       reduce add [a0+a2, a1+a3, .., ...]
// xmm0 = xmm1
// xmm0 = xmm0 >> 32 bits
// xmm1 = xmm1 + xmm0       reduce add [a0+a2 + a1+a3, .., .., ..]
// movd xmm1, GPR1
//
// serial_label
//
// cmp i end
// jle end
// serial_loop
// GPR2 = GPR1
// GPR1 = GPR1 << 5
// GPR1 = GPR1 - GPR2
// GPR2 = load c[i]
// add GPR1, GPR2
// dec i
// cmp i, end
// jl serial_loop
//
// end_label
static TR::Register *inlineStringHashCode(TR::Node *node, bool isCompressed, TR::CodeGenerator *cg)
{
    TR_ASSERT(node->getChild(1)->getOpCodeValue() == TR::iconst && node->getChild(1)->getInt() == 0,
        "String hashcode offset can only be const zero.");

    const int size = 4;
    auto shift = isCompressed ? 0 : 1;

    auto address = cg->evaluate(node->getChild(0));
    auto length = cg->evaluate(node->getChild(2));
    auto index = cg->allocateRegister();
    auto hash = cg->allocateRegister();
    auto tmp = cg->allocateRegister();
    auto hashXMM = cg->allocateRegister(TR_VRF);
    auto tmpXMM = cg->allocateRegister(TR_VRF);
    auto multiplierXMM = cg->allocateRegister(TR_VRF);

    auto begLabel = generateLabelSymbol(cg);
    auto endLabel = generateLabelSymbol(cg);
    auto loopLabel = generateLabelSymbol(cg);
    begLabel->setStartInternalControlFlow();
    endLabel->setEndInternalControlFlow();
    auto deps = RegDeps((uint8_t)6, (uint8_t)6, cg);
    deps->addPreCondition(address, TR::RealRegister::NoReg, cg);
    deps->addPreCondition(index, TR::RealRegister::NoReg, cg);
    deps->addPreCondition(length, TR::RealRegister::NoReg, cg);
    deps->addPreCondition(multiplierXMM, TR::RealRegister::NoReg, cg);
    deps->addPreCondition(tmpXMM, TR::RealRegister::NoReg, cg);
    deps->addPreCondition(hashXMM, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(address, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(index, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(length, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(multiplierXMM, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(tmpXMM, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(hashXMM, TR::RealRegister::NoReg, cg);

    Inst_RegReg(OP::MOV4RegReg, node, index, length, cg);
    Inst_RegImm(OP::AND4RegImms, node, index, size - 1, cg); // mod size
    Inst_RegMem(OP::CMOVE4RegMem, node, index, MRef_const(cg->findOrCreate4ByteConstant(node, size), cg), cg);

    // Prepend zeros
    {
        TR::Compilation *comp = cg->comp();

        static uint64_t MASKDECOMPRESSED[] = { 0x0000000000000000ULL, 0xffffffffffffffffULL };
        static uint64_t MASKCOMPRESSED[] = { 0xffffffff00000000ULL, 0x0000000000000000ULL };
        Inst_RegMem(isCompressed ? OP::MOVDRegMem : OP::MOVQRegMem, node, hashXMM,
            MRef_BISdisp32(address, index, shift,
                -(size << shift) + TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg),
            cg);
        Inst_RegMem(OP::LEARegMem(), node, tmp,
            MRef_const(cg->findOrCreate16ByteConstant(node, isCompressed ? MASKCOMPRESSED : MASKDECOMPRESSED), cg), cg);

        auto mr = MRef_BISdisp32(tmp, index, shift, 0, cg);
        if (comp->target().cpu.supportsAVX()) {
            Inst_RegMem(OP::PANDRegMem, node, hashXMM, mr, cg);
        } else {
            Inst_RegMem(OP::MOVDQURegMem, node, tmpXMM, mr, cg);
            Inst_RegReg(OP::PANDRegReg, node, hashXMM, tmpXMM, cg);
        }
        Inst_RegReg(isCompressed ? OP::PMOVZXBDRegReg : OP::PMOVZXWDRegReg, node, hashXMM, hashXMM, cg);
    }

    // Reduction Loop
    {
        static uint32_t multiplier[] = { 31 * 31 * 31 * 31, 31 * 31 * 31 * 31, 31 * 31 * 31 * 31, 31 * 31 * 31 * 31 };
        Inst_Label(OP::label, node, begLabel, cg);
        Inst_RegReg(OP::CMP4RegReg, node, index, length, cg);
        Inst_Label(OP::JGE4, node, endLabel, cg);
        Inst_RegMem(OP::MOVDQURegMem, node, multiplierXMM,
            MRef_const(cg->findOrCreate16ByteConstant(node, multiplier), cg), cg);
        Inst_Label(OP::label, node, loopLabel, cg);
        Inst_RegReg(OP::PMULLDRegReg, node, hashXMM, multiplierXMM, cg);
        Inst_RegMem(isCompressed ? OP::PMOVZXBDRegMem : OP::PMOVZXWDRegMem, node, tmpXMM,
            MRef_BISdisp32(address, index, shift, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg), cg);
        Inst_RegImm(OP::ADD4RegImms, node, index, 4, cg);
        Inst_RegReg(OP::PADDDRegReg, node, hashXMM, tmpXMM, cg);
        Inst_RegReg(OP::CMP4RegReg, node, index, length, cg);
        Inst_Label(OP::JL4, node, loopLabel, cg);
        Inst_Label(OP::label, node, endLabel, deps, cg);
    }

    // Finalization
    {
        static uint32_t multiplier[] = { 31 * 31 * 31, 31 * 31, 31, 1 };
        Inst_RegMem(OP::PMULLDRegMem, node, hashXMM, MRef_const(cg->findOrCreate16ByteConstant(node, multiplier), cg),
            cg);
        Inst_RegRegImm(OP::PSHUFDRegRegImm1, node, tmpXMM, hashXMM, 0x0e, cg);
        Inst_RegReg(OP::PADDDRegReg, node, hashXMM, tmpXMM, cg);
        Inst_RegRegImm(OP::PSHUFDRegRegImm1, node, tmpXMM, hashXMM, 0x01, cg);
        Inst_RegReg(OP::PADDDRegReg, node, hashXMM, tmpXMM, cg);
    }

    Inst_RegReg(OP::MOVDReg4Reg, node, hash, hashXMM, cg);

    cg->stopUsingRegister(index);
    cg->stopUsingRegister(tmp);
    cg->stopUsingRegister(hashXMM);
    cg->stopUsingRegister(tmpXMM);
    cg->stopUsingRegister(multiplierXMM);

    node->setRegister(hash);
    cg->decReferenceCount(node->getChild(0));
    cg->recursivelyDecReferenceCount(node->getChild(1));
    cg->decReferenceCount(node->getChild(2));
    return hash;
}

TR::Register *J9::X86::TreeEvaluator::inlineVectorizedHashCode(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Node *initialValueNode = node->getChild(3);
    TR::Node *elementTypeNode = node->getChild(4);
    TR::Register *registerHash = NULL;

    switch (elementTypeNode->getConstValue()) {
        case 4: // T_BOOLEAN
            registerHash = vectorizedHashCodeHelper(node, TR::Int8, initialValueNode, false, cg);
            break;
        case 8: // T_BYTE
            registerHash = vectorizedHashCodeHelper(node, TR::Int8, initialValueNode, true, cg);
            break;
        case 5: // T_CHAR
            registerHash = vectorizedHashCodeHelper(node, TR::Int16, initialValueNode, false, cg);
            break;
        case 9: // T_SHORT
            registerHash = vectorizedHashCodeHelper(node, TR::Int16, initialValueNode, true, cg);
            break;
        case 10: // T_INT
            registerHash = vectorizedHashCodeHelper(node, TR::Int32, initialValueNode, true, cg);
            break;
        default:
            return NULL;
    }

    if (registerHash != NULL)
        cg->decReferenceCount(elementTypeNode);

    node->setRegister(registerHash);

    return registerHash;
}

TR::Register *J9::X86::TreeEvaluator::vectorizedHashCodeReductionHelper(TR::Node *node, TR::Register **vectorRegisters,
    int32_t numVectors, TR::Register *tmpVectorRegVRF, TR::Register *result, TR::VectorLength vl, TR::DataType dt,
    TR::CodeGenerator *cg)
{
    TR::InstOpCode opcode = OP::PADDDRegReg;
    TR::Register *vectorRegVRF = vectorRegisters[0];

    // If we unrolled the main loop, vertically add the vectors together first
    // then proceed to do horizontal reduction
    for (int32_t i = 1; i < numVectors; i++) {
        OMR::X86::Encoding opcodeEncoding = opcode.getSIMDEncoding(&cg->comp()->target().cpu, vl);
        Inst_RegReg(OP::PADDDRegReg, node, vectorRegVRF, vectorRegisters[i], cg, opcodeEncoding);
    }

    // Reduce lanes -> horizontally add all vector elements together
    // Store the result in a GPR

    switch (vl) {
        case TR::VectorLength512:
            // extract 256-bits from zmm and store in ymm, then perform vertical operation
            Inst_RegRegImm(OP::VEXTRACTF64X4YmmZmmImm1, node, tmpVectorRegVRF, vectorRegVRF, 0xFF, cg);
            Inst_RegReg(opcode.getMnemonic(), node, vectorRegVRF, tmpVectorRegVRF, cg,
                opcode.getSIMDEncoding(&cg->comp()->target().cpu, TR::VectorLength256));
            // Fallthrough to treat remaining result as 256-bit vector
        case TR::VectorLength256:
            // extract 128 bits from ymm and store in xmm, then perform vertical operation
            Inst_RegRegImm(OP::VEXTRACTF128RegRegImm1, node, tmpVectorRegVRF, vectorRegVRF, 0xFF, cg);
            Inst_RegReg(opcode.getMnemonic(), node, vectorRegVRF, tmpVectorRegVRF, cg,
                opcode.getSIMDEncoding(&cg->comp()->target().cpu, TR::VectorLength128));
            // Fallthrough to treat remaining result as 128-bit vector
        case TR::VectorLength128:
            Inst_RegRegImm(OP::PSHUFDRegRegImm1, node, tmpVectorRegVRF, vectorRegVRF, 0x0e, cg);
            Inst_RegReg(OP::PADDDRegReg, node, vectorRegVRF, tmpVectorRegVRF, cg);
            Inst_RegRegImm(OP::PSHUFDRegRegImm1, node, tmpVectorRegVRF, vectorRegVRF, 0x01, cg);
            Inst_RegReg(OP::PADDDRegReg, node, vectorRegVRF, tmpVectorRegVRF, cg);
            break;
        default:
            TR_ASSERT_FATAL(false, "Unsupported vector length");
    }

    Inst_RegReg(OP::MOVDReg4Reg, node, result, vectorRegVRF, cg);
    return result;
}

// 31^64, 31^63, ..., 31^0
static const int32_t powersOf31[65] = { 1304393729, 2120287199, -208698303, -1807847521, -1166696319, 100911967,
    280349889, -1930619105, 630458625, 20337375, 693392705, 438009503, -1925533311, 769170015, 1133190593, -240540129,
    -7759359, 969581023, 1970939457, -1183347297, -1700740479, -1024693921, -448696639, 124073247, -1935660287,
    1461579999, -922683583, 1632803999, 329765761, 1950300255, 1725480897, 1025491999, 2111290369, -2010103841,
    350799937, 11316127, 693101697, -254736545, 961614017, 31019807, -2077209343, -67006753, 1244764481, -2038056289,
    211350913, -408824225, -844471871, -997072353, 1353309697, -510534177, 1507551809, -505558625, -293403007,
    129082719, -1796951359, -196513505, -1807454463, 1742810335, 887503681, 28629151, 923521, 29791, 961, 31, 1 };

//
// This function generates the main vectorized loop in the vectorizedHashCode(...) implementation. It supports both
// signed and unsigned integer elements up to 32-bits in size, vector lengths from 128-bit up to 512-bit and up to 4x
// loop unrolling.
//
// This helper generates code in three sections,
//   1. Setup registers, load multiplier constants
//     a. Initialize vector constants used in step 2.
//     b. Zero out running hash vectors. The number of running hash vectors is equal to the unrolling factor.
//     c. If an initial hash is non-zero, move that value into the first element of the first running hash vector.
//   2. Generate main loop with x unrolling
//     a. Load batch of data using size appropriate vector load, with zero extension for unsigned types, sign extension
//        for signed types.
//     b. Multiply batch by (31^(n-1), 31^(n-2), ..., 31^0), where n is the number of elements being processed in the
//     loop
//       i. In case of a loop unroll x times, the multiplier is split into x vectors. The first batch of elements is
//          multiplied by the higher powers of 31. The next batch of elements are multiplied by the next multiplier
//          vector, and so on.
//     c. Multiply the existing running hash the vector (31^n, 31^n, ..., 31^n)
//     d. Add product from 2 (b) to the running hash.
//   3. Combine and reduces vectors into a single result
//     a. If the main loop is unrolled, add the running hash vectors together.
//     b. Horizontally add the elements together and move the result to a generate-purpose register.
//
TR::Register *J9::X86::TreeEvaluator::vectorizedHashCodeLoopHelper(TR::Node *node, TR::DataType dt, TR::VectorLength vl,
    bool isSigned, TR::Register *result, TR::Register *initialHash, TR::Register *index, TR::Register *length,
    TR::Register *arrayAddress, int32_t unrollCount, TR::CodeGenerator *cg)
{
    static OMR::X86::Encoding vectorEncodingMethods[3] = { OMR::X86::Default, OMR::X86::VEX_L256, OMR::X86::EVEX_L512 };
    static int32_t vectorSizes[3] = { 4, 8, 16 };
    int32_t shift = dt - TR::Int8; /* i8 -> 0, i16 -> 1, i32 -> 2 */

    TR_ASSERT_FATAL(shift >= 0 && shift <= 2, "Unsupported datatype for vectorized hashcode");
    TR_ASSERT_FATAL(unrollCount == 1 || unrollCount == 2 || unrollCount == 4, "Unroll count must be 1/2/4");
    TR_ASSERT_FATAL(vl >= TR::VectorLength128 && vl <= TR::VectorLength512, "Unsupported vector length");

    OMR::X86::Encoding vectorEncoding = vectorEncodingMethods[vl - TR::VectorLength128];
    int32_t vectorSizeElements = vectorSizes[vl - TR::VectorLength128];
    int32_t numElements = vectorSizeElements * unrollCount;

    TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, (uint8_t)11, cg);
    TR::Register *tmp = cg->allocateRegister(TR_GPR);
    TR::Register *tmpVRF = cg->allocateRegister(TR_VRF);
    TR::Register *multiplierVRF = cg->allocateRegister(TR_VRF);

    TR::Register *hashRegsVRF[4];
    TR::Register *multiplier31PowNRegsVRF[4];

    deps->addPostCondition(tmp, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(tmpVRF, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(multiplierVRF, TR::RealRegister::NoReg, cg);

    for (int32_t i = 0; i < unrollCount; i++) {
        hashRegsVRF[i] = cg->allocateRegister(TR_VRF);
        multiplier31PowNRegsVRF[i] = cg->allocateRegister(TR_VRF);

        deps->addPostCondition(hashRegsVRF[i], TR::RealRegister::NoReg, cg);
        deps->addPostCondition(multiplier31PowNRegsVRF[i], TR::RealRegister::NoReg, cg);
    }

    deps->stopAddingConditions();

    TR::LabelSymbol *begLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);

    begLabel->setStartInternalControlFlow();
    endLabel->setEndInternalControlFlow();

    Inst_RegReg(OP::MOVRegReg(), node, result, initialHash, cg);
    Inst_Label(OP::label, node, begLabel, cg);
    Inst_RegReg(OP::MOVRegReg(), node, tmp, length, cg);
    Inst_RegImm(OP::AND4RegImm4, node, tmp, ~(numElements - 1), cg);

    {
        Inst_RegReg(OP::CMP4RegReg, node, index, tmp, cg);
        Inst_Label(OP::JGE4, node, endLabel, cg);

        // Initialize Constants outside of loop body but after the first compare
        for (int32_t i = 0; i < unrollCount; i++)
            Inst_RegReg(OP::PXORRegReg, node, hashRegsVRF[i], hashRegsVRF[i], cg, vectorEncoding);

        Inst_RegReg(OP::MOVDRegReg4, node, hashRegsVRF[0], initialHash, cg);

        int32_t multiplier31PowNData[16];
        // Fill multiplier array with 31^numElements
        std::fill_n(multiplier31PowNData, 16, powersOf31[64 - numElements]);
        Inst_RegMem(OP::MOVDQURegMem, node, multiplierVRF,
            MRef_const(
                cg->findOrCreateConstantDataSnippet(node, multiplier31PowNData, vectorSizeElements * sizeof(int32_t)),
                cg),
            cg, vectorEncoding);

        for (int32_t i = 0; i < unrollCount; i++) {
            // Based on the unrolling factor (x), we need to split multiplier constant (powers of 31) into x vectors.
            //   The constant is as follows (31^(n-1), 31^(n-2), ..., 31^0)
            // Where n is the number of elements processed in the loop. n is directly proportional to the unrolling
            // factor (x)
            //
            // For example, given an unrolling factor (x) of 4, and a vector length of 128-bits, we will process up to
            // 16 elements per iteration of the main loop. This means the multiplication vectors are as follows:
            //
            //   multiplier31PowNRegsVRF[0] = (31^15, 31^14, 31^13, 31^12),
            //   multiplier31PowNRegsVRF[1] = (31^11, 31^10, 31^9, 31^8),
            //   multiplier31PowNRegsVRF[2] = (31^7, 31^6, 31^5, 31^4),
            //   multiplier31PowNRegsVRF[3] = (31^3, 31^2, 31^1, 31^0),
            //
            TR::Register *multiplier31PowN_i = multiplier31PowNRegsVRF[i];
            const int32_t vectorSize = vectorSizeElements * 4;
            int32_t offset = sizeof(powersOf31) / sizeof(int32_t) - (vectorSizeElements * (unrollCount - i));

            int32_t *multiplier = const_cast<int32_t *>(powersOf31 + offset);
            TR::MemoryReference *mr = MRef_const(cg->findOrCreateConstantDataSnippet(node, multiplier, vectorSize), cg);

            Inst_RegMem(OP::MOVDQURegMem, node, multiplier31PowN_i, mr, cg, vectorEncoding);
        }
    }

    Inst_Label(OP::label, node, loopLabel, cg);

    {
        // Main loop body;

        for (int32_t i = 0; i < unrollCount; i++) {
            // Load in the next batch of elements. (sign/zero) extend i8, i16 to i32
            OP::Mnemonic loadOpcode = OP::bad;
            int32_t elementSize;

            switch (dt) {
                case TR::Int8:
                    loadOpcode = isSigned ? OP::PMOVSXBDRegMem : OP::PMOVZXBDRegMem;
                    elementSize = 1;
                    break;
                case TR::Int16:
                    loadOpcode = isSigned ? OP::PMOVSXWDRegMem : OP::PMOVZXWDRegMem;
                    elementSize = 2;
                    break;
                case TR::Int32:
                    loadOpcode = OP::MOVDQURegMem;
                    elementSize = 4;
                    break;
                default:
                    TR_ASSERT_FATAL(false, "Unsupported element type");
                    break;
            }

            int32_t displacement
                = TR::Compiler->om.contiguousArrayHeaderSizeInBytes() + i * (vectorSizeElements * elementSize);
            TR::MemoryReference *mr = MRef_BISdisp32(arrayAddress, index, shift, displacement, cg);
            // load next batch of data
            Inst_RegMem(loadOpcode, node, tmpVRF, mr, cg, vectorEncoding);

            // tmpVRF = tmpVRF * multiplierVRF
            Inst_RegReg(OP::PMULLDRegReg, node, tmpVRF, multiplier31PowNRegsVRF[i], cg, vectorEncoding);
            // hashRegsVRF = ( hashRegsVRF * {31^vl, ..., 31^vl} ) + tmpVRF
            Inst_RegReg(OP::PMULLDRegReg, node, hashRegsVRF[i], multiplierVRF, cg, vectorEncoding);
            Inst_RegReg(OP::PADDDRegReg, node, hashRegsVRF[i], tmpVRF, cg, vectorEncoding);
        }
    }

    // Increase loop index by the number of processed elements
    Inst_RegImm(OP::ADD4RegImms, node, index, numElements, cg);
    // Compare index with numElements and loop back if necessary
    Inst_RegReg(OP::CMP4RegReg, node, index, tmp, cg);
    Inst_Label(OP::JL4, node, loopLabel, cg);

    vectorizedHashCodeReductionHelper(node, hashRegsVRF, unrollCount, tmpVRF, result, vl, dt, cg);
    Inst_Label(OP::label, node, endLabel, deps, cg);

    cg->stopUsingRegister(tmp);
    cg->stopUsingRegister(tmpVRF);
    cg->stopUsingRegister(multiplierVRF);

    for (int32_t i = 0; i < unrollCount; i++)
        cg->stopUsingRegister(multiplier31PowNRegsVRF[i]);
    for (int32_t i = 0; i < unrollCount; i++)
        cg->stopUsingRegister(hashRegsVRF[i]);

    return result;
}

/**
 * @brief Implements the vectorized `hashCode` computation using SIMD instructions.
 *
 * This implementation supports various processor microarchitectures (sse4.1+), enabling vectorization
 * with 128-bit, 256-bit, and 512-bit vectors. It handles both signed and unsigned integer element types
 * of 8-bit, 16-bit, and 32-bit sizes. Elements are processed iteratively as 32-bit integers, with
 * smaller types being sign- or zero-extended to 32-bit integers.
 *
 * The following steps are performed to generate the vectorized `hashCode` implementation:
 *
 * 1. Generate the main vectorized loop unrolled by a factor of x (default is 4) at the highest
 *    available vector length.
 * 2. Generate a secondary vectorized loop, not unrolled, using 128-bit vectors.
 *    - This secondary loop is necessary because small amounts of data or residual data
 *      may not fit into the unrolled main loop, which often requires large amounts of
 *      data to process efficiently. By using a simpler 128-bit vectorized loop, better
 *      performance can be achieved for these cases.
 * 3. Process any remaining elements sequentially:
 *    @code
 *    for (; index < length; index++) { hash = 31 * hash + arr[index]; }
 *    @endcode
 *
 * The implementation relies on two helper functions:
 * - `vectorizedHashCodeLoopHelper(...)`:
 *   Generates the vectorized loop code for the specified type and vector size.
 * - `vectorizedHashCodeReductionHelper(...)`:
 *   Generates code to reduce x vectors by summing all elements together into a scalar hashCode value.
 *
 * @note Future enhancements could investigate optimal unrolling factors based on:
 * - Block hotness
 * - Array length
 * - Cache implications of unrolling
 *
 * Given the large expected size of the generated code with unrolling, this intrinsic could
 * exert significant pressure on the code cache. This effect may be especially pronounced if
 * the vectorized hashCode algorithm is inlined multiple times in the same method.
 *
 * @param node      The input node to process.
 * @param dt        The data type of the elements.
 * @param nodeHash  The node representing the initial hash value.
 * @param isSigned  Indicates whether the elements are signed.
 * @param cg        The code generator instance.
 * @return The register containing the computed hashCode value.
 */
TR::Register *J9::X86::TreeEvaluator::vectorizedHashCodeHelper(TR::Node *node, TR::DataType dt, TR::Node *nodeHash,
    bool isSigned, TR::CodeGenerator *cg)
{
    int32_t shift = dt - TR::Int8; /* i8 -> 0, i16 -> 1, i32 -> 2 */

    TR_ASSERT_FATAL(shift >= 0 && shift <= 2, "Unsupported datatype for vectorized hashcode");

    TR::Compilation *comp = cg->comp();
    TR::VectorLength vl = cg->getMaxPreferredVectorLength();
    TR::Node *addressNode = node->getChild(0);

    bool nonZeroOffset = node->getChild(1)->getOpCodeValue() != TR::iconst || node->getChild(1)->getInt() != 0;
    bool addressIs64bits = TR::TreeEvaluator::getNodeIs64Bit(addressNode, cg);

    TR::Register *address = nonZeroOffset
        ? TR::TreeEvaluator::intOrLongClobberEvaluate(addressNode, addressIs64bits, cg)
        : cg->evaluate(addressNode);
    TR::Register *length = cg->evaluate(node->getChild(2));
    TR::Register *initHash = nodeHash ? cg->intClobberEvaluate(nodeHash) : cg->allocateRegister(TR_GPR);
    TR::Register *index = cg->allocateRegister();
    TR::Register *result = cg->allocateRegister();
    TR::Register *tmp = cg->allocateRegister();

    TR::RegisterDependencyConditions *deps = RegDeps(0, 6, cg);

    deps->addPostCondition(result, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(address, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(index, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(tmp, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(initHash, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(length, TR::RealRegister::NoReg, cg);
    deps->stopAddingConditions();

    if (nonZeroOffset) {
        TR::Register *offset = cg->evaluate(node->getChild(1));
        TR::MemoryReference *memRef = MRef_BISdisp32(address, offset, shift, 0, cg);
        Inst_RegMem(OP::LEARegMem(), node, address, memRef, cg);
    }

    if (!nodeHash) {
        // If nodeHash is not provided, assume initial hash value of 0.
        Inst_RegReg(OP::XOR4RegReg, node, initHash, initHash, cg);
    }

    // Set index ptr to 0
    Inst_RegReg(OP::XOR4RegReg, node, index, index, cg);

    // Generate Main Loop; 4x Unrolled seems to yield the best performance for large arrays
    static char *unrollVar = feGetEnv("TR_setInlineVectorHashCodeUnrollCount");

#ifdef TR_TARGET_64BIT
    int32_t unrollCount = unrollVar ? atoi(unrollVar) : 4;
#else
    int32_t unrollCount = 1;
#endif

    vectorizedHashCodeLoopHelper(node, dt, vl, isSigned, result, initHash, index, length, address, unrollCount, cg);

    static bool disableSecondLoop = feGetEnv("TR_disableVectorHashCodeSecondLoop") != NULL;

    // Generate a second vectorized loop if not disabled and Vl/unrollCount are not the same as the first loop
    if (!disableSecondLoop && (unrollCount != 1 || vl != TR::VectorLength128)) {
        Inst_RegReg(OP::MOV4RegReg, node, initHash, result, cg);
        vectorizedHashCodeLoopHelper(node, dt, TR::VectorLength128, isSigned, result, initHash, index, length, address,
            1, cg);
    }

    // handle residual elements sequentially
    // for (; index < length; index++) { hash = 31 * hash + arr[index]; }
    {
        TR::LabelSymbol *residueBeginLoopLabel = generateLabelSymbol(cg);
        TR::LabelSymbol *residueEndLoopLabel = generateLabelSymbol(cg);
        TR::LabelSymbol *residueLoopLabel = generateLabelSymbol(cg);

        residueBeginLoopLabel->setStartInternalControlFlow();
        residueEndLoopLabel->setEndInternalControlFlow();

        Inst_Label(OP::label, node, residueBeginLoopLabel, cg);
        Inst_RegReg(OP::CMP4RegReg, node, index, length, cg);
        Inst_Label(OP::JGE4, node, residueEndLoopLabel, cg);
        Inst_Label(OP::label, node, residueLoopLabel, cg);

        // hash = 31 * hash + arr[index] = tmp * hash + arr[index]
        Inst_RegRegImm(OP::IMUL4RegRegImm4, node, result, result, 31, cg);

        static OP::Mnemonic signedLoadOpcode[3] = { OP::MOVSXReg4Mem1, OP::MOVSXReg4Mem2, OP::L4RegMem };
        static OP::Mnemonic unsignedLoadOpcode[3] = { OP::MOVZXReg4Mem1, OP::MOVZXReg4Mem2, OP::L4RegMem };
        OP::Mnemonic loadOpcode = isSigned ? signedLoadOpcode[dt - TR::Int8] : unsignedLoadOpcode[dt - TR::Int8];

        Inst_RegMem(loadOpcode, node, tmp,
            MRef_BISdisp32(address, index, shift, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg), cg);
        Inst_RegReg(OP::ADDRegReg(), node, result, tmp, cg);

        // Increase loop index by the number of processed elements
        Inst_Reg(OP::INCReg(), node, index, cg);

        // Compare index with numElements and loop back if necessary
        Inst_RegReg(OP::CMP4RegReg, node, index, length, cg);
        Inst_Label(OP::JL4, node, residueLoopLabel, cg);
        Inst_Label(OP::label, node, residueEndLoopLabel, deps, cg);
    }

    if (nonZeroOffset) {
        cg->stopUsingRegister(address);
    }

    cg->stopUsingRegister(initHash);
    cg->stopUsingRegister(index);
    cg->stopUsingRegister(tmp);

    cg->decReferenceCount(node->getChild(0));
    cg->decReferenceCount(node->getChild(1));
    cg->decReferenceCount(node->getChild(2));

    if (nodeHash)
        cg->decReferenceCount(nodeHash);

    return result;
}

static bool getNodeIs64Bit(TR::Node *node, TR::CodeGenerator *cg)
{
    /* This function is intended to allow existing 32-bit instruction selection code
     * to be reused, almost unchanged, to do the corresponding 64-bit logic on AMD64.
     * It compiles away to nothing on IA32, thus preserving performance and code size
     * on IA32, while allowing the logic to be generalized to suit AMD64.
     *
     * Don't use this function for 64-bit logic on IA32; instead, either (1) use
     * separate logic, or (2) use a different test for 64-bitness.  Usually this is
     * not a hindrance, because 64-bit code on IA32 uses register pairs and other
     * things that are totally different from their 32-bit counterparts.
     */

    TR_ASSERT(cg->comp()->target().is64Bit() || node->getSize() <= 4,
        "64-bit nodes on 32-bit platforms shouldn't use getNodeIs64Bit");
    return cg->comp()->target().is64Bit() && node->getSize() > 4;
}

static TR::Register *intOrLongClobberEvaluate(TR::Node *node, bool nodeIs64Bit, TR::CodeGenerator *cg)
{
    if (nodeIs64Bit) {
        TR_ASSERT(getNodeIs64Bit(node, cg), "nodeIs64Bit must be consistent with node size");
        return cg->longClobberEvaluate(node);
    } else {
        TR_ASSERT(!getNodeIs64Bit(node, cg), "nodeIs64Bit must be consistent with node size");
        return cg->intClobberEvaluate(node);
    }
}

/**
 * \brief
 *   Generate inlined instructions equivalent to com/ibm/jit/JITHelpers.intrinsicIndexOfLatin1 or
 * com/ibm/jit/JITHelpers.intrinsicIndexOfUTF16
 *
 * \param node
 *   The tree node
 *
 * \param cg
 *   The Code Generator
 *
 * \param isLatin1
 *   True when the string is Latin1, False when the string is UTF16
 *
 * Note that this version does not support discontiguous arrays
 */
static TR::Register *inlineIntrinsicIndexOf(TR::Node *node, TR::CodeGenerator *cg, bool isLatin1)
{
    static uint8_t MASKOFSIZEONE[] = {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
    };
    static uint8_t MASKOFSIZETWO[] = {
        0x00,
        0x01,
        0x00,
        0x01,
        0x00,
        0x01,
        0x00,
        0x01,
        0x00,
        0x01,
        0x00,
        0x01,
        0x00,
        0x01,
        0x00,
        0x01,
    };

    uint8_t width = 16;
    uint8_t shift = 0;
    uint8_t *shuffleMask = NULL;
    auto compareOp = OP::bad;
    if (isLatin1) {
        shuffleMask = MASKOFSIZEONE;
        compareOp = OP::PCMPEQBRegReg;
        shift = 0;
    } else {
        shuffleMask = MASKOFSIZETWO;
        compareOp = OP::PCMPEQWRegReg;
        shift = 1;
    }

    // This evaluator function handles different indexOf() intrinsics, some of which are static calls without a
    // receiver. Hence, the need for static call check.
    const bool isStaticCall = node->getSymbolReference()->getSymbol()->castToMethodSymbol()->isStatic();
    const uint8_t firstCallArgIdx = isStaticCall ? 0 : 1;
    auto array = cg->evaluate(node->getChild(firstCallArgIdx));
    auto ch = cg->evaluate(node->getChild(firstCallArgIdx + 1));
    auto offset = cg->evaluate(node->getChild(firstCallArgIdx + 2));
    auto length = cg->evaluate(node->getChild(firstCallArgIdx + 3));

    auto ECX = cg->allocateRegister();
    auto result = cg->allocateRegister();
    auto scratch = cg->allocateRegister();
    auto scratchXMM = cg->allocateRegister(TR_VRF);
    auto valueXMM = cg->allocateRegister(TR_VRF);

    auto dependencies = RegDeps((uint8_t)7, (uint8_t)7, cg);
    dependencies->addPreCondition(ECX, TR::RealRegister::ecx, cg);
    dependencies->addPreCondition(array, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(length, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(result, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(scratch, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(scratchXMM, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(valueXMM, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(ECX, TR::RealRegister::ecx, cg);
    dependencies->addPostCondition(array, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(length, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(result, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(scratch, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(scratchXMM, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(valueXMM, TR::RealRegister::NoReg, cg);

    auto begLabel = generateLabelSymbol(cg);
    auto endLabel = generateLabelSymbol(cg);
    auto loopLabel = generateLabelSymbol(cg);
    begLabel->setStartInternalControlFlow();
    endLabel->setEndInternalControlFlow();

    Inst_RegReg(OP::MOVDRegReg4, node, valueXMM, ch, cg);
    Inst_RegMem(OP::PSHUFBRegMem, node, valueXMM, MRef_const(cg->findOrCreate16ByteConstant(node, shuffleMask), cg),
        cg);

    Inst_RegReg(OP::MOV4RegReg, node, result, offset, cg);

    Inst_Label(OP::label, node, begLabel, cg);
    Inst_RegMem(OP::LEARegMem(), node, scratch,
        MRef_BISdisp32(array, result, shift, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg), cg);
    Inst_RegReg(OP::MOVRegReg(), node, ECX, scratch, cg);
    Inst_RegImm(OP::ANDRegImms(), node, scratch, ~(width - 1), cg);
    Inst_RegImm(OP::ANDRegImms(), node, ECX, width - 1, cg);
    Inst_Label(OP::JE1, node, loopLabel, cg);

    Inst_RegMem(OP::MOVDQURegMem, node, scratchXMM, MRef_Bdisp32(scratch, 0, cg), cg);
    Inst_RegReg(compareOp, node, scratchXMM, valueXMM, cg);
    Inst_RegReg(OP::PMOVMSKB4RegReg, node, scratch, scratchXMM, cg);
    Inst_Reg(OP::SHR4RegCL, node, scratch, cg);
    Inst_RegReg(OP::TEST4RegReg, node, scratch, scratch, cg);
    Inst_Label(OP::JNE1, node, endLabel, cg);
    if (shift) {
        Inst_RegImm(OP::SHR4RegImm1, node, ECX, shift, cg);
    }
    Inst_RegImm(OP::ADD4RegImms, node, result, width >> shift, cg);
    Inst_RegReg(OP::SUB4RegReg, node, result, ECX, cg);
    Inst_RegReg(OP::CMP4RegReg, node, result, length, cg);
    Inst_Label(OP::JGE1, node, endLabel, cg);

    Inst_Label(OP::label, node, loopLabel, cg);
    Inst_RegMem(OP::MOVDQURegMem, node, scratchXMM,
        MRef_BISdisp32(array, result, shift, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg), cg);
    Inst_RegReg(compareOp, node, scratchXMM, valueXMM, cg);
    Inst_RegReg(OP::PMOVMSKB4RegReg, node, scratch, scratchXMM, cg);
    Inst_RegReg(OP::TEST4RegReg, node, scratch, scratch, cg);
    Inst_Label(OP::JNE1, node, endLabel, cg);
    Inst_RegImm(OP::ADD4RegImms, node, result, width >> shift, cg);
    Inst_RegReg(OP::CMP4RegReg, node, result, length, cg);
    Inst_Label(OP::JL1, node, loopLabel, cg);
    Inst_Label(OP::label, node, endLabel, dependencies, cg);

    Inst_RegReg(OP::BSF4RegReg, node, scratch, scratch, cg);
    if (shift) {
        Inst_RegImm(OP::SHR4RegImm1, node, scratch, shift, cg);
    }
    Inst_RegReg(OP::ADDRegReg(), node, result, scratch, cg);
    Inst_RegReg(OP::CMPRegReg(), node, result, length, cg);
    Inst_RegMem(OP::CMOVGERegMem(), node, result,
        MRef_const(cg->comp()->target().is32Bit() ? cg->findOrCreate4ByteConstant(node, -1)
                                                  : cg->findOrCreate8ByteConstant(node, -1),
            cg),
        cg);

    cg->stopUsingRegister(ECX);
    cg->stopUsingRegister(scratch);
    cg->stopUsingRegister(scratchXMM);
    cg->stopUsingRegister(valueXMM);

    node->setRegister(result);
    if (!isStaticCall) {
        cg->recursivelyDecReferenceCount(node->getChild(0));
    }
    for (int32_t i = firstCallArgIdx; i < node->getNumChildren(); i++) {
        cg->decReferenceCount(node->getChild(i));
    }
    return result;
}

/**
 * \brief
 *   Generate inlined instructions equivalent to java/lang/StringLatin1.indexOf([BI[BII)I or
 * java/lang/StringUTF16.indexOf([BI[BII)I
 *
 * \param node
 *   The tree node
 *
 * \param cg
 *   The Code Generator
 *
 * \param isLatin1
 *   True when the string is Latin1, False when the string is UTF16
 *
 * Note that this version does not support discontiguous arrays
 */
static TR::Register *inlineIntrinsicStringIndexOfString(TR::Node *node, TR::CodeGenerator *cg, bool isLatin1)
{
    static bool disableStrIdxOfStr = (feGetEnv("TR_disableStrIdxOfStr") != NULL);
    if (disableStrIdxOfStr)
        return NULL;

    static bool verboseStrIdxOfStr = (feGetEnv("TR_verboseStrIdxOfStr") != NULL);
    if (verboseStrIdxOfStr) {
        fprintf(stderr, "*%s.indexOfString: %s @%s\n", isLatin1 ? "Latin1" : "UTF16", cg->comp()->signature(),
            cg->comp()->getHotnessName());
    }

    TR_ASSERT_FATAL(cg->comp()->target().is64Bit(), "Not supported on 32-bit platform");

    // This evaluator function handles different indexOf() intrinsics, some of which are static calls without a
    // receiver. Hence, the need for static call check.
    const bool isStaticCall = node->getSymbolReference()->getSymbol()->castToMethodSymbol()->isStatic();
    const uint8_t firstCallArgIdx = isStaticCall ? 0 : 1;
    TR::Register *s1Reg = cg->evaluate(node->getChild(firstCallArgIdx));
    TR::Node *s1lenNode = node->getChild(firstCallArgIdx + 1);
    TR::Register *s1lenReg = cg->evaluate(s1lenNode);
    TR::Register *s2Reg = cg->evaluate(node->getChild(firstCallArgIdx + 2));
    TR::Register *s2lenReg = cg->evaluate(node->getChild(firstCallArgIdx + 3));
    TR::Node *offsetNode = node->getChild(firstCallArgIdx + 4);
    TR::Register *offsetReg = cg->evaluate(offsetNode);

    TR::Register *maxReg;
    if (s1lenNode->getReferenceCount() == 1) {
        maxReg = s1lenReg;
    } else {
        maxReg = cg->allocateRegister(TR_GPR);
        Inst_RegReg(OP::MOV4RegReg, node, maxReg, s1lenReg, cg);
    }

    TR::Register *resultReg;
    if (offsetNode->getReferenceCount() == 1) {
        resultReg = offsetReg;
    } else {
        resultReg = cg->allocateRegister(TR_GPR);
        Inst_RegReg(OP::MOV4RegReg, node, resultReg, offsetReg, cg);
    }

    static uint8_t MASKOFSIZEONE[] = {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
    };
    static uint8_t MASKOFSIZETWO[] = {
        0x00,
        0x01,
        0x00,
        0x01,
        0x00,
        0x01,
        0x00,
        0x01,
        0x00,
        0x01,
        0x00,
        0x01,
        0x00,
        0x01,
        0x00,
        0x01,
    };

    const uint8_t width = 16;
    uint8_t shift = 0;
    uint8_t *shuffleMask = NULL;
    OP::Mnemonic compareOp = OP::bad;
    if (isLatin1) {
        shuffleMask = MASKOFSIZEONE;
        compareOp = OP::PCMPEQBRegReg;
        shift = 0;
    } else {
        shuffleMask = MASKOFSIZETWO;
        compareOp = OP::PCMPEQWRegReg;
        shift = 1;
    }

    TR::Register *ECX = cg->allocateRegister(TR_GPR);
    TR::Register *tmpReg = cg->allocateRegister(TR_GPR);
    TR::Register *xmmReg1 = cg->allocateRegister(TR_VRF);
    TR::Register *xmmReg2 = cg->allocateRegister(TR_VRF);
    TR::Register *xmmReg3 = cg->allocateRegister(TR_VRF);
    TR::Register *s1addrReg = cg->allocateRegister(TR_GPR);
    TR::Register *s2idxReg = cg->allocateRegister(TR_GPR);

    TR::RegisterDependencyConditions *dependencies = RegDeps((uint8_t)12, (uint8_t)12, cg);
    dependencies->addPreCondition(s1Reg, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(s2Reg, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(s2lenReg, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(maxReg, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(resultReg, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(ECX, TR::RealRegister::ecx, cg);
    dependencies->addPreCondition(tmpReg, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(xmmReg1, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(xmmReg2, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(xmmReg3, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(s1addrReg, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(s2idxReg, TR::RealRegister::NoReg, cg);

    dependencies->addPostCondition(s1Reg, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(s2Reg, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(s2lenReg, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(maxReg, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(resultReg, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(ECX, TR::RealRegister::ecx, cg);
    dependencies->addPostCondition(tmpReg, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(xmmReg1, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(xmmReg2, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(xmmReg3, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(s1addrReg, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(s2idxReg, TR::RealRegister::NoReg, cg);

    TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *outerLoopLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *firstCharLoopLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *firstCharMatchedLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *qwordLoopLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *byteLoopLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *unmatchedLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *notFoundLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *foundLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);

    startLabel->setStartInternalControlFlow();
    doneLabel->setEndInternalControlFlow();

    Inst_Label(OP::label, node, startLabel, cg);

    int32_t hdrSize = static_cast<int32_t>(TR::Compiler->om.contiguousArrayHeaderSizeInBytes());

    // load first char of s2
    Inst_RegMem(isLatin1 ? OP::MOVZXReg4Mem1 : OP::MOVZXReg4Mem2, node, tmpReg, MRef_Bdisp32(s2Reg, hdrSize, cg), cg);
    Inst_RegReg(OP::MOVDRegReg4, node, xmmReg2, tmpReg, cg);
    Inst_RegMem(OP::PSHUFBRegMem, node, xmmReg2, MRef_const(cg->findOrCreate16ByteConstant(node, shuffleMask), cg), cg);

    // calculate max
    Inst_RegReg(OP::SUB4RegReg, node, maxReg, s2lenReg, cg); // s1len - s2len

    // outer loop
    Inst_Label(OP::label, node, outerLoopLabel, cg);
    Inst_RegReg(OP::CMP4RegReg, node, resultReg, maxReg, cg);
    Inst_Label(OP::JG4, node, notFoundLabel, cg);

    Inst_RegMem(OP::LEARegMem(), node, tmpReg, MRef_BISdisp32(s1Reg, resultReg, shift, hdrSize, cg), cg);
    Inst_RegReg(OP::MOV4RegReg, node, ECX, tmpReg, cg);
    Inst_RegImm(OP::AND4RegImms, node, ECX, width - 1, cg);
    Inst_Label(OP::JE1, node, firstCharLoopLabel, cg);

    Inst_RegImm(OP::ANDRegImms(), node, tmpReg, ~(width - 1), cg);
    Inst_RegMem(OP::MOVDQURegMem, node, xmmReg1, MRef_Bdisp32(tmpReg, 0, cg), cg);
    Inst_RegReg(compareOp, node, xmmReg1, xmmReg2, cg);
    Inst_RegReg(OP::PMOVMSKB4RegReg, node, tmpReg, xmmReg1, cg);
    Inst_Reg(OP::SHR4RegCL, node, tmpReg, cg);
    Inst_RegReg(OP::TEST4RegReg, node, tmpReg, tmpReg, cg);
    Inst_Label(OP::JNE1, node, firstCharMatchedLabel, cg);
    if (!isLatin1) {
        Inst_RegImm(OP::SHR4RegImm1, node, ECX, 1, cg);
    }
    Inst_RegImm(OP::ADD4RegImms, node, resultReg, width >> shift, cg);
    Inst_RegReg(OP::SUB4RegReg, node, resultReg, ECX, cg);
    Inst_RegReg(OP::CMP4RegReg, node, resultReg, maxReg, cg);
    Inst_Label(OP::JG4, node, notFoundLabel, cg);

    // loop for finding the first char
    Inst_Label(OP::label, node, firstCharLoopLabel, cg);
    Inst_RegMem(OP::MOVDQURegMem, node, xmmReg1, MRef_BISdisp32(s1Reg, resultReg, shift, hdrSize, cg), cg);
    Inst_RegReg(compareOp, node, xmmReg1, xmmReg2, cg);
    Inst_RegReg(OP::PMOVMSKB4RegReg, node, tmpReg, xmmReg1, cg);
    Inst_RegReg(OP::TEST4RegReg, node, tmpReg, tmpReg, cg);
    Inst_Label(OP::JNE1, node, firstCharMatchedLabel, cg);
    Inst_RegImm(OP::ADD4RegImms, node, resultReg, width >> shift, cg);
    Inst_RegReg(OP::CMP4RegReg, node, resultReg, maxReg, cg);
    Inst_Label(OP::JLE1, node, firstCharLoopLabel, cg);
    Inst_Label(OP::JMP4, node, notFoundLabel, cg);

    // first char matched
    Inst_Label(OP::label, node, firstCharMatchedLabel, cg);

    Inst_RegReg(OP::BSF4RegReg, node, tmpReg, tmpReg, cg);
    if (!isLatin1) {
        Inst_RegImm(OP::SHR4RegImm1, node, tmpReg, 1, cg);
    }
    Inst_RegReg(OP::ADD4RegReg, node, resultReg, tmpReg, cg);

    Inst_RegReg(OP::CMP4RegReg, node, resultReg, maxReg, cg);
    Inst_Label(OP::JG4, node, notFoundLabel, cg);

    Inst_RegMem(OP::LEARegMem(), node, s1addrReg, MRef_BISdisp32(s1Reg, resultReg, shift, hdrSize, cg),
        cg); // s1addr = &(s1[resultReg << shift])
    Inst_RegImm(OP::MOV4RegImm4, node, s2idxReg, 1, cg); // s2idx = 1

    Inst_RegMem(OP::LEARegMem(), node, ECX, MRef_Bdisp32(s2lenReg, -1, cg),
        cg); // ECX = s2len - 1: 1st char has already matched
    Inst_RegImm(OP::SHR4RegImm1, node, ECX, 4 - shift, cg); // div by 16 or 8
    Inst_Label(OP::JE1, node, byteLoopLabel, cg);

    // Compare by 16 bytes
    Inst_Label(OP::label, node, qwordLoopLabel, cg);
    Inst_RegMem(OP::MOVDQURegMem, node, xmmReg1, MRef_BISdisp32(s1addrReg, s2idxReg, shift, 0, cg), cg);
    Inst_RegMem(OP::MOVDQURegMem, node, xmmReg3, MRef_BISdisp32(s2Reg, s2idxReg, shift, hdrSize, cg), cg);
    Inst_RegReg(OP::PCMPEQBRegReg, node, xmmReg1, xmmReg3, cg);
    Inst_RegReg(OP::PMOVMSKB4RegReg, node, tmpReg, xmmReg1, cg);
    Inst_RegImm(OP::CMP4RegImm4, node, tmpReg, 0xffff, cg);
    Inst_Label(OP::JNE1, node, unmatchedLabel, cg);

    Inst_RegImm(OP::ADD4RegImms, node, s2idxReg, width >> shift, cg);
    Inst_RegImm(OP::SUB4RegImms, node, ECX, 1, cg);
    Inst_Label(OP::JG1, node, qwordLoopLabel, cg);

    // Compare each byte
    Inst_Label(OP::label, node, byteLoopLabel, cg);
    Inst_RegReg(OP::CMP4RegReg, node, s2lenReg, s2idxReg, cg);
    Inst_Label(OP::JLE1, node, doneLabel, cg); // resultReg has the result

    Inst_RegMem(isLatin1 ? OP::L1RegMem : OP::L2RegMem, node, tmpReg,
        MRef_BISdisp32(s2Reg, s2idxReg, shift, hdrSize, cg), cg);
    Inst_MemReg(isLatin1 ? OP::CMP1MemReg : OP::CMP2MemReg, node, MRef_BISdisp32(s1addrReg, s2idxReg, shift, 0, cg),
        tmpReg, cg);
    Inst_Label(OP::JNE1, node, unmatchedLabel, cg);

    Inst_RegImm(OP::ADD4RegImms, node, s2idxReg, 1, cg);
    Inst_Label(OP::JMP1, node, byteLoopLabel, cg);

    // substring did not match
    Inst_Label(OP::label, node, unmatchedLabel, cg);
    Inst_RegImm(OP::ADD4RegImms, node, resultReg, 1, cg);
    Inst_Label(OP::JMP4, node, outerLoopLabel, cg);

    // not found
    Inst_Label(OP::label, node, notFoundLabel, cg);
    Inst_RegImm(OP::OR4RegImms, node, resultReg, -1, cg);
    // fall through to doneLabel

    Inst_Label(OP::label, node, doneLabel, dependencies, cg);

    cg->stopUsingRegister(ECX);
    cg->stopUsingRegister(tmpReg);
    cg->stopUsingRegister(xmmReg1);
    cg->stopUsingRegister(xmmReg2);
    cg->stopUsingRegister(xmmReg3);
    cg->stopUsingRegister(s1addrReg);
    cg->stopUsingRegister(s2idxReg);

    if (maxReg != s1lenReg) {
        cg->stopUsingRegister(maxReg);
    }

    node->setRegister(resultReg);

    if (!isStaticCall) {
        cg->recursivelyDecReferenceCount(node->getChild(0));
    }
    for (int32_t i = firstCallArgIdx; i < node->getNumChildren(); i++) {
        cg->decReferenceCount(node->getChild(i));
    }

    return resultReg;
}

/**
 * \brief
 *   Generate inlined instructions equivalent to sun/misc/Unsafe.compareAndSwapObject or
 * jdk/internal/misc/Unsafe.compareAndSwapObject
 *
 * \param node
 *   The tree node
 *
 * \param cg
 *   The Code Generator
 *
 */
static TR::Register *inlineCompareAndSwapObjectNative(TR::Node *node, TR::CodeGenerator *cg, bool isExchange)
{
    TR::Compilation *comp = cg->comp();

    TR_ASSERT(!TR::Compiler->om.canGenerateArraylets() || node->isUnsafeGetPutCASCallOnNonArray(),
        "This evaluator does not support arraylets.");

    cg->recursivelyDecReferenceCount(node->getChild(0)); // The Unsafe
    TR::Node *objectNode = node->getChild(1);
    TR::Node *offsetNode = node->getChild(2);
    TR::Node *oldValueNode = node->getChild(3);
    TR::Node *newValueNode = node->getChild(4);

    TR::Register *object = cg->evaluate(objectNode);
    TR::Register *offset = cg->evaluate(offsetNode);
    TR::Register *oldValue = cg->evaluate(oldValueNode);
    TR::Register *newValue = cg->evaluate(newValueNode);
    TR::Register *result = isExchange ? NULL : cg->allocateRegister();
    TR::Register *EAX = cg->allocateRegister();
    TR::Register *tmp = cg->allocateRegister();

    bool use64BitClasses = comp->target().is64Bit() && !comp->useCompressedPointers();

    if (comp->target().is32Bit()) {
        // Assume that the offset is positive and not pathologically large (i.e., > 2^31).
        offset = offset->getLowOrder();
    }

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
    switch (TR::Compiler->om.readBarrierType()) {
        case gc_modron_readbar_none:
            break;
        case gc_modron_readbar_always:
            Inst_RegMem(OP::LEARegMem(), node, tmp, MRef_BIS(object, offset, 0, cg), cg);
            Inst_MemReg(OP::SMemReg(), node,
                MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, floatTemp1), cg), tmp, cg);
            Inst_HelperCall(node, TR_softwareReadBarrier, NULL, cg);
            break;
        case gc_modron_readbar_range_check: {
            Inst_RegMem(OP::LRegMem(use64BitClasses), node, tmp, MRef_BIS(object, offset, 0, cg), cg);

            TR::LabelSymbol *begLabel = generateLabelSymbol(cg);
            TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
            TR::LabelSymbol *rdbarLabel = generateLabelSymbol(cg);
            begLabel->setStartInternalControlFlow();
            endLabel->setEndInternalControlFlow();

            TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)1, 1, cg);
            deps->addPreCondition(tmp, TR::RealRegister::NoReg, cg);
            deps->addPostCondition(tmp, TR::RealRegister::NoReg, cg);

            Inst_Label(OP::label, node, begLabel, cg);

            Inst_RegMem(OP::CMPRegMem(use64BitClasses), node, tmp,
                MRef_Bdisp32(cg->getVMThreadRegister(), comp->fej9()->thisThreadGetEvacuateBaseAddressOffset(), cg),
                cg);
            Inst_Label(OP::JAE4, node, rdbarLabel, cg);

            {
                TR_OutlinedInstructionsGenerator og(rdbarLabel, node, cg);
                Inst_RegMem(OP::CMPRegMem(use64BitClasses), node, tmp,
                    MRef_Bdisp32(cg->getVMThreadRegister(), comp->fej9()->thisThreadGetEvacuateTopAddressOffset(), cg),
                    cg);
                Inst_Label(OP::JA4, node, endLabel, cg);
                Inst_RegMem(OP::LEARegMem(), node, tmp, MRef_BIS(object, offset, 0, cg), cg);
                Inst_MemReg(OP::SMemReg(), node,
                    MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, floatTemp1), cg), tmp, cg);
                Inst_HelperCall(node, TR_softwareReadBarrier, NULL, cg);
                Inst_Label(OP::JMP4, node, endLabel, cg);

                og.endOutlinedInstructionSequence();
            }

            Inst_Label(OP::label, node, endLabel, deps, cg);
        } break;
        default:
            TR_ASSERT(false, "Unsupported Read Barrier Type.");
            break;
    }
#endif

    Inst_RegReg(OP::MOVRegReg(), node, EAX, oldValue, cg);
    Inst_RegReg(OP::MOVRegReg(), node, tmp, newValue, cg);
    if (TR::Compiler->om.compressedReferenceShiftOffset() != 0) {
        if (!oldValueNode->isNull()) {
            Inst_RegImm(OP::SHRRegImm1(), node, EAX, TR::Compiler->om.compressedReferenceShiftOffset(), cg);
        }
        if (!newValueNode->isNull()) {
            Inst_RegImm(OP::SHRRegImm1(), node, tmp, TR::Compiler->om.compressedReferenceShiftOffset(), cg);
        }
    }

    TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)1, 1, cg);
    deps->addPreCondition(EAX, TR::RealRegister::eax, cg);
    deps->addPostCondition(EAX, TR::RealRegister::eax, cg);
    Inst_MemReg(use64BitClasses ? OP::LCMPXCHG8MemReg : OP::LCMPXCHG4MemReg, node, MRef_BIS(object, offset, 0, cg), tmp,
        deps, cg);

    if (isExchange) {
        result = EAX;
        result->setContainsCollectedReference();
        if (TR::Compiler->om.compressedReferenceShiftOffset() != 0) {
            Inst_RegImm(OP::SHLRegImm1(), node, EAX, TR::Compiler->om.compressedReferenceShiftOffset(), cg);
        }
    } else {
        Inst_Reg(OP::SETE1Reg, node, result, cg);
        Inst_RegReg(OP::MOVZXReg4Reg1, node, result, result, cg);
    }

    // Non-realtime: Generate a write barrier for this kind of object.
    //
    if (!comp->getOptions()->realTimeGC()) {
        // We could insert a runtime test for whether the write actually succeeded or not.
        // However, since in practice it will almost always succeed we do not want to
        // penalize general runtime performance especially if it is still correct to do
        // a write barrier even if the store never actually happened.
        TR_X86ScratchRegisterManager *scratchRegisterManager = cg->generateScratchRegisterManager();

        TR::TreeEvaluator::VMwrtbarWithoutStoreEvaluator(node, objectNode, newValueNode, NULL, scratchRegisterManager,
            cg);
    }

    cg->stopUsingRegister(tmp);
    if (!isExchange) {
        cg->stopUsingRegister(EAX);
    }
    node->setRegister(result);
    for (int32_t i = 1; i < node->getNumChildren(); i++) {
        cg->decReferenceCount(node->getChild(i));
    }
    return result;
}

/** Replaces a call to an Unsafe CAS method with inline instructions.
   @return true if the call was replaced, false if it was not.

   Note that this function must have behaviour consistent with the function
   willNotInlineCompareAndSwapNative in openj9/runtime/compiler/x/codegen/J9CodeGenerator.cpp
*/
static bool inlineCompareAndSwapNative(TR::Node *node, int8_t size, bool isObject, bool isExchange,
    TR::CodeGenerator *cg)
{
    TR::Node *firstChild = node->getFirstChild();
    TR::Node *objectChild = node->getSecondChild();
    TR::Node *offsetChild = node->getChild(2);
    TR::Node *oldValueChild = node->getChild(3);
    TR::Node *newValueChild = node->getChild(4);
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

    OP::Mnemonic op;

    if (TR::Compiler->om.canGenerateArraylets() && !node->isUnsafeGetPutCASCallOnNonArray())
        return false;

    // size = 4 --> CMPXCHG4
    // size = 8 --> if 64-bit -> CMPXCHG8
    //              else if proc supports CMPXCHG8B -> CMPXCHG8B
    //              else return false
    //
    // Do this early so we can return early without additional evaluations.
    //
    switch (size) {
        case 4:
            op = OP::LCMPXCHG4MemReg;
            break;
        case 8:
            if (comp->target().is64Bit()) {
                op = OP::LCMPXCHG8MemReg;
            } else if (comp->target().cpu.supportsFeature(OMR_FEATURE_X86_CX8)) {
                op = OP::LCMPXCHG8BMem;
            } else {
                return false;
            }
            break;
        default:
            TR_ASSERT_FATAL_WITH_NODE(node, false, "Unknown dataSize: %d\n", size);
            return false;
    }

    // In Java9 the sun.misc.Unsafe JNI methods have been moved to jdk.internal,
    // with a set of wrappers remaining in sun.misc to delegate to the new package.
    // We can be called in this function for the wrappers (which we will
    // not be converting to assembly), the new jdk.internal JNI methods or the
    // Java8 sun.misc JNI methods (both of which we will convert). We can
    // differentiate between these cases by testing with isNative() on the method.
    {
        TR::MethodSymbol *methodSymbol = node->getSymbol()->getMethodSymbol();
        if (methodSymbol && !methodSymbol->isNative())
            return false;
    }

    cg->recursivelyDecReferenceCount(firstChild);

    TR::Register *objectReg = cg->evaluate(objectChild);

    TR::Register *offsetReg = NULL;
    int32_t offset = 0;

    if (offsetChild->getOpCode().isLoadConst() && !offsetChild->getRegister()
        && IS_32BIT_SIGNED(offsetChild->getLongInt())) {
        offset = (int32_t)(offsetChild->getLongInt());
    } else {
        offsetReg = cg->evaluate(offsetChild);

        // Assume that the offset is positive and not pathologically large (i.e., > 2^31).
        //
        if (comp->target().is32Bit())
            offsetReg = offsetReg->getLowOrder();
    }

    TR::MemoryReference *mr;

    if (offsetReg)
        mr = MRef_BIS(objectReg, offsetReg, 0, cg);
    else
        mr = MRef_Bdisp32(objectReg, offset, cg);

    bool bumpedRefCount = false;
    TR::Node *translatedNode = newValueChild;
    if (comp->useCompressedPointers() && isObject && (newValueChild->getDataType() != TR::Address)) {
        bool useShiftedOffsets = (TR::Compiler->om.compressedReferenceShiftOffset() != 0);

        translatedNode = newValueChild;
        if (translatedNode->getOpCode().isConversion())
            translatedNode = translatedNode->getFirstChild();
        if (translatedNode->getOpCode().isRightShift()) // optional
            translatedNode = translatedNode->getFirstChild();

        translatedNode = newValueChild;
        if (useShiftedOffsets) {
            while ((translatedNode->getNumChildren() > 0) && (translatedNode->getOpCodeValue() != TR::a2l))
                translatedNode = translatedNode->getFirstChild();

            if (translatedNode->getOpCodeValue() == TR::a2l)
                translatedNode = translatedNode->getFirstChild();

            // this is required so that different registers are
            // allocated for the actual store and translated values
            bumpedRefCount = true;
            translatedNode->incReferenceCount();
        }
    }

    TR::Register *newValueRegister = cg->evaluate(newValueChild);

    TR::Register *oldValueRegister;
    switch (size) {
        case 4:
            oldValueRegister = cg->intClobberEvaluate(oldValueChild);
            break;
        case 8:
            oldValueRegister = cg->longClobberEvaluate(oldValueChild);
            break;
        default:
            TR_ASSERT_FATAL_WITH_NODE(node, false, "Unknown dataSize: %d\n", size);
            break;
    }
    bool killOldValueRegister = (oldValueChild->getReferenceCount() > 1) ? true : false;

    TR::RegisterDependencyConditions *deps;
    TR_X86ScratchRegisterManager *scratchRegisterManagerForRealTime = NULL;
    TR::Register *storeAddressRegForRealTime = NULL;

    if (comp->getOptions()->realTimeGC() && isObject) {
        scratchRegisterManagerForRealTime = cg->generateScratchRegisterManager();

        // If reference is unresolved, need to resolve it right here before the barrier starts
        // Otherwise, we could get stopped during the resolution and that could invalidate any tests we would have
        // performend
        //   beforehand
        // For simplicity, just evaluate the store address into storeAddressRegForRealTime right now
        storeAddressRegForRealTime = scratchRegisterManagerForRealTime->findOrCreateScratchRegister();
        Inst_RegMem(OP::LEARegMem(), node, storeAddressRegForRealTime, mr, cg);
        if (node->getSymbolReference()->isUnresolved()) {
            TR::TreeEvaluator::padUnresolvedDataReferences(node, *node->getSymbolReference(), cg);

            // storeMR was created against a (i)wrtbar node which is a store.  The unresolved data snippet that
            //  was created set the checkVolatility bit based on that node being a store.  Since the resolution
            //  is now going to occur on a LEA instruction, which does not require any memory fence and hence
            //  no volatility check, we need to clear that "store" ness of the unresolved data snippet
            TR::UnresolvedDataSnippet *snippet = mr->getUnresolvedDataSnippet();
            if (snippet)
                snippet->resetUnresolvedStore();
        }

        TR::TreeEvaluator::VMwrtbarRealTimeWithoutStoreEvaluator(node, mr, storeAddressRegForRealTime, objectChild,
            translatedNode, NULL, scratchRegisterManagerForRealTime, cg);
    }

    TR::MemoryReference *cmpxchgMR = mr;

    TR::Register *resultReg;
    if (op == OP::LCMPXCHG8BMem) {
        int numDeps = 4;
        if (storeAddressRegForRealTime != NULL) {
            numDeps++;
            cmpxchgMR = MRef_Bdisp32(storeAddressRegForRealTime, 0, cg);
        }

        if (scratchRegisterManagerForRealTime)
            numDeps += scratchRegisterManagerForRealTime->numAvailableRegisters();

        deps = RegDeps(numDeps, numDeps, cg);
        deps->addPreCondition(oldValueRegister->getLowOrder(), TR::RealRegister::eax, cg);
        deps->addPreCondition(oldValueRegister->getHighOrder(), TR::RealRegister::edx, cg);
        deps->addPreCondition(newValueRegister->getLowOrder(), TR::RealRegister::ebx, cg);
        deps->addPreCondition(newValueRegister->getHighOrder(), TR::RealRegister::ecx, cg);
        deps->addPostCondition(oldValueRegister->getLowOrder(), TR::RealRegister::eax, cg);
        deps->addPostCondition(oldValueRegister->getHighOrder(), TR::RealRegister::edx, cg);
        deps->addPostCondition(newValueRegister->getLowOrder(), TR::RealRegister::ebx, cg);
        deps->addPostCondition(newValueRegister->getHighOrder(), TR::RealRegister::ecx, cg);

        if (scratchRegisterManagerForRealTime)
            scratchRegisterManagerForRealTime->addScratchRegistersToDependencyList(deps);

        deps->stopAddingConditions();

        Inst_Mem(op, node, cmpxchgMR, deps, cg);
    } else {
        int numDeps = 1;
        if (storeAddressRegForRealTime != NULL) {
            numDeps++;
            cmpxchgMR = MRef_Bdisp32(storeAddressRegForRealTime, 0, cg);
        }

        if (scratchRegisterManagerForRealTime)
            numDeps += scratchRegisterManagerForRealTime->numAvailableRegisters();

        deps = RegDeps(numDeps, numDeps, cg);
        deps->addPreCondition(oldValueRegister, TR::RealRegister::eax, cg);
        deps->addPostCondition(oldValueRegister, TR::RealRegister::eax, cg);

        if (scratchRegisterManagerForRealTime)
            scratchRegisterManagerForRealTime->addScratchRegistersToDependencyList(deps);

        deps->stopAddingConditions();

        Inst_MemReg(op, node, cmpxchgMR, newValueRegister, deps, cg);
    }

    if (isExchange) {
        killOldValueRegister = false;
        resultReg = oldValueRegister;
        if (isObject) {
            resultReg->setContainsCollectedReference();
            if (TR::Compiler->om.compressedReferenceShiftOffset() != 0) {
                Inst_RegImm(OP::SHLRegImm1(), node, resultReg, TR::Compiler->om.compressedReferenceShiftOffset(), cg);
            }
        }
    }

    if (killOldValueRegister)
        cg->stopUsingRegister(oldValueRegister);

    if (storeAddressRegForRealTime)
        scratchRegisterManagerForRealTime->reclaimScratchRegister(storeAddressRegForRealTime);

    if (!isExchange) {
        resultReg = cg->allocateRegister();
        Inst_Reg(OP::SETE1Reg, node, resultReg, cg);
        Inst_RegReg(OP::MOVZXReg4Reg1, node, resultReg, resultReg, cg);
    }

    // Non-realtime: Generate a write barrier for this kind of object.
    //
    if (!comp->getOptions()->realTimeGC() && isObject) {
        // We could insert a runtime test for whether the write actually succeeded or not.
        // However, since in practice it will almost always succeed we do not want to
        // penalize general runtime performance especially if it is still correct to do
        // a write barrier even if the store never actually happened.
        //
        // A branch
        //
        TR_X86ScratchRegisterManager *scratchRegisterManager = cg->generateScratchRegisterManager();

        TR::TreeEvaluator::VMwrtbarWithoutStoreEvaluator(node, objectChild, translatedNode, NULL,
            scratchRegisterManager, cg);
    }

    node->setRegister(resultReg);

    cg->decReferenceCount(objectChild);
    if (offsetReg) {
        cg->decReferenceCount(offsetChild);
    } else {
        cg->recursivelyDecReferenceCount(offsetChild);
    }
    cg->decReferenceCount(oldValueChild);
    cg->decReferenceCount(newValueChild);
    if (bumpedRefCount)
        cg->decReferenceCount(translatedNode);

    return true;
}

/**
 * \brief
 *   Generate inlined instructions equivalent to java/lang/StringCoding.hasNegatives or
 * java/lang/StringCoding.countPositives
 *
 * \param node
 *   The tree node
 *
 * \param recognizedMethod
 *   The method being inlined, should be either hasNegatives or countPositives
 *
 * \param cg
 *   The Code Generator
 */
static TR::Register *inlineHasNegativesOrCountPositives(TR::Node *node, TR::RecognizedMethod recognizedMethod,
    TR::CodeGenerator *cg)
{
    // Arguments to hasNegatives or countPositives
    // Byte array
    TR::Register *bufReg = cg->longClobberEvaluate(node->getChild(0));
    // Offset (i.e. index to begin counting from)
    TR::Register *offsetReg = cg->evaluate(node->getChild(1));
    // Length of byte array
    TR::Register *lengthReg = cg->evaluate(node->getChild(2));

    // Store a boolean indicating whether we are generating code for hasNegatives or countPositives
    bool isHasNegatives = recognizedMethod == TR::java_lang_StringCoding_hasNegatives;

    // The offset of the start of array data
    int32_t offsetToDataElements = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();

    // A key part of the main loop of this algorithm is the pmovmskb instruction,
    // which extracts the sign bits from a source register and collects them into a destination register
    // We need to specify the encoding of this instruction so that the code generator knows we want the 16 byte version
    TR::InstOpCode pmovmskb = OP::PMOVMSKB4RegReg;
    OMR::X86::Encoding pmovmskbEncoding = pmovmskb.getSIMDEncoding(&cg->comp()->target().cpu, TR::VectorLength128);
    static bool disableSIMDHasNegativesCountPositives = feGetEnv("TR_disableSIMDHasNegativesCountPositives") != NULL;
    bool useVectorInstructions
        = (pmovmskbEncoding != OMR::X86::Encoding::Bad) && !disableSIMDHasNegativesCountPositives;

    TR::Register *loopLimitReg = cg->allocateRegister();
    TR::Register *limitReg = cg->allocateRegister();
    TR::Register *maskReg = cg->allocateRegister();
    TR::Register *indexReg = cg->allocateRegister();
    TR::Register *chunkReg = cg->allocateRegister();
    TR::Register *xmmChunkReg = NULL;
    if (useVectorInstructions) {
        xmmChunkReg = cg->allocateRegister(TR_VRF);
    }

    uint8_t numDependencies = useVectorInstructions ? 9 : 8;
    TR::RegisterDependencyConditions *dependencies = RegDeps(numDependencies, numDependencies, cg);
    dependencies->addPreCondition(bufReg, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(offsetReg, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(lengthReg, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(loopLimitReg, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(limitReg, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(maskReg, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(indexReg, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(chunkReg, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(bufReg, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(offsetReg, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(lengthReg, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(loopLimitReg, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(limitReg, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(maskReg, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(indexReg, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(chunkReg, TR::RealRegister::NoReg, cg);
    if (useVectorInstructions) {
        dependencies->addPreCondition(xmmChunkReg, TR::RealRegister::NoReg, cg);
        dependencies->addPostCondition(xmmChunkReg, TR::RealRegister::NoReg, cg);
    }
    dependencies->stopAddingConditions();

    // Labels
    TR::LabelSymbol *begLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *residualLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *threeOrMoreBytesLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *fiveOrMoreBytesLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *nineOrMoreBytesLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *residualTestLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *returnNoNegativesLabel = NULL;
    TR::LabelSymbol *returnHasNegativesLabel = NULL;
    TR::LabelSymbol *returnBooleanLabel = NULL;
    if (isHasNegatives) {
        returnBooleanLabel = generateLabelSymbol(cg);
    } else {
        returnNoNegativesLabel = generateLabelSymbol(cg);
        returnHasNegativesLabel = generateLabelSymbol(cg);
    }
    begLabel->setStartInternalControlFlow();
    endLabel->setEndInternalControlFlow();

    Inst_Label(OP::label, node, begLabel, cg);

    // If offheap is enabled, we will update bufReg to point to the actual start of the array data
    // and set offsetToDataElements to zero
#ifdef J9VM_GC_SPARSE_HEAP_ALLOCATION
    if (TR::Compiler->om.isOffHeapAllocationEnabled()) {
        Inst_RegMem(OP::L8RegMem, node, bufReg,
            MRef_Bdisp32(bufReg, cg->comp()->fej9()->getOffsetOfContiguousDataAddrField(), cg), cg);

        // We'll be loading first data element address from array header so no need for offset
        offsetToDataElements = 0;
    }
#endif /* J9VM_GC_SPARSE_HEAP_ALLOCATION */

    // index = offset
    Inst_RegReg(OP::MOV4RegReg, node, indexReg, offsetReg, cg);

    // limit = offset + length
    Inst_RegMem(OP::LEA4RegMem, node, limitReg, MRef_BISdisp32(offsetReg, lengthReg, 0, 0, cg), cg);

    // loopLimit = (length & -16) + offset
    Inst_RegReg(OP::MOV4RegReg, node, loopLimitReg, lengthReg, cg);
    Inst_RegImm(OP::AND4RegImm4, node, loopLimitReg, -16, cg);
    Inst_RegReg(OP::ADD4RegReg, node, loopLimitReg, offsetReg, cg);

    // If the 16 byte encoding of the pmovmskb instruction is not supported on this architecture,
    // Prepare an 8 byte sign bit mask so we can run an alternate version of the algorithm
    if (!useVectorInstructions) {
        Inst_RegImm64(OP::MOV8RegImm64, node, maskReg, 0x8080808080808080, cg);
    }

    Inst_Label(OP::label, node, loopLabel, cg);

    // if index >= loopLimit, jump to handling the residual bytes
    Inst_RegReg(OP::CMP4RegReg, node, indexReg, loopLimitReg, cg);
    Inst_Label(OP::JGE4, node, residualLabel, cg);

    // If the 16 byte version of pmovmskb is supported on this architecture,
    // we can proceed to generate code for the loop
    if (useVectorInstructions) {
        // Load 16 bytes from address [buf + index]
        Inst_RegMem(OP::MOVDQURegMem, node, xmmChunkReg, MRef_BISdisp32(bufReg, indexReg, 0, offsetToDataElements, cg),
            cg);

        // Extract bitmask of sign bits
        Inst_RegReg(OP::PMOVMSKB4RegReg, node, maskReg, xmmChunkReg, cg, pmovmskbEncoding);

        // Check if any negative values exist
        Inst_RegReg(OP::TEST2RegReg, node, maskReg, maskReg, cg);
    }
    // If the 16 byte version of pmovmskb is not supported,
    // run an alternate version of the loop with an 8 byte chunk instead of a 16 byte chunk
    else {
        // Load 8 bytes from address [buf + index]
        Inst_RegMem(OP::L8RegMem, node, chunkReg, MRef_BISdisp32(bufReg, indexReg, 0, offsetToDataElements, cg), cg);

        // Check if any negative values exist
        Inst_RegReg(OP::TEST8RegReg, node, chunkReg, maskReg, cg);
    }

    // If the result is nonzero, we found at least one negative byte
    Inst_Label(OP::JNE4, node, isHasNegatives ? returnBooleanLabel : returnHasNegativesLabel, cg);

    // increment index by the appropriate amount and jump back to the top of the loop
    Inst_RegImm(OP::ADD4RegImm4, node, indexReg, useVectorInstructions ? 16 : 8, cg);
    Inst_Label(OP::JMP4, node, loopLabel, cg);

    // Deal with the residual (last 15 or fewer) bytes
    Inst_Label(OP::label, node, residualLabel, cg);

    // Calculate bytes remaining: loopLimit = -(loopLimit - limit)
    Inst_RegReg(OP::SUB4RegReg, node, loopLimitReg, limitReg, cg);
    Inst_Reg(OP::NEG4Reg, node, loopLimitReg, cg);

    /*
     *    if loopLimit == 0
     *       return result
     *    if loopLimit > 8
     *       jmp nineOrMoreBytesLabel -------+
     *    if loopLimit > 2                   |
     *       jmp threeOrMoreBytesLabel ----+ |
     *                                     | |
     *    load 1-2 bytes                   | |
     *    jmp residualTestLabel            | |
     *                                     | |
     *    threeOrMoreBytesLabel: <---------+ |
     *       if loopLimit > 4                |
     *          jmp fiveOrMoreBytesLabel --+ |
     *                                     | |
     *       load 3-4 bytes                | |
     *       jmp residualTestLabel         | |
     *                                     | |
     *    fiveOrMoreBytesLabel: <----------+ |
     *       load 5-8 Bytes                  |
     *       jmp residualTestLabel           |
     *                                       |
     *    nineOrMoreBytesLabel: <------------+
     *       load 9-16 bytes
     *
     *    residualTestLabel:
     *       AND chunkReg with maskReg
     *
     *    return result
     */

    // if loopLimit = 0, we did not find any negative bytes
    Inst_RegReg(OP::TEST4RegReg, node, loopLimitReg, loopLimitReg, cg);
    Inst_Label(OP::JE4, node, isHasNegatives ? returnBooleanLabel : returnNoNegativesLabel, cg);

    // Prepare an 8 byte sign bit mask
    // (if the 16 byte pmovmskb instruction above isn't supported, we already did this at the start)
    if (useVectorInstructions) {
        Inst_RegImm64(OP::MOV8RegImm64, node, maskReg, 0x8080808080808080, cg);
    }

    // if loopLimit > 8, jump to nineOrMoreBytesLabel
    Inst_RegImm(OP::CMP4RegImm4, node, loopLimitReg, 8, cg);
    Inst_Label(OP::JG4, node, nineOrMoreBytesLabel, cg);

    // Zero out the chunk register
    Inst_RegReg(OP::XOR4RegReg, node, chunkReg, chunkReg, cg);

    // if loopLimit > 2, jump to threeOrMoreBytesLabel
    Inst_RegImm(OP::CMP4RegImm4, node, loopLimitReg, 2, cg);
    Inst_Label(OP::JG4, node, threeOrMoreBytesLabel, cg);

    // Case in which there are one or two residual bytes
    // Load the byte at address [buf + index] into the chunk register
    Inst_RegMem(OP::L1RegMem, node, chunkReg, MRef_BISdisp32(bufReg, indexReg, 0, offsetToDataElements, cg), cg);
    // OR the second byte (which is the same byte again in the 1 byte case)
    Inst_RegMem(OP::OR1RegMem, node, chunkReg, MRef_BISdisp32(bufReg, limitReg, 0, offsetToDataElements - 1, cg), cg);

    Inst_Label(OP::JMP4, node, residualTestLabel, cg);

    // Case in which there are three or more residual bytes
    Inst_Label(OP::label, node, threeOrMoreBytesLabel, cg);

    // if loopLimit > 4, jump to fiveOrMoreBytesLabel
    Inst_RegImm(OP::CMP4RegImm4, node, loopLimitReg, 4, cg);
    Inst_Label(OP::JG4, node, fiveOrMoreBytesLabel, cg);

    // Load the first two bytes at address [buf + index] into the chunk register
    Inst_RegMem(OP::L2RegMem, node, chunkReg, MRef_BISdisp32(bufReg, indexReg, 0, offsetToDataElements, cg), cg);
    // OR the second two bytes at address [buf + (limit - 2)] into the chunk register
    Inst_RegMem(OP::OR2RegMem, node, chunkReg, MRef_BISdisp32(bufReg, limitReg, 0, offsetToDataElements - 2, cg), cg);

    Inst_Label(OP::JMP4, node, residualTestLabel, cg);

    // Case in which there are five or more residual bytes
    Inst_Label(OP::label, node, fiveOrMoreBytesLabel, cg);

    // Load the first four bytes at address [buf + index] into the chunk register
    Inst_RegMem(OP::L4RegMem, node, chunkReg, MRef_BISdisp32(bufReg, indexReg, 0, offsetToDataElements, cg), cg);
    // OR the second four bytes at address [buf + (limit - 4)] into the chunk register
    Inst_RegMem(OP::OR4RegMem, node, chunkReg, MRef_BISdisp32(bufReg, limitReg, 0, offsetToDataElements - 4, cg), cg);

    Inst_Label(OP::JMP4, node, residualTestLabel, cg);

    // Case in which there are nine or more residual bytes
    Inst_Label(OP::label, node, nineOrMoreBytesLabel, cg);

    // Load the first eight bytes at address [buf + index] into the chunk register
    Inst_RegMem(OP::L8RegMem, node, chunkReg, MRef_BISdisp32(bufReg, indexReg, 0, offsetToDataElements, cg), cg);
    // OR the second eight bytes at address [buf + (limit - 8)] into the chunk register
    Inst_RegMem(OP::OR8RegMem, node, chunkReg, MRef_BISdisp32(bufReg, limitReg, 0, offsetToDataElements - 8, cg), cg);

    // Examine the chunk register now that all of the residual bytes have been ORed into it
    Inst_Label(OP::label, node, residualTestLabel, cg);
    // AND the residual bytes with the new mask
    Inst_RegReg(OP::TEST8RegReg, node, chunkReg, maskReg, cg);
    if (!isHasNegatives) {
        // If the result is nonzero (i.e. at least one of the sign bits is set), jump to returnHasNegativesLabel
        Inst_Label(OP::JNE4, node, returnHasNegativesLabel, cg);
    }

    // Return result
    if (isHasNegatives) {
        Inst_Label(OP::label, node, returnBooleanLabel, cg);
        // If the result of the previous comparison is nonzero, a negative byte has been found somewhere, so return true
        // Otherwise, return false
        Inst_Reg(OP::SETNE1Reg, node, indexReg, cg);
        Inst_RegReg(OP::MOVZXReg4Reg1, node, indexReg, indexReg, cg);
    } else {
        // no negatives found case, result = length
        Inst_Label(OP::label, node, returnNoNegativesLabel, cg);
        Inst_RegReg(OP::MOV4RegReg, node, indexReg, lengthReg, cg);
        Inst_Label(OP::JMP4, node, endLabel, cg);

        // negative(s) found case, result = index - offset
        Inst_Label(OP::label, node, returnHasNegativesLabel, cg);
        Inst_RegReg(OP::SUB4RegReg, node, indexReg, offsetReg, cg);
    }

    Inst_Label(OP::label, node, endLabel, dependencies, cg);

    cg->stopUsingRegister(bufReg);
    cg->stopUsingRegister(loopLimitReg);
    cg->stopUsingRegister(limitReg);
    cg->stopUsingRegister(maskReg);
    cg->stopUsingRegister(chunkReg);
    if (useVectorInstructions) {
        cg->stopUsingRegister(xmmChunkReg);
    }

    node->setRegister(indexReg);
    for (int32_t i = 0; i < node->getNumChildren(); i++) {
        cg->decReferenceCount(node->getChild(i));
    }
    return indexReg;
}

static TR::Register *inlineIntegerLongCompareUnsigned(TR::Node *node, bool isInt, TR::CodeGenerator *cg)
{
    TR::Register *aReg = cg->evaluate(node->getChild(0));
    TR::Register *bReg = cg->evaluate(node->getChild(1));
    TR::Register *resultReg = cg->allocateRegister();

    Inst_RegReg(OP::XOR4RegReg, node, resultReg, resultReg, cg);

    Inst_RegReg((isInt ? OP::CMP4RegReg : OP::CMP8RegReg), node, aReg, bReg, cg);

    // Set return register to 1 if x > y unsigned, otherwise remain 0
    Inst_Reg(OP::SETA1Reg, node, resultReg, cg);
    // Subtract CF from return register if x < y, otherwise remain 0
    Inst_RegImm(OP::SBB4RegImm4, node, resultReg, 0, cg);

    node->setRegister(resultReg);
    cg->decReferenceCount(node->getChild(0));
    cg->decReferenceCount(node->getChild(1));

    return resultReg;
}

// Generate inline code if possible for a call to an inline method. The call
// may be direct or indirect; if it is indirect a guard will be generated around
// the inline code and a fall-back to the indirect call.
// Returns true if the call was inlined, otherwise a regular call sequence must
// be issued by the caller of this method.
//
bool J9::X86::TreeEvaluator::VMinlineCallEvaluator(TR::Node *node, bool isIndirect, TR::CodeGenerator *cg)
{
    TR::MethodSymbol *methodSymbol = node->getSymbol()->castToMethodSymbol();
    TR::ResolvedMethodSymbol *resolvedMethodSymbol = node->getSymbol()->getResolvedMethodSymbol();

    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

    bool callWasInlined = false;
    TR::Compilation *comp = cg->comp();

    bool disableCASInlining = !cg->getSupportsInlineUnsafeCompareAndSet();
    bool disableCAEInlining = !cg->getSupportsInlineUnsafeCompareAndExchange();

    if (methodSymbol) {
        switch (methodSymbol->getRecognizedMethod()) {
            case TR::sun_nio_ch_NativeThread_current:
                // The spec says that on systems that do not require signaling
                // that this method should return -1. I'm not sure what do realtime
                // systems do here
                if (!comp->getOptions()->realTimeGC() && node->getNumChildren() > 0) {
                    TR::Register *nativeThreadReg = cg->allocateRegister();
                    TR::Register *nativeThreadRegHigh = NULL;
                    TR::Register *vmThreadReg = cg->getVMThreadRegister();
                    int32_t numDeps = 2;

                    if (comp->target().is32Bit()) {
                        nativeThreadRegHigh = cg->allocateRegister();
                        numDeps++;
                    }

                    TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, (uint8_t)numDeps, cg);
                    deps->addPostCondition(nativeThreadReg, TR::RealRegister::NoReg, cg);
                    if (comp->target().is32Bit()) {
                        deps->addPostCondition(nativeThreadRegHigh, TR::RealRegister::NoReg, cg);
                    }
                    deps->addPostCondition(vmThreadReg, TR::RealRegister::ebp, cg);

                    if (comp->target().is64Bit()) {
                        TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
                        TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
                        startLabel->setStartInternalControlFlow();
                        Inst_Label(OP::label, node, startLabel, cg);

                        Inst_RegMem(OP::LRegMem(), node, nativeThreadReg,
                            MRef_Bdisp32(vmThreadReg, fej9->thisThreadOSThreadOffset(), cg), cg);
                        Inst_RegMem(OP::LRegMem(), node, nativeThreadReg,
                            MRef_Bdisp32(nativeThreadReg, offsetof(J9Thread, handle), cg), cg);
                        doneLabel->setEndInternalControlFlow();
                        Inst_Label(OP::label, node, doneLabel, deps, cg);
                    } else {
                        TR::MemoryReference *lowMR = MRef_Bdisp32(vmThreadReg, fej9->thisThreadOSThreadOffset(), cg);
                        TR::MemoryReference *highMR = MRef_MRefOff(*lowMR, 4, cg);

                        TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
                        TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
                        startLabel->setStartInternalControlFlow();
                        Inst_Label(OP::label, node, startLabel, cg);

                        Inst_RegMem(OP::L4RegMem, node, nativeThreadReg, lowMR, cg);
                        Inst_RegMem(OP::L4RegMem, node, nativeThreadRegHigh, highMR, cg);

                        TR::MemoryReference *lowHandleMR
                            = MRef_Bdisp32(nativeThreadReg, offsetof(J9Thread, handle), cg);
                        TR::MemoryReference *highHandleMR = MRef_MRefOff(*lowMR, 4, cg);

                        Inst_RegMem(OP::L4RegMem, node, nativeThreadReg, lowHandleMR, cg);
                        Inst_RegMem(OP::L4RegMem, node, nativeThreadRegHigh, highHandleMR, cg);

                        doneLabel->setEndInternalControlFlow();
                        Inst_Label(OP::label, node, doneLabel, deps, cg);
                    }

                    if (comp->target().is32Bit()) {
                        TR::RegisterPair *longRegister = cg->allocateRegisterPair(nativeThreadReg, nativeThreadRegHigh);
                        node->setRegister(longRegister);
                    } else {
                        node->setRegister(nativeThreadReg);
                    }
                    cg->recursivelyDecReferenceCount(node->getFirstChild());
                    return true;
                }
                return false; // Call the native version of NativeThread.current()
            case TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z: {
                if (!disableCASInlining && node->isSafeForCGToFastPathUnsafeCall())
                    return inlineCompareAndSwapNative(node, 4, false, false, cg);
            } break;
            case TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z: {
                if (!disableCASInlining && node->isSafeForCGToFastPathUnsafeCall())
                    return inlineCompareAndSwapNative(node, 8, false, false, cg);
            } break;
            case TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z: {
                static bool useOldCompareAndSwapObject = (bool)feGetEnv("TR_UseOldCompareAndSwapObject");
                if (!disableCASInlining && node->isSafeForCGToFastPathUnsafeCall()) {
                    if (useOldCompareAndSwapObject)
                        return inlineCompareAndSwapNative(node, TR::Compiler->om.sizeofReferenceField(), true, false,
                            cg);
                    else {
                        inlineCompareAndSwapObjectNative(node, cg, false);
                        return true;
                    }
                }
            } break;
            case TR::jdk_internal_misc_Unsafe_compareAndExchangeInt: {
                if (!disableCAEInlining && node->isSafeForCGToFastPathUnsafeCall())
                    return inlineCompareAndSwapNative(node, 4, false, true, cg);
            } break;
            case TR::jdk_internal_misc_Unsafe_compareAndExchangeLong: {
                if (!disableCAEInlining && node->isSafeForCGToFastPathUnsafeCall())
                    return inlineCompareAndSwapNative(node, 8, false, true, cg);
            } break;
            case TR::jdk_internal_misc_Unsafe_compareAndExchangeObject:
            case TR::jdk_internal_misc_Unsafe_compareAndExchangeReference: {
                static bool useOldCompareAndSwapObject = (bool)feGetEnv("TR_UseOldCompareAndSwapObject");
                if (!disableCAEInlining && node->isSafeForCGToFastPathUnsafeCall()) {
                    if (useOldCompareAndSwapObject)
                        return inlineCompareAndSwapNative(node, TR::Compiler->om.sizeofReferenceField(), true, true,
                            cg);
                    else {
                        inlineCompareAndSwapObjectNative(node, cg, true);
                        return true;
                    }
                }
            } break;

            case TR::java_lang_Object_clone: {
                return (objectCloneEvaluator(node, cg) != NULL);
                break;
            }
            default:
                break;
        }
    }

    if (!resolvedMethodSymbol)
        return false;

    if (resolvedMethodSymbol) {
        switch (resolvedMethodSymbol->getRecognizedMethod()) {
#ifdef LINUX
            case TR::java_lang_System_nanoTime: {
                TR_ASSERT(!isIndirect, "Indirect call to nanoTime");
                callWasInlined = inlineNanoTime(node, cg);
                break;
            }
#endif
            default:
                break;
        }
    }

    return callWasInlined;
}

/**
 * \brief
 *   Generate instructions to conditionally branch to a write barrier helper call
 *
 * \oaram branchOp
 *   The branch instruction to jump to the write barrier helper call
 *
 * \param node
 *   The write barrier node
 *
 * \param gcMode
 *   The GC Mode
 *
 * \param owningObjectReg
 *   The register holding the owning object
 *
 * \param sourceReg
 *   The register holding the source object
 *
 * \param doneLabel
 *   The label to jump to when returning from the write barrier helper
 *
 * \param cg
 *   The Code Generator
 *
 * Note that RealTimeGC is handled separately in a different method.
 */
static void generateWriteBarrierCall(OP::Mnemonic branchOp, TR::Node *node, MM_GCWriteBarrierType gcMode,
    TR::Register *owningObjectReg, TR::Register *sourceReg, TR::LabelSymbol *doneLabel, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR_ASSERT(gcMode != gc_modron_wrtbar_satb && !comp->getOptions()->realTimeGC(),
        "This helper is not for RealTimeGC.");

    uint8_t helperArgCount = 0; // Number of arguments passed on the runtime helper.
    TR::SymbolReference *wrtBarSymRef = NULL;

    if (node->getOpCodeValue() == TR::arraycopy) {
        wrtBarSymRef = comp->getSymRefTab()->findOrCreateWriteBarrierBatchStoreSymbolRef();
        helperArgCount = 1;
    } else if (gcMode == gc_modron_wrtbar_cardmark_and_oldcheck) {
        wrtBarSymRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreGenerationalAndConcurrentMarkSymbolRef();
        helperArgCount = 2;
    } else if (gcMode == gc_modron_wrtbar_always) {
        wrtBarSymRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef();
        helperArgCount = 2;
    } else if (comp->generateArraylets()) {
        wrtBarSymRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef();
        helperArgCount = 2;
    } else {
        // Default case is a generational barrier (non-concurrent).
        //
        wrtBarSymRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreGenerationalSymbolRef();
        helperArgCount = 2;
    }

    TR::LabelSymbol *wrtBarLabel = generateLabelSymbol(cg);

    Inst_Label(branchOp, node, wrtBarLabel, cg);

    TR_OutlinedInstructionsGenerator og(wrtBarLabel, node, cg);

    Inst_MemReg(OP::SMemReg(), node, MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, floatTemp1), cg),
        owningObjectReg, cg);
    if (helperArgCount > 1) {
        Inst_MemReg(OP::SMemReg(), node, MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, floatTemp2), cg),
            sourceReg, cg);
    }
    Inst_ImmSym(OP::CALLImm4, node, (uintptr_t)wrtBarSymRef->getMethodAddress(), wrtBarSymRef, cg);
    Inst_Label(OP::JMP4, node, doneLabel, cg);

    og.endOutlinedInstructionSequence();
}

static void reportFlag(bool value, char *name, TR::CodeGenerator *cg)
{
    logprintf(value, cg->comp()->log(), " %s", name);
}

static int32_t byteOffsetForMask(int32_t mask, TR::CodeGenerator *cg)
{
    int32_t result;
    for (result = 3; result >= 0; --result) {
        int32_t shift = 8 * result;
        if (((mask >> shift) << shift) == mask)
            break;
    }

    if (result != -1
        && performTransformation(cg->comp(), "O^O TREE EVALUATION: Use 1-byte TEST with offset %d for mask %08x\n",
            result, mask))
        return result;

    return -1;
}

#define REPORT_FLAG(name) reportFlag((name), #name, cg)

void J9::X86::TreeEvaluator::VMwrtbarRealTimeWithoutStoreEvaluator(TR::Node *node,
    TR::MemoryReference *storeMRForRealTime, // RTJ only
    TR::Register *storeAddressRegForRealTime, // RTJ only
    TR::Node *destOwningObject, // only NULL for ME, always evaluated except for AC (evaluated below)
    TR::Node *sourceObject, // NULL for ME and AC(Array Copy?)
    TR::Register *srcReg, // should only be provided when sourceObject == NULL  (ME Multimidlet)
    TR_X86ScratchRegisterManager *srm, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
    TR_ASSERT(comp->getOptions()->realTimeGC(), "Call the non real-time barrier");
    auto gcMode = TR::Compiler->om.writeBarrierType();

    if (node->getOpCode().isWrtBar() && node->skipWrtBar())
        gcMode = gc_modron_wrtbar_none;
    else if ((node->getOpCodeValue() == TR::ArrayStoreCHK) && node->getFirstChild()->getOpCode().isWrtBar()
        && node->getFirstChild()->skipWrtBar())
        gcMode = gc_modron_wrtbar_none;

    // PR98283: it is not acceptable to emit a label symbol twice so always generate a new label here
    // we can clean up the API later in a less risky manner
    TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);

    // srcReg could only be NULL at this point for arraycopy
    if (sourceObject) {
        TR_ASSERT(!srcReg, "assertion failure");
        srcReg = sourceObject->getRegister();
        TR_ASSERT(srcReg, "assertion failure");
    } //

    TR::Node *wrtbarNode;
    switch (node->getOpCodeValue()) {
        case TR::ArrayStoreCHK:
            wrtbarNode = node->getFirstChild();
            break;
        case TR::arraycopy:
            wrtbarNode = NULL;
            break;
        case TR::awrtbari:
        case TR::awrtbar:
            wrtbarNode = node;
            break;
        default:
            wrtbarNode = NULL;
            break;
    }

    bool doInternalControlFlow;

    if (node->getOpCodeValue() == TR::ArrayStoreCHK) {
        // TR::ArrayStoreCHK will create its own internal control flow.
        //
        doInternalControlFlow = false;
    } else {
        doInternalControlFlow = true;
    }

    if (comp->getOption(TR_TraceCG)) {
        OMR::Logger *log = comp->log();
        log->prints(" | Real Time Write barrier info:\n");
        log->printf(" |   GC mode = %d:%s\n", gcMode, cg->getDebug()->getWriteBarrierKindName(gcMode));
        log->printf(" |   Node = %s %s  sourceObject = %s\n", cg->getDebug()->getName(node->getOpCodeValue()),
            cg->getDebug()->getName(node), sourceObject ? cg->getDebug()->getName(sourceObject) : "(none)");
        log->prints(" |   Action flags:");
        REPORT_FLAG(doInternalControlFlow);
        log->println();
    }

    //
    // Phase 2: Generate the appropriate code.
    //
    TR::Register *owningObjectReg;
    TR::Register *tempReg = NULL;

    owningObjectReg = cg->evaluate(destOwningObject);

    if (doInternalControlFlow) {
        TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
        startLabel->setStartInternalControlFlow();
        Inst_Label(OP::label, node, startLabel, cg);
        doneLabel->setEndInternalControlFlow();
    }

    if (comp->getOption(TR_BreakOnWriteBarrier)) {
        Inst(OP::INT3, node, cg);
    }

    TR::SymbolReference *wrtBarSymRef = NULL;
    if (wrtbarNode && (wrtbarNode->getOpCodeValue() == TR::awrtbar || wrtbarNode->isUnsafeStaticWrtBar()))
        wrtBarSymRef = comp->getSymRefTab()->findOrCreateWriteBarrierClassStoreRealTimeGCSymbolRef();
    else
        wrtBarSymRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreRealTimeGCSymbolRef();

    TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);

    // TR IL doesn't have a way to express the address of a field in an object, so we need some sneakiness here:
    //   1) create a dummy node for this argument to the call
    //   2) explicitly set that node's register to storeAddressRegForRealTime, preventing it from being evaluated
    //      (will just push storeAddressRegForRealTime for the call)
    //
    TR::Node *dummyDestAddressNode = TR::Node::create(node, TR::aconst, 0, 0);
    dummyDestAddressNode->setRegister(storeAddressRegForRealTime);
    TR::Node *callNode = TR::Node::createWithSymRef(TR::call, 3, 3, sourceObject, dummyDestAddressNode,
        destOwningObject, wrtBarSymRef);

    if (comp->getOption(TR_DisableInlineWriteBarriersRT)) {
        cg->evaluate(callNode);
    } else {
        TR_OutlinedInstructions *outlinedHelperCall
            = new (cg->trHeapMemory()) TR_OutlinedInstructions(callNode, TR::call, NULL, snippetLabel, doneLabel, cg);

        // have to disassemble the call node we just created, first have to give it a ref count 1
        callNode->setReferenceCount(1);
        cg->recursivelyDecReferenceCount(callNode);

        cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);
        cg->generateDebugCounter(outlinedHelperCall->getFirstInstruction(),
            TR::DebugCounter::debugCounterName(comp, "helperCalls/%s/(%s)/%d/%d", node->getOpCode().getName(),
                comp->signature(), node->getByteCodeInfo().getCallerIndex(),
                node->getByteCodeInfo().getByteCodeIndex()),
            1, TR::DebugCounter::Cheap);

        if (comp->getOption(TR_CountWriteBarriersRT)) {
            TR::MemoryReference *barrierCountMR
                = MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, debugEventData6), cg);
            Inst_Mem(OP::INCMem(comp->target().is64Bit()), node, barrierCountMR, cg);
        }

        tempReg = srm->findOrCreateScratchRegister();

        // if barrier not enabled, nothing to do
        TR::MemoryReference *fragmentParentMR = MRef_Bdisp32(cg->getVMThreadRegister(),
            fej9->thisThreadRememberedSetFragmentOffset() + fej9->getFragmentParentOffset(), cg);
        Inst_RegMem(OP::LRegMem(comp->target().is64Bit()), node, tempReg, fragmentParentMR, cg);
        TR::MemoryReference *globalFragmentIDMR
            = MRef_Bdisp32(tempReg, fej9->getRememberedSetGlobalFragmentOffset(), cg);
        Inst_MemImm(OP::CMPMemImms(), node, globalFragmentIDMR, 0, cg);
        Inst_Label(OP::JE4, node, doneLabel, cg);

        // now check if double barrier is enabled and definitely execute the barrier if it is
        // if (vmThread->localFragmentIndex == 0) goto snippetLabel
        TR::MemoryReference *localFragmentIndexMR = MRef_Bdisp32(cg->getVMThreadRegister(),
            fej9->thisThreadRememberedSetFragmentOffset() + fej9->getLocalFragmentOffset(), cg);
        Inst_MemImm(OP::CMPMemImms(), node, localFragmentIndexMR, 0, cg);
        Inst_Label(OP::JE4, node, snippetLabel, cg);

        // null test on the reference we're about to store over: if it is null goto doneLabel
        // if (destObject->field == null) goto doneLabel
        TR::MemoryReference *nullTestMR = MRef_Bdisp32(storeAddressRegForRealTime, 0, cg);
        if (comp->target().is64Bit() && comp->useCompressedPointers())
            Inst_MemImm(OP::CMP4MemImms, node, nullTestMR, 0, cg);
        else
            Inst_MemImm(OP::CMPMemImms(), node, nullTestMR, 0, cg);
        Inst_Label(OP::JNE4, node, snippetLabel, cg);

        // fall-through means write barrier not needed, just do the store
    }

    if (doInternalControlFlow) {
        int32_t numPostConditions = 2 + srm->numAvailableRegisters();

        numPostConditions += 4;

        if (srcReg) {
            numPostConditions++;
        }

        TR::RegisterDependencyConditions *conditions = RegDeps((uint8_t)0, numPostConditions, cg);

        conditions->addPostCondition(owningObjectReg, TR::RealRegister::NoReg, cg);
        if (srcReg) {
            conditions->addPostCondition(srcReg, TR::RealRegister::NoReg, cg);
        }

        conditions->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);

        if (!comp->getOption(TR_DisableInlineWriteBarriersRT)) {
            TR_ASSERT(storeAddressRegForRealTime != NULL, "assertion failure");
            conditions->addPostCondition(storeAddressRegForRealTime, TR::RealRegister::NoReg, cg);

            TR_ASSERT(tempReg != NULL, "assertion failure");
            conditions->addPostCondition(tempReg, TR::RealRegister::NoReg, cg);
        }

        if (destOwningObject->getOpCode().hasSymbolReference() && destOwningObject->getSymbol()
            && !destOwningObject->getSymbol()->isLocalObject()) {
            if (storeMRForRealTime->getBaseRegister()) {
                conditions->unionPostCondition(storeMRForRealTime->getBaseRegister(), TR::RealRegister::NoReg, cg);
            }
            if (storeMRForRealTime->getIndexRegister()) {
                conditions->unionPostCondition(storeMRForRealTime->getIndexRegister(), TR::RealRegister::NoReg, cg);
            }
        }

        srm->addScratchRegistersToDependencyList(conditions);
        conditions->stopAddingConditions();

        Inst_Label(OP::label, node, doneLabel, conditions, cg);

        srm->stopUsingRegisters();
    } else {
        TR_ASSERT(node->getOpCodeValue() == TR::ArrayStoreCHK, "assertion failure");
        Inst_Label(OP::label, node, doneLabel, cg);
    }
}

void J9::X86::TreeEvaluator::VMwrtbarWithoutStoreEvaluator(TR::Node *node,
    TR::Node *destOwningObject, // only NULL for ME, always evaluated except for AC (evaluated below)
    TR::Node *sourceObject, // NULL for ME and AC(Array Copy?)
    TR::Register *srcReg, // should only be provided when sourceObject == NULL  (ME Multimidlet)
    TR_X86ScratchRegisterManager *srm, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
    TR_ASSERT(!(comp->getOptions()->realTimeGC()), "Call the real-time barrier");
    auto gcMode = TR::Compiler->om.writeBarrierType();

    if (node->getOpCode().isWrtBar() && node->skipWrtBar())
        gcMode = gc_modron_wrtbar_none;
    else if ((node->getOpCodeValue() == TR::ArrayStoreCHK) && node->getFirstChild()->getOpCode().isWrtBar()
        && node->getFirstChild()->skipWrtBar())
        gcMode = gc_modron_wrtbar_none;

    // PR98283: it is not acceptable to emit a label symbol twice so always generate a new label here
    // we can clean up the API later in a less risky manner
    TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);

    TR::LabelSymbol *cardMarkDoneLabel = NULL;

    bool isSourceNonNull;

    // If a source node is provided, derive the source object register from it.
    // The source node must be evaluated before this function is called so it must
    // always be in a register.
    //
    if (sourceObject) {
        TR_ASSERT(!srcReg, "assertion failure");
        srcReg = sourceObject->getRegister();
        TR_ASSERT(srcReg, "assertion failure");
        isSourceNonNull = sourceObject->isNonNull();
    } else {
        isSourceNonNull = false;
    }

    // srcReg could only be NULL at this point for arraycopy

    //
    // Phase 1: Decide what parts of this logic we need to do
    //

    TR::Node *wrtbarNode;
    switch (node->getOpCodeValue()) {
        case TR::ArrayStoreCHK:
            wrtbarNode = node->getFirstChild();
            break;
        case TR::arraycopy:
            wrtbarNode = NULL;
            break;
        case TR::awrtbari:
        case TR::awrtbar:
            wrtbarNode = node;
            break;
        default:
            wrtbarNode = NULL;
            break;
    }

    bool doInlineCardMarkingWithoutOldSpaceCheck, doIsDestAHeapObjectCheck;

    if (wrtbarNode) {
        TR_ASSERT(wrtbarNode->getOpCode().isWrtBar(), "Expected node " POINTER_PRINTF_FORMAT " to be a WrtBar",
            wrtbarNode);
        // Note: for gc_modron_wrtbar_cardmark_and_oldcheck we let the helper do the card mark (ie. we don't inline it)
        doInlineCardMarkingWithoutOldSpaceCheck
            = (gcMode == gc_modron_wrtbar_cardmark || gcMode == gc_modron_wrtbar_cardmark_incremental)
            && !wrtbarNode->getSymbol()->isLocalObject() && !wrtbarNode->isNonHeapObjectWrtBar();

        doIsDestAHeapObjectCheck = doInlineCardMarkingWithoutOldSpaceCheck && !wrtbarNode->isHeapObjectWrtBar();
    } else {
        // TR::arraycopy or TR::ArrayStoreCHK
        //
        // Old space checks will be done out-of-line, and if a card mark policy requires an old space check
        // as well then both will be done out-of-line.
        //
        doInlineCardMarkingWithoutOldSpaceCheck = doIsDestAHeapObjectCheck
            = (gcMode == gc_modron_wrtbar_cardmark || gcMode == gc_modron_wrtbar_cardmark_incremental);
    }

    // for Tarok  gc_modron_wrtbar_cardmark
    //
    //   doIsDestAHeapObjectCheck = true (if req)   OK
    //   doIsDestInOldSpaceCheck = false            OK
    //   doInlineCardMarkingWithoutOldSpaceCheck = maybe  OK
    //   doCheckConcurrentMarkActive = false        OK
    //   dirtyCardTableOutOfLine = false            OK

    bool doIsDestInOldSpaceCheck = gcMode == gc_modron_wrtbar_oldcheck
        || gcMode == gc_modron_wrtbar_cardmark_and_oldcheck || gcMode == gc_modron_wrtbar_always;

    bool unsafeCallBarrier = false;
    if (doIsDestInOldSpaceCheck
        && (gcMode == gc_modron_wrtbar_cardmark || gcMode == gc_modron_wrtbar_cardmark_and_oldcheck
            || gcMode == gc_modron_wrtbar_cardmark_incremental)
        && node->getOpCode().isCall()) {
        TR::MethodSymbol *symbol = node->getSymbol()->castToMethodSymbol();
        if (symbol
            && (symbol->getRecognizedMethod() == TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z
                || symbol->getRecognizedMethod() == TR::jdk_internal_misc_Unsafe_compareAndExchangeObject
                || symbol->getRecognizedMethod() == TR::jdk_internal_misc_Unsafe_compareAndExchangeReference)) {
            unsafeCallBarrier = true;
        }
    }

    bool doCheckConcurrentMarkActive
        = (gcMode == gc_modron_wrtbar_cardmark || gcMode == gc_modron_wrtbar_cardmark_and_oldcheck
              || gcMode == gc_modron_wrtbar_cardmark_incremental)
        && (doInlineCardMarkingWithoutOldSpaceCheck || (doIsDestInOldSpaceCheck && wrtbarNode) || unsafeCallBarrier);

    // Use out-of-line instructions to dirty the card table.
    //
    bool dirtyCardTableOutOfLine = true;

    if (gcMode == gc_modron_wrtbar_cardmark_incremental) {
        // Override these settings for policies that don't support concurrent mark.
        //
        doCheckConcurrentMarkActive = false;
        dirtyCardTableOutOfLine = false;
    }

    // For practical applications, adding an explicit test for NULL is not worth the pathlength cost
    // especially since storing null values is not the dominant case.
    //
    static char *doNullCheckOnWrtBar = feGetEnv("TR_doNullCheckOnWrtBar");
    bool doSrcIsNullCheck = (doNullCheckOnWrtBar && doIsDestInOldSpaceCheck && srcReg && !isSourceNonNull);

    bool doInternalControlFlow;

    if (node->getOpCodeValue() == TR::ArrayStoreCHK) {
        // TR::ArrayStoreCHK will create its own internal control flow.
        //
        doInternalControlFlow = false;
    } else {
        doInternalControlFlow
            = (doIsDestInOldSpaceCheck || doIsDestAHeapObjectCheck || doCheckConcurrentMarkActive || doSrcIsNullCheck);
    }

    if (comp->getOption(TR_TraceCG)) {
        OMR::Logger *log = comp->log();
        log->prints(" | Write barrier info:\n");
        log->printf(" |   GC mode = %d:%s\n", gcMode, cg->getDebug()->getWriteBarrierKindName(gcMode));
        log->printf(" |   Node = %s %s  sourceObject = %s\n", cg->getDebug()->getName(node->getOpCodeValue()),
            cg->getDebug()->getName(node), sourceObject ? cg->getDebug()->getName(sourceObject) : "(none)");
        log->prints(" |   Action flags:");
        REPORT_FLAG(doInternalControlFlow);
        REPORT_FLAG(doCheckConcurrentMarkActive);
        REPORT_FLAG(doInlineCardMarkingWithoutOldSpaceCheck);
        REPORT_FLAG(dirtyCardTableOutOfLine);
        REPORT_FLAG(doIsDestAHeapObjectCheck);
        REPORT_FLAG(doIsDestInOldSpaceCheck);
        REPORT_FLAG(isSourceNonNull);
        REPORT_FLAG(doSrcIsNullCheck);
        log->println();
    }

    //
    // Phase 2: Generate the appropriate code.
    //
    TR::Register *owningObjectReg;
    TR::Register *tempReg = NULL;

#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
    bool stopUsingCopyBaseReg;
    if (gcMode == gc_modron_wrtbar_cardmark_incremental && TR::Compiler->om.isOffHeapAllocationEnabled()
        && destOwningObject->isDataAddrPointer())
        owningObjectReg = cg->evaluate(destOwningObject->getFirstChild());
    else
#endif /* defined(J9VM_GC_SPARSE_HEAP_ALLOCATION) */
        owningObjectReg = cg->evaluate(destOwningObject);

    if (doInternalControlFlow) {
        TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
        startLabel->setStartInternalControlFlow();
        Inst_Label(OP::label, node, startLabel, cg);
        doneLabel->setEndInternalControlFlow();
    }

    if (comp->getOption(TR_BreakOnWriteBarrier)) {
        Inst(OP::INT3, node, cg);
    }

    TR::MemoryReference *fragmentParentMR = MRef_Bdisp32(cg->getVMThreadRegister(),
        fej9->thisThreadRememberedSetFragmentOffset() + fej9->getFragmentParentOffset(), cg);
    TR::MemoryReference *localFragmentIndexMR = MRef_Bdisp32(cg->getVMThreadRegister(),
        fej9->thisThreadRememberedSetFragmentOffset() + fej9->getLocalFragmentOffset(), cg);
    TR_OutlinedInstructions *inlineCardMarkPath = NULL;
    if (doInlineCardMarkingWithoutOldSpaceCheck && doCheckConcurrentMarkActive) {
        TR::MemoryReference *vmThreadPrivateFlagsMR
            = MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, privateFlags), cg);
        Inst_MemImm(OP::TEST4MemImm4, node, vmThreadPrivateFlagsMR, J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE, cg);

        // Branch to outlined instructions to inline card dirtying.
        //
        TR::LabelSymbol *inlineCardMarkLabel = generateLabelSymbol(cg);

        Inst_Label(OP::JNE4, node, inlineCardMarkLabel, cg);

        // Dirty the card table.
        //
        TR_OutlinedInstructionsGenerator og(inlineCardMarkLabel, node, cg);
        TR::Register *tempReg = srm->findOrCreateScratchRegister();

        Inst_RegReg(OP::MOVRegReg(), node, tempReg, owningObjectReg, cg);

        if (comp->getOptions()->isVariableHeapBaseForBarrierRange0()) {
            TR::MemoryReference *vhbMR
                = MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, heapBaseForBarrierRange0), cg);
            Inst_RegMem(OP::SUBRegMem(), node, tempReg, vhbMR, cg);
        } else {
            uintptr_t chb = comp->getOptions()->getHeapBaseForBarrierRange0();

            if (comp->target().is64Bit()
                && (!IS_32BIT_SIGNED(chb) || TR::Compiler->om.nativeAddressesCanChangeSize())) {
                TR::Register *chbReg = srm->findOrCreateScratchRegister();
                Inst_RegImm64(OP::MOV8RegImm64, node, chbReg, chb, cg, TR_HEAP_BASE_FOR_BARRIER_RANGE);
                Inst_RegReg(OP::SUBRegReg(), node, tempReg, chbReg, cg);
                srm->reclaimScratchRegister(chbReg);
            } else {
                Inst_RegImm(OP::SUBRegImm4(), node, tempReg, (int32_t)chb, cg, TR_HEAP_BASE_FOR_BARRIER_RANGE);
            }
        }

        if (doIsDestAHeapObjectCheck) {
            cardMarkDoneLabel = doIsDestInOldSpaceCheck ? generateLabelSymbol(cg) : doneLabel;

            if (comp->getOptions()->isVariableHeapSizeForBarrierRange0()) {
                TR::MemoryReference *vhsMR
                    = MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, heapSizeForBarrierRange0), cg);
                Inst_RegMem(OP::CMPRegMem(), node, tempReg, vhsMR, cg);
            } else {
                uintptr_t chs = comp->getOptions()->getHeapSizeForBarrierRange0();

                if (comp->target().is64Bit()
                    && (!IS_32BIT_SIGNED(chs) || TR::Compiler->om.nativeAddressesCanChangeSize())) {
                    TR::Register *chsReg = srm->findOrCreateScratchRegister();
                    Inst_RegImm64(OP::MOV8RegImm64, node, chsReg, chs, cg, TR_HEAP_SIZE_FOR_BARRIER_RANGE);
                    Inst_RegReg(OP::CMPRegReg(), node, tempReg, chsReg, cg);
                    srm->reclaimScratchRegister(chsReg);
                } else {
                    Inst_RegImm(OP::CMPRegImm4(), node, tempReg, (int32_t)chs, cg, TR_HEAP_SIZE_FOR_BARRIER_RANGE);
                }
            }

            Inst_Label(OP::JAE4, node, cardMarkDoneLabel, cg);
        }

        Inst_RegImm(OP::SHRRegImm1(), node, tempReg, comp->getOptions()->getHeapAddressToCardAddressShift(), cg);

        // Mark the card
        //
        const uint8_t dirtyCard = 1;

        TR::MemoryReference *cardTableMR;

        if (comp->getOptions()->isVariableActiveCardTableBase()) {
            TR::MemoryReference *actbMR
                = MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, activeCardTableBase), cg);
            Inst_RegMem(OP::ADDRegMem(), node, tempReg, actbMR, cg);
            cardTableMR = MRef_Bdisp32(tempReg, 0, cg);
        } else {
            uintptr_t actb = comp->getOptions()->getActiveCardTableBase();

            if (comp->target().is64Bit()
                && (!IS_32BIT_SIGNED(actb) || TR::Compiler->om.nativeAddressesCanChangeSize())) {
                TR::Register *tempReg3 = srm->findOrCreateScratchRegister();
                Inst_RegImm64(OP::MOV8RegImm64, node, tempReg3, actb, cg, TR_ACTIVE_CARD_TABLE_BASE);
                cardTableMR = MRef_BIS(tempReg3, tempReg, 0, cg);
                srm->reclaimScratchRegister(tempReg3);
            } else {
                cardTableMR = MRef_BISdisp32(NULL, tempReg, 0, (int32_t)actb, cg);
                cardTableMR->setReloKind(TR_ACTIVE_CARD_TABLE_BASE);
            }
        }

        Inst_MemImm(OP::S1MemImm1, node, cardTableMR, dirtyCard, cg);
        srm->reclaimScratchRegister(tempReg);
        Inst_Label(OP::JMP4, node, doneLabel, cg);

        og.endOutlinedInstructionSequence();
    } else if (doInlineCardMarkingWithoutOldSpaceCheck && !dirtyCardTableOutOfLine) {
        // Dirty the card table.
        //
        TR::Register *tempReg = srm->findOrCreateScratchRegister();

        Inst_RegReg(OP::MOVRegReg(), node, tempReg, owningObjectReg, cg);

        if (comp->getOptions()->isVariableHeapBaseForBarrierRange0()) {
            TR::MemoryReference *vhbMR
                = MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, heapBaseForBarrierRange0), cg);
            Inst_RegMem(OP::SUBRegMem(), node, tempReg, vhbMR, cg);
        } else {
            uintptr_t chb = comp->getOptions()->getHeapBaseForBarrierRange0();

            if (comp->target().is64Bit()
                && (!IS_32BIT_SIGNED(chb) || TR::Compiler->om.nativeAddressesCanChangeSize())) {
                TR::Register *chbReg = srm->findOrCreateScratchRegister();
                Inst_RegImm64(OP::MOV8RegImm64, node, chbReg, chb, cg, TR_HEAP_BASE_FOR_BARRIER_RANGE);
                Inst_RegReg(OP::SUBRegReg(), node, tempReg, chbReg, cg);
                srm->reclaimScratchRegister(chbReg);
            } else {
                Inst_RegImm(OP::SUBRegImm4(), node, tempReg, (int32_t)chb, cg, TR_HEAP_BASE_FOR_BARRIER_RANGE);
            }
        }

        if (doIsDestAHeapObjectCheck) {
            cardMarkDoneLabel = doIsDestInOldSpaceCheck ? generateLabelSymbol(cg) : doneLabel;

            if (comp->getOptions()->isVariableHeapSizeForBarrierRange0()) {
                TR::MemoryReference *vhsMR
                    = MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, heapSizeForBarrierRange0), cg);
                Inst_RegMem(OP::CMPRegMem(), node, tempReg, vhsMR, cg);
            } else {
                uintptr_t chs = comp->getOptions()->getHeapSizeForBarrierRange0();

                if (comp->target().is64Bit()
                    && (!IS_32BIT_SIGNED(chs) || TR::Compiler->om.nativeAddressesCanChangeSize())) {
                    TR::Register *chsReg = srm->findOrCreateScratchRegister();
                    Inst_RegImm64(OP::MOV8RegImm64, node, chsReg, chs, cg, TR_HEAP_SIZE_FOR_BARRIER_RANGE);
                    Inst_RegReg(OP::CMPRegReg(), node, tempReg, chsReg, cg);
                    srm->reclaimScratchRegister(chsReg);
                } else {
                    Inst_RegImm(OP::CMPRegImm4(), node, tempReg, (int32_t)chs, cg, TR_HEAP_SIZE_FOR_BARRIER_RANGE);
                }
            }

            Inst_Label(OP::JAE4, node, cardMarkDoneLabel, cg);
        }

        Inst_RegImm(OP::SHRRegImm1(), node, tempReg, comp->getOptions()->getHeapAddressToCardAddressShift(), cg);

        // Mark the card
        //
        const uint8_t dirtyCard = 1;

        TR::MemoryReference *cardTableMR;

        if (comp->getOptions()->isVariableActiveCardTableBase()) {
            TR::MemoryReference *actbMR
                = MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, activeCardTableBase), cg);
            Inst_RegMem(OP::ADDRegMem(), node, tempReg, actbMR, cg);
            cardTableMR = MRef_Bdisp32(tempReg, 0, cg);
        } else {
            uintptr_t actb = comp->getOptions()->getActiveCardTableBase();

            if (comp->target().is64Bit()
                && (!IS_32BIT_SIGNED(actb) || TR::Compiler->om.nativeAddressesCanChangeSize())) {
                TR::Register *tempReg3 = srm->findOrCreateScratchRegister();
                Inst_RegImm64(OP::MOV8RegImm64, node, tempReg3, actb, cg, TR_ACTIVE_CARD_TABLE_BASE);
                cardTableMR = MRef_BIS(tempReg3, tempReg, 0, cg);
                srm->reclaimScratchRegister(tempReg3);
            } else {
                cardTableMR = MRef_BISdisp32(NULL, tempReg, 0, (int32_t)actb, cg);
                cardTableMR->setReloKind(TR_ACTIVE_CARD_TABLE_BASE);
            }
        }

        Inst_MemImm(OP::S1MemImm1, node, cardTableMR, dirtyCard, cg);

        srm->reclaimScratchRegister(tempReg);
    }

    if (doIsDestAHeapObjectCheck && doIsDestInOldSpaceCheck) {
        Inst_Label(OP::label, node, cardMarkDoneLabel, cg);
    }

    if (doSrcIsNullCheck) {
        Inst_RegReg(OP::TESTRegReg(), node, srcReg, srcReg, cg);
        Inst_Label(OP::JE4, node, doneLabel, cg);
    }

    if (doIsDestInOldSpaceCheck) {
        static char *disableWrtbarOpt = feGetEnv("TR_DisableWrtbarOpt");

        OP::Mnemonic branchOp;
        auto gcModeForSnippet = gcMode;

        bool skipSnippetIfSrcNotOld = false;
        bool skipSnippetIfDestOld = false;
        bool skipSnippetIfDestRemembered = false;

        TR::LabelSymbol *labelAfterBranchToSnippet = NULL;

        if (gcMode == gc_modron_wrtbar_always) {
            // Always call the write barrier helper.
            //
            // TODO: this should be an inline call.
            //
            branchOp = OP::JMP4;
        } else if (doCheckConcurrentMarkActive) {
            // TR_ASSERT(wrtbarNode, "Must not be an arraycopy");

            // If the concurrent mark thread IS active then call the gencon write barrier in the helper
            // to perform card marking and any necessary remembered set updates.
            //
            // This is expected to be true for only a very small percentage of the time and hence
            // handling it out of line is justified.
            //
            if (!comp->getOption(TR_DisableWriteBarriersRangeCheck) && (node->getOpCodeValue() == TR::awrtbari)
                && doInternalControlFlow) {
                bool is64Bit = comp->target().is64Bit(); // On compressed refs, owningObjectReg is already uncompressed,
                                                         // and the vmthread fields are 64 bits
                labelAfterBranchToSnippet = generateLabelSymbol(cg);
                // AOT support to be implemented in another PR
                if (!comp->getOptions()->isVariableHeapSizeForBarrierRange0() && !comp->compileRelocatableCode()
                    && !disableWrtbarOpt) {
                    uintptr_t che = comp->getOptions()->getHeapBaseForBarrierRange0()
                        + comp->getOptions()->getHeapSizeForBarrierRange0();
                    if (comp->target().is64Bit() && !IS_32BIT_SIGNED(che)) {
                        Inst_RegMem(OP::CMP8RegMem, node, owningObjectReg,
                            MRef_const(cg->findOrCreate8ByteConstant(node, che), cg), cg);
                    } else {
                        Inst_RegImm(OP::CMPRegImm4(), node, owningObjectReg, (int32_t)che, cg);
                    }
                } else {
                    uintptr_t chb = comp->getOptions()->getHeapBaseForBarrierRange0();
                    TR::Register *tempOwningObjReg = srm->findOrCreateScratchRegister();
                    Inst_RegReg(OP::MOVRegReg(), node, tempOwningObjReg, owningObjectReg, cg);
                    if (comp->target().is64Bit()
                        && (!IS_32BIT_SIGNED(chb) || TR::Compiler->om.nativeAddressesCanChangeSize())) {
                        TR::Register *chbReg = srm->findOrCreateScratchRegister();
                        Inst_RegImm64(OP::MOV8RegImm64, node, chbReg, chb, cg, TR_HEAP_BASE_FOR_BARRIER_RANGE);
                        Inst_RegReg(OP::SUBRegReg(), node, tempOwningObjReg, chbReg, cg);
                        srm->reclaimScratchRegister(chbReg);
                    } else {
                        Inst_RegImm(OP::SUBRegImm4(), node, tempOwningObjReg, (int32_t)chb, cg,
                            TR_HEAP_BASE_FOR_BARRIER_RANGE);
                    }
                    TR::MemoryReference *vhsMR1
                        = MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, heapSizeForBarrierRange0), cg);
                    Inst_RegMem(OP::CMPRegMem(), node, tempOwningObjReg, vhsMR1, cg);
                    srm->reclaimScratchRegister(tempOwningObjReg);
                }

                Inst_Label(OP::JAE1, node, doneLabel, cg);

                skipSnippetIfSrcNotOld = true;
            } else {
                skipSnippetIfDestOld = true;
            }

            // See if we can do a OP::TEST1MemImm1
            //
            int32_t byteOffset = byteOffsetForMask(J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE, cg);
            if (byteOffset != -1) {
                TR::MemoryReference *vmThreadPrivateFlagsMR
                    = MRef_Bdisp32(cg->getVMThreadRegister(), byteOffset + offsetof(J9VMThread, privateFlags), cg);
                Inst_MemImm(OP::TEST1MemImm1, node, vmThreadPrivateFlagsMR,
                    J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE >> (8 * byteOffset), cg);
            } else {
                TR::MemoryReference *vmThreadPrivateFlagsMR
                    = MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, privateFlags), cg);
                Inst_MemImm(OP::TEST4MemImm4, node, vmThreadPrivateFlagsMR, J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE,
                    cg);
            }

            generateWriteBarrierCall(OP::JNE4, node, gc_modron_wrtbar_cardmark_and_oldcheck, owningObjectReg, srcReg,
                doneLabel, cg);

            // If the destination object is old and not remembered then process the remembered
            // set update out-of-line with the generational helper.
            //
            skipSnippetIfDestRemembered = true;
            gcModeForSnippet = gc_modron_wrtbar_oldcheck;
        } else if (gcMode == gc_modron_wrtbar_oldcheck) {
            // For pure generational barriers if the object is old and remembered then the helper
            // can be skipped.
            //
            skipSnippetIfDestOld = true;
            skipSnippetIfDestRemembered = true;
        } else {
            skipSnippetIfDestOld = true;
            skipSnippetIfDestRemembered = false;
        }

        if (skipSnippetIfSrcNotOld || skipSnippetIfDestOld) {
            TR_ASSERT((!skipSnippetIfSrcNotOld || !skipSnippetIfDestOld),
                "At most one of skipSnippetIfSrcNotOld and skipSnippetIfDestOld can be true");
            TR_ASSERT(skipSnippetIfDestOld || (srcReg != NULL), "Expected to have a source register for wrtbari");

            bool is64Bit = comp->target().is64Bit(); // On compressed refs, owningObjectReg is already uncompressed, and
                                                     // the vmthread fields are 64 bits
            bool checkDest = skipSnippetIfDestOld; // Otherwise, check the src value
            bool skipSnippetIfOld
                = skipSnippetIfDestOld; // Otherwise, skip if the checked value (source or destination) is not old
            labelAfterBranchToSnippet = generateLabelSymbol(cg);
            // AOT support to be implemented in another PR
            if (!comp->getOptions()->isVariableHeapSizeForBarrierRange0() && !comp->compileRelocatableCode()
                && !disableWrtbarOpt) {
                uintptr_t che = comp->getOptions()->getHeapBaseForBarrierRange0()
                    + comp->getOptions()->getHeapSizeForBarrierRange0();
                if (comp->target().is64Bit() && !IS_32BIT_SIGNED(che)) {
                    Inst_RegMem(OP::CMP8RegMem, node, checkDest ? owningObjectReg : srcReg,
                        MRef_const(cg->findOrCreate8ByteConstant(node, che), cg), cg);
                } else {
                    Inst_RegImm(OP::CMPRegImm4(), node, checkDest ? owningObjectReg : srcReg, (int32_t)che, cg);
                }
            } else {
                uintptr_t chb = comp->getOptions()->getHeapBaseForBarrierRange0();
                TR::Register *tempReg = srm->findOrCreateScratchRegister();
                Inst_RegReg(OP::MOVRegReg(), node, tempReg, checkDest ? owningObjectReg : srcReg, cg);
                if (comp->target().is64Bit()
                    && (!IS_32BIT_SIGNED(chb) || TR::Compiler->om.nativeAddressesCanChangeSize())) {
                    TR::Register *chbReg = srm->findOrCreateScratchRegister();
                    Inst_RegImm64(OP::MOV8RegImm64, node, chbReg, chb, cg, TR_HEAP_BASE_FOR_BARRIER_RANGE);
                    Inst_RegReg(OP::SUBRegReg(), node, tempReg, chbReg, cg);
                    srm->reclaimScratchRegister(chbReg);
                } else {
                    Inst_RegImm(OP::SUBRegImm4(), node, tempReg, (int32_t)chb, cg, TR_HEAP_BASE_FOR_BARRIER_RANGE);
                }
                TR::MemoryReference *vhsMR1
                    = MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, heapSizeForBarrierRange0), cg);
                Inst_RegMem(OP::CMPRegMem(), node, tempReg, vhsMR1, cg);
            }

            branchOp = skipSnippetIfOld ? OP::JB4 : OP::JAE4; // For branch to snippet
            OP::Mnemonic reverseBranchOp = skipSnippetIfOld ? OP::JAE4 : OP::JB4; // For branch past snippet

            // Now performing check for remembered
            if (skipSnippetIfDestRemembered) {
                // Set up for branch *past* snippet call for previous comparison
                Inst_Label(reverseBranchOp, node, labelAfterBranchToSnippet, cg);

                int32_t byteOffset = byteOffsetForMask(J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST, cg);
                if (byteOffset != -1) {
                    TR::MemoryReference *MR
                        = MRef_Bdisp32(owningObjectReg, byteOffset + TR::Compiler->om.offsetOfHeaderFlags(), cg);
                    Inst_MemImm(OP::TEST1MemImm1, node, MR,
                        J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST >> (8 * byteOffset), cg);
                } else {
                    TR::MemoryReference *MR = MRef_Bdisp32(owningObjectReg, TR::Compiler->om.offsetOfHeaderFlags(), cg);
                    Inst_MemImm(OP::TEST4MemImm4, node, MR, J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST, cg);
                }
                branchOp = OP::JE4;
            }
        }

        generateWriteBarrierCall(branchOp, node, gcModeForSnippet, owningObjectReg, srcReg, doneLabel, cg);

        if (labelAfterBranchToSnippet)
            Inst_Label(OP::label, node, labelAfterBranchToSnippet, cg);
    }

    int32_t numPostConditions = 2 + srm->numAvailableRegisters();

    if (srcReg) {
        numPostConditions++;
    }

    TR::RegisterDependencyConditions *conditions = RegDeps((uint8_t)0, numPostConditions, cg);

    conditions->addPostCondition(owningObjectReg, TR::RealRegister::NoReg, cg);
    if (srcReg) {
        conditions->addPostCondition(srcReg, TR::RealRegister::NoReg, cg);
    }

    conditions->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);

    srm->addScratchRegistersToDependencyList(conditions);
    conditions->stopAddingConditions();

    Inst_Label(OP::label, node, doneLabel, conditions, cg);

    srm->stopUsingRegisters();
}

static TR::Instruction *doReferenceStore(TR::Node *node, TR::MemoryReference *storeMR, TR::Register *sourceReg,
    bool usingCompressedPointers, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    OP::Mnemonic storeOp = usingCompressedPointers ? OP::S4MemReg : OP::SMemReg();
    TR::Instruction *instr = Inst_MemReg(storeOp, node, storeMR, sourceReg, cg);

    // for real-time GC, the data reference has already been resolved into an earlier LEA instruction so this padding
    // isn't needed
    //   even if the node symbol is marked as unresolved (the store instruction above is storing through a register
    //   that contains the resolved address)
    if (!comp->getOptions()->realTimeGC() && node->getSymbolReference()->isUnresolved()) {
        TR::TreeEvaluator::padUnresolvedDataReferences(node, *node->getSymbolReference(), cg);
    }

    return instr;
}

void J9::X86::TreeEvaluator::VMwrtbarWithStoreEvaluator(TR::Node *node, TR::MemoryReference *storeMR,
    TR_X86ScratchRegisterManager *scratchRegisterManager, TR::Node *destOwningObject, TR::Node *sourceObject,
    bool isImplicitExceptionPoint, TR::CodeGenerator *cg, bool nullAdjusted)
{
    TR_ASSERT(storeMR, "assertion failure");

    TR::Compilation *comp = cg->comp();

    TR::Register *owningObjectRegister = cg->evaluate(destOwningObject);
    TR::Register *sourceRegister = cg->evaluate(sourceObject);

    auto gcMode = TR::Compiler->om.writeBarrierType();
    bool isRealTimeGC = (comp->getOptions()->realTimeGC()) ? true : false;

    bool usingCompressedPointers = false;
    bool usingLowMemHeap = false;
    bool useShiftedOffsets = (TR::Compiler->om.compressedReferenceShiftOffset() != 0);
    TR::Node *translatedStore = NULL;

    // NOTE:
    //
    // If you change this code you also need to change writeBarrierEvaluator() in TreeEvaluator.cpp
    //
    if (comp->useCompressedPointers()
        && ((node->getOpCode().isCheck() && node->getFirstChild()->getOpCode().isIndirect()
                && (node->getFirstChild()->getSecondChild()->getDataType() != TR::Address))
            || (node->getOpCode().isIndirect() && (node->getSecondChild()->getDataType() != TR::Address)))) {
        if (node->getOpCode().isCheck())
            translatedStore = node->getFirstChild();
        else
            translatedStore = node;

        usingLowMemHeap = true;
        usingCompressedPointers = true;
    }

    TR::Register *translatedSourceReg = sourceRegister;
    if (usingCompressedPointers && (!usingLowMemHeap || useShiftedOffsets)) {
        // handle stores of null values here

        if (nullAdjusted)
            translatedSourceReg = translatedStore->getSecondChild()->getRegister();
        else {
            translatedSourceReg = cg->evaluate(translatedStore->getSecondChild());
            if (!usingLowMemHeap) {
                Inst_RegReg(OP::TESTRegReg(), translatedStore, sourceRegister, sourceRegister, cg);
                Inst_RegReg(OP::CMOVERegReg(), translatedStore, translatedSourceReg, sourceRegister, cg);
            }
        }
    }

    TR::Instruction *storeInstr = NULL;
    TR::Register *storeAddressRegForRealTime = NULL;

    if (isRealTimeGC) {
        // Realtime GC evaluates storeMR into a register here and then uses it to do the store after the write barrier

        // If reference is unresolved, need to resolve it right here before the barrier starts
        // Otherwise, we could get stopped during the resolution and that could invalidate any tests we would have
        // performend
        //   beforehand
        // For simplicity, just evaluate the store address into storeAddressRegForRealTime right now
        storeAddressRegForRealTime = scratchRegisterManager->findOrCreateScratchRegister();
        Inst_RegMem(OP::LEARegMem(), node, storeAddressRegForRealTime, storeMR, cg);
        if (node->getSymbolReference()->isUnresolved()) {
            TR::TreeEvaluator::padUnresolvedDataReferences(node, *node->getSymbolReference(), cg);

            // storeMR was created against a (i)wrtbar node which is a store.  The unresolved data snippet that
            //  was created set the checkVolatility bit based on that node being a store.  Since the resolution
            //  is now going to occur on a LEA instruction, which does not require any memory fence and hence
            //  no volatility check, we need to clear that "store" ness of the unresolved data snippet
            TR::UnresolvedDataSnippet *snippet = storeMR->getUnresolvedDataSnippet();
            if (snippet)
                snippet->resetUnresolvedStore();
        }
    } else {
        // Non-realtime does the store first, then the write barrier.
        //
        storeInstr = doReferenceStore(node, storeMR, translatedSourceReg, usingCompressedPointers, cg);
    }

    if (TR::Compiler->om.writeBarrierType() == gc_modron_wrtbar_always && !isRealTimeGC) {
        TR::RegisterDependencyConditions *deps = NULL;
        TR::LabelSymbol *doneWrtBarLabel = generateLabelSymbol(cg);

        if (comp->target().is32Bit() && sourceObject->isNonNull() == false) {
            TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
            startLabel->setStartInternalControlFlow();
            doneWrtBarLabel->setEndInternalControlFlow();

            Inst_Label(OP::label, node, startLabel, cg);
            Inst_RegReg(OP::TESTRegReg(), node, sourceRegister, sourceRegister, cg);
            Inst_Label(OP::JE4, node, doneWrtBarLabel, cg);

            deps = RegDeps(0, 3, cg);
            deps->addPostCondition(sourceRegister, TR::RealRegister::NoReg, cg);
            deps->addPostCondition(owningObjectRegister, TR::RealRegister::NoReg, cg);
            deps->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);
            deps->stopAddingConditions();
        }

        Inst_MemReg(OP::SMemReg(), node, MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, floatTemp1), cg),
            owningObjectRegister, cg);
        Inst_MemReg(OP::SMemReg(), node, MRef_Bdisp32(cg->getVMThreadRegister(), offsetof(J9VMThread, floatTemp2), cg),
            sourceRegister, cg);

        TR::SymbolReference *wrtBarSymRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef();
        Inst_ImmSym(OP::CALLImm4, node, (uintptr_t)wrtBarSymRef->getMethodAddress(), wrtBarSymRef, cg);

        Inst_Label(OP::label, node, doneWrtBarLabel, deps, cg);
    } else {
        if (isRealTimeGC) {
            TR::TreeEvaluator::VMwrtbarRealTimeWithoutStoreEvaluator(node, storeMR, storeAddressRegForRealTime,
                destOwningObject, sourceObject, NULL, scratchRegisterManager, cg);
        } else {
            TR::TreeEvaluator::VMwrtbarWithoutStoreEvaluator(node, destOwningObject, sourceObject, NULL,
                scratchRegisterManager, cg);
        }
    }

    // Realtime GCs must do the write barrier first and then the store.
    //
    if (isRealTimeGC) {
        TR_ASSERT(storeAddressRegForRealTime, "assertion failure");
        TR::MemoryReference *myStoreMR = MRef_Bdisp32(storeAddressRegForRealTime, 0, cg);
        storeInstr = doReferenceStore(node, myStoreMR, translatedSourceReg, usingCompressedPointers, cg);
        scratchRegisterManager->reclaimScratchRegister(storeAddressRegForRealTime);
    }

    if (!usingLowMemHeap || useShiftedOffsets)
        cg->decReferenceCount(sourceObject);

    cg->decReferenceCount(destOwningObject);
    storeMR->decNodeReferenceCounts(cg);

    if (isImplicitExceptionPoint)
        cg->setImplicitExceptionPoint(storeInstr);
}

void J9::X86::TreeEvaluator::generateVFTMaskInstruction(TR::Node *node, TR::Register *reg, TR::CodeGenerator *cg)
{
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
    uintptr_t mask = TR::Compiler->om.maskOfObjectVftField();
    bool is64Bit = cg->comp()->target().is64Bit(); // even with compressed object headers, a 64-bit mask operation is
                                                   // safe, though it may waste 1 byte because of the rex prefix
    if (~mask == 0) {
        // no mask instruction required
    } else if (~mask <= 127) {
        Inst_RegImm(OP::ANDRegImms(is64Bit), node, reg, TR::Compiler->om.maskOfObjectVftField(), cg);
    } else {
        Inst_RegImm(OP::ANDRegImm4(is64Bit), node, reg, TR::Compiler->om.maskOfObjectVftField(), cg);
    }
}

void VMgenerateCatchBlockBBStartPrologue(TR::Node *node, TR::Instruction *fenceInstruction, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

    if (comp->getJittedMethodSymbol()->usesSinglePrecisionMode() && cg->enableSinglePrecisionMethods()) {
        cg->setLastCatchAppendInstruction(fenceInstruction);
    }

    TR::Block *block = node->getBlock();
    if (fej9->shouldPerformEDO(block, comp)) {
        TR_ASSERT_FATAL(cg->comp()->getRecompilationInfo(), "Recompilation info should be available");

        TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
        TR::LabelSymbol *restartLabel = generateLabelSymbol(cg);

        Inst_Mem(OP::DEC4Mem, node, MRef_sym(comp->getRecompilationInfo()->getCounterSymRef(), cg), cg);
        Inst_Label(OP::JE4, node, snippetLabel, cg);
        Inst_Label(OP::label, node, restartLabel, cg);
        cg->addSnippet(new (cg->trHeapMemory()) TR::X86ForceRecompilationSnippet(cg, node, restartLabel, snippetLabel));
        cg->comp()->getRecompilationInfo()->getJittedBodyInfo()->setHasEdoSnippet();
    }
}

TR::Register *J9::X86::TreeEvaluator::tstartEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    /*
       xbegin fall_back_path
       mov monReg, [obj+Lw_offset]
       cmp monReg, 0;
       je fallThroughLabel
       cmp monReg, rbp
       je fallThroughLabel
       xabort
    fall_back_path:
       test eax, 0x2
       jne gotoTransientFailureNodeLabel
       test eax, 0x00000001
       je persistentFailureLabel
       test eax, 0x01000000
       jne gotoTransientFailureNodeLabel
       jmp persistentFailLabel
    gotoTransientFailureNodeLabel:
       mov counterReg,100
    spinLabel:
       dec counterReg
       jne spinLabel
       jmp TransientFailureNodeLabel
    */
    TR::Compilation *comp = cg->comp();
    TR::Node *persistentFailureNode = node->getFirstChild();
    TR::Node *transientFailureNode = node->getSecondChild();
    TR::Node *fallThroughNode = node->getThirdChild();
    TR::Node *objNode = node->getChild(3);
    TR::Node *GRANode = NULL;

    TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    startLabel->setStartInternalControlFlow();
    TR::LabelSymbol *endLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    endLabel->setEndInternalControlFlow();

    TR::LabelSymbol *gotoTransientFailure = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    TR::LabelSymbol *gotoPersistentFailure = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    TR::LabelSymbol *gotoFallThrough = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    TR::LabelSymbol *transientFailureLabel = transientFailureNode->getBranchDestination()->getNode()->getLabel();
    TR::LabelSymbol *persistentFailureLabel = persistentFailureNode->getBranchDestination()->getNode()->getLabel();
    TR::LabelSymbol *fallBackPathLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    TR::LabelSymbol *fallThroughLabel = fallThroughNode->getBranchDestination()->getNode()->getLabel();

    TR::Register *objReg = cg->evaluate(objNode);
    TR::Register *accReg = cg->allocateRegister();
    TR::Register *monReg = cg->allocateRegister();
    TR::RegisterDependencyConditions *fallBackConditions = RegDeps((uint8_t)0, 2, cg);
    TR::RegisterDependencyConditions *endLabelConditions;
    TR::RegisterDependencyConditions *fallThroughConditions = NULL;
    TR::RegisterDependencyConditions *persistentConditions = NULL;
    TR::RegisterDependencyConditions *transientConditions = NULL;

    if (fallThroughNode->getNumChildren() != 0) {
        GRANode = fallThroughNode->getFirstChild();
        cg->evaluate(GRANode);
        fallThroughConditions = RegDeps(GRANode, cg, 0);
        cg->decReferenceCount(GRANode);
    }

    if (persistentFailureNode->getNumChildren() != 0) {
        GRANode = persistentFailureNode->getFirstChild();
        cg->evaluate(GRANode);
        persistentConditions = RegDeps(GRANode, cg, 0);
        cg->decReferenceCount(GRANode);
    }

    if (transientFailureNode->getNumChildren() != 0) {
        GRANode = transientFailureNode->getFirstChild();
        cg->evaluate(GRANode);
        transientConditions = RegDeps(GRANode, cg, 0);
        cg->decReferenceCount(GRANode);
    }

    // startLabel
    // add place holder register so that eax would not contain any useful value before xbegin
    TR::Register *dummyReg = cg->allocateRegister();
    dummyReg->setPlaceholderReg();
    TR::RegisterDependencyConditions *startLabelConditions = RegDeps((uint8_t)0, 1, cg);
    startLabelConditions->addPostCondition(dummyReg, TR::RealRegister::eax, cg);
    startLabelConditions->stopAddingConditions();
    cg->stopUsingRegister(dummyReg);
    Inst_Label(OP::label, node, startLabel, startLabelConditions, cg);

    // xbegin fall_back_path
    Inst_LongLabel(OP::XBEGIN4, node, fallBackPathLabel, cg);
    // mov monReg, obj+offset
    int32_t lwOffset = cg->fej9()->getByteOffsetToLockword((TR_OpaqueClassBlock *)cg->getMonClass(node));
    TR::MemoryReference *objLockRef = MRef_Bdisp32(objReg, lwOffset, cg);
    if (comp->target().is64Bit() && cg->fej9()->generateCompressedLockWord()) {
        Inst_RegMem(OP::L4RegMem, node, monReg, objLockRef, cg);
    } else {
        Inst_RegMem(OP::LRegMem(), node, monReg, objLockRef, cg);
    }

    if (comp->target().is64Bit() && cg->fej9()->generateCompressedLockWord()) {
        Inst_RegImm(OP::CMP4RegImm4, node, monReg, 0, cg);
    } else {
        Inst_RegImm(OP::CMPRegImm4(), node, monReg, 0, cg);
    }

    if (fallThroughConditions)
        Inst_Label(OP::JE4, node, fallThroughLabel, fallThroughConditions, cg);
    else
        Inst_Label(OP::JE4, node, fallThroughLabel, cg);

    TR::Register *vmThreadReg = cg->getVMThreadRegister();
    if (comp->target().is64Bit() && cg->fej9()->generateCompressedLockWord()) {
        Inst_RegReg(OP::CMP4RegReg, node, monReg, vmThreadReg, cg);
    } else {
        Inst_RegReg(OP::CMPRegReg(), node, monReg, vmThreadReg, cg);
    }

    if (fallThroughConditions)
        Inst_Label(OP::JE4, node, fallThroughLabel, fallThroughConditions, cg);
    else
        Inst_Label(OP::JE4, node, fallThroughLabel, cg);

    // xabort
    Inst_Imm(OP::XABORT, node, 0x01, cg);

    cg->stopUsingRegister(monReg);
    // fall_back_path:
    Inst_Label(OP::label, node, fallBackPathLabel, cg);

    endLabelConditions = RegDeps((uint8_t)0, 1, cg);
    endLabelConditions->addPostCondition(accReg, TR::RealRegister::eax, cg);
    endLabelConditions->stopAddingConditions();

    // test eax, 0x2
    Inst_RegImm(OP::TEST1AccImm1, node, accReg, 0x2, cg);
    Inst_Label(OP::JNE4, node, gotoTransientFailure, cg);

    // abort because of nonzero lockword is also transient failure
    Inst_RegImm(OP::TEST4AccImm4, node, accReg, 0x00000001, cg);
    if (persistentConditions)
        Inst_Label(OP::JE4, node, persistentFailureLabel, persistentConditions, cg);
    else
        Inst_Label(OP::JE4, node, persistentFailureLabel, cg);

    Inst_RegImm(OP::TEST4AccImm4, node, accReg, 0x01000000, cg);
    // je gotransientFailureNodeLabel
    Inst_Label(OP::JNE4, node, gotoTransientFailure, cg);

    if (persistentConditions)
        Inst_Label(OP::JMP4, node, persistentFailureLabel, persistentConditions, cg);
    else
        Inst_Label(OP::JMP4, node, persistentFailureLabel, cg);
    cg->stopUsingRegister(accReg);

    // gotoTransientFailureLabel:
    if (transientConditions)
        Inst_Label(OP::label, node, gotoTransientFailure, transientConditions, cg);
    else
        Inst_Label(OP::label, node, gotoTransientFailure, cg);

    // delay
    TR::Register *counterReg = cg->allocateRegister();
    Inst_RegImm(OP::MOV4RegImm4, node, counterReg, 100, cg);
    TR::LabelSymbol *spinLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    Inst_Label(OP::label, node, spinLabel, cg);
    Inst(OP::PAUSE, node, cg);
    Inst(OP::PAUSE, node, cg);
    Inst(OP::PAUSE, node, cg);
    Inst(OP::PAUSE, node, cg);
    Inst(OP::PAUSE, node, cg);
    Inst_Reg(OP::DEC4Reg, node, counterReg, cg);
    TR::RegisterDependencyConditions *loopConditions = RegDeps((uint8_t)0, 1, cg);
    loopConditions->addPostCondition(counterReg, TR::RealRegister::NoReg, cg);
    loopConditions->stopAddingConditions();
    Inst_Label(OP::JNE4, node, spinLabel, loopConditions, cg);
    cg->stopUsingRegister(counterReg);

    if (transientConditions)
        Inst_Label(OP::JMP4, node, transientFailureLabel, transientConditions, cg);
    else
        Inst_Label(OP::JMP4, node, transientFailureLabel, cg);

    Inst_Label(OP::label, node, endLabel, endLabelConditions, cg);
    cg->decReferenceCount(objNode);
    cg->decReferenceCount(persistentFailureNode);
    cg->decReferenceCount(transientFailureNode);
    return NULL;
}

TR::Register *J9::X86::TreeEvaluator::tfinishEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    Inst(OP::XEND, node, cg);
    return NULL;
}

TR::Register *J9::X86::TreeEvaluator::tabortEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    Inst_Imm(OP::XABORT, node, 0x04, cg);
    return NULL;
}

TR::Register *J9::X86::TreeEvaluator::directCallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    static bool useJapaneseCompression = (feGetEnv("TR_JapaneseComp") != NULL);
    TR::Compilation *comp = cg->comp();
    TR::SymbolReference *symRef = node->getSymbolReference();

    bool callInlined = false;
    TR::Register *returnRegister = NULL;
    TR::MethodSymbol *symbol = node->getSymbol()->castToMethodSymbol();

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
    if (cg->inlineCryptoMethod(node, returnRegister)) {
        return returnRegister;
    }
#endif

    if (symbol->isHelper()) {
        switch (symRef->getReferenceNumber()) {
            case TR_checkAssignable:
                return TR::TreeEvaluator::checkcastinstanceofEvaluator(node, cg);
            default:
                break;
        }
    }

    switch (symbol->getMandatoryRecognizedMethod()) {
        case TR::java_lang_StringLatin1_indexOfChar:
        case TR::com_ibm_jit_JITHelpers_intrinsicIndexOfLatin1:
            if (cg->getSupportsInlineStringIndexOf())
                return inlineIntrinsicIndexOf(node, cg, true);
            break;

        case TR::java_lang_StringUTF16_indexOfCharUnsafe:
        case TR::com_ibm_jit_JITHelpers_intrinsicIndexOfUTF16:
            if (cg->getSupportsInlineStringIndexOf())
                return inlineIntrinsicIndexOf(node, cg, false);
            break;

        case TR::java_lang_StringLatin1_indexOf:
        case TR::com_ibm_jit_JITHelpers_intrinsicIndexOfStringLatin1:
            if (cg->getSupportsInlineStringIndexOfString())
                returnRegister = inlineIntrinsicStringIndexOfString(node, cg, true);

            callInlined = (returnRegister != NULL);
            break;

        case TR::java_lang_StringUTF16_indexOf:
        case TR::java_lang_StringUTF16_indexOfUnsafe:
        case TR::com_ibm_jit_JITHelpers_intrinsicIndexOfStringUTF16:
            if (cg->getSupportsInlineStringIndexOfString())
                returnRegister = inlineIntrinsicStringIndexOfString(node, cg, false);

            callInlined = (returnRegister != NULL);
            break;

        case TR::com_ibm_jit_JITHelpers_transformedEncodeUTF16Big:
        case TR::com_ibm_jit_JITHelpers_transformedEncodeUTF16Little:
            return TR::TreeEvaluator::encodeUTF16Evaluator(node, cg);

        case TR::java_lang_String_hashCodeImplDecompressed:
            if (cg->getSupportsInlineStringHashCode())
                returnRegister = inlineStringHashCode(node, false, cg);

            callInlined = (returnRegister != NULL);
            break;

        case TR::java_lang_String_hashCodeImplCompressed:
            if (cg->getSupportsInlineStringHashCode())
                returnRegister = inlineStringHashCode(node, true, cg);

            callInlined = (returnRegister != NULL);
            break;

        default:
            break;
    }

    if (cg->getSupportsInlineStringCaseConversion()) {
        switch (symbol->getRecognizedMethod()) {
            case TR::com_ibm_jit_JITHelpers_toUpperIntrinsicUTF16:
                return TR::TreeEvaluator::toUpperIntrinsicUTF16Evaluator(node, cg);
            case TR::com_ibm_jit_JITHelpers_toUpperIntrinsicLatin1:
                return TR::TreeEvaluator::toUpperIntrinsicLatin1Evaluator(node, cg);
            case TR::com_ibm_jit_JITHelpers_toLowerIntrinsicUTF16:
                return TR::TreeEvaluator::toLowerIntrinsicUTF16Evaluator(node, cg);
            case TR::com_ibm_jit_JITHelpers_toLowerIntrinsicLatin1:
                return TR::TreeEvaluator::toLowerIntrinsicLatin1Evaluator(node, cg);
            default:
                break;
        }
    }

    switch (symbol->getRecognizedMethod()) {
        case TR::java_lang_Thread_onSpinWait: {
            static char *disableOSW = feGetEnv("TR_noPauseOnSpinWait");
            if (!disableOSW) {
                Inst(OP::PAUSE, node, cg);

                static char *printIt = feGetEnv("TR_showPauseOnSpinWait");
                if (printIt && comp->getOption(TR_TraceCG)) {
                    comp->log()->printf("insert PAUSE for onSpinWait : node=%p, %s\n", node, comp->signature());
                }

                return NULL;
            } else
                break;
        }
#if JAVA_SPEC_VERSION < 19
        case TR::java_lang_StringCoding_hasNegatives:
#endif /* JAVA_SPEC_VERSION < 19 */
        case TR::java_lang_StringCoding_countPositives: {
            if (cg->comp()->target().is64Bit() && !TR::Compiler->om.canGenerateArraylets()) {
                return inlineHasNegativesOrCountPositives(node, symbol->getRecognizedMethod(), cg);
            }
        } break;
        case TR::java_nio_Bits_keepAlive:
        case TR::java_lang_ref_Reference_reachabilityFence: {
            TR_ASSERT(node->getNumChildren() == 1, "keepAlive is assumed to have just one argument");

            // The only purpose of keepAlive is to prevent an otherwise
            // unreachable object from being garbage collected, because we don't
            // want its finalizer to be called too early.  There's no need to
            // generate a full-blown call site just for this purpose.

            TR::Register *valueToKeepAlive = cg->evaluate(node->getFirstChild());

            // In theory, a value could be kept alive on the stack, rather than in
            // a register.  It is unfortunate that the following deps will force
            // the value into a register for no reason.  However, in many common
            // cases, this label will have no effect on the generated code, and
            // will only affect GC maps.
            //
            TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)1, (uint8_t)1, cg);
            deps->addPreCondition(valueToKeepAlive, TR::RealRegister::NoReg, cg);
            deps->addPostCondition(valueToKeepAlive, TR::RealRegister::NoReg, cg);
            new (cg->trHeapMemory()) TR::X86LabelInstruction(OP::label, node, generateLabelSymbol(cg), deps, cg);
            cg->decReferenceCount(node->getFirstChild());

            return NULL; // keepAlive has no return value
        }

        case TR::java_math_BigDecimal_noLLOverflowAdd:
        case TR::java_math_BigDecimal_noLLOverflowMul:
            if (cg->getSupportsBDLLHardwareOverflowCheck()) {
                // Eat this call as its only here to anchor where a long lookaside overflow check
                // needs to be done.  There should be a TR::icmpeq node following
                // this one where the real overflow check will be inserted.
                //
                cg->recursivelyDecReferenceCount(node->getFirstChild());
                cg->recursivelyDecReferenceCount(node->getSecondChild());
                cg->evaluate(node->getChild(2));
                cg->decReferenceCount(node->getChild(2));
                returnRegister = cg->allocateRegister();
                node->setRegister(returnRegister);
                return returnRegister;
            }

            break;
        case TR::java_lang_StringLatin1_inflate_BICII:
            if (cg->getSupportsInlineStringLatin1Inflate()) {
                return TR::TreeEvaluator::inlineStringLatin1Inflate(node, cg);
            }
            break;
        case TR::java_lang_Math_fma_F:
        case TR::java_lang_Math_fma_D:
        case TR::java_lang_StrictMath_fma_F:
        case TR::java_lang_StrictMath_fma_D: {
            static bool disableInlineFMA = feGetEnv("TR_DisableInlineFMA") != NULL;

            if (!disableInlineFMA && cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_X86_FMA))
                return inlineMathFma(node, cg);

            break;
        }
        case TR::jdk_internal_util_ArraysSupport_vectorizedHashCode: {
            if (cg->getSupportsInlineVectorizedHashCode()) {
                TR::Register *result = inlineVectorizedHashCode(node, cg);
                if (result)
                    return result;
            }
        }
        case TR::java_lang_Math_sqrt:
        case TR::java_lang_StrictMath_sqrt:
        case TR::java_lang_System_nanoTime:
        case TR::sun_nio_ch_NativeThread_current:
            if (TR::TreeEvaluator::VMinlineCallEvaluator(node, false, cg)) {
                returnRegister = node->getRegister();
            } else {
                returnRegister = TR::TreeEvaluator::performCall(node, false, true, cg);
            }

            callInlined = true;
            break;
        case TR::java_lang_Integer_compareUnsigned:
            if (cg->getSupportsInlineIntegerCompareUnsigned()) {
                return inlineIntegerLongCompareUnsigned(node, true /* isInt */, cg);
            }
            break;
        case TR::java_lang_Long_compareUnsigned:
            if (cg->getSupportsInlineLongCompareUnsigned()) {
                return inlineIntegerLongCompareUnsigned(node, false /* isInt */, cg);
            }
            break;
        default:
            break;
    }

    // If the method to be called is marked as an inline method, see if it can
    // actually be generated inline.
    //
    if (!callInlined && (symbol->isVMInternalNative() || symbol->isJITInternalNative())) {
        if (TR::TreeEvaluator::VMinlineCallEvaluator(node, false, cg))
            return node->getRegister();
        else
            return TR::TreeEvaluator::performCall(node, false, true, cg);
    } else if (callInlined) {
        return returnRegister;
    }

    // Call was not inlined.  Delegate to the parent directCallEvaluator.
    //
    return J9::TreeEvaluator::directCallEvaluator(node, cg);
}

TR::Register *J9::X86::TreeEvaluator::inlineStringLatin1Inflate(TR::Node *node, TR::CodeGenerator *cg)
{
    TR_ASSERT_FATAL(cg->comp()->target().is64Bit(), "StringLatin1.inflate only supported on 64-bit targets");
    TR_ASSERT_FATAL(cg->getSupportsInlineStringLatin1Inflate(), "Inlining of StringLatin1.inflate not supported");
    TR_ASSERT_FATAL(!TR::Compiler->om.canGenerateArraylets(),
        "StringLatin1.inflate intrinsic is not supported with arraylets");
    TR_ASSERT_FATAL_WITH_NODE(node, node->getNumChildren() == 5,
        "Wrong number of children in inlineStringLatin1Inflate");

    intptr_t headerOffsetConst = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
    uint8_t vectorLengthConst = 16;

    TR::Register *srcBufferReg = cg->evaluate(node->getChild(0));
    TR::Register *srcOffsetReg = cg->gprClobberEvaluate(node->getChild(1), OP::MOV4RegReg);
    TR::Register *destBufferReg = cg->evaluate(node->getChild(2));
    TR::Register *destOffsetReg = cg->gprClobberEvaluate(node->getChild(3), OP::MOV4RegReg);
    TR::Register *lengthReg = cg->gprClobberEvaluate(node->getChild(4), OP::MOV4RegReg);

    TR::Register *xmmHighReg = cg->allocateRegister(TR_VRF);
    TR::Register *xmmLowReg = cg->allocateRegister(TR_VRF);
    TR::Register *zeroReg = cg->allocateRegister(TR_VRF);
    TR::Register *scratchReg = cg->allocateRegister(TR_GPR);

    int maxDepCount = 10;
    TR::RegisterDependencyConditions *deps = RegDeps(0, maxDepCount, cg);
    deps->addPostCondition(xmmHighReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(xmmLowReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(zeroReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(lengthReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(srcBufferReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(destBufferReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(scratchReg, TR::RealRegister::eax, cg);
    deps->addPostCondition(srcOffsetReg, TR::RealRegister::ecx, cg);
    deps->addPostCondition(destOffsetReg, TR::RealRegister::edx, cg);

    TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *copyResidueLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *afterCopy8Label = generateLabelSymbol(cg);

    TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
    startLabel->setStartInternalControlFlow();
    Inst_Label(OP::label, node, startLabel, cg);

    TR::Node *destOffsetNode = node->getChild(3);

    if (!destOffsetNode->isConstZeroValue()) {
        // dest offset measured in characters, convert it to bytes
        Inst_RegReg(OP::ADD4RegReg, node, destOffsetReg, destOffsetReg, cg);
    }

    Inst_RegReg(OP::TEST4RegReg, node, lengthReg, lengthReg, cg);
    Inst_Label(OP::JE4, node, doneLabel, cg);

    Inst_RegImm(OP::CMP4RegImm4, node, lengthReg, 8, cg);
    Inst_Label(OP::JL4, node, afterCopy8Label, cg);

    // make sure the register is zero before interleaving
    Inst_RegReg(OP::PXORRegReg, node, zeroReg, zeroReg, cg);

    TR::LabelSymbol *startLoop = generateLabelSymbol(cg);
    TR::LabelSymbol *endLoop = generateLabelSymbol(cg);

    // vectorized add in loop, 16 bytes per iteration
    // use srcOffsetReg for loop counter, add starting offset to lengthReg, subtract 16 (xmm register size)
    // to prevent reading/writing beyond the end of the array
    Inst_RegMem(OP::LEA4RegMem, node, scratchReg, MRef_BISdisp32(lengthReg, srcOffsetReg, 0, -vectorLengthConst, cg),
        cg);

    Inst_Label(OP::label, node, startLoop, cg);
    Inst_RegReg(OP::CMP4RegReg, node, srcOffsetReg, scratchReg, cg);
    Inst_Label(OP::JG4, node, endLoop, cg);

    Inst_RegMem(OP::MOVDQURegMem, node, xmmHighReg,
        MRef_BISdisp32(srcBufferReg, srcOffsetReg, 0, headerOffsetConst, cg), cg);

    Inst_RegReg(OP::MOVDQURegReg, node, xmmLowReg, xmmHighReg, cg);
    Inst_RegReg(OP::PUNPCKHBWRegReg, node, xmmLowReg, zeroReg, cg);
    Inst_MemReg(OP::MOVDQUMemReg, node,
        MRef_BISdisp32(destBufferReg, destOffsetReg, 0, headerOffsetConst + vectorLengthConst, cg), xmmLowReg, cg);

    Inst_RegReg(OP::PUNPCKLBWRegReg, node, xmmHighReg, zeroReg, cg);
    Inst_MemReg(OP::MOVDQUMemReg, node, MRef_BISdisp32(destBufferReg, destOffsetReg, 0, headerOffsetConst, cg),
        xmmHighReg, cg);

    // increase src offset by size of imm register
    // increase dest offset by double, to account for the byte->char inflation
    Inst_RegImm(OP::ADD4RegImm4, node, srcOffsetReg, vectorLengthConst, cg);
    Inst_RegImm(OP::ADD4RegImm4, node, destOffsetReg, 2 * vectorLengthConst, cg);

    // LOOP BACK
    Inst_Label(OP::JMP4, node, startLoop, cg);
    Inst_Label(OP::label, node, endLoop, cg);

    // AND length with 15 to compute residual remainder
    // then copy and interleave 8 bytes from src buffer with 0s into dest buffer if possible
    Inst_RegImm(OP::AND4RegImm4, node, lengthReg, vectorLengthConst - 1, cg);

    Inst_RegImm(OP::CMP4RegImm4, node, lengthReg, 8, cg);
    Inst_Label(OP::JL1, node, afterCopy8Label, cg);

    Inst_RegMem(OP::MOVQRegMem, node, xmmLowReg, MRef_BISdisp32(srcBufferReg, srcOffsetReg, 0, headerOffsetConst, cg),
        cg);
    Inst_RegReg(OP::PUNPCKLBWRegReg, node, xmmLowReg, zeroReg, cg);
    Inst_MemReg(OP::MOVDQUMemReg, node, MRef_BISdisp32(destBufferReg, destOffsetReg, 0, headerOffsetConst, cg),
        xmmLowReg, cg);
    Inst_RegImm(OP::SUB4RegImm4, node, lengthReg, 8, cg);

    Inst_RegImm(OP::ADD4RegImm4, node, srcOffsetReg, 8, cg);
    Inst_RegImm(OP::ADD4RegImm4, node, destOffsetReg, 16, cg);

    Inst_Label(OP::label, node, afterCopy8Label, cg);

    // handle residual (< 8 bytes left) & jump to copy instructions based on the number of bytes left
    // calculate how many bytes to skip based on length;

    const int copy_instruction_size = 5 // size of MOVZXReg2Mem1
        + 4; // size of S2MemReg

    // since copy_instruction_size could change depending on which registers are allocated to scratchReg, srcOffsetReg
    // and destOffsetReg we reserve them to be eax, ecx, edx, respectively

    Inst_RegRegImm(OP::IMUL4RegRegImm4, node, lengthReg, lengthReg, -copy_instruction_size, cg);
    Inst_RegImm(OP::ADD4RegImm4, node, lengthReg, copy_instruction_size * 7, cg);

    bool is64bit = cg->comp()->target().is64Bit();
    // calculate address to jump too

    TR::MemoryReference *residueLabelMR = MRef_label(copyResidueLabel, cg);

    if (cg->comp()->target().is64Bit() && residueLabelMR->getAddressRegister()) {
        deps->addPostCondition(residueLabelMR->getAddressRegister(), TR::RealRegister::NoReg, cg);
    }

    Inst_RegMem(OP::LEARegMem(is64bit), node, scratchReg, residueLabelMR, cg);
    Inst_RegReg(OP::ADDRegReg(is64bit), node, lengthReg, scratchReg, cg);

    Inst_RegMem(OP::LEARegMem(is64bit), node, srcOffsetReg, MRef_BISdisp32(srcBufferReg, srcOffsetReg, 0, 0, cg), cg);
    Inst_RegMem(OP::LEARegMem(is64bit), node, destOffsetReg, MRef_BISdisp32(destBufferReg, destOffsetReg, 0, 0, cg),
        cg);

    Inst_Reg(OP::JMPReg, node, lengthReg, cg);

    Inst_Label(OP::label, node, copyResidueLabel, cg);

    for (int i = 0; i < 7; i++) {
        Inst_RegMem(OP::MOVZXReg2Mem1, node, scratchReg, MRef_Bdisp32(srcOffsetReg, headerOffsetConst + 6 - i, cg), cg);
        Inst_MemReg(OP::S2MemReg, node, MRef_Bdisp32(destOffsetReg, headerOffsetConst + 2 * (6 - i), cg), scratchReg,
            cg);
    }

    deps->stopAddingConditions();
    Inst_Label(OP::label, node, doneLabel, deps, cg);
    doneLabel->setEndInternalControlFlow();

    cg->stopUsingRegister(srcOffsetReg);
    cg->stopUsingRegister(destOffsetReg);
    cg->stopUsingRegister(lengthReg);

    cg->stopUsingRegister(xmmHighReg);
    cg->stopUsingRegister(xmmLowReg);
    cg->stopUsingRegister(zeroReg);
    cg->stopUsingRegister(scratchReg);

    for (int i = 0; i < 5; i++) {
        cg->decReferenceCount(node->getChild(i));
    }

    return NULL;
}

TR::Register *J9::X86::TreeEvaluator::encodeUTF16Evaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // tree looks like:
    // icall com.ibm.jit.JITHelpers.encodeUTF16{Big,Little}()
    //    input ptr
    //    output ptr
    //    input length (in elements)
    // Number of elements translated is returned

    TR::MethodSymbol *symbol = node->getSymbol()->castToMethodSymbol();
    bool bigEndian = symbol->getRecognizedMethod() == TR::com_ibm_jit_JITHelpers_transformedEncodeUTF16Big;

    // Set up register dependencies
    const int gprClobberCount = 2;
    const int maxFprClobberCount = 5;
    const int fprClobberCount = bigEndian ? 5 : 4; // xmm4 only needed for big-endian
    TR::Register *srcPtrReg, *dstPtrReg, *lengthReg, *resultReg;
    TR::Register *gprClobbers[gprClobberCount], *fprClobbers[maxFprClobberCount];
    bool killSrc = TR::TreeEvaluator::stopUsingCopyRegAddr(node->getChild(0), srcPtrReg, cg);
    bool killDst = TR::TreeEvaluator::stopUsingCopyRegAddr(node->getChild(1), dstPtrReg, cg);
    bool killLen = TR::TreeEvaluator::stopUsingCopyRegInteger(node->getChild(2), lengthReg, cg);
    resultReg = cg->allocateRegister();
    for (int i = 0; i < gprClobberCount; i++)
        gprClobbers[i] = cg->allocateRegister();
    for (int i = 0; i < fprClobberCount; i++)
        fprClobbers[i] = cg->allocateRegister(TR_FPR);

    int depCount = 11;
    TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, depCount, cg);

    deps->addPostCondition(srcPtrReg, TR::RealRegister::esi, cg);
    deps->addPostCondition(dstPtrReg, TR::RealRegister::edi, cg);
    deps->addPostCondition(lengthReg, TR::RealRegister::edx, cg);
    deps->addPostCondition(resultReg, TR::RealRegister::eax, cg);

    deps->addPostCondition(gprClobbers[0], TR::RealRegister::ecx, cg);
    deps->addPostCondition(gprClobbers[1], TR::RealRegister::ebx, cg);

    deps->addPostCondition(fprClobbers[0], TR::RealRegister::xmm0, cg);
    deps->addPostCondition(fprClobbers[1], TR::RealRegister::xmm1, cg);
    deps->addPostCondition(fprClobbers[2], TR::RealRegister::xmm2, cg);
    deps->addPostCondition(fprClobbers[3], TR::RealRegister::xmm3, cg);
    if (bigEndian)
        deps->addPostCondition(fprClobbers[4], TR::RealRegister::xmm4, cg);

    deps->stopAddingConditions();

    // Generate helper call
    TR_RuntimeHelper helper;
    if (cg->comp()->target().is64Bit())
        helper = bigEndian ? TR_AMD64encodeUTF16Big : TR_AMD64encodeUTF16Little;
    else
        helper = bigEndian ? TR_IA32encodeUTF16Big : TR_IA32encodeUTF16Little;

    Inst_HelperCall(node, helper, deps, cg);

    // Free up registers
    for (int i = 0; i < gprClobberCount; i++)
        cg->stopUsingRegister(gprClobbers[i]);
    for (int i = 0; i < fprClobberCount; i++)
        cg->stopUsingRegister(fprClobbers[i]);

    for (uint16_t i = 0; i < node->getNumChildren(); i++)
        cg->decReferenceCount(node->getChild(i));

    TR_LiveRegisters *liveRegs = cg->getLiveRegisters(TR_GPR);
    if (killSrc)
        liveRegs->registerIsDead(srcPtrReg);
    if (killDst)
        liveRegs->registerIsDead(dstPtrReg);
    if (killLen)
        liveRegs->registerIsDead(lengthReg);

    node->setRegister(resultReg);
    return resultReg;
}

/*
 * The CaseConversionManager is used to store info about the conversion. It defines the lower bound and upper bound
 * value depending on whether it's a toLower or toUpper case conversion. It also chooses byte or word data type
 * depending on whether it's compressed string or not. The stringCaseConversionHelper queries the manager for those info
 * when generating the actual instructions.
 */
class J9::X86::TreeEvaluator::CaseConversionManager {
public:
    CaseConversionManager(bool isCompressedString, bool toLowerCase)
        : _isCompressedString(isCompressedString)
        , _toLowerCase(toLowerCase)
    {
        if (isCompressedString) {
            static uint8_t UPPERCASE_A_ASCII_MINUS1_bytes[] = { 'A' - 1, 'A' - 1, 'A' - 1, 'A' - 1, 'A' - 1, 'A' - 1,
                'A' - 1, 'A' - 1, 'A' - 1, 'A' - 1, 'A' - 1, 'A' - 1, 'A' - 1, 'A' - 1, 'A' - 1, 'A' - 1 };

            static uint8_t UPPERCASE_Z_ASCII_bytes[]
                = { 'Z', 'Z', 'Z', 'Z', 'Z', 'Z', 'Z', 'Z', 'Z', 'Z', 'Z', 'Z', 'Z', 'Z', 'Z', 'Z' };

            static uint8_t LOWERCASE_A_ASCII_MINUS1_bytes[] = { 'a' - 1, 'a' - 1, 'a' - 1, 'a' - 1, 'a' - 1, 'a' - 1,
                'a' - 1, 'a' - 1, 'a' - 1, 'a' - 1, 'a' - 1, 'a' - 1, 'a' - 1, 'a' - 1, 'a' - 1, 'a' - 1 };

            static uint8_t LOWERCASE_Z_ASCII_bytes[] = {
                'z',
                'z',
                'z',
                'z',
                'z',
                'z',
                'z',
                'z',
                'z',
                'z',
                'z',
                'z',
                'z',
                'z',
                'z',
                'z',
            };

            static uint8_t CONVERSION_DIFF_bytes[] = {
                0x20,
                0x20,
                0x20,
                0x20,
                0x20,
                0x20,
                0x20,
                0x20,
                0x20,
                0x20,
                0x20,
                0x20,
                0x20,
                0x20,
                0x20,
                0x20,
            };

            static uint16_t ASCII_UPPERBND_bytes[] = {
                0x7f,
                0x7f,
                0x7f,
                0x7f,
                0x7f,
                0x7f,
                0x7f,
                0x7f,
                0x7f,
                0x7f,
                0x7f,
                0x7f,
                0x7f,
                0x7f,
                0x7f,
                0x7f,
            };

            if (toLowerCase) {
                _lowerBndMinus1 = UPPERCASE_A_ASCII_MINUS1_bytes;
                _upperBnd = UPPERCASE_Z_ASCII_bytes;
            } else {
                _lowerBndMinus1 = LOWERCASE_A_ASCII_MINUS1_bytes;
                _upperBnd = LOWERCASE_Z_ASCII_bytes;
            }
            _conversionDiff = CONVERSION_DIFF_bytes;
            _asciiMax = ASCII_UPPERBND_bytes;
        } else {
            static uint16_t UPPERCASE_A_ASCII_MINUS1_words[]
                = { 'A' - 1, 'A' - 1, 'A' - 1, 'A' - 1, 'A' - 1, 'A' - 1, 'A' - 1, 'A' - 1 };

            static uint16_t LOWERCASE_A_ASCII_MINUS1_words[]
                = { 'a' - 1, 'a' - 1, 'a' - 1, 'a' - 1, 'a' - 1, 'a' - 1, 'a' - 1, 'a' - 1 };

            static uint16_t UPPERCASE_Z_ASCII_words[] = { 'Z', 'Z', 'Z', 'Z', 'Z', 'Z', 'Z', 'Z' };

            static uint16_t LOWERCASE_Z_ASCII_words[] = { 'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z' };

            static uint16_t CONVERSION_DIFF_words[] = { 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20 };
            static uint16_t ASCII_UPPERBND_words[] = { 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f };

            if (toLowerCase) {
                _lowerBndMinus1 = UPPERCASE_A_ASCII_MINUS1_words;
                _upperBnd = UPPERCASE_Z_ASCII_words;
            } else {
                _lowerBndMinus1 = LOWERCASE_A_ASCII_MINUS1_words;
                _upperBnd = LOWERCASE_Z_ASCII_words;
            }
            _conversionDiff = CONVERSION_DIFF_words;
            _asciiMax = ASCII_UPPERBND_words;
        }
    };

    inline bool isCompressedString() { return _isCompressedString; };

    inline bool toLowerCase() { return _toLowerCase; };

    inline void *getLowerBndMinus1() { return _lowerBndMinus1; };

    inline void *getUpperBnd() { return _upperBnd; };

    inline void *getConversionDiff() { return _conversionDiff; };

    inline void *getAsciiMax() { return _asciiMax; };

private:
    void *_lowerBndMinus1;
    void *_upperBnd;
    void *_asciiMax;
    void *_conversionDiff;
    bool _isCompressedString;
    bool _toLowerCase;
};

TR::Register *J9::X86::TreeEvaluator::toUpperIntrinsicLatin1Evaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    CaseConversionManager manager(true /* isCompressedString */, false /* toLowerCase */);
    return TR::TreeEvaluator::stringCaseConversionHelper(node, cg, manager);
}

TR::Register *J9::X86::TreeEvaluator::toLowerIntrinsicLatin1Evaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    CaseConversionManager manager(true /* isCompressedString */, true /* toLowerCase */);
    return TR::TreeEvaluator::stringCaseConversionHelper(node, cg, manager);
}

TR::Register *J9::X86::TreeEvaluator::toUpperIntrinsicUTF16Evaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    CaseConversionManager manager(false /* isCompressedString */, false /* toLowerCase */);
    return TR::TreeEvaluator::stringCaseConversionHelper(node, cg, manager);
}

TR::Register *J9::X86::TreeEvaluator::toLowerIntrinsicUTF16Evaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    CaseConversionManager manager(false /* isCompressedString */, true /* toLowerCase */);
    return TR::TreeEvaluator::stringCaseConversionHelper(node, cg, manager);
}

static TR::Register *allocateRegAndAddCondition(TR::CodeGenerator *cg, TR::RegisterDependencyConditions *deps,
    TR_RegisterKinds rk = TR_GPR)
{
    TR::Register *reg = cg->allocateRegister(rk);
    deps->addPostCondition(reg, TR::RealRegister::NoReg, cg);
    deps->addPreCondition(reg, TR::RealRegister::NoReg, cg);
    return reg;
}

/**
 * \brief This evaluator is used to perform string toUpper and toLower conversion.
 *
 * This JIT HW optimized conversion helper is designed to convert strings that contains only ascii characters.
 * If a string contains non ascii characters, HW optimized routine will return NULL and fall back to the software
 * implementation, which is able to convert a broader range of characters.
 *
 * There are the following steps in the generated assembly code:
 *   1. preparation (load value into register, calculate length etc)
 *   2. vectorized case conversion loop
 *   3. handle residue with non vectorized case conversion loop
 *   4. handle invalid case
 *
 * \param node
 * \param cg
 * \param manager Contains info about the conversion: whether it's toUpper or toLower conversion, the valid range of
 * characters, etc
 *
 * This version does not support discontiguous arrays
 */
TR::Register *J9::X86::TreeEvaluator::stringCaseConversionHelper(TR::Node *node, TR::CodeGenerator *cg,
    CaseConversionManager &manager)
{
#define iComment(str) \
    if (debug)        \
        debug->addInstructionComment(cursor, (const_cast<char *>(str)));
    TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)14, (uint8_t)14, cg);
    TR::Register *srcArray = cg->evaluate(node->getChild(1));
    deps->addPostCondition(srcArray, TR::RealRegister::NoReg, cg);
    deps->addPreCondition(srcArray, TR::RealRegister::NoReg, cg);

    TR::Register *dstArray = cg->evaluate(node->getChild(2));
    deps->addPostCondition(dstArray, TR::RealRegister::NoReg, cg);
    deps->addPreCondition(dstArray, TR::RealRegister::NoReg, cg);

    TR::Register *length = cg->intClobberEvaluate(node->getChild(3));
    deps->addPostCondition(length, TR::RealRegister::NoReg, cg);
    deps->addPreCondition(length, TR::RealRegister::NoReg, cg);

    TR::Register *counter = allocateRegAndAddCondition(cg, deps);
    TR::Register *residueStartLength = allocateRegAndAddCondition(cg, deps);
    TR::Register *singleChar
        = residueStartLength; // residueStartLength and singleChar do not overlap and can share the same register
    TR::Register *result = allocateRegAndAddCondition(cg, deps);

    TR::Register *xmmRegLowerBndMinus1
        = allocateRegAndAddCondition(cg, deps, TR_FPR); // 'A-1' for toLowerCase, 'a-1' for toUpperCase
    TR::Register *xmmRegUpperBnd
        = allocateRegAndAddCondition(cg, deps, TR_FPR); // 'Z-1' for toLowerCase, 'z-1' for toUpperCase
    TR::Register *xmmRegConversionDiff = allocateRegAndAddCondition(cg, deps, TR_FPR);
    TR::Register *xmmRegMinus1 = allocateRegAndAddCondition(cg, deps, TR_FPR);
    TR::Register *xmmRegAsciiUpperBnd = allocateRegAndAddCondition(cg, deps, TR_FPR);
    TR::Register *xmmRegArrayContentCopy0 = allocateRegAndAddCondition(cg, deps, TR_FPR);
    TR::Register *xmmRegArrayContentCopy1 = allocateRegAndAddCondition(cg, deps, TR_FPR);
    TR::Register *xmmRegArrayContentCopy2 = allocateRegAndAddCondition(cg, deps, TR_FPR);
    TR_Debug *debug = cg->getDebug();
    TR::Instruction *cursor = NULL;

    uint32_t strideSize = 16;
    uintptr_t headerSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();

    static uint16_t MINUS1[] = {
        0xffff,
        0xffff,
        0xffff,
        0xffff,
        0xffff,
        0xffff,
        0xffff,
        0xffff,
    };

    TR::LabelSymbol *failLabel = generateLabelSymbol(cg);
    // Under decompressed string case for 32bits platforms, bail out if string is larger than INT_MAX32/2 since #
    // character to # byte conversion will cause overflow.
    if (!cg->comp()->target().is64Bit() && !manager.isCompressedString()) {
        Inst_RegImm(OP::CMPRegImm4(), node, length, (uint16_t)0x8000, cg);
        Inst_Label(OP::JGE4, node, failLabel, cg);
    }

    // 1. preparation (load value into registers, calculate length etc)
    auto lowerBndMinus1 = MRef_const(cg->findOrCreate16ByteConstant(node, manager.getLowerBndMinus1()), cg);
    cursor = Inst_RegMem(OP::MOVDQURegMem, node, xmmRegLowerBndMinus1, lowerBndMinus1, cg);
    iComment("lower bound ascii value minus one");

    auto upperBnd = MRef_const(cg->findOrCreate16ByteConstant(node, manager.getUpperBnd()), cg);
    cursor = Inst_RegMem(OP::MOVDQURegMem, node, xmmRegUpperBnd, upperBnd, cg);
    iComment("upper bound ascii value");

    auto conversionDiff = MRef_const(cg->findOrCreate16ByteConstant(node, manager.getConversionDiff()), cg);
    cursor = Inst_RegMem(OP::MOVDQURegMem, node, xmmRegConversionDiff, conversionDiff, cg);
    iComment("case conversion diff value");

    auto minus1 = MRef_const(cg->findOrCreate16ByteConstant(node, MINUS1), cg);
    cursor = Inst_RegMem(OP::MOVDQURegMem, node, xmmRegMinus1, minus1, cg);
    iComment("-1");

    auto asciiUpperBnd = MRef_const(cg->findOrCreate16ByteConstant(node, manager.getAsciiMax()), cg);
    cursor = Inst_RegMem(OP::MOVDQURegMem, node, xmmRegAsciiUpperBnd, asciiUpperBnd, cg);
    iComment("maximum ascii value ");

    Inst_RegImm(OP::MOV4RegImm4, node, result, 1, cg);

    // initialize the loop counter
    cursor = Inst_RegReg(OP::XOR4RegReg, node, counter, counter, cg);
    iComment("initialize loop counter");

    // calculate the residueStartLength. Later instructions compare the counter with this length and decide when to jump
    // to the residue handling sequence
    Inst_RegReg(OP::MOVRegReg(), node, residueStartLength, length, cg);
    Inst_RegImm(OP::SUBRegImms(), node, residueStartLength, strideSize - 1, cg);

    // 2. vectorized case conversion loop
    TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *residueStartLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *storeToArrayLabel = generateLabelSymbol(cg);

    startLabel->setStartInternalControlFlow();
    endLabel->setEndInternalControlFlow();
    Inst_Label(OP::label, node, startLabel, cg);

    TR::LabelSymbol *caseConversionMainLoopLabel = generateLabelSymbol(cg);
    Inst_Label(OP::label, node, caseConversionMainLoopLabel, cg);
    Inst_RegReg(OP::CMPRegReg(), node, counter, residueStartLength, cg);
    Inst_Label(OP::JGE4, node, residueStartLabel, cg);

    auto srcArrayMemRef = MRef_BISdisp32(srcArray, counter, 0, headerSize, cg);
    Inst_RegMem(OP::MOVDQURegMem, node, xmmRegArrayContentCopy0, srcArrayMemRef, cg);

    // detect invalid characters
    Inst_RegReg(OP::MOVDQURegReg, node, xmmRegArrayContentCopy1, xmmRegArrayContentCopy0, cg);
    Inst_RegReg(OP::MOVDQURegReg, node, xmmRegArrayContentCopy2, xmmRegArrayContentCopy0, cg);
    cursor = Inst_RegReg(manager.isCompressedString() ? OP::PCMPGTBRegReg : OP::PCMPGTWRegReg, node,
        xmmRegArrayContentCopy1, xmmRegMinus1, cg);
    iComment(" > -1");
    cursor = Inst_RegReg(manager.isCompressedString() ? OP::PCMPGTBRegReg : OP::PCMPGTWRegReg, node,
        xmmRegArrayContentCopy2, xmmRegAsciiUpperBnd, cg);
    iComment(" > maximum ascii value");
    cursor = Inst_RegReg(OP::PANDNRegReg, node, xmmRegArrayContentCopy2, xmmRegArrayContentCopy1, cg);
    iComment(" >-1 && !(> maximum ascii value) valid when all bits are set");
    cursor = Inst_RegReg(OP::PXORRegReg, node, xmmRegArrayContentCopy2, xmmRegMinus1, cg);
    iComment("reverse all bits");
    Inst_RegReg(OP::PTESTRegReg, node, xmmRegArrayContentCopy2, xmmRegArrayContentCopy2, cg);
    Inst_Label(OP::JNE4, node, failLabel, cg);
    iComment("jump out if invalid chars are detected");

    // calculate case conversion with vector registers
    Inst_RegReg(OP::MOVDQURegReg, node, xmmRegArrayContentCopy1, xmmRegArrayContentCopy0, cg);
    Inst_RegReg(OP::MOVDQURegReg, node, xmmRegArrayContentCopy2, xmmRegArrayContentCopy0, cg);
    cursor = Inst_RegReg(manager.isCompressedString() ? OP::PCMPGTBRegReg : OP::PCMPGTWRegReg, node,
        xmmRegArrayContentCopy0, xmmRegLowerBndMinus1, cg);
    iComment(manager.toLowerCase() ? " > 'A-1'" : "> 'a-1'");
    cursor = Inst_RegReg(manager.isCompressedString() ? OP::PCMPGTBRegReg : OP::PCMPGTWRegReg, node,
        xmmRegArrayContentCopy1, xmmRegUpperBnd, cg);
    iComment(manager.toLowerCase() ? " > 'Z'" : " > 'z'");
    cursor = Inst_RegReg(OP::PANDNRegReg, node, xmmRegArrayContentCopy1, xmmRegArrayContentCopy0, cg);
    iComment(const_cast<char *>(manager.toLowerCase() ? " >='A' && !( >'Z')" : " >='a' && !( >'z')"));
    Inst_RegReg(OP::PANDRegReg, node, xmmRegArrayContentCopy1, xmmRegConversionDiff, cg);

    if (manager.toLowerCase())
        Inst_RegReg(manager.isCompressedString() ? OP::PADDBRegReg : OP::PADDWRegReg, node, xmmRegArrayContentCopy2,
            xmmRegArrayContentCopy1, cg);
    else
        Inst_RegReg(manager.isCompressedString() ? OP::PSUBBRegReg : OP::PSUBWRegReg, node, xmmRegArrayContentCopy2,
            xmmRegArrayContentCopy1, cg);

    auto dstArrayMemRef = MRef_BISdisp32(dstArray, counter, 0, headerSize, cg);
    Inst_MemReg(OP::MOVDQUMemReg, node, dstArrayMemRef, xmmRegArrayContentCopy2, cg);
    Inst_RegImm(OP::ADDRegImms(), node, counter, strideSize, cg);
    Inst_Label(OP::JMP4, node, caseConversionMainLoopLabel, cg);

    // 3. handle residue with non vectorized case conversion loop
    Inst_Label(OP::label, node, residueStartLabel, cg);
    Inst_RegReg(OP::CMPRegReg(), node, counter, length, cg);
    Inst_Label(OP::JGE4, node, endLabel, cg);
    srcArrayMemRef = MRef_BISdisp32(srcArray, counter, 0, headerSize, cg);
    Inst_RegMem(manager.isCompressedString() ? OP::MOVZXReg4Mem1 : OP::MOVZXReg4Mem2, node, singleChar, srcArrayMemRef,
        cg);

    // use unsigned compare to detect invalid range
    Inst_RegImm(OP::CMP4RegImms, node, singleChar, 0x7F, cg);
    Inst_Label(OP::JA4, node, failLabel, cg);

    Inst_RegImm(OP::CMP4RegImms, node, singleChar, manager.toLowerCase() ? 'A' : 'a', cg);
    Inst_Label(OP::JB4, node, storeToArrayLabel, cg);

    Inst_RegImm(OP::CMP4RegImms, node, singleChar, manager.toLowerCase() ? 'Z' : 'z', cg);
    Inst_Label(OP::JA4, node, storeToArrayLabel, cg);

    if (manager.toLowerCase())
        Inst_RegMem(OP::LEARegMem(), node, singleChar, MRef_Bdisp32(singleChar, 0x20, cg), cg);

    else
        Inst_RegImm(OP::SUB4RegImms, node, singleChar, 0x20, cg);

    Inst_Label(OP::label, node, storeToArrayLabel, cg);

    dstArrayMemRef = MRef_BISdisp32(dstArray, counter, 0, headerSize, cg);
    Inst_MemReg(manager.isCompressedString() ? OP::S1MemReg : OP::S2MemReg, node, dstArrayMemRef, singleChar, cg);
    Inst_RegImm(OP::ADDRegImms(), node, counter, manager.isCompressedString() ? 1 : 2, cg);
    Inst_Label(OP::JMP4, node, residueStartLabel, cg);

    // 4. handle invalid case
    Inst_Label(OP::label, node, failLabel, cg);
    Inst_RegReg(OP::XOR4RegReg, node, result, result, cg);

    Inst_Label(OP::label, node, endLabel, deps, cg);
    node->setRegister(result);

    cg->stopUsingRegister(length);
    cg->stopUsingRegister(counter);
    cg->stopUsingRegister(residueStartLength);

    cg->stopUsingRegister(xmmRegLowerBndMinus1);
    cg->stopUsingRegister(xmmRegUpperBnd);
    cg->stopUsingRegister(xmmRegConversionDiff);
    cg->stopUsingRegister(xmmRegMinus1);
    cg->stopUsingRegister(xmmRegAsciiUpperBnd);
    cg->stopUsingRegister(xmmRegArrayContentCopy0);
    cg->stopUsingRegister(xmmRegArrayContentCopy1);
    cg->stopUsingRegister(xmmRegArrayContentCopy2);

    cg->decReferenceCount(node->getChild(0));
    cg->decReferenceCount(node->getChild(1));
    cg->decReferenceCount(node->getChild(2));
    cg->decReferenceCount(node->getChild(3));
    return result;
}

/*
 *
 * Generates instructions to fill in the J9JITWatchedStaticFieldData.fieldAddress,
 * J9JITWatchedStaticFieldData.fieldClass for static fields, and J9JITWatchedInstanceFieldData.offset for instance
 * fields at runtime. Used for fieldwatch support. Fill in the J9JITWatchedStaticFieldData.fieldAddress,
 * J9JITWatchedStaticFieldData.fieldClass for static field and J9JITWatchedInstanceFieldData.offset for instance field
 *
 * cmp J9JITWatchedStaticFieldData.fieldAddress / J9JITWatchedInstanceFieldData.offset,  -1
 * je unresolvedLabel
 * restart Label:
 * ....
 *
 * unresolvedLabel:
 * mov J9JITWatchedStaticFieldData.fieldClass J9Class (static field only)
 * call helper
 * mov J9JITWatchedStaticFieldData.fieldAddress / J9JITWatchedInstanceFieldData.offset  resultReg
 * jmp restartLabel
 */
void J9::X86::TreeEvaluator::generateFillInDataBlockSequenceForUnresolvedField(TR::CodeGenerator *cg, TR::Node *node,
    TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister, TR::Register *dataSnippetRegister)
{
    TR::Compilation *comp = cg->comp();
    TR::SymbolReference *symRef = node->getSymbolReference();
    bool is64Bit = comp->target().is64Bit();
    bool isStatic = symRef->getSymbol()->getKind() == TR::Symbol::IsStatic;
    TR_RuntimeHelper helperIndex = isWrite
        ? (isStatic ? TR_jitResolveStaticFieldSetterDirect : TR_jitResolveFieldSetterDirect)
        : (isStatic ? TR_jitResolveStaticFieldDirect : TR_jitResolveFieldDirect);
    TR::Linkage *linkage = cg->getLinkage(runtimeHelperLinkage(helperIndex));
    auto linkageProperties = linkage->getProperties();
    intptr_t offsetInDataBlock = isStatic ? offsetof(J9JITWatchedStaticFieldData, fieldAddress)
                                          : offsetof(J9JITWatchedInstanceFieldData, offset);

    TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *unresolveLabel = generateLabelSymbol(cg);
    startLabel->setStartInternalControlFlow();
    endLabel->setEndInternalControlFlow();

    // 64bit needs 2 argument registers (return register and first argument are the same),
    // 32bit only one return register
    // both 64/32bits need dataBlockReg
    uint8_t numOfConditions = is64Bit ? 3 : 2;
    if (isStatic) // needs fieldClassReg
    {
        numOfConditions++;
    }
    TR::RegisterDependencyConditions *deps = RegDeps(numOfConditions, numOfConditions, cg);
    TR::Register *resultReg = NULL;
    TR::Register *dataBlockReg = cg->allocateRegister();
    deps->addPreCondition(dataBlockReg, TR::RealRegister::NoReg, cg);
    deps->addPostCondition(dataBlockReg, TR::RealRegister::NoReg, cg);

    Inst_Label(OP::label, node, startLabel, cg);
    Inst_RegMem(OP::LEARegMem(), node, dataBlockReg, MRef_label(dataSnippet->getSnippetLabel(), cg), cg);
    Inst_MemImm(OP::CMPMemImms(), node, MRef_Bdisp32(dataBlockReg, offsetInDataBlock, cg), -1, cg);
    Inst_Label(OP::JE4, node, unresolveLabel, cg);

    {
        TR_OutlinedInstructionsGenerator og(unresolveLabel, node, cg);
        if (isStatic) {
            // Fills in J9JITWatchedStaticFieldData.fieldClass
            TR::Register *fieldClassReg;
            if (isWrite) {
                fieldClassReg = cg->allocateRegister();
                Inst_RegMem(OP::LRegMem(), node, fieldClassReg,
                    MRef_Bdisp32(sideEffectRegister, comp->fej9()->getOffsetOfClassFromJavaLangClassField(), cg), cg);
            } else {
                fieldClassReg = sideEffectRegister;
            }
            Inst_MemReg(OP::SMemReg(is64Bit), node,
                MRef_Bdisp32(dataBlockReg, (intptr_t)(offsetof(J9JITWatchedStaticFieldData, fieldClass)), cg),
                fieldClassReg, cg);
            deps->addPreCondition(fieldClassReg, TR::RealRegister::NoReg, cg);
            deps->addPostCondition(fieldClassReg, TR::RealRegister::NoReg, cg);
            if (isWrite) {
                cg->stopUsingRegister(fieldClassReg);
            }
        }

        TR::ResolvedMethodSymbol *methodSymbol = node->getByteCodeInfo().getCallerIndex() == -1
            ? comp->getMethodSymbol()
            : comp->getInlinedResolvedMethodSymbol(node->getByteCodeInfo().getCallerIndex());
        if (is64Bit) {
            TR::Register *cpAddressReg = cg->allocateRegister();
            TR::Register *cpIndexReg = cg->allocateRegister();
            Inst_RegImm64Sym(OP::MOV8RegImm64, node, cpAddressReg,
                (uintptr_t)methodSymbol->getResolvedMethod()->constantPool(),
                comp->getSymRefTab()->findOrCreateConstantPoolAddressSymbolRef(methodSymbol), cg);
            Inst_RegImm(OP::MOV8RegImm4, node, cpIndexReg, symRef->getCPIndex(), cg);
            deps->addPreCondition(cpAddressReg, linkageProperties.getArgumentRegister(0, false /* isFloat */), cg);
            deps->addPostCondition(cpAddressReg, linkageProperties.getArgumentRegister(0, false /* isFloat */), cg);
            deps->addPreCondition(cpIndexReg, linkageProperties.getArgumentRegister(1, false /* isFloat */), cg);
            deps->addPostCondition(cpIndexReg, linkageProperties.getArgumentRegister(1, false /* isFloat */), cg);
            cg->stopUsingRegister(cpIndexReg);
            resultReg
                = cpAddressReg; // for 64bit private linkage both the first argument reg and the return reg are rax
        } else {
            Inst_Imm(OP::PUSHImm4, node, symRef->getCPIndex(), cg);
            Inst_ImmSym(OP::PUSHImm4, node, (uintptr_t)methodSymbol->getResolvedMethod()->constantPool(),
                comp->getSymRefTab()->findOrCreateConstantPoolAddressSymbolRef(methodSymbol), cg);
            resultReg = cg->allocateRegister();
            deps->addPreCondition(resultReg, linkageProperties.getIntegerReturnRegister(), cg);
            deps->addPostCondition(resultReg, linkageProperties.getIntegerReturnRegister(), cg);
        }
        TR::Instruction *call = Inst_HelperCall(node, helperIndex, NULL, cg);
        call->setNeedsGCMap(0xFF00FFFF);

        /*
        For instance field offset, the result returned by the vmhelper includes header size.
        subtract the header size to get the offset needed by field watch helpers
        */
        if (!isStatic) {
            Inst_RegImm(OP::SubRegImm4(is64Bit, false /*isWithBorrow*/), node, resultReg,
                TR::Compiler->om.objectHeaderSizeInBytes(), cg);
        }

        // store result into J9JITWatchedStaticFieldData.fieldAddress / J9JITWatchedInstanceFieldData.offset
        Inst_MemReg(OP::SMemReg(is64Bit), node, MRef_Bdisp32(dataBlockReg, offsetInDataBlock, cg), resultReg, cg);
        Inst_Label(OP::JMP4, node, endLabel, cg);

        og.endOutlinedInstructionSequence();
    }

    deps->stopAddingConditions();
    Inst_Label(OP::label, node, endLabel, deps, cg);
    cg->stopUsingRegister(dataBlockReg);
    cg->stopUsingRegister(resultReg);
}

/*
 * Generate the reporting field access helper call with required arguments
 *
 * jitReportInstanceFieldRead
 * arg1 pointer to static data block
 * arg2 object being read
 *
 * jitReportInstanceFieldWrite
 * arg1 pointer to static data block
 * arg2 object being written to (represented by sideEffectRegister)
 * arg3 pointer to value being written
 *
 * jitReportStaticFieldRead
 * arg1 pointer to static data block
 *
 * jitReportStaticFieldWrite
 * arg1 pointer to static data block
 * arg2 pointer to value being written
 *
 */
void generateReportFieldAccessOutlinedInstructions(TR::Node *node, TR::LabelSymbol *endLabel, TR::Snippet *dataSnippet,
    bool isWrite, TR::RegisterDependencyConditions *deps, TR::CodeGenerator *cg, TR::Register *sideEffectRegister,
    TR::Register *valueReg)
{
    bool is64Bit = cg->comp()->target().is64Bit();
    bool isInstanceField = node->getSymbolReference()->getSymbol()->getKind() != TR::Symbol::IsStatic;
    J9Method *owningMethod = (J9Method *)node->getOwningMethod();

    TR_RuntimeHelper helperIndex = isWrite
        ? (isInstanceField ? TR_jitReportInstanceFieldWrite : TR_jitReportStaticFieldWrite)
        : (isInstanceField ? TR_jitReportInstanceFieldRead : TR_jitReportStaticFieldRead);

    TR::Linkage *linkage = cg->getLinkage(runtimeHelperLinkage(helperIndex));
    auto linkageProperties = linkage->getProperties();

    TR::Register *valueReferenceReg = NULL;
    TR::MemoryReference *valueMR = NULL;
    TR::Register *dataBlockReg = cg->allocateRegister();
    bool reuseValueReg = false;

    /*
     * For reporting field write, reference to the valueNode (valueNode is evaluated in valueReg) is needed so we need
     * to store the value on to a stack location first and pass the stack location address as an arguement to the VM
     * helper
     */
    if (isWrite) {
        valueMR = cg->machine()->getDummyLocalMR(node->getType());
        if (!valueReg->getRegisterPair()) {
            if (valueReg->getKind() == TR_GPR) {
                TR::AutomaticSymbol *autoSymbol = valueMR->getSymbolReference().getSymbol()->getAutoSymbol();
                Inst_MemReg(OP::SMemReg(autoSymbol->getRoundedSize() == 8), node, valueMR, valueReg, cg);
            } else if (valueReg->isSinglePrecision())
                Inst_MemReg(OP::MOVSSMemReg, node, valueMR, valueReg, cg);
            else
                Inst_MemReg(OP::MOVSDMemReg, node, valueMR, valueReg, cg);
            // valueReg and valueReferenceReg are different. Add conditions for valueReg here
            deps->addPreCondition(valueReg, TR::RealRegister::NoReg, cg);
            deps->addPostCondition(valueReg, TR::RealRegister::NoReg, cg);
            valueReferenceReg = cg->allocateRegister();
        } else { // 32bit long
            Inst_MemReg(OP::SMemReg(), node, valueMR, valueReg->getLowOrder(), cg);
            Inst_MemReg(OP::SMemReg(), node, MRef_MRefOff(*valueMR, 4, cg), valueReg->getHighOrder(), cg);

            // Add the dependency for higher half register here
            deps->addPostCondition(valueReg->getHighOrder(), TR::RealRegister::NoReg, cg);
            deps->addPreCondition(valueReg->getHighOrder(), TR::RealRegister::NoReg, cg);

            // on 32bit reuse lower half register to save one register
            // lower half register dependency will be added later when using as valueReferenceReg and a call argument
            // to keep consistency with the other call arguments
            valueReferenceReg = valueReg->getLowOrder();
            reuseValueReg = true;
        }

        // store the stack location into a register
        Inst_RegMem(OP::LEARegMem(), node, valueReferenceReg, valueMR, cg);
    }

    Inst_RegMem(OP::LEARegMem(), node, dataBlockReg, MRef_label(dataSnippet->getSnippetLabel(), cg), cg);
    int numArgs = 0;
    if (is64Bit) {
        deps->addPreCondition(dataBlockReg, linkageProperties.getArgumentRegister(numArgs, false /* isFloat */), cg);
        deps->addPostCondition(dataBlockReg, linkageProperties.getArgumentRegister(numArgs, false /* isFloat */), cg);
        numArgs++;

        if (isInstanceField) {
            deps->addPreCondition(sideEffectRegister,
                linkageProperties.getArgumentRegister(numArgs, false /* isFloat */), cg);
            deps->addPostCondition(sideEffectRegister,
                linkageProperties.getArgumentRegister(numArgs, false /* isFloat */), cg);
            numArgs++;
        }

        if (isWrite) {
            deps->addPreCondition(valueReferenceReg,
                linkageProperties.getArgumentRegister(numArgs, false /* isFloat */), cg);
            deps->addPostCondition(valueReferenceReg,
                linkageProperties.getArgumentRegister(numArgs, false /* isFloat */), cg);
        }
    } else {
        if (isWrite) {
            Inst_Reg(OP::PUSHReg, node, valueReferenceReg, cg);
            deps->addPostCondition(valueReferenceReg, TR::RealRegister::NoReg, cg);
            deps->addPreCondition(valueReferenceReg, TR::RealRegister::NoReg, cg);
        }

        if (isInstanceField) {
            Inst_Reg(OP::PUSHReg, node, sideEffectRegister, cg);
            deps->addPreCondition(sideEffectRegister, TR::RealRegister::NoReg, cg);
            deps->addPostCondition(sideEffectRegister, TR::RealRegister::NoReg, cg);
        }
        Inst_Reg(OP::PUSHReg, node, dataBlockReg, cg);
        deps->addPreCondition(dataBlockReg, TR::RealRegister::NoReg, cg);
        deps->addPostCondition(dataBlockReg, TR::RealRegister::NoReg, cg);
    }

    TR::Instruction *call = Inst_HelperCall(node, helperIndex, NULL, cg);
    call->setNeedsGCMap(0xFF00FFFF);
    // Restore the value of lower part register
    if (isWrite && valueReg->getRegisterPair() && valueReg->getKind() == TR_GPR)
        Inst_RegMem(OP::L4RegMem, node, valueReg->getLowOrder(), valueMR, cg);
    if (!reuseValueReg)
        cg->stopUsingRegister(valueReferenceReg);
    Inst_Label(OP::JMP4, node, endLabel, cg);
    cg->stopUsingRegister(dataBlockReg);
}

/*
 * Get the number of register dependencies needed to generate the out-of-line sequence reporting field accesses
 */
static uint8_t getNumOfConditionsForReportFieldAccess(TR::Node *node, bool isResolved, bool isWrite,
    bool isInstanceField, TR::CodeGenerator *cg)
{
    uint8_t numOfConditions = 1; // 1st arg is always the data block
    if (!isResolved || isInstanceField || cg->needClassAndMethodPointerRelocations())
        numOfConditions = numOfConditions + 1; // classReg is needed in both cases.
    if (isWrite) {
        /* Field write report needs
         * a) value being written
         * b) the reference to the value being written
         *
         * The following cases are considered
         * 1. For 32bits using register pair(long), the valueReg is actually 2 registers,
         *    and valueReferenceReg reuses one reg in valueReg to avoid running out of registers on 32bits
         * 2. For 32bits and 64bits no register pair, valueReferenceReg and valueReg are 2 different registers
         */
        numOfConditions = numOfConditions + 2;
    }
    if (isInstanceField)
        numOfConditions = numOfConditions + 1; // Instance field report needs the base object
    return numOfConditions;
}

void J9::X86::TreeEvaluator::generateTestAndReportFieldWatchInstructions(TR::CodeGenerator *cg, TR::Node *node,
    TR::Snippet *dataSnippet, bool isWrite, TR::Register *sideEffectRegister, TR::Register *valueReg,
    TR::Register *dataSnippetRegister)
{
    bool isResolved = !node->getSymbolReference()->isUnresolved();
    TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *fieldReportLabel = generateLabelSymbol(cg);
    startLabel->setStartInternalControlFlow();
    endLabel->setEndInternalControlFlow();

    Inst_Label(OP::label, node, startLabel, cg);

    TR::Register *fieldClassReg = NULL;
    TR::MemoryReference *classFlagsMemRef = NULL;
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
    bool isInstanceField = node->getOpCode().isIndirect();
    bool fieldClassNeedsRelocation = cg->needClassAndMethodPointerRelocations();

    if (isInstanceField) {
        TR::Register *objReg = sideEffectRegister;
        fieldClassReg = cg->allocateRegister();
        generateLoadJ9Class(node, fieldClassReg, objReg, cg);
        classFlagsMemRef = MRef_Bdisp32(fieldClassReg, (uintptr_t)(fej9->getOffsetOfClassFlags()), cg);
    } else {
        if (isResolved) {
            if (!fieldClassNeedsRelocation) {
                // For non-AOT (JIT and JITServer) compiles we don't need to use sideEffectRegister here as the class
                // information is available to us at compile time.
                J9Class *fieldClass = static_cast<TR::J9WatchedStaticFieldSnippet *>(dataSnippet)->getFieldClass();
                classFlagsMemRef = MRef_abs((uintptr_t)fieldClass + fej9->getOffsetOfClassFlags(), cg);
            } else {
                // If this is an AOT compile, we generate instructions to load the fieldClass directly from the snippet
                // because the fieldClass in an AOT body will be invalid if we load using the dataSnippet's helper query
                // at compile time.
                fieldClassReg = cg->allocateRegister();
                Inst_RegMem(OP::LEARegMem(), node, fieldClassReg, MRef_label(dataSnippet->getSnippetLabel(), cg), cg);
                Inst_RegMem(OP::LRegMem(), node, fieldClassReg,
                    MRef_Bdisp32(fieldClassReg, offsetof(J9JITWatchedStaticFieldData, fieldClass), cg), cg);
                classFlagsMemRef = MRef_Bdisp32(fieldClassReg, fej9->getOffsetOfClassFlags(), cg);
            }
        } else {
            if (isWrite) {
                fieldClassReg = cg->allocateRegister();
                Inst_RegMem(OP::LRegMem(), node, fieldClassReg,
                    MRef_Bdisp32(sideEffectRegister, fej9->getOffsetOfClassFromJavaLangClassField(), cg), cg);
            } else {
                fieldClassReg = sideEffectRegister;
            }
            classFlagsMemRef = MRef_Bdisp32(fieldClassReg, fej9->getOffsetOfClassFlags(), cg);
        }
    }

    Inst_MemImm(OP::TEST2MemImm2, node, classFlagsMemRef, J9ClassHasWatchedFields, cg);
    Inst_Label(OP::JNE4, node, fieldReportLabel, cg);

    uint8_t numOfConditions = getNumOfConditionsForReportFieldAccess(node, !node->getSymbolReference()->isUnresolved(),
        isWrite, isInstanceField, cg);
    TR::RegisterDependencyConditions *deps = RegDeps(numOfConditions, numOfConditions, cg);
    if (isInstanceField || !isResolved || fieldClassNeedsRelocation) {
        deps->addPreCondition(fieldClassReg, TR::RealRegister::NoReg, cg);
        deps->addPostCondition(fieldClassReg, TR::RealRegister::NoReg, cg);
    }

    {
        TR_OutlinedInstructionsGenerator og(fieldReportLabel, node, cg);
        generateReportFieldAccessOutlinedInstructions(node, endLabel, dataSnippet, isWrite, deps, cg,
            sideEffectRegister, valueReg);
        og.endOutlinedInstructionSequence();
    }
    deps->stopAddingConditions();
    Inst_Label(OP::label, node, endLabel, deps, cg);

    if (isInstanceField || (!isResolved && isWrite) || fieldClassNeedsRelocation) {
        cg->stopUsingRegister(fieldClassReg);
    }
}

TR::Register *J9::X86::TreeEvaluator::generateConcurrentScavengeSequence(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Register *object = TR::TreeEvaluator::performHeapLoadWithReadBarrier(node, cg);

    if (!node->getSymbolReference()->isUnresolved()
        && (node->getSymbolReference()->getSymbol()->getKind() == TR::Symbol::IsShadow)
        && (node->getSymbolReference()->getCPIndex() >= 0) && (cg->comp()->getMethodHotness() >= scorching)) {
        int32_t len;
        const char *fieldName = node->getSymbolReference()
                                    ->getOwningMethod(cg->comp())
                                    ->fieldSignatureChars(node->getSymbolReference()->getCPIndex(), len);

        if (fieldName && strstr(fieldName, "Ljava/lang/String;")) {
            Inst_Mem(OP::PREFETCHT0, node, MRef_Bdisp32(object, 0, cg), cg);
        }
    }
    return object;
}

TR::Register *J9::X86::TreeEvaluator::irdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // For rdbar and wrtbar nodes we first evaluate the children we need to
    // handle the side effects. Then we delegate the evaluation of the remaining
    // children and the load/store operation to the appropriate load/store evaluator.
    TR::Node *sideEffectNode = node->getFirstChild();
    TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

    if (cg->comp()->getOption(TR_EnableFieldWatch)) {
        TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
    }

    TR::Register *resultReg = NULL;
    // Perform regular load if no rdbar required.
    if (TR::Compiler->om.readBarrierType() == gc_modron_readbar_none) {
        resultReg = TR::TreeEvaluator::iloadEvaluator(node, cg);
    } else {
        if (cg->comp()->useCompressedPointers()
            && (node->getOpCode().hasSymbolReference()
                && node->getSymbolReference()->getSymbol()->getDataType() == TR::Address)) {
            resultReg = TR::TreeEvaluator::generateConcurrentScavengeSequence(node, cg);
            node->setRegister(resultReg);
        }
    }

    // Note: For indirect rdbar nodes, the first child (sideEffectNode) is also used by the
    // load evaluator. The load evaluator will also evaluate+decrement it. In order to avoid double
    // decrementing the node we skip doing it here and let the load evaluator do it.
    return resultReg;
}

TR::Register *J9::X86::TreeEvaluator::irdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // For rdbar and wrtbar nodes we first evaluate the children we need to
    // handle the side effects. Then we delegate the evaluation of the remaining
    // children and the load/store operation to the appropriate load/store evaluator.
    TR::Node *sideEffectNode = node->getFirstChild();
    TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

    if (cg->comp()->getOption(TR_EnableFieldWatch)) {
        TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
    }

    cg->decReferenceCount(sideEffectNode);
    return TR::TreeEvaluator::iloadEvaluator(node, cg);
}

TR::Register *J9::X86::TreeEvaluator::ardbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // For rdbar and wrtbar nodes we first evaluate the children we need to
    // handle the side effects. Then we delegate the evaluation of the remaining
    // children and the load/store operation to the appropriate load/store evaluator.
    TR::Node *sideEffectNode = node->getFirstChild();
    TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

    if (cg->comp()->getOption(TR_EnableFieldWatch)) {
        TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
    }

    cg->decReferenceCount(sideEffectNode);
    return TR::TreeEvaluator::aloadEvaluator(node, cg);
}

TR::Register *J9::X86::TreeEvaluator::ardbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // For rdbar and wrtbar nodes we first evaluate the children we need to
    // handle the side effects. Then we delegate the evaluation of the remaining
    // children and the load/store operation to the appropriate load/store evaluator.
    TR::Register *sideEffectRegister = cg->evaluate(node->getFirstChild());

    if (cg->comp()->getOption(TR_EnableFieldWatch)) {
        TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, NULL);
    }

    TR::Register *resultReg = NULL;
    // Perform regular load if no rdbar required.
    if (TR::Compiler->om.readBarrierType() == gc_modron_readbar_none) {
        resultReg = TR::TreeEvaluator::aloadEvaluator(node, cg);
    } else {
        resultReg = TR::TreeEvaluator::generateConcurrentScavengeSequence(node, cg);
        resultReg->setContainsCollectedReference();
        node->setRegister(resultReg);
    }

    // Note: For indirect rdbar nodes, the first child (sideEffectNode) is also used by the
    // load evaluator. The load evaluator will also evaluate+decrement it. In order to avoid double
    // decrementing the node we skip doing it here and let the load evaluator do it.
    return resultReg;
}

TR::Register *J9::X86::TreeEvaluator::fwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // For rdbar and wrtbar nodes we first evaluate the children we need to
    // handle the side effects. Then we delegate the evaluation of the remaining
    // children and the load/store operation to the appropriate load/store evaluator.
    TR::Register *valueReg = cg->evaluate(node->getFirstChild());
    TR::Node *sideEffectNode = node->getSecondChild();
    TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

    if (cg->comp()->getOption(TR_EnableFieldWatch)) {
        TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
    }

    // Note: The reference count for valueReg's node is not decremented here because the
    // store evaluator also uses it and so it will evaluate+decrement it. Thus we must skip decrementing here
    // to avoid double decrementing.
    cg->decReferenceCount(sideEffectNode);
    return TR::TreeEvaluator::floatingPointStoreEvaluator(node, cg);
}

TR::Register *J9::X86::TreeEvaluator::fwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // For rdbar and wrtbar nodes we first evaluate the children we need to
    // handle the side effects. Then we delegate the evaluation of the remaining
    // children and the load/store operation to the appropriate load/store evaluator.
    TR::Register *valueReg = cg->evaluate(node->getSecondChild());
    TR::Node *sideEffectNode = node->getThirdChild();
    TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

    if (cg->comp()->getOption(TR_EnableFieldWatch)) {
        TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
    }

    // Note: The reference count for valueReg's node is not decremented here because the
    // store evaluator also uses it and so it will evaluate+decrement it. Thus we must skip decrementing here
    // to avoid double decrementing.
    cg->decReferenceCount(sideEffectNode);
    return TR::TreeEvaluator::floatingPointStoreEvaluator(node, cg);
}

#ifdef TR_TARGET_32BIT
TR::Register *J9::X86::I386::TreeEvaluator::dwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // For rdbar and wrtbar nodes we first evaluate the children we need to
    // handle the side effects. Then we delegate the evaluation of the remaining
    // children and the load/store operation to the appropriate load/store evaluator.
    TR::Register *valueReg = cg->evaluate(node->getFirstChild());
    TR::Node *sideEffectNode = node->getSecondChild();
    TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

    if (cg->comp()->getOption(TR_EnableFieldWatch)) {
        TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
    }

    // Note: The reference count for valueReg's node is not decremented here because the
    // store evaluator also uses it and so it will evaluate+decrement it. Thus we must skip decrementing here
    // to avoid double decrementing.
    cg->decReferenceCount(sideEffectNode);
    return TR::TreeEvaluator::dstoreEvaluator(node, cg);
}

TR::Register *J9::X86::I386::TreeEvaluator::dwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // For rdbar and wrtbar nodes we first evaluate the children we need to
    // handle the side effects. Then we delegate the evaluation of the remaining
    // children and the load/store operation to the appropriate load/store evaluator.
    TR::Register *valueReg = cg->evaluate(node->getSecondChild());
    TR::Node *sideEffectNode = node->getThirdChild();
    TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

    if (cg->comp()->getOption(TR_EnableFieldWatch)) {
        TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
    }

    // Note: The reference count for valueReg's node is not decremented here because the
    // store evaluator also uses it and so it will evaluate+decrement it. Thus we must skip decrementing here
    // to avoid double decrementing.
    cg->decReferenceCount(sideEffectNode);
    return TR::TreeEvaluator::dstoreEvaluator(node, cg);
}

TR::Register *J9::X86::I386::TreeEvaluator::integerPairDivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Node *firstChild = node->getFirstChild();
    TR::Node *secondChild = node->getSecondChild();
    TR::Register *lowRegister = cg->allocateRegister();
    TR::Register *highRegister = cg->allocateRegister();

    TR::Register *firstRegister = cg->evaluate(firstChild);
    TR::Register *secondRegister = cg->evaluate(secondChild);

    TR::Register *firstHigh = firstRegister->getHighOrder();
    TR::Register *secondHigh = secondRegister->getHighOrder();

    TR::Instruction *divInstr = NULL;

    TR::RegisterDependencyConditions *idivDependencies = RegDeps((uint8_t)6, 6, cg);
    idivDependencies->addPreCondition(lowRegister, TR::RealRegister::eax, cg);
    idivDependencies->addPreCondition(highRegister, TR::RealRegister::edx, cg);
    idivDependencies->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
    idivDependencies->addPostCondition(highRegister, TR::RealRegister::edx, cg);
    idivDependencies->addPreCondition(firstHigh, TR::RealRegister::NoReg, cg);
    idivDependencies->addPreCondition(secondHigh, TR::RealRegister::NoReg, cg);
    idivDependencies->addPostCondition(firstHigh, TR::RealRegister::NoReg, cg);
    idivDependencies->addPostCondition(secondHigh, TR::RealRegister::NoReg, cg);
    idivDependencies->addPreCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
    idivDependencies->addPreCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
    idivDependencies->addPostCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
    idivDependencies->addPostCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);

    TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    TR::LabelSymbol *doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    TR::LabelSymbol *callLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);

    startLabel->setStartInternalControlFlow();
    doneLabel->setEndInternalControlFlow();

    Inst_Label(OP::label, node, startLabel, cg);

    Inst_RegReg(OP::MOV4RegReg, node, highRegister, secondHigh, cg);
    Inst_RegReg(OP::OR4RegReg, node, highRegister, firstHigh, cg);
    // Inst_RegReg(OP::TEST4RegReg, node, highRegister, highRegister, cg);
    Inst_Label(OP::JNE4, node, callLabel, cg);

    Inst_RegReg(OP::MOV4RegReg, node, lowRegister, firstRegister->getLowOrder(), cg);
    divInstr = Inst_RegReg(OP::DIV4AccReg, node, lowRegister, secondRegister->getLowOrder(), idivDependencies, cg);

    cg->setImplicitExceptionPoint(divInstr);
    divInstr->setNeedsGCMap(0xFF00FFF6);

    TR::RegisterDependencyConditions *xorDependencies1 = RegDeps((uint8_t)2, 2, cg);
    xorDependencies1->addPreCondition(lowRegister, TR::RealRegister::eax, cg);
    xorDependencies1->addPreCondition(highRegister, TR::RealRegister::edx, cg);
    xorDependencies1->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
    xorDependencies1->addPostCondition(highRegister, TR::RealRegister::edx, cg);
    Inst_RegReg(OP::XOR4RegReg, node, highRegister, highRegister, xorDependencies1, cg);

    Inst_Label(OP::JMP4, node, doneLabel, cg);

    Inst_Label(OP::label, node, callLabel, cg);

    TR::RegisterDependencyConditions *dependencies = RegDeps((uint8_t)0, 2, cg);
    dependencies->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
    dependencies->addPostCondition(highRegister, TR::RealRegister::edx, cg);
    J9::IA32PrivateLinkage *linkage = static_cast<J9::IA32PrivateLinkage *>(cg->getLinkage(TR_Private));
    TR::IA32LinkageUtils::pushLongArg(secondChild, cg);
    TR::IA32LinkageUtils::pushLongArg(firstChild, cg);
    TR::X86ImmSymInstruction *instr = Inst_HelperCall(node, TR_IA32longDivide, dependencies, cg);
    if (!linkage->getProperties().getCallerCleanup()) {
        instr->setAdjustsFramePointerBy(-16); // 2 long args
    }

    // Don't preserve eax and edx
    //
    instr->setNeedsGCMap(0xFF00FFF6);

    TR::RegisterDependencyConditions *labelDependencies = RegDeps((uint8_t)6, 6, cg);
    labelDependencies->addPreCondition(lowRegister, TR::RealRegister::eax, cg);
    labelDependencies->addPreCondition(highRegister, TR::RealRegister::edx, cg);
    labelDependencies->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
    labelDependencies->addPostCondition(highRegister, TR::RealRegister::edx, cg);
    labelDependencies->addPreCondition(firstHigh, TR::RealRegister::NoReg, cg);
    labelDependencies->addPreCondition(secondHigh, TR::RealRegister::NoReg, cg);
    labelDependencies->addPostCondition(firstHigh, TR::RealRegister::NoReg, cg);
    labelDependencies->addPostCondition(secondHigh, TR::RealRegister::NoReg, cg);
    labelDependencies->addPreCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
    labelDependencies->addPreCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
    labelDependencies->addPostCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
    labelDependencies->addPostCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);

    Inst_Label(OP::label, node, doneLabel, labelDependencies, cg);

    TR::Register *targetRegister = cg->allocateRegisterPair(lowRegister, highRegister);
    node->setRegister(targetRegister);

    return targetRegister;
}

TR::Register *J9::X86::I386::TreeEvaluator::integerPairRemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // TODO: Consider combining with integerPairDivEvaluator
    TR::Node *firstChild = node->getFirstChild();
    TR::Node *secondChild = node->getSecondChild();
    TR::Register *lowRegister = cg->allocateRegister();
    TR::Register *highRegister = cg->allocateRegister();

    TR::Register *firstRegister = cg->evaluate(firstChild);
    TR::Register *secondRegister = cg->evaluate(secondChild);

    TR::Register *firstHigh = firstRegister->getHighOrder();
    TR::Register *secondHigh = secondRegister->getHighOrder();

    TR::Instruction *divInstr = NULL;

    TR::RegisterDependencyConditions *idivDependencies = RegDeps((uint8_t)6, 6, cg);
    idivDependencies->addPreCondition(lowRegister, TR::RealRegister::eax, cg);
    idivDependencies->addPreCondition(highRegister, TR::RealRegister::edx, cg);
    idivDependencies->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
    idivDependencies->addPostCondition(highRegister, TR::RealRegister::edx, cg);
    idivDependencies->addPreCondition(firstHigh, TR::RealRegister::NoReg, cg);
    idivDependencies->addPreCondition(secondHigh, TR::RealRegister::NoReg, cg);
    idivDependencies->addPostCondition(firstHigh, TR::RealRegister::NoReg, cg);
    idivDependencies->addPostCondition(secondHigh, TR::RealRegister::NoReg, cg);
    idivDependencies->addPreCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
    idivDependencies->addPreCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
    idivDependencies->addPostCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
    idivDependencies->addPostCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);

    TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    TR::LabelSymbol *doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    TR::LabelSymbol *callLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);

    startLabel->setStartInternalControlFlow();
    doneLabel->setEndInternalControlFlow();

    Inst_Label(OP::label, node, startLabel, cg);

    Inst_RegReg(OP::MOV4RegReg, node, highRegister, secondHigh, cg);
    Inst_RegReg(OP::OR4RegReg, node, highRegister, firstHigh, cg);
    // it doesn't need the test instruction, OR will set the flags properly
    // Inst_RegReg(OP::TEST4RegReg, node, highRegister, highRegister, cg);
    Inst_Label(OP::JNE4, node, callLabel, cg);

    Inst_RegReg(OP::MOV4RegReg, node, lowRegister, firstRegister->getLowOrder(), cg);
    divInstr = Inst_RegReg(OP::DIV4AccReg, node, lowRegister, secondRegister->getLowOrder(), idivDependencies, cg);

    cg->setImplicitExceptionPoint(divInstr);
    divInstr->setNeedsGCMap(0xFF00FFF6);

    Inst_RegReg(OP::MOV4RegReg, node, lowRegister, highRegister, cg);

    Inst_RegReg(OP::XOR4RegReg, node, highRegister, highRegister, cg);
    Inst_Label(OP::JMP4, node, doneLabel, cg);

    Inst_Label(OP::label, node, callLabel, cg);

    TR::RegisterDependencyConditions *dependencies = RegDeps((uint8_t)4, 6, cg);

    dependencies->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
    dependencies->addPostCondition(highRegister, TR::RealRegister::edx, cg);
    dependencies->addPreCondition(firstHigh, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(secondHigh, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(firstHigh, TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(secondHigh, TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
    dependencies->addPreCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
    dependencies->addPostCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);

    J9::IA32PrivateLinkage *linkage = static_cast<J9::IA32PrivateLinkage *>(cg->getLinkage(TR_Private));
    TR::IA32LinkageUtils::pushLongArg(secondChild, cg);
    TR::IA32LinkageUtils::pushLongArg(firstChild, cg);
    TR::X86ImmSymInstruction *instr = Inst_HelperCall(node, TR_IA32longRemainder, dependencies, cg);
    if (!linkage->getProperties().getCallerCleanup()) {
        instr->setAdjustsFramePointerBy(-16); // 2 long args
    }

    // Don't preserve eax and edx
    //
    instr->setNeedsGCMap(0xFF00FFF6);

    TR::RegisterDependencyConditions *movDependencies = RegDeps((uint8_t)6, 6, cg);
    movDependencies->addPreCondition(lowRegister, TR::RealRegister::eax, cg);
    movDependencies->addPreCondition(highRegister, TR::RealRegister::edx, cg);
    movDependencies->addPostCondition(lowRegister, TR::RealRegister::eax, cg);
    movDependencies->addPostCondition(highRegister, TR::RealRegister::edx, cg);
    movDependencies->addPreCondition(firstHigh, TR::RealRegister::NoReg, cg);
    movDependencies->addPreCondition(secondHigh, TR::RealRegister::NoReg, cg);
    movDependencies->addPostCondition(firstHigh, TR::RealRegister::NoReg, cg);
    movDependencies->addPostCondition(secondHigh, TR::RealRegister::NoReg, cg);
    movDependencies->addPreCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
    movDependencies->addPreCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
    movDependencies->addPostCondition(firstRegister->getLowOrder(), TR::RealRegister::NoReg, cg);
    movDependencies->addPostCondition(secondRegister->getLowOrder(), TR::RealRegister::NoReg, cg);

    Inst_Label(OP::label, node, doneLabel, movDependencies, cg);

    TR::Register *targetRegister = cg->allocateRegisterPair(lowRegister, highRegister);
    node->setRegister(targetRegister);

    return targetRegister;
}

bool J9::X86::I386::TreeEvaluator::lstoreEvaluatorIsNodeVolatile(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR::SymbolReference *symRef = node->getSymbolReference();
    bool isVolatile = false;

    if (symRef && !symRef->isUnresolved()) {
        TR::Symbol *symbol = symRef->getSymbol();
        isVolatile = symbol->isVolatile();
        TR_OpaqueMethodBlock *caller = node->getOwningMethod();
        if (isVolatile && caller) {
            TR_ResolvedMethod *m = comp->fe()->createResolvedMethod(cg->trMemory(), caller,
                node->getSymbolReference()->getOwningMethod(comp));
            if (m->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicLong_lazySet) {
                isVolatile = false;
            }
        }
    }

    return isVolatile;
}

void J9::X86::I386::TreeEvaluator::lStoreEvaluatorSetHighLowMRIfNeeded(TR::Node *node, TR::MemoryReference *lowMR,
    TR::MemoryReference *highMR, TR::CodeGenerator *cg)
{
    if (node->getSymbolReference()->getSymbol()->isVolatile()) {
        TR_OpaqueMethodBlock *caller = node->getOwningMethod();
        if ((lowMR || highMR) && caller) {
            TR_ResolvedMethod *m = cg->comp()->fe()->createResolvedMethod(cg->trMemory(), caller,
                node->getSymbolReference()->getOwningMethod(cg->comp()));
            if (m->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicLong_lazySet) {
                if (lowMR)
                    lowMR->setIgnoreVolatile();
                if (highMR)
                    highMR->setIgnoreVolatile();
            }
        }
    }
}
#endif

#ifdef TR_TARGET_64BIT
TR::Register *J9::X86::AMD64::TreeEvaluator::dwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // For rdbar and wrtbar nodes we first evaluate the children we need to
    // handle the side effects. Then we delegate the evaluation of the remaining
    // children and the load/store operation to the appropriate load/store evaluator.
    TR::Register *valueReg = cg->evaluate(node->getFirstChild());
    TR::Node *sideEffectNode = node->getSecondChild();
    TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

    if (cg->comp()->getOption(TR_EnableFieldWatch)) {
        TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
    }

    // Note: The reference count for valueReg's node is not decremented here because the
    // store evaluator also uses it and so it will evaluate+decrement it. Thus we must skip decrementing here
    // to avoid double decrementing.
    cg->decReferenceCount(sideEffectNode);
    return TR::TreeEvaluator::floatingPointStoreEvaluator(node, cg);
}

TR::Register *J9::X86::AMD64::TreeEvaluator::dwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // For rdbar and wrtbar nodes we first evaluate the children we need to
    // handle the side effects. Then we delegate the evaluation of the remaining
    // children and the load/store operation to the appropriate load/store evaluator.
    TR::Register *valueReg = cg->evaluate(node->getSecondChild());
    TR::Node *sideEffectNode = node->getThirdChild();
    TR::Register *sideEffectRegister = cg->evaluate(sideEffectNode);

    if (cg->comp()->getOption(TR_EnableFieldWatch)) {
        TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
    }

    // Note: The reference count for valueReg's node is not decremented here because the
    // store evaluator also uses it and so it will evaluate+decrement it. Thus we must skip decrementing here
    // to avoid double decrementing.
    cg->decReferenceCount(sideEffectNode);
    return TR::TreeEvaluator::floatingPointStoreEvaluator(node, cg);
}
#endif

TR::Register *J9::X86::TreeEvaluator::awrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    return TR::TreeEvaluator::awrtbarEvaluator(node, cg);
}

TR::Register *J9::X86::TreeEvaluator::awrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
{
    // For rdbar and wrtbar nodes we first evaluate the children we need to
    // handle the side effects. Then we delegate the evaluation of the remaining
    // children and the load/store operation to the appropriate load/store evaluator.
    TR::Register *valueReg;
    TR::Register *sideEffectRegister;
    TR::Node *firstChildNode = node->getFirstChild();

    // Evaluate the children we need to handle the side effect, then go to writeBarrierEvaluator to handle the write
    // barrier
    if (node->getOpCode().isIndirect()) {
        TR::Node *valueNode = NULL;
        // Pass in valueNode so it can be set to the correct node.
        TR::TreeEvaluator::getIndirectWrtbarValueNode(cg, node, valueNode, false);
        valueReg = cg->evaluate(valueNode);
        sideEffectRegister = cg->evaluate(node->getThirdChild());
    } else {
        valueReg = cg->evaluate(firstChildNode);
        sideEffectRegister = cg->evaluate(node->getSecondChild());
    }

    if (cg->comp()->getOption(TR_EnableFieldWatch) && !node->getSymbolReference()->getSymbol()->isArrayShadowSymbol()) {
        TR::TreeEvaluator::rdWrtbarHelperForFieldWatch(node, cg, sideEffectRegister, valueReg);
    }

    // This evaluator handles the actual awrtbar operation. We also avoid decrementing the reference
    // counts of the children evaluated here and let this helper handle it.
    return TR::TreeEvaluator::writeBarrierEvaluator(node, cg);
}
