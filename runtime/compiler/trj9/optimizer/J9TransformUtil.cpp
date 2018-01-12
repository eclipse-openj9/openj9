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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "optimizer/TransformUtil.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "env/CompilerEnv.hpp"
#include "il/Block.hpp"
#include "il/Block_inlines.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "il/symbol/StaticSymbol_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/VMJ9.h"
#include "ras/DebugCounter.hpp"
#include "j9.h"
#include "optimizer/OMROptimization_inlines.hpp"



TR::Node *
J9::TransformUtil::generateArrayElementShiftAmountTrees(
      TR::Compilation *comp,
      TR::Node *object)
   {
   TR::Node* shiftAmount;
   TR::SymbolReferenceTable* symRefTab = comp->getSymRefTab();

   shiftAmount = TR::Node::createWithSymRef(TR::aloadi, 1, 1,object,symRefTab->findOrCreateVftSymbolRef());
   shiftAmount = TR::Node::createWithSymRef(TR::aloadi, 1, 1,shiftAmount,symRefTab->findOrCreateArrayClassRomPtrSymbolRef());
   shiftAmount = TR::Node::createWithSymRef(TR::iloadi, 1, 1,shiftAmount,symRefTab->findOrCreateIndexableSizeSymbolRef());
   return shiftAmount;
   }

//
// A few predicates describing shadow symbols that we can reason about at
// compile time.  Note that "final field" here doesn't rule out a pointer to a
// Java object, as long as it always points at the same object.
//
// {{{
//

static bool isFinalFieldOfNativeStruct(TR::SymbolReference *symRef, TR::Compilation *comp)
   {
   switch (symRef->getReferenceNumber() - comp->getSymRefTab()->getNumHelperSymbols())
      {
      case TR::SymbolReferenceTable::componentClassSymbol:
      case TR::SymbolReferenceTable::arrayClassRomPtrSymbol:
      case TR::SymbolReferenceTable::indexableSizeSymbol:
      case TR::SymbolReferenceTable::isArraySymbol:
      case TR::SymbolReferenceTable::classRomPtrSymbol:
      case TR::SymbolReferenceTable::ramStaticsFromClassSymbol:
         TR_ASSERT(symRef->getSymbol()->isShadow(), "isFinalFieldOfNativeStruct expected shadow symbol");
         return true;
      default:
         return false;
      }
   }

static bool isFinalFieldPointingAtNativeStruct(TR::SymbolReference *symRef, TR::Compilation *comp)
   {
   switch (symRef->getReferenceNumber() - comp->getSymRefTab()->getNumHelperSymbols())
      {
      case TR::SymbolReferenceTable::componentClassSymbol:
      case TR::SymbolReferenceTable::arrayClassRomPtrSymbol:
      case TR::SymbolReferenceTable::classRomPtrSymbol:
      case TR::SymbolReferenceTable::classFromJavaLangClassSymbol:
      case TR::SymbolReferenceTable::classFromJavaLangClassAsPrimitiveSymbol:
      case TR::SymbolReferenceTable::ramStaticsFromClassSymbol:
      case TR::SymbolReferenceTable::vftSymbol:
         TR_ASSERT(symRef->getSymbol()->isShadow(), "isFinalFieldPointingAtNativeStruct expected shadow symbol");
         return true;
      default:
         return false;
      }
   }

static bool isFinalFieldPointingAtRepresentableNativeStruct(TR::SymbolReference *symRef, TR::Compilation *comp)
   {
   // A "representable native struct" can be turned into a node that is not a field load,
   // such as a const or a loadaddr.  Most native structs are not "representable" because
   // we don't have infrastructure for them such as AOT relocations.
   //
   switch (symRef->getReferenceNumber() - comp->getSymRefTab()->getNumHelperSymbols())
      {
      case TR::SymbolReferenceTable::componentClassSymbol:
      case TR::SymbolReferenceTable::classFromJavaLangClassSymbol:
      case TR::SymbolReferenceTable::classFromJavaLangClassAsPrimitiveSymbol:
         // Note: We could also do vftSymbol, except replacing those with
         // loadaddres mucks up indirect loads in ways the optimizer/codegen
         // isn't expecting yet
         TR_ASSERT(symRef->getSymbol()->isShadow(), "isFinalFieldPointingAtRepresentableNativeStruct expected shadow symbol");
         return true;
      default:
         return false;
      }
   }

static bool isJavaField(TR::SymbolReference *symRef, TR::Compilation *comp)
   {
   TR::Symbol *symbol = symRef->getSymbol();
   if (symbol->isShadow() &&
       (symRef->getCPIndex() >= 0 ||
        // java/lang/invoke/VarHandle.handleTable can be fabricated by VarHandleTransformer, thus does not have a valid cp index
        symbol->getRecognizedField() == TR::Symbol::Java_lang_invoke_VarHandle_handleTable))
      return true;

   return false;
   }

static bool isFieldOfJavaObject(TR::SymbolReference *symRef, TR::Compilation *comp)
   {
   TR::Symbol *symbol = symRef->getSymbol();
   if (isJavaField(symRef, comp))
      return true;
   else if (symbol->isShadow()) switch (symRef->getReferenceNumber() - comp->getSymRefTab()->getNumHelperSymbols())
      {
      case TR::SymbolReferenceTable::classFromJavaLangClassSymbol:
      case TR::SymbolReferenceTable::classFromJavaLangClassAsPrimitiveSymbol:
      case TR::SymbolReferenceTable::vftSymbol:
         return true;
      default:
         return false;
      }

   return false;
   }

static bool isFinalFieldPointingAtUnrepresentableNativeStruct(TR::SymbolReference *symRef, TR::Compilation *comp)
   {
   return isFinalFieldPointingAtNativeStruct(symRef, comp) && !isFinalFieldPointingAtRepresentableNativeStruct(symRef, comp);
   }

static bool isArrayWithConstantElements(TR::SymbolReference *symRef, TR::Compilation *comp)
   {
   TR::Symbol *symbol = symRef->getSymbol();
   if (symbol->isShadow() && !symRef->isUnresolved())
      {
      switch (symbol->getRecognizedField())
         {
         case TR::Symbol::Java_lang_invoke_VarHandle_handleTable:
            return true;
         default:
            break;
         }
      }
   return false;
   }

static bool isFabricatedVarHandleField(TR::SymbolReference *symRef, TR::Compilation *comp)
   {
   TR::Symbol *symbol = symRef->getSymbol();
   if (symbol->isShadow() && symRef->getCPIndex() < 0)
      {
      switch (symbol->getRecognizedField())
         {
         case TR::Symbol::Java_lang_invoke_VarHandle_handleTable:
            return true;
         default:
            break;
         }
      }
   return false;
   }

static bool verifyFieldAccess(void *curStruct, TR::SymbolReference *field, TR::Compilation *comp)
   {
   // Return true only if loading the given field from the given struct will
   // itself produce a verifiable value.  (Primitives are trivially verifiable.)
   //

   if (!curStruct)
      return false;

   TR_J9VMBase *fej9 = comp->fej9();

   if (isJavaField(field, comp))
      {
      // For Java fields, a "verifiable" access is one where we can check
      // whether curStruct is an object of the right type.  If we can't even
      // check that, then we shouldn't be in this function in the first place.

      TR_OpaqueClassBlock *objectClass = fej9->getObjectClass((uintptrj_t)curStruct);
      TR_OpaqueClassBlock *fieldClass = NULL;
      if (isFabricatedVarHandleField(field, comp))
         fieldClass = fej9->getSystemClassFromClassName("java/lang/invoke/VarHandle", strlen("java/lang/invoke/VarHandle"));
      else
         {
         //
         // TODO: getDeclaringClassFromFieldOrStatic would be better, but it's a
         // little scarier because it's new.  getClassFromFieldOrStatic should be
         // more conservative in this case, so probably more suitable for
         // r13.java.
         fieldClass = field->getOwningMethod(comp)->getClassFromFieldOrStatic(comp, field->getCPIndex());
         }

      if (fieldClass == NULL)
         return false;

      TR_YesNoMaybe objectContainsField = fej9->isInstanceOf(objectClass, fieldClass, true);
      return objectContainsField == TR_yes;
      }
   else if (comp->getSymRefTab()->isImmutableArrayShadow(field))
      {
      TR_OpaqueClassBlock *arrayClass = fej9->getObjectClass((uintptrj_t)curStruct);
      if (!fej9->isClassArray(arrayClass) ||
          (field->getSymbol()->isCollectedReference() &&
          fej9->isPrimitiveArray(arrayClass)) ||
          (!field->getSymbol()->isCollectedReference() &&
          fej9->isReferenceArray(arrayClass)))
         return false;

      return true;
      }
   else if (isFieldOfJavaObject(field, comp))
      {
      // For special shadows representing data in Java objects, we need to verify the Java object types.
      TR_OpaqueClassBlock *objectClass = fej9->getObjectClass((uintptrj_t)curStruct);
      switch (field->getReferenceNumber() - comp->getSymRefTab()->getNumHelperSymbols())
         {
         case TR::SymbolReferenceTable::vftSymbol:
            return true; // Every java object has a vft pointer
         case TR::SymbolReferenceTable::classFromJavaLangClassSymbol:
         case TR::SymbolReferenceTable::classFromJavaLangClassAsPrimitiveSymbol:
            return objectClass == fej9->getClassClassPointer(objectClass);
         default:
            TR_ASSERT(false, "Cannot verify unknown field of java object");
            return false;
         }
      }
   else if (isFinalFieldOfNativeStruct(field, comp))
      {
      // These are implicitly verified by virtue of being verifiable
      //
      return true;
      }
   else
      {
      // Don't know how to verify this
      //
      return false;
      }

   return true;
   }


