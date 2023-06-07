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

#include "codegen/CodeGenerator.hpp"
#include "compile/InlineBlock.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/CompilerEnv.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "infra/Cfg.hpp"
#include "infra/Checklist.hpp"
#include "env/VMJ9.h"
#include "ilgen/J9ByteCodeIlGenerator.hpp"
#include "optimizer/BoolArrayStoreTransformer.hpp"
#include "ras/DebugCounter.hpp"
#include "optimizer/TransformUtil.hpp"
#include "env/JSR292Methods.h"

#define OPT_DETAILS "O^O ILGEN: "

TR_J9ByteCodeIlGenerator::TR_J9ByteCodeIlGenerator(
   TR::IlGeneratorMethodDetails & methodDetails, TR::ResolvedMethodSymbol * methodSymbol, TR_J9VMBase * fe, TR::Compilation * comp,
   TR::SymbolReferenceTable * symRefTab, bool forceClassLookahead, TR_InlineBlocks *blocksToInline, int32_t argPlaceholderSlot) //TR_ScratchList<TR_InlineBlock> *blocksToInline)
   : TR_J9ByteCodeIteratorWithState(methodSymbol, fe, comp),
     _methodDetails(methodDetails),
     _symRefTab(symRefTab),
     _classLookaheadSymRefTab(NULL),
     _blockAddedVisitCount(comp->incVisitCount()),
     _generateWriteBarriersForGC(TR::Compiler->om.writeBarrierType() != gc_modron_wrtbar_none),
     _generateWriteBarriersForFieldWatch(comp->getOption(TR_EnableFieldWatch)),
     _generateReadBarriersForFieldWatch(comp->getOption(TR_EnableFieldWatch)),
     _suppressSpineChecks(false),
     _implicitMonitorExits(comp->trMemory()),
     _finalizeCallsBeforeReturns(comp->trMemory()),
     _classInfo(0),
     _blocksToInline(blocksToInline),
     _argPlaceholderSlot(argPlaceholderSlot),
     _intrinsicErrorHandling(0),
     _invokeSpecialInterface(NULL),
     _invokeSpecialInterfaceCalls(NULL),
     _invokeSpecialSeen(false),
     _couldOSRAtNextBC(false),
     _processedOSRNodes(NULL),
     _invokeHandleCalls(NULL),
     _invokeHandleGenericCalls(NULL),
     _invokeDynamicCalls(NULL),
     _ilGenMacroInvokeExactCalls(NULL),
     _methodHandleInvokeCalls(NULL)
   {
   static const char *noLookahead = feGetEnv("TR_noLookahead");
   _noLookahead = (noLookahead || comp->getOption(TR_DisableLookahead)) ? true : false;
   _thisChanged = false;
   if (
       (forceClassLookahead ||
        (comp->getNeedsClassLookahead() && !_noLookahead &&
         ((comp->getMethodHotness() >= scorching) ||
          (comp->couldBeRecompiled() && (comp->getMethodHotness() >= hot ))))))
      {
      bool allowForAOT = comp->getOption(TR_UseSymbolValidationManager);
      _classInfo = comp->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(method()->containingClass(), comp, allowForAOT);
      }
   else
      {
      if (!comp->getOption(TR_PerformLookaheadAtWarmCold))
         _noLookahead = true;
      }

   if (argPlaceholderSlot == -1)
      {
      _argPlaceholderSignatureOffset = 0xdead1127;
      }
   else
      {
      // Compute _argPlaceholderSignatureOffset
      //
      char *signatureChars = _methodSymbol->getResolvedMethod()->signatureChars();
      TR_ASSERT(*signatureChars = '(', "assertion failure");
      char *curArg = signatureChars+1;
      int32_t argSlotsSkipped = methodSymbol->isStatic()? 0 : 1; // receiver doesn't appear in the signature
      while (argSlotsSkipped < argPlaceholderSlot)
         {
         switch (*curArg)
            {
            case 'D':
            case 'J':
               argSlotsSkipped += 2;
               break;
            default:
               argSlotsSkipped += 1;
               break;
            }
         curArg = nextSignatureArgument(curArg);
         }
      _argPlaceholderSignatureOffset = curArg - signatureChars;
      }
   for( int i=0 ; i < _numDecFormatRenames ; i++ )
      {
      _decFormatRenamesDstSymRef[i] = NULL;
      }
   if (comp->getOption(TR_EnableOSR)
       && !comp->isPeekingMethod()
       && comp->isOSRTransitionTarget(TR::postExecutionOSR)
       && !_cannotAttemptOSR)
      _processedOSRNodes = new (trStackMemory()) TR::NodeChecklist(comp);
   }

bool
TR_J9ByteCodeIlGenerator::genIL()
   {
   if (comp()->isOutermostMethod())
      comp()->reportILGeneratorPhase();

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   comp()->setCurrentIlGenerator(this);

   bool success = internalGenIL();

   if (success && !comp()->isPeekingMethod())
      {
      TR_SharedCache *sc = fej9()->sharedCache();
      if (sc)
         {
         /*
          * if DelayRelocationForAOT don't persist iprofiler info now.
          * instead, persist iprofiler info when loading the aot compilation
          */
         if (comp()->getOption(TR_DisableDelayRelocationForAOTCompilations) || !fej9()->shouldDelayAotLoad())
            {
            sc->persistIprofileInfo(_methodSymbol->getResolvedMethodSymbol(), comp());
            }
         }
      }

   /*
    * If we're generating IL for DecimalformatHelper.formatAsDouble(Float), replace
    * the necessary fields, statics, and methods appropriately.  This is part of
    * the optimization that replaces df.format(bd.doubleValue()) and
    * df.format(bd.floatValue()) with, respectively,
    * DecimalFormatHelper.formatAsDouble(df, bd) and
    * DecimalFormatHelper.formatAsFloat(df, bd) in which bd is a BigDecimal object
    * and df is DecimalFormat object. The latter pair of calls are much faster than
    * the former ones because it avoids many of the conversions that the former
    * performs.
    */
   if (success)
      {
      const char* methodName = _methodSymbol->signature(comp()->trMemory());
      if (!strcmp(methodName, "com/ibm/jit/DecimalFormatHelper.formatAsDouble(Ljava/text/DecimalFormat;Ljava/math/BigDecimal;)Ljava/lang/String;") ||
          !strcmp(methodName, "com/ibm/jit/DecimalFormatHelper.formatAsFloat(Ljava/text/DecimalFormat;Ljava/math/BigDecimal;)Ljava/lang/String;"))
         success = success && replaceMembersOfFormat();
      }

   if (success && !comp()->isPeekingMethod())
      {
      _methodSymbol->clearProfilingOffsetInfo();
      for (TR::Block *block = _methodSymbol->getFirstTreeTop()->getEnclosingBlock(); block; block = block->getNextBlock())
         _methodSymbol->addProfilingOffsetInfo(block->getEntry()->getNode()->getByteCodeIndex(), block->getExit()->getNode()->getByteCodeIndex());
      }

   comp()->setCurrentIlGenerator(0);

   return success;
   }

bool TR_J9ByteCodeIlGenerator::internalGenIL()
   {
   _stack = new (trStackMemory()) TR_Stack<TR::Node *>(trMemory(), 20, false, stackAlloc);

   bool success = false;

   if ((method()->isNewInstanceImplThunk() || debug("testGenNewInstanceImplThunk")))
      {
      success = genNewInstanceImplThunk();
      if (!success) // must jit the body (throw instantiation exception)
         success = genILFromByteCodes();
      else if (comp()->getOption(TR_EnableOSR) && !comp()->isPeekingMethod() && !comp()->getOption(TR_FullSpeedDebug))
         _methodSymbol->setCannotAttemptOSR(0);

      return success;
      }

   TR::RecognizedMethod recognizedMethod = _methodSymbol->getRecognizedMethod();
   if (recognizedMethod != TR::unknownMethod)
      {
      if (recognizedMethod == TR::com_ibm_jit_JITHelpers_supportsIntrinsicCaseConversion && !TR::Compiler->om.canGenerateArraylets())
         {
         if (performTransformation(comp(), "O^O IlGenerator: Generate com/ibm/jit/JITHelpers.supportsIntrinsicCaseConversion\n"))
            {
            genHWOptimizedStrProcessingAvailable();
            return true;
            }
         }

      if (recognizedMethod == TR::com_ibm_dataaccess_DecimalData_JITIntrinsicsEnabled)
         {
         if (performTransformation(comp(), "O^O IlGenerator: Generate com/ibm/dataaccess/DecimalData.JITIntrinsicsEnabled\n"))
            {
            genJITIntrinsicsEnabled();
            return true;
            }
         }

      if (recognizedMethod == TR::com_ibm_rmi_io_FastPathForCollocated_isVMDeepCopySupported)
         {
         if (performTransformation(comp(), "O^O IlGenerator: Generate com/ibm/rmi/io/FastPathForCollocated/isVMDeepCopySupported\n"))
            {
            genIsORBDeepCopyAvailable();
            return true;
            }
         }

      if (!comp()->getOption(TR_DisableInliningOfNatives))
         {
         // If we're inlining then there are some stack walking routines that can be made faster
         // by avoiding the stack walk.
         //
         TR_ResolvedMethod * caller1 = method()->owningMethod();
         TR_ResolvedMethod * caller = caller1 ? caller1->owningMethod() : 0;

         if( caller && caller1)
            {
            TR_OpaqueClassBlock *callerClass  = caller  ? caller->classOfMethod() : 0;
            TR_OpaqueClassBlock *callerClass1 = caller1 ? caller1->classOfMethod() : 0;

            bool doIt = !(fej9()->stackWalkerMaySkipFrames(caller->getPersistentIdentifier(),callerClass) ||
                          fej9()->stackWalkerMaySkipFrames(caller1->getPersistentIdentifier(),callerClass1));


            if (doIt && !comp()->compileRelocatableCode())
               {
               if (recognizedMethod == TR::java_lang_ClassLoader_callerClassLoader)
                  {
                  createGeneratedFirstBlock();
                  // check for bootstrap classloader, if so
                  // return null (see semantics of ClassLoader.callerClassLoader())
                  //
                  if (fej9()->isClassLoadedBySystemClassLoader(caller->classOfMethod()))
                     {
                     loadConstant(TR::aconst, (void *)0);
                     }
                  else
                     {
                     loadSymbol(TR::aload, symRefTab()->findOrCreateClassLoaderSymbolRef(caller));
                     }
                  genTreeTop(TR::Node::create(method()->returnOpCode(), 1, pop()));
                  return true;
                  }
               if (recognizedMethod == TR::com_ibm_oti_vm_VM_callerClass)
                  {
                  createGeneratedFirstBlock();
                  loadConstant(TR::aconst, caller->classOfMethod());
                  genTreeTop(TR::Node::create(method()->returnOpCode(), 1, pop()));
                  return true;
                  }
               }
            }
         }
      }
   if (method()->isJNINative())
      return genJNIIL();

   return genILFromByteCodes();
   }

