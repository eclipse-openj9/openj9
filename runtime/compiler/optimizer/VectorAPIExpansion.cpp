/*******************************************************************************
 * Copyright (c) 2021, 2022 IBM Corp. and others
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
#include "codegen/CodeGenerator.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "optimizer/VectorAPIExpansion.hpp"


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

bool
TR_VectorAPIExpansion::returnsVector(TR::MethodSymbol * methodSymbol)
   {
   if (!isVectorAPIMethod(methodSymbol)) return false;

   TR::RecognizedMethod index = methodSymbol->getRecognizedMethod();

   return methodTable[index - _firstMethod]._returnType == Vector;
   }

TR::DataType
TR_VectorAPIExpansion::dataType(TR::MethodSymbol * methodSymbol)
   {
   if (!isVectorAPIMethod(methodSymbol)) return TR::NoType;

   TR::RecognizedMethod index = methodSymbol->getRecognizedMethod();

   return methodTable[index - _firstMethod]._elementType;
   }

bool
TR_VectorAPIExpansion::isArgType(TR::MethodSymbol *methodSymbol, int32_t i, vapiArgType type)
   {
   if (!isVectorAPIMethod(methodSymbol)) return false;

   TR::RecognizedMethod index = methodSymbol->getRecognizedMethod();

   if (i < 0) return false;

   TR_ASSERT_FATAL(i < _numArguments, "Argument index %d is too big", i);
   return (methodTable[index - _firstMethod]._argumentTypes[i] == type);
   }

void
TR_VectorAPIExpansion::invalidateSymRef(TR::SymbolReference *symRef)
   {
   int32_t id = symRef->getReferenceNumber();
   _aliasTable[id]._classId = -1;
   }

void
TR_VectorAPIExpansion::alias(TR::Node *node1, TR::Node *node2)
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
      traceMsg(comp(), "%s aliasing symref #%d to symref #%d (nodes %p %p)\n", OPT_DETAILS_VECTOR, id1, id2, node1, node2);

   _aliasTable[id1]._aliases->set(id2);
   _aliasTable[id2]._aliases->set(id1);
   }


void
TR_VectorAPIExpansion::buildVectorAliases()
   {
   if (_trace)
      traceMsg(comp(), "%s Aliasing symrefs\n", OPT_DETAILS_VECTOR);

   _visitedNodes.empty();

   for (TR::TreeTop *tt = comp()->getMethodSymbol()->getFirstTreeTop(); tt ; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      TR::ILOpCodes opCodeValue = node->getOpCodeValue();

      if (opCodeValue == TR::treetop || opCodeValue == TR::NULLCHK)
          {
          node = node->getFirstChild();
          }

      visitNodeToBuildVectorAliases(node);
      }
   }


void
TR_VectorAPIExpansion::visitNodeToBuildVectorAliases(TR::Node *node)
   {
   if (_visitedNodes.isSet(node->getGlobalIndex()))
      return;
   _visitedNodes.set(node->getGlobalIndex());


   TR::ILOpCode opCode = node->getOpCode();
   TR::ILOpCodes opCodeValue = node->getOpCodeValue();

   if (opCodeValue == TR::astore || opCodeValue == TR::astorei)
      {
      if (!node->chkStoredValueIsIrrelevant())
         {
         TR::Node *rhs = (opCodeValue == TR::astore) ? node->getFirstChild() : node->getSecondChild();
         if (rhs->getOpCode().hasSymbolReference())
            {
            alias(node, rhs);
            }
         else
            {
            if (_trace)
               traceMsg(comp(), "Invalidating #%p due to rhs %p in node %p\n", node->getSymbolReference()->getReferenceNumber(), rhs, node);
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

      for (int32_t i = 0; i < numChildren; i++)
         {
         if (!isVectorAPIMethod(methodSymbol) || isArgType(methodSymbol, i, Vector))
            {
            TR::Node *child = node->getChild(i);
            if (child->getOpCode().hasSymbolReference())
               {
               alias(node, child);
               }
            else
               {
               if (_trace)
                  traceMsg(comp(), "Invalidating #%d due to child %p in node %p\n",
                           node->getSymbolReference()->getReferenceNumber(), child, node);
               invalidateSymRef(node->getSymbolReference());
               }
            }

         if (!isVectorAPIMethod(methodSymbol))
            {
            if (_trace)
               traceMsg(comp(), "Invalidating #%d since it's not a vector API method in node %p\n",
                     node->getSymbolReference()->getReferenceNumber(), node);
            invalidateSymRef(node->getSymbolReference());
            continue;
            }

         // Update type and length
         if (isArgType(methodSymbol, i, Species))
            {
            vec_sz_t methodLen = _aliasTable[methodRefNum]._vecLen;

            TR::Node *speciesNode = node->getChild(i);
            int32_t speciesRefNum = speciesNode->getSymbolReference()->getReferenceNumber();
            vec_sz_t speciesLen;

            if (_aliasTable[speciesRefNum]._vecLen == vec_len_default)
               {
               speciesLen = getVectorSizeFromVectorSpecies(speciesNode);
               if (_trace)
                  traceMsg(comp(), "%snode n%dn (#%d) was updated with vecLen : %d\n",
                               OPT_DETAILS_VECTOR, speciesNode->getGlobalIndex(), speciesRefNum, speciesLen);
               }
            else
               {
               speciesLen = _aliasTable[speciesRefNum]._vecLen;
               }

            if (methodLen != vec_len_default && speciesLen != methodLen)
               {
               if (_trace)
                  traceMsg(comp(), "%snode n%dn (#%d) species are %d but method is : %d\n",
                                   OPT_DETAILS_VECTOR, node->getGlobalIndex(), methodRefNum, speciesLen, methodLen);
               speciesLen = vec_len_unknown;
               }

            _aliasTable[methodRefNum]._vecLen = speciesLen;

            if (_trace)
               traceMsg(comp(), "%snode n%dn (#%d) was updated with vecLen : %d\n",
                            OPT_DETAILS_VECTOR, node->getGlobalIndex(), methodRefNum, speciesLen);

            methodElementType = dataType(methodSymbol);
            int32_t elementSize = OMR::DataType::getSize(methodElementType);
            methodNumLanes = speciesLen/8/elementSize;
            }
         else if (isArgType(methodSymbol, i, elementType))
            {
            TR::Node *elementTypeNode = node->getChild(i);
            methodElementType = getDataTypeFromClassNode(comp(), elementTypeNode);
            _aliasTable[methodRefNum]._elementType = methodElementType;

            }
         else if (isArgType(methodSymbol, i, numLanes))
            {
            TR::Node *numLanesNode = node->getChild(i);
            _aliasTable[methodRefNum]._vecLen = vec_len_unknown;
            if (numLanesNode->getOpCode().isLoadConst())
               {
               methodNumLanes = numLanesNode->get32bitIntegralValue();
               if (methodElementType != TR::NoType)  // type was seen but could've been non-const
                  {
                  int32_t elementSize = OMR::DataType::getSize(methodElementType);
                  _aliasTable[methodRefNum]._vecLen = methodNumLanes*8*elementSize;
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
         if (_trace)
            traceMsg(comp(), "Invalidating #%d due to unknown elementType=%d, numLanes=%d in node %p\n",
                     node->getSymbolReference()->getReferenceNumber(), (int)methodElementType, methodNumLanes, node);
         invalidateSymRef(node->getSymbolReference());
         }
      else
         {
         int32_t elementSize = OMR::DataType::getSize(methodElementType);
         vec_sz_t vectorLength = methodNumLanes*8*elementSize;

         bool canVectorize = methodTable[handlerIndex]._methodHandler(this, NULL, node, methodElementType, vectorLength,
                                                                      checkVectorization);
         bool canScalarize = methodTable[handlerIndex]._methodHandler(this, NULL, node, methodElementType, vectorLength,
                                                                         checkScalarization);
         if (!canVectorize)
            {
            if (_trace)
               traceMsg(comp(), "Can't vectorize #%d due to unsupported opcode in node %p\n",
                                 node->getSymbolReference()->getReferenceNumber(), node);

            _aliasTable[methodRefNum]._cantVectorize = true;

            if (!canScalarize)
               {
               _aliasTable[methodRefNum]._cantScalarize = true;
               if (_trace)
                  traceMsg(comp(), "Invalidating #%d due to unsupported opcode in node %p\n",
                           node->getSymbolReference()->getReferenceNumber(), node);
               invalidateSymRef(node->getSymbolReference());
               }
            }
         else if (!canScalarize)
            {
            if (_trace)
               traceMsg(comp(), "Can't scalarize #%d due to unsupported opcode in node %p\n",
                                 node->getSymbolReference()->getReferenceNumber(), node);

            _aliasTable[methodRefNum]._cantScalarize = true;
            }
         }
      }
   else if (opCode.isLoadAddr())
      {
      if (_trace)
         traceMsg(comp(), "Invalidating #%d due to loadaddr node %p\n", node->getSymbolReference()->getReferenceNumber(), node);
      invalidateSymRef(node->getSymbolReference());
      }
   else if (opCode.isArrayRef() ||
            opCode.isLoadIndirect() ||
            opCode.isStoreIndirect() ||
            node->getOpCodeValue() == TR::areturn ||
            node->getOpCodeValue() == TR::aRegStore)
      {
      TR::Node *child = node->getFirstChild();
      if (child->getOpCode().hasSymbolReference())
         {
         if (_trace)
            traceMsg(comp(), "Invalidating #%d due to its address used by %p\n",
                             child->getSymbolReference()->getReferenceNumber(), node);
         invalidateSymRef(child->getSymbolReference());
         }
      }
   else
      {
      for (int32_t i = 0; i < node->getNumChildren(); i++)
         {
         TR::Node *child = node->getChild(i);
         if (child->getOpCode().hasSymbolReference())
            {
            if (_trace)
               traceMsg(comp(), "Invalidating #%d since it's used by unsupported node %p\n",
                                 child->getSymbolReference()->getReferenceNumber(), node);
             invalidateSymRef(child->getSymbolReference());
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
      visitNodeToBuildVectorAliases(node->getChild(i));
      }
   }


void
TR_VectorAPIExpansion::findAllAliases(int32_t classId, int32_t id)
   {
   if (_aliasTable[id]._aliases == NULL)
      {
      TR_ASSERT_FATAL(_aliasTable[id]._classId <= 0 , "#%d should have class -1 or 0, but it's %d\n",
                                                              id, _aliasTable[id]._classId);

      if (_aliasTable[id]._classId == 0)
          _aliasTable[id]._classId = id;  // in their own empty class
      return;
      }
   if (_trace)
      {
       traceMsg(comp(), "Iterating through aliases for #%d:\n", id);
      _aliasTable[id]._aliases->print(comp());
      traceMsg(comp(), "\n");
      }

   // we need to create a new bit vector so that we don't iterate and modify at the same time
   TR_BitVector *aliasesToIterate = (classId == id) ? new (comp()->trStackMemory()) TR_BitVector(*_aliasTable[id]._aliases)
                                                    : _aliasTable[id]._aliases;

   TR_BitVectorIterator bvi(*aliasesToIterate);

   while (bvi.hasMoreElements())
      {
      int32_t i = bvi.getNextElement();

      if (_aliasTable[i]._classId > 0)
         {
         TR_ASSERT_FATAL(_aliasTable[i]._classId == classId, "#%d should belong to class %d but it belongs to class %d\n",
                     i, classId, _aliasTable[i]._classId );
         continue;
         }
      _aliasTable[classId]._aliases->set(i);

      if (_aliasTable[i]._classId == -1)
         {
         if (_trace)
            traceMsg(comp(), "Invalidating the whole class #%d due to #%d\n", classId, i);
         _aliasTable[classId]._classId = -1; // invalidate the whole class
         }

      if (_aliasTable[i]._classId != -1 || i != classId)
         {
         if (_trace)
            traceMsg(comp(), "Set class %d for symref #%d\n", classId, i);
         _aliasTable[i]._classId = classId;
         }

      if (i != classId)
          findAllAliases(classId, i);
      }


   }


void
TR_VectorAPIExpansion::buildAliasClasses()
   {
   if (_trace)
      traceMsg(comp(), "%s Building alias classes\n", OPT_DETAILS_VECTOR);

   int32_t symRefCount = comp()->getSymRefTab()->getNumSymRefs();

   for (int32_t i = 0; i < symRefCount; i++)
      {
      if (_aliasTable[i]._classId <= 0)
          findAllAliases(i, i);
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
         TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());
         TR::VMAccessCriticalSection getVectorSizeFromVectorSpeciesSection(fej9);

         uintptr_t vectorSpeciesLocation = comp()->getKnownObjectTable()->getPointer(vSpeciesSymRef->getKnownObjectIndex());
         uintptr_t vectorShapeLocation = fej9->getReferenceField(vectorSpeciesLocation, "vectorShape", "Ljdk/incubator/vector/VectorShape;");
         int32_t vectorBitSize = fej9->getInt32Field(vectorShapeLocation, "vectorBitSize");
         return (vec_sz_t)vectorBitSize;
         }
      }
      return vec_len_unknown;
   }

TR::DataType
TR_VectorAPIExpansion::getDataTypeFromClassNode(TR::Compilation *comp, TR::Node *classNode)
   {
   TR::SymbolReference *symRef = classNode->getSymbolReference();
   if (symRef)
      {
      if (symRef->hasKnownObjectIndex())
         {
         TR_J9VMBase *fej9 = comp->fej9();

         TR::VMAccessCriticalSection getDataTypeFromClassNodeSection(fej9);

         uintptr_t javaLangClass = comp->getKnownObjectTable()->getPointer(symRef->getKnownObjectIndex());
         J9Class *j9class = (J9Class *)(intptr_t)fej9->getInt64Field(javaLangClass, "vmRef");
         J9JavaVM *vm = fej9->getJ9JITConfig()->javaVM;

         if (j9class == vm->floatReflectClass)
            return TR::Float;
         else if (j9class == vm->doubleReflectClass)
            return TR::Double;
         else if (j9class == vm->byteReflectClass)
            return TR::Int8;
         else if (j9class == vm->shortReflectClass)
            return TR::Int16;
         else if (j9class == vm->intReflectClass)
            return TR::Int32;
         else if (j9class == vm->longReflectClass)
            return TR::Int64;
         }
      }
   return TR::NoType;
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

      if (opCodeValue == TR::treetop || opCodeValue == TR::NULLCHK)
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
TR_VectorAPIExpansion::validateSymRef(int32_t id, int32_t i, vec_sz_t &classLength, TR::DataType &classType)
   {
   TR::SymbolReference *symRef = comp()->getSymRefTab()->getSymRef(i);

   if (!symRef || !symRef->getSymbol())
      return false;

   if (_aliasTable[i]._classId == -1)
      {
      if (_trace)
         traceMsg(comp(), "%s invalidating1 class #%d due to symref #%d\n", OPT_DETAILS_VECTOR, id, i);
      return false;
      }
   else if (symRef->getSymbol()->isShadow() ||
             symRef->getSymbol()->isStatic() ||
             symRef->getSymbol()->isParm())
      {
      if (_trace)
         traceMsg(comp(), "%s invalidating2 class #%d due to symref #%d\n", OPT_DETAILS_VECTOR, id, i);
      return false;
      }
   else if (symRef->getSymbol()->isMethod())
      {
      TR::MethodSymbol * methodSymbol = symRef->getSymbol()->castToMethodSymbol();

      if (!isVectorAPIMethod(methodSymbol))
         {
         if (_trace)
            traceMsg(comp(), "%s invalidating3 class #%d due to non-API method #%d\n", OPT_DETAILS_VECTOR, id, i);
         return false;
         }

      vec_sz_t methodLength = _aliasTable[i]._vecLen;
      TR::DataType methodType = _aliasTable[i]._elementType;

      if (classLength == vec_len_default)
         {
         classLength = methodLength;
         }
      else if (methodLength != vec_len_default &&
               methodLength != classLength)
         {
         if (_trace)
            traceMsg(comp(), "%s invalidating5 class #%d due to symref #%d method length %d, seen length %d\n",
                               OPT_DETAILS_VECTOR, id, i, methodLength, classLength);
         return false;
         }

      if (classType == TR::NoType)
         {
         classType = methodType;
         }
      else if (methodType != TR::NoType &&
               methodType != classType)
         {
         if (_trace)
            traceMsg(comp(), "%s invalidating6 class #%d due to symref #%d method type %d, seen type %d\n",
                     OPT_DETAILS_VECTOR, id, i, (int)methodType, (int)classType);
         return false;
         }
      }
      return true;
   }


void
TR_VectorAPIExpansion::validateVectorAliasClasses()
   {
   if (_trace)
      traceMsg(comp(), "%s Validating alias classes\n", OPT_DETAILS_VECTOR);

   int32_t symRefCount = comp()->getSymRefTab()->getNumSymRefs();

   for (int32_t id = 1; id < symRefCount; id++)
      {
      if (_aliasTable[id]._classId != id)
         continue;  // not an alias class or is already invalid

      if (_aliasTable[id]._aliases && _trace)
         {
         traceMsg(comp(), "Verifying class: %d\n", id);
         _aliasTable[id]._aliases->print(comp());
         traceMsg(comp(), "\n");
         }

      bool vectorClass = true;
      vec_sz_t classLength = vec_len_default;
      TR::DataType classType = TR::NoType;

      if (!_aliasTable[id]._aliases)
         {
         // class might consist of just the symref itself
         vectorClass = validateSymRef(id, id, classLength, classType);
         }
      else
         {
         TR_BitVectorIterator bvi(*_aliasTable[id]._aliases);
         while (bvi.hasMoreElements())
            {
            int32_t i = bvi.getNextElement();
            vectorClass = validateSymRef(id, i, classLength, classType);
            if (!vectorClass)
               break;

            if (_aliasTable[i]._cantVectorize)
               {
               if (_trace)
                  traceMsg(comp(), "Class #%d can't be vectorized due to #%d\n", id, i);

               _aliasTable[id]._cantVectorize = true;
               }

            if (_aliasTable[i]._cantScalarize)
               {
               if (_trace)
                  traceMsg(comp(), "Class #%d can't be scalarized due to #%d\n", id, i);
               _aliasTable[id]._cantScalarize = true;
               }
            }
         }

      // update class vector length and element type
      _aliasTable[id]._vecLen = classLength;
      _aliasTable[id]._elementType = classType;


      if (vectorClass &&
          classLength != vec_len_unknown &&
          classLength != vec_len_default)
         continue;

      // invalidate the whole class
      if (_trace && _aliasTable[id]._aliases)  // to reduce number of messages
         traceMsg(comp(), "Invalidating class #%d\n", id);
      _aliasTable[id]._classId = -1;
      }
   }


int32_t
TR_VectorAPIExpansion::expandVectorAPI()
   {
   if (_trace)
      traceMsg(comp(), "%s In expandVectorAPI\n", OPT_DETAILS_VECTOR);

   buildVectorAliases();
   buildAliasClasses();
   validateVectorAliasClasses();

   if (_trace)
      traceMsg(comp(), "%s Starting Expansion\n", OPT_DETAILS_VECTOR);

   _seenClasses.empty();

   for (TR::TreeTop *treeTop = comp()->getMethodSymbol()->getFirstTreeTop(); treeTop ; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      TR::ILOpCodes opCodeValue = node->getOpCodeValue();
      TR::Node *parent = NULL;
      TR::MethodSymbol *methodSymbol = NULL;

      if (opCodeValue == TR::treetop || opCodeValue == TR::NULLCHK)
          {
          parent = node;
          node = node->getFirstChild();
          opCodeValue = node->getOpCodeValue();
          }

      TR::ILOpCode opCode = node->getOpCode();

      if (opCodeValue != TR::astore && !opCode.isFunctionCall())
         continue;

      if (node->chkStoredValueIsIrrelevant())
         continue;

      if (opCode.isFunctionCall())
         {
         methodSymbol = node->getSymbolReference()->getSymbol()->castToMethodSymbol();

         if (!isVectorAPIMethod(methodSymbol))
            continue;
         }

      TR_ASSERT_FATAL(node->getOpCode().hasSymbolReference(), "Node %p should have symbol reference\n", node);

      int32_t classId = _aliasTable[node->getSymbolReference()->getReferenceNumber()]._classId;

      if (_trace)
         traceMsg(comp(), "#%d classId = %d\n", node->getSymbolReference()->getReferenceNumber(), classId);

      if (classId <= 0)
         continue;

      if (_trace)
         traceMsg(comp(), "#%d classId._classId = %d\n", node->getSymbolReference()->getReferenceNumber(), _aliasTable[classId]._classId);

      if (_aliasTable[classId]._classId == -1)  // class was invalidated
         continue;

      handlerMode checkMode = checkVectorization;
      handlerMode doMode = doVectorization;

      if (_aliasTable[classId]._cantVectorize)
         {
         TR_ASSERT_FATAL(!_aliasTable[classId]._cantScalarize, "Class %d should be either vectorizable or scalarizable",
                                                                classId);
         checkMode = checkScalarization;
         doMode = doScalarization;
         }

      if (!_seenClasses.isSet(classId))
         {
         _seenClasses.set(classId);


         //printf("%s Starting to %s class #%d\n", optDetailString(), doMode == doVectorization ? "vectorize" : "scalarize", classId);

         if (!performTransformation(comp(), "%s Starting to %s class #%d\n", optDetailString(),
                                             doMode == doVectorization ? "vectorize" : "scalarize",
                                             classId))
            {
            _aliasTable[classId]._classId = -1; // invalidate the whole class
            continue;
            }
         }

      if (_trace)
         traceMsg(comp(), "Transforming node %p of class %d\n", node, classId);

      TR::DataType elementType = _aliasTable[classId]._elementType;
      int32_t vectorLength = _aliasTable[classId]._vecLen;

      if (opCodeValue == TR::astore)
         {
         if (_trace)
            traceMsg(comp(), "handling astore %p\n", node);
         astoreHandler(this, treeTop, node, elementType, vectorLength, doMode);
         }
      else if (opCode.isFunctionCall())
         {
         TR_ASSERT_FATAL(parent, "All VectorAPI calls are expected to have a treetop");

         TR::RecognizedMethod index = methodSymbol->getRecognizedMethod();
         int32_t handlerIndex = index - _firstMethod;

         TR_ASSERT_FATAL(methodTable[handlerIndex]._methodHandler(this, treeTop, node, elementType, vectorLength, checkMode),
                         "Analysis should've proved that method is supported");

         TR::Node::recreate(parent, TR::treetop);
         methodTable[handlerIndex]._methodHandler(this, treeTop, node, elementType, vectorLength, doMode);
         }

      if (doMode == doScalarization)
         {
         int32_t elementSize = OMR::DataType::getSize(elementType);
         int32_t numLanes = vectorLength/8/elementSize;

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

   if (_trace)
      comp()->dumpMethodTrees("After Vectorization");

   return 1;
   }

//
// static transformation routines
//

void
TR_VectorAPIExpansion::vectorizeLoadOrStore(TR_VectorAPIExpansion *opt, TR::Node *node, TR::DataType type)
   {
   TR::Compilation *comp = opt->comp();

   TR_ASSERT_FATAL(node->getOpCode().hasSymbolReference(), "%s node %p should have symbol reference", OPT_DETAILS_VECTOR, node);

   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::SymbolReference *vecSymRef = (opt->_aliasTable)[symRef->getReferenceNumber()]._vecSymRef;
   if (vecSymRef == NULL)
      {
      vecSymRef = comp->cg()->allocateLocalTemp(type);
      (opt->_aliasTable)[symRef->getReferenceNumber()]._vecSymRef = vecSymRef;
      if (opt->_trace)
         traceMsg(comp, "   created new vector symRef #%d for #%d\n", vecSymRef->getReferenceNumber(), symRef->getReferenceNumber());

      }
   if (node->getOpCode().isStore())
      TR::Node::recreate(node, TR::vstore);
   else
      TR::Node::recreate(node, TR::vload);

   node->setSymbolReference(vecSymRef);
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
TR_VectorAPIExpansion::generateAddressNode(TR::Node *array, TR::Node *arrayIndex, int32_t elementSize)
   {
   int32_t shiftAmount = 0;
   while ((elementSize = (elementSize >> 1)))
        ++shiftAmount;


   TR::Node *i2lNode = TR::Node::create(TR::i2l, 1);
   i2lNode->setAndIncChild(0, arrayIndex);

   TR::Node *lshlNode = TR::Node::create(TR::lshl, 2);
   lshlNode->setAndIncChild(0, i2lNode);
   lshlNode->setAndIncChild(1, TR::Node::create(TR::iconst, 0, shiftAmount));

   TR::Node *laddNode = TR::Node::create(TR::ladd, 2);
   laddNode->setAndIncChild(0, lshlNode);

   int32_t arrayHeaderSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
   laddNode->setAndIncChild(1, TR::Node::create(TR::lconst, 0, arrayHeaderSize));

   TR::Node *aladdNode = TR::Node::create(TR::aladd, 2);
   aladdNode->setAndIncChild(0, array);
   aladdNode->setAndIncChild(1, laddNode);

   return aladdNode;
   }


void TR_VectorAPIExpansion::aloadHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                          TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode)
   {
   TR::Compilation *comp = opt->comp();

   if (mode == doScalarization)
      {
      int32_t elementSize = OMR::DataType::getSize(elementType);
      int32_t numLanes = vectorLength/8/elementSize;
      int32_t id = node->getSymbolReference()->getReferenceNumber();

      scalarizeLoadOrStore(opt, node, elementType, numLanes);

      TR_Array<TR::SymbolReference*>  *scalarSymRefs = (opt->_aliasTable)[id]._scalarSymRefs;
      TR_ASSERT_FATAL(scalarSymRefs, "reference should not be NULL");

      for (int32_t i = 1; i < numLanes; i++)
         {
         TR_ASSERT_FATAL((*scalarSymRefs)[i], "reference should not be NULL");
         TR::Node *loadNode = TR::Node::createWithSymRef(node, comp->il.opCodeForDirectLoad(elementType), 0, (*scalarSymRefs)[i]);
         addScalarNode(opt, node, numLanes, i, loadNode);

         }
      }
   else if (mode == doVectorization)
      {
      TR::VectorLength lengthEnum = supportedOnPlatform(comp, vectorLength);
      TR::DataType vectorType = OMR::DataType(elementType).scalarToVector(lengthEnum);
      vectorizeLoadOrStore(opt, node, vectorType);
      }

   return;
   }


void TR_VectorAPIExpansion::astoreHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                          TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode)
   {
   TR::Compilation *comp = opt->comp();

   TR::Node *rhs = node->getFirstChild();

   if (mode == doScalarization)
      {
      int32_t elementSize = OMR::DataType::getSize(elementType);
      int32_t numLanes = vectorLength/8/elementSize;
      int32_t id = node->getSymbolReference()->getReferenceNumber();

      TR::ILOpCodes storeOpCode = comp->il.opCodeForDirectStore(elementType);
      scalarizeLoadOrStore(opt, node, elementType, numLanes);

      TR_Array<TR::SymbolReference*>  *scalarSymRefs = (opt->_aliasTable)[id]._scalarSymRefs;
      TR_ASSERT_FATAL(scalarSymRefs, "reference should not be NULL");

      TR::SymbolReference *rhsSymRef = rhs->getSymbolReference();

      if (rhs->getOpCodeValue() == TR::aload) aloadHandler(opt, treeTop, rhs, elementType, vectorLength, mode);

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
      TR::VectorLength lengthEnum = supportedOnPlatform(comp, vectorLength);
      TR::DataType vectorType = OMR::DataType(elementType).scalarToVector(lengthEnum);
      vectorizeLoadOrStore(opt, node, vectorType);
      if (rhs->getOpCodeValue() == TR::aload) vectorizeLoadOrStore(opt, rhs, vectorType);
      }

   return;
   }


TR::Node *TR_VectorAPIExpansion::unsupportedHandler(TR_VectorAPIExpansion *, TR::TreeTop *treeTop,
                                                    TR::Node *node, TR::DataType elementType,
                                                    vec_sz_t length, handlerMode mode)
   {
   return NULL;
   }


TR::Node *TR_VectorAPIExpansion::fromArrayHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop,
                                                  TR::Node *node, TR::DataType elementType,
                                                  vec_sz_t vectorLength, handlerMode mode)
   {
   TR::Compilation *comp = opt->comp();

   if (mode == checkScalarization) return node;
   if (mode == checkVectorization)
      return supportedOnPlatform(comp, vectorLength) != TR::NoVectorLength ? node : NULL;

   if (opt->_trace)
      traceMsg(comp, "fromArrayHandler for node %p\n", node);

   TR::Node *array = node->getSecondChild();
   TR::Node *arrayIndex = node->getThirdChild();

   // TODO: insert exception check

   return transformLoadFromArray(opt, treeTop, node, elementType, vectorLength, mode, array, arrayIndex);
   }


TR::Node *TR_VectorAPIExpansion::intoArrayHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                  TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode)
   {
   TR::Compilation *comp = opt->comp();

   if (mode == checkScalarization) return node;
   if (mode == checkVectorization)
      return supportedOnPlatform(comp, vectorLength) != TR::NoVectorLength ? node : NULL;

   if (opt->_trace)
      traceMsg(comp, "intoArrayHandler for node %p\n", node);

   TR::Node *valueToWrite = node->getFirstChild();
   TR::Node *array = node->getSecondChild();
   TR::Node *arrayIndex = node->getThirdChild();

   // TODO: insert exception check

   return transformStoreToArray(opt, treeTop, node, elementType, vectorLength, mode, valueToWrite, array, arrayIndex);
   }


TR::Node *TR_VectorAPIExpansion::addHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                            TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode)
   {
   // This handler is not for intrinsic and is currently disabled

   TR::Compilation *comp = opt->comp();

   if (mode == checkScalarization) return node;
   if (mode == checkVectorization)
      return supportedOnPlatform(comp, vectorLength) != TR::NoVectorLength ? node : NULL;

   // TODO: The above does not check the actual opcode and type

   if (opt->_trace)
      traceMsg(comp, "addHandler for node %p\n", node);

   TR::ILOpCodes scalarOpCode = ILOpcodeFromVectorAPIOpcode(VECTOR_OP_ADD, elementType, true/*scalar*/);
   TR::ILOpCodes vectorOpCode = ILOpcodeFromVectorAPIOpcode(VECTOR_OP_ADD, elementType, false);

   return transformNary(opt, treeTop, node, elementType, vectorLength, mode, scalarOpCode, vectorOpCode, 0, 2, false);
}


