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
#include "optimizer/ValuePropagation.hpp"
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

   TR::Node *newInputNode = TR::TransformUtil::generateFirstArrayElementAddressTrees(comp(), sourceArrayNode);
   TR::Node *newOutputNode = TR::TransformUtil::generateFirstArrayElementAddressTrees(comp(), destinationArrayNode);

   encodeASCIINode->setAndIncChild(0, newInputNode);
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

void J9::RecognizedCallTransformer::process_java_lang_StringLatin1_inflate_BIBII(TR::TreeTop *treetop, TR::Node *node)
   {
   /*
    * Replace the call with the following tree (+ boundary checks)
    *
    * treetop
    *   arraytranslate (TROTNoBreak)
    *     aladd
    *       srcObj
    *       ladd
    *         srcOff
    *         hdrSize
    *     aladd
    *       dstObj
    *       ladd
    *         lmul
    *           dstOff
    *           lconst 2
    *         hdrSize
    *     iconst 0 (dummy: table node)
    *     iconst 0xffff (term char node)
    *     length
    *     iconst -1 (dummy: stop index node)
    */
   static bool verboseLatin1inflate = (feGetEnv("TR_verboseLatin1inflate") != NULL);
   if (verboseLatin1inflate)
      {
      fprintf(stderr, "Recognize StringLatin1.inflate([BI[BII)V: %s @ %s\n",
         comp()->signature(),
         comp()->getHotnessName(comp()->getMethodHotness()));
      }

   TR_ASSERT_FATAL(comp()->cg()->getSupportsArrayTranslateTROTNoBreak(), "Support for arraytranslateTROTNoBreak is required");

   // Anchor a copy of the call node just before treetop so that all of the
   // children will be commoned across the split point, and all of the temps
   // will be initialized before the first opportunity to go to the fallback
   // path. Otherwise, the fallback path could end up using temps that are
   // sometimes uninitialized. This copy will be removed just after splitting.
   TR::TreeTop *callCopyTT = TR::TreeTop::create(
      comp(), TR::Node::create(node, TR::treetop, 1, node->duplicateTree(false)));

   treetop->insertBefore(callCopyTT);

   bool is64BitTarget = comp()->target().is64Bit();

   TR::Node *srcObj = node->getChild(0);
   TR::Node *srcOff = node->getChild(1);
   TR::Node *dstObj = node->getChild(2);
   TR::Node *dstOff = node->getChild(3);
   TR::Node *length = node->getChild(4);

   TR::Node *arrayTranslateNode = TR::Node::create(node, TR::arraytranslate, 6);
   arrayTranslateNode->setSourceIsByteArrayTranslate(true);
   arrayTranslateNode->setTargetIsByteArrayTranslate(false);
   arrayTranslateNode->setTermCharNodeIsHint(false);
   arrayTranslateNode->setSourceCellIsTermChar(false);
   arrayTranslateNode->setTableBackedByRawStorage(true);
   arrayTranslateNode->setSymbolReference(comp()->getSymRefTab()->findOrCreateArrayTranslateSymbol());

   TR::Node *tmpNode = TR::TransformUtil::generateConvertArrayElementIndexToOffsetTrees(comp(), srcOff, NULL, 1, false);
   TR::Node *srcAddr = TR::TransformUtil::generateArrayElementAddressTrees(comp(), srcObj, tmpNode);
   tmpNode = TR::TransformUtil::generateConvertArrayElementIndexToOffsetTrees(comp(), dstOff, NULL, 2, false);
   TR::Node *dstAddr = TR::TransformUtil::generateArrayElementAddressTrees(comp(), dstObj, tmpNode);

   TR::Node *termCharNode = TR::Node::create(node, TR::iconst, 0, 0xffff); // mask for ISO 8859-1 decoder
   TR::Node *tableNode = TR::Node::create(node, TR::iconst, 0, 0); // dummy table node
   TR::Node *stoppingNode = TR::Node::create(node, TR::iconst, 0, -1); // dummy stop index node

   arrayTranslateNode->setAndIncChild(0, srcAddr);
   arrayTranslateNode->setAndIncChild(1, dstAddr);
   arrayTranslateNode->setAndIncChild(2, tableNode);
   arrayTranslateNode->setAndIncChild(3, termCharNode);
   arrayTranslateNode->setAndIncChild(4, length);
   arrayTranslateNode->setAndIncChild(5, stoppingNode);

   TR::CFG *cfg = comp()->getFlowGraph();

   // if (length < 0) { call the original method }
   TR::Node *constZeroNode1 = TR::Node::create(node, TR::iconst, 0, 0);
   TR::Node *ifCmpNode1 = TR::Node::createif(TR::ificmplt, length, constZeroNode1);
   TR::TreeTop *ifCmpTreeTop1 = TR::TreeTop::create(comp(), treetop->getPrevTreeTop(), ifCmpNode1);
   // if (srcOff < 0) { call the original method }
   TR::Node *constZeroNode2 = TR::Node::create(node, TR::iconst, 0, 0);
   TR::Node *ifCmpNode2 = TR::Node::createif(TR::ificmplt, srcOff, constZeroNode2);
   TR::TreeTop *ifCmpTreeTop2 = TR::TreeTop::create(comp(), ifCmpTreeTop1, ifCmpNode2);
   // if (srcObj.length < srcOff + length) { call the original method }
   TR::Node *arrayLenNode1 = TR::Node::create(node, TR::arraylength, 1, srcObj);
   TR::Node *iaddNode1 = TR::Node::create(node, TR::iadd, 2, srcOff, length);
   TR::Node *ifCmpNode3 = TR::Node::createif(TR::ificmplt, arrayLenNode1, iaddNode1);
   TR::TreeTop *ifCmpTreeTop3 = TR::TreeTop::create(comp(), ifCmpTreeTop2, ifCmpNode3);
   // if (dstOff < 0) { call the original method }
   TR::Node *constZeroNode3 = TR::Node::create(node, TR::iconst, 0, 0);
   TR::Node *ifCmpNode4 = TR::Node::createif(TR::ificmplt, dstOff, constZeroNode3);
   TR::TreeTop *ifCmpTreeTop4 = TR::TreeTop::create(comp(), ifCmpTreeTop3, ifCmpNode4);
   // if ((dstObj.length >> 1) < dstOff + length) { call the original method }
   TR::Node *arrayLenNode2 = TR::Node::create(node, TR::arraylength, 1, dstObj);
   TR::Node *constOneNode = TR::Node::create(node, TR::iconst, 0, 1);
   TR::Node *ishrNode = TR::Node::create(node, TR::ishr, 2, arrayLenNode2, constOneNode);
   TR::Node *iaddNode2 = TR::Node::create(node, TR::iadd, 2, dstOff, length);
   TR::Node *ifCmpNode5 = TR::Node::createif(TR::ificmplt, ishrNode, iaddNode2);
   TR::TreeTop *ifCmpTreeTop5 = TR::TreeTop::create(comp(), ifCmpTreeTop4, ifCmpNode5);

   TR::TreeTop *arrayTranslateTreeTop = TR::TreeTop::create(comp(), ifCmpTreeTop5, arrayTranslateNode);

   TR::Block *ifCmpBlock1 = ifCmpTreeTop1->getEnclosingBlock();
   TR::Block *ifCmpBlock2 = ifCmpBlock1->split(ifCmpTreeTop2, cfg, true /* fixUpCommoning */, true /* copyExceptionSuccessors */);
   TR::Block *ifCmpBlock3 = ifCmpBlock2->split(ifCmpTreeTop3, cfg, true /* fixUpCommoning */, true /* copyExceptionSuccessors */);
   TR::Block *ifCmpBlock4 = ifCmpBlock3->split(ifCmpTreeTop4, cfg, true /* fixUpCommoning */, true /* copyExceptionSuccessors */);
   TR::Block *ifCmpBlock5 = ifCmpBlock4->split(ifCmpTreeTop5, cfg, true /* fixUpCommoning */, true /* copyExceptionSuccessors */);
   TR::Block *fallThroughPathBlock = ifCmpBlock5->split(arrayTranslateTreeTop, cfg, true /* fixUpCommoning */, true /* copyExceptionSuccessors */);
   // This block contains the original call node
   TR::Block *fallbackPathBlock = fallThroughPathBlock->split(treetop, cfg, true /* fixUpCommoning */, true /* copyExceptionSuccessors */);
   TR::Block *tailBlock = fallbackPathBlock->split(treetop->getNextTreeTop(), cfg, true /* fixUpCommoning */, true /* copyExceptionSuccessors */);

   TR::TransformUtil::removeTree(comp(), callCopyTT);

   // Go to the tail block from the fall-through block
   TR::Node *gotoNode = TR::Node::create(node, TR::Goto);
   TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode, NULL, NULL);
   gotoNode->setBranchDestination(tailBlock->getEntry());
   fallThroughPathBlock->getExit()->insertBefore(gotoTree);

   // Set the ificmp blocks' destinations to the fallback block and update the CFG
   ifCmpNode1->setBranchDestination(fallbackPathBlock->getEntry());
   cfg->addEdge(ifCmpBlock1, fallbackPathBlock);
   ifCmpNode2->setBranchDestination(fallbackPathBlock->getEntry());
   cfg->addEdge(ifCmpBlock2, fallbackPathBlock);
   ifCmpNode3->setBranchDestination(fallbackPathBlock->getEntry());
   cfg->addEdge(ifCmpBlock3, fallbackPathBlock);
   ifCmpNode4->setBranchDestination(fallbackPathBlock->getEntry());
   cfg->addEdge(ifCmpBlock4, fallbackPathBlock);
   ifCmpNode5->setBranchDestination(fallbackPathBlock->getEntry());
   cfg->addEdge(ifCmpBlock5, fallbackPathBlock);
   cfg->addEdge(fallThroughPathBlock, tailBlock);
   cfg->removeEdge(fallThroughPathBlock, fallbackPathBlock);

   // The original call to StringLatin1.inflate([BI[BII)V will only be used
   // if an exception needs to be thrown.  Mark it as cold.
   fallbackPathBlock->setFrequency(UNKNOWN_COLD_BLOCK_COUNT);
   fallbackPathBlock->setIsCold();
   }