/**
 * Dereference through indirect load chain and return the address of field for curNode
 *
 * @param baseStruct The value of baseNode
 * @param curNode The field to be dereferenced
 * @param comp The compilation object needed in the dereference process
 *
 * @return The address of the field or NULL if dereference failed due to incorrect trees or types
 *
 * The concepts of "verified" and "verifiable" are used here, they're explained in
 * J9::TransformUtil::transformIndirectLoadChainImpl
 */
static void *dereferenceStructPointerChain(void *baseStruct, TR::Node *baseNode, TR::Node *curNode, TR::Compilation *comp)
   {
   if (baseNode == curNode)
      {
      TR_ASSERT(false, "dereferenceStructPointerChain has no idea what to dereference");
      traceMsg(comp, "Caller has already dereferenced node %p, returning NULL as dereferenceStructPointerChain has no idea what to dereference\n", curNode);
      return NULL;
      }
   else
      {
      TR_ASSERT(curNode != NULL, "Field node is NULL");
      TR_ASSERT(curNode->getOpCode().hasSymbolReference(), "Node must have a symref");

      TR::SymbolReference *symRef = curNode->getSymbolReference();
      TR::Symbol *symbol = symRef->getSymbol();
      TR::Node *addressChildNode = symbol->isArrayShadowSymbol() ? curNode->getFirstChild()->getFirstChild() : curNode->getFirstChild();

      // The addressChildNode must has a symRef so that we can verify it
      if (!addressChildNode->getOpCode().hasSymbolReference())
         return NULL;

      // Use uintptrj_t for pointer arithmetic operations and to save type conversions
      uintptrj_t curStruct = 0;
      if (addressChildNode == baseNode)
         {
         // baseStruct is the value of baseNode, dereference is not needed
         curStruct = (uintptrj_t)baseStruct;
         // baseStruct/baseNode are deemed verifiable by the caller         }
         }
      else
         {
         TR::SymbolReference *addressChildSymRef = addressChildNode->getSymbolReference();
         // Get the address of struct containing current field and dereference it
         void* addressChildAddress = dereferenceStructPointerChain(baseStruct, baseNode, addressChildNode, comp);

         if (addressChildAddress == NULL)
            {
            return NULL;
            }
         // Since we're going to dereference a field from addressChild, addressChild must be a java reference or a native struct
         else if (addressChildSymRef->getSymbol()->isCollectedReference())
            {
            curStruct = comp->fej9()->getReferenceFieldAtAddress((uintptrj_t)addressChildAddress);
            }
         else // Native struct
            {
            TR_ASSERT(isFinalFieldPointingAtNativeStruct(addressChildSymRef, comp), "dereferenceStructPointerChain should be dealing with reference fields");
            curStruct = *(uintptrj_t*)addressChildAddress;
            }
         }

      // Get the field address of curNode
      if (curStruct)
         {
         if (verifyFieldAccess((void*)curStruct, symRef, comp))
            {
            uintptrj_t fieldAddress = 0;
            // The offset of a java field is in its symRef
            if (isJavaField(symRef, comp))
               {
               fieldAddress = curStruct + symRef->getOffset();
               }
            else if (comp->getSymRefTab()->isImmutableArrayShadow(symRef))
               {
               TR::Node* offsetNode = curNode->getFirstChild()->getSecondChild();
               if (!offsetNode->getOpCode().isLoadConst())
                  return NULL;

               int64_t offset = 0;
               if (offsetNode->getDataType() == TR::Int64)
                  offset = offsetNode->getUnsignedLongInt();
               else
                  offset = offsetNode->getUnsignedInt();

               uint64_t arrayLengthInBytes = TR::Compiler->om.getArrayLengthInBytes(comp, curStruct);
               int64_t minOffset = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
               int64_t maxOffset = arrayLengthInBytes + TR::Compiler->om.contiguousArrayHeaderSizeInBytes();

               // Check array bound
               if (offset < minOffset ||
                   offset >= maxOffset)
                  {
                  traceMsg(comp, "Offset %d is out of bound [%d, %d] for %s on array shadow %p!\n", offset, minOffset, maxOffset, symRef->getName(comp->getDebug()), curNode);
                  return NULL;
                  }

               fieldAddress = TR::Compiler->om.getAddressOfElement(comp, curStruct, offset);
               }
            else
               {
               TR_ASSERT(isFinalFieldPointingAtNativeStruct(symRef, comp), "dereferenceStructPointerChain should be dealing with reference fields");
               fieldAddress = curStruct + symRef->getOffset();
               }

            return (void*)fieldAddress;
            }
         else
            {
            traceMsg(comp, "Unable to verify field access to %s on %p!\n", symRef->getName(comp->getDebug()), curNode);
            return NULL;
            }
         }
      else
         {
         return NULL;
         }
      }

   TR_ASSERT(0, "Should never get here");
   return NULL;
   }

static bool foldFinalFieldsIn(TR_OpaqueClassBlock *clazz, char *className, int32_t classNameLength, bool isStatic, TR::Compilation *comp)
   {
   TR::SimpleRegex *classRegex = comp->getOptions()->getClassesWithFoldableFinalFields();
   if (classRegex)
      return TR::SimpleRegex::match(classRegex, className);
   else if (classNameLength >= 17 && !strncmp(className, "java/lang/invoke/", 17))
      return true; // We can ONLY do this opt to fields that are never victimized by setAccessible
   else if (classNameLength >= 30 && !strncmp(className, "java/lang/String$UnsafeHelpers", 30))
      return true;
   
   // Fold static final fields in java/lang/String* for string compression flag
   else if (classNameLength >= 16 && !strncmp(className, "java/lang/String", 16))
      return true;
   else if (classNameLength >= 22 && !strncmp(className, "java/lang/StringBuffer", 22))
      return true;
   else if (classNameLength >= 23 && !strncmp(className, "java/lang/StringBuilder", 23))
      return true;
   else if (classNameLength >= 17 && !strncmp(className, "com/ibm/oti/vm/VM", 17))
      return true;
   else if (classNameLength >= 22 && !strncmp(className, "com/ibm/jit/JITHelpers", 22))
      return true;
   else if (classNameLength >= 23 && !strncmp(className, "java/lang/J9VMInternals", 23))
      return true;
   else if (classNameLength >= 34 && !strncmp(className, "java/util/concurrent/atomic/Atomic", 34))
      return true;
   else if (classNameLength >= 17 && !strncmp(className, "java/util/EnumMap", 17))
      return true;

   static char *enableJCLFolding = feGetEnv("TR_EnableJCLStaticFinalFieldFolding");
   if ((enableJCLFolding || TR::Options::getCmdLineOptions()->getOption(TR_AggressiveOpts))
       && isStatic
       && comp->fej9()->isClassLibraryClass(clazz)
       && comp->fej9()->isClassInitialized(clazz))
      {
      if (classNameLength >= 16 && !strncmp(className, "java/lang/System", 16))
         return false;
      return true;
      }
   return false;
   }


static bool changeIndirectLoadIntoConst(TR::Node *node, TR::ILOpCodes opCode, TR::Node **removedChild, TR::Compilation *comp)
   {
   // Note that this only does part of the job.  Caller must actually set the
   // constant value / symref / anything else that may be necessary.
   //
   TR::ILOpCode opCodeObject; opCodeObject.setOpCodeValue(opCode);
   if (performTransformation(comp, "O^O transformIndirectLoadChain: change %s [%p] into %s\n", node->getOpCode().getName(), node, opCodeObject.getName()))
      {
      *removedChild = node->getFirstChild();
      node->setNumChildren(0);
      TR::Node::recreate(node, opCode);
      node->setFlags(0);
      return true;
      }
   return false;
   }

