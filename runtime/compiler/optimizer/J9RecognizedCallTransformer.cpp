/*******************************************************************************
* Copyright (c) 2017, 2019 IBM Corp. and others
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
#include "il/ILOps.hpp"
#include "ilgen/IlGenRequest.hpp"
#include "ilgen/IlGeneratorMethodDetails.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "optimizer/CallInfo.hpp"
#include "optimizer/Structure.hpp"
#include "codegen/CodeGenerator.hpp"
#include "optimizer/TransformUtil.hpp"
#include "env/j9method.h"
#include "optimizer/Optimization_inlines.hpp"

void J9::RecognizedCallTransformer::processIntrinsicFunction(TR::TreeTop* treetop, TR::Node* node, TR::ILOpCodes opcode)
   {
   TR::Node::recreate(node, opcode);
   TR::TransformUtil::removeTree(comp(), treetop);
   }

void J9::RecognizedCallTransformer::process_java_lang_Class_IsAssignableFrom(TR::TreeTop* treetop, TR::Node* node)
   {
   auto toClass = node->getChild(0);
   auto fromClass = node->getChild(1);
   auto nullchk = comp()->getSymRefTab()->findOrCreateNullCheckSymbolRef(comp()->getMethodSymbol());
   treetop->insertBefore(TR::TreeTop::create(comp(), TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, TR::Node::create(node, TR::PassThrough, 1, toClass), nullchk)));
   treetop->insertBefore(TR::TreeTop::create(comp(), TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, TR::Node::create(node, TR::PassThrough, 1, fromClass), nullchk)));

   TR::Node::recreate(treetop->getNode(), TR::treetop);
   node->setSymbolReference(comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_checkAssignable, false, false, false));
   node->setAndIncChild(0, TR::Node::createWithSymRef(TR::aloadi, 1, 1, toClass, comp()->getSymRefTab()->findOrCreateClassFromJavaLangClassSymbolRef()));
   node->setAndIncChild(1, TR::Node::createWithSymRef(TR::aloadi, 1, 1, fromClass, comp()->getSymRefTab()->findOrCreateClassFromJavaLangClassSymbolRef()));
   node->swapChildren();

   toClass->recursivelyDecReferenceCount();
   fromClass->recursivelyDecReferenceCount();
   }

void J9::RecognizedCallTransformer::process_java_lang_StringUTF16_toBytes(TR::TreeTop* treetop, TR::Node* node)
   {
   TR_J9VMBase* fej9 = static_cast<TR_J9VMBase*>(comp()->fe());

   TR::Node* valueNode = node->getChild(0);
   TR::Node* offNode = node->getChild(1);
   TR::Node* lenNode = node->getChild(2);

   anchorAllChildren(node, treetop);
   prepareToReplaceNode(node);

   int32_t byteArrayType = fej9->getNewArrayTypeFromClass(reinterpret_cast<TR_OpaqueClassBlock*>(fej9->getJ9JITConfig()->javaVM->byteArrayClass));

   TR::Node::recreateWithoutProperties(node, TR::newarray, 2, 
      TR::Node::create(TR::ishl, 2,
         lenNode, 
         TR::Node::iconst(1)),
      TR::Node::iconst(byteArrayType), 

      getSymRefTab()->findOrCreateNewArraySymbolRef(node->getSymbolReference()->getOwningMethodSymbol(comp())));

   TR::Node* newByteArrayNode = node;
   newByteArrayNode->setCanSkipZeroInitialization(true);
   newByteArrayNode->setIsNonNull(true);

   TR::Node* newCallNode = TR::Node::createWithSymRef(node, TR::call, 5, 
      getSymRefTab()->methodSymRefFromName(comp()->getMethodSymbol(), "java/lang/String", "decompressedArrayCopy", "([CI[BII)V", TR::MethodSymbol::Static));
   newCallNode->setAndIncChild(0, valueNode);
   newCallNode->setAndIncChild(1, offNode);
   newCallNode->setAndIncChild(2, newByteArrayNode);
   newCallNode->setAndIncChild(3, TR::Node::iconst(0));
   newCallNode->setAndIncChild(4, lenNode);

   treetop->insertAfter(TR::TreeTop::create(comp(), TR::Node::create(node, TR::treetop, 1, newCallNode)));
   }

void J9::RecognizedCallTransformer::process_java_lang_StrictMath_sqrt(TR::TreeTop* treetop, TR::Node* node)
   {
   if (!node->getSymbol()->castToResolvedMethodSymbol()->canReplaceWithHWInstr())
      {
      TR::Node* dumpNode = node->getChild(0);
      TR::Node* valueNode = node->getChild(1);

      anchorNode(dumpNode, treetop);
      prepareToReplaceNode(node);

      TR::Node::recreateWithoutProperties(node, TR::dsqrt, 1, valueNode, getSymRefTab()->findOrCreateNewArraySymbolRef(node->getSymbolReference()->getOwningMethodSymbol(comp())));
      TR::TransformUtil::removeTree(comp(), treetop);
      }

   else
      {
      TR::Node::recreate(node, TR::dsqrt);
      TR::TransformUtil::removeTree(comp(), treetop);
      }
   }
/*
Transform an Unsafe atomic call to diamonds with equivalent semantics

                          yes
isObjectNull ------------------------------------------>
    |                                                  |
    | no                                               |
    |                     yes                          |
isNotLowTagged ---------------------------------------->
    |                                                  |
    |  no                                              |
    |           no                                     |
isFinal ---------------------->                        |
    |                         |                        |
    | yes                     |                        |
    |                         |                        |
call the                calculate address      calculate address
original method         for static field       for instance field
    |                         |                and absolute address
    |                         |                        |
    |                         |________________________|
    |                                     |
    |                         xcall atomic method helper
    |                                     |
    |_____________________________________|
                    |
      program after the original call

Block before the transformation: ===>

start Block_A
  ...
xcall Unsafe.xxx
  ...
end Block_A

Blocks after the transformation: ===>

start Block_A
...
ifacmpeq -> <Block_E>
  object
  aconst null
end Block_A

start Block_B
iflcmpne -> <Block_E>
  land
    lload <offset>
    lconst J9_SUN_STATIC_FIELD_OFFSET_TAG
  lconst J9_SUN_STATIC_FIELD_OFFSET_TAG
end Block_B

start Block_C
iflcmpeq -> <Block_F>
  land
    lload <offset>
    lconst J9_SUN_FINAL_FIELD_OFFSET_TAG
  lconst J9_SUN_FINAL_FIELD_OFFSET_TAG
end Block_C

start Block_D
astore <object>
  aloadi ramStaticsFromClass
   ...
lstore <offset>
  land
    lload <offset>
    lconst ~J9_SUN_FIELD_OFFSET_MASK
end Block_D

start Block_E
xcall atomic method helper
  aladd
    aload <object>
    lload <offset>
  xload value
end Block_E

...

start Block_F
call jitReportFinalFieldModified
go to <Block_E>
end Block_F
*/
void J9::RecognizedCallTransformer::processUnsafeAtomicCall(TR::TreeTop* treetop, TR::SymbolReferenceTable::CommonNonhelperSymbol helper, bool needsNullCheck)
   {
   bool enableTrace = trace() || comp()->getOption(TR_TraceUnsafeInlining);
   bool isNotStaticField = !strncmp(comp()->getCurrentMethod()->classNameChars(), "java/util/concurrent/atomic/", strlen("java/util/concurrent/atomic/"));

   TR::Node* unsafeCall = treetop->getNode()->getFirstChild();
   TR::Node* objectNode = unsafeCall->getChild(1);
   TR::Node* offsetNode = unsafeCall->getChild(2);
   TR::Node* address = NULL;

   // Preserve null check on the unsafe object
   if (treetop->getNode()->getOpCode().isNullCheck())
      {
      TR::Node *passthrough = TR::Node::create(unsafeCall, TR::PassThrough, 1);
      passthrough->setAndIncChild(0, unsafeCall->getFirstChild());
      TR::Node * checkNode = TR::Node::createWithSymRef(treetop->getNode(), TR::NULLCHK, 1, passthrough, treetop->getNode()->getSymbolReference());
      treetop->insertBefore(TR::TreeTop::create(comp(), checkNode));
      TR::Node::recreate(treetop->getNode(), TR::treetop);
      if (enableTrace)
         traceMsg(comp(), "Created node %p to preserve NULLCHK on unsafe call %p\n", checkNode, unsafeCall);
      }

   if (isNotStaticField)
      {
      // It is safe to skip diamond, the address can be calculated directly via [object+offset]
      address = TR::Compiler->target.is32Bit() ? TR::Node::create(TR::aiadd, 2, objectNode, TR::Node::create(TR::l2i, 1, offsetNode)) :
                                              TR::Node::create(TR::aladd, 2, objectNode, offsetNode);
      if (enableTrace)
         traceMsg(comp(), "Field is not static, use the object and offset directly\n");
      }
   else
      {
      // Otherwise, the address is [object+offset] for non-static field,
      //            or [object's ramStaticsFromClass + (offset & ~mask)] for static field

      // Save all the children to temps before splitting the block
      TR::TransformUtil::createTempsForCall(this, treetop);

      auto cfg = comp()->getMethodSymbol()->getFlowGraph();
      objectNode = unsafeCall->getChild(1);
      offsetNode = unsafeCall->getChild(2);

      // Null Check
      if (needsNullCheck)
         {
         auto NULLCHKNode = TR::Node::createWithSymRef(unsafeCall, TR::NULLCHK, 1,
                                                       TR::Node::create(TR::PassThrough, 1, objectNode->duplicateTree()),
                                                       comp()->getSymRefTab()->findOrCreateNullCheckSymbolRef(comp()->getMethodSymbol()));
         treetop->insertBefore(TR::TreeTop::create(comp(), NULLCHKNode));
         if (enableTrace)
            traceMsg(comp(), "Created NULLCHK tree %p on the first argument of Unsafe call\n", treetop->getPrevTreeTop());
         }

      // Test if object is null
      auto isObjectNullNode = TR::Node::createif(TR::ifacmpeq, objectNode->duplicateTree(), TR::Node::aconst(0), NULL);
      auto isObjectNullTreeTop = TR::TreeTop::create(comp(), isObjectNullNode);
      treetop->insertBefore(isObjectNullTreeTop);
      treetop->getEnclosingBlock()->split(treetop, cfg);

      if (enableTrace)
         traceMsg(comp(), "Created isObjectNull test node n%dn, non-null object will fall through to Block_%d\n", isObjectNullNode->getGlobalIndex(), treetop->getEnclosingBlock()->getNumber());

      // Test if low tag is set
      auto isNotLowTaggedNode = TR::Node::createif(TR::iflcmpne,
                                               TR::Node::create(TR::land, 2, offsetNode->duplicateTree(), TR::Node::lconst(J9_SUN_STATIC_FIELD_OFFSET_TAG)),
                                               TR::Node::lconst(J9_SUN_STATIC_FIELD_OFFSET_TAG),
                                               NULL);
      auto isNotLowTaggedTreeTop = TR::TreeTop::create(comp(), isNotLowTaggedNode);
      treetop->insertBefore(isNotLowTaggedTreeTop);
      treetop->getEnclosingBlock()->split(treetop, cfg);

      if (enableTrace)
         traceMsg(comp(), "Created isNotLowTagged test node n%dn, static field will fall through to Block_%d\n", isNotLowTaggedNode->getGlobalIndex(), treetop->getEnclosingBlock()->getNumber());

      static char *disableIllegalWriteReport = feGetEnv("TR_DisableIllegalWriteReport");
      // Test if the call is a write to a static final field
      if (!disableIllegalWriteReport
          && !comp()->getOption(TR_DisableGuardedStaticFinalFieldFolding)
          && TR_J9MethodBase::isUnsafePut(unsafeCall->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod()))
         {
         // If the field is static final
         auto isFinalNode = TR::Node::createif(TR::iflcmpeq,
                                                  TR::Node::create(TR::land, 2, offsetNode->duplicateTree(), TR::Node::lconst(J9_SUN_FINAL_FIELD_OFFSET_TAG)),
                                                  TR::Node::lconst(J9_SUN_FINAL_FIELD_OFFSET_TAG),
                                                  NULL /*branchTarget*/);
         auto isFinalTreeTop = TR::TreeTop::create(comp(), isFinalNode);
         auto reportFinalFieldModification = TR::TransformUtil::generateReportFinalFieldModificationCallTree(comp(), objectNode->duplicateTree());
         auto elseBlock = treetop->getEnclosingBlock();
         TR::TransformUtil::createConditionalAlternatePath(comp(), isFinalTreeTop, reportFinalFieldModification, elseBlock, elseBlock, comp()->getMethodSymbol()->getFlowGraph(), true /*markCold*/);
         if (enableTrace)
            {
            traceMsg(comp(), "Created isFinal test node n%dn, non-final-static field will fall through to Block_%d, final field goes to Block_%d\n",
                     isFinalNode->getGlobalIndex(), treetop->getEnclosingBlock()->getNumber(), reportFinalFieldModification->getEnclosingBlock()->getNumber());
            }
         TR::DebugCounter::prependDebugCounter(comp(),
                                               TR::DebugCounter::debugCounterName(comp(),
                                                                                  "illegalWriteReport/atomic/(%s %s)",
                                                                                  comp()->signature(),
                                                                                  comp()->getHotnessName(comp()->getMethodHotness())),
                                                                                  reportFinalFieldModification->getNextTreeTop());

         }

      // Calculate static address
      auto objectAdjustmentNode = TR::Node::createWithSymRef(TR::astore, 1, 1,
                                                             TR::Node::createWithSymRef(TR::aloadi, 1, 1,
                                                                                        TR::Node::createWithSymRef(TR::aloadi, 1, 1,
                                                                                                                   objectNode->duplicateTree(),
                                                                                                                   comp()->getSymRefTab()->findOrCreateClassFromJavaLangClassSymbolRef()),
                                                                                        comp()->getSymRefTab()->findOrCreateRamStaticsFromClassSymbolRef()),
                                                             objectNode->getSymbolReference());
      auto offsetAdjustmentNode = TR::Node::createWithSymRef(TR::lstore, 1, 1,
                                                             TR::Node::create(TR::land, 2,
                                                                              offsetNode->duplicateTree(),
                                                                              TR::Node::lconst(~J9_SUN_FIELD_OFFSET_MASK)),
                                                             offsetNode->getSymbolReference());

      if (enableTrace)
         traceMsg(comp(), "Created node n%dn and n%dn to adjust object and offset for static field\n", objectAdjustmentNode->getGlobalIndex(), offsetAdjustmentNode->getGlobalIndex());

      treetop->insertBefore(TR::TreeTop::create(comp(), objectAdjustmentNode));
      treetop->insertBefore(TR::TreeTop::create(comp(), offsetAdjustmentNode));
      treetop->getEnclosingBlock()->split(treetop, cfg);

      if (enableTrace)
         traceMsg(comp(), "Block_%d contains call to atomic method helper, and is the target of isObjectNull and isNotLowTagged tests\n", treetop->getEnclosingBlock()->getNumber());

      // Setup CFG edges
      isObjectNullNode->setBranchDestination(treetop->getEnclosingBlock()->getEntry());
      cfg->addEdge(TR::CFGEdge::createEdge(isObjectNullTreeTop->getEnclosingBlock(), treetop->getEnclosingBlock(), comp()->trMemory()));
      isNotLowTaggedNode->setBranchDestination(treetop->getEnclosingBlock()->getEntry());
      cfg->addEdge(TR::CFGEdge::createEdge(isNotLowTaggedTreeTop->getEnclosingBlock(), treetop->getEnclosingBlock(), comp()->trMemory()));
      address = TR::Compiler->target.is32Bit() ? TR::Node::create(TR::aiadd, 2, objectNode->duplicateTree(), TR::Node::create(TR::l2i, 1, offsetNode->duplicateTree())) :
                                              TR::Node::create(TR::aladd, 2, objectNode->duplicateTree(), offsetNode->duplicateTree());
      }

   // Transmute the unsafe call to a call to atomic method helper
   unsafeCall->getChild(0)->recursivelyDecReferenceCount(); // replace unsafe object with the address
   unsafeCall->setAndIncChild(0, address);
   unsafeCall->removeChild(2); // remove offset node
   unsafeCall->removeChild(1); // remove object node
   unsafeCall->setSymbolReference(comp()->getSymRefTab()->findOrCreateCodeGenInlinedHelper(helper));
   }

