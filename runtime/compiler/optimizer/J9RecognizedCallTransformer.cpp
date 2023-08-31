/*******************************************************************************
* Copyright IBM Corp. and others 2017
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

#include "optimizer/RecognizedCallTransformer.hpp"

#include "compile/ResolvedMethod.hpp"
#include "env/CompilerEnv.hpp"
#include "env/VMJ9.h"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/StaticSymbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "ilgen/IlGenRequest.hpp"
#include "ilgen/IlGeneratorMethodDetails.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "infra/ILWalk.hpp"
#include "infra/String.hpp"
#include "optimizer/CallInfo.hpp"
#include "optimizer/IdiomRecognitionUtils.hpp"
#include "optimizer/Structure.hpp"
#include "codegen/CodeGenerator.hpp"
#include "optimizer/TransformUtil.hpp"
#include "env/j9method.h"
#include "optimizer/Optimization_inlines.hpp"

void J9::RecognizedCallTransformer::processIntrinsicFunction(TR::TreeTop* treetop, TR::Node* node, TR::ILOpCodes opcode)
   {
   TR::Node::recreate(node, opcode);
   }

void J9::RecognizedCallTransformer::processConvertingUnaryIntrinsicFunction(TR::TreeTop* treetop, TR::Node* node, TR::ILOpCodes argConvertOpcode, TR::ILOpCodes opcode, TR::ILOpCodes resultConvertOpcode)
   {
   TR::Node* actualArg = TR::Node::create(argConvertOpcode, 1, node->getFirstChild());
   TR::Node* actualResult = TR::Node::create(opcode, 1, actualArg);

   TR::Node::recreate(node, resultConvertOpcode);
   node->getFirstChild()->decReferenceCount();
   node->setAndIncChild(0, actualResult);
   }

void J9::RecognizedCallTransformer::process_java_lang_Class_IsAssignableFrom(TR::TreeTop* treetop, TR::Node* node)
   {
   auto toClass = node->getChild(0);
   auto fromClass = node->getChild(1);
   auto nullchk = comp()->getSymRefTab()->findOrCreateNullCheckSymbolRef(comp()->getMethodSymbol());
   treetop->insertBefore(TR::TreeTop::create(comp(), TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, TR::Node::create(node, TR::PassThrough, 1, toClass), nullchk)));
   treetop->insertBefore(TR::TreeTop::create(comp(), TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, TR::Node::create(node, TR::PassThrough, 1, fromClass), nullchk)));

   TR::Node::recreate(treetop->getNode(), TR::treetop);
   node->setSymbolReference(comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_checkAssignable));
   node->setAndIncChild(0, TR::Node::createWithSymRef(TR::aloadi, 1, 1, toClass, comp()->getSymRefTab()->findOrCreateClassFromJavaLangClassSymbolRef()));
   node->setAndIncChild(1, TR::Node::createWithSymRef(TR::aloadi, 1, 1, fromClass, comp()->getSymRefTab()->findOrCreateClassFromJavaLangClassSymbolRef()));
   node->swapChildren();

   toClass->recursivelyDecReferenceCount();
   fromClass->recursivelyDecReferenceCount();
   }

static void substituteNode(
   TR::NodeChecklist &visited, TR::Node *subOld, TR::Node *subNew, TR::Node *node)
   {
   if (visited.contains(node))
      return;

   visited.add(node);

   TR_ASSERT_FATAL(node != subOld, "unexpected occurrence of old node");

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      if (child == subOld)
         {
         // subOld occurs at least at its point of evaluation and here, so it
         // won't hit refcount 0.
         TR_ASSERT_FATAL_WITH_NODE(
            subOld,
            subOld->getReferenceCount() >= 2,
            "expected node to be referenced elsewhere");

         subOld->decReferenceCount();
         node->setAndIncChild(i, subNew);
         }
      else
         {
         substituteNode(visited, subOld, subNew, child);
         }
      }
   }

void J9::RecognizedCallTransformer::process_java_lang_Class_cast(
   TR::TreeTop* treetop, TR::Node* node)
   {
   // See the comment in isInlineable().
   TR_ASSERT_FATAL_WITH_NODE(
      node,
      comp()->getOSRMode() != TR::involuntaryOSR,
      "unexpectedly transforming Class.cast with involuntary OSR");

   // These don't need to be anchored because they will both occur beneath
   // checkcast in the same treetop.
   TR::Node *jlClass = node->getArgument(0);
   TR::Node *object = node->getArgument(1);

   TR::TransformUtil::separateNullCheck(comp(), treetop, trace());

   TR::SymbolReferenceTable *srTab = comp()->getSymRefTab();

   TR::SymbolReference *classFromJavaLangClassSR =
      srTab->findOrCreateClassFromJavaLangClassSymbolRef();

   TR::SymbolReference *checkcastSR =
      srTab->findOrCreateCheckCastSymbolRef(comp()->getMethodSymbol());

   TR::Node *j9class = TR::Node::createWithSymRef(
      TR::aloadi, 1, 1, jlClass, classFromJavaLangClassSR);

   TR::Node *cast = TR::Node::createWithSymRef(
      node, TR::checkcast, 2, checkcastSR);

   cast->setAndIncChild(0, object);
   cast->setAndIncChild(1, j9class);

   if (node->getReferenceCount() > 1)
      {
      TR::NodeChecklist visited(comp());
      TR::TreeTop *entry = treetop->getEnclosingBlock()->getEntry();
      TR::TreeTop *start = treetop->getNextTreeTop();
      TR::TreeTop *end = entry->getExtendedBlockExitTreeTop();
      for (TR::TreeTopIterator it(start, comp()); it != end; ++it)
         {
         substituteNode(visited, node, object, it.currentNode());
         if (node->getReferenceCount() == 1)
            break;
         }
      }

   TR_ASSERT_FATAL_WITH_NODE(
      node,
      node->getReferenceCount() == 1,
      "expected exactly one occurrence to remain");

   treetop->setNode(cast);
   node->recursivelyDecReferenceCount();
   }

// This methods inlines a call node that calls StringCoding.encodeASCII into an if-diamond. The forward path of the
// if-diamond inlines the call using a compiler intrinsic and the fallback path reverts back to calling the method traditionally.
void J9::RecognizedCallTransformer::process_java_lang_StringCoding_encodeASCII(TR::TreeTop* treetop, TR::Node* node)
   {
   TR_J9VMBase *fej9 = static_cast<TR_J9VMBase*>(comp()->fe());
   TR_OpaqueClassBlock *stringClass = comp()->getStringClassPointer();
   if (!stringClass || !fej9->getByteArrayClass())
      {
      return;
      }
   uint32_t *latin1FieldAddress = (uint32_t *)fej9->getStaticFieldAddress(stringClass, (unsigned char *) "LATIN1", 6, (unsigned char *)"B", 1);
   TR::CFG *cfg = comp()->getFlowGraph();

   TR::Node *coderNode = node->getChild(0);
   TR::Node *sourceArrayNode = node->getChild(1);

   // Anchor the source array node as we need it in the fallthrough block.
   anchorNode(sourceArrayNode, treetop);

   // Now generate the ificmpne tree and insert it before the original call tree.
   // Note that *latin1FieldAddress will be correct for AOT compilations as well because String.LATIN1 is a final static field with
   // a constant initializer. This means its value is determined by the ROM class of String, and therefore when the String class chain validation succeeds
   // on AOT load, the value is guaranteed to be the same as the one we see during AOT compilation.
   TR::Node *constNode = TR::Node::iconst(node, ((TR_J9VM *)fej9)->dereferenceStaticFinalAddress(latin1FieldAddress, TR::Int32).dataInt32Bit);
   TR::Node *ifCmpNode = TR::Node::createif(TR::ificmpne, coderNode, constNode);
   TR::TreeTop *ifCmpTreeTop = TR::TreeTop::create(comp(), treetop->getPrevTreeTop(), ifCmpNode);

   // Split the current block right before the call (or after the ificmpne).
   TR::Block *ifCmpBlock = ifCmpTreeTop->getEnclosingBlock();
   TR::Block *fallbackPathBlock = ifCmpBlock->split(treetop, cfg, true /* fixUpCommoning */, true /* copyExceptionSuccessors */);
   // Then split again to create the tail block
   TR::Block *tailBlock = fallbackPathBlock->split(treetop->getNextTreeTop(), cfg, true /* fixUpCommoning */, true /* copyExceptionSuccessors */);

   // Now we have the originalBlock, the fallback block and the merge block (tailBlock). The call result will be an astore.
   // If the nodes in the above blocks were uncommoned as expected, then the treetop after the call node should be an astore to a temp slot.
   // So assert that it is, and then grab a reference to the temp slot so we can store to it ourselves in the forward path.
   // Note that we can rely on the call result having been used in a later tree since encodeASCII represents a private method whose call sites we can inspect
   // and because this transformation happens very early as part of ilgen opts
   TR::Node *resultStoreNode = treetop->getNextTreeTop()->getNode();
   TR::SymbolReference *tempSlotForCallResult = NULL;
   TR_ASSERT_FATAL(resultStoreNode, "Treetop after call is not an astore");
   TR_ASSERT_FATAL(resultStoreNode->getOpCode().getOpCodeValue() == TR::astore, "Treetop after call must be an astore to a temp!");
   tempSlotForCallResult = resultStoreNode->getSymbolReference();
   TR_ASSERT_FATAL(tempSlotForCallResult, "Symbol reference for store node can't be null\n");
   TR_ASSERT_FATAL(resultStoreNode->getChild(0) == node, "The value stored must be the call result");

   // Ready to create our fallthrough block now. Connect it to ifCmpTreeTop, and then split it with nodes commoned.
   int32_t byteArrayType = fej9->getNewArrayTypeFromClass(fej9->getByteArrayClass());

   // Create a new arraylength node.
   sourceArrayNode = node->getChild(1)->duplicateTree();
   TR::Node *lenNode = TR::Node::create(node, TR::arraylength, 1, sourceArrayNode);

   // Create the destination array
   TR::Node *destinationArrayNode = TR::Node::createWithSymRef(node, TR::newarray, 2, comp()->getSymRefTab()->findOrCreateNewArraySymbolRef(node->getSymbolReference()->getOwningMethodSymbol(comp())));
   destinationArrayNode->setAndIncChild(0, lenNode);
   destinationArrayNode->setAndIncChild(1, TR::Node::iconst(byteArrayType));
   TR::TreeTop *destinationArrayTreeTop = TR::TreeTop::create(comp(), TR::Node::create(node, TR::treetop, 1, destinationArrayNode));
   ifCmpTreeTop->insertAfter(destinationArrayTreeTop);
   destinationArrayNode->setCanSkipZeroInitialization(true);
   destinationArrayNode->setIsNonNull(true);

   // We now have the length node, and also the destination array. Now we create an encodeASCIISymbol node to do the encoding operation.
   /*
   // tree looks as follows:
   // call encodeASCIISymbol
   //    input ptr
   //    output ptr
   //    input length (in elements)
   */

   TR::SymbolReference *methodSymRef = comp()->getSymRefTab()->findOrCreateEncodeASCIISymbolRef();
   TR::Node *encodeASCIINode = TR::Node::createWithSymRef(TR::call, 3, methodSymRef);

   TR::Node *newInputNode = NULL;
   TR::Node *arrayHeaderSizeNode = NULL;
   TR::Node *newOutputNode = NULL;

   if (comp()->target().is64Bit())
      {
      newInputNode = TR::Node::create(sourceArrayNode, TR::aladd, 2);
      newOutputNode = TR::Node::create(destinationArrayNode, TR::aladd, 2);
      arrayHeaderSizeNode = TR::Node::lconst((int64_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
      }
   else
      {
      newInputNode = TR::Node::create(sourceArrayNode, TR::aiadd, 2);
      newOutputNode = TR::Node::create(destinationArrayNode, TR::aiadd, 2);
      arrayHeaderSizeNode = TR::Node::iconst((int32_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
      }

   newInputNode->setAndIncChild(0, sourceArrayNode);
   newInputNode->setAndIncChild(1, arrayHeaderSizeNode);
   encodeASCIINode->setAndIncChild(0, newInputNode);

   newOutputNode->setAndIncChild(0, destinationArrayNode);
   newOutputNode->setAndIncChild(1, arrayHeaderSizeNode);
   encodeASCIINode->setAndIncChild(1, newOutputNode);

   encodeASCIINode->setAndIncChild(2, lenNode);

   TR::TreeTop *encodeASCIITreeTop = TR::TreeTop::create(comp(), TR::Node::create(node, TR::treetop, 1, encodeASCIINode));
   destinationArrayTreeTop->insertAfter(encodeASCIITreeTop);

   TR::Node *storeNode = TR::Node::createWithSymRef(node, TR::astore, 1, destinationArrayNode, tempSlotForCallResult);
   TR::TreeTop *storeNodeTreeTop = TR::TreeTop::create(comp(), encodeASCIITreeTop, storeNode);

   // Now split starting from destinationArrayTreeTop and uncommon the nodes..
   TR::Block *fallthroughBlock = destinationArrayTreeTop->getEnclosingBlock()->split(destinationArrayTreeTop, cfg, true /* fixUpCommoning */, true /* copyExceptionSuccessors */);

   // Now create a node to go to the merge (i.e. tail) block.
   TR::Node *gotoNode = TR::Node::create(node, TR::Goto);
   TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode, NULL, NULL);
   gotoNode->setBranchDestination(tailBlock->getEntry());
   fallthroughBlock->getExit()->insertBefore(gotoTree);

   // Now we have fallthrough block, fallback block and tail/merge block. Let's set the ifcmp's destination to the fallback block, and update the CFG as well.
   ifCmpNode->setBranchDestination(fallbackPathBlock->getEntry());
   cfg->addEdge(ifCmpTreeTop->getEnclosingBlock(), fallbackPathBlock);
   cfg->addEdge(fallthroughBlock, tailBlock);
   cfg->removeEdge(fallthroughBlock, fallbackPathBlock);
   }

void J9::RecognizedCallTransformer::process_java_lang_StringUTF16_toBytes(TR::TreeTop* treetop, TR::Node* node)
   {
   TR_J9VMBase* fej9 = static_cast<TR_J9VMBase*>(comp()->fe());

   TR::Node* valueNode = node->getChild(0);
   TR::Node* offNode = node->getChild(1);
   TR::Node* lenNode = node->getChild(2);

   anchorAllChildren(node, treetop);
   prepareToReplaceNode(node);

   int32_t byteArrayType = fej9->getNewArrayTypeFromClass(fej9->getByteArrayClass());

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

void J9::RecognizedCallTransformer::process_java_lang_StrictMath_and_Math_sqrt(TR::TreeTop* treetop, TR::Node* node)
   {
   TR::Node* valueNode = node->getLastChild();

   anchorAllChildren(node, treetop);
   prepareToReplaceNode(node);

   TR::Node::recreate(node, TR::dsqrt);
   node->setNumChildren(1);
   node->setAndIncChild(0, valueNode);

   TR::TransformUtil::removeTree(comp(), treetop);
   }
/*
Transform an Unsafe atomic call to diamonds with equivalent semantics

                          yes
isObjectNull [A] ------------------------------------------>
    |                                                      |
    | no                                                   |
    |                     yes                              |
isNotLowTagged [B] ---------------------------------------->
    |                                                      |
    |  no                                                  |
    |           no                                         |
isFinal [C] -------------------->                          |
    |                           |                          |
    | yes                       |                          |
jitReportFinalFieldModified [F] |                          |
    |                           |                          |
    +--------------------------->                          |
                                |                          |
                       calculate address [D]       calculate address
                        for static field           for instance field
                      (non collected reference)    and absolute address
                                |                 (collected reference)
                                |                          |
                  xcall atomic method helper [G]     xcall atomic method helper [E]
                                |                          |
                                +-----------+--------------+
                                            |
                                            v
                             program after the original call [H]

Block before the transformation: ===>

start Block_A
  ...
xcall Unsafe.xxx
  ...
  <load of unsafe call argX>
  ...
end Block_A

Blocks after the transformation: ===>

start Block_A
...
 astore  object-1
   <load of unsafe call argX>
...
ifacmpeq -> <Block_E>
  object-1
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
astore <object-2>
  aloadi ramStaticsFromClass
    aloadi  <classFromJavaLangClass>
      aload  <object-1>
lstore <offset>
  land
    lload <offset>
    lconst ~J9_SUN_FIELD_OFFSET_MASK
end Block_D

start Block_G
xcall atomic method helper
  aladd
    aload <object-2>
    lload <offset>
  xload value
xstore
  ==>xcall
goto --> block_H
end Block_G

start Block_E
xcall atomic method helper
  aladd
    aload <object-1>
    lload <offset>
  xload value
xstore
  ==>xcall
end Block_E

start Block_H
xreturn
  xload
end Block_H
...

start Block_F
call jitReportFinalFieldModified
  aloadi  <classFromJavaLangClass>
    aload <object-1>
go to <Block_D>
end Block_F

*/
void J9::RecognizedCallTransformer::processUnsafeAtomicCall(TR::TreeTop* treetop, TR::SymbolReferenceTable::CommonNonhelperSymbol helper, bool needsNullCheck)
   {
   bool enableTrace = trace();
   bool isNotStaticField = !strncmp(comp()->getCurrentMethod()->classNameChars(), "java/util/concurrent/atomic/", strlen("java/util/concurrent/atomic/"));
   bool fixupCommoning = true;
   TR::Node* unsafeCall = treetop->getNode()->getFirstChild();
   TR::Node* objectNode = unsafeCall->getChild(1);
   TR::Node* offsetNode = unsafeCall->getChild(2);
   TR::Node* address = NULL;

   TR::SymbolReference* newSymbolReference = NULL;
   TR::CFG*     cfg = comp()->getMethodSymbol()->getFlowGraph();
   TR::Node*    isObjectNullNode = NULL;
   TR::TreeTop* isObjectNullTreeTop = NULL;
   TR::Node*    isNotLowTaggedNode = NULL;
   TR::TreeTop* isNotLowTaggedTreeTop = NULL;

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
      address = comp()->target().is32Bit() ? TR::Node::create(TR::aiadd, 2, objectNode, TR::Node::create(TR::l2i, 1, offsetNode)) :
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
      isObjectNullNode = TR::Node::createif(TR::ifacmpeq, objectNode->duplicateTree(), TR::Node::aconst(0), NULL);
      isObjectNullTreeTop = TR::TreeTop::create(comp(), isObjectNullNode);
      treetop->insertBefore(isObjectNullTreeTop);
      treetop->getEnclosingBlock()->split(treetop, cfg, fixupCommoning);

      if (enableTrace)
         traceMsg(comp(), "Created isObjectNull test node n%dn, non-null object will fall through to Block_%d\n", isObjectNullNode->getGlobalIndex(), treetop->getEnclosingBlock()->getNumber());

      // Test if low tag is set
      isNotLowTaggedNode = TR::Node::createif(TR::iflcmpne,
                                               TR::Node::create(TR::land, 2, offsetNode->duplicateTree(), TR::Node::lconst(J9_SUN_STATIC_FIELD_OFFSET_TAG)),
                                               TR::Node::lconst(J9_SUN_STATIC_FIELD_OFFSET_TAG),
                                               NULL);
      isNotLowTaggedTreeTop = TR::TreeTop::create(comp(), isNotLowTaggedNode);
      treetop->insertBefore(isNotLowTaggedTreeTop);
      treetop->getEnclosingBlock()->split(treetop, cfg, fixupCommoning);

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

      // ramStatics is not collected. We cannot reuse objectNode->getSymbolReference()
      newSymbolReference = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Address);
      newSymbolReference->getSymbol()->setNotCollected();

      // Calculate static address
      auto objectAdjustmentNode = TR::Node::createWithSymRef(TR::astore, 1, 1,
                                                             TR::Node::createWithSymRef(TR::aloadi, 1, 1,
                                                                                        TR::Node::createWithSymRef(TR::aloadi, 1, 1,
                                                                                                                   objectNode->duplicateTree(),
                                                                                                                   comp()->getSymRefTab()->findOrCreateClassFromJavaLangClassSymbolRef()),
                                                                                        comp()->getSymRefTab()->findOrCreateRamStaticsFromClassSymbolRef()),
                                                             newSymbolReference);
      auto offsetAdjustmentNode = TR::Node::createWithSymRef(TR::lstore, 1, 1,
                                                             TR::Node::create(TR::land, 2,
                                                                              offsetNode->duplicateTree(),
                                                                              TR::Node::lconst(~J9_SUN_FIELD_OFFSET_MASK)),
                                                             offsetNode->getSymbolReference());

      if (enableTrace)
         traceMsg(comp(), "Created node objectAdjustmentNode n%dn #%d and offsetAdjustmentNode #%d n%dn to adjust object and offset for static field\n",
               objectAdjustmentNode->getGlobalIndex(), objectAdjustmentNode->getOpCode().hasSymbolReference() ? objectAdjustmentNode->getSymbolReference()->getReferenceNumber() : -1,
               offsetAdjustmentNode->getGlobalIndex(), offsetAdjustmentNode->getOpCode().hasSymbolReference() ? offsetAdjustmentNode->getSymbolReference()->getReferenceNumber() : -1);

      treetop->insertBefore(TR::TreeTop::create(comp(), objectAdjustmentNode));
      treetop->insertBefore(TR::TreeTop::create(comp(), offsetAdjustmentNode));
      treetop->getEnclosingBlock()->split(treetop, cfg, fixupCommoning);

      address = comp()->target().is32Bit() ? TR::Node::create(TR::aiadd, 2, objectNode->duplicateTree(), TR::Node::create(TR::l2i, 1, offsetNode->duplicateTree())) :
                                              TR::Node::create(TR::aladd, 2, objectNode->duplicateTree(), offsetNode->duplicateTree());
      }

   // Transmute the unsafe call to a call to atomic method helper
   unsafeCall->getChild(0)->recursivelyDecReferenceCount(); // replace unsafe object with the address
   unsafeCall->setAndIncChild(0, address);
   unsafeCall->removeChild(2); // remove offset node
   unsafeCall->removeChild(1); // remove object node
   unsafeCall->setSymbolReference(comp()->getSymRefTab()->findOrCreateCodeGenInlinedHelper(helper));

   if (!isNotStaticField)
      {
      // Split so that the return value from the atomic method helper call will be stored into a temp if required
      TR::TreeTop *nextTreeTop = treetop->getNextTreeTop();
      TR::Block *returnBlock = nextTreeTop->getEnclosingBlock()->split(nextTreeTop, cfg, fixupCommoning);

      // Check if the return value from the atomic method helper call is stored into a temp
      nextTreeTop = treetop->getNextTreeTop();
      TR::Node *storeReturnNode = NULL;
      if (nextTreeTop)
         {
         storeReturnNode = nextTreeTop->getNode();
         if (!storeReturnNode->getOpCode().isStore() || !(storeReturnNode->getFirstChild() == unsafeCall))
            {
            storeReturnNode = NULL;
            }
         else if (enableTrace)
            {
            traceMsg(comp(), "storeNode n%dn #%d for the return value of atomic method helper\n", storeReturnNode->getGlobalIndex(), storeReturnNode->getSymbolReference()->getReferenceNumber());
            }
         }

      /* Example of the atomic method helper
       * n92n  treetop
       * n93n    icall  <atomicFetchAndAdd>
       * n94n      aladd
       * n95n        aload  <temp slot 5>
       * n96n        lload  <temp slot 3>
       * n97n      iload  <temp slot 4>
       */
      // Create another helper call that loads from ramStatics
      TR::TreeTop *unsafeCallRamStaticsTT = treetop->duplicateTree();
      TR::Node *unsafeCallRamStaticsNode = unsafeCallRamStaticsTT->getNode()->getFirstChild();
      TR::Node *addressNode = unsafeCallRamStaticsNode->getFirstChild();
      TR::Node *loadNode = addressNode->getFirstChild();

      loadNode->setSymbolReference(newSymbolReference); // Use the same symRef as the objectAdjustmentNode
      treetop->insertBefore(unsafeCallRamStaticsTT);

      // Store the return value from the helper call that loads from ramStatics
      if (storeReturnNode)
         {
         TR::Node *storeNode = TR::Node::createStore(unsafeCall, storeReturnNode->getSymbolReference(), unsafeCallRamStaticsNode);
         treetop->insertBefore(TR::TreeTop::create(comp(), storeNode));
         }

      // Insert goto from the helper call that loads from ramStatics to the final return block
      TR::Node *gotoNode = TR::Node::create(unsafeCall, TR::Goto);
      gotoNode->setBranchDestination(returnBlock->getEntry());
      treetop->insertBefore(TR::TreeTop::create(comp(), gotoNode));

      if (enableTrace)
         traceMsg(comp(), "Created atomic method helper block_%d that loads from ramStatics treetop n%dn. returnBlock block_%d\n",
            unsafeCallRamStaticsTT->getEnclosingBlock()->getNumber(), unsafeCallRamStaticsTT->getNode()->getGlobalIndex(), returnBlock->getNumber());

      // Split the block that contains the original helper call into a separate block
      treetop->getEnclosingBlock()->split(treetop, cfg, fixupCommoning);

      // Setup CFG edges
      cfg->addEdge(unsafeCallRamStaticsTT->getEnclosingBlock(), returnBlock);

      if (enableTrace)
         traceMsg(comp(), "Block_%d contains call to atomic method helper, and is the target of isObjectNull and isNotLowTagged tests\n", treetop->getEnclosingBlock()->getNumber());

      isObjectNullNode->setBranchDestination(treetop->getEnclosingBlock()->getEntry());
      cfg->addEdge(TR::CFGEdge::createEdge(isObjectNullTreeTop->getEnclosingBlock(), treetop->getEnclosingBlock(), comp()->trMemory()));
      isNotLowTaggedNode->setBranchDestination(treetop->getEnclosingBlock()->getEntry());
      cfg->addEdge(TR::CFGEdge::createEdge(isNotLowTaggedTreeTop->getEnclosingBlock(), treetop->getEnclosingBlock(), comp()->trMemory()));

      cfg->removeEdge(unsafeCallRamStaticsTT->getEnclosingBlock(), treetop->getEnclosingBlock());
      }
   }

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)

