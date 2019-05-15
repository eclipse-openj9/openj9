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

#include "ilgen/ClassLookahead.hpp"
#include "codegen/CodeGenerator.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compiler/il/OMRTreeTop_inlines.hpp"
#include "env/ClassTableCriticalSection.hpp"
#include "env/IO.hpp"
#include "env/PersistentCHTable.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "ilgen/J9ByteCodeIlGenerator.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "infra/Cfg.hpp"

TR_ClassLookahead::TR_ClassLookahead(
   TR_PersistentClassInfo * classInfo, TR_FrontEnd * fe, TR::Compilation * comp,
   TR::SymbolReferenceTable * symRefTab)
   :
   _classPointer((TR_OpaqueClassBlock *) classInfo->getClassId()),
   _compilation(comp),
   _symRefTab(symRefTab),
   _fe(fe),
   _currentMethodSymbol(NULL)
   {
   _classFieldInfo = new (PERSISTENT_NEW) TR_PersistentClassInfoForFields;
   _classInfo = classInfo;
    _classFieldInfo->setFirst(0);
   _traceIt = comp->getOption(TR_TraceLookahead);
   }


int32_t
TR_ClassLookahead::perform()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe();

   if ((fej9->getNumInnerClasses(_classPointer) > 0) ||
       _classInfo->cannotTrustStaticFinal())
      return 0;

    bool isClassInitialized = false;
    bool seenFirstInitializerMethod = false;
    bool allowForAOT = comp()->getOption(TR_UseSymbolValidationManager);
    TR_PersistentClassInfo * classInfo =
                           comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(_classPointer, comp(), allowForAOT);
    if (classInfo && classInfo->isInitialized())
       isClassInitialized = true;

    if (!isClassInitialized)
       return 0;

   TR_ScratchList<TR_ResolvedMethod> resolvedMethodsInClass(comp()->trMemory());
   fej9->getResolvedMethods(comp()->trMemory(), _classPointer, &resolvedMethodsInClass);

   ListIterator<TR_ResolvedMethod> resolvedMethIt(&resolvedMethodsInClass);
   TR_ResolvedMethod *resolvedMethod = NULL;
   for (resolvedMethod = resolvedMethIt.getFirst(); resolvedMethod; resolvedMethod = resolvedMethIt.getNext())
      {
      if (resolvedMethod->isNative() ||
          resolvedMethod->isJNINative() ||
          resolvedMethod->isJITInternalNative())
         {
         _classInfo->setCannotTrustStaticFinal();
         return 0;
         }
      }

   bool b = comp()->getNeedsClassLookahead();
   comp()->setNeedsClassLookahead(false);

   int32_t len; char * name = fej9->getClassNameChars(_classPointer, len);

   if (_traceIt)
      printf("ATTN: Doing classlookahead for %.*s\n", len, name);

   if (!performTransformation(comp(), "O^O CLASS LOOKAHEAD: Performing class lookahead for %s\n", name))
       return 0;

   TR_ScratchList<TR::ResolvedMethodSymbol> initializerMethodsInClass(comp()->trMemory());
   TR_ScratchList<TR::ResolvedMethodSymbol> resolvedMethodSyms(comp()->trMemory());
   TR::ResolvedMethodSymbol *classInitializer = NULL;

   // check if peeking failed for any of the resolved methods
   // if so, classLookahead cannot proceed
   //
   bool peekFailedForAnyMethod = false;
   findInitializerMethods(&resolvedMethodsInClass, &initializerMethodsInClass, &resolvedMethodSyms, &classInitializer, &peekFailedForAnyMethod);

   if (peekFailedForAnyMethod)
      {
      comp()->setNeedsClassLookahead(b);
      _classInfo->setCannotTrustStaticFinal();
      return 0;
      }

   _inClassInitializerMethod = false;
   _inFirstInitializerMethod = false;

   if (classInitializer)
      {
      _currentMethodSymbol = classInitializer;
      _inClassInitializerMethod = true;
      _inInitializerMethod = true;
      _inFirstBlock = true;
      ///fprintf(stderr, "looking at method %s\n", classInitializer->getResolvedMethod()->nameChars()); fflush(stderr);

      vcount_t visitCount = comp()->incVisitCount();
      TR::TreeTop *tt;
      comp()->resetVisitCounts(0, classInitializer->getFirstTreeTop());
      for (tt = classInitializer->getFirstTreeTop(); tt; tt = tt->getNextTreeTop())
         {
         TR::Node *node = tt->getNode();
         if (!examineNode(tt->getNextTreeTop(), NULL, NULL, -1, node, visitCount))
            {
            _classFieldInfo->setFirst(0);
            _classInfo->setCannotTrustStaticFinal();
            comp()->setNeedsClassLookahead(b);
            return 2;
            }
         }

      _inClassInitializerMethod = false;
      }


   ListIterator<TR::ResolvedMethodSymbol> resolvedIt(&initializerMethodsInClass);
   TR::ResolvedMethodSymbol *resolvedMethodSymbol = NULL;
   resolvedMethod = NULL;
   for (resolvedMethodSymbol = resolvedIt.getFirst(); resolvedMethodSymbol; resolvedMethodSymbol = resolvedIt.getNext())
      {
      _currentMethodSymbol = resolvedMethodSymbol;
      resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
      if (strncmp(resolvedMethod->nameChars(), "<clinit>", 8))
         {
         //_inInitializerMethod = false;
         //if (findMethod(&initializerMethodsInClass, resolvedMethodSymbol))
            {
            if (!seenFirstInitializerMethod)
               {
               _inFirstInitializerMethod = true;
               seenFirstInitializerMethod = true;
               }

            _inInitializerMethod = true;
            if (!_inFirstInitializerMethod)
               initializeFieldInfo();
            }

            /////fprintf(stderr, "looking at method %s\n", resolvedMethod->nameChars()); fflush(stderr);
         TR::TreeTop *startTree = resolvedMethodSymbol->getFirstTreeTop();
         _inFirstBlock = true;
         vcount_t visitCount = comp()->incVisitCount();
         TR::TreeTop *tt;
         comp()->resetVisitCounts(0, startTree);
         for (tt = startTree; tt; tt = tt->getNextTreeTop())
            {
            TR::Node *node = tt->getNode();
            //dumpOptDetails("Node = %p node vc %d comp vc %d\n", node, node->getVisitCount(), visitCount);
            if (!examineNode(tt->getNextTreeTop(), NULL, NULL, -1, node, visitCount))
               {
               _classFieldInfo->setFirst(0);
               _classInfo->setCannotTrustStaticFinal();
               comp()->setNeedsClassLookahead(b);
               return 2;
               }
            }

         if (_inInitializerMethod)
            {
            updateFieldInfo();
            _inFirstInitializerMethod = false;
            }
         }
      }

   resolvedIt.set(&resolvedMethodSyms);
   resolvedMethodSymbol = NULL;
   resolvedMethod = NULL;
   for (resolvedMethodSymbol = resolvedIt.getFirst(); resolvedMethodSymbol; resolvedMethodSymbol = resolvedIt.getNext())
      {
      _currentMethodSymbol = resolvedMethodSymbol;
      resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
      if (strncmp(resolvedMethod->nameChars(), "<clinit>", 8))
         {
         if (!findMethod(&initializerMethodsInClass, resolvedMethodSymbol))
            {
            ////fprintf(stderr, "looking at method %s\n", resolvedMethod->nameChars()); fflush(stderr);
            _inInitializerMethod = false;
            _inFirstInitializerMethod = false;

            TR::TreeTop *startTree = resolvedMethodSymbol->getFirstTreeTop();
            _inFirstBlock = true;
            vcount_t visitCount = comp()->incVisitCount();
            TR::TreeTop *tt;
            comp()->resetVisitCounts(0, startTree);
            for (tt = startTree; tt; tt = tt->getNextTreeTop())
               {
               TR::Node *node = tt->getNode();
               //dumpOptDetails("Node = %p node vc %d comp vc %d\n", node, node->getVisitCount(), visitCount);
               ////fprintf(stderr, "looking at node in method %s\n", resolvedMethod->nameChars()); fflush(stderr);
               if (!examineNode(tt->getNextTreeTop(), NULL, NULL, -1, node, visitCount))
                  {
                  _classFieldInfo->setFirst(0);
                  _classInfo->setCannotTrustStaticFinal();
                  comp()->setNeedsClassLookahead(b);
                  return 2;
                  }
               }
            }
         }
      }

   if (_classFieldInfo->getFirst())
       makeInfoPersistent();

   classInfo->setFieldInfo(_classFieldInfo);

   comp()->setNeedsClassLookahead(b);
   return 2;
   }


