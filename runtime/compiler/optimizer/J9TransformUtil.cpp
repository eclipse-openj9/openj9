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

#include "optimizer/TransformUtil.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#if defined(J9VM_OPT_JITSERVER)
#include "control/CompilationRuntime.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */
#include "env/CompilerEnv.hpp"
#include "env/ClassTableCriticalSection.hpp"
#include "env/PersistentCHTable.hpp"
#include "il/AnyConst.hpp"
#include "il/Block.hpp"
#include "il/Block_inlines.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "infra/String.hpp"
#include "il/StaticSymbol.hpp"
#include "il/StaticSymbol_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/VMJ9.h"
#include "env/j9method.h"
#include "ras/DebugCounter.hpp"
#include "j9.h"
#include "optimizer/OMROptimization_inlines.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/HCRGuardAnalysis.hpp"

/**
 * Walks the TR_RegionStructure counting loops to get the nesting depth of the block
 */
int32_t J9::TransformUtil::getLoopNestingDepth(TR::Compilation *comp, TR::Block *block)
   {
   TR_RegionStructure *region = block->getParentStructureIfExists(comp->getFlowGraph());
   int32_t nestingDepth = 0;
   while (region && region->isNaturalLoop())
      {
      nestingDepth++;
      region = region->getParent();
      }
   return nestingDepth;
   }

/*
 * Generate trees for call to jitRetranslateCallerWithPrep to trigger recompilation from JIT-Compiled code.
 */
TR::TreeTop *
J9::TransformUtil::generateRetranslateCallerWithPrepTrees(TR::Node *node, TR_PersistentMethodInfo::InfoBits reason, TR::Compilation *comp)
   {
   TR::Node *callNode = TR::Node::createWithSymRef(node, TR::icall, 3, comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_jitRetranslateCallerWithPrep, false, false, true));
   callNode->setAndIncChild(0, TR::Node::create(node, TR::iconst, 0, reason));
   callNode->setAndIncChild(1, TR::Node::createWithSymRef(node, TR::loadaddr, 0, comp->getSymRefTab()->findOrCreateStartPCSymbolRef()));
   callNode->setAndIncChild(2, TR::Node::createWithSymRef(node, TR::loadaddr, 0, comp->getSymRefTab()->findOrCreateCompiledMethodSymbolRef()));
   TR::TreeTop *tt = TR::TreeTop::create(comp, TR::Node::create(TR::treetop, 1, callNode));
   return tt;
   }

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
         // loadaddr mucks up indirect loads in ways the optimizer/codegen
         // isn't expecting yet
         TR_ASSERT(symRef->getSymbol()->isShadow(), "isFinalFieldPointingAtRepresentableNativeStruct expected shadow symbol");
         return true;
      default:
         return false;
      }
   }

static bool isJavaField(TR::Symbol *symbol, int32_t cpIndex, TR::Compilation *comp)
   {
   if (symbol->isShadow() &&
       (cpIndex >= 0 ||
        // recognized fields are java fields
        symbol->getRecognizedField() != TR::Symbol::UnknownField))
      return true;

   return false;
   }

static bool isJavaField(TR::SymbolReference *symRef, TR::Compilation *comp)
   {
   return isJavaField(symRef->getSymbol(), symRef->getCPIndex(), comp);
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

static bool isNullValueAtAddress(TR::Compilation *comp, TR::DataType loadType, uintptr_t fieldAddress, TR::Symbol *field)
   {
   TR_J9VMBase *fej9 = comp->fej9();

   switch (loadType)
      {
      case TR::Int8:
         {
         int8_t value = *(int8_t*)fieldAddress;
         if (value == 0)
            return true;
         }
         break;
      case TR::Int16:
         {
         int16_t value = *(int16_t*)fieldAddress;
         if (value == 0)
            return true;
         }
         break;
      case TR::Int32:
         {
         int32_t value = *(int32_t*)fieldAddress;
         if (value == 0)
            return true;
         }
         break;
      case TR::Int64:
         {
         int64_t value = *(int64_t*)fieldAddress;
         if (value == 0)
            return true;
         }
         break;
      case TR::Float:
         {
         float value = *(float*)fieldAddress;
         // This will not fold -0.0 but will fold NaN
         if (value == 0.0)
            return true;
         }
         break;
     case TR::Double:
        {
        double value = *(double*)fieldAddress;
        // This will not fold -0.0 but will fold NaN
        if (value == 0.0)
           return true;
        }
        break;
     case TR::Address:
        {
        TR_ASSERT_FATAL(field->isCollectedReference(), "Expecting a collectable reference\n");
        uintptr_t value = fej9->getReferenceFieldAtAddress((uintptr_t)fieldAddress);
        if (value == 0)
           return true;
        }
        break;
     default:
        TR_ASSERT_FATAL(false, "Unknown type of field being dereferenced\n");
        break;
     }

   return false;
   }

bool J9::TransformUtil::avoidFoldingInstanceField(
   uintptr_t object,
   TR::Symbol *field,
   uint32_t fieldOffset,
   int cpIndex,
   TR_ResolvedMethod *owningMethod,
   TR::Compilation *comp)
   {
   TR_J9VMBase *fej9 = comp->fej9();

   TR_ASSERT_FATAL(fej9->haveAccess(), "avoidFoldingInstanceField requires VM access\n");

   TR_ASSERT_FATAL(
      isJavaField(field, cpIndex, comp),
      "avoidFoldingInstanceField: symbol %p is not a Java field shadow\n",
      field);

   TR_ASSERT_FATAL(
      fej9->canDereferenceAtCompileTimeWithFieldSymbol(field, cpIndex, owningMethod),
      "avoidFoldingInstanceField: symbol %p is never foldable (expected possibly foldable)\n",
      field);

   if (fej9->isStable(cpIndex, owningMethod, comp) && !field->isFinal())
      {
      uintptr_t fieldAddress = object + fieldOffset;
      TR::DataType loadType = field->getDataType();

      if (isNullValueAtAddress(comp, loadType, fieldAddress, field))
         return true;
      }

   switch (field->getRecognizedField())
      {
      // In the LambdaForm-based JSR292 implementation, CallSite declares a
      // target field to be inherited by all subclasses, including
      // MutableCallSite, and that field is declared final even though it's
      // actually mutable in instances of MCS.
      //
      // Because it is a final field declared in a trusted JCL package, loads
      // of this field would normally be folded to a known object. However,
      // folding the target in the case of a MCS can result in the spurious
      // removal of the MCS target guard, so refuse to fold in that case.
      //
      // VolatileCallSite also has a mutable target, but there should be no
      // need to check for it here because its implementation of getTarget()
      // reads it using Unsafe.getReferenceVolatile().
      //
      // This checks only for CallSite.target because field recognition is
      // based on the defining class even when the class named in the bytecode
      // is a subclass.
      //
      // In the non-LambdaForm implementation, MCS declares its own target
      // field, which is not final (and is in fact volatile), so we would not
      // attempt to fold it. Furthermore, MutableCallSite.target will not be
      // recognized as CallSite.target. Therefore, loads of CallSite.target
      // from MCS instances can be rejected unconditionally here without
      // affecting the non-LambdaForm implementation.
      //
      case TR::Symbol::Java_lang_invoke_CallSite_target:
         {
         TR_OpaqueClassBlock *objectClass =
            fej9->getObjectClass((uintptr_t)object);

         const char * const mcsName = "java/lang/invoke/MutableCallSite";
         TR_OpaqueClassBlock *mcsClass =
            fej9->getSystemClassFromClassName(mcsName, strlen(mcsName));

         // If MCS isn't loaded, then object isn't an instance of it.
         return mcsClass != NULL
            && fej9->isInstanceOf(objectClass, mcsClass, true) != TR_no;
         }

      // MethodHandle.form can change even though it's declared final, so don't
      // create a known object for it. Refinement doesn't rely on folding loads
      // of this field to a known object. (LambdaForm.vmentry doesn't need to
      // be listed here because it isn't even final.)
      case TR::Symbol::Java_lang_invoke_MethodHandle_form:
         return true;

      default:
         break;
      }

   return false;
   }

bool J9::TransformUtil::avoidFoldingInstanceField(
   uintptr_t object,
   TR::SymbolReference *field,
   TR::Compilation *comp)
   {
   return TR::TransformUtil::avoidFoldingInstanceField(
      object,
      field->getSymbol(),
      field->getOffset(),
      field->getCPIndex(),
      field->getOwningMethod(comp),
      comp);
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
         case TR::Symbol::Java_lang_invoke_BruteArgumentMoverHandle_extra:
         case TR::Symbol::Java_lang_invoke_MethodType_ptypes:
         case TR::Symbol::Java_lang_invoke_VarHandle_handleTable:
         case TR::Symbol::Java_lang_invoke_MethodHandleImpl_LoopClauses_clauses:
         case TR::Symbol::Java_lang_String_value:
            return true;
         default:
            break;
         }
      }
   return false;
   }

static int32_t isArrayWithStableElements(int32_t cpIndex, TR_ResolvedMethod *owningMethod, TR::Compilation *comp)
   {
   TR_J9VMBase *fej9 = comp->fej9();
   if (fej9->isStable(cpIndex, owningMethod, comp))
      {
      int32_t signatureLength = 0;
      char *signature = owningMethod->classSignatureOfFieldOrStatic(cpIndex, signatureLength);

      if (signature)
         {
         int32_t rank = 0;
         for (int32_t i = 0; i < signatureLength; i++)
            {
            if (signature[i] != '[') break;
            rank++;
            }

         if (comp->getOption(TR_TraceOptDetails) && rank > 0)
            traceMsg(comp, "Stable array with rank %d: %.*s\n", rank, signatureLength, signature);

         return rank;
         }
      }

   return 0;
   }