TR::Node *
J9::TransformUtil::transformStringIndexOfCall(TR::Compilation * comp, TR::Node * callNode)
   {
   TR_J9VMBase *fej9 = comp->fej9();
   TR_ASSERT(callNode->getNumChildren() == 2, "unexpected numChildren on String.indexOf(String) call");
   TR::ResolvedMethodSymbol * resolvedMethodSymbol =
      callNode->getSymbolReference()->getSymbol()->castToResolvedMethodSymbol();

   TR::Node * needle = callNode->getChild(1);

   if (needle->getOpCodeValue() != TR::aload)
      return callNode;
   if (!needle->getSymbolReference()->getSymbol()->isConstObjectRef())
      return callNode;
   if (needle->getSymbolReference()->isUnresolved())
      return callNode;

   // Everything below will be run with VM Access in hand
   TR::VMAccessCriticalSection transformStringIndexCriticalSection(fej9,
                                                                    TR::VMAccessCriticalSection::tryToAcquireVMAccess,
                                                                    comp);

   if (!transformStringIndexCriticalSection.hasVMAccess())
      return callNode;

   uintptrj_t string = *(uintptrj_t*)needle->getSymbol()->castToStaticSymbol()->getStaticAddress();
   int32_t len = fej9->getStringLength(string);
   if (len < 3)
      return callNode;

   TR_OpaqueClassBlock * stringClass = comp->getStringClassPointer();
   TR_ScratchList<TR_ResolvedMethod> stringMethods(comp->trMemory());
   fej9->getResolvedMethods(comp->trMemory(), stringClass, &stringMethods);
   ListIterator<TR_ResolvedMethod> it(&stringMethods);
   TR_ResolvedMethod * method;
   for (method = it.getCurrent(); method; method = it.getNext())
      {
      char name[] = "indexOf";
      char sig[]  = "(Ljava/lang/String;Ljava/lang/String;IIC)I";
      if (!strncmp(method->nameChars(), name, strlen(name)) &&
          !strncmp(method->signatureChars(), sig, strlen(sig)))
         break;
      }

   if (!method)
      return callNode; // fast String.indexOf not supported by the class libraries

   // HACK: if this is first time we are doing this transformation, change the invocation
   // count of the method - otherwise the optimizer will think that no one is calling this
   // method is that this method is cold
   //
   intptrj_t count = fej9->getInvocationCount((TR_OpaqueMethodBlock *) method->getPersistentIdentifier());
   if (count == comp->getOptions()->getCmdLineOptions()->getInitialBCount())
      fej9->setInvocationCount((TR_OpaqueMethodBlock *) method->getPersistentIdentifier(), count, count / 2);

   // Pattern Matched !

   TR::SymbolReferenceTable * symRefTab = comp->getSymRefTab();
   TR::SymbolReference * newSymRef =
      symRefTab->findOrCreateMethodSymbol(callNode->getSymbolReference()->getOwningMethodIndex(), -1, method, TR::MethodSymbol::Static);

   uint16_t lastChar = fej9->getStringCharacter(string, len-1);

   int32_t  cache    = 0;
   int32_t  md2      = len-1;

   int32_t  i;
   for (i = len-1; i >= 0; --i)
      {
      uint16_t thisChar = fej9->getStringCharacter(string, i);
      cache |= (1 << (thisChar & 31));
      }

   for (i = len-2; i >= 0; --i)
      {
      uint16_t thisChar = fej9->getStringCharacter(string, i);
      if (lastChar == thisChar)
         {
         md2 -= i;
         break;
         }
      }

   TR::Node * haystack = callNode->getChild(0);
   haystack->decReferenceCount();
   needle->decReferenceCount();

   TR::Node * node = TR::Node::createWithSymRef(TR::icall, 5, newSymRef);
   node->setAndIncChild(0, haystack);
   node->setAndIncChild(1, needle);
   node->setAndIncChild(2, TR::Node::create(TR::iconst, 0, cache));
   node->setAndIncChild(3, TR::Node::create(TR::iconst, 0, md2));
   node->setAndIncChild(4, TR::Node::create(TR::iconst, 0, lastChar));

   node->incReferenceCount();

   return node;
   }