TR::Node *TR_VectorAPIExpansion::loadIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop,
                                                      TR::Node *node, TR::DataType elementType,
                                                      vec_sz_t vectorLength, handlerMode mode)
   {
   TR::Compilation *comp = opt->comp();

   if (mode == checkScalarization) return node;
   if (mode == checkVectorization)
      return supportedOnPlatform(comp, vectorLength) != TR::NoVectorLength ? node : NULL;

   if (opt->_trace)
      traceMsg(comp, "loadIntrinsicHandler for node %p\n", node);

   TR::Node *array = node->getChild(5);
   TR::Node *arrayIndex = node->getChild(6);

   return transformLoadFromArray(opt, treeTop, node, elementType, vectorLength, mode, array, arrayIndex);
   }

TR::Node *TR_VectorAPIExpansion::transformLoadFromArray(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType,
                                               vec_sz_t vectorLength, handlerMode mode,
                                               TR::Node *array, TR::Node *arrayIndex)

   {
   TR::Compilation *comp = opt->comp();

   int32_t elementSize = OMR::DataType::getSize(elementType);
   TR::Node *aladdNode = generateAddressNode(array, arrayIndex, elementSize);

   anchorOldChildren(opt, treeTop, node);
   node->setAndIncChild(0, aladdNode);
   node->setNumChildren(1);

   if (mode == doScalarization)
      {
      int32_t numLanes = vectorLength/8/elementSize;
      TR::ILOpCodes loadOpCode = TR::ILOpCode::indirectLoadOpCode(elementType);
      TR::SymbolReference *scalarShadow = comp->getSymRefTab()->findOrCreateArrayShadowSymbolRef(elementType, NULL);

      TR::Node::recreate(node, loadOpCode);
      node->setSymbolReference(scalarShadow);

      // keep Byte and Short as Int after it's loaded from array
      if (elementType == TR::Int8 || elementType == TR::Int16)
         {
         TR::Node *newLoadNode = node->duplicateTree(false);
         TR::Node::recreate(node, elementType == TR::Int8 ? TR::b2i : TR::s2i);
         node->setAndIncChild(0, newLoadNode);
         }

      for (int32_t i = 1; i < numLanes; i++)
         {
         TR::Node *newLoadNode = TR::Node::createWithSymRef(node, loadOpCode, 1, scalarShadow);
         TR::Node *newAddressNode = TR::Node::create(TR::aladd, 2, aladdNode, TR::Node::create(TR::lconst, 0, i*elementSize));
         newLoadNode->setAndIncChild(0, newAddressNode);

         // keep Byte and Short as Int after it's loaded from array
         if (elementType == TR::Int8 || elementType == TR::Int16)
            newLoadNode = TR::Node::create(newLoadNode, elementType == TR::Int8 ? TR::b2i : TR::s2i, 1, newLoadNode);

         addScalarNode(opt, node, numLanes, i, newLoadNode);
         }
      }
   else if (mode == doVectorization)
      {
      TR::VectorLength lengthEnum = supportedOnPlatform(comp, vectorLength);
      TR::DataType vectorType = OMR::DataType(elementType).scalarToVector(lengthEnum);
      TR::SymbolReference *vecShadow = comp->getSymRefTab()->findOrCreateArrayShadowSymbolRef(vectorType, NULL);
      TR::Node::recreate(node, TR::vloadi);
      node->setSymbolReference(vecShadow);
      }

   return node;
   }


