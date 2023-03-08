/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#ifndef ESCAPEANALYSIS_INCL
#define ESCAPEANALYSIS_INCL

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "infra/BitVector.hpp"
#include "infra/Flags.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"

class TR_EscapeAnalysis;
class TR_FlowSensitiveEscapeAnalysis;
class TR_LocalFlushElimination;
class TR_OpaqueClassBlock;
class TR_ResolvedMethod;
class TR_UseDefInfo;
class TR_ValueNumberInfo;
namespace TR { class Block; }
namespace TR { class BlockChecklist; }
namespace TR { class CFGEdge; }
namespace TR { class NodeChecklist; }
namespace TR { class Optimizer; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }
template <class T> class TR_Array;

typedef TR::typed_allocator<TR::Node *, TR::Region &> NodeDequeAllocator;
typedef std::deque<TR::Node *, NodeDequeAllocator> NodeDeque;

typedef TR::typed_allocator<std::pair<TR::Node* const, std::pair<TR_BitVector *, NodeDeque*> >, TR::Region&> CallLoadMapAllocator;
typedef std::less<TR::Node *> CallLoadMapComparator;
typedef std::map<TR::Node *, std::pair<TR_BitVector *, NodeDeque *>, CallLoadMapComparator, CallLoadMapAllocator> CallLoadMap;

typedef TR::typed_allocator<std::pair<TR::Node *const, int32_t>, TR::Region&> RemainingUseCountMapAllocator;
typedef std::less<TR::Node *> RemainingUseCountMapComparator;
typedef std::map<TR::Node *, int32_t, RemainingUseCountMapComparator, RemainingUseCountMapAllocator> RemainingUseCountMap;

// Escape analysis
//
// Find object allocations that are either method local or thread local.
//
// Requires value numbering and use/def information.
//

class TR_ColdBlockEscapeInfo
   {
   public:
   TR_ALLOC(TR_Memory::EscapeAnalysis)

   TR_ColdBlockEscapeInfo(TR::Block *block, TR::Node *node, TR::TreeTop *tree, TR_Memory * m)
       : _block(block), _escapeTrees(m), _nodes(m)
      {
      _nodes.add(node);
      _escapeTrees.add(tree);
      }

   TR_ScratchList<TR::Node> *getNodes()  {return &_nodes;}
   TR_ScratchList<TR::TreeTop> *getTrees()  {return &_escapeTrees;}

   void addNode(TR::Node *node, TR::TreeTop *tree)
      {
      if (!_nodes.find(node))
         {
         _nodes.add(node);
         //if (!_escapeTrees.find(tree))
         _escapeTrees.add(tree);
         }
      }
   TR::Block *getBlock() {return _block;}
   void     setBlock(TR::Block *block) {_block = block;}

   private:
   TR_ScratchList<TR::TreeTop> _escapeTrees;
   TR::Block                   *_block;
   TR_ScratchList<TR::Node>    _nodes;
   };


class Candidate;


struct FieldInfo
   {
   int32_t                               _offset;
   int32_t                               _size;
   TR::SymbolReference                 *_symRef;
   TR_ScratchList<TR::SymbolReference> *_goodFieldSymrefs;
   TR_ScratchList<TR::SymbolReference> *_badFieldSymrefs;
   char                                  _vectorElem;

   void                rememberFieldSymRef(TR::Node *node, int32_t fieldOffset, Candidate *candidate, TR_EscapeAnalysis *ea);
   bool                symRefIsForFieldInAllocatedClass(TR::SymbolReference *symRef);
   bool                hasBadFieldSymRef();
   TR::SymbolReference *fieldSymRef(); // Any arbitrary good field symref
   };