void J9::RecognizedCallTransformer::process_java_lang_StringUTF16_toBytes(TR::TreeTop* treetop, TR::Node* node)
   {
   TR_J9VMBase* fej9 = static_cast<TR_J9VMBase*>(comp()->fe());

   // Place arguments to StringUTF16.toBytes in temporaries
   // to allow for control flow
   TransformUtil::createTempsForCall(this, treetop);

   TR::Node* valueNode = node->getChild(0);
   TR::Node* offNode = node->getChild(1);
   TR::Node* lenNode = node->getChild(2);

   TR::CFG *cfg = comp()->getFlowGraph();

   // The implementation of java.lang.StringUTF16.toBytes(char[],int,int) will
   // throw a NegativeArraySizeException or OutOfMemoryError if the specified
   // length is outside the range [0,0x3fffffff] or [0,0x3ffffffe], depending on
   // the JDK level.  In order to avoid deciding which to throw in the IL, fall
   // back to the out-of-line call if the length is negative or greater than or
   // equal to 0x3fffffff.  Otherwise, create the byte array and copy the input
   // char array to it with java.lang.String.decompressedArrayCopy
   //
   // Before:
   //
   // +----------------------------------------+
   // | treetop                                |
   // |   acall  java/lang/StringUTF16.toBytes |
   // |     aload  charArr                     |
   // |     iload  off                         |
   // |     iload  len                         |
   // +----------------------------------------+
   //
   // After:
   //
   // ifCmpblock
   // +----------------------------------------+
   // | astore charArrTemp                     |
   // |   aload  charArr                       |
   // | istore offTemp                         |
   // |   iload  off                           |
   // | istore lenTemp                         |
   // |   iload  len                           |
   // | ifiucmpge --> fallbackPathBlock  -----------------+
   // |   iload lenTemp                        |          |
   // |   iconst 0x3fffffff                    |          |
   // +--------------------+-------------------+          |
   //                      |                              |
   // fallThroughPathBlock V                              |
   // +----------------------------------------+          |
   // | astore result                          |          |
   // |   newarray jitNewArray                 |          |
   // |     ishl                               |          |
   // |       iload lenTemp                    |          |
   // |       icosnt 1                         |          |
   // |     iconst 8   ; array type is byte    |          |
   // | treetop                                |          |
   // |   call java/lang/String.decompressedArrayCopy     |
   // |     aload charArrTemp                  |          |
   // |     iload offTemp                      |          |
   // |     ==>newarray                        |          |
   // |     iconst 0                           |          |
   // |     iload lenTemp                      |          |
   // | goto joinBlock   ----------------------------+    |
   // +----------------------------------------+     |    |
   //                                                |    |
   //                      +------------------------------+
   //                      |                         |
   // fallbackPathBlock    V (freq 0) (cold)         |
   // +----------------------------------------+     |
   // | astore result                          |     |
   // |   acall  java/lang/StringUTF16.toBytes |     |
   // |     aload  charArrTemp                 |     |
   // |     iload  offTemp                     |     |
   // |     iload  lenTemp                     |     |
   // +--------------------+-------------------+     |
   //                      |                         |
   //                      +-------------------------+
   //                      |
   // joinBlock            V
   // +----------------------------------------+
   // | treetop                                |
   // |   aload result  ; Replaces acall StringUTF16.toBytes
   // +----------------------------------------+
   //
   TR::Node *upperBoundConstNode = TR::Node::iconst(node, TR::getMaxSigned<TR::Int32>() >> 1);
   TR::Node *ifCmpNode = TR::Node::createif(TR::ifiucmpge, lenNode, upperBoundConstNode);
   TR::TreeTop *ifCmpTreeTop = TR::TreeTop::create(comp(), treetop->getPrevTreeTop(), ifCmpNode);

   // Create temporary variable that will be used to hold result
   TR::DataType resultDataType = node->getDataType();
   TR::SymbolReference *resultSymRef = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), resultDataType);

   // Create result byte array and copy input char array to it with String.decompressedArrayCopy
   int32_t byteArrayType = fej9->getNewArrayTypeFromClass(fej9->getByteArrayClass());

   TR::Node *newByteArrayNode = TR::Node::createWithSymRef(TR::newarray, 2, 2,
                                      TR::Node::create(TR::ishl, 2, lenNode,
                                                       TR::Node::iconst(1)),
                                      TR::Node::iconst(byteArrayType),
                                      getSymRefTab()->findOrCreateNewArraySymbolRef(
                                            node->getSymbolReference()->getOwningMethodSymbol(comp())));

   newByteArrayNode->copyByteCodeInfo(node);
   newByteArrayNode->setCanSkipZeroInitialization(true);
   newByteArrayNode->setIsNonNull(true);

   TR::Node *newByteArrayStoreNode = TR::Node::createStore(node, resultSymRef, newByteArrayNode);
   TR::TreeTop *newByteArraryTreeTop = TR::TreeTop::create(comp(), ifCmpTreeTop, newByteArrayStoreNode);

   TR::Node* newCallNode = TR::Node::createWithSymRef(node, TR::call, 5,
      getSymRefTab()->methodSymRefFromName(comp()->getMethodSymbol(), "java/lang/String", "decompressedArrayCopy", "([CI[BII)V", TR::MethodSymbol::Static));
   newCallNode->setAndIncChild(0, valueNode);
   newCallNode->setAndIncChild(1, offNode);
   newCallNode->setAndIncChild(2, newByteArrayNode);
   newCallNode->setAndIncChild(3, TR::Node::iconst(0));
   newCallNode->setAndIncChild(4, lenNode);

   TR::TreeTop* lastFallThroughTreeTop = TR::TreeTop::create(comp(), newByteArraryTreeTop,
                                          TR::Node::create(node, TR::treetop, 1, newCallNode));

   // Insert the allocationFence after the arraycopy because the array can be allocated from the non-zeroed TLH
   // and therefore we need to make sure no other thread sees stale memory from the array element section.
   if (cg()->getEnforceStoreOrder())
      {
      TR::Node *allocationFence = TR::Node::createAllocationFence(newByteArrayNode, newByteArrayNode);
      lastFallThroughTreeTop = TR::TreeTop::create(comp(), lastFallThroughTreeTop, allocationFence);
      }

   // Copy the original call tree for the fallback path, and store the
   // result into the temporary that was created.
   TR::Node *fallbackCallNode = node->duplicateTree();
   TR::Node *fallbackStoreNode = TR::Node::createStore(node, resultSymRef, fallbackCallNode);
   TR::TreeTop *fallbackTreeTop = TR::TreeTop::create(comp(), lastFallThroughTreeTop, fallbackStoreNode);

   // Replace original call node with the load of the temporary
   // variable that is stored on both sides of the if branch.
   prepareToReplaceNode(node);
   TR::Node::recreate(node, comp()->il.opCodeForDirectLoad(resultDataType));
   node->setSymbolReference(resultSymRef);

   // Split the current block right after the ifiucmpge
   TR::Block *ifCmpBlock = ifCmpTreeTop->getEnclosingBlock();

   // Then split the inline version of the code into its own block
   TR::Block *fallThroughPathBlock = ifCmpBlock->split(newByteArraryTreeTop, cfg, true /* fixUpCommoning */, true /* copyExceptionSuccessors */);

   // Then split the fallback, out-of-line call into its own block
   TR::Block *fallbackPathBlock = fallThroughPathBlock->split(fallbackTreeTop, cfg, true /* fixUpCommoning */, true /* copyExceptionSuccessors */);

   // Then split again at the original call TreeTop to create the tail block
   TR::Block *tailBlock = fallbackPathBlock->split(treetop, cfg, true /* fixUpCommoning */, true /* copyExceptionSuccessors */);

   // Now create a node to go to the merge (i.e. tail) block.
   TR::Node *gotoNode = TR::Node::create(node, TR::Goto);
   TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode, NULL, NULL);
   gotoNode->setBranchDestination(tailBlock->getEntry());
   fallThroughPathBlock->getExit()->insertBefore(gotoTree);

   // Now we have fall-through block, fallback block and tail/merge block.
   // Set the ifiucmp's destination to the fallback block and update the CFG as well.
   ifCmpNode->setBranchDestination(fallbackPathBlock->getEntry());
   cfg->addEdge(ifCmpBlock, fallbackPathBlock);
   cfg->addEdge(fallThroughPathBlock, tailBlock);
   cfg->removeEdge(fallThroughPathBlock, fallbackPathBlock);

   // The original call to StringUTF16.toBytes will only be used
   // if an exception needs to be thrown.  Mark it as cold.
   fallbackPathBlock->setFrequency(UNKNOWN_COLD_BLOCK_COUNT);
   fallbackPathBlock->setIsCold();
   }