TR::Node *TR_VectorAPIExpansion::storeIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                  TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode)
   {
   TR::Compilation *comp = opt->comp();

   if (mode == checkScalarization) return node;
   if (mode == checkVectorization)
      return supportedOnPlatform(comp, vectorLength) != TR::NoVectorLength ? node : NULL;

   if (opt->_trace)
      traceMsg(comp, "storeIntrinsicHandler for node %p\n", node);

   TR::Node *valueToWrite = node->getChild(5);
   TR::Node *array = node->getChild(6);
   TR::Node *arrayIndex = node->getChild(7);

   return transformStoreToArray(opt, treeTop, node, elementType, vectorLength, mode, valueToWrite, array, arrayIndex);
   }


TR::Node *TR_VectorAPIExpansion::transformStoreToArray(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                       TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode,
                                                       TR::Node *valueToWrite, TR::Node *array, TR::Node *arrayIndex)

   {
   TR::Compilation *comp = opt->comp();

   int32_t  elementSize = OMR::DataType::getSize(elementType);
   TR::Node *aladdNode = generateAddressNode(array, arrayIndex, elementSize);

   anchorOldChildren(opt, treeTop, node);
   node->setAndIncChild(0, aladdNode);
   node->setAndIncChild(1, valueToWrite);
   node->setNumChildren(2);

   if (mode == doScalarization)
      {
      int32_t numLanes = vectorLength/8/elementSize;

      // TODO: use TR::ILOpCode::indirectLoadOpCode(elementType) after adding it to OMR
      TR_ASSERT_FATAL(elementType < TR::NumOMRTypes, "unexpected type");
      TR::ILOpCodes storeOpCode = comp->il.OMR::IL::opCodeForIndirectStore(elementType);

      TR::SymbolReference *scalarShadow = comp->getSymRefTab()->findOrCreateArrayShadowSymbolRef(elementType, NULL);

      if (valueToWrite->getOpCodeValue() == TR::aload)
         aloadHandler(opt, treeTop, valueToWrite, elementType, vectorLength, mode);

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
      // vectorization(will be enabled later)
      TR::VectorLength lengthEnum = supportedOnPlatform(comp, vectorLength);
      TR::DataType vectorType = OMR::DataType(elementType).scalarToVector(lengthEnum);
      TR::SymbolReference *vecShadow = comp->getSymRefTab()->findOrCreateArrayShadowSymbolRef(vectorType, NULL);

      if (valueToWrite->getOpCodeValue() == TR::aload) vectorizeLoadOrStore(opt, valueToWrite, vectorType);

      TR::Node::recreate(node, TR::vstorei);
      node->setSymbolReference(vecShadow);
      }

   return node;
   }


