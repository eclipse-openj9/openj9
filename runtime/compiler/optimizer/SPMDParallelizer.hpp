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

#ifndef SPMDPARALLELIZER_INCL
#define SPMDPARALLELIZER_INCL

#include "optimizer/GeneralLoopUnroller.hpp"

class TR_SPMDKernelParallelizer : public TR_LoopTransformer
   {
   int32_t _parmSlot;
   int32_t _verboseTrace;
   TR_LinkageConventions _helperLinkage;
   TR_HashTab *  _loopDataType;
   CS2::ArrayOf<TR_PrimaryInductionVariable *, TR::Allocator> _pivList;
   TR_BitVector *_reversedBranchNodes; //BitVector to track which ibmTryGPU have had their branch reversed
   TR::Block **_origCfgBlocks;  // blocks at the time of the analysis
   TR_BitVector _visitedNodes;
   bool   _fpreductionAnnotation;

   public:

   TR_SPMDKernelParallelizer(TR::OptimizationManager *manager);

   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_SPMDKernelParallelizer(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   struct TR_SPMDKernelInfo
      {
      TR_ALLOC(TR_Memory::LocalOpts); // dummy
      public:
      TR_SPMDKernelInfo(TR::Compilation *comp, TR_PrimaryInductionVariable *piv): _comp(comp), _piv(piv), _strideNode(NULL), _isPIVVectorized(false)
         {
         _reductionScalarSymRefs = new (comp->trStackMemory()) TR_Array<TR::SymbolReference*>(comp->trMemory(), 4, true, stackAlloc);
         _reductionVectorSymRefs = new (comp->trStackMemory()) TR_Array<TR::SymbolReference*>(comp->trMemory(), 4, true, stackAlloc);
         };

      TR::SymbolReference* getInductionVariableSymRef()
         {
         if (_piv == NULL) return NULL;
         return _piv->getSymRef();
         }

      TR::SymbolReference* getVectorSymRef(TR::SymbolReference* scalarSymRef)
         {
         for (int i = 0; i < _reductionScalarSymRefs->size(); i++)
            {
            if ((*_reductionScalarSymRefs)[i] == scalarSymRef) return (*_reductionVectorSymRefs)[i];
            }
         return NULL;
         }

      void addVectorSymRef(TR::SymbolReference* scalarSymRef, TR::SymbolReference* vectorSymRef)
         {
         _reductionScalarSymRefs->add(scalarSymRef);
         _reductionVectorSymRefs->add(vectorSymRef);
         }

      TR::Node *getStrideNode()       { return _strideNode; }
      void setStrideNode(TR::Node *n) { _strideNode = n; }

      bool getIsNegated()            { return _isNegated; }
      void setIsNegated(bool b)      { _isNegated = b; }

      bool getIsPIVVectorized()            { return _isPIVVectorized; }
      void setIsPIVVectorized(bool b)      { _isPIVVectorized = b; }

      private:
      TR::Compilation *comp() { return _comp; };

      // fields of TR_SPMDKernelInfo
      TR::Compilation *_comp;
      TR_Array<TR::SymbolReference*> *_reductionScalarSymRefs;
      TR_Array<TR::SymbolReference*> *_reductionVectorSymRefs;
      TR_PrimaryInductionVariable *_piv;
      TR::Node *_strideNode;
      bool _isNegated;
      bool _isPIVVectorized;

      // bool _disableSPMD;
      // using unrollCount tentatively so far
      //   int32_t _waysOfSIMD;
      };

   enum TR_SPMDScopeInfoType 
      {
      scopeSingleKernel = 0,
      scopeNaturalLoop = 1
      };

   class TR_SPMDScopeInfo
      {
      public:
      TR_ALLOC(TR_Memory::LocalOpts); // dummy

      TR_SPMDScopeInfo(TR::Compilation *comp, TR_RegionStructure *loop, TR_SPMDScopeInfoType scopeType)
         : _scopeType(scopeType), _loop(loop), _envelopeLoopTransformed(false), _coldLoops(comp->trMemory()), _exitRegionLocations(comp->trMemory())
         {
         if (scopeType == scopeNaturalLoop) 
            {
            _kernels =  new (comp->trHeapMemory()) List<TR_RegionStructure>(comp->trMemory());
            _flushGPUBlocks = new (comp->trHeapMemory()) TR_BitVector(comp->getFlowGraph()->getNextNodeNumber(), comp->trMemory(), stackAlloc, growable);
            }
         }

      List<TR::TreeTop>* getExitLocationsList()
         {
         return &_exitRegionLocations;
         }

      TR_RegionStructure* getSingleKernelRegion() 
         {
         TR_ASSERT(_scopeType == scopeSingleKernel,"getSingleKernelRegion() called when GPU scope is not a single kernel");   
         return _loop;
         }

      TR_RegionStructure* getEnvelopingLoop()
         {
         TR_ASSERT(_scopeType == scopeNaturalLoop,"getEnvelopingLoop() called when GPU scope is not a natural loop");
         return _loop;
         }
      
      void setKernelList(List<TR_RegionStructure> &gpuKernels)
         {
         TR_ASSERT(_scopeType == scopeNaturalLoop,"setKernelList() called when GPU scope is not a natural loop");
         _kernels->deleteAll();
         _kernels->add(gpuKernels);
         }
      
      List<TR_RegionStructure>* getKernelList() 
         { 
         TR_ASSERT(_scopeType == scopeNaturalLoop,"getKernelList() called when GPU scope is not a natural loop"); 
         return _kernels; 
         }       
     
      List<TR_RegionStructure>* getColdLoops() 
         { 
         TR_ASSERT(_scopeType == scopeNaturalLoop,"getColdLoops() called when GPU scope is not a natural loop"); 
         return &_coldLoops; 
         }       
       
      TR_BitVector* getFlushGPUBlocks() 
         { 
         TR_ASSERT(_scopeType == scopeNaturalLoop,"getFlushGPUBlocks() called when GPU scope is not a natural loop"); 
         return _flushGPUBlocks; 
         }       


      TR_SPMDScopeInfoType getScopeType() { return _scopeType; }
      
      TR::SymbolReference *_scopeSymRef;
      TR::SymbolReference *_liveSymRef;
      bool _envelopeLoopTransformed;

      private:
      
      TR_RegionStructure       *_loop;
      TR_SPMDScopeInfoType     _scopeType; 
      List<TR_RegionStructure> *_kernels;
      List<TR_RegionStructure>  _coldLoops;
      TR_BitVector             *_flushGPUBlocks;
      List<TR::TreeTop>         _exitRegionLocations;
      };

   //additional auto SIMD reduction types go here
   enum TR_SPMDReductionOp
      {
      Reduction_OpUninitialized, //we do not know which op the reduction is yet. This is a valid op to end with if the reduction has no operators. eg. sum = sum;
      Reduction_Invalid, //the reduction uses multiple different operators or is unsupported for other reasons
      Reduction_Add,
      Reduction_Mul,
      };

   struct TR_SPMDReductionInfo
      {
      TR_ALLOC(TR_Memory::LocalOpts);

      TR_SPMDReductionInfo(TR::Compilation *comp) : storeNodes(comp->trMemory())
         {
         }

      TR_SPMDReductionOp reductionOp;
      TR_ScratchList<TR::Node> storeNodes;
      TR::SymbolReference *reductionSymRef;
      };

   void collectGPUKernels(TR_RegionStructure *region, List<TR_RegionStructure> &gpuKernels);
   void collectColdLoops(TR_RegionStructure *region, List<TR_RegionStructure> &coldLoops);
   void collectGPUScopes(TR_RegionStructure *region, List<TR_RegionStructure> &gpuKernels, List<TR_SPMDScopeInfo> &gpuScopes);
   bool analyzeGPUScope(TR_SPMDScopeInfo* pScopeInfo);

   void calculateNonColdCPUBlocks(TR_RegionStructure *region, TR_ScratchList<TR::Block> *kernelBlocks,
                               TR_ScratchList<TR::Block> *coldLoopBlocks, SharedSparseBitVector *nonColdCPUBlocks);

   void collectParallelLoops(TR_RegionStructure *region, List<TR_RegionStructure> &simdLoops, TR_HashTab* reductionOperationsHashTab, TR_UseDefInfo *useDefInfo);
   bool processSPMDKernelLoopForSIMDize(TR::Compilation *comp, TR::Optimizer *optimizer, TR_RegionStructure *loop, TR_PrimaryInductionVariable *piv, TR_HashTab* reductionHashTab, int32_t peelCount, TR::Block *invariantBlock);

   bool vectorize(TR::Compilation *comp, TR_RegionStructure *loop, TR_PrimaryInductionVariable *piv, TR_HashTab* reductionHashTab, int32_t peelCount, TR::Optimizer *optimizer=NULL);
   void vectorizer_cleanup(TR::Compilation *comp, TR::Optimizer *optimizer=NULL);

   bool visitNodeToSIMDize(TR::Node *parent, int32_t childIndex, TR::Node *node, TR_SPMDKernelInfo *pSPMDInfo, bool isCheckMode, TR_RegionStructure *loop, TR::Compilation *comp, SharedSparseBitVector* usesInLoop, CS2::ArrayOf<TR::Node *, TR::Allocator> &useNodesOfDefsInLoop, TR_UseDefInfo *useDefInfo, SharedSparseBitVector &defsInLoop, TR_HashTab* reductionHashTab, TR::SymbolReference* storeSymRef);
   bool visitTreeTopToSIMDize(TR::TreeTop *tt, TR_SPMDKernelInfo *pSPMDInfo, bool isCheckMode, TR_RegionStructure *loop, CS2::ArrayOf<TR::Node *, TR::Allocator> &useNodesOfDefsInLoop, TR::Compilation *comp, TR_UseDefInfo *useDefInfo, SharedSparseBitVector &defsInLoop, SharedSparseBitVector* usesInLoop, TR_HashTab* reductionHashTab);

   bool autoSIMDReductionSupported(TR::Compilation *comp, TR::Node *node);
   bool isReduction(TR::Compilation *comp, TR_RegionStructure *loop, TR::Node *node, TR_SPMDReductionInfo* reductionInfo, TR_SPMDReductionOp pathOp);
   bool noReductionVar(TR::Compilation *comp, TR_RegionStructure *loop, TR::Node *node, TR_SPMDReductionInfo* reductionInfo);
   bool reductionLoopEntranceProcessing(TR::Compilation *comp, TR_RegionStructure *loop, TR::SymbolReference *symRef, TR::SymbolReference *vecSymRef, TR_SPMDReductionOp reductionOp);
   bool reductionLoopExitProcessing(TR::Compilation *comp, TR_RegionStructure *loop, TR::SymbolReference *symRef, TR::SymbolReference *vecSymRef, TR_SPMDReductionOp reductionOp);

   bool isSPMDKernelLoop(TR_RegionStructure *region, TR::Compilation *comp);
   bool isParallelForEachLoop(TR_RegionStructure *region, TR::Compilation *comp);
   bool isPerfectNest(TR_RegionStructure *region, TR::Compilation *comp);
   bool isSequentialAccess(TR_RegionStructure *loop, TR::Compilation *comp);
   bool isAffineAccess(TR::Compilation *comp, TR::Node *node, TR_RegionStructure *loop, TR::SymbolReference *piv, int32_t &pivStride);

   void collectDefsAndUses(TR::Node *node, vcount_t visitCount, CS2::ArrayOf<TR::Node *, TR::Allocator> &defs, CS2::ArrayOf<TR::Node *, TR::Allocator> &uses, TR::Compilation *comp);

   TR::Node* findSingleLoopVariant(TR::Node *node, TR_RegionStructure *loop, int* sign, int* constantsOnly);
   int symbolicEvaluateTree(TR::Node *node);

   int32_t getUnrollCount(TR::DataType);
   TR::Node * findLoopDataType(TR::Node*, TR::Compilation *comp);
   void setLoopDataType(TR_RegionStructure *loop,TR::Compilation *comp);
   void genVectorAccessForScalar(TR::Node *parent, int32_t childIndex, TR::Node *node);
   bool checkDataLocality(TR_RegionStructure *loop, CS2::ArrayOf<TR::Node *, TR::Allocator> &useNodesOfDefsInLoop, SharedSparseBitVector &defsInLoop, TR::Compilation *comp, TR_UseDefInfo *useDefInfo, TR_HashTab* reductionHashTab);
   bool checkConstantDistanceDependence(TR_RegionStructure *loop, TR::Node *node1, TR::Node *node2, TR::Compilation *comp, int type);
   bool checkIndependence(TR_RegionStructure *loop, TR_UseDefInfo *useDefInfo, CS2::ArrayOf<TR::Node *, TR::Allocator> &useNodesOfDefsInLoop, SharedSparseBitVector &defsInLoop, TR::Compilation *comp);
   bool checkLoopIteration(TR_RegionStructure *loop, TR::Compilation *comp);

   //for doing data dependence analysis
   bool areNodesEquivalent(TR::Compilation *comp, TR::Node *node1, TR::Node *node2);

   bool processGPULoop(TR_RegionStructure *loop, TR_SPMDScopeInfo* gpuScope);

   bool visitNodeToMapSymbols(TR::Node *node, ListAppender<TR::ParameterSymbol> &parms, ListAppender<TR::AutomaticSymbol> &autos, TR_RegionStructure *loop, TR_PrimaryInductionVariable *piv, int32_t lineNumber, int32_t visitCount);

   bool visitNodeToDetectArrayAccesses(TR::Node *node, TR_RegionStructure *loop, TR_PrimaryInductionVariable *piv, int32_t visitCount, int32_t lineNumber, bool &arrayNotFound, int trace, bool postDominates);

   bool visitCPUNode(TR::Node *node, int32_t visitCount, TR::Block *block, TR_BitVector *flushCPUBlocks);

   int32_t findArrayElementSize(TR::Node *node);

   void convertIntoParm(TR::Node *node, int32_t elementSize, ListAppender<TR::ParameterSymbol> &parms);

   bool addRegionCost(TR_RegionStructure *region, TR_RegionStructure *loop, TR::Block *loopInvariantBlock, TR::SymbolReference *lambdaCost);
   bool estimateGPUCost(TR_RegionStructure *region, TR::Block * loopInvariantBlock, TR::SymbolReference *lambdaCost);

   void reportRejected(const char *msg1, const char *msg2, int32_t lineNumber, TR::Node *node);

   void insertGPUTemporariesLivenessCode(List<TR::TreeTop> *exitPointsList, TR::SymbolReference *liveSymRef, bool firstKernel);
   void generateGPUParmsBlock(TR::SymbolReference *allocSymRef, TR::Block *populateParmsBlock, TR::Node *firstNode);
   void insertGPUEstimate(TR::Node *firstNode, TR::Block *estimateGPUBlock, TR::SymbolReference *lambdaCost, TR::SymbolReference *dataCost, TR_PrimaryInductionVariable *piv, TR::TreeTop *loopTestTree, TR::Block* origLoopBlock, TR::Block* loopInvariantBlock, TR::SymbolReference *scopeSymRef);
   void insertGPUParmsAllocate(TR::Node *firstNode, TR::Block *allocateParmsBlock, TR::SymbolReference *allocSymRef, TR::SymbolReference *launchSymRef);
   void insertGPUInvalidateSequence(TR::Node *firstNode, TR::Block *invalidateGPUBlock, TR::SymbolReference *initSymRef);
   void insertGPUErrorHandler(TR::Node *firstNode, TR::Block *errorHandleBlock, TR::SymbolReference *initSymRef, TR::Block* recoveryBlock);
   void insertGPUKernelExit(TR::Node *firstNode, TR::Block *kernelExitGPUBlock, TR::SymbolReference *initSymRef, TR::SymbolReference *launchSymRef);
   void insertGPUCopyFromSequence(TR::Node *firstNode, TR::Block *copyFromGPUBlock, TR::SymbolReference *initSymRef, TR::SymbolReference *launchSymRef, TR_PrimaryInductionVariable *piv);
   void insertGPUCopyToSequence(TR::Node *firstNode, TR::Block *copyToGPUBlock, TR::SymbolReference *initSymRef, TR_PrimaryInductionVariable *piv);
   void insertGPUKernelLaunch(TR::SymbolReference *allocSymRef, TR::SymbolReference *initSymRef, TR::Block *launchBlock, TR::Node *firstNode, TR_PrimaryInductionVariable *piv, TR::TreeTop *loopTestTree, int kernelID, bool hasExceptionChecks);

   void insertGPURegionEntry(TR::Block * loopInvariantBlock, TR::SymbolReference * initSymRef, int gpuPtxCount, TR_SPMDScopeInfoType scopeType);
   void insertGPURegionExits(List<TR::Block>* exitBlocks, TR::SymbolReference *initSymRef, int gpuPtxCount, TR::SymbolReference *liveSymRef, List<TR::TreeTop> *exitPointsList);
   void insertGPURegionExitInRegionExits(List<TR::Block> *exitBlocks, List<TR::Block> *blocksInLoop,TR::SymbolReference *initSymRef, TR::SymbolReference *liveSymRef, List<TR::TreeTop> *exitPointsList);
   void insertFlushGPU(TR_BitVector *flushGPUBlocks, TR::Block **cfgBlocks, TR::SymbolReference *scopeSymRef);
   TR::Node* insertFlushGPU(TR::Block* flushGPUBlock, TR::SymbolReference *scopeSymRef);
   };

#endif

