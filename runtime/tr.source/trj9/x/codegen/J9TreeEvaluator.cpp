/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
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
#include "codegen/LiveRegister.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/ScratchRegisterManager.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CompilerEnv.hpp"
#include "env/CHTable.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/SimpleRegex.hpp"
#include "trj9/env/j9method.h"
#include "trj9/env/VMJ9.h"
#include "x/codegen/AllocPrefetchSnippet.hpp"
#include "x/codegen/CheckFailureSnippet.hpp"
#include "x/codegen/CompareAnalyser.hpp"
#include "x/codegen/ForceRecompilationSnippet.hpp"
#include "x/codegen/FPTreeEvaluator.hpp"
#include "x/codegen/J9X86Instruction.hpp"
#include "x/codegen/MonitorSnippet.hpp"
#include "x/codegen/OutlinedInstructions.hpp"
#include "x/codegen/WriteBarrierSnippet.hpp"
#include "x/codegen/X86Evaluator.hpp"
#include "env/CompilerEnv.hpp"

#ifdef TR_TARGET_64BIT
#include "codegen/AMD64PrivateLinkage.hpp"
#endif

#ifdef TR_TARGET_32BIT
#include "codegen/IA32PrivateLinkage.hpp"
#endif

#ifdef WINDOWS
#include <windows.h>
#else
#include <time.h>
#include <sys/time.h>
#endif

#define NUM_PICS 3

// Minimum number of words for zero-initialization via REP STOSD
//
#define MIN_REPSTOSD_WORDS 64
static int32_t minRepstosdWords = 0;

// Maximum number of words per loop iteration for loop zero-initialization.
//
#define MAX_ZERO_INIT_WORDS_PER_ITERATION 4
static int32_t maxZeroInitWordsPerIteration = 0;

static bool getNodeIs64Bit(TR::Node *node, TR::CodeGenerator *cg);
static TR::Register *intOrLongClobberEvaluate(TR::Node *node, bool nodeIs64Bit, TR::CodeGenerator *cg);

static uint32_t logBase2(uintptrj_t n)
   {
   // Could use leadingZeroes, except we can't call it from here
   //
   uint32_t result = 8*sizeof(n)-1;
   uintptrj_t mask  = ((uintptrj_t)1) << result;
   while (mask && !(mask & n))
      {
      mask >>= 1;
      result--;
      }
   return result;
   }


static TR_OutlinedInstructions *generateArrayletReference(
   TR::Node           *node,
   TR::Node           *loadOrStoreOrArrayElementNode,
   TR::Instruction *checkInstruction,
   TR::LabelSymbol     *arrayletRefLabel,
   TR::LabelSymbol     *restartLabel,
   TR::Register       *baseArrayReg,
   TR::Register       *loadOrStoreReg,
   TR::Register       *indexReg,
   int32_t            indexValue,
   TR::Register       *valueReg,
   bool               needsBoundCheck,
   TR::CodeGenerator  *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

   TR::Register *scratchReg = cg->allocateRegister();

   TR_OutlinedInstructions *arrayletRef = new (cg->trHeapMemory()) TR_OutlinedInstructions(arrayletRefLabel, cg);
   arrayletRef->setRestartLabel(restartLabel);

   if (needsBoundCheck)
      {
      // The current block is required for exception handling and anchoring
      // the GC map.
      //
      arrayletRef->setBlock(cg->getCurrentEvaluationBlock());
      arrayletRef->setCallNode(node);
      }

   cg->getOutlinedInstructionsList().push_front(arrayletRef);

   arrayletRef->swapInstructionListsWithCompilation();

   generateLabelInstruction(NULL, LABEL, arrayletRefLabel, cg)->setNode(node);

   // TODO: REMOVE THIS!
   //
   // This merely indicates that this OOL sequence should be assigned with the non-linear
   // assigner, and should go away when the non-linear assigner handles all OOL sequences.
   //
   arrayletRefLabel->setNonLinear();

   static char *forceArrayletInt = feGetEnv("TR_forceArrayletInt");
   if (forceArrayletInt)
      {
      generateInstruction(BADIA32Op, node, cg);
      }

   // -----------------------------------------------------------------------------------
   // Track all virtual register use within the arraylet path.  This info will be used
   // to adjust the virtual register use counts within the mainline path for more precise
   // register assignment.
   // -----------------------------------------------------------------------------------

   cg->startRecordingRegisterUsage();

   if (needsBoundCheck)
      {
      // -------------------------------------------------------------------------
      // Check if the base array has a spine.  If not, this is a real AIOB.
      // -------------------------------------------------------------------------

      TR::MemoryReference *arraySizeMR =
         generateX86MemoryReference(baseArrayReg, fej9->getOffsetOfContiguousArraySizeField(), cg);

      generateMemImmInstruction(CMP4MemImms, node, arraySizeMR, 0, cg);

      TR::LabelSymbol *boundCheckFailureLabel = generateLabelSymbol(cg);

      checkInstruction = generateLabelInstruction(JNE4, node, boundCheckFailureLabel, cg);

      bool needsVMThread = needsBoundCheck &&
         (!cg->allowVMThreadRematerialization() || !cg->getSupportsVMThreadGRA());

      cg->addSnippet(
         new (cg->trHeapMemory()) TR::X86CheckFailureSnippet(
            cg, node->getSymbolReference(),
            boundCheckFailureLabel,
            checkInstruction,
            false,
            !needsVMThread));

      // -------------------------------------------------------------------------
      // The array has a spine.  Do a bound check on its true length.
      // -------------------------------------------------------------------------

      arraySizeMR = generateX86MemoryReference(baseArrayReg, fej9->getOffsetOfDiscontiguousArraySizeField(), cg);

      if (!indexReg)
         {
         TR_X86OpCodes op = (indexValue >= -128 && indexValue <= 127) ? CMP4MemImms : CMP4MemImm4;
         generateMemImmInstruction(op, node, arraySizeMR, indexValue, cg);
         }
      else
         {
         generateMemRegInstruction(CMP4MemReg, node, arraySizeMR, indexReg, cg);
         }

      boundCheckFailureLabel = generateLabelSymbol(cg);
      checkInstruction = generateLabelInstruction(JBE4, node, boundCheckFailureLabel, cg);

      cg->addSnippet(
         new (cg->trHeapMemory()) TR::X86CheckFailureSnippet(
            cg, node->getSymbolReference(),
            boundCheckFailureLabel,
            checkInstruction,
            false,
            !needsVMThread));
      }

   // -------------------------------------------------------------------------
   // Determine if a load needs to be decompressed.
   // -------------------------------------------------------------------------

   bool seenCompressionSequence = false;
   bool loadNeedsDecompression = false;

   if (loadOrStoreOrArrayElementNode->getOpCodeValue() == TR::l2a ||
       ((loadOrStoreOrArrayElementNode->getOpCodeValue() == TR::aload ||
         loadOrStoreOrArrayElementNode->getOpCodeValue() == TR::aRegLoad) &&
        node->isSpineCheckWithArrayElementChild()) &&
       TR::Compiler->target.is64Bit() && comp->useCompressedPointers())
      loadNeedsDecompression = true;

   TR::Node *actualLoadOrStoreOrArrayElementNode = loadOrStoreOrArrayElementNode;
   while ((loadNeedsDecompression && actualLoadOrStoreOrArrayElementNode->getOpCode().isConversion()) ||
          actualLoadOrStoreOrArrayElementNode->containsCompressionSequence())
      {
      if (actualLoadOrStoreOrArrayElementNode->containsCompressionSequence())
         seenCompressionSequence = true;

      actualLoadOrStoreOrArrayElementNode = actualLoadOrStoreOrArrayElementNode->getFirstChild();
      }

   // -------------------------------------------------------------------------
   // Do the load, store, or array address calculation
   // -------------------------------------------------------------------------

   TR::DataType dt = actualLoadOrStoreOrArrayElementNode->getDataType();
   int32_t elementSize;

   if (dt == TR::Address)
      {
      elementSize = TR::Compiler->om.sizeofReferenceField();
      }
   else
      {
      elementSize = TR::Symbol::convertTypeToSize(dt);
      }

   int32_t spinePointerSize = (TR::Compiler->target.is64Bit() && !comp->useCompressedPointers()) ? 8 : 4;
   int32_t arrayHeaderSize = TR::Compiler->om.discontiguousArrayHeaderSizeInBytes();
   int32_t arrayletMask = fej9->getArrayletMask(elementSize);

   TR::MemoryReference *spineMR;

   // Load the arraylet from the spine.
   //
   if (indexReg)
      {
      TR_X86OpCodes op = TR::Compiler->target.is64Bit() ? MOVSXReg8Reg4 : MOVRegReg();
      generateRegRegInstruction(op, node, scratchReg, indexReg, cg);

      int32_t spineShift = fej9->getArraySpineShift(elementSize);
      generateRegImmInstruction(SARRegImm1(), node, scratchReg, spineShift, cg);

      spineMR =
         generateX86MemoryReference(
            baseArrayReg,
            scratchReg,
            TR::MemoryReference::convertMultiplierToStride(spinePointerSize),
            arrayHeaderSize,
            cg);
      }
   else
      {
      int32_t spineIndex = fej9->getArrayletLeafIndex(indexValue, elementSize);
      int32_t spineDisp32 = (spineIndex * spinePointerSize) + arrayHeaderSize;

      spineMR = generateX86MemoryReference(baseArrayReg, spineDisp32, cg);
      }

   TR_X86OpCodes op = (spinePointerSize == 8) ? L8RegMem : L4RegMem;
   generateRegMemInstruction(op, node, scratchReg, spineMR, cg);

   // Decompress the arraylet pointer from the spine.
   //
   bool isHB0 = false;
   int32_t shiftOffset = 0;

   if (TR::Compiler->target.is64Bit() && comp->useCompressedPointers())
      {
      if (TR::Compiler->vm.heapBaseAddress() == 0)
         isHB0 = true;

      shiftOffset = TR::Compiler->om.compressedReferenceShiftOffset();

      if (isHB0)
         {
         if (shiftOffset > 0)
            {
            generateRegImmInstruction(SHL8RegImm1, node, scratchReg, shiftOffset, cg);
            }
         }
      else
         {
         TR_ASSERT(0, "!HB0 not supported yet");
         }
      }

   TR::MemoryReference *arrayletMR;

   // Calculate the offset with the arraylet for the index.
   //
   if (indexReg)
      {
      TR::Register *scratchReg2 = cg->allocateRegister();

      generateRegRegInstruction(MOVRegReg(), node, scratchReg2, indexReg, cg);
      generateRegImmInstruction(ANDRegImm4(), node, scratchReg2, arrayletMask, cg);
      arrayletMR = generateX86MemoryReference(
         scratchReg,
         scratchReg2,
         TR::MemoryReference::convertMultiplierToStride(elementSize),
         cg);

      cg->stopUsingRegister(scratchReg2);
      }
   else
      {
      int32_t arrayletIndex = ((TR_J9VMBase *)fej9)->getLeafElementIndex(indexValue, elementSize);
      arrayletMR = generateX86MemoryReference(scratchReg, arrayletIndex*elementSize, cg);
      }

   cg->stopUsingRegister(scratchReg);

   if (!actualLoadOrStoreOrArrayElementNode->getOpCode().isStore())
      {
      TR_X86OpCodes op;

      TR::MemoryReference *highArrayletMR = NULL;
      TR::Register *highRegister = NULL;

      // If we're not loading an array shadow then this must be an effective
      // address computation on the array element (for a write barrier).
      //
      if ((!actualLoadOrStoreOrArrayElementNode->getOpCode().hasSymbolReference() ||
           !actualLoadOrStoreOrArrayElementNode->getSymbolReference()->getSymbol()->isArrayShadowSymbol()) &&
          !node->isSpineCheckWithArrayElementChild())
         {
         op = LEARegMem();
         }
      else
         {
         switch (dt)
            {
            case TR::Int8:   op = L1RegMem; break;
            case TR::Int16:  op = L2RegMem; break;
            case TR::Int32:  op = L4RegMem; break;
            case TR::Int64:
               if (TR::Compiler->target.is64Bit())
                  op = L8RegMem;
               else
                  {
                  TR_ASSERT(loadOrStoreReg->getRegisterPair(), "expecting a register pair");

                  op = L4RegMem;
                  highArrayletMR = generateX86MemoryReference(*arrayletMR, 4, cg);
                  highRegister = loadOrStoreReg->getHighOrder();
                  loadOrStoreReg = loadOrStoreReg->getLowOrder();
                  }
               break;

            case TR::Float:  op = MOVSSRegMem; break;
            case TR::Double: op = MOVSDRegMem; break;

            case TR::Address:
               if (TR::Compiler->target.is32Bit() || comp->useCompressedPointers())
                  op = L4RegMem;
               else
                  op = L8RegMem;
               break;

            default:
               TR_ASSERT(0, "unsupported array element load type");
               op = BADIA32Op;
            }
         }

      generateRegMemInstruction(op, node, loadOrStoreReg, arrayletMR, cg);

      if (highArrayletMR)
         {
         generateRegMemInstruction(op, node, highRegister, highArrayletMR, cg);
         }

      // Decompress the loaded address if necessary.
      //
      if (loadNeedsDecompression)
         {
         if (TR::Compiler->target.is64Bit() && comp->useCompressedPointers())

         if (isHB0)
            {
            if (shiftOffset > 0)
               {
               generateRegImmInstruction(SHL8RegImm1, node, loadOrStoreReg, shiftOffset, cg);
               }
            }
         else
            {
            TR_ASSERT(0, "!HB0 not supported yet");
            }
         }
      }
   else
      {
      if (dt != TR::Address)
         {
         // movE [S + S2], value
         //
         TR_X86OpCodes op;
         bool needStore = true;

         switch (dt)
            {
            case TR::Int8:   op = valueReg ? S1MemReg : S1MemImm1; break;
            case TR::Int16:  op = valueReg ? S2MemReg : S2MemImm2; break;
            case TR::Int32:  op = valueReg ? S4MemReg : S4MemImm4; break;
            case TR::Int64:
               if (TR::Compiler->target.is64Bit())
                  {
                  // The range of the immediate must be verified before this function to
                  // fall within a signed 32-bit integer.
                  //
                  op = valueReg ? S8MemReg : S8MemImm4;
                  }
               else
                  {
                  if (valueReg)
                     {
                     TR_ASSERT(valueReg->getRegisterPair(), "value must be a register pair");
                     generateMemRegInstruction(S4MemReg, node, arrayletMR, valueReg->getLowOrder(), cg);
                     generateMemRegInstruction(S4MemReg, node,
                        generateX86MemoryReference(*arrayletMR, 4, cg),
                        valueReg->getHighOrder(), cg);
                     }
                  else
                     {
                     TR::Node *valueChild = actualLoadOrStoreOrArrayElementNode->getSecondChild();
                     TR_ASSERT(valueChild->getOpCode().isLoadConst(), "expecting a long constant child");

                     generateMemImmInstruction(S4MemImm4, node, arrayletMR, valueChild->getLongIntLow(), cg);
                     generateMemImmInstruction(S4MemImm4, node,
                        generateX86MemoryReference(*arrayletMR, 4, cg),
                        valueChild->getLongIntHigh(), cg);
                     }

                  needStore = false;
                  }
               break;

            case TR::Float:  op = MOVSSMemReg; break;
            case TR::Double: op = MOVSDMemReg; break;

            default:
               TR_ASSERT(0, "unsupported array element store type");
               op = BADIA32Op;
            }

         if (needStore)
            {
            if (valueReg)
               generateMemRegInstruction(op, node, arrayletMR, valueReg, cg);
            else
               {
               int32_t value = actualLoadOrStoreOrArrayElementNode->getSecondChild()->getInt();
               generateMemImmInstruction(op, node, arrayletMR, value, cg);
               }
            }
         }
      else
         {
         // lea S, [S+S2]
         TR_ASSERT(0, "OOL reference stores not supported yet");
         }
      }

   generateLabelInstruction(JMP4, node, restartLabel, cg);

   // -----------------------------------------------------------------------------------
   // Stop tracking virtual register usage.
   // -----------------------------------------------------------------------------------

   arrayletRef->setOutlinedPathRegisterUsageList(cg->stopRecordingRegisterUsage());

   arrayletRef->swapInstructionListsWithCompilation();

   return arrayletRef;
   }


// Generate a null test on an object reference
//
static inline void genNullTest(TR::Node * node, TR::Register *objectReg, TR::CodeGenerator *cg)
   {
   generateRegRegInstruction(TESTRegReg(), node, objectReg, objectReg, cg);
   }


static TR::Instruction *generatePrefetchAfterHeaderAccess(TR::Node              *node,
                                                         TR::Register          *objectReg,
                                                         TR::CodeGenerator     *cg)
   {
   TR::Instruction *instr = NULL;

   static const char *prefetch = feGetEnv("TR_EnableSoftwarePrefetch");
   if (prefetch && cg->comp()->getMethodHotness()>=scorching && cg->getX86ProcessorInfo().isIntelCore2())
      {
      int32_t fieldOffset = 0;
      if (TR::TreeEvaluator::loadLookaheadAfterHeaderAccess(node, fieldOffset, cg))
         {
         if (fieldOffset > 32)
            instr = generateMemInstruction(PREFETCHT0, node, generateX86MemoryReference(objectReg, fieldOffset, cg), cg);

         //printf("found a field load after monitor field at field offset %d\n", fieldOffset);
         }
      }

   return instr;
   }


// Generate the inline class equality test for checkcast and instanceof
//
static TR::Instruction *genEqualityInstanceOfTest(TR::Node                *node,
                                                 TR::Register            *objectClassReg,
                                                 TR::MemoryReference *objectClassMR,
                                                 TR::Register            *castClassReg,
                                                 TR_X86ScratchRegisterManager *srm,
                                                 TR::CodeGenerator       *cg,
                                                 TR_OpaqueClassBlock    *iClazz)
   {
   TR::Node            *castClassRef    = node->getSecondChild();
   TR::SymbolReference *castClassSymRef = castClassRef->getSymbolReference();
   TR::StaticSymbol    *castClassSym    = castClassSymRef->getSymbol()->getStaticSymbol();

   TR::Instruction     *patchableInstruction = NULL;
   TR::Instruction     *exceptionPoint       = NULL;
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

   bool                use64BitClasses = TR::Compiler->target.is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders();
   bool                objectClassRegIsScratch = false;
   bool                castClassRegIsScratch   = false;

   if (fej9->vftFieldRequiresMask() && objectClassReg == NULL)
      {
      objectClassReg = srm->findOrCreateScratchRegister(); // must reclaim this before each return!
      objectClassRegIsScratch = true;
      exceptionPoint = generateRegMemInstruction(LRegMem(use64BitClasses), node, objectClassReg, objectClassMR, cg);
      TR::TreeEvaluator::generateVFTMaskInstruction(node, objectClassReg, cg);
      }

   if ((iClazz != NULL) && IS_32BIT_SIGNED((intptrj_t)iClazz))
      {
      if (objectClassReg == NULL)
         exceptionPoint = patchableInstruction = generateMemImmSymInstruction(CMPMemImm4(use64BitClasses), node,
                                             objectClassMR,
                                             (int32_t)(uintptr_t)iClazz,
                                             castClassSymRef, cg);
      else
         patchableInstruction = generateRegImmSymInstruction(CMPRegImm4(use64BitClasses), node,
                                             objectClassReg,
                                             (int32_t)(uintptr_t)iClazz,
                                             castClassSymRef, cg);
      // HCR in genEqualityInstanceOfTest
      if (cg->wantToPatchClassPointer(iClazz, node))
         comp->getStaticHCRPICSites()->push_front(patchableInstruction);

      if (objectClassRegIsScratch)
         srm->reclaimScratchRegister(objectClassReg);

      return exceptionPoint;
      }

   if (castClassReg == NULL && !IS_32BIT_SIGNED((intptrj_t)castClassSym->getStaticAddress()))
      {
      castClassReg = srm->findOrCreateScratchRegister(); // must reclaim this before each return!
      castClassRegIsScratch = true;
      generateRegImm64Instruction(MOV8RegImm64, node, castClassReg, (uintptrj_t)castClassSym->getStaticAddress(), cg);
      }

   if (castClassReg == NULL)
      {
      TR_ASSERT(!castClassSymRef->isUnresolved(),"Expecting class to be resolved");

      if (castClassSymRef->isUnresolved())
         {
         TR::UnresolvedDataSnippet *snippet = TR::UnresolvedDataSnippet::create(cg, node, castClassSymRef, false, castClassSymRef->canCauseGC());
         cg->addSnippet(snippet);
         TR::Instruction  *tempInstr =
            generateMemImmSnippetInstruction(CMP4MemImm4, node,
                                             objectClassMR,
                                             castClassSymRef->getCPIndex(),
                                             snippet, cg);
         snippet->setDataReferenceInstruction(tempInstr);

         if (objectClassRegIsScratch)
            srm->reclaimScratchRegister(objectClassReg);
         if (castClassRegIsScratch)
            srm->reclaimScratchRegister(castClassReg);

         return tempInstr;
         }
      else
         {
         if (objectClassReg == NULL)
            exceptionPoint = patchableInstruction = generateMemImmSymInstruction(CMP4MemImm4, node,
                                         objectClassMR,
                                         (int32_t)(uintptr_t)castClassSym->getStaticAddress(),
                                         castClassSymRef, cg);
         else
            patchableInstruction = generateRegImmSymInstruction(CMP4RegImm4, node,
                                         objectClassReg,
                                         (int32_t)(uintptr_t)castClassSym->getStaticAddress(),
                                         castClassSymRef, cg);

         // HCR in genEqualityInstanceOfTest
         if (cg->wantToPatchClassPointer((TR_OpaqueClassBlock*)castClassSym->getStaticAddress(), node))
            comp->getStaticHCRPICSites()->push_front(patchableInstruction);

         if (objectClassRegIsScratch)
            srm->reclaimScratchRegister(objectClassReg);
         if (castClassRegIsScratch)
            srm->reclaimScratchRegister(castClassReg);

         return exceptionPoint;
         }
      }
   else
      {
      if (objectClassReg == NULL)
         exceptionPoint = patchableInstruction = generateMemRegInstruction(CMPMemReg(use64BitClasses), node, objectClassMR, castClassReg, cg);
      else
         patchableInstruction = generateRegRegInstruction(CMPRegReg(use64BitClasses), node, objectClassReg, castClassReg, cg);

      if (objectClassRegIsScratch)
         srm->reclaimScratchRegister(objectClassReg);
      if (castClassRegIsScratch)
         srm->reclaimScratchRegister(castClassReg);

      return exceptionPoint;
      }
   }

static void generateInstanceOfCacheTest (TR::Node                *node,
                                         TR::SymbolReference     *castClassSymRef,
                                         TR::Register            *objectClassReg,
                                         TR::Register            *testRegister,
                                         TR::LabelSymbol          *falsePath,
                                         TR::LabelSymbol          *truePath,
                                         TR::LabelSymbol          *helperCall,
                                         TR::LabelSymbol          *restartLabel,
                                         TR::CodeGenerator       *cg);


// Generate the inline superclass test for checkcast and instanceof
//
static void genSuperClassInstanceOfTest(TR::Node        *node,
                                        TR::Register    *objectClassReg,
                                        TR::Register    *testerReg,
                                        TR::Register    *castClassReg,
                                        int32_t         classDepth,
                                        TR::LabelSymbol *failLabel,
                                        TR::LabelSymbol *trueLabel,
                                        TR::RegisterDependencyConditions  *failDeps,
                                        TR_X86ScratchRegisterManager *srm,
                                        TR::CodeGenerator *cg,
                                        J9Class         *iClazz,
                                        TR::LabelSymbol   *endLabel,
                                        TR::Node       *castClassRef,
                                        bool            needObjectClassRegClear,
                                        bool            addSnippet)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::SymbolReference *castClassSymRef = castClassRef->getSymbolReference();
   TR::StaticSymbol    *castClassSym    = castClassSymRef->getSymbol()->getStaticSymbol();

   bool                use64BitClasses = TR::Compiler->target.is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders();
   bool                castClassRegIsScratch   = false;

   TR::Register        *classDepthReg   = NULL;

   TR::Instruction     *instr           = NULL;

   TR::Compilation *comp = cg->comp();

   bool eliminateSuperClassArraySizeCheck = (classDepth < comp->getOptions()->_minimumSuperclassArraySize);

   if (classDepth == -1)
      eliminateSuperClassArraySizeCheck = false;

   // Find the object's class depth and check it against the (known) depth
   // of the cast class
   //
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
   // objectClassReg contains the class offset so we may need to generate code
   // to convert from class offset to real J9Class pointer
#endif

   if (classDepth == -1)
      {
      TR_ASSERT((trueLabel &&
               node->getOpCodeValue() == TR::instanceof &&
               node->getSecondChild()->getOpCodeValue() != TR::loadaddr), "inconsistent");

      // check if this is an interface
      TR::LabelSymbol *notInterfaceLabel = generateLabelSymbol(cg);

      instr=generateRegMemInstruction(LRegMem(), node,
                                      testerReg,
                                      generateX86MemoryReference(castClassReg, offsetof(J9Class,romClass), cg), cg);

      instr=generateRegMemInstruction(LRegMem(), node,
                                      testerReg,
                                      generateX86MemoryReference(testerReg, offsetof(J9ROMClass, modifiers), cg), cg);

      instr=generateRegImmInstruction(TEST4RegImm4, node, testerReg, J9AccInterface | J9AccClassArray, cg);

      instr=generateLabelInstruction(JE4, node, notInterfaceLabel, cg);

      // vm interface test
      bool rematerializeVMThreadInSnippet = false;
      static const char *allowVMThreadRemat = feGetEnv("TR_allowVMThreadRemat");

      if (allowVMThreadRemat && cg->supportsFS0VMThreadRematerialization())
         rematerializeVMThreadInSnippet = true;

      TR::LabelSymbol *callHelper = generateLabelSymbol(cg);
      TR::LabelSymbol *restartLabel = generateLabelSymbol(cg);

      TR_OutlinedInstructions *outlinedHelperCall = NULL;

      if (endLabel != NULL)
         {
         outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::icall, testerReg, callHelper,
                                                                               endLabel, rematerializeVMThreadInSnippet, cg);
         }
      else
         {
         outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::icall, testerReg, callHelper,
                                                                               restartLabel, rematerializeVMThreadInSnippet, cg);
         }

      cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);
      cg->generateDebugCounter(
         outlinedHelperCall->getFirstInstruction(),
         TR::DebugCounter::debugCounterName(comp, "helperCalls/%s/(%s)/%d/%d", node->getOpCode().getName(), comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
         1, TR::DebugCounter::Cheap);


      // insert snippet check
      TR::IA32DataSnippet *classObjectClazzSnippet = NULL;
      TR::IA32DataSnippet *instanceOfClazzSnippet = NULL;
      bool snippetAdded = false;
      if (addSnippet)
         {
         TR_OpaqueClassBlock* castClassAddr = TR::TreeEvaluator::getCastClassAddress(castClassRef);
         if (castClassAddr == NULL)
            {
            // snippet1 and snippet2 stores values that instanceof is true/false, now it is true, it should depend on profiling
            //
            // insert castCachedClass etst
            // cmp objectClass->castCacheClass, instanceOfClass
            // je true_label
            //
            // insert isntanceof site snippet test
            // cmp objectClassReg, snippet1
            // jne later checks
            // cmp castclassreg, snippet2
            // je true_label
            // later checks
            // test if class is interface or not
            // jump to outlined label
            // test jitInstanceOf results
            // JE fail_label        // instanceof result is not true
            //
            //
            // cmp snippet1 with value -1
            // jne true_label         // snippet already updated
            // update snippet1, snippet2 with object class and instance of class
            // jmp true_label
            static char *disableFailCastClassCacheCheck = feGetEnv("TR_DisableFailCastClassCacheCheck");
            TR::MemoryReference *insideClassCacheMR = generateX86MemoryReference(objectClassReg, offsetof(J9Class, castClassCache), cg);
            if (disableFailCastClassCacheCheck)
               {
               generateMemRegInstruction(CMPMemReg(), node, insideClassCacheMR, castClassReg, cg);
               generateLabelInstruction(JE4, node, trueLabel, cg);
               }
            else
               {
               generateRegMemInstruction(LRegMem(), node, testerReg, insideClassCacheMR, cg);
               generateRegRegInstruction  (XORRegReg(), node, testerReg, castClassReg, cg);
               generateRegImmInstruction  (CMPRegImm4(),  node, testerReg, 1, cg);
               generateLabelInstruction   (JB4, node, trueLabel, cg);
               generateLabelInstruction   (JE4, node, failLabel, cg);
               }

            TR::LabelSymbol *laterCheckLabel = generateLabelSymbol(cg);
            if (TR::Compiler->target.is64Bit())
               {
               classObjectClazzSnippet = cg->create8ByteData(node, -1);
               instanceOfClazzSnippet = cg->create8ByteData(node, -1);
               }
            else
               {
               classObjectClazzSnippet = cg->create4ByteData(node, -1);
               instanceOfClazzSnippet = cg->create4ByteData(node, -1);
               }
            classObjectClazzSnippet->setClassAddress(true);
            instanceOfClazzSnippet->setClassAddress(true);
            generateMemRegInstruction(CMPMemReg(), node, generateX86MemoryReference(classObjectClazzSnippet, cg), objectClassReg, cg);
            generateLabelInstruction(JNE4, node, callHelper, cg);
            generateMemRegInstruction(CMPMemReg(), node, generateX86MemoryReference(instanceOfClazzSnippet, cg), castClassReg, cg);
            generateLabelInstruction(JE4, node, trueLabel, cg);
            generateLabelInstruction(LABEL, node, laterCheckLabel, cg);

            snippetAdded = true;
            }
         }



      generateLabelInstruction  (JMP4, node, callHelper, cg);
      generateLabelInstruction  (LABEL, node, restartLabel, cg);
      generateRegImmInstruction (TEST4RegImm4, node, testerReg, 0x1, cg);
      generateLabelInstruction  (JE4, node, failLabel, cg);

      if (addSnippet && snippetAdded)
         {
         /*
          * Update data snippet with objectClassReg and castClassReg
          * Pseudo code
          * cmp memimm, datasnippet1, -1
          * jne trueLabel     // if not -1, jump to true label, cover most cases
          * jmp outlined label
          *
          * start of out of lined code
          * eax = -1
          * lock cmpxchg data_snippet1, objectClassReg   // compare datasnippet1 with eax, if equal set ZF and store objectClassReg into data snippet1
          * jne trueLabel
          * store castClassReg into data snippet2
          * jmp trueLabel
          *
          */

         generateMemImmInstruction(CMPMemImms(), node, generateX86MemoryReference(classObjectClazzSnippet, cg), -1, cg);
         generateLabelInstruction(JNE4, node, trueLabel, cg);
         TR::LabelSymbol *slowPathLabel = generateLabelSymbol(cg);
         generateLabelInstruction(JMP4, node, slowPathLabel, cg);
         TR_OutlinedInstructions *slowPath = new (cg->trHeapMemory()) TR_OutlinedInstructions(slowPathLabel, cg);
         cg->getOutlinedInstructionsList().push_front(slowPath);
         slowPath->swapInstructionListsWithCompilation();

         // outlined code
         generateLabelInstruction(LABEL, node, slowPathLabel, cg);
         TR::Register    *eaxReg = cg->allocateRegister();
         generateRegImmInstruction(MOVRegImm4(), node, eaxReg, -1, cg);
         TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions((uint8_t)1, (uint8_t)1, cg);
         deps->addPreCondition(eaxReg, TR::RealRegister::eax, cg);
         deps->addPostCondition(eaxReg, TR::RealRegister::eax, cg);

         generateMemRegInstruction(LCMPXCHGMemReg(), node, generateX86MemoryReference(classObjectClazzSnippet, cg), objectClassReg, deps, cg);
         cg->stopUsingRegister(eaxReg);

         generateLabelInstruction(JNE4, node, trueLabel, cg);
         generateMemRegInstruction(SMemReg(), node, generateX86MemoryReference(instanceOfClazzSnippet, cg), castClassReg, cg);
         generateLabelInstruction(JMP4, node, trueLabel, cg);

         // back to mainline code
         slowPath->swapInstructionListsWithCompilation();
         }
      else
         {
         generateLabelInstruction(JMP4, node, trueLabel, cg);
         }

      //instr=generateInstruction(BADIA32Op, node, cg); // FIXME:

      generateLabelInstruction(LABEL, node, notInterfaceLabel, cg);
      }

   TR::MemoryReference *depthMR = NULL;
   bool eliminateDepthMask = (J9_JAVA_CLASS_DEPTH_MASK == 0xffff);
   if(!eliminateSuperClassArraySizeCheck)
      {
      if (eliminateDepthMask)
         {
         depthMR = generateX86MemoryReference(objectClassReg, offsetof(J9Class,classDepthAndFlags), cg);
         /*instr=generateRegMemInstruction(L2RegMem, node,
                                   testerReg,
                                   generateX86MemoryReference(objectClassReg, offsetof(J9Class,classDepthAndFlags), cg), cg);*/
         }
      else
         {
         instr=generateRegMemInstruction(LRegMem(), node,
                                   testerReg,
                                   generateX86MemoryReference(objectClassReg, offsetof(J9Class,classDepthAndFlags), cg), cg);
         }
      }

   if (TR::Compiler->target.is64Bit())
      {
      TR_ASSERT(!(J9_JAVA_CLASS_DEPTH_MASK & 0x80000000), "AMD64: need to use a second register for AND mask");
      if(!eliminateSuperClassArraySizeCheck && (!(J9_JAVA_CLASS_DEPTH_MASK & 0x80000000)) && !eliminateDepthMask)
         instr=generateRegImmInstruction(AND8RegImm4, node, testerReg, J9_JAVA_CLASS_DEPTH_MASK, cg);
      }
   else
      {
      if(!eliminateSuperClassArraySizeCheck && !eliminateDepthMask)
         instr=generateRegImmInstruction(AND4RegImm4, node, testerReg, J9_JAVA_CLASS_DEPTH_MASK, cg);
      }

   if (classDepth == -1)
      {
      classDepthReg = srm->findOrCreateScratchRegister(); // must reclaim this before each return!

      TR::MemoryReference * mr = generateX86MemoryReference(castClassReg, offsetof(J9Class,classDepthAndFlags), cg);
      if (eliminateDepthMask)
         {
         if (TR::Compiler->target.is64Bit())
            instr=generateRegMemInstruction(MOVZXReg8Mem2, node, classDepthReg, mr, cg);
         else
            instr=generateRegMemInstruction(MOVZXReg4Mem2, node, classDepthReg, mr, cg);

         instr=generateMemRegInstruction(CMP2MemReg, node, depthMR, classDepthReg, cg);
         }
      else
         {
         instr=generateRegMemInstruction(LRegMem(), node, classDepthReg, mr, cg);
         instr=generateRegImmInstruction(AND4RegImm4, node, classDepthReg, J9_JAVA_CLASS_DEPTH_MASK, cg);
         instr=generateRegRegInstruction(CMP4RegReg, node, testerReg, classDepthReg, cg);
         }
      }
   else if(!eliminateSuperClassArraySizeCheck)
      {
         if (eliminateDepthMask)
            {
            instr=generateMemImmInstruction(CMP2MemImm2, node, depthMR, classDepth, cg);
            }
         else
            {
            instr=generateRegImmInstruction(CMP4RegImm4, node, testerReg, classDepth, cg);
            }
      }


   if(!eliminateSuperClassArraySizeCheck)
      {
      if (failDeps)
         instr=generateLabelInstruction(JBE4, node, failLabel, failDeps, cg);
      else
         instr=generateLabelInstruction(JBE4, node, failLabel, cg);
      }

   // Find the superclass of the object at the cast class' depth and see if
   // it is the cast class
   //
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
   // objectClassReg contains the class offset so we may need to generate code
   // to convert from class offset to real J9Class pointer
#endif

   instr=generateRegMemInstruction(LRegMem(), node,
                             testerReg,
                             generateX86MemoryReference(objectClassReg, offsetof(J9Class,superclasses), cg), cg);

   TR::MemoryReference *superClass;

   if (classDepth == -1)
      {
      superClass = generateX86MemoryReference(testerReg, classDepthReg, TR::Compiler->target.is64Bit()?3:2, 0, cg);
      }
   else
      {
      superClass = generateX86MemoryReference(testerReg, classDepth*sizeof(J9Class*), cg);
      }

   if (castClassReg == NULL && !IS_32BIT_SIGNED((intptrj_t)castClassSym->getStaticAddress()))
      {
      castClassReg = srm->findOrCreateScratchRegister(); // must reclaim this before each return!
      castClassRegIsScratch = true;
      instr=generateRegImm64Instruction(MOV8RegImm64, node, castClassReg, (uintptrj_t)castClassSym->getStaticAddress(), cg);
      }

   if (castClassReg == NULL)
      {
      TR_ASSERT(!(castClassSymRef->isUnresolved() && (iClazz==NULL)),"Expecting class to be resolved");

      if  (castClassSymRef->isUnresolved() && (iClazz==NULL))
         {
         TR::UnresolvedDataSnippet *snippet = TR::UnresolvedDataSnippet::create(cg, node, castClassSymRef, false, castClassSymRef->canCauseGC());
         cg->addSnippet(snippet);
         TR::Instruction  *tempInstr =
            generateMemImmSnippetInstruction(CMP4MemImm4, node,
                                             superClass,
                                             castClassSymRef->getCPIndex(),
                                             snippet, cg);
         snippet->setDataReferenceInstruction(tempInstr);
         }
      else
         {
         void *classPointer = (iClazz==NULL) ? castClassSym->getStaticAddress() : iClazz;
         TR::Instruction *instr = generateMemImmSymInstruction(CMPMemImm4(use64BitClasses), node,
                                      superClass,
                                      (int32_t)(uintptrj_t)classPointer,
                                      castClassSymRef, cg);

         // HCR in genSuperClassInstanceOfTest
         if (cg->wantToPatchClassPointer((TR_OpaqueClassBlock*)classPointer, node))
            comp->getStaticHCRPICSites()->push_front(instr);
         }
      }
   else
      {
      if (needObjectClassRegClear)
         {
         generateRegRegInstruction(XOR4RegReg, node,objectClassReg, objectClassReg, cg);
         }
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
      // superClass is a memory reference to a J9Class
      // castClassReg contains a TR_OpaqueClassBlock
      // We may need to convert superClass to a class offset before doing the comparison
#endif
      if (iClazz == NULL)
         {
         instr=generateMemRegInstruction(CMPMemReg(), node, superClass, castClassReg, cg);
         }
      else
         {
         TR::Instruction *instr = generateMemImmSymInstruction(CMP4MemImm4, node,
                                      superClass,
                                      (int32_t)(uintptr_t)iClazz,
                                      castClassSymRef, cg);
         // HCR in genSuperClassInstanceOfTest
         if (cg->wantToPatchClassPointer((TR_OpaqueClassBlock*)iClazz, node))
            comp->getStaticHCRPICSites()->push_front(instr);
         }
      }

   if (castClassRegIsScratch)
      srm->reclaimScratchRegister(castClassReg);

   if (classDepthReg)
      srm->reclaimScratchRegister(classDepthReg);
   }

/*
 * generate test if object class is reference array
 * testerReg = load (objectClassReg+offset_romClass)
 * testerReg = load (testerReg+offset_modifiers)
 * test4reg J9AccClassArray
 * je failLabel
 * testerReg = load (objectClassReg + component_offset)
 * testerReg = load (objectClassReg + offset_romClass)
 * testerReg = load (objectClassReg + offset_modifiers)
 * TEST4REG, J9AccClassInternalPrimitiveType
 */
static void genIsReferenceArrayTest(TR::Node        *node,
                                    TR::Register    *objectClassReg,
                                    TR::Register    *testerReg,
                                    TR::LabelSymbol *failLabel,
                                    TR::LabelSymbol *trueLabel,
                                    TR::CodeGenerator *cg)
   {


   generateRegMemInstruction(LRegMem(), node,
                             testerReg,
                             generateX86MemoryReference(objectClassReg, offsetof(J9Class,romClass), cg), cg);

   //generateRegMemInstruction(LRegMem(), node,
   //                          testerReg,
   //                          generateX86MemoryReference(testerReg, offsetof(J9ROMClass, modifiers), cg), cg);

   generateMemImmInstruction(TESTMemImm4(), node, generateX86MemoryReference(testerReg, offsetof(J9ROMClass, modifiers), cg), J9AccClassArray, cg);
   generateLabelInstruction(JE4, node, failLabel, cg);

   generateRegMemInstruction(LRegMem(), node,
                             testerReg,
                             generateX86MemoryReference(objectClassReg, offsetof(J9ArrayClass, componentType), cg), cg);

   generateRegMemInstruction(LRegMem(), node,
                             testerReg,
                             generateX86MemoryReference(testerReg, offsetof(J9Class, romClass), cg), cg);


   generateMemImmInstruction(TESTMemImm4(), node, generateX86MemoryReference(testerReg, offsetof(J9ROMClass, modifiers), cg), J9AccClassInternalPrimitiveType, cg);

   }

static void fixupHelperCall(bool              moveFPRegSpill,
                            TR::Instruction   *startOfSequence)
   {
   // Find the call instruction and grab its dependencies
   //
   TR::Compilation *comp = TR::comp();
   TR::Instruction  *instr = comp->getAppendInstruction();
   while(instr->getOpCodeValue() == FENCE)
      instr = instr->getPrev();

   // Now find the FPREGSPILL instruction and move it to the start of this
   // whole sequence
   //
   if (moveFPRegSpill)
      {
      while(instr->getOpCodeValue() != FPREGSPILL &&
            instr != startOfSequence)
         {
         instr = instr->getPrev();
         }
      instr->move(startOfSequence);
      }
   }

// Only need a helper call if the class is not super and not final, otherwise
// it can be determined without a call-out
static bool needHelperCall(bool testCastClassIsSuper, bool isFinalClass, TR::CodeGenerator *cg)
   {
   return !testCastClassIsSuper && !isFinalClass || cg->comp()->getOption(TR_DisableInlineCheckCast);
   }

static bool needTestCache(TR::Node *node, bool cachingEnabled, bool needsHelperCall, bool superClassTest, TR::CodeGenerator *cg)
   {
   TR_OpaqueClassBlock* castClassAddr = TR::TreeEvaluator::getCastClassAddress(node);
   TR_OpaqueClassBlock* guessClassArray[NUM_PICS];
   uint8_t num_PICs = TR::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastInfo(cg, node, guessClassArray);

   return (castClassAddr || num_PICs) && !cg->comp()->getOption(TR_DisableInlineCheckCast) && cachingEnabled && needsHelperCall && !superClassTest;
   }

/*
 * Analysis if it worth to perform single profiled test and equal test before super class test.
 *
 * Idea is
 * super class test cost is
 * 1. when no class depth test (checking class depth less than mini superclass array length), 1 load + 1 cmp_mem + 1 branch
 * 2. when need class depth test and depth is known at compile time. 1 cmp_mem + 1 branch + cost in #1
 * 3. when class depth is unkown, this means checking class is unkown at compile time (arraystorechk or Class.Instance)
 *    cost is 1 load + 1 cmp_mem + 1 branch + costn in #1
 *
 * For equal test, if (benefit <= cost) then set performEqualityTest false
 *    benefit is (equal_test_hit_probaility * superclasstestcost)
 *    cost is 2 (1 cmp + 1 branch)
 *
 * same condition for single profiled test.
 */
static void benefitAnalysisOnBeforeSuperClassTest(TR::Node *castClassNode,
                                                    bool &performSingleProfiledTest,
                                                    TR::StaticSymbol *castClassSym,
                                                    TR::CodeGenerator *cg)
   {
   static char *disableTypeCheckBenefitAnalysis = feGetEnv("TR_DisableTypeCheckBenefitAnalysis");
   if (performSingleProfiledTest == false || disableTypeCheckBenefitAnalysis)
      {
      return;
      }

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR_OpaqueClassBlock* guessClassArray[NUM_PICS];
   float guessClassProbability[NUM_PICS];

   uint8_t num = TR::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastInfo(cg, castClassNode, guessClassArray, guessClassProbability, true);
   if (num == 0)
      return;

   TR_OpaqueClassBlock* castClassAddr  = TR::TreeEvaluator::getCastClassAddress(castClassNode);

   // evaluate superclasstestcost cost
   int32_t classDepth = -1;
   if (castClassSym)
      {
      TR_OpaqueClassBlock * clazz = (TR_OpaqueClassBlock *)castClassSym->getStaticAddress();
      classDepth = (int32_t)TR::Compiler->cls.classDepthOf(clazz);
      }
   int32_t superClassCheckCost = 10;
   if (classDepth != -1 && classDepth < cg->comp()->getOptions()->_minimumSuperclassArraySize)
      {
      superClassCheckCost = 5;
      }
   else if(classDepth != -1)
      {
      superClassCheckCost = 8;
      }

   // evalute profiled test benefit
   if (performSingleProfiledTest)
      {
      int32_t profiledcost = 2;
      if (profiledcost > superClassCheckCost*guessClassProbability[0])
         {
         //fprintf(stderr, "skip single profiled test %f, %d\n", guessClassProbability[0], superClassCheckCost);
         performSingleProfiledTest = false;
         }
      }
   return;
   }

static bool classAddressFitsIn32Bits(TR::Node *node,  TR::CodeGenerator *cg)
   {
   if (TR::Compiler->target.is32Bit())
      return true;

   if (cg->constantAddressesCanChangeSize(node))
      return false;

   TR_OpaqueClassBlock* castClassAddr = TR::TreeEvaluator::getCastClassAddress(node);

   return IS_32BIT_SIGNED((uintptrj_t)castClassAddr);
   }

static bool profiledAddressFitsIn32Bits(TR::Node *node,  TR::CodeGenerator *cg)
   {
   if (TR::Compiler->target.is32Bit())
      return true;

   if (cg->constantAddressesCanChangeSize(node))
      return false;

   TR_OpaqueClassBlock* guessClassArray[NUM_PICS];
   uint8_t num_PICs = TR::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastInfo(cg, node, guessClassArray);
   if (num_PICs != 0)
      return IS_32BIT_SIGNED((uintptrj_t)(guessClassArray[0]));
   else
      return FALSE;
   }

static TR::Register *generateClassCacheTest (TR::Node                *node,
                                    TR::SymbolReference     *castClassSymRef,
                                    TR::Register            *objectClassReg,
                                    TR::Register            *testRegister,
                                    TR::LabelSymbol          *slowPathCall,
                                    TR::CodeGenerator       *cg)
   {
   TR::Register *classPointerReg = NULL;
   TR_OpaqueClassBlock* castClassAddr = TR::TreeEvaluator::getCastClassAddress(node);

   TR::MemoryReference *insideClassCacheMR = generateX86MemoryReference(objectClassReg, offsetof(J9Class, castClassCache), cg);
   TR::Instruction *instr = NULL;
   TR::Compilation *comp = cg->comp();

   if (TR::Compiler->target.is32Bit())
      {
      if (castClassAddr)
         {
         instr = generateMemImmSymInstruction(CMP4MemImm4,
                                   node,
                                   insideClassCacheMR,
                                   (int32_t)(uintptr_t)castClassAddr,
                                   castClassSymRef,
                                   cg);
         }
      else
         {
         //  no need set instr, as not used later
         classPointerReg = cg->evaluate(node);
         generateMemRegInstruction(CMP4MemReg,
                                   node,
                                   insideClassCacheMR,
                                   classPointerReg,
                                   cg);
         }
      }
   else
      {
      if (testRegister)
         {
         if (castClassAddr)
            {
            instr = generateRegImm64Instruction(MOV8RegImm64, node, testRegister, (uintptr_t)castClassAddr, cg, TR_ClassAddress);

            generateMemRegInstruction(CMP8MemReg,
                                      node,
                                      insideClassCacheMR,
                                      testRegister,
                                      cg);
            }
         else
            {
            classPointerReg = cg->evaluate(node);
            generateMemRegInstruction(CMP8MemReg,
                                      node,
                                      insideClassCacheMR,
                                      classPointerReg,
                                      cg);
            }
         }
      else
         {
         if (castClassAddr)
            {
            instr = generateMemImmInstruction(CMP8MemImm4,
                                node,
                                insideClassCacheMR,
                                (int32_t)(uintptr_t)castClassAddr,
                                cg, TR_ClassAddress);
            }
         else
            {
            classPointerReg = cg->evaluate(node);
            generateMemRegInstruction(CMP8MemReg,
                                      node,
                                      insideClassCacheMR,
                                      classPointerReg,
                                      cg);
            }
         }
      }

      generateLabelInstruction(JNE4, node, slowPathCall, cg);
      // HCR in generateClassCacheTest
      if (cg->wantToPatchClassPointer(castClassAddr, node))
         comp->getStaticHCRPICSites()->push_front(instr);

      return classPointerReg;
   }

static TR::Register *generateClassCacheTestInlined (TR::Node                *node,
                                           TR::SymbolReference     *castClassSymRef,
                                           TR::Register            *objectClassReg,
                                           TR::Register            *testRegister,
                                           TR::LabelSymbol          *branchAround,
                                           TR::CodeGenerator       *cg)
   {
   TR::Register *classPointerReg = NULL;
   TR_OpaqueClassBlock* castClassAddr = TR::TreeEvaluator::getCastClassAddress(node);

   TR::MemoryReference *insideClassCacheMR = generateX86MemoryReference(objectClassReg, offsetof(J9Class, castClassCache), cg);
   TR::Instruction *instr = NULL;
   TR::Compilation *comp = cg->comp();

   if (TR::Compiler->target.is32Bit())
      {
      if (castClassAddr)
         {
         instr = generateMemImmSymInstruction(CMP4MemImm4,
                                              node,
                                              insideClassCacheMR,
                                              (int32_t)(uintptr_t)castClassAddr,
                                              castClassSymRef,
                                              cg);
         }
      else
         {
         //  no need set instr, as not used later
         classPointerReg = cg->evaluate(node);
         generateMemRegInstruction(CMP4MemReg,
                                   node,
                                   insideClassCacheMR,
                                   classPointerReg,
                                   cg);
         }
      }
   else
      {
      if (testRegister)
         {
         if (castClassAddr)
            {
            instr = generateRegImm64Instruction(MOV8RegImm64, node, testRegister, (uintptr_t)castClassAddr, cg, TR_ClassAddress);

            generateMemRegInstruction(CMP8MemReg,
                                      node,
                                      insideClassCacheMR,
                                      testRegister,
                                      cg);
            }
         else
            {
            classPointerReg = cg->evaluate(node);
            generateMemRegInstruction(CMP8MemReg,
                                      node,
                                      insideClassCacheMR,
                                      classPointerReg,
                                      cg);
            }
         }
      else
         {
         if (castClassAddr)
            {
            instr = generateMemImmInstruction(CMP8MemImm4,
                                              node,
                                              insideClassCacheMR,
                                              (int32_t)(uintptr_t)castClassAddr,
                                              cg, TR_ClassAddress);
            }
         else
            {
            classPointerReg = cg->evaluate(node);
            generateMemRegInstruction(CMP8MemReg,
                                      node,
                                      insideClassCacheMR,
                                      classPointerReg,
                                      cg);
            }
         }
      }

      generateLabelInstruction(JE4, node, branchAround, cg);

      // HCR in generateClassCacheTestInlined
      if (cg->wantToPatchClassPointer(castClassAddr, node))
         comp->getStaticHCRPICSites()->push_front(instr);
      return classPointerReg;
   }

static void generateInstanceOfCacheTest (TR::Node                *node,
                                         TR::SymbolReference     *castClassSymRef,
                                         TR::Register            *objectClassReg,
                                         TR::Register            *testRegister,
                                         TR::LabelSymbol          *falsePath,
                                         TR::LabelSymbol          *truePath,
                                         TR::LabelSymbol          *helperCall,
                                         TR::LabelSymbol          *restartLabel,
                                         TR::CodeGenerator       *cg)
   {
   TR_OpaqueClassBlock* castClassAddr = TR::TreeEvaluator::getCastClassAddress(node);

   if (!castClassAddr)
      {
      generateLabelInstruction  (JMP4, node, helperCall, cg);
      generateLabelInstruction  (LABEL, node, restartLabel, cg);
      generateRegImmInstruction (TEST4RegImm4, node, testRegister, 0x1, cg);
      return;
      }

   TR_ASSERT(testRegister != NULL, "we need test register for instanceof");

   TR::MemoryReference *insideClassCacheMR = generateX86MemoryReference(objectClassReg, offsetof(J9Class, castClassCache), cg);

   static const char *envp = NULL; // FIXME: instruction comments

   TR::Instruction *instr = NULL;
   TR::Instruction *hcrInstr = NULL;

   TR::Compilation *comp = cg->comp();

   if (TR::Compiler->target.is32Bit())
      {
      static char *disableFailCastClassCacheCheck = feGetEnv("TR_DisableFailCastClassCacheCheck");
      if (disableFailCastClassCacheCheck)
         {
         hcrInstr = generateMemImmInstruction (CMP4MemImm4,
                                            node,
                                            insideClassCacheMR,
                                            (int32_t)(uintptr_t)castClassAddr,
                                            cg,
                                            TR_ClassAddress);
         if (envp)
            {
            diagnostic("\ncast classaddr:");
            cg->getDebug()->print(comp->getOutFile(), hcrInstr);
            }

         instr = generateLabelInstruction  (JE4, node, truePath, cg);
         if (envp)
            {
            cg->getDebug()->print(comp->getOutFile(), instr);
            }
         }
      else
         {
         hcrInstr = generateRegImmInstruction (MOV4RegImm4, node, testRegister, (int32_t)(uintptr_t)castClassAddr, cg, TR_ClassAddress);
         if (envp)
            {
            diagnostic("\ncast classaddr:");
            cg->getDebug()->print(comp->getOutFile(), hcrInstr);
            }

         instr = generateRegMemInstruction  (XORRegMem(), node, testRegister, insideClassCacheMR, cg);
         if (envp)
            {
            cg->getDebug()->print(comp->getOutFile(), instr);
            }

         instr = generateRegImmInstruction  (CMPRegImm4(),  node, testRegister, 1, cg);
         if (envp)
            {
            cg->getDebug()->print(comp->getOutFile(), instr);
            }

         instr = generateLabelInstruction   (JB4, node, truePath, cg);
         if (envp)
            {
            cg->getDebug()->print(comp->getOutFile(), instr);
            }

         instr = generateLabelInstruction   (JE4, node, falsePath, cg);
         if (envp)
            {
            cg->getDebug()->print(comp->getOutFile(), instr);
            }
         }

      instr = generateLabelInstruction  (JMP4, node, helperCall, cg);
      if (envp)
         {
         cg->getDebug()->print(comp->getOutFile(), instr);
         }

      instr = generateLabelInstruction  (LABEL, node, restartLabel, cg);
      if (envp)
         {
         cg->getDebug()->print(comp->getOutFile(), instr);
         }

      instr = generateRegImmInstruction (TEST4RegImm4, node, testRegister, 0x1, cg);
      if (envp)
         {
         cg->getDebug()->print(comp->getOutFile(), instr);
         }
      }
   else
      {
      cg->generateDebugCounter(
            TR::DebugCounter::debugCounterName(comp, "instanceof:generateInstanceOfCacheTest/%s/(%s)/%d/%d", "beforeCastClassCacheCheck",
                                    comp->signature(),
                                    node->getByteCodeInfo().getCallerIndex(),
                                    node->getByteCodeInfo().getByteCodeIndex()),
            1, TR::DebugCounter::Undetermined);

      hcrInstr = generateRegImm64Instruction (MOV8RegImm64, node, testRegister, (uintptr_t)castClassAddr, cg, TR_ClassAddress);

      static char *disableFailCastClassCacheCheck = feGetEnv("TR_DisableFailCastClassCacheCheck");
      if (disableFailCastClassCacheCheck)
         {
         generateMemRegInstruction  (CMPMemReg(true), node, insideClassCacheMR, testRegister, cg);
         generateLabelInstruction   (JE4, node, truePath, cg);
         }
      else
         {
         generateRegMemInstruction  (XORRegMem(), node, testRegister, insideClassCacheMR, cg);
         generateRegImmInstruction  (CMPRegImm4(),  node, testRegister, 1, cg);
         generateLabelInstruction   (JB4, node, truePath, cg);
         generateLabelInstruction   (JE4, node, falsePath, cg);
         }

      cg->generateDebugCounter(
            TR::DebugCounter::debugCounterName(comp, "instanceof:generateInstanceOfCacheTest/%s/(%s)/%d/%d", "castCacheCheckFail",
                                    comp->signature(),
                                    node->getByteCodeInfo().getCallerIndex(),
                                    node->getByteCodeInfo().getByteCodeIndex())
            , 1, TR::DebugCounter::Undetermined);

      generateLabelInstruction   (JMP4, node, helperCall, cg);
      generateLabelInstruction   (LABEL, node, restartLabel, cg);
      generateRegImmInstruction  (TEST4RegImm4, node, testRegister, 0x1, cg);
      // HCR in generateInstanceOfCacheTest
      if (cg->wantToPatchClassPointer(castClassAddr, node))
         comp->getStaticHCRPICSites()->push_front(hcrInstr);
      }
   }

static void generateInstanceOfCacheTestInline (TR::Node                *instanceofNode,
                                               TR::Node                *node,
                                               TR::SymbolReference     *castClassSymRef,
                                               TR::Register            *objectClassReg,
                                               TR::Register            *testRegister,
                                               TR::LabelSymbol          *falsePath,
                                               TR::LabelSymbol          *truePath,
                                               TR::LabelSymbol          *restartLabel,
                                               TR::CodeGenerator       *cg)
   {
   TR_OpaqueClassBlock* castClassAddr = TR::TreeEvaluator::getCastClassAddress(node);
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

   if (castClassAddr)
      {
      TR::MemoryReference *insideClassCacheMR = generateX86MemoryReference(objectClassReg, offsetof(J9Class, castClassCache), cg);

      static char *disableFailCastClassCacheCheck = feGetEnv("TR_DisableFailCastClassCacheCheck");
      TR::Instruction *instr = generateRegImm64Instruction(MOV8RegImm64, node, testRegister, (uintptrj_t)castClassAddr, cg, TR_ClassAddress);
      if (disableFailCastClassCacheCheck)
         {
         generateMemRegInstruction  (CMPMemReg(true), node, insideClassCacheMR, testRegister, cg);
         generateLabelInstruction  (JE4, node, truePath, cg);
         }
      else
         {
         generateRegMemInstruction  (XORRegMem(), node, testRegister, insideClassCacheMR, cg);
         generateRegImmInstruction  (CMPRegImm4(),  node, testRegister, 1, cg);
         generateLabelInstruction   (JB4, node, truePath, cg);
         generateLabelInstruction   (JE4, node, falsePath, cg);
         // keep checking class in eax before call
         TR::Instruction *instr2 = generateRegImm64Instruction(MOV8RegImm64, node, testRegister, (uintptrj_t)castClassAddr, cg, TR_ClassAddress);
         if (cg->wantToPatchClassPointer(castClassAddr, node))
            {
            comp->getStaticHCRPICSites()->push_front(instr2);
            }
         }

      if (cg->wantToPatchClassPointer(castClassAddr, node))
         {
         comp->getStaticHCRPICSites()->push_front(instr);
         }
      }
   else
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      TR::MemoryReference *memRef = generateX86MemoryReference(symRef, cg);
      TR_X86OpCodes op = LEARegMem();

      if (TR::Compiler->om.generateCompressedObjectHeaders())
         op = LEA4RegMem;

      TR::Instruction *instr = generateRegMemInstruction(op, node, testRegister, memRef, cg);
      if (cg->wantToPatchClassPointer(castClassAddr, node))
         {
         // devinmp: I think this has no effect; instr has no immediate source operand.
         comp->getStaticHCRPICSites()->push_front(instr);
         }
      }

   TR::MethodSymbol *helperMethod = instanceofNode->getSymbol()->castToMethodSymbol();
   TR::Instruction *instr = generateImmSymInstruction(CALLImm4, node, (uint32_t)(uintptrj_t)helperMethod->getMethodAddress(), instanceofNode->getSymbolReference(), cg);
   instr->setNeedsGCMap(0xFF00FFFF);

   generateLabelInstruction   (LABEL, node, restartLabel, cg);
   generateRegImmInstruction  (TEST4RegImm4, node, testRegister, 0x1, cg);
   }

static TR_AtomicRegion ia32VPicProfiledAtomicRegions[] =
   {
   { 0x02, 4 }, // Class test immediate 1
   { 0,0 }      // (null terminator)
   };

static TR_AtomicRegion amd64VPicAtomicRegions[] =
   {
   { 0x02, 8 }, // Class test immediate 1
   { 0,0 }      // (null terminator)
   };

static void generateProfiledCacheTest (TR::Node                *node,
                                       TR::SymbolReference     *castClassSymRef,
                                       TR::Register            *objectClassReg,
                                       TR::Register            *testRegister,
                                       TR::LabelSymbol          *falsePath,
                                       TR::LabelSymbol          *truePath,
                                       bool                    isCheckCast,
                                       TR::CodeGenerator       *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR_OpaqueClassBlock* castClassAddr  = TR::TreeEvaluator::getCastClassAddress(node);
   TR_OpaqueClassBlock* guessClassArray[NUM_PICS];
   uint8_t num_PICs = 0;

   if (!castClassAddr)
      {
      return;
      }

   if (isCheckCast)
      {
      TR_OpaqueClassBlock *tmpClassArray[NUM_PICS];
      uint8_t num = TR::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastInfo(cg, node, tmpClassArray);
      TR_OpaqueClassBlock *tmpClassAddr;
      for (uint8_t i = 0; i < num; i++)
         {
         tmpClassAddr = tmpClassArray[i];
         if (instanceOfOrCheckCast((J9Class*)tmpClassAddr, (J9Class*)castClassAddr))
            guessClassArray[num_PICs++] = tmpClassAddr;
         }
      }
   else
      num_PICs = TR::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastInfo(cg, node, guessClassArray);

   if (num_PICs == 0)
      {
      return;
      }

   TR::Instruction *staticPIC[NUM_PICS];
   int32_t result_value = -1;
   TR::LabelSymbol *result_label;
   for (uint8_t i = 0; i < num_PICs; i++)
      {
      if (TR::Compiler->target.is32Bit())
         {
         staticPIC[i] = generateRegImmInstruction(CMP4RegImm4,
                                node,
                                objectClassReg,
                                (int32_t)(uintptr_t)(guessClassArray[i]),
                                cg);
         if (fej9->isUnloadAssumptionRequired(guessClassArray[i], comp->getCurrentMethod()))
            comp->getStaticPICSites()->push_front(staticPIC[i]);
         // HCR in generateProfiledCacheTest
         if (cg->wantToPatchClassPointer(guessClassArray[i], node))
            comp->getStaticHCRPICSites()->push_front(staticPIC[i]);

         // disable PCAI, because class address is only modified when hook happen
         //if (TR::Compiler->target.isSMP())
         //   generatePatchableCodeAlignmentInstruction(ia32VPicProfiledAtomicRegions, staticPIC[i], cg);
         }
      else
         {
         staticPIC[i] = NULL;

         if (testRegister)
            staticPIC[i] = generateRegImm64Instruction(MOV8RegImm64, node, testRegister, (uintptrj_t)guessClassArray[i], cg);
         else
            staticPIC[i] = generateRegImmInstruction(CMP8RegImm4, node, objectClassReg, (int32_t)(uintptr_t)guessClassArray[i], cg);

         // disable PCAI
         //if (TR::Compiler->target.isSMP())
         //   generatePatchableCodeAlignmentInstruction(amd64VPicAtomicRegions, staticPIC[i], cg);

         if (testRegister)
            generateRegRegInstruction(CMP8RegReg,
                                node,
                                objectClassReg,
                                testRegister,
                                cg);

         if (fej9->isUnloadAssumptionRequired(guessClassArray[i], comp->getCurrentMethod()))
            comp->getStaticPICSites()->push_front(staticPIC[i]);

         // HCR in generateProfiledCacheTest
         if (cg->wantToPatchClassPointer(guessClassArray[i], node))
           comp->getStaticHCRPICSites()->push_front(staticPIC[i]);
         }
      result_value = (int32_t)instanceOfOrCheckCast((J9Class*)guessClassArray[i], (J9Class*)castClassAddr);
      result_label = isCheckCast ? truePath : (result_value == 1 ? truePath : falsePath);
      generateLabelInstruction(JE4, node, result_label, cg);
      }
   }


static bool generateSingleProfiledCacheTest (
                                       TR::Node                *node,
                                       TR::SymbolReference     *castClassSymRef,
                                       TR::Register            *objectClassReg,
                                       TR::LabelSymbol          *falsePath,
                                       TR::LabelSymbol          *truePath,
                                       bool                    isCheckCast,
                                       TR::CodeGenerator       *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR_OpaqueClassBlock* castClassAddr  = TR::TreeEvaluator::getCastClassAddress(node);
   TR_OpaqueClassBlock* guessClassArray[NUM_PICS];
   uint8_t num_PICs = 0;

   if (!castClassAddr)
      {
      return false;
      }

   if (isCheckCast)
      {
      TR_OpaqueClassBlock *tmpClassArray[NUM_PICS];
      uint8_t num = TR::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastInfo(cg, node, tmpClassArray);
      TR_OpaqueClassBlock *tmpClassAddr;
      for (uint8_t i = 0; i < num; i++)
         {
         tmpClassAddr = tmpClassArray[i];
         if (instanceOfOrCheckCast((J9Class*)tmpClassAddr, (J9Class*)castClassAddr))
            guessClassArray[num_PICs++] = tmpClassAddr;
         }
      }
   else
      num_PICs = TR::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastInfo(cg, node, guessClassArray);

   if (num_PICs == 0)
      {
      return false;
      }

   // skip single profiled test when cast class same with profiled class
   // because cast class will perform equal test
   if ((uintptr_t)castClassAddr == (uintptr_t)guessClassArray[0])
      {
      return false;
      }

   TR::Instruction *staticPIC[NUM_PICS];
   int32_t result_value = -1;
   TR::LabelSymbol *result_label;
   int i = 0;
   if (TR::Compiler->target.is32Bit())
      {
      staticPIC[i] = generateRegImmInstruction(CMP4RegImm4,
                             node,
                             objectClassReg,
                             (int32_t)(uintptr_t)(guessClassArray[i]),
                             cg);
      if (fej9->isUnloadAssumptionRequired(guessClassArray[i], comp->getCurrentMethod()))
         comp->getStaticPICSites()->push_front(staticPIC[i]);
      if (cg->wantToPatchClassPointer(guessClassArray[i], node))
         comp->getStaticHCRPICSites()->push_front(staticPIC[i]);
      // disable PCAI, class address will not modified when jitted code.
      //if (TR::Compiler->target.isSMP())
      //   generatePatchableCodeAlignmentInstruction(ia32VPicProfiledAtomicRegions, staticPIC[i], cg);
      }
   else
      {
      staticPIC[i] = generateRegImmInstruction(CMP8RegImm4, node, objectClassReg, (int32_t)(uintptr_t)guessClassArray[i], cg);

      // disable PCAI, class address will not modified when jitted code.
      //if (TR::Compiler->target.isSMP())
      //   generatePatchableCodeAlignmentInstruction(amd64VPicAtomicRegions, staticPIC[i], cg);

      if (fej9->isUnloadAssumptionRequired(guessClassArray[i], comp->getCurrentMethod()))
         comp->getStaticPICSites()->push_front(staticPIC[i]);

      if (cg->wantToPatchClassPointer(guessClassArray[i], node))
        comp->getStaticHCRPICSites()->push_front(staticPIC[i]);
      }

   result_value = (int32_t)instanceOfOrCheckCast((J9Class*)guessClassArray[i], (J9Class*)castClassAddr);
   result_label = isCheckCast ? truePath : (result_value == 1 ? truePath : falsePath);
   generateLabelInstruction(JE4, node, result_label, cg);
   return true;
   }




/*
 * J9 X86 specific tree evaluator table overrides
 */
extern void TEMPORARY_initJ9X86TreeEvaluatorTable(TR::CodeGenerator *cg)
   {
   TR_TreeEvaluatorFunctionPointer *tet = cg->getTreeEvaluatorTable();

   tet[TR::wrtbar] =                TR::TreeEvaluator::writeBarrierEvaluator;
   tet[TR::wrtbari] =               TR::TreeEvaluator::writeBarrierEvaluator;
   tet[TR::monent] =                TR::TreeEvaluator::monentEvaluator;
   tet[TR::monexit] =               TR::TreeEvaluator::monexitEvaluator;
   tet[TR::monexitfence] =          TR::TreeEvaluator::monexitfenceEvaluator;
   tet[TR::asynccheck] =            TR::TreeEvaluator::asynccheckEvaluator;
   tet[TR::instanceof] =            TR::TreeEvaluator::instanceofEvaluator;
   tet[TR::checkcast] =             TR::TreeEvaluator::checkcastEvaluator;
   tet[TR::checkcastAndNULLCHK] =   TR::TreeEvaluator::checkcastAndNULLCHKEvaluator;
   tet[TR::New] =                   TR::TreeEvaluator::newEvaluator;
   tet[TR::newarray] =              TR::TreeEvaluator::newEvaluator;
   tet[TR::anewarray] =             TR::TreeEvaluator::newEvaluator;
   tet[TR::variableNew] =           TR::TreeEvaluator::newEvaluator;
   tet[TR::variableNewArray] =      TR::TreeEvaluator::newEvaluator;
   tet[TR::multianewarray]        = TR::TreeEvaluator::multianewArrayEvaluator;
   tet[TR::arraylength] =           TR::TreeEvaluator::arraylengthEvaluator;
   tet[TR::lookup] =                TR::TreeEvaluator::lookupEvaluator;
   tet[TR::exceptionRangeFence] =   TR::TreeEvaluator::exceptionRangeFenceEvaluator;
   tet[TR::dbgFence] =              TR::TreeEvaluator::exceptionRangeFenceEvaluator;
   tet[TR::NULLCHK] =               TR::TreeEvaluator::NULLCHKEvaluator;
   tet[TR::ZEROCHK] =               TR::TreeEvaluator::ZEROCHKEvaluator;
   tet[TR::ResolveCHK] =            TR::TreeEvaluator::resolveCHKEvaluator;
   tet[TR::ResolveAndNULLCHK] =     TR::TreeEvaluator::resolveAndNULLCHKEvaluator;
   tet[TR::DIVCHK] =                TR::TreeEvaluator::DIVCHKEvaluator;
   tet[TR::BNDCHK] =                TR::TreeEvaluator::BNDCHKEvaluator;
   tet[TR::ArrayCopyBNDCHK] =       TR::TreeEvaluator::ArrayCopyBNDCHKEvaluator;
   tet[TR::BNDCHKwithSpineCHK] =    TR::TreeEvaluator::BNDCHKwithSpineCHKEvaluator;
   tet[TR::SpineCHK] =              TR::TreeEvaluator::BNDCHKwithSpineCHKEvaluator;
   tet[TR::ArrayStoreCHK] =         TR::TreeEvaluator::ArrayStoreCHKEvaluator;
   tet[TR::ArrayCHK] =              TR::TreeEvaluator::ArrayCHKEvaluator;
   tet[TR::MethodEnterHook] =       TR::TreeEvaluator::conditionalHelperEvaluator;
   tet[TR::MethodExitHook] =        TR::TreeEvaluator::conditionalHelperEvaluator;
   tet[TR::allocationFence] =       TR::TreeEvaluator::NOPEvaluator;
   tet[TR::loadFence] =             TR::TreeEvaluator::barrierFenceEvaluator;
   tet[TR::storeFence] =            TR::TreeEvaluator::barrierFenceEvaluator;
   tet[TR::fullFence] =             TR::TreeEvaluator::barrierFenceEvaluator;
   tet[TR::ihbit] =                 TR::TreeEvaluator::integerHighestOneBit;
   tet[TR::ilbit] =                 TR::TreeEvaluator::integerLowestOneBit;
   tet[TR::inolz] =                 TR::TreeEvaluator::integerNumberOfLeadingZeros;
   tet[TR::inotz] =                 TR::TreeEvaluator::integerNumberOfTrailingZeros;
   tet[TR::ipopcnt] =               TR::TreeEvaluator::integerBitCount;
   tet[TR::lhbit] =                 TR::TreeEvaluator::longHighestOneBit;
   tet[TR::llbit] =                 TR::TreeEvaluator::longLowestOneBit;
   tet[TR::lnolz] =                 TR::TreeEvaluator::longNumberOfLeadingZeros;
   tet[TR::lnotz] =                 TR::TreeEvaluator::longNumberOfTrailingZeros;
   tet[TR::lpopcnt] =               TR::TreeEvaluator::longBitCount;
   tet[TR::tstart] =                TR::TreeEvaluator::tstartEvaluator;
   tet[TR::tfinish] =               TR::TreeEvaluator::tfinishEvaluator;
   tet[TR::tabort] =                TR::TreeEvaluator::tabortEvaluator;

#if defined(TR_TARGET_32BIT)
   // 32-bit overrides
   tet[TR::ldiv] =                  TR::TreeEvaluator::integerPairDivEvaluator;
   tet[TR::lrem] =                  TR::TreeEvaluator::integerPairRemEvaluator;
#endif
   }


static void generateCommonLockNurseryCodes(TR::Node          *node,
                                     TR::CodeGenerator *cg,
                                     bool              monent, //true for VMmonentEvaluator, false for VMmonexitEvaluator
                                     TR::LabelSymbol    *monitorLookupCacheLabel,
                                     TR::LabelSymbol    *fallThruFromMonitorLookupCacheLabel,
                                     TR::LabelSymbol    *snippetLabel,
                                     uint32_t         &numDeps,
                                     int              &lwOffset,
                                     TR::Register      *objectClassReg,
                                     TR::Register      *&lookupOffsetReg,
                                     TR::Register      *vmThreadReg,
                                     TR::Register      *objectReg
                                     )
   {
#if !defined(J9VM_THR_LOCK_NURSERY)
   TR_ASSERT(0, "the x86 evaluator requires lock nursery to be defined, otherwise you'll need to make sure the code below works");
#endif
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   if (comp->getOption(TR_EnableMonitorCacheLookup))
      {
      if (monent) lwOffset = 0;
      generateLabelInstruction(JLE4, node, monitorLookupCacheLabel, cg);
      generateLabelInstruction(JMP4, node, fallThruFromMonitorLookupCacheLabel, cg);

      generateLabelInstruction(LABEL, node, monitorLookupCacheLabel, cg);

      lookupOffsetReg = cg->allocateRegister();
      numDeps++;

      int32_t offsetOfMonitorLookupCache = offsetof(J9VMThread, objectMonitorLookupCache);

      //generateRegMemInstruction(LRegMem(), node, objectClassReg, generateX86MemoryReference(vmThreadReg, offsetOfMonitorLookupCache, cg), cg);
      generateRegRegInstruction(MOVRegReg(), node, lookupOffsetReg, objectReg, cg);

      generateRegImmInstruction(SARRegImm1(TR::Compiler->target.is64Bit()), node, lookupOffsetReg, trailingZeroes(fej9->getObjectAlignmentInBytes()), cg);

      J9JavaVM * jvm = fej9->getJ9JITConfig()->javaVM;
      generateRegImmInstruction(ANDRegImms(), node, lookupOffsetReg, J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE - 1, cg);
      generateRegImmInstruction(SHLRegImm1(), node, lookupOffsetReg, trailingZeroes(TR::Compiler->om.sizeofReferenceField()), cg);
      generateRegMemInstruction((TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord()) ? L4RegMem : LRegMem(), node, objectClassReg, generateX86MemoryReference(vmThreadReg, lookupOffsetReg, 0, offsetOfMonitorLookupCache, cg), cg);

      generateRegRegInstruction(TESTRegReg(), node, objectClassReg, objectClassReg, cg);
      generateLabelInstruction(JE4, node, snippetLabel, cg);

      int32_t offsetOfMonitor = offsetof(J9ObjectMonitor, monitor);
      generateRegMemInstruction(LRegMem(), node, lookupOffsetReg, generateX86MemoryReference(objectClassReg, offsetOfMonitor, cg), cg);

      int32_t offsetOfUserData = offsetof(J9ThreadAbstractMonitor, userData);
      generateRegMemInstruction(LRegMem(), node, lookupOffsetReg, generateX86MemoryReference(lookupOffsetReg, offsetOfUserData, cg), cg);

      generateRegRegInstruction(CMPRegReg(), node, lookupOffsetReg, objectReg, cg);
      generateLabelInstruction(JNE4, node, snippetLabel, cg);

      int32_t offsetOfAlternateLockWord = offsetof(J9ObjectMonitor, alternateLockword);
      //generateRegMemInstruction(LRegMem(), node, lookupOffsetReg, generateX86MemoryReference(objectClassReg, offsetOfAlternateLockWord, cg), cg);
      generateRegImmInstruction(ADDRegImms(), node, objectClassReg, offsetOfAlternateLockWord, cg);
      //generateRegRegInstruction(ADDRegReg(), node, objectClassReg, lookupOffsetReg, cg);
      generateRegRegInstruction(SUBRegReg(), node, objectClassReg, objectReg, cg);

      generateLabelInstruction(LABEL, node, fallThruFromMonitorLookupCacheLabel, cg);
      }
   else
      generateLabelInstruction(JLE4, node, snippetLabel, cg);
   }

#ifdef TR_TARGET_32BIT
TR::Register *J9::X86::i386::TreeEvaluator::conditionalHelperEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // used by asynccheck, methodEnterhook, and methodExitHook

   // Decrement the reference count on the constant placeholder parameter to
   // the MethodEnterHook call.  An evaluation isn't necessary because the
   // constant value isn't used here.
   //
   if (node->getOpCodeValue() == TR::MethodEnterHook)
      {
      if (node->getSecondChild()->getOpCode().isCall() &&
          node->getSecondChild()->getNumChildren() > 1)
         {
         cg->decReferenceCount(node->getSecondChild()->getFirstChild());
         }
      }

   // The child contains an inline test.
   //
   TR::Node *testNode = node->getFirstChild();
   TR::Node *secondChild = testNode->getSecondChild();
   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      int32_t      value            = secondChild->getInt();
      TR::Node     *firstChild       = testNode->getFirstChild();
      TR_X86OpCodes opCode;
      if (value >= -128 && value <= 127)
         opCode = CMP4MemImms;
      else
         opCode = CMP4MemImm4;
      TR::MemoryReference * memRef =  generateX86MemoryReference(firstChild, cg);
      generateMemImmInstruction(opCode, node, memRef, value, cg);
      memRef->decNodeReferenceCounts(cg);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);
      temp.integerCompareAnalyser(testNode, CMP4RegReg, CMP4RegMem, CMP4MemReg);
      }

   cg->setVMThreadRequired(true);

   TR::LabelSymbol *startLabel   = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *reStartLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *snippetLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   startLabel->setStartInternalControlFlow();
   reStartLabel->setEndInternalControlFlow();
   generateLabelInstruction(LABEL, node, startLabel, cg);
   generateLabelInstruction(testNode->getOpCodeValue() == TR::icmpeq ? JE4 : JNE4, node, snippetLabel, true, cg);

   TR::Snippet *snippet;
   if (node->getNumChildren() == 2)
      snippet = new (cg->trHeapMemory()) TR::X86HelperCallSnippet(cg, reStartLabel, snippetLabel, node->getSecondChild());
   else
      snippet = new (cg->trHeapMemory()) TR::X86HelperCallSnippet(cg, node, reStartLabel, snippetLabel, node->getSymbolReference());

   cg->addSnippet(snippet);

   generateLabelInstruction(LABEL, node, reStartLabel, true, cg);
   cg->setVMThreadRequired(false);
   cg->decReferenceCount(testNode);
   return NULL;
   }
#endif


#ifdef TR_TARGET_64BIT
// Hack markers
#define JVMPI_HOOKS_WITHOUT_SNIPPETS (!debug("enableSnippetsForJVMPI")) // Snippets can't yet do the necessary linkage logic


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

   // Decrement the reference count on the constant placeholder parameter to
   // the MethodEnterHook call.  An evaluation isn't necessary because the
   // constant value isn't used here.
   //
   if (node->getOpCodeValue() == TR::MethodEnterHook && !JVMPI_HOOKS_WITHOUT_SNIPPETS)
      {
      if (node->getSecondChild()->getOpCode().isCall() &&
          node->getSecondChild()->getNumChildren() > 1)
         {
         cg->decReferenceCount(node->getSecondChild()->getFirstChild());
         }
      }

   // The child contains an inline test.
   //
   TR::Node *testNode    = node->getFirstChild();
   TR::Node *secondChild = testNode->getSecondChild();
   bool testIs64Bit     = TR::TreeEvaluator::getNodeIs64Bit(secondChild, cg);
   bool testIsEQ        = testNode->getOpCodeValue() == TR::icmpeq || testNode->getOpCodeValue() == TR::lcmpeq;

   TR::Register *thisReg = NULL;
   TR::Register *ramMethodReg = NULL;

   // The receiver and RAM method parameters must be evaluated outside of the internal control flow region if it is commoned,
   // and their registers added to the post dependency condition on the merge label.
   //
   // The reference counts will be decremented when the call node is evaluated.
   //
   if (JVMPI_HOOKS_WITHOUT_SNIPPETS &&
       (node->getOpCodeValue() == TR::MethodEnterHook || node->getOpCodeValue() == TR::MethodExitHook))
      {
      TR::Node *callNode = node->getSecondChild();

      if (callNode->getNumChildren() > 1)
         {
         if (callNode->getFirstChild()->getReferenceCount() > 1)
            thisReg = cg->evaluate(callNode->getFirstChild());

         if (callNode->getSecondChild()->getReferenceCount() > 1)
            ramMethodReg = cg->evaluate(callNode->getSecondChild());
         }
      else
         {
         if (callNode->getFirstChild()->getReferenceCount() > 1)
            ramMethodReg = cg->evaluate(callNode->getFirstChild());
         }
      }

   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL &&
       (!testIs64Bit || IS_32BIT_SIGNED(secondChild->getLongInt())))
      {
      // Try to compare memory directly with immediate
      //
      TR::MemoryReference * memRef = generateX86MemoryReference(testNode->getFirstChild(), cg);
      TR_X86OpCodes op;

      if (testIs64Bit)
         {
         int64_t value = secondChild->getLongInt();
         op = IS_8BIT_SIGNED(value) ? CMP8MemImms : CMP8MemImm4;
         generateMemImmInstruction(op, node, memRef, value, cg);
         }
      else
         {
         int32_t value = secondChild->getInt();
         op = IS_8BIT_SIGNED(value) ? CMP4MemImms : CMP4MemImm4;
         generateMemImmInstruction(op, node, memRef, value, cg);
         }

      memRef->decNodeReferenceCounts(cg);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);
      temp.integerCompareAnalyser(testNode, CMPRegReg(testIs64Bit), CMPRegMem(testIs64Bit), CMPMemReg(testIs64Bit));
      }

   cg->setVMThreadRequired(true);

   TR::LabelSymbol *startLabel   = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *reStartLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *snippetLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   startLabel->setStartInternalControlFlow();
   reStartLabel->setEndInternalControlFlow();

   TR::Instruction *startInstruction = generateLabelInstruction(LABEL, node, startLabel, cg);

   if (JVMPI_HOOKS_WITHOUT_SNIPPETS &&
       (node->getOpCodeValue() == TR::MethodEnterHook || node->getOpCodeValue() == TR::MethodExitHook))
      {
      TR::Node *callNode = node->getSecondChild();

      // Generate an inverted jump around the call.  This is necessary because we want to do the call inline rather
      // than through the snippet.
      //
      generateLabelInstruction(testIsEQ? JNE4 : JE4, node, reStartLabel, true, cg);
      TR::TreeEvaluator::performCall(callNode, false, false, cg);

      // Collect postconditions from the internal control flow region and put
      // them on the restart label to prevent spills in the internal control
      // flow region.
      // TODO:AMD64: This would be a useful general facility to have.
      //
      TR::Machine *machine = cg->machine();
      TR::RegisterDependencyConditions  *postConditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions((uint8_t)0, TR::RealRegister::NumRegisters, cg->trMemory());
      TR::Compilation *comp = cg->comp();
      if (thisReg)
         postConditions->addPostCondition(thisReg, TR::RealRegister::NoReg, cg);

      if (ramMethodReg)
         postConditions->addPostCondition(ramMethodReg, TR::RealRegister::NoReg, cg);

      for (TR::Instruction *cursor = comp->getAppendInstruction(); cursor != startInstruction; cursor = cursor->getPrev())
         {
         TR::RegisterDependencyConditions  *cursorDeps = cursor->getDependencyConditions();
         if (cursorDeps && cursor->getOpCodeValue() != ASSOCREGS)
            {
            if (debug("traceConditionalHelperEvaluator"))
               {
               diagnostic("conditionalHelperEvaluator: Adding deps from " POINTER_PRINTF_FORMAT "\n", cursor);
               }
            for (int32_t i = 0; i < cursorDeps->getNumPostConditions(); i++)
               {
               TR::RegisterDependency  *cursorPostCondition = cursorDeps->getPostConditions()->getRegisterDependency(i);
               postConditions->unionPostCondition(cursorPostCondition->getRegister(), cursorPostCondition->getRealRegister(), cg);
               if (debug("traceConditionalHelperEvaluator"))
                  {
                  TR_Debug *debug = cg->getDebug();
                  diagnostic("conditionalHelperEvaluator:    [%s : %s]\n", debug->getName(cursorPostCondition->getRegister()), debug->getName(machine->getX86RealRegister(cursorPostCondition->getRealRegister())));
                  }
               }
            }
         }
      postConditions->stopAddingPostConditions();

      generateLabelInstruction(LABEL, node, reStartLabel, postConditions, cg);
      }
   else
      {
      generateLabelInstruction(testIsEQ? JE4 : JNE4, node, snippetLabel, true, cg);

      TR::Snippet *snippet;
      if (node->getNumChildren() == 2)
         snippet = new (cg->trHeapMemory()) TR::X86HelperCallSnippet(cg, reStartLabel, snippetLabel, node->getSecondChild());
      else
         snippet = new (cg->trHeapMemory()) TR::X86HelperCallSnippet(cg, node, reStartLabel, snippetLabel, node->getSymbolReference());

      cg->addSnippet(snippet);
      generateLabelInstruction(LABEL, node, reStartLabel, true, cg);
      }

   cg->setVMThreadRequired(false);
   cg->decReferenceCount(testNode);
   return NULL;
   }
#endif


// Should only be called for pure TR::wrtbar and TR::wrtbari nodes.
//
TR::Register *J9::X86::TreeEvaluator::writeBarrierEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::MemoryReference *storeMR = generateX86MemoryReference(node, cg);
   TR::Node               *destOwningObject;
   TR::Node               *sourceObject;
   TR::Compilation *comp = cg->comp();
   bool                   usingCompressedPointers = false;
   bool                   usingLowMemHeap = false;
   bool                   useShiftedOffsets = (TR::Compiler->om.compressedReferenceShiftOffset() != 0);

   if (node->getOpCodeValue() == TR::wrtbari)
      {
      destOwningObject = node->getChild(2);
      sourceObject = node->getSecondChild();
      if (comp->useCompressedPointers() &&
          (node->getSymbolReference()->getSymbol()->getDataType() == TR::Address) &&
          (node->getSecondChild()->getDataType() != TR::Address))
         {
         // pattern match the sequence
         //     iwrtbar f     iwrtbar f         <- node
         //       aload O       aload O
         //     value           l2i
         //                       lshr
         //                         lsub        <- translatedNode
         //                           a2l
         //                             value   <- sourceObject
         //                           lconst HB
         //                         iconst shftKonst
         //
         // -or- if the field is known to be null or usingLowMemHeap
         // iwrtbar f
         //    aload O
         //    l2i
         //      a2l
         //        value  <- sourceObject
         //
         TR::Node *translatedNode = sourceObject;
         if (translatedNode->getOpCode().isConversion())
            translatedNode = translatedNode->getFirstChild();
         if (translatedNode->getOpCode().isRightShift()) // optional
            translatedNode = translatedNode->getFirstChild();

         if (TR::Compiler->vm.heapBaseAddress() == 0 ||
             sourceObject->isNull())
            usingLowMemHeap = true;

         if (translatedNode->getOpCode().isSub() || usingLowMemHeap)
            usingCompressedPointers = true;

         if (usingCompressedPointers)
            {
            if (!usingLowMemHeap || useShiftedOffsets)
               {
               while ((sourceObject->getNumChildren() > 0) &&
                        (sourceObject->getOpCodeValue() != TR::a2l))
                  sourceObject = sourceObject->getFirstChild();
               if (sourceObject->getOpCodeValue() == TR::a2l)
               sourceObject = sourceObject->getFirstChild();
               // this is required so that different registers are
               // allocated for the actual store and translated values
               sourceObject->incReferenceCount();
               }
            }
         }
      }
   else
      {
      TR_ASSERT((node->getOpCodeValue() == TR::wrtbar), "expecting a TR::wrtbar");
      destOwningObject = node->getSecondChild();
      sourceObject = node->getFirstChild();
      }

   TR_X86ScratchRegisterManager *scratchRegisterManager =
      cg->generateScratchRegisterManager(TR::Compiler->target.is64Bit() ? 15 : 7);

   TR::TreeEvaluator::VMwrtbarWithStoreEvaluator(
      node,
      storeMR,
      scratchRegisterManager,
      destOwningObject,
      sourceObject,
      (node->getOpCodeValue() == TR::wrtbari) ? true : false,
      cg,
      false);

   if (comp->useAnchors() && (node->getOpCodeValue() == TR::wrtbari))
      node->setStoreAlreadyEvaluated(true);

   if (usingCompressedPointers)
      cg->decReferenceCount(node->getSecondChild());

   return NULL;
   }


TR::Register *J9::X86::TreeEvaluator::monentEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->enableRematerialisation() &&
       cg->supportsStaticMemoryRematerialization())
      TR::TreeEvaluator::removeLiveDiscardableStatics(cg);

   return TR::TreeEvaluator::VMmonentEvaluator(node, cg);
   }

TR::Register *J9::X86::TreeEvaluator::monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->enableRematerialisation() &&
       cg->supportsStaticMemoryRematerialization())
      TR::TreeEvaluator::removeLiveDiscardableStatics(cg);

   return TR::TreeEvaluator::VMmonexitEvaluator(node, cg);
   }

TR::Register *J9::X86::TreeEvaluator::monexitfenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *J9::X86::TreeEvaluator::asynccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // Generate the test and branch for async message processing.
   //
   cg->setVMThreadRequired(true);

   TR::Node *compareNode = node->getFirstChild();
   TR::Node *secondChild = compareNode->getSecondChild();
   TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
   TR::Compilation *comp = cg->comp();

   if (comp->getOption(TR_RTGCMapCheck))
      {
      TR::TreeEvaluator::asyncGCMapCheckPatching(node, cg, snippetLabel);
      }
   else if (secondChild->getOpCode().isLoadConst())
      {
      TR::MemoryReference *mr = generateX86MemoryReference(compareNode->getFirstChild(), cg);

      if ((secondChild->getRegister() != NULL) ||
          (TR::Compiler->target.is64Bit() && !IS_32BIT_SIGNED(secondChild->getLongInt())))
         {
         TR::Register *valueReg = cg->evaluate(secondChild);
         TR::X86CheckAsyncMessagesMemRegInstruction *ins =
            generateCheckAsyncMessagesInstruction(node, CMPMemReg(), mr, valueReg, cg);
         }
      else
         {
         int32_t value = secondChild->getInt();
         TR_X86OpCodes op = (value < 127 && value >= -128) ? CMPMemImms() : CMPMemImm4();
         TR::X86CheckAsyncMessagesMemImmInstruction *ins =
            generateCheckAsyncMessagesInstruction(node, op, mr, value, cg);
         }

      mr->decNodeReferenceCounts(cg);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_ASSERT(secondChild->getOpCode().isLoadConst(), "unrecognized asynccheck test: special async check value is not a constant");
      return NULL;
      }

   TR::LabelSymbol *startControlFlowLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *endControlFlowLabel = generateLabelSymbol(cg);

   bool testIsEqual = compareNode->getOpCodeValue() == TR::icmpeq || compareNode->getOpCodeValue() == TR::lcmpeq;

   TR_ASSERT(testIsEqual, "unrecognized asynccheck test: test is not equal");

   startControlFlowLabel->setStartInternalControlFlow();
   generateLabelInstruction(LABEL, node, startControlFlowLabel, cg);

   generateLabelInstruction(testIsEqual ? JE4 : JNE4, node, snippetLabel, true, cg);

   TR::Snippet *snippet = new (cg->trHeapMemory()) TR::X86CheckAsyncMessagesSnippet(node, endControlFlowLabel, snippetLabel, cg);
   cg->addSnippet(snippet);

   endControlFlowLabel->setEndInternalControlFlow();
   generateLabelInstruction(LABEL, node, endControlFlowLabel, true, cg);

   cg->setVMThreadRequired(false);
   cg->decReferenceCount(compareNode);

   return NULL;
   }

// For instanceof a boolean value is generated into the target register;
// the case where this is a child of an "ificmpxx" is assumed to have been
// handled by that node.
//
TR::Register *J9::X86::TreeEvaluator::instanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::VMinstanceOfEvaluator(node, cg);
   }

TR::Register *J9::X86::TreeEvaluator::checkcastEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::VMcheckcastEvaluator(node, cg);
   }

TR::Register *J9::X86::TreeEvaluator::checkcastAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::VMcheckcastEvaluator(node, cg);
   }


// Handles newObject, newArray, anewArray
//
TR::Register *J9::X86::TreeEvaluator::newEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Register *targetRegister=NULL;

   targetRegister = TR::TreeEvaluator::VMnewEvaluator(node, cg);
   if (!targetRegister)
      {
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

TR::Register *J9::X86::TreeEvaluator::multianewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::performHelperCall(node, NULL, TR::acall, true, cg);
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

   TR::MemoryReference *contiguousArraySizeMR =
      generateX86MemoryReference(objectReg, fej9->getOffsetOfContiguousArraySizeField(), cg);

   TR::MemoryReference *discontiguousArraySizeMR =
      generateX86MemoryReference(objectReg, fej9->getOffsetOfDiscontiguousArraySizeField(), cg);

   generateRegMemInstruction(L4RegMem, node, lengthReg, contiguousArraySizeMR, cg);
   generateRegRegInstruction(TEST4RegReg, node, lengthReg, lengthReg, cg);
   generateRegMemInstruction(CMOVE4RegMem, node, lengthReg, discontiguousArraySizeMR, cg);

   cg->decReferenceCount(node->getFirstChild());
   node->setRegister(lengthReg);
   return lengthReg;
   }

TR::Register *J9::X86::TreeEvaluator::exceptionRangeFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   generateFenceInstruction(FENCE, node, node, cg);
   return NULL;
   }


TR::Register *J9::X86::TreeEvaluator::evaluateNULLCHKWithPossibleResolve(
   TR::Node          *node,
   bool              needResolution,
   TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

   // NOTE:
   //
   // If no code is generated for the null check, just evaluate the
   // child and decrement its use count UNLESS the child is a pass-through node
   // in which case some kind of explicit test or indirect load must be generated
   // to force the null check at this point.
   //
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *reference  = NULL;
   TR::Compilation *comp = cg->comp();

   bool usingCompressedPointers = false;

   if (comp->useCompressedPointers() &&
       firstChild->getOpCodeValue() == TR::l2a)
      {
      // pattern match the sequence under the l2a
      // NULLCHK        NULLCHK                     <- node
      //    iaload f      l2a
      //       aload O       ladd
      //                       lshl
      //                          i2l
      //                            iiload f        <- firstChild
      //                               aload O      <- reference
      //                          iconst shftKonst
      //                       lconst HB
      //
      usingCompressedPointers = true;

      TR::ILOpCodes loadOp = comp->il.opCodeForIndirectLoad(TR::Int32);

      while (firstChild->getOpCodeValue() != loadOp)
         firstChild = firstChild->getFirstChild();
      reference = firstChild->getFirstChild();
      }
   else
      reference = node->getNullCheckReference();

   TR::ILOpCode &opCode = firstChild->getOpCode();

   // Skip the NULLCHK for TR::loadaddr nodes.
   //
   if (reference->getOpCodeValue() == TR::loadaddr)
      {
      if (usingCompressedPointers)
         firstChild = node->getFirstChild();
      cg->evaluate(firstChild);
      cg->decReferenceCount(firstChild);
      return NULL;
      }

   bool needExplicitCheck  = true;
   bool needLateEvaluation = true;

   // Add the explicit check after this instruction
   //
   TR::Instruction *appendTo = 0;

   if (opCode.isLoadVar() || (TR::Compiler->target.is64Bit() && opCode.getOpCodeValue()==TR::l2i))
      {
      TR::SymbolReference *symRef = NULL;

      if (opCode.getOpCodeValue()==TR::l2i)
         {
         symRef = firstChild->getFirstChild()->getSymbolReference();
         }
      else
         symRef = firstChild->getSymbolReference();

      if (symRef &&
          (symRef->getSymbol()->getOffset() + symRef->getOffset() < cg->getNumberBytesReadInaccessible()))
         {
         needExplicitCheck = false;

         // If the child is an arraylength which has been reduced to an iiload,
         // and is only going to be used immediately in a bound check then combine the checks.
         //
         TR::TreeTop *nextTreeTop = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();
         if (firstChild->getReferenceCount() == 2 && nextTreeTop)
            {
            TR::Node *nextTopNode = nextTreeTop->getNode();

            if (nextTopNode)
               {
               if (nextTopNode->getOpCode().isBndCheck() || nextTopNode->getOpCode().isSpineCheck())
                  {
                  bool doIt = false;

                  if (nextTopNode->getOpCodeValue() == TR::SpineCHK)
                     {
                     // Implicit NULLCHKs and SpineCHKs can be merged if the base array
                     // is the same.
                     //
                     if (firstChild->getOpCode().isIndirect() && firstChild->getOpCode().isLoadVar())
                        {
                        if (nextTopNode->getChild(1) == firstChild->getFirstChild())
                           doIt = true;
                        }
                     }
                  else
                     {
                     int32_t arrayLengthChildNum = (nextTopNode->getOpCodeValue() == TR::BNDCHKwithSpineCHK) ? 2 : 0;

                     if (nextTopNode->getChild(arrayLengthChildNum) == firstChild)
                        doIt = true;
                     }

                  if (doIt &&
                     performTransformation(comp,
                     "\nMerging NULLCHK [" POINTER_PRINTF_FORMAT "] and BNDCHK/SpineCHK [" POINTER_PRINTF_FORMAT "] of load child [" POINTER_PRINTF_FORMAT "]\n",
                      node, nextTopNode, firstChild))
                     {
                     needLateEvaluation = false;
                     nextTopNode->setHasFoldedImplicitNULLCHK(true);
                     }
                  }
               else if (nextTopNode->getOpCode().isIf() &&
                        nextTopNode->isNonoverriddenGuard() &&
                        nextTopNode->getFirstChild() == firstChild)
                  {
                  needLateEvaluation = false;
                  needExplicitCheck = true;
                  reference->incReferenceCount(); // will be decremented again later
                  }
               }
            }
         }
      else if (firstChild->getReferenceCount() == 1 && !firstChild->getSymbolReference()->isUnresolved())
         {
         // If the child is only used here, we don't need to evaluate it
         // since all we need is the grandchild which will be evaluated by
         // the generation of the explicit check below.
         //
         needLateEvaluation = false;

         // at this point, firstChild is the raw iiload (created by lowerTrees) and
         // reference is the aload of the object. node->getFirstChild is the
         // l2a sequence; as a result, firstChild's refCount will always be 1
         // and node->getFirstChild's refCount will be at least 2 (one under the nullchk
         // and the other under the translate treetop)
         //
         if (usingCompressedPointers && node->getFirstChild()->getReferenceCount() >= 2)
            needLateEvaluation = true;
         }
      }
   else if (opCode.isStore())
      {
      TR::SymbolReference *symRef = firstChild->getSymbolReference();
      if (symRef &&
          symRef->getSymbol()->getOffset() + symRef->getOffset() < cg->getNumberBytesWriteInaccessible())
         {
         needExplicitCheck = false;
         }
      }
   else if (opCode.isCall()     &&
            opCode.isIndirect() &&
            cg->getNumberBytesReadInaccessible() > TR::Compiler->om.offsetOfObjectVftField())
      {
      needExplicitCheck = false;
      }
   else if (opCode.getOpCodeValue() == TR::monent ||
            opCode.getOpCodeValue() == TR::monexit)
      {
      // The child may generate inline code that provides an implicit null check
      // but we won't know until the child is evaluated.
      //
      reference->incReferenceCount(); // will be decremented again later
      needLateEvaluation = false;
      cg->evaluate(reference);
      appendTo = comp->getAppendInstruction();
      cg->evaluate(firstChild);

      // TODO: this shouldn't be getOffsetOfContiguousArraySizeField
      //
      if (cg->getImplicitExceptionPoint() &&
          cg->getNumberBytesReadInaccessible() > fej9->getOffsetOfContiguousArraySizeField())
         {
         needExplicitCheck = false;
         cg->decReferenceCount(reference);
         }
      }

   // Generate the code for the null check.
   //
   if (needExplicitCheck)
      {
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
      if (opCode.getOpCodeValue() == TR::PassThrough &&
          reference->getOpCode().isLoadVar() &&
          reference->getRegister() == NULL &&
          reference->getReferenceCount() == 1)
         {
         TR::MemoryReference *tempMR = generateX86MemoryReference(reference, cg);

         if (!appendTo)
             appendTo = comp->getAppendInstruction();

         TR_X86OpCodes op = CMPMemImms();
         appendTo = generateMemImmInstruction(appendTo, op, tempMR, NULLVALUE, cg);
         tempMR->decNodeReferenceCounts(cg);
         needLateEvaluation = false;
         }
      else
         {
         TR::Register *targetRegister = cg->evaluate(reference);

         if (!appendTo)
            appendTo = comp->getAppendInstruction();

         appendTo = generateRegRegInstruction(appendTo, TESTRegReg(), targetRegister, targetRegister, cg);
         }

      TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
      appendTo = generateLabelInstruction(appendTo, JE4, snippetLabel, cg);
      //the _node field should point to the current node
      appendTo->setNode(node);
      appendTo->setLiveLocals(cg->getLiveLocals());

      TR::Snippet *snippet;
      if (opCode.isCall() || !needResolution || TR::Compiler->target.is64Bit()) //TODO:AMD64: Implement the "withresolve" version
         {
         if (cg->supportsFS0VMThreadRematerialization() && cg->allowVMThreadRematerialization())
            snippet = new (cg->trHeapMemory()) TR::X86CheckFailureSnippet(cg, node->getSymbolReference(),
                                                                    snippetLabel, appendTo, false, true);
         else
            snippet = new (cg->trHeapMemory()) TR::X86CheckFailureSnippet(cg, node->getSymbolReference(),
                                                                    snippetLabel, appendTo);
         }
      else
         {
         TR_RuntimeHelper resolverCall;
         TR::Machine *machine = cg->machine();
         TR::Symbol * firstChildSym = firstChild->getSymbolReference()->getSymbol();

         if (firstChildSym->isShadow())
            {
            resolverCall = opCode.isStore() ?
               TR_X86interpreterUnresolvedFieldSetterGlue : TR_X86interpreterUnresolvedFieldGlue;
            }
         else if (firstChildSym->isClassObject())
            {
            resolverCall = firstChildSym->addressIsCPIndexOfStatic() ?
               TR_X86interpreterUnresolvedClassFromStaticFieldGlue : TR_X86interpreterUnresolvedClassGlue;
            }
         else if (firstChildSym->isConstString())
            {
            resolverCall = TR_X86interpreterUnresolvedStringGlue;
            }
         else if (firstChildSym->isConstMethodType())
            {
            resolverCall = TR_interpreterUnresolvedMethodTypeGlue;
            }
         else if (firstChildSym->isConstMethodHandle())
            {
            resolverCall = TR_interpreterUnresolvedMethodHandleGlue;
            }
         else if (firstChildSym->isCallSiteTableEntry())
            {
            resolverCall = TR_interpreterUnresolvedCallSiteTableEntryGlue;
            }
         else if (firstChildSym->isMethodTypeTableEntry())
            {
            resolverCall = TR_interpreterUnresolvedMethodTypeTableEntryGlue;
            }
         else
            {
            resolverCall = opCode.isStore() ?
               TR_X86interpreterUnresolvedStaticFieldSetterGlue : TR_X86interpreterUnresolvedStaticFieldGlue;
            }

         snippet = new (cg->trHeapMemory()) TR::X86CheckFailureSnippetWithResolve(cg, node->getSymbolReference(),
                                                                      firstChild->getSymbolReference(),
                                                                      resolverCall,
                                                                      snippetLabel,
                                                                      appendTo);

         ((TR::X86CheckFailureSnippetWithResolve *)(snippet))->setNumLiveX87Registers(machine->fpGetNumberOfLiveFPRs());
         if (cg->useSSEForDoublePrecision())
            ((TR::X86CheckFailureSnippetWithResolve *)(snippet))->setHasLiveXMMRs();
         }

      cg->addSnippet(snippet);
      }

   // If we need to evaluate the child, do so. Otherwise, if we have
   // evaluated the reference node, then decrement its use count.
   // The use count of the child is decremented when we are done
   // evaluating the NULLCHK.
   //
   if (needLateEvaluation)
      {
      if (comp->useCompressedPointers())
         {
         // for stores under NULLCHKs, artificially bump
         // down the reference count before evaluation (since stores
         // return null as registers)
         //
         bool fixRefCount = false;
         if (node->getFirstChild()->getOpCode().isStoreIndirect() &&
               node->getFirstChild()->getReferenceCount() > 1)
            {
            node->getFirstChild()->decReferenceCount();
            fixRefCount = true;
            }
         cg->evaluate(node->getFirstChild());
         if (fixRefCount)
            node->getFirstChild()->incReferenceCount();
         }
      else
         cg->evaluate(firstChild);
      }
   else if (needExplicitCheck)
      {
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
   if (!needExplicitCheck)
      {
      TR::Instruction *faultingInstruction = cg->getImplicitExceptionPoint();
      if (faultingInstruction)
         {
         faultingInstruction->setNeedsGCMap(0xFF00FFFF);
         faultingInstruction->setNode(node);
         }
      }

   TR::Node *n = NULL;
   if (comp->useCompressedPointers() &&
         reference->getOpCodeValue() == TR::l2a)
      {
      reference->setIsNonNull(true);
      n = reference->getFirstChild();
      TR::ILOpCodes loadOp = comp->il.opCodeForIndirectLoad(TR::Int32);
      while (n->getOpCodeValue() != loadOp)
         {
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

TR::Register *J9::X86::TreeEvaluator::resolveCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // No code is generated for the resolve check. The child will reference an
   // unresolved symbol and all check handling is done via the corresponding
   // snippet.
   //
   TR::Node *firstChild = node->getFirstChild();
   TR::Compilation *comp = cg->comp();
   bool fixRefCount = false;
   if (comp->useCompressedPointers())
      {
      // for stores under ResolveCHKs, artificially bump
      // down the reference count before evaluation (since stores
      // return null as registers)
      //
      if (node->getFirstChild()->getOpCode().isStoreIndirect() &&
            node->getFirstChild()->getReferenceCount() > 1)
         {
         node->getFirstChild()->decReferenceCount();
         fixRefCount = true;
         }
      }
   cg->evaluate(firstChild);
   if (fixRefCount)
      firstChild->incReferenceCount();

   cg->decReferenceCount(firstChild);
   return NULL;
   }


// Generate explicit checks for division by zero and division
// overflow (i.e. 0x80000000 / 0xFFFFFFFF), if necessary.
//
TR::Register *J9::X86::TreeEvaluator::DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool     hasConversion;
   TR::Node *divisionNode = node->getFirstChild();
   TR::Compilation *comp = cg->comp();

   TR::ILOpCodes op = divisionNode->getOpCodeValue();

   if (op == TR::iu2l ||
       op == TR::bu2i ||
       op == TR::bu2l ||
       op == TR::bu2s ||
       op == TR::su2i ||
       op == TR::su2l)
      {
      divisionNode = divisionNode->getFirstChild();
      hasConversion = true;
      }
   else
      hasConversion = false;

   bool use64BitRegisters =  TR::Compiler->target.is64Bit() && divisionNode->getOpCode().isLong();
   bool useRegisterPairs  = TR::Compiler->target.is32Bit() && divisionNode->getOpCode().isLong();

   // Not all targets support implicit division checks, so we generate explicit
   // tests and snippets to jump to.
   //
   bool platformNeedsExplicitCheck = !cg->enableImplicitDivideCheck();

   // Only do this for TR::ldiv/TR::lrem and TR::idiv/TR::irem by non-constant
   // divisors, or by a constant of zero.
   // Other constant divisors are optimized in signedIntegerDivOrRemAnalyser,
   // and do not cause hardware exceptions.
   //
   bool operationNeedsCheck = (divisionNode->getOpCode().isInt() &&
      (!divisionNode->getSecondChild()->getOpCode().isLoadConst() || divisionNode->getSecondChild()->getInt() == 0));
   if (use64BitRegisters)
      {
      operationNeedsCheck = operationNeedsCheck |
         ((!divisionNode->getSecondChild()->getOpCode().isLoadConst() || divisionNode->getSecondChild()->getLongInt() == 0));
      }
   else
      {
      operationNeedsCheck = operationNeedsCheck | useRegisterPairs;
      }

   if (platformNeedsExplicitCheck && operationNeedsCheck)
      {
      TR::Register *dividendReg = cg->evaluate(divisionNode->getFirstChild());
      TR::Register *divisorReg  = cg->evaluate(divisionNode->getSecondChild());

      TR::LabelSymbol *startLabel               = generateLabelSymbol(cg);
      TR::LabelSymbol *divisionLabel            = generateLabelSymbol(cg);
      TR::LabelSymbol *divideByZeroSnippetLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *restartLabel             = generateLabelSymbol(cg);

      // These instructions are dissected in the divide check snippet to determine
      // the source registers. If they or their format are changed, you may need to
      // change the snippet(s) also.
      //
      TR::X86RegRegInstruction *lowDivisorTestInstr;
      TR::X86RegRegInstruction *highDivisorTestInstr;

      startLabel->setStartInternalControlFlow();
      restartLabel->setEndInternalControlFlow();

      generateLabelInstruction(LABEL, node, startLabel, cg);

      if (useRegisterPairs)
         {
         TR::Register *tempReg = cg->allocateRegister(TR_GPR);
         lowDivisorTestInstr =  generateRegRegInstruction(MOV4RegReg, node, tempReg, divisorReg->getLowOrder(),  cg);
         highDivisorTestInstr = generateRegRegInstruction(OR4RegReg,  node, tempReg, divisorReg->getHighOrder(), cg);
         generateRegRegInstruction(TEST4RegReg, node, tempReg, tempReg, cg);
         cg->stopUsingRegister(tempReg);
         }
      else
         lowDivisorTestInstr = generateRegRegInstruction(TESTRegReg(use64BitRegisters), node, divisorReg, divisorReg, cg);

      generateLabelInstruction(JE4, node, divideByZeroSnippetLabel, cg);

      cg->addSnippet(new (cg->trHeapMemory()) TR::X86CheckFailureSnippet(cg, node->getSymbolReference(),
                                                         divideByZeroSnippetLabel,
                                                         comp->getAppendInstruction()));

      generateLabelInstruction(LABEL, node, divisionLabel, cg);

      TR::Register *resultRegister = cg->evaluate(divisionNode);

      if (!hasConversion)
         cg->decReferenceCount(divisionNode);

      // We need to make sure that any spilling occurs only after restartLabel,
      // otherwise the divide check snippet may store into the wrong register.
      //
      TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t) 0, 2, cg);
      TR::Register *scratchRegister;

      if (useRegisterPairs)
         {
         deps->addPostCondition(resultRegister->getLowOrder(), TR::RealRegister::eax, cg);
         deps->addPostCondition(resultRegister->getHighOrder(), TR::RealRegister::edx, cg);
         }
      else switch(divisionNode->getOpCodeValue())
         {
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

      generateLabelInstruction(LABEL, node, restartLabel, deps, cg);

      if (hasConversion)
         {
         cg->evaluate(node->getFirstChild());
         cg->decReferenceCount(node->getFirstChild());
         }
      }
   else
      {
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
   if (TR::Compiler->target.is64Bit())
      return op.isIntegerOrAddress();
   else
      return op.isIntegerOrAddress() && (op.getSize() <= 4);
   }


static TR_X86OpCodes branchOpCodeForCompare(TR::ILOpCode &op, bool opposite=false)
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

   static const TR_X86OpCodes opTable[] =
      {
      BADIA32Op,  JL4,  JG4,  JNE4,
      JE4,        JLE4, JGE4, BADIA32Op,
      BADIA32Op,  JB4,  JA4,  JNE4,
      JE4,        JBE4, JAE4, BADIA32Op,
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

   TR::LabelSymbol *slowPathLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *restartLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   slowPathLabel->setStartInternalControlFlow();
   restartLabel->setEndInternalControlFlow();
   TR::Compilation *comp = cg->comp();

   // Temporarily hide the first child so it doesn't appear in the outlined call
   //
   node->rotateChildren(node->getNumChildren()-1, 0);
   node->setNumChildren(node->getNumChildren()-1);

   // Outlined instructions for check failure
   // Note: we don't pass the restartLabel in here because we don't want a
   // restart branch.
   //
   TR_OutlinedInstructions *outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::call, NULL, slowPathLabel, NULL, cg);
   cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);
   cg->generateDebugCounter(
         outlinedHelperCall->getFirstInstruction(),
         TR::DebugCounter::debugCounterName(comp, "helperCalls/%s/(%s)/%d/%d", node->getOpCode().getName(), comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
         1, TR::DebugCounter::Cheap);

   // Restore the first child
   //
   node->setNumChildren(node->getNumChildren()+1);
   node->rotateChildren(0, node->getNumChildren()-1);

   // Children other than the first are only for the outlined path; we don't need them here
   //
   for (int32_t i = 1; i < node->getNumChildren(); i++)
      cg->recursivelyDecReferenceCount(node->getChild(i));

   // In-line instructions for the check
   //
   TR::Node *valueToCheck = node->getFirstChild();
   if ( valueToCheck->getOpCode().isBooleanCompare()
     && isInteger(valueToCheck->getChild(0)->getOpCode(), cg)
     && isInteger(valueToCheck->getChild(1)->getOpCode(), cg)
     && performTransformation(comp, "O^O CODEGEN Optimizing ZEROCHK+%s %s\n", valueToCheck->getOpCode().getName(), valueToCheck->getName(cg->getDebug())))
      {
      if (valueToCheck->getOpCode().isCompareForOrder())
         {
         TR::TreeEvaluator::compareIntegersForOrder(valueToCheck, cg);
         }
      else
         {
         TR_ASSERT(valueToCheck->getOpCode().isCompareForEquality(), "Compare opcode must either be compare for order or for equality");
         TR::TreeEvaluator::compareIntegersForEquality(valueToCheck, cg);
         }
      generateLabelInstruction(branchOpCodeForCompare(valueToCheck->getOpCode(), true), node, slowPathLabel, cg);
      }
   else
      {
      TR::Register *value = cg->evaluate(node->getFirstChild());
      generateRegRegInstruction(TESTRegReg(!value->areUpperBitsZero()), node, value, value, cg);
      cg->decReferenceCount(node->getFirstChild());
      generateLabelInstruction(JE4, node, slowPathLabel, cg);
      }
   generateLabelInstruction(LABEL, node, restartLabel,  cg);

   return NULL;
   }


bool isConditionCodeSetForCompare(TR::Node *node, bool *jumpOnOppositeCondition)
   {
   TR::Compilation *comp = TR::comp();
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
   TR::Instruction     *prevInstr;
   TR::X86RegInstruction  *prevRegInstr;
   for (prevInstr = comp->getAppendInstruction();
        prevInstr;
        prevInstr = prevInstr->getPrev())
      {
      prevRegInstr = prevInstr->getIA32RegInstruction();
      if (prevRegInstr && (prevInstr->getOpCodeValue() == CMP4RegReg))
         {
         TR::Register *prevInstrTargetRegister = prevInstr->getTargetRegister();
         TR::Register *prevInstrSourceRegister = prevInstr->getSourceRegister();

         if (prevInstrTargetRegister && prevInstrSourceRegister &&
             (((prevInstrSourceRegister == firstChildReg) && (prevInstrTargetRegister == secondChildReg)) ||
              ((prevInstrSourceRegister == secondChildReg) && (prevInstrTargetRegister == firstChildReg))))
            {
            if (performTransformation(comp, "O^O SKIP BOUND CHECK COMPARISON at node %p\n", node))
               {
               if (prevInstrTargetRegister == secondChildReg)
                  *jumpOnOppositeCondition = true;
               return true;
               }
            else
               return false;
            }
         }

      if (prevInstr->getOpCodeValue() == LABEL)
         {
         // This instruction is a possible branch target.
         return false;
         }

      if (prevInstr->getOpCode().modifiesSomeArithmeticFlags())
         {
         // This instruction overwrites the condition flags.
         return false;
         }
      }

   return false;
   }


TR::Register *J9::X86::TreeEvaluator::BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild    = node->getFirstChild();
   TR::Node *secondChild   = node->getSecondChild();
   bool     needsVMThread = !cg->allowVMThreadRematerialization() ||
                                !cg->getSupportsVMThreadGRA();

   // Perform a bound check.
   //
   // Value propagation or profile-directed optimization may have determined
   // that the array bound is a constant, and lowered TR::arraylength into an
   // iconst. In this case, make sure that the constant is the second child.
   //
   TR::LabelSymbol *boundCheckFailureLabel = generateLabelSymbol(cg);
   TR::Instruction *instr;
   TR::Compilation *comp = cg->comp();

   if (needsVMThread)
      cg->setVMThreadRequired(true);

   bool skippedComparison = false;
   bool jumpOnOppositeCondition = false;
   if (firstChild->getOpCode().isLoadConst())
      {
      if (secondChild->getOpCode().isLoadConst() && firstChild->getInt() <= secondChild->getInt())
         {
         instr = generateLabelInstruction(JMP4, node, boundCheckFailureLabel, needsVMThread, cg);
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         }
      else
         {
         if (!isConditionCodeSetForCompare(node, &jumpOnOppositeCondition))
            {
            node->swapChildren();
            TR::TreeEvaluator::compareIntegersForOrder(node, cg);
            node->swapChildren();
            instr = generateLabelInstruction(JAE4, node, boundCheckFailureLabel, needsVMThread, cg);
            }
         else
            skippedComparison = true;
         }
      }
   else
      {
      if (!isConditionCodeSetForCompare(node, &jumpOnOppositeCondition))
         {
         TR::TreeEvaluator::compareIntegersForOrder(node, cg);
         instr = generateLabelInstruction(JBE4, node, boundCheckFailureLabel, needsVMThread, cg);
         }
      else
         skippedComparison = true;
      }

   if (skippedComparison)
      {
      if (jumpOnOppositeCondition)
         instr = generateLabelInstruction(JAE4, node, boundCheckFailureLabel, needsVMThread, cg);
      else
         instr = generateLabelInstruction(JBE4, node, boundCheckFailureLabel, needsVMThread, cg);

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }

   cg->addSnippet(new (cg->trHeapMemory()) TR::X86CheckFailureSnippet(cg, node->getSymbolReference(),
                                                     boundCheckFailureLabel,
                                                     instr,
                                                     false,
                                                     ! needsVMThread));

   if (needsVMThread)
      cg->setVMThreadRequired(false);

   if (node->hasFoldedImplicitNULLCHK())
      {
      TR::Instruction *faultingInstruction = cg->getImplicitExceptionPoint();
      traceMsg(comp,"Node %p has foldedimplicitNULLCHK, and a faulting instruction of %p\n",node,faultingInstruction);
      if (faultingInstruction)
         {
         faultingInstruction->setNeedsGCMap(0xFF00FFFF);
         faultingInstruction->setNode(node);
         }
      }

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
   TR::Node        *firstChild             = node->getFirstChild();
   TR::Node        *secondChild            = node->getSecondChild();
   TR::LabelSymbol *boundCheckFailureLabel = generateLabelSymbol(cg);
   TR::Instruction *instr;

   // vmThread required in BNDCHK's mainline code.
   bool needsVMThread = !cg->allowVMThreadRematerialization() ||
                        node->hasFoldedImplicitNULLCHK() ||
                        !cg->getSupportsVMThreadGRA();

   if (needsVMThread)
      cg->setVMThreadRequired(true);

   if (firstChild->getOpCode().isLoadConst())
      {
      if (secondChild->getOpCode().isLoadConst())
         {
         if (firstChild->getInt() < secondChild->getInt())
            {
            // Check will always fail, just jump to failure snippet
            //
            instr = generateLabelInstruction(JMP4, node, boundCheckFailureLabel, needsVMThread, cg);
            }
         else
            {
            // Check will always succeed, no need for an instruction
            //
            instr = NULL;
            }
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         }
      else
         {
         node->swapChildren();
         TR::TreeEvaluator::compareIntegersForOrder(node, cg);
         node->swapChildren();
         instr = generateLabelInstruction(JG4, node, boundCheckFailureLabel, needsVMThread, cg);
         }
      }
   else
      {
      TR::TreeEvaluator::compareIntegersForOrder(node, cg);
      instr = generateLabelInstruction(JL4, node, boundCheckFailureLabel, needsVMThread, cg);
      }

   if (instr)
      cg->addSnippet(new (cg->trHeapMemory()) TR::X86CheckFailureSnippet(cg, node->getSymbolReference(),
                                                             boundCheckFailureLabel,
                                                             instr,
                                                             false,
                                                             ! needsVMThread));

   if (needsVMThread)
      cg->setVMThreadRequired(false);

   return NULL;
   }


TR::Register *J9::X86::TreeEvaluator::ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR::Instruction *prevInstr = comp->getAppendInstruction();
   TR::LabelSymbol *startLabel,
                  *startOfWrtbarLabel,
                  *doNullStoreLabel,
                  *doneLabel;

   // skipStoreNullCheck
   // skipJLOCheck
   // skipSuperClassCheck
   // cannotSkipWriteBarrier

   flags16_t actions;

   TR::Node *firstChild = node->getFirstChild();
   TR::Node *sourceChild = firstChild->getSecondChild();

   static bool isRealTimeGC = comp->getOptions()->realTimeGC();
   TR_WriteBarrierKind gcMode = comp->getOptions()->getGcMode();

   bool isNonRTWriteBarrierRequired = (gcMode != TR_WrtbarNone && !firstChild->skipWrtBar()) ? true : false;
   bool generateWriteBarrier = isRealTimeGC || isNonRTWriteBarrierRequired;
   bool nopASC = (node->getArrayStoreClassInNode() &&
                  comp->performVirtualGuardNOPing() &&
                  !fej9->classHasBeenExtended(node->getArrayStoreClassInNode())
                 ) ?  true : false;

   doneLabel = generateLabelSymbol(cg);
   doneLabel->setEndInternalControlFlow();

   doNullStoreLabel = generateWriteBarrier ? generateLabelSymbol(cg) : doneLabel;
   startOfWrtbarLabel = generateWriteBarrier ? generateLabelSymbol(cg) : doNullStoreLabel;

   bool usingCompressedPointers = false;
   bool usingLowMemHeap  = false;
   bool useShiftedOffsets = (TR::Compiler->om.compressedReferenceShiftOffset() != 0);

   if (comp->useCompressedPointers() && firstChild->getOpCode().isIndirect())
      {
      // pattern match the sequence
      //     iistore f     iistore f         <- node
      //       aload O       aload O
      //     value           l2i
      //                       lshr
      //                         lsub
      //                           a2l
      //                             value   <- sourceChild
      //                           lconst HB
      //                         iconst shftKonst
      //
      // -or- if the field is known to be null or usingLowMemHeap
      // iistore f
      //    aload O
      //    l2i
      //      a2l
      //        value  <- valueChild
      //
      TR::Node *translatedNode = sourceChild;
      if (translatedNode->getOpCode().isConversion())
         translatedNode = translatedNode->getFirstChild();
      if (translatedNode->getOpCode().isRightShift()) //optional
         translatedNode = translatedNode->getFirstChild();

      if ((TR::Compiler->vm.heapBaseAddress() == 0) ||
            sourceChild->isNull())
         usingLowMemHeap = true;

      if (translatedNode->getOpCode().isSub() || usingLowMemHeap)
         usingCompressedPointers = true;

      if (usingCompressedPointers)
         {
         if (!usingLowMemHeap || useShiftedOffsets)
            {
            while ((sourceChild->getNumChildren() > 0) &&
                     (sourceChild->getOpCodeValue() != TR::a2l))
               sourceChild = sourceChild->getFirstChild();
            if (sourceChild->getOpCodeValue() == TR::a2l)
               sourceChild = sourceChild->getFirstChild();
            // this is required so that different registers are
            // allocated for the actual store and translated values
            sourceChild->incReferenceCount();
            }
         }
      }

   // -------------------------------------------------------------------------
   //
   // Evaluate all of the children here to avoid issues with internal control
   // flow and outlined instructions.
   //
   // -------------------------------------------------------------------------

   TR::MemoryReference *tempMR = NULL;

   if (generateWriteBarrier)
      {
      tempMR = generateX86MemoryReference(firstChild, cg);
      }

   TR::Node *destinationChild = firstChild->getChild(2);
   TR::Register *destinationRegister = cg->evaluate(destinationChild);
   TR::Register *sourceRegister = cg->evaluate(sourceChild);

   TR_X86ScratchRegisterManager *scratchRegisterManager =
      cg->generateScratchRegisterManager(TR::Compiler->target.is64Bit() ? 15 : 7);

   TR::Register *compressedRegister = NULL;
   if (usingCompressedPointers)
      {
      if (usingLowMemHeap && !useShiftedOffsets)
         compressedRegister = sourceRegister;
      else
         {
         // valid for useShiftedOffsets
         compressedRegister = cg->evaluate(firstChild->getSecondChild());
         if (!usingLowMemHeap)
            {
            generateRegRegInstruction(TESTRegReg(), firstChild, sourceRegister, sourceRegister, cg);
            generateRegRegInstruction(CMOVERegReg(), firstChild, compressedRegister, sourceRegister, cg);
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
   generateLabelInstruction(LABEL, node, startLabel, cg);

   generateRegRegInstruction(TESTRegReg(), node, sourceRegister, sourceRegister, cg);

   TR::LabelSymbol *nullTargetLabel =
      isRealTimeGC ? startOfWrtbarLabel : doNullStoreLabel;

   generateLabelInstruction(JE4, node, nullTargetLabel, cg);

   TR_OutlinedInstructions *nullStorePath = NULL;
   if (!isRealTimeGC && isNonRTWriteBarrierRequired)
      {
      nullStorePath = new (cg->trHeapMemory()) TR_OutlinedInstructions(nullTargetLabel, cg);
      cg->getOutlinedInstructionsList().push_front(nullStorePath);
      }

   // -------------------------------------------------------------------------
   //
   // Generate up-front array store checks to avoid calling out to the helper.
   //
   // -------------------------------------------------------------------------

   cg->setVMThreadRequired(true);

   TR::LabelSymbol *postASCLabel = NULL;
   if (nopASC)
      {
      // Speculatively NOP the array store check if VP is able to prove that the ASC
      // would always succeed given the current state of the class hierarchy.
      //
      TR::Node *helperCallNode = TR::Node::createWithSymRef(TR::call, 2, 2, sourceChild, destinationChild, node->getSymbolReference());
      helperCallNode->copyByteCodeInfo(node);

      TR::LabelSymbol *oolASCLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *restartLabel;

      if (generateWriteBarrier)
         {
         restartLabel = startOfWrtbarLabel;
         }
      else
         {
         restartLabel = postASCLabel = generateLabelSymbol(cg);
         }

      TR_OutlinedInstructions *outlinedASCHelperCall =
         new (cg->trHeapMemory()) TR_OutlinedInstructions(helperCallNode, TR::call, NULL, oolASCLabel, restartLabel, false, cg);
      cg->getOutlinedInstructionsList().push_front(outlinedASCHelperCall);

      static char *alwaysDoOOLASCc = feGetEnv("TR_doOOLASC");
      static bool alwaysDoOOLASC = alwaysDoOOLASCc ? true : false;

      if (!alwaysDoOOLASC)
         {
         TR_VirtualGuard *virtualGuard = TR_VirtualGuard::createArrayStoreCheckGuard(comp, node, node->getArrayStoreClassInNode());
         TR::Instruction *pachable = generateVirtualGuardNOPInstruction(node, virtualGuard->addNOPSite(), NULL, oolASCLabel, cg);
         }
      else
         {
         generateLabelInstruction(JMP4, node, oolASCLabel, cg);
         }

      // Restore the reference counts of the children created for the temporary vacll node above.
      //
      sourceChild->decReferenceCount();
      destinationChild->decReferenceCount();
      }
   else
      {
      TR::TreeEvaluator::VMarrayStoreCHKEvaluator(
         node,
         sourceChild,
         destinationChild,
         scratchRegisterManager,
         startOfWrtbarLabel,
         prevInstr,
         cg);
      }

   // -------------------------------------------------------------------------
   //
   // Generate write barrier.
   //
   // -------------------------------------------------------------------------

   bool isSourceNonNull = sourceChild->isNonNull();

   if (generateWriteBarrier)
      {
      generateLabelInstruction(LABEL, node, startOfWrtbarLabel, cg);

      if (!isRealTimeGC)
         {
         // HACK: set the nullness property on the source so that the write barrier
         //       doesn't do the same test.
         //
         sourceChild->setIsNonNull(true);
         }

      TR::TreeEvaluator::VMwrtbarWithStoreEvaluator(
         node,
         tempMR,
         scratchRegisterManager,
         destinationChild,
         sourceChild,
         true,
         cg,
         true);
      }
   else if (postASCLabel)
      {
      // Lay down a arestart label for OOL ASC if the write barrier was skipped
      //
      generateLabelInstruction(LABEL, node, postASCLabel, cg);
      }

   // -------------------------------------------------------------------------
   //
   // Either do the bypassed NULL store out of line or the reference store
   // inline if the write barrier was omitted.
   //
   // -------------------------------------------------------------------------

   TR::MemoryReference *tempMR2 = NULL;

   TR::Instruction *dependencyAnchorInstruction = NULL;

   if (!isRealTimeGC)
      {
      if (generateWriteBarrier)
         {
         assert(nullStorePath);
         assert(isNonRTWriteBarrierRequired);
         assert(tempMR);

         // HACK: reset the nullness property on the source.
         //
         sourceChild->setIsNonNull(isSourceNonNull);

         // Perform the NULL store that was bypassed earlier by the write barrier.
         //
         nullStorePath->swapInstructionListsWithCompilation();
         generateLabelInstruction(NULL, LABEL, nullTargetLabel, cg)->setNode(node);

         tempMR2 = generateX86MemoryReference(*tempMR, 0, cg);

         if (usingCompressedPointers)
            generateMemRegInstruction(S4MemReg, node, tempMR2, compressedRegister, cg);
         else
            generateMemRegInstruction(SMemReg(), node, tempMR2, sourceRegister, cg);

         generateLabelInstruction(JMP4, node, doneLabel, cg);
         nullStorePath->swapInstructionListsWithCompilation();
         }
      else
         {
         // No write barrier emitted.  Evaluate the store here.
         //

         assert(!nullStorePath);
         assert(!isNonRTWriteBarrierRequired);
         assert(doneLabel == nullTargetLabel);

         // This is where the dependency condition will eventually go.
         //
         dependencyAnchorInstruction = comp->getAppendInstruction();

         tempMR = generateX86MemoryReference(firstChild, cg);

         TR::X86MemRegInstruction *storeInstr;

         if (usingCompressedPointers)
            storeInstr = generateMemRegInstruction(S4MemReg, node, tempMR, compressedRegister, cg);
         else
            storeInstr = generateMemRegInstruction(SMemReg(), node, tempMR, sourceRegister, cg);

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

   TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions(12, 12, cg);
   deps->unionPostCondition(destinationRegister, TR::RealRegister::NoReg, cg);
   deps->unionPostCondition(sourceRegister, TR::RealRegister::NoReg, cg);

   scratchRegisterManager->addScratchRegistersToDependencyList(deps);

   if (usingCompressedPointers && (!usingLowMemHeap || useShiftedOffsets))
      {
      deps->unionPostCondition(compressedRegister, TR::RealRegister::NoReg, cg);
      }

   if (generateWriteBarrier)
      {
      // Memory reference is not live in an internal control flow region.
      //
      if (tempMR->getBaseRegister() && tempMR->getBaseRegister() != destinationRegister)
         {
         deps->unionPostCondition(tempMR->getBaseRegister(), TR::RealRegister::NoReg, cg);
         }

      if (tempMR->getIndexRegister() && tempMR->getIndexRegister() != destinationRegister)
         {
         deps->unionPostCondition(tempMR->getIndexRegister(), TR::RealRegister::NoReg, cg);
         }

      if (TR::Compiler->target.is64Bit())
         {
         TR::Register *addressRegister =tempMR->getAddressRegister();
         if (addressRegister && addressRegister != destinationRegister)
            {
            deps->unionPostCondition(addressRegister, TR::RealRegister::NoReg, cg);
            }
         }
      }

   if (tempMR2 && TR::Compiler->target.is64Bit())
      {
      TR::Register *addressRegister = tempMR2->getAddressRegister();
      if (addressRegister && addressRegister != destinationRegister)
         deps->unionPostCondition(addressRegister, TR::RealRegister::NoReg, cg);
      }

   deps->unionPostCondition(
      cg->getVMThreadRegister(),
      (TR::RealRegister::RegNum)cg->getVMThreadRegister()->getAssociation(), cg);

   deps->stopAddingConditions();

   scratchRegisterManager->stopUsingRegisters();

   if (dependencyAnchorInstruction)
      {
      generateLabelInstruction(dependencyAnchorInstruction, LABEL, doneLabel, deps, cg);
      }
   else
      {
      generateLabelInstruction(LABEL, node, doneLabel, deps, cg);
      }

   cg->setVMThreadRequired(false);

   if (usingCompressedPointers)
      {
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
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   bool needsBoundCheck = (node->getOpCodeValue() == TR::BNDCHKwithSpineCHK) ? true : false;

   TR::Node *loadOrStoreChild = node->getFirstChild();
   TR::Node *baseArrayChild = node->getSecondChild();
   TR::Node *arrayLengthChild;
   TR::Node *indexChild;

   if (needsBoundCheck)
      {
      arrayLengthChild = node->getChild(2);
      indexChild = node->getChild(3);
      }
   else
      {
      arrayLengthChild = NULL;
      indexChild = node->getChild(2);
      }

   bool needsVMThread = !cg->allowVMThreadRematerialization() ||
                        !cg->getSupportsVMThreadGRA();

   // Perform a bound check.
   //
   // Value propagation or profile-directed optimization may have determined
   // that the array bound is a constant, and lowered TR::arraylength into an
   // iconst.  In this case, make sure that the constant is the second child.
   //
   TR_X86OpCodes branchOpCode;

   if (needsVMThread)
      cg->setVMThreadRequired(true);

   // For primitive stores anchored under the check node, we must evaluate the source node
   // before the bound check branch so that its available to the snippet.  We can make
   // an exception for constant values that could be folded directly into a immediate
   // store instruction.
   //
   if (loadOrStoreChild->getOpCode().isStore() && loadOrStoreChild->getReferenceCount() <= 1)
      {
      TR::Node *valueChild = loadOrStoreChild->getSecondChild();

      if (!valueChild->getOpCode().isLoadConst() ||
          (valueChild->getOpCode().isLoadConst() &&
          ((valueChild->getDataType() == TR::Float) || (valueChild->getDataType() == TR::Double) ||
           (TR::Compiler->target.is64Bit() && !IS_32BIT_SIGNED(valueChild->getLongInt())))))
         {
         cg->evaluate(valueChild);
         }
      }

   TR::Register *baseArrayReg = cg->evaluate(baseArrayChild);

   TR::TreeEvaluator::preEvaluateEscapingNodesForSpineCheck(node, cg);

   TR::Instruction *faultingInstruction = NULL;

   TR::LabelSymbol *boundCheckFailureLabel = generateLabelSymbol(cg);
   TR::X86LabelInstruction *checkInstr = NULL;

   if (needsBoundCheck)
      {
      if (arrayLengthChild->getOpCode().isLoadConst())
         {
         if (indexChild->getOpCode().isLoadConst() && arrayLengthChild->getInt() <= indexChild->getInt())
            {
            // Create real check failure snippet if we can prove the
            // bound check will always fail.
            //
            branchOpCode = JMP4;
            cg->decReferenceCount(arrayLengthChild);
            cg->decReferenceCount(indexChild);
            }
         else
            {
            // Check the bounds.
            //
            TR::TreeEvaluator::compareIntegersForOrder(node, indexChild, arrayLengthChild, cg);
            branchOpCode = JAE4;
            faultingInstruction = cg->getImplicitExceptionPoint();
            }
         }
      else
         {
         // Check the bounds.
         //
         TR::TreeEvaluator::compareIntegersForOrder(node, arrayLengthChild, indexChild, cg);
         branchOpCode = JBE4;
         faultingInstruction = cg->getImplicitExceptionPoint();
         }

      static char *forceArraylet = feGetEnv("TR_forceArraylet");
      if (forceArraylet)
         {
         branchOpCode = JMP4;
         }

      checkInstr = generateLabelInstruction(branchOpCode, node, boundCheckFailureLabel, false, cg);
      }
   else
      {
      // -------------------------------------------------------------------------
      // Check if the base array has a spine.  If so, process out of line.
      // -------------------------------------------------------------------------

      if (!indexChild->getOpCode().isLoadConst())
         {
         cg->evaluate(indexChild);
         }

      TR::MemoryReference *arraySizeMR =
         generateX86MemoryReference(baseArrayReg, fej9->getOffsetOfContiguousArraySizeField(), cg);

      generateMemImmInstruction(CMP4MemImms, node, arraySizeMR, 0, cg);
      generateLabelInstruction(JE4, node, boundCheckFailureLabel, cg);
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
   if (loadOrStoreChild->getOpCode().isStore())
      {
      if (loadOrStoreChild->getReferenceCount() > 1)
         {
         TR_ASSERT(loadOrStoreChild->getOpCode().isWrtBar(), "Opcode must be wrtbar");
         loadOrStoreReg = cg->evaluate(loadOrStoreChild->getFirstChild());
         cg->decReferenceCount(loadOrStoreChild->getFirstChild());
         }
      else
         {
         // If the store is not commoned then it must be a primitive store.
         //
         loadOrStoreReg = cg->evaluate(loadOrStoreChild);
         valueReg = loadOrStoreChild->getSecondChild()->getRegister();

         if (!valueReg)
            {
            // If the immediate value was not evaluated then it must have been folded
            // into the instruction.
            //
            TR_ASSERT(loadOrStoreChild->getSecondChild()->getOpCode().isLoadConst(), "unevaluated, non-constant value child");
            TR_ASSERT(IS_32BIT_SIGNED(loadOrStoreChild->getSecondChild()->getInt()), "immediate value too wide for instruction");
            }
         }
      }
   else
      {
      loadOrStoreReg = cg->evaluate(loadOrStoreChild);
      }

   // -----------------------------------------------------------------------------------
   // Stop tracking virtual register usage.
   // -----------------------------------------------------------------------------------

   TR::list<OMR::RegisterUsage*> *mainlineRUL = cg->stopRecordingRegisterUsage();

   TR::Register *indexReg = indexChild->getRegister();

   // Index register must be in a register or a constant.
   //
   TR_ASSERT((indexReg || indexChild->getOpCode().isLoadConst()),
      "index child is not evaluated or constant: indexReg=%p, indexChild=%p", indexReg, indexChild);

   if (indexReg)
      {
      indexValue = -1;
      }
   else
      {
      indexValue = indexChild->getInt();
      }

   // TODO: don't always require the VM thread
   //
   TR::RegisterDependencyConditions *deps =
      generateRegisterDependencyConditions((uint8_t) 0, 1, cg);

   deps->addPostCondition(
      cg->getVMThreadRegister(),
      (TR::RealRegister::RegNum)cg->getVMThreadRegister()->getAssociation(), cg);

   deps->stopAddingConditions();

   TR::LabelSymbol *mergeLabel = generateLabelSymbol(cg);
   mergeLabel->setInternalControlFlowMerge();
   TR::X86LabelInstruction *restartInstr = generateLabelInstruction(LABEL, node, mergeLabel, deps, cg);

   TR_OutlinedInstructions *arrayletOI =
      generateArrayletReference(
         node,
         loadOrStoreChild,
         checkInstr,
         boundCheckFailureLabel,
         mergeLabel,
         baseArrayReg,
         loadOrStoreReg,
         indexReg,
         indexValue,
         valueReg,
         needsBoundCheck,
         cg);

   arrayletOI->setMainlinePathRegisterUsageList(mainlineRUL);

   if (needsVMThread)
      cg->setVMThreadRequired(false);

   if (node->hasFoldedImplicitNULLCHK())
      {
      if (faultingInstruction)
         {
         faultingInstruction->setNeedsGCMap(0xFF00FFFF);
         faultingInstruction->setNode(node);
         }
      }

   if (arrayLengthChild)
      arrayLengthChild->setIsNonNegative(true);

   indexChild->setIsNonNegative(true);

   cg->decReferenceCount(loadOrStoreChild);
   cg->decReferenceCount(baseArrayChild);

   if (!needsBoundCheck)
      {
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
 * than the intel specification, a full mfence instrcution have to
 * be used for all three of them
 *
 * Due to performance penalty of mfence, a faster lockor on RSP is used
 * it achieve the same funtionaility but runs faster.
 */
TR::Register *J9::X86::TreeEvaluator::barrierFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   if (opCode == TR::fullFence && node->canOmitSync())
      {
      generateLabelInstruction(LABEL, node, generateLabelSymbol(cg), cg);
      }
   else if(cg->comp()->getOption(TR_X86UseMFENCE))
      {
      generateInstruction(MFENCE, node, cg);
      }
   else
      {
      TR::RealRegister *stackReg = cg->machine()->getX86RealRegister(TR::RealRegister::esp);
      TR::MemoryReference *mr = generateX86MemoryReference(stackReg, intptrj_t(0), cg);

      mr->setRequiresLockPrefix();
      generateMemImmInstruction(OR4MemImms, node, mr, 0, cg);
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
   traceMsg(comp, "\nnode %p has folded implicit nullchk: %d\n", node, node->hasFoldedImplicitNULLCHK());
   traceMsg(comp, "node %p is nonnull: %d\n", node, node->isNonNull());
   traceMsg(comp, "node %p needs branchAround: %d\n", node, needBranchAroundForNULL);

   TR::LabelSymbol *startLabel=NULL;
   TR::LabelSymbol *doneLabel=NULL;
   if (needBranchAroundForNULL)
      {
      startLabel = generateLabelSymbol(cg);
      doneLabel  = generateLabelSymbol(cg);

      generateLabelInstruction(LABEL, node, startLabel, true, cg);
      startLabel->setStartInternalControlFlow();
      }

   TR::Register *handleRegister = cg->intClobberEvaluate(handleNode);

   if (needBranchAroundForNULL)
      {
      // if handle is NULL, then just branch around the redirection
      generateRegRegInstruction(TESTRegReg(), node, handleRegister, handleRegister, cg);
      generateLabelInstruction(JE4, handleNode, doneLabel, cg);
      }

   // handle is not NULL or we're an implicit nullcheck, so go through forwarding pointer to get object
   TR::MemoryReference *handleMR = generateX86MemoryReference(handleRegister, node->getSymbolReference()->getOffset(), cg);
   TR::Instruction *forwardingInstr=generateRegMemInstruction(L4RegMem, handleNode, handleRegister, handleMR, cg);
   cg->setImplicitExceptionPoint(forwardingInstr);

   if (needBranchAroundForNULL)
      {
      TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions((uint8_t) 0, 1, cg);
      deps->addPostCondition(handleRegister, TR::RealRegister::NoReg, cg);

      // and we're done
      generateLabelInstruction(LABEL, node, doneLabel, deps, cg);

      doneLabel->setEndInternalControlFlow();
      }

   node->setRegister(handleRegister);
   cg->decReferenceCount(handleNode);

   return handleRegister;
   }


TR::Register *J9::X86::TreeEvaluator::ScopeCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "ScopeCHK node should have one child");
   TR::Node *child = node->getChild(0);
   cg->evaluate(child);
   cg->decReferenceCount(child);
   return NULL;
   }

TR::Register *J9::X86::TreeEvaluator::atccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "atccheck nodes should have 1 children");

   TR::Node *pendingAIELoad = node->getChild(0);
   TR_ASSERT(pendingAIELoad->getOpCodeValue() == TR::aload, "atccheck first child should be a compare test");

   cg->setVMThreadRequired(true);

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *doneLabel  = generateLabelSymbol(cg);

   startLabel->setStartInternalControlFlow();
   doneLabel->setEndInternalControlFlow();

   generateLabelInstruction(LABEL, node, startLabel, true, cg);

   // if there is no pending AIE, then just branch around the throw
   TR::Register *pendingAIERegister = cg->evaluate(pendingAIELoad);

   generateRegRegInstruction(TESTRegReg(), node, pendingAIERegister, pendingAIERegister, cg);
   generateLabelInstruction(JE4, node, doneLabel, true, cg);

   // here, we've got a pending AIE so that means we're interruptible
   // so throw the pending AIE here
   TR::Node *throwCallNode = TR::Node::createWithSymRef(TR::call, 1, 1, pendingAIELoad, node->getSymbolReference());
   cg->evaluate(throwCallNode);

   TR::Register *vmThreadRegister = cg->getVMThreadRegister();

   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t) 0, 2, cg);
   deps->addPostCondition(pendingAIERegister, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(vmThreadRegister, (TR::RealRegister::RegNum)vmThreadRegister->getAssociation(), cg);

   // and we're done
   generateLabelInstruction(LABEL, node, doneLabel, deps, cg);

   cg->setVMThreadRequired(false);

   cg->decReferenceCount(pendingAIELoad);

   return NULL;
   }

static
TR::Register * highestOneBit(TR::Node *node, TR::CodeGenerator *cg, TR::Register *reg, bool is64Bit)
   {
   // xor r1, r1
   // bsr r2, reg
   // setne r1
   // shl r1, r2
   TR::Register *scratchReg = cg->allocateRegister();
   TR::Register *bsrReg = cg->allocateRegister();
   generateRegRegInstruction(XORRegReg(is64Bit), node, scratchReg, scratchReg, cg);
   generateRegRegInstruction(BSRRegReg(is64Bit), node, bsrReg, reg, cg);
   generateRegInstruction(SETNE1Reg, node, scratchReg, cg);
   TR::RegisterDependencyConditions  *shiftDependencies = generateRegisterDependencyConditions((uint8_t)1, 1, cg);
   shiftDependencies->addPreCondition(bsrReg, TR::RealRegister::ecx, cg);
   shiftDependencies->addPostCondition(bsrReg, TR::RealRegister::ecx, cg);
   shiftDependencies->stopAddingConditions();
   generateRegRegInstruction(SHLRegCL(is64Bit), node, scratchReg, bsrReg, shiftDependencies, cg);
   cg->stopUsingRegister(bsrReg);
   return scratchReg;
   }

TR::Register *J9::X86::TreeEvaluator::integerHighestOneBit(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Node has a wrong number of children (i.e. !=1 )! ");
   TR::Node *child = node->getFirstChild();
   TR::Register *inputReg = cg->evaluate(child);
   TR::Register *resultReg = highestOneBit(node, cg, inputReg, TR::Compiler->target.is64Bit());
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
   if (TR::Compiler->target.is64Bit())
      {
      resultReg = highestOneBit(node, cg, inputReg, true);
      }
   else
      {
      //mask out low part result if high part is not 0
      //xor r1 r1
      //cmp inputHigh, 0
      //setne r1
      //dec r1
      //and resultLow, r1
      //ret resultHigh:resultLow
      TR::Register *inputLow = inputReg->getLowOrder();
      TR::Register *inputHigh = inputReg->getHighOrder();
      TR::Register *maskReg = cg->allocateRegister();
      TR::Register *resultHigh = highestOneBit(node, cg, inputHigh, false);
      TR::Register *resultLow = highestOneBit(node, cg, inputLow, false);
      generateRegRegInstruction(XOR4RegReg, node, maskReg, maskReg, cg);
      generateRegImmInstruction(CMP4RegImm4, node, inputHigh, 0, cg);
      generateRegInstruction(SETNE1Reg, node, maskReg, cg);
      generateRegInstruction(DEC4Reg, node, maskReg, cg);
      generateRegRegInstruction(AND4RegReg, node, resultLow, maskReg, cg);
      resultReg = cg->allocateRegisterPair(resultLow, resultHigh);
      cg->stopUsingRegister(maskReg);
      }
   cg->decReferenceCount(child);
   node->setRegister(resultReg);
   return resultReg;
   }

static
TR::Register *lowestOneBit(TR::Node *node, TR::CodeGenerator *cg, TR::Register *reg, bool is64Bit)
   {
   TR::Register *resultReg = cg->allocateRegister();
   generateRegRegInstruction(MOVRegReg(is64Bit), node, resultReg, reg, cg);
   generateRegInstruction(NEGReg(is64Bit), node, resultReg, cg);
   generateRegRegInstruction(ANDRegReg(is64Bit), node, resultReg, reg, cg);
   return resultReg;
   }

TR::Register *J9::X86::TreeEvaluator::integerLowestOneBit(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Node has a wrong number of children (i.e. !=1 )! ");
   TR::Node *child = node->getFirstChild();
   TR::Register *inputReg = cg->evaluate(child);
   TR::Register *reg = lowestOneBit(node, cg, inputReg, TR::Compiler->target.is64Bit());
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
   if (TR::Compiler->target.is64Bit())
      {
      resultReg = lowestOneBit(node, cg, inputReg, true);
      }
   else
      {
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
      generateRegRegInstruction(XOR4RegReg, node, scratchReg, scratchReg, cg);
      TR::Register *resultLow = lowestOneBit(node, cg, inputLow, false);
      generateRegInstruction(SETNE1Reg, node, scratchReg, cg);
      generateRegInstruction(DEC4Reg, node, scratchReg, cg);
      generateRegRegInstruction(AND4RegReg, node, scratchReg, inputHigh, cg);
      TR::Register *resultHigh = lowestOneBit(node, cg, scratchReg, false);
      cg->stopUsingRegister(scratchReg);
      resultReg = cg->allocateRegisterPair(resultLow, resultHigh);
      }
   node->setRegister(resultReg);
   cg->decReferenceCount(child);
   return resultReg;
   }


static
TR::Register *numberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg, TR::Register *reg, bool is64Bit, bool isLong)
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
   generateRegRegInstruction(XORRegReg(is64Bit), node, maskReg, maskReg, cg);
   generateRegRegInstruction(BSRRegReg(is64Bit), node, bsrReg, reg, cg);
   generateRegInstruction(SETE1Reg, node, maskReg, cg);
   generateRegInstruction(DECReg(is64Bit), node, maskReg, cg);
   generateRegInstruction(INCReg(is64Bit), node, bsrReg, cg);
   generateRegRegInstruction(ANDRegReg(is64Bit), node, bsrReg, maskReg, cg);
   generateRegImmInstruction(MOVRegImm4(is64Bit), node, maskReg, isLong ? 64 : 32, cg);
   generateRegRegInstruction(SUBRegReg(is64Bit), node, maskReg, bsrReg, cg);
   cg->stopUsingRegister(bsrReg);
   return maskReg;
   }

TR::Register *J9::X86::TreeEvaluator::integerNumberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Node has a wrong number of children (i.e. !=1 )! ");
   TR::Node* child = node->getFirstChild();
   TR::Register* inputReg = cg->evaluate(child);
   TR::Register* resultReg = numberOfLeadingZeros(node, cg, inputReg, false, false);
   node->setRegister(resultReg);
   cg->decReferenceCount(child);
   return resultReg;
   }

TR::Register *J9::X86::TreeEvaluator::longNumberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Node has a wrong number of children (i.e. !=1 )! ");
   TR::Node* child = node->getFirstChild();
   TR::Register* inputReg = cg->evaluate(child);
   TR::Register *resultReg = NULL;
   if (TR::Compiler->target.is64Bit())
      {
      resultReg = numberOfLeadingZeros(node, cg, inputReg, true, true);
      }
   else
      {
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
      generateRegRegInstruction(XOR4RegReg, node, maskReg, maskReg, cg);
      generateRegImmInstruction(CMP4RegImm4, node, inputHigh, 0, cg);
      generateRegInstruction(SETNE1Reg, node, maskReg, cg);
      generateRegInstruction(DEC4Reg, node, maskReg, cg);
      generateRegRegInstruction(AND4RegReg, node, resultLow, maskReg, cg);
      generateRegRegInstruction(ADD4RegReg, node, resultHigh, resultLow, cg);
      cg->stopUsingRegister(resultLow);
      cg->stopUsingRegister(maskReg);
      resultReg = resultHigh;
      }
   node->setRegister(resultReg);
   cg->decReferenceCount(child);
   return resultReg;
   }

static
TR::Register * numberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg, TR::Register *reg, bool is64Bit, bool isLong)
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
   generateRegRegInstruction(XORRegReg(is64Bit), node, tempReg, tempReg, cg);
   generateRegRegInstruction(BSFRegReg(is64Bit), node, bsfReg, reg, cg);
   generateRegInstruction(SETE1Reg, node, tempReg, cg);
   generateRegRegInstruction(MOVRegReg(is64Bit), node, maskReg, tempReg, cg);
   generateRegInstruction(DECReg(is64Bit), node, maskReg, cg);
   generateRegImmInstruction(SHLRegImm1(is64Bit), node, tempReg, isLong ? 6 : 5, cg);
   generateRegRegInstruction(ANDRegReg(is64Bit), node, bsfReg, maskReg, cg);
   generateRegRegInstruction(ADDRegReg(is64Bit), node, bsfReg, tempReg, cg);
   cg->stopUsingRegister(tempReg);
   cg->stopUsingRegister(maskReg);
   return bsfReg;
   }

TR::Register *J9::X86::TreeEvaluator::integerNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Node has a wrong number of children (i.e. !=1 )! ");
   TR::Node* child = node->getFirstChild();
   TR::Register* inputReg = cg->evaluate(child);
   TR::Register *resultReg = numberOfTrailingZeros(node, cg, inputReg, TR::Compiler->target.is64Bit(), false);
   node->setRegister(resultReg);
   cg->decReferenceCount(child);
   return resultReg;
   }

TR::Register *J9::X86::TreeEvaluator::longNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Node has a wrong number of children (i.e. !=1 )! ");
   TR::Node * child = node->getFirstChild();
   TR::Register * inputReg = cg->evaluate(child);
   TR::Register * resultReg = NULL;
   if (TR::Compiler->target.is64Bit())
      {
      resultReg = numberOfTrailingZeros(node, cg, inputReg, true, true);
      }
   else
      {
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
      generateRegRegInstruction(XOR4RegReg, node, maskReg, maskReg, cg);
      generateRegImmInstruction(CMP4RegImm4, node, resultLow, 32, cg);
      generateRegInstruction(SETNE1Reg, node, maskReg, cg);
      generateRegInstruction(DEC4Reg, node, maskReg, cg);
      generateRegRegInstruction(AND4RegReg, node, maskReg, resultHigh, cg);
      generateRegRegInstruction(ADD4RegReg, node, resultLow, maskReg, cg);
      cg->stopUsingRegister(resultHigh);
      cg->stopUsingRegister(maskReg);
      resultReg = resultLow;
      }
   node->setRegister(resultReg);
   cg->decReferenceCount(child);
   return resultReg;
   }

static
TR::Register *bitCount(TR::Node *node, TR::CodeGenerator *cg, TR::Register *reg, bool is64Bit)
   {
   TR::Register *bsfReg = cg->allocateRegister();
   generateRegRegInstruction(POPCNTRegReg(is64Bit), node, bsfReg, reg, cg);
   return bsfReg;
   }

TR::Register *J9::X86::TreeEvaluator::integerBitCount(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Node has a wrong number of children (i.e. !=1 )! ");
   TR::Node* child = node->getFirstChild();
   TR::Register* inputReg = cg->evaluate(child);
   TR::Register* resultReg = bitCount(node, cg, inputReg, TR::Compiler->target.is64Bit());
   node->setRegister(resultReg);
   cg->decReferenceCount(child);
   return resultReg;
   }

TR::Register *J9::X86::TreeEvaluator::longBitCount(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Node has a wrong number of children (i.e. !=1 )! ");
   TR::Node * child = node->getFirstChild();
   TR::Register * inputReg = cg->evaluate(child);
   TR::Register * resultReg = NULL;
   if (TR::Compiler->target.is64Bit())
      {
      resultReg = bitCount(node, cg, inputReg, true);
      }
   else
      {
      //add low result and hight result together
      TR::Register * inputHigh = inputReg->getHighOrder();
      TR::Register * inputLow = inputReg->getLowOrder();
      TR::Register * resultLow = bitCount(node, cg, inputLow, false);
      TR::Register * resultHigh = bitCount(node, cg, inputHigh, false);
      generateRegRegInstruction(ADD4RegReg, node, resultLow, resultHigh, cg);
      cg->stopUsingRegister(resultHigh);
      resultReg = resultLow;
      }
   node->setRegister(resultReg);
   cg->decReferenceCount(child);
   return resultReg;
   }


// ----------------------------------------------------------------------------


TR::Register *J9::X86::TreeEvaluator::VMcheckcastEvaluator(TR::Node          *node,
                                                       TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   bool aotStats= comp->getOption(TR_EnableAOTStats);
   if (aotStats)
      {
      ((TR_JitPrivateConfig *)fej9->getPrivateConfig())->aotStats->numVMCheckCastEvaluator++;
      }

   // Evaluate the children of the checkcast node
   //
   TR::Node        *objectRef      = node->getFirstChild();
   TR::Register    *objectReg      = cg->evaluate(objectRef);
   TR::Node        *castClassRef   = node->getSecondChild();
   TR::Register    *castClassReg   = NULL;
   TR::Register    *objectClassReg = NULL;
   TR::Register    *testerReg      = NULL;
   TR::Register    *classPointerReg = NULL;
   uint32_t        numDeps        = 2;
   int32_t         classDepth     = -1;

   if (!cg->useSSEForDoublePrecision())
      {
      TR::RegisterDependencyConditions  *fpSpillDependency = generateRegisterDependencyConditions(1, 0, cg);
      fpSpillDependency->addPreCondition(NULL, TR::RealRegister::AllFPRegisters, cg);
      generateInstruction(FPREGSPILL, node, fpSpillDependency, cg);
      }

   bool isCheckCastOrNullCheck    = (node->getOpCodeValue() == TR::checkcastAndNULLCHK);

   bool performEqualityTestInline   = false;
   bool performSuperclassTestInline = false;
   bool performSuperclassTestOutlined = false;
   bool castClassIs32Bit     = classAddressFitsIn32Bits(castClassRef, cg);
   bool profiledClassIs32Bit = profiledAddressFitsIn32Bits(castClassRef, cg);
   bool use64BitClasses      = TR::Compiler->target.is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders();

   // See if inline tests are appropriate
   //
   TR::SymbolReference *castClassSymRef = castClassRef->getSymbolReference();
   TR::StaticSymbol    *castClassSym    = castClassSymRef ? castClassSymRef->getSymbol()->getStaticSymbol() : NULL;
   TR::MethodSymbol    *helperSym       = node->getSymbol()->castToMethodSymbol();

   generatePrefetchAfterHeaderAccess (node, objectReg, cg);

   if (!comp->getOption(TR_DisableInlineCheckCast))
      {
      performEqualityTestInline   = TR::TreeEvaluator::instanceOfOrCheckCastNeedEqualityTest(node, cg);
      performSuperclassTestInline = TR::TreeEvaluator::instanceOfOrCheckCastNeedSuperTest(node, cg);
      //if (performSuperclassTestInline)
      //   performSuperclassTestOutlined = checkcastShouldOutlineSuperClassTest (node, cg);
      //if (performEqualityTestInline && performSuperclassTestOutlined)
      //   performSuperclassTestInline = false;
      }

   bool isFinalClass          = castClassSymRef ? castClassSymRef->isNonArrayFinal(comp) : false;
   bool needsHelperCall       = needHelperCall((performSuperclassTestInline || performEqualityTestInline), isFinalClass, cg);
   bool testCache             = needTestCache(castClassRef, true, needsHelperCall, (performSuperclassTestInline || performEqualityTestInline), cg);

   if (TR::comp()->getOption(TR_TraceCG))
      traceMsg(TR::comp(), "VMCHECKCASTEVAL: test for %p - isFinalClass: %d, needsHelperCall: %d, testCache: %d, performEqualityTestInline: %d, performSuperclassTestInline: %d, performSuperclassTestOutlined: %d\n", node, isFinalClass, needsHelperCall, testCache, performEqualityTestInline, performSuperclassTestInline, performSuperclassTestOutlined);

   if (!performEqualityTestInline &&
       !performSuperclassTestInline &&
       !testCache)
      {
      // No inline tests to be performed, just call the helper
      //
      if (isCheckCastOrNullCheck)
         {
         // Just touch the memory in case this is a NULL pointer and we need to throw
         // the exception after the checkcast. If the checkcast was combined with nullpointer
         // there's nobody after the checkcast to throw the exception.
         TR::MemoryReference *objectClassMR = generateX86MemoryReference(objectReg, TR::Compiler->om.offsetOfObjectVftField(), cg);
         TR::Instruction *implicitExceptionPointInstr = generateMemImmInstruction(TEST1MemImm1, node, objectClassMR, 0, cg);
         cg->setImplicitExceptionPoint(implicitExceptionPointInstr);
         implicitExceptionPointInstr->setNeedsGCMap(0xFF00FFFF);
         // find the bytecodeinfo of the NULLCHK
         // that was compacted
         TR::Node *implicitExceptionPointNode = comp->findNullChkInfo(node);
         implicitExceptionPointInstr->setNode(implicitExceptionPointNode);
         }

      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::call);
      TR::TreeEvaluator::performCall(node, false, false, cg);
      TR::Node::recreate(node, opCode);

      if (aotStats)
         {
         ((TR_JitPrivateConfig *)fej9->getPrivateConfig())->aotStats->numCheckCastVMHelperInstructions++;
         }

      return NULL;
      }

   if (castClassRef->getOpCodeValue() != TR::loadaddr ||
       castClassRef->getRegister() != NULL           ||
       castClassSym == NULL                          ||
       // AMD64 must evaluate the cast class into a register for 64-bit comparisons.
       //
       !castClassIs32Bit)
      {
      castClassReg = cg->evaluate(castClassRef);
      numDeps++;
      }

   bool rematerializeVMThreadInSnippet = false;

   static const char *allowVMThreadRemat = feGetEnv("TR_allowVMThreadRemat");
   if (allowVMThreadRemat && cg->supportsFS0VMThreadRematerialization())
      {
      rematerializeVMThreadInSnippet = true;
      numDeps--; // remove vmThread (ebp) as dependency
      }

   if (!rematerializeVMThreadInSnippet)
      cg->setVMThreadRequired(true);

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *fallThru   = generateLabelSymbol(cg);
   startLabel->setStartInternalControlFlow();
   fallThru->setEndInternalControlFlow();
   generateLabelInstruction(LABEL, node, startLabel, cg);
   TR_X86ScratchRegisterManager *srm = cg->generateScratchRegisterManager();

   static const char *inlinedHelpers = feGetEnv("TR_InlinedHelperCallsCheckcast");

   if (TR::Compiler->target.is32Bit())
      inlinedHelpers = NULL;

   TR::LabelSymbol *callHelper = generateLabelSymbol(cg);
   if (!(testCache && inlinedHelpers))
      {
      TR_OutlinedInstructions *outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::call, NULL,
                                                                                 callHelper, fallThru, rematerializeVMThreadInSnippet, cg );
      cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);
      cg->generateDebugCounter(
         outlinedHelperCall->getFirstInstruction(),
         TR::DebugCounter::debugCounterName(comp, "helperCalls/%s/(%s)/%d/%d", node->getOpCode().getName(), comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
         1, TR::DebugCounter::Cheap);
      }
   // First test for null object reference if necessary
   //
   // this is used to setup the implicit exception
   // point for a checkcastAndNULLCHK node
   TR::Instruction *implicitExceptionPointInstr = NULL;
   if (!objectRef->isNonNull())
      {
      if (!isCheckCastOrNullCheck)
         {
         genNullTest(node, objectReg, cg);
         generateLabelInstruction(JE4, node, fallThru, cg);
         }
      }

   // If we're going to need the object class in a register, get it now
   //
   TR::MemoryReference *objectClassMR = generateX86MemoryReference(objectReg, TR::Compiler->om.offsetOfObjectVftField(), cg);
   if (performSuperclassTestInline || testCache)
      {
      objectClassReg = cg->allocateRegister();
      implicitExceptionPointInstr = generateRegMemInstruction(LRegMem(use64BitClasses), node, objectClassReg, objectClassMR, cg);
      TR::TreeEvaluator::generateVFTMaskInstruction(node, objectClassReg, cg);
      numDeps++;
      }

   bool generatedQuickProfiledTest = false;
   TR::LabelSymbol *continueChecksLabel = generateLabelSymbol(cg);

   bool performSingleProfiledTest = !testCache && !castClassReg && profiledClassIs32Bit && performSuperclassTestInline;
   if (performSingleProfiledTest)
      {
      benefitAnalysisOnBeforeSuperClassTest(castClassRef, performSingleProfiledTest, castClassSym, cg);
      }

   if (performSingleProfiledTest)
      {
      generatedQuickProfiledTest = generateSingleProfiledCacheTest (
           castClassRef,
              castClassSymRef,
              objectClassReg,
              continueChecksLabel,
              fallThru,
              true,
              cg);
      }


   if (testCache)
      {
      TR::LabelSymbol *secondCacheLabel = generateLabelSymbol(cg);
      if (TR::Compiler->target.is64Bit())
         {
         if (!castClassIs32Bit || !profiledClassIs32Bit || inlinedHelpers)
            {
            testerReg = cg->allocateRegister();
            numDeps++;
            }
         }

      generateProfiledCacheTest (castClassRef,
                                 castClassSymRef,
                                 objectClassReg,
                                 (profiledClassIs32Bit) ? NULL : testerReg,
                                 secondCacheLabel,
                                 fallThru,
                                 true,
                                 cg);

      generateLabelInstruction(LABEL, node, secondCacheLabel, cg);

      if (inlinedHelpers)
         {
         classPointerReg  = generateClassCacheTestInlined (castClassRef,
                                     castClassSymRef,
                                     objectClassReg,
                                     (castClassIs32Bit) ? NULL : testerReg,
                                     fallThru,
                                     cg);

         if (classPointerReg)
            numDeps++;

         TR_OpaqueClassBlock* castClassAddr = TR::TreeEvaluator::getCastClassAddress(node);
         if (castClassAddr)
            {
            if (castClassIs32Bit)
               generateRegImm64Instruction(MOV4RegImm4, node, testerReg, (uintptrj_t)castClassAddr, cg);
            else
               generateRegImm64Instruction(MOV8RegImm64, node, testerReg, (uintptrj_t)castClassAddr, cg);
            }
         else
            {
            TR::SymbolReference *symRef = castClassRef->getSymbolReference();
            TR::MemoryReference *memRef = generateX86MemoryReference(symRef, cg);
            TR_X86OpCodes op = LEARegMem();

            if (TR::Compiler->om.generateCompressedObjectHeaders())
               op = LEA4RegMem;

            TR::Instruction *instr = generateRegMemInstruction(op, castClassRef, testerReg, memRef, cg);
            if (cg->wantToPatchClassPointer(castClassAddr, node))
               {
               // devinmp: I think this has no effect; instr has no immediate source operand.
               comp->getStaticHCRPICSites()->push_front(instr);
               }
            }

         TR::MethodSymbol *helperMethod = node->getSymbol()->castToMethodSymbol();
         TR::Instruction *instr = generateImmSymInstruction(CALLImm4, node, (uint32_t)(uintptrj_t)helperMethod->getMethodAddress(), node->getSymbolReference(), cg);
         instr->setNeedsGCMap(0xFF00FFFF);
         }
      else
         {
         classPointerReg  = generateClassCacheTest (castClassRef,
                              castClassSymRef,
                              objectClassReg,
                              (castClassIs32Bit) ? NULL : testerReg,
                              callHelper,
                              cg);
         if (classPointerReg)
            numDeps++;
         }
      }

   generateLabelInstruction(LABEL, node, continueChecksLabel, cg);

   // Generate the inline test for equality of the object's class with the given
   // class object
   //
   if (performEqualityTestInline)
      {
      TR::Instruction *instr = genEqualityInstanceOfTest(node, objectClassReg, objectClassMR, castClassReg, srm, cg, NULL);
      if (!implicitExceptionPointInstr)
         implicitExceptionPointInstr = instr;
      if (performSuperclassTestInline)
         generateLabelInstruction(JE4, node, fallThru, cg);
      else
         generateLabelInstruction(JNE4, node, callHelper, cg);
      }

   // Generate the inline superclass test
   //
   if (performSuperclassTestInline)
      {
      /*static char *quickProfiledTestOnly = feGetEnv("TR_quickProfiledTestOnly");
      if (quickProfiledTestOnly && generatedQuickProfiledTest)
         {
         generateLabelInstruction(JMP4, node, callHelper, cg);
         }
      else*/
         {
         TR_OpaqueClassBlock * clazz = (TR_OpaqueClassBlock *) castClassSym->getStaticAddress();
         classDepth = (int32_t)TR::Compiler->cls.classDepthOf(clazz);

         if (!testerReg)
            {
            testerReg = cg->allocateRegister();
            numDeps++;
            }

         genSuperClassInstanceOfTest(node, objectClassReg, testerReg, castClassReg, classDepth, callHelper, NULL, NULL, srm, cg, NULL, NULL,node->getSecondChild(),false, false);
         generateLabelInstruction(JNE4, node, callHelper, cg);
         }
      }

   if (isCheckCastOrNullCheck &&
         implicitExceptionPointInstr &&
         !objectRef->isNonNull())
      {
      cg->setImplicitExceptionPoint(implicitExceptionPointInstr);
      implicitExceptionPointInstr->setNeedsGCMap(0xFF00FFFF);
      // find the bytecodeinfo of the
      // NULLCHK that was compacted
      TR::Node *implicitExceptionPointNode = comp->findNullChkInfo(node);
      implicitExceptionPointInstr->setNode(implicitExceptionPointNode);
      }

   // Create dependencies for the registers used.
   // The object register and cast class register (if any) must be the first
   // dependencies, since the helper call snippet needs to find them to grab
   // the real registers from them.
   //
   numDeps += srm->numAvailableRegisters();
   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)numDeps, cg);
   if (testCache && inlinedHelpers)
      deps->addPostCondition(objectReg, TR::RealRegister::esi, cg);
   else
      deps->addPostCondition(objectReg, TR::RealRegister::NoReg, cg);
   if (castClassReg)
      deps->addPostCondition(castClassReg, TR::RealRegister::NoReg, cg);
   if (!rematerializeVMThreadInSnippet)
      deps->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);
   if (objectClassReg)
      deps->addPostCondition(objectClassReg, TR::RealRegister::NoReg, cg);
   if (testerReg)
      {
      if (testCache && inlinedHelpers)
         deps->addPostCondition(testerReg, TR::RealRegister::eax, cg);
      else
         deps->addPostCondition(testerReg, TR::RealRegister::NoReg, cg);
      }

   if (classPointerReg)
      deps->addPostCondition(classPointerReg, TR::RealRegister::NoReg, cg);

   srm->addScratchRegistersToDependencyList(deps);
   deps->stopAddingConditions();
   generateLabelInstruction(LABEL, node, fallThru, deps, cg);
   srm->stopUsingRegisters();

   if (!rematerializeVMThreadInSnippet)
      cg->setVMThreadRequired(false);

   if (objectClassReg)
      cg->stopUsingRegister(objectClassReg);
   if (testerReg)
      cg->stopUsingRegister(testerReg);

   // Decrement use counts on the children
   //
   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());

   return NULL;
   }

TR::Register *J9::X86::TreeEvaluator::VMifArrayCmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // Set up the correct sense for the true and false labels
   //

   TR::LabelSymbol *branchLabel = node->getBranchDestination()->getNode()->getLabel();
   int32_t  value              = node->getSecondChild()->getInt();
   bool     branchOnEqaul;

   // only two cases allowed controlled in canBeHandledByIfArraycmpHelper
   if (value == 0 && node->getOpCodeValue() == TR::ificmpeq)
      {
      branchOnEqaul = true;
      }
   else if (value == 0 && node->getOpCodeValue() == TR::ificmpne)
      {
      branchOnEqaul = false;
      }
   else
      {
      TR_ASSERT(false, "unexpected case\n");
      }

   TR::TreeEvaluator::SSE2IfArraycmpEvaluator(node->getFirstChild(), branchLabel, branchOnEqaul, cg);

   cg->decReferenceCount(node->getSecondChild());
   cg->decReferenceCount(node->getFirstChild());

   if (node->getNumChildren() == 3)
      {
      TR::Node *thirdChild = node->getThirdChild();
      TR_ASSERT(thirdChild->getOpCodeValue() == TR::GlRegDeps, "third child of a compare should be a TR_GlRegDeps");
      cg->evaluate(thirdChild);

      List<TR::Register> popRegisters(cg->trMemory());
      TR::RegisterDependencyConditions *thirdChildDeps = generateRegisterDependencyConditions(thirdChild, cg, 0, &popRegisters);
      cg->decReferenceCount(thirdChild);
      TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
      generateLabelInstruction(LABEL, node, doneLabel, thirdChildDeps, cg);
      }
   return NULL;
   }

// Comparing instanceof to a constant
//
TR::Register *J9::X86::TreeEvaluator::VMifInstanceOfEvaluator(TR::Node          *node,
                                                          TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   bool aotStats= comp->getOption(TR_EnableAOTStats);
   if (aotStats)
      {
      ((TR_JitPrivateConfig *)fej9->getPrivateConfig())->aotStats->numVMIfInstanceOfEvaluator++;
      }

   TR::Node *instanceofNode = node->getFirstChild();
   TR::Node *valueNode      = node->getSecondChild();
   TR::Node *thirdChild     = NULL;
   int32_t  value          = valueNode->getInt();

   // Set up the correct sense for the true and false labels
   //
   TR_X86OpCodes  branchOpCode, reverseBranchOpCode;
   TR::LabelSymbol *branchLabel = node->getBranchDestination()->getNode()->getLabel();
   bool            branchOnTrue;

   if ((node->getOpCodeValue() == TR::ificmpeq && value == 1) ||
       (node->getOpCodeValue() != TR::ificmpeq && value == 0))
      {
      branchOnTrue = true;
      branchOpCode = JE4;
      reverseBranchOpCode = JNE4;
      }
   else
      {
      branchOnTrue = false;
      branchOpCode = JNE4;
      reverseBranchOpCode = JE4;
      }

   TR::Node *objectRef    = instanceofNode->getFirstChild();
   TR::Node *castClassRef = instanceofNode->getSecondChild();
   int32_t  classDepth   = -1;

   bool performEqualityTestInline   = false;
   bool performSuperclassTestInline = false;
   bool performHelperCall           = true;
   bool performReferenceArrayTestInline = false;
   bool castClassIsFinal            = false;
   bool castClassIs32Bit            = classAddressFitsIn32Bits(castClassRef, cg);
   bool profiledClassIs32Bit        = profiledAddressFitsIn32Bits(castClassRef, cg);
   bool use64BitClasses             = TR::Compiler->target.is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders();

   TR::RegisterDependencyConditions  *deps;

   // See if inline tests are appropriate
   //
   TR::SymbolReference *castClassSymRef = castClassRef->getSymbolReference();
   TR::StaticSymbol    *castClassSym    = castClassSymRef ? castClassSymRef->getSymbol()->getStaticSymbol() : NULL;
   TR::MethodSymbol    *helperSym       = instanceofNode->getSymbol()->castToMethodSymbol();

   if (node->getNumChildren() == 3)
      {
      thirdChild = node->getChild(2);
      }

   if (!comp->getOption(TR_DisableInlineCheckCast))
      {
      performEqualityTestInline   = TR::TreeEvaluator::instanceOfOrCheckCastNeedEqualityTest(instanceofNode, cg);
      performSuperclassTestInline = TR::TreeEvaluator::instanceOfOrCheckCastNeedSuperTest(instanceofNode, cg);
      }

   castClassIsFinal           = castClassSymRef ? castClassSymRef->isNonArrayFinal(comp) : false;
   performHelperCall          = needHelperCall((performSuperclassTestInline || performEqualityTestInline), castClassIsFinal, cg);
   bool testCache             = needTestCache(castClassRef, true, performHelperCall, (performSuperclassTestInline || performEqualityTestInline), cg);

   // new logic only affect X86
   if (TR::TreeEvaluator::instanceOfOrCheckCastIsFinalArray(instanceofNode, cg))
      {
      performEqualityTestInline = true;
      performSuperclassTestInline = false;
      performHelperCall = false;
      testCache = false;
      }
   else if (TR::TreeEvaluator::instanceOfOrCheckCastIsJavaLangObjectArray(instanceofNode, cg)) // array of Object
      {
      performEqualityTestInline = false;
      performSuperclassTestInline = false;
      performHelperCall = false;
      performReferenceArrayTestInline = true;
      }

   bool rematerializeVMThreadInSnippet = false;

   if (performEqualityTestInline || performSuperclassTestInline || testCache || performReferenceArrayTestInline)
      {
      TR::Register    *objectReg                           = cg->evaluate(objectRef);
      TR::Register    *castClassReg                        = NULL;
      TR::Register    *objectClassReg                      = NULL;
      TR::Register    *testerReg                           = NULL;
      TR::Register    *helperTargetReg                     = NULL;
      TR::RegisterDependencyConditions  *thirdChildDeps = NULL;
      List<TR::Register> popRegisters(cg->trMemory());
      uint32_t        numDeps                             = 2;

      static const char *allowVMThreadRemat = feGetEnv("TR_allowVMThreadRemat");
      if (allowVMThreadRemat && cg->supportsFS0VMThreadRematerialization())
         {
         rematerializeVMThreadInSnippet = true;
         numDeps--; // remove vmThread (ebp) dependency
         }

      TR_ASSERT(!performHelperCall || testCache, "We shouldn't be calling helper if we are here");

      generatePrefetchAfterHeaderAccess (instanceofNode, objectReg, cg);

      if (castClassRef->getOpCodeValue() != TR::loadaddr ||
          castClassRef->getRegister() != NULL           ||
          castClassSym == NULL                          ||

          // AMD64 must evaluate the cast class into a register for 64-bit comparisons.
          //
          !castClassIs32Bit)
         {
         castClassReg = cg->evaluate(castClassRef);
         numDeps++;
         }

      if (thirdChild)
         {
         TR_ASSERT(thirdChild->getOpCodeValue() == TR::GlRegDeps, "third child of a compare should be a TR::GlRegCHK");
         cg->evaluate(thirdChild);

         thirdChildDeps = generateRegisterDependencyConditions(thirdChild, cg, 0, &popRegisters);
         cg->decReferenceCount(thirdChild);
         }

      // Create a start and end label for the internal control flow sequence
      //
      TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *fallThru   = generateLabelSymbol(cg);
      startLabel->setStartInternalControlFlow();
      fallThru->setEndInternalControlFlow();
      TR_X86ScratchRegisterManager *srm = cg->generateScratchRegisterManager();

      TR::LabelSymbol *callHelper = generateLabelSymbol(cg);
      TR::LabelSymbol *superClassTestNext = NULL;

      generateLabelInstruction(LABEL, node, startLabel, cg);

      if (!rematerializeVMThreadInSnippet)
         cg->setVMThreadRequired(true);

      TR::LabelSymbol *trueLabel, *falseLabel;
      if (branchOnTrue)
         {
         trueLabel  = branchLabel;
         falseLabel = fallThru;
         }
      else
         {
         falseLabel = branchLabel;
         trueLabel  = fallThru;
         }

      // First test for null object reference.
      //
      if (!objectRef->isNonNull())
         {
         genNullTest(node, objectReg, cg);
         generateLabelInstruction(JE4, node, falseLabel, cg);
         }

      // If we're going to need the object class in a register, get it now
      //
      TR::MemoryReference *objectClassMR = generateX86MemoryReference(objectReg, TR::Compiler->om.offsetOfObjectVftField(), cg);
      if (performSuperclassTestInline || testCache || performReferenceArrayTestInline)
         {
         objectClassReg = cg->allocateRegister();
         generateRegMemInstruction(LRegMem(use64BitClasses), node, objectClassReg, objectClassMR, cg);
	 TR::TreeEvaluator::generateVFTMaskInstruction(node, objectClassReg, cg);
         numDeps++;
         }

      bool performSingleProfiledTest = !testCache && !castClassReg && profiledClassIs32Bit && performSuperclassTestInline;
      if (performSingleProfiledTest)
         {
         benefitAnalysisOnBeforeSuperClassTest(instanceofNode, performSingleProfiledTest, castClassSym, cg);
         }

      if (performSingleProfiledTest)
         {
         generateSingleProfiledCacheTest (
                 castClassRef,
                 castClassSymRef,
                 objectClassReg,
                 falseLabel,
                 trueLabel,
                 false,
                 cg);
         }

      if (testCache && !performReferenceArrayTestInline)
         {
         helperTargetReg = cg->allocateRegister();
         numDeps++;
         if (TR::Compiler->target.is64Bit())
            {
            if (!castClassIs32Bit || !profiledClassIs32Bit)
               {
               testerReg = cg->allocateRegister();
               numDeps++;
               }
            }
         }

      if (performSuperclassTestInline || performReferenceArrayTestInline)
         {
         testerReg = cg->allocateRegister();
         numDeps++;
         }

      static const char *inlinedHelpers = feGetEnv("TR_InlinedHelperCallsIfInstanceOf");

      if (thirdChild)
         inlinedHelpers = NULL;

      if (TR::Compiler->target.is32Bit())
         inlinedHelpers = NULL;

      if (testCache)
         {
         TR_ASSERT(!performEqualityTestInline && !performSuperclassTestInline, "Test cache should be generated only when we can't do other inlined tests");

         TR::LabelSymbol *callHelper = generateLabelSymbol(cg);
         TR::LabelSymbol *restartLabel = generateLabelSymbol(cg);
         if (!inlinedHelpers && !performReferenceArrayTestInline)
            {
            TR_OutlinedInstructions *outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(instanceofNode, TR::icall, helperTargetReg, callHelper,
                                                                                                           restartLabel, rematerializeVMThreadInSnippet, cg);
            cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);
            cg->generateDebugCounter(
               outlinedHelperCall->getFirstInstruction(),
               TR::DebugCounter::debugCounterName(comp, "helperCalls/%s/(%s)/%d/%d", node->getOpCode().getName(), comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
               1, TR::DebugCounter::Cheap);
            }
         //printf("***%s\n",comp->getCurrentMethod()->signature());


         generateProfiledCacheTest (castClassRef,
                                    castClassSymRef,
                                    objectClassReg,
                                    (profiledClassIs32Bit) ? NULL : testerReg,
                                    falseLabel,
                                    trueLabel,
                                    false,
                                    cg);


         if (inlinedHelpers)
            {
            if(!performReferenceArrayTestInline)
            generateInstanceOfCacheTestInline (instanceofNode,
                                               castClassRef,
                                               castClassSymRef,
                                               objectClassReg,
                                               helperTargetReg, // helperTargetReg is used as temp register in the function
                                               falseLabel,
                                               trueLabel,
                                               restartLabel,
                                               cg);
            }
         else
            {
            if(!performReferenceArrayTestInline)
            generateInstanceOfCacheTest (castClassRef,
                                      castClassSymRef,
                                      objectClassReg,
                                      helperTargetReg, // helperTargetReg is used as temp register in the function
                                      falseLabel,
                                      trueLabel,
                                      callHelper,
                                      restartLabel,
                                      cg);
            }

         // test jitInstanceOf_result, 1
         // when jitInstanceOf return 1 means instanceof is true. return 0 means instanceof is false
         if (!performReferenceArrayTestInline)
            {
            if (trueLabel != fallThru)
               generateLabelInstruction (JNE4, node, trueLabel, cg);
            else
               generateLabelInstruction (JE4, node, falseLabel, cg);
            }
         }

      // Generate the inline test for equality of the object's class with the given
      // class object
      //
      if (performEqualityTestInline)
         {
         genEqualityInstanceOfTest(instanceofNode, objectClassReg, objectClassMR, castClassReg, srm, cg, NULL);

         // If this is known to be a final class the above tests are all that are
         // needed.
         //
         if (!performSuperclassTestInline)
            generateLabelInstruction(branchOpCode, node, branchLabel, cg);
         else
            generateLabelInstruction(JE4, node, trueLabel, cg);
         }

      // Generate the inline superclass test
      //
      if (performSuperclassTestInline)
         {
         if (castClassSym)
            {
            TR_OpaqueClassBlock * clazz = (TR_OpaqueClassBlock *)castClassSym->getStaticAddress();
            classDepth = (int32_t)TR::Compiler->cls.classDepthOf(clazz);
            }

         genSuperClassInstanceOfTest(instanceofNode, objectClassReg, testerReg, castClassReg, classDepth, falseLabel, trueLabel, NULL, srm, cg, NULL, NULL,instanceofNode->getSecondChild(),false, true);

         generateLabelInstruction(branchOpCode, node, branchLabel, cg);
         }

      if (performReferenceArrayTestInline)
         {
         genIsReferenceArrayTest(node, objectClassReg, testerReg, falseLabel, trueLabel, cg);
         generateLabelInstruction(branchOpCode, node, branchLabel, cg);
         }

      numDeps += srm->numAvailableRegisters();

      if (thirdChildDeps)
         deps = thirdChildDeps->clone(cg, numDeps);
      else
         deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)numDeps, cg);

      if (testCache && inlinedHelpers)
         deps->unionPostCondition(objectReg, TR::RealRegister::esi, cg);
      else
         deps->unionPostCondition(objectReg, TR::RealRegister::NoReg, cg);

      if (castClassReg)
         deps->addPostCondition(castClassReg, TR::RealRegister::NoReg, cg);

      if (objectClassReg)
         deps->addPostCondition(objectClassReg, TR::RealRegister::NoReg, cg);

      if (helperTargetReg)
         if (testCache && inlinedHelpers)
            deps->addPostCondition(helperTargetReg, TR::RealRegister::eax, cg);
         else
            deps->addPostCondition(helperTargetReg, TR::RealRegister::NoReg, cg);

      if (!rematerializeVMThreadInSnippet)
         deps->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);

      if (testerReg)
         deps->addPostCondition(testerReg, TR::RealRegister::NoReg, cg);

      srm->addScratchRegistersToDependencyList(deps);
      deps->stopAddingConditions();
      generateLabelInstruction(LABEL, node, fallThru, deps, cg);
      srm->stopUsingRegisters();

      if (!rematerializeVMThreadInSnippet)
         cg->setVMThreadRequired(false);

      if (!popRegisters.isEmpty())
         {
         ListIterator<TR::Register> popRegsIt(&popRegisters);
         for (TR::Register *popRegister = popRegsIt.getFirst();
              popRegister != NULL;
              popRegister = popRegsIt.getNext())
            {
            generateFPSTiST0RegRegInstruction(FSTRegReg, node, popRegister, popRegister, cg);
            cg->stopUsingRegister(popRegister);
            }
         }

      cg->decReferenceCount(objectRef);
      cg->decReferenceCount(castClassRef);
      cg->decReferenceCount(instanceofNode);
      cg->decReferenceCount(valueNode);

      if (objectClassReg)
         cg->stopUsingRegister(objectClassReg);
      if (testerReg)
         cg->stopUsingRegister(testerReg);
      if (helperTargetReg)
         cg->stopUsingRegister(helperTargetReg);

      return NULL;
      }
   else
      // If the helper is to be called, just call it and branch on the result
      //
      {
      TR_ASSERT(performHelperCall, "Should be either inlining tests or calling helper\n");

      if (aotStats)
         {
         ((TR_JitPrivateConfig *)fej9->getPrivateConfig())->aotStats->numIfInstanceOfVMHelperInstructions++;
         }

      List<TR::Register> popRegisters(cg->trMemory());

      TR::ILOpCodes opCode = instanceofNode->getOpCodeValue();
      TR::Node::recreate(instanceofNode, TR::icall);
      TR::Register *targetRegister = TR::TreeEvaluator::directCallEvaluator(instanceofNode, cg);
      TR::Node::recreate(instanceofNode, opCode);

      generateRegRegInstruction(TEST4RegReg, node, targetRegister, targetRegister, cg);

      if (node->getNumChildren() == 3)
         {
         TR::Node *glRegDeps = node->getChild(2);

         TR_ASSERT(glRegDeps->getOpCodeValue() == TR::GlRegDeps, "third child of a compare should be a TR::GlRegCHK");
         cg->evaluate(glRegDeps);

         deps = generateRegisterDependencyConditions(glRegDeps, cg, 0, &popRegisters);
         cg->decReferenceCount(glRegDeps);
         }
      else
         {
         deps = NULL;
         }

      if (deps)
         generateLabelInstruction(reverseBranchOpCode, node, branchLabel, deps, cg);
      else
         generateLabelInstruction(reverseBranchOpCode, node, branchLabel, true, cg);

      if (!popRegisters.isEmpty())
         {
         ListIterator<TR::Register> popRegsIt(&popRegisters);
         for (TR::Register *popRegister = popRegsIt.getFirst();
              popRegister != NULL;
              popRegister = popRegsIt.getNext())
            {
            generateFPSTiST0RegRegInstruction(FSTRegReg, node, popRegister, popRegister, cg);
            cg->stopUsingRegister(popRegister);
            }
         }

      cg->decReferenceCount(instanceofNode);
      cg->decReferenceCount(valueNode);
      return NULL;
      }


   return NULL;
   }

// Generic instanceof generating a 1 or 0 result
//
TR::Register *
J9::X86::TreeEvaluator::VMinstanceOfEvaluator(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   bool aotStats= comp->getOption(TR_EnableAOTStats);

   if (aotStats)
      {
      ((TR_JitPrivateConfig *)fej9->getPrivateConfig())->aotStats->numVMInstanceOfEvaluator++;
      }

   // Evaluate the children of the instanceof node
   //
   TR::Node     *objectRef    = node->getFirstChild();
   TR::Node     *castClassRef = node->getSecondChild();
   TR::Register *targetReg;
   int32_t      classDepth   = -1;

   bool performEqualityTestInline   = false;
   bool performSuperclassTestInline = false;
   bool performHelperCall           = true;
   bool castClassIsFinal            = false;
   bool performReferenceArrayTestInline = false;
   bool castClassIs32Bit            = classAddressFitsIn32Bits(castClassRef, cg);
   bool profiledClassIs32Bit        = profiledAddressFitsIn32Bits(castClassRef, cg);
   bool use64BitClasses             = TR::Compiler->target.is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders();

   // See if inline tests are appropriate
   //
   TR::SymbolReference *castClassSymRef = castClassRef->getSymbolReference();
   TR::StaticSymbol    *castClassSym    = castClassSymRef ? castClassSymRef->getSymbol()->getStaticSymbol() : NULL;
   TR::MethodSymbol    *helperSym       = node->getSymbol()->castToMethodSymbol();

   if (!comp->getOption(TR_DisableInlineCheckCast))
      {
      performEqualityTestInline   = TR::TreeEvaluator::instanceOfOrCheckCastNeedEqualityTest(node, cg);
      performSuperclassTestInline = TR::TreeEvaluator::instanceOfOrCheckCastNeedSuperTest(node, cg);
      }

   castClassIsFinal           = castClassSymRef ? castClassSymRef->isNonArrayFinal(comp) : false;
   performHelperCall          = needHelperCall((performSuperclassTestInline || performEqualityTestInline), castClassIsFinal, cg);
   bool testCache             = needTestCache(castClassRef, true, performHelperCall, (performSuperclassTestInline || performEqualityTestInline), cg);

      // new logic only affect X86
   if (TR::TreeEvaluator::instanceOfOrCheckCastIsFinalArray(node, cg))
      {
      performEqualityTestInline = true;
      performSuperclassTestInline = false;
      performHelperCall = false;
      testCache = false;
      }
   else if (TR::TreeEvaluator::instanceOfOrCheckCastIsJavaLangObjectArray(node, cg)) // array of Object
      {
      performEqualityTestInline = false;
      performSuperclassTestInline = false;
      performHelperCall = false;
      performReferenceArrayTestInline = true;
      }

   // If we must call the helper, just do that without any inline tests
   //
   if (performHelperCall && !testCache)
      {
      if (aotStats)
         {
         ((TR_JitPrivateConfig *)fej9->getPrivateConfig())->aotStats->numInstanceOfVMHelperInstructions++;
         }

      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::icall);
      targetReg = TR::TreeEvaluator::performCall(node, false, false, cg);
      TR::Node::recreate(node, opCode);
      return targetReg;
      }

   bool rematerializeVMThreadInSnippet = false;
   uint32_t     numDeps        = 3;

   static const char *allowVMThreadRemat = feGetEnv("TR_allowVMThreadRemat");
   if (allowVMThreadRemat && cg->supportsFS0VMThreadRematerialization())
      {
      rematerializeVMThreadInSnippet = true;
      numDeps --;
      }

   if (!rematerializeVMThreadInSnippet)
      cg->setVMThreadRequired(true);

   TR::Register *objectReg      = cg->evaluate(objectRef);
   TR::Register *castClassReg   = NULL;
   TR::Register *objectClassReg = NULL;
   TR::Register *testerReg      = NULL;

   targetReg                   = cg->allocateRegister();

   TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *callHelper = generateLabelSymbol(cg);
   TR::LabelSymbol *superClassTestNext = NULL;

   generatePrefetchAfterHeaderAccess (node, objectReg, cg);

   if (castClassRef->getOpCodeValue() != TR::loadaddr ||
       castClassRef->getRegister() != NULL           ||
       castClassSym == NULL                          ||

       // AMD64 must evaluate the cast class into a register for 64-bit comparisons.
       //
       TR::Compiler->target.is64Bit())
      {
      castClassReg = cg->evaluate(castClassRef);
      numDeps++;
      }

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *fallThru   = generateLabelSymbol(cg);
   startLabel->setStartInternalControlFlow();
   fallThru->setEndInternalControlFlow();
   generateLabelInstruction(LABEL, node, startLabel, cg);
   TR_X86ScratchRegisterManager *srm = cg->generateScratchRegisterManager();

   TR::LabelSymbol *trueLabel  = NULL;
   TR::LabelSymbol *falseLabel = NULL;

   // First test for null object reference.
   //
   if (!objectRef->isNonNull())
      {
      genNullTest(node, objectReg, cg);
      falseLabel = generateLabelSymbol(cg);
      generateLabelInstruction(JE4, node, falseLabel, cg);
      }

   // If we're going to need the object class in a register, get it now
   //
   TR::MemoryReference *objectClassMR = generateX86MemoryReference(objectReg, TR::Compiler->om.offsetOfObjectVftField(), cg);
   if (performSuperclassTestInline || testCache || performReferenceArrayTestInline)
      {
      objectClassReg = cg->allocateRegister();
      generateRegMemInstruction(LRegMem(use64BitClasses), node, objectClassReg, objectClassMR, cg);
      TR::TreeEvaluator::generateVFTMaskInstruction(node, objectClassReg, cg);
      numDeps++;
      if (testCache && TR::Compiler->target.is64Bit())
         {
          if (!castClassIs32Bit || !profiledClassIs32Bit)
            {
            testerReg = cg->allocateRegister();
            numDeps++;
            }
         }
      }

   static const char *inlinedHelpers = feGetEnv("TR_InlinedHelperCallsInstanceOf");

   if (TR::Compiler->target.is32Bit())
      inlinedHelpers = NULL;

   if (testCache)
      {
      TR_ASSERT(!performEqualityTestInline && !performSuperclassTestInline, "Test cache should be generated only when we can't do other inlined tests");

      TR::LabelSymbol *callHelper = generateLabelSymbol(cg);
      TR::LabelSymbol *restartLabel = generateLabelSymbol(cg);

      if (!inlinedHelpers && !performReferenceArrayTestInline)
         {
         TR_OutlinedInstructions *outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::icall, targetReg, callHelper,
                                                                                                        restartLabel, rematerializeVMThreadInSnippet, cg);
         cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);
         cg->generateDebugCounter(
            outlinedHelperCall->getFirstInstruction(),
            TR::DebugCounter::debugCounterName(comp, "helperCalls/%s/(%s)/%d/%d", node->getOpCode().getName(), comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
            1, TR::DebugCounter::Cheap);
         }

      if (!trueLabel)
         trueLabel   = generateLabelSymbol(cg);
      if (!falseLabel)
         falseLabel  = generateLabelSymbol(cg);

      generateProfiledCacheTest (castClassRef,
                                 castClassSymRef,
                                 objectClassReg,
                                 (profiledClassIs32Bit) ? NULL : testerReg,
                                 falseLabel,
                                 trueLabel,
                                 false,
                                 cg);

      if (inlinedHelpers)
         {
         if (!performReferenceArrayTestInline)
         generateInstanceOfCacheTestInline (node,
                                            castClassRef,
                                            castClassSymRef,
                                            objectClassReg,
                                            targetReg, //targetReg is used as temp register in the function
                                            falseLabel,
                                            trueLabel,
                                            restartLabel,
                                            cg);
         }
      else
         {
         if (!performReferenceArrayTestInline)
         generateInstanceOfCacheTest (castClassRef,
                                      castClassSymRef,
                                      objectClassReg,
                                      targetReg, //targetReg is used as temp register in the function
                                      falseLabel,
                                      trueLabel,
                                      callHelper,
                                      restartLabel,
                                      cg);
         }
      if (!performReferenceArrayTestInline)
      generateLabelInstruction  (JE4, node, falseLabel, cg);
      }

   // Generate the inline test for equality of the object's class with the given
   // class object
   //
   if (performEqualityTestInline)
      {
      genEqualityInstanceOfTest(node, objectClassReg, objectClassMR, castClassReg, srm, cg, NULL);

      // If this is known to be a final class the above tests are all that are
      // needed.
      //
      if (!performSuperclassTestInline)
         {
         // Jump to the false label and fall through to the true label
         //
         if (!falseLabel)
            falseLabel = generateLabelSymbol(cg);
         generateLabelInstruction(JNE4, node, falseLabel, cg);
         }
      else
         {
         trueLabel = generateLabelSymbol(cg);
         generateLabelInstruction(JE4, node, trueLabel, cg);
         }
      }

   // Generate the inline superclass test
   //
   if (performSuperclassTestInline)
      {

      if (castClassSym)
         {
         TR_OpaqueClassBlock * clazz = (TR_OpaqueClassBlock *)castClassSym->getStaticAddress();
         classDepth = (int32_t)TR::Compiler->cls.classDepthOf(clazz);
         }

      if (!falseLabel)
         falseLabel = generateLabelSymbol(cg);

      genSuperClassInstanceOfTest(node, objectClassReg, targetReg, castClassReg, classDepth, falseLabel, trueLabel, NULL, srm, cg, NULL, fallThru,node->getSecondChild(),false, false);

      // Jump to the false label and fall through to the true label
      //
      generateLabelInstruction(JNE4, node, falseLabel, cg);
      }

   if (performReferenceArrayTestInline)
      {
      if (!trueLabel)
         trueLabel = generateLabelSymbol(cg);

      if (!falseLabel)
         falseLabel = generateLabelSymbol(cg);

      genIsReferenceArrayTest(node, objectClassReg, targetReg, falseLabel, trueLabel, cg);
      generateLabelInstruction(JNE4, node, falseLabel, cg);
      }

   // Generate the true/false setting
   //
   if (trueLabel)
      generateLabelInstruction(LABEL, node, trueLabel, cg);

   generateRegImmInstruction(MOV4RegImm4, node, targetReg, 1, cg);
   if (falseLabel)
      {
      generateLabelInstruction(JMP4, node, fallThru, cg);
      generateLabelInstruction(LABEL, node, falseLabel, cg);
      generateRegRegInstruction(XOR4RegReg, node, targetReg, targetReg, cg);
      }

   if (!rematerializeVMThreadInSnippet)
      cg->setVMThreadRequired(false);

   numDeps += srm->numAvailableRegisters();
   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)numDeps, cg);
   if (testCache && inlinedHelpers)
      deps->addPostCondition(objectReg, TR::RealRegister::esi, cg);
   else
      deps->addPostCondition(objectReg, TR::RealRegister::NoReg, cg);
   if (castClassReg)
      deps->addPostCondition(castClassReg, TR::RealRegister::NoReg, cg);
   if (objectClassReg)
      deps->addPostCondition(objectClassReg, TR::RealRegister::NoReg, cg);
   if (testerReg)
      deps->addPostCondition(objectClassReg, TR::RealRegister::NoReg, cg);
   if (!rematerializeVMThreadInSnippet)
      deps->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);
   deps->addPostCondition(targetReg, TR::RealRegister::eax, cg);

   srm->addScratchRegistersToDependencyList(deps);
   deps->stopAddingConditions();
   generateLabelInstruction(LABEL, node, fallThru, deps, cg);
   srm->stopUsingRegisters();

   cg->decReferenceCount(objectRef);
   cg->decReferenceCount(castClassRef);

   if (objectClassReg)
      cg->stopUsingRegister(objectClassReg);
   if (testerReg)
      cg->stopUsingRegister(testerReg);
   if (node->getRegister() != targetReg)
      node->setRegister(targetReg);

   return targetReg;
   }


#if defined(DEBUG)
extern "C"
{
   uint32_t totalMonEnters            = 0;
   uint32_t totalNestedMonEnters      = 0;
   uint32_t totalUncontendedMonEnters = 0;
}
#endif


static TR_OpaqueMethodBlock *
getMethodFromBCInfo(
      TR_ByteCodeInfo &bcInfo,
      TR::Compilation *comp)
   {
   TR_OpaqueMethodBlock *method = NULL;

   if (bcInfo.getCallerIndex() >= 0)
      method = (TR_OpaqueMethodBlock *)(comp->getInlinedCallSite(bcInfo.getCallerIndex())._vmMethodInfo);
   else
      method = (TR_OpaqueMethodBlock *)(comp->getCurrentMethod()->getPersistentIdentifier());

   return method;
   }

static bool comesFromClassLib(TR::Node *node, TR::Compilation *comp)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR_OpaqueMethodBlock *mb = getMethodFromBCInfo(node->getByteCodeInfo(), comp);
   char buf[512];
   const char *methodSig = fej9->sampleSignature(mb, buf, 512, comp->trMemory());
   if (methodSig &&
       (strncmp(methodSig, "java", 4)==0 ||
        strncmp(methodSig, "sun", 3) ==0))
      return true;
   return false;
   }

static TR::MemoryReference *getMemoryReference(TR::Register *objectClassReg, TR::Register *objectReg, int32_t lwOffset, TR::CodeGenerator *cg)
   {
   if (objectClassReg)
      return generateX86MemoryReference(objectReg, objectClassReg, 0, cg);
   else
      return generateX86MemoryReference(objectReg, lwOffset, cg);
   }

void J9::X86::TreeEvaluator::asyncGCMapCheckPatching(TR::Node *node, TR::CodeGenerator *cg, TR::LabelSymbol *snippetLabel)
   {
   TR::MemoryReference *SOMmr = generateX86MemoryReference(node->getFirstChild()->getFirstChild(), cg);
   TR::Compilation *comp = cg->comp();

   if (TR::Compiler->target.is64Bit())
      {
      //64 bit sequence
      //
      //Generate a call to the out-of-line patching sequence.
      //This sequence will conver the call back into an asynch message check cmp
      //
      TR::LabelSymbol *gcMapPatchingLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *outlinedStartLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *outlinedEndLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *asyncWithoutPatch = generateLabelSymbol(cg);

      //Start inline patching sequence
      //
      TR::Register *patchableAddrReg = cg->allocateRegister();
      TR::Register *patchValReg = cg->allocateRegister();
      TR::Register *tempReg = cg->allocateRegister();


      outlinedStartLabel->setStartInternalControlFlow();
      outlinedEndLabel->setEndInternalControlFlow();

      //generateLabelInstruction(CALLImm4, node, gcMapPatchingLabel, cg);
      generatePatchableCodeAlignmentInstruction(TR::X86PatchableCodeAlignmentInstruction::CALLImm4AtomicRegions, generateLabelInstruction(CALLImm4, node, gcMapPatchingLabel, cg), cg);

      TR_OutlinedInstructions *gcMapPatching = new (cg->trHeapMemory()) TR_OutlinedInstructions(gcMapPatchingLabel, cg);
      cg->getOutlinedInstructionsList().push_front(gcMapPatching);
      gcMapPatching->swapInstructionListsWithCompilation();

      generateLabelInstruction(NULL, LABEL, gcMapPatchingLabel, cg)->setNode(node);

      generateLabelInstruction(LABEL, node, outlinedStartLabel, cg);
      //Load the address that we are going to patch and clean up the stack
      //
      generateRegInstruction(POPReg, node, patchableAddrReg, cg);

      //check if there is already an async even pending
      //
      generateMemImmInstruction(CMP8MemImm4, node, SOMmr, -1, cg);
      generateLabelInstruction(JE4, node, asyncWithoutPatch, cg);

      //Signal the async event
      //
      static char *d = feGetEnv("TR_GCOnAsyncBREAK");
      if (d)
         generateInstruction(BADIA32Op, node, cg);

      generateMemImmInstruction(S8MemImm4, node, generateX86MemoryReference(cg->getVMThreadRegister(), offsetof(J9VMThread, stackOverflowMark), cg), -1, cg);
      generateRegImmInstruction(MOV8RegImm4, node, tempReg, 1 << comp->getPersistentInfo()->getGCMapCheckEventHandle(), cg);
      generateMemRegInstruction(LOR8MemReg, node, generateX86MemoryReference(cg->getVMThreadRegister(), offsetof(J9VMThread, asyncEventFlags),cg), tempReg, cg);

      //Populate the code we are going to patch in
      //
      //existing
      //000007ff`7d340578 e8f4170000      call    000007ff`7d341d71  <------
      //000007ff`7d34057d 0f84ee1e0000    je      000007ff`7d342471
      //*********
      //patching in
      //000007ff'7d34056f 48837d50ff      cmp qword ptr [rbp+0x50], 0xffffffffffffffff   <-----
      //000007ff`7d34057d 0f84ee1e0000    je      000007ff`7d342471

      //Load the original value
      //

      generateRegMemInstruction(L8RegMem, node, patchValReg, generateX86MemoryReference(patchableAddrReg, -5, cg), cg);
      generateRegImm64Instruction(MOV8RegImm64, node, tempReg, (uint64_t) 0x0, cg);
      generateRegRegInstruction(OR8RegReg, node, patchValReg, tempReg, cg);
      generateRegImm64Instruction(MOV8RegImm64, node, tempReg, (uint64_t) 0x0, cg);
      generateRegRegInstruction(AND8RegReg, node, patchValReg, tempReg , cg);

      TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions((uint8_t)0, 4, cg);
      deps->addPostCondition(patchableAddrReg, TR::RealRegister::NoReg, cg);
      deps->addPostCondition(patchValReg, TR::RealRegister::NoReg, cg);
      deps->addPostCondition(tempReg, TR::RealRegister::NoReg, cg);
      deps->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);
      deps->stopAddingConditions();

      generateMemRegInstruction(S8MemReg, node, generateX86MemoryReference(patchableAddrReg, -5, cg), patchValReg, deps, cg);
      generateLabelInstruction(LABEL, node, asyncWithoutPatch, cg);
      generateLabelInstruction(JMP4, node, snippetLabel, cg);

      cg->stopUsingRegister(patchableAddrReg);
      cg->stopUsingRegister(patchValReg);
      cg->stopUsingRegister(tempReg);
      generateLabelInstruction(LABEL, node, outlinedEndLabel, cg);
      gcMapPatching->swapInstructionListsWithCompilation();
      }
  else
      {
      //32 bit sequence
      //

      //Generate a call to the out-of-line patching sequence.
      //This sequence will conver the call back into an asynch message check cmp
      //
      TR::LabelSymbol *gcMapPatchingLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *outlinedStartLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *outlinedEndLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *asyncWithoutPatch = generateLabelSymbol(cg);

      //Start inline patching sequence
      //
      TR::Register *patchableAddrReg = cg->allocateRegister();
      TR::Register *lowPatchValReg = cg->allocateRegister();
      TR::Register *highPatchValReg = cg->allocateRegister();
      TR::Register *lowExistingValReg = cg->allocateRegister();
      TR::Register *highExistingValReg = cg->allocateRegister();

      outlinedStartLabel->setStartInternalControlFlow();
      outlinedEndLabel->setEndInternalControlFlow();

      //generateBoundaryAvoidanceInstruction(TR::X86BoundaryAvoidanceInstruction::CALLImm4AtomicRegions, 8, 8,generateLabelInstruction(CALLImm4, node, gcMapPatchingLabel, cg), cg);
      TR::Instruction *callInst =  generatePatchableCodeAlignmentInstruction(TR::X86PatchableCodeAlignmentInstruction::CALLImm4AtomicRegions, generateLabelInstruction(CALLImm4, node, gcMapPatchingLabel, cg), cg);
      TR::X86VFPSaveInstruction *vfpSaveInst = generateVFPSaveInstruction(callInst->getPrev(), cg);

      TR_OutlinedInstructions *gcMapPatching = new (cg->trHeapMemory()) TR_OutlinedInstructions(gcMapPatchingLabel, cg);
      cg->getOutlinedInstructionsList().push_front(gcMapPatching);
      gcMapPatching->swapInstructionListsWithCompilation();

      generateLabelInstruction(NULL, LABEL, gcMapPatchingLabel, cg)->setNode(node);

      generateLabelInstruction(LABEL, node, outlinedStartLabel, cg);
      //Load the address that we are going to patch and clean up the stack
      //
      generateRegInstruction(POPReg, node, patchableAddrReg, cg);


      //check if there is already an async even pending
      //
      generateMemImmInstruction(CMP4MemImm4, node, SOMmr, -1, cg);
      generateLabelInstruction(JE4, node, asyncWithoutPatch, cg);

      //Signal the async event
      //
      generateMemImmInstruction(S4MemImm4, node, generateX86MemoryReference(cg->getVMThreadRegister(), offsetof(J9VMThread, stackOverflowMark), cg), -1, cg);
      generateRegImmInstruction(MOV4RegImm4, node, lowPatchValReg, 1 << comp->getPersistentInfo()->getGCMapCheckEventHandle(), cg);
      generateMemRegInstruction(LOR4MemReg, node, generateX86MemoryReference(cg->getVMThreadRegister(), offsetof(J9VMThread, asyncEventFlags),cg), lowPatchValReg, cg);

      //Populate the registers we are going to use in the lock cmp xchg
      //

      static char *d = feGetEnv("TR_GCOnAsyncBREAK");
      if (d)
         generateInstruction(BADIA32Op, node, cg);

      //Populate the existing inline code
      //
      generateRegMemInstruction(L4RegMem, node, lowExistingValReg, generateX86MemoryReference(patchableAddrReg, -5, cg), cg);
      generateRegMemInstruction(L4RegMem, node, highExistingValReg, generateX86MemoryReference(patchableAddrReg, -1, cg), cg);

      //Populate the code we are going to patch in
      //837d28ff        cmp     dword ptr [ebp+28h],0FFFFFFFFh <--- patching in
      //90              nop
      //*******************
      //                call imm4                              <---- patching over
      //
      generateRegImmInstruction(MOV4RegImm4, node, lowPatchValReg, (uint32_t) 0x287d8390, cg);
      generateRegRegInstruction(MOV4RegReg, node, highPatchValReg, highExistingValReg, cg);
      generateRegImmInstruction(OR4RegImm4, node, highPatchValReg, (uint32_t) 0x000000ff, cg);

      TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions((uint8_t)0, 6, cg);

      deps->addPostCondition(patchableAddrReg, TR::RealRegister::edi, cg);
      deps->addPostCondition(lowPatchValReg, TR::RealRegister::ebx, cg);
      deps->addPostCondition(highPatchValReg, TR::RealRegister::ecx, cg);
      deps->addPostCondition(lowExistingValReg, TR::RealRegister::eax, cg);
      deps->addPostCondition(highExistingValReg, TR::RealRegister::edx, cg);
      deps->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);
      deps->stopAddingConditions();
      generateMemInstruction(LCMPXCHG8BMem, node, generateX86MemoryReference(patchableAddrReg, -5, cg), deps, cg);
      generateLabelInstruction(LABEL, node, asyncWithoutPatch, cg);
      generateVFPRestoreInstruction(generateLabelInstruction(JMP4, node, snippetLabel, cg),vfpSaveInst,cg);

      cg->stopUsingRegister(patchableAddrReg);
      cg->stopUsingRegister(lowPatchValReg);
      cg->stopUsingRegister(highPatchValReg);
      cg->stopUsingRegister(lowExistingValReg);
      cg->stopUsingRegister(highExistingValReg);
      generateLabelInstruction(LABEL, node, outlinedEndLabel, cg);
      gcMapPatching->swapInstructionListsWithCompilation();
     }
  }

void J9::X86::TreeEvaluator::inlineRecursiveMonitor(TR::Node          *node,
                                                               TR::CodeGenerator   *cg,
                                                               TR::LabelSymbol     *fallThruLabel,
                                                               TR::LabelSymbol     *jitMonitorEnterOrExitSnippetLabel,
                                                               TR::LabelSymbol     *inlineRecursiveSnippetLabel,
                                                               TR::Register        *objectReg,
                                                               int                  lwOffset,
                                                               TR::LabelSymbol     *snippetRestartLabel,
                                                               bool                 reservingLock)
   {
   //Code generated:
   // mov lockWordReg, [obj+lwOffset]
   // add lockWordReg, INC_DEC_VALUE/-INC_DEC_VALUE  ---> lock word with increased recursive count
   // mov lockWordMaskedReg, NON_INC_DEC_MASK
   // and lockWordMaskedReg, lockWordReg  ---> lock word masked out counter bits
   // cmp lockWordMaskedReg, ebp
   // jne jitMonitorEnterOrExitSnippetLabel
   // mov [obj+lwOffset], lockWordReg
   // jmp fallThruLabel

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::LabelSymbol *outlinedStartLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *outlinedEndLabel = generateLabelSymbol(cg);

   outlinedStartLabel->setStartInternalControlFlow();
   outlinedEndLabel->setEndInternalControlFlow();

   TR_OutlinedInstructions *recursiveMonitorEnter = new (cg->trHeapMemory()) TR_OutlinedInstructions(inlineRecursiveSnippetLabel, cg);
   cg->getOutlinedInstructionsList().push_front(recursiveMonitorEnter);
   recursiveMonitorEnter->swapInstructionListsWithCompilation();

   generateLabelInstruction(NULL, LABEL, inlineRecursiveSnippetLabel, cg)->setNode(node);
   generateLabelInstruction(LABEL, node, outlinedStartLabel, cg);
   TR::Register *lockWordReg = cg->allocateRegister();
   TR::Register *lockWordMaskedReg = cg->allocateRegister();
   TR::Register *vmThreadReg = cg->getVMThreadRegister();
   bool use64bitOp = TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord();
   bool isMonitorEnter = node->getSymbolReference() == cg->comp()->getSymRefTab()->findOrCreateMethodMonitorEntrySymbolRef(NULL)
                       ||  node->getSymbolReference() == cg->comp()->getSymRefTab()->findOrCreateMonitorEntrySymbolRef(NULL);

   generateRegMemInstruction(LRegMem(use64bitOp), node, lockWordReg, generateX86MemoryReference(objectReg, lwOffset, cg), cg);
   generateRegImmInstruction(ADDRegImm4(use64bitOp), node, lockWordReg, isMonitorEnter? INC_DEC_VALUE: -INC_DEC_VALUE, cg);
   if (reservingLock)
      generateRegImmInstruction(MOVRegImm4(use64bitOp), node, lockWordMaskedReg, NON_INC_DEC_MASK - RES_BIT, cg);
   else
      generateRegImmInstruction(MOVRegImm4(use64bitOp), node, lockWordMaskedReg, NON_INC_DEC_MASK, cg);
   generateRegRegInstruction(ANDRegReg(use64bitOp), node, lockWordMaskedReg, lockWordReg, cg);
   generateRegRegInstruction(CMPRegReg(use64bitOp), node, lockWordMaskedReg, vmThreadReg, cg);

   generateLabelInstruction(JNE4, node, jitMonitorEnterOrExitSnippetLabel, cg);
   generateMemRegInstruction(SMemReg(use64bitOp), node, generateX86MemoryReference(objectReg, lwOffset, cg), lockWordReg, cg);

   TR::RegisterDependencyConditions *restartDeps = generateRegisterDependencyConditions((uint8_t)0, 4, cg);
   restartDeps->addPostCondition(objectReg, TR::RealRegister::NoReg, cg);
   restartDeps->addPostCondition(vmThreadReg, TR::RealRegister::ebp, cg);
   restartDeps->addPostCondition(lockWordMaskedReg, TR::RealRegister::NoReg, cg);
   restartDeps->addPostCondition(lockWordReg, TR::RealRegister::NoReg, cg);
   restartDeps->stopAddingConditions();
   generateLabelInstruction(LABEL, node, snippetRestartLabel, restartDeps, cg);

   generateLabelInstruction(JMP4, node, fallThruLabel, cg);

   cg->stopUsingRegister(lockWordReg);
   cg->stopUsingRegister(lockWordMaskedReg);

   TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions((uint8_t)0, 1, cg);
   deps->addPostCondition(vmThreadReg, TR::RealRegister::ebp, cg);
   deps->stopAddingConditions();
   generateLabelInstruction(LABEL, node, outlinedEndLabel, deps, cg);
   recursiveMonitorEnter->swapInstructionListsWithCompilation();
   }

void J9::X86::TreeEvaluator::transactionalMemoryJITMonitorEntry(TR::Node           *node,
                                                               TR::CodeGenerator  *cg,
                                                               TR::LabelSymbol     *startLabel,
                                                               TR::LabelSymbol     *snippetLabel,
                                                               TR::LabelSymbol     *JITMonitorEnterSnippetLabel,
                                                               TR::Register       *objectReg,
                                                               int               lwOffset)

   {
      TR::LabelSymbol *txJITMonitorEntryLabel = snippetLabel;
      TR::LabelSymbol *outlinedStartLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *outlinedEndLabel = generateLabelSymbol(cg);

      outlinedStartLabel->setStartInternalControlFlow();
      outlinedEndLabel->setEndInternalControlFlow();

      TR_OutlinedInstructions *txJITMonitorEntry = new (cg->trHeapMemory()) TR_OutlinedInstructions(txJITMonitorEntryLabel, cg);
      cg->getOutlinedInstructionsList().push_front(txJITMonitorEntry);
      txJITMonitorEntry->swapInstructionListsWithCompilation();

      generateLabelInstruction(NULL, LABEL, txJITMonitorEntryLabel, cg)->setNode(node);
      TR::Register *counterReg = cg->allocateRegister();
      generateRegImmInstruction(MOV4RegImm4, node, counterReg, 1024, cg);
      TR::LabelSymbol *spinLabel = outlinedStartLabel;
      generateLabelInstruction(LABEL, node, spinLabel, cg);

      generateInstruction(PAUSE, node, cg);
      generateRegInstruction(DEC4Reg, node, counterReg, cg); // might need to consider 32bits later
      generateLabelInstruction(JE4, node, JITMonitorEnterSnippetLabel, cg);
      TR::MemoryReference *objLockRef = generateX86MemoryReference(objectReg, lwOffset, cg);
      generateMemImmInstruction(CMP4MemImm4, node, objLockRef, 0, cg);
      generateLabelInstruction(JNE4, node, spinLabel, cg);
      generateLabelInstruction(JMP4, node, startLabel, cg);

      TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions((uint8_t)0, 1, cg);
      deps->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);
      deps->stopAddingConditions();
      generateLabelInstruction(LABEL, node, outlinedEndLabel, cg);

      cg->stopUsingRegister(counterReg);
      txJITMonitorEntry->swapInstructionListsWithCompilation();
   }

TR::Register *
J9::X86::TreeEvaluator::VMmonentEvaluator(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   // If there is a NULLCHK above this node it will be expecting us to set
   // up the excepting instruction. If we are not going to inline an
   // appropriate excepting instruction we must make sure to reset the
   // excepting instruction since our children may have set it.
   //
   static const char *noInline = feGetEnv("TR_NoInlineMonitor");
   static int32_t monEntCount = 0;
   static const char *firstMonEnt = feGetEnv("TR_FirstMonEnt");
   static const char *doCmpFirst = feGetEnv("TR_AddCMPBeforeCMPXCHG");
   bool reservingLock = false;
   bool normalLockPreservingReservation = false;
   bool dummyMethodMonitor = false;
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::Compilation *comp = cg->comp();

   int lwOffset = fej9->getByteOffsetToLockword((TR_OpaqueClassBlock *) cg->getMonClass(node));
   if (comp->getOption(TR_MimicInterpreterFrameShape) ||
       (comp->getOption(TR_FullSpeedDebug) && node->isSyncMethodMonitor()) ||
       noInline ||
       comp->getOption(TR_DisableInlineMonEnt) ||
       (firstMonEnt && (*firstMonEnt-'0') > monEntCount++))
      {
      // Don't inline
      //
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::call);
      TR::TreeEvaluator::directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      cg->setImplicitExceptionPoint(NULL);
      return NULL;
      }

   if (lwOffset > 0 && comp->getOption(TR_ReservingLocks))
      {
      bool dummy=false;
      TR::TreeEvaluator::evaluateLockForReservation (node, &reservingLock, &normalLockPreservingReservation, cg);
      TR::TreeEvaluator::isPrimitiveMonitor (node, cg);

      if (node->isPrimitiveLockedRegion() && reservingLock)
         dummyMethodMonitor = TR::TreeEvaluator::isDummyMonitorEnter(node, cg);

      if (reservingLock && !node->isPrimitiveLockedRegion())
         dummyMethodMonitor = false;
      }

   TR::Node *objectRef = node->getFirstChild();

   static const char *disableInlineRecursiveEnv = feGetEnv("TR_DisableInlineRecursiveMonitor");
   bool inlineRecursive = disableInlineRecursiveEnv ? false : true;
   if (comp->getOption(TR_X86HLE) || lwOffset <= 0)
     inlineRecursive = false;

   // Evaluate the object reference
   //
   TR::Register *objectReg = cg->evaluate(objectRef);
   TR::Register *eaxReal   = cg->allocateRegister();
   TR::Register *scratchReg = NULL;
   uint32_t numDeps = 3; // objectReg, eax, ebp

   generatePrefetchAfterHeaderAccess (node, objectReg, cg);

   cg->setVMThreadRequired(true);
   cg->setImplicitExceptionPoint(NULL);

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *fallThru = generateLabelSymbol(cg);
   TR::LabelSymbol *snippetFallThru = inlineRecursive ? generateLabelSymbol(cg) : fallThru;

   startLabel->setStartInternalControlFlow();
   fallThru->setEndInternalControlFlow();
   generateLabelInstruction(LABEL, node, startLabel, cg);

   TR::Register *vmThreadReg = cg->getVMThreadRegister();

   #if defined(DEBUG)
   if (debug("monenterstats"))
      {
      generateMemInstruction(INC4Mem, node, generateX86MemoryReference((intptr_t)&totalMonEnters, cg), cg);
      }
   #endif

   TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *monitorLookupCacheLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *fallThruFromMonitorLookupCacheLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *exitLabel = NULL;

   TR_OutlinedInstructions *outlinedHelperCall;
   // In the reserving lock case below, we change the symref on the node... Here, we are going to store the original symref, so that we can restore our change.
   TR::SymbolReference *originalNodeSymRef = NULL;

   if (comp->getOption(TR_ReservingLocks))
      {
      // About to change the node's symref... store the original.
      originalNodeSymRef = node->getSymbolReference();

      if (reservingLock && node->isPrimitiveLockedRegion() && dummyMethodMonitor)
         {
         if (node->getSymbolReference() == cg->getSymRef(TR_methodMonitorEntry))
            node->setSymbolReference(comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_AMD64JitMethodMonitorExitReservedPrimitive, true, true, true));
         else
            node->setSymbolReference(comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_AMD64JitMonitorExitReservedPrimitive, true, true, true));

         exitLabel = generateLabelSymbol(cg);
         TR_OutlinedInstructions *outlinedExitHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::call, NULL, exitLabel, fallThru, cg);
         cg->getOutlinedInstructionsList().push_front(outlinedExitHelperCall);
         }

      TR_RuntimeHelper helper;
      bool success = TR::TreeEvaluator::monEntryExitHelper(true, node, reservingLock, normalLockPreservingReservation, helper, cg);
      if (success)
         node->setSymbolReference(comp->getSymRefTab()->findOrCreateRuntimeHelper(helper, true, true, true));
      }

   if (TR::Compiler->target.is64Bit() && fej9->getX86SupportsHLE() && comp->getOption(TR_X86HLE))
      {
      TR::LabelSymbol *JITMonitorEntrySnippetLabel = generateLabelSymbol(cg);
      TR::TreeEvaluator::transactionalMemoryJITMonitorEntry(node, cg, startLabel, snippetLabel, JITMonitorEntrySnippetLabel, objectReg, lwOffset);
      outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::call, NULL,
                                                            JITMonitorEntrySnippetLabel, (exitLabel) ? exitLabel : fallThru, cg);
      }
   else
      outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::call, NULL,
                                                         snippetLabel, (exitLabel) ? exitLabel : snippetFallThru, cg);

   cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);
   cg->generateDebugCounter(
          outlinedHelperCall->getFirstInstruction(),
          TR::DebugCounter::debugCounterName(comp, "helperCalls/%s/(%s)/%d/%d", node->getOpCode().getName(), comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
          1, TR::DebugCounter::Cheap);

   // Okay, and we've made it down here and we've successfully generated all outlined snippets, let's restore the node's symref.
   if (comp->getOption(TR_ReservingLocks))
      {
      node->setSymbolReference(originalNodeSymRef);
      }

   if (inlineRecursive)
      {
      TR::LabelSymbol *inlineRecursiveSnippetLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *jitMonitorEnterSnippetLabel = snippetLabel;
      snippetLabel = inlineRecursiveSnippetLabel;
      TR::TreeEvaluator::inlineRecursiveMonitor(node, cg, fallThru, jitMonitorEnterSnippetLabel, inlineRecursiveSnippetLabel, objectReg, lwOffset, snippetFallThru, reservingLock);
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
   TR_X86OpCodes op = BADIA32Op;

   if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
      {
      op = TR::Compiler->target.isSMP() ? LCMPXCHG8MemReg : CMPXCHG8MemReg;
      if (fej9->getX86SupportsHLE() && comp->getOption(TR_X86HLE))
         op = TR::Compiler->target.isSMP() ? XALCMPXCHG8MemReg : XACMPXCHG8MemReg;
      }
   else
      {
      op = TR::Compiler->target.isSMP() ? LCMPXCHG4MemReg : CMPXCHG4MemReg;
      if (fej9->getX86SupportsHLE() && comp->getOption(TR_X86HLE))
         op = TR::Compiler->target.isSMP() ? XALCMPXCHG4MemReg : XACMPXCHG4MemReg;
      }

   TR::Register *objectClassReg = NULL;
   TR::Register *lookupOffsetReg = NULL;

   if (lwOffset <= 0)
      {
      TR::MemoryReference *objectClassMR = generateX86MemoryReference(objectReg, TMP_OFFSETOF_J9OBJECT_CLAZZ, cg);
      objectClassReg = cg->allocateRegister();
      numDeps++;
      TR::X86RegMemInstruction *instr;
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
      instr = generateRegMemInstruction(L4RegMem, node, objectClassReg, objectClassMR, cg);
#else
      instr = generateRegMemInstruction(LRegMem(), node, objectClassReg, objectClassMR, cg);
#endif
      // This instruction may try to dereference a null memory address
      // add an implicit exception point for it.
      //
      cg->setImplicitExceptionPoint(instr);
      instr->setNeedsGCMap(0xFF00FFFF);

      TR::TreeEvaluator::generateVFTMaskInstruction(node, objectClassReg, cg);
      int32_t offsetOfLockOffset = offsetof(J9Class, lockOffset);
      generateRegMemInstruction(LRegMem(), node, objectClassReg, generateX86MemoryReference(objectClassReg, offsetOfLockOffset, cg), cg);
      generateRegImmInstruction(CMPRegImms(), node, objectClassReg, 0, cg);

      generateCommonLockNurseryCodes(
         node,
         cg,
         true, //true for VMmonentEvaluator, false for VMmonexitEvaluator
         monitorLookupCacheLabel,
         fallThruFromMonitorLookupCacheLabel,
         snippetLabel,
         numDeps,
         lwOffset,
         objectClassReg,
         lookupOffsetReg,
         vmThreadReg,
         objectReg);
      }

   if (comp->getOption(TR_ReservingLocks) && reservingLock)
      {

#if defined(TRACE_LOCK_RESERVATION)
      {
      TR::IA32ConstantDataSnippet *cds = cg->findOrCreate4ByteConstant(node, (int)node);
      TR::MemoryReference *tempMR = generateX86MemoryReference(cds, cg);

      TR::X86MemImmInstruction  * instr;
      if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
         {
         generateRegRegInstruction(XOR4RegReg, node, eaxReal, eaxReal, cg);  // Zero out eaxReal
         instr = generateRegMemInstruction(L4RegMem, node, eaxReal, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), cg);
         }
      else
         instr = generateRegMemInstruction(LRegMem(), node, eaxReal, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), cg);

      cg->setImplicitExceptionPoint(instr);
      instr->setNeedsGCMap(0xFF00FFFF);

      TR::SymbolReference *tempRef = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), TR_UInt32);
      TR::MemoryReference *tempMR1 = generateX86MemoryReference(tempRef, cg);

      generateMemRegInstruction(SMemReg(),node, tempMR, eaxReal, cg);
      generateMemRegInstruction(SMemReg(),node, tempMR1, eaxReal, cg);

      TR::IA32ConstantDataSnippet *cds1 = cg->findOrCreate4ByteConstant(node, (int)node+2);
      TR::MemoryReference *tempMR3 = generateX86MemoryReference(cds1, cg);
      TR::SymbolReference *tempRef2 = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), TR_UInt32);
      TR::MemoryReference *tempMR2 = generateX86MemoryReference(tempRef2, cg);

      generateMemRegInstruction(SMemReg(),node, tempMR3, objectReg, cg);
      generateMemRegInstruction(SMemReg(),node, tempMR2, objectReg, cg);

      scratchReg = cg->allocateRegister();
      numDeps++;
      TR::TreeEvaluator::generateValueTracingCode (node, vmThreadReg, scratchReg, objectReg, eaxReal, cg);
      }
#endif

      generateRegMemInstruction(LEARegMem(), node, eaxReal, generateX86MemoryReference(vmThreadReg, RES_BIT, cg), cg);

      TR::X86MemRegInstruction  * instr;
      if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
         {
         // Use CMP4RegMem instead of CMPRegMem(...).
         instr = generateMemRegInstruction(CMP4MemReg, node, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), eaxReal, cg);
         }
      else
         instr = generateMemRegInstruction(CMPMemReg(TR::Compiler->target.is64Bit()), node, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), eaxReal, cg);

      cg->setImplicitExceptionPoint(instr);
      instr->setNeedsGCMap(0xFF00FFFF);

      generateLabelInstruction(JNE4, node, snippetLabel, cg);

      if (!node->isPrimitiveLockedRegion())
         {
         if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
            {
            // Use ADD4memImms instead of ADDMemImms
            generateMemImmInstruction(ADD4MemImms, node, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), REC_BIT, cg);
            }
         else
            generateMemImmInstruction(ADDMemImms(), node, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), REC_BIT, cg);
         }
      }
   else
      {
      if (comp->getOption(TR_ReservingLocks) && normalLockPreservingReservation)
         {
         TR::X86MemImmInstruction  * instr;
         if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
            instr = generateMemImmInstruction(CMP4MemImms, node, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), 0, cg);
         else
            instr = generateMemImmInstruction(CMPMemImms(), node, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), 0, cg);
         cg->setImplicitExceptionPoint(instr);
         instr->setNeedsGCMap(0xFF00FFFF);

         generateLabelInstruction(JNE4, node, snippetLabel, cg);
         }

      generateRegRegInstruction(XOR4RegReg, node, eaxReal, eaxReal, cg);

      if (doCmpFirst &&
         !comesFromClassLib(node, comp))
         {
         TR::X86MemImmInstruction  * instr;
         if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
            instr = generateMemImmInstruction(CMP4MemImms, node, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), 0, cg);
         else
            instr = generateMemImmInstruction(CMPMemImms(), node, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), 0, cg);

         cg->setImplicitExceptionPoint(instr);
         instr->setNeedsGCMap(0xFF00FFFF);

         generateLabelInstruction(JNE4, node, snippetLabel, cg);
         }

      if (node->isReadMonitor())
         {
         lockedReg = cg->allocateRegister();
         if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
            generateRegRegInstruction(XOR4RegReg, node, lockedReg, lockedReg, cg);  //After lockedReg is allocated zero it out.
         generateRegImmInstruction(MOVRegImm4(), node, lockedReg, INC_DEC_VALUE, cg);
         ++numDeps;
         }
      else
         {
         #if defined(J9VM_OPT_REAL_TIME_LOCKING_SUPPORT)
            // need to get monitor from cache, if we can
            lockedReg = cg->allocateRegister();
            numDeps++;
            generateRegMemInstruction(LRegMem(), node, lockedReg,
                                      generateX86MemoryReference(vmThreadReg, fej9->thisThreadMonitorCacheOffset(), cg), cg);
            generateRegRegInstruction(TESTRegReg(), node, lockedReg, lockedReg, cg);
            generateLabelInstruction(JE4, node, snippetLabel, cg);

         #else
            // we want to write thread reg into lock word
            lockedReg = vmThreadReg;
         #endif
         }

      // try to swap into lock word
      TR::X86MemRegInstruction  *instr = generateMemRegInstruction(op, node, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), lockedReg, cg);
      cg->setImplicitExceptionPoint(instr);
      instr->setNeedsGCMap(0xFF00FFFF);

      generateLabelInstruction(JNE4, node, snippetLabel, cg);
      }

   // Create dependencies for the registers used.
   // The dependencies must be in the order:
   //      objectReg, eaxReal, vmThreadReg
   // since the snippet needs to find them to grab the real registers from them.
   //
   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)numDeps, cg);
   deps->addPostCondition(objectReg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(eaxReal, TR::RealRegister::eax, cg);
   deps->addPostCondition(vmThreadReg, TR::RealRegister::ebp, cg);

   if (scratchReg)
      deps->addPostCondition(scratchReg, TR::RealRegister::NoReg, cg);

#if !defined(J9VM_OPT_REAL_TIME_LOCKING_SUPPORT)
   if (node->isReadMonitor())
#endif
      {
      deps->addPostCondition(lockedReg, TR::RealRegister::NoReg, cg);
      }

   if (objectClassReg)
      deps->addPostCondition(objectClassReg, TR::RealRegister::NoReg, cg);

   if (lookupOffsetReg)
      deps->addPostCondition(lookupOffsetReg, TR::RealRegister::NoReg, cg);

   deps->stopAddingConditions();

   #if defined(J9VM_OPT_REAL_TIME_LOCKING_SUPPORT)
      // our lock is in the object, now need to advance to next monitor in cache
      generateRegMemInstruction(LRegMem(), node, lockedReg,
                                generateX86MemoryReference(lockedReg, fej9->getMonitorNextOffset(), cg), cg);
      generateMemRegInstruction(SMemReg(), node,
                                generateX86MemoryReference(vmThreadReg, fej9->thisThreadMonitorCacheOffset(), cg),
                                lockedReg, cg);
   #endif

   generateLabelInstruction(LABEL, node, fallThru, deps, cg);

#if defined(TRACE_LOCK_RESERVATION)
   {
   TR::IA32ConstantDataSnippet *cds = cg->findOrCreate4ByteConstant(node, (int)node+1);
   TR::MemoryReference *tempMR = generateX86MemoryReference(cds, cg);

   TR::X86RegMemInstruction  *instr;
   if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
      instr = generateRegMemInstruction(L4RegMem, node, eaxReal, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), cg)
   else
      instr = generateRegMemInstruction(LRegMem(), node, eaxReal, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), cg);

   cg->setImplicitExceptionPoint(instr);
   instr->setNeedsGCMap(0xFF00FFFF);

   TR::SymbolReference *tempRef = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), TR_UInt32);
   TR::MemoryReference *tempMR1 = generateX86MemoryReference(tempRef, cg);

   generateMemRegInstruction(SMemReg(),node, tempMR, eaxReal, cg);
   generateMemRegInstruction(SMemReg(),node, tempMR1, eaxReal, cg);

   TR::IA32ConstantDataSnippet *cds1 = cg->findOrCreate4ByteConstant(node, (int)node+2);
   TR::MemoryReference *tempMR3 = generateX86MemoryReference(cds1, cg);
   TR::SymbolReference *tempRef2 = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), TR_UInt32);
   TR::MemoryReference *tempMR2 = generateX86MemoryReference(tempRef2, cg);

   generateMemRegInstruction(SMemReg(),node, tempMR3, objectReg, cg);
   generateMemRegInstruction(SMemReg(),node, tempMR2, objectReg, cg);
   }
#endif

   cg->setVMThreadRequired(false);

   cg->decReferenceCount(objectRef);
   cg->stopUsingRegister(eaxReal);
   if (scratchReg)
      cg->stopUsingRegister(scratchReg);
   if (objectClassReg)
      cg->stopUsingRegister(objectClassReg);
   if (lookupOffsetReg)
      cg->stopUsingRegister(lookupOffsetReg);

   if (node->isReadMonitor())
      {
      cg->stopUsingRegister(lockedReg);
      }

   #if defined(J9VM_OPT_REAL_TIME_LOCKING_SUPPORT)
   cg->stopUsingRegister(lockedReg);
   #endif

   return NULL;
   }


void J9::X86::TreeEvaluator::generateValueTracingCode(
   TR::Node          *node,
   TR::Register      *vmThreadReg,
   TR::Register      *scratchReg,
   TR::Register      *valueReg,
   TR::CodeGenerator *cg)
   {
   if (!TR::Options::getCmdLineOptions()->getOption(TR_EnableValueTracing))
      return;
   // the code requires that the caller has vmThread in EBP as well as
   // that the caller has already setup internal control flow
   uint32_t vmThreadBase    = offsetof(J9VMThread, debugEventData6);
   uint32_t vmThreadTop     = offsetof(J9VMThread, debugEventData4);
   uint32_t vmThreadCursor  = offsetof(J9VMThread, debugEventData5);
   TR::LabelSymbol *endLabel = generateLabelSymbol(cg);

   generateRegMemInstruction(LRegMem(), node, scratchReg, generateX86MemoryReference(vmThreadReg, vmThreadCursor, cg), cg);
   generateRegImmInstruction(ADDRegImms(), node, scratchReg, 8, cg);

   generateMemRegInstruction(CMPMemReg(), node, generateX86MemoryReference(vmThreadReg, vmThreadTop, cg), scratchReg, cg);
   generateLabelInstruction(JG4, node, endLabel, cg);
   generateRegMemInstruction(LRegMem(), node, scratchReg, generateX86MemoryReference(vmThreadReg, vmThreadBase, cg), cg);
   generateLabelInstruction(LABEL, node, endLabel, cg);
   generateMemImmInstruction(SMemImm4(), node, generateX86MemoryReference(scratchReg, 0, cg), node->getOpCodeValue(), cg);
   generateMemRegInstruction(SMemReg(), node, generateX86MemoryReference(scratchReg, 0, cg), valueReg, cg);
   generateMemRegInstruction(SMemReg(), node, generateX86MemoryReference(vmThreadReg, vmThreadCursor, cg), scratchReg, cg);
   }

void J9::X86::TreeEvaluator::generateValueTracingCode(
   TR::Node          *node,
   TR::Register      *vmThreadReg,
   TR::Register      *scratchReg,
   TR::Register      *valueRegHigh,
   TR::Register      *valueRegLow,
   TR::CodeGenerator *cg)
   {
   if (!TR::Options::getCmdLineOptions()->getOption(TR_EnableValueTracing))
      return;

   // the code requires that the caller has vmThread in EBP as well as
   // that the caller has already setup internal control flow
   uint32_t vmThreadBase    = offsetof(J9VMThread, debugEventData6);
   uint32_t vmThreadTop     = offsetof(J9VMThread, debugEventData4);
   uint32_t vmThreadCursor  = offsetof(J9VMThread, debugEventData5);
   TR::LabelSymbol *endLabel = generateLabelSymbol(cg);

   generateRegMemInstruction(LRegMem(), node, scratchReg, generateX86MemoryReference(vmThreadReg, vmThreadCursor, cg), cg);
   generateRegImmInstruction(ADDRegImms(), node, scratchReg, 0x10, cg);

   generateMemRegInstruction(CMPMemReg(), node, generateX86MemoryReference(vmThreadReg, vmThreadTop, cg), scratchReg, cg);
   generateLabelInstruction(JG4, node, endLabel, cg);
   generateRegMemInstruction(LRegMem(), node, scratchReg, generateX86MemoryReference(vmThreadReg, vmThreadBase, cg), cg);
   generateLabelInstruction(LABEL, node, endLabel, cg);
   generateMemImmInstruction(SMemImm4(), node, generateX86MemoryReference(scratchReg,  0, cg), node->getOpCodeValue(), cg);
   generateMemRegInstruction(SMemReg(), node, generateX86MemoryReference(scratchReg,   4, cg), valueRegHigh, cg);
   generateMemRegInstruction(SMemReg(), node, generateX86MemoryReference(scratchReg,   8, cg), valueRegLow, cg);
   generateRegMemInstruction(LRegMem(), node, valueRegLow, generateX86MemoryReference(valueRegHigh, 0, cg), cg);
   generateMemRegInstruction(SMemReg(), node, generateX86MemoryReference(scratchReg, 0xc, cg), valueRegLow, cg);
   generateMemRegInstruction(SMemReg(), node, generateX86MemoryReference(vmThreadReg, vmThreadCursor, cg), scratchReg, cg);
   }

TR::Register *J9::X86::TreeEvaluator::VMmonexitEvaluator(TR::Node          *node,
                                                     TR::CodeGenerator *cg)
   {
   // If there is a NULLCHK above this node it will be expecting us to set
   // up the excepting instruction.  If we are not going to inline an
   // appropriate excepting instruction we must make sure to reset the
   // excepting instruction since our children may have set it.
   //
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   static const char *noInline = feGetEnv("TR_NoInlineMonitor");
   static int32_t monExitCount = 0;
   static const char *firstMonExit = feGetEnv("TR_FirstMonExit");
   bool reservingLock = false;
   bool normalLockPreservingReservation = false;
   bool dummyMethodMonitor = false;
   bool gen64BitInstr = TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord();

   int lwOffset = fej9->getByteOffsetToLockword((TR_OpaqueClassBlock *) cg->getMonClass(node));
   if ((comp->getOption(TR_MimicInterpreterFrameShape) /*&& !comp->getOption(TR_EnableLiveMonitorMetadata)*/) ||
       noInline ||
       comp->getOption(TR_DisableInlineMonExit) ||
       (firstMonExit && (*firstMonExit-'0') > monExitCount++))
      {
      // Don't inline
      //
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::call);
      TR::TreeEvaluator::directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      cg->setImplicitExceptionPoint(NULL);
      return NULL;
      }

   if (lwOffset > 0 && comp->getOption(TR_ReservingLocks))
      {
      bool dummy=false;
      TR::TreeEvaluator::evaluateLockForReservation (node, &reservingLock, &normalLockPreservingReservation, cg);
      if (node->isPrimitiveLockedRegion() && reservingLock)
         dummyMethodMonitor = TR::TreeEvaluator::isDummyMonitorExit(node, cg);

      if (!node->isPrimitiveLockedRegion() && reservingLock)
         dummyMethodMonitor = false;
      }

   if (dummyMethodMonitor)
      {
      cg->decReferenceCount(node->getFirstChild());
      return NULL;
      }

   static const char *disableInlineRecursiveEnv = feGetEnv("TR_DisableInlineRecursiveMonitor");
   bool inlineRecursive = disableInlineRecursiveEnv ? false : true;
   if (comp->getOption(TR_X86HLE) || lwOffset <= 0)
     inlineRecursive = false;

   // Evaluate the object reference
   //
   TR::Node     *objectRef = node->getFirstChild();
   TR::Register *objectReg = cg->evaluate(objectRef);
   TR::Register *tempReg   = NULL;
   uint32_t     numDeps   = 2; // objectReg, ebp

   cg->setVMThreadRequired(true);
   cg->setImplicitExceptionPoint(NULL);
   TR::Register *vmThreadReg = cg->getVMThreadRegister();

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *fallThru   = generateLabelSymbol(cg);

#if !defined(J9VM_OPT_REAL_TIME_LOCKING_SUPPORT)
   // Now that the object reference has been generated, see if this is the end
   // of a small synchronized block.
   // The definition of "small" depends on the method hotness and is measured
   // in instructions.
   // The following method makes use of the fact that the body of the sync
   // block has been generated but the monitor exit hasn't yet.
   //
   int32_t maxInstructions;
   TR_Hotness hotness = comp->getMethodHotness();
   if (hotness == scorching) maxInstructions = 30;
   else if (hotness == hot)  maxInstructions = 20;
   else                      maxInstructions = 10;
#endif

   startLabel->setStartInternalControlFlow();
   TR::LabelSymbol *snippetFallThru = inlineRecursive ? generateLabelSymbol(cg): fallThru;
   fallThru->setEndInternalControlFlow();
   generateLabelInstruction(LABEL, node, startLabel, cg);

   TR::Register *eaxReal     = 0;
   TR::Register *unlockedReg = 0;
   TR::Register *scratchReg  = 0;

   TR::Register *objectClassReg = NULL;
   TR::Register *lookupOffsetReg = NULL;

   if (lwOffset <= 0)
      {
      TR::MemoryReference *objectClassMR = generateX86MemoryReference(objectReg, TMP_OFFSETOF_J9OBJECT_CLAZZ, cg);
      objectClassReg = cg->allocateRegister();
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
      TR::Instruction *instr = generateRegMemInstruction(L4RegMem, node, objectClassReg, objectClassMR, cg);
#else
      TR::Instruction *instr = generateRegMemInstruction(LRegMem(), node, objectClassReg, objectClassMR, cg);
#endif
      //this instruction may try to dereference a null memory address
      //add an implicit exception point for it.
      cg->setImplicitExceptionPoint(instr);
      instr->setNeedsGCMap(0xFF00FFFF);

      TR::TreeEvaluator::generateVFTMaskInstruction(node, objectClassReg, cg);
      int32_t offsetOfLockOffset = offsetof(J9Class, lockOffset);
      generateRegMemInstruction(LRegMem(), node, objectClassReg, generateX86MemoryReference(objectClassReg, offsetOfLockOffset, cg), cg);

      numDeps++;
      }

   TR::LabelSymbol *monitorLookupCacheLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *fallThruFromMonitorLookupCacheLabel = generateLabelSymbol(cg);

#if defined(J9VM_OPT_REAL_TIME_LOCKING_SUPPORT)
   TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *decCountLabel = generateLabelSymbol(cg);

   unlockedReg = cg->allocateRegister();
   tempReg     = cg->allocateRegister();
   eaxReal     = cg->allocateRegister();

   numDeps += 3;

   if (lwOffset <= 0)
      {
      generateRegImmInstruction(CMPRegImms(), node, objectClassReg, 0, cg);

      generateCommonLockNurseryCodes(node,
                               cg,
                               false, //true for VMmonentEvaluator, false for VMmonexitEvaluator
                               monitorLookupCacheLabel,
                               fallThruFromMonitorLookupCacheLabel,
                               snippetLabel,
                               numDeps,
                               lwOffset,
                               objectClassReg,
                               lookupOffsetReg,
                               vmThreadReg,
                               objectReg);
      }


   // load lock word
   generateRegMemInstruction(LRegMem(), node, tempReg, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), cg);

   // extract monitor from lock word
   generateRegRegInstruction(MOVRegReg(), node, unlockedReg, tempReg, cg);

   #define LOCK_PINNED_BIT (0x1)
   generateRegImmInstruction(ANDRegImms(), node, unlockedReg, ~((UDATA) LOCK_PINNED_BIT), cg);

   // need a NULL test to snippet: about to dereference lock word
   generateRegRegInstruction(TESTRegReg(), node, unlockedReg, unlockedReg, cg);
   generateLabelInstruction(JE4, node, snippetLabel, cg);

   // if OS monitors don't match, let snippet handle it
   generateRegMemInstruction(LRegMem(), node, eaxReal,
                             generateX86MemoryReference(unlockedReg, fej9->getMonitorOwnerOffset(), cg), cg);
   generateRegMemInstruction(CMPRegMem(), node, eaxReal,
                             generateX86MemoryReference(vmThreadReg, fej9->thisThreadOSThreadOffset(), cg), cg);
   generateLabelInstruction(JNE4, node, snippetLabel, cg);

   // monitors match so we can unlock it
   // decrement count, maybe unlock object
   generateRegMemInstruction(LRegMem(), node, eaxReal,
                             generateX86MemoryReference(unlockedReg, fej9->getMonitorEntryCountOffset(), cg), cg);
   generateRegImmInstruction(CMPRegImms(), node, eaxReal, 1, cg);
   generateLabelInstruction(JA4, node, decCountLabel, cg);


   // leaving main-line code path
      // create the outlined path that decrements the count
      TR_OutlinedInstructions *decCountPath = new (cg->trHeapMemory()) TR_OutlinedInstructions(decCountLabel, cg);
      cg->getOutlinedInstructionsList().push_front(decCountPath);
      decCountPath->swapInstructionListsWithCompilation();
         generateLabelInstruction(NULL, LABEL,      decCountLabel, cg)->setNode(node);
         generateMemInstruction(        DECMem(cg), node, generateX86MemoryReference(unlockedReg, fej9->getMonitorEntryCountOffset(), cg), cg);
         generateLabelInstruction(      JMP4,       node, fallThru, cg);
      decCountPath->swapInstructionListsWithCompilation();

   // back to main-line code path

   // unlock object...but only if lock pinned bit is clear
   generateRegRegInstruction(CMPRegReg(), node, eaxReal, unlockedReg, cg);
   generateLabelInstruction(JNE4, node, snippetLabel, cg);


   TR_X86OpCodes op = TR::Compiler->target.isSMP() ? LCMPXCHGMemReg(gen64BitInstr) : CMPXCHGMemReg(gen64BitInstr);

   // compare-and-swap to unlock:
   generateRegRegInstruction(XORRegReg(), node, eaxReal, eaxReal, cg);
   cg->setImplicitExceptionPoint(generateMemRegInstruction(op, node, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), eaxReal, cg));

   generateRegRegInstruction(CMPRegReg(), node, eaxReal, unlockedReg, cg);
   generateLabelInstruction(JNE4, node, snippetLabel, cg);

   // unlocked the object, just need to put monitor back in thread cache
   generateRegMemInstruction(LRegMem(), node, eaxReal,
                             generateX86MemoryReference(vmThreadReg, fej9->thisThreadMonitorCacheOffset(), cg), cg);
   generateMemRegInstruction(SMemReg(), node,
                             generateX86MemoryReference(unlockedReg, fej9->getMonitorNextOffset(), cg), eaxReal, cg);
   generateMemRegInstruction(SMemReg(), node,
                             generateX86MemoryReference(vmThreadReg, fej9->thisThreadMonitorCacheOffset(), cg),
                             unlockedReg, cg);

   TR_OutlinedInstructions *outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::call, NULL, snippetLabel, fallThru, cg);
   cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);
   cg->generateDebugCounter(
      outlinedHelperCall->getFirstInstruction(),
      TR::DebugCounter::debugCounterName(comp, "helperCalls/%s/(%s)/%d/%d", node->getOpCode().getName(), comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
      1, TR::DebugCounter::Cheap);

#else

   // Create the monitor exit snippet
   //
   TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);

   if (lwOffset <= 0)
      {
      generateRegImmInstruction(CMP4RegImm4, node, objectClassReg, 0, cg);

      generateCommonLockNurseryCodes(node,
                               cg,
                               false, //true for VMmonentEvaluator, false for VMmonexitEvaluator
                               monitorLookupCacheLabel,
                               fallThruFromMonitorLookupCacheLabel,
                               snippetLabel,
                               numDeps,
                               lwOffset,
                               objectClassReg,
                               lookupOffsetReg,
                               vmThreadReg,
                               objectReg);
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
   if (comp->getOption(TR_ReservingLocks))
      {
      if (reservingLock)
         {
         tempReg = cg->allocateRegister();
         numDeps++;
         }
      }

   if (comp->getOption(TR_ReservingLocks))
      {
      if (reservingLock || normalLockPreservingReservation)
         {
         TR_RuntimeHelper helper;
         bool success = TR::TreeEvaluator::monEntryExitHelper(false, node, reservingLock, normalLockPreservingReservation, helper, cg);

         TR_ASSERT(success == true, "monEntryExitHelper: could not find runtime helper");

         node->setSymbolReference(comp->getSymRefTab()->findOrCreateRuntimeHelper(helper, true, true, true));
         }
      }
   TR_OutlinedInstructions *outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::call, NULL, snippetLabel, snippetFallThru, cg);
   cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);

   if (inlineRecursive)
      {
      TR::LabelSymbol *inlineRecursiveSnippetLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *jitMonitorExitSnippetLabel = snippetLabel;
      snippetLabel = inlineRecursiveSnippetLabel;
      TR::TreeEvaluator::inlineRecursiveMonitor(node, cg, fallThru, jitMonitorExitSnippetLabel, inlineRecursiveSnippetLabel, objectReg, lwOffset, snippetFallThru, reservingLock);
      }

   bool reservingDecrementNeeded = false;

   if (node->isReadMonitor())
      {
      unlockedReg = cg->allocateRegister();
      eaxReal     = cg->allocateRegister();
      generateRegRegInstruction(XORRegReg(), node, unlockedReg, unlockedReg, cg);
      generateRegImmInstruction(MOVRegImm4(), node, eaxReal, INC_DEC_VALUE, cg);

      TR_X86OpCodes op = TR::Compiler->target.isSMP() ? LCMPXCHGMemReg(gen64BitInstr) : CMPXCHGMemReg(gen64BitInstr);
      cg->setImplicitExceptionPoint(generateMemRegInstruction(op, node, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), unlockedReg, cg));
      numDeps += 2;
      }
   else
      {
      if (reservingLock)
         {
#if defined(TRACE_LOCK_RESERVATION)
         TR::IA32ConstantDataSnippet *cds = cg->findOrCreate4ByteConstant(node, (int)node);
         TR::MemoryReference *tempMR = generateX86MemoryReference(cds, cg);

         if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
            {
            generateRegRegInstruction(XORRegReg(), node, tempReg, tempReg, cg);  // Zero out tempReg before LRegMem op.
            }
         cg->setImplicitExceptionPoint(generateRegMemInstruction(
               LRegMem(gen64BitInstr), node, tempReg,
                                                                    getMemoryReference(objectClassReg, objectReg, lwOffset, cg), cg));

         TR::SymbolReference *tempRef = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), TR_UInt32);
         TR::MemoryReference *tempMR1 = generateX86MemoryReference(tempRef, cg);

         generateMemRegInstruction(SMemReg(),node, tempMR, tempReg, cg);
         generateMemRegInstruction(SMemReg(),node, tempMR1, tempReg, cg);

         TR::IA32ConstantDataSnippet *cds1 = cg->findOrCreate4ByteConstant(node, (int)node+2);
         TR::MemoryReference *tempMR3 = generateX86MemoryReference(cds1, cg);
         TR::SymbolReference *tempRef2 = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), TR_UInt32);
         TR::MemoryReference *tempMR2 = generateX86MemoryReference(tempRef2, cg);

         generateMemRegInstruction(SMemReg(),node, tempMR3, objectReg, cg);
         generateMemRegInstruction(SMemReg(),node, tempMR2, objectReg, cg);

         scratchReg = cg->allocateRegister();
         numDeps++;

         TR::LabelSymbol *doneTestLabel = generateLabelSymbol(cg);

         //generateLabelInstruction(LABEL, node, doneTestLabel, cg);
         //generateImmSymInstruction(PUSHImm4, node, (uintptrj_t)doneTestLabel->getStaticSymbol()->getStaticAddress(), node->getSymbolReference(), cg);
         //generateRegInstruction(POPReg, node, scratchReg, cg);

	 TR::TreeEvaluator::generateValueTracingCode (node, vmThreadReg, scratchReg, objectReg, tempReg, cg);

         // cause crash in some cases
         if (0)
            {
            generateRegImmInstruction(TEST1RegImm1, node, tempReg, 0xA, cg);
            generateLabelInstruction(JNE4, node, doneTestLabel, cg);
            generateRegRegInstruction(XOR4RegReg, node, scratchReg, scratchReg, cg);
            generateRegMemInstruction(LRegMem(), node,
                                                   scratchReg,
                                                   generateX86MemoryReference(scratchReg, 0, cg), cg);
            generateLabelInstruction(LABEL, node, doneTestLabel, cg);
            }
#endif
         if (node->isPrimitiveLockedRegion())
            {
            cg->setImplicitExceptionPoint(generateRegMemInstruction(
                  LRegMem(gen64BitInstr), node, tempReg,
                                                                       getMemoryReference(objectClassReg, objectReg, lwOffset, cg), cg));
            // Mask out the thread ID and reservation count
            generateRegImmInstruction(ANDRegImms(), node, tempReg, FLAGS_MASK, cg);
            // If only the RES flag is set and no other we can continue
            generateRegImmInstruction(XORRegImms(), node, tempReg, RES_BIT, cg);
            }
         else
            {
            reservingDecrementNeeded = true;
            generateRegMemInstruction(LEARegMem(), node, tempReg, generateX86MemoryReference(vmThreadReg, (REC_BIT | RES_BIT), cg), cg);
            cg->setImplicitExceptionPoint(generateMemRegInstruction(
                  CMPMemReg(gen64BitInstr), node,
                  getMemoryReference(objectClassReg, objectReg, lwOffset, cg), tempReg, cg));
            }
         }
      else
         {
         cg->setImplicitExceptionPoint(generateRegMemInstruction(
               CMPRegMem(gen64BitInstr), node, vmThreadReg,
                                                                   getMemoryReference(objectClassReg, objectReg, lwOffset, cg), cg));
         }
      }

   generateLabelInstruction(JNE4, node, snippetLabel, cg);

   if (reservingDecrementNeeded)
      {
      // Subtract the reservation count
      generateMemImmInstruction(SUBMemImms(gen64BitInstr), node,
         getMemoryReference(objectClassReg, objectReg, lwOffset, cg), REC_BIT, cg);  // I'm not sure SUB4MemImms will work.
      }

   if (!node->isReadMonitor() && !reservingLock)
      {
#ifdef J9VM_TASUKI_LOCKS_DOUBLE_SLOT
      TR_ASSERT(TR::Compiler->target.is32Bit(), "This code-piece should never be reached");
#endif
      if (fej9->getX86SupportsHLE() && comp->getOption(TR_X86HLE))
         generateMemImmInstruction(XRSMemImm4(gen64BitInstr),
            node, getMemoryReference(objectClassReg, objectReg, lwOffset, cg), 0, cg);
      else
         generateMemImmInstruction(SMemImm4(gen64BitInstr), node,
            getMemoryReference(objectClassReg, objectReg, lwOffset, cg), 0, cg);
      }

#endif // J9VM_OPT_REAL_TIME_LOCKING_SUPPORT


   // Create dependencies for the registers used.
   // The first dependencies must be objectReg, vmThreadReg, tempReg
   // Or, for readmonitors they must be objectReg, vmThreadReg, unlockedReg, eaxReal
   // snippet needs to find them to grab the real registers from them.
   //
   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)numDeps, cg);
   deps->addPostCondition(objectReg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);

#if !defined(J9VM_OPT_REAL_TIME_LOCKING_SUPPORT)
   if (node->isReadMonitor())
#endif
      {
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
   generateLabelInstruction(LABEL, node, fallThru, deps, cg);

#if defined(TRACE_LOCK_RESERVATION)
   if (reservingLock)
      {
      TR::IA32ConstantDataSnippet *cds = cg->findOrCreate4ByteConstant(node, (int)node+1);
      TR::MemoryReference *tempMR = generateX86MemoryReference(cds, cg);

      cg->setImplicitExceptionPoint(generateRegMemInstruction(
            LRegMem(gen64BitInstr), node, tempReg,
                                                                 getMemoryReference(objectClassReg, objectReg, lwOffset, cg), cg));

      TR::SymbolReference *tempRef = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), TR_UInt32);
      TR::MemoryReference *tempMR1 = generateX86MemoryReference(tempRef, cg);

      generateMemRegInstruction(SMemReg(),node, tempMR, tempReg, cg);
      generateMemRegInstruction(SMemReg(),node, tempMR1, tempReg, cg);

      TR::IA32ConstantDataSnippet *cds1 = cg->findOrCreate4ByteConstant(node, (int)node+2);
      TR::MemoryReference *tempMR3 = generateX86MemoryReference(cds1, cg);
      TR::SymbolReference *tempRef2 = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), TR_UInt32);
      TR::MemoryReference *tempMR2 = generateX86MemoryReference(tempRef2, cg);

      generateMemRegInstruction(SMemReg(),node, tempMR3, objectReg, cg);
      generateMemRegInstruction(SMemReg(),node, tempMR2, objectReg, cg);
      }
#endif

   cg->setVMThreadRequired(false);

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


bool J9::X86::TreeEvaluator::monEntryExitHelper(
      bool entry,
      TR::Node* node,
      bool reservingLock,
      bool normalLockPreservingReservation,
      TR_RuntimeHelper &helper,
      TR::CodeGenerator* cg)
   {
   bool methodMonitor = entry
      ? (node->getSymbolReference() == cg->getSymRef(TR_methodMonitorEntry))
      : (node->getSymbolReference() == cg->getSymRef(TR_methodMonitorExit));

   if (reservingLock)
      {
      if (node->isPrimitiveLockedRegion())
         {
         static TR_RuntimeHelper helpersCase1[2][2][2] =
            {
               {
                  {TR_IA32JitMonitorExitReservedPrimitive, TR_IA32JitMethodMonitorExitReservedPrimitive},
                  {TR_AMD64JitMonitorExitReservedPrimitive, TR_AMD64JitMethodMonitorExitReservedPrimitive}
               },
               {
                  {TR_IA32JitMonitorEnterReservedPrimitive, TR_IA32JitMethodMonitorEnterReservedPrimitive},
                  {TR_AMD64JitMonitorEnterReservedPrimitive, TR_AMD64JitMethodMonitorEnterReservedPrimitive}
               }
            };

         helper = helpersCase1[entry?1:0][TR::Compiler->target.is64Bit()?1:0][methodMonitor?1:0];
         return true;
         }
      else
         {
         static TR_RuntimeHelper helpersCase2[2][2][2] =
            {
               {
                  {TR_IA32JitMonitorExitReserved, TR_IA32JitMethodMonitorExitReserved},
                  {TR_AMD64JitMonitorExitReserved, TR_AMD64JitMethodMonitorExitReserved}
               },
               {
                  {TR_IA32JitMonitorEnterReserved, TR_IA32JitMethodMonitorEnterReserved},
                  {TR_AMD64JitMonitorEnterReserved, TR_AMD64JitMethodMonitorEnterReserved}
               }
            };

         helper = helpersCase2[entry?1:0][TR::Compiler->target.is64Bit()?1:0][methodMonitor?1:0];
         return true;
         }
      }
   else if (normalLockPreservingReservation)
      {
      static TR_RuntimeHelper helpersCase2[2][2][2] =
         {
            {
               {TR_IA32JitMonitorExitPreservingReservation, TR_IA32JitMethodMonitorExitPreservingReservation},
               {TR_AMD64JitMonitorExitPreservingReservation, TR_AMD64JitMethodMonitorExitPreservingReservation}
            },
            {
               {TR_IA32JitMonitorEnterPreservingReservation, TR_IA32JitMethodMonitorEnterPreservingReservation},
               {TR_AMD64JitMonitorEnterPreservingReservation, TR_AMD64JitMethodMonitorEnterPreservingReservation}
            }
         };

      helper = helpersCase2[entry?1:0][TR::Compiler->target.is64Bit()?1:0][methodMonitor?1:0];
      return true;
      }

   return false;
   }



// Generate code to allocate from the object heap.  Returns the register
// containing the address of the allocation.
//
// If the sizeReg is non-null, the allocation is variable length.  In this case
// the elementSize is meaningful and "size" is the extra size to be added.
// Otherwise "size" contains the total size of the allocation.
//
// Also, on return the "segmentReg" register is set to the address of the
// memory segment.
//
static void genHeapAlloc(
      TR::Node *node,
      TR_OpaqueClassBlock *clazz,
      int32_t allocationSizeOrDataOffset,
      int32_t elementSize,
      TR::Register *sizeReg,
      TR::Register *eaxReal,
      TR::Register *segmentReg,
      TR::Register *tempReg,
      TR::LabelSymbol *failLabel,
      TR::CodeGenerator *cg)
   {

   // Load the current heap segment and see if there is room in it. Loop if
   // we can't get the lock on the segment.
   //
   TR::Compilation *comp = cg->comp();
   TR::Register *vmThreadReg = cg->getVMThreadRegister();
   bool generateArraylets = comp->generateArraylets();

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

   if (TR::Options::getCmdLineOptions()->realTimeGC())
      {
#if defined(J9VM_GC_REALTIME)
      // this will be bogus for variable length allocations because it only includes the header size (+ arraylet ptr for arrays)
      UDATA sizeClass = fej9->getObjectSizeClass(allocationSizeOrDataOffset);

      if (comp->getOption(TR_BreakOnNew))
         generateInstruction(BADIA32Op, node, cg);

      // heap allocation, so proceed
      if (sizeReg)
         {
         generateRegRegInstruction(XORRegReg(), node, eaxReal, eaxReal, cg);

         // make sure size isn't too big
         // convert max object size to num elements because computing an object size from num elements may overflow
         TR_ASSERT(fej9->getMaxObjectSizeForSizeClass() <= UINT_MAX, "assertion failure");
         generateRegImmInstruction(CMPRegImm4(), node, sizeReg, (fej9->getMaxObjectSizeForSizeClass()-allocationSizeOrDataOffset)/elementSize, cg);
         generateLabelInstruction(JA4, node, failLabel, cg);

         // Hybrid arraylets need a zero length test if the size is unknown.
         //
         if (!generateArraylets)
            {
            generateRegRegInstruction(TEST4RegReg, node, sizeReg, sizeReg, cg);
            generateLabelInstruction(JE4, node, failLabel, cg);
            }

         // need to round up to sizeof(UDATA) so we can use it to index into size class index array
         // conservatively just add sizeof(UDATA) bytes and round
         int32_t round = 0;
         if (elementSize < sizeof(UDATA))
            round = sizeof(UDATA) - 1;

         // now compute size of object in bytes
         generateRegMemInstruction(LEARegMem(),
                                   node,
                                   segmentReg,
                                   generateX86MemoryReference(eaxReal,
                                                              sizeReg,
                                                              TR::MemoryReference::convertMultiplierToStride(elementSize),
                                                              allocationSizeOrDataOffset + round, cg), cg);


         if (elementSize < sizeof(UDATA))
            generateRegImmInstruction(ANDRegImms(), node, segmentReg, -(int32_t)sizeof(UDATA), cg);

#ifdef J9VM_INTERP_FLAGS_IN_CLASS_SLOT
         generateRegImmInstruction(CMPRegImm4(), node, segmentReg, J9_GC_MINIMUM_OBJECT_SIZE, cg);
         TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
         generateLabelInstruction(JAE4, node, doneLabel, cg);
         generateRegImmInstruction(MOVRegImm4(), node, segmentReg, J9_GC_MINIMUM_OBJECT_SIZE, cg);
         generateLabelInstruction(LABEL, node, doneLabel, cg);
#endif

         // get size class
         generateRegMemInstruction(LRegMem(),
                                   node,
                                   tempReg,
                                   generateX86MemoryReference(vmThreadReg, fej9->thisThreadJavaVMOffset(), cg), cg);
         generateRegMemInstruction(LRegMem(),
                                   node,
                                   tempReg,
                                   generateX86MemoryReference(tempReg, fej9->getRealtimeSizeClassesOffset(), cg), cg);
         generateRegMemInstruction(LRegMem(),
                                   node,
                                   tempReg,
                                   generateX86MemoryReference(tempReg,
                                                              segmentReg, TR::MemoryReference::convertMultiplierToStride(1),
                                                              fej9->getSizeClassesIndexOffset(),
                                                              cg),
                                   cg);

         // tempReg now holds size class
         TR::MemoryReference *currentMemRef, *topMemRef, *currentMemRefBump;
         if (TR::Compiler->target.is64Bit())
            {
            TR_ASSERT(sizeof(J9VMGCSegregatedAllocationCacheEntry) == 16, "unexpected J9VMGCSegregatedAllocationCacheEntry size");
            // going to play some games here
            //   need to use tempReg to index into two arrays:
            //        1) allocation caches
            //        2) cell size array
            //   The first one has stride 16, second one stride sizeof(UDATA)
            //   We need a shift instruction to be able to do stride 16
            //   To avoid two shifts, only do one for stride sizeof(UDATA) and use a multiplier in memory ref for 16
            //   64-bit, so shift 3 times for sizeof(UDATA) and use multiplier stride 2 in memory references
            generateRegImmInstruction(SHLRegImm1(), node, tempReg, 3, cg);
            currentMemRef =     generateX86MemoryReference(vmThreadReg, tempReg, TR::MemoryReference::convertMultiplierToStride(2), fej9->thisThreadAllocationCacheCurrentOffset(0), cg);
            topMemRef =         generateX86MemoryReference(vmThreadReg, tempReg, TR::MemoryReference::convertMultiplierToStride(2), fej9->thisThreadAllocationCacheTopOffset(0), cg);
            currentMemRefBump = generateX86MemoryReference(vmThreadReg, tempReg, TR::MemoryReference::convertMultiplierToStride(2), fej9->thisThreadAllocationCacheCurrentOffset(0), cg);
            }
         else
            {
            // size needs to be 8 or less or it there's no multiplier stride available (would need to use other branch of else)
            TR_ASSERT(sizeof(J9VMGCSegregatedAllocationCacheEntry) <= 8, "unexpected J9VMGCSegregatedAllocationCacheEntry size");

            currentMemRef =     generateX86MemoryReference(vmThreadReg, tempReg,
                                TR::MemoryReference::convertMultiplierToStride(sizeof(J9VMGCSegregatedAllocationCacheEntry)),
                                fej9->thisThreadAllocationCacheCurrentOffset(0), cg);
            topMemRef =         generateX86MemoryReference(vmThreadReg, tempReg,
                                TR::MemoryReference::convertMultiplierToStride(sizeof(J9VMGCSegregatedAllocationCacheEntry)),
                                fej9->thisThreadAllocationCacheTopOffset(0), cg);
            currentMemRefBump = generateX86MemoryReference(vmThreadReg, tempReg,
                                TR::MemoryReference::convertMultiplierToStride(sizeof(J9VMGCSegregatedAllocationCacheEntry)),
                                fej9->thisThreadAllocationCacheCurrentOffset(0), cg);
            }
         // tempReg now contains size class (32-bit) or size class * sizeof(J9VMGCSegregatedAllocationCacheEntry) (64-bit)

         // get next cell for this size class
         generateRegMemInstruction(LRegMem(), node, eaxReal, currentMemRef, cg);

         // if null, then no cell available, use slow path
         generateRegMemInstruction(CMPRegMem(), node, eaxReal, topMemRef, cg);
         generateLabelInstruction(JAE4, node, failLabel, cg);

         // have a valid cell, need to update current cell pointer
         generateRegMemInstruction(LRegMem(),
                                   node,
                                   segmentReg,
                                   generateX86MemoryReference(vmThreadReg, fej9->thisThreadJavaVMOffset(), cg), cg);
         generateRegMemInstruction(LRegMem(),
                                   node,
                                   segmentReg,
                                   generateX86MemoryReference(segmentReg, fej9->getRealtimeSizeClassesOffset(), cg), cg);
         if (TR::Compiler->target.is64Bit())
            {
            // tempReg already has already been shifted for sizeof(UDATA)
            generateRegMemInstruction(LRegMem(),
                                      node,
                                      segmentReg,
                                      generateX86MemoryReference(segmentReg,
                                                                 tempReg,
                                                                 TR::MemoryReference::convertMultiplierToStride(1),
                                                                 fej9->getSmallCellSizesOffset(),
                                                                 cg),
                                      cg);
            }
         else
            {
            // tempReg needs to be shifted for sizeof(UDATA)
            generateRegMemInstruction(LRegMem(),
                                   node,
                                   segmentReg,
                                   generateX86MemoryReference(segmentReg,
                                                              tempReg,
                                                              TR::MemoryReference::convertMultiplierToStride(sizeof(UDATA)),
                                                              fej9->getSmallCellSizesOffset(),
                                                              cg),
                                   cg);
            }
         // segmentReg now holds cell size

         // update current cell by cell size
         generateMemRegInstruction(ADDMemReg(), node, currentMemRefBump, segmentReg, cg);
         }
      else
         {
         generateRegMemInstruction(LRegMem(),
                                node,
                                eaxReal,
                                generateX86MemoryReference(vmThreadReg,
                                                           fej9->thisThreadAllocationCacheCurrentOffset(sizeClass),
                                                           cg),
                                cg);

         generateRegMemInstruction(CMPRegMem(),
                                node,
                                eaxReal,
                                generateX86MemoryReference(vmThreadReg,
                                                           fej9->thisThreadAllocationCacheTopOffset(sizeClass),
                                                           cg),
                                cg);

         generateLabelInstruction(JAE4, node, failLabel, cg);

         // we have an object in eaxReal, now bump the current updatepointer
         TR_X86OpCodes opcode;
         uint32_t cellSize = fej9->getCellSizeForSizeClass(sizeClass);
         if (cellSize <= 127)
            opcode = ADDMemImms();
         else if (cellSize == 128)
            {
            opcode = SUBMemImms();
            cellSize = (uint32_t)-128;
            }
         else
            opcode = ADDMemImm4();

         generateMemImmInstruction(opcode, node,
                                generateX86MemoryReference(vmThreadReg,
                                                           fej9->thisThreadAllocationCacheCurrentOffset(sizeClass),
                                                           cg),
                                cellSize, cg);
         }

      // we're done
      return;
#endif
      }
   else
      {
      bool shouldAlignToCacheBoundary = false;
      bool isSmallAllocation = false;

      size_t heapAlloc_offset=offsetof(J9VMThread, heapAlloc);
      size_t heapTop_offset=offsetof(J9VMThread, heapTop);
      size_t tlhPrefetchFTA_offset= offsetof(J9VMThread, tlhPrefetchFTA);
#ifdef J9VM_GC_NON_ZERO_TLH
      if (!comp->getOption(TR_DisableDualTLH) && node->canSkipZeroInitialization())
         {
         heapAlloc_offset=offsetof(J9VMThread, nonZeroHeapAlloc);
         heapTop_offset=offsetof(J9VMThread, nonZeroHeapTop);
         tlhPrefetchFTA_offset= offsetof(J9VMThread, nonZeroTlhPrefetchFTA);
         }
#endif
      // Load the base of the next available heap storage.  This load is done speculatively on the assumption that the
      // allocation will be inlined.  If the assumption turns out to be false then the performance impact should be minimal
      // because the helper will be called in that case.  It is necessary to insert this load here so that it dominates all
      // control paths through this internal control flow region.
      //
      generateRegMemInstruction(LRegMem(),
                                node,
                                eaxReal,
                                generateX86MemoryReference(vmThreadReg,heapAlloc_offset, cg), cg);

      if (comp->getOption(TR_EnableNewAllocationProfiling))
         {
         TR::LabelSymbol *doneProfilingLabel = generateLabelSymbol(cg);

         uint32_t *globalAllocationDataPointer = fej9->getGlobalAllocationDataPointer();
         if (globalAllocationDataPointer)
            {
            TR::MemoryReference *gmr = generateX86MemoryReference((uintptrj_t)globalAllocationDataPointer, cg);

            generateMemImmInstruction(CMP4MemImm4,
                                      node,
                                      generateX86MemoryReference((uint32_t)(uintptrj_t)globalAllocationDataPointer, cg),
                                      0x07ffffff,
                                      cg);
            generateLabelInstruction(JAE4, node, doneProfilingLabel, cg);

            generateMemInstruction(INC4Mem, node, gmr, cg);
            uint32_t *dataPointer = fej9->getAllocationProfilingDataPointer(node->getByteCodeInfo(), clazz, node->getOwningMethod(), comp);
            if (dataPointer)
               {
               TR::MemoryReference *mr = generateX86MemoryReference((uint32_t)(uintptrj_t)dataPointer, cg);
               generateMemInstruction(INC4Mem, node, mr, cg);
               }

            generateLabelInstruction(LABEL, node, doneProfilingLabel, cg);
            }
         }

      bool canSkipOverflowCheck = false;

      // If the array length is constant, check to see if the size of the array will fit in a single arraylet leaf.
      // If the allocation size is too large, call the snippet.
      //
      if (generateArraylets && (node->getOpCodeValue() == TR::anewarray || node->getOpCodeValue() == TR::newarray))
         {
         if (comp->getOption(TR_DisableTarokInlineArrayletAllocation))
            generateLabelInstruction(JMP4, node, failLabel, cg);

         if (sizeReg)
            {
            uint32_t maxContiguousArrayletLeafSizeInBytes =
               (uint32_t)(TR::Compiler->om.arrayletLeafSize() - TR::Compiler->om.sizeofReferenceAddress());

            int32_t maxArrayletSizeInElements = maxContiguousArrayletLeafSizeInBytes/elementSize;

            // Hybrid arraylets need a zero length test if the size is unknown.
            //
            generateRegRegInstruction(TEST4RegReg, node, sizeReg, sizeReg, cg);
            generateLabelInstruction(JE4, node, failLabel, cg);

            generateRegImmInstruction(CMP4RegImm4, node, sizeReg, maxArrayletSizeInElements, cg);
            generateLabelInstruction(JAE4, node, failLabel, cg);

            // If the max arraylet leaf size is less than the amount of free space available on
            // the stack, there is no need to check for an overflow scenario.
            //
            if (maxContiguousArrayletLeafSizeInBytes <= cg->getMaxObjectSizeGuaranteedNotToOverflow() )
               canSkipOverflowCheck = true;
            }
         else if (TR::Compiler->om.isDiscontiguousArray(allocationSizeOrDataOffset))
            {
            // TODO: just call the helper directly and don't generate any
            //       further instructions.
            //
            // Actually, we should never get here because we've already checked
            // constant lengths for discontiguity...
            //
            generateLabelInstruction(JMP4, node, failLabel, cg);
            }
         }

      if (sizeReg && !canSkipOverflowCheck)
         {
         // Hybrid arraylets need a zero length test if the size is unknown.
         // The length could be zero.
         //
         if (!generateArraylets)
            {
            generateRegRegInstruction(TEST4RegReg, node, sizeReg, sizeReg, cg);
            generateLabelInstruction(JE4, node, failLabel, cg);
            }

         // The GC will guarantee that at least 'maxObjectSizeGuaranteedNotToOverflow' bytes
         // of slush will exist between the top of the heap and the end of the address space.
         //
         uintptrj_t maxObjectSize = cg->getMaxObjectSizeGuaranteedNotToOverflow();
         uintptrj_t maxObjectSizeInElements = maxObjectSize / elementSize;

         if (TR::Compiler->target.is64Bit() && !(maxObjectSizeInElements > 0 && maxObjectSizeInElements <= (uintptrj_t)INT_MAX))
            {
            generateRegImm64Instruction(MOV8RegImm64, node, tempReg, maxObjectSizeInElements, cg);
            generateRegRegInstruction(CMP8RegReg, node, sizeReg, tempReg, cg);
            }
         else
            {
            generateRegImmInstruction(CMPRegImm4(), node, sizeReg, (int32_t)maxObjectSizeInElements, cg);
            }

         // Must be an unsigned comparison on sizes.
         //
         generateLabelInstruction(JAE4, node, failLabel, cg);
         }

#if !defined(J9VM_GC_THREAD_LOCAL_HEAP)
      // Establish a loop label in case the new heap pointer cannot be committed.
      //
      TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);
      generateLabelInstruction(LABEL, node, loopLabel, cg);
#endif

      if (sizeReg)
         {
         // calculate variable size, rounding up if necessary to a intptrj_t multiple boundary
         //
         int32_t round; // zero indicates no rounding is necessary

         if (!generateArraylets)
            {
//            TR_ASSERT(allocationSizeOrDataOffset % fej9->getObjectAlignmentInBytes() == 0, "Array header size of %d is not a multiple of %d", allocationSizeOrDataOffset, fej9->getObjectAlignmentInBytes());
            }


         round = (elementSize < fej9->getObjectAlignmentInBytes()) ? fej9->getObjectAlignmentInBytes() : 0;

         int32_t disp32 = round ? (round-1) : 0;
#ifdef J9VM_INTERP_FLAGS_IN_CLASS_SLOT
         if ( (node->getOpCodeValue() == TR::anewarray || node->getOpCodeValue() == TR::newarray))
            {
            // All arrays in combo builds will always be at least 12 bytes in size in all specs:
            //
            // 1)  class pointer + contig length + one or more elements
            // 2)  class pointer + 0 + 0 (for zero length arrays)
            //
            TR_ASSERT(J9_GC_MINIMUM_OBJECT_SIZE >= 8, "Expecting a minimum object size >= 8 (actual minimum is %d)\n", J9_GC_MINIMUM_OBJECT_SIZE);

            generateRegMemInstruction(
               LEARegMem(),
               node,
               tempReg,
               generateX86MemoryReference(
                  eaxReal,
                  sizeReg,
                  TR::MemoryReference::convertMultiplierToStride(elementSize),
                  allocationSizeOrDataOffset+disp32, cg), cg);

            if (round)
               {
               generateRegImmInstruction(ANDRegImm4(), node, tempReg, -round, cg);
               }
            }
         else
#endif
            {
#ifdef J9VM_INTERP_FLAGS_IN_CLASS_SLOT
         generateRegRegInstruction(XORRegReg(), node, tempReg, tempReg, cg);
#endif


         generateRegMemInstruction(
                                   LEARegMem(),
                                   node,
                                   tempReg,
                                   generateX86MemoryReference(
#ifdef J9VM_INTERP_FLAGS_IN_CLASS_SLOT
                                                              tempReg,
#else
                                                              eaxReal,
#endif
                                                              sizeReg,
                                                              TR::MemoryReference::convertMultiplierToStride(elementSize),
                                                              allocationSizeOrDataOffset+disp32, cg), cg);

         if (round)
            {
            generateRegImmInstruction(ANDRegImm4(), node, tempReg, -round, cg);
            }

#ifdef J9VM_INTERP_FLAGS_IN_CLASS_SLOT
         generateRegImmInstruction(CMPRegImm4(), node, tempReg, J9_GC_MINIMUM_OBJECT_SIZE, cg);
         TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
         generateLabelInstruction(JAE4, node, doneLabel, cg);
         generateRegImmInstruction(MOVRegImm4(), node, tempReg, J9_GC_MINIMUM_OBJECT_SIZE, cg);
         generateLabelInstruction(LABEL, node, doneLabel, cg);
         generateRegRegInstruction(ADDRegReg(), node, tempReg, eaxReal, cg);
#endif
            }
         }
      else
         {
         isSmallAllocation = allocationSizeOrDataOffset <= 0x40 ? true : false;
         allocationSizeOrDataOffset = (allocationSizeOrDataOffset+fej9->getObjectAlignmentInBytes()-1) & (-fej9->getObjectAlignmentInBytes());

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
         if ((node->getOpCodeValue() == TR::New) &&
             (comp->getMethodHotness() >= hot || node->shouldAlignTLHAlloc()))
            {
            TR_OpaqueMethodBlock *ownMethod = node->getOwningMethod();
            TR::Node *classChild = node->getFirstChild();
            char * className = NULL;
            TR_OpaqueClassBlock *clazz = NULL;

            if (classChild &&
                classChild->getSymbolReference() &&
                !classChild->getSymbolReference()->isUnresolved())
               {
               TR::SymbolReference *symRef = classChild->getSymbolReference();
               TR::Symbol *sym = symRef->getSymbol();

               if (sym &&
                   sym->getKind() == TR::Symbol::IsStatic &&
                   sym->isClassObject())
                  {
                  TR::StaticSymbol * staticSym = symRef->getSymbol()->castToStaticSymbol();
                  void * staticAddress = staticSym->getStaticAddress();
                  if (symRef->getCPIndex() >= 0)
                     {
                     if (!staticSym->addressIsCPIndexOfStatic() && staticAddress)
                        {
                        int32_t len;
                        className = TR::Compiler->cls.classNameChars(comp,symRef, len);
                        clazz = (TR_OpaqueClassBlock *)staticAddress;
                        }
                     }
                  }
               }

            uint32_t instanceSizeForAlignment = 30;
            static char *p= feGetEnv("TR_AlignInstanceSize");
            if (p)
               instanceSizeForAlignment = atoi(p);

            if ((comp->getMethodHotness() >= hot) && clazz &&
                !cg->getCurrentEvaluationBlock()->isCold() &&
                TR::Compiler->cls.classInstanceSize(clazz)>=instanceSizeForAlignment)
               {
               shouldAlignToCacheBoundary = true;

               generateRegMemInstruction(LEARegMem(), node, eaxReal,
                                         generateX86MemoryReference(eaxReal, 63, cg), cg);
               generateRegImmInstruction(ANDRegImm4(), node, eaxReal, 0xFFFFFFC0, cg);
               }
            }
#endif // J9VM_GC_THREAD_LOCAL_HEAP

         if ((uint32_t)allocationSizeOrDataOffset > cg->getMaxObjectSizeGuaranteedNotToOverflow())
            {
            generateRegRegInstruction(MOVRegReg(),  node, tempReg, eaxReal, cg);
            if (allocationSizeOrDataOffset <= 127)
               generateRegImmInstruction(ADDRegImms(), node, tempReg, allocationSizeOrDataOffset, cg);
            else if (allocationSizeOrDataOffset == 128)
               generateRegImmInstruction(SUBRegImms(), node, tempReg, (unsigned)-128, cg);
            else
               generateRegImmInstruction(ADDRegImm4(), node, tempReg, allocationSizeOrDataOffset, cg);

            // Check for overflow
            generateLabelInstruction(JB4, node, failLabel, cg);
            }
         else
            {
            generateRegMemInstruction(LEARegMem(), node, tempReg,
                                      generateX86MemoryReference(eaxReal, allocationSizeOrDataOffset, cg), cg);
            }
         }

      generateRegMemInstruction(CMPRegMem(),
                                node,
                                tempReg,
                                generateX86MemoryReference(vmThreadReg, heapTop_offset, cg), cg);

      generateLabelInstruction(JA4, node, failLabel, cg);

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)

      if (shouldAlignToCacheBoundary)
         {
         // Alignment to a cache line boundary may require inserting more padding than is normally
         // necessary to achieve the alignment.  In those cases, insert GC dark matter to describe
         // the space inserted.
         //

         generateRegInstruction(PUSHReg, node, tempReg, cg);
         generateRegMemInstruction(LRegMem(),
                                   node,
                                   tempReg,
                                   generateX86MemoryReference(vmThreadReg,heapAlloc_offset, cg), cg);

         generateRegRegInstruction(SUBRegReg(),  node, eaxReal, tempReg, cg);

         TR::LabelSymbol *doneAlignLabel = generateLabelSymbol(cg);
         TR::LabelSymbol *multiSlotGapLabel = generateLabelSymbol(cg);

         generateRegImmInstruction(CMPRegImms(), node, eaxReal, sizeof(uintptrj_t), cg);
         generateLabelInstruction(JB4, node, doneAlignLabel, cg);
         generateLabelInstruction(JA4, node, multiSlotGapLabel, cg);

         int32_t singleSlotHole;

         singleSlotHole = J9_GC_SINGLE_SLOT_HOLE;

         if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
            {
            generateMemImmInstruction(S4MemImm4, node,
                                      generateX86MemoryReference(tempReg, 0, cg), singleSlotHole, cg);
            generateMemImmInstruction(S4MemImm4, node,
                                      generateX86MemoryReference(tempReg, 4, cg), singleSlotHole, cg);
            }
         else
            {
            generateMemImmInstruction(
            SMemImm4(), node,
            generateX86MemoryReference(tempReg, 0, cg), singleSlotHole, cg);
            }

         generateLabelInstruction(JMP4, node, doneAlignLabel, cg);
         generateLabelInstruction(LABEL, node, multiSlotGapLabel, cg);

         int32_t multiSlotHole;

         multiSlotHole = J9_GC_MULTI_SLOT_HOLE;

         generateMemImmInstruction(
                                   SMemImm4(), node,
                                   generateX86MemoryReference(tempReg, 0, cg),
                                   multiSlotHole, cg);

         generateMemRegInstruction(
                                   SMemReg(), node,
                                   generateX86MemoryReference(tempReg, sizeof(uintptrj_t), cg),
                                   eaxReal, cg);

         generateLabelInstruction(LABEL, node, doneAlignLabel, cg);
         generateRegRegInstruction(ADDRegReg(), node, eaxReal, tempReg, cg);
         generateRegInstruction(POPReg, node, tempReg, cg);
         }

      // Make sure that the arraylet is aligned properly.
      //
      if (generateArraylets && (node->getOpCodeValue() == TR::anewarray || node->getOpCodeValue() == TR::newarray) )
         {
         generateRegMemInstruction(LEARegMem(),node,tempReg, generateX86MemoryReference(tempReg,fej9->getObjectAlignmentInBytes()-1,cg),cg);
         if (TR::Compiler->target.is64Bit())
            generateRegImmInstruction(AND8RegImm4,node,tempReg,-fej9->getObjectAlignmentInBytes(),cg);
         else
            generateRegImmInstruction(AND4RegImm4,node,tempReg,-fej9->getObjectAlignmentInBytes(),cg);
         }

      generateMemRegInstruction(SMemReg(),
                                node,
                                generateX86MemoryReference(vmThreadReg, heapAlloc_offset, cg),
                                tempReg, cg);

      if (!isSmallAllocation && cg->enableTLHPrefetching())
         {
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
         if (!comp->getOption(TR_DisableDualTLH) && node->canSkipZeroInitialization())
            {
            prefetchThunkGenerated = (fej9->getAllocationNoZeroPrefetchCodeSnippetAddress(comp) !=0);
            }
#endif
         if (useSharedCodeCacheSnippet && prefetchThunkGenerated)
            {
            useDirectPrefetchCall = true;
            }

         if (!comp->getOption(TR_EnableNewX86PrefetchTLH))
            {
            generateRegRegInstruction(SUB4RegReg, node, tempReg, eaxReal, cg);

            generateMemRegInstruction(SUB4MemReg,
                                      node,
                                      generateX86MemoryReference(vmThreadReg, tlhPrefetchFTA_offset, cg),
                                      tempReg, cg);
            if (!useDirectPrefetchCall)
               generateLabelInstruction(JLE4, node, prefetchSnippetLabel, cg);
            else
               {
               generateLabelInstruction(JG4, node, restartLabel, cg);
               TR::SymbolReference * helperSymRef = cg->getSymRefTab()->findOrCreateRuntimeHelper(TR_X86CodeCachePrefetchHelper, false, false, false);
               TR::MethodSymbol *helperSymbol = helperSymRef->getSymbol()->castToMethodSymbol();
#ifdef J9VM_GC_NON_ZERO_TLH
               if (!comp->getOption(TR_DisableDualTLH) && node->canSkipZeroInitialization())
                  {
                  helperSymbol->setMethodAddress(fej9->getAllocationNoZeroPrefetchCodeSnippetAddress(comp));
                  }
               else
                  {
                  helperSymbol->setMethodAddress(fej9->getAllocationPrefetchCodeSnippetAddress(comp));
                  }
#else
               helperSymbol->setMethodAddress(fej9->getAllocationPrefetchCodeSnippetAddress(comp));
#endif
               generateImmSymInstruction(CALLImm4, node, (uintptr_t)helperSymbol->getMethodAddress(), helperSymRef, cg);
               }
            }
         else
            {
            // This currently only works when 'tlhPrefetchFTA' field is 4 bytes (on 32-bit or a
            // compressed references build).  True 64-bit support requires this field be widened
            // to 64-bits.
            //
            generateRegMemInstruction(CMP4RegMem, node,
                                      tempReg,
                                      generateX86MemoryReference(vmThreadReg,tlhPrefetchFTA_offset, cg),
                                      cg);
            generateLabelInstruction(JAE4, node, prefetchSnippetLabel, cg);
            }

         generateLabelInstruction(LABEL, node, restartLabel, cg);
         }

#else // J9VM_GC_THREAD_LOCAL_HEAP
      generateMemRegInstruction(CMPXCHGMemReg(), node, generateX86MemoryReference(vmThreadReg, heapAlloc_offset, cg), tempReg, cg);
      generateLabelInstruction(JNE4, node, loopLabel, cg);
#endif // !J9VM_GC_THREAD_LOCAL_HEAP
      }
   }

// ------------------------------------------------------------------------------
// genHeapAlloc2
//
// Will eventually become the de facto genHeapAlloc.  Needs packed array and 2TLH
// support.
// ------------------------------------------------------------------------------

static void genHeapAlloc2(
   TR::Node             *node,
   TR_OpaqueClassBlock *clazz,
   int32_t              allocationSizeOrDataOffset,
   int32_t              elementSize,
   TR::Register         *sizeReg,
   TR::Register         *eaxReal,
   TR::Register         *segmentReg,
   TR::Register         *tempReg,
   TR::LabelSymbol       *failLabel,
   TR::CodeGenerator    *cg)
   {
   // Load the current heap segment and see if there is room in it. Loop if
   // we can't get the lock on the segment.
   //
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR::Register *vmThreadReg = cg->getVMThreadRegister();
   bool generateArraylets = comp->generateArraylets();
   bool isTooSmallToPrefetch = false;

      {
      bool shouldAlignToCacheBoundary = false;

      // Load the base of the next available heap storage.  This load is done speculatively on the assumption that the
      // allocation will be inlined.  If the assumption turns out to be false then the performance impact should be minimal
      // because the helper will be called in that case.  It is necessary to insert this load here so that it dominates all
      // control paths through this internal control flow region.
      //

      if (sizeReg)
         {

         // -------------
         //
         // VARIABLE SIZE
         //
         // -------------

         // The GC will guarantee that at least 'maxObjectSizeGuaranteedNotToOverflow' bytes
         // of slush will exist between the top of the heap and the end of the address space.
         //
         uintptrj_t maxObjectSize = cg->getMaxObjectSizeGuaranteedNotToOverflow();
         uintptrj_t maxObjectSizeInElements = maxObjectSize / elementSize;

         if (TR::Compiler->target.is64Bit() && !(maxObjectSizeInElements > 0 && maxObjectSizeInElements <= (uintptrj_t)INT_MAX))
            {
            generateRegImm64Instruction(MOV8RegImm64, node, segmentReg, maxObjectSizeInElements, cg);
            generateRegRegInstruction(CMP8RegReg, node, sizeReg, segmentReg, cg);
            }
         else
            {
            generateRegImmInstruction(CMPRegImm4(), node, sizeReg, (int32_t)maxObjectSizeInElements, cg);
            }

         // Must be an unsigned comparison on sizes.
         //
         generateLabelInstruction(JAE4, node, failLabel, cg);


         generateRegMemInstruction(LRegMem(),
                                   node,
                                   eaxReal,
                                   generateX86MemoryReference(vmThreadReg,
                                                              offsetof(J9VMThread, heapAlloc), cg), cg);


         // calculate variable size, rounding up if necessary to a intptrj_t multiple boundary
         //
         int32_t round; // zero indicates no rounding is necessary

         if (!generateArraylets)
            {
//            TR_ASSERT(allocationSizeOrDataOffset % fej9->getObjectAlignmentInBytes() == 0, "Array header size of %d is not a multiple of %d", allocationSizeOrDataOffset, fej9->getObjectAlignmentInBytes());
            }

         round = (elementSize >= fej9->getObjectAlignmentInBytes())? 0 : fej9->getObjectAlignmentInBytes();
         int32_t disp32 = round ? (round-1) : 0;

/*
   mov rcx, rdx               ; # of array elements                  (1)
   cmp rcx, 1                                                        (1)
   adc rcx, 0                 ; adjust for zero length               (1)

   shl rcx, 2                                                        (1)
   add rcx, 0xf               ; rcx + header (8) + 7                 (1)

   and rcx,0xfffffffffffffff8 ; round down                           (1)
*/

         generateRegRegInstruction(MOV4RegReg, node, segmentReg, sizeReg, cg);

         // Artificially adjust the number of elements by 1 if the array is zero length.  This works
         // because either the array is zero length and needs a discontiguous array length field
         // (occupying a slot) or it has at least 1 element which will take up a slot anyway.
         //
         // Native 64-bit array headers do not need this adjustment because the
         // contiguous and discontiguous array headers are the same size.
         //
         if (TR::Compiler->target.is32Bit() || (TR::Compiler->target.is64Bit() && comp->useCompressedPointers()))
            {
            generateRegImmInstruction(CMP4RegImm4, node, segmentReg, 1, cg);
            generateRegImmInstruction(ADC4RegImm4, node, segmentReg, 0, cg);
            }

         uint8_t shiftVal = TR::MemoryReference::convertMultiplierToStride(elementSize);
         if (shiftVal > 0)
            {
            generateRegImmInstruction(SHLRegImm1(), node, segmentReg, shiftVal, cg);
            }

         generateRegImmInstruction(ADDRegImm4(), node, segmentReg, allocationSizeOrDataOffset+disp32, cg);

         if (round)
            {
            generateRegImmInstruction(ANDRegImm4(), node, segmentReg, -round, cg);
            }

         // Copy full object size in bytes to RCX for zero init via REP STOSQ
         //
         generateRegRegInstruction(MOVRegReg(), node, tempReg, segmentReg, cg);

         generateRegRegInstruction(ADDRegReg(), node, segmentReg, eaxReal, cg);
         }
      else
         {
         // ----------
         //
         // FIXED SIZE
         //
         // ----------

         generateRegMemInstruction(LRegMem(),
                                   node,
                                   eaxReal,
                                   generateX86MemoryReference(vmThreadReg,
                                                              offsetof(J9VMThread, heapAlloc), cg), cg);

         if (comp->getOptLevel() < hot)
            isTooSmallToPrefetch = allocationSizeOrDataOffset <= 0x40 ? true : false;

         allocationSizeOrDataOffset = (allocationSizeOrDataOffset+fej9->getObjectAlignmentInBytes()-1) & (-fej9->getObjectAlignmentInBytes());

#if defined(J9VM_GC_THREAD_LOCAL_HEAP)
         if ((node->getOpCodeValue() == TR::New) &&
             (comp->getMethodHotness() >= hot || node->shouldAlignTLHAlloc()))
            {
            TR_OpaqueMethodBlock *ownMethod = node->getOwningMethod();

            TR::Node *classChild = node->getFirstChild();
            char * className = NULL;
            TR_OpaqueClassBlock *clazz = NULL;

            if (classChild &&
                classChild->getSymbolReference() &&
                !classChild->getSymbolReference()->isUnresolved())
               {
               TR::SymbolReference *symRef = classChild->getSymbolReference();
               TR::Symbol *sym = symRef->getSymbol();

               if (sym &&
                   sym->getKind() == TR::Symbol::IsStatic &&
                   sym->isClassObject())
                  {
                  TR::StaticSymbol * staticSym = symRef->getSymbol()->castToStaticSymbol();
                  void * staticAddress = staticSym->getStaticAddress();
                  if (symRef->getCPIndex() >= 0)
                     {
                     if (!staticSym->addressIsCPIndexOfStatic() && staticAddress)
                        {
                        int32_t len;
                        className = TR::Compiler->cls.classNameChars(comp, symRef, len);
                        clazz = (TR_OpaqueClassBlock *)staticAddress;
                        }
                     }
                  }
               }

            uint32_t instanceSizeForAlignment = 30;
            static char *p= feGetEnv("TR_AlignInstanceSize");
            if (p)
               instanceSizeForAlignment = atoi(p);

            if ((comp->getMethodHotness() >= hot) && clazz &&
                !cg->getCurrentEvaluationBlock()->isCold() &&
                TR::Compiler->cls.classInstanceSize(clazz)>=instanceSizeForAlignment)
               {
               shouldAlignToCacheBoundary = true;

               generateRegMemInstruction(LEARegMem(), node, eaxReal,
                                         generateX86MemoryReference(eaxReal, 63, cg), cg);
               generateRegImmInstruction(ANDRegImm4(), node, eaxReal, 0xFFFFFFC0, cg);
               }
            }
#endif // J9VM_GC_THREAD_LOCAL_HEAP

         if ((uint32_t)allocationSizeOrDataOffset > cg->getMaxObjectSizeGuaranteedNotToOverflow())
            {
            generateRegRegInstruction(MOVRegReg(),  node, segmentReg, eaxReal, cg);
            if (allocationSizeOrDataOffset <= 127)
               generateRegImmInstruction(ADDRegImms(), node, segmentReg, allocationSizeOrDataOffset, cg);
            else if (allocationSizeOrDataOffset == 128)
               generateRegImmInstruction(SUBRegImms(), node, segmentReg, (unsigned)-128, cg);
            else
               generateRegImmInstruction(ADDRegImm4(), node, segmentReg, allocationSizeOrDataOffset, cg);

            // Check for overflow
            generateLabelInstruction(JB4, node, failLabel, cg);
            }
         else
            {
            generateRegMemInstruction(LEARegMem(), node, segmentReg,
                                      generateX86MemoryReference(eaxReal, allocationSizeOrDataOffset, cg), cg);
            }
         }


      // -----------
      // MERGED PATH
      // -----------

      generateRegMemInstruction(CMPRegMem(),
                                node,
                                segmentReg,
                                generateX86MemoryReference(vmThreadReg, offsetof(J9VMThread, heapTop), cg), cg);

      generateLabelInstruction(JA4, node, failLabel, cg);

      // ------------
      // 1st PREFETCH
      // ------------

      if (!isTooSmallToPrefetch)
         generateMemInstruction(PREFETCHNTA, node, generateX86MemoryReference(segmentReg, 0xc0, cg), cg);

      if (shouldAlignToCacheBoundary)
         {
         // Alignment to a cache line boundary may require inserting more padding than is normally
         // necessary to achieve the alignment.  In those cases, insert GC dark matter to describe
         // the space inserted.
         //

         generateRegInstruction(PUSHReg, node, segmentReg, cg);
         generateRegMemInstruction(LRegMem(),
                                   node,
                                   segmentReg,
                                   generateX86MemoryReference(vmThreadReg,
                                                              offsetof(J9VMThread, heapAlloc), cg), cg);

         generateRegRegInstruction(SUBRegReg(),  node, eaxReal, segmentReg, cg);

         TR::LabelSymbol *doneAlignLabel = generateLabelSymbol(cg);
         TR::LabelSymbol *multiSlotGapLabel = generateLabelSymbol(cg);

         generateRegImmInstruction(CMPRegImms(), node, eaxReal, sizeof(uintptrj_t), cg);
         generateLabelInstruction(JB4, node, doneAlignLabel, cg);
         generateLabelInstruction(JA4, node, multiSlotGapLabel, cg);

         int32_t singleSlotHole;

         singleSlotHole = J9_GC_SINGLE_SLOT_HOLE;

         if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
            {
            generateMemImmInstruction(S4MemImm4, node,
                                      generateX86MemoryReference(segmentReg, 0, cg), singleSlotHole, cg);
            generateMemImmInstruction(S4MemImm4, node,
                                      generateX86MemoryReference(segmentReg, 4, cg), singleSlotHole, cg);
            }
         else
            {
            generateMemImmInstruction(
                                      SMemImm4(), node,
                                      generateX86MemoryReference(segmentReg, 0, cg), singleSlotHole, cg);
            }

         generateLabelInstruction(JMP4, node, doneAlignLabel, cg);
         generateLabelInstruction(LABEL, node, multiSlotGapLabel, cg);

         int32_t multiSlotHole;

         multiSlotHole = J9_GC_MULTI_SLOT_HOLE;

         generateMemImmInstruction(
                                   SMemImm4(), node,
                                   generateX86MemoryReference(segmentReg, 0, cg),
                                   multiSlotHole, cg);

         generateMemRegInstruction(
                                   SMemReg(), node,
                                   generateX86MemoryReference(segmentReg, sizeof(uintptrj_t), cg),
                                   eaxReal, cg);

         generateLabelInstruction(LABEL, node, doneAlignLabel, cg);
         generateRegRegInstruction(ADDRegReg(), node, eaxReal, segmentReg, cg);
         generateRegInstruction(POPReg, node, segmentReg, cg);
         }

      // Make sure that the arraylet is aligned properly.
      //
      if (generateArraylets && (node->getOpCodeValue() == TR::anewarray || node->getOpCodeValue() == TR::newarray) )
         {
         generateRegMemInstruction(LEARegMem(),node,segmentReg, generateX86MemoryReference(tempReg,fej9->getObjectAlignmentInBytes()-1,cg),cg);
         if (TR::Compiler->target.is64Bit())
            generateRegImmInstruction(AND8RegImm4,node,segmentReg,-fej9->getObjectAlignmentInBytes(),cg);
         else
            generateRegImmInstruction(AND4RegImm4,node,segmentReg,-fej9->getObjectAlignmentInBytes(),cg);
         }

      generateMemRegInstruction(SMemReg(),
                                node,
                                generateX86MemoryReference(vmThreadReg, offsetof(J9VMThread, heapAlloc), cg),
                                segmentReg, cg);

      if (!isTooSmallToPrefetch && node->getOpCodeValue() != TR::New)
         {
         // ------------
         // 2nd PREFETCH
         // ------------
         generateMemInstruction(PREFETCHNTA, node, generateX86MemoryReference(segmentReg, 0x100, cg), cg);

         // ------------
         // 3rd PREFETCH
         // ------------
         generateMemInstruction(PREFETCHNTA, node, generateX86MemoryReference(segmentReg, 0x140, cg), cg);

         // ------------
         // 4th PREFETCH
         // ------------
         generateMemInstruction(PREFETCHNTA, node, generateX86MemoryReference(segmentReg, 0x180, cg), cg);
         }
      }
   }

// Generate the code to initialize an object header - used for both new and
// array new
//
static void genInitObjectHeader(TR::Node             *node,
                                TR_OpaqueClassBlock *clazz,
                                TR::Register         *classReg,
                                TR::Register         *objectReg,
                                TR::Register         *tempReg,
                                bool                 isZeroInitialized,
                                TR::CodeGenerator    *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

   bool use64BitClasses = TR::Compiler->target.is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders();

   // This code was moved to this point so that the romClass can be used by the AOT
   // portion without calling the method again.
   //
   J9ROMClass *romClass = 0;
   TR_ASSERT(clazz, "Cannot have a null OpaqueClassBlock\n");
   romClass = TR::Compiler->cls.romClassOf(clazz);

   // --------------------------------------------------------------------------------
   //
   // Initialize CLASS field
   //
   // --------------------------------------------------------------------------------
   //
   TR_X86OpCodes opSMemReg = SMemReg(use64BitClasses);

   TR::Register * clzReg = classReg;

   // TODO: should be able to use a TR_ClassPointer relocation without this stuff (along with class validation)
   if (cg->needClassAndMethodPointerRelocations())
      {
      TR::Register *vmThreadReg = cg->getVMThreadRegister();
      if (node->getOpCodeValue() == TR::newarray)
         {
         generateRegMemInstruction(LRegMem(), node,tempReg,
             generateX86MemoryReference(vmThreadReg,offsetof(J9VMThread, javaVM), cg), cg);
         generateRegMemInstruction(LRegMem(), node, tempReg,
             generateX86MemoryReference(tempReg,
             offsetof(J9JavaVM, booleanArrayClass)+(node->getSecondChild()->getInt()-4)*sizeof(J9Class*), cg), cg);
         // tempReg should contain a 32 bit pointer.
         generateMemRegInstruction(opSMemReg, node,
             generateX86MemoryReference(objectReg, TR::Compiler->om.offsetOfObjectVftField(), cg),
             tempReg, cg);
         clzReg = tempReg;
         }
      else if (node->getOpCodeValue() == TR::anewarray)
         {
         TR_ASSERT(classReg, "must have a classReg for TR::anewarray in AOT mode");
         generateRegMemInstruction(LRegMem(), node, tempReg,
             generateX86MemoryReference(classReg, offsetof(J9Class, arrayClass), cg), cg);
         clzReg = tempReg;
         }
      else
         {
         TR_ASSERT((node->getOpCodeValue() == TR::New)
                   && classReg, "must have a classReg for TR::New in AOT mode");
         clzReg = classReg;
         }
      }


   // For RealTime Code Only.
   int32_t orFlags = 0;
   int32_t orFlagsClass = 0;

   if (!clzReg)
      {
      TR::Instruction *instr = NULL;
      if (use64BitClasses)
         {
         instr = generateRegImm64Instruction(MOV8RegImm64, node, tempReg, ((intptrj_t)clazz|orFlagsClass), cg);
         generateMemRegInstruction(S8MemReg, node, generateX86MemoryReference(objectReg, TR::Compiler->om.offsetOfObjectVftField(), cg), tempReg, cg);
         }
      else
         {
         instr = generateMemImmInstruction(S4MemImm4, node, generateX86MemoryReference(objectReg, TR::Compiler->om.offsetOfObjectVftField(), cg), (int32_t)((uintptr_t)clazz|orFlagsClass), cg);
         }

      // HCR in genInitObjectHeader
      if (instr && cg->wantToPatchClassPointer(clazz, node))
         comp->getStaticHCRPICSites()->push_front(instr);
      }
   else
      {
      if (orFlagsClass != 0)
         generateRegImmInstruction(use64BitClasses ? OR8RegImm4 : OR4RegImm4,  node, clzReg, orFlagsClass, cg);
      generateMemRegInstruction(opSMemReg, node,
          generateX86MemoryReference(objectReg, TR::Compiler->om.offsetOfObjectVftField(), cg), clzReg, cg);
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
   if (romClass)
      {
      orFlags |= romClass->instanceShape;
      orFlags |= fej9->getStaticObjectFlags();

#if defined(J9VM_OPT_NEW_OBJECT_HASH)
      // put orFlags or 0 into header if needed
      generateMemImmInstruction(S4MemImm4, node,
                                generateX86MemoryReference(objectReg, TMP_OFFSETOF_J9OBJECT_FLAGS, cg),
                                orFlags, cg);

#endif /* !J9VM_OPT_NEW_OBJECT_HASH */
      }
#endif /* FLAGS_IN_CLASS_SLOT */

   // --------------------------------------------------------------------------------
   //
   // Initialize MONITOR field
   //
   // --------------------------------------------------------------------------------
   //
   if (!isZeroInitialized)
      {
      bool initLw = (node->getOpCodeValue() != TR::New);
      int lwOffset = fej9->getByteOffsetToLockword(clazz);
      if (lwOffset == -1)
         initLw = false;

      if (initLw)
         {
         TR_X86OpCodes op = (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord()) ? S4MemImm4 : SMemImm4();
         generateMemImmInstruction(op, node, generateX86MemoryReference(objectReg, lwOffset, cg), 0, cg);
         }
      }
   }


// Generate the code to initialize an array object header
//
static void genInitArrayHeader(
      TR::Node *node,
      TR_OpaqueClassBlock *clazz,
      TR::Register *classReg,
      TR::Register *objectReg,
      TR::Register *sizeReg,
      int32_t elementSize,
      int32_t arrayletDataOffset,
      TR::Register *tempReg,
      bool isZeroInitialized,
      TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

   // Initialize the object header
   //
   genInitObjectHeader(node, clazz, classReg, objectReg, tempReg, isZeroInitialized, cg);

   int32_t arraySizeOffset = fej9->getOffsetOfContiguousArraySizeField();

   TR::MemoryReference *arraySizeMR = generateX86MemoryReference(objectReg, arraySizeOffset, cg);

   TR::Compilation *comp = cg->comp();

   bool canUseFastInlineAllocation =
      (!TR::Options::getCmdLineOptions()->realTimeGC() &&
       !comp->generateArraylets()) ? true : false;

   // Initialize the array size
   //
   if (sizeReg)
      {
      // Variable size
      //
      if (canUseFastInlineAllocation)
         {
         // Native 64-bit needs to cover the discontiguous size field
         //
         TR_X86OpCodes storeOp = (TR::Compiler->target.is64Bit() && !comp->useCompressedPointers()) ? S8MemReg : S4MemReg;
         generateMemRegInstruction(storeOp, node, arraySizeMR, sizeReg, cg);
         }
      else
         {
         generateMemRegInstruction(S4MemReg, node, arraySizeMR, sizeReg, cg);
         }
      }
   else
      {
      // Fixed size
      //
      int32_t instanceSize = 0;
      if (canUseFastInlineAllocation)
         {
         // Native 64-bit needs to cover the discontiguous size field
         //
         TR_X86OpCodes storeOp = (TR::Compiler->target.is64Bit() && !comp->useCompressedPointers()) ? S8MemImm4 : S4MemImm4;
         int32_t instanceSize = node->getFirstChild()->getInt();
         generateMemImmInstruction(storeOp, node, arraySizeMR, instanceSize, cg);
         }
      else
         {
         int32_t instanceSize = node->getFirstChild()->getInt();
         generateMemImmInstruction(S4MemImm4, node, arraySizeMR, instanceSize, cg);
         }
      }

   bool generateArraylets = comp->generateArraylets();

   if (generateArraylets)
      {
      // write arraylet pointer
      TR_X86OpCodes storeOp;

      generateRegMemInstruction(
         LEARegMem(), node,
         tempReg,
         generateX86MemoryReference(objectReg, arrayletDataOffset, cg), cg);

      if (comp->useCompressedPointers())
         {
         storeOp = S4MemReg;

         // Compress the arraylet pointer.
         //
         if (TR::Compiler->vm.heapBaseAddress() != 0)
            generateRegImmInstruction(SUB8RegImm4, node, tempReg, TR::Compiler->vm.heapBaseAddress(), cg);
         else if (TR::Compiler->om.compressedReferenceShiftOffset() > 0)
            generateRegImmInstruction(SHR8RegImm1, node, tempReg, TR::Compiler->om.compressedReferenceShiftOffset(), cg);
         }
      else
         {
         storeOp = SMemReg();
         }

      TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
      generateMemRegInstruction(storeOp, node, generateX86MemoryReference(objectReg, fej9->getFirstArrayletPointerOffset(comp), cg), tempReg, cg);
      }

   }


// ------------------------------------------------------------------------------
// genZeroInitObject2
// ------------------------------------------------------------------------------

static bool genZeroInitObject2(
      TR::Node *node,
      int32_t objectSize,
      int32_t elementSize,
      TR::Register *sizeReg,
      TR::Register *targetReg,
      TR::Register *tempReg,
      TR::Register *segmentReg,
      TR::Register *&repzScratchReg,
      TR::CodeGenerator *cg)
   {

   bool isArrayNew = (node->getOpCodeValue() != TR::New);

   TR_OpaqueClassBlock *clazz = NULL;
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   // set up clazz value here
   comp->canAllocateInline(node, clazz);

   int32_t numSlots = 0;
   int32_t startOfZeroInits = -1;

   startOfZeroInits = isArrayNew ? sizeof(J9IndexableObjectContiguous) : sizeof(J9Object);

   if (TR::Compiler->target.is64Bit())
      {
      // round down to the nearest word size
      TR_ASSERT(startOfZeroInits < 0xF8, "expecting start of zero inits to be the size of the header");
      startOfZeroInits &= 0xF8;
      }

   numSlots = (int32_t)((objectSize - startOfZeroInits)/TR::Compiler->om.sizeofReferenceAddress());

   bool generateArraylets = comp->generateArraylets();

   int32_t i;

   if (!minRepstosdWords)
      {
      static char *p= feGetEnv("TR_MinRepstosdWords");
      if (p)
         minRepstosdWords = atoi(p);
      else
         minRepstosdWords = MIN_REPSTOSD_WORDS; // Use default value
      }

   int32_t alignmentDelta = 0; // for aligning properly to get best performance from REP STOSD/STOSQ

   if (sizeReg || (numSlots + alignmentDelta) >= minRepstosdWords)
      {
      // Zero-initialize by using REP STOSD/STOSQ.
      //

      if (sizeReg)
         {
         // -------------
         //
         // VARIABLE SIZE
         //
         // -------------

         int32_t additionalSlots = 0;

         if (generateArraylets)
            {
            additionalSlots++;
            if (elementSize > sizeof(UDATA))
               additionalSlots++;
            }

         // Subtract off the header size and initialize the remaining slots.
         //
         generateRegImmInstruction(SUBRegImm4(), node, tempReg, startOfZeroInits, cg);
         generateRegImmInstruction(SHRRegImm1(), node, tempReg, TR::Compiler->target.is64Bit()? 3 : 2, cg);

         }
      else
         {
         // ----------
         // FIXED SIZE
         // ----------

         generateRegImmInstruction(MOVRegImm4(), node, tempReg, numSlots + alignmentDelta, cg);
         if (TR::Compiler->target.is64Bit())
            {
            // TODO AMD64: replace both instructions with a LEA tempReg, [disp32]
            //
            generateRegRegInstruction(MOVSXReg8Reg4, node, tempReg, tempReg, cg);
            }
         }


      // -----------
      // Destination
      // -----------

      generateRegMemInstruction(LEARegMem(), node, segmentReg, generateX86MemoryReference(targetReg, startOfZeroInits, cg), cg);

      if (TR::Compiler->target.is64Bit())
         {
         repzScratchReg = cg->allocateRegister();
         generateRegRegInstruction(MOVRegReg(), node, repzScratchReg, targetReg, cg);
         }
      else
         {
         generateRegInstruction(PUSHReg, node, targetReg, cg);
         }

      generateRegRegInstruction(XORRegReg(), node, targetReg, targetReg, cg);

      // We just pushed targetReg on the stack and zeroed it out. targetReg contained the address of the
      // beginning of the header. We want to use the 0-reg to initialize the monitor slot, so we use
      // segmentReg, which points to to targetReg+startOfZeroInits and subtract the extra offset.
      //
      bool initLw = (node->getOpCodeValue() != TR::New);
      int lwOffset = fej9->getByteOffsetToLockword(clazz);
      initLw = false;

      if (initLw)
         {
         TR_X86OpCodes op = (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord()) ? S4MemReg : SMemReg();
         generateMemRegInstruction(op, node, generateX86MemoryReference(segmentReg, lwOffset-startOfZeroInits, cg), targetReg, cg);
         }

      TR_X86OpCodes op = TR::Compiler->target.is64Bit() ? REPSTOSQ : REPSTOSD;
      generateInstruction(op, node, cg);

      if (TR::Compiler->target.is64Bit())
         {
         generateRegRegInstruction(MOVRegReg(), node, targetReg, repzScratchReg, cg);
         }
      else
         {
         generateRegInstruction(POPReg, node, targetReg, cg);
         }

      return true;
      }

   if (numSlots > 0)
      {
      generateRegRegInstruction(XORRegReg(), node, tempReg, tempReg, cg);

      bool initLw = (node->getOpCodeValue() != TR::New);
      int lwOffset = fej9->getByteOffsetToLockword(clazz);
      initLw = false;

      if (initLw)
         {
         TR_X86OpCodes op = (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord()) ? S4MemReg : SMemReg();
         generateMemRegInstruction(op, node, generateX86MemoryReference(targetReg, lwOffset, cg), tempReg, cg);
         }
      }
   else
      {
      bool initLw = (node->getOpCodeValue() != TR::New);
      int lwOffset = fej9->getByteOffsetToLockword(clazz);
      initLw = false;

      if (initLw)
         {
         TR_X86OpCodes op = (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord()) ? S4MemImm4 : SMemImm4();
         generateMemImmInstruction(op, node, generateX86MemoryReference(targetReg, lwOffset, cg), 0, cg);
         }
      return false;
      }

   int32_t numIterations = numSlots/maxZeroInitWordsPerIteration;
   if (numIterations > 1)
      {
      // Generate the initializations in a loop
      //
      int32_t numLoopSlots = numIterations*maxZeroInitWordsPerIteration;
      int32_t endOffset;

      endOffset = (int32_t)(numLoopSlots*TR::Compiler->om.sizeofReferenceAddress() + startOfZeroInits);

      generateRegImmInstruction(MOVRegImm4(), node, segmentReg, -((numIterations-1)*maxZeroInitWordsPerIteration), cg);

      if (TR::Compiler->target.is64Bit())
         generateRegRegInstruction(MOVSXReg8Reg4, node, segmentReg, segmentReg, cg);

      TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);
      generateLabelInstruction(LABEL, node, loopLabel, cg);
      for (i = maxZeroInitWordsPerIteration; i > 0; i--)
         {
         generateMemRegInstruction(SMemReg(), node,
                                   generateX86MemoryReference(targetReg,
                                                           segmentReg,
                                                           TR::MemoryReference::convertMultiplierToStride((int32_t)TR::Compiler->om.sizeofReferenceAddress()),
                                                           endOffset - TR::Compiler->om.sizeofReferenceAddress()*i, cg),
                                   tempReg, cg);
         }
      generateRegImmInstruction(ADDRegImms(), node, segmentReg, maxZeroInitWordsPerIteration, cg);
      generateLabelInstruction(JLE4, node, loopLabel, cg);

      // Generate the left-over initializations
      //
      for (i = 0; i < numSlots % maxZeroInitWordsPerIteration; i++)
         {
         generateMemRegInstruction(SMemReg(), node,
                                   generateX86MemoryReference(targetReg,
                                                           endOffset+TR::Compiler->om.sizeofReferenceAddress()*i, cg),
                                   tempReg, cg);
         }
      }
   else
      {
      // Generate the initializations inline
      //
      for (i = 0; i < numSlots; i++)
         {
         // Don't bother initializing the array-size slot
         //
         generateMemRegInstruction(SMemReg(), node,
                                   generateX86MemoryReference(targetReg,
                                   i*TR::Compiler->om.sizeofReferenceAddress() + startOfZeroInits, cg),
                                   tempReg, cg);
         }
      }

   return false;
   }






// Generate the code to initialize the data portion of an allocated object.
// Zero-initialize the monitor slot in the header at the same time.
// If "sizeReg"  is non-null it contains the number of array elements and
// "elementSize" contains the size of each element.
// Otherwise the object size is in "objectSize".
//
static bool genZeroInitObject(
      TR::Node *node,
      int32_t objectSize,
      int32_t elementSize,
      TR::Register *sizeReg,
      TR::Register *targetReg,
      TR::Register *tempReg,
      TR::Register *segmentReg,
      TR::Register *&repzScratchReg,
      TR::CodeGenerator *cg)
   {
   // object header flags now occupy 4bytes on 64-bit
   TR::ILOpCodes opcode = node->getOpCodeValue();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR::Compilation *comp = cg->comp();

   bool isArrayNew = (opcode != TR::New) ;
   TR_OpaqueClassBlock *clazz = NULL;

   // set up clazz value here
   comp->canAllocateInline(node, clazz);

   int32_t numSlots = 0;
   int32_t startOfZeroInits = isArrayNew ? sizeof(J9IndexableObjectContiguous) : sizeof(J9Object);

   if (TR::Compiler->target.is64Bit())
      {
      // round down to the nearest word size
      TR_ASSERT(startOfZeroInits < 0xF8, "expecting start of zero inits to be the size of the header");
      startOfZeroInits &= 0xF8;
      }

   numSlots = (int32_t)((objectSize - startOfZeroInits)/TR::Compiler->om.sizeofReferenceAddress());

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

   if (!minRepstosdWords)
      {
      static char *p= feGetEnv("TR_MinRepstosdWords");
      if (p)
         minRepstosdWords = atoi(p);
      else
         minRepstosdWords = MIN_REPSTOSD_WORDS; // Use default value
      }

   int32_t alignmentDelta = 0; // for aligning properly to get best performance from REP STOSD/STOSQ

   if (sizeReg || (numSlots + alignmentDelta) >= minRepstosdWords)
      {
      // Zero-initialize by using REP STOSD/STOSQ.
      //
      // startOffset will be monitorSlot only for arrays

      generateRegMemInstruction(LEARegMem(), node, segmentReg, generateX86MemoryReference(targetReg, startOfZeroInits, cg), cg);

      if (sizeReg)
         {
         int32_t additionalSlots = 0;

         if (generateArraylets)
            {
            additionalSlots++;
            if (elementSize > sizeof(UDATA))
               additionalSlots++;
            }

         switch (elementSize)
            {
            // Calculate the number of slots by rounding up to number of words,
            // adding in partialHeaderSize.adding in partialHeaderSize.
            //
            case 1:
               if (TR::Compiler->target.is64Bit())
                  {
                  generateRegMemInstruction(LEA8RegMem, node, tempReg, generateX86MemoryReference(sizeReg, (additionalSlots*8)+7, cg), cg);
                  generateRegImmInstruction(SHR8RegImm1, node, tempReg, 3, cg);
                  }
               else
                  {
                  generateRegMemInstruction(LEA4RegMem, node, tempReg, generateX86MemoryReference(sizeReg, (additionalSlots*4)+3, cg), cg);
                  generateRegImmInstruction(SHR4RegImm1, node, tempReg, 2, cg);
                  }
               break;
            case 2:
               if (TR::Compiler->target.is64Bit())
                  {
                  generateRegMemInstruction(LEA8RegMem, node, tempReg, generateX86MemoryReference(sizeReg, (additionalSlots*4)+3, cg), cg);
                  generateRegImmInstruction(SHR8RegImm1, node, tempReg, 2, cg);
                  }
               else
                  {
                  generateRegMemInstruction(LEA4RegMem, node, tempReg, generateX86MemoryReference(sizeReg, (additionalSlots*2)+1, cg), cg);
                  generateRegImmInstruction(SHR4RegImm1, node, tempReg, 1, cg);
                  }
               break;
            case 4:
               if (TR::Compiler->target.is64Bit())
                  {
                  generateRegMemInstruction(LEA8RegMem, node, tempReg, generateX86MemoryReference(sizeReg, (additionalSlots*2)+1, cg), cg);
                  generateRegImmInstruction(SHR8RegImm1, node, tempReg, 1, cg);
                  }
               else
                  {
                  generateRegMemInstruction(LEA4RegMem, node, tempReg,
                                            generateX86MemoryReference(sizeReg, additionalSlots, cg), cg);
                  }
               break;
            case 8:
               if (TR::Compiler->target.is64Bit())
                  {
                  generateRegMemInstruction(LEA8RegMem, node, tempReg,
                                            generateX86MemoryReference(sizeReg, additionalSlots, cg), cg);
                  }
               else
                  {
                  generateRegMemInstruction(LEA4RegMem, node, tempReg,
                                            generateX86MemoryReference(NULL, sizeReg,
                                                                    TR::MemoryReference::convertMultiplierToStride(2),
                                                                    additionalSlots, cg), cg);
                  }
               break;
            }
         }
      else
         {
         // Fixed size
         //
         generateRegImmInstruction(MOVRegImm4(), node, tempReg, numSlots + alignmentDelta, cg);
         if (TR::Compiler->target.is64Bit())
            {
            // TODO AMD64: replace both instructions with a LEA tempReg, [disp32]
            //
            generateRegRegInstruction(MOVSXReg8Reg4, node, tempReg, tempReg, cg);
            }
         }

      if (TR::Compiler->target.is64Bit())
         {
         repzScratchReg = cg->allocateRegister();
         generateRegRegInstruction(MOVRegReg(), node, repzScratchReg, targetReg, cg);
         }
      else
         {
         generateRegInstruction(PUSHReg, node, targetReg, cg);
         }

      generateRegRegInstruction(XORRegReg(), node, targetReg, targetReg, cg);

      // We just pushed targetReg on the stack and zeroed it out. targetReg contained the address of the
      // beginning of the header. We want to use the 0-reg to initialize the monitor slot, so we use
      // segmentReg, which points to to targetReg+startOfZeroInits and subtract the extra offset.

      bool initLw = (node->getOpCodeValue() != TR::New);
      int lwOffset = fej9->getByteOffsetToLockword(clazz);
      initLw = false;

      if (initLw)
         {
         TR_X86OpCodes op = (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord()) ? S4MemReg : SMemReg();
         generateMemRegInstruction(op, node, generateX86MemoryReference(segmentReg, lwOffset-startOfZeroInits, cg), targetReg, cg);
         }

      TR_X86OpCodes op = TR::Compiler->target.is64Bit() ? REPSTOSQ : REPSTOSD;
      generateInstruction(op, node, cg);

      if (TR::Compiler->target.is64Bit())
         {
         generateRegRegInstruction(MOVRegReg(), node, targetReg, repzScratchReg, cg);
         }
      else
         {
         generateRegInstruction(POPReg, node, targetReg, cg);
         }

      return true;
      }

   if (numSlots > 0)
      {
      generateRegRegInstruction(XORRegReg(), node, tempReg, tempReg, cg);

      bool initLw = (node->getOpCodeValue() != TR::New);
      int lwOffset = fej9->getByteOffsetToLockword(clazz);
      initLw = false;

      if (initLw)
         {
         TR_X86OpCodes op = (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord()) ? S4MemReg : SMemReg();
         generateMemRegInstruction(op, node, generateX86MemoryReference(targetReg, lwOffset, cg), tempReg, cg);
         }
      }
   else
      {
      bool initLw = (node->getOpCodeValue() != TR::New);
      int lwOffset = fej9->getByteOffsetToLockword(clazz);
      initLw = false;

      if (initLw)
         {
         TR_X86OpCodes op = (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord()) ? S4MemImm4 : SMemImm4();
         generateMemImmInstruction(op, node, generateX86MemoryReference(targetReg, lwOffset, cg), 0, cg);
         }
      return false;
      }

   int32_t numIterations = numSlots/maxZeroInitWordsPerIteration;
   if (numIterations > 1)
      {
      // Generate the initializations in a loop
      //
      int32_t numLoopSlots = numIterations*maxZeroInitWordsPerIteration;
      int32_t endOffset;

      endOffset = (int32_t)(numLoopSlots*TR::Compiler->om.sizeofReferenceAddress() + startOfZeroInits);

      generateRegImmInstruction(MOVRegImm4(), node, segmentReg, -((numIterations-1)*maxZeroInitWordsPerIteration), cg);

      if (TR::Compiler->target.is64Bit())
         generateRegRegInstruction(MOVSXReg8Reg4, node, segmentReg, segmentReg, cg);

      TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);
      generateLabelInstruction(LABEL, node, loopLabel, cg);
      for (i = maxZeroInitWordsPerIteration; i > 0; i--)
         {
         generateMemRegInstruction(SMemReg(), node,
                                   generateX86MemoryReference(targetReg,
                                                           segmentReg,
                                                           TR::MemoryReference::convertMultiplierToStride((int32_t)TR::Compiler->om.sizeofReferenceAddress()),
                                                           endOffset - TR::Compiler->om.sizeofReferenceAddress()*i, cg),
                                   tempReg, cg);
         }
      generateRegImmInstruction(ADDRegImms(), node, segmentReg, maxZeroInitWordsPerIteration, cg);
      generateLabelInstruction(JLE4, node, loopLabel, cg);

      // Generate the left-over initializations
      //
      for (i = 0; i < numSlots % maxZeroInitWordsPerIteration; i++)
         {
         generateMemRegInstruction(SMemReg(), node,
                                   generateX86MemoryReference(targetReg,
                                                           endOffset+TR::Compiler->om.sizeofReferenceAddress()*i, cg),
                                   tempReg, cg);
         }
      }
   else
      {
      // Generate the initializations inline
      //
      for (i = 0; i < numSlots; i++)
         {
         // Don't bother initializing the array-size slot
         //
         generateMemRegInstruction(SMemReg(), node,
                                   generateX86MemoryReference(targetReg,
                                   i*TR::Compiler->om.sizeofReferenceAddress() + startOfZeroInits, cg),
                                   tempReg, cg);
         }
      }

   return false;
   }

TR::Register *
objectCloneEvaluator(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   /*
    * Commented out Object.clone() code has been removed for code cleanliness.
    * If it needs to be resurrected it can be found in RTC or CMVC.
    */
   return NULL;
   }


TR::Register *
J9::X86::TreeEvaluator::VMnewEvaluator(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   // See if inline allocation is appropriate.
   //
   // Don't do the inline allocation if we are generating JVMPI hooks, since
   // JVMPI needs to know about the allocation.
   //
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   if (comp->suppressAllocationInlining())
      return NULL;

   if (!cg->useSSEForDoublePrecision())
      {
      TR::RegisterDependencyConditions  *fpSpillDependency = generateRegisterDependencyConditions(1, 0, cg);
      fpSpillDependency->addPreCondition(NULL, TR::RealRegister::AllFPRegisters, cg);
      generateInstruction(FPREGSPILL, node, fpSpillDependency, cg);
      }

   // If the helper does not preserve all the registers there will not be
   // enough registers to do the inline allocation.
   // Also, don't do the inline allocation if optimizing for space
   //
   TR::MethodSymbol *helperSym = node->getSymbol()->castToMethodSymbol();
   if (!helperSym->preservesAllRegisters() || comp->getOption(TR_OptimizeForSpace))
      return NULL;

   TR_OpaqueClassBlock *clazz      = NULL;
   TR::Register        *classReg   = NULL;
   bool                 isArrayNew = false;

   int32_t allocationSize = 0;
   int32_t objectSize     = 0;
   int32_t elementSize    = 0;
   int32_t dataOffset     = 0;

   bool realTimeGC = comp->getOptions()->realTimeGC();
   bool generateArraylets = comp->generateArraylets();
   bool outlineNew = false;

   TR::Register *segmentReg      = NULL;
   TR::Register *tempReg         = NULL;
   TR::Register *targetReg       = NULL;
   TR::Register *sizeReg         = NULL;


   /**
    * Study of registers used in inline allocation.
    *
    * Result goes to targetReg. Unless outlinedHelperCall is used, which requires an extra register move to targetReg2.
    *    targetReg2 is needed because the result needs to be CollectedReferenceRegister, but only after object is ready.
    *
    * classReg contains the J9Class for the object to be allocated. Not always used; instead, when loadaddr is not evaluated, it
    *    is rematerialized like a constant (in which case, clazz contains the known value). When it is rematerialized, there are
    *    'interesting' AOT/HCR patching routines.
    *
    * sizeReg is used for array allocations to hold the number of elements. However...
    *    for packed variable (objectSize==0) arrays, sizeReg behaves like segmentReg should (i.e. contains size in _bytes_): elementSize
    *    is set to 1 and sizeReg is result of multiplication of real elementSize vs element count.
    *
    * segmentReg contains the size, _in bytes!_, of the object/array to be allocated. When outlining is used, it will be bound to edi.
    *    This must contain the rounding (i.e. 8-aligned, so address will always end in 0x0 or 0x8). When size cannot be known (i.e.
    *    dynamic array size) explicit assembly is generated to do rounding (allocationSize is reused to contain the header offset).
    *    After tlh-top comparison, this register is reused as a temporary register (i.e. genHeapAlloc in non-outlined path, and
    *    inside the outlined codert asm sequences). This size is not available at non-outlined zero-initialization routine and needs
    *    to be re-materialized.
    *
    */

   TR::RegisterDependencyConditions  *deps;

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

   allocationSize = objectSize;

   static long count = 0;
   if (!performTransformation(comp, "O^O <%3d> Inlining Allocation of %s [0x%p].\n", count++, node->getOpCode().getName(), node))
      return NULL;

   if (node->getOpCodeValue() == TR::New)
      {
      if (comp->getOption(TR_DisableAllocationInlining))
         return 0;

      // realtimeGC: cannot inline if object size is too big to get a size class
      if (comp->getOptions()->realTimeGC())
         {
         if ((uint32_t) objectSize > fej9->getMaxObjectSizeForSizeClass())
            return NULL;
         }

      dataOffset = sizeof(J9Object); //Not used...
      classReg = node->getFirstChild()->getRegister();
      TR_ASSERT(objectSize > 0, "assertion failure");
      }
   else
      {
      if (node->getOpCodeValue() == TR::newarray)
         {
         if (comp->getOption(TR_DisableAllocationInlining))
            return 0;

         elementSize = TR::Compiler->om.getSizeOfArrayElement(node);
         }
      else
         {
         // Must be TR::anewarray
         //
         if (comp->getOption(TR_DisableAllocationInlining))
            return 0;

         if (comp->useCompressedPointers())
            elementSize = TR::Compiler->om.sizeofReferenceField();
         else
            elementSize = (int32_t)TR::Compiler->om.sizeofReferenceAddress();

         classReg = node->getSecondChild()->getRegister();
         }

      isArrayNew = true;

      if (generateArraylets)
         {
         dataOffset = fej9->getArrayletFirstElementOffset(elementSize, comp);
         }
      else
         {
         dataOffset = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
         }
      }

   bool disableOutlinedNew = comp->getOption(TR_DisableOutlinedNew);

   if (generateArraylets)
      {
      if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "OUTLINED NEW: Disable for %s %p because outlined allocation can't deal with arraylets\n", node->getOpCode().getName(), node);
      disableOutlinedNew = true;
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "cg.new/refusedToOutline/arraylets/%s", node->getOpCode().getName()), 1, TR::DebugCounter::Undetermined);
      }
   else if (comp->getMethodHotness() > warm)
      {
      if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "OUTLINED NEW: Disable for %p because opt level is %s\n", node, comp->getHotnessName());
      disableOutlinedNew = true;
      cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "cg.new/refusedToOutline/optlevel/%s", comp->getHotnessName()), 1, TR::DebugCounter::Undetermined);
      }
   else if (objectSize !=0 && objectSize <=0x40)
      {
      if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "OUTLINED NEW: Disable for %p because allocation size is small %d\n", node, objectSize);

      disableOutlinedNew = true;
      }
   else if (comp->getDebug() &&
            comp->getOptions()->getPackedTestRegex() &&
            TR::SimpleRegex::match(comp->getOptions()->getPackedTestRegex(), "disablePackedOutlineNew"))
      {
      disableOutlinedNew = true;
      }
   else if(!comp->getOption(TR_EnableOutlinedNew) && cg->getX86ProcessorInfo().isGenuineIntel() && !cg->getX86ProcessorInfo().isIntelOldMachine())
      {
      disableOutlinedNew = true; //disable outlinednew for new machines
      }

   if (!disableOutlinedNew && performTransformation(comp, "O^O OUTLINED NEW: outlining %s %p, size %d\n", node->getOpCode().getName(), node, allocationSize))
      outlineNew = true;

   cg->setVMThreadRequired(true);

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *fallThru = generateLabelSymbol(cg);
   startLabel->setStartInternalControlFlow();
   fallThru->setEndInternalControlFlow();

#ifdef J9VM_GC_NON_ZERO_TLH
   // If we can skip zero init, and it is not outlined new, we use the new TLH
   // same logic also appears later, but we need to do this before generate the helper call
   //
   if (node->canSkipZeroInitialization() && !comp->getOption(TR_DisableDualTLH) && !comp->getOptions()->realTimeGC())
      {
      if (node->getOpCodeValue() == TR::New)
         node->setSymbolReference(comp->getSymRefTab()->findOrCreateNewObjectNoZeroInitSymbolRef(comp->getMethodSymbol()));
      else if (node->getOpCodeValue() == TR::newarray)
         node->setSymbolReference(comp->getSymRefTab()->findOrCreateNewArrayNoZeroInitSymbolRef(comp->getMethodSymbol()));
      if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "SKIPZEROINIT: for %p, change the symbol to %p ", node, node->getSymbolReference());
      }
   else
      {
      if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "NOSKIPZEROINIT: for %p,  keep symbol as %p ", node, node->getSymbolReference());
      }
#endif
   TR::LabelSymbol *failLabel = generateLabelSymbol(cg);

   segmentReg = cg->allocateRegister();

   tempReg = cg->allocateRegister();

   // If the size is variable, evaluate it into a register
   //
   if (objectSize == 0)
      {
      sizeReg = cg->evaluate(node->getFirstChild());
      allocationSize += dataOffset;
      if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "allocationSize %d dataOffset %d\n", allocationSize, dataOffset);
      }
   else
      {
      sizeReg = NULL;
      }

   generateLabelInstruction(LABEL, node, startLabel, cg);

   // Generate the heap allocation, and the snippet that will handle heap overflow.
   //
   TR_OutlinedInstructions *outlinedHelperCall = NULL;
   targetReg = cg->allocateRegister();
   outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::acall, targetReg, failLabel, fallThru, cg);
   cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);

   TR::Instruction * startInstr = comp->getAppendInstruction();

   // --------------------------------------------------------------------------------
   //
   // Do the allocation from the TLH and bump pointers.
   //
   // The address of the start of the allocated heap space will be in targetReg.
   //
   // --------------------------------------------------------------------------------

   bool canUseFastInlineAllocation =
      (!TR::Options::getCmdLineOptions()->realTimeGC() &&
       !comp->generateArraylets()) ? true : false;

   bool useRepInstruction;
   bool monitorSlotIsInitialized;
   bool skipOutlineZeroInit = false;
   TR_ExtraInfoForNew *initInfo = node->getSymbolReference()->getExtraInfo();
   if (node->canSkipZeroInitialization())
      {
      skipOutlineZeroInit = true;
      }
   else if (initInfo)
      {
      if (node->canSkipZeroInitialization())
         {
         initInfo->zeroInitSlots = NULL;
         initInfo->numZeroInitSlots = 0;
         skipOutlineZeroInit = true;
         }
      else if (initInfo->numZeroInitSlots <= 0)
         {
         skipOutlineZeroInit = true;
         }
      }

   if (skipOutlineZeroInit && !performTransformation(comp, "O^O OUTLINED NEW: skip outlined zero init on %s %p\n", cg->getDebug()->getName(node), node))
      skipOutlineZeroInit = false;

   if (skipOutlineZeroInit)
      {
      // The NoZeroInit outlined allocation causes sanity failures.  If we
      // can't do outlined allocation without zero init, just do it inline instead.
      // TODO: Figure this out and fix it.
      //
      outlineNew = false;
      if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "OUTLINED NEW: Disable for %p because we can't do it without zero-init\n", node);
      disableOutlinedNew = true;
      cg->generateDebugCounter("cg.new/refusedToOutline/noZeroInitBug", 1, TR::DebugCounter::Undetermined);
      }

   if (outlineNew)
      {
      // Detect overflow in array size and let jitNewArray JIT helper handle overflow
      //
      if (sizeReg && !node->getFirstChild()->isNonNegative())
         {
         TR::Register *elementCountReg = sizeReg;
         uintptrj_t maxObjectSize = cg->getMaxObjectSizeGuaranteedNotToOverflow();
         uintptrj_t maxObjectSizeInElements = maxObjectSize / elementSize;
         // 64 bit and max array size is larger than 0x7fffffff
         if (TR::Compiler->target.is64Bit() && !(maxObjectSizeInElements > 0 && maxObjectSizeInElements <= (uintptrj_t)INT_MAX))
            {
            generateRegImm64Instruction(MOV8RegImm64, node, tempReg, maxObjectSizeInElements, cg);
            generateRegRegInstruction(CMP8RegReg, node, elementCountReg, tempReg, cg);
            }
         else
            {
            generateRegImmInstruction(CMPRegImm4(), node, elementCountReg, (int32_t)maxObjectSizeInElements, cg);
            }
         generateLabelInstruction(JAE4, node, failLabel, cg);
         }

      if (sizeReg)
         {
         int32_t round = (elementSize < fej9->getObjectAlignmentInBytes()) ?
        		 fej9->getObjectAlignmentInBytes() : 0;
         int32_t disp = round ? (round-1) : 0;

         // Now compute size of object in bytes
         //
         generateRegMemInstruction(LEARegMem(),
                                   node,
                                   segmentReg,
                                   generateX86MemoryReference(NULL,
                                                              sizeReg,
                                                              TR::MemoryReference::convertMultiplierToStride(elementSize),
                                                              allocationSize + disp, cg), cg);
         if (round)
            {
            generateRegImmInstruction(ANDRegImms(), node, segmentReg, -round, cg);
            }
         }
      else
         {
         // make sure the allocationSize is aligned
         //
         allocationSize = (allocationSize + fej9->getObjectAlignmentInBytes()-1) & (-fej9->getObjectAlignmentInBytes());
         generateRegImmInstruction(MOV4RegImm4, node, segmentReg, allocationSize, cg);
         }

      if (skipOutlineZeroInit)
         {
         TR_RuntimeHelper helper;
         if (isArrayNew && sizeReg)
            helper = TR_X86OutlinedNewArrayNoZeroInit;
         else
            helper = TR_X86OutlinedNewNoZeroInit;
         cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "cg.new/outlined/noZeroInit/%s", node->getOpCode().getName()), 1, TR::DebugCounter::Undetermined);
         generateHelperCallInstruction(node, helper, NULL, cg)->setAdjustsFramePointerBy(0);
         }
      else
         {
         TR_RuntimeHelper helper;
         if (isArrayNew && sizeReg)
            helper = TR_X86OutlinedNewArray;
         else
            helper = TR_X86OutlinedNew;
         cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "cg.new/outlined/zeroInit/%s", node->getOpCode().getName()), 1, TR::DebugCounter::Undetermined);
         generateHelperCallInstruction(node, helper, NULL, cg)->setAdjustsFramePointerBy(0);
         }
      generateRegRegInstruction(TESTRegReg(), node, targetReg, targetReg, cg);
      generateLabelInstruction(JE4, node, failLabel, cg);
      }
   else
      {
      // Faster inlined sequence.  It does not understand arraylet shapes yet.
      //
      if (canUseFastInlineAllocation)
         {
         genHeapAlloc2(node, clazz, allocationSize, elementSize, sizeReg, targetReg, segmentReg, tempReg, failLabel, cg);
         }
      else
         {
         genHeapAlloc(node, clazz, allocationSize, elementSize, sizeReg, targetReg, segmentReg, tempReg, failLabel, cg);
         }
      }

   // --------------------------------------------------------------------------------
   //
   // Perform zero-initialization on data slots.
   //
   // There may be information about which slots are to be zero-initialized.
   // If there is no information, all slots must be zero-initialized.
   //
   // --------------------------------------------------------------------------------

   TR::Register *repzScratchReg = NULL;

#ifdef J9VM_GC_NON_ZERO_TLH
   if (comp->getOption(TR_DisableDualTLH) || comp->getOptions()->realTimeGC())
      {
#endif
      if (!maxZeroInitWordsPerIteration)
         {
         static char *p = feGetEnv("TR_MaxZeroInitWordsPerIteration");
         if (p)
            maxZeroInitWordsPerIteration = atoi(p);
         else
            maxZeroInitWordsPerIteration = MAX_ZERO_INIT_WORDS_PER_ITERATION; // Use default value
         }

      if (initInfo && initInfo->zeroInitSlots && !outlineNew)
         {
         // If there are too many words to be individually initialized, initialize
         // them all
         //
         if (initInfo->numZeroInitSlots >= maxZeroInitWordsPerIteration*2-1)
            initInfo->zeroInitSlots = NULL;
         }

      if (initInfo && initInfo->zeroInitSlots)
         {
         // Zero-initialize by explicit zero stores.
         // Use the supplied bit vector to identify which slots to initialize
         //
         // Zero-initialize the monitor slot in the header at the same time.
         //
         generateRegRegInstruction(XORRegReg(), node, tempReg, tempReg, cg);
         bool gen64BitInstr = TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord();

         bool initLw = (node->getOpCodeValue() != TR::New);
         int32_t lwOffset = fej9->getByteOffsetToLockword(clazz);
         initLw = false;

         if (initLw)
            generateMemRegInstruction(SMemReg(gen64BitInstr), node,
                                      generateX86MemoryReference(targetReg, lwOffset, cg),
                                      tempReg, cg);

         TR_BitVectorIterator bvi(*initInfo->zeroInitSlots);
         while (bvi.hasMoreElements())
            {
            generateMemRegInstruction(S4MemReg, node,
                                      generateX86MemoryReference(targetReg, bvi.getNextElement()*4 +dataOffset, cg),
                                      tempReg, cg);
            }
         useRepInstruction = false;

         J9JavaVM * jvm = fej9->getJ9JITConfig()->javaVM;
         if (jvm->lockwordMode == LOCKNURSERY_ALGORITHM_ALL_INHERIT)
            monitorSlotIsInitialized = false;
         else
            monitorSlotIsInitialized = true;
         }
      else if ((!initInfo || initInfo->numZeroInitSlots > 0) &&
               !node->canSkipZeroInitialization() && !outlineNew)
         {
         // Initialize all slots
         //
         if (canUseFastInlineAllocation)
            {
            useRepInstruction = genZeroInitObject2(node, objectSize, elementSize, sizeReg, targetReg, tempReg, segmentReg, repzScratchReg, cg);
            }
         else
            {
            useRepInstruction = genZeroInitObject(node, objectSize, elementSize, sizeReg, targetReg, tempReg, segmentReg, repzScratchReg, cg);
            }

         J9JavaVM * jvm = fej9->getJ9JITConfig()->javaVM;
         if (jvm->lockwordMode == LOCKNURSERY_ALGORITHM_ALL_INHERIT)
            monitorSlotIsInitialized = false;
         else
            monitorSlotIsInitialized = true;
         }
      else
         {
         // Skip data initialization
         //
         if (canUseFastInlineAllocation)
            {
            // Even though we can skip the data initialization, for arrays of unknown size we still have
            // to initialize at least one slot to cover the discontiguous length field in case the array
            // is zero sized.  This is because the length is not checked at runtime and is only needed
            // for non-native 64-bit targets where the discontiguous length slot is already initialized
            // via the contiguous length slot.
            //
            if (node->getOpCodeValue() != TR::New && !outlineNew &&
                (TR::Compiler->target.is32Bit() || comp->useCompressedPointers()))
               {
               generateMemImmInstruction(SMemImm4(), node,
                  generateX86MemoryReference(targetReg, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg),
                  0, cg);
               }
            }

         monitorSlotIsInitialized = false;
         useRepInstruction = false;
         }
#ifdef J9VM_GC_NON_ZERO_TLH
      }
   else
      {
      monitorSlotIsInitialized = false;
      useRepInstruction = false;
      }
#endif

   // --------------------------------------------------------------------------------
   // Initialize the header
   // --------------------------------------------------------------------------------
   if (fej9->inlinedAllocationsMustBeVerified() && node->getOpCodeValue() == TR::anewarray)
      {
      genInitArrayHeader(
            node,
            clazz,
            classReg,
            targetReg,
            sizeReg,
            elementSize,
            dataOffset,
            tempReg,
            monitorSlotIsInitialized,
            cg);
      }
   else if (isArrayNew)
      {
      genInitArrayHeader(
            node,
            clazz,
            NULL,
            targetReg,
            sizeReg,
            elementSize,
            dataOffset,
            tempReg,
            monitorSlotIsInitialized,
            cg);
      }
   else
      {
      genInitObjectHeader(node, clazz, classReg, targetReg, tempReg, monitorSlotIsInitialized, cg);
      }

   cg->setVMThreadRequired(false);

   if (fej9->inlinedAllocationsMustBeVerified() && (node->getOpCodeValue() == TR::New ||
                                                        node->getOpCodeValue() == TR::anewarray) )
      {
      startInstr = startInstr->getNext();
      TR_RelocationRecordInformation *recordInfo =
         (TR_RelocationRecordInformation *) comp->trMemory()->allocateMemory(sizeof(TR_RelocationRecordInformation), heapAlloc);
      recordInfo->data1 = allocationSize;
      recordInfo->data2 = node->getInlinedSiteIndex();
      recordInfo->data3 = (uintptr_t) failLabel;
      recordInfo->data4 = (uintptr_t) startInstr;
      TR::SymbolReference * classSymRef;
      TR_ExternalRelocationTargetKind reloKind;

      if (node->getOpCodeValue() == TR::New)
         {
         classSymRef = node->getFirstChild()->getSymbolReference();
         reloKind = TR_VerifyClassObjectForAlloc;
         }
      else
         {
         classSymRef = node->getSecondChild()->getSymbolReference();
         reloKind = TR_VerifyRefArrayForAlloc;
         }

      cg->addAOTRelocation(new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(startInstr,
                                                                               (uint8_t *) classSymRef,
                                                                               (uint8_t *) recordInfo,
                                                                               reloKind, cg),
                           __FILE__, __LINE__, node);
      }

   int32_t numDeps = 4;
   if (classReg)
      numDeps += 2;
   if (sizeReg)
      numDeps += 2;

   if (repzScratchReg)
      numDeps++;

   if (outlinedHelperCall)
      {
      if (node->getOpCodeValue() == TR::New)
         numDeps++;
      else
         numDeps += 2;
      }

   if (outlineNew)
      {
      numDeps++;
      }

   // Create dependencies for the allocation registers here.
   // The size and class registers, if they exist, must be the first
   // dependencies since the heap allocation snippet needs to find them to grab
   // the real registers from them.
   //
   deps = generateRegisterDependencyConditions((uint8_t)0, numDeps, cg);

   if (sizeReg)
      deps->addPostCondition(sizeReg, TR::RealRegister::NoReg, cg);
   if (classReg)
      deps->addPostCondition(classReg, TR::RealRegister::NoReg, cg);

   deps->addPostCondition(targetReg, TR::RealRegister::eax, cg);
   deps->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);

   if (useRepInstruction || outlineNew)
      {
      deps->addPostCondition(tempReg, TR::RealRegister::ecx, cg);
      deps->addPostCondition(segmentReg, TR::RealRegister::edi, cg);
      }
   else
      {
      deps->addPostCondition(tempReg, TR::RealRegister::NoReg, cg);
      if (segmentReg)
         deps->addPostCondition(segmentReg, TR::RealRegister::NoReg, cg);
      }

   if (repzScratchReg)
      {
      deps->addPostCondition(repzScratchReg, TR::RealRegister::NoReg, cg);
      cg->stopUsingRegister(repzScratchReg);
      }

   if (outlineNew)
      {
      TR::Register *dummyReg = cg->allocateRegister();
      deps->addPostCondition(dummyReg, TR::RealRegister::esi, cg);
      cg->stopUsingRegister(dummyReg);
      }

   if (outlinedHelperCall)
      {
      TR::Node *callNode = outlinedHelperCall->getCallNode();
      TR::Register *reg;

      if (callNode->getFirstChild() == node->getFirstChild())
         if (reg = callNode->getFirstChild()->getRegister())
            deps->unionPostCondition(reg, TR::RealRegister::NoReg, cg);

      if (node->getOpCodeValue() != TR::New)
         if (callNode->getSecondChild() == node->getSecondChild())
            if (reg = callNode->getSecondChild()->getRegister())
               deps->unionPostCondition(reg, TR::RealRegister::NoReg, cg);
      }

   deps->stopAddingConditions();

   generateLabelInstruction(LABEL, node, fallThru, deps, cg);

   if (outlinedHelperCall) // 64bit or TR_newstructRef||TR_anewarraystructRef
      {
      // Copy the newly allocated object into a collected reference register now that it is a valid object.
      //
      TR::Register *targetReg2 = cg->allocateCollectedReferenceRegister();
      TR::RegisterDependencyConditions  *deps2 = generateRegisterDependencyConditions(0, 1, cg);
      deps2->addPostCondition(targetReg2, TR::RealRegister::eax, cg);
      generateRegRegInstruction(MOVRegReg(), node, targetReg2, targetReg, deps2, cg);
      cg->stopUsingRegister(targetReg);
      targetReg = targetReg2;
      }

   cg->stopUsingRegister(segmentReg);
   cg->stopUsingRegister(tempReg);

   // Decrement use counts on the children
   //
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
void
J9::X86::TreeEvaluator::VMarrayStoreCHKEvaluator(
      TR::Node *node,
      TR::Node *sourceChild,
      TR::Node *destinationChild,
      TR_X86ScratchRegisterManager *scratchRegisterManager,
      TR::LabelSymbol *wrtbarLabel,
      TR::Instruction *prevInstr,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR::Register *sourceReg  = sourceChild->getRegister();
   TR::Register *destReg = destinationChild->getRegister();
   TR::LabelSymbol *helperCallLabel = generateLabelSymbol(cg);

   static char *disableArrayStoreCheckOpts = feGetEnv("TR_disableArrayStoreCheckOpts");
   if (!disableArrayStoreCheckOpts || !debug("noInlinedArrayStoreCHKs"))
      {
      // If the component type of the array is equal to the type of the source reference,
      // then the store always succeeds. The component type of the array is stored in a
      // field of the J9ArrayClass that represents the type of the array.
      //

      TR::Register *sourceClassReg = scratchRegisterManager->findOrCreateScratchRegister();
      TR::Register *destComponentClassReg = scratchRegisterManager->findOrCreateScratchRegister();

      TR::Instruction* instr;

#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
      // FIXME: Add check for hint when doing the arraystore check as below when class pointer compression
      // is enabled.

      TR::MemoryReference *destTypeMR = generateX86MemoryReference(destReg, TR::Compiler->om.offsetOfObjectVftField(), cg);

      generateRegMemInstruction(L4RegMem, node, destComponentClassReg, destTypeMR, cg); // class pointer is 32 bits
      TR::TreeEvaluator::generateVFTMaskInstruction(node, destComponentClassReg, cg);

      // -------------------------------------------------------------------------
      //
      // If the component type is java.lang.Object then the store always succeeds.
      //
      // -------------------------------------------------------------------------

      TR_OpaqueClassBlock *objectClass = fej9->getSystemClassFromClassName("java/lang/Object", 16);

      if (TR::Compiler->target.is64Bit())
         {
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
         TR_ASSERT((((uintptr_t)objectClass) >> 32) == 0, "TR_OpaqueClassBlock must fit on 32 bits when using class pointer compression");
         instr = generateRegImmInstruction(CMP4RegImm4, node, destComponentClassReg, (uint32_t) ((uint64_t) objectClass), cg);

#else // 64 bit but no class pointer compression

         if ((uintptrj_t)objectClass <= (uintptrj_t)0x7fffffff)
            {
            instr = generateRegImmInstruction(CMP8RegImm4, node, destComponentClassReg, (uintptr_t) objectClass, cg);
            }
         else
            {
            TR::Register *objectClassReg = scratchRegisterManager->findOrCreateScratchRegister();
            instr = generateRegImm64Instruction(MOV8RegImm64, node, objectClassReg, (uintptr_t) objectClass, cg);
            generateRegRegInstruction(CMP8RegReg, node, destComponentClassReg, objectClassReg, cg);
            scratchRegisterManager->reclaimScratchRegister(objectClassReg);
            }
#endif
         }
      else
         {
         instr = generateRegImmInstruction(CMP4RegImm4, node, destComponentClassReg, (int32_t)(uintptr_t) objectClass, cg);
         }

      generateLabelInstruction(JE4, node, wrtbarLabel, cg);

      // HCR in VMarrayStoreCHKEvaluator
      if (cg->wantToPatchClassPointer(objectClass, node))
         comp->getStaticHCRPICSites()->push_front(instr);

      // here we may have to convert the TR_OpaqueClassBlock into a J9Class pointer
      // and store it in destComponentClassReg
      // ..

      TR::MemoryReference *destCompTypeMR =
         generateX86MemoryReference(destComponentClassReg, offsetof(J9ArrayClass, componentType), cg);
      generateRegMemInstruction(LRegMem(), node, destComponentClassReg, destCompTypeMR, cg);

      // here we may have to convert the J9Class pointer from destComponentClassReg into
      // a TR_OpaqueClassBlock and store it back into destComponentClassReg
      // ..

      TR::MemoryReference *sourceRegClassMR = generateX86MemoryReference(sourceReg, TR::Compiler->om.offsetOfObjectVftField(), cg);
      generateRegMemInstruction(L4RegMem, node, sourceClassReg, sourceRegClassMR, cg);
      TR::TreeEvaluator::generateVFTMaskInstruction(node, sourceClassReg, cg);

      generateRegRegInstruction(CMP4RegReg, node, destComponentClassReg, sourceClassReg, cg); // compare only 32 bits
      generateLabelInstruction(JE4, node, wrtbarLabel, cg);

      // -------------------------------------------------------------------------
           //
           // Check the source class cast cache
           //
           // -------------------------------------------------------------------------

           generateMemRegInstruction(
              CMP4MemReg,
              node,
              generateX86MemoryReference(sourceClassReg, offsetof(J9Class, castClassCache), cg), destComponentClassReg, cg);


#else // no class pointer compression
      TR::MemoryReference *sourceClassMR = generateX86MemoryReference(sourceReg, TR::Compiler->om.offsetOfObjectVftField(), cg);
      generateRegMemInstruction(LRegMem(), node, sourceClassReg, sourceClassMR, cg);
      TR::TreeEvaluator::generateVFTMaskInstruction(node, sourceClassReg, cg);

      TR::MemoryReference *destClassMR = generateX86MemoryReference(destReg, TR::Compiler->om.offsetOfObjectVftField(), cg);
      generateRegMemInstruction(LRegMem(), node, destComponentClassReg, destClassMR, cg);
      TR::TreeEvaluator::generateVFTMaskInstruction(node, destComponentClassReg, cg);
      TR::MemoryReference *destCompTypeMR =
         generateX86MemoryReference(destComponentClassReg, offsetof(J9ArrayClass, componentType), cg);
      generateRegMemInstruction(LRegMem(), node, destComponentClassReg, destCompTypeMR, cg);

      generateRegRegInstruction(CMPRegReg(), node, destComponentClassReg, sourceClassReg, cg);
      generateLabelInstruction(JE4, node, wrtbarLabel, cg);

      // -------------------------------------------------------------------------
      //
      // Check the source class cast cache
      //
      // -------------------------------------------------------------------------

      generateMemRegInstruction(
         CMPMemReg(),
         node,
         generateX86MemoryReference(sourceClassReg, offsetof(J9Class, castClassCache), cg), destComponentClassReg, cg);

#endif
      generateLabelInstruction(JE4, node, wrtbarLabel, cg);

      instr = NULL;
      /*
      TR::Instruction *instr;


      // -------------------------------------------------------------------------
      //
      // If the component type is java.lang.Object then the store always succeeds.
      //
      // -------------------------------------------------------------------------

      TR_OpaqueClassBlock *objectClass = fej9->getSystemClassFromClassName("java/lang/Object", 16);

      if (TR::Compiler->target.is64Bit())
         {
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
         TR_ASSERT((((uintptr_t)objectClass) >> 32) == 0, "TR_OpaqueClassBlock must fit on 32 bits when using class pointer compression");
         instr = generateRegImmInstruction(CMP4RegImm4, node, destComponentClassReg, (uint32_t) ((uint64_t) objectClass), cg);

#else // 64 bit but no class pointer compression

         if ((uintptrj_t)objectClass <= (uintptrj_t)0x7fffffff)
            {
            instr = generateRegImmInstruction(CMP8RegImm4, node, destComponentClassReg, (uintptr_t) objectClass, cg);
            }
         else
            {
            TR::Register *objectClassReg = scratchRegisterManager->findOrCreateScratchRegister();
            instr = generateRegImm64Instruction(MOV8RegImm64, node, objectClassReg, (uintptr_t) objectClass, cg);
            generateRegRegInstruction(CMP8RegReg, node, destComponentClassReg, objectClassReg, cg);
            scratchRegisterManager->reclaimScratchRegister(objectClassReg);
            }
#endif
         }
      else
         {
         instr = generateRegImmInstruction(CMP4RegImm4, node, destComponentClassReg, (int32_t)(uintptr_t) objectClass, cg);
         }

      generateLabelInstruction(JE4, node, wrtbarLabel, cg);

      // HCR in VMarrayStoreCHKEvaluator
      if (cg->wantToPatchClassPointer(objectClass, node))
         comp->getStaticHCRPICSites()->push_front(instr);
      */


      // ---------------------------------------------
      //
      // If isInstanceOf (objectClass,ArrayComponentClass,true,true) was successful and stored during VP, we need to test again the real arrayComponentClass
      // Need to relocate address of arrayComponentClass under aot sharedcache
      // Need to possibility of class unloading.
      // --------------------------------------------


      if (!(TR::Options::getCmdLineOptions()->getOption(TR_DisableArrayStoreCheckOpts)) && node->getArrayComponentClassInNode() )
         {
         TR_OpaqueClassBlock *arrayComponentClass = (TR_OpaqueClassBlock *) node->getArrayComponentClassInNode();
         if (TR::Compiler->target.is64Bit())
            {
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
            TR_ASSERT((((uintptr_t)arrayComponentClass) >> 32) == 0, "TR_OpaqueClassBlock must fit on 32 bits when using class pointer compression");
            instr = generateRegImmInstruction(CMP4RegImm4, node, destComponentClassReg, (uint32_t) ((uint64_t) arrayComponentClass), cg);

            if (fej9->isUnloadAssumptionRequired(arrayComponentClass, comp->getCurrentMethod()))
                         comp->getStaticPICSites()->push_front(instr);


#else // 64 bit but no class pointer compression
            if ((uintptrj_t)arrayComponentClass <= (uintptrj_t)0x7fffffff)
               {
               instr = generateRegImmInstruction(CMP8RegImm4, node, destComponentClassReg, (uintptr_t) arrayComponentClass, cg);
               if (fej9->isUnloadAssumptionRequired(arrayComponentClass, comp->getCurrentMethod()))
                  comp->getStaticPICSites()->push_front(instr);

               }
            else
               {
               TR::Register *arrayComponentClassReg = scratchRegisterManager->findOrCreateScratchRegister();
               instr = generateRegImm64Instruction(MOV8RegImm64, node, arrayComponentClassReg, (uintptr_t) arrayComponentClass, cg);
               generateRegRegInstruction(CMP8RegReg, node, destComponentClassReg, arrayComponentClassReg, cg);
               scratchRegisterManager->reclaimScratchRegister(arrayComponentClassReg);
               }
#endif
            }
         else
            {
            instr = generateRegImmInstruction(CMP4RegImm4, node, destComponentClassReg, (int32_t)(uintptr_t) arrayComponentClass, cg);
            if (fej9->isUnloadAssumptionRequired(arrayComponentClass, comp->getCurrentMethod()))
               comp->getStaticPICSites()->push_front(instr);

            }

         generateLabelInstruction(JE4, node, wrtbarLabel, cg);

         // HCR in VMarrayStoreCHKEvaluator
         if (cg->wantToPatchClassPointer(arrayComponentClass, node))
            comp->getStaticHCRPICSites()->push_front(instr);

         }




#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
   // destComponentClassReg contains the class offset so we may need to generate code
   // to convert from class offset to real J9Class pointer
#endif

      // -------------------------------------------------------------------------
      //
      // Compare source and dest class depths
      //
      // -------------------------------------------------------------------------

      // Get the depth of array component type in testerReg
      //
      bool eliminateDepthMask = (J9_JAVA_CLASS_DEPTH_MASK == 0xffff);
      TR::MemoryReference *destComponentClassDepthMR =
         generateX86MemoryReference(destComponentClassReg, offsetof(J9Class,classDepthAndFlags), cg);

      // DMDM  32-bit only???
      if (TR::Compiler->target.is32Bit())
         {
         scratchRegisterManager->reclaimScratchRegister(destComponentClassReg);
         }

      TR::Register *destComponentClassDepthReg = scratchRegisterManager->findOrCreateScratchRegister();

      if (eliminateDepthMask)
         {
         if (TR::Compiler->target.is64Bit())
            generateRegMemInstruction(MOVZXReg8Mem2, node, destComponentClassDepthReg, destComponentClassDepthMR, cg);
         else
            generateRegMemInstruction(MOVZXReg4Mem2, node, destComponentClassDepthReg, destComponentClassDepthMR, cg);
         }
      else
         {
         generateRegMemInstruction(
            LRegMem(),
            node,
            destComponentClassDepthReg,
            destComponentClassDepthMR, cg);
         }

      if (!eliminateDepthMask)
         {
         if (TR::Compiler->target.is64Bit())
            {
            TR_ASSERT(!(J9_JAVA_CLASS_DEPTH_MASK & 0x80000000), "AMD64: need to use a second register for AND mask");
            if (!(J9_JAVA_CLASS_DEPTH_MASK & 0x80000000))
               generateRegImmInstruction(AND8RegImm4, node, destComponentClassDepthReg, J9_JAVA_CLASS_DEPTH_MASK, cg);
            }
         else
            {
            generateRegImmInstruction(AND4RegImm4, node, destComponentClassDepthReg, J9_JAVA_CLASS_DEPTH_MASK, cg);
            }
         }

#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
   // temp2 contains the class offset so we may need to generate code
   // to convert from class offset to real J9Class pointer
#endif

      // Get the depth of type of object being stored into the array in testerReg2
      //

      TR::MemoryReference *mr = generateX86MemoryReference(sourceClassReg, offsetof(J9Class,classDepthAndFlags), cg);

      // There aren't enough registers available on 32-bit across this internal
      // control flow region.  Give one back and manually and force the source
      // class to be rematerialized later.
      //
      if (TR::Compiler->target.is32Bit())
         {
         scratchRegisterManager->reclaimScratchRegister(sourceClassReg);
         }

       TR::Register *sourceClassDepthReg = NULL;
       if (eliminateDepthMask)
         {
         generateMemRegInstruction(CMP2MemReg, node, mr, destComponentClassDepthReg, cg);
         }
       else
         {
         sourceClassDepthReg = scratchRegisterManager->findOrCreateScratchRegister();
         generateRegMemInstruction(
            LRegMem(),
            node,
            sourceClassDepthReg,
            mr, cg);

         if (TR::Compiler->target.is64Bit())
            {
            TR_ASSERT(!(J9_JAVA_CLASS_DEPTH_MASK & 0x80000000), "AMD64: need to use a second register for AND mask");
            if (!(J9_JAVA_CLASS_DEPTH_MASK & 0x80000000))
               generateRegImmInstruction(AND8RegImm4, node, sourceClassDepthReg, J9_JAVA_CLASS_DEPTH_MASK, cg);
            }
         else
            {
            generateRegImmInstruction(AND4RegImm4, node, sourceClassDepthReg, J9_JAVA_CLASS_DEPTH_MASK, cg);
            }
         generateRegRegInstruction(CMP4RegReg, node, sourceClassDepthReg, destComponentClassDepthReg, cg);
         }

      /*TR::Register *sourceClassDepthReg = scratchRegisterManager->findOrCreateScratchRegister();
      generateRegMemInstruction(
         LRegMem(),
         node,
         sourceClassDepthReg,
         mr, cg);

      if (TR::Compiler->target.is64Bit())
         {
         TR_ASSERT(!(J9_JAVA_CLASS_DEPTH_MASK & 0x80000000), "AMD64: need to use a second register for AND mask");
         if (!(J9_JAVA_CLASS_DEPTH_MASK & 0x80000000))
            generateRegImmInstruction(AND8RegImm4, node, sourceClassDepthReg, J9_JAVA_CLASS_DEPTH_MASK, cg);
         }
      else
         {
         generateRegImmInstruction(AND4RegImm4, node, sourceClassDepthReg, J9_JAVA_CLASS_DEPTH_MASK, cg);
         }

      generateRegRegInstruction(CMP4RegReg, node, sourceClassDepthReg, destComponentClassDepthReg, cg);*/

      generateLabelInstruction(JBE4, node, helperCallLabel, cg);
      if (sourceClassDepthReg != NULL)
         scratchRegisterManager->reclaimScratchRegister(sourceClassDepthReg);


#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
   // destComponentClassReg contains the class offset so we may need to generate code
   // to convert from class offset to real J9Class pointer
#endif

      if (TR::Compiler->target.is32Bit())
         {
         // Rematerialize the source class.
         //
         sourceClassReg = scratchRegisterManager->findOrCreateScratchRegister();
         TR::MemoryReference *sourceClassMR = generateX86MemoryReference(sourceReg, TR::Compiler->om.offsetOfObjectVftField(), cg);
         generateRegMemInstruction(LRegMem(), node, sourceClassReg, sourceClassMR, cg);
	 TR::TreeEvaluator::generateVFTMaskInstruction(node, sourceClassReg, cg);
         }

      TR::MemoryReference *tempMR = generateX86MemoryReference(sourceClassReg, offsetof(J9Class,superclasses), cg);

      if (TR::Compiler->target.is32Bit())
         {
         scratchRegisterManager->reclaimScratchRegister(sourceClassReg);
         }

      TR::Register *sourceSuperClassReg = scratchRegisterManager->findOrCreateScratchRegister();

      generateRegMemInstruction(
         LRegMem(),
         node,
         sourceSuperClassReg,
         tempMR,
         cg);

      TR::MemoryReference *leaMR =
         generateX86MemoryReference(sourceSuperClassReg, destComponentClassDepthReg, logBase2(sizeof(uintptrj_t)), 0, cg);

#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
      // leaMR is a memory reference to a J9Class
      // destComponentClassReg contains a TR_OpaqueClassBlock
      // We may need to convert superClass to a class offset before doing the comparison
#endif

      if (TR::Compiler->target.is32Bit())
         {

#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
         TR_ASSERT(0, "Unexpected 32-bit ArrayStoreCHK path");
#endif

         generateRegMemInstruction(LRegMem(), node, sourceSuperClassReg, leaMR, cg);

         // Rematerialize destination component class
         //
         TR::MemoryReference *destClassMR = generateX86MemoryReference(destReg, TR::Compiler->om.offsetOfObjectVftField(), cg);

         generateRegMemInstruction(LRegMem(), node, destComponentClassReg, destClassMR, cg);
	 TR::TreeEvaluator::generateVFTMaskInstruction(node, destComponentClassReg, cg);
         TR::MemoryReference *destCompTypeMR =
            generateX86MemoryReference(destComponentClassReg, offsetof(J9ArrayClass, componentType), cg);

         generateMemRegInstruction(CMPMemReg(), node, destCompTypeMR, sourceSuperClassReg, cg);
         }
      else
         {
         generateRegMemInstruction(CMP4RegMem, node, destComponentClassReg, leaMR, cg);
         }

      scratchRegisterManager->reclaimScratchRegister(destComponentClassReg);
      scratchRegisterManager->reclaimScratchRegister(destComponentClassDepthReg);
      scratchRegisterManager->reclaimScratchRegister(sourceClassReg);
      scratchRegisterManager->reclaimScratchRegister(sourceSuperClassReg);

      generateLabelInstruction(JE4, node, wrtbarLabel, cg);
      }

   // The fast paths failed; execute the type-check helper call.
   //
   TR::LabelSymbol* helperReturnLabel = generateLabelSymbol(cg);
   TR::Node *helperCallNode = TR::Node::createWithSymRef(TR::call, 2, 2, sourceChild, destinationChild, node->getSymbolReference());
   helperCallNode->copyByteCodeInfo(node);
   generateLabelInstruction(JMP4, helperCallNode, helperCallLabel, cg);
   TR_OutlinedInstructions* outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(helperCallNode, TR::call, NULL, helperCallLabel, helperReturnLabel, cg);
   cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);
   generateLabelInstruction(LABEL, helperCallNode, helperReturnLabel, cg);
   // TODO: This is gross and should be cleaned up.
   //
   if (TR::Compiler->target.is32Bit() && !cg->useSSEForDoublePrecision())
      fixupHelperCall(true, prevInstr);
   cg->decReferenceCount(sourceChild);
   cg->decReferenceCount(destinationChild);
   }

TR::Register *J9::X86::TreeEvaluator::VMarrayStoreCheckArrayCopyEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_ASSERT(node->getNumChildren() == 5 && !node->isNoArrayStoreCheckArrayCopy(),
      "Node %s is unsuitable for VMarrayStoreCheckArrayCopyEvaluator", cg->getDebug()->getName(node));

   TR::Node *srcNode     = node->getChild(0);
   TR::Node *dstNode     = node->getChild(1);
   TR::Node *byteSrcNode = node->getChild(2);
   TR::Node *byteDstNode = node->getChild(3);
   TR::Node *byteLenNode = node->getChild(4);

   // See if we can skip the bytes -> elements conversion
   //
   TR::Node *elementLenNode = NULL;
   bool     skippedEvaluatingByteLenNode = false;
   TR::Compilation *comp = cg->comp();
   if (byteLenNode &&
       byteLenNode->getRegister() == NULL &&
       byteLenNode->getOpCode().isMul() &&
       byteLenNode->getSecondChild()->getOpCode().isLoadConst() &&
       TR::TreeEvaluator::integerConstNodeValue(byteLenNode->getSecondChild(), cg) == TR::Compiler->om.sizeofReferenceField())//TR::Compiler->om.sizeofReferenceAddress())
      {
      skippedEvaluatingByteLenNode = true;
      elementLenNode = byteLenNode->getFirstChild();
      }
   else
      {
      TR::ILOpCodes xushr = TR::Compiler->target.is64Bit()? TR::lushr : TR::iushr;
      int32_t shftSize = 2;
      if (TR::Compiler->target.is64Bit() && !comp->useCompressedPointers()) shftSize = 3;
      elementLenNode = TR::Node::create(xushr, 2, byteLenNode,
         TR::Node::create(node, TR::iconst, 0, shftSize));
      cg->decReferenceCount(byteLenNode);
      }

   // Create helper call node
   //
   TR::SymbolReference *helperSymRef = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_referenceArrayCopy, false, false, false);

   TR::Node *callNode =
      TR::Node::createWithSymRef(TR::icall, 6, 1,
         TR::Node::createWithSymRef(node, TR::loadaddr, 0,
             new (cg->trHeapMemory()) TR::SymbolReference(cg->getSymRefTab(), TR::RegisterMappedSymbol::createMethodMetaDataSymbol(cg->trHeapMemory(), "vmThread"))),
             helperSymRef);

   // icall's args 1-4 are same as arraycopy's args 0-3
   for (uint16_t i=1; i<5; i++)
      {
      callNode->setChild(i, node->getChild(i-1));
      }
   callNode->setAndIncChild(5, elementLenNode);
   callNode->incReferenceCount();
   if (skippedEvaluatingByteLenNode)
      cg->recursivelyDecReferenceCount(byteLenNode);

   // Generate the call
   //
   TR::Register *checkFailureIndexReg = TR::TreeEvaluator::performCall(callNode, false, false, cg);

   // Must throw an array store exception if helper returned anything other than -1
   //
   TR::Instruction *instr;
   TR::Snippet *snippet;
   TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);

   generateRegImmInstruction(CMP4RegImm4, node, checkFailureIndexReg, (uint32_t)-1, cg);
   cg->decReferenceCount(callNode);
   bool rematerializeVMThreadInSnippet = (cg->allowVMThreadRematerialization() && cg->getSupportsVMThreadGRA()) ? true : false;
   instr = generateLabelInstruction (JNE4, node, snippetLabel, !rematerializeVMThreadInSnippet, cg);
   snippet = new (cg->trHeapMemory()) TR::X86CheckFailureSnippet(cg,
      comp->getSymRefTab()->findOrCreateArrayStoreExceptionSymbolRef(comp->getJittedMethodSymbol()),
      snippetLabel, instr, false, rematerializeVMThreadInSnippet);
   cg->addSnippet(snippet);

   return NULL;
   }


// Check that two objects are compatible for use in an arraycopy operation.
// If not, an ArrayStoreException is thrown.
//
TR::Register *J9::X86::TreeEvaluator::VMarrayCheckEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   bool use64BitClasses = TR::Compiler->target.is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders();

   TR::Node     *object1    = node->getFirstChild();
   TR::Node     *object2    = node->getSecondChild();
   TR::Register *object1Reg = cg->evaluate(object1);
   TR::Register *object2Reg = cg->evaluate(object2);

   TR::LabelSymbol *startLabel   = generateLabelSymbol(cg);
   TR::LabelSymbol *fallThrough  = generateLabelSymbol(cg);
   TR::Instruction *instr;
   TR::LabelSymbol *snippetLabel = NULL;
   TR::Snippet     *snippet      = NULL;
   TR::Register    *tempReg      = cg->allocateRegister();

   startLabel->setStartInternalControlFlow();
   fallThrough->setEndInternalControlFlow();
   generateLabelInstruction(LABEL, node, startLabel, cg);

   // If the objects are the same and one of them is known to be an array, they
   // are compatible.
   //
   if (node->isArrayChkPrimitiveArray1() ||
       node->isArrayChkReferenceArray1() ||
       node->isArrayChkPrimitiveArray2() ||
       node->isArrayChkReferenceArray2())
      {
      generateRegRegInstruction(CMPRegReg(), node, object1Reg, object2Reg, cg);
      generateLabelInstruction(JE4, node, fallThrough, cg);
      }

   else
      {
      // Neither object is known to be an array
      // Check that object 1 is an array. If not, throw exception.
      //
      TR_X86OpCodes testOpCode;
      if ((J9_JAVA_CLASS_RAM_ARRAY >= CHAR_MIN) && (J9_JAVA_CLASS_RAM_ARRAY <= CHAR_MAX))
         testOpCode = TEST1MemImm1;
      else
         testOpCode = TEST4MemImm4;

      #ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
         generateRegMemInstruction(L4RegMem, node, tempReg, generateX86MemoryReference(object1Reg,  TR::Compiler->om.offsetOfObjectVftField(), cg), cg);
      #else
         generateRegMemInstruction(LRegMem(), node, tempReg, generateX86MemoryReference(object1Reg,  TR::Compiler->om.offsetOfObjectVftField(), cg), cg);
      #endif

	 TR::TreeEvaluator::generateVFTMaskInstruction(node, tempReg, cg);
         generateMemImmInstruction(testOpCode, node, generateX86MemoryReference(tempReg,  offsetof(J9Class, classDepthAndFlags), cg), J9_JAVA_CLASS_RAM_ARRAY, cg);
      if (!snippetLabel)
         {
         snippetLabel = generateLabelSymbol(cg);
         instr        = generateLabelInstruction(JE4, node, snippetLabel, cg);
         snippet      = new (cg->trHeapMemory()) TR::X86CheckFailureSnippet(cg, node->getSymbolReference(), snippetLabel, instr);
         cg->addSnippet(snippet);
         }
      else
         generateLabelInstruction(JE4, node, snippetLabel, cg);
      }

   // Test equality of the object classes.
   //
   generateRegMemInstruction(LRegMem(use64BitClasses), node, tempReg, generateX86MemoryReference(object1Reg, TR::Compiler->om.offsetOfObjectVftField(), cg), cg);
   generateRegMemInstruction(XORRegMem(use64BitClasses), node, tempReg, generateX86MemoryReference(object2Reg, TR::Compiler->om.offsetOfObjectVftField(), cg), cg);
   TR::TreeEvaluator::generateVFTMaskInstruction(node, tempReg, cg);

   // If either object is known to be a primitive array, we are done. Either
   // the equality test fails and we throw the exception or it succeeds and
   // we finish.
   //
   if (node->isArrayChkPrimitiveArray1() || node->isArrayChkPrimitiveArray2())
      {
      if (!snippetLabel)
         {
         snippetLabel = generateLabelSymbol(cg);
         instr        = generateLabelInstruction(JNE4, node, snippetLabel, cg);
         snippet      = new (cg->trHeapMemory()) TR::X86CheckFailureSnippet(cg, node->getSymbolReference(), snippetLabel, instr);
         cg->addSnippet(snippet);
         }
      else
         generateLabelInstruction(JNE4, node, snippetLabel, cg);
      }

   // Otherwise, there is more testing to do. If the classes are equal we
   // are done, and branch to the fallThrough label.
   //
   else
      {
      generateLabelInstruction(JE4, node, fallThrough, cg);

      // If either object is not known to be a reference array type, check it
      // We already know that object1 is an array type but we may have to now
      // check object2.
      //
      if (!node->isArrayChkReferenceArray1())
         {

         #ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
            generateRegMemInstruction(L4RegMem, node, tempReg, generateX86MemoryReference(object1Reg,  TR::Compiler->om.offsetOfObjectVftField(), cg), cg);
         #else
            generateRegMemInstruction(LRegMem(), node, tempReg, generateX86MemoryReference(object1Reg,  TR::Compiler->om.offsetOfObjectVftField(), cg), cg);
         #endif

	 TR::TreeEvaluator::generateVFTMaskInstruction(node, tempReg, cg);
         generateRegMemInstruction(LRegMem(), node, tempReg, generateX86MemoryReference(tempReg, offsetof(J9Class, classDepthAndFlags), cg), cg);
         // X = (ramclass->ClassDepthAndFlags)>>J9_JAVA_CLASS_RAM_SHAPE_SHIFT

         // X & OBJECT_HEADER_SHAPE_MASK
         generateRegImmInstruction(ANDRegImm4(), node, tempReg, (OBJECT_HEADER_SHAPE_MASK << J9_JAVA_CLASS_RAM_SHAPE_SHIFT), cg);
         generateRegImmInstruction(CMPRegImm4(), node, tempReg, (OBJECT_HEADER_SHAPE_POINTERS << J9_JAVA_CLASS_RAM_SHAPE_SHIFT), cg);

         if (!snippetLabel)
            {
            snippetLabel = generateLabelSymbol(cg);
            instr        = generateLabelInstruction(JNE4, node, snippetLabel, cg);
            snippet      = new (cg->trHeapMemory()) TR::X86CheckFailureSnippet(cg, node->getSymbolReference(), snippetLabel, instr);
            cg->addSnippet(snippet);
            }
         else
            generateLabelInstruction(JNE4, node, snippetLabel, cg);
         }
      if (!node->isArrayChkReferenceArray2())
         {
         // Check that object 2 is an array. If not, throw exception.
         //
         TR_X86OpCodes testOpCode;
         if ((J9_JAVA_CLASS_RAM_ARRAY >= CHAR_MIN) && (J9_JAVA_CLASS_RAM_ARRAY <= CHAR_MAX))
            testOpCode = TEST1MemImm1;
         else
            testOpCode = TEST4MemImm4;

         // Check that object 2 is an array. If not, throw exception.
         //
         #ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
            generateRegMemInstruction(L4RegMem, node, tempReg, generateX86MemoryReference(object2Reg,  TR::Compiler->om.offsetOfObjectVftField(), cg), cg);
         #else
            generateRegMemInstruction(LRegMem(), node, tempReg, generateX86MemoryReference(object2Reg,  TR::Compiler->om.offsetOfObjectVftField(), cg), cg);
         #endif
	    TR::TreeEvaluator::generateVFTMaskInstruction(node, tempReg, cg);
            generateMemImmInstruction(testOpCode, node, generateX86MemoryReference(tempReg,  offsetof(J9Class, classDepthAndFlags), cg), J9_JAVA_CLASS_RAM_ARRAY, cg);
         if (!snippetLabel)
            {
            snippetLabel = generateLabelSymbol(cg);
            instr        = generateLabelInstruction(JE4, node, snippetLabel, cg);
            snippet      = new (cg->trHeapMemory()) TR::X86CheckFailureSnippet(cg, node->getSymbolReference(), snippetLabel, instr);
            cg->addSnippet(snippet);
            }
         else
            generateLabelInstruction(JE4, node, snippetLabel, cg);

         generateRegMemInstruction(LRegMem(), node, tempReg, generateX86MemoryReference(tempReg, offsetof(J9Class, classDepthAndFlags), cg), cg);
         generateRegImmInstruction(ANDRegImm4(), node, tempReg, (OBJECT_HEADER_SHAPE_MASK << J9_JAVA_CLASS_RAM_SHAPE_SHIFT), cg);
         generateRegImmInstruction(CMPRegImm4(), node, tempReg, (OBJECT_HEADER_SHAPE_POINTERS << J9_JAVA_CLASS_RAM_SHAPE_SHIFT), cg);

         generateLabelInstruction(JNE4, node, snippetLabel, cg);
         }

      // Now both objects are known to be reference arrays, so they are
      // compatible for arraycopy.
      }

   // Now generate the fall-through label
   //
   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)4, cg);
   deps->addPostCondition(object1Reg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(object2Reg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(tempReg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);

   generateLabelInstruction(LABEL, node, fallThrough, deps, cg);

   cg->stopUsingRegister(tempReg);
   cg->decReferenceCount(object1);
   cg->decReferenceCount(object2);

   return NULL;
   }



#if defined(TR_TARGET_64BIT)
static bool inlineCurrentTimeMillis(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   if (debug("traceInlInlining"))
      diagnostic("currentTimeMillis called by %s\n", comp->signature());

   if (cg->getSupportsFastCTM())
      {  // Fully Inlined Version

      // First, evaluate resultAddress if provided.  There's no telling how
      // many regs that address computation needs, so let's get it out of the
      // way before we start using registers for other things.
      //

      TR::Register *resultAddress;
      if (node->getNumChildren() == 1)
         {
         resultAddress = cg->evaluate(node->getFirstChild());
         }
      else
         {
         TR_ASSERT(node->getNumChildren() == 0, "currentTimeMillis must have zero or one children");
         resultAddress = NULL;
         }

      // Call gettimeofday(&currentTimeMillisTemp, NULL) linux
      // Call GetSystemTimeAsFileTime(&currentTimeMillisTemp) windows
#if defined(LINUX)
      TR::SymbolReference *gtod = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_AMD64getTimeOfDay,false,false,false);
#else
      TR::SymbolReference *gtod = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_AMD64GetSystemTimeAsFileTime,false,false,false);
#endif

      TR::Node *timevalNode = TR::Node::createWithSymRef(node, TR::loadaddr, 0, cg->findOrCreateCurrentTimeMillisTempSymRef());
#if defined(LINUX)
      TR::Node *aconstNode  = TR::Node::create(node, TR::aconst, 0);
      TR::Node *callNode    = TR::Node::createWithSymRef(TR::call, 2, 2, timevalNode, aconstNode, gtod);
#else
      TR::Node *callNode    = TR::Node::createWithSymRef(TR::call, 1, 1, timevalNode, gtod);
#endif

      // TODO: Use performCall
      TR::Linkage *linkage = cg->getLinkage(gtod->getSymbol()->getMethodSymbol()->getLinkageConvention());
      linkage->buildDirectDispatch(callNode, false);

#if defined(LINUX)
      // Compute milliseconds given seconds and microseconds
      //
      TR::Register *result  = cg->allocateRegister();
      TR::Register *reg     = cg->allocateRegister();
      TR::Register *raxReal = cg->allocateRegister();
      TR::Register *rdxReal = cg->allocateRegister();

      TR::RegisterDependencyConditions  *raxRdxDeps = generateRegisterDependencyConditions((uint8_t)2, (uint8_t)2, cg);
      raxRdxDeps->addPreCondition (raxReal, TR::RealRegister::eax, cg);
      raxRdxDeps->addPostCondition(raxReal, TR::RealRegister::eax, cg);
      raxRdxDeps->addPreCondition (rdxReal, TR::RealRegister::edx, cg);
      raxRdxDeps->addPostCondition(rdxReal, TR::RealRegister::edx, cg);

      TR::MemoryReference *tv_sec;
      tv_sec = generateX86MemoryReference(timevalNode, cg, false);

      // result = tv_sec * 1000 (converts seconds to milliseconds)
      generateRegMemInstruction(L8RegMem, node, result, tv_sec, cg);
      generateRegRegImmInstruction(IMUL8RegRegImm4, node, result, result, 0x3e8,cg);
      // rdx = 0x20c49ba5e353f7cf ("magic number" for multiply by 1/1000 to convert microseconds to milliseconds)
      generateRegImm64Instruction(MOV8RegImm64, node, rdxReal, 0x20c49ba5e353f7cf, cg);
      // reg = tv_usec;
      generateRegMemInstruction(L8RegMem, node, reg, generateX86MemoryReference(*tv_sec, offsetof(struct timeval, tv_usec), cg), cg);
      // rax = reg; (i.e. a copy of tv_usec)
      generateRegRegInstruction(MOV8RegReg, node, raxReal, reg, cg);
      // reg = reg >> 63;
      generateRegImmInstruction(SAR8RegImm1, node, reg, 0x3f, cg);
      // rax * rdx, result in rdx:rax
      generateRegRegInstruction(IMUL8AccReg, node, raxReal, rdxReal, raxRdxDeps, cg);
      // rdx4 = rdx4 >> 0x7;
      generateRegImmInstruction(SAR8RegImm1, node, rdxReal, 0x7, cg);
      // rdx = rdx - reg;
      generateRegRegInstruction(SUB8RegReg, node, rdxReal, reg, cg);
      // result = rdxReal + result
      generateRegMemInstruction(LEA8RegMem, node, result,
                                 generateX86MemoryReference(rdxReal, result, 0, cg), cg);

      cg->stopUsingRegister(rdxReal);
      cg->stopUsingRegister(raxReal);
      cg->stopUsingRegister(reg);



#else

      TR::MemoryReference *time;

      TR::Register *result  = cg->allocateRegister();
      TR::Register *raxReal = cg->allocateRegister();
      TR::Register *rdxReal = cg->allocateRegister();

      TR::RegisterDependencyConditions  *raxRdxDeps = generateRegisterDependencyConditions((uint8_t)2, (uint8_t)2, cg);
      raxRdxDeps->addPreCondition (raxReal, TR::RealRegister::eax, cg);
      raxRdxDeps->addPostCondition(raxReal, TR::RealRegister::eax, cg);
      raxRdxDeps->addPreCondition (rdxReal, TR::RealRegister::edx, cg);
      raxRdxDeps->addPostCondition(rdxReal, TR::RealRegister::edx, cg);

      // (I_64) ((time - 116444736000000000) /10000)

      // rdx = time
      time = generateX86MemoryReference(timevalNode, cg, false);
      generateRegMemInstruction(L8RegMem, node, rdxReal, time, cg);
      // rax = 0xfe624e212ac18000
      generateRegImm64Instruction(MOV8RegImm64, node, raxReal, 0xfe624e212ac18000, cg);
      // rdx = rdx + rax
      generateRegRegInstruction(ADD8RegReg, node, rdxReal, raxReal, cg);
      // rax = 0x346dc5d63886594b
      generateRegImm64Instruction(MOV8RegImm64, node, raxReal, 0x346dc5d63886594b, cg);
      // rdx:rax = rax * rdx
      generateRegRegInstruction(IMUL8AccReg, node, raxReal, rdxReal, raxRdxDeps, cg);
      // rdx >> 11
      generateRegImmInstruction(SAR8RegImm1, node, rdxReal, 0xb, cg);
      // rax = rdx
      generateRegRegInstruction(MOV8RegReg, node, raxReal, rdxReal, cg);
      // rax >> 63
      generateRegImmInstruction(SHR8RegImm1, node, raxReal, 0x3f, cg);
      // rdx = rax + rdx
      generateRegRegInstruction(ADD8RegReg, node, rdxReal, raxReal, cg);
      // result = rdx
      generateRegRegInstruction(MOV8RegReg, node, result, rdxReal, cg);

      cg->stopUsingRegister(raxReal);
      cg->stopUsingRegister(rdxReal);

#endif

      // Store the result to memory if necessary
      if (resultAddress)
         {
         generateMemRegInstruction(S8MemReg, node, generateX86MemoryReference(resultAddress, 0, cg), result, cg);

         cg->decReferenceCount(node->getFirstChild());
         if (node->getReferenceCount() == 1 && cg->getCurrentEvaluationTreeTop()->getNode()->getOpCodeValue() == TR::treetop)
            {
            // Result is not needed in a register, so free it up
            //
            cg->stopUsingRegister(result);
            result = NULL;
            }
         }

      node->setRegister(result);

      return true;
      }
   else
      {  // Inlined call to Port Library
      return false;
      }
   }

#else

static void
addFPXMMDependencies(
      TR::CodeGenerator *cg,
      TR::RegisterDependencyConditions *dependencies)
   {
   TR::Machine *machine = cg->machine();

   if (cg->useSSEForSinglePrecision() || cg->useSSEForDoublePrecision())
      {
      TR_LiveRegisters *lr = cg->getLiveRegisters(TR_FPR);
      if (!lr || lr->getNumberOfLiveRegisters() > 0)
         {
         for (int regIndex = TR::RealRegister::FirstXMMR; regIndex <= TR::RealRegister::LastXMMR; regIndex++)
            {
            TR::Register *dummy = cg->allocateRegister(TR_FPR);
            dummy->setPlaceholderReg();
            dependencies->addPostCondition(dummy, (TR::RealRegister::RegNum)regIndex, cg);
            cg->stopUsingRegister(dummy);
            }
         }
      }
   }

static bool
inlineCurrentTimeMillis(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   if (debug("traceInlInlining"))
      diagnostic("currentTimeMillis called by %s\n", comp->signature());

   TR::RealRegister *espReal = cg->machine()->getX86RealRegister(TR::RealRegister::esp);


      // Call gettimeofday(&currentTimeMillisTemp, NULL) linux
      // Call GetSystemTimeAsFileTime(&currentTimeMillisTemp) windows
#if defined(LINUX)
      TR::SymbolReference *gtod = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_IA32getTimeOfDay,false,false,false);
#else
      TR::SymbolReference *gtod = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_IA32GetSystemTimeAsFileTime,false,false,false);
#endif


    //const char* c = comp->getDebug()->getName(gtod);


   // Setting "supportsFastCTM" will inline the port library implementation.
   // Leaving it false will call the port library directly rather than through an INL.
   //
   if (!cg->getSupportsFastCTM())
      return false;

   TR::Register *resultAddressReg;

   // The simplier will transform a call to CTM followed immediately by an lstore/ilstore of the
   // result into a CTM call with the address of the result storage as its only child.  The result
   // of the CTM call must be stored in this situation.
   //
   bool resultUsesAutoDirectly = (node->getNumChildren() == 1) ? true : false;

   bool resultMustBeInRegisters =
      (!resultUsesAutoDirectly ||
       (node->getReferenceCount() > 1 || cg->getCurrentEvaluationTreeTop()->getNode()->getOpCodeValue() != TR::treetop)) ? true : false;

   bool resultIsInRegisters;

   if (resultUsesAutoDirectly)
      {
      resultAddressReg = cg->evaluate(node->getFirstChild());
      }
   else
      {
      TR::SymbolReference *ctmSymRef = cg->findOrCreateCurrentTimeMillisTempSymRef();
      TR::MemoryReference *resultMR = generateX86MemoryReference(ctmSymRef, cg);

      resultAddressReg = cg->allocateRegister();
      generateRegMemInstruction(LEA4RegMem, node, resultAddressReg, resultMR, cg);
      }

   #ifdef LINUX
   generateImmInstruction(PUSHImm4, node, 0, cg);
   #endif
   generateRegInstruction(PUSHReg, node, resultAddressReg, cg);

   // Force the FP register stack to be spilled.
   //
   if (!cg->useSSEForDoublePrecision())
      {
      TR::RegisterDependencyConditions  *fpSpillDependency = generateRegisterDependencyConditions(1, 0, cg);
      fpSpillDependency->addPreCondition(NULL, TR::RealRegister::AllFPRegisters, cg);
      generateInstruction(FPREGSPILL, node, fpSpillDependency, cg);
      }

   TR::Register *eaxReal = cg->allocateRegister();
   TR::Register *edxReal = cg->allocateRegister();
   TR::Register *ecxReal = cg->allocateRegister();
   TR::Register *scratchReg1 = NULL;
   TR::Register *scratchReg2 = NULL;

   #ifdef WINDOWS

   // The call to GetSystemTimeAsFileTime() returns the number of 100ns intervals since Jan. 1, 1601.
   // This value is converted to the number of milliseconds since Jan. 1, 1970 via the formula:
   //
   //   milliSecondsSince1970 = (nanoSecondsSince1601 / 10000) - 11644473600000
   //

     generateImmSymInstruction(CALLImm4, node, (int32_t)&GetSystemTimeAsFileTime, gtod, cg)->setAdjustsFramePointerBy(-4);
     //generateImmInstruction(CALLImm4, node, (int32_t)&GetSystemTimeAsFileTime, cg)->setAdjustsFramePointerBy(-4);

   if (cg->useGPRsForWin32CTMConversion())
      {
      // The long division by 10000 is performed via a magic number multiply and shift.
      //
      TR::MemoryReference *fileTimeMRLow1 = generateX86MemoryReference(resultAddressReg, 0, cg);
      TR::MemoryReference *fileTimeMRHigh1 = generateX86MemoryReference(*fileTimeMRLow1, 4, cg);

      TR::MemoryReference *fileTimeMRLow2 = generateX86MemoryReference(resultAddressReg, 0, cg);
      TR::MemoryReference *fileTimeMRHigh2 = generateX86MemoryReference(*fileTimeMRLow1, 4, cg);

      generateRegMemInstruction(L4RegMem, node, eaxReal, fileTimeMRLow1, cg);

      scratchReg1 = cg->allocateRegister();
      generateRegImmInstruction(MOV4RegImm4, node, scratchReg1, 0x3886594b, cg);
      generateRegRegInstruction(MUL4AccReg, node, eaxReal, scratchReg1, cg);
      generateRegRegInstruction(MOV4RegReg, node, ecxReal, edxReal, cg);

      generateRegMemInstruction(L4RegMem, node, eaxReal, fileTimeMRHigh1, cg);
      generateRegRegInstruction(MUL4AccReg, node, eaxReal, scratchReg1, cg);

      scratchReg2 = cg->allocateRegister();
      generateRegRegInstruction(MOV4RegReg, node, scratchReg2, edxReal, cg);
      generateRegRegInstruction(ADD4RegReg, node, ecxReal, eaxReal, cg);
      generateRegImmInstruction(ADC4RegImm4, node, scratchReg2, 0, cg);

      generateRegMemInstruction(L4RegMem, node, eaxReal, fileTimeMRLow2, cg);
      generateRegImmInstruction(MOV4RegImm4, node, scratchReg1, 0x346dc5d6, cg);
      generateRegRegInstruction(MUL4AccReg, node, eaxReal, scratchReg1, cg);
      generateRegRegInstruction(ADD4RegReg, node, ecxReal, eaxReal, cg);
      generateRegRegInstruction(ADC4RegReg, node, scratchReg2, edxReal, cg);

      generateRegMemInstruction(L4RegMem, node, eaxReal, fileTimeMRHigh2, cg);
      generateRegRegInstruction(MUL4AccReg, node, eaxReal, scratchReg1, cg);
      generateRegRegInstruction(ADD4RegReg, node, eaxReal, scratchReg2, cg);
      generateRegImmInstruction(ADC4RegImm4, node, edxReal, 0, cg);

      generateRegRegImmInstruction(SHRD4RegRegImm1, node, eaxReal, edxReal, 11, cg);
      generateRegImmInstruction(SHR4RegImm1, node, edxReal, 11, cg);

      // Convert to milliseconds since Jan. 1, 1970
      //
      generateRegImmInstruction(SUB4RegImm4, node, eaxReal, 0x30b66800, cg);
      generateRegImmInstruction(SBB4RegImm4, node, edxReal, 0x00000a97, cg);

      resultIsInRegisters = true;
      }
   else if (cg->useLongDivideHelperForWin32CTMConversion())
      {
      static char *useDIVforCTM = feGetEnv("TR_useDIVforCTM");

      if (useDIVforCTM)
         {
         scratchReg1 = cg->allocateRegister();
         generateRegImmInstruction(MOV4RegImm4, node, scratchReg1, 10000, cg);

         TR::MemoryReference *fileTimeMRLow = generateX86MemoryReference(resultAddressReg, 0, cg);
         TR::MemoryReference *fileTimeMRHigh = generateX86MemoryReference(*fileTimeMRLow, 4, cg);

         generateRegMemInstruction(L4RegMem, node, eaxReal, fileTimeMRLow, cg);
         generateRegMemInstruction(L4RegMem, node, edxReal, fileTimeMRHigh, cg);
         generateRegRegInstruction(DIV4AccReg, node, eaxReal, scratchReg1, cg);
         }
      else
         {
         // The long division by 10000 is performed by calling the long division helper.
         //
         generateImmInstruction(PUSHImms, node, 0, cg);                 // high word of 10000L
         generateImmInstruction(PUSHImm4, node, 10000, cg);             // low word of 10000L

         TR::MemoryReference *fileTimeMRLow = generateX86MemoryReference(resultAddressReg, 0, cg);
         TR::MemoryReference *fileTimeMRHigh = generateX86MemoryReference(*fileTimeMRLow, 4, cg);

         generateMemInstruction(PUSHMem, node, fileTimeMRHigh, cg);     // high word of FILETIME
         TR::Instruction *cursor =
            generateMemInstruction(PUSHMem, node, fileTimeMRLow, cg);   // low word of FILETIME

         TR::X86ImmSymInstruction *instr = generateHelperCallInstruction(cursor, TR_IA32longDivide, cg);

         TR::IA32PrivateLinkage *linkage = TR::toIA32PrivateLinkage(cg->getLinkage(TR_Private));
         if (!linkage->getProperties().getCallerCleanup())
            {
            instr->setAdjustsFramePointerBy(-16);  // 2 long args
            }

         // Don't preserve eax and edx
         //
         instr->setNeedsGCMap(0xFF00FFF6);
         }

      // Convert to milliseconds since Jan. 1, 1970
      //
      generateRegImmInstruction(SUB4RegImm4, node, eaxReal, 0x30b66800, cg);
      generateRegImmInstruction(SBB4RegImm4, node, edxReal, 0x00000a97, cg);

      resultIsInRegisters = true;
      }

#ifdef DEBUG
   static char *validateGPRConversion = feGetEnv("TR_ValidateCTMConversion");
#else
   char validateGPRConversion = 0;
#endif

   if ((!cg->useGPRsForWin32CTMConversion() && !cg->useLongDivideHelperForWin32CTMConversion()) || validateGPRConversion)
      {
      // First divide by 10000 and then subtract 11644473600000 get in milliseconds
      // since Jan 1, 1970.

      TR::IA32ConstantDataSnippet *tenKFloat = cg->findOrCreate4ByteConstant(node, 0x461c4000); // 10000.0
      TR::MemoryReference *tenKMR    = generateX86MemoryReference(tenKFloat, cg);

      TR::IA32ConstantDataSnippet *intDivControlWord   = cg->findOrCreate2ByteConstant(node, EXTENDED_PRECISION_ROUND_TO_ZERO); // integer divide control word value
      TR::MemoryReference *intDivControlWordMR = generateX86MemoryReference(intDivControlWord, cg);

      int16_t fpcw = comp->getJittedMethodSymbol()->containsOnlySinglePrecision() ?
                     SINGLE_PRECISION_ROUND_TO_NEAREST :
                     DOUBLE_PRECISION_ROUND_TO_NEAREST;

      TR::IA32ConstantDataSnippet *restoredControlWord  = cg->findOrCreate2ByteConstant(node, fpcw);
      TR::MemoryReference *restoreControlWordMR = generateX86MemoryReference(restoredControlWord, cg);

      uint64_t millisFrom1601To1970Const = CONSTANT64(0x42a52e616cd00000);

      TR::IA32ConstantDataSnippet *millisFrom1601To1970   = cg->findOrCreate8ByteConstant(node, millisFrom1601To1970Const); // 11644473600000
      TR::MemoryReference *millisFrom1601To1970MR = generateX86MemoryReference(millisFrom1601To1970, cg);

      // change control word to do int division properly
      generateMemInstruction(LDCWMem, node, intDivControlWordMR, cg);

      // load 64-bit integer result of api call and convert to extended precision fp
      TR::Register *fpReg = cg->allocateSinglePrecisionRegister(TR_X87);
      generateFPRegMemInstruction(DLLDRegMem, node, fpReg, generateX86MemoryReference(resultAddressReg, 0, cg), cg);

      // divide by 10000.0 to get to milliseconds
      generateFPRegMemInstruction(FDIVRegMem, node, fpReg, tenKMR, cg);

      // subtract 11644473600000.0 to get to milliseconds since Jan 1, 1970
      generateFPRegMemInstruction(DSUBRegMem, node, fpReg, millisFrom1601To1970MR, cg);

      // store back to memory
      generateFPMemRegInstruction(DLSTPMem, node, generateX86MemoryReference(resultAddressReg, 0, cg), fpReg, cg);

      // restore control word
      generateMemInstruction(LDCWMem, node, restoreControlWordMR, cg);

      resultIsInRegisters = false;

#ifdef DEBUG
      if (validateGPRConversion)
         {
         TR::LabelSymbol *failLabel = generateLabelSymbol(cg);
         TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
         TR::MemoryReference *lowMR = generateX86MemoryReference(resultAddressReg, 0, cg);
         generateRegMemInstruction(CMP4RegMem, node, eaxReal, lowMR, cg);
         generateLabelInstruction(JNE4, node, failLabel, cg);
         TR::MemoryReference *highMR = generateX86MemoryReference(*lowMR, 4, cg);
         generateRegMemInstruction(CMP4RegMem, node, edxReal, highMR, cg);
         generateLabelInstruction(JE4, node, doneLabel, cg);
         generateLabelInstruction(LABEL, node, failLabel, cg);
         generateInstruction(BADIA32Op, node, cg);
         generateLabelInstruction(LABEL, node, doneLabel, cg);
         resultIsInRegisters = true;
         }
#endif
         }

   #elif defined (LINUX)
   //TR::X86ImmInstruction *callInstr = generateImmInstruction(CALLImm4, node, (int32_t)&gettimeofday, cg);
   TR::X86ImmSymInstruction *callInstr = generateImmSymInstruction(CALLImm4, node, (int32_t)&gettimeofday, gtod, cg);

   generateRegImmInstruction(ADD4RegImms, node, espReal, 8, cg);

   // Calculates Second * 1000 + MicroSecond / 1000

   // load usec to a register then divide by 1000

   scratchReg1 = cg->allocateRegister();
   generateRegMemInstruction(L4RegMem, node, scratchReg1, generateX86MemoryReference(resultAddressReg, 4, cg), cg);
   generateRegImmInstruction(MOV4RegImm4, node, eaxReal, 0x10624dd3, cg);
   generateRegRegInstruction(IMUL4AccReg, node, eaxReal, scratchReg1, cg);
   generateRegImmInstruction(SAR4RegImm1, node, edxReal, 0x6, cg);
   generateRegRegInstruction(MOV4RegReg, node, scratchReg1, edxReal, cg);
   generateRegImmInstruction(ADD4RegImm4, node, scratchReg1, 0x80000000, cg);
   generateRegImmInstruction(ADC4RegImm4, node, edxReal, 0, cg);
   generateRegRegInstruction(MOV4RegReg, node, scratchReg1, edxReal, cg);

   // load second to eax then multiply by 1000
   generateRegMemInstruction(L4RegMem, node, eaxReal, generateX86MemoryReference(resultAddressReg, 0, cg), cg);
   generateRegImmInstruction(MOV4RegImm4, node, edxReal, 0x3e8, cg);
   generateRegRegInstruction(IMUL4AccReg, node, eaxReal, edxReal, cg);

   // add the two parts
   generateRegRegInstruction(ADD4RegReg, node, eaxReal, scratchReg1, cg);
   generateRegImmInstruction(ADC4RegImm4, node, edxReal, 0, cg);

   resultIsInRegisters = true;
   #endif


   if (resultIsInRegisters)
      {
      if (resultUsesAutoDirectly)
         {
         TR::MemoryReference *storeMRLow = generateX86MemoryReference(resultAddressReg, 0, cg);
         TR::MemoryReference *storeMRHigh = generateX86MemoryReference(*storeMRLow, 4, cg);
         generateMemRegInstruction(S4MemReg, node, storeMRLow, eaxReal, cg);
         generateMemRegInstruction(S4MemReg, node, storeMRHigh, edxReal, cg);
         }
      }
   else if (resultMustBeInRegisters)
      {
      // The result is in memory but it must be returned in registers.
      //
      TR::MemoryReference *loadMRLow = generateX86MemoryReference(resultAddressReg, 0, cg);
      TR::MemoryReference *loadMRHigh = generateX86MemoryReference(*loadMRLow, 4, cg);
      generateRegMemInstruction(L4RegMem, node, eaxReal, loadMRLow, cg);
      generateRegMemInstruction(L4RegMem, node, edxReal, loadMRHigh, cg);
      }
   else
      {
      TR_ASSERT(resultUsesAutoDirectly, "result of CTM not stored");
      }

   if (resultMustBeInRegisters)
      {
      TR::RegisterPair *result  = cg->allocateRegisterPair(eaxReal, edxReal);
      node->setRegister(result);
      }

   if (resultUsesAutoDirectly)
      {
      cg->decReferenceCount(node->getFirstChild());
      }

   // Build register dependencies and call the method in the system library
   // directly. Since this is a "C"-style call, ebx, esi and edi are preserved
   //
   int32_t numDeps = 4;
   if (scratchReg1)
      numDeps++;

   if (scratchReg2)
      numDeps++;

   int32_t extraFPDeps = (uint8_t)(TR::RealRegister::LastXMMR - TR::RealRegister::FirstXMMR+1);
   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, numDeps + extraFPDeps, cg);
   deps->addPostCondition(eaxReal, TR::RealRegister::eax, cg);
   deps->addPostCondition(edxReal, TR::RealRegister::edx, cg);
   deps->addPostCondition(ecxReal, TR::RealRegister::ecx, cg);
   cg->stopUsingRegister(ecxReal);
   deps->addPostCondition(resultAddressReg, TR::RealRegister::NoReg, cg);

   if (scratchReg1)
      {
      deps->addPostCondition(scratchReg1, TR::RealRegister::NoReg, cg);
      cg->stopUsingRegister(scratchReg1);
      }

   if (scratchReg2)
      {
      deps->addPostCondition(scratchReg2, TR::RealRegister::NoReg, cg);
      cg->stopUsingRegister(scratchReg2);
      }

   if (!resultUsesAutoDirectly)
      {
      cg->stopUsingRegister(resultAddressReg);
      }

   if (!resultMustBeInRegisters)
      {
      // Kill the result registers if they will not persist.
      //
      cg->stopUsingRegister(eaxReal);
      cg->stopUsingRegister(edxReal);
      }

   // add the XMM dependendencies
   addFPXMMDependencies(cg, deps);
   deps->stopAddingConditions();

   TR::LabelSymbol *labelSym = generateLabelSymbol(cg);
   generateLabelInstruction(LABEL, node, labelSym, deps, cg);

   return true;
   }
#endif

#define J9TIME_NANOSECONDS_PER_SECOND ((I_64) 1000000000)

#ifndef LINUX
bool getWindowsNanoMultiplier(uint32_t &multiplier)
   {
   LARGE_INTEGER freq;
   uint32_t hiresClockFrequency;
   if (QueryPerformanceFrequency(&freq))
      {
      hiresClockFrequency = freq.QuadPart;
      }
   else
      {
      hiresClockFrequency = 1000; /* GetTickCount() returns millis */
      }

   multiplier = 1;
   if (hiresClockFrequency < J9TIME_NANOSECONDS_PER_SECOND)
      {
      multiplier = (J9TIME_NANOSECONDS_PER_SECOND / hiresClockFrequency);
      return true;
      }
   else if (hiresClockFrequency > J9TIME_NANOSECONDS_PER_SECOND)
      {
      return false;
      }

   return true;
   }
#endif

#if defined(TR_TARGET_64BIT)
static bool
inlineNanoTime(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   if (debug("traceInlInlining"))
      diagnostic("nanoTime called by %s\n", comp->signature());

#if !defined(LINUX)
   static uint32_t multiplier=1;
   //if (!getWindowsNanoMultiplier(multiplier))
   //   return false;

   // nano time is currently disabled on Windows, we need port library call to
   // get the multiplier since because of CPU power states the frequency reported will vary
   // and the timestamp will be caclulated incorrectly.
   return false;
#endif

   if (fej9->supportsFastNanoTime())
      {  // Fully Inlined Version

      // First, evaluate resultAddress if provided.  There's no telling how
      // many regs that address computation needs, so let's get it out of the
      // way before we start using registers for other things.
      //

      TR::Register *resultAddress;
      if (node->getNumChildren() == 1)
         {
         resultAddress = cg->evaluate(node->getFirstChild());
         }
      else
         {
         TR_ASSERT(node->getNumChildren() == 0, "nanoTime must have zero or one children");
         resultAddress = NULL;
         }

#if defined(LINUX)
      TR::SymbolReference *gtod = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_AMD64clockGetTime,false,false,false);
      TR::Node *timevalNode = TR::Node::createWithSymRef(node, TR::loadaddr, 0, cg->getNanoTimeTemp());
      TR::Node *clockSourceNode = TR::Node::create(node, TR::iconst, 0, CLOCK_MONOTONIC);
      TR::Node *callNode    = TR::Node::createWithSymRef(TR::call, 2, 2, clockSourceNode, timevalNode, gtod);
      // TODO: Use performCall
      TR::Linkage *linkage = cg->getLinkage(gtod->getSymbol()->getMethodSymbol()->getLinkageConvention());
      linkage->buildDirectDispatch(callNode, false);
#else
      TR::SymbolReference *gtod = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_AMD64QueryPerformanceCounter,false,false,false);
      TR::Node *timevalNode = TR::Node::createWithSymRef(node, TR::loadaddr, 0, cg->getNanoTimeTemp());
      TR::Node *callNode    = TR::Node::createWithSymRef(TR::icall, 1, 1, timevalNode, gtod);
      // TODO: Use performCall
      TR::Linkage *linkage = cg->getLinkage(gtod->getSymbol()->getMethodSymbol()->getLinkageConvention());
      TR::Register *testreg = linkage->buildDirectDispatch(callNode, false);
#endif

#if defined(LINUX)

      TR::Register *result  = cg->allocateRegister();
      TR::Register *reg     = cg->allocateRegister();

      TR::MemoryReference *tv_sec;

      // result = tv_sec * 1,000,000,000 (converts seconds to nanoseconds)

      tv_sec = generateX86MemoryReference(timevalNode, cg, false);
      generateRegMemInstruction(L8RegMem, node, result, tv_sec, cg);
      generateRegRegImmInstruction(IMUL8RegRegImm4, node, result, result, J9TIME_NANOSECONDS_PER_SECOND, cg);

      // reg = tv_usec
      generateRegMemInstruction(L8RegMem, node, reg, generateX86MemoryReference(*tv_sec, offsetof(struct timespec, tv_nsec), cg), cg);

      // result = reg + result
      generateRegMemInstruction(LEA8RegMem, node, result, generateX86MemoryReference(reg, result, 0, cg), cg);

      cg->stopUsingRegister(reg);


#else
      TR::LabelSymbol *tickCountLabel    = generateLabelSymbol(cg);
      TR::LabelSymbol *StartLabel        = generateLabelSymbol(cg);
      TR::LabelSymbol *reStartLabel    = generateLabelSymbol(cg);
      StartLabel->setStartInternalControlFlow();
      reStartLabel->setEndInternalControlFlow();

      generateRegRegInstruction(TEST1RegReg, node, testreg, testreg, cg);

      cg->stopUsingRegister(testreg);

      generateLabelInstruction(LABEL, node, StartLabel, cg);

      TR::Register *result  = cg->allocateRegister();

      generateLabelInstruction(JE4, node, tickCountLabel, cg);

      TR::MemoryReference *time = generateX86MemoryReference(timevalNode, cg, false);
      generateRegMemInstruction(L8RegMem, node, result, time, cg);

      generateLabelInstruction(LABEL, node, reStartLabel, cg);

      generateRegRegImmInstruction(IMUL8RegRegImm4, node, result, result, multiplier, cg);

      //Outlined Code get tick count
      gtod = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_AMD64GetTickCount,false,false,false);

      TR::Node *callNode2 = TR::Node::createWithSymRef(node, TR::icall, 0, gtod);

      TR_OutlinedInstructions *outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(callNode2, TR::icall, result, tickCountLabel, reStartLabel, MOVZXReg8Reg4, cg);
      cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);
      cg->generateDebugCounter(
         outlinedHelperCall->getFirstInstruction(),
         TR::DebugCounter::debugCounterName(comp, "helperCalls/%s/(%s)/%d/%d", node->getOpCode().getName(), comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
         1, TR::DebugCounter::Cheap);

#endif

      // Store the result to memory if necessary
      if (resultAddress)
         {
         generateMemRegInstruction(S8MemReg, node, generateX86MemoryReference(resultAddress, 0, cg), result, cg);

         cg->decReferenceCount(node->getFirstChild());
         if (node->getReferenceCount() == 1 && cg->getCurrentEvaluationTreeTop()->getNode()->getOpCodeValue() == TR::treetop)
            {
            // Result is not needed in a register, so free it up
            //
            cg->stopUsingRegister(result);
            result = NULL;
            }
         }

      node->setRegister(result);

      return true;
      }
   else
      {  // Inlined call to Port Library
      return false;
      }
   }
#else // !64bit

static bool
inlineNanoTime(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   if (debug("traceInlInlining"))
      diagnostic("nanoTime called by %s\n", comp->signature());

   TR::RealRegister  *espReal = cg->machine()->getX86RealRegister(TR::RealRegister::esp);
   TR::Register *vmThreadReg = cg->getVMThreadRegister();
   TR::Register *temp2 = 0;

#if !defined(LINUX)
   static uint32_t multiplier=1;
   //if (!getWindowsNanoMultiplier(multiplier))
   //   return false;

   // nano time is currently disabled on Windows, we need port library call to
   // get the multiplier since because of CPU power states the frequency reported will vary
   // and the timestamp will be caclulated incorrectly.
   return false;

#endif

   if (fej9->supportsFastNanoTime())
      {
      TR::Register *resultAddress;
      if (node->getNumChildren() == 1)
         {
         resultAddress = cg->evaluate(node->getFirstChild());
         generateRegInstruction(PUSHReg,  node, resultAddress, cg);
         #ifdef LINUX
         generateImmInstruction(PUSHImm4, node, CLOCK_MONOTONIC, cg);
         #endif
         }
      else
         {
         // Leave space on the stack for the 64-bit result
         //

         generateRegImmInstruction(SUB4RegImms, node, espReal, 8, cg);

         #ifdef LINUX
         resultAddress = cg->allocateRegister();
         generateRegRegInstruction(MOV4RegReg, node, resultAddress, espReal, cg); // save away esp before the push
         generateRegInstruction(PUSHReg,  node, resultAddress, cg);
         generateImmInstruction(PUSHImm4, node, CLOCK_MONOTONIC, cg);
         cg->stopUsingRegister(resultAddress);
         resultAddress = espReal;
         #else
         resultAddress = espReal;
         generateRegInstruction(PUSHReg,  node, resultAddress, cg);
         #endif
         }

      // Force the FP register stack to be spilled.
      //
      if (!cg->useSSEForDoublePrecision())
         {
         TR::RegisterDependencyConditions  *fpSpillDependency = generateRegisterDependencyConditions(1, 0, cg);
         fpSpillDependency->addPreCondition(NULL, TR::RealRegister::AllFPRegisters, cg);
         generateInstruction(FPREGSPILL, node, fpSpillDependency, cg);
         }

      // 64-bit issues on the call instructions below
      #ifdef WINDOWS

      TR::LabelSymbol *tickCountLabel    = generateLabelSymbol(cg);
      TR::LabelSymbol *StartLabel        = generateLabelSymbol(cg);
      TR::LabelSymbol *reStartLabel    = generateLabelSymbol(cg);
      StartLabel->setStartInternalControlFlow();
      reStartLabel->setEndInternalControlFlow();

      int32_t extraFPDeps = (uint8_t)(TR::RealRegister::LastXMMR - TR::RealRegister::FirstXMMR+1);
      TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)4 + extraFPDeps, cg);
      TR::Register *eaxReal = cg->allocateRegister();
      deps->addPostCondition(eaxReal, TR::RealRegister::eax, cg);
      TR::Register *temp1 = cg->allocateRegister();
      deps->addPostCondition(temp1, TR::RealRegister::edx, cg);
      cg->stopUsingRegister(temp1);
      temp1 = cg->allocateRegister();
      deps->addPostCondition(temp1, TR::RealRegister::ecx, cg);
      cg->stopUsingRegister(temp1);
      deps->addPostCondition(cg->getMethodMetaDataRegister(), TR::RealRegister::ebp, cg);

      // add the XMM dependendencies
      addFPXMMDependencies(cg, deps);
      deps->stopAddingConditions();

      TR::X86ImmInstruction  *callInstr = generateImmInstruction(CALLImm4, node, (int32_t)&QueryPerformanceCounter, deps, cg);
      callInstr->setAdjustsFramePointerBy(-4);

      generateRegRegInstruction(TEST1RegReg, node, eaxReal, eaxReal, cg);

      cg->stopUsingRegister(eaxReal);

      generateLabelInstruction(LABEL, node, StartLabel, cg);

      generateLabelInstruction(JE4, node, tickCountLabel, cg);

      #elif defined (LINUX)

      // Build register dependencies and call the method in the system library
      // directly. Since this is a "C"-style call, ebx, esi and edi are preserved
      //
      int32_t extraFPDeps = (uint8_t)(TR::RealRegister::LastXMMR - TR::RealRegister::FirstXMMR+1);
      TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)4 + extraFPDeps, cg);
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

      // add the XMM dependendencies
      addFPXMMDependencies(cg, deps);
      deps->stopAddingConditions();

      TR::X86ImmInstruction  *callInstr = generateImmInstruction(CALLImm4, node, (int32_t)&clock_gettime, deps, cg);

      generateRegImmInstruction(ADD4RegImms, node, espReal, 8, cg);

      TR::Register *eaxReal = cg->allocateRegister();
      TR::Register *edxReal = cg->allocateRegister();

      // load usec to a register
      TR::Register *reglow = cg->allocateRegister();
      generateRegMemInstruction(L4RegMem, node, reglow, generateX86MemoryReference(resultAddress, 4, cg), cg);


      TR::RegisterDependencyConditions  *dep1 = generateRegisterDependencyConditions((uint8_t)2, 2, cg);
      dep1->addPreCondition(eaxReal, TR::RealRegister::eax, cg);
      dep1->addPreCondition(edxReal, TR::RealRegister::edx, cg);
      dep1->addPostCondition(eaxReal, TR::RealRegister::eax, cg);
      dep1->addPostCondition(edxReal, TR::RealRegister::edx, cg);


      // load second to eax then multiply by 1,000,000,000

      generateRegMemInstruction(L4RegMem, node, edxReal, generateX86MemoryReference(resultAddress, 0, cg), cg);
      generateRegImmInstruction(MOV4RegImm4, node, eaxReal, J9TIME_NANOSECONDS_PER_SECOND, cg);
      generateRegRegInstruction(IMUL4AccReg, node, eaxReal, edxReal, dep1, cg);


      // add the two parts then store it back
      generateRegRegInstruction(ADD4RegReg, node, eaxReal, reglow, cg);
      generateRegImmInstruction(ADC4RegImm4, node, edxReal, 0x0, cg);
      generateMemRegInstruction(S4MemReg, node, generateX86MemoryReference(resultAddress, 0, cg), eaxReal, cg);
      generateMemRegInstruction(S4MemReg, node, generateX86MemoryReference(resultAddress, 4, cg), edxReal, cg);

      cg->stopUsingRegister(eaxReal);
      cg->stopUsingRegister(edxReal);
      cg->stopUsingRegister(reglow);


      #endif

      TR::Register *lowReg = cg->allocateRegister();
      TR::Register *highReg = cg->allocateRegister();

      if (node->getNumChildren() == 1)
         {
         if (node->getReferenceCount() > 1 ||
             cg->getCurrentEvaluationTreeTop()->getNode()->getOpCodeValue() != TR::treetop)
            {
            generateRegMemInstruction(L4RegMem, node, lowReg, generateX86MemoryReference(resultAddress, 0, cg), cg);
            generateRegMemInstruction(L4RegMem, node, highReg, generateX86MemoryReference(resultAddress, 4, cg), cg);

            TR::RegisterPair *result  = cg->allocateRegisterPair(lowReg, highReg);
            node->setRegister(result);
            }
         cg->decReferenceCount(node->getFirstChild());
         }
      else
         {
         // The result of the call is now on the stack. Get it into registers.
         //
         generateRegInstruction(POPReg, node, lowReg, cg);
         generateRegInstruction(POPReg, node, highReg, cg);
         TR::RegisterPair *result = cg->allocateRegisterPair(lowReg, highReg);
         node->setRegister(result);
         }

#ifdef WINDOWS
         generateLabelInstruction(LABEL, node, reStartLabel, cg);

         eaxReal = cg->allocateRegister();
         TR::Register *edxReal = cg->allocateRegister();

         TR::RegisterDependencyConditions  *dep1 = generateRegisterDependencyConditions((uint8_t)2, 4, cg);
         dep1->addPreCondition(eaxReal, TR::RealRegister::eax, cg);
         dep1->addPreCondition(edxReal, TR::RealRegister::edx, cg);
         dep1->addPostCondition(eaxReal, TR::RealRegister::eax, cg);
         dep1->addPostCondition(edxReal, TR::RealRegister::edx, cg);
         dep1->addPostCondition(lowReg, TR::RealRegister::NoReg, cg);
         dep1->addPostCondition(highReg, TR::RealRegister::NoReg, cg);
         dep1->stopAddingConditions();

         generateRegImmInstruction(MOV4RegImm4, node, eaxReal, multiplier, cg);
         generateRegRegInstruction(MUL4AccReg, node, eaxReal, lowReg, dep1, cg);
         generateRegRegImmInstruction(IMUL4RegRegImm4, node, highReg, highReg, multiplier, cg);

         generateRegRegInstruction(MOV4RegReg, node, lowReg, eaxReal, cg);
         generateRegRegInstruction(ADD4RegReg, node, highReg, edxReal, cg);

         cg->stopUsingRegister(eaxReal);
         cg->stopUsingRegister(edxReal);

          //Outlined Code get tick count

         TR::SymbolReference *gtod = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_IA32GetTickCount,false,false,false);
         TR::Node *callNode2    = TR::Node::createWithSymRef(node, TR::icall, 0, gtod);
         TR_OutlinedInstructions *outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(callNode2, TR::icall, node->getRegister()->getLowOrder(), tickCountLabel, reStartLabel, cg);
         cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);
         outlinedHelperCall->swapInstructionListsWithCompilation();
         generateRegImmInstruction(comp->getFirstInstruction(), ADD4RegImm4, espReal,8, cg);
         generateRegRegInstruction(comp->getAppendInstruction()->getPrev()->getPrev(), XOR4RegReg, node->getRegister()->getHighOrder(), node->getRegister()->getHighOrder(), cg);
         outlinedHelperCall->swapInstructionListsWithCompilation();
         cg->generateDebugCounter(
            outlinedHelperCall->getFirstInstruction(),
            TR::DebugCounter::debugCounterName(comp, "helperCalls/%s/(%s)/%d/%d", node->getOpCode().getName(), comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
            1, TR::DebugCounter::Cheap);

#endif

      }
   else
      {
      // This code is busted. The hires clock is measured in microseconds, not
      // nanoseconds, and this code doesn't correct for that. The above code
      // will be faster anyway, and it should be upgraded to support AOT, so
      // then we'll never need the hires clock version again.
      static char *useHiResClock = feGetEnv("TR_useHiResClock");
      if (!useHiResClock)
         return false;
      // Leave space on the stack for the 64-bit result
      //
      cg->setVMThreadRequired(true);
      temp2 = cg->allocateRegister();
      generateRegMemInstruction(L4RegMem, node, temp2, generateX86MemoryReference(vmThreadReg, offsetof(J9VMThread, javaVM), cg), cg);
      cg->setVMThreadRequired(false);
      generateRegMemInstruction(L4RegMem, node, temp2, generateX86MemoryReference(temp2, offsetof(J9JavaVM, portLibrary), cg), cg);
      generateRegInstruction(PUSHReg, node, espReal, cg);
      generateRegInstruction(PUSHReg, node, temp2, cg);

      if (!cg->useSSEForDoublePrecision())
         {
         // Force the FP register stack to be spilled.
         //
         TR::RegisterDependencyConditions  *fpSpillDependency = generateRegisterDependencyConditions(1, 0, cg);
         fpSpillDependency->addPreCondition(NULL, TR::RealRegister::AllFPRegisters, cg);
         generateInstruction(FPREGSPILL, node, fpSpillDependency, cg);
         }

      int32_t extraFPDeps = (uint8_t)(TR::RealRegister::LastXMMR - TR::RealRegister::FirstXMMR+1);

      // Build register dependencies and call the method in the port library
      // directly. Since this is a "C"-style call, ebx, esi and edi are preserved
      //
      TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)4 + extraFPDeps, cg);
      TR::Register *temp1 = cg->allocateRegister();
      deps->addPostCondition(temp1, TR::RealRegister::ecx, cg);
      cg->stopUsingRegister(temp1);

      TR::Register *lowReg = cg->allocateRegister();
      deps->addPostCondition(lowReg, TR::RealRegister::eax, cg);

      TR::Register *highReg = cg->allocateRegister();
      deps->addPostCondition(highReg, TR::RealRegister::edx, cg);

      deps->addPostCondition(cg->getMethodMetaDataRegister(), TR::RealRegister::ebp, cg);

      // add the XMM dependendencies
      addFPXMMDependencies(cg, deps);
      deps->stopAddingConditions();

      generateCallMemInstruction(CALLMem, node, generateX86MemoryReference(temp2, offsetof(OMRPortLibrary, time_hires_clock), cg), deps, cg);
      cg->stopUsingRegister(temp2);

      generateRegImmInstruction(ADD4RegImms, node, espReal, 8, cg);

      TR::RegisterPair *result = cg->allocateRegisterPair(lowReg, highReg);
      node->setRegister(result);
      }

   return true;
   }
#endif


enum TR_InlinedMathFunctions
   {
   TR_Max_I    = 0,
   TR_Min_I    = 1,
   TR_Abs_I    = 2,
   TR_Max_L    = 3,
   TR_Min_L    = 4,
   TR_Abs_L    = 5,
   TR_Abs_F    = 6,
   TR_Abs_D    = 7
   };



#if !defined(_LONG_LONG) && !defined(LINUX)
#define DOUBLE_NEGATIVE_ZERO 0x8000000000000000L
#define FLOAT_NEGATIVE_ZERO  0x80000000
#else
#define DOUBLE_NEGATIVE_ZERO 0x8000000000000000LL
#define FLOAT_NEGATIVE_ZERO  0x80000000
#endif

static bool
inlineSimpleMathFunction(
      TR_InlinedMathFunctions func,
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   switch(func)
      {
      case TR_Max_I:
      case TR_Min_I:
         {
         if (!cg->supportsCMOV())
            return false;

         TR::Node *firstChild = node->getFirstChild();
         TR::Node *secondChild = node->getSecondChild();
         TR::Register *resultReg = cg->allocateRegister(), *firstReg, *secondReg;

         firstReg = cg->evaluate(firstChild);
         secondReg = cg->evaluate(secondChild);

         TR_ASSERT(firstReg && secondReg, "We should've had both operands in registers by now.");

         generateRegRegInstruction(MOV4RegReg, node, resultReg, firstReg, cg);
         generateRegRegInstruction(CMP4RegReg, node, resultReg, secondReg, cg);
         if (func == TR_Min_I)
            generateRegRegInstruction(CMOVG4RegReg, node, resultReg, secondReg, cg);
         else
            generateRegRegInstruction(CMOVL4RegReg, node, resultReg, secondReg, cg);

         node->setRegister(resultReg);
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);

         return true;
         }

      case TR_Max_L:
      case TR_Min_L:
         {
         if (TR::Compiler->target.is32Bit())
            return false;

         if (!cg->supportsCMOV())
            return false;

         TR::Node *firstChild = node->getFirstChild();
         TR::Node *secondChild = node->getSecondChild();
         TR::Register *resultReg = cg->allocateRegister(), *firstReg, *secondReg;

         firstReg = cg->evaluate(firstChild);
         secondReg = cg->evaluate(secondChild);

         TR_ASSERT(firstReg && secondReg, "We should've had both operands in registers by now.");

         generateRegRegInstruction(MOV8RegReg, node, resultReg, firstReg, cg);
         generateRegRegInstruction(CMP8RegReg, node, resultReg, secondReg, cg);
         if (func == TR_Min_L)
            generateRegRegInstruction(CMOVG8RegReg, node, resultReg, secondReg, cg);
         else
            generateRegRegInstruction(CMOVL8RegReg, node, resultReg, secondReg, cg);

         node->setRegister(resultReg);
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);

         return true;
         }

      case TR_Abs_I:
         {
         TR::Node *firstChild = node->getFirstChild();
         TR::Register *resultReg = cg->allocateRegister(), *firstReg, *tempReg;

         if (firstChild->getOpCode().isLoadConst())
            {
            int32_t value = firstChild->getInt();
            if (value<0) value = -value;

            generateRegImmInstruction(MOV4RegImm4, node, resultReg, value, cg);
            node->setRegister(resultReg);
            cg->decReferenceCount(firstChild);
            }
         else
            {
            firstReg = cg->evaluate(firstChild);
            generateRegRegInstruction(MOV4RegReg, node, resultReg, firstReg, cg);
            if (!firstChild->isNonNegative())
               {
                  tempReg = cg->allocateRegister();
               generateRegRegInstruction(MOV4RegReg, node, tempReg, resultReg, cg);
               generateRegImmInstruction(SAR4RegImm1, node, tempReg, sizeof(int32_t)*8-1, cg);
               generateRegRegInstruction(XOR4RegReg, node, resultReg, tempReg, cg);
               generateRegRegInstruction(SUB4RegReg, node, resultReg, tempReg, cg);
               cg->stopUsingRegister(tempReg);
               }

            node->setRegister(resultReg);
            cg->decReferenceCount(firstChild);
            }

         return true;
         }

      case TR_Abs_L:
         {
         if (TR::Compiler->target.is32Bit())
            return false;

         TR::Node *firstChild = node->getFirstChild();
         TR::Register *resultReg = cg->allocateRegister(), *firstReg, *tempReg;

         firstReg = cg->evaluate(firstChild);
         generateRegRegInstruction(MOV8RegReg, node, resultReg, firstReg, cg);

         if (!firstChild->isNonNegative())
            {
               tempReg = cg->allocateRegister();
            generateRegRegInstruction(MOV8RegReg, node, tempReg, resultReg, cg);
            generateRegImmInstruction(SAR8RegImm1, node, tempReg, sizeof(int64_t)*8-1, cg);
            generateRegRegInstruction(XOR8RegReg, node, resultReg, tempReg, cg);
            generateRegRegInstruction(SUB8RegReg, node, resultReg, tempReg, cg);
            cg->stopUsingRegister(tempReg);
            }

         node->setRegister(resultReg);
         cg->decReferenceCount(firstChild);

         return true;
         }

      case TR_Abs_F:
      case TR_Abs_D:
         {
         if (TR::Compiler->target.is32Bit())
            return false;

         TR::Node     *operand        = NULL;
         TR::Node     *firstChild     = NULL;

         if (node->getNumChildren() == 1)
            {
            operand = node->getFirstChild();
            }
         else
            {
            firstChild = node->getFirstChild();
            operand    = node->getSecondChild();
            }

         TR::Register *targetRegister = NULL;
         TR::Register *opRegister     = cg->evaluate(operand);

         TR_ASSERT(opRegister->getKind() == TR_FPR, "floating point ABS is only supported for SSE instructions");

         TR::IA32ConstantDataSnippet *cds = NULL;
         TR_X86OpCodes andOpCode, movOpCode;
         targetRegister = cg->allocateRegister(TR_FPR);

         if (func == TR_Abs_F)
            {
            cds = cg->findOrCreate4ByteConstant(node, FLOAT_NEGATIVE_ZERO);
            movOpCode = MOVSSRegMem;
            andOpCode = ANDNPSRegReg;
            targetRegister->setIsSinglePrecision();
            }
         else
            {
            cds = cg->findOrCreate8ByteConstant(node, DOUBLE_NEGATIVE_ZERO);
            movOpCode = MOVSDRegMem;
            andOpCode = ANDNPDRegReg;
            }

         TR::MemoryReference *constMR = generateX86MemoryReference(cds, cg);

         generateRegMemInstruction(movOpCode, node, targetRegister, constMR, cg);
         generateRegRegInstruction(andOpCode, node, targetRegister, opRegister, cg);

         node->setRegister(targetRegister);
         if (firstChild)
            cg->recursivelyDecReferenceCount(firstChild);

         cg->decReferenceCount(operand);

         return true;
         }
      default:
         {
         TR_ASSERT(0,"Undefined simple math function");
         return false;
         }
      }
   return false;
   }


static bool inlineMathLog(TR::Node *node, TR::CodeGenerator *cg)
   {

   // The base 'e' logarithm can be implemented with a faster X87
   // instruction sequence:
   //
   //    fldln2
   //    movsd [tempMR], xmm0
   //    fld [tempMR]
   //    fyl2x
   //    fstp [tempMR]
   //    movsd xmm0, [tempMR]
   //

   TR::Register *firstChildReg = cg->evaluate(node->getFirstChild());

   generateInstruction(FLDLN2, node, cg);

   TR::MemoryReference *tempMR = cg->machine()->getDummyLocalMR(TR::Double);
   generateMemRegInstruction(MOVSDMemReg, node, tempMR, firstChildReg, cg);

   tempMR = cg->machine()->getDummyLocalMR(TR::Double);
   generateMemInstruction(DLDMem, node, tempMR, cg);
   generateInstruction(FYL2X, node, cg);

   tempMR = cg->machine()->getDummyLocalMR(TR::Double);
   generateMemInstruction(DSTPMemReg, node, tempMR, cg);

   tempMR = cg->machine()->getDummyLocalMR(TR::Double);
   TR::Register *targetReg = cg->allocateRegister(TR_FPR);
   generateRegMemInstruction(MOVSDRegMem, node, targetReg, tempMR, cg);

   node->setRegister(targetReg);
   cg->decReferenceCount(node->getFirstChild());

   return true;
   }


static bool
inlineMathSQRT(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   // Sometimes the call can have 2 children, where the first one is loadaddr
   TR::Node *operand = NULL;
   TR::Node *firstChild = NULL;
   TR::Register *targetRegister = NULL;

   if (node->getNumChildren() == 1)
      {
      operand = node->getFirstChild();
      }
   else
      {
      firstChild = node->getFirstChild();
      operand    = node->getSecondChild();
      }

   if (node->getReferenceCount()==1)
      {
      if (firstChild)
         cg->recursivelyDecReferenceCount(firstChild);

      cg->recursivelyDecReferenceCount(operand);
      return true;
      }


   // The current Neutrino sqrt() runtime function does not always return
   // a result in the expected precision.  Hence, do not fold constants
   // at compile time.
   //
   // TODO: Instead of avoiding it altogether the preferred fix is to call
   // the fdlibm version of sqrt (involves linking in the fdlibm library).
   //
   if (operand->getOpCode().isLoadConst())
      {
      union
         {
         struct
            {
            uint32_t lower;
            uint32_t upper;
            } s;
         double   doubleVal;
         int64_t  rawBits;
         } d;

      d.doubleVal = operand->getDouble();

      // Do not assume that the C runtime will return the correct values that Java requires
      // for a j/l/Math.sqrt() call.  Provide the correct values for the special cases.
      //
      if (d.s.upper & 0x80000000)
         {
         if (d.s.upper != 0x80000000 || d.s.lower != 0x00000000)
            {
            // -ve number other than -0.0 returns NaN.
            // -0.0 returns -0.0.
            //
            d.s.lower = 0;
            d.s.upper = 0x7ff80000;
            }
         }
      else if (! ((d.s.lower == 0) && (d.s.upper == 0 || d.s.upper == 0x7ff00000 || d.s.upper == 0x7ff80000)))
         {
         // +0.0 or +INF or NaN returns the argument.
         // Anything else can be computed from the sqrt() function.
         //
         d.doubleVal = sqrt(d.doubleVal);
         }

      TR::IA32ConstantDataSnippet *cds = cg->findOrCreate8ByteConstant(operand, d.rawBits);

      if (cg->useSSEForDoublePrecision())
         {
         targetRegister = cg->allocateRegister(TR_FPR);
         generateRegMemInstruction(MOVSDRegMem, node, targetRegister, generateX86MemoryReference(cds, cg), cg);
         }
      else
         {
         targetRegister = cg->allocateRegister(TR_X87);
         generateFPRegMemInstruction(DLDRegMem, node, targetRegister, generateX86MemoryReference(cds, cg), cg);
         }
      }
   else
      {
      TR::Register *opRegister = NULL;
      opRegister = cg->evaluate(operand);

      if (opRegister->getKind() == TR_FPR)
         {
         if (operand->getReferenceCount()==1)
            targetRegister = opRegister;
         else
            targetRegister = cg->allocateRegister(TR_FPR);

         generateRegRegInstruction(SQRTSDRegReg, node, targetRegister, opRegister, cg);
         }
      else
         {
         targetRegister = cg->doubleClobberEvaluate(operand);
         generateFPRegInstruction(DSQRTReg, node, targetRegister, cg);
         }
      }

   node->setRegister(targetRegister);
   if (firstChild)
      cg->recursivelyDecReferenceCount(firstChild);

   cg->decReferenceCount(operand);
   return true;

   }

static bool inlineObjectHashCode(TR::Node *node, bool isIndirect)
   {
   // The hashcode implementation originally contained in this method has been deprecated
   // and deleted.  See any JIT release <= tr.r11 for the original code.
   //
   return false;
   }


// Convert serial String.hashCode computation into vectorization copy and implement with SSE instruction
//
// start_label
// if size < threshold, goto serial_label, current threshold is 4
//    xmm0 = load 16 bytes align constant [923521, 923521, 923521, 923521]
//    xmm1 = 0
// SSEloop
//    xmm2 = load 8 byte value in lower 8 bytes.
//    xmm1 = xmm1 * xmm0
//    movzxwd xmm2, xmm2
//    xmm1 = xmm1 + xmm2
//    i = i + 4;
//    cmp i, end -3
//    jge SSEloop
// xmm0 = load 16 bytes align [31^3, 31^2, 31, 1]
// xmm1 = xmm1 * xmm0      value containst [a0, a1, a2, a3]
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
static bool
inlineStringHashCode(
      TR::Node *node,
      bool isIndirect,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   if (isIndirect)
      diagnostic("String.hashCode called indirectly by %s\n", comp->signature());
   else
      {
      TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *SSELabel = generateLabelSymbol(cg);
      TR::LabelSymbol *SerialLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *SerialLoopLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *SSEReduceLabel = generateLabelSymbol(cg);
      startLabel->setStartInternalControlFlow();
      endLabel->setEndInternalControlFlow();

      // get register for value, offset, count
      TR::Node *valueNode = node->getFirstChild();
      TR::Node *offsetNode = node->getSecondChild();
      TR::Node *countNode = node->getThirdChild();

      TR::Register *valueReg = cg->evaluate(valueNode);
      // Use clobber evaluator because we're going to write to indexReg so we need a copy if it's not the last time it's used,
      // which is what clobber evaluator does
      TR::Register *indexReg = intOrLongClobberEvaluate(offsetNode, getNodeIs64Bit(offsetNode, cg) , cg);
      TR::Register *countReg = cg->evaluate(countNode);
      TR::Register *endReg = cg->allocateRegister();
      TR::Register *hashReg = cg->allocateRegister();
      TR::Register *tempReg = cg->allocateRegister();
      TR::Register *xmm0 = cg->allocateRegister(TR_FPR);
      TR::Register *xmm1 = cg->allocateRegister(TR_FPR);
      TR::Register *xmm2 = cg->allocateRegister(TR_FPR);

      generateLabelInstruction(LABEL, node, startLabel, cg);
      generateRegRegInstruction(MOVRegReg(), node, endReg, countReg, cg);
      generateRegRegInstruction(ADDRegReg(), node, endReg, indexReg, cg);
      generateRegRegInstruction(XORRegReg(), node, hashReg, hashReg, cg);
      generateRegImmInstruction(CMPRegImm4(), node, countReg, 0x4, cg);
      cg->stopUsingRegister(countReg);
      generateLabelInstruction(JL4, node, SerialLabel, cg);

      // SSE version
      // xmm2 = load 4 chars and expand into 4 int
      generateRegImmInstruction(SUBRegImms(), node, endReg, 3, cg);
      int vector1[4] = {923521, 923521, 923521, 923521};
      TR::IA32ConstantDataSnippet *vector1Snippet   = cg->findOrCreate16ByteConstant(node, vector1);
      TR::MemoryReference       *vector1MR = generateX86MemoryReference(vector1Snippet, cg);
      generateRegMemInstruction(MOVAPSRegMem, node, xmm0, vector1MR, cg);
      generateRegRegInstruction(XORPSRegReg, node, xmm1, xmm1, cg);

      generateLabelInstruction(LABEL, node, SSELabel, cg);
      generateRegMemInstruction(MOVSDRegMem,
                                node,
                                xmm2,
                                generateX86MemoryReference(valueReg, indexReg, 1, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg),
                                cg);
      // xmm1 = xmm1 * xmm0
      generateRegRegInstruction(PMULLD, node, xmm1, xmm0, cg);
      // movzxwd xmm2, xmm2
      generateRegRegInstruction(PMOVZXWD, node, xmm2, xmm2, cg);
      // xmm1 = xmm1 + xmm2
      generateRegRegInstruction(PADDD, node, xmm1, xmm2, cg);

      //    i = i + 4;
      //    cmp i, end -3
      //    jle SSEloop
      generateRegImmInstruction(ADDRegImms(), node, indexReg, 4, cg);
      generateRegRegInstruction(CMPRegReg(), node, indexReg, endReg, cg);
      generateLabelInstruction(JL4, node, SSELabel, cg);

      // xmm0 = load 16 bytes align [31^3, 31^2, 31, 1]
      // xmm1 = xmm1 * xmm0      value containst [a0, a1, a2, a3]
      // xmm0 = xmm1
      // xmm0 = xmm0 >> 64 bits
      // xmm1 = xmm1 + xmm0       reduce add [a0+a2, a1+a3, .., ...]
      // xmm0 = xmm1
      // xmm0 = xmm0 >> 32 bits
      // xmm1 = xmm1 + xmm0       reduce add [a0+a2 + a1+a3, .., .., ..]
      // movd xmm1, GPR1

      generateLabelInstruction(LABEL, node, SSEReduceLabel, cg);
      int vector2[4] = {29791, 961, 31, 1};
      TR::IA32ConstantDataSnippet *vector2Snippet   = cg->findOrCreate16ByteConstant(node, vector2);
      TR::MemoryReference       *vector2MR = generateX86MemoryReference(vector2Snippet, cg);
      generateRegMemInstruction(MOVAPSRegMem, node, xmm0, vector2MR, cg);
      generateRegRegInstruction(PMULLD, node, xmm1, xmm0, cg);
      generateRegRegInstruction(MOVAPDRegReg, node, xmm0, xmm1, cg);
      generateRegImmInstruction(PSRLDQRegImm1, node, xmm0, 0x8, cg);
      generateRegRegInstruction(PADDD, node, xmm1, xmm0, cg);
      generateRegRegInstruction(MOVAPDRegReg, node, xmm0, xmm1, cg);
      generateRegImmInstruction(PSRLDQRegImm1, node, xmm0, 0x4, cg);
      generateRegRegInstruction(PADDD, node, xmm1, xmm0, cg);
      generateRegRegInstruction(MOVDReg4Reg, node, hashReg, xmm1, cg);
      generateRegImmInstruction(ADDRegImms(), node, endReg, 3, cg);

      // Serial label
      generateLabelInstruction(LABEL, node, SerialLabel, cg);
      generateRegRegInstruction(CMPRegReg(), node, indexReg, endReg, cg);
      generateLabelInstruction(JGE4, node, endLabel, cg);
      generateLabelInstruction(LABEL, node, SerialLoopLabel, cg);
      generateRegRegInstruction(MOV4RegReg, node, tempReg, hashReg, cg);
      generateRegImmInstruction(SHL4RegImm1, node, hashReg, 5, cg);
      generateRegRegInstruction(SUB4RegReg, node, hashReg, tempReg, cg);
      generateRegMemInstruction(MOVZXReg4Mem2,
                                node,
                                tempReg,
                                generateX86MemoryReference(valueReg, indexReg, 1, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg),
                                cg);
      generateRegRegInstruction(ADD4RegReg, node, hashReg, tempReg, cg);
      generateRegInstruction(INCReg(TR::Compiler->target.is64Bit()), node, indexReg, cg);
      generateRegRegInstruction(CMPRegReg(), node, indexReg, endReg, cg);
      generateLabelInstruction(JL4, node, SerialLoopLabel, cg);

      TR::RegisterDependencyConditions  *dependencies = generateRegisterDependencyConditions((uint8_t)0, 6, cg);
      dependencies->addPostCondition(valueReg, TR::RealRegister::NoReg, cg);
      dependencies->addPostCondition(indexReg, TR::RealRegister::NoReg, cg);
      dependencies->addPostCondition(countReg, TR::RealRegister::NoReg, cg);
      dependencies->addPostCondition(endReg, TR::RealRegister::NoReg, cg);
      dependencies->addPostCondition(hashReg, TR::RealRegister::NoReg, cg);
      dependencies->addPostCondition(tempReg, TR::RealRegister::NoReg, cg);
      generateLabelInstruction(LABEL, node, endLabel, dependencies, cg);
      node->setRegister(hashReg);
      cg->decReferenceCount(valueNode);
      cg->decReferenceCount(offsetNode);
      cg->decReferenceCount(countNode);
      cg->stopUsingRegister(tempReg);
      cg->stopUsingRegister(indexReg);
      cg->stopUsingRegister(valueReg);
      cg->stopUsingRegister(endReg);
      cg->stopUsingRegister(xmm0);
      cg->stopUsingRegister(xmm1);
      cg->stopUsingRegister(xmm2);
      return true;
      }
   return false;
   }

static void
inlineDoubleWordCASSupportedCommon(
      TR::Node *node,
      int32_t referenceOffset,
      int32_t intArgOffset,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node *objectStoreChild = node->getFirstChild();
   TR::Register *objectReg = cg->evaluate(objectStoreChild);

   TR_ASSERT( TR::Compiler->target.is64Bit() && !comp->useCompressedPointers(), "this is required on 64bit uncompressed only");

   int32_t offset = referenceOffset;
   if (intArgOffset < referenceOffset) offset = intArgOffset;

   TR::Register *resultReg = cg->allocateRegister();

   // check for 16 byte alignment of the store
   TR::MemoryReference *mr = generateX86MemoryReference(objectReg, offset, cg);
   generateRegMemInstruction(LEARegMem(), node, resultReg, mr, cg);
   generateRegImmInstruction(TESTRegImm4(), node, resultReg, 0xf, cg);
   generateRegImmInstruction(MOVRegImm4(), node, resultReg, 0, cg);
   generateRegInstruction(SETE1Reg, node, resultReg, cg);

   node->setRegister(resultReg);
   cg->decReferenceCount(objectStoreChild);
   }

static bool
getNodeIs64Bit(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   /* This function is intended to allow existing 32-bit instruction selection code
    * to be reused, almost unchanged, to do the corresponding 64-bit logic on AMD64.
    * It compiles away to nothing on IA32, thus preserving performance and code size
    * on IA32, while allowing the logic to be generalized to suit AMD64.
    *
    * Don't use this function for 64-bit logic on IA32; instead, either (1) use
    * separate logic, or (2) use a different test for 64-bitness.  Usually this is
    * not a hinderance, because 64-bit code on IA32 uses register pairs and other
    * things that are totally different from their 32-bit counterparts.
    */

   TR_ASSERT(TR::Compiler->target.is64Bit() || node->getSize() <= 4, "64-bit nodes on 32-bit platforms shouldn't use getNodeIs64Bit");
   return TR::Compiler->target.is64Bit() && node->getSize() > 4;
   }

static
TR::Register *intOrLongClobberEvaluate(
      TR::Node *node,
      bool nodeIs64Bit,
      TR::CodeGenerator *cg)
   {
   if (nodeIs64Bit)
      {
      TR_ASSERT(getNodeIs64Bit(node, cg), "nodeIs64Bit must be consistent with node size");
      return cg->longClobberEvaluate(node);
      }
   else
      {
      TR_ASSERT(!getNodeIs64Bit(node, cg), "nodeIs64Bit must be consistent with node size");
      return cg->intClobberEvaluate(node);
      }
   }

static void
inlineDoubleWordCASCommon(
      TR::Node *node,
      int32_t referenceOffset,
      int32_t intArgOffset,
      TR::CodeGenerator *cg)
   {
   TR::Node *objectStoreChild = node->getSecondChild();
   TR::Node *existingObjValChild = node->getChild(3);
   TR::Node *newObjValChild = node->getChild(2);
   TR::Node *existingBoolValChild = node->getChild(5);
   TR::Node *newBoolValChild = node->getChild(4);

   int32_t offset = referenceOffset;
   if (intArgOffset < referenceOffset) offset = intArgOffset;

   TR::RegisterDependencyConditions  *deps;

   TR::Register *objectReg = cg->evaluate(objectStoreChild);
   TR::Register *eaxReal = cg->allocateRegister();
   TR::Register *edxReal = NULL;
   TR::Register *existingObjValReg = cg->evaluate(existingObjValChild);
   TR::Register *existingBoolValReg = cg->evaluate(existingBoolValChild);
   TR::Register *newObjValReg;
   TR::Register *newBoolValReg;

   TR::LabelSymbol *lockLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *endLabel = generateLabelSymbol(cg);

   lockLabel->setStartInternalControlFlow();
   endLabel->setEndInternalControlFlow();

   generateLabelInstruction(LABEL, node, lockLabel, cg);

   TR::MemoryReference *mr = generateX86MemoryReference(objectReg, offset, cg);

   TR_X86ScratchRegisterManager *scratchRegisterManagerForRealTime = NULL;
   TR::Register *storeAddressRegForRealTime = NULL;
   TR::Compilation *comp = cg->comp();

   if (comp->getOptions()->realTimeGC())
      {
      scratchRegisterManagerForRealTime = cg->generateScratchRegisterManager();

      // If reference is unresolved, need to resolve it right here before the barrier starts
      // Otherwise, we could get stopped during the resolution and that could invalidate any tests we would have performend
      //   beforehand
      // For simplicity, just evaluate the store address into storeAddressRegForRealTime right now
      storeAddressRegForRealTime = scratchRegisterManagerForRealTime->findOrCreateScratchRegister();
      generateRegMemInstruction(LEARegMem(), node, storeAddressRegForRealTime, mr, cg);
      if (node->getSymbolReference()->isUnresolved())
         {
         TR::TreeEvaluator::padUnresolvedDataReferences(node, *node->getSymbolReference(), cg);

         // storeMR was created against a (i)wrtbar node which is a store.  The unresolved data snippet that
         //  was created set the checkVolatility bit based on that node being a store.  Since the resolution
         //  is now going to occur on a LEA instruction, which does not require any memory fence and hence
         //  no volatility check, we need to clear that "store" ness of the unresolved data snippet
         TR::UnresolvedDataSnippet *snippet = mr->getUnresolvedDataSnippet();
         if (snippet)
            snippet->resetUnresolvedStore();
         }
      }

   int32_t extraDeps = 0;

   if (scratchRegisterManagerForRealTime)
      extraDeps += scratchRegisterManagerForRealTime->numAvailableRegisters();

   if (storeAddressRegForRealTime != NULL)
      extraDeps++;


   if (TR::Compiler->target.is64Bit() &&
       comp->useCompressedPointers())
      {
      deps = generateRegisterDependencyConditions(3+extraDeps,
                                                  3+extraDeps,
                                                  cg);
      deps->addPreCondition(eaxReal,
                            TR::RealRegister::eax,
                            cg);

      deps->addPostCondition(eaxReal,
                             TR::RealRegister::eax,
                             cg);

      if (intArgOffset < referenceOffset)
         {
         generateRegRegInstruction(MOVRegReg(), node, eaxReal, existingObjValReg, cg);
         }
      else
         {
         generateRegRegInstruction(MOVRegReg(), node, eaxReal, existingBoolValReg, cg);
         }

      deps->addPreCondition(existingBoolValReg,
                                TR::RealRegister::NoReg,
                                cg);
      deps->addPostCondition(existingBoolValReg,
                                TR::RealRegister::NoReg,
                                cg);
      deps->addPreCondition(existingObjValReg,
                                TR::RealRegister::NoReg,
                                cg);
      deps->addPostCondition(existingObjValReg,
                                TR::RealRegister::NoReg,
                                cg);

      generateRegImmInstruction(SHLRegImm1(), node, eaxReal, 32, cg);
      if (intArgOffset < referenceOffset)
         {
         generateRegRegInstruction(ORRegReg(), node, eaxReal, existingBoolValReg, cg);

         newObjValReg = intOrLongClobberEvaluate(newObjValChild, getNodeIs64Bit(newObjValChild,cg), cg);
         newBoolValReg = cg->evaluate(newBoolValChild);

         generateRegImmInstruction(SHLRegImm1(), node, newObjValReg, 32, cg);
         generateRegRegInstruction(ORRegReg(), node, newObjValReg, newBoolValReg, cg);
         }
      else
         {
         generateRegRegInstruction(ORRegReg(), node, eaxReal, existingObjValReg, cg);

         newBoolValReg = intOrLongClobberEvaluate(newBoolValChild, getNodeIs64Bit(newBoolValChild,cg), cg);
         newObjValReg = cg->evaluate(newObjValChild);

         generateRegImmInstruction(SHLRegImm1(), node, newBoolValReg, 32, cg);
         generateRegRegInstruction(ORRegReg(), node, newBoolValReg, newObjValReg, cg);
         }
      }
   else
      {
      deps = generateRegisterDependencyConditions(6+extraDeps,
                                                  6+extraDeps,
                                                  cg);

      edxReal = cg->allocateRegister();

      deps->addPreCondition(edxReal,
                            TR::RealRegister::edx,
                            cg);
      deps->addPreCondition(eaxReal,
                            TR::RealRegister::eax,
                            cg);

      deps->addPostCondition(edxReal,
                            TR::RealRegister::edx,
                            cg);
      deps->addPostCondition(eaxReal,
                            TR::RealRegister::eax,
                            cg);


      if (TR::Compiler->target.is64Bit())
         {
         newBoolValReg = intOrLongClobberEvaluate(newBoolValChild, getNodeIs64Bit(newBoolValChild,cg), cg);
         newObjValReg = cg->evaluate(newObjValChild);
         }
      else
         {
         newBoolValReg = cg->evaluate(newBoolValChild);
         newObjValReg = cg->evaluate(newObjValChild);
         }

      if (intArgOffset < referenceOffset)
         {
         generateRegRegInstruction(MOVRegReg(), node, edxReal, existingObjValReg, cg);
         generateRegRegInstruction(MOVRegReg(), node, eaxReal, existingBoolValReg, cg);

         deps->addPreCondition(newObjValReg,
                               TR::RealRegister::ecx,
                               cg);
         deps->addPreCondition(newBoolValReg,
                               TR::RealRegister::ebx,
                               cg);
         deps->addPostCondition(newObjValReg,
                                TR::RealRegister::ecx,
                                cg);
         deps->addPostCondition(newBoolValReg,
                                TR::RealRegister::ebx,
                                cg);
         deps->addPreCondition(existingObjValReg,
                               TR::RealRegister::NoReg,
                               cg);
         deps->addPreCondition(existingBoolValReg,
                               TR::RealRegister::NoReg,
                               cg);
         deps->addPostCondition(existingObjValReg,
                                TR::RealRegister::NoReg,
                                cg);
         deps->addPostCondition(existingBoolValReg,
                                TR::RealRegister::NoReg,
                                cg);
         }
      else
         {
         generateRegRegInstruction(MOVRegReg(), node, eaxReal, existingObjValReg, cg);
         generateRegRegInstruction(MOVRegReg(), node, edxReal, existingBoolValReg, cg);

         deps->addPreCondition(newObjValReg,
                               TR::RealRegister::ebx,
                               cg);
         deps->addPreCondition(newBoolValReg,
                               TR::RealRegister::ecx,
                               cg);
         deps->addPostCondition(newObjValReg,
                                TR::RealRegister::ebx,
                                cg);
         deps->addPostCondition(newBoolValReg,
                                TR::RealRegister::ecx,
                                cg);
         deps->addPreCondition(existingObjValReg,
                               TR::RealRegister::NoReg,
                               cg);
         deps->addPreCondition(existingBoolValReg,
                               TR::RealRegister::NoReg,
                               cg);
         deps->addPostCondition(existingObjValReg,
                                TR::RealRegister::NoReg,
                                cg);
         deps->addPostCondition(existingBoolValReg,
                                TR::RealRegister::NoReg,
                                cg);
         }
      }

   if (scratchRegisterManagerForRealTime)
      scratchRegisterManagerForRealTime->addScratchRegistersToDependencyList(deps);

   deps->stopAddingConditions();

   // do the real-time write barrier after new object val child has been evaluated
   if (comp->getOptions()->realTimeGC())
      {
      TR::TreeEvaluator::VMwrtbarRealTimeWithoutStoreEvaluator(
         node,
         mr,
         storeAddressRegForRealTime,
         objectStoreChild,
         newObjValChild,
         NULL,
         scratchRegisterManagerForRealTime,
         cg);

      if (storeAddressRegForRealTime)
         mr = generateX86MemoryReference(storeAddressRegForRealTime, 0, cg);
      }

   TR_X86OpCodes cmpSwpOp=BADIA32Op;

   if (TR::Compiler->target.is64Bit())
      {
      if (comp->useCompressedPointers())
         {
         cmpSwpOp = TR::Compiler->target.isSMP() ? LCMPXCHG8MemReg : CMPXCHG8MemReg;
         }
      else
         {
         cmpSwpOp = TR::Compiler->target.isSMP() ? LCMPXCHG16BMem : CMPXCHG16BMem;
         if (intArgOffset < referenceOffset)
            generateRegImmInstruction(SHLRegImm1(), node, eaxReal, 32, cg);
         else
            generateRegImmInstruction(SHLRegImm1(), node, edxReal, 32, cg);
         generateRegImmInstruction(SHLRegImm1(), node, newBoolValReg, 32, cg);
         }
      }
   else
      {
      cmpSwpOp = TR::Compiler->target.isSMP() ? LCMPXCHG8BMem : CMPXCHG8BMem;
      }

   if (TR::Compiler->target.is64Bit() &&
       comp->useCompressedPointers())
      {
      if (intArgOffset < referenceOffset)
         {
         generateMemRegInstruction(cmpSwpOp, node, mr, newObjValReg, cg);
         }
      else
         {
         generateMemRegInstruction(cmpSwpOp, node, mr, newBoolValReg, cg);
         }
      }
   else
      generateMemInstruction(cmpSwpOp, node, mr, cg);

   generateRegImmInstruction(MOVRegImm4(), node, eaxReal, 0, cg);
   generateRegInstruction(SETE1Reg, node, eaxReal, cg);

   generateLabelInstruction(LABEL, node, endLabel, deps, cg);

   if (storeAddressRegForRealTime)
      scratchRegisterManagerForRealTime->reclaimScratchRegister(storeAddressRegForRealTime);

   if (edxReal)
      cg->stopUsingRegister(edxReal);


   if (TR::Compiler->target.is64Bit())
      {
      if (comp->useCompressedPointers())
         {
         if (intArgOffset < referenceOffset)
            {
            cg->stopUsingRegister(newObjValReg);
            }
         else
            {
            cg->stopUsingRegister(newBoolValReg);
            }
         }
      else
         {
         cg->stopUsingRegister(newBoolValReg);
         }
      }

   // Non-realtime: Generate a write barrier for this kind of object.
   //
   if (!comp->getOptions()->realTimeGC())
      {
      // We could insert a runtime test for whether the write actually succeeded or not.
      // However, since in practice it will almost always succeed we do not want to
      // penalize general runtime performance especially if it is still correct to do
      // a write barrier even if the store never actually happened.
      //
      // A branch
      //
      TR_X86ScratchRegisterManager *scratchRegisterManager = cg->generateScratchRegisterManager();

      TR::TreeEvaluator::VMwrtbarWithoutStoreEvaluator(
         node,
         objectStoreChild,
         newObjValChild,
         NULL,
         scratchRegisterManager,
         cg);
      }

   node->setRegister(eaxReal);

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(objectStoreChild);
   cg->decReferenceCount(existingObjValChild);
   cg->decReferenceCount(newObjValChild);
   cg->decReferenceCount(existingBoolValChild);
   cg->decReferenceCount(newBoolValChild);
   }

static void
inlineDoubleWordSet64CompressedCommon(
      TR::Node *node,
      int32_t referenceOffset,
      int32_t intArgOffset,
      TR::CodeGenerator *cg)
   {
   TR::Node *objectStoreChild = node->getSecondChild();
   TR::Node *newBoolValChild = node->getChild(3);
   TR::Node *newObjValChild = node->getChild(2);

   int32_t offset = referenceOffset;
   if (intArgOffset < referenceOffset) offset = intArgOffset;

   TR::RegisterDependencyConditions  *deps;

   TR::Register *objectReg = cg->evaluate(objectStoreChild);
   TR::Register *existingValReg = cg->allocateRegister();
   TR::Register *newObjValReg = intOrLongClobberEvaluate(newObjValChild, getNodeIs64Bit(newObjValChild, cg), cg);
   TR::Register *newBoolValReg = intOrLongClobberEvaluate(newBoolValChild, getNodeIs64Bit(newBoolValChild, cg), cg);

   TR::MemoryReference *lockMr = generateX86MemoryReference(objectReg, offset, cg);
   TR::MemoryReference *loadMr = generateX86MemoryReference(objectReg, offset, cg);

   TR_X86ScratchRegisterManager *scratchRegisterManagerForRealTime = NULL;
   TR::Register *storeAddressRegForRealTime = NULL;
   TR::Compilation *comp = cg->comp();

   if (comp->getOptions()->realTimeGC())
      {
      scratchRegisterManagerForRealTime = cg->generateScratchRegisterManager();

      // If reference is unresolved, need to resolve it right here before the barrier starts
      // Otherwise, we could get stopped during the resolution and that could invalidate any tests we would have performend
      //   beforehand
      // For simplicity, just evaluate the store address into storeAddressRegForRealTime right now
      storeAddressRegForRealTime = scratchRegisterManagerForRealTime->findOrCreateScratchRegister();
      generateRegMemInstruction(LEARegMem(), node, storeAddressRegForRealTime, lockMr, cg);
      if (node->getSymbolReference()->isUnresolved())
         {
         TR::TreeEvaluator::padUnresolvedDataReferences(node, *node->getSymbolReference(), cg);

         // storeMR was created against a (i)wrtbar node which is a store.  The unresolved data snippet that
         //  was created set the checkVolatility bit based on that node being a store.  Since the resolution
         //  is now going to occur on a LEA instruction, which does not require any memory fence and hence
         //  no volatility check, we need to clear that "store" ness of the unresolved data snippet
         TR::UnresolvedDataSnippet *snippet = lockMr->getUnresolvedDataSnippet();
         if (snippet)
            snippet->resetUnresolvedStore();
         }

      TR::TreeEvaluator::VMwrtbarRealTimeWithoutStoreEvaluator(
         node,
         lockMr,
         storeAddressRegForRealTime,
         objectStoreChild,
         newObjValChild,
         NULL,
         scratchRegisterManagerForRealTime,
         cg);

      if (storeAddressRegForRealTime)
         lockMr = generateX86MemoryReference(storeAddressRegForRealTime, 0, cg);
      }

   int32_t extraDeps = 0;

   if (scratchRegisterManagerForRealTime)
      extraDeps += scratchRegisterManagerForRealTime->numAvailableRegisters();

   if (storeAddressRegForRealTime != NULL)
      extraDeps++;

   deps = generateRegisterDependencyConditions(1+extraDeps,
                                               4+extraDeps,
                                               cg);

   deps->addPreCondition(existingValReg,
                         TR::RealRegister::eax,
                         cg);

   deps->addPostCondition(existingValReg,
                          TR::RealRegister::eax,
                          cg);

   deps->addPostCondition(newObjValReg,
                          TR::RealRegister::NoReg,
                          cg);

   deps->addPostCondition(newBoolValReg,
                          TR::RealRegister::NoReg,
                          cg);

   deps->addPostCondition(objectReg,
                          TR::RealRegister::NoReg,
                          cg);

   if (scratchRegisterManagerForRealTime)
      scratchRegisterManagerForRealTime->addScratchRegistersToDependencyList(deps);


   deps->stopAddingConditions();

   if (intArgOffset > referenceOffset)
      {
      generateRegImmInstruction(SHLRegImm1(), node, newBoolValReg, 32, cg);
      generateRegRegInstruction(ORRegReg(), node, newObjValReg, newBoolValReg, cg);
      }
   else
      {
      generateRegImmInstruction(SHLRegImm1(), node, newObjValReg, 32, cg);
      generateRegRegInstruction(ORRegReg(), node, newBoolValReg, newObjValReg, cg);
      }

   TR::LabelSymbol *lockLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *endLabel = generateLabelSymbol(cg);

   lockLabel->setStartInternalControlFlow();
   endLabel->setEndInternalControlFlow();

   generateLabelInstruction(LABEL, node, lockLabel, cg);

   generateRegMemInstruction(LRegMem(), node, existingValReg, loadMr, cg);

   if (intArgOffset > referenceOffset)
      {
      generateMemRegInstruction(TR::Compiler->target.isSMP() ? LCMPXCHG8MemReg : CMPXCHG8MemReg, node, lockMr, newObjValReg, deps, cg);
      }
   else
      {
      generateMemRegInstruction(TR::Compiler->target.isSMP() ? LCMPXCHG8MemReg : CMPXCHG8MemReg, node, lockMr, newBoolValReg, deps, cg);
      }

   generateLabelInstruction(JNE4, node, lockLabel, cg);

   generateLabelInstruction(LABEL, node, endLabel, deps, cg);

   if (storeAddressRegForRealTime)
      scratchRegisterManagerForRealTime->reclaimScratchRegister(storeAddressRegForRealTime);

   // Non-realtime: Generate a write barrier for this kind of object.
   //
   if (!comp->getOptions()->realTimeGC())
      {
      // We could insert a runtime test for whether the write actually succeeded or not.
      // However, since in practice it will almost always succeed we do not want to
      // penalize general runtime performance especially if it is still correct to do
      // a write barrier even if the store never actually happened.
      //
      // A branch
      //
      TR_X86ScratchRegisterManager *scratchRegisterManager = cg->generateScratchRegisterManager();

      TR::TreeEvaluator::VMwrtbarWithoutStoreEvaluator(
         node,
         objectStoreChild,
         newObjValChild,
         NULL,
         scratchRegisterManager,
         cg);
      }

   cg->stopUsingRegister(existingValReg);
   cg->stopUsingRegister(newObjValReg);
   cg->stopUsingRegister(newBoolValReg);

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(objectStoreChild);
   cg->decReferenceCount(newObjValChild);
   cg->decReferenceCount(newBoolValChild);
   }

static void
inlineDoubleWordSetCommon(
      TR::Node *node,
      int32_t referenceOffset,
      int32_t intArgOffset,
      TR::CodeGenerator *cg)
   {
   TR::Node *objectStoreChild = node->getSecondChild();
   TR::Node *newBoolValChild = node->getChild(3);
   TR::Node *newObjValChild = node->getChild(2);
   TR::Compilation *comp = cg->comp();

   int32_t offset = referenceOffset;
   if (intArgOffset < referenceOffset) offset = intArgOffset;

   TR::RegisterDependencyConditions  *deps;

   TR::Register *objectReg = cg->evaluate(objectStoreChild);
   TR::Register *existingObjValReg = cg->allocateRegister();
   TR::Register *existingBoolValReg = cg->allocateRegister();
   TR::Register *newObjValReg = cg->evaluate(newObjValChild);
   TR::Register *newBoolValReg = NULL;

   if (TR::Compiler->target.is64Bit())
      {
      newBoolValReg = intOrLongClobberEvaluate(newBoolValChild, getNodeIs64Bit(newBoolValChild, cg), cg);
      }
   else
      {
      newBoolValReg = cg->evaluate(newBoolValChild);
      }

   TR::MemoryReference *lockMr = generateX86MemoryReference(objectReg, offset, cg);
   TR::MemoryReference *loadFirstMr = generateX86MemoryReference(objectReg, offset, cg);
   TR::MemoryReference *loadSecondMr = generateX86MemoryReference(objectReg, offset+TR::Compiler->om.sizeofReferenceAddress(), cg);


   TR_X86ScratchRegisterManager *scratchRegisterManagerForRealTime = NULL;
   TR::Register *storeAddressRegForRealTime = NULL;

   if (comp->getOptions()->realTimeGC())
      {
      scratchRegisterManagerForRealTime = cg->generateScratchRegisterManager();

      // If reference is unresolved, need to resolve it right here before the barrier starts
      // Otherwise, we could get stopped during the resolution and that could invalidate any tests we would have performend
      //   beforehand
      // For simplicity, just evaluate the store address into storeAddressRegForRealTime right now
      storeAddressRegForRealTime = scratchRegisterManagerForRealTime->findOrCreateScratchRegister();
      generateRegMemInstruction(LEARegMem(), node, storeAddressRegForRealTime, lockMr, cg);
      if (node->getSymbolReference()->isUnresolved())
         {
         TR::TreeEvaluator::padUnresolvedDataReferences(node, *node->getSymbolReference(), cg);

         // storeMR was created against a (i)wrtbar node which is a store.  The unresolved data snippet that
         //  was created set the checkVolatility bit based on that node being a store.  Since the resolution
         //  is now going to occur on a LEA instruction, which does not require any memory fence and hence
         //  no volatility check, we need to clear that "store" ness of the unresolved data snippet
         TR::UnresolvedDataSnippet *snippet = lockMr->getUnresolvedDataSnippet();
         if (snippet)
            snippet->resetUnresolvedStore();
         }

      TR::TreeEvaluator::VMwrtbarRealTimeWithoutStoreEvaluator(
         node,
         lockMr,
         storeAddressRegForRealTime,
         objectStoreChild,
         newObjValChild,
         NULL,
         scratchRegisterManagerForRealTime,
         cg);

      if (storeAddressRegForRealTime)
         lockMr = generateX86MemoryReference(storeAddressRegForRealTime, 0, cg);
      }

   int32_t extraDeps = 0;

   if (scratchRegisterManagerForRealTime)
      extraDeps += scratchRegisterManagerForRealTime->numAvailableRegisters();

   if (storeAddressRegForRealTime != NULL)
      extraDeps++;

   deps = generateRegisterDependencyConditions(4+extraDeps,
                                               5+extraDeps,
                                               cg);

   if (intArgOffset < referenceOffset)
      {
      deps->addPreCondition(existingObjValReg,
                            TR::RealRegister::edx,
                            cg);
      deps->addPreCondition(existingBoolValReg,
                            TR::RealRegister::eax,
                            cg);
      deps->addPreCondition(newObjValReg,
                            TR::RealRegister::ecx,
                            cg);
      deps->addPreCondition(newBoolValReg,
                            TR::RealRegister::ebx,
                            cg);
      deps->addPostCondition(existingObjValReg,
                             TR::RealRegister::edx,
                             cg);
      deps->addPostCondition(existingBoolValReg,
                             TR::RealRegister::eax,
                             cg);
      deps->addPostCondition(newObjValReg,
                             TR::RealRegister::ecx,
                             cg);
      deps->addPostCondition(newBoolValReg,
                             TR::RealRegister::ebx,
                             cg);
      }
   else
      {
      deps->addPreCondition(existingObjValReg,
                            TR::RealRegister::eax,
                            cg);
      deps->addPreCondition(existingBoolValReg,
                            TR::RealRegister::edx,
                            cg);
      deps->addPreCondition(newObjValReg,
                            TR::RealRegister::ebx,
                            cg);
      deps->addPreCondition(newBoolValReg,
                            TR::RealRegister::ecx,
                            cg);
      deps->addPostCondition(existingObjValReg,
                             TR::RealRegister::eax,
                             cg);
      deps->addPostCondition(existingBoolValReg,
                             TR::RealRegister::edx,
                             cg);
      deps->addPostCondition(newObjValReg,
                             TR::RealRegister::ebx,
                             cg);
      deps->addPostCondition(newBoolValReg,
                             TR::RealRegister::ecx,
                             cg);
      }

   deps->addPostCondition(objectReg,
                          TR::RealRegister::NoReg,
                          cg);

   if (scratchRegisterManagerForRealTime)
      scratchRegisterManagerForRealTime->addScratchRegistersToDependencyList(deps);

   deps->stopAddingConditions();

   TR::LabelSymbol *lockLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *endLabel = generateLabelSymbol(cg);

   lockLabel->setStartInternalControlFlow();
   endLabel->setEndInternalControlFlow();

   TR_X86OpCodes cmpSwpOp=BADIA32Op;

   if (TR::Compiler->target.is64Bit())
      {
      TR_ASSERT(!comp->useCompressedPointers(), "You can't use compressed pointers here, see compressed pointers version of this function");
      cmpSwpOp = TR::Compiler->target.isSMP() ? LCMPXCHG16BMem : CMPXCHG16BMem;
      generateRegImmInstruction(SHLRegImm1(), node, newBoolValReg, 32, cg);
      }
   else
      {
      cmpSwpOp = TR::Compiler->target.isSMP() ? LCMPXCHG8BMem : CMPXCHG8BMem;
      }

   generateLabelInstruction(LABEL, node, lockLabel, cg);

   generateRegMemInstruction(LRegMem(), node, existingObjValReg, loadFirstMr, cg);
   generateRegMemInstruction(LRegMem(), node, existingBoolValReg, loadSecondMr, cg);
   generateMemInstruction(cmpSwpOp, node, lockMr, cg);

   generateLabelInstruction(JNE4, node, lockLabel, cg);

   generateLabelInstruction(LABEL, node, endLabel, deps, cg);

   if (storeAddressRegForRealTime)
      scratchRegisterManagerForRealTime->reclaimScratchRegister(storeAddressRegForRealTime);

   // Non-realtime: Generate a write barrier for this kind of object.
   //
   if (!comp->getOptions()->realTimeGC())
      {
      // We could insert a runtime test for whether the write actually succeeded or not.
      // However, since in practice it will almost always succeed we do not want to
      // penalize general runtime performance especially if it is still correct to do
      // a write barrier even if the store never actually happened.
      //
      // A branch
      //
      TR_X86ScratchRegisterManager *scratchRegisterManager = cg->generateScratchRegisterManager();

      TR::TreeEvaluator::VMwrtbarWithoutStoreEvaluator(
         node,
         objectStoreChild,
         newObjValChild,
         NULL,
         scratchRegisterManager,
         cg);
      }

   cg->stopUsingRegister(existingObjValReg);
   cg->stopUsingRegister(existingBoolValReg);
   if (TR::Compiler->target.is64Bit())
      cg->stopUsingRegister(newBoolValReg);

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(objectStoreChild);
   cg->decReferenceCount(newObjValChild);
   cg->decReferenceCount(newBoolValChild);
   }

static bool inlineAtomicMarkableReference_doubleWordCASSupported(TR::Node *node,
                                                                 TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   if (TR::Compiler->target.is64Bit() && !comp->useCompressedPointers() &&
       !cg->getX86ProcessorInfo().supportsCMPXCHG16BInstruction())
      return false;

   char *classSignature = "Ljava/util/concurrent/atomic/AtomicMarkableReference$ReferenceBooleanPair;";
   int32_t boolOffset = fej9->getInstanceFieldOffsetIncludingHeader(
      classSignature, "bit", "Z", comp->getCurrentMethod());
   int32_t referenceOffset = fej9->getInstanceFieldOffsetIncludingHeader(
      classSignature, "reference", "Ljava/lang/Object;", comp->getCurrentMethod());

   inlineDoubleWordCASSupportedCommon(node, referenceOffset, boolOffset, cg);

   return true;
   }

static bool inlineAtomicMarkableReference_doubleWordCAS(TR::Node *node,
                                                        TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   if (TR::Compiler->target.is32Bit() &&
       !cg->getX86ProcessorInfo().supportsCMPXCHG8BInstruction())
      return false;

   if (TR::Compiler->target.is64Bit() && !comp->useCompressedPointers() &&
       !cg->getX86ProcessorInfo().supportsCMPXCHG16BInstruction())
      return false;

   char *classSignature = "Ljava/util/concurrent/atomic/AtomicMarkableReference$ReferenceBooleanPair;";
   int32_t boolOffset = fej9->getInstanceFieldOffsetIncludingHeader(
      classSignature, "bit", "Z", comp->getCurrentMethod());
   int32_t referenceOffset = fej9->getInstanceFieldOffsetIncludingHeader(
      classSignature, "reference", "Ljava/lang/Object;", comp->getCurrentMethod());

   inlineDoubleWordCASCommon(node, referenceOffset, boolOffset, cg);

   return true;
   }

static bool inlineAtomicMarkableReference_doubleWordSet(TR::Node *node,
                                                        TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   if (TR::Compiler->target.is32Bit() &&
       !cg->getX86ProcessorInfo().supportsCMPXCHG8BInstruction())
      return false;

   if (TR::Compiler->target.is64Bit() && !comp->useCompressedPointers() &&
       !cg->getX86ProcessorInfo().supportsCMPXCHG16BInstruction())
      return false;

   char *classSignature = "Ljava/util/concurrent/atomic/AtomicMarkableReference$ReferenceBooleanPair;";
   int32_t boolOffset = fej9->getInstanceFieldOffsetIncludingHeader(
      classSignature, "bit", "Z", comp->getCurrentMethod());
   int32_t referenceOffset = fej9->getInstanceFieldOffsetIncludingHeader(
      classSignature, "reference", "Ljava/lang/Object;", comp->getCurrentMethod());

   if (TR::Compiler->target.is64Bit() &&
       comp->useCompressedPointers())
      inlineDoubleWordSet64CompressedCommon(node, referenceOffset, boolOffset, cg);
   else
      inlineDoubleWordSetCommon(node, referenceOffset, boolOffset, cg);

   return true;
   }

static bool inlineAtomicStampedReference_doubleWordCASSupported(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   if (TR::Compiler->target.is64Bit() && !comp->useCompressedPointers() &&
       !cg->getX86ProcessorInfo().supportsCMPXCHG16BInstruction())
      return false;

   char *classSignature = "Ljava/util/concurrent/atomic/AtomicStampedReference$ReferenceIntegerPair;";
   int32_t intOffset = fej9->getInstanceFieldOffsetIncludingHeader(
      classSignature, "integer", "I", comp->getCurrentMethod());
   int32_t referenceOffset = fej9->getInstanceFieldOffsetIncludingHeader(
      classSignature, "reference", "Ljava/lang/Object;", comp->getCurrentMethod());

   inlineDoubleWordCASSupportedCommon(node, referenceOffset, intOffset, cg);

   return true;
   }

static bool inlineAtomicStampedReference_doubleWordCAS(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   if (TR::Compiler->target.is32Bit() &&
       !cg->getX86ProcessorInfo().supportsCMPXCHG8BInstruction())
      return false;

   if (TR::Compiler->target.is64Bit() && !comp->useCompressedPointers() &&
       !cg->getX86ProcessorInfo().supportsCMPXCHG16BInstruction())
      return false;

   char *classSignature = "Ljava/util/concurrent/atomic/AtomicStampedReference$ReferenceIntegerPair;";
   int32_t intOffset = fej9->getInstanceFieldOffsetIncludingHeader(
      classSignature, "integer", "I", comp->getCurrentMethod());
   int32_t referenceOffset = fej9->getInstanceFieldOffsetIncludingHeader(
      classSignature, "reference", "Ljava/lang/Object;", comp->getCurrentMethod());

   inlineDoubleWordCASCommon(node, referenceOffset, intOffset, cg);

   return true;
   }

static bool
inlineAtomicStampedReference_doubleWordSet(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   if (TR::Compiler->target.is32Bit() &&
       !cg->getX86ProcessorInfo().supportsCMPXCHG8BInstruction())
      return false;

   if (TR::Compiler->target.is64Bit() && !comp->useCompressedPointers() &&
       !cg->getX86ProcessorInfo().supportsCMPXCHG16BInstruction())
      return false;

   char *classSignature = "Ljava/util/concurrent/atomic/AtomicStampedReference$ReferenceIntegerPair;";
   int32_t intOffset = fej9->getInstanceFieldOffsetIncludingHeader(
      classSignature, "integer", "I", comp->getCurrentMethod());
   int32_t referenceOffset = fej9->getInstanceFieldOffsetIncludingHeader(
      classSignature, "reference", "Ljava/lang/Object;", comp->getCurrentMethod());

   if (TR::Compiler->target.is64Bit() &&
       comp->useCompressedPointers())
      inlineDoubleWordSet64CompressedCommon(node, referenceOffset, intOffset, cg);
   else
      inlineDoubleWordSetCommon(node, referenceOffset, intOffset, cg);

   return true;
   }

// Note that this function must have behaviour consistent with the OMR function
// willNotInlineCompareAndSwapNative in omr/compiler/x/codegen/OMRCodeGenerator.cpp
static bool
inlineCompareAndSwapNative(
      TR::Node *node,
      int8_t size,
      bool isObject,
      TR::CodeGenerator *cg)
   {
   TR::Node *firstChild    = node->getFirstChild();
   TR::Node *objectChild   = node->getSecondChild();
   TR::Node *offsetChild   = node->getChild(2);
   TR::Node *oldValueChild = node->getChild(3);
   TR::Node *newValueChild = node->getChild(4);
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   TR_X86OpCodes op;

   TR::SymbolReference *callSymRef = node->getSymbolReference();
   TR::MethodSymbol *methodSymbol = callSymRef->getSymbol()->castToMethodSymbol();

   if (TR::Compiler->om.canGenerateArraylets() && !node->isUnsafeGetPutCASCallOnNonArray())
      return false;

   static char *disableCASInlining = feGetEnv("TR_DisableCASInlining");

   if (disableCASInlining /* || comp->useCompressedPointers() */)
      return false;

   // size = 4 --> CMPXCHG4
   // size = 8 --> if 64-bit -> CMPXCHG8
   //              else if proc supports CMPXCHG8B -> CMPXCHG8B
   //              else return false
   //
   // Do this early so we can return early without additional evaluations.
   //
   if (size == 4)
      {
      op = LCMPXCHG4MemReg;
      }
   else if (size == 8 && TR::Compiler->target.is64Bit())
      {
      op = LCMPXCHG8MemReg;
      }
   else
      {
      if (!cg->getX86ProcessorInfo().supportsCMPXCHG8BInstruction())
         return false;

      op = LCMPXCHG8BMem;
      }

   // In Java9 the sun.misc.Unsafe JNI methods have been moved to jdk.internal,
   // with a set of wrappers remaining in sun.misc to delegate to the new package.
   // We can be called in this function for the wrappers (which we will
   // not be converting to assembly), the new jdk.internal JNI methods or the
   // Java8 sun.misc JNI methods (both of which we will convert). We can
   // differentiate between these cases by testing with isNative() on the method.
   if (!methodSymbol->isNative())
      return false;

   cg->recursivelyDecReferenceCount(firstChild);

   TR::Register *objectReg = cg->evaluate(objectChild);

   TR::Register *offsetReg = NULL;
   int32_t      offset    = 0;

   if (offsetChild->getOpCode().isLoadConst() && !offsetChild->getRegister() && IS_32BIT_SIGNED(offsetChild->getLongInt()))
      {
      offset = (int32_t)(offsetChild->getLongInt());
      }
   else
      {
      offsetReg = cg->evaluate(offsetChild);

      // Assume that the offset is positive and not pathologically large (i.e., > 2^31).
      //
      if (TR::Compiler->target.is32Bit())
         offsetReg = offsetReg->getLowOrder();
      }
   cg->decReferenceCount(offsetChild);

   TR::MemoryReference *mr;

   if (offsetReg)
      mr = generateX86MemoryReference(objectReg, offsetReg, 0, cg);
   else
      mr = generateX86MemoryReference(objectReg, offset, cg);

   bool bumpedRefCount = false;
   TR::Node *translatedNode = newValueChild;
   if (comp->useCompressedPointers() &&
       isObject &&
       (newValueChild->getDataType() != TR::Address))
      {
      bool usingLowMemHeap = false;
      bool useShiftedOffsets = (TR::Compiler->om.compressedReferenceShiftOffset() != 0);
      bool usingCompressedPointers = false;

      if (TR::Compiler->vm.heapBaseAddress() == 0 ||
          newValueChild->isNull())
         usingLowMemHeap = true;

      translatedNode = newValueChild;
      if (translatedNode->getOpCode().isConversion())
         translatedNode = translatedNode->getFirstChild();
      if (translatedNode->getOpCode().isRightShift()) // optional
         translatedNode = translatedNode->getFirstChild();

      if (translatedNode->getOpCode().isSub() || usingLowMemHeap)
         usingCompressedPointers = true;

      translatedNode = newValueChild;
      if (usingCompressedPointers)
         {
         if (!usingLowMemHeap || useShiftedOffsets)
            {
            while ((translatedNode->getNumChildren() > 0) &&
                   (translatedNode->getOpCodeValue() != TR::a2l))
               translatedNode = translatedNode->getFirstChild();

            if (translatedNode->getOpCodeValue() == TR::a2l)
               translatedNode = translatedNode->getFirstChild();

            // this is required so that different registers are
            // allocated for the actual store and translated values
            bumpedRefCount = true;
            translatedNode->incReferenceCount();
            }
         }
      }

   TR::Register *newValueRegister = cg->evaluate(newValueChild);

   TR::Register *oldValueRegister = (size == 8) ?
                                      cg->longClobberEvaluate(oldValueChild) : cg->intClobberEvaluate(oldValueChild);
   bool killOldValueRegister = (oldValueChild->getReferenceCount() > 1) ? true : false;
   cg->decReferenceCount(oldValueChild);

   TR::RegisterDependencyConditions  *deps;
   TR_X86ScratchRegisterManager *scratchRegisterManagerForRealTime = NULL;
   TR::Register *storeAddressRegForRealTime = NULL;

   if (comp->getOptions()->realTimeGC() && isObject)
      {
      scratchRegisterManagerForRealTime = cg->generateScratchRegisterManager();

      // If reference is unresolved, need to resolve it right here before the barrier starts
      // Otherwise, we could get stopped during the resolution and that could invalidate any tests we would have performend
      //   beforehand
      // For simplicity, just evaluate the store address into storeAddressRegForRealTime right now
      storeAddressRegForRealTime = scratchRegisterManagerForRealTime->findOrCreateScratchRegister();
      generateRegMemInstruction(LEARegMem(), node, storeAddressRegForRealTime, mr, cg);
      if (node->getSymbolReference()->isUnresolved())
         {
         TR::TreeEvaluator::padUnresolvedDataReferences(node, *node->getSymbolReference(), cg);

         // storeMR was created against a (i)wrtbar node which is a store.  The unresolved data snippet that
         //  was created set the checkVolatility bit based on that node being a store.  Since the resolution
         //  is now going to occur on a LEA instruction, which does not require any memory fence and hence
         //  no volatility check, we need to clear that "store" ness of the unresolved data snippet
         TR::UnresolvedDataSnippet *snippet = mr->getUnresolvedDataSnippet();
         if (snippet)
            snippet->resetUnresolvedStore();
         }

      TR::TreeEvaluator::VMwrtbarRealTimeWithoutStoreEvaluator(
         node,
         mr,
         storeAddressRegForRealTime,
         objectChild,
         translatedNode,
         NULL,
         scratchRegisterManagerForRealTime,
         cg);
      }

   TR::MemoryReference *cmpxchgMR = mr;

   if (op == LCMPXCHG8BMem)
      {
      int numDeps = 4;
      if (storeAddressRegForRealTime != NULL)
         {
         numDeps++;
         cmpxchgMR = generateX86MemoryReference(storeAddressRegForRealTime, 0, cg);
         }

      if (scratchRegisterManagerForRealTime)
         numDeps += scratchRegisterManagerForRealTime->numAvailableRegisters();

      deps = generateRegisterDependencyConditions(numDeps, numDeps, cg);
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

      generateMemInstruction(op, node, cmpxchgMR, deps, cg);
      }
   else
      {
      int numDeps = 1;
      if (storeAddressRegForRealTime != NULL)
         {
         numDeps++;
         cmpxchgMR = generateX86MemoryReference(storeAddressRegForRealTime, 0, cg);
         }

      if (scratchRegisterManagerForRealTime)
         numDeps += scratchRegisterManagerForRealTime->numAvailableRegisters();

      deps = generateRegisterDependencyConditions(numDeps, numDeps, cg);
      deps->addPreCondition(oldValueRegister, TR::RealRegister::eax, cg);
      deps->addPostCondition(oldValueRegister, TR::RealRegister::eax, cg);

      if (scratchRegisterManagerForRealTime)
         scratchRegisterManagerForRealTime->addScratchRegistersToDependencyList(deps);

      deps->stopAddingConditions();

      generateMemRegInstruction(op, node, cmpxchgMR, newValueRegister, deps, cg);
      }

   if (killOldValueRegister)
      cg->stopUsingRegister(oldValueRegister);

   if (storeAddressRegForRealTime)
      scratchRegisterManagerForRealTime->reclaimScratchRegister(storeAddressRegForRealTime);

   TR::Register *resultReg = cg->allocateRegister();
   generateRegInstruction(SETE1Reg, node, resultReg, cg);
   generateRegRegInstruction(MOVZXReg4Reg1, node, resultReg, resultReg, cg);

   // Non-realtime: Generate a write barrier for this kind of object.
   //
   if (!comp->getOptions()->realTimeGC() && isObject)
      {
      // We could insert a runtime test for whether the write actually succeeded or not.
      // However, since in practice it will almost always succeed we do not want to
      // penalize general runtime performance especially if it is still correct to do
      // a write barrier even if the store never actually happened.
      //
      // A branch
      //
      TR_X86ScratchRegisterManager *scratchRegisterManager = cg->generateScratchRegisterManager();

      TR::TreeEvaluator::VMwrtbarWithoutStoreEvaluator(
         node,
         objectChild,
         translatedNode,
         NULL,
         scratchRegisterManager,
         cg);
      }

   node->setRegister(resultReg);

   cg->decReferenceCount(newValueChild);
   cg->decReferenceCount(objectChild);
   if (bumpedRefCount)
      cg->decReferenceCount(translatedNode);

   return true;
   }

static TR::MemoryReference* obtainfieldRM(TR::Node* node, TR::MethodSymbol* method, TR::Register* receiver, TR::Register* index, TR::CodeGenerator* cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   char* className = NULL;
   char* fieldSig = NULL;
   uint8_t indexShift = 0;

   switch (method->getRecognizedMethod())
      {
      case TR::java_util_concurrent_atomic_AtomicBoolean_getAndSet:
         className = "Ljava/util/concurrent/atomic/AtomicBoolean;";
         fieldSig = "I";  // not a typo, the field is int
         break;
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicInteger_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicInteger_incrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicInteger_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndDecrement:
         className = "Ljava/util/concurrent/atomic/AtomicInteger;";
         fieldSig = "I";
         break;
      case TR::java_util_concurrent_atomic_AtomicLong_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicLong_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicLong_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicLong_incrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLong_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicLong_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLong_getAndDecrement:
         className = "Ljava/util/concurrent/atomic/AtomicLong;";
         fieldSig = "J";
         break;
      case TR::java_util_concurrent_atomic_AtomicReference_getAndSet:
         className = "Ljava/util/concurrent/atomic/AtomicReference;";
         fieldSig = "Ljava/lang/Object;";
         break;
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_incrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndAdd:
         className = "Ljava/util/concurrent/atomic/AtomicIntegerArray;";
         fieldSig = "[I";
         indexShift = 4;
         break;
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_incrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicLongArray_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicLongArray_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndAdd:
         className = "Ljava/util/concurrent/atomic/AtomicLongArray;";
         fieldSig = "[J";
         indexShift = 8;
         break;
      case TR::java_util_concurrent_atomic_AtomicReferenceArray_getAndSet:
         className = "Ljava/util/concurrent/atomic/AtomicReferenceArray;";
         fieldSig = "Ljava/lang/Object;";
         indexShift = TR::Compiler->target.is64Bit() ? 8 : 4;
         break;
      default:
         TR_ASSERT(0, "Unknown atomic operation method\n");
         return NULL;
      }

   // obtain object address
   TR_ResolvedMethod* owningMethod = node->getSymbolReference()->getOwningMethod(cg->comp());
   if(indexShift) // array
      {
      // get the address of array, and load the address to receiver; the original receiver address is not needed anymore
      intptrj_t fieldOffset = fej9->getInstanceFieldOffsetIncludingHeader(className, "array", fieldSig, owningMethod);
      generateRegMemInstruction(LRegMem(), node, receiver, generateX86MemoryReference(receiver, fieldOffset, cg), cg);
      // then find the actual element, note that at this point, the receiver register holds the address to the array
      return generateX86MemoryReference(receiver, index, indexShift, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg);
      }
   else // not array
      {
      intptrj_t fieldOffset = fej9->getInstanceFieldOffsetIncludingHeader(className, "value", fieldSig, owningMethod);
      return generateX86MemoryReference(receiver, fieldOffset, cg);
      }
   }

static bool generateAtomicMemRegOps(TR::Node* node, TR::MethodSymbol* method, TR_X86OpCodes op, bool returnNewValue, bool isArray, bool isConstantParam, int32_t immi, TR::CodeGenerator* cg)
   {
   // FIVE steps to generate atomic code
   // 1. Obtain field address           => fieldRM
   // 2. Obtain value                   => value = evaluate(child) OR 1 OR -1
   // 3. Copy value if needed           => copy = value
   // 4. lock xadd / xchg               => lock add /xchg [fieldRM] value
   // 5. add to return valual if needed => lea value, [value+copy]

   // Step 1
   TR::Register* receiver = cg->evaluate(node->getFirstChild());
   TR::Register* index = NULL;
   if (isArray)
      {
      index = cg->evaluate(node->getChild(1)); // first param is the array index
      }
   TR::MemoryReference* fieldRM = obtainfieldRM(node, method, receiver, index, cg);

   // Step 2
   TR::Register* value = NULL;
   if (isConstantParam) // value to be added not a immidiate
      {
      value = TR::TreeEvaluator::loadConstant(node, immi, TR_RematerializableInt, cg);
      }
   else
      {
      value = intOrLongClobberEvaluate(node->getLastChild(), getNodeIs64Bit(node, cg), cg);
      }

   // Step 3
   TR::Register* tmp = NULL;
   if (returnNewValue && !isConstantParam)
      {
      tmp = cg->allocateRegister();
      generateRegRegInstruction(MOVRegReg(), node, tmp, value, cg);
      }

   // Step 4
   generateMemRegInstruction(op, node, fieldRM, value, cg);

   // Step 5
   if (returnNewValue)
      {
      if (isConstantParam)
         {
         generateRegMemInstruction(LEARegMem(), node, value, generateX86MemoryReference(value, immi, cg), cg);
         }
      else
         {
         generateRegMemInstruction(LEARegMem(), node, value, generateX86MemoryReference(value, tmp, 0, cg), cg);
         cg->stopUsingRegister(tmp);
         }
      }

   // Return result
   node->setRegister(value);

   // Clean up children nodes
   for (uint16_t i = 0; i < node->getNumChildren(); i++)
      {
      cg->decReferenceCount(node->getChild(i));
      }
   return true;
   }
static bool inlineAtomicOps(TR::Node* node, TR::MethodSymbol* method, TR::CodeGenerator* cg)
   {
   if (cg->comp()->getOption(TR_TraceCG))
      {
      traceMsg(cg->comp(),
               "O^O: Inlining recognized atomic primitive 0x%08x: %s.%s%s\n",
               method->getRecognizedMethod(),
               method->getMethod()->classNameChars(),
               method->getMethod()->nameChars(),
               method->getMethod()->signatureChars());
      }

   if(TR::Compiler->om.canGenerateArraylets())
      {
      // arraylet accesses are not done properly in the fast path for Atomic*Array below
      switch (method->getRecognizedMethod())
         {
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndAdd:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndIncrement:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndDecrement:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndSet:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_addAndGet:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_incrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_decrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicLongArray_addAndGet:
         case TR::java_util_concurrent_atomic_AtomicLongArray_decrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicLongArray_getAndAdd:
         case TR::java_util_concurrent_atomic_AtomicLongArray_getAndDecrement:
         case TR::java_util_concurrent_atomic_AtomicLongArray_getAndIncrement:
         case TR::java_util_concurrent_atomic_AtomicLongArray_getAndSet:
         case TR::java_util_concurrent_atomic_AtomicLongArray_incrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicReferenceArray_getAndSet:
            if (cg->comp()->getOption(TR_TraceCG))
              {
              traceMsg(cg->comp(), "Inlining aborted since OM can generate arraylets\n");
              }
            return false;
         default:
            break;
         }
      }

   if (TR::Compiler->target.is32Bit())
      {
      switch (method->getRecognizedMethod())
         {
         case TR::java_util_concurrent_atomic_AtomicLong_addAndGet:
         case TR::java_util_concurrent_atomic_AtomicLong_decrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicLong_getAndAdd:
         case TR::java_util_concurrent_atomic_AtomicLong_getAndDecrement:
         case TR::java_util_concurrent_atomic_AtomicLong_getAndIncrement:
         case TR::java_util_concurrent_atomic_AtomicLong_getAndSet:
         case TR::java_util_concurrent_atomic_AtomicLong_incrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicLongArray_addAndGet:
         case TR::java_util_concurrent_atomic_AtomicLongArray_decrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicLongArray_getAndAdd:
         case TR::java_util_concurrent_atomic_AtomicLongArray_getAndDecrement:
         case TR::java_util_concurrent_atomic_AtomicLongArray_getAndIncrement:
         case TR::java_util_concurrent_atomic_AtomicLongArray_getAndSet:
         case TR::java_util_concurrent_atomic_AtomicLongArray_incrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicReferenceArray_getAndSet:
            if (cg->comp()->getOption(TR_TraceCG))
               {
               traceMsg(cg->comp(), "Inlining aborted since inlining 64-bit primitive on 32-bit platform is not supported\n");
               }
            return false;
         default:
            break;
         }
      }

   switch (method->getRecognizedMethod())
      {
      // no optimization for now since instanceof checks for the updaters make it non trivial to simply reduce to an instruction; maybe a tagged field updater (non null and no type check read)
      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_incrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_incrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicReferenceFieldUpdater_getAndSet:
         if (cg->comp()->getOption(TR_TraceCG))
            {
            traceMsg(cg->comp(), "Inlining aborted since inlining field updater is not supported\n");
            }
         return false;
      default:
         break;
      }

   switch (method->getRecognizedMethod())
      {
      case TR::java_util_concurrent_atomic_AtomicBoolean_getAndSet:
         // Note that boolean is represented by int
         return generateAtomicMemRegOps(node, method, XCHG4MemReg, false, false, false, 0, cg);

      case TR::java_util_concurrent_atomic_AtomicInteger_getAndAdd:
         return generateAtomicMemRegOps(node, method, LXADD4MemReg, false, false, false, 0, cg);

      case TR::java_util_concurrent_atomic_AtomicInteger_getAndIncrement:
         return generateAtomicMemRegOps(node, method, LXADD4MemReg, false, false, true, 1, cg);

      case TR::java_util_concurrent_atomic_AtomicInteger_getAndDecrement:
         return generateAtomicMemRegOps(node, method, LXADD4MemReg, false, false, true, -1, cg);

      case TR::java_util_concurrent_atomic_AtomicInteger_getAndSet:
         return generateAtomicMemRegOps(node, method, XCHG4MemReg, false, false, false, 0, cg);

      case TR::java_util_concurrent_atomic_AtomicInteger_addAndGet:
         return generateAtomicMemRegOps(node, method, LXADD4MemReg, true, false, false, 0, cg);

      case TR::java_util_concurrent_atomic_AtomicInteger_incrementAndGet:
         return generateAtomicMemRegOps(node, method, LXADD4MemReg, true, false, true, 1, cg);

      case TR::java_util_concurrent_atomic_AtomicInteger_decrementAndGet:
         return generateAtomicMemRegOps(node, method, LXADD4MemReg, true, false, true, -1, cg);

      case TR::java_util_concurrent_atomic_AtomicLong_addAndGet:
         return generateAtomicMemRegOps(node, method, LXADD8MemReg, true, false, false, 0, cg);

      case TR::java_util_concurrent_atomic_AtomicLong_decrementAndGet:
         return generateAtomicMemRegOps(node, method, LXADD8MemReg, true, false, true, -1, cg);

      case TR::java_util_concurrent_atomic_AtomicLong_getAndAdd:
         return generateAtomicMemRegOps(node, method, LXADD8MemReg, false, false, false, 0, cg);

      case TR::java_util_concurrent_atomic_AtomicLong_getAndDecrement:
         return generateAtomicMemRegOps(node, method, LXADD8MemReg, false, false, true, -1, cg);

      case TR::java_util_concurrent_atomic_AtomicLong_getAndIncrement:
         return generateAtomicMemRegOps(node, method, LXADD8MemReg, false, false, true, 1, cg);

      case TR::java_util_concurrent_atomic_AtomicLong_getAndSet:
         return generateAtomicMemRegOps(node, method, XCHG8MemReg, false, false, false, 0, cg);

      case TR::java_util_concurrent_atomic_AtomicLong_incrementAndGet:
         return generateAtomicMemRegOps(node, method, LXADD8MemReg, true, false, true, 1, cg);

      case TR::java_util_concurrent_atomic_AtomicReference_getAndSet:
         return generateAtomicMemRegOps(node, method, TR::Compiler->target.is64Bit() ? XCHG8MemReg : XCHG4MemReg, false, false, false, 0, cg);

      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndAdd:
         return generateAtomicMemRegOps(node, method, LXADD4MemReg, false, true, false, 0, cg);

      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndIncrement:
         return generateAtomicMemRegOps(node, method, LXADD4MemReg, false, true, true, 1, cg);

      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndDecrement:
         return generateAtomicMemRegOps(node, method, LXADD4MemReg, false, true, true, -1, cg);

      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndSet:
         return generateAtomicMemRegOps(node, method, XCHG4MemReg, false, true, false, 0, cg);

      case TR::java_util_concurrent_atomic_AtomicIntegerArray_addAndGet:
         return generateAtomicMemRegOps(node, method, LXADD4MemReg, true, true, false, 0, cg);

      case TR::java_util_concurrent_atomic_AtomicIntegerArray_incrementAndGet:
         return generateAtomicMemRegOps(node, method, LXADD4MemReg, true, true, true, 1, cg);

      case TR::java_util_concurrent_atomic_AtomicIntegerArray_decrementAndGet:
         return generateAtomicMemRegOps(node, method, LXADD4MemReg, true, true, true, -1, cg);

      case TR::java_util_concurrent_atomic_AtomicLongArray_addAndGet:
         return generateAtomicMemRegOps(node, method, LXADD8MemReg, true, true, false, 0, cg);

      case TR::java_util_concurrent_atomic_AtomicLongArray_decrementAndGet:
         return generateAtomicMemRegOps(node, method, LXADD8MemReg, true, true, true, -1, cg);

      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndAdd:
         return generateAtomicMemRegOps(node, method, LXADD8MemReg, false, true, false, 0, cg);

      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndDecrement:
         return generateAtomicMemRegOps(node, method, LXADD8MemReg, false, true, true, -1, cg);

      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndIncrement:
         return generateAtomicMemRegOps(node, method, LXADD8MemReg, false, true, true, 1, cg);

      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndSet:
         return generateAtomicMemRegOps(node, method, XCHG8MemReg, false, true, false, 0, cg);

      case TR::java_util_concurrent_atomic_AtomicLongArray_incrementAndGet:
         return generateAtomicMemRegOps(node, method, LXADD8MemReg, true, true, true, 1, cg);

      case TR::java_util_concurrent_atomic_AtomicReferenceArray_getAndSet:
         return generateAtomicMemRegOps(node, method, TR::Compiler->target.is64Bit() ? XCHG8MemReg : XCHG4MemReg, false, true, false, 0, cg);

      default:
         if (cg->comp()->getOption(TR_TraceCG))
            {
            traceMsg(cg->comp(), "WARNING: Trying to inline an unsupported method (0x%08x), abort.\n", method->getRecognizedMethod());
            }
         TR_ASSERT(0, "Trying to inline an unsupported method, abort.");
         return false;
      }
   }


static bool
inlineIsAssignableFrom(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   static char *disable = feGetEnv("TR_disableInlineIsAssignableFrom");
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   if (disable)
      return false;

   // Do not use an inline class check if the 'this' Class object is known to be an
   // abstract class or an interface at compile-time (the check will always fail
   // and go out of line).
   //
   TR::Node *thisClass = node->getFirstChild();
   if (thisClass->getOpCodeValue() == TR::aloadi &&
       thisClass->getFirstChild()->getOpCodeValue() == TR::loadaddr)
      {
      TR::SymbolReference *thisClassSymRef = thisClass->getFirstChild()->getSymbolReference();

      if (thisClassSymRef->isClassInterface(comp) || thisClassSymRef->isClassAbstract(comp))
         {
         return false;
         }
      }

   TR::Register        *returnRegister = NULL;
   TR::SymbolReference *symRef     = node->getSymbolReference();
   TR::MethodSymbol    *callSymbol = symRef->getSymbol()->castToMethodSymbol();

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   startLabel->setStartInternalControlFlow();
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *noCacheLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *outlinedCallLabel = generateLabelSymbol(cg);
   doneLabel->setEndInternalControlFlow();

   TR::Register *thisClassReg = cg->evaluate(node->getFirstChild());
   TR::Register *checkClassReg = cg->evaluate(node->getSecondChild());

   //pattern matching:
   //assume:
   //
   // aloadi  <javaLangClassFromClass>
   //    loadaddr j9class
   //
   // icall  java/lang/Class.isAssignableFrom(Ljava/lang/Class;)Z
   //    ==>aloadi
   //
   int32_t classDepth = -1;
   TR::Node     *javaLangClassFrom = node->getFirstChild();
   if(javaLangClassFrom->getOpCodeValue() == TR::aloadi
         && javaLangClassFrom->getSymbolReference() == comp->getSymRefTab()->findJavaLangClassFromClassSymbolRef()
         && javaLangClassFrom->getFirstChild()->getOpCodeValue() == TR::loadaddr)
   {
      TR::Node   *castClassRef = javaLangClassFrom->getFirstChild();

      TR::SymbolReference *castClassSymRef = castClassRef->getSymbolReference();

      TR::StaticSymbol    *castClassSym = NULL;
      if (castClassSymRef && !castClassSymRef->isUnresolved())
         castClassSym = castClassSymRef ? castClassSymRef->getSymbol()->getStaticSymbol() : NULL;

      TR_OpaqueClassBlock * clazz = NULL;
      if (castClassSym)
         clazz = (TR_OpaqueClassBlock *) castClassSym->getStaticAddress();

      if(clazz)
         classDepth = (int32_t)TR::Compiler->cls.classDepthOf(clazz);
   }

   TR::RegisterDependencyConditions *deps = NULL;
   if(classDepth != -1)
        deps = generateRegisterDependencyConditions((uint8_t) 0, 5, cg);
   else
        deps = generateRegisterDependencyConditions((uint8_t) 0, 3, cg);

   generateLabelInstruction(LABEL, node, startLabel, cg);

   TR::Register *tempReg = cg->allocateRegister();

   deps->addPostCondition(thisClassReg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(checkClassReg, TR::RealRegister::NoReg, cg);
   if (classDepth != -1)
     deps->addPostCondition(tempReg, TR::RealRegister::ByteReg, cg);
   else
      deps->addPostCondition(tempReg, TR::RealRegister::NoReg, cg);

   genNullTest(node, thisClassReg, cg);
   generateLabelInstruction(JE4, node, outlinedCallLabel, cg);
   genNullTest(node, checkClassReg, cg);
   generateLabelInstruction(JE4, node, outlinedCallLabel, cg);

   TR::MemoryReference *checkClassMemRef = generateX86MemoryReference(checkClassReg, fej9->getOffsetOfClassFromJavaLangClassField(), cg);
   generateRegMemInstruction(LRegMem(TR::Compiler->target.is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders()), node, tempReg, checkClassMemRef, cg);

   TR::MemoryReference *cmpEqClassMemRef = generateX86MemoryReference(thisClassReg, fej9->getOffsetOfClassFromJavaLangClassField(), cg);

   generateMemRegInstruction(CMPMemReg(TR::Compiler->target.is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders()), node, cmpEqClassMemRef, tempReg, cg);

   if(classDepth != -1)
   {
      TR::LabelSymbol *fastPathLabel = generateLabelSymbol(cg);
      //end of equality test
      generateLabelInstruction(JE4, node, fastPathLabel, cg);

      TR::Register *thisClassRefReg = cg->allocateRegister();
      deps->addPostCondition(thisClassRefReg, TR::RealRegister::NoReg, cg);
      //pepare the load
      TR::MemoryReference *thisClassMemRef = generateX86MemoryReference(thisClassReg, fej9->getOffsetOfClassFromJavaLangClassField(), cg);
      generateRegMemInstruction(LRegMem(TR::Compiler->target.is64Bit() && !TR::Compiler->om.generateCompressedObjectHeaders()), node, thisClassRefReg, thisClassMemRef, cg);

      TR_X86ScratchRegisterManager *srm = cg->generateScratchRegisterManager();

      TR::Register *scratchReg = cg->allocateRegister();
      deps->addPostCondition(scratchReg, TR::RealRegister::NoReg, cg);
      genSuperClassInstanceOfTest(node, tempReg, scratchReg, thisClassRefReg, classDepth, outlinedCallLabel, NULL, NULL, srm, cg, NULL, NULL,node->getFirstChild(),false, false);

      cg->stopUsingRegister(scratchReg);
      cg->stopUsingRegister(thisClassRefReg);
      srm->stopUsingRegisters();

      generateLabelInstruction(LABEL, node, fastPathLabel, deps, cg);
      generateRegInstruction(SETE1Reg, node, tempReg, cg);
      generateRegRegInstruction(MOVZXReg4Reg1, node, tempReg, tempReg, cg);

   }
   else
   {
       generateRegImmInstruction(MOVRegImm4(), node, tempReg, 1, cg);
       generateLabelInstruction(JNE4, node, outlinedCallLabel, cg);
   }

   TR_OutlinedInstructions *outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::icall, tempReg, outlinedCallLabel, doneLabel, cg);
   cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);
   cg->generateDebugCounter(
      outlinedHelperCall->getFirstInstruction(),
      TR::DebugCounter::debugCounterName(comp, "helperCalls/%s/(%s)/%d/%d", node->getOpCode().getName(), comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
      1, TR::DebugCounter::Cheap);

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());

   node->setRegister(tempReg);

   generateLabelInstruction(LABEL, node, doneLabel, deps, cg);

   return true;
   }


// Generate inline code if possible for a call to an inline method. The call
// may be direct or indirect; if it is indirect a guard will be generated around
// the inline code and a fall-back to the indirect call.
// Returns true if the call was inlined, otherwise a regular call sequence must
// be issued by the caller of this method.
//
bool J9::X86::TreeEvaluator::VMinlineCallEvaluator(
      TR::Node *node,
      bool isIndirect,
      TR::CodeGenerator *cg)
   {
   TR::MethodSymbol *methodSymbol = node->getSymbol()->castToMethodSymbol();
   TR::ResolvedMethodSymbol *resolvedMethodSymbol = node->getSymbol()->getResolvedMethodSymbol();

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

   bool callWasInlined = false;
   TR::Compilation *comp = cg->comp();

   if (methodSymbol)
      {
      switch (methodSymbol->getRecognizedMethod())
         {
         case TR::sun_nio_ch_NativeThread_current:
            // The spec says that on systems that do not require signaling
            // that this method should return -1. I'm not sure what do realtime
            // systems do here
            if (!comp->getOptions()->realTimeGC() && node->getNumChildren()>0)
               {
               TR::Register *nativeThreadReg = cg->allocateRegister();
               TR::Register *nativeThreadRegHigh = NULL;
               TR::Register *vmThreadReg = cg->getVMThreadRegister();
               int32_t numDeps = 2;

               if (TR::Compiler->target.is32Bit())
                  {
                  nativeThreadRegHigh = cg->allocateRegister();
                  numDeps ++;
                  }

               cg->setVMThreadRequired(true);

               TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)numDeps, cg);
               deps->addPostCondition(nativeThreadReg, TR::RealRegister::NoReg, cg);
               if (TR::Compiler->target.is32Bit())
                  {
                  deps->addPostCondition(nativeThreadRegHigh, TR::RealRegister::NoReg, cg);
                  }
               deps->addPostCondition(vmThreadReg, TR::RealRegister::ebp, cg);

               if (TR::Compiler->target.is64Bit())
                  {
                  TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
                  TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
                  startLabel->setStartInternalControlFlow();
                  generateLabelInstruction(LABEL, node, startLabel, cg);

                  generateRegMemInstruction(LRegMem(), node, nativeThreadReg,
                                         generateX86MemoryReference(vmThreadReg, fej9->thisThreadOSThreadOffset(), cg), cg);
                  generateRegMemInstruction(LRegMem(), node, nativeThreadReg,
                                                           generateX86MemoryReference(nativeThreadReg, offsetof(J9Thread, handle), cg), cg);
                  doneLabel->setEndInternalControlFlow();
                  generateLabelInstruction(LABEL, node, doneLabel, deps, cg);
                  }
               else
                  {
                  TR::MemoryReference *lowMR = generateX86MemoryReference(vmThreadReg, fej9->thisThreadOSThreadOffset(), cg);
                  TR::MemoryReference *highMR = generateX86MemoryReference(*lowMR, 4, cg);

                  TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
                  TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
                  startLabel->setStartInternalControlFlow();
                  generateLabelInstruction(LABEL, node, startLabel, cg);

                  generateRegMemInstruction(L4RegMem, node, nativeThreadReg, lowMR, cg);
                  generateRegMemInstruction(L4RegMem, node, nativeThreadRegHigh, highMR, cg);

                  TR::MemoryReference *lowHandleMR = generateX86MemoryReference(nativeThreadReg, offsetof(J9Thread, handle), cg);
                  TR::MemoryReference *highHandleMR = generateX86MemoryReference(*lowMR, 4, cg);

                  generateRegMemInstruction(L4RegMem, node, nativeThreadReg, lowHandleMR, cg);
                  generateRegMemInstruction(L4RegMem, node, nativeThreadRegHigh, highHandleMR, cg);

                  doneLabel->setEndInternalControlFlow();
                  generateLabelInstruction(LABEL, node, doneLabel, deps, cg);
                  }

               cg->setVMThreadRequired(false);

               if (TR::Compiler->target.is32Bit())
                  {
                  TR::RegisterPair *longRegister = cg->allocateRegisterPair(nativeThreadReg, nativeThreadRegHigh);
                  node->setRegister(longRegister);
                  }
               else
                  {
                  node->setRegister(nativeThreadReg);
                  }
               cg->recursivelyDecReferenceCount(node->getFirstChild());
               return true;
               }
            return false; // Call the native version of NativeThread.current()

         case TR::sun_misc_Unsafe_copyMemory:
            {
            if (comp->canTransformUnsafeCopyToArrayCopy() &&
                performTransformation(comp, "O^O Call arraycopy instead of Unsafe.copyMemory: %s\n", cg->getDebug()->getName(node)))
               {
               TR::Node *src = node->getChild(1);
               TR::Node *srcOffset = node->getChild(2);
               TR::Node *dest = node->getChild(3);
               TR::Node *destOffset = node->getChild(4);
               TR::Node *len = node->getChild(5);

               if (TR::Compiler->target.is32Bit())
                  {
                  srcOffset = TR::Node::create(TR::l2i, 1, srcOffset);
                  destOffset = TR::Node::create(TR::l2i, 1, destOffset);
                  len = TR::Node::create(TR::l2i, 1, len);
                  src = TR::Node::create(TR::aiadd, 2, src, srcOffset);
                  dest = TR::Node::create(TR::aiadd, 2, dest, destOffset);
                  }
               else
                  {
                  src = TR::Node::create(TR::aladd, 2, src, srcOffset);
                  dest = TR::Node::create(TR::aladd, 2, dest, destOffset);
                  }

               TR::Node *arraycopyNode = TR::Node::createArraycopy(src, dest, len);
               TR::TreeEvaluator::arraycopyEvaluator(arraycopyNode,cg);

               if (node->getChild(0)->getRegister())
                  cg->decReferenceCount(node->getChild(0));
               else
                  node->getChild(0)->recursivelyDecReferenceCount();

               cg->decReferenceCount(node->getChild(1));
               cg->decReferenceCount(node->getChild(2));
               cg->decReferenceCount(node->getChild(3));
               cg->decReferenceCount(node->getChild(4));
               cg->decReferenceCount(node->getChild(5));

               return true;
               }
               return false; // Perform the original Unsafe.copyMemory call
            }
         case TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z:
            {
            if(node->isSafeForCGToFastPathUnsafeCall())
               return inlineCompareAndSwapNative(node, 4, false, cg);
            }
            break;
         case TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z:
            {
            if(node->isSafeForCGToFastPathUnsafeCall())
               return inlineCompareAndSwapNative(node, 8, false, cg);
            }
            break;
         case TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z:
            {
            if(node->isSafeForCGToFastPathUnsafeCall())
               return inlineCompareAndSwapNative(node, (TR::Compiler->target.is64Bit() && !comp->useCompressedPointers()) ? 8 : 4, true, cg);
            }
            break;
         case TR::java_lang_Integer_rotateLeft:
            {
            TR::TreeEvaluator::integerRolEvaluator(node, cg);
            return true;
            }
         case TR::java_lang_Math_log:
            {
            return inlineMathLog(node, cg);
            }

         case TR::java_lang_Math_sqrt:
         case TR::java_lang_StrictMath_sqrt:
            {
            return inlineMathSQRT(node, cg);
            }

         case TR::java_lang_Math_max_I:
            {
            return inlineSimpleMathFunction(TR_Max_I, node, cg);
            }

         case TR::java_lang_Math_min_I:
            {
            return inlineSimpleMathFunction(TR_Min_I, node, cg);
            }

         case TR::java_lang_Math_max_L:
            {
            return inlineSimpleMathFunction(TR_Max_L, node, cg);
            }

         case TR::java_lang_Math_min_L:
            {
            return inlineSimpleMathFunction(TR_Min_L, node, cg);
            }

         case TR::java_lang_Math_abs_I:
            {
            return inlineSimpleMathFunction(TR_Abs_I, node, cg);
            }

         case TR::java_lang_Math_abs_L:
            {
            return inlineSimpleMathFunction(TR_Abs_L, node, cg);
            }

         case TR::java_lang_Math_abs_F:
            {
            return inlineSimpleMathFunction(TR_Abs_F, node, cg);
            }

         case TR::java_lang_Math_abs_D:
            {
            return inlineSimpleMathFunction(TR_Abs_D, node, cg);
            }
         case TR::java_lang_Long_reverseBytes:
         case TR::java_lang_Integer_reverseBytes:
         case TR::java_lang_Short_reverseBytes:
            {
            if(comp->getOption(TR_EnableJCLInline)
               && performTransformation(comp, "O^O Enable JCL Integer/Long methods inline for: %s\n", cg->getDebug()->getName(node)) )
               return TR::TreeEvaluator::sbyteswapEvaluator(node, cg) != NULL;
            break;
            }

         case TR::java_util_concurrent_atomic_Fences_reachabilityFence:
            {
            cg->decReferenceCount(node->getChild(0));
            break;
            }

         case TR::java_util_concurrent_atomic_Fences_orderAccesses:
            {
            if (cg->getX86ProcessorInfo().supportsMFence())
               {
               TR_X86OpCode fenceOp;
               fenceOp.setOpCodeValue(MFENCE);
               generateInstruction(fenceOp.getOpCodeValue(), node, cg);
               }

            cg->decReferenceCount(node->getChild(0));
            break;
            }

         case TR::java_util_concurrent_atomic_Fences_orderReads:
            {
            if (cg->getX86ProcessorInfo().requiresLFENCE() &&
                cg->getX86ProcessorInfo().supportsLFence())
               {
               TR_X86OpCode fenceOp;
               fenceOp.setOpCodeValue(LFENCE);
               generateInstruction(fenceOp.getOpCodeValue(), node, cg);
               }

            cg->decReferenceCount(node->getChild(0));
            break;
            }

         case TR::java_util_concurrent_atomic_Fences_orderWrites:
            {
            if (cg->getX86ProcessorInfo().supportsSFence())
               {
               TR_X86OpCode fenceOp;
               fenceOp.setOpCodeValue(SFENCE);
               generateInstruction(fenceOp.getOpCodeValue(), node, cg);
               }

            cg->decReferenceCount(node->getChild(0));
            break;
            }

         case TR::java_util_concurrent_atomic_AtomicBoolean_getAndSet:
         case TR::java_util_concurrent_atomic_AtomicInteger_addAndGet:
         case TR::java_util_concurrent_atomic_AtomicInteger_decrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicInteger_getAndAdd:
         case TR::java_util_concurrent_atomic_AtomicInteger_getAndDecrement:
         case TR::java_util_concurrent_atomic_AtomicInteger_getAndIncrement:
         case TR::java_util_concurrent_atomic_AtomicInteger_getAndSet:
         case TR::java_util_concurrent_atomic_AtomicInteger_incrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_addAndGet:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_decrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndAdd:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndDecrement:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndIncrement:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndSet:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_incrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_addAndGet:
         case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_decrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndAdd:
         case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndDecrement:
         case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndIncrement:
         case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_getAndSet:
         case TR::java_util_concurrent_atomic_AtomicIntegerFieldUpdater_incrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicLongArray_addAndGet:
         case TR::java_util_concurrent_atomic_AtomicLongArray_decrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicLongArray_getAndAdd:
         case TR::java_util_concurrent_atomic_AtomicLongArray_getAndDecrement:
         case TR::java_util_concurrent_atomic_AtomicLongArray_getAndIncrement:
         case TR::java_util_concurrent_atomic_AtomicLongArray_getAndSet:
         case TR::java_util_concurrent_atomic_AtomicLongArray_incrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_addAndGet:
         case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_decrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_getAndAdd:
         case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_getAndDecrement:
         case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_getAndIncrement:
         case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_getAndSet:
         case TR::java_util_concurrent_atomic_AtomicLongFieldUpdater_incrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicLong_addAndGet:
         case TR::java_util_concurrent_atomic_AtomicLong_decrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicLong_getAndAdd:
         case TR::java_util_concurrent_atomic_AtomicLong_getAndDecrement:
         case TR::java_util_concurrent_atomic_AtomicLong_getAndIncrement:
         case TR::java_util_concurrent_atomic_AtomicLong_getAndSet:
         case TR::java_util_concurrent_atomic_AtomicLong_incrementAndGet:
            {
            return inlineAtomicOps(node, methodSymbol, cg);
            break;
            }
        case TR::java_lang_Object_clone:
           {
           return (objectCloneEvaluator(node, cg) != NULL);
           break;
           }
        default:
      	  break;
         }
      }

   if (!resolvedMethodSymbol)
      return false;

   if (resolvedMethodSymbol)
      {
      switch (resolvedMethodSymbol->getRecognizedMethod())
         {
         case TR::java_lang_System_currentTimeMillis:
            {
            TR_ASSERT(!isIndirect, "Indirect call to currentTimeMillis");
            callWasInlined = inlineCurrentTimeMillis(node, cg);
            break;
            }
         case TR::java_lang_System_nanoTime:
            {
            TR_ASSERT(!isIndirect, "Indirect call to nanoTime");
            callWasInlined = inlineNanoTime(node, cg);
            break;
            }
         case TR::java_lang_Object_hashCodeImpl:
            {
            if (debug("testObjecthashCode"))
               callWasInlined = inlineObjectHashCode(node, isIndirect);
            break;
            }
         case TR::java_lang_String_hashCodeImplDecompressed:
            {
            if (!comp->getOption(TR_DisableSIMDStringHashCode) && cg->getX86ProcessorInfo().supportsSSE4_1()&& !TR::Compiler->om.canGenerateArraylets())
               callWasInlined = inlineStringHashCode(node, isIndirect, cg);
            break;
            }
         case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordCAS:
            {
            callWasInlined = inlineAtomicMarkableReference_doubleWordCAS(node, cg);
            break;
            }
         case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordSet:
            {
            callWasInlined = inlineAtomicMarkableReference_doubleWordSet(node, cg);
            break;
            }
         case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordCASSupported:
            {
            callWasInlined = inlineAtomicMarkableReference_doubleWordCASSupported(node, cg);
            break;
            }
         case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordSetSupported:
            {
            callWasInlined = inlineAtomicMarkableReference_doubleWordCASSupported(node, cg);
            break;
            }
         case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordCAS:
            {
            callWasInlined = inlineAtomicStampedReference_doubleWordCAS(node, cg);
            break;
            }
         case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordSet:
            {
            callWasInlined = inlineAtomicStampedReference_doubleWordSet(node, cg);
            break;
            }
         case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordCASSupported:
            {
            callWasInlined = inlineAtomicStampedReference_doubleWordCASSupported(node, cg);
            break;
            }
         case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordSetSupported:
            {
            callWasInlined = inlineAtomicStampedReference_doubleWordCASSupported(node, cg);
            break;
            }
         case TR::java_lang_Class_isAssignableFrom:
            {
            callWasInlined = inlineIsAssignableFrom(node, cg);
            break;
            }
         default:
         	break;
         }
      }

   return callWasInlined;
   }


char * const wrtbarOptDetailsFormat = "%35s: %s\n";

static TR::X86WriteBarrierSnippet *generateWriteBarrierSnippet(
   TR::Node             *node,
   TR_WriteBarrierKind  gcMode,
   TR::Register         *owningObjectReg,
   TR::Register         *sourceReg,
   TR::Register         *destAddressReg,
   TR::LabelSymbol      *doneLabel,
   TR::CodeGenerator    *cg)
   {
   TR::Compilation *comp = cg->comp();
   int32_t helperArgCount = 0;  // Number of arguments passed on the runtime helper.
   TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);

   TR::SymbolReference *wrtBarSymRef = NULL;

   if (node->getOpCodeValue() == TR::arraycopy)
      {
      TR_ASSERT(gcMode != TR_WrtbarRealTime, "RealTimeGC currently has no batch write barrier");
      wrtBarSymRef = comp->getSymRefTab()->findOrCreateWriteBarrierBatchStoreSymbolRef();
      helperArgCount = 1;
      }
   else if ((gcMode == TR_WrtbarRealTime)|| (TR::Options::getCmdLineOptions()->realTimeGC()))
      {
      TR_ASSERT(0, "We should not call generateWriteBarrierSnippet for realTime");
      TR_ASSERT(TR::Compiler->target.is32Bit(), "realTime not yet supported on 64b platforms");
      wrtBarSymRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreRealTimeGCSymbolRef();
      helperArgCount = 3;
      }
   else if (gcMode == TR_WrtbarCardMarkAndOldCheck)
      {
      wrtBarSymRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreGenerationalAndConcurrentMarkSymbolRef();
      helperArgCount = 2;
      }
   else if (gcMode == TR_WrtbarAlways)
      {
      wrtBarSymRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef();
      helperArgCount = 2;
      }
   else if (comp->generateArraylets())
      {
      wrtBarSymRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef();
      helperArgCount = 2;
      }
   else
      {
      // Default case is a generational barrier (non-concurrent).
      //
      static char *disable = feGetEnv("TR_disableGenWrtBar");
      wrtBarSymRef = disable ?
         comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef() :
         comp->getSymRefTab()->findOrCreateWriteBarrierStoreGenerationalSymbolRef();
      helperArgCount = 2;
      }

   TR::RegisterDependencyConditions *deps =
      generateRegisterDependencyConditions(0, helperArgCount, cg);

   if (helperArgCount >= 1)
      {
      TR_ASSERT(owningObjectReg, "assertion failure");
      deps->addPostCondition(owningObjectReg, TR::RealRegister::NoReg, cg);
      }

   if (helperArgCount >= 2)
      {
      TR_ASSERT(sourceReg, "assertion failure");
      deps->addPostCondition(sourceReg, TR::RealRegister::NoReg, cg);
      }

   if (helperArgCount >= 3)
      {
      TR_ASSERT(destAddressReg, "assertion failure");
      deps->addPostCondition(destAddressReg, TR::RealRegister::NoReg, cg);
      }

   deps->stopAddingConditions();

   if (debug("traceWrtbar"))
      {
      diagnostic(wrtbarOptDetailsFormat, "Helper",      cg->getDebug()->getName(wrtBarSymRef));
      diagnostic("%35s: %d\n", "Number of helper args", helperArgCount);
      }

   TR::X86WriteBarrierSnippet *snippet =
      TR::generateX86WriteBarrierSnippet(
         cg,
         node,
         doneLabel,
         snippetLabel,
         wrtBarSymRef,
         helperArgCount,
         gcMode,
         deps);

   cg->addSnippet(snippet);

   return snippet;
   }

void J9::X86::TreeEvaluator::generateWrtbarForArrayCopy(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *destOwningObject = node->getChild(1);

   TR_X86ScratchRegisterManager *scratchRegisterManager = cg->generateScratchRegisterManager();

   // srcReg is NULL because there is no one single pointer that's being stored
   // into the destination object.
   //
   TR_ASSERT(!cg->comp()->getOptions()->realTimeGC(), "assertion failure");

   TR::TreeEvaluator::VMwrtbarWithoutStoreEvaluator(
      node,
      destOwningObject,
      NULL,
      NULL,
      scratchRegisterManager,
      cg);
   }

static void reportFlag(bool value, char *name, TR::CodeGenerator *cg)
   {
   if (value)
      traceMsg(cg->comp(), " %s", name);
   }

static int32_t byteOffsetForMask(int32_t mask, TR::CodeGenerator *cg)
   {
   int32_t result;
   for (result = 3; result >= 0; --result)
      {
      int32_t shift = 8*result;
      if ( ((mask>>shift)<<shift) == mask )
         break;
      }

   if (result != -1
      && performTransformation(cg->comp(), "O^O TREE EVALUATION: Use 1-byte TEST with offset %d for mask %08x\n", result, mask))
      return result;

   return -1;
   }


#define REPORT_FLAG(name) reportFlag((name), #name, cg)

void J9::X86::TreeEvaluator::VMwrtbarRealTimeWithoutStoreEvaluator(
   TR::Node                   *node,
   TR::MemoryReference *storeMRForRealTime,          // RTJ only
   TR::Register               *storeAddressRegForRealTime,  // RTJ only
   TR::Node                   *destOwningObject, // only NULL for ME, always evaluated except for AC (evaluated below)
   TR::Node                   *sourceObject,     // NULL for ME and AC(Array Copy?)
   TR::Register               *srcReg,           // should only be provided when sourceObject == NULL  (ME Multimidlet)
   TR_X86ScratchRegisterManager *srm,
   TR::CodeGenerator          *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR_ASSERT(TR::Options::getCmdLineOptions()->realTimeGC(),"Call the non real-time barrier");
   TR_WriteBarrierKind gcMode = comp->getOptions()->getGcMode();

   if (node->getOpCode().isWrtBar() && node->skipWrtBar())
      gcMode = TR_WrtbarNone;
   else if ((node->getOpCodeValue() == TR::ArrayStoreCHK) &&
            node->getFirstChild()->getOpCode().isWrtBar() &&
            node->getFirstChild()->skipWrtBar())
      gcMode = TR_WrtbarNone;

   // PR98283: it is not acceptable to emit a label symbol twice so always generate a new label here
   // we can clean up the API later in a less risky manner
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);

   // srcReg could only be NULL at this point for arraycopy
   if (sourceObject)
      {
      TR_ASSERT(!srcReg, "assertion failure");
      srcReg = sourceObject->getRegister();
      TR_ASSERT(srcReg, "assertion failure");
      }   //

   TR::Node *wrtbarNode;
   switch (node->getOpCodeValue())
      {
      case TR::ArrayStoreCHK:
         wrtbarNode = node->getFirstChild();
         break;
      case TR::arraycopy:
         wrtbarNode = NULL;
         break;
      case TR::wrtbari:
      case TR::wrtbar:
         wrtbarNode = node;
         break;
      default:
         wrtbarNode = NULL;
         break;
      }

   bool doInternalControlFlow;

   if (node->getOpCodeValue() == TR::ArrayStoreCHK)
      {
      // TR::ArrayStoreCHK will create its own internal control flow.
      //
      doInternalControlFlow = false;
      }
   else
      {
      doInternalControlFlow = true;
      }

   if (comp->getOption(TR_TraceCG) /*&& comp->getOption(TR_TraceOptDetails)*/)
      {
      traceMsg(comp, " | Real Time Write barrier info:\n");
      traceMsg(comp, " |   GC mode = %d:%s\n", gcMode, cg->getDebug()->getWriteBarrierKindName(gcMode));
      traceMsg(comp, " |   Node = %s %s  sourceObject = %s\n",
         cg->getDebug()->getName(node->getOpCodeValue()),
         cg->getDebug()->getName(node),
         sourceObject? cg->getDebug()->getName(sourceObject) : "(none)");
      traceMsg(comp, " |   Action flags:");
         REPORT_FLAG(doInternalControlFlow);
      traceMsg(comp, "\n");
      }

   //
   // Phase 2: Generate the appropriate code.
   //
   TR::Register *owningObjectReg;
   TR::Register *tempReg = NULL;

   owningObjectReg = cg->evaluate(destOwningObject);

   if (doInternalControlFlow)
      {
      TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
      startLabel->setStartInternalControlFlow();
      generateLabelInstruction(LABEL, node, startLabel, cg);
      doneLabel->setEndInternalControlFlow();
      }

   if (comp->getOption(TR_BreakOnWriteBarrier))
      {
      generateInstruction(BADIA32Op, node, cg);
      }

   TR::SymbolReference *wrtBarSymRef = NULL;
   if (wrtbarNode && (wrtbarNode->getOpCodeValue()==TR::wrtbar || wrtbarNode->isUnsafeStaticWrtBar()))
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
   TR::Node *callNode = TR::Node::createWithSymRef(TR::call, 3, 3, sourceObject, dummyDestAddressNode, destOwningObject, wrtBarSymRef);

   if (comp->getOption(TR_DisableInlineWriteBarriersRT))
      {
      cg->evaluate(callNode);
      }
   else
      {
      TR_OutlinedInstructions *outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(callNode, TR::call, NULL, snippetLabel, doneLabel, cg);

      // have to disassemble the call node we just created, first have to give it a ref count 1
      callNode->setReferenceCount(1);
      cg->recursivelyDecReferenceCount(callNode);

      cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);
      cg->generateDebugCounter(
            outlinedHelperCall->getFirstInstruction(),
            TR::DebugCounter::debugCounterName(comp, "helperCalls/%s/(%s)/%d/%d", node->getOpCode().getName(), comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
            1, TR::DebugCounter::Cheap);

      if (TR::Options::getCmdLineOptions()->getOption(TR_CountWriteBarriersRT))
         {
         TR::MemoryReference *barrierCountMR = generateX86MemoryReference(cg->getVMThreadRegister(), offsetof(J9VMThread, debugEventData6), cg);
         generateMemInstruction(INCMem(TR::Compiler->target.is64Bit()), node, barrierCountMR, cg);
         }

      tempReg = srm->findOrCreateScratchRegister();

      // if barrier not enabled, nothing to do
      TR::MemoryReference *fragmentParentMR = generateX86MemoryReference(cg->getVMThreadRegister(), fej9->thisThreadRememberedSetFragmentOffset() + fej9->getFragmentParentOffset(), cg);
      generateRegMemInstruction(LRegMem(TR::Compiler->target.is64Bit()), node, tempReg, fragmentParentMR, cg);
      TR::MemoryReference *globalFragmentIDMR = generateX86MemoryReference(tempReg, fej9->getRememberedSetGlobalFragmentOffset(), cg);
      generateMemImmInstruction(CMPMemImms(), node, globalFragmentIDMR, 0, cg);
      generateLabelInstruction(JE4, node, doneLabel, cg);

      // now check if double barrier is enabled and definitely execute the barrier if it is
      // if (vmThread->localFragmentIndex == 0) goto snippetLabel
      TR::MemoryReference *localFragmentIndexMR = generateX86MemoryReference(cg->getVMThreadRegister(), fej9->thisThreadRememberedSetFragmentOffset() + fej9->getLocalFragmentOffset(), cg);
      generateMemImmInstruction(CMPMemImms(), node, localFragmentIndexMR, 0, cg);
      generateLabelInstruction(JE4, node, snippetLabel, cg);

      // null test on the reference we're about to store over: if it is null goto doneLabel
      // if (destObject->field == null) goto doneLabel
      TR::MemoryReference *nullTestMR = generateX86MemoryReference(storeAddressRegForRealTime, 0, cg);
      if (TR::Compiler->target.is64Bit() && comp->useCompressedPointers())
         generateMemImmInstruction(CMP4MemImms, node, nullTestMR, 0, cg);
      else
         generateMemImmInstruction(CMPMemImms(), node, nullTestMR, 0, cg);
      generateLabelInstruction(JNE4, node, snippetLabel, cg);

      // fall-through means write barrier not needed, just do the store
      }

   if (doInternalControlFlow)
      {
      int32_t numPostConditions = 2 + srm->numAvailableRegisters();

      numPostConditions += 4;

      if (srcReg)
         {
         numPostConditions++;
         }

      TR::RegisterDependencyConditions *conditions =
         generateRegisterDependencyConditions((uint8_t) 0, numPostConditions, cg);

      conditions->addPostCondition(owningObjectReg, TR::RealRegister::NoReg, cg);
      if (srcReg)
         {
         conditions->addPostCondition(srcReg, TR::RealRegister::NoReg, cg);
         }

      conditions->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);

      if (!comp->getOption(TR_DisableInlineWriteBarriersRT))
         {
         TR_ASSERT(storeAddressRegForRealTime != NULL, "assertion failure");
         conditions->addPostCondition(storeAddressRegForRealTime, TR::RealRegister::NoReg, cg);

         TR_ASSERT(tempReg != NULL, "assertion failure");
         conditions->addPostCondition(tempReg, TR::RealRegister::NoReg, cg);
         }

      if (destOwningObject->getOpCode().hasSymbolReference()
       && destOwningObject->getSymbol()
       && !destOwningObject->getSymbol()->isLocalObject())
         {
         if (storeMRForRealTime->getBaseRegister())
            {
            conditions->unionPostCondition(storeMRForRealTime->getBaseRegister(), TR::RealRegister::NoReg, cg);
            }
         if (storeMRForRealTime->getIndexRegister())
            {
            conditions->unionPostCondition(storeMRForRealTime->getIndexRegister(), TR::RealRegister::NoReg, cg);
            }
         }

      srm->addScratchRegistersToDependencyList(conditions);
      conditions->stopAddingConditions();

      generateLabelInstruction(LABEL, node, doneLabel, conditions, cg);

      srm->stopUsingRegisters();
      }
   else
      {
      TR_ASSERT(node->getOpCodeValue() == TR::ArrayStoreCHK, "assertion failure");
      generateLabelInstruction(LABEL, node, doneLabel, cg);
      }
  }




void J9::X86::TreeEvaluator::VMwrtbarWithoutStoreEvaluator(
   TR::Node                   *node,
   TR::Node                   *destOwningObject, // only NULL for ME, always evaluated except for AC (evaluated below)
   TR::Node                   *sourceObject,     // NULL for ME and AC(Array Copy?)
   TR::Register               *srcReg,           // should only be provided when sourceObject == NULL  (ME Multimidlet)
   TR_X86ScratchRegisterManager *srm,
   TR::CodeGenerator          *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
   TR_ASSERT(!(TR::Options::getCmdLineOptions()->realTimeGC()),"Call the real-time barrier");
   TR_WriteBarrierKind gcMode = comp->getOptions()->getGcMode();

   if (node->getOpCode().isWrtBar() && node->skipWrtBar())
      gcMode = TR_WrtbarNone;
   else if ((node->getOpCodeValue() == TR::ArrayStoreCHK) &&
            node->getFirstChild()->getOpCode().isWrtBar() &&
            node->getFirstChild()->skipWrtBar())
      gcMode = TR_WrtbarNone;

   // PR98283: it is not acceptable to emit a label symbol twice so always generate a new label here
   // we can clean up the API later in a less risky manner
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);

   TR::LabelSymbol *cardMarkDoneLabel = NULL;

   bool isSourceNonNull;

   // If a source node is provided, derive the source object register from it.
   // The source node must be evaluated before this function is called so it must
   // always be in a register.
   //
   if (sourceObject)
      {
      TR_ASSERT(!srcReg, "assertion failure");
      srcReg = sourceObject->getRegister();
      TR_ASSERT(srcReg, "assertion failure");
      isSourceNonNull = sourceObject->isNonNull();
      }
   else
      {
      isSourceNonNull = false;
      }


   // srcReg could only be NULL at this point for arraycopy

   //
   // Phase 1: Decide what parts of this logic we need to do
   //

   TR::Node *wrtbarNode;
   switch (node->getOpCodeValue())
      {
      case TR::ArrayStoreCHK:
         wrtbarNode = node->getFirstChild();
         break;
      case TR::arraycopy:
         wrtbarNode = NULL;
         break;
      case TR::wrtbari:
      case TR::wrtbar:
         wrtbarNode = node;
         break;
      default:
         wrtbarNode = NULL;
         break;
      }

   bool doInlineCardMarkingWithoutOldSpaceCheck, doIsDestAHeapObjectCheck;

   if (wrtbarNode)
      {
      TR_ASSERT(wrtbarNode->getOpCode().isWrtBar(), "Expected node " POINTER_PRINTF_FORMAT " to be a WrtBar", wrtbarNode);
      // Note: for TR_WrtbarCardMarkAndOldCheck we let the helper do the card mark (ie. we don't inline it)
      doInlineCardMarkingWithoutOldSpaceCheck =
         (gcMode == TR_WrtbarCardMark || gcMode == TR_WrtbarCardMarkIncremental)
         && !wrtbarNode->getSymbol()->isLocalObject()
         && !wrtbarNode->isNonHeapObjectWrtBar();

      doIsDestAHeapObjectCheck = doInlineCardMarkingWithoutOldSpaceCheck && !wrtbarNode->isHeapObjectWrtBar();
      }
   else
      {
      // TR::arraycopy or TR::ArrayStoreCHK
      //
      // Old space checks will be done out-of-line, and if a card mark policy requires an old space check
      // as well then both will be done out-of-line.
      //
      doInlineCardMarkingWithoutOldSpaceCheck = doIsDestAHeapObjectCheck = (gcMode == TR_WrtbarCardMark || gcMode == TR_WrtbarCardMarkIncremental);
      }


// for Tarok  TR_WrtbarCardMarkAndRSM
//
//   doIsDestAHeapObjectCheck = true (if req)   OK
//   doIsDestInOldSpaceCheck = false            OK
//   doInlineCardMarkingWithoutOldSpaceCheck = maybe  OK
//   doCheckConcurrentMarkActive = false        OK
//   dirtyCardTableOutOfLine = false            OK


   bool doIsDestInOldSpaceCheck =
         gcMode == TR_WrtbarOldCheck
      || gcMode == TR_WrtbarCardMarkAndOldCheck
      || gcMode == TR_WrtbarAlways
      ;


   bool unsafeCallBarrier = false;
   if (doIsDestInOldSpaceCheck &&
       (gcMode == TR_WrtbarCardMark
       || gcMode == TR_WrtbarCardMarkAndOldCheck
       || gcMode == TR_WrtbarCardMarkIncremental) &&
       (node->getOpCodeValue()==TR::icall)) {
       TR::MethodSymbol *symbol = node->getSymbol()->castToMethodSymbol();
       if (symbol != NULL && symbol->getRecognizedMethod())
          unsafeCallBarrier = true;
   }

   bool doCheckConcurrentMarkActive =
         (gcMode == TR_WrtbarCardMark
       || gcMode == TR_WrtbarCardMarkAndOldCheck
       || gcMode == TR_WrtbarCardMarkIncremental
       ) && (doInlineCardMarkingWithoutOldSpaceCheck || (doIsDestInOldSpaceCheck && wrtbarNode) || unsafeCallBarrier);

   // Use out-of-line instructions to dirty the card table.
   //
   bool dirtyCardTableOutOfLine = true;

   if (gcMode == TR_WrtbarCardMarkIncremental)
      {
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

   if (node->getOpCodeValue() == TR::ArrayStoreCHK)
      {
      // TR::ArrayStoreCHK will create its own internal control flow.
      //
      doInternalControlFlow = false;
      }
   else
      {
      doInternalControlFlow =
            (doIsDestInOldSpaceCheck
         || doIsDestAHeapObjectCheck
         || doCheckConcurrentMarkActive
         || doSrcIsNullCheck);
      }

   if (comp->getOption(TR_TraceCG) /*&& comp->getOption(TR_TraceOptDetails)*/)
      {
      traceMsg(comp, " | Write barrier info:\n");
      traceMsg(comp, " |   GC mode = %d:%s\n", gcMode, cg->getDebug()->getWriteBarrierKindName(gcMode));
      traceMsg(comp, " |   Node = %s %s  sourceObject = %s\n",
         cg->getDebug()->getName(node->getOpCodeValue()),
         cg->getDebug()->getName(node),
         sourceObject? cg->getDebug()->getName(sourceObject) : "(none)");
      traceMsg(comp, " |   Action flags:");
         REPORT_FLAG(doInternalControlFlow);
         REPORT_FLAG(doCheckConcurrentMarkActive);
         REPORT_FLAG(doInlineCardMarkingWithoutOldSpaceCheck);
         REPORT_FLAG(dirtyCardTableOutOfLine);
         REPORT_FLAG(doIsDestAHeapObjectCheck);
         REPORT_FLAG(doIsDestInOldSpaceCheck);
         REPORT_FLAG(isSourceNonNull);
         REPORT_FLAG(doSrcIsNullCheck);
      traceMsg(comp, "\n");
      }

   //
   // Phase 2: Generate the appropriate code.
   //
   TR::Register *owningObjectReg;
   TR::Register *tempReg = NULL;

   owningObjectReg = cg->evaluate(destOwningObject);

   if (doInternalControlFlow)
      {
      TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
      startLabel->setStartInternalControlFlow();
      generateLabelInstruction(LABEL, node, startLabel, cg);
      doneLabel->setEndInternalControlFlow();
      }

   if (comp->getOption(TR_BreakOnWriteBarrier))
      {
      generateInstruction(BADIA32Op, node, cg);
      }

   TR::MemoryReference *fragmentParentMR = generateX86MemoryReference(cg->getVMThreadRegister(), fej9->thisThreadRememberedSetFragmentOffset() + fej9->getFragmentParentOffset(), cg);
   TR::MemoryReference *localFragmentIndexMR = generateX86MemoryReference(cg->getVMThreadRegister(), fej9->thisThreadRememberedSetFragmentOffset() + fej9->getLocalFragmentOffset(), cg);
   TR_OutlinedInstructions *inlineCardMarkPath = NULL;
   if (doInlineCardMarkingWithoutOldSpaceCheck && doCheckConcurrentMarkActive)
      {
      TR::MemoryReference *vmThreadPrivateFlagsMR = generateX86MemoryReference(cg->getVMThreadRegister(), offsetof(J9VMThread, privateFlags), cg);
      generateMemImmInstruction(TEST4MemImm4, node, vmThreadPrivateFlagsMR, J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE, cg);

      // Branch to outlined instructions to inline card dirtying.
      //
      TR::LabelSymbol *inlineCardMarkLabel = generateLabelSymbol(cg);

      generateLabelInstruction(JNE4, node, inlineCardMarkLabel, cg);

      inlineCardMarkPath = new (cg->trHeapMemory()) TR_OutlinedInstructions(inlineCardMarkLabel, cg);
      cg->getOutlinedInstructionsList().push_front(inlineCardMarkPath);
      inlineCardMarkPath->swapInstructionListsWithCompilation();

      generateLabelInstruction(NULL, LABEL, inlineCardMarkLabel, cg)->setNode(node);
      }

   // Dirty the card table.
   //
   if (doInlineCardMarkingWithoutOldSpaceCheck && (doCheckConcurrentMarkActive || !dirtyCardTableOutOfLine))
      {
      TR::Register *tempReg = srm->findOrCreateScratchRegister();

      generateRegRegInstruction(MOVRegReg(),  node, tempReg, owningObjectReg, cg);

      if (comp->getOptions()->isVariableHeapBaseForBarrierRange0())
         {
         TR::MemoryReference *vhbMR =
         generateX86MemoryReference(cg->getVMThreadRegister(), offsetof(J9VMThread, heapBaseForBarrierRange0), cg);
         generateRegMemInstruction(SUBRegMem(), node, tempReg, vhbMR, cg);
         }
      else
         {
         uintptr_t chb = comp->getOptions()->getHeapBaseForBarrierRange0();

         if (TR::Compiler->target.is64Bit() && (!IS_32BIT_SIGNED(chb) || TR::Compiler->om.nativeAddressesCanChangeSize()))
            {
            TR::Register *chbReg = srm->findOrCreateScratchRegister();
            generateRegImm64Instruction(MOV8RegImm64, node, chbReg, chb, cg, TR_HEAP_BASE_FOR_BARRIER_RANGE);
            generateRegRegInstruction(SUBRegReg(), node, tempReg, chbReg, cg);
            srm->reclaimScratchRegister(chbReg);
            }
         else
            {
               generateRegImmInstruction(SUBRegImm4(), node, tempReg, (int32_t)chb, cg, TR_HEAP_BASE_FOR_BARRIER_RANGE);
            }
         }

      if (doIsDestAHeapObjectCheck)
         {
         cardMarkDoneLabel = doIsDestInOldSpaceCheck ? generateLabelSymbol(cg) : doneLabel;

         if (comp->getOptions()->isVariableHeapSizeForBarrierRange0())
            {
            TR::MemoryReference *vhsMR =
               generateX86MemoryReference(cg->getVMThreadRegister(), offsetof(J9VMThread, heapSizeForBarrierRange0), cg);
            generateRegMemInstruction(CMPRegMem(), node, tempReg, vhsMR, cg);
            }
         else
            {
            uintptr_t chs = comp->getOptions()->getHeapSizeForBarrierRange0();

            if (TR::Compiler->target.is64Bit() && (!IS_32BIT_SIGNED(chs) || TR::Compiler->om.nativeAddressesCanChangeSize()))
               {
               TR::Register *chsReg = srm->findOrCreateScratchRegister();
               generateRegImm64Instruction(MOV8RegImm64, node, chsReg, chs, cg, TR_HEAP_SIZE_FOR_BARRIER_RANGE);
               generateRegRegInstruction(CMPRegReg(), node, tempReg, chsReg, cg);
               srm->reclaimScratchRegister(chsReg);
               }
            else
               {
               generateRegImmInstruction(CMPRegImm4(), node, tempReg, (int32_t)chs, cg, TR_HEAP_SIZE_FOR_BARRIER_RANGE);
               }
            }

         generateLabelInstruction(JAE4, node, cardMarkDoneLabel, cg);
         }

      generateRegImmInstruction(SHRRegImm1(), node, tempReg, comp->getOptions()->getHeapAddressToCardAddressShift(), cg);

      // Mark the card
      //
      const uint8_t dirtyCard = 1;

      TR::MemoryReference *cardTableMR;

      if (comp->getOptions()->isVariableActiveCardTableBase())
         {
         TR::MemoryReference *actbMR =
            generateX86MemoryReference(cg->getVMThreadRegister(), offsetof(J9VMThread, activeCardTableBase), cg);
         generateRegMemInstruction(ADDRegMem(), node, tempReg, actbMR, cg);
         cardTableMR = generateX86MemoryReference(tempReg, 0, cg);
         }
      else
         {
         uintptr_t actb = comp->getOptions()->getActiveCardTableBase();

         if (TR::Compiler->target.is64Bit() && (!IS_32BIT_SIGNED(actb) || TR::Compiler->om.nativeAddressesCanChangeSize()))
            {
            TR::Register *tempReg3 = srm->findOrCreateScratchRegister();
               generateRegImm64Instruction(MOV8RegImm64, node, tempReg3, actb, cg, TR_ACTIVE_CARD_TABLE_BASE);
            cardTableMR = generateX86MemoryReference(tempReg3, tempReg, 0, cg);
            srm->reclaimScratchRegister(tempReg3);
            }
         else
            {
            cardTableMR = generateX86MemoryReference(NULL, tempReg, 0, (int32_t)actb, cg);
            cardTableMR->setReloKind(TR_ACTIVE_CARD_TABLE_BASE);
            }
         }

      generateMemImmInstruction(S1MemImm1, node, cardTableMR, dirtyCard, cg);

      srm->reclaimScratchRegister(tempReg);

      if (dirtyCardTableOutOfLine)
         {
         generateLabelInstruction(JMP4, node, doneLabel, cg);
         inlineCardMarkPath->swapInstructionListsWithCompilation();
         }
      }

   if (doIsDestAHeapObjectCheck && doIsDestInOldSpaceCheck)
      {
      generateLabelInstruction(LABEL, node, cardMarkDoneLabel, cg);
      }

   if (doSrcIsNullCheck)
      {
      generateRegRegInstruction(TESTRegReg(), node, srcReg, srcReg, cg);
      generateLabelInstruction(JE4, node, doneLabel, cg);
      }

   if (doIsDestInOldSpaceCheck)
      {
      TR_X86OpCodes branchOp;
      TR_WriteBarrierKind gcModeForSnippet = gcMode;

      bool branchToSnippetIfOld = false;
      bool branchToSnippetIfOldAndNotRemembered = false;
      TR::LabelSymbol *labelAfterBranchToSnippet = NULL;

      if (gcMode == TR_WrtbarAlways)
         {
         // Always call the write barrier helper.
         //
         // TODO: this should be an inline call.
         //
         branchOp = JMP4;
         }
      else if (doCheckConcurrentMarkActive)
         {
         //TR_ASSERT(wrtbarNode, "Must not be an arraycopy");

         // If the concurrent mark thread IS active then call the gencon write barrier in the helper
         // to perform card marking and any necessary remembered set updates.
         //
         // This is expected to be true for only a very small percentage of the time and hence
         // handling it out of line is justified.
         //
         if (!comp->getOption(TR_DisableWriteBarriersRangeCheck)
             && (node->getOpCodeValue() == TR::wrtbari)
             && doInternalControlFlow)
            {
            uintptrj_t nurseryBase, nurseryTop;

            fej9->getNurserySpaceBounds(&nurseryBase, &nurseryTop);

            // sometimes the tenured space can grow, so we can't deal with the case
            // where the tenured area is above the nursery, unless in the future we
            // can tell the possible tenured space max boundary.
            //
            if ((nurseryBase != 0) && (nurseryTop != 0))
               {
               TR_X86OpCodes nurseryCompareOp = JAE4;
               TR_X86OpCodes stackCompareOp = JB4;
               uintptrj_t nurseryCompareValue = nurseryBase;

#if defined(J9VM_GC_MODRON_CONCURRENT_MARK)
               uintptrj_t stackCompareValue = comp->getOptions()->getHeapBase();
#else
               uintptrj_t stackCompareValue = 0;
#endif
               //printf("Heap base %p, Nursery base %p\n", stackCompareValue, nurseryCompareValue);

               bool canDoImmCompare = (TR::Compiler->target.is64Bit()) ? IS_32BIT_SIGNED(nurseryCompareValue) : true;
               if (nurseryCompareValue && canDoImmCompare)
                  {
                  generateRegImmInstruction(CMPRegImm4(), node, owningObjectReg, (uint32_t)nurseryCompareValue, cg);
                  generateLabelInstruction(nurseryCompareOp, node, doneLabel, cg);
                  }

               if (stackCompareValue != 0 && wrtbarNode->isLikelyStackWrtBar())
                  {
                  canDoImmCompare = (TR::Compiler->target.is64Bit()) ? IS_32BIT_SIGNED(stackCompareValue) : true;
                  if (stackCompareValue && canDoImmCompare)
                     {
                     //printf("Likely stack wrtbar in method %s, tenureTop %p\n", comp->signature(), tenureBase);
                     generateRegImmInstruction(CMPRegImm4(), node, owningObjectReg, (uint32_t)stackCompareValue, cg);
                     generateLabelInstruction(stackCompareOp, node, doneLabel, cg);
                     }
                  }
               }
            }

         // See if we can do a TEST1MemImm1
         //
         int32_t byteOffset = byteOffsetForMask(J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE, cg);
         if (byteOffset != -1)
            {
            TR::MemoryReference *vmThreadPrivateFlagsMR = generateX86MemoryReference(cg->getVMThreadRegister(), byteOffset + offsetof(J9VMThread, privateFlags), cg);
            generateMemImmInstruction(TEST1MemImm1, node, vmThreadPrivateFlagsMR, J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE >> (8*byteOffset), cg);
            }
         else
            {
            TR::MemoryReference *vmThreadPrivateFlagsMR = generateX86MemoryReference(cg->getVMThreadRegister(), offsetof(J9VMThread, privateFlags), cg);
            generateMemImmInstruction(TEST4MemImm4, node, vmThreadPrivateFlagsMR, J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE, cg);
            }

         TR::X86WriteBarrierSnippet *snippet =
            generateWriteBarrierSnippet(node, TR_WrtbarCardMarkAndOldCheck, owningObjectReg, srcReg, NULL, doneLabel, cg);
         generateLabelInstruction(JNE4, node, snippet->getSnippetLabel(), snippet->getDependencies(), cg);

         // If the destination object is old and not remembered then process the remembered
         // set update out-of-line with the generational helper.
         //
         branchToSnippetIfOldAndNotRemembered = true;
         gcModeForSnippet = TR_WrtbarOldCheck;
         }
      else if (gcMode == TR_WrtbarOldCheck)
         {
         // For pure generational barriers if the object is old and remembered then the helper
         // can be skipped.
         //
         branchToSnippetIfOldAndNotRemembered = true;
         }
      else
         {
         branchToSnippetIfOld = true;
         }

      if (branchToSnippetIfOldAndNotRemembered)
         {
         bool is64Bit = TR::Compiler->target.is64Bit(); // On compressed refs, owningObjectReg is already uncompressed, and the vmthread fields are 64 bits
         labelAfterBranchToSnippet = generateLabelSymbol(cg);

         if (!comp->getOptions()->isVariableHeapBaseForBarrierRange0())
            {
            TR::Register *tempReg = srm->findOrCreateScratchRegister();
            generateRegRegInstruction(MOVRegReg(),  node, tempReg, owningObjectReg, cg);
            uintptr_t chb = comp->getOptions()->getHeapBaseForBarrierRange0();
            // asum chb is align to 2
            uintptr_t halfchb = chb >> 1;

            if (TR::Compiler->target.is64Bit() && !TR::Compiler->om.nativeAddressesCanChangeSize() && !IS_32BIT_SIGNED(chb) && IS_32BIT_SIGNED(halfchb))
               {
               generateRegImmInstruction(SUBRegImm4(), node, tempReg, (int32_t)halfchb, cg, TR_HEAP_BASE_FOR_BARRIER_RANGE);
               generateRegImmInstruction(SUBRegImm4(), node, tempReg, (int32_t)halfchb, cg, TR_HEAP_BASE_FOR_BARRIER_RANGE);
               }
            else if (TR::Compiler->target.is64Bit() && (!IS_32BIT_SIGNED(chb) || TR::Compiler->om.nativeAddressesCanChangeSize()))
               {
               TR::Register *chbReg = srm->findOrCreateScratchRegister();
               generateRegImm64Instruction(MOV8RegImm64, node, chbReg, chb, cg, TR_HEAP_BASE_FOR_BARRIER_RANGE);
               generateRegRegInstruction(SUBRegReg(), node, tempReg, chbReg, cg);
               srm->reclaimScratchRegister(chbReg);
               }
            else
               {
               generateRegImmInstruction(SUBRegImm4(), node, tempReg, (int32_t)chb, cg, TR_HEAP_BASE_FOR_BARRIER_RANGE);
               }
            TR::MemoryReference *vhsMR1 =
                  generateX86MemoryReference(cg->getVMThreadRegister(), offsetof(J9VMThread, heapSizeForBarrierRange0), cg);
            generateRegMemInstruction(CMPRegMem(), node, tempReg, vhsMR1, cg);
            generateLabelInstruction(JAE1, node, labelAfterBranchToSnippet, cg);
            }
         else
            {
            // Since srcReg is NULL, only check whether the owning object is in tenure
            generateMemRegInstruction(CMPMemReg(is64Bit), node,
                  generateX86MemoryReference(cg->getVMThreadRegister(),
                        offsetof(J9VMThread, lowTenureAddress), cg),
                  owningObjectReg, cg);
            generateLabelInstruction(JA1, node, labelAfterBranchToSnippet,
                  cg);
            generateMemRegInstruction(CMPMemReg(is64Bit), node,
                  generateX86MemoryReference(cg->getVMThreadRegister(),
                        offsetof(J9VMThread, highTenureAddress), cg),
                  owningObjectReg, cg);
            generateLabelInstruction(JBE1, node, labelAfterBranchToSnippet,
                  cg);
            }

         // Now performing check for remembered
         int32_t byteOffset = byteOffsetForMask(J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST, cg);
         if (byteOffset != -1)
            {
            TR::MemoryReference *MR = generateX86MemoryReference(owningObjectReg, byteOffset + TR::Compiler->om.offsetOfHeaderFlags(), cg);
            generateMemImmInstruction(TEST1MemImm1, node, MR, J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST >> (8*byteOffset), cg);
            }
         else
            {
            TR::MemoryReference *MR = generateX86MemoryReference(owningObjectReg, TR::Compiler->om.offsetOfHeaderFlags(), cg);
            generateMemImmInstruction(TEST4MemImm4, node, MR, J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST, cg);
            }
         branchOp=JE4;
         }

      if (branchToSnippetIfOld)
         {
         bool is64Bit = TR::Compiler->target.is64Bit(); // On compressed refs, owningObjectReg is already uncompressed, and the vmthread fields are 64 bits
         labelAfterBranchToSnippet = generateLabelSymbol(cg);

         if (!comp->getOptions()->isVariableHeapBaseForBarrierRange0())
            {
            TR::Register *tempReg = srm->findOrCreateScratchRegister();
            generateRegRegInstruction(MOVRegReg(),  node, tempReg, owningObjectReg, cg);
            uintptr_t chb = comp->getOptions()->getHeapBaseForBarrierRange0();
            // asum chb is align to 2
            uintptr_t halfchb = chb >> 1;

            if (TR::Compiler->target.is64Bit() && !TR::Compiler->om.nativeAddressesCanChangeSize() && !IS_32BIT_SIGNED(chb) && IS_32BIT_SIGNED(halfchb))
               {
               generateRegImmInstruction(SUBRegImm4(), node, tempReg, (int32_t)halfchb, cg, TR_HEAP_BASE_FOR_BARRIER_RANGE);
               generateRegImmInstruction(SUBRegImm4(), node, tempReg, (int32_t)halfchb, cg, TR_HEAP_BASE_FOR_BARRIER_RANGE);
               }
            else if (TR::Compiler->target.is64Bit() && (!IS_32BIT_SIGNED(chb) || TR::Compiler->om.nativeAddressesCanChangeSize()))
               {
               TR::Register *chbReg = srm->findOrCreateScratchRegister();
               generateRegImm64Instruction(MOV8RegImm64, node, chbReg, chb, cg, TR_HEAP_BASE_FOR_BARRIER_RANGE);
               generateRegRegInstruction(SUBRegReg(), node, tempReg, chbReg, cg);
               srm->reclaimScratchRegister(chbReg);
               }
            else
               {
               generateRegImmInstruction(SUBRegImm4(), node, tempReg, (int32_t)chb, cg, TR_HEAP_BASE_FOR_BARRIER_RANGE);
               }
            TR::MemoryReference *vhsMR1 =
               generateX86MemoryReference(cg->getVMThreadRegister(), offsetof(J9VMThread, heapSizeForBarrierRange0), cg);
            generateRegMemInstruction(CMPRegMem(), node, tempReg, vhsMR1, cg);
            branchOp = JB4;
            }
         else
            {
            // Since srcReg is NULL, only check whether the owning object is in tenure
            generateMemRegInstruction(CMPMemReg(is64Bit), node,
                  generateX86MemoryReference(cg->getVMThreadRegister(),
                        offsetof(J9VMThread, lowTenureAddress), cg),
                  owningObjectReg, cg);
            generateLabelInstruction(JA1, node, labelAfterBranchToSnippet,
                  cg);
            generateMemRegInstruction(CMPMemReg(is64Bit), node,
                  generateX86MemoryReference(cg->getVMThreadRegister(),
                        offsetof(J9VMThread, highTenureAddress), cg),
                  owningObjectReg, cg);
            branchOp = JA4;
            }
         }

      TR::X86WriteBarrierSnippet *snippet = generateWriteBarrierSnippet(node, gcModeForSnippet, owningObjectReg, srcReg, NULL, doneLabel, cg);
      generateLabelInstruction(branchOp, node, snippet->getSnippetLabel(), snippet->getDependencies(), cg);
      if (labelAfterBranchToSnippet)
         generateLabelInstruction(LABEL, node, labelAfterBranchToSnippet, cg);
      }

   int32_t numPostConditions = 2 + srm->numAvailableRegisters();

   if (srcReg)
      {
      numPostConditions++;
      }

   TR::RegisterDependencyConditions *conditions =
      generateRegisterDependencyConditions((uint8_t) 0, numPostConditions, cg);

   conditions->addPostCondition(owningObjectReg, TR::RealRegister::NoReg, cg);
   if (srcReg)
      {
      conditions->addPostCondition(srcReg, TR::RealRegister::NoReg, cg);
      }

   conditions->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);

   srm->addScratchRegistersToDependencyList(conditions);
   conditions->stopAddingConditions();

   generateLabelInstruction(LABEL, node, doneLabel, conditions, cg);

   srm->stopUsingRegisters();
  }


static TR::Instruction *
doReferenceStore(
   TR::Node               *node,
   TR::MemoryReference *storeMR,
   TR::Register           *sourceReg,
   bool                   usingCompressedPointers,
   TR::CodeGenerator      *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_X86OpCodes storeOp = usingCompressedPointers ? S4MemReg : SMemReg();
   TR::Instruction *instr = generateMemRegInstruction(storeOp, node, storeMR, sourceReg, cg);

   // for real-time GC, the data reference has already been resolved into an earlier LEA instruction so this padding isn't needed
   //   even if the node symbol is marked as unresolved (the store instruction above is storing through a register
   //   that contains the resolved address)
   if (!comp->getOptions()->realTimeGC() && node->getSymbolReference()->isUnresolved())
      {
      TR::TreeEvaluator::padUnresolvedDataReferences(node, *node->getSymbolReference(), cg);
      }

   return instr;
   }


void J9::X86::TreeEvaluator::VMwrtbarWithStoreEvaluator(
   TR::Node                   *node,
   TR::MemoryReference *storeMR,
   TR_X86ScratchRegisterManager *scratchRegisterManager,
   TR::Node                   *destOwningObject,
   TR::Node                   *sourceObject,
   bool                       isImplicitExceptionPoint,
   TR::CodeGenerator          *cg,
   bool                       nullAdjusted)
   {
   TR_ASSERT(storeMR, "assertion failure");

   TR::Compilation *comp = cg->comp();

   TR::Register *owningObjectRegister = cg->evaluate(destOwningObject);
   TR::Register *sourceRegister = cg->evaluate(sourceObject);

   TR_WriteBarrierKind gcMode = comp->getOptions()->getGcMode();
   bool isRealTimeGC = (TR::Options::getCmdLineOptions()->realTimeGC())? true:false;

   bool usingCompressedPointers = false;
   bool usingLowMemHeap = false;
   bool useShiftedOffsets = (TR::Compiler->om.compressedReferenceShiftOffset() != 0);
   TR::Node *translatedStore = NULL;

   // NOTE:
   //
   // If you change this code you also need to change writeBarrierEvaluator() in TreeEvaluator.cpp
   //
   if (comp->useCompressedPointers() &&
       ((node->getOpCode().isCheck() && node->getFirstChild()->getOpCode().isIndirect() &&
        (node->getFirstChild()->getSecondChild()->getDataType() != TR::Address)) ||
        (node->getOpCode().isIndirect() && (node->getSecondChild()->getDataType() != TR::Address))))
      {
      if (node->getOpCode().isCheck())
         translatedStore = node->getFirstChild();
      else
         translatedStore = node;

      // pattern match the sequence
      //     iwrtbar f     iwrtbar f         <- node
      //       aload O       aload O
      //     value           l2i
      //                       lshr
      //                         lsub        <- translatedNode
      //                           a2l
      //                             value   <- sourceObject
      //                           lconst HB
      //                         iconst shftKonst
      //
      // -or- if the field is known to be null
      // iwrtbar f
      //    aload O
      //    l2i
      //      a2l
      //        value  <- sourceObject
      //
      TR::Node *translatedNode = translatedStore->getSecondChild();
      if (translatedNode->getOpCode().isConversion())
         translatedNode = translatedNode->getFirstChild();
      if (translatedNode->getOpCode().isRightShift()) // optional
         translatedNode = translatedNode->getFirstChild();

      if ((TR::Compiler->vm.heapBaseAddress() == 0) ||
            translatedStore->getSecondChild()->isNull())
         usingLowMemHeap = true;

      if (translatedNode->getOpCode().isSub() || usingLowMemHeap)
         usingCompressedPointers = true;
      }

   TR::Register *translatedSourceReg = sourceRegister;
   if (usingCompressedPointers && (!usingLowMemHeap || useShiftedOffsets))
      {
      // handle stores of null values here

      if (nullAdjusted)
         translatedSourceReg = translatedStore->getSecondChild()->getRegister();
      else
         {
         translatedSourceReg = cg->evaluate(translatedStore->getSecondChild());
         if (!usingLowMemHeap)
            {
            generateRegRegInstruction(TESTRegReg(), translatedStore, sourceRegister, sourceRegister, cg);
            generateRegRegInstruction(CMOVERegReg(), translatedStore, translatedSourceReg, sourceRegister, cg);
            }
         }
      }

   TR::Instruction *storeInstr = NULL;
   TR::Register *storeAddressRegForRealTime = NULL;

   if (isRealTimeGC)
      {
      // Realtime GC evalutes storeMR into a register here and then uses it to do the store after the write barrier

      // If reference is unresolved, need to resolve it right here before the barrier starts
      // Otherwise, we could get stopped during the resolution and that could invalidate any tests we would have performend
      //   beforehand
      // For simplicity, just evaluate the store address into storeAddressRegForRealTime right now
      storeAddressRegForRealTime = scratchRegisterManager->findOrCreateScratchRegister();
      generateRegMemInstruction(LEARegMem(), node, storeAddressRegForRealTime, storeMR, cg);
      if (node->getSymbolReference()->isUnresolved())
         {
         TR::TreeEvaluator::padUnresolvedDataReferences(node, *node->getSymbolReference(), cg);

         // storeMR was created against a (i)wrtbar node which is a store.  The unresolved data snippet that
         //  was created set the checkVolatility bit based on that node being a store.  Since the resolution
         //  is now going to occur on a LEA instruction, which does not require any memory fence and hence
         //  no volatility check, we need to clear that "store" ness of the unresolved data snippet
         TR::UnresolvedDataSnippet *snippet = storeMR->getUnresolvedDataSnippet();
         if (snippet)
            snippet->resetUnresolvedStore();
         }
      }
   else
      {
      // Non-realtime does the store first, then the write barrier.
      //
      storeInstr = doReferenceStore(node, storeMR, translatedSourceReg, usingCompressedPointers, cg);
      }


   cg->setVMThreadRequired(true);

   if (comp->getOptions()->alwaysCallWriteBarrier() && !isRealTimeGC)
      {
      TR::RegisterDependencyConditions *deps = NULL;
      TR::LabelSymbol *doneWrtBarLabel = generateLabelSymbol(cg);

      if (TR::Compiler->target.is64Bit())
         {
         // TODO: do this inline, or via a shared snippet
         //
         TR::X86WriteBarrierSnippet *snippet =
            generateWriteBarrierSnippet(node, gcMode, owningObjectRegister, sourceRegister, NULL, doneWrtBarLabel, cg);
         generateLabelInstruction(JMP4, node, snippet->getSnippetLabel(), cg);
         deps = snippet->getDependencies();
         }
      else
         {
         if (sourceObject->isNonNull() == false)
            {
            TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
            startLabel->setStartInternalControlFlow();
            doneWrtBarLabel->setEndInternalControlFlow();

            generateLabelInstruction(LABEL, node, startLabel, cg);
            generateRegRegInstruction(TESTRegReg(), node, sourceRegister, sourceRegister, cg);
            generateLabelInstruction(JE4, node, doneWrtBarLabel, cg);

            deps = generateRegisterDependencyConditions(0, 3, cg);
            deps->addPostCondition(sourceRegister, TR::RealRegister::NoReg, cg);
            deps->addPostCondition(owningObjectRegister, TR::RealRegister::NoReg, cg);
            deps->addPostCondition(cg->getVMThreadRegister(), TR::RealRegister::ebp, cg);
            deps->stopAddingConditions();
            }

         generateRegInstruction(PUSHReg, node, sourceRegister, cg);

         TR::SymbolReference *wrtBarSymRef;

         wrtBarSymRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef();

         generateRegInstruction(PUSHReg, node, owningObjectRegister, cg);

         TR::X86ImmSymInstruction *instr =
            generateImmSymInstruction(CALLImm4, node, (int32_t)(uintptr_t)wrtBarSymRef->getMethodAddress(), wrtBarSymRef, cg);

         instr->setAdjustsFramePointerBy(-8);
         }

      if (deps != NULL)
         generateLabelInstruction(LABEL, node, doneWrtBarLabel, deps, cg);
      else
         generateLabelInstruction(LABEL, node, doneWrtBarLabel, true, cg);
      }
   else
      {
      if (isRealTimeGC)
         {
         TR::TreeEvaluator::VMwrtbarRealTimeWithoutStoreEvaluator(
                  node,
                  storeMR,
                  storeAddressRegForRealTime,
                  destOwningObject,
                  sourceObject,
                  NULL,
                  scratchRegisterManager,
                  cg);
         }
      else
         {
         TR::TreeEvaluator::VMwrtbarWithoutStoreEvaluator(
            node,
            destOwningObject,
            sourceObject,
            NULL,
            scratchRegisterManager,
            cg);
         }
      }

   cg->setVMThreadRequired(false);

   // Realtime GCs must do the write barrier first and then the store.
   //
   if (isRealTimeGC)
      {
      TR_ASSERT(storeAddressRegForRealTime, "assertion failure");
      TR::MemoryReference *myStoreMR = generateX86MemoryReference(storeAddressRegForRealTime, 0, cg);
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
   uintptrj_t mask = TR::Compiler->om.maskOfObjectVftField();
   bool is64Bit = TR::Compiler->target.is64Bit(); // even with compressed object headers, a 64-bit mask operation is safe, though it may waste 1 byte because of the rex prefix
   if (~mask == 0)
      {
      // no mask instruction required
      }
   else if (~mask <= 127)
      {
      generateRegImmInstruction(ANDRegImms(is64Bit), node, reg, TR::Compiler->om.maskOfObjectVftField(), cg);
      }
   else
      {
      generateRegImmInstruction(ANDRegImm4(is64Bit), node, reg, TR::Compiler->om.maskOfObjectVftField(), cg);
      }
   }


void
VMgenerateCatchBlockBBStartPrologue(
      TR::Node *node,
      TR::Instruction *fenceInstruction,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

   if (comp->getOption(TR_FullSpeedDebug))
      {
      fenceInstruction->setNeedsGCMap(); // a catch entry is a gc point in FSD mode
      }

   if (comp->getJittedMethodSymbol()->usesSinglePrecisionMode() &&
       cg->enableSinglePrecisionMethods())
      {
      cg->setLastCatchAppendInstruction(fenceInstruction);
      }

   TR::Block *block = node->getBlock();
   if (fej9->shouldPerformEDO(block, comp))
      {
      TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *restartLabel = generateLabelSymbol(cg);

      cg->setVMThreadRequired(true);
      generateMemInstruction(DEC4Mem, node, generateX86MemoryReference((intptrj_t)comp->getRecompilationInfo()->getCounterAddress(), cg), cg);
      generateLabelInstruction(JE4, node, snippetLabel, cg);
      generateLabelInstruction(LABEL, node, restartLabel, cg);
      cg->setVMThreadRequired(false);
      cg->addSnippet(new (cg->trHeapMemory()) TR::X86ForceRecompilationSnippet(cg, node, restartLabel, snippetLabel));
      }

   }


TR::Register *
J9::X86::TreeEvaluator::tstartEvaluator(TR::Node *node, TR::CodeGenerator *cg)
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
   TR::Node *persistentFailureNode = node->getFirstChild();
   TR::Node *transientFailureNode  = node->getSecondChild();
   TR::Node *fallThroughNode = node->getThirdChild();
   TR::Node *objNode = node->getChild(3);
   TR::Node *GRANode = NULL;

   TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   startLabel->setStartInternalControlFlow();
   TR::LabelSymbol *endLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   endLabel->setEndInternalControlFlow();

   TR::LabelSymbol *gotoTransientFailure = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *gotoPersistentFailure = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *gotoFallThrough = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *transientFailureLabel  = transientFailureNode->getBranchDestination()->getNode()->getLabel();
   TR::LabelSymbol *persistentFailureLabel = persistentFailureNode->getBranchDestination()->getNode()->getLabel();
   TR::LabelSymbol *fallBackPathLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *fallThroughLabel = fallThroughNode->getBranchDestination()->getNode()->getLabel();

   TR::Register *objReg = cg->evaluate(objNode);
   TR::Register *accReg = cg->allocateRegister();
   TR::Register *monReg = cg->allocateRegister();
   TR::RegisterDependencyConditions *fallBackConditions = generateRegisterDependencyConditions((uint8_t)0, 2, cg);
   TR::RegisterDependencyConditions *endLabelConditions;
   TR::RegisterDependencyConditions *fallThroughConditions = NULL;
   TR::RegisterDependencyConditions *persistentConditions = NULL;
   TR::RegisterDependencyConditions *transientConditions = NULL;

   if (fallThroughNode->getNumChildren() != 0)
      {
      GRANode = fallThroughNode->getFirstChild();
      cg->evaluate(GRANode);
      List<TR::Register> popRegisters(cg->trMemory());
      fallThroughConditions = generateRegisterDependencyConditions(GRANode, cg, 0, &popRegisters);
      cg->decReferenceCount(GRANode);
      }

   if (persistentFailureNode->getNumChildren() != 0)
      {
      GRANode = persistentFailureNode->getFirstChild();
      cg->evaluate(GRANode);
      List<TR::Register> popRegisters(cg->trMemory());
      persistentConditions = generateRegisterDependencyConditions(GRANode, cg, 0, &popRegisters);
      cg->decReferenceCount(GRANode);
      }

   if (transientFailureNode->getNumChildren() != 0)
      {
      GRANode = transientFailureNode->getFirstChild();
      cg->evaluate(GRANode);
      List<TR::Register> popRegisters(cg->trMemory());
      transientConditions = generateRegisterDependencyConditions(GRANode, cg, 0, &popRegisters);
      cg->decReferenceCount(GRANode);
      }

   //startLabel
   //add place holder register so that eax would not contain any useful value before xbegin
   TR::Register *dummyReg = cg->allocateRegister();
   dummyReg->setPlaceholderReg();
   TR::RegisterDependencyConditions *startLabelConditions = generateRegisterDependencyConditions((uint8_t)0, 1, cg);
   startLabelConditions->addPostCondition(dummyReg, TR::RealRegister::eax, cg);
   startLabelConditions->stopAddingConditions();
   cg->stopUsingRegister(dummyReg);
   generateLabelInstruction(LABEL, node, startLabel, startLabelConditions, cg);

   //xbegin fall_back_path
   generateLongLabelInstruction(XBEGIN4, node, fallBackPathLabel, cg);
   //mov monReg, obj+offset
   int32_t lwOffset = cg->fej9()->getByteOffsetToLockword((TR_OpaqueClassBlock *) cg->getMonClass(node));
   TR::MemoryReference *objLockRef = generateX86MemoryReference(objReg, lwOffset, cg);
   if (TR::Compiler->target.is64Bit() && cg->fej9()->generateCompressedLockWord())
      {
      generateRegMemInstruction(L4RegMem, node, monReg, objLockRef, cg);
      }
   else
      {
      generateRegMemInstruction(LRegMem(), node, monReg, objLockRef, cg);
      }

   if (TR::Compiler->target.is64Bit() && cg->fej9()->generateCompressedLockWord())
      {
      generateRegImmInstruction(CMP4RegImm4, node, monReg, 0, cg);
      }
   else
      {
      generateRegImmInstruction(CMPRegImm4(), node, monReg, 0, cg);
      }

   if (fallThroughConditions)
      generateLabelInstruction(JE4, node, fallThroughLabel, fallThroughConditions, cg);
   else
      generateLabelInstruction(JE4, node, fallThroughLabel, cg);

   TR::Register *vmThreadReg = cg->getVMThreadRegister();
   if (TR::Compiler->target.is64Bit() && cg->fej9()->generateCompressedLockWord())
      {
      generateRegRegInstruction(CMP4RegReg, node, monReg, vmThreadReg, cg);
      }
   else
      {
      generateRegRegInstruction(CMPRegReg(), node, monReg, vmThreadReg, cg);
      }

   if (fallThroughConditions)
      generateLabelInstruction(JE4, node, fallThroughLabel, fallThroughConditions, cg);
   else
      generateLabelInstruction(JE4, node, fallThroughLabel, cg);

   //xabort
   generateImmInstruction(XABORT, node, 0x01, cg);

   cg->stopUsingRegister(monReg);
   //fall_back_path:
   generateLabelInstruction(LABEL, node, fallBackPathLabel, cg);

   endLabelConditions = generateRegisterDependencyConditions((uint8_t)0, 1, cg);
   endLabelConditions->addPostCondition(accReg, TR::RealRegister::eax, cg);
   endLabelConditions->stopAddingConditions();

   // test eax, 0x2
   generateRegImmInstruction(TEST1AccImm1, node, accReg, 0x2, cg);
   generateLabelInstruction(JNE4, node, gotoTransientFailure, cg);

   // abort because of nonzero lockword is also transient failure
   generateRegImmInstruction(TEST4AccImm4, node, accReg, 0x00000001, cg);
   if (persistentConditions)
      generateLabelInstruction(JE4, node, persistentFailureLabel, persistentConditions, cg);
   else
      generateLabelInstruction(JE4, node, persistentFailureLabel, cg);

   generateRegImmInstruction(TEST4AccImm4, node, accReg, 0x01000000, cg);
   // je gotransientFailureNodeLabel
   generateLabelInstruction(JNE4, node, gotoTransientFailure, cg);

   if (persistentConditions)
      generateLabelInstruction(JMP4, node, persistentFailureLabel, persistentConditions, cg);
   else
      generateLabelInstruction(JMP4, node, persistentFailureLabel, cg);
   cg->stopUsingRegister(accReg);

   // gotoTransientFailureLabel:
   if (transientConditions)
      generateLabelInstruction(LABEL, node, gotoTransientFailure, transientConditions, cg);
   else
      generateLabelInstruction(LABEL, node, gotoTransientFailure, cg);

   //delay
   TR::Register *counterReg = cg->allocateRegister();
   generateRegImmInstruction(MOV4RegImm4, node, counterReg, 100, cg);
   TR::LabelSymbol *spinLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   generateLabelInstruction(LABEL, node, spinLabel, cg);
   generateInstruction(PAUSE, node, cg);
   generateInstruction(PAUSE, node, cg);
   generateInstruction(PAUSE, node, cg);
   generateInstruction(PAUSE, node, cg);
   generateInstruction(PAUSE, node, cg);
   generateRegInstruction(DEC4Reg, node, counterReg, cg);
   TR::RegisterDependencyConditions *loopConditions = generateRegisterDependencyConditions((uint8_t)0, 1, cg);
   loopConditions->addPostCondition(counterReg, TR::RealRegister::NoReg, cg);
   loopConditions->stopAddingConditions();
   generateLabelInstruction(JNE4, node, spinLabel, loopConditions, cg);
   cg->stopUsingRegister(counterReg);

   if(transientConditions)
      generateLabelInstruction(JMP4, node, transientFailureLabel, transientConditions, cg);
   else
      generateLabelInstruction(JMP4, node, transientFailureLabel, cg);

   generateLabelInstruction(LABEL, node, endLabel, endLabelConditions, cg);
   cg->decReferenceCount(objNode);
   cg->decReferenceCount(persistentFailureNode);
   cg->decReferenceCount(transientFailureNode);
   return NULL;
   }

TR::Register *
J9::X86::TreeEvaluator::tfinishEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   generateInstruction(XEND, node, cg);
   return NULL;
   }

TR::Register *
J9::X86::TreeEvaluator::tabortEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   generateImmInstruction(XABORT, node, 0x04, cg);
   return NULL;
   }

TR::Register *
J9::X86::TreeEvaluator::directCallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {

   static bool useJapaneseCompression = (feGetEnv("TR_JapaneseComp") != NULL);
   TR::Compilation *comp = cg->comp();
   TR::SymbolReference *symRef = node->getSymbolReference();

   bool callInlined = false;
   TR::Register *returnRegister = NULL;
   TR::MethodSymbol *symbol = node->getSymbol()->castToMethodSymbol();

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
   if (cg->inlineCryptoMethod(node, returnRegister))
      {
      return returnRegister;
      }
#endif

   switch (symbol->getMandatoryRecognizedMethod())
      {
      case TR::com_ibm_jit_JITHelpers_transformedEncodeUTF16Big:
      case TR::com_ibm_jit_JITHelpers_transformedEncodeUTF16Little:
         return TR::TreeEvaluator::encodeUTF16Evaluator(node, cg);

      default:
         break;
      }

   switch (symbol->getRecognizedMethod())
      {
      case TR::java_nio_Bits_keepAlive:
         {
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
         TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions((uint8_t)1, (uint8_t)1, cg);
         deps->addPreCondition  (valueToKeepAlive, TR::RealRegister::NoReg, cg);
         deps->addPostCondition (valueToKeepAlive, TR::RealRegister::NoReg, cg);
         new (cg->trHeapMemory()) TR::X86LabelInstruction(LABEL, node, generateLabelSymbol(cg), deps, cg);
         cg->decReferenceCount(node->getFirstChild());

         return NULL; // keepAlive has no return value
         }

      case TR::java_math_BigDecimal_noLLOverflowAdd:
      case TR::java_math_BigDecimal_noLLOverflowMul:
         if (cg->getSupportsBDLLHardwareOverflowCheck())
            {
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

      case TR::java_lang_Integer_rotateLeft:
      case TR::java_lang_Math_sqrt:
      case TR::java_lang_StrictMath_sqrt:
      case TR::java_lang_Math_max_I:
      case TR::java_lang_Math_min_I:
      case TR::java_lang_Math_max_L:
      case TR::java_lang_Math_min_L:
      case TR::java_lang_Math_abs_L:
      case TR::java_lang_Math_abs_D:
      case TR::java_lang_Math_abs_F:
      case TR::java_lang_Math_abs_I:
      case TR::java_lang_Long_reverseBytes:
      case TR::java_lang_Integer_reverseBytes:
      case TR::java_lang_Short_reverseBytes:
      case TR::java_lang_Class_isAssignableFrom:
      case TR::java_lang_System_nanoTime:
      case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordCAS:
      case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordCAS:
      case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordSet:
      case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordSet:
      case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordCASSupported:
      case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordCASSupported:
      case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordSetSupported:
      case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordSetSupported:
      case TR::java_util_concurrent_atomic_Fences_orderAccesses:
      case TR::java_util_concurrent_atomic_Fences_orderReads:
      case TR::java_util_concurrent_atomic_Fences_orderWrites:
      case TR::java_util_concurrent_atomic_Fences_reachabilityFence:
      case TR::sun_nio_ch_NativeThread_current:
      case TR::sun_misc_Unsafe_copyMemory:
      case TR::java_lang_String_hashCodeImplCompressed:
      case TR::java_lang_String_hashCodeImplDecompressed:
         if (TR::TreeEvaluator::VMinlineCallEvaluator(node, false, cg))
            {
            returnRegister = node->getRegister();
            }
         else
            {
            returnRegister = TR::TreeEvaluator::performCall(node, false, true, cg);
            }

         callInlined = true;
         break;

      case TR::java_lang_String_compress:
         return TR::TreeEvaluator::compressStringEvaluator(node, cg, useJapaneseCompression);

      case TR::java_lang_String_compressNoCheck:
         return TR::TreeEvaluator::compressStringNoCheckEvaluator(node, cg, useJapaneseCompression);

      case TR::java_lang_String_andOR:
         return TR::TreeEvaluator::andORStringEvaluator(node, cg);

      default:
         break;
      }


   // If the method to be called is marked as an inline method, see if it can
   // actually be generated inline.
   //
   if (!callInlined && (symbol->isVMInternalNative() || symbol->isJITInternalNative()))
      {
      if (TR::TreeEvaluator::VMinlineCallEvaluator(node, false, cg))
         returnRegister = node->getRegister();
      else
         returnRegister = TR::TreeEvaluator::performCall(node, false, true, cg);

      callInlined = true;
      }

   if (callInlined)
      {
      // A strictfp caller needs to adjust double return values;
      // a float callee always returns values that have correct precision.
      //
      if (returnRegister &&
          returnRegister->needsPrecisionAdjustment() &&
          comp->getCurrentMethod()->isStrictFP())
         {
         TR::TreeEvaluator::insertPrecisionAdjustment(returnRegister, node, cg);
         }

      return returnRegister;
      }

   // Call was not inlined.  Delegate to the parent directCallEvaluator.
   //
   return J9::TreeEvaluator::directCallEvaluator(node, cg); 
   }


TR::Register *
J9::X86::TreeEvaluator::encodeUTF16Evaluator(TR::Node *node, TR::CodeGenerator *cg)
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
   TR::RegisterDependencyConditions *deps =
      generateRegisterDependencyConditions((uint8_t)0, depCount, cg);

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
   if (TR::Compiler->target.is64Bit())
      helper = bigEndian ? TR_AMD64encodeUTF16Big : TR_AMD64encodeUTF16Little;
   else
      helper = bigEndian ? TR_IA32encodeUTF16Big : TR_IA32encodeUTF16Little;

   generateHelperCallInstruction(node, helper, deps, cg);

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


TR::Register *
J9::X86::TreeEvaluator::compressStringEvaluator(
      TR::Node *node,
      TR::CodeGenerator *cg,
      bool japaneseMethod)
   {
   TR::Node *srcObjNode, *dstObjNode, *startNode, *lengthNode;
   TR::Register *srcObjReg, *dstObjReg, *lengthReg, *startReg;
   bool stopUsingCopyReg1, stopUsingCopyReg2, stopUsingCopyReg3, stopUsingCopyReg4;

   srcObjNode = node->getChild(0);
   dstObjNode = node->getChild(1);
   startNode = node->getChild(2);
   lengthNode = node->getChild(3);

   stopUsingCopyReg1 = TR::TreeEvaluator::stopUsingCopyRegAddr(srcObjNode, srcObjReg, cg);
   stopUsingCopyReg2 = TR::TreeEvaluator::stopUsingCopyRegAddr(dstObjNode, dstObjReg, cg);
   stopUsingCopyReg3 = TR::TreeEvaluator::stopUsingCopyRegInteger(startNode, startReg, cg);
   stopUsingCopyReg4 = TR::TreeEvaluator::stopUsingCopyRegInteger(lengthNode, lengthReg, cg);

   uintptrj_t hdrSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
   generateRegImmInstruction(ADDRegImms(), node, srcObjReg, hdrSize, cg);
   generateRegImmInstruction(ADDRegImms(), node, dstObjReg, hdrSize, cg);


   // Now that we have all the registers, set up the dependencies
   TR::RegisterDependencyConditions  *dependencies =
      generateRegisterDependencyConditions((uint8_t)0, 6, cg);
   TR::Register *resultReg = cg->allocateRegister();
   TR::Register *dummy = cg->allocateRegister();
   dependencies->addPostCondition(srcObjReg, TR::RealRegister::esi, cg);
   dependencies->addPostCondition(dstObjReg, TR::RealRegister::edi, cg);
   dependencies->addPostCondition(lengthReg, TR::RealRegister::ecx, cg);
   dependencies->addPostCondition(startReg, TR::RealRegister::eax, cg);
   dependencies->addPostCondition(resultReg, TR::RealRegister::edx, cg);
   dependencies->addPostCondition(dummy, TR::RealRegister::ebx, cg);
   dependencies->stopAddingConditions();

   TR_RuntimeHelper helper;
   if (TR::Compiler->target.is64Bit())
      helper = japaneseMethod ? TR_AMD64compressStringJ : TR_AMD64compressString;
   else
      helper = japaneseMethod ? TR_IA32compressStringJ : TR_IA32compressString;
   generateHelperCallInstruction(node, helper, dependencies, cg);
   cg->stopUsingRegister(dummy);

   for (uint16_t i = 0; i < node->getNumChildren(); i++)
     cg->decReferenceCount(node->getChild(i));

   if (stopUsingCopyReg1)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(srcObjReg);
   if (stopUsingCopyReg2)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(dstObjReg);
   if (stopUsingCopyReg3)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(startReg);
   if (stopUsingCopyReg4)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(lengthReg);
   node->setRegister(resultReg);
   return resultReg;
   }


TR::Register *
J9::X86::TreeEvaluator::compressStringNoCheckEvaluator(
      TR::Node *node,
      TR::CodeGenerator *cg,
      bool japaneseMethod)
   {
   TR::Node *srcObjNode, *dstObjNode, *startNode, *lengthNode;
   TR::Register *srcObjReg, *dstObjReg, *lengthReg, *startReg;
   bool stopUsingCopyReg1, stopUsingCopyReg2, stopUsingCopyReg3, stopUsingCopyReg4;

   srcObjNode = node->getChild(0);
   dstObjNode = node->getChild(1);
   startNode = node->getChild(2);
   lengthNode = node->getChild(3);

   stopUsingCopyReg1 = TR::TreeEvaluator::stopUsingCopyRegAddr(srcObjNode, srcObjReg, cg);
   stopUsingCopyReg2 = TR::TreeEvaluator::stopUsingCopyRegAddr(dstObjNode, dstObjReg, cg);
   stopUsingCopyReg3 = TR::TreeEvaluator::stopUsingCopyRegInteger(startNode, startReg, cg);
   stopUsingCopyReg4 = TR::TreeEvaluator::stopUsingCopyRegInteger(lengthNode, lengthReg, cg);

   uintptrj_t hdrSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
   generateRegImmInstruction(ADDRegImms(), node, srcObjReg, hdrSize, cg);
   generateRegImmInstruction(ADDRegImms(), node, dstObjReg, hdrSize, cg);


   // Now that we have all the registers, set up the dependencies
   TR::RegisterDependencyConditions  *dependencies =
      generateRegisterDependencyConditions((uint8_t)0, 5, cg);
   dependencies->addPostCondition(srcObjReg, TR::RealRegister::esi, cg);
   dependencies->addPostCondition(dstObjReg, TR::RealRegister::edi, cg);
   dependencies->addPostCondition(lengthReg, TR::RealRegister::ecx, cg);
   dependencies->addPostCondition(startReg, TR::RealRegister::eax, cg);
   TR::Register *dummy = cg->allocateRegister();
   dependencies->addPostCondition(dummy, TR::RealRegister::ebx, cg);
   dependencies->stopAddingConditions();

   TR_RuntimeHelper helper;
   if (TR::Compiler->target.is64Bit())
      helper = japaneseMethod ? TR_AMD64compressStringNoCheckJ : TR_AMD64compressStringNoCheck;
   else
      helper = japaneseMethod ? TR_IA32compressStringNoCheckJ : TR_IA32compressStringNoCheck;

   generateHelperCallInstruction(node, helper, dependencies, cg);
   cg->stopUsingRegister(dummy);

   for (uint16_t i = 0; i < node->getNumChildren(); i++)
     cg->decReferenceCount(node->getChild(i));

   if (stopUsingCopyReg1)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(srcObjReg);
   if (stopUsingCopyReg2)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(dstObjReg);
   if (stopUsingCopyReg3)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(startReg);
   if (stopUsingCopyReg4)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(lengthReg);
   return NULL;
   }


TR::Register *
J9::X86::TreeEvaluator::andORStringEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *srcObjNode, *startNode, *lengthNode;
   TR::Register *srcObjReg, *lengthReg, *startReg;
   bool stopUsingCopyReg1, stopUsingCopyReg2, stopUsingCopyReg3;

   srcObjNode = node->getChild(0);
   startNode = node->getChild(1);
   lengthNode = node->getChild(2);

   stopUsingCopyReg1 = TR::TreeEvaluator::stopUsingCopyRegAddr(srcObjNode, srcObjReg, cg);
   stopUsingCopyReg2 = TR::TreeEvaluator::stopUsingCopyRegInteger(startNode, startReg, cg);
   stopUsingCopyReg3 = TR::TreeEvaluator::stopUsingCopyRegInteger(lengthNode, lengthReg, cg);

   uintptrj_t hdrSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
   generateRegImmInstruction(ADDRegImms(), node, srcObjReg, hdrSize, cg);

   // Now that we have all the registers, set up the dependencies
   TR::RegisterDependencyConditions  *dependencies =
      generateRegisterDependencyConditions((uint8_t)0, 5, cg);
   TR::Register *resultReg = cg->allocateRegister();
   dependencies->addPostCondition(srcObjReg, TR::RealRegister::esi, cg);
   dependencies->addPostCondition(lengthReg, TR::RealRegister::ecx, cg);
   dependencies->addPostCondition(startReg, TR::RealRegister::eax, cg);
   dependencies->addPostCondition(resultReg, TR::RealRegister::edx, cg);
   TR::Register *dummy = cg->allocateRegister();
   dependencies->addPostCondition(dummy, TR::RealRegister::ebx, cg);
   dependencies->stopAddingConditions();

   TR_RuntimeHelper helper =
      TR::Compiler->target.is64Bit() ? TR_AMD64andORString : TR_IA32andORString;
   generateHelperCallInstruction(node, helper, dependencies, cg);
   cg->stopUsingRegister(dummy);

   for (uint16_t i = 0; i < node->getNumChildren(); i++)
     cg->decReferenceCount(node->getChild(i));

   if (stopUsingCopyReg1)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(srcObjReg);
   if (stopUsingCopyReg2)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(startReg);
   if (stopUsingCopyReg3)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(lengthReg);
   node->setRegister(resultReg);
   return resultReg;
   }