bool J9::RecognizedCallTransformer::isInlineable(TR::TreeTop* treetop)
   {
   auto node = treetop->getNode()->getFirstChild();
   switch(node->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod())
      {
      case TR::sun_misc_Unsafe_getAndAddInt:
         return !comp()->getOption(TR_DisableUnsafe) && !comp()->compileRelocatableCode() && !TR::Compiler->om.canGenerateArraylets() && 
            cg()->supportsNonHelper(TR::SymbolReferenceTable::atomicFetchAndAddSymbol);
      case TR::sun_misc_Unsafe_getAndSetInt:
         return !comp()->getOption(TR_DisableUnsafe) && !comp()->compileRelocatableCode() && !TR::Compiler->om.canGenerateArraylets() && 
            cg()->supportsNonHelper(TR::SymbolReferenceTable::atomicSwapSymbol);
      case TR::sun_misc_Unsafe_getAndAddLong:
         return !comp()->getOption(TR_DisableUnsafe) && !comp()->compileRelocatableCode() && !TR::Compiler->om.canGenerateArraylets() && TR::Compiler->target.is64Bit() && 
            cg()->supportsNonHelper(TR::SymbolReferenceTable::atomicFetchAndAddSymbol);
      case TR::sun_misc_Unsafe_getAndSetLong:
         return !comp()->getOption(TR_DisableUnsafe) && !comp()->compileRelocatableCode() && !TR::Compiler->om.canGenerateArraylets() && TR::Compiler->target.is64Bit() && 
            cg()->supportsNonHelper(TR::SymbolReferenceTable::atomicSwapSymbol);
      case TR::java_lang_Class_isAssignableFrom:
         return cg()->supportsInliningOfIsAssignableFrom();
      case TR::java_lang_Integer_rotateLeft:
      case TR::java_lang_Long_rotateLeft:
      case TR::java_lang_Math_abs_I:
      case TR::java_lang_Math_abs_L:
      case TR::java_lang_Math_abs_F:
      case TR::java_lang_Math_abs_D:
         return TR::Compiler->target.cpu.isX86() || TR::Compiler->target.cpu.isZ();
      case TR::java_lang_Math_max_I:
      case TR::java_lang_Math_min_I:
      case TR::java_lang_Math_max_L:
      case TR::java_lang_Math_min_L:
         return !comp()->getOption(TR_DisableMaxMinOptimization);
      case TR::java_lang_StringUTF16_toBytes:
         return !comp()->compileRelocatableCode();
      case TR::java_lang_StrictMath_sqrt:
      case TR::java_lang_Math_sqrt:
         return TR::Compiler->target.cpu.isZ();
      default:
         return false;
      }
   }

