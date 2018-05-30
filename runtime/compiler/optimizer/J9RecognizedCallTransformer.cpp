/*******************************************************************************
* Copyright (c) 2017, 2018 IBM Corp. and others
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

#include "optimizer/RecognizedCallTransformer.hpp"

#include "compile/ResolvedMethod.hpp"
#include "env/CompilerEnv.hpp"
#include "env/VMJ9.h"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                                   // for ILOpCode, etc
#include "ilgen/IlGenRequest.hpp"
#include "ilgen/IlGeneratorMethodDetails.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "optimizer/CallInfo.hpp"
#include "optimizer/Structure.hpp"
#include "codegen/CodeGenerator.hpp"                      // for CodeGenerator
#include "optimizer/TransformUtil.hpp"

/*
 * \brief transform the arguments of case conversion methods to make the call node friendly to code generator
 *
 * \details
 *    The original case conversion method call has 2 children: the src string obj and the dst string obj which will be 
 *    the converted string obj.
 *    After the transformation, the 2 children are replaced with 3 new children: the src string array address, the dst string 
 *    array address and the string length. The code generator can then focus on converting the actual content of the array and
 *    doesn't need to calculate the various offsets on different platforms.
 *
 *    Before the transformation:
 *    icall
 *       =>aload src string obj
 *       =>aload dst string obj
 *
 *    After the transformation:
 *    icall
 *       =>aladd
 *          =>aloadi String.value 
 *             =>aload src string obj
 *          lonst arrayheaderoffset  
 *       =>aladd
 *          =>aloadi String.value 
 *             =>aload dst string obj
 *          lonst arrayheaderoffset  
 *       =>aloadi String.count
 *          =>aload src string obj
 */
void J9::RecognizedCallTransformer::processCaseConversion(TR::Node* node)
   {
   TR::Node *srcStringObject = node->getFirstChild();
   TR::Node *dstStringObject = node->getSecondChild();

   /* Offset of java/lang/String.value */
   uint32_t stringValueFieldOffset = 0;
   char *valueFieldString = NULL;
   TR_OpaqueClassBlock *stringClass = comp()->fej9()->getClassFromSignature("Ljava/lang/String;", strlen("Ljava/lang/String;"), comp()->getCurrentMethod(), true);

   if (comp()->fej9()->getInstanceFieldOffset(stringClass, "value", "[B") != ~0)
      {
      stringValueFieldOffset = comp()->fej9()->getInstanceFieldOffsetIncludingHeader("Ljava/lang/String;", "value", "[B", comp()->getCurrentMethod());
      valueFieldString = "java/lang/String.value [B";
      }
   else
      {
      TR_ASSERT(comp()->fej9()->getInstanceFieldOffset(stringClass, "value", "[C") != ~0, "can't find java/lang/String.value field");
      stringValueFieldOffset = comp()->fej9()->getInstanceFieldOffsetIncludingHeader("Ljava/lang/String;", "value", "[C", comp()->getCurrentMethod());
      valueFieldString = "java/lang/String.value [C";
      }

   TR::SymbolReference *stringValueFieldSymRef = comp()->getSymRefTab()->findOrFabricateShadowSymbol(comp()->getMethodSymbol(), 
                                                 TR::Symbol::Java_lang_String_value, 
                                                 TR::Address, 
                                                 stringValueFieldOffset, 
                                                 false /* isVolatile */, 
                                                 true  /* isPrivate */,
                                                 true  /* isFinal */,
                                                 valueFieldString);

   TR::Node* srcArrayObject = TR::Node::createWithSymRef(node, TR::aloadi, 1, srcStringObject, stringValueFieldSymRef); 
   TR::Node* dstArrayObject = TR::Node::createWithSymRef(node, TR::aloadi, 1, dstStringObject, stringValueFieldSymRef); 

   TR::Node *srcArray = TR::Compiler->target.is64Bit() ?
      TR::Node::create(node, TR::aladd, 2, srcArrayObject, TR::Node::lconst(srcArrayObject, TR::Compiler->om.contiguousArrayHeaderSizeInBytes())): 
      TR::Node::create(node, TR::aiadd, 2, srcArrayObject, TR::Node::iconst(srcArrayObject, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()));
 
   TR::Node *dstArray = TR::Compiler->target.is64Bit() ?
      TR::Node::create(node, TR::aladd, 2, dstArrayObject, TR::Node::lconst(dstArrayObject, TR::Compiler->om.contiguousArrayHeaderSizeInBytes())): 
      TR::Node::create(node, TR::aiadd, 2, dstArrayObject, TR::Node::iconst(dstArrayObject, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()));

   node->setAndIncChild(0, srcArray);
   srcStringObject->recursivelyDecReferenceCount();
   node->setAndIncChild(1, dstArray);
   dstStringObject->recursivelyDecReferenceCount();

   /* Offset of java/lang/String.count */
   uint32_t stringCountFieldOffset = comp()->fej9()->getInstanceFieldOffsetIncludingHeader("Ljava/lang/String;", "count", "I", comp()->getCurrentMethod());
   TR::SymbolReference *stringCountFieldSymRef = comp()->getSymRefTab()->findOrFabricateShadowSymbol(comp()->getMethodSymbol(), 
                                                 TR::Symbol::Java_lang_String_count, 
                                                 TR::Int32, 
                                                 stringCountFieldOffset, 
                                                 false /* isVolatile */, 
                                                 true  /* isPrivate */,
                                                 true  /* isFinal */,
                                                 "java/lang/String.count I");

   TR::Node *stringLength = TR::Node::createWithSymRef(node, TR::iloadi, 1, srcStringObject, stringCountFieldSymRef); 
   TR::Node *newChildren[1];
   newChildren[0] = stringLength;
   node->addChildren(newChildren, 1);
   }

