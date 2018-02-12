/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifdef TR_TARGET_POWER
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "j9modron.h"
#include "thrdsup.h"
#include "thrtypes.h"
#include "codegen/AheadOfTimeCompile.hpp"           // for AheadOfTimeCompile
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CHTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "infra/Annotations.hpp"
#include "infra/Bit.hpp"
#include "p/codegen/ForceRecompilationSnippet.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/InterfaceCastSnippet.hpp"
#include "p/codegen/J9PPCSnippet.hpp"
//#include "p/codegen/PPCAheadOfTimeCompile.hpp"
#include "p/codegen/PPCEvaluator.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/PPCJNILinkage.hpp"
#include "p/codegen/PPCPrivateLinkage.hpp"
#include "p/codegen/PPCTableOfConstants.hpp"
#include "runtime/CodeCache.hpp"
#include "trj9/env/VMJ9.h"

#define AIX_NULL_ZERO_AREA_SIZE 256
#define NUM_PICS 3

extern TR::Register *addConstantToLong(TR::Node * node, TR::Register *srcReg, int64_t value, TR::Register *trgReg, TR::CodeGenerator *cg);
extern TR::Register *addConstantToInteger(TR::Node * node, TR::Register *trgReg, TR::Register *srcReg, int32_t value, TR::CodeGenerator *cg);
extern void addPrefetch(TR::CodeGenerator *cg, TR::Node *node, TR::Register *targetRegister);

static const char *ppcSupportsReadMonitors = feGetEnv("TR_ppcSupportReadMonitors");

extern uint32_t getPPCCacheLineSize();

static TR::RegisterDependencyConditions *createConditionsAndPopulateVSXDeps(TR::CodeGenerator *cg, int32_t nonVSXDepsCount)
   {
   TR::RegisterDependencyConditions *conditions;
   TR_LiveRegisters *lrVector = cg->getLiveRegisters(TR_VSX_VECTOR);
   bool liveVSXVectorReg = (!lrVector || (lrVector->getNumberOfLiveRegisters() > 0));
   const TR::PPCLinkageProperties& properties = cg->getLinkage()->getProperties();
   if (liveVSXVectorReg)
      {
      int32_t depsCount = 64 + nonVSXDepsCount;
      conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(depsCount, depsCount, cg->trMemory());
      for (int32_t i = TR::RealRegister::FirstFPR; i <= TR::RealRegister::LastFPR; i++)
         {
         if (!properties.getPreserved((TR::RealRegister::RegNum) i))
            {
            addDependency(conditions, NULL, (TR::RealRegister::RegNum) i, TR_FPR, cg);
            }
         }
      for (int32_t i = TR::RealRegister::FirstVRF; i <= TR::RealRegister::LastVRF; i++)
         {
         if (!properties.getPreserved((TR::RealRegister::RegNum) i))
            {
            addDependency(conditions, NULL, (TR::RealRegister::RegNum) i, TR_VRF, cg);
            }
         }
      }
   else
      {
      TR_LiveRegisters *lrScalar = cg->getLiveRegisters(TR_VSX_SCALAR);
      if (!lrScalar || (lrScalar->getNumberOfLiveRegisters() > 0))
         {
         int32_t depsCount = 32 + nonVSXDepsCount;
         conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(depsCount, depsCount, cg->trMemory());
         for (int32_t i = TR::RealRegister::FirstVRF; i <= TR::RealRegister::LastVRF; i++)
            {
            if (!properties.getPreserved((TR::RealRegister::RegNum) i))
               {
               addDependency(conditions, NULL, (TR::RealRegister::RegNum) i, TR_VRF, cg);
               }
            }
         }
      else
         {
         conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(nonVSXDepsCount, nonVSXDepsCount, cg->trMemory());
         }
      }
   return conditions;
   }

static TR::Instruction *genNullTest(TR::Node *node, TR::Register *objectReg, TR::Register *crReg, TR::CodeGenerator *cg, TR::Instruction *cursor = NULL)
   {
#if (NULLVALUE <= UPPER_IMMED && NULLVALUE >= LOWER_IMMED)
   cursor = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, crReg, objectReg, NULLVALUE, cursor);
#else
#error "The NULL value cannot be contained in a 16-bit signed immediate."
#endif
   return cursor;
   }

static inline TR::Instruction *genNullTest(TR::Node *node, TR::Register *objectReg, TR::Register *crReg, TR::Register *scratchReg, TR::Instruction *cursor, TR::CodeGenerator *cg)
   {
   TR::Instruction *iRet;

#if (NULLVALUE<=UPPER_IMMED && NULLVALUE>=LOWER_IMMED)
   iRet = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, crReg, objectReg, NULLVALUE, cursor);
#else
   iRet = loadConstant(cg, node, NULLVALUE, scratchReg, cursor);
   iRet = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, crReg, objectReg,
         scratchReg, (cursor==NULL)?NULL:iRet);
#endif
   return (iRet);
   }

static void genInlineTest(TR::Node * node, TR_OpaqueClassBlock* castClassAddr, TR_OpaqueClassBlock** guessClassArray, uint8_t num_PICS, TR::Register * objClassReg,
      TR::Register * resultReg, TR::Register * cndReg, TR::Register * scratch1Reg, TR::Register * scratch2Reg, bool checkCast, bool needsResult, TR::LabelSymbol * falseLabel,
      TR::LabelSymbol * trueLabel, TR::LabelSymbol * doneLabel, TR::LabelSymbol * callLabel, bool testCastClassIsSuper, TR::Instruction *iCursor, TR::CodeGenerator * cg)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (comp->fe());
   dumpOptDetails(comp, "inline test with %d guess classes\n", num_PICS);
   TR_ASSERT(num_PICS != 0, "genInlineTest is called with at least 1 guess class\n");

   TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);

   // TODO: we pass callLabel twice in TR::PPCInterfaceCastSnippet. However, this is because the first one is used as
   // the restart point on TR::PPCInterfaceCastSnippet, and the second as the reference to the call to the helper function
   // this lets us have a more generic Snippet. This can be simplified later on, should it be needed.
   //
   TR::PPCInterfaceCastSnippet * guessCacheSnippet = new (cg->trHeapMemory()) TR::PPCInterfaceCastSnippet(cg, node, callLabel, snippetLabel, trueLabel, falseLabel, doneLabel,
         callLabel, testCastClassIsSuper, checkCast, TR::Compiler->om.offsetOfObjectVftField(), offsetof(J9Class, castClassCache), needsResult);
   cg->addSnippet(guessCacheSnippet);

   uint8_t i;
   bool result_bool;
   int32_t result_value;
   TR::LabelSymbol *result_label;
   for (i = 0; i < num_PICS - 1; i++)
      {
      // Load the cached value in scratch1Reg
      // HCR in genInlineTest for checkcast and instanceof
      if (cg->wantToPatchClassPointer(guessClassArray[i], node))
         iCursor = loadAddressConstantInSnippet(cg, node, (intptrj_t) (guessClassArray[i]), scratch1Reg, scratch2Reg,TR::InstOpCode::Op_load, true, iCursor);
      else
         iCursor = loadAddressConstant(cg, node, (intptrj_t) (guessClassArray[i]), scratch1Reg, iCursor);
      result_bool = instanceOfOrCheckCast((J9Class*) guessClassArray[i], (J9Class*) castClassAddr);
      int32_t result_value = result_bool ? 1 : 0;
      result_label = (falseLabel != trueLabel) ? (result_bool ? trueLabel : falseLabel) : doneLabel;
      iCursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, cndReg, objClassReg, scratch1Reg, iCursor);
      if (needsResult)
         iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, result_value, iCursor);
      iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, result_label, cndReg, iCursor);
      }

   // Load the cached value in scratch1Reg
   if (cg->wantToPatchClassPointer(guessClassArray[num_PICS - 1], node))
      iCursor = loadAddressConstantInSnippet(cg, node, (intptrj_t) (guessClassArray[num_PICS - 1]), scratch1Reg, scratch2Reg,TR::InstOpCode::Op_load, true, iCursor);
   else
      iCursor = loadAddressConstant(cg, node, (intptrj_t) (guessClassArray[num_PICS - 1]), scratch1Reg, iCursor);
   iCursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, cndReg, objClassReg, scratch1Reg, iCursor);
   iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, snippetLabel, cndReg, iCursor);
   result_bool = instanceOfOrCheckCast((J9Class*) guessClassArray[num_PICS - 1], (J9Class*) castClassAddr);
   result_value = result_bool ? 1 : 0;
   result_label = (falseLabel != trueLabel) ? (result_bool ? trueLabel : falseLabel) : doneLabel;
   if (needsResult)
      {
      iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, result_value, iCursor);
      }
   iCursor = generateLabelInstruction(cg, TR::InstOpCode::b, node, result_label, iCursor);
   return;
   }

static TR::Register *nonFixedDependency(TR::RegisterDependencyConditions *conditions, TR::Register *nonFixedReg, int32_t *depIndex, TR_RegisterKinds kind, bool excludeGR0,
      TR::CodeGenerator *cg)
   {
   int32_t index = *depIndex;

   if (nonFixedReg == NULL)
      nonFixedReg = cg->allocateRegister(kind);

   addDependency(conditions, nonFixedReg, TR::RealRegister::NoReg, kind, cg);

   if (excludeGR0)
      {
      conditions->getPreConditions()->getRegisterDependency(index)->setExcludeGPR0();
      conditions->getPostConditions()->getRegisterDependency(index)->setExcludeGPR0();
      }

   *depIndex += 1;
   return nonFixedReg;
   }

static void reviveResultRegister(TR::Register *realResult, TR::Register *deadRes, TR::CodeGenerator *cg)
   {
   TR_RegisterKinds kind = realResult->getKind();
   TR_LiveRegisters *liveRegs = cg->getLiveRegisters(kind);

   if (deadRes->isLive())
      deadRes->getLiveRegisterInfo()->decNodeCount();
   cg->stopUsingRegister(deadRes);

   if (liveRegs != NULL)
      {
      liveRegs->addRegister(realResult);
      }
   }

static int32_t numberOfRegisterCandidate(TR::CodeGenerator *cg, TR::Node *depNode, TR_RegisterKinds kind)
   {
   int32_t idx, result = 0;

   for (idx = 0; idx < depNode->getNumChildren(); idx++)
      {
      TR::Node *child = depNode->getChild(idx);
      TR::Register *reg;

      if (child->getOpCodeValue() == TR::PassThrough)
         child = child->getFirstChild();
      reg = child->getRegister();
      if (reg != NULL && reg->getKind() == kind)
         {
         result += 1;
         if (kind == TR_GPR && TR::Compiler->target.is32Bit() && child->getType().isInt64())
            result += 1;
         }
      }
   return (result);
   }

// ----------------------------------------------------------------------------



/*
 * J9 PPC specific tree evaluator table overrides
 */
extern void TEMPORARY_initJ9PPCTreeEvaluatorTable(TR::CodeGenerator *cg)
   {
   TR_TreeEvaluatorFunctionPointer *tet = cg->getTreeEvaluatorTable();

   tet[TR::wrtbar] = TR::TreeEvaluator::wrtbarEvaluator;
   tet[TR::wrtbari] = TR::TreeEvaluator::iwrtbarEvaluator;
   tet[TR::monent] = TR::TreeEvaluator::monentEvaluator;
   tet[TR::monexit] = TR::TreeEvaluator::monexitEvaluator;
   tet[TR::monexitfence] = TR::TreeEvaluator::monexitfenceEvaluator;
   tet[TR::asynccheck] = TR::TreeEvaluator::asynccheckEvaluator;
   tet[TR::instanceof] = TR::TreeEvaluator::instanceofEvaluator;
   tet[TR::checkcast] = TR::TreeEvaluator::checkcastEvaluator;
   tet[TR::checkcastAndNULLCHK] = TR::TreeEvaluator::checkcastAndNULLCHKEvaluator;
   tet[TR::New] = TR::TreeEvaluator::newObjectEvaluator;
   tet[TR::variableNew] = TR::TreeEvaluator::newObjectEvaluator;
   tet[TR::newarray] = TR::TreeEvaluator::newArrayEvaluator;
   tet[TR::anewarray] = TR::TreeEvaluator::anewArrayEvaluator;
   tet[TR::variableNewArray] = TR::TreeEvaluator::anewArrayEvaluator;
   tet[TR::multianewarray] = TR::TreeEvaluator::multianewArrayEvaluator;
   tet[TR::arraylength] = TR::TreeEvaluator::arraylengthEvaluator;
   tet[TR::ResolveCHK] = TR::TreeEvaluator::resolveCHKEvaluator;
   tet[TR::DIVCHK] = TR::TreeEvaluator::DIVCHKEvaluator;
   tet[TR::BNDCHK] = TR::TreeEvaluator::BNDCHKEvaluator;
   tet[TR::ArrayCopyBNDCHK] = TR::TreeEvaluator::ArrayCopyBNDCHKEvaluator;
   tet[TR::BNDCHKwithSpineCHK] = TR::TreeEvaluator::BNDCHKwithSpineCHKEvaluator;
   tet[TR::SpineCHK] = TR::TreeEvaluator::BNDCHKwithSpineCHKEvaluator;
   tet[TR::ArrayStoreCHK] = TR::TreeEvaluator::ArrayStoreCHKEvaluator;
   tet[TR::ArrayCHK] = TR::TreeEvaluator::ArrayCHKEvaluator;
   tet[TR::MethodEnterHook] = TR::TreeEvaluator::conditionalHelperEvaluator;
   tet[TR::MethodExitHook] = TR::TreeEvaluator::conditionalHelperEvaluator;
   tet[TR::allocationFence] = TR::TreeEvaluator::flushEvaluator;
   tet[TR::loadFence] = TR::TreeEvaluator::flushEvaluator;
   tet[TR::storeFence] = TR::TreeEvaluator::flushEvaluator;
   tet[TR::fullFence] = TR::TreeEvaluator::flushEvaluator;

   tet[TR::icall]  = TR::TreeEvaluator::directCallEvaluator;
   tet[TR::lcall]  = TR::TreeEvaluator::directCallEvaluator;
   tet[TR::fcall]  = TR::TreeEvaluator::directCallEvaluator;
   tet[TR::dcall]  = TR::TreeEvaluator::directCallEvaluator;
   tet[TR::acall]  = TR::TreeEvaluator::directCallEvaluator;
   tet[TR::call]   = TR::TreeEvaluator::directCallEvaluator;
   tet[TR::iucall] = TR::TreeEvaluator::directCallEvaluator;
   tet[TR::lucall] = TR::TreeEvaluator::directCallEvaluator;
   tet[TR::vcall]  = TR::TreeEvaluator::directCallEvaluator;

   tet[TR::tstart] = TR::TreeEvaluator::tstartEvaluator;
   tet[TR::tfinish] = TR::TreeEvaluator::tfinishEvaluator;
   tet[TR::tabort] = TR::TreeEvaluator::tabortEvaluator;

   }


static void
VMoutlinedHelperWrtbarEvaluator(
      TR::Node *node,
      TR::Register *srcObjectReg,
      TR::Register *dstObjectReg,
      bool srcIsNonNull,
      TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   const TR_WriteBarrierKind gcMode = comp->getOptions()->getGcMode();

   if (gcMode == TR_WrtbarNone)
      return;

   const bool doWrtbar = gcMode == TR_WrtbarRealTime || gcMode == TR_WrtbarOldCheck || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarAlways
         || TR::Options::getCmdLineOptions()->realTimeGC();

   const bool doCardMark = !node->isNonHeapObjectWrtBar() && (gcMode == TR_WrtbarCardMark || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarCardMarkIncremental);

   TR::CodeCache *codeCache = comp->getCurrentCodeCache();
   TR::LabelSymbol *doneWrtbarLabel = generateLabelSymbol(cg);
   TR::Register *srcNullCondReg;

   TR_PPCScratchRegisterDependencyConditions deps;

   // Clobbered by the helper
   deps.addDependency(cg, NULL, TR::RealRegister::cr0);
   deps.addDependency(cg, NULL, TR::RealRegister::cr1);

   if (!srcIsNonNull)
      {
      TR_ASSERT(NULLVALUE <= UPPER_IMMED && NULLVALUE >= LOWER_IMMED, "Expecting NULLVALUE to fit in immediate field");
      srcNullCondReg = cg->allocateRegister(TR_CCR);
      generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, srcNullCondReg, srcObjectReg, NULLVALUE);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneWrtbarLabel, srcNullCondReg);
      }

   if (doWrtbar)
      {
      deps.addDependency(cg, dstObjectReg, TR::RealRegister::gr3);
      deps.addDependency(cg, srcObjectReg, TR::RealRegister::gr4);
      // Clobbered by the helper
      deps.addDependency(cg, NULL, TR::RealRegister::gr5);
      deps.addDependency(cg, NULL, TR::RealRegister::gr6);
      deps.addDependency(cg, NULL, TR::RealRegister::gr11);

      TR_CCPreLoadedCode helper = doCardMark ? TR_writeBarrierAndCardMark : TR_writeBarrier;
      uintptrj_t helperAddr = (uintptrj_t) codeCache->getCCPreLoadedCodeAddress(helper, cg);
      TR::SymbolReference *symRef = comp->getSymRefTab()->findOrCreatePerCodeCacheHelperSymbolRef(helper, helperAddr);

      TR::Instruction *gcPoint = generateDepImmSymInstruction(cg, TR::InstOpCode::bl, node, helperAddr, new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 0, cg->trMemory()),
            symRef);
      gcPoint->PPCNeedsGCMap(0xFFFFFFFF);
      }
   else
      {
      TR_ASSERT(doCardMark, "Expecting at least one write barrier to be performed");

      deps.addDependency(cg, dstObjectReg, TR::RealRegister::gr3);
      // Clobbered by the helper
      deps.addDependency(cg, NULL, TR::RealRegister::gr4);
      deps.addDependency(cg, NULL, TR::RealRegister::gr5);
      deps.addDependency(cg, NULL, TR::RealRegister::gr11);

      uintptrj_t helperAddr = (uintptrj_t) codeCache->getCCPreLoadedCodeAddress(TR_cardMark, cg);
      TR::SymbolReference *symRef = comp->getSymRefTab()->findOrCreatePerCodeCacheHelperSymbolRef(TR_cardMark, helperAddr);
      generateDepImmSymInstruction(cg, TR::InstOpCode::bl, node, helperAddr, new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 0, cg->trMemory()), symRef);
      }

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneWrtbarLabel, TR_PPCScratchRegisterDependencyConditions::createDependencyConditions(cg, NULL, &deps));

   if (!srcIsNonNull)
      cg->stopUsingRegister(srcNullCondReg);

   cg->machine()->setLinkRegisterKilled(true);
   cg->setHasCall();
   }

TR::Register *outlinedHelperWrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *srcObjectReg = cg->gprClobberEvaluate(node->getFirstChild());
   TR::Register *dstObjectReg = cg->gprClobberEvaluate(node->getSecondChild());
   TR::MemoryReference *memRef;
   TR::Compilation* comp = cg->comp();

   TR::Symbol *storeSym = node->getSymbolReference()->getSymbol();
   const bool isOrderedShadowStore = storeSym->isShadow() && storeSym->isOrdered();
   const bool needSync = TR::Compiler->target.isSMP() && (storeSym->isSyncVolatile() || isOrderedShadowStore);
   const bool lazyVolatile = TR::Compiler->target.isSMP() && isOrderedShadowStore;

   // Under real-time, store happens after the wrtbar
   // For other GC modes, store happens before the wrtbar
   if (TR::Options::getCmdLineOptions()->realTimeGC())
      {
      }
   else
      {
      memRef = new (cg->trHeapMemory()) TR::MemoryReference(node, TR::Compiler->om.sizeofReferenceAddress(), cg);
      if (needSync)
         generateInstruction(cg, TR::InstOpCode::lwsync, node);
      generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, memRef, srcObjectReg);
      if (needSync)
         {
         // ordered and lazySet operations will not generate a post-write sync
         // and will not mark unresolved snippets with in sync sequence to avoid
         // PicBuilder overwriting the instruction that normally would have been sync
         TR::TreeEvaluator::postSyncConditions(node, cg, srcObjectReg, memRef, TR::InstOpCode::sync, lazyVolatile);
         }
      if (!node->getFirstChild()->isNull())
         VMoutlinedHelperWrtbarEvaluator(node, srcObjectReg, dstObjectReg, node->getFirstChild()->isNonNull(), cg);
      }

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   memRef->decNodeReferenceCounts(cg);

   if (srcObjectReg != node->getFirstChild()->getRegister())
      cg->stopUsingRegister(srcObjectReg);
   if (dstObjectReg != node->getSecondChild()->getRegister())
      cg->stopUsingRegister(dstObjectReg);

   return NULL;
   }

static int32_t getOffsetOfJ9ObjectFlags()
   {
#if defined(J9VM_INTERP_FLAGS_IN_CLASS_SLOT)
#if defined(TR_TARGET_64BIT) && !defined(J9VM_INTERP_COMPRESSED_OBJECT_HEADER)
#if defined(__LITTLE_ENDIAN__)
   return TMP_OFFSETOF_J9OBJECT_CLAZZ;
#else
   return TMP_OFFSETOF_J9OBJECT_CLAZZ + 4;
#endif
#else
   return TMP_OFFSETOF_J9OBJECT_CLAZZ;
#endif
#else
#if defined(TMP_OFFSETOF_J9OBJECT_FLAGS)
   return TMP_OFFSETOF_J9OBJECT_FLAGS;
#else
   return 0;
#endif
#endif
   }

static void VMnonNullSrcWrtBarCardCheckEvaluator(TR::Node *node, TR::Register *srcReg, TR::Register *dstReg, TR::Register *condReg, TR::Register *temp1Reg, TR::Register *temp2Reg,
      TR::Register *temp3Reg, TR::Register *temp4Reg, TR::LabelSymbol *doneLabel, TR::RegisterDependencyConditions *deps, TR::MemoryReference *tempMR, bool dstAddrComputed,
      bool isCompressedRef, TR::CodeGenerator *cg, bool flagsInTemp1 = false)
   {
   // non-heap objects cannot be marked
   // Make sure we really should be here
   TR::Compilation *comp = cg->comp();
   TR::Options *options = comp->getOptions();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   TR_WriteBarrierKind gcMode = comp->getOptions()->getGcMode();
   TR::LabelSymbol *callLabel = generateLabelSymbol(cg);
   TR::Node *wrtbarNode = NULL;
   TR::SymbolReference *wbRef;
   bool definitelyHeapObject = false, definitelyNonHeapObject = false;
   bool doCrdMrk = (gcMode == TR_WrtbarCardMarkAndOldCheck);
   bool doWrtBar = (gcMode == TR_WrtbarRealTime || gcMode == TR_WrtbarOldCheck || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarAlways
         || TR::Options::getCmdLineOptions()->realTimeGC());
   bool temp3RegIsNull = (temp3Reg == NULL);

   TR_ASSERT(doWrtBar == true, "VMnonNullSrcWrtBarCardCheckEvaluator: Invalid call to VMnonNullSrcWrtBarCardCheckEvaluator\n");

   if (temp3RegIsNull)
      {
      temp3Reg = cg->allocateRegister();
      addDependency(deps, temp3Reg, TR::RealRegister::NoReg, TR_GPR, cg);
      }

   if (node->getOpCodeValue() == TR::wrtbari || node->getOpCodeValue() == TR::wrtbar)
      wrtbarNode = node;
   else if (node->getOpCodeValue() == TR::ArrayStoreCHK)
      wrtbarNode = node->getFirstChild();

   if (wrtbarNode != NULL)
      {
      definitelyHeapObject = wrtbarNode->isHeapObjectWrtBar();
      definitelyNonHeapObject = wrtbarNode->isNonHeapObjectWrtBar();
      }

   if (gcMode == TR_WrtbarCardMarkAndOldCheck)
      wbRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreGenerationalAndConcurrentMarkSymbolRef(comp->getMethodSymbol());
   else if (gcMode == TR_WrtbarOldCheck)
      wbRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreGenerationalSymbolRef(comp->getMethodSymbol());
   else if (TR::Options::getCmdLineOptions()->realTimeGC())
      {
      if (wrtbarNode->getOpCodeValue() == TR::wrtbar || wrtbarNode->isUnsafeStaticWrtBar())
         wbRef = comp->getSymRefTab()->findOrCreateWriteBarrierClassStoreRealTimeGCSymbolRef(comp->getMethodSymbol());
      else
         wbRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreRealTimeGCSymbolRef(comp->getMethodSymbol());
      }
   else
      wbRef = comp->getSymRefTab()->findOrCreateWriteBarrierStoreSymbolRef(comp->getMethodSymbol());

   if (gcMode != TR_WrtbarAlways && !TR::Options::getCmdLineOptions()->realTimeGC())
      {
      bool inlineCrdmrk = (doCrdMrk && !definitelyNonHeapObject);
      // object header flags now occupy 4bytes (instead of 8) on 64-bit. Keep it in temp1Reg for following checks.

      TR::Register *metaReg = cg->getMethodMetaDataRegister();

      // dstReg - heapBaseForBarrierRange0
      TR::Register *tempRegister = (temp4Reg != NULL) ? temp4Reg : temp3Reg;
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, tempRegister,
            new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapBaseForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg));
      generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, temp3Reg, tempRegister, dstReg);

      // if (temp3Reg >= heapSizeForBarrierRage0), object not in the tenured area
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp2Reg,
            new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapSizeForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg));
      generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, temp3Reg, temp2Reg);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, node, doneLabel, condReg);

      if (!flagsInTemp1)
         generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, temp1Reg, new (cg->trHeapMemory()) TR::MemoryReference(dstReg, getOffsetOfJ9ObjectFlags(), 4, cg));

      if (inlineCrdmrk)
         {
         TR::LabelSymbol *noChkLabel = NULL;

         // Balanced policy must always dirty the card table.
         //
         if (gcMode != TR_WrtbarCardMarkIncremental)
            {
            noChkLabel = generateLabelSymbol(cg);

            generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp2Reg,
                  new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, privateFlags), TR::Compiler->om.sizeofReferenceAddress(), cg));

            // The value for J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE is a generated value when VM code is created
            // At the moment we are safe here, but it is better to be careful and avoid any unexpected behavoiur
            // Make sure this falls within the scope of andis
            //
            TR_ASSERT(J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE >= 0x00010000 && J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE <= 0x80000000,
                  "Concurrent mark active Value assumption broken.");
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andis_r, node, temp2Reg, temp2Reg, condReg, J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE >> 16);
            generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, noChkLabel, condReg);
            }

#if defined(TR_TARGET_64BIT)
         uintptr_t card_size_shift = trailingZeroes((uint64_t)options->getGcCardSize());
#else
         uintptr_t card_size_shift = trailingZeroes((uint32_t) options->getGcCardSize());
#endif

         // dirty(activeCardTableBase + temp3Reg >> card_size_shift)
         generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp2Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, activeCardTableBase), TR::Compiler->om.sizeofReferenceAddress(), cg));
#if defined(TR_TARGET_64BIT)
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicl, node, temp3Reg, temp3Reg, 64-card_size_shift, (CONSTANT64(1)<<(64-card_size_shift)) - CONSTANT64(1));
#else
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, temp3Reg, temp3Reg, 32 - card_size_shift, ((uint32_t) 0xFFFFFFFF) >> card_size_shift);
#endif
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, temp4Reg, CARD_DIRTY);
         generateMemSrc1Instruction(cg, TR::InstOpCode::stb, node, new (cg->trHeapMemory()) TR::MemoryReference(temp2Reg, temp3Reg, 1, cg), temp4Reg);

         if (noChkLabel)
            generateLabelInstruction(cg, TR::InstOpCode::label, node, noChkLabel);

         if (gcMode == TR_WrtbarCardMarkAndOldCheck)
            {
            //check for src in new space
            generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp2Reg,
                  new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapBaseForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg));
            generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, temp2Reg, temp2Reg, srcReg);

            generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp3Reg,
                  new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapSizeForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg));
            generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, temp2Reg, temp3Reg);
            generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, doneLabel, condReg);
            }
         }
      else
         {
         if (gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarOldCheck)
            {
            //check for src in new space
            if (temp4Reg != NULL)
               {
               generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, temp3Reg, temp4Reg, srcReg);
               }
            else
               {
               generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp3Reg,
                     new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapBaseForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg));
               generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, temp3Reg, temp3Reg, srcReg);
               }
            generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, temp3Reg, temp2Reg);
            generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, doneLabel, condReg);
            }
         }

      TR::Instruction * i = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, temp2Reg, temp1Reg, condReg, J9_OBJECT_HEADER_REMEMBERED_MASK_FOR_TEST);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, doneLabel, condReg);
      }
   else if (TR::Options::getCmdLineOptions()->realTimeGC())
      {
      if (!dstAddrComputed)
         {
         if (tempMR->getIndexRegister() != NULL)
            generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, temp3Reg, tempMR->getBaseRegister(), tempMR->getIndexRegister());
         else
            generateTrg1MemInstruction(cg, TR::InstOpCode::addi2, node, temp3Reg, tempMR);
         }

      if (!comp->getOption(TR_DisableInlineWriteBarriersRT))
         {
         // check if barrier is enabled: if disabled then branch around the rest

         TR::MemoryReference *fragmentParentMR = new (cg->trHeapMemory()) TR::MemoryReference(cg->getMethodMetaDataRegister(),
               fej9->thisThreadRememberedSetFragmentOffset() + fej9->getFragmentParentOffset(), TR::Compiler->om.sizeofReferenceAddress(), cg);
         generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp2Reg, fragmentParentMR);

         TR::MemoryReference *globalFragmentIDMR = new (cg->trHeapMemory()) TR::MemoryReference(temp2Reg, fej9->getRememberedSetGlobalFragmentOffset(), TR::Compiler->om.sizeofReferenceAddress(),
               cg);
         generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp1Reg, globalFragmentIDMR);

         // if barrier not enabled, nothing to do
         generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpi, node, condReg, temp1Reg, NULLVALUE);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, condReg);

         // now check if barrier absolutely necessary, according to GC: if it is, then make sure we go out-of-line
         // if the global fragment index and local fragment index don't match, go to the snippet
         TR::MemoryReference *localFragmentIndexMR = new (cg->trHeapMemory()) TR::MemoryReference(cg->getMethodMetaDataRegister(),
               fej9->thisThreadRememberedSetFragmentOffset() + fej9->getLocalFragmentOffset(), TR::Compiler->om.sizeofReferenceAddress(), cg);
         generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp2Reg, localFragmentIndexMR);
         generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpi, node, condReg, temp2Reg, NULLVALUE);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, callLabel, condReg);

         // null test on the reference we're about to store over: if it is null goto doneLabel
         TR::InstOpCode::Mnemonic opCode = TR::InstOpCode::lwz;
         int32_t size = 4;
         if (TR::Compiler->target.is64Bit() && !isCompressedRef)
            {
            opCode = TR::InstOpCode::ld;
            size = 8;
            }
         generateTrg1MemInstruction(cg, opCode, node, temp1Reg, new (cg->trHeapMemory()) TR::MemoryReference(temp3Reg, 0, size, cg));
         generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpi, node, condReg, temp1Reg, NULLVALUE);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, condReg);

         generateLabelInstruction(cg, TR::InstOpCode::label, node, callLabel);
         }
      }

   generateDepImmSymInstruction(cg, TR::InstOpCode::bl, node, (uintptrj_t) wbRef->getSymbol()->castToMethodSymbol()->getMethodAddress(),
         new (cg->trHeapMemory()) TR::RegisterDependencyConditions((uint8_t) 0, 0, cg->trMemory()), wbRef, NULL);
   cg->machine()->setLinkRegisterKilled(true);

   if (temp3RegIsNull && temp3Reg)
      cg->stopUsingRegister(temp3Reg);
   }

static void VMCardCheckEvaluator(TR::Node *node, TR::Register *dstReg, TR::Register *condReg, TR::Register *temp1Reg, TR::Register *temp2Reg, TR::Register *temp3Reg,
      TR::RegisterDependencyConditions *deps, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   // non-heap objects cannot be marked
   // Make sure we really should be here
   TR::Options *options = comp->getOptions();
   TR::Node *wrtbarNode = NULL;
   bool definitelyHeapObject = false, definitelyNonHeapObject = false;
   TR_WriteBarrierKind gcMode = comp->getOptions()->getGcMode();

   if (node->getOpCodeValue() == TR::wrtbari || node->getOpCodeValue() == TR::wrtbar)
      wrtbarNode = node;
   else if (node->getOpCodeValue() == TR::ArrayStoreCHK)
      wrtbarNode = node->getFirstChild();

   if (wrtbarNode != NULL)
      {
      definitelyHeapObject = wrtbarNode->isHeapObjectWrtBar();
      definitelyNonHeapObject = wrtbarNode->isNonHeapObjectWrtBar();
      }

   TR_ASSERT((gcMode == TR_WrtbarCardMark || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarCardMarkIncremental) && !definitelyNonHeapObject,
         "VMCardCheckEvaluator: Invalid call to cardCheckEvaluator\n");

   if (!definitelyNonHeapObject)
      {
      TR::Register *metaReg = cg->getMethodMetaDataRegister();
      TR::LabelSymbol *noChkLabel = generateLabelSymbol(cg);

      // Balanced policy must always dirty the card table.
      //
      if (gcMode != TR_WrtbarCardMarkIncremental)
         {
         generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp1Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, privateFlags), TR::Compiler->om.sizeofReferenceAddress(), cg));

         // The value for J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE is a generated value when VM code is created
         // At the moment we are safe here, but it is better to be careful and avoid any unexpected behavoiur
         // Make sure this falls within the scope of andis
         //
         TR_ASSERT(J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE >= 0x00010000 && J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE <= 0x80000000,
               "Concurrent mark active Value assumption broken.");
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andis_r, node, temp1Reg, temp1Reg, condReg, J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE >> 16);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, noChkLabel, condReg);
         }

#if defined(TR_TARGET_64BIT)
      uintptr_t card_size_shift = trailingZeroes((uint64_t)options->getGcCardSize());
#else
      uintptr_t card_size_shift = trailingZeroes((uint32_t) options->getGcCardSize());
#endif

      // temp3Reg = dstReg - heapBaseForBarrierRange0
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp3Reg,
            new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapBaseForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg));
      generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, temp3Reg, temp3Reg, dstReg);

      if (!definitelyHeapObject)
         {
         // if (temp3Reg >= heapSizeForBarrierRage0), object not in the heap
         generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp1Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapSizeForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg));
         generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, temp3Reg, temp1Reg);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, node, noChkLabel, condReg);
         }

      // dirty(activeCardTableBase + temp3Reg >> card_size_shift)
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp1Reg,
            new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, activeCardTableBase), TR::Compiler->om.sizeofReferenceAddress(), cg));
#if defined(TR_TARGET_64BIT)
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicl, node, temp3Reg, temp3Reg, 64-card_size_shift, (CONSTANT64(1)<<(64-card_size_shift)) - CONSTANT64(1));
#else
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, temp3Reg, temp3Reg, 32 - card_size_shift, ((uint32_t) 0xFFFFFFFF) >> card_size_shift);
#endif
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, temp2Reg, CARD_DIRTY);
      generateMemSrc1Instruction(cg, TR::InstOpCode::stb, node, new (cg->trHeapMemory()) TR::MemoryReference(temp1Reg, temp3Reg, 1, cg), temp2Reg);

      generateLabelInstruction(cg, TR::InstOpCode::label, node, noChkLabel);
      }

   }

static void VMwrtbarEvaluator(TR::Node *node, TR::Register *srcReg, TR::Register *dstReg, TR::Register *dstAddrReg, TR::MemoryReference *tempMR,
      TR::RegisterDependencyConditions *conditions, bool srcNonNull, bool needDeps, bool isCompressedRef, TR::CodeGenerator *cg, TR::Register *flagsReg = NULL)
   {
   TR::Compilation *comp = cg->comp();
   TR_WriteBarrierKind gcMode = comp->getOptions()->getGcMode();
   bool doWrtBar = (gcMode == TR_WrtbarRealTime || gcMode == TR_WrtbarOldCheck || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarAlways
         || TR::Options::getCmdLineOptions()->realTimeGC());
   bool doCrdMrk = ((gcMode == TR_WrtbarCardMark || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarCardMarkIncremental) && !node->isNonHeapObjectWrtBar());
   TR::LabelSymbol *label;
   TR::Register *cr0, *temp1Reg, *temp2Reg, *temp3Reg = NULL, *temp4Reg = NULL;
   uint8_t numRegs = (doWrtBar && doCrdMrk) ? 7 : 5;

   //Also need to pass in destinationAddress to jitWriteBarrierStoreMetronome
   if (TR::Options::getCmdLineOptions()->realTimeGC())
      numRegs += 3;

   if (doWrtBar || doCrdMrk)
      numRegs += 2;

   if ((!doWrtBar && !doCrdMrk) || (node->getOpCode().isWrtBar() && node->skipWrtBar()))
      {
      if (flagsReg)
         cg->stopUsingRegister(flagsReg);
      return;
      }
   if (flagsReg)
      temp1Reg = flagsReg;
   else
      temp1Reg = cg->allocateRegister();

   temp2Reg = cg->allocateRegister();
   label = generateLabelSymbol(cg);

   if (doWrtBar || doCrdMrk)
      needDeps = true;

   if (needDeps)
      {
      cr0 = cg->allocateRegister(TR_CCR);
      conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(numRegs, numRegs, cg->trMemory());
      addDependency(conditions, cr0, TR::RealRegister::cr0, TR_CCR, cg);
      addDependency(conditions, dstReg, TR::RealRegister::gr3, TR_GPR, cg);
      addDependency(conditions, temp1Reg, TR::RealRegister::gr11, TR_GPR, cg);
      if (TR::Options::getCmdLineOptions()->realTimeGC())
         {
         addDependency(conditions, temp2Reg, TR::RealRegister::NoReg, TR_GPR, cg);
         conditions->getPostConditions()->getRegisterDependency(3)->setExcludeGPR0(); //3=temp2Reg
         addDependency(conditions, dstAddrReg, TR::RealRegister::gr4, TR_GPR, cg);
         temp3Reg = dstAddrReg;

         if (node->getSymbolReference()->isUnresolved())
            {
            if (tempMR->getBaseRegister() != NULL)
               {
               addDependency(conditions, tempMR->getBaseRegister(), TR::RealRegister::NoReg, TR_GPR, cg);
               conditions->getPreConditions()->getRegisterDependency(3)->setExcludeGPR0();
               conditions->getPostConditions()->getRegisterDependency(3)->setExcludeGPR0();
               }

            if (tempMR->getIndexRegister() != NULL)
               addDependency(conditions, tempMR->getIndexRegister(), TR::RealRegister::NoReg, TR_GPR, cg);
            }
         }
      else
         addDependency(conditions, temp2Reg, TR::RealRegister::NoReg, TR_GPR, cg);
      }
   else
      cr0 = conditions->getPostConditions()->getRegisterDependency(2)->getRegister();

   if (doWrtBar)
      {
      if (doCrdMrk)
         {
         temp3Reg = cg->allocateRegister();
         temp4Reg = cg->allocateRegister();
         if (needDeps)
            {
            addDependency(conditions, temp3Reg, TR::RealRegister::NoReg, TR_GPR, cg);
            addDependency(conditions, temp4Reg, TR::RealRegister::NoReg, TR_GPR, cg);
            conditions->getPostConditions()->getRegisterDependency(3)->setExcludeGPR0(); //3=temp2Reg
            }
         }
      if (needDeps)
         {
         if (TR::Options::getCmdLineOptions()->realTimeGC())
            addDependency(conditions, srcReg, TR::RealRegister::gr5, TR_GPR, cg);
         else
            addDependency(conditions, srcReg, TR::RealRegister::gr4, TR_GPR, cg);
         }
      if (!srcNonNull && !TR::Options::getCmdLineOptions()->realTimeGC())
         {
#if (NULLVALUE <= UPPER_IMMED && NULLVALUE >= LOWER_IMMED)
         generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpi, node, cr0, srcReg, NULLVALUE);
#else
         loadConstant(node, NULLVALUE, temp1Reg);
         generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, cr0, srcReg, temp1Reg);
#endif
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, label, cr0);
         }

      VMnonNullSrcWrtBarCardCheckEvaluator(node, srcReg, dstReg, cr0, temp1Reg, temp2Reg, temp3Reg, temp4Reg, label, conditions, tempMR, false, isCompressedRef, cg,
            flagsReg != NULL);
      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, label, conditions);
      if (doCrdMrk)
         {
         cg->stopUsingRegister(temp3Reg);
         cg->stopUsingRegister(temp4Reg);
         }
      else if (TR::Options::getCmdLineOptions()->realTimeGC())
         cg->stopUsingRegister(temp3Reg);
      }
   else if (doCrdMrk)
      {
      temp3Reg = cg->allocateRegister();
      if (needDeps)
         {
         addDependency(conditions, temp3Reg, TR::RealRegister::NoReg, TR_GPR, cg);
         conditions->getPostConditions()->getRegisterDependency(1)->setExcludeGPR0(); //1=dstReg
         conditions->getPostConditions()->getRegisterDependency(2)->setExcludeGPR0(); //2=temp1Reg
         }

      VMCardCheckEvaluator(node, dstReg, cr0, temp1Reg, temp2Reg, temp3Reg, conditions, cg);
      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, label, conditions);
      cg->stopUsingRegister(temp3Reg);
      }
   if (needDeps)
      {
      cg->stopUsingRegister(cr0);
      }
   cg->stopUsingRegister(temp1Reg);
   cg->stopUsingRegister(temp2Reg);
   }

TR::Register *J9::Power::TreeEvaluator::wrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (comp->fe());

   if (comp->isOptServer() && !comp->compileRelocatableCode() && !TR::Options::getCmdLineOptions()->realTimeGC())
      {
      if (TR::Compiler->target.cpu.id() < TR_PPCp8)
         {
         static bool disableOutlinedWrtbar = feGetEnv("TR_ppcDisableOutlinedWriteBarrier") != NULL;
         if (!disableOutlinedWrtbar)
            {
            return outlinedHelperWrtbarEvaluator(node, cg);
            }
         }
      else
         {
         static bool enableOutlinedWrtbar = feGetEnv("TR_ppcEnableOutlinedWriteBarrier") != NULL;
         if (enableOutlinedWrtbar)
            {
            return outlinedHelperWrtbarEvaluator(node, cg);
            }
         }
      }

   TR::MemoryReference *tempMR = NULL;
   TR::Register *destinationRegister = cg->evaluate(node->getSecondChild());
   TR::Register *flagsReg = NULL;
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *sourceRegister;
   bool killSource = false;
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && TR::Compiler->target.isSMP());
   bool lazyVolatile = false;

   if (node->getSymbolReference()->getSymbol()->isShadow() && node->getSymbolReference()->getSymbol()->isOrdered() && TR::Compiler->target.isSMP())
      {
      needSync = true;
      lazyVolatile = true;
      }

   if (firstChild->getReferenceCount() > 1 && firstChild->getRegister() != NULL)
      {
      if (!firstChild->getRegister()->containsInternalPointer())
         sourceRegister = cg->allocateCollectedReferenceRegister();
      else
         {
         sourceRegister = cg->allocateRegister();
         sourceRegister->setPinningArrayPointer(firstChild->getRegister()->getPinningArrayPointer());
         sourceRegister->setContainsInternalPointer();
         }
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, sourceRegister, firstChild->getRegister());
      killSource = true;
      }
   else
      sourceRegister = cg->evaluate(firstChild);

   if (!node->skipWrtBar() && !node->hasUnresolvedSymbolReference()
         && (comp->getOptions()->getGcMode() == TR_WrtbarOldCheck || comp->getOptions()->getGcMode() == TR_WrtbarCardMarkAndOldCheck))
      {
      flagsReg = cg->allocateRegister();
      generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, flagsReg, new (cg->trHeapMemory()) TR::MemoryReference(destinationRegister, getOffsetOfJ9ObjectFlags(), 4, cg));
      }

   // RealTimeGC write barriers occur BEFORE the store
   if (TR::Options::getCmdLineOptions()->realTimeGC())
      {
      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, TR::Compiler->om.sizeofReferenceAddress(), cg);
      TR::Register *destinationAddressRegister = cg->allocateRegister();
      VMwrtbarEvaluator(node, sourceRegister, destinationRegister, destinationAddressRegister, tempMR, NULL, firstChild->isNonNull(), true, false, cg);

      TR::MemoryReference *dstMR = new (cg->trHeapMemory()) TR::MemoryReference(destinationAddressRegister, 0, TR::Compiler->om.sizeofReferenceAddress(), cg);

      if (needSync)
         generateInstruction(cg, TR::InstOpCode::lwsync, node);

      generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, dstMR, sourceRegister);

      if (needSync)
         {
         // ordered and lazySet operations will not generate a post-write sync
         // and will not mark unresolved snippets with in sync sequence to avoid
         // PicBuilder overwriting the instruction that normally would have been sync
         postSyncConditions(node, cg, sourceRegister, dstMR, TR::InstOpCode::sync, lazyVolatile);
         }

      cg->stopUsingRegister(destinationAddressRegister);
      }
   else
      {
      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, TR::Compiler->om.sizeofReferenceAddress(), cg);

      if (needSync)
         generateInstruction(cg, TR::InstOpCode::lwsync, node);

      generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, tempMR, sourceRegister);

      if (needSync)
         {
         // ordered and lazySet operations will not generate a post-write sync
         // and will not mark unresolved snippets with in sync sequence to avoid
         // PicBuilder overwriting the instruction that normally would have been sync
         postSyncConditions(node, cg, sourceRegister, tempMR, TR::InstOpCode::sync, lazyVolatile);
         }

      VMwrtbarEvaluator(node, sourceRegister, destinationRegister, NULL, NULL, NULL, firstChild->isNonNull(), true, false, cg, flagsReg);
      }

   if (killSource)
      cg->stopUsingRegister(sourceRegister);

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   tempMR->decNodeReferenceCounts(cg);

   return NULL;
   }

TR::Register *J9::Power::TreeEvaluator::iwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());

   TR::Register *destinationRegister = cg->evaluate(node->getChild(2));
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *sourceRegister;
   TR::Register *flagsReg = NULL;
   bool killSource = false;

   bool usingCompressedPointers = false;
   bool bumpedRefCount = false;
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && TR::Compiler->target.isSMP());
   bool lazyVolatile = false;

   if (node->getSymbolReference()->getSymbol()->isShadow() && node->getSymbolReference()->getSymbol()->isOrdered() && TR::Compiler->target.isSMP())
      {
      needSync = true;
      lazyVolatile = true;
      }

   if (comp->useCompressedPointers() && (node->getSymbolReference()->getSymbol()->getDataType() == TR::Address) && (node->getSecondChild()->getDataType() != TR::Address))
      {
      // pattern match the sequence
      //     iwrtbar f     iwrtbar f         <- node
      //       aload O       aload O
      //     value           l2i
      //                       lshr         <- translatedNode
      //                         lsub
      //                           a2l
      //                             value   <- secondChild
      //                           lconst HB
      //                         iconst shftKonst
      //
      // -or- if the field is known to be null or usingLowMemHeap
      // iwrtbar f
      //    aload O
      //    l2i
      //      a2l
      //        value  <- secondChild
      //
      TR::Node *translatedNode = secondChild;
      if (translatedNode->getOpCode().isConversion())
         translatedNode = translatedNode->getFirstChild();
      if (translatedNode->getOpCode().isRightShift()) // optional
         translatedNode = translatedNode->getFirstChild();

      bool usingLowMemHeap = false;
      if (TR::Compiler->vm.heapBaseAddress() == 0 || secondChild->isNull())
         usingLowMemHeap = true;

      if (translatedNode->getOpCode().isSub() || usingLowMemHeap)
         usingCompressedPointers = true;

      if (usingCompressedPointers)
         {
         while (secondChild->getNumChildren() && secondChild->getOpCodeValue() != TR::a2l)
            secondChild = secondChild->getFirstChild();
         if (secondChild->getNumChildren())
            secondChild = secondChild->getFirstChild();
         }
      }

   int32_t sizeofMR = TR::Compiler->om.sizeofReferenceAddress();
   if (usingCompressedPointers)
      sizeofMR = TR::Compiler->om.sizeofReferenceField();

   TR::MemoryReference *tempMR = NULL;

   TR::Register *compressedReg;
   if (secondChild->getReferenceCount() > 1 && secondChild->getRegister() != NULL)
      {
      if (!secondChild->getRegister()->containsInternalPointer())
         sourceRegister = cg->allocateCollectedReferenceRegister();
      else
         {
         sourceRegister = cg->allocateRegister();
         sourceRegister->setPinningArrayPointer(secondChild->getRegister()->getPinningArrayPointer());
         sourceRegister->setContainsInternalPointer();
         }
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, sourceRegister, secondChild->getRegister());
      killSource = true;
      compressedReg = sourceRegister;
      if (usingCompressedPointers)
         compressedReg = cg->evaluate(node->getSecondChild());
      }
   else
      {
      sourceRegister = cg->evaluate(secondChild);
      compressedReg = sourceRegister;
      if (usingCompressedPointers)
         compressedReg = cg->evaluate(node->getSecondChild());
      }

   if (!node->skipWrtBar() && !node->hasUnresolvedSymbolReference()
         && (comp->getOptions()->getGcMode() == TR_WrtbarOldCheck || comp->getOptions()->getGcMode() == TR_WrtbarCardMarkAndOldCheck))
      {
      flagsReg = cg->allocateRegister();
      generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, flagsReg, new (cg->trHeapMemory()) TR::MemoryReference(destinationRegister, getOffsetOfJ9ObjectFlags(), 4, cg));
      }

   // RealTimeGC write barriers occur BEFORE the store
   if (TR::Options::getCmdLineOptions()->realTimeGC())
      {
      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, sizeofMR, cg);

      TR::Register *destinationAddressRegister = cg->allocateRegister();
      VMwrtbarEvaluator(node, sourceRegister, destinationRegister, destinationAddressRegister, tempMR, NULL, secondChild->isNonNull(), true, usingCompressedPointers, cg);

      TR::MemoryReference *dstMR = new (cg->trHeapMemory()) TR::MemoryReference(destinationAddressRegister, 0, TR::Compiler->om.sizeofReferenceAddress(), cg);

      if (needSync)
         generateInstruction(cg, TR::InstOpCode::lwsync, node);

      if (usingCompressedPointers)
         generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, dstMR, compressedReg);
      else
         generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, dstMR, sourceRegister);

      if (needSync)
         {
         // ordered and lazySet operations will not generate a post-write sync
         // and will not mark unresolved snippets with in sync sequence to avoid
         // PicBuilder overwriting the instruction that normally would have been sync
         if (usingCompressedPointers)
            postSyncConditions(node, cg, compressedReg, dstMR, TR::InstOpCode::sync, lazyVolatile);
         else
            postSyncConditions(node, cg, sourceRegister, dstMR, TR::InstOpCode::sync, lazyVolatile);
         }

      cg->stopUsingRegister(destinationAddressRegister);
      }
   else
      {
      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, sizeofMR, cg);

      if (needSync)
         generateInstruction(cg, TR::InstOpCode::lwsync, node);

      if (usingCompressedPointers)
         generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempMR, compressedReg);
      else
         generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, tempMR, sourceRegister);

      if (needSync)
         {
         // ordered and lazySet operations will not generate a post-write sync
         // and will not mark unresolved snippets with in sync sequence to avoid
         // PicBuilder overwriting the instruction that normally would have been sync
         if (usingCompressedPointers)
            postSyncConditions(node, cg, compressedReg, tempMR, TR::InstOpCode::sync, lazyVolatile);
         else
            postSyncConditions(node, cg, sourceRegister, tempMR, TR::InstOpCode::sync, lazyVolatile);
         }

      if ((TR::Compiler->target.is32Bit() || (TR::Compiler->target.is64Bit() && comp->useCompressedPointers() && (TR::Compiler->om.compressedReferenceShiftOffset() == 0))) && comp->getMethodHotness() >= scorching
            && TR::Compiler->target.cpu.id() >= TR_PPCp6)
         {
         addPrefetch(cg, node, sourceRegister);
         }

      VMwrtbarEvaluator(node, sourceRegister, destinationRegister, NULL, NULL, NULL, secondChild->isNonNull(), true, usingCompressedPointers, cg, flagsReg);
      }

   if (killSource)
      cg->stopUsingRegister(sourceRegister);

   cg->decReferenceCount(node->getSecondChild());
   cg->decReferenceCount(node->getChild(2));
   tempMR->decNodeReferenceCounts(cg);

   if (comp->useCompressedPointers())
      node->setStoreAlreadyEvaluated(true);

   return NULL;
   }

TR::Register *J9::Power::TreeEvaluator::monentEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::VMmonentEvaluator(node, cg);
   }

TR::Register *J9::Power::TreeEvaluator::monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::VMmonexitEvaluator(node, cg);
   }

TR::Register *J9::Power::TreeEvaluator::monexitfenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *J9::Power::TreeEvaluator::asynccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The child contains an inline test. If it succeeds, the helper is called
   // or (with TR_ASYNC_CHECK_TRAPS) trap handler is invoked.
   // The address of the helper is contained as an int in this node.
   //
   TR::Node *testNode = node->getFirstChild();
   TR::Node *firstChild = testNode->getFirstChild();
   TR::Register *src1Reg = cg->evaluate(firstChild);
   TR::Node *secondChild = testNode->getSecondChild();

   TR_ASSERT(testNode->getOpCodeValue() == (TR::Compiler->target.is64Bit() ? TR::lcmpeq : TR::icmpeq), "asynccheck bad format");
   TR_ASSERT(secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL, "asynccheck bad format");

   TR::Instruction *gcPoint;

#if defined(TR_ASYNC_CHECK_TRAPS)
   if (cg->getHasResumableTrapHandler())
      {
      if (TR::Compiler->target.is64Bit())
         {
         TR_ASSERT(secondChild->getLongInt() == -1L, "asynccheck bad format");
         gcPoint = generateSrc1Instruction(cg, TR::InstOpCode::tdeqi, node, src1Reg, secondChild->getLongInt());
         }
      else
         {
         TR_ASSERT(secondChild->getInt() == -1, "asynccheck bad format");
         gcPoint = generateSrc1Instruction(cg, TR::InstOpCode::tweqi, node, src1Reg, secondChild->getInt());
         }
      cg->setCanExceptByTrap();
      }
   else
#endif
      {
      TR::Register *condReg = cg->allocateRegister(TR_CCR);
      if (TR::Compiler->target.is64Bit())
         {
         TR_ASSERT(secondChild->getLongInt() == -1L, "asynccheck bad format");
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi8, node, condReg, src1Reg, secondChild->getLongInt());
         }
      else
         {
         TR_ASSERT(secondChild->getInt() == -1, "asynccheck bad format");
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, src1Reg, secondChild->getInt());
         }

      TR::LabelSymbol *snippetLabel;
      TR::Register *jumpRegister = cg->allocateRegister();
      TR::RegisterDependencyConditions *dependencies = createConditionsAndPopulateVSXDeps(cg, 2);

      snippetLabel = cg->lookUpSnippet(TR::Snippet::IsHelperCall, node->getSymbolReference());
      if (snippetLabel == NULL)
         {
         snippetLabel = generateLabelSymbol(cg);
         cg->addSnippet(new (cg->trHeapMemory()) TR::PPCHelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference()));
         }
      addDependency(dependencies, jumpRegister, TR::RealRegister::gr11, TR_GPR, cg);
      addDependency(dependencies, condReg, TR::RealRegister::cr7, TR_CCR, cg);
      if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
         // use PPC AS branch hint
         gcPoint = generateDepConditionalBranchInstruction(cg, TR::InstOpCode::beql, PPCOpProp_BranchUnlikely, node, snippetLabel, condReg, dependencies);
      else
         gcPoint = generateDepConditionalBranchInstruction(cg, TR::InstOpCode::beql, node, snippetLabel, condReg, dependencies);
      gcPoint->setAsyncBranch();
      cg->machine()->setLinkRegisterKilled(true);

      dependencies->stopUsingDepRegs(cg);
      }

   gcPoint->PPCNeedsGCMap(0xFFFFFFFF);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->decReferenceCount(testNode);
   return NULL;
   }

TR::Register *J9::Power::TreeEvaluator::instanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   if (comp->getOption(TR_OptimizeForSpace) || comp->getOption(TR_DisableInlineInstanceOf))
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::icall);
      TR::Register *targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }
   else
      return VMinstanceOfEvaluator(node, cg);
   }

TR::Register *J9::Power::TreeEvaluator::checkcastEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   if (comp->getOption(TR_OptimizeForSpace) || comp->getOption(TR_DisableInlineCheckCast))
      {
      bool isCheckcastAndNullChk = (node->getOpCodeValue() == TR::checkcastAndNULLCHK);
      TR::Node *objNode = node->getFirstChild();
      bool needsNullTest = !objNode->isNonNull();

      if (needsNullTest && isCheckcastAndNullChk)
         {
         TR::Instruction *gcPoint;
         // get the bytecode info of the
         // NULLCHK that was compacted
         TR::Node *nullChkInfo = comp->findNullChkInfo(node);
         TR::Register *objReg = cg->evaluate(objNode);

         if (cg->getHasResumableTrapHandler())
            gcPoint = generateNullTestInstructions(cg, objReg, nullChkInfo);
         else
            // a helper is needed
            gcPoint = generateNullTestInstructions(cg, objReg, nullChkInfo, true);
         gcPoint->PPCNeedsGCMap(0xFFFFFFFF);
         }

      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::call);
      TR::Register *targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);

      return targetRegister;
      }
   else
      return TR::TreeEvaluator::VMcheckcastEvaluator(node, cg);
   }

TR::Register *J9::Power::TreeEvaluator::checkcastAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return checkcastEvaluator(node, cg);
   }

TR::Register *J9::Power::TreeEvaluator::newObjectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->suppressAllocationInlining())
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::acall);
      TR::Register *targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }
   else
      return TR::TreeEvaluator::VMnewEvaluator(node, cg);
   }

TR::Register *J9::Power::TreeEvaluator::newArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->suppressAllocationInlining())
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::acall);
      TR::Register *targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }
   else
      return TR::TreeEvaluator::VMnewEvaluator(node, cg);
   }

TR::Register *J9::Power::TreeEvaluator::anewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->suppressAllocationInlining())
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::acall);
      TR::Register *targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }
   else
      return TR::TreeEvaluator::VMnewEvaluator(node, cg);
   }

TR::Register *J9::Power::TreeEvaluator::multianewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::acall);
   TR::Register *targetRegister = directCallEvaluator(node, cg);
   TR::Node::recreate(node, opCode);
   return targetRegister;
   }

TR::Register *J9::Power::TreeEvaluator::arraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(cg->comp()->requiresSpineChecks(), "TR::arraylength should be lowered when hybrid arraylets are not in use");
   TR_ASSERT(node->getOpCodeValue() == TR::arraylength, "arraylengthEvaluator expecting TR::arraylength");

   TR::Register *objectReg = cg->evaluate(node->getFirstChild());
   TR::Register *lengthReg = cg->allocateRegister();
   TR::Register *condReg = cg->allocateRegister(TR_CCR);
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());

   TR::MemoryReference *contiguousArraySizeMR = new (cg->trHeapMemory()) TR::MemoryReference(objectReg, fej9->getOffsetOfContiguousArraySizeField(), 4, cg);
   TR::MemoryReference *discontiguousArraySizeMR = new (cg->trHeapMemory()) TR::MemoryReference(objectReg, fej9->getOffsetOfDiscontiguousArraySizeField(), 4, cg);

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg->trMemory());
   deps->addPostCondition(objectReg, TR::RealRegister::NoReg);
   deps->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0();
   deps->addPostCondition(lengthReg, TR::RealRegister::NoReg);
   deps->addPostCondition(condReg, TR::RealRegister::NoReg);

   // lwz R, [B + contiguousSize]       ; Load contiguous array length
   // cmp R, 0                          ; If 0, must be a discontiguous array
   // beq out-of-line
   // done:
   //
   // out-of-line:
   // lwz R, [B + discontiguousSize]    ; Load discontiguous array length
   // b done

   TR::LabelSymbol *discontiguousArrayLabel = generateLabelSymbol(cg);
   TR_PPCOutOfLineCodeSection *discontiguousArrayOOL = new (cg->trHeapMemory()) TR_PPCOutOfLineCodeSection(discontiguousArrayLabel, doneLabel, cg);
   cg->getPPCOutOfLineCodeSectionList().push_front(discontiguousArrayOOL);

   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, lengthReg, contiguousArraySizeMR);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, node, condReg, lengthReg, 0);
   if (TR::Compiler->target.cpu.id() >= TR_PPCgp) // Use PPC AS branch hint.
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, PPCOpProp_BranchUnlikely, node, discontiguousArrayLabel, condReg);
   else
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, discontiguousArrayLabel, condReg);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);

   // OOL begin.
   //
   discontiguousArrayOOL->swapInstructionListsWithCompilation();
      {
      generateLabelInstruction(cg, TR::InstOpCode::label, node, discontiguousArrayLabel);
      generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, lengthReg, discontiguousArraySizeMR);
      generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);
      }
   discontiguousArrayOOL->swapInstructionListsWithCompilation();
   //
   // OOL end.

   TR::LabelSymbol *depLabel = generateLabelSymbol(cg);
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, depLabel, deps);

   cg->stopUsingRegister(condReg);
   cg->decReferenceCount(node->getFirstChild());
   node->setRegister(lengthReg);

   return lengthReg;
   }

TR::Register *J9::Power::TreeEvaluator::resolveCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // No code is generated for the resolve check. The child will reference an
   // unresolved symbol and all check handling is done via the corresponding
   // snippet.
   //
   TR::Node *firstChild = node->getFirstChild();
   bool fixRefCount = false;
   if (cg->comp()->useCompressedPointers())
      {
      // for stores under ResolveCHKs, artificially bump
      // down the reference count before evaluation (since stores
      // return null as registers)
      //
      if (node->getFirstChild()->getOpCode().isStoreIndirect() && node->getFirstChild()->getReferenceCount() > 1)
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

TR::Register *J9::Power::TreeEvaluator::DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *secondChild = node->getFirstChild()->getSecondChild();
   TR::DataType type = secondChild->getType();
   TR::Register *srcReg;
   TR::Instruction *gcPoint;
   bool constDivisor = secondChild->getOpCode().isLoadConst();
   bool killSrc = false;

   if (!constDivisor || (type.isInt32() && secondChild->getInt() == 0) || (type.isInt64() && secondChild->getLongInt() == 0))
      {
      if (!constDivisor || cg->getHasResumableTrapHandler())
         {
         srcReg = cg->evaluate(secondChild);
         if (type.isInt64() && TR::Compiler->target.is32Bit())
            {
            TR::Register *trgReg = cg->allocateRegister();
            generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, trgReg, srcReg->getHighOrder(), srcReg->getLowOrder());
            srcReg = trgReg;
            killSrc = true;
            }
         }

      if (cg->getHasResumableTrapHandler())
         {
         if (type.isInt64() && TR::Compiler->target.is64Bit())
            gcPoint = generateSrc1Instruction(cg, TR::InstOpCode::tdllti, node, srcReg, 1);
         else
            gcPoint = generateSrc1Instruction(cg, TR::InstOpCode::twllti, node, srcReg, 1);
         cg->setCanExceptByTrap();
         }
      else
         {
         TR::LabelSymbol *snippetLabel = cg->lookUpSnippet(TR::Snippet::IsHelperCall, node->getSymbolReference());
         TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
         TR::Register *jumpReg = cg->allocateRegister();

         if (snippetLabel == NULL)
            {
            snippetLabel = generateLabelSymbol(cg);
            cg->addSnippet(new (cg->trHeapMemory()) TR::PPCHelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference()));
            }

         // trampoline kills gr11
         addDependency(conditions, jumpReg, TR::RealRegister::gr11, TR_GPR, cg);
         if (constDivisor)
            {
            // Can be improved to: call the helper directly.
            gcPoint = generateDepLabelInstruction(cg, TR::InstOpCode::bl, node, snippetLabel, conditions);
            }
         else
            {
            TR::Register *condReg = cg->allocateRegister(TR_CCR);
            if (type.isInt64() && TR::Compiler->target.is64Bit())
               generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli8, node, condReg, srcReg, 0);
            else
               generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, node, condReg, srcReg, 0);
            if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
               // use PPC AS branch hint
               gcPoint = generateDepConditionalBranchInstruction(cg, TR::InstOpCode::beql, PPCOpProp_BranchUnlikely, node, snippetLabel, condReg, conditions);
            else
               gcPoint = generateDepConditionalBranchInstruction(cg, TR::InstOpCode::beql, node, snippetLabel, condReg, conditions);
            cg->stopUsingRegister(condReg);
            }
         cg->stopUsingRegister(jumpReg);
         }

      if (killSrc)
         cg->stopUsingRegister(srcReg);

      gcPoint->PPCNeedsGCMap(0xFFFFFFFF);
      }

   cg->evaluate(node->getFirstChild());
   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }

static void genBoundCheck(TR::CodeGenerator *cg, TR::Node *node, TR::Register *indexReg, int32_t indexVal, TR::Register *arrayLengthReg, int32_t arrayLengthVal,
      TR::Register *condReg, bool noTrap)
   {
   TR::Instruction *gcPoint;
   if (noTrap)
      {
      TR::LabelSymbol *boundCheckFailSnippetLabel = cg->lookUpSnippet(TR::Snippet::IsHelperCall, node->getSymbolReference());
      if (!boundCheckFailSnippetLabel)
         {
         boundCheckFailSnippetLabel = generateLabelSymbol(cg);
         cg->addSnippet(new (cg->trHeapMemory()) TR::PPCHelperCallSnippet(cg, node, boundCheckFailSnippetLabel, node->getSymbolReference()));
         }

      if (indexReg)
         generateTrg1Src2Instruction(cg, TR::InstOpCode::cmpl4, node, condReg, arrayLengthReg, indexReg);
      else
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, node, condReg, arrayLengthReg, indexVal);

      // NOTE: Trampoline kills gr11
      gcPoint =
            TR::Compiler->target.cpu.id() >= TR_PPCgp ? // Use PPC AS branch hint.
            generateConditionalBranchInstruction(cg, TR::InstOpCode::blel, PPCOpProp_BranchUnlikely, node, boundCheckFailSnippetLabel, condReg) :
                  generateConditionalBranchInstruction(cg, TR::InstOpCode::blel, node, boundCheckFailSnippetLabel, condReg);
      }
   else
      {
      if (indexReg)
         {
         if (arrayLengthReg)
            gcPoint = generateSrc2Instruction(cg, TR::InstOpCode::twlle, node, arrayLengthReg, indexReg);
         else
            gcPoint = generateSrc1Instruction(cg, TR::InstOpCode::twlgei, node, indexReg, arrayLengthVal);
         }
      else
         {
         if (arrayLengthReg)
            gcPoint = generateSrc1Instruction(cg, TR::InstOpCode::twllei, node, arrayLengthReg, indexVal);
         else if (arrayLengthVal <= indexVal)
            gcPoint = generateInstruction(cg, TR::InstOpCode::trap, node);
         }

      cg->setCanExceptByTrap();
      }

   // Exception edges don't have any live regs
   gcPoint->PPCNeedsGCMap(0);
   }

TR::Register *J9::Power::TreeEvaluator::BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   static bool noReversedTrap = (feGetEnv("TR_noReversedTrap") != NULL);
   TR::Node *secondChild = node->getSecondChild();
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *src1Reg;
   TR::Register *src2Reg = NULL;
   TR::Register *cndReg, *tmpReg = NULL;
   int32_t value;
   TR::RegisterDependencyConditions *conditions;
   TR::LabelSymbol *snippetLabel;
   TR::Instruction *gcPoint;
   bool noTrap = !cg->getHasResumableTrapHandler();
   bool reversed = false;

   if (noTrap)
      {
      cndReg = cg->allocateRegister(TR_CCR);
      conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(4, 4, cg->trMemory());
      }

   if (firstChild->getOpCode().isLoadConst() && firstChild->getInt() <= UPPER_IMMED && firstChild->getRegister() == NULL && !noReversedTrap)
      {
      src2Reg = cg->evaluate(secondChild);
      reversed = true;
      }
   else
      {
      src1Reg = cg->evaluate(firstChild);

      if (secondChild->getOpCode().isLoadConst() && secondChild->getRegister() == NULL)
         {
         value = secondChild->getInt();
         if (value < LOWER_IMMED || value > UPPER_IMMED)
            {
            src2Reg = cg->evaluate(secondChild);
            }
         }
      else
         src2Reg = cg->evaluate(secondChild);
      }

   if (!noTrap)
      {
      if (reversed)
         {
         gcPoint = generateSrc1Instruction(cg, TR::InstOpCode::twlgei, node, src2Reg, firstChild->getInt());
         }
      else
         {
         if (src2Reg == NULL)
            gcPoint = generateSrc1Instruction(cg, TR::InstOpCode::twllei, node, src1Reg, value);
         else
            gcPoint = generateSrc2Instruction(cg, TR::InstOpCode::twlle, node, src1Reg, src2Reg);
         }
      }
   else
      {
      if (reversed)
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, node, cndReg, src2Reg, firstChild->getInt());
         }
      else
         {
         if (src2Reg == NULL)
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, node, cndReg, src1Reg, value);
         else
            generateTrg1Src2Instruction(cg, TR::InstOpCode::cmpl4, node, cndReg, src1Reg, src2Reg);
         }

      snippetLabel = cg->lookUpSnippet(TR::Snippet::IsHelperCall, node->getSymbolReference());
      if (snippetLabel == NULL)
         {
         snippetLabel = generateLabelSymbol(cg);
         cg->addSnippet(new (cg->trHeapMemory()) TR::PPCHelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference()));
         }

      tmpReg = cg->allocateRegister();
      // trampoline kills gr11
      addDependency(conditions, tmpReg, TR::RealRegister::gr11, TR_GPR, cg);
      if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
         // use PPC AS branch hint
         gcPoint = generateDepConditionalBranchInstruction(cg, reversed ? TR::InstOpCode::bgel : TR::InstOpCode::blel, PPCOpProp_BranchUnlikely, node, snippetLabel, cndReg, conditions);
      else
         gcPoint = generateDepConditionalBranchInstruction(cg, reversed ? TR::InstOpCode::bgel : TR::InstOpCode::blel, node, snippetLabel, cndReg, conditions);
      cg->stopUsingRegister(tmpReg);
      }

   if (noTrap)
      cg->stopUsingRegister(cndReg);

   gcPoint->PPCNeedsGCMap(0xFFFFFFFF);
   if (!noTrap)
      cg->setCanExceptByTrap();
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   secondChild->setIsNonNegative(true);
   return (NULL);
   }

TR::Register *J9::Power::TreeEvaluator::ArrayCopyBNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::LabelSymbol *snippetLabel = NULL;
   TR::Instruction *gcPoint;
   bool noTrap = !cg->getHasResumableTrapHandler();
   bool directCase = firstChild->getOpCode().isLoadConst() && secondChild->getOpCode().isLoadConst() && firstChild->getInt() < secondChild->getInt();

   if (directCase || noTrap)
      {
      snippetLabel = cg->lookUpSnippet(TR::Snippet::IsHelperCall, node->getSymbolReference());
      if (snippetLabel == NULL)
         {
         snippetLabel = generateLabelSymbol(cg);
         cg->addSnippet(new (cg->trHeapMemory()) TR::PPCHelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference()));
         }
      }

   // We are checking for firstChild>=secondChild. We don't need to copy registers.

   if (directCase)
      {
      gcPoint = generateLabelInstruction(cg, TR::InstOpCode::bl, node, snippetLabel);
      }
   else if (firstChild->getOpCode().isLoadConst() && firstChild->getInt() <= UPPER_IMMED && firstChild->getRegister() == NULL)
      {
      TR::Register *copyIndexReg = cg->evaluate(secondChild);
      if (noTrap)
         {
         TR::Register *cndReg = cg->allocateRegister(TR_CCR);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, cndReg, copyIndexReg, firstChild->getInt());
         if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
            // use PPC AS branch hint
            gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgtl, PPCOpProp_BranchUnlikely, node, snippetLabel, cndReg);
         else
            gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgtl, node, snippetLabel, cndReg);
         cg->stopUsingRegister(cndReg);
         }
      else
         {
         gcPoint = generateSrc1Instruction(cg, TR::InstOpCode::twgti, node, copyIndexReg, firstChild->getInt());
         cg->setCanExceptByTrap();
         }
      }
   else
      {
      TR::Register *boundReg = cg->evaluate(firstChild);
      if (secondChild->getOpCode().isLoadConst() && secondChild->getInt() <= UPPER_IMMED)
         {
         if (noTrap)
            {
            TR::Register *cndReg = cg->allocateRegister(TR_CCR);
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, cndReg, boundReg, secondChild->getInt());
            if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
               // use PPC AS branch hint
               gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::bltl, PPCOpProp_BranchUnlikely, node, snippetLabel, cndReg);
            else
               gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::bltl, node, snippetLabel, cndReg);
            cg->stopUsingRegister(cndReg);
            }
         else
            {
            gcPoint = generateSrc1Instruction(cg, TR::InstOpCode::twlti, node, boundReg, secondChild->getInt());
            cg->setCanExceptByTrap();
            }
         }
      else
         {
         TR::Register *copyIndexReg = cg->evaluate(secondChild);
         if (noTrap)
            {
            TR::Register *cndReg = cg->allocateRegister(TR_CCR);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp4, node, cndReg, boundReg, copyIndexReg);
            if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
               // use PPC AS branch hint
               gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::bltl, PPCOpProp_BranchUnlikely, node, snippetLabel, cndReg);
            else
               gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::bltl, node, snippetLabel, cndReg);
            cg->stopUsingRegister(cndReg);
            }
         else
            {
            gcPoint = generateSrc2Instruction(cg, TR::InstOpCode::twlt, node, boundReg, copyIndexReg);
            cg->setCanExceptByTrap();
            }
         }
      }

   gcPoint->PPCNeedsGCMap(0xFFFFFFFF);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   if (secondChild->getOpCode().isLoadConst() && secondChild->getInt() >= 0)
      firstChild->setIsNonNegative(true);
   return (NULL);
   }

static TR::Instruction* genSpineCheck(TR::CodeGenerator *cg, TR::Node *node, TR::Register *arrayLengthReg, TR::Register *condReg, TR::LabelSymbol *discontiguousArrayLabel)
   {
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, node, condReg, arrayLengthReg, 0);
   return generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, discontiguousArrayLabel, condReg);
   }

static TR::Instruction* genSpineCheck(TR::CodeGenerator *cg, TR::Node *node, TR::Register *baseArrayReg, TR::Register *arrayLengthReg, TR::Register *condReg,
      TR::LabelSymbol *discontiguousArrayLabel)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   TR::MemoryReference *contiguousArraySizeMR = new (cg->trHeapMemory()) TR::MemoryReference(baseArrayReg, fej9->getOffsetOfContiguousArraySizeField(), 4, cg);
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, arrayLengthReg, contiguousArraySizeMR);
   return genSpineCheck(cg, node, arrayLengthReg, condReg, discontiguousArrayLabel);
   }

static void genArrayletAccessAddr(TR::CodeGenerator *cg, TR::Node *node, int32_t elementSize,
// Inputs:
      TR::Register *baseArrayReg, TR::Register *indexReg, int32_t indexVal,
      // Outputs:
      TR::Register *arrayletReg, TR::Register *offsetReg, int32_t& offsetVal)
   {
   TR::Compilation* comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   TR_ASSERT(offsetReg || !indexReg, "Expecting valid offset reg when index reg is passed");

   uintptrj_t arrayHeaderSize = TR::Compiler->om.discontiguousArrayHeaderSizeInBytes();
   int32_t spinePointerSize = TR::Compiler->om.sizeofReferenceField();
   int32_t spinePointerSizeShift = spinePointerSize == 8 ? 3 : 2;

   TR::MemoryReference *spineMR;

   // Calculate the spine offset.
   //
   if (indexReg)
      {
      int32_t spineShift = fej9->getArraySpineShift(elementSize);

      // spineOffset = (index >> spineShift) * spinePtrSize
      //             = (index >> spineShift) << spinePtrSizeShift
      //             = (index >> (spineShift - spinePtrSizeShift)) & ~(spinePtrSize - 1)
      // spineOffset += arrayHeaderSize
      //
      TR_ASSERT(spineShift >= spinePointerSizeShift, "Unexpected spine shift value");
      generateShiftRightLogicalImmediate(cg, node, arrayletReg, indexReg, spineShift - spinePointerSizeShift);
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, arrayletReg, arrayletReg, 0, ~(spinePointerSize - 1));
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, arrayletReg, arrayletReg, arrayHeaderSize);

      spineMR = new (cg->trHeapMemory()) TR::MemoryReference(baseArrayReg, arrayletReg, spinePointerSize, cg);
      }
   else
      {
      int32_t spineIndex = fej9->getArrayletLeafIndex(indexVal, elementSize);
      int32_t spineDisp32 = spineIndex * spinePointerSize + arrayHeaderSize;

      spineMR = new (cg->trHeapMemory()) TR::MemoryReference(baseArrayReg, spineDisp32, spinePointerSize, cg);
      }

   // Load the arraylet from the spine.
   //
   generateTrg1MemInstruction(cg, spinePointerSize == 8 ? TR::InstOpCode::ld : TR::InstOpCode::lwz, node, arrayletReg, spineMR);

   // Calculate the arraylet offset.
   //
   if (indexReg)
      {
      int32_t arrayletMask = fej9->getArrayletMask(elementSize);
      int32_t elementSizeShift = CHAR_BIT * sizeof(int32_t) - leadingZeroes(elementSize - 1);

      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, offsetReg, indexReg, elementSizeShift, arrayletMask << elementSizeShift);
      }
   else
      offsetVal = (fej9->getLeafElementIndex(indexVal, elementSize) * elementSize);
   }

static TR::InstOpCode::Mnemonic getLoadOrStoreFromDataType(TR::CodeGenerator *cg, TR::DataType dt, int32_t elementSize, bool isUnsigned, bool returnLoad)
   {
   switch (dt)
      {
   case TR::Int8:
      return returnLoad ? TR::InstOpCode::lbz : TR::InstOpCode::stb;
   case TR::Int16:
      if (returnLoad)
         return isUnsigned ? TR::InstOpCode::lhz : TR::InstOpCode::lha;
      else
         return TR::InstOpCode::sth;
   case TR::Int32:
      if (returnLoad)
         return TR::InstOpCode::lwz;
      else
         return TR::InstOpCode::stw;
   case TR::Int64:
      return returnLoad ? TR::InstOpCode::ld : TR::InstOpCode::std;
   case TR::Float:
      return returnLoad ? TR::InstOpCode::lfs : TR::InstOpCode::stfs;
   case TR::Double:
      return returnLoad ? TR::InstOpCode::lfd : TR::InstOpCode::stfd;
   case TR::Address:
      if (returnLoad)
         return elementSize == 8 ? TR::InstOpCode::ld : TR::InstOpCode::lwz;
      else
         return elementSize == 8 ? TR::InstOpCode::std : TR::InstOpCode::stw;
   default:
      TR_ASSERT(false, "Unexpected array data type");
      return TR::InstOpCode::bad;
      }
   }

static void genDecompressPointer(TR::CodeGenerator *cg, TR::Node *node, TR::Register *ptrReg, TR::Register *condReg = NULL, bool nullCheck = true)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->comp()->fe());
   uintptrj_t heapBase = TR::Compiler->vm.heapBaseAddress();
   int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();

   if (heapBase != 0)
      {
      TR::LabelSymbol *skipLabel = NULL;
      if (nullCheck)
         {
         TR_ASSERT(condReg, "Need a condition reg for non-zero heap base decompression");
         skipLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli8, node, condReg, ptrReg, NULLVALUE);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, skipLabel, condReg);
         }
      if (shiftAmount != 0)
         generateShiftLeftImmediateLong(cg, node, ptrReg, ptrReg, shiftAmount);
      addConstantToLong(node, ptrReg, heapBase, ptrReg, cg);
      if (nullCheck)
         generateLabelInstruction(cg, TR::InstOpCode::label, node, skipLabel);
      }
   else if (shiftAmount != 0)
      generateShiftLeftImmediateLong(cg, node, ptrReg, ptrReg, shiftAmount);
   }

static TR::Register *addConstantToLongWithTempReg(TR::Node * node, TR::Register *srcReg, int64_t value, TR::Register *trgReg, TR::Register *tempReg, TR::CodeGenerator *cg)
   {
   if (!trgReg)
      trgReg = cg->allocateRegister();

   if ((int16_t) value == value)
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, trgReg, srcReg, value);
      }
   // NOTE: the following only works if the second add's immediate is not sign extended
   else if (((int32_t) value == value) && ((value & 0x8000) == 0))
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, trgReg, srcReg, value >> 16);
      if (value & 0xffff)
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, trgReg, trgReg, value);
      }
   else
      {
      loadConstant(cg, node, value, tempReg);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, trgReg, srcReg, tempReg);
      }

   return trgReg;
   }

static void genDecompressPointerWithTempReg(TR::CodeGenerator *cg, TR::Node *node, TR::Register *ptrReg, TR::Register *tempReg, TR::Register *condReg = NULL, bool nullCheck = true)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->comp()->fe());
   uintptrj_t heapBase = TR::Compiler->vm.heapBaseAddress();
   int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();

   if (heapBase != 0)
      {
      TR::LabelSymbol *skipLabel = NULL;
      if (nullCheck)
         {
         TR_ASSERT(condReg, "Need a condition reg for non-zero heap base decompression");
         skipLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli8, node, condReg, ptrReg, NULLVALUE);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, skipLabel, condReg);
         }
      if (shiftAmount != 0)
         generateShiftLeftImmediateLong(cg, node, ptrReg, ptrReg, shiftAmount);
      addConstantToLongWithTempReg(node, ptrReg, heapBase, ptrReg, tempReg, cg);
      if (nullCheck)
         generateLabelInstruction(cg, TR::InstOpCode::label, node, skipLabel);
      }
   else if (shiftAmount != 0)
      generateShiftLeftImmediateLong(cg, node, ptrReg, ptrReg, shiftAmount);
   }

static TR::Register *genDecompressPointerNonNull2Regs(TR::CodeGenerator *cg, TR::Node *node, TR::Register *ptrReg, TR::Register *targetReg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->comp()->fe());
   uintptrj_t heapBase = TR::Compiler->vm.heapBaseAddress();
   int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();

   if (heapBase != 0)
      {
      TR::Register *temp = ptrReg;
      if (shiftAmount != 0)
         {
         generateShiftLeftImmediateLong(cg, node, targetReg, ptrReg, shiftAmount);
         temp = targetReg;
         }
      addConstantToLong(node, temp, heapBase, targetReg, cg);
      return targetReg;
      }
   else if (shiftAmount != 0)
      {
      generateShiftLeftImmediateLong(cg, node, targetReg, ptrReg, shiftAmount);
      return targetReg;
      }
   else
      {
      return ptrReg;
      }
   }

static TR::Register *genDecompressPointerNonNull2RegsWithTempReg(TR::CodeGenerator *cg, TR::Node *node, TR::Register *ptrReg, TR::Register *targetReg, TR::Register *tempReg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->comp()->fe());
   uintptrj_t heapBase = TR::Compiler->vm.heapBaseAddress();
   int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();

   if (heapBase != 0)
      {
      TR::Register *temp = ptrReg;
      if (shiftAmount != 0)
         {
         generateShiftLeftImmediateLong(cg, node, targetReg, ptrReg, shiftAmount);
         temp = targetReg;
         }
      addConstantToLongWithTempReg(node, temp, heapBase, targetReg, tempReg, cg);
      return targetReg;
      }
   else if (shiftAmount != 0)
      {
      generateShiftLeftImmediateLong(cg, node, targetReg, ptrReg, shiftAmount);
      return targetReg;
      }
   else
      {
      return ptrReg;
      }
   }

static void genCompressPointerWithTempReg(TR::CodeGenerator *cg, TR::Node *node, TR::Register *ptrReg, TR::Register *tempReg, TR::Register *condReg = NULL, bool nullCheck = true)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->comp()->fe());
   uintptrj_t heapBase = TR::Compiler->vm.heapBaseAddress();
   int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();

   if (heapBase != 0)
      {
      TR::LabelSymbol *skipLabel = NULL;
      if (nullCheck)
         {
         TR_ASSERT(condReg, "Need a condition reg for non-zero heap base compression");
         skipLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli8, node, condReg, ptrReg, NULLVALUE);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, skipLabel, condReg);
         }
      addConstantToLongWithTempReg(node, ptrReg, (int64_t)(-heapBase), ptrReg, tempReg, cg);
      if (shiftAmount != 0)
         generateShiftRightLogicalImmediateLong(cg, node, ptrReg, ptrReg, shiftAmount);
      if (nullCheck)
         generateLabelInstruction(cg, TR::InstOpCode::label, node, skipLabel);
      }
   else if (shiftAmount != 0)
      {
      generateShiftRightLogicalImmediateLong(cg, node, ptrReg, ptrReg, shiftAmount);
      }
   }

static void genCompressPointer(TR::CodeGenerator *cg, TR::Node *node, TR::Register *ptrReg, TR::Register *condReg = NULL, bool nullCheck = true)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->comp()->fe());
   uintptrj_t heapBase = TR::Compiler->vm.heapBaseAddress();
   int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();

   if (heapBase != 0)
      {
      TR::LabelSymbol *skipLabel = NULL;
      if (nullCheck)
         {
         TR_ASSERT(condReg, "Need a condition reg for non-zero heap base compression");
         skipLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli8, node, condReg, ptrReg, NULLVALUE);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, skipLabel, condReg);
         }
      addConstantToLong(node, ptrReg, -heapBase, ptrReg, cg);
      if (shiftAmount != 0)
         generateShiftRightLogicalImmediateLong(cg, node, ptrReg, ptrReg, shiftAmount);
      if (nullCheck)
         generateLabelInstruction(cg, TR::InstOpCode::label, node, skipLabel);
      }
   else if (shiftAmount != 0)
      {
      generateShiftRightLogicalImmediateLong(cg, node, ptrReg, ptrReg, shiftAmount);
      }
   }

static TR::Register *genCompressPointerNonNull2RegsWithTempReg(TR::CodeGenerator *cg, TR::Node *node, TR::Register *ptrReg, TR::Register *targetReg, TR::Register *tempReg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->comp()->fe());
   uintptrj_t heapBase = TR::Compiler->vm.heapBaseAddress();
   int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();

   if (heapBase != 0)
      {
      addConstantToLongWithTempReg(node, targetReg, (int64_t)(-heapBase), ptrReg, tempReg, cg);
      if (shiftAmount != 0)
         generateShiftRightLogicalImmediateLong(cg, node, targetReg, targetReg, shiftAmount);
      return targetReg;
      }
   else if (shiftAmount != 0)
      {
      generateShiftRightLogicalImmediateLong(cg, node, targetReg, ptrReg, shiftAmount);
      return targetReg;
      }
   else
      return ptrReg;
   }

static TR::Register *genCompressPointerNonNull2Regs(TR::CodeGenerator *cg, TR::Node *node, TR::Register *ptrReg, TR::Register *targetReg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->comp()->fe());
   uintptrj_t heapBase = TR::Compiler->vm.heapBaseAddress();
   int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();

   if (heapBase != 0)
      {
      addConstantToLong(node, targetReg, (int64_t)(-heapBase), ptrReg, cg);
      if (shiftAmount != 0)
         generateShiftRightLogicalImmediateLong(cg, node, targetReg, targetReg, shiftAmount);
      return targetReg;
      }
   else if (shiftAmount != 0)
      {
      generateShiftRightLogicalImmediateLong(cg, node, targetReg, ptrReg, shiftAmount);
      return targetReg;
      }
   else
      return ptrReg;
   }

// Handles both BNDCHKwithSpineCHK and SpineCHK nodes.
//
TR::Register *J9::Power::TreeEvaluator::BNDCHKwithSpineCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   bool noTrap = !cg->getHasResumableTrapHandler();
   bool needsBoundCheck = node->getOpCodeValue() == TR::BNDCHKwithSpineCHK;
   bool needsBoundCheckOOL;

   TR_PPCScratchRegisterManager *srm = cg->generateScratchRegisterManager();

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
      indexChild = node->getChild(2);

   TR::Register *baseArrayReg = cg->evaluate(baseArrayChild);
   TR::Register *indexReg;
   TR::Register *loadOrStoreReg;
   TR::Register *arrayLengthReg;

   // If the index is too large to be an immediate load it in a register
   if (!indexChild->getOpCode().isLoadConst() || (indexChild->getInt() > UPPER_IMMED || indexChild->getInt() < LOWER_IMMED))
      indexReg = cg->evaluate(indexChild);
   else
      indexReg = NULL;

   // For primitive stores anchored under the check node, we must evaluate the source node
   // before the bound check branch so that its available to the snippet.
   //
   if (loadOrStoreChild->getOpCode().isStore() && !loadOrStoreChild->getRegister())
      {
      TR::Node *valueChild = loadOrStoreChild->getSecondChild();
      cg->evaluate(valueChild);
      }

   // Evaluate any escaping nodes before the OOL branch since they won't be evaluated in the OOL path.
   preEvaluateEscapingNodesForSpineCheck(node, cg);

   // Label to the OOL code that will perform the load/store/agen for discontigous arrays (and the bound check if needed).
   TR::LabelSymbol *discontiguousArrayLabel = generateLabelSymbol(cg);

   // Label back to main-line that the OOL code will branch to when done.
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   doneLabel->setEndInternalControlFlow();

   TR_PPCOutOfLineCodeSection *discontiguousArrayOOL = new (cg->trHeapMemory()) TR_PPCOutOfLineCodeSection(discontiguousArrayLabel, doneLabel, cg);
   cg->getPPCOutOfLineCodeSectionList().push_front(discontiguousArrayOOL);

   TR::Instruction *OOLBranchInstr;

   if (needsBoundCheck)
      {
      TR_ASSERT(arrayLengthChild, "Expecting to have an array length child for BNDCHKwithSpineCHK node");
      TR_ASSERT(
            arrayLengthChild->getOpCode().isConversion() || arrayLengthChild->getOpCodeValue() == TR::iloadi || arrayLengthChild->getOpCodeValue() == TR::iload
                  || arrayLengthChild->getOpCodeValue() == TR::iRegLoad || arrayLengthChild->getOpCode().isLoadConst(),
            "Expecting array length child under BNDCHKwithSpineCHK to be a conversion, iiload, iload, iRegLoad or iconst");

      TR::Register *condReg = srm->findOrCreateScratchRegister(TR_CCR);

      arrayLengthReg = arrayLengthChild->getRegister();

      if (arrayLengthReg)
         {
         OOLBranchInstr = genSpineCheck(cg, node, baseArrayReg, arrayLengthReg, condReg, discontiguousArrayLabel);
         needsBoundCheckOOL = true;
         genBoundCheck(cg, node, indexReg, indexChild->getInt(), arrayLengthReg, arrayLengthChild->getInt(), condReg, noTrap);
         }
      else if (arrayLengthChild->getOpCode().isLoadConst())
         {
         // If the constant arraylength is non-zero then it will pass the spine check (hence its
         // a contiguous array) and the BNDCHK can be done inline with no OOL path.
         //
         // If the constant arraylenth is zero then we will always go OOL.
         //
         // TODO: in the future there shouldn't be an OOL path because any valid access must be
         //       on a discontiguous array.
         //
         if (arrayLengthChild->getInt() != 0)
            {
            // The array must be contiguous.
            //

            // If the array length is too large to be an immediate load it in a register for the bound check
            if (arrayLengthChild->getInt() > UPPER_IMMED || arrayLengthChild->getInt() < LOWER_IMMED)
               arrayLengthReg = cg->evaluate(arrayLengthChild);

            // Do the bound check first.
            genBoundCheck(cg, node, indexReg, indexChild->getInt(), arrayLengthReg, arrayLengthChild->getInt(), condReg, noTrap);
            needsBoundCheckOOL = false;
            TR::Register *scratchArrayLengthReg = srm->findOrCreateScratchRegister();
            OOLBranchInstr = genSpineCheck(cg, node, baseArrayReg, scratchArrayLengthReg, condReg, discontiguousArrayLabel);
            srm->reclaimScratchRegister(scratchArrayLengthReg);
            }
         else
            {
            // Zero length array or discontiguous array.  Unconditionally branch to the OOL path
            // to find out which.
            //
            OOLBranchInstr = generateLabelInstruction(cg, TR::InstOpCode::b, node, discontiguousArrayLabel);
            needsBoundCheckOOL = true;
            }
         }
      else
         {
         // Load the contiguous array length.
         arrayLengthReg = cg->evaluate(arrayLengthChild);
         // If the array length is 0, this is a discontiguous array and the bound check will be handled OOL.
         OOLBranchInstr = genSpineCheck(cg, node, arrayLengthReg, condReg, discontiguousArrayLabel);
         needsBoundCheckOOL = true;
         // Do the bound check using the contiguous array length.
         genBoundCheck(cg, node, indexReg, indexChild->getInt(), arrayLengthReg, arrayLengthChild->getInt(), condReg, noTrap);
         }

      srm->reclaimScratchRegister(condReg);
      cg->decReferenceCount(arrayLengthChild);
      }
   else
      {
      // Spine check only; load the contiguous length, check for 0, branch OOL if discontiguous.
      needsBoundCheckOOL = false;

      arrayLengthReg = srm->findOrCreateScratchRegister();

      TR::Register *condReg = srm->findOrCreateScratchRegister(TR_CCR);

      OOLBranchInstr = genSpineCheck(cg, node, baseArrayReg, arrayLengthReg, condReg, discontiguousArrayLabel);

      srm->reclaimScratchRegister(arrayLengthReg);
      srm->reclaimScratchRegister(condReg);
      }

   // For reference stores, only evaluate the array element address because the store cannot
   // happen here (it must be done via the array store check).
   //
   // For primitive stores, evaluate them now.
   // For loads, evaluate them now.
   // For address calculations (aladd/aiadd), evaluate them now.
   //
   bool doLoadOrStore;
   bool doLoadDecompress = false;
   bool doAddressComputation;
   TR::Compilation * comp = cg->comp();

   if (loadOrStoreChild->getOpCode().isStore() && loadOrStoreChild->getReferenceCount() > 1)
      {
      TR_ASSERT(loadOrStoreChild->getOpCode().isWrtBar(), "Opcode must be wrtbar");
      loadOrStoreReg = cg->evaluate(loadOrStoreChild->getFirstChild());
      cg->decReferenceCount(loadOrStoreChild->getFirstChild());
      doLoadOrStore = false;
      doAddressComputation = true;
      }
   else
      {
      // If it's a store and not commoned then it must be a primitive store.
      // If it's an address load it may need decompression in the OOL path.

      // Top-level check whether a decompression sequence is necessary, because the first child
      // may have been created by a PRE temp.
      //
      if ((loadOrStoreChild->getOpCodeValue() == TR::aload || loadOrStoreChild->getOpCodeValue() == TR::aRegLoad) && node->isSpineCheckWithArrayElementChild() && TR::Compiler->target.is64Bit()
            && comp->useCompressedPointers())
         {
         doLoadDecompress = true;
         }

      TR::Node *actualLoadOrStoreChild = loadOrStoreChild;
      while (actualLoadOrStoreChild->getOpCode().isConversion() || actualLoadOrStoreChild->containsCompressionSequence())
         {
         if (actualLoadOrStoreChild->containsCompressionSequence())
            doLoadDecompress = true;
         actualLoadOrStoreChild = actualLoadOrStoreChild->getFirstChild();
         }

      doLoadOrStore = actualLoadOrStoreChild->getOpCode().hasSymbolReference()
            && (actualLoadOrStoreChild->getSymbolReference()->getSymbol()->isArrayShadowSymbol()
                  || actualLoadOrStoreChild->getSymbolReference()->getSymbol()->isArrayletShadowSymbol()) && node->isSpineCheckWithArrayElementChild();

      // If the 1st child is not a load/store/aladd/aiadd it's probably a nop (e.g. const) at this point due to commoning
      //
      doAddressComputation = !doLoadOrStore && actualLoadOrStoreChild->getOpCode().isArrayRef() && !node->isSpineCheckWithArrayElementChild();

      if (doLoadOrStore || doAddressComputation || !loadOrStoreChild->getOpCode().isLoadConst())
         loadOrStoreReg = cg->evaluate(loadOrStoreChild);
      else
         loadOrStoreReg = NULL;
      }

   if (noTrap)
      {
      TR::RegisterDependencyConditions *mainlineDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 1, cg->trMemory());
      // Trampoline to bound check fail kills gr11
      // TODO: Try to use this as the arraylet reg
      TR::Register *gr11 = cg->allocateRegister();
      mainlineDeps->addPostCondition(gr11, TR::RealRegister::gr11);
      cg->stopUsingRegister(gr11);
      
      generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);
      TR::LabelSymbol *doneMainlineLabel = generateLabelSymbol(cg);
      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneMainlineLabel, mainlineDeps);
      }
   else
      generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);

   // OOL begin.
   //
   discontiguousArrayOOL->swapInstructionListsWithCompilation();
      {
      TR::Instruction *OOLLabelInstr = generateLabelInstruction(cg, TR::InstOpCode::label, node, discontiguousArrayLabel);
      // XXX: Temporary fix, OOL instruction stream does not pick up live locals or monitors correctly.
      TR_ASSERT(!OOLLabelInstr->getLiveLocals() && !OOLLabelInstr->getLiveMonitors(), "Expecting first OOL instruction to not have live locals/monitors info");
      OOLLabelInstr->setLiveLocals(OOLBranchInstr->getLiveLocals());
      OOLLabelInstr->setLiveMonitors(OOLBranchInstr->getLiveMonitors());

      if (needsBoundCheckOOL)
         {
         TR_ASSERT(needsBoundCheck, "Inconsistent state, needs bound check OOL but doesn't need bound check");

         TR::MemoryReference *discontiguousArraySizeMR = new (cg->trHeapMemory()) TR::MemoryReference(baseArrayReg, fej9->getOffsetOfDiscontiguousArraySizeField(), 4, cg);
         TR::Register *arrayLengthScratchReg = srm->findOrCreateScratchRegister();

         generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, arrayLengthScratchReg, discontiguousArraySizeMR);

         TR::Register *condReg = srm->findOrCreateScratchRegister(TR_CCR);

         // Do the bound check using the discontiguous array length.
         genBoundCheck(cg, node, indexReg, indexChild->getInt(), arrayLengthScratchReg, arrayLengthChild->getInt(), condReg, noTrap);

         srm->reclaimScratchRegister(condReg);
         srm->reclaimScratchRegister(arrayLengthScratchReg);
         }

      TR_ASSERT(!(doLoadOrStore && doAddressComputation), "Unexpected condition");

      TR::Register *arrayletReg;
      TR::DataType dt = loadOrStoreChild->getDataType();

      if (doLoadOrStore || doAddressComputation)
         {
         arrayletReg = doAddressComputation ? loadOrStoreReg : /* Needs to be in !gr0 in this case */cg->allocateRegister();

         // Generate the base+offset address pair into the arraylet.
         //
         int32_t elementSize = dt == TR::Address ? TR::Compiler->om.sizeofReferenceField() : TR::Symbol::convertTypeToSize(dt);
         TR::Register *arrayletOffsetReg;
         int32_t arrayletOffsetVal;

         if (indexReg)
            arrayletOffsetReg = srm->findOrCreateScratchRegister();

         genArrayletAccessAddr(cg, node, elementSize, baseArrayReg, indexReg, indexChild->getInt(), arrayletReg, arrayletOffsetReg, arrayletOffsetVal);

         // Decompress the arraylet pointer if necessary.
         //
         if (TR::Compiler->vm.heapBaseAddress())
            {
            TR::Register *condReg = srm->findOrCreateScratchRegister(TR_CCR);
            genDecompressPointer(cg, node, arrayletReg, condReg);
            srm->reclaimScratchRegister(condReg);
            }
         else
            genDecompressPointer(cg, node, arrayletReg);

         if (doLoadOrStore)
            {
            // Generate the load or store.
            //
            if (loadOrStoreChild->getOpCode().isStore())
               {
               bool isUnsigned = loadOrStoreChild->getOpCode().isUnsigned();
               TR::InstOpCode::Mnemonic storeOp = getLoadOrStoreFromDataType(cg, dt, elementSize, isUnsigned, false);

               if (storeOp == TR::InstOpCode::std && TR::Compiler->target.is32Bit())
                  {
                  // Store using consecutive stws.
                  TR_ASSERT(elementSize == 8 && loadOrStoreChild->getSecondChild()->getRegister()->getRegisterPair(), "Expecting 64-bit store value to be in a register pair");
                  if (indexReg)
                     {
                     TR::MemoryReference *arrayletMR = new (cg->trHeapMemory()) TR::MemoryReference(arrayletReg, arrayletOffsetReg, 4, cg);
                     generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, arrayletMR, loadOrStoreChild->getSecondChild()->getRegister()->getHighOrder());

                     generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, arrayletOffsetReg, arrayletOffsetReg, 4);

                     TR::MemoryReference *arrayletMR2 = new (cg->trHeapMemory()) TR::MemoryReference(arrayletReg, arrayletOffsetReg, 4, cg);
                     generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, arrayletMR2, loadOrStoreChild->getSecondChild()->getRegister()->getLowOrder());
                     }
                  else
                     {
                     TR::MemoryReference *arrayletMR = new (cg->trHeapMemory()) TR::MemoryReference(arrayletReg, arrayletOffsetVal, 4, cg);
                     TR::MemoryReference *arrayletMR2 = new (cg->trHeapMemory()) TR::MemoryReference(arrayletReg, arrayletOffsetVal + 4, 4, cg);
                     generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, arrayletMR, loadOrStoreChild->getSecondChild()->getRegister()->getHighOrder());
                     generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, arrayletMR2, loadOrStoreChild->getSecondChild()->getRegister()->getLowOrder());
                     }
                  }
               else
                  {
                  TR::MemoryReference *arrayletMR =
                        indexReg ? new (cg->trHeapMemory()) TR::MemoryReference(arrayletReg, arrayletOffsetReg, elementSize, cg) :
                              new (cg->trHeapMemory()) TR::MemoryReference(arrayletReg, arrayletOffsetVal, elementSize, cg);
                  generateMemSrc1Instruction(cg, storeOp, node, arrayletMR, loadOrStoreChild->getSecondChild()->getRegister());
                  }
               }
            else
               {
               TR_ASSERT(loadOrStoreChild->getOpCode().isConversion() || loadOrStoreChild->getOpCode().isLoad(), "Unexpected op");

               bool isUnsigned = loadOrStoreChild->getOpCode().isUnsigned();
               TR::InstOpCode::Mnemonic loadOp = getLoadOrStoreFromDataType(cg, dt, elementSize, isUnsigned, true);

               if (loadOp == TR::InstOpCode::ld && TR::Compiler->target.is32Bit())
                  {
                  // Load using consecutive lwzs.
                  TR_ASSERT(elementSize == 8 && loadOrStoreReg->getRegisterPair(), "Expecting 64-bit load value to be in a register pair");
                  if (indexReg)
                     {
                     TR::MemoryReference *arrayletMR = new (cg->trHeapMemory()) TR::MemoryReference(arrayletReg, arrayletOffsetReg, 4, cg);
                     generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, loadOrStoreReg->getHighOrder(), arrayletMR);

                     generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, arrayletOffsetReg, arrayletOffsetReg, 4);

                     TR::MemoryReference *arrayletMR2 = new (cg->trHeapMemory()) TR::MemoryReference(arrayletReg, arrayletOffsetReg, 4, cg);
                     generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, loadOrStoreReg->getLowOrder(), arrayletMR2);
                     }
                  else
                     {
                     TR::MemoryReference *arrayletMR = new (cg->trHeapMemory()) TR::MemoryReference(arrayletReg, arrayletOffsetVal, 4, cg);
                     TR::MemoryReference *arrayletMR2 = new (cg->trHeapMemory()) TR::MemoryReference(arrayletReg, arrayletOffsetVal + 4, 4, cg);
                     generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, loadOrStoreReg->getHighOrder(), arrayletMR);
                     generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, loadOrStoreReg->getLowOrder(), arrayletMR2);
                     }
                  }
               else
                  {
                  TR::MemoryReference *arrayletMR =
                        indexReg ? new (cg->trHeapMemory()) TR::MemoryReference(arrayletReg, arrayletOffsetReg, elementSize, cg) :
                              new (cg->trHeapMemory()) TR::MemoryReference(arrayletReg, arrayletOffsetVal, elementSize, cg);
                  generateTrg1MemInstruction(cg, loadOp, node, loadOrStoreReg, arrayletMR);
                  }

               if (doLoadDecompress)
                  {
                  TR_ASSERT(dt == TR::Address, "Expecting loads with decompression trees to have data type TR::Address");

                  if (TR::Compiler->vm.heapBaseAddress())
                     {
                     TR::Register *condReg = srm->findOrCreateScratchRegister(TR_CCR);
                     genDecompressPointer(cg, node, loadOrStoreReg, condReg);
                     srm->reclaimScratchRegister(condReg);
                     }
                  else
                     genDecompressPointer(cg, node, loadOrStoreReg);
                  }
               }

            cg->stopUsingRegister(arrayletReg);
            }
         else
            {
            if (indexReg)
               generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, loadOrStoreReg, loadOrStoreReg, arrayletOffsetReg);
            else
               addConstantToInteger(node, loadOrStoreReg, loadOrStoreReg, arrayletOffsetVal, cg);
            }

         if (indexReg)
            srm->reclaimScratchRegister(arrayletOffsetReg);
         }

      const uint32_t numOOLDeps = 1 + (doLoadOrStore ? 1 : 0) + (needsBoundCheck && arrayLengthReg ? 1 : 0) + (loadOrStoreReg ? (loadOrStoreReg->getRegisterPair() ? 2 : 1) : 0)
            + (indexReg ? 1 : 0) + srm->numAvailableRegisters();
      TR::RegisterDependencyConditions *OOLDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numOOLDeps, cg->trMemory());
      OOLDeps->addPostCondition(baseArrayReg, TR::RealRegister::NoReg);
      TR_ASSERT(OOLDeps->getPostConditions()->getRegisterDependency(0)->getRegister() == baseArrayReg, "Unexpected register");
      OOLDeps->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0();
      if (doLoadOrStore)
         {
         OOLDeps->addPostCondition(arrayletReg, TR::RealRegister::NoReg);
         TR_ASSERT(OOLDeps->getPostConditions()->getRegisterDependency(1)->getRegister() == arrayletReg, "Unexpected register");
         OOLDeps->getPostConditions()->getRegisterDependency(1)->setExcludeGPR0();
         }
      if (indexReg)
         OOLDeps->addPostCondition(indexReg, TR::RealRegister::NoReg);
      if (loadOrStoreReg)
         {
         if (loadOrStoreReg->getRegisterPair())
            {
            TR_ASSERT(dt == TR::Int64 && TR::Compiler->target.is32Bit(), "Unexpected register pair");
            OOLDeps->addPostCondition(loadOrStoreReg->getHighOrder(), TR::RealRegister::NoReg);
            OOLDeps->addPostCondition(loadOrStoreReg->getLowOrder(), TR::RealRegister::NoReg);
            }
         else
            OOLDeps->addPostCondition(loadOrStoreReg, TR::RealRegister::NoReg);
         }
      if (needsBoundCheck && arrayLengthReg)
         OOLDeps->addPostCondition(arrayLengthReg, TR::RealRegister::NoReg);
      srm->addScratchRegistersToDependencyList(OOLDeps);

      srm->stopUsingRegisters();

      TR::LabelSymbol *doneOOLLabel = generateLabelSymbol(cg);
      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneOOLLabel, OOLDeps);
      generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);
      }
   discontiguousArrayOOL->swapInstructionListsWithCompilation();
   //
   // OOL end.

   cg->decReferenceCount(loadOrStoreChild);
   cg->decReferenceCount(baseArrayChild);
   cg->decReferenceCount(indexChild);

   return NULL;
   }

static void VMoutlinedHelperArrayStoreCHKEvaluator(TR::Node *node, TR::Register *srcReg, TR::Register *dstReg, bool srcIsNonNull, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   TR::CodeCache *codeCache = comp->getCurrentCodeCache();
   TR::LabelSymbol *doneArrayStoreCHKLabel = generateLabelSymbol(cg);
   TR::Register *rootClassReg = cg->allocateRegister();
   TR::Register *scratchReg = cg->allocateRegister();
   TR::Register *srcNullCondReg;

   TR_PPCScratchRegisterDependencyConditions deps;

   if (!srcIsNonNull)
      {
      TR_ASSERT(NULLVALUE <= UPPER_IMMED && NULLVALUE >= LOWER_IMMED, "Expecting NULLVALUE to fit in immediate field");
      srcNullCondReg = cg->allocateRegister(TR_CCR);
      generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, srcNullCondReg, srcReg, NULLVALUE);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneArrayStoreCHKLabel, srcNullCondReg);
      }

   deps.addDependency(cg, dstReg, TR::RealRegister::gr3);
   deps.addDependency(cg, srcReg, TR::RealRegister::gr4);
   // Clobbered by the helper
   deps.addDependency(cg, NULL, TR::RealRegister::gr5);
   deps.addDependency(cg, NULL, TR::RealRegister::gr6);
   deps.addDependency(cg, scratchReg, TR::RealRegister::gr7);
   deps.addDependency(cg, rootClassReg, TR::RealRegister::gr11);

   TR_OpaqueClassBlock *rootClass = fej9->getSystemClassFromClassName("java/lang/Object", 16);
   if (cg->wantToPatchClassPointer(rootClass, node))
      loadAddressConstantInSnippet(cg, node, (intptrj_t) rootClass, rootClassReg, scratchReg,TR::InstOpCode::Op_load, false, NULL);
   else
      loadAddressConstant(cg, node, (intptrj_t) rootClass, rootClassReg);

   TR_CCPreLoadedCode helper = TR_arrayStoreCHK;
   uintptrj_t helperAddr = (uintptrj_t) codeCache->getCCPreLoadedCodeAddress(helper, cg);
   TR::SymbolReference *symRef = comp->getSymRefTab()->findOrCreatePerCodeCacheHelperSymbolRef(helper, helperAddr);

   TR::Instruction *gcPoint = generateDepImmSymInstruction(cg, TR::InstOpCode::bl, node, helperAddr, new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 0, cg->trMemory()),
         symRef);
   gcPoint->PPCNeedsGCMap(0xFFFFFFFF);

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneArrayStoreCHKLabel, TR_PPCScratchRegisterDependencyConditions::createDependencyConditions(cg, NULL, &deps));

   cg->stopUsingRegister(rootClassReg);
   cg->stopUsingRegister(scratchReg);

   if (!srcIsNonNull)
      cg->stopUsingRegister(srcNullCondReg);
   }

TR::Register *outlinedHelperArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *srcNode = node->getFirstChild()->getSecondChild();
   TR::Node *dstNode = node->getFirstChild()->getThirdChild();
   TR::Register *srcReg = cg->evaluate(srcNode);
   TR::Register *dstReg = cg->evaluate(dstNode);
   TR::Compilation* comp = cg->comp();

   if (!srcNode->isNull())
      {
      VMoutlinedHelperArrayStoreCHKEvaluator(node, srcReg, dstReg, srcNode->isNonNull(), cg);
      }

   TR::MemoryReference *storeMR = new (cg->trHeapMemory()) TR::MemoryReference(node->getFirstChild(), TR::Compiler->om.sizeofReferenceAddress(), cg);

   generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, storeMR, srcReg);

   if (!srcNode->isNull())
      {
      VMoutlinedHelperWrtbarEvaluator(node->getFirstChild(), srcReg, dstReg, srcNode->isNonNull(), cg);
      }

   cg->decReferenceCount(srcNode);
   cg->decReferenceCount(dstNode);
   storeMR->decNodeReferenceCounts(cg);
   cg->decReferenceCount(node->getFirstChild());

   return NULL;
   }

TR::Instruction *J9::Power::TreeEvaluator::generateVFTMaskInstruction(TR::CodeGenerator *cg, TR::Node *node, TR::Register *dstReg, TR::Register *srcReg, TR::Instruction *preced)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   uintptrj_t mask = TR::Compiler->om.maskOfObjectVftField();
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
   bool isCompressed = true;
#else
   bool isCompressed = false;
#endif
   if (~mask == 0)
      {
      // no mask instruction required
      return preced;
      }
   else
      {
      TR::InstOpCode::Mnemonic op = (TR::Compiler->target.is64Bit() && !isCompressed) ? TR::InstOpCode::rldicr : TR::InstOpCode::rlwinm;
      return generateTrg1Src1Imm2Instruction(cg, op, node, dstReg, srcReg, 0, mask, preced);
      }
   }

TR::Instruction *J9::Power::TreeEvaluator::generateVFTMaskInstruction(TR::CodeGenerator *cg, TR::Node *node, TR::Register *reg, TR::Instruction *preced)
   {
   return TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, reg, reg, preced);
   }

static TR::Instruction *genTestIsSuper(TR::Node *node, TR::Register *objClassReg, TR::Register *castClassReg, TR::Register *crReg, TR::Register *scratch1Reg,
      TR::Register *scratch2Reg, int32_t castClassDepth, TR::LabelSymbol *failLabel, TR::Instruction *cursor, TR::CodeGenerator *cg, int32_t depthInReg2 = 0)
   {
   TR::Compilation* comp = cg->comp();
   int32_t superClassOffset = castClassDepth * TR::Compiler->om.sizeofReferenceAddress();
   bool outOfBound = (!depthInReg2 && (superClassOffset > UPPER_IMMED || superClassOffset < LOWER_IMMED)) ? true : false;
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
   // objClassReg contains the class offset so we may need to generate code
   // to convert from class offset to real J9Class pointer
#endif
   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, scratch1Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(objClassReg, offsetof(J9Class, classDepthAndFlags), TR::Compiler->om.sizeofReferenceAddress(), cg), cursor);
   if (outOfBound)
      {
      cursor = loadConstant(cg, node, castClassDepth, scratch2Reg, cursor);
      }
   cursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, scratch1Reg, scratch1Reg, 0, J9_JAVA_CLASS_DEPTH_MASK, cursor);

   if (outOfBound || depthInReg2)
      {
      cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp4, node, crReg, scratch1Reg, scratch2Reg, cursor);
      }
   else
      {
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, crReg, scratch1Reg, castClassDepth, cursor);
      }
   cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::ble, node, failLabel, crReg, cursor);

   if (outOfBound || depthInReg2)
      {
      if (TR::Compiler->target.is64Bit())
         cursor = generateShiftLeftImmediateLong(cg, node, scratch2Reg, scratch2Reg, 3, cursor);
      else
         cursor = generateShiftLeftImmediate(cg, node, scratch2Reg, scratch2Reg, 2, cursor);
      }
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
   // objClassReg contains the class offset so we may need to generate code
   // to convert from class offset to real J9Class pointer
#endif
   cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, scratch1Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(objClassReg, offsetof(J9Class, superclasses), TR::Compiler->om.sizeofReferenceAddress(), cg), cursor);

   if (outOfBound || depthInReg2)
      {
      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_loadx, node, scratch1Reg, new (cg->trHeapMemory()) TR::MemoryReference(scratch1Reg, scratch2Reg, TR::Compiler->om.sizeofReferenceAddress(), cg),
            cursor);
      }
   else
      {
      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, scratch1Reg,
            new (cg->trHeapMemory()) TR::MemoryReference(scratch1Reg, superClassOffset, TR::Compiler->om.sizeofReferenceAddress(), cg), cursor);
      }
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
   // castClassReg has a class offset and scratch1Reg contains a J9Class pointer
   // May need to convert the J9Class pointer to a class offset
#endif
   cursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmp, node, crReg, scratch1Reg, castClassReg, cursor);
   return (cursor);
   }

static void VMarrayStoreCHKEvaluator(TR::Node *node, TR::Register *src, TR::Register *dst, TR::Register *t1Reg, TR::Register *t2Reg, TR::Register *t3Reg, TR::Register *t4Reg,
      TR::Register *cndReg, TR::LabelSymbol *toWB, TR::LabelSymbol *fLabel, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());

#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
   // must read only 32 bits
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, t1Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(dst, (int32_t)TR::Compiler->om.offsetOfObjectVftField(), 4, cg));
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, t2Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(src, (int32_t)TR::Compiler->om.offsetOfObjectVftField(), 4, cg));
   TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, t1Reg);
   TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, t2Reg);
   // here we may need to convert from class offset contained in t1Reg to
   // J9Class pointer
   generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, t1Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(t1Reg, (int32_t)offsetof(J9ArrayClass, componentType), TR::Compiler->om.sizeofReferenceAddress(), cg));
   // here we may need to convert from J9Class pointer contained in t1Reg
   // into class offset (so that we can compare two classes)
#else
   generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, t1Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(dst, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceAddress(), cg));
   generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, t2Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(src, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceAddress(), cg));
   TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, t1Reg);
   TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, t2Reg);
   generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, t1Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(t1Reg, (int32_t) offsetof(J9ArrayClass, componentType), TR::Compiler->om.sizeofReferenceAddress(), cg));
#endif

   TR_OpaqueClassBlock *rootClass = fej9->getSystemClassFromClassName("java/lang/Object", 16);

   if (cg->wantToPatchClassPointer(rootClass, node))
      loadAddressConstantInSnippet(cg, node, (intptrj_t) rootClass, t3Reg, t4Reg,TR::InstOpCode::Op_load, false, NULL);
   else
      loadAddressConstant(cg, node, (intptrj_t) rootClass, t3Reg);

   generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, t4Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(t2Reg, (int32_t) offsetof(J9Class, castClassCache), TR::Compiler->om.sizeofReferenceAddress(), cg));

   generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, cndReg, t1Reg, t2Reg);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, toWB, cndReg);

   generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, cndReg, t1Reg, t4Reg);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, toWB, cndReg);

   generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, cndReg, t1Reg, t3Reg);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, toWB, cndReg);

   if ((!TR::Options::getCmdLineOptions()->getOption(TR_DisableArrayStoreCheckOpts)) && node->getArrayComponentClassInNode())
      {
      TR_OpaqueClassBlock *castClass = (TR_OpaqueClassBlock *) node->getArrayComponentClassInNode();

      if (cg->wantToPatchClassPointer(castClass, node))
         loadAddressConstantInSnippet(cg, node, (intptrj_t) castClass, t3Reg, t4Reg,TR::InstOpCode::Op_load, false, NULL);
      else
         loadAddressConstant(cg, node, (intptrj_t) castClass, t3Reg);

      generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, cndReg, t1Reg, t3Reg);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, toWB, cndReg);
      }

#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
   // For the following two instructions
   // we may need to convert the class offset from t1Reg into J9Class
#endif
   generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, t3Reg, new (cg->trHeapMemory()) TR::MemoryReference(t1Reg, (int32_t) offsetof(J9Class, romClass), TR::Compiler->om.sizeofReferenceAddress(), cg));
   generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, t4Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(t1Reg, (int32_t) offsetof(J9Class, classDepthAndFlags), TR::Compiler->om.sizeofReferenceAddress(), cg));
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, t3Reg, new (cg->trHeapMemory()) TR::MemoryReference(t3Reg, (int32_t) offsetof(J9ROMClass, modifiers), 4, cg));
   generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, t4Reg, t4Reg, 0, J9_JAVA_CLASS_DEPTH_MASK);
   generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, t3Reg, t3Reg, 31, 0xFFFFFFFF);
   TR_ASSERT(J9_JAVA_CLASS_ARRAY == (1 << 16) && J9_JAVA_INTERFACE == (1 << 9), "Verify code sequence is still right ...");
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, t3Reg, t3Reg, cndReg, (J9_JAVA_CLASS_ARRAY | J9_JAVA_INTERFACE) >> 1);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, fLabel, cndReg);
   genTestIsSuper(node, t2Reg, t1Reg, cndReg, t3Reg, t4Reg, 0, fLabel, cg->getAppendInstruction(), cg, 1);

   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, fLabel, cndReg);
   }

TR::Register *J9::Power::TreeEvaluator::ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   static bool oldArrayStoreCHK = feGetEnv("TR_newArrayStoreCheck") == NULL;
   if (!comp->compileRelocatableCode() && !TR::Options::getCmdLineOptions()->realTimeGC() && !comp->useCompressedPointers() && !oldArrayStoreCHK)
      return outlinedHelperArrayStoreCHKEvaluator(node, cg);

   // Note: we take advantage of the register conventions of the helpers by limiting register usages on
   //       the fast-path (most likely 4 registers; at most, 6 registers)

   TR::Node * firstChild = node->getFirstChild();

   TR_WriteBarrierKind gcMode = comp->getOptions()->getGcMode();

   bool doWrtBar = (gcMode == TR_WrtbarRealTime || gcMode == TR_WrtbarOldCheck || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarAlways)
         || (TR::Options::getCmdLineOptions()->realTimeGC());

   bool doCrdMrk = ((gcMode == TR_WrtbarCardMark || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarCardMarkIncremental) && !firstChild->isNonHeapObjectWrtBar());

   TR::Node *sourceChild = firstChild->getSecondChild();
   TR::Node *destinationChild = firstChild->getChild(2);

   bool usingCompressedPointers = false;
   bool bumpedRefCount = false;
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
      // -or- if the field is known to be null
      // iistore f
      //    aload O
      //    l2i
      //      a2l
      //        value  <- valueChild
      //
      TR::Node *translatedNode = sourceChild;
      if (translatedNode->getOpCode().isConversion())
         translatedNode = translatedNode->getFirstChild();
      if (translatedNode->getOpCode().isRightShift()) // optional
         translatedNode = translatedNode->getFirstChild();

      bool usingLowMemHeap = false;
      if (TR::Compiler->vm.heapBaseAddress() == 0 || sourceChild->isNull())
         usingLowMemHeap = true;

      if ((translatedNode->getOpCode().isSub()) || usingLowMemHeap)
         usingCompressedPointers = true;

      if (usingCompressedPointers)
         {
         while ((sourceChild->getNumChildren() > 0) && (sourceChild->getOpCodeValue() != TR::a2l))
            sourceChild = sourceChild->getFirstChild();
         if (sourceChild->getOpCodeValue() == TR::a2l)
            sourceChild = sourceChild->getFirstChild();
         }
      }

   // Since ArrayStoreCHK doesn't have the shape of the corresponding helper call we have to create this tree
   // so we can have it evaluated out of line
   TR::Node *helperCallNode = TR::Node::createWithSymRef(node, TR::call, 2, node->getSymbolReference());
   helperCallNode->setAndIncChild(0, sourceChild);
   helperCallNode->setAndIncChild(1, destinationChild);
   if (comp->getOption(TR_TraceCG))
      {
      traceMsg(comp, "%s: Creating and evaluating the following tree to generate the necessary helper call for this node\n", node->getOpCode().getName());
      cg->getDebug()->print(comp->getOutFile(), helperCallNode);
      }

   TR::Register *srcReg, *dstReg;
   TR::Register *condReg, *temp1Reg, *temp2Reg, *temp3Reg, *temp4Reg, *baseReg, *indexReg;
   TR::MemoryReference *tempMR1, *tempMR2;
   TR::LabelSymbol *wbLabel, *doneLabel, *startLabel, *storeLabel, *wrtBarEndLabel;
   TR::RegisterDependencyConditions *conditions;
   TR::PPCPrivateLinkage *linkage = (TR::PPCPrivateLinkage *) cg->getLinkage();
   const TR::PPCLinkageProperties &properties = linkage->getProperties();
   TR::Instruction *gcPoint;
   bool stopUsingSrc = false;

   wbLabel = generateLabelSymbol(cg);
   doneLabel = generateLabelSymbol(cg);
   storeLabel = (doWrtBar) ? (generateLabelSymbol(cg)) : NULL;

   tempMR1 = new (cg->trHeapMemory()) TR::MemoryReference(firstChild, TR::Compiler->om.sizeofReferenceAddress(), cg);
   dstReg = cg->evaluate(destinationChild);
   TR::Register *compressedReg;
   if (sourceChild->getReferenceCount() > 1 && (srcReg = sourceChild->getRegister()) != NULL)
      {
      TR::Register *tempReg = cg->allocateCollectedReferenceRegister();

      // Source must be an object.
      TR_ASSERT(!srcReg->containsInternalPointer(), "Stored value is an internal pointer");

      generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, tempReg, srcReg);
      srcReg = tempReg;
      stopUsingSrc = true;
      compressedReg = srcReg;
      if (usingCompressedPointers)
         compressedReg = cg->evaluate(firstChild->getSecondChild());
      }
   else
      {
      srcReg = cg->evaluate(sourceChild);
      compressedReg = srcReg;
      if (usingCompressedPointers)
         compressedReg = cg->evaluate(firstChild->getSecondChild());
      }

   int32_t numDeps = 11;
   if (usingCompressedPointers)
      numDeps++;
   if ((gcMode == TR_WrtbarRealTime) || (TR::Options::getCmdLineOptions()->realTimeGC()))
      numDeps++;
   if (!firstChild->skipWrtBar())
      numDeps += 2;

   conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(numDeps, numDeps, cg->trMemory());
   temp1Reg = cg->allocateRegister();
   temp2Reg = cg->allocateRegister();
   temp3Reg = cg->allocateRegister();
   temp4Reg = cg->allocateRegister();
   condReg = cg->allocateRegister(TR_CCR);

   // !!! Adding any dependency before the baseReg, you have to be careful with the excludeGPR0 order
   addDependency(conditions, dstReg, TR::RealRegister::gr3, TR_GPR, cg);

   if (TR::Options::getCmdLineOptions()->realTimeGC())
      addDependency(conditions, srcReg, TR::RealRegister::gr5, TR_GPR, cg);
   else
      addDependency(conditions, srcReg, TR::RealRegister::gr4, TR_GPR, cg);

   addDependency(conditions, temp1Reg, TR::RealRegister::gr11, TR_GPR, cg);
   addDependency(conditions, temp2Reg, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, temp3Reg, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, temp4Reg, TR::RealRegister::NoReg, TR_GPR, cg);
   conditions->getPreConditions()->getRegisterDependency(3)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(3)->setExcludeGPR0();
   conditions->getPreConditions()->getRegisterDependency(4)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(4)->setExcludeGPR0();
   addDependency(conditions, condReg, TR::RealRegister::cr0, TR_CCR, cg);

   baseReg = tempMR1->getBaseRegister();
   if (baseReg != NULL && baseReg != srcReg && baseReg != dstReg)
      {
      addDependency(conditions, baseReg, TR::RealRegister::NoReg, TR_GPR, cg);
      conditions->getPreConditions()->getRegisterDependency(7)->setExcludeGPR0();
      conditions->getPostConditions()->getRegisterDependency(7)->setExcludeGPR0();
      }
   indexReg = tempMR1->getIndexRegister();
   if (indexReg != NULL && indexReg != srcReg && indexReg != dstReg && indexReg != baseReg)
      {
      addDependency(conditions, indexReg, TR::RealRegister::NoReg, TR_GPR, cg);
      }

   TR::Register *dstAddrReg = NULL;

   if (TR::Options::getCmdLineOptions()->realTimeGC())
      {
      dstAddrReg = cg->allocateRegister();
      addDependency(conditions, dstAddrReg, TR::RealRegister::gr4, TR_GPR, cg);
      }

   if (usingCompressedPointers)
      addDependency(conditions, compressedReg, TR::RealRegister::NoReg, TR_GPR, cg);

   //--Generate Test to see if 'sourceRegister' is NULL
   generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, condReg, srcReg, NULLVALUE);
   //--Generate Jump past ArrayStoreCHKEvaluator or WrtBar depending on GC policy
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, ((!TR::Options::getCmdLineOptions()->realTimeGC()) && doWrtBar) ? storeLabel : wbLabel, condReg);

   TR::LabelSymbol *helperCall = generateLabelSymbol(cg);
   VMarrayStoreCHKEvaluator(node, srcReg, dstReg, temp1Reg, temp2Reg, temp3Reg, temp4Reg, condReg, wbLabel, helperCall, cg);

   TR_PPCOutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_PPCOutOfLineCodeSection(helperCallNode, TR::call, NULL, helperCall, wbLabel, cg);
   cg->getPPCOutOfLineCodeSectionList().push_front(outlinedHelperCall);
   cg->decReferenceCount(helperCallNode->getFirstChild());
   cg->decReferenceCount(helperCallNode->getSecondChild());

   generateLabelInstruction(cg, TR::InstOpCode::label, node, wbLabel);

   if (!TR::Options::getCmdLineOptions()->realTimeGC())
      {
      if (usingCompressedPointers)
         generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempMR1, compressedReg);
      else
         generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, tempMR1, srcReg);

      wrtBarEndLabel = doneLabel;
      }
   else
      wrtBarEndLabel = storeLabel;

   if (!firstChild->skipWrtBar())
      {
      if (doCrdMrk && !doWrtBar)
         VMCardCheckEvaluator(firstChild, dstReg, condReg, temp1Reg, temp2Reg, temp3Reg, conditions, cg);
      else if (doWrtBar && doCrdMrk)
         VMnonNullSrcWrtBarCardCheckEvaluator(node, srcReg, dstReg, condReg, temp1Reg, temp2Reg, temp3Reg, temp4Reg, doneLabel, conditions, NULL, false, usingCompressedPointers,
               cg);
      else if (doWrtBar && !doCrdMrk)
         VMnonNullSrcWrtBarCardCheckEvaluator(node, srcReg, dstReg, condReg, temp1Reg, temp2Reg, dstAddrReg, temp4Reg, wrtBarEndLabel, conditions, tempMR1, false,
               usingCompressedPointers, cg);
      }

   if (doWrtBar)
      {
      if (!TR::Options::getCmdLineOptions()->realTimeGC())
         generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);

      //--Generate the store instruction skipped over if source is null
      generateLabelInstruction(cg, TR::InstOpCode::label, node, storeLabel);
      int32_t sizeofMR;
      TR::InstOpCode::Mnemonic storeOp;
      if (usingCompressedPointers)
         {
         sizeofMR = TR::Compiler->om.sizeofReferenceField();
         storeOp = TR::InstOpCode::stw;
         }
      else
         {
         sizeofMR = TR::Compiler->om.sizeofReferenceAddress();
         storeOp =TR::InstOpCode::Op_st;
         }
      tempMR2 = new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMR1, 0, sizeofMR, cg);
      generateMemSrc1Instruction(cg, storeOp, node, tempMR2, compressedReg);
      }

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

   if (comp->useCompressedPointers() && firstChild->getOpCode().isIndirect())
      firstChild->setStoreAlreadyEvaluated(true);

   if (usingCompressedPointers)
      {
      cg->decReferenceCount(firstChild->getSecondChild());
      }
   else
      cg->decReferenceCount(sourceChild);
   cg->decReferenceCount(destinationChild);
   tempMR1->decNodeReferenceCounts(cg);
   cg->decReferenceCount(firstChild);

   if (stopUsingSrc)
      cg->stopUsingRegister(srcReg);
   if (TR::Options::getCmdLineOptions()->realTimeGC())
      cg->stopUsingRegister(dstAddrReg);
   cg->stopUsingRegister(temp1Reg);
   cg->stopUsingRegister(temp2Reg);
   cg->stopUsingRegister(temp3Reg);
   cg->stopUsingRegister(temp4Reg);
   cg->stopUsingRegister(condReg);

   return NULL;
   }

TR::Register *J9::Power::TreeEvaluator::ArrayCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::VMarrayCheckEvaluator(node, cg);
   }

TR::Register *J9::Power::TreeEvaluator::conditionalHelperEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // There are two points which have to be considered for this evaluator:
   //  1) This conditional tree is of a special form which is lowered just
   //     before instruction selection. This means the conditional is invisible
   //     to the control flow analysis. As such, we have to treat it just as
   //     part of a basic block;
   //  2) The two opCodes using this evaluator are intended as quick JVMPI
   //     probes. They should not impact the normal code (in terms of performance).
   //     As such, their call evaluation will be specially treated not to go
   //     through the normal linkage. OTI will preserve all the registers as
   //     needed.

   TR::RegisterDependencyConditions *conditions;
   TR::Register *cndReg, *jumpReg, *valReg;
   TR::Node *testNode = node->getFirstChild(), *callNode = node->getSecondChild();
   TR::Node *firstChild = testNode->getFirstChild(), *secondChild = testNode->getSecondChild();
   int32_t value, i, numArgs = callNode->getNumChildren();

   conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(4, 4, cg->trMemory());

   TR_ASSERT(numArgs <= 2, "Unexpected number of arguments for helper.");

   // Helper arguments are in reversed order of the private linkage
   // Argument registers are not needed to be split since the helper will
   // preserve all of them.
   int32_t iArgIndex = 0, fArgIndex = 0;
   TR::Linkage *linkage = cg->createLinkage(TR_Private);
   for (i = numArgs - 1; i >= 0; i--)
      {
      TR::Register *argReg = cg->evaluate(callNode->getChild(i));
      addDependency(conditions, argReg, (argReg->getKind() == TR_GPR) ? // Didn't consider Long here
            linkage->getProperties().getIntegerArgumentRegister(iArgIndex++) : linkage->getProperties().getFloatArgumentRegister(fArgIndex++), argReg->getKind(), cg);
      }

   cndReg = cg->allocateRegister(TR_CCR);
   jumpReg = cg->evaluate(firstChild);
   addDependency(conditions, cndReg, TR::RealRegister::NoReg, TR_CCR, cg);
   addDependency(conditions, jumpReg, TR::RealRegister::gr11, TR_GPR, cg);

   if (secondChild->getOpCode().isLoadConst() && (value = secondChild->getInt()) <= UPPER_IMMED && value >= LOWER_IMMED)
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, testNode, cndReg, jumpReg, value);
      }
   else
      {
      valReg = cg->evaluate(secondChild);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp4, testNode, cndReg, jumpReg, valReg);
      }

   TR::Instruction *gcPoint;
   TR::LabelSymbol *snippetLabel = cg->lookUpSnippet(TR::Snippet::IsHelperCall, node->getSymbolReference());
   if (snippetLabel == NULL)
      {
      snippetLabel = generateLabelSymbol(cg);
      cg->addSnippet(new (cg->trHeapMemory()) TR::PPCHelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference()));
      }
   gcPoint = generateDepConditionalBranchInstruction(cg, testNode->getOpCodeValue() == TR::icmpeq ? TR::InstOpCode::beql : TR::InstOpCode::bnel, node, snippetLabel, cndReg, conditions);
   gcPoint->PPCNeedsGCMap(0xFFFFFFFF);

   cg->stopUsingRegister(cndReg);
   for (i = numArgs - 1; i >= 0; i--)
      cg->decReferenceCount(callNode->getChild(i));
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->decReferenceCount(testNode);
   cg->decReferenceCount(callNode);
   return NULL;
   }

TR::Register *J9::Power::TreeEvaluator::flushEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();

   if (opCode == TR::allocationFence)
      {
      if (!node->canOmitSync())
         generateInstruction(cg, TR::InstOpCode::lwsync, node);
      }
   else
      {
      if (opCode == TR::loadFence)
         {
         if (TR::Compiler->target.cpu.id() == TR_PPCp7)
            generateInstruction(cg, TR::InstOpCode::lwsync, node);
         else
            generateInstruction(cg, TR::InstOpCode::isync, node);
         }
      else if (opCode == TR::storeFence)
         generateInstruction(cg, TR::InstOpCode::lwsync, node);
      else if (opCode == TR::fullFence)
         {
         if (node->canOmitSync())
            generateLabelInstruction(cg, TR::InstOpCode::label, node, generateLabelSymbol(cg));
         else
            generateInstruction(cg, TR::InstOpCode::sync, node);
         }
      }

   return NULL;
   }

static
void genCheckCastTransitionToNextSequence(TR::Node *node, J9::TreeEvaluator::InstanceOfOrCheckCastSequences *iter, TR::LabelSymbol *nextSequenceLabel, TR::LabelSymbol *trueLabel, TR::Register *condReg, TR::CodeGenerator *cg)
   {
   J9::TreeEvaluator::InstanceOfOrCheckCastSequences current = *iter++;

   if (current == J9::TreeEvaluator::EvaluateCastClass || current == J9::TreeEvaluator::LoadObjectClass)
      return;
   if (current == J9::TreeEvaluator::NullTest && node->getOpCodeValue() == TR::checkcastAndNULLCHK)
      return;

   J9::TreeEvaluator::InstanceOfOrCheckCastSequences next = *iter;

   while (next == J9::TreeEvaluator::EvaluateCastClass || next == J9::TreeEvaluator::LoadObjectClass)
      next = *++iter;

   bool nextSequenceIsOutOfLine = next == J9::TreeEvaluator::HelperCall || next == J9::TreeEvaluator::GoToFalse;

   if (nextSequenceIsOutOfLine)
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, nextSequenceLabel, condReg);
   else
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, trueLabel, condReg);
   }

static
void genInstanceOfTransitionToNextSequence(TR::Node *node, J9::TreeEvaluator::InstanceOfOrCheckCastSequences *iter, TR::LabelSymbol *nextSequenceLabel, TR::LabelSymbol *doneLabel, TR::Register *condReg, TR::Register *resultReg, bool resultRegInitialValue, TR::LabelSymbol *oppositeResultLabel, bool profiledClassIsInstanceOf, TR::CodeGenerator *cg)
   {
   J9::TreeEvaluator::InstanceOfOrCheckCastSequences current = *iter++;

   if (current == J9::TreeEvaluator::EvaluateCastClass || current == J9::TreeEvaluator::LoadObjectClass)
      return;
   if (current == J9::TreeEvaluator::NullTest && node->getOpCodeValue() == TR::checkcastAndNULLCHK)
      return;
   if (current == J9::TreeEvaluator::GoToFalse)
      return;

   J9::TreeEvaluator::InstanceOfOrCheckCastSequences next = *iter;

   while (next == J9::TreeEvaluator::EvaluateCastClass || next == J9::TreeEvaluator::LoadObjectClass)
      next = *++iter;

   if (next == J9::TreeEvaluator::GoToFalse && resultRegInitialValue == false)
      nextSequenceLabel = doneLabel;

   bool nextSequenceIsOutOfLine = next == J9::TreeEvaluator::HelperCall;

   if (current == J9::TreeEvaluator::NullTest)
      {
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, resultRegInitialValue == false ? doneLabel : oppositeResultLabel, condReg);
      }
   else if (current == J9::TreeEvaluator::CastClassCacheTest)
      {
      if (resultRegInitialValue == true)
         {
         if (nextSequenceIsOutOfLine)
            {
            generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, doneLabel, condReg);
            generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, node, nextSequenceLabel, condReg);
            // Fall through to oppositeResultLabel
            }
         else
            {
            generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, doneLabel, condReg);
            generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, oppositeResultLabel, condReg);
            // Fall through to nextSequenceLabel
            }
         }
      else
         {
         if (nextSequenceIsOutOfLine)
            {
            generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, condReg);
            generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, node, nextSequenceLabel, condReg);
            // Fall through to oppositeResultLabel
            }
         else
            {
            generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, condReg);
            generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, oppositeResultLabel, condReg);
            // Fall through to nextSequenceLabel
            }
         }
      }
   else if (current == J9::TreeEvaluator::ProfiledClassTest)
      {
      if (resultRegInitialValue == true)
         {
         if (profiledClassIsInstanceOf)
            {
            if (nextSequenceIsOutOfLine)
               {
               generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, condReg);
               generateLabelInstruction(cg, TR::InstOpCode::b, node, nextSequenceLabel);
               }
            else
               {
               generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, condReg);
               }
            }
         else
            {
            if (nextSequenceIsOutOfLine)
               {
               generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, nextSequenceLabel, condReg);
               // Fall through to oppositeResultLabel
               }
            else
               {
               generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, oppositeResultLabel, condReg);
               }
            }
         }
      else
         {
         if (profiledClassIsInstanceOf)
            {
            if (nextSequenceIsOutOfLine)
               {
               generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, nextSequenceLabel, condReg);
               // Fall through to oppositeResultLabel
               }
            else
               {
               generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, oppositeResultLabel, condReg);
               }
            }
         else
            {
            if (nextSequenceIsOutOfLine)
               {
               generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, condReg);
               generateLabelInstruction(cg, TR::InstOpCode::b, node, nextSequenceLabel);
               }
            else
               {
               generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, condReg);
               }
            }
         }
      }
   else
      {
      if (nextSequenceIsOutOfLine)
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, nextSequenceLabel, condReg);
      else
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, resultRegInitialValue == true ? doneLabel : oppositeResultLabel, condReg);
      }
   }

static
void genInstanceOfOrCheckCastClassEqualityTest(TR::Node *node, TR::Register *condReg, TR::Register *instanceClassReg, TR::Register *castClassReg, TR::CodeGenerator *cg)
   {
   generateTrg1Src2Instruction(cg, TR::InstOpCode::Op_cmpl, node, condReg, instanceClassReg, castClassReg);

   // At this point condReg[eq] will be set if the cast class matches the instance class. Caller is responsible for acting on the result.
   }

static
void genInstanceOfOrCheckCastSuperClassTest(TR::Node *node, TR::Register *condReg, TR::Register *instanceClassReg, TR::Register *castClassReg, int32_t castClassDepth, TR::LabelSymbol *falseLabel, TR_PPCScratchRegisterManager *srm, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   // Compare the instance class depth to the cast class depth. If the instance class depth is less than or equal to
   // to the cast class depth then the cast class cannot be a superclass of the instance class.
   //
   TR::Register *instanceClassDepthReg = srm->findOrCreateScratchRegister();
   TR::Register *castClassDepthReg = NULL;
   generateTrg1MemInstruction(cg, TR::InstOpCode::Op_load, node, instanceClassDepthReg,
                              new (cg->trHeapMemory()) TR::MemoryReference(instanceClassReg, offsetof(J9Class, classDepthAndFlags), TR::Compiler->om.sizeofReferenceAddress(), cg));
   static_assert(J9_JAVA_CLASS_DEPTH_MASK < UINT_MAX, "Class depth is not containable in 32 bits");
   generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, instanceClassDepthReg, instanceClassDepthReg, 0, J9_JAVA_CLASS_DEPTH_MASK);
   if (castClassDepth <= UPPER_IMMED && castClassDepth >= LOWER_IMMED)
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, instanceClassDepthReg, castClassDepth);
   else
      {
      castClassDepthReg = srm->findOrCreateScratchRegister();
      loadConstant(cg, node, castClassDepth, castClassDepthReg);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp4, node, condReg, instanceClassDepthReg, castClassDepthReg);
      }
   srm->reclaimScratchRegister(instanceClassDepthReg);

   // At this point condReg[gt] will be set if the instance class depth is greater than the cast class depth.
   //
   generateConditionalBranchInstruction(cg, TR::InstOpCode::ble, node, falseLabel, condReg);

   // Load the superclasses array of the instance class and check if the superclass that appears at the depth of the cast class is in fact the cast class.
   // If not, the instance class and cast class are not in the same hierarchy.
   //
   TR::Register *instanceClassSuperClassesArrayReg = srm->findOrCreateScratchRegister();
   TR::Register *instanceClassSuperClassReg = srm->findOrCreateScratchRegister();
   generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, instanceClassSuperClassesArrayReg,
                              new (cg->trHeapMemory()) TR::MemoryReference(instanceClassReg, offsetof(J9Class, superclasses), TR::Compiler->om.sizeofReferenceAddress(), cg));
   int32_t castClassDepthOffset = castClassDepth * TR::Compiler->om.sizeofReferenceAddress();
   if (castClassDepthOffset <= UPPER_IMMED && castClassDepthOffset >= LOWER_IMMED)
      generateTrg1MemInstruction(cg, TR::InstOpCode::Op_load, node, instanceClassSuperClassReg,
                                 new (cg->trHeapMemory()) TR::MemoryReference(instanceClassSuperClassesArrayReg, castClassDepthOffset, TR::Compiler->om.sizeofReferenceAddress(), cg));
   else
      {
      if (!castClassDepthReg)
         {
         castClassDepthReg = srm->findOrCreateScratchRegister();
         generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, castClassDepthReg, HI_VALUE(castClassDepthOffset));
         generateTrg1MemInstruction(cg, TR::InstOpCode::Op_load, node, instanceClassSuperClassReg,
                                    new (cg->trHeapMemory()) TR::MemoryReference(instanceClassSuperClassesArrayReg, LO_VALUE(castClassDepthOffset), TR::Compiler->om.sizeofReferenceAddress(), cg));
         }
      else
         {
         if (TR::Compiler->target.is64Bit())
            generateShiftLeftImmediateLong(cg, node, castClassDepthReg, castClassDepthReg, 3);
         else
            generateShiftLeftImmediate(cg, node, castClassDepthReg, castClassDepthReg, 2);
         generateTrg1MemInstruction(cg, TR::InstOpCode::Op_loadx, node, instanceClassSuperClassReg, new (cg->trHeapMemory()) TR::MemoryReference(instanceClassSuperClassesArrayReg, castClassDepthReg, TR::Compiler->om.sizeofReferenceAddress(), cg));
         }
      }
   generateTrg1Src2Instruction(cg, TR::InstOpCode::Op_cmpl, node, condReg, instanceClassSuperClassReg, castClassReg);
   if (castClassDepthReg)
      srm->reclaimScratchRegister(castClassDepthReg);
   srm->reclaimScratchRegister(instanceClassSuperClassesArrayReg);
   srm->reclaimScratchRegister(instanceClassSuperClassReg);

   // At this point condReg[eq] will be set if the cast class is a superclass of the instance class. Caller is responsible for acting on the result.
   }

static
void genInstanceOfOrCheckCastArbitraryClassTest(TR::Node *node, TR::Register *condReg, TR::Register *instanceClassReg, TR_OpaqueClassBlock *arbitraryClass, TR_PPCScratchRegisterManager *srm, TR::CodeGenerator *cg)
   {
   TR::Register *arbitraryClassReg = srm->findOrCreateScratchRegister();
   loadAddressConstant(cg, node, (intptr_t)arbitraryClass, arbitraryClassReg);
   genInstanceOfOrCheckCastClassEqualityTest(node, condReg, instanceClassReg, arbitraryClassReg, cg);
   srm->reclaimScratchRegister(arbitraryClassReg);

   // At this point condReg[eq] will be set if the cast class matches the arbitrary class. Caller is responsible for acting on the result.
   }

static
void genInstanceOfCastClassCacheTest(TR::Node *node, TR::Register *condReg, TR::Register *instanceClassReg, TR::Register *castClassReg, TR_PPCScratchRegisterManager *srm, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   // Compare the cast class against the cache on the instance class.
   // If they are the same the cast is successful.
   // If not it's either because the cache class does not match the cast class, or it does match except the cache class has the low bit set, which means the cast is not successful.
   //
   TR::Register *castClassCacheReg = srm->findOrCreateScratchRegister();
   generateTrg1MemInstruction(cg, TR::InstOpCode::Op_load, node, castClassCacheReg,
                              new (cg->trHeapMemory()) TR::MemoryReference(instanceClassReg, offsetof(J9Class, castClassCache), TR::Compiler->om.sizeofReferenceAddress(), cg));
   generateTrg1Src2Instruction(cg, TR::InstOpCode::XOR, node, castClassCacheReg, castClassCacheReg, castClassReg);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::Op_cmpli, node, condReg, castClassCacheReg, 1);

   // At this point condReg[lt] will be set if the cast is successful, condReg[eq] will be set if the cast is unsuccessful, and condReg[gt] will be set if the cache class did not match the cast class.
   // Caller is responsible for acting on the result.
   }

static
void genCheckCastCastClassCacheTest(TR::Node *node, TR::Register *condReg, TR::Register *instanceClassReg, TR::Register *castClassReg, TR_PPCScratchRegisterManager *srm, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   // Compare the cast class against the cache on the instance class.
   // If they are the same the cast is successful.
   // If not it's either because the cache class does not match the cast class, or it does match except the cache class has the low bit set, which means the cast is invalid.
   //
   TR::Register *castClassCacheReg = srm->findOrCreateScratchRegister();
   generateTrg1MemInstruction(cg, TR::InstOpCode::Op_load, node, castClassCacheReg,
                              new (cg->trHeapMemory()) TR::MemoryReference(instanceClassReg, offsetof(J9Class, castClassCache), TR::Compiler->om.sizeofReferenceAddress(), cg));
   generateTrg1Src2Instruction(cg, TR::InstOpCode::Op_cmpl, node, condReg, castClassReg, castClassCacheReg);
   srm->reclaimScratchRegister(castClassCacheReg);

   // At this point condReg[eq] will be set if the cast is successful. Caller is responsible for acting on the result.
   }

static
void genInstanceOfOrCheckCastObjectArrayTest(TR::Node *node, TR::Register *cr0Reg, TR::Register *instanceClassReg, TR::LabelSymbol *falseLabel, TR_PPCScratchRegisterManager *srm, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   // Load the object ROM class and test the modifiers to see if this is an array.
   //
   TR::Register *scratchReg = srm->findOrCreateScratchRegister();
   generateTrg1MemInstruction(cg, TR::InstOpCode::Op_load, node, scratchReg, new (cg->trHeapMemory()) TR::MemoryReference(instanceClassReg, offsetof(J9Class, romClass), TR::Compiler->om.sizeofReferenceAddress(), cg));
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, scratchReg, new (cg->trHeapMemory()) TR::MemoryReference(scratchReg, offsetof(J9ROMClass, modifiers), 4, cg));
   if (J9AccClassArray <= UPPER_IMMED && J9AccClassArray >= LOWER_IMMED)
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, scratchReg, scratchReg, cr0Reg, J9AccClassArray);
   else
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andis_r, node, scratchReg, scratchReg, cr0Reg, J9AccClassArray >> 16);

   // At this point cr0[eq] will be set if this is not an array.
   //
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, falseLabel, cr0Reg);

   // If it's an array, load the component ROM class and test the modifiers to see if this is a primitive array.
   //
   generateTrg1MemInstruction(cg, TR::InstOpCode::Op_load, node, scratchReg, new (cg->trHeapMemory()) TR::MemoryReference(instanceClassReg, offsetof(J9ArrayClass, componentType), TR::Compiler->om.sizeofReferenceAddress(), cg));
   generateTrg1MemInstruction(cg, TR::InstOpCode::Op_load, node, scratchReg, new (cg->trHeapMemory()) TR::MemoryReference(scratchReg, offsetof(J9Class, romClass), TR::Compiler->om.sizeofReferenceAddress(), cg));
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, scratchReg, new (cg->trHeapMemory()) TR::MemoryReference(scratchReg, offsetof(J9ROMClass, modifiers), 4, cg));
   if (J9AccClassInternalPrimitiveType <= UPPER_IMMED && J9AccClassInternalPrimitiveType >= LOWER_IMMED)
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, scratchReg, scratchReg, cr0Reg, J9AccClassInternalPrimitiveType);
   else
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andis_r, node, scratchReg, scratchReg, cr0Reg, J9AccClassInternalPrimitiveType >> 16);
   srm->reclaimScratchRegister(scratchReg);

   // At this point cr0[eq] will be set if this is not a primitive array. Caller is responsible acting on the result.
   }

static
void genInstanceOfOrCheckCastHelperCall(TR::Node *node, TR::Register *objectReg, TR::Register *castClassReg, TR::Register *resultReg, TR::CodeGenerator *cg)
   {
   TR_ASSERT((resultReg != NULL) == (node->getOpCodeValue() == TR::instanceof),
             "Expecting result reg for instanceof but not checkcast");

   TR::Register *helperReturnReg = resultReg ? cg->allocateRegister() : NULL;

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 4, cg->trMemory());

   deps->addPreCondition(castClassReg, TR::RealRegister::gr3);
   deps->addPostCondition(resultReg ? helperReturnReg : castClassReg, TR::RealRegister::gr3);
   deps->addPostCondition(objectReg, TR::RealRegister::gr4);
   TR::Register *dummyReg, *dummyRegCCR;
   // gr11 is killed by the trampoline.
   deps->addPostCondition(dummyReg = cg->allocateRegister(), TR::RealRegister::gr11);
   dummyReg->setPlaceholderReg();
   // cr0 is not preserved by the helper.
   deps->addPostCondition(dummyRegCCR = cg->allocateRegister(TR_CCR), TR::RealRegister::cr0);
   dummyRegCCR->setPlaceholderReg();

   TR::Instruction *gcPoint = generateDepImmSymInstruction(cg, TR::InstOpCode::bl, node, (uintptrj_t)node->getSymbolReference()->getMethodAddress(), deps, node->getSymbolReference());
   gcPoint->PPCNeedsGCMap(0xFFFFFFFF);

   if (resultReg)
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, resultReg, helperReturnReg);

   cg->stopUsingRegister(dummyReg);
   cg->stopUsingRegister(dummyRegCCR);

   cg->machine()->setLinkRegisterKilled(true);

   TR::Register *nodeRegs[3] = {objectReg, castClassReg, resultReg};
   deps->stopUsingDepRegs(cg, 3, nodeRegs);
   }

TR::Register *J9::Power::TreeEvaluator::VMcheckcastEvaluator2(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation                      *comp = cg->comp();
   TR_J9VMBase                          *fej9 = reinterpret_cast<TR_J9VMBase *>(comp->fe());
   TR_OpaqueClassBlock                  *compileTimeGuessClass;
   InstanceOfOrCheckCastProfiledClasses  profiledClassesList[1];
   InstanceOfOrCheckCastSequences        sequences[InstanceOfOrCheckCastMaxSequences];
   uint32_t                              numberOfProfiledClass = 0;
   float                                 topClassProbability = 0.0;
   bool                                  topClassWasCastClass = false;
   uint32_t                              numSequencesRemaining = calculateInstanceOfOrCheckCastSequences(node, sequences, &compileTimeGuessClass, cg, profiledClassesList, &numberOfProfiledClass, 1, &topClassProbability, &topClassWasCastClass);

   TR::Node        *objectNode = node->getFirstChild();
   TR::Node        *castClassNode = node->getSecondChild();
   TR::Register    *objectReg = cg->evaluate(objectNode);
   TR::Register    *castClassReg = NULL;

   TR::LabelSymbol *depLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *helperReturnLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *nextSequenceLabel = generateLabelSymbol(cg);

   TR::Instruction *gcPoint;

   TR_PPCScratchRegisterManager *srm = cg->generateScratchRegisterManager();
   TR::Register                 *objectClassReg = NULL;
   TR::Register                 *cr0Reg = cg->allocateRegister(TR_CCR);

   InstanceOfOrCheckCastSequences *iter = &sequences[0];
   while (numSequencesRemaining > 1)
      {
      switch (*iter)
         {
         case EvaluateCastClass:
            TR_ASSERT(!castClassReg, "Cast class already evaluated");
            castClassReg = cg->evaluate(castClassNode);
            break;
         case LoadObjectClass:
            TR_ASSERT(!objectClassReg, "Object class already loaded");
            objectClassReg = srm->findOrCreateScratchRegister();
            generateTrg1MemInstruction(cg,
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
                                       TR::InstOpCode::lwz,
#else
                                       TR::InstOpCode::Op_load,
#endif
                                       node, objectClassReg, new (cg->trHeapMemory()) TR::MemoryReference(objectReg, TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceField(), cg));
	    TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, objectClassReg);
            break;
         case NullTest:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting NullTest\n", node->getOpCode().getName());
            TR_ASSERT(!objectNode->isNonNull(), "Object is known to be non-null, no need for a null test");
            if (node->getOpCodeValue() == TR::checkcastAndNULLCHK)
               {
               TR::Node *nullChkInfo = comp->findNullChkInfo(node);
               gcPoint = generateNullTestInstructions(cg, objectReg, nullChkInfo, !cg->getHasResumableTrapHandler());
               gcPoint->PPCNeedsGCMap(0x0);
               }
            else
               {
               genNullTest(node, objectReg, cr0Reg, cg);
               }
            break;
         case GoToTrue:
            TR_ASSERT(false, "Doesn't make sense, GoToTrue should not be part of multiple sequences");
            break;
         case GoToFalse:
            TR_ASSERT(false, "Doesn't make sense, GoToFalse should be the terminal sequence");
            break;
         case ClassEqualityTest:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting ClassEqualityTest\n", node->getOpCode().getName());
            genInstanceOfOrCheckCastClassEqualityTest(node, cr0Reg, objectClassReg, castClassReg, cg);
            break;
         case SuperClassTest:
            {
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting SuperClassTest\n", node->getOpCode().getName());
            int32_t castClassDepth = castClassNode->getSymbolReference()->classDepth(comp);
            genInstanceOfOrCheckCastSuperClassTest(node, cr0Reg, objectClassReg, castClassReg, castClassDepth, nextSequenceLabel, srm, cg);
            break;
            }
         case ProfiledClassTest:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting ProfiledClassTest\n", node->getOpCode().getName());
            genInstanceOfOrCheckCastArbitraryClassTest(node, cr0Reg, objectClassReg, profiledClassesList[0].profiledClass, srm, cg);
            break;
         case CompileTimeGuessClassTest:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting CompileTimeGuessClassTest\n", node->getOpCode().getName());
            genInstanceOfOrCheckCastArbitraryClassTest(node, cr0Reg, objectClassReg, compileTimeGuessClass, srm, cg);
            break;
         case CastClassCacheTest:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting CastClassCacheTest\n", node->getOpCode().getName());
            genCheckCastCastClassCacheTest(node, cr0Reg, objectClassReg, castClassReg, srm, cg);
            break;
         case ArrayOfJavaLangObjectTest:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting ArrayOfJavaLangObjectTest\n", node->getOpCode().getName());
            genInstanceOfOrCheckCastObjectArrayTest(node, cr0Reg, objectClassReg, nextSequenceLabel, srm, cg);
            break;
         case HelperCall:
            TR_ASSERT(false, "Doesn't make sense, HelperCall should be the terminal sequence");
            break;
         }

      genCheckCastTransitionToNextSequence(node, iter, nextSequenceLabel, doneLabel, cr0Reg, cg);

      --numSequencesRemaining;
      ++iter;

      if (*iter != HelperCall && *iter != GoToFalse)
         {
         generateLabelInstruction(cg, TR::InstOpCode::label, node, nextSequenceLabel);
         nextSequenceLabel = generateLabelSymbol(cg);
         }
      }

   if (objectClassReg)
      srm->reclaimScratchRegister(objectClassReg);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);

   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfOrCheckCast/%s/fastPath",
                                                   node->getOpCode().getName()),
                            *srm);
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfOrCheckCast.perMethod/%s/(%s)/%d/%d/fastPath",
                                                   node->getOpCode().getName(), comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
                            *srm);

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3 + srm->numAvailableRegisters(), cg->trMemory());
   srm->addScratchRegistersToDependencyList(deps, true);
   deps->addPostCondition(cr0Reg, TR::RealRegister::cr0);
   deps->addPostCondition(objectReg, TR::RealRegister::NoReg);
   deps->getPostConditions()->getRegisterDependency(deps->getAddCursorForPost() - 1)->setExcludeGPR0();
   if (castClassReg)
      {
      deps->addPostCondition(castClassReg, TR::RealRegister::NoReg);
      deps->getPostConditions()->getRegisterDependency(deps->getAddCursorForPost() - 1)->setExcludeGPR0();
      }

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, depLabel, deps);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, helperReturnLabel);

   if (numSequencesRemaining > 0 && *iter != GoToTrue)
      {
      TR_ASSERT(*iter == HelperCall || *iter == GoToFalse, "Expecting helper call or fail here");
      bool helperCallForFailure = *iter != HelperCall;
      if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting HelperCall%s\n", node->getOpCode().getName(), helperCallForFailure ? " for failure" : "");
      TR_PPCOutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_PPCOutOfLineCodeSection(node, TR::call, NULL, nextSequenceLabel, helperReturnLabel, cg);
      cg->generateDebugCounter(nextSequenceLabel->getInstruction(),
                               TR::DebugCounter::debugCounterName(comp, "instanceOfOrCheckCast/%s/%s",
                                                                  node->getOpCode().getName(),
                                                                  helperCallForFailure ? "fail" : "helperCall"));
      cg->generateDebugCounter(nextSequenceLabel->getInstruction(),
                               TR::DebugCounter::debugCounterName(comp, "instanceOfOrCheckCast.perMethod/%s/(%s)/%d/%d/%s",
                                                                  node->getOpCode().getName(), comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex(),
                                                                  helperCallForFailure ? "fail" : "helperCall"));

      cg->getPPCOutOfLineCodeSectionList().push_front(outlinedHelperCall);
      }

   deps->stopUsingDepRegs(cg, objectReg, castClassReg);
   cg->decReferenceCount(objectNode);
   cg->decReferenceCount(castClassNode);

   return NULL;
   }

TR::Register *J9::Power::TreeEvaluator::VMinstanceOfEvaluator2(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation                      *comp = cg->comp();
   TR_OpaqueClassBlock                  *compileTimeGuessClass;
   bool                                  profiledClassIsInstanceOf;
   InstanceOfOrCheckCastProfiledClasses  profiledClassesList[1];
   InstanceOfOrCheckCastSequences        sequences[InstanceOfOrCheckCastMaxSequences];
   bool                                  topClassWasCastClass = false;
   float                                 topClassProbability = 0.0;
   uint32_t                              numberOfProfiledClass;
   uint32_t                              numSequencesRemaining = calculateInstanceOfOrCheckCastSequences(node, sequences, &compileTimeGuessClass, cg, profiledClassesList, &numberOfProfiledClass, 1, &topClassProbability, &topClassWasCastClass);

   TR::Node                       *objectNode = node->getFirstChild();
   TR::Node                       *castClassNode = node->getSecondChild();
   TR::Register                   *objectReg = cg->evaluate(objectNode);
   TR::Register                   *castClassReg = NULL;
   TR::Register                   *resultReg = cg->allocateRegister();

   TR::LabelSymbol *depLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *helperReturnLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *nextSequenceLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *oppositeResultLabel = generateLabelSymbol(cg);

   TR::Instruction *gcPoint;

   TR_PPCScratchRegisterManager *srm = cg->generateScratchRegisterManager();
   TR::Register                 *objectClassReg = NULL;
   TR::Register                 *cr0Reg = cg->allocateRegister(TR_CCR);

   static bool initialResult = feGetEnv("TR_instanceOfInitialValue") != NULL;//true;
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, initialResult);

   InstanceOfOrCheckCastSequences *iter = &sequences[0];
   while (numSequencesRemaining > 1 || (numSequencesRemaining == 1 && *iter != HelperCall))
      {
      switch (*iter)
         {
         case EvaluateCastClass:
            TR_ASSERT(!castClassReg, "Cast class already evaluated");
            castClassReg = cg->evaluate(castClassNode);
            break;
         case LoadObjectClass:
            TR_ASSERT(!objectClassReg, "Object class already loaded");
            objectClassReg = srm->findOrCreateScratchRegister();
            generateTrg1MemInstruction(cg,
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
                                       TR::InstOpCode::lwz,
#else
                                       TR::InstOpCode::Op_load,
#endif
                                       node, objectClassReg, new (cg->trHeapMemory()) TR::MemoryReference(objectReg, TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceField(), cg));
	    TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, objectClassReg);
            break;
         case NullTest:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting NullTest\n", node->getOpCode().getName());
            TR_ASSERT(!objectNode->isNonNull(), "Object is known to be non-null, no need for a null test");
            if (node->getOpCodeValue() == TR::checkcastAndNULLCHK)
               {
               TR::Node *nullChkInfo = comp->findNullChkInfo(node);
               gcPoint = generateNullTestInstructions(cg, objectReg, nullChkInfo, !cg->getHasResumableTrapHandler());
               gcPoint->PPCNeedsGCMap(0x0);
               }
            else
               {
               genNullTest(node, objectReg, cr0Reg, cg);
               }
            break;
         case GoToTrue:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting GoToTrue\n", node->getOpCode().getName());
            if (initialResult == true)
               generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);
            break;
         case GoToFalse:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting GoToFalse\n", node->getOpCode().getName());
            if (initialResult == false)
               generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);
            break;
         case ClassEqualityTest:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting ClassEqualityTest\n", node->getOpCode().getName());
            genInstanceOfOrCheckCastClassEqualityTest(node, cr0Reg, objectClassReg, castClassReg, cg);
            break;
         case SuperClassTest:
            {
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting SuperClassTest\n", node->getOpCode().getName());
            int32_t castClassDepth = castClassNode->getSymbolReference()->classDepth(comp);
            genInstanceOfOrCheckCastSuperClassTest(node, cr0Reg, objectClassReg, castClassReg, castClassDepth, nextSequenceLabel, srm, cg);
            break;
            }
         case ProfiledClassTest:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting ProfiledClassTest\n", node->getOpCode().getName());
            profiledClassIsInstanceOf = profiledClassesList[0].isProfiledClassInstanceOfCastClass;
            genInstanceOfOrCheckCastArbitraryClassTest(node, cr0Reg, objectClassReg, profiledClassesList[0].profiledClass, srm, cg);
            break;
         case CompileTimeGuessClassTest:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting CompileTimeGuessClassTest\n", node->getOpCode().getName());
            genInstanceOfOrCheckCastArbitraryClassTest(node, cr0Reg, objectClassReg, compileTimeGuessClass, srm, cg);
            break;
         case CastClassCacheTest:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting CastClassCacheTest\n", node->getOpCode().getName());
            genInstanceOfCastClassCacheTest(node, cr0Reg, objectClassReg, castClassReg, srm, cg);
            break;
         case ArrayOfJavaLangObjectTest:
            if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting ArrayOfJavaLangObjectTest\n", node->getOpCode().getName());
            genInstanceOfOrCheckCastObjectArrayTest(node, cr0Reg, objectClassReg, nextSequenceLabel, srm, cg);
            break;
         case DynamicCacheObjectClassTest:
            TR_ASSERT_FATAL(false, "%s: DynamicCacheObjectClassTest is not implemented on P\n", node->getOpCode().getName());
            break;
         case DynamicCacheDynamicCastClassTest: 
            TR_ASSERT_FATAL(false, "%s: DynamicCacheDynamicCastClassTest is not implemented on P\n", node->getOpCode().getName());
            break;
         case HelperCall:
            TR_ASSERT(false, "Doesn't make sense, HelperCall should be the terminal sequence");
            break;
         }

      genInstanceOfTransitionToNextSequence(node, iter, nextSequenceLabel, doneLabel, cr0Reg, resultReg, initialResult, oppositeResultLabel, profiledClassIsInstanceOf, cg);

      --numSequencesRemaining;
      ++iter;

      if (*iter != HelperCall)
         {
         generateLabelInstruction(cg, TR::InstOpCode::label, node, nextSequenceLabel);
         nextSequenceLabel = generateLabelSymbol(cg);
         }
      }

   if (objectClassReg)
      srm->reclaimScratchRegister(objectClassReg);

   if (true)
      {
      generateLabelInstruction(cg, TR::InstOpCode::label, node, oppositeResultLabel);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, !initialResult);
      }

   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);

   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfOrCheckCast/%s/fastPath",
                                                               node->getOpCode().getName()),
                            *srm);
   cg->generateDebugCounter(TR::DebugCounter::debugCounterName(comp, "instanceOfOrCheckCast.perMethod/%s/(%s)/%d/%d/fastPath",
                                                               node->getOpCode().getName(), comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()),
                            *srm);

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4 + srm->numAvailableRegisters(), cg->trMemory());
   srm->addScratchRegistersToDependencyList(deps, true);
   deps->addPostCondition(cr0Reg, TR::RealRegister::cr0);
   deps->addPostCondition(resultReg, TR::RealRegister::NoReg);
   deps->addPostCondition(objectReg, TR::RealRegister::NoReg);
   deps->getPostConditions()->getRegisterDependency(deps->getAddCursorForPost() - 1)->setExcludeGPR0();
   if (castClassReg)
      {
      deps->addPostCondition(castClassReg, TR::RealRegister::NoReg);
      deps->getPostConditions()->getRegisterDependency(deps->getAddCursorForPost() - 1)->setExcludeGPR0();
      }

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, depLabel, deps);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, helperReturnLabel);

   if (numSequencesRemaining > 0)
      {
      if (*iter == HelperCall)
         {
         if (comp->getOption(TR_TraceCG)) traceMsg(comp, "%s: Emitting HelperCall\n", node->getOpCode().getName());
         TR_PPCOutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_PPCOutOfLineCodeSection(node, TR::icall, resultReg, nextSequenceLabel, helperReturnLabel, cg);
         cg->generateDebugCounter(nextSequenceLabel->getInstruction(),
                                  TR::DebugCounter::debugCounterName(comp, "instanceOfOrCheckCast/%s/helperCall",
                                                                     node->getOpCode().getName()));
         cg->generateDebugCounter(nextSequenceLabel->getInstruction(),
                                  TR::DebugCounter::debugCounterName(comp, "instanceOfOrCheckCast.perMethod/%s/(%s)/%d/%d/helperCall",
                                                                     node->getOpCode().getName(), comp->signature(), node->getByteCodeInfo().getCallerIndex(), node->getByteCodeInfo().getByteCodeIndex()));

         cg->getPPCOutOfLineCodeSectionList().push_front(outlinedHelperCall);
         }
      }

   // Stop using every reg in the deps except these ones.
   //
   TR::Register *nodeRegs[3] = {objectReg, castClassReg, resultReg};
   deps->stopUsingDepRegs(cg, 3, nodeRegs);

   cg->decReferenceCount(objectNode);
   cg->decReferenceCount(castClassNode);

   node->setRegister(resultReg);

   return resultReg;
   }

// ----------------------------------------------------------------------------

// only need a helper call if the class is not super and not final, otherwise
// it can be determined without a call-out
static bool needHelperCall(bool testCastClassIsSuper, bool isFinalClass)
   {
   return !testCastClassIsSuper && !isFinalClass;
   }

static bool needTestCache(bool cachingEnabled, bool needsHelperCall, bool superClassTest, TR_OpaqueClassBlock* castClassAddr, uint8_t num_PICS)
   {
   return cachingEnabled && needsHelperCall && !superClassTest && castClassAddr && num_PICS;
   }

TR::Register *J9::Power::TreeEvaluator::VMcheckcastEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   static bool newCheckCast = feGetEnv("TR_oldCheckCast") == NULL;
   if (newCheckCast)
      return VMcheckcastEvaluator2(node, cg);

   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   TR::Register *objReg, *castClassReg, *objClassReg, *scratch1Reg, *scratch2Reg, *cndReg;
   TR::LabelSymbol *doneLabel, *callLabel, *callForInlineLabel, *nextTestLabel;
   TR::SymbolReference *castClassSymRef;
   TR::StaticSymbol *castClassSym;
   TR::Node *objNode, *castClassNode;
   TR_OpaqueClassBlock *castClassAddr;
   TR_OpaqueClassBlock *guessClassArray[NUM_PICS];
   TR::RegisterDependencyConditions *conditions = createConditionsAndPopulateVSXDeps(cg, 7);
   TR::Instruction *gcPoint;
   TR_Debug *debugObj = cg->getDebug();
   objNode = node->getFirstChild();
   castClassNode = node->getSecondChild();
   castClassSymRef = castClassNode->getSymbolReference();
   castClassAddr = TR::TreeEvaluator::getCastClassAddress(castClassNode);
   uint8_t num_PICS = TR::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastInfo(cg, node, guessClassArray);
   bool foundGuessClass = false;
   TR_OpaqueClassBlock *tmpClassAddr, *validGuessClassArray[NUM_PICS];
   uint8_t numberOfValidGuessClasses = 0;
   TR::Compilation * comp = cg->comp();
   // Defect# 90652 (PPC), 92901 (390)
   // if the test fails for the checkcast, do not generate inline check for guess value
   if (guessClassArray && castClassAddr)
      {
      for (uint8_t i = 0; i < num_PICS; i++)
         {
         tmpClassAddr = guessClassArray[i];
         if (instanceOfOrCheckCast((J9Class*) tmpClassAddr, (J9Class*) castClassAddr))
            {
            foundGuessClass = true;
            validGuessClassArray[numberOfValidGuessClasses] = tmpClassAddr;
            numberOfValidGuessClasses++;
            }
         }
      }

   bool testEqualClass = instanceOfOrCheckCastNeedEqualityTest(node, cg);
   bool testCastClassIsSuper = instanceOfOrCheckCastNeedSuperTest(node, cg);
   bool isFinalClass = (castClassSymRef == NULL) ? false : castClassSymRef->isNonArrayFinal(comp);
   bool needsHelperCall = needHelperCall(testCastClassIsSuper, isFinalClass);
   bool testCache = needTestCache(!comp->getOption(TR_DisableInlineCheckCast), needsHelperCall, testCastClassIsSuper, castClassAddr, numberOfValidGuessClasses);
   bool needsNullTest = !objNode->isNonNull();

   bool isCheckcastAndNullChk = (node->getOpCodeValue() == TR::checkcastAndNULLCHK);

   castClassReg = cg->evaluate(castClassNode);

   objReg = cg->evaluate(objNode);

   if (needsNullTest && isCheckcastAndNullChk)
      {
      // get the bytecode info of the
      // NULLCHK that was compacted
      TR::Node *nullChkInfo = comp->findNullChkInfo(node);
      if (cg->getHasResumableTrapHandler())
         gcPoint = generateNullTestInstructions(cg, objReg, nullChkInfo);
      else
         // a helper is needed
         gcPoint = generateNullTestInstructions(cg, objReg, nullChkInfo, true);
      gcPoint->PPCNeedsGCMap(0xFFFFFFFF);
      }

   objClassReg = cg->allocateRegister();

   // Assuming: CR is preserved by the call --- OTI commitment
   addDependency(conditions, objReg, TR::RealRegister::gr4, TR_GPR, cg);
   addDependency(conditions, castClassReg, TR::RealRegister::gr3, TR_GPR, cg);
   addDependency(conditions, objClassReg, TR::RealRegister::gr11, TR_GPR, cg);

   // We should be able to relax the mask to 0x00000000 if GC only happens under exception.
   if (!testEqualClass && !testCache && !testCastClassIsSuper)
      {
      gcPoint = generateDepImmSymInstruction(cg, TR::InstOpCode::bl, node, (uintptrj_t) node->getSymbolReference()->getMethodAddress(), conditions, node->getSymbolReference());
      gcPoint->PPCNeedsGCMap(0xFFFFFFFF);
      cg->stopUsingRegister(objClassReg);
      cg->decReferenceCount(objNode);
      cg->decReferenceCount(castClassNode);
      cg->machine()->setLinkRegisterKilled(true);
      return NULL;
      }

   doneLabel = generateLabelSymbol(cg);
   callLabel = generateLabelSymbol(cg);

   TR::LabelSymbol *startOOLLabel, *doneOOLLabel, *helperReturnOOLLabel = NULL;
   TR_PPCOutOfLineCodeSection *outlinedSlowPath = NULL;
   TR::RegisterDependencyConditions *condOOL = NULL;

   cndReg = cg->allocateRegister(TR_CCR);
   addDependency(conditions, cndReg, TR::RealRegister::NoReg, TR_CCR, cg);

   // From this point forward we know that at least one of testEqualClass, testCache or testCastClassIsSuper is true

   // Ideally, value profiling on checkCasts is the way to detect if we should skip nullTest. For now, we pick
   // well-known ones to not skip.
   bool dontSkipNullTest = !strncmp(comp->signature(), "org/apache/xerces/dom/ParentNode.internalInsertBefore", 53);

   bool skipNullTest = ((TR::Compiler->target.is32Bit() && TR::Compiler->target.isAIX()) && (!testCastClassIsSuper || castClassSymRef->classDepth(comp) < (AIX_NULL_ZERO_AREA_SIZE / TR::Compiler->om.sizeofReferenceAddress()))
         && offsetof(J9Class, classDepthAndFlags) < AIX_NULL_ZERO_AREA_SIZE && offsetof(J9Class, superclasses) < AIX_NULL_ZERO_AREA_SIZE) && !dontSkipNullTest;
   if (needsNullTest && !isCheckcastAndNullChk && !skipNullTest)
      {
      // objClassReg is used as a scratch register here
      genNullTest(node, objReg, cndReg, objClassReg, NULL, cg);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, cndReg);
      }

#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
   // read only 32 bits
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, objClassReg,
         new (cg->trHeapMemory()) TR::MemoryReference(objReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), 4, cg));
#else
   generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, objClassReg,
         new (cg->trHeapMemory()) TR::MemoryReference(objReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceAddress(), cg));
#endif
   TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, objClassReg);

   if (testEqualClass)
      {
      generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, cndReg, objClassReg, castClassReg);
      if (testCache || testCastClassIsSuper)
         {
         // Direct the code to OOL section, so flip the branch when OOL enabled.
         static bool disableOOLcheckcast = (feGetEnv("TR_DisableOOLcheckcast") != NULL);
         if (!comp->getOption(TR_DisableOOL) && !disableOOLcheckcast)
            {
            startOOLLabel = generateLabelSymbol(cg);
            doneOOLLabel = generateLabelSymbol(cg);
            helperReturnOOLLabel = generateLabelSymbol(cg);

            condOOL = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(3, 3, cg->trMemory());
            addDependency(condOOL, objReg, TR::RealRegister::gr4, TR_GPR, cg);
            addDependency(condOOL, castClassReg, TR::RealRegister::NoReg, TR_GPR, cg);
            addDependency(condOOL, objClassReg, TR::RealRegister::NoReg, TR_GPR, cg);

            TR::Instruction *OOLBranchInstr = generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, startOOLLabel, cndReg);
            if (debugObj)
               debugObj->addInstructionComment(OOLBranchInstr, "Branch to OOL checkCast sequence");
            outlinedSlowPath = new (cg->trHeapMemory()) TR_PPCOutOfLineCodeSection(startOOLLabel, doneOOLLabel, cg);
            cg->getPPCOutOfLineCodeSectionList().push_front(outlinedSlowPath);
            outlinedSlowPath->swapInstructionListsWithCompilation();
            TR::Instruction *OOLLabelInstr = generateLabelInstruction(cg, TR::InstOpCode::label, node, startOOLLabel);
            if (debugObj)
               debugObj->addInstructionComment(OOLLabelInstr, "Denotes start of OOL checkCast sequence");
            // XXX: Temporary fix, OOL instruction stream does not pick up live locals or monitors correctly.
            TR_ASSERT(!OOLLabelInstr->getLiveLocals() && !OOLLabelInstr->getLiveMonitors(), "Expecting first OOL instruction to not have live locals/monitors info");
            OOLLabelInstr->setLiveLocals(OOLBranchInstr->getLiveLocals());
            OOLLabelInstr->setLiveMonitors(OOLBranchInstr->getLiveMonitors());
            }
         else
            generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, cndReg);
         }
      else
         {
         gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, callLabel, cndReg);
         }
      }
   TR::Snippet *snippet = NULL;
   if (outlinedSlowPath)
      {
      snippet = new (cg->trHeapMemory()) TR::PPCHelperCallSnippet(cg, node, callLabel, node->getSymbolReference(), helperReturnOOLLabel);
      }
   else
      {
      snippet = new (cg->trHeapMemory()) TR::PPCHelperCallSnippet(cg, node, callLabel, node->getSymbolReference(), doneLabel);
      }
   snippet->gcMap().setGCRegisterMask(0xFFFFFFFF);
   cg->addSnippet(snippet);

   if (testCache || testCastClassIsSuper)
      {
      scratch1Reg = cg->allocateRegister();
      // Only 64-bit target uses scratch2Reg on testCache
      scratch2Reg = cg->allocateRegister();
      addDependency(conditions, scratch1Reg, TR::RealRegister::NoReg, TR_GPR, cg);
      conditions->getPostConditions()->getRegisterDependency(4)->setExcludeGPR0();
      addDependency(conditions, scratch2Reg, TR::RealRegister::NoReg, TR_GPR, cg);
      // This is necessary only for testCastClassIsSuper
      if (testCastClassIsSuper)
         {
         conditions->getPostConditions()->getRegisterDependency(5)->setExcludeGPR0();
         }
      }

   if (testCache)
      {
      // The cached value could have been from a previously successful checkcast or instanceof.
      // An answer of 0 in the low order bit indicates 'success' (the cast or instanceof was successful).
      // An answer of 1 in the lower order bit indicates 'failure' (the cast would have thrown an exception, instanceof would have been unsuccessful)
      // Because of this, we can just do a simple load and compare of the 2 class pointers. If it succeeds, the low order bit
      // must be off (success) from a previous checkcast or instanceof. If the low order bit is on, it is guaranteed not to
      // compare and we will take the slow path.
      // Generate inline test for checkcast

      callForInlineLabel = generateLabelSymbol(cg);
      if (outlinedSlowPath)
         {
         genInlineTest(node, castClassAddr, validGuessClassArray, numberOfValidGuessClasses, objClassReg, NULL, cndReg, scratch1Reg, scratch2Reg, true, false, helperReturnOOLLabel,
               helperReturnOOLLabel, helperReturnOOLLabel, callForInlineLabel, testCastClassIsSuper, NULL, cg);
         }
      else
         {
         genInlineTest(node, castClassAddr, validGuessClassArray, numberOfValidGuessClasses, objClassReg, NULL, cndReg, scratch1Reg, scratch2Reg, true, false, doneLabel, doneLabel,
               doneLabel, callForInlineLabel, testCastClassIsSuper, NULL, cg);
         }
      }

   if (testCastClassIsSuper)
      {
      int32_t castClassDepth = castClassSymRef->classDepth(comp);
      genTestIsSuper(node, objClassReg, castClassReg, cndReg, scratch1Reg, scratch2Reg, castClassDepth, callLabel, NULL, cg);
      if (outlinedSlowPath)
         {
         gcPoint = generateDepConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, callLabel, cndReg, conditions);
         }
      else
         {
         gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, callLabel, cndReg);
         }
      }

   // Just fall through

   if (testCache)
      {
      // This is used for InterfaceCastSnippet to call the helper
      // This solves two problems: 1- We set the GC mask here
      //                           2- There is no guarantee in which order the snippets are created, so it is more difficult for InterfaceCastSnippet
      //                              to know the location of the helper call snippet when it is created. Though a relocation can be used for such case.
      generateLabelInstruction(cg, TR::InstOpCode::label, node, callForInlineLabel);
      if (outlinedSlowPath)
         {
         gcPoint = generateDepLabelInstruction(cg, TR::InstOpCode::b, node, callLabel, conditions);
         }
      else
         {
         gcPoint = generateLabelInstruction(cg, TR::InstOpCode::b, node, callLabel);
         }
      }

   if (outlinedSlowPath)
      {
      // Return label from VM Helper call back to OOL sequence
      // We can not branch directly back from VM Helper to main line because
      // there might be reg spills in the rest of the OOL sequence, these code need to be executed.
      TR::Instruction * temp = generateDepLabelInstruction(cg, TR::InstOpCode::label, node, helperReturnOOLLabel, conditions);
      if (debugObj)
         {
         debugObj->addInstructionComment(temp, "OOL checkCast VMHelper return label");
         }

      temp = generateLabelInstruction(cg, TR::InstOpCode::b, node, doneOOLLabel);
      cg->stopUsingRegister(scratch1Reg);
      cg->stopUsingRegister(scratch2Reg);

      if (debugObj)
         debugObj->addInstructionComment(temp, "Denotes end of OOL checkCast sequence: return to mainline");

      // Done using OOL with manual code generation
      outlinedSlowPath->swapInstructionListsWithCompilation();

      temp = generateLabelInstruction(cg, TR::InstOpCode::label, node, doneOOLLabel);
      if (debugObj)
         debugObj->addInstructionComment(temp, "OOL checkCast return label");
      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, condOOL);
      }
   else
      // Done
      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

   // One of the branches is enough to carry stackMap to the snippet
   gcPoint->PPCNeedsGCMap(0xFFFFFFFF);

   conditions->stopUsingDepRegs(cg, objReg, castClassReg);
   cg->decReferenceCount(objNode);
   cg->decReferenceCount(castClassNode);
   return (NULL);
   }

TR::Register *J9::Power::TreeEvaluator::ifInstanceOfEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::VMifInstanceOfEvaluator(node, cg);
   }


TR::Register *J9::Power::TreeEvaluator::VMifInstanceOfEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *depNode;
   int32_t depIndex;
   int32_t numDep;
   TR::LabelSymbol *branchLabel;
   TR::Node *instanceOfNode;
   TR::Node *valueNode;
   int32_t value;
   TR::LabelSymbol *doneLabel, *res0Label, *res1Label;
   bool branchOn1;
   TR::Register *resultReg;
   TR::ILOpCodes opCode;
   TR::Compilation * comp = cg->comp();
   TR::SymbolReference *castClassSymRef;
   TR::Node *castClassNode;
   TR_OpaqueClassBlock *castClassAddr;
   TR_OpaqueClassBlock *guessClassArray[NUM_PICS];

   depIndex = 0;
   numDep = 0;
   depNode = NULL;
   branchLabel = node->getBranchDestination()->getNode()->getLabel();
   instanceOfNode = node->getFirstChild();
   valueNode = node->getSecondChild();
   value = valueNode->getInt();
   opCode = node->getOpCodeValue();

   castClassNode = instanceOfNode->getSecondChild();
   castClassSymRef = castClassNode->getSymbolReference();
   castClassAddr = TR::TreeEvaluator::getCastClassAddress(castClassNode);
   uint8_t num_PICS = TR::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastInfo(cg, instanceOfNode, guessClassArray);

   bool testEqualClass = instanceOfOrCheckCastNeedEqualityTest(instanceOfNode, cg);
   bool testCastClassIsSuper = instanceOfOrCheckCastNeedSuperTest(instanceOfNode, cg);
   bool isFinalClass = (castClassSymRef == NULL) ? false : castClassSymRef->isNonArrayFinal(comp);
   bool needsHelperCall = needHelperCall(testCastClassIsSuper, isFinalClass);
   bool testCache = needTestCache(!comp->getOption(TR_DisableInlineCheckCast), needsHelperCall, testCastClassIsSuper, castClassAddr, num_PICS);

   if (node->getNumChildren() == 3)
      {
      depNode = node->getChild(2);
      numDep = depNode->getNumChildren();
      }

   // If the result itself is assigned to a global register, we still have to evaluate it
   int32_t needResult = (instanceOfNode->getReferenceCount() > 1);

   if (depNode != NULL)
      {
      int32_t i1;

      if (!needsHelperCall && numberOfRegisterCandidate(cg, depNode, TR_GPR) + 7 > cg->getMaximumNumberOfGPRsAllowedAcrossEdge(node))
         return ((TR::Register *) 1);

      const TR::PPCLinkageProperties& properties = cg->getLinkage()->getProperties();
      TR::Node *objNode = instanceOfNode->getFirstChild();
      for (i1 = 0; i1 < depNode->getNumChildren(); i1++)
         {
         TR::Node *childNode = depNode->getChild(i1);
         int32_t regIndex = cg->getGlobalRegister(childNode->getGlobalRegisterNumber());

         int32_t validHighRegNum = TR::TreeEvaluator::getHighGlobalRegisterNumberIfAny(childNode, cg);

         if (needsHelperCall)
            {
            int32_t highIndex;

            if (validHighRegNum != -1)
               highIndex = cg->getGlobalRegister(validHighRegNum);

            if (!properties.getPreserved((TR::RealRegister::RegNum) regIndex)
                  || (validHighRegNum != -1 && !properties.getPreserved((TR::RealRegister::RegNum) highIndex)))
               return ((TR::Register *) 1);
            }
         else
            {
            if (childNode->getOpCodeValue() == TR::PassThrough)
               childNode = childNode->getFirstChild();
            if ((childNode == objNode || childNode == castClassNode) && regIndex == TR::RealRegister::gr0)
               return ((TR::Register *) 1);
            }
         }
      depIndex = numberOfRegisterCandidate(cg, depNode, TR_GPR) + numberOfRegisterCandidate(cg, depNode, TR_FPR) +
                 numberOfRegisterCandidate(cg, depNode, TR_CCR) + numberOfRegisterCandidate(cg, depNode, TR_VRF) +
	         numberOfRegisterCandidate(cg, depNode, TR_VSX_SCALAR) + numberOfRegisterCandidate(cg, depNode, TR_VSX_VECTOR);
      }

   doneLabel = generateLabelSymbol(cg);

   if ((opCode == TR::ificmpeq && value == 1) || (opCode != TR::ificmpeq && value == 0))
      {
      res0Label = doneLabel;
      res1Label = branchLabel;
      branchOn1 = true;
      }
   else
      {
      res0Label = branchLabel;
      res1Label = doneLabel;
      branchOn1 = false;
      }

   resultReg = TR::TreeEvaluator::VMgenCoreInstanceofEvaluator(instanceOfNode, cg, true, depIndex, numDep, depNode, needResult, needsHelperCall, testEqualClass, testCache, testCastClassIsSuper,
         doneLabel, res0Label, res1Label, branchOn1);

   if (resultReg != instanceOfNode->getRegister())
      instanceOfNode->setRegister(resultReg);
   cg->decReferenceCount(instanceOfNode);
   cg->decReferenceCount(valueNode);

   node->setRegister(NULL);
   return NULL;
   }

TR::Register *J9::Power::TreeEvaluator::VMinstanceOfEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   static bool newInstanceOf = feGetEnv("TR_oldInstanceOf") == NULL;
   if (newInstanceOf)
      return VMinstanceOfEvaluator2(node, cg);

   TR::Compilation *comp = cg->comp();
   TR::Node *depNode;
   int32_t depIndex;
   int32_t numDep;
   int32_t needResult;
   TR::LabelSymbol *doneLabel, *res0Label, *res1Label;
   bool branchOn1;
   TR::Register *resultReg;

   TR::SymbolReference *castClassSymRef;
   TR::Node *castClassNode;
   TR_OpaqueClassBlock *castClassAddr;
   TR_OpaqueClassBlock *guessClassArray[NUM_PICS];

   depIndex = 0;
   numDep = 0;
   depNode = NULL;

   castClassNode = node->getSecondChild();
   castClassSymRef = castClassNode->getSymbolReference();
   castClassAddr = TR::TreeEvaluator::getCastClassAddress(castClassNode);
   uint8_t num_PICS = TR::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastInfo(cg, node, guessClassArray);

   bool testEqualClass = instanceOfOrCheckCastNeedEqualityTest(node, cg);
   bool testCastClassIsSuper = instanceOfOrCheckCastNeedSuperTest(node, cg);
   bool isFinalClass = (castClassSymRef == NULL) ? false : castClassSymRef->isNonArrayFinal(comp);
   bool needsHelperCall = needHelperCall(testCastClassIsSuper, isFinalClass);
   bool testCache = needTestCache(!comp->getOption(TR_DisableInlineCheckCast), needsHelperCall, testCastClassIsSuper, castClassAddr, num_PICS);

   // these are the conditions under which resultReg is set in VMgenCoreInstanceofEvaluator
   // needResult = testEqualClass | testCastClassIsSuper | testCache | !needsHelperCall;
   // but we always need a result for instanceOf
   needResult = true;
   doneLabel = generateLabelSymbol(cg);

   res0Label = doneLabel;
   res1Label = doneLabel;
   branchOn1 = false;

   resultReg = TR::TreeEvaluator::VMgenCoreInstanceofEvaluator(node, cg, false, depIndex, numDep, depNode, needResult, needsHelperCall, testEqualClass, testCache, testCastClassIsSuper, doneLabel,
         res0Label, res1Label, branchOn1);

   if (resultReg != node->getRegister())
      node->setRegister(resultReg);

   return resultReg;
   }

// genCoreInstanceofEvaluator is used by if instanceof and instanceof routines.
// The routine generates the 'core' code for instanceof evaluation. It requires a true and false label
// (which are the same and are just fall-through labels if no branching is required) as well as
// a boolean to indicate if the result should be calculated and returned in a register.
// The code also needs to indicate if the fall-through case if for 'true' or 'false'.
TR::Register * J9::Power::TreeEvaluator::VMgenCoreInstanceofEvaluator(TR::Node * node, TR::CodeGenerator * cg, bool isVMifInstanceOf, int32_t depIndex, int32_t numDep,
      TR::Node *depNode, bool needResult, bool needHelperCall, bool testEqualClass, bool testCache, bool testCastClassIsSuper, TR::LabelSymbol *doneLabel,
      TR::LabelSymbol *res0Label, TR::LabelSymbol *res1Label, bool branchOn1)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   TR::Register *objectReg, *castClassReg, *objClassReg;
   TR::Register *resultReg, *crReg, *scratch1Reg, *scratch2Reg, *scratch3Reg;
   TR::Instruction *iCursor;
   TR::SymbolReference *castClassSymRef;
   TR::RegisterDependencyConditions *conditions;
   TR::LabelSymbol *trueLabel, *callLabel, *nextTestLabel;
   TR::Node *objectNode, *castClassNode;
   int32_t castClassDepth;
   TR::ILOpCodes opCode;
   TR::RegisterDependencyConditions *deps;
   TR_OpaqueClassBlock *castClassAddr;
   TR_OpaqueClassBlock *guessClassArray[NUM_PICS];
   TR::Compilation * comp = cg->comp();
   castClassDepth = -1;
   trueLabel = NULL;
   resultReg = NULL;
   deps = NULL;
   iCursor = NULL;

   castClassNode = node->getSecondChild();
   castClassSymRef = castClassNode->getSymbolReference();
   castClassAddr = TR::TreeEvaluator::getCastClassAddress(castClassNode);
   uint8_t num_PICS = TR::TreeEvaluator::interpreterProfilingInstanceOfOrCheckCastInfo(cg, node, guessClassArray);
   objectNode = node->getFirstChild();

   bool stopUsingHelperCallCR = false;

   if (needHelperCall)
      {
      callLabel = generateLabelSymbol(cg);

      TR::ILOpCodes opCode = node->getOpCodeValue();

      TR::Node::recreate(node, TR::icall);
      directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);

      iCursor = cg->getAppendInstruction();
      while (iCursor->getOpCodeValue() != TR::InstOpCode::bl)
         iCursor = iCursor->getPrev();
      conditions = ((TR::PPCDepImmSymInstruction *) iCursor)->getDependencyConditions();
      iCursor = iCursor->getPrev();
      iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, callLabel, iCursor);
      iCursor = iCursor->getPrev();

      auto firstArgReg = node->getSymbol()->castToMethodSymbol()->getLinkageConvention() == TR_CHelper ? TR::RealRegister::gr4 : TR::RealRegister::gr3;
      objectReg = conditions->searchPreConditionRegister(static_cast<TR::RealRegister::RegNum>(firstArgReg + 1));
      castClassReg = conditions->searchPreConditionRegister(firstArgReg);
      scratch1Reg = conditions->searchPreConditionRegister(static_cast<TR::RealRegister::RegNum>(firstArgReg + 2));
      scratch2Reg = conditions->searchPreConditionRegister(static_cast<TR::RealRegister::RegNum>(firstArgReg + 4));
      scratch3Reg = conditions->searchPreConditionRegister(static_cast<TR::RealRegister::RegNum>(firstArgReg + 6));
      objClassReg = conditions->searchPreConditionRegister(static_cast<TR::RealRegister::RegNum>(firstArgReg + 3));
      crReg = conditions->searchPreConditionRegister(TR::RealRegister::cr0);
      if (!crReg)
         {
         // This is another hack to compensate for skipping the preservating of CCRs for helper calls in linkage code.
         // This hack wouldn't be necessary if this code wasn't being lazy and re-using the dependencies
         // set up by the direct call evaluator, but alas...
         // Since we are not preserving CCRs and this code previously expected to find a CCR in the call instruction's
         // dependency conditions we allocate a new virtual here and stick it in the dependencies, making sure to
         // repeat everything that is normally done for virtual regs when they are referenced by a generated instruction,
         // which just means we record two uses to correspond to the fact that we added them to the pre/post deps of the
         // call.
         crReg = nonFixedDependency(conditions, NULL, &depIndex, TR_CCR, false, cg);
         stopUsingHelperCallCR = true;
         TR::Instruction *callInstruction = scratch1Reg->getStartOfRange();
         callInstruction->useRegister(crReg);
         callInstruction->useRegister(crReg);
         }

      if (depNode != NULL)
         {
         cg->evaluate(depNode);
         deps = generateRegisterDependencyConditions(cg, depNode, 0, &iCursor);
         }

      if (testEqualClass || testCache || testCastClassIsSuper)
         {
         if (needResult)
            {
            resultReg = conditions->searchPreConditionRegister(static_cast<TR::RealRegister::RegNum>(firstArgReg + 5));
            iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 0, iCursor);
            }
         if (!objectNode->isNonNull())
            {
            iCursor = genNullTest(objectNode, objectReg, crReg, scratch1Reg, iCursor, cg);
            iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, res0Label, crReg, iCursor);
            }
         }
      }
   else
      {
      bool earlyEval = (castClassNode->getOpCodeValue() != TR::loadaddr || castClassNode->getReferenceCount() > 1) ? true : false;

      bool objInDep = false, castClassInDep = false;
      if (depNode != NULL)
         {
         int32_t i1;

         TR::Node *grNode;
         cg->evaluate(depNode);
         deps = generateRegisterDependencyConditions(cg, depNode, 7);

         for (i1 = 0; i1 < numDep; i1++)
            {
            grNode = depNode->getChild(i1);
            if (grNode->getOpCodeValue() == TR::PassThrough)
               grNode = grNode->getFirstChild();
            if (grNode == objectNode)
               objInDep = true;
            if (grNode == castClassNode)
               castClassInDep = true;
            }
         }

      if (deps == NULL)
         {
         conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(7, 7, cg->trMemory());
         }
      else
         {
         conditions = deps;
         }

      crReg = nonFixedDependency(conditions, NULL, &depIndex, TR_CCR, false, cg);

      if (needResult)
         resultReg = nonFixedDependency(conditions, NULL, &depIndex, TR_GPR, false, cg);

      objClassReg = nonFixedDependency(conditions, NULL, &depIndex, TR_GPR, true, cg);

      objectReg = cg->evaluate(objectNode);

      if (!objInDep)
         objectReg = nonFixedDependency(conditions, objectReg, &depIndex, TR_GPR, true, cg);

      if (earlyEval)
         {
         castClassReg = cg->evaluate(castClassNode);
         castClassReg = nonFixedDependency(conditions, castClassReg, &depIndex, TR_GPR, true, cg);
         }

      if (needResult)
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 0);

      if (!objectNode->isNonNull())
         {
         genNullTest(objectNode, objectReg, crReg, objClassReg, NULL, cg);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, res0Label, crReg);
         }

      if (!earlyEval)
         {
         castClassReg = cg->evaluate(castClassNode);
         castClassReg = nonFixedDependency(conditions, castClassReg, &depIndex, TR_GPR, true, cg);
         }

      if (testCastClassIsSuper)
         {
         scratch1Reg = nonFixedDependency(conditions, NULL, &depIndex, TR_GPR, true, cg);
         scratch2Reg = nonFixedDependency(conditions, NULL, &depIndex, TR_GPR, true, cg);
         cg->stopUsingRegister(scratch1Reg);
         cg->stopUsingRegister(scratch2Reg);
         }

      iCursor = cg->getAppendInstruction();

      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

      cg->decReferenceCount(objectNode);
      cg->decReferenceCount(castClassNode);
      cg->stopUsingRegister(crReg);
      cg->stopUsingRegister(objClassReg);
      }

   if (testEqualClass || testCache || testCastClassIsSuper)
      {
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
      // read only 32 bits
      iCursor = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, objClassReg,
            new (cg->trHeapMemory()) TR::MemoryReference(objectReg,
                  (int32_t) TR::Compiler->om.offsetOfObjectVftField(), 4, cg),
            iCursor);
#else
      iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, objClassReg,
            new (cg->trHeapMemory()) TR::MemoryReference(objectReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);
#endif
      iCursor = TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, objClassReg, iCursor);
      }

   if (testEqualClass)
      {
      iCursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, crReg, objClassReg, castClassReg, iCursor);

      if (testCache || testCastClassIsSuper)
         {
         if (needResult)
            {
            trueLabel = generateLabelSymbol(cg);
            iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, trueLabel, crReg, iCursor);
            }
         else
            {
            iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, res1Label, crReg, iCursor);
            }
         }
      else if (needHelperCall)
         {
         if (needResult)
            {
            iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 1, iCursor);
            }
         iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, res1Label, crReg, iCursor);

         bool castClassIsReferenceArray = (comp->isOptServer()) && (!castClassNode->getSymbolReference()->isUnresolved())
               && (castClassNode->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef()) && (castClassNode->getNumChildren() == 1)
               && (castClassNode->getFirstChild()->getOpCodeValue() == TR::anewarray);
         if (castClassIsReferenceArray)
            {
            TR_OpaqueClassBlock * jlobjectclassBlock = fej9->getSystemClassFromClassName("java/lang/Object", 16);
            J9Class* jlobjectarrayclassBlock = jlobjectclassBlock ? ((J9Class*) jlobjectclassBlock)->arrayClass : NULL;
            if (jlobjectarrayclassBlock != NULL)
               {
               iCursor = loadAddressConstant(cg, node, (intptrj_t) (jlobjectarrayclassBlock), scratch1Reg, iCursor);
               iCursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, crReg, objClassReg, scratch1Reg, iCursor);
               if (needResult)
                  {
                  iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 0, iCursor);
                  }
               iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, res0Label, crReg, iCursor);
               }
            }
         }
      else
         {
         iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, res0Label, crReg, iCursor);
         if (needResult)
            {
            iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 1, iCursor);
            }

         if (branchOn1)
            {
            iCursor = generateLabelInstruction(cg, TR::InstOpCode::b, node, res1Label, iCursor);
            }
         }
      }

   if (testCache)
      {
      // The cached value could have been from a previously successful checkcast or instanceof.
      // An answer of 0 in the low order bit indicates 'success' (the cast or instanceof was successful).
      // An answer of 1 in the lower order bit indicates 'failure' (the cast would have thrown an exception, instanceof would have been unsuccessful)
      // Because of this, we can just do a simple load and compare of the 2 class pointers. If it succeeds, the low order bit
      // must be off (success) from a previous checkcast or instanceof. If the low order bit is on, it is guaranteed not to
      // compare and we will take the slow path.
      // Generate inline test for instanceOf
      genInlineTest(node, castClassAddr, guessClassArray, num_PICS, objClassReg, resultReg, crReg, scratch1Reg, scratch2Reg, false, needResult, res0Label, res1Label, doneLabel,
            callLabel, testCastClassIsSuper, iCursor, cg);
      }

   if (testCastClassIsSuper)
      {
      TR::StaticSymbol *castClassSym;

      castClassSym = castClassSymRef->getSymbol()->getStaticSymbol();
      void * clazz = castClassSym->getStaticAddress();
      castClassDepth = TR::Compiler->cls.classDepthOf((TR_OpaqueClassBlock *) clazz);

      iCursor = genTestIsSuper(node, objClassReg, castClassReg, crReg, scratch1Reg, scratch2Reg, castClassDepth, res0Label, iCursor, cg);
      iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, res0Label, crReg, iCursor);

      if (trueLabel == NULL)
         {
         if (needResult)
            {
            iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 1, iCursor);
            }
         }
      else
         {
         iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, trueLabel, iCursor);
         iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 1, iCursor);
         }

      if (branchOn1)
         {
         iCursor = generateLabelInstruction(cg, TR::InstOpCode::b, node, res1Label, iCursor);
         }
      }
   else if (testCache)
      {
      if (trueLabel == NULL)
         {
         if (needResult)
            {
            iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 1, iCursor);
            }
         }
      else
         {
         iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, trueLabel, iCursor);
         iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 1, iCursor);
         }
      }

   if (needHelperCall)
      {
      TR::Register *callResult = node->getRegister();
      TR::RegisterDependencyConditions *newDeps;

      if (resultReg == NULL)
         {
         resultReg = callResult;
         }
      else
         {
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, resultReg, callResult);
         }

      if (isVMifInstanceOf)
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, node, crReg, resultReg, 0);
         }

      if (depNode != NULL)
         {
         newDeps = conditions->cloneAndFix(cg, deps);
         }
      else
         {
         newDeps = conditions->cloneAndFix(cg);
         }

      if (isVMifInstanceOf)
         {
         if (branchOn1)
            {
            generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, res1Label, crReg);
            }
         else
            {
            generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, res0Label, crReg);
            }
         }

      // Just fall through
      // generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);

      // Done
      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, newDeps);

      if (stopUsingHelperCallCR)
         cg->stopUsingRegister(crReg);

      // We have to revive the resultReg here. Should revisit to see how to use registers better
      // (e.g., reducing the chance of moving around).
      if (resultReg != callResult)
         reviveResultRegister(resultReg, callResult, cg);
      }

   if (depNode != NULL)
      cg->decReferenceCount(depNode);

   return resultReg;
   }

static TR::Register *
reservationLockEnter(TR::Node *node, int32_t lwOffset, TR::Register *objectClassReg, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   TR::RegisterDependencyConditions *conditions = createConditionsAndPopulateVSXDeps(cg, 5);
   TR::Register *objReg, *monitorReg, *metaReg, *valReg, *tempReg, *cndReg;
   TR::LabelSymbol *resLabel, *callLabel, *doneLabel, *doneOOLLabel;
   int32_t lockSize;
   TR::Compilation * comp = cg->comp();

   if (objectClassReg)
      objReg = objectClassReg;
   else
      objReg = node->getFirstChild()->getRegister();

   metaReg = cg->getMethodMetaDataRegister();
   monitorReg = cg->allocateRegister();
   valReg = cg->allocateRegister();
   tempReg = cg->allocateRegister();
   cndReg = cg->allocateRegister(TR_CCR);

   resLabel = generateLabelSymbol(cg);
   callLabel = generateLabelSymbol(cg);
   doneLabel = generateLabelSymbol(cg);
   doneOOLLabel = generateLabelSymbol(cg);

   TR::TreeEvaluator::isPrimitiveMonitor(node, cg);
   bool isPrimitive = node->isPrimitiveLockedRegion();
   lockSize = TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord() ? 8 : 4;

   generateTrg1MemInstruction(cg, lockSize == 8 ? TR::InstOpCode::ld : TR::InstOpCode::lwz, node, monitorReg, new (cg->trHeapMemory()) TR::MemoryReference(objReg, lwOffset, lockSize, cg));
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, valReg, metaReg, LOCK_RESERVATION_BIT);
   generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, cndReg, monitorReg, valReg);

   addDependency(conditions, objReg, TR::RealRegister::NoReg, TR_GPR, cg);
   conditions->getPreConditions()->getRegisterDependency(conditions->getAddCursorForPre() - 1)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(conditions->getAddCursorForPost() - 1)->setExcludeGPR0();
   addDependency(conditions, monitorReg, TR::RealRegister::NoReg, TR_GPR, cg);
   conditions->getPreConditions()->getRegisterDependency(conditions->getAddCursorForPre() - 1)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(conditions->getAddCursorForPost() - 1)->setExcludeGPR0();
   addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);
   addDependency(conditions, valReg, TR::RealRegister::NoReg, TR_GPR, cg);

   if (!isPrimitive)
      {
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, resLabel, cndReg);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, tempReg, monitorReg, LOCK_INC_DEC_VALUE);
      generateMemSrc1Instruction(cg, lockSize == 8 ? TR::InstOpCode::std : TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(objReg, lwOffset, lockSize, cg), tempReg);
      generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);
      }
   else
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, cndReg);

   TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *reserved_checkLabel = generateLabelSymbol(cg);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, resLabel);
   // PRIMITIVE lockReservation enter sequence:            Non-Primitive lockReservation enter sequence:
   // resLabel(startOOLLabel):
   //    cmpli cr0, monitorReg, 0                             cmpli cr0, monitorReg, 0
   //    bne   reserved_checkLabel                            bne   reserved_checkLabel
   //    li    tempReg, lwOffset                              li    tempReg, lwOffset
   //                                                         addi  valReg, metaReg, RES+INC           <--- Diff
   // loopLabel:
   //    larx  monitorReg, [objReg, tempReg]                  larx  monitorReg, [objReg, tempReg]
   //    cmpli cr0, monitorReg, 0                             cmpli cr0, monitorReg, 0
   //    bne   callLabel                                      bne   callLabel
   //    stcx. valReg, [objReg, tempReg]                      stcx. valReg, [objReg, tempReg]
   //    bne   loopLabel                                      bne   loopLabel
   //    isync                                                isync
   //    b     doneLabel                                      b     doneLabel
   // reserved_checkLabel:
   //    li    tempReg, PRIMITIVE_ENTER_MASK                  li    tempReg, NON_PRIMITIVE_ENTER_MASK  <--- Diff
   //    andc  tempReg, monitorReg, tempReg                   andc  tempReg, monitorReg, tempReg
   //                                                         addi  valReg, metaReg, RES               <--- Diff
   //    cmpl  cr0, tempReg, valReg                           cmpl  cr0, tempReg, valReg
   //    bne   callLabel                                      bne   callLabel
   //                                                         addi  monitorReg, monitorReg, INC        <--- Diff
   //                                                         st    monitorReg, [objReg, lwOffset]     <--- Diff
   // doneLabel:
   // doneOOLLabel:
   // === OUT OF LINE ===
   // callLabel:
   //    bl    jitMonitorEntry
   //    b doneOOLLabel
   generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, cndReg, monitorReg, 0);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, reserved_checkLabel, cndReg);
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, tempReg, lwOffset & 0x0000FFFF);
   if (!isPrimitive)
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, valReg, metaReg, LOCK_RESERVATION_BIT | LOCK_INC_DEC_VALUE);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel);
   generateTrg1MemInstruction(cg, lockSize == 8 ? TR::InstOpCode::ldarx : TR::InstOpCode::lwarx,
                              PPCOpProp_LoadReserveExclusiveAccess, node, monitorReg, new (cg->trHeapMemory()) TR::MemoryReference(objReg, tempReg, lockSize, cg));
   generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, cndReg, monitorReg, 0);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, callLabel, cndReg);
   generateMemSrc1Instruction(cg, lockSize == 8 ? TR::InstOpCode::stdcx_r : TR::InstOpCode::stwcx_r,
                              node, new (cg->trHeapMemory()) TR::MemoryReference(objReg, tempReg, lockSize, cg), valReg);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, loopLabel, cndReg);
   generateInstruction(cg, TR::InstOpCode::isync, node);
   generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, reserved_checkLabel);
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, tempReg, isPrimitive ? LOCK_RES_PRIMITIVE_ENTER_MASK : LOCK_RES_NON_PRIMITIVE_ENTER_MASK);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::andc, node, tempReg, monitorReg, tempReg);
   if (!isPrimitive)
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, valReg, metaReg, LOCK_RESERVATION_BIT);
   generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, cndReg, tempReg, valReg);
   cg->stopUsingRegister(tempReg);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, callLabel, cndReg);
   if (!isPrimitive)
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, monitorReg, monitorReg, LOCK_INC_DEC_VALUE);
      generateMemSrc1Instruction(cg, lockSize == 8 ? TR::InstOpCode::std : TR::InstOpCode::stw,
                                 node, new (cg->trHeapMemory()) TR::MemoryReference(objReg, lwOffset & 0x0000FFFF, lockSize, cg), monitorReg);
      }

   TR_PPCOutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_PPCOutOfLineCodeSection(node, TR::call, NULL, callLabel, doneOOLLabel, cg);
   cg->getPPCOutOfLineCodeSectionList().push_front(outlinedHelperCall);

   doneLabel->setEndInternalControlFlow();
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneOOLLabel);

   conditions->stopUsingDepRegs(cg, objReg);
   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }

static TR::Register *
reservationLockExit(TR::Node *node, int32_t lwOffset, TR::Register *objectClassReg, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   TR::RegisterDependencyConditions *conditions = createConditionsAndPopulateVSXDeps(cg, 5);
   TR::Register *objReg, *monitorReg, *metaReg, *valReg, *tempReg, *cndReg;
   TR::LabelSymbol *resLabel, *callLabel, *doneLabel, *doneOOLLabel;
   int32_t lockSize;
   TR::Compilation *comp = cg->comp();

   if (objectClassReg)
      objReg = objectClassReg;
   else
      objReg = node->getFirstChild()->getRegister();

   metaReg = cg->getMethodMetaDataRegister();
   monitorReg = cg->allocateRegister();
   valReg = cg->allocateRegister();
   tempReg = cg->allocateRegister();
   cndReg = cg->allocateRegister(TR_CCR);
   addDependency(conditions, objReg, TR::RealRegister::NoReg, TR_GPR, cg);
   conditions->getPreConditions()->getRegisterDependency(conditions->getAddCursorForPre() - 1)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(conditions->getAddCursorForPost() - 1)->setExcludeGPR0();
   addDependency(conditions, monitorReg, TR::RealRegister::NoReg, TR_GPR, cg);
   conditions->getPreConditions()->getRegisterDependency(conditions->getAddCursorForPre() - 1)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(conditions->getAddCursorForPost() - 1)->setExcludeGPR0();
   addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);
   addDependency(conditions, valReg, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, tempReg, TR::RealRegister::NoReg, TR_GPR, cg);

   resLabel = generateLabelSymbol(cg);
   callLabel = generateLabelSymbol(cg);
   doneLabel = generateLabelSymbol(cg);
   doneOOLLabel = generateLabelSymbol(cg);

   bool isPrimitive = node->isPrimitiveLockedRegion();
   lockSize = TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord() ? 8 : 4;

   generateTrg1MemInstruction(cg, lockSize == 8 ? TR::InstOpCode::ld : TR::InstOpCode::lwz, node, monitorReg, new (cg->trHeapMemory()) TR::MemoryReference(objReg, lwOffset, lockSize, cg));
   if (!node->isPrimitiveLockedRegion())
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, tempReg, metaReg, LOCK_RESERVATION_BIT + LOCK_INC_DEC_VALUE);
      generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, cndReg, monitorReg, tempReg);
      }
   else
      {
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, tempReg, monitorReg, LOCK_RES_PRIMITIVE_EXIT_MASK);
      generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, cndReg, tempReg, LOCK_RESERVATION_BIT);
      }

   if (!isPrimitive)
      {
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, resLabel, cndReg);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, valReg, metaReg, LOCK_RESERVATION_BIT);
      generateMemSrc1Instruction(cg, lockSize == 8 ? TR::InstOpCode::std : TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(objReg, lwOffset, lockSize, cg), valReg);
      generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);
      }
   else
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, resLabel, cndReg);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, resLabel);
   // PRIMITIVE reservationLock exit sequence           Non-PRIMITIVE reservationLock exit sequence
   // resLabel(startOOLLabel):
   //    li     tempReg, RES_OWNING_COMPLEMENT          li     tempReg, RES_OWNING_COMPLEMENT
   //    andc   tempReg, monitorReg, tempReg            andc   tempReg, monitorReg, tempReg
   //    addi   valReg, metaReg, RES_BIT                addi   valReg, metaReg, RES_BIT
   //    cmpl   cndReg, tempReg, valReg                 cmpl   cndReg, tempReg, valReg
   //    bne    callLabel                               bne    callLabel
   //    andi_r tempReg, monitorReg, RECURSION_MASK     andi_r tempReg, monitorReg, NON_PRIMITIVE_EXIT_MASK <-- Diff
   //    bne    doneLabel                               beq    callLabel                                    <-- Diff
   //    addi   monitorReg, monitorReg, INC             addi   monitorReg, monitorReg, -INC                 <-- Diff
   //    st     monitorReg, [objReg, lwOffset]          st     monitorReg, [objReg, lwOffset]
   //                                                   b      doneLabel
   // doneLabel:
   // doneOOLLabel:
   // callLabel:
   //    bl     jitMonitorExit
   //    b      doneOOLLabel
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, tempReg, LOCK_RES_OWNING_COMPLEMENT);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::andc, node, tempReg, monitorReg, tempReg);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, valReg, metaReg, LOCK_RESERVATION_BIT);
   generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, cndReg, tempReg, valReg);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, callLabel, cndReg);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, tempReg, monitorReg, isPrimitive ? OBJECT_HEADER_LOCK_RECURSION_MASK : LOCK_RES_NON_PRIMITIVE_EXIT_MASK);
   if (isPrimitive)
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, doneLabel, cndReg);
   else
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, callLabel, cndReg);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, monitorReg, monitorReg, (isPrimitive ? LOCK_INC_DEC_VALUE : -LOCK_INC_DEC_VALUE) & 0x0000FFFF);
   generateMemSrc1Instruction(cg, lockSize == 8 ? TR::InstOpCode::std : TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(objReg, lwOffset, lockSize, cg), monitorReg);
   if (!isPrimitive)
      generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);

   TR_PPCOutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_PPCOutOfLineCodeSection(node, TR::call, NULL, callLabel, doneOOLLabel, cg);
   cg->getPPCOutOfLineCodeSectionList().push_front(outlinedHelperCall);

   doneLabel->setEndInternalControlFlow();
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, doneOOLLabel);

   conditions->stopUsingDepRegs(cg, objReg);
   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }

TR::Register *J9::Power::TreeEvaluator::VMmonexitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   int32_t lwOffset = fej9->getByteOffsetToLockword((TR_OpaqueClassBlock *) cg->getMonClass(node));
   TR::Compilation * comp = cg->comp();

   if (comp->getOption(TR_OptimizeForSpace) || (comp->getOption(TR_FullSpeedDebug))
         || comp->getOption(TR_DisableInlineMonExit))
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::call);
      TR::Register *targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }

   TR::Node *objNode = node->getFirstChild();
   TR::Register *objReg = cg->evaluate(objNode);

   int32_t numDeps = 9;
   TR::Register *objectClassReg = NULL;
   TR::Register *baseReg = objReg;
   TR::Register *condReg = NULL;
   TR::LabelSymbol *callLabel = generateLabelSymbol(cg);
   bool simpleLocking = false;

   TR::LabelSymbol *monitorLookupCacheLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *fallThruFromMonitorLookupCacheLabel = generateLabelSymbol(cg);

   TR::Register *lookupOffsetReg = NULL;

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, startLabel, NULL);
   startLabel->setStartInternalControlFlow();

#if defined (J9VM_THR_LOCK_NURSERY)
   if (lwOffset <= 0)
      {
      objectClassReg = cg->allocateRegister();
      condReg = cg->allocateRegister(TR_CCR);

#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
      // must read only 32 bits
      generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, objectClassReg,
            new (cg->trHeapMemory()) TR::MemoryReference(objReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), 4, cg));
#else
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, objectClassReg,
            new (cg->trHeapMemory()) TR::MemoryReference(objReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceAddress(), cg));
#endif
      TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, objectClassReg);

      int32_t offsetOfLockOffset = offsetof(J9Class, lockOffset);
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(objectClassReg, offsetOfLockOffset, TR::Compiler->om.sizeofReferenceAddress(), cg);
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, objectClassReg, tempMR);

      generateTrg1Src1ImmInstruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::cmpi8 : TR::InstOpCode::cmpi4, node, condReg, objectClassReg, 0);

      if (comp->getOption(TR_EnableMonitorCacheLookup))
         {
         lwOffset = 0;
         generateConditionalBranchInstruction(cg, TR::InstOpCode::ble, node, monitorLookupCacheLabel, condReg);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, objectClassReg, objReg, objectClassReg);
         generateLabelInstruction(cg, TR::InstOpCode::b, node, fallThruFromMonitorLookupCacheLabel);

         generateLabelInstruction(cg, TR::InstOpCode::label, node, monitorLookupCacheLabel);

         lookupOffsetReg = cg->allocateRegister();
         numDeps++;

         int32_t offsetOfMonitorLookupCache = offsetof(J9VMThread, objectMonitorLookupCache);

         generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, lookupOffsetReg, objReg);

         int32_t t = trailingZeroes(fej9->getObjectAlignmentInBytes());

         if (TR::Compiler->target.is64Bit())
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sradi, node, lookupOffsetReg, lookupOffsetReg, t);
         else
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi, node, lookupOffsetReg, lookupOffsetReg, t);

         J9JavaVM * jvm = fej9->getJ9JITConfig()->javaVM;

         if (TR::Compiler->target.is64Bit())
         simplifyANDRegImm(node, lookupOffsetReg, lookupOffsetReg, (int64_t) J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE - 1, cg, objNode);
         else
         simplifyANDRegImm(node, lookupOffsetReg, lookupOffsetReg, J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE - 1, cg, objNode);

         if (TR::Compiler->target.is64Bit())
         generateShiftLeftImmediateLong(cg, node, lookupOffsetReg, lookupOffsetReg, trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField()));
         else
         generateShiftLeftImmediate(cg, node, lookupOffsetReg, lookupOffsetReg, trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField()));

         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, lookupOffsetReg, lookupOffsetReg, offsetOfMonitorLookupCache);

         generateTrg1MemInstruction(cg, (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord()) ? TR::InstOpCode::lwz :TR::InstOpCode::Op_load, node, objectClassReg,
               new (cg->trHeapMemory()) TR::MemoryReference(cg->getMethodMetaDataRegister(), lookupOffsetReg, TR::Compiler->om.sizeofReferenceField(), cg));

         generateTrg1Src1ImmInstruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::cmpi8 : TR::InstOpCode::cmpi4, node, condReg, objectClassReg, 0);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, callLabel, condReg);

         int32_t offsetOfMonitor = offsetof(J9ObjectMonitor, monitor);
         generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, lookupOffsetReg,
               new (cg->trHeapMemory()) TR::MemoryReference(objectClassReg, offsetOfMonitor, TR::Compiler->om.sizeofReferenceAddress(), cg));

         int32_t offsetOfUserData = offsetof(J9ThreadAbstractMonitor, userData);
         generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, lookupOffsetReg,
               new (cg->trHeapMemory()) TR::MemoryReference(lookupOffsetReg, offsetOfUserData, TR::Compiler->om.sizeofReferenceAddress(), cg));

         generateTrg1Src2Instruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::cmp8 : TR::InstOpCode::cmp4, node, condReg, lookupOffsetReg, objReg);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, callLabel, condReg);

         int32_t offsetOfAlternateLockWord = offsetof(J9ObjectMonitor, alternateLockword);

         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, objectClassReg, objectClassReg, offsetOfAlternateLockWord);

         generateLabelInstruction(cg, TR::InstOpCode::label, node, fallThruFromMonitorLookupCacheLabel);
         }
      else
         {
         generateConditionalBranchInstruction(cg, TR::InstOpCode::ble, node, callLabel, condReg);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, objectClassReg, objReg, objectClassReg);
         }

      lwOffset = 0;
      baseReg = objectClassReg;
      simpleLocking = true;

      numDeps = numDeps + 2;
      }
#endif

   bool reserveLocking = false, normalLockWithReservationPreserving = false;

   if (!simpleLocking && comp->getOption(TR_ReservingLocks))
      TR::TreeEvaluator::evaluateLockForReservation(node, &reserveLocking, &normalLockWithReservationPreserving, cg);
   if (reserveLocking)
      return reservationLockExit(node, lwOffset, objectClassReg, cg);

   TR::RegisterDependencyConditions *conditions;
   TR::Register *monitorReg, *ctempReg, *metaReg;
   TR::Register *threadReg, *offsetReg;
   TR::InstOpCode::Mnemonic opCode;
   int32_t lockSize;

   conditions = createConditionsAndPopulateVSXDeps(cg, numDeps);

   if (condReg == NULL)
      condReg = cg->allocateRegister(TR_CCR);
   threadReg = cg->allocateRegister();
   monitorReg = cg->allocateRegister();
   ctempReg = cg->allocateRegister();
   addDependency(conditions, objReg, TR::RealRegister::NoReg, TR_GPR, cg);
   conditions->getPreConditions()->getRegisterDependency(conditions->getAddCursorForPre() - 1)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(conditions->getAddCursorForPost() - 1)->setExcludeGPR0();
   addDependency(conditions, monitorReg, TR::RealRegister::NoReg, TR_GPR, cg);
   conditions->getPreConditions()->getRegisterDependency(conditions->getAddCursorForPre() - 1)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(conditions->getAddCursorForPost() - 1)->setExcludeGPR0();
   addDependency(conditions, ctempReg, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, threadReg, TR::RealRegister::NoReg, TR_GPR, cg);

   if (objectClassReg)
      {
      addDependency(conditions, objectClassReg, TR::RealRegister::NoReg, TR_GPR, cg);
      conditions->getPreConditions()->getRegisterDependency(conditions->getAddCursorForPre() - 1)->setExcludeGPR0();
      conditions->getPostConditions()->getRegisterDependency(conditions->getAddCursorForPost() - 1)->setExcludeGPR0();
      }

   addDependency(conditions, condReg, TR::RealRegister::cr0, TR_CCR, cg);

   if (lookupOffsetReg)
      addDependency(conditions, lookupOffsetReg, TR::RealRegister::NoReg, TR_GPR, cg);

   if (!ppcSupportsReadMonitors || !node->isReadMonitor())
      {
      //    lwz    monitorReg, offset(objReg)
      //    li     ctempReg, 0
      //    cmp    condReg, monitorReg, metaReg
      //    bne    condReg, decLabel
      //    lwsync
      //    st     offset(objReg), ctempReg
      //    b      doneLabel
      // decLabel:
      // #if 32BIT:
      //    rlwinm threadReg, monitorReg, 0, ~OBJECT_HEADER_LOCK_RECURSION_MASK  // mask out count
      // #elif 64BIT
      //    li      threadReg, OBJECT_HEADER_LOCK_RECURSION_MASK
      //    andc    threadReg, monitorReg, threadReg
      // #endif
      //    cmp    cr0, 0, metaReg, threadReg
      //    bne    callLabel                   // inflated or flc bit set
      //    addi   monitorReg, monitorReg, -LOCK_INC_DEC_VALUE
      //    st     [objReg, monitor offset], monitorReg
      // doneLabel:
      // callReturnLabel:
      // === OUT OF LINE ===
      // callLabel:
      //    bl     jitMonitorExit
      //    b      callReturnLabel

      TR::LabelSymbol *decLabel, *doneLabel;
      metaReg = cg->getMethodMetaDataRegister();

      decLabel = generateLabelSymbol(cg);
      doneLabel = generateLabelSymbol(cg);

      int32_t moffset = lwOffset;
      if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
         {
         opCode = TR::InstOpCode::ld;
         lockSize = 8;
         }
      else
         {
         opCode = TR::InstOpCode::lwz;
         lockSize = 4;
         }
      generateTrg1MemInstruction(cg, opCode, node, monitorReg, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, moffset, lockSize, cg));
#if defined(J9VM_TASUKI_LOCKS_SINGLE_SLOT)
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, ctempReg, 0);
#endif
      generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmp, node, condReg, monitorReg, metaReg);

      if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
         // use PPC AS branch hint
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, node, decLabel, condReg);
      else
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, decLabel, condReg);

      // exiting from read monitors still needs lwsync
      // (ensures loads have completed before releasing lock)
      if (TR::Compiler->target.isSMP())
         generateInstruction(cg, TR::InstOpCode::lwsync, node);
      if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
         opCode = TR::InstOpCode::std;
      else
         opCode = TR::InstOpCode::stw;
      generateMemSrc1Instruction(cg, opCode, node, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, moffset, lockSize, cg), ctempReg);
      generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);

      generateLabelInstruction(cg, TR::InstOpCode::label, node, decLabel);
      if (TR::Compiler->target.is64Bit())
         {
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, threadReg, OBJECT_HEADER_LOCK_RECURSION_MASK);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::andc, node, threadReg, monitorReg, threadReg);
         }
      else
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, threadReg, monitorReg, 0, ~OBJECT_HEADER_LOCK_RECURSION_MASK);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::Op_cmpl, node, condReg, threadReg, metaReg);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, callLabel, condReg);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, monitorReg, monitorReg, -LOCK_INC_DEC_VALUE);
      generateMemSrc1Instruction(cg, opCode, node, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, moffset, lockSize, cg), monitorReg);

      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

      doneLabel->setEndInternalControlFlow();

      TR::LabelSymbol *callReturnLabel = generateLabelSymbol(cg);

      TR_PPCOutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_PPCOutOfLineCodeSection(node, TR::call, NULL, callLabel, callReturnLabel, cg);
      cg->getPPCOutOfLineCodeSectionList().push_front(outlinedHelperCall);

      generateLabelInstruction(cg, TR::InstOpCode::label, node, callReturnLabel);

      conditions->stopUsingDepRegs(cg, objReg);
      cg->decReferenceCount(objNode);
      }
   else
      {
      // read-only locks, ReaderReg = 0x0000 0000
      //    li     offsetReg, offset
      //    lwsync
      // loopLabel:
      //    lwarx  monitorReg, [objReg, offsetReg]
      //    cmpi   condReg,monitorReg, 0x6 // count == 1 (flc bit is set)
      //    addi   ctempReg, monitorReg, -4
      //    beq    decLabel
      //    stwcx  [objReg, offsetReg], ctempReg
      //    bne    loopLabel
      //    b      doneLabel
      // decLabel: (a misleading name, really a RestoreAndCallLabel)
      //    andi_r threadReg, monitorReg, 0x3
      //    or     threadReg, metaReg, threadReg
      //    stwcx  [objReg, monitor offset], threadReg
      //    beq    callLabel
      //    b      loopLabel
      // doneLabel:
      // callReturnLabel:
      // === OUT OF LINE ===
      // callLabel:
      //    bl   jitMonitorExit
      //    b    callReturnLabel;

      TR::LabelSymbol *loopLabel, *decLabel, *doneLabel;
      loopLabel = generateLabelSymbol(cg);
      decLabel = generateLabelSymbol(cg);
      doneLabel = generateLabelSymbol(cg);

      offsetReg = cg->allocateRegister();
      addDependency(conditions, offsetReg, TR::RealRegister::gr4, TR_GPR, cg);

      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, offsetReg, lwOffset);

      // exiting from read monitors still needs lwsync
      // (ensures loads have completed before releasing lock)
      if (TR::Compiler->target.isSMP())
         generateInstruction(cg, TR::InstOpCode::lwsync, node);

      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel, conditions);
      if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
         {
         opCode = TR::InstOpCode::ldarx;
         lockSize = 8;
         }
      else
         {
         opCode = TR::InstOpCode::lwarx;
         lockSize = 4;
         }
      generateTrg1MemInstruction(cg, opCode, node, monitorReg, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, offsetReg, lockSize, cg));

      generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpi, node, condReg, monitorReg, 0x6);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, ctempReg, monitorReg, -4);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, decLabel, condReg);

      if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
         opCode = TR::InstOpCode::stdcx_r;
      else
         opCode = TR::InstOpCode::stwcx_r;
      generateMemSrc1Instruction(cg, opCode, node, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, offsetReg, lockSize, cg), ctempReg);

      if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
         // use PPC AS branch hint
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, node, loopLabel, condReg);
      else
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, loopLabel, condReg);
      generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);

      generateLabelInstruction(cg, TR::InstOpCode::label, node, decLabel);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, threadReg, monitorReg, condReg, 0x3);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, threadReg, threadReg, metaReg);
      generateMemSrc1Instruction(cg, opCode, node, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, offsetReg, lockSize, cg), threadReg);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, callLabel, condReg);
      generateLabelInstruction(cg, TR::InstOpCode::b, node, loopLabel);

      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

      doneLabel->setEndInternalControlFlow();

      TR::LabelSymbol *callReturnLabel = generateLabelSymbol(cg);

      TR_PPCOutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_PPCOutOfLineCodeSection(node, TR::call, NULL, callLabel, callReturnLabel, cg);
      cg->getPPCOutOfLineCodeSectionList().push_front(outlinedHelperCall);

      generateLabelInstruction(cg, TR::InstOpCode::label, node, callReturnLabel);

      conditions->stopUsingDepRegs(cg, objReg);
      cg->decReferenceCount(objNode);
      }

   return (NULL);
   }

static void genInitObjectHeader(TR::Node *node, TR::Instruction *&iCursor, TR_OpaqueClassBlock *clazz, TR::Register *classReg, TR::Register *resReg, TR::Register *zeroReg,
      TR::Register *condReg, TR::Register *temp1Reg, TR::Register *temp2Reg, TR::Register * packedClobberedOffset, TR::RegisterDependencyConditions *conditions, bool needZeroInit,
      TR::CodeGenerator *cg);

static void genHeapAlloc(TR::Node *node, TR::Instruction *&iCursor, TR_OpaqueClassBlock *clazz, bool isVariableLen, TR::Register *enumReg, TR::Register *classReg, TR::Register *resReg, TR::Register *zeroReg, TR::Register *condReg,
      TR::Register *dataSizeReg, TR::Register *heapTopReg, TR::Register *sizeReg, TR::Register *temp1Reg, TR::Register *temp2Reg, TR::Register *temp3Reg, TR::LabelSymbol *callLabel, TR::LabelSymbol *doneLabel,
      int32_t allocSize, int32_t elementSize, bool usingTLH, bool needZeroInit,  TR::RegisterDependencyConditions *dependencies, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   TR::Register *metaReg = cg->getMethodMetaDataRegister();
   TR::ILOpCodes opCode = node->getOpCodeValue();

   if (TR::Options::getCmdLineOptions()->realTimeGC())
      {
#if defined(J9VM_GC_REALTIME)
      // Use temp3Reg to hold the javaVM.
      iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp3Reg,
                                           new (cg->trHeapMemory()) TR::MemoryReference(metaReg, fej9->thisThreadJavaVMOffset(), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);

      // Realtime size classes are now a pointer in J9JavaVM rather than an inlined struct of arrays, so we can't use J9JavaVM as a base pointer anymore
      // Use temp3Reg to hold J9JavaVM->realTimeSizeClasses
      iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp3Reg,
                                           new (cg->trHeapMemory()) TR::MemoryReference(temp3Reg, fej9->getRealtimeSizeClassesOffset(), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);

      // heap allocation, so proceed
      if (isVariableLen)
         {
         // make sure size isn't too big
         // convert max object size to num elements because computing an object size from num elements may overflow

         uintptrj_t maxSize = fej9->getMaxObjectSizeForSizeClass();
         TR_ASSERT(((int32_t)(maxSize)-allocSize)/elementSize <= USHRT_MAX, "MaxObjectSizeForSizeClass won't fit into 16-bits.");

         iCursor = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, condReg, enumReg, ((int32_t)(maxSize)-allocSize)/elementSize, iCursor);
         iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, node, callLabel, condReg, iCursor);

         if (TR::Compiler->om.useHybridArraylets())
            {
            // Zero length arrays are discontiguous (i.e. they also need the discontiguous length field to be 0) because
            // they are indistinguishable from non-zero length discontiguous arrays
            iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, node, condReg, enumReg, 0, iCursor);
            iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, callLabel, condReg, iCursor);
            }

         // now compute size of object in bytes
         TR_ASSERT(elementSize == 1 || elementSize == 2 || elementSize == 4 || elementSize == 8 || elementSize == 16, "Illegal element size for genHeapAlloc()");

         TR::Register *scaledSizeReg = enumReg;

         if (elementSize > 1)
            {
            iCursor = generateShiftLeftImmediate(cg, node, dataSizeReg, enumReg, trailingZeroes(elementSize), iCursor);
            scaledSizeReg = dataSizeReg;
            }

         // need to round up to sizeof(UDATA) so we can use it to index into size class index array
         // conservatively just add sizeof(UDATA) bytes and round
         if (elementSize < sizeof(UDATA))
            iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, dataSizeReg, scaledSizeReg, allocSize + sizeof(UDATA) - 1, iCursor);
         else
            iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, dataSizeReg, scaledSizeReg, allocSize, iCursor);

         if (elementSize < sizeof(UDATA))
            {
            if (TR::Compiler->target.is64Bit())
               iCursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicr, node, dataSizeReg, dataSizeReg, 0, int64_t(-sizeof(UDATA)), iCursor);
            else
               iCursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, dataSizeReg, dataSizeReg, 0, -sizeof(UDATA), iCursor);
            }

#ifdef J9VM_INTERP_FLAGS_IN_CLASS_SLOT
         iCursor = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, condReg, dataSizeReg, J9_GC_MINIMUM_OBJECT_SIZE, iCursor);
         TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
         if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
            iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, PPCOpProp_BranchLikely, node, doneLabel, condReg, iCursor);
         else
            iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, node, doneLabel, condReg, iCursor);
         iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, dataSizeReg, J9_GC_MINIMUM_OBJECT_SIZE, iCursor);
         iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, iCursor);
#endif

         // J9JavaVM (or realTimeSizeClasses when J9_CHANGES_PR95193 is defined) + rounded data size + SizeClassesIndexOffset is pointer to the size class
         iCursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, temp1Reg, dataSizeReg, temp3Reg, iCursor);

         //Now adjust dataSizeReg to only include the size of the array data in bytes, since this will be used in genInitArrayHeader();
         iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, dataSizeReg, dataSizeReg, -allocSize, iCursor);
         iCursor = generateTrg1MemInstruction(cg, TR::InstOpCode::Op_load, node, heapTopReg,
                                              new (cg->trHeapMemory()) TR::MemoryReference(temp1Reg, fej9->getSizeClassesIndexOffset(), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);
         // heapTopReg now holds size class

         // get next cell for this size class at vmThread + thisThreadAllocationCacheCurrentOffset(0) + (size class)*sizeof(J9VMGCSegregatedAllocationCacheEntry)
         TR_ASSERT(sizeof(J9VMGCSegregatedAllocationCacheEntry) == sizeof(UDATA*) * 2,
                   "J9VMGCSegregatedAllocationCacheEntry may need to be padded to avoid multiply in array access.");

         if (TR::Compiler->target.is64Bit())
            iCursor = generateShiftLeftImmediateLong(cg, node, temp1Reg, heapTopReg, trailingZeroes((int32_t)sizeof(J9VMGCSegregatedAllocationCacheEntry)), iCursor);
         else
            iCursor = generateShiftLeftImmediate(cg, node, temp1Reg, heapTopReg, trailingZeroes((int32_t)sizeof(J9VMGCSegregatedAllocationCacheEntry)), iCursor);

         iCursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, sizeReg, temp1Reg, metaReg, iCursor);
         iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, resReg,
                                              new (cg->trHeapMemory()) TR::MemoryReference(sizeReg, fej9->thisThreadAllocationCacheCurrentOffset(0), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);
         iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp1Reg,
                                              new (cg->trHeapMemory()) TR::MemoryReference(sizeReg, fej9->thisThreadAllocationCacheTopOffset(0), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);

         // if we've reached the top, then no cell available, use slow path
         iCursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, resReg, temp1Reg, iCursor);
         iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, node, callLabel, condReg, iCursor);

         // have a valid cell, need to update current cell pointer
         if (TR::Compiler->target.is64Bit())
            iCursor = generateShiftLeftImmediateLong(cg, node, temp1Reg, heapTopReg, trailingZeroes((int32_t)sizeof(UDATA)), iCursor);
         else
            iCursor = generateShiftLeftImmediate(cg, node, temp1Reg, heapTopReg, trailingZeroes((int32_t)sizeof(UDATA)), iCursor);

         iCursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, temp1Reg, temp1Reg, temp3Reg, iCursor);

         iCursor = generateTrg1MemInstruction(cg, TR::InstOpCode::Op_load, node, temp1Reg,
                                              new (cg->trHeapMemory()) TR::MemoryReference(temp1Reg, fej9->getSmallCellSizesOffset(), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);

         // temp1Reg now holds cell size
         iCursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, temp1Reg, temp1Reg, resReg, iCursor);

         //temp1Reg now holds new current cell pointer. Update it in memory:
         iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node,
                                              new (cg->trHeapMemory()) TR::MemoryReference(sizeReg, fej9->thisThreadAllocationCacheCurrentOffset(0), TR::Compiler->om.sizeofReferenceAddress(), cg), temp1Reg, iCursor);
         }
      else
         {
         // sizeClass will be bogus for variable length allocations because it only includes the header size (+ arraylet ptr for arrays)
         UDATA sizeClass = fej9->getObjectSizeClass(allocSize);

         iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, resReg, new (cg->trHeapMemory()) TR::MemoryReference(metaReg,
                     fej9->thisThreadAllocationCacheCurrentOffset(sizeClass), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);
         iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp2Reg, new (cg->trHeapMemory()) TR::MemoryReference(metaReg,
                     fej9->thisThreadAllocationCacheTopOffset(sizeClass), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);
         iCursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, resReg, temp2Reg, iCursor);
         iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, node, callLabel, condReg, iCursor);

         //now bump the current updatepointer
         if (fej9->getCellSizeForSizeClass(sizeClass) <= UPPER_IMMED)
            iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, temp1Reg, resReg, fej9->getCellSizeForSizeClass(sizeClass), iCursor);
         else
            {
            iCursor = loadConstant(cg, node, (int32_t)fej9->getCellSizeForSizeClass(sizeClass), temp1Reg, iCursor);
            iCursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, temp1Reg, temp1Reg, resReg, iCursor);
            }
         iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node,
                                              new (cg->trHeapMemory()) TR::MemoryReference(metaReg, fej9->thisThreadAllocationCacheCurrentOffset(sizeClass), TR::Compiler->om.sizeofReferenceAddress(), cg), temp1Reg, iCursor);
         }

      iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, zeroReg, 0, iCursor);

      //we're done
      return;
#endif
      } // if (TR::Options::getCmdLineOptions()->realTimeGC())
   else
      {
      uint32_t maxSafeSize = cg->getMaxObjectSizeGuaranteedNotToOverflow();

      bool generateArraylets = comp->generateArraylets();
      bool isArrayAlloc = (opCode == TR::newarray || opCode == TR::anewarray);

      if (isArrayAlloc)
         {
         if (TR::Compiler->om.useHybridArraylets())
            {
            int32_t maxContiguousArraySize = TR::Compiler->om.maxContiguousArraySizeInBytes() / elementSize;
            TR_ASSERT(maxContiguousArraySize >= 0, "Unexpected negative size for max contiguous array size");
            if (maxContiguousArraySize > UPPER_IMMED)
               {
               iCursor = loadConstant(cg, node, maxContiguousArraySize, temp2Reg, iCursor);
               iCursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::cmpli4, node, condReg, enumReg, temp2Reg, iCursor);
               }
            else
               iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, node, condReg, enumReg, maxContiguousArraySize, iCursor);

            iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, node, callLabel, condReg, iCursor);
            }

         if (isVariableLen)
            {  // Inline runtime zero length non-packed array allocation
               // Zero length arrays are discontiguous (i.e. they also need the discontiguous length field to be 0) because
               // they are indistinguishable from non-zero length discontiguous arrays
               iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpli4, node, condReg, enumReg, 0, iCursor);
               TR::LabelSymbol *nonZeroLengthLabel = generateLabelSymbol(cg);
               iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, nonZeroLengthLabel, condReg, iCursor);

               int32_t zeroLenArraySize = (sizeof(J9IndexableObjectDiscontiguous) + fej9->getObjectAlignmentInBytes() - 1) & (-fej9->getObjectAlignmentInBytes());
               TR_ASSERT(zeroLenArraySize >= J9_GC_MINIMUM_OBJECT_SIZE, "Zero-length array size must be bigger than MIN_OBJECT_SIZE");
               TR_ASSERT(zeroLenArraySize <= maxSafeSize, "Zero-length array size must be smaller than maxSafeSize");
               // Load TLH heapAlloc and heapTop values.
               iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, resReg,
                     new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapAlloc), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);
               iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, heapTopReg,
                     new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapTop), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);

               iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, temp2Reg, resReg, zeroLenArraySize, iCursor);
               iCursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, temp2Reg, heapTopReg, iCursor);
               iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, node, callLabel, condReg, iCursor);

               if (cg->comp()->compileRelocatableCode())
                  genInitObjectHeader(node, iCursor, clazz, classReg, resReg, zeroReg, condReg, heapTopReg, sizeReg, NULL, dependencies, needZeroInit, cg);
               else
                  genInitObjectHeader(node, iCursor, clazz, NULL, resReg, zeroReg, condReg, heapTopReg, sizeReg, NULL, dependencies, needZeroInit, cg);

               UDATA offsetSizeFields = fej9->getOffsetOfDiscontiguousArraySizeField();
               iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
                                         new (cg->trHeapMemory()) TR::MemoryReference(resReg, offsetSizeFields-4, 4, cg), enumReg, iCursor);
               iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
                                         new (cg->trHeapMemory()) TR::MemoryReference(resReg, offsetSizeFields, 4, cg), enumReg, iCursor);

               iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node,
                            new (cg->trHeapMemory()) TR::MemoryReference(metaReg,
                            offsetof(J9VMThread, heapAlloc), TR::Compiler->om.sizeofReferenceAddress(), cg), temp2Reg, iCursor);
               iCursor = generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);
               iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, nonZeroLengthLabel);
            }
         }

      if (usingTLH)
         {
         bool sizeInReg = (isVariableLen || (allocSize > UPPER_IMMED));
         bool shouldAlignToCacheBoundary = false;
         int32_t instanceBoundaryForAlignment = 64;

         static bool verboseDualTLH = feGetEnv("TR_verboseDualTLH") != NULL;

         if (isVariableLen)
            {
            // Detect large or negative number of elements in case addr wrap-around

            if (maxSafeSize < 0x00100000)
               {
               // NOTE: TR::InstOpCode::bge and the special shifts are used to cover every
               //       possible corner cases, with 32byte maximum object header.
               iCursor = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, condReg, enumReg, (maxSafeSize >> 6) << 2, iCursor);
               iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, node, callLabel, condReg, iCursor);
               }
            else
               {
               uintptrj_t mask;
               if (TR::Compiler->target.is64Bit())
                  {
                  mask = CONSTANT64(0xFFFFFFFFFFFFFFFF) << (27 - leadingZeroes(maxSafeSize));
                  iCursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicr_r, node, temp2Reg, enumReg, condReg, 0, mask, iCursor);
                  }
               else
                  {
                  mask = (0xFFFFFFFF << (11 - leadingZeroes(maxSafeSize))) & 0x0000FFFF;
                  iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andis_r, node, temp2Reg, enumReg, condReg, mask, iCursor);
                  }
               iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, callLabel, condReg, iCursor);
               }
            }

         //TODO: This code is never executed, check if this can be deleted now.
         if (!cg->isDualTLH())
            {
            //All of this code never gets executed because of the 0 && in
            //the inside if statement. Candidate for deletion

            if (!isVariableLen)
               {
               static char *disableAlign = feGetEnv("TR_DisableAlignAlloc");

               if (0 && !disableAlign && (node->getOpCodeValue() == TR::New) && (comp->getMethodHotness() >= hot || node->shouldAlignTLHAlloc()))
                  {
                  TR_OpaqueMethodBlock *ownMethod = node->getOwningMethod();

                  TR::Node *classChild = node->getFirstChild();
                  char * className = NULL;
                  TR_OpaqueClassBlock *clazz = NULL;

                  if (classChild && classChild->getSymbolReference() && !classChild->getSymbolReference()->isUnresolved())
                     {
                     TR::SymbolReference *symRef = classChild->getSymbolReference();
                     TR::Symbol *sym = symRef->getSymbol();

                     if (sym && sym->getKind() == TR::Symbol::IsStatic && sym->isClassObject())
                        {
                        TR::StaticSymbol * staticSym = symRef->getSymbol()->castToStaticSymbol();
                        void * staticAddress = staticSym->getStaticAddress();
                        if (symRef->getCPIndex() >= 0)
                           {
                           if (!staticSym->addressIsCPIndexOfStatic() && staticAddress)
                              {
                              int32_t len;
                              className = TR::Compiler->cls.classNameChars(comp, symRef, len);
                              clazz = (TR_OpaqueClassBlock *) staticAddress;
                              }
                           }
                        }
                     }

                  int32_t instanceSizeForAlignment = 56;
                  static char *alignSize = feGetEnv("TR_AlignInstanceSize");
                  static char *alignBoundary = feGetEnv("TR_AlignInstanceBoundary");

                  if (alignSize)
                     instanceSizeForAlignment = atoi(alignSize);
                  if (alignBoundary)
                     instanceBoundaryForAlignment = atoi(alignBoundary);

                  if (clazz && !cg->getCurrentEvaluationBlock()->isCold() && TR::Compiler->cls.classInstanceSize(clazz) >= instanceSizeForAlignment)
                     {
                     shouldAlignToCacheBoundary = true;

                     iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp1Reg,
                           new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapAlloc), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);

                     iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, resReg, temp1Reg, instanceBoundaryForAlignment - 1, iCursor);
                     if (TR::Compiler->target.is64Bit())
                        iCursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicr, node, resReg, resReg, 0, int64_t(-instanceBoundaryForAlignment), iCursor);
                     else
                        iCursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, resReg, resReg, 0, -instanceBoundaryForAlignment, iCursor);
                     }
                  }
               }

            if (!shouldAlignToCacheBoundary)
               {
               iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, resReg,
                     new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapAlloc), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);
               }
            iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, heapTopReg,
                  new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapTop), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);

            if (needZeroInit)
               iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, zeroReg, 0, iCursor);

            }
         else
            {
            //DualTLH support, use nonZeroTLH if optimizer says we can.

            if (node->canSkipZeroInitialization())
               {

               if (verboseDualTLH)
                  fprintf(stderr, "DELETEME genHeapAlloc useNonZeroTLH [%d] isVariableLen [%d] node: [%p]  %s [@%s]\n", 1, isVariableLen, node, comp->signature(),
                        comp->getHotnessName(comp->getMethodHotness()));

               //Load non-zeroed TLH heapAlloc and heapTop values.
               iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, resReg,
                     new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, nonZeroHeapAlloc), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);

               iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, heapTopReg,
                     new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, nonZeroHeapTop), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);

               }
            else
               {
               if (verboseDualTLH)
                  fprintf(stderr, "DELETEME genHeapAlloc useNonZeroTLH [%d] isVariableLen [%d] node: [%p]  %s [@%s]\n", 0, isVariableLen, node, comp->signature(),
                        comp->getHotnessName(comp->getMethodHotness()));

               //Load zeroed TLH heapAlloc and heapTop values.
               iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, resReg,
                     new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapAlloc), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);

               iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, heapTopReg,
                     new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapTop), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);
               }

            } //if(!cg->isDualTLH()) == false

         if (sizeInReg)
            {
            //Size will put put into sizeReg
            //Get the size of the object
            //See if we can put into a single arraylet, if not call the helper.
            //Add enough padding to make it a multiple of OBJECT_ALIGNMENT
            if (isVariableLen)
               {
               int32_t elementSize;
               if (comp->useCompressedPointers() && node->getOpCodeValue() == TR::anewarray)
                  elementSize = TR::Compiler->om.sizeofReferenceField();
               else
                  elementSize = TR::Compiler->om.getSizeOfArrayElement(node);

               // Check to see if the size of the array will fit in a single arraylet leaf.
               //
               if (generateArraylets && (node->getOpCodeValue() == TR::anewarray || node->getOpCodeValue() == TR::newarray))
                  {
                  int32_t arrayletLeafSize = TR::Compiler->om.arrayletLeafSize();
                  int32_t maxContiguousArrayletLeafSizeInBytes = arrayletLeafSize - TR::Compiler->om.sizeofReferenceAddress(); //need to add definition
                  int32_t maxArrayletSizeInElements = maxContiguousArrayletLeafSizeInBytes / elementSize;
                  iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, enumReg, maxArrayletSizeInElements, iCursor);
                  static const char *p = feGetEnv("TR_TarokPreLeafSizeCheckVarBreak");
                  if (p)
                     generateInstruction(cg, TR::InstOpCode::bad, node);

                  iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, node, callLabel, condReg, iCursor);
                  }

               int32_t round; // zero indicates no rounding is necessary
               round = (elementSize >= fej9->getObjectAlignmentInBytes()) ? 0 : fej9->getObjectAlignmentInBytes();
               bool headerAligned = allocSize % fej9->getObjectAlignmentInBytes() ? 0 : 1;

               //TODO: The code below pads up the object allocation size so that zero init code later
               //will have multiples of wordsize to work with. For now leaving this code as is, but
               //check if its worthwhile to remove these extra instuctions added here for padding as
               //zero init will be removed now.
               if (elementSize >= 4)
                  {
                  if (TR::Compiler->target.is64Bit())
                     iCursor = generateShiftLeftImmediateLong(cg, node, dataSizeReg, enumReg, trailingZeroes(elementSize), iCursor);
                  else
                     iCursor = generateShiftLeftImmediate(cg, node, dataSizeReg, enumReg, trailingZeroes(elementSize), iCursor);

                  }
               else if (elementSize == 2)
                  {
                  // RS64 processors prefer adds to shifts
                  iCursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, dataSizeReg, enumReg, enumReg, iCursor);
                  if (needZeroInit)
                     {
                     // the zero initialization code uses a loop of stwu's, and
                     // so dataSizeReg must be rounded up to a multiple of 4
                     iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, dataSizeReg, dataSizeReg, 3, iCursor);
                     if (TR::Compiler->target.is64Bit())
                        iCursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicr, node, dataSizeReg, dataSizeReg, 0, int64_t(-4), iCursor);
                     else
                        iCursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, dataSizeReg, dataSizeReg, 0, -4, iCursor);
                     }
                  }
               else if (needZeroInit) // elementSize == 1
                  {
                  // dataSizeReg must be set in this case for use by the zero
                  // initializing code, and it must be rounded up to a multiple
                  // of 4 (for the loop of stwu's)
                  iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, dataSizeReg, enumReg, 3, iCursor);
                  if (TR::Compiler->target.is64Bit())
                     iCursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicr, node, dataSizeReg, dataSizeReg, 0, int64_t(-4), iCursor);
                  else
                     iCursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, dataSizeReg, dataSizeReg, 0, -4, iCursor);
                  }

               if ((round != 0 && (!needZeroInit || round != 4)) || !headerAligned)
                  {
                  // Round the total size to a multiple of OBJECT_ALIGNMENT
                  if (elementSize == 1 && !needZeroInit)
                     iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, sizeReg, enumReg, allocSize + round - 1, iCursor);
                  else
                     iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, sizeReg, dataSizeReg, allocSize + round - 1, iCursor);
                  if (TR::Compiler->target.is64Bit())
                     iCursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicr, node, sizeReg, sizeReg, 0, int64_t(-round), iCursor);
                  else
                     iCursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, sizeReg, sizeReg, 0, -round, iCursor);
                  }
               else
                  {
                  if (elementSize == 1 && !needZeroInit)
                     iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, sizeReg, enumReg, allocSize, iCursor);
                  else
                     iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, sizeReg, dataSizeReg, allocSize, iCursor);
                  }

#ifdef J9VM_INTERP_FLAGS_IN_CLASS_SLOT
               if (!((node->getOpCodeValue() == TR::New) || (node->getOpCodeValue() == TR::anewarray) || (node->getOpCodeValue() == TR::newarray)))
                  {
                  iCursor = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, condReg, sizeReg, J9_GC_MINIMUM_OBJECT_SIZE, iCursor);
                  TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
                  if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
                  iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, PPCOpProp_BranchLikely, node, doneLabel, condReg, iCursor);
                  else
                  iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, node, doneLabel, condReg, iCursor);

                  iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, sizeReg, J9_GC_MINIMUM_OBJECT_SIZE, iCursor);
                  iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, iCursor);
                  }
#endif
               } //if (isVariableLen)
            else
               {
               // Check to see if the size of the array will fit in a single arraylet leaf.
               //
               if (generateArraylets && (node->getOpCodeValue() == TR::anewarray || node->getOpCodeValue() == TR::newarray))
                  {
                  int32_t arrayletLeafSize = TR::Compiler->om.arrayletLeafSize();

                  if (allocSize >= arrayletLeafSize + sizeof(J9IndexableObjectContiguous))
                     {
                     static const char *p = feGetEnv("TR_TarokPreLeafSizeCheckConstBreak");
                     if (p)
                        generateInstruction(cg, TR::InstOpCode::bad, node);

                     iCursor = generateLabelInstruction(cg, TR::InstOpCode::b, node, callLabel, iCursor);
                     }
                  }

               iCursor = loadConstant(cg, node, allocSize, sizeReg, iCursor);
               }
            } //if (sizeInReg)

         // Calculate the after-allocation heapAlloc: if the size is huge,
         //   we need to check address wrap-around also. This is unsigned
         //   integer arithmetic, checking carry bit is enough to detect it.
         //   For variable length array, we did an up-front check already.
         if (sizeInReg)
            iCursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, temp2Reg, resReg, sizeReg, iCursor);
         else
            iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, temp2Reg, resReg, allocSize, iCursor);

         //TODO: shouldAlignToCacheBoundary is never true, check its effects here.
         int32_t padding = shouldAlignToCacheBoundary ? instanceBoundaryForAlignment : 0;

         if (!isVariableLen && ((uint32_t) allocSize + padding) > maxSafeSize)
            {
            if (!shouldAlignToCacheBoundary)
               iCursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, temp2Reg, resReg, iCursor);
            else
               iCursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, temp2Reg, temp1Reg, iCursor);
            iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, callLabel, condReg, iCursor);
            }

         // We need to make sure that the TLH pointer is bumped correctly to support the Arraylet header.
         //
         if (generateArraylets && (node->getOpCodeValue() == TR::anewarray || node->getOpCodeValue() == TR::newarray))
            {
            iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, temp2Reg, temp2Reg, fej9->getObjectAlignmentInBytes() - 1, iCursor);
            iCursor = generateTrg1Src1Imm2Instruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::rldicr : TR::InstOpCode::rlwinm, node, temp2Reg, temp2Reg, 0, int64_t(-fej9->getObjectAlignmentInBytes()), iCursor);
            static const char *p = feGetEnv("TR_TarokAlignHeapTopBreak");
            if (p)
               iCursor = generateInstruction(cg, TR::InstOpCode::bad, node, iCursor);
            }

         // Ok, temp2Reg now points to where the object will end on the TLH.
         //    resReg will contain the start of the object where we'll write out our
         //    J9Class*. Should look like this in memory:
         //    [heapAlloc == resReg] ... temp2Reg ...//... heapTopReg.

         //Here we check if we overflow the TLH Heap Top (heapTop, or nonZeroHeapTop)
         //branch to regular heapAlloc Snippet if we overflow (ie callLabel).
         iCursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, temp2Reg, heapTopReg, iCursor);
         iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, node, callLabel, condReg, iCursor);

         //TODO: this code is never executed, check if we can remove this now.
         if (!cg->isDualTLH())
            {
            //shouldAlignToCacheBoundary is false at defintion at the top, and
            //the only codepoint where its set to true is never executed
            //so this looks like a candidate for deletion.
            if (shouldAlignToCacheBoundary)
               {
               TR::LabelSymbol *doneAlignLabel = generateLabelSymbol(cg);
               TR::LabelSymbol *multiSlotGapLabel = generateLabelSymbol(cg);
               ;
               iCursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, dataSizeReg, temp1Reg, resReg, iCursor);

               if (sizeInReg)
                  iCursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, sizeReg, dataSizeReg, sizeReg, iCursor);
               else
                  iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, sizeReg, dataSizeReg, allocSize, iCursor);

               sizeInReg = true;

               iCursor = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, condReg, dataSizeReg, sizeof(uintptrj_t), iCursor);
               iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, doneAlignLabel, condReg, iCursor);
               iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, node, multiSlotGapLabel, condReg, iCursor);
               iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, dataSizeReg, J9_GC_SINGLE_SLOT_HOLE, iCursor);

               if (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord())
                  {
                  iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(temp1Reg, 0, 4, cg), dataSizeReg, iCursor);
                  iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(temp1Reg, 4, 4, cg), dataSizeReg, iCursor);
                  }
               else
                  {
                  iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, new (cg->trHeapMemory()) TR::MemoryReference(temp1Reg, 0, TR::Compiler->om.sizeofReferenceAddress(), cg), dataSizeReg,
                        iCursor);
                  }

               iCursor = generateLabelInstruction(cg, TR::InstOpCode::b, node, doneAlignLabel, iCursor);
               iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, multiSlotGapLabel, iCursor);
               iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, new (cg->trHeapMemory()) TR::MemoryReference(temp1Reg, TR::Compiler->om.sizeofReferenceAddress(), TR::Compiler->om.sizeofReferenceAddress(), cg),
                     dataSizeReg, iCursor);
               iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, dataSizeReg, J9_GC_MULTI_SLOT_HOLE, iCursor);
               iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, new (cg->trHeapMemory()) TR::MemoryReference(temp1Reg, 0, TR::Compiler->om.sizeofReferenceAddress(), cg), dataSizeReg,
                     iCursor);
               iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, doneAlignLabel, iCursor);
               }
            }

         if (cg->enableTLHPrefetching())
            {
            //Decide between zeroed and non-zero TLH'es

            if (cg->isDualTLH() && node->canSkipZeroInitialization())
               {

               if (verboseDualTLH)
                  fprintf(stderr, "DELETEME genHeapAlloc PREFETCH useNonZeroTLH [%d] isVariableLen [%d] node: [%p]  %s [@%s]\n", 1, isVariableLen, node, comp->signature(),
                        comp->getHotnessName(comp->getMethodHotness()));

               iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp1Reg,
                     new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, nonZeroTlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);
               }
            else
               {
               if (verboseDualTLH)
                  fprintf(stderr, "DELETEME genHeapAlloc PREFETCH useNonZeroTLH [%d] isVariableLen [%d] node: [%p]  %s [@%s]\n", 0, isVariableLen, node, comp->signature(),
                        comp->getHotnessName(comp->getMethodHotness()));

               iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp1Reg,
                     new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, tlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);
               }

            if (sizeInReg)
               iCursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf_r, node, temp1Reg, sizeReg, temp1Reg, condReg, iCursor);
            else
               iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic_r, node, temp1Reg, temp1Reg, condReg, -allocSize, iCursor);

            //Write back and allocate Snippet if needed.
            TR::LabelSymbol * callLabel = NULL;
            if (cg->isDualTLH() && node->canSkipZeroInitialization())
               {
               iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node,
                     new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, nonZeroTlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg), temp1Reg, iCursor);

               callLabel = cg->lookUpSnippet(TR::Snippet::IsNonZeroAllocPrefetch, NULL);
               if (callLabel == NULL)
                  {
                  callLabel = generateLabelSymbol(cg);
                  TR::Snippet *snippet = new (cg->trHeapMemory()) TR::PPCNonZeroAllocPrefetchSnippet(cg, node, callLabel);
                  cg->addSnippet(snippet);
                  }
               }
            else
               {
               iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node,
                     new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, tlhPrefetchFTA), TR::Compiler->om.sizeofReferenceAddress(), cg), temp1Reg, iCursor);

               callLabel = cg->lookUpSnippet(TR::Snippet::IsAllocPrefetch, NULL);
               if (callLabel == NULL)
                  {
                  callLabel = generateLabelSymbol(cg);
                  TR::Snippet *snippet = new (cg->trHeapMemory()) TR::PPCAllocPrefetchSnippet(cg, node, callLabel);
                  cg->addSnippet(snippet);
                  }
               }

            // kills temp1Reg and sizeReg
            if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
               // use PPC AS branch hint
               iCursor = generateDepConditionalBranchInstruction(cg, TR::InstOpCode::blel, PPCOpProp_BranchUnlikely, node, callLabel, condReg, dependencies, iCursor);
            else
               iCursor = generateDepConditionalBranchInstruction(cg, TR::InstOpCode::blel, node, callLabel, condReg, dependencies, iCursor);

            cg->machine()->setLinkRegisterKilled(true);
            }

         //Done, write back to heapAlloc (zero or nonZero TLH) here.
         if (cg->isDualTLH() && node->canSkipZeroInitialization())
            {
            iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node,
                  new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, nonZeroHeapAlloc), TR::Compiler->om.sizeofReferenceAddress(), cg), temp2Reg, iCursor);
            }
         else
            {
            iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node,
                  new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapAlloc), TR::Compiler->om.sizeofReferenceAddress(), cg), temp2Reg, iCursor);
            }
         }
      else
         {
         bool sizeInReg = (allocSize > UPPER_IMMED);
         TR::LabelSymbol *tryLabel = generateLabelSymbol(cg);

         iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, heapTopReg,
               new (cg->trHeapMemory()) TR::MemoryReference(temp1Reg, offsetof(J9MemorySegment, heapTop), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);
         iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, dataSizeReg, temp1Reg, offsetof(J9MemorySegment, heapAlloc), iCursor);
         iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, zeroReg, 0, iCursor);
         if (sizeInReg)
            {
            iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, sizeReg, (allocSize >> 16) + ((allocSize & (1 << 15)) ? 1 : 0), iCursor);
            iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, sizeReg, sizeReg, allocSize & 0x0000FFFF, iCursor);
            }

         // Try to allocate
         iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, tryLabel, iCursor);
         iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, resReg, new (cg->trHeapMemory()) TR::MemoryReference(dataSizeReg, 0, TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);

         // Calculate the after-allocation heapAlloc: if the size is huge,
         //   we need to check address wrap-around also. This is unsigned
         //   integer arithmetic, checking carry bit is enough to detect it.
         if ((uint32_t) allocSize > maxSafeSize)
            {
            if (sizeInReg)
               {
               iCursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, temp1Reg, resReg, sizeReg, iCursor);
               }
            else
               {
               iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, temp1Reg, resReg, allocSize, iCursor);
               }

            iCursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, temp1Reg, resReg, iCursor);

            iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, callLabel, condReg, iCursor);
            }
         else
            {
            if (sizeInReg)
               {
               iCursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, temp1Reg, resReg, sizeReg, iCursor);
               }
            else
               {
               iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, temp1Reg, resReg, allocSize, iCursor);
               }
            }

         iCursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, temp1Reg, heapTopReg, iCursor);
         iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, node, callLabel, condReg, iCursor);

         // todo64: I think these need to be ldarx/stdcx. in 64-bit mode
         iCursor = generateTrg1MemInstruction(cg, TR::InstOpCode::lwarx, node, temp2Reg, new (cg->trHeapMemory()) TR::MemoryReference(dataSizeReg, zeroReg, 4, cg), iCursor);
         iCursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::cmpl4, node, condReg, temp2Reg, resReg, iCursor);
         iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, tryLabel, condReg, iCursor);
         iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stwcx_r, node, new (cg->trHeapMemory()) TR::MemoryReference(dataSizeReg, zeroReg, 4, cg), temp1Reg, iCursor);

         if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
            // use PPC AS branch hint
            iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, node, tryLabel, condReg, iCursor);
         else
            iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, tryLabel, condReg, iCursor);
         }
      }
   }


static void genInitObjectHeader(TR::Node *node, TR::Instruction *&iCursor, TR_OpaqueClassBlock *clazz, TR::Register *classReg, TR::Register *resReg, TR::Register *zeroReg,
      TR::Register *condReg, TR::Register *temp1Reg, TR::Register *temp2Reg, TR::Register * packedClobberedOffset, TR::RegisterDependencyConditions *conditions, bool needZeroInit,
      TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   J9ROMClass *romClass = 0;
   uint32_t staticFlag = 0;
   uint32_t orFlag = 0;
   TR::Compilation *comp = cg->comp();

   TR_ASSERT(clazz, "Cannot have a null OpaqueClassBlock\n");
   romClass = TR::Compiler->cls.romClassOf(clazz);
   staticFlag = romClass->instanceShape;
   TR::Register *metaReg = cg->getMethodMetaDataRegister();

   TR::Register * clzReg = classReg;

   if (comp->compileRelocatableCode())
      {
      if (node->getOpCodeValue() == TR::newarray)
         {
         iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp1Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, javaVM), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);
         iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp1Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(temp1Reg, ((TR_J9VM *) fej9)->getPrimitiveArrayOffsetInJavaVM(node->getSecondChild()->getInt()),
                     TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);
         clzReg = temp1Reg;
         }
      else if (node->getOpCodeValue() == TR::anewarray)
         {
         TR_ASSERT(classReg, "must have a classReg for TR::anewarray in AOT mode");
         iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp1Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(classReg, offsetof(J9Class, arrayClass), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);
         clzReg = temp1Reg;
         }
      else
         {
         TR_ASSERT(node->getOpCodeValue() == TR::New && classReg, "must have a classReg for TR::New in AOT mode");
         clzReg = classReg;
         }
      }

   // Store the class
   if (clzReg == NULL)
      {
      // HCR in genInitObjectHeader
      if (cg->wantToPatchClassPointer(clazz, node))
         {
         iCursor = loadAddressConstantInSnippet(cg, node, (int64_t) clazz, temp1Reg, temp2Reg,TR::InstOpCode::Op_load, false, iCursor);
         iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, temp1Reg, temp1Reg, orFlag, iCursor);
         }
      else
         {
#ifdef TR_TARGET_64BIT
         int32_t offset;
         intptrj_t classPtr = (intptrj_t)clazz;

         offset = TR_PPCTableOfConstants::lookUp((int8_t *)&classPtr, sizeof(intptrj_t), true, 0, cg);

         if (offset != PTOC_FULL_INDEX)
            {
            offset *= TR::Compiler->om.sizeofReferenceAddress();
            if (TR_PPCTableOfConstants::getTOCSlot(offset) == 0)
            TR_PPCTableOfConstants::setTOCSlot(offset, (int64_t)clazz);
            if (offset<LOWER_IMMED||offset>UPPER_IMMED)
               {
               iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, temp1Reg, cg->getTOCBaseRegister(), HI_VALUE(offset), iCursor);
               iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp1Reg, new (cg->trHeapMemory()) TR::MemoryReference(temp1Reg, LO_VALUE(offset), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);
               }
            else
               {
               iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp1Reg, new (cg->trHeapMemory()) TR::MemoryReference(cg->getTOCBaseRegister(), offset, TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);
               }
            if (orFlag != 0)
               {
               iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, temp1Reg, temp1Reg, orFlag, iCursor);
               }
            }
         else
            {
            iCursor = loadConstant(cg, node, (int64_t)clazz|(int64_t)orFlag, temp1Reg, iCursor);
            }
#else
         iCursor = loadConstant(cg, node, (int32_t) clazz | (int32_t) orFlag, temp1Reg, iCursor);
#endif /* TR_TARGET_64BIT */
         }
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
      // must store only 32 bits
      iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
            new (cg->trHeapMemory()) TR::MemoryReference(resReg, (int32_t)TR::Compiler->om.offsetOfObjectVftField(), 4, cg),
            temp1Reg, iCursor);
#else
      iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node,
            new (cg->trHeapMemory()) TR::MemoryReference(resReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceAddress(), cg), temp1Reg, iCursor);
#endif
      }
   else
      {
      if (orFlag != 0)
         {
         iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, clzReg, clzReg, orFlag, iCursor);
         }
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
      // must store only 32 bits
      iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
            new (cg->trHeapMemory()) TR::MemoryReference(resReg, (int32_t)TR::Compiler->om.offsetOfObjectVftField(), 4, cg),
            clzReg, iCursor);
#else
      iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node,
            new (cg->trHeapMemory()) TR::MemoryReference(resReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceAddress(), cg), clzReg, iCursor);
#endif
      }


#ifndef J9VM_INTERP_FLAGS_IN_CLASS_SLOT

   bool isStaticFlag = fej9->isStaticObjectFlags();
   if (isStaticFlag)
      {
      // The object flags can be determined at compile time.
      staticFlag |= fej9->getStaticObjectFlags();
      if (staticFlag != 0 || needZeroInit)
         {
         iCursor = loadConstant(cg, node, (int32_t) staticFlag, temp2Reg, iCursor);
         // Store the flags
         iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(resReg, (int32_t) TMP_OFFSETOF_J9OBJECT_FLAGS, 4, cg), temp2Reg,
               iCursor);
         }
      }

   else if (!TR::Options::getCmdLineOptions()->realTimeGC())
      {
      // If the object flags cannot be determined at compile time, we add a load for it.
      //
      iCursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp2Reg,
            new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, allocateThreadLocalHeap.objectFlags), TR::Compiler->om.sizeofReferenceAddress(), cg), iCursor);

      // OR staticFlag with temp2Reg.
      // For now, only the lower 16 bits are set in staticFlag. But check the higher 16 bits just in case.
      if (staticFlag != 0)
         {
         if ((staticFlag & 0xFFFF0000) != 0)
            {
            iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::oris, node, temp2Reg, temp2Reg, (staticFlag >> 16) & 0x0000FFFF, iCursor);
            }
         if ((staticFlag & 0x0000FFFF) != 0)
            {
            iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, temp2Reg, temp2Reg, staticFlag & 0x0000FFFF, iCursor);
            }
         }
      iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(resReg, (int32_t) TMP_OFFSETOF_J9OBJECT_FLAGS, 4, cg), temp2Reg,
            iCursor);
      }

#endif /*J9VM_INTERP_FLAGS_IN_CLASS_SLOT*/

   TR::InstOpCode::Mnemonic opCode;
   int32_t lockSize;

   if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
      {
      opCode = TR::InstOpCode::std;
      lockSize = 8;
      }
   else
      {
      opCode = TR::InstOpCode::stw;
      lockSize = 4;
      }

#if !defined(J9VM_THR_LOCK_NURSERY)
   // Init monitor
   if (needZeroInit)
      iCursor = generateMemSrc1Instruction(cg, opCode, node, new (cg->trHeapMemory()) TR::MemoryReference(resReg, TMP_OFFSETOF_J9OBJECT_MONITOR, lockSize, cg), zeroReg, iCursor);
#endif
#if defined(J9VM_THR_LOCK_NURSERY) && defined(J9VM_THR_LOCK_NURSERY_FAT_ARRAYS)
   // Initialize the monitor
   // word for arrays that have a
   // lock word
   int32_t lwOffset = fej9->getByteOffsetToLockword(clazz);
   if (needZeroInit &&
         (node->getOpCodeValue() != TR::New) &&
         (lwOffset > 0) && !fej9->isPackedClass(classAddress))
   iCursor = generateMemSrc1Instruction(cg, opCode, node,
         new (cg->trHeapMemory()) TR::MemoryReference(resReg, TMP_OFFSETOF_J9INDEXABLEOBJECT_MONITOR, lockSize, cg),
         zeroReg, iCursor);
#endif
   }

static void genAlignArray(TR::Node *node, TR::Instruction *&iCursor, bool isVariableLen, TR::Register *resReg, int32_t objectSize, int32_t dataBegin, TR::Register *dataSizeReg,
      TR::Register *condReg, TR::Register *temp1Reg, TR::Register *temp2Reg, TR::CodeGenerator *cg)
   {
   TR::LabelSymbol *slotAtStart = generateLabelSymbol(cg);
   TR::LabelSymbol *doneAlign = generateLabelSymbol(cg);

   iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, temp1Reg, resReg, condReg, 7, iCursor);
   iCursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, temp2Reg, 3, iCursor);
   iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, slotAtStart, condReg, iCursor);

   // The slop bytes are at the end of the allocated object.
   if (isVariableLen)
      {
      iCursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, temp1Reg, resReg, dataSizeReg, iCursor);
      iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(temp1Reg, dataBegin, 4, cg), temp2Reg, iCursor);
      }
   else if (objectSize > UPPER_IMMED)
      {
      iCursor = loadConstant(cg, node, objectSize, temp1Reg, iCursor);
      iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(resReg, temp1Reg, 4, cg), temp2Reg, iCursor);
      }
   else
      {
      iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(resReg, objectSize, 4, cg), temp2Reg, iCursor);
      }
   iCursor = generateLabelInstruction(cg, TR::InstOpCode::b, node, doneAlign, iCursor);

   // the slop bytes are at the start of the allocation
   iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, slotAtStart, iCursor);
   iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(resReg, (int32_t) 0, 4, cg), temp2Reg, iCursor);
   iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, resReg, resReg, 4, iCursor);
   iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, doneAlign, iCursor);
   }

static void genInitArrayHeader(TR::Node *node, TR::Instruction *&iCursor, bool isVariableLen, TR_OpaqueClassBlock *clazz, TR::Register *classReg, TR::Register *resReg,
      TR::Register *zeroReg, TR::Register *condReg, TR::Register *eNumReg, TR::Register *dataSizeReg, TR::Register *temp1Reg, TR::Register *temp2Reg,
      TR::RegisterDependencyConditions *conditions, bool needZeroInit, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   TR::Register *instanceSizeReg;

   genInitObjectHeader(node, iCursor, clazz, classReg, resReg, zeroReg, condReg, temp1Reg, temp2Reg, NULL, conditions, needZeroInit, cg);

   instanceSizeReg = eNumReg;

   if ((node->getOpCodeValue() == TR::newarray || node->getOpCodeValue() == TR::anewarray) && node->getFirstChild()->getOpCode().isLoadConst() && (node->getFirstChild()->getInt() == 0))
      {// constant zero length non-packed array
       // Zero length arrays are discontiguous (i.e. they also need the discontiguous length field to be 0) because
       // they are indistinguishable from non-zero length discontiguous arrays
      iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
                                            new (cg->trHeapMemory()) TR::MemoryReference(resReg, fej9->getOffsetOfDiscontiguousArraySizeField()-4, 4, cg),
                                            instanceSizeReg, iCursor);
      iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
                                            new (cg->trHeapMemory()) TR::MemoryReference(resReg, fej9->getOffsetOfDiscontiguousArraySizeField(), 4, cg),
                                            instanceSizeReg, iCursor);
      }
   else
      {
      // Store the array size
      iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
                                           new (cg->trHeapMemory()) TR::MemoryReference(resReg, fej9->getOffsetOfContiguousArraySizeField(), 4, cg),
                                           instanceSizeReg, iCursor);
      }
   }

static void genZeroInit(TR::CodeGenerator *cg, TR::Node *node, TR::Register *objectReg, int32_t headerSize, int32_t totalSize, bool useInitInfo)
   {
   TR_ASSERT((totalSize - headerSize > 0) && ((totalSize - headerSize) % 4 == 0), "Expecting non-zero word-aligned data size");

   TR::Register *zeroReg = cg->allocateRegister();
   TR::Compilation* comp = cg->comp();

   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, zeroReg, 0);

   // Perform initialization if it is needed:
   //   1) Initialize certain array elements individually. This depends on the optimizer
   //      providing a "short" list of individual indices;
   //   2) Initialize the whole array:
   //      a) If the object size is constant and small use straight line code;
   //      b) For large objects and variable length arrays, do nothing, it was handled in the allocation helper.
   if (useInitInfo)
      {
      TR_ExtraInfoForNew *initInfo = node->getSymbolReference()->getExtraInfo();

      TR_ASSERT(initInfo && initInfo->zeroInitSlots && initInfo->numZeroInitSlots > 0, "Expecting valid init info");

      TR_BitVectorIterator bvi(*initInfo->zeroInitSlots);
      if (TR::Compiler->target.is64Bit())
         {
         int32_t lastNotInit = -1;
         int32_t currElem = -1;

         // Try to group set of words into smallest groupings using double-word stores (where possible)
         while (bvi.hasMoreElements())
            {
            currElem = bvi.getNextElement();

            if (lastNotInit == -1)
               {
               // currently looking at only one slot (either just starting, or just emitted double-word store in last iter)
               lastNotInit = currElem;
               }
            else if (currElem == lastNotInit + 1)
               {
               // consecutive slots, can use std
               generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, new (cg->trHeapMemory()) TR::MemoryReference(objectReg, headerSize + lastNotInit * 4, 8, cg), zeroReg);
               lastNotInit = -1;
               }
            else
               {
               // Two non-consecutive slots, use stw for earlier slot and keep looking for slot
               // consecutive with the second slot currently held
               generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(objectReg, headerSize + lastNotInit * 4, 4, cg), zeroReg);
               lastNotInit = currElem;
               }
            }

         if (lastNotInit != -1)
            {
            // Last slot is not consecutive with other slots, hasn't been initialized yet
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(objectReg, headerSize + lastNotInit * 4, 4, cg), zeroReg);
            }
         }
      else
         {
         while (bvi.hasMoreElements())
            {
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
                  new (cg->trHeapMemory()) TR::MemoryReference(objectReg, headerSize + bvi.getNextElement() * TR::Compiler->om.sizeofReferenceAddress(), TR::Compiler->om.sizeofReferenceAddress(), cg), zeroReg);
            }
         }
      }
   else
      {
      int32_t ofs;
      for (ofs = headerSize; ofs < totalSize - TR::Compiler->om.sizeofReferenceAddress(); ofs += TR::Compiler->om.sizeofReferenceAddress())
         {
         generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, new (cg->trHeapMemory()) TR::MemoryReference(objectReg, ofs, TR::Compiler->om.sizeofReferenceAddress(), cg), zeroReg);
         }
      if (ofs + 4 == totalSize)
         generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(objectReg, ofs, 4, cg), zeroReg);
      else if (ofs + 8 == totalSize)
         generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, new (cg->trHeapMemory()) TR::MemoryReference(objectReg, ofs, 8, cg), zeroReg);
      }

   // Scheduling barrier
   generateLabelInstruction(cg, TR::InstOpCode::label, node, generateLabelSymbol(cg));

   cg->stopUsingRegister(zeroReg);
   }

TR::Register *outlinedHelperNewEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
#if (defined(__IBMCPP__) || defined(__IBMC__)) && (!defined(__ibmxl__))
     // __func__ is not defined for this function on XLC compilers (Notably XLC on Linux PPC and ZOS)
     static const char __func__[] = "outlinedHelperNewEvaluator";
#endif
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   const TR::ILOpCodes opCode = node->getOpCodeValue();
   const bool isArray = opCode != TR::New;
   TR_OpaqueClassBlock *clazz;
   const int32_t objectSizeBytes = comp->canAllocateInline(node, clazz);
   bool doInline = true;

   TR_ASSERT(!isArray || opCode == TR::newarray || opCode == TR::anewarray, "Unexpected op for new evaluator");

   if (objectSizeBytes < 0)
      doInline = false;
   else if (isArray && TR::Compiler->om.useHybridArraylets() && comp->getOption(TR_DisableTarokInlineArrayletAllocation))
      doInline = false;

   if (OMR_UNLIKELY(!doInline))
      {
      TR::Node::recreate(node, TR::acall);
      TR::Register *callResult = TR::TreeEvaluator::directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return callResult;
      }

   //
   // Inline allocation starts here
   //

   const bool isVariableLen = objectSizeBytes == 0;
   const int32_t allocSizeBytes = isVariableLen ? sizeof(J9IndexableObjectContiguous) : (objectSizeBytes + fej9->getObjectAlignmentInBytes() - 1) & (-fej9->getObjectAlignmentInBytes());
   const bool needsZeroInit = !fej9->tlhHasBeenCleared() && !node->canSkipZeroInitialization();
   // Only zero-init inline if the object size is known and relatively small, otherwise do in the helper
   const int32_t maxInitSlots = 8;
   const bool useInitInfo = node->getSymbolReference()->getExtraInfo() && (node->getSymbolReference()->getExtraInfo())->zeroInitSlots
         && (node->getSymbolReference()->getExtraInfo())->numZeroInitSlots > 0
         && (node->getSymbolReference()->getExtraInfo())->numZeroInitSlots <= maxInitSlots;
   const bool doZeroInitInline = needsZeroInit && !isVariableLen && (useInitInfo || (objectSizeBytes <= TR::Compiler->om.sizeofReferenceAddress() * maxInitSlots));

   TR::Instruction *appendInstruction = cg->getAppendInstruction();

   TR_ASSERT(objectSizeBytes >= J9_GC_MINIMUM_OBJECT_SIZE || isVariableLen, "Expecting object to be at least minimum required by GC");
   TR_ASSERT((objectSizeBytes & TR::Compiler->om.sizeofReferenceAddress() - 1) == 0, "Expecting object size to be aligned");
   TR_ASSERT(((allocSizeBytes & fej9->getObjectAlignmentInBytes() - 1) == 0 || isVariableLen), "Expecting allocation to be aligned");

   // Scheduling barrier
   generateLabelInstruction(cg, TR::InstOpCode::label, node, generateLabelSymbol(cg));

   // Class reg (r3) is clobbered by the helper to return the newly allocated object
   TR::Register *classReg = cg->gprClobberEvaluate(node->getLastChild());
   TR::Register *objectReg = cg->allocateCollectedReferenceRegister();
   TR::Register *gr11 = cg->allocateRegister();
   TR::CodeCache *codeCache = comp->getCurrentCodeCache();
   uintptrj_t helperAddr;
   TR::Instruction *gcPoint;

   TR_PPCScratchRegisterDependencyConditions preDeps, postDeps;

   preDeps.addDependency(cg, classReg, TR::RealRegister::gr3);
   postDeps.addDependency(cg, objectReg, TR::RealRegister::gr3);
   // Clobbered by the helper
   postDeps.addDependency(cg, NULL, TR::RealRegister::cr0);
   postDeps.addDependency(cg, NULL, TR::RealRegister::cr1);
   postDeps.addDependency(cg, NULL, TR::RealRegister::cr2);
   // Clobbered by the helper, and used in the zero-init case
   postDeps.addDependency(cg, gr11, TR::RealRegister::gr11);

   bool useNonZeroTLH = cg->isDualTLH() && node->canSkipZeroInitialization();
   static bool verboseDualTLH = feGetEnv("TR_verboseDualTLH") != NULL;

   TR_Debug *debugObj = cg->getDebug();

   //In Snippet, if (r11 == 1) --> do nonZeroTLHAlloc, else do zeroTLHAlloc
   TR::Instruction* tlhHeapSelector = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, gr11, useNonZeroTLH ? 1 : 0);

   if (debugObj)
      debugObj->addInstructionComment(tlhHeapSelector, "Toggle between zeroInitialized (gr11 <-- 0) and non-zeroIntialized (gr11 <-- 1) TLHs.");

   if (!isArray)
      {
      if (verboseDualTLH)
         fprintf(stderr, "DELETEME outlinedHelperNewEvaluator useNonZeroTLH [%d] isArray [%d] node: [%p] %s [@%s]\n", useNonZeroTLH, isArray, node, comp->signature(),
               comp->getHotnessName(comp->getMethodHotness()));

      TR::Register *allocSizeBytesReg = cg->allocateRegister();

      postDeps.addDependency(cg, allocSizeBytesReg, TR::RealRegister::gr4);
      // Clobbered by the helper
      if (cg->enableTLHPrefetching())
         postDeps.addDependency(cg, NULL, TR::RealRegister::gr8);
      postDeps.addDependency(cg, NULL, TR::RealRegister::gr10);

      loadConstant(cg, node, allocSizeBytes, allocSizeBytesReg);
      helperAddr = (uintptrj_t) codeCache->getCCPreLoadedCodeAddress(TR_ObjAlloc, cg);
      gcPoint = generateDepImmSymInstruction(cg, TR::InstOpCode::bl, node, helperAddr, TR_PPCScratchRegisterDependencyConditions::createDependencyConditions(cg, &preDeps, &postDeps),
      // Grab the right symref rather than using node->getSymbolReference(), which will refer to jitNewObject
            comp->getSymRefTab()->findOrCreatePerCodeCacheHelperSymbolRef(TR_ObjAlloc, helperAddr));

      cg->stopUsingRegister(allocSizeBytesReg);
      } //if (!isArray)
   else
      {

      if (verboseDualTLH)
         fprintf(stderr, "DELETEME outlinedHelperNewEvaluator useNonZeroTLH [%d] isArray [%d] isVariableLen [%d] node: [%p]  %s [@%s]\n", useNonZeroTLH, isArray, isVariableLen,
               node, comp->signature(), comp->getHotnessName(comp->getMethodHotness()));

      TR::Register *numElementsReg = cg->evaluate(node->getFirstChild());
      TR::Register *allocSizeBytesReg = cg->allocateRegister();
      TR::Register *arrayClassReg = cg->allocateRegister();
      TR::Register *gr10 = cg->allocateRegister();

      postDeps.addDependency(cg, numElementsReg, TR::RealRegister::gr4);
      postDeps.addDependency(cg, allocSizeBytesReg, TR::RealRegister::gr5);
      postDeps.addDependency(cg, arrayClassReg, TR::RealRegister::gr8);

      //TODO: What HCR case?
      // Clobbered by the helper, and used in the HCR case
      postDeps.addDependency(cg, gr10, TR::RealRegister::gr10);

      if (isVariableLen)
         {
         const uint32_t elementSize = TR::Compiler->om.getSizeOfArrayElement(node);
         switch (elementSize)
            {
         case 2:
            // RS64 prefers addition to shifts
            generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, allocSizeBytesReg, numElementsReg, numElementsReg);
            break;
         case 1:
            generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, allocSizeBytesReg, numElementsReg);
            break;
         default:
            TR_ASSERT(elementSize != 0 && (elementSize & 3) == 0, "Expecting element size to be a multiple of 4");
            if (TR::Compiler->target.is64Bit())
               generateShiftLeftImmediateLong(cg, node, allocSizeBytesReg, numElementsReg, trailingZeroes(elementSize));
            else
               generateShiftLeftImmediate(cg, node, allocSizeBytesReg, numElementsReg, trailingZeroes(elementSize));
            }
         }
      else
         loadConstant(cg, node, allocSizeBytes, allocSizeBytesReg);

      if (cg->wantToPatchClassPointer(clazz, node))
         loadAddressConstantInSnippet(cg, node, (intptrj_t) clazz, arrayClassReg, gr10,TR::InstOpCode::Op_load, false);
      else
         loadAddressConstant(cg, node, (intptrj_t) clazz, arrayClassReg);

      TR_CCPreLoadedCode helper = isVariableLen ? TR_VariableLenArrayAlloc : TR_ConstLenArrayAlloc;
      helperAddr = (uintptrj_t) codeCache->getCCPreLoadedCodeAddress(helper, cg);
      gcPoint = generateDepImmSymInstruction(cg, TR::InstOpCode::bl, node, helperAddr, TR_PPCScratchRegisterDependencyConditions::createDependencyConditions(cg, &preDeps, &postDeps),
      // Grab the right symref rather than using node->getSymbolReference(), which will refer to jitNewArray
            comp->getSymRefTab()->findOrCreatePerCodeCacheHelperSymbolRef(helper, helperAddr));

      cg->stopUsingRegister(allocSizeBytesReg);
      cg->stopUsingRegister(arrayClassReg);
      cg->stopUsingRegister(gr10);
      }

   gcPoint->PPCNeedsGCMap(0xFFFFFFFF);
   cg->machine()->setLinkRegisterKilled(true);
   cg->setHasCall();

   cg->stopUsingRegister(gr11);
   if (node->getLastChild()->getRegister() != classReg)
      cg->stopUsingRegister(classReg);
   cg->decReferenceCount(node->getFirstChild());
   if (isArray)
      cg->decReferenceCount(node->getSecondChild());

   node->setRegister(objectReg);

   if (OMR_UNLIKELY(cg->comp()->compileRelocatableCode()) && opCode != TR::newarray)
      {
      TR_ASSERT(opCode == TR::New || opCode == TR::anewarray, "Unexpected op for new evaluator AOT relo");
      TR::Instruction *firstInstruction = appendInstruction->getNext();
      TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation *) comp->trMemory()->allocateMemory(sizeof(TR_RelocationRecordInformation), heapAlloc);
      recordInfo->data1 = allocSizeBytes;
      recordInfo->data2 = node->getInlinedSiteIndex();
      recordInfo->data3 = helperAddr;
      recordInfo->data4 = (uintptr_t) firstInstruction;
      TR::SymbolReference *classSymRef = node->getChild(opCode == TR::New ? 0 : 1)->getSymbolReference();
      TR_ExternalRelocationTargetKind reloKind = opCode == TR::New ? TR_VerifyClassObjectForAlloc : TR_VerifyRefArrayForAlloc;

      TR::Relocation *r = new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(firstInstruction, (uint8_t *) classSymRef, (uint8_t *) recordInfo, reloKind, cg);
      cg->addAOTRelocation(r, __FILE__, __LINE__, node);
      }

   return objectReg;
   }

TR::Register *J9::Power::TreeEvaluator::VMnewEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   int32_t allocateSize, objectSize, dataBegin, idx;
   TR::ILOpCodes opCode;
   TR_OpaqueClassBlock *clazz;
   TR::Register *classReg, *resReg, *zeroReg = NULL;
   TR::RealRegister::RegNum resultRealReg = TR::RealRegister::gr8;
   TR::Register *condReg, *callResult, *enumReg, *dataSizeReg;
   TR::Register *tmp3Reg = NULL, *tmp4Reg, *tmp5Reg, *tmp6Reg, *tmp7Reg = NULL;
   TR::Register *packedRegAddr, *packedRegOffs, *packedRegLength;
   TR::LabelSymbol *callLabel, *callReturnLabel, *doneLabel;
   TR::RegisterDependencyConditions *conditions;
   TR::Instruction *iCursor = NULL;
   bool doInline = true, isArray = false, doublewordAlign = false;
   bool isVariableLen;
   bool insertType = false;
   static bool TR_noThreadLocalHeap = feGetEnv("TR_noThreadLocalHeap") ? 1 : 0;
   TR::Compilation * comp = cg->comp();
   bool generateArraylets = comp->generateArraylets();

   if (comp->getOption(TR_DisableTarokInlineArrayletAllocation) && (node->getOpCodeValue() == TR::anewarray || node->getOpCodeValue() == TR::newarray))
      doInline = false;

   bool usingTLH = (TR_noThreadLocalHeap == NULL);
   bool needZeroInit = (!usingTLH || !fej9->tlhHasBeenCleared()) || comp->getOptions()->realTimeGC();
   bool isDualTLH = cg->isDualTLH();

   int32_t elementSize = 0;

   opCode = node->getOpCodeValue();
   objectSize = comp->canAllocateInline(node, clazz);

   bool isArrayAlloc = (opCode == TR::newarray || opCode == TR::anewarray);
   bool isConstantLenArrayAlloc = isArrayAlloc && node->getFirstChild()->getOpCode().isLoadConst();
   bool isConstantZeroLenArrayAlloc = isConstantLenArrayAlloc && (node->getFirstChild()->getInt() == 0);
   bool canOutlineNew = false;

   if (isConstantZeroLenArrayAlloc)
      canOutlineNew = false;

   if (canOutlineNew && comp->isOptServer())
      {
      if (TR::Compiler->target.cpu.id() < TR_PPCp8)
         {
         if (!comp->getOption(TR_DisableOutlinedNew))
            {
            return outlinedHelperNewEvaluator(node, cg);
            }
         }
      else
         {
         if (comp->getOption(TR_EnableOutlinedNew))
            {
            return outlinedHelperNewEvaluator(node, cg);
            }
         }
      }

   TR_ASSERT(objectSize <= 0 || (objectSize & (TR::Compiler->om.sizeofReferenceAddress() - 1)) == 0, "object size causes an alignment problem");
   if (objectSize < 0 || (objectSize == 0 && !usingTLH))
      doInline = false;
   isVariableLen = (objectSize == 0);

   allocateSize = objectSize;

   if (opCode == TR::New)
      {
      if (comp->getOptions()->realTimeGC())
         {
         if (objectSize > fej9->getMaxObjectSizeForSizeClass())
            {
            doInline = false;
            }
         }
      }

   if (!isVariableLen)
      {
      allocateSize = (allocateSize + fej9->getObjectAlignmentInBytes() - 1) & (-fej9->getObjectAlignmentInBytes());
      }

   static int count = 0;
   doInline = doInline && performTransformation(comp, "O^O <%3d> Inlining Allocation of %s [0x%p].\n", count++, node->getOpCode().getName(), node);

   if (doInline)
      {
      generateLabelInstruction(cg, TR::InstOpCode::label, node, generateLabelSymbol(cg));

      callLabel = generateLabelSymbol(cg);
      callReturnLabel = generateLabelSymbol(cg);
      doneLabel = generateLabelSymbol(cg);
      conditions = createConditionsAndPopulateVSXDeps(cg, 14);

      TR::Node *firstChild = node->getFirstChild();
      TR::Node *secondChild = NULL;

      if (opCode == TR::New)
         {
         // classReg is passed to the VM helper on the slow path and subsequently clobbered; copy it for later nodes if necessary
         classReg = cg->gprClobberEvaluate(firstChild);
         dataBegin = sizeof(J9Object);
         }
      else
         {
         TR_ASSERT(opCode == TR::newarray || opCode == TR::anewarray, "Bad opCode for VMnewEvaluator");
         isArray = true;
         if (generateArraylets || TR::Compiler->om.useHybridArraylets())
            {
            if (node->getOpCodeValue() == TR::newarray)
               elementSize = TR::Compiler->om.getSizeOfArrayElement(node);
            else if (comp->useCompressedPointers())
               elementSize = TR::Compiler->om.sizeofReferenceField();
            else
               elementSize = TR::Compiler->om.sizeofReferenceAddress();

            if (generateArraylets)
               dataBegin = fej9->getArrayletFirstElementOffset(elementSize, comp);
            else
               dataBegin = sizeof(J9IndexableObjectContiguous);
            static const char *p = feGetEnv("TR_TarokDataBeginBreak");
            if (p)
               TR_ASSERT(false, "Hitting Arraylet Data Begin Break");
            }
         else
            {
            dataBegin = sizeof(J9IndexableObjectContiguous);
            }
         secondChild = node->getSecondChild();
         enumReg = cg->evaluate(firstChild);

         insertType = (opCode == TR::newarray && secondChild->getDataType() == TR::Int32 && secondChild->getReferenceCount() == 1 && secondChild->getOpCode().isLoadConst());

         if (insertType)
            {
            classReg = cg->allocateRegister();
            }
         else
            {
            // classReg is passed to the VM helper on the slow path and subsequently clobbered; copy it for later nodes if necessary
            classReg = cg->gprClobberEvaluate(secondChild);
            }
         }
      addDependency(conditions, classReg, TR::RealRegister::NoReg, TR_GPR, cg);
      conditions->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0();

      if (secondChild == NULL)
         enumReg = cg->allocateRegister();
      addDependency(conditions, enumReg, TR::RealRegister::NoReg, TR_GPR, cg);
      conditions->getPostConditions()->getRegisterDependency(1)->setExcludeGPR0();

      condReg = cg->allocateRegister(TR_CCR);
      addDependency(conditions, condReg, TR::RealRegister::cr0, TR_CCR, cg);
      tmp6Reg = cg->allocateRegister();
      addDependency(conditions, tmp6Reg, TR::RealRegister::NoReg, TR_GPR, cg);
      tmp4Reg = cg->allocateRegister();
      addDependency(conditions, tmp4Reg, TR::RealRegister::gr11, TR_GPR, cg); // used by TR::PPCAllocPrefetchSnippet
      tmp5Reg = cg->allocateRegister();
      addDependency(conditions, tmp5Reg, TR::RealRegister::NoReg, TR_GPR, cg);
      conditions->getPostConditions()->getRegisterDependency(5)->setExcludeGPR0();
      resReg = cg->allocateRegister();
      addDependency(conditions, resReg, resultRealReg, TR_GPR, cg); // used by TR::PPCAllocPrefetchSnippet
      conditions->getPostConditions()->getRegisterDependency(6)->setExcludeGPR0();
      dataSizeReg = cg->allocateRegister();
      addDependency(conditions, dataSizeReg, TR::RealRegister::NoReg, TR_GPR, cg);
      conditions->getPostConditions()->getRegisterDependency(7)->setExcludeGPR0();
      tmp7Reg = cg->allocateRegister();
      addDependency(conditions, tmp7Reg, TR::RealRegister::NoReg, TR_GPR, cg);
      conditions->getPostConditions()->getRegisterDependency(8)->setExcludeGPR0();
      tmp3Reg = cg->allocateRegister();
      addDependency(conditions, tmp3Reg, TR::RealRegister::gr10, TR_GPR, cg); // used by TR::PPCAllocPrefetchSnippet

      TR::Instruction *firstInstruction = cg->getAppendInstruction();

      if (!isDualTLH && needZeroInit)
         {
         zeroReg = cg->allocateRegister();
         addDependency(conditions, zeroReg, TR::RealRegister::NoReg, TR_GPR, cg);
         }

      if (isVariableLen)
         allocateSize += dataBegin;

      //Here we set up backout paths if we overflow nonZeroTLH in genHeapAlloc.
      //If we overflow the nonZeroTLH, set the destination to the right VM runtime helper (eg jitNewObjectNoZeroInit, etc...)
      //The zeroed-TLH versions have their correct destinations already setup in TR_ByteCodeIlGenerator::genNew, TR_ByteCodeIlGenerator::genNewArray, TR_ByteCodeIlGenerator::genANewArray
      //TR::PPCHeapAllocSnippet retrieves this destination via node->getSymbolReference() below after genHeapAlloc.
      if (isDualTLH && node->canSkipZeroInitialization())
         {
         if (node->getOpCodeValue() == TR::New)
            node->setSymbolReference(comp->getSymRefTab()->findOrCreateNewObjectNoZeroInitSymbolRef(comp->getMethodSymbol()));
         else if (node->getOpCodeValue() == TR::newarray)
            node->setSymbolReference(comp->getSymRefTab()->findOrCreateNewArrayNoZeroInitSymbolRef(comp->getMethodSymbol()));
         else if (node->getOpCodeValue() == TR::anewarray)
            node->setSymbolReference(comp->getSymRefTab()->findOrCreateANewArrayNoZeroInitSymbolRef(comp->getMethodSymbol()));
         }

      // On return, zeroReg is set to 0 if needZeroInit is true, and
      // dataSizeReg is set to the size of data area if isVariableLen
      // is true, and either elementSize != 1 or needZeroInit is true
      // (and we only ever use dataSizeReg below in those cases).
      genHeapAlloc(node, iCursor, clazz, isVariableLen, enumReg, classReg, resReg, zeroReg, condReg, dataSizeReg, tmp5Reg, tmp4Reg, tmp3Reg, tmp6Reg, tmp7Reg, callLabel, doneLabel, allocateSize, elementSize,
            usingTLH, needZeroInit, conditions, cg);

      // Call out to VM runtime helper if needed.
      TR::Register *objReg = cg->allocateCollectedReferenceRegister();
      TR_PPCOutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_PPCOutOfLineCodeSection(node, TR::acall, objReg, callLabel, callReturnLabel, cg);
      cg->getPPCOutOfLineCodeSectionList().push_front(outlinedHelperCall);

      if (isArray)
         {
         // Align the array if necessary.
         if (doublewordAlign && !comp->getOptions()->realTimeGC())
            genAlignArray(node, iCursor, isVariableLen, resReg, objectSize, dataBegin, dataSizeReg, condReg, tmp5Reg, tmp4Reg, cg);
         if (cg->comp()->compileRelocatableCode() && opCode == TR::anewarray)
            genInitArrayHeader(node, iCursor, isVariableLen, clazz, classReg, resReg, zeroReg, condReg, enumReg, dataSizeReg, tmp5Reg, tmp4Reg, conditions, needZeroInit, cg);
         else
            genInitArrayHeader(node, iCursor, isVariableLen, clazz, NULL, resReg, zeroReg, condReg, enumReg, dataSizeReg,
                  tmp5Reg, tmp4Reg, conditions, needZeroInit, cg);

         if (generateArraylets)
            {
            //write arraylet pointer to object header
            static const char *p = feGetEnv("TR_TarokPreWritePointerBreak");
            if (p)
               generateInstruction(cg, TR::InstOpCode::bad, node);

            iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, tmp4Reg, resReg, dataBegin, iCursor);
            if (TR::Compiler->vm.heapBaseAddress() != 0)
               iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, tmp4Reg, tmp4Reg, -TR::Compiler->vm.heapBaseAddress(), iCursor);
            if (TR::Compiler->om.compressedReferenceShiftOffset() > 0)
               iCursor = generateShiftRightLogicalImmediateLong(cg, node, tmp4Reg, tmp4Reg, TR::Compiler->om.compressedReferenceShiftOffset(), iCursor);
            iCursor = generateMemSrc1Instruction(cg, (TR::Compiler->target.is64Bit() && !comp->useCompressedPointers()) ? TR::InstOpCode::std : TR::InstOpCode::stw, node,
                  new (cg->trHeapMemory()) TR::MemoryReference(resReg, fej9->getFirstArrayletPointerOffset(comp), comp->useCompressedPointers() ? 4 : TR::Compiler->om.sizeofReferenceAddress(),
                        cg), tmp4Reg, iCursor);
            static const char *p1 = feGetEnv("TR_TarokPostWritePointerBreak");
            if (p1)
               generateInstruction(cg, TR::InstOpCode::bad, node);
            }
         }
      else
         {
         genInitObjectHeader(node, iCursor, clazz, classReg, resReg, zeroReg, condReg, tmp5Reg, tmp4Reg, packedRegOffs, conditions, needZeroInit, cg);
         }

      //Do not need zero init if using dualTLH now.
      if (!isDualTLH && needZeroInit && (!isConstantZeroLenArrayAlloc))
         {
         // Perform initialization if it is needed:
         //   1) Initialize certain array elements individually. This depends on the optimizer
         //      providing a "short" list of individual indices;
         //   2) Initialize the whole array:
         //      a) If the object size is constant, we can choose strategy depending on the
         //         size of the array. Using straight line of code, or unrolled loop;
         //      b) For variable length of array, do a counted loop;

         TR_ExtraInfoForNew *initInfo = node->getSymbolReference()->getExtraInfo();

         static bool disableFastArrayZeroInit = (feGetEnv("TR_DisableFastArrayZeroInit") != NULL);

         if (!node->canSkipZeroInitialization() && (initInfo == NULL || initInfo->numZeroInitSlots > 0))
            {
            if (!isVariableLen)
               {
               if (initInfo != NULL && initInfo->zeroInitSlots != NULL && initInfo->numZeroInitSlots <= 9 && objectSize <= UPPER_IMMED)
                  {
                  TR_BitVectorIterator bvi(*initInfo->zeroInitSlots);

                  if (TR::Compiler->target.is64Bit())
                     {
                     int32_t lastNotInit = -1;
                     int32_t currElem = -1;

                     // Try to group set of words into smallest groupings using double-word stores (where possible)
                     while (bvi.hasMoreElements())
                        {
                        currElem = bvi.getNextElement();

                        if (lastNotInit == -1)
                           {
                           // currently looking at only one slot (either just starting, or just emitted double-word store in last iter)
                           lastNotInit = currElem;
                           }
                        else if (currElem == lastNotInit + 1)
                           {
                           // consecutive slots, can use std
                           iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, new (cg->trHeapMemory()) TR::MemoryReference(resReg, lastNotInit * 4 + dataBegin, 8, cg),
                                 zeroReg, iCursor);

                           lastNotInit = -1;
                           }
                        else
                           {
                           // Two non-consecutive slots, use stw for earlier slot and keep looking for slot
                           // consecutive with the second slot currently held
                           iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(resReg, lastNotInit * 4 + dataBegin, 4, cg),
                                 zeroReg, iCursor);

                           lastNotInit = currElem;
                           }
                        }

                     if (lastNotInit != -1)
                        {
                        // Last slot is not consecutive with other slots, hasn't been initialized yet
                        iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(resReg, lastNotInit * 4 + dataBegin, 4, cg),
                              zeroReg, iCursor);
                        }
                     }
                  else
                     {
                     while (bvi.hasMoreElements())
                        {
                        iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,
                              new (cg->trHeapMemory()) TR::MemoryReference(resReg, bvi.getNextElement() * 4 + dataBegin, 4, cg), zeroReg, iCursor);
                        }
                     }
                  }
               else if (objectSize <= (TR::Compiler->om.sizeofReferenceAddress() * 12))
                  {
                  for (idx = dataBegin; idx < (objectSize - TR::Compiler->om.sizeofReferenceAddress()); idx += TR::Compiler->om.sizeofReferenceAddress())
                     {
                     iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, new (cg->trHeapMemory()) TR::MemoryReference(resReg, idx, TR::Compiler->om.sizeofReferenceAddress(), cg), zeroReg,
                           iCursor);
                     }
                  if (idx + 4 == objectSize)
                     iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(resReg, idx, 4, cg), zeroReg, iCursor);
                  else if (idx + 8 == objectSize)
                     iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, new (cg->trHeapMemory()) TR::MemoryReference(resReg, idx, 8, cg), zeroReg, iCursor);
                  }
               else
                  {
                  static bool disableUnrollNewInitLoop1 = (feGetEnv("TR_DisableUnrollNewInitLoop1") != NULL);
                  if (!disableUnrollNewInitLoop1)
                     {
                     int32_t unrollFactor = 8;
                     int32_t width = TR::Compiler->target.is64Bit() ? 8 : 4;
                     int32_t loopCount = (objectSize - dataBegin) / (unrollFactor * width);
                     int32_t res1 = (objectSize - dataBegin) % (unrollFactor * width);
                     int32_t residueCount = res1 / width;
                     int32_t res2 = res1 % width;
                     TR::LabelSymbol *loopStart = generateLabelSymbol(cg);

                     iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, tmp4Reg, resReg, dataBegin, iCursor);
                     if (loopCount > 0)
                        {
                        if (loopCount > 1)
                           {
                           iCursor = loadConstant(cg, node, loopCount, tmp5Reg, iCursor);
                           iCursor = generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, tmp5Reg, 0, iCursor);
                           iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, loopStart, iCursor);
                           }

                        for (int32_t i = 0; i < unrollFactor; i++)
                           iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, new (cg->trHeapMemory()) TR::MemoryReference(tmp4Reg, i * width, width, cg), zeroReg, iCursor);

                        iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, tmp4Reg, tmp4Reg, (unrollFactor * width), iCursor);
                        if (loopCount > 1)
                           iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bdnz, node, loopStart, condReg, iCursor);
                        }

                     for (int32_t i = 0; i < residueCount; i++)
                        iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, new (cg->trHeapMemory()) TR::MemoryReference(tmp4Reg, i * width, width, cg), zeroReg, iCursor);

                     if (res2 && residueCount != 0)
                        iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, tmp4Reg, tmp4Reg, residueCount * width, iCursor);

                     if (TR::Compiler->target.is64Bit() && res2 >= 4) // Should only be 0 or 4 here
                        iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(tmp4Reg, 0, 4, cg), zeroReg, iCursor);
                     }
                  else
                     {
                     int32_t loopCnt = (objectSize - dataBegin) >> 4;
                     int32_t residue = ((objectSize - dataBegin) >> 2) & 0x03;
                     TR::LabelSymbol *initLoop = generateLabelSymbol(cg);

                     iCursor = loadConstant(cg, node, loopCnt, tmp5Reg, iCursor);
                     iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, tmp4Reg, resReg, dataBegin, iCursor);
                     iCursor = generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, tmp5Reg, 0, iCursor);
                     iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, initLoop, iCursor);
                     iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, new (cg->trHeapMemory()) TR::MemoryReference(tmp4Reg, 0, TR::Compiler->om.sizeofReferenceAddress(), cg), zeroReg,
                           iCursor);
                     iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node,
                           new (cg->trHeapMemory()) TR::MemoryReference(tmp4Reg, TR::Compiler->om.sizeofReferenceAddress(), TR::Compiler->om.sizeofReferenceAddress(), cg), zeroReg, iCursor);
                     if (TR::Compiler->target.is32Bit())
                        {
                        iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(tmp4Reg, 8, 4, cg), zeroReg, iCursor);
                        iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(tmp4Reg, 12, 4, cg), zeroReg, iCursor);
                        }
                     iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, tmp4Reg, tmp4Reg, 16, iCursor);
                     iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bdnz, node, initLoop, condReg, iCursor);

                     idx = 0;
                     if (residue & 0x2)
                        {
                        iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, new (cg->trHeapMemory()) TR::MemoryReference(tmp4Reg, 0, TR::Compiler->om.sizeofReferenceAddress(), cg), zeroReg,
                              iCursor);

                        if (TR::Compiler->target.is32Bit())
                           iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(tmp4Reg, 4, 4, cg), zeroReg, iCursor);

                        idx = 8;
                        }
                     if (residue & 0x1)
                        {
                        iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(tmp4Reg, idx, 4, cg), zeroReg, iCursor);
                        }
                     }
                  }
               }
            else // Init variable length array
               {
               static bool disableUnrollNewInitLoop2 = (feGetEnv("TR_DisableUnrollNewInitLoop2") != NULL);
               static bool disableNewZeroInit = (feGetEnv("TR_DisableNewZeroInit") != NULL);
               if (!disableUnrollNewInitLoop2)
                  {
                  int32_t unrollFactor = 8, i = 1;
                  int32_t width = TR::Compiler->target.is64Bit() ? 8 : 4;
                  TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);
                  TR::LabelSymbol *resLabel = generateLabelSymbol(cg);
                  TR::LabelSymbol *left4Label = generateLabelSymbol(cg);
                  TR::LabelSymbol *resLoopLabel = NULL;

                  if (!disableNewZeroInit)
                     resLoopLabel = generateLabelSymbol(cg);

                  iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, tmp5Reg, resReg, (dataBegin - width), iCursor);
                  iCursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, tmp4Reg, tmp5Reg, dataSizeReg, iCursor);
                  iCursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, tmp4Reg, tmp5Reg, iCursor);
                  iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, condReg, iCursor);

                  iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, tmp6Reg, tmp4Reg, (int32_t)(-unrollFactor * width), iCursor);

                  // Switch order of compare to avoid FXU reject in P6 for later store instruction
                  iCursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, tmp6Reg, tmp5Reg, iCursor);
                  iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, resLabel, condReg, iCursor);

                  iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel, iCursor);
                  for (; i <= unrollFactor; i++)
                     iCursor = generateMemSrc1Instruction(cg, (width == 8) ? TR::InstOpCode::std : TR::InstOpCode::stw, node,
                           new (cg->trHeapMemory()) TR::MemoryReference(tmp5Reg, i * width, width, cg), zeroReg, iCursor);
                  iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, tmp5Reg, tmp5Reg, (int32_t)(unrollFactor * width), iCursor);

                  iCursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, tmp6Reg, tmp5Reg, iCursor);
                  iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, node, loopLabel, condReg, iCursor);

                  iCursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, tmp4Reg, tmp5Reg, iCursor);
                  iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, condReg, iCursor);

                  iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, resLabel, iCursor);

                  if (!disableNewZeroInit)
                     {
                     // End pointer-4
                     iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, tmp6Reg, tmp4Reg, -4, iCursor);

                     // Residue loop start
                     iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, resLoopLabel, iCursor);

                     // If current pointer == end pointer - 4, there is 4 bytes left to write
                     // If current pointer > end pointer - 4, data is 8-byte divisible and we are done
                     iCursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, tmp6Reg, tmp5Reg, iCursor);
                     iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, left4Label, condReg, iCursor);
                     iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, doneLabel, condReg, iCursor);

                     // Do 8-byte zero init and restart loop
                     iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, new (cg->trHeapMemory()) TR::MemoryReference(tmp5Reg, width, width, cg), zeroReg, iCursor);

                     if (TR::Compiler->target.is32Bit())
                        {
                        iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(tmp5Reg, 8, 4, cg), zeroReg, iCursor);
                        }

                     iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, tmp5Reg, tmp5Reg, 8, iCursor);
                     iCursor = generateLabelInstruction(cg, TR::InstOpCode::b, node, resLoopLabel);

                     // Finish last 4 bytes if necessary
                     iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, left4Label, iCursor);
                     iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(tmp5Reg, width, 4, cg), zeroReg, iCursor);
                     }
                  else
                     {
                     /* Need this in 64-bit as well */
                     iCursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, tmp6Reg, tmp5Reg, tmp4Reg, iCursor);
                     iCursor = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, condReg, tmp6Reg, 4, iCursor);
                     iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, left4Label, condReg, iCursor);

                     iCursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, new (cg->trHeapMemory()) TR::MemoryReference(tmp5Reg, 8, width, cg), zeroReg, iCursor);

                     if (TR::Compiler->target.is32Bit())
                        {
                        iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(tmp5Reg, 12, 4, cg), zeroReg, iCursor);
                        }

                     iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, tmp5Reg, tmp5Reg, 8, iCursor);
                     iCursor = generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, tmp4Reg, tmp5Reg, iCursor);
                     iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, condReg, iCursor);
                     iCursor = generateLabelInstruction(cg, TR::InstOpCode::b, node, resLabel);

                     /* Need this in 64-bit as well */
                     iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, left4Label, iCursor);
                     iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(tmp5Reg, 8, 4, cg), zeroReg, iCursor);
                     }
                  }
               else
                  {
                  // Test for zero length array: the following two instructions will be conbined
                  // to the record form by later pass.
                  iCursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp5Reg, dataSizeReg, 30, 0x3FFFFFFF, iCursor);
                  iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, tmp5Reg, 0, iCursor);
                  iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, condReg, iCursor);
                  iCursor = generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, tmp5Reg, 0, iCursor);
                  iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, tmp4Reg, resReg, dataBegin - 4, iCursor);

                  TR::LabelSymbol *initLoop = generateLabelSymbol(cg);
                  iCursor = generateLabelInstruction(cg, TR::InstOpCode::label, node, initLoop, iCursor);
                  iCursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(tmp4Reg, 4, 4, cg), zeroReg, iCursor);
                  iCursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, tmp4Reg, tmp4Reg, 4, iCursor);
                  iCursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::bdnz, node, initLoop, condReg, iCursor);
                  }
               }
            }
         }

      if (cg->comp()->compileRelocatableCode() && (opCode == TR::New || opCode == TR::anewarray))
         {
         firstInstruction = firstInstruction->getNext();
         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation *) comp->trMemory()->allocateMemory(sizeof(TR_RelocationRecordInformation), heapAlloc);
         recordInfo->data1 = allocateSize;
         recordInfo->data2 = node->getInlinedSiteIndex();
         recordInfo->data3 = (uintptr_t) callLabel;
         recordInfo->data4 = (uintptr_t) firstInstruction;
         TR::SymbolReference * classSymRef;
         TR_ExternalRelocationTargetKind reloKind;

         if (opCode == TR::New)
            {
            classSymRef = node->getFirstChild()->getSymbolReference();
            reloKind = TR_VerifyClassObjectForAlloc;
            }
         else
            {
            classSymRef = node->getSecondChild()->getSymbolReference();
            reloKind = TR_VerifyRefArrayForAlloc;
            }

         cg->addAOTRelocation(new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(firstInstruction, (uint8_t *) classSymRef, (uint8_t *) recordInfo, reloKind, cg),
                  __FILE__, __LINE__, node);
         }

      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

      // At this point the object is initialized and we can move it to a collected register.
      // The out of line path will do the same. Since the live ranges of resReg and objReg
      // do not overlap we want them to occupy the same real register so that this instruction
      // becomes a nop and can be optimized away.
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, objReg, resReg);
      conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
      addDependency(conditions, objReg, resultRealReg, TR_GPR, cg);

      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, callReturnLabel, conditions);

      cg->stopUsingRegister(condReg);
      cg->stopUsingRegister(tmp3Reg);
      cg->stopUsingRegister(tmp4Reg);
      cg->stopUsingRegister(tmp5Reg);
      cg->stopUsingRegister(tmp6Reg);
      cg->stopUsingRegister(tmp7Reg);
      cg->stopUsingRegister(resReg);
      cg->stopUsingRegister(dataSizeReg);
      if (!isDualTLH && needZeroInit)
         cg->stopUsingRegister(zeroReg);
      cg->decReferenceCount(firstChild);
      if (opCode == TR::New)
         {
         if (classReg != firstChild->getRegister())
            cg->stopUsingRegister(classReg);
         cg->stopUsingRegister(enumReg);
         }
      else
         {
         cg->decReferenceCount(secondChild);
         if (classReg != secondChild->getRegister())
            cg->stopUsingRegister(classReg);
         }

      node->setRegister(objReg);
      return objReg;
      }
   else
      {
      TR::Node::recreate(node, TR::acall);
      callResult = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return (callResult);
      }
   }

#define LOCK_INC_DEC_VALUE                                OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT
#define LOCK_THREAD_PTR_MASK                              (~OBJECT_HEADER_LOCK_BITS_MASK)
#define LOCK_THREAD_PTR_AND_UPPER_COUNT_BIT_MASK          (LOCK_THREAD_PTR_MASK | OBJECT_HEADER_LOCK_LAST_RECURSION_BIT)
#define LOCK_FIRST_RECURSION_BIT_NUMBER	                  leadingZeroes(OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT)
#define LOCK_LAST_RECURSION_BIT_NUMBER	                  leadingZeroes(OBJECT_HEADER_LOCK_LAST_RECURSION_BIT)
#define LOCK_OWNING_NON_INFLATED_COMPLEMENT               (OBJECT_HEADER_LOCK_BITS_MASK & ~OBJECT_HEADER_LOCK_INFLATED)
#define LOCK_RESERVATION_BIT                              OBJECT_HEADER_LOCK_RESERVED
#define LOCK_RES_PRIMITIVE_ENTER_MASK                     (OBJECT_HEADER_LOCK_RECURSION_MASK | OBJECT_HEADER_LOCK_FLC)
#define LOCK_RES_PRIMITIVE_EXIT_MASK                      (OBJECT_HEADER_LOCK_BITS_MASK & ~OBJECT_HEADER_LOCK_RECURSION_MASK)
#define LOCK_RES_NON_PRIMITIVE_ENTER_MASK                 (OBJECT_HEADER_LOCK_RECURSION_MASK & ~OBJECT_HEADER_LOCK_LAST_RECURSION_BIT)
#define LOCK_RES_NON_PRIMITIVE_EXIT_MASK                  (OBJECT_HEADER_LOCK_RECURSION_MASK & ~OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT)
#define LOCK_RES_OWNING_COMPLEMENT                        (OBJECT_HEADER_LOCK_RECURSION_MASK | OBJECT_HEADER_LOCK_FLC)
#define LOCK_RES_PRESERVE_ENTER_COMPLEMENT                (OBJECT_HEADER_LOCK_RECURSION_MASK & ~OBJECT_HEADER_LOCK_LAST_RECURSION_BIT)
#define LOCK_RES_CONTENDED_VALUE                          (OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT|OBJECT_HEADER_LOCK_RESERVED|OBJECT_HEADER_LOCK_FLC)

// Special case evaluator for simple read monitors.
static bool simpleReadMonitor(TR::Node *node, TR::CodeGenerator *cg, TR::Node *objNode, TR::Register *objReg, TR::Register *objectClassReg, TR::Register *condReg,
      TR::Register *lookupOffsetReg)
   {
#if defined(J9VM_JIT_NEW_DUAL_HELPERS)
   // This needs to be updated to work with dual-mode helpers by not using snippets
   return false;
#endif
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   static char *disableSRM = feGetEnv("TR_disableSimpleReadMonitors");
   static char *nolwsync = feGetEnv("TR_noSRMlwsync");

   if (disableSRM)
      return false;

   // look for the special case of a read monitor sequence that protects
   // a single fixed-point load, ie:
   //   monenter (object)
   //   a simple form of iaload or iiload
   //   monexit (object)

   // note: before we make the following checks it is important that the
   // object on the monenter has been evaluated (currently done before the
   // call to this routine, and the result passed in as objReg)

   if (!node->isReadMonitor())
      return false;

   TR::TreeTop *nextTreeTop = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();
   if (!nextTreeTop)
      return false;

   TR::TreeTop *liveMonitorExitStore = NULL;
   TR::TreeTop *liveMonitorEnterStore = NULL;

   if (nextTreeTop->getNode()->getOpCode().isStore() && nextTreeTop->getNode()->getSymbol()->holdsMonitoredObject())
      {
      liveMonitorEnterStore = nextTreeTop;
      nextTreeTop = nextTreeTop->getNextTreeTop();
      }

   if (nextTreeTop->getNode()->getOpCode().getOpCodeValue() == TR::monexitfence)
      {
      liveMonitorExitStore = nextTreeTop;
      nextTreeTop = nextTreeTop->getNextTreeTop();
      }

   TR::TreeTop *secondNextTreeTop = nextTreeTop->getNextTreeTop();
   if (!secondNextTreeTop)
      return false;

   if (secondNextTreeTop->getNode()->getOpCode().getOpCodeValue() == TR::monexitfence)
      {
      liveMonitorExitStore = secondNextTreeTop;
      secondNextTreeTop = secondNextTreeTop->getNextTreeTop();
      }

   // check that the second node after the monent is the matching monexit
   TR::Node *secondNextTopNode = secondNextTreeTop->getNode();
   if (!secondNextTopNode)
      return false;
   if (secondNextTopNode->getOpCodeValue() == TR::treetop)
      secondNextTopNode = secondNextTopNode->getFirstChild();
   if (secondNextTopNode->getOpCodeValue() != TR::monexit || secondNextTopNode->getFirstChild() != objNode)
      return false;

   // check that the first node after the monent is a simple load
   TR::Node *nextTopNode = nextTreeTop->getNode();
   if (!nextTopNode)
      return false;
   if (nextTopNode->getOpCodeValue() == TR::treetop || nextTopNode->getOpCodeValue() == TR::iRegStore)
      nextTopNode = nextTopNode->getFirstChild();
   if (!nextTopNode->getOpCode().isMemoryReference())
      return false;
   if (nextTopNode->getSymbolReference()->isUnresolved())
      return false;
   if (nextTopNode->getSymbolReference()->getSymbol()->isSyncVolatile() && TR::Compiler->target.isSMP())
      return false;
   // possible TODO: expand the set of load types we can handle
   if (nextTopNode->getOpCodeValue() != TR::aloadi && nextTopNode->getOpCodeValue() != TR::iloadi)
      {
      // printf("Rejecting read-only monitor on load type in %s\n", comp->getCurrentMethod()->signature());
      return false;
      }
   // possible TODO: expand the complexity of loads we can handle
   // iaload and iiload are indirect and have a child
   // if we don't need to evaluate that child then the iaload or iiload
   // consists of a single hardware instruction and satisfies our current
   // constraint of simple
   if (!nextTopNode->getFirstChild()->getRegister())
      {
      // printf("Rejecting read-only monitor on complex load in %s\n", cg->comp()->getCurrentMethod()->signature());
      return false;
      }

   // end of checks, we can handle this case
   // printf("Success on read-only monitor optimization in %s\n", cg->comp()->getCurrentMethod()->signature());

   // RAS: dump out the load and monexit trees, since we are effectively
   // evaluating them here
   if (comp->getOption(TR_TraceCG) || debug("traceGRA"))
      {
      trfprintf(comp->getOutFile(), "\n");
      comp->getDebug()->dumpSingleTreeWithInstrs(nextTreeTop, NULL, true, false, true, comp->getOption(TR_TraceRegisterPressureDetails));
      trfprintf(comp->getOutFile(), "\n");
      comp->getDebug()->dumpSingleTreeWithInstrs(secondNextTreeTop, NULL, true, false, true, comp->getOption(TR_TraceRegisterPressureDetails));
      trfflush(comp->getOutFile());
      }

   if (liveMonitorEnterStore)
      {
      TR::TreeTop *oldCur = cg->getCurrentEvaluationTreeTop();
      cg->setCurrentEvaluationTreeTop(liveMonitorEnterStore);
      cg->evaluate(liveMonitorEnterStore->getNode());
      cg->setCurrentEvaluationTreeTop(oldCur);
      }
   // check the TR::monexit to see if an lwsync is required
   // or whether to suppress the lwsync for performance analysis purposes
   bool needlwsync = TR::Compiler->target.isSMP() && !secondNextTopNode->canSkipSync() && nolwsync == NULL;

   // generate code for the combined monenter, load and monexit:
   //    <monitor object evaluation code>
   //    <TR_MemoryReference evaluation code for load>
   // (see design 908 for more information on the code sequences used)
   // if running on a POWER4/5 and the load is from the monitor object:
   //    (altsequence)
   //   if an lwsync is needed:
   //          li     offsetReg, offset_of_monitor_word
   //       loopLabel:
   //          lwarx  monitorReg, [objReg, offsetReg]
   //          cmpli  cndReg, monitorReg, 0
   //          bne    cndReg, recurCheckLabel
   //          stwcx  [objReg, offsetReg], metaReg
   //          bne-   loopLabel
   //          or     loadBaseReg,loadBaseReg,monitorReg
   //          <load>
   //          lwsync
   //          stw    offsetReg(objReg), monitorReg
   //       restartLabel:
   //   else (no lwsync):
   //          li     offsetReg, offset_of_monitor_word
   //       loopLabel:
   //          lwarx  monitorReg, [objReg, offsetReg]
   //          cmpli  cndReg, monitorReg, 0
   //          bne    cndReg, recurCheckLabel
   //          stwcx  [objReg, offsetReg], metaReg
   //          bne-   loopLabel
   //          or     loadBaseReg,loadBaseReg,monitorReg
   //          <load>
   //          xor    monitorReg,loadResultReg,loadResultReg
   //          stw    offsetReg(objReg), monitorReg
   //       restartLabel:
   // else:
   //   if an lwsync is needed:
   //          li     offsetReg, offset_of_monitor_word
   //       loopLabel:
   //          lwarx  monitorReg, [objReg, offsetReg]
   //          cmpli  cndReg, monitorReg, 0
   //          bne    cndReg, recurCheckLabel
   //          or     loadBaseReg,loadBaseReg,monitorReg
   //          <load>
   //          lwsync
   //          stwcx  [objReg, offsetReg], monitorReg
   //          bne-   loopLabel
   //       restartLabel:
   //   else (no lwsync):
   //          li     offsetReg, offset_of_monitor_word
   //       loopLabel:
   //          lwarx  monitorReg, [objReg, offsetReg]
   //          cmpli  cndReg, monitorReg, 0
   //          bne    cndReg, recurCheckLabel
   //          or     loadBaseReg,loadBaseReg,monitorReg
   //          <load>
   //          xor    monitorReg,loadResultReg,loadResultReg
   //          stwcx  [objReg, offsetReg], monitorReg
   //          bne-   loopLabel
   //       restartLabel:
   // === SNIPPET === (generated later)
   //   recurCheckLabel:
   //      rlwinm monitorReg, monitorReg, 0, THREAD_MASK_CONST
   //      cmp    cndReg, metaReg, monitorReg
   //      bne    cndReg, slowPath
   //      <load>
   //      b      restartLabel
   //   slowPath:
   //      bl     monitorEnterHelper
   //      <load>
   //      bl     monitorExitHelper
   //      b      restartLabel

   TR::RegisterDependencyConditions *conditions;
   TR::Register *metaReg, *monitorReg, *cndReg, *offsetReg;
   TR::InstOpCode::Mnemonic opCode;
   int32_t lockSize;

   int32_t numDeps = 6;
   if (objectClassReg)
      numDeps = numDeps + 2;

   if (lookupOffsetReg)
      numDeps = numDeps + 1;

   conditions = createConditionsAndPopulateVSXDeps(cg, numDeps);

   monitorReg = cg->allocateRegister();
   offsetReg = cg->allocateRegister();
   cndReg = cg->allocateRegister(TR_CCR);
   addDependency(conditions, monitorReg, TR::RealRegister::gr11, TR_GPR, cg);
   addDependency(conditions, offsetReg, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);

   // the following code is derived from the iaload and iiload evaluators
   TR::Register *loadResultReg;
   TR::MemoryReference *tempMR = NULL;
   if (nextTopNode->getOpCodeValue() == TR::aloadi)
      loadResultReg = cg->allocateCollectedReferenceRegister();
   else
      loadResultReg = cg->allocateRegister();
   if (nextTopNode->getSymbolReference()->getSymbol()->isInternalPointer())
      {
      loadResultReg->setPinningArrayPointer(nextTopNode->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
      loadResultReg->setContainsInternalPointer();
      }
   nextTopNode->setRegister(loadResultReg);
   TR::InstOpCode::Mnemonic loadOpCode;
   if (nextTopNode->getOpCodeValue() == TR::aloadi && TR::Compiler->target.is64Bit())
      {
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
      if (nextTopNode->getSymbol()->isClassObject())
         {
         tempMR = new (cg->trHeapMemory()) TR::MemoryReference(nextTopNode, 4, cg);
         loadOpCode = TR::InstOpCode::lwz;
         }
      else
#endif
         {
         tempMR = new (cg->trHeapMemory()) TR::MemoryReference(nextTopNode, 8, cg);
         loadOpCode = TR::InstOpCode::ld;
         }
      }
   else
      {
      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(nextTopNode, 4, cg);
      loadOpCode = TR::InstOpCode::lwz;
      }
   // end of code derived from the iaload and iiload evaluators

   addDependency(conditions, loadResultReg, TR::RealRegister::NoReg, TR_GPR, cg);
   if (tempMR->getBaseRegister() != objReg)
      {
      addDependency(conditions, tempMR->getBaseRegister(), TR::RealRegister::NoReg, TR_GPR, cg);
      conditions->getPreConditions()->getRegisterDependency(4)->setExcludeGPR0();
      conditions->getPostConditions()->getRegisterDependency(4)->setExcludeGPR0();
      }
   addDependency(conditions, objReg, TR::RealRegister::gr3, TR_GPR, cg);

   TR::Register *baseReg = objReg;
   if (objectClassReg)
      baseReg = objectClassReg;

   bool altsequence = (TR::Compiler->target.cpu.id() == TR_PPCgp || TR::Compiler->target.cpu.id() == TR_PPCgr) && tempMR->getBaseRegister() == objReg;

   TR::LabelSymbol *loopLabel, *restartLabel, *recurCheckLabel, *monExitCallLabel;
   loopLabel = generateLabelSymbol(cg);
   restartLabel = generateLabelSymbol(cg);
   recurCheckLabel = generateLabelSymbol(cg);
   monExitCallLabel = generateLabelSymbol(cg);

   metaReg = cg->getMethodMetaDataRegister();

   if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
      {
      opCode = TR::InstOpCode::ldarx;
      lockSize = 8;
      }
   else
      {
      opCode = TR::InstOpCode::lwarx;
      lockSize = 4;
      }

   if (objectClassReg)
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, offsetReg, 0);
   else
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, offsetReg, fej9->getByteOffsetToLockword((TR_OpaqueClassBlock *) cg->getMonClass(node)));
   generateLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel);
   generateTrg1MemInstruction(cg, opCode, node, monitorReg, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, offsetReg, lockSize, cg));
   generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, cndReg, monitorReg, 0);
   TR::Instruction *gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, recurCheckLabel, cndReg);
   gcPoint->PPCNeedsGCMap(0xFFFFFFFF);

   if (altsequence)
      {
      if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
         opCode = TR::InstOpCode::stdcx_r;
      else
         opCode = TR::InstOpCode::stwcx_r;
      generateMemSrc1Instruction(cg, opCode, node, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, offsetReg, lockSize, cg), metaReg);
      if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
         // use PPC AS branch hint
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, node, loopLabel, cndReg);
      else
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, loopLabel, cndReg);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tempMR->getBaseRegister(), tempMR->getBaseRegister(), monitorReg);
      generateTrg1MemInstruction(cg, loadOpCode, node, loadResultReg, tempMR);
      if (needlwsync)
         generateInstruction(cg, TR::InstOpCode::lwsync, node);
      else
         generateTrg1Src2Instruction(cg, TR::InstOpCode::XOR, node, monitorReg, loadResultReg, loadResultReg);
      if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
         opCode = TR::InstOpCode::std;
      else
         opCode = TR::InstOpCode::stw;
      generateMemSrc1Instruction(cg, opCode, node,
            new (cg->trHeapMemory()) TR::MemoryReference(baseReg, objectClassReg ? 0 : fej9->getByteOffsetToLockword((TR_OpaqueClassBlock *) cg->getMonClass(node)), lockSize,
                  cg), monitorReg);
      }
   else
      {
      generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tempMR->getBaseRegister(), tempMR->getBaseRegister(), monitorReg);
      generateTrg1MemInstruction(cg, loadOpCode, node, loadResultReg, tempMR);
      if (needlwsync)
         generateInstruction(cg, TR::InstOpCode::lwsync, node);
      else
         generateTrg1Src2Instruction(cg, TR::InstOpCode::XOR, node, monitorReg, loadResultReg, loadResultReg);
      if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
         opCode = TR::InstOpCode::stdcx_r;
      else
         opCode = TR::InstOpCode::stwcx_r;
      generateMemSrc1Instruction(cg, opCode, node, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, offsetReg, lockSize, cg), monitorReg);
      if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
         // use PPC AS branch hint
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, node, loopLabel, cndReg);
      else
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, loopLabel, cndReg);
      }

   if (objectClassReg)
      addDependency(conditions, objectClassReg, TR::RealRegister::NoReg, TR_GPR, cg);

   if (lookupOffsetReg)
      addDependency(conditions, lookupOffsetReg, TR::RealRegister::NoReg, TR_GPR, cg);

   if (condReg)
      addDependency(conditions, condReg, TR::RealRegister::cr1, TR_CCR, cg);

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, restartLabel, conditions);

   conditions->stopUsingDepRegs(cg, objReg, loadResultReg);
   cg->decReferenceCount(objNode);

   TR::Snippet *snippet = new (cg->trHeapMemory()) TR::PPCReadMonitorSnippet(cg, node, secondNextTopNode, recurCheckLabel, monExitCallLabel, restartLabel, loadOpCode,
         tempMR->getOffset(), objectClassReg);
   cg->addSnippet(snippet);

   // the load and monexit trees need reference count adjustments and
   // must not be evaluated
   tempMR->decNodeReferenceCounts(cg);
   cg->decReferenceCount(nextTopNode);
   if (secondNextTopNode == secondNextTreeTop->getNode())
      cg->recursivelyDecReferenceCount(secondNextTopNode->getFirstChild());
   else
      cg->recursivelyDecReferenceCount(secondNextTopNode);

   if (liveMonitorExitStore)
      {
      TR::TreeTop *prev = liveMonitorExitStore->getPrevTreeTop();
      TR::TreeTop *next = liveMonitorExitStore->getNextTreeTop();
      prev->join(next);

      next = secondNextTreeTop->getNextTreeTop();
      secondNextTreeTop->join(liveMonitorExitStore);
      liveMonitorExitStore->join(next);
      }

   cg->setCurrentEvaluationTreeTop(secondNextTreeTop);

   return true;
   }

TR::Register *J9::Power::TreeEvaluator::VMmonentEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   int32_t lwOffset = fej9->getByteOffsetToLockword((TR_OpaqueClassBlock *) cg->getMonClass(node));
   TR::Compilation *comp = cg->comp();
   TR_ASSERT(lwOffset>=LOWER_IMMED && lwOffset<=UPPER_IMMED, "Need re-work on using lwOffset.");
   if (comp->getOption(TR_OptimizeForSpace) ||
         comp->getOption(TR_MimicInterpreterFrameShape) ||
         (comp->getOption(TR_FullSpeedDebug) && node->isSyncMethodMonitor()) ||
         comp->getOption(TR_DisableInlineMonEnt))
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::call);
      TR::Register *targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }

   TR::Node *objNode = node->getFirstChild();
   TR::Register *objReg = cg->evaluate(objNode);
   bool simpleLocking = false;
   TR::LabelSymbol *callLabel = generateLabelSymbol(cg);

   TR::LabelSymbol *monitorLookupCacheLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *fallThruFromMonitorLookupCacheLabel = generateLabelSymbol(cg);

   int32_t numDeps = 9;
   TR::Register *objectClassReg = NULL;
   TR::Register *lookupOffsetReg = NULL;
   TR::Register *baseReg = objReg;
   TR::Register *condReg = NULL;
   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, startLabel, NULL);
   startLabel->setStartInternalControlFlow();

#if defined (J9VM_THR_LOCK_NURSERY)
   if (lwOffset <= 0)
      {
      objectClassReg = cg->allocateRegister();
      condReg = cg->allocateRegister(TR_CCR);

#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
      // must read only 32 bits
      generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, objectClassReg,
            new (cg->trHeapMemory()) TR::MemoryReference(objReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), 4, cg));
#else
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, objectClassReg,
            new (cg->trHeapMemory()) TR::MemoryReference(objReg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceAddress(), cg));
#endif
      TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, objectClassReg);

      int32_t offsetOfLockOffset = offsetof(J9Class, lockOffset);
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(objectClassReg, offsetOfLockOffset, TR::Compiler->om.sizeofReferenceAddress(), cg);
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, objectClassReg, tempMR);

      generateTrg1Src1ImmInstruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::cmpi8 : TR::InstOpCode::cmpi4, node, condReg, objectClassReg, 0);

      if (comp->getOption(TR_EnableMonitorCacheLookup))
         {
         lwOffset = 0;
         generateConditionalBranchInstruction(cg, TR::InstOpCode::ble, node, monitorLookupCacheLabel, condReg);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, objectClassReg, objReg, objectClassReg);
         generateLabelInstruction(cg, TR::InstOpCode::b, node, fallThruFromMonitorLookupCacheLabel);

         generateLabelInstruction(cg, TR::InstOpCode::label, node, monitorLookupCacheLabel);

         lookupOffsetReg = cg->allocateRegister();
         numDeps++;

         int32_t offsetOfMonitorLookupCache = offsetof(J9VMThread, objectMonitorLookupCache);

         generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, lookupOffsetReg, objReg);

         int32_t t = trailingZeroes(fej9->getObjectAlignmentInBytes());

         if (TR::Compiler->target.is64Bit())
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sradi, node, lookupOffsetReg, lookupOffsetReg, t);
         else
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi, node, lookupOffsetReg, lookupOffsetReg, t);

         J9JavaVM * jvm = fej9->getJ9JITConfig()->javaVM;
         if (TR::Compiler->target.is64Bit())
            simplifyANDRegImm(node, lookupOffsetReg, lookupOffsetReg, (int64_t) J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE - 1, cg, objNode);
         else
            simplifyANDRegImm(node, lookupOffsetReg, lookupOffsetReg, J9VMTHREAD_OBJECT_MONITOR_CACHE_SIZE - 1, cg, objNode);

         if (TR::Compiler->target.is64Bit())
            generateShiftLeftImmediateLong(cg, node, lookupOffsetReg, lookupOffsetReg, trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField()));
         else
            generateShiftLeftImmediate(cg, node, lookupOffsetReg, lookupOffsetReg, trailingZeroes((int32_t) TR::Compiler->om.sizeofReferenceField()));

         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, lookupOffsetReg, lookupOffsetReg, offsetOfMonitorLookupCache);

         generateTrg1MemInstruction(cg, (TR::Compiler->target.is64Bit() && fej9->generateCompressedLockWord()) ? TR::InstOpCode::lwz :TR::InstOpCode::Op_load, node, objectClassReg,
               new (cg->trHeapMemory()) TR::MemoryReference(cg->getMethodMetaDataRegister(), lookupOffsetReg, TR::Compiler->om.sizeofReferenceField(), cg));

         generateTrg1Src1ImmInstruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::cmpi8 : TR::InstOpCode::cmpi4, node, condReg, objectClassReg, 0);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, callLabel, condReg);

         int32_t offsetOfMonitor = offsetof(J9ObjectMonitor, monitor);
         generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, lookupOffsetReg,
               new (cg->trHeapMemory()) TR::MemoryReference(objectClassReg, offsetOfMonitor, TR::Compiler->om.sizeofReferenceAddress(), cg));

         int32_t offsetOfUserData = offsetof(J9ThreadAbstractMonitor, userData);
         generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, lookupOffsetReg,
               new (cg->trHeapMemory()) TR::MemoryReference(lookupOffsetReg, offsetOfUserData, TR::Compiler->om.sizeofReferenceAddress(), cg));

         generateTrg1Src2Instruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::cmp8 : TR::InstOpCode::cmp4, node, condReg, lookupOffsetReg, objReg);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, callLabel, condReg);

         int32_t offsetOfAlternateLockWord = offsetof(J9ObjectMonitor, alternateLockword);

         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, objectClassReg, objectClassReg, offsetOfAlternateLockWord);

         generateLabelInstruction(cg, TR::InstOpCode::label, node, fallThruFromMonitorLookupCacheLabel);
         }
      else
         {
         generateConditionalBranchInstruction(cg, TR::InstOpCode::ble, node, callLabel, condReg);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, objectClassReg, objReg, objectClassReg);
         }

      simpleLocking = true;
      lwOffset = 0;
      baseReg = objectClassReg;

      numDeps = numDeps + 2;
      }
#endif

   bool reserveLocking = false, normalLockWithReservationPreserving = false;

   if (!simpleLocking && comp->getOption(TR_ReservingLocks))
      TR::TreeEvaluator::evaluateLockForReservation(node, &reserveLocking, &normalLockWithReservationPreserving, cg);

   if (reserveLocking)
      return reservationLockEnter(node, lwOffset, objectClassReg, cg);

   if (!simpleLocking && !reserveLocking && !normalLockWithReservationPreserving && simpleReadMonitor(node, cg, objNode, objReg, objectClassReg, condReg, lookupOffsetReg))
      return NULL;

   TR::RegisterDependencyConditions *conditions;
   TR::Register *metaReg, *monitorReg, *offsetReg;
   TR::Register *readerReg, *threadReg;
   TR::InstOpCode::Mnemonic opCode;
   int32_t lockSize;

   conditions = createConditionsAndPopulateVSXDeps(cg, numDeps);

   if (condReg == NULL)
      condReg = cg->allocateRegister(TR_CCR);
   threadReg = cg->allocateRegister();
   monitorReg = cg->allocateRegister();
   offsetReg = cg->allocateRegister();
   addDependency(conditions, objReg, TR::RealRegister::NoReg, TR_GPR, cg);
   conditions->getPreConditions()->getRegisterDependency(conditions->getAddCursorForPre() - 1)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(conditions->getAddCursorForPost() - 1)->setExcludeGPR0();
   addDependency(conditions, monitorReg, TR::RealRegister::NoReg, TR_GPR, cg);
   conditions->getPreConditions()->getRegisterDependency(conditions->getAddCursorForPre() - 1)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(conditions->getAddCursorForPost() - 1)->setExcludeGPR0();
   addDependency(conditions, offsetReg, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, threadReg, TR::RealRegister::NoReg, TR_GPR, cg);
   if (objectClassReg)
      {
      addDependency(conditions, objectClassReg, TR::RealRegister::NoReg, TR_GPR, cg);
      conditions->getPreConditions()->getRegisterDependency(conditions->getAddCursorForPre() - 1)->setExcludeGPR0();
      conditions->getPostConditions()->getRegisterDependency(conditions->getAddCursorForPost() - 1)->setExcludeGPR0();
      }

   if (lookupOffsetReg)
      addDependency(conditions, lookupOffsetReg, TR::RealRegister::NoReg, TR_GPR, cg);

   addDependency(conditions, condReg, TR::RealRegister::cr0, TR_CCR, cg);

   // full codegen support for read monitors is not enabled, pending performance tuning
   if (!ppcSupportsReadMonitors || !node->isReadMonitor())
      {
      //    li     offsetReg, offset_of_monitor_word
      // loopLabel:
      //    lwarx  monitorReg, offsetReg(objReg)
      //    cmpli  condReg, monitorReg, 0
      //    bne    condReg, incLabel
      //    stwcx  offsetReg(objReg), metaReg
      //    bne    loopLabel
      //    isync
      //    b      doneLabel
      // incLabel:
      //    rlwinm/rldicr threadReg, monitorReg, 0, LOCK_THREAD_PTR_AND_UPPER_COUNT_BIT_MASK
      //    cmp    cr0, 0, metaReg, threadReg
      //    bne    callLabel
      //    addi   monitorReg, monitorReg, LOCK_INC_DEC_VALUE
      //    st     [objReg, monitor offset], monitorReg
      // doneLabel:
      // callReturnLabel:
      // === OUT OF LINE ===
      // callLabel:
      //    bl     jitMonitorEntry
      //    b      callReturnLabel

      TR::LabelSymbol *doneLabel, *incLabel, *loopLabel;
      doneLabel = generateLabelSymbol(cg);
      incLabel = generateLabelSymbol(cg);
      loopLabel = generateLabelSymbol(cg);

      if (normalLockWithReservationPreserving)
         {
         TR::Register *tempReg = cg->allocateRegister();
         addDependency(conditions, tempReg, TR::RealRegister::NoReg, TR_GPR, cg);
         }

      metaReg = cg->getMethodMetaDataRegister();

      if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
         {
         opCode = TR::InstOpCode::ldarx;
         lockSize = 8;
         }
      else
         {
         opCode = TR::InstOpCode::lwarx;
         lockSize = 4;
         }

      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, offsetReg, lwOffset);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel);
      generateTrg1MemInstruction(cg, opCode, PPCOpProp_LoadReserveExclusiveAccess, node, monitorReg,
            new (cg->trHeapMemory()) TR::MemoryReference(baseReg, offsetReg, lockSize, cg));
      generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, condReg, monitorReg, 0);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, incLabel, condReg);
      if (lockSize == 8)
         opCode = TR::InstOpCode::stdcx_r;
      else
         opCode = TR::InstOpCode::stwcx_r;
      generateMemSrc1Instruction(cg, opCode, node, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, offsetReg, lockSize, cg), metaReg);
      if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
         // use PPC AS branch hint
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, node, loopLabel, condReg);
      else
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, loopLabel, condReg);

      if (TR::Compiler->target.isSMP())
         {
         // either an isync or a sync can be used here to prevent any
         // following loads from executing out-of-order
         // on nstar and pulsar a sync is cheaper, while an isync is
         // cheaper on other processors
         if (TR::Compiler->target.cpu.id() == TR_PPCnstar || TR::Compiler->target.cpu.id() == TR_PPCpulsar)
            generateInstruction(cg, TR::InstOpCode::sync, node);
         else
            generateInstruction(cg, TR::InstOpCode::isync, node);
         }
      generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);

      generateLabelInstruction(cg, TR::InstOpCode::label, node, incLabel);

      generateTrg1Src1Imm2Instruction(cg, TR::Compiler->target.is64Bit() ? TR::InstOpCode::rldicr : TR::InstOpCode::rlwinm, node, threadReg, monitorReg, 0, LOCK_THREAD_PTR_AND_UPPER_COUNT_BIT_MASK);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::Op_cmpl, node, condReg, threadReg, metaReg);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, callLabel, condReg);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, monitorReg, monitorReg, LOCK_INC_DEC_VALUE);
      generateMemSrc1Instruction(cg, lockSize == 8 ? TR::InstOpCode::std : TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, lwOffset, lockSize, cg), monitorReg);

      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

      doneLabel->setEndInternalControlFlow();

      TR::LabelSymbol *callReturnLabel = generateLabelSymbol(cg);

      TR_PPCOutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_PPCOutOfLineCodeSection(node, TR::call, NULL, callLabel, callReturnLabel, cg);
      cg->getPPCOutOfLineCodeSectionList().push_front(outlinedHelperCall);

      generateLabelInstruction(cg, TR::InstOpCode::label, node, callReturnLabel);

      conditions->stopUsingDepRegs(cg, objReg);
      cg->decReferenceCount(objNode);
      }
   else
      {
      // read-only locks, readerReg = 0x0000 0000
      //    li     offsetReg, offset_of_monitor_word
      // loopLabel:
      //    lwarx  monitorReg, [objReg, offsetReg]
      //    rlwinm threadReg, monitorReg, 0, 0xFFFFFF83
      //    cmpi   cr0, threadReg, 0x0
      //    bne    callLabel
      //    addi   readerReg, monitorReg, 4
      //    stwcx  [objReg, offsetReg], readerReg
      //    bne    loopLabel
      //    isync
      // doneLabel:
      // callReturnLabel:
      // === OUT OF LINE ===
      // callLabel:
      //    bl     jitMonitorEntry
      //    b      callReturnLabel

      TR::LabelSymbol *doneLabel, *loopLabel;
      doneLabel = generateLabelSymbol(cg);
      loopLabel = generateLabelSymbol(cg);

      readerReg = cg->allocateRegister();
      addDependency(conditions, readerReg, TR::RealRegister::NoReg, TR_GPR, cg);

      if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
         {
         opCode = TR::InstOpCode::ldarx;
         lockSize = 8;
         }
      else
         {
         opCode = TR::InstOpCode::lwarx;
         lockSize = 4;
         }
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, offsetReg, lwOffset);
      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel, conditions);
      generateTrg1MemInstruction(cg, opCode, PPCOpProp_LoadReserveExclusiveAccess, node, monitorReg,
            new (cg->trHeapMemory()) TR::MemoryReference(baseReg, offsetReg, lockSize, cg));
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, threadReg, monitorReg, 0, 0xFFFFFF80);
      generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpi, node, condReg, threadReg, 0);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, callLabel, condReg);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, readerReg, monitorReg, 4);
      if (TR::Compiler->target.is64Bit() && !fej9->generateCompressedLockWord())
         opCode = TR::InstOpCode::stdcx_r;
      else
         opCode = TR::InstOpCode::stwcx_r;
      generateMemSrc1Instruction(cg, opCode, node, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, offsetReg, lockSize, cg), readerReg);

      if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
         // use PPC AS branch hint
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, node, loopLabel, condReg);
      else
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, loopLabel, condReg);

      if (TR::Compiler->target.isSMP())
         {
         // either an isync or a sync can be used here to prevent any
         // following loads from executing out-of-order
         // on nstar and pulsar a sync is cheaper, while an isync is
         // cheaper on other processors
         if (TR::Compiler->target.cpu.id() == TR_PPCnstar || TR::Compiler->target.cpu.id() == TR_PPCpulsar)
            generateInstruction(cg, TR::InstOpCode::sync, node);
         else
            generateInstruction(cg, TR::InstOpCode::isync, node);
         }
      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

      doneLabel->setEndInternalControlFlow();

      TR::LabelSymbol *callReturnLabel = generateLabelSymbol(cg);

      TR_PPCOutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_PPCOutOfLineCodeSection(node, TR::call, NULL, callLabel, callReturnLabel, cg);
      cg->getPPCOutOfLineCodeSectionList().push_front(outlinedHelperCall);

      generateLabelInstruction(cg, TR::InstOpCode::label, node, callReturnLabel);

      conditions->stopUsingDepRegs(cg, objReg);
      cg->decReferenceCount(objNode);
      }

   return (NULL);
   }

TR::Register *J9::Power::TreeEvaluator::VMarrayCheckEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   TR::Register *obj1Reg, *obj2Reg, *tmp1Reg, *tmp2Reg, *cndReg;
   TR::LabelSymbol *doneLabel, *snippetLabel;
   TR::Instruction *gcPoint;
   TR::Snippet *snippet;
   TR::RegisterDependencyConditions *conditions;
   int32_t depIndex;
   TR::Compilation* comp = cg->comp();

   obj1Reg = cg->evaluate(node->getFirstChild());
   obj2Reg = cg->evaluate(node->getSecondChild());
   doneLabel = generateLabelSymbol(cg);
   conditions = createConditionsAndPopulateVSXDeps(cg, 5);
   depIndex = 0;
   nonFixedDependency(conditions, obj1Reg, &depIndex, TR_GPR, true, cg);
   nonFixedDependency(conditions, obj2Reg, &depIndex, TR_GPR, true, cg);
   tmp1Reg = nonFixedDependency(conditions, NULL, &depIndex, TR_GPR, false, cg);
   tmp2Reg = nonFixedDependency(conditions, NULL, &depIndex, TR_GPR, false, cg);
   cndReg = cg->allocateRegister(TR_CCR);
   addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);

   // We have a unique snippet sharing arrangement in this code sequence.
   // It is not generally applicable for other situations.
   snippetLabel = NULL;

   // Same array, we are done.
   //
   generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, cndReg, obj1Reg, obj2Reg);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, cndReg);

   // If we know nothing about either object, test object1 first. It has to be an array.
   //
   if (!node->isArrayChkPrimitiveArray1() && !node->isArrayChkReferenceArray1() && !node->isArrayChkPrimitiveArray2() && !node->isArrayChkReferenceArray2())
      {
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
      generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, tmp1Reg,
            new (cg->trHeapMemory()) TR::MemoryReference(obj1Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), 4, cg));
#else
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, tmp1Reg, new (cg->trHeapMemory()) TR::MemoryReference(obj1Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), 4, cg));
#endif

      TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, tmp1Reg);
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, tmp1Reg, new (cg->trHeapMemory()) TR::MemoryReference(tmp1Reg, (int32_t) offsetof(J9Class, classDepthAndFlags), 4, cg));

      loadConstant(cg, node, (int32_t) J9_JAVA_CLASS_RAM_ARRAY, tmp2Reg);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::AND, node, tmp2Reg, tmp1Reg, tmp2Reg);

      generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpi, node, cndReg, tmp2Reg, NULLVALUE);

      snippetLabel = generateLabelSymbol(cg);
      gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, snippetLabel, cndReg);
      snippet = new (cg->trHeapMemory()) TR::PPCHelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference(), doneLabel);
      cg->addSnippet(snippet);
      }

   // One of the object is array. Test equality of two objects' classes.
   //
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
   // must read only 32 bits
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, tmp2Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(obj2Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), 4, cg));
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, tmp1Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(obj1Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), 4, cg));
#else
   generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, tmp2Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(obj2Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceAddress(), cg));
   generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, tmp1Reg,
         new (cg->trHeapMemory()) TR::MemoryReference(obj1Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), TR::Compiler->om.sizeofReferenceAddress(), cg));
#endif
   TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, tmp2Reg);
   TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, tmp1Reg);
   generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, cndReg, tmp1Reg, tmp2Reg);

   // If either object is known to be of primitive component type,
   // we are done: since both of them have to be of equal class.
   if (node->isArrayChkPrimitiveArray1() || node->isArrayChkPrimitiveArray2())
      {
      if (snippetLabel == NULL)
         {
         snippetLabel = generateLabelSymbol(cg);
         gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, snippetLabel, cndReg);
         snippet = new (cg->trHeapMemory()) TR::PPCHelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference(), doneLabel);
         cg->addSnippet(snippet);
         }
      else
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, snippetLabel, cndReg);
      }
   // We have to take care of the un-equal class situation: both of them must be of reference array
   else
      {
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, cndReg);

      // Object1 must be of reference component type, otherwise throw exception
      if (!node->isArrayChkReferenceArray1())
         {

         // Loading the Class Pointer -> classDepthandFlags
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
         generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, tmp1Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(obj1Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), 4, cg));
#else
         generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, tmp1Reg, new (cg->trHeapMemory()) TR::MemoryReference(obj1Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), 4, cg));
#endif

         TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, tmp1Reg);
         generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, tmp1Reg, new (cg->trHeapMemory()) TR::MemoryReference(tmp1Reg, (int32_t) offsetof(J9Class, classDepthAndFlags), 4, cg));

         // We already have classDepth&Flags in tmp1Reg.  X = (ramclass->ClassDepthAndFlags)>>J9_JAVA_CLASS_RAM_SHAPE_SHIFT
         generateShiftRightLogicalImmediate(cg, node, tmp1Reg, tmp1Reg, J9_JAVA_CLASS_RAM_SHAPE_SHIFT);

         // We need to perform a X & OBJECT_HEADER_SHAPE_MASK

         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp2Reg, tmp1Reg, 0, OBJECT_HEADER_SHAPE_MASK);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, cndReg, tmp2Reg, OBJECT_HEADER_SHAPE_POINTERS);

         if (snippetLabel == NULL)
            {
            snippetLabel = generateLabelSymbol(cg);
            gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, snippetLabel, cndReg);
            snippet = new (cg->trHeapMemory()) TR::PPCHelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference(), doneLabel);
            cg->addSnippet(snippet);
            }
         else
            generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, snippetLabel, cndReg);
         }

      // Object2 must be of reference component type array, otherwise throw exception
      if (!node->isArrayChkReferenceArray2())
         {
#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
         generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, tmp1Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(obj2Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), 4, cg));
#else
         generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, tmp1Reg, new (cg->trHeapMemory()) TR::MemoryReference(obj2Reg, (int32_t) TR::Compiler->om.offsetOfObjectVftField(), 4, cg));
#endif

         TR::TreeEvaluator::generateVFTMaskInstruction(cg, node, tmp1Reg);
         generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, tmp1Reg, new (cg->trHeapMemory()) TR::MemoryReference(tmp1Reg, (int32_t) offsetof(J9Class, classDepthAndFlags), 4, cg));

         loadConstant(cg, node, (int32_t) J9_JAVA_CLASS_RAM_ARRAY, tmp2Reg);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::AND, node, tmp2Reg, tmp1Reg, tmp2Reg);

         TR::Instruction * i = generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpi, node, cndReg, tmp2Reg, NULLVALUE);

         if (snippetLabel == NULL)
            {
            snippetLabel = generateLabelSymbol(cg);
            gcPoint = generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, snippetLabel, cndReg);
            snippet = new (cg->trHeapMemory()) TR::PPCHelperCallSnippet(cg, node, snippetLabel, node->getSymbolReference(), doneLabel);
            cg->addSnippet(snippet);
            }
         else
            generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, snippetLabel, cndReg);

         // We already have classDepth&Flags in tmp1Reg.  X = (ramclass->ClassDepthAndFlags)>>J9_JAVA_CLASS_RAM_SHAPE_SHIFT
         generateShiftRightLogicalImmediate(cg, node, tmp1Reg, tmp1Reg, J9_JAVA_CLASS_RAM_SHAPE_SHIFT);

         // We need to perform a X & OBJECT_HEADER_SHAPE_MASK

         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp2Reg, tmp1Reg, 0, OBJECT_HEADER_SHAPE_MASK);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, cndReg, tmp2Reg, OBJECT_HEADER_SHAPE_POINTERS);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, snippetLabel, cndReg);
         }
      }

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);
   if (snippetLabel != NULL)
      {
      gcPoint->PPCNeedsGCMap(0x0);
      snippet->gcMap().setGCRegisterMask(0x0);
      }

   conditions->stopUsingDepRegs(cg, obj1Reg, obj2Reg);

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   return (NULL);
   }

void J9::Power::TreeEvaluator::genArrayCopyWithArrayStoreCHK(TR::Node* node, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   TR::Register *metaReg = cg->getMethodMetaDataRegister();
   TR::Register *gr3Reg, *gr2Reg, *lengthReg, *temp1Reg;

   //      child 0 ------  Source array object;
   //      child 1 ------  Destination array object;
   //      child 2 ------  Source byte address;
   //      child 3 ------  Destination byte address;
   //      child 4 ------  Copy length in byte;

   bool aix_style_linkage = (TR::Compiler->target.isAIX() || (TR::Compiler->target.is64Bit() && TR::Compiler->target.isLinux()));
   TR::PPCJNILinkage *jniLinkage = (TR::PPCJNILinkage*) cg->getLinkage(TR_J9JNILinkage);
   const TR::PPCLinkageProperties &pp = jniLinkage->getProperties();
   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(pp.getNumberOfDependencyGPRegisters(),
         pp.getNumberOfDependencyGPRegisters(), cg->trMemory());

   // I_32 referenceArrayCopy(
   //                   J9VMThread *vmThread,
   //                   J9IndexableObjectContiguous *srcObject,
   //                   J9IndexableObjectContiguous *destObject,
   //                   U_8 *srcAddress,
   //                   U_8 *destAddress,
   //                   I_32 lengthInSlots)
   intptrj_t *funcdescrptr = (intptrj_t*) fej9->getReferenceArrayCopyHelperAddress();

   TR::Instruction *iCursor;
   if (aix_style_linkage)
      {
      gr2Reg = cg->allocateRegister();
      addDependency(conditions, gr2Reg, TR::RealRegister::gr2, TR_GPR, cg);
      }
   // set up the arguments and dependencies
   int32_t argSize = jniLinkage->buildJNIArgs(node, conditions, pp, true);
   temp1Reg = conditions->searchPreConditionRegister(TR::RealRegister::gr12);
   if (aix_style_linkage &&
      !(TR::Compiler->target.is64Bit() && TR::Compiler->target.isLinux() && TR::Compiler->target.cpu.isLittleEndian()))
      {
      intptrj_t target_ip = funcdescrptr[0];
      intptrj_t target_toc = funcdescrptr[1];
      iCursor = loadAddressConstant(cg, node, target_ip, temp1Reg, NULL, false, TR_ArrayCopyHelper);
      iCursor = loadAddressConstant(cg, node, target_toc, gr2Reg, iCursor, false, TR_ArrayCopyToc);
      }
   else
      {
      iCursor = loadAddressConstant(cg, node, (intptrj_t) funcdescrptr, temp1Reg, NULL, false, TR_ArrayCopyHelper);
      }

   iCursor = generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, temp1Reg, NULL, iCursor);
   // the C routine expects length measured by slots
   lengthReg = conditions->searchPreConditionRegister(TR::RealRegister::gr8);
   int32_t elementSize;
   if (comp->useCompressedPointers())
      elementSize = TR::Compiler->om.sizeofReferenceField();
   else
      elementSize = (int32_t) TR::Compiler->om.sizeofReferenceAddress();
   generateShiftRightLogicalImmediate(cg, node, lengthReg, lengthReg, trailingZeroes(elementSize));
   // pass vmThread as the first parameter
   gr3Reg = cg->allocateRegister();
   addDependency(conditions, gr3Reg, TR::RealRegister::gr3, TR_GPR, cg);
   iCursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, gr3Reg, metaReg, iCursor);
   // call the C routine
   TR::Instruction *gcPoint = generateDepInstruction(cg, TR::InstOpCode::bctrl, node, conditions);
   gcPoint->PPCNeedsGCMap(pp.getPreservedRegisterMapForGC());
   // check return value
   TR::Register *cr0Reg = conditions->searchPreConditionRegister(TR::RealRegister::cr0);
   generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpi, node, cr0Reg, gr3Reg, -1);
   // throw exception if needed
   TR::SymbolReference *throwSymRef = comp->getSymRefTab()->findOrCreateArrayStoreExceptionSymbolRef(comp->getJittedMethodSymbol());
   TR::LabelSymbol *exceptionSnippetLabel = cg->lookUpSnippet(TR::Snippet::IsHelperCall, throwSymRef);
   if (exceptionSnippetLabel == NULL)
      {
      exceptionSnippetLabel = generateLabelSymbol(cg);
      cg->addSnippet(new (cg->trHeapMemory()) TR::PPCHelperCallSnippet(cg, node, exceptionSnippetLabel, throwSymRef));
      }

   gcPoint = generateDepConditionalBranchInstruction(cg, TR::InstOpCode::bnel, node, exceptionSnippetLabel, cr0Reg, conditions->cloneAndFix(cg));
   // somewhere to hang the dependencies
   TR::LabelSymbol *depLabel = generateLabelSymbol(cg);
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, depLabel, conditions);
   gcPoint->PPCNeedsGCMap(pp.getPreservedRegisterMapForGC());
   cg->machine()->setLinkRegisterKilled(true);
   conditions->stopUsingDepRegs(cg);
   cg->setHasCall();
   return;
   }

void J9::Power::TreeEvaluator::genWrtbarForArrayCopy(TR::Node *node, TR::Register *srcObjReg, TR::Register *dstObjReg, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   bool ageCheckIsNeeded = false;
   bool cardMarkIsNeeded = false;
   TR_WriteBarrierKind gcMode = comp->getOptions()->getGcMode();

   ageCheckIsNeeded = (gcMode == TR_WrtbarOldCheck || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarAlways);
   cardMarkIsNeeded = (gcMode == TR_WrtbarCardMark || gcMode == TR_WrtbarCardMarkIncremental);

   if (!ageCheckIsNeeded && !cardMarkIsNeeded)
      return;

   if (ageCheckIsNeeded)
      {
      TR::Register *temp1Reg;
      TR::Register *temp2Reg;
      TR::Register *condReg = cg->allocateRegister(TR_CCR);
      TR::Instruction *gcPoint;
      TR::RegisterDependencyConditions *conditions;

      if (gcMode != TR_WrtbarAlways)
         {
         temp1Reg = cg->allocateRegister();
         temp2Reg = cg->allocateRegister();
         conditions = createConditionsAndPopulateVSXDeps(cg, 4);
         addDependency(conditions, temp1Reg, TR::RealRegister::gr11, TR_GPR, cg);
         addDependency(conditions, temp2Reg, TR::RealRegister::NoReg, TR_GPR, cg);
         }
      else
         {
         conditions = createConditionsAndPopulateVSXDeps(cg, 2);
         }

      addDependency(conditions, dstObjReg, TR::RealRegister::gr3, TR_GPR, cg);
      addDependency(conditions, condReg, TR::RealRegister::cr0, TR_CCR, cg);

      TR::LabelSymbol *doneLabel;
      TR::SymbolReference *wbRef = comp->getSymRefTab()->findOrCreateWriteBarrierBatchStoreSymbolRef(comp->getMethodSymbol());

      if (gcMode != TR_WrtbarAlways)
         {
         doneLabel = generateLabelSymbol(cg);

         TR::Register *metaReg = cg->getMethodMetaDataRegister();

         // temp1Reg = dstObjReg - heapBaseForBarrierRange0
         generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp1Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapBaseForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg));
         generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, temp1Reg, temp1Reg, dstObjReg);

         // if (temp1Reg >= heapSizeForBarrierRage0), object not in the tenured area
         generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, temp2Reg,
               new (cg->trHeapMemory()) TR::MemoryReference(metaReg, offsetof(J9VMThread, heapSizeForBarrierRange0), TR::Compiler->om.sizeofReferenceAddress(), cg));
         generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, temp1Reg, temp2Reg);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, node, doneLabel, condReg);
         }

      gcPoint = generateDepImmSymInstruction(cg, TR::InstOpCode::bl, node, (uintptrj_t) wbRef->getSymbol()->castToMethodSymbol()->getMethodAddress(),
            new (cg->trHeapMemory()) TR::RegisterDependencyConditions((uint8_t) 0, 0, cg->trMemory()), wbRef, NULL);

      if (gcMode != TR_WrtbarAlways)
         generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

      cg->machine()->setLinkRegisterKilled(true);
      cg->setHasCall();

      if (gcMode != TR_WrtbarAlways)
         {
         cg->stopUsingRegister(temp1Reg);
         cg->stopUsingRegister(temp2Reg);
         }
      cg->stopUsingRegister(condReg);
      // This GC point can only happen when there is an exception. As a result, we can ditch
      // all registers.
      gcPoint->PPCNeedsGCMap(0xFFFFFFFF);
      }

   if (!ageCheckIsNeeded && cardMarkIsNeeded)
      {
      if (!TR::Options::getCmdLineOptions()->realTimeGC())

         {
         TR::Register *cndReg = cg->allocateRegister(TR_CCR);
         TR::Register *temp1Reg = cg->allocateRegister();
         TR::Register *temp2Reg = cg->allocateRegister();
         TR::Register *temp3Reg = cg->allocateRegister();
         TR::RegisterDependencyConditions *conditions = createConditionsAndPopulateVSXDeps(cg, 7);

         addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);
         addDependency(conditions, dstObjReg, TR::RealRegister::NoReg, TR_GPR, cg);
         addDependency(conditions, temp1Reg, TR::RealRegister::NoReg, TR_GPR, cg);
         conditions->getPostConditions()->getRegisterDependency(1)->setExcludeGPR0();
         conditions->getPostConditions()->getRegisterDependency(2)->setExcludeGPR0();
         addDependency(conditions, temp2Reg, TR::RealRegister::NoReg, TR_GPR, cg);
         addDependency(conditions, temp3Reg, TR::RealRegister::NoReg, TR_GPR, cg);

         VMCardCheckEvaluator(node, dstObjReg, cndReg, temp1Reg, temp2Reg, temp3Reg, conditions, cg);
         generateDepLabelInstruction(cg, TR::InstOpCode::label, node, generateLabelSymbol(cg), conditions);

         cg->stopUsingRegister(cndReg);
         cg->stopUsingRegister(temp1Reg);
         cg->stopUsingRegister(temp2Reg);
         cg->stopUsingRegister(temp3Reg);
         }
      else
         TR_ASSERT(0, "genWrtbarForArrayCopy card marking not supported for RT");
      }
   }

static TR::Register *genCAS(TR::Node *node, TR::CodeGenerator *cg, TR::Register *objReg, TR::Register *offsetReg, TR::Register *oldVReg, TR::Register *newVReg, TR::Register *cndReg,
      TR::LabelSymbol *doneLabel, TR::Node *objNode, int32_t oldValue, bool oldValueInReg, bool isLong, bool casWithoutSync = false)
   {
   TR::Register *resultReg = cg->allocateRegister();
   TR::Instruction *gcPoint;

   // Memory barrier --- NOTE: we should be able to do a test upfront to save this barrier,
   //                          but Hursley advised to be conservative due to lack of specification.
   generateInstruction(cg, TR::InstOpCode::lwsync, node);

   TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel);

   generateTrg1MemInstruction(cg, isLong ? TR::InstOpCode::ldarx : TR::InstOpCode::lwarx, node, resultReg, new (cg->trHeapMemory()) TR::MemoryReference(objReg, offsetReg, isLong ? 8 : 4, cg));
   if (oldValueInReg)
      generateTrg1Src2Instruction(cg, isLong ? TR::InstOpCode::cmp8 : TR::InstOpCode::cmp4, node, cndReg, resultReg, oldVReg);
   else
      generateTrg1Src1ImmInstruction(cg, isLong ? TR::InstOpCode::cmpi8 : TR::InstOpCode::cmpi4, node, cndReg, resultReg, oldValue);
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 0);

   // We don't know how the compare will fare such that we don't dictate the prediction
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, doneLabel, cndReg);

   generateMemSrc1Instruction(cg, isLong ? TR::InstOpCode::stdcx_r : TR::InstOpCode::stwcx_r, node, new (cg->trHeapMemory()) TR::MemoryReference(objReg, offsetReg, isLong ? 8 : 4, cg), newVReg);
   // We expect this store is usually successful, i.e., the following branch will not be taken
   if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, node, loopLabel, cndReg);
   else
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, loopLabel, cndReg);

   // We deviate from the VM helper here: no-store-no-barrier instead of always-barrier
   if (!casWithoutSync)
      generateInstruction(cg, TR::InstOpCode::sync, node);
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 1);

   node->setRegister(resultReg);
   return resultReg;
   }

static TR::Register *VMinlineCompareAndSwap(TR::Node *node, TR::CodeGenerator *cg, bool isLong)
   {
   TR::Compilation * comp = cg->comp();
   TR::Register *objReg, *offsetReg, *oldVReg, *newVReg, *resultReg, *cndReg;
   TR::Node *firstChild, *secondChild, *thirdChild, *fourthChild, *fifthChild;
   TR::RegisterDependencyConditions *conditions;
   TR::LabelSymbol *doneLabel;
   intptrj_t offsetValue, oldValue;
   bool oldValueInReg = true, freeOffsetReg = false;
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());

   firstChild = node->getFirstChild();
   secondChild = node->getSecondChild();
   thirdChild = node->getChild(2);
   fourthChild = node->getChild(3);
   fifthChild = node->getChild(4);
   objReg = cg->evaluate(secondChild);

   // VM helper chops off the value in 32bit, and we don't want the whole long value either
   if (thirdChild->getOpCode().isLoadConst() && thirdChild->getRegister() == NULL && TR::Compiler->target.is32Bit())
      {
      offsetValue = thirdChild->getLongInt();
      offsetReg = cg->allocateRegister();
      loadConstant(cg, node, (int32_t) offsetValue, offsetReg);
      freeOffsetReg = true;
      }
   else
      {
      offsetReg = cg->evaluate(thirdChild);
      if (TR::Compiler->target.is32Bit())
         offsetReg = offsetReg->getLowOrder();
      }

   if (fourthChild->getOpCode().isLoadConst() && fourthChild->getRegister() == NULL)
      {
      if (isLong)
         oldValue = fourthChild->getLongInt();
      else
         oldValue = fourthChild->getInt();
      if (oldValue >= LOWER_IMMED && oldValue <= UPPER_IMMED)
         oldValueInReg = false;
      }
   if (oldValueInReg)
      oldVReg = cg->evaluate(fourthChild);
   newVReg = cg->evaluate(fifthChild);
   cndReg = cg->allocateRegister(TR_CCR);
   doneLabel = generateLabelSymbol(cg);

   bool casWithoutSync = false;
   TR_OpaqueMethodBlock *caller = node->getOwningMethod();
   if (caller)
      {
      TR_ResolvedMethod *m = fej9->createResolvedMethod(cg->trMemory(), caller, node->getSymbolReference()->getOwningMethod(comp));
      if ((m->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicInteger_weakCompareAndSet)
            || (m->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicLong_weakCompareAndSet)
            || (m->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicReference_weakCompareAndSet))
         {
         casWithoutSync = true;
         }
      }

   resultReg = genCAS(node, cg, objReg, offsetReg, oldVReg, newVReg, cndReg, doneLabel, secondChild, oldValue, oldValueInReg, isLong, casWithoutSync);

   conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(6, 6, cg->trMemory());
   addDependency(conditions, objReg, TR::RealRegister::NoReg, TR_GPR, cg);
   conditions->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0();
   addDependency(conditions, offsetReg, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, resultReg, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, newVReg, TR::RealRegister::NoReg, TR_GPR, cg);
   if (oldValueInReg)
      addDependency(conditions, oldVReg, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

   cg->stopUsingRegister(cndReg);
   cg->recursivelyDecReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   if (freeOffsetReg)
      {
      cg->stopUsingRegister(offsetReg);
      cg->recursivelyDecReferenceCount(thirdChild);
      }
   else
      cg->decReferenceCount(thirdChild);
   if (oldValueInReg)
      cg->decReferenceCount(fourthChild);
   else
      cg->recursivelyDecReferenceCount(fourthChild);
   cg->decReferenceCount(fifthChild);
   return resultReg;
   }

static TR::Register *VMinlineCompareAndSwapObject(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (comp->fe());
   TR::Register *objReg, *offsetReg, *oldVReg, *newVReg, *resultReg, *cndReg;
   TR::Node *firstChild, *secondChild, *thirdChild, *fourthChild, *fifthChild;
   TR::RegisterDependencyConditions *conditions;
   TR::LabelSymbol *doneLabel, *storeLabel, *wrtBarEndLabel;
   intptrj_t offsetValue;
   bool freeOffsetReg = false;
   bool needDup = false;

   TR_WriteBarrierKind gcMode = comp->getOptions()->getGcMode();
   bool doWrtBar = (gcMode == TR_WrtbarRealTime || gcMode == TR_WrtbarOldCheck || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarAlways
         || TR::Options::getCmdLineOptions()->realTimeGC());
   bool doCrdMrk = (gcMode == TR_WrtbarCardMark || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarCardMarkIncremental);

   firstChild = node->getFirstChild();
   secondChild = node->getSecondChild();
   thirdChild = node->getChild(2);
   fourthChild = node->getChild(3);
   fifthChild = node->getChild(4);
   objReg = cg->evaluate(secondChild);

   // VM helper chops off the value in 32bit, and we don't want the whole long value either
   if (thirdChild->getOpCode().isLoadConst() && thirdChild->getRegister() == NULL && TR::Compiler->target.is32Bit())
      {
      offsetValue = thirdChild->getLongInt();
      offsetReg = cg->allocateRegister();
      loadConstant(cg, node, (int32_t) offsetValue, offsetReg);
      freeOffsetReg = true;
      }
   else
      {
      offsetReg = cg->evaluate(thirdChild);
      if (TR::Compiler->target.is32Bit())
         offsetReg = offsetReg->getLowOrder();
      }

   oldVReg = cg->evaluate(fourthChild);

   TR::Node *translatedNode = fifthChild;
   bool bumpedRefCount = false;
   if (comp->useCompressedPointers() && (fifthChild->getDataType() != TR::Address))
      {
      bool usingLowMemHeap = false;
      bool useShiftedOffsets = (TR::Compiler->om.compressedReferenceShiftOffset() != 0);
      bool usingCompressedPointers = false;

      if (TR::Compiler->vm.heapBaseAddress() == 0 || fifthChild->isNull())
         usingLowMemHeap = true;

      translatedNode = fifthChild;
      if (translatedNode->getOpCode().isConversion())
         translatedNode = translatedNode->getFirstChild();
      if (translatedNode->getOpCode().isRightShift()) // optional
         translatedNode = translatedNode->getFirstChild();

      if (translatedNode->getOpCode().isSub() || usingLowMemHeap)
         usingCompressedPointers = true;

      translatedNode = fifthChild;
      if (usingCompressedPointers)
         {
         if (!usingLowMemHeap || useShiftedOffsets)
            {
            while ((translatedNode->getNumChildren() > 0) && (translatedNode->getOpCodeValue() != TR::a2l))
               translatedNode = translatedNode->getFirstChild();

            if (translatedNode->getOpCodeValue() == TR::a2l)
               translatedNode = translatedNode->getFirstChild();

            // this is required so that different registers are
            // allocated for the actual store and translated values
            translatedNode->incReferenceCount();
            bumpedRefCount = true;
            }
         }
      }

   newVReg = cg->evaluate(fifthChild);
   if (objReg == newVReg)
      {
      newVReg = cg->allocateCollectedReferenceRegister();
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, newVReg, objReg);
      needDup = true;
      }
   cndReg = cg->allocateRegister(TR_CCR);
   doneLabel = generateLabelSymbol(cg);
   storeLabel = generateLabelSymbol(cg);

   if (TR::Options::getCmdLineOptions()->realTimeGC())
      wrtBarEndLabel = storeLabel;
   else
      wrtBarEndLabel = doneLabel;

   if (!TR::Options::getCmdLineOptions()->realTimeGC())
      resultReg = genCAS(node, cg, objReg, offsetReg, oldVReg, newVReg, cndReg, doneLabel, secondChild, 0, true, (TR::Compiler->target.is64Bit() && !comp->useCompressedPointers()));

   uint32_t numDeps = (doWrtBar || doCrdMrk) ? 13 : 11;
   conditions = createConditionsAndPopulateVSXDeps(cg, numDeps);

   if (doWrtBar && doCrdMrk)
      {
      TR::Register *temp1Reg = cg->allocateRegister(), *temp2Reg = cg->allocateRegister(), *temp3Reg, *temp4Reg = cg->allocateRegister();
      addDependency(conditions, objReg, TR::RealRegister::gr3, TR_GPR, cg);
      TR::Register *wrtbarSrcReg;
      if (translatedNode != fifthChild)
         {
         addDependency(conditions, newVReg, TR::RealRegister::NoReg, TR_GPR, cg);
         addDependency(conditions, translatedNode->getRegister(), TR::RealRegister::gr4, TR_GPR, cg);
         wrtbarSrcReg = translatedNode->getRegister();
         }
      else
         {
         addDependency(conditions, newVReg, TR::RealRegister::gr4, TR_GPR, cg);
         wrtbarSrcReg = newVReg;
         }

      addDependency(conditions, temp1Reg, TR::RealRegister::gr11, TR_GPR, cg);
      addDependency(conditions, temp2Reg, TR::RealRegister::NoReg, TR_GPR, cg);
      addDependency(conditions, offsetReg, TR::RealRegister::NoReg, TR_GPR, cg);
      addDependency(conditions, temp4Reg, TR::RealRegister::NoReg, TR_GPR, cg);

      if (freeOffsetReg)
         {
         temp3Reg = offsetReg;
         }
      else
         {
         temp3Reg = cg->allocateRegister();
         addDependency(conditions, temp3Reg, TR::RealRegister::NoReg, TR_GPR, cg);
         }

      if (!fifthChild->isNonNull())
         {
         generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpi, node, cndReg, newVReg, NULLVALUE);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, cndReg);
         }

      VMnonNullSrcWrtBarCardCheckEvaluator(node, comp->useCompressedPointers() ? wrtbarSrcReg : newVReg, objReg, cndReg, temp1Reg, temp2Reg, temp3Reg, temp4Reg, doneLabel,
            conditions, NULL, false, false, cg);

      cg->stopUsingRegister(temp1Reg);
      cg->stopUsingRegister(temp2Reg);
      if (!freeOffsetReg)
         cg->stopUsingRegister(temp3Reg);
      cg->stopUsingRegister(temp4Reg);
      }
   else if (doWrtBar && !doCrdMrk)
      {
      TR::Register *temp1Reg = cg->allocateRegister(), *temp2Reg;
      addDependency(conditions, objReg, TR::RealRegister::gr3, TR_GPR, cg);

      if (newVReg != translatedNode->getRegister())
         {
         addDependency(conditions, newVReg, TR::RealRegister::NoReg, TR_GPR, cg);
         if (TR::Options::getCmdLineOptions()->realTimeGC())
            addDependency(conditions, translatedNode->getRegister(), TR::RealRegister::gr5, TR_GPR, cg);
         else
            addDependency(conditions, translatedNode->getRegister(), TR::RealRegister::gr4, TR_GPR, cg);
         }
      else
         {
         if (TR::Options::getCmdLineOptions()->realTimeGC())
            addDependency(conditions, newVReg, TR::RealRegister::gr5, TR_GPR, cg);
         else
            addDependency(conditions, newVReg, TR::RealRegister::gr4, TR_GPR, cg);
         }

      addDependency(conditions, temp1Reg, TR::RealRegister::NoReg, TR_GPR, cg);

      //Realtime needs the offsetReg to be preserved after the wrtbar to do the store in genCAS()
      if (freeOffsetReg && !TR::Options::getCmdLineOptions()->realTimeGC())
         {
         addDependency(conditions, offsetReg, TR::RealRegister::gr11, TR_GPR, cg);
         temp2Reg = offsetReg;
         }
      else
         {
         temp2Reg = cg->allocateRegister();
         addDependency(conditions, temp2Reg, TR::RealRegister::gr11, TR_GPR, cg);
         addDependency(conditions, offsetReg, TR::RealRegister::NoReg, TR_GPR, cg);
         }

      if (!fifthChild->isNonNull())
         {
         generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpi, node, cndReg, newVReg, NULLVALUE);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, wrtBarEndLabel, cndReg);
         }

      TR::Register *dstAddrReg = NULL;
      bool dstAddrComputed = false;

      if (TR::Options::getCmdLineOptions()->realTimeGC())
         {
         dstAddrReg = cg->allocateRegister();
         addDependency(conditions, dstAddrReg, TR::RealRegister::gr4, TR_GPR, cg);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, dstAddrReg, objReg, offsetReg);
         dstAddrComputed = true;
         }

      VMnonNullSrcWrtBarCardCheckEvaluator(node, comp->useCompressedPointers() ? translatedNode->getRegister() : newVReg, objReg, cndReg, temp1Reg, temp2Reg, dstAddrReg, NULL,
            wrtBarEndLabel, conditions, NULL, dstAddrComputed, false, cg);

      if (TR::Options::getCmdLineOptions()->realTimeGC())
         cg->stopUsingRegister(dstAddrReg);

      cg->stopUsingRegister(temp1Reg);
      if (!freeOffsetReg || TR::Options::getCmdLineOptions()->realTimeGC())
         cg->stopUsingRegister(temp2Reg);
      }
   else if (!doWrtBar && doCrdMrk)
      {
      TR::Register *temp1Reg = cg->allocateRegister(), *temp2Reg = cg->allocateRegister(), *temp3Reg;
      addDependency(conditions, objReg, TR::RealRegister::NoReg, TR_GPR, cg);
      conditions->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0();
      addDependency(conditions, temp1Reg, TR::RealRegister::NoReg, TR_GPR, cg);
      conditions->getPostConditions()->getRegisterDependency(1)->setExcludeGPR0();
      if (newVReg != translatedNode->getRegister())
         addDependency(conditions, newVReg, TR::RealRegister::NoReg, TR_GPR, cg);
      addDependency(conditions, translatedNode->getRegister(), TR::RealRegister::NoReg, TR_GPR, cg);

      addDependency(conditions, temp2Reg, TR::RealRegister::NoReg, TR_GPR, cg);
      addDependency(conditions, offsetReg, TR::RealRegister::NoReg, TR_GPR, cg);
      if (freeOffsetReg)
         {
         temp3Reg = offsetReg;
         }
      else
         {
         temp3Reg = cg->allocateRegister();
         addDependency(conditions, temp3Reg, TR::RealRegister::NoReg, TR_GPR, cg);
         }

      VMCardCheckEvaluator(node, objReg, cndReg, temp1Reg, temp2Reg, temp3Reg, conditions, cg);

      cg->stopUsingRegister(temp1Reg);
      cg->stopUsingRegister(temp2Reg);
      if (!freeOffsetReg)
         cg->stopUsingRegister(temp3Reg);
      }
   else
      {
      addDependency(conditions, objReg, TR::RealRegister::NoReg, TR_GPR, cg);
      conditions->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0();
      addDependency(conditions, offsetReg, TR::RealRegister::NoReg, TR_GPR, cg);
      if (newVReg != translatedNode->getRegister())
         addDependency(conditions, newVReg, TR::RealRegister::NoReg, TR_GPR, cg);
      addDependency(conditions, translatedNode->getRegister(), TR::RealRegister::NoReg, TR_GPR, cg);
      }

   generateLabelInstruction(cg, TR::InstOpCode::label, node, storeLabel);

   if (TR::Options::getCmdLineOptions()->realTimeGC())
      resultReg = genCAS(node, cg, objReg, offsetReg, oldVReg, newVReg, cndReg, doneLabel, secondChild, 0, true, (TR::Compiler->target.is64Bit() && !comp->useCompressedPointers()));

   addDependency(conditions, resultReg, TR::RealRegister::NoReg, TR_GPR, cg);
   if (oldVReg != newVReg && oldVReg != objReg)
      addDependency(conditions, oldVReg, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

   if (needDup)
      cg->stopUsingRegister(newVReg);
   cg->stopUsingRegister(cndReg);
   cg->recursivelyDecReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   if (freeOffsetReg)
      {
      cg->stopUsingRegister(offsetReg);
      cg->recursivelyDecReferenceCount(thirdChild);
      }
   else
      cg->decReferenceCount(thirdChild);
   cg->decReferenceCount(fourthChild);
   cg->decReferenceCount(fifthChild);
   if (bumpedRefCount)
      cg->decReferenceCount(translatedNode);

   return resultReg;
   }


void J9::Power::TreeEvaluator::restoreTOCRegister(TR::Node *node, TR::CodeGenerator *cg, TR::RegisterDependencyConditions *dependencies)
   {
#if defined(TR_HOST_POWER)
   TR::Compilation* comp = cg->comp();
   TR::Register *tocReg = dependencies->searchPreConditionRegister(TR::RealRegister::gr2);
   generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, tocReg,
         new (cg->trHeapMemory()) TR::MemoryReference(cg->getMethodMetaDataRegister(), offsetof(J9VMThread, jitTOC), TR::Compiler->om.sizeofReferenceAddress(), cg));
#else
   TR_ASSERT(0, "direct C calls directly are not supported on this platform.\n");
#endif
   }

void J9::Power::TreeEvaluator::buildArgsProcessFEDependencies(TR::Node *node, TR::CodeGenerator *cg, TR::RegisterDependencyConditions *dependencies)
   {
   //Java uses gr2 as a volatile for targets except for 32-bit Linux.
   bool aix_style_linkage = (TR::Compiler->target.isAIX() || (TR::Compiler->target.is64Bit() && TR::Compiler->target.isLinux()));

   if(aix_style_linkage)
      addDependency(dependencies, NULL, TR::RealRegister::gr2, TR_GPR, cg);
   }

TR::Register *J9::Power::TreeEvaluator::retrieveTOCRegister(TR::Node *node, TR::CodeGenerator *cg, TR::RegisterDependencyConditions *dependencies)
   {
#if defined(DEBUG)
   //We should not land here if we're compiling for 32-bit Linux.
   bool aix_style_linkage = (TR::Compiler->target.isAIX() || (TR::Compiler->target.is64Bit() && TR::Compiler->target.isLinux()));
   TR_ASSERT(aix_style_linkage, "Landed to restore gr2 for TOC with 32bit Linux as target\n");
#endif

   TR::Register *grTOCReg=dependencies->searchPreConditionRegister(TR::RealRegister::gr2);
   TR_ASSERT(grTOCReg != NULL, "Dependency not set in J9 Java on gr2 for TOC.\n");
   return grTOCReg;
   }


static TR::Register *inlineAtomicOps(TR::Node *node, TR::CodeGenerator *cg, int8_t size, TR::MethodSymbol *method, bool isArray)
   {
   TR::Node *valueChild = node->getFirstChild();
   TR::Node *deltaChild = NULL;
   TR::Register *valueReg = cg->evaluate(valueChild);
   TR::Register *deltaReg = NULL;
   TR::Register *resultReg = cg->allocateRegister();
   TR::Register *cndReg = cg->allocateRegister(TR_CCR);
   TR::Register *fieldOffsetReg = cg->allocateRegister();
   TR::Register *tempReg = NULL;
   int32_t delta = 0;
   int32_t numDeps = 5;
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   bool isAddOp = true;
   bool isGetAndOp = true;
   bool isLong = false;
   bool isArgConstant = false;
   bool isArgImmediate = false;
   bool isArgImmediateShifted = false;
   TR::RecognizedMethod currentMethod = method->getRecognizedMethod();

   switch (currentMethod)
      {
   case TR::java_util_concurrent_atomic_AtomicBoolean_getAndSet:
   case TR::java_util_concurrent_atomic_AtomicInteger_getAndSet:
   case TR::java_util_concurrent_atomic_AtomicLong_getAndSet:
   case TR::java_util_concurrent_atomic_AtomicReference_getAndSet:
   case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndSet:
   case TR::java_util_concurrent_atomic_AtomicLongArray_getAndSet:
   case TR::java_util_concurrent_atomic_AtomicReferenceArray_getAndSet:
      {
      isAddOp = false;
      break;
      }
   case TR::java_util_concurrent_atomic_AtomicInteger_addAndGet:
   case TR::java_util_concurrent_atomic_AtomicIntegerArray_addAndGet:
      {
      isGetAndOp = false;
      }
   case TR::java_util_concurrent_atomic_AtomicInteger_getAndAdd:
   case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndAdd:
      {
      break;
      }

   case TR::java_util_concurrent_atomic_AtomicInteger_incrementAndGet:
   case TR::java_util_concurrent_atomic_AtomicIntegerArray_incrementAndGet:
      {
      isGetAndOp = false;
      }
   case TR::java_util_concurrent_atomic_AtomicInteger_getAndIncrement:
   case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndIncrement:
      {
      delta = (int32_t) 1;
      isArgConstant = true;
      isArgImmediate = true;
      break;
      }
   case TR::java_util_concurrent_atomic_AtomicInteger_decrementAndGet:
   case TR::java_util_concurrent_atomic_AtomicIntegerArray_decrementAndGet:
      {
      isGetAndOp = false;
      }
   case TR::java_util_concurrent_atomic_AtomicInteger_getAndDecrement:
   case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndDecrement:
      {
      delta = (int32_t) - 1;
      isArgConstant = true;
      isArgImmediate = true;
      break;
      }
      }

   if (node->getNumChildren() > 1)
      deltaChild = node->getSecondChild();

   //determine if the delta is a constant.
   if (deltaChild && deltaChild->getOpCode().isLoadConst() && !deltaChild->getRegister() && deltaChild->getDataType() == TR::Int32)
      {
      delta = (int32_t)(deltaChild->getInt());
      isArgConstant = true;

      //determine if the constant can be represented as an immediate
      if (delta <= UPPER_IMMED && delta >= LOWER_IMMED)
         {
         // avoid evaluating immediates for add operations
         isArgImmediate = true;
         }
      else if (delta & 0xFFFF == 0 && (delta & 0xFFFF0000) >> 16 <= UPPER_IMMED && (delta & 0xFFFF0000) >> 16 >= LOWER_IMMED)
         {
         // avoid evaluating shifted immediates for add operations
         isArgImmediate = true;
         isArgImmediateShifted = true;
         }
      else
         {
         // evaluate non-immediate constants since there may be reuse
         // and they have to go into a reg anyway
         tempReg = cg->evaluate(deltaChild);
         }
      }
   else if (deltaChild)
      tempReg = cg->evaluate(deltaChild);

   //determine the offset of the value field
   int32_t fieldOffset = 0;
   int32_t shiftAmount = 0;
   TR::Node *indexChild = NULL;
   TR::Register *indexRegister = NULL;

   TR::Register *scratchRegister = NULL;

   if (!isArray)
      {
      TR_OpaqueClassBlock * bdClass;
      char *className, *fieldSig;
      int32_t classNameLen, fieldSigLen;

      fieldSigLen = 1;

      switch (currentMethod)
         {
      case TR::java_util_concurrent_atomic_AtomicBoolean_getAndSet:
         className = "Ljava/util/concurrent/atomic/AtomicBoolean;";
         classNameLen = 43;
         fieldSig = "I"; // not a typo, the field is int
         break;
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicInteger_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicInteger_incrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicInteger_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndDecrement:
         className = "Ljava/util/concurrent/atomic/AtomicInteger;";
         classNameLen = 43;
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
         classNameLen = 40;
         fieldSig = "J";
         break;
      case TR::java_util_concurrent_atomic_AtomicReference_getAndSet:
         className = "Ljava/util/concurrent/atomic/AtomicReference;";
         classNameLen = 45;
         fieldSig = "Ljava/lang/Object;";
         fieldSigLen = 18;
         break;
      default:
         TR_ASSERT(0, "Unknown atomic operation method\n");
         return NULL;
         }

      TR_ResolvedMethod *owningMethod = node->getSymbolReference()->getOwningMethod(comp);
      TR_OpaqueClassBlock *containingClass = fej9->getClassFromSignature(className, classNameLen, owningMethod, true);
      fieldOffset = fej9->getInstanceFieldOffset(containingClass, "value", 5, fieldSig, fieldSigLen, true) + fej9->getObjectHeaderSizeInBytes(); // size of a J9 object header
      }
   else
      {
      if (isArray)
         {
         indexChild = node->getChild(1);
         indexRegister = cg->evaluate(indexChild);
         fieldOffset = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
         if (size == 4)
            shiftAmount = 2;
         else if (size == 8)
            shiftAmount = 3;

         TR_OpaqueClassBlock * bdClass;
         char *className, *fieldSig;
         int32_t classNameLen, fieldSigLen;

         fieldSigLen = 1;

         switch (currentMethod)
            {
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndSet:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_incrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndIncrement:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_decrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndDecrement:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_addAndGet:
         case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndAdd:
            className = "Ljava/util/concurrent/atomic/AtomicIntegerArray;";
            classNameLen = 48;
            fieldSig = "[I";
            fieldSigLen = 2;
            break;

         case TR::java_util_concurrent_atomic_AtomicLongArray_getAndSet:
         case TR::java_util_concurrent_atomic_AtomicLongArray_incrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicLongArray_getAndIncrement:
         case TR::java_util_concurrent_atomic_AtomicLongArray_decrementAndGet:
         case TR::java_util_concurrent_atomic_AtomicLongArray_getAndDecrement:
         case TR::java_util_concurrent_atomic_AtomicLongArray_addAndGet:
         case TR::java_util_concurrent_atomic_AtomicLongArray_getAndAdd:
            className = "Ljava/util/concurrent/atomic/AtomicLongArray;";
            classNameLen = 45;
            fieldSig = "[J";
            fieldSigLen = 2;
            break;

         case TR::java_util_concurrent_atomic_AtomicReferenceArray_getAndSet:
            className = "Ljava/util/concurrent/atomic/AtomicReferenceArray;";
            classNameLen = 50;
            fieldSig = "Ljava/lang/Object;";
            fieldSigLen = 18;
            break;
            }

         TR_ResolvedMethod *owningMethod = node->getSymbolReference()->getOwningMethod(comp);
         TR_OpaqueClassBlock *containingClass = fej9->getClassFromSignature(className, classNameLen, owningMethod, true);
         int32_t arrayFieldOffset = fej9->getInstanceFieldOffset(containingClass, "array", 5, fieldSig, fieldSigLen) + fej9->getObjectHeaderSizeInBytes(); // size of a J9 object header

         TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(valueReg, arrayFieldOffset, isLong ? 8 : 4, cg);

         numDeps++;
         scratchRegister = cg->allocateCollectedReferenceRegister();
         TR::Register *memRefRegister = scratchRegister;

#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
         // read only 32 bits
         generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, memRefRegister,
               new (cg->trHeapMemory()) TR::MemoryReference(valueReg, arrayFieldOffset, 4, cg));
#else
         generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, memRefRegister, new (cg->trHeapMemory()) TR::MemoryReference(valueReg, arrayFieldOffset, TR::Compiler->om.sizeofReferenceAddress(), cg));
#endif

         valueReg = memRefRegister;

         generateShiftLeftImmediate(cg, node, fieldOffsetReg, indexRegister, shiftAmount);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, fieldOffsetReg, fieldOffsetReg, fieldOffset);
         }
      }

   // Memory barrier --- NOTE: we should be able to do a test upfront to save this barrier,
   //                          but Hursley advised to be conservative due to lack of specification.
   generateInstruction(cg, TR::InstOpCode::lwsync, node);

   TR::LabelSymbol *doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *loopLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   loopLabel->setStartInternalControlFlow();
   if (!isArray)
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, fieldOffsetReg, fieldOffset);

   deltaReg = cg->allocateRegister();
   if (isArgImmediate && isAddOp)
      {
      // If argument is an immediate value and operation is an add,
      // it will be used as an immediate operand in an add immediate instruction
      generateLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel);
      }
   else if (isArgImmediate)
      {
      // If argument is immediate, but the operation is not an add,
      // the value must still be loaded into a register
      generateLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel);
      loadConstant(cg, node, delta, deltaReg);
      }
   else
      {
      // For non-constant arguments, use evaluated register
      // For non-immediate constants, evaluate since they may be re-used
      numDeps++;
      generateLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, deltaReg, tempReg);
      }

   generateTrg1MemInstruction(cg, isLong ? TR::InstOpCode::ldarx : TR::InstOpCode::lwarx, node, resultReg,
         new (cg->trHeapMemory()) TR::MemoryReference(valueReg, fieldOffsetReg, isLong ? 8 : 4, cg));

   if (isAddOp)
      {
      if (isArgImmediateShifted)
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, deltaReg, resultReg, ((delta & 0xFFFF0000) >> 16));
      else if (isArgImmediate)
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, deltaReg, resultReg, delta);
      else
         generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, deltaReg, resultReg, deltaReg);
      }

   generateMemSrc1Instruction(cg, isLong ? TR::InstOpCode::stdcx_r : TR::InstOpCode::stwcx_r, node, new (cg->trHeapMemory()) TR::MemoryReference(valueReg, fieldOffsetReg, isLong ? 8 : 4, cg),
         deltaReg);

   // We expect this store is usually successful, i.e., the following branch will not be taken
   if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, node, loopLabel, cndReg);
   else
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, loopLabel, cndReg);

   // We deviate from the VM helper here: no-store-no-barrier instead of always-barrier
   generateInstruction(cg, TR::InstOpCode::sync, node);

   TR::RegisterDependencyConditions *conditions;

   //Set the conditions and dependencies
   conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions((uint16_t) numDeps, (uint16_t) numDeps, cg->trMemory());
   addDependency(conditions, valueReg, TR::RealRegister::NoReg, TR_GPR, cg);
   conditions->getPreConditions()->getRegisterDependency(0)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0();
   addDependency(conditions, resultReg, TR::RealRegister::NoReg, TR_GPR, cg);
   conditions->getPreConditions()->getRegisterDependency(1)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(1)->setExcludeGPR0();
   addDependency(conditions, deltaReg, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);
   addDependency(conditions, fieldOffsetReg, TR::RealRegister::NoReg, TR_GPR, cg);
   if (tempReg)
      addDependency(conditions, tempReg, TR::RealRegister::NoReg, TR_GPR, cg);
   if (scratchRegister)
      addDependency(conditions, scratchRegister, TR::RealRegister::NoReg, TR_GPR, cg);

   doneLabel->setEndInternalControlFlow();
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

   cg->decReferenceCount(valueChild);
   cg->stopUsingRegister(cndReg);
   cg->stopUsingRegister(fieldOffsetReg);

   if (tempReg)
      cg->stopUsingRegister(tempReg);

   if (scratchRegister)
      cg->stopUsingRegister(scratchRegister);

   if (deltaChild)
      cg->decReferenceCount(deltaChild);

   if (isGetAndOp)
      {
      //for Get And Op, we will store the result in the result register
      cg->stopUsingRegister(deltaReg);
      node->setRegister(resultReg);
      return resultReg;
      }
   else
      {
      //for Op And Get, we will store the return value in the delta register
      //we no longer need the result register
      cg->stopUsingRegister(resultReg);
      node->setRegister(deltaReg);
      return deltaReg;
      }
   }

static TR::Register *inlineSinglePrecisionFP(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Wrong number of children in inlineSinglePrecisionFP");

   TR::Node *firstChild = node->getFirstChild();
   TR::Register *srcRegister = cg->evaluate(firstChild);
   TR::Register *targetRegister = cg->allocateSinglePrecisionRegister();

   generateTrg1Src1Instruction(cg, op, node, targetRegister, srcRegister);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }

static TR::Register *inlineSinglePrecisionFPTrg1Src2(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 2, "Wrong number of children in inlineSinglePrecisionFPTrg1Src2");

   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Register = cg->evaluate(firstChild);
   TR::Register *src2Register = cg->evaluate(secondChild);
   TR::Register *targetRegister = cg->allocateSinglePrecisionRegister();

   if (op == TR::InstOpCode::fcpsgn) // fcpsgn orders operands opposite of Math.copySign
      generateTrg1Src2Instruction(cg, op, node, targetRegister, src2Register, src1Register);
   else
      generateTrg1Src2Instruction(cg, op, node, targetRegister, src1Register, src2Register);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

static TR::Register *inlineDoublePrecisionFP(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Wrong number of children in inlineDoublePrecisionFP");

   TR::Node *firstChild = node->getFirstChild();
   TR::Register *srcRegister = cg->evaluate(firstChild);
   TR::Register *targetRegister = cg->allocateRegister(TR_FPR);

   generateTrg1Src1Instruction(cg, op, node, targetRegister, srcRegister);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }

static TR::Register *inlineDoublePrecisionFPTrg1Src2(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 2, "Wrong number of children in inlineDoublePrecisionFPTrg1Src2");

   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *src1Register = cg->evaluate(firstChild);
   TR::Register *src2Register = cg->evaluate(secondChild);
   TR::Register *targetRegister = cg->allocateRegister(TR_FPR);

   if (op == TR::InstOpCode::fcpsgn) // fcpsgn orders operands opposite of Math.copySign
      generateTrg1Src2Instruction(cg, op, node, targetRegister, src2Register, src1Register);
   else
      generateTrg1Src2Instruction(cg, op, node, targetRegister, src1Register, src2Register);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetRegister;
   }

static TR::Register *inlineDoubleWordCASOrSetSupported(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *resultReg = cg->allocateRegister();

   // Replace this call with TRUE if running 64-bit compressed pointers, FALSE otherwise
   if (TR::Compiler->target.is64Bit() && cg->comp()->useCompressedPointers())
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 1);
   else
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 0);

   cg->decReferenceCount(node->getChild(0));
   node->setRegister(resultReg);
   return resultReg;
   }

static void checkFieldOrderForDCASOrSet(TR::CodeGenerator *cg, bool isAMR, bool *isRefFirst, int32_t *firstFieldOffset)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->comp()->fe());
   TR_OpaqueClassBlock * classBlock = NULL;
   char *fieldName;
   int32_t fieldNameLen;
   char *fieldSig;
   int32_t fieldSigLen;
   int32_t intOrBoolOffset;

   if (isAMR)
      {
      classBlock = fej9->getClassFromSignature("Ljava/util/concurrent/atomic/AtomicMarkableReference$ReferenceBooleanPair;", 74, comp->getCurrentMethod(), true);
      fieldName = "bit";
      fieldNameLen = 3;
      fieldSig = "Z";
      fieldSigLen = 1;

      intOrBoolOffset = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, fieldName, fieldNameLen, fieldSig, fieldSigLen);
      }
   else
      {
      classBlock = fej9->getClassFromSignature("Ljava/util/concurrent/atomic/AtomicStampedReference$ReferenceIntegerPair;", 73, comp->getCurrentMethod(), true);
      fieldName = "integer";
      fieldNameLen = 7;
      fieldSig = "I";
      fieldSigLen = 1;

      intOrBoolOffset = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, fieldName, fieldNameLen, fieldSig, fieldSigLen);
      }

   fieldName = "reference";
   fieldNameLen = 9;
   fieldSig = "Ljava/lang/Object;";
   fieldSigLen = 18;

   int32_t referenceOffset = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, fieldName, fieldNameLen, fieldSig, fieldSigLen);

   *isRefFirst = (referenceOffset == (intOrBoolOffset - 4));
   *firstFieldOffset = (*isRefFirst) ? referenceOffset : intOrBoolOffset;

   }

static TR::Register *inlineAtomicOperation(TR::Node *node, TR::CodeGenerator *cg, TR::MethodSymbol *method)
   {
   TR::Node *firstChild = NULL;
   TR::Node *valueChild = NULL;
   TR::Node *indexChild = NULL;
   TR::Node *deltaChild = NULL;
   TR::Node *newValChild = NULL;
   TR::Node *expValChild = NULL;
   TR::Node *newRefChild = NULL;
   TR::Node *expRefChild = NULL;
   TR::Node *overNewNode = NULL;
   TR::Node *overExpNode = NULL;

   TR::LabelSymbol *startLabel = NULL;
   TR::LabelSymbol *failLabel = NULL;
   TR::LabelSymbol *doneLabel = NULL;

   TR::Register *valueReg = NULL;
   TR::Register *scratchReg = NULL;
   TR::Register *fieldOffsetReg = NULL;
   TR::Register *cndReg = NULL;
   TR::Register *currentReg = NULL;
   TR::Register *newComposedReg = NULL;
   TR::Register *expComposedReg = NULL;
   TR::Register *expRefReg = NULL;
   TR::Register *newRefReg = NULL;
   TR::Register *expValReg = NULL;
   TR::Register *newValReg = NULL;
   TR::Register *resultReg = NULL;
   TR::Register *objReg = NULL;
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   bool isArray = false;
   bool isAddOp = true;
   bool isDeltaImplied = false;
   bool isDelta = false;
   bool isGetAndOp = false;
   bool isCAS = false;
   bool isSetOnly = false;
   bool isAMRASR = false;
   bool isWeak = false;
   bool isHalfCAS = false;
   bool isRefFirst = false;
   bool isUnsafe = false;

   bool isArgImm = false;
   bool isArgImmShifted = false;

   bool fieldOffsetRegIsEval = false;
   bool isRefWrite = false;

   bool newPair = false;
   bool expPair = false;

   bool stopNewComposedCopy = false;

   int64_t expValImm;
   int32_t delta;
   uint8_t size = 4;
   int32_t fieldOffset = 0;
   int32_t idx;
   uint8_t numDeps = 1;

   TR::RecognizedMethod currentMethod = method->getRecognizedMethod();

   switch (currentMethod)
      {
   case TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z:
      size = 8;
      isUnsafe = true;
      isCAS = true;
      isAddOp = false;
      break;
   case TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z:
      size = TR::Compiler->om.sizeofReferenceAddress();
      isRefWrite = true;
   case TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z:
      isUnsafe = true;
      isCAS = true;
      isAddOp = false;
      break;
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
      size = 4;
      break;
   case TR::java_util_concurrent_atomic_AtomicReference_getAndSet:
   case TR::java_util_concurrent_atomic_AtomicReferenceArray_getAndSet:
      size = TR::Compiler->om.sizeofReferenceAddress();
      break;
   default:
      size = 8;
      }

   switch (currentMethod)
      {
   case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordSet:
   case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordSet:
      isSetOnly = true;
   case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordCAS:
   case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordCAS:
      isAddOp = false;
      isAMRASR = true;
      break;
      }

   switch (currentMethod)
      {
   case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordCAS:
   case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordCAS:
      isAddOp = false;
      isCAS = true;
      break;
   case TR::java_util_concurrent_atomic_AtomicBoolean_getAndSet:
   case TR::java_util_concurrent_atomic_AtomicInteger_getAndSet:
   case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndSet:
   case TR::java_util_concurrent_atomic_AtomicLong_getAndSet:
   case TR::java_util_concurrent_atomic_AtomicLongArray_getAndSet:
   case TR::java_util_concurrent_atomic_AtomicReference_getAndSet:
   case TR::java_util_concurrent_atomic_AtomicReferenceArray_getAndSet:
      isAddOp = false;
   case TR::java_util_concurrent_atomic_AtomicInteger_getAndAdd:
   case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndAdd:
   case TR::java_util_concurrent_atomic_AtomicLong_getAndAdd:
   case TR::java_util_concurrent_atomic_AtomicLongArray_getAndAdd:
      isDelta = true;
   case TR::java_util_concurrent_atomic_AtomicInteger_getAndDecrement:
   case TR::java_util_concurrent_atomic_AtomicInteger_getAndIncrement:
   case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndDecrement:
   case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndIncrement:
   case TR::java_util_concurrent_atomic_AtomicLong_getAndDecrement:
   case TR::java_util_concurrent_atomic_AtomicLong_getAndIncrement:
   case TR::java_util_concurrent_atomic_AtomicLongArray_getAndDecrement:
   case TR::java_util_concurrent_atomic_AtomicLongArray_getAndIncrement:
      isGetAndOp = true;
      break;
   case TR::java_util_concurrent_atomic_AtomicInteger_addAndGet:
   case TR::java_util_concurrent_atomic_AtomicIntegerArray_addAndGet:
   case TR::java_util_concurrent_atomic_AtomicLong_addAndGet:
   case TR::java_util_concurrent_atomic_AtomicLongArray_addAndGet:
      isDelta = true;
      break;
      }

   isHalfCAS = isAMRASR && !isSetOnly && !isCAS;
   isDeltaImplied = isAddOp && !isDelta;
   isRefWrite = isRefWrite || (isAMRASR && !isHalfCAS);

   if (isDeltaImplied)
      {
      switch (currentMethod)
         {
      case TR::java_util_concurrent_atomic_AtomicInteger_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicLong_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLong_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicLongArray_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndDecrement:
         delta = (int32_t) - 1;
      default:
         delta = (int32_t) 1;
         }
      }

   if (isAMRASR)
      {
      TR_ASSERT(TR::Compiler->target.is32Bit() || comp->useCompressedPointers(), "JUC AMR/ASR methods can only be used in 64-bit compressed.");
      firstChild = node->getChild(0);
      valueChild = node->getChild(1);
      newRefChild = (isCAS || isSetOnly) ? node->getChild(2) : NULL;
      newValChild = (isCAS) ? node->getChild(4) : node->getChild(3);
      expValChild = (isCAS) ? node->getChild(5) : NULL;
      if (!isSetOnly)
         {
         expRefChild = (isCAS) ? node->getChild(3) : node->getChild(2);
         }
      }
   else if (isUnsafe && isCAS)
      {
      firstChild = node->getChild(0);
      valueChild = node->getChild(1);
      indexChild = node->getChild(2);
      if (isRefWrite)
         {
         expRefChild = node->getChild(3);
         newRefChild = node->getChild(4);
         }
      else
         {
         expValChild = node->getChild(3);
         newValChild = node->getChild(4);
         }
      }
   else
      {
      valueChild = node->getChild(0);
      idx = 1;
      if (isArray)
         {
         indexChild = node->getChild(idx++);
         }
      if (isDelta)
         {
         deltaChild = node->getChild(idx++);
         }
      else if (isCAS)
         {
         expValChild = node->getChild(idx);
         newValChild = node->getChild(idx + 1);
         idx += 2;
         }
      if (idx != node->getNumChildren())
         {
         TR_ASSERT(0, "Wrong number of children for JUC Atomic node\n");
         return NULL;
         }
      }

   valueReg = cg->evaluate(valueChild);
   objReg = valueReg;

   if (isAMRASR)
      {
      switch (currentMethod)
         {
      case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordCAS:
      case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordSet:
         checkFieldOrderForDCASOrSet(cg, false, &isRefFirst, &fieldOffset);
         break;
      default:
         checkFieldOrderForDCASOrSet(cg, true, &isRefFirst, &fieldOffset);
         break;
         }
      }
   else if (isUnsafe)
      {
      if (indexChild->getOpCode().isLoadConst() && indexChild->getRegister() == NULL && TR::Compiler->target.is32Bit())
         {
         fieldOffset = (int32_t) indexChild->getLongInt();
         //traceMsg(comp,"Allocate fieldOffsetReg\n");
         fieldOffsetReg = cg->allocateRegister();
         numDeps++;
         loadConstant(cg, node, fieldOffset, fieldOffsetReg);
         }
      else
         {
         fieldOffsetReg = cg->evaluate(indexChild);
         resultReg = cg->allocateRegister();
         numDeps += 2;
         if (TR::Compiler->target.is32Bit())
            fieldOffsetReg = fieldOffsetReg->getLowOrder();
         }
      }
   else if (!isArray)
      {
      TR_OpaqueClassBlock *classBlock;
      char *className, *fieldSig;
      int32_t classNameLen, fieldSigLen;
      fieldSigLen = 1;

      switch (currentMethod)
         {
      case TR::java_util_concurrent_atomic_AtomicBoolean_getAndSet:
         className = "Ljava/util/concurrent/atomic/AtomicBoolean;";
         classNameLen = 43;
         fieldSig = "I"; // not a type, the field is int
      case TR::java_util_concurrent_atomic_AtomicInteger_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicInteger_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicInteger_incrementAndGet:
         className = "Ljava/util/concurrent/atomic/AtomicInteger;";
         classNameLen = 43;
         fieldSig = "I";
         break;
      case TR::java_util_concurrent_atomic_AtomicLong_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicLong_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLong_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicLong_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicLong_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicLong_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicLong_incrementAndGet:
         className = "Ljava/util/concurrent/atomic/AtomicLong;";
         classNameLen = 40;
         fieldSig = "J";
         break;
      case TR::java_util_concurrent_atomic_AtomicReference_getAndSet:
         className = "Ljava/util/concurrent/atomic/AtomicReference;";
         classNameLen = 45;
         fieldSig = "Ljava/lang/Object;";
         fieldSigLen = 18;
         break;
      default:
         TR_ASSERT(0, "Unknown atomic operation method\n");
         return NULL;
         }
      classBlock = fej9->getClassFromSignature(className, classNameLen, comp->getCurrentMethod(), true);
      fieldOffset = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "value", 5, fieldSig, fieldSigLen);
      }
   else // isArray
      {
      TR::Register *indexReg = cg->evaluate(indexChild);
      int32_t shiftAmount = size == 8 ? 3 : 2;
      fieldOffset = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();

      TR_OpaqueClassBlock *classBlock;
      char *className, *fieldSig;
      int32_t classNameLen, fieldSigLen;
      fieldSigLen = 1;

      switch (currentMethod)
         {
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_incrementAndGet:
         className = "Ljava/util/concurrent/atomic/AtomicIntegerArray;";
         classNameLen = 48;
         fieldSig = "[I";
         fieldSigLen = 2;
         break;
      case TR::java_util_concurrent_atomic_AtomicLongArray_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_decrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicLongArray_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicLongArray_incrementAndGet:
         className = "Ljava/util/concurrent/atomic/AtomicLongArray;";
         classNameLen = 45;
         fieldSig = "[J";
         fieldSigLen = 2;
         break;
      case TR::java_util_concurrent_atomic_AtomicReferenceArray_getAndSet:
         className = "Ljava/util/concurrent/atomic/AtomicReferenceArray;";
         classNameLen = 50;
         fieldSig = "[Ljava/lang/Object;";
         fieldSigLen = 19;
         break;
      default:
         TR_ASSERT(0, "Unknown atomic operation method\n");
         return NULL;
         }
      classBlock = fej9->getClassFromSignature(className, classNameLen, comp->getCurrentMethod(), true);
      int32_t arrayFieldOffset = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "array", 5, fieldSig, fieldSigLen);

      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(valueReg, arrayFieldOffset, size, cg);
      scratchReg = cg->allocateCollectedReferenceRegister();

#ifdef J9VM_INTERP_COMPRESSED_OBJECT_HEADER
      // read only 32 bits
      generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, scratchReg,
            new (cg->trHeapMemory()) TR::MemoryReference(valueReg, arrayFieldOffset, 4, cg));
#else
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, scratchReg, new (cg->trHeapMemory()) TR::MemoryReference(valueReg, arrayFieldOffset, TR::Compiler->om.sizeofReferenceAddress(), cg));
#endif
      valueReg = scratchReg;
      fieldOffsetReg = cg->allocateRegister();
      numDeps += 2;
      generateShiftLeftImmediate(cg, node, fieldOffsetReg, indexReg, shiftAmount);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, fieldOffsetReg, fieldOffsetReg, fieldOffset);
      }

   if (isAMRASR)
      {
      newValReg = cg->evaluate(newValChild);
      numDeps++;
      if (!isHalfCAS)
         {
         newRefReg = cg->evaluate(newRefChild);
         numDeps++;
         overNewNode = !isRefFirst ? newRefChild : newValChild;
         newComposedReg = !isRefFirst ? newRefReg : newValReg;
         }
      if (!isSetOnly)
         {
         expRefReg = cg->evaluate(expRefChild);
         numDeps++;
         if (!isHalfCAS)
            {
            expValReg = cg->evaluate(expValChild);
            numDeps++;
            overExpNode = !isRefFirst ? expRefChild : expValChild;
            expComposedReg = !isRefFirst ? expRefReg : expValReg;
            }
         }
      else
         {
         // AMR/ASR.set requires no internal control flow
         if (TR::Compiler->target.is64Bit())
            {
            /*
             * AMR/ASR.set (64-bit compressed references)
             * mr newComposedReg, newRefReg/newValReg ;depends on order of fields
             *                                        ;may not be necessary if no reuse
             *                                        ;then just use newRefReg or newValReg
             *                                        ;instead of newComposedReg
             * rldimi newComposedReg, newValReg/newRefReg, 32, 0xFFFFFFFF00000000
             * lwsync
             * std newComposedReg, fieldOffset(valueReg)
             * sync
             */
            if (overNewNode->getReferenceCount() > 1 || newComposedReg == valueReg)
               {
               TR::Register *tmpReg = newComposedReg;
               stopNewComposedCopy = true;
               newComposedReg = cg->allocateRegister();
               generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, newComposedReg, tmpReg);
               }
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldimi, node, newComposedReg, (!isRefFirst ? newValReg : newRefReg), 32, CONSTANT64(0xFFFFFFFF00000000));
            generateInstruction(cg, TR::InstOpCode::lwsync, node);
            generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, new (cg->trHeapMemory()) TR::MemoryReference(valueReg, fieldOffset, 8, cg), newComposedReg);
            generateInstruction(cg, TR::InstOpCode::sync, node);
            if (stopNewComposedCopy)
               {
               cg->stopUsingRegister(newComposedReg);
               }
            }
         else
            {
            /*
             * AMR/ASR.set (32-bit)
             * stw newValReg/newRefReg, 0(temp)
             * stw newRefReg/newValReg, 4(temp)
             * lwsync
             * lfd fr1, 0(temp)
             * stfd fr1, fieldOffset(valueReg)
             * sync
             */
            TR::Register *doubleReg = cg->allocateRegister(TR_FPR);
            TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(valueReg, fieldOffset, 8, cg);
            TR_BackingStore *location;
            location = cg->allocateSpill(8, false, NULL);
            TR::MemoryReference *tempMRStore1 = new (cg->trHeapMemory()) TR::MemoryReference(node, location->getSymbolReference(), 4, cg);
            TR::MemoryReference *tempMRStore2 = new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMRStore1, 4, 4, cg);
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempMRStore1, (!isRefFirst ? newValReg : newRefReg));
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempMRStore2, (!isRefFirst ? newRefReg : newValReg));
            generateInstruction(cg, TR::InstOpCode::lwsync, node);
            generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, doubleReg, new (cg->trHeapMemory()) TR::MemoryReference(node, location->getSymbolReference(), 8, cg));
            generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, tempMR, doubleReg);
            generateInstruction(cg, TR::InstOpCode::sync, node);
            cg->stopUsingRegister(doubleReg);
            cg->freeSpill(location, 8, 0);
            }
         cg->decReferenceCount(node->getChild(0));
         cg->decReferenceCount(valueChild);
         cg->decReferenceCount(newValChild);
         cg->decReferenceCount(newRefChild);
         node->setRegister(NULL);
         return NULL;
         }
      }
   else if (isUnsafe)
      {
      //right now not checking for ref, just doing long!
      if (isRefWrite)
         {
         }
      else
         {
         if (expValChild->getOpCode().isLoadConst() && expValChild->getRegister() == NULL)
            {
            if (size == 8)
               expValImm = expValChild->getLongInt();
            else
               expValImm = expValChild->getInt();
            if (expValImm >= LOWER_IMMED && expValImm <= UPPER_IMMED)
               isArgImm = true;
            }

         if (!isArgImm)
            {
            expValReg = cg->evaluate(expValChild);
            numDeps++;
            }

         newValReg = cg->evaluate(newValChild);
         numDeps++;
         }
      }

   if (isCAS || isHalfCAS)
      {
      TR::LabelSymbol *rsvFailLabel;
      cndReg = cg->allocateRegister(TR_CCR);
      currentReg = cg->allocateRegister();
      numDeps += 2;
      startLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      failLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

      //allow spurious failures
      rsvFailLabel = (isWeak || isHalfCAS) ? failLabel : startLabel;

      startLabel->setStartInternalControlFlow();

      if (isCAS && isAMRASR && TR::Compiler->target.is64Bit())
         {
         // compose new and expected
         }
      // Expected value

      if (fieldOffsetReg == NULL)
         {
         numDeps++;
         fieldOffsetReg = cg->allocateRegister();
         loadConstant(cg, node, fieldOffset, fieldOffsetReg);
         }
      if (resultReg == NULL)
         {
         resultReg = fieldOffsetReg;
         }

      if (!isWeak)
         generateInstruction(cg, TR::InstOpCode::lwsync, node);

      generateLabelInstruction(cg, TR::InstOpCode::label, node, startLabel);

      generateTrg1MemInstruction(cg, size == 8 ? TR::InstOpCode::ldarx : TR::InstOpCode::lwarx, node, currentReg,
            new (cg->trHeapMemory()) TR::MemoryReference(valueReg, (fieldOffsetReg->getRegisterPair() ? fieldOffsetReg->getLowOrder() : fieldOffsetReg), size, cg));

      if (size == 8 && TR::Compiler->target.is32Bit())
         {
         if (isAMRASR)
            {
            // compose new and expected
            }
         else
            {
            if (!isArgImm && expValReg->getRegisterPair())
               {
               generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldimi, node, expValReg->getLowOrder(), expValReg->getHighOrder(), 32, CONSTANT64(0xFFFFFFFF00000000));
               expPair = true;
               numDeps++;
               }
            if (newValReg->getRegisterPair())
               {
               if (newValReg != expValReg)
                  {
                  generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldimi, node, newValReg->getLowOrder(), newValReg->getHighOrder(), 32, CONSTANT64(0xFFFFFFFF00000000));
                  }
               numDeps++;
               newPair = true;
               }
            }
         }

      if (!isArgImm && expComposedReg == NULL)
         {
         if (expPair)
            expComposedReg = expValReg->getLowOrder();
         else
            expComposedReg = expValReg != NULL ? expValReg : expRefReg;
         }
      if (newComposedReg == NULL)
         {
         if (newPair)
            newComposedReg = newValReg->getLowOrder();
         else
            newComposedReg = newValReg != NULL ? newValReg : newRefReg;
         }

      if (isArgImm)
         {
         generateTrg1Src1ImmInstruction(cg, size == 8 ? TR::InstOpCode::cmpi8 : TR::InstOpCode::cmpi4, node, cndReg, currentReg, expValImm);
         }
      else
         {
         generateTrg1Src2Instruction(cg, size == 8 ? TR::InstOpCode::cmp8 : TR::InstOpCode::cmp4, node, cndReg, currentReg, expComposedReg);
         }

      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, failLabel, cndReg);
      generateMemSrc1Instruction(cg, size == 8 ? TR::InstOpCode::stdcx_r : TR::InstOpCode::stwcx_r, node,
            new (cg->trHeapMemory()) TR::MemoryReference(valueReg, (fieldOffsetReg->getRegisterPair() ? fieldOffsetReg->getLowOrder() : fieldOffsetReg), size, cg),
            newComposedReg);

      /* Expect store to be successful, so this branch is usually not taken */
      if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, node, rsvFailLabel, cndReg);
      else
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, rsvFailLabel, cndReg);

      if (!isWeak)
         generateInstruction(cg, TR::InstOpCode::sync, node);

      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 1);
      generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, failLabel);

      if (!isWeak && !isHalfCAS && size == 8 && TR::Compiler->target.is32Bit())
         {
         /* store original current value conditionally
          * if it succeeds, then this was a true failure, if it
          * does not, it could have been corruption messing up the compare
          */
         generateMemSrc1Instruction(cg, size == 8 ? TR::InstOpCode::stdcx_r : TR::InstOpCode::stwcx_r, node,
               new (cg->trHeapMemory()) TR::MemoryReference(valueReg, (fieldOffsetReg->getRegisterPair() ? fieldOffsetReg->getLowOrder() : fieldOffsetReg), size, cg),
               currentReg);
         /* Expect store to be successful, so this branch is usually not taken */
         if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
            generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, node, startLabel, cndReg);
         else
            generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, startLabel, cndReg);
         }

      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 0);

      }

   TR::RegisterDependencyConditions *conditions;
   conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(numDeps, numDeps, cg->trMemory());
   addDependency(conditions, valueReg, TR::RealRegister::NoReg, TR_GPR, cg);
   conditions->getPreConditions()->getRegisterDependency(0)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0();
   addDependency(conditions, currentReg, TR::RealRegister::NoReg, TR_GPR, cg);

   numDeps -= 2;
   if (fieldOffsetReg != NULL)
      {
      addDependency(conditions, fieldOffsetReg, TR::RealRegister::NoReg, TR_GPR, cg);
      numDeps--;
      }
   if (resultReg != fieldOffsetReg)
      {
      addDependency(conditions, resultReg, TR::RealRegister::NoReg, TR_GPR, cg);
      numDeps--;
      }
   if (objReg != valueReg)
      {
      addDependency(conditions, objReg, TR::RealRegister::NoReg, TR_GPR, cg);
      numDeps--;
      }
   if (isUnsafe)
      {
      if (isRefWrite)
         {
         addDependency(conditions, newRefReg, TR::RealRegister::NoReg, TR_GPR, cg);
         addDependency(conditions, expRefReg, TR::RealRegister::NoReg, TR_GPR, cg);
         numDeps -= 2;
         }
      else
         {
         if (newPair)
            {
            addDependency(conditions, newValReg->getHighOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
            addDependency(conditions, newValReg->getLowOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
            numDeps--;
            }
         else
            {
            addDependency(conditions, newValReg, TR::RealRegister::NoReg, TR_GPR, cg);
            }
         numDeps--;
         if (!isArgImm)
            {
            if (expPair)
               {
               addDependency(conditions, expValReg->getHighOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
               addDependency(conditions, expValReg->getLowOrder(), TR::RealRegister::NoReg, TR_GPR, cg);
               numDeps--;
               }
            else
               {
               addDependency(conditions, expValReg, TR::RealRegister::NoReg, TR_GPR, cg);
               }
            numDeps--;
            }
         }
      }
   if (isAMRASR)
      {
      addDependency(conditions, newValReg, TR::RealRegister::NoReg, TR_GPR, cg);
      numDeps--;
      if (!isHalfCAS)
         {
         addDependency(conditions, newRefReg, TR::RealRegister::NoReg, TR_GPR, cg);
         numDeps--;
         }
      if (!isSetOnly)
         {
         addDependency(conditions, expRefReg, TR::RealRegister::NoReg, TR_GPR, cg);
         numDeps--;
         }
      if (isCAS)
         {
         addDependency(conditions, expValReg, TR::RealRegister::NoReg, TR_GPR, cg);
         numDeps--;
         }
      }

   addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);
   numDeps--;

   doneLabel->setEndInternalControlFlow();
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

   if (firstChild)
      cg->decReferenceCount(firstChild);
   cg->decReferenceCount(valueChild);
   cg->decReferenceCount(indexChild);
   cg->decReferenceCount(expValChild);
   cg->decReferenceCount(newValChild);
   cg->stopUsingRegister(cndReg);
   cg->stopUsingRegister(currentReg);
   node->setRegister(resultReg);

   return resultReg;
   }

static TR::Register *inlineIntegerRotateRight(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 2, "Wrong number of children in inlineIntegerRotateRight");

   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *srcRegister = cg->evaluate(firstChild);
   TR::Register *targetRegister = cg->allocateRegister();

   if (secondChild->getOpCodeValue() == TR::iconst || secondChild->getOpCodeValue() == TR::iuconst)
      {
      int32_t value = (32 - secondChild->getInt()) & 0x1f;
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, targetRegister, srcRegister, value, 0xffffffff);
      }
   else
      {
      TR::Register *shiftAmountReg = cg->evaluate(secondChild);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::subfic, node, targetRegister, shiftAmountReg, 32);
      generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::rlwnm, node, targetRegister, srcRegister, targetRegister, 0xffffffff);
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return targetRegister;
   }

static TR::Register *inlineLongRotateRight(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 2, "Wrong number of children in inlineLongRotateRight");

   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *srcRegister = cg->evaluate(firstChild);
   TR::Register *targetRegister = cg->allocateRegister();

   if (secondChild->getOpCodeValue() == TR::iconst || secondChild->getOpCodeValue() == TR::iuconst)
      {
      int32_t value = (64 - secondChild->getInt()) & 0x3f;
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicl, node, targetRegister, srcRegister, value, CONSTANT64(0xffffffffffffffff));
      }
   else
      {
      TR::Register *shiftAmountReg = cg->evaluate(secondChild);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::subfic, node, targetRegister, shiftAmountReg, 64);
      generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::rldcl, node, targetRegister, srcRegister, targetRegister, CONSTANT64(0xffffffffffffffff));
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return targetRegister;
   }

static TR::Register *inlineShortReverseBytes(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Wrong number of children in inlineShortReverseBytes");

   TR::Node *firstChild = node->getFirstChild();
   TR::Register *tgtRegister = cg->allocateRegister();

   TR::Node *firstNonConversionOpCodeNode = node->getFirstChild();
   TR::DataType nodeType = firstNonConversionOpCodeNode->getType();

   //Move through descendants until a non conversion opcode is reached,
   //while making sure all nodes have a ref count of 1 and the types are between 2-8 bytes
   while (firstNonConversionOpCodeNode->getOpCode().isConversion() &&
          firstNonConversionOpCodeNode->getReferenceCount() == 1 &&
          (nodeType.isInt16() || nodeType.isInt32() || nodeType.isInt64()))
      {
      firstNonConversionOpCodeNode = firstNonConversionOpCodeNode->getFirstChild();
      nodeType = firstNonConversionOpCodeNode->getType();
      }

   if (!firstNonConversionOpCodeNode->getRegister() &&
       firstNonConversionOpCodeNode->getOpCode().isMemoryReference() &&
       firstNonConversionOpCodeNode->getReferenceCount() == 1 &&
       (nodeType.isInt16() || nodeType.isInt32() || nodeType.isInt64()))
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(firstNonConversionOpCodeNode, 2, cg);
#ifndef __LITTLE_ENDIAN__
      //On Big Endian Machines
      if (nodeType.isInt32())
         tempMR->addToOffset(node,2,cg);
      else if (nodeType.isInt64())
         tempMR->addToOffset(node,6,cg);
#endif
      tempMR->forceIndexedForm(firstNonConversionOpCodeNode, cg);
      generateTrg1MemInstruction(cg, TR::InstOpCode::lhbrx, node, tgtRegister, tempMR);
      tempMR->decNodeReferenceCounts(cg);

      //Decrement Ref count for the intermediate conversion nodes
      firstNonConversionOpCodeNode = node->getFirstChild();
      while (firstNonConversionOpCodeNode->getOpCode().isConversion())
         {
         cg->decReferenceCount(firstNonConversionOpCodeNode);
         firstNonConversionOpCodeNode = firstNonConversionOpCodeNode->getFirstChild();
         }
      }
   else
      {
      TR::Register *srcRegister = cg->evaluate(firstChild);
      TR::Register *tmpRegister = cg->allocateRegister();

      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tgtRegister, srcRegister, 24, 0x00000000ff);
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmpRegister, srcRegister, 8, 0x000000ff00);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister, tgtRegister, tmpRegister);

      cg->stopUsingRegister(tmpRegister);
      cg->decReferenceCount(firstChild);
      }

   generateTrg1Src1Instruction(cg, TR::InstOpCode::extsh, node, tgtRegister, tgtRegister);

   node->setRegister(tgtRegister);
   return tgtRegister;
   }


static TR::Register *inlineIntegerReverseBytes(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Wrong number of children in inlineIntegerReverseBytes");

   //ibyteswap provides the same functionality as reverseBytes, so the node is recreated as an ibyteswap and evaluated as such
   TR::Node::recreate(node, TR::ibyteswap);
   return OMR::Power::TreeEvaluator::ibyteswapEvaluator(node, cg);
   }


static TR::Register *inlineLongReverseBytes(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Wrong number of children in inlineLongReverseBytes");

   if (TR::Compiler->target.is64Bit())
      {
      TR::Node *firstChild = node->getFirstChild();
      TR::Register *tgtRegister = cg->allocateRegister();

      if (TR::Compiler->target.cpu.id() >= TR_PPCp7 &&
          !firstChild->getRegister() &&
          firstChild->getOpCode().isMemoryReference() &&
          firstChild->getReferenceCount() == 1)
         {
         TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(firstChild, 8, cg);
         tempMR->forceIndexedForm(firstChild, cg);
         generateTrg1MemInstruction(cg, TR::InstOpCode::ldbrx, node, tgtRegister, tempMR);
         tempMR->decNodeReferenceCounts(cg);
         }
      else
         {
         TR::Register *srcLRegister = cg->evaluate(firstChild);
         TR::Register *srcHRegister = cg->allocateRegister();
         TR::Register *tgtHRegister = cg->allocateRegister();
         TR::Register *tmp1Register = cg->allocateRegister();
         TR::Register *tmp2Register = cg->allocateRegister();

         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicl, node, srcHRegister, srcLRegister, 32, 0x00ffffffff);

         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tgtRegister, srcHRegister, 8, 0x00000000ff);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tgtHRegister, srcLRegister, 8, 0x00000000ff);

         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp1Register, srcHRegister, 8, 0x0000ff0000);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp2Register, srcLRegister, 8, 0x0000ff0000);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister, tgtRegister, tmp1Register);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtHRegister, tgtHRegister, tmp2Register);

         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp1Register, srcHRegister, 24, 0x000000ff00);
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp2Register, srcLRegister, 24, 0x000000ff00);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister, tgtRegister, tmp1Register);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtHRegister, tgtHRegister, tmp2Register);

         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp1Register, srcHRegister, 24, CONSTANT64(0x00ff000000));
         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp2Register, srcLRegister, 24, CONSTANT64(0x00ff000000));
         generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister, tgtRegister, tmp1Register);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtHRegister, tgtHRegister, tmp2Register);

         generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldimi, node, tgtRegister, tgtHRegister, 32, CONSTANT64(0xffffffff00000000));

         cg->stopUsingRegister(tmp2Register);
         cg->stopUsingRegister(tmp1Register);
         cg->stopUsingRegister(tgtHRegister);
         cg->stopUsingRegister(srcHRegister);
         cg->decReferenceCount(firstChild);
         }

      node->setRegister(tgtRegister);
      return tgtRegister;
      }
   else //32-Bit Target
      {
      TR::Node *firstChild = node->getFirstChild();
      TR::RegisterPair *tgtRegister = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());
      TR::Register *srcRegister = cg->evaluate(firstChild);

      TR::Register *tmp1Register = cg->allocateRegister();
      TR::Register *tmp2Register = cg->allocateRegister();

      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tgtRegister->getLowOrder(), srcRegister->getHighOrder(), 8, 0x00000000ff);
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tgtRegister->getHighOrder(), srcRegister->getLowOrder(), 8, 0x00000000ff);

      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp1Register, srcRegister->getHighOrder(), 8, 0x0000ff0000);
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp2Register, srcRegister->getLowOrder(), 8, 0x0000ff0000);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister->getLowOrder(), tgtRegister->getLowOrder(), tmp1Register);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister->getHighOrder(), tgtRegister->getHighOrder(), tmp2Register);

      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp1Register, srcRegister->getHighOrder(), 24, 0x000000ff00);
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp2Register, srcRegister->getLowOrder(), 24, 0x000000ff00);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister->getLowOrder(), tgtRegister->getLowOrder(), tmp1Register);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister->getHighOrder(), tgtRegister->getHighOrder(), tmp2Register);
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp1Register, srcRegister->getHighOrder(), 24, 0x00ff000000);
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmp2Register, srcRegister->getLowOrder(), 24, 0x00ff000000);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister->getLowOrder(), tgtRegister->getLowOrder(), tmp1Register);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::OR, node, tgtRegister->getHighOrder(), tgtRegister->getHighOrder(), tmp2Register);

      cg->stopUsingRegister(tmp2Register);
      cg->stopUsingRegister(tmp1Register);
      cg->decReferenceCount(firstChild);

      node->setRegister(tgtRegister);
      return tgtRegister;
      }
   }

static TR::Register *compressStringEvaluator(TR::Node *node, TR::CodeGenerator *cg, bool japaneseMethod)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->comp()->fe());
   TR::Node *srcObjNode, *dstObjNode, *startNode, *lengthNode;
   TR::Register *srcObjReg = NULL, *dstObjReg = NULL, *lengthReg = NULL, *startReg = NULL;

   srcObjNode = node->getChild(0);
   dstObjNode = node->getChild(1);
   startNode = node->getChild(2);
   lengthNode = node->getChild(3);

   bool stopUsingCopyReg1, stopUsingCopyReg2, stopUsingCopyReg3, stopUsingCopyReg4;

   stopUsingCopyReg1 = TR::TreeEvaluator::stopUsingCopyReg(srcObjNode, srcObjReg, cg);
   stopUsingCopyReg2 = TR::TreeEvaluator::stopUsingCopyReg(dstObjNode, dstObjReg, cg);
   stopUsingCopyReg3 = TR::TreeEvaluator::stopUsingCopyReg(startNode, startReg, cg);
   stopUsingCopyReg4 = TR::TreeEvaluator::stopUsingCopyReg(lengthNode, lengthReg, cg);

   uintptrj_t hdrSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, srcObjReg, srcObjReg, hdrSize);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, dstObjReg, dstObjReg, hdrSize);

   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(12, 12, cg->trMemory());
   TR::Register *cndRegister = cg->allocateRegister(TR_CCR);
   TR::Register *resultReg = cg->allocateRegister(TR_GPR);
   addDependency(conditions, cndRegister, TR::RealRegister::cr0, TR_CCR, cg);
   addDependency(conditions, lengthReg, TR::RealRegister::gr8, TR_GPR, cg);
   addDependency(conditions, startReg, TR::RealRegister::gr7, TR_GPR, cg);
   addDependency(conditions, srcObjReg, TR::RealRegister::gr9, TR_GPR, cg);
   addDependency(conditions, dstObjReg, TR::RealRegister::gr10, TR_GPR, cg);

   addDependency(conditions, NULL, TR::RealRegister::gr0, TR_GPR, cg);
   addDependency(conditions, NULL, TR::RealRegister::gr11, TR_GPR, cg);
   addDependency(conditions, NULL, TR::RealRegister::gr6, TR_GPR, cg);
   addDependency(conditions, NULL, TR::RealRegister::gr4, TR_GPR, cg);
   addDependency(conditions, NULL, TR::RealRegister::gr5, TR_GPR, cg);
   addDependency(conditions, NULL, TR::RealRegister::gr12, TR_GPR, cg);
   addDependency(conditions, resultReg, TR::RealRegister::gr3, TR_GPR, cg);

   if (japaneseMethod)
      TR::TreeEvaluator::generateHelperBranchAndLinkInstruction(TR_PPCcompressStringJ, node, conditions, cg);
   else
      TR::TreeEvaluator::generateHelperBranchAndLinkInstruction(TR_PPCcompressString, node, conditions, cg);

   TR::Register* regs[5] =
      {
      lengthReg, startReg, srcObjReg, dstObjReg, resultReg
      };
   conditions->stopUsingDepRegs(cg, 5, regs);
   for (uint16_t i = 0; i < node->getNumChildren(); i++)
      cg->decReferenceCount(node->getChild(i));
   if (stopUsingCopyReg1)
      cg->stopUsingRegister(srcObjReg);
   if (stopUsingCopyReg2)
      cg->stopUsingRegister(dstObjReg);
   if (stopUsingCopyReg3)
      cg->stopUsingRegister(startReg);
   if (stopUsingCopyReg4)
      cg->stopUsingRegister(lengthReg);

   node->setRegister(resultReg);

   cg->machine()->setLinkRegisterKilled(true);
   cg->setHasCall();
   return (resultReg);
   }

static TR::Register *compressStringNoCheckEvaluator(TR::Node *node, TR::CodeGenerator *cg, bool japaneseMethod)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->comp()->fe());
   TR::Node *srcObjNode, *dstObjNode, *startNode, *lengthNode;
   TR::Register *srcObjReg = NULL, *dstObjReg = NULL, *lengthReg = NULL, *startReg = NULL;

   srcObjNode = node->getChild(0);
   dstObjNode = node->getChild(1);
   startNode = node->getChild(2);
   lengthNode = node->getChild(3);

   bool stopUsingCopyReg1, stopUsingCopyReg2, stopUsingCopyReg3, stopUsingCopyReg4;

   stopUsingCopyReg1 = TR::TreeEvaluator::stopUsingCopyReg(srcObjNode, srcObjReg, cg);
   stopUsingCopyReg2 = TR::TreeEvaluator::stopUsingCopyReg(dstObjNode, dstObjReg, cg);
   stopUsingCopyReg3 = TR::TreeEvaluator::stopUsingCopyReg(startNode, startReg, cg);
   stopUsingCopyReg4 = TR::TreeEvaluator::stopUsingCopyReg(lengthNode, lengthReg, cg);

   uintptrj_t hdrSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, srcObjReg, srcObjReg, hdrSize);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, dstObjReg, dstObjReg, hdrSize);

   int numOfRegs = japaneseMethod ? 11 : 12;
   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(numOfRegs, numOfRegs, cg->trMemory());
   TR::Register *cndRegister = cg->allocateRegister(TR_CCR);
   addDependency(conditions, cndRegister, TR::RealRegister::cr0, TR_CCR, cg);
   addDependency(conditions, lengthReg, TR::RealRegister::gr8, TR_GPR, cg);
   addDependency(conditions, startReg, TR::RealRegister::gr7, TR_GPR, cg);
   addDependency(conditions, srcObjReg, TR::RealRegister::gr9, TR_GPR, cg);
   addDependency(conditions, dstObjReg, TR::RealRegister::gr10, TR_GPR, cg);

   addDependency(conditions, NULL, TR::RealRegister::gr0, TR_GPR, cg);
   addDependency(conditions, NULL, TR::RealRegister::gr11, TR_GPR, cg);
   addDependency(conditions, NULL, TR::RealRegister::gr6, TR_GPR, cg);
   addDependency(conditions, NULL, TR::RealRegister::gr4, TR_GPR, cg);
   addDependency(conditions, NULL, TR::RealRegister::gr5, TR_GPR, cg);
   addDependency(conditions, NULL, TR::RealRegister::gr3, TR_GPR, cg);
   if (!japaneseMethod)
      addDependency(conditions, NULL, TR::RealRegister::gr12, TR_GPR, cg);

   if (japaneseMethod)
      TR::TreeEvaluator::generateHelperBranchAndLinkInstruction(TR_PPCcompressStringNoCheckJ, node, conditions, cg);
   else
      TR::TreeEvaluator::generateHelperBranchAndLinkInstruction(TR_PPCcompressStringNoCheck, node, conditions, cg);

   TR::Register* regs[4] =
      {
      lengthReg, startReg, srcObjReg, dstObjReg
      };
   conditions->stopUsingDepRegs(cg, 4, regs);
   for (uint16_t i = 0; i < node->getNumChildren(); i++)
      cg->decReferenceCount(node->getChild(i));
   if (stopUsingCopyReg1)
      cg->stopUsingRegister(srcObjReg);
   if (stopUsingCopyReg2)
      cg->stopUsingRegister(dstObjReg);
   if (stopUsingCopyReg3)
      cg->stopUsingRegister(startReg);
   if (stopUsingCopyReg4)
      cg->stopUsingRegister(lengthReg);

   cg->machine()->setLinkRegisterKilled(true);
   cg->setHasCall();
   return NULL;
   }

static TR::Register *andORStringEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->comp()->fe());
   TR::Node *srcObjNode, *startNode, *lengthNode;
   TR::Register *srcObjReg = NULL, *lengthReg = NULL, *startReg = NULL;

   srcObjNode = node->getChild(0);
   startNode = node->getChild(1);
   lengthNode = node->getChild(2);

   bool stopUsingCopyReg1, stopUsingCopyReg2, stopUsingCopyReg3;

   stopUsingCopyReg1 = TR::TreeEvaluator::stopUsingCopyReg(srcObjNode, srcObjReg, cg);
   stopUsingCopyReg2 = TR::TreeEvaluator::stopUsingCopyReg(startNode, startReg, cg);
   stopUsingCopyReg3 = TR::TreeEvaluator::stopUsingCopyReg(lengthNode, lengthReg, cg);

   uintptrj_t hdrSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, srcObjReg, srcObjReg, hdrSize);

   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(8, 8, cg->trMemory());
   TR::Register *cndRegister = cg->allocateRegister(TR_CCR);
   TR::Register *resultReg = cg->allocateRegister(TR_GPR);
   addDependency(conditions, cndRegister, TR::RealRegister::cr0, TR_CCR, cg);
   addDependency(conditions, lengthReg, TR::RealRegister::gr8, TR_GPR, cg);
   addDependency(conditions, startReg, TR::RealRegister::gr7, TR_GPR, cg);
   addDependency(conditions, srcObjReg, TR::RealRegister::gr9, TR_GPR, cg);

   addDependency(conditions, NULL, TR::RealRegister::gr0, TR_GPR, cg);
   addDependency(conditions, NULL, TR::RealRegister::gr4, TR_GPR, cg);
   addDependency(conditions, NULL, TR::RealRegister::gr5, TR_GPR, cg);
   addDependency(conditions, resultReg, TR::RealRegister::gr3, TR_GPR, cg);

   TR::TreeEvaluator::generateHelperBranchAndLinkInstruction(TR_PPCandORString, node, conditions, cg);

   TR::Register* regs[4] =
      {
      lengthReg, startReg, srcObjReg, resultReg
      };
   conditions->stopUsingDepRegs(cg, 4, regs);

   for (uint16_t i = 0; i < node->getNumChildren(); i++)
      cg->decReferenceCount(node->getChild(i));
   if (stopUsingCopyReg1)
      cg->stopUsingRegister(srcObjReg);
   if (stopUsingCopyReg2)
      cg->stopUsingRegister(startReg);
   if (stopUsingCopyReg3)
      cg->stopUsingRegister(lengthReg);

   node->setRegister(resultReg);

   cg->machine()->setLinkRegisterKilled(true);
   cg->setHasCall();
   return (resultReg);
   }

/*
 * 1) JCL need to finalize the pair field (is it pair (Java 8) OR atomicRef (Java 7)).
 * 2) Should load the volatile pair field inside TX so will abort if another non-TM sucessfully did a atomicRef.compareAndSet
 *         ReferenceBooleanPair<V> current = atomicRef.get();
 *         if (doubleWordCASSupported(current))
 *            return doubleWordCAS(current, newReference, expectedReference, newMark, expectedMark);
 *
 *         if (tmEnabled())
 *            {
 *            int ret = tmDoubleWordCAS(expectedReference,newReference,expectedMark,newMark);
 *            if (0 != ret)
 *               return (2==ret)? true : false;
 *            }
 *         return (expectedReference == current.reference && expectedMark == current.bit) &&
 *             ((newReference == current.reference && newMark == current.bit) ||
 *             atomicRef.compareAndSet(current, new ReferenceBooleanPair<V>(newReference, newMark)));
 */

static TR::Register *inlineTMDoubleWordCASOrSet(TR::Node *node, TR::CodeGenerator *cg, TR::MethodSymbol *method)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (comp->fe());
   TR_ASSERT(TR::Compiler->target.is64Bit() && !comp->useCompressedPointers(), "inlineTMDoubleWordCASOrSet() can only be used in 64-bit non-compressed.");
   TR::RecognizedMethod currentMethod = method->getRecognizedMethod();

   int32_t addressFieldSize = TR::Compiler->om.sizeofReferenceField();
   TR::InstOpCode::Mnemonic loadOpCode = (addressFieldSize == 8) ? TR::InstOpCode::ld : TR::InstOpCode::lwz;
   TR::InstOpCode::Mnemonic storeOpCode = (addressFieldSize == 8) ? TR::InstOpCode::std : TR::InstOpCode::stw;

   bool isAMR = ((currentMethod == TR::java_util_concurrent_atomic_AtomicMarkableReference_tmDoubleWordCAS)
         || (currentMethod == TR::java_util_concurrent_atomic_AtomicMarkableReference_tmDoubleWordSet));
   bool isCAS = ((currentMethod == TR::java_util_concurrent_atomic_AtomicStampedReference_tmDoubleWordCAS)
         || (currentMethod == TR::java_util_concurrent_atomic_AtomicMarkableReference_tmDoubleWordCAS));

   TR::Register *resultReg = cg->allocateRegister();
   TR::Register *pairReg = cg->allocateRegister();
   TR::Register *rTmp = cg->allocateRegister();
   TR::Register *retryCountReg = cg->allocateRegister();
   TR::Register *cndReg = cg->allocateRegister(TR_CCR);
   TR::Register *rTmp2 = cg->allocateRegister();
   TR::Register *temp4Reg = cg->allocateRegister();

   TR_OpaqueClassBlock * classBlock =
         isAMR ? (fej9->getClassFromSignature("Ljava/util/concurrent/atomic/AtomicMarkableReference$Pair;", 58, comp->getCurrentMethod())) :
               (fej9->getClassFromSignature("Ljava/util/concurrent/atomic/AtomicStampedReference$Pair;", 57, comp->getCurrentMethod()));

   int32_t offsetReference = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "reference", 9, "Ljava/lang/Object;", 18);
   int32_t offsetBoolOrInt =
         isAMR ? fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "mark", 4, "Z", 1) :
               fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "stamp", 5, "I", 1);

   classBlock =
         isAMR ? (fej9->getClassFromSignature("Ljava/util/concurrent/atomic/AtomicMarkableReference;", 53, comp->getCurrentMethod())) :
               (fej9->getClassFromSignature("Ljava/util/concurrent/atomic/AtomicStampedReference;", 52, comp->getCurrentMethod()));

   int32_t offsetPair =
         isAMR ? fej9->getObjectHeaderSizeInBytes()
               + fej9->getInstanceFieldOffset(classBlock, "pair", 4, "Ljava/util/concurrent/atomic/AtomicMarkableReference$Pair;", 58) :
               fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "pair", 4, "Ljava/util/concurrent/atomic/AtomicStampedReference$Pair;", 57);

   TR::Node *newReferenceNode, *expectedReferenceNode;
   TR::Node *newMarkOrStampNode, *expectedMarkOrStampNode;
   TR::Register *newReferenceReg, *expectedReferenceReg;
   TR::Register *newMarkOrStampReg, *expectedMarkOrStampReg;
   TR::Node *receiverNode = node->getFirstChild();
   TR::Register *receiverReg = cg->evaluate(receiverNode);

   if (isCAS)
      {
      expectedReferenceNode = node->getChild(1);
      expectedReferenceReg = cg->evaluate(expectedReferenceNode);
      newReferenceNode = node->getChild(2);
      newReferenceReg = cg->evaluate(newReferenceNode);
      expectedMarkOrStampNode = node->getChild(3);
      expectedMarkOrStampReg = cg->evaluate(expectedMarkOrStampNode);
      newMarkOrStampNode = node->getChild(4);
      newMarkOrStampReg = cg->evaluate(newMarkOrStampNode);

      TR::LabelSymbol * loopLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol * failureLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol * returnLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol * doSetLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol * returnFalseLabal = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

      TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(12, 12, cg->trMemory());

      addDependency(conditions, resultReg, TR::RealRegister::NoReg, TR_GPR, cg);
      addDependency(conditions, pairReg, TR::RealRegister::NoReg, TR_GPR, cg); // dstReg for wrtbar
      addDependency(conditions, rTmp, TR::RealRegister::NoReg, TR_GPR, cg); // temp1Reg for wrtbar
      addDependency(conditions, rTmp2, TR::RealRegister::NoReg, TR_GPR, cg); // temp2Reg for wrtbar
      addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);
      addDependency(conditions, retryCountReg, TR::RealRegister::NoReg, TR_GPR, cg); // temp3Reg for wrtbar
      addDependency(conditions, temp4Reg, TR::RealRegister::NoReg, TR_GPR, cg); // temp4Reg for wrtbar
      addDependency(conditions, receiverReg, TR::RealRegister::NoReg, TR_GPR, cg);
      addDependency(conditions, expectedReferenceReg, TR::RealRegister::NoReg, TR_GPR, cg);
      addDependency(conditions, newReferenceReg, TR::RealRegister::NoReg, TR_GPR, cg); // srcReg for wrtbar
      addDependency(conditions, expectedMarkOrStampReg, TR::RealRegister::NoReg, TR_GPR, cg);
      addDependency(conditions, newMarkOrStampReg, TR::RealRegister::NoReg, TR_GPR, cg);

      static char * debugTM = feGetEnv("TR_DebugTM");
      static char * disableTMDCAS = feGetEnv("TR_DisableAtomicDCAS");
      if (disableTMDCAS)
         {
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 0);
         generateLabelInstruction(cg, TR::InstOpCode::b, node, returnLabel);
         }

      if (debugTM)
         {
         printf("\nTM: use TM DCAS in %s (%s)", comp->signature(), comp->getHotnessName(comp->getMethodHotness()));
         fflush (stdout);
         }

      static const char *s = feGetEnv("TR_TMDCASRetry");
      static uint16_t TMDCASRetry = s ? atoi(s) : 5;
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, retryCountReg, TMDCASRetry);

      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel, conditions);

      generateInstruction(cg, TR::InstOpCode::tbegin_r, node);

      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, PPCOpProp_BranchUnlikely, node, failureLabel, cndReg);

      generateTrg1MemInstruction(cg, loadOpCode, node, pairReg, new (cg->trHeapMemory()) TR::MemoryReference(receiverReg, offsetPair, addressFieldSize, cg));

      generateMemSrc1Instruction(cg, storeOpCode, node, new (cg->trHeapMemory()) TR::MemoryReference(pairReg, offsetReference, addressFieldSize, cg), newReferenceReg);
      generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(pairReg, offsetBoolOrInt, 4, cg), newMarkOrStampReg);
      // tend
      generateInstruction(cg, TR::InstOpCode::tend_r, node);

      // resultReg = 2; // Tx success, will return true
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 2);

      TR_WriteBarrierKind gcMode = comp->getOptions()->getGcMode();
      bool doWrtBar = (gcMode == TR_WrtbarRealTime || gcMode == TR_WrtbarOldCheck || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarAlways
            || TR::Options::getCmdLineOptions()->realTimeGC());
      bool doCrdMrk = ((gcMode == TR_WrtbarCardMark || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarCardMarkIncremental) && (!node->getOpCode().isWrtBar() || !node->isNonHeapObjectWrtBar()));
      if (doWrtBar)
         {
         if (doCrdMrk)
            {
            conditions->getPostConditions()->getRegisterDependency(3)->setExcludeGPR0(); //3=temp2Reg
            }
         VMnonNullSrcWrtBarCardCheckEvaluator(node, newReferenceReg, pairReg, cndReg, rTmp, rTmp2, retryCountReg, temp4Reg, returnLabel, conditions, NULL, false, false, cg, false);
         }
      else if (doCrdMrk)
         {
         conditions->getPostConditions()->getRegisterDependency(1)->setExcludeGPR0(); //1=dstReg
         conditions->getPostConditions()->getRegisterDependency(2)->setExcludeGPR0(); //2=temp1Reg

         VMCardCheckEvaluator(node, pairReg, cndReg, rTmp, rTmp2, retryCountReg, conditions, cg);
         }

      generateLabelInstruction(cg, TR::InstOpCode::b, node, returnLabel);

      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, failureLabel, conditions);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic_r, node, retryCountReg, retryCountReg, -1);
      // ALG: if retryCount != 0, goto loopLabel
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, loopLabel, cndReg);
      // ALG: exceeds retry count, res := 0 // Tx failure, will try slow path
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 0);

      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, returnLabel, conditions);

      cg->decReferenceCount(node->getFirstChild());
      cg->decReferenceCount(node->getSecondChild());
      cg->decReferenceCount(node->getChild(2));
      cg->decReferenceCount(node->getChild(3));
      cg->decReferenceCount(node->getChild(4));
      }
   else
      {
      newReferenceNode = node->getChild(1);
      newReferenceReg = cg->evaluate(newReferenceNode);
      newMarkOrStampNode = node->getChild(2);
      newMarkOrStampReg = cg->evaluate(newMarkOrStampNode);

      TR::LabelSymbol * loopLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol * failureLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol * returnLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol * doSetLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

      TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(10, 10, cg->trMemory());

      addDependency(conditions, resultReg, TR::RealRegister::NoReg, TR_GPR, cg);
      addDependency(conditions, pairReg, TR::RealRegister::NoReg, TR_GPR, cg); // dstReg for wrtbar
      addDependency(conditions, rTmp, TR::RealRegister::NoReg, TR_GPR, cg); // temp1Reg for wrtbar
      addDependency(conditions, rTmp2, TR::RealRegister::NoReg, TR_GPR, cg); // temp2Reg for wrtbar
      addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);
      addDependency(conditions, retryCountReg, TR::RealRegister::NoReg, TR_GPR, cg); // temp3Reg for wrtbar
      addDependency(conditions, temp4Reg, TR::RealRegister::NoReg, TR_GPR, cg); // temp4Reg for wrtbar
      addDependency(conditions, receiverReg, TR::RealRegister::NoReg, TR_GPR, cg);
      addDependency(conditions, newReferenceReg, TR::RealRegister::NoReg, TR_GPR, cg); // srcReg for wrtbar
      addDependency(conditions, newMarkOrStampReg, TR::RealRegister::NoReg, TR_GPR, cg);

      static char * debugTM = feGetEnv("TR_DebugTM");
      static char * disableTMDSET = feGetEnv("TR_DisableAtomicDSET");
      if (disableTMDSET)
         {
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 0);
         generateLabelInstruction(cg, TR::InstOpCode::b, node, returnLabel);
         }

      if (debugTM)
         {
         printf("\nTM: use TM DSET in %s (%s)", comp->signature(), comp->getHotnessName(comp->getMethodHotness()));
         fflush (stdout);
         }

      static const char *s = feGetEnv("TR_TMDSetRetry");
      static uint16_t TMDSetRetry = s ? atoi(s) : 5;
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, retryCountReg, TMDSetRetry);

      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel, conditions);

      generateInstruction(cg, TR::InstOpCode::tbegin_r, node);

      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, PPCOpProp_BranchUnlikely, node, failureLabel, cndReg);

      generateTrg1MemInstruction(cg, loadOpCode, node, pairReg, new (cg->trHeapMemory()) TR::MemoryReference(receiverReg, offsetPair, addressFieldSize, cg));
      // pairReg.reference = newReferenceReg
      generateMemSrc1Instruction(cg, storeOpCode, node, new (cg->trHeapMemory()) TR::MemoryReference(pairReg, offsetReference, addressFieldSize, cg), newReferenceReg);
      // pairReg.mark(stamp) = newMarkOrStampReg;
      generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(pairReg, offsetBoolOrInt, 4, cg), newMarkOrStampReg);
      // tend
      generateInstruction(cg, TR::InstOpCode::tend_r, node);
      // resultReg = 1;
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 1);

      // wrtbar for pairReg.reference = newReferenceReg
      TR_WriteBarrierKind gcMode = comp->getOptions()->getGcMode();
      bool doWrtBar = (gcMode == TR_WrtbarRealTime || gcMode == TR_WrtbarOldCheck || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarAlways
            || TR::Options::getCmdLineOptions()->realTimeGC());
      bool doCrdMrk = ((gcMode == TR_WrtbarCardMark || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarCardMarkIncremental) && (!node->getOpCode().isWrtBar() || !node->isNonHeapObjectWrtBar()));
      if (doWrtBar)
         {
         if (doCrdMrk)
            {
            conditions->getPostConditions()->getRegisterDependency(3)->setExcludeGPR0(); //3=temp2Reg
            }
         VMnonNullSrcWrtBarCardCheckEvaluator(node, newReferenceReg, pairReg, cndReg, rTmp, rTmp2, retryCountReg, temp4Reg, returnLabel, conditions, NULL, false, false, cg, false);
         }
      else if (doCrdMrk)
         {
         conditions->getPostConditions()->getRegisterDependency(1)->setExcludeGPR0(); //1=dstReg
         conditions->getPostConditions()->getRegisterDependency(2)->setExcludeGPR0(); //2=temp1Reg

         VMCardCheckEvaluator(node, pairReg, cndReg, rTmp, rTmp2, retryCountReg, conditions, cg);
         }

      // goto returnLabel
      generateLabelInstruction(cg, TR::InstOpCode::b, node, returnLabel);

      // *** failureLabel
      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, failureLabel, conditions);
      // retryCount-=1
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic_r, node, retryCountReg, retryCountReg, -1);
      // ALG: if retryCount != 0, goto loopLabel
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, loopLabel, cndReg);
      // ALG: exceeds retry count, res := 0 failure
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 0);

      // *** returnLabel
      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, returnLabel, conditions);

      cg->decReferenceCount(node->getFirstChild());
      cg->decReferenceCount(node->getSecondChild());
      cg->decReferenceCount(node->getChild(2));
      }

   cg->stopUsingRegister(pairReg);
   cg->stopUsingRegister(rTmp);
   cg->stopUsingRegister(rTmp2);
   cg->stopUsingRegister(retryCountReg);
   cg->stopUsingRegister(temp4Reg);
   cg->stopUsingRegister(cndReg);

   node->setRegister(resultReg);
   return resultReg;
   }

static TR::Register *inlineDoubleWordCAS(TR::Node *node, TR::CodeGenerator *cg, TR::MethodSymbol *method)
   {
   TR::Compilation * comp = cg->comp();
   TR_ASSERT(TR::Compiler->target.is64Bit() && comp->useCompressedPointers(), "inlineDoubleWordCAS() can only be used in 64-bit compressed.");

   TR::RecognizedMethod currentMethod = method->getRecognizedMethod();
   TR::Register *resultReg = cg->allocateRegister();
   TR::Register *tmpReg;
   TR::Register *resCndReg;
   TR::Node *currentPairNode = node->getChild(1);
   TR::Register *currentPairReg = cg->evaluate(currentPairNode);
   TR::Node *newReferenceNode, *expectedReferenceNode;
   TR::Node *newMarkOrStampNode, *expectedMarkOrStampNode;

   TR::Node *overNewNode, *overExpectedNode;

   TR::Register *fieldOffsetReg;

   TR::Register *newReferenceReg, *expectedReferenceReg;
   TR::Register *newMarkOrStampReg, *expectedMarkOrStampReg;

   // Just use as pointers to referenceReg or MarkOrStampReg for varying order of reference and int/bool in class
   TR::Register *expectedComposedReg, *newComposedReg;

   // If the lower-order member registers for new and expected values are the same, need to copy one to avoid overwrite
   TR::Register *tmpCopyReg;

   bool isBoolNotInt = false;
   bool isSetOnly = false;
   bool isConstantNewMarkOrStamp = false;
   bool isConstantExpectedMarkOrStamp = false;
   bool isRefFirst;
   bool needNewRegCopy = false;
   bool needExpectedRegCopy = false;
   int32_t offsetOfFirstField;

   switch (currentMethod)
      {
   case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordSet:
      isSetOnly = true;
   case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordCAS:
      isBoolNotInt = true;
      break;
   case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordSet:
      isSetOnly = true;
   case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordCAS:
      isBoolNotInt = false;
      break;
      }

   // check for ordering of reference and int/bool to do register composing in correct order
   checkFieldOrderForDCASOrSet(cg, isBoolNotInt, &isRefFirst, &offsetOfFirstField);

   // in doubleWordSet, the new reference pair is in nodes 2 and 3
   // for doubleWordCompareAndSet, the new reference pair is in nodes 2 and 4
   newReferenceNode = node->getChild(2);
   newMarkOrStampNode = (isSetOnly) ? node->getChild(3) : node->getChild(4);

   newReferenceReg = cg->evaluate(newReferenceNode);

   /* Check if new Mark or Stamp is a constant */
   if (newMarkOrStampNode->getOpCode().isLoadConst() && !newMarkOrStampNode->getRegister())
      {
      int32_t newMarkOrStampVal = (int32_t)(newMarkOrStampNode->getInt());
      isConstantNewMarkOrStamp = true;
      newMarkOrStampReg = cg->allocateRegister();
      loadConstant(cg, node, newMarkOrStampVal, newMarkOrStampReg);
      }
   else
      newMarkOrStampReg = cg->evaluate(newMarkOrStampNode);

   /* Coalesce the reference and mark/stamp into a single 64-bit register */
   newComposedReg = !isRefFirst ? newReferenceReg : newMarkOrStampReg;
   overNewNode = !isRefFirst ? newReferenceNode : newMarkOrStampNode;

   if (isSetOnly)
      {
      /* set method can write directly to the location without reservation */
      if (overNewNode->getReferenceCount() > 1)
         {
         needNewRegCopy = true;
         tmpCopyReg = newComposedReg;
         newComposedReg = cg->allocateRegister();
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, newComposedReg, tmpCopyReg);
         }

      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldimi, node, newComposedReg, (!isRefFirst ? newMarkOrStampReg : newReferenceReg), 32, CONSTANT64(0xFFFFFFFF00000000));
      generateInstruction(cg, TR::InstOpCode::lwsync, node);
      generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, new (cg->trHeapMemory()) TR::MemoryReference(currentPairReg, offsetOfFirstField, 8, cg), newComposedReg);
      generateInstruction(cg, TR::InstOpCode::sync, node);
      }
   else
      {
      fieldOffsetReg = cg->allocateRegister();
      tmpReg = cg->allocateRegister();
      resCndReg = cg->allocateRegister(TR_CCR);
      TR::LabelSymbol *loopLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol *doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

      loopLabel->setStartInternalControlFlow();

      // expected reference pair is in nodes 3 and 5
      expectedReferenceNode = node->getChild(3);
      expectedMarkOrStampNode = node->getChild(5);

      expectedReferenceReg = cg->evaluate(expectedReferenceNode);

      /* check if Mark or Stamp is a constant */
      if (expectedMarkOrStampNode->getOpCode().isLoadConst() && !expectedMarkOrStampNode->getRegister())
         {
         int32_t expectedMarkOrStampVal = (int32_t)(expectedMarkOrStampNode->getInt());
         isConstantExpectedMarkOrStamp = true;
         expectedMarkOrStampReg = cg->allocateRegister();
         loadConstant(cg, node, expectedMarkOrStampVal, expectedMarkOrStampReg);
         }
      else
         expectedMarkOrStampReg = cg->evaluate(expectedMarkOrStampNode);

      /* Coalesce the reference and mark/stamp into a single 64-bit register */
      expectedComposedReg = !isRefFirst ? expectedReferenceReg : expectedMarkOrStampReg;
      overExpectedNode = !isRefFirst ? expectedReferenceNode : expectedMarkOrStampNode;

      if (expectedComposedReg == newComposedReg || overNewNode->getReferenceCount() > 1)
         {
         needNewRegCopy = true;
         tmpCopyReg = newComposedReg;
         newComposedReg = cg->allocateRegister();
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, newComposedReg, tmpCopyReg);
         }

      if (overExpectedNode->getReferenceCount() > 2 || (expectedComposedReg != newComposedReg && overExpectedNode->getReferenceCount() > 1))
         {
         needExpectedRegCopy = true;
         tmpCopyReg = expectedComposedReg;
         expectedComposedReg = cg->allocateRegister();
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, expectedComposedReg, tmpCopyReg);
         }

      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldimi, node, newComposedReg, (!isRefFirst ? newMarkOrStampReg : newReferenceReg), 32, CONSTANT64(0xFFFFFFFF00000000));

      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldimi, node, expectedComposedReg, (!isRefFirst ? expectedMarkOrStampReg : expectedReferenceReg), 32,
            CONSTANT64(0xFFFFFFFF00000000));

      /* store offset into object for ldarx/stdcx instructions */
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, fieldOffsetReg, offsetOfFirstField);

      /* Load FALSE as output of inlined function (default) */
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 0);

      generateInstruction(cg, TR::InstOpCode::lwsync, node);

      /* start of loop for failing stdcx. */
      generateLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel);

      /* Perform reservation load of current reference pair */
      generateTrg1MemInstruction(cg, TR::InstOpCode::ldarx, node, tmpReg, new (cg->trHeapMemory()) TR::MemoryReference(currentPairReg, fieldOffsetReg, 8, cg));

      /* Compare with expected pair, branch to failure if not the same */
      generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp8, node, resCndReg, tmpReg, expectedComposedReg);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, doneLabel, resCndReg);

      /* If current pair matches expected pair, conditional store new pair */
      generateMemSrc1Instruction(cg, TR::InstOpCode::stdcx_r, node, new (cg->trHeapMemory()) TR::MemoryReference(currentPairReg, fieldOffsetReg, 8, cg), newComposedReg);

      /* Expect store to be successful, so this branch is usually not taken */
      if (TR::Compiler->target.cpu.id() >= TR_PPCgp)
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, node, loopLabel, resCndReg);
      else
         generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, loopLabel, resCndReg);

      generateInstruction(cg, TR::InstOpCode::sync, node);

      // Expected pair matched current pair, new pair atomically stored into the object.
      // Load TRUE as output of inlined function and jump to end of inlined code
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 1);

      // If we branch over this (take the bne to doneLabel), resultReg is already 0

      doneLabel->setEndInternalControlFlow();
      generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);
      }

   cg->decReferenceCount(node->getChild(0));
   cg->decReferenceCount(currentPairNode);
   cg->decReferenceCount(newReferenceNode);
   cg->decReferenceCount(newMarkOrStampNode);
   if (isConstantNewMarkOrStamp)
      cg->stopUsingRegister(newMarkOrStampReg);

   if (needNewRegCopy)
      cg->stopUsingRegister(newComposedReg);

   if (!isSetOnly)
      {
      cg->decReferenceCount(expectedReferenceNode);
      cg->decReferenceCount(expectedMarkOrStampNode);
      if (isConstantExpectedMarkOrStamp)
         cg->stopUsingRegister(expectedMarkOrStampReg);

      if (needExpectedRegCopy)
         cg->stopUsingRegister(expectedComposedReg);

      cg->stopUsingRegister(fieldOffsetReg);
      cg->stopUsingRegister(resCndReg);
      cg->stopUsingRegister(tmpReg);
      }

   node->setRegister(resultReg);

   return resultReg;
   }


static TR::Register *inlineConcurrentHashMapTmPut(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (comp->fe());
   int32_t addressFieldSize = TR::Compiler->om.sizeofReferenceField();
   TR::InstOpCode::Mnemonic loadOpCode = (addressFieldSize == 8) ? TR::InstOpCode::ld : TR::InstOpCode::lwz;
   TR::InstOpCode::Mnemonic storeOpCode = (addressFieldSize == 8) ? TR::InstOpCode::std : TR::InstOpCode::stw;
   int32_t shiftAmount = (addressFieldSize == 8) ? 3 : 2;
   bool usesCompressedrefs = comp->useCompressedPointers();

   int32_t offsetSyncObj, offsetState, offsetTable, offsetCount, offsetModCount, offsetNext, offsetHash;
   TR_OpaqueClassBlock * classBlock = NULL;

   classBlock = fej9->getClassFromSignature("Ljava/util/concurrent/ConcurrentHashMap$Segment;", 48, comp->getCurrentMethod(), true);
   offsetSyncObj = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "sync", 4, "Ljava/util/concurrent/locks/ReentrantLock$Sync;", 47);
   offsetTable = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "table", 5, "[Ljava/util/concurrent/ConcurrentHashMap$HashEntry;", 51);
   offsetCount = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "count", 5, "I", 1);
   offsetModCount = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "modCount", 8, "I", 1);
   classBlock = fej9->getClassFromSignature("java/util/concurrent/locks/AbstractQueuedSynchronizer", 53, comp->getCurrentMethod(), true);
   offsetState = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "state", 5, "I", 1);
   classBlock = fej9->getClassFromSignature("Ljava/util/concurrent/ConcurrentHashMap$HashEntry;", 50, comp->getCurrentMethod(), true);
   offsetHash = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "hash", 4, "I", 1);
   offsetNext = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "next", 4, "Ljava/util/concurrent/ConcurrentHashMap$HashEntry;", 50);

   TR::Register * rReturn = cg->allocateRegister();
   TR::Register * rThis = cg->evaluate(node->getFirstChild()); //segment
   TR::Register * rHash = cg->evaluate(node->getSecondChild()); //hash
   TR::Register * rNode = cg->evaluate(node->getChild(2)); //Entry
   TR::Register * rTmp = cg->allocateRegister();
   TR::Register * rTmp2 = cg->allocateRegister();
   TR::Register * rTmp3 = cg->allocateRegister();
   TR::Register * rIndex = cg->allocateRegister();
   TR::Register * cndReg = cg->allocateRegister(TR_CCR);
   TR::Register * retryCountReg = cg->allocateRegister();

   TR::LabelSymbol * loopLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * failureLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * abortTxLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * returnLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * bucketNotEmptyLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * lockContendedLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(10, 10, cg->trMemory());

   addDependency(conditions, rReturn, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, rThis, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, rHash, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, rNode, TR::RealRegister::gr4, TR_GPR, cg); // srcReg for wrtbar
   addDependency(conditions, rTmp, TR::RealRegister::gr3, TR_GPR, cg); // dstReg for wrtbar
   addDependency(conditions, rTmp2, TR::RealRegister::gr11, TR_GPR, cg); // temp1Reg for wrtbar
   addDependency(conditions, rTmp3, TR::RealRegister::NoReg, TR_GPR, cg); // temp2Reg for wrtbar
   addDependency(conditions, rIndex, TR::RealRegister::NoReg, TR_GPR, cg); // temp3Reg for wrtbar
   addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);
   addDependency(conditions, retryCountReg, TR::RealRegister::NoReg, TR_GPR, cg); // temp4Reg for wrtbar

   static char * debugTM = feGetEnv("TR_DebugTM");
   static char * disableTMPut = feGetEnv("TR_DisableTMPut");
   if (disableTMPut)
      {
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, rReturn, 3); // TM offer disabled
      generateLabelInstruction(cg, TR::InstOpCode::b, node, returnLabel);
      }

   if (debugTM)
      {
      printf("\nTM: use TM CHM.put in %s (%s)", comp->signature(), comp->getHotnessName(comp->getMethodHotness()));
      fflush (stdout);
      }

   static const char *s = feGetEnv("TR_TMPutRetry");
   static uint16_t TMPutRetry = s ? atoi(s) : 5;

   //#define CHM_PUT_RETRY 5
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, retryCountReg, TMPutRetry);

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel, conditions);

   generateInstruction(cg, TR::InstOpCode::tbegin_r, node);

   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, failureLabel, cndReg);

   // rTmp := this.sync
   generateTrg1MemInstruction(cg, loadOpCode, node, rTmp, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetSyncObj, addressFieldSize, cg));
   if (usesCompressedrefs)
      {
      genDecompressPointer(cg, node, rTmp, NULL, false);
      }

   // rtmp := rtmp.state
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, rTmp, new (cg->trHeapMemory()) TR::MemoryReference(rTmp, offsetState, 4, cg));

   // if rtmp != 0 goto lockContendedLabel (for possible retry)
   generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, cndReg, rTmp, 0);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, lockContendedLabel, cndReg);

   // rTmp := this.table
   generateTrg1MemInstruction(cg, loadOpCode, node, rTmp, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetTable, addressFieldSize, cg));
   if (usesCompressedrefs)
      {
      genDecompressPointer(cg, node, rTmp, NULL, false);
      }
   // rIndex := rTmp.length
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, rIndex, new (cg->trHeapMemory()) TR::MemoryReference(rTmp, fej9->getOffsetOfContiguousArraySizeField(), 4, cg));
   // rIndex -= 1
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic_r, node, rIndex, rIndex, -1);
   // rIndex = hash & rIndex
   generateTrg1Src2Instruction(cg, TR::InstOpCode::AND, node, rIndex, rHash, rIndex);
   // rIndex = rIndex * shiftAmount + ContiguousArrayHeaderSize
   generateShiftLeftImmediate(cg, node, rIndex, rIndex, shiftAmount);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, rIndex, rIndex, TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
   // rReturn := rTmp[rIndex]
   generateTrg1MemInstruction(cg, loadOpCode, node, rReturn, new (cg->trHeapMemory()) TR::MemoryReference(rTmp, rIndex, addressFieldSize, cg));

   // if (null != rReturn) goto bucketNotEmptyLabel
   generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, cndReg, rReturn, 0);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, node, bucketNotEmptyLabel, cndReg);
   // bucket_empty, rTmp[rIndex] = rnode
   TR::Register *rNodeSt = rNode;
   if (usesCompressedrefs)
      {
      rNodeSt = genCompressPointerNonNull2Regs(cg, node, rNode, rTmp2);
      }
   generateMemSrc1Instruction(cg, storeOpCode, node, new (cg->trHeapMemory()) TR::MemoryReference(rTmp, rIndex, addressFieldSize, cg), rNodeSt);
   // MAY NOT NEED WRTBAR HERE since node is newly allocated
   // ++count
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, rTmp3, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetCount, 4, cg));
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, rTmp3, rTmp3, 1);
   generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetCount, 4, cg), rTmp3);
   // ++modCount
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, rIndex, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetModCount, addressFieldSize, cg));
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, rIndex, rIndex, 1);
   generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetModCount, 4, cg), rIndex);

   // ALG: tend
   generateInstruction(cg, TR::InstOpCode::tend_r, node);

   // res := 0, success
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, rReturn, 0);

   // wrtbar for rTmp[rIndex] = rnode
   TR_WriteBarrierKind gcMode = comp->getOptions()->getGcMode();
   bool doWrtBar = (gcMode == TR_WrtbarRealTime || gcMode == TR_WrtbarOldCheck || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarAlways
         || TR::Options::getCmdLineOptions()->realTimeGC());
   bool doCrdMrk = ((gcMode == TR_WrtbarCardMark || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarCardMarkIncremental) && (!node->getOpCode().isWrtBar() || !node->isNonHeapObjectWrtBar()));
   if (doWrtBar)
      {
      if (doCrdMrk)
         {
         conditions->getPostConditions()->getRegisterDependency(6)->setExcludeGPR0(); //6=temp2Reg
         }
      VMnonNullSrcWrtBarCardCheckEvaluator(node, rNode, rTmp, cndReg, rTmp2, rTmp3, rIndex, retryCountReg, returnLabel, conditions, NULL, false, false, cg, false);
      }
   else if (doCrdMrk)
      {
      conditions->getPostConditions()->getRegisterDependency(4)->setExcludeGPR0(); //4=dstReg
      conditions->getPostConditions()->getRegisterDependency(5)->setExcludeGPR0(); //5=temp1Reg

      VMCardCheckEvaluator(node, rTmp, cndReg, rTmp2, rTmp3, rIndex, conditions, cg);
      }

   //goto returnLabel
   generateLabelInstruction(cg, TR::InstOpCode::b, node, returnLabel);

   // *** bucket_not_empty lable
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, bucketNotEmptyLabel, conditions);
   TR::Register *firstNodeLd = rReturn;
   if (usesCompressedrefs)
      {
      firstNodeLd = genDecompressPointerNonNull2Regs(cg, node, rReturn, rTmp3); //rReturn not null, decompress it
      }

   // rTmp2 = rReturn.next
   generateTrg1MemInstruction(cg, loadOpCode, node, rTmp2, new (cg->trHeapMemory()) TR::MemoryReference(firstNodeLd, offsetNext, addressFieldSize, cg));
   // if rTmp2 != null, goto abortTxLabel
   generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, cndReg, rTmp2, 0);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, node, abortTxLabel, cndReg);
   // rTmp2 = rReturn.hash
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, rTmp2, new (cg->trHeapMemory()) TR::MemoryReference(firstNodeLd, offsetHash, 4, cg));
   // if rTmp2 == hash, goto abortTxLabel
   // Different hash -> must be different objects (i.e. fails .equals )
   generateTrg1Src2Instruction(cg, TR::InstOpCode::cmpl4, node, cndReg, rTmp2, rHash);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, PPCOpProp_BranchUnlikely, node, abortTxLabel, cndReg);

   // rNode.next = rReturn // NO WRT BAR SINCE rNode is NEW
   generateMemSrc1Instruction(cg, storeOpCode, node, new (cg->trHeapMemory()) TR::MemoryReference(rNode, offsetNext, addressFieldSize, cg), rReturn);

   // rTmp[rIndex] = rNode
   rNodeSt = rNode;
   if (usesCompressedrefs)
      {
      rNodeSt = genCompressPointerNonNull2Regs(cg, node, rNode, rTmp2); //rNode not null, compress
      }
   generateMemSrc1Instruction(cg, storeOpCode, node, new (cg->trHeapMemory()) TR::MemoryReference(rTmp, rIndex, addressFieldSize, cg), rNodeSt);
   // ++count
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, rTmp3, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetCount, 4, cg));
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, rTmp3, rTmp3, 1);
   generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetCount, 4, cg), rTmp3);
   // ++modCount
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, rIndex, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetModCount, addressFieldSize, cg));
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, rIndex, rIndex, 1);
   generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetModCount, 4, cg), rIndex);

   // ALG: tend
   generateInstruction(cg, TR::InstOpCode::tend_r, node);
   // res := 0, success
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, rReturn, 0);

   // wrtbar for rTmp[rIndex] = rnode
   if (doWrtBar)
      {
      if (doCrdMrk)
         {
         conditions->getPostConditions()->getRegisterDependency(6)->setExcludeGPR0(); //6=temp2Reg
         }
      VMnonNullSrcWrtBarCardCheckEvaluator(node, rNode, rTmp, cndReg, rTmp2, rTmp3, rIndex, retryCountReg, returnLabel, conditions, NULL, false, false, cg, false);
      }
   else if (doCrdMrk)
      {
      conditions->getPostConditions()->getRegisterDependency(4)->setExcludeGPR0(); //4=dstReg
      conditions->getPostConditions()->getRegisterDependency(5)->setExcludeGPR0(); //5=temp1Reg
      VMCardCheckEvaluator(node, rTmp, cndReg, rTmp2, rTmp3, rIndex, conditions, cg);
      }

   //goto returnLabel
   generateLabelInstruction(cg, TR::InstOpCode::b, node, returnLabel);

   // *** abortTxLabel
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, abortTxLabel, conditions);
   // tend
   generateInstruction(cg, TR::InstOpCode::tend_r, node);
   // res := 1 failure
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, rReturn, 1);
   //goto returnLabel
   generateLabelInstruction(cg, TR::InstOpCode::b, node, returnLabel);

   // ALG: *** lockContendedLabel
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, lockContendedLabel, conditions);
   // tend
   generateInstruction(cg, TR::InstOpCode::tend_r, node);
   // ALG: *** failureLabel
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, failureLabel, conditions);
   // retryCount-=1
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic_r, node, retryCountReg, retryCountReg, -1);
   // ALG: if retryCount != 0, goto loopLabel
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, loopLabel, cndReg);
   // ALG: exceeds retry count, res := 2 failure
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, rReturn, 2);

   // ALG: *** return
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, returnLabel, conditions);

   cg->stopUsingRegister(rTmp);
   cg->stopUsingRegister(rTmp2);
   cg->stopUsingRegister(rTmp3);
   cg->stopUsingRegister(rIndex);
   cg->stopUsingRegister(retryCountReg);
   cg->stopUsingRegister(cndReg);
   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   cg->decReferenceCount(node->getChild(2));

   node->setRegister(rReturn);
   return rReturn;
   }

static TR::Register *inlineConcurrentHashMapTmRemove(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (comp->fe());
   int32_t addressFieldSize = TR::Compiler->om.sizeofReferenceField();
   TR::InstOpCode::Mnemonic loadOpCode = (addressFieldSize == 8) ? TR::InstOpCode::ld : TR::InstOpCode::lwz;
   TR::InstOpCode::Mnemonic storeOpCode = (addressFieldSize == 8) ? TR::InstOpCode::std : TR::InstOpCode::stw;
   int32_t shiftAmount = (addressFieldSize == 8) ? 3 : 2;
   bool usesCompressedrefs = comp->useCompressedPointers();

   int32_t offsetSyncObj, offsetState, offsetTable, offsetCount, offsetModCount, offsetNext, offsetHash, offsetKey, offsetValue;
   TR_OpaqueClassBlock * classBlock = NULL;

   classBlock = fej9->getClassFromSignature("Ljava/util/concurrent/ConcurrentHashMap$Segment;", 48, comp->getCurrentMethod(), true);
   offsetSyncObj = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "sync", 4, "Ljava/util/concurrent/locks/ReentrantLock$Sync;", 47);
   offsetTable = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "table", 5, "[Ljava/util/concurrent/ConcurrentHashMap$HashEntry;", 51);
   offsetCount = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "count", 5, "I", 1);
   offsetModCount = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "modCount", 8, "I", 1);
   classBlock = fej9->getClassFromSignature("java/util/concurrent/locks/AbstractQueuedSynchronizer", 53, comp->getCurrentMethod(), true);
   offsetState = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "state", 5, "I", 1);
   classBlock = fej9->getClassFromSignature("Ljava/util/concurrent/ConcurrentHashMap$HashEntry;", 50, comp->getCurrentMethod(), true);
   offsetHash = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "hash", 4, "I", 1);
   offsetNext = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "next", 4, "Ljava/util/concurrent/ConcurrentHashMap$HashEntry;", 50);
   offsetKey = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "key", 3, "Ljava/lang/Object;", 18);
   offsetValue = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "value", 5, "Ljava/lang/Object;", 18);

   TR::Register * rReturn = cg->allocateRegister();
   TR::Register * rThis = cg->evaluate(node->getFirstChild()); //segment
   TR::Register * rKey = cg->evaluate(node->getSecondChild()); //key
   TR::Register * rHash = cg->evaluate(node->getChild(2)); //hash
   TR::Register * rValue = cg->evaluate(node->getChild(3)); //may/may not want to use value here
   TR::Register * rTmp = cg->allocateRegister();
   TR::Register * rTmp2 = cg->allocateRegister();
   TR::Register * rIndex = cg->allocateRegister();
   TR::Register * cndReg = cg->allocateRegister(TR_CCR);
   TR::Register * retryCountReg = cg->allocateRegister();

   TR::LabelSymbol * loopLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * failureLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * lockContendedLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * checkSecondEntryLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * abortTxLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * returnLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(10, 10, cg->trMemory());

   addDependency(conditions, rReturn, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, rThis, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, rKey, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, rHash, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, rValue, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, rTmp, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, rTmp2, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, rIndex, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);
   addDependency(conditions, retryCountReg, TR::RealRegister::NoReg, TR_GPR, cg);

   static char * debugTM = feGetEnv("TR_DebugTM");
   static char * disableTMPut = feGetEnv("TR_DisableTMRemove");
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, rReturn, 0);
   generateLabelInstruction(cg, TR::InstOpCode::b, node, returnLabel);
   if (debugTM)
      {
      printf("\nTM: use TM CHM.remove in %s (%s)", comp->signature(), comp->getHotnessName(comp->getMethodHotness()));
      fflush (stdout);
      }

#define CHM_REMOVE_RETRY 5
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, retryCountReg, CHM_REMOVE_RETRY);

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel, conditions);

   generateInstruction(cg, TR::InstOpCode::tbegin_r, node);

   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, node, failureLabel, cndReg);

   // rTmp := this.sync
   generateTrg1MemInstruction(cg, loadOpCode, node, rTmp, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetSyncObj, addressFieldSize, cg));
   if (usesCompressedrefs)
      {
      genDecompressPointer(cg, node, rTmp, NULL, false);
      }
   // rtmp := rtmp.state
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, rTmp, new (cg->trHeapMemory()) TR::MemoryReference(rTmp, offsetState, 4, cg));

   // if rtmp != 0 goto lockContendedLabel (for possible retry)
   generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, cndReg, rTmp, 0);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, lockContendedLabel, cndReg);

   // rTmp := this.table
   generateTrg1MemInstruction(cg, loadOpCode, node, rTmp, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetTable, addressFieldSize, cg));
   if (usesCompressedrefs)
      {
      genDecompressPointer(cg, node, rTmp, NULL, false); //table not null
      }
   // rIndex := rTmp.length
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, rIndex, new (cg->trHeapMemory()) TR::MemoryReference(rTmp, fej9->getOffsetOfContiguousArraySizeField(), 4, cg));
   // rIndex -= 1
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic_r, node, rIndex, rIndex, -1);
   // rIndex = hash & rIndex
   generateTrg1Src2Instruction(cg, TR::InstOpCode::AND, node, rIndex, rHash, rIndex);
   // rIndex = rIndex * shiftAmount + ContiguousArrayHeaderSize
   generateShiftLeftImmediate(cg, node, rIndex, rIndex, shiftAmount);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, rIndex, rIndex, TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
   // rReturn := rTmp[rIndex]
   generateTrg1MemInstruction(cg, loadOpCode, node, rReturn, new (cg->trHeapMemory()) TR::MemoryReference(rTmp, rIndex, addressFieldSize, cg));

   // if (null == rReturn) goto abortTxLabel
   generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, cndReg, rReturn, 0);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, PPCOpProp_BranchUnlikely, node, abortTxLabel, cndReg);

   if (usesCompressedrefs)
      {
      genDecompressPointer(cg, node, rReturn, NULL, false); //rReturn not null, decompress it
      }
   // rTmp2 = rReturn.key
   generateTrg1MemInstruction(cg, loadOpCode, node, rTmp2, new (cg->trHeapMemory()) TR::MemoryReference(rReturn, offsetKey, addressFieldSize, cg));
   if (usesCompressedrefs)
      {
      genDecompressPointer(cg, node, rTmp2, NULL, false); //null key not allowed, decompress it
      }
   // if rKey != rTmp2, goto keyNE1Label
   generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, cndReg, rTmp2, rKey);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, node, checkSecondEntryLabel, cndReg);
   // rTmp2 = rReturn.next
   generateTrg1MemInstruction(cg, loadOpCode, node, rTmp2, new (cg->trHeapMemory()) TR::MemoryReference(rReturn, offsetNext, addressFieldSize, cg));
   // rTmp[rIndex] = rTmp2
   generateMemSrc1Instruction(cg, storeOpCode, node, new (cg->trHeapMemory()) TR::MemoryReference(rTmp, rIndex, addressFieldSize, cg), rTmp2);
   // rReturn = rReturn.value
   generateTrg1MemInstruction(cg, loadOpCode, node, rReturn, new (cg->trHeapMemory()) TR::MemoryReference(rReturn, offsetValue, addressFieldSize, cg));
   if (usesCompressedrefs)
      {
      genDecompressPointer(cg, node, rReturn, NULL, false); //null value not allowed, decompress it
      }
   // ++count
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, rTmp, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetCount, 4, cg));
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, rTmp, rTmp, 1);
   generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetCount, 4, cg), rTmp);
   // --modCount
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, rIndex, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetModCount, addressFieldSize, cg));
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, rIndex, rIndex, -1);
   generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetModCount, 4, cg), rIndex);
   // ALG: tend
   generateInstruction(cg, TR::InstOpCode::tend_r, node);
   //goto returnLabel
   generateLabelInstruction(cg, TR::InstOpCode::b, node, returnLabel);

   // *** checkSecondEntryLabel (rReturn has 1st entry)
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, checkSecondEntryLabel, conditions);

   // rTmp = rReturn.next (rTmp has 2nd entry)
   generateTrg1MemInstruction(cg, loadOpCode, node, rTmp, new (cg->trHeapMemory()) TR::MemoryReference(rReturn, offsetNext, addressFieldSize, cg));
   // if NULL == rTmp, goto abortTxLabel
   generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, cndReg, rTmp, 0);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, PPCOpProp_BranchUnlikely, node, abortTxLabel, cndReg);
   if (usesCompressedrefs)
      {
      genDecompressPointer(cg, node, rTmp, NULL, false); //2nd entry not null, decompress it
      }
   //rIndex = rTmp.key
   generateTrg1MemInstruction(cg, loadOpCode, node, rIndex, new (cg->trHeapMemory()) TR::MemoryReference(rTmp, offsetKey, addressFieldSize, cg));
   if (usesCompressedrefs)
      {
      genDecompressPointer(cg, node, rIndex, NULL, false); //null key not allowed, decompress it
      }
   // if rKey != rIndex, goto abortTxLabel
   generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, cndReg, rIndex, rKey);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, PPCOpProp_BranchUnlikely, node, abortTxLabel, cndReg);

   // rTmp2 = rTmp.next
   generateTrg1MemInstruction(cg, loadOpCode, node, rTmp2, new (cg->trHeapMemory()) TR::MemoryReference(rTmp, offsetNext, addressFieldSize, cg));
   // rReturn.next = rTmp2
   generateMemSrc1Instruction(cg, storeOpCode, node, new (cg->trHeapMemory()) TR::MemoryReference(rReturn, offsetNext, addressFieldSize, cg), rTmp2);
   // rReturn = rTmp.value
   generateTrg1MemInstruction(cg, loadOpCode, node, rReturn, new (cg->trHeapMemory()) TR::MemoryReference(rTmp, offsetValue, addressFieldSize, cg));
   if (usesCompressedrefs)
      {
      genDecompressPointer(cg, node, rReturn, NULL, false); //null value not allowed, decompress it
      }
   // ++count
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, rTmp, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetCount, 4, cg));
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, rTmp, rTmp, 1);
   generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetCount, 4, cg), rTmp);
   // --modCount
   generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, rIndex, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetModCount, addressFieldSize, cg));
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, rIndex, rIndex, -1);
   generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(rThis, offsetModCount, 4, cg), rIndex);
   // ALG: tend
   generateInstruction(cg, TR::InstOpCode::tend_r, node);
   //goto returnLabel
   generateLabelInstruction(cg, TR::InstOpCode::b, node, returnLabel);

   // *** abortTxLabel
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, abortTxLabel, conditions);
   // tend
   generateInstruction(cg, TR::InstOpCode::tend_r, node);
   // res := 0 failure (do slow path)
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, rReturn, 0);
   //goto returnLabel
   generateLabelInstruction(cg, TR::InstOpCode::b, node, returnLabel);

   // *** lockContendedLabel
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, lockContendedLabel, conditions);
   // tend
   generateInstruction(cg, TR::InstOpCode::tend_r, node);
   // ALG: *** failureLabel
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, failureLabel, conditions);
   // retryCount-=1
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic_r, node, retryCountReg, retryCountReg, -1);
   // ALG: if retryCount != 0, goto loopLabel
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, loopLabel, cndReg);
   // ALG: exceeds retry count, res := 0 failure
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, rReturn, 0);

   // ALG: *** return
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, returnLabel, conditions);

   cg->stopUsingRegister(rTmp);
   cg->stopUsingRegister(rTmp2);
   cg->stopUsingRegister(rIndex);
   cg->stopUsingRegister(retryCountReg);
   cg->stopUsingRegister(cndReg);
   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   cg->decReferenceCount(node->getChild(2));
   cg->decReferenceCount(node->getChild(3));

   node->setRegister(rReturn);
   return rReturn;
   }

static TR::Register *inlineConcurrentLinkedQueueTMOffer(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (comp->fe());
   int32_t addressFieldSize = TR::Compiler->om.sizeofReferenceField();
   TR::InstOpCode::Mnemonic loadOpCode = (addressFieldSize == 8) ? TR::InstOpCode::ld : TR::InstOpCode::lwz;
   TR::InstOpCode::Mnemonic storeOpCode = (addressFieldSize == 8) ? TR::InstOpCode::std : TR::InstOpCode::stw;
   bool usesCompressedrefs = comp->useCompressedPointers();

   TR_OpaqueClassBlock * classBlock = NULL;
   classBlock = fej9->getClassFromSignature("Ljava/util/concurrent/ConcurrentLinkedQueue$Node;", 49, comp->getCurrentMethod(), true);
   int32_t offsetNext = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "next", 4, "Ljava/util/concurrent/ConcurrentLinkedQueue$Node;", 49);
   classBlock = fej9->getClassFromSignature("Ljava/util/concurrent/ConcurrentLinkedQueue;", 44, comp->getCurrentMethod(), true);
   int32_t offsetTail = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "tail", 4, "Ljava/util/concurrent/ConcurrentLinkedQueue$Node;", 49);

   TR::Register * cndReg = cg->allocateRegister(TR_CCR);
   TR::Register * resultReg = cg->allocateRegister();
   TR::Register * objReg = cg->evaluate(node->getFirstChild());
   TR::Register * nReg = cg->evaluate(node->getSecondChild());
   TR::Register * pReg = cg->allocateRegister();
   TR::Register * qReg = cg->allocateRegister();
   TR::Register * retryCountReg = cg->allocateRegister();
   TR::Register * temp3Reg = NULL;
   TR::Register * temp4Reg = NULL;

   TR::LabelSymbol * loopLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * insertLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * failureLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * returnLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * wrtbar1Donelabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(9, 9, cg->trMemory());

   addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);
   addDependency(conditions, resultReg, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, objReg, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, pReg, TR::RealRegister::gr3, TR_GPR, cg); // dstReg for wrtbar
   addDependency(conditions, qReg, TR::RealRegister::gr11, TR_GPR, cg); // temp1Reg for wrtbar
   addDependency(conditions, retryCountReg, TR::RealRegister::NoReg, TR_GPR, cg); // temp2Reg for wrtbar
   addDependency(conditions, nReg, TR::RealRegister::gr4, TR_GPR, cg); // srcReg for wrtbar

   static char * disableTMOffer = feGetEnv("TR_DisableTMOffer");
   static char * debugTM = feGetEnv("TR_DebugTM");

   if (disableTMOffer)
      {
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 3); // TM offer disabled
      generateLabelInstruction(cg, TR::InstOpCode::b, node, returnLabel);
      }

   if (debugTM)
      {
      printf("\nTM: use TM CLQ.Offer in %s (%s)", comp->signature(), comp->getHotnessName(comp->getMethodHotness()));
      fflush (stdout);
      }

   static const char *s = feGetEnv("TR_TMOfferRetry");
   static uint16_t TMOfferRetry = s ? atoi(s) : 7;

   //#define CLQ_OFFER_RETRY 7
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, retryCountReg, TMOfferRetry);

   generateLabelInstruction(cg, TR::InstOpCode::label, node, loopLabel);

   generateInstruction(cg, TR::InstOpCode::tbegin_r, node);

   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, failureLabel, cndReg);

   // ALG: p := this.tail
   generateTrg1MemInstruction(cg, loadOpCode, node, pReg, new (cg->trHeapMemory()) TR::MemoryReference(objReg, offsetTail, addressFieldSize, cg));
   if (usesCompressedrefs)
      {
      genDecompressPointerWithTempReg(cg, node, pReg, resultReg, NULL, false); // p is not null
      }

   // ALG: q := p.next
   generateTrg1MemInstruction(cg, loadOpCode, node, qReg, new (cg->trHeapMemory()) TR::MemoryReference(pReg, offsetNext, addressFieldSize, cg));

   // ALG: if q == null goto insertLabel
   generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, cndReg, qReg, 0);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, insertLabel, cndReg);

   if (usesCompressedrefs)
      {
      genDecompressPointerWithTempReg(cg, node, qReg, resultReg, NULL, false); // q has been checked not null
      }

   // ALG: p := q
   generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, pReg, qReg);

   // ALG: q := p.next
   generateTrg1MemInstruction(cg, loadOpCode, node, qReg, new (cg->trHeapMemory()) TR::MemoryReference(pReg, offsetNext, addressFieldSize, cg));

   // ALG: if q == null goto insertLabel
   generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, cndReg, qReg, 0);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, PPCOpProp_BranchLikely, node, insertLabel, cndReg);

   // ALG: tend.
   generateInstruction(cg, TR::InstOpCode::tend_r, node);

   // ALG: res := 1
   // cannot find tail
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 1);

   // ALG: goto returnLabel
   generateLabelInstruction(cg, TR::InstOpCode::b, node, returnLabel);

   // ALG: *** insert:
   generateLabelInstruction(cg, TR::InstOpCode::label, node, insertLabel);

   // need uncompressed nReg for the wrtbar
   TR::Register *compressedReg = nReg;
   if (usesCompressedrefs)
      {
      compressedReg = genCompressPointerNonNull2RegsWithTempReg(cg, node, nReg, qReg, resultReg); //In Java code checkNotNull() already ensures n != null
      }
   // ALG: p.next := n
   generateMemSrc1Instruction(cg, storeOpCode, node, new (cg->trHeapMemory()) TR::MemoryReference(pReg, offsetNext, addressFieldSize, cg), compressedReg);

   // ALG: this.tail = n
   generateMemSrc1Instruction(cg, storeOpCode, node, new (cg->trHeapMemory()) TR::MemoryReference(objReg, offsetTail, addressFieldSize, cg), compressedReg);

   // ALG: tend
   generateInstruction(cg, TR::InstOpCode::tend_r, node);

   // ALG: res := 0
   // TM success
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 0);

   // wrt bar
   TR_WriteBarrierKind gcMode = comp->getOptions()->getGcMode();
   bool doWrtBar = (gcMode == TR_WrtbarRealTime || gcMode == TR_WrtbarOldCheck || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarAlways
         || TR::Options::getCmdLineOptions()->realTimeGC());
   bool doCrdMrk = ((gcMode == TR_WrtbarCardMark || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarCardMarkIncremental) && (!node->getOpCode().isWrtBar() || !node->isNonHeapObjectWrtBar()));
   if (doWrtBar)
      {
      if (doCrdMrk)
         {
         temp3Reg = cg->allocateRegister();
         temp4Reg = cg->allocateRegister();
         addDependency(conditions, temp3Reg, TR::RealRegister::NoReg, TR_GPR, cg);
         addDependency(conditions, temp4Reg, TR::RealRegister::NoReg, TR_GPR, cg);
         conditions->getPostConditions()->getRegisterDependency(5)->setExcludeGPR0(); //5=temp2Reg
         }

      VMnonNullSrcWrtBarCardCheckEvaluator(node, nReg, pReg, cndReg, qReg, retryCountReg, temp3Reg, temp4Reg, wrtbar1Donelabel, conditions, NULL, false, false, cg, false);
      // ALG: *** wrtbar1Donelabel
      generateLabelInstruction(cg, TR::InstOpCode::label, node, wrtbar1Donelabel);

      generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, pReg, objReg);

      VMnonNullSrcWrtBarCardCheckEvaluator(node, nReg, pReg, cndReg, qReg, retryCountReg, temp3Reg, temp4Reg, returnLabel, conditions, NULL, false, false, cg, false);

      }
   else if (doCrdMrk)
      {
      temp3Reg = cg->allocateRegister();
      addDependency(conditions, temp3Reg, TR::RealRegister::NoReg, TR_GPR, cg);
      conditions->getPostConditions()->getRegisterDependency(3)->setExcludeGPR0(); //3=dstReg
      conditions->getPostConditions()->getRegisterDependency(4)->setExcludeGPR0(); //4=temp1Reg

      VMCardCheckEvaluator(node, pReg, cndReg, qReg, retryCountReg, temp3Reg, conditions, cg);

      generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, pReg, objReg);

      VMCardCheckEvaluator(node, pReg, cndReg, qReg, retryCountReg, temp3Reg, conditions, cg);
      }

   // ALG: goto returnLabel
   generateLabelInstruction(cg, TR::InstOpCode::b, node, returnLabel);

   // ALG: *** failureLabel
   generateLabelInstruction(cg, TR::InstOpCode::label, node, failureLabel);
   //generateDepLabelInstruction(cg, TR::InstOpCode::label, node, failureLabel, conditions);

   // retryCount-=1
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic_r, node, retryCountReg, retryCountReg, -1);

   // ALG: goto loopLabel
   generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, loopLabel, cndReg);

   // ALG: res := 2
   // exceeds retry count
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 2);

   // ALG: *** return
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, returnLabel, conditions);

   // wrtbar or after TM success earlier

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   cg->stopUsingRegister(pReg);
   cg->stopUsingRegister(qReg);
   cg->stopUsingRegister(retryCountReg);
   cg->stopUsingRegister(cndReg);
   if (temp3Reg != NULL)
      cg->stopUsingRegister(temp3Reg);
   if (temp4Reg != NULL)
      cg->stopUsingRegister(temp4Reg);

   node->setRegister(resultReg);
   resultReg->setContainsCollectedReference();
   return resultReg;
   }

static TR::Register *inlineConcurrentLinkedQueueTMPoll(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (comp->fe());
   int32_t addressFieldSize = TR::Compiler->om.sizeofReferenceField();
   TR::InstOpCode::Mnemonic loadOpCode = (addressFieldSize == 8) ? TR::InstOpCode::ld : TR::InstOpCode::lwz;
   TR::InstOpCode::Mnemonic storeOpCode = (addressFieldSize == 8) ? TR::InstOpCode::std : TR::InstOpCode::stw;
   bool usesCompressedrefs = comp->useCompressedPointers();

   TR_OpaqueClassBlock *classBlock = fej9->getClassFromSignature("Ljava/util/concurrent/ConcurrentLinkedQueue;", 44, comp->getCurrentMethod(), true);
   int32_t offsetHead = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "head", 4, "Ljava/util/concurrent/ConcurrentLinkedQueue$Node;", 49);
   classBlock = fej9->getClassFromSignature("Ljava/util/concurrent/ConcurrentLinkedQueue$Node;", 49, comp->getCurrentMethod(), true);
   int32_t offsetNext = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "next", 4, "Ljava/util/concurrent/ConcurrentLinkedQueue$Node;", 49);
   int32_t offsetItem = fej9->getObjectHeaderSizeInBytes() + fej9->getInstanceFieldOffset(classBlock, "item", 4, "Ljava/lang/Object;", 18);

   TR::Register * objReg = cg->evaluate(node->getFirstChild());
   TR::Register * cndReg = cg->allocateRegister(TR_CCR);
   TR::Register * resultReg = cg->allocateRegister();
   TR::Register * nullReg = cg->allocateRegister();
   TR::Register * pReg = cg->allocateRegister();
   TR::Register * qReg = cg->allocateRegister();
   TR::Register * temp3Reg = cg->allocateRegister();
   TR::Register * temp4Reg = NULL;

   TR::LabelSymbol * tendLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * returnLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol * failureLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(8, 8, cg->trMemory());

   addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);
   addDependency(conditions, resultReg, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, objReg, TR::RealRegister::gr3, TR_GPR, cg); // dstReg for wrtbar
   addDependency(conditions, nullReg, TR::RealRegister::gr11, TR_GPR, cg); // temp1Reg for wrtbar
   addDependency(conditions, pReg, TR::RealRegister::NoReg, TR_GPR, cg); // temp2Reg for wrtbar
   addDependency(conditions, qReg, TR::RealRegister::gr4, TR_GPR, cg); // srcReg for wrtbar
   addDependency(conditions, temp3Reg, TR::RealRegister::NoReg, TR_GPR, cg);

   static char * disableTMPoll = feGetEnv("TR_DisableTMPoll");
   static char * debugTM = feGetEnv("TR_DebugTM");

   if (disableTMPoll)
      {
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 0); // BEFORE WAS 0
      generateLabelInstruction(cg, TR::InstOpCode::b, node, returnLabel);
      }

   if (debugTM)
      {
      printf("\nTM: use TM CLQ.Poll in %s (%s)", comp->signature(), comp->getHotnessName(comp->getMethodHotness()));
      fflush (stdout);
      }

   generateInstruction(cg, TR::InstOpCode::tbegin_r, node);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, failureLabel, cndReg);

   // ALG: p := this.head
   generateTrg1MemInstruction(cg, loadOpCode, node, temp3Reg, new (cg->trHeapMemory()) TR::MemoryReference(objReg, offsetHead, addressFieldSize, cg));
   TR::Register *decompressedP = temp3Reg;
   if (usesCompressedrefs)
      {
      decompressedP = genDecompressPointerNonNull2RegsWithTempReg(cg, node, temp3Reg, pReg, qReg);
      }

   // ALG: res := p.item
   generateTrg1MemInstruction(cg, loadOpCode, node, resultReg, new (cg->trHeapMemory()) TR::MemoryReference(decompressedP, offsetItem, addressFieldSize, cg));
   if (usesCompressedrefs)
      {
      genDecompressPointerWithTempReg(cg, node, resultReg, qReg, cndReg); // NOTE: res could be null
      }

   // ALG: q := p.next
   generateTrg1MemInstruction(cg, loadOpCode, node, qReg, new (cg->trHeapMemory()) TR::MemoryReference(decompressedP, offsetNext, addressFieldSize, cg));

   // ALG: p.item := null
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, nullReg, 0);

   generateMemSrc1Instruction(cg, storeOpCode, node, new (cg->trHeapMemory()) TR::MemoryReference(decompressedP, offsetItem, addressFieldSize, cg), nullReg);

   // ALG: if q == null goto tendLabel
   generateTrg1Src1ImmInstruction(cg,TR::InstOpCode::Op_cmpli, node, cndReg, qReg, 0);

   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, tendLabel, cndReg);

   // ALG: this.head = q
   // qReg is still compressed, use it to store
   generateMemSrc1Instruction(cg, storeOpCode, node, new (cg->trHeapMemory()) TR::MemoryReference(objReg, offsetHead, addressFieldSize, cg), qReg);

   if (usesCompressedrefs)
      {
      genDecompressPointerWithTempReg(cg, node, qReg, nullReg, NULL, false); // qReg is not null
      }

   // ALG: p.next := p
   generateMemSrc1Instruction(cg, storeOpCode, node, new (cg->trHeapMemory()) TR::MemoryReference(decompressedP, offsetNext, addressFieldSize, cg), temp3Reg);

   // ALG: tend
   generateInstruction(cg, TR::InstOpCode::tend_r, node);

   // WrtBar for this.head = q
   TR_WriteBarrierKind gcMode = comp->getOptions()->getGcMode();
   bool doWrtBar = (gcMode == TR_WrtbarRealTime || gcMode == TR_WrtbarOldCheck || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarAlways
         || TR::Options::getCmdLineOptions()->realTimeGC());
   bool doCrdMrk = ((gcMode == TR_WrtbarCardMark || gcMode == TR_WrtbarCardMarkAndOldCheck || gcMode == TR_WrtbarCardMarkIncremental) && (!node->getOpCode().isWrtBar() || !node->isNonHeapObjectWrtBar()));
   if (doWrtBar)
      {
      if (doCrdMrk)
         {
         temp4Reg = cg->allocateRegister();
         addDependency(conditions, temp4Reg, TR::RealRegister::NoReg, TR_GPR, cg);
         conditions->getPostConditions()->getRegisterDependency(4)->setExcludeGPR0(); //4=temp2Reg
         }

      VMnonNullSrcWrtBarCardCheckEvaluator(node, qReg, objReg, cndReg, nullReg, pReg, temp3Reg, temp4Reg, returnLabel, conditions, NULL, false, false, cg, false);
      }
   else if (doCrdMrk)
      {
      conditions->getPostConditions()->getRegisterDependency(2)->setExcludeGPR0(); //2=dstReg
      conditions->getPostConditions()->getRegisterDependency(3)->setExcludeGPR0(); //3=temp1Reg

      VMCardCheckEvaluator(node, objReg, cndReg, nullReg, pReg, temp3Reg, conditions, cg);
      }

   // ALG: goto returnLabel
   generateLabelInstruction(cg, TR::InstOpCode::b, node, returnLabel);

   // ALG:**** tendLabel:
   generateLabelInstruction(cg, TR::InstOpCode::label, node, tendLabel);

   // ALG: tend
   generateInstruction(cg, TR::InstOpCode::tend_r, node);

   // ALG: goto returnLabel
   generateLabelInstruction(cg, TR::InstOpCode::b, node, returnLabel);

   // ALG: *** failureLabel
   generateLabelInstruction(cg, TR::InstOpCode::label, node, failureLabel);
   // resultReg := 0
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 0);

   // ALG: *** returnLabel
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, returnLabel, conditions);

   // wrtbar if result is non-null

   cg->decReferenceCount(node->getFirstChild());
   cg->stopUsingRegister(nullReg);
   cg->stopUsingRegister(pReg);
   cg->stopUsingRegister(qReg);
   cg->stopUsingRegister(cndReg);
   if (temp3Reg != NULL)
      cg->stopUsingRegister(temp3Reg);
   if (temp4Reg != NULL)
      cg->stopUsingRegister(temp4Reg);

   node->setRegister(resultReg);
   resultReg->setContainsCollectedReference();
   return resultReg;
   }

#if defined(AIXPPC)
static TR::Register *dangerousGetCPU(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   // This is a bad idea that works most of the time.
   // We don't bother with ANY of the usual native method protocol(!)
   // But we do adjust r1 to be way out of the way and then call
   // "mycpu" which is buried deep in the kernel

   TR::Register *retReg = cg->allocateRegister();
   TR::Register *gr1 = cg->allocateRegister();
   TR::Register *gr2 = cg->allocateRegister();
   TR::Register *tmpReg = cg->allocateRegister();
   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(4, 4, cg->trMemory());
   TR::Compilation* comp = cg->comp();
   addDependency(dependencies, retReg, TR::RealRegister::gr3, TR_GPR, cg);
   addDependency(dependencies, gr1, TR::RealRegister::gr1, TR_GPR, cg);
   addDependency(dependencies, gr2, TR::RealRegister::gr2, TR_GPR, cg);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, gr1, gr1, -256);
   generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node,
         new (cg->trHeapMemory()) TR::MemoryReference(gr1, 0x14, TR::Compiler->om.sizeofReferenceAddress(), cg),
         gr2);
   intptr_t xx[3];
   xx[0] = ((intptr_t *)(void *)&mycpu)[0];
   xx[1] = ((intptr_t *)(void *)&mycpu)[1];
   xx[2] = ((intptr_t *)(void *)&mycpu)[2];
   loadAddressConstant(cg, node, xx[0], tmpReg);
   loadAddressConstant(cg, node, xx[1], gr2);
   generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, tmpReg);
   generateInstruction(cg, TR::InstOpCode::bctrl, node);

   generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, gr2,
         new (cg->trHeapMemory()) TR::MemoryReference(gr1, 0x14, TR::Compiler->om.sizeofReferenceAddress(), cg));
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, gr1, gr1, 256);
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, TR::LabelSymbol::create(cg->trHeapMemory(),cg), dependencies);
   cg->stopUsingRegister(gr1);
   cg->stopUsingRegister(gr2);
   cg->stopUsingRegister(tmpReg);
   node->setRegister(retReg);
   cg->machine()->setLinkRegisterKilled(true);
   return retReg;
   }
#else
static TR::Register *dangerousGetCPU(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }
#endif

static TR::Register *inlineFixedTrg1Src1(TR::Node *node, TR::InstOpCode::Mnemonic op, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Wrong number of children in inlineFixedTrg1Src1");

   TR::Node *firstChild = node->getFirstChild();
   TR::Register *srcRegister = cg->evaluate(firstChild);
   TR::Register *targetRegister = cg->allocateRegister();

   generateTrg1Src1Instruction(cg, op, node, targetRegister, srcRegister);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }

static TR::Register *inlineAbsInt(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Node *firstChild = node->getFirstChild();
    TR::Register *resultRegister = cg->allocateRegister(), *firstRegister,*tempRegister;

    if (firstChild->getOpCode().isLoadConst())
    {
        int32_t value = firstChild->getInt();
        if (value<0) value = -value;
        loadConstant(cg, node, value, resultRegister);
    }
    else
    {
        firstRegister = cg->evaluate(firstChild);
        generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, resultRegister, firstRegister);
        if (!firstChild->isNonNegative())
        {
            tempRegister = cg->allocateRegister();
            // srawi $t1 $t0 31
            // xor   $t0 $t0 $t1
            // subf  $t0 $t1 $t0
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi, node, tempRegister, resultRegister, 31);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::XOR, node, resultRegister, resultRegister, tempRegister);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, resultRegister, tempRegister, resultRegister);
            cg->stopUsingRegister(tempRegister);
        }
    }
    node->setRegister(resultRegister);
    cg->decReferenceCount(firstChild);
    return resultRegister;
}

static TR::Register *inlineAbsLong(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Node *firstChild = node->getFirstChild();
    TR::Register *firstRegister, *tempRegister;

    if (TR::Compiler->target.is64Bit())
    {
        TR::Register *resultRegister = cg->allocateRegister();
        firstRegister = cg->evaluate(firstChild);
        generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, resultRegister, firstRegister);
        if (!firstChild->isNonNegative())
        {
            tempRegister = cg->allocateRegister();
            // sradi $t1 $t0 63
            // xor   $t0 $t0 $t1
            // subf  $t0 $t1 $t0
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sradi, node, tempRegister, resultRegister, 63);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::XOR, node, resultRegister, resultRegister, tempRegister);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, resultRegister, tempRegister, resultRegister);
            cg->stopUsingRegister(tempRegister);
        }
        node->setRegister(resultRegister);
        cg->decReferenceCount(firstChild);
        return resultRegister;
    }
    else
    {
        TR::RegisterPair *resultRegister = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());

        firstRegister = cg->evaluate(firstChild);
        generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, resultRegister->getLowOrder(), firstRegister->getLowOrder());
        generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, resultRegister->getHighOrder(), firstRegister->getHighOrder());

        if (!firstChild->isNonNegative())
        {
            tempRegister = cg->allocateRegister();
            // srawi $t1       $t0->high 31
            // xor   $t0->high $t0->high $t1
            // xor   $t0->low  $t0->low  $t1
            // subfc $t0->low  $t1       $t0->low
            // subfe $t0->high $t1       $t0->high
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi, node, tempRegister, resultRegister->getHighOrder(), 31);

            generateTrg1Src2Instruction(cg, TR::InstOpCode::XOR, node, resultRegister->getHighOrder(), resultRegister->getHighOrder(), tempRegister);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::XOR, node, resultRegister->getLowOrder(), resultRegister->getLowOrder(), tempRegister);

            generateTrg1Src2Instruction(cg, TR::InstOpCode::subfc, node, resultRegister->getLowOrder(), tempRegister, resultRegister->getLowOrder());
            generateTrg1Src2Instruction(cg, TR::InstOpCode::subfe, node, resultRegister->getHighOrder(), tempRegister, resultRegister->getHighOrder());

            cg->stopUsingRegister(tempRegister);
        }
        node->setRegister(resultRegister);
        cg->decReferenceCount(firstChild);
        return resultRegister;
    }
}

static TR::Register *inlineLongNumberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Wrong number of children in inlineLongNumberOfLeadingZeros");

   TR::Node *firstChild = node->getFirstChild();
   TR::Register *srcRegister = cg->evaluate(firstChild);
   TR::Register *targetRegister = cg->allocateRegister();
   TR::Register *tempRegister = cg->allocateRegister();
   TR::Register *maskRegister = cg->allocateRegister();

   generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, targetRegister, srcRegister->getHighOrder());
   generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, tempRegister, srcRegister->getLowOrder());
   generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, maskRegister, targetRegister, 27, 0x1);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::neg, node, maskRegister, maskRegister);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::AND, node, tempRegister, tempRegister, maskRegister);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, targetRegister, targetRegister, tempRegister);
   // An alternative to generating this right shift/neg/and sequence is:
   // mask = targetRegister << 26
   // mask = mask >> 31 (algebraic shift to replicate sign bit)
   // tempRegister &= mask
   // add targetRegister, targetRegister, tempRegister
   // Of course, there is also the alternative of:
   // cmpwi srcRegister->getHighOrder(), 0
   // bne over
   // add targetRegister, targetRegister, tempRegister
   // over:

   cg->stopUsingRegister(tempRegister);
   cg->stopUsingRegister(maskRegister);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }

static TR::Register *inlineNumberOfTrailingZeros(TR::Node *node, TR::InstOpCode::Mnemonic op, int32_t subfconst, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Wrong number of children in inlineNumberOfTrailingZeros");

   TR::Node *firstChild = node->getFirstChild();
   TR::Register *srcRegister = cg->evaluate(firstChild);
   TR::Register *targetRegister = cg->allocateRegister();

   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, targetRegister, srcRegister, -1);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::andc, node, targetRegister, targetRegister, srcRegister);
   generateTrg1Src1Instruction(cg, op, node, targetRegister, targetRegister);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::subfic, node, targetRegister, targetRegister, subfconst);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }

static TR::Register *inlineLongNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Wrong number of children in inlineLongNumberOfTrailingZeros");

   TR::Node *firstChild = node->getFirstChild();
   TR::Register *srcRegister = cg->evaluate(firstChild);
   TR::Register *targetRegister = cg->allocateRegister();
   TR::Register *tempRegister = cg->allocateRegister();
   TR::Register *maskRegister = cg->allocateRegister();

   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addic, node, tempRegister, srcRegister->getLowOrder(), -1);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::addme, node, targetRegister, srcRegister->getHighOrder());
   generateTrg1Src2Instruction(cg, TR::InstOpCode::andc, node, tempRegister, tempRegister, srcRegister->getLowOrder());
   generateTrg1Src2Instruction(cg, TR::InstOpCode::andc, node, targetRegister, targetRegister, srcRegister->getHighOrder());
   generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, targetRegister, targetRegister);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, tempRegister, tempRegister);
   generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, maskRegister, targetRegister, 27, 0x1);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::neg, node, maskRegister, maskRegister);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::AND, node, tempRegister, tempRegister, maskRegister);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, targetRegister, targetRegister, tempRegister);
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::subfic, node, targetRegister, targetRegister, 64);

   cg->stopUsingRegister(tempRegister);
   cg->stopUsingRegister(maskRegister);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }

static TR::Register *inlineIntegerHighestOneBit(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Wrong number of children in inlineIntegerHighestOneBit");

   TR::Node *firstChild = node->getFirstChild();
   TR::Register *srcRegister = cg->evaluate(firstChild);
   TR::Register *targetRegister = cg->allocateRegister();
   TR::Register *tempRegister = cg->allocateRegister();

   generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, tempRegister, srcRegister);
   generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, targetRegister, 0x8000);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::srw, node, targetRegister, targetRegister, tempRegister);

   cg->stopUsingRegister(tempRegister);

   node->setRegister(targetRegister);
   cg->decReferenceCount(firstChild);

   return targetRegister;
   }

static TR::Register *inlineLongHighestOneBit(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Wrong number of children in inlineLongHighestOneBit");

   TR::Node *firstChild = node->getFirstChild();
   TR::Register *srcRegister = cg->evaluate(firstChild);

   if (TR::Compiler->target.is64Bit())
      {
      TR::Register *targetRegister = cg->allocateRegister();
      TR::Register *tempRegister = cg->allocateRegister();

      generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzd, node, tempRegister, srcRegister);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, targetRegister, 0x8000);
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicr, node, targetRegister, targetRegister, 32, CONSTANT64(0x8000000000000000));
      generateTrg1Src2Instruction(cg, TR::InstOpCode::srd, node, targetRegister, targetRegister, tempRegister);

      cg->stopUsingRegister(tempRegister);

      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);

      return targetRegister;
      }
   else
      {
      TR::RegisterPair *targetRegister = cg->allocateRegisterPair(cg->allocateRegister(), cg->allocateRegister());
      TR::Register *tempRegister = cg->allocateRegister();
      TR::Register *condReg = cg->allocateRegister(TR_CCR);
      TR::LabelSymbol *jumpLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol *doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, srcRegister->getHighOrder(), 0);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, tempRegister, srcRegister->getHighOrder());
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, jumpLabel, condReg);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, targetRegister->getHighOrder(), 0x8000);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, targetRegister->getLowOrder(), 0);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::srw, node, targetRegister->getHighOrder(), targetRegister->getHighOrder(), tempRegister);
      generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, jumpLabel);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::cntlzw, node, tempRegister, srcRegister->getLowOrder());
      generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, targetRegister->getLowOrder(), 0x8000);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, targetRegister->getHighOrder(), 0);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::srw, node, targetRegister->getLowOrder(), targetRegister->getLowOrder(), tempRegister);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);

      cg->stopUsingRegister(tempRegister);
      cg->stopUsingRegister(condReg);

      node->setRegister(targetRegister);
      cg->decReferenceCount(firstChild);

      return targetRegister;
      }
   }

static TR::Register *inlineIsAssignableFrom(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR::Node        *receiverNode = node->getFirstChild();
   TR::Node        *parmNode = node->getSecondChild();

   // If the receiver class is known at compile-time and the receiver and parm classes aren't the same,
   // we can check if the receiver is a super class of the parm class
   // Look for:
   // icall java/lang/Class.isAssignableFrom(Ljava/lang/Class;)Z
   //   aloadi <javaLangClassFromClass>
   //      loadaddr receiverj9class
   int32_t receiverClassDepth = -1;
   if (receiverNode->getOpCodeValue() == TR::aloadi &&
       receiverNode->getSymbolReference() == comp->getSymRefTab()->findJavaLangClassFromClassSymbolRef() &&
       receiverNode->getFirstChild()->getOpCodeValue() == TR::loadaddr)
      {
      TR::Node            *receiverJ9ClassNode = receiverNode->getFirstChild();
      TR::SymbolReference *receiverJ9ClassSymRef = receiverJ9ClassNode->getSymbolReference();
      if (receiverJ9ClassSymRef && !receiverJ9ClassSymRef->isUnresolved())
         {
         TR::StaticSymbol *receiverJ9ClassSym = receiverJ9ClassSymRef->getSymbol()->getStaticSymbol();
         if (receiverJ9ClassSym)
            {
            TR_OpaqueClassBlock *receiverJ9ClassPtr = (TR_OpaqueClassBlock *)receiverJ9ClassSym->getStaticAddress();
            if (receiverJ9ClassPtr)
               {
               receiverClassDepth = (int32_t)TR::Compiler->cls.classDepthOf(receiverJ9ClassPtr);
               if (receiverClassDepth >= 0)
                  {
                  static bool disable = feGetEnv("TR_disableInlineIsAssignableFromSuperTest") != NULL;
                  if (disable)
                     receiverClassDepth = -1;
                  }
               }
            }
         }
      }

   TR_PPCScratchRegisterManager *srm = cg->generateScratchRegisterManager();
   TR::Register                *receiverReg = cg->evaluate(node->getFirstChild());
   TR::Register                *parmReg = cg->evaluate(node->getSecondChild());
   TR::Register                *resultReg = cg->allocateRegister();
   TR::LabelSymbol                *outlinedCallLabel = generateLabelSymbol(cg);
   TR::LabelSymbol                *returnTrueLabel = generateLabelSymbol(cg);
   TR::LabelSymbol                *doneLabel = generateLabelSymbol(cg);

   doneLabel->setEndInternalControlFlow();

   // We don't want this guy to be live across the call to isAssignableFrom in the outlined section
   // because the CR reg will have to be spilled/restored.
   // Instead, we allocate it outside the srm and don't bother putting it in the dependencies on doneLabel.
   // For correctness we could free all CRs in the post conditions of doneLabel, but currently CRs are
   // never live outside of the evaluator they were created in so it's not a concern.
   TR::Register                *condReg = cg->allocateRegister(TR_CCR);

   // This is fine since we don't use any scratch regs after writing to the result reg
   srm->donateScratchRegister(resultReg);

   // If either the receiver or the parm is null, bail out
   TR::Register *nullTestReg = srm->findOrCreateScratchRegister();
   if (!receiverNode->isNonNull())
      {
      genNullTest(node, receiverReg, condReg, nullTestReg, NULL, cg);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, outlinedCallLabel, condReg);
      }
   if (!parmNode->isNonNull())
      {
      genNullTest(node, parmReg, condReg, nullTestReg, NULL, cg);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, outlinedCallLabel, condReg);
      }
   srm->reclaimScratchRegister(nullTestReg);

   // Compare receiver and parm
   generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, receiverReg, parmReg);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, returnTrueLabel, condReg);

   // Compare receiver and parm classes
   TR::Register *receiverJ9ClassReg = srm->findOrCreateScratchRegister();
   TR::Register *parmJ9ClassReg = srm->findOrCreateScratchRegister();
   generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, receiverJ9ClassReg,
                              new (cg->trHeapMemory()) TR::MemoryReference(receiverReg, fej9->getOffsetOfClassFromJavaLangClassField(), TR::Compiler->om.sizeofReferenceAddress(), cg));
   generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, parmJ9ClassReg,
                              new (cg->trHeapMemory()) TR::MemoryReference(parmReg, fej9->getOffsetOfClassFromJavaLangClassField(), TR::Compiler->om.sizeofReferenceAddress(), cg));
   generateTrg1Src2Instruction(cg,TR::InstOpCode::Op_cmpl, node, condReg, receiverJ9ClassReg, parmJ9ClassReg);

   TR::Register *scratch1Reg = NULL;
   if (receiverClassDepth >= 0)
      {
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, returnTrueLabel, condReg);

      scratch1Reg = srm->findOrCreateScratchRegister();
      TR::Register *scratch2Reg = srm->findOrCreateScratchRegister();

      genTestIsSuper(node, parmJ9ClassReg, receiverJ9ClassReg, condReg, scratch1Reg, scratch2Reg, receiverClassDepth, outlinedCallLabel, NULL, cg);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, outlinedCallLabel, condReg);

      srm->reclaimScratchRegister(scratch1Reg);
      srm->reclaimScratchRegister(scratch2Reg);
      }
   else
      {
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, outlinedCallLabel, condReg);
      }

   srm->reclaimScratchRegister(receiverJ9ClassReg);
   srm->reclaimScratchRegister(parmJ9ClassReg);
   generateLabelInstruction(cg, TR::InstOpCode::label, node, returnTrueLabel);
   generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, resultReg, 1);

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2 + srm->numAvailableRegisters(), cg->trMemory());
   deps->addPostCondition(receiverReg, TR::RealRegister::NoReg, UsesDependentRegister | ExcludeGPR0InAssigner);
   deps->addPostCondition(parmReg, TR::RealRegister::NoReg, UsesDependentRegister | ExcludeGPR0InAssigner);
   srm->addScratchRegistersToDependencyList(deps);
   // Make sure these two (added to the deps by the srm) have !gr0, since we use them as base regs
   deps->setPostDependencyExcludeGPR0(receiverJ9ClassReg);
   if (scratch1Reg)
      deps->setPostDependencyExcludeGPR0(scratch1Reg);
   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, deps);

   TR_PPCOutOfLineCodeSection *outlinedHelperCall = new (cg->trHeapMemory()) TR_PPCOutOfLineCodeSection(node, TR::icall, resultReg, outlinedCallLabel, doneLabel, cg);
   cg->getPPCOutOfLineCodeSectionList().push_front(outlinedHelperCall);

   node->setRegister(resultReg);
   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   srm->stopUsingRegisters();
   cg->stopUsingRegister(condReg);

   return resultReg;
   }

static TR::Register *inlineStringHashcode(TR::Node *node, TR::CodeGenerator *cg)
{
    TR::Compilation *comp = cg->comp();
    TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
    bool isLE = TR::Compiler->target.cpu.isLittleEndian();

    TR::Node *valueNode = node->getFirstChild();
    TR::Node *offsetNode = node->getSecondChild();
    TR::Node *countNode = node->getThirdChild();

    TR::Register *valueReg = cg->gprClobberEvaluate(valueNode);
    TR::Register *endReg = cg->gprClobberEvaluate(offsetNode);
    TR::Register *vendReg = cg->gprClobberEvaluate(countNode);
    TR::Register *hashReg = cg->allocateRegister();
    TR::Register *tempReg = cg->allocateRegister();
    TR::Register *constant0Reg = cg->allocateRegister();
    TR::Register *multiplierAddrReg = cg->allocateRegister();
    TR::Register *condReg = cg->allocateRegister(TR_CCR);

    TR::Register *multiplierReg = cg->allocateRegister(TR_VRF);
    TR::Register *high4Reg = cg->allocateRegister(TR_VRF);
    TR::Register *low4Reg = cg->allocateRegister(TR_VRF);
    TR::Register *vtmp1Reg = cg->allocateRegister(TR_VRF);
    TR::Register *vtmp2Reg = cg->allocateRegister(TR_VRF);
    TR::Register *vconstant0Reg = cg->allocateRegister(TR_VRF);
    TR::Register *vconstantNegReg = cg->allocateRegister(TR_VRF);
    TR::Register *vunpackMaskReg = cg->allocateRegister(TR_VRF);

    TR::LabelSymbol *serialLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *VSXLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *POSTVSXLabel = generateLabelSymbol(cg);
    TR::LabelSymbol *endLabel = generateLabelSymbol(cg);

    // Skip header of the array
    // v = v + offset<<1
    // end = v + count<<1
    // hash = 0
    // temp = 0
    intptrj_t hdrSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
    generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, valueReg, valueReg, hdrSize);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, endReg, endReg, endReg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, valueReg, valueReg, endReg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, vendReg, vendReg, vendReg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, endReg, valueReg, vendReg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::XOR, node, hashReg, hashReg, hashReg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::XOR, node, tempReg, tempReg, tempReg);
    loadConstant(cg, node, 0x0, constant0Reg);

    // if count<<1 < 16 goto serial
    generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, vendReg, 0x10);
    generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, serialLabel, condReg);

    // load multiplier, clear high4 low4
    static uint32_t multiplierVectors_be[12] = {0x94446F01, 0x94446F01, 0x94446F01, 0x94446F01, 0x67E12CDF, 887503681, 28629151, 923521, 29791, 961, 31, 1};
    static uint32_t multiplierVectors_le[12] = {0x94446F01, 0x94446F01, 0x94446F01, 0x94446F01, 1, 31, 961, 29791, 923521, 28629151, 887503681, 0x67E12CDF};

    if (isLE){
        loadAddressConstant(cg, node, (intptrj_t)multiplierVectors_le, multiplierAddrReg);
    }else{
        loadAddressConstant(cg, node, (intptrj_t)multiplierVectors_be, multiplierAddrReg);
    }
    generateTrg1MemInstruction(cg, TR::InstOpCode::lxvw4x, node, multiplierReg, new (cg->trHeapMemory()) TR::MemoryReference(multiplierAddrReg, constant0Reg, 16, cg));
    generateTrg1Src2Instruction(cg, TR::InstOpCode::vxor, node, high4Reg, high4Reg, high4Reg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::vxor, node, low4Reg, low4Reg, low4Reg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::vxor, node, vconstant0Reg, vconstant0Reg, vconstant0Reg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::vnor, node, vconstantNegReg, vconstant0Reg, vconstant0Reg);
    generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::vsldoi, node, vunpackMaskReg, vconstant0Reg, vconstantNegReg, 2);
    generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::vspltw, node, vunpackMaskReg, vunpackMaskReg, 3);

    // vend = end & (~0xf)
    // if v is 16byte aligned goto VSX_LOOP
    generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, tempReg, 0xFFFFFFFFFFFFFFF0);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::AND, node, vendReg, endReg, tempReg);
    loadConstant(cg, node, 0xF, tempReg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::AND, node, tempReg, valueReg, tempReg);
    generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, tempReg, 0x0);
    generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, VSXLabel, condReg);

    // The reason we don't do VSX loop directly is we want to avoid loading unaligned data and deal it with vperm.
    // Instead, we load the first unaligned part, let VSX handle the rest aligned part.
    // load unaligned v, mask out unwanted part
    // for example, if value = 0x12345, we mask out 0x12340~0x12344, keep 0x12345~0x1234F
    generateTrg1MemInstruction(cg, TR::InstOpCode::lvx, node, vtmp1Reg, new (cg->trHeapMemory()) TR::MemoryReference(valueReg, constant0Reg, 16, cg));
    loadConstant(cg, node, 0xF, tempReg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::AND, node, tempReg, valueReg, tempReg);
    generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tempReg, tempReg, 3, 0xFFFFFFFFFFFFFFF8);
    generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrd, node, vtmp2Reg, tempReg);
    generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::vsldoi, node, vtmp2Reg, vconstant0Reg, vtmp2Reg, 8);
    if (isLE) {
        generateTrg1Src2Instruction(cg, TR::InstOpCode::vslo, node, vtmp2Reg, vconstantNegReg, vtmp2Reg);
    } else {
        generateTrg1Src2Instruction(cg, TR::InstOpCode::vsro, node, vtmp2Reg, vconstantNegReg, vtmp2Reg);
    }
    generateTrg1Src2Instruction(cg, TR::InstOpCode::vand, node, vtmp1Reg, vtmp1Reg, vtmp2Reg);

    // unpack masked v to high4 low4
    generateTrg1Src1Instruction(cg, TR::InstOpCode::vupkhsh, node, high4Reg, vtmp1Reg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::vand, node, high4Reg, high4Reg, vunpackMaskReg);
    generateTrg1Src1Instruction(cg, TR::InstOpCode::vupklsh, node, low4Reg, vtmp1Reg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::vand, node, low4Reg, low4Reg, vunpackMaskReg);

    // advance v to next aligned pointer
    // if v >= vend goto POST_VSX
    generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, valueReg, valueReg, 0xF);
    generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, tempReg, 0xFFFFFFFFFFFFFFF0);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::AND, node, valueReg, valueReg, tempReg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp8, node, condReg, valueReg, vendReg);
    generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, node, POSTVSXLabel, condReg);

    //VSX_LOOP:
    //load v (here v must be aligned)
    //unpack v to temphigh4 templow4
    //high4 = high4 * multiplier
    //low4 = low4 * multiplier
    //high4 = high4 + temphigh4
    //low4 = low4 + templow4
    //v = v + 0x10
    //if v < vend goto VSX_LOOP
    generateLabelInstruction(cg, TR::InstOpCode::label, node, VSXLabel);
    generateTrg1MemInstruction(cg, TR::InstOpCode::lvx, node, vtmp1Reg, new (cg->trHeapMemory()) TR::MemoryReference(valueReg, constant0Reg, 16, cg));
    generateTrg1Src1Instruction(cg, TR::InstOpCode::vupkhsh, node, vtmp2Reg, vtmp1Reg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::vand, node, vtmp2Reg, vtmp2Reg, vunpackMaskReg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::vmuluwm, node, high4Reg, high4Reg, multiplierReg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::vadduwm, node, high4Reg, high4Reg, vtmp2Reg);
    generateTrg1Src1Instruction(cg, TR::InstOpCode::vupklsh, node, vtmp2Reg, vtmp1Reg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::vand, node, vtmp2Reg, vtmp2Reg, vunpackMaskReg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::vmuluwm, node, low4Reg, low4Reg, multiplierReg);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::vadduwm, node, low4Reg, low4Reg, vtmp2Reg);
    generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, valueReg, valueReg, 0x10);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp8, node, condReg, valueReg, vendReg);
    generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, VSXLabel, condReg);

    // POST_VSX:
    // shift and sum low4 and high4
    // move result to hashReg
    generateLabelInstruction(cg, TR::InstOpCode::label, node, POSTVSXLabel);

    generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, multiplierAddrReg, multiplierAddrReg, 0x10);
    generateTrg1MemInstruction(cg, TR::InstOpCode::lxvw4x, node, multiplierReg, new (cg->trHeapMemory()) TR::MemoryReference(multiplierAddrReg, constant0Reg, 16, cg));
    generateTrg1Src2Instruction(cg, TR::InstOpCode::vmuluwm, node, high4Reg, high4Reg, multiplierReg);
    generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, multiplierAddrReg, multiplierAddrReg, 0x10);
    generateTrg1MemInstruction(cg, TR::InstOpCode::lxvw4x, node, multiplierReg, new (cg->trHeapMemory()) TR::MemoryReference(multiplierAddrReg, constant0Reg, 16, cg));
    generateTrg1Src2Instruction(cg, TR::InstOpCode::vmuluwm, node, low4Reg, low4Reg, multiplierReg);

    generateTrg1Src2Instruction(cg, TR::InstOpCode::vadduwm, node, high4Reg, high4Reg, low4Reg);
    generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::vsldoi, node, vtmp1Reg, vconstant0Reg, high4Reg, 8);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::vadduwm, node, high4Reg, high4Reg, vtmp1Reg);
    generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::vsldoi, node, vtmp1Reg, vconstant0Reg, high4Reg, 12);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::vadduwm, node, high4Reg, high4Reg, vtmp1Reg);

    generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::vsldoi, node, vtmp1Reg, high4Reg, vconstant0Reg, 8);
    generateTrg1Src1Instruction(cg, TR::InstOpCode::mfvsrwz, node, hashReg, vtmp1Reg);

    // Head of the serial loop
    generateLabelInstruction(cg, TR::InstOpCode::label, node, serialLabel);

    // temp = hash
    // hash = hash << 5
    // hash = hash - temp
    // temp = v[i]
    // hash = hash + temp

    generateTrg1Src2Instruction(cg, TR::InstOpCode::cmp8, node, condReg, valueReg, endReg);
    generateConditionalBranchInstruction(cg, TR::InstOpCode::bge, node, endLabel, condReg);
    generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, tempReg, hashReg);
    generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, hashReg, hashReg, 5, 0xFFFFFFFFFFFFFFE0);
    generateTrg1Src2Instruction(cg, TR::InstOpCode::subf, node, hashReg, tempReg, hashReg);
    generateTrg1MemInstruction(cg, TR::InstOpCode::lhzx, node, tempReg, new (cg->trHeapMemory()) TR::MemoryReference(valueReg, constant0Reg, 2, cg));
    generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, hashReg, hashReg, tempReg);
    generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, valueReg, valueReg, 0x2);
    generateLabelInstruction(cg, TR::InstOpCode::b, node, serialLabel);

    // End of this method
    TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 16, cg->trMemory());
    dependencies->addPostCondition(valueReg, TR::RealRegister::NoReg);
    dependencies->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0(); // valueReg

    dependencies->addPostCondition(endReg, TR::RealRegister::NoReg);
    dependencies->addPostCondition(vendReg, TR::RealRegister::NoReg);
    dependencies->addPostCondition(hashReg, TR::RealRegister::NoReg);
    dependencies->addPostCondition(tempReg, TR::RealRegister::NoReg);
    dependencies->addPostCondition(constant0Reg, TR::RealRegister::NoReg);
    dependencies->addPostCondition(condReg, TR::RealRegister::NoReg);

    dependencies->addPostCondition(multiplierAddrReg, TR::RealRegister::NoReg);
    dependencies->getPostConditions()->getRegisterDependency(7)->setExcludeGPR0(); // multiplierAddrReg

    dependencies->addPostCondition(multiplierReg, TR::RealRegister::NoReg);
    dependencies->addPostCondition(high4Reg, TR::RealRegister::NoReg);
    dependencies->addPostCondition(low4Reg, TR::RealRegister::NoReg);
    dependencies->addPostCondition(vtmp1Reg, TR::RealRegister::NoReg);
    dependencies->addPostCondition(vtmp2Reg, TR::RealRegister::NoReg);
    dependencies->addPostCondition(vconstant0Reg, TR::RealRegister::NoReg);
    dependencies->addPostCondition(vconstantNegReg, TR::RealRegister::NoReg);
    dependencies->addPostCondition(vunpackMaskReg, TR::RealRegister::NoReg);

    generateDepLabelInstruction(cg, TR::InstOpCode::label, node, endLabel, dependencies);

    node->setRegister(hashReg);
    cg->decReferenceCount(valueNode);
    cg->decReferenceCount(offsetNode);
    cg->decReferenceCount(countNode);

    cg->stopUsingRegister(valueReg);
    cg->stopUsingRegister(endReg);
    cg->stopUsingRegister(vendReg);
    cg->stopUsingRegister(tempReg);
    cg->stopUsingRegister(constant0Reg);
    cg->stopUsingRegister(condReg);
    cg->stopUsingRegister(multiplierAddrReg);

    cg->stopUsingRegister(multiplierReg);
    cg->stopUsingRegister(high4Reg);
    cg->stopUsingRegister(low4Reg);
    cg->stopUsingRegister(vtmp1Reg);
    cg->stopUsingRegister(vtmp2Reg);
    cg->stopUsingRegister(vconstant0Reg);
    cg->stopUsingRegister(vconstantNegReg);
    cg->stopUsingRegister(vunpackMaskReg);
    return hashReg;
}

static TR::Register *inlineEncodeUTF16(TR::Node *node, TR::CodeGenerator *cg)
   {
   // tree looks like:
   // icall com.ibm.jit.JITHelpers.encodeUtf16{Big,Little}()
   // input ptr
   // output ptr
   // input length (in elements)
   // Number of elements converted returned

   TR::MethodSymbol *symbol = node->getSymbol()->castToMethodSymbol();
   bool bigEndian = symbol->getRecognizedMethod() == TR::com_ibm_jit_JITHelpers_transformedEncodeUTF16Big;

   // Set up register dependencies
   const int gprClobberCount = 5;
   const int fprClobberCount = 4;
   const int vrClobberCount = 6;
   const int crClobberCount = 2;
   const int totalDeps = crClobberCount + gprClobberCount + fprClobberCount + vrClobberCount + 3;
   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, totalDeps, cg->trMemory());

   TR::Register *inputReg = cg->gprClobberEvaluate(node->getChild(0));
   TR::Register *outputReg = cg->gprClobberEvaluate(node->getChild(1));
   TR::Register *inputLenReg = cg->gprClobberEvaluate(node->getChild(2));
   TR::Register *outputLenReg = cg->allocateRegister();

   // Allocate clobbered registers
   TR::Register *gprClobbers[gprClobberCount], *fprClobbers[fprClobberCount], *vrClobbers[vrClobberCount], *crClobbers[crClobberCount];
   for (int i = 0; i < gprClobberCount; ++i) gprClobbers[i] = cg->allocateRegister(TR_GPR);
   for (int i = 0; i < fprClobberCount; ++i) fprClobbers[i] = cg->allocateRegister(TR_FPR);
   for (int i = 0; i < vrClobberCount; ++i) vrClobbers[i] = cg->allocateRegister(TR_VRF);
   for (int i = 0; i < crClobberCount; ++i) crClobbers[i] = cg->allocateRegister(TR_CCR);

   // Add the pre and post conditions
   // Input and output registers
   deps->addPreCondition(inputReg, TR::RealRegister::gr3);

   deps->addPostCondition(outputLenReg, TR::RealRegister::gr3);
   deps->addPostCondition(outputReg, TR::RealRegister::gr4);
   deps->addPostCondition(inputLenReg, TR::RealRegister::gr5);

   //CCR.
   deps->addPostCondition(crClobbers[0], TR::RealRegister::cr0);
   deps->addPostCondition(crClobbers[1], TR::RealRegister::cr6);

   //GPRs + Trampoline
   deps->addPostCondition(gprClobbers[0], TR::RealRegister::gr6);
   deps->addPostCondition(gprClobbers[1], TR::RealRegister::gr7);
   deps->addPostCondition(gprClobbers[2], TR::RealRegister::gr8);
   deps->addPostCondition(gprClobbers[3], TR::RealRegister::gr9);
   deps->addPostCondition(gprClobbers[4], TR::RealRegister::gr11);

   //VR's
   deps->addPostCondition(vrClobbers[0], TR::RealRegister::vr0);
   deps->addPostCondition(vrClobbers[1], TR::RealRegister::vr1);
   deps->addPostCondition(vrClobbers[2], TR::RealRegister::vr2);
   deps->addPostCondition(vrClobbers[3], TR::RealRegister::vr3);
   deps->addPostCondition(vrClobbers[4], TR::RealRegister::vr4);
   deps->addPostCondition(vrClobbers[5], TR::RealRegister::vr5);

   //FP/VSR
   deps->addPostCondition(fprClobbers[0], TR::RealRegister::fp0);
   deps->addPostCondition(fprClobbers[1], TR::RealRegister::fp1);
   deps->addPostCondition(fprClobbers[2], TR::RealRegister::fp2);
   deps->addPostCondition(fprClobbers[3], TR::RealRegister::fp3);

   // Generate helper call
   TR_RuntimeHelper helper;
   helper = bigEndian ? TR_PPCencodeUTF16Big : TR_PPCencodeUTF16Little;
   TR::SymbolReference *helperSym = cg->comp()->getSymRefTab()->findOrCreateRuntimeHelper(helper, false, false, false);
   generateDepImmSymInstruction(cg, TR::InstOpCode::bl, node, (uintptrj_t)helperSym->getMethodAddress(), deps, helperSym);

   for (uint32_t i = 0; i < node->getNumChildren(); ++i) cg->decReferenceCount(node->getChild(i));

   // Spill the clobbered registers
   if (inputReg != node->getChild(0)->getRegister()) cg->stopUsingRegister(inputReg);
   if (outputReg != node->getChild(1)->getRegister()) cg->stopUsingRegister(outputReg);
   if (inputLenReg != node->getChild(2)->getRegister()) cg->stopUsingRegister(inputLenReg);
   for (int i = 0; i < gprClobberCount; ++i) cg->stopUsingRegister(gprClobbers[i]);
   for (int i = 0; i < vrClobberCount; ++i) cg->stopUsingRegister(vrClobbers[i]);
   for (int i = 0; i < fprClobberCount; ++i) cg->stopUsingRegister(fprClobbers[i]);
   for (int i = 0; i < crClobberCount; ++i) cg->stopUsingRegister(crClobbers[i]);

   cg->machine()->setLinkRegisterKilled(true);
   cg->setHasCall();
   node->setRegister(outputLenReg);

   return outputLenReg;
   }

extern TR::Register *inlineLongRotateLeft(TR::Node *node, TR::CodeGenerator *cg);
extern TR::Register *inlineIntegerRotateLeft(TR::Node *node, TR::CodeGenerator *cg);
extern TR::Register *inlineBigDecimalConstructor(TR::Node *node, TR::CodeGenerator *cg, bool isLong, bool exp);
extern TR::Register *inlineBigDecimalBinaryOp(TR::Node * node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, bool scaled);
extern TR::Register *inlineBigDecimalDivide(TR::Node * node, TR::CodeGenerator *cg);
extern TR::Register *inlineBigDecimalRound(TR::Node * node, TR::CodeGenerator *cg);
extern TR::Register *inlineBigDecimalCompareTo(TR::Node * node, TR::CodeGenerator * cg);
extern TR::Register *inlineBigDecimalUnaryOp(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic op, bool precision);
extern TR::Register *inlineBigDecimalSetScale(TR::Node * node, TR::CodeGenerator * cg);
extern TR::Register *inlineBigDecimalUnscaledValue(TR::Node * node, TR::CodeGenerator * cg);

bool
J9::Power::CodeGenerator::inlineDirectCall(TR::Node *node, TR::Register *&resultReg)
   {
   TR::CodeGenerator *cg = self();
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->comp()->fe());
   TR::MethodSymbol * methodSymbol = node->getSymbol()->getMethodSymbol();

   static bool disableDCAS = (feGetEnv("TR_DisablePPCDCAS") != NULL);
   static bool useJapaneseCompression = (feGetEnv("TR_JapaneseComp") != NULL);

   if (comp->getSymRefTab()->isNonHelper(node->getSymbolReference(), TR::SymbolReferenceTable::singlePrecisionSQRTSymbol))
      {
      resultReg = inlineSinglePrecisionFP(node, TR::InstOpCode::fsqrts, cg);
      return true;
      }
   else if (methodSymbol)
      {
      switch (methodSymbol->getRecognizedMethod())
         {
      case TR::java_util_concurrent_ConcurrentHashMap_tmPut:
         if (cg->getSupportsTM())
            {
            resultReg = inlineConcurrentHashMapTmPut(node, cg);
            return true;
            }
         break;
      case TR::java_util_concurrent_ConcurrentHashMap_tmRemove:
         if (cg->getSupportsTM())
            {
            return inlineConcurrentHashMapTmRemove(node, cg);
            }
         break;
      case TR::java_util_concurrent_ConcurrentLinkedQueue_tmOffer:
         {
         if (cg->getSupportsTM())
            {
            resultReg = inlineConcurrentLinkedQueueTMOffer(node, cg);
            return true;
            }
         break;
         }

      case TR::java_util_concurrent_ConcurrentLinkedQueue_tmPoll:
         {
         if (cg->getSupportsTM())
            {
            resultReg = inlineConcurrentLinkedQueueTMPoll(node, cg);
            return true;
            }
         break;
         }

      case TR::java_util_concurrent_atomic_AtomicMarkableReference_tmDoubleWordCAS:
      case TR::java_util_concurrent_atomic_AtomicMarkableReference_tmDoubleWordSet:
      case TR::java_util_concurrent_atomic_AtomicStampedReference_tmDoubleWordCAS:
      case TR::java_util_concurrent_atomic_AtomicStampedReference_tmDoubleWordSet:
         if (cg->getSupportsTM() && cg->getSupportsTMDoubleWordCASORSet())
            {
            resultReg = inlineTMDoubleWordCASOrSet(node, cg, methodSymbol);
            return true;
            }
         break;

      case TR::sun_misc_Unsafe_copyMemory:
         if (comp->canTransformUnsafeCopyToArrayCopy() &&
             performTransformation(comp,
               "O^O Call arraycopy instead of Unsafe.copyMemory: %s\n", cg->getDebug()->getName(node)))
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
         break;

      case TR::sun_misc_Unsafe_setMemory:
         if (comp->canTransformUnsafeSetMemory())
            {
            TR::Node *dest = node->getChild(1);
            TR::Node *destOffset = node->getChild(2);
            TR::Node *len = node->getChild(3);
            TR::Node *byteValue = node->getChild(4);
            dest = TR::Node::create(TR::aladd, 2, dest, destOffset);

            TR::Node * copyMemNode = TR::Node::createWithSymRef(TR::arrayset, 3, 3, dest, len, byteValue, node->getSymbolReference());
            copyMemNode->setByteCodeInfo(node->getByteCodeInfo());

            TR::TreeEvaluator::setmemoryEvaluator(copyMemNode,cg);

            if (node->getChild(0)->getRegister())
               cg->decReferenceCount(node->getChild(0));
            else
               node->getChild(0)->recursivelyDecReferenceCount();

            cg->decReferenceCount(node->getChild(1));
            cg->decReferenceCount(node->getChild(2));
            cg->decReferenceCount(node->getChild(3));
            cg->decReferenceCount(node->getChild(4));
            return true;
            }
         break;

      case TR::java_lang_String_compress:
         resultReg = compressStringEvaluator(node, cg, useJapaneseCompression);
         return true;
         break;

      case TR::java_lang_String_compressNoCheck:
         resultReg = compressStringNoCheckEvaluator(node, cg, useJapaneseCompression);
         return true;

      case TR::java_lang_String_andOR:
         resultReg = andORStringEvaluator(node, cg);
         return true;
         break;

      case TR::java_util_concurrent_atomic_AtomicBoolean_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicInteger_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicInteger_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicInteger_incrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicInteger_decrementAndGet:
         {
         resultReg = inlineAtomicOps(node, cg, 4, methodSymbol, false);
         return true;
         break;
         }

      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndAdd:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndIncrement:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndDecrement:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_getAndSet:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_addAndGet:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_incrementAndGet:
      case TR::java_util_concurrent_atomic_AtomicIntegerArray_decrementAndGet:
         {
         resultReg = inlineAtomicOps(node, cg, 4, methodSymbol, true);
         return true;
         break;
         }

      case TR::java_lang_Math_sqrt:
      case TR::java_lang_StrictMath_sqrt:
         if (methodSymbol->getResolvedMethodSymbol()->canReplaceWithHWInstr())
            {
            resultReg = inlineDoublePrecisionFP(node, TR::InstOpCode::fsqrt, cg);
            return true;
            }
         break;

      case TR::java_lang_Math_ceil:
      case TR::java_lang_StrictMath_ceil:
         if (methodSymbol->getResolvedMethodSymbol()->canReplaceWithHWInstr())
            {
            resultReg = inlineDoublePrecisionFP(node, TR::InstOpCode::frip, cg);
            return true;
            }
         break;

      case TR::java_lang_Math_floor:
      case TR::java_lang_StrictMath_floor:
         if (methodSymbol->getResolvedMethodSymbol()->canReplaceWithHWInstr())
            {
            resultReg = inlineDoublePrecisionFP(node, TR::InstOpCode::frim, cg);
            return true;
            }
         break;

      case TR::java_lang_Math_copySign_F:
         // StrictMath copySign spec not guaranteed by fcpsgn instruction
         if (methodSymbol->getResolvedMethodSymbol()->canReplaceWithHWInstr())
            {
            resultReg = inlineSinglePrecisionFPTrg1Src2(node, TR::InstOpCode::fcpsgn, cg);
            return true;
            }
         break;

      case TR::java_lang_Math_copySign_D:
         // StrictMath copySign spec not guaranteed by fcpsgn instruction
         if (methodSymbol->getResolvedMethodSymbol()->canReplaceWithHWInstr())
            {
            resultReg = inlineDoublePrecisionFPTrg1Src2(node, TR::InstOpCode::fcpsgn, cg);
            return true;
            }
         break;

      case TR::java_lang_Math_abs_F:
         resultReg = inlineSinglePrecisionFP(node, TR::InstOpCode::fabs, cg);
         return true;

      case TR::java_lang_Math_abs_D:
         resultReg = inlineDoublePrecisionFP(node, TR::InstOpCode::fabs, cg);
         return true;

      case TR::java_lang_Math_abs_I:
         resultReg = inlineAbsInt(node,cg);
         return true;

      case TR::java_lang_Math_abs_L:
         resultReg = inlineAbsLong(node,cg);
         return true;

      case TR::java_lang_Integer_numberOfLeadingZeros:
         resultReg = inlineFixedTrg1Src1(node, TR::InstOpCode::cntlzw, cg);
         return true;

      case TR::java_lang_Long_numberOfLeadingZeros:
         if (TR::Compiler->target.is64Bit())
            {
            resultReg = inlineFixedTrg1Src1(node, TR::InstOpCode::cntlzd, cg);
            return true;
            }
         else
            {
            resultReg = inlineLongNumberOfLeadingZeros(node, cg);
            return true;
            }

      case TR::java_lang_Integer_numberOfTrailingZeros:
         {
         resultReg = inlineNumberOfTrailingZeros(node, TR::InstOpCode::cntlzw, 32, cg);
         return true;
         }

      case TR::java_lang_Long_numberOfTrailingZeros:
         if (TR::Compiler->target.is64Bit())
            {
            resultReg = inlineNumberOfTrailingZeros(node, TR::InstOpCode::cntlzd, 64, cg);
            return true;
            }
         else
            {
            resultReg = inlineLongNumberOfTrailingZeros(node, cg);
            return true;
            }

      case TR::java_lang_Integer_highestOneBit:
         resultReg = inlineIntegerHighestOneBit(node, cg);
         return true;

      case TR::java_lang_Long_highestOneBit:
         resultReg = inlineLongHighestOneBit(node, cg);
         return true;

      case TR::java_lang_Integer_rotateLeft:
         resultReg = inlineIntegerRotateLeft(node, cg);
         return true;

      case TR::java_lang_Long_rotateLeft:
         if (TR::Compiler->target.is64Bit())
            {
            resultReg = inlineLongRotateLeft(node, cg);
            return true;
            }
         break;

      case TR::java_lang_Integer_rotateRight:
         resultReg = inlineIntegerRotateRight(node, cg);
         return true;

      case TR::java_lang_Long_rotateRight:
         if (TR::Compiler->target.is64Bit())
            {
            resultReg = inlineLongRotateRight(node, cg);
            return true;
            }
         break;

      case TR::java_lang_Short_reverseBytes:
         resultReg = inlineShortReverseBytes(node, cg);
         return true;

      case TR::java_lang_Integer_reverseBytes:
         resultReg = inlineIntegerReverseBytes(node, cg);
         return true;

      case TR::java_lang_Long_reverseBytes:
         resultReg = inlineLongReverseBytes(node, cg);
         return true;

      case TR::java_lang_String_hashCodeImplDecompressed:
         if (TR::Compiler->target.cpu.id() >= TR_PPCp8 && TR::Compiler->target.cpu.getPPCSupportsVSX() && !cg->comp()->compileRelocatableCode())
            {
            resultReg = inlineStringHashcode(node, cg);
            return true;
            }
         break;

      case TR::com_ibm_jit_JITHelpers_transformedEncodeUTF16Big:
      case TR::com_ibm_jit_JITHelpers_transformedEncodeUTF16Little:
         if (TR::Compiler->target.cpu.id() >= TR_PPCp7 && TR::Compiler->target.cpu.getPPCSupportsVSX())
            {
            resultReg = inlineEncodeUTF16(node, cg);
            return true;
            }
         break;

      case TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z:
        // In Java9 this can be either the jdk.internal JNI method or the sun.misc Java wrapper.
        // In Java8 it will be sun.misc which will contain the JNI directly.
        // We only want to inline the JNI methods, so add an explicit test for isNative().
        if (!methodSymbol->isNative())
           break;

        if ((node->isUnsafeGetPutCASCallOnNonArray() || !TR::Compiler->om.canGenerateArraylets()) && node->isSafeForCGToFastPathUnsafeCall())
            {
            resultReg = VMinlineCompareAndSwap(node, cg, false);
            return true;
            }
         break;

      case TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z:
         traceMsg(comp, "In evaluator for compareAndSwapLong. node = %p node->isSafeForCGToFastPathUnsafeCall = %p\n", node, node->isSafeForCGToFastPathUnsafeCall());
         // As above, we only want to inline the JNI methods, so add an explicit test for isNative()
         if (!methodSymbol->isNative())
            break;

         if (TR::Compiler->target.is64Bit() && (node->isUnsafeGetPutCASCallOnNonArray() || !TR::Compiler->om.canGenerateArraylets()) && node->isSafeForCGToFastPathUnsafeCall())
            {
            resultReg = VMinlineCompareAndSwap(node, cg, true);
            return true;
            }
         else if ((node->isUnsafeGetPutCASCallOnNonArray() || !TR::Compiler->om.canGenerateArraylets()) && node->isSafeForCGToFastPathUnsafeCall())
            {
            resultReg = inlineAtomicOperation(node, cg, methodSymbol);
            return true;
            }
         break;

      case TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z:
         // As above, we only want to inline the JNI methods, so add an explicit test for isNative()
         if (!methodSymbol->isNative())
            break;

         if ((node->isUnsafeGetPutCASCallOnNonArray() || !TR::Compiler->om.canGenerateArraylets()) && node->isSafeForCGToFastPathUnsafeCall())
            {
            resultReg = VMinlineCompareAndSwapObject(node, cg);
            return true;
            }
         break;

      case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordCASSupported:
      case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordSetSupported:
      case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordCASSupported:
      case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordSetSupported:
         if (!disableDCAS && TR::Compiler->target.is64Bit() && comp->useCompressedPointers())
            {
            resultReg = inlineDoubleWordCASOrSetSupported(node, cg);
            return true;
            }
         break;

      case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordCAS:
      case TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordSet:
      case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordCAS:
      case TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordSet:
         if (!disableDCAS && TR::Compiler->target.is64Bit() && comp->useCompressedPointers())
            {
            resultReg = inlineDoubleWordCAS(node, cg, methodSymbol);
            return true;
            }
         break;

      case TR::java_nio_Bits_keepAlive:
      case TR::java_lang_ref_Reference_reachabilityFence:
         {
         TR::Node *paramNode = node->getFirstChild();
         TR::Register *paramReg = cg->evaluate(paramNode);
         TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
         addDependency(conditions, paramReg, TR::RealRegister::NoReg, TR_GPR, cg);
         TR::LabelSymbol *label = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         generateDepLabelInstruction(cg, TR::InstOpCode::label, node, label, conditions);
         cg->decReferenceCount(paramNode);
         resultReg = NULL;
         return true;
         }

      case TR::x10JITHelpers_getCPU:
         break;

      case TR::java_util_concurrent_atomic_Fences_reachabilityFence:
         {
         cg->decReferenceCount(node->getChild(0));
         resultReg = NULL;
         return true;
         }

      case TR::java_util_concurrent_atomic_Fences_orderReads:
         if (performTransformation(comp, "O^O PPC Evaluator: Replacing read/read Fence with an lwsync [%p].\n", node))
            {
            // mark as seen and then just don't bother generating instructions
            TR::Node *callNode = node;
            int32_t numArgs = callNode->getNumChildren();
            for (int32_t i = numArgs - 1; i >= 0; i--)
               cg->decReferenceCount(callNode->getChild(i));
            cg->decReferenceCount(callNode);
            generateInstruction(cg, TR::InstOpCode::isync, node);
            resultReg = NULL;
            return true;
            }
         break;

         // for now all preStores simply emit lwsync  See JIT design 1598
      case TR::java_util_concurrent_atomic_Fences_preStoreFence:
      case TR::java_util_concurrent_atomic_Fences_preStoreFence_jlObject:
      case TR::java_util_concurrent_atomic_Fences_preStoreFence_jlObjectI:
      case TR::java_util_concurrent_atomic_Fences_preStoreFence_jlObjectjlrField:
      case TR::java_util_concurrent_atomic_Fences_postLoadFence:
      case TR::java_util_concurrent_atomic_Fences_postLoadFence_jlObjectjlrField:
      case TR::java_util_concurrent_atomic_Fences_postLoadFence_jlObject:
      case TR::java_util_concurrent_atomic_Fences_postLoadFence_jlObjectI:
      case TR::java_util_concurrent_atomic_Fences_orderWrites:
         if (performTransformation(comp, "O^O PPC Evaluator: Replacing store/store Fence with an lwsync [%p].\n", node))
            {
            // mark as seen and then just don't bother generating instructions
            TR::Node *callNode = node;
            int32_t numArgs = callNode->getNumChildren();
            for (int32_t i = numArgs - 1; i >= 0; i--)
               cg->decReferenceCount(callNode->getChild(i));
            cg->decReferenceCount(callNode);
            generateInstruction(cg, TR::InstOpCode::lwsync, node);
            resultReg = NULL;
            return true;
            }
         break;

         // for now just emit a sync.  See design 1598
      case TR::java_util_concurrent_atomic_Fences_postStorePreLoadFence_jlObject:
      case TR::java_util_concurrent_atomic_Fences_postStorePreLoadFence_jlObjectjlrField:
      case TR::java_util_concurrent_atomic_Fences_postStorePreLoadFence:
      case TR::java_util_concurrent_atomic_Fences_postStorePreLoadFence_jlObjectI:
      case TR::java_util_concurrent_atomic_Fences_orderAccesses:
         if (performTransformation(comp, "O^O PPC Evaluator: Replacing store/load Fence with a sync [%p].\n", node))
            {
            TR::Node *callNode = node;
            int32_t numArgs = callNode->getNumChildren();
            for (int32_t i = numArgs - 1; i >= 0; i--)
               cg->decReferenceCount(callNode->getChild(i));
            cg->decReferenceCount(callNode);
            generateInstruction(cg, TR::InstOpCode::sync, node);
            resultReg = NULL;
            return true;
            }
         break;

      case TR::java_lang_Class_isAssignableFrom:
         {
         // Do not use an inline class check if the 'this' Class object is known to be an
         // abstract class or an interface at compile-time (the check will always fail
         // and go out of line).
         //
         TR::Node *receiverNode = node->getFirstChild();
         if (receiverNode->getOpCodeValue() == TR::aloadi &&
             receiverNode->getSymbolReference() == comp->getSymRefTab()->findJavaLangClassFromClassSymbolRef() &&
             receiverNode->getFirstChild()->getOpCodeValue() == TR::loadaddr)
            {
            TR::SymbolReference *receiverClassSymRef = receiverNode->getFirstChild()->getSymbolReference();
            if (receiverClassSymRef->isClassInterface(comp) ||
                receiverClassSymRef->isClassAbstract(comp))
               break;
            }

         static bool disable = feGetEnv("TR_disableInlineIsAssignableFrom") != NULL;
         if (!disable &&
             performTransformation(comp, "O^O PPC Evaluator: Specialize call to java/lang/Class.isAssignableFrom [%p].\n", node))
            {
            resultReg = inlineIsAssignableFrom(node, cg);
            return true;
            }

         break;
         }

      default:
         break;
         }
      }

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
   if (self()->inlineCryptoMethod(node, resultReg))
      {
      return true;
      }
#endif

   if (!comp->compileRelocatableCode() && !comp->getOption(TR_DisableDFP) && TR::Compiler->target.cpu.supportsDecimalFloatingPoint())
      {
      if (methodSymbol)
         {
         switch (methodSymbol->getMandatoryRecognizedMethod())
            {
         case TR::java_math_BigDecimal_DFPIntConstructor:
            resultReg = inlineBigDecimalConstructor(node, cg, false, false);
            return true;
         case TR::java_math_BigDecimal_DFPLongConstructor:
            resultReg = inlineBigDecimalConstructor(node, cg, true, false);
            return true;
         case TR::java_math_BigDecimal_DFPLongExpConstructor:
            resultReg = inlineBigDecimalConstructor(node, cg, true, true);
            return true;
         case TR::java_math_BigDecimal_DFPAdd:
            resultReg = inlineBigDecimalBinaryOp(node, cg, TR::InstOpCode::dadd, false);
            return true;
         case TR::java_math_BigDecimal_DFPSubtract:
            resultReg = inlineBigDecimalBinaryOp(node, cg, TR::InstOpCode::dsub, false);
            return true;
         case TR::java_math_BigDecimal_DFPMultiply:
            resultReg = inlineBigDecimalBinaryOp(node, cg, TR::InstOpCode::dmul, false);
            return true;
         case TR::java_math_BigDecimal_DFPDivide:
            resultReg = inlineBigDecimalDivide(node, cg);
            return true;
         case TR::java_math_BigDecimal_DFPScaledAdd:
            resultReg = inlineBigDecimalBinaryOp(node, cg, TR::InstOpCode::dadd, true);
            return true;
         case TR::java_math_BigDecimal_DFPScaledSubtract:
            resultReg = inlineBigDecimalBinaryOp(node, cg, TR::InstOpCode::dsub, true);
            return true;
         case TR::java_math_BigDecimal_DFPScaledMultiply:
            resultReg = inlineBigDecimalBinaryOp(node, cg, TR::InstOpCode::dmul, true);
            return true;
         case TR::java_math_BigDecimal_DFPRound:
            resultReg = inlineBigDecimalRound(node, cg);
            return true;
         case TR::java_math_BigDecimal_DFPExponent:
            resultReg = inlineBigDecimalUnaryOp(node, cg, TR::InstOpCode::dxex, false);
            return true;
         case TR::java_math_BigDecimal_DFPCompareTo:
            resultReg = inlineBigDecimalCompareTo(node, cg);
            return true;
         case TR::java_math_BigDecimal_DFPBCDDigits:
            resultReg = inlineBigDecimalUnaryOp(node, cg, TR::InstOpCode::ddedpd, false);
            return true;
         case TR::java_math_BigDecimal_DFPSignificance:
            resultReg = inlineBigDecimalUnaryOp(node, cg, TR::InstOpCode::ddedpd, true);
            return true;
         case TR::java_math_BigDecimal_DFPUnscaledValue:
            resultReg = inlineBigDecimalUnscaledValue(node, cg);
            return true;
         case TR::java_math_BigDecimal_DFPSetScale:
            resultReg = inlineBigDecimalSetScale(node, cg);
            return true;
         default:
            break;
            }
         }
      }

   // No method specialization was done.
   //
   resultReg = NULL;
   return false;
   }

void VMgenerateCatchBlockBBStartPrologue(TR::Node *node, TR::Instruction *fenceInstruction, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (comp->fe());
   if (comp->getOption(TR_FullSpeedDebug))
      {
      fenceInstruction->setNeedsGCMap(); // a catch entry is a gc point in FSD mode
      }

   TR::Block *block = node->getBlock();

   if (fej9->shouldPerformEDO(block, comp))
      {
      TR::Register *biAddrReg = cg->allocateRegister();
      TR::Register *recompCounterReg = cg->allocateRegister();
      intptrj_t addr = (intptrj_t) (comp->getRecompilationInfo()->getCounterAddress());
      TR::Instruction *cursor = loadAddressConstant(cg, node, addr, biAddrReg);
      TR::MemoryReference *loadbiMR = new (cg->trHeapMemory()) TR::MemoryReference(biAddrReg, 0, TR::Compiler->om.sizeofReferenceAddress(), cg);
      TR::MemoryReference *storebiMR = new (cg->trHeapMemory()) TR::MemoryReference(biAddrReg, 0, TR::Compiler->om.sizeofReferenceAddress(), cg);
      cursor = generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, recompCounterReg, loadbiMR);
      TR::Register *cndRegister = cg->allocateRegister(TR_CCR);
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2_r, node, recompCounterReg, recompCounterReg, cndRegister, -1, cursor);
      cursor = generateMemSrc1Instruction(cg,TR::InstOpCode::Op_st, node, storebiMR, recompCounterReg, cursor);
      cg->stopUsingRegister(biAddrReg);
      cg->stopUsingRegister(recompCounterReg);

      TR::LabelSymbol *snippetLabel = NULL;
      TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
      // snippet call chain through jitCallCFunction kills 3 parameter
      // registers that we will reserve here
      TR::Register *arg1Reg, *arg2Reg, *arg3Reg;
      arg1Reg = cg->allocateRegister();
      arg2Reg = cg->allocateRegister();
      arg3Reg = cg->allocateRegister();

      snippetLabel = cg->lookUpSnippet(TR::Snippet::IsForceRecompilation, NULL);
      if (snippetLabel == NULL)
         {
         TR::Snippet *snippet;
         snippetLabel = generateLabelSymbol(cg);
         snippet = new (cg->trHeapMemory()) TR::PPCForceRecompilationSnippet(snippetLabel, doneLabel, cursor, cg);
         cg->addSnippet(snippet);
         }

      TR::RegisterDependencyConditions *conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(4, 4, cg->trMemory());
      addDependency(conditions, arg1Reg, TR::RealRegister::gr3, TR_GPR, cg);
      addDependency(conditions, arg2Reg, TR::RealRegister::gr4, TR_GPR, cg);
      addDependency(conditions, arg3Reg, TR::RealRegister::gr5, TR_GPR, cg);
      addDependency(conditions, cndRegister, TR::RealRegister::cr0, TR_CCR, cg);

      // used to be blel
      // only need to call snippet once, so using beql instead
      cursor = generateConditionalBranchInstruction(cg, TR::InstOpCode::beql, node, snippetLabel, cndRegister);
      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, conditions);

      cg->machine()->setLinkRegisterKilled(true);
      conditions->stopUsingDepRegs(cg);
      }
   }

TR::Instruction *loadAddressRAM32(TR::CodeGenerator *cg, TR::Node * node, int32_t value, TR::Register *trgReg)
   {
   // load a 32-bit constant into a register with a fixed 2 instruction sequence
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (comp->fe());

   bool isAOT = comp->compileRelocatableCode();

   TR::Instruction *cursor = cg->getAppendInstruction();

   cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, trgReg, isAOT ? 0 : value>>16, cursor);

   if (value != 0x0)
      {
      uint32_t reloType;
      if (node->getSymbol()->castToResolvedMethodSymbol()->isSpecial())
         reloType = TR_SpecialRamMethodConst;
      else if (node->getSymbol()->castToResolvedMethodSymbol()->isStatic())
         reloType = TR_StaticRamMethodConst;
      else if (node->getSymbol()->castToResolvedMethodSymbol()->isVirtual())
         reloType = TR_VirtualRamMethodConst;
      else
         {
         reloType = TR_NoRelocation;
         TR_ASSERT(0,"JNI relocation not supported.");
         }
      if(isAOT)
         cg->addAOTRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation((uint8_t *)cursor, (uint8_t *) node->getSymbolReference(),
               node ? (uint8_t *)(intptr_t)node->getInlinedSiteIndex() : (uint8_t *)-1,
                  (TR_ExternalRelocationTargetKind) reloType, cg),
                  __FILE__, __LINE__, node);
      }

   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, trgReg, trgReg, isAOT ? 0 : value&0x0000ffff, cursor);

   cg->setAppendInstruction(cursor);
   return(cursor);
   }

TR::Instruction *loadAddressRAM(TR::CodeGenerator *cg, TR::Node * node, intptrj_t value, TR::Register *trgReg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (comp->fe());
   bool isAOT = comp->compileRelocatableCode();
   if (TR::Compiler->target.is32Bit())
      {
      return loadAddressRAM32(cg, node, (int32_t)value, trgReg);
      }

   // load a 64-bit constant into a register with a fixed 5 instruction sequence
   TR::Instruction *cursor = cg->getAppendInstruction();

   // lis trgReg, upper 16-bits
   cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, trgReg, isAOT? 0: (value>>48) , cursor);
   if (value != 0x0)
      {
      uint32_t reloType;
      if (node->getSymbol()->castToResolvedMethodSymbol()->isSpecial())
         reloType = TR_SpecialRamMethodConst;
      else if (node->getSymbol()->castToResolvedMethodSymbol()->isStatic())
         reloType = TR_StaticRamMethodConst;
      else if (node->getSymbol()->castToResolvedMethodSymbol()->isVirtual())
         reloType = TR_VirtualRamMethodConst;
      else
         {
         reloType = TR_NoRelocation;
         TR_ASSERT(0,"JNI relocation not supported.");
         }
      if(isAOT)
         cg->addAOTRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation((uint8_t *)cursor, (uint8_t *) node->getSymbolReference(),
               node ? (uint8_t *)(intptr_t)node->getInlinedSiteIndex() : (uint8_t *)-1,
                  (TR_ExternalRelocationTargetKind) reloType, cg),
                  __FILE__,__LINE__, node);
      }
   // ori trgReg, trgReg, next 16-bits
   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, trgReg, trgReg, isAOT ? 0 : ((value>>32) & 0x0000ffff), cursor);
   // shiftli trgReg, trgReg, 32
   cursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicr, node, trgReg, trgReg, 32, CONSTANT64(0xFFFFFFFF00000000), cursor);
   // oris trgReg, trgReg, next 16-bits
   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::oris, node, trgReg, trgReg, isAOT ? 0 : ((value>>16) & 0x0000ffff), cursor);
   // ori trgReg, trgReg, last 16-bits
   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, trgReg, trgReg, isAOT ? 0 : (value & 0x0000ffff), cursor);

   cg->setAppendInstruction(cursor);

   return(cursor);
   }

TR::Instruction *loadAddressJNI32(TR::CodeGenerator *cg, TR::Node * node, int32_t value, TR::Register *trgReg)
   {
   // load a 32-bit constant into a register with a fixed 2 instruction sequence
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->comp()->fe());

   bool isAOT = comp->compileRelocatableCode();

   TR::Instruction *cursor = cg->getAppendInstruction();

   cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, trgReg, isAOT ? 0 : value>>16, cursor);

   if (value != 0x0)
      {
      uint32_t reloType;
      if (node->getSymbol()->castToResolvedMethodSymbol()->isSpecial())
         reloType = TR_JNISpecialTargetAddress;
      else if (node->getSymbol()->castToResolvedMethodSymbol()->isStatic())
         reloType = TR_JNIStaticTargetAddress;
      else if (node->getSymbol()->castToResolvedMethodSymbol()->isVirtual())
         reloType = TR_JNIVirtualTargetAddress;
      else
         {
         reloType = TR_NoRelocation;
         TR_ASSERT(0,"JNI relocation not supported.");
         }
      if(isAOT)
         cg->addAOTRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation((uint8_t *)cursor, (uint8_t *) node->getSymbolReference(),
               node ? (uint8_t *)(intptr_t)node->getInlinedSiteIndex() : (uint8_t *)-1,
                  (TR_ExternalRelocationTargetKind) reloType, cg),
                  __FILE__, __LINE__, node);
      }

   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, trgReg, trgReg, isAOT ? 0 : value&0x0000ffff, cursor);

   cg->setAppendInstruction(cursor);
   return(cursor);
   }

TR::Instruction *loadAddressJNI(TR::CodeGenerator *cg, TR::Node * node, intptrj_t value, TR::Register *trgReg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->comp()->fe());
   bool isAOT = comp->compileRelocatableCode();
   if (TR::Compiler->target.is32Bit())
      {
      return loadAddressJNI32(cg, node, (int32_t)value, trgReg);
      }

   // load a 64-bit constant into a register with a fixed 5 instruction sequence
   TR::Instruction *cursor = cg->getAppendInstruction();

   // lis trgReg, upper 16-bits
   cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, trgReg, isAOT? 0: (value>>48) , cursor);
   if (value != 0x0)
      {
      uint32_t reloType;
      if (node->getSymbol()->castToResolvedMethodSymbol()->isSpecial())
         reloType = TR_JNISpecialTargetAddress;
      else if (node->getSymbol()->castToResolvedMethodSymbol()->isStatic())
         reloType = TR_JNIStaticTargetAddress;
      else if (node->getSymbol()->castToResolvedMethodSymbol()->isVirtual())
         reloType = TR_JNIVirtualTargetAddress;
      else
         {
         reloType = TR_NoRelocation;
         TR_ASSERT(0,"JNI relocation not supported.");
         }
      if(isAOT)
         cg->addAOTRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation((uint8_t *)cursor, (uint8_t *) node->getSymbolReference(),
               node ? (uint8_t *)(intptr_t)node->getInlinedSiteIndex() : (uint8_t *)-1,
                  (TR_ExternalRelocationTargetKind) reloType, cg),
                  __FILE__,__LINE__, node);
      }
   // ori trgReg, trgReg, next 16-bits
   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, trgReg, trgReg, isAOT ? 0 : ((value>>32) & 0x0000ffff), cursor);
   // shiftli trgReg, trgReg, 32
   cursor = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicr, node, trgReg, trgReg, 32, CONSTANT64(0xFFFFFFFF00000000), cursor);
   // oris trgReg, trgReg, next 16-bits
   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::oris, node, trgReg, trgReg, isAOT ? 0 : ((value>>16) & 0x0000ffff), cursor);
   // ori trgReg, trgReg, last 16-bits
   cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, trgReg, trgReg, isAOT ? 0 : (value & 0x0000ffff), cursor);

   cg->setAppendInstruction(cursor);

   return(cursor);
   }

TR::Register *J9::Power::TreeEvaluator::directCallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::SymbolReference *symRef    = node->getSymbolReference();
   TR::MethodSymbol    *callee    = symRef->getSymbol()->castToMethodSymbol();
   TR::Linkage      *linkage;
   TR::Register        *returnRegister;

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());

   if(callee->isJNI() && callee->getLinkageConvention() != TR_J9JNILinkage)
      {
      //This needs a cleanup, canRelocateDirectNativeCalls() might go away soon.
      //Too many checks here, can we simplify this?

      static char * disableDirectNativeCall = feGetEnv("TR_DisableDirectNativeCall");
      bool doJNIDirectDispatch =    fej9->canRelocateDirectNativeCalls() &&
                                       (node->isPreparedForDirectJNI() ||
                                             (disableDirectNativeCall == NULL && callee->getResolvedMethodSymbol()->canDirectNativeCall()));

      if(doJNIDirectDispatch)
         callee->setLinkage(TR_J9JNILinkage);
      }


   if (!cg->inlineDirectCall(node, returnRegister))
      {
      linkage = cg->getLinkage(callee->getLinkageConvention());
      returnRegister = linkage->buildDirectDispatch(node);
      }

   return returnRegister;
   }


TR::Register *J9::Power::TreeEvaluator::tstartEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
#ifndef PUBLIC_BUILD
   // tstart
   //  - persistentFailNode
   //  - transientFailNode
   //  - fallThroughNode (next block)
   //  - monitorObject
   //fprintf(stderr,"tstart Start\n");
   TR::Node *persistentFailureNode = node->getFirstChild();
   TR::Node *transientFailureNode  = node->getSecondChild();
   TR::Node *fallThrough = node->getThirdChild();
   TR::Node *objNode = node->getChild(3);
   TR::Node *GRANode = NULL;
   TR::Compilation *comp = cg->comp();

   TR::LabelSymbol *labelPersistentFailure = persistentFailureNode->getBranchDestination()->getNode()->getLabel();
   TR::LabelSymbol *labelTransientFailure  = transientFailureNode->getBranchDestination()->getNode()->getLabel();
   TR::LabelSymbol *labelfallThrough  = fallThrough->getBranchDestination()->getNode()->getLabel();
   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *lockwordLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *noTexStatsLabel = generateLabelSymbol(cg);

   TR::Register *objReg = cg->evaluate(objNode);
   TR::Register *monReg = cg->allocateRegister();
   TR::Register *cndReg = cg->allocateRegister(TR_CCR);
   TR::Register *tempReg = cg->allocateRegister();
   //TR::Register *temp2Reg = cg->allocateRegister();
   //TR::Register *temp3Reg = cg->allocateRegister();

   // Dependency conditions for this evaluator's internal control flow
   TR::RegisterDependencyConditions *conditions = NULL;
   // Dependency conditions for GRA
   TR::RegisterDependencyConditions *fallThroughConditions = NULL;
   TR::RegisterDependencyConditions *persistentConditions = NULL;
   TR::RegisterDependencyConditions *transientConditions  = NULL;

   conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(4, 4, cg->trMemory());

   if (fallThrough->getNumChildren() != 0)
      {
      GRANode = fallThrough->getFirstChild();
      cg->evaluate(GRANode);
      fallThroughConditions = generateRegisterDependencyConditions(cg, GRANode, 0);
      cg->decReferenceCount(GRANode);
      }

   if (persistentFailureNode->getNumChildren() != 0)
      {
      GRANode = persistentFailureNode->getFirstChild();
      cg->evaluate(GRANode);
      persistentConditions = generateRegisterDependencyConditions(cg, GRANode, 0);
      cg->decReferenceCount(GRANode);
      }

   if (transientFailureNode->getNumChildren() != 0)
      {
      GRANode = transientFailureNode->getFirstChild();
      cg->evaluate(GRANode);
      transientConditions = generateRegisterDependencyConditions(cg, GRANode, 0);
      cg->decReferenceCount(GRANode);
      }

   uint32_t conditionCursor = conditions->getAddCursorForPre();
   addDependency(conditions, objReg, TR::RealRegister::NoReg, TR_GPR, cg);
   conditions->getPreConditions()->getRegisterDependency(conditionCursor)->setExcludeGPR0();
   conditions->getPostConditions()->getRegisterDependency(conditionCursor++)->setExcludeGPR0();
   addDependency(conditions, cndReg, TR::RealRegister::cr0, TR_CCR, cg);
   addDependency(conditions, monReg, TR::RealRegister::NoReg, TR_GPR, cg);
   addDependency(conditions, tempReg, TR::RealRegister::NoReg, TR_GPR, cg);

   static char * debugTMTLE = feGetEnv("debugTMTLE");

   if (debugTMTLE)
      printf ("\nTM: use TM TLE in %s (%s) %p", comp->signature(), comp->getHotnessName(comp->getMethodHotness()), node);

   if (debugTMTLE )
      {
      if (persistentConditions)
        {
        generateDepLabelInstruction(cg, TR::InstOpCode::b, node, labelPersistentFailure, persistentConditions);
        }
     else
        {
        generateLabelInstruction(cg, TR::InstOpCode::b, node, labelPersistentFailure);
         }
      }
   else
      {
      generateInstruction(cg, TR::InstOpCode::tbegin_r, node);
      }

      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node,
                                           lockwordLabel, cndReg);

   generateTrg1Instruction(cg, TR::InstOpCode::mftexasru, node, monReg);

   // This mask *should* correspond to the TEXASR failure persistent bit (8)
   // NOT the abort bit (31, indicates an explicit abort)
   //loadConstant(cg, node, 0x01000001, tempReg);
   loadConstant(cg, node, 0x01000000, tempReg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::and_r, node, tempReg, tempReg, monReg);
   if (transientConditions)
      {
      generateDepConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, labelTransientFailure, cndReg, transientConditions);
      }
   else
      {
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, labelTransientFailure, cndReg);
      }
   if (persistentConditions)
      {
      generateDepLabelInstruction(cg, TR::InstOpCode::b, node, labelPersistentFailure, persistentConditions);
      }
   else
      {
      generateLabelInstruction(cg, TR::InstOpCode::b, node, labelPersistentFailure);
      }

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, lockwordLabel, conditions);

   int32_t lwOffset = cg->fej9()->getByteOffsetToLockword((TR_OpaqueClassBlock *) cg->getMonClass(node));

   if (TR::Compiler->target.is64Bit() && cg->fej9()->generateCompressedLockWord())
      {
      generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, monReg,
                                 new (cg->trHeapMemory()) TR::MemoryReference(objReg, lwOffset, 4, cg));
      }
   else
      {
      generateTrg1MemInstruction(cg,TR::InstOpCode::Op_load, node, monReg,
                                 new (cg->trHeapMemory()) TR::MemoryReference(objReg, lwOffset, TR::Compiler->om.sizeofReferenceAddress(), cg));
      }

   // abort if lock is held
   if (TR::Compiler->target.is32Bit() || cg->fej9()->generateCompressedLockWord())
      {
      generateSrc1Instruction(cg, TR::InstOpCode::tabortwneqi_r, node, monReg, 0);
      }
   else
      {
      generateSrc1Instruction(cg, TR::InstOpCode::tabortdneqi_r, node, monReg, 0);
      }

   TR::TreeTop *BBendTreeTop = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();
   TR::TreeTop *BBstartTreeTop = NULL;
   if (BBendTreeTop)
      BBstartTreeTop = BBendTreeTop->getNextTreeTop();
   TR::TreeTop *fallThruTarget = fallThrough->getBranchDestination();

   if (BBstartTreeTop && (fallThruTarget == BBstartTreeTop))
      {
      if (fallThroughConditions)
         generateDepLabelInstruction(cg, TR::InstOpCode::label, node, startLabel, fallThroughConditions);
      else
         generateLabelInstruction(cg, TR::InstOpCode::label, node, startLabel);
      }
   else
      {
      if (fallThroughConditions)
         generateDepLabelInstruction(cg, TR::InstOpCode::b, node, labelfallThrough, fallThroughConditions);
      else
         generateLabelInstruction(cg, TR::InstOpCode::b, node, labelfallThrough);
      }

   cg->stopUsingRegister(monReg);
   cg->stopUsingRegister(cndReg);
   cg->stopUsingRegister(tempReg);

   cg->decReferenceCount(objNode);
   cg->decReferenceCount(persistentFailureNode);
   cg->decReferenceCount(transientFailureNode);
   cg->decReferenceCount(fallThrough);
#endif //PUBLIC_BUILD

   return NULL;
   }

TR::Register *J9::Power::TreeEvaluator::tfinishEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
#ifndef PUBLIC_BUILD
   generateInstruction(cg, TR::InstOpCode::tend_r, node);
#endif //PUBLIC_BUILD
   return NULL;
   }

TR::Register *J9::Power::TreeEvaluator::tabortEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   generateInstruction(cg, TR::InstOpCode::tabort_r, node);
   return NULL;
   }


#else /* TR_TARGET_POWER */
// the following is to force an export to keep ilib happy
int J9PPCEvaluator=0;
#endif /* TR_TARGET_POWER */