#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
// helper function for process_jdk_internal_util_ArraysSupport_vectorizedMismatch
// see comments there for more details
static TR::Node* insertVectorizedMisMatchArgumentChecksAndAdjustForOffHeap(TR::Optimization* optimization,
                     TR::Node* node,               // node of the parameter a/b
                     TR::Block* currentBlock,      // the block before callBlock
                     TR::Block* callBlock,         // original callBlock
                     bool insertArrayCheck,        // whether we need to do a type check
                     TR::CFG* cfg)
   {
   TR::Compilation* comp = optimization->comp();
   // create the storeTree to store the value of the array in a symRef
   TR::SymbolReference* symRef = comp->getSymRefTab()->createTemporary(comp->getMethodSymbol(), TR::Address);
   symRef->getSymbol()->setNotCollected();
   TR::Node* storeNode = TR::Node::createStore(symRef, node);
   TR::TreeTop* storeTree = TR::TreeTop::create(comp, storeNode);
   currentBlock->getExit()->insertBefore(storeTree);

   // insert the trees 0/3
   TR::Block* nullCheckBlock = callBlock;
   TR::Block* newCallBlock =
      nullCheckBlock->split(nullCheckBlock->getEntry()->getNextTreeTop(), cfg);
   TR::Block* adjustBlock = nullCheckBlock->split(nullCheckBlock->getExit(), cfg);

   // insert null check tree 1/3
   TR::Node* nullCheckNode = TR::Node::createif(TR::ifacmpeq,
                                                node->duplicateTree(),
                                                TR::Node::aconst(node, 0),
                                                newCallBlock->getEntry());
   TR::TreeTop *nullCheckTT = TR::TreeTop::create(comp, nullCheckNode);
   nullCheckBlock->append(nullCheckTT);
   cfg->addEdge(nullCheckBlock, newCallBlock);
   optimization->anchorAllChildren(nullCheckNode, nullCheckTT);

   // insert array check tree 2/3
   if (insertArrayCheck)
      {
      TR::Block* arrayCheckBlock = callBlock->split(callBlock->getExit(), cfg);

      TR::Node* vftLoad = TR::Node::createWithSymRef(TR::aloadi, 1, 1,
         node->duplicateTree(), comp->getSymRefTab()->findOrCreateVftSymbolRef());
      TR::Node* maskedIsArrayClassNode = comp->fej9()->testIsClassArrayType(vftLoad);
      TR::Node* arrayCheckNode = TR::Node::createif(TR::ificmpeq, maskedIsArrayClassNode,
                                                   TR::Node::iconst(node, 0),
                                                   newCallBlock->getEntry());

      TR::TreeTop *arrayCheckTT = TR::TreeTop::create(comp, arrayCheckNode, NULL, NULL);
      arrayCheckBlock->append(arrayCheckTT);
      cfg->addEdge(callBlock, newCallBlock);
      optimization->anchorAllChildren(arrayCheckNode, arrayCheckTT);
      }

   // insert newStoreTree 3/3
   TR::Node* adjustedNode = TR::TransformUtil::generateDataAddrLoadTrees(comp,
                                                                  node->duplicateTree());
   TR::Node* newStore = TR::Node::createStore(symRef, adjustedNode);
   TR::TreeTop* newStoreTree = TR::TreeTop::create(comp, newStore);
   adjustBlock->append(newStoreTree);
   optimization->anchorAllChildren(adjustedNode, newStoreTree);

   TR::Node* resultNode = TR::Node::createLoad(node, symRef);
   return resultNode;
   }
