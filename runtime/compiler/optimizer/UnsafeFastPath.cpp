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

#include "optimizer/UnsafeFastPath.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/VMJ9.h"
#include "env/j9method.h"
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
#include "infra/Assert.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/TransformUtil.hpp"
#include "infra/Checklist.hpp"


static TR::RecognizedMethod getVarHandleAccessMethodFromInlinedCallStack(TR::Compilation* comp, TR::Node* node)
   {
   int16_t callerIndex = node->getInlinedSiteIndex();
   while (callerIndex > -1)
      {
      TR_ResolvedMethod* caller = comp->getInlinedResolvedMethod(callerIndex);
      TR::RecognizedMethod callerRm = caller->getRecognizedMethod();
      if (TR_J9MethodBase::isVarHandleOperationMethod(callerRm))
         {
         return callerRm;
         }

      TR_InlinedCallSite callSite = comp->getInlinedCallSite(callerIndex);
      callerIndex = callSite._byteCodeInfo.getCallerIndex();
      }

   TR::RecognizedMethod rm = comp->getJittedMethodSymbol()->getRecognizedMethod();
   if (TR_J9MethodBase::isVarHandleOperationMethod(rm))
      return rm;

   return TR::unknownMethod;
   }

static TR::SymbolReferenceTable::CommonNonhelperSymbol equivalentAtomicIntrinsic(TR::RecognizedMethod rm)
   {
   switch (rm)
      {
      case TR::sun_misc_Unsafe_getAndSetInt:
           return TR::SymbolReferenceTable::atomicSwapSymbol;
      case TR::sun_misc_Unsafe_getAndSetLong:
           return TR::Compiler->target.is64Bit() ? TR::SymbolReferenceTable::atomicSwapSymbol : TR::SymbolReferenceTable::lastCommonNonhelperSymbol;
      case TR::sun_misc_Unsafe_getAndAddInt:
           return TR::SymbolReferenceTable::atomicFetchAndAddSymbol;
      case TR::sun_misc_Unsafe_getAndAddLong:
           return TR::Compiler->target.is64Bit() ? TR::SymbolReferenceTable::atomicFetchAndAddSymbol : TR::SymbolReferenceTable::lastCommonNonhelperSymbol;
      default:
         break;
      }
   return TR::SymbolReferenceTable::lastCommonNonhelperSymbol;
   }

static bool isTransformableUnsafeAtomic(TR::RecognizedMethod rm)
   {
   if (equivalentAtomicIntrinsic(rm) != TR::SymbolReferenceTable::lastCommonNonhelperSymbol)
      return true;

   return false;
   }

static bool isKnownUnsafeCaller(TR::RecognizedMethod rm)
   {
   switch (rm)
      {
      case TR::java_lang_invoke_ArrayVarHandle_ArrayVarHandleOperations_OpMethod:
      case TR::java_lang_invoke_StaticFieldVarHandle_StaticFieldVarHandleOperations_OpMethod:
      case TR::java_lang_invoke_InstanceFieldVarHandle_InstanceFieldVarHandleOperations_OpMethod:
      case TR::java_lang_invoke_ByteArrayViewVarHandle_ByteArrayViewVarHandleOperations_OpMethod:
      case TR::java_lang_invoke_StaticFieldGetterHandle_invokeExact:
      case TR::java_lang_invoke_StaticFieldSetterHandle_invokeExact:
      case TR::java_lang_invoke_FieldGetterHandle_invokeExact:
      case TR::java_lang_invoke_FieldSetterHandle_invokeExact:
         return true;
      default:
         return false;
      }
   return false;
   }

static bool isUnsafeCallerAccessingStaticField(TR::RecognizedMethod rm)
   {
   switch (rm)
      {
      case TR::java_lang_invoke_StaticFieldVarHandle_StaticFieldVarHandleOperations_OpMethod:
      case TR::java_lang_invoke_StaticFieldGetterHandle_invokeExact:
      case TR::java_lang_invoke_StaticFieldSetterHandle_invokeExact:
         return true;
      default:
         return false;
      }
   return false;
   }

static bool isUnsafeCallerAccessingArrayElement(TR::RecognizedMethod rm)
   {
   switch (rm)
      {
      case TR::java_lang_invoke_ArrayVarHandle_ArrayVarHandleOperations_OpMethod:
      case TR::java_lang_invoke_ByteArrayViewVarHandle_ByteArrayViewVarHandleOperations_OpMethod:
         return true;
      default:
         return false;
      }
   return false;
   }

static bool isVarHandleOperationMethodOnArray(TR::RecognizedMethod rm)
   {
   switch (rm)
      {
      case TR::java_lang_invoke_ArrayVarHandle_ArrayVarHandleOperations_OpMethod:
      case TR::java_lang_invoke_ByteArrayViewVarHandle_ByteArrayViewVarHandleOperations_OpMethod:
         return true;
      default:
         return false;
      }
   return false;
   }

static bool isVarHandleOperationMethodOnNonStaticField(TR::RecognizedMethod rm)
   {
   switch (rm)
      {
      case TR::java_lang_invoke_InstanceFieldVarHandle_InstanceFieldVarHandleOperations_OpMethod:
      case TR::java_lang_invoke_ArrayVarHandle_ArrayVarHandleOperations_OpMethod:
      case TR::java_lang_invoke_ByteArrayViewVarHandle_ByteArrayViewVarHandleOperations_OpMethod:
         return true;
      default:
         return false;
      }
   return false;
   }