/** \brief
 *    Constructs signature strings for constructing methods representing computed calls
 *    from the signature of the original INL calls. Adapted from getSignatureForLinkToStatic
 *    in j9method.cpp
 *
 *  \param extraParamsBefore
 *    the params to be prepended to the original signature
 *
 *  \param extraParamsAfter
 *    the params to be appended to the original signature
 *
 *  \param comp
 *    the current compilation
 *
 *  \param origSignature
 *    the original signautre
 *
 *  \param signatureLength
 *    the constructed signature length
 *
 *  \return
 *    static char * the constructed signature
 */

static char *
getSignatureForComputedCall(
   const char * const extraParamsBefore,
   const char * const extraParamsAfter,
   TR::Compilation* comp,
   const char * const origSignature,
   int32_t &signatureLength)
   {
   const size_t extraParamsLength = strlen(extraParamsBefore) + strlen(extraParamsAfter);

   const int origSignatureLength = strlen(origSignature);
   signatureLength = origSignatureLength + extraParamsLength;

   const int32_t signatureAllocSize = signatureLength + 28; // +1 for NULL terminator
   char * computedCallSignature =
      (char *)comp->trMemory()->allocateMemory(signatureAllocSize, heapAlloc);

   // Can't simply strchr() for ')' because that can appear within argument
   // types, in particular within class names. It's necessary to parse the
   // signature.
   const char * const paramsStart = origSignature + 1;
   const char * paramsEnd = paramsStart;
   const char * previousParam = paramsEnd;
   const char * returnType = NULL;
   while (*paramsEnd != ')')
      {
      paramsEnd = nextSignatureArgument(paramsEnd);
      if (!strncmp(paramsEnd, "Ljava/lang/invoke/MemberName;", 29)) // MemberName object must be discarded
         {
         returnType = nextSignatureArgument(paramsEnd) + 1;
         break;
         }
      }

   if (!returnType) returnType = paramsEnd + 1;
   const char * const returnTypeEnd = nextSignatureArgument(returnType);

   // Put together the new signature.
   TR::snprintfNoTrunc(
      computedCallSignature,
      signatureAllocSize,
      "(%s%.*s%s)%.*s",
      extraParamsBefore,
      (int)(paramsEnd - paramsStart),
      paramsStart,
      extraParamsAfter,
      (int)(returnTypeEnd - returnType),
      returnType);

   return computedCallSignature;
   }