TR::Node *
J9::TransformUtil::transformIndirectLoad(TR::Compilation *comp, TR::Node *node)
   {
   // TODO: This code does lots of refcount decrementing, which is not
   // safe if we don't anchor the child trees first!
   //
   static char *enableTransformIndirectLoad = feGetEnv("TR_enableTransformIndirectLoad");
   if (!enableTransformIndirectLoad)
      return NULL;

   TR_J9VMBase *fej9 = comp->fej9();

   TR_ASSERT(node->getOpCode().isLoadIndirect(), "Expecting indirect load; found %s %p", node->getOpCode().getName(), node);
   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::Symbol           *sym    = node->getSymbol();

   if (!symRef->isUnresolved()
      && sym->isShadow())
      {
      TR::Node *baseObject = node->getFirstChild();

      // Start with a few field-specific goodies
      //
      switch (sym->getRecognizedField())
         {
         case TR::Symbol::Java_lang_invoke_MethodHandle_thunks:
         case TR::Symbol::Java_lang_invoke_DynamicInvokerHandle_site:
         case TR::Symbol::Java_lang_invoke_MutableCallSiteDynamicInvokerHandle_mutableSite:
            if (!node->isNonNull() && performTransformation(comp, "O^O transformIndirectLoad: [%p] recognized field is never null\n", node))
               {
               node->setIsNull(false);
               node->setIsNonNull(true);
               }
            break;
         default:
            // Here, we can check a few cases where the "base object" is not
            // actually an object at all.
            //
            if (symRef == comp->getSymRefTab()->findJavaLangClassFromClassSymbolRef()
               && !symRef->hasKnownObjectIndex()
               && baseObject->getOpCodeValue() == TR::loadaddr
               && !baseObject->getSymbolReference()->isUnresolved())
               {
               TR::SymbolReference *improvedSymRef;

                  {
                  TR::VMAccessCriticalSection createSymRefWithKnownObject(comp->fej9());
                  uintptrj_t jlClass = (uintptrj_t)J9VM_J9CLASS_TO_HEAPCLASS((J9Class*)baseObject->getSymbol()->castToStaticSymbol()->getStaticAddress());
                  TR_ASSERT(jlClass, "java/lang/Class reference from heap class must be non null");
                  improvedSymRef = comp->getSymRefTab()->findOrCreateSymRefWithKnownObject(node->getSymbolReference(), &jlClass);
                  }

               if (improvedSymRef->hasKnownObjectIndex()
                   && performTransformation(comp, "O^O transformIndirectLoad: [%p] use object-specific symref #%d (=obj%d) for load of java/lang/Class\n",
                                            node,
                                            improvedSymRef->getReferenceNumber(),
                                            improvedSymRef->getKnownObjectIndex()))
                  {
                  node->setSymbolReference(improvedSymRef);
                  node->setIsNull(false);
                  node->setIsNonNull(true);
                  return node;
                  }
               }
            break;
         }

      // Check for loads of final primitive fields on objects that have finished initializing.
      //
      if (1) //sym->isFinal())
         {
         J9Class *fieldClass = (J9Class*)symRef->getOwningMethod(comp)->getClassFromFieldOrStatic(comp, symRef->getCPIndex());
         if (!fieldClass)
            return NULL;

         int32_t len;
         char * name = fej9->getClassNameChars((TR_OpaqueClassBlock*)fieldClass, len);
         if (sym->getRecognizedField() != TR::Symbol::assertionsDisabled
             && !foldFinalFieldsIn((TR_OpaqueClassBlock *)fieldClass, name, len, sym->isStaticField(), comp))
            return NULL;

         // Note that the load type can differ from the type of the symbol, eg.
         // for sub-integer types.  The sub-integer types are included below
         // just for completeness, but we likely never hit them.
         //
         TR::DataType loadType = node->getDataType();
         bool typeIsConstible    = false;
         bool symrefIsImprovable = false;
         switch (loadType)
            {
            case TR::Int32:
            case TR::Int64:
               typeIsConstible = true;
               break;
            case TR::Address:
               symrefIsImprovable = !symRef->hasKnownObjectIndex();
               break;
            default:
                break;
            }

         TR::Node *firstDereference = NULL;
         TR::Node *secondDereference = NULL;
         if (isFinalFieldOfNativeStruct(symRef, comp))
            {
            // Ok, this is a bit complicated.
            //
            // We have an expression y.z where y is a native struct and z is
            // some final field within that native struct.  Because y is a
            // child node of y.z we can assume it has already been simplified
            // because the optimizer does such things in a bottom-up fashion.
            // If y happens to be a so-called "representable" native struct
            // like a J9Class, then we're in luck because we can just read the
            // field out of y at compile time.
            //
            // However, we also want to handle situations where y is not
            // "representable", meaning it can only appear in the trees as a
            // field load from some other base object x.  In that case, we
            // must handle the whole expression x.y.z in one chunk, because
            // we can't simplify x.y nor y.z independently.
            //
            // Generalizing, we can end up with an arbitrarily long reference
            // chain a.b.c...y.z where b...y are final fields pointing at
            // unrepresentable native structs, and we must collapse this whole
            // chain in one chunk, or not at all.
            //
            // The following logic detects such chains, and finishes with
            // baseObject = a (the representable base expression), and
            // firstDereference = a.b and secondDereference = a.b.c (or NULL,
            // if the expression is just a.b and a is representable).
            // Note that our analysis starts from the "z" dereference, because
            // that is the root TR::Node in this expression tree, and works its
            // way down the tree to "a".

            if (typeIsConstible) // can't yet tell whether the expression chain results in a Java object, so just do primitives
               {
               while (baseObject->getOpCode().isLoadIndirect() && isFinalFieldPointingAtUnrepresentableNativeStruct(baseObject->getSymbolReference(), comp))
                  {
                  secondDereference = firstDereference;
                  firstDereference  = baseObject;
                  baseObject        = firstDereference->getFirstChild();
                  }

               // at this point, baseObject is some representable expression
               J9Class *clazz = NULL;
               if (baseObject->getOpCode().isLoadIndirect())
                  {
                  if (isFinalFieldPointingAtNativeStruct(baseObject->getSymbolReference(), comp))
                     {
                     TR_ASSERT(isFinalFieldPointingAtRepresentableNativeStruct(baseObject->getSymbolReference(), comp), "Base expression pointing at native struct must be representable"); // Good luck to the poor soul doing triage trying to figure out what this means

                     // Don't bother continuing.  Under normal circumstances, a
                     // representable base object expression should already
                     // have gone through this process and been simplified into
                     // a const or loadaddr.
                     dumpOptDetails(comp, "Could have transformed %p if representable baseObject %p had already been simplified\n", node, baseObject);
                     return NULL;
                     }
                  else
                     {
                     // baseObject is an expression resulting in a Java object reference.
                     // The logic below handles figuring out the address of the base Java
                     // object.  Proceed with that, then do the dereference below.
                     }
                  }
               else if (baseObject->chkClassPointerConstant())
                  {
                  clazz = (J9Class*)baseObject->getAddress();
                  }
               else if (baseObject->getOpCodeValue() == TR::loadaddr
                     && baseObject->getSymbol()->isClassObject())
                  {
                  clazz = (J9Class*)baseObject->getSymbol()->castToStaticSymbol()->getStaticAddress();
                  }
               else
                  {
                  // TODO: The above checks are too conservative.  There's no
                  // need to check for class pointers explicitly.  Once we know
                  // it's a representable object, we can do getAddress, getInt,
                  // getLongInt, or getStaticAddress depending on the opcode,
                  // and use whatever we get back from that to start dereferencing.
                  dumpOptDetails(comp, "Can't yet transform %p based on representable baseObject %p that isn't a J9Class\n", node, baseObject);
                  return NULL;
                  }

               if (clazz && performTransformation(comp, "O^O transformIndirectLoad: [%p] evaluate native structure walk into const from %s, based on class pointer node %p\n", node, node->getSymbolReference()->getName(comp->getDebug()), baseObject))
                  {
                  // We have clazz.a.b.c.d.primitiveField
                  // We can simplify this into a const.
                  //
                  uint8_t *structure = (uint8_t*)dereferenceStructPointerChain(clazz, baseObject, node->getFirstChild(), comp);
                  if (verifyFieldAccess(structure, node->getSymbolReference(), comp)) switch (loadType)
                     {
                     case TR::Int32:
                        node = TR::Node::iconst(node, *(int32_t*)(structure + node->getSymbolReference()->getOffset()));
                        break;
                     case TR::Int64:
                        node = TR::Node::lconst(node, *(int64_t*)(structure + node->getSymbolReference()->getOffset()));
                        break;
                     default:
                        TR_ASSERT(0, "Unexpected native field type %s", node->getDataType().toString());
                        break;
                     }
                  return node;
                  }
               }
            }

         uintptrj_t *baseObjectRefLocation = NULL;
         if (baseObject->getOpCode().hasSymbolReference() && baseObject->getSymbolReference()->hasKnownObjectIndex())
            {
            baseObjectRefLocation = baseObject->getSymbolReference()->getKnownObjectReferenceLocation(comp);
            }
         else if (baseObject->getOpCode().isLoadVarDirect() && baseObject->getSymbol()->isStatic())
            {
            TR::StaticSymbol *baseSym = baseObject->getSymbol()->castToStaticSymbol();
            if (!baseObject->getSymbolReference()->isUnresolved())
               {
               // Note: There is an implicit assumption here that if we can reach
               // a const object, then its constructor must have completed and its
               // final fields will never change again.  Since an object can't be
               // reached through the constant pool or call site table until its
               // constructor completes, there is no known case where this is false,
               // but if one ever arises, we'll need to check for that case here.
               //
               if (baseSym->isFixedObjectRef())
                  baseObjectRefLocation = (uintptrj_t*)baseSym->getStaticAddress();
               }
            }

         if (!baseObjectRefLocation)
            return NULL; // Nothing left we can do

         //
         // Now we know we have an indirect from an identifiable location.
         // We can use this information to simplify loads of final fields.
         //

         if (isFinalFieldOfNativeStruct(symRef, comp))
            {
            if (baseObjectRefLocation && typeIsConstible)
               {
               if (performTransformation(comp, "O^O transformIndirectLoad: [%p] evaluate native structure walk into const from %s, based on known object node %p\n", node, node->getSymbolReference()->getName(comp->getDebug()), baseObject))
                  {
                  // We have object.a.b.c.d.primitiveField, where a.b.c.d walks through some native data structure.
                  // We can simplify this into a const.
                  //
                  bool loadFailed = false;
                  uintptrj_t fieldAddress = (*baseObjectRefLocation) + firstDereference->getSymbolReference()->getOffset();
                  uint8_t *structure;

                     {
                     // First dereference with VM access to dig a pointer out of an object
                     TR::VMAccessCriticalSection digPointerOutOfObject(comp->fej9());
                     switch (firstDereference->getDataType())
                        {
                        case TR::Int32:
                           {
                           uint32_t *intAddress = (uint32_t*)fieldAddress;
                           structure = (uint8_t*)(uintptr_t)(*intAddress);
                           }
                           break;
                        case TR::Int64:
                           {
                           uint64_t *longAddress = (uint64_t*)fieldAddress;
                           structure = (uint8_t*)(uintptr_t)(*longAddress);
                           }
                           break;
                        case TR::Address:
                           {
                           uintptrj_t *refAddress = (uintptrj_t*)fieldAddress;
                           structure = (uint8_t*)(uintptr_t)(*refAddress);
                           }
                           break;
                        default:
                           loadFailed = true;
                           break;
                        }
                     }

                  TR_ASSERT(!loadFailed, "Expected int, long, or address load");
                  TR_ASSERT(secondDereference, "Expected expression %p like a.b...c where a is a Java object, b points at a native struct, and c is a field in a native struct", node);
                  if (secondDereference)
                     structure = (uint8_t*)dereferenceStructPointerChain(structure, firstDereference, node->getFirstChild(), comp);

                  if (verifyFieldAccess(structure, node->getSymbolReference(), comp)) switch (loadType)
                     {
                     case TR::Int32:
                        node = TR::Node::iconst(node, *(int32_t*)(structure + node->getSymbolReference()->getOffset()));
                        break;
                     case TR::Int64:
                        node = TR::Node::lconst(node, *(int64_t*)(structure + node->getSymbolReference()->getOffset()));
                        break;
                     default:
                        TR_ASSERT(0, "Unexpected native field type %s", node->getDataType().toString());
                        break;
                     }
                  return node;
                  }
               }
            }
         else if (fej9->isFinalFieldPointingAtJ9Class(symRef, comp)
               && isFinalFieldPointingAtRepresentableNativeStruct(symRef, comp)
               && !comp->compileRelocatableCode())
            {
            TR_OpaqueClassBlock *clazz;

               {
               TR::VMAccessCriticalSection getClassFromJavaLangClassCS(comp->fej9());
               clazz = fej9->getClassFromJavaLangClass(*baseObjectRefLocation);
               }

            TR::Node *clazzNode = TR::Node::createWithSymRef(node, TR::loadaddr, 0,
               comp->getSymRefTab()->findOrCreateClassSymbol(comp->getMethodSymbol(), -1, clazz));
            if (performTransformation(comp, "O^O transformIndirectLoad: [%p] turn load of J9Class into %s %p\n", node, clazzNode->getOpCode().getName(), clazzNode))
               return clazzNode;
            }
         else if (sym->isFinal() && !comp->compileRelocatableCode()) // Constructor can set different values for the same field in different runs on AOT
            {
            int32_t fieldOffset = symRef->getOffset() - sizeof(J9Object); // blah
            if (typeIsConstible)
               {
               if (performTransformation(comp, "O^O transformIndirectLoad: [%p] turn final %s %s into load const\n", node, node->getOpCode().getName(), symRef->getName(comp->getDebug())))
                  {
                  TR::VMAccessCriticalSection recreate(comp->fej9());
                  node->getAndDecChild(0);
                  node->setNumChildren(0);
                  switch (loadType)
                     {
                     case TR::Int32:
                        TR::Node::recreate(node, TR::iconst);
                        node->setInt(fej9->getInt32FieldAt(*baseObjectRefLocation, fieldOffset));
                        break;
                     case TR::Int64:
                        TR::Node::recreate(node, TR::lconst);
                        node->setLongInt(fej9->getInt64FieldAt(*baseObjectRefLocation, fieldOffset));
                        break;
                     default:
                        TR_ASSERT(0, "Unexpected type %s", node->getDataType().toString());
                        break;
                     }
                  return node;
                  }
               }
            else if (symrefIsImprovable)
               {
               uintptrj_t targetObjectReference;
               TR::SymbolReference *improvedSymRef;

                  {
                  TR::VMAccessCriticalSection getReferenceField(comp->fej9());
                  targetObjectReference = fej9->getReferenceFieldAt(*baseObjectRefLocation, fieldOffset);
                  improvedSymRef = comp->getSymRefTab()->findOrCreateSymRefWithKnownObject(node->getSymbolReference(), &targetObjectReference);
                  }

               if (targetObjectReference)
                  {
                  if (improvedSymRef->hasKnownObjectIndex()
                     && performTransformation(comp, "O^O transformIndirectLoad: [%p] use object-specific symref #%d (=obj%d) for %s of final field %s\n",
                        node, improvedSymRef->getReferenceNumber(), improvedSymRef->getKnownObjectIndex(), node->getOpCode().getName(), symRef->getName(comp->getDebug())))
                     {
                     node->setSymbolReference(improvedSymRef);
                     node->setIsNull(false);
                     node->setIsNonNull(true);
                     }
                  }
               else
                  {
                  switch (improvedSymRef->getSymbol()->getRecognizedField())
                     {
                     // J9VMInternals.jitHelpers is initialized after the class has been
                     // initialized via an Unsafe helper - don't fold null in to improve perf
                     case TR::Symbol::Java_lang_J9VMInternals_jitHelpers:
                        return NULL;
                     default:
                        if (performTransformation(comp, "O^O transformIndirectLoad: [%p] field is null - change to aconst NULL\n", node))
                           {
                           node->getAndDecChild(0);
                           node->setNumChildren(0);
                           TR::Node::recreate(node, TR::aconst);
                           node->setAddress(0);
                           node->setIsNull(true);
                           node->setIsNonNull(false);
                           }
                     }
                  }
               return node;
               }
            }
         }
      }

   return NULL;
   }


