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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
#include "compile/ResolvedMethod.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/VerboseLog.hpp"
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

TR_VectorAPIExpansion::vapiObjType
TR_VectorAPIExpansion::getReturnType(TR::MethodSymbol * methodSymbol)
   {
   if (!isVectorAPIMethod(methodSymbol)) return Unknown;

   TR::RecognizedMethod index = methodSymbol->getRecognizedMethod();

   return methodTable[index - _firstMethod]._returnType;
   }

bool
TR_VectorAPIExpansion::isArgType(TR::MethodSymbol *methodSymbol, int32_t i, vapiObjType type)
   {
   if (!isVectorAPIMethod(methodSymbol) || i < 0 ) return false;

   TR::RecognizedMethod index = methodSymbol->getRecognizedMethod();

   TR_ASSERT_FATAL(i < _maxNumberArguments, "Argument index %d is too big", i);
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

            int32_t id1 = node->getSymbolReference()->getReferenceNumber();
            int32_t id2 = rhs->getSymbolReference()->getReferenceNumber();

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
      bool isVectorAPICall = isVectorAPIMethod(methodSymbol);

      _aliasTable[methodRefNum]._objectType = getReturnType(methodSymbol);

      if (_aliasTable[methodRefNum]._objectType == Unknown &&
          isVectorAPICall)
         {
         if (methodSymbol->getRecognizedMethod() == TR::jdk_internal_vm_vector_VectorSupport_load)
            {
            _aliasTable[methodRefNum]._objectType = getObjectTypeFromClassNode(comp(), node->getFirstChild());
            }
         else if (methodSymbol->getRecognizedMethod() == TR::jdk_internal_vm_vector_VectorSupport_fromBitsCoerced)
            {
            TR::Node *broadcastTypeNode = node->getChild(BROADCAST_TYPE_CHILD);

            if (!broadcastTypeNode->getOpCode().isLoadConst())
               {
               if (_trace) traceMsg(comp(), "Unknown broadcast type in node %p\n", node);
               }
            else
               {
               int32_t broadcastType = broadcastTypeNode->get32bitIntegralValue();

               TR_ASSERT_FATAL(broadcastType == MODE_BROADCAST || broadcastType == MODE_BITS_COERCED_LONG_TO_MASK,
                              "Unexpected broadcast type in node %p\n", node);

               _aliasTable[methodRefNum]._objectType =(broadcastType == MODE_BROADCAST) ? Vector : Mask;
               }
            }
         }

      for (int32_t i = 0; i < numChildren; i++)
         {
         if (!isVectorAPICall ||
             isArgType(methodSymbol, i, Vector) ||
             isArgType(methodSymbol, i, Mask))
            {
            TR::Node *child = node->getChild(i);
            bool hasSymbolReference = child->getOpCode().hasSymbolReference();
            bool isMask = isVectorAPICall && isArgType(methodSymbol, i, Mask);
            bool isNullMask = isMask && child->isConstZeroValue();

            if (hasSymbolReference)
               {
               alias(node, child);
               }

            if (!hasSymbolReference && !isNullMask)
               {
               if (_trace)
                  traceMsg(comp(), "Invalidating #%d due to child %d (%p) in node %p\n",
                           node->getSymbolReference()->getReferenceNumber(), i, child, node);
               invalidateSymRef(node->getSymbolReference());
               }
            }

         if (!isVectorAPICall)
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
            }
         else if (isArgType(methodSymbol, i, ElementType))
            {
            TR::Node *elementTypeNode = node->getChild(i);
            methodElementType = getDataTypeFromClassNode(comp(), elementTypeNode);

            // maskReductionCoerced intrinsic has element type Int for Float vectors and Long for Double vectors.
            // For the sake of class verification we can leave _elementType field unset
            // and it will be automatically derived from the child type (since they will be in the same class)
            if (methodSymbol->getRecognizedMethod() != TR::jdk_internal_vm_vector_VectorSupport_maskReductionCoerced)
               _aliasTable[methodRefNum]._elementType = methodElementType;
            }
         else if (isArgType(methodSymbol, i, NumLanes))
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
            bool scalarResult = false;
            if (child->getOpCode().isFunctionCall())
               {
               TR::MethodSymbol * methodSymbol = child->getSymbolReference()->getSymbol()->castToMethodSymbol();
               if (getReturnType(methodSymbol) ==  Scalar) continue; // OK to use by any other parent node
               }

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
            traceMsg(comp(), "Set class #%d for symref #%d\n", classId, i);
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


J9Class *
TR_VectorAPIExpansion::getJ9ClassFromClassNode(TR::Compilation *comp, TR::Node *classNode)
   {
   if (!classNode->getOpCode().hasSymbolReference())
      return NULL;

   TR::SymbolReference *symRef = classNode->getSymbolReference();
   if (symRef)
      {
      if (symRef->hasKnownObjectIndex())
         {
         TR_J9VMBase *fej9 = comp->fej9();

         TR::VMAccessCriticalSection getDataTypeFromClassNodeSection(fej9);

         uintptr_t javaLangClass = comp->getKnownObjectTable()->getPointer(symRef->getKnownObjectIndex());
         J9Class *j9class = (J9Class *)(intptr_t)fej9->getInt64Field(javaLangClass, "vmRef");

         return j9class;
         }
      }
   return NULL;
   }


TR::DataType
TR_VectorAPIExpansion::getDataTypeFromClassNode(TR::Compilation *comp, TR::Node *classNode)
   {
   J9Class *j9class = getJ9ClassFromClassNode(comp, classNode);

   if (!j9class) return TR::NoType;

   TR_J9VMBase *fej9 = comp->fej9();
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
   else
      return TR::NoType;
   }


TR_VectorAPIExpansion::vapiObjType
TR_VectorAPIExpansion::getObjectTypeFromClassNode(TR::Compilation *comp, TR::Node *classNode)
   {
   J9Class *j9class = getJ9ClassFromClassNode(comp, classNode);

   if (!j9class) return Unknown;

   J9UTF8 *className = J9ROMCLASS_CLASSNAME(j9class->romClass);
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
            traceMsg(comp(), "%s invalidating6 class #%d due to symref #%d method type %s, seen type %s\n",
                     OPT_DETAILS_VECTOR, id, i, TR::DataType::getName(methodType), TR::DataType::getName(classType));
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

            if (_aliasTable[i]._objectType == Invalid)
               {
               if (_trace)
                  traceMsg(comp(), "Class #%d can't be vectorized or scalarized due to invalid object type of  #%d\n", id, i);

               _aliasTable[id]._cantVectorize = true;
               _aliasTable[id]._cantScalarize = true;
               vectorClass = false;
               break;
               }


            if (_aliasTable[i]._cantVectorize)
               {
               if (_trace)
                  traceMsg(comp(), "Class #%d can't be vectorized due to #%d\n", id, i);

               _aliasTable[id]._cantVectorize = true;

               if (_aliasTable[id]._cantScalarize)
                  {
                  vectorClass = false;
                  break;
                  }
               }

            if (_aliasTable[i]._cantScalarize)
               {
               if (_trace)
                  traceMsg(comp(), "Class #%d can't be scalarized due to #%d\n", id, i);
               _aliasTable[id]._cantScalarize = true;

               if (_aliasTable[id]._cantVectorize)
                  {
                  vectorClass = false;
                  break;
                  }
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
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

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
         TR_ASSERT_FATAL(!_aliasTable[classId]._cantScalarize, "Class #%d should be either vectorizable or scalarizable",
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
         traceMsg(comp(), "Transforming node %p of class #%d\n", node, classId);

      TR::DataType elementType = _aliasTable[classId]._elementType;
      int32_t bitsLength = _aliasTable[classId]._vecLen;
      TR::VectorLength vectorLength = OMR::DataType::bitsToVectorLength(bitsLength);
      int32_t elementSize = OMR::DataType::getSize(elementType);
      int32_t numLanes = bitsLength/8/elementSize;

      if (opCodeValue == TR::astore)
         {
         if (_trace)
            traceMsg(comp(), "handling astore %p\n", node);
         astoreHandler(this, treeTop, node, elementType, vectorLength, numLanes, doMode);
         }
      else if (opCode.isFunctionCall())
         {
         TR_ASSERT_FATAL(parent, "All VectorAPI calls are expected to have a treetop");

         TR::RecognizedMethod index = methodSymbol->getRecognizedMethod();
         int32_t handlerIndex = index - _firstMethod;

         TR_ASSERT_FATAL(methodTable[handlerIndex]._methodHandler(this, treeTop, node, elementType, vectorLength, numLanes, checkMode),
                         "Analysis should've proved that method is supported");

         TR::Node::recreate(parent, TR::treetop);
         methodTable[handlerIndex]._methodHandler(this, treeTop, node, elementType, vectorLength, numLanes, doMode);
         }

      if (doMode == doScalarization)
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

   if (_trace)
      comp()->dumpMethodTrees("After Vectorization");

   return 1;
   }

//
// static transformation routines
//

void
TR_VectorAPIExpansion::vectorizeLoadOrStore(TR_VectorAPIExpansion *opt, TR::Node *node,
                                            TR::DataType opCodeType)
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
   if (node->getOpCode().isStore())
      TR::Node::recreate(node, TR::ILOpCode::createVectorOpCode(opCodeType.isVector() ? TR::vstore : TR::mstore, opCodeType));
   else
      TR::Node::recreate(node, TR::ILOpCode::createVectorOpCode(opCodeType.isVector() ? TR::vload : TR::mload, opCodeType));

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


   TR::Node *lshlNode = TR::Node::create(TR::lshl, 2);
   lshlNode->setAndIncChild(0, arrayIndex);
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
                                         TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode)
   {
   TR::Compilation *comp = opt->comp();

   if (mode == doScalarization)
      {
      int32_t elementSize = OMR::DataType::getSize(elementType);
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

   TR::Node *array = node->getChild(5);
   TR::Node *arrayIndex = node->getChild(6);

   return transformLoadFromArray(opt, treeTop, node, elementType, vectorLength, numLanes, mode, array, arrayIndex, objType);
   }

TR::Node *TR_VectorAPIExpansion::transformLoadFromArray(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                        TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                        handlerMode mode,
                                                        TR::Node *array, TR::Node *arrayIndex, vapiObjType objType)

   {
   TR::Compilation *comp = opt->comp();

   int32_t elementSize = OMR::DataType::getSize(elementType);
   TR::Node *aladdNode = generateAddressNode(array, arrayIndex, objType == Mask ? 1 : elementSize);

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

   TR::Node *valueToWrite = node->getChild(5);
   TR::Node *array = node->getChild(6);
   TR::Node *arrayIndex = node->getChild(7);

   return transformStoreToArray(opt, treeTop, node, elementType, vectorLength, numLanes, mode, valueToWrite, array, arrayIndex, objType);
   }


TR::Node *TR_VectorAPIExpansion::transformStoreToArray(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                       TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                       handlerMode mode,
                                                       TR::Node *valueToWrite, TR::Node *array, TR::Node *arrayIndex, vapiObjType objType)

   {
   TR::Compilation *comp = opt->comp();

   int32_t  elementSize = OMR::DataType::getSize(elementType);
   TR::Node *aladdNode = generateAddressNode(array, arrayIndex, objType == Mask ? 1 : elementSize);

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


TR::Node *TR_VectorAPIExpansion::naryIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                      TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                      handlerMode mode,
                                                      int32_t numChildren, vapiOpCodeType opCodeType)
   {
   TR::Compilation *comp = opt->comp();
   TR::Node *opcodeNode = node->getFirstChild();
   int firstOperand = 5;

   if (opCodeType == Test || opCodeType == MaskReduction || opCodeType == Blend)
      firstOperand = 4;

   bool withMask = false;

   if (opCodeType != MaskReduction)
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
      scalarOpCode = ILOpcodeFromVectorAPIOpcode(vectorAPIOpcode, opType, TR::NoVectorLength, opCodeType, withMask);

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
      if (mode == checkVectorization)
         {
         vectorOpCode = ILOpcodeFromVectorAPIOpcode(vectorAPIOpcode, opType, vectorLength, opCodeType, withMask);

         if (vectorOpCode == TR::BadILOp || !comp->cg()->getSupportsOpCodeForAutoSIMD(vectorOpCode))
            {
            if (opt->_trace) traceMsg(comp, "Unsupported vector opcode in node %p\n", node);
            return NULL;
            }
         else
            {
            return node;
            }
         }
      else
         {
         vectorOpCode = ILOpcodeFromVectorAPIOpcode(vectorAPIOpcode, opType, vectorLength, opCodeType, withMask);

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
                        firstOperand, numChildren, opCodeType);
   }

TR::Node *TR_VectorAPIExpansion::blendIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                       TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                       handlerMode mode)
   {
   return naryIntrinsicHandler(opt, treeTop, node, elementType, vectorLength, numLanes, mode, 3, Blend);
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

TR::Node *TR_VectorAPIExpansion::convertIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node,
                                                         TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes,
                                                         handlerMode mode)
   {
   return NULL;
   }

TR::ILOpCodes TR_VectorAPIExpansion::ILOpcodeFromVectorAPIOpcode(int32_t vectorAPIOpCode, TR::DataType elementType,
                                                                 TR::VectorLength vectorLength, vapiOpCodeType opCodeType, bool withMask)
   {
   // TODO: support more scalarization

   bool scalar = (vectorLength == TR::NoVectorLength);
   TR::DataType vectorType = scalar ? TR::NoType : TR::DataType::createVectorType(elementType, vectorLength);

   if (opCodeType == Blend)
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
   else if ((opCodeType == Compare) && withMask)
      {
      switch (vectorAPIOpCode)
         {
         case BT_eq: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmcmpeq, vectorType);
         case BT_ne: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmcmpne, vectorType);
         case BT_le: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmcmple, vectorType);
         case BT_ge: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmcmpge, vectorType);
         case BT_lt: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmcmplt, vectorType);
         case BT_gt: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vmcmpgt, vectorType);
         default:
            return TR::BadILOp;
         }
      }
   else if (opCodeType == Compare)
      {
      switch (vectorAPIOpCode)
         {
         case BT_eq: return scalar ? TR::ILOpCode::cmpeqOpCode(elementType) : TR::ILOpCode::createVectorOpCode(TR::vcmpeq, vectorType);
         case BT_ne: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vcmpne, vectorType);
         case BT_le: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vcmple, vectorType);
         case BT_ge: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vcmpge, vectorType);
         case BT_lt: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vcmplt, vectorType);
         case BT_gt: return scalar ? TR::BadILOp : TR::ILOpCode::createVectorOpCode(TR::vcmpgt, vectorType);
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
                                               int32_t numOperands, vapiOpCodeType opCodeType)
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
         if (operands[i]->getOpCodeValue() == TR::aload)
            {
            TR::DataType opCodeType = vectorType;

            if (opt->_aliasTable[operands[i]->getSymbolReference()->getReferenceNumber()]._objectType == Mask)
               opCodeType = TR::DataType::createMaskType(elementType, vectorLength);

            vectorizeLoadOrStore(opt, operands[i], opCodeType);
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

      }

   return node;
}