static bool verifyFieldAccess(void *curStruct, TR::SymbolReference *field, bool isStableArrayElement, TR::Compilation *comp)
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

      TR_OpaqueClassBlock *objectClass = fej9->getObjectClass((uintptr_t)curStruct);
      TR_OpaqueClassBlock *fieldClass = NULL;
      // Fabriated fields don't have valid cp index
      if (field->getCPIndex() < 0 &&
          field->getSymbol()->getRecognizedField() != TR::Symbol::UnknownField)
         {
         const char* className;
         int32_t length;
         className = field->getSymbol()->owningClassNameCharsForRecognizedField(length);
         fieldClass = fej9->getClassFromSignature(className, length, field->getOwningMethod(comp));
         }
      else
         fieldClass = field->getOwningMethod(comp)->getDeclaringClassFromFieldOrStatic(comp, field->getCPIndex());

      if (fieldClass == NULL)
         return false;

      TR_YesNoMaybe objectContainsField = fej9->isInstanceOf(objectClass, fieldClass, true);
      return objectContainsField == TR_yes;
      }
   else if (comp->getSymRefTab()->isImmutableArrayShadow(field) || isStableArrayElement)
      {
      TR_OpaqueClassBlock *arrayClass = fej9->getObjectClass((uintptr_t)curStruct);
      if (!fej9->isClassArray(arrayClass) ||
          (field->getSymbol()->isCollectedReference() &&
          fej9->isPrimitiveArray(arrayClass)) ||
          (!field->getSymbol()->isCollectedReference() &&
          fej9->isReferenceArray(arrayClass)))
         return false;

      if (isStableArrayElement && fej9->isPrimitiveArray(arrayClass))
         {
         if (TR::Compiler->om.getArrayElementWidthInBytes(comp, (uintptr_t)curStruct) != field->getSymbol()->getSize())
            return false;
         }


      return true;
      }
   else if (isFieldOfJavaObject(field, comp))
      {
      // For special shadows representing data in Java objects, we need to verify the Java object types.
      TR_OpaqueClassBlock *objectClass = fej9->getObjectClass((uintptr_t)curStruct);
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
static void *dereferenceStructPointerChain(void *baseStruct, TR::Node *baseNode, bool isBaseStableArray, TR::Node *curNode, TR::Compilation *comp)
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

      if (isBaseStableArray)
         TR_ASSERT_FATAL(addressChildNode == baseNode, "We should have only one level of indirection for stable arrays\n");

      // Use uintptr_t for pointer arithmetic operations and to save type conversions
      uintptr_t curStruct = 0;
      if (addressChildNode == baseNode)
         {
         // baseStruct is the value of baseNode, dereference is not needed
         curStruct = (uintptr_t)baseStruct;
         // baseStruct/baseNode are deemed verifiable by the caller         }
         }
      else
         {
         TR::SymbolReference *addressChildSymRef = addressChildNode->getSymbolReference();
         // Get the address of struct containing current field and dereference it
         void* addressChildAddress = dereferenceStructPointerChain(baseStruct, baseNode, false, addressChildNode, comp);

         if (addressChildAddress == NULL)
            {
            return NULL;
            }
         // Since we're going to dereference a field from addressChild, addressChild must be a java reference or a native struct
         else if (addressChildSymRef->getSymbol()->isCollectedReference())
            {
            curStruct = comp->fej9()->getReferenceFieldAtAddress((uintptr_t)addressChildAddress);
            }
         else // Native struct
            {
            // Because addressChildAddress is going to be dereferenced, the field must be a final field pointing at native struct
            TR_ASSERT(isFinalFieldPointingAtNativeStruct(addressChildSymRef, comp), "dereferenceStructPointerChain should be dealing with reference fields");
            curStruct = *(uintptr_t*)addressChildAddress;
            }
         }

      // Get the field address of curNode
      if (curStruct)
         {
         if (verifyFieldAccess((void*)curStruct, symRef, isBaseStableArray, comp))
            {
            uintptr_t fieldAddress = 0;
            // The offset of a java field is in its symRef
            if (isJavaField(symRef, comp))
               {
               if (TR::TransformUtil::avoidFoldingInstanceField(curStruct, symRef, comp))
                  {
                  if (comp->getOption(TR_TraceOptDetails))
                     {
                     traceMsg(
                        comp,
                        "avoid folding load of field #%d from object at %p\n",
                        symRef->getReferenceNumber(),
                        (void*)curStruct);
                     }
                  return NULL;
                  }

               fieldAddress = curStruct + symRef->getOffset();
               }
            else if (comp->getSymRefTab()->isImmutableArrayShadow(symRef) ||
                     isBaseStableArray)
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
               int64_t minOffset = 0;
               int64_t maxOffset = arrayLengthInBytes;
               if (!TR::Compiler->om.isOffHeapAllocationEnabled())
                  {
                  minOffset += TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
                  maxOffset += TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
                  }

               // Check array bound
               if (offset < minOffset ||
                   offset >= maxOffset)
                  {
                  traceMsg(comp, "Offset %d is out of bound [%d, %d] for %s on array shadow %p!\n", offset, minOffset, maxOffset, symRef->getName(comp->getDebug()), curNode);
                  return NULL;
                  }

               fieldAddress = TR::Compiler->om.getAddressOfElement(comp, curStruct, offset);

               if (!comp->getSymRefTab()->isImmutableArrayShadow(symRef))
                  {
                  // if stable array, check that we are loading from the beginning of an element that it's not Null
                  // size of the element was checked in verifyFieldAccess
                  TR::DataType type = symRef->getSymbol()->getDataType();
                  int32_t width = TR::Symbol::convertTypeToSize(type);
                  if (type == TR::Address)
                     width = TR::Compiler->om.sizeofReferenceField();

                  if ((fieldAddress % width) != 0 ||
                      isNullValueAtAddress(comp, type, fieldAddress, symRef->getSymbol()))
                     return NULL;
                  }
               }
            else
               {
               // Native struct
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

bool J9::TransformUtil::foldFinalFieldsIn(TR_OpaqueClassBlock *clazz, const char *className, int32_t classNameLength, bool isStatic, TR::Compilation *comp)
   {
   TR::SimpleRegex *classRegex = comp->getOptions()->getClassesWithFoldableFinalFields();
   if (classRegex)
      {
      // TR::SimpleRegex::match needs a NUL-terminated string
      size_t size = classNameLength + 1;
      char *name = (char*)comp->trMemory()->allocateMemory(size, stackAlloc);
      strncpy(name, className, classNameLength);
      name[size - 1] = '\0';
      return TR::SimpleRegex::match(classRegex, name);
      }
   else if (classNameLength >= 21 && !strncmp(className, "jdk/internal/reflect/", 21))
      return true;
   else if (classNameLength >= 17 && !strncmp(className, "java/lang/invoke/", 17))
      return true; // We can ONLY do this opt to fields that are never victimized by setAccessible
   else if (classNameLength >= 18 && !strncmp(className, "java/lang/reflect/", 18))
      return true;
   else if (classNameLength >= 18 && !strncmp(className, "java/lang/foreign/", 18))
      return true;
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
   else if (classNameLength >= 38 && !strncmp(className, "java/util/concurrent/ThreadLocalRandom", 38))
      return true;
   else if (classNameLength >= 18 && !strncmp(className, "java/nio/ByteOrder", 18))
      return true;
   else if (classNameLength >= 13 && !strncmp(className, "java/nio/Bits", 13))
      return true;
   else if (classNameLength >= 20 && !strncmp(className, "jdk/incubator/vector", 20))
      return true;
   else if (classNameLength >= 22 && !strncmp(className, "jdk/internal/vm/vector", 22))
      return true;
   else if (classNameLength >= 14 && !strncmp(className, "java/lang/Byte", 14))
      return true;
   else if (classNameLength >= 15 && !strncmp(className, "java/lang/Short", 15))
      return true;
   else if (classNameLength >= 17 && !strncmp(className, "java/lang/Integer", 17))
      return true;
   else if (classNameLength >= 14 && !strncmp(className, "java/lang/Long", 14))
      return true;
   else if (classNameLength >= 15 && !strncmp(className, "java/lang/Float", 15))
      return true;
   else if (classNameLength >= 16 && !strncmp(className, "java/lang/Double", 16))
      return true;
   else if (classNameLength >= 17 && !strncmp(className, "java/lang/Boolean", 17))
      return true;

   if (classNameLength == 16 && !strncmp(className, "java/lang/System", 16))
      return false;

   static char *enableJCLFolding = feGetEnv("TR_EnableJCLStaticFinalFieldFolding");
   if (enableJCLFolding
       && isStatic
       && comp->fej9()->isClassLibraryClass(clazz)
       && comp->fej9()->isClassInitialized(clazz))
      {
      return true;
      }

   static char *enableAggressiveFolding = feGetEnv("TR_EnableAggressiveStaticFinalFieldFolding");
   if (enableAggressiveFolding
      && isStatic
      && comp->fej9()->isClassInitialized(clazz))
      {
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

/** \brief
 *     Entry point for folding allowlist'd static final fields.
 *     These typically belong to fundamental system/bootstrap classes.
 *
 *  \param comp
 *     The compilation object.
 *
 *  \param node
 *     The node which is a load of a static final field.
 *
 *  \return
 *    True if the field is folded.
*/
bool
J9::TransformUtil::foldReliableStaticFinalField(TR::Compilation *comp, TR::Node *node)
   {
   TR_ASSERT(node->isLoadOfStaticFinalField(),
             "Expecting load of static final field on %s %p",
             node->getOpCode().getName(), node);

   if (!node->getOpCode().isLoadVarDirect())
      return false;

   if (J9::TransformUtil::canFoldStaticFinalField(comp, node) == TR_yes)
      {
      return J9::TransformUtil::foldStaticFinalFieldImpl(comp, node);
      }

   return false;
   }

bool
J9::TransformUtil::foldStaticFinalFieldAssumingProtection(TR::Compilation *comp, TR::Node *node)
   {
   TR_ASSERT(node->isLoadOfStaticFinalField(),
             "Expecting load of static final field on %s %p",
             node->getOpCode().getName(), node);

   if (!node->getOpCode().isLoadVarDirect())
      return false;

   if (J9::TransformUtil::canFoldStaticFinalField(comp, node) != TR_no)
      {
      return J9::TransformUtil::foldStaticFinalFieldImpl(comp, node);
      }

   return false;
   }

static bool
classHasFinalPutstaticOutsideClinit(TR::Compilation *comp, TR_OpaqueClassBlock *clazz)
   {
   TR_J9VMBase *fej9 = comp->fej9();
   J9VMThread *vmThread = fej9->vmThread();

   J9ROMClass *romClass = TR::Compiler->cls.romClassOf(clazz);
   TR::Region &stackRegion = comp->trMemory()->currentStackRegion();

   // Get all of the final fields declared by clazz from the VM.
   J9Class *j9c = TR::Compiler->cls.convertClassOffsetToClassPtr(clazz);
   uintptr_t numStaticFields =
      fej9->_vmFunctionTable->getStaticFields(vmThread, romClass, NULL);

   uintptr_t staticFieldsSize = numStaticFields * sizeof(J9ROMFieldShape*);
   J9ROMFieldShape **staticFields =
      (J9ROMFieldShape**)stackRegion.allocate(staticFieldsSize);

   fej9->_vmFunctionTable->getStaticFields(vmThread, romClass, staticFields);

   // Partition so that the final ones are at the beginning and the non-final
   // ones follow.
   uintptr_t numStaticFinalFields = 0;
   for (uintptr_t i = 0; i < numStaticFields; i++)
      {
      if ((staticFields[i]->modifiers & J9AccFinal) != 0)
         {
         // Field i is final. Swap to add it to the end of the final prefix.
         // Swapping is OK because we don't depend on the order of the fields.
         TR_ASSERT_FATAL(numStaticFinalFields < numStaticFields, "out of bounds");
         J9ROMFieldShape *tmp = staticFields[numStaticFinalFields];
         staticFields[numStaticFinalFields] = staticFields[i];
         staticFields[i] = tmp;
         numStaticFinalFields++;
         }
      }

   if (numStaticFinalFields == 0)
      return false;

   // Iterate the bytecode of all methods looking for a putstatic to a static
   // field of clazz with a name and signature matching one in the final prefix
   // of staticFields, i.e. staticFields[0..numStaticFinalFields].
   J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);

   TR_ScratchList<TR_ResolvedMethod> resolvedMethods(comp->trMemory());
   fej9->getResolvedMethods(comp->trMemory(), clazz, &resolvedMethods);

   ListIterator<TR_ResolvedMethod> mIt(&resolvedMethods);
   for (auto *method = mIt.getFirst(); method != NULL; method = mIt.getNext())
      {
      if (method->isNative() || method->isAbstract())
         continue; // method has no bytecode

      if (method->nameLength() == 8 && !strncmp(method->nameChars(), "<clinit>", 8))
         continue; // putstatic is always allowed in <clinit>

      auto *methodJ9 = (TR_ResolvedJ9Method*)method;
      TR_J9ByteCodeIterator bcIt(
         TR::ResolvedMethodSymbol::create(comp->trHeapMemory(), method, comp),
         methodJ9,
         fej9,
         comp);

      for (TR_J9ByteCode bc = bcIt.first(); bc != J9BCunknown; bc = bcIt.next())
         {
         if (bc != J9BCputstatic)
            continue;

         int32_t cpIndex = bcIt.next2Bytes();
         J9ROMConstantPoolItem *romCP = ((TR_ResolvedJ9Method*)method)->romLiterals();
         J9ROMFieldRef *fieldRef = (J9ROMFieldRef*)&romCP[cpIndex];
         J9ROMClassRef *classRef = (J9ROMClassRef*)&romCP[fieldRef->classRefCPIndex];
         J9UTF8 *classRefName = J9ROMCLASSREF_NAME(classRef);
         if (!J9UTF8_EQUALS(classRefName, className))
            continue;

         // putstatic to one of clazz's own static fields. Check whether it's
         // one of the final ones.
         J9ROMNameAndSignature *refNameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(fieldRef);
         J9UTF8 *refName = J9ROMNAMEANDSIGNATURE_NAME(refNameAndSig);
         J9UTF8 *refSig = J9ROMNAMEANDSIGNATURE_SIGNATURE(refNameAndSig);
         for (uintptr_t i = 0; i < numStaticFinalFields; i++)
            {
            J9UTF8 *finalFieldName = J9ROMFIELDSHAPE_NAME(staticFields[i]);
            J9UTF8 *finalFieldSig = J9ROMFIELDSHAPE_SIGNATURE(staticFields[i]);
            if (J9UTF8_EQUALS(refName, finalFieldName)
                && J9UTF8_EQUALS(refSig, finalFieldSig))
               {
               return true;
               }
            }
         }
      }

   return false;
   }

TR_YesNoMaybe
J9::TransformUtil::canFoldStaticFinalField(TR::Compilation *comp, TR::Node* node)
   {
   TR_ASSERT(node->getOpCode().isLoadVarDirect() && node->isLoadOfStaticFinalField(), "Expecting direct load of static final field on %s %p", node->getOpCode().getName(), node);
   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::Symbol           *sym    = node->getSymbol();

   if (symRef->isUnresolved()
       || !sym->isStaticField()
       || !sym->isFinal())
      return TR_no;

   TR_ResolvedMethod *owningMethod = symRef->getOwningMethod(comp);
   TR_OpaqueClassBlock* declaringClass = owningMethod->getClassFromFieldOrStatic(comp, symRef->getCPIndex(), true);
   TR::Symbol::RecognizedField recField = sym->getRecognizedField();
   return TR::TransformUtil::canFoldStaticFinalField(
      comp, declaringClass, recField, owningMethod, symRef->getCPIndex());
   }

TR_YesNoMaybe
J9::TransformUtil::canFoldStaticFinalField(
   TR::Compilation *comp,
   TR_OpaqueClassBlock *declaringClass,
   TR::Symbol::RecognizedField recField,
   TR_ResolvedMethod *owningMethod,
   int32_t cpIndex)
   {
   TR_J9VMBase *fej9 = comp->fej9();

   // Can't trust final statics unless class init is finished
   //
   if (!declaringClass
       || !fej9->isClassInitialized(declaringClass))
      return TR_no;

   // Rely on the CP entry to get the name and signature. There is always an
   // owning method and CP index here because we don't (yet) fabricate static
   // field symrefs.
   TR_ASSERT_FATAL(owningMethod != NULL, "missing owningMethod");
   TR_ASSERT_FATAL(cpIndex >= 0, "missing CP index");

   int32_t classNameLen;
   char *className = fej9->getClassNameChars(declaringClass, classNameLen);

   // Keep our hands off out/in/err, which are declared final but aren't really
   if (classNameLen == 16 && !strncmp(className, "java/lang/System", 16))
      return TR_no;

   int32_t fieldNameLen;
   const char *fieldName = owningMethod->staticNameChars(cpIndex, fieldNameLen);

   int32_t sigLen;
   const char *sig = owningMethod->staticSignatureChars(cpIndex, sigLen);

   // Reject any fields that are disabled by option.
   TR::SimpleRegex *dontFold = comp->getOptions()->getDontFoldStaticFinalFields();
   if (dontFold != NULL)
      {
      char buf[256];
      TR::Region &stackRegion = comp->trMemory()->currentStackRegion();
      TR::StringBuf fqNameAndSig(stackRegion, buf, sizeof(buf));

      // <class>.<field>:<sig>
      fqNameAndSig.appendf("%.*s", classNameLen, className);
      fqNameAndSig.appendf(".%.*s", fieldNameLen, fieldName);
      fqNameAndSig.appendf(":%.*s", sigLen, sig);

      if (dontFold->match(fqNameAndSig.text()))
         return TR_no;
      }

   // Static initializer can produce different values in different runs
   // so for AOT we cannot allow this transformation. However for String
   // we will embed some bits in the aotMethodHeader for this method.
   // The string compression enabled bit is set in staticFinalFieldValue().
   if (comp->compileRelocatableCode())
      {
      if (recField == TR::Symbol::Java_lang_String_enableCompression)
         return TR_yes;
      else
         return TR_no;
      }

   // In classes with version 53 and later, putstatic can write to static final
   // fields only during class initialization.
   //
   // We can also trust our own JCL classes not to use putstatic to modify
   // static final fields after initialization.
   //
   J9ROMClass *romClass = TR::Compiler->cls.romClassOf(declaringClass);
   bool putstaticIsToSpec =
      romClass->majorVersion >= 53 || fej9->isClassLibraryClass(declaringClass);

   // in Java 17 and above, it is not possible to remove the final attribute of a field
   // to make it writable via reflection.
#if JAVA_SPEC_VERSION >= 17
   if (putstaticIsToSpec)
      {
      // Both putstatic and reflection have been ruled out, so fields declared
      // in declaringClass must not be modified. We do not support modifying
      // them using Unsafe. Go for it.
      static const bool disableAggressiveSFFF =
         feGetEnv("TR_disableAggressiveSFFF17") != NULL;

      if (!disableAggressiveSFFF)
         return TR_yes;

      // General aggressive folding is disabled by environment variable. Still
      // try to apply it to static final fields with declared type VarHandle,
      // as before.
      static const bool disableAggressiveVarHandleSFFF =
         feGetEnv("TR_disableAggressiveVarHandleSFFF17") != NULL;

      if (!disableAggressiveVarHandleSFFF)
         {
         const char *vhSig = "Ljava/lang/invoke/VarHandle;";
         if (sigLen == (int32_t)strlen(vhSig) && !strncmp(sig, vhSig, sigLen))
            return TR_yes;
         }
      }
#endif

   // Fold $assertionsDisabled and static final fields declared by allowlisted
   // classes regardless of classHasIllegalStaticFinalFieldModification(). An
   // allowlisted class (e.g. String) can have "illegal" stores occur during
   // bootstrap that we want to ignore.
   if (!comp->getOption(TR_RestrictStaticFieldFolding) ||
       recField == TR::Symbol::assertionsDisabled ||
       J9::TransformUtil::foldFinalFieldsIn(
         declaringClass, className, classNameLen, true, comp))
      return TR_yes;

   // Determine whether this class contains putstatic instructions outside of
   // <clinit> that store to its static final fields. If it does, it can be
   // considered to have an illegal modification, preventing all future folding
   // of its static final fields.
   bool putstaticScanDone = false;
   if (putstaticIsToSpec)
      {
      // The problem can't occur. Such putstatic instructions would simply
      // fail, so no scan is needed.
      putstaticScanDone = true;
      }
   else
      {
      // Use the persistent class info to avoid repeated scanning.
      TR_PersistentCHTable *cht = comp->getPersistentInfo()->getPersistentCHTable();
      if (cht != NULL)
         {
         TR_PersistentClassInfo *classInfo =
            cht->findClassInfoAfterLocking(declaringClass, comp);

         if (classInfo != NULL)
            {
            putstaticScanDone = true;
            if (!classInfo->alreadyScannedForFinalPutstatic())
               {
               if (classHasFinalPutstaticOutsideClinit(comp, declaringClass))
                  {
                  // There are no assumptions to invalidate when setting this
                  // flag here. Any earlier attempts to fold static final fields
                  // in this class have failed to scan, and therefore they have
                  // refused to do even guarded folding.
                  //
                  // There could be a concurrent scan in another compilation
                  // thread, but if so, it will also find the bad putstatic and
                  // refuse to fold.
                  //
                  TR::Compiler->cls.setClassHasIllegalStaticFinalFieldModification(
                     declaringClass, comp);
                  }

               TR::ClassTableCriticalSection chtCS(comp->fe());
               classInfo->setAlreadyScannedForFinalPutstatic();
               }
            }
         }
      }

   // If this class needs a scan that we failed to perform, then don't fold at
   // all. Without the scan, we can't trust its static final fields. We could
   // still do guarded folding, but if we did, there would be a possibility of
   // successfully scanning later on and finding a bad putstatic. Then when
   // setting the illegal write flag, there would be assumptions to invalidate.
   if (!putstaticScanDone)
      return TR_no;

   // Reject fields belonging to classes in which illegal static final stores
   // have been observed, and classes with old versions that have been found to
   // contain a bad putstatic (even if no illegal store has occurred yet).
   if (TR::Compiler->cls.classHasIllegalStaticFinalFieldModification(declaringClass))
      return TR_no;

   // The putstatic scan has been done (or ruled out), so we know that if
   // declaringClass had a bad putstatic, it would have been flagged as having
   // an illegal write. But it wasn't flagged, so there is no bad putstatic,
   // and we can be very aggressive here.
   //
   // This applies in JDK 8 and 11, and it applies in JDK 17+ to static final
   // fields declared by classes with !putstaticIsToSpec. We should be able to
   // support references here in the future, at which point the path specific
   // to JDK 17+ will be redundant.
   //
   switch (sig[0])
      {
      case 'B':
      case 'Z':
      case 'S':
      case 'C':
      case 'I':
      case 'J':
      case 'F':
      case 'D':
         {
         static const bool disable =
            feGetEnv("TR_disableAggressivePrimitiveSFFF") != NULL;

         if (!disable)
            return TR_yes;

         break;
         }

      case 'L':
      case '[':
         {
         // Ideally we should trust these as well, but current known
         // object handling in the compiler is unable to deal with the
         // possibility of a later store to the field, e.g. via the
         // setAccessible modifiers hack. If we derive a known object at
         // a particular point and then later reach that point after the
         // field is modified, we get undefined behaviour, which is too
         // steep a consequence.
         break;
         }
      }

   // Fall back to guarded folding.
   return TR_maybe;
   }

static bool isTakenSideOfAVirtualGuard(TR::Compilation* comp, TR::Block* block)
   {
   // First block can never be taken side
   if (block == comp->getStartTree()->getEnclosingBlock())
      return false;

   for (auto edge = block->getPredecessors().begin(), end = block->getPredecessors().end(); edge != end; ++edge)
      {
      TR::Block *pred = toBlock((*edge)->getFrom());
      TR::Node* predLastRealNode = pred->getLastRealTreeTop()->getNode();

      if (predLastRealNode
          && predLastRealNode->isTheVirtualGuardForAGuardedInlinedCall()
          && predLastRealNode->getBranchDestination()->getEnclosingBlock() == block)
         return true;

      }

   return false;
   }

static bool skipFinalFieldFoldingInBlock(TR::Compilation* comp, TR::Block* block)
   {
   if (block->isCold()
       || block->isOSRCatchBlock()
       || block->isOSRCodeBlock()
       || isTakenSideOfAVirtualGuard(comp, block))
      return true;

   return false;
   }

// This is a place holder for when we may want to run HCR guard analysis.
// Currently the analysis is skipped due to concern of added computation/footprint cost at compile-time.
static TR_HCRGuardAnalysis* runHCRGuardAnalysisIfPossible()
   {
   return NULL;
   }

// Do not add fear point in a frame that doesn't support OSR
static bool cannotAttemptOSRDuring(TR::Compilation* comp, int32_t callerIndex)
   {
   TR::ResolvedMethodSymbol *method = callerIndex == -1 ?
      comp->getJittedMethodSymbol() : comp->getInlinedResolvedMethodSymbol(callerIndex);

   return method->cannotAttemptOSRDuring(callerIndex, comp, false);
   }

static TR_YesNoMaybe safeToAddFearPointAt(TR::Optimization* opt, TR::TreeTop* tt)
   {
   TR::Compilation* comp = opt->comp();
   if (opt->trace())
      {
      traceMsg(comp, "Checking if it is safe to add fear point at n%dn\n", tt->getNode()->getGlobalIndex());
      }

   int32_t callerIndex = tt->getNode()->getByteCodeInfo().getCallerIndex();
   if (!cannotAttemptOSRDuring(comp, callerIndex) && !comp->isOSRProhibitedOverRangeOfTrees())
      {
      if (opt->trace())
         {
         traceMsg(comp, "Safe to add fear point because there is no OSR prohibition\n");
         }
      return TR_yes;
      }

   TR_ASSERT_FATAL(
      !comp->isFearPointPlacementUnrestricted(),
      "unrestricted fear point placement should have prevented prohibition");

   // Look for an OSR point dominating tt in block
   TR::Block* block = tt->getEnclosingBlock();
   TR::TreeTop* firstTT = block->getEntry();
   while (tt != firstTT)
      {
      if (comp->isPotentialOSRPoint(tt->getNode()))
         {
         TR_YesNoMaybe result = comp->isPotentialOSRPointWithSupport(tt) ? TR_yes : TR_no;
         if (opt->trace())
            {
            traceMsg(comp, "Found %s potential OSR point n%dn, %s to add fear point\n",
                     result == TR_yes ? "supported" : "unsupported",
                     tt->getNode()->getGlobalIndex(),
                     result == TR_yes ? "Safe" : "Not safe");
            }

         return result;
         }
      tt = tt->getPrevTreeTop();
      }

   TR_HCRGuardAnalysis* guardAnalysis = runHCRGuardAnalysisIfPossible();
   if (guardAnalysis)
      {
      TR_YesNoMaybe result = guardAnalysis->_blockAnalysisInfo[block->getNumber()]->isEmpty() ? TR_yes : TR_no;
      if (opt->trace())
         {
         traceMsg(comp, "%s to add fear point based on HCRGuardAnalysis\n", result == TR_yes ? "Safe" : "Not safe");
         }

      return result;
      }

   if (opt->trace())
      {
      traceMsg(comp, "Cannot determine if it is safe to add fear point at n%dn\n", tt->getNode()->getGlobalIndex());
      }
   return TR_maybe;
   }

static bool isVarHandleFolding(TR::Compilation* comp, TR_OpaqueClassBlock* declaringClass, char* fieldSignature, int32_t fieldSigLength)
   {
   if (comp->getMethodSymbol()->hasMethodHandleInvokes())
      {
      if (fieldSigLength == 28 && !strncmp(fieldSignature, "Ljava/lang/invoke/VarHandle;", 28))
         return true;
      }

   return false;
   }

/** \brief
 *     Try to fold var handle static final field with protection
 *
 *  \param opt
 *     The current optimization object.
 *
 *  \param currentTree
 *     The tree with the load of static final field.
 *
 *  \param node
 *     The node which is a load of a static final field.
 */
bool J9::TransformUtil::attemptVarHandleStaticFinalFieldFolding(TR::Optimization* opt, TR::TreeTop * currentTree, TR::Node *node)
   {
   return J9::TransformUtil::attemptStaticFinalFieldFoldingImpl(opt, currentTree, node, true);
   }

/** \brief
 *     Try to fold generic static final field with protection
 *
 *  \param opt
 *     The current optimization object.
 *
 *  \param currentTree
 *     The tree with the load of static final field.
 *
 *  \param node
 *     The node which is a load of a static final field.
 */
bool J9::TransformUtil::attemptGenericStaticFinalFieldFolding(TR::Optimization* opt, TR::TreeTop * currentTree, TR::Node *node)
   {
   return J9::TransformUtil::attemptStaticFinalFieldFoldingImpl(opt, currentTree, node, false);
   }

bool J9::TransformUtil::canDoGuardedStaticFinalFieldFolding(
   TR::Compilation *comp)
   {
   return !comp->getOption(TR_DisableGuardedStaticFinalFieldFolding)
      && comp->canAddOSRAssumptions();
   }

/** \brief
 *     Try to fold static final field with protection
 *
 *  \param opt
 *     The current optimization object.
 *
 *  \param currentTree
 *     The tree with the load of static final field.
 *
 *  \param node
 *     The node which is a load of a static final field.
 *
 *  \param varHandleOnly
 *     True if only folding varHandle static final fields.
 *     Faslse if folding all static final fileds.
 */
bool J9::TransformUtil::attemptStaticFinalFieldFoldingImpl(TR::Optimization* opt, TR::TreeTop * currentTree, TR::Node *node, bool varHandleOnly)
   {
   TR::Compilation* comp = opt->comp();
   // first attempt folding reliable fields
   if (J9::TransformUtil::foldReliableStaticFinalField(comp, node))
      {
      if (opt->trace())
         traceMsg(comp, "SFFF fold reliable at node %p\n", node);
      return true;
      }

   // try folding regular static final fields
   TR::SymbolReference* symRef = node->getSymbolReference();
   if (symRef->hasKnownObjectIndex())
      {
      return false;
      }

   if (!TR::TransformUtil::canDoGuardedStaticFinalFieldFolding(comp))
      {
      return false;
      }

   int32_t cpIndex = symRef->getCPIndex();
   TR_OpaqueClassBlock* declaringClass = symRef->getOwningMethod(comp)->getClassFromFieldOrStatic(comp, cpIndex);
   if (J9::TransformUtil::canFoldStaticFinalField(comp, node) != TR_maybe
       || !declaringClass)
      {
      return false;
      }

   if (skipFinalFieldFoldingInBlock(comp, currentTree->getEnclosingBlock())
       || safeToAddFearPointAt(opt, currentTree) != TR_yes)
      {
      return false;
      }

   int32_t fieldNameLen;
   char* fieldName = symRef->getOwningMethod(comp)->fieldName(cpIndex, fieldNameLen, comp->trMemory(), stackAlloc);
   int32_t fieldSigLength;
   char* fieldSignature = symRef->getOwningMethod(comp)->staticSignatureChars(cpIndex, fieldSigLength);

   if (opt->trace())
      {
      traceMsg(comp,
              "Looking at static final field n%dn %.*s declared in class %p\n",
              node->getGlobalIndex(), fieldNameLen, fieldName, declaringClass);
      }
   if (!varHandleOnly || isVarHandleFolding(comp, declaringClass, fieldSignature, fieldSigLength))
      {
      if (J9::TransformUtil::foldStaticFinalFieldAssumingProtection(comp, node))
         {
         // Add class to assumption table
         comp->addClassForStaticFinalFieldModification(declaringClass);
         // Insert osrFearPointHelper call
         TR::TreeTop* helperTree = TR::TreeTop::create(comp, TR::Node::create(node, TR::treetop, 1, TR::Node::createOSRFearPointHelperCall(node)));
         currentTree->insertBefore(helperTree);

         if (opt->trace())
            {
            traceMsg(comp,
                    "Static final field n%dn is folded with OSRFearPointHelper call tree n%dn  helper tree n%dn\n",
                    node->getGlobalIndex(), currentTree->getNode()->getGlobalIndex(), helperTree->getNode()->getGlobalIndex());
            }

         TR::DebugCounter::prependDebugCounter(comp,
                                            TR::DebugCounter::debugCounterName(comp,
                                                                               "staticFinalFieldFolding/success/(field %.*s)/(%s %s)",
                                                                               fieldNameLen,
                                                                               fieldName,
                                                                               comp->signature(),
                                                                               comp->getHotnessName(comp->getMethodHotness())),
                                                                               currentTree->getNextTreeTop());
         return true;
         }
      }
   else
      {
      TR::DebugCounter::prependDebugCounter(comp,
                                            TR::DebugCounter::debugCounterName(comp,
                                                                               "staticFinalFieldFolding/notFolded/(field %.*s)/(%s %s)",
                                                                               fieldNameLen,
                                                                               fieldName,
                                                                               comp->signature(),
                                                                               comp->getHotnessName(comp->getMethodHotness())),
                                                                               currentTree->getNextTreeTop());
      }

   return false;
   }

/*
 * Load const node should have zero children
 */
static void prepareNodeToBeLoadConst(TR::Node *node)
   {
   for (int i=0; i < node->getNumChildren(); i++)
      node->getAndDecChild(i);
   node->setNumChildren(0);
   }

bool
J9::TransformUtil::foldStaticFinalFieldImpl(TR::Compilation *comp, TR::Node *node)
   {
   TR_ASSERT(node->getOpCode().isLoadVarDirect() && node->isLoadOfStaticFinalField(), "Expecting direct load of static final field on %s %p", node->getOpCode().getName(), node);
   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::Symbol           *sym    = node->getSymbol();

   if (symRef->isUnresolved()
       || symRef->hasKnownObjectIndex())
      return false;

   // Note that the load type can differ from the symbol type, eg. Java uses
   // integer loads for the sub-integer types.  The sub-integer types are
   // included below just for completeness, but we likely never hit them.
   //
   TR::DataType loadType = node->getDataType();
   bool typeIsConstible = false;
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
         break;
      default:
         return false;
      }

   TR_ResolvedMethod *owningMethod = symRef->getOwningMethod(comp);
   TR::StaticSymbol *staticSym = sym->castToStaticSymbol();
   int32_t cpIndex = symRef->getCPIndex();
   void *staticAddr = staticSym->getStaticAddress();
   TR::Symbol::RecognizedField recField = sym->getRecognizedField();
   TR::AnyConst value = TR::AnyConst::makeAddress(0);
   bool gotValue = TR::TransformUtil::staticFinalFieldValue(
      comp, owningMethod, cpIndex, staticAddr, loadType, recField, &value);

   if (!gotValue)
      return false;

   if (typeIsConstible)
      {
      if (performTransformation(comp, "O^O foldStaticFinalField: turn [%p] %s %s into load const\n", node, node->getOpCode().getName(), symRef->getName(comp->getDebug())))
         {
         prepareNodeToBeLoadConst(node);
         switch (loadType)
            {
            case TR::Int8:
               TR::Node::recreate(node, TR::bconst);
               node->setByte(value.getInt8());
               break;
            case TR::Int16:
               TR::Node::recreate(node, TR::sconst);
               node->setShortInt(value.getInt16());
               break;
            case TR::Int32:
               TR::Node::recreate(node, TR::iconst);
               node->setInt(value.getInt32());
               break;
            case TR::Int64:
               TR::Node::recreate(node, TR::lconst);
               node->setLongInt(value.getInt64());
               break;
            case TR::Float:
               TR::Node::recreate(node, TR::fconst);
               node->setFloat(value.getFloat());
               break;
            case TR::Double:
               TR::Node::recreate(node, TR::dconst);
               node->setDouble(value.getDouble());
               break;
            default:
               TR_ASSERT_FATAL(false, "Unexpected type %s", loadType.toString());
               break;
            }

         TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "foldFinalField.const/(%s)/%s/(%s)",
                                                            comp->signature(),
                                                            comp->getHotnessName(comp->getMethodHotness()),
                                                            symRef->getName(comp->getDebug())));
         return true;
         }
      }
   else if (value.isAddress() && value.getAddress() == 0)
      {
      if (performTransformation(comp, "O^O transformDirectLoad: [%p] field is null - change to aconst NULL\n", node))
         {
         prepareNodeToBeLoadConst(node);
         TR::Node::recreate(node, TR::aconst);
         node->setAddress(0);
         TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "foldFinalField.null/(%s)/%s/(%s)", comp->signature(), comp->getHotnessName(comp->getMethodHotness()), symRef->getName(comp->getDebug())));
         return true;
         }
      }
   else if (value.isKnownObject())
      {
      TR::KnownObjectTable::Index koi = value.getKnownObjectIndex();
      if (!performTransformation(comp, "O^O transformDirectLoad: mark n%un [%p] as obj%d\n",
            node->getGlobalIndex(),
            node,
            koi))
         return false;

      TR::SymbolReference *improvedSymRef =
         comp->getSymRefTab()->findOrCreateSymRefWithKnownObject(
            node->getSymbolReference(), koi);

      node->setSymbolReference(improvedSymRef);
      node->setIsNull(false);
      node->setIsNonNull(true);
      TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "foldFinalField.knownObject/(%s)/%s/(%s)", comp->signature(), comp->getHotnessName(comp->getMethodHotness()), symRef->getName(comp->getDebug())));
      return true;
      }

   return false; // Indicates we did nothing
   }