class Candidate : public TR_Link<Candidate>
   {
   public:
   Candidate(TR::Node *node, TR::TreeTop *treeTop, TR::Block *block, int32_t size, void *classInfo, TR::Compilation * c)
      : _kind(node->getOpCodeValue()), _node(node), _treeTop(treeTop), _origKind(node->getOpCodeValue()), _stringCopyNode(NULL), _stringCopyCallTree(NULL),
        _block(block),
        _class(classInfo),
        _size(size), _fieldSize(0), _valueNumbers(0), _fields(0), _origSize(size),
        _initializedWords(0),
        _maxInlineDepth(0), _inlineBytecodeSize(0), _seenFieldStore(false), _seenSelfStore(false), _seenStoreToLocalObject(false), _seenArrayCopy(false), _argToCall(false), _usedInNonColdBlock(false), _lockedInNonColdBlock(false),_isImmutable(false),
        _originalAllocationNode(0), _lockedObject(false), _index(-1), _flushRequired(true),
        _comp(c), _trMemory(c->trMemory()),
        _flushMovedFrom(c->trMemory()),
        _symRefs(c->trMemory()),
        _callSites(c->trMemory()),
        _dememoizedMethodSymRef(NULL),
        _dememoizedConstructorCall(NULL),
    	_virtualCallSitesToBeFixed(c->trMemory()),
        _coldBlockEscapeInfo(c->trMemory())
         {
          static const char *forceContinguousAllocation = feGetEnv("TR_forceContinguousAllocation");
          if (forceContinguousAllocation)
             setMustBeContiguousAllocation();
         }

   TR::Compilation *          comp()                        { return _comp; }

   TR_Memory *               trMemory()                    { return _trMemory; }
   TR_StackMemory            trStackMemory()               { return _trMemory; }
   TR_HeapMemory             trHeapMemory()                { return _trMemory; }
   TR_PersistentMemory *     trPersistentMemory()          { return _trMemory->trPersistentMemory(); }

   bool isLocalAllocation()          {return _flags.testAny(LocalAllocation);}
   bool isContiguousAllocation()     {return mustBeContiguousAllocation() || hasCallSites();}
   bool mustBeContiguousAllocation() {return _flags.testAny(MustBeContiguous);}
   bool isExplicitlyInitialized()    {return _flags.testAny(ExplicitlyInitialized);}
   bool objectIsReferenced()         {return _flags.testAny(ObjectIsReferenced);}
   bool fillsInStackTrace()          {return _flags.testAny(FillsInStackTrace);}
   bool callsStringCopyConstructor() {return _flags.testAny(CallsStringCopy);}
   bool isInsideALoop()              {return _flags.testAny(InsideALoop);}
   bool isInAColdBlock()             {return _flags.testAny(InAColdBlock);}
   bool isProfileOnly()              {return _flags.testAny(ProfileOnly);}

   bool usesStackTrace()             {return _flags.testAny(UsesStackTrace);}
   bool isArgToCall(int32_t depth)   {return _argToCallFlags.testAny(1<<depth);}
   bool isNonThisArgToCall(int32_t depth) {return _nonThisArgToCallFlags.testAny(1<<(depth));}
   bool forceLocalAllocation()       { return _flags.testAny(ForceLocalAllocation);}

   void setForceLocalAllocation(bool v = true)       {_flags.set(ForceLocalAllocation, v);}
   void setLocalAllocation(bool v = true)            {_flags.set(LocalAllocation, v);}
   void setMustBeContiguousAllocation(bool v = true) {_flags.set(MustBeContiguous, v);}
   void setExplicitlyInitialized(bool v = true)      {_flags.set(ExplicitlyInitialized, v);}
   void setObjectIsReferenced(bool v = true)         {_flags.set(ObjectIsReferenced, v);}
   void setFillsInStackTrace(bool v = true)          {_flags.set(FillsInStackTrace, v);}
   void setCallsStringCopyConstructor(bool v = true) {_flags.set(CallsStringCopy, v);}
   void setInsideALoop(bool v = true)                {_flags.set(InsideALoop, v); }
   void setInAColdBlock(bool v = true)               {_flags.set(InAColdBlock, v); }
   void setProfileOnly(bool v = true)                {_flags.set(ProfileOnly, v); }

   void setUsesStackTrace(bool v = true)             {_flags.set(UsesStackTrace, v);}
   void setArgToCall(int32_t depth, bool v = true)   {_argToCallFlags.set(1<<depth, v); if (v == true) _argToCall = true;}
   void setNonThisArgToCall(int32_t depth, bool v = true)   {_nonThisArgToCallFlags.set(1<<depth, v);}


   TR::Node *getStringCopyNode() {return _stringCopyNode; }
   void setStringCopyNode(TR::Node *n) { _stringCopyNode = n; }

   bool isLockedObject()  { return _lockedObject; }
   void setLockedObject(bool b) {_lockedObject = b;}

   bool usedInNonColdBlock() { return _usedInNonColdBlock; }
   void setUsedInNonColdBlock(bool b = true) { _usedInNonColdBlock = b; }

   bool lockedInNonColdBlock() { return _lockedInNonColdBlock; }
   void setLockedInNonColdBlock(bool b = true) { _lockedInNonColdBlock = b; }

   TR_ScratchList<TR::SymbolReference> *getSymRefs() { return &_symRefs; }
   void addSymRef(TR::SymbolReference *symRef)      { _symRefs.add(symRef); }

   bool hasCallSites()                        { return !_callSites.isEmpty(); }
   TR_ScratchList<TR::TreeTop> *getCallSites() { return &_callSites; }
   void addCallSite(TR::TreeTop *treeTop)      { _callSites.add(treeTop); }

   bool hasVirtualCallsToBeFixed() {return !_virtualCallSitesToBeFixed.isEmpty();}
   TR_ScratchList<TR::TreeTop> *getVirtualCallSitesToBeFixed() { return &_virtualCallSitesToBeFixed; }
   void addVirtualCallSiteToBeFixed(TR::TreeTop *treeTop)      { _virtualCallSitesToBeFixed.add(treeTop); }
   bool escapesInColdBlocks()                 { return !_coldBlockEscapeInfo.isEmpty(); }

   bool escapesInColdBlock(TR::Block *block)
      {
      ListElement<TR_ColdBlockEscapeInfo> *curColdBlockInfo = _coldBlockEscapeInfo.getListHead();
      while (curColdBlockInfo)
         {
         if (curColdBlockInfo->getData()->getBlock() == block)
            return true;
         curColdBlockInfo = curColdBlockInfo->getNextElement();
         }
      return false;
      }

   TR_ScratchList<TR_ColdBlockEscapeInfo> *getColdBlockEscapeInfo() { return &_coldBlockEscapeInfo; }
   void addColdBlockEscapeInfo(TR::Block *block, TR::Node *node, TR::TreeTop *tree)
      {
      ListElement<TR_ColdBlockEscapeInfo> *curColdBlockInfo = _coldBlockEscapeInfo.getListHead();
      while (curColdBlockInfo)
         {
         if (curColdBlockInfo->getData()->getBlock() == block)
            break;
         curColdBlockInfo = curColdBlockInfo->getNextElement();
         }

      if (curColdBlockInfo)
         {
         curColdBlockInfo->getData()->addNode(node, tree);
         }
      else
         {
         TR_ColdBlockEscapeInfo *coldBlockInfo = new (trStackMemory()) TR_ColdBlockEscapeInfo(block, node, tree, trMemory());
         _coldBlockEscapeInfo.add(coldBlockInfo);
         }
      }

     void print();

     TR::ILOpCodes            _kind;
     TR::ILOpCodes            _origKind;
     TR::Node                *_node;
     TR::TreeTop             *_treeTop;
     TR::Block               *_block;
     TR_Array<int32_t>      *_valueNumbers;
     TR_Array<FieldInfo>    *_fields;
     TR_BitVector           *_initializedWords;
     void                   *_class;
     TR::Node                *_originalAllocationNode;
     TR::Compilation *        _comp;
     TR_Memory *             _trMemory;
     TR::Node                *_stringCopyNode;

     TR::TreeTop             *_stringCopyCallTree;

     TR::SymbolReference     *_dememoizedMethodSymRef;
     TR::TreeTop             *_dememoizedConstructorCall;

     TR_ScratchList<Candidate> _flushMovedFrom;

     int32_t                 _size;
     int32_t                 _fieldSize;
     int32_t                 _origSize;
     int32_t                 _maxInlineDepth;
     int32_t                 _inlineBytecodeSize;
     bool                    _seenFieldStore;
     bool                    _seenSelfStore;

     bool                    _seenStoreToLocalObject;
     bool                    _seenArrayCopy;
     bool                    _argToCall;
     bool                    _isImmutable;

     bool                    _lockedObject;
     bool                    _flushRequired;
     bool                    _usedInNonColdBlock;
     bool                    _lockedInNonColdBlock;
     int32_t                 _index;

   protected:

     TR_ScratchList<TR::SymbolReference> _symRefs;
     TR_ScratchList<TR::TreeTop> _callSites;
     TR_ScratchList<TR::TreeTop> _virtualCallSitesToBeFixed;
     TR_ScratchList<TR_ColdBlockEscapeInfo>   _coldBlockEscapeInfo;
     flags32_t                  _flags;
     flags16_t                  _nonThisArgToCallFlags;
     flags16_t                  _argToCallFlags;

     enum // flag bits
         {
         LocalAllocation              = 0x80000000,
         MustBeContiguous             = 0x40000000,
         ExplicitlyInitialized        = 0x20000000,
         ObjectIsReferenced           = 0x10000000,

         // Flags used for Throwable objects
         //
         FillsInStackTrace            = 0x08000000,
         UsesStackTrace               = 0x04000000,

         // Object that is being allocated inside a loop
         //
         InsideALoop                  = 0x02000000,

         // Object that is in a cold block
         //
         InAColdBlock                 = 0x01000000,

         // Object is an array whose length is not a constant, so we can't do
         // anything with it in this EA pass, but continue analyzing it because
         // if the unknown size is the ONLY reason we can't stack-allocate it,
         // then we should add profiling trees.
         //
         ProfileOnly                  = 0x00800000,


         // Object whose class has been annotated by user that
         // any instance should be locally allocated (X10)
         ForceLocalAllocation         = 0x00100000,

         CallsStringCopy              = 0x00200000,
         };
   };