// high level methods are disabled because they require exception handling
TR_VectorAPIExpansion::methodTableEntry
TR_VectorAPIExpansion::methodTable[] =
   {
   {loadIntrinsicHandler,                 Unknown, {Unknown, ElementType, NumLanes}},                                           // jdk_internal_vm_vector_VectorSupport_load
   {storeIntrinsicHandler,                Unknown, {Unknown, ElementType, NumLanes, Unknown, Unknown, Vector}},                 // jdk_internal_vm_vector_VectorSupport_store
   {binaryIntrinsicHandler,               Vector,  {Unknown, Unknown, Unknown, ElementType, NumLanes, Vector, Vector, Mask}},   // jdk_internal_vm_vector_VectorSupport_binaryOp
   {blendIntrinsicHandler,                Vector,  {Unknown, Unknown, ElementType, NumLanes, Vector, Vector, Vector, Unknown}}, // jdk_internal_vm_vector_VectorSupport_blend
   {compareIntrinsicHandler,              Mask,    {Unknown, Unknown, Unknown, ElementType, NumLanes, Vector, Vector, Mask}},   // jdk_internal_vm_vector_VectorSupport_compare
   {convertIntrinsicHandler,              Mask,    {Unknown, Unknown, Unknown, ElementType, NumLanes, Vector, Vector, Mask}},   // jdk_internal_vm_vector_VectorSupport_convert
   {fromBitsCoercedIntrinsicHandler,      Unknown, {Unknown, ElementType, NumLanes, Unknown, Unknown, Unknown}},                // jdk_internal_vm_vector_VectorSupport_fromBitsCoerced
   {maskReductionCoercedIntrinsicHandler, Scalar,  {Unknown, Unknown, ElementType, NumLanes, Mask}},                            // jdk_internal_vm_vector_VectorSupport_maskReductionCoerced
   {reductionCoercedIntrinsicHandler,     Scalar,  {Unknown, Unknown, Unknown, ElementType, NumLanes, Vector, Mask}},           // jdk_internal_vm_vector_VectorSupport_reductionCoerced
   {ternaryIntrinsicHandler,              Vector,  {Unknown, Unknown, Unknown, ElementType, NumLanes, Vector, Vector, Vector, Mask}},  // jdk_internal_vm_vector_VectorSupport_ternaryOp
   {testIntrinsicHandler,                 Scalar,  {Unknown, Unknown, ElementType, NumLanes, Mask, Mask, Unknown}},             // jdk_internal_vm_vector_VectorSupport_test
   {unaryIntrinsicHandler,                Vector,  {Unknown, Unknown, Unknown, ElementType, NumLanes, Vector, Mask}},           // jdk_internal_vm_vector_VectorSupport_unaryOp
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
// 10) cost-benefit analysis for boxing
// 11) implement useDef based approach
// 12) handle methods that return vector type different from the argument
// 13) OPT: optimize java/util/Objects.checkIndex
// 15) OPT: too many aliased temps (reused privatized args) that cause over-aliasing after GVP
// 16) OPT: add to other opt levels
// 18) OPT: jdk/incubator/vector/VectorOperators.opCode() is not inlined with Byte,Short,Long add
// 20) Fix TR_ALLOC