bool
TR_J9ByteCodeIlGenerator::genILFromByteCodes()
   {
   // first passthrough of the byte code to see if this pointer has been changed
   if (isThisChanged())
      _thisChanged = true;

   initialize();

   // don't go peeking into massive methods
   if (comp()->isPeekingMethod() && _maxByteCodeIndex >= USHRT_MAX/8)
      return false;

   // FSD sync object support
   //
   // Ideally, I'd like the sync object in FSD to work exactly as it does in the interpreter.
   // This means that the sync object (receiver for instance methods, declaring class for
   // static methods) is always stored in synthetic local N+1 after the stack frame is built,
   // and is re-read from memory before use in the method monitor exit.  If it's a lot of trouble
   // to place it at N+1, the decompiler can read it from somewhere else as long as you can point me there
   // via the metadata.  Whatever slot is used, it must be marked as a GC reference even when it is a class.
   // If we eventually support hot code replace which does not flush the JIT code caches, we'll
   // need to do some more work for static methods, since the class sync object in static sync frames must always be the "current" class.
   //
   if (_methodSymbol->isSynchronised())
      {
      if (comp()->getOption(TR_FullSpeedDebug) || !comp()->getOption(TR_DisableLiveMonitorMetadata))
         {
         TR::SymbolReference * symRef;
         if (comp()->getOption(TR_FullSpeedDebug))
            symRef = symRefTab()->findOrCreateAutoSymbol(_methodSymbol, _methodSymbol->getSyncObjectTempIndex(), TR::Address);
         else
            symRef = symRefTab()->createTemporary(_methodSymbol, TR::Address);

         _methodSymbol->setSyncObjectTemp(symRef);

         if (!comp()->getOption(TR_DisableLiveMonitorMetadata))
            {
            symRef->setHoldsMonitoredObjectForSyncMethod();
            comp()->addAsMonitorAuto(symRef, true);
            }
         }
      }

   if (_methodSymbol->getResolvedMethod()->isNonEmptyObjectConstructor())
      {
      if (comp()->getOption(TR_FullSpeedDebug))
         {
         TR::SymbolReference *symRef = symRefTab()->findOrCreateAutoSymbol(_methodSymbol, _methodSymbol->getThisTempForObjectCtorIndex(), TR::Address);
         _methodSymbol->setThisTempForObjectCtor(symRef);
         symRef->getSymbol()->setThisTempForObjectCtor();
         }
      }

   _staticFieldReferenceEncountered = false;
   _staticMethodInvokeEncountered = false;

   // Allocate zero-length bit vectors before walker so that the bit vectors can grow on the right
   // stack memory region
   //
   _methodHandleInvokeCalls = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc, growable);
   _invokeHandleCalls = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc, growable);
   _invokeHandleGenericCalls = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc, growable);
   _invokeDynamicCalls = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc, growable);
   _ilGenMacroInvokeExactCalls = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc, growable);

   TR::Block * lastBlock = walker(0);

   if (hasExceptionHandlers())
      {
      _methodSymbol->setHasExceptionHandlers();
      lastBlock = genExceptionHandlers(lastBlock);
      }

   _bcIndex = 0;

   _methodSymbol->setFirstTreeTop(blocks(0)->getEntry());

   if (inliningCheckIfFinalizeObjectIsBeneficial())
      {
      inlineJitCheckIfFinalizeObject(blocks(0));
      }

   prependEntryCode(blocks(0));

   if (!comp()->getOption(TR_DisableGuardedCountingRecompilations) &&
       comp()->getRecompilationInfo() && comp()->getRecompilationInfo()->shouldBeCompiledAgain() &&
       !comp()->getRecompilationInfo()->isRecompilation() && // only do it for first time compilations
       (!comp()->getPersistentInfo()->_countForRecompile || comp()->getOption(TR_EnableMultipleGCRPeriods)) &&
       comp()->isOutermostMethod() &&
       comp()->getOptions()->getInsertGCRTrees() &&
       !comp()->isDLT() && !method()->isJNINative())
      {
      // GCR filtering: Do not insert GCR trees for methods that are small and have no calls
      // This code is better suited for CompilationThread.cpp
      // but we need support from VM to tell us that a method has calls
      if (_methodSymbol->mayHaveInlineableCall() ||
         // Possible tweak: increase the threshold for methods that do not have loops
         _maxByteCodeIndex > TR::Options::_smallMethodBytecodeSizeThresholdForCold ||
         _methodSymbol->mayHaveLoops())
         {
         prependGuardedCountForRecompilation(comp()->getStartTree()->getNode()->getBlock());
         comp()->getOptimizationPlan()->resetAddToUpgradeQueue(); // do not add to upgrade queue methods for which we used GCR
         // stats
         comp()->getPersistentInfo()->incNumGCRBodies();
         }
      else
         {
         //TR_VerboseLog::writeLineLocked(TR_Vlog_PERF,"Saved a GCR gen body nmayHaveInlineableCall=%d  mayHaveLoops=%d _maxByteCodeIndex=%d\n", _methodSymbol->mayHaveInlineableCall(), _methodSymbol->mayHaveLoops(), _maxByteCodeIndex);
         // stats
         comp()->getPersistentInfo()->incNumGCRSaves();
         }
      }

   // Logic related to SamplingJProfiling
   //
   if (comp()->isOutermostMethod() && // Only do it once for the method to be compiled
      !comp()->getOptions()->isDisabled(OMR::samplingJProfiling)) // If heuristic enabled samplingJProfiling for this body
      {
      // Verify that the GCR logic above actually inserted GCR trees
      // or that this is a DLT body
      // Also verify that method has bytecodes we want to profile (invokes/checkcasts/branches)
      //
      if ((comp()->isDLT() || (comp()->getRecompilationInfo() &&
                               comp()->getRecompilationInfo()->getJittedBodyInfo()->getUsesGCR()))
         && (_methodSymbol->mayHaveInlineableCall()
            || _methodSymbol->hasCheckcastsOrInstanceOfs()
            || _methodSymbol->hasBranches())
         )
         {
         // Disable inlining to compile fast and avoid the bug with not profiling on the fast path
         comp()->getOptions()->setDisabled(OMR::inlining, true);
         }
      else // Canot profile or there is nothing to profile; take corrective actions
         {
         // Disable the samplingJProfiling opt
         comp()->getOptions()->setDisabled(OMR::samplingJProfiling, true);

         // If the opt level was reduced to cold solely because we wanted
         // to do samplingJProfiling then we must move the opt level back to warm
         if (comp()->getOptimizationPlan()->isDowngradedDueToSamplingJProfiling())
            {
            comp()->changeOptLevel(warm);
            comp()->getOptimizationPlan()->setDowngradedDueToSamplingJProfiling(false);
            comp()->getOptimizationPlan()->setOptLevelDowngraded(false);
            }
         }
      }


   // Code pertaining to the secondary/upgrade compilation queue
   // If the method is small, doesn't have loops or calls, then do not try to upgrade it
   if (comp()->isOutermostMethod() && comp()->getOptimizationPlan()->shouldAddToUpgradeQueue())
      {
      if (!_methodSymbol->mayHaveInlineableCall() && !_methodSymbol->mayHaveLoops() &&
          _maxByteCodeIndex <= TR::Options::_smallMethodBytecodeSizeThresholdForCold)
          comp()->getOptimizationPlan()->resetAddToUpgradeQueue();
      }

   // the optimizer assumes that the ilGenerator doesn't gen code for
   // unreachable blocks.  An exception handler may be unreachable.
   //
   if (hasExceptionHandlers())
      cfg()->removeUnreachableBlocks();

   int32_t fpIndex = hasFPU() ? -1 : findFloatingPointInstruction();
   if (fpIndex != -1) _unimplementedOpcode = _code[fpIndex];

   if (_unimplementedOpcode)
      {
      _methodSymbol->setUnimplementedOpcode(_unimplementedOpcode);

      if (debug("traceInfo"))
         {
         if (_unimplementedOpcode == 255)
            diagnostic("\nUnimplemented opcodes found\n");
         else
            diagnostic("\nUnimplemented opcode found: %s(%d)\n",
                        ((TR_J9VM *)fej9())->getByteCodeName(_unimplementedOpcode), _unimplementedOpcode);
         }

      if (!debug("continueWithUnimplementedOpCode"))
         return false;
      }

   //if (!_thisChanged)
      //setThisNonNullProperty(_methodSymbol->getFirstTreeTop(), comp());

   bool needMonitor = _methodSymbol->isSynchronised() && !comp()->getOption(TR_DisableLiveMonitorMetadata);
   int32_t numMonents = 0;
   int32_t numMonexits = 0;
   bool primitive = true;
   TR::TreeTop *monentStore = NULL;
   TR::TreeTop *monexitStore = NULL;
   TR::Node *monentTree = NULL;
   TR::Node *monexitTree = NULL;
   TR::TreeTop *currTree = _methodSymbol->getFirstTreeTop()->getNextTreeTop();

   List<TR::SymbolReference> autoOrParmSymRefList(comp()->trMemory());
   TR_ScratchList<TR::TreeTop> unresolvedCheckcastTopsNeedingNullGuard(comp()->trMemory());
   TR_ScratchList<TR::TreeTop> unresolvedInstanceofTops(comp()->trMemory());
   TR_ScratchList<TR::TreeTop> invokeSpecialInterfaceTops(comp()->trMemory());
   TR::NodeChecklist evaluatedInvokeSpecialCalls(comp());
   TR::NodeChecklist evaluatedMethodHandleInvokeCalls(comp());
   TR_BoolArrayStoreTransformer::NodeSet bstoreiUnknownArrayTypeNodes(std::less<TR::Node *>(), comp()->trMemory()->currentStackRegion());
   TR_BoolArrayStoreTransformer::NodeSet bstoreiBoolArrayTypeNodes(std::less<TR::Node *>(), comp()->trMemory()->currentStackRegion());
   TR_BoolArrayStoreTransformer boolArrayStoreTransformer(&bstoreiUnknownArrayTypeNodes, &bstoreiBoolArrayTypeNodes);

   for (; currTree != NULL; currTree = currTree->getNextTreeTop())
      {
      TR::Node *currNode = currTree->getNode();
      TR::ILOpCode opcode = currNode->getOpCode();

      if (currNode->getNumChildren() >= 1
          && currNode->getFirstChild()->getOpCode().isCall()
          && !currNode->getFirstChild()->getSymbol()->castToMethodSymbol()->isHelper()
          && _methodHandleInvokeCalls->isSet(currNode->getFirstChild()->getByteCodeIndex())
          && !evaluatedMethodHandleInvokeCalls.contains(currNode->getFirstChild()))
         {
         expandMethodHandleInvokeCall(currTree);
         evaluatedMethodHandleInvokeCalls.add(currNode->getFirstChild());
         continue;
         }

      if ((opcode.isStoreDirect() && opcode.hasSymbolReference() && currNode->getSymbolReference()->getSymbol()->isAutoOrParm()) ||
          opcode.isCheckCast())
         {
         TR::SymbolReference *symRef = currNode->getSymbolReference();
         TR::Node *typeNode = NULL;
         if (opcode.isStoreDirect())
            typeNode = currNode->getFirstChild(); // store auto
         else typeNode = currNode->getSecondChild(); // checkcast
         if (boolArrayStoreTransformer.isAnyDimensionBoolArrayNode(typeNode))
            boolArrayStoreTransformer.setHasBoolArrayAutoOrCheckCast();
         else if (boolArrayStoreTransformer.isAnyDimensionByteArrayNode(typeNode))
            boolArrayStoreTransformer.setHasByteArrayAutoOrCheckCast();

         if (opcode.isStoreDirect() && symRef->getSymbol()->isParm() && currNode->getDataType() == TR::Address)
            {
            int lhsLength;
            int rhsLength;
            const char *lhsSig = currNode->getTypeSignature(lhsLength, stackAlloc, false /* parmAsAuto */);
            const char *rhsSig = typeNode->getTypeSignature(rhsLength, stackAlloc, true /* parmAsAuto */);
            if (!lhsSig || !rhsSig || lhsLength != rhsLength || strncmp(lhsSig, rhsSig, lhsLength))
               boolArrayStoreTransformer.setHasVariantArgs();
            }
         }
      else if (opcode.getOpCodeValue() == TR::bstorei && currNode->getSymbolReference()->getCPIndex() == -1
            && currNode->getFirstChild()->isInternalPointer())
         {
         TR::Node *arrayBase = currNode->getFirstChild()->getFirstChild();
         if (arrayBase->getOpCode().hasSymbolReference())
            {
            if (boolArrayStoreTransformer.isBoolArrayNode(arrayBase))
               {
               if (comp()->getOption(TR_TraceILGen))
                  traceMsg(comp(), "bstorei node n%dn is bool array store\n", currNode->getGlobalIndex());
               bstoreiBoolArrayTypeNodes.insert(currNode);
               }
            else if (!boolArrayStoreTransformer.isByteArrayNode(arrayBase))
               bstoreiUnknownArrayTypeNodes.insert(currNode);
            }
         else
            bstoreiUnknownArrayTypeNodes.insert(currNode);
         }

      if (currNode->getOpCodeValue() == TR::checkcast
          && currNode->getSecondChild()->getOpCodeValue() == TR::loadaddr
          && currNode->getSecondChild()->getSymbolReference()->isUnresolved())
          {
          unresolvedCheckcastTopsNeedingNullGuard.add(currTree);
          }
      else if (currNode->getOpCodeValue() == TR::treetop
               && currNode->getFirstChild()->getOpCodeValue() == TR::instanceof
               && currNode->getFirstChild()->getSecondChild()->getOpCodeValue() == TR::loadaddr
               && currNode->getFirstChild()->getSecondChild()->getSymbolReference()->isUnresolved())
         {
         unresolvedInstanceofTops.add(currTree);
         }
      else if (_invokeSpecialInterfaceCalls != NULL
               && currNode->getNumChildren() >= 1
               && currNode->getFirstChild()->getOpCode().isCallDirect()
               && !currNode->getFirstChild()->isPotentialOSRPointHelperCall()
               && _invokeSpecialInterfaceCalls->isSet(
                     currNode->getFirstChild()->getByteCodeIndex())
               && !evaluatedInvokeSpecialCalls.contains(currNode->getFirstChild()))
         {
         evaluatedInvokeSpecialCalls.add(currNode->getFirstChild());
         invokeSpecialInterfaceTops.add(currTree);
         }

   // modify the vftChild
   // If the receiver pointer is a simple load of an auto or parm, then clone
   // it rather than incrementing its reference count.  This can prevent the
   // inliner from creating unnecessary temporaries. The special case is when
   // there is a store to the auto or parm between the load of receiver pointer
   //and the virtual function call

   //keep track of the symbol references of auto or parm changed by stores
      if(opcode.isStoreDirect() && opcode.hasSymbolReference())
         {
            TR::SymbolReference *symRef = currNode->getSymbolReference();
            if (symRef && symRef->getSymbol()->isAutoOrParm())
               autoOrParmSymRefList.add(symRef);
         }

      if(opcode.isResolveOrNullCheck())
         {
         TR::Node *firstChild = currNode->getFirstChild();
         opcode = firstChild->getOpCode(); // the first child is indirect call to method
         if (opcode.isCallIndirect()
             && !firstChild->getSymbol()->castToMethodSymbol()->isComputed())
            {
            TR::Node *receiver;
            TR::Node *firstGrandChild = firstChild->getFirstChild(); // firstGrandChild is the vft child
            receiver = firstGrandChild->getFirstChild();
            TR::ILOpCode receiverOpcode = receiver->getOpCode();
            TR::SymbolReference *symRef = NULL;
            if(receiverOpcode.hasSymbolReference() && receiverOpcode.isLoadVarDirect())
               symRef = receiver->getSymbolReference();
            bool canCopyReceiver =symRef && symRef->getSymbol()->isAutoOrParm() && !autoOrParmSymRefList.find(symRef);
            if (canCopyReceiver)
               {
               TR::Node *newReceiver = TR::Node::copy(receiver);
               newReceiver->setReferenceCount(1);
               firstGrandChild->setChild(0, newReceiver);
               receiver->decReferenceCount();
               }
            }
         }

      if (needMonitor && primitive)
         {
         if ((currNode->getOpCode().isStore() &&
            currNode->getSymbol()->holdsMonitoredObject() &&
               !currNode->isLiveMonitorInitStore()) ||  currNode->getOpCode().getOpCodeValue() == TR::monexitfence)
            {
            bool isMonent = currNode->getOpCode().getOpCodeValue() != TR::monexitfence;
            if (isMonent)
               monentStore = currTree;
            else
               monexitStore = currTree;
            }

         if ((currNode->getOpCodeValue() == TR::monexit) || (currNode->getOpCodeValue() == TR::monent))
            {
            if (currNode->getOpCodeValue() == TR::monexit)
               {
               if (numMonexits > 0)
                  {
                  primitive = false;
                  continue;
                  }

               monexitTree = currNode;
               numMonexits++;
               }
            else if (currNode->getOpCodeValue() == TR::monent)
               {
               if (numMonents > 0)
                  {
                  primitive = false;
                  continue;
                  }

               monentTree = currNode;
               numMonents++;
               }
            }
         else if (currNode->getNumChildren() > 0 &&
                  currNode->getFirstChild()->getNumChildren() > 0 &&
                  ((currNode->getFirstChild()->getOpCodeValue() == TR::monexit) || (currNode->getFirstChild()->getOpCodeValue() == TR::monent)))
            {
            if (currNode->getFirstChild()->getOpCodeValue() == TR::monexit)
               {
               if (numMonexits > 0)
                  {
                  primitive = false;
                  continue;
                  }
               monexitTree = currNode->getFirstChild();
               numMonexits++;
               }
            else if (currNode->getFirstChild()->getOpCodeValue() == TR::monent)
               {
               if (numMonents > 0)
                  {
                  primitive = false;
                  continue;
                  }
               monentTree = currNode->getFirstChild();
               numMonents++;
               }
            }
         else if (currNode->exceptionsRaised() != 0        ||
             currNode->canCauseGC())
            {
            primitive = false;
            continue;
            }
         }
      }

   if( needMonitor)
      {
      if (primitive &&
          monentTree &&
          monexitTree &&
          monentStore &&
          monexitStore)
         {
         TR::SymbolReference *replaceSymRef = NULL;
         if (monentStore->getNode()->getFirstChild()->getSymbolReference()->getSymbol()->isAutoOrParm())
            replaceSymRef = monentStore->getNode()->getFirstChild()->getSymbolReference();

         if (replaceSymRef)
            {
            if (monentTree->getFirstChild()->getSymbolReference()->getSymbol()->isAutoOrParm())
               {
               monentTree->getFirstChild()->setSymbolReference(replaceSymRef);
               }

            if (monexitTree->getFirstChild()->getSymbolReference()->getSymbol()->isAutoOrParm())
               {
               monexitTree->getFirstChild()->setSymbolReference(replaceSymRef);
               }

            TR::TreeTop *prev = monentStore->getPrevTreeTop();
            TR::TreeTop *next = monentStore->getNextTreeTop();
            monentStore->getNode()->recursivelyDecReferenceCount();
            prev->join(next);

            prev = monexitStore->getPrevTreeTop();
            next = monexitStore->getNextTreeTop();
            monexitStore->getNode()->recursivelyDecReferenceCount();
            prev->join(next);
            _methodSymbol->setSyncObjectTemp(NULL);
            }
         }
      }

      {
      ListIterator<TR::TreeTop> it(&unresolvedCheckcastTopsNeedingNullGuard);
      for (TR::TreeTop *tree = it.getCurrent(); tree != NULL; tree = it.getNext())
         expandUnresolvedClassCheckcast(tree);
      }
      {
      ListIterator<TR::TreeTop> it(&unresolvedInstanceofTops);
      for (TR::TreeTop *tree = it.getCurrent(); tree != NULL; tree = it.getNext())
         expandUnresolvedClassInstanceof(tree);
      }
      {
      ListIterator<TR::TreeTop> it(&invokeSpecialInterfaceTops);
      for (TR::TreeTop *tree = it.getCurrent(); tree != NULL; tree = it.getNext())
         expandInvokeSpecialInterface(tree);
      }

   if (!bstoreiUnknownArrayTypeNodes.empty() || !bstoreiBoolArrayTypeNodes.empty())
      boolArrayStoreTransformer.perform();

   return true;
   }


TR::Block *
TR_J9ByteCodeIlGenerator::cloneHandler(TryCatchInfo * handlerInfo, TR::Block * firstBlock, TR::Block *lastBlock, TR::Block *lastBlockInMethod, List<TR::Block> *clonedCatchBlocks)
   {
   TR_BlockCloner cloner(cfg());
   handlerInfo->_firstBlock = cloner.cloneBlocks(firstBlock, lastBlock);
   lastBlockInMethod->getExit()->join(handlerInfo->_firstBlock->getEntry());
   handlerInfo->_lastBlock = lastBlockInMethod = cloner.getLastClonedBlock();
   handlerInfo->_catchBlock = cloner.getToBlock(firstBlock);

   TR::Block *cursorBlock = firstBlock;
   while (cursorBlock != lastBlockInMethod)
      {
      clonedCatchBlocks->add(cursorBlock);
      cursorBlock = cursorBlock->getNextBlock();
      }
   clonedCatchBlocks->add(cursorBlock);

   cfg()->addSuccessorEdges(lastBlockInMethod);
   return lastBlockInMethod;
   }



