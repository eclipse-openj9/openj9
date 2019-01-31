/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include "optimizer/InterProceduralAnalyzer.hpp"

#include <stdint.h>
#include <string.h>
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CHTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/PersistentInfo.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "env/ClassTableCriticalSection.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/Stack.hpp"
#include "ras/Debug.hpp"
#include "runtime/RuntimeAssumptions.hpp"

#define MAX_SNIFF_BYTECODE_SIZE             1000
#define MAX_SNIFF_DEPTH 10
#define MAX_SUB_METHODS 5

class TR_OpaqueClassBlock;
class TR_OpaqueMethodBlock;

TR::InterProceduralAnalyzer::InterProceduralAnalyzer(TR::Compilation * c, bool trace)
   :
   _compilation(c),
   _trMemory(c->trMemory()),
   _maxSniffDepth(MAX_SNIFF_DEPTH),
   _sniffDepth(0),
   _maxSniffDepthExceeded(false),
   _totalPeekedBytecodeSize(0),
   _maxPeekedBytecodeSize(c->getMaxPeekedBytecodeSize()),
   _trace(trace),
   _fe(c->fe()),
   _successfullyPeekedMethods(trMemory()),
   _unsuccessfullyPeekedMethods(trMemory()),
   _classesThatShouldNotBeLoadedInCurrentPeek(trMemory()),
   _classesThatShouldNotBeNewlyExtendedInCurrentPeek(trMemory()),
   _globalsWrittenInCurrentPeek(trMemory())
   {
   //TR_ScratchList<TR_ClassExtendCheck> _classesThatShouldNotBeNewlyExtendedInCurrentPeekHT[1+ CLASSHASHTABLE_SIZE];
   _classesThatShouldNotBeNewlyExtendedInCurrentPeekHT =
      (TR_ScratchList<TR_ClassExtendCheck> *)
         trMemory()->allocateHeapMemory(sizeof(TR_ScratchList<TR_ClassExtendCheck>) * (CLASSHASHTABLE_SIZE + 1));

   memset(_classesThatShouldNotBeNewlyExtendedInCurrentPeekHT, 0, sizeof(TR_ScratchList<TR_ClassExtendCheck>) * (CLASSHASHTABLE_SIZE + 1));
   for (int32_t i = 0; i < CLASSHASHTABLE_SIZE + 1; ++i)
      _classesThatShouldNotBeNewlyExtendedInCurrentPeekHT[i].setRegion(trMemory()->currentStackRegion());

   _classesThatShouldNotBeNewlyExtendedHT =
      (TR_LinkHead<TR_ClassExtendCheck> *)
         trMemory()->allocateHeapMemory(sizeof(TR_LinkHead<TR_ClassExtendCheck>) * (CLASSHASHTABLE_SIZE + 1));
   memset(_classesThatShouldNotBeNewlyExtendedHT, 0, sizeof(TR_LinkHead<TR_ClassExtendCheck>) * (CLASSHASHTABLE_SIZE + 1));

   }


int32_t
TR::InterProceduralAnalyzer::perform()
   {
   return 2;
   }



bool TR::InterProceduralAnalyzer::capableOfPeekingVirtualCalls()
   {
   if (comp()->performVirtualGuardNOPing() &&
       !comp()->getOption(TR_DisableIPA))
      return true;
   return false;
   }