void J9::RecognizedCallTransformer::process_java_lang_invoke_MethodHandle_invokeBasic(TR::TreeTop * treetop, TR::Node* node)
   {
   TR_J9VMBase* fej9 = static_cast<TR_J9VMBase*>(comp()->fe());
   TR::TransformUtil::separateNullCheck(comp(), treetop, trace());
   TR::Node * inlCallNode = node->duplicateTree(false);
   TR::list<TR::SymbolReference *>* argsList = new (comp()->trStackMemory()) TR::list<TR::SymbolReference*>(getTypedAllocator<TR::SymbolReference*>(comp()->allocator()));
   for (int i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node * currentChild = node->getChild(i);
      TR::SymbolReference *newSymbolReference = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), currentChild->getDataType());
      argsList->push_back(newSymbolReference);
      TR::Node * storeNode = TR::Node::createStore(node, newSymbolReference, currentChild);
      treetop->insertBefore(TR::TreeTop::create(comp(),storeNode));
      inlCallNode->setAndIncChild(i, TR::Node::createLoad(node, newSymbolReference));
      currentChild->recursivelyDecReferenceCount();
      }

   TR::Node* mhNode = TR::Node::createLoad(node, argsList->front()); // the first arg of invokeBasic call is the receiver MethodHandle object
   // load MethodHandle.form, which is the LambdaForm object
   uint32_t offset = fej9->getInstanceFieldOffsetIncludingHeader("Ljava/lang/invoke/MethodHandle;", "form", "Ljava/lang/invoke/LambdaForm;", comp()->getCurrentMethod());
   TR::SymbolReference * lambdaFormSymRef = comp()->getSymRefTab()->findOrFabricateShadowSymbol(comp()->getMethodSymbol(),
                                                                                                TR::Symbol::Java_lang_invoke_MethodHandle_form,
                                                                                                TR::Address,
                                                                                                offset,
                                                                                                false,
                                                                                                false,
                                                                                                true,
                                                                                                "java/lang/invoke/MethodHandle.form Ljava/lang/invoke/LambdaForm;");
   TR::Node * lambdaFormNode = TR::Node::createWithSymRef(node, comp()->il.opCodeForIndirectLoad(TR::Address), 1 , mhNode, lambdaFormSymRef);
   lambdaFormNode->setIsNonNull(true);
   // load from lambdaForm.vmEntry, which is the MemberName object
   offset = fej9->getInstanceFieldOffsetIncludingHeader("Ljava/lang/invoke/LambdaForm;", "vmentry", "Ljava/lang/invoke/MemberName;", comp()->getCurrentMethod());
   TR::SymbolReference * memberNameSymRef = comp()->getSymRefTab()->findOrFabricateShadowSymbol(comp()->getMethodSymbol(),
                                                                                                TR::Symbol::Java_lang_invoke_LambdaForm_vmentry,
                                                                                                TR::Address,
                                                                                                offset,
                                                                                                false,
                                                                                                false,
                                                                                                true,
                                                                                                "java/lang/invoke/LambdaForm.vmentry Ljava/lang/invoke/MemberName;");
   TR::Node * memberNameNode = TR::Node::createWithSymRef(node, comp()->il.opCodeForIndirectLoad(TR::Address), 1, lambdaFormNode, memberNameSymRef);
   // load from membername.vmTarget, which is the J9Method
   TR::SymbolReference * vmTargetSymRef = comp()->getSymRefTab()->findOrFabricateMemberNameVmTargetShadow();
   TR::Node * vmTargetNode = TR::Node::createWithSymRef(node, TR::aloadi, 1, memberNameNode, vmTargetSymRef);
   processVMInternalNativeFunction(treetop, node, vmTargetNode, argsList, inlCallNode);
   }