TR::Block *
TR_J9ByteCodeIlGenerator::genExceptionHandlers(TR::Block * lastBlock)
   {
   bool trace = comp()->getOption(TR_TraceILGen);
   _inExceptionHandler = true;
   TR::SymbolReference * catchObjectSymRef = symRefTab()->findOrCreateExcpSymbolRef();
   uint16_t i;
   List<TR::Block> clonedCatchBlocks(comp()->trMemory());

   for (auto handlerInfoIter = _tryCatchInfo.begin(); handlerInfoIter != _tryCatchInfo.end(); ++handlerInfoIter)
      {
      TryCatchInfo & handlerInfo = *handlerInfoIter;
      int32_t firstIndex = handlerInfo._handlerIndex;

      // Two exception data entries can have ranges pointing at the same handler.
      // If the types are different then we have to clone the handler.

      //
      //Partial Inlining - Deal with exception Handlers
      //
      if(_blocksToInline && !_blocksToInline->isInExceptionList(firstIndex))                //Case 1: item is not in exception list, therefore no ilgen to be done on it
         {
         continue; // nothing to be done for this handler!
         }
      else if (
            _blocksToInline
         && _blocksToInline->isInExceptionList(firstIndex)
         && !_blocksToInline->isInList(firstIndex)
         && !isGenerated(firstIndex))   //Case 2: item is in exception list, but not in list of blocks to be ilgen'd
         {
         _blocksToInline->hasGeneratedRestartTree()  ? genGotoPartialInliningCallBack(firstIndex,_blocksToInline->getGeneratedRestartTree()) :
                                                    _blocksToInline->setGeneratedRestartTree(genPartialInliningCallBack(firstIndex,_blocksToInline->getCallNodeTreeTop()));
         handlerInfo._lastBlock =  blocks(firstIndex);
         handlerInfo._firstBlock = blocks(firstIndex);
         handlerInfo._catchBlock = blocks(firstIndex);

         blocks(firstIndex)->setIsAdded();
         if(blocks(firstIndex) != _blocksToInline->getGeneratedRestartTree()->getEnclosingBlock())
            {
            lastBlock->getExit()->join(blocks(firstIndex)->getEntry());
            cfg()->addNode(blocks(firstIndex));
            cfg()->addEdge(blocks(firstIndex),_blocksToInline->getGeneratedRestartTree()->getEnclosingBlock());
            }
         else
            {
            lastBlock->getExit()->join(blocks(firstIndex)->getEntry());
            cfg()->insertBefore(blocks(firstIndex),cfg()->getEnd()->asBlock());
            }
         lastBlock=handlerInfo._lastBlock;
         //ok what I'm trying here is saying that my first block, last block and catchblock in my catcher are all the same (the one block)

         handlerInfo._catchBlock->setHandlerInfo(handlerInfo._catchType, (uint8_t)comp()->getInlineDepth(), handlerInfoIter - _tryCatchInfo.begin(), method(), comp());
         continue;
         }


      bool generateNewBlock = true;
      TR::Block * handlerBlockFromNonExceptionControlFlow = 0;
      if (isGenerated(firstIndex))
         {
         generateNewBlock = false;
         TryCatchInfo * dupHandler = 0;
         for (int32_t j = 0; j < (handlerInfoIter - _tryCatchInfo.begin()); ++j)
            {
            TryCatchInfo & h = _tryCatchInfo[j];
            if (h._handlerIndex == firstIndex)
               {
               if (!dupHandler)
                  dupHandler = &h;
               if (h._catchType == handlerInfo._catchType)
                  {
                  dupHandler = &h;
                  break;
                  }
               }
            }

         if (!dupHandler)
            {
            handlerBlockFromNonExceptionControlFlow = _blocks[firstIndex];
            generateNewBlock = true;

            // this handler must also be reachable from the mainline code....we don't
            // know how to handle this yet
            // todo: figure out how to handle this.
            //
            // TR_ASSERT(dupHandler, "can't figure out why the handler is already generated");
            // comp()->failCompilation<TR::CompilationException>("can't figure out why the handler is already generated");
            }

         if (!generateNewBlock)
            {
            if (handlerInfo._catchType != dupHandler->_catchType)
               {
               lastBlock = cloneHandler(&handlerInfo, dupHandler->_firstBlock, dupHandler->_lastBlock, lastBlock, &clonedCatchBlocks);
               /*
               TR_BlockCloner cloner(cfg());
               handlerInfo->_firstBlock = cloner.cloneBlocks(dupHandler->_firstBlock, dupHandler->_lastBlock);
               lastBlock->getExit()->join(handlerInfo->_firstBlock->getEntry());
               handlerInfo->_lastBlock = lastBlock = cloner.getLastClonedBlock();
               handlerInfo->_catchBlock = cloner.getToBlock(blocks(firstIndex));
               cfg()->addSuccessorEdges(lastBlock);
               */
               }
            else
               handlerInfo._catchBlock = dupHandler->_catchBlock;
            }
         }

      if (generateNewBlock)
         {
         setupBBStartContext(firstIndex);

         TR::SymbolReference *exceptionLoadSymRef = NULL;
         if (handlerBlockFromNonExceptionControlFlow &&
             _stack->topIndex() == 0)
            {
            TR::Node *exceptionLoadOnStack = _stack->top();
            if (exceptionLoadOnStack->getOpCode().isLoadVarDirect() &&
                exceptionLoadOnStack->getOpCode().hasSymbolReference() &&
                exceptionLoadOnStack->getSymbolReference()->getSymbol()->isAutoOrParm())
               {
               exceptionLoadSymRef = exceptionLoadOnStack->getSymbolReference();
               //dumpOptDetails("exc load on stack = %p\n", exceptionLoadOnStack);
               }
            }

         _bcIndex = firstIndex;

         // The stack at the start of a catch block only contains the catch object
         loadSymbol(TR::aload, catchObjectSymRef);
         if (_compilation->getHCRMode() == TR::osr || _compilation->getOSRMode() == TR::involuntaryOSR)
            {
            genTreeTop(TR::Node::createWithSymRef(_block->getEntry()->getNode(), TR::asynccheck, 0,
               symRefTab()->findOrCreateAsyncCheckSymbolRef(_methodSymbol)));

            // Under NextGenHCR, the commoned catch object will now be on the stack. This results
            // in the addition of an unneeded temp if the block is split here. Short cut this by
            // uncommoning them.
            if (_compilation->getHCRMode() == TR::osr)
               {
               pop();
               loadSymbol(TR::aload, catchObjectSymRef);
               }
            }

         /*
          * Spill the exception object into a temporary if it isn't immediately done so
          * in the catch block.  This is necessary because the exception object will not
          * be preserved across method calls (and for a handful of other reasons) and
          * cannot be materialized from the metadata.  A stack temp is necessary in this
          * case.
          */
         uint8_t firstOpCode = _code[_bcIndex];
         int32_t bc = convertOpCodeToByteCodeEnum(firstOpCode);

         if (bc != J9BCastore)
            {
            TR::SymbolReference *exceptionObjectSymRef = symRefTab()->createTemporary(_methodSymbol, TR::Address);
            TR::Node *exceptionNode = pop();
            genTreeTop(TR::Node::createStore(exceptionObjectSymRef, exceptionNode));
            loadConstant(TR::aconst, (void *)0);
            genTreeTop(TR::Node::createStore(exceptionNode->getSymbolReference(), pop()));
            loadSymbol(TR::aload, exceptionObjectSymRef);
            if (trace)
               traceMsg(comp(), "catch block first BC is not an astore, inserting explicit store of exception object\n");
            }

         TR::Node *node = _stack->top();
         node->setIsNonNull(true);

         TR::TreeTop *storeTree = NULL;
         if (handlerBlockFromNonExceptionControlFlow)
            {
            if (!exceptionLoadSymRef)
               {
               comp()->failCompilation<TR::ILGenFailure>("Aborting at generate exception handlers");
               }
            else
               {
               TR::Node *storeNode = TR::Node::createStore(exceptionLoadSymRef, pop());
               storeTree = TR::TreeTop::create(comp(), storeNode);
               }

            TR::Node *lastNode = handlerBlockFromNonExceptionControlFlow->getLastRealTreeTop()->getNode();
            if (lastNode->getOpCode().isResolveOrNullCheck() ||
                (lastNode->getOpCodeValue() == TR::treetop))
               lastNode = lastNode->getFirstChild();

            //traceMsg(comp(), "last node %p bc index %d and opcode %s\n", lastNode, lastNode->getByteCodeIndex(), lastNode->getOpCode().getName());

            TR::ILOpCode &lastOpCode = lastNode->getOpCode();
            if (!lastOpCode.isGoto() &&
                !lastOpCode.isReturn() &&
                (lastOpCode.getOpCodeValue() != TR::athrow))
               {
               comp()->failCompilation<TR::ILGenFailure>("Aborting: not goto, not return and not a throw.");
               }
            }

         if (handlerBlockFromNonExceptionControlFlow)
            {
            lastBlock = cloneHandler(&handlerInfo, handlerBlockFromNonExceptionControlFlow, handlerBlockFromNonExceptionControlFlow, lastBlock, &clonedCatchBlocks);
            lastBlock->prepend(storeTree);
            }
         else
            {
            handlerInfo._lastBlock = walker(lastBlock);
            handlerInfo._firstBlock = lastBlock->getNextBlock();
            handlerInfo._catchBlock = blocks(firstIndex);
            lastBlock = handlerInfo._lastBlock;
            }
         }

      handlerInfo._catchBlock->setHandlerInfo(handlerInfo._catchType, (uint8_t)comp()->getInlineDepth(), handlerInfoIter - _tryCatchInfo.begin(), method(), comp());
      }

   for (auto handlerInfoIter = _tryCatchInfo.begin(); handlerInfoIter != _tryCatchInfo.end(); ++handlerInfoIter)
      {
      TryCatchInfo & handlerInfo = *handlerInfoIter;
      if(_blocksToInline && !_blocksToInline->isInExceptionList(handlerInfo._handlerIndex))                //Case 1: item is not in exception list, therefore no ilgen to be done on it
         {
         continue; // nothing to be done for this handler!
         }
      TR::Block *catchBlock   = handlerInfo._catchBlock;
      TR::Block *restartBlock = _blocksToInline? _blocksToInline->getGeneratedRestartTree()->getEnclosingBlock() : NULL;
      // Checking for a preceding J9BCmonitorenter only make sense when the try region starts at a bytecode index above 0
      if (handlerInfo._startIndex > 0)
         {
         uint8_t precedingOpcode = _code[handlerInfo._startIndex-1];
         TR_J9ByteCode precedingBytecode = convertOpCodeToByteCodeEnum(precedingOpcode);
         uint8_t lastOpcode = _code[handlerInfo._endIndex];
         TR_J9ByteCode lastBytecode = convertOpCodeToByteCodeEnum(lastOpcode);
         if (precedingBytecode == J9BCmonitorenter && lastBytecode == J9BCmonitorexit)
            catchBlock->setIsSynchronizedHandler(); // monitorenter preceding try region that ends in monitorexit means synchronized
         }

      for (int32_t j = handlerInfo._startIndex; j <= handlerInfo._endIndex; ++j)
         if (blocks(j) && cfg()->getNodes().find(blocks(j)))
            {
            if (blocks(j) == catchBlock)
               {
               _methodSymbol->setMayHaveNestedLoops(true);
               }

            if (blocks(j) != restartBlock)
               {
               cfg()->addExceptionEdge(blocks(j), catchBlock);
               ListIterator<TR::Block> clonedIt(&clonedCatchBlocks);
               for (TR::Block * cb = clonedIt.getFirst(); cb; cb = clonedIt.getNext())
                  {
                  if (cb->getEntry()->getNode()->getByteCodeIndex() == j)
                     cfg()->addExceptionEdge(cb, catchBlock);
                  }
               }
            }
      }

   _inExceptionHandler = false;
   return lastBlock;
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genMethodEnterHook()
   {
   if (method()->isStatic())
      return TR::Node::createWithSymRef(TR::MethodEnterHook, 0, symRefTab()->findOrCreateReportStaticMethodEnterSymbolRef(_methodSymbol));

   loadAuto(TR::Address, 0);
   return TR::Node::createWithSymRef(TR::MethodEnterHook, 1, 1, pop(), symRefTab()->findOrCreateReportMethodEnterSymbolRef(_methodSymbol));
   }

void
TR_J9ByteCodeIlGenerator::prependEntryCode(TR::Block * firstBlock)
   {
   bool trace = comp()->getOption(TR_TraceILGen);
   TR::Node * monitorEnter = 0;
   TR::Node * syncObjectStore = 0;
   TR::Node * thisObjectStore = 0;
   TR::TreeTop * nhrttCheckTree1 = 0, * nhrttCheckTree2 = 0, * nhrttCheckTree3 = 0, * nhrttCheckTree4 = 0;
   TR::Node * lockObject = 0;
   if (_methodSymbol->isSynchronised())
      {
      loadMonitorArg();

      TR::Node * firstChild = pop();
      TR::SymbolReference * monEnterSymRef = symRefTab()->findOrCreateMethodMonitorEntrySymbolRef(_methodSymbol);

      if (firstChild->getOpCodeValue() == TR::loadaddr && firstChild->getSymbol()->isClassObject())
         {
         monitorEnter = TR::Node::createWithSymRef(TR::aloadi, 1, 1, firstChild, symRefTab()->findOrCreateJavaLangClassFromClassSymbolRef());
         monitorEnter = TR::Node::createWithSymRef(TR::monent, 1, 1, monitorEnter, monEnterSymRef);
         }
      else
         {
         monitorEnter = TR::Node::createWithSymRef(TR::monent, 1, 1, firstChild, monEnterSymRef);
         }

      monitorEnter->setSyncMethodMonitor(true);
      TR_OpaqueClassBlock * owningClass = _methodSymbol->getResolvedMethod()->containingClass();
      if (owningClass != comp()->getObjectClassPointer())
         {
         monitorEnter->setSecond((TR::Node*)owningClass);
         if (trace)
            traceMsg(comp(), "setting class for %p to be %p\n", monitorEnter, owningClass);
         }

      _methodSymbol->setMayContainMonitors(true);

      if (_methodSymbol->isStatic())
         monitorEnter->setStaticMonitor(true);

      // Store the receiver object to a temporary to deal with the case where the bytecode has been hacked
      // to write to the receiver.  The temporary will be used on the monitor exit
      //
      if (_methodSymbol->getSyncObjectTemp())
         {
         if (_methodSymbol->isStatic())
            loadSymbol(TR::loadaddr, symRefTab()->findOrCreateClassSymbol(_methodSymbol, 0, method()->containingClass()));
         else
            loadAuto(TR::Address, 0);

         lockObject = pop();
         if (monitorEnter->getFirstChild()->getOpCodeValue() == TR::aloadi &&
               monitorEnter->getFirstChild()->getSymbolReference() == symRefTab()->findJavaLangClassFromClassSymbolRef())
            lockObject = monitorEnter->getFirstChild();
         syncObjectStore = TR::Node::createStore(_methodSymbol->getSyncObjectTemp(), lockObject);
         }
      }

   // if the receiver of Object.<init> has been written into, then we need to save a copy of the receiver into
   // a temporary and use that in the call to the finalizer
   //
   if (_methodSymbol->getThisTempForObjectCtor())
      {
      loadAuto(TR::Address, 0);
      thisObjectStore = TR::Node::createStore(_methodSymbol->getThisTempForObjectCtor(), pop());
      }

   TR::Node * methodEnterHook = 0;

   static const char* disableMethodHookForCallees = feGetEnv("TR_DisableMethodHookForCallees");
   if ((fej9()->isMethodTracingEnabled(_methodSymbol->getResolvedMethod()->getPersistentIdentifier())
        || (!comp()->getOption(TR_FullSpeedDebug)
            && TR::Compiler->vm.canMethodEnterEventBeHooked(comp())))
       && (isOutermostMethod() || !disableMethodHookForCallees))
      {
      methodEnterHook = genMethodEnterHook();
      }

   if (methodEnterHook || monitorEnter)
      {
      // If there's a branch to the first byte code then we have to prepend
      // the monitor enter and/or entry hook into its own block
      //
      if ((firstBlock->getPredecessors().size() > 1) || !isOutermostMethod())
         firstBlock = _methodSymbol->prependEmptyFirstBlock();

      if (methodEnterHook)
         firstBlock->prepend(TR::TreeTop::create(comp(), methodEnterHook));

      TR::TreeTop *syncObjectTT = NULL;
      if (syncObjectStore)
         syncObjectTT = TR::TreeTop::create(comp(), syncObjectStore);

      if (monitorEnter)
         firstBlock->prepend(TR::TreeTop::create(comp(), monitorEnter));

      if (nhrttCheckTree3)
         firstBlock->prepend(nhrttCheckTree3);

      if (nhrttCheckTree2)
         firstBlock->prepend(nhrttCheckTree2);

      if (nhrttCheckTree1)
         firstBlock->prepend(nhrttCheckTree1);

      // the sync object store must be the first thing here, or everything above it will load a bad object reference
      if (syncObjectTT)
         firstBlock->prepend(syncObjectTT);
      }

   if (thisObjectStore)
      {
      if (nhrttCheckTree4)
         firstBlock->prepend(nhrttCheckTree4);
      firstBlock->prepend(TR::TreeTop::create(comp(), thisObjectStore));
      }

   if (comp()->isDLT() && isOutermostMethod())
      {
      genDLTransfer(firstBlock);
      }
   }

void
TR_J9ByteCodeIlGenerator::prependGuardedCountForRecompilation(TR::Block * originalFirstBlock)
   {
   //
   // guardBlock:           if (!countForRecompile) goto originalFirstBlock;
   // bumpCounterBlock:     bodyInfo->count--;
   //                       if (bodyInfo->count > 0) goto originalFirstBlock;
   // callRecompileBlock:   call jitRetranslateCallerWithPreparation(j9method, startPC);
   //                       bodyInfo->count=10000
   // originalFirstBlock:   ...
   //

   bool trace = comp()->getOption(TR_TraceILGen);
   TR::TreeTop *originalFirstTree = _methodSymbol->getFirstTreeTop();
   TR::Node *node=originalFirstTree->getNode();

   // construct guard
   TR::Block *guardBlock = TR::Block::createEmptyBlock(comp());
   TR::Node *cmpFlagNode;
   if (comp()->getOption(TR_ImmediateCountingRecompilation))
      {
      cmpFlagNode = TR::Node::createif(TR::ificmpeq, TR::Node::iconst(1234), TR::Node::iconst(5678), originalFirstBlock->getEntry());
      }
   else
      {
      TR::Node *loadFlagNode = TR::Node::createWithSymRef(node, TR::iload, 0, comp()->getSymRefTab()->findOrCreateCountForRecompileSymbolRef());

      if (comp()->getOption(TR_EnableGCRPatching))
         cmpFlagNode = TR::Node::createif(TR::ificmpne, loadFlagNode, TR::Node::create(node, TR::iconst, 0, 1), originalFirstBlock->getEntry());
      else
         cmpFlagNode = TR::Node::createif(TR::ificmpeq, loadFlagNode, TR::Node::create(node, TR::iconst, 0, 0), originalFirstBlock->getEntry());
      }
   TR::TreeTop *cmpFlag = TR::TreeTop::create(comp(), cmpFlagNode);
   guardBlock->append(cmpFlag);
   TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "gcrMethods/byJittedBody/(%s)", comp()->signature()), cmpFlag, 1, TR::DebugCounter::Moderate);


   // construct counter bump
   TR::Block *bumpCounterBlock = TR::Block::createEmptyBlock(comp());
   TR::TreeTop * treeTop = TR::TreeTop::createIncTree(comp(), node, comp()->getRecompilationInfo()->getCounterSymRef(),
                                                    -comp()->getOptions()->getGCRDecCount(), NULL, true);
   bumpCounterBlock->append(treeTop);
   TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "gcrCounterBumps/byJittedBody/(%s)", comp()->signature()), treeTop, 1, TR::DebugCounter::Free);
   TR::Node *bumpNode = treeTop->getNode()->getNumChildren() > 1 ? treeTop->getNode()->getSecondChild() : treeTop->getNode()->getFirstChild();

   TR::Node *cmpCountNode = TR::Node::createif(TR::ificmpgt, bumpNode, TR::Node::create(TR::iconst, 0, 0), originalFirstBlock->getEntry());
   bumpCounterBlock->append(TR::TreeTop::create(comp(), cmpCountNode));
   bumpCounterBlock->setIsCold(true);
   bumpCounterBlock->setFrequency(UNKNOWN_COLD_BLOCK_COUNT);


   // construct call block
   TR::Block *callRecompileBlock = TR::Block::createEmptyBlock(comp());
   callRecompileBlock->append(TR::TreeTop::createResetTree(comp(), node, comp()->getRecompilationInfo()->getCounterSymRef(),
                                                          comp()->getOptions()->getGCRResetCount(), NULL, true));

   // Create the instruction that will patch my cmp
   if (comp()->getOption(TR_EnableGCRPatching))
      {
      TR::Node *constNode = TR::Node::create(node, TR::bconst, 0);
      constNode->setByte(2);
      callRecompileBlock->append(TR::TreeTop::create(comp(), TR::Node::createWithSymRef(TR::bstore, 1, 1, constNode,
                             comp()->getSymRefTab()->findOrCreateGCRPatchPointSymbolRef())));
      }

   TR::TreeTop *callTree = TR::TransformUtil::generateRetranslateCallerWithPrepTrees(node, TR_PersistentMethodInfo::RecompDueToGCR, comp());
   callRecompileBlock->append(callTree);
   callRecompileBlock->setIsCold(true);
   callRecompileBlock->setFrequency(UNKNOWN_COLD_BLOCK_COUNT);

      // get all the blocks into the CFG
   if (trace) traceMsg(comp(), "adding edge start to guard\n");
   cfg()->addEdge(cfg()->getStart(), guardBlock);

   if (trace) traceMsg(comp(), "insert before guard to bump\n");
   cfg()->insertBefore(guardBlock, bumpCounterBlock);
   if (trace) traceMsg(comp(), "insert before bump to call\n");
   cfg()->insertBefore(bumpCounterBlock, callRecompileBlock);
   if (trace) traceMsg(comp(), "insertbefore call to original\n");
   cfg()->insertBefore(callRecompileBlock, originalFirstBlock);

   if (trace) traceMsg(comp(), "remove start to original\n");
   cfg()->removeEdge(cfg()->getStart(), originalFirstBlock);
   if (trace) traceMsg(comp(), "set first\n");
   _methodSymbol->setFirstTreeTop(guardBlock->getEntry());

   comp()->getRecompilationInfo()->getJittedBodyInfo()->setUsesGCR();
   }