class FlushCandidate : public TR_Link<FlushCandidate>
   {
   public:
   FlushCandidate(TR::TreeTop *flushNode, TR::Node *node, int32_t blockNum, Candidate *candidate = 0)
     : _flushNode(flushNode), _node(node), _blockNum(blockNum), _candidate(candidate), _isKnownToLackCandidate(false), _optimallyPlaced(false)
     {
     }

   TR::Node *getAllocation() {return _node;}
   void setAllocation(TR::Node *node) {_node = node;}

   TR::TreeTop *getFlush() {return _flushNode;}
   void setFlush(TR::TreeTop *node) {_flushNode = node;}

   int32_t getBlockNum() {return _blockNum;}
   void setBlockNum(int32_t n) {_blockNum = n;}

   Candidate *getCandidate() { return _candidate;}
   void setCandidate(Candidate *c) {_candidate = c;}

   /**
    * \brief Indicates whether this \c FlushCandidate is known to have no
    * candidate for stack allocation associated with it.  That is, that
    * the \ref getCandidate() method will always return \c NULL.
    *
    * \return \c true if this \c FlushCandidate is known to have no
    * candidate for stack allocation associated with it;
    * \c false if this \c FlushCandidate is known to have a candidate
    * for stack allocation associated with it, or if it has not yet
    * been determined whether there is a candidate for stack allocation
    * associated with it.
    */
   bool getIsKnownToLackCandidate() { return _isKnownToLackCandidate;}

   /**
    * \brief Sets the status of this \c FlushCandidate, indicating whether
    * it is known to have no candidate for stack allocation associated with it.
    *
    * \param setting The updated status indicating whether this \c FlushCandidate
    * is known to have no candidate for stack allocation associated with it
    */
   void setIsKnownToLackCandidate(bool setting) {_isKnownToLackCandidate = setting;}

    /**
    * \brief Indicates whether this \c FlushCandidate is known to be
    * optimally placed above a volatile access node and therefor should
    * not be be considered for further optimization.
    *
    * \return \c true if this \c FlushCandidate is known be already optimally placed;
    * \c false if this \c FlushCandidate can be considered for further optimization,
    * or if it has not yet been determined whether it is optimally placed.
    */
   bool isOptimallyPlaced() { return _optimallyPlaced;}

   /**
    * \brief Sets the status of this \c FlushCandidate, indicating whether
    * it is known to be optimally placed above a volatile access node.
    *
    * \param setting The updated status indicating whether this \c FlushCandidate
    * is known to be optimally placed.
    */
   void setOptimallyPlaced(bool setting) {_optimallyPlaced = setting;}

   private:
   TR::Node *_node;
   TR::TreeTop *_flushNode;
   int32_t _blockNum;
   Candidate *_candidate;
   bool _isKnownToLackCandidate;
   bool _optimallyPlaced;
   };


