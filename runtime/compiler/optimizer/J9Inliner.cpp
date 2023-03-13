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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "optimizer/Inliner.hpp"
#include "optimizer/J9Inliner.hpp"

#include <algorithm>
#include "env/KnownObjectTable.hpp"
#include "compile/OSRData.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/CompilerEnv.hpp"
#include "env/CHTable.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/VMJ9.h"
#include "env/jittypes.h"
#include "env/VMAccessCriticalSection.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "optimizer/CallInfo.hpp"
#include "optimizer/J9CallGraph.hpp"
#include "optimizer/PreExistence.hpp"
#include "optimizer/RematTools.hpp"
#include "optimizer/Structure.hpp"
#include "runtime/J9Profiler.hpp"
#include "runtime/J9ValueProfiler.hpp"
#include "codegen/CodeGenerator.hpp"
#include "ilgen/J9ByteCode.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"

#define OPT_DETAILS "O^O INLINER: "
const float MIN_PROFILED_CALL_FREQUENCY = (.65f); // lowered this from .80f since opportunities were being missed in WAS; in those cases getting rid of the call even in 65% of the cases was beneficial probably due to the improved icache impact

extern int32_t          *NumInlinedMethods;  // Defined in Inliner.cpp
extern int32_t          *InlinedSizes;       // Defined in Inliner.cpp


//duplicated as long as there are two versions of findInlineTargets
static uintptr_t *failMCS(char *reason, TR_CallSite *callSite, TR_InlinerBase* inliner)
   {
   debugTrace(inliner->tracer(),"  Fail isMutableCallSiteTargetInvokeExact(%p): %s", callSite, reason);
   return NULL;
   }

static uintptr_t *isMutableCallSiteTargetInvokeExact(TR_CallSite *callSite, TR_InlinerBase *inliner)
   {
   // Looking for either mcs.target.invokeExact(...) or mcs.getTarget().invokeExact(...)
   // on some known/fixed MutableCallSite object mcs.
   // Return NULL if it's neither of these.

   if (inliner->comp()->getOption(TR_DisableMutableCallSiteGuards))
      return NULL;

   TR::Node *callNode = callSite->_callNode;
   if (!callNode || !callNode->getOpCode().isCall())
      return failMCS("No call node", callSite, inliner);
   else if (callNode->getSymbolReference()->isUnresolved())
      return failMCS("Call symref is unresolved", callSite, inliner);
   else switch (callNode->getSymbol()->castToResolvedMethodSymbol()->getRecognizedMethod())
      {
      case TR::java_lang_invoke_MethodHandle_invokeExact:
         break;
      default:
         return failMCS("Call symref is not invokeExact", callSite, inliner);
      }

   TR::Node *targetNode = callNode->getChild(callNode->getFirstArgumentIndex());
   if (!targetNode->getOpCode().hasSymbolReference() || targetNode->getSymbolReference()->isUnresolved())
      return failMCS("No target symref", callSite, inliner);

   TR::Node *mcsNode = (TR::Node*)(intptr_t)0xdead;
   if (targetNode->getOpCode().isCall())
      {
      switch (targetNode->getSymbol()->castToResolvedMethodSymbol()->getRecognizedMethod())
         {
         case TR::java_lang_invoke_MutableCallSite_getTarget:
            mcsNode = targetNode->getChild(targetNode->getFirstArgumentIndex());
            break;
         default:
            return failMCS("Call receiver isn't a call to getTarget", callSite, inliner);
         }
      }
   else if (targetNode->getOpCode().isLoadIndirect() && targetNode->getDataType() == TR::Address)
      {
      switch (targetNode->getSymbol()->getRecognizedField())
         {
         case TR::Symbol::Java_lang_invoke_MutableCallSite_target:
            mcsNode = targetNode->getFirstChild();
            break;
         default:
            return failMCS("Call receiver isn't a load of target field", callSite, inliner);
         }
      }
   else
      {
      return failMCS("Unsuitable call receiver", callSite, inliner);
      }

   if (mcsNode->getSymbolReference()->hasKnownObjectIndex())
      {
      uintptr_t *result = mcsNode->getSymbolReference()->getKnownObjectReferenceLocation(inliner->comp());
      heuristicTrace(inliner->tracer(), "  Success: isMutableCallSiteTargetInvokeExact(%p)=%p (obj%d)", callSite, result, mcsNode->getSymbolReference()->getKnownObjectIndex());
      return result;
      }
   else if (mcsNode->getSymbol()->isFixedObjectRef())
      {
      uintptr_t *result = (uintptr_t*)mcsNode->getSymbol()->castToStaticSymbol()->getStaticAddress();
      heuristicTrace(inliner->tracer(),"  Success: isMutableCallSiteTargetInvokeExact(%p)=%p (fixed object reference)", callSite, result);
      return result;
      }
   else
      {
      return failMCS("Unknown MutableCallSite object", callSite, inliner);
      }
   }



TR_CallSite* TR_CallSite::create(TR::TreeTop* callNodeTreeTop,
                           TR::Node *parent,
                           TR::Node* callNode,
                           TR_OpaqueClassBlock *receiverClass,
                           TR::SymbolReference *symRef,
                           TR_ResolvedMethod *resolvedMethod,
                           TR::Compilation* comp,
                           TR_Memory* trMemory,
                           TR_AllocationKind kind,
                           TR_ResolvedMethod* caller,
                           int32_t depth,
                           bool allConsts)

   {

   TR::MethodSymbol *calleeSymbol = symRef->getSymbol()->castToMethodSymbol();
   TR_ResolvedMethod* lCaller = caller ? caller : symRef->getOwningMethod(comp);

   if (callNode->getOpCode().isCallIndirect())
      {
      if (calleeSymbol->isInterface() )
         {
         return new (trMemory, kind) TR_J9InterfaceCallSite (lCaller,
                              callNodeTreeTop,
                              parent,
                              callNode,
                              calleeSymbol->getMethod(),
                              receiverClass,
                              (int32_t)symRef->getOffset(),
                              symRef->getCPIndex(),
                              resolvedMethod,
                              calleeSymbol->getResolvedMethodSymbol(),
                              callNode->getOpCode().isCallIndirect(),
                              calleeSymbol->isInterface(),
                              callNode->getByteCodeInfo(),
                              comp,
                              depth,
                              allConsts);
         }
      else
         {
         if (calleeSymbol->getResolvedMethodSymbol() &&
            calleeSymbol->getResolvedMethodSymbol()->getResolvedMethod()->convertToMethod()->isArchetypeSpecimen() &&
            calleeSymbol->getResolvedMethodSymbol()->getResolvedMethod()->getMethodHandleLocation())
            {
            return new (trMemory, kind) TR_J9MethodHandleCallSite  (lCaller,
                     callNodeTreeTop,
                     parent,
                     callNode,
                     calleeSymbol->getMethod(),
                     receiverClass,
                     (int32_t)symRef->getOffset(),
                     symRef->getCPIndex(),
                     resolvedMethod,
                     calleeSymbol->getResolvedMethodSymbol(),
                     callNode->getOpCode().isCallIndirect(),
                     calleeSymbol->isInterface(),
                     callNode->getByteCodeInfo(),
                     comp,
                     depth,
                     allConsts) ;
            }

         if (calleeSymbol->getResolvedMethodSymbol() && calleeSymbol->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_lang_invoke_MethodHandle_invokeExact)
            {
            return new (trMemory, kind) TR_J9MutableCallSite  (lCaller,
                  callNodeTreeTop,
                  parent,
                  callNode,
                  calleeSymbol->getMethod(),
                  receiverClass,
                  (int32_t)symRef->getOffset(),
                  symRef->getCPIndex(),
                  resolvedMethod,
                  calleeSymbol->getResolvedMethodSymbol(),
                  callNode->getOpCode().isCallIndirect(),
                  calleeSymbol->isInterface(),
                  callNode->getByteCodeInfo(),
                  comp,
                  depth,
                  allConsts) ;
            }

         return new (trMemory, kind) TR_J9VirtualCallSite  (lCaller,
                              callNodeTreeTop,
                              parent,
                              callNode,
                              calleeSymbol->getMethod(),
                              receiverClass,
                              (int32_t)symRef->getOffset(),
                              symRef->getCPIndex(),
                              resolvedMethod,
                              calleeSymbol->getResolvedMethodSymbol(),
                              callNode->getOpCode().isCallIndirect(),
                              calleeSymbol->isInterface(),
                              callNode->getByteCodeInfo(),
                              comp,
                              depth,
                              allConsts) ;

         }
      }

      return new (trMemory, kind) TR_DirectCallSite (lCaller,
                              callNodeTreeTop,
                              parent,
                              callNode,
                              calleeSymbol->getMethod(),
                              resolvedMethod && !resolvedMethod->isStatic() ? receiverClass : NULL,
                              (int32_t)symRef->getOffset(),
                              symRef->getCPIndex(),
                              resolvedMethod,
                              calleeSymbol->getResolvedMethodSymbol(),
                              callNode->getOpCode().isCallIndirect(),
                              calleeSymbol->isInterface(),
                              callNode->getByteCodeInfo(),
                              comp,
                              depth,
                              allConsts) ;

   }



