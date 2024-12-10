/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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

#include <algorithm>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "compile/ResolvedMethod.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TypeLayout.hpp"
#include "env/VerboseLog.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "optimizer/VectorAPIExpansion.hpp"
#include "optimizer/TransformUtil.hpp"


const char *
TR_VectorAPIExpansion::optDetailString() const throw()
   {
   return "O^O VECTOR API EXPANSION: ";
   }

#define OPT_DETAILS_VECTOR "O^O VECTOR API: "


int32_t
TR_VectorAPIExpansion::perform()
   {
   bool disableVectorAPIExpansion = comp()->getOption(TR_DisableVectorAPIExpansion);
   bool traceVectorAPIExpansion = comp()->getOption(TR_TraceVectorAPIExpansion);
   _boxingAllowed = comp()->getOption(TR_EnableVectorAPIBoxing);

   _trace = traceVectorAPIExpansion;

   if (J2SE_VERSION(TR::Compiler->javaVM) >= J2SE_V17 &&
       !disableVectorAPIExpansion &&
       !TR::Compiler->om.canGenerateArraylets() &&
       findVectorMethods(comp()))
      expandVectorAPI();

   return 0;
   }

bool
TR_VectorAPIExpansion::isVectorAPIMethod(TR::MethodSymbol * methodSymbol)
   {
   TR::RecognizedMethod index = methodSymbol->getRecognizedMethod();

   return (index >= _firstMethod &&
           index <= _lastMethod);
   }

TR_VectorAPIExpansion::vapiObjType
TR_VectorAPIExpansion::getReturnType(TR::MethodSymbol * methodSymbol)
   {
   if (!isVectorAPIMethod(methodSymbol)) return Unknown;

   TR::RecognizedMethod index = methodSymbol->getRecognizedMethod();

   return methodTable[index - _firstMethod]._returnType;
   }

TR_VectorAPIExpansion::vapiObjType
TR_VectorAPIExpansion::getArgumentType(TR::MethodSymbol * methodSymbol, int32_t i)
   {
   TR_ASSERT_FATAL(i < _maxNumberArguments, "Wrong argument index");

   if (!isVectorAPIMethod(methodSymbol)) return Unknown;

   TR::RecognizedMethod index = methodSymbol->getRecognizedMethod();

   return methodTable[index - _firstMethod]._argumentTypes[i];
   }

int32_t
TR_VectorAPIExpansion::getElementTypeIndex(TR::MethodSymbol *methodSymbol)
   {
   TR_ASSERT_FATAL(isVectorAPIMethod(methodSymbol), "getElementTypeIndex should be called on VectorAPI method");

   TR::RecognizedMethod index = methodSymbol->getRecognizedMethod();

   return methodTable[index - _firstMethod]._elementTypeIndex;
   }

int32_t
TR_VectorAPIExpansion::getNumLanesIndex(TR::MethodSymbol *methodSymbol)
   {
   TR_ASSERT_FATAL(isVectorAPIMethod(methodSymbol), "getNumLanesIndex should be called on VectorAPI method");

   TR::RecognizedMethod index = methodSymbol->getRecognizedMethod();

   return methodTable[index - _firstMethod]._numLanesIndex;
   }

int32_t
TR_VectorAPIExpansion::getFirstOperandIndex(TR::MethodSymbol *methodSymbol)
   {
   TR_ASSERT_FATAL(isVectorAPIMethod(methodSymbol), "getFirstOperandIndex should be called on VectorAPI method");

   TR::RecognizedMethod index = methodSymbol->getRecognizedMethod();

   return methodTable[index - _firstMethod]._firstOperandIndex;
   }

int32_t
TR_VectorAPIExpansion::getNumOperands(TR::MethodSymbol *methodSymbol)
   {
   TR_ASSERT_FATAL(isVectorAPIMethod(methodSymbol), "getNumOperands should be called on VectorAPI method");

   TR::RecognizedMethod index = methodSymbol->getRecognizedMethod();

   return methodTable[index - _firstMethod]._numOperands;
   }

int32_t
TR_VectorAPIExpansion::getMaskIndex(TR::MethodSymbol *methodSymbol)
   {
   TR_ASSERT_FATAL(isVectorAPIMethod(methodSymbol), "getMaskIndex should be called on VectorAPI method");

   TR::RecognizedMethod index = methodSymbol->getRecognizedMethod();

   return methodTable[index - _firstMethod]._maskIndex;
   }

void
TR_VectorAPIExpansion::getElementTypeAndNumLanes(TR::Node *node, TR::DataType &elementType, int32_t &numLanes)
   {
   TR_ASSERT_FATAL(node->getOpCode().isFunctionCall(), "getElementTypeAndVectorLength can only be called on a call node");

   TR::MethodSymbol *methodSymbol = node->getSymbolReference()->getSymbol()->castToMethodSymbol();

   int32_t i = getElementTypeIndex(methodSymbol);
   TR::Node *elementTypeNode = node->getChild(i);
   elementType = getDataTypeFromClassNode(comp(), elementTypeNode);

   i = getNumLanesIndex(methodSymbol);
   TR::Node *numLanesNode = node->getChild(i);
   numLanes = numLanesNode->get32bitIntegralValue();
   }

void
TR_VectorAPIExpansion::invalidateSymRef(TR::SymbolReference *symRef)
   {
   int32_t id = symRef->getReferenceNumber();
   _aliasTable[id]._classId = -1;
   _aliasTable[id]._tempClassId = -1;
   }

void
TR_VectorAPIExpansion::alias(TR::Node *node1, TR::Node *node2, bool aliasTemps)
   {
   TR_ASSERT_FATAL(node1->getOpCode().hasSymbolReference() && node2->getOpCode().hasSymbolReference(),
                   "%s nodes should have symbol references %p %p", OPT_DETAILS_VECTOR, node1, node2);

   int32_t id1 = node1->getSymbolReference()->getReferenceNumber();
   int32_t id2 = node2->getSymbolReference()->getReferenceNumber();

   // TODO: box here
   if (id1 == TR_prepareForOSR || id2 == TR_prepareForOSR)
      return;

   int32_t symRefCount = comp()->getSymRefTab()->getNumSymRefs();

   if (_aliasTable[id1]._aliases == NULL)
      _aliasTable[id1]._aliases = new (comp()->trStackMemory()) TR_BitVector(symRefCount, comp()->trMemory(), stackAlloc);

   if (_aliasTable[id2]._aliases == NULL)
      _aliasTable[id2]._aliases = new (comp()->trStackMemory()) TR_BitVector(symRefCount, comp()->trMemory(), stackAlloc);

   if (_trace)
      traceMsg(comp(), "%s aliasing symref #%d to symref #%d (nodes %p %p) for the whole class\n", OPT_DETAILS_VECTOR, id1, id2, node1, node2);

   _aliasTable[id1]._aliases->set(id2);
   _aliasTable[id2]._aliases->set(id1);

   if (aliasTemps)
      {
      if (_aliasTable[id1]._tempAliases == NULL)
         _aliasTable[id1]._tempAliases = new (comp()->trStackMemory()) TR_BitVector(symRefCount, comp()->trMemory(), stackAlloc);

      if (_aliasTable[id2]._tempAliases == NULL)
         _aliasTable[id2]._tempAliases = new (comp()->trStackMemory()) TR_BitVector(symRefCount, comp()->trMemory(), stackAlloc);

      if (_trace)
         traceMsg(comp(), "%s aliasing symref #%d to symref #%d (nodes %p %p) as temps\n", OPT_DETAILS_VECTOR, id1, id2, node1, node2);

      _aliasTable[id1]._tempAliases->set(id2);
      _aliasTable[id2]._tempAliases->set(id1);
      }
   }


bool
TR_VectorAPIExpansion::treeTopAllowedWithBoxing(TR::ILOpCodes opCodeValue)
   {
   return (opCodeValue == TR::ResolveCHK ||
           opCodeValue == TR::ResolveAndNULLCHK);
   }

void
TR_VectorAPIExpansion::buildVectorAliases(bool verifyMode)
   {
   if (_trace)
      traceMsg(comp(), "%s Aliasing symrefs verifyMode=%\n", OPT_DETAILS_VECTOR, verifyMode);

   _visitedNodes.empty();

   for (TR::TreeTop *tt = comp()->getMethodSymbol()->getFirstTreeTop(); tt ; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      TR::ILOpCodes opCodeValue = node->getOpCodeValue();

      if (opCodeValue == TR::treetop || opCodeValue == TR::NULLCHK ||
          (boxingAllowed() && treeTopAllowedWithBoxing(opCodeValue)))
          {
          node = node->getFirstChild();
          }

      visitNodeToBuildVectorAliases(node, verifyMode);
      }
   }


void
TR_VectorAPIExpansion::visitNodeToBuildVectorAliases(TR::Node *node, bool verifyMode)
   {
   if (_visitedNodes.isSet(node->getGlobalIndex()))
      return;
   _visitedNodes.set(node->getGlobalIndex());

   TR::ILOpCode opCode = node->getOpCode();
   TR::ILOpCodes opCodeValue = node->getOpCodeValue();

   if ((opCodeValue == TR::astore || opCodeValue == TR::astorei) &&
       !node->chkStoredValueIsIrrelevant())
      {
      int32_t id1 = node->getSymbolReference()->getReferenceNumber();
      TR::Node *rhs = (opCodeValue == TR::astore) ? node->getFirstChild() : node->getSecondChild();

      TR_ASSERT_FATAL(rhs->getDataType() == TR::Address, "Child %p of node %p should have address type", rhs, node);
      if (verifyMode) return;

      if (rhs->getOpCode().hasSymbolReference())
         {
         int32_t id2 = rhs->getSymbolReference()->getReferenceNumber();

         bool aliasTemps = false;

         if (opCodeValue == TR::astore &&
             rhs->getOpCode().isFunctionCall() &&
             isVectorAPIMethod(rhs->getSymbolReference()->getSymbol()->castToMethodSymbol()))
            {
            // propagate vector info from VectorAPI call to temp
            TR::DataType elementType;
            int32_t numLanes;

            getElementTypeAndNumLanes(rhs, elementType, numLanes);

            vapiObjType objectType = getReturnType(rhs->getSymbolReference()->getSymbol()->castToMethodSymbol());

            if (objectType == Mask &&
                (elementType == TR::Float || elementType == TR::Double))
               elementType = (elementType == TR::Float) ? TR::Int32 : TR::Int64;

            int32_t elementSize = OMR::DataType::getSize(elementType);
            int32_t bitsLength = numLanes*elementSize*8;


            if ((_aliasTable[id1]._elementType != TR::NoType && _aliasTable[id1]._elementType != elementType) ||
                (_aliasTable[id1]._vecLen != vec_len_default && _aliasTable[id1]._vecLen != bitsLength))
               {
               if (boxingAllowed())
                  {
                  dontVectorizeNode(node);
                  }
               else
                  {
                  if (_trace)
                     traceMsg(comp(), "Invalidating1 #%d due to rhs %p in node %p\n", id1, rhs, node);
                  invalidateSymRef(node->getSymbolReference());
                  }

               }
            else
               {
               _aliasTable[id1]._elementType = elementType;
               _aliasTable[id1]._vecLen = bitsLength;
               _aliasTable[id1]._objectType = objectType;

               if (boxingAllowed() &&
                   !_nodeTable[rhs->getGlobalIndex()]._canVectorize)
                  dontVectorizeNode(node);
               }
            }
         else if (boxingAllowed() &&
                  opCodeValue == TR::astore &&
                  rhs->getOpCodeValue() != TR::aload)
            {
            _aliasTable[id1]._elementType = TR::Address;
            dontVectorizeNode(node);

            if (_trace)
               traceMsg(comp(), "Making #%d a box of unknown type due to node %p\n", id1, node);
            }

         if (opCodeValue == TR::astore && rhs->getOpCodeValue() == TR::aload)
            aliasTemps = true;

         alias(node, rhs, aliasTemps);

         if (_aliasTable[id1]._objectType == Unknown &&
             _aliasTable[id2]._objectType == Unknown)
            {
            _aliasTable[id1]._objectType = Invalid;
            _aliasTable[id2]._objectType = Invalid;
            }
         else if (_aliasTable[id1]._objectType == Unknown)
            {
            _aliasTable[id1]._objectType = _aliasTable[id2]._objectType;
            }
         else if (_aliasTable[id2]._objectType == Unknown)
            {
            _aliasTable[id2]._objectType = _aliasTable[id1]._objectType;
            }
         else if (_aliasTable[id1]._objectType != _aliasTable[id2]._objectType)
            {
            _aliasTable[id1]._objectType = Invalid;
            _aliasTable[id2]._objectType = Invalid;
            }
         }
      else
         {
         if (boxingAllowed())
            {
            dontVectorizeNode(node);
            }
         else
            {
            if (_trace)
               traceMsg(comp(), "Invalidating2 #%d due to rhs %p in node %p\n", id1, rhs, node);
            invalidateSymRef(node->getSymbolReference());
            }
         }
      }
   else if (opCode.isFunctionCall())
      {
      TR::MethodSymbol * methodSymbol = node->getSymbolReference()->getSymbol()->castToMethodSymbol();
      TR::DataType methodElementType = TR::NoType;
      int32_t methodNumLanes = 0;
      int32_t methodRefNum = node->getSymbolReference()->getReferenceNumber();
      int32_t numChildren = node->getNumChildren();
      bool isVectorAPICall = isVectorAPIMethod(methodSymbol);
      ncount_t nodeIndex = node->getGlobalIndex();

      _aliasTable[methodRefNum]._objectType = getReturnType(methodSymbol);
      _nodeTable[nodeIndex]._objectType = getReturnType(methodSymbol);

      // Find return object type
      if (_aliasTable[methodRefNum]._objectType == Unknown &&
          isVectorAPICall)
         {
         if (methodSymbol->getRecognizedMethod() == TR::jdk_internal_vm_vector_VectorSupport_load ||
             methodSymbol->getRecognizedMethod() == TR::jdk_internal_vm_vector_VectorSupport_fromBitsCoerced)
            {
            _aliasTable[methodRefNum]._objectType = getObjectTypeFromClassNode(comp(), node->getFirstChild());
            _nodeTable[nodeIndex]._objectType = getObjectTypeFromClassNode(comp(), node->getFirstChild());
            }
         if (methodSymbol->getRecognizedMethod() == TR::jdk_internal_vm_vector_VectorSupport_binaryOp) // can be used for Mask objects too
            {
            _aliasTable[methodRefNum]._objectType = getObjectTypeFromClassNode(comp(), node->getSecondChild());
            _nodeTable[nodeIndex]._objectType = getObjectTypeFromClassNode(comp(), node->getSecondChild());
            }
         if (methodSymbol->getRecognizedMethod() == TR::jdk_internal_vm_vector_VectorSupport_compressExpandOp)
            {
            if (node->getFirstChild()->getOpCode().isLoadConst() &&
                node->getFirstChild()->get32bitIntegralValue() == VECTOR_OP_MASK_COMPRESS)
               {
               _aliasTable[methodRefNum]._objectType = Mask;
               _nodeTable[nodeIndex]._objectType = Mask;
               }
            else
               {
               _aliasTable[methodRefNum]._objectType = Vector;
               _nodeTable[nodeIndex]._objectType = Vector;
               }
            }
         }

      for (int32_t i = 0; i < numChildren; i++)
         {
         bool isMask = false;

         if (!isVectorAPICall ||
             (i >= getFirstOperandIndex(methodSymbol) && i < (getFirstOperandIndex(methodSymbol) + getNumOperands(methodSymbol))) ||
             (isMask = (i == getMaskIndex(methodSymbol))))
            {
            TR::Node *child = node->getChild(i);
            bool hasSymbolReference = child->getOpCode().hasSymbolReference();
            bool isNullMask = isMask && child->isConstZeroValue();

            bool nullVectorInMaskCompress = false;

            if (methodSymbol->getRecognizedMethod() == TR::jdk_internal_vm_vector_VectorSupport_compressExpandOp &&
                node->getFirstChild()->getOpCode().isLoadConst() &&
                node->getFirstChild()->get32bitIntegralValue() == VECTOR_OP_MASK_COMPRESS)
               nullVectorInMaskCompress = true;

            bool constOperandOfBroadcastInt = false;

            if (methodSymbol->getRecognizedMethod() == TR::jdk_internal_vm_vector_VectorSupport_broadcastInt &&
                i == (getFirstOperandIndex(methodSymbol) + 1))
                constOperandOfBroadcastInt = true;

            if (hasSymbolReference &&
                child->getDataType() == TR::Address &&
                (!boxingAllowed() || isVectorAPICall))
               {
               alias(node, child);
               }

            if (!hasSymbolReference &&
                !isNullMask &&
                !nullVectorInMaskCompress &&
                !constOperandOfBroadcastInt)
               {
               if (boxingAllowed())
                  {
                  dontVectorizeNode(node);
                  }
               else
                  {
                  if (_trace)
                     traceMsg(comp(), "Invalidating3 #%d due to child %d (%p) in node %p\n",
                              node->getSymbolReference()->getReferenceNumber(), i, child, node);
                  invalidateSymRef(node->getSymbolReference());
                  }
               }
            }

         if (!isVectorAPICall)
            {
            if (boxingAllowed())
               {
               dontVectorizeNode(node);
               continue;
               }

            if (_trace)
               traceMsg(comp(), "Invalidating4 #%d since it's not a vector API method in node %p\n",
                     node->getSymbolReference()->getReferenceNumber(), node);
            invalidateSymRef(node->getSymbolReference());
            continue;
            }

         // Update method element type and vector length
         if (i == getElementTypeIndex(methodSymbol))
            {
            TR::Node *elementTypeNode = node->getChild(i);
            methodElementType = getDataTypeFromClassNode(comp(), elementTypeNode);
            _aliasTable[methodRefNum]._elementType = methodElementType;
            _nodeTable[nodeIndex]._elementType = methodElementType;
            }
         else if (i == getNumLanesIndex(methodSymbol))
            {
            TR::Node *numLanesNode = node->getChild(i);

            _aliasTable[methodRefNum]._vecLen = vec_len_unknown;
            _nodeTable[nodeIndex]._vecLen = vec_len_unknown;

            if (numLanesNode->getOpCode().isLoadConst())
               {
               methodNumLanes = numLanesNode->get32bitIntegralValue();
               if (methodElementType != TR::NoType)  // type was seen but could've been non-const
                  {
                  int32_t elementSize = OMR::DataType::getSize(methodElementType);
                  _aliasTable[methodRefNum]._vecLen = methodNumLanes*8*elementSize;
                  _nodeTable[nodeIndex]._vecLen = methodNumLanes*8*elementSize;
                  }
               }
            }
         }


      // check if VectorAPI method is supported
      TR::RecognizedMethod index = methodSymbol->getRecognizedMethod();
      int32_t handlerIndex = index - _firstMethod;

      if (methodElementType == TR::NoType ||
          methodNumLanes == 0)
         {
         if (boxingAllowed())
            {
            dontVectorizeNode(node);
            }
         else
            {
            if (_trace)
               traceMsg(comp(), "Invalidating5 #%d (isVectorAPICall=%d) due to unknown elementType=%d or numLanes=%d in node %p\n",
                        node->getSymbolReference()->getReferenceNumber(), isVectorAPICall, (int)methodElementType, methodNumLanes, node);

            invalidateSymRef(node->getSymbolReference());
            }
         }
      else
         {
         int32_t elementSize = OMR::DataType::getSize(methodElementType);
         vec_sz_t bitsLength = methodNumLanes*8*elementSize;
         TR::VectorLength vectorLength = OMR::DataType::bitsToVectorLength(bitsLength);
         bool canVectorize = false;

         if (supportedOnPlatform(comp(), bitsLength) != TR::NoVectorLength)
            {
            canVectorize = methodTable[handlerIndex]._methodHandler(this, NULL, node, methodElementType, vectorLength, methodNumLanes,
                                                                    checkVectorization);
            }

         bool canScalarize = methodTable[handlerIndex]._methodHandler(this, NULL, node, methodElementType, vectorLength, methodNumLanes,
                                                                      checkScalarization);

         if (boxingAllowed())
            canScalarize = false; // TODO: enable

         _nodeTable[nodeIndex]._canVectorize = canVectorize;
         _nodeTable[nodeIndex]._canScalarize = canScalarize;

         if (!canVectorize)
            {
            if (_trace)
               traceMsg(comp(), "Can't vectorize #%d due to unsupported opcode in node %p\n",
                                 node->getSymbolReference()->getReferenceNumber(), node);
            _aliasTable[methodRefNum]._cantVectorize = true;

            if (!canScalarize)
               {
               _aliasTable[methodRefNum]._cantScalarize = true;

               if (boxingAllowed())
                  {
                  dontVectorizeNode(node);
                  }
               else
                  {
                  if (_trace)
                     traceMsg(comp(), "Invalidating6 #%d due to unsupported opcode in node %p\n",
                              node->getSymbolReference()->getReferenceNumber(), node);
                  invalidateSymRef(node->getSymbolReference());
                  }
               }
            }
         else if (!canScalarize)
            {
            if (_trace)
               traceMsg(comp(), "Can't scalarize #%d due to unsupported opcode in node %p\n",
                                 node->getSymbolReference()->getReferenceNumber(), node);
            _aliasTable[methodRefNum]._cantScalarize = true;
            }

         if (boxingAllowed())
            {
            createClassesForBoxing(node->getSymbolReference()->getOwningMethod(comp()), methodElementType, bitsLength);
            }
         }
      }
   else if (opCode.isLoadAddr())
      {
      if (boxingAllowed())
         {
         dontVectorizeNode(node);
         }
      else
         {
         if (_trace)
            traceMsg(comp(), "Invalidating7 #%d due to its adress used by loadaddr node %p\n", node->getSymbolReference()->getReferenceNumber(), node);
         invalidateSymRef(node->getSymbolReference());
         }
      }
   else if (opCode.isArrayRef() ||
            opCode.isLoadIndirect())
      {
      TR::Node *child = node->getFirstChild();
      if (child->getOpCode().hasSymbolReference())
         {
         if (boxingAllowed())
            {
            // make it boxed since, currently, transformation pass is not recursive
            // and we will not detect if boxing is necessary deeper in the trees
            dontVectorizeNode(child);
            }
         else
            {
            if (_trace)
               traceMsg(comp(), "Invalidating8 #%d due to its address used by %p\n",
                                 child->getSymbolReference()->getReferenceNumber(), node);
               invalidateSymRef(child->getSymbolReference());
            }
         }
      }
   else if (opCode.isStoreIndirect() ||
            node->getOpCodeValue() == TR::areturn ||
            node->getOpCodeValue() == TR::aRegStore)
      {
      TR::Node *child = node->getFirstChild();
      if (child->getOpCode().hasSymbolReference())
         {
         if (!boxingAllowed())
            {
            if (_trace)
               traceMsg(comp(), "Invalidating9 #%d due to its address used by %p\n",
                                 child->getSymbolReference()->getReferenceNumber(), node);
            invalidateSymRef(child->getSymbolReference());
            }
         }
      }
   else if (boxingAllowed() &&
            (node->getOpCodeValue() == TR::checkcast ||
             node->getOpCodeValue() == TR::athrow))
      {
      // do nothing here to allow this treetop when boxing is enabled
      }
   else
      {
      for (int32_t i = 0; i < node->getNumChildren(); i++)
         {
         TR::Node *child = node->getChild(i);
         if (child->getOpCode().hasSymbolReference() &&
             child->getDataType() == TR::Address)
            {
            bool scalarResult = false;
            if (child->getOpCode().isFunctionCall())
               {
               TR::MethodSymbol * methodSymbol = child->getSymbolReference()->getSymbol()->castToMethodSymbol();
               if (getReturnType(methodSymbol) ==  Scalar) continue; // OK to use by any other parent node
               }

            if (boxingAllowed())
               {
               if (_trace)
                  traceMsg(comp(), "Making #%d boxed since it's used by unsupported node %p (%s)\n",
                                    child->getSymbolReference()->getReferenceNumber(), node,
                                    node->getOpCode().getName());

#if 0
               if (TR::Options::getVerboseOption(TR_VerboseVectorAPI))
                  {
                  TR_VerboseLog::writeLine(TR_Vlog_VECTOR_API, "Not vectorizing node since it's used by %s",
                                        node->getOpCode().getName());
                  }
#endif
               dontVectorizeNode(child);
               }
            else
               {
               if (_trace)
                  traceMsg(comp(), "Invalidating10 #%d since it's used by unsupported node %p (%s)\n",
                                    child->getSymbolReference()->getReferenceNumber(), node,
                                    node->getOpCode().getName());
               invalidateSymRef(child->getSymbolReference());
               }
            }
         }
      }

   // skip PassTrough only if it's a child of a known parent
   if ((node->getOpCodeValue() == TR::checkcast ||
        node->getOpCodeValue() == TR::NULLCHK) &&
        node->getFirstChild()->getOpCodeValue() == TR::PassThrough)
      node = node->getFirstChild();

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      visitNodeToBuildVectorAliases(node->getChild(i), verifyMode);
      }
   }