static bool isCalledByNonConstructorMethodsInClass(TR_ResolvedMethod *calleeMethod, List<TR::ResolvedMethodSymbol> *resolvedMethodSyms)
   {
   ListIterator<TR::ResolvedMethodSymbol> resolvedIt(resolvedMethodSyms);
   TR::ResolvedMethodSymbol *resolvedMethodSymbol;
   for (resolvedMethodSymbol = resolvedIt.getFirst(); resolvedMethodSymbol; resolvedMethodSymbol = resolvedIt.getNext())
      {
      TR_ResolvedMethod *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
      if (!resolvedMethod->isConstructor())
         {
         for (TR::TreeTop *tt = resolvedMethodSymbol->getFirstTreeTop(); tt; tt = tt->getNextTreeTop())
            {
            TR::Node *node = tt->getNode();
            if (node->getNumChildren() > 0 &&
                node->getFirstChild()->getOpCode().isCall() &&
                !node->getFirstChild()->getOpCode().isIndirect())
               {
               TR_Method *calleeMethod1 = node->getFirstChild()->getSymbol()->getMethodSymbol()->getMethod();
               if (calleeMethod1 &&
                   calleeMethod1->nameLength() == calleeMethod->nameLength() &&
                   calleeMethod1->signatureLength() == calleeMethod->signatureLength() &&
                   calleeMethod1->classNameLength() == calleeMethod->classNameLength() &&
                   !strncmp(calleeMethod1->nameChars(), calleeMethod->nameChars(), calleeMethod->nameLength()) &&
                   !strncmp(calleeMethod1->signatureChars(), calleeMethod->signatureChars(), calleeMethod->signatureLength()) &&
                   !strncmp(calleeMethod1->classNameChars(), calleeMethod->classNameChars(), calleeMethod->classNameLength()))
                  return true;
               }
            }
         }
      }

   return false;
   }



void
TR_ClassLookahead::findInitializerMethods(List<TR_ResolvedMethod> *resolvedMethodsInClass, List<TR::ResolvedMethodSymbol> *initializerMethodsInClass, List<TR::ResolvedMethodSymbol> *resolvedMethodSyms, TR::ResolvedMethodSymbol **classInitializer, bool *peekFailedForAnyMethod)
   {
   TR_IlGenerator *prevIlGen = comp()->getCurrentIlGenerator();

   ListIterator<TR_ResolvedMethod> resolvedIt(resolvedMethodsInClass);
   TR_ResolvedMethod *resolvedMethod;
   for (resolvedMethod = resolvedIt.getFirst(); resolvedMethod; resolvedMethod = resolvedIt.getNext())
      {
      TR::SymbolReference *methodSymRef;
      if (resolvedMethod->isStatic())
         methodSymRef = _symRefTab->findOrCreateMethodSymbol(JITTED_METHOD_INDEX, -1, resolvedMethod, TR::MethodSymbol::Static);
      else
         methodSymRef = _symRefTab->findOrCreateMethodSymbol(JITTED_METHOD_INDEX, -1, resolvedMethod, TR::MethodSymbol::Interface);

      TR::ResolvedMethodSymbol *resolvedMethodSymbol = methodSymRef->getSymbol()->castToResolvedMethodSymbol();
      if (resolvedMethod->isCompilable(comp()->trMemory()) && !resolvedMethod->isNewInstanceImplThunk() && !resolvedMethod->isJNINative())
         {
         resolvedMethodSyms->add(resolvedMethodSymbol);
         //printf("IL gen succeeded for %s resolvedMethod %x ramMethod %x\n", resolvedMethod->signature(), resolvedMethod, resolvedMethod->getPersistentIdentifier());
         _symRefTab->addParameters(resolvedMethodSymbol);
         //ilGenSuccess = (0 != TR_J9ByteCodeIlGenerator::genMethodILForPeeking(resolvedMethodSymbol, comp()));
         *peekFailedForAnyMethod = (NULL == resolvedMethodSymbol->getResolvedMethod()->genMethodILForPeeking(resolvedMethodSymbol, comp(), true));
         }
      }

   if (*peekFailedForAnyMethod)
      {
      comp()->setCurrentIlGenerator(prevIlGen);
      return;
      }

   ListIterator<TR::ResolvedMethodSymbol> resolvedMethodSymsIt(resolvedMethodSyms);
   TR::ResolvedMethodSymbol *resolvedMethodSymbol;
   for (resolvedMethodSymbol = resolvedMethodSymsIt.getFirst(); resolvedMethodSymbol; resolvedMethodSymbol = resolvedMethodSymsIt.getNext())
      {
      resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
      if ((resolvedMethod->isConstructor() || (!strncmp(resolvedMethod->nameChars(), "<clinit>", 8))) && resolvedMethodSymbol->getFirstTreeTop())
         {
         if (!strncmp(resolvedMethod->nameChars(), "<clinit>", 8))
            *classInitializer = resolvedMethodSymbol;
         else
            {
            TR::ResolvedMethodSymbol *initializerMethodSymbol = resolvedMethodSymbol;

            TR::TreeTop * tt = resolvedMethodSymbol->getFirstTreeTop()->getNextRealTreeTop();
            TR::Node *firstRealNode = tt->getNode();
            if (firstRealNode->getOpCodeValue() == TR::treetop)
               {
               TR::Node *callChild = firstRealNode->getFirstChild();
               if ((callChild->getOpCodeValue() == TR::call) &&
                   !callChild->getSymbolReference()->isUnresolved())
                  {
                  TR_ResolvedMethod *calleeMethod = callChild->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod();
                  if (!strncmp(calleeMethod->classNameChars(), "java/lang/Object", 16))
                     {
                     if (calleeMethod->isConstructor())
                        {
                        if (!strncmp(calleeMethod->signatureChars(), "()V", 3))
                           tt = tt->getNextRealTreeTop();
                        }
                     }
                  }
               }

            TR::Node *node = tt->getNode();
            if (node->getOpCode().isTreeTop() &&
                (node->getNumChildren() > 0))
                node = node->getFirstChild();

            if (node->getOpCode().isCall() &&
                !node->getOpCode().isIndirect() &&
                !node->getSymbolReference()->isUnresolved())
                {
                TR::ResolvedMethodSymbol *calleeSymbol = node->getSymbol()->castToResolvedMethodSymbol();
                TR_ResolvedMethod *calleeMethod = calleeSymbol->getResolvedMethod();
                if (calleeMethod->containingClass() == _classPointer &&
                    (!strncmp(calleeMethod->nameChars(), "<init>", 6) ||
                    calleeMethod->isPrivate() &&
                    !isCalledByNonConstructorMethodsInClass(calleeMethod, resolvedMethodSyms)))
                    initializerMethodSymbol = calleeSymbol;
                }

            //if (!initializerMethodsInClass->find(initializerMethodSymbol))
            if (!findMethod(initializerMethodsInClass, initializerMethodSymbol))
               initializerMethodsInClass->add(initializerMethodSymbol);
               ///initializerMethodsInClass->addAfter(initializerMethodSymbol, initializerMethodsInClass->getListHead());
            }
         }
      }

   ListElement<TR::ResolvedMethodSymbol> *initMethodSymElem = initializerMethodsInClass->getListHead();
   while (initMethodSymElem)
      {
      TR::ResolvedMethodSymbol *initMethodSym = initMethodSymElem->getData();
      if (!resolvedMethodSyms->find(initMethodSym))
         {
         ListIterator<TR::ResolvedMethodSymbol> resolvedIt(resolvedMethodSyms);
         TR::ResolvedMethodSymbol *resolvedMethodSym;
         for (resolvedMethodSym = resolvedIt.getFirst(); resolvedMethodSym; resolvedMethodSym = resolvedIt.getNext())
            {
            if (resolvedMethodSym->getResolvedMethod()->isSameMethod(initMethodSym->getResolvedMethod()))
               {
               initMethodSymElem->setData(resolvedMethodSym);
               break;
               }
            }
         }

      initMethodSymElem = initMethodSymElem->getNextElement();
      }

   comp()->setCurrentIlGenerator(prevIlGen);
   }