static void computeNumLivePendingSlotsAndNestingDepth(TR::Optimizer* optimizer, TR_CallTarget* calltarget, TR_CallStack* callStack, int32_t& numLivePendingPushSlots, int32_t& nestingDepth)
   {
   TR::Compilation *comp = optimizer->comp();

   if (comp->getOption(TR_EnableOSR))
       {
       TR::Block *containingBlock = calltarget->_myCallSite->_callNodeTreeTop->getEnclosingBlock();
        int32_t weight = 1;
        nestingDepth = weight/10;

       TR::Node *callNode = calltarget->_myCallSite->_callNode;
       int32_t callerIndex = callNode->getByteCodeInfo().getCallerIndex();
       TR::ResolvedMethodSymbol *caller = (callerIndex == -1) ? comp->getMethodSymbol()
                                                              : comp->getInlinedResolvedMethodSymbol(callerIndex);
       TR_OSRMethodData *osrMethodData = comp->getOSRCompilationData()->findOrCreateOSRMethodData(callerIndex, caller);
       TR_Array<List<TR::SymbolReference> > *pendingPushSymRefs = caller->getPendingPushSymRefs();

       int32_t numPendingSlots = 0;

       if (pendingPushSymRefs)
          numPendingSlots = pendingPushSymRefs->size();

       TR_BitVector *deadSymRefs = osrMethodData->getLiveRangeInfo(calltarget->_myCallSite->_callNode->getByteCodeIndex());

       for (int32_t i=0;i<numPendingSlots;i++)
         {
         List<TR::SymbolReference> symRefsAtThisSlot = (*pendingPushSymRefs)[i];

         if (symRefsAtThisSlot.isEmpty()) continue;

         ListIterator<TR::SymbolReference> symRefsIt(&symRefsAtThisSlot);
         TR::SymbolReference *nextSymRef;
         for (nextSymRef = symRefsIt.getCurrent(); nextSymRef; nextSymRef=symRefsIt.getNext())
            {
            if (!deadSymRefs || !deadSymRefs->get(nextSymRef->getReferenceNumber()))
               numLivePendingPushSlots++;
            }
         }

       optimizer->comp()->incNumLivePendingPushSlots(numLivePendingPushSlots);
       optimizer->comp()->incNumLoopNestingLevels(nestingDepth);
       }
   }

/*
 * Populate the OSRCallSiteRematTable using the pending push stores before this call.
 * To achieve this, RematTools is applied to the pending pushes that correspond to the call,
 * however, it is limited to using autos, parms and pending push temps as others may be
 * modified within the call.
 */
static void populateOSRCallSiteRematTable(TR::Optimizer* optimizer, TR_CallTarget* calltarget,
   TR_CallStack* callStack)
   {
   static const char *verboseCallSiteRemat = feGetEnv("TR_VerboseOSRCallSiteRemat");
   TR::TreeTop *call = calltarget->_myCallSite->_callNodeTreeTop;
   TR::ResolvedMethodSymbol *method = callStack->_methodSymbol;
   TR::Compilation *comp = optimizer->comp();
   TR_ByteCodeInfo &bci = method->getOSRByteCodeInfo(call->getNode());
   TR::TreeTop *blockStart = call->getEnclosingBlock()->getFirstRealTreeTop();

   TR::SparseBitVector scanTargets(comp->allocator());
   RematSafetyInformation safetyInfo(comp);
   TR::list<TR::TreeTop *> failedPP(getTypedAllocator<TR::TreeTop*>(comp->allocator()));

   // Search through all of the PPS for those that can be remated
   //
   for (
      TR::TreeTop *cursor = call->getPrevTreeTop();
      cursor && method->isOSRRelatedNode(cursor->getNode(), bci);
      cursor = cursor->getPrevTreeTop())
      {
      TR::Node *store = cursor->getNode();
      if (!store->getOpCode().isStoreDirect() || !store->getSymbol()->isPendingPush())
         continue;

      TR::Node *child = store->getFirstChild();
      // A PPS of an auto/parm. Necessary to scan to check if auto/parm has not been modified
      // since it was anchored.
      //
      int32_t callerIndex = child->getByteCodeInfo().getCallerIndex();

      if (child->getOpCode().hasSymbolReference()
          && (child->getSymbol()->isParm()
             || (child->getSymbol()->isAuto()
                 && child->getSymbolReference()->getCPIndex() <
                    (( (callerIndex == -1) ? comp->getMethodSymbol()
                                           : comp->getInlinedResolvedMethodSymbol(callerIndex) )->getFirstJitTempIndex()))))
         {
         if (comp->trace(OMR::inlining))
            traceMsg(comp, "callSiteRemat: found potential pending push #%d with store #%d\n", store->getSymbolReference()->getReferenceNumber(),
               child->getSymbolReference()->getReferenceNumber());

         TR::SparseBitVector symRefsToCheck(comp->allocator());
         symRefsToCheck[child->getSymbolReference()->getReferenceNumber()] = true;
         scanTargets[child->getGlobalIndex()] = true;
         safetyInfo.add(cursor, symRefsToCheck);
         }

      // Storing failures, will search for a double store that occurs before
      //
      else
         {
         if (comp->trace(OMR::inlining))
            traceMsg(comp, "callSiteRemat: failed to find store for pending push #%d\n", store->getSymbolReference()->getReferenceNumber());

         failedPP.push_back(cursor);
         }
      }

   // Perform search for any double stores
   // This goes from the start of the block to the call, as PPs may store
   // duplicate values
   //
   if (failedPP.size() > 0)
      RematTools::walkTreeTopsCalculatingRematFailureAlternatives(comp,
         blockStart, call, failedPP, scanTargets, safetyInfo, verboseCallSiteRemat != NULL);

   // Perform the safety check, to ensure symrefs haven't been
   // modified.
   //
   TR::SparseBitVector unsafeSymRefs(comp->allocator());
   if (!scanTargets.IsZero())
      RematTools::walkTreesCalculatingRematSafety(comp, blockStart,
         call, scanTargets, unsafeSymRefs, verboseCallSiteRemat != NULL);

   // Perform place those without unsafe symrefs in the remat table
   //
   for (uint32_t i = 0; i < safetyInfo.size(); ++i)
      {
      TR::TreeTop *storeTree = safetyInfo.argStore(i);
      TR::TreeTop *rematTree = safetyInfo.rematTreeTop(i);
      TR::Node *node = rematTree->getNode();
      TR::Node *child = node->getFirstChild();

      if (!unsafeSymRefs.Intersects(safetyInfo.symRefDependencies(i)))
         {
         if (storeTree == rematTree)
            {
            if (comp->trace(OMR::inlining))
               traceMsg(comp, "callSiteRemat: adding pending push #%d with store #%d to remat table\n",
                  storeTree->getNode()->getSymbolReference()->getReferenceNumber(),
                  child->getSymbolReference()->getReferenceNumber());

            comp->setOSRCallSiteRemat(comp->getCurrentInlinedSiteIndex(),
               storeTree->getNode()->getSymbolReference(),
               child->getSymbolReference());
            }
         else
            {
            int32_t callerIndex = node->getByteCodeInfo().getCallerIndex();
            if (node->getSymbol()->isParm()
               || node->getSymbol()->isPendingPush()
               || (node->getSymbol()->isAuto()
                  && node->getSymbolReference()->getCPIndex() <
                        (( (callerIndex == -1) ? comp->getMethodSymbol()
                                               : comp->getInlinedResolvedMethodSymbol(callerIndex) )->getFirstJitTempIndex())))
               {
               if (comp->trace(OMR::inlining))
                  traceMsg(comp, "callSiteRemat: adding pending push #%d with store #%d to remat table\n",
                     storeTree->getNode()->getSymbolReference()->getReferenceNumber(),
                     node->getSymbolReference()->getReferenceNumber());

               comp->setOSRCallSiteRemat(comp->getCurrentInlinedSiteIndex(),
                  storeTree->getNode()->getSymbolReference(),
                  node->getSymbolReference());
               }
            }
         }
      }
   }