TR::Node *TR_VectorAPIExpansion::unaryIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                        TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode)
   {
   return naryIntrinsicHandler(opt, treeTop, node, elementType, vectorLength, mode, 1);
   }

TR::Node *TR_VectorAPIExpansion::binaryIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                        TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode)
   {
   return naryIntrinsicHandler(opt, treeTop, node, elementType, vectorLength, mode, 2);
   }

TR::Node *TR_VectorAPIExpansion::ternaryIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                        TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode)
   {
   return naryIntrinsicHandler(opt, treeTop, node, elementType, vectorLength, mode, 3);
   }

bool TR_VectorAPIExpansion::useVcallForVectorAPIOpcode(TR::Compilation *comp, int32_t vectorAPIOpcode, vec_sz_t vectorLength)
   {
   // Check for opcodes that will use vcall temporarily.
   // Add other specific opcodes here that you would like to prototype using vcall
   // This is meant for prototyping only. Please do not enable when merging.
#if 0
   if ((vectorAPIOpcode == VECTOR_OP_FMA
        || vectorAPIOpcode == VECTOR_OP_ADD) &&
        comp->target().cpu.isPower() && vectorLength == 128)
        return true;
#endif

   return false;
   }

TR::Node *TR_VectorAPIExpansion::naryIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                      TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode,
                                                      int32_t numChildren)
   {
   TR::Compilation *comp = opt->comp();
   TR::Node *opcodeNode = node->getFirstChild();

   if (!opcodeNode->getOpCode().isLoadConst())
      {
      if (opt->_trace) traceMsg(comp, "Unknown opcode in node %p\n", node);
      return NULL;
      }

   bool useVcall = false;
   int32_t vectorAPIOpcode = opcodeNode->get32bitIntegralValue();
   TR::DataType opType = elementType;

   // Byte and Short are promoted after being loaded from array
   // and all operations should be done in Int
   if (elementType == TR::Int8 || elementType == TR::Int16)
      opType = TR::Int32;

   TR::ILOpCodes scalarOpCode = ILOpcodeFromVectorAPIOpcode(vectorAPIOpcode, opType, 0);
   TR::ILOpCodes vectorOpCode = ILOpcodeFromVectorAPIOpcode(vectorAPIOpcode, opType, vectorLength);

   if (scalarOpCode == TR::BadILOp)
      if (opt->_trace) traceMsg(comp, "Unsupported opcode in node %p\n", node);

   if (mode == checkVectorization || mode == doVectorization)
      {
      useVcall = useVcallForVectorAPIOpcode(comp, vectorAPIOpcode, vectorLength);
      }

   if (mode == checkScalarization)
      return (scalarOpCode != TR::BadILOp) ? node : NULL;

   if (mode == checkVectorization)
      {
      if (supportedOnPlatform(comp, vectorLength) == TR::NoVectorLength) return NULL;

      if (useVcall) return node;


      if (vectorOpCode == TR::BadILOp)
          return NULL;

      if (!comp->cg()->getSupportsOpCodeForAutoSIMD(vectorOpCode, elementType, OMR::DataType::bitsToVectorLength(vectorLength)))
         return NULL;

      return node;
      }

   return transformNary(opt, treeTop, node, elementType, vectorLength, mode, scalarOpCode, vectorOpCode,
                        5/*first operand*/, numChildren, useVcall);
   }

