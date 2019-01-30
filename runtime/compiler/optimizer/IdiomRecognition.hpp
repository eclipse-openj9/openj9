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

#ifndef IDIOMRECOGNITION_INCL
#define IDIOMRECOGNITION_INCL

#include <stdint.h>
#include <string.h>
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/ILProps.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Flags.hpp"
#include "infra/HashTab.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/TRlist.hpp"
#include "optimizer/LoopCanonicalizer.hpp"
#include "optimizer/OptimizationManager.hpp"

class TR_CISCTransformer;
class TR_RegionStructure;
class TR_UseDefInfo;
namespace TR { class Block; }
namespace TR { class CFG; }
namespace TR { class CFGEdge; }
namespace TR { class CFGNode; }
namespace TR { class Node; }
namespace TR { class Optimization; }
namespace TR { class Optimizer; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }

typedef enum
   {
   TR_variable = TR::NumIlOps,
   TR_booltable,
   TR_entrynode,
   TR_exitnode,
   TR_allconst,
   TR_ahconst,          // constant for array header
   TR_variableORconst,
   TR_quasiConst,       // Currently, variable or constant or arraylength
   TR_quasiConst2,      // Currently, variable or constant, arraylength, or *iiload* (for non-array)
                        // We shouldn't use it in an idiom including iistore to *non-array* variable.
   TR_iaddORisub,       // addition or subtraction
   TR_conversion,       // all data conversions, such as b2i, c2i, i2l, ...
   TR_ifcmpall,         // all if instructions
   TR_ishrall,          // ishr and iushr
   TR_bitop1,           // AND, OR, and XOR
   TR_arrayindex,       // variable or addition
   TR_arraybase,        // variable or iaload
   TR_inbload,          // indirect non-byte load
   TR_inbstore,         // indirect non-byte store
   TR_indload,          // indirect load
   TR_indstore,         // indirect store
   TR_ibcload,          // indirect byte or char load
   TR_ibcstore          // indirect byte or char store
   } TR_CISCOps;

struct TrNodeInfo
   {
   TR::Block *_block;
   TR::Node *_node;
   TR::TreeTop *_treeTop;
   };