#endif /* defined(J9VM_GC_SPARSE_HEAP_ALLOCATION) */

void J9::RecognizedCallTransformer::process_jdk_internal_util_ArraysSupport_vectorizedMismatch(TR::TreeTop* treetop, TR::Node* node)
   {
   TR::Node* a = node->getChild(0);
   TR::Node* aOffset = node->getChild(1);
   TR::Node* b = node->getChild(2);
   TR::Node* bOffset = node->getChild(3);
   TR::Node* length = node->getChild(4);
   TR::Node* log2ArrayIndexScale = node->getChild(5);

   anchorAllChildren(node, treetop);

#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
   if (TR::Compiler->om.isOffHeapAllocationEnabled())
      {
      int aLen, bLen;
      const char* aObjTypeSig = a->getSymbolReference() ?
         a->getSymbolReference()->getTypeSignature(aLen) : NULL;
      const char* bObjTypeSig = b->getSymbolReference() ?
         b->getSymbolReference()->getTypeSignature(bLen) : NULL;

      // if the object type is not known at compile time we need to check for arrays at runtime
      bool aCheckNeeded = (aObjTypeSig == NULL);
      bool bCheckNeeded = (bObjTypeSig == NULL);
      if (!aCheckNeeded) // we do know it on compile
         {
         TR_OpaqueClassBlock* aClass = comp()->fej9()->getClassFromSignature(aObjTypeSig,
            aLen, a->getSymbolReference()->getOwningMethod(comp()));
         aCheckNeeded = aClass == NULL ||
                        aClass == comp()->getObjectClassPointer() ||
                        TR::Compiler->cls.isInterfaceClass(comp(), aClass);
         }

      if (!bCheckNeeded)
         {
         TR_OpaqueClassBlock* bClass = comp()->fej9()->getClassFromSignature(bObjTypeSig,
            bLen, b->getSymbolReference()->getOwningMethod(comp()));
         bCheckNeeded = bClass == NULL ||
                        bClass == comp()->getObjectClassPointer() ||
                        TR::Compiler->cls.isInterfaceClass(comp(), bClass);
         }


      // true if the object type is either an array, or not known at compile time
      bool aAdjustmentNeeded = aCheckNeeded || (aObjTypeSig[0] == '[');
      bool bAdjustmentNeeded = bCheckNeeded || (bObjTypeSig[0] == '[');

      TR::TransformUtil::separateNullCheck(comp(), treetop);

      // If neither a nor b is an array, then the address is simply ref + offset, identical to
      // the default implementation, allowing us to skip the adjustments
      //
      // If we are certain a or b is an array during compile time, we just need to load its
      // starting point when needed after a null check.
      //
      // Otherwise, we will do an additional check at run time, and skip the loading if the objects
      // are not an array and fall through to the default implementation.
      //
      // The resulting blocks should look like this:
      // CurrentBlock:
      //    Trees anchoring the callNode's children
      //    Trees preceding the callNode's tree
      // ------ done by insertVectorizedMisMatchArgumentChecksAndAdjustForOffHeap
      //    storeTree
      // callBlock i.e. nullCheckBlock:
      //    Tree for null check
      // arrayCheckBlock (only if type is uncertain on compile):
      //    Tree for type check
      // adjustBlock:
      //    newStoreTree
      // newCallBlock:
      //    Tree containing callNode (later transformed into iselect)
      // ------ back to mainline
      // nextBlock:
      //    Trees after callNode's tree
      if (aAdjustmentNeeded || bAdjustmentNeeded)
         {
         // callBlock should contain the tree of this treetop only
         TR::CFG* cfg = comp()->getFlowGraph();
         TR::Block* currentBlock = treetop->getEnclosingBlock();
         TR::Block* callBlock = currentBlock->split(treetop, cfg, true);
         TR::Block* nextBlock = callBlock->split(treetop->getNextTreeTop(), cfg, true);

         if (aAdjustmentNeeded)
            {
            // create and arrange nullCheckBlock, arrayCheckBlock, adjustBlock, and newCallBlock
            // also create storeTree in currentBlock
            a = insertVectorizedMisMatchArgumentChecksAndAdjustForOffHeap(this,
               a, currentBlock, callBlock, aCheckNeeded, cfg);
            }
         else
            {
            a = a->duplicateTree();
            }

         if (bAdjustmentNeeded)
            {
            // create and arrange nullCheckBlock, arrayCheckBlock, adjustBlock, and newCallBlock
            // also create storeTree in currentBlock
            b = insertVectorizedMisMatchArgumentChecksAndAdjustForOffHeap(this,
               b, currentBlock, callBlock, bCheckNeeded, cfg);
            }
         else
            {
            b = b->duplicateTree();
            }

         // all the children need to be duplicated after the block split
         aOffset = aOffset->duplicateTree();
         bOffset = bOffset->duplicateTree();
         length = length->duplicateTree();
         log2ArrayIndexScale = log2ArrayIndexScale->duplicateTree();
         }
      }
#endif /* defined(J9VM_GC_SPARSE_HEAP_ALLOCATION) */

   TR::Node* log2ArrayIndexScale64Bits = TR::Node::create(node, TR::iu2l, 1, log2ArrayIndexScale);

   TR::Node* lengthInBytes = TR::Node::create(node, TR::lshl, 2,
      TR::Node::create(node, TR::iu2l, 1, length),
      log2ArrayIndexScale);

   TR::Node* mask = TR::Node::create(node, TR::lor, 2,
      TR::Node::create(node, TR::lshl, 2,
         log2ArrayIndexScale64Bits,
         TR::Node::iconst(node, 1)),
      TR::Node::lconst(node, 3));

   TR::Node* lengthToCompare = TR::Node::create(node, TR::land, 2,
      lengthInBytes,
      TR::Node::create(node, TR::lxor, 2, mask, TR::Node::lconst(node, -1)));

   TR::Node* mismatchByteIndex = TR::Node::create(node, TR::arraycmplen, 3);


   mismatchByteIndex->setAndIncChild(0, TR::Node::create(node, TR::aladd, 2, a, aOffset));
   mismatchByteIndex->setAndIncChild(1, TR::Node::create(node, TR::aladd, 2, b, bOffset));
   mismatchByteIndex->setAndIncChild(2, lengthToCompare);
   mismatchByteIndex->setSymbolReference(getSymRefTab()->findOrCreateArrayCmpLenSymbol());

   TR::Node* invertedRemainder = TR::Node::create(node, TR::ixor, 2,
      TR::Node::create(node, TR::l2i, 1,
         TR::Node::create(node, TR::lshr, 2,
            TR::Node::create(node, TR::land, 2, lengthInBytes, mask),
            log2ArrayIndexScale)),
      TR::Node::iconst(node, -1));

   TR::Node* mismatchElementIndex = TR::Node::create(node, TR::l2i, 1, TR::Node::create(node, TR::lshr, 2, mismatchByteIndex, log2ArrayIndexScale));
   TR::Node* noMismatchFound = TR::Node::create(node, TR::lcmpeq, 2, mismatchByteIndex, lengthToCompare);

   prepareToReplaceNode(node);

   TR::Node::recreate(node, TR::iselect);
   node->setNumChildren(3);
   node->setAndIncChild(0, noMismatchFound);
   node->setAndIncChild(1, invertedRemainder);
   node->setAndIncChild(2, mismatchElementIndex);

   TR::TransformUtil::removeTree(comp(), treetop);
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

If OffHeap is enabled, a runtime isArray check is added to determine
if loading the dataAddrPtr is necessary.

                          yes
isObjectNull [A] ------------------------------------------>
    |                                                      |
    | no                                                   |
#if OffHeap                                                |
    |                yes                                   |
isArray [I] ------------------------>                      |
    |                               |                      |
    |                use the dataAddrPtr of the array      |
    |                 xcall atomic method helper [J]       |
    |                               |                      |
    |                              [H]                     |
    | no                                                   |
    |                                                      |
#endif OffHeap                                             |
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

#if OffHeap
start Block_I
ifacmpne -> <Block_J>
   andi
     lloadi <isClassDepthAndFlags>
       aloadi  <vft-symbol>
         aload  <object-1>
     iconst 0x10000 // array-flag
   iconst 0
end Block_I
#endif OffHeap

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

#if OffHeap
start Block_J
xcall atomic method helper
  aladd
    aloadi <contiguousArrayDataAddrField>
      aload <object-1>
    lload <offset>
  xload value
xstore
  ==>xcall
goto --> block_H
end Block_J
#endif OffHeap

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
   OMR::Logger *log = comp()->log();
   TR::Node* unsafeCall = treetop->getNode()->getFirstChild();
   TR::Node* objectNode = unsafeCall->getChild(1);
   TR::Node* offsetNode = unsafeCall->getChild(2);
   TR::Node* address = NULL;

   TR::SymbolReference* newSymbolReference = NULL;
   TR::CFG*     cfg = comp()->getMethodSymbol()->getFlowGraph();
   TR::Node*    isObjectNullNode = NULL;
   TR::TreeTop* isObjectNullTreeTop = NULL;
   TR::Node*    isObjectArrayNode = NULL;
   TR::TreeTop* isObjectArrayTreeTop = NULL;
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

      logprintf(enableTrace, log, "Created node %p to preserve NULLCHK on unsafe call %p\n", checkNode, unsafeCall);
      }

   if (isNotStaticField)
      {
      // It is safe to skip diamond, the address can be calculated directly via [object+offset]

      // Except if OffHeap is used, then check if object is array
#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
      if (TR::Compiler->om.isOffHeapAllocationEnabled())
         {
         // Generate null-check and array-check blocks
         TR::TransformUtil::createTempsForCall(this, treetop);
         objectNode = unsafeCall->getChild(1);
         offsetNode = unsafeCall->getChild(2);

         // Test if object is null
         isObjectNullNode = TR::Node::createif(TR::ifacmpeq, objectNode->duplicateTree(), TR::Node::aconst(0), NULL);
         isObjectNullTreeTop = TR::TreeTop::create(comp(), isObjectNullNode);
         treetop->insertBefore(isObjectNullTreeTop);
         treetop->getEnclosingBlock()->split(treetop, cfg, fixupCommoning);

         logprintf(enableTrace, comp()->log(), "Created isObjectNull test node n%dn, non-null object will fall through to Block_%d\n",
               isObjectNullNode->getGlobalIndex(), treetop->getEnclosingBlock()->getNumber());

         //generate array check treetop
         TR::Node *vftLoad = TR::Node::createWithSymRef(TR::aloadi, 1, 1,
                                                        objectNode->duplicateTree(),
                                                        comp()->getSymRefTab()->findOrCreateVftSymbolRef());

         isObjectArrayNode = TR::Node::createif(TR::ificmpne,
                                                comp()->fej9()->testIsClassArrayType(vftLoad),
                                                TR::Node::create(TR::iconst, 0),
                                                NULL);
         isObjectArrayTreeTop = TR::TreeTop::create(comp(), isObjectArrayNode, NULL, NULL);
         treetop->insertBefore(isObjectArrayTreeTop);
         treetop->getEnclosingBlock()->split(treetop, cfg, fixupCommoning);

         logprintf(enableTrace, log, "Created isObjectArray test node n%dn, array will branch to array access block\n", isObjectArrayNode->getGlobalIndex());

         address = comp()->target().is32Bit() ? TR::Node::create(TR::aiadd, 2, objectNode->duplicateTree(), TR::Node::create(TR::l2i, 1, offsetNode->duplicateTree())) :
                                              TR::Node::create(TR::aladd, 2, objectNode->duplicateTree(), offsetNode->duplicateTree());
         }
      else
#endif /* J9VM_GC_SPARSE_HEAP_ALLOCATION */
         {
         address = comp()->target().is32Bit() ? TR::Node::create(TR::aiadd, 2, objectNode, TR::Node::create(TR::l2i, 1, offsetNode)) :
                                                TR::Node::create(TR::aladd, 2, objectNode, offsetNode);
         logprints(enableTrace, log, "Field is not static, use the object and offset directly\n");
         }
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
         logprintf(enableTrace, log, "Created NULLCHK tree %p on the first argument of Unsafe call\n", treetop->getPrevTreeTop());
         }

      // Test if object is null
      isObjectNullNode = TR::Node::createif(TR::ifacmpeq, objectNode->duplicateTree(), TR::Node::aconst(0), NULL);
      isObjectNullTreeTop = TR::TreeTop::create(comp(), isObjectNullNode);
      treetop->insertBefore(isObjectNullTreeTop);
      treetop->getEnclosingBlock()->split(treetop, cfg, fixupCommoning);

      logprintf(enableTrace, log, "Created isObjectNull test node n%dn, non-null object will fall through to Block_%d\n",
         isObjectNullNode->getGlobalIndex(), treetop->getEnclosingBlock()->getNumber());

      // Test if object is array - offheap only
   #if defined (J9VM_GC_SPARSE_HEAP_ALLOCATION)
      if (TR::Compiler->om.isOffHeapAllocationEnabled() && comp()->target().is64Bit())
         {
         //generate array check treetop
         TR::Node *vftLoad = TR::Node::createWithSymRef(TR::aloadi, 1, 1,
                                                        objectNode->duplicateTree(),
                                                        comp()->getSymRefTab()->findOrCreateVftSymbolRef());

         isObjectArrayNode = TR::Node::createif(TR::ificmpne,
                                                comp()->fej9()->testIsClassArrayType(vftLoad),
                                                TR::Node::create(TR::iconst, 0),
                                                NULL);
         isObjectArrayTreeTop = TR::TreeTop::create(comp(), isObjectArrayNode, NULL, NULL);
         treetop->insertBefore(isObjectArrayTreeTop);
         treetop->getEnclosingBlock()->split(treetop, cfg, fixupCommoning);

         logprintf(enableTrace, log, "Created isObjectArray test node n%dn, array will branch to array access block\n", isObjectArrayNode->getGlobalIndex());
         }
   #endif /* J9VM_GC_SPARSE_HEAP_ALLOCATION */

      // Test if low tag is set
      isNotLowTaggedNode = TR::Node::createif(TR::iflcmpne,
                                               TR::Node::create(TR::land, 2, offsetNode->duplicateTree(), TR::Node::lconst(J9_SUN_STATIC_FIELD_OFFSET_TAG)),
                                               TR::Node::lconst(J9_SUN_STATIC_FIELD_OFFSET_TAG),
                                               NULL);
      isNotLowTaggedTreeTop = TR::TreeTop::create(comp(), isNotLowTaggedNode);
      treetop->insertBefore(isNotLowTaggedTreeTop);
      treetop->getEnclosingBlock()->split(treetop, cfg, fixupCommoning);

      logprintf(enableTrace, log, "Created isNotLowTagged test node n%dn, static field will fall through to Block_%d\n",
         isNotLowTaggedNode->getGlobalIndex(), treetop->getEnclosingBlock()->getNumber());

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
         logprintf(enableTrace, log, "Created isFinal test node n%dn, non-final-static field will fall through to Block_%d, final field goes to Block_%d\n",
            isFinalNode->getGlobalIndex(), treetop->getEnclosingBlock()->getNumber(), reportFinalFieldModification->getEnclosingBlock()->getNumber());

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

      logprintf(enableTrace, log, "Created node objectAdjustmentNode n%dn #%d and offsetAdjustmentNode #%d n%dn to adjust object and offset for static field\n",
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

   // Setup and connect check blocks if generated
   if (isObjectNullTreeTop)
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
         else
            logprintf(enableTrace, log, "storeNode n%dn #%d for the return value of atomic method helper\n",
               storeReturnNode->getGlobalIndex(), storeReturnNode->getSymbolReference()->getReferenceNumber());
         }

      // If isNotStaticField and array-check is generated then
      // setup and connect the null and array check blocks