/**
 * \brief
 *    Try transform unsafe atomic method called from VarHandle to codegen intrinsic
 *
 * \parm callTree
 *    Tree containing the call
 *
 * \parm callerMethod
 *    VarHandle concrete operation method
 *
 * \parm calleeMethod
 *    The unsafe method
 *
 * \return True if the call is transformed, otherwise false
 *
 */
bool TR_UnsafeFastPath::tryTransformUnsafeAtomicCallInVarHandleAccessMethod(TR::TreeTop* callTree, TR::RecognizedMethod callerMethod, TR::RecognizedMethod calleeMethod)
   {
   TR::Node* node = callTree->getNode()->getFirstChild();

   // Give up on arraylet
   //
   if (isVarHandleOperationMethodOnArray(callerMethod)
       && TR::Compiler->om.canGenerateArraylets())
      {
      if (trace())
         {
         traceMsg(comp(), "Call %p n%dn is accessing an element from an array that might be arraylet, quit\n", node, node->getGlobalIndex());
         }
      return false;
      }

    TR::MethodSymbol *symbol = node->getSymbol()->castToMethodSymbol();
   // Codegen will inline the call with the flag
   //
   if (symbol->getMethod()->isUnsafeCAS(comp()))
      {
      // codegen doesn't optimize CAS on a static field
      //
      if (isVarHandleOperationMethodOnNonStaticField(callerMethod) &&
         performTransformation(comp(), "%s transforming Unsafe.CAS [" POINTER_PRINTF_FORMAT "] into codegen inlineable\n", optDetailString(), node))
         {
         node->setIsSafeForCGToFastPathUnsafeCall(true);
         if (!isVarHandleOperationMethodOnArray(callerMethod))
            {
            node->setUnsafeGetPutCASCallOnNonArray();
            }

         if (trace())
            {
            traceMsg(comp(), "Found Unsafe CAS node %p n%dn on non-static field, set the flag\n", node, node->getGlobalIndex());
            }

         return true;
         }

      // TODO (#3532): Remove special handling for CAS when atomic intrinsic symbol is available for CAS
      return false;
      }

   TR::SymbolReferenceTable::CommonNonhelperSymbol helper = equivalentAtomicIntrinsic(calleeMethod);
   if (!comp()->cg()->supportsNonHelper(helper))
      {
      if (trace())
         {
         traceMsg(comp(), "Equivalent atomic intrinsic is not supported on current platform, quit\n");
         }
      return false;
      }

   if (!performTransformation(comp(), "%s turning the call [" POINTER_PRINTF_FORMAT "] into atomic intrinsic\n", optDetailString(), node))
      return false;

   TR::Node* unsafeAddress = NULL;
   if (callerMethod == TR::java_lang_invoke_StaticFieldVarHandle_StaticFieldVarHandleOperations_OpMethod)
      {
      TR::Node *jlClass = node->getChild(1);
      TR::Node *j9Class = TR::Node::createWithSymRef(node, TR::aloadi, 1, jlClass, comp()->getSymRefTab()->findOrCreateClassFromJavaLangClassSymbolRef());
      TR::Node *ramStatics = TR::Node::createWithSymRef(node, TR::aloadi, 1, j9Class, comp()->getSymRefTab()->findOrCreateRamStaticsFromClassSymbolRef());
      TR::Node *offset = node->getChild(2);
      // The offset for a static field is low taged, mask out the flag bit to get the real offset
      //
      offset = TR::Node::create(node, TR::land, 2, offset,
                                                   TR::Node::lconst(node, ~J9_SUN_FIELD_OFFSET_MASK));
      unsafeAddress = TR::Compiler->target.is32Bit() ? TR::Node::create(node, TR::aiadd, 2, ramStatics, TR::Node::create(node, TR::l2i, 1, offset)) :
                                                       TR::Node::create(node, TR::aladd, 2, ramStatics, offset);
      }
   else
      {
      TR::Node* object = node->getChild(1);
      TR::Node* offset = node->getChild(2);
      unsafeAddress = TR::Compiler->target.is32Bit() ? TR::Node::create(node, TR::aiadd, 2, object, TR::Node::create(node, TR::l2i, 1, offset)) :
                                                       TR::Node::create(node, TR::aladd, 2, object, offset);
      unsafeAddress->setIsInternalPointer(true);
      }


   if (callTree->getNode()->getOpCode().isNullCheck())
      {
      TR::Node* nullChkNode = callTree->getNode();
      TR::Node *passthrough = TR::Node::create(nullChkNode, TR::PassThrough, 1);
      passthrough->setAndIncChild(0, node->getFirstChild());
      TR::Node * checkNode = TR::Node::createWithSymRef(nullChkNode, TR::NULLCHK, 1, passthrough, nullChkNode->getSymbolReference());
      callTree->insertBefore(TR::TreeTop::create(comp(), checkNode));
      TR::Node::recreate(nullChkNode, TR::treetop);

      if (trace())
         {
         traceMsg(comp(), "Created node %p n%dn to preserve null check on call %p n%dn\n", checkNode, checkNode->getGlobalIndex(), node, node->getGlobalIndex());
         }
      }

   // Transform the symbol on the call to equivalent atomic method symbols
   TR::Node* unsafeObject = node->getChild(0);
   node->setAndIncChild(0, unsafeAddress);
   unsafeObject->recursivelyDecReferenceCount();
   node->removeChild(2); // Remove offset child
   node->removeChild(1); // Remove object child
   node->setSymbolReference(comp()->getSymRefTab()->findOrCreateCodeGenInlinedHelper(helper));

   if (trace())
      {
      traceMsg(comp(), "Transformed the call %p n%dn to codegen inlineable intrinsic\n", node, node->getGlobalIndex());
      }

   return true;
   }