void
TR_J9ByteCodeIlGenerator::createGeneratedFirstBlock()
   {
   _block = TR::Block::createEmptyBlock(comp());
   cfg()->addNode(_block);
   cfg()->addEdge(cfg()->getStart(), _block);
   cfg()->addEdge(_block, cfg()->getEnd());
   _methodSymbol->setFirstTreeTop(_block->getEntry());
   }

bool
TR_J9ByteCodeIlGenerator::hasFPU()
   {
   bool result = !comp()->getOption(TR_DisableFPCodeGen) ? comp()->target().cpu.hasFPU() : false;
   return result;
   }


bool
TR_J9ByteCodeIlGenerator::genJNIIL()
   {
   if (!cg()->getSupportsDirectJNICalls() || comp()->getOption(TR_DisableDirectToJNI) || (comp()->compileRelocatableCode() && !cg()->supportsDirectJNICallsForAOT()))
      return false;

   // A JNI thunk method cannot call back to the slow path, as would occur in the following case
   if (method()->numberOfParameterSlots() > J9_INLINE_JNI_MAX_ARG_COUNT && comp()->cg()->hasFixedFrameC_CallingConvention())
      return false;

   if (_methodSymbol->getRecognizedMethod() == TR::sun_misc_Unsafe_ensureClassInitialized)
      {
      return false;
      }


   bool HasFPU = hasFPU();
#if defined(__ARM_PCS_VFP)
   HasFPU = false;
#endif

   if (!HasFPU && (method()->returnOpCode() == TR::freturn || method()->returnOpCode() == TR::dreturn))
      return false;

   if (!HasFPU)
      {
      for (uint32_t i = 0; i < method()->numberOfParameterSlots(); ++i)
         {
            if (method()->parmType(i) == TR::Float || method()->parmType(i) == TR::Double)
               return false;
         }
      }

   if (debug("traceInfo"))
      diagnostic("Compiling JNI virtual thunk for %s.\n", method()->signature(trMemory()));

   createGeneratedFirstBlock();

   _methodSymbol->setJNI();

   ListIterator<TR::ParameterSymbol> parms(&_methodSymbol->getParameterList());
   for (TR::ParameterSymbol * p = parms.getFirst(); p; p = parms.getNext())
      loadAuto(p->getDataType(), p->getSlot());

   TR::MethodSymbol::Kinds callKind = method()->isStatic() ? TR::MethodSymbol::Static : TR::MethodSymbol::Virtual;

   TR::SymbolReference * callSymRef =
      symRefTab()->findOrCreateMethodSymbol(_methodSymbol->getResolvedMethodIndex(), -1, method(), callKind);

   genInvokeDirect(callSymRef);

   bool genMonitors = _methodSymbol->isSynchronised();

   genReturn(method()->returnOpCode(), genMonitors);

   prependEntryCode(_block);

   return true;
   }

void
TR_J9ByteCodeIlGenerator::genHWOptimizedStrProcessingAvailable()
   {
   static int32_t constToLoad = -1;
   initialize();
   int32_t firstIndex = _bcIndex;
   setIsGenerated(_bcIndex);
   if (constToLoad == -1)
      {
      if (cg()->getSupportsInlineStringCaseConversion())
         constToLoad = 1;
      else
         constToLoad = 0;
      }

   loadConstant(TR::iconst, constToLoad);

   setIsGenerated(++_bcIndex);
   _bcIndex = genReturn(method()->returnOpCode(), method()->isSynchronized());
   TR::Block * block = blocks(firstIndex);
   cfg()->addEdge(cfg()->getStart(), block);
   block->setVisitCount(_blockAddedVisitCount);
   block->getExit()->getNode()->copyByteCodeInfo(block->getLastRealTreeTop()->getNode());
   cfg()->insertBefore(block, 0);
   _bcIndex = 0;
   _methodSymbol->setFirstTreeTop(blocks(0)->getEntry());
   prependEntryCode(blocks(0));

   dumpOptDetails(comp(), "\tOverriding default return value with %d.\n", constToLoad);
   }

void
TR_J9ByteCodeIlGenerator::genJITIntrinsicsEnabled()
   {
   bool isZLinux = comp()->target().cpu.isZ() && comp()->target().isLinux();

   static int32_t constToLoad = (comp()->target().isZOS() || isZLinux) &&
           !comp()->getOption(TR_DisablePackedDecimalIntrinsics) ? 1 : 0;

   initialize();
   int32_t firstIndex = _bcIndex;
   setIsGenerated(_bcIndex);

   loadConstant(TR::iconst, constToLoad);

   setIsGenerated(++_bcIndex);
   _bcIndex = genReturn(method()->returnOpCode(), method()->isSynchronized());
   TR::Block * block = blocks(firstIndex);
   cfg()->addEdge(cfg()->getStart(), block);
   block->setVisitCount(_blockAddedVisitCount);
   block->getExit()->getNode()->copyByteCodeInfo(block->getLastRealTreeTop()->getNode());
   cfg()->insertBefore(block, 0);
   _bcIndex = 0;
   _methodSymbol->setFirstTreeTop(blocks(0)->getEntry());
   prependEntryCode(blocks(0));

   dumpOptDetails(comp(), "\tOverriding default return value with %d.\n", constToLoad);
   }

void
TR_J9ByteCodeIlGenerator::genIsORBDeepCopyAvailable()
   {
   static int32_t constToLoad = 1;
   initialize();
   int32_t firstIndex = _bcIndex;
   setIsGenerated(_bcIndex);

   loadConstant(TR::iconst, constToLoad);

   setIsGenerated(++_bcIndex);
   _bcIndex = genReturn(method()->returnOpCode(), method()->isSynchronized());
   TR::Block * block = blocks(firstIndex);
   cfg()->addEdge(cfg()->getStart(), block);
   block->setVisitCount(_blockAddedVisitCount);
   block->getExit()->getNode()->copyByteCodeInfo(block->getLastRealTreeTop()->getNode());
   cfg()->insertBefore(block, 0);
   _bcIndex = 0;
   _methodSymbol->setFirstTreeTop(blocks(0)->getEntry());
   prependEntryCode(blocks(0));

   dumpOptDetails(comp(), "\tOverriding default return value with %d.\n", constToLoad);
   }


void
TR_J9ByteCodeIlGenerator::genJavaLangSystemIdentityHashCode()
   {
   TR::Block * ifBlock, * return0Block, * computeBlock;

   //printf("Transforming TR::java_lang_System_identityHashCode in %s\n", comp()->signature());

   // the object parameter
   ListIterator<TR::ParameterSymbol> parms(&_methodSymbol->getParameterList());
   TR::ParameterSymbol * p = parms.getFirst();
   TR::Node * objectRef = TR::Node::createLoad(symRefTab()->findOrCreateAutoSymbol(_methodSymbol, p->getSlot(), p->getDataType()));

   // ifblock
   _block = ifBlock = TR::Block::createEmptyBlock(comp());
   _methodSymbol->setFirstTreeTop(ifBlock->getEntry());

   loadAuto(p->getDataType(),p->getSlot());
   loadConstant(TR::aconst, 0);
   TR::Node * second = pop();
   TR::Node * first = pop();

   computeBlock = TR::Block::createEmptyBlock(comp());

   genTreeTop(TR::Node::createif(TR::ifacmpne, first, second, computeBlock->getEntry()));

   // return0 block
   _block = return0Block = TR::Block::createEmptyBlock(comp());
   loadConstant(TR::iconst, 0);
   genTreeTop(TR::Node::create(method()->returnOpCode(), 1, pop()));

   // compute block
   _block = computeBlock;

   TR::Node                 *objHeaderFlagsField;
   TR::Node                 *opNode;
   TR::Node                 *constNode;
   TR::Node                   *shlNode;
   TR::Node                 *orNode;
   TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();

   // object header flags now occupy 4bytes (instead of 8) on 64-bit
   //
   objHeaderFlagsField = TR::Node::createWithSymRef(TR::iloadi, 1, 1, objectRef,
                                            symRefTab->findOrCreateHeaderFlagsSymbolRef());
   opNode = TR::Node::create(TR::ishr, 2, objHeaderFlagsField, TR::Node::create(objHeaderFlagsField,
         TR::iconst, 0, 16));

   opNode = TR::Node::create(TR::iand, 2, opNode, TR::Node::create(opNode, TR::iconst, 0, 32767));
   shlNode = TR::Node::create(TR::ishl, 2, opNode, TR::Node::create(opNode, TR::iconst, 0, 16));

   orNode = TR::Node::create(TR::ior, 2, opNode, shlNode);

   TR::Node *trTreeTopNode = TR::Node::create(TR::treetop, 1, orNode);

   computeBlock->append(TR::TreeTop::create(comp(), trTreeTopNode));

   push(orNode);
   genTreeTop(TR::Node::create(method()->returnOpCode(), 1, pop()));

   cfg()->addEdge(cfg()->getStart(), ifBlock);

   cfg()->insertBefore(ifBlock, return0Block);
   cfg()->insertBefore(return0Block, computeBlock);
   cfg()->insertBefore(computeBlock, 0);
   }