bool
J9::TransformUtil::staticFinalFieldValue(
   TR::Compilation *comp,
   TR_ResolvedMethod *owningMethod,
   int32_t cpIndex,
   void *staticAddr,
   TR::DataType loadType,
   TR::Symbol::RecognizedField recField,
   TR::AnyConst *outValue)
   {
   TR_J9VM *fej9 = (TR_J9VM *)comp->fej9();
   TR_StaticFinalData data = fej9->dereferenceStaticFinalAddress(staticAddr, loadType);

   if (comp->compileRelocatableCode())
      {
      TR_ASSERT_FATAL(
         recField == TR::Symbol::Java_lang_String_enableCompression,
         "folding unexpected static final in AOT");

      // Add the flags in TR_AOTMethodHeader
      TR_AOTMethodHeader *aotMethodHeaderEntry = comp->getAotMethodHeaderEntry();
      aotMethodHeaderEntry->flags |= TR_AOTMethodHeader_UsesEnableStringCompressionFolding;
      TR_ASSERT_FATAL(loadType == TR::Int32, "Java_lang_String_enableCompression must be Int32");
      bool fieldValue = data.dataInt32Bit != 0;
      bool compressionEnabled = comp->fej9()->isStringCompressionEnabledVM();
      TR_ASSERT_FATAL(
         fieldValue == compressionEnabled,
         "java/lang/String.enableCompression and javaVM->strCompEnabled must be in sync");
      if (fieldValue)
         aotMethodHeaderEntry->flags |= TR_AOTMethodHeader_StringCompressionEnabled;
      }

   switch (loadType)
      {
      case TR::Int8:
         *outValue = TR::AnyConst::makeInt8(data.dataInt8Bit);
         return true;

      case TR::Int16:
         *outValue = TR::AnyConst::makeInt16(data.dataInt16Bit);
         return true;

      case TR::Int32:
         *outValue = TR::AnyConst::makeInt32(data.dataInt32Bit);
         return true;

      case TR::Int64:
         *outValue = TR::AnyConst::makeInt64(data.dataInt64Bit);
         return true;

      case TR::Float:
         *outValue = TR::AnyConst::makeFloat(data.dataFloat);
         return true;

      case TR::Double:
         *outValue = TR::AnyConst::makeDouble(data.dataDouble);
         return true;

      case TR::Address:
         // address logic follows
         break;

      default:
         return false;
      }

   if (data.dataAddress == 0)
      {
      // J9VMInternals.jitHelpers is initialized after the class has been
      // initialized via an Unsafe helper - don't fold null in to improve perf
      if (recField == TR::Symbol::Java_lang_J9VMInternals_jitHelpers)
         return false;

      *outValue = TR::AnyConst::makeAddress(0);
      return true;
      }

   // The value is a known object.
   TR::KnownObjectTable *knot = comp->getOrCreateKnownObjectTable();
   if (knot == NULL)
      return false;

   uintptr_t *refLocation = (uintptr_t*)staticAddr;
   TR::KnownObjectTable::Index koi = knot->getOrCreateIndexAt(refLocation);
   if (koi == TR::KnownObjectTable::UNKNOWN)
      return false;

   // It's possible to get the null index if the field was mutated between
   // dereferenceStaticFinalAddress() and getOrCreateIndexAt(). This will be
   // exceedingly rare for any field that should be folded. In this case, just
   // give up to avoid setting *outValue to the null index.
   if (knot->isNull(koi))
      return false;

   if (cpIndex >= 0)
      {
      int32_t stableArrayRank = isArrayWithStableElements(cpIndex, owningMethod, comp);
      if (stableArrayRank > 0)
         knot->addStableArray(koi, stableArrayRank);
      }

   *outValue = TR::AnyConst::makeKnownObject(koi);
   return true;
   }