class TR_CISCNode
   {
public:
   TR_ALLOC(TR_Memory::LoopTransformer);
   TR_HeapMemory  trHeapMemory()  { return _trMemory; }

   bool isEqualOpc(TR_CISCNode *t);

   TR_CISCNode(TR_Memory * m, uint32_t opc, TR::DataType dt, uint16_t id, int16_t dagId, uint16_t ncfgs, uint16_t nchildren, TR_AllocationKind allocKind = heapAlloc)
      : _allocKind(allocKind), _trMemory(m), _preds(m), _parents(m), _dest(m), _chains(m), _hintChildren(m), _trNodeInfo(m), _dt(dt)
      {
      initialize(opc, id, dagId, ncfgs, nchildren);
      }

   TR_CISCNode(TR_Memory * m, uint32_t opc, TR::DataType dt, uint16_t id, int16_t dagId, uint16_t ncfgs, uint16_t nchildren, TR_CISCNode *pred, TR_AllocationKind allocKind = heapAlloc)
      : _allocKind(allocKind), _trMemory(m), _preds(m), _parents(m), _dest(m), _chains(m), _hintChildren(m), _trNodeInfo(m), _dt(dt)
      {
      initialize(opc, id, dagId, ncfgs, nchildren);
      pred->setSucc(0, this);
      }

   TR_CISCNode(TR_Memory * m, uint32_t opc, TR::DataType dt, uint16_t id, int16_t dagId, uint16_t ncfgs, uint16_t nchildren, TR_CISCNode *pred, TR_CISCNode *child0, TR_AllocationKind allocKind = heapAlloc)
      : _allocKind(allocKind), _trMemory(m), _preds(m), _parents(m), _dest(m), _chains(m), _hintChildren(m), _trNodeInfo(m), _dt(dt)
      {
      initialize(opc, id, dagId, ncfgs, nchildren);
      pred->setSucc(0, this);
      setChild(0, child0);
      }

   TR_CISCNode(TR_Memory * m, uint32_t opc, TR::DataType dt, uint16_t id, int16_t dagId, uint16_t ncfgs, uint16_t nchildren, TR_CISCNode *pred, TR_CISCNode *child0, TR_CISCNode *child1, TR_AllocationKind allocKind = heapAlloc)
      : _allocKind(allocKind), _trMemory(m), _preds(m), _parents(m), _dest(m), _chains(m), _hintChildren(m), _trNodeInfo(m), _dt(dt)
      {
      initialize(opc, id, dagId, ncfgs, nchildren);
      pred->setSucc(0, this);
      setChildren(child0, child1);
      }

   TR_CISCNode(TR_Memory * m, uint32_t opc, TR::DataType dt, uint16_t id, int16_t dagId, uint16_t ncfgs, uint16_t nchildren, int32_t otherInfo, TR_AllocationKind allocKind = heapAlloc)
   : _allocKind(allocKind), _trMemory(m), _preds(m), _parents(m), _dest(m), _chains(m), _hintChildren(m), _trNodeInfo(m), _dt(dt)
      {
      initialize(opc, id, dagId, ncfgs, nchildren, otherInfo);
      }

   TR_CISCNode(TR_Memory * m, uint32_t opc, TR::DataType dt, uint16_t id, int16_t dagId, uint16_t ncfgs, uint16_t nchildren, int32_t otherInfo, TR_CISCNode *pred, TR_AllocationKind allocKind = heapAlloc)
   : _allocKind(allocKind), _trMemory(m), _preds(m), _parents(m), _dest(m), _chains(m), _hintChildren(m), _trNodeInfo(m), _dt(dt)
      {
      initialize(opc, id, dagId, ncfgs, nchildren, otherInfo);
      pred->setSucc(0, this);
      }

   void initialize(uint32_t opc, uint16_t id, int16_t dagId, uint16_t ncfgs, uint16_t nchildren)
      {
      initializeMembers(opc, id, dagId, ncfgs, nchildren);
      allocArrays(ncfgs, nchildren);
      }

   void initializeMembers(uint32_t opc, uint16_t id, int16_t dagId, uint16_t ncfgs, uint16_t nchildren);
   void initialize(uint32_t opc, uint16_t id, int16_t dagId, uint16_t ncfgs, uint16_t nchildren, int32_t otherInfo)
      {
      initialize(opc, id, dagId, ncfgs, nchildren);
      setOtherInfo(otherInfo);
      }

   TR_Memory * trMemory() { return _trMemory; }

   void setOtherInfo(int32_t otherInfo)
      {
      _flags.set(_isValidOtherInfo);
      _otherInfo = otherInfo;
      checkFlags();
      }

   void checkFlags()
      {
      if (isValidOtherInfo())
         {
         switch(_opcode)
            {
            case TR::iconst:
            case TR::cconst:
            case TR::sconst:
            case TR::bconst:
            case TR::lconst:
               setIsInterestingConstant();
               break;
            }
         }
      }

   void replaceSucc(uint32_t index, TR_CISCNode *to);
   inline void setSucc(uint32_t index, TR_CISCNode *ch)
      {
      TR_ASSERT(index < _numSuccs, "TR_CISCNode::setSucc index out of range");
      _succs[index] = ch;
      ch->addPred(this);
      }

   void setSucc(TR_CISCNode *ch)
      {
      setSucc(0, ch);
      }

   void setSuccs(TR_CISCNode *ch1, TR_CISCNode *ch2)
      {
      setSucc(0, ch1);
      setSucc(1, ch2);
      }

   void replaceChild(uint32_t index, TR_CISCNode *ch);
   inline void setChild(uint32_t index, TR_CISCNode *ch)
      {
      TR_ASSERT(index < _numChildren, "TR_CISCNode::setChild index out of range");
      _children[index] = ch;
      ch->addParent(this);
      }

   void setChild(TR_CISCNode *ch)
      {
      setChild(0, ch);
      }

   void setChildren(TR_CISCNode *ch1, TR_CISCNode *ch2)
      {
      setChild(0, ch1);
      setChild(1, ch2);
      }

   inline TR_CISCNode *getSucc(uint32_t index)
      {
      TR_ASSERT(index < _numSuccs, "TR_CISCNode::getSucc index out of range");
      return _succs[index];
      }

   inline TR_CISCNode *getChild(uint32_t index)
      {
      TR_ASSERT(index < _numChildren, "TR_CISCNode::getChild index out of range");
      return _children[index];
      }

   TR_CISCNode *getNodeIfSingleChain() { return _chains.isSingleton() ? _chains.getListHead()->getData() : 0; }

   inline uint16_t  getChildID(uint32_t index) { return getChild(index)->_id; }
   inline uint32_t getOpcode() { return _opcode; }
   inline const TR::ILOpCode& getIlOpCode() { return _ilOpCode; }
   inline TR::DataType getDataType() { return _dt; }
   inline uint16_t  getNumChildren() { return _numChildren; }
   inline uint16_t  getNumSuccs() { return _numSuccs; }
   inline uint16_t  getID() { return _id; }
   inline uint16_t  getDagID() { return _dagId; }
   inline void setDagID(int16_t d) { _dagId = d; }
   inline int32_t getOtherInfo() { return _otherInfo; }
   void initChains() { _chains.init(); }

   // flags
   bool isValidOtherInfo()                      { return _flags.testAny(_isValidOtherInfo); }
   void setIsStoreDirect(bool v = true)         { _flags.set(_isStoreDirect, v); }
   bool isStoreDirect()                         { return _flags.testAny(_isStoreDirect); }
   void setIsLoadVarDirect(bool v = true)       { _flags.set(_isLoadVarDirect, v); }
   bool isLoadVarDirect()                       { return _flags.testAny(_isLoadVarDirect); }
   void setIsNegligible(bool v = true)          { _flags.set(_isNegligible, v); }
   bool isNegligible()                          { return _flags.testAny(_isNegligible); }
   void setIsSuccSimplyConnected(bool v = true) { _flags.set(_isSuccSimplyConnected, v); }
   bool isSuccSimplyConnected()                 { return _flags.testAny(_isSuccSimplyConnected); }
   void setIsPredSimplyConnected(bool v = true) { _flags.set(_isPredSimplyConnected, v); }
   bool isPredSimplyConnected()                 { return _flags.testAny(_isPredSimplyConnected); }
   void setIsChildSimplyConnected(bool v = true) { _flags.set(_isChildSimplyConnected, v); }
   bool isChildSimplyConnected()                        { return _flags.testAny(_isChildSimplyConnected); }
   void setIsParentSimplyConnected(bool v = true) { _flags.set(_isParentSimplyConnected, v); }
   bool isParentSimplyConnected()                       { return _flags.testAny(_isParentSimplyConnected); }
   void setIsEssentialNode(bool v = true)       { _flags.set(_isEssentialNode, v); }
   bool isEssentialNode()                       { return _flags.testAny(_isEssentialNode); }
   void setIsOptionalNode(bool v = true)        { _flags.set(_isOptionalNode, v); }
   bool isOptionalNode()                        { return _flags.testAny(_isOptionalNode); }
   void setIsChildDirectlyConnected(bool v = true)      { _flags.set(_isChildDirectlyConnected, v); }
   bool isChildDirectlyConnected()                      { return _flags.testAny(_isChildDirectlyConnected); }
   void setIsSuccDirectlyConnected(bool v = true)       { _flags.set(_isSuccDirectlyConnected, v); }
   bool isSuccDirectlyConnected()                       { return _flags.testAny(_isSuccDirectlyConnected); }
   void setIsInterestingConstant(bool v = true)         { _flags.set(_isInterestingConstant, v); }
   bool isInterestingConstant()                 { return _flags.testAny(_isInterestingConstant); }
   void setIsNecessaryScreening(bool v = true)  { _flags.set(_isNecessaryScreening, v); }
   bool isNecessaryScreening()                  { return _flags.testAny(_isNecessaryScreening); }
   void setIsLightScreening(bool v = true)  { _flags.set(_isLightScreening, v); }
   bool isLightScreening()                  { return _flags.testAny(_isLightScreening); }
   bool isDataConnected()                       { return _flags.testAll(_isChildSimplyConnected |
                                                                        _isParentSimplyConnected); }
   bool isCFGConnected()                        { return _flags.testAll(_isSuccSimplyConnected |
                                                                        _isPredSimplyConnected); }
   bool isCFGDataConected()                     { return _flags.testAll(_isSuccSimplyConnected |
                                                                        _isPredSimplyConnected |
                                                                        _isChildSimplyConnected |
                                                                        _isParentSimplyConnected); }
   void setOutsideOfLoop(bool v = true)         { _flags.set(_isOutsideOfLoop, v); }
   bool isOutsideOfLoop()                       { return _flags.testAny(_isOutsideOfLoop); }
   void setCISCNodeModified(bool v = true)         { _flags.set(_isCISCNodeModified, v); }
   bool isCISCNodeModified()                       { return _flags.testAny(_isCISCNodeModified); }
   void setNewCISCNode(bool v = true)         { _flags.set(_isNewCISCNode, v); }
   bool isNewCISCNode()                       { return _flags.testAny(_isNewCISCNode); }
   void setSkipParentsCheck(bool v = true)         { _flags.set(_isSkipParentsCheck, v); }
   bool isSkipParentsCheck()                       { return _flags.testAny(_isSkipParentsCheck); }

   // short cut
   bool isCommutative()                         { return _ilOpCode.isCommutative(); }

   TR_CISCNode *getLatestDest()
      {
      TR_ASSERT( _opcode == TR_variable, "TR_CISCNode::getLatestDest()");
      return _latestDest;
      }
   //bool checkParents(TR_CISCNode *, const int8_t level);
   static bool checkParentsNonRec(TR_CISCNode *, TR_CISCNode *, int8_t level, TR::Compilation *);
   void reverseBranchOpCodes();

   static const char *getName(TR_CISCOps, TR::Compilation *);
   void dump(TR::FILE *pOutFile, TR::Compilation *);
   void printStdout();

   void addChain(TR_CISCNode *tgt, bool alsoAddChainsTgt = false)
      {
      _chains.add(tgt);
      if (alsoAddChainsTgt) tgt->_chains.add(this);
      }

   enum // flag bits
      {
      _isValidOtherInfo           = 0x00000001,
      _isStoreDirect              = 0x00000002,
      _isNegligible               = 0x00000004,
      _isSuccSimplyConnected      = 0x00000008,
      _isPredSimplyConnected      = 0x00000010,
      _isChildSimplyConnected     = 0x00000020,
      _isParentSimplyConnected    = 0x00000040,
      _isLoadVarDirect            = 0x00000080,
      _isEssentialNode            = 0x00000100,
      _isOptionalNode             = 0x00000200,
      _isChildDirectlyConnected   = 0x00000400,
      _isSuccDirectlyConnected    = 0x00000800,
      _isInterestingConstant      = 0x00001000,
      _isNecessaryScreening       = 0x00002000,
      _isLightScreening           = 0x00004000,
      _isOutsideOfLoop            = 0x00008000,
      _isCISCNodeModified         = 0x00010000,
      _isNewCISCNode              = 0x00020000,
      _isSkipParentsCheck         = 0x00040000,
      _dummyEnum
      };


   void initializeLists()
      {
      _preds.init();
      _parents.init();
      _dest.init();
      _hintChildren.init();
      _trNodeInfo.init();
      initChains();
      }

   virtual void allocArrays(uint16_t ncfgs, uint16_t nchildren)
      {
      _succs = (TR_CISCNode **)(ncfgs ? trMemory()->allocateMemory(ncfgs * sizeof(*_succs), _allocKind) : 0);
      _children = (TR_CISCNode **)(nchildren ? trMemory()->allocateMemory(nchildren * sizeof(*_children), _allocKind) : 0);
      }

   virtual void addPred(TR_CISCNode *t) { _preds.add(t); }
   virtual void addParent(TR_CISCNode *t)       { _parents.add(t); }
   List<TR_CISCNode> *getPreds() { return &_preds; }
   TR_CISCNode *getHeadOfPredecessors() { return _preds.getListHead()->getData(); }
   List<TR_CISCNode> *getParents() { return &_parents; }
   TR_CISCNode *getHeadOfParents() { return _parents.getListHead()->getData(); }

   void addTrNode(TR::Block *b, TR::TreeTop *t, TR::Node *n)
      {
      TrNodeInfo *newInfo = (TrNodeInfo *)trMemory()->allocateMemory(sizeof(*newInfo), _allocKind);
      newInfo->_block = b;
      newInfo->_treeTop = t;
      newInfo->_node = n;
      _trNodeInfo.add(newInfo);
      }
   List<TrNodeInfo> *getTrNodeInfo() { return &_trNodeInfo; }
   inline TrNodeInfo *getHeadOfTrNodeInfo() { return _trNodeInfo.getListHead()->getData(); }
   inline TR::Node *getHeadOfTrNode() { return _trNodeInfo.getListHead()->getData()->_node; }
   inline TR::TreeTop *getHeadOfTreeTop() { return _trNodeInfo.getListHead()->getData()->_treeTop; }
   inline TR::Block *getHeadOfBlock() { return _trNodeInfo.getListHead()->getData()->_block; }

   void setDest(TR_CISCNode *v)
      {
      TR_ASSERT(v != 0 && v->_opcode == TR_variable, "TR_CISCNode::setDest(), input error");
      if (v->_latestDest)
         {
         TR_ASSERT(v->_latestDest->_dest.find(v), "TR_CISCNode::setDest(), input variable is not found in _latestDest");
         v->_latestDest->_dest.remove(v);
         }
      ListAppender<TR_CISCNode> appender(&_dest);
      appender.add(v);
      v->_latestDest = this;
      }
   TR_CISCNode *getFirstDest()
      {
      return _dest.isEmpty() ? 0 : _dest.getListHead()->getData();
      }
   List<TR_CISCNode> *getHintChildren() { return &_hintChildren; }
   void addHint(TR_CISCNode *n) { _hintChildren.add(n); }
   bool isEmptyHint() { return _hintChildren.isEmpty(); }
   List<TR_CISCNode> *getChains() {return &_chains; }
   bool checkDagIdInChains();
   TR::TreeTop *getDestination(bool isFallThrough = false);
   void deadAllChildren();
   TR_CISCNode *duplicate()
      {
      TR_CISCNode *ret = new (trHeapMemory()) TR_CISCNode(_trMemory, _opcode, _dt, _id, _dagId, _numSuccs, _numChildren);
      ret->_otherInfo = _otherInfo;
      ret->_flags = _flags;

      // duplicate _trNodeIndo
      ListIterator<TrNodeInfo> li(&_trNodeInfo);
      ListAppender<TrNodeInfo> appender(&ret->_trNodeInfo);
      for (TrNodeInfo *t = li.getFirst(); t; t = li.getNext()) appender.add(t);

      // Duplicating other lists will be performed in the caller.

      return ret;
      }

   // Variables
protected:
   void setOpcode(uint32_t opc)
      {
      _opcode = opc;
      _ilOpCode.setOpCodeValue(opc < TR::NumIlOps ? (TR::ILOpCodes)opc : TR::BadILOp);
      }
   uint32_t _opcode;    // TR::ILOpCodes enum
   TR::ILOpCode _ilOpCode;
   TR::DataType _dt;
   TR_CISCNode **_succs;
   TR_CISCNode **_children;
   TR_CISCNode *_latestDest;
   int32_t  _otherInfo;
   uint16_t _numSuccs;
   uint16_t _numChildren;

   uint16_t _id;
   uint16_t  _dagId;
   flags32_t _flags;

   TR_AllocationKind _allocKind;
   TR_Memory *       _trMemory;
   List<TR_CISCNode> _preds;
   List<TR_CISCNode> _parents;
   List<TR_CISCNode> _dest; // destination operands
   List<TR_CISCNode> _chains;
   List<TR_CISCNode> _hintChildren;
   List<TrNodeInfo> _trNodeInfo;
   };