bool
TR_ClassLookahead::examineNode(TR::TreeTop *nextTree, TR::Node *grandParent, TR::Node *parent, int32_t childNum, TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      {
      if (node->getOpCode().isLoadVar())
         invalidateIfEscapingLoad(nextTree, grandParent, parent, childNum, node);
      return true;
      }

   if (node->getOpCode().isStore())
      {
      TR::SymbolReference *storedSymRef = node->getSymbolReference();
      TR::Symbol *sym = storedSymRef->getSymbol();
      //dumpOptDetails("Store node %p fieldInfo %p\n", node, fieldInfo);

      if (isProperFieldAccess(node))
          {
          //dumpOptDetails("Proper store node %p fieldInfo %p\n", node, fieldInfo);
          int32_t childAdjust = (node->getOpCode().isWrtBar()) ? 2 : 1;
          TR::Node *rhsOfStoreNode = node->getChild(node->getNumChildren() - childAdjust);

          bool isConstantLengthArrayAllocation = false;
          bool isInitialized = false;
          int32_t numberOfDimensions = -1;
          int32_t firstDimensionChild = 0;
          int32_t firstConstantLengthArrayDim = -1;
          int32_t rhsNumChildren = rhsOfStoreNode->getNumChildren();
          const char *sig = NULL;
          int32_t length = 0;
          bool isNewArray = true;

          if ((rhsOfStoreNode->getOpCodeValue() == TR::newarray) ||
              (rhsOfStoreNode->getOpCodeValue() == TR::anewarray) ||
              (rhsOfStoreNode->getOpCodeValue() == TR::multianewarray))
             {
             if (rhsOfStoreNode->getOpCodeValue() == TR::multianewarray)
                {
                firstDimensionChild = 1;
                numberOfDimensions = rhsOfStoreNode->getFirstChild()->getInt();
                }
             else
                {
                numberOfDimensions = 1;

                if (rhsOfStoreNode->getOpCodeValue() == TR::anewarray)
                   {
                     // TODO: Do not consider types for arrays yet; have to
                     // be careful when appending '[' correct number of times
                     // for compatibility with constant propagation.
                     //
                     //
                     //sig = rhsOfStoreNode->getSecondChild()->getSymbolReference()->getTypeSignature(length);
                   }
                }

             int32_t j;
             for (j=firstDimensionChild;j<rhsNumChildren - 1;j++)
                {
                TR::Node *rhsChild = rhsOfStoreNode->getChild(j);
                if (rhsChild->getOpCodeValue() == TR::iconst)
                   {
                   isConstantLengthArrayAllocation = true;
                   firstConstantLengthArrayDim = j;
                   break;
                   }
                }
             }
          else if (rhsOfStoreNode->getOpCodeValue() == TR::New)
             {
             isNewArray = false;
             sig = rhsOfStoreNode->getFirstChild()->getSymbolReference()->getTypeSignature(length);
             }
          else
             isNewArray = false;

          // if this is a store to an arrayfield and a fieldInfo already
          // exists (say created via a store seen in another ctor), then
          // invalidate the fieldInfo
          //
          // check for stores of aconst null
          //
          bool canMorph = false;
          if (rhsOfStoreNode->getOpCodeValue() == TR::aconst &&
              rhsOfStoreNode->getInt() == 0)
             canMorph = true;

          TR_PersistentFieldInfo *fieldInfo = NULL;

          if (isNewArray)
              fieldInfo = getExistingArrayFieldInfo(sym, storedSymRef);
          else
              fieldInfo = getExistingFieldInfo(sym, storedSymRef, canMorph);

          //
          // TODO: remove restriction on number of dimensions
          //
          if (fieldInfo &&
              !storedSymRef->isUnresolved() &&
              (sym->isPrivate() ||
               sym->isFinal()))
             {
             if (fieldInfo->isTypeInfoValid())
                {
                bool isClassPtrInitialized = false;
                bool classPtrSame = true;
                if (_inFirstBlock &&
                    _inInitializerMethod)
                   {
                   int32_t oldNumChars = fieldInfo->getNumChars();
                   if (oldNumChars > -1)
                      {
                      if (length != oldNumChars)
                         classPtrSame = false;
                      else
                         classPtrSame = (memcmp(sig, fieldInfo->getClassPointer(), length) == 0);
                      }
                   else
                      {
                      isClassPtrInitialized = true;
                      }
                   }
                else
                   classPtrSame = false;

                if (classPtrSame || isClassPtrInitialized)
                   {
                   if (isClassPtrInitialized)
                      {
                      //char *persistentSig = (char *)TR_Memory::jitPersistentAlloc(length);
                      //memcpy(persistentSig, sig, length);
                      //fieldInfo->setClassPointer(persistentSig);
                      fieldInfo->setClassPointer(sig);
                      }

                   fieldInfo->setNumChars(length);
                   if (_inClassInitializerMethod)
                      {
                      if (_traceIt)
                         traceMsg(comp(), "CStatically initializing type info for symbol %x at node %x\n", sym, node);
                      fieldInfo->setIsTypeInfoValid(VALID_AND_INITIALIZED_STATICALLY);
                      }
                   else
                      {
                      if (fieldInfo->isTypeInfoValid() == VALID_AND_INITIALIZED_STATICALLY)
                         {
                         if (!_inFirstInitializerMethod &&
                             !sym->isStatic())
                            {
                            if (_traceIt)
                               traceMsg(comp(), "C0Not always initializing type info for symbol %x at node %x\n", sym, node);
                            fieldInfo->setIsTypeInfoValid(VALID_BUT_NOT_ALWAYS_INITIALIZED);
                            }
                         else
                            {
                            if (!sym->isStatic())
                               {
                               if (_traceIt)
                                  traceMsg(comp(), "C0Always initializing type info for symbol %x at node %x\n", sym, node);
                               fieldInfo->setIsTypeInfoValid(VALID_AND_ALWAYS_INITIALIZED);
                               }
                            }
                         }
                      else
                         {
                         if (!sym->isStatic())
                            {
                            if (_traceIt)
                               traceMsg(comp(), "C1Always initializing type info for symbol %x at node %x\n", sym, node);
                            fieldInfo->setIsTypeInfoValid(VALID_AND_ALWAYS_INITIALIZED);
                            }
                         else
                            {
                            if (_traceIt)
                              traceMsg(comp(), "C0Invalidating type info for symbol %x at node %x\n", sym, node);
                            fieldInfo->setIsTypeInfoValid(INVALID);
                            }
                         }
                      }
                   }
                else
                   {
                   if (_traceIt)
                      traceMsg(comp(), "C0Invalidating type info for symbol %x at node %x\n", sym, node);
                   fieldInfo->setIsTypeInfoValid(INVALID);
                   }
                }

             TR_PersistentArrayFieldInfo *arrayFieldInfo = fieldInfo->asPersistentArrayFieldInfo();
             if (isNewArray &&
                 arrayFieldInfo &&
                 isConstantLengthArrayAllocation &&
                 (numberOfDimensions <= 2))
                {
                int32_t oldNumDimensions = arrayFieldInfo->getNumDimensions();
                if (oldNumDimensions > -1)
                   {
                   if (arrayFieldInfo->isDimensionInfoValid())
                      {
                      if (oldNumDimensions != numberOfDimensions)
                         {
                         // Same field is assigned different dimension arrays
                         // at different program points within this class.
                         //
                         if (_traceIt)
                             traceMsg(comp(), "0Invalidating dimension info for symbol %x at node %x\n", sym, node);
                         arrayFieldInfo->setIsDimensionInfoValid(INVALID);
                         }
                      }
                   }
                else
                   {
                   isInitialized = true;
                   arrayFieldInfo->prepareArrayFieldInfo(numberOfDimensions, comp());
                   }

                if (arrayFieldInfo->isDimensionInfoValid())
                   {
                   // Invalidate the dimensions that are known to be
                   // non constants at this stage
                   //
                   int32_t k;
                   for (k=0;k<firstConstantLengthArrayDim - firstDimensionChild;k++)
                       arrayFieldInfo->setDimensionInfo(k, -1);

                   for (;k<(rhsNumChildren - 1 - firstDimensionChild);k++)
                       {
                       TR::Node *rhsChild = rhsOfStoreNode->getChild(k+firstDimensionChild);
                       if (_inFirstBlock &&
                           _inInitializerMethod &&
                           (rhsChild->getOpCodeValue() == TR::iconst) &&
                           ((rhsChild->getInt() == arrayFieldInfo->getDimensionInfo(k)) ||
                            isInitialized))
                           {
                           arrayFieldInfo->setDimensionInfo(k, rhsChild->getInt());
                           if (_inClassInitializerMethod)
                              {
                              if (_traceIt)
                                  traceMsg(comp(), "Statically initializing dimension info for symbol %x at node %x\n", sym, node);
                              arrayFieldInfo->setIsDimensionInfoValid(VALID_AND_INITIALIZED_STATICALLY);
                              }
                           else
                              {
                              if (arrayFieldInfo->isDimensionInfoValid() == VALID_AND_INITIALIZED_STATICALLY)
                                 {
                                 if (!_inFirstInitializerMethod &&
                                     !sym->isStatic())
                                    {
                                    if (_traceIt)
                                       traceMsg(comp(), "0Not always initializing dimension info for symbol %x at node %x\n", sym, node);
                                    arrayFieldInfo->setIsDimensionInfoValid(VALID_BUT_NOT_ALWAYS_INITIALIZED);
                                    }
                                 else
                                    {
                                    if (!sym->isStatic())
                                       {
                                       if (_traceIt)
                                          traceMsg(comp(), "0Always initializing dimension info for symbol %x at node %x\n", sym, node);
                                       arrayFieldInfo->setIsDimensionInfoValid(VALID_AND_ALWAYS_INITIALIZED);
                                       }
                                    }
                                 }
                              else
                                 {
                                 if (!sym->isStatic())
                                    {
                                    if (_traceIt)
                                       traceMsg(comp(), "1Always initializing dimension info for symbol %x at node %x\n", sym, node);
                                    arrayFieldInfo->setIsDimensionInfoValid(VALID_AND_ALWAYS_INITIALIZED);
                                    }
                                 else
                                    {
                                    if (_traceIt)
                                       traceMsg(comp(), "C0Invalidating dimension info for symbol %x at node %x\n", sym, node);
                                    arrayFieldInfo->setIsDimensionInfoValid(INVALID);
                                    }
                                 }
                              }
                           }
                       else
                           {
                           if (_traceIt)
                              traceMsg(comp(), "C1Invalidating dimension info for symbol %x at node %x\n", sym, node);
                           arrayFieldInfo->setIsDimensionInfoValid(INVALID);
                           arrayFieldInfo->setDimensionInfo(k, -1);
                           }
                       }
                   }
                }
             else if (arrayFieldInfo)
                {
                // Not a constant length allocation event
                //
                if (_traceIt)
                  traceMsg(comp(), "1Invalidating dimension info for symbol %x at node %x\n", sym, node);
                arrayFieldInfo->setIsDimensionInfoValid(INVALID);
                }
             }
          else if (fieldInfo)
             {
             // It is a store to this array field but not an allocation event
             //
             if (_traceIt)
                 traceMsg(comp(), "1Invalidating dimension and type info for symbol %x at node %x(%s)\n", sym, node, node->getOpCode().getName());

             fieldInfo->setIsTypeInfoValid(INVALID);
             TR_PersistentArrayFieldInfo *arrayFieldInfo = fieldInfo->asPersistentArrayFieldInfo();
             if (arrayFieldInfo)
                arrayFieldInfo->setIsDimensionInfoValid(INVALID);
             else
	        fieldInfo->setCanChangeToArray(false);
             }
          }
       else if ((sym->isShadow() ||
                 sym->isStatic()) &&
                !storedSymRef->isUnresolved() &&
                (sym->isPrivate() ||
                 sym->isFinal()))
          {
          TR_PersistentFieldInfo *fieldInfo = getExistingFieldInfo(sym, storedSymRef, false);

          if (fieldInfo)
             {
             if (_traceIt)
               traceMsg(comp(), "2Invalidating dimension and type info for symbol %x at node %x\n", sym, node);

             fieldInfo->setIsTypeInfoValid(INVALID);
             TR_PersistentArrayFieldInfo *arrayFieldInfo = fieldInfo->asPersistentArrayFieldInfo();
             if (arrayFieldInfo)
                arrayFieldInfo->setIsDimensionInfoValid(INVALID);
             }
          }

      if (sym->isStatic() && sym->isFinal() && !_inClassInitializerMethod)
	 _classInfo->setCannotTrustStaticFinal();

      if (isPrivateFieldAccess(node) || isProperFieldAccess(node))
         {
         //int32_t length = 0;
         //char *sig = TR_ClassLookahead::getFieldSignature(comp(), sym, storedSymRef, length);

         TR_PersistentFieldInfo *previousFieldInfo = _classFieldInfo->find(comp(), sym, storedSymRef);
         TR_PersistentFieldInfo *fieldInfo = previousFieldInfo;
         if (!fieldInfo)
            {
            fieldInfo = getExistingFieldInfo(sym, storedSymRef);
            if (fieldInfo)
               {
               fieldInfo->setIsTypeInfoValid(INVALID);
               TR_PersistentArrayFieldInfo *arrayFieldInfo = fieldInfo->asPersistentArrayFieldInfo();
               if (arrayFieldInfo)
                  arrayFieldInfo->setIsDimensionInfoValid(INVALID);
               else
	          fieldInfo->setCanChangeToArray(false);
               }
            }

         //if (isProperFieldAccess(node))
            {
            if (fieldInfo)
               {
               if (!_inInitializerMethod && !_inClassInitializerMethod)
                  {
                  fieldInfo->setImmutable(false);
                  }

               int32_t childAdjust = (node->getOpCode().isWrtBar()) ? 2 : 1;
               TR::Node *rhsOfStoreNode = node->getChild(node->getNumChildren() - childAdjust);

               int32_t length;
               char *sig = getFieldSignature(comp(), sym, storedSymRef, length);

               if (rhsOfStoreNode->getOpCode().isCall() &&
                   rhsOfStoreNode->getSymbol()->getResolvedMethodSymbol())
                  {
                  if (rhsOfStoreNode->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigDecimal_valueOf)
                     {
                     //fprintf(stderr, "0Resetting big integer property for %s in %s\n", sig, _currentMethodSymbol->getResolvedMethod()->nameChars()); fflush(stderr);
                     fieldInfo->setBigIntegerType(false);
                     }
                  else if ((rhsOfStoreNode->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigDecimal_add) ||
                      (rhsOfStoreNode->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigDecimal_subtract) ||
                      (rhsOfStoreNode->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigDecimal_multiply))
                     {
                     //fprintf(stderr, "1Resetting big integer property for %s in %s\n", sig, _currentMethodSymbol->getResolvedMethod()->nameChars()); fflush(stderr);
                     fieldInfo->setBigIntegerType(false);
                     }
                  else if ((rhsOfStoreNode->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigInteger_add) ||
                           (rhsOfStoreNode->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigInteger_subtract) ||
                           (rhsOfStoreNode->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigInteger_multiply))
                     {
                     fieldInfo->setBigDecimalType(false);
                     }
                  }
               else
                  {
                  if (rhsOfStoreNode->getOpCode().isLoadVar() &&
                      rhsOfStoreNode->getSymbolReference()->getSymbol()->isStatic())
                     {
                     TR::Symbol::RecognizedField field = rhsOfStoreNode->getSymbolReference()->getSymbol()->getRecognizedField();

                     if (field != TR::Symbol::Java_math_BigInteger_ZERO)
                        {
                        //fprintf(stderr, "2Resetting big integer property for %s in %s\n", sig, _currentMethodSymbol->getResolvedMethod()->nameChars()); fflush(stderr);
                        fieldInfo->setBigIntegerType(false);
                        }
                     else
                        {
                        //fprintf(stderr, "Found load of big integer ZERO in for %s (big integer type %d) in %s compiling %s\n", sig, fieldInfo->isBigIntegerType(), _currentMethodSymbol->getResolvedMethod()->nameChars(), comp()->signature()); fflush(stderr);
                        }
                     }
                  else
                     {
                     //fprintf(stderr, "3Resetting big integer property for %s in %s\n", sig, _currentMethodSymbol->getResolvedMethod()->nameChars()); fflush(stderr);
                     fieldInfo->setBigIntegerType(false);
                     }

                  fieldInfo->setBigDecimalType(false);
                  }
               }
            }
         }
      }
   else if (node->getOpCode().isLoadVar())
      invalidateIfEscapingLoad(nextTree, grandParent, parent, childNum, node);
   else if (node->getOpCodeValue() == TR::BBEnd)
      _inFirstBlock = false;

   if (node->getOpCode().isCall())
      {
      TR_Method *calleeMethod = node->getSymbol()->castToMethodSymbol()->getMethod();
      if (calleeMethod != NULL && !strncmp(calleeMethod->classNameChars(), "java/lang/reflect", 17))
         return false;
      }

   node->setVisitCount(visitCount);

   int32_t i;
   int32_t numChildren = node->getNumChildren();
   for (i=0;i<numChildren;i++)
      {
      if (!examineNode(nextTree, parent, node, i, node->getChild(i), visitCount))
         return false;
      }

   return true;
   }