void
TR_VectorAPIExpansion::findAllAliases(int32_t classId, int32_t id,
                                      TR_BitVector * vectorAliasTableElement::* aliasesField,
                                      int32_t vectorAliasTableElement::* classField)
   {
   bool tempAliases = &vectorAliasTableElement::_tempAliases == aliasesField;

   if (_aliasTable[id].*aliasesField == NULL)
      {
      TR_ASSERT_FATAL(_aliasTable[id].*classField <= 0 , "#%d should have class -1 or 0, but it's %d\n",
                                                              id, _aliasTable[id].*classField);

      if (_aliasTable[id].*classField == 0)
          _aliasTable[id].*classField = id;  // in their own empty class
      return;
      }

   if (_trace)
      {
      traceMsg(comp(), "Iterating through %s aliases for #%d:\n", tempAliases ? "temp" : "whole", id);
      (_aliasTable[id].*aliasesField)->print(comp());
      traceMsg(comp(), "\n");
      }

   // we need to create a new bit vector so that we don't iterate and modify at the same time
   TR_BitVector *aliasesToIterate = (classId == id) ? new (comp()->trStackMemory()) TR_BitVector(*(_aliasTable[id].*aliasesField))
                                                    : _aliasTable[id].*aliasesField;

   TR_BitVectorIterator bvi(*aliasesToIterate);

   while (bvi.hasMoreElements())
      {
      int32_t i = bvi.getNextElement();

      if (_aliasTable[i].*classField > 0)
         {
         TR_ASSERT_FATAL(_aliasTable[i].*classField == classId, "#%d should belong to class %d but it belongs to class %d\n",
                     i, classId, _aliasTable[i].*classField );
         continue;
         }
      (_aliasTable[classId].*aliasesField)->set(i);

      if (_aliasTable[i].*classField == -1)
         {
         if (boxingAllowed())
            {
            _aliasTable[classId]._vecLen = vec_len_boxed_unknown;
            }
         else
            {
            if (_trace)
               traceMsg(comp(), "Invalidating11 %s class #%d since #%d is already invalid\n", tempAliases ? "temp" : "whole", classId, i);
            _aliasTable[classId].*classField = -1; // invalidate the whole class
            }

         }

      if (_aliasTable[i].*classField != -1 || i != classId)
         {
         if (_trace)
            traceMsg(comp(), "Set %s class #%d for symref #%d\n", tempAliases ? "temp" : "whole", classId, i);
         _aliasTable[i].*classField = classId;
         }

      if (i != classId)
         findAllAliases(classId, i, aliasesField, classField);
      }
   }


void
TR_VectorAPIExpansion::buildAliasClasses()
   {
   if (_trace)
      traceMsg(comp(), "%s Building alias classes\n", OPT_DETAILS_VECTOR);

   int32_t symRefCount = comp()->getSymRefTab()->getNumSymRefs();

   TR_BitVector * vectorAliasTableElement::* aliasesField = &vectorAliasTableElement::_aliases;
   int32_t vectorAliasTableElement::* classField = &vectorAliasTableElement::_classId;

   for (int32_t i = 0; i < symRefCount; i++)
      {
      if (_aliasTable[i].*classField <= 0)
         findAllAliases(i, i, aliasesField, classField);
      }

   if (_trace)
      traceMsg(comp(), "%s Building temp alias classes\n", OPT_DETAILS_VECTOR);

   aliasesField = &vectorAliasTableElement::_tempAliases;
   classField = &vectorAliasTableElement::_tempClassId;

   for (int32_t i = 0; i < symRefCount; i++)
      {
      if (_aliasTable[i].*classField <= 0)
         findAllAliases(i, i, aliasesField, classField);
      }
   }


TR_VectorAPIExpansion::vec_sz_t
TR_VectorAPIExpansion::getVectorSizeFromVectorSpecies(TR::Node *vectorSpeciesNode)
   {
   TR::SymbolReference *vSpeciesSymRef = vectorSpeciesNode->getSymbolReference();
   if (vSpeciesSymRef)
      {
      if (vSpeciesSymRef->hasKnownObjectIndex())
         {
         int32_t vectorBitSize = 0;
#if defined(J9VM_OPT_JITSERVER)
         if (comp()->isOutOfProcessCompilation()) /* In server mode */
            {
            auto stream = comp()->getStream();
            stream->write(JITServer::MessageType::KnownObjectTable_getVectorBitSize,
                          vSpeciesSymRef->getKnownObjectIndex());
            vectorBitSize = std::get<0>(stream->read<int32_t>());
            }
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());
            TR::VMAccessCriticalSection getVectorSizeFromVectorSpeciesSection(fej9);

            uintptr_t vectorSpeciesLocation =
               comp()->getKnownObjectTable()->getPointer(vSpeciesSymRef->getKnownObjectIndex());
            uintptr_t vectorShapeLocation =
               fej9->getReferenceField(vectorSpeciesLocation,
                                       "vectorShape",
                                       "Ljdk/incubator/vector/VectorShape;");
            vectorBitSize = fej9->getInt32Field(vectorShapeLocation, "vectorBitSize");
            }

         return (vec_sz_t)vectorBitSize;
         }
      }
      return vec_len_unknown;
   }


TR_OpaqueClassBlock *
TR_VectorAPIExpansion::getOpaqueClassBlockFromClassNode(TR::Compilation *comp, TR::Node *classNode)
   {
   if (!classNode->getOpCode().hasSymbolReference())
      return NULL;

   TR::SymbolReference *symRef = classNode->getSymbolReference();

   TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN;

   if (symRef && symRef->hasKnownObjectIndex())
      {
      knownObjectIndex = symRef->getKnownObjectIndex();
      }
   else if (classNode->hasKnownObjectIndex())
      {
      knownObjectIndex = classNode->getKnownObjectIndex();
      }

   if (knownObjectIndex != TR::KnownObjectTable::UNKNOWN)
      {
      TR_OpaqueClassBlock *clazz = NULL;
#if defined(J9VM_OPT_JITSERVER)
      if (comp->isOutOfProcessCompilation()) /* In server mode */
         {
         auto stream = comp->getStream();
         stream->write(JITServer::MessageType::KnownObjectTable_getOpaqueClass,
                        knownObjectIndex);

         clazz = (TR_OpaqueClassBlock *)std::get<0>(stream->read<uintptr_t>());
         }
      else
#endif /* defined(J9VM_OPT_JITSERVER) */
         {
         TR_J9VMBase *fej9 = comp->fej9();

         TR::VMAccessCriticalSection getDataTypeFromClassNodeSection(fej9);

         uintptr_t javaLangClass = comp->getKnownObjectTable()->getPointer(knownObjectIndex);
         clazz = (TR_OpaqueClassBlock *)(intptr_t)fej9->getInt64Field(javaLangClass, "vmRef");
         }

      return clazz;
      }

   return NULL;
   }

TR::DataType
TR_VectorAPIExpansion::getDataTypeFromClassNode(TR::Compilation *comp, TR::Node *classNode)
   {
   TR_OpaqueClassBlock *clazz = getOpaqueClassBlockFromClassNode(comp, classNode);

   if (!clazz) return TR::NoType;

   TR_J9VMBase *fej9 = comp->fej9();
   return fej9->getClassPrimitiveDataType(clazz);
   }

TR_VectorAPIExpansion::vapiObjType
TR_VectorAPIExpansion::getObjectTypeFromClassNode(TR::Compilation *comp, TR::Node *classNode)
   {
   TR_OpaqueClassBlock *clazz = getOpaqueClassBlockFromClassNode(comp, classNode);

   if (!clazz) return Unknown;

   J9ROMClass *romClass = TR::Compiler->cls.romClassOf(clazz);
   J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);
   int32_t length = J9UTF8_LENGTH(className);
   char *classNameChars = (char*)J9UTF8_DATA(className);

   // Currently, classNode can be one of the following types
   // jdk/incubator/vector/<species name>Vector
   // jdk/incubator/vector/<species name>Vector$<species name>Mask
   // jdk/incubator/vector/<species name>Vector$<species name>Shuffle

   if (!strncmp(classNameChars + length - 6, "Vector", 6))
      return Vector;
   else if (!strncmp(classNameChars + length - 4, "Mask", 4))
      return Mask;
   else if (!strncmp(classNameChars + length - 7, "Shuffle", 7))
      return Shuffle;

   return Unknown;
   }