bool TR_InlinerBase::inlineCallTarget(TR_CallStack *callStack, TR_CallTarget *calltarget, bool inlinefromgraph, TR_PrexArgInfo *argInfo, TR::TreeTop** cursorTreeTop)
   {

   TR_InlinerDelimiter delimiter(tracer(),"TR_InlinerBase::inlineCallTarget");

   char *sig = "multiLeafArrayCopy";
   if (strncmp(calltarget->_calleeMethod->nameChars(), sig, strlen(sig)) == 0)
      {
      _nodeCountThreshold = 8192;
      heuristicTrace(tracer(),"Setting _nodeCountThreshold to %d for multiLeafArrayCopy",_nodeCountThreshold);
      }

   if (!((TR_J9InlinerPolicy* )getPolicy())->doCorrectnessAndSizeChecksForInlineCallTarget(callStack, calltarget, inlinefromgraph, argInfo))
      {
      //@TODO do we need to undo _nodeCountThreshold???!
      return false;
      }

   // Last chance to improve our prex info
   //
   if (!calltarget->_prexArgInfo)
      calltarget->_prexArgInfo = getUtil()->computePrexInfo(calltarget);

   argInfo = TR_PrexArgInfo::enhance(calltarget->_prexArgInfo, argInfo, comp());
   calltarget->_prexArgInfo = argInfo;
   bool tracePrex = comp()->trace(OMR::inlining) || comp()->trace(OMR::invariantArgumentPreexistence);
   if (tracePrex && argInfo)
      {
      traceMsg(comp(), "Final prex argInfo:\n");
      argInfo->dumpTrace();
      }

   if (!comp()->incInlineDepth(calltarget->_calleeSymbol,
                               calltarget->_myCallSite->_callNode,
                               !calltarget->_myCallSite->_isIndirectCall,
                               calltarget->_guard,
                               calltarget->_receiverClass,
                               argInfo))
		{
		return false;
		}

   //OSR
   int32_t numLivePendingPushSlots = 0;
   int32_t nestingDepth = 0;
   if (comp()->getOption(TR_EnableOSR))
      {
      computeNumLivePendingSlotsAndNestingDepth(_optimizer, calltarget, callStack, numLivePendingPushSlots, nestingDepth);
      }

   // Add the pending pushes above this call to the OSRCallSiteRematTable
   if (comp()->getOption(TR_EnableOSR)
       && !comp()->getOption(TR_DisableOSRCallSiteRemat)
       && comp()->getOSRMode() == TR::voluntaryOSR
       && comp()->isOSRTransitionTarget(TR::postExecutionOSR)
       && comp()->isPotentialOSRPointWithSupport(calltarget->_myCallSite->_callNodeTreeTop)
       && performTransformation(comp(), "O^O CALL SITE REMAT: populate OSR call site remat table for call [%p]\n", calltarget->_myCallSite->_callNode))
      {
      if (comp()->trace(OMR::inlining))
         traceMsg(comp(), "callSiteRemat: populating OSR call site remat table for call [%p]\n", calltarget->_myCallSite->_callNode);
      populateOSRCallSiteRematTable(_optimizer, calltarget, callStack);
      }

   bool successful = inlineCallTarget2(callStack, calltarget, cursorTreeTop, inlinefromgraph, 99);

   // if inlining fails, we need to tell decInlineDepth to remove elements that
   // we added during inlineCallTarget2
   comp()->decInlineDepth(!successful);

   if (comp()->getOption(TR_EnableOSR))
       {
       comp()->incNumLivePendingPushSlots(-numLivePendingPushSlots);
       comp()->incNumLoopNestingLevels(-nestingDepth);
       }

    if (NumInlinedMethods != NULL)
       {
       NumInlinedMethods[comp()->getMethodHotness()]++;
       InlinedSizes[comp()->getMethodHotness()] += TR::Compiler->mtd.bytecodeSize(calltarget->_calleeSymbol->getResolvedMethod()->getPersistentIdentifier());
       }
   return successful;
   }

TR_ResolvedMethod* TR_J9VirtualCallSite::findSingleJittedImplementer (TR_InlinerBase *inliner)
   {
   return comp()->getPersistentInfo()->getPersistentCHTable()->findSingleJittedImplementer(_receiverClass,TR::Compiler->cls.isInterfaceClass(comp(), _receiverClass) ? _cpIndex : _vftSlot,_callerResolvedMethod, comp(), _initialCalleeSymbol);
   }

bool TR_J9VirtualCallSite::findCallSiteForAbstractClass(TR_InlinerBase* inliner)
   {
   TR_PersistentCHTable *chTable = comp()->getPersistentInfo()->getPersistentCHTable();
   TR_ResolvedMethod *implementer;

   bool canInline = (!comp()->compileRelocatableCode() || comp()->getOption(TR_UseSymbolValidationManager));
   if (canInline && TR::Compiler->cls.isAbstractClass(comp(), _receiverClass) &&!comp()->getOption(TR_DisableAbstractInlining) &&
       (implementer = chTable->findSingleAbstractImplementer(_receiverClass, _vftSlot, _callerResolvedMethod, comp())))
      {
      heuristicTrace(inliner->tracer(),"Found a single Abstract Implementer %p, signature = %s",implementer,inliner->tracer()->traceSignature(implementer));
      TR_VirtualGuardSelection *guard = new (comp()->trHeapMemory()) TR_VirtualGuardSelection(TR_AbstractGuard, TR_MethodTest);
      addTarget(comp()->trMemory(),inliner,guard,implementer,_receiverClass,heapAlloc);
      return true;
      }

   return false;
   }

TR_OpaqueClassBlock* TR_J9VirtualCallSite::getClassFromMethod ()
   {
   return _initialCalleeMethod->classOfMethod();
   }

// Ensure the call site is a basic invokevirtual; the bytecode is an 'invokevirtaul' and the
// cpIndex is the same as the call site _cpIndex. This will insure that the call site is not
// some type of transformed call site that may not be a valid case for allowing an isInstanceOf()
// call during AOT compiles
bool TR_J9VirtualCallSite::isBasicInvokeVirtual()
   {
   TR_OpaqueMethodBlock *method = ((TR_ResolvedJ9Method*)_initialCalleeMethod->owningMethod())->getPersistentIdentifier();
   int32_t methodSize = TR::Compiler->mtd.bytecodeSize(method);
   uintptr_t methodStart = TR::Compiler->mtd.bytecodeStart(method);

   TR_ASSERT_FATAL(_bcInfo.getByteCodeIndex() >= 0 && _bcInfo.getByteCodeIndex()+2 < methodSize, "Bytecode index can't be less than zero or higher than the methodSize");

   uint8_t *pc = (uint8_t *)(methodStart + _bcInfo.getByteCodeIndex());
   TR_J9ByteCode bytecode = TR_J9ByteCodeIterator::convertOpCodeToByteCodeEnum(*pc);
   //fprintf( stderr, "method %p, size %d, start %p, PC %p, BC: %d==%d? (%d)\n", method, methodSize, methodStart, pc, bytecode, J9BCinvokevirtual, (bytecode==J9BCinvokevirtual));
   if (bytecode==J9BCinvokevirtual)
      {
      uint16_t cpIndex = *(uint16_t*)(pc + 1);
      //fprintf( stderr, "BC cpIndex %d, callSite cpIndex %d\n", cpIndex, _cpIndex );
      if (_cpIndex==cpIndex)
         {
         return true;
         }
      }
   return false;
   }