class TR_DependentAllocations
   {
   public:
   TR_ALLOC(TR_Memory::EscapeAnalysis)

   TR_DependentAllocations(Candidate *allocNode, Candidate *dependentNode, TR_Memory * m)
     : _allocNode(allocNode), _dependentNodes(m)
      {
      addDependentAllocation(dependentNode);
      }

   TR_ScratchList<Candidate> *getDependentAllocations()  {return &_dependentNodes;}
   void addDependentAllocation(Candidate *c)
      {
      if (c && !_dependentNodes.find(c))
         _dependentNodes.add(c);
      }

   Candidate *getAllocation() {return _allocNode;}
   void     setAllocation(Candidate *node) {_allocNode = node;}

   private:
   Candidate                    *_allocNode;
   TR_ScratchList<Candidate>    _dependentNodes;
   };


class TR_CFGEdgeAllocationPair
   {
   public:
   TR_ALLOC(TR_Memory::EscapeAnalysis)

   TR_CFGEdgeAllocationPair(TR::CFGEdge *edge, Candidate *allocNode)
     : _allocNode(allocNode),
       _edge(edge)
      {
      }

   Candidate *getAllocation() {return _allocNode;}
   void     setAllocation(Candidate *node) {_allocNode = node;}

   TR::CFGEdge *getEdge() {return _edge;}
   void     setEdge(TR::CFGEdge *edge) {_edge = edge;}

   private:
   Candidate                    *_allocNode;
   TR::CFGEdge                   *_edge;
   };


