/*******************************************************************************
* Copyright (c) 2017, 2017 IBM Corp. and others
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
#include "optimizer/CallInfo.hpp"
#include "optimizer/J9CallGraph.hpp"
#include "optimizer/PreExistence.hpp"
#include "optimizer/Structure.hpp"
#include "codegen/CodeGenerator.hpp"                      // for CodeGenerator
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                                   // for ILOpCode, etc
#include "ilgen/IlGenRequest.hpp"
#include "ilgen/IlGeneratorMethodDetails.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "optimizer/Structure.hpp"                        // TR_RegionAnalysis
#include "optimizer/StructuralAnalysis.hpp"
#include "optimizer/TransformUtil.hpp"
#include "il/symbol/ParameterSymbol.hpp"
#include "env/SharedCache.hpp"                            // for TR_SharedCache
#include "env/VMJ9.h"
#include "ras/DebugCounter.hpp"

void J9::RecognizedCallTransformer::processSimpleMath(TR::Node* node, TR::ILOpCodes opcode)
   {
   TR::Node::recreate(node, opcode);
   }

void J9::RecognizedCallTransformer::processCRC32(TR::Node* node)
   {
   //TR::Node::recreate(node, opcode);
   while (node->getNumChildren() > 4)
      {
      node->removeChild(0);
      }
   auto iv     = node->getAndDecChild(0);
   auto object = node->getAndDecChild(1);
   auto offset = node->getAndDecChild(2);
   auto size   = node->getAndDecChild(3);

   node->setAndIncChild(0, iv);
   node->setAndIncChild(1, TR::Node::create(TR::aiadd, 2, TR::Node::create(TR::aiadd, 2, object,
                                                                           TR::Node::iconst(8)), // TODO: 8 be replaced of array header size
                                            offset));
   node->setAndIncChild(2, size);
   node->setChild(3, NULL);
   node->setNumChildren(3);
   node->setSymbolReference(comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_CRC32, true, true, true));
   }

bool J9::RecognizedCallTransformer::isInlineable(TR::TreeTop* treetop)
   {
   auto node = treetop->getNode()->getFirstChild();
   switch(node->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod())
      {
      case TR::java_lang_Integer_rotateLeft:
         return TR::Compiler->target.cpu.isX86();
      case TR::java_lang_Math_max_I:
      case TR::java_lang_Math_min_I:
      case TR::java_lang_Math_max_L:
      case TR::java_lang_Math_min_L:
         return TR::Compiler->target.cpu.isX86() && !comp()->getOption(TR_DisableMaxMinOptimization);
      case TR::java_util_zip_CRC32_updateBytes:
         return TR::Compiler->target.cpu.isX86();
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
         processSimpleMath(node, TR::irol);
         break;
      case TR::java_lang_Math_max_I:
         processSimpleMath(node, TR::imax);
         break;
      case TR::java_lang_Math_min_I:
         processSimpleMath(node, TR::imin);
         break;
      case TR::java_lang_Math_max_L:
         processSimpleMath(node, TR::lmax);
         break;
      case TR::java_lang_Math_min_L:
         processSimpleMath(node, TR::lmin);
         break;
      case TR::java_util_zip_CRC32_updateBytes:
         processCRC32(node);
         break;
      default:
         break;
      }
   }