bool
TR_VectorAPIExpansion::findVectorMethods(TR::Compilation *comp)
   {
   bool trace = comp->getOption(TR_TraceVectorAPIExpansion);

   if (trace)
      traceMsg(comp, "%s in findVectorMethods\n", OPT_DETAILS_VECTOR);

   for (TR::TreeTop *tt = comp->getMethodSymbol()->getFirstTreeTop(); tt ; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      TR::ILOpCodes opCodeValue = node->getOpCodeValue();

      if (opCodeValue == TR::treetop || opCodeValue == TR::NULLCHK ||
          treeTopAllowedWithBoxing(opCodeValue))
          {
          node = node->getFirstChild();
          }

      TR::ILOpCode opCode = node->getOpCode();

      if (opCode.isFunctionCall())
         {
         TR::MethodSymbol * methodSymbol = node->getSymbolReference()->getSymbol()->castToMethodSymbol();

         if (isVectorAPIMethod(methodSymbol))
            {
            if (trace)
               traceMsg(comp, "%s found Vector API method\n", OPT_DETAILS_VECTOR);
            return true;
            }
         }
      }
   return false;
   }

bool
TR_VectorAPIExpansion::validateSymRef(int32_t id, int32_t i, vec_sz_t &classLength, TR::DataType &classType,
                                      int32_t vectorAliasTableElement::* classField)
   {
   bool tempClasses = &vectorAliasTableElement::_tempClassId == classField;

   TR::SymbolReference *symRef = comp()->getSymRefTab()->getSymRef(i);

   if (!symRef || !symRef->getSymbol())
      return false;

   if (_aliasTable[i].*classField == -1)
      {
      if (_trace)
         traceMsg(comp(), "%s invalidating12 class #%d due to symref #%d\n", OPT_DETAILS_VECTOR, id, i);
      return false;
      }
   else if (symRef->getSymbol()->isShadow() ||
            symRef->getSymbol()->isStatic() ||
             symRef->getSymbol()->isParm())
      {
      if (boxingAllowed())
         {
         _aliasTable[i]._vecLen = vec_len_boxed_unknown;
         _aliasTable[id]._vecLen = vec_len_boxed_unknown;
         return true;
         }

      if (_trace)
         traceMsg(comp(), "%s invalidating13 class #%d due to symref #%d\n", OPT_DETAILS_VECTOR, id, i);
      return false;
      }
   else if (symRef->getSymbol()->isMethod())
      {
      if (!isVectorAPIMethod(symRef->getSymbol()->castToMethodSymbol()))
         {
         if (boxingAllowed())
            {
            return true;
            }
         else
            {
            if (_trace)
               traceMsg(comp(), "%s Invalidating14 class #%d due to non-API method #%d\n", OPT_DETAILS_VECTOR, id, i);
            return false;
            }
         }
      }
   else if (tempClasses)
      {
      vec_sz_t tempLength = _aliasTable[i]._vecLen;
      TR::DataType tempType = _aliasTable[i]._elementType;
      // TODO: object type?

      symRef = comp()->getSymRefTab()->getSymRef(i);

      // Check length
      if (tempLength == vec_len_boxed_unknown)
         {
         // Treat the whole class as a box of unknown length
         classLength = vec_len_boxed_unknown;

         if (_trace)
            traceMsg(comp(), "%s making temp class #%d boxed due to symref #%d\n",
                              OPT_DETAILS_VECTOR, id, i);
         }
      else if (classLength == vec_len_default)
         {
         if (_trace)
            traceMsg(comp(), "%s assigning length to temp class #%d from symref #%d of length %d\n",
                              OPT_DETAILS_VECTOR, id, i, tempLength);

         classLength = tempLength;
         }
      else if (tempLength != vec_len_default &&
               tempLength != classLength)
         {
         if (_trace)
            traceMsg(comp(), "%s invalidating15 class #%d due to symref #%d temp length %d, class length %d\n",
                               OPT_DETAILS_VECTOR, id, i, tempLength, classLength);
         return false;
         }

      // Check type
      if (tempLength == vec_len_boxed_unknown)
         {
         // Treat the whole class as a box of unknown type
         classType = TR::Address;
         }
      else if (classType == TR::NoType)
         {
         if (_trace)
            traceMsg(comp(), "%s assigning element type to temp class #%d from symref #%d of type %s\n",
                     OPT_DETAILS_VECTOR, id, i, TR::DataType::getName(tempType));

         classType = tempType;
         }
      else if (tempType != TR::NoType &&
               tempType != classType)
         {
         if (_trace)
            traceMsg(comp(), "%s invalidating16 class #%d due to symref #%d temp type %s, class type %s\n",
                     OPT_DETAILS_VECTOR, id, i, TR::DataType::getName(tempType), TR::DataType::getName(classType));
         return false;
         }
      }

      return true;
   }


void
TR_VectorAPIExpansion::validateVectorAliasClasses(TR_BitVector * vectorAliasTableElement::* aliasesField,
                                                  int32_t vectorAliasTableElement::* classField)
   {
   bool tempClasses = &vectorAliasTableElement::_tempAliases == aliasesField;

   if (_trace)
      traceMsg(comp(), "\n%s ***Verifying all %s alias classes***\n", OPT_DETAILS_VECTOR, tempClasses ? "temp" : "whole");

   int32_t symRefCount = comp()->getSymRefTab()->getNumSymRefs();

   for (int32_t id = 1; id < symRefCount; id++)
      {
      TR::SymbolReference *symRef = comp()->getSymRefTab()->getSymRef(id);

      if (tempClasses &&
          symRef &&
          symRef->getSymbol() &&
          symRef->getSymbol()->isMethod())
         continue; // classes of temps should not include methods

      if ((_aliasTable[id].*classField) == -1)
         {
         if (_trace)
            traceMsg(comp(), "%s class #%d is already invalid\n", tempClasses ? "temp" : "whole", id);
         continue;
         }

      if ((_aliasTable[id].*classField) != id)
         continue;  // not an alias class

      if (_trace)
         {
         traceMsg(comp(), "**Verifying %s class: #%d**\n", tempClasses ? "temp" : "whole", id);

         if (_aliasTable[id].*aliasesField)
            {
            (_aliasTable[id].*aliasesField)->print(comp());
            traceMsg(comp(), "\n");
            }
         }

      bool vectorClass = true;
      vec_sz_t classLength = vec_len_default;
      TR::DataType classType = TR::NoType;

      if (!(_aliasTable[id].*aliasesField))
         {
         // class might consist of just the symref itself
         vectorClass = validateSymRef(id, id, classLength, classType, classField);

         if (_trace)
            traceMsg(comp(), "   Validating #%d: %s\n", id, vectorClass ? "OK" : "X");
         }
      else
         {
         TR_BitVectorIterator bvi(*(_aliasTable[id].*aliasesField));
         while (bvi.hasMoreElements())
            {
            int32_t i = bvi.getNextElement();

            vectorClass = validateSymRef(id, i, classLength, classType, classField);

            if (_trace)
               traceMsg(comp(), "   Validating #%d: %s\n", i, vectorClass ? "OK" : "X");

            if (!vectorClass)
               {
               if (_trace)
                  traceMsg(comp(), "Class #%d can't be vectorized or scalarized due to invalid symRef #%d\n", id, i);
               break;
               }

            if (_aliasTable[i]._objectType == Invalid &&
                (!boxingAllowed() || tempClasses))
               {
               if (_trace)
                  traceMsg(comp(), "Class #%d can't be vectorized or scalarized due to invalid object type of #%d\n", id, i);

               _aliasTable[id]._cantVectorize = true;
               _aliasTable[id]._cantScalarize = true;
               vectorClass = false;
               break;
               }

            if (_aliasTable[i]._cantVectorize)
               {
               if (_trace)
                  traceMsg(comp(), "Class #%d can't be vectorized due to #%d\n", id, i);

               if (!boxingAllowed())
                  {
                  _aliasTable[id]._cantVectorize = true;

                  if (_aliasTable[id]._cantScalarize)
                     {
                     vectorClass = false;
                     break;
                     }
                  }
               }

            if (_aliasTable[i]._cantScalarize)
               {
               if (_trace)
                  traceMsg(comp(), "Class #%d can't be scalarized due to #%d\n", id, i);
               _aliasTable[id]._cantScalarize = true;

               if (_aliasTable[id]._cantVectorize)
                  {
                  if (!boxingAllowed())
                     {
                     vectorClass = false;
                     break;
                     }
                  }
               }
            }
         }

      if (vectorClass && !tempClasses)
         continue;

      // update class vector length and element type
      if (_trace)
         traceMsg(comp(), "Setting length and type for %s class #%d to %d and %s\n", tempClasses ? "temp" : "whole",
                  id, classLength, TR::DataType::getName(classType));

      _aliasTable[id]._vecLen = classLength;
      _aliasTable[id]._elementType = classType;

      if (vectorClass &&
          ((classLength != vec_len_unknown &&
            classLength != vec_len_default) ||
           classLength == vec_len_boxed_unknown))
         continue;

      // invalidate the whole class
      if (_trace && _aliasTable[id].*aliasesField)  // to reduce number of messages
         traceMsg(comp(), "Invalidating17 %s class #%d\n", tempClasses ? "temp" : "whole", id);

      _aliasTable[id].*classField = -1;

      int32_t &wholeClass = _aliasTable[id]._classId;

      if (tempClasses && wholeClass >= 0)
         {
         if (!boxingAllowed())
            {
            // invalidate the whole class that temp class belongs to
            if (_trace)
               traceMsg(comp(), "Invalidating18 whole class #%d due to temp class #%d\n", wholeClass, id);

            _aliasTable[wholeClass]._classId = -1;
            wholeClass = -1;
            }
         }
      }
   }

TR::SymbolReference *
TR_VectorAPIExpansion::createPayloadSymbolReference(TR::Compilation *comp, TR_OpaqueClassBlock *vecClass)
   {
   const TR::TypeLayout *layout = comp->typeLayout(vecClass);
   const TR::TypeLayoutEntry *field = NULL;
   size_t i = 0;

   for (; i < layout->count(); i++)
       {
       field = &layout->entry(i);
       if (strcmp("payload", field->_fieldname) == 0)
          break;
       }
   TR_ASSERT_FATAL(i < layout->count(), "Should've found payload field in the VectorPayload class");

   return comp->getSymRefTab()->findOrFabricateShadowSymbol(vecClass, field->_datatype, field->_offset, field->_isVolatile,
                                                              field->_isPrivate, field->_isFinal, field->_fieldname, field->_typeSignature);
   }

void
TR_VectorAPIExpansion::dontVectorizeNode(TR::Node *node)
   {
   if (!node->getOpCode().isLoadAddr() &&
       !node->getOpCode().isLoadDirect() &&
       !node->getOpCode().isStoreDirect() &&
       !node->getOpCode().isFunctionCall())
      return; // will not be vectorized anyway

   if (node->getOpCodeValue() == TR::aload ||
       node->getOpCodeValue() == TR::astore ||
       node->getOpCodeValue() == TR::loadaddr)
      {
      _aliasTable[node->getSymbolReference()->getReferenceNumber()]._vecLen = vec_len_boxed_unknown;
      }
   else if (node->getOpCode().isFunctionCall())
      {
      _nodeTable[node->getGlobalIndex()]._vecLen = vec_len_boxed_unknown;
      }
   else
      {
      TR_ASSERT_FATAL(false, "Incorrect node passed to dontVectorizeNode: %s", node->getOpCode().getName());
      }

   }


bool
TR_VectorAPIExpansion::isVectorizedOrScalarizedNode(TR::Node *node, TR::DataType &elementType, int32_t &bitsLength,
                                                    vapiObjType &objectType, bool &scalarized)
   {
   // TODO: do not override if not vectorized
   elementType = TR::NoType;
   bitsLength = vec_len_default;
   objectType = Unknown;
   scalarized = false;

   int32_t refId = -1;

   if (node->getOpCodeValue() == TR::aload ||
       node->getOpCodeValue() == TR::astore)
      {
      refId = node->getSymbolReference()->getReferenceNumber();

      if (_aliasTable[refId]._vecLen == vec_len_boxed_unknown)
         return false;

      int32_t classId = _aliasTable[refId]._classId;

      if (classId <= 0)
         return false;

      if (_aliasTable[classId]._classId <= 0)
         return false;

      refId = _aliasTable[refId]._tempClassId;

      if (refId <= 0)
         return false;

      if (_aliasTable[refId]._tempClassId <= 0)
         return false;

      if (_aliasTable[classId]._cantVectorize &&
          !_aliasTable[classId]._cantScalarize)
         scalarized = true;
      }
   else if (node->getOpCode().isFunctionCall() &&
            isVectorAPIMethod(node->getSymbolReference()->getSymbol()->castToMethodSymbol()))
      {
      ncount_t nodeIndex = node->getGlobalIndex();
      if (_nodeTable[nodeIndex]._vecLen == vec_len_boxed_unknown)
         return false;

      refId = node->getSymbolReference()->getReferenceNumber();
      int32_t classId = _aliasTable[refId]._classId;

      if (classId <= 0)
         return false;

      if (_aliasTable[classId]._classId <= 0)
         return false;

      elementType = _nodeTable[nodeIndex]._elementType;
      bitsLength = _nodeTable[nodeIndex]._vecLen;
      objectType = _nodeTable[nodeIndex]._objectType;

      if (!_nodeTable[nodeIndex]._canVectorize)
         scalarized = true;

      return true;
      }
   else if (node->getOpCode().isVectorOpCode())
      {
      TR::SymbolReference *origSymRef = _nodeTable[node->getGlobalIndex()]._origSymRef;

      if (!origSymRef)
         return false;  // was vectorized earlier by auto-SIMD or another pass of VectorAPIExpansion

      if (origSymRef->getSymbol()->isMethod())
         {
         ncount_t nodeIndex = node->getGlobalIndex();

         elementType = _nodeTable[nodeIndex]._elementType;
         bitsLength = _nodeTable[nodeIndex]._vecLen;
         objectType = _nodeTable[nodeIndex]._objectType;
         return true;
         }

      refId = origSymRef->getReferenceNumber();
      }
   else   // TODO: check if node was already scalarized
      {
      return false;
      }

   elementType = _aliasTable[refId]._elementType;
   bitsLength = _aliasTable[refId]._vecLen;
   objectType = _aliasTable[refId]._objectType;

   if (_trace)
      traceMsg(comp(), "#%d bitsLength=%d\n", refId, bitsLength);

   if (bitsLength != vec_len_unknown &&
       bitsLength != vec_len_default &&
       bitsLength != vec_len_boxed_unknown)
      {
      return true;
      }

   scalarized = false;
   return false;
   }

void
TR_VectorAPIExpansion::createClassesForBoxing(TR_ResolvedMethod *owningMethod, TR::DataType methodElementType,
                                              vec_sz_t bitsLength)
   {
   if (methodElementType == TR::Int8 && bitsLength == 128 &&
       _classByte128Vector == NULL)
      {
      _classByte128Vector = comp()->fej9()->getClassFromSignature("jdk/incubator/vector/Byte128Vector", 34, owningMethod, true);
      TR_ASSERT_FATAL(_classByte128Vector, "Could not create Vector class from signature");
      }

   if (methodElementType == TR::Int8 && bitsLength == 128 &&
       _classByte128Mask == NULL)
      {
      _classByte128Mask = comp()->fej9()->getClassFromSignature("jdk/incubator/vector/Byte128Vector$Byte128Mask", 46, owningMethod, true);
      TR_ASSERT_FATAL(_classByte128Mask, "Could not create Mask class from signature");
      }
   }