TR::Node *TR_VectorAPIExpansion::blendIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                       TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode)
   {
   TR::Compilation *comp = opt->comp();

   if (opt->_trace)
      traceMsg(comp, "blendIntrinsicHandler for node %p\n", node);

   TR::ILOpCodes scalarOpCode = TR::BadILOp;
   TR::ILOpCodes vectorOpCode = TR::vselect;

   if (mode == checkScalarization)
      return NULL;

   if (mode == checkVectorization)
      {
      if (!supportedOnPlatform(comp, vectorLength)) return NULL;

      if (!comp->cg()->getSupportsOpCodeForAutoSIMD(vectorOpCode, elementType, OMR::DataType::bitsToVectorLength(vectorLength)))
         return NULL;

      return node;
      }

   return transformNary(opt, treeTop, node, elementType, vectorLength, mode, scalarOpCode, vectorOpCode,
                        4/*first operand*/, 3, false);
   }

TR::Node *TR_VectorAPIExpansion::broadcastCoercedIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                        TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode)
   {
   TR::Compilation *comp = opt->comp();

   if (mode == checkScalarization)
      return node;

   if (mode == checkVectorization)
      {
      if (!comp->cg()->getSupportsOpCodeForAutoSIMD(TR::vsplats, elementType, OMR::DataType::bitsToVectorLength(vectorLength)))
         return NULL;
      else
         return node;
      }

   if (opt->_trace)
      traceMsg(comp, "broadcastCoercedIntrinsicHandler for node %p\n", node);

   int32_t elementSize = OMR::DataType::getSize(elementType);
   TR::Node *valueToBroadcast = node->getChild(3);

   anchorOldChildren(opt, treeTop, node);

   TR::Node *newNode;

   switch (elementType) {
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
      node->setAndIncChild(0, newNode->getChild(0));
      node->setNumChildren(1);

      int32_t numLanes = vectorLength/8/elementSize;

      TR::Node::recreate(node, newNode->getOpCodeValue());

      for (int32_t i = 1; i < numLanes; i++)
         {
         addScalarNode(opt, node, numLanes, i, node);
         }
      }
   else if (mode == doVectorization)
      {
      node->setAndIncChild(0, newNode);
      node->setNumChildren(1);
      TR::Node::recreate(node, TR::vsplats);
      }

   return node;
   }