List<OMR::RuntimeAssumption> *TR::InterProceduralAnalyzer::analyzeCall(TR::Node *callNode)
   {
   if (comp()->isProfilingCompilation() ||
       !capableOfPeekingVirtualCalls())
      return 0;

   comp()->incVisitCount();
   bool success = true;
   _maxSniffDepthExceeded = false;
   _sniffDepth = 0;
   _prevCec = NULL;
   _prevClc = NULL;
   _classesThatShouldNotBeLoaded.setFirst(NULL);
   _classesThatShouldNotBeNewlyExtended.setFirst(NULL);
   int32_t i;

   for (i=0;i<CLASSHASHTABLE_SIZE;i++)
      _classesThatShouldNotBeNewlyExtendedHT[i].setFirst(NULL);

   _globalsWritten.setFirst(NULL);

   List<OMR::RuntimeAssumption> *runtimeAssumptions = analyzeCallGraph(callNode, &success);
   if (trace())
      {
      if (success)
         {
         traceMsg(comp(), "Ended peek which was successful\n");
         traceMsg(comp(), "Number of unloaded classes are %d\n", _classesThatShouldNotBeLoaded.getSize());
         traceMsg(comp(), "Number of classes that should not be newly extended are %d\n", _classesThatShouldNotBeNewlyExtended.getSize());
         }
      else
         traceMsg(comp(), "Ended peek which was unsuccessful\n");
      }

   ListElement<TR_ClassExtendCheck> *currCec = _classesThatShouldNotBeNewlyExtendedInCurrentPeek.getListHead();
   while (currCec)
      {
      TR_PersistentClassInfo * cl = comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(currCec->getData()->_clazz, comp());
      cl->resetShouldNotBeNewlyExtended(comp()->getCompThreadID());
      currCec = currCec->getNextElement();
      }
   _classesThatShouldNotBeLoadedInCurrentPeek.deleteAll();
   _classesThatShouldNotBeNewlyExtendedInCurrentPeek.deleteAll();
   for (i=0;i<CLASSHASHTABLE_SIZE;i++)
      _classesThatShouldNotBeNewlyExtendedInCurrentPeekHT[i].deleteAll();
   _globalsWrittenInCurrentPeek.deleteAll();

   if (success)
      return new (trStackMemory()) TR_ScratchList<OMR::RuntimeAssumption>(trMemory());
   else
      return NULL;

   //return runtimeAssumptions;
   }