void J9::RecognizedCallTransformer::process_java_lang_invoke_MethodHandle_linkToStaticSpecial(TR::TreeTop* treetop, TR::Node* node)
   {
   TR_J9VMBase* fej9 = static_cast<TR_J9VMBase*>(comp()->fe());
   TR::TransformUtil::separateNullCheck(comp(), treetop, trace());
   TR::Node * inlCallNode = node->duplicateTree(false);
   TR::list<TR::SymbolReference *>* argsList = new (comp()->trStackMemory()) TR::list<TR::SymbolReference*>(getTypedAllocator<TR::SymbolReference*>(comp()->allocator()));
   for (int i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node * currentChild = node->getChild(i);
      TR::SymbolReference *newSymbolReference = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), currentChild->getDataType());
      argsList->push_back(newSymbolReference);
      TR::Node * storeNode = TR::Node::createStore(node, newSymbolReference, currentChild);
      treetop->insertBefore(TR::TreeTop::create(comp(),storeNode));
      inlCallNode->setAndIncChild(i, TR::Node::createLoad(node, newSymbolReference));
      currentChild->recursivelyDecReferenceCount();
      }
   // load from membername.vmTarget, which is the J9Method
   TR::Node* mnNode = TR::Node::createLoad(node, argsList->back());
   TR::SymbolReference * vmTargetSymRef = comp()->getSymRefTab()->findOrFabricateMemberNameVmTargetShadow();
   TR::Node * vmTargetNode = TR::Node::createWithSymRef(node, TR::aloadi, 1, mnNode, vmTargetSymRef);
   vmTargetNode->setIsNonNull(true);
   argsList->pop_back(); // MemberName is not required when dispatching directly to the jitted method address
   processVMInternalNativeFunction(treetop, node, vmTargetNode, argsList, inlCallNode);
   }

