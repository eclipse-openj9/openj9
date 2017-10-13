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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "optimizer/UnsafeFastPath.hpp"

#include <stddef.h>                            // for size_t
#include <stdint.h>                            // for int32_t, uint32_t, etc
#include "codegen/CodeGenerator.hpp"           // for CodeGenerator, etc
#include "codegen/FrontEnd.hpp"                // for TR_FrontEnd, feGetEnv, etc
#include "compile/Compilation.hpp"             // for Compilation, comp, etc
#include "compile/ResolvedMethod.hpp"          // for TR_ResolvedMethod
#include "compile/SymbolReferenceTable.hpp"    // for SymbolReferenceTable, etc
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"         // for TR::Options, etc
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/VMJ9.h"
#include "env/j9method.h"
#include "il/ILOpCodes.hpp"                    // for ILOpCodes::treetop, etc
#include "il/ILOps.hpp"                        // for ILOpCode, TR::ILOpCode
#include "il/Node.hpp"                         // for Node, etc
#include "il/Node_inlines.hpp"                 // for Node::getFirstChild, etc
#include "il/Symbol.hpp"                       // for Symbol, etc
#include "il/SymbolReference.hpp"              // for SymbolReference, etc
#include "il/TreeTop.hpp"                      // for TreeTop
#include "il/TreeTop_inlines.hpp"              // for TreeTop::getNode, etc
#include "il/symbol/MethodSymbol.hpp"          // for MethodSymbol, etc
#include "il/symbol/ResolvedMethodSymbol.hpp"  // for ResolvedMethodSymbol
#include "infra/Assert.hpp"                    // for TR_ASSERT
#include "optimizer/Optimization.hpp"          // for Optimization
#include "optimizer/Optimization_inlines.hpp"  // for trace()
#include "optimizer/TransformUtil.hpp"         // for calculateElementAddress

/**
 * This replaces recognized unsafe calls to direct memory operations
 */
int32_t TR_UnsafeFastPath::perform()
   {
   if (comp()->getOption(TR_DisableUnsafe))
      return 0;

   TR::ResolvedMethodSymbol *methodSymbol = comp()->getMethodSymbol();
   for (TR::TreeTop * tt = methodSymbol->getFirstTreeTop(); tt != NULL; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode()->getChild(0); // Get the first child of the tree
      if (node && node->getOpCode().isCall())
         {
         TR::SymbolReference *symRef = node->getSymbolReference();
         TR::MethodSymbol *symbol = node->getSymbol()->castToMethodSymbol();

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
         if (TR_J9MethodBase::isVarHandleOperationMethod(callerMethod) &&
             TR_J9MethodBase::isUnsafeGetPutWithObjectArg(calleeMethod))
            {
            switch (callerMethod)
               {
               case TR::java_lang_invoke_StaticFieldVarHandle_StaticFieldVarHandleOperations_OpMethod:
                  isStatic = true;
                  break;
               default:
                  break;
               }

            switch (callerMethod)
               {
               case TR::java_lang_invoke_ArrayVarHandle_ArrayVarHandleOperations_OpMethod:
               case TR::java_lang_invoke_ByteArrayViewVarHandle_ByteArrayViewVarHandleOperations_OpMethod:
                  isArrayOperation = true;
                  break;
               default:
                  break;
               }

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
            TR::SymbolReference * unsafeSymRef = comp()->getSymRefTab()->findOrCreateUnsafeSymbolRef(type, true, false, isVolatile);

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

               // Genarete a spine check, need a symref
               // We don't need a BNDCHK because these are unsafe calls from interal code
               TR::SymbolReference * bndCHKSymRef = comp()->getSymRefTab()->findOrCreateArrayBoundsCheckSymbolRef(methodSymbol);
               TR::Node * spineCHK = TR::Node::create(node, TR::SpineCHK, 3);
               spineCHK->setAndIncChild(1, object);
               spineCHK->setAndIncChild(2, index);
               spineCHK->setSymbolReference(bndCHKSymRef);

               // Replace the call with a load/store into/from the element address
               if (value)
                  {
                  // This is a store
                  if (type == TR::Address && (comp()->getOptions()->getGcMode() != TR_WrtbarNone))
                     {
                     node = TR::Node::recreateWithoutProperties(node, TR::wrtbari, 3, addrCalc, value, object, unsafeSymRef);
                     spineCHK->setAndIncChild(0, addrCalc);
                     }
                  else
                     {
                     if (isByIndex && value->getDataType() != type)
                        {
                        TR::ILOpCodes conversionOpCode = TR::ILOpCode::getProperConversion(value->getDataType(), type, true);

                        // Sanity check for future modifications
                        TR_ASSERT(conversionOpCode != TR::BadILOp, "Could not get proper conversion for a JITHelpers byIndex operation.\n");

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
                  if (isByIndex && node->getDataType() != type)
                     {
                     TR::ILOpCodes conversionOpCode = TR::ILOpCode::getProperConversion(type, node->getDataType(), true);

                     // Sanity check for future modifications
                     TR_ASSERT(conversionOpCode != TR::BadILOp, "Could not get proper conversion for a JITHelpers byIndex operation.\n");

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
               if (comp()->useCompressedPointers() && (type == TR::Address))
                  {
                  node = TR::Node::createCompressedRefsAnchor(node);
                  newTree = newTree->insertAfter(TR::TreeTop::create(comp(), node));
                  }
               else if (node->getOpCodeValue() == TR::wrtbari)
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
                  if (type == TR::Address && (comp()->getOptions()->getGcMode() != TR_WrtbarNone))
                     node = TR::Node::recreateWithoutProperties(node, TR::wrtbari, 3, addrCalc, value, object, unsafeSymRef);
                  else
                     {
                     if (isByIndex && value->getDataType() != type)
                        {
                        TR::ILOpCodes conversionOpCode = TR::ILOpCode::getProperConversion(value->getDataType(), type, true);

                        // Sanity check for future modifications
                        TR_ASSERT(conversionOpCode != TR::BadILOp, "Could not get proper conversion for a JITHelpers byIndex operation.\n");

                        value = TR::Node::create(conversionOpCode, 1, value);
                        }

                     TR::ILOpCodes opCodeForIndirectStore = isArrayOperation ? comp()->il.opCodeForIndirectArrayStore(type) : comp()->il.opCodeForIndirectStore(type);

                     node = TR::Node::recreateWithoutProperties(node, opCodeForIndirectStore, 2, addrCalc, value, unsafeSymRef);
                     }

                  if (trace())
                     traceMsg(comp(), "Created node [" POINTER_PRINTF_FORMAT "] to store the value [" POINTER_PRINTF_FORMAT "] to target location [" POINTER_PRINTF_FORMAT "]\n", node, value, addrCalc);

                  if (comp()->useCompressedPointers() && type == TR::Address)
                     node = TR::Node::createCompressedRefsAnchor(node);
                  }
               else
                  {
                  TR::ILOpCodes opCodeForIndirectLoad = isArrayOperation ? comp()->il.opCodeForIndirectArrayLoad(type) : comp()->il.opCodeForIndirectLoad(type);

                  // This is a load
                  if (isByIndex && node->getDataType() != type)
                     {
                     TR::ILOpCodes conversionOpCode = TR::ILOpCode::getProperConversion(type, node->getDataType(), true);

                     // Sanity check for future modifications
                     TR_ASSERT(conversionOpCode != TR::BadILOp, "Could not get proper conversion for a JITHelpers byIndex operation.\n");

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

                  if (comp()->useCompressedPointers() && (type == TR::Address))
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