bool
J9::TransformUtil::transformDirectLoad(TR::Compilation *comp, TR::Node *node)
   {
   TR_ASSERT(node->getOpCode().isLoadVarDirect(), "Expecting direct load; found %s %p", node->getOpCode().getName(), node);
   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::Symbol           *sym    = node->getSymbol();

   TR_J9VMBase *fej9 = comp->fej9();

   if (symRef->hasKnownObjectIndex())
      return false;

   // Check for loads of static final primitive fields on classes that have finished initializing.
   //
   if (!symRef->isUnresolved()
      && sym->isStaticField()
      && sym->isFinal())
      {
      J9Class *fieldClass = (J9Class*)symRef->getOwningMethod(comp)->getClassFromFieldOrStatic(comp, symRef->getCPIndex(), true);
      if (!fieldClass)
         return false;
      // Can't trust final statics unless class init is finished
      //
      bool classInitIsFinished = (fieldClass->initializeStatus & J9ClassInitStatusMask) == J9ClassInitSucceeded;
      if (!classInitIsFinished)
         return false;

      int32_t len;
      char * name = fej9->getClassNameChars((TR_OpaqueClassBlock*)fieldClass, len);
      if (!strncmp(name, "java/lang/System", len))
         return false; // Keep our hands off out/in/err, which are declared final but aren't really

      // We used to call foldFinalFieldsIn here, but since there's no known
      // reason NOT to fold final static fields, why hold back?
      //
      if (comp->getOption(TR_RestrictStaticFieldFolding)
          && sym->getRecognizedField() != TR::Symbol::assertionsDisabled
          && !foldFinalFieldsIn((TR_OpaqueClassBlock*)fieldClass, name, len, true, comp))
         return false;

      // Static initializer can produce different values in different runs
      // so for AOT we cannot allow this transformation. However for String
      // we will embed some bits in the aotMethodHeader for this method
      if (comp->compileRelocatableCode())
         {
         if (sym->getRecognizedField() == TR::Symbol::Java_lang_String_enableCompression)
            {
            // Add the flags in TR_AOTMethodHeader
            J9JITDataCacheHeader *aotMethodHeader = (J9JITDataCacheHeader *)comp->getAotMethodDataStart();
            TR_AOTMethodHeader *aotMethodHeaderEntry = (TR_AOTMethodHeader *)(aotMethodHeader + 1);
            aotMethodHeaderEntry->flags |= TR_AOTMethodHeader_UsesEnableStringCompressionFolding;
            TR_ASSERT(node->getDataType() == TR::Int32, "Java_lang_String_enableCompression must be Int32");
            bool fieldValue = *(int32_t*)sym->castToStaticSymbol()->getStaticAddress() != 0;
            bool compressionEnabled = IS_STRING_COMPRESSION_ENABLED_VM(static_cast<TR_J9VMBase *>(comp->fe())->getJ9JITConfig()->javaVM);
            TR_ASSERT((fieldValue && compressionEnabled) || (!fieldValue && !compressionEnabled), 
               "java/lang/String.enableCompression and javaVM->strCompEnabled must be in sync");
            if (fieldValue)
               aotMethodHeaderEntry->flags |= TR_AOTMethodHeader_StringCompressionEnabled;
            }
         else
            {
            return false;
            }
         }

      // Note that the load type can differ from the symbol type, eg. Java uses
      // integer loads for the sub-integer types.  The sub-integer types are
      // included below just for completeness, but we likely never hit them.
      //
      TR::DataType loadType = node->getDataType();
      bool typeIsConstible = false;
      bool symrefIsImprovable  = false;
      switch (loadType)
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Int64:
         case TR::Float:
         case TR::Double:
            typeIsConstible = true;
            break;
         case TR::Address:
            symrefIsImprovable = !symRef->hasKnownObjectIndex();
            break;
         default:
            break;
         }

      TR::StaticSymbol *staticSym = sym->castToStaticSymbol();
      if (typeIsConstible)
         {
         if (performTransformation(comp, "O^O transformDirectLoad: turn [%p] %s %s into load const\n", node, node->getOpCode().getName(), symRef->getName(comp->getDebug())))
            {
            TR::VMAccessCriticalSection isConsitble(comp->fej9());
            switch (loadType)
               {
               case TR::Int8:
                  TR::Node::recreate(node, TR::bconst);
                  node->setByte(*(int8_t*)staticSym->getStaticAddress());
                  break;
               case TR::Int16:
                  TR::Node::recreate(node, TR::sconst);
                  node->setShortInt(*(int16_t*)staticSym->getStaticAddress());
                  break;
               case TR::Int32:
                  TR::Node::recreate(node, TR::iconst);
                  node->setInt(*(int32_t*)staticSym->getStaticAddress());
                  break;
               case TR::Int64:
                  TR::Node::recreate(node, TR::lconst);
                  node->setLongInt(*(int64_t*)staticSym->getStaticAddress());
                  break;
               case TR::Float:
                  TR::Node::recreate(node, TR::fconst);
                  node->setFloat(*(float*)staticSym->getStaticAddress());
                  break;
               case TR::Double:
                  TR::Node::recreate(node, TR::dconst);
                  node->setDouble(*(double*)staticSym->getStaticAddress());
                  break;
               default:
                  TR_ASSERT(0, "Unexpected type %s", loadType.toString());
                  break;
               }
            }
         TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "foldFinalField.const/(%s)/%s/(%s)",
                                                            comp->signature(),
                                                            comp->getHotnessName(comp->getMethodHotness()),
                                                            symRef->getName(comp->getDebug())));
         return true;
         }
      else if (*(uintptrj_t*)staticSym->getStaticAddress() == 0) // Seems ok just to check for a static to be NULL without vm access
         {
         switch (staticSym->getRecognizedField())
            {
            // J9VMInternals.jitHelpers is initialized after the class has been
            // initialized via an Unsafe helper - don't fold null in to improve perf
            case TR::Symbol::Java_lang_J9VMInternals_jitHelpers:
               return false;
            default:
               if (performTransformation(comp, "O^O transformDirectLoad: [%p] field is null - change to aconst NULL\n", node))
                  {
                  TR::Node::recreate(node, TR::aconst);
                  node->setAddress(0);
                  node->setIsNull(true);
                  node->setIsNonNull(false);
                  TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "foldFinalField.null/(%s)/%s/(%s)", comp->signature(), comp->getHotnessName(comp->getMethodHotness()), symRef->getName(comp->getDebug())));
                  return true;
                  }
            }
         }
      else if (symrefIsImprovable)
         {
         uintptrj_t *refLocation = (uintptrj_t*)staticSym->getStaticAddress();
         TR::SymbolReference *improvedSymRef = comp->getSymRefTab()->findOrCreateSymRefWithKnownObject(node->getSymbolReference(), refLocation);
         if (improvedSymRef->hasKnownObjectIndex()
            && performTransformation(comp, "O^O transformDirectLoad: [%p] use object-specific symref #%d (=obj%d) for %s %s\n",
               node,
               improvedSymRef->getReferenceNumber(),
               improvedSymRef->getKnownObjectIndex(),
               node->getOpCode().getName(),
               symRef->getName(comp->getDebug())))
            {
            node->setSymbolReference(improvedSymRef);
            bool isNull = comp->getKnownObjectTable()->isNull(improvedSymRef->getKnownObjectIndex());
            node->setIsNull(isNull);
            node->setIsNonNull (!isNull);
            TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "foldFinalField.knownObject/(%s)/%s/(%s)", comp->signature(), comp->getHotnessName(comp->getMethodHotness()), symRef->getName(comp->getDebug())));
            return true;
            }
         }
      }

   return false; // Indicates we did nothing
   }