TR::Node *TR_VectorAPIExpansion::compareIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                       TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode)
   {
   TR::Compilation *comp = opt->comp();
   TR::Node *opcodeNode = node->getFirstChild();

   if (!opcodeNode->getOpCode().isLoadConst())
      {
      if (opt->_trace) traceMsg(comp, "Unknown opcode in node %p\n", node);
      return NULL;
      }

   bool useVcall = false;
   int32_t vectorAPIOpcode = opcodeNode->get32bitIntegralValue();
   TR::DataType opType = elementType;

   // Byte and Short are promoted after being loaded from array
   // and all operations should be done in Int
   if (elementType == TR::Int8 || elementType == TR::Int16)
      opType = TR::Int32;

   TR::ILOpCodes scalarOpCode = ILOpcodeFromVectorAPIOpcode(vectorAPIOpcode, opType, 0, true/*compare*/);
   TR::ILOpCodes vectorOpCode = ILOpcodeFromVectorAPIOpcode(vectorAPIOpcode, opType, vectorLength, true/*compare*/);

   if (mode == checkVectorization || mode == doVectorization)
      {
      useVcall = useVcallForVectorAPIOpcode(comp, vectorAPIOpcode, vectorLength);
      }

   if (mode == checkScalarization)
      {
      if (scalarOpCode == TR::BadILOp)
         {
         if (opt->_trace) traceMsg(comp, "Unsupported opcode in node %p\n", node);
         return NULL;
         }
      return node;
      }

   if (mode == checkVectorization)
      {
      if (!supportedOnPlatform(comp, vectorLength)) return NULL;

      if (useVcall) return node;

      if (vectorOpCode == TR::BadILOp)
          return NULL;

      if (!comp->cg()->getSupportsOpCodeForAutoSIMD(vectorOpCode, elementType, OMR::DataType::bitsToVectorLength(vectorLength)))
         return NULL;

      return node;
      }

   return transformNary(opt, treeTop, node, elementType, vectorLength, mode, scalarOpCode, vectorOpCode,
                        5/*first operand*/, 2, useVcall);
   }