#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION)
      if (isNotStaticField && isObjectArrayTreeTop)
         {
         // Generate array access block
         TR::TreeTop *arrayAccessTreeTop = treetop->duplicateTree();
         TR::Node *addrToAccessNode = arrayAccessTreeTop->getNode()->getChild(0)->getChild(0);
         TR::Block *arrayAccessBlock = TR::Block::createEmptyBlock(arrayAccessTreeTop->getNode(), comp(),
                                                                   treetop->getEnclosingBlock()->getFrequency());
         arrayAccessBlock->append(arrayAccessTreeTop);

         //load dataAddr
         TR::Node *objBaseAddrNode = addrToAccessNode->getChild(0);
         TR::Node *dataAddrNode = TR::TransformUtil::generateDataAddrLoadTrees(comp(), objBaseAddrNode);
         addrToAccessNode->setChild(0, dataAddrNode);

         //correct refcounts
         objBaseAddrNode->decReferenceCount();
         dataAddrNode->incReferenceCount();

         //set as array test destination and insert array access into IL trees
         // - if object is array, goto array access block
         // - else, fall through to non-array access
         treetop->getEnclosingBlock()->getExit()->insertTreeTopsAfterMe(arrayAccessBlock->getEntry(), arrayAccessBlock->getExit());
         isObjectArrayNode->setBranchDestination(arrayAccessTreeTop->getEnclosingBlock()->getEntry());

         treetop->getEnclosingBlock()->append(TR::TreeTop::create(comp(), TR::Node::create(arrayAccessTreeTop->getNode(),
                                                                              TR::Goto, 0,
                                                                              returnBlock->getEntry())));
         cfg->addNode(arrayAccessBlock);
         cfg->addEdge(TR::CFGEdge::createEdge(isObjectArrayTreeTop->getEnclosingBlock(), arrayAccessBlock, comp()->trMemory()));
         cfg->addEdge(TR::CFGEdge::createEdge(arrayAccessBlock, returnBlock, comp()->trMemory()));

         // Store the return value from the helper call for array access
         if (storeReturnNode)
            {
            TR::Node *storeNode = TR::Node::createStore(unsafeCall, storeReturnNode->getSymbolReference(), arrayAccessTreeTop->getNode()->getFirstChild());
            arrayAccessTreeTop->insertTreeTopsAfterMe(TR::TreeTop::create(comp(), storeNode));
            }

         isObjectNullNode->setBranchDestination(treetop->getEnclosingBlock()->getEntry());
         cfg->addEdge(TR::CFGEdge::createEdge(isObjectNullTreeTop->getEnclosingBlock(), treetop->getEnclosingBlock(), comp()->trMemory()));

         logprintf(enableTrace, log, "Created array access helper block_%d that loads dataAddr pointer from array object address\n", arrayAccessBlock->getNumber());
         }
      else