// Use persistent allocation
class TR_PCISCNode : public TR_CISCNode
   {
public:
   TR_PCISCNode(TR_Memory * m, uint32_t opc, TR::DataType dt, uint16_t id, int16_t dagId, uint16_t ncfgs, uint16_t nchildren)
      : TR_CISCNode(m, opc, dt, id, dagId, ncfgs, nchildren, persistentAlloc)
      {
      };

   TR_PCISCNode(TR_Memory * m, uint32_t opc, TR::DataType dt, uint16_t id, int16_t dagId, uint16_t ncfgs, uint16_t nchildren, TR_CISCNode *pred)
      : TR_CISCNode(m, opc, dt, id, dagId, ncfgs, nchildren, pred, persistentAlloc)
      {
      }

   TR_PCISCNode(TR_Memory * m, uint32_t opc, TR::DataType dt, uint16_t id, int16_t dagId, uint16_t ncfgs, uint16_t nchildren, TR_CISCNode *pred, TR_CISCNode *child0)
      : TR_CISCNode(m, opc, dt, id, dagId, ncfgs, nchildren, pred, child0, persistentAlloc)
      {
      }

   TR_PCISCNode(TR_Memory * m, uint32_t opc, TR::DataType dt, uint16_t id, int16_t dagId, uint16_t ncfgs, uint16_t nchildren, TR_CISCNode *pred, TR_CISCNode *child0, TR_CISCNode *child1)
      : TR_CISCNode(m, opc, dt, id, dagId, ncfgs, nchildren, pred, child0, child1, persistentAlloc)
      {
      }

   TR_PCISCNode(TR_Memory * m, uint32_t opc, TR::DataType dt, uint16_t id, int16_t dagId, uint16_t ncfgs, uint16_t nchildren, int32_t otherInfo)
      : TR_CISCNode(m, opc, dt, id, dagId, ncfgs, nchildren, otherInfo, persistentAlloc)
      {
      }

   TR_PCISCNode(TR_Memory * m, uint32_t opc, TR::DataType dt, uint16_t id, int16_t dagId, uint16_t ncfgs, uint16_t nchildren, int32_t otherInfo, TR_CISCNode *pred)
      : TR_CISCNode(m, opc, dt, id, dagId, ncfgs, nchildren, otherInfo, pred, persistentAlloc)
      {
      }

   TR_PCISCNode *getSucc(uint32_t index)
      {
      return (TR_PCISCNode *) TR_CISCNode::getSucc(index);
      }

   TR_PCISCNode *getChild(uint32_t index)
      {
      return (TR_PCISCNode *) TR_CISCNode::getChild(index);
      }

   virtual void addPred(TR_CISCNode *t)
      {
      TR_PersistentList<TR_CISCNode> *p = (TR_PersistentList<TR_CISCNode> *)&_preds;
      p->add(t);
      }
   virtual void addParent(TR_CISCNode *t)
      {
      TR_PersistentList<TR_CISCNode> *p = (TR_PersistentList<TR_CISCNode> *)&_parents;
      p->add(t);
      }
   virtual void allocArrays(uint16_t ncfgs, uint16_t nchildren)
      {
      _succs = (TR_CISCNode **)(ncfgs ? jitPersistentAlloc(ncfgs * sizeof(*_succs)) : 0);
      _children = (TR_CISCNode **)(nchildren ? jitPersistentAlloc(nchildren * sizeof(*_children)) : 0);
      }
   };


class TR_CISCHash
   {
   public:
   TR_ALLOC(TR_Memory::LoopTransformer);

   TR_CISCHash(TR_Memory * m, uint32_t num = 0, TR_AllocationKind allocKind = heapAlloc)
      :_trMemory(m)
      {
      if (num > 0)
         setNumBuckets(num, allocKind);
      else
         {
         _numBuckets = 0;
         _buckets = 0;
         _allocationKind = allocKind;
         }
      }

   TR_Memory * trMemory() { return _trMemory; }

   void setNumBuckets(uint32_t num, TR_AllocationKind allocKind)
      {
      _allocationKind = allocKind;
      setNumBuckets(num);
      }

   void setNumBuckets(uint32_t num)
      {
      TR_ASSERT(num > 0, "error: num == 0!");
      _numBuckets = num;
      uint32_t size = num * sizeof(*_buckets);
      _buckets = (HashTableEntry **)trMemory()->allocateMemory(size, _allocationKind);
      memset(_buckets, 0, size);
      }

   struct HashTableEntry
      {
      HashTableEntry *_next;
      uint64_t        _key;
      TR_CISCNode    *_node;
      };

   TR_CISCNode *find(uint64_t key);
   bool add(uint64_t key, TR_CISCNode *, bool checkExist = false);
   uint32_t getNumBuckets() { return _numBuckets; }

   private:

   uint32_t          _numBuckets;
   HashTableEntry    **_buckets;
   TR_Memory *       _trMemory;
   TR_AllocationKind _allocationKind;
   };


enum TR_GraphAspects // flag bits
   {
   storeShiftCount      = 12,
   nbValue              = (0xFF & ~ILTypeProp::Size_1),
   existAccess          = 0x00000100,
   loadMasks            = 0x000001FF,
   storeMasks           = 0x001FF000,
   sameTypeLoadStore    = 0x00200000,

   bitop1               = 0x00800000,
   iadd                 = 0x01000000,
   isub                 = 0x02000000,
   call                 = 0x04000000,
   shr                  = 0x08000000,
   bndchk               = 0x10000000,
   reminder             = 0x20000000,
   division             = 0x40000000,
   mul                  = 0x80000000
   };

class TR_CISCGraphAspects : public flags32_t
   {
public:

   TR_CISCGraphAspects() {init();}
   void init() {clear();}
   void setLoadAspects(uint32_t val, bool orExistAccess = true);   // assuming val as a combination of ILTypeProp::*
   void setStoreAspects(uint32_t val, bool orExistAccess = true);  // assuming val as a combination of ILTypeProp::*
   void modifyAspects();
   uint32_t getLoadAspects() { return getValue(loadMasks); }
   uint32_t getStoreAspects() { return getValue(storeMasks) >> storeShiftCount; }
   virtual void print(TR::Compilation *, bool noaspects);
   };


class TR_CISCGraphAspectsWithCounts : public TR_CISCGraphAspects
   {
public:

   TR_CISCGraphAspectsWithCounts() {init();}
   void init() {TR_CISCGraphAspects::init(); setMinCounts(0,0,0);}
   void setAspectsByOpcode(int opc);
   void setAspectsByOpcode(TR_CISCNode *n) { setAspectsByOpcode(n->getOpcode()); }
   void print(TR::Compilation *, bool noaspects);

   void setMinCounts(uint8_t ifCount, uint8_t iloadCount, uint8_t istoreCount)
      { _ifCount = ifCount; _indirectLoadCount = iloadCount; _indirectStoreCount = istoreCount; }
   uint8_t incIfCount() { return ++_ifCount; }
   uint8_t incIndirectLoadCount() { return ++_indirectLoadCount; }
   uint8_t incIndirectStoreCount() { return ++_indirectStoreCount; }
   uint8_t getIfCount() { return _ifCount; }
   uint8_t getIndirectLoadCount() { return _indirectLoadCount; }
   uint8_t getIndirectStoreCount() { return _indirectStoreCount; }
   bool meetMinCounts(TR_CISCGraphAspectsWithCounts *p) // p is for the pattern
      {
      return (_ifCount >= p->_ifCount &&
              _indirectLoadCount >= p->_indirectLoadCount &&
              _indirectStoreCount >= p->_indirectStoreCount);
      }

protected:
   uint8_t _ifCount;
   uint8_t _indirectLoadCount;
   uint8_t _indirectStoreCount;
   };