TR::ILOpCodes TR_VectorAPIExpansion::ILOpcodeFromVectorAPIOpcode(int32_t vectorAPIOpCode, TR::DataType elementType, vec_sz_t bitsLength, bool compare)
   {
   bool scalar = (bitsLength == 0);

   if (compare)
      {
      switch (vectorAPIOpCode)
         {
         case BT_eq: return scalar ? TR::ILOpCode::cmpeqOpCode(elementType) : TR::vcmpeq;
         case BT_ne: return scalar ? TR::BadILOp : TR::vcmpne;
         case BT_le: return scalar ? TR::BadILOp : TR::vcmple;
         case BT_ge: return scalar ? TR::BadILOp : TR::vcmpge;
         case BT_lt: return scalar ? TR::BadILOp : TR::vcmplt;
         case BT_gt: return scalar ? TR::BadILOp : TR::vcmpgt;
         default:
            return TR::BadILOp;
         }
      }

   TR::VectorLength vectorLength = OMR::DataType::bitsToVectorLength(bitsLength);

   switch (vectorAPIOpCode)
      {
      case VECTOR_OP_ABS: return scalar ? TR::ILOpCode::absOpCode(elementType) : TR::BadILOp;
      case VECTOR_OP_NEG: return scalar ? TR::ILOpCode::negateOpCode(elementType) : TR::vneg;
      case VECTOR_OP_SQRT:return TR::BadILOp;
      case VECTOR_OP_ADD: return scalar ? TR::ILOpCode::addOpCode(elementType, true)
         : TR::ILOpCode::createVectorOpCode(OMR::vadd, TR::DataType::createVectorType(elementType.getDataType(), vectorLength)).getOpCodeValue();
      case VECTOR_OP_SUB: return scalar ? TR::ILOpCode::subtractOpCode(elementType) : TR::vsub;
      case VECTOR_OP_MUL: return scalar ? TR::ILOpCode::multiplyOpCode(elementType) : TR::vmul;
      case VECTOR_OP_DIV: return scalar ? TR::ILOpCode::divideOpCode(elementType) : TR::vdiv;
      case VECTOR_OP_MIN: return TR::BadILOp;
      case VECTOR_OP_MAX: return TR::BadILOp;
      case VECTOR_OP_AND: return scalar ? TR::ILOpCode::andOpCode(elementType) : TR::vand;
      case VECTOR_OP_OR:  return scalar ? TR::ILOpCode::orOpCode(elementType)  : TR::vor;
      case VECTOR_OP_XOR: return scalar ? TR::ILOpCode::xorOpCode(elementType) : TR::vxor;
      case VECTOR_OP_FMA: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(OMR::vfma, TR::DataType::createVectorType(elementType.getDataType(), vectorLength)).getOpCodeValue();
      default:
         return TR::BadILOp;
      // shiftLeftOpCode
      // shiftRightOpCode
      }
   return TR::BadILOp;
   }