void
TR_VectorAPIExpansion::boxChild(TR::TreeTop *treeTop, TR::Node *node, uint32_t i, bool checkBoxing)
   {
   TR::Node *child = node->getChild(i);

   TR::DataType elementType;
   int32_t bitsLength;
   vapiObjType objectType;
   bool scalarized;

   if (!isVectorizedOrScalarizedNode(child, elementType, bitsLength, objectType, scalarized))
      return;

   if ((objectType != Vector && objectType != Mask) ||
       bitsLength != 128 ||
       elementType != TR::Int8 ||
       scalarized)
      {
      TR_ASSERT_FATAL(checkBoxing, "Incorrect boxing type can only be encountered during check mode");

      int32_t classId = _aliasTable[child->getSymbolReference()->getReferenceNumber()]._classId;

      _aliasTable[classId]._classId = -1;

      if (_trace)
         traceMsg(comp(), "Invalidated class #%d due to unsupported boxing of %d child of node %p in %s\n",
                          classId, i, node, comp()->signature());
      return;
      }

   if (checkBoxing) return;

   TR_OpaqueClassBlock *vecClass = objectType == Vector ? _classByte128Vector : _classByte128Mask;

   TR_ASSERT_FATAL(vecClass, "vecClass is NULL when boxing %p\n", child);

   // generate "newarray  jitNewArray"
   TR_OpaqueClassBlock *j9arrayClass = comp()->fej9()->getArrayClassFromDataType(elementType,
                                                                                objectType == Mask);

   int32_t elementSize = OMR::DataType::getSize(elementType);
   int32_t numLanes = bitsLength/8/elementSize;

   TR::Node *lenConst = TR::Node::iconst(node, numLanes);
   TR::Node *typeConst = TR::Node::iconst(node, comp()->fe()->getNewArrayTypeFromClass(j9arrayClass));
   TR::SymbolReference *symRef = comp()->getSymRefTab()->findOrCreateNewArraySymbolRef(comp()->getMethodSymbol());
   TR::Node *newArray = TR::Node::createWithSymRef(TR::newarray, 2, lenConst, typeConst, 0, symRef);

   treeTop->insertBefore(TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, newArray)));

   // Generate vector store to the payload array

   TR::VectorLength vectorLength = OMR::DataType::bitsToVectorLength(bitsLength);
   TR::DataType opCodeType = (objectType == Vector) ?
                               TR::DataType::createVectorType(elementType, vectorLength)
                               : TR::DataType::createMaskType(elementType, vectorLength);

   TR::Node *vloadNode = child;

   if (!child->getOpCode().isVectorOpCode())  // not vectorized yet
      vloadNode = vectorizeLoadOrStore(this, child, opCodeType, true);

   TR::Node *aladdNode = generateArrayElementAddressNode(comp(), newArray, TR::Node::lconst(node, 0), elementSize);

   TR::SymbolReference *vectorShadow = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(opCodeType, NULL);
   TR::ILOpCodes storeOpcode = TR::ILOpCode::createVectorOpCode(TR::vstorei, opCodeType);
   TR::Node *storeNode = TR::Node::createWithSymRef(storeOpcode, 2, aladdNode, vloadNode, 0, vectorShadow);
   treeTop->insertBefore(TR::TreeTop::create(comp(), storeNode));
   TR::Node *fence = TR::Node::createAllocationFence(newArray, newArray);
   //fence->setAllocation(NULL);
   treeTop->insertBefore(TR::TreeTop::create(comp(), fence));

   // generate "new  jitNewObject"
   TR::Node *newObject = TR::Node::create(child, TR::New, 1);
   newObject->setSymbolReference(comp()->getSymRefTab()->findOrCreateNewObjectSymbolRef(comp()->getMethodSymbol()));

   TR_J9VMBase *fej9 = comp()->fej9();
   TR::SymbolReference *j9class = comp()->getSymRefTab()->findOrCreateClassSymbol(comp()->getMethodSymbol(), -1, vecClass);

   TR_ASSERT_FATAL(j9class, "J9Class symbol reference should not be null");

   newObject->setAndIncChild(0, TR::Node::createWithSymRef(child, TR::loadaddr, 0, j9class));
   treeTop->insertBefore(TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, newObject)));

   // anchor old child
   treeTop->insertBefore(TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, child)));
   child->recursivelyDecReferenceCount();

   node->setAndIncChild(i, newObject);

   fence = TR::Node::createAllocationFence(newObject, newObject);
   //fence->setAllocation(NULL);
   treeTop->insertBefore(TR::TreeTop::create(comp(), fence));

   TR::SymbolReference *payloadSymRef = createPayloadSymbolReference(comp(), vecClass);
   treeTop->insertBefore(TR::TreeTop::create(comp(), TR::Node::storeToAddressField(comp(), newObject, payloadSymRef, newArray)));

   fence = TR::Node::createAllocationFence(newObject, newObject);
   //fence->setAllocation(NULL);
   treeTop->insertBefore(TR::TreeTop::create(comp(), fence));

   if (_trace)
      traceMsg(comp(), "Boxed: %dth child of node %p into %p\n", i, node, newObject);

   if (TR::Options::getVerboseOption(TR_VerboseVectorAPI))
      {
      TR_VerboseLog::writeLine(TR_Vlog_VECTOR_API, "Boxed in %s at %s",
                               comp()->signature(), comp()->getHotnessName(comp()->getMethodHotness()));
      }
   }


TR::Node *
TR_VectorAPIExpansion::unboxNode(TR::Node *parentNode, TR::Node *operand, vapiObjType operandObjectType,
                                 bool checkBoxing)
   {
   TR::DataType elementType;
   int32_t bitsLength;
   vapiObjType parentType;
   bool parentScalarized;
   bool parentVectorizedOrScalarized = isVectorizedOrScalarizedNode(parentNode, elementType, bitsLength,
                                                                    parentType, parentScalarized);

   if ((operandObjectType != Vector && operandObjectType != Mask) ||
        elementType != TR::Int8 ||
        bitsLength != 128 ||
        parentScalarized)  // TODO: support unboxing into scalars
      {
      TR_ASSERT_FATAL(checkBoxing, "Incorrect unboxing type can only be encountered during check mode");

      int32_t classId = _aliasTable[operand->getSymbolReference()->getReferenceNumber()]._classId;

      if (classId > 0)
         _aliasTable[classId]._classId = -1;

      if (_trace)
         traceMsg(comp(), "Invalidated class #%d due to unsupported unboxing of operand %p of node %p in %s\n",
                           classId, operand, parentNode, comp()->signature());

      return NULL;
      }

   if (checkBoxing) return NULL;

   TR_ASSERT_FATAL(parentVectorizedOrScalarized, "Node %p should be vectorized or scalarized", parentNode);

   TR::DataType opCodeType = TR::NoType;
   TR::VectorLength vectorLength = OMR::DataType::bitsToVectorLength(bitsLength);

   if (operandObjectType == Vector)
      {
      opCodeType = TR::DataType::createVectorType(elementType, vectorLength);
      }
   else if (operandObjectType == Mask)
      {
      opCodeType = TR::DataType::createMaskType(elementType, vectorLength);
      }
   else
      {
      TR_ASSERT_FATAL(false, "Unsupported Unboxing type");
      }

   TR_OpaqueClassBlock *vecClass = operandObjectType == Vector ? _classByte128Vector : _classByte128Mask;
   TR_ASSERT_FATAL(vecClass, "vecClass is NULL when unboxing %p\n", operand);

   TR::SymbolReference *payloadSymRef = createPayloadSymbolReference(comp(), vecClass);
   TR::Node *payloadLoad = TR::Node::createWithSymRef(operand, TR::aloadi, 1, payloadSymRef);
   payloadLoad->setAndIncChild(0, operand);


   TR::ILOpCodes opcode = TR::ILOpCode::createVectorOpCode(opCodeType.isVector() ? TR::vloadi : TR::mloadi, opCodeType);
   TR::SymbolReference *vectorShadow = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(opCodeType, NULL);
   TR::Node *newOperand = TR::Node::createWithSymRef(operand, opcode, 1, vectorShadow);
   int32_t elementSize = OMR::DataType::getSize(elementType);
   TR::Node *aladdNode = generateArrayElementAddressNode(comp(), payloadLoad, TR::Node::iconst(operand, 0), elementSize);
   newOperand->setAndIncChild(0, aladdNode);

   if (_trace)
      traceMsg(comp(), "Unboxed: node %p into new node %p for parent %p\n", operand, newOperand, parentNode);

   if (TR::Options::getVerboseOption(TR_VerboseVectorAPI))
      {
      TR_VerboseLog::writeLine(TR_Vlog_VECTOR_API, "Unboxed in %s at %s",
                               comp()->signature(), comp()->getHotnessName(comp()->getMethodHotness()));
      }

   return newOperand;
   }


int32_t
TR_VectorAPIExpansion::expandVectorAPI()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   if (_trace)
      traceMsg(comp(), "%s In expandVectorAPI\n", OPT_DETAILS_VECTOR);

   buildVectorAliases(false);
   buildAliasClasses();
   validateVectorAliasClasses(&vectorAliasTableElement::_aliases, &vectorAliasTableElement::_classId);
   validateVectorAliasClasses(&vectorAliasTableElement::_tempAliases, &vectorAliasTableElement::_tempClassId);

   if (boxingAllowed())
      transformIL(true);

   transformIL(false);

   if (boxingAllowed())
      buildVectorAliases(true);

   if (_trace)
      comp()->dumpMethodTrees("After Vectorization");

   return 1;
   }

void
TR_VectorAPIExpansion::transformIL(bool checkBoxing)
   {
   if (_trace)
      traceMsg(comp(), "%s Starting Expansion checkBoxing=%d\n", OPT_DETAILS_VECTOR, checkBoxing);

   _seenClasses.empty();

   for (TR::TreeTop *treeTop = comp()->getMethodSymbol()->getFirstTreeTop(); treeTop ; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      TR::ILOpCodes opCodeValue = node->getOpCodeValue();
      TR::Node *parent = NULL;
      TR::MethodSymbol *methodSymbol = NULL;

      if (opCodeValue == TR::treetop || opCodeValue == TR::NULLCHK ||
          (boxingAllowed() && treeTopAllowedWithBoxing(opCodeValue)))
          {
          parent = node;
          node = node->getFirstChild();
          opCodeValue = node->getOpCodeValue();
          }

      if (node->chkStoredValueIsIrrelevant())
         continue;

      TR::ILOpCode opCode = node->getOpCode();

      if (opCode.isFunctionCall() && node->getSymbolReference()->getReferenceNumber() == TR_prepareForOSR) // TODO
         continue;

      bool scalarized;
      bool vectorizedNode;

      TR::DataType elementTypeTmp;
      int32_t bitsLengthTmp;
      vapiObjType objectTypeTmp;

      vectorizedNode = isVectorizedOrScalarizedNode(node, elementTypeTmp, bitsLengthTmp, objectTypeTmp, scalarized);

      // Handle non-vectorized nodes by boxing their children
      if (boxingAllowed() &&
          !vectorizedNode &&
          (opCodeValue == TR::astore ||
           opCodeValue == TR::astorei ||
           opCode.isFunctionCall() ||
           opCodeValue == TR::areturn ||
           opCodeValue == TR::aRegStore ||
           opCodeValue == TR::checkcast ||
           opCodeValue == TR::athrow))
         {
         if (_trace)
            traceMsg(comp(), "Checking if children of non-vector node %p need to be boxed\n", node);

         for (int32_t i = 0; i < node->getNumChildren(); i++)
            {
            boxChild(treeTop, node, i, checkBoxing);
            }
         continue;
         }

      if (opCodeValue != TR::astore && !opCode.isFunctionCall())
         continue;

      if (opCode.isFunctionCall())
         {
         methodSymbol = node->getSymbolReference()->getSymbol()->castToMethodSymbol();

         if (!isVectorAPIMethod(methodSymbol))
            continue;
         }

      TR_ASSERT_FATAL(node->getOpCode().hasSymbolReference(), "Node %p should have symbol reference\n", node);

      int32_t symRefId = node->getSymbolReference()->getReferenceNumber();
      int32_t classId = _aliasTable[symRefId]._classId;
      int32_t tempClassId = _aliasTable[symRefId]._tempClassId;

      if (_trace)
         traceMsg(comp(), "#%d classId = %d\n", symRefId, classId);

      if (classId <= 0)
         continue;

      if (_trace)
         traceMsg(comp(), "#%d classId._classId = %d\n", symRefId, _aliasTable[classId]._classId);

      if (_aliasTable[classId]._classId == -1)  // class was invalidated
         continue;

      TR_ASSERT_FATAL(!boxingAllowed() || vectorizedNode,
                      "Node %p should be either a candidate for vectorization or already vectorized", node);

      handlerMode checkMode = checkVectorization;
      handlerMode doMode = doVectorization;

      if ((!boxingAllowed() && _aliasTable[classId]._cantVectorize) ||  //To preserve old behaviour
          (boxingAllowed() && scalarized))                              //TODO: use "scalarized" only
         {
         TR_ASSERT_FATAL(!_aliasTable[classId]._cantScalarize || scalarized, "Class #%d should be either vectorizable or scalarizable",
                                                                              classId);
         checkMode = checkScalarization;
         doMode = doScalarization;
         }

      // if boxing is enabled it might be too late to disable the transformation
      // since some nodes are already boxed above
      if (!boxingAllowed() &&
          !_seenClasses.isSet(classId))
         {
         _seenClasses.set(classId);

         if (!performTransformation(comp(), "%s Starting to %s class #%d\n", optDetailString(),
                                             doMode == doVectorization ? "vectorize" : "scalarize", classId))
            {
            _aliasTable[classId]._classId = -1; // invalidate the whole class
            continue;
            }
         }

      if (_trace)
         traceMsg(comp(), "%s node %p of class #%d\n", checkBoxing ? "Checking for boxing" : "Transforming",
                          node, classId);

      int32_t numLanes;

      if (opCodeValue == TR::astore)
         {
         if (_trace)
            traceMsg(comp(), "%s astore %p (temp class #%d)\n", checkBoxing ? "Checking for boxing" : "Handling",
                              node, tempClassId);

         TR::DataType elementType = _aliasTable[tempClassId]._elementType;
         int32_t bitsLength = _aliasTable[tempClassId]._vecLen;
         TR::VectorLength vectorLength = OMR::DataType::bitsToVectorLength(bitsLength);
         int32_t elementSize = OMR::DataType::getSize(elementType);
         numLanes = bitsLength/8/elementSize;

         if (boxingAllowed())
            {
            TR::DataType elementTypeTmp;
            int32_t bitsLengthTmp;
            vapiObjType objectTypeTmp;
            bool scalarizedTmp;
            bool rhsVectorizedOrScalarized = isVectorizedOrScalarizedNode(node->getFirstChild(), elementTypeTmp, bitsLengthTmp,
                                                                          objectTypeTmp, scalarizedTmp);

            TR_ASSERT_FATAL(rhsVectorizedOrScalarized, "RHS of vectorized astore should be vectorized too");
            }

         if (!checkBoxing)
            astoreHandler(this, treeTop, node, elementType, vectorLength, numLanes, doMode);
         }
      else if (opCode.isFunctionCall())
         {
         TR_ASSERT_FATAL(parent, "All VectorAPI calls are expected to have a treetop");

         TR_ASSERT_FATAL(_nodeTable[node->getGlobalIndex()]._canVectorize || _nodeTable[node->getGlobalIndex()]._canScalarize,
                         "call in node %p should be vectorizable or scalarizable", node);

         TR::RecognizedMethod index = methodSymbol->getRecognizedMethod();
         int32_t handlerIndex = index - _firstMethod;
         TR::DataType elementType;

         getElementTypeAndNumLanes(node, elementType, numLanes);

         int32_t elementSize = OMR::DataType::getSize(elementType);
         int32_t bitsLength = numLanes*elementSize*8;
         TR::VectorLength vectorLength = OMR::DataType::bitsToVectorLength(bitsLength);

         TR_ASSERT_FATAL(methodTable[handlerIndex]._methodHandler(this, treeTop, node, elementType, vectorLength, numLanes, checkMode),
                         "Analysis should've proved that method %p is supported for %s", node,
                         (checkMode == checkScalarization) ? "scalarization" : "vectorization");

         if (!checkBoxing)
            {
            _nodeTable[node->getGlobalIndex()]._origSymRef = node->getSymbolReference();
            TR::Node::recreate(parent, TR::treetop);
            }

         if (boxingAllowed())
            {
            // Unbox operands if needed, before calling handler
            int32_t numChildren = node->getNumChildren();

            if (_trace)
               traceMsg(comp(), "Checking if children of vectorized node %p need to be unboxed\n", node);

            for (int32_t i = 0; i < numChildren; i++)
               {
               // TO DO: check Mask type through the method table
               if ((i >= getFirstOperandIndex(methodSymbol) &&
                   i < (getFirstOperandIndex(methodSymbol) + getNumOperands(methodSymbol))) ||
                   (i == getMaskIndex(methodSymbol) && node->getChild(i)->getOpCodeValue() != TR::aconst))
                  {
                  TR::Node *operand = node->getChild(i);
                  bool vectorizedOrScalarized = false;

                  TR::DataType elementTypeTmp;
                  int32_t bitsLengthTmp;
                  vapiObjType objectTypeTmp;
                  bool scalarizedTmp;

                  vectorizedOrScalarized = isVectorizedOrScalarizedNode(operand, elementTypeTmp, bitsLengthTmp,
                                                                        objectTypeTmp, scalarizedTmp);

                  if (!vectorizedOrScalarized)
                     {
                     TR_ASSERT_FATAL(operand->getOpCodeValue() == TR::aload || operand->getOpCodeValue() == TR::acall,
                                     "Operand can only be aload or acall");
                     vapiObjType operandObjectType = Vector;

                     if (getArgumentType(methodSymbol, i) == Mask)
                        {
                        operandObjectType = Mask;
                        }
                     else if (index == TR::jdk_internal_vm_vector_VectorSupport_binaryOp)
                        {
                        // override argument type for methods for which Vector can actually be Mask
                        // TODO: use Uknown in the table
                        if (_nodeTable[node->getGlobalIndex()]._objectType == Mask)
                           operandObjectType = Mask;
                        }

                     TR::Node *unboxedOperand = unboxNode(node, operand, operandObjectType, checkBoxing);

                     if (!checkBoxing)
                        {
                        treeTop->insertBefore(TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, operand)));
                        operand->recursivelyDecReferenceCount();
                        node->setAndIncChild(i, unboxedOperand);
                        }
                     }
                  }
               }
            }

         if (!checkBoxing)
            {
            methodTable[handlerIndex]._methodHandler(this, treeTop, node, elementType, vectorLength, numLanes, doMode);
            }
         }

      if (!checkBoxing &&
          doMode == doScalarization)
         {
         TR::TreeTop *prevTreeTop = treeTop;
         for (int32_t i = 1; i < numLanes; i++)
            {
            TR::Node *scalarNode = getScalarNode(this, node, i);
            TR::TreeTop *newTreeTop;
            if (scalarNode->getOpCode().isStore())
               {
               newTreeTop = TR::TreeTop::create(comp(), scalarNode, 0, 0);
               }
            else
               {
               TR::Node *treeTopNode = TR::Node::create(TR::treetop, 1);
               newTreeTop = TR::TreeTop::create(comp(), treeTopNode, 0, 0);
               treeTopNode->setAndIncChild(0, scalarNode);
               }

            prevTreeTop->insertAfter(newTreeTop);
            prevTreeTop = newTreeTop;
            }
         }
      }
   }