bool TR_J9VirtualCallSite::findCallSiteTarget(TR_CallStack *callStack, TR_InlinerBase* inliner)
   {
   if (hasFixedTypeArgInfo())
      {
      bool result = findCallTargetUsingArgumentPreexistence(inliner);
      if (!result) //findCallTargetUsingArgumentPreexistence couldn't reconcile class types
         {
         heuristicTrace(inliner->tracer(), "Don't inline anything at the risk of inlining dead code");
         return false;
         }

      if (numTargets()) //findCallTargetUsingArgumentPreexistence added a target
         {
         return true;
         }

      //findCallTargetUsingArgumentPreexistence couldn't use argInfo
      //Clear _ecsPrexArgInfo so it isn't propagated down to callees of this callsite
      //And try other techniques
      _ecsPrexArgInfo->set(0, NULL);
      }

   tryToRefineReceiverClassBasedOnResolvedTypeArgInfo(inliner);

   // Refine receiver class based on CP class
   // When we have an invokevirtual on an abstract method defined in an interface class,
   // the call site's class will be more concrete than class of method.
   // This happens when an abstract class implements an interface class without providing
   // implementation for the given method, and the call site is refering to the method of
   // the abstract class, the cp entry of the method ref will be resolved to j9method of
   // the interface class. However, the class ref from cp will be resolved to the abstract
   // class, which is more concrete
   //
   if (_cpIndex != -1 && _receiverClass && TR::Compiler->cls.isInterfaceClass(comp(), _receiverClass) && isBasicInvokeVirtual())
      {
      TR_ResolvedMethod* owningMethod = _initialCalleeMethod->owningMethod();
      TR_ResolvedJ9Method* j9OwningMethod = (TR_ResolvedJ9Method*)owningMethod;
      int32_t nameLen=0, sigLen=0;
      char *cpMethodName = j9OwningMethod->getMethodNameFromConstantPool(_cpIndex, nameLen);
      char *cpMethodSig  = j9OwningMethod->getMethodSignatureFromConstantPool(_cpIndex, sigLen);
      char *methodName   = _initialCalleeMethod->nameChars();
      char *methodSig    = _initialCalleeMethod->signatureChars();
      if (nameLen && nameLen == _initialCalleeMethod->nameLength() && sigLen && sigLen == _initialCalleeMethod->signatureLength() &&
         strncmp(cpMethodName, methodName, nameLen)==0 && strncmp(cpMethodSig, methodSig, sigLen)==0)
         {
         int32_t classRefCPIndex = owningMethod->classCPIndexOfMethod(_cpIndex);
         TR_OpaqueClassBlock* callSiteClass = owningMethod->getClassFromConstantPool(comp(), classRefCPIndex, true);
         if (callSiteClass && callSiteClass != _receiverClass)
            {
            if (comp()->fej9()->isJavaLangObject(callSiteClass))
               _isCallingObjectMethod = TR_yes;
            else
               {
               // Verify subtyping against the defining interface rather than
               // _receiverClass, since the latter could have been refined to a
               // more specific interface.
               TR_OpaqueClassBlock *defInterface = getClassFromMethod();
               TR_YesNoMaybe callSiteClassOk = fe()->isInstanceOf(
                  callSiteClass, defInterface, true, true, true);

               TR_ASSERT_FATAL(
                  callSiteClassOk == TR_yes,
                  "class %p inherits a method from interface %p without implementing it",
                  callSiteClass,
                  defInterface);

               _isCallingObjectMethod = TR_no;
               if (comp()->trace(OMR::inlining))
                  {
                  char* oldClassSig = TR::Compiler->cls.classSignature(comp(), _receiverClass, comp()->trMemory());
                  char* callSiteClassSig = TR::Compiler->cls.classSignature(comp(), callSiteClass, comp()->trMemory());
                  traceMsg(comp(), "Receiver type %p sig %s is class of an interface method for invokevirtual, improve it to call site receiver type %p sig %s\n", _receiverClass, oldClassSig, callSiteClass, callSiteClassSig);
                  }
               // Update receiver class
               _receiverClass = callSiteClass;
               }
            }
         }
      }

   if (addTargetIfMethodIsNotOverriden(inliner) ||
      addTargetIfMethodIsNotOverridenInReceiversHierarchy(inliner) ||
      findCallSiteForAbstractClass(inliner) ||
      addTargetIfThereIsSingleImplementer(inliner))
      {
      return true;
      }

   return findProfiledCallTargets(callStack, inliner);
   }

/*
static TR_ResolvedMethod * findSingleImplementer(
   TR_OpaqueClassBlock * thisClass, int32_t cpIndexOrVftSlot, TR_ResolvedMethod * callerMethod, TR::Compilation * comp, bool locked, TR_YesNoMaybe useGetResolvedInterfaceMethod)
   {
   if (comp->getOption(TR_DisableCHOpts))
      return 0;



   TR_PersistentClassInfo * classInfo = comp->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(thisClass, comp, true);
   if (!classInfo)
      {
      return 0;
      }

   TR_ResolvedMethod *implArray[2]; // collect maximum 2 implementers if you can
   int32_t implCount = TR_ClassQueries::collectImplementorsCapped(classInfo, implArray, 2, cpIndexOrVftSlot, callerMethod, comp, locked, useGetResolvedInterfaceMethod);
   return (implCount == 1 ? implArray[0] : 0);
   }
*/

bool TR_J9InterfaceCallSite::findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner)
   {
   // First make sure that we can get the interface named at this call site. It
   // may be missing in AOT, since it comes from getClassFromSignature(), which
   // needs validation, and rememberClass() can fail at any time. The interface
   // is necessary for safety conditions in findCallSiteTargetImpl() and also
   // for the assertions below.
   TR_OpaqueClassBlock *iface = getClassFromMethod();
   if (iface == NULL)
      return false;

   bool result = findCallSiteTargetImpl(callStack, inliner, iface);

   // A passing vgnop-based interface guard can guarantee the receiver type is
   // as expected by the inlined body, but only if we already know before the
   // guard that the receiver implements the expected interface. Here, an
   // interface type bound is insufficient to know that, because bytecode
   // verification does not guarantee any particular receiver type at
   // invokeinterface.
   //
   // As such, in the absence of a class (i.e. non-interface) type bound on the
   // receiver, all targets must use profiled guard. Additionally, the profiled
   // guard must ensure on the hot side that the receiver is an instance of the
   // interface expected at this call site.
   //
   // These requirements could be relaxed in the future by generating a type
   // check against the interface type at the beginning of the inlined body
   // (taking care to ensure that the exception successors are the same as
   // those of the call, rather than those of the first block of the callee).
   //
   //    ZEROCHK jitThrowIncompatibleClassChangeError
   //      instanceof
   //        <receiver>
   //        loadaddr <interface>
   //
   // In particular, such a type check would allow the following cases:
   //
   // - A default method inlined using a nonoverridden guard. Currently,
   //   default methods are never inlined (at interface call sites) because
   //   TR_ResolvedJ9Method::getResolvedInterfaceMethod() does not return them.
   //
   // - A profiled guard with method test for a method defined by a class that
   //   does not implement the expected interface. Subtypes may still implement
   //   the interface.
   //
   // If/when we want to start inlining in one or both of these cases, the
   // assertions below can be relaxed accordingly. However, testing instanceof
   // against the interface will be expensive, so there would have to be a
   // considerable benefit to the inlining to motivate such a change.
   //
   if (_receiverClass != NULL
       && !TR::Compiler->cls.isInterfaceClass(comp(), _receiverClass))
      {
      TR_ASSERT_FATAL(
         fe()->isInstanceOf(_receiverClass, iface, true, true, true) == TR_yes,
         "interface call site %p receiver type %p "
         "does not implement the expected interface %p",
         this,
         _receiverClass,
         iface);

      heuristicTrace(
         inliner->tracer(),
         "Interface call site %p has receiver class bound %p; nop guards ok",
         this,
         _receiverClass);
      }
   else
      {
      TR_Debug *debug = comp()->getDebug();
      for (int32_t i = 0; i < numTargets(); i++)
         {
         TR_CallTarget *tgt = getTarget(i);
         TR_VirtualGuardKind kind = tgt->_guard->_kind;
         TR_ASSERT_FATAL(
            kind == TR_ProfiledGuard,
            "interface call site %p requires profiled guard (kind %d), "
            "but target %d [%p] uses %s (kind %d)",
            this,
            (int)TR_ProfiledGuard,
            i,
            tgt,
            debug == NULL ? "<unknown name>" : debug->getVirtualGuardKindName(kind),
            (int)kind);

         // Bound on the receiver types that pass the profiled guard
         TR_OpaqueClassBlock *passClass = NULL;
         TR_ResolvedMethod *callee = tgt->_calleeMethod;
         if (tgt->_guard->_type == TR_VftTest)
            passClass = tgt->_receiverClass;
         else
            passClass = callee->containingClass();

         TR_ASSERT_FATAL(
            fe()->isInstanceOf(passClass, iface, true, true, true) == TR_yes,
            "interface call site %p target %d [%p] (J9Method %p) "
            "accepts receivers of type %p, "
            "which does not implement the expected interface %p",
            this,
            i,
            tgt,
            callee->getPersistentIdentifier(),
            passClass,
            iface);
         }
      }

   return result;
   }