class SniffCallCache : public TR_Link<SniffCallCache>
   {
   public:
   SniffCallCache(TR_ResolvedMethod *vmMethod, bool isCold, int32_t bytecodeSize)
      : _vmMethod(vmMethod), _isCold(isCold), _bytecodeSize(bytecodeSize)
      {
      }
   static bool isInCache(TR_LinkHead<SniffCallCache> *sniffCacheList, TR_ResolvedMethod *vmMethod, bool isCold, int32_t &bytecodeSize);
   private:
   TR_ResolvedMethod          *_vmMethod;
   bool                        _isCold;
   int32_t                     _bytecodeSize;
   };

class SymRefCache : public TR_Link<SymRefCache>
   {
   public:
   SymRefCache(TR::SymbolReference *symRef, TR_ResolvedMethod *resolvedMethod)
      : _symRef(symRef), _vmMethod(resolvedMethod)
      {
      }
   static TR::SymbolReference* findSymRef(TR_LinkHead<SymRefCache> *symRefList, TR_ResolvedMethod *resolvedMethod);
   TR::SymbolReference *getSymRef() {return _symRef;}
   TR_ResolvedMethod *getMethod() {return _vmMethod;}
   private:
   TR::SymbolReference           *_symRef;
   TR_ResolvedMethod            *_vmMethod;
   };