//
// static transformation routines
//

TR::Node *
TR_VectorAPIExpansion::vectorizeLoadOrStore(TR_VectorAPIExpansion *opt, TR::Node *node,
                                            TR::DataType opCodeType, bool newLoad)
   {
   TR::Compilation *comp = opt->comp();

   TR_ASSERT_FATAL(node->getOpCode().hasSymbolReference(), "%s node %p should have symbol reference", OPT_DETAILS_VECTOR, node);

   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::SymbolReference *vecSymRef = (opt->_aliasTable)[symRef->getReferenceNumber()]._vecSymRef;
   if (vecSymRef == NULL)
      {
      vecSymRef = comp->cg()->allocateLocalTemp(opCodeType);
      (opt->_aliasTable)[symRef->getReferenceNumber()]._vecSymRef = vecSymRef;
      if (opt->_trace)
         traceMsg(comp, "   created new vector symRef #%d for #%d\n", vecSymRef->getReferenceNumber(), symRef->getReferenceNumber());

      }

   TR::ILOpCodes opcode;

   if (node->getOpCode().isStore())
      opcode = TR::ILOpCode::createVectorOpCode(opCodeType.isVector() ? TR::vstore : TR::mstore, opCodeType);
   else
      opcode = TR::ILOpCode::createVectorOpCode(opCodeType.isVector() ? TR::vload : TR::mload, opCodeType);

   if (!newLoad)
      {
      TR::Node::recreate(node, opcode);
      }
   else
      {
      TR_ASSERT_FATAL(!node->getOpCode().isStore(), "Should be a load node");
      node = TR::Node::create(node, opcode, 0);
      }

   node->setSymbolReference(vecSymRef);

   (opt->_nodeTable)[node->getGlobalIndex()]._origSymRef = symRef;

   return node;
   }

void
TR_VectorAPIExpansion::scalarizeLoadOrStore(TR_VectorAPIExpansion *opt, TR::Node *node, TR::DataType elementType, int32_t numLanes)
   {
   TR::Compilation *comp = opt->comp();

   TR_ASSERT_FATAL(node->getOpCode().hasSymbolReference(), "%s node %p should have symbol reference", OPT_DETAILS_VECTOR, node);

   if (elementType == TR::Int8 || elementType == TR::Int16)
      elementType = TR::Int32;

   TR::SymbolReference *nodeSymRef = node->getSymbolReference();
   TR_Array<TR::SymbolReference*> *scalarSymRefs = (opt->_aliasTable)[nodeSymRef->getReferenceNumber()]._scalarSymRefs;

   if (scalarSymRefs == NULL)
      {
      scalarSymRefs = new (comp->trStackMemory()) TR_Array<TR::SymbolReference*>(comp->trMemory(), numLanes, true, stackAlloc);

      for (int32_t i = 0; i < numLanes; i++)
         {
         (*scalarSymRefs)[i] = comp->cg()->allocateLocalTemp(elementType);
         if (opt->_trace)
            traceMsg(comp, "   created new scalar symRef #%d for #%d\n", (*scalarSymRefs)[i]->getReferenceNumber(), nodeSymRef->getReferenceNumber());
         }

      (opt->_aliasTable)[nodeSymRef->getReferenceNumber()]._scalarSymRefs = scalarSymRefs;
      }

   // transform the node
   if (node->getOpCode().isStore())
      TR::Node::recreate(node, comp->il.opCodeForDirectStore(elementType));
   else
      TR::Node::recreate(node, comp->il.opCodeForDirectLoad(elementType));

   node->setSymbolReference((*scalarSymRefs)[0]);
   }


void
TR_VectorAPIExpansion::addScalarNode(TR_VectorAPIExpansion *opt, TR::Node *node, int32_t numLanes, int32_t i, TR::Node *scalarNode)
   {
   TR::Compilation *comp = opt->comp();

   if (opt->_trace)
      traceMsg(comp, "Adding new scalar node %p (lane %d) for node %p\n", scalarNode, i, node);

   TR_Array<TR::Node *> *scalarNodes = (opt->_nodeTable)[node->getGlobalIndex()]._scalarNodes;

    if (scalarNodes == NULL)
      {
      scalarNodes = new (comp->trStackMemory()) TR_Array<TR::Node *>(comp->trMemory(), numLanes, true, stackAlloc);
      (opt->_nodeTable)[node->getGlobalIndex()]._scalarNodes = scalarNodes;
      }

    (*scalarNodes)[i] = scalarNode;
   }

TR::Node *
TR_VectorAPIExpansion::getScalarNode(TR_VectorAPIExpansion *opt, TR::Node *node, int32_t i)
   {
   TR::Compilation *comp = opt->comp();

   TR_Array<TR::Node *> *scalarNodes = (opt->_nodeTable)[node->getGlobalIndex()]._scalarNodes;

   TR_ASSERT_FATAL(scalarNodes, "Pointer should not be NULL for node %p", node);

   return (*scalarNodes)[i];
   }

void TR_VectorAPIExpansion::anchorOldChildren(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node)
   {
   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *oldNode = node->getChild(i);
      treeTop->insertBefore(TR::TreeTop::create(opt->comp(), TR::Node::create(TR::treetop, 1, oldNode)));
      oldNode->recursivelyDecReferenceCount();
      }
   }


TR::Node *
TR_VectorAPIExpansion::generateArrayElementAddressNode(TR::Compilation *comp, TR::Node *array, TR::Node *arrayIndex, int32_t elementSize)
   {
   TR_ASSERT_FATAL_WITH_NODE(array, comp->target().is64Bit(), "TR_VectorAPIExpansion::generateArrayElementAddressNode supports 64 bit vm only.");

   int32_t shiftAmount = 0;
   while ((elementSize = (elementSize >> 1)))
        ++shiftAmount;

   if (shiftAmount != 0)
      {
      TR::Node *lshlNode = TR::Node::create(TR::lshl, 2);
      lshlNode->setAndIncChild(0, arrayIndex);
      lshlNode->setAndIncChild(1, TR::Node::create(TR::iconst, 0, shiftAmount));
      arrayIndex = lshlNode;
      }

   TR::Node *aladdNode = TR::TransformUtil::generateArrayElementAddressTrees(comp, array, arrayIndex);
   aladdNode->setIsInternalPointer(true);

   return aladdNode;
   }

TR::Node *
TR_VectorAPIExpansion::generateAddressNode(TR::Node *base, TR::Node *offset)
   {
   TR::Node *aladdNode = TR::Node::create(TR::aladd, 2, base, offset);
   aladdNode->setIsInternalPointer(true);
   return aladdNode;
   }

void TR_VectorAPIExpansion::aloadHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                         TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode)
   {
   TR::Compilation *comp = opt->comp();

   if (mode == doScalarization)
      {
      int32_t elementSize = OMR::DataType::getSize(elementType);
      int32_t id = node->getSymbolReference()->getReferenceNumber();

      scalarizeLoadOrStore(opt, node, elementType, numLanes);

      TR_Array<TR::SymbolReference*>  *scalarSymRefs = (opt->_aliasTable)[id]._scalarSymRefs;
      TR_ASSERT_FATAL(scalarSymRefs, "scalar references array should not be NULL");

      for (int32_t i = 1; i < numLanes; i++)
         {
         TR_ASSERT_FATAL((*scalarSymRefs)[i], "scalar reference %d should not be NULL", i);
         TR::Node *loadNode = TR::Node::createWithSymRef(node, comp->il.opCodeForDirectLoad(elementType), 0, (*scalarSymRefs)[i]);
         addScalarNode(opt, node, numLanes, i, loadNode);

         }
      }
   else if (mode == doVectorization)
      {
      TR::DataType opCodeType = TR::DataType::createVectorType(elementType, vectorLength);

      if (opt->_aliasTable[node->getSymbolReference()->getReferenceNumber()]._objectType == Mask)
         opCodeType = TR::DataType::createMaskType(elementType, vectorLength);

      vectorizeLoadOrStore(opt, node, opCodeType);
      }

   return;
   }


void TR_VectorAPIExpansion::astoreHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                          TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode)
   {
   TR::Compilation *comp = opt->comp();

   TR::Node *rhs = node->getFirstChild();

   if (mode == doScalarization)
      {
      int32_t elementSize = OMR::DataType::getSize(elementType);
      int32_t id = node->getSymbolReference()->getReferenceNumber();

      TR::ILOpCodes storeOpCode = comp->il.opCodeForDirectStore(elementType);
      scalarizeLoadOrStore(opt, node, elementType, numLanes);

      TR_Array<TR::SymbolReference*>  *scalarSymRefs = (opt->_aliasTable)[id]._scalarSymRefs;
      TR_ASSERT_FATAL(scalarSymRefs, "reference should not be NULL");

      TR::SymbolReference *rhsSymRef = rhs->getSymbolReference();

      if (rhs->getOpCodeValue() == TR::aload) aloadHandler(opt, treeTop, rhs, elementType, vectorLength, numLanes, mode);

      for (int32_t i = 1; i < numLanes; i++)
         {
         TR_ASSERT_FATAL((*scalarSymRefs)[i], "reference should not be NULL");

         TR::Node *storeNode = TR::Node::createWithSymRef(node, storeOpCode, 1, (*scalarSymRefs)[i]);
         storeNode->setAndIncChild(0, getScalarNode(opt, rhs, i));
         addScalarNode(opt, node, numLanes, i, storeNode);
         }
      }
   else if (mode == doVectorization)
      {
      TR::DataType opCodeType = TR::DataType::createVectorType(elementType, vectorLength);

      if (opt->_aliasTable[node->getSymbolReference()->getReferenceNumber()]._objectType == Mask)
         opCodeType = TR::DataType::createMaskType(elementType, vectorLength);

      vectorizeLoadOrStore(opt, node, opCodeType);

      if (rhs->getOpCodeValue() == TR::aload) vectorizeLoadOrStore(opt, rhs, opCodeType);
      }

   return;
   }


TR::Node *TR_VectorAPIExpansion::unsupportedHandler(TR_VectorAPIExpansion *, TR::TreeTop *treeTop,
                                                    TR::Node *node, TR::DataType elementType,
                                                    TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode)
   {
   return NULL;
   }


TR::Node *TR_VectorAPIExpansion::loadIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop,
                                                      TR::Node *node, TR::DataType elementType,
                                                      TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode)
   {
   TR::Compilation *comp = opt->comp();

   vapiObjType objType = getObjectTypeFromClassNode(comp, node->getFirstChild());

   if (mode == checkScalarization)
      {
      return (objType == Vector) ? node : NULL;
      }
   else if (mode == checkVectorization)
      {
      if (objType == Vector)
         {
         if (opt->_trace)
            traceMsg(comp, "Vector load with numLanes %d in node %p\n", numLanes, node);

         TR::DataType vectorType = TR::DataType::createVectorType(elementType, vectorLength);
         TR::ILOpCodes vectorOpCode = TR::ILOpCode::createVectorOpCode(TR::vloadi, vectorType);

         if (!comp->cg()->getSupportsOpCodeForAutoSIMD(vectorOpCode))
            return NULL;

         return node;
         }
      else if (objType == Mask)
         {
         if (opt->_trace)
            traceMsg(comp, "Mask load with numLanes %d in node %p\n", numLanes, node);

         TR::DataType resultType = TR::DataType::createMaskType(elementType, vectorLength);
         TR::ILOpCodes maskConversionOpCode;

         switch (numLanes)
            {
            case 1:
               maskConversionOpCode = TR::ILOpCode::createVectorOpCode(TR::b2m, resultType);
               break;
            case 2:
               maskConversionOpCode = TR::ILOpCode::createVectorOpCode(TR::s2m, resultType);
               break;
            case 4:
               maskConversionOpCode = TR::ILOpCode::createVectorOpCode(TR::i2m, resultType);
               break;
            case 8:
               maskConversionOpCode = TR::ILOpCode::createVectorOpCode(TR::l2m, resultType);
               break;
            case 16:
            case 32:
            case 64:
               {
               TR::VectorLength vectorLength = supportedOnPlatform(comp, numLanes*8);
               if (vectorLength == TR::NoVectorLength) return NULL;
               TR::DataType sourceType = TR::DataType::createVectorType(TR::Int8, vectorLength);
               maskConversionOpCode = TR::ILOpCode::createVectorOpCode(TR::v2m, sourceType, resultType);
               break;
               }
            default:
               TR_ASSERT_FATAL(false, "Unsupported number of lanes when loading a mask\n");
               return NULL;
            }

         if (!comp->cg()->getSupportsOpCodeForAutoSIMD(maskConversionOpCode))
            return NULL;

         return node;
         }

      return NULL; // TODO: support other types of loads
      }

   if (opt->_trace)
      traceMsg(comp, "loadIntrinsicHandler for node %p\n", node);

   TR::Node *base = node->getChild(3);
   TR::Node *offset = node->getChild(4);

   return transformLoadFromArray(opt, treeTop, node, elementType, vectorLength, numLanes, mode, base, offset, objType);
   }

TR::Node *TR_VectorAPIExpansion::transformLoadFromArray(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                        TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                        handlerMode mode,
                                                        TR::Node *base, TR::Node *offset, vapiObjType objType)

   {
   TR::Compilation *comp = opt->comp();

   int32_t elementSize = OMR::DataType::getSize(elementType);
   TR::Node *aladdNode = generateAddressNode(base, offset);

   anchorOldChildren(opt, treeTop, node);

   if (objType != Mask)
      node->setAndIncChild(0, aladdNode);

   node->setNumChildren(1);

   if (mode == doScalarization)
      {
      TR::ILOpCodes loadOpCode = TR::ILOpCode::indirectLoadOpCode(elementType);
      TR::SymbolReference *scalarShadow = comp->getSymRefTab()->findOrCreateArrayShadowSymbolRef(elementType, NULL);

      TR::Node::recreate(node, loadOpCode);
      node->setSymbolReference(scalarShadow);

      // keep Byte and Short as Int after it's loaded from array
      if (elementType == TR::Int8 || elementType == TR::Int16)
         {
         TR::Node *newLoadNode = node->duplicateTree(false);
         aladdNode->decReferenceCount(); // since aladd has one extra count due to duplication
         TR::Node::recreate(node, elementType == TR::Int8 ? TR::b2i : TR::s2i);
         node->setAndIncChild(0, newLoadNode);
         }

      for (int32_t i = 1; i < numLanes; i++)
         {
         TR::Node *newLoadNode = TR::Node::createWithSymRef(node, loadOpCode, 1, scalarShadow);
         TR::Node *newAddressNode = TR::Node::create(TR::aladd, 2, aladdNode, TR::Node::create(TR::lconst, 0, i*elementSize));
         newAddressNode->setIsInternalPointer(true);
         newLoadNode->setAndIncChild(0, newAddressNode);

         // keep Byte and Short as Int after it's loaded from array
         if (elementType == TR::Int8 || elementType == TR::Int16)
            newLoadNode = TR::Node::create(newLoadNode, elementType == TR::Int8 ? TR::b2i : TR::s2i, 1, newLoadNode);

         addScalarNode(opt, node, numLanes, i, newLoadNode);
         }
      }
   else if (mode == doVectorization)
      {
      TR::DataType vectorType = TR::DataType::createVectorType(elementType, vectorLength);
      TR::ILOpCodes op;

      if (objType == Vector)
         {
         TR::DataType symRefType = vectorType;
         TR::SymbolReference *symRef = comp->getSymRefTab()->findOrCreateArrayShadowSymbolRef(symRefType, NULL);
         op = TR::ILOpCode::createVectorOpCode(TR::vloadi, vectorType);
         TR::Node::recreate(node, op);
         node->setSymbolReference(symRef);
         }
      else if (objType == Mask)
         {
         TR::ILOpCodes loadOpCode;
         TR::DataType symRefType;

         switch (numLanes)
            {
            case 1:
               op = TR::ILOpCode::createVectorOpCode(TR::b2m, vectorType);
               loadOpCode = TR::bloadi;
               symRefType = TR::Int8;
               break;
            case 2:
               op = TR::ILOpCode::createVectorOpCode(TR::s2m, vectorType);
               loadOpCode = TR::sloadi;
               symRefType = TR::Int16;
               break;
            case 4:
               op = TR::ILOpCode::createVectorOpCode(TR::i2m, vectorType);
               loadOpCode = TR::iloadi;
               symRefType = TR::Int32;
               break;
            case 8:
               op = TR::ILOpCode::createVectorOpCode(TR::l2m, vectorType);
               loadOpCode = TR::lloadi;
               symRefType = TR::Int64;
               break;
            case 16:
            case 32:
            case 64:
               {
               TR::VectorLength vectorLength = OMR::DataType::bitsToVectorLength(numLanes*8);
               TR::DataType sourceType = TR::DataType::createVectorType(TR::Int8, vectorLength);
               op = TR::ILOpCode::createVectorOpCode(TR::v2m, sourceType, vectorType);
               loadOpCode = TR::ILOpCode::createVectorOpCode(TR::vloadi, sourceType);
               symRefType = sourceType;
               break;
               }
            default:
               TR_ASSERT_FATAL(false, "Unsupported number of lanes when loading a mask\n");
               return NULL;
            }

         TR::Node::recreate(node, op);

         // need to alias with boolean array elements, so creating GenericIntArrayShadow
         TR::SymbolReference *symRef = comp->getSymRefTab()->findOrCreateGenericIntArrayShadowSymbolReference(0);

         TR::Node *loadNode = TR::Node::createWithSymRef(node, loadOpCode, 1, symRef);
         loadNode->setAndIncChild(0, aladdNode);
         node->setAndIncChild(0, loadNode);
         }

      if (TR::Options::getVerboseOption(TR_VerboseVectorAPI))
         {
         TR::ILOpCode opcode(op);
         TR_VerboseLog::writeLine(TR_Vlog_VECTOR_API, "Vectorized using %s%s in %s at %s",
                                  opcode.getName(), TR::DataType::getName(opcode.getVectorResultDataType()),
                                  comp->signature(), comp->getHotnessName(comp->getMethodHotness()));
         }
      }

   return node;
   }