void J9::RecognizedCallTransformer::transform(TR::TreeTop* treetop)
   {
   auto node = treetop->getNode()->getFirstChild();
   switch(node->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod())
      {
      case TR::sun_misc_Unsafe_getAndAddInt:
      case TR::sun_misc_Unsafe_getAndAddLong:
         processUnsafeAtomicCall(treetop, TR::SymbolReferenceTable::atomicFetchAndAddSymbol);
         break;
      case TR::sun_misc_Unsafe_getAndSetInt:
      case TR::sun_misc_Unsafe_getAndSetLong:
         processUnsafeAtomicCall(treetop, TR::SymbolReferenceTable::atomicSwapSymbol);
         break;
      case TR::java_lang_Class_isAssignableFrom:
         process_java_lang_Class_IsAssignableFrom(treetop, node);
         break;
      case TR::java_lang_Integer_rotateLeft:
         processIntrinsicFunction(treetop, node, TR::irol);
         break;
      case TR::java_lang_Long_rotateLeft:
         processIntrinsicFunction(treetop, node, TR::lrol);
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
      case TR::java_lang_StringUTF16_toBytes:
         process_java_lang_StringUTF16_toBytes(treetop, node);
         break;
      case TR::java_lang_StrictMath_sqrt:
         process_java_lang_StrictMath_sqrt(treetop, node);
         break;
      case TR::java_lang_Math_sqrt:
         processIntrinsicFunction(treetop, node, TR::dsqrt);
         break;
      default:
         break;
      }
   }