/** Dereference node and fold it into a constant when possible.
 *
 *  The jit can tolerate certain mismatches between the tree and the data to be
 *  manipulated as long as *baseReferenceLocation/baseExpression is "verifiable".
 *  see comment in J9::TransformUtil::transformIndirectLoadChainImpl
 */
bool
J9::TransformUtil::transformIndirectLoadChainAt(TR::Compilation *comp, TR::Node *node, TR::Node *baseExpression, uintptrj_t *baseReferenceLocation, TR::Node **removedNode)
   {
   TR::VMAccessCriticalSection transformIndirectLoadChainAt(comp->fej9());
   bool result = TR::TransformUtil::transformIndirectLoadChainImpl(comp, node, baseExpression, (void*)*baseReferenceLocation, removedNode);
   return result;
   }

/** Dereference node and fold it into a constant when possible.
 *
 *  The jit can tolerate certain mismatches between the tree and the data to be
 *  manipulated as long as baseKnownObject/baseExpression is "verifiable".
 *  In this case, since the data is a java object reference, baseExpression must
 *  be a java object reference in order to be "verifiable".
 *  Please see comment in J9::TransformUtil::transformIndirectLoadChainImpl
 */
bool
J9::TransformUtil::transformIndirectLoadChain(TR::Compilation *comp, TR::Node *node, TR::Node *baseExpression, TR::KnownObjectTable::Index baseKnownObject, TR::Node **removedNode)
   {
   TR::VMAccessCriticalSection transformIndirectLoadChain(comp->fej9());
   bool result = TR::TransformUtil::transformIndirectLoadChainImpl(comp, node, baseExpression, (void*)comp->getKnownObjectTable()->getPointer(baseKnownObject), removedNode);
   return result;
   }

/** Dereference node and fold it into a constant when possible
 *
 *  @parm comp The compilation object
 *  @parm node The node to be folded
 *  @parm baseExpression The start of the indirect load chain
 *  @parm baseAddress Value of baseExpression
 *  @parm removedNode Pointer to the removed node if removal happens
 *
 *  @return true if the load chain has been folded, false otherwise
 *
 *  IMPORTANT NOTE on "interpreting" code at compile time:
 *
 *  Sometimes we're looking at dead code on impossible paths. In that case,
 *  the node symbol info might not match the actual type of the data it is
 *  manipulating.
 *
 *  To make this facility easier to use, we have decided to tolerate certain
 *  kinds of mismatches between the trees and the actual data being examined.
 *  We return false to indicate we have been unable to perform the
 *  dereference.  (false is otherwise not a valid result of this function,
 *  since the caller of this function is always intending to dereference the
 *  result.)
 *
 *  The onus is on the caller to make sure baseAddress/baseExpression meets certain
 *  rules (see below) but beyond that, this function must verify that the
 *  actual data structure being walked matches the nodes closely enough to
 *  prevent the jit from doing something wrong.
 *
 *  This function must must verify that the structure we're loading from is
 *  of the correct type before the dereference.
 *
 *  In this narrow usage, "correct type" really means that subsequent
 *  dereferences are guaranteed to "work properly" in the sense that they
 *  won't crash the jit.  If the data does not match the trees, it is valid
 *  to return nonsensical answers (garbage in, garbage out) but it is not
 *  valid to crash.
 *
 *  To be able to perform the verification, the data/tree we're looking at must be
 *  "verifiable". What that means depends on the type of data we're looking at.
 *  In the case of a Java object, those are self-describing.  It is enough
 *  merely to know that a pointer is the address of a Java object in order to
 *  declare it "verifiable", and the verification step can do the rest.  In
 *  the case of a C struct, there is generally no way to verify it, so the
 *  verification step for a C struct is, of necessity, trivial, and therefore
 *  the "verifiable" criterion itself perform the entire verification.  (This
 *  corresponds to the fact that Java can do dynamic type checking while C
 *  does all type checking statically.)
 *
 *  This leads us to the following situation:
 *
 *    For Java object references:
 *     - Verifiable if it is known to point at an object or null
 *     - Verified if it is non-null and points to an object that contains the field being loaded
 *    For C structs pointers:
 *     - Verifiable if it is known to point to a struct of the proper type or null
 *     - Verified trivially by virtue of being verifiable and non null
 *
 *  The onus is on the caller to ensure that baseAddress/baseExpression is
 *  verifiable.  This is why we have separated the notion of "verifiable"
 *  from "verified": there is less burden on the caller if it only needs to
 *  ensure verifiability.
 *
 */