List<OMR::RuntimeAssumption> *TR::InterProceduralAnalyzer::analyzeCallGraph(TR::Node *callNode, bool *success)
   {
   if (_sniffDepth >= _maxSniffDepth)
      {
      _maxSniffDepthExceeded = true;
      *success = false;

      if (trace())
         {
         traceMsg(comp(), "High sniff depth made peek unsuccessful\n");
         }

      return 0;
      }

   TR::SymbolReference *symRef        = callNode->getSymbolReference();
   TR::MethodSymbol    *methodSymbol  = symRef->getSymbol()->castToMethodSymbol();
   TR::ResolvedMethodSymbol * resolvedMethodSymbol = methodSymbol->getResolvedMethodSymbol();
   TR_ResolvedMethod *owningMethod = symRef->getOwningMethod(comp());

   if (!resolvedMethodSymbol &&
       !methodSymbol->isInterface())
      {
      *success = false;
      if (trace())
         {
         traceMsg(comp(), "Unresolved non-interface call node %p made peek unsuccessful\n", callNode);
         }

      return 0;
      }

   if ((*success) &&
       callNode->getOpCode().isIndirect() &&
       !capableOfPeekingVirtualCalls())
      {
      *success = false;
      return 0;
      }

   TR_OpaqueClassBlock *clazz = NULL;
   if (!resolvedMethodSymbol)  // as per current logic, it is an interface method
      {
      int32_t cpIndex = symRef->getCPIndex();
      TR_Method * originalMethod = methodSymbol->getMethod();
      int32_t len = originalMethod->classNameLength();
      char *s = classNameToSignature(originalMethod->classNameChars(), len, comp());
      clazz = fe()->getClassFromSignature(s, len, owningMethod);
      if (!clazz)
         {
         // Add assumption here
         //
         if (!s)
            {
            *success = false;
            if (trace())
               {
               traceMsg(comp(), "Found unresolved method call node %p while peeking whose class is unresolved and unable to add assumption -- peek unsuccessful\n", callNode);
               }
            }
         else
            {
            addClassThatShouldNotBeLoaded(s, len);

            if (trace())
               {
               traceMsg(comp(), "Found unresolved method call node %p while peeking -- add assumption\n", callNode);
               }
            }
         return 0;
         }
      }
   else
      {
      TR_ResolvedMethod *method = resolvedMethodSymbol->getResolvedMethod();
      if (!method)
         {
         *success = false;
         return 0;
         }

      analyzeMethod(callNode, method, success);
      clazz = method->containingClass();
      }


   if ((*success) &&
       callNode->getOpCode().isIndirect())
      {
      TR::Node *thisChild = callNode->getChild(callNode->getFirstArgumentIndex());
      int32_t len;
      const char *s = thisChild->getTypeSignature(len);
      if (!s)
         {
         if (thisChild->getOpCodeValue() == TR::New)
            s = thisChild->getFirstChild()->getTypeSignature(len);
         }

      //traceMsg(comp(), "callNode %p thisChild %p\n", callNode, thisChild);
      //if (s)
      //   traceMsg(comp(), "sig %s\n", s);
      //else
      //        traceMsg(comp(), "sig is NULL\n");

      if (s)
         {
         TR_OpaqueClassBlock *thisClazz = fe()->getClassFromSignature(s, len, owningMethod);
         if (thisClazz && (clazz != thisClazz))
            {
            TR_YesNoMaybe isInstance = fe()->isInstanceOf(thisClazz, clazz, true);
            if (isInstance == TR_yes)
               clazz = thisClazz;
            }
         }

      if (clazz)
         {
         if (!addClassThatShouldNotBeNewlyExtended(clazz))
            {
            if(trace())
               traceMsg(comp(), "Could not add Class That should not be newly extended to assumptions list.\n");

            *success = false;
            return 0;
            }

         if (trace())
            {
            traceMsg(comp(), "Found class for this object -- add assumption that the class should not be newly extended\n");
            }
         }

      bool allowForAOT = comp()->getOption(TR_UseSymbolValidationManager);
      TR_PersistentClassInfo *classInfo = comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(clazz, comp(), allowForAOT);
      if (classInfo)
         {
         TR_ScratchList<TR_PersistentClassInfo> subClasses(trMemory());
	 TR_ClassQueries::getSubClasses(classInfo, subClasses, fe());
         if (trace())
            traceMsg(comp(), "Number of subclasses = %d\n", subClasses.getSize());
         TR_ScratchList<TR_ResolvedMethod> subMethods(trMemory());
         int32_t numSubMethods = 0;
         ListIterator<TR_PersistentClassInfo> subClassesIt(&subClasses);
         for (TR_PersistentClassInfo *subClassInfo = subClassesIt.getFirst(); subClassInfo; subClassInfo = subClassesIt.getNext())
            {
            TR_OpaqueClassBlock *subClass = (TR_OpaqueClassBlock *) subClassInfo->getClassId();
            if (TR::Compiler->cls.isInterfaceClass(comp(), subClass))
               continue;
            TR_ResolvedMethod *subClassMethod;
            if (methodSymbol->isInterface())
               subClassMethod = owningMethod->getResolvedInterfaceMethod(comp(), subClass, symRef->getCPIndex());
            else
               subClassMethod = owningMethod->getResolvedVirtualMethod(comp(), subClass, symRef->getOffset());
            int32_t length;
            if (trace())
               traceMsg(comp(), "Class name %s\n", TR::Compiler->cls.classNameChars(comp(), subClass, length));
            if (subClassMethod && !subMethods.find(subClassMethod))
               {
               subMethods.add(subClassMethod);
               numSubMethods++;
               analyzeMethod(callNode, subClassMethod, success);
               }

            if (numSubMethods > MAX_SUB_METHODS)
               *success = false;

            if (!(*success))
               break;
            }
         }
      return 0;
      }

   return 0;
   }