template <class T> class TR_ListDuplicator
   {
public:
   TR_ALLOC(TR_Memory::LoopTransformer);
   TR_HeapMemory  trHeapMemory()  { return _trMemory; }

   TR_ListDuplicator<T>(TR_Memory * m)
      : _trMemory(m), _list(0)
         {
         _flags.clear();
         }

   virtual void setList(List<T>* l) { setRegistered(); setDuplicated(false); _list = l; }
   List<T>* getList() { TR_ASSERT(isRegistered(), "error"); return _list; }
   virtual List<T>* duplicateList(bool ifNecessary = true)
      {
      if (ifNecessary && isDuplicated()) return _list;
      setDuplicated();
      List<T>* ret = new (trHeapMemory()) List<T>(_trMemory);
      ListAppender<T> appender(ret);
      ListIterator<T> li(_list);
      T *t;
      for (t = li.getFirst(); t; t = li.getNext()) appender.add(t);
      _list = ret;
      return ret;
      }

   enum // flag bits
      {
      _isRegistered            = 0x0001,
      _isDuplicated            = 0x0002,
      _isDuplicateElement      = 0x0004,
      _dummyEnum
      };

   void setRegistered(bool v = true)                     { _flags.set(_isRegistered, v); }
   bool isRegistered()                                   { return _flags.testAny(_isRegistered); }
   void setDuplicated(bool v = true)                     { _flags.set(_isDuplicated, v); }
   bool isDuplicated()                                   { return _flags.testAny(_isDuplicated); }
   void setDuplicateElement(bool v = true)               { _flags.set(_isDuplicateElement, v); }
   bool isDuplicateElement()                             { return _flags.testAny(_isDuplicateElement); }

protected:
   List<T> *_list;
   flags8_t _flags;
   TR_Memory * _trMemory;
   };


class ListOfCISCNodeDuplicator : public TR_ListDuplicator<TR_CISCNode>
   {
public:
   ListOfCISCNodeDuplicator(TR_Memory * m) : TR_ListDuplicator<TR_CISCNode>(m), _mapping(m) {};
   virtual void setList(List<TR_CISCNode>* l)
      {
      TR_ListDuplicator<TR_CISCNode>::setList(l);
      _mapping.init();
      }
   virtual List<TR_CISCNode> *duplicateList(bool ifNecessary = true)
      {
      if (ifNecessary && isDuplicated()) return _list;
      setDuplicated();
      List<TR_CISCNode>* ret = new (trHeapMemory()) List<TR_CISCNode>(_trMemory);
      ListAppender<TR_CISCNode> appender(ret);
      ListIterator<TR_CISCNode> li(_list);
      TR_CISCNode *t;
      _list = ret;
      if (isDuplicateElement())
         {
         TR_CISCNode *newCISC;
         for (t = li.getFirst(); t; t = li.getNext())
            {
            newCISC = t->duplicate();
            appender.add(newCISC);
            TR_Pair<TR_CISCNode,TR_CISCNode> *pair = new (trHeapMemory()) TR_Pair<TR_CISCNode,TR_CISCNode>(t, newCISC);
            _mapping.add(pair);
            }
         ListIterator<TR_CISCNode> liNew(ret);
         for (t = li.getFirst(), newCISC = liNew.getFirst(); t; t = li.getNext(), newCISC = liNew.getNext())
            {
            int32_t i;
            for (i = 0; i < t->getNumChildren(); i++)
               newCISC->setChild(i, findCorrespondingNode(t->getChild(i)));
            for (i = 0; i < t->getNumSuccs(); i++)
               newCISC->setSucc(i, findCorrespondingNode(t->getSucc(i)));
            restructureList(t->getChains(), newCISC->getChains());
            restructureList(t->getHintChildren(), newCISC->getHintChildren());
            }
         }
      else
         {
         for (t = li.getFirst(); t; t = li.getNext()) { appender.add(t); }
         }
      return ret;
      }

   TR_CISCNode *findCorrespondingNode(TR_CISCNode *old)
      {
      TR_Pair<TR_CISCNode,TR_CISCNode> *pair;
      ListElement<TR_Pair<TR_CISCNode,TR_CISCNode> > *le;
      // Search for old
      for (le = _mapping.getListHead(); le; le = le->getNextElement())
         {
         pair = le->getData();
         if (pair->getKey() == old) return pair->getValue();
         }
      TR_ASSERT(false, "error");
      return NULL;
      }

   void restructureList(List<TR_CISCNode> *oldList, List<TR_CISCNode> *newList)
      {
      TR_ASSERT(newList->isEmpty(), "error");
      ListAppender<TR_CISCNode> appender(newList);
      ListIterator<TR_CISCNode> li(oldList);
      TR_CISCNode *t;
      for (t = li.getFirst(); t; t = li.getNext()) appender.add(findCorrespondingNode(t));
      }

protected:
   List<TR_Pair<TR_CISCNode,TR_CISCNode> > _mapping;
   };



