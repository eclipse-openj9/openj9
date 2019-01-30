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

#ifndef IDIOM_RECOGNITION_UTILS_INCL
#define IDIOM_RECOGNITION_UTILS_INCL

#include <stddef.h>
#include <stdint.h>
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"

class TR_BitVector;
class TR_CISCNode;
class TR_CISCTransformer;
class TR_PCISCGraph;
class TR_PCISCNode;
namespace TR { class SymbolReference; }
namespace TR { class Block; }
namespace TR { class Compilation; }
namespace TR { class Node; }
template <class T> class List;

enum
   {
   CISCUtilCtl_64Bit = 1,
   CISCUtilCtl_NoI2L = 2,
   CISCUtilCtl_ChildDirectConnected = 4,
   CISCUtilCtl_BigEndian = 8,
   CISCUtilCtl_NoConversion = 16,
   CISCUtilCtl_AllConversion = 32
   };
TR_PCISCGraph *makeStrlen16Graph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makePtrArraySetGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeMemSetGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeMixedMemSetGraph(TR::Compilation *c, int32_t ctrl);

TR_PCISCGraph *makeTRTGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeTRTGraph2(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeTRT4NestedArrayGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeTRT4NestedArrayIfGraph(TR::Compilation *c, int32_t ctrl);

TR_PCISCGraph *makeTROTArrayGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeTRTOArrayGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeTRTOArrayGraphSpecial(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeCopyingTROOSpecialGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeCopyingTROxGraph(TR::Compilation *c, int32_t ctrl, int pattern);
TR_PCISCGraph *makeCopyingTROTInduction1Graph(TR::Compilation *c, int32_t ctrl, int32_t pattern);
TR_PCISCGraph *makeCopyingTRTxGraph(TR::Compilation *c, int32_t ctrl, int pattern);
TR_PCISCGraph *makeCopyingTRTxThreeIfsGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeCopyingTRTOGraphSpecial(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeCopyingTRTOInduction1Graph(TR::Compilation *c, int32_t ctrl, int32_t pattern);
TR_PCISCGraph *makeCopyingTRTTSpecialGraph(TR::Compilation *c, int32_t ctrl);

TR_PCISCGraph *makeMemCmpGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeMemCmpSpecialGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeMemCmpIndexOfGraph(TR::Compilation *c, int32_t ctrl);

TR_PCISCGraph *makeMemCpySpecialGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeMemCpyGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeMemCpyDecGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeMemCpyByteToCharGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeMemCpyByteToCharBndchkGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeMemCpyCharToByteGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeMEMCPYChar2ByteGraph2(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeMEMCPYChar2ByteMixedGraph(TR::Compilation *c, int32_t ctrl);
// not active idioms
TR_PCISCGraph *makeMEMCPYByte2IntGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeMEMCPYInt2ByteGraph(TR::Compilation *c, int32_t ctrl);

TR_PCISCGraph *makeBitOpMemGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeCountDecimalDigitLongGraph(TR::Compilation *c, int32_t ctrl, bool isDiv2Mul);
TR_PCISCGraph *makeCountDecimalDigitIntGraph(TR::Compilation *c, int32_t ctrl, bool isDiv2Mul);
TR_PCISCGraph *makeLongToStringGraph(TR::Compilation *c, int32_t ctrl);
TR_PCISCGraph *makeIntToStringGraph(TR::Compilation *c, int32_t ctrl, bool isDiv2Mul);
TR_PCISCNode *createIdiomIOP2VarInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, int opcode, TR_PCISCNode *v, TR_PCISCNode *subval);
TR_PCISCNode *createIdiomIOP2VarInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, int opcode, TR_PCISCNode *v, TR_PCISCNode *src1, TR_PCISCNode *subval);
TR_PCISCNode *createIdiomDecVarInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *v, TR_PCISCNode *src1, TR_PCISCNode *subval);
TR_PCISCNode *createIdiomIncVarInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *v, TR_PCISCNode *src1, TR_PCISCNode *subval);
TR_PCISCNode *createIdiomDecVarInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *v, TR_PCISCNode *subval);
TR_PCISCNode *createIdiomIncVarInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *v, TR_PCISCNode *subval);
TR_PCISCNode *createIdiomArrayRelatedConst(TR_PCISCGraph *tgt, int32_t ctrl, uint16_t id, int dagId, int32_t val);
TR_PCISCNode *createIdiomArrayHeaderConst(TR_PCISCGraph *tgt, int32_t ctrl, uint16_t id, int dagId, TR::Compilation *c);
TR_PCISCNode *createIdiomArrayAddressInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *base, TR_PCISCNode *indexTree);
TR_PCISCNode *createIdiomI2LIfNecessary(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode **pred, TR_PCISCNode *node);
TR_PCISCNode *createIdiomArrayAddressIndexTreeInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *index, TR_PCISCNode *cmah, TR_PCISCNode *const2);
TR_PCISCNode *createIdiomArrayAddressInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *base, TR_PCISCNode *index, TR_PCISCNode *cmah, TR_PCISCNode *const2);
TR_PCISCNode *createIdiomCharArrayLoadInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *base, TR_PCISCNode *index, TR_PCISCNode *cmah, TR_PCISCNode *const2);
TR_PCISCNode *createIdiomArrayLoadInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, int opcode, TR::DataType, TR_PCISCNode *base, TR_PCISCNode *index, TR_PCISCNode *cmah, TR_PCISCNode *mulconst);
TR_PCISCNode *createIdiomArrayStoreBodyInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, int opcode, TR_PCISCNode *addr, TR_PCISCNode *storeval);
TR_PCISCNode *createIdiomCharArrayStoreBodyInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *addr, TR_PCISCNode *storeval);
TR_PCISCNode *createIdiomCharArrayStoreInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *base, TR_PCISCNode *index, TR_PCISCNode *cmah, TR_PCISCNode *const2, TR_PCISCNode *storeval);
TR_PCISCNode *createIdiomArrayStoreInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, int opcode, TR::DataType, TR_PCISCNode *base, TR_PCISCNode *index, TR_PCISCNode *cmah, TR_PCISCNode *const2, TR_PCISCNode *storeval);
TR_PCISCNode *createIdiomByteDirectArrayLoadInLoop(TR_PCISCGraph *tgt, int32_t ctrl, int dagId, TR_PCISCNode *pred, TR_PCISCNode *base, TR_PCISCNode *index);
TR_PCISCNode *createIdiomIDiv10InLoop(TR_PCISCGraph *tgt, int32_t ctrl, bool isDiv2Mul, int dagId, TR_PCISCNode *pred, TR_PCISCNode *src1, TR_PCISCNode *src2, TR_PCISCNode *c2, TR_PCISCNode *c31);
bool searchNodeInBlock(TR_CISCNode *start, TR_CISCNode *target);
bool checkSuccsSet(TR_CISCTransformer *const trans, TR_CISCNode *t, TR_BitVector *const pBV);
bool getThreeNodesForArray(TR_CISCNode *top, TR_CISCNode **ixload, TR_CISCNode **aload, TR_CISCNode **iload, bool allowArrayIndex = false);
bool testExitIF(int opcode, bool *isDecrement = NULL, int32_t *modLength = NULL, int32_t *modStartIdx = NULL);
bool testIConst(TR_CISCNode *n, int idx, int32_t value);
TR::Node* createI2LIfNecessary(TR::Compilation *comp, bool is64bit, TR::Node *child);
TR::Node* createLoadWithI2LIfNecessary(TR::Compilation *comp, bool is64bit, TR::Node *indexNode);
TR::Node* createBytesFromElement(TR::Compilation *comp, bool is64bit, TR::Node *indexNode, int multiply);
TR::Node* createIndexOffsetTree(TR::Compilation *comp, bool is64bit, TR::Node *indexNode, int multiply);
TR::Node* createLoad(TR::Node *baseNode);
TR::Node* createArrayHeaderConst(TR::Compilation *comp, bool is64bit, TR::Node *baseNode);
TR::Node* createArrayTopAddressTree(TR::Compilation *comp, bool is64bit, TR::Node *baseNode);
TR::Node* createArrayAddressTree(TR::Compilation *comp, bool is64bit, TR::Node *baseNode, TR::Node *indexNode, int multiply = 1);
TR::Node* createArrayLoad(TR::Compilation *comp, bool is64bit, TR::Node *ixload, TR::Node *baseNode, TR::Node *indexNode, int multiply = 1);
TR::Node* replaceIndexInAddressTree(TR::Compilation *comp, TR::Node *tree, TR::SymbolReference *symRef, TR::Node *newNode);
TR::Node* createTableLoad(TR::Compilation *comp, TR::Node* repNode, uint8_t inputSize, uint8_t outputSize, void *array, bool dispTrace);
TR::Node* createOP2(TR::Compilation *comp, TR::ILOpCodes op2, TR::Node* ch1, TR::Node* ch2);
TR::Node* createStoreOP2(TR::Compilation *comp, TR::Node* storeNode, TR::ILOpCodes op2, TR::Node* ch1, TR::Node* ch2);
TR::Node* createStoreOP2(TR::Compilation *comp, TR::SymbolReference* store, TR::ILOpCodes op2, TR::SymbolReference* ch1, TR::Node* ch2, TR::Node *rep);
TR::Node* createStoreOP2(TR::Compilation *comp, TR::SymbolReference* store, TR::ILOpCodes op2, TR::SymbolReference* ch1, TR::SymbolReference* ch2, TR::Node *rep);
TR::Node* createStoreOP2(TR::Compilation *comp, TR::SymbolReference* store, TR::ILOpCodes op2, TR::SymbolReference* ch1, int ch2Const, TR::Node *rep);
TR::Node* createMin(TR::Compilation *comp, TR::Node* x, TR::Node* y);
TR::Node* createMax(TR::Compilation *comp, TR::Node* x, TR::Node* y);
TR::Node* convertStoreToLoadWithI2LIfNecessary(TR::Compilation *comp, bool is64bit, TR::Node *indexNode);
TR::Node* convertStoreToLoad(TR::Compilation *comp, TR::Node *indexNode);
TR::Node* modifyArrayHeaderConst(TR::Compilation *comp, TR::Node *tree, int32_t offset);
bool getMultiplier(TR_CISCTransformer *trans, TR_CISCNode *mulConst, TR::Node **multiplier, int *elementSize, TR::DataType srcNodeType);
void getP2TTrRepNodes(TR_CISCTransformer *trans, TR::Node** array, int count);
void getP2TTrRepNodes(TR_CISCTransformer *trans, TR::Node** n1, TR::Node** n2);
void getP2TTrRepNodes(TR_CISCTransformer *trans, TR::Node** n1, TR::Node** n2, TR::Node** n3);
void getP2TTrRepNodes(TR_CISCTransformer *trans, TR::Node** n1, TR::Node** n2, TR::Node** n3, TR::Node** n4);
void getP2TTrRepNodes(TR_CISCTransformer *trans, TR::Node** n1, TR::Node** n2, TR::Node** n3, TR::Node** n4, TR::Node** n5);
void getP2TTrRepNodes(TR_CISCTransformer *trans, TR::Node** n1, TR::Node** n2, TR::Node** n3, TR::Node** n4, TR::Node** n5, TR::Node** n6);
void getP2TTrRepNodes(TR_CISCTransformer *trans, TR::Node** n1, TR::Node** n2, TR::Node** n3, TR::Node** n4, TR::Node** n5, TR::Node** n6, TR::Node** n7);
bool isFitTRTFunctionTable(uint8_t *table);
TR::Node* createTableAlignmentCheck(TR::Compilation *comp, TR::Node *tableNode, bool isByteSource, bool isByteTarget, bool tableBackedByRawStorage);
List<TR_CISCNode>* sortList(List<TR_CISCNode>* input, List<TR_CISCNode>* output, List<TR_CISCNode>* order, bool reverse = false);

void dump256Bytes(uint8_t *t, TR::Compilation * comp);

bool isLoopPreheaderLastBlockInMethod(TR::Compilation *comp, TR::Block *block, TR::Block **predBlock = NULL);
bool findAndOrReplaceNodesWithMatchingSymRefNumber(TR::Node *curNode, TR::Node *targetNode, int32_t symRefNumberToBeMatched);
TR::Node *findLoadWithMatchingSymRefNumber(TR::Node *curNode, int32_t symRefNumberToBeMatched);

#endif