static bool  isArithmeticForSameField(TR::Node *treeNode, TR::Node *grandParent, TR::Node *parent, TR::Node *node)
   {
   if ((node->getOpCodeValue() != TR::iloadi) &&
       (node->getOpCodeValue() != TR::lloadi) &&
       (node->getOpCodeValue() != TR::iload) &&
       (node->getOpCodeValue() != TR::lload))
      return false;

   if (parent->getReferenceCount() != 1)
      return false;

   if (!grandParent)
      return false;

   TR::Node *storeNode = NULL;
   if (!grandParent->getOpCode().isStore())
      {
      if (grandParent->getOpCode().isConversion())
         {
         if (treeNode->getOpCodeValue() == TR::treetop ||
             treeNode->getOpCode().isNullCheck())
            treeNode = treeNode->getFirstChild();

         if (treeNode->getOpCode().isStore())
             {
             TR::Node *valueChild = NULL;
             if (treeNode->getOpCode().isIndirect())
                valueChild = treeNode->getSecondChild();
             else
                valueChild = treeNode->getFirstChild();

             if (valueChild->getOpCode().isConversion() &&
                 (valueChild->getReferenceCount() == 1) &&
                 valueChild->getFirstChild()->getOpCode().isConversion() &&
                 (valueChild->getFirstChild()->getReferenceCount() == 1) &&
                 (grandParent == valueChild->getFirstChild()) &&
                 (treeNode->getDataType() == parent->getDataType()))
                {
                // Tree matches for update of byte/char/short datatype
                storeNode = treeNode;
                }
             else
                return false;
             }
          else
             return false;
          }
      else
         return false;
      }
   else
      {
      if (grandParent->getOpCode().isIndirect() &&
          (grandParent->getSecondChild() != parent))
         return false;

      storeNode = grandParent;
      }


   if ((parent->getOpCodeValue() == TR::iadd) ||
       (parent->getOpCodeValue() == TR::isub) ||
       (parent->getOpCodeValue() == TR::imul))
      {
      if (node->getOpCodeValue() == TR::iloadi)
         {
         if (storeNode->getOpCodeValue() == TR::istorei)
            {
            if (storeNode->getSymbolReference() == node->getSymbolReference())
               {
               if (storeNode->getFirstChild()->getOpCodeValue() == node->getFirstChild()->getOpCodeValue())
                  {
                  if ((storeNode->getFirstChild() == node->getFirstChild()) ||
                      (storeNode->getFirstChild()->getSymbolReference() == node->getFirstChild()->getSymbolReference()))
                     return true;
                  }
               }
            }
         }
      else if ((node->getOpCodeValue() == TR::iload) &&
               (storeNode->getOpCodeValue() == TR::istore) &&
               (storeNode->getSymbolReference() == node->getSymbolReference()))
         return true;
      }
   else if ((parent->getOpCodeValue() == TR::ladd) ||
            (parent->getOpCodeValue() == TR::lsub) ||
            (parent->getOpCodeValue() == TR::lmul))
      {
      if (node->getOpCodeValue() == TR::lloadi)
         {
         if (storeNode->getOpCodeValue() == TR::lstorei)
            {
            if (storeNode->getSymbolReference() == node->getSymbolReference())
               {
               if (storeNode->getFirstChild()->getOpCodeValue() == node->getFirstChild()->getOpCodeValue())
                  {
                  if ((storeNode->getFirstChild() == node->getFirstChild()) ||
                      (storeNode->getFirstChild()->getSymbolReference() == node->getFirstChild()->getSymbolReference()))
                     return true;
                  }
               }
            }
         }
      else if ((node->getOpCodeValue() == TR::lload) &&
               (storeNode->getOpCodeValue() == TR::lstore) &&
               (storeNode->getSymbolReference() == node->getSymbolReference()))
         return true;
      }

   return false;
   }