static bool needUnsignedConversion(TR::RecognizedMethod methodToReduce)
   {
   switch (methodToReduce)
      {
      case TR::com_ibm_jit_JITHelpers_getCharFromArray:
      case TR::com_ibm_jit_JITHelpers_getCharFromArrayByIndex:
      case TR::com_ibm_jit_JITHelpers_getCharFromArrayVolatile:
      case TR::java_lang_StringUTF16_getChar:
         return true;
      }

   return false;
   }

/**
 * This replaces recognized unsafe calls to direct memory operations
 */
int32_t TR_UnsafeFastPath::perform()
   {
   if (comp()->getOption(TR_DisableUnsafe))
      return 0;

   TR::NodeChecklist transformed(comp());

   TR::ResolvedMethodSymbol *methodSymbol = comp()->getMethodSymbol();
   for (TR::TreeTop * tt = methodSymbol->getFirstTreeTop(); tt != NULL; tt = tt->getNextTreeTop())
      {
      TR::Node *ttNode = tt->getNode();
      TR::Node *node = ttNode->getNumChildren() > 0 ? ttNode->getFirstChild() : NULL; // Get the first child of the tree
      if (node && node->getOpCode().isCall() && !node->getSymbol()->castToMethodSymbol()->isHelper())
         {
         TR::SymbolReference *symRef = node->getSymbolReference();
         TR::MethodSymbol *symbol = node->getSymbol()->castToMethodSymbol();

         if (!transformed.contains(node))
            {
            TR::RecognizedMethod caller = getVarHandleAccessMethodFromInlinedCallStack(comp(), node);
            TR::RecognizedMethod callee = symbol->getRecognizedMethod();
            if (TR_J9MethodBase::isVarHandleOperationMethod(caller) &&
                (isTransformableUnsafeAtomic(callee) ||
                 symbol->getMethod()->isUnsafeCAS(comp())))
               {
               if (tryTransformUnsafeAtomicCallInVarHandleAccessMethod(tt, caller, callee))
                  {
                  transformed.add(node);
                  continue;
                  }
               }
            }

         // Unsafe for TR::java_math_BigDecimal_storeTwoCharsFromInt
         if (!symRef->isUnresolved() &&
         !comp()->generateArraylets() &&
         (symbol->getRecognizedMethod() == TR::java_math_BigDecimal_storeTwoCharsFromInt) &&
         performTransformation(comp(), "%s turning the call [" POINTER_PRINTF_FORMAT "] into an indirect store\n", optDetailString(), node))
            {
            anchorAllChildren(node, tt);
            TR::Node *value = node->getChild(3);
            TR::Node *index = node->getChild(2);
            TR::Node *charArray = node->getChild(1);
            index->setIsNonNegative(true);
            charArray->setIsNonNull(true);

            prepareToReplaceNode(node); // This will remove the usedef info, valueNumber info and all children of the node
            TR::Node *addrCalc;
            if (TR::Compiler->target.is64Bit())
               {
               addrCalc = TR::Node::create(TR::aladd, 2, charArray,
                     TR::Node::create(TR::ladd, 2, TR::Node::create(TR::lmul, 2, TR::Node::create(TR::i2l, 1, index), TR::Node::lconst(index, 4)),
                                       TR::Node::lconst(index, TR::Compiler->om.contiguousArrayHeaderSizeInBytes())));
               }
            else
               {
               addrCalc = TR::Node::create(TR::aiadd, 2, charArray,
                     TR::Node::create(TR::iadd, 2, TR::Node::create(TR::imul, 2, index, TR::Node::iconst(index, 4)),
                                       TR::Node::iconst(index, TR::Compiler->om.contiguousArrayHeaderSizeInBytes())));
               }

            TR::SymbolReference * unsafeSymRef = comp()->getSymRefTab()->findOrCreateUnsafeSymbolRef(TR::Int32, true, false);
            node = TR::Node::recreateWithoutProperties(node, TR::istorei, 2, addrCalc, value, unsafeSymRef);

            TR::TreeTop * newTree = tt->insertAfter(TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, node)));
            tt->getNode()->removeAllChildren();
            TR::TransformUtil::removeTree(comp(), tt);
            tt = newTree;

            if (trace())
               traceMsg(comp(), "Created node [" POINTER_PRINTF_FORMAT "] to store the value [" POINTER_PRINTF_FORMAT "] to target location [" POINTER_PRINTF_FORMAT "]\n", node, value, addrCalc);
            continue;
            }


         if (symbol->getRecognizedMethod() == TR::com_ibm_jit_JITHelpers_byteToCharUnsigned)
            {
            if (performTransformation(comp(), "%s Found unsafe/JITHelpers calls, turning node [" POINTER_PRINTF_FORMAT "] into a unsigned widening\n", optDetailString(), node))
               {
               TR::Node* value = node->getChild(1);

               anchorAllChildren(node, tt);

               prepareToReplaceNode(node);

               if (value->getDataType() != TR::Int8)
                  {
                  TR::ILOpCodes conversionOpCode = TR::ILOpCode::getProperConversion(value->getDataType(), TR::Int8, true);

                  // Sanity check for future modifications
                  TR_ASSERT(conversionOpCode != TR::BadILOp, "Could not get proper conversion for a JITHelpers byIndex operation.\n");

                  value = TR::Node::create(conversionOpCode, 1, value);
                  }

               node = TR::Node::recreateWithoutProperties(node, TR::bu2i, 1, value);

               TR::TreeTop* newTree = tt->insertAfter(TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, node)));
               tt->getNode()->removeAllChildren();
               TR::TransformUtil::removeTree(comp(), tt);
               tt = newTree;

               if (trace())
                  traceMsg(comp(), "Created node [" POINTER_PRINTF_FORMAT "] to unsigned widen value [" POINTER_PRINTF_FORMAT "] \n", node, value);

               continue;
               }
            }

         if (symbol->getRecognizedMethod() == TR::com_ibm_jit_JITHelpers_acmplt)
            {
            if (!performTransformation(comp(),
                  "%s Found unsafe/JITHelpers calls, turning node "
                  "[" POINTER_PRINTF_FORMAT "] "
                  "into an address less-than comparison\n",
                  optDetailString(),
                  node))
               {
               continue;
               }

            TR::Node *obj0 = node->getChild(1);
            TR::Node *obj1 = node->getChild(2);
            anchorAllChildren(node, tt);
            prepareToReplaceNode(node);
            node = TR::Node::recreateWithoutProperties(
               node, TR::acmplt, 2, obj0, obj1);

            TR::TreeTop *newTree = TR::TreeTop::create(
               comp(), TR::Node::create(TR::treetop, 1, node));

            tt->insertAfter(newTree);
            tt->getNode()->removeAllChildren();
            TR::TransformUtil::removeTree(comp(), tt);
            tt = newTree;

            if (trace())
               {
               traceMsg(comp(),
                  "Created node [" POINTER_PRINTF_FORMAT "] to compare "
                  "values [" POINTER_PRINTF_FORMAT "] "
                  "and [" POINTER_PRINTF_FORMAT "]\n",
                  node,
                  obj0,
                  obj1);
               }

            continue;
            }

         // Unsafes for other recognized methods
         TR::Node *offset = NULL, *index = NULL, *object = NULL, *value = NULL;
         TR::DataType type = TR::NoType;
         bool isVolatile = false;
         bool isArrayOperation = false;
         bool isByIndex = false;
         int32_t objectChild = 1;
         int32_t offsetChild = 2;

         switch (symbol->getRecognizedMethod())
            {
            case TR::java_lang_StringUTF16_getChar:
               objectChild = 0;
               offsetChild = 1;
               break;
            }

         // Check for array operation
         switch (symbol->getRecognizedMethod())
            {
            case TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V:
            case TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V:
            case TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject:
            case TR::sun_misc_Unsafe_getObject_jlObjectJ_jlObject:
               switch (methodSymbol->getRecognizedMethod())
                  {
               case TR::java_util_concurrent_ConcurrentHashMap_tabAt:
               case TR::java_util_concurrent_ConcurrentHashMap_setTabAt:
                  isArrayOperation = true;
               default:
                  break;
                  }
               break;
            case TR::com_ibm_jit_JITHelpers_getByteFromArrayVolatile:
            case TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex:
            case TR::com_ibm_jit_JITHelpers_getByteFromArray:
            case TR::com_ibm_jit_JITHelpers_getCharFromArrayVolatile:
            case TR::com_ibm_jit_JITHelpers_getCharFromArrayByIndex:
            case TR::com_ibm_jit_JITHelpers_getCharFromArray:
            case TR::com_ibm_jit_JITHelpers_getIntFromArrayVolatile:
            case TR::com_ibm_jit_JITHelpers_getIntFromArray:
            case TR::com_ibm_jit_JITHelpers_getLongFromArrayVolatile:
            case TR::com_ibm_jit_JITHelpers_getLongFromArray:
            case TR::com_ibm_jit_JITHelpers_getObjectFromArrayVolatile:
            case TR::com_ibm_jit_JITHelpers_getObjectFromArray:
            case TR::com_ibm_jit_JITHelpers_putByteInArrayVolatile:
            case TR::com_ibm_jit_JITHelpers_putByteInArrayByIndex:
            case TR::com_ibm_jit_JITHelpers_putByteInArray:
            case TR::com_ibm_jit_JITHelpers_putCharInArrayVolatile:
            case TR::com_ibm_jit_JITHelpers_putCharInArrayByIndex:
            case TR::com_ibm_jit_JITHelpers_putCharInArray:
            case TR::com_ibm_jit_JITHelpers_putIntInArrayVolatile:
            case TR::com_ibm_jit_JITHelpers_putIntInArray:
            case TR::com_ibm_jit_JITHelpers_putLongInArrayVolatile:
            case TR::com_ibm_jit_JITHelpers_putLongInArray:
            case TR::com_ibm_jit_JITHelpers_putObjectInArrayVolatile:
            case TR::com_ibm_jit_JITHelpers_putObjectInArray:
            case TR::java_lang_StringUTF16_getChar:
               isArrayOperation = true;
               break;
            default:
               break;
            }

         // Set other properties(data type, isVolatile and new value for its field or array element) of the object to be operated
         switch (symbol->getRecognizedMethod())
            {
            case TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V:
               isVolatile = true;
            case TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V:
               switch (comp()->getMethodSymbol()->getRecognizedMethod())
                  {
               case TR::java_util_concurrent_ConcurrentHashMap_setTabAt:
                  type = TR::Address;
                  value = node->getChild(3);
                  break;
               default:
                  break;
                  // by not setting type the call will not be recognized here and will
                  // be left to the inliner since we need to generate control flow
                  }
               break;
            case TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex:
               isByIndex = true;
               type = TR::Int8;
               break;
            case TR::com_ibm_jit_JITHelpers_getByteFromArrayVolatile:
               isVolatile = true;
            case TR::com_ibm_jit_JITHelpers_getByteFromArray:
               type = TR::Int8;
               break;
            case TR::java_lang_StringUTF16_getChar:
            case TR::com_ibm_jit_JITHelpers_getCharFromArrayByIndex:
               isByIndex = true;
               type = TR::Int16;
               break;
            case TR::com_ibm_jit_JITHelpers_getCharFromArrayVolatile:
               isVolatile = true;
            case TR::com_ibm_jit_JITHelpers_getCharFromArray:
               type = TR::Int16;
               break;
            case TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject:
               isVolatile = true;
            case TR::sun_misc_Unsafe_getObject_jlObjectJ_jlObject:
               switch (methodSymbol->getRecognizedMethod())
                  {
               case TR::java_util_concurrent_ConcurrentHashMap_tabAt:
                  type = TR::Address;
                  break;
               default:
                  break;
                  // by not setting type the call will not be recognized here and will
                  // be left to the inliner since we need to generate control flow
                  }
               break;
            case TR::sun_misc_Unsafe_putInt_jlObjectJI_V:
            case TR::sun_misc_Unsafe_putLong_jlObjectJJ_V:
               if (!strncmp(methodSymbol->getResolvedMethod()->classNameChars(), "java/util/concurrent/ThreadLocalRandom", 38))
                  value = node->getChild(3);
            case TR::sun_misc_Unsafe_getInt_jlObjectJ_I:
            case TR::sun_misc_Unsafe_getLong_jlObjectJ_J:
               if (!strncmp(methodSymbol->getResolvedMethod()->classNameChars(), "java/util/concurrent/ThreadLocalRandom", 38))
                   {
                   if ((symbol->getRecognizedMethod() == TR::sun_misc_Unsafe_getLong_jlObjectJ_J) ||
                       (symbol->getRecognizedMethod() == TR::sun_misc_Unsafe_putLong_jlObjectJJ_V))
                      type = TR::Int64;
                   else
                      type = TR::Int32;
                   }
               break;
            case TR::com_ibm_jit_JITHelpers_getIntFromArrayVolatile:
            case TR::com_ibm_jit_JITHelpers_getIntFromObjectVolatile:
               isVolatile = true;
            case TR::com_ibm_jit_JITHelpers_getIntFromArray:
            case TR::com_ibm_jit_JITHelpers_getIntFromObject:
               type = TR::Int32;
               break;
            case TR::com_ibm_jit_JITHelpers_getLongFromArrayVolatile:
            case TR::com_ibm_jit_JITHelpers_getLongFromObjectVolatile:
               if (TR::Compiler->target.is32Bit() && !comp()->cg()->getSupportsInlinedAtomicLongVolatiles())
                  break; // if the platform cg does not support volatile longs just generate the call
               isVolatile = true;
            case TR::com_ibm_jit_JITHelpers_getLongFromArray:
            case TR::com_ibm_jit_JITHelpers_getLongFromObject:
               type = TR::Int64;
               break;
            case TR::com_ibm_jit_JITHelpers_getObjectFromArrayVolatile:
            case TR::com_ibm_jit_JITHelpers_getObjectFromObjectVolatile:
               isVolatile = true;
            case TR::com_ibm_jit_JITHelpers_getObjectFromArray:
            case TR::com_ibm_jit_JITHelpers_getObjectFromObject:
               type = TR::Address;
               break;
            case TR::com_ibm_jit_JITHelpers_putByteInArrayByIndex:
               isByIndex = true;
               value = node->getChild(3);
               type = TR::Int8;
               break;
            case TR::com_ibm_jit_JITHelpers_putByteInArrayVolatile:
               isVolatile = true;
            case TR::com_ibm_jit_JITHelpers_putByteInArray:
               value = node->getChild(3);
               type = TR::Int8;
               break;
            case TR::com_ibm_jit_JITHelpers_putCharInArrayByIndex:
               isByIndex = true;
               value = node->getChild(3);
               type = TR::Int16;
               break;
            case TR::com_ibm_jit_JITHelpers_putCharInArrayVolatile:
               isVolatile = true;
            case TR::com_ibm_jit_JITHelpers_putCharInArray:
               value = node->getChild(3);
               type = TR::Int16;
               break;
            case TR::com_ibm_jit_JITHelpers_putIntInArrayVolatile:
            case TR::com_ibm_jit_JITHelpers_putIntInObjectVolatile:
               isVolatile = true;
            case TR::com_ibm_jit_JITHelpers_putIntInArray:
            case TR::com_ibm_jit_JITHelpers_putIntInObject:
               value = node->getChild(3);
               type = TR::Int32;
               break;
            case TR::com_ibm_jit_JITHelpers_putLongInArrayVolatile:
            case TR::com_ibm_jit_JITHelpers_putLongInObjectVolatile:
               if (TR::Compiler->target.is32Bit() && !comp()->cg()->getSupportsInlinedAtomicLongVolatiles())
                  break; // if the platform cg does not support volatile longs just generate the call
               isVolatile = true;
            case TR::com_ibm_jit_JITHelpers_putLongInArray:
            case TR::com_ibm_jit_JITHelpers_putLongInObject:
               value = node->getChild(3);
               type = TR::Int64;
               break;
            case TR::com_ibm_jit_JITHelpers_putObjectInArrayVolatile:
            case TR::com_ibm_jit_JITHelpers_putObjectInObjectVolatile:
               isVolatile = true;
            case TR::com_ibm_jit_JITHelpers_putObjectInArray:
            case TR::com_ibm_jit_JITHelpers_putObjectInObject:
               value = node->getChild(3);
               type = TR::Address;
               break;
            default:
               break;
            }

         // Handle VarHandle operation methods
         bool isStatic = false;
         TR::RecognizedMethod callerMethod = methodSymbol->getRecognizedMethod();
         TR::RecognizedMethod calleeMethod = symbol->getRecognizedMethod();
         if (isKnownUnsafeCaller(callerMethod) &&
             TR_J9MethodBase::isUnsafeGetPutWithObjectArg(calleeMethod))
            {
            if (isUnsafeCallerAccessingStaticField(callerMethod))
                isStatic = true;
            if (isUnsafeCallerAccessingArrayElement(callerMethod))
                isArrayOperation = true;

            if (isArrayOperation)
               type = TR_J9MethodBase::unsafeDataTypeForArray(calleeMethod);
            else
               type = TR_J9MethodBase::unsafeDataTypeForObject(calleeMethod);


            if (TR_J9MethodBase::isUnsafePut(calleeMethod))
               value = node->getChild(3);

            if (TR_J9MethodBase::isVolatileUnsafe(calleeMethod))
               isVolatile = true;

            if (trace())
               traceMsg(comp(), "VarHandle operation: isArrayOperation %d type %s value %p isVolatile %d on node %p\n", isArrayOperation, J9::DataType::getName(type), value, isVolatile, node);
            }

         bool mightBeArraylets = isArrayOperation && TR::Compiler->om.canGenerateArraylets();

         // Skip inlining of helpers for arraylets if unsafe for arraylets is disabled
         static char * disableUnsafeForArraylets = feGetEnv("TR_DisableUnsafeForArraylets");

         if (mightBeArraylets && disableUnsafeForArraylets)
            {
            if (trace())
               traceMsg(comp(), "unsafeForArraylets is disabled, skip unsafeFastPath for node [" POINTER_PRINTF_FORMAT "]\n", node);
            continue;
            }

         if (type != TR::NoType && performTransformation(comp(), "%s Found unsafe/JITHelpers calls, turning node [" POINTER_PRINTF_FORMAT "] into a load/store\n", optDetailString(), node))
            {
            TR::SymbolReference * unsafeSymRef = comp()->getSymRefTab()->findOrCreateUnsafeSymbolRef(type, true, isStatic, isVolatile);

            // some helpers are special - we know they are accessing an array and we know the kind of that array
            // so use the more helpful symref if we can
            switch (calleeMethod)
               {
               case TR::java_lang_StringUTF16_getChar:
                  unsafeSymRef = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::Int8);
                  break;
               }

            // Change the object child to the starting address of static fields in J9Class
            if (isStatic)
               {
               TR::Node *jlClass = node->getChild(objectChild);
               TR::Node *j9Class =
                  TR::Node::createWithSymRef(TR::aloadi, 1, 1, jlClass,
                                  comp()->getSymRefTab()->findOrCreateClassFromJavaLangClassSymbolRef());
               TR::Node *ramStatics =
                  TR::Node::createWithSymRef(TR::aloadi, 1, 1, j9Class,
                                  comp()->getSymRefTab()->findOrCreateRamStaticsFromClassSymbolRef());
               node->setAndIncChild(objectChild, ramStatics);
               jlClass->recursivelyDecReferenceCount();
               offset = node->getChild(offsetChild);
               // The offset for a static field is low taged, mask out the last bit to get the real offset
               TR::Node *newOffset =
                  TR::Node::create(offset, TR::land, 2, offset,
                                  TR::Node::lconst(offset, ~1));
               node->setAndIncChild(offsetChild, newOffset);
               offset->recursivelyDecReferenceCount();
               }

            // Anchor children of the call node to preserve their values
            anchorAllChildren(node, tt);

            // When accessing a static field, object is the starting address of static fields
            object = node->getChild(objectChild);
            if (!isStatic)
               object->setIsNonNull(true);

            if (isByIndex)
               {
               index = node->getChild(offsetChild);
               index->setIsNonNegative(true);

               offset = J9::TransformUtil::calculateOffsetFromIndexInContiguousArray(comp(), index, type);
               }
            else
               {
               offset = node->getChild(offsetChild);
               offset->setIsNonNegative(true);

               // Index is not used in the non-arraylet case so no need to compute it
               if (mightBeArraylets)
                  index = J9::TransformUtil::calculateIndexFromOffsetInContiguousArray(comp(), offset, type);
               }

            prepareToReplaceNode(node);

            if (mightBeArraylets)
               {
               if (trace())
                  traceMsg(comp(), "This is an array operation in arraylets mode with array [" POINTER_PRINTF_FORMAT "] and offset [ " POINTER_PRINTF_FORMAT "], creating a load/store and a spineCHK\n", object, offset);

               TR::Node *addrCalc = NULL;

               // Calculate element address
               if (TR::Compiler->target.is64Bit())
                  addrCalc = TR::Node::create(TR::aladd, 2, object, offset);
               else
                  addrCalc = TR::Node::create(TR::aiadd, 2, object, TR::Node::create(TR::l2i, 1, offset));

               addrCalc->setIsInternalPointer(true);

               // Generate a spine check, need a symref
               // We don't need a BNDCHK because these are unsafe calls from internal code
               TR::SymbolReference * bndCHKSymRef = comp()->getSymRefTab()->findOrCreateArrayBoundsCheckSymbolRef(methodSymbol);
               TR::Node * spineCHK = TR::Node::create(node, TR::SpineCHK, 3);
               spineCHK->setAndIncChild(1, object);
               spineCHK->setAndIncChild(2, index);
               spineCHK->setSymbolReference(bndCHKSymRef);

               // Replace the call with a load/store into/from the element address
               if (value)
                  {
                  // This is a store
                  if (!isStatic && type == TR::Address && (TR::Compiler->om.writeBarrierType() != gc_modron_wrtbar_none))
                     {
                     node = TR::Node::recreateWithoutProperties(node, TR::awrtbari, 3, addrCalc, value, object, unsafeSymRef);
                     spineCHK->setAndIncChild(0, addrCalc);
                     }
                  else
                     {
                     if (value->getDataType() != type)
                        {
                        TR::ILOpCodes conversionOpCode = TR::ILOpCode::getProperConversion(value->getDataType(), type, needUnsignedConversion(symbol->getRecognizedMethod()));

                        // Sanity check for future modifications
                        TR_ASSERT(conversionOpCode != TR::BadILOp, "Could not get proper conversion on the value node %p n%dn for call %p n%dn.\n", value, value->getGlobalIndex(), node, node->getGlobalIndex());

                        value = TR::Node::create(conversionOpCode, 1, value);
                        }

                     node = TR::Node::recreateWithoutProperties(node, comp()->il.opCodeForIndirectArrayStore(type), 2, addrCalc, value, unsafeSymRef);

                     spineCHK->setAndIncChild(0, node);
                     spineCHK->setSpineCheckWithArrayElementChild(true);
                     }

                  if (trace())
                     traceMsg(comp(), "Created node [" POINTER_PRINTF_FORMAT "] to store the value [" POINTER_PRINTF_FORMAT "] to target location [" POINTER_PRINTF_FORMAT "] with spineCHK [" POINTER_PRINTF_FORMAT "]\n", node, value, addrCalc, spineCHK);
                  }
               else
                  {
                  //This is a load
                  if (node->getDataType() != type)
                     {
                     TR::ILOpCodes conversionOpCode = TR::ILOpCode::getProperConversion(type, node->getDataType(), needUnsignedConversion(symbol->getRecognizedMethod()));

                     // Sanity check for future modifications
                     TR_ASSERT(conversionOpCode != TR::BadILOp, "Could not get proper conversion on the result of call %p n%dn.\n", node, node->getGlobalIndex());

                     TR::Node *load = TR::Node::createWithSymRef(node, comp()->il.opCodeForIndirectArrayLoad(type), 1, unsafeSymRef);

                     load->setAndIncChild(0, addrCalc);

                     node = TR::Node::recreateWithoutProperties(node, conversionOpCode, 1, load);

                     spineCHK->setAndIncChild(0, load);
                     }
                  else
                     {
                     node = TR::Node::recreateWithoutProperties(node, comp()->il.opCodeForIndirectArrayLoad(type), 1, addrCalc, unsafeSymRef);

                     spineCHK->setAndIncChild(0, node);
                     }

                  spineCHK->setSpineCheckWithArrayElementChild(true);

                  if (trace())
                     traceMsg(comp(), "Created node [" POINTER_PRINTF_FORMAT "] to load from location [" POINTER_PRINTF_FORMAT "] with spineCHK [" POINTER_PRINTF_FORMAT "]\n", node, addrCalc, spineCHK);
                  }

               // Create a tree for spine check
               TR::TreeTop * newTree = TR::TreeTop::create(comp(), spineCHK);
               tt->insertAfter(newTree);

               // Anchor the store or load with compressedrefs if needed
               // wrtbari needs to be under a treetop node if not anchored by compressedrefs
               if (comp()->useCompressedPointers() && TR::TransformUtil::fieldShouldBeCompressed(node, comp()))
                  {
                  node = TR::Node::createCompressedRefsAnchor(node);
                  newTree = newTree->insertAfter(TR::TreeTop::create(comp(), node));
                  }
               else if (node->getOpCodeValue() == TR::awrtbari)
                  {
                  node = TR::Node::create(TR::treetop, 1, node);
                  newTree = newTree->insertAfter(TR::TreeTop::create(comp(), node));
                  }

               tt->getNode()->removeAllChildren();
               TR::TransformUtil::removeTree(comp(), tt);
               tt = newTree;

               }
            else
               {
               TR::Node *addrCalc = NULL;

               // Calculate element address
               if (TR::Compiler->target.is64Bit())
                  addrCalc = TR::Node::create(TR::aladd, 2, object, offset);
               else
                  addrCalc = TR::Node::create(TR::aiadd, 2, object, TR::Node::create(TR::l2i, 1, offset));

               if (value)
                  {
                  // This is a store
                  if (!isStatic && type == TR::Address && (TR::Compiler->om.writeBarrierType() != gc_modron_wrtbar_none))
                     node = TR::Node::recreateWithoutProperties(node, TR::awrtbari, 3, addrCalc, value, object, unsafeSymRef);
                  else
                     {
                     if (value->getDataType() != type)
                        {
                        TR::ILOpCodes conversionOpCode = TR::ILOpCode::getProperConversion(value->getDataType(), type, needUnsignedConversion(symbol->getRecognizedMethod()));

                        // Sanity check for future modifications
                        TR_ASSERT(conversionOpCode != TR::BadILOp, "Could not get proper conversion on the value node %p n%dn for call %p n%dn.\n", value, value->getGlobalIndex(), node, node->getGlobalIndex());

                        value = TR::Node::create(conversionOpCode, 1, value);
                        }

                     TR::ILOpCodes opCodeForIndirectStore = isArrayOperation ? comp()->il.opCodeForIndirectArrayStore(type) : comp()->il.opCodeForIndirectStore(type);

                     node = TR::Node::recreateWithoutProperties(node, opCodeForIndirectStore, 2, addrCalc, value, unsafeSymRef);
                     }

                  if (trace())
                     traceMsg(comp(), "Created node [" POINTER_PRINTF_FORMAT "] to store the value [" POINTER_PRINTF_FORMAT "] to target location [" POINTER_PRINTF_FORMAT "]\n", node, value, addrCalc);

                  if (comp()->useCompressedPointers() && TR::TransformUtil::fieldShouldBeCompressed(node, comp()))
                     node = TR::Node::createCompressedRefsAnchor(node);
                  }
               else
                  {
                  TR::ILOpCodes opCodeForIndirectLoad = isArrayOperation ? comp()->il.opCodeForIndirectArrayLoad(type) : comp()->il.opCodeForIndirectLoad(type);

                  // This is a load
                  if (node->getDataType() != type)
                     {
                     TR::ILOpCodes conversionOpCode = TR::ILOpCode::getProperConversion(type, node->getDataType(), needUnsignedConversion(symbol->getRecognizedMethod()));

                     // Sanity check for future modifications
                     TR_ASSERT(conversionOpCode != TR::BadILOp, "Could not get proper conversion on the result of call %p n%dn.\n", node, node->getGlobalIndex());

                     TR::Node *load = TR::Node::createWithSymRef(node, opCodeForIndirectLoad, 1, unsafeSymRef);

                     load->setAndIncChild(0, addrCalc);

                     node = TR::Node::recreateWithoutProperties(node, conversionOpCode, 1, load);
                     }
                  else
                     {
                     node = TR::Node::recreateWithoutProperties(node, opCodeForIndirectLoad, 1, addrCalc, unsafeSymRef);
                     }

                  if (trace())
                     traceMsg(comp(), "Created node [" POINTER_PRINTF_FORMAT "] to load from location [" POINTER_PRINTF_FORMAT "]\n", node, addrCalc);

                  if (comp()->useCompressedPointers() && TR::TransformUtil::fieldShouldBeCompressed(node, comp()))
                     node = TR::Node::createCompressedRefsAnchor(node);
                  }

               if (node->getOpCodeValue() != TR::compressedRefs)
                  node = TR::Node::create(TR::treetop, 1, node);
               TR::TreeTop * newTree = tt->insertAfter(TR::TreeTop::create(comp(), node));
               tt->getNode()->removeAllChildren();
               TR::TransformUtil::removeTree(comp(), tt);
               tt = newTree;
               }
            }
         }
      }

   return 0;
   }

const char *
TR_UnsafeFastPath::optDetailString() const throw()
   {
   return "O^O UNSAFE FAST PATH: ";
   }