bool
TR_J9ByteCodeIlGenerator::genNewInstanceImplThunk()
   {
   // Answers a new instance of the class represented by the
   // receiver, created by invoking the default (i.e. zero-argument)
   // constructor. If there is no such constructor, or if the
   // creation fails (either because of a lack of available memory or
   // because an exception is thrown by the constructor), an
   // InstantiationException is thrown. If the default constructor
   // exists, but is not accessible from the context where this
   // message is sent, an IllegalAccessException is thrown.
   //
   if (debug("traceInfo"))
      diagnostic("Compiling newInstanceImpl thunk: %s.\n", method()->signature(trMemory()));

   if (comp()->getRecompilationInfo())
      {
      comp()->getRecompilationInfo()->preventRecompilation();

      // Disable Sampling
      TR_PersistentJittedBodyInfo *bodyInfo = comp()->getRecompilationInfo()->getJittedBodyInfo();
      if (bodyInfo)
         bodyInfo->setDisableSampling(true);
      }

   TR_OpaqueClassBlock *classId = method()->classOfMethod(); // will never be java.lang.Class (unless we are in a hacky world, e.g. JarTester?)

   TR_ResolvedMethod *ctor = fej9()->getDefaultConstructor(trMemory(), classId);
   if (!ctor || TR::Compiler->cls.isAbstractClass(comp(), classId)) return false;

   TR::Block * firstBlock, * initBlock, * catchAllBlock;

   _block = firstBlock = TR::Block::createEmptyBlock(comp());
   cfg()->addEdge(cfg()->getStart(), firstBlock);
   _methodSymbol->setFirstTreeTop(firstBlock->getEntry());

   ListIterator<TR::ParameterSymbol> parms(&_methodSymbol->getParameterList());
   TR::ParameterSymbol *p0 = parms.getFirst();// this class
   TR::ParameterSymbol *p1 = parms.getNext(); // caller class

   // HACK HACK HACK
   p0->setReferencedParameter();

   if (!((TR_J9VM *)fej9())->isPublicClass(classId) || !ctor->isPublic())
      {
      TR::SymbolReference * accessCheckSymRef =
         symRefTab()->findOrCreateRuntimeHelper(TR_newInstanceImplAccessCheck, true, true, true);

      loadConstant(TR::aconst, ctor->getPersistentIdentifier()); // Nullary Constructor's identifier
      loadAuto(p1->getDataType(), p1->getSlot());                                // Caller Class
      //the call to findOrCreateClassSymbol is safe even though we pass CPI of -1 since it is guarded by !isAOT check in createResolvedMethodWithSignature
      loadSymbol(TR::loadaddr, symRefTab()->findOrCreateClassSymbol(_methodSymbol, -1, classId)); // This Class

      TR::Node* node = pop();
      node = TR::Node::createWithSymRef(TR::aloadi, 1, 1, node, symRefTab()->findOrCreateJavaLangClassFromClassSymbolRef());
      push(node);

      genTreeTop(genNodeAndPopChildren(TR::call, 3, accessCheckSymRef));
      }
   //the call to findOrCreateClassSymbol is safe even though we pass CPI of -1 since it is guarded by !isAOT check in createResolvedMethodWithSignature
   loadSymbol(TR::loadaddr, symRefTab()->findOrCreateClassSymbol(_methodSymbol, -1, classId));
   genNew();
   TR::SymbolReference * tempSymRef = symRefTab()->findOrCreatePendingPushTemporary(_methodSymbol, 0, TR::Address);
   genTreeTop(TR::Node::createStore(tempSymRef, pop()));

   _block = initBlock = TR::Block::createEmptyBlock(comp());

   push(TR::Node::createLoad(tempSymRef));
   dup();
   genInvokeDirect(symRefTab()->findOrCreateMethodSymbol(JITTED_METHOD_INDEX, -1, ctor, TR::MethodSymbol::Special));
   _methodSymbol->setMayHaveInlineableCall(true);
   genTreeTop(TR::Node::create(method()->returnOpCode(), 1, pop()));

   cfg()->insertBefore(firstBlock, initBlock);
   cfg()->insertBefore(initBlock, 0);

   return true;
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genNewInstanceImplCall(TR::Node *classNode)
   {
   TR_ASSERT(_methodSymbol->getRecognizedMethod() == TR::java_lang_Class_newInstance,
          "should only be finding a call to newInstanceImpl from newInstance");
   TR_ResolvedMethod *caller = method()->owningMethod(); // the caller of Class.newInstance()
   TR_ASSERT(caller, "should only be transforming newInstanceImpl call if newInstance is being inlined");

   TR::Node *classNodeAsClass = TR::Node::createWithSymRef(TR::aloadi, 1, 1, classNode, symRefTab()->findOrCreateClassFromJavaLangClassSymbolRef());
   //the call to findOrCreateClassSymbol is safe even though we pass CPI of -1 since we check for !compileRelocatableCode() in the caller
   TR::Node *node         = TR::Node::createWithSymRef(TR::loadaddr, 0, symRefTab()->findOrCreateClassSymbol(_methodSymbol, -1, caller->classOfMethod()));
   TR::Node *nodeAsObject = TR::Node::createWithSymRef(TR::aloadi, 1, 1, node, symRefTab()->findOrCreateJavaLangClassFromClassSymbolRef());

   TR::Node *callNode = TR::Node::createWithSymRef(TR::acalli, 3, 3,
                     classNodeAsClass,
                     classNode,
                     nodeAsObject,
                     symRefTab()->findOrCreateObjectNewInstanceImplSymbol(_methodSymbol));

   return callNode;
   }

void
TR_J9ByteCodeIlGenerator::genDLTransfer(TR::Block *firstBlock)
   {
   TR::Block  *newFirstBlock;
   IndexPair *innermostPair = NULL, *outmostPair = NULL, *ip;
   TR::SymbolReference *dltSteerSymRef = NULL;
   TR::TreeTop *steerTarget, *storeDltSteer = NULL;
   int32_t    dltBCIndex = comp()->getDltBcIndex(), nestedCnt = 0;

   // if we've DLTed into a non-empty object constructor (an evil bytecode modifier may have caused this),
   // then we will be broken because the we have not initialized the temp slot to use in the
   // call to jitCheckIfFinalize. in most cases this should be a non-issue because we almost always
   // inline Object.<init>
   //
   if (_methodSymbol->getResolvedMethod()->isNonEmptyObjectConstructor())
      {
      comp()->failCompilation<TR::ILGenFailure>("DLT into a non-empty Object constructor");
      }

   for (ip = _backwardBranches.getFirst(); ip; ip = ip->getNext())
      {
      if (ip->_toIndex>dltBCIndex || ip->_fromIndex<dltBCIndex)
         continue;
      nestedCnt++;
      if (innermostPair == NULL)
         innermostPair = ip;
      outmostPair = ip;
      }

   // if we are not within loops or we start the only loop, we can jump to the particular bytecode directly
   if (nestedCnt>1 || (innermostPair!=NULL && dltBCIndex!=innermostPair->_toIndex))
      {
      dltSteerSymRef = symRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Int32);

      TR::Node *storeNode = TR::Node::createWithSymRef(TR::istore, 1, 1, TR::Node::create(TR::iconst, 0, 0), dltSteerSymRef);
      storeDltSteer = TR::TreeTop::create(comp(), storeNode);
      }

   // usually we should be at block starting point, but deal with middle of block anyway
   if (_blocks[dltBCIndex] != NULL)
      {
      if (dltSteerSymRef != NULL)
         _blocks[dltBCIndex]->prepend(storeDltSteer);
      steerTarget = _blocks[dltBCIndex]->getEntry();
      }
   else
      {
      TR::Block *myBlock = NULL;
      for (int32_t i=dltBCIndex-1; i>=0 && (myBlock=_blocks[i])==NULL; i--)
         ;
      TR_ASSERT(myBlock!=NULL, "Cannot find the block DLT point belongs to");

      for (steerTarget=myBlock->getEntry(); steerTarget!=myBlock->getExit() && steerTarget->getNode()->getByteCodeIndex()!=dltBCIndex; steerTarget=steerTarget->getNextTreeTop())
         ;
      TR_ASSERT(steerTarget->getNode()->getByteCodeIndex()==dltBCIndex, "Cannot find the treeTop DLT point belongs to");

      if (steerTarget == myBlock->getEntry())
         {
         if (dltSteerSymRef != NULL)
            myBlock->prepend(storeDltSteer);
         }
      else
         {
         // FIXME: fixupCommoning or copyExceptionSuccessor need to be revisited
         myBlock = myBlock->split(steerTarget, cfg());
         if (dltSteerSymRef != NULL)
            myBlock->prepend(storeDltSteer);
         steerTarget = myBlock->getEntry();
         }
      }

   TR::TreeTop **haveSeenTargets = (TR::TreeTop **)comp()->trMemory()->allocateMemory(sizeof(void *) * (nestedCnt+1), heapAlloc);
   haveSeenTargets[0] = steerTarget;
   int32_t     haveSeenCount = 1;
   for (; innermostPair != NULL && innermostPair != outmostPair->getNext(); innermostPair = innermostPair->getNext())
      {
      if (innermostPair->_fromIndex < dltBCIndex || innermostPair->_toIndex > dltBCIndex)
         continue;
      TR::Block *loopStart = _blocks[innermostPair->_toIndex];
      TR::TreeTop *entryTP = loopStart->getEntry();
      int32_t seenIndex;

      // Handle multiple loops having the same _toIndex
      for (seenIndex=0; seenIndex < haveSeenCount && entryTP != haveSeenTargets[seenIndex]; seenIndex++);
      if (seenIndex < haveSeenCount)
         continue;
      loopStart->split(entryTP->getNextTreeTop(), cfg());
      TR::Node *childNode = TR::Node::createWithSymRef(TR::iload, 0, dltSteerSymRef);
      TR::Node *constNode = TR::Node::create(TR::iconst, 0, 0);
      TR::Node *ifNode = TR::Node::createif(TR::ificmpne, childNode, constNode, steerTarget);
      loopStart->prepend(TR::TreeTop::create(comp(), ifNode));
      cfg()->addEdge(loopStart, steerTarget->getNode()->getBlock());
      steerTarget = entryTP;
      haveSeenTargets[haveSeenCount++] = steerTarget;
      }

   // Transfer the data in
   comp()->setDltSlotDescription(fej9()->getCurrentLocalsMapForDLT(comp()));
   newFirstBlock = _methodSymbol->prependEmptyFirstBlock();

   if (dltSteerSymRef != NULL)
      {
      TR::Node *storeNode = TR::Node::createWithSymRef(TR::istore, 1, 1, TR::Node::create(TR::iconst, 0, 1), dltSteerSymRef);
      storeDltSteer = TR::TreeTop::create(comp(), storeNode);
      newFirstBlock->append(storeDltSteer);
      }

   bool isSyncMethod=_methodSymbol->isSynchronised(), isStaticMethod=_methodSymbol->isStatic();
   int32_t  tempSlots = _method->numberOfTemps();
   int32_t  parmSlots = _method->numberOfParameterSlots();
   int32_t *slotsDescription = comp()->getDltSlotDescription();
   int32_t  parmIteratorIdx = 0;
   TR::SymbolReference *shadowSymRef = symRefTab()->findOrCreateGenericIntShadowSymbolReference(0);
   TR::Node *dltBufChild = NULL;

   symRefTab()->aliasBuilder.gcSafePointSymRefNumbers().set(shadowSymRef->getReferenceNumber());

   if (parmSlots != 0 || tempSlots != 0 || (isSyncMethod && !isStaticMethod))
      {
      int32_t fixedOffset;

      dltBufChild = TR::Node::createWithSymRef(TR::loadaddr, 0, symRefTab()->findOrCreateDLTBlockSymbolRef());

      fixedOffset = fej9()->getDLTBufferOffsetInBlock();
      if (comp()->target().is64Bit())
         {
         TR::Node  *constNode = TR::Node::create(TR::lconst, 0);
         constNode->setLongInt(fixedOffset);
         dltBufChild = TR::Node::create(TR::aladd, 2, dltBufChild, constNode);
         }
      else
         dltBufChild = TR::Node::create(TR::aiadd, 2, dltBufChild, TR::Node::create(TR::iconst, 0, fixedOffset));
      dltBufChild = TR::Node::createWithSymRef(TR::aloadi, 1, 1, dltBufChild, shadowSymRef);
      }

   if (isSyncMethod && !isStaticMethod)
      {
      int32_t *first32Slots = slotsDescription;
      TR::SymbolReference *syncObjectSymRef = _methodSymbol->getSyncObjectTemp();
      bool slot0Modified = (syncObjectSymRef!=NULL);
      bool slot0StillActive = (*first32Slots) & 1;

      parmIteratorIdx = 1;

      // We have 4 scenarios to deal with:
      // slot0StillActive:  no matter if slot0Modified or not, we cannot touch slot0 itself.
      //                    By and large, these should account for the majority cases for DLT.
      //                    There is no difference from normal JIT in these cases;
      //                    For modified case, we need to refresh the syncObjectTemp;
      // slot0 not active and notModified either:
      //                    This is the most likely error scenario we can encounter. For exception
      //                    handling purpose, we have to reinstate slot0, and we cannot go wrong.
      // slot0 not active but modified:
      //                    We cannot do much about it until METADATA can carry the receiver info
      //                    back to the exception handler. For now, we just bail out of the compilation.
      //

      if (!slot0StillActive && slot0Modified)
         {
         comp()->failCompilation<TR::ILGenFailure>("DLT on a tampered method, need better MetaData to deal with it");
         }

      // the syncObjectTemp cannot be relied on anymore, since its store is by-passed
      // Copy the hidden slot to our syncObjectTemp
      //
      TR::Node *loadHiddenSlot = NULL;
      if (slot0Modified)
         {
         loadHiddenSlot = TR::Node::createWithSymRef(TR::aloadi, 1, 1, dltBufChild, shadowSymRef);

         TR::Node *storeSyncObject = TR::Node::createWithSymRef(TR::astore, 1, 1, loadHiddenSlot, syncObjectSymRef);
         newFirstBlock->append(TR::TreeTop::create(comp(), storeSyncObject));
         }

      if (!slot0StillActive && !slot0Modified)
         {
         List<TR::SymbolReference> &slot0list = _methodSymbol->getAutoSymRefs(0);
         ListIterator<TR::SymbolReference> s0i(&slot0list);
         TR::SymbolReference *slot0SymRef = s0i.getFirst();

         TR_ASSERT(slot0list.isSingleton(), "Unexpected use of slot 0");
         TR_ASSERT(slot0SymRef->getSymbol()->getDataType() == TR::Address, "This is not an address type");

         if (loadHiddenSlot == NULL)
            loadHiddenSlot = TR::Node::createWithSymRef(TR::aloadi, 1, 1, dltBufChild, shadowSymRef);

         // Reinstate slot0
         *first32Slots |= 1;

         // Defect 121354: this store cannot be eliminated, since stackWalker needs it.
         slot0SymRef->getSymbol()->setReinstatedReceiver();
         TR::Node *storeSlot0 = TR::Node::createWithSymRef(TR::astore, 1, 1, loadHiddenSlot, slot0SymRef);
         newFirstBlock->append(TR::TreeTop::create(comp(), storeSlot0));
         }
      }

   // Defect 131382: we need to make sure parameter GC map right for dead parameters revived by JIT
   for (; parmIteratorIdx<parmSlots; parmIteratorIdx++)
      {
      List<TR::SymbolReference> &slotlist = _methodSymbol->getAutoSymRefs(parmIteratorIdx);
      ListIterator<TR::SymbolReference> si(&slotlist);
      TR::SymbolReference *parmSymRef = si.getFirst(), *slotSymRef;
      TR::ParameterSymbol *parmSym = NULL;
      int32_t            *currentSlots = slotsDescription + (parmIteratorIdx/32);
      TR::Node            *addrNode = NULL;
      int32_t             nodeRealOffset = -1;

      for (; parmSymRef; parmSymRef = si.getNext())
         {
         parmSym = parmSymRef->getSymbol()->getParmSymbol();
         if (parmSym)
            break;
         }

      si.reset();
      slotSymRef = si.getFirst();

      bool OSlotParm = (*currentSlots) & (1<<(parmIteratorIdx%32));

      for (; slotSymRef; slotSymRef = si.getNext())
         {
         if (slotSymRef != parmSymRef)
            {
            TR::Node      *loadNode, *storeNode;
            TR::DataType dataType = slotSymRef->getSymbol()->getDataType();
            bool         zeroIt = ((OSlotParm && dataType!=TR::Address) ||
                                   (!OSlotParm && dataType==TR::Address));

            TR_ASSERT(slotSymRef->getSymbol()->getParmSymbol()==NULL, "Multiple parms on the same slot");
            if (!zeroIt)
               {
               int32_t realOffset = (tempSlots+parmSlots-parmIteratorIdx-1) * TR::Compiler->om.sizeofReferenceAddress();

               if (isSyncMethod)
                  realOffset += TR::Compiler->om.sizeofReferenceAddress();

               if (TR::Symbol::convertTypeToNumberOfSlots(dataType) == 2)
                  {
                  TR_ASSERT(parmIteratorIdx!=tempSlots+parmSlots-1, "Access beyond the temp buffer");
                  realOffset -= TR::Compiler->om.sizeofReferenceAddress();
                  }

               if (addrNode==NULL || nodeRealOffset!=realOffset)
                  {
                  nodeRealOffset = realOffset;
                  if (comp()->target().is64Bit())
                     {
                     TR::Node *constNode = TR::Node::create(TR::lconst, 0);
                     constNode->setLongInt(realOffset);
                     addrNode = TR::Node::create(TR::aladd, 2, dltBufChild, constNode);
                     }
                  else
                     addrNode = TR::Node::create(TR::aiadd, 2, dltBufChild, TR::Node::create(TR::iconst, 0, realOffset));
                  }
               loadNode = TR::Node::createWithSymRef(comp()->il.opCodeForIndirectLoad(dataType), 1, 1, addrNode, shadowSymRef);
               }
            else
               {
               switch (dataType)
                  {
                  case TR::Int32:
                     loadNode = TR::Node::iconst(0);
                     break;

                  case TR::Int64:
                     loadNode = TR::Node::lconst(0);
                     break;

                  case TR::Address:
                     loadNode = TR::Node::aconst(0);
                     break;

                  case TR::Float:
                     loadNode = TR::Node::create(TR::fconst, 0);
                     loadNode->setFloat(0.0);
                     break;

                  case TR::Double:
                     loadNode = TR::Node::create(TR::dconst, 0);
                     loadNode->setDouble(0.0);
                     break;

                  default:
                     TR_ASSERT(false, "Unexpected auto slot data type");
                     loadNode = TR::Node::iconst(0);
                     break;
                  }
               }
            storeNode = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(dataType), 1, 1, loadNode, slotSymRef);
            newFirstBlock->append(TR::TreeTop::create(comp(), storeNode));
            }
         }

      if (parmSymRef && parmSym->isReferencedParameter() && parmSym->isCollectedReference() && !OSlotParm)
         {
         *currentSlots |= 1<<(parmIteratorIdx%32);
   parmSym->setReinstatedReceiver();
         TR::Node *resetNode = TR::Node::createWithSymRef(TR::astore, 1, 1, TR::Node::aconst(0), parmSymRef);
         newFirstBlock->append(TR::TreeTop::create(comp(), resetNode));
         }
      }

   if (tempSlots != 0)
      {
      int32_t  currentBundle, remainInBundle;

      slotsDescription += parmSlots/32;
      currentBundle = *slotsDescription++;
      remainInBundle = 32 - parmSlots%32;
      if (remainInBundle != 32)
         currentBundle >>= (32 - remainInBundle);

      for (int32_t i=0; i < tempSlots; i++)
         {
         TR::Node *addrNode = NULL;
         int32_t  nodeRealOffset = -1;
         bool    isOSlot = currentBundle & 1;

         currentBundle >>= 1;
         remainInBundle -= 1;
         if (remainInBundle==0 && i!=tempSlots-1)
            {
            currentBundle = *slotsDescription++;
            remainInBundle = 32;
            }

         List<TR::SymbolReference> &list = _methodSymbol->getAutoSymRefs(parmSlots+i);
         ListIterator<TR::SymbolReference> iterator(&list);
         TR::SymbolReference *symRef = iterator.getFirst();
         for (; symRef; symRef = iterator.getNext())
            {
            TR::Node *loadNode, *storeNode;
            TR::DataType dataType = symRef->getSymbol()->getDataType();
            bool         zeroIt = ((isOSlot && dataType!=TR::Address) ||
                                   (!isOSlot && dataType==TR::Address));
            if (!zeroIt)
               {
               int32_t realOffset = (tempSlots-i-1) * TR::Compiler->om.sizeofReferenceAddress();

               if (isSyncMethod)
                  realOffset += TR::Compiler->om.sizeofReferenceAddress();

               if (TR::Symbol::convertTypeToNumberOfSlots(dataType) == 2)
                  {
                  TR_ASSERT(i!=tempSlots-1, "Access beyond the temp buffer");
                  realOffset -= TR::Compiler->om.sizeofReferenceAddress();
                  }

               if (addrNode==NULL || nodeRealOffset!=realOffset)
                  {
                  nodeRealOffset = realOffset;
                  if (comp()->target().is64Bit())
                     {
                     TR::Node *constNode = TR::Node::create(TR::lconst, 0);
                     constNode->setLongInt(realOffset);
                     addrNode = TR::Node::create(TR::aladd, 2, dltBufChild, constNode);
                     }
                  else
                     addrNode = TR::Node::create(TR::aiadd, 2, dltBufChild, TR::Node::create(TR::iconst, 0, realOffset));
                  }
               loadNode = TR::Node::createWithSymRef(comp()->il.opCodeForIndirectLoad(dataType), 1, 1, addrNode, shadowSymRef);
               }
            else
               {
               switch (dataType)
                  {
                  case TR::Int32:
                     loadNode = TR::Node::iconst(0);
                     break;

                  case TR::Int64:
                     loadNode = TR::Node::lconst(0);
                     break;

                  case TR::Address:
                     loadNode = TR::Node::aconst(0);
                     break;

                  case TR::Float:
                     loadNode = TR::Node::create(TR::fconst, 0);
                     loadNode->setFloat(0.0);
                     break;

                  case TR::Double:
                     loadNode = TR::Node::create(TR::dconst, 0);
                     loadNode->setDouble(0.0);
                     break;

                  default:
                     TR_ASSERT(false, "Unexpected auto slot data type");
                     loadNode = TR::Node::iconst(0);
                     break;
                  }
               }
            storeNode = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(dataType), 1, 1, loadNode, symRef);
            newFirstBlock->append(TR::TreeTop::create(comp(), storeNode));
            }
         }
      }

   for (auto nextSymRef = comp()->getMonitorAutoSymRefsInCompiledMethod()->begin(); nextSymRef != comp()->getMonitorAutoSymRefsInCompiledMethod()->end(); ++nextSymRef)
      {
      TR_ASSERT(!comp()->getOption(TR_MimicInterpreterFrameShape), "cannot initialize sync temp symRefs in DLT block when autos can alias each other\n");
      TR::Node *aconstNode = TR::Node::aconst(0);
      TR::Node *storeNode = TR::Node::createWithSymRef(TR::astore, 1, 1, aconstNode, *nextSymRef);
      storeNode->setLiveMonitorInitStore(true);
      newFirstBlock->append(TR::TreeTop::create(comp(), storeNode));
      }

   newFirstBlock->append(TR::TreeTop::create(comp(), TR::Node::create(TR::Goto, 0, steerTarget)));
   if (firstBlock != steerTarget->getNode()->getBlock())
      {
      cfg()->addEdge(newFirstBlock, steerTarget->getNode()->getBlock());
      cfg()->removeEdge(newFirstBlock, firstBlock);
      }
   }

void
TR_J9ByteCodeIlGenerator::inlineJitCheckIfFinalizeObject(TR::Block *firstBlock)
   {

   ///comp()->dumpMethodTrees("before inlineJitCheckIfFinalizeObject", _methodSymbol);
   TR::SymbolReference *finalizeSymRef = comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_jitCheckIfFinalizeObject, true, true, true);
   int32_t origNumBlocks = cfg()->getNextNodeNumber();

   for (TR::Block *block = firstBlock; block; block = block->getNextBlock())
      {
      // ignore processing any new blocks created
      //
      if (block->getNumber() >= origNumBlocks)
         continue;

      TR::TreeTop *tt = block->getEntry();
      for (; tt != block->getExit(); tt = tt->getNextTreeTop())
         {
         TR::Node *node = tt->getNode();
         if (node->getOpCodeValue() == TR::treetop)
            node = node->getFirstChild();

         // look for
         // vcall jitCheckIfFinalizeObject
         //   receiverArg
         //
         // if found, create a diamond control flow
         // if (classDepthAndFlags & FINALIZE)
         //    vcall jitCheckIfFinalizeObject
         // else
         //    remainder
         //
         bool is64bit = comp()->target().is64Bit();
         //
         if (node->getOpCode().isCall() &&
               (node->getSymbolReference() == finalizeSymRef))
            {
            TR::Node *vftLoad = TR::Node::createWithSymRef(TR::aloadi, 1, 1,
                                               node->getFirstChild(),
                                               comp()->getSymRefTab()->findOrCreateVftSymbolRef());

            TR::ILOpCodes loadOp = is64bit ? TR::lloadi : TR::iloadi;

            TR::Node *classDepthAndFlagsNode = TR::Node::createWithSymRef(loadOp, 1, 1,
                                              vftLoad,
                                              comp()->getSymRefTab()->findOrCreateClassAndDepthFlagsSymbolRef());
            TR::Node *andConstNode = TR::Node::create(classDepthAndFlagsNode, is64bit ? TR::lconst : TR::iconst, 0);
            if (is64bit)
               andConstNode->setLongInt(fej9()->getFlagValueForFinalizerCheck());
            else
               andConstNode->setInt(fej9()->getFlagValueForFinalizerCheck());

            TR::Node *andNode = TR::Node::create(is64bit ? TR::land : TR::iand, 2, classDepthAndFlagsNode, andConstNode);
            TR::Node *constCheckNode = TR::Node::create(andConstNode, is64bit ? TR::lconst : TR::iconst, 0);
            if (is64bit)
               constCheckNode->setLongInt(0);
            else
               constCheckNode->setInt(0);
            TR::Node *cmp = TR::Node::createif(is64bit ? TR::iflcmpne : TR::ificmpne,
                                             andNode,
                                             constCheckNode,
                                             NULL);
            TR::TreeTop *cmpTree = TR::TreeTop::create(comp(), cmp);
            TR::Node *dupCallNode = tt->getNode()->duplicateTree();
            TR::TreeTop *dupCallTreeTop = TR::TreeTop::create(comp(), dupCallNode);
            block->createConditionalBlocksBeforeTree(tt, cmpTree, dupCallTreeTop, NULL, cfg(), false);
            // createConditionalBlocks sets the ifBlock as cold
            // reset the flag and setup the appropriate frequency here
            //
            TR::Block *callBlock = cmp->getBranchDestination()->getNode()->getBlock();
            callBlock->setIsCold(false);
            callBlock->setFrequency(MAX_COLD_BLOCK_COUNT+1);
            ///comp()->dumpMethodTrees("after inlineJitCheckIfFinalizeObject", _methodSymbol);
            break;
            }
         }
      }
   }