void J9::RecognizedCallTransformer::processVMInternalNativeFunction(TR::TreeTop* treetop, TR::Node* node, TR::Node* vmTargetNode, TR::list<TR::SymbolReference *>* argsList, TR::Node* inlCallNode)
   {
   TR::SymbolReference *extraField = comp()->getSymRefTab()->findOrCreateJ9MethodExtraFieldSymbolRef(offsetof(struct J9Method, extra));
   TR::Node *extra = TR::Node::createWithSymRef(node, comp()->il.opCodeForIndirectLoad(extraField->getSymbol()->getDataType()), 1, vmTargetNode, extraField);
   TR::SymbolReference *extraTempSlotSymRef = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(),extraField->getSymbol()->getDataType());
   treetop->insertBefore(TR::TreeTop::create(comp(), TR::Node::createStore(node, extraTempSlotSymRef, extra)));

   TR::ILOpCodes xcmpne = comp()->target().is64Bit()? TR::iflcmpne : TR::ificmpne;
   TR::ILOpCodes xand   = comp()->target().is64Bit()? TR::land   : TR::iand;
   TR::Node *zero = comp()->target().is64Bit()? TR::Node::lconst(node, 0) : TR::Node::iconst(node, 0);
   TR::Node *mask = comp()->target().is64Bit()? TR::Node::lconst(node, J9_STARTPC_NOT_TRANSLATED) : TR::Node::iconst(node, J9_STARTPC_NOT_TRANSLATED);

   TR::Node* isCompiledNode = TR::Node::createif(xcmpne,
         TR::Node::create(xand, 2, TR::Node::createLoad(node, extraTempSlotSymRef), mask),
         zero,
         NULL);
   isCompiledNode->copyByteCodeInfo(node);
   TR::TreeTop * cmpCheckTreeTop = TR::TreeTop::create(self()->comp(), isCompiledNode);

   TR::Node *jitAddress;
   if (comp()->target().cpu.isI386())
      {
      jitAddress = TR::Node::create(TR::i2l, 1, TR::Node::createLoad(node, extraTempSlotSymRef));
      }
   else
      {
      TR::SymbolReference *linkageInfoSymRef = comp()->getSymRefTab()->findOrCreateStartPCLinkageInfoSymbolRef(-4);
      TR::ILOpCodes x2a = comp()->target().is64Bit()? TR::l2a : TR::i2a;
      TR::Node *linkageInfo    = TR::Node::createWithSymRef(TR::iloadi, 1, 1, TR::Node::create(x2a, 1, TR::Node::createLoad(node, extraTempSlotSymRef)), linkageInfoSymRef);
      TR::Node *jitEntryOffset = TR::Node::create(TR::ishr,   2, linkageInfo, TR::Node::iconst(node, 16));
      if (comp()->target().is64Bit())
         jitAddress = TR::Node::create(TR::ladd, 2, TR::Node::createLoad(node, extraTempSlotSymRef), TR::Node::create(TR::i2l, 1, jitEntryOffset));
      else
         jitAddress = TR::Node::create(TR::iadd, 2, TR::Node::createLoad(node, extraTempSlotSymRef), jitEntryOffset);
      }
      TR_J9VMBase* fej9 = static_cast<TR_J9VMBase*>(comp()->fe());
      TR_OpaqueMethodBlock *dummyInvoke = fej9->getMethodFromName("com/ibm/jit/JITHelpers", "dispatchComputedStaticCall", "()V");
      int signatureLength;
      char * signature = getSignatureForComputedCall(
         "J",
         "",
         comp(),
         node->getSymbol()->castToMethodSymbol()->getMethod()->signatureChars(),
         signatureLength);

   // construct a dummy resolved method
   TR::ResolvedMethodSymbol * owningMethodSymbol = node->getSymbolReference()->getOwningMethodSymbol(comp());
   TR_ResolvedMethod * dummyMethod = fej9->createResolvedMethodWithSignature(comp()->trMemory(),dummyInvoke, NULL, signature,signatureLength, owningMethodSymbol->getResolvedMethod());
   TR::SymbolReference * computedCallSymbol = comp()->getSymRefTab()->findOrCreateMethodSymbol(owningMethodSymbol->getResolvedMethodIndex(), -1, dummyMethod, TR::MethodSymbol::ComputedStatic);

   uint32_t numComputedCallArgs = argsList->size() + 1;
   TR::Node * computedCallNode = TR::Node::createWithSymRef(node, node->getSymbol()->castToMethodSymbol()->getMethod()->indirectCallOpCode(),numComputedCallArgs, computedCallSymbol);
   computedCallNode->setAndIncChild(0,jitAddress); // first arguments of computed calls is the method address to jump to
   int32_t child_i = 1;
   for (auto symRefIt = argsList->begin(); symRefIt != argsList->end(); ++symRefIt)
      {
      TR::SymbolReference * currentArg = *symRefIt;
      computedCallNode->setAndIncChild(child_i, TR::Node::createLoad(node, currentArg));
      child_i++;
      }
   TR::TreeTop * computedCallTreeTop = TR::TreeTop::create(comp(), TR::Node::create(node, TR::treetop, 1, computedCallNode));
   TR::RecognizedMethod rm = node->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod();
   TR::Node *nullChkNode = NULL;
   if (rm == TR::java_lang_invoke_MethodHandle_linkToSpecial)
      {
      TR::Node *passthroughNode = TR::Node::create(node, TR::PassThrough, 1);
      passthroughNode->setAndIncChild(0, TR::Node::createLoad(node, argsList->front()));
      TR::SymbolReference *symRef = comp()->getSymRefTab()->findOrCreateNullCheckSymbolRef(comp()->getMethodSymbol());
      nullChkNode = TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, passthroughNode, symRef);
      }

   TR::TreeTop * inlCallTreeTop = TR::TreeTop::create(comp(), TR::Node::create(node, TR::treetop, 1, inlCallNode)); // original treetop gets deleted by createDiamondForCall
   TR::TransformUtil::createDiamondForCall(this, treetop, cmpCheckTreeTop, inlCallTreeTop, computedCallTreeTop, false, false);
   if (nullChkNode) computedCallTreeTop->insertBefore(TR::TreeTop::create(comp(), nullChkNode));
   _processedINLCalls->set(inlCallNode->getGlobalIndex());
   }