TR::Node *TR_VectorAPIExpansion::storeIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                       TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                       handlerMode mode)
   {
   TR::Compilation *comp = opt->comp();

   vapiObjType objType = getObjectTypeFromClassNode(comp, node->getFirstChild());

   if (mode == checkScalarization)
      {
      return (objType == Vector) ? node : NULL;
      }
   else if (mode == checkVectorization)
      {
      if (objType == Vector)
         {
         TR::DataType vectorType = TR::DataType::createVectorType(elementType, vectorLength);
         TR::ILOpCodes vectorOpCode = TR::ILOpCode::createVectorOpCode(TR::vstorei, vectorType);

         if (!comp->cg()->getSupportsOpCodeForAutoSIMD(vectorOpCode))
            return NULL;

         return node;
         }
      else if (objType == Mask)
         {
         if (opt->_trace)
            traceMsg(comp, "Mask store with numLanes %d in node %p\n", numLanes, node);

         TR::DataType sourceType = TR::DataType::createMaskType(elementType, vectorLength);
         TR::ILOpCodes maskConversionOpCode;

         switch (numLanes)
            {
            case 1:
               maskConversionOpCode = TR::ILOpCode::createVectorOpCode(TR::m2b, sourceType);
               break;
            case 2:
               maskConversionOpCode = TR::ILOpCode::createVectorOpCode(TR::m2s, sourceType);
               break;
            case 4:
               maskConversionOpCode = TR::ILOpCode::createVectorOpCode(TR::m2i, sourceType);
               break;
            case 8:
               maskConversionOpCode = TR::ILOpCode::createVectorOpCode(TR::m2l, sourceType);
               break;
            case 16:
            case 32:
            case 64:
               {
               TR::VectorLength vectorLength = supportedOnPlatform(comp, numLanes*8);
               if (vectorLength == TR::NoVectorLength) return NULL;
               TR::DataType resultType = TR::DataType::createVectorType(TR::Int8, vectorLength);
               maskConversionOpCode = TR::ILOpCode::createVectorOpCode(TR::m2v, sourceType, resultType);
               break;
               }
            default:
               TR_ASSERT_FATAL(false, "Unsupported number of lanes when loading a mask\n");
               return NULL;
            }

         if (!comp->cg()->getSupportsOpCodeForAutoSIMD(maskConversionOpCode))
            return NULL;

         return node;
         }
      else  // TODO: Shuffle
         {
         return NULL;
         }
      }

   if (opt->_trace)
      traceMsg(comp, "storeIntrinsicHandler for node %p\n", node);

   TR::Node *base = node->getChild(3);
   TR::Node *offset = node->getChild(4);
#if JAVA_SPEC_VERSION <= 21
   TR::Node *valueToWrite = node->getChild(5);
#else
   TR::Node *valueToWrite = node->getChild(6);
#endif

   return transformStoreToArray(opt, treeTop, node, elementType, vectorLength, numLanes, mode, valueToWrite, base, offset, objType);
   }


TR::Node *TR_VectorAPIExpansion::transformStoreToArray(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                       TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                       handlerMode mode,
                                                       TR::Node *valueToWrite, TR::Node *base, TR::Node *offset, vapiObjType objType)

   {
   TR::Compilation *comp = opt->comp();

   int32_t  elementSize = OMR::DataType::getSize(elementType);
   TR::Node *aladdNode = generateAddressNode(base, offset);

   anchorOldChildren(opt, treeTop, node);
   node->setAndIncChild(0, aladdNode);
   node->setAndIncChild(1, valueToWrite);
   node->setNumChildren(2);

   if (mode == doScalarization)
      {
      // TODO: use TR::ILOpCode::indirectLoadOpCode(elementType) after adding it to OMR
      TR_ASSERT_FATAL(elementType < TR::NumOMRTypes, "unexpected type");
      TR::ILOpCodes storeOpCode = comp->il.OMR::IL::opCodeForIndirectStore(elementType);

      TR::SymbolReference *scalarShadow = comp->getSymRefTab()->findOrCreateArrayShadowSymbolRef(elementType, NULL);

      if (valueToWrite->getOpCodeValue() == TR::aload)
         aloadHandler(opt, treeTop, valueToWrite, elementType, vectorLength, numLanes, mode);

      TR::Node::recreate(node, storeOpCode);
      node->setSymbolReference(scalarShadow);

      // Truncate to Byte or Short before writing to array
      if (elementType == TR::Int8 || elementType == TR::Int16)
         {
         TR::Node *newValueToWrite = TR::Node::create(valueToWrite, elementType == TR::Int8 ? TR::i2b : TR::i2s, 1, valueToWrite);
         valueToWrite->recursivelyDecReferenceCount();
         node->setAndIncChild(1, newValueToWrite);
         }


      for (int32_t i = 1; i < numLanes; i++)
         {
         TR::Node *newStoreNode = TR::Node::createWithSymRef(node, storeOpCode, 2, scalarShadow);
         TR::Node *newAddressNode = TR::Node::create(TR::aladd, 2, aladdNode, TR::Node::create(TR::lconst, 0, i*elementSize));
         newAddressNode->setIsInternalPointer(true);
         newStoreNode->setAndIncChild(0, newAddressNode);
         TR::Node *newValueToWrite = getScalarNode(opt, valueToWrite, i);

         // Truncate to Byte or Short before writing to array
         if (elementType == TR::Int8 || elementType == TR::Int16)
            newValueToWrite = TR::Node::create(newValueToWrite, elementType == TR::Int8 ? TR::i2b : TR::i2s, 1, newValueToWrite);

         newStoreNode->setAndIncChild(1, newValueToWrite);
         addScalarNode(opt, node, numLanes, i, newStoreNode);
         }
      }
   else if (mode == doVectorization)
      {
      TR::DataType opCodeType = TR::DataType::createVectorType(elementType, vectorLength);
      if (objType == Mask)
          opCodeType = TR::DataType::createMaskType(elementType, vectorLength);

      if (valueToWrite->getOpCodeValue() == TR::aload) vectorizeLoadOrStore(opt, valueToWrite, opCodeType);

      TR::ILOpCodes op;

      if (objType == Vector)
         {
         TR::SymbolReference *symRef = comp->getSymRefTab()->findOrCreateArrayShadowSymbolRef(opCodeType, NULL);
         op = TR::ILOpCode::createVectorOpCode(TR::vstorei, opCodeType);
         TR::Node::recreate(node, op);
         node->setSymbolReference(symRef);
         }
      else if (objType == Mask)
         {
         TR::ILOpCodes storeOpCode;

         switch (numLanes)
            {
            case 1:
               op = TR::ILOpCode::createVectorOpCode(TR::m2b, opCodeType);
               storeOpCode = TR::bstorei;
               break;
            case 2:
               op = TR::ILOpCode::createVectorOpCode(TR::m2s, opCodeType);
               storeOpCode = TR::sstorei;
               break;
            case 4:
               op = TR::ILOpCode::createVectorOpCode(TR::m2i, opCodeType);
               storeOpCode = TR::istorei;
               break;
            case 8:
               op = TR::ILOpCode::createVectorOpCode(TR::m2l, opCodeType);
               storeOpCode = TR::lstorei;
               break;
            case 16:
            case 32:
            case 64:
               {
               TR::VectorLength vectorLength = OMR::DataType::bitsToVectorLength(numLanes*8);
               TR::DataType targetType = TR::DataType::createVectorType(TR::Int8, vectorLength);
               op = TR::ILOpCode::createVectorOpCode(TR::m2v, opCodeType, targetType);
               storeOpCode = TR::ILOpCode::createVectorOpCode(TR::vstorei, targetType);
               break;
               }
            default:
               TR_ASSERT_FATAL(false, "Unsupported number of lanes when loading a mask\n");
               return NULL;
            }

         // need to alias with boolean array elements, so creating GenericIntArrayShadow
         TR::SymbolReference *symRef = comp->getSymRefTab()->findOrCreateGenericIntArrayShadowSymbolReference(0);
         TR::Node::recreate(node, storeOpCode);
         node->setSymbolReference(symRef);

         TR::Node *convNode = TR::Node::create(node, op, 1);
         convNode->setChild(0, valueToWrite);
         node->setAndIncChild(1, convNode);
         }

      if (TR::Options::getVerboseOption(TR_VerboseVectorAPI))
         {
         TR::ILOpCode opcode(op);
         TR_VerboseLog::writeLine(TR_Vlog_VECTOR_API, "Vectorized using %s%s in %s at %s",
                                  opcode.getName(), TR::DataType::getName(opcode.getVectorResultDataType()),
                                  comp->signature(), comp->getHotnessName(comp->getMethodHotness()));
         }
      }

   return node;
   }


TR::Node *TR_VectorAPIExpansion::unaryIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                       TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                       handlerMode mode)
   {
   return naryIntrinsicHandler(opt, treeTop, node, elementType, vectorLength, numLanes, mode, 1, Other);
   }

TR::Node *TR_VectorAPIExpansion::binaryIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                        TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                        handlerMode mode)
   {
   return naryIntrinsicHandler(opt, treeTop, node, elementType, vectorLength, numLanes, mode, 2, Other);
   }

TR::Node *TR_VectorAPIExpansion::maskReductionCoercedIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                                  TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                                  handlerMode mode)
   {
   return naryIntrinsicHandler(opt, treeTop, node, elementType, vectorLength, numLanes, mode, 1, MaskReduction);
   }


TR::Node *TR_VectorAPIExpansion::reductionCoercedIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                                  TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                                  handlerMode mode)
   {
   return naryIntrinsicHandler(opt, treeTop, node, elementType, vectorLength, numLanes, mode, 1, Reduction);
   }

TR::Node *TR_VectorAPIExpansion::ternaryIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                         TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                         handlerMode mode)
   {
   return naryIntrinsicHandler(opt, treeTop, node, elementType, vectorLength, numLanes, mode, 3, Other);
   }

TR::Node *TR_VectorAPIExpansion::testIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                         TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                         handlerMode mode)
   {
   return naryIntrinsicHandler(opt, treeTop, node, elementType, vectorLength, numLanes, mode, 1, Test);
   }

TR::Node *TR_VectorAPIExpansion::transformRORtoROL(TR_VectorAPIExpansion *opt, TR::Node *shiftAmount,
                                                   TR::DataType elementType, TR::VectorLength vectorLength,
                                                   vapiOpCodeType opCodeType)
   {
   // genearate <element size in bits> - ShiftAmount
   // library already does the masking of the shift amount whether it's vector or scalar

   TR::Compilation *comp = opt->comp();

   int32_t bitSize = OMR::DataType::getSize(elementType)*8;
   TR::Node *subNode;
   TR::ILOpCodes subOpCode;
   TR::Node *bitSizeNode;

   if (opCodeType != BroadcastInt)
      {
      bitSizeNode = TR::Node::create(shiftAmount, TR::ILOpCode::constOpCode(elementType), 0, bitSize);

      TR::DataType vectorType = TR::DataType::createVectorType(elementType, vectorLength);

      bitSizeNode = TR::Node::create(shiftAmount, TR::ILOpCode::createVectorOpCode(TR::vsplats, vectorType), 1, bitSizeNode);
      subOpCode =  TR::ILOpCode::createVectorOpCode(TR::vsub, vectorType);
      }
   else
      {
      bitSizeNode = TR::Node::create(shiftAmount, TR::iconst, 0, bitSize);
      subOpCode = TR::isub;
      }

   subNode = TR::Node::create(shiftAmount, subOpCode, 2);

   subNode->setAndIncChild(0, bitSizeNode);
   subNode->setChild(1, shiftAmount);

   return subNode;
   }