bool
J9::TransformUtil::transformIndirectLoadChainImpl(TR::Compilation *comp, TR::Node *node, TR::Node *baseExpression, void *baseAddress, TR::Node **removedNode)
   {
   TR_J9VMBase *fej9 = comp->fej9();

   TR_ASSERT(TR::Compiler->vm.hasAccess(comp), "transformIndirectLoadChain requires VM access");
   TR_ASSERT(node->getOpCode().isLoadIndirect(), "Expecting indirect load; found %s %p", node->getOpCode().getName(), node);
   TR_ASSERT(node->getNumChildren() == 1, "Expecting indirect load %s %p to have one child; actually has %d", node->getOpCode().getName(), node, node->getNumChildren());

   if (comp->compileRelocatableCode())
      {
      return false;
      }

   TR::SymbolReference *symRef = node->getSymbolReference();

   if (symRef->hasKnownObjectIndex())
      return false;

   // Fold initializeStatus field in J9Class whose finality is conditional on the value it is holding
   if (!symRef->isUnresolved() && symRef == comp->getSymRefTab()->findInitializeStatusFromClassSymbolRef())
      {
      J9Class* clazz = (J9Class*)baseAddress;
      traceMsg(comp, "Looking at node %p with initializeStatusFromClassSymbol, class %p initialize status is %d\n", node, clazz, clazz->initializeStatus);
      // Only fold the load if the class has been initialized
      if (clazz->initializeStatus == J9ClassInitSucceeded)
         {
         if (node->getDataType() == TR::Int32)
            {
            if (changeIndirectLoadIntoConst(node, TR::iconst, removedNode, comp))
               node->setInt(J9ClassInitSucceeded);
            else
               return false;
            }
         else
            {
            if (changeIndirectLoadIntoConst(node, TR::lconst, removedNode, comp))
               node->setLongInt(J9ClassInitSucceeded);
            else
               return false;
            }
         return true;
         }
      else
         return false;
      }

   if (!fej9->canDereferenceAtCompileTime(symRef, comp))
      {
      traceMsg(comp, "Abort transformIndirectLoadChain - cannot dereference at compile time!\n");
      return false;
      }

   // Dereference the chain starting from baseAddress and get the field address
   void *fieldAddress = dereferenceStructPointerChain(baseAddress, baseExpression, node, comp);
   if (!fieldAddress)
      {
      traceMsg(comp, "Abort transformIndirectLoadChain - cannot verify/dereference field access to %s in %p!\n", symRef->getName(comp->getDebug()), baseAddress);
      return false;
      }

   // The last step in the dereference chain is not necessarily an address.
   // Determine what it is and transform node appropriately.
   //
   TR::DataType loadType = node->getDataType();
   switch (loadType)
      {
      case TR::Int32:
         {
         int32_t value = *(int32_t*)fieldAddress;
         if (changeIndirectLoadIntoConst(node, TR::iconst, removedNode, comp))
            node->setInt(value);
         else
            return false;
         }
         break;
      case TR::Int64:
         {
         int64_t value = *(int64_t*)fieldAddress;
         if (changeIndirectLoadIntoConst(node, TR::lconst, removedNode, comp))
            node->setLongInt(value);
         else
            return false;
         }
         break;
      case TR::Float:
         {
         float value = *(float*)fieldAddress;
         if (changeIndirectLoadIntoConst(node, TR::fconst, removedNode, comp))
            node->setFloat(value);
         else
            return false;
         }
         break;
      case TR::Double:
         {
         double value = *(double*)fieldAddress;
         if (changeIndirectLoadIntoConst(node, TR::dconst, removedNode, comp))
            node->setDouble(value);
         else
            return false;
         }
         break;
      case TR::Address:
         {
         if (isFinalFieldPointingAtRepresentableNativeStruct(symRef, comp))
            {
            if (fej9->isFinalFieldPointingAtJ9Class(symRef, comp))
               {
               if (changeIndirectLoadIntoConst(node, TR::loadaddr, removedNode, comp))
                  {
                  TR_OpaqueClassBlock *value = *(TR_OpaqueClassBlock**)fieldAddress;
                  node->setSymbolReference(comp->getSymRefTab()->findOrCreateClassSymbol(comp->getMethodSymbol(), -1, value));
                  }
               else
                  {
                  return false;
                  }
               }
            else
               {
               // TODO: What about representable native structs that aren't J9Classes?
               // (Are there any?)
               return false;
               }
            }
         else if (isFinalFieldPointingAtNativeStruct(symRef, comp))
            {
            // Representable native structs are handled before now.  All
            // remaining natives are hopeless.
            return false;
            }
         else if (symRef->getSymbol()->isCollectedReference())
            {
            uintptrj_t value = fej9->getReferenceFieldAtAddress((uintptrj_t)fieldAddress);
            if (value)
               {
               TR::SymbolReference *improvedSymRef = comp->getSymRefTab()->findOrCreateSymRefWithKnownObject(symRef, &value, isArrayWithConstantElements(symRef, comp));
               if (  improvedSymRef->hasKnownObjectIndex()
                  && performTransformation(comp, "O^O transformIndirectLoadChain: %s [%p] with fieldOffset %d is obj%d referenceAddr is %p\n", node->getOpCode().getName(), node, improvedSymRef->getKnownObjectIndex(), symRef->getOffset(), value))
                  {
                  node->setSymbolReference(improvedSymRef);
                  node->setIsNull(false);
                  node->setIsNonNull(true);
                  }
               else
                  {
                  return false;
                  }
               }
            else
               {
               if (changeIndirectLoadIntoConst(node, TR::aconst, removedNode, comp))
                  {
                  node->setAddress(0);
                  node->setIsNull(true);
                  node->setIsNonNull(false);
                  }
               else
                  {
                  return false;
                  }
               }
            }
         else
            {
            return false;
            }
         }
         break;
      default:
         return false;
      }

   // If we don't change the node, we ought to return "false" before this
   // point.  However, "true" is a safe answer whenever the node MIGHT have
   // changed, so that's the best default thing to do here.
   //
   return true;
   }


bool
J9::TransformUtil::fieldShouldBeCompressed(TR::Node *node, TR::Compilation *comp)
   {
   //FIXME: remove check for j9class sig (isFieldClassObject)
   //when j9classes are allocated on the heap
   //

   TR::SymbolReferenceTable *symRefTab = comp->getSymRefTab();

   if (!node->getOpCode().hasSymbolReference())
      return false;

   int32_t numChildren = node->getNumChildren();
   if (numChildren > 0)
      {
      TR::Node *child = node->getFirstChild();

      if (child->getOpCode().isArrayRef())
         child = child->getFirstChild();

      if ((child->getOpCode().hasSymbolReference()) &&
          (child->getNumChildren() > 0))
         {
         child = child->getFirstChild();

         if (child->getOpCode().isArrayRef())
            child = child->getFirstChild();

         if (child->getOpCode().hasSymbolReference())
            {
            if (child->getSymbolReference() == symRefTab->findDLTBlockSymbolRef())
               return false;
            }
         }
      }

   TR::SymbolReference *symRef = node->getSymbolReference();

   // Unusual: a shadow symbol pointing at a heap object, yet it's not compressed.
   // There could be more than one symref for these, with different known-object info,
   // so we check the symbol itself.
   //
   // TODO: Why is symRef->getSymbol()->isCollectedReference() not sufficient?
   // The DLT block logic above is particularly strange.  Why do we need to
   // check if the child's symref is findDLTBlockSymbolRef?  Can't we tell from
   // the node's own shadow?
   //
   if (  symRefTab->findJavaLangClassFromClassSymbolRef() != NULL
      && symRefTab->findJavaLangClassFromClassSymbolRef()->getSymbol() == symRef->getSymbol())
      return false;

   TR::Symbol* symbol = symRef->getSymbol();

   if (symRef != symRefTab->findVftSymbolRef() &&
         symRef != symRefTab->findClassRomPtrSymbolRef() &&
         symRef != symRefTab->findArrayClassRomPtrSymbolRef() &&
         !symRefTab->isVtableEntrySymbolRef(symRef) &&
         (symRef != symRefTab->findClassFromJavaLangClassSymbolRef()) &&
         (symRef != symRefTab->findAddressOfClassOfMethodSymbolRef()) &&
         (symRef != symRefTab->findUnsafeSymbolRef(TR::Address, true, true, symbol->isVolatile())) &&
         !symbol->isStatic() &&
         (symbol->isCollectedReference() || symbol->isArrayletShadowSymbol() || symbol == symRefTab->findGenericIntShadowSymbol()) &&
         !(symbol->isUnsafeShadowSymbol() && symbol->getDataType() != TR::Address))
      return true;

   // return true in the case where we have an arrayset
   // node and its second child is an indirect reference
   // ex:
   //    arrayset
   //       aladd
   //          ...
   //
   //       aload <string "ok">...  -->string literal representing indirect reference
   //
   //       iconst
   //          ...
   if (node->getOpCodeValue() == TR::arrayset &&
         node->getSecondChild()->getDataType() == TR::Address)
      return true;

   return false;
   }


TR::Block *
J9::TransformUtil::insertNewFirstBlockForCompilation(TR::Compilation *comp)
   {
   TR::Node *oldBBStart = comp->getStartTree()->getNode();
   TR::Block *oldFirstBlock = comp->getStartTree()->getNode()->getBlock();
   TR::Node *glRegDeps=NULL;
   if (oldBBStart->getNumChildren() == 1)
      glRegDeps = oldBBStart->getChild(0);

   TR::Block *newFirstBlock = TR::Block::createEmptyBlock(oldBBStart, comp, oldFirstBlock->getFrequency());
   newFirstBlock->takeGlRegDeps(comp, glRegDeps);

   TR::CFG *cfg = comp->getFlowGraph();
   cfg->addNode(newFirstBlock, (TR_RegionStructure *)cfg->getStructure());

   cfg->join(newFirstBlock, oldFirstBlock);
   cfg->addEdge(TR::CFGEdge::createEdge(cfg->getStart(),  newFirstBlock, comp->trMemory()));
   comp->setStartTree(newFirstBlock->getEntry());

   return newFirstBlock;
   }

TR::Node * J9::TransformUtil::calculateElementAddress(TR::Compilation *comp, TR::Node *array, TR::Node *index, TR::DataType type)
{
   TR::Node * offset = TR::TransformUtil::calculateOffsetFromIndexInContiguousArray(comp, index, type);
   offset->setIsNonNegative(true);
   // Calculate element address
   TR::Node *addrCalc = NULL;
   if (TR::Compiler->target.is64Bit())
      addrCalc = TR::Node::create(TR::aladd, 2, array, offset);
   else
      addrCalc = TR::Node::create(TR::aiadd, 2, array, TR::Node::create(TR::l2i, 1, offset));

   return addrCalc;
}

/**
 * Calculate offset for an array element given its index
 *
 * @param index The index in the array of type Int32
 * @param type  The data type of the array element
 *
 * @return The Int64 value representing the offset into an array of the specified type given the index
 */