void J9::RecognizedCallTransformer::processIntrinsicFunction(TR::TreeTop* treetop, TR::Node* node, TR::ILOpCodes opcode)
   {
   TR::Node::recreate(node, opcode);
   TR::TransformUtil::removeTree(comp(), treetop);
   }

bool J9::RecognizedCallTransformer::isInlineable(TR::TreeTop* treetop)
   {
   auto node = treetop->getNode()->getFirstChild();
   switch(node->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod())
      {
      case TR::java_lang_Integer_rotateLeft:
      //case TR::java_lang_Math_abs_I: TODO OMR's iabs evaluator does not fully comply Java Specs
      //case TR::java_lang_Math_abs_L: TODO OMR's labs evaluator does not fully comply Java Specs
      case TR::java_lang_Math_abs_F:
      case TR::java_lang_Math_abs_D:
         return TR::Compiler->target.cpu.isX86();
      case TR::java_lang_Math_max_I:
      case TR::java_lang_Math_min_I:
      case TR::java_lang_Math_max_L:
      case TR::java_lang_Math_min_L:
         return TR::Compiler->target.cpu.isX86() && !comp()->getOption(TR_DisableMaxMinOptimization);
      case TR::java_lang_String_toLowerHWOptimized:
      case TR::java_lang_String_toLowerHWOptimizedDecompressed:
      case TR::java_lang_String_toLowerHWOptimizedCompressed:
      case TR::java_lang_String_toUpperHWOptimized:
      case TR::java_lang_String_toUpperHWOptimizedDecompressed:
      case TR::java_lang_String_toUpperHWOptimizedCompressed:
         return comp()->cg()->getSupportsInlineStringCaseConversion() && TR::Compiler->target.cpu.isX86();
      default:
         return false;
      }
   }

void J9::RecognizedCallTransformer::transform(TR::TreeTop* treetop)
   {
   auto node = treetop->getNode()->getFirstChild();
   switch(node->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod())
      {
      case TR::java_lang_Integer_rotateLeft:
         processIntrinsicFunction(treetop, node, TR::irol);
         break;
      case TR::java_lang_Math_abs_I:
         processIntrinsicFunction(treetop, node, TR::iabs);
         break;
      case TR::java_lang_Math_abs_L:
         processIntrinsicFunction(treetop, node, TR::labs);
         break;
      case TR::java_lang_Math_abs_D:
         processIntrinsicFunction(treetop, node, TR::dabs);
         break;
      case TR::java_lang_Math_abs_F:
         processIntrinsicFunction(treetop, node, TR::fabs);
         break;
      case TR::java_lang_Math_max_I:
         processIntrinsicFunction(treetop, node, TR::imax);
         break;
      case TR::java_lang_Math_min_I:
         processIntrinsicFunction(treetop, node, TR::imin);
         break;
      case TR::java_lang_Math_max_L:
         processIntrinsicFunction(treetop, node, TR::lmax);
         break;
      case TR::java_lang_Math_min_L:
         processIntrinsicFunction(treetop, node, TR::lmin);
         break;
      case TR::java_lang_String_toLowerHWOptimized:
      case TR::java_lang_String_toLowerHWOptimizedDecompressed:
      case TR::java_lang_String_toUpperHWOptimized:
      case TR::java_lang_String_toUpperHWOptimizedDecompressed:
      case TR::java_lang_String_toLowerHWOptimizedCompressed:
      case TR::java_lang_String_toUpperHWOptimizedCompressed:
         processCaseConversion(node);
         break;
      default:
         break;
      }
   }