TR::Node *TR_VectorAPIExpansion::naryIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                      TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                      handlerMode mode,
                                                      int32_t numChildren, vapiOpCodeType opCodeType)
   {
   TR::Compilation *comp = opt->comp();
   TR::Node *opcodeNode = node->getFirstChild();

   // TODO: use getFirstOperandIndex()
   int firstOperand = 5;

   if (opCodeType == Test || opCodeType == MaskReduction || opCodeType == Blend)
      firstOperand = 4;

   if (opCodeType == Convert)
      firstOperand = 7;

   if (opCodeType == Compress && numChildren == 1)  // mask compress
      firstOperand = 6;

   bool withMask = false;

   if (opCodeType != MaskReduction && opCodeType != Convert && opCodeType != Compress)
      {
      TR::Node *maskNode = node->getChild(firstOperand + numChildren);  // each intrinsic has a mask argument
      withMask = !maskNode->isConstZeroValue();
      }

   if (withMask) numChildren++;

   int32_t vectorAPIOpcode;

   //skip when opCodeType is Blend since VectorSupport.blend doesn't have an opcode as its first argument
   if (opCodeType != Blend)
      {
      if (!opcodeNode->getOpCode().isLoadConst())
         {
         if (opt->_trace) traceMsg(comp, "Unknown opcode in node %p\n", node);
         return NULL;
         }

      vectorAPIOpcode = opcodeNode->get32bitIntegralValue();
      }

   TR::DataType opType = elementType;

   TR::ILOpCodes scalarOpCode = TR::BadILOp;
   TR::ILOpCodes vectorOpCode = TR::BadILOp;

   if (mode == checkScalarization || mode == doScalarization)
      {
      // Byte and Short are promoted after being loaded from array
      // and all operations should be done in Int in the case of scalarization
      if (elementType == TR::Int8 || elementType == TR::Int16)
           opType = TR::Int32;
      scalarOpCode = ILOpcodeFromVectorAPIOpcode(comp, vectorAPIOpcode, opType, TR::NoVectorLength, opCodeType, withMask);

      if (mode == checkScalarization)
         {
         if (scalarOpCode == TR::BadILOp)
            {
            if (opt->_trace) traceMsg(comp, "Unsupported scalar opcode in node %p\n", node);
            return NULL;
            }
         else
            {
            return node;
            }
         }
      else
         {
         TR_ASSERT_FATAL(scalarOpCode != TR::BadILOp, "Scalar opcode should exist for node %p\n", node);

         if (TR::Options::getVerboseOption(TR_VerboseVectorAPI))
            {
            TR::ILOpCode opcode(scalarOpCode);
            TR_VerboseLog::writeLine(TR_Vlog_VECTOR_API, "Scalarized using %s in %s at %s",
                                     opcode.getName(), comp->signature(), comp->getHotnessName(comp->getMethodHotness()));
            }
         }
      }
   else
      {
      TR::DataType resultElementType = TR::NoType;
      TR::VectorLength resultVectorLength = TR::NoVectorLength;

      if (opCodeType == Convert)
         {
         // result vector type info is in children 5 and 6
         TR::Node *resultElementTypeNode = node->getChild(5);
         resultElementType = getDataTypeFromClassNode(comp, resultElementTypeNode);

         TR::Node *resultNumLanesNode = node->getChild(6);

         if (resultNumLanesNode->getOpCode().isLoadConst())
            {
            int32_t elementSize = OMR::DataType::getSize(resultElementType);
            vec_sz_t bitsLength = resultNumLanesNode->get32bitIntegralValue()*8*elementSize;

            if (supportedOnPlatform(comp, bitsLength) == TR::NoVectorLength)
               return NULL;

            resultVectorLength = OMR::DataType::bitsToVectorLength(bitsLength);
            }

         if (resultElementType == TR::NoType || resultVectorLength == TR::NoVectorLength)
            return NULL;
         }


      if (opCodeType == Compare)
         {
         resultElementType = elementType;
         resultVectorLength = vectorLength;

         if (elementType == TR::Float)
            resultElementType = TR::Int32;

         if (elementType == TR::Double)
            resultElementType = TR::Int64;
         }

      if (mode == checkVectorization)
         {
         vectorOpCode = ILOpcodeFromVectorAPIOpcode(comp, vectorAPIOpcode, opType, vectorLength, opCodeType, withMask,
                                                    resultElementType, resultVectorLength);

         if (vectorOpCode == TR::BadILOp || !comp->cg()->getSupportsOpCodeForAutoSIMD(vectorOpCode))
            {
            if (opt->_trace) traceMsg(comp, "Unsupported vector opcode in node %p %s\n", node,
                                      vectorOpCode == TR::BadILOp ? "(no IL)" : "(no codegen)");
            return NULL;
            }

         if (opCodeType == BroadcastInt)
            {
            TR::ILOpCodes splatsOpCode = TR::ILOpCode::createVectorOpCode(TR::vsplats,
                                                                          TR::DataType::createVectorType(elementType, vectorLength));

            if (!comp->cg()->getSupportsOpCodeForAutoSIMD(splatsOpCode))
               {
               if (opt->_trace) traceMsg(comp, "Unsupported vsplats opcode in node %p (no codegen)\n", node);

               return NULL;
               }
            }
         else if (vectorAPIOpcode == VECTOR_OP_RROTATE)
            {
            TR::DataType vectorType = TR::DataType::createVectorType(elementType, vectorLength);

            TR::ILOpCodes splatsOpCode = TR::ILOpCode::createVectorOpCode(TR::vsplats, vectorType);
            TR::ILOpCodes subOpCode = TR::ILOpCode::createVectorOpCode(TR::vsub, vectorType);

            if (!comp->cg()->getSupportsOpCodeForAutoSIMD(splatsOpCode) ||
                !comp->cg()->getSupportsOpCodeForAutoSIMD(subOpCode))
               {
               if (opt->_trace) traceMsg(comp, "Unsupported vsplats or vsub opcode in node %p (no codegen)\n", node);

               return NULL;
               }
            }

         return node;
         }
      else
         {
         vectorOpCode = ILOpcodeFromVectorAPIOpcode(comp, vectorAPIOpcode, opType, vectorLength, opCodeType, withMask,
                                                    resultElementType, resultVectorLength);

         TR_ASSERT_FATAL(vectorOpCode != TR::BadILOp, "Vector opcode should exist for node %p\n", node);

         if (TR::Options::getVerboseOption(TR_VerboseVectorAPI))
            {
            TR::ILOpCode opcode(vectorOpCode);
            TR_VerboseLog::writeLine(TR_Vlog_VECTOR_API, "Vectorized using %s%s in %s at %s",
                                     opcode.getName(), TR::DataType::getName(opcode.getVectorResultDataType()),
                                     comp->signature(), comp->getHotnessName(comp->getMethodHotness()));
            }

         }
      }

   return transformNary(opt, treeTop, node, elementType, vectorLength, numLanes, mode, scalarOpCode, vectorOpCode,
                        firstOperand, numChildren, opCodeType, vectorAPIOpcode == VECTOR_OP_RROTATE);
   }

TR::Node *TR_VectorAPIExpansion::blendIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                       TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                       handlerMode mode)
   {
   return naryIntrinsicHandler(opt, treeTop, node, elementType, vectorLength, numLanes, mode, 3, Blend);
   }

TR::Node *TR_VectorAPIExpansion::broadcastIntIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                       TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                       handlerMode mode)
   {
   return naryIntrinsicHandler(opt, treeTop, node, elementType, vectorLength, numLanes, mode, 2, BroadcastInt);
   }

TR::Node *TR_VectorAPIExpansion::fromBitsCoercedIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                                 TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                                 handlerMode mode)
   {
   TR::Compilation *comp = opt->comp();

   TR::Node *broadcastTypeNode = node->getChild(4);

   if (!broadcastTypeNode->getOpCode().isLoadConst())
      {
      if (opt->_trace) traceMsg(comp, "Unknown broadcast type in node %p\n", node);
      return NULL;
      }

   int32_t broadcastType = broadcastTypeNode->get32bitIntegralValue();

   TR_ASSERT_FATAL(broadcastType == MODE_BROADCAST || broadcastType == MODE_BITS_COERCED_LONG_TO_MASK,
                   "Unexpected broadcast type in node %p\n", node);

   bool mask = (broadcastType == MODE_BITS_COERCED_LONG_TO_MASK);

   if (mode == checkScalarization)
      return mask ? NULL : node;

   if (mode == checkVectorization)
      {
      TR::ILOpCodes splatsOpCode = TR::ILOpCode::createVectorOpCode(mask ? TR::mLongBitsToMask : TR::vsplats,
                                                                    TR::DataType::createVectorType(elementType, vectorLength));

      if (!comp->cg()->getSupportsOpCodeForAutoSIMD(splatsOpCode))
         return NULL;

      return node;
      }

   if (opt->_trace)
      traceMsg(comp, "fromBitsCoercedIntrinsicHandler for node %p\n", node);

   int32_t elementSize = OMR::DataType::getSize(elementType);
   TR::Node *valueToBroadcast = node->getChild(3);

   anchorOldChildren(opt, treeTop, node);

   TR::Node *newNode;

   int32_t type = mask ? TR::Int64 : elementType;
   switch (type) {
      case TR::Float:
          newNode = TR::Node::create(node, TR::ibits2f, 1, TR::Node::create(node, TR::l2i, 1, valueToBroadcast));
          break;
      case TR::Double:
          newNode = TR::Node::create(node, TR::lbits2d, 1, valueToBroadcast);
          break;
      case TR::Int8:
          newNode = TR::Node::create(node, mode == doScalarization ? TR::l2i : TR::l2b, 1, valueToBroadcast);
          break;
      case TR::Int16:
          newNode = TR::Node::create(node, mode == doScalarization ? TR::l2i : TR::l2s, 1, valueToBroadcast);
          break;
      case TR::Int32:
          newNode = TR::Node::create(node, TR::l2i, 1, valueToBroadcast);
          break;
      case TR::Int64:
          // redundant conversion to simplify node recreation
          newNode = TR::Node::create(node, TR::dbits2l, 1, TR::Node::create(node, TR::lbits2d, 1, valueToBroadcast));
          break;
      default:
         TR_ASSERT_FATAL(false, "Unexpected vector element type for the Vector API\n");
         break;
   }

   if (mode == doScalarization)
      {
      // modify original node in place
      node->setChild(0, newNode->getChild(0));
      node->setNumChildren(1);

      TR::Node::recreate(node, newNode->getOpCodeValue());

      for (int32_t i = 1; i < numLanes; i++)
         {
         addScalarNode(opt, node, numLanes, i, node);
         }

      if (TR::Options::getVerboseOption(TR_VerboseVectorAPI))
         {
         TR_VerboseLog::writeLine(TR_Vlog_VECTOR_API, "Scalarized fromBitsCoerced for %s in %s at%s", TR::DataType::getName(elementType), comp->signature(), comp->getHotnessName(comp->getMethodHotness()));
         }
      }
   else if (mode == doVectorization)
      {
      node->setAndIncChild(0, newNode);
      node->setNumChildren(1);
      TR::ILOpCodes splatsOpCode = TR::ILOpCode::createVectorOpCode(mask ? TR::mLongBitsToMask : TR::vsplats,
                                                                    TR::DataType::createVectorType(elementType, vectorLength));

      TR::Node::recreate(node, splatsOpCode);

      if (TR::Options::getVerboseOption(TR_VerboseVectorAPI))
         {
         TR::ILOpCode opcode(splatsOpCode);
         TR_VerboseLog::writeLine(TR_Vlog_VECTOR_API, "Vectorized using %s%s in %s at %s", opcode.getName(),
                                  TR::DataType::getName(opcode.getVectorResultDataType()), comp->signature(),
                                  comp->getHotnessName(comp->getMethodHotness()));
         }
      }

   return node;
   }

TR::Node *TR_VectorAPIExpansion::compareIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                         TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                         handlerMode mode)
   {
   return naryIntrinsicHandler(opt, treeTop, node, elementType, vectorLength, numLanes, mode, 2, Compare);
   }

TR::Node *TR_VectorAPIExpansion::compressExpandOpIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                         TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                         handlerMode mode)
   {
   TR::Compilation *comp = opt->comp();
   vapiObjType objType;

   if (node->getFirstChild()->getOpCode().isLoadConst() &&
       node->getFirstChild()->get32bitIntegralValue() == VECTOR_OP_MASK_COMPRESS)
       objType = Mask;
   else
       objType = Vector;

   return naryIntrinsicHandler(opt, treeTop, node, elementType, vectorLength, numLanes, mode, objType == Vector ? 2 : 1, Compress);
   }

TR::Node *TR_VectorAPIExpansion::convertIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                         TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                         handlerMode mode)
   {
   return naryIntrinsicHandler(opt, treeTop, node, elementType, vectorLength, numLanes, mode, 1, Convert);
   }

TR::ILOpCodes TR_VectorAPIExpansion::ILOpcodeFromVectorAPIOpcode(TR::Compilation *comp, int32_t vectorAPIOpCode, TR::DataType elementType,
                                                                 TR::VectorLength vectorLength, vapiOpCodeType opCodeType,
                                                                 bool withMask,
                                                                 TR::DataType resultElementType,
                                                                 TR::VectorLength resultVectorLength)
   {
   // TODO: support more scalarization

   bool scalar = (vectorLength == TR::NoVectorLength);
   TR::DataType vectorType = scalar ? TR::NoType : TR::DataType::createVectorType(elementType, vectorLength);
   TR::DataType resultVectorType = TR::NoType;

   if (resultElementType != TR::NoType)
      resultVectorType = scalar ? TR::NoType : TR::DataType::createVectorType(resultElementType, resultVectorLength);


   if (opCodeType == Convert)
      {
      switch (vectorAPIOpCode)
         {
         case VECTOR_OP_CAST: return TR::BadILOp;
         case VECTOR_OP_UCAST: return TR::BadILOp;
         case VECTOR_OP_REINTERPRET:
            if (scalar) return TR::BadILOp;

            if (OMR::DataType::getSize(resultElementType) != OMR::DataType::getSize(elementType) ||
                resultVectorLength != vectorLength)
               {
               traceMsg(comp, "\nCalling VECTOR_OP_REINTERPRET on %s to %s in %s\n", TR::DataType::getName(vectorType),
                                                                            TR::DataType::getName(resultVectorType),
                                                                            comp->signature());
               return TR::BadILOp;
               }

            return TR::ILOpCode::createVectorOpCode(TR::vcast, vectorType, resultVectorType);
         default:
            return TR::BadILOp;
         }
      }
   else if (opCodeType == Blend)
      {
      if (scalar)
         return TR::BadILOp;
      else
         return TR::ILOpCode::createVectorOpCode(TR::vbitselect, vectorType);
      }
   else if ((opCodeType == Test) && withMask)
      {
      switch (vectorAPIOpCode)
         {
         case BT_ne:       return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::mmAnyTrue, vectorType);
         case BT_overflow: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::mmAllTrue, vectorType);
         default:
            return TR::BadILOp;
         }
      }
   else if ((opCodeType == BroadcastInt) && withMask)
      {
      switch (vectorAPIOpCode)
         {
         case VECTOR_OP_LSHIFT:  return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmshl, vectorType);
         case VECTOR_OP_RSHIFT:  return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmshr, vectorType);
         case VECTOR_OP_URSHIFT: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmushr, vectorType);
         case VECTOR_OP_LROTATE: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmrol, vectorType);
         case VECTOR_OP_RROTATE: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmrol, vectorType);
         default:
            return TR::BadILOp;
         }
      }
   else if (opCodeType == BroadcastInt)
      {
      switch (vectorAPIOpCode)
         {
         case VECTOR_OP_LSHIFT:  return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vshl, vectorType);
         case VECTOR_OP_RSHIFT:  return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vshr, vectorType);
         case VECTOR_OP_URSHIFT: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vushr, vectorType);
         case VECTOR_OP_LROTATE: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vrol, vectorType);
         case VECTOR_OP_RROTATE: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vrol, vectorType);
         default:
            return TR::BadILOp;
         }
      }
   else if ((opCodeType == Compare) && withMask)
      {
      TR::DataType resultMaskType = scalar ? TR::NoType : TR::DataType::createMaskType(resultElementType, resultVectorLength);

      switch (vectorAPIOpCode)
         {
         case BT_eq: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmcmpeq, vectorType, resultMaskType);
         case BT_ne: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmcmpne, vectorType, resultMaskType);
         case BT_le: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmcmple, vectorType, resultMaskType);
         case BT_ge: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmcmpge, vectorType, resultMaskType);
         case BT_lt: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmcmplt, vectorType, resultMaskType);
         case BT_gt: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmcmpgt, vectorType, resultMaskType);
         default:
            return TR::BadILOp;
         }
      }
   else if (opCodeType == Compare)
      {
      TR::DataType resultMaskType = scalar ? TR::NoType : TR::DataType::createMaskType(resultElementType, resultVectorLength);

      switch (vectorAPIOpCode)
         {
         case BT_eq: return scalar ? TR::ILOpCode::cmpeqOpCode(elementType)
                                   : TR::ILOpCode::createVectorOpCode(TR::vcmpeq, vectorType, resultMaskType);
         case BT_ne: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vcmpne, vectorType, resultMaskType);
         case BT_le: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vcmple, vectorType, resultMaskType);
         case BT_ge: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vcmpge, vectorType, resultMaskType);
         case BT_lt: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vcmplt, vectorType, resultMaskType);
         case BT_gt: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vcmpgt, vectorType, resultMaskType);
         default:
            return TR::BadILOp;
         }
      }
   else if ((opCodeType == Reduction) && withMask)
      {
      switch (vectorAPIOpCode)
         {
         case VECTOR_OP_ADD: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmreductionAdd, vectorType);
         case VECTOR_OP_MUL: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmreductionMul, vectorType);
         case VECTOR_OP_MIN: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmreductionMin, vectorType);
         case VECTOR_OP_MAX: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmreductionMax, vectorType);
         case VECTOR_OP_AND: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmreductionAnd, vectorType);
         case VECTOR_OP_OR:  return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmreductionOr, vectorType);
         case VECTOR_OP_XOR: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmreductionXor, vectorType);
            // These don't seem to be generated by the library:
            // vreductionOrUnchecked
            // vreductionFirstNonZero
         default:
            return TR::BadILOp;
         }
      }
   else if (opCodeType == Reduction)
      {
      switch (vectorAPIOpCode)
         {
         case VECTOR_OP_ADD: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vreductionAdd, vectorType);
         case VECTOR_OP_MUL: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vreductionMul, vectorType);
         case VECTOR_OP_MIN: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vreductionMin, vectorType);
         case VECTOR_OP_MAX: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vreductionMax, vectorType);
         case VECTOR_OP_AND: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vreductionAnd, vectorType);
         case VECTOR_OP_OR:  return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vreductionOr,  vectorType);
         case VECTOR_OP_XOR: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vreductionXor, vectorType);
            // These don't seem to be generated by the library:
            // vreductionOrUnchecked
            // vreductionFirstNonZero
         default:
            return TR::BadILOp;
         }
      }
   else if (opCodeType == MaskReduction)
      {
      switch (vectorAPIOpCode)
         {
         case VECTOR_OP_MASK_TRUECOUNT: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::mTrueCount, vectorType);
         case VECTOR_OP_MASK_FIRSTTRUE: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::mFirstTrue, vectorType);
         case VECTOR_OP_MASK_LASTTRUE:  return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::mLastTrue, vectorType);
         case VECTOR_OP_MASK_TOLONG:    return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::mToLongBits, vectorType);
         default:
            return TR::BadILOp;
         }
      }
   else if (withMask)
      {
      switch (vectorAPIOpCode)
         {
         case VECTOR_OP_ABS: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmabs, vectorType);
         case VECTOR_OP_NEG: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmneg, vectorType);
         case VECTOR_OP_SQRT:return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmsqrt, vectorType);
         case VECTOR_OP_ADD: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmadd, vectorType);
         case VECTOR_OP_SUB: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmsub, vectorType);
         case VECTOR_OP_MUL: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmmul, vectorType);
         case VECTOR_OP_DIV: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmdiv, vectorType);
         case VECTOR_OP_MIN: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmmin, vectorType);
         case VECTOR_OP_MAX: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmmax, vectorType);
         case VECTOR_OP_AND: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmand, vectorType);
         case VECTOR_OP_OR:  return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmor, vectorType);
         case VECTOR_OP_XOR: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmxor, vectorType);
         case VECTOR_OP_FMA: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmfma, vectorType);

         case VECTOR_OP_LSHIFT:  return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmshl, vectorType);
         case VECTOR_OP_RSHIFT:  return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmshr, vectorType);
         case VECTOR_OP_URSHIFT: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmushr, vectorType);
         case VECTOR_OP_BIT_COUNT:     return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmpopcnt, vectorType);
         case VECTOR_OP_LROTATE:       return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmrol, vectorType);
         case VECTOR_OP_RROTATE:       return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmrol, vectorType);
         case VECTOR_OP_TZ_COUNT:      return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmnotz, vectorType);
         case VECTOR_OP_LZ_COUNT:      return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmnolz, vectorType);
         case VECTOR_OP_REVERSE:       return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmbitswap, vectorType);
         case VECTOR_OP_REVERSE_BYTES: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmbyteswap, vectorType);
         case VECTOR_OP_COMPRESS_BITS: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmcompressbits, vectorType);
         case VECTOR_OP_EXPAND_BITS:   return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmexpandbits, vectorType);

         default:
            return TR::BadILOp;
         // shiftLeftOpCode
         // shiftRightOpCode
         }
      }
   else
      {
      switch (vectorAPIOpCode)
         {
         case VECTOR_OP_ABS: return scalar ? TR::ILOpCode::absOpCode(elementType) : TR::ILOpCode::createVectorOpCode(TR::vabs, vectorType);
         case VECTOR_OP_NEG: return scalar ? TR::ILOpCode::negateOpCode(elementType) : TR::ILOpCode::createVectorOpCode(TR::vneg, vectorType);
         case VECTOR_OP_SQRT:return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vsqrt, vectorType);
         case VECTOR_OP_ADD: return scalar ? TR::ILOpCode::addOpCode(elementType, true) : TR::ILOpCode::createVectorOpCode(TR::vadd, vectorType);
         case VECTOR_OP_SUB: return scalar ? TR::ILOpCode::subtractOpCode(elementType) : TR::ILOpCode::createVectorOpCode(TR::vsub, vectorType);
         case VECTOR_OP_MUL: return scalar ? TR::ILOpCode::multiplyOpCode(elementType) : TR::ILOpCode::createVectorOpCode(TR::vmul, vectorType);
         case VECTOR_OP_DIV: return scalar ? TR::ILOpCode::divideOpCode(elementType) : TR::ILOpCode::createVectorOpCode(TR::vdiv, vectorType);
         case VECTOR_OP_MIN: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmin, vectorType);
         case VECTOR_OP_MAX: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmax, vectorType);
         case VECTOR_OP_AND: return scalar ? TR::ILOpCode::andOpCode(elementType) : TR::ILOpCode::createVectorOpCode(TR::vand, vectorType);
         case VECTOR_OP_OR:  return scalar ? TR::ILOpCode::orOpCode(elementType)  : TR::ILOpCode::createVectorOpCode(TR::vor, vectorType);
         case VECTOR_OP_XOR: return scalar ? TR::ILOpCode::xorOpCode(elementType) : TR::ILOpCode::createVectorOpCode(TR::vxor, vectorType);
         case VECTOR_OP_FMA: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vfma, vectorType);

         case VECTOR_OP_LSHIFT:  return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vshl, vectorType);
         case VECTOR_OP_RSHIFT:  return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vshr, vectorType);
         case VECTOR_OP_URSHIFT: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vushr, vectorType);
         case VECTOR_OP_BIT_COUNT:     return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vpopcnt, vectorType);
         case VECTOR_OP_LROTATE:       return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vrol, vectorType);
         case VECTOR_OP_RROTATE:       return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vrol, vectorType);
         case VECTOR_OP_COMPRESS:      return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vcompress, vectorType);
         case VECTOR_OP_EXPAND:        return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vexpand, vectorType);
         case VECTOR_OP_MASK_COMPRESS: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::mcompress, vectorType);
         case VECTOR_OP_TZ_COUNT:      return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vnotz, vectorType);
         case VECTOR_OP_LZ_COUNT:      return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vnolz, vectorType);
         case VECTOR_OP_REVERSE:       return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vbitswap, vectorType);
         case VECTOR_OP_REVERSE_BYTES: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vbyteswap, vectorType);
         case VECTOR_OP_COMPRESS_BITS: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vcompressbits, vectorType);
         case VECTOR_OP_EXPAND_BITS:   return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vexpandbits, vectorType);

         /* to be added
         case VECTOR_OP_TAN:           return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vtan, vectorType);
         case VECTOR_OP_TANH:          return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vtanh, vectorType);
         case VECTOR_OP_SIN:           return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vsin, vectorType);
         case VECTOR_OP_SINH:          return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vsinh, vectorType);
         case VECTOR_OP_COS:           return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vcos, vectorType);
         case VECTOR_OP_COSH:          return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vcosh, vectorType);
         case VECTOR_OP_ASIN:          return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vasin, vectorType);
         case VECTOR_OP_ACOS:          return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vacos, vectorType);
         case VECTOR_OP_ATAN:          return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vatan, vectorType);
         case VECTOR_OP_ATAN2:         return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vatan2, vectorType);
         case VECTOR_OP_CBRT:          return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vcbrt, vectorType);
         case VECTOR_OP_LOG:           return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vlog, vectorType);
         case VECTOR_OP_LOG10:         return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vlog10, vectorType);
         case VECTOR_OP_LOG1P:         return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vlog1p, vectorType);
         case VECTOR_OP_POW:           return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vpow, vectorType);
         case VECTOR_OP_EXP:           return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vexp, vectorType);
         case VECTOR_OP_EXPM1:         return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vexpm1, vectorType);
         case VECTOR_OP_HYPOT:         return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vhypot, vectorType);
         */

         default:
            return TR::BadILOp;
         // shiftLeftOpCode
         // shiftRightOpCode
         }
      }
   return TR::BadILOp;
   }