List<OMR::RuntimeAssumption> *TR::InterProceduralAnalyzer::analyzeMethod(TR::Node *callNode, TR_ResolvedMethod *method, bool *success)
   {
   if (trace())
      {
      traceMsg(comp(), "Consider method %s for peek\n", method->signature(trMemory()));
      }

   if (!method->isCompilable(trMemory()) || method->isJNINative())
      {
      *success = false;
      return 0;
      }

   bool successfulPriorPeek = true;
   TR::PriorPeekInfo *priorPeek = NULL;
   if (0 && alreadyPeekedMethod(method, &successfulPriorPeek, &priorPeek))
      {
      if (!successfulPriorPeek)
         {
         *success = false;
         if (trace())
            {
            traceMsg(comp(), "Prior peek failure for method %s caused current peek to be unsuccessful\n", method->signature(trMemory()));
            }
         }
      else
         {
         if (trace())
            {
            traceMsg(comp(), "Prior peek success for method %s caused current peek to be successful -- prior assumptions added\n", method->signature(trMemory()));
            }

         for (TR_ClassLoadCheck * clc = priorPeek->_classesThatShouldNotBeLoaded.getFirst(); clc; clc = clc->getNext())
            addClassThatShouldNotBeLoaded(clc->_name, clc->_length);

         for (TR_ClassExtendCheck * cec = priorPeek->_classesThatShouldNotBeNewlyExtended.getFirst(); cec; cec = cec->getNext())
            addClassThatShouldNotBeNewlyExtended(cec->_clazz);
         }
      return 0;
      }

    uint32_t bytecodeSize = method->maxBytecodeIndex();
    if (bytecodeSize > MAX_SNIFF_BYTECODE_SIZE)
      {
      *success = false;
      if (trace())
         {
         traceMsg(comp(), "Large bytecode size %d made peek unsuccessful\n", bytecodeSize);
         }

      return 0;
      }

   if (isOnPeekingStack(method))
      return 0;

   if (trace())
      traceMsg(comp(), "\nDepth %d sniffing into call at [%p] to %s\n", _sniffDepth, callNode, method->signature(trMemory()));

   TR::SymbolReference * symRef = callNode->getSymbolReference();
   int32_t offset = symRef->getOffset();
   TR::SymbolReference * newSymRef = NULL;

    if (method->isStatic())
       newSymRef = comp()->getSymRefTab()->findOrCreateMethodSymbol(symRef->getOwningMethodIndex(), -1, method, TR::MethodSymbol::Static);
    else
       newSymRef = comp()->getSymRefTab()->findOrCreateMethodSymbol(symRef->getOwningMethodIndex(), -1, method, TR::MethodSymbol::Interface);

      newSymRef->copyAliasSets(symRef, comp()->getSymRefTab());
      newSymRef->setOffset(offset);
   TR::MethodSymbol *methodSymbol = newSymRef->getSymbol()->castToMethodSymbol();
   TR::ResolvedMethodSymbol * resolvedMethodSymbol = methodSymbol->getResolvedMethodSymbol();

   vcount_t visitCount = comp()->getVisitCount();
   if (!resolvedMethodSymbol->getFirstTreeTop())
      {
      int32_t firstArgIndex = callNode->getFirstArgumentIndex();
      int32_t numRealArgs = callNode->getNumChildren() - firstArgIndex;
      const char **argInfo = (const char **)trMemory()->allocateHeapMemory(numRealArgs*sizeof(char *)); // can stack allocate in some cases
      memset(argInfo, 0, numRealArgs*sizeof(char *));
      int32_t *lenInfo = (int32_t *) trMemory()->allocateHeapMemory(numRealArgs*sizeof(int32_t)); // can stack allocate in some cases
      memset(lenInfo, 0xFF, numRealArgs*sizeof(int32_t));

      for (int32_t c = callNode->getNumChildren() - 1 ; c >= firstArgIndex; --c)
         {
         TR::Node *argument = callNode->getChild(c);
         if (argument->getDataType() == TR::Address)
            {
            int32_t len;
            const char *s = argument->getTypeSignature(len);
            if (!s)
               {
               if (argument->getOpCodeValue() == TR::New)
                  s = argument->getFirstChild()->getTypeSignature(len);
               }

            if (trace())
               {
               traceMsg(comp(), "callNode %p arg %p\n", callNode, argument);
               if (s)
                  traceMsg(comp(), "sig %s\n", s);
               else
                  traceMsg(comp(), "sig is NULL\n");
               }

            if (s && (c == firstArgIndex))
               {
               TR_OpaqueClassBlock *argClazz = fe()->getClassFromSignature(s, len, symRef->getOwningMethod(comp()));
               TR_OpaqueClassBlock *thisClazz = method->containingClass();

               if (!argClazz || !thisClazz)
                  {
                  *success = false;
                  if (trace())
                     {
                     traceMsg(comp(), "The call argument class is NULL, bailing out. (probably because of different class loaders)\n");
                     }

                  return 0;
                  }

               if (argClazz != thisClazz)
                  {
                  TR_YesNoMaybe isInstance = fe()->isInstanceOf(thisClazz, argClazz, true);
                  if (isInstance == TR_yes)
                     {
                     s = TR::Compiler->cls.classSignature_DEPRECATED(comp(), thisClazz, len, trMemory());
                     }
                  }
               }

            argInfo[c-firstArgIndex] = s;
            lenInfo[c-firstArgIndex] = len;
            }
         }

      _totalPeekedBytecodeSize =  _totalPeekedBytecodeSize + bytecodeSize;
      if (_totalPeekedBytecodeSize > _maxPeekedBytecodeSize)
          {
          *success = false;
          if (trace())
             {
             traceMsg(comp(), "Large bytecode size %d made peek unsuccessful\n", bytecodeSize);
             }

         return 0;
         }

      if (!performTransformation(comp(), "O^O INTERPROCEDURAL ANALYZER: Peeking into the IL for doing a limited form of interprocedural analysis  \n"))
          return 0;

      TR_PeekingArgInfo *peekInfo = (TR_PeekingArgInfo *) trMemory()->allocateStackMemory(sizeof(TR_PeekingArgInfo)); // can stack allocate in some cases
      peekInfo->_args = argInfo;
      peekInfo->_lengths = lenInfo;
      peekInfo->_method = resolvedMethodSymbol->getResolvedMethod();
      comp()->addPeekingArgInfo(peekInfo);
      //comp()->setVisitCount(1);

      _currentPeekingSymRefTab = resolvedMethodSymbol->getResolvedMethod()->genMethodILForPeeking(resolvedMethodSymbol, comp());
      //comp()->setVisitCount(visitCount);
      comp()->removePeekingArgInfo();

      if (!_currentPeekingSymRefTab)
         {
         *success = false;
         if (trace())
            traceMsg(comp(), "   (IL generation failed)\n");
         return 0;
         }

      if (trace())
         {
	 //comp()->setVisitCount(1);
         for (TR::TreeTop *tt = resolvedMethodSymbol->getFirstTreeTop(); tt; tt = tt->getNextTreeTop())
            comp()->getDebug()->print(comp()->getOutFile(), tt);
         //comp()->setVisitCount(visitCount);
         }
      }
   else
      {
      if (trace())
         traceMsg(comp(), "   (trees already dumped)\n");
      }

   ++_sniffDepth;


   ListElement<TR_ClassLoadCheck> *prevClc = NULL; //_classesThatShouldNotBeLoadedInCurrentPeek.getListHead();
   ListElement<TR_ClassExtendCheck> *prevCec = NULL; //_classesThatShouldNotBeNewlyExtendedInCurrentPeek.getListHead();
   ListElement<TR::GlobalSymbol> *prevSymRef = NULL; //_globalsWrittenInCurrentPeek.getListHead();
   _prevClc = prevClc;
   _prevCec = prevCec;
   _prevSymRef = prevSymRef;


   TR::TreeTop *treeTop = NULL;
   TR::Node *node = NULL;
   TR::Block *block = NULL;
   for (treeTop = resolvedMethodSymbol->getFirstTreeTop(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      node = treeTop->getNode();

      if (node->getOpCodeValue() == TR::BBStart)
         {
         block = node->getBlock();
         }

      if (node->getOpCode().isCheck() || node->getOpCodeValue() == TR::treetop)
         node = node->getFirstChild();

      if (node->getOpCode().isCall() && node->getVisitCount() != visitCount)
         {
         _prevClc = prevClc;
         _prevCec = prevCec;
         _prevSymRef = prevSymRef;

         //node->setVisitCount(visitCount);
         analyzeCallGraph(node, success);
         }

      if (!*success)
         {
         if (trace())
            {
            traceMsg(comp(), "Node %p made peek unsuccessful\n", node);
            }
         break;
         }

      if (analyzeNode(node, visitCount, success))
         treeTop = block->getExit();

      if (!*success)
         {
         if (trace())
            {
            traceMsg(comp(), "Node %p made peek unsuccessful\n", node);
            }
         break;
         }
      }

   --_sniffDepth;

   if (0 && *success)
      {
      TR::PriorPeekInfo *priorPeek = (TR::PriorPeekInfo *) trMemory()->allocateHeapMemory(sizeof(TR::PriorPeekInfo));
      priorPeek->_method = resolvedMethodSymbol->getResolvedMethod();
      priorPeek->_classesThatShouldNotBeLoaded.setFirst(NULL);
      priorPeek->_classesThatShouldNotBeNewlyExtended.setFirst(NULL);
      priorPeek->_globalsWritten.setFirst(NULL);
      ListElement<TR_ClassLoadCheck> *currClc = _classesThatShouldNotBeLoadedInCurrentPeek.getListHead();
      ListElement<TR_ClassExtendCheck> *currCec = _classesThatShouldNotBeNewlyExtendedInCurrentPeek.getListHead();
      ListElement<TR::GlobalSymbol> *currSymRef = _globalsWrittenInCurrentPeek.getListHead();
      while (currClc != prevClc)
         {
         priorPeek->_classesThatShouldNotBeLoaded.add(new (trHeapMemory()) TR_ClassLoadCheck(currClc->getData()->_name, currClc->getData()->_length));
         currClc = currClc->getNextElement();
         }

      while (currCec != prevCec)
         {
         priorPeek->_classesThatShouldNotBeNewlyExtended.add(new (trHeapMemory()) TR_ClassExtendCheck(currCec->getData()->_clazz));
         currCec = currCec->getNextElement();
         }

      while (currSymRef != prevSymRef)
         {
         priorPeek->_globalsWritten.add(new (trHeapMemory()) TR::GlobalSymbol(currSymRef->getData()->_symRef));
         currSymRef = currSymRef->getNextElement();
         }

      _successfullyPeekedMethods.add(priorPeek);
      if (trace())
         {
         traceMsg(comp(), "Method %s is successfully peeked\n", resolvedMethodSymbol->getResolvedMethod()->signature(trMemory()));
         }
      }
   else
      {
      if (_sniffDepth == 0)
         {
         if (trace())
            {
            traceMsg(comp(), "1Method %s is unsuccessfully peeked\n", resolvedMethodSymbol->getResolvedMethod()->signature(trMemory()));
            }

         _unsuccessfullyPeekedMethods.add(resolvedMethodSymbol->getResolvedMethod());
         _maxSniffDepthExceeded = false;
         }
      else
         {
         if (!_maxSniffDepthExceeded)
            {
            if (trace())
               {
               traceMsg(comp(), "2Method %s is unsuccessfully peeked\n", resolvedMethodSymbol->getResolvedMethod()->signature(trMemory()));
               }
            _unsuccessfullyPeekedMethods.add(resolvedMethodSymbol->getResolvedMethod());
            }
         }
      }


   return 0;
   }


bool TR::InterProceduralAnalyzer::isOnPeekingStack(TR_ResolvedMethod *method)
   {
   TR_Stack<TR_PeekingArgInfo *> *peekingStackInfo = comp()->getPeekingArgInfo();
   int32_t topOfStack = peekingStackInfo->topIndex();
   int32_t i;
   for (i=0;i<=topOfStack;i++)
      {
      TR_PeekingArgInfo *peekInfo = peekingStackInfo->element(i);
      if (peekInfo)
         {
         if (peekInfo->_method->isSameMethod(method))
            return true;
         /*
           if ((nameLength(peekInfo->_method) == nameLength(method)) &&
           !strncmp(nameChars(peekInfo->_method), nameChars(method), nameLength(method)) &&
           (signatureLength(peekInfo->_method) == signatureLength(method)) &&
           !strncmp(signatureChars(peekInfo->_method), signatureChars(method), signatureLength(method)))
           {
           TR_OpaqueClassBlock *peekedClazz = peekInfo->_method->containingClass();
           TR_OpaqueClassBlock *clazz = method->containingClass();
           TR_PersistentClassInfo *classInfo = comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(clazz, comp());
           if (classInfo)
           {
           TR_ScratchList<TR_PersistentClassInfo> subClasses;
           classInfo->getSubClasses(subClasses, comp());
           if (trace())
           traceMsg(comp(), "Number of subclasses = %d\n", subClasses.getSize());
           ListIterator<TR_PersistentClassInfo> subClassesIt(&subClasses);
           for (TR_PersistentClassInfo *subClassInfo = subClassesIt.getFirst(); subClassInfo; subClassInfo = subClassesIt.getNext())
           {
           TR_OpaqueClassBlock *subClass = subClassInfo->getClassId();
           if (subClass == peekedClazz)
           return true;
           }
           }
           }
         */
         }
      }

   return false;
   }



bool TR::InterProceduralAnalyzer::alreadyPeekedMethod(TR_ResolvedMethod *method, bool *success, TR::PriorPeekInfo **priorPeek)
   {
   ListIterator<TR::PriorPeekInfo> priorPeekedMethodsIt(&_successfullyPeekedMethods);
   TR::PriorPeekInfo *priorPeekedMethod = NULL;
   for (priorPeekedMethod = priorPeekedMethodsIt.getFirst(); priorPeekedMethod; priorPeekedMethod = priorPeekedMethodsIt.getNext())
      {
      if (priorPeekedMethod->_method->isSameMethod(method))
         {
         *priorPeek = priorPeekedMethod;
         //*success = true;
         return true;
         }
      }

   //peekedMethodsIt.set(&_unsuccessfullyPeekedMethods);
   ListIterator<TR_ResolvedMethod> peekedMethodsIt(&_unsuccessfullyPeekedMethods);
   TR_ResolvedMethod *peekedMethod;
   for (peekedMethod = peekedMethodsIt.getFirst(); peekedMethod; peekedMethod = peekedMethodsIt.getNext())
      {
      if (peekedMethod->isSameMethod(method))
         {
         *success = false;
         return true;
         }
      }

   return false;
   }



bool
TR::InterProceduralAnalyzer::addClassThatShouldNotBeLoaded(char *name, int32_t len)
   {
   bool found = false;

   for (ListElement<TR_ClassLoadCheck> *currClc = _classesThatShouldNotBeLoadedInCurrentPeek.getListHead(); currClc != _prevClc; currClc = currClc->getNextElement())
      {
     if (currClc->getData()->_length == len && !strncmp(currClc->getData()->_name, name, len))
         {
         found = true;
         break;
         }
      }

   if (!found)
      _classesThatShouldNotBeLoadedInCurrentPeek.add(new (trStackMemory()) TR_ClassLoadCheck(name, len));

   for (TR_ClassLoadCheck * clc = _classesThatShouldNotBeLoaded.getFirst(); clc; clc = clc->getNext())
      if (clc->_length == len && !strncmp(clc->_name, name, len))
         return false;

   _classesThatShouldNotBeLoaded.add(new (trHeapMemory()) TR_ClassLoadCheck(name, len));

   return true;
   }





bool
TR::InterProceduralAnalyzer::addClassThatShouldNotBeNewlyExtended(TR_OpaqueClassBlock *clazz)
   {
   TR::ClassTableCriticalSection addClassThatShouldNotBeNewlyExtended(comp()->fe());

   TR_PersistentClassInfo * cl = comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(clazz, comp());
   if(!cl)
      return false;

   if (!cl->shouldNotBeNewlyExtended(comp()->getCompThreadID()))
      addSingleClassThatShouldNotBeNewlyExtended(clazz);


   cl->setShouldNotBeNewlyExtended(comp()->getCompThreadID());
   TR_ScratchList<TR_PersistentClassInfo> subClasses(trMemory());

   TR_ClassQueries::collectAllSubClasses(cl, &subClasses, comp());

   ListIterator<TR_PersistentClassInfo> it(&subClasses);
   TR_PersistentClassInfo *info = NULL;
   for (info = it.getFirst(); info; info = it.getNext())
      {
      if (!info->shouldNotBeNewlyExtended(comp()->getCompThreadID()))
         {
         info->setShouldNotBeNewlyExtended(comp()->getCompThreadID());
         TR_OpaqueClassBlock *subClass = info->getClassId();
         addSingleClassThatShouldNotBeNewlyExtended(subClass);
         }
      }

   return true;
   }


uint32_t TR::InterProceduralAnalyzer::hash(void * h, uint32_t size)
   {
   // 2654435761 is the golden ratio of 2^32.
   //
   return (((uint32_t)(uintptrj_t)h >> 2) * 2654435761u) % size;
   }


bool TR::InterProceduralAnalyzer::addSingleClassThatShouldNotBeNewlyExtended(TR_OpaqueClassBlock *clazz)
   {
   bool found = false;
   uint32_t hashNum = hash(clazz, CLASSHASHTABLE_SIZE);
   for (ListElement<TR_ClassExtendCheck> *currCec = _classesThatShouldNotBeNewlyExtendedInCurrentPeekHT[hashNum].getListHead(); currCec != _prevCec; currCec = currCec->getNextElement())
      {
      if (currCec->getData()->_clazz == clazz)
         {
         found = true;
         break;
         }
      }

   if (!found)
      {
      _classesThatShouldNotBeNewlyExtendedInCurrentPeek.add(new (trStackMemory()) TR_ClassExtendCheck(clazz));
      _classesThatShouldNotBeNewlyExtendedInCurrentPeekHT[hashNum].add(new (trStackMemory()) TR_ClassExtendCheck(clazz));
      }

   found = false;
   for (TR_ClassExtendCheck *clc = _classesThatShouldNotBeNewlyExtendedHT[hashNum].getFirst(); clc; clc = clc->getNext())
      if (clc->_clazz == clazz)
         {
         found = true;
         break;
         //return false;
         }

   if (!found)
      {
      _classesThatShouldNotBeNewlyExtended.add(new (trHeapMemory()) TR_ClassExtendCheck(clazz));
      _classesThatShouldNotBeNewlyExtendedHT[hashNum].add(new (trHeapMemory()) TR_ClassExtendCheck(clazz));
      }
   return true;
   }




bool
TR::InterProceduralAnalyzer::addWrittenGlobal(TR::SymbolReference *symRef)
   {
   char *sig = NULL;
   int32_t length = 0;
   if (symRef->getSymbol()->isStaticField())
      sig = symRef->getOwningMethod(comp())->staticName(symRef->getCPIndex(), length, trMemory());
   else if (symRef->getSymbol()->isShadow())
      sig = symRef->getOwningMethod(comp())->fieldName(symRef->getCPIndex(), length, trMemory());

   bool found = false;
   for (ListElement<TR::GlobalSymbol> *currSymRef = _globalsWrittenInCurrentPeek.getListHead(); currSymRef != _prevSymRef; currSymRef = currSymRef->getNextElement())
      {
      TR::SymbolReference *currSymReference = currSymRef->getData()->_symRef;
      char *currSig = NULL;
      int32_t currLength = 0;
      if (currSymReference->getSymbol()->isStaticField())
         currSig = currSymReference->getOwningMethod(comp())->staticName(currSymReference->getCPIndex(), currLength, trMemory());
      else if (currSymReference->getSymbol()->isShadow())
         currSig = currSymReference->getOwningMethod(comp())->fieldName(currSymReference->getCPIndex(), currLength, trMemory());

      if ((length == currLength) &&
          (memcmp(sig, currSig, length) == 0))
         {
         found = true;
         break;
         }
      }

   if (!found)
      _globalsWrittenInCurrentPeek.add(new (trStackMemory()) TR::GlobalSymbol(symRef));

   for (TR::GlobalSymbol *currSym = _globalsWritten.getFirst(); currSym; currSym = currSym->getNext())
      {
      TR::SymbolReference *currSymReference = currSym->_symRef;
      char *currSig = NULL;
      int32_t currLength = 0;
      if (currSymReference->getSymbol()->isStaticField())
         currSig = currSymReference->getOwningMethod(comp())->staticName(currSymReference->getCPIndex(), currLength, trMemory());
      else if (currSymReference->getSymbol()->isShadow())
         currSig = currSymReference->getOwningMethod(comp())->fieldName(currSymReference->getCPIndex(), currLength, trMemory());

      if ((length == currLength) &&
          (memcmp(sig, currSig, length) == 0))
         return false;
      }

   _globalsWritten.add(new (trHeapMemory()) TR::GlobalSymbol(symRef));

   return true;
   }




bool
TR::InterProceduralAnalyzer::addMethodThatShouldNotBeNewlyOverridden(TR_OpaqueMethodBlock *method)
   {
   //
   // Should check if method is overridden in future causing peeked call graph to
   // change
   //
   // This check will be more precise than class extension check but
   // will also be moe expensive at run-time. Implement if really required in
   // the future
   //
   return true;
   }