class TR_EscapeAnalysis : public TR::Optimization
   {
   public:

   TR_EscapeAnalysis(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_EscapeAnalysis(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   /**
    * Indicates whether stack allocation of \c newvalue operations may be
    * performed.  If the value is set to \c true, \c newvalue operations
    * will not be considered as candidates for stack allocation; otherwise,
    * they will be considered as candidates.
    */
   bool                       _disableValueTypeStackAllocation;

   protected:

   enum restrictionType { MakeNonLocal, MakeContiguous, MakeObjectReferenced };

   int32_t  performAnalysisOnce();
   void     findCandidates();
   void     findIgnorableUses();
   void     markUsesAsIgnorable(TR::Node *node, TR::NodeChecklist& visited);
   void     findLocalObjectsValueNumbers();
   void     findLocalObjectsValueNumbers(TR::Node *node, TR::NodeChecklist& visited);

   Candidate *createCandidateIfValid(TR::Node *node, TR_OpaqueClassBlock *&classInfo,bool);
   //int32_t  checkForValidCandidate(TR::Node *node, TR_OpaqueClassBlock *&classInfo,bool);
   bool     collectValueNumbersOfIndirectAccessesToObject(TR::Node *node, Candidate *candidate, TR::Node *indirectStore, TR::NodeChecklist& visited, int32_t baseChildVN = -1);
   void     checkDefsAndUses();
   bool     checkDefsAndUses(TR::Node *node, Candidate *candidate);

   /**
    * Walk through trees looking for \c aselect operations.  For the
    * value operands of an \c aselect, populate \ref _nodeUsesThroughAselect
    * with an entry mapping from the operand to a list containing the
    * \c aselect nodes that refer to it.
    *
    * \see _nodeUsesThroughAselect
    */
   void     gatherUsesThroughAselect(void);

   /**
    * Recursive implementation method for \ref gatherUsesThroughAselect
    *
    * \param[in] node The root of the subtree that is to be processed
    * \param[inout] visited A bit vector indicating whether a node has
    *                       already been visited
    */
   void     gatherUsesThroughAselectImpl(TR::Node *node, TR::NodeChecklist& visited);

   /**
    * Add an entry to \ref _nodeUsesThroughAselect mapping from the child node
    * of \c aselectNode at the specified index to the \c aselectNode itself.
    *
    * \param[in] aselectNode A node whose opcode is an \c aselect operation
    * \param[in] idx The index of a child of \c aselectNode
    */
   void     associateAselectWithChild(TR::Node *aselectNode, int32_t idx);

   /**
    * Trace contents of \ref _nodeUsesThroughAselect
    */
   void     printUsesThroughAselect(void);

   /**
    * Check whether \c node, which is a use of the candidate for stack
    * allocation, \c candidate, is itself used as one of the value operands
    * in an \c aselect operation, as found in \ref _nodeUsesThroughAselect.
    * If it is, the value number of any such \c aselect is added to the list
    * of value numbers associated with the candidate.
    *
    * \param[in] node The use of \c candidate that is under consideration
    * \param[in] candidate A candidate for stack allocation
    */
   bool     checkUsesThroughAselect(TR::Node *node, Candidate *candidate);
   bool     checkOtherDefsOfLoopAllocation(TR::Node *useNode, Candidate *candidate, bool isImmediateUse);
   bool     checkOverlappingLoopAllocation(TR::Node *useNode, Candidate *candidate);
   bool     checkOverlappingLoopAllocation(TR::Node *node, TR::Node *useNode, TR::Node *allocNode, rcount_t &numReferences);

   /**
    * Visit nodes in the subtree, keeping track of those visited in
    * @ref _visitedNodes
    * @param[in] node The subtree that is to be visited
    */
   void     visitTree(TR::Node *node);

   /**
    * Collect aliases of an allocation node in the specified subtree
    * in @ref _aliasesOfOtherAllocNode
    * Nodes in the subtree that are visited are tracked in
    * @ref _visitedNodes, and those that have been marked as already visited
    * are skipped.
    * @param[in] node The subtree that is to be visited
    * @param[in] allocNode The allocation node whose aliases are to be collected
    */
   void     collectAliasesOfAllocations(TR::Node *node, TR::Node *allocNode);

   bool     checkAllNewsOnRHSInLoopWithAliasing(int32_t defIndex, TR::Node *useNode, Candidate *candidate);
   bool     usesValueNumber(Candidate *candidate, int32_t valueNumber);
   Candidate *findCandidate(int32_t valueNumber);


   bool     detectStringCopy(TR::Node *node);
   void     markCandidatesUsedInNonColdBlock(TR::Node *node);

   bool     checkIfUseIsInLoopAndOverlapping(Candidate *candidate, TR::TreeTop *defTree, TR::Node *useNode);
   bool     checkIfUseIsInLoopAndOverlapping(TR::TreeTop *start, TR::TreeTop *end, TR::TreeTop *defTree, TR::Node *useNode, TR::NodeChecklist& visited, TR::BlockChecklist& vBlocks, bool & decisionMade);
   bool     checkUse(TR::Node *node, TR::Node *useNode, TR::NodeChecklist& visited);
   bool     checkIfUseIsInSameLoopAsDef(TR::TreeTop *defTree, TR::Node *useNode);

   bool     isEscapePointCold(Candidate *candidate, TR::Node *node);
   bool     checkIfEscapePointIsCold(Candidate *candidate, TR::Node *node);
   void     forceEscape(TR::Node *node, TR::Node *reason, bool forceFail = false);
   bool     restrictCandidates(TR::Node *node, TR::Node *reason, restrictionType);
   //void     referencedField(TR::Node *base, TR::Node *field, bool isStore, bool seenSelfStore = false);
   void     referencedField(TR::Node *base, TR::Node *field, bool isStore, bool seenSelfStore = false, bool seenStoreToLocalObject = false);
   TR::Node *resolveSniffedNode(TR::Node *node);
   void     checkEscape(TR::TreeTop *firstTree, bool isCold, bool & ignoreRecursion);
   void     checkEscapeViaNonCall(TR::Node *node, TR::NodeChecklist& visited);
   void     checkEscapeViaCall(TR::Node *node, TR::NodeChecklist& visited, bool & ignoreRecursion);
   int32_t  sniffCall(TR::Node *callNode, TR::ResolvedMethodSymbol *methodSymbol, bool ignoreOpCode, bool isCold, bool & ignoreRecursion);
   void     checkObjectSizes();
   void     fixupTrees();
   void     anchorCandidateReference(Candidate *candidate, TR::Node *reference);
   bool     fixupNode(TR::Node *node, TR::Node *parent, TR::NodeChecklist& visited);
   bool     fixupFieldAccessForContiguousAllocation(TR::Node *node, Candidate *candidate);
   bool     fixupFieldAccessForNonContiguousAllocation(TR::Node *node, Candidate *candidate, TR::Node *parent);
   void     makeLocalObject(Candidate *candidate);
   void     avoidStringCopyAllocation(Candidate *candidate);

   /** \brief
    *     Attempts to zero initialize a stack allocated candidate using TR::arrayset.
    *
    *  \param candidate
    *     The candidate to zero initialize.
    *
    *  \param precedingTreeTop
    *     The preceding tree top to which the TR::arrayset will be attached to.
    *
    *  \return
    *     true if a TR::arrayset was emitted to zero initialize the candidate; false otherwise.
    */
   bool tryToZeroInitializeUsingArrayset(Candidate* candidate, TR::TreeTop* precedingTreeTop);

   void     makeContiguousLocalAllocation(Candidate *candidate);
   void     makeNonContiguousLocalAllocation(Candidate *candidate);
   void     heapifyForColdBlocks(Candidate *candidate);

   /**
    * \brief Store the supplied address to the specified temporary
    *
    * \param candidate
    *     The candidate that is being heapified
    *
    * \param addr
    *     The address of the possibly heapified object
    *
    * \param symRef
    *     The \ref TR::SymbolReference for the temporay
    *
    * \return A pointer to the \ref TR::TreeTop containing the store
    */
   TR::TreeTop *storeHeapifiedToTemp(Candidate *candidate, TR::Node *addr, TR::SymbolReference *symRef);
   bool     inlineCallSites();
   void     scanForExtraCallsToInline();
   bool     alwaysWorthInlining(TR::Node *callNode);
   bool     devirtualizeCallSites();
   bool     hasFlushOnEntry(int32_t blockNum) {if (_blocksWithFlushOnEntry->get(blockNum)) return true; return false;}
   void     setHasFlushOnEntry(int32_t blockNum) {_blocksWithFlushOnEntry->set(blockNum);}
   void     rememoize(Candidate *c, bool mayDememoizeNextTime=false);

   void     printCandidates(char *);

   char *getClassName(TR::Node *classNode);

   bool isImmutableObject(TR::Node *node);
   bool isImmutableObject(Candidate *candidate);

   TR::Node *createConst(TR::Compilation *comp, TR::Node *node, TR::DataType type, int value);

   struct TR_CallSitesFixedMapper : public TR_Link<TR_CallSitesFixedMapper>
      {
      TR_CallSitesFixedMapper(TR::TreeTop * virtualCallSite, TR::TreeTop * directCallSite)
	 :	 _vCallSite(virtualCallSite), _dCallSite(directCallSite){ }

      TR::TreeTop * _vCallSite;
      TR::TreeTop * _dCallSite;
      };

   class PersistentData : public TR::OptimizationData
      {
      public:
      PersistentData(TR::Compilation *comp)
         : TR::OptimizationData(comp),
           _totalInlinedBytecodeSize(0),
           _totalPeekedBytecodeSize(0)
         {
         _symRefList.setFirst(NULL);
         _peekableCalls = new (comp->trHeapMemory()) TR_BitVector(0, comp->trMemory(), heapAlloc);
         _processedCalls = new (comp->trHeapMemory()) TR_BitVector(0, comp->trMemory(), heapAlloc);
         }

      int32_t                    _totalInlinedBytecodeSize;
      int32_t                    _totalPeekedBytecodeSize;
      TR_BitVector              *_peekableCalls;
      TR_BitVector              *_processedCalls;
      TR_LinkHead<SymRefCache>   _symRefList;
      };

   PersistentData * getOptData() { return (PersistentData *) manager()->getOptData(); }

   //TR::TreeTop *findCallSiteFixed(TR::TreeTop * virtualCallSite);
   bool findCallSiteFixed(TR::TreeTop * virtualCallSite);

   TR::SymbolReference        *_newObjectNoZeroInitSymRef;
   TR::SymbolReference        *_newValueSymRef;
   TR::SymbolReference        *_newArrayNoZeroInitSymRef;
   TR::SymbolReference        *_aNewArrayNoZeroInitSymRef;
   TR_UseDefInfo             *_useDefInfo;
   bool                      _invalidateUseDefInfo;
   TR_BitVector              *_otherDefsForLoopAllocation;
   TR_BitVector              *_ignorableUses;
   TR_BitVector              *_nonColdLocalObjectsValueNumbers;
   TR_BitVector              *_allLocalObjectsValueNumbers;
   TR_BitVector              *_notOptimizableLocalObjectsValueNumbers;
   TR_BitVector              *_notOptimizableLocalStringObjectsValueNumbers;
   TR_BitVector              *_blocksWithFlushOnEntry;
   TR_BitVector              *_visitedNodes;

   CallLoadMap               *_callsToProtect;

   /**
    * Contains sym refs that are just aliases for a fresh allocation
    * i.e., it is used to track allocations in cases such as
    * ...
    * a = new A()
    * ...
    * b = a
    * ...
    * c = b
    *
    * In this case a, b and c will all be considered aliases of an alloc node
    * and so a load of any of those sym refs will be treated akin to how the
    * fresh allocation would be treated
    */
   TR_BitVector              *_aliasesOfAllocNode;

   /**
    * Contains sym refs that are just aliases for a second fresh allocation
    * that is under consideration, as with @ref _aliasesOfAllocNode
    */
   TR_BitVector              *_aliasesOfOtherAllocNode;
   TR_ValueNumberInfo        *_valueNumberInfo;
   TR_LinkHead<Candidate>     _candidates;
   TR_Array<TR::Node*>        *_parms;
   TR_ScratchList<TR::TreeTop> _inlineCallSites;
   TR_ScratchList<TR::TreeTop> _devirtualizedCallSites;
   TR_LinkHead<TR_CallSitesFixedMapper> _fixedVirtualCallSites;

   List<TR::Node>          _dememoizedAllocs;
   TR::SymbolReference        *_dememoizationSymRef;

   TR::Block                  *_curBlock;
   TR::TreeTop                *_curTree;
   int32_t                    _sniffDepth;
   int32_t                    _maxSniffDepth;
   int32_t                    _maxPassNumber;
   TR::ResolvedMethodSymbol     *_methodSymbol;
   bool                       _inBigDecimalAdd;
   int32_t                    _maxInlinedBytecodeSize;
   int32_t                    _maxPeekedBytecodeSize;
   bool                       _inColdBlock;
   bool                       _createStackAllocations;
   bool                       _createLocalObjects;
   bool                       _desynchronizeCalls;
   bool                       _classObjectLoadForVirtualCall;
#if CHECK_MONITORS
   bool                       _removeMonitors;
#endif
   bool                       _repeatAnalysis;
   bool                       _somethingChanged;
   TR_ScratchList<TR_DependentAllocations> _dependentAllocations;
   TR_BitVector *             _vnTemp;
   TR_BitVector *             _vnTemp2;

   typedef TR::typed_allocator<TR::Node *, TR::Region &> NodeDequeAllocator;
   typedef std::deque<TR::Node *, NodeDequeAllocator> NodeDeque;

   typedef TR::typed_allocator<std::pair<TR::Node* const, NodeDeque*>, TR::Region&> NodeToNodeDequeMapAllocator;
   typedef std::less<TR::Node*> NodeComparator;
   typedef std::map<TR::Node*, NodeDeque*, NodeComparator, NodeToNodeDequeMapAllocator> NodeToNodeDequeMap;

   /**
    * A mapping from nodes to a \c deque of \c aselect nodes that directly
    * reference them.
    */
   NodeToNodeDequeMap *       _nodeUsesThroughAselect;

   friend class TR_FlowSensitiveEscapeAnalysis;
   friend class TR_LocalFlushElimination;
   friend struct FieldInfo;
   };

//class Candidate;
//class TR_EscapeAnalysis;
//class TR_DependentAllocations;

class TR_LocalFlushElimination
   {
   public:
   TR_ALLOC(TR_Memory::LocalOpts)
   TR_LocalFlushElimination(TR_EscapeAnalysis *, int32_t numAllocations);

   virtual int32_t perform();
   bool examineNode(TR::Node *, TR::TreeTop *, TR::Block *, TR::NodeChecklist& visited);

   TR::Optimizer *        optimizer()                     { return _escapeAnalysis->optimizer(); }
   TR::Compilation *        comp()                          { return _escapeAnalysis->comp(); }

   TR_Memory *               trMemory()                      { return comp()->trMemory(); }
   TR_StackMemory            trStackMemory()                 { return comp()->trStackMemory(); }

   bool                      trace()                         { return _escapeAnalysis->trace(); }

   private:
   TR_LinkHead<Candidate> *_candidates;
   TR_LinkHead<FlushCandidate> *_flushCandidates;
   TR_EscapeAnalysis *_escapeAnalysis;
   int32_t _numAllocations;
   TR_BitVector *_allocationInfo;
   TR_BitVector *_temp;
   TR_ScratchList<TR_DependentAllocations> _dependentAllocations;
   };



#if CHECK_MONITORS
class TR_MonitorStructureChecker
   {
   public:
   TR_ALLOC(TR_Memory::EscapeAnalysis)
   TR_MonitorStructureChecker() {}

   // returns true if illegal structure is found
   bool checkMonitorStructure(TR::CFG *cfg);

   private:
   void processBlock(TR::Block *block);
   void propagateInfoTo(TR::Block *block, int32_t inInfo);

   int32_t            *_blockInfo;
   TR_BitVector       *_seenNodes;
   bool                _foundIllegalStructure;
   };

#endif
#endif