#define MAX_IMPORTANT_NODES 8
#define MAX_SPECIALCARE_NODES 4
typedef    bool (* TransformerPtr)(TR_CISCTransformer *);
typedef    bool (* SpecialNodeTransformerPtr)(TR_CISCTransformer *);
class TR_CISCGraph
   {
public:
   TR_ALLOC(TR_Memory::LoopTransformer);

   static void setEssentialNodes(TR_CISCGraph*);
   static void makePreparedCISCGraphs(TR::Compilation *c);

   TR_CISCGraph(TR_Memory * m, const char *title = 0, int32_t numHashTrNode = 31, int32_t numHashOpc = 17)
      : _titleOfCISC(title), _entryNode(0), _exitNode(0), _numNodes(0), _numDagIds(0),
        _dagId2Nodes(0), _transformer(0), _specialNodeTransformer(0), _hotness(noOpt),
        _javaSource(0), _versionLength(0), _trMemory(m),
        _trNode2CISCNode(m), _opc2CISCNode(m), _nodes(m), _orderByData(m),
        _duplicatorNodes(m), _patternType(-1)
      {
      initializeLists();
      _flags.clear();
      _aspects.init();
      if (numHashTrNode > 0) _trNode2CISCNode.setNumBuckets(numHashTrNode);
      if (numHashOpc > 0) _opc2CISCNode.setNumBuckets(numHashOpc);
      int32_t i;
      for (i = MAX_IMPORTANT_NODES; --i >= 0; ) _importantNode[i] = 0;
      for (i = MAX_SPECIALCARE_NODES; --i >= 0; ) _specialCareNode[i] = 0;
      }

   TR_CISCNode *getCISCNode(TR::Node *n) { return _trNode2CISCNode.find(getHashKeyTrNode(n)); }
   TR_CISCNode *getCISCNode(uint32_t opc, bool validOther, int32_t other)
      {
      return _opc2CISCNode.find(getHashKeyOpcs(opc, validOther, other));
      }

   void setEntryNode(TR_CISCNode *n) {_entryNode = n;}
   TR_CISCNode *getEntryNode() {return _entryNode;}

   void setExitNode(TR_CISCNode *n) {_exitNode = n;}
   TR_CISCNode *getExitNode() {return _exitNode;}

   void setImportantNode(uint32_t idx, TR_CISCNode *n)
      {
      TR_ASSERT(idx < MAX_IMPORTANT_NODES, "TR_CISCGraph::setImportantNode index out of range");
      _importantNode[idx] = n;
      }
   void setImportantNodes(TR_CISCNode *n1, TR_CISCNode *n2)
      {
      setImportantNode(0, n1);
      setImportantNode(1, n2);
      }
   void setImportantNodes(TR_CISCNode *n1, TR_CISCNode *n2, TR_CISCNode *n3)
      {
      setImportantNode(0, n1);
      setImportantNode(1, n2);
      setImportantNode(2, n3);
      }
   void setImportantNodes(TR_CISCNode *n1, TR_CISCNode *n2, TR_CISCNode *n3, TR_CISCNode *n4)
      {
      setImportantNode(0, n1);
      setImportantNode(1, n2);
      setImportantNode(2, n3);
      setImportantNode(3, n4);
      }
   void setImportantNodes(TR_CISCNode *n1, TR_CISCNode *n2, TR_CISCNode *n3, TR_CISCNode *n4, TR_CISCNode *n5)
      {
      setImportantNode(0, n1);
      setImportantNode(1, n2);
      setImportantNode(2, n3);
      setImportantNode(3, n4);
      setImportantNode(4, n5);
      }
   void setImportantNodes(TR_CISCNode *n1, TR_CISCNode *n2, TR_CISCNode *n3, TR_CISCNode *n4, TR_CISCNode *n5, TR_CISCNode *n6)
      {
      setImportantNode(0, n1);
      setImportantNode(1, n2);
      setImportantNode(2, n3);
      setImportantNode(3, n4);
      setImportantNode(4, n5);
      setImportantNode(5, n6);
      }
   void setImportantNodes(TR_CISCNode *n1, TR_CISCNode *n2, TR_CISCNode *n3, TR_CISCNode *n4, TR_CISCNode *n5, TR_CISCNode *n6, TR_CISCNode *n7)
      {
      setImportantNode(0, n1);
      setImportantNode(1, n2);
      setImportantNode(2, n3);
      setImportantNode(3, n4);
      setImportantNode(4, n5);
      setImportantNode(5, n6);
      setImportantNode(6, n7);
      }
   void setImportantNodes(TR_CISCNode *n1, TR_CISCNode *n2, TR_CISCNode *n3, TR_CISCNode *n4, TR_CISCNode *n5, TR_CISCNode *n6, TR_CISCNode *n7, TR_CISCNode *n8)
      {
      setImportantNode(0, n1);
      setImportantNode(1, n2);
      setImportantNode(2, n3);
      setImportantNode(3, n4);
      setImportantNode(4, n5);
      setImportantNode(5, n6);
      setImportantNode(6, n7);
      setImportantNode(7, n8);
      }
   TR_CISCNode *getImportantNode(uint32_t idx)
      {
      TR_ASSERT(idx < MAX_IMPORTANT_NODES, "TR_CISCGraph::getImportantNode index out of range");
      return _importantNode[idx];
      }

   void setSpecialCareNode(uint32_t idx, TR_CISCNode *n)
      {
      TR_ASSERT(idx < MAX_SPECIALCARE_NODES, "TR_CISCGraph::setSpecialCareNode index out of range");
      _specialCareNode[idx] = n;
      }
   TR_CISCNode *getSpecialCareNode(uint32_t idx)
      {
      TR_ASSERT(idx < MAX_SPECIALCARE_NODES, "TR_CISCGraph::getSpecialCareNode index out of range");
      return _specialCareNode[idx];
      }

   TR_Memory * trMemory() { return _trMemory; }

   uint16_t getNumNodes() {return _numNodes;}
   uint16_t incNumNodes() {return _numNodes++;}

   uint16_t  getNumDagIds() {return _numDagIds;}
   void setNumDagIds(uint16_t n) {_numDagIds = n;}

   TR_CISCNode *getHeadOfNodes()
      {
      return _nodes.isEmpty() ? 0 : _nodes.getListHead()->getData();
      }
   void addTrNode(TR_CISCNode *n, TR::Block *block, TR::TreeTop *top, TR::Node *trNode);
   virtual void addNode(TR_CISCNode *n, TR::Block *block = 0, TR::TreeTop *top = 0, TR::Node *trNode = 0);

   void createInternalData(int32_t loopBodyDagId)
      {
      createDagId2NodesTable();
      createOrderByData();
      setOutsideOfLoopFlag(loopBodyDagId);
      }

   bool isDagIdCycle(uint16_t dagId) { TR_ASSERT(dagId < _numDagIds, "error"); return getDagId2Nodes()[dagId].isMultipleEntry(); }
   bool isDagIdDag(uint16_t dagId) { TR_ASSERT(dagId < _numDagIds, "error"); return getDagId2Nodes()[dagId].isSingleton(); }
   void dump(TR::FILE *pOutFile, TR::Compilation *);

   void importUDchains(TR::Compilation *comp, TR_UseDefInfo *useDefInfo, bool reinitialize = false);

   void setTransformer(TransformerPtr t) { _transformer = t; }
   TransformerPtr getTransformer() { return _transformer; }
   bool isPatternGraph() { return _transformer != 0; }

   void setSpecialNodeTransformer(SpecialNodeTransformerPtr t) { _specialNodeTransformer = t; }
   SpecialNodeTransformerPtr getSpecialNodeTransformer() { return _specialNodeTransformer; }

   // flags
   bool isSetUDDUchains()                               { return _flags.testAny(_isSetUDDUchains); }
   void setIsSetUDDUchains(bool v = true)               { _flags.set(_isSetUDDUchains, v); }
   bool isInhibitAfterVersioning()                      { TR_ASSERT(isPatternGraph(), "error"); return _flags.testAny(_inhibitAfterVersioning); }
   void setInhibitAfterVersioning(bool v = true)        { TR_ASSERT(isPatternGraph(), "error"); _flags.set(_inhibitAfterVersioning, v); }
   bool isInhibitBeforeVersioning()                     { TR_ASSERT(isPatternGraph(), "error"); return _flags.testAny(_inhibitBeforeVersioning); }
   void setInhibitBeforeVersioning(bool v = true)       { TR_ASSERT(isPatternGraph(), "error"); _flags.set(_inhibitBeforeVersioning, v); }
   bool isInsideOfFastVersioned()                       { TR_ASSERT(!isPatternGraph(), "error"); return _flags.testAny(_insideOfFastVersioned); }
   void setInsideOfFastVersioned(bool v = true)         { TR_ASSERT(!isPatternGraph(), "error"); _flags.set(_insideOfFastVersioned, v); }
   bool isRequireAHconst()                              { TR_ASSERT(isPatternGraph(), "error"); return _flags.testAny(_requireAHconst); }
   void setRequireAHconst(bool v = true)                { TR_ASSERT(isPatternGraph(), "error"); _flags.set(_requireAHconst, v); }
   bool isHighFrequency()                               { return _flags.testAny(_highFrequency); }
   void setHighFrequency(bool v = true)                 { _flags.set(_highFrequency, v); }
   bool isNoFragmentDagId()                             { return _flags.testAny(_noFragmentDagId); }
   void setNoFragmentDagId(bool v = true)               { _flags.set(_noFragmentDagId, v); }
   bool isRecordingAspectsByOpcode()                    { return _flags.testAny(_recordingAspectsByOpcode); }
   void setRecordingAspectsByOpcode(bool v = true)      { _flags.set(_recordingAspectsByOpcode, v); }

   bool needsInductionVariableInit()                    { return _flags.testAny(_needsInductionVariableInit); }
   void setNeedsInductionVariableInit(bool v = false)   { _flags.set(_needsInductionVariableInit, v); }

   enum // flag bits
      {
      _isSetUDDUchains            = 0x0001,
      _inhibitAfterVersioning     = 0x0002, // for compilation time reduction
      _inhibitBeforeVersioning    = 0x0004, // for compilation time reduction
      _insideOfFastVersioned      = _inhibitAfterVersioning,
      _highFrequency              = 0x0008,
      _noFragmentDagId            = 0x0010,
      _recordingAspectsByOpcode   = 0x0020,
      _requireAHconst             = 0x0040,
      _needsInductionVariableInit = 0x0080,
      _dummyEnum
      };

   void initializeLists()
      {
      _nodes.init();
      _orderByData.init();
      }

   List<TR_CISCNode> *getNodes() { return &_nodes; }
   List<TR_CISCNode> *getDagId2Nodes() { return _dagId2Nodes; }
   List<TR_CISCNode> *getOrderByData() { return &_orderByData; }
   ListOfCISCNodeDuplicator *getDuplicatorNodes() { return &_duplicatorNodes; }
   virtual void createDagId2NodesTable();
   virtual void createOrderByData();
   const char *getTitle() { return _titleOfCISC; }
   void setMemAspects(uint32_t load, uint32_t store)
      {
      _aspects.setLoadAspects(load);
      _aspects.setStoreAspects(store);
      }
   void setAspects(uint32_t val) { _aspects.set(val); }
   void setAspects(uint32_t val, uint32_t load, uint32_t store)
      {
      setAspects(val);
      setMemAspects(load, store);
      }
   uint32_t getAspectsValue() { return _aspects.getValue(); }
   bool testAllAspects(TR_CISCGraph *P) { return _aspects.testAll(P->getAspectsValue()); }
   void modifyTargetGraphAspects() { _aspects.modifyAspects(); }

   void setNoMemAspects(uint32_t load, uint32_t store)
      {
      _noaspects.setLoadAspects(load, false);
      _noaspects.setStoreAspects(store, false);
      }
   void setNoAspects(uint32_t val) { _noaspects.set(val); }
   void setNoAspects(uint32_t val, uint32_t load, uint32_t store)
      {
      setNoAspects(val);
      setNoMemAspects(load, store);
      }
   uint32_t getNoAspectsValue() { return _noaspects.getValue(); }
   bool testAnyNoAspects(TR_CISCGraph *P) { return _aspects.testAny(P->getNoAspectsValue()); }

   TR_Hotness getHotness() { return _hotness; }
   void setHotness(TR_Hotness hotness) { _hotness = hotness; }
   void setHotness(TR_Hotness hotness, bool highFreq) { setHotness(hotness); setHighFrequency(highFreq); }
   int32_t defragDagId();
   void setJavaSource(char *s) { _javaSource = s; }
   char *getJavaSource() { return _javaSource; }
   TR_CISCGraphAspectsWithCounts * getAspects() { return &_aspects; }
   void setMinCounts(uint8_t ifCount, uint8_t iloadCount, uint8_t istoreCount) { _aspects.setMinCounts(ifCount,iloadCount,istoreCount);}
   bool meetMinCounts(TR_CISCGraph *p) { return _aspects.meetMinCounts(&p->_aspects); }
   uint16_t getVersionLength() { return _versionLength; }
   void setVersionLength(uint16_t len) { _versionLength = len; }
   void setListsDuplicator()
      {
      _duplicatorNodes.setList(&_nodes);
      _duplicatorNodes.setDuplicateElement();
      }
   void duplicateListsDuplicator()
      {
      _duplicatorNodes.duplicateList();
      }
   void restoreListsDuplicator()
      {
      _nodes.setListHead(_duplicatorNodes.getList()->getListHead());
      if (_duplicatorNodes.isDuplicated())
         {
         createOrderByData();
         createDagId2NodesTable();
         _entryNode = _duplicatorNodes.findCorrespondingNode(_entryNode);
         _exitNode = _duplicatorNodes.findCorrespondingNode(_exitNode);
         }
      }
   TR_CISCNode *searchStore(TR_CISCNode *target, TR_CISCNode *to);

   int32_t getPatternType() { return _patternType; }
   void    setPatternType(int32_t v) { _patternType = v; }

protected:
   uint64_t getHashKeyTrNode(TR::Node *n) { return ((uintptr_t)n) >> 2; }
   uint64_t getHashKeyOpcs(uint32_t opc, bool validOtherInfo, uint32_t otherInfo)
      {
      return (((uint64_t)(opc << 1) | (uint64_t)(validOtherInfo ? 1 : 0)) << 32) ^ (uint64_t)otherInfo;
      }
   bool addTrNode2CISCNode(TR::Node *n, TR_CISCNode *c)
      {
      return _trNode2CISCNode.add(getHashKeyTrNode(n), c, true);
      }
   void addOpc2CISCNode(TR_CISCNode *c);
   bool addOpc2CISCNode(uint32_t opc, bool valid, int32_t other, TR_CISCNode *c)
      {
      return _opc2CISCNode.add(getHashKeyOpcs(opc, valid, other), c, true);
      }
   void setOutsideOfLoopFlag(uint16_t loopBodyDagId);
   const char *_titleOfCISC;    // title for debug message

   TR_Memory * _trMemory;
   TransformerPtr _transformer;
   SpecialNodeTransformerPtr _specialNodeTransformer;
   TR_CISCNode *_entryNode;
   TR_CISCNode *_exitNode;
   TR_CISCNode *_importantNode[MAX_IMPORTANT_NODES];
   TR_CISCNode *_specialCareNode[MAX_SPECIALCARE_NODES];
   TR_CISCHash _trNode2CISCNode;
   TR_CISCHash _opc2CISCNode;
   TR_CISCGraphAspectsWithCounts _aspects;
   TR_CISCGraphAspects _noaspects;
   TR_Hotness _hotness;
   uint16_t _numNodes;
   uint16_t _numDagIds;
   flags16_t _flags;
   uint16_t _versionLength;

   List<TR_CISCNode> _nodes;
   List<TR_CISCNode> *_dagId2Nodes;
   List<TR_CISCNode> _orderByData;
   ListOfCISCNodeDuplicator _duplicatorNodes;
   char *_javaSource;     // corresponding Java code to assist programmers
   int32_t _patternType;
   };