bool
J9::TransformUtil::transformDirectLoad(TR::Compilation *comp, TR::Node *node)
   {
   TR_ASSERT(node->getOpCode().isLoadVarDirect(), "Expecting direct load; found %s %p", node->getOpCode().getName(), node);

   if (node->isLoadOfStaticFinalField())
      {
      return J9::TransformUtil::foldReliableStaticFinalField(comp, node);
      }

   return false;
   }

/** Dereference node and fold it into a constant when possible.
 *
 *  The jit can tolerate certain mismatches between the tree and the data to be
 *  manipulated as long as *baseReferenceLocation/baseExpression is "verifiable".
 *  see comment in J9::TransformUtil::transformIndirectLoadChainImpl
 */
bool
J9::TransformUtil::transformIndirectLoadChainAt(TR::Compilation *comp, TR::Node *node, TR::Node *baseExpression, uintptr_t *baseReferenceLocation, TR::Node **removedNode)
   {
#if defined(J9VM_OPT_JITSERVER)
   // Bypass this method, because baseReferenceLocation is often an address of a pointer
   // on server's stack, which causes a segfault when getStaticReferenceFieldAtAddress is called
   // on the client.
   if (comp->isOutOfProcessCompilation())
      {
      return false;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */

   TR::VMAccessCriticalSection transformIndirectLoadChainAt(comp->fej9());
   uintptr_t baseAddress;
   if (baseExpression->getOpCode().hasSymbolReference() && baseExpression->getSymbol()->isStatic())
      {
      baseAddress = comp->fej9()->getStaticReferenceFieldAtAddress((uintptr_t)baseReferenceLocation);
      }
   else
      {
      baseAddress = *baseReferenceLocation;
      }
   bool result = TR::TransformUtil::transformIndirectLoadChainImpl(comp, node, baseExpression, (void*)baseAddress, 0, removedNode);
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
#if defined(J9VM_OPT_JITSERVER)
   // JITServer KOT: Bypass this method at the JITServer.
   // transformIndirectLoadChainImpl requires access to the VM.
   // It is already bypassed by transformIndirectLoadChainAt().
   if (comp->isOutOfProcessCompilation())
      {
      return false;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */

   TR::VMAccessCriticalSection transformIndirectLoadChain(comp->fej9());
   int32_t stableArrayRank = comp->getKnownObjectTable()->getArrayWithStableElementsRank(baseKnownObject);

   bool result = TR::TransformUtil::transformIndirectLoadChainImpl(comp, node, baseExpression, (void*)comp->getKnownObjectTable()->getPointer(baseKnownObject), stableArrayRank, removedNode);
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
 *  This function must verify that the structure we're loading from is
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
J9::TransformUtil::transformIndirectLoadChainImpl(TR::Compilation *comp, TR::Node *node, TR::Node *baseExpression, void *baseAddress, int32_t baseStableArrayRank, TR::Node **removedNode)
   {
   bool isBaseStableArray = baseStableArrayRank > 0;

   TR_J9VMBase *fej9 = comp->fej9();
#if defined(J9VM_OPT_JITSERVER)
   TR_ASSERT_FATAL(!comp->isOutOfProcessCompilation(), "It's not safe to call transformIndirectLoadChainImpl() in JITServer mode");
#endif
   TR_ASSERT(TR::Compiler->vm.hasAccess(comp), "transformIndirectLoadChain requires VM access");
   TR_ASSERT(node->getOpCode().isLoadIndirect(), "Expecting indirect load; found %s %p", node->getOpCode().getName(), node);
   TR_ASSERT(node->getNumChildren() == 1, "Expecting indirect load %s %p to have one child; actually has %d", node->getOpCode().getName(), node, node->getNumChildren());

   if (comp->compileRelocatableCode())
      {
      return false;
      }

   TR::SymbolReference *symRef = node->getSymbolReference();

   if (isBaseStableArray && !symRef->getSymbol()->isArrayShadowSymbol())
      return false;

   if (symRef->hasKnownObjectIndex())
      return false;

   // Fold initializeStatus field in J9Class whose finality is conditional on the value it is holding
   if (!symRef->isUnresolved() && symRef == comp->getSymRefTab()->findInitializeStatusFromClassSymbolRef())
      {
      J9Class* clazz = (J9Class*)baseAddress;
      traceMsg(comp, "Looking at node %p with initializeStatusFromClassSymbol, class %p initialize status is %d\n", node, clazz, clazz->initializeStatus);
      // Only fold the load if the class has been initialized
      if (fej9->isClassInitialized((TR_OpaqueClassBlock *) clazz) == J9ClassInitSucceeded)
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

   if (!isBaseStableArray && !fej9->canDereferenceAtCompileTime(symRef, comp))
      {
      if (comp->getOption(TR_TraceOptDetails))
         {
         traceMsg(comp, "Abort transformIndirectLoadChain - cannot dereference at compile time!\n");
         }
      return false;
      }

   // Dereference the chain starting from baseAddress and get the field address
   void *fieldAddress = dereferenceStructPointerChain(baseAddress, baseExpression, isBaseStableArray, node, comp);
   if (!fieldAddress)
      {
      if (comp->getOption(TR_TraceOptDetails))
         {
         traceMsg(comp, "Abort transformIndirectLoadChain - cannot verify/dereference field access to %s in %p!\n", symRef->getName(comp->getDebug()), baseAddress);
         }
      return false;
      }

   // The last step in the dereference chain is not necessarily an address.
   // Determine what it is and transform node appropriately.
   //
   if (isBaseStableArray && comp->getOption(TR_TraceOptDetails))
      traceMsg(comp, "Transforming a load from stable array %p\n", node);

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
            if (symRef->getReferenceNumber() - comp->getSymRefTab()->getNumHelperSymbols() == TR::SymbolReferenceTable::ramStaticsFromClassSymbol)
               {
               uintptr_t value = *(uintptr_t*)fieldAddress;
               if (changeIndirectLoadIntoConst(node, TR::aconst, removedNode, comp))
                  {
                  node->setAddress(value);
                  }
               }
            else
               {
               // Representable native structs are handled before now.  All
               // remaining natives are hopeless.
               return false;
               }
            }
         else if (symRef->getSymbol()->isCollectedReference())
            {
            uintptr_t value = fej9->getReferenceFieldAtAddress((uintptr_t)fieldAddress);
            if (value)
               {
               TR::SymbolReference *improvedSymRef = comp->getSymRefTab()->findOrCreateSymRefWithKnownObject(symRef, &value, isArrayWithConstantElements(symRef, comp));

               if (improvedSymRef->hasKnownObjectIndex()
                  && performTransformation(comp, "O^O transformIndirectLoadChain: %s [%p] with fieldOffset %d is obj%d referenceAddr is %p\n", node->getOpCode().getName(), node, improvedSymRef->getKnownObjectIndex(), symRef->getOffset(), value))
                  {
                  node->setSymbolReference(improvedSymRef);
                  node->setIsNull(false);
                  node->setIsNonNull(true);

                  int32_t stableArrayRank = isArrayWithStableElements(symRef->getCPIndex(),
                                                                      symRef->getOwningMethod(comp),
                                                                      comp);
                  if (isBaseStableArray)
                     stableArrayRank = baseStableArrayRank - 1;

                  if (stableArrayRank > 0)
                     {
                     TR::KnownObjectTable *knot = comp->getOrCreateKnownObjectTable();
                     knot->addStableArray(improvedSymRef->getKnownObjectIndex(), stableArrayRank);
                     }
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

#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
   if (TR::Compiler->om.isOffHeapAllocationEnabled())
      array = TR::TransformUtil::generateDataAddrLoadTrees(comp, array);
#endif /* J9VM_GC_SPARSE_HEAP_ALLOCATION */

   if (comp->target().is64Bit())
      addrCalc = TR::Node::create(TR::aladd, 2, array, offset);
   else
      addrCalc = TR::Node::create(TR::aiadd, 2, array, TR::Node::create(TR::l2i, 1, offset));

   addrCalc->setIsInternalPointer(true);
   return addrCalc;
}

TR::Node * J9::TransformUtil::calculateOffsetFromIndexInContiguousArray(TR::Compilation *comp, TR::Node * index, TR::DataType type)
   {
   int32_t width = TR::Symbol::convertTypeToSize(type);
   // In compressedrefs mode, each element of an reference array is a compressed pointer,
   // modify the width accordingly, so the stride is 4bytes instead of 8
   //
   if (comp->useCompressedPointers() && type == TR::Address)
      width = TR::Compiler->om.sizeofReferenceField();

   return calculateOffsetFromIndexInContiguousArrayWithElementStride(comp, index, width);
   }

TR::Node * J9::TransformUtil::calculateElementAddressWithElementStride(TR::Compilation *comp, TR::Node *array, TR::Node *index, int32_t elementStride)
{
   TR::Node * offset = TR::TransformUtil::calculateOffsetFromIndexInContiguousArrayWithElementStride(comp, index, elementStride);
   offset->setIsNonNegative(true);

   // Calculate element address
   TR::Node *addrCalc = NULL;
   if (comp->target().is64Bit())
      addrCalc = TR::Node::create(TR::aladd, 2, array, offset);
   else
      addrCalc = TR::Node::create(TR::aiadd, 2, array, TR::Node::create(TR::l2i, 1, offset));

   addrCalc->setIsInternalPointer(true);
   return addrCalc;
}

static int32_t checkNonNegativePowerOfTwo(int32_t value)
   {
   if (isNonNegativePowerOf2(value))
      {
      int32_t shiftAmount = 0;
      while ((value = ((uint32_t) value) >> 1))
         {
         ++shiftAmount;
         }
      return shiftAmount;
      }
   else
      {
      return -1;
      }
   }

TR::Node * J9::TransformUtil::calculateOffsetFromIndexInContiguousArrayWithElementStride(TR::Compilation *comp, TR::Node * index, int32_t elementStride)
   {
   int32_t shift = -1;
   if (elementStride > 0)
      {
      shift = checkNonNegativePowerOfTwo(elementStride);
      }

   int32_t headerSize;
#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
   if (TR::Compiler->om.isOffHeapAllocationEnabled())
      {
      headerSize = 0;
      }
   else
#endif /* J9VM_GC_SPARSE_HEAP_ALLOCATION */
      {
      headerSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
      }

   TR::Node * offset = index;

   // Return type must be Int64 and the index parameter is Int32 so we must do a conversion here
   if (comp->target().is64Bit())
      offset = TR::Node::create(TR::i2l, 1, index);

   TR::ILOpCodes constOp = comp->target().is64Bit() ? TR::lconst : TR::iconst;
   TR::ILOpCodes shlOp = comp->target().is64Bit() ? TR::lshl : TR::ishl;
   TR::ILOpCodes addOp = comp->target().is64Bit() ? TR::ladd : TR::iadd;
   TR::ILOpCodes mulOp = comp->target().is64Bit() ? TR::lmul : TR::imul;

   if (shift > 0)
      {
      TR::Node *shiftNode = TR::Node::create(TR::iconst, 0);
      shiftNode->setConstValue(shift);
      offset = TR::Node::create(shlOp, 2, offset, shiftNode);
      }
   else
      {
      TR::Node *elementStrideNode = TR::Node::create(constOp, 0);
      elementStrideNode->setConstValue(elementStride);
      offset = TR::Node::create(mulOp, 2, offset, elementStrideNode);
      }

   if (headerSize > 0)
      {
      TR::Node *headerSizeNode = TR::Node::create(constOp, 0);
      headerSizeNode->setConstValue(headerSize);
      offset = TR::Node::create(addOp, 2, offset, headerSizeNode);
      }

   // Return type must be Int64 but on 32-bit platforms we carry out the above arithmetic using Int32 so this conversion is necessary
   if (!comp->target().is64Bit())
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
   if (!comp->target().is64Bit())
      index = TR::Node::create(TR::l2i, 1, index);

   TR::ILOpCodes constOp = comp->target().is64Bit() ? TR::lconst : TR::iconst;
   TR::ILOpCodes shrOp = comp->target().is64Bit() ? TR::lshr : TR::ishr;
   TR::ILOpCodes subOp = comp->target().is64Bit() ? TR::lsub : TR::isub;

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
   if (comp->target().is64Bit())
      index = TR::Node::create(TR::l2i, 1, index);

   return index;
   }

/**
 * \brief
 *    Save a node to temp slot
 *
 * \parm comp
 *    The compilation object asking for the transformation
 *
 * \parm node
 *    The node to be saved
 *
 * \parm insertTreeTop
 *    The treetop containing the node
 *
 * \return
 *    A node that loads the saved node from the temp slot
 */
TR::Node*
J9::TransformUtil::saveNodeToTempSlot(TR::Compilation* comp, TR::Node* node, TR::TreeTop* insertTreeTop)
   {
   auto type = node->getDataType();
   auto symRef = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), type);
   insertTreeTop->insertBefore(TR::TreeTop::create(comp, TR::Node::createWithSymRef(comp->il.opCodeForDirectStore(type), 1, 1, node, symRef)));
   return TR::Node::createWithSymRef(node, comp->il.opCodeForDirectLoad(type), 0, symRef);
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

/** \brief
 *     Remove potentialOSRPointHelper calls from start to end inclusive.
 *
 *  \param comp
 *     The compilation object.
 *
 *  \param start
 *     The tree marking the start of the range.
 *
 *  \param end
 *     The tree marking the end of the range.
 */
void J9::TransformUtil::removePotentialOSRPointHelperCalls(TR::Compilation* comp, TR::TreeTop* start, TR::TreeTop* end)
   {
   TR_ASSERT(start->getEnclosingBlock() == end->getEnclosingBlock(), "Does not support range across blocks");
   TR_ASSERT(comp->supportsInduceOSR() && comp->isOSRTransitionTarget(TR::postExecutionOSR) && comp->getOSRMode() == TR::voluntaryOSR,
             "removePotentialOSRPointHelperCalls only works in certain modes");

   TR::TreeTop* ttAfterEnd = end->getNextTreeTop();
   TR::TreeTop *tt = start;

   do
      {
      TR::Node *osrNode = NULL;
      if (comp->isPotentialOSRPoint(tt->getNode(), &osrNode))
         {
         if (osrNode->isPotentialOSRPointHelperCall())
            {
            dumpOptDetails(comp, "Remove tt n%dn with potential osr point %p n%dn\n", tt->getNode()->getGlobalIndex(), osrNode, osrNode->getGlobalIndex());
            TR::TreeTop* prevTT = tt->getPrevTreeTop();
            TR::TransformUtil::removeTree(comp, tt);
            tt = prevTT;
            }
         }
      tt = tt->getNextTreeTop();
      }
   while (tt != ttAfterEnd);

   }

/** \brief
 *     Prohibit OSR over range between start and end inclusive.
 *
 *     Walk region and ensure all possible OSR transition points has doNotProfile set on their bci
 *     as they will not be able to reconstruct their stacks due to the optimizations done on them
 *     or on the surrounding trees.
 *
 *  \param comp
 *     The compilation object.
 *
 *  \param start
 *     The tree marking the start of the range.
 *
 *  \param end
 *     The tree marking the end of the range.
 */
void J9::TransformUtil::prohibitOSROverRange(TR::Compilation* comp, TR::TreeTop* start, TR::TreeTop* end)
   {
   TR_ASSERT_FATAL(
      !comp->isFearPointPlacementUnrestricted(),
      "disallowed attempt to prohibit OSR");

   TR_ASSERT(start->getEnclosingBlock() == end->getEnclosingBlock(), "Does not support range across blocks");
   TR_ASSERT(comp->supportsInduceOSR() && comp->isOSRTransitionTarget(TR::postExecutionOSR) && comp->getOSRMode() == TR::voluntaryOSR,
             "prohibitOSROverRange only works in certain modes");

   TR::TreeTop* ttAfterEnd = end->getNextTreeTop();
   TR::TreeTop *tt = start;

   do
      {
      TR::Node *osrNode = NULL;
      if (comp->isPotentialOSRPoint(tt->getNode(), &osrNode))
         {
         dumpOptDetails(comp, "Can no longer OSR at [%p] n%dn\n", osrNode, osrNode->getGlobalIndex());
         // Record the prohibition so other opts are aware of the existence of dangerous region
         comp->setOSRProhibitedOverRangeOfTrees();
         osrNode->getByteCodeInfo().setDoNotProfile(true);
         }
      tt = tt->getNextTreeTop();
      }
   while (tt != ttAfterEnd);

   }

void J9::TransformUtil::separateNullCheck(TR::Compilation* comp, TR::TreeTop* tree, bool trace)
   {
   TR::Node *nullCheck = tree->getNode();
   if (!nullCheck->getOpCode().isNullCheck())
      return;

   TR::Node *checkedRef = nullCheck->getNullCheckReference();
   if (trace)
      {
      traceMsg(comp,
         "separating null check on n%un from n%un\n",
         checkedRef->getGlobalIndex(),
         nullCheck->getGlobalIndex());
      }

   TR::Node * const passthrough = TR::Node::create(nullCheck, TR::PassThrough, 1, checkedRef);

   TR::SymbolReference * const nullCheckSR =
      comp->getSymRefTab()->findOrCreateNullCheckSymbolRef(comp->getMethodSymbol());
   TR::Node * const separatedNullCheck =
      TR::Node::createWithSymRef(nullCheck, TR::NULLCHK, 1, passthrough, nullCheckSR);

   tree->insertBefore(TR::TreeTop::create(comp, separatedNullCheck));

   switch (nullCheck->getOpCodeValue())
      {
      case TR::NULLCHK:
         nullCheck->setSymbolReference(NULL);
         TR::Node::recreate(nullCheck, TR::treetop);
         break;

      case TR::ResolveAndNULLCHK:
         nullCheck->setSymbolReference(
            comp->getSymRefTab()->findOrCreateResolveCheckSymbolRef(comp->getMethodSymbol()));
         TR::Node::recreate(nullCheck, TR::ResolveCHK);
         break;

      default:
         break;
      }
   }

TR::TreeTop *
J9::TransformUtil::generateReportFinalFieldModificationCallTree(TR::Compilation *comp, TR::Node *node)
   {
   TR::Node* j9class = TR::Node::createWithSymRef(node, TR::aloadi, 1, node, comp->getSymRefTab()->findOrCreateClassFromJavaLangClassSymbolRef());
   // If this is the first modification on static final field for this class, the notification is a stop-the-world event, gc may happen.
   TR::SymbolReference* symRef = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_reportFinalFieldModified, true /*canGCandReturn*/, false /*canGCandExcept*/, true);
   TR::Node *callNode = TR::Node::createWithSymRef(node, TR::call, 1, j9class, symRef);
   TR::TreeTop *tt = TR::TreeTop::create(comp, TR::Node::create(TR::treetop, 1, callNode));
   return tt;
   }

void
J9::TransformUtil::truncateBooleanForUnsafeGetPut(TR::Compilation *comp, TR::TreeTop* tree)
   {
   TR::Node* unsafeCall = tree->getNode()->getFirstChild();
   TR::RecognizedMethod rm = unsafeCall->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod();
   TR_ASSERT(TR_J9MethodBase::isUnsafeGetPutBoolean(rm), "Not unsafe get/put boolean method");

   if (TR_J9MethodBase::isUnsafePut(rm))
      {
      int32_t valueChildIndex = unsafeCall->getFirstArgumentIndex() + 3;
      TR::Node* value = unsafeCall->getChild(valueChildIndex);
      TR::Node* truncatedValue = TR::Node::create(unsafeCall, TR::icmpne, 2, value, TR::Node::iconst(unsafeCall, 0));
      unsafeCall->setAndIncChild(valueChildIndex, truncatedValue);
      value->recursivelyDecReferenceCount();
      dumpOptDetails(comp, "Truncate the boolean value of unsafe put %p n%dn, resulting in %p n%dn\n", unsafeCall, unsafeCall->getGlobalIndex(), truncatedValue, truncatedValue->getGlobalIndex());
      }
   else
      {
      // Insert a tree to truncate the result
      TR::Node* truncatedReturn = TR::Node::create(unsafeCall, TR::icmpne, 2, unsafeCall, TR::Node::iconst(unsafeCall, 0));
      tree->insertAfter(TR::TreeTop::create(comp, TR::Node::create(unsafeCall, TR::treetop, 1, truncatedReturn)));
      dumpOptDetails(comp, "Truncate the return of unsafe get %p n%dn, resulting in %p n%dn\n", unsafeCall, unsafeCall->getGlobalIndex(), truncatedReturn, truncatedReturn->getGlobalIndex());
      }
   }

bool
J9::TransformUtil::specializeInvokeExactSymbol(TR::Compilation *comp, TR::Node *callNode, uintptr_t *methodHandleLocation)
   {
   TR::SymbolReference      *symRef         = callNode->getSymbolReference();
   TR::ResolvedMethodSymbol *owningMethod   = callNode->getSymbolReference()->getOwningMethodSymbol(comp);
   TR_ResolvedMethod       *resolvedMethod = comp->fej9()->createMethodHandleArchetypeSpecimen(comp->trMemory(), methodHandleLocation, owningMethod->getResolvedMethod());
   if (resolvedMethod)
      {
      TR::SymbolReference      *specimenSymRef = comp->getSymRefTab()->findOrCreateMethodSymbol(owningMethod->getResolvedMethodIndex(), -1, resolvedMethod, TR::MethodSymbol::ComputedVirtual);
      if (performTransformation(comp, "Substituting more specific method symbol on %p: %s <- %s\n", callNode,
            specimenSymRef->getName(comp->getDebug()),
            callNode->getSymbolReference()->getName(comp->getDebug())))
         {
         callNode->setSymbolReference(specimenSymRef);
         return true;
         }
      }
      return false;
   }

bool
J9::TransformUtil::refineMethodHandleInvokeBasic(TR::Compilation* comp, TR::TreeTop* treetop, TR::Node* node, TR::KnownObjectTable::Index mhIndex, bool trace)
   {
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
   if (!comp->fej9()->isResolvedDirectDispatchGuaranteed(comp))
      {
      if (trace)
         {
         traceMsg(
            comp,
            "Cannot refine invokeBasic n%un %p without isResolvedDirectDispatchGuaranteed()\n",
            node->getGlobalIndex(),
            node);
         }
      return false;
      }

   auto knot = comp->getKnownObjectTable();
   if (mhIndex == TR::KnownObjectTable::UNKNOWN ||
       !knot ||
       knot->isNull(mhIndex))
      {
      if (trace)
         traceMsg(comp, "MethodHandle for invokeBasic n%dn %p is unknown or null\n", node->getGlobalIndex(), node);
      return false;
      }

   TR_J9VMBase* fej9 = comp->fej9();
   auto targetMethod = fej9->targetMethodFromMethodHandle(comp, mhIndex);

   TR_ASSERT(targetMethod, "Can't get target method from MethodHandle obj%d\n", mhIndex);

   auto symRef = node->getSymbolReference();
   // Refine the call
   auto refinedMethod = fej9->createResolvedMethod(comp->trMemory(), targetMethod, symRef->getOwningMethod(comp));
   if (!performTransformation(comp, "O^O Refine invokeBasic n%dn %p with known MH object\n", node->getGlobalIndex(), node))
      return false;

   // Preserve NULLCHK
   TR::TransformUtil::separateNullCheck(comp, treetop, trace);

   TR::SymbolReference *newSymRef =
       comp->getSymRefTab()->findOrCreateMethodSymbol
       (symRef->getOwningMethodIndex(), -1, refinedMethod, TR::MethodSymbol::Static);

   TR::Node::recreateWithSymRef(node, refinedMethod->directCallOpCode(), newSymRef);
   // doNotProfile flag is used to imply whether the bytecode can OSR under voluntary OSR. Above transformation
   // will set this flag to indicate the node cannot OSR. However, the transformation does not change the operand
   // stack and local state before the call, hence it can OSR.
   //
   node->getByteCodeInfo().setDoNotProfile(false);

   return true;
#else
   return false;
#endif
   }

TR::MethodSymbol::Kinds getTargetMethodCallKind(TR::RecognizedMethod rm)
   {
   TR::MethodSymbol::Kinds callKind;
   switch (rm)
      {
      case TR::java_lang_invoke_MethodHandle_invokeBasic:
      case TR::java_lang_invoke_MethodHandle_linkToStatic:
         callKind = TR::MethodSymbol::Static; break;
      case TR::java_lang_invoke_MethodHandle_linkToSpecial:
         callKind = TR::MethodSymbol::Special; break;
      case TR::java_lang_invoke_MethodHandle_linkToVirtual:
         callKind = TR::MethodSymbol::Virtual; break;
      case TR::java_lang_invoke_MethodHandle_linkToInterface:
         callKind = TR::MethodSymbol::Interface; break;
      default:
         TR_ASSERT_FATAL(0, "Unsupported method");
      }
   return callKind;
   }

// Use getIndirectCall(datatype), pass in return type
TR::ILOpCodes getTargetMethodCallOpCode(TR::RecognizedMethod rm, TR::DataType type)
   {
   switch (rm)
      {
      case TR::java_lang_invoke_MethodHandle_invokeBasic:
      case TR::java_lang_invoke_MethodHandle_linkToStatic:
      case TR::java_lang_invoke_MethodHandle_linkToSpecial:
         return TR::ILOpCode::getDirectCall(type);
      case TR::java_lang_invoke_MethodHandle_linkToVirtual:
      case TR::java_lang_invoke_MethodHandle_linkToInterface:
         return TR::ILOpCode::getIndirectCall(type);
      default:
         TR_ASSERT_FATAL(0, "Unsupported method");
      }
   return TR::BadILOp;
   }

bool
J9::TransformUtil::refineMethodHandleLinkTo(TR::Compilation* comp, TR::TreeTop* treetop, TR::Node* node, TR::KnownObjectTable::Index mnIndex, bool trace)
   {
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
   TR_J9VMBase* fej9 = comp->fej9();
   auto symRef = node->getSymbolReference();
   auto rm = node->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod();
   const char *missingResolvedDispatch = NULL;
   const char *whichLinkTo = NULL;
   switch (rm)
      {
      case TR::java_lang_invoke_MethodHandle_linkToStatic:
         whichLinkTo = "Static";
         // fall through
      case TR::java_lang_invoke_MethodHandle_linkToSpecial:
         if (rm == TR::java_lang_invoke_MethodHandle_linkToSpecial)
            whichLinkTo = "Special";
         if (!fej9->isResolvedDirectDispatchGuaranteed(comp))
            missingResolvedDispatch = "Direct";
         break;

      case TR::java_lang_invoke_MethodHandle_linkToVirtual:
         whichLinkTo = "Virtual";
         if (!fej9->isResolvedVirtualDispatchGuaranteed(comp))
            missingResolvedDispatch = "Virtual";
         break;

      default:
        TR_ASSERT_FATAL(false, "Unsupported method %s", symRef->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod()->signature(comp->trMemory()));
      }

   char nodeStr[64];
   TR::snprintfNoTrunc(
      nodeStr,
      sizeof(nodeStr),
      "linkTo%s n%un [%p]",
      whichLinkTo,
      node->getGlobalIndex(),
      node);

   if (missingResolvedDispatch != NULL)
      {
      if (trace)
         {
         traceMsg(
            comp,
            "Cannot refine %s without isResolved%sDispatchGuaranteed()\n",
            nodeStr,
            missingResolvedDispatch);
         }
      return false;
      }

   auto knot = comp->getKnownObjectTable();
   if (mnIndex == TR::KnownObjectTable::UNKNOWN ||
       !knot ||
       knot->isNull(mnIndex))
      {
      if (trace)
         traceMsg(comp, "%s: MemberName is unknown or null\n", nodeStr);

      return false;
      }

   TR_J9VMBase::MemberNameMethodInfo info = {};
   if (!fej9->getMemberNameMethodInfo(comp, mnIndex, &info) || info.vmtarget == NULL)
      {
      if (trace)
         traceMsg(comp, "%s: Failed to get MemberName method info\n", nodeStr);

      return false;
      }

   TR::MethodSymbol::Kinds callKind = getTargetMethodCallKind(rm);
   TR::ILOpCodes callOpCode = getTargetMethodCallOpCode(rm, node->getDataType());

   TR::SymbolReference* newSymRef = NULL;
   uint32_t vTableSlot = 0;
   int32_t jitVTableOffset = 0;
   if (rm == TR::java_lang_invoke_MethodHandle_linkToVirtual)
      {
      if (info.refKind != MH_REF_INVOKEVIRTUAL)
         {
         if (trace)
            traceMsg(comp, "%s: wrong MemberName kind %d\n", nodeStr, info.refKind);

         return false;
         }

      vTableSlot = (uint32_t)info.vmindex;
      jitVTableOffset = fej9->vTableSlotToVirtualCallOffset(vTableSlot);
      }

   if (!performTransformation(comp, "O^O Refine %s with known MemberName\n", nodeStr))
      return false;

   auto resolvedMethod = fej9->createResolvedMethodWithVTableSlot(comp->trMemory(), vTableSlot, info.vmtarget, symRef->getOwningMethod(comp));
   newSymRef = comp->getSymRefTab()->findOrCreateMethodSymbol(symRef->getOwningMethodIndex(), -1, resolvedMethod, callKind);
   if (callKind == TR::MethodSymbol::Virtual)
      newSymRef->setOffset(jitVTableOffset);

   bool needNullChk, needVftChild, needResolveChk;
   needNullChk = needVftChild = false;
   switch (rm)
      {
      case TR::java_lang_invoke_MethodHandle_linkToVirtual:
         if (callKind == TR::MethodSymbol::Virtual)
            needVftChild = true;
         // fall through
      case TR::java_lang_invoke_MethodHandle_linkToSpecial:
         needNullChk = true;
         break;
      default:
         break;
      }

  if (needNullChk)
      {
      TR::Node::recreateWithSymRef(treetop->getNode(), TR::NULLCHK, comp->getSymRefTab()->findOrCreateNullCheckSymbolRef(symRef->getOwningMethodSymbol(comp)));
      }

   if (needVftChild)
      {
      auto vftLoad = TR::Node::createWithSymRef(node, TR::aloadi, 1, node->getFirstArgument(), comp->getSymRefTab()->findOrCreateVftSymbolRef());
      // Save all arguments of linkTo* to an array
      int32_t numArgs = node->getNumArguments();
      TR::Node **args= new (comp->trStackMemory()) TR::Node*[numArgs];
      for (int32_t i = 0; i < numArgs; i++)
         args[i] = node->getArgument(i);

      node->removeLastChild();
      // Anchor all children to a treetop before transmuting the call node
      for (int i = 0; i <node->getNumChildren(); i++)
         {
         TR::TreeTop *tt = TR::TreeTop::create(comp, TR::Node::create(TR::treetop, 1, node->getChild(i)));
         treetop->insertBefore(tt);
         }

      node->removeAllChildren();
      // Recreate the node to a indirect call node
      TR::Node::recreateWithoutProperties(node, callOpCode, numArgs, vftLoad, newSymRef);
      // doNotProfile flag is used to imply whether the bytecode can OSR under voluntary OSR. Above transformation
      // will set this flag to indicate the node cannot OSR. However, the transformation does not change the operand
      // stack and local state before the call, hence it can OSR.
      //
      node->getByteCodeInfo().setDoNotProfile(false);

      for (int32_t i = 0; i < numArgs - 1; i++)
         node->setAndIncChild(i + 1, args[i]);
      }
   else
      {
      // VFT child is not needed, the call is direct, just need to change the symref and remove MemberName arg
      node->setSymbolReference(newSymRef);
      // Remove MemberName arg
      node->removeLastChild();
      }

   return true;
#else
   return false;
#endif
   }

#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
TR::TreeTop* J9::TransformUtil::convertUnsafeCopyMemoryCallToArrayCopyWithSymRefLoad(TR::Compilation *comp, TR::TreeTop *arrayCopyTT, TR::SymbolReference * srcRef, TR::SymbolReference * destRef)
   {
   // Convert call to arraycopy node
   TR::Node *arrayCopyNode = arrayCopyTT->getNode()->getFirstChild();
   arrayCopyNode->setNodeIsRecognizedArrayCopyCall(false);
   TR::Node::recreate(arrayCopyNode, TR::arraycopy);

   // Adjust src/dest
   TR::Node* adjustedSrc = srcRef ? TR::Node::createLoad(arrayCopyNode, srcRef) : arrayCopyNode->getChild(1);
   TR::Node* adjustedDest = destRef ? TR::Node::createLoad(arrayCopyNode, destRef) : arrayCopyNode->getChild(3);
   adjustedSrc = TR::Node::create(TR::aladd, 2, adjustedSrc, arrayCopyNode->getChild(2));
   adjustedDest = TR::Node::create(TR::aladd, 2, adjustedDest, arrayCopyNode->getChild(4));

   TR::Node* newArrayCopyNode = TR::Node::createArraycopy(adjustedSrc, adjustedDest,  arrayCopyNode->getChild(5));
   TR::TreeTop* newTT = TR::TreeTop::create(comp, newArrayCopyNode);
   arrayCopyTT->insertAfter(newTT);
   TR::TransformUtil::removeTree(comp, arrayCopyTT);

   return newTT;

   }

TR::Block* J9::TransformUtil::insertUnsafeCopyMemoryArgumentChecksAndAdjustForOffHeap(TR::Compilation *comp, TR::Node* node, TR::SymbolReference* symRef, TR::Block* callBlock, bool insertArrayCheck, TR::CFG* cfg)
   {
   /**
    *    Called if src/dest type is array (insertArrayCheck = false) or `java/lang/Object` (insertArrayCheck = true)
    *    Inserts the following blocks:
    *
    *    BBStart nullCheckBlock
    *    ifacmpeq --> newCallBlock
    *      aload  src/dest
    *      aconst NULL
    *    BBEnd
    *
    *    ===== if (insertArrayCheck == true) =====
    *    BBStart arrayCheckBlock
    *    ificmpeq --> newCallBlock           // jumps if not an array
    *      iand
    *        l2i
    *          lloadi  <isClassDepthAndFlags>
    *            aloadi  <vft-symbol>
    *              aload  src/dest
    *        iconst 0x10000                  // array flag
    *      iconst 0
    *    BBEnd
    *    =========================================
    *
    *    BBStart
    *    astore  temp symRef
    *      aloadi  <contiguousArrayDataAddrFieldSymbol>
    *        aload  src/dest
    *    BBEnd
    */

   TR::Block *nullCheckBlock = callBlock;
   TR::Block *newCallBlock = nullCheckBlock->split(nullCheckBlock->getEntry()->getNextTreeTop(), cfg);
   TR::Block *adjustBlock = nullCheckBlock->split(nullCheckBlock->getExit(), cfg);

   // Insert null check trees
   TR::Node* nullCheckNode = TR::Node::createif(TR::ifacmpeq, node->duplicateTree(), TR::Node::create(node, TR::aconst, 0, 0), newCallBlock->getEntry());
   nullCheckBlock->append(TR::TreeTop::create(comp, nullCheckNode));
   cfg->addEdge(nullCheckBlock, newCallBlock);

   if (insertArrayCheck)
      {
      TR::Block* arrayCheckBlock = callBlock->split(callBlock->getExit(), cfg);

      TR::Node *vftLoad = TR::Node::createWithSymRef(TR::aloadi, 1, 1, node->duplicateTree(), comp->getSymRefTab()->findOrCreateVftSymbolRef());
      TR::Node *maskedIsArrayClassNode = comp->fej9()->testIsClassArrayType(vftLoad);
      TR::Node *arrayCheckNode = TR::Node::createif(TR::ificmpeq, maskedIsArrayClassNode,
                                                    TR::Node::create(node, TR::iconst, 0),
                                                    newCallBlock->getEntry());

      arrayCheckBlock->append(TR::TreeTop::create(comp, arrayCheckNode, NULL, NULL));
      cfg->addEdge(callBlock, newCallBlock);
      }

   // Insert adjust trees
   TR::Node* adjustedNode = TR::TransformUtil::generateDataAddrLoadTrees(comp, node->duplicateTree());
   TR::Node *newStore = TR::Node::createStore(symRef, adjustedNode);
   TR::TreeTop *newStoreTree = TR::TreeTop::create(comp, newStore);
   adjustBlock->append(newStoreTree);

   return newCallBlock;
   }

void J9::TransformUtil::transformUnsafeCopyMemorytoArrayCopyForOffHeap(TR::Compilation *comp, TR::TreeTop *arrayCopyTT, TR::Node *arraycopyNode)
   {
   // When using balanced GC policy with offheap allocation enabled, there are three possible for an argument type:
   //
   // 1.) The type is known to be a non-array object at compile time. In this scenario, the final address
   //     can be calculated by simply adding ref and offset.
   // 2.) The type is known to be an array at compile time. In this scenario, if the ref at runtime is `null` then
   //     final address is calculated as in case 1. If not `null` then final address is the adjusted ref, by loading
   //     the dataAddr pointer field then add it to the offset.
   // 3.) The type of the object at dest is unknown at compile time (type is `java/lang/Object` or an interface).
   //     In this scenario, a runtime null check and array check must be generated to determine whether it needs to
   //     be handled such as case 1 or case 2.
   //     Interface is included as valid bytecode can store an array into an interface type and then gets passed to
   //     Unsafe.copyMemory().

   TR::Node *src        = arraycopyNode->getChild(1);
   TR::Node *dest       = arraycopyNode->getChild(3);

   // Check src/dest type at compile time
   int srcSigLen, destSigLen;
   const char *srcObjTypeSig = src->getSymbolReference() ? src->getSymbolReference()->getTypeSignature(srcSigLen) : 0;
   const char *destObjTypeSig = dest->getSymbolReference() ? dest->getSymbolReference()->getTypeSignature(destSigLen) : 0;

   // Case 3
   bool srcArrayCheckNeeded, destArrayCheckNeeded;
   if (!srcObjTypeSig)
      srcArrayCheckNeeded = true;
   else
      {
      TR_OpaqueClassBlock *srcClass = comp->fej9()->getClassFromSignature(srcObjTypeSig, srcSigLen, src->getSymbolReference()->getOwningMethod(comp));
      srcArrayCheckNeeded = srcClass == NULL ||
                              srcClass == comp->getObjectClassPointer() ||
                              TR::Compiler->cls.isInterfaceClass(comp, srcClass);
      }

   if (!destObjTypeSig)
      destArrayCheckNeeded = true;
   else
      {
      TR_OpaqueClassBlock *destClass = comp->fej9()->getClassFromSignature(destObjTypeSig, destSigLen, dest->getSymbolReference()->getOwningMethod(comp));
      destArrayCheckNeeded = destClass == NULL ||
                              destClass == comp->getObjectClassPointer() ||
                              TR::Compiler->cls.isInterfaceClass(comp, destClass);
      }

   // Case 2 & 3
   bool srcAdjustmentNeeded = srcArrayCheckNeeded || srcObjTypeSig[0] == '[';
   bool destAdjustmentNeeded = destArrayCheckNeeded || destObjTypeSig[0] == '[';

   TR::TransformUtil::separateNullCheck(comp, arrayCopyTT);

   if (!(srcAdjustmentNeeded || destAdjustmentNeeded))
      {
      TR::TransformUtil::convertUnsafeCopyMemoryCallToArrayCopyWithSymRefLoad(comp, arrayCopyTT, NULL, NULL);
      return;
      }

   // Anchor nodes
   for (int32_t i=1; i < arraycopyNode->getNumChildren(); i++)
      {
      TR::Node* childNode = arraycopyNode->getChild(i);
      if ( !(childNode->getOpCode().isLoadConst() ||
            (childNode->getOpCode().isLoadVarDirect() && childNode->getSymbolReference()->getSymbol()->isAutoOrParm())) )
         arrayCopyTT->insertBefore(TR::TreeTop::create(comp, TR::Node::create(TR::treetop, 1, childNode)));
      }

   TR::CFG *cfg = comp->getFlowGraph();
   TR::Block *currentBlock = arrayCopyTT->getEnclosingBlock();
   TR::Block *callBlock = currentBlock->split(arrayCopyTT, cfg, true);
   TR::Block *nextBlock = callBlock->split(arrayCopyTT->getNextTreeTop(), cfg, true);

   TR::SymbolReference *adjustSrcTempRef = NULL;
   TR::SymbolReference *adjustDestTempRef = NULL;
   if (srcAdjustmentNeeded)
      {
      adjustSrcTempRef = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), TR::Address);
      adjustSrcTempRef->getSymbol()->setNotCollected();
      TR::Node* storeNode = TR::Node::createStore(adjustSrcTempRef, src);
      TR::TreeTop* storeTree = TR::TreeTop::create(comp, storeNode);
      currentBlock->getExit()->insertBefore(storeTree);
      callBlock = TR::TransformUtil::insertUnsafeCopyMemoryArgumentChecksAndAdjustForOffHeap(comp, arraycopyNode->getChild(1), adjustSrcTempRef, callBlock, srcArrayCheckNeeded, cfg);
      }
   if (destAdjustmentNeeded)
      {
      adjustDestTempRef = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), TR::Address);
      adjustDestTempRef->getSymbol()->setNotCollected();
      TR::Node* storeNode = TR::Node::createStore(adjustDestTempRef, dest);
      TR::TreeTop* storeTree = TR::TreeTop::create(comp, storeNode);
      currentBlock->getExit()->insertBefore(storeTree);
      callBlock = TR::TransformUtil::insertUnsafeCopyMemoryArgumentChecksAndAdjustForOffHeap(comp, arraycopyNode->getChild(3), adjustDestTempRef, callBlock, destArrayCheckNeeded, cfg);
      }

   TR::TransformUtil::convertUnsafeCopyMemoryCallToArrayCopyWithSymRefLoad(comp, arrayCopyTT, adjustSrcTempRef, adjustDestTempRef);

   return;
   }
#endif /* J9VM_GC_SPARSE_HEAP_ALLOCATION */