//A structure to hold the names of all methods in DecimalFormatHelper that we need to replace with other
//methods.
struct TR_J9ByteCodeIlGenerator::methodRenamePair TR_J9ByteCodeIlGenerator::_decFormatRenames[_numDecFormatRenames] =
   { {"com/ibm/jit/DecimalFormatHelper$FieldPosition.setBeginIndex(I)V", "java/text/FieldPosition.setBeginIndex(I)V"},
     {"com/ibm/jit/DecimalFormatHelper$FieldPosition.setEndIndex(I)V", "java/text/FieldPosition.setEndIndex(I)V"},
     {"com/ibm/jit/DecimalFormatHelper$FieldPosition.getFieldDelegate()Lcom/ibm/jit/DecimalFormatHelper$FieldDelegate;", "java/text/FieldPosition.getFieldDelegate()Ljava/text/Format$FieldDelegate;"},
     {"com/ibm/jit/DecimalFormatHelper$FieldPosition.getFieldAttribute()Ljava/text/Format$Field;", "java/text/FieldPosition.getFieldAttribute()Ljava/text/Format$Field;"},
     {"com/ibm/jit/DecimalFormatHelper$DigitList.isZero()Z", "java/text/DigitList.isZero()Z"},
     {"com/ibm/jit/DecimalFormatHelper$DigitList.set(ZJ)V", "java/text/DigitList.set(ZJ)V"},
     {"com/ibm/jit/DecimalFormatHelper.isGroupingUsed(Ljava/text/NumberFormat;)Z", "java/text/NumberFormat.isGroupingUsed()Z"},
     {"com/ibm/jit/DecimalFormatHelper.getBigDecimalMultiplier(Ljava/text/DecimalFormat;)Ljava/math/BigDecimal;", "java/text/DecimalFormat.getBigDecimalMultiplier()Ljava/math/BigDecimal;"},
     {"com/ibm/jit/DecimalFormatHelper.subformat(Ljava/text/DecimalFormat;Ljava/lang/StringBuffer;Lcom/ibm/jit/DecimalFormatHelper$FieldDelegate;ZZIIII)Ljava/lang/StringBuffer;", "java/text/DecimalFormat.subformat(Ljava/lang/StringBuffer;Ljava/text/Format$FieldDelegate;ZZIIII)Ljava/lang/StringBuffer;"},
   };

//Replaces all methods and fields in DecimalFormatHelper.format routine with appropriate methods and fields.
//returns true if all methods and fields are replaced successfully
bool TR_J9ByteCodeIlGenerator::replaceMembersOfFormat()
   {
   for (int i =0; i < _numDecFormatRenames; i++)
      {
      _decFormatRenamesDstSymRef[i] = fej9()->findOrCreateMethodSymRef(comp(), _methodSymbol, _decFormatRenames[i].dstMethodSignature);
      }

   bool successful = true;
   for (TR::TreeTop* tt = _methodSymbol->getFirstTreeTop(); (tt != NULL); tt = tt->getNextRealTreeTop())
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCodeValue() == TR::treetop)
         node = node->getFirstChild();
      TR::Node *callNode = node;
      if (!node->getOpCode().isCall() && (node->getNumChildren() > 0))
    callNode = node->getFirstChild();
      successful = successful && replaceMethods(tt, callNode);
      successful = successful && replaceFieldsAndStatics(tt, callNode);
      }
   return successful;
   }

bool TR_J9ByteCodeIlGenerator::replaceMethods(TR::TreeTop *tt, TR::Node *node)
   {
   if (!node->getOpCode().isCall() || !node->getOpCode().hasSymbolReference()) return true;
   if (node->getSymbolReference()->getSymbol()->castToMethodSymbol()->isHelper()) return true;
   TR::Method * method = node->getSymbolReference()->getSymbol()->castToMethodSymbol()->getMethod();
   //I use heapAlloc because this function is called many times for a small set of methods.
   const char* nodeName = method->signature(trMemory(), heapAlloc);
   for (int i = 0; i < _numDecFormatRenames; i++)
      {
      if (!strcmp(nodeName, _decFormatRenames[i].srcMethodSignature))
         {
         if (performTransformation(comp(), "%sreplaced %s by %s in [%p]\n",
                                     OPT_DETAILS, _decFormatRenames[i].srcMethodSignature, _decFormatRenames[i].dstMethodSignature, node))
            {
            if (_decFormatRenamesDstSymRef[i] == NULL)
               return false;
            node->setSymbolReference(_decFormatRenamesDstSymRef[i]);
            return true;
            }
         else
            {
            return false;
            }
         }
      }
   return true;
   }

static bool matchFieldOrStaticName(TR::Compilation* comp, TR::Node* node, char* staticOrFieldName) {
   if ((!node->getOpCode().isLoad() && !node->getOpCode().isStore()) ||
       !node->getOpCode().hasSymbolReference())
      return false;
   TR::SymbolReference* symRef = node->getSymbolReference();
   TR::Symbol* sym = symRef->getSymbol();
   if (sym == NULL || symRef->getCPIndex() < 0) return false;
   TR_ResolvedMethod* method = comp->getOwningMethodSymbol(symRef->getOwningMethodIndex())->getResolvedMethod();
   if (!method) return false;
   switch(sym->getKind()) {
   case TR::Symbol::IsStatic:
      {
      //make sure it's not a "helper" symbol
      int32_t index = symRef->getReferenceNumber();
      int32_t nonhelperIndex = comp->getSymRefTab()->getNonhelperIndex(comp->getSymRefTab()->getLastCommonNonhelperSymbol());
      int32_t numHelperSymbols = comp->getSymRefTab()->getNumHelperSymbols();
      if ((index < numHelperSymbols) || (index < nonhelperIndex) || !sym->isStaticField()) return false;

      const char * nodeName = method->staticName(symRef->getCPIndex(), comp->trMemory(), stackAlloc);
      return !strcmp(nodeName, staticOrFieldName);
      }
   case TR::Symbol::IsShadow:
      {
      const char * nodeName = method->fieldName(symRef->getCPIndex(), comp->trMemory(), stackAlloc);
      return !strcmp(nodeName, staticOrFieldName);
      }
   default:
      return false;
   }
}

bool TR_J9ByteCodeIlGenerator::replaceField(TR::Node* node, char* destClass,
                 char* destFieldName, char* destFieldSignature,
                 int parmIndex)
   {
   TR_OpaqueClassBlock *c = fej9()->getClassFromSignature(destClass, strlen(destClass), comp()->getCurrentMethod());
   //When, e.g., AOT is enabled, classes are not loaded at compile time so the
   //function above returns NULL
   if (c == NULL)
      return false;
   if (performTransformation(comp(), "%ssymref replaced by %s.%s %s in [%p]\n", OPT_DETAILS, destClass, destFieldName, destFieldSignature, node))
      {
      //The following code (up to and including the call to initShadowSymbol) has been adapted from
      //TR::SymbolReferenceTable::findOrCreateShadowSymbol
      uint32_t offset =
    fej9()->getInstanceFieldOffset(c, destFieldName, destFieldSignature) +
    fej9()->getObjectHeaderSizeInBytes();

      TR::DataType type = node->getDataType();
      TR::Symbol * sym = TR::Symbol::createShadow(trHeapMemory(), type);
      sym->setPrivate();
      TR::SymbolReference* symRef =
    new (trHeapMemory()) TR::SymbolReference(comp()->getSymRefTab(), sym,
                   comp()->getMethodSymbol()->getResolvedMethodIndex(),
                   -1, 0);
      comp()->getSymRefTab()->checkUserField(symRef);
      //Is the last field (isUnresolvedInCP) set correctly ?
      comp()->getSymRefTab()->initShadowSymbol(comp()->getCurrentMethod(), symRef, true, type, offset, false);

      //we need to change a field reference f to p.f where p is the first parameter (of class DecimalFormat)
      //so we need to make the load/store node an indirect one and add, as a child, an indirect load to load p
      if (!node->getOpCode().isIndirect())
    {
    if (node->getOpCode().isLoad())
       {
       TR::Node::recreate(node, comp()->il.opCodeForIndirectLoad(type));
       node->setNumChildren(1);
       }
    else
       {
       TR_ASSERT(node->getOpCode().isStore(), "node can be either a load or a store\n");
       TR::Node::recreate(node, comp()->il.opCodeForIndirectStore(type));
       node->setNumChildren(2);
       node->setChild(1, node->getChild(0));
       node->setChild(0, NULL);
       }
    ListIterator<TR::ParameterSymbol> parms(&_methodSymbol->getParameterList());
    TR_ASSERT(parmIndex == 0 || parmIndex == 1, "invalid parmIndex\n");
    TR::ParameterSymbol * p = parms.getFirst();
    if (parmIndex == 1)
       p = parms.getNext();
    TR::Node* decFormObjLoad = TR::Node::createLoad(node, symRefTab()->findOrCreateAutoSymbol(_methodSymbol, p->getSlot(), p->getDataType()));
    node->setAndIncChild(0, decFormObjLoad);
    }
      node->setSymbolReference(symRef);
      return true;
      }
   return false;
   }

bool TR_J9ByteCodeIlGenerator::replaceStatic(TR::Node* node,
                                           char* dstClassName, char* staticName, char* type)
   {
   TR_OpaqueClassBlock *c = fej9()->getClassFromSignature(dstClassName, strlen(dstClassName), comp()->getCurrentMethod());

   // When, e.g., AOT is enabled, classes are not loaded at compile time so the
   // function above returns NULL
   //
   if (c == NULL)
      return false;

   void* dataAddress = fej9()->getStaticFieldAddress(c, (unsigned char *) staticName, strlen(staticName),
                                                           (unsigned char *) type, strlen(type));

   if ( dataAddress != NULL &&
        !node->getSymbolReference()->isUnresolved() &&
        performTransformation(comp(), "%sreplaced %s.%s in [%p]\n", OPT_DETAILS, dstClassName, staticName, node)
      )
      {
      //I'm not setting the correct SymRef. I'm just changing the address
      ((TR::StaticSymbol*)node->getSymbolReference()->getSymbol())->setStaticAddress(dataAddress);
      return true;
      }
   else
      return false;
   }