void J9::RecognizedCallTransformer::process_java_lang_invoke_MethodHandle_linkToVirtual(TR::TreeTop * treetop, TR::Node * node)
   {
   TR::Node *receiver = node->getChild(0);
   TR::Node *memberNameNode = node->getChild(node->getNumChildren() - 1);

   TR::SymbolReference *vftSymRef = comp()->getSymRefTab()->findOrCreateVftSymbolRef();
   TR::Node *vftNode =
      TR::Node::createWithSymRef(node, TR::aloadi, 1, receiver, vftSymRef);

   // Null check the receiver

   TR::SymbolReference *nullCheckSymRef =
      comp()->getSymRefTab()->findOrCreateNullCheckSymbolRef(
         comp()->getMethodSymbol());

   TR::Node *nullCheck = TR::Node::createWithSymRef(
      node, TR::NULLCHK, 1, vftNode, nullCheckSymRef);

   treetop->insertBefore(TR::TreeTop::create(comp(), nullCheck));

   // Get the VFT offset from MemberName.vmindex
   TR::SymbolReference* vmIndexSymRef = comp()->getSymRefTab()->findOrFabricateShadowSymbol(
      comp()->getMethodSymbol(),
      TR::Symbol::Java_lang_invoke_MemberName_vmindex,
      TR::Int64,
      comp()->fej9()->getVMIndexOffset(),
      false,
      false,
      true,
      "java/lang/invoke/MemberName.vmindex J");

   TR::Node *vftOffset =
      TR::Node::createWithSymRef(node, TR::aloadi, 1, memberNameNode, vmIndexSymRef);

   if (!comp()->target().is64Bit())
      vftOffset = TR::Node::create(node, TR::l2i, 1, vftOffset);

   makeIntoDispatchVirtualCall(node, vftOffset, vftNode, memberNameNode);
   }