static bool isPureBigDecimalMethod(TR::Node *node, TR::Compilation *comp, TR_PersistentFieldInfo *fieldInfo, bool & isBigDecimal, bool & isBigInteger)
   {
   if (!node)
      return false;

   //fprintf(stderr, "0Calling isPure %s\n", node->getOpCode().getName()); fflush(stderr);
   //if (node->getOpCodeValue() == TR::aloadi)
   //   {
   //   fprintf(stderr, "node sym ref %p offset %d vft %p\n", node->getSymbolReference(), node->getSymbolReference()->getOffset(), comp->getSymRefTab()->findVftSymbolRef());
   //   }


   if (node->getOpCodeValue() != TR::acalli)
      {
      //fprintf(stderr, "1Returning false from isPure %s\n", node->getOpCode().getName()); fflush(stderr);
      return false;
      }

   if (node->getSymbolReference()->isUnresolved())
      {
      //fprintf(stderr, "2Returning false from isPure\n"); fflush(stderr);
      return false;
      }

   if ((fieldInfo->isBigDecimalType() &&
        ((node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigDecimal_add) ||
         (node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigDecimal_subtract) ||
         (node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigDecimal_multiply))))
      {
      isBigDecimal = true;
      return true;
      }


   if ((fieldInfo->isBigIntegerType() &&
        ((node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigInteger_add) ||
         (node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigInteger_subtract) ||
         (node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigInteger_multiply))))
      {
      isBigInteger = true;
      return true;
      }

   //fprintf(stderr, "is big decimal %d is big integer %d\n", fieldInfo->isBigDecimalType(), fieldInfo->isBigIntegerType());
   return false;
   }