TR::Node *TR_VectorAPIExpansion::transformNary(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                               TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode,
                                               TR::ILOpCodes scalarOpCode, TR::ILOpCodes vectorOpCode, int32_t firstOperand, int32_t numOperands,
                                               bool useVcall)
   {
   TR::Compilation *comp = opt->comp();

   // Since we are modifying node's children in place
   // it's better to save the original ones
   TR_ASSERT_FATAL(numOperands <= _maxNumberOperands, "number of operands exceeds %d\n", _maxNumberOperands);

   TR::Node* operands[_maxNumberOperands];
   for (int32_t i = 0; i < numOperands; i++)
      {
      operands[i] = node->getChild(firstOperand + i);
      }

   if (mode == doScalarization)
      {
      anchorOldChildren(opt, treeTop, node);

      int32_t elementSize = OMR::DataType::getSize(elementType);
      int32_t numLanes = vectorLength/8/elementSize;

      for (int32_t i = 0; i < numOperands; i++)
         {
         if (operands[i]->getOpCodeValue() == TR::aload)
            aloadHandler(opt, treeTop, operands[i], elementType, vectorLength, mode);
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
      TR::VectorLength lengthEnum = supportedOnPlatform(comp, vectorLength);
      TR::DataType vectorType = OMR::DataType(elementType).scalarToVector(lengthEnum);

      for (int32_t i = 0; i < numOperands; i++)
         {
         if (operands[i]->getOpCodeValue() == TR::aload)
            vectorizeLoadOrStore(opt, operands[i], vectorType);
         }

      if (useVcall)
         {
         TR::Node::recreate(node, TR::vcall);
         }
      else
         {
         TR_ASSERT_FATAL(vectorOpCode != TR::BadILOp, "Vector opcode should exist for node %p\n", node);

         anchorOldChildren(opt, treeTop, node);
         for (int32_t i = 0; i < numOperands; i++)
            {
            node->setAndIncChild(i, operands[i]);
            }
         node->setNumChildren(numOperands);
         TR::Node::recreate(node, vectorOpCode);
         }
      }

   return node;
}


// high level methods are disabled because they require exception handling
TR_VectorAPIExpansion::methodTableEntry
TR_VectorAPIExpansion::methodTable[] =
   {
   {loadIntrinsicHandler,  TR::NoType, Vector,  {Unknown, elementType, numLanes}},                           // jdk_internal_vm_vector_VectorSupport_load
   {storeIntrinsicHandler, TR::NoType, Unknown, {Unknown, elementType, numLanes, Unknown, Unknown, Vector}}, // jdk_internal_vm_vector_VectorSupport_store
   {binaryIntrinsicHandler, TR::NoType, Vector,  {Unknown, Unknown, Unknown, elementType, numLanes, Vector, Vector}},  // jdk_internal_vm_vector_VectorSupport_binaryOp
   {blendIntrinsicHandler, TR::NoType, Vector, {Unknown, Unknown, elementType, numLanes, Vector, Vector, Vector, Unknown}}, // jdk_internal_vm_vector_VectorSupport_blend
   {broadcastCoercedIntrinsicHandler, TR::NoType, Vector, {Unknown, elementType, numLanes, Unknown, Unknown, Unknown}},  // jdk_internal_vm_vector_VectorSupport_broadcastCoerced
   {compareIntrinsicHandler, TR::NoType, Vector, {Unknown, Unknown, Unknown, elementType, numLanes, Vector, Vector, Unknown}}, // jdk_internal_vm_vector_VectorSupport_compare
   {ternaryIntrinsicHandler, TR::NoType, Vector,  {Unknown, Unknown, Unknown, elementType, numLanes, Vector, Vector}},  // jdk_internal_vm_vector_VectorSupport_ternaryOp
   {unaryIntrinsicHandler, TR::NoType, Vector,  {Unknown, Unknown, Unknown, elementType, numLanes, Vector}},  // jdk_internal_vm_vector_VectorSupport_unaryOp

   {unsupportedHandler /*fromArrayHandler*/,      TR::Float,  Vector,  {Species}}, // jdk_incubator_vector_FloatVector_fromArray,
   {unsupportedHandler /*intoArrayHandler*/,      TR::Float,  Unknown, {Vector}},  // jdk_incubator_vector_FloatVector_intoArray,
   {unsupportedHandler,    TR::Float,  Vector,  {Species}},                 // jdk_incubator_vector_FloatVector_fromArray_mask
   {unsupportedHandler,    TR::Float,  Unknown, {}},                        // jdk_incubator_vector_FloatVector_intoArray_mask
   {unsupportedHandler /*addHandler*/,            TR::Float,  Vector,  {Vector, Vector}},          // jdk_incubator_vector_FloatVector_add
   {unsupportedHandler,    TR::Float,  Vector,  {}}                         // jdk_incubator_vector_VectorSpecies_indexInRange
   };



TR_VectorAPIExpansion::TR_VectorAPIExpansion(TR::OptimizationManager *manager)
      : TR::Optimization(manager), _trace(false), _aliasTable(trMemory()), _nodeTable(trMemory())
   {
   static_assert(sizeof(methodTable) / sizeof(methodTable[0]) == _numMethods,
                 "methodTable should contain recognized methods between TR::FirstVectorMethod and TR::LastVectorMethod");
   }


//TODOs:
// 4) handle OSR guards
// 6) make scalarization and vectorization coexist in one web
// 7) handle all intrinsics
// 8) box vector objects if passed to unvectorized methods
// 9) don't use vcall's for vectorization
// 10) cost-benefit analysis for boxing
// 11) implement useDef based approach
// 12) handle methods that return vector type different from the argument
// 13) OPT: optimize java/util/Objects.checkIndex
// 15) OPT: too many aliased temps (reused privatized args) that cause over-aliasing after GVP
// 16) OPT: add to other opt levels
// 18) OPT: jdk/incubator/vector/VectorOperators.opCode() is not inlined with Byte,Short,Long add
// 20) Fix TR_ALLOC
