/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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
#if defined(J9VM_OPT_JITSERVER)
#include "control/CompilationRuntime.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */
#include "env/CompilerEnv.hpp"
#include "il/Block.hpp"
#include "il/Block_inlines.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "il/StaticSymbol.hpp"
#include "il/StaticSymbol_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
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
      switch (loadType)
         {
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
   else if (comp->getSymRefTab()->isImmutableArrayShadow(field))
      {
      TR_OpaqueClassBlock *arrayClass = fej9->getObjectClass((uintptr_t)curStruct);
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
         void* addressChildAddress = dereferenceStructPointerChain(baseStruct, baseNode, addressChildNode, comp);

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
         if (verifyFieldAccess((void*)curStruct, symRef, comp))
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

   if (classNameLength == 16 && !strncmp(className, "java/lang/System", 16))
      return false;

   static char *enableJCLFolding = feGetEnv("TR_EnableJCLStaticFinalFieldFolding");
   if ((enableJCLFolding || comp->getOption(TR_AggressiveOpts))
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

/** \brief
 *     Compile time query that answers if a direct load for static can be folded by compiler.
 *
 *  \param comp
 *     The compilation object.
 *
 *  \param node
 *     The node which is a load of a static final field.
 *
 *  \return
 *    True TR_yes if the field is allowlisted and can be folded.
 *    True TR_maybe if the field is a generic static final and can be folded with protection.
 *    True TR_no if the field is cannot be folded due to being unresolved, already been written post initialization, or owning class uninitialized, etc.
 */
TR_YesNoMaybe
J9::TransformUtil::canFoldStaticFinalField(TR::Compilation *comp, TR::Node* node)
   {
   TR_ASSERT(node->getOpCode().isLoadVarDirect() && node->isLoadOfStaticFinalField(), "Expecting direct load of static final field on %s %p", node->getOpCode().getName(), node);
   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::Symbol           *sym    = node->getSymbol();
   TR_J9VMBase *fej9 = comp->fej9();

   if (symRef->isUnresolved()
       || !sym->isStaticField()
       || !sym->isFinal())
      return TR_no;

   TR_OpaqueClassBlock* declaringClass = symRef->getOwningMethod(comp)->getClassFromFieldOrStatic(comp, symRef->getCPIndex(), true);

   // Can't trust final statics unless class init is finished
   //
   if (!declaringClass
       || !fej9->isClassInitialized(declaringClass))
      return TR_no;

   int32_t len;
   char * name = fej9->getClassNameChars(declaringClass, len);

   // Keep our hands off out/in/err, which are declared final but aren't really
   if (len == 16 && !strncmp(name, "java/lang/System", 16))
      return TR_no;

   if (!comp->getOption(TR_RestrictStaticFieldFolding) ||
       sym->getRecognizedField() == TR::Symbol::assertionsDisabled ||
       J9::TransformUtil::foldFinalFieldsIn(declaringClass, name, len, true, comp))
      return TR_yes;

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
   if (comp->getMethodSymbol()->hasMethodHandleInvokes()
       && !TR::Compiler->cls.classHasIllegalStaticFinalFieldModification(declaringClass))
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

   if (comp->getOption(TR_DisableGuardedStaticFinalFieldFolding))
      {
      return false;
      }

   if (!comp->supportsInduceOSR()
       || !comp->isOSRTransitionTarget(TR::postExecutionOSR)
       || comp->getOSRMode() != TR::voluntaryOSR)
      {
      return false;
      }

   int32_t cpIndex = symRef->getCPIndex();
   TR_OpaqueClassBlock* declaringClass = symRef->getOwningMethod(comp)->getClassFromFieldOrStatic(comp, cpIndex);
   if (J9::TransformUtil::canFoldStaticFinalField(comp, node) != TR_maybe
       || !declaringClass
       || TR::Compiler->cls.classHasIllegalStaticFinalFieldModification(declaringClass))
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

   // Static initializer can produce different values in different runs
   // so for AOT we cannot allow this transformation. However for String
   // we will embed some bits in the aotMethodHeader for this method
   if (comp->compileRelocatableCode())
      {
      if (sym->getRecognizedField() == TR::Symbol::Java_lang_String_enableCompression)
         {
         // Add the flags in TR_AOTMethodHeader
         TR_AOTMethodHeader *aotMethodHeaderEntry = comp->getAotMethodHeaderEntry();
         aotMethodHeaderEntry->flags |= TR_AOTMethodHeader_UsesEnableStringCompressionFolding;
         TR_ASSERT(node->getDataType() == TR::Int32, "Java_lang_String_enableCompression must be Int32");
         bool fieldValue = ((TR_J9VM *) comp->fej9())->dereferenceStaticFinalAddress(sym->castToStaticSymbol()->getStaticAddress(), TR::Int32).dataInt32Bit != 0;
         bool compressionEnabled = comp->fej9()->isStringCompressionEnabledVM();
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
   TR_StaticFinalData data = ((TR_J9VM *) comp->fej9())->dereferenceStaticFinalAddress(staticSym->getStaticAddress(), loadType);
   if (typeIsConstible)
      {
      if (performTransformation(comp, "O^O foldStaticFinalField: turn [%p] %s %s into load const\n", node, node->getOpCode().getName(), symRef->getName(comp->getDebug())))
         {
         TR::VMAccessCriticalSection isConsitble(comp->fej9());
         prepareNodeToBeLoadConst(node);
         switch (loadType)
            {
            case TR::Int8:
               TR::Node::recreate(node, TR::bconst);
               node->setByte(data.dataInt8Bit);
               break;
            case TR::Int16:
               TR::Node::recreate(node, TR::sconst);
               node->setShortInt(data.dataInt16Bit);
               break;
            case TR::Int32:
               TR::Node::recreate(node, TR::iconst);
               node->setInt(data.dataInt32Bit);
               break;
            case TR::Int64:
               TR::Node::recreate(node, TR::lconst);
               node->setLongInt(data.dataInt64Bit);
               break;
            case TR::Float:
               TR::Node::recreate(node, TR::fconst);
               node->setFloat(data.dataFloat);
               break;
            case TR::Double:
               TR::Node::recreate(node, TR::dconst);
               node->setDouble(data.dataDouble);
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
   else if (data.dataAddress == 0) // Seems ok just to check for a static to be NULL without vm access
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
               prepareNodeToBeLoadConst(node);
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
      uintptr_t *refLocation = (uintptr_t*)staticSym->getStaticAddress();
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

   return false; // Indicates we did nothing
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
   bool result = TR::TransformUtil::transformIndirectLoadChainImpl(comp, node, baseExpression, (void*)baseAddress, removedNode);
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

   if (!fej9->canDereferenceAtCompileTime(symRef, comp))
      {
      if (comp->getOption(TR_TraceOptDetails))
         {
         traceMsg(comp, "Abort transformIndirectLoadChain - cannot dereference at compile time!\n");
         }
      return false;
      }

   // Dereference the chain starting from baseAddress and get the field address
   void *fieldAddress = dereferenceStructPointerChain(baseAddress, baseExpression, node, comp);
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
   if (comp->target().is64Bit())
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
   if (comp->target().is64Bit())
      offset = TR::Node::create(TR::i2l, 1, index);

   TR::ILOpCodes constOp = comp->target().is64Bit() ? TR::lconst : TR::iconst;
   TR::ILOpCodes shlOp = comp->target().is64Bit() ? TR::lshl : TR::ishl;
   TR::ILOpCodes addOp = comp->target().is64Bit() ? TR::ladd : TR::iadd;

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
   auto symRef = node->getSymbolReference();
   auto rm = node->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod();
   switch (rm)
      {
      case TR::java_lang_invoke_MethodHandle_linkToStatic:
      case TR::java_lang_invoke_MethodHandle_linkToSpecial:
      case TR::java_lang_invoke_MethodHandle_linkToVirtual:
         break;
      default:
        TR_ASSERT_FATAL(false, "Unsupported method %s", symRef->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod()->signature(comp->trMemory()));
      }

   auto knot = comp->getKnownObjectTable();
   if (mnIndex == TR::KnownObjectTable::UNKNOWN ||
       !knot ||
       knot->isNull(mnIndex))
      {
      if (trace)
         traceMsg(comp, "MethodName for linkToXXX n%dn %p is unknown or null\n", node->getGlobalIndex(), node);
      return false;
      }

   TR_J9VMBase* fej9 = comp->fej9();
   auto targetMethod = fej9->targetMethodFromMemberName(comp, mnIndex);

   TR_ASSERT(targetMethod, "Can't get target method from MethodName obj%d\n", mnIndex);

   if (!performTransformation(comp, "O^O Refine linkToXXX n%dn [%p] with known MemberName object\n", node->getGlobalIndex(), node))
      return false;

   TR::MethodSymbol::Kinds callKind = getTargetMethodCallKind(rm);
   TR::ILOpCodes callOpCode = getTargetMethodCallOpCode(rm, node->getDataType());

   TR::SymbolReference* newSymRef = NULL;
   if (rm == TR::java_lang_invoke_MethodHandle_linkToVirtual)
      {
      uint32_t vTableSlot = fej9->vTableOrITableIndexFromMemberName(comp, mnIndex);
      auto resolvedMethod = fej9->createResolvedMethodWithVTableSlot(comp->trMemory(), vTableSlot, targetMethod, symRef->getOwningMethod(comp));
      newSymRef = comp->getSymRefTab()->findOrCreateMethodSymbol(symRef->getOwningMethodIndex(), -1, resolvedMethod, callKind);
      newSymRef->setOffset(fej9->vTableSlotToVirtualCallOffset(vTableSlot));
      }
   else
      {
      uint32_t vTableSlot = 0;
      auto resolvedMethod = fej9->createResolvedMethodWithVTableSlot(comp->trMemory(), vTableSlot, targetMethod, symRef->getOwningMethod(comp));
      newSymRef = comp->getSymRefTab()->findOrCreateMethodSymbol(symRef->getOwningMethodIndex(), -1, resolvedMethod, callKind);
      }

   bool needNullChk, needVftChild, needResolveChk;
   needNullChk = needVftChild = false;
   switch (rm)
      {
      case TR::java_lang_invoke_MethodHandle_linkToVirtual:
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

TR::KnownObjectTable::Index
J9::TransformUtil::knownObjectFromFinalStatic(
   TR::Compilation *comp,
   TR_ResolvedMethod *owningMethod,
   int32_t cpIndex,
   void *dataAddress)
   {
   if (comp->compileRelocatableCode())
      return TR::KnownObjectTable::UNKNOWN;

   TR::KnownObjectTable *knot = comp->getOrCreateKnownObjectTable();
   if (knot == NULL)
      return TR::KnownObjectTable::UNKNOWN;

#if defined(J9VM_OPT_JITSERVER)
   if (comp->isOutOfProcessCompilation())
      {
      TR_ResolvedJ9JITServerMethod *serverMethod = static_cast<TR_ResolvedJ9JITServerMethod*>(owningMethod);
      TR_ResolvedMethod *clientMethod = serverMethod->getRemoteMirror();

      auto stream = TR::CompilationInfo::getStream();
      stream->write(JITServer::MessageType::KnownObjectTable_symbolReferenceTableCreateKnownObject, dataAddress, clientMethod, cpIndex);

      auto recv = stream->read<TR::KnownObjectTable::Index, uintptr_t*>();
      TR::KnownObjectTable::Index knownObjectIndex = std::get<0>(recv);
      uintptr_t *objectPointerReference = std::get<1>(recv);

      if (knownObjectIndex != TR::KnownObjectTable::UNKNOWN)
         {
         knot->updateKnownObjectTableAtServer(knownObjectIndex, objectPointerReference);
         }

      return knownObjectIndex;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */

   TR::VMAccessCriticalSection getObjectReferenceLocation(comp);
   TR_J9VMBase *fej9 = comp->fej9();

   // Could return the null index when the reference is null, but current
   // callers aren't interested in it.
   if (*((uintptr_t*)dataAddress) == 0)
      return TR::KnownObjectTable::UNKNOWN;

   TR_OpaqueClassBlock *declaringClass = owningMethod->getDeclaringClassFromFieldOrStatic(comp, cpIndex);
   if (!declaringClass || !fej9->isClassInitialized(declaringClass))
      return TR::KnownObjectTable::UNKNOWN;

   static const char *foldVarHandle = feGetEnv("TR_FoldVarHandleWithoutFear");
   int32_t clazzNameLength = 0;
   char *clazzName = fej9->getClassNameChars(declaringClass, clazzNameLength);
   bool createKnownObject = false;

   if (J9::TransformUtil::foldFinalFieldsIn(declaringClass, clazzName, clazzNameLength, true, comp))
      {
      createKnownObject = true;
      }
   else if (foldVarHandle
            && (clazzNameLength != 16 || strncmp(clazzName, "java/lang/System", 16)))
      {
      TR_OpaqueClassBlock *varHandleClass =  fej9->getSystemClassFromClassName("java/lang/invoke/VarHandle", 26);
      TR_OpaqueClassBlock *objectClass = TR::Compiler->cls.objectClass(comp, *((uintptr_t*)dataAddress));

      if (varHandleClass != NULL
          && objectClass != NULL
          && fej9->isInstanceOf(objectClass, varHandleClass, true, true))
         {
         createKnownObject = true;
         }
      }

   if (!createKnownObject)
      return TR::KnownObjectTable::UNKNOWN;

   return knot->getOrCreateIndexAt((uintptr_t*)dataAddress);
   }