bool TR_J9InterfaceCallSite::findCallSiteTargetImpl(
   TR_CallStack *callStack, TR_InlinerBase *inliner, TR_OpaqueClassBlock *iface)
   {
   TR_ASSERT_FATAL(iface != NULL, "no declaring interface");

   static char *minimizedInlineJIT = feGetEnv("TR_JITInlineMinimized");

   if (minimizedInlineJIT)
      return false;

   if (hasFixedTypeArgInfo())
      {
      bool result = findCallTargetUsingArgumentPreexistence(inliner);
      if (!result) //findCallTargetUsingArgumentPreexistence couldn't reconcile class types
         {
         heuristicTrace(inliner->tracer(), "Don't inline anything at the risk of inlining dead code");
         return false;
         }

      if (numTargets()) //findCallTargetUsingArgumentPreexistence added a target
         {
         return true;
         }

      //findCallTargetUsingArgumentPreexistence couldn't use argInfo
      //Clear _ecsPrexArgInfo so it wont be propagated down to callees of this callsite
      //And try other techniques
      _ecsPrexArgInfo->set(0, NULL);
      }

   if (!_receiverClass)
      {
      int32_t len = _interfaceMethod->classNameLength();
      char * s = TR::Compiler->cls.classNameToSignature(_interfaceMethod->classNameChars(), len, comp());
      _receiverClass = comp()->fej9()->getClassFromSignature(s, len, _callerResolvedMethod, true);
      }

   //TR_OpaqueClassBlock* _receiverClass = NULL;
   tryToRefineReceiverClassBasedOnResolvedTypeArgInfo(inliner);

   //TR_ResolvedMethod* calleeResolvedMethod = inliner->findInterfaceImplementationToInline(_interfaceMethod, _cpIndex, _callerResolvedMethod, _receiverClass);
   TR_ResolvedMethod* calleeResolvedMethod = comp()->getPersistentInfo()->getPersistentCHTable()->findSingleImplementer(_receiverClass, _cpIndex, _callerResolvedMethod, inliner->comp(), false, TR_yes);

   if (!comp()->performVirtualGuardNOPing() || (comp()->compileRelocatableCode() && !TR::Options::getCmdLineOptions()->allowRecompilation()))
      {
      calleeResolvedMethod = NULL;
      }

   heuristicTrace(inliner->tracer(), "Found a Single Interface Implementer with Resolved Method %p for callsite %p",calleeResolvedMethod,this);

   if (calleeResolvedMethod && !calleeResolvedMethod->virtualMethodIsOverridden())
      {
      TR_VirtualGuardKind kind = TR_ProfiledGuard;
      TR_VirtualGuardTestType testType = TR_DummyTest;
      TR_OpaqueClassBlock *thisClass = _receiverClass;
      if (_receiverClass != NULL
          && !TR::Compiler->cls.isInterfaceClass(comp(), _receiverClass))
         {
         kind = TR_InterfaceGuard;
         testType = TR_MethodTest;
         }
      else
         {
         // Whether to try to choose VFT test based on a non-extended defClass
         // or based on profiling. This does not affect the final defClass
         // heuristic because in that case we can be certain that the class
         // won't be extended later.
         bool useVftTestHeuristics = true;
         TR::PersistentInfo *persistInfo = comp()->getPersistentInfo();
         if (TR::Compiler->vm.isVMInStartupPhase(comp()->fej9()->getJ9JITConfig()))
            {
            const static bool useVftTestHeuristicsDuringStartup =
               feGetEnv("TR_useInterfaceVftTestHeuristicsDuringStartup") != NULL;

            useVftTestHeuristics = useVftTestHeuristicsDuringStartup;
            }

         // Profiled guards must guarantee that passing receivers are instances
         // of some class that implements the expected interface. See the
         // comment in TR_J9InterfaceCallSite::findCallSiteTarget().
         //
         // The choice of VFT test vs. method test is irrelevant here, as
         // either one would accept instances of defClass. However, a VFT test
         // obtained by consulting the profiled receiver types would work.
         //
         TR_OpaqueClassBlock *defClass = calleeResolvedMethod->containingClass();
         thisClass = defClass;
         if (fe()->isInstanceOf(defClass, iface, true, true, true) != TR_yes)
            {
            calleeResolvedMethod = NULL; // hope to get a VFT test from profiling
            }
         // Heuristically choose between VFT test and method test. VFT test
         // is cheaper, but method test can potentially allow the inlined
         // body to be run for more receivers.
         else if (TR::Compiler->cls.isClassFinal(comp(), defClass))
            {
            testType = TR_VftTest; // method test will never help
            }
         else if (useVftTestHeuristics && !fe()->classHasBeenExtended(defClass))
            {
            // Hope that defClass won't be extended in the future, or if it is,
            // that its subtypes will override the inlined method anyway.
            testType = TR_VftTest;
            }
         else
            {
            // There's already at least one subclass inheriting the single
            // implementation. Choose method test because it covers the
            // defining class and (so far) all of its subclasses. (If there
            // were a subclass with its own override, then calleeResolvedMethod
            // would be overridden.)
            testType = TR_MethodTest;

            // Still consult the profiling though, since it might reveal that
            // one type is overwhelmingly frequent at this call site. In that
            // case, change back to VFT test.
            TR_ValueProfileInfoManager *profMgr =
               TR_ValueProfileInfoManager::get(comp());

            TR_AddressInfo *valueInfo = NULL;
            if (profMgr != NULL)
               {
               valueInfo = static_cast<TR_AddressInfo*>(
                  profMgr->getValueInfo(_bcInfo, comp(), AddressInfo));
               }

            if (useVftTestHeuristics
                && valueInfo != NULL
                && !comp()->getOption(TR_DisableProfiledInlining))
               {
               TR_ScratchList<TR_ExtraAddressInfo> byFreqDesc(comp()->trMemory());
               valueInfo->getSortedList(comp(), &byFreqDesc);
               ListIterator<TR_ExtraAddressInfo> it(&byFreqDesc);
               uint32_t remainingTotalFreq = valueInfo->getTotalFrequency();
               TR_OpaqueClassBlock *topProfiledClass = NULL;
               uint32_t topProfiledFreq = 0;
               TR_ExtraAddressInfo *cur = it.getFirst();
               for (; cur != NULL; cur = it.getNext())
                  {
                  auto *curClass =
                     reinterpret_cast<TR_OpaqueClassBlock*>(cur->_value);

                  if (persistInfo->isObsoleteClass(curClass, comp()->fe())
                      || fe()->isInstanceOf(curClass, iface, true, true, true) != TR_yes)
                     {
                     remainingTotalFreq -= cur->_frequency;
                     }
                  else if (topProfiledClass == NULL)
                     {
                     topProfiledClass = curClass;
                     topProfiledFreq = cur->_frequency;
                     }
                  }

               if (topProfiledClass != NULL
                   && remainingTotalFreq >= 32
                   && topProfiledFreq == remainingTotalFreq)
                  {
                  testType = TR_VftTest;
                  thisClass = topProfiledClass;
                  }
               }
            }
         }

      if (calleeResolvedMethod != NULL)
         {
         TR_ASSERT_FATAL(testType != TR_DummyTest, "failed to select a guard test type");
         TR_VirtualGuardSelection *guard =
            new (comp()->trHeapMemory()) TR_VirtualGuardSelection(
               kind, testType, thisClass);

         if (kind == TR_ProfiledGuard)
            {
            // Almost all bytecode would pass type checking even including
            // interface types. So even though this can't be a nop guard
            // (because verification doesn't check interface types), treat it
            // as much like a nop guard as possible. In particular, this will
            // ensure that the block containing the cold call is actually
            // marked cold, and ensure that priv. arg remat is allowed.
            guard->_forceTakenSideCold = true;

            // It is still a high-probability profiled guard...
            guard->setIsHighProbablityProfiledGuard();
            }

         addTarget(
            comp()->trMemory(),
            inliner,
            guard,
            calleeResolvedMethod,
            thisClass,
            heapAlloc);

         heuristicTrace(inliner->tracer(),"Call is an Interface with a Single Implementer guard %p\n", guard);
         return true;
         }
      }

   return findProfiledCallTargets(callStack, inliner);
   }