TR::Node *TR_VectorAPIExpansion::transformNary(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                               TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                               handlerMode mode,
                                               TR::ILOpCodes scalarOpCode, TR::ILOpCodes vectorOpCode, int32_t firstOperand,
                                               int32_t numOperands, vapiOpCodeType opCodeType,
                                               bool transformROR)
   {
   TR::Compilation *comp = opt->comp();

   // Since we are modifying node's children in place
   // it's better to save the original ones
   TR_ASSERT_FATAL(numOperands <= _maxNumberOperands, "number of operands exceeds %d\n", _maxNumberOperands);

   TR::Node *operands[_maxNumberOperands];
   for (int32_t i = 0; i < numOperands; i++)
      {
      operands[i] = node->getChild(firstOperand + i);
      }

   if (mode == doScalarization)
      {
      anchorOldChildren(opt, treeTop, node);

      int32_t elementSize = OMR::DataType::getSize(elementType);

      for (int32_t i = 0; i < numOperands; i++)
         {
         if (operands[i]->getOpCodeValue() == TR::aload)
            aloadHandler(opt, treeTop, operands[i], elementType, vectorLength, numLanes, mode);
         }

      for (int32_t i = 0; i < numOperands; i++)
         {
         node->setAndIncChild(i, operands[i]);
         }
      node->setNumChildren(numOperands);
      TR::Node::recreate(node, scalarOpCode);

      for (int32_t i = 1; i < numLanes; i++)
         {
         TR::Node *newNode = TR::Node::create(node, scalarOpCode, numOperands);
         addScalarNode(opt, node, numLanes, i, newNode);
         for (int32_t j = 0; j < numOperands; j++)
            {
            newNode->setAndIncChild(j, getScalarNode(opt, operands[j], i));
            }
         }
      }
   else if (mode == doVectorization)
      {
      TR::DataType vectorType = TR::DataType::createVectorType(elementType, vectorLength);

      for (int32_t i = 0; i < numOperands; i++)
         {
         TR::Node *operand = operands[i];

         if (operand->getOpCodeValue() == TR::aload)
            {
            TR::DataType opCodeType = vectorType;
            TR::SymbolReference *operandSymRef = operand->getSymbolReference();
            int32_t operandId = operandSymRef->getReferenceNumber();

            if (opt->_aliasTable[operandId]._objectType == Mask)
               {
               opCodeType = TR::DataType::createMaskType(elementType, vectorLength);
               }
            vectorizeLoadOrStore(opt, operand, opCodeType);
            }
         else if (operand->getOpCodeValue() == TR::acall)
            {
            TR::DataType opCodeType = vectorType;

            if (opt->_nodeTable[operand->getGlobalIndex()]._objectType == Mask)
               {
               opCodeType = TR::DataType::createMaskType(elementType, vectorLength);
               }
            vectorizeLoadOrStore(opt, operand, opCodeType);
            }
         }


      TR_ASSERT_FATAL(vectorOpCode != TR::BadILOp, "Vector opcode should exist for node %p\n", node);

      anchorOldChildren(opt, treeTop, node);

      TR::Node *vectorNode;

      if (opCodeType == Reduction && elementType != TR::Int64)
         {
         // reductionCoersed intrinsic returns Long but reduction opcode has vector element type
         TR::ILOpCodes convOpCode = TR::BadILOp;

         switch (elementType)
            {
            case TR::Int8:
            case TR::Int16:
            case TR::Int32:
               convOpCode = TR::ILOpCode::getDataTypeConversion(elementType, TR::Int64);
               break;
            case TR::Float:
               convOpCode = TR::i2l;  // will have fbits2i as a child
               break;
            case TR::Double:
               convOpCode = TR::ILOpCode::getDataTypeBitConversion(TR::Double, TR::Int64);
               break;
            default:
               TR_ASSERT_FATAL(false, "Wrong vector element type for reduction operation\n");
            }

         TR::Node::recreate(node, convOpCode);

         vectorNode = TR::Node::create(node, vectorOpCode, numOperands);
         TR::Node *childNode = vectorNode;

         if (elementType == TR::Float)
            {
            childNode = TR::Node::create(node, TR::ILOpCode::getDataTypeBitConversion(TR::Float, TR::Int32), 1);
            childNode->setAndIncChild(0, vectorNode);
            }

         node->setAndIncChild(0, childNode);
         node->setNumChildren(1);
         }
      else
         {
         TR::Node::recreate(node, vectorOpCode);
         vectorNode = node;
         }

      for (int32_t i = 0; i < numOperands; i++)
         {
         vectorNode->setAndIncChild(i, operands[i]);
         }
      vectorNode->setNumChildren(numOperands);


      if (transformROR)
         {
         // vrol was already generated, need to change the shift amount
         node->setAndIncChild(1, transformRORtoROL(opt, node->getChild(1), elementType, vectorLength, opCodeType));
         }

      if (opCodeType == BroadcastInt)
         {
         // broadcast second child (int type)
         TR::ILOpCodes splatsOpCode = TR::ILOpCode::createVectorOpCode(TR::vsplats, vectorType);
         TR::Node *splatsNode = TR::Node::create(node, splatsOpCode, 1);
         TR::Node *scalarNode =  node->getSecondChild();

         if (elementType != TR::Int32)
            {
            TR::ILOpCodes convOpCode = TR::ILOpCode::getDataTypeConversion(TR::Int32, elementType);
            scalarNode->decReferenceCount();
            scalarNode = TR::Node::create(node, convOpCode, 1, scalarNode);
            splatsNode->setAndIncChild(0, scalarNode);
            }
         else
            {
            splatsNode->setChild(0, scalarNode);
            }

         vectorNode->setAndIncChild(1, splatsNode);
         }

      }

   return node;
}


// high level methods are disabled because they require exception handling
TR_VectorAPIExpansion::methodTableEntry
TR_VectorAPIExpansion::methodTable[] =
   {
   {loadIntrinsicHandler,                 Unknown, 1, 2, -1, 0, -1, {Unknown, ElementType, NumLanes}},                                           // jdk_internal_vm_vector_VectorSupport_load
#if JAVA_SPEC_VERSION <= 21
   {storeIntrinsicHandler,                Unknown, 1, 2,  5, 1, -1, {Unknown, ElementType, NumLanes, Unknown, Unknown, Vector}},                 // jdk_internal_vm_vector_VectorSupport_store
#else
   {storeIntrinsicHandler,                Unknown, 1, 2,  6, 1, -1, {Unknown, ElementType, NumLanes, Unknown, Unknown, Unknown, Vector}},        // jdk_internal_vm_vector_VectorSupport_store
#endif
   {binaryIntrinsicHandler,               Unknown,  3, 4,  5, 2,  7, {Unknown, Unknown, Unknown, ElementType, NumLanes, Vector, Vector, Mask}},   // jdk_internal_vm_vector_VectorSupport_binaryOp
   {blendIntrinsicHandler,                Vector,  2, 3,  4, 3, -1, {Unknown, Unknown, ElementType, NumLanes, Vector, Vector, Mask, Unknown}}, // jdk_internal_vm_vector_VectorSupport_blend
   {broadcastIntIntrinsicHandler,         Vector,  3, 4,  5, 2,  7, {Unknown, Unknown, Unknown, ElementType, NumLanes, Vector, Unknown, Mask}},  //jdk_internal_vm_vector_VectorSupport_broadcastInt
   {compareIntrinsicHandler,              Mask,    3, 4,  5, 2,  7, {Unknown, Unknown, Unknown, ElementType, NumLanes, Vector, Vector, Mask}},   // jdk_internal_vm_vector_VectorSupport_compare
   {compressExpandOpIntrinsicHandler,     Unknown, 3, 4,  5, 2, -1, {Unknown, Unknown, Unknown, ElementType, NumLanes, Vector, Mask}},           // jdk_internal_vm_vector_VectorSupport_compressExpandOp
   {convertIntrinsicHandler,              Vector,  2, 3,  7, 1, -1, {Unknown, Unknown, ElementType, NumLanes, Unknown, Unknown, Unknown, Vector}},   // jdk_internal_vm_vector_VectorSupport_convert
   {fromBitsCoercedIntrinsicHandler,      Unknown, 1, 2, -1, 0, -1, {Unknown, ElementType, NumLanes, Unknown, Unknown, Unknown}},                // jdk_internal_vm_vector_VectorSupport_fromBitsCoerced
   {maskReductionCoercedIntrinsicHandler, Scalar,  2, 3,  4, 1, -1, {Unknown, Unknown, ElementType, NumLanes, Mask}},                            // jdk_internal_vm_vector_VectorSupport_maskReductionCoerced
   {reductionCoercedIntrinsicHandler,     Scalar,  3, 4,  5, 1,  6, {Unknown, Unknown, Unknown, ElementType, NumLanes, Vector, Mask}},           // jdk_internal_vm_vector_VectorSupport_reductionCoerced
   {ternaryIntrinsicHandler,              Vector,  3, 4,  5, 3,  8, {Unknown, Unknown, Unknown, ElementType, NumLanes, Vector, Vector, Vector, Mask}},  // jdk_internal_vm_vector_VectorSupport_ternaryOp
   {testIntrinsicHandler,                 Scalar,  2, 3,  4, 1,  5, {Unknown, Unknown, ElementType, NumLanes, Mask, Mask, Unknown}},             // jdk_internal_vm_vector_VectorSupport_test
   {unaryIntrinsicHandler,                Vector,  3, 4,  5, 1,  6, {Unknown, Unknown, Unknown, ElementType, NumLanes, Vector, Mask}},           // jdk_internal_vm_vector_VectorSupport_unaryOp
   };


TR_VectorAPIExpansion::TR_VectorAPIExpansion(TR::OptimizationManager *manager)
                      : TR::Optimization(manager), _trace(false), _aliasTable(trMemory()), _nodeTable(trMemory()),
                        _classByte128Vector(NULL), _classByte128Mask(NULL)
   {
   static_assert(sizeof(methodTable) / sizeof(methodTable[0]) == _numMethods,
                 "methodTable should contain recognized methods between TR::FirstVectorMethod and TR::LastVectorMethod");
   }


//TODOs:
// 1) Use getFirstOperandIndex in all handlers instead of the hardcoded numbers
// 4) handle OSR guards
// 6) make scalarization and vectorization coexist in one web
// 7) handle all intrinsics
// 8) box vector objects if passed to unvectorized methods
// 10) cost-benefit analysis for boxing
// 11) implement useDef based approach
// 12) handle methods that return vector type different from the argument
// 13) OPT: optimize java/util/Objects.checkIndex
// 15) OPT: too many aliased temps (reused privatized args) that cause over-aliasing after GVP
// 16) OPT: add to other opt levels
// 18) OPT: jdk/incubator/vector/VectorOperators.opCode() is not inlined with Byte,Short,Long add
// 20) Fix TR_ALLOC