#endif /* J9VM_GC_SPARSE_HEAP_ALLOCATION */
      // If not isNotStaticField then setup and connect all
      // generated check blocks
      if (!isNotStaticField)
         {
         // Create another helper call that loads from ramStatics
         /* Example of the atomic method helper
         * n92n  treetop
         * n93n    icall  <atomicFetchAndAdd>
         * n94n      aladd
         * n95n        aload  <temp slot 5>
         * n96n        lload  <temp slot 3>
         * n97n      iload  <temp slot 4>
         */
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

         logprintf(enableTrace, log, "Created atomic method helper block_%d that loads from ramStatics treetop n%dn. returnBlock block_%d\n",
               unsafeCallRamStaticsTT->getEnclosingBlock()->getNumber(), unsafeCallRamStaticsTT->getNode()->getGlobalIndex(), returnBlock->getNumber());

         // Split the block that contains the original helper call into a separate block
         treetop->getEnclosingBlock()->split(treetop, cfg, fixupCommoning);

         // Create another helper call for array access (offheap only)
      #if defined (J9VM_GC_SPARSE_HEAP_ALLOCATION)
         if (isObjectArrayTreeTop != NULL)
            {
            TR::TreeTop *arrayAccessTreeTop = treetop->duplicateTree();
            TR::Node *addrToAccessNode = arrayAccessTreeTop->getNode()->getChild(0)->getChild(0);
            TR::Block *arrayAccessBlock = TR::Block::createEmptyBlock(arrayAccessTreeTop->getNode(), comp(),
                                                                     treetop->getEnclosingBlock()->getFrequency());
            arrayAccessBlock->append(arrayAccessTreeTop);
            arrayAccessBlock->append(TR::TreeTop::create(comp(), TR::Node::create(arrayAccessTreeTop->getNode(),
                                                                                 TR::Goto, 0,
                                                                                 returnBlock->getEntry())));

            //load dataAddr
            TR::Node *objBaseAddrNode = addrToAccessNode->getChild(0);
            TR::Node *dataAddrNode = TR::TransformUtil::generateDataAddrLoadTrees(comp(), objBaseAddrNode);
            addrToAccessNode->setChild(0, dataAddrNode);

            //correct refcounts
            objBaseAddrNode->decReferenceCount();
            dataAddrNode->incReferenceCount();

            //set as array test destination and insert array access into IL trees
            // - if object is array, goto array access block
            // - else, fall through to lowtag test
            unsafeCallRamStaticsTT->getEnclosingBlock()->getExit()->insertTreeTopsAfterMe(arrayAccessBlock->getEntry(), arrayAccessBlock->getExit());
            isObjectArrayNode->setBranchDestination(arrayAccessTreeTop->getEnclosingBlock()->getEntry());

            cfg->addNode(arrayAccessBlock);
            cfg->addEdge(TR::CFGEdge::createEdge(isObjectArrayTreeTop->getEnclosingBlock(), arrayAccessBlock, comp()->trMemory()));
            cfg->addEdge(TR::CFGEdge::createEdge(arrayAccessBlock, returnBlock, comp()->trMemory()));

            // Store the return value from the helper call for array access
            if (storeReturnNode)
               {
               TR::Node *storeNode = TR::Node::createStore(unsafeCall, storeReturnNode->getSymbolReference(), arrayAccessTreeTop->getNode()->getFirstChild());
               arrayAccessTreeTop->insertTreeTopsAfterMe(TR::TreeTop::create(comp(), storeNode));
               }

            logprintf(enableTrace, log, "Created array access helper block_%d that loads dataAddr pointer from array object address\n", arrayAccessBlock->getNumber());
            }
      #endif /* J9VM_GC_SPARSE_HEAP_ALLOCATION */

         // Setup CFG edges
         cfg->addEdge(unsafeCallRamStaticsTT->getEnclosingBlock(), returnBlock);

         logprintf(enableTrace, log, "Block_%d contains call to atomic method helper, and is the target of isObjectNull and isNotLowTagged tests\n", treetop->getEnclosingBlock()->getNumber());

         isObjectNullNode->setBranchDestination(treetop->getEnclosingBlock()->getEntry());
         cfg->addEdge(TR::CFGEdge::createEdge(isObjectNullTreeTop->getEnclosingBlock(), treetop->getEnclosingBlock(), comp()->trMemory()));
         isNotLowTaggedNode->setBranchDestination(treetop->getEnclosingBlock()->getEntry());
         cfg->addEdge(TR::CFGEdge::createEdge(isNotLowTaggedTreeTop->getEnclosingBlock(), treetop->getEnclosingBlock(), comp()->trMemory()));

         cfg->removeEdge(unsafeCallRamStaticsTT->getEnclosingBlock(), treetop->getEnclosingBlock());
         }
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
   TR::DebugCounter::prependDebugCounter(
      comp(),
      TR::DebugCounter::debugCounterName(
         comp(),
         "mh.unrefined/invokeBasic/(%s)/%s",
         comp()->signature(),
         comp()->getHotnessName()),
      treetop);

   TR_J9VMBase* fej9 = static_cast<TR_J9VMBase*>(comp()->fe());
   TR::TransformUtil::separateNullCheck(comp(), treetop, trace());

   // We'll get the J9Method by loading mh.form.vmentry.vmtarget.
   // isVolatile, isPrivate, and isFinal here describe both form and vmentry.
   bool isVolatile = false;
   bool isPrivate = false;
   bool isFinal = true;

   uint32_t offset = fej9->getInstanceFieldOffsetIncludingHeader(
      "Ljava/lang/invoke/MethodHandle;",
      "form",
      "Ljava/lang/invoke/LambdaForm;",
      comp()->getCurrentMethod());

   TR::SymbolReference *lambdaFormSymRef =
      comp()->getSymRefTab()->findOrFabricateShadowSymbol(
         comp()->getMethodSymbol(),
         TR::Symbol::Java_lang_invoke_MethodHandle_form,
         TR::Address,
         offset,
         isVolatile,
         isPrivate,
         isFinal,
         "java/lang/invoke/MethodHandle.form Ljava/lang/invoke/LambdaForm;");

   offset = fej9->getInstanceFieldOffsetIncludingHeader(
      "Ljava/lang/invoke/LambdaForm;",
      "vmentry",
      "Ljava/lang/invoke/MemberName;",
      comp()->getCurrentMethod());

   TR::SymbolReference *memberNameSymRef =
      comp()->getSymRefTab()->findOrFabricateShadowSymbol(
         comp()->getMethodSymbol(),
         TR::Symbol::Java_lang_invoke_LambdaForm_vmentry,
         TR::Address,
         offset,
         isVolatile,
         isPrivate,
         isFinal,
         "java/lang/invoke/LambdaForm.vmentry Ljava/lang/invoke/MemberName;");

   TR::SymbolReference *vmTargetSymRef =
      comp()->getSymRefTab()->findOrFabricateMemberNameVmTargetShadow();

   if (comp()->cg()->enableJitDispatchJ9Method())
      {
      node->setSymbolReference(
         comp()->getSymRefTab()->findOrCreateDispatchJ9MethodSymbolRef());

      TR::Node *mh = node->getChild(0);
      TR::Node *lf = TR::Node::createWithSymRef(node, TR::aloadi, 1, mh, lambdaFormSymRef);
      TR::Node *mn = TR::Node::createWithSymRef(node, TR::aloadi, 1, lf, memberNameSymRef);
      TR::Node *j9m = TR::Node::createWithSymRef(node, TR::aloadi, 1, mn, vmTargetSymRef);
      node->addChildren(&j9m, 1);
      for (int32_t i = node->getNumChildren() - 1; i > 0; i--)
         node->setChild(i, node->getChild(i - 1));

      node->setChild(0, j9m);
      return;
      }

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
   TR::Node * lambdaFormNode = TR::Node::createWithSymRef(node, comp()->il.opCodeForIndirectLoad(TR::Address), 1 , mhNode, lambdaFormSymRef);
   lambdaFormNode->setIsNonNull(true);
   TR::Node * memberNameNode = TR::Node::createWithSymRef(node, comp()->il.opCodeForIndirectLoad(TR::Address), 1, lambdaFormNode, memberNameSymRef);
   TR::Node * vmTargetNode = TR::Node::createWithSymRef(node, TR::aloadi, 1, memberNameNode, vmTargetSymRef);
   processVMInternalNativeFunction(treetop, node, vmTargetNode, argsList, inlCallNode);
   }