void J9::RecognizedCallTransformer::makeIntoDispatchVirtualCall(
   TR::Node *node, TR::Node *vftOffset, TR::Node *vftNode, TR::Node *memberNameNode)
   {
   // Construct a dummy "resolved method" for JITHelpers.dispatchVirtual()V to
   // dispatch through the VFT.
   TR_J9VMBase *fej9 = comp()->fej9();
   TR_OpaqueMethodBlock *dispatchVirtual =
      fej9->getMethodFromName("com/ibm/jit/JITHelpers", "dispatchVirtual", "()V");

   int signatureLength;
   char *signature = getSignatureForComputedCall(
      "JJ",
      "",
      comp(),
      node->getSymbol()->castToMethodSymbol()->getMethod()->signatureChars(),
      signatureLength);

   TR::ResolvedMethodSymbol *owningMethodSymbol =
      node->getSymbolReference()->getOwningMethodSymbol(comp());

   TR_ResolvedMethod *dispatchVirtualResolvedMethod =
      fej9->createResolvedMethodWithSignature(
         comp()->trMemory(),
         dispatchVirtual,
         NULL,
         signature,
         signatureLength,
         owningMethodSymbol->getResolvedMethod());

   TR::SymbolReference * dispatchVirtualSymRef =
      comp()->getSymRefTab()->findOrCreateMethodSymbol(
         owningMethodSymbol->getResolvedMethodIndex(),
         -1,
         dispatchVirtualResolvedMethod,
         TR::MethodSymbol::ComputedStatic);

   TR::ILOpCodes indirectCallOp =
      node->getSymbol()->castToMethodSymbol()->getMethod()->indirectCallOpCode();

   TR::Node::recreateWithSymRef(node, indirectCallOp, dispatchVirtualSymRef);

   // 2 extra args prepended (address in vtable entry and vtable slot index),
   // and last arg (MemberName object) removed, so net 1 extra child.
   TR::Node *dummyExtraChild = NULL;
   node->addChildren(&dummyExtraChild, 1);
   for (int32_t i = node->getNumChildren() - 1; i >= 2; i--)
      node->setChild(i, node->getChild(i - 2));

   TR::Node *xconstSizeofJ9Class = comp()->target().is64Bit()
      ? TR::Node::lconst(node, sizeof(J9Class))
      : TR::Node::iconst(node, sizeof(J9Class));

   TR::ILOpCodes subOp = comp()->target().is64Bit() ? TR::lsub : TR::isub;
   TR::ILOpCodes axadd = comp()->target().is64Bit() ? TR::aladd : TR::aiadd;

   TR::SymbolReference *genericIntShadow =
      comp()->getSymRefTab()->createGenericIntShadowSymbolReference(0);

   genericIntShadow->getSymbol()->setNotCollected();

   TR::Node *jitVftOffset = TR::Node::create(subOp, 2, xconstSizeofJ9Class, vftOffset);
   TR::Node *jitVftSlotPtr = TR::Node::create(axadd, 2, vftNode, jitVftOffset);

   TR::ILOpCodes vftEntryLoadOp = comp()->target().is64Bit() ? TR::lloadi : TR::iloadi;
   TR::Node *jittedMethodEntryPoint =
      TR::Node::createWithSymRef(vftEntryLoadOp, 1, 1, jitVftSlotPtr, genericIntShadow);

   node->setAndIncChild(0, jittedMethodEntryPoint);
   node->setAndIncChild(1, jitVftOffset);

   memberNameNode->decReferenceCount(); // no longer a child of node
   }

void J9::RecognizedCallTransformer::process_java_lang_invoke_MethodHandle_linkToInterface(TR::TreeTop *treetop, TR::Node *node)
   {
   TR::Node *receiverNode = node->getChild(0);
   TR::Node *memberNameNode = node->getChild(node->getNumChildren() - 1);

   // NOTE: There's no need for a null check, since null won't get past the
   // type check in DirectMethodHandle$Interface.checkReceiver() anyway.

   // Call a VM helper to do the interface dispatch. It finds the declaring
   // interface and the iTable index from the MemberName. The helper throws
   // IllegalAccessError when the found method is non-public. It won't throw
   // ICCE because the bytecode has already done a type check on the receiver,
   // so we'll always find an itable.

   // This produces a VFT offset (to the interpreter VFT slot from the start of
   // the J9Class) that we can use to call through the vtable.

   TR::SymbolReference *lookupDynamicInterfaceMethod =
      comp()->getSymRefTab()->findOrCreateLookupDynamicPublicInterfaceMethodSymbolRef();

   TR::Node *vftOffset = TR::Node::createWithSymRef(
      node,
      comp()->target().is64Bit() ? TR::lcall : TR::icall,
      2,
      lookupDynamicInterfaceMethod);

   // The helper takes (receiver J9Class, MemberName reference), but helper
   // arguments are passed in reverse order.
   vftOffset->setAndIncChild(0, memberNameNode);

   TR::SymbolReference *vftSymRef = comp()->getSymRefTab()->findOrCreateVftSymbolRef();
   TR::Node *vftNode =
      TR::Node::createWithSymRef(node, TR::aloadi, 1, receiverNode, vftSymRef);

   vftOffset->setAndIncChild(1, vftNode);

   treetop->insertBefore(
      TR::TreeTop::create(
         comp(),
         TR::Node::create(node, TR::treetop, 1, vftOffset)));

   makeIntoDispatchVirtualCall(node, vftOffset, vftNode, memberNameNode);
   }
#endif // J9VM_OPT_OPENJDK_METHODHANDLE