TR_OpaqueClassBlock* TR_J9InterfaceCallSite::getClassFromMethod ()
   {
   int32_t len = _interfaceMethod->classNameLength();
   char * s = TR::Compiler->cls.classNameToSignature(_interfaceMethod->classNameChars(), len, comp());
   return comp()->fej9()->getClassFromSignature(s, len, _callerResolvedMethod, true);
   }

TR_ResolvedMethod* TR_J9InterfaceCallSite::getResolvedMethod (TR_OpaqueClassBlock* klass)
   {
   return _callerResolvedMethod->getResolvedInterfaceMethod(comp(), klass , _cpIndex);
   }

void TR_J9InterfaceCallSite::findSingleProfiledMethod(ListIterator<TR_ExtraAddressInfo>& sortedValuesIt, TR_AddressInfo * valueInfo, TR_InlinerBase* inliner)
   {
   return;
   }

bool TR_J9MethodHandleCallSite::findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner)
   {
   heuristicTrace(inliner->tracer(),"Call is MethodHandle thunk call.");
   addTarget(comp()->trMemory(), inliner,
      new (comp()->trHeapMemory()) TR_VirtualGuardSelection(TR_NoGuard),
      _initialCalleeMethod,
      _receiverClass, heapAlloc);

   return true;
   }

bool TR_J9MutableCallSite::findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner)
   {
   if (!_mcsReferenceLocation)
      _mcsReferenceLocation = isMutableCallSiteTargetInvokeExact(this, inliner); // JSR292: Looking for calls through MutableCallSite
   if (_mcsReferenceLocation)
      {
      // TODO:JSR292: This belongs behind the FE interface
      heuristicTrace(inliner->tracer(),"Call is MutableCallSite.target.invokeExact call.");
      if (!comp()->performVirtualGuardNOPing())
         {
         heuristicTrace(inliner->tracer(),"  Virtual guard NOPing disabled");
         return false;
         }
      TR_VirtualGuardSelection *vgs = new (comp()->trHeapMemory()) TR_VirtualGuardSelection(TR_MutableCallSiteTargetGuard, TR_DummyTest);
      vgs->_mutableCallSiteObject = _mcsReferenceLocation;
      TR::KnownObjectTable *knot = comp()->getOrCreateKnownObjectTable();

#if defined(J9VM_OPT_JITSERVER)
      if (comp()->isOutOfProcessCompilation())
         {
         vgs->_mutableCallSiteEpoch = TR::KnownObjectTable::UNKNOWN;
         bool knotEnabled = (knot != NULL);
         auto stream = TR::CompilationInfo::getStream();
         stream->write(JITServer::MessageType::KnownObjectTable_mutableCallSiteEpoch, _mcsReferenceLocation, knotEnabled);

         auto recv = stream->read<uintptr_t, TR::KnownObjectTable::Index, uintptr_t*>();
         uintptr_t mcsObject = std::get<0>(recv);
         TR::KnownObjectTable::Index knotIndex = std::get<1>(recv);
         uintptr_t *objectPointerReference = std::get<2>(recv);

         if (mcsObject && knot && (knotIndex != TR::KnownObjectTable::UNKNOWN))
            {
            vgs->_mutableCallSiteEpoch = knotIndex;
            knot->updateKnownObjectTableAtServer(knotIndex, objectPointerReference);
            }
         else
            {
            vgs->_mutableCallSiteObject = NULL;
            }
         }
      else
#endif /* defined(J9VM_OPT_JITSERVER) */
         {
         TR::VMAccessCriticalSection mutableCallSiteEpoch(comp()->fej9());
         vgs->_mutableCallSiteEpoch = TR::KnownObjectTable::UNKNOWN;
         uintptr_t mcsObject = comp()->fej9()->getStaticReferenceFieldAtAddress((uintptr_t)_mcsReferenceLocation);
         if (mcsObject && knot)
            {
            TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fej9());
            vgs->_mutableCallSiteEpoch = fej9->mutableCallSiteEpoch(comp(), mcsObject);
            }
         else
            {
            vgs->_mutableCallSiteObject = NULL;
            }
         }

      if (vgs->_mutableCallSiteEpoch != TR::KnownObjectTable::UNKNOWN)
         {
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
         TR::MethodSymbol::Kinds methodKind = TR::MethodSymbol::Static;
         TR_OpaqueMethodBlock *targetJ9Method =
            comp()->fej9()->targetMethodFromMethodHandle(comp(), vgs->_mutableCallSiteEpoch);

         TR_ASSERT_FATAL(
            targetJ9Method != NULL,
            "failed to find MCS target (obj%d) LambdaForm method",
            (int)vgs->_mutableCallSiteEpoch);

         TR_ResolvedMethod *targetMethod = comp()->fej9()->createResolvedMethod(
            comp()->trMemory(), targetJ9Method, callStack->_method);

         heuristicTrace(
            inliner->tracer(),
            "Refine callee of MCS target invokeBasic to %s\n",
            targetMethod->signature(comp()->trMemory(), stackAlloc));
#else
         TR::MethodSymbol::Kinds methodKind = TR::MethodSymbol::ComputedVirtual;
         TR_ResolvedMethod *targetMethod = comp()->fej9()->createMethodHandleArchetypeSpecimen(
            comp()->trMemory(),
            knot->getPointerLocation(vgs->_mutableCallSiteEpoch),
            _callerResolvedMethod);
#endif
         TR_CallTarget *target = addTarget(comp()->trMemory(), inliner, vgs,
            targetMethod, _receiverClass, heapAlloc);
         TR_ASSERT(target , "There should be only one target for TR_MutableCallSite");
         target->_calleeMethodKind = methodKind;

         heuristicTrace(
            inliner->tracer(),
            "  addTarget: MutableCallSite %p epoch is obj%d",
            vgs->_mutableCallSiteObject,
            vgs->_mutableCallSiteEpoch);

         return true;
         }
      else if (vgs->_mutableCallSiteObject)
         {
         heuristicTrace(inliner->tracer(),"  MutableCallSite.epoch is currently NULL.  Can't devirtualize.");
         }
      else
         {
         heuristicTrace(inliner->tracer(),"  MutableCallSite is NULL!  That is rather unexpected.");
         }
      return false;
      }

   return false;
   }

bool TR_InlinerBase::tryToGenerateILForMethod (TR::ResolvedMethodSymbol* calleeSymbol, TR::ResolvedMethodSymbol* callerSymbol, TR_CallTarget* calltarget)
   {
   TR_J9InlinerPolicy *j9inlinerPolicy = (TR_J9InlinerPolicy *) getPolicy();
   return j9inlinerPolicy->_tryToGenerateILForMethod (calleeSymbol, callerSymbol, calltarget);
   }

void TR_InlinerBase::getBorderFrequencies(int32_t &hotBorderFrequency, int32_t &coldBorderFrequency, TR_ResolvedMethod * calleeResolvedMethod, TR::Node *callNode)
   {
   if (comp()->getMethodHotness() > warm)
      {
      hotBorderFrequency = comp()->isServerInlining() ? 2000 : 2500;
      coldBorderFrequency = 0;
      }
   else if (!comp()->getOption(TR_DisableConservativeInlining) &&
             calleeResolvedMethod->maxBytecodeIndex() >= comp()->getOptions()->getAlwaysWorthInliningThreshold() &&
            !alwaysWorthInlining(calleeResolvedMethod, callNode))
      {
      hotBorderFrequency = 6000;
      coldBorderFrequency = 1500;
      }
   else // old days
      {
      if (comp()->isServerInlining())
         {
         hotBorderFrequency = 2000;
         coldBorderFrequency = 50;
         }
      else
         {
         hotBorderFrequency = 2500;
         coldBorderFrequency = 1000;
         }
      }

   // Did the user specify specific values? If so, use those
   if (comp()->getOptions()->getInlinerBorderFrequency() >= 0)
      hotBorderFrequency = comp()->getOptions()->getInlinerBorderFrequency();
   //if (comp()->getOptions()->getInlinerColdBorderFrequency() >= 0)
   //   coldBorderFrequency = comp()->getOptions()->getInlinerColdBorderFrequency();
   if (comp()->getOptions()->getInlinerVeryColdBorderFrequency() >= 0)
      coldBorderFrequency = comp()->getOptions()->getInlinerVeryColdBorderFrequency();



   return;
   }