class TR_PCISCGraph : public TR_CISCGraph
   {
public:
   TR_PCISCGraph(TR_Memory * m, const char *title = 0, int32_t numHashTrNode = 31, int32_t numHashOpc = 17)
      : TR_CISCGraph(m, title, 0, 0)
         {
         if (numHashTrNode > 0) _trNode2CISCNode.setNumBuckets(numHashTrNode, persistentAlloc);
         if (numHashOpc > 0) _opc2CISCNode.setNumBuckets(numHashOpc, persistentAlloc);
         };
   virtual void createDagId2NodesTable();
   virtual void createOrderByData();
   virtual void addNode(TR_CISCNode *n, TR::Block *block = 0, TR::TreeTop *top = 0, TR::Node *trNode = 0);
   };


/*********** FOR OPTIMIZATION ************/
class TR_CISCNodeRegion : public ListHeadAndTail<TR_CISCNode>
   {
   private:
   flags16_t _flags;
   TR_BitVector _bv;
   int32_t _bvnum;

   public:

   TR_ALLOC(TR_Memory::LLList)

   TR_CISCNodeRegion(int32_t bvnum, TR::Region &region) : ListHeadAndTail<TR_CISCNode>(region) { init(bvnum, region); }
   //void init() {ListHeadAndTail<TR_CISCNode>::init(); _flags.clear();}

   void init(int32_t bvnum, TR::Region &region)
      {ListHeadAndTail<TR_CISCNode>::init(); _flags.clear(); _bvnum = bvnum; _bv.init(bvnum, region);}
   // flags
   bool isIncludeEssentialNode()                        { return _flags.testAny(_isIncludeEssentialNode); }
   void setIncludeEssentialNode(bool v = true)          { _flags.set(_isIncludeEssentialNode, v); }
   bool isOptionalNode()                                { return _flags.testAny(_isOptionalNode); }
   void setIsOptionalNode(bool v = true)                { _flags.set(_isOptionalNode, v); }

   enum // flag bits
      {
      _isIncludeEssentialNode            = 0x0001,
      _isOptionalNode               = 0x0002,
      _dummyEnum
      };

   bool isIncluded(int32_t id) { return _bv.isSet(id); }
   bool isIncluded(TR_CISCNode *n) { return _bv.isSet(n->getID()); }

   ListElement<TR_CISCNode> *add(TR_CISCNode *p)
      {
      if (p->isEssentialNode()) setIncludeEssentialNode();
      if (p->isOptionalNode()) setIsOptionalNode();
      _bv.set(p->getID());
      return ListHeadAndTail<TR_CISCNode>::add(p);
      }

   ListElement<TR_CISCNode> *append(TR_CISCNode *p)
      {
      if (p->isEssentialNode()) setIncludeEssentialNode();
      if (p->isOptionalNode()) setIsOptionalNode();
      _bv.set(p->getID());
      return ListHeadAndTail<TR_CISCNode>::append(p);
      }

   TR_CISCNodeRegion *clone();
   };


class TR_CFGReversePostOrder
   {
public:
   TR_ALLOC(TR_Memory::LoopTransformer);

   TR_CFGReversePostOrder(TR_Memory * m) : _revPost(m) { }

   List<TR::CFGNode> *compute( TR::CFG *cfg);
   void createReversePostOrder(TR::CFG *cfg, TR::CFGNode *n);
   void dump(TR::Compilation * comp);
   //void initReversePostOrder();
   //void initReversePostOrder(TR::CFG *cfg);
   //void createReversePostOrderRec(TR::CFGNode *n);

protected:
   struct TR_StackForRevPost : public TR_Link<TR_StackForRevPost>
      {
      TR::CFGNode *n;
//      ListElement<TR::CFGEdge> *le;
      TR::CFGEdgeList::iterator le;
      };

   ListHeadAndTail<TR::CFGNode> _revPost;
   // TR::CFG *_cfg;
   //TR_BitVector _visited;
   };


class TR_CISCCandidate
   {
public:
   TR_ALLOC(TR_Memory::LoopTransformer);

   TR_CISCCandidate(TR_Memory * m) : _listIdioms(m) { init(); }
   void init()
      {
      _minBCIndex = 0x7fffffff;
      _maxBCIndex = -_minBCIndex;
      _minLineNumber = 0x7fffffff;
      _maxLineNumber = -_minBCIndex;
      _listIdioms.init();
      }
   void add(TR_CISCGraph *idiom, int32_t minBC, int32_t maxBC, int32_t minLN, int32_t maxLN)
      {
      _listIdioms.add(idiom);
      if (_minBCIndex > minBC) _minBCIndex = minBC;
      if (_maxBCIndex < maxBC) _maxBCIndex = maxBC;
      if (_minLineNumber > minLN) _minLineNumber = minLN;
      if (_maxLineNumber < maxLN) _maxLineNumber = maxLN;
      }
   int32_t getMinBCIndex() { return _minBCIndex; }
   int32_t getMaxBCIndex() { return _maxBCIndex; }
   int32_t getMinLineNumber() { return _minLineNumber; }
   int32_t getMaxLineNumber() { return _maxLineNumber; }
   List<TR_CISCGraph> *getListIdioms() { return &_listIdioms; }

protected:
   int32_t _minBCIndex;
   int32_t _maxBCIndex;
   int32_t _minLineNumber;
   int32_t _maxLineNumber;
   List<TR_CISCGraph> _listIdioms;
   };


class TR_NodeDuplicator
   {
public:
   TR_ALLOC(TR_Memory::LoopTransformer);

   TR_NodeDuplicator(TR::Compilation * comp) : _list(comp->trMemory()), _comp(comp) { init(); }

   TR::Compilation * comp() { return _comp; }

   TR_HeapMemory  trHeapMemory()  { return _comp->trMemory(); }

   void init() { _list.init(); }
   TR::Node *duplicateTree(TR::Node *org);

protected:
   TR::Node *restructureTree(TR::Node *oldTree, TR::Node *newTree);
   List<TR_Pair<TR::Node,TR::Node> > _list;

private:
   TR::Compilation *       _comp;
   };