static bool isStoreToSameField(TR::Node *callNode, TR::Node *nextNode, TR::Node *loadNode)
   {
   if (callNode->getReferenceCount() == 2)
      {
      if (nextNode->getOpCodeValue() == TR::treetop ||
          nextNode->getOpCode().isNullCheck())
         nextNode = nextNode->getFirstChild();

      if ((nextNode->getOpCodeValue() == TR::awrtbari) ||
          (nextNode->getOpCodeValue() == TR::astorei))
        {
        if (nextNode->getSymbolReference() == loadNode->getSymbolReference())
           {
           if (nextNode->getFirstChild()->getOpCodeValue() == loadNode->getFirstChild()->getOpCodeValue())
              {
              if ((nextNode->getFirstChild() == loadNode->getFirstChild()) ||
                  (nextNode->getFirstChild()->getSymbolReference() == loadNode->getFirstChild()->getSymbolReference()))
                 {
                 if (nextNode->getSecondChild() == callNode)
                    return true;
                 }
              }
           }
        }
      else if ((nextNode->getOpCodeValue() == TR::awrtbar) ||
               (nextNode->getOpCodeValue() == TR::astore))
        {
        if (nextNode->getSymbolReference() == loadNode->getSymbolReference())
           {
           if (nextNode->getFirstChild() == callNode)
              return true;
           }
        }
      }

   return false;
   }


void TR_ClassLookahead::invalidateIfEscapingLoad(TR::TreeTop *nextTree, TR::Node *grandParent, TR::Node *parent, int32_t childNum, TR::Node *node)
   {
   TR::SymbolReference *storedSymRef = node->getSymbolReference();
   TR::Symbol *sym = storedSymRef->getSymbol();

   //fprintf(stderr, "Check escaping load for node %s\n", node->getOpCode().getName()); fflush(stderr);

   if ((sym->isShadow() ||
       sym->isStatic()) &&
       (storedSymRef->isUnresolved() ||
        (sym->isPrivate() ||
         sym->isFinal())))
       {
       //fprintf(stderr, "2Check escaping load for node %s\n", node->getOpCode().getName()); fflush(stderr);
       TR_PersistentFieldInfo *previousFieldInfo = _classFieldInfo->find(comp(), sym, storedSymRef);
       TR_PersistentFieldInfo *fieldInfo = previousFieldInfo;
       if (!fieldInfo)
          {
          fieldInfo = getExistingFieldInfo(sym, storedSymRef);
          //fprintf(stderr, "Check escaping load for fieldInfo %p\n", fieldInfo); fflush(stderr);
          if (fieldInfo)
             {
             fieldInfo->setIsTypeInfoValid(INVALID);
             TR_PersistentArrayFieldInfo *arrayFieldInfo = fieldInfo->asPersistentArrayFieldInfo();
             if (arrayFieldInfo)
                arrayFieldInfo->setIsDimensionInfoValid(INVALID);
             else
	        fieldInfo->setCanChangeToArray(false);
             }
          }

       TR_PersistentArrayFieldInfo *arrayFieldInfo = NULL;
       if (fieldInfo)
          {
          int32_t length;
          char *sig = getFieldSignature(comp(), sym, storedSymRef, length);
          /////fprintf(stderr, "Check NOT read for field sig %s\n", sig); fflush(stderr);

          bool knownRead = false;
          bool isBigDecimal = false;
          bool isBigInteger = false;

          if  (sym->isStatic() ||
               (sym->isShadow() &&
                node->getFirstChild()->isThisPointer()))
             {
             bool refCountMatches = false;
             int32_t refCount = 2;
             int32_t extraRefCount = comp()->useAnchors() ? 1 : 0;
             if ((node->getReferenceCount() == (refCount + extraRefCount)) ||
                 (node->getReferenceCount() == (refCount + extraRefCount)))
                refCountMatches = true;

             if (refCountMatches)
                {
                if (parent &&
                    ((parent->getOpCodeValue() == TR::treetop) || parent->getOpCode().isAnchor()))
                   knownRead = true;

                if (isPureBigDecimalMethod(parent, comp(), fieldInfo, isBigDecimal, isBigInteger) && (childNum == 1) &&
                    (parent->getFirstChild()->getOpCodeValue() == TR::aloadi) &&
                    (parent->getFirstChild()->getFirstChild() == node))
                   {
                   if ((parent->getReferenceCount() == 1) ||
                       isStoreToSameField(parent, nextTree->getNode(), node))
                      knownRead = true;
                   }

                if (parent && (parent->getOpCodeValue() == TR::aloadi) && comp()->getSymRefTab()->findVftSymbolRef() &&
                    (parent->getSymbolReference()->getOffset() == comp()->getSymRefTab()->findVftSymbolRef()->getOffset()) &&
                    isPureBigDecimalMethod(grandParent, comp(), fieldInfo, isBigDecimal, isBigInteger) &&
                    (grandParent->getSecondChild() == node))
                   {
                   if ((grandParent->getReferenceCount() == 1) ||
                       isStoreToSameField(grandParent, nextTree->getNode(), node))
                      knownRead = true;
                   }
                }
             else if (node->getReferenceCount() == 1)
                {
                if (isArithmeticForSameField(nextTree->getPrevTreeTop()->getNode(), grandParent, parent, node))
                   knownRead = true;
                }
             }

           if (!isBigDecimal)
              fieldInfo->setBigDecimalType(false);
           if (!isBigInteger)
              fieldInfo->setBigIntegerType(false);

           if (!knownRead)
             {
             ////int32_t length;
             ////char *sig = getFieldSignature(comp(), sym, storedSymRef, length);
             ////fprintf(stderr, "%s is read after all\n", sig); fflush(stderr);
             ////traceMsg(comp(), "Field %s is read after all in node %p\n", sig, node);

             fieldInfo->setNotRead(false);
             }
          else
             {
             if (isBigDecimal)
                fieldInfo->setBigDecimalAssumption(true);

             if (isBigInteger)
                fieldInfo->setBigIntegerAssumption(true);
             }


          if (storedSymRef->isUnresolved() ||
              (sym->isPrivate() &&
               !sym->isFinal()))
             arrayFieldInfo = fieldInfo->asPersistentArrayFieldInfo();
          }

       if (arrayFieldInfo)
          {
          if (!parent ||
              ((!parent->isInternalPointer()) &&
              (parent->getOpCodeValue() != TR::treetop) &&
              (!parent->getOpCode().isArrayLength()) &&
              (!parent->getOpCode().isAnchor()) &&
              (parent->getOpCodeValue() != TR::ArrayStoreCHK) &&
              ((parent->getOpCodeValue() != TR::awrtbari) || (childNum != 2))))
              {
              if (_traceIt)
                  traceMsg(comp(), "2Invalidating dimension and type info for symbol %x at node %x\n", sym, node);
              arrayFieldInfo->setIsDimensionInfoValid(INVALID);
              arrayFieldInfo->setIsTypeInfoValid(INVALID);
              }
          }
       }
   }



bool TR_ClassLookahead::isProperFieldAccess(TR::Node *node)
   {
   TR::SymbolReference *storedSymRef = node->getSymbolReference();
   TR::Symbol *sym = storedSymRef->getSymbol();
   if (((sym->isShadow() &&
         node->getFirstChild()->isThisPointer()) ||
        (sym->isStatic() && sym->isFinal())) &&
       !storedSymRef->isUnresolved() &&
       (sym->isPrivate() ||
        sym->isFinal()))
      return true;

   return false;
   }