int TR_InlinerBase::checkInlineableWithoutInitialCalleeSymbol (TR_CallSite* callsite, TR::Compilation* comp)
   {
   if (!callsite->_isInterface)
      {
      return Unresolved_Callee;
      }

   return InlineableTarget;
   }


int32_t TR_InlinerBase::scaleSizeBasedOnBlockFrequency(int32_t bytecodeSize, int32_t frequency, int32_t borderFrequency, TR_ResolvedMethod * calleeResolvedMethod, TR::Node *callNode, int32_t coldBorderFrequency)
   {
    int32_t maxFrequency = MAX_BLOCK_COUNT + MAX_COLD_BLOCK_COUNT;
    if (frequency > borderFrequency)
        {
        float factor = (float)(maxFrequency - frequency) / (float)maxFrequency;
        factor = std::max(factor, 0.7f);


        bytecodeSize = (int32_t)((float)bytecodeSize * factor);
        if (bytecodeSize < 10) bytecodeSize = 10;
        }
    else if (frequency < coldBorderFrequency &&
        !alwaysWorthInlining(calleeResolvedMethod, callNode))
        {

        float factor = (float)frequency / (float)maxFrequency;
        bytecodeSize = (int32_t)((float)bytecodeSize / (factor*factor));
        }

   return bytecodeSize;

   }

float TR_MultipleCallTargetInliner::getScalingFactor(float factor)
   {
   return std::max(factor, 0.7f);
   }


void TR_ProfileableCallSite::findSingleProfiledReceiver(ListIterator<TR_ExtraAddressInfo>& sortedValuesIt, TR_AddressInfo * valueInfo, TR_InlinerBase* inliner)
   {

   bool firstInstanceOfCheckFailed = false;
   int32_t totalFrequency = valueInfo->getTotalFrequency();


   for (TR_ExtraAddressInfo *profiledInfo = sortedValuesIt.getFirst(); profiledInfo != NULL; profiledInfo = sortedValuesIt.getNext())
      {
      int32_t freq = profiledInfo->_frequency;
      TR_OpaqueClassBlock* tempreceiverClass = (TR_OpaqueClassBlock *) profiledInfo->_value;

      float val = (float)freq/(float)valueInfo->getTotalFrequency();        //x87 hardware rounds differently if you leave this division in compare


      bool preferMethodTest = false;

      bool isClassObsolete = comp()->getPersistentInfo()->isObsoleteClass((void*)tempreceiverClass, comp()->fe());

      if (!isClassObsolete)
         {
         int32_t len = 1;
         const char *className = TR::Compiler->cls.classNameChars(comp(), tempreceiverClass, len);

         if (!strncmp(className, "java/lang/ThreadLocal", 21) && !isInterface())
            {
            preferMethodTest = true;
            }
         // high opt level compiles during JIT STARTUP could be affected by classes being loaded - maximize the chances
         // of success by using method tests
         else if (comp()->getPersistentInfo()->getJitState() == STARTUP_STATE && comp()->getMethodHotness() >= hot)
            {
            preferMethodTest = true;
            }
         }


      static const char* userMinProfiledCallFreq = feGetEnv("TR_MinProfiledCallFrequency");
      static const float minProfiledCallFrequency = userMinProfiledCallFreq ? atof (userMinProfiledCallFreq) :
         comp()->getOption(TR_DisableMultiTargetInlining) ? MIN_PROFILED_CALL_FREQUENCY : .10f;

      if ((val >= minProfiledCallFrequency ||
               (firstInstanceOfCheckFailed && val >= SECOND_BEST_MIN_CALL_FREQUENCY)) &&
          !comp()->getPersistentInfo()->isObsoleteClass((void*)tempreceiverClass, comp()->fe()))
         {
         TR_OpaqueClassBlock* callSiteClass = _receiverClass ? _receiverClass : getClassFromMethod();

         if (callSiteClass && !isInterface() && TR::Compiler->cls.isInterfaceClass(comp(), callSiteClass) && isCallingObjectMethod() != TR_yes)
            {
            // TR_J9VirtualCallSite::findCallSiteTarget() should have refined the _receiverClass, but it must have failed, we need to abort this inlining
            if (comp()->trace(OMR::inlining))
               traceMsg(comp(), "inliner: callSiteClass [%p] is an interface making it impossible to confirm correct context of the profiled class [%p]\n", callSiteClass, tempreceiverClass);
            callSiteClass = 0;
            }

         bool profiledClassIsNotInstanceOfCallSiteClass = true;
         if (callSiteClass)
            {
            comp()->enterHeuristicRegion();
            profiledClassIsNotInstanceOfCallSiteClass = (fe()->isInstanceOf(tempreceiverClass, callSiteClass, true, true, true) != TR_yes);
            comp()->exitHeuristicRegion();
            }

         if (profiledClassIsNotInstanceOfCallSiteClass)
            {
            inliner->tracer()->insertCounter(Not_Sane,_callNodeTreeTop);
            firstInstanceOfCheckFailed = true;

            if (comp()->trace(OMR::inlining))
               traceMsg(comp(), "inliner: profiled class [%p] is not instanceof callSiteClass [%p]\n", tempreceiverClass, callSiteClass);

            continue;
            }

         comp()->enterHeuristicRegion();
         TR_ResolvedMethod* targetMethod = getResolvedMethod (tempreceiverClass);
         comp()->exitHeuristicRegion();

         if (!targetMethod)
            {
            continue;
            }

         if (preferMethodTest && isInterface())
            {
            // For interface call sites, the profiled guard must allow only
            // receivers that are instances of the expected interface. See the
            // comment in TR_J9InterfaceCallSite::findCallSiteTarget().
            TR_OpaqueClassBlock *defClass = targetMethod->containingClass();
            TR_OpaqueClassBlock *iface = getClassFromMethod();
            if (fe()->isInstanceOf(defClass, iface, true, true, true) != TR_yes)
               preferMethodTest = false;
            }

         TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());
         // need to be able to store class chains for these methods
         if (comp()->compileRelocatableCode())
            {
            if (tempreceiverClass && comp()->getOption(TR_UseSymbolValidationManager))
               {
               if (!comp()->getSymbolValidationManager()->addProfiledClassRecord(tempreceiverClass))
                  continue;
               /* call getResolvedMethod again to generate the validation records */
               TR_ResolvedMethod* target_method = getResolvedMethod (tempreceiverClass);

               /* it is possible for getResolvedMethod to return NULL, since there might be
                * a problem when generating validation records
                */
               if (!target_method)
                  continue;

               TR_OpaqueClassBlock *classOfMethod = target_method->classOfMethod();
               SVM_ASSERT_ALREADY_VALIDATED(comp()->getSymbolValidationManager(), classOfMethod);
               }

            if (!fej9->canRememberClass(tempreceiverClass) ||
                !fej9->canRememberClass(callSiteClass))
               {
               if (comp()->trace(OMR::inlining))
                  traceMsg(comp(), "inliner: profiled class [%p] or callSiteClass [%p] cannot be rememberd in shared cache\n", tempreceiverClass, callSiteClass);
               continue;
               }
            }

         TR_VirtualGuardSelection *guard = NULL;
         if (preferMethodTest)
            guard = new (comp()->trHeapMemory()) TR_VirtualGuardSelection(TR_ProfiledGuard, TR_MethodTest, tempreceiverClass);
         else
            guard = new (comp()->trHeapMemory()) TR_VirtualGuardSelection(TR_ProfiledGuard, TR_VftTest, tempreceiverClass);

         // if the previous value was from the interpreter profiler
         // don't apply the optimization
         TR_ByteCodeInfo &bcInfo = _bcInfo;  //callNode->getByteCodeInfo();
         if (valueInfo->getTopProbability() == 1.0f && valueInfo->getProfiler()->getSource() < LastProfiler)
            guard->setIsHighProbablityProfiledGuard();

         heuristicTrace(inliner->tracer(),"Creating a profiled call. callee Symbol %p frequencyadjustment %f",_initialCalleeSymbol, val);
         addTarget(comp()->trMemory(),inliner,guard,targetMethod,tempreceiverClass,heapAlloc,val);

         if (comp()->getOption(TR_DisableMultiTargetInlining))
            return;
         }
      else  // if we're below the above threshold, lets stop considering call targets
         {
         if (comp()->trace(OMR::inlining))
            traceMsg(comp(), "bailing, below inlining threshold\n");
         break;
         }

      }

   }