class TR_UseTreeTopMap
   {
public:
   TR_ALLOC(TR_Memory::LoopTransformer);

   TR_UseTreeTopMap(TR::Compilation * comp, TR::Optimizer * optimizer);
   TR_HashTabInt  _useToParentMap; // lists of owning treetops indexed by use index
   int32_t buildAllMap();
   void buildUseTreeTopMap(TR::TreeTop* treeTop,TR::Node *node);
   TR::TreeTop * findParentTreeTop(TR::Node *useNode);
   TR::Compilation *comp() { return _compilation; }

protected:
   bool _buildAllMap;
   TR::Compilation *_compilation;
   TR::Optimizer *_optimizer;
   TR_UseDefInfo *_info;
   };


typedef enum
   {
   _T2P_NULL = 0,
   _T2P_NotMatch = 1,
   _T2P_MatchMask = 2,
   _T2P_Single = 4,
   _T2P_Multiple = 8,
   _T2P_MatchAndSingle =   _T2P_MatchMask | _T2P_Single,    // Match and T2P is single
   _T2P_MatchAndMultiple = _T2P_MatchMask | _T2P_Multiple,  // Match and T2P is multiple
   } CISCT2PCond;

class TR_CISCTransformer : public TR_LoopTransformer
   {
   public:
   static void showCISCNodeRegion(TR_CISCNodeRegion *region, TR::Compilation * );
   static void showCISCNodeRegions(List<TR_CISCNodeRegion> *regions, TR::Compilation * );
   static TR::Block *getSuccBlockIfSingle(TR::Block *block);
   static bool searchNodeInTrees(TR::Node *top, TR::Node *target, TR::Node **retParent = NULL, int *retChildNum = NULL);
   static bool compareTrNodeTree(TR::Node *a, TR::Node *b);
   static bool compareBlockTrNodeTree(TR::Block *a, TR::Block *b);
   bool getBCIndexMinMax(List<TR_CISCNode> *l, int32_t *minIndex, int32_t *maxIndex, int32_t *_minLN, int32_t *_maxLN, bool allowInlined);

   TR_CISCTransformer(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_CISCTransformer(manager);
      }

   void easyTreeSimplification(TR::Node *const node);
   TR_CISCNode *addAllSubNodes(TR_CISCGraph *const graph, TR::Block *const block, TR::TreeTop *const top, TR::Node *const parent, TR::Node *const node, const int32_t dagId);
   bool makeCISCGraphForBlock(TR_CISCGraph *graph, TR::Block *const block, int32_t dagId);
   void resolveBranchTargets(TR_CISCGraph *graph, TR_CISCNode *);
   uint16_t renumberDagId(TR_CISCGraph *graph, int32_t tempMaxDagId, int32_t bodyDagId);
   TR_CISCGraph *makeCISCGraph(List<TR::Block> *pred, List<TR::Block> *body, List<TR::Block> *succ);

   struct TR_BitsKeepAliveInfo
      {
      TR_ALLOC(TR_Memory::LoopTransformer);

      TR_BitsKeepAliveInfo(TR::Block * block, TR::TreeTop *treeTop, TR::TreeTop *prevTreeTop):
         _block(block), _treeTop(treeTop), _prevTreeTop(prevTreeTop)
      {}

      TR::Block *_block;
      TR::TreeTop *_treeTop;
      TR::TreeTop *_prevTreeTop;
      };

   bool removeBitsKeepAliveCalls(List<TR::Block> *body);
   void insertBitsKeepAliveCalls(TR::Block * block);
   void restoreBitsKeepAliveCalls();

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   bool createLoopCandidates(List<TR_RegionStructure> *ret);
   TR::Block * findPredecessorBlockOfLoopEntry(TR_RegionStructure *loop);
   TR::Block *addPreHeaderIfNeeded(TR_RegionStructure *loop);

   bool computeTopologicalEmbedding(TR_CISCGraph *P, TR_CISCGraph *T);
   bool embeddingHasConflictingBranches();
   void showEmbeddedData(char *title, uint8_t *data);
   bool computeEmbeddedForData();
   bool computeEmbeddedForCFG();
   bool dagEmbed(TR_CISCNode *, TR_CISCNode*);
   bool cycleEmbed(uint16_t, uint16_t);
   bool checkParents(TR_CISCNode *p, TR_CISCNode *t, uint8_t *result, bool *inLoop, bool *optionalParents);
   bool makeLists();
   int32_t analyzeConnectionOnePairChild(TR_CISCNode *const p, TR_CISCNode *const t, TR_CISCNode *const pn, TR_CISCNode *tn);
   void analyzeConnectionOnePair(TR_CISCNode *const p, TR_CISCNode *const t);
   void analyzeConnection();
   void analyzeArrayHeaderConst();
   void showT2P();
   TR_CISCNodeRegion* extractMatchingRegion();
   bool findFirstNode(TR::TreeTop **retTree, TR::Node **retNode, TR::Block **retBlock);
   int countP2T(TR_CISCNode *p, bool inLoop = false);
   TR_CISCNode *getP2TRep(TR_CISCNode *p);
   TR_CISCNode *getP2TRepInLoop(TR_CISCNode *p, TR_CISCNode *exclude = NULL);
   TR_CISCNode *getP2TInLoopIfSingle(TR_CISCNode *p);
   TR_CISCNode *getP2TInLoopAllowOptionalIfSingle(TR_CISCNode *p);
   bool verifyCandidate();
   void addEdge(TR::CFGEdgeList *succList, TR::Block *, TR::Block *);
   void removeEdge(List<TR::CFGEdge> *succList, TR::Block *, TR::Block *);
   void removeEdgesExceptFor(TR::CFGEdgeList *succList, TR::Block *, TR::Block *);
   void setEdge(TR::CFGEdgeList *succList, TR::Block *, TR::Block *);
   TR::Block *analyzeSuccessorBlock(TR::Node *ignoreTree = 0);
   void setSuccessorEdge(TR::Block *block, TR::Block *target = 0);
   void setEdges(TR::CFGEdgeList *succList, TR::Block *, TR::Block *, TR::Block *);
   TR::Block *searchOtherBlockInSuccBlocks(TR::Block *target0, TR::Block *target1);
   TR::Block *searchOtherBlockInSuccBlocks(TR::Block *target0);
   TR::Block *setSuccessorEdges(TR::Block *block, TR::Block *target, TR::Block *);
   TR::TreeTop *removeAllNodes(TR::TreeTop *start, TR::TreeTop *end);
   void initTopologicalEmbedding()
      {
      _beforeInsertions.init();
      _afterInsertions.init();
      _afterInsertionsIdiom[0].init();
      _afterInsertionsIdiom[1].init();
      _offsetOperand1 = 0;
      _offsetOperand2 = 0;
      }
   TR::Block *insertBeforeNodes(TR::Block *block);
   TR::Block *insertAfterNodes(TR::Block *block, List<TR::Node> *l, bool prepend = false);
   TR::Block *insertAfterNodes(TR::Block *block, bool prepend = false);
   TR::Block *insertAfterNodesIdiom(TR::Block *block, int32_t pos, bool prepend = false);
   TR::Node *findStoreToSymRefInInsertBeforeNodes(int32_t symRefNumberToBeMatched);
   bool areAllNodesIncluded(TR_CISCNodeRegion *r);

   inline uint32_t idx(uint32_t a, uint32_t x) { return (a * _numTNodes) + x; }

   TR_CISCGraph *getP() { return _P; }
   TR_CISCGraph *getT() { return _T; }
   List<TR_CISCNode> *getP2T() { return _P2T; }
   List<TR_CISCNode> *getT2P() { return _T2P; }
   TR_CISCNode *getT2PheadRep(int32_t tid)
      {
      List<TR_CISCNode> *l = _T2P + tid;
      return l->isEmpty() ? 0 : l->getListHead()->getData();
      }
   TR_CISCNode *getT2PheadRep(TR_CISCNode *t) {return getT2PheadRep(t->getID());}
   TR_CISCNode *getT2Phead(int32_t tid)
      {
      TR_ASSERT(!_T2P[tid].isMultipleEntry(), "maybe error");
      return getT2PheadRep(tid);
      }
   TR_CISCNode *getT2Phead(TR_CISCNode *t) {return getT2Phead(t->getID());}
   bool isGenerateI2L() { return _isGenerateI2L; }
   bool showMesssagesStdout() { return _showMesssagesStdout; }
   TR_CISCNodeRegion *getCandidateRegion() { return _candidateRegion; }
   bool analyzeBoolTable(TR_BitVector **bv, TR::TreeTop **retTreeTop, TR_CISCNode *boolTable,
                         TR_BitVector *defBV, TR_CISCNode *defNode,
                         TR_CISCNode *ignoreNode,
                         int32_t bvoffset, int32_t allocBVSize);
   int32_t analyzeByteBoolTable(TR_CISCNode *booltable, uint8_t *table256, TR_CISCNode *ignoreNode = 0, TR::TreeTop **retSameExit = 0);
   int32_t analyzeCharBoolTable(TR_CISCNode *booltable, uint8_t *table65536, TR_CISCNode *ignoreNode = 0, TR::TreeTop **retSameExit = 0);
   bool isEmptyBeforeInsertionList() { return _beforeInsertions.isEmpty(); }
   ListHeadAndTail<TR::Node> *getBeforeInsertionList() { return &_beforeInsertions; }
   bool isEmptyAfterInsertionList() { return _afterInsertions.isEmpty(); }
   ListHeadAndTail<TR::Node> *getAfterInsertionList() { return &_afterInsertions; }
   bool isEmptyAfterInsertionIdiomList(int32_t pos) { return _afterInsertionsIdiom[pos].isEmpty(); }
   ListHeadAndTail<TR::Node> *getAfterInsertionIdiomList(int32_t pos) { TR_ASSERT(pos < 2, "error"); return _afterInsertionsIdiom+pos; }
   bool alignTopOfRegion(TR_CISCNodeRegion *r);
   static bool isInsideOfFastVersionedLoop(TR_RegionStructure *l);
   void setColdLoopBody();
   void analyzeHighFrequencyLoop(TR_CISCGraph *graph, TR_RegionStructure *naturalLoop);
   int32_t getNumOfBBlistPred() { return _bblistPred.getSize(); }
   int32_t getNumOfBBlistBody() { return _bblistBody.getSize(); }
   int32_t getNumOfBBlistSucc() { return _bblistSucc.getSize(); }
   bool isBlockInLoopBody(TR::Block *b);
   int16_t getOffsetOperand1() { return _offsetOperand1; }
   int16_t getOffsetOperand2() { return _offsetOperand2; }
   void setOffsetOperand1(int16_t v) { _offsetOperand1 = v; }
   void setOffsetOperand2(int16_t v) { _offsetOperand2 = v; }

   CISCT2PCond analyzeT2P(TR_CISCNode *t, TR_CISCNode *p = 0);

   enum
      {
      _Unknown = 0,               // the pair (p, t) is not processed yet.
      _NotEmbed = 0x1,            // the status when the other four status are not satisfied
      _Desc =  (_NotEmbed | 0x2), // there is the _Embed status within a pair (p, a descendent of t).
      _Cond = 4,                  // (1) the labels of p and t are equivalent but (2) all children of p and t are not processed yet.
      _Embed = (_Desc | 0x4)      // (1) the labels of p and t are equivalent and (2) descendents of t include all children of p.
      };
   static inline bool isDescOrEmbed(uint8_t status) { return ((status & _Desc) == _Desc); }     // Assume _Desc=3 and _Embed=7

   void setFlag1(bool v = true)                         { _flagsForTransformer.set(_flag1, v); }
   bool isFlag1()                                       { return _flagsForTransformer.testAny(_flag1); }
   void setFlag2(bool v = true)                         { _flagsForTransformer.set(_flag2, v); }
   bool isFlag2()                                       { return _flagsForTransformer.testAny(_flag2); }
   void setFlag3(bool v = true)                         { _flagsForTransformer.set(_flag3, v); }
   bool isFlag3()                                       { return _flagsForTransformer.testAny(_flag3); }
   void setFlag4(bool v = true)                         { _flagsForTransformer.set(_flag4, v); }
   bool isFlag4()                                       { return _flagsForTransformer.testAny(_flag4); }
   void resetFlags()                                    { _flagsForTransformer.reset(_maskForFlagsPtr); }
   void setIsInitializeNegative128By1(bool v = true)    { setFlag1(v); }
   bool isInitializeNegative128By1()                    { return isFlag1(); }
   void setTableBackedByRawStorage(bool v = true)       { setFlag1(v); }
   bool isTableBackedByRawStorage()                     { return isFlag1(); }
   void setCompareTo(bool v = true)                     { setFlag1(v); }
   bool isCompareTo()                                   { return isFlag1(); }
   void setIndexOf(bool v = true)                       { setFlag2(v); }
   bool isIndexOf()                                     { return isFlag2(); }
   void setMEMCPYDec(bool v = true)                     { setFlag1(v); }
   bool isMEMCPYDec()                                   { return isFlag1(); }
   bool isAfterVersioning()                             { return manager()->numPassesCompleted() > 0; }
   void setShowingCandidates(bool v = true)             { _flagsForTransformer.set(_isShowingCandidates, v); }
   bool isShowingCandidates()                           { return _flagsForTransformer.testAny(_isShowingCandidates); }
   void setFirstTime(bool v = true)                     { _flagsForTransformer.set(_isFirstTime, v); }
   bool isFirstTime()                                   { return _flagsForTransformer.testAny(_isFirstTime); }
   void setConverged(bool v = true)                     { _flagsForTransformer.set(_isConverged, v); }
   bool isConverged()                                   { return _flagsForTransformer.testAny(_isConverged); }
   enum // flag bits for _flagsForTransformer
      {
      _flag1                             = 0x0001,      // for TransformerPtr
      _flag2                             = 0x0002,      // for TransformerPtr
      _flag3                             = 0x0004,      // for TransformerPtr
      _flag4                             = 0x0008,      // for TransformerPtr
      _maskForFlagsPtr                   = _flag1|_flag2|_flag3|_flag4, // for TransformerPtr
      // UNUSED                          = 0x1000,
      _isShowingCandidates               = 0x2000,
      _isFirstTime                       = 0x4000,
      _isConverged                       = 0x8000,
      _dummyEnum
      };
   TR_UseDefInfo *getUseDefInfo() { return _useDefInfo; }
   void showCandidates();
   void registerCandidates();
   void moveCISCNodesInList(List<TR_CISCNode> *l, TR_CISCNode *from, TR_CISCNode *to, TR_CISCNode *moveTo);
   void moveCISCNodes(TR_CISCNode *from, TR_CISCNode *to, TR_CISCNode *moveTo, char *debugStr = NULL);
   TR::Block *searchPredecessorOfBlock(TR::Block *block);
   TR::Block *modifyBlockByVersioningCheck(TR::Block *block, TR::TreeTop *startTop, TR::Node *lengthNode, List<TR::Node> *guardList = NULL);
   TR::Block *modifyBlockByVersioningCheck(TR::Block *block, TR::TreeTop *startTop, List<TR::Node> *guardList);
   TR::Block *cloneLoopBodyForPeel(TR::Block **firstBlock, TR::Block **lastBlock, TR_CISCNode *cmpifCISCNode = NULL);
   TR::Block *appendBlocks(TR::Block *block, TR::Block *firstBlock, TR::Block *lastBlock);
   bool isDeadStore(TR::Node *node);
   TR::Block *skipGoto(TR::Block *block, TR::Node *ignoreTree);
   bool analyzeOneArrayIndex(TR_CISCNode *arrayindex, TR::SymbolReference *tInductionVariable);
   bool analyzeArrayIndex(TR::SymbolReference *tInductionVariable);
   int32_t countGoodArrayIndex(TR::SymbolReference *tInductionVariable);
   bool simpleOptimization();
   uint64_t getHashValue(TR_CISCNodeRegion *);
   bool canConvertArrayCmpSign(TR::Node *storeNode, List<TR::TreeTop> *compareIfs, bool *canConvertToArrayCmp);

   TR_RegionStructure *getCurrentLoop() { return _loopStructure; }
   void setCurrentLoop(TR_RegionStructure *loop) { _loopStructure = loop; }

private:
   List<TR::Block> _bblistPred;
   List<TR::Block> _bblistBody;
   List<TR::Block> _bblistSucc;
   List<TR_BitsKeepAliveInfo> _BitsKeepAliveList;

   TR_CISCNodeRegion *_candidateRegion;
   TR_CISCCandidate _candidatesForShowing;
   List<TR_CISCNodeRegion> _candidatesForRegister;
   ListHeadAndTail<TR_CISCNode> *_candidateBBStartEnd;

   TR_UseDefInfo *_useDefInfo;
   TR_CISCNode *_lastCFGNode;           // just for working
   List<TR_CISCNode> _backPatchList;    // just for working
   TR_UseTreeTopMap _useTreeTopMap;

   // embedded information
   List<TR_CISCNode> *_P2T;             // just for working
   List<TR_CISCNode> *_T2P;             // just for working
   ListHeadAndTail<TR::Node> _beforeInsertions;
   ListHeadAndTail<TR::Node> _afterInsertions;
   ListHeadAndTail<TR::Node> * _afterInsertionsIdiom;   // Idiom specific
   TR_RegionStructure *_loopStructure; // current loop being analyzed

   uint16_t _sizeP2T;
   uint16_t _sizeT2P;
   uint16_t _numPNodes;
   uint16_t _numTNodes;
   uint16_t _sizeResult;
   uint16_t _sizeDE;
   int16_t _offsetOperand1;
   int16_t _offsetOperand2;
   flags16_t _flagsForTransformer;
   TR_CISCGraph *_P;
   TR_CISCGraph *_T;
   uint8_t *_embeddedForData;   // result of data dependence
   uint8_t *_embeddedForCFG;    // result of control flow graph
   uint8_t *_EM;        // just for working
   uint8_t *_DE;        // just for working
   bool _isGenerateI2L;
   bool _showMesssagesStdout;
   };

#endif