void J9::RecognizedCallTransformer::process_java_lang_invoke_MethodHandle_linkToStaticSpecial(TR::TreeTop* treetop, TR::Node* node)
   {
   TR::RecognizedMethod rm =
      node->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod();

   bool isLinkToSpecial = rm == TR::java_lang_invoke_MethodHandle_linkToSpecial;

   TR::DebugCounter::prependDebugCounter(
      comp(),
      TR::DebugCounter::debugCounterName(
         comp(),
         "mh.unrefined/linkTo%s/(%s)/%s",
         isLinkToSpecial ? "Special" : "Static",
         comp()->signature(),
         comp()->getHotnessName()),
      treetop);

   TR_J9VMBase* fej9 = static_cast<TR_J9VMBase*>(comp()->fe());

   TR::SymbolReference *vmTargetSymRef =
      comp()->getSymRefTab()->findOrFabricateMemberNameVmTargetShadow();

   if (comp()->cg()->enableJitDispatchJ9Method())
      {
      if (isLinkToSpecial)
         {
         // Null check the receiver
         TR::SymbolReference *nullCheckSymRef =
            comp()->getSymRefTab()->findOrCreateNullCheckSymbolRef(
               comp()->getMethodSymbol());

         TR::Node *passThrough =
            TR::Node::create(node, TR::PassThrough, 1, node->getChild(0));

         TR::Node *nullCheck = TR::Node::createWithSymRef(
            node, TR::NULLCHK, 1, passThrough, nullCheckSymRef);

         treetop->insertBefore(TR::TreeTop::create(comp(), nullCheck));
         }

      node->setSymbolReference(
         comp()->getSymRefTab()->findOrCreateDispatchJ9MethodSymbolRef());

      int32_t lastChildIndex = node->getNumChildren() - 1;
      TR::Node *memberName = node->getChild(lastChildIndex);

      for (int32_t i = lastChildIndex; i > 0; i--)
         node->setChild(i, node->getChild(i - 1));

      TR::Node *target =
         TR::Node::createWithSymRef(node, TR::aloadi, 1, memberName, vmTargetSymRef);

      node->setAndIncChild(0, target);
      memberName->decReferenceCount();
      return;
      }

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
   TR::DebugCounter::prependDebugCounter(
      comp(),
      TR::DebugCounter::debugCounterName(
         comp(),
         "mh.unrefined/linkToVirtual/(%s)/%s",
         comp()->signature(),
         comp()->getHotnessName()),
      treetop);

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
      TR::Node::createWithSymRef(node, TR::lloadi, 1, memberNameNode, vmIndexSymRef);

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

   if (!comp()->target().is64Bit())
      {
      jittedMethodEntryPoint = TR::Node::create(node, TR::i2l, 1, jittedMethodEntryPoint);
      jitVftOffset = TR::Node::create(node, TR::i2l, 1, jitVftOffset);
      }

   node->setAndIncChild(0, jittedMethodEntryPoint);
   node->setAndIncChild(1, jitVftOffset);

   memberNameNode->decReferenceCount(); // no longer a child of node
   }