void TR_ProfileableCallSite::findSingleProfiledMethod(ListIterator<TR_ExtraAddressInfo>& sortedValuesIt, TR_AddressInfo * valueInfo, TR_InlinerBase* inliner)
   {
   if (!comp()->cg()->getSupportsProfiledInlining())
      {
      return;
      }

   uint32_t totalFrequency = valueInfo->getTotalFrequency();

   if (totalFrequency<=0)
      {
      return;
      }
   TR_OpaqueClassBlock* callSiteClass = _receiverClass ? _receiverClass : getClassFromMethod();
   TR_ASSERT_FATAL(!isInterface(), "Interface call site called TR_ProfileableCallSite::findSingleProfiledMethod()");
   if (!callSiteClass || (TR::Compiler->cls.isInterfaceClass(comp(), callSiteClass) && isCallingObjectMethod() != TR_yes))
      {
      // TR_J9VirtualCallSite::findCallSiteTarget() should refine the _receiverClass, but if it failed, we need to abort this inlining
      if (callSiteClass && comp()->trace(OMR::inlining))
         traceMsg(comp(), "callSiteClass [%p] is an interface making it impossible to confirm correct context for any profiled class\n", callSiteClass);
      return;
      }

   // first let's do sanity test on all profiled targets
   if (comp()->trace(OMR::inlining))
      traceMsg(comp(), "No decisive class profiling info for the virtual method, we'll try to see if more than one class uses the same method implementation.\n");

   bool classValuesAreSane = true;
   for (TR_ExtraAddressInfo *profiledInfo = sortedValuesIt.getFirst(); profiledInfo != NULL; profiledInfo = sortedValuesIt.getNext())
      {
      TR_OpaqueClassBlock *clazz = (TR_OpaqueClassBlock *) profiledInfo->_value;
      bool isClassObsolete = comp()->getPersistentInfo()->isObsoleteClass((void*)clazz, comp()->fe());
      if (isClassObsolete)
         {
         classValuesAreSane = false;
         break;
         }

      TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());
      // need to be able to store class chains for these methods
      if (comp()->compileRelocatableCode())
         {
         if (clazz && comp()->getOption(TR_UseSymbolValidationManager))
            if (!comp()->getSymbolValidationManager()->addProfiledClassRecord(clazz))
               {
               classValuesAreSane = false;
               break;
               }

         if (!fej9->canRememberClass(clazz) ||
             !fej9->canRememberClass(callSiteClass))
            {
            classValuesAreSane = false;
            break;
            }
         }
      }

   if (!classValuesAreSane)
      return;

   if (comp()->trace(OMR::inlining))
      traceMsg(comp(), "OK, all classes check out, we'll try to get their method implementations.\n");

   TR_ScratchList<TR_AddressInfo::ProfiledMethod> methodsList(comp()->trMemory());
   // this API doesn't do a sort
   valueInfo->getMethodsList(comp(), _callerResolvedMethod, callSiteClass, _vftSlot, &methodsList);

   int numMethods = methodsList.getSize();

   if (comp()->trace(OMR::inlining))
      traceMsg(comp(), "OK, all classes check out, we'll try to get their method implementations (%d).\n", numMethods);


   ListIterator<TR_AddressInfo::ProfiledMethod> methodValuesIt(&methodsList);
   TR_AddressInfo::ProfiledMethod *profiledMethodInfo;
   TR_AddressInfo::ProfiledMethod *bestMethodInfo = methodValuesIt.getFirst();

   float methodProbability = .0f;

   if (bestMethodInfo)
      {
         for (profiledMethodInfo = methodValuesIt.getNext(); profiledMethodInfo != NULL; profiledMethodInfo = methodValuesIt.getNext())
            {
            if (profiledMethodInfo->_frequency > bestMethodInfo->_frequency)
               bestMethodInfo = profiledMethodInfo;
            }

         methodProbability = (float)bestMethodInfo->_frequency/(float)totalFrequency;

         if (comp()->trace(OMR::inlining))
            {
            TR_ResolvedMethod *targetMethod = (TR_ResolvedMethod *)bestMethodInfo->_value;
            traceMsg(comp(), "Found a target method %s with probability of %f%%.\n",
                  targetMethod->signature(comp()->trMemory()), methodProbability * 100.0);
            }

            static const char* userMinProfiledCallFreq = feGetEnv("TR_MinProfiledCallFrequency");
            static const float minProfiledCallFrequency = userMinProfiledCallFreq ? atof (userMinProfiledCallFreq) : MIN_PROFILED_CALL_FREQUENCY;

            if (methodProbability >= minProfiledCallFrequency)
            {
            TR_ResolvedMethod *targetMethod = (TR_ResolvedMethod *)bestMethodInfo->_value;
            TR_OpaqueClassBlock *targetClass = targetMethod->classOfMethod();

            if (targetMethod && targetClass)
               {
               TR_VirtualGuardSelection *guard = new (comp()->trHeapMemory()) TR_VirtualGuardSelection(TR_ProfiledGuard, TR_MethodTest, targetClass);
               addTarget(comp()->trMemory(), inliner, guard, targetMethod, targetClass, heapAlloc, methodProbability);
               if (comp()->trace(OMR::inlining))
                  {
                  TR_ResolvedMethod *targetMethod = (TR_ResolvedMethod *)bestMethodInfo->_value;
                  traceMsg(comp(), "Added target method %s with probability of %f%%.\n",
                        targetMethod->signature(comp()->trMemory()), methodProbability * 100.0);
                  char* sig = TR::Compiler->cls.classSignature(comp(), targetClass, comp()->trMemory());
                  traceMsg(comp(), "target class %s\n", sig);
                  }
               return;
               }
            }
      }
   else if (comp()->trace(OMR::inlining))
      traceMsg(comp(), "Failed to find any methods compatible with callsite class %p signature %s\n", callSiteClass, TR::Compiler->cls.classSignature(comp(), callSiteClass, comp()->trMemory()));
   }

bool TR_ProfileableCallSite::findProfiledCallTargets (TR_CallStack *callStack, TR_InlinerBase* inliner)
   {
   heuristicTrace(inliner->tracer(),"Looking for a profiled Target %p \n", this);
   TR_ValueProfileInfoManager * profileManager = TR_ValueProfileInfoManager::get(comp());

   if (!profileManager)
      {
      heuristicTrace(inliner->tracer()," no profileManager %p\n", this);
      return false;
      }

   TR_AddressInfo *valueInfo = static_cast<TR_AddressInfo*>(profileManager->getValueInfo(_bcInfo, comp(), AddressInfo));

   if(!valueInfo || comp()->getOption(TR_DisableProfiledInlining))
      {
      heuristicTrace(inliner->tracer()," no valueInfo or valueInfo is not of AddressInfo type or TR_DisableProfiledInlining specified for %p\n", this);
      return false;
      }



   TR_ScratchList<TR_ExtraAddressInfo> valuesSortedByFrequency(comp()->trMemory());
   valueInfo->getSortedList(comp(), &valuesSortedByFrequency);
   ListIterator<TR_ExtraAddressInfo> sortedValuesIt(&valuesSortedByFrequency);

   uint32_t totalFrequency = valueInfo->getTotalFrequency();
   ((TR_J9InlinerTracer *)inliner->tracer())->dumpProfiledClasses(sortedValuesIt, totalFrequency);

   //@TODO: put in a separate function
   if (inliner->isEDODisableInlinedProfilingInfo() && _callerResolvedMethod != comp()->getCurrentMethod())
      {
      // if the previous value was from the interpreter profiler
      // don't devirtualize
      if (valueInfo->getProfiler()->getSource() == LastProfiler)
         {
         inliner->tracer()->insertCounter(EDO_Callee,_callNodeTreeTop);
         heuristicTrace(inliner->tracer()," EDO callsite %p, so not inlineable\n", this);
         return false;
         }
      }

   findSingleProfiledReceiver(sortedValuesIt, valueInfo, inliner);
   if (!numTargets())
      {
      findSingleProfiledMethod(sortedValuesIt, valueInfo, inliner);
      }

   return numTargets();
   }