bool TR_ClassLookahead::isPrivateFieldAccess(TR::Node *node)
   {
   TR::SymbolReference *storedSymRef = node->getSymbolReference();
   TR::Symbol *sym = storedSymRef->getSymbol();
   if ((sym->isShadow() ||
        (sym->isStatic() && sym->isFinal())) &&
       !storedSymRef->isUnresolved() &&
       sym->isPrivate())
      return true;

   return false;
   }


bool TR_ClassLookahead::findMethod(List<TR::ResolvedMethodSymbol> *methodSyms, TR::ResolvedMethodSymbol *methodSym)
   {
   ListIterator<TR::ResolvedMethodSymbol> methodsIt(methodSyms);
   TR_ResolvedMethod *method = methodSym->getResolvedMethod();
   TR::ResolvedMethodSymbol *cursorMethodSym;
   for (cursorMethodSym = methodsIt.getFirst(); cursorMethodSym; cursorMethodSym = methodsIt.getNext())
      {
      if (cursorMethodSym->getResolvedMethod()->isSameMethod(method))
         return true;
      }
   return false;
   }


char *TR_ClassLookahead::getFieldSignature(TR::Compilation *comp, TR::Symbol *sym, TR::SymbolReference *symRef, int32_t &length)
   {
   if (!symRef->isUnresolved() ||
       !sym->isStatic() ||
       !sym->isConstObjectRef())
      {
      char *sig = NULL;
      if (sym->isStatic())
         sig = symRef->getOwningMethod(comp)->staticName(symRef->getCPIndex(), length, comp->trMemory());
      else if (sym->isShadow())
         sig = symRef->getOwningMethod(comp)->fieldName(symRef->getCPIndex(), length, comp->trMemory());

      return sig;
      }

   length = -1;
   return NULL;
   }


void TR_PersistentArrayFieldInfo::prepareArrayFieldInfo(int32_t numDimensions, TR::Compilation * comp)
   {
   setNumDimensions(numDimensions);
   //int32_t *newDimensionInfo = (int32_t *)jitPersistentAlloc(numDimensions*sizeof(int32_t));
   int32_t *newDimensionInfo = (int32_t *)comp->trMemory()->allocateHeapMemory(numDimensions*sizeof(int32_t));
   memset(newDimensionInfo, 0xFF, numDimensions*sizeof(int32_t));
   setDimensionInfo(newDimensionInfo);
   }

TR_PersistentArrayFieldInfo *TR_ClassLookahead::getExistingArrayFieldInfo(TR::Symbol *fieldSym, TR::SymbolReference *fieldSymRef)
   {
   TR::ClassTableCriticalSection getExistingArrayFieldInfo(comp()->fej9());

   TR_PersistentFieldInfo *newFieldInfo = _classFieldInfo->find(comp(), fieldSym, fieldSymRef);
   TR_PersistentArrayFieldInfo *newArrayFieldInfo = NULL;
   if (newFieldInfo)
      newArrayFieldInfo = newFieldInfo->asPersistentArrayFieldInfo();

   if (!newArrayFieldInfo /* &&
                             (_inFirstInitializerMethod || _inClassInitializerMethod) */)
      {
      int32_t length = 0;
      char *sig = TR_ClassLookahead::getFieldSignature(comp(), fieldSym, fieldSymRef, length);
      if (length > -1)
         {
             //char *persistentSig = (char *)TR_Memory::jitPersistentAlloc(length);
             //memcpy(persistentSig, sig, length);
           //printf("Creating persistent info for array field %s\n", sig);
         //newArrayFieldInfo = new (PERSISTENT_NEW) TR_PersistentArrayFieldInfo(persistentSig, length);
         newArrayFieldInfo = new (comp()->trHeapMemory()) TR_PersistentArrayFieldInfo(sig, length);
         if (newFieldInfo)
            {
            if (newFieldInfo->canChangeToArray())
               {
               newArrayFieldInfo->copyData(newFieldInfo);
               _classFieldInfo->remove(newFieldInfo);
               }
            else
               {
               traceMsg(comp(), "fieldInfo %p exists already for array field %s, so cannot morph\n", newFieldInfo, sig);
               newArrayFieldInfo = NULL;
               newFieldInfo->setIsTypeInfoValid(INVALID);
               }
            }

         if (newArrayFieldInfo)
            _classFieldInfo->add(newArrayFieldInfo);
         }
      }

   return newArrayFieldInfo;
   }


TR_PersistentFieldInfo *TR_ClassLookahead::getExistingFieldInfo(TR::Symbol *fieldSym, TR::SymbolReference *fieldSymRef, bool canMorph)
   {
   TR::ClassTableCriticalSection getExistingFieldInfo(comp()->fej9());

   TR_PersistentFieldInfo *newFieldInfo = _classFieldInfo->find(comp(), fieldSym, fieldSymRef);
   if (!newFieldInfo /* &&
                        (_inFirstInitializerMethod || _inClassInitializerMethod) */)
      {
      int32_t length = 0;
      char *sig = TR_ClassLookahead::getFieldSignature(comp(), fieldSym, fieldSymRef, length);
      if (length > -1)
         {
         //char *persistentSig = (char *)TR_Memory::jitPersistentAlloc(length);
         //memcpy(persistentSig, sig, length);
           //printf("Creating persistent info for field %s\n", sig);
         //newFieldInfo = new (PERSISTENT_NEW) TR_PersistentFieldInfo(persistentSig, length);
         newFieldInfo = new (comp()->trHeapMemory()) TR_PersistentFieldInfo(sig, length);
         if (!canMorph)
            newFieldInfo->setCanChangeToArray(false);
         _classFieldInfo->add(newFieldInfo);
         }
      }

   return newFieldInfo;
   }


void TR_ClassLookahead::makeInfoPersistent()
   {
   TR::ClassTableCriticalSection makeInfoPersistent(comp()->fej9());

   TR_PersistentFieldInfo *fieldInfo = _classFieldInfo->getFirst();
   TR_PersistentFieldInfo *prevInfo = NULL;

   while (fieldInfo)
      {
      bool isTypeInfoValid = fieldInfo->isTypeInfoValid();
      bool isDimensionInfoValid = false;
      TR_PersistentArrayFieldInfo *arrayFieldInfo = fieldInfo->asPersistentArrayFieldInfo();
      if (arrayFieldInfo &&
          arrayFieldInfo->isDimensionInfoValid())
          isDimensionInfoValid = true;
      bool isImmutable = fieldInfo->isImmutable();
      bool isNotRead = fieldInfo->isNotRead();
      bool isBigDecimalType = fieldInfo->isBigDecimalType();
      bool isBigIntegerType = fieldInfo->isBigIntegerType();
      bool hasBigDecimalAssumption = fieldInfo->hasBigDecimalAssumption();
      bool hasBigIntegerAssumption = fieldInfo->hasBigIntegerAssumption();

      TR_PersistentFieldInfo *newFieldInfo = NULL;
      if (isTypeInfoValid || isDimensionInfoValid || isImmutable || (isNotRead && ((!hasBigDecimalAssumption || isBigDecimalType) && (!hasBigIntegerAssumption || isBigIntegerType))))
         {
         //if (isImmutable &&
         //    (!isTypeInfoValid && !isDimensionInfoValid))
         //   {
         //   printf("Type info and dimension info are invalid but field %s is immutable \n", fieldInfo->getFieldSignature());
         //   fflush(stdout);
         //   }

         int32_t length = fieldInfo->getFieldSignatureLength();
         char *persistentSig = (char *)jitPersistentAlloc(length);
         memcpy(persistentSig, fieldInfo->getFieldSignature(), length);
         if (arrayFieldInfo)
            {
            if (_traceIt)
               printf("Creating persistent info for array field %s\n", persistentSig);
            newFieldInfo = new (PERSISTENT_NEW) TR_PersistentArrayFieldInfo(persistentSig, length);
            memcpy(newFieldInfo, fieldInfo, sizeof(TR_PersistentArrayFieldInfo));
            }
         else
            {
            if (_traceIt)
               printf("Creating persistent info for field %s\n", persistentSig);
            newFieldInfo = new (PERSISTENT_NEW) TR_PersistentFieldInfo(persistentSig, length);
            memcpy(newFieldInfo, fieldInfo, sizeof(TR_PersistentFieldInfo));
            }

         newFieldInfo->setFieldSignature(persistentSig);

         char *persistentType = 0;
         if (isTypeInfoValid)
            {
            length = fieldInfo->getNumChars();
            persistentType = (char *)jitPersistentAlloc(length);
            memcpy(persistentType, fieldInfo->getClassPointer(), length);
            }
         newFieldInfo->setClassPointer(persistentType);

         if (arrayFieldInfo)
            {
            TR_PersistentArrayFieldInfo *newArrayFieldInfo = newFieldInfo->asPersistentArrayFieldInfo();
            int32_t numDimensions = arrayFieldInfo->getNumDimensions();
            if (arrayFieldInfo->getDimensionInfo())
               {
               int32_t *newDimensionInfo = (int32_t *)jitPersistentAlloc(numDimensions*sizeof(int32_t));
               memcpy(newDimensionInfo, arrayFieldInfo->getDimensionInfo(), numDimensions*sizeof(int32_t));
               newArrayFieldInfo->setDimensionInfo(newDimensionInfo);
               }
            }

         if (prevInfo)
            prevInfo->setNext(newFieldInfo);
         else
            _classFieldInfo->setFirst(newFieldInfo);
         prevInfo = newFieldInfo;
         }
      else
         {
         if (prevInfo)
            prevInfo->setNext(fieldInfo->getNext());
         else
            _classFieldInfo->setFirst(fieldInfo->getNext());
         }

      fieldInfo = fieldInfo->getNext();
      }
   }