void J9::RecognizedCallTransformer::process_java_lang_invoke_MethodHandle_linkToInterface(TR::TreeTop *treetop, TR::Node *node)
   {
   // This counter isn't very informative yet. Every linkToInterface() call
   // should come through here until/unless we come up with a representation for
   // resolved interface calls without constant pool entries.
   TR::DebugCounter::prependDebugCounter(
      comp(),
      TR::DebugCounter::debugCounterName(
         comp(),
         "mh.unrefined/linkToInterface/(%s)/%s",
         comp()->signature(),
         comp()->getHotnessName()),
      treetop);

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
         case TR::java_lang_Integer_compress:
            return cg()->getSupports32BitCompress();
         case TR::java_lang_Long_compress:
            return cg()->getSupports64BitCompress();
         case TR::java_lang_Integer_expand:
            return cg()->getSupports32BitExpand();
         case TR::java_lang_Long_expand:
            return cg()->getSupports64BitExpand();
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
         case TR::java_lang_Math_max_F:
         case TR::java_lang_Math_min_F:
         case TR::java_lang_Math_max_D:
         case TR::java_lang_Math_min_D:
            return !comp()->getOption(TR_DisableMaxMinOptimization) && cg()->getSupportsInlineMath_MaxMin_FD();
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
            return cg()->supportsByteswap();
         case TR::java_lang_StringCoding_encodeASCII:
         case TR::java_lang_String_encodeASCII:
            return cg()->getSupportsInlineEncodeASCII();
         case TR::java_lang_StringLatin1_inflate_BIBII:
            return (cg()->getSupportsArrayTranslateTROTNoBreak() && !comp()->target().cpu.isPower());
         case TR::jdk_internal_util_ArraysSupport_vectorizedMismatch:
            return cg()->getSupportsInlineVectorizedMismatch();
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
         case TR::java_lang_Integer_compress:
            processIntrinsicFunction(treetop, node, TR::icompressbits);
            break;
         case TR::java_lang_Long_compress:
            processIntrinsicFunction(treetop, node, TR::lcompressbits);
            break;
         case TR::java_lang_Integer_expand:
            processIntrinsicFunction(treetop, node, TR::iexpandbits);
            break;
         case TR::java_lang_Long_expand:
            processIntrinsicFunction(treetop, node, TR::lexpandbits);
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
         case TR::java_lang_Math_max_F:
            processIntrinsicFunction(treetop, node, TR::fmax);
            break;
        case TR::java_lang_Math_min_F:
            processIntrinsicFunction(treetop, node, TR::fmin);
            break;
        case TR::java_lang_Math_max_D:
            processIntrinsicFunction(treetop, node, TR::dmax);
            break;
        case TR::java_lang_Math_min_D:
            processIntrinsicFunction(treetop, node, TR::dmin);
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
         case TR::java_lang_StringLatin1_inflate_BIBII:
            process_java_lang_StringLatin1_inflate_BIBII(treetop, node);
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
         case TR::jdk_internal_util_ArraysSupport_vectorizedMismatch:
            process_jdk_internal_util_ArraysSupport_vectorizedMismatch(treetop, node);
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