bool J9::RecognizedCallTransformer::isInlineable(TR::TreeTop* treetop)
   {
   auto node = treetop->getNode()->getFirstChild();
   TR::RecognizedMethod rm =
      node->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod();

   bool isILGenPass = !getLastRun();
   if (isILGenPass)
      {
      switch(rm)
         {
         case TR::sun_misc_Unsafe_getAndAddInt:
            return !comp()->getOption(TR_DisableUnsafe) && !comp()->compileRelocatableCode() && !TR::Compiler->om.canGenerateArraylets() &&
               cg()->supportsNonHelper(TR::SymbolReferenceTable::atomicFetchAndAddSymbol);
         case TR::sun_misc_Unsafe_getAndSetInt:
            return !comp()->getOption(TR_DisableUnsafe) && !comp()->compileRelocatableCode() && !TR::Compiler->om.canGenerateArraylets() &&
               cg()->supportsNonHelper(TR::SymbolReferenceTable::atomicSwapSymbol);
         case TR::sun_misc_Unsafe_getAndAddLong:
            return !comp()->getOption(TR_DisableUnsafe) && !comp()->compileRelocatableCode() && !TR::Compiler->om.canGenerateArraylets() && comp()->target().is64Bit() &&
               cg()->supportsNonHelper(TR::SymbolReferenceTable::atomicFetchAndAddSymbol);
         case TR::sun_misc_Unsafe_getAndSetLong:
            return !comp()->getOption(TR_DisableUnsafe) && !comp()->compileRelocatableCode() && !TR::Compiler->om.canGenerateArraylets() && comp()->target().is64Bit() &&
               cg()->supportsNonHelper(TR::SymbolReferenceTable::atomicSwapSymbol);
         case TR::java_lang_Class_isAssignableFrom:
            return cg()->supportsInliningOfIsAssignableFrom();
         case TR::java_lang_Class_cast:
            {
            static const bool disable =
               feGetEnv("TR_disableClassCastToCheckcast") != NULL;

            if (disable)
               return false;

            // Do not transform in involuntary OSR, since the resulting
            // checkcast will still be an OSR point, but it won't be the call
            // that is expected to correspond to the bytecode instruction. (It
            // might turn out that there's a safe way to transform anyway, but
            // it isn't obvious.)
            return comp()->getOSRMode() != TR::involuntaryOSR;
            }
         case TR::java_lang_Integer_rotateLeft:
         case TR::java_lang_Integer_rotateRight:
            return comp()->target().cpu.getSupportsHardware32bitRotate();
         case TR::java_lang_Long_rotateLeft:
         case TR::java_lang_Long_rotateRight:
            return comp()->target().cpu.getSupportsHardware64bitRotate();
         case TR::java_lang_Math_abs_I:
         case TR::java_lang_Math_abs_L:
            return cg()->supportsIntAbs();
         case TR::java_lang_Math_abs_F:
         case TR::java_lang_Math_abs_D:
            return cg()->supportsFPAbs();
         case TR::java_lang_Math_max_I:
         case TR::java_lang_Math_min_I:
         case TR::java_lang_Math_max_L:
         case TR::java_lang_Math_min_L:
            return !comp()->getOption(TR_DisableMaxMinOptimization);
         case TR::java_lang_Math_multiplyHigh:
            return cg()->getSupportsLMulHigh();
         case TR::java_lang_StringUTF16_toBytes:
            return !comp()->compileRelocatableCode();
         case TR::java_lang_StrictMath_sqrt:
         case TR::java_lang_Math_sqrt:
            return comp()->target().cpu.getSupportsHardwareSQRT();
         case TR::java_lang_Short_reverseBytes:
         case TR::java_lang_Integer_reverseBytes:
         case TR::java_lang_Long_reverseBytes:
            return comp()->cg()->supportsByteswap();
         case TR::java_lang_StringCoding_encodeASCII:
         case TR::java_lang_String_encodeASCII:
            return comp()->cg()->getSupportsInlineEncodeASCII();
         default:
            return false;
         }
      }
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
   else
      {
      // Post-inlining pass
      switch (rm)
         {
         case TR::java_lang_invoke_MethodHandle_invokeBasic:
            if (_processedINLCalls->get(node->getGlobalIndex()))
               return false;
            else
               return true;
         case TR::java_lang_invoke_MethodHandle_linkToStatic:
         case TR::java_lang_invoke_MethodHandle_linkToSpecial:
         // linkToStatic calls are also used for unresolved invokedynamic/invokehandle, which we can not
         // bypass as we may push null appendix object that we can not check at compile time
            if (_processedINLCalls->get(node->getGlobalIndex()) || node->getSymbolReference()->getSymbol()->isDummyResolvedMethod())
               return false;
            else
               return true;
         case TR::java_lang_invoke_MethodHandle_linkToVirtual:
         case TR::java_lang_invoke_MethodHandle_linkToInterface:
            return true;
         default:
            return false;
         }
      }
#else
   return false;
#endif
   }

void J9::RecognizedCallTransformer::transform(TR::TreeTop* treetop)
   {
   auto node = treetop->getNode()->getFirstChild();
   TR::RecognizedMethod rm =
      node->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod();

   bool isILGenPass = !getLastRun();
   if (isILGenPass)
      {
      switch(rm)
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
         case TR::java_lang_Class_cast:
            process_java_lang_Class_cast(treetop, node);
            break;
         case TR::java_lang_Integer_rotateLeft:
            processIntrinsicFunction(treetop, node, TR::irol);
            break;
         case TR::java_lang_Integer_rotateRight:
            {
            // rotateRight(x, distance) = rotateLeft(x, -distance)
            TR::Node *distance = TR::Node::create(node, TR::ineg, 1);
            distance->setChild(0, node->getSecondChild());
            node->setAndIncChild(1, distance);

            processIntrinsicFunction(treetop, node, TR::irol);

            break;
            }
         case TR::java_lang_Long_rotateLeft:
            processIntrinsicFunction(treetop, node, TR::lrol);
            break;
         case TR::java_lang_Long_rotateRight:
            {
            // rotateRight(x, distance) = rotateLeft(x, -distance)
            TR::Node *distance = TR::Node::create(node, TR::ineg, 1);
            distance->setChild(0, node->getSecondChild());
            node->setAndIncChild(1, distance);

            processIntrinsicFunction(treetop, node, TR::lrol);

            break;
            }
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
         case TR::java_lang_Math_multiplyHigh:
            processIntrinsicFunction(treetop, node, TR::lmulh);
            break;
         case TR::java_lang_StringUTF16_toBytes:
            process_java_lang_StringUTF16_toBytes(treetop, node);
            break;
         case TR::java_lang_StringCoding_encodeASCII:
         case TR::java_lang_String_encodeASCII:
            process_java_lang_StringCoding_encodeASCII(treetop, node);
            break;
         case TR::java_lang_StrictMath_sqrt:
         case TR::java_lang_Math_sqrt:
            process_java_lang_StrictMath_and_Math_sqrt(treetop, node);
            break;
         case TR::java_lang_Short_reverseBytes:
            processConvertingUnaryIntrinsicFunction(treetop, node, TR::i2s, TR::sbyteswap, TR::s2i);
            break;
         case TR::java_lang_Integer_reverseBytes:
            processIntrinsicFunction(treetop, node, TR::ibyteswap);
            break;
         case TR::java_lang_Long_reverseBytes:
            processIntrinsicFunction(treetop, node, TR::lbyteswap);
            break;
         default:
            break;
         }
      }
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
   else
      {
      // Post-inlining pass
      switch (rm)
         {
         case TR::java_lang_invoke_MethodHandle_invokeBasic:
            process_java_lang_invoke_MethodHandle_invokeBasic(treetop, node);
            break;
         case TR::java_lang_invoke_MethodHandle_linkToStatic:
         case TR::java_lang_invoke_MethodHandle_linkToSpecial:
            process_java_lang_invoke_MethodHandle_linkToStaticSpecial(treetop, node);
            break;
         case TR::java_lang_invoke_MethodHandle_linkToVirtual:
            process_java_lang_invoke_MethodHandle_linkToVirtual(treetop, node);
            break;
         case TR::java_lang_invoke_MethodHandle_linkToInterface:
            process_java_lang_invoke_MethodHandle_linkToInterface(treetop, node);
            break;
         default:
            break;
         }
      }
#endif
   }