bool TR_J9ByteCodeIlGenerator::replaceFieldsAndStatics(TR::TreeTop *tt, TR::Node *node) {
   bool result = true;
   if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper.INSTANCE Lcom/ibm/jit/DecimalFormatHelper$FieldPosition;"))
      {
      //Replace static com/ibm/jit/DecimalFormatHelper.INSTANCE with static java/text/DontCareFieldPosition.INSTANCE
      result = replaceStatic(node, "java/text/DontCareFieldPosition", "INSTANCE", "Ljava/text/FieldPosition;");
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper.doubleDigitsTens [C"))
      {
      result = replaceStatic(node, "java/math/BigDecimal", "doubleDigitsTens", "[C");
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper.doubleDigitsOnes [C"))
      {
      result = replaceStatic(node, "java/math/BigDecimal", "doubleDigitsOnes", "[C");
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper.multiplier I"))
      {
      result = replaceField(node, "java/text/DecimalFormat", "multiplier", "I", 0);
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper.digitList Lcom/ibm/jit/DecimalFormatHelper$DigitList;"))
      {
      result = replaceField(node, "java/text/DecimalFormat", "digitList", "Ljava/text/DigitList;", 0);
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper$DigitList.digits [C"))
      {
      result = replaceField(node, "java/text/DigitList", "digits", "[C", 0);
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper$DigitList.decimalAt I"))
      {
      result = replaceField(node, "java/text/DigitList", "decimalAt", "I", 0);
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper$DigitList.count I"))
      {
      result = replaceField(node, "java/text/DigitList", "count", "I", 0);
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper.flags I"))
      {
      result = replaceField(node, "java/math/BigDecimal", "flags", "I", 1);
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper.laside J"))
      {
      result = replaceField(node, "java/math/BigDecimal", "laside", "J", 1);
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper.scale I"))
      {
   //Use "scale" for java 6 or 626 and "cachedScale" for java 7
      result = replaceField(node, "java/math/BigDecimal", "cachedScale", "I", 1);
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper.symbols Ljava/text/DecimalFormatSymbols;"))
      {
      result = replaceField(node, "java/text/DecimalFormat", "symbols", "Ljava/text/DecimalFormatSymbols;", 0);
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper.isCurrencyFormat Z"))
      {
      result = replaceField(node, "java/text/DecimalFormat", "isCurrencyFormat", "Z", 0);
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper.decimalSeparatorAlwaysShown Z"))
      {
      result = replaceField(node, "java/text/DecimalFormat", "decimalSeparatorAlwaysShown", "Z", 0);
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper.useExponentialNotation Z"))
      {
      result = replaceField(node, "java/text/DecimalFormat", "useExponentialNotation", "Z", 0);
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper.negativePrefix Ljava/lang/String;"))
      {
      result = replaceField(node, "java/text/DecimalFormat", "negativePrefix", "Ljava/lang/String;", 0);
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper.negativeSuffix Ljava/lang/String;"))
      {
      result = replaceField(node, "java/text/DecimalFormat", "negativeSuffix", "Ljava/lang/String;", 0);
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper.positivePrefix Ljava/lang/String;"))
      {
      result = replaceField(node, "java/text/DecimalFormat", "positivePrefix", "Ljava/lang/String;", 0);
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper.positiveSuffix Ljava/lang/String;"))
      {
      result = replaceField(node, "java/text/DecimalFormat", "positiveSuffix", "Ljava/lang/String;", 0);
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper.groupingSize B"))
      {
      result = replaceField(node, "java/text/DecimalFormat", "groupingSize", "B", 0);
      }
   else if (matchFieldOrStaticName(comp(), node, "com/ibm/jit/DecimalFormatHelper.minExponentDigits B"))
      {
      result = replaceField(node, "java/text/DecimalFormat", "minExponentDigits", "B", 0);
      }
   for (int i = 0; i < node->getNumChildren(); i++) {
      result = result && replaceFieldsAndStatics(tt, node->getChild(i));
   }
   return result;
}

/**
 * Expand a checkcast tree for an unresolved class.
 *
 * A ResolveCHK is emitted before the checkcast, and both the ResolveCHK and
 * checkcast are skipped when the object tested is null.
 *
 * Pictorially, transform this:
 *
   @verbatim
   +-<block_$orig>------------------------------+
   |  ... $pre                                  |
   |  n1n checkcast                             |
   |  n2n   $obj                                |
   |  n3n   loadaddr $klass [unresolved Static] |
   |  ... $post                                 |
   +--------------------------------------------+
   @endverbatim
 *
 * into this:
 *
   @verbatim
         +-<block_$orig>------------------+
         |  ... $pre                      |
         |  ... treetop                   |
         |  n2n   $obj                    |
         |  ... astore $tmp_obj           |
         |  n2n   =>$obj                  |
         |  ... (more fixupCommoning)     |
         |  ... ifacmpeq --> block_$merge |----------+
         |  n2n   =>$obj                  |          |
         |  ...   aconst NULL             |          |
         +--------------------------------+          |
                        |                            |
            fallthrough |                            |
                        |                            |
                        v                            |
   +-<block_$nonnull>---------------------------+    |
   |  ... ResolveCHK                            |    |
   |  n3n   loadaddr $klass [unresolved Static] |    |
   |  n1n checkcast                             |    |
   |  ...   aload $tmp_obj                      |    |
   |  n3n   =>loadaddr                          |    |
   +--------------------------------------------+    |
                        |                            |
            fallthrough |        +-------------------+
                        |        |
                        v        v
            +-<block_$merge>--------------+
            |  $post (w/ fixupCommoning)  |
            +-----------------------------+
   @endverbatim
 *
 * @param tree The checkcast tree as generated by the walker
 *
 * @see genCheckCast(int32_t)
 * @see expandUnresolvedClassTypeTests(List<TR::TreeTop>*)
 */
void TR_J9ByteCodeIlGenerator::expandUnresolvedClassCheckcast(TR::TreeTop *tree)
   {
   TR::Node *checkcastNode = tree->getNode();
   TR_ASSERT(checkcastNode->getOpCodeValue() == TR::checkcast, "unresolved class checkcast: expected treetop %p node n%un to be checkcast but was %s\n", tree, checkcastNode->getGlobalIndex(), checkcastNode->getOpCode().getName());

   TR::Node *objNode = checkcastNode->getFirstChild();
   TR::Node *classNode = checkcastNode->getSecondChild();
   TR_ASSERT(classNode->getOpCodeValue() == TR::loadaddr, "unresolved class checkcast n%un: expected class child n%un to be loadaddr but was %s\n", checkcastNode->getGlobalIndex(), classNode->getGlobalIndex(), classNode->getOpCode().getName());
   TR_ASSERT(classNode->getSymbolReference()->isUnresolved(), "unresolved class checkcast n%un: expected symref of class child n%un to be unresolved\n", checkcastNode->getGlobalIndex(), classNode->getGlobalIndex());
   TR_ASSERT(classNode->getReferenceCount() == 1, "unresolved class checkcast n%un: expected class child n%un to have reference count 1\n", checkcastNode->getGlobalIndex(), classNode->getGlobalIndex());

   bool trace = comp()->getOption(TR_TraceILGen);
   if (trace)
      traceMsg(comp(), "expanding unresolved class checkcast n%un in block_%d\n", checkcastNode->getGlobalIndex(), tree->getEnclosingBlock()->getNumber());

   TR::Node *objAnchor = TR::Node::create(TR::treetop, 1, objNode);
   objAnchor->copyByteCodeInfo(checkcastNode);
   tree->insertBefore(TR::TreeTop::create(comp(), objAnchor));

   TR::Block *headBlock = tree->getEnclosingBlock();
   const bool fixupCommoning = true;
   TR::Block *nonNullCaseBlock = headBlock->split(tree, cfg(), fixupCommoning);
   TR::Block *tailBlock = nonNullCaseBlock->split(tree->getNextTreeTop(), cfg(), fixupCommoning);

   headBlock->getExit()->getNode()->copyByteCodeInfo(checkcastNode);
   nonNullCaseBlock->getEntry()->getNode()->copyByteCodeInfo(checkcastNode);
   nonNullCaseBlock->getExit()->getNode()->copyByteCodeInfo(checkcastNode);
   tailBlock->getEntry()->getNode()->copyByteCodeInfo(checkcastNode);

   TR::Node *aconstNull = TR::Node::aconst(0);
   TR::Node *nullTest = TR::Node::createif(TR::ifacmpeq, objNode, aconstNull, tailBlock->getEntry());
   aconstNull->copyByteCodeInfo(checkcastNode);
   nullTest->copyByteCodeInfo(checkcastNode);
   headBlock->append(TR::TreeTop::create(comp(), nullTest));
   cfg()->addEdge(headBlock, tailBlock);

   TR_ASSERT(checkcastNode->getSecondChild() == classNode, "unresolved class checkcast n%un: split block should not have replaced class child n%un with n%un\n", checkcastNode->getGlobalIndex(), classNode->getGlobalIndex(), checkcastNode->getSecondChild()->getGlobalIndex());
   TR::Node *resolveCheckNode = genResolveCheck(classNode);
   resolveCheckNode->copyByteCodeInfo(checkcastNode);
   nonNullCaseBlock->prepend(TR::TreeTop::create(comp(), resolveCheckNode));

   if (trace)
      traceMsg(comp(), "\tblock_%d: resolve, checkcast\n\tblock_%d: tail of original block\n", nonNullCaseBlock->getNumber(), tailBlock->getNumber());
   }

/**
 * Expand an instanceof tree for an unresolved class.
 *
 * A ResolveCHK is emitted before the instanceof, and both the ResolveCHK and
 * instanceof are skipped when the object tested is null. In that case, the
 * result is false. In the remainder of the block, occurrences of the original
 * instanceof node are made into occurrences of an iload of a new temp, into
 * which each of the conditional blocks stores the appropriate result.
 *
 * Pictorially, transform this:
 *
   @verbatim
   +-<block_$orig>---------------------------------+
   |  ... $pre                                     |
   |  n1n treetop                                  |
   |  n2n   instanceof                             |
   |  n3n     $obj                                 |
   |  n4n     loadaddr $klass [unresolved Static]  |
   |  ... $post                                    |
   +-----------------------------------------------+
   @endverbatim
 *
 * into this:
 *
   @verbatim
         +-<block_$orig>------------------+
         |  ... $pre                      |
         |  ... treetop                   |
         |  n3n   $obj                    |
         |  ... astore $tmp_obj           |
         |  n3n   =>$obj                  |
         |  ... (more fixupCommoning)     |
         |  ... ifacmpeq --> block_$null  |-----------------+
         |  n3n   =>$obj                  |                 |
         |  ...   aconst NULL             |                 |
         +--------------------------------+                 |
                        |                                   |
            fallthrough |                                   |
                        |                                   |
                        v                                   |
   +-<block_$nonnull>-----------------------+               v
   |  ResolveCHK                            |   +-<block_$null>----------+
   |    loadaddr $klass [unresolved Static] |   |  istore $tmp_result    |
   |  istore $tmp_result                    |   |    iconst 0            |
   |    instanceof                          |   |  goto --> block_$merge |
   |      aload $tmp_obj                    |   +------------------------+
   |      =>loadaddr                        |               |
   +----------------------------------------+               |
                        |                                   |
            fallthrough |        +--------------------------+
                        |        |
                        v        v
          +-<block_$merge>-----------------+
          |  n1n treetop                   |
          |  n2n   iload $tmp_result       |
          |  ... $post (w/ fixupCommoning) |
          +--------------------------------+
   @endverbatim
 *
 * except that the treetop n1n is removed from the merge block, because it's
 * clearly unnecessary.
 *
 * @param tree The instanceof tree as generated by the walker
 *
 * @see genInstanceof(int32_t)
 * @see expandUnresolvedClassTypeTests(List<TR::TreeTop>*)
 */
void TR_J9ByteCodeIlGenerator::expandUnresolvedClassInstanceof(TR::TreeTop *tree)
   {
   TR::Node *treetopNode = tree->getNode();
   TR_ASSERT(treetopNode->getOpCodeValue() == TR::treetop, "unresolved class instanceof: expected treetop %p node n%un to be treetop but was %s\n", tree, treetopNode->getGlobalIndex(), treetopNode->getOpCode().getName());

   TR::Node *instanceofNode = treetopNode->getFirstChild();
   TR_ASSERT(instanceofNode->getOpCodeValue() == TR::instanceof, "unresolved class instanceof: expected treetop %p node n%un child n%un to be instanceof but was %s\n", tree, treetopNode->getGlobalIndex(), instanceofNode->getGlobalIndex(), instanceofNode->getOpCode().getName());

   TR::Node *objNode = instanceofNode->getFirstChild();
   TR::Node *origClassNode = instanceofNode->getSecondChild();
   TR_ASSERT(origClassNode->getOpCodeValue() == TR::loadaddr, "unresolved class instanceof n%un: expected class child n%un to be loadaddr but was %s\n", instanceofNode->getGlobalIndex(), origClassNode->getGlobalIndex(), origClassNode->getOpCode().getName());
   TR_ASSERT(origClassNode->getSymbolReference()->isUnresolved(), "unresolved class instanceof n%un: expected symref of class child n%un to be unresolved\n", instanceofNode->getGlobalIndex(), origClassNode->getGlobalIndex());

   bool trace = comp()->getOption(TR_TraceILGen);
   if (trace)
      traceMsg(comp(), "expanding unresolved class instanceof n%un in block_%d\n", instanceofNode->getGlobalIndex(), tree->getEnclosingBlock()->getNumber());

   TR::Node *objAnchor = TR::Node::create(TR::treetop, 1, objNode);
   objAnchor->copyByteCodeInfo(instanceofNode);
   tree->insertBefore(TR::TreeTop::create(comp(), objAnchor));

   // create blocks
   TR::Block *headBlock = tree->getEnclosingBlock();
   const bool fixupCommoning = true;
   TR::Block *nonNullCaseBlock = headBlock->split(tree, cfg(), fixupCommoning);
   TR::Block *tailBlock = nonNullCaseBlock->split(tree, cfg(), fixupCommoning);
   TR::Block *nullCaseBlock = TR::Block::createEmptyBlock(comp());
   cfg()->addNode(nullCaseBlock);
   cfg()->findLastTreeTop()->join(nullCaseBlock->getEntry());

   headBlock->getExit()->getNode()->copyByteCodeInfo(instanceofNode);
   nonNullCaseBlock->getEntry()->getNode()->copyByteCodeInfo(instanceofNode);
   nonNullCaseBlock->getExit()->getNode()->copyByteCodeInfo(instanceofNode);
   nullCaseBlock->getEntry()->getNode()->copyByteCodeInfo(instanceofNode);
   nullCaseBlock->getExit()->getNode()->copyByteCodeInfo(instanceofNode);
   tailBlock->getEntry()->getNode()->copyByteCodeInfo(instanceofNode);

   // headBlock ends with a null test conditionally jumping to nullCaseBlock
   TR::Node *aconstNull = TR::Node::aconst(0);
   TR::Node *nullTest = TR::Node::createif(TR::ifacmpeq, objNode, aconstNull, nullCaseBlock->getEntry());
   aconstNull->copyByteCodeInfo(instanceofNode);
   nullTest->copyByteCodeInfo(instanceofNode);
   headBlock->append(TR::TreeTop::create(comp(), nullTest));
   cfg()->addEdge(headBlock, nullCaseBlock);

   // New temporary for the result of instanceof.
   TR::SymbolReference *resultTemp = symRefTab()->createTemporary(_methodSymbol, TR::Int32);

   // In the null case, instanceof returns false (without resolving the class)
   TR::Node *iconst0 = TR::Node::iconst(0);
   TR::Node *store0 = TR::Node::createWithSymRef(TR::istore, 1, 1, iconst0, resultTemp);
   iconst0->copyByteCodeInfo(instanceofNode);
   store0->copyByteCodeInfo(instanceofNode);
   nullCaseBlock->append(TR::TreeTop::create(comp(), store0));
   TR::Node *gotoTail = TR::Node::create(TR::Goto, 0, tailBlock->getEntry());
   gotoTail->copyByteCodeInfo(instanceofNode);
   nullCaseBlock->append(TR::TreeTop::create(comp(), gotoTail));
   cfg()->addEdge(nullCaseBlock, tailBlock);

   // In the non-null case, compute instanceof after resolving the class
   TR::TreeTop *newInstanceofTree = tree->duplicateTree();
   TR::Node *resultStoreNode = newInstanceofTree->getNode();
   resultStoreNode = TR::Node::recreateWithSymRef(resultStoreNode, TR::istore, resultTemp);
   TR::Node *classNode = resultStoreNode->getFirstChild()->getSecondChild();
   TR::Node *resolveCheckNode = genResolveCheck(classNode);
   resolveCheckNode->copyByteCodeInfo(instanceofNode);
   nonNullCaseBlock->append(TR::TreeTop::create(comp(), resolveCheckNode));
   nonNullCaseBlock->append(newInstanceofTree);

   // Uses of the original instanceof will now load the temp instead
   instanceofNode = TR::Node::recreateWithSymRef(instanceofNode, TR::iload, resultTemp);
   instanceofNode->removeAllChildren();

   // Remove the tree that was originally anchoring instanceof
   const bool decRefOriginalTreetopNode = true;
   tree->unlink(decRefOriginalTreetopNode);

   if (trace)
      {
      traceMsg(comp(), "\tresult in temp #%d\n", resultTemp->getReferenceNumber());
      traceMsg(comp(), "\tblock_%d: resolve, instanceof\n", nonNullCaseBlock->getNumber());
      traceMsg(comp(), "\tblock_%d: false\n", nullCaseBlock->getNumber());
      traceMsg(comp(), "\tblock_%d: tail of original block\n", tailBlock->getNumber());
      }
   }

bool TR_J9ByteCodeIlGenerator::skipInvokeSpecialInterfaceTypeChecks()
   {
   static const bool disable =
      feGetEnv("TR_skipInvokeSpecialInterfaceTypeChecks") != NULL;
   return disable;
   }

/**
 * Expand a call tree for invokespecial within an interface.
 *
 * An explicit type test is inserted ahead of the call to ensure that the
 * receiver is an instance of the containing interface. The type test is in
 * terms of instanceof, so it will fail when the receiver is null. The call's
 * null check (if any) is moved before the type test so that it gets the first
 * opportunity to throw.
 *
 * Pictorially, transform this:
 *
   @verbatim
   +-<block_$orig>-------------+
   |  ... $pre                 |
   |  ... NULLCHK              |
   |  n1n   call X.foo(...)T   |
   |  n2n     $receiver        |
   |          ...args          |
   |  ... $post                |
   +---------------------------+
   @endverbatim
 *
 * into the following, where @c $Interface is the interface type that was
 * detected for @c _invokeSpecialInterface:
 *
   @verbatim
   +-<block_$orig>------------------+
   |  ... $pre                      |
   |  ... NULLCHK                   |
   |  ...   PassThrough             |
   |  n2n     $receiver             |
   |  ... astore $tmp_receiver      |
   |  n2n   ==>$receiver            |
   |  ... (more fixupCommoning)     |
   |  ... ificmpne --> block_$ok    |-----+
   |        instanceof              |     |
   |          ==>$receiver          |     |
   |          loadaddr $Interface   |     |
   |        iconst 0                |     |
   +--------------------------------+     |
                  |                       |
      fallthrough |                       |
                  |                       |
                  v                       |
   +-<block_$fail>-(cold)-------------+   |
   |  ... (throw IllegalAccessError)  |   |
   +----------------------------------+   |
                                          |
                  +-----------------------+
                  |
                  v
   +-<block_$ok>--------------------+
   |  ... treetop                   |
   |  n1n   call X.foo(...)T        |
   |  ...     aload $tmp_receiver   |
   |          ...args               |
   |  ... $post (w/ fixupCommoning) |
   +--------------------------------+
   @endverbatim
 *
 * @param tree The call tree as generated by the walker
 */

void TR_J9ByteCodeIlGenerator::expandInvokeSpecialInterface(TR::TreeTop *tree)
   {
   TR_ASSERT(
      !comp()->compileRelocatableCode(),
      "in expandInvokeSpecialInterface under AOT\n");

   TR_ASSERT(
      !skipInvokeSpecialInterfaceTypeChecks(),
      "in expandInvokeSpecialInterface, but it is disabled\n");

   const bool trace = comp()->getOption(TR_TraceILGen);
   static const bool verbose =
      feGetEnv("TR_verboseInvokeSpecialInterface") != NULL;

   if (trace)
      {
      traceMsg(comp(),
         "expanding invokespecial in interface method at n%un\n",
         tree->getNode()->getGlobalIndex());
      if (verbose)
         comp()->dumpMethodTrees("Trees before expanding invokespecial", _methodSymbol);
      }

   TR::Node * const parent = tree->getNode();
   TR::Node * const callNode = parent->getChild(0);
   TR::Node * const receiver = callNode->getArgument(0);

   // Temporarily set _bcIndex so that new nodes will pick it up
   const int32_t rememberedBcIndex = _bcIndex;
   _bcIndex = callNode->getByteCodeIndex();

   // Separate the null check if there is one, so it will precede the type test.
   TR::TransformUtil::separateNullCheck(comp(), tree, trace);

   // Insert the type test
   TR::SymbolReference * const ifaceSR =
      symRefTab()->findOrCreateClassSymbol(_methodSymbol, -1, _invokeSpecialInterface);
   TR::Node * const iface = TR::Node::createWithSymRef(TR::loadaddr, 0, ifaceSR);

   TR::SymbolReference * const instofSR =
      symRefTab()->findOrCreateInstanceOfSymbolRef(_methodSymbol);
   TR::Node * const instof =
      TR::Node::createWithSymRef(TR::instanceof, 2, 2, receiver, iface, instofSR);

   TR::Node * const typeTest = TR::Node::createif(
      TR::ificmpne,
      instof,
      TR::Node::iconst(0));

   tree->insertBefore(TR::TreeTop::create(comp(), typeTest));

   if (trace)
      traceMsg(comp(), "Inserted type test n%un\n", typeTest->getGlobalIndex());

   // Split block between type test and call
   TR::Block * const headBlock = tree->getEnclosingBlock();

   const bool fixupCommoning = true;
   TR::Block * const failBlock = headBlock->split(tree, cfg(), fixupCommoning);
   TR::Block * const okBlock = failBlock->split(tree, cfg(), fixupCommoning);

   if (trace)
      {
      traceMsg(comp(),
         "Split block_%d into:\n"
         "\tblock_%d (preceding code, and type test),\n"
         "\tblock_%d (helper call for type test failure)\n"
         "\tblock_%d (successful call, and succeeding code)\n",
         headBlock->getNumber(),
         headBlock->getNumber(),
         failBlock->getNumber(),
         okBlock->getNumber());
      }

   // Fix branch destination
   typeTest->setBranchDestination(okBlock->getEntry());
   cfg()->addEdge(headBlock, okBlock);

   // Failure case: throw IllegalAccessError by calling jitThrowIncompatibleReceiver
   failBlock->setIsCold();
   failBlock->setFrequency(UNKNOWN_COLD_BLOCK_COUNT);

   TR::Node * const rejectedVFT = TR::Node::createWithSymRef(
      TR::aloadi,
      1,
      1,
      callNode->getArgument(0)->duplicateTree(), // load split temp
      symRefTab()->findOrCreateVftSymbolRef());

   TR::Node * const helperCall = TR::Node::createWithSymRef(
      TR::call,
      2,
      2,
      iface->duplicateTree(), // avoid commoning across blocks
      rejectedVFT,
      symRefTab()->findOrCreateIncompatibleReceiverSymbolRef(_methodSymbol));

   failBlock->append(TR::TreeTop::create(comp(),
      TR::Node::create(TR::treetop, 1, helperCall)));

   bool voidReturn = _methodSymbol->getResolvedMethod()->returnOpCode() == TR::Return;
   TR::Node *retNode = TR::Node::create(_methodSymbol->getResolvedMethod()->returnOpCode(), voidReturn?0:1);
   if (!voidReturn)
      {
      TR::Node *retValNode = TR::Node::create(comp()->il.opCodeForConst(_methodSymbol->getResolvedMethod()->returnType()), 0);
      retValNode->setLongInt(0);
      retNode->setAndIncChild(0, retValNode);
      }
   failBlock->append(TR::TreeTop::create(comp(), retNode));

   if (trace)
      {
      traceMsg(comp(),
         "generated helper call n%un for type test failure\n",
         helperCall->getGlobalIndex());
      }

   cfg()->removeEdge(failBlock, okBlock);
   cfg()->addEdge(failBlock, cfg()->getEnd());

   if (trace && verbose)
      comp()->dumpMethodTrees("Trees after expanding invokespecial", _methodSymbol);

   _bcIndex = rememberedBcIndex;
   }

TR::Node*
TR_J9ByteCodeIlGenerator::loadFromMethodTypeTable(TR::Node* methodHandleInvokeCall)
   {
   int32_t cpIndex = next2Bytes();
   TR::SymbolReference *symRef = symRefTab()->findOrCreateMethodTypeTableEntrySymbol(_methodSymbol, cpIndex);
   TR::Node *methodType = TR::Node::createWithSymRef(methodHandleInvokeCall, TR::aload, 0, symRef);
   if (!symRef->isUnresolved())
      {
      if (_methodSymbol->getResolvedMethod()->methodTypeTableEntryAddress(cpIndex))
         methodType->setIsNonNull(true);
      else
         methodType->setIsNull(true);
      }

   return methodType;
   }


TR::Node*
TR_J9ByteCodeIlGenerator::loadCallSiteMethodTypeFromCP(TR::Node* methodHandleInvokeCall)
   {
   int32_t cpIndex = next2Bytes();
   TR_ASSERT(method()->isMethodTypeConstant(cpIndex), "Address-type CP entry %d must be methodType", cpIndex);
   TR::SymbolReference *symRef = symRefTab()->findOrCreateMethodTypeSymbol(_methodSymbol, cpIndex);
   TR::Node* methodType = TR::Node::createWithSymRef(methodHandleInvokeCall, TR::aload, 0, symRef);
   return methodType;
   }

TR::Node*
TR_J9ByteCodeIlGenerator::loadCallSiteMethodType(TR::Node* methodHandleInvokeCall)
   {
   if (fej9()->hasMethodTypesSideTable())
      return loadFromMethodTypeTable(methodHandleInvokeCall);
   else
      return loadCallSiteMethodTypeFromCP(methodHandleInvokeCall);
   }

void TR_J9ByteCodeIlGenerator::insertCustomizationLogicTreeIfEnabled(TR::TreeTop* tree, TR::Node* methodHandle)
   {
   if (comp()->getOption(TR_EnableMHCustomizationLogicCalls))
      {
      TR::SymbolReference *doCustomizationLogic = comp()->getSymRefTab()->methodSymRefFromName(_methodSymbol, JSR292_MethodHandle, "doCustomizationLogic", "()V", TR::MethodSymbol::Special);
      TR::Node* customization = TR::Node::createWithSymRef(TR::call, 1, 1, methodHandle, doCustomizationLogic);
      customization->getByteCodeInfo().setDoNotProfile(true);
      tree->insertBefore(TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, customization)));

      if (comp()->getOption(TR_TraceILGen))
         {
         traceMsg(comp(), "Inserted call to doCustomizationLogic n%dn %p\n", customization->getGlobalIndex(), customization);
         }
      }
   }

/** \brief
 *     Expand a MethodHandle.invokeExact call generated from invokeHandle
 *
 *     Transforms the following tree
 *
   @verbatim
     ... NULLCHK
     n1n   xcalli MethodHandle.invokeExact
     n2n     lconst 0
     n3n     $receiver
             ...args
   @endverbatim
 *
 * into
 *
   @verbatim
     ... NULLCHK
     ...   PassThrough
     n3n     $receiver
     ... acall MethodHandle.getType
            ==>$receiver
     ... ZEROCHK
            acmpeq
               $callSiteMethodType
               ==>acall MethodHandle.getType
     ... lcall MethodHandle.invokeExactTargetAddress
             ==>$receiver
     n1n xcalli MethodHandle.invokeExact
           ==>lcall MethodHandle.invokeExactTargetAddress
           ==>$receiver
             ...args
   @endverbatim
 *
 *  \param tree
 *     Tree of the invokeHandle call.
 *
 *  \note
 *     A call to doCustomizationLogic will be inserted if customization
 *     logic is enabled for invocations outside of thunk archetype.
 */
void TR_J9ByteCodeIlGenerator::expandInvokeHandle(TR::TreeTop *tree)
   {
   TR_ASSERT(!comp()->compileRelocatableCode(), "in expandInvokeHandle under AOT\n");

   if (comp()->getOption(TR_TraceILGen))
      {
      traceMsg(comp(), "expanding invokehandle at n%dn\n", tree->getNode()->getGlobalIndex());
      }

   TR::Node * callNode = tree->getNode()->getChild(0);
   TR::Node * receiverHandle = callNode->getArgument(0);
   callNode->getByteCodeInfo().setDoNotProfile(true);

   TR::Node* callSiteMethodType = loadCallSiteMethodType(callNode);

   if (callSiteMethodType->getSymbolReference()->isUnresolved())
      {
      TR::Node *resolveChkOnMethodType = TR::Node::createWithSymRef(callNode, TR::ResolveCHK, 1, callSiteMethodType, comp()->getSymRefTab()->findOrCreateResolveCheckSymbolRef(_methodSymbol));
      tree->insertBefore(TR::TreeTop::create(comp(), resolveChkOnMethodType));
      }

   // Generate zerochk
   TR::Node* zerochkNode = genHandleTypeCheck(receiverHandle, callSiteMethodType);
   tree->insertBefore(TR::TreeTop::create(comp(), zerochkNode));

   if (comp()->getOption(TR_TraceILGen))
      {
      traceMsg(comp(), "Inserted ZEROCHK n%dn %p\n", zerochkNode->getGlobalIndex(), zerochkNode);
      }

   insertCustomizationLogicTreeIfEnabled(tree, receiverHandle);

   expandInvokeExact(tree);
   }

/** \brief
 *     Expand a MethodHandle.invokeExact call generated from invokeDynamic.
 *
 *     Transforms the following tree
 *
   @verbatim
     ... NULLCHK
     n1n   xcalli MethodHandle.invokeExact
     n2n     lconst 0
     n3n     $receiver
             ...args
   @endverbatim
 *
 * into
 *
   @verbatim
     ... NULLCHK
     ...   PassThrough
     n3n     $receiver
     ... lcall MethodHandle.invokeExactTargetAddress
             ==>$receiver
     n1n xcalli MethodHandle.invokeExact
           ==>lcall MethodHandle.invokeExactTargetAddress
           ==>$receiver
             ...args
   @endverbatim
 *
 *  \param tree
 *     Tree of the invokeExact call.
 *
 *  \note
 *     A call to doCustomizationLogic will be inserted if customization
 *     logic is enabled for invocations outside of thunk archetype.
 */
void TR_J9ByteCodeIlGenerator::expandInvokeDynamic(TR::TreeTop *tree)
   {
   TR_ASSERT(!comp()->compileRelocatableCode(), "in expandInvokeDynamic under AOT\n");

   if (comp()->getOption(TR_TraceILGen))
      {
      traceMsg(comp(), "expanding invokeDynamic at n%dn\n", tree->getNode()->getGlobalIndex());
      }

   TR::Node * callNode = tree->getNode()->getChild(0);
   TR::Node * receiverHandle = callNode->getArgument(0);
   callNode->getByteCodeInfo().setDoNotProfile(true);

   insertCustomizationLogicTreeIfEnabled(tree, receiverHandle);
   expandInvokeExact(tree);
   }

/** \brief
 *     Expand a MethodHandle.invokeExact call generated from invokeHandleGeneric.
 *
 *     Transforms the following tree
 *
   @verbatim
     ... NULLCHK
     n1n   xcalli MethodHandle.invokeExact
     n2n     lconst 0
     n3n     $receiver
             ...args
   @endverbatim
 *
 * into
 *
   @verbatim
     ... NULLCHK
     ...   PassThrough
     n3n     $receiver
     ... acall MethodHandle.asType
             ==>$receiver
     ...     $callSiteMethodType
     ... lcall MethodHandle.invokeExactTargetAddress
             ==>acall MethodHandle.asType
     n1n xcalli MethodHandle.invokeExact
           ==>lcall MethodHandle.invokeExactTargetAddress
           ==>acall MethodHandle.asType
             ...args
   @endverbatim
 *
 *  \param tree
 *     Tree of the invokeExact call.
 *
 *  \note
 *     A call to doCustomizationLogic will be inserted if customization
 *     logic is enabled for invocations outside of thunk archetype.
 *
 */
void TR_J9ByteCodeIlGenerator::expandInvokeHandleGeneric(TR::TreeTop *tree)
   {
   TR_ASSERT(!comp()->compileRelocatableCode(), "in expandInvokeHandleGeneric under AOT\n");

   if (comp()->getOption(TR_TraceILGen))
      {
      traceMsg(comp(), "expanding invokeHandleGeneric at n%dn\n", tree->getNode()->getGlobalIndex());
      }

   TR::Node * callNode = tree->getNode()->getChild(0);
   TR::Node * receiverHandle = callNode->getArgument(0);
   callNode->getByteCodeInfo().setDoNotProfile(true);

   TR::Node* callSiteMethodType = loadCallSiteMethodType(callNode);
   if (callSiteMethodType->getSymbolReference()->isUnresolved())
      {
      TR::Node *resolveChkOnMethodType = TR::Node::createWithSymRef(callNode, TR::ResolveCHK, 1, callSiteMethodType, comp()->getSymRefTab()->findOrCreateResolveCheckSymbolRef(_methodSymbol));
      tree->insertBefore(TR::TreeTop::create(comp(), resolveChkOnMethodType));
      }

   TR::SymbolReference *typeConversionSymRef = comp()->getSymRefTab()->methodSymRefFromName(_methodSymbol, JSR292_MethodHandle, JSR292_asType, JSR292_asTypeSig, TR::MethodSymbol::Static);
   TR::Node* asType = TR::Node::createWithSymRef(callNode, TR::acall, 2, typeConversionSymRef);
   asType->setAndIncChild(0, receiverHandle);
   asType->setAndIncChild(1, callSiteMethodType);
   asType->getByteCodeInfo().setDoNotProfile(true);
   tree->insertBefore(TR::TreeTop::create(comp(), TR::Node::create(callNode, TR::treetop, 1, asType)));

   if (comp()->getOption(TR_TraceILGen))
      {
      traceMsg(comp(), "Inserted asType call n%dn %p\n", asType->getGlobalIndex(), asType);
      }

   int32_t receiverChildIndex = callNode->getFirstArgumentIndex();
   callNode->setAndIncChild(receiverChildIndex, asType);
   receiverHandle->recursivelyDecReferenceCount();

   insertCustomizationLogicTreeIfEnabled(tree, asType);

   expandInvokeExact(tree);
   }

/** \brief
 *     Expand a MethodHandle.invokeExact call.
 *
 *     Transforms the following tree
 *
   @verbatim
     ... NULLCHK
     n1n   xcalli MethodHandle.invokeExact
     n2n     lconst 0
     n3n     $receiver
             ...args
   @endverbatim
 *
 * into
 *
   @verbatim
     ... NULLCHK
     ...   PassThrough
     n3n     $receiver
     ... lcall MethodHandle.invokeExactTargetAddress
             ==>$receiver
     n1n xcalli MethodHandle.invokeExact
           ==>lcall MethodHandle.invokeExactTargetAddress
           ==>$receiver
             ...args
   @endverbatim
 *
 *  \param tree
 *     Tree of the invokeExact call.
 */
void TR_J9ByteCodeIlGenerator::expandInvokeExact(TR::TreeTop *tree)
   {
   TR_ASSERT(!comp()->compileRelocatableCode(), "in expandInvokeExact under AOT\n");

   if (comp()->getOption(TR_TraceILGen))
      {
      traceMsg(comp(), "expanding invokeExact at n%dn\n", tree->getNode()->getGlobalIndex());
      }

   TR::Node * callNode = tree->getNode()->getChild(0);
   TR::Node * receiverHandle = callNode->getArgument(0);
   callNode->getByteCodeInfo().setDoNotProfile(true);

   // Get the method address
   uint32_t offset = fej9()->getInstanceFieldOffsetIncludingHeader("Ljava/lang/invoke/MethodHandle;", "thunks", "Ljava/lang/invoke/ThunkTuple;", method());
   TR::SymbolReference *thunksSymRef = comp()->getSymRefTab()->findOrFabricateShadowSymbol(_methodSymbol,
                                                                                           TR::Symbol::Java_lang_invoke_MethodHandle_thunks,
                                                                                           TR::Address,
                                                                                           offset,
                                                                                           false,
                                                                                           false,
                                                                                           false,
                                                                                           "java/lang/invoke/MethodHandle.thunks Ljava/lang/invoke/ThunkTuple;");
   TR::Node *thunksNode = TR::Node::createWithSymRef(callNode, comp()->il.opCodeForIndirectLoad(TR::Address), 1, receiverHandle, thunksSymRef);
   thunksNode->setIsNonNull(true);


   offset = fej9()->getInstanceFieldOffsetIncludingHeader("Ljava/lang/invoke/ThunkTuple;", "invokeExactThunk", "J", method());
   TR::SymbolReference *invokeExactTargetAddrSymRef = comp()->getSymRefTab()->findOrFabricateShadowSymbol(_methodSymbol,
                                                                                           TR::Symbol::Java_lang_invoke_ThunkTuple_invokeExactThunk,
                                                                                           TR::Int64,
                                                                                           offset,
                                                                                           false,
                                                                                           false,
                                                                                           true,
                                                                                           "java/lang/invoke/ThunkTuple.invokeExactThunk J");
   TR::Node *invokeExactTargetAddr = TR::Node::createWithSymRef(callNode, comp()->il.opCodeForIndirectLoad(TR::Int64), 1, thunksNode, invokeExactTargetAddrSymRef);
   tree->insertBefore(TR::TreeTop::create(comp(), TR::Node::create(callNode, TR::treetop, 1, invokeExactTargetAddr)));

   if (comp()->getOption(TR_TraceILGen))
      {
      traceMsg(comp(), "Replacing first child n%dn with invoke exact thunk address n%dn\n", callNode->getFirstChild()->getGlobalIndex(), invokeExactTargetAddr->getGlobalIndex());
      }

   // Replace the `lconst 0` node with the actual thunk address
   TR::Node *lnode = callNode->getFirstChild();
   callNode->setAndIncChild(0, invokeExactTargetAddr);
   lnode->decReferenceCount();
   }

/** \brief
 *     Expand MethodHandle.invokeExact calls generated from different bytecodes.
 *
 *  \param tree
 *     Tree of the invokeExact call.
 */
void TR_J9ByteCodeIlGenerator::expandMethodHandleInvokeCall(TR::TreeTop *tree)
   {
   TR::Node *ttNode = tree->getNode();
   TR::Node* callNode = ttNode->getFirstChild();
   TR::TreeTop* prevTree = tree->getPrevTreeTop();
   TR::TreeTop* nextTree = tree->getNextTreeTop();

   if (comp()->getOption(TR_TraceILGen))
      {
      traceMsg(comp(), "Found MethodHandle invoke call n%dn %p to expand\n", callNode->getGlobalIndex(), callNode);
      traceMsg(comp(), "   /--- Tree before expanding n%dn --------------------\n", callNode->getGlobalIndex());
      comp()->getDebug()->printWithFixedPrefix(comp()->getOutFile(), ttNode, 1, true, true, "      ");
      traceMsg(comp(), "\n");
      }

   int32_t oldBCIndex = _bcIndex;
   _bcIndex = callNode->getByteCodeIndex();

   // Preserve the NULLCHK
   //
   TR::TransformUtil::separateNullCheck(comp(), tree, comp()->getOption(TR_TraceILGen));
   // Anchor all children
   //
   for (int i = callNode->getFirstArgumentIndex(); i < callNode->getNumChildren(); i++)
      {
      TR::Node* child = callNode->getChild(i);
      TR::TreeTop *anchorTT = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, child));
      if (comp()->getOption(TR_TraceILGen))
         {
         traceMsg(comp(), "TreeTop n%dn is created to anchor node n%dn\n", anchorTT->getNode()->getGlobalIndex(), child->getGlobalIndex());
         }
       tree->insertBefore(anchorTT);
      }

   if (_invokeHandleCalls &&
       _invokeHandleCalls->isSet(_bcIndex))
      {
      expandInvokeHandle(tree);
      }
   else if (_invokeHandleGenericCalls &&
            _invokeHandleGenericCalls->isSet(_bcIndex))
      {
      expandInvokeHandleGeneric(tree);
      }
   else if (_invokeDynamicCalls &&
            _invokeDynamicCalls->isSet(_bcIndex))
      {
      expandInvokeDynamic(tree);
      }
   else if (_ilGenMacroInvokeExactCalls &&
            _ilGenMacroInvokeExactCalls->isSet(_bcIndex))
      {
      expandInvokeExact(tree);
      }
   else
      {
      TR_ASSERT(comp(), "Unexpected MethodHandle invoke call at n%dn %p", callNode->getGlobalIndex(), callNode);
      }

   // Specialize MethodHandle.invokeExact if the receiver handle is a known object
   TR::Node* methodHandle = callNode->getFirstArgument();
   if (methodHandle->getOpCode().hasSymbolReference()
       && methodHandle->getSymbolReference()->hasKnownObjectIndex())
      {
      TR::KnownObjectTable::Index index = methodHandle->getSymbolReference()->getKnownObjectIndex();
      uintptr_t* objectLocation = comp()->getKnownObjectTable()->getPointerLocation(index);
      TR::TransformUtil::specializeInvokeExactSymbol(comp(), callNode,  objectLocation);
      }

   _bcIndex = oldBCIndex;

   if (comp()->getOption(TR_TraceILGen))
      {
      traceMsg(comp(), "   /--- Trees after expanding n%dn --------------------\n", callNode->getGlobalIndex());
      TR::TreeTop *tt = prevTree->getNextTreeTop();
      do
         {
         comp()->getDebug()->printWithFixedPrefix(comp()->getOutFile(), tt->getNode(), 1, true, true, "      ");
         traceMsg(comp(), "\n");
         tt = tt->getNextTreeTop();
         } while( tt != nextTree);
      }
   }