TR::Node * J9::TransformUtil::calculateOffsetFromIndexInContiguousArray(TR::Compilation *comp, TR::Node * index, TR::DataType type)
   {
   int32_t width = TR::Symbol::convertTypeToSize(type);
   // In compressedrefs mode, each element of an reference array is a compressed pointer,
   // modify the width accordingly, so the stride is 4bytes instead of 8
   //
   if (comp->useCompressedPointers() && type == TR::Address)
      width = TR::Compiler->om.sizeofReferenceField();

   int32_t shift = TR::TransformUtil::convertWidthToShift(width);
   int32_t headerSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();

   TR::Node * offset = index;

   // Return type must be Int64 and the index parameter is Int32 so we must do a conversion here
   if (TR::Compiler->target.is64Bit())
      offset = TR::Node::create(TR::i2l, 1, index);

   TR::ILOpCodes constOp = TR::Compiler->target.is64Bit() ? TR::lconst : TR::iconst;
   TR::ILOpCodes shlOp = TR::Compiler->target.is64Bit() ? TR::lshl : TR::ishl;
   TR::ILOpCodes addOp = TR::Compiler->target.is64Bit() ? TR::ladd : TR::iadd;

   if (shift)
      {
      TR::Node *shiftNode = TR::Node::create(TR::iconst, 0);
      shiftNode->setConstValue(shift);
      offset = TR::Node::create(shlOp, 2, offset, shiftNode);
      }
   if (headerSize > 0)
      {
      TR::Node *headerSizeNode = TR::Node::create(constOp, 0);
      headerSizeNode->setConstValue(headerSize);
      offset = TR::Node::create(addOp, 2, offset, headerSizeNode);
      }

   // Return type must be Int64 but on 32-bit platforms we carry out the above arithmetic using Int32 so this conversion is necessary
   if (!TR::Compiler->target.is64Bit())
      offset = TR::Node::create(TR::i2l, 1, offset);

   return offset;
   }

/**
 * Calculate element index given its offset in the array
 *
 * @param offset The offset in the array of type Int64
 * @param type   The data type of the array element
 *
 * @return The Int32 value representing the index into an array of the specified type given the offset
 */
TR::Node * J9::TransformUtil::calculateIndexFromOffsetInContiguousArray(TR::Compilation *comp, TR::Node * offset, TR::DataType type)
   {
   int32_t width = TR::Symbol::convertTypeToSize(type);
   // In compressedrefs mode, each element of an reference array is a compressed pointer,
   // modify the width accordingly, so the stride is 4bytes instead of 8
   //
   if (comp->useCompressedPointers() && type == TR::Address)
      width = TR::Compiler->om.sizeofReferenceField();

   int32_t shift = TR::TransformUtil::convertWidthToShift(width);
   int32_t headerSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();

   TR::Node * index = offset;

   // Offset must be in an Int32 range on 32-bit platforms so carry out the conversion early so arithmetic can be done using integers rather than longs
   if (!TR::Compiler->target.is64Bit())
      index = TR::Node::create(TR::l2i, 1, index);

   TR::ILOpCodes constOp = TR::Compiler->target.is64Bit() ? TR::lconst : TR::iconst;
   TR::ILOpCodes shrOp = TR::Compiler->target.is64Bit() ? TR::lshr : TR::ishr;
   TR::ILOpCodes subOp = TR::Compiler->target.is64Bit() ? TR::lsub : TR::isub;

   if (headerSize > 0)
      {
      TR::Node *headerSizeNode = TR::Node::create(constOp, 0);
      headerSizeNode->setConstValue(headerSize);
      index = TR::Node::create(subOp, 2, index, headerSizeNode);
      }
   if (shift)
      {
      TR::Node *shiftNode = TR::Node::create(constOp, 0);
      shiftNode->setConstValue(shift);
      index = TR::Node::create(shrOp, 2, index, shiftNode);
      }

   // Return type must be Int32 and the offset parameter is Int64 so we must do a conversion here
   if (TR::Compiler->target.is64Bit())
      index = TR::Node::create(TR::l2i, 1, index);

   return index;
   }


/**
 * \brief
 *    Create temps for a call's children and replace the call's children with loads from the newly created temps.
 *
 * \parm opt
 *    The optimization object asking for the transformation.
 *
 * \parm callTree
 *    The tree containing the call whose children are to be replaced with loads from temps
 */
void
J9::TransformUtil::createTempsForCall(TR::Optimization* opt, TR::TreeTop *callTree)
   {
   TR::Compilation* comp = opt->comp();
   TR::Node *callNode = callTree->getNode()->getFirstChild();
   if (opt->trace())
      traceMsg(comp,"Creating temps for children of call node %p\n", callNode);
   for (int32_t i = 0 ; i < callNode->getNumChildren() ; ++i)
      {
      TR::Node *child = callNode->getChild(i);


      //create a store of the correct type and insert before call.

      TR::DataType dataType = child->getDataType();
      TR::SymbolReference *newSymbolReference = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), dataType);

      TR::Node *storeNode = TR::Node::createStore(callNode, newSymbolReference, child);
      TR::TreeTop *storeTree = TR::TreeTop::create(comp, storeNode);
      if (opt->trace())
         traceMsg(comp,"Creating store node %p for child %p\n", storeNode, child);

      callTree->insertBefore(storeTree);

      // Replace the old child with a load of the new sym ref
      TR::Node *value = TR::Node::createLoad(callNode, newSymbolReference);
      if (opt->trace())
         traceMsg(comp,"Replacing call node %p child %p with %p\n",callNode, callNode->getChild(i),value);

      callNode->setAndIncChild(i, value);
      child->recursivelyDecReferenceCount();
      }
   }

/**
 * \brief
 *    Generate a diamond for a call.
 *
 * \parm opt
 *    The optimization object asking for the transformation.
 *
 * \parm callTree
 *    Tree of call for which the diamond will be generated.
 *
 * \parm compareTree
 *    The tree containing the if node
 *
 * \parm ifTree
 *    The tree to be taken when comparison succeeds.
 *
 * \parm elseTree
 *    The tree to be taken when comparison fails.
 *
 * \parm changeBlockExtensions
 *    If true the block containing the elseTree will be marked as an extension of the original block containing the call.
 *
 * \parm markCold
 *    If true the block containing the ifTree will be marked as cold.
 *
 * \note
 *    This function does not create temps for the call's children, please create temps for the call outside of this function if needed.
 *    CallTree will be removed, thus ifTree or elseTree cannot be the same as the callTree.
 */
void
J9::TransformUtil::createDiamondForCall(TR::Optimization* opt, TR::TreeTop *callTree, TR::TreeTop *compareTree, TR::TreeTop *ifTree, TR::TreeTop *elseTree, bool changeBlockExtensions, bool markCold)
   {
   TR_ASSERT(callTree != ifTree && callTree != elseTree, "callTree cannot be the same as ifTree nor elseTree");

   TR::Compilation* comp = opt->comp();
   if (opt->trace())
      traceMsg(comp, "Creating diamond for call tree %p with compare tree %p if tree %p and else tree %p\n", callTree, compareTree, ifTree, elseTree);

   TR::Node *callNode = callTree->getNode()->getFirstChild();
   // the call itself may be commoned, so we need to create a temp for the callnode itself
   TR::SymbolReference *newSymbolReference = 0;
   TR::DataType dataType = callNode->getDataType();
   if(callNode->getReferenceCount() > 1)
      {
      if (opt->trace())
         traceMsg(comp, "Creating temps for call node %p before generating the diamond\n", callNode);
      newSymbolReference = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), dataType);
      TR::Node::recreate(callNode, comp->il.opCodeForDirectLoad(dataType));
      callNode->setSymbolReference(newSymbolReference);
      callNode->removeAllChildren();
      }

   TR::Block *callBlock = callTree->getEnclosingBlock();

   callBlock->createConditionalBlocksBeforeTree(callTree, compareTree, ifTree, elseTree, comp->getFlowGraph(), changeBlockExtensions, markCold);

   // the original call will be deleted by createConditionalBlocksBeforeTree, but if the refcount was > 1, we need to insert stores.
   if (newSymbolReference)
      {
      TR::Node *ifStoreNode = TR::Node::createStore(callNode, newSymbolReference, ifTree->getNode()->getFirstChild());
      TR::TreeTop *ifStoreTree = TR::TreeTop::create(comp, ifStoreNode);

      ifTree->insertAfter(ifStoreTree);

      TR::Node *elseStoreNode = TR::Node::createStore(callNode, newSymbolReference, elseTree->getNode()->getFirstChild());
      TR::TreeTop *elseStoreTree = TR::TreeTop::create(comp, elseStoreNode);
      elseTree->insertAfter(elseStoreTree);
      if (opt->trace())
         traceMsg(comp, "Two store nodes %p and %p are inserted in the diamond\n", ifStoreNode, elseStoreNode);
      }
   }