TR_PersistentFieldInfo *TR_PersistentClassInfoForFields::find(TR::Compilation *comp, TR::Symbol *fieldSym, TR::SymbolReference *fieldSymRef)
   {
   int32_t length = 0;
   char *sig = TR_ClassLookahead::getFieldSignature(comp, fieldSym, fieldSymRef, length);

   TR::ClassTableCriticalSection findFields(comp->fej9());
   for (TR_PersistentFieldInfo *fieldInfo = getFirst(); fieldInfo; fieldInfo = fieldInfo->getNext())
      {
      // Revisit
      //

      if ((length == fieldInfo->getFieldSignatureLength()) &&
          (memcmp(sig, fieldInfo->getFieldSignature(), length) == 0))
         {
         return fieldInfo;
         }

      //if ((fieldInfo->getFieldSymbol() == fieldSym) ||
      //          (fieldInfo->getFieldSymRef() == fieldSymRef))
      //         return fieldInfo;

      /*
      else if (fieldSym->isShadow())
         {
         if (fieldsAreSame(fieldInfo->getOwningMethodSymbol()->getResolvedMethod(), fieldInfo->getFieldSymRef()->getCPIndex(), fieldSymRef->getOwningMethod(), fieldSymRef->getCPIndex()))
            return fieldInfo->asPersistentArrayFieldInfo();
         }
      else if (fieldSym->isStatic())
         {
         if (staticsAreSame(fieldInfo->getOwningMethodSymbol()->getResolvedMethod(), fieldInfo->getFieldSymRef()->getCPIndex(), fieldSymRef->getOwningMethod(), fieldSymRef->getCPIndex()))
            return fieldInfo->asPersistentArrayFieldInfo();
         }
      */
      }

   return 0;
   }

TR_PersistentFieldInfo *
TR_PersistentClassInfoForFields::findFieldInfo(TR::Compilation *comp, TR::Node * & node, bool canBeArrayShadow)
   {
   if (!getFirst() || !node->getOpCode().hasSymbolReference())
      return 0;

   TR::SymbolReference * symRef = node->getSymbolReference();
   if (symRef->isUnresolved())
      return 0;

   TR::Symbol * sym = symRef->getSymbol();
   if (!sym->isPrivate() &&
       !sym->isFinal())
      return 0;

   if (sym->isArrayShadowSymbol())
      {
      if (!canBeArrayShadow)
         return 0;
      TR::Node *firstChild = node->getFirstChild();
      if (firstChild->getNumChildren() > 0)
         firstChild = firstChild->getFirstChild();
      if (firstChild->getOpCode().hasSymbolReference())
         node = firstChild;
      }

   if (sym->isStatic() ||
       (sym->isShadow() && node->getNumChildren() > 0 && node->getFirstChild()->isThisPointer()))
      return find(comp, sym, symRef);

   return 0;
   }

void TR_ClassLookahead::initializeFieldInfo()
   {
   TR::ClassTableCriticalSection initializeFieldInfo(comp()->fej9());

   TR_PersistentFieldInfo *fieldInfo = _classFieldInfo->getFirst();
   while (fieldInfo)
      {
      TR_PersistentArrayFieldInfo *arrayFieldInfo = NULL;
      arrayFieldInfo = fieldInfo->asPersistentArrayFieldInfo();

      if (arrayFieldInfo)
         {
         if (arrayFieldInfo->isDimensionInfoValid() == VALID_AND_ALWAYS_INITIALIZED)
            arrayFieldInfo->setIsDimensionInfoValid(VALID_BUT_NOT_ALWAYS_INITIALIZED);
         }

      if (fieldInfo->isTypeInfoValid() == VALID_AND_ALWAYS_INITIALIZED)
         fieldInfo->setIsTypeInfoValid(VALID_BUT_NOT_ALWAYS_INITIALIZED);
      fieldInfo = fieldInfo->getNext();
      }
   }



void TR_ClassLookahead::updateFieldInfo()
   {
   TR::ClassTableCriticalSection updateFieldInfo(comp()->fej9());

   TR_PersistentFieldInfo *fieldInfo = _classFieldInfo->getFirst();
   while (fieldInfo)
      {
      TR_PersistentArrayFieldInfo *arrayFieldInfo = NULL;
      arrayFieldInfo = fieldInfo->asPersistentArrayFieldInfo();

      if (arrayFieldInfo)
         {
         if (arrayFieldInfo->isDimensionInfoValid() < VALID_AND_ALWAYS_INITIALIZED)
            arrayFieldInfo->setIsDimensionInfoValid(INVALID);
         }

      if (fieldInfo->isTypeInfoValid() < VALID_AND_ALWAYS_INITIALIZED)
         {
         fieldInfo->setIsTypeInfoValid(INVALID);
         if (!arrayFieldInfo)
            fieldInfo->setCanChangeToArray(false);
         }

      fieldInfo = fieldInfo->getNext();
      }
   }


#ifdef DEBUG
void TR_PersistentFieldInfo::dumpInfo(TR_FrontEnd *fe, TR::FILE *logFile)
   {
   trfprintf(logFile, "\n\nPersistentFieldInfo for (%s) : (next : %x)\n", _signature, getNext());

   if (_isTypeInfoValid)
      {
      trfprintf(logFile, "Type info : %s\n", *_classPointer);
      }
   else
      trfprintf(logFile, "Type info INVALID\n");


   if (getNext())
       getNext()->dumpInfo(fe, logFile);
   }

void TR_PersistentArrayFieldInfo::dumpInfo(TR_FrontEnd *fe, TR::FILE *logFile)
   {
   TR_PersistentFieldInfo::dumpInfo(fe, logFile);

   if (_isDimensionInfoValid)
      {
      trfprintf(logFile, "numDimensions : %d\n", _numDimensions);
      int32_t i;
      for (i=0;i<_numDimensions;i++)
         trfprintf(logFile, "_dimensionInfo[%d] : %d\n", i, _dimensionInfo[i]);
      }
   else
      trfprintf(logFile, "Dimension info INVALID\n");
   }

#endif

