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

#include "optimizer/SPMDParallelizer.hpp"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/allocator.h"
#include "cs2/arrayof.h"
#include "cs2/bitvectr.h"
#include "cs2/sparsrbit.h"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/HashTab.hpp"
#include "infra/List.hpp"
#include "infra/TRCfgEdge.hpp"
#include "infra/TRCfgNode.hpp"
#include "optimizer/Dominators.hpp"
#include "optimizer/InductionVariable.hpp"
#include "optimizer/LoopCanonicalizer.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/SPMDPreCheck.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/UseDefInfo.hpp"
#include "optimizer/ValueNumberInfo.hpp"
#include "runtime/J9Runtime.hpp"
#include "ras/DebugCounter.hpp"
#include "env/annotations/GPUAnnotation.hpp"

#define OPT_SIMD_DETAILS "O^O AUTO SIMD: "

#define INVALID_STRIDE INT_MAX
#define VECTOR_SIZE 16
#define INVALID_ADDR  (TR::Node *)-1

namespace TR { class OptimizationManager; }
namespace TR { class ParameterSymbol; }

typedef TR::CodeGenerator::gpuMapElement gpuMapElement;


// SIMD_TODO: use visitCount or some caching?
static bool hasPIV(TR::Node *node, TR::SymbolReference *piv)
   {
   if (node->getOpCodeValue() == TR::iload &&
       node->getSymbolReference() == piv)
      return true;

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      if (hasPIV(node->getChild(i), piv))
         return true;
      }
   return false;
   }

int32_t TR_SPMDKernelParallelizer::getUnrollCount(TR::DataType dt)
   {
   int32_t unrollCount = -1;
   switch(dt)
      {
      case TR::Int8:
         unrollCount=VECTOR_SIZE; break;
      case TR::Int16:
         unrollCount=VECTOR_SIZE/2;  break;
      case TR::Int32:
      case TR::Float:
         unrollCount=VECTOR_SIZE/4;  break;
      case TR::Int64:
      case TR::Double:
         unrollCount=VECTOR_SIZE/8;  break;
      }

   return unrollCount;
   }

TR::Node * TR_SPMDKernelParallelizer::findLoopDataType(TR::Node* node, TR::Compilation *comp)
   {
   if(!node)
      return node;

   if (_visitedNodes.isSet(node->getGlobalIndex()))
      return NULL;

   _visitedNodes.set(node->getGlobalIndex());

   if(node->getOpCode().hasSymbolReference() && node->getSymbolReference()->getSymbol()->isArrayShadowSymbol())
      return node;

   for(int32_t i=0;i<node->getNumChildren();i++)
      {
      if(findLoopDataType(node->getChild(i),comp))
       return node;
      }
   return NULL;
   }

void TR_SPMDKernelParallelizer::setLoopDataType(TR_RegionStructure *loop,TR::Compilation *comp)
   {

   TR_ScratchList<TR::Block> blocksInLoop(trMemory());
   loop->getBlocks(&blocksInLoop);
   ListIterator<TR::Block> blocksIt(&blocksInLoop);

   //clear the node visit list as we need to visit again.
   _visitedNodes.empty();

   for (TR::Block *nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      {
      for (TR::TreeTop *tt = nextBlock->getEntry() ; tt != nextBlock->getExit() ; tt = tt->getNextTreeTop())
         {
         TR::Node *node = findLoopDataType(tt->getNode(),comp);
         if(node)
            {
             TR_HashId id = 0;
            _loopDataType->add(loop,id,node->duplicateTree());
            return;
            }
         }
      }
   }


// SIMD_TODO: use visitCount or some caching?
bool TR_SPMDKernelParallelizer::isAffineAccess(TR::Compilation *comp, TR::Node *node, TR_RegionStructure *loop, TR::SymbolReference *piv, int32_t &pivStride)
   {
   int32_t child1Stride = INVALID_STRIDE;
   int32_t child2Stride = INVALID_STRIDE;

   pivStride = 0;


   if (node->getOpCodeValue() == TR::i2l)
      {
      if (isAffineAccess(comp, node->getFirstChild(), loop, piv, child1Stride))
         {
         pivStride = child1Stride;
         return true;
         }
      }
   else if (node->getOpCode().isAdd() || node->getOpCode().isSub())
      {
      if (isAffineAccess(comp, node->getFirstChild(), loop, piv, child1Stride) &&
          isAffineAccess(comp, node->getSecondChild(), loop, piv, child2Stride))
         {
         if (child1Stride != INVALID_STRIDE && child2Stride != INVALID_STRIDE)
            pivStride = node->getOpCode().isAdd() ? child1Stride + child2Stride : child1Stride - child2Stride;
         else
            {
            pivStride = INVALID_STRIDE;
            }
         return true;
         }
      }
   else if (node->getOpCode().isMul())
      {
      bool isSecondChildInvar = loop->isExprInvariant(node->getSecondChild());
      bool isFirstChildInvar = loop->isExprInvariant(node->getFirstChild());

      if (isFirstChildInvar && isSecondChildInvar)
         {
         return true;
         }
      else if (isSecondChildInvar)
         {
         if (isAffineAccess(comp, node->getFirstChild(), loop, piv, child1Stride))
            {
            if (child1Stride == 0)
               return true;
            else if (child1Stride != INVALID_STRIDE &&
               node->getSecondChild()->getOpCode().isLoadConst())
               pivStride = node->getSecondChild()->get64bitIntegralValue() * child1Stride;
            else
               {
               pivStride = INVALID_STRIDE;
               }
            return true;
            }
         }
      else if (isFirstChildInvar)
         {
         if (isAffineAccess(comp, node->getSecondChild(), loop, piv, child2Stride))
            {
            if (child2Stride == 0)
               return true;
            else if (child2Stride != INVALID_STRIDE &&
               node->getFirstChild()->getOpCode().isLoadConst())
               pivStride = node->getFirstChild()->get64bitIntegralValue() * child2Stride;
            else
               {
               pivStride = INVALID_STRIDE;
               }
            return true;
            }
         }
      }
   else if (loop->isExprInvariant(node))
      {
      return true;
      }
   else if(node->getOpCodeValue() == TR::iload)
      {
      if (node->getSymbolReference() == piv)
         {
         pivStride = 1;
         return true;
         }
      for (int32_t idx = 0; idx < _pivList.NumberOfElements(); idx++) // SIMD_TODO: faster way ?
         {
         if (node->getSymbolReference() == _pivList[idx]->getSymRef())
            {
            return true;
            }
         }
      }
   return false;
   }

void TR_SPMDKernelParallelizer::genVectorAccessForScalar(TR::Node *parent, int32_t childIndex, TR::Node *node)
   {
   //we need to duplicate tree because this node could be commoned with any other expression inside the loop, e.g. address expression. we don't want to vectorize all the common-ed nodes
   TR::Node *splatsNode = TR::Node::create(TR::vsplats, 1, node->duplicateTree());
   node->recursivelyDecReferenceCount();
   // can visit the commoned node again, if needed.
   _visitedNodes.reset(node->getGlobalIndex());
   parent->setAndIncChild(childIndex, splatsNode);
   }



static void collectUses(TR::Node *defNode, TR::Compilation *comp, TR_UseDefInfo *info, CS2::ArrayOf<TR::Node *, TR::Allocator> &useNodesOfDefsInLoop)
   {
   int32_t useDefIndex = defNode->getUseDefIndex();

   TR_UseDefInfo::BitVector usesOfThisDef(comp->allocator());
   info->getUsesFromDef(usesOfThisDef, useDefIndex);

   if (usesOfThisDef.IsZero())
      return;

   TR_UseDefInfo::BitVector::Cursor cursor(usesOfThisDef);
   for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
      {
      int32_t useIndex = (int32_t) cursor + info->getFirstUseIndex();
      useNodesOfDefsInLoop[useNodesOfDefsInLoop.NumberOfElements()]=info->getNode(useIndex);
      }

   }

bool TR_SPMDKernelParallelizer::visitTreeTopToSIMDize(TR::TreeTop *tt, TR_SPMDKernelInfo *pSPMDInfo, bool isCheckMode, TR_RegionStructure *loop, CS2::ArrayOf<TR::Node *, TR::Allocator> &useNodesOfDefsInLoop, TR::Compilation *comp, TR_UseDefInfo *useDefInfo, SharedSparseBitVector &defsInLoop, SharedSparseBitVector* usesInLoop, TR_HashTab* reductionHashTab)
   {
   TR::Node *node = tt->getNode();
   TR::ILOpCode scalarOp = node->getOpCode();
   TR::SymbolReference *piv = pSPMDInfo->getInductionVariableSymRef();
   bool trace = comp->trace(OMR::SPMDKernelParallelization);

   // SIMD_TODO: check first child if node is TR::TreeTop 

   if (trace)
      {
      traceMsg(comp,"   Visiting Tree Top [%p] during %s mode \n", node, isCheckMode ? "detection" : "transformation");
      }

   if (scalarOp.isStore())
      {
      TR_HashId id = 0;
      if (scalarOp.isIndirect())
         {
         if (!_loopDataType->locate(loop,id))
            {
            _loopDataType->add(loop,id,node->duplicateTree());
            _loopDataType->locate(loop,id); //add can invalidate hashId so locate is rerun to refresh the hashId
            }

         if (((TR::Node *)_loopDataType->getData(id))->getSize() != node->getSize() && isCheckMode)
            {
            if (trace) traceMsg(comp,"   Node size does not match loop data type for node: %p\n", node);
            return false;
            }

         TR::SymbolReference *symRef = node->getSymbolReference();
         TR::SymbolReference *vecSymRef = pSPMDInfo->getVectorSymRef(symRef);
         TR::ILOpCode scalarOp = node->getOpCode();
         TR::ILOpCodes vectorOpCode = TR::ILOpCode::convertScalarToVector(scalarOp.getOpCodeValue());

         if (isCheckMode && vectorOpCode == TR::BadILOp)
            return false;

         TR_ASSERT(vectorOpCode != TR::BadILOp, "BAD IL Opcode to be assigned during transformation");

         if (isCheckMode && !comp->cg()->getSupportsOpCodeForAutoSIMD(vectorOpCode, node->getDataType()))
            return false;

         if (!isCheckMode)
            {
            // just convert the original scalar store into vector store
            TR::Node::recreate(node, vectorOpCode);
            vecSymRef = comp->getSymRefTab()->findOrCreateArrayShadowSymbolRef(node->getDataType().scalarToVector(), NULL);
            node->setSymbolReference(vecSymRef);
            }
         else if (loop->isExprInvariant(node->getFirstChild()))
            {
            if (trace)
               traceMsg(comp, "   node %p has loop invariant address\n", node);
            return false;
            }
         else
            {
            int32_t pivStride = INVALID_STRIDE;
            bool affine = isAffineAccess(comp, node->getFirstChild(), loop, piv, pivStride);
            if (trace)
               traceMsg(comp, "   node %p affine = %d stride = %d\n", node, affine, pivStride);

            if (!affine || !(pivStride*getUnrollCount(node->getDataType()) == VECTOR_SIZE))
               return false;
            }
         return visitNodeToSIMDize(node, 1, node->getSecondChild(), pSPMDInfo, isCheckMode, loop, comp, usesInLoop, useNodesOfDefsInLoop, useDefInfo, defsInLoop, reductionHashTab, /*storeSymRef*/0);
         }
      else
         {
         // Direct store
         if (node->getOpCodeValue() == TR::istore )
            {
            //check if it's one of the induction variables
            bool isPIVDefNode = false;
            for (int32_t iv=0; iv < _pivList.NumberOfElements(); iv++)
               {
               if (node->getSymbolReference() == _pivList[iv]->getSymRef())
                  {
                  isPIVDefNode=true;
                  break;
                  }
               }

            if (isPIVDefNode)
               {
               int32_t pivStride = INVALID_STRIDE;
               bool isAffine = isAffineAccess(comp, node->getFirstChild(), loop, piv, pivStride);

               // if this is transformation mode and this PIV is used by a vectorized expression and satisfies
               // affine access conditions, duplicate the trees to create a vectorized loop increment
               // for the vectorized version of the PIV
               if (!isCheckMode && pSPMDInfo->getIsPIVVectorized() && isAffine)
                  {
                  // adding increments for vectorized induction variables at the same location
                  // where the scalar induction variable is incremented

                  TR::TreeTop *currTree=tt;
                  TR::TreeTop *prevTree=tt->getPrevTreeTop();
                  TR::Node * dupNode = node->duplicateTree();
                  TR::TreeTop* dupTree = TR::TreeTop::create(comp, dupNode, NULL, NULL);
                  prevTree->join(dupTree);
                  dupTree->join(currTree);

                  TR::SymbolReference *symRef = node->getSymbolReference();
                  TR::SymbolReference *vecSymRef = pSPMDInfo->getVectorSymRef(symRef);
                  if (vecSymRef == NULL)
                     {
                     vecSymRef = comp->cg()->allocateLocalTemp(node->getDataType().scalarToVector()); // need to handle alignment?
                     pSPMDInfo->addVectorSymRef(symRef, vecSymRef);

                     if (trace)
                         traceMsg(comp, "   created new symRef #%d for #%d\n", vecSymRef->getReferenceNumber(), symRef->getReferenceNumber());
                     }

                  TR::ILOpCode scalarOp = node->getOpCode();
                  TR::ILOpCodes vectorOpCode = TR::ILOpCode::convertScalarToVector(scalarOp.getOpCodeValue());
                  TR_ASSERT(vectorOpCode != TR::BadILOp, "BAD IL Opcode to be assigned during transformation");

                  dupNode->setSymbolReference(vecSymRef);
                  TR::Node::recreate(dupNode, vectorOpCode);

                  if (trace)
                     traceMsg(comp, "   Duplicating trees [%p] into [%p] and vectorizing for Vectorized PIV increment\n", node, dupNode);

                  return visitNodeToSIMDize(dupNode, 0, dupNode->getFirstChild(), pSPMDInfo, false, loop, comp, usesInLoop, useNodesOfDefsInLoop, useDefInfo, defsInLoop, reductionHashTab, /*storeSymRef*/0);
                  }

               // this is an increment step of the PIV, don't vectorize it
               return isAffine;
               }
            }

         if (isCheckMode)
            {
            TR_SPMDReductionInfo* reductionInfo;
            if (reductionHashTab->locate(node->getSymbolReference(), id))
               {
               reductionInfo = (TR_SPMDReductionInfo*)reductionHashTab->getData(id);
               if (trace) traceMsg(comp, "   visitTreeTopToSIMDize: Found reductionInfo for node: %p, symRef: %p\n", node, node->getSymbolReference());
               }
            else
               {
               reductionInfo = new (comp->trStackMemory()) TR_SPMDReductionInfo(comp);
               reductionInfo->reductionOp = Reduction_OpUninitialized;
               reductionInfo->reductionSymRef = node->getSymbolReference();
               reductionHashTab->add(node->getSymbolReference(), id, reductionInfo);

               if (trace) traceMsg(comp, "   visitTreeTopToSIMDize: Created reductionInfo for node: %p, symRef: %p\n", node, node->getSymbolReference());
               }

            //if autoSIMD reduction is not supported, immediately set the reduction to invalid.
            if (!autoSIMDReductionSupported(comp, node))
               reductionInfo->reductionOp = Reduction_Invalid;

            //Similar to the piv, nodes that store to a reduction symref are not added to defsInLoop.
            //However, we do not know which symrefs are reduction symrefs at this time.
            //As a result we keep track of potential stores to reduction symrefs.
            //If we find out that it is not a supported reduction symref, the node is added to defsInLoop

            //If the reductionOp field is Reduction_Invalid we already know it is not a valid or supported reduction
            if (reductionInfo->reductionOp != Reduction_Invalid)
               {
               //isReduction is run to check that the reduction matches the reduction pattern
               //-only uses the reduction symref once
               //-reduction operation is supported (currently only add and mul)
               //-only the reduction operation is used between the store node and the reduction variable load
               if (isReduction(comp, loop, node->getFirstChild(), reductionInfo, reductionInfo->reductionOp))
                  {
                  if (trace) traceMsg(comp, "   visitTreeTopToSIMDize: Reduction pattern match on node: %p\n", node);
                  //add node to list of nodes that might be a store to a reduction variable
                  reductionInfo->storeNodes.add(node);
                  }
               else
                  {
                  //we know this isn't a supported reduction at this point
                  if (trace) traceMsg(comp, "   visitTreeTopToSIMDize: Reduction pattern mismatch for node: %p\n", node);

                  reductionInfo->reductionOp = Reduction_Invalid;

                  defsInLoop[node->getGlobalIndex()] = true;
                  collectUses(node, comp, useDefInfo, useNodesOfDefsInLoop);

                  //the symref is not a reduction variable or not a supported reduction variable
                  //as a result the nodes that store to it are no longer considered stores to a reduction variable
                  //this adds the recorded nodes back into defsInLoop
                  ListIterator<TR::Node> nodeIt(&reductionInfo->storeNodes);
                  for (TR::Node *nextNode = nodeIt.getCurrent(); nextNode; nextNode=nodeIt.getNext())
                     {
                     if (trace) traceMsg(comp, "   visitTreeTopToSIMDize: Adding recorded invalid reduction node to defsInLoop. node: %p\n", nextNode);
                     defsInLoop[nextNode->getGlobalIndex()] = true;
                     collectUses(nextNode, comp, useDefInfo, useNodesOfDefsInLoop);
                     }
                  }
               }
            else
               {
               //non-reductions are just added to defsInLoop
               defsInLoop[node->getGlobalIndex()] = true;
               collectUses(node, comp, useDefInfo, useNodesOfDefsInLoop);
               }
            }

         TR::ILOpCode scalarOp = node->getOpCode();
         TR::ILOpCodes vectorOpCode = TR::ILOpCode::convertScalarToVector(scalarOp.getOpCodeValue());

         if (isCheckMode && vectorOpCode == TR::BadILOp)
            return false;

         if (loop->isExprInvariant(node->getFirstChild()))
            {
            if (isCheckMode)
               return true;
            if (!isCheckMode)
               {
               TR::TreeTop *currTree=tt;
               TR::TreeTop *prevTree=tt->getPrevTreeTop();
               TR::Node * dupNode = node->duplicateTree();
               TR::TreeTop* dupTree = TR::TreeTop::create(comp, dupNode, NULL, NULL);
               prevTree->join(dupTree);
               dupTree->join(currTree);
               }
            }


         if (!_loopDataType->locate(loop,id))
            {
            _loopDataType->add(loop,id,node->duplicateTree());
            _loopDataType->locate(loop,id); //add can invalidate hashId so locate is rerun to refresh the hashId
            }

         if (((TR::Node *)_loopDataType->getData(id))->getSize() != node->getSize() && isCheckMode)
            {
            if (trace) traceMsg(comp,"   Node size does not match loop data type for node: %p\n", node);
            return false;
            }

         TR::SymbolReference *symRef = node->getSymbolReference();
         TR::SymbolReference *vecSymRef = pSPMDInfo->getVectorSymRef(symRef);

         TR_ASSERT_FATAL(vectorOpCode != TR::BadILOp, "BAD IL Opcode to be assigned during transformation");

         if (isCheckMode && !comp->cg()->getSupportsOpCodeForAutoSIMD(vectorOpCode, node->getDataType()))
            return false;

         if (!isCheckMode)
            {
            bool createdNewVecSym = false;
            if (vecSymRef == NULL)
               {
               vecSymRef = comp->cg()->allocateLocalTemp(node->getDataType().scalarToVector()); // need to handle alignment?
               pSPMDInfo->addVectorSymRef(symRef, vecSymRef);
               createdNewVecSym = true;

               if (trace)
                  traceMsg(comp, "   created new symRef #%d for #%d\n", vecSymRef->getReferenceNumber(), symRef->getReferenceNumber());
               }

            if (trace)
               traceMsg(comp, "   using symRef #%d for #%d\n", vecSymRef->getReferenceNumber(), symRef->getReferenceNumber());

            if (createdNewVecSym && reductionHashTab->locate(symRef, id))
               {
               TR_SPMDReductionInfo* reductionInfo = (TR_SPMDReductionInfo*)reductionHashTab->getData(id);
               if (reductionInfo->reductionOp != Reduction_Invalid)
                  {
                  if (trace) traceMsg(comp, "   node: %p is a store to a reduction var\n", node);

                  //creating trees to initialize reduction vector symref
                  reductionLoopEntranceProcessing(comp, loop, symRef, vecSymRef, reductionInfo->reductionOp);

                  //creating trees to transfer reduction vector symref data back to scalar symref
                  //also performs the combining operation when converting the vector symref to a scalar one
                  reductionLoopExitProcessing(comp, loop, symRef, vecSymRef, reductionInfo->reductionOp);
                  }
               }

            node->setSymbolReference(vecSymRef);
            TR::Node::recreate(node, vectorOpCode);
            }

         return visitNodeToSIMDize(node, 0, node->getFirstChild(), pSPMDInfo, isCheckMode, loop, comp, usesInLoop, useNodesOfDefsInLoop, useDefInfo, defsInLoop, reductionHashTab, node->getSymbolReference());
         }
      }
   else if (scalarOp.isBranch() || node->getOpCodeValue()==TR::BBEnd ||
            node->getOpCodeValue()==TR::BBStart || node->getOpCodeValue()==TR::asynccheck || 
            (node->getOpCodeValue()==TR::compressedRefs && node->getFirstChild()->getOpCode().isLoad()))
      {
      //Compressed ref treetops with a load can be ignored due to the load 
      // either being present under another tree top in the loop, which
      // will be processed, or not used at all. Stores under a compressed ref
      // tree top, will need to be handled differently

      //ignore
      return true;
      }
   else
      { //unsupported
      return false;
      }

    return false;
   }

TR::Block *findLoopInvariantBlockSIMD(TR::Compilation *comp, TR_RegionStructure *loop);
TR::Block *createLoopInvariantBlockSIMD(TR::Compilation *comp, TR_RegionStructure *loop);

bool TR_SPMDKernelParallelizer::visitNodeToSIMDize(TR::Node *parent, int32_t childIndex, TR::Node *node, TR_SPMDKernelInfo *pSPMDInfo, bool isCheckMode, TR_RegionStructure *loop, TR::Compilation *comp, SharedSparseBitVector *usesInLoop, CS2::ArrayOf<TR::Node *, TR::Allocator> &useNodesOfDefsInLoop, TR_UseDefInfo *useDefInfo, SharedSparseBitVector &defsInLoop, TR_HashTab* reductionHashTab, TR::SymbolReference* storeSymRef)
   {
   if (_visitedNodes.isSet(node->getGlobalIndex()))
      return true;

   _visitedNodes.set(node->getGlobalIndex());

   bool trace = comp->trace(OMR::SPMDKernelParallelization);

   TR::SymbolReference *piv = pSPMDInfo->getInductionVariableSymRef();
   TR::ILOpCode scalarOp = node->getOpCode();
   TR::ILOpCodes vectorOpCode = TR::ILOpCode::convertScalarToVector(scalarOp.getOpCodeValue());
   int32_t pivStride = INVALID_STRIDE;

   if (trace)
      {
      traceMsg(comp,"   Visiting Node [%p] during %s mode - %s\n", node, isCheckMode ? "detection" : "transformation", scalarOp.getName());
      }

   TR_HashId id = 0;
   _loopDataType->locate(loop,id);

   if (isCheckMode && node->getSize() != ((TR::Node *)_loopDataType->getData(id))->getSize())
      {
      if (trace) traceMsg(comp,"   Node size does not match loop data type for node: %p\n", node);
      return false;
      }

   if (loop->isExprInvariant(node))
      {
      if (!isCheckMode)
         genVectorAccessForScalar(parent, childIndex, node);

      return true;
      }

   if (isCheckMode)
      (*usesInLoop)[node->getGlobalIndex()] = true;

   //If we get BadIlOP during transformation, we can either return half transformed trees or set BadIlOp. Half transformed trees are harder
   //to debug in codegen and can happen due to multiple reasons. So for easy of debugging, we go ahead and set BadIlOp.
   if (isCheckMode && vectorOpCode == TR::BadILOp)
      {
      if (trace)
         {
         traceMsg(comp,"   [%p]: Can't convert scalar OpCode %s to a vectorized instruction\n", node, scalarOp.getName());
         }
      return false;
      }

   TR_ASSERT(vectorOpCode != TR::BadILOp, "BAD IL Opcode to be assigned during transformation");


   if (isCheckMode && !comp->cg()->getSupportsOpCodeForAutoSIMD(vectorOpCode, node->getDataType()))
      {
      if (trace)
         {
         traceMsg(comp,"   [%p - %s]: vector Opcode and data type are not supported by this platform\n", node, scalarOp.getName());
         }
      return false;
      }

   TR::ILOpCode vectorOp;
   vectorOp.setOpCodeValue(vectorOpCode);

   if (scalarOp.isLoadVar())
      {
      if (isCheckMode)
         {
         if (!scalarOp.isLoadIndirect())
            {
            bool loadInductionVar = false;
            // handle expressions using the induction variable itself
            for (int32_t iv=0;iv<_pivList.NumberOfElements();iv++)
               {
               if (node->getSymbolReference()==_pivList[iv]->getSymRef())
                  {
                  loadInductionVar = true;
                  break;
                  }
               }
            if (loadInductionVar)
               {
               // confirm platform supports vectorizing PIV uses
               bool platformSupport = comp->cg()->getSupportsOpCodeForAutoSIMD(TR::vstore, node->getDataType());
               platformSupport = platformSupport && comp->cg()->getSupportsOpCodeForAutoSIMD(TR::vsetelem, node->getDataType());
               platformSupport = platformSupport && comp->cg()->getSupportsOpCodeForAutoSIMD(TR::vadd,     node->getDataType());
               platformSupport = platformSupport && comp->cg()->getSupportsOpCodeForAutoSIMD(TR::vsplats,  node->getDataType());

               if (trace && platformSupport)
		  traceMsg(comp, "   Found use of induction variable at node [%p]\n", node);

	       if (trace && !platformSupport)
                  traceMsg(comp, "   Found use of induction variable at node [%p] - platform does not support this vectorization\n", node);

               return platformSupport;
               }
            }
         }

      if (!isCheckMode)
         {
         if (scalarOp.isLoadIndirect() && !hasPIV(node, piv)) // SIMD_TODO: strictly speaking should check for stride == 0
            {
            genVectorAccessForScalar(parent, childIndex, node);
            return true;
            }

         TR::SymbolReference *symRef = node->getSymbolReference();
         TR::SymbolReference *vecSymRef = pSPMDInfo->getVectorSymRef(symRef);
         bool createdNewVecSym = false;

         if (!vecSymRef)
            {
            vecSymRef = scalarOp.isLoadIndirect() ?
                           comp->getSymRefTab()->findOrCreateArrayShadowSymbolRef(node->getDataType().scalarToVector(), NULL)
                           : vecSymRef = comp->cg()->allocateLocalTemp(node->getDataType().scalarToVector()); // need to handle alignment?
            pSPMDInfo->addVectorSymRef(symRef, vecSymRef);
            createdNewVecSym = true;

            if (trace)
                traceMsg(comp, "   created new symRef #%d for #%d\n", vecSymRef->getReferenceNumber(), symRef->getReferenceNumber());

            }

         if (trace)
             traceMsg(comp, "   using symRef #%d for #%d\n", vecSymRef->getReferenceNumber(), symRef->getReferenceNumber());

         bool loadInductionVar = false;
         if (!scalarOp.isLoadIndirect())
            {
            // handle expressions using the induction variable itself
            for (int32_t iv=0;iv<_pivList.NumberOfElements();iv++)
               {
               if (node->getSymbolReference() == _pivList[iv]->getSymRef())
                  {
                  loadInductionVar = true;
                  break;
                  }
               }
            }

         if (loadInductionVar && createdNewVecSym)
            {
            // initializing the vectorized version of the induction variable

            pSPMDInfo->setIsPIVVectorized(true);

            TR::Block* loopInvariantBlock = findLoopInvariantBlockSIMD(comp, loop);
            if (!loopInvariantBlock)
               {
               loopInvariantBlock = createLoopInvariantBlockSIMD(comp, loop);
               }

            TR::Node *splatsNode = TR::Node::create(TR::vsplats, 1, node->duplicateTree());

            TR::Node *vconstNode = TR::Node::create(TR::vsplats, 1, TR::Node::create(TR::iconst, 0, 0));

            TR::Node *vsetelem1Node = TR::Node::create(TR::vsetelem, 3);
            vsetelem1Node->setAndIncChild(0, vconstNode);
            vsetelem1Node->setAndIncChild(1, TR::Node::create(TR::iconst, 0, 1));
            vsetelem1Node->setAndIncChild(2, TR::Node::create(TR::iconst, 0, 1));

            TR::Node *vsetelem2Node = TR::Node::create(TR::vsetelem, 3);
            vsetelem2Node->setAndIncChild(0, vsetelem1Node);
            vsetelem2Node->setAndIncChild(1, TR::Node::create(TR::iconst, 0, 2));
            vsetelem2Node->setAndIncChild(2, TR::Node::create(TR::iconst, 0, 2));

            TR::Node *vsetelem3Node = TR::Node::create(TR::vsetelem, 3);
            vsetelem3Node->setAndIncChild(0, vsetelem2Node);
            vsetelem3Node->setAndIncChild(1, TR::Node::create(TR::iconst, 0, 3));
            vsetelem3Node->setAndIncChild(2, TR::Node::create(TR::iconst, 0, 3));

            TR::Node *vaddNode   = TR::Node::create(TR::vadd, 2);
            vaddNode->setAndIncChild(0, splatsNode);
            vaddNode->setAndIncChild(1, vsetelem3Node);

            TR::Node *vstoreNode = TR::Node::createWithSymRef(TR::vstore, 1, 1, vaddNode, vecSymRef);

            TR::TreeTop *insertionPoint = loopInvariantBlock->getEntry();
            TR::Node    *treetopNode = TR::Node::create(TR::treetop, 1, vstoreNode);
            TR::TreeTop *vinitTreeTop = TR::TreeTop::create(comp, treetopNode, 0, 0);
            insertionPoint->insertAfter(vinitTreeTop);

            if (trace)
               traceMsg(comp, "   Created trees to initialize vectorized PIV at node [%p]\n", vstoreNode);

            }

         if (loadInductionVar)
            {
            // the load op for the PIV is usually commoned with address calculations
            // so uncommoning it so it can be vectorized without touching address calc
            TR::Node *vloadNode = node->duplicateTree();
            node->recursivelyDecReferenceCount();
            // can visit the commoned node again, if needed.
            _visitedNodes.reset(node->getGlobalIndex());

            TR::Node::recreate(vloadNode, vectorOpCode);
            vloadNode->setSymbolReference(vecSymRef);
            parent->setAndIncChild(childIndex, vloadNode);

            if (trace)
               {
               traceMsg(comp, "   Vectorizing PIV use at node [%p]\n", vloadNode);
               traceMsg(comp, "Transforming  node [%p]  from %s to %s\n", node, scalarOp.getName(), vectorOp.getName());
               }
            }
         else
            {
            TR::Node::recreate(node, vectorOpCode);
            node->setSymbolReference(vecSymRef);
            }

         //handling initialization of reduction symRefs
         if (!scalarOp.isLoadIndirect())
            {
            if (createdNewVecSym && reductionHashTab->locate(node->getSymbolReference(), id))
               {
               TR_SPMDReductionInfo* reductionInfo = (TR_SPMDReductionInfo*)reductionHashTab->getData(id);
               if (reductionInfo->reductionOp != Reduction_Invalid)
                  {
                  if (trace) traceMsg(comp, "   node: %p is a load from a reduction var\n", node);

                  //creating trees to initialize reduction vector symref
                  reductionLoopEntranceProcessing(comp, loop, symRef, vecSymRef, reductionInfo->reductionOp);

                  //creating trees to transfer reduction vector symref data back to scalar symref
                  //also performs the combining operation when converting the vector symref to a scalar one
                  reductionLoopExitProcessing(comp, loop, symRef, vecSymRef, reductionInfo->reductionOp);
                  }
               }
            }

         return true;
         }

      if (scalarOp.isLoadIndirect())
         {
         int32_t pivStride = INVALID_STRIDE;
         bool affine = isAffineAccess(comp, node->getFirstChild(), loop, piv, pivStride);

         if (trace)
            traceMsg(comp, "   node %p affine = %d stride = %d\n", node, affine, pivStride);

         if (!affine || !(pivStride*getUnrollCount(node->getDataType()) == VECTOR_SIZE || pivStride == 0))
            {
            return false;
            }
         }
      else
         {
         if (reductionHashTab->locate(node->getSymbolReference(), id))
            {
            TR_SPMDReductionInfo* reductionInfo = (TR_SPMDReductionInfo*)reductionHashTab->getData(id);
            //reduction symRefs can only be read from a statement that also stores back to the same symRef (eg. sum = sum + a[i])
            //this code checks that this condition is being met. If not, the symRef is not a supported reduction symRef
            if (reductionInfo->reductionOp != Reduction_Invalid && node->getSymbolReference() != storeSymRef)
               {
               if (trace) traceMsg(comp, "   visitNodeToSIMDize: Load symRef does not match store symref at node: %p\n", node);
               reductionInfo->reductionOp = Reduction_Invalid;
               ListIterator<TR::Node> nodeIt(&reductionInfo->storeNodes);
               for (TR::Node *nextNode = nodeIt.getCurrent(); nextNode; nextNode=nodeIt.getNext())
                  {
                  if (trace) traceMsg(comp, "   visitNodeToSIMDize: Adding recorded invalid reduction node to defsInLoop. node: %p\n", nextNode);
                  defsInLoop[nextNode->getGlobalIndex()] = true;
                  collectUses(nextNode, comp, useDefInfo, useNodesOfDefsInLoop);
                  }
               }
            }
         else
            {
            if (trace) traceMsg(comp, "   visitNodeToSIMDize: Never before seen symRef being recorded as not a reduction at node: %p\n", node);
            TR_SPMDReductionInfo* reductionInfo = new (comp->trStackMemory()) TR_SPMDReductionInfo(comp);
            reductionInfo->reductionOp = Reduction_Invalid;
            reductionInfo->reductionSymRef = node->getSymbolReference();
            reductionHashTab->add(node->getSymbolReference(), id, reductionInfo);
            }
         }

      return true;
      }
   else
      {
      for (int32_t i = 0; i < node->getNumChildren(); i++)
         {
         if (!visitNodeToSIMDize(node, i, node->getChild(i), pSPMDInfo, isCheckMode, loop, comp, usesInLoop, useNodesOfDefsInLoop, useDefInfo, defsInLoop, reductionHashTab, storeSymRef))
            return false;
         }

      if (scalarOp.isAdd() || scalarOp.isSub() || scalarOp.isMul() || scalarOp.isDiv() || scalarOp.isRem() || scalarOp.isLeftShift() || scalarOp.isRightShift() || scalarOp.isShiftLogical() || scalarOp.isAnd() || scalarOp.isXor() || scalarOp.isOr())
         {
         if (isCheckMode)
            return true;

         if (trace)
            traceMsg(comp, "Transforming node [%p] from %s to %s\n", node, scalarOp.getName(), vectorOp.getName());

         TR::Node *firstChild = node->getFirstChild();
         TR::Node *secondChild = node->getSecondChild();

         while (firstChild->getOpCodeValue() == TR::PassThrough) firstChild = firstChild->getFirstChild();
         while (secondChild->getOpCodeValue() == TR::PassThrough) secondChild = secondChild->getFirstChild();

         TR_ASSERT(firstChild->getDataType().isVector() && firstChild->getDataType() == secondChild->getDataType()," Both children of the node to be transformed must be vector types %s %s", firstChild->getOpCode().getName(), secondChild->getOpCode().getName());
         TR::Node::recreate(node, vectorOpCode);
         return true;
         }

      if (scalarOp.isNeg() || 
          scalarOp.getOpCodeValue() == TR::l2d) 
         {
         if (isCheckMode)
            return true;

         if (trace)
            traceMsg(comp, "Transforming node [%p] from %s to %s\n", node, scalarOp.getName(), vectorOp.getName());

         TR::Node *firstChild = node->getFirstChild();

         while (firstChild->getOpCodeValue() == TR::PassThrough) firstChild = firstChild->getFirstChild();

         TR_ASSERT(firstChild->getDataType().isVector()," Child of the node to be transformed must be a vector type");
         TR::Node::recreate(node, vectorOpCode);
         return true;
         }
      }

   if (trace)
      {
      traceMsg(comp,"   [%p - %s]:  Vectorization failed due to unknown reason.\n", node, scalarOp.getName());
      }
   return false;
   }

bool TR_SPMDKernelParallelizer::autoSIMDReductionSupported(TR::Compilation *comp, TR::Node *node)
   {
   bool trace = comp->trace(OMR::SPMDKernelParallelization);

   //float and double not currently supported.
   static bool enableFPAutoSIMDReduction = feGetEnv("TR_enableFPAutoSIMDReduction") ? true : false;

   if (!enableFPAutoSIMDReduction 
       && !_fpreductionAnnotation
       && (node->getDataType() == TR::Float || node->getDataType() == TR::Double))
      {
      if (trace) traceMsg(comp, "   autoSIMDReductionSupported: float and double reduction are not supported right now. node: %p\n", node);
      return false;
      }

   //These checks are to make sure vector opcodes uses for reduction are supported
   if (!comp->cg()->getSupportsOpCodeForAutoSIMD(TR::vsplats, node->getDataType()))
      {
      if (trace) traceMsg(comp, "   autoSIMDReductionSupported: vsplats is not supported for dataType: %s\n", node->getDataType().toString());
      return false;
      }

   if (!comp->cg()->getSupportsOpCodeForAutoSIMD(TR::vstore, node->getDataType()))
      {
      if (trace) traceMsg(comp, "   autoSIMDReductionSupported: vstore is not supported for dataType: %s\n", node->getDataType().toString());
      return false;
      }

   if (!comp->cg()->getSupportsOpCodeForAutoSIMD(TR::vload, node->getDataType()))
      {
      if (trace) traceMsg(comp, "   autoSIMDReductionSupported: vload is not supported for dataType: %s\n", node->getDataType().toString());
      return false;
      }

   if (!comp->cg()->getSupportsOpCodeForAutoSIMD(TR::getvelem, node->getDataType()))
      {
      if (trace) traceMsg(comp, "   autoSIMDReductionSupported: getvelem is not supported for dataType: %s\n", node->getDataType().toString());
      return false;
      }

   //if all checks pass, return true
   return true;
   }

//isReduction is run to check that the reduction matches the reduction pattern
//-only uses the reduction symref once
//-reduction operation is support (currently only add and mul)
//-only the reduction operation is used between the store node and the reduction variable load
bool TR_SPMDKernelParallelizer::isReduction(TR::Compilation *comp, TR_RegionStructure *loop, TR::Node *node, TR_SPMDReductionInfo* reductionInfo, TR_SPMDReductionOp pathOp)
   {
   bool trace = comp->trace(OMR::SPMDKernelParallelization);

   if (reductionInfo->reductionOp == Reduction_Invalid)
      return false;

   if (loop->isExprInvariant(node))
      return false;

   if (node->getReferenceCount() != 1)
      return false;

   TR::ILOpCode opCode = node->getOpCode();

   if (opCode.isConversion() && node->getFirstChild()->getOpCode().isLoadVar()) {
      node = node->getFirstChild(); 
      opCode = node->getOpCode();
   }

   if (opCode.isLoadVar())
      {
      if (opCode.isLoadDirect() && node->getSymbolReference() == reductionInfo->reductionSymRef)
         {
         if (trace) traceMsg(comp, "   isReduction: found potential reduction symRef. Node %p\n", node);
         reductionInfo->reductionOp = pathOp;
         return true;
         }
      else
         return false;
      }
   else if (opCode.isAdd() || opCode.isMul() || opCode.isSub()) //TODO: add max, min and bitwise operations here
      {
      if (opCode.isAdd() || opCode.isSub()) //sub is a special case of add. It only works if the reduction var is on the left
         {
         switch (pathOp)
            {
            case Reduction_OpUninitialized:
               pathOp = Reduction_Add;
               break;
            case Reduction_Add:
               break;
            default:
               return false;
            }
         }
      else if (opCode.isMul())
         {
         switch (pathOp)
            {
            case Reduction_OpUninitialized:
               pathOp = Reduction_Mul;
               break;
            case Reduction_Mul:
               break;
            default:
               return false;
            }
         }
      else
         {
         return false;
         }

      TR::Node *firstChild = node->getFirstChild();
      TR::Node *secondChild = node->getSecondChild();

      while (firstChild->getOpCodeValue() == TR::PassThrough) firstChild = firstChild->getFirstChild();
      while (secondChild->getOpCodeValue() == TR::PassThrough) secondChild = secondChild->getFirstChild();

      if (isReduction(comp, loop, firstChild, reductionInfo, pathOp))
         {
         if (noReductionVar(comp, loop, secondChild, reductionInfo))
            return (reductionInfo->reductionOp != Reduction_Invalid); //if noReductionVar sets Reduction_Invalid we want to still return false here
         else
            return false;
         }
      else if (!opCode.isSub() && isReduction(comp, loop, secondChild, reductionInfo, pathOp)) //sub is a special case of add. It only works if the reduction var is on the left
         {
         if (noReductionVar(comp, loop, firstChild, reductionInfo))
            return (reductionInfo->reductionOp != Reduction_Invalid); //if noReductionVar sets Reduction_Invalid we want to still return false here
         else
            return false;
         }
      else
         return false;
      }

   //the node is not a load or a supported reduction operation
   reductionInfo->reductionOp = Reduction_Invalid;
   return false;
   }

//checks that this side of the tree does not load from the reduction symref
bool TR_SPMDKernelParallelizer::noReductionVar(TR::Compilation *comp, TR_RegionStructure *loop, TR::Node *node, TR_SPMDReductionInfo* reductionInfo)
   {
   bool trace = comp->trace(OMR::SPMDKernelParallelization);

   if (loop->isExprInvariant(node))
      return true;

   TR::ILOpCode opCode = node->getOpCode();

   if (opCode.isConversion() && node->getFirstChild()->getOpCode().isLoadVar()) {
      node = node->getFirstChild(); 
      opCode = node->getOpCode();
   }

   if (opCode.isLoadVar())
      {
      if (opCode.isLoadDirect() && node->getSymbolReference() == reductionInfo->reductionSymRef)
         {
         if (trace) traceMsg(comp, "   noReductionVar: found multiple uses of reduction symRef. Node %p\n", node);
         return false;
         }
      else
         return true;
      }
   else if (opCode.isAdd() || opCode.isSub() || opCode.isMul() || opCode.isDiv() || opCode.isRem()) //TODO: add max, min and bitwise operations here
      {
      TR::Node *firstChild = node->getFirstChild();
      TR::Node *secondChild = node->getSecondChild();

      while (firstChild->getOpCodeValue() == TR::PassThrough) firstChild = firstChild->getFirstChild();
      while (secondChild->getOpCodeValue() == TR::PassThrough) secondChild = secondChild->getFirstChild();

      if (!noReductionVar(comp, loop, firstChild, reductionInfo))
         return false;

      if (!noReductionVar(comp, loop, secondChild, reductionInfo))
         return false;

      return true;
      }

   //the node is not a load or a supported reduction operation
   reductionInfo->reductionOp = Reduction_Invalid;
   return false;
   }

//initializes the reduction vector symref with the identity
bool TR_SPMDKernelParallelizer::reductionLoopEntranceProcessing(TR::Compilation *comp, TR_RegionStructure *loop, TR::SymbolReference *symRef, TR::SymbolReference *vecSymRef, TR_SPMDReductionOp reductionOp)
   {
   bool trace = comp->trace(OMR::SPMDKernelParallelization);

   if (trace) traceMsg(comp, "   reductionLoopEntranceProcessing: loop: %d, symRef: %p, vecSymRef: %p\n", loop->getNumber(), symRef, vecSymRef);

   if (reductionOp == Reduction_OpUninitialized)
      return true; //Nothing needs to be done

   //we only know how to handle add and multiply ops right now
   if (!(reductionOp == Reduction_Add || reductionOp == Reduction_Mul))
      {
      if (trace) traceMsg(comp, "   reductionLoopEntranceProcessing: Invalid or unknown reductionOp during transformation phase.\n");
      TR_ASSERT(0, "Invalid or unknown reductionOp during transformation phase");
      return false;
      }

   // initializing the vectorized version of the reduction variable
   TR::Block* loopInvariantBlock = findLoopInvariantBlockSIMD(comp, loop);
   if (!loopInvariantBlock)
      {
      if (trace) traceMsg(comp, "   reductionLoopEntranceProcessing: Loop: %d. No loop invariant block. Creating one.\n", loop->getNumber());
      loopInvariantBlock = createLoopInvariantBlockSIMD(comp, loop);
      }

   TR::TreeTop *insertionPoint = loopInvariantBlock->getEntry();
   TR::DataType scalarDataType = symRef->getSymbol()->getDataType();
   TR::ILOpCodes splatConstType = comp->il.opCodeForConst(scalarDataType);

   //splat the identity for the initial value
   TR::Node *splatsNode = TR::Node::create(insertionPoint->getNode(), TR::vsplats, 1);
   TR::Node *constNode = TR::Node::create(insertionPoint->getNode(), splatConstType, 0);
   uint8_t identity = 0;

   switch (reductionOp)
      {
      case Reduction_Add: //identity is 0
         identity = 0;
         break;
      case Reduction_Mul: //identity is 1
         identity = 1;
         break;
      default:
         if (trace) traceMsg(comp, "   reductionLoopEntranceProcessing: Invalid or unknown reductionOp during transformation phase (2).\n");
         TR_ASSERT(0, "Invalid or unknown reductionOp during transformation phase (2)");
         return false;
      }

   switch (scalarDataType)
      {
      case TR::Int8:
         constNode->setByte(identity);
         break;
      case TR::Int16:
         constNode->setShortInt(identity);
         break;
      case TR::Int32:
         constNode->setInt(identity);
         break;
      case TR::Int64:
         constNode->setLongInt(identity);
         break;
      case TR::Float:
         constNode->setFloat(identity);
         break;
      case TR::Double:
         constNode->setDouble(identity);
         break;
      default:
         if (trace) traceMsg(comp, "   reductionLoopEntranceProcessing: Unknown vector data type during transformation phase.\n");
         TR_ASSERT(0, "Unknown vector data type during transformation phase.");
         return false;
         break;
      }

   splatsNode->setAndIncChild(0, constNode);

   TR::Node *vstoreNode = TR::Node::create(insertionPoint->getNode(), TR::vstore, 1);
   vstoreNode->setAndIncChild(0, splatsNode);
   vstoreNode->setSymbolReference(vecSymRef);

   TR::Node *treetopNode = TR::Node::create(TR::treetop, 1, vstoreNode);
   TR::TreeTop *vIdentityTreeTop = TR::TreeTop::create(comp, treetopNode, 0, 0);
   insertionPoint->insertAfter(vIdentityTreeTop);

   if (trace) traceMsg(comp, "   reductionLoopEntranceProcessing: Loop: %d. Created reduction identity store node: %p\n", loop->getNumber(), vstoreNode);

   return true;
   }

//performs the final reduction operation on each vector element to return the final computed value in a scalar symref
bool TR_SPMDKernelParallelizer::reductionLoopExitProcessing(TR::Compilation *comp, TR_RegionStructure *loop, TR::SymbolReference *symRef, TR::SymbolReference *vecSymRef, TR_SPMDReductionOp reductionOp)
   {
   bool trace = comp->trace(OMR::SPMDKernelParallelization);

   if (trace) traceMsg(comp, "   reductionLoopExitProcessing: loop: %d, symRef: %p, vecSymRef: %p\n", loop->getNumber(), symRef, vecSymRef);

   if (reductionOp == Reduction_OpUninitialized)
      return true; //Nothing needs to be done

   //we only know how to handle add and multiply ops right now
   if (!(reductionOp == Reduction_Add || reductionOp == Reduction_Mul))
      {
      if (trace) traceMsg(comp, "   reductionLoopExitProcessing: Invalid or unknown reductionOp during transformation phase.\n");
      TR_ASSERT(0, "Invalid or unknown reductionOp during transformation phase");
      return false;
      }

   TR::DataType scalarDataType = symRef->getSymbol()->getDataType();
   TR::ILOpCodes scalarReductionOp = TR::BadILOp;
   switch (reductionOp)
      {
      case Reduction_Add:
         scalarReductionOp = TR::ILOpCode::addOpCode(scalarDataType, false);
         break;
      case Reduction_Mul:
         scalarReductionOp = TR::ILOpCode::multiplyOpCode(scalarDataType);
         break;
      default:
         if (trace) traceMsg(comp, "   reductionLoopExitProcessing: Invalid or unknown reductionOp during transformation phase (2).\n");
         TR_ASSERT(0, "Invalid or unknown reductionOp during transformation phase (2)");
         return false;
      }

   TR::ILOpCodes loadOp = comp->il.opCodeForDirectLoad(scalarDataType);
   int numelements = 0;
   switch (scalarDataType)
      {
      case TR::Int8:
         numelements = 16;
         break;
      case TR::Int16:
         numelements = 8;
         break;
      case TR::Int32:
      case TR::Float:
         numelements = 4;
         break;
      case TR::Int64:
      case TR::Double:
         numelements = 2;
         break;
      default:
         if (trace) traceMsg(comp, "   reductionLoopExitProcessing: Unknown vector data type during transformation phase.\n");
         TR_ASSERT(0, "Unknown vector data type during transformation phase.");
         return false;
         break;
      }

   List<TR::Block> exitBlocks(comp->trMemory());
   List<TR::Block> blocksInRegion(comp->trMemory());
   List<TR::CFGEdge> exitEdges(comp->trMemory());

   loop->collectExitBlocks(&exitBlocks);
   loop->getBlocks(&blocksInRegion);

   TR::CFG *cfg = comp->getFlowGraph();
   TR_BitVector *blocksInsideLoop = new (comp->trStackMemory()) TR_BitVector(cfg->getNextNodeNumber(), comp->trMemory(), stackAlloc);

   TR::Block *blockInLoop = 0;
   ListIterator<TR::Block> si(&blocksInRegion);
   for (blockInLoop = si.getCurrent(); blockInLoop; blockInLoop = si.getNext())
      blocksInsideLoop->set(blockInLoop->getNumber());

   TR::Block *exitBlock = 0;
   si.set(&exitBlocks);
   for (exitBlock = si.getCurrent(); exitBlock; exitBlock = si.getNext())
      {
      for (auto edge = exitBlock->getSuccessors().begin(); edge != exitBlock->getSuccessors().end(); ++edge)
         {
         TR::Block* next = toBlock((*edge)->getTo());

         if (!blocksInsideLoop->get(next->getNumber()))
            {
            exitEdges.add(*edge);
            }
         }
      }

   TR::CFGEdge *edge = 0;
   ListIterator<TR::CFGEdge> ei(&exitEdges);
   for (edge = ei.getCurrent(); edge; edge = ei.getNext())
      {
      TR::Block* exitBlock = toBlock(edge->getFrom());
      TR::Block* next = toBlock(edge->getTo());

      //create a new block for each edge that leaves each exit block that does not reenter the loop
      TR::Block* reductionBlock = exitBlock->splitEdge(exitBlock, next, comp);
      if (trace) traceMsg(comp, "   reductionLoopExitProcessing: Created block: %d\n", reductionBlock->getNumber());

      TR::TreeTop *insertionPoint = reductionBlock->getEntry();

      //read each element from the vector and perform the reduction operation to combine them
      //only add and mult are supported right now
      TR::Node *loadVectorNode = TR::Node::create(insertionPoint->getNode(), TR::vload, 0);
      loadVectorNode->setSymbolReference(vecSymRef);

      TR::Node* topNode = TR::Node::createWithSymRef(insertionPoint->getNode(), loadOp, 0, symRef);
      for (int i = 0; i < numelements; i++)
         {
         TR::Node *getvelemNode = TR::Node::create(insertionPoint->getNode(), TR::getvelem, 2);
         getvelemNode->setAndIncChild(0, loadVectorNode);
         getvelemNode->setAndIncChild(1, TR::Node::create(insertionPoint->getNode(), TR::iconst, 0, i));

         TR::Node* opNode = TR::Node::create(insertionPoint->getNode(), scalarReductionOp, 2);
         opNode->setAndIncChild(0, getvelemNode);
         opNode->setAndIncChild(1, topNode);

         topNode = opNode;
         }

      TR::Node *treetopNode = TR::Node::create(TR::treetop, 1, topNode);
      TR::TreeTop *reductionTreeTop = TR::TreeTop::create(comp, treetopNode, 0, 0);
      insertionPoint->insertAfter(reductionTreeTop);
      TR::DebugCounter::prependDebugCounter(comp, "auto-SIMD-reduction-end", reductionTreeTop);

      if (trace) traceMsg(comp, "   reductionLoopExitProcessing: Created tree: %p\n", treetopNode);

      //store the final value back in the original scalar symref
      TR::TreeTop::create(comp, reductionTreeTop, TR::Node::createStore(symRef, topNode));
      }

   return true;
   }

bool TR_SPMDKernelParallelizer::processSPMDKernelLoopForSIMDize(TR::Compilation *comp, TR::Optimizer *optimizer, TR_RegionStructure *loop,TR_PrimaryInductionVariable *piv, TR_HashTab* reductionHashTab, int32_t peelCount, TR::Block *invariantBlock)
   {

   TR_HashId id = 0;
   int32_t vectorSize, unrollCount;

   if(_loopDataType->locate(loop,id))
      vectorSize = getUnrollCount(((TR::Node *)_loopDataType->getData(id))->getDataType());
   else
      vectorSize = 8;

   unrollCount = vectorSize * 4;

   TR_LoopUnroller unroller(comp, optimizer, loop, piv, TR_LoopUnroller::SPMDKernel, unrollCount-1, peelCount, invariantBlock, vectorSize);

   TR_StructureSubGraphNode * branchNode =  unroller._branchNode;
   TR::Block *branchBlock = unroller._piv->getBranchBlock();
   TR::Node *node = branchBlock->getLastRealTreeTop()->getNode();

   if (!performTransformation(comp, "%s Simdizing loop %d  branch block = %d inc = %d piv = %d\n", OPT_SIMD_DETAILS, loop->getNumber(), branchBlock->getNumber(), unroller._piv->getDeltaOnBackEdge(), unroller._piv->getSymRef()->getReferenceNumber())) //method->nameChars())
      return false;


   TR_SPMDKernelInfo *pSPMDInfo = new (comp->trStackMemory()) TR_SPMDKernelInfo(comp, unroller._piv);

   int32_t iters = unroller._piv->getIterationCount();
   unroller._spillLoopRequired = true;

   if (iters > 0 && !(iters % unrollCount))
      unroller._spillLoopRequired = false;

   traceMsg(comp, "   Checking Loop ==== (iter = %d, unroll = %d, vectorSize = %d) and unroller._spillLoopRequired %d \n", iters, unrollCount, vectorSize, unroller._spillLoopRequired);

   TR::Block *origFallthruBlock = branchBlock->getExit()->getNextTreeTop()->getNode()->getBlock();

   // original fall thru block

   // need scalar loop for handling residual iterations
   // piggybacking on loop unroller implementation

   //void *stackMark = trMemory()->markStack();
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   unroller.unroll(loop, branchNode);

#if 0
   unroller._optimizer->setEnableOptimization(deadTreesElimination, true, NULL);
   unroller._optimizer->setEnableOptimization(trivialDeadTreeRemoval, true, NULL);
   unroller._optimizer->setEnableOptimization(basicBlockOrdering, true, NULL);
   unroller._optimizer->setEnableOptimization(inductionVariableAnalysis, true, NULL);
#endif

   //trMemory()->releaseStack(stackMark);

   TR_ScratchList<TR::Block> blocksInLoop(trMemory());
   loop->getBlocks(&blocksInLoop);
   ListIterator<TR::Block> blocksIt1(&blocksInLoop);


   // SIMD_TODO: improve this code

   for (TR::Block *nextBlock = blocksIt1.getCurrent(); nextBlock; nextBlock=blocksIt1.getNext())
      {
      for (TR::TreeTop *tt = nextBlock->getEntry() ; tt != nextBlock->getExit() ; tt = tt->getNextTreeTop())
         {
         // identify operation for primary induction variable
         TR::Node *storeNode = tt->getNode();

         if (storeNode->getOpCodeValue() == TR::istore && storeNode->getSymbolReference() == unroller._piv->getSymRef())
            {
            // increment of PIV
            traceMsg(comp, "Reducing the number of iterations of the loop %d at storeNode [%p] by vector length %d \n",loop->getNumber(), storeNode, unrollCount);
            TR::Node *constNode = storeNode->getFirstChild()->getSecondChild()->duplicateTree();
            constNode->setInt(constNode->getInt() * vectorSize);
            storeNode->getFirstChild()->getSecondChild()->recursivelyDecReferenceCount();
            //can visit the commoned node again
            _visitedNodes.reset(storeNode->getFirstChild()->getSecondChild()->getGlobalIndex());
            storeNode->getFirstChild()->setAndIncChild(1,constNode);

            }
         }
      }

   ListIterator<TR::Block> blocksIt(&blocksInLoop);

   // TODO: can pass NULL on the second pass for each of these
   CS2::ArrayOf<TR::Node *, TR::Allocator> useNodesOfDefsInLoop(comp->allocator(), NULL);
   SharedSparseBitVector defsInLoop(comp->allocator());

   _visitedNodes.empty();
   loop->resetInvariance();

   for (TR::Block *nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      {
      for (TR::TreeTop *tt = nextBlock->getEntry() ; tt != nextBlock->getExit() ; tt = tt->getNextTreeTop())
         {
         visitTreeTopToSIMDize(tt, pSPMDInfo, false, loop, useNodesOfDefsInLoop, comp, NULL, defsInLoop, NULL, reductionHashTab);
         }
      }

   TR::DebugCounter::prependDebugCounter(comp, "auto-SIMD", loop->getEntryBlock()->getFirstRealTreeTop());

   loop->resetInvariance();
   return true;

}

// SIMD_TODO's:
//  1) speedup search for symrefs in pSPMDInfo (see the header)


// GPU code

int32_t getArrayElementSizeFromSignature(char* sig, int32_t sigLength)
   {
   TR_ASSERT(sig[0] == '[', "should be array signature");

   if (sigLength != 2) return -1;

   switch (sig[1])
      {
      case 'Z': return 1;
      case 'B': return 1;
      case 'C': return 2;
      case 'S': return 2;
      case 'I': return 4;
      case 'J': return 8;
      case 'F': return 4;
      case 'D': return 8;
      }
   return -1;
   }


int32_t getArrayElementSize(TR::Compilation *comp, TR::SymbolReference *symRef)
   {
   int32_t signatureLength = 0;

   char *signature = symRef->getOwningMethod(comp)->classSignatureOfFieldOrStatic(symRef->getCPIndex(), signatureLength);

   if (signature && signature[0] == '[')
      {
      traceMsg(comp, "signature %.*s\n", signatureLength, signature);
      return getArrayElementSizeFromSignature(signature, signatureLength);
      }

   return -1;
   }

int32_t TR_SPMDKernelParallelizer::findArrayElementSize(TR::Node *node)
   {
   TR_UseDefInfo *useDefInfo = optimizer()->getUseDefInfo();
   if (!useDefInfo)
      return -1;

   uint16_t useIndex = node->getUseDefIndex();
   if (!useIndex || !useDefInfo->isUseIndex(useIndex))
      return -1;

   TR_UseDefInfo::BitVector defs(comp()->allocator());
   useDefInfo->getUseDef(defs, useIndex);

   if (defs.PopulationCount() > 1)
       traceMsg(comp(), "More than one def for node %p\n", node);

   if (!defs.IsZero() && (defs.PopulationCount() == 1))
      {
      TR_UseDefInfo::BitVector::Cursor cursor(defs);
      for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
         {
         int32_t defIndex = cursor;
         if (defIndex < useDefInfo->getFirstRealDefIndex())
            return -1;

         TR::Node *defNode = useDefInfo->getNode(defIndex);

         // GPU_TODO: handle newarray
         if (!defNode->getOpCode().isStoreDirect())
            return -1;

         traceMsg(comp(), "found def node %p\n", defNode);

         if (defNode->getFirstChild()->getOpCode().isLoadIndirect() ||
               (defNode->getFirstChild()->getOpCode().isLoad() &&
                defNode->getFirstChild()->getSymbolReference()->getSymbol()->isStatic()))
            {
            return getArrayElementSize(comp(), defNode->getFirstChild()->getSymbolReference());
            }
         }
      }

   return -1;
   }


void TR_SPMDKernelParallelizer::convertIntoParm(TR::Node *node, int32_t elementSize, ListAppender<TR::ParameterSymbol> &parms)
   {
   TR::SymbolReference *hostSymRef = node->getSymbolReference();

   if (comp()->cg()->_gpuSymbolMap[hostSymRef->getReferenceNumber()]._parmSlot == -1)
      {
      gpuMapElement element(node->duplicateTree(), hostSymRef, elementSize, _parmSlot);
      comp()->cg()->_gpuSymbolMap[hostSymRef->getReferenceNumber()] = element;
      _parmSlot++;
      }
   }

void TR_SPMDKernelParallelizer::reportRejected(const char *msg1, const char *msg2,  int32_t lineNumber, TR::Node *node)
   {
   traceMsg(comp(), msg1, node);

   if (msg2)
      {
      traceMsg(comp(), msg2, comp()->signature(), lineNumber, comp()->getLineNumber(node));
      traceMsg(comp(), "\n");
      }

   if (_verboseTrace > 0 && msg2)
      TR_VerboseLog::writeLine(TR_Vlog_GPU, msg2, comp()->signature(), lineNumber, comp()->getLineNumber(node));

   }

bool TR_SPMDKernelParallelizer::visitNodeToMapSymbols(TR::Node *node,
                                                      ListAppender<TR::ParameterSymbol> &parms,
                                                      ListAppender<TR::AutomaticSymbol> &autos,
                                                      TR_RegionStructure *loop,
                                                      TR_PrimaryInductionVariable *piv,
                                                      int32_t lineNumber,
                                                      int32_t visitCount)
   {

   bool is_parent_nullcheck = false;

   if (node->getOpCodeValue() == TR::compressedRefs)
      {
      if (loop->isExprInvariant(node))
         return true; // ignore for now
      node = node->getFirstChild();
      }

   if (node->getOpCodeValue() == TR::treetop)
       node = node->getFirstChild();

   if (node->getVisitCount() == visitCount) return true;

   node->setVisitCount(visitCount);

   TR::ILOpCode opcode = node->getOpCode();

   if (opcode.isNullCheck())
      {
      node = node->getFirstChild();
      is_parent_nullcheck = true;
      }

   if (opcode.isCall() &&
       node->getSymbolReference()->isUnresolved())
      {
      reportRejected("Stop processing since node %p is unresolved call\n",
                     "Rejected forEach in %s at line %d: contains unresolved call",
                     lineNumber, node);
      return false;
      }

   if (opcode.isLoadVarOrStore())
      {
      TR::SymbolReference *hostSymRef = node->getSymbolReference();

      if (hostSymRef->getSymbol()->isAuto() &&
          hostSymRef != piv->getSymRef())
         {
         if (loop->isExprInvariant(node))
            {
            int32_t elementSize = -1;
            if (node->getType().isAddress())
               {
               elementSize = findArrayElementSize(node);
               if (elementSize < 0)
                  {
                  reportRejected("Stop processing since auto symref node %p is not a supported array\n",
                                 "Rejected forEach in %s at line %d: could not transform",
                                 lineNumber, node);
                  return false;
                  }
               }

            convertIntoParm(node, elementSize, parms);
            return true;
            }
         else
            {
            if (!comp()->cg()->_gpuSymbolMap[hostSymRef->getReferenceNumber()]._hostSymRef)
               {
               traceMsg(comp(), "Adding node %p into auto list\n", node);

               autos.add((TR::AutomaticSymbol *)hostSymRef->getSymbol());

               gpuMapElement element(node, hostSymRef, -1, -1);
               comp()->cg()->_gpuSymbolMap[hostSymRef->getReferenceNumber()] = element;
               }
            }
         }
      else if (hostSymRef == piv->getSymRef())
         {
         // allow
         }
      else if (hostSymRef->getSymbol()->isShadow())
         {
         if (loop->isExprInvariant(node))
            {
            TR_ASSERT(opcode.isLoadIndirect(), "should be indirect load");
            int32_t elementSize = -1;

            if (node->getType().isAddress())
               {
               elementSize = getArrayElementSize(comp(), hostSymRef);
               if (elementSize < 0)
                  {
                  reportRejected("Stop processing since shadow symref node %p is not a supported array\n",
                                 "Rejected forEach in %s at line %d: could not transform",
                                 lineNumber, node);
                  return false;
                  }
               }

         if (!is_parent_nullcheck)
               {
               convertIntoParm(node, elementSize, parms);
               return true; // skip children
               }
            }
         else if (node->getType().isAddress())
            {
            reportRejected("Stop processing since node %p is not an invariant address\n",
                           "Rejected forEach in %s at line %d: could not transform",
                           lineNumber, node);
            return false;
            }
         }
      else
         {
         reportRejected("Stop processing since node %p has unsupported symbol reference\n",
                        "Rejected forEach in %s at line %d: could not transform",
                        lineNumber, node);
         return false;
         }
      }

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      if (!visitNodeToMapSymbols(node->getChild(i), parms, autos, loop, piv, lineNumber, visitCount))
         return false;
      }

   return true;
   }



bool TR_SPMDKernelParallelizer::visitNodeToDetectArrayAccesses(TR::Node *node,
                                                               TR_RegionStructure *loop,
                                                               TR_PrimaryInductionVariable *piv, int32_t visitCount,
                                                               int32_t lineNumber,
                                                               bool &arrayNotFound, int trace,
                                                               bool postDominates)
   {
   if (node->getVisitCount() == visitCount) return true;

   node->setVisitCount(visitCount);

   TR::ILOpCode opcode = node->getOpCode();

   static bool disableDataTransferElimination = (feGetEnv("TR_disableGPUDataTransferElimination") != NULL);
   if ((opcode.isLoadVarOrStore() && opcode.isIndirect() &&
       node->getSymbolReference()->getSymbol()->isArrayShadowSymbol()) ||
       node->getOpCodeValue() == TR::arraylength ||
       node->getOpCodeValue() == TR::arraycopy)
      {
      int numberAddrChildren = node->getOpCodeValue() == TR::arraycopy ? 2 : 1;
      int childrenNodeOffset;

      //Some forms of array copy have five children. First two nodes are used for write barriers which we don't need
      if (node->getOpCodeValue() == TR::arraycopy && node->getNumChildren() == 5)
         childrenNodeOffset = 2;
      else
         childrenNodeOffset = 0;

      for (int childNum=childrenNodeOffset; childNum < numberAddrChildren+childrenNodeOffset; ++childNum)
         {
         bool foundArray = false;
         TR::Node *addrNode = node->getChild(childNum);

         if (addrNode->getOpCodeValue() == TR::aiadd || addrNode->getOpCodeValue() == TR::aladd)
            {
            addrNode = addrNode->getFirstChild();
            }

         if ((addrNode->getOpCodeValue() == TR::aload) || (addrNode->getOpCodeValue() == TR::aloadi))
            {
            TR::SymbolReference *symRef = addrNode->getSymbolReference();
            int32_t symRefIndex = symRef->getReferenceNumber();

            CS2::ArrayOf<gpuMapElement, TR::Allocator> &gpuSymbolMap = comp()->cg()->_gpuSymbolMap;

            int32_t nc = symRefIndex;

            TR::SymbolReference *hostSymRef = gpuSymbolMap[nc]._hostSymRef;
            int32_t parmSlot = gpuSymbolMap[nc]._parmSlot;

            if (!hostSymRef || parmSlot == -1)
               {
               TR::Node *tempNode = gpuSymbolMap[nc]._node;

               if (tempNode && (tempNode->getOpCodeValue() == TR::astore) && (tempNode->getFirstChild()->getOpCodeValue() == TR::aloadi))
                  {
                  TR::Node *parmNode = tempNode->getFirstChild();
                  nc = parmNode->getSymbolReference()->getReferenceNumber();

                  hostSymRef = gpuSymbolMap[nc]._hostSymRef;
                  parmSlot = gpuSymbolMap[nc]._parmSlot;
                  }
               }

            if (hostSymRef && parmSlot != -1)
               {
               foundArray = true;

               if (opcode.isLoadVar() || opcode.getOpCodeValue() == TR::arraylength ||
                   (opcode.getOpCodeValue() == TR::arraycopy && childNum == childrenNodeOffset))
                  {
                  traceMsg(comp(), "Node[%p]: addrNode[%p], #%d, READ\n", node, addrNode, symRefIndex);
                  gpuSymbolMap[nc]._accessKind |= TR::CodeGenerator::ReadAccess;

                  if (!disableDataTransferElimination)
                     {
                     if (gpuSymbolMap[nc]._rhsAddrExpr == NULL &&
                         gpuSymbolMap[nc]._rhsAddrExpr != INVALID_ADDR)  // single read from that array
                        {
                        TR::Node *addrExpr = node->getChild(childNum);

                        //_pivList[0] = piv;

                        int32_t pivStride = INVALID_STRIDE;
                        bool affine = isAffineAccess(comp(), addrExpr, loop, piv->getSymRef(), pivStride);
                        traceMsg(comp(), "RHS node %p has stride %d with regards to #%d, isAffine=%s\n", addrExpr, pivStride, piv->getSymRef()->getReferenceNumber(), affine ? "T":"F");
                        traceMsg(comp(), "gpuSymbolMap[%d]._elementSize=%d\n", nc, gpuSymbolMap[nc]._elementSize);

                        //TODO: currently excludes some cases that would work. Need to expand in the future.
                        if ((pivStride != INVALID_STRIDE) && (pivStride > 0))
                           {
                           gpuSymbolMap[nc]._rhsAddrExpr = addrExpr;

                           if (trace > 1)
                              TR_VerboseLog::writeLine(TR_Vlog_GPU, "Detected affine load in %s at line %d",
                                                                    comp()->signature(), comp()->getLineNumber(node));

                           traceMsg(comp(), "Detected affine load %p in RHS for gpuSymbolMap[%d]\n", addrExpr, nc);
                           }
                        else
                           {
                           gpuSymbolMap[nc]._rhsAddrExpr = INVALID_ADDR;
                           }
                        }
                     else
                        {
                        gpuSymbolMap[nc]._rhsAddrExpr = INVALID_ADDR;
                        }
                     }
                  }
               else
                  {
                  traceMsg(comp(), "Node[%p]: addrNode[%p], #%d, WRITE\n", node, addrNode, symRefIndex);
                  gpuSymbolMap[nc]._accessKind |= TR::CodeGenerator::WriteAccess;

                  if (!disableDataTransferElimination || !comp()->getOptions()->getEnableGPU(TR_EnableSafeMT))
                     {
                     if (gpuSymbolMap[nc]._lhsAddrExpr == NULL &&
                         gpuSymbolMap[nc]._lhsAddrExpr != INVALID_ADDR &&
                         postDominates)  // single unconditional write to that array
                        {
                        TR::Node *addrExpr = node->getChild(childNum);

                        //_pivList[0] = piv;

                        int32_t pivStride = INVALID_STRIDE;
                        bool affine = isAffineAccess(comp(), addrExpr, loop, piv->getSymRef(), pivStride);
                        traceMsg(comp(), "LHS node %p has stride %d with regards to #%d, isAffine=%s\n", addrExpr, pivStride, piv->getSymRef()->getReferenceNumber(), affine ? "T":"F");
                        traceMsg(comp(), "gpuSymbolMap[%d]._elementSize=%d\n", nc, gpuSymbolMap[nc]._elementSize);

                        if (pivStride == gpuSymbolMap[nc]._elementSize)
                           {
                           if (comp()->getOptions()->getEnableGPU(TR_EnableSafeMT))
                              {
                              if (trace > 1)
                                 TR_VerboseLog::writeLine(TR_Vlog_GPU, "Detected contiguous store in %s at line %d",
                                                                        comp()->signature(), comp()->getLineNumber(node));
                              }
                           gpuSymbolMap[nc]._lhsAddrExpr = addrExpr;

                           traceMsg(comp(), "Detected contiguous store %p in LHS for gpuSymbolMap[%d]\n", addrExpr, nc);
                           }
                        else
                           {
                           gpuSymbolMap[nc]._lhsAddrExpr = INVALID_ADDR;
                           if (!comp()->getOptions()->getEnableGPU(TR_EnableSafeMT))
                              {
                              if (trace > 0)
                                 TR_VerboseLog::writeLine(TR_Vlog_GPU, "Rejected forEach in %s at line %d: non-contiguous store to array at line %d", comp()->signature(), lineNumber, comp()->getLineNumber(node));
                              return false;
                              }
                           }
                        }
                     else
                        {
                        gpuSymbolMap[nc]._lhsAddrExpr = INVALID_ADDR;

                        if (!comp()->getOptions()->getEnableGPU(TR_EnableSafeMT))
                           {
                           if (trace > 0)
                              TR_VerboseLog::writeLine(TR_Vlog_GPU, "Rejected forEach in %s at line %d: multiple or conditional stores to array at line %d", comp()->signature(), lineNumber, comp()->getLineNumber(node));

                           return false;
                           }
                        }
                     }
                  }
               }
            }

         if (!foundArray)
            {
            arrayNotFound = true;

            if (opcode.isStore())
               {
               if (trace > 0)
                  TR_VerboseLog::writeLine(TR_Vlog_GPU, "Rejected forEach in %s at line %d: ambiguous store to array at line %d", comp()->signature(), lineNumber, comp()->getLineNumber(node));
               return false;  // we don't know which part of array is written to
               }
            }
         }
      }

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      if (!visitNodeToDetectArrayAccesses(node->getChild(i), loop, piv, visitCount, lineNumber, arrayNotFound, trace, postDominates))
         return false;
      }

   return true;
   }

TR::Block *findLoopInvariantBlockSIMD(TR::Compilation *comp, TR_RegionStructure *loop)
   {
   TR::Block * loopEntryBlock = loop->getEntryBlock();
   for (auto predecessor = loopEntryBlock->getPredecessors().begin(); predecessor != loopEntryBlock->getPredecessors().end(); ++predecessor)
      {
        TR::Block * predBlock = toBlock((*predecessor)->getFrom());
        if(predBlock == comp->getFlowGraph()->getStart()->asBlock())
            return NULL;
      }

   TR_RegionStructure *parentStructure = loop->getParent()->asRegion();
   if (parentStructure->getNumber() == loop->getNumber())
      {
      parentStructure = parentStructure->getParent()->asRegion();
      }

   TR_StructureSubGraphNode *subNode = 0;
   TR_RegionStructure::Cursor si(*parentStructure);
   for (subNode = si.getCurrent(); subNode != 0; subNode = si.getNext())
      {
      if (subNode->getNumber() == loop->getNumber())
         break;
      }

   if (subNode->getPredecessors().size() == 1)
      {
      TR_StructureSubGraphNode *loopInvariantNode = toStructureSubGraphNode(subNode->getPredecessors().front()->getFrom());
      if (loopInvariantNode->getStructure()->asBlock())
          return loopInvariantNode->getStructure()->asBlock()->getBlock();
      }

   return NULL;
   }

TR::Block *createLoopInvariantBlockSIMD(TR::Compilation *comp, TR_RegionStructure *loop)
   {
   if (loop->getEntryBlock() == comp->getStartBlock())
      return NULL;

   TR_ScratchList<TR::Block> blocksInRegion(comp->trMemory());
   loop->getBlocks(&blocksInRegion);
   TR_RegionStructure *parentStructure = loop->getParent()->asRegion();

   TR::Block *entryBlock = loop->getEntryBlock();
   int32_t sumPredFreq = 0;
   for (auto pred = entryBlock->getPredecessors().begin(); pred != entryBlock->getPredecessors().end(); ++pred)
   {
   TR::Block *predBlock = toBlock((*pred)->getFrom());
   if (!blocksInRegion.find(predBlock))
      sumPredFreq = sumPredFreq + (*pred)->getFrequency();
   }

   TR::Block *newBlock = TR::Block::createEmptyBlock(entryBlock->getEntry()->getNode(), comp, sumPredFreq < 0 ? 0 : sumPredFreq, entryBlock);
   bool insertAsFallThrough = false;
   TR::CFG * cfg = comp->getFlowGraph();
   cfg->addNode(newBlock, parentStructure);
   TR::CFGEdge *entryEdge = cfg->addEdge(newBlock, entryBlock);
   entryEdge->setFrequency(sumPredFreq);
   TR::TreeTop *lastTreeTop = comp->getMethodSymbol()->getLastTreeTop();

   for (auto pred = entryBlock->getPredecessors().begin(); pred != entryBlock->getPredecessors().end();)
      {
      TR::CFGEdge* curPred = *(pred++);
      TR::Block *predBlock = toBlock(curPred->getFrom());
      if ((predBlock != newBlock) &&
          !blocksInRegion.find(predBlock))
         {
         predBlock->getLastRealTreeTop()->adjustBranchOrSwitchTreeTop(comp, entryBlock->getEntry(), newBlock->getEntry());
         if (predBlock->getNextBlock() == entryBlock)
           {
           insertAsFallThrough = true;
           }

         TR::CFGEdge *newEdge = cfg->addEdge(predBlock, newBlock);
         newEdge->setFrequency(curPred->getFrequency());
         cfg->removeEdge(predBlock, entryBlock);
         }
      }

   TR::TreeTop *gotoTreeTop = TR::TreeTop::create(comp, TR::Node::create(entryBlock->getEntry()->getNode(), TR::Goto, 0, entryBlock->getEntry()));
   newBlock->append(gotoTreeTop);

   TR::TreeTop *entryTree = entryBlock->getEntry();
   if (insertAsFallThrough)
      {
      TR::TreeTop *prevTree = entryTree->getPrevTreeTop();
      prevTree->join(newBlock->getEntry());
      newBlock->getExit()->join(entryTree);
      }
   else
      {
      lastTreeTop->join(newBlock->getEntry());
      lastTreeTop = newBlock->getExit();
      }

   return newBlock;
   }

TR::Block *findLoopInvariantBlock(TR::Compilation *comp, TR_RegionStructure *loop)
   {
   TR::Block * loopEntryBlock = loop->getEntryBlock();
   for (auto predecessor = loopEntryBlock->getPredecessors().begin(); predecessor != loopEntryBlock->getPredecessors().end(); ++predecessor)
      {
        TR::Block * predBlock = toBlock((*predecessor)->getFrom());
        if(predBlock == comp->getFlowGraph()->getStart()->asBlock())
            return NULL;
      }

   TR_RegionStructure *parentStructure = loop->getParent()->asRegion();
   if (parentStructure->getNumber() == loop->getNumber())
      {
      parentStructure = parentStructure->getParent()->asRegion();
      }

   TR_StructureSubGraphNode *subNode = 0;
   TR_RegionStructure::Cursor si(*parentStructure);
   for (subNode = si.getCurrent(); subNode != 0; subNode = si.getNext())
      {
      if (subNode->getNumber() == loop->getNumber())
         break;
      }

   if (subNode->getPredecessors().size() == 1)
      {
      TR_StructureSubGraphNode *loopInvariantNode = toStructureSubGraphNode(subNode->getPredecessors().front()->getFrom());
      if (loopInvariantNode->getStructure()->asBlock() &&
          loopInvariantNode->getStructure()->asBlock()->isLoopInvariantBlock())
          return loopInvariantNode->getStructure()->asBlock()->getBlock();
      }

   return NULL;
   }

static TR::Block *insertBlock(TR::Compilation *comp, TR::CFG *cfg, TR::Block *prevBlock, TR::Block *nextBlock)
   {
   TR::Block *newBlock = TR::Block::createEmptyBlock(prevBlock->getEntry()->getNode(), comp, prevBlock->getFrequency(), prevBlock);
   prevBlock->getExit()->join(newBlock->getEntry());
   newBlock->getExit()->join(nextBlock->getEntry());
   cfg->addNode(newBlock, prevBlock->getParentStructureIfExists(cfg));
   cfg->addEdge(prevBlock, newBlock);
   cfg->addEdge(newBlock, nextBlock);
   cfg->removeEdge(prevBlock, nextBlock);

   return newBlock;
   }

// generates code for populating the GPU parms variable
// this variable contains GPU device pointers for kernel input parameters
void TR_SPMDKernelParallelizer::generateGPUParmsBlock(TR::SymbolReference *allocSymRef, TR::Block *populateParmsBlock, TR::Node *firstNode)
   {
   CS2::ArrayOf<gpuMapElement, TR::Allocator> &gpuSymbolMap = comp()->cg()->_gpuSymbolMap;
   CS2::ArrayOf<gpuMapElement, TR::Allocator>::Cursor nc(gpuSymbolMap);

   TR::SymbolReference *arrayShadowSymRef = comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::Int64, NULL);
   TR::ILOpCodes addressLoadOpCode = TR::lload;

   for (nc.SetToFirst(); nc.Valid(); nc.SetToNext())
      {
      TR::SymbolReference *hostSymRef = gpuSymbolMap[nc]._hostSymRefTemp;
      TR::SymbolReference *devSymRef = gpuSymbolMap[nc]._devSymRef;
      int32_t parmSlot = gpuSymbolMap[nc]._parmSlot;

      TR::Node *populateParmsNode;
      TR::Node *parmLocationNode;

      if (!hostSymRef) continue;
      if (parmSlot == -1) continue;

      if (!hostSymRef->getSymbol()->getType().isAddress())
         {
         parmLocationNode = TR::Node::create(firstNode, TR::aladd, 2);
         parmLocationNode->setAndIncChild(0, TR::Node::createWithSymRef(firstNode, addressLoadOpCode, 0, allocSymRef)); //base address of array to store GPU parameters in
         parmLocationNode->setAndIncChild(1, TR::Node::create(firstNode, TR::lconst, 0, parmSlot*sizeof(void*))); //offset into the array

         populateParmsNode = TR::Node::createWithSymRef(firstNode, TR::astorei, 2, arrayShadowSymRef);
         populateParmsNode->setAndIncChild(0, parmLocationNode); //address to store GPU parameter in
         populateParmsNode->setAndIncChild(1, TR::Node::createWithSymRef(firstNode, TR::loadaddr, 0, hostSymRef)); //GPU parameter to store
         populateParmsBlock->append(TR::TreeTop::create(comp(), populateParmsNode));
         continue;
         }

      parmLocationNode = TR::Node::create(firstNode, TR::aladd, 2);
      parmLocationNode->setAndIncChild(0, TR::Node::createWithSymRef(firstNode, addressLoadOpCode, 0, allocSymRef)); //base address of array to store GPU parameters in
      parmLocationNode->setAndIncChild(1, TR::Node::create(firstNode, TR::lconst, 0, parmSlot*sizeof(void*))); //offset into the array

      populateParmsNode = TR::Node::createWithSymRef(firstNode, TR::astorei, 2, arrayShadowSymRef);
      populateParmsNode->setAndIncChild(0, parmLocationNode); //address to store GPU parameter in
      populateParmsNode->setAndIncChild(1, TR::Node::createWithSymRef(firstNode, TR::loadaddr, 0, devSymRef)); //GPU parameter to store
      populateParmsBlock->append(TR::TreeTop::create(comp(), populateParmsNode));
      }

   }

void TR_SPMDKernelParallelizer::insertGPUEstimate(TR::Node *firstNode, TR::Block *estimateGPUBlock, TR::SymbolReference *lambdaCost, TR::SymbolReference *dataCost, TR_PrimaryInductionVariable *piv, TR::TreeTop *loopTestTree, TR::Block* origLoopBlock, TR::Block* loopInvariantBlock, TR::SymbolReference *scopeSymRef)
   {
   TR::SymbolReference *helper;
   int gpuPtxId = comp()->getGPUPtxCount()-1;
   TR::CFG *cfg = comp()->getFlowGraph();

   /////////////////////////////////////////////////////////////////////////////////////////
   // generate code segment for calculating dataCost and lambdaCost
   /////////////////////////////////////////////////////////////////////////////////////////
   CS2::ArrayOf<gpuMapElement, TR::Allocator> &gpuSymbolMap = comp()->cg()->_gpuSymbolMap;
   CS2::ArrayOf<gpuMapElement, TR::Allocator>::Cursor nc(gpuSymbolMap);

   TR::Block *nullCheckBlock = estimateGPUBlock;
   for (nc.SetToFirst(); nc.Valid(); nc.SetToNext())
      {
      TR::Node *node = gpuSymbolMap[nc]._node;
      TR::SymbolReference *hostSymRef = gpuSymbolMap[nc]._hostSymRef;
      TR::SymbolReference *tempDevicePtr = gpuSymbolMap[nc]._devSymRef;
      int32_t parmSlot = gpuSymbolMap[nc]._parmSlot;
      int32_t elementSize = gpuSymbolMap[nc]._elementSize;
      bool hoistAccess = gpuSymbolMap[nc]._hoistAccess;

      if (!hostSymRef) continue;
      if (parmSlot == -1) continue;

      hostSymRef = gpuSymbolMap[nc]._hostSymRefTemp;
      if (!hostSymRef->getSymbol()->getType().isAddress()) continue;

      TR::Block *dataCostBlock = insertBlock(comp(), cfg, loopInvariantBlock, nullCheckBlock);
      TR::Block *prevNullCheckBlock = nullCheckBlock;
      nullCheckBlock = insertBlock(comp(), cfg, loopInvariantBlock, dataCostBlock);

      TR::Node *cmpNode = TR::Node::createif(TR::ifacmpeq,
                                             TR::Node::createLoad(hostSymRef),
                                             TR::Node::createAddressNode(firstNode, TR::aconst, 0),
                                             prevNullCheckBlock->getEntry());

      nullCheckBlock->append(TR::TreeTop::create(comp(), cmpNode));
      cfg->addEdge(nullCheckBlock, prevNullCheckBlock);

      TR::Node *addNode = TR::Node::create(firstNode, TR::iadd, 2);
      addNode->setAndIncChild(0, TR::Node::createLoad(dataCost));

      TR::Node *lengthNode = TR::Node::create(TR::arraylength, 1, TR::Node::createLoad(hostSymRef));
      lengthNode->setArrayStride(elementSize);

      TR::Node *costNode = TR::Node::create(firstNode, TR::imul, 2);
      costNode->setAndIncChild(0, lengthNode);
      costNode->setAndIncChild(1, TR::Node::create(firstNode, TR::iconst, 0, elementSize));
      addNode->setAndIncChild(1, costNode);
      dataCostBlock->append(TR::TreeTop::create(comp(), TR::Node::createStore(dataCost, addNode)));
      }

   ///////////////////////////////////////////////////////////////////////////////////////////////
   // Generate call that will take in dataCost and lambdaCost to determine if GPU or CPU should be used
   ///////////////////////////////////////////////////////////////////////////////////////////////
   TR::ILOpCodes addressLoadOpCode = TR::lload;

   TR::Node *estimateGPUNode = TR::Node::create(firstNode, TR::icall, 7);
   helper = comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_estimateGPU, false, false, false);
   helper->getSymbol()->castToMethodSymbol()->setLinkage(_helperLinkage);
   estimateGPUNode->setSymbolReference(helper);

   // *cudaInfo
   estimateGPUNode->setAndIncChild(0, TR::Node::createWithSymRef(estimateGPUNode, addressLoadOpCode, 0, scopeSymRef));

   // ptxSourceID
   estimateGPUNode->setAndIncChild(1, TR::Node::create(estimateGPUNode, TR::iconst, 0, gpuPtxId));

   // startPC
   estimateGPUNode->setAndIncChild(2, TR::Node::createWithSymRef(estimateGPUNode, TR::loadaddr, 0, comp()->getSymRefTab()->findOrCreateStartPCSymbolRef()));

   // lambdaCost
   estimateGPUNode->setAndIncChild(3, TR::Node::createWithSymRef(estimateGPUNode, TR::iload, 0, lambdaCost));

   // dataCost
   estimateGPUNode->setAndIncChild(4, TR::Node::createWithSymRef(estimateGPUNode, TR::iload, 0, dataCost));

   // startInclusive
   TR::Node *entryNode = TR::Node::createLoad(firstNode, piv->getSymRef());
   estimateGPUNode->setAndIncChild(5, entryNode);

   // endExclusive
   TR::Node *loopLimit = loopTestTree->getNode()->getSecondChild()->duplicateTree();
   estimateGPUNode->setAndIncChild(6, loopLimit);

   TR::TreeTop *estimateTreeTop = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, estimateGPUNode));

   estimateGPUBlock->append(estimateTreeTop);

   TR::Node *cmpNode = TR::Node::createif(TR::ificmpne,
                                            estimateGPUNode,
                                            TR::Node::create(firstNode, TR::iconst, 0, 0), origLoopBlock->getEntry());

   estimateGPUBlock->append(TR::TreeTop::create(comp(), cmpNode));
   }


void TR_SPMDKernelParallelizer::insertGPUParmsAllocate(TR::Node *firstNode, TR::Block *allocateParmsBlock, TR::SymbolReference *allocSymRef, TR::SymbolReference *scopeSymRef)
   {
   TR::SymbolReference *helper;
   TR::ILOpCodes addressCallOpCode = TR::lcall;
   TR::ILOpCodes addressLoadOpCode = TR::lload;

   TR::Node *allocateParmsNode = TR::Node::create(firstNode, addressCallOpCode, 2);
   helper = comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_allocateGPUKernelParms, false, false, false);
   helper->getSymbol()->castToMethodSymbol()->setLinkage(_helperLinkage);
   allocateParmsNode->setSymbolReference(helper);

   // trace
   allocateParmsNode->setAndIncChild(0, TR::Node::create(firstNode, TR::iconst, 0, _verboseTrace));

   // argCount
   allocateParmsNode->setAndIncChild(1, TR::Node::create(allocateParmsNode, TR::iconst, 0, _parmSlot));

   TR::TreeTop *allocateParmsTreeTop = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, allocateParmsNode));
   allocateParmsBlock->append(allocateParmsTreeTop);

   TR::TreeTop::create(comp(), allocateParmsTreeTop, TR::Node::createStore(allocSymRef, allocateParmsNode));
   }

void TR_SPMDKernelParallelizer::insertGPUInvalidateSequence(TR::Node *firstNode, TR::Block *invalidateGPUBlock, TR::SymbolReference *scopeSymRef)
   {
   CS2::ArrayOf<gpuMapElement, TR::Allocator> &gpuSymbolMap = comp()->cg()->_gpuSymbolMap;
   CS2::ArrayOf<gpuMapElement, TR::Allocator>::Cursor nc(gpuSymbolMap);

   TR::Node *cmpNodeinvalidateGPU;
   TR::Node *invalidateGPUNode;
   TR::SymbolReference *helper;

   TR::ILOpCodes addressLoadOpCode = TR::lload;
   TR::ILOpCodes addressCallOpCode = TR::lcall;

   for (nc.SetToFirst(); nc.Valid(); nc.SetToNext())
      {
      TR::Node *node = gpuSymbolMap[nc]._node;
      TR::SymbolReference *hostSymRef = gpuSymbolMap[nc]._hostSymRef;
      TR::SymbolReference *tempDevicePtr = gpuSymbolMap[nc]._devSymRef;
      int32_t parmSlot = gpuSymbolMap[nc]._parmSlot;
      int32_t elementSize = gpuSymbolMap[nc]._elementSize;
      bool hoistAccess = gpuSymbolMap[nc]._hoistAccess;

      if (!hostSymRef) continue;
      if (parmSlot == -1) continue;

      hostSymRef = gpuSymbolMap[nc]._hostSymRefTemp;

      if (!hostSymRef->getSymbol()->getType().isAddress()) continue;

      if (hoistAccess) continue;

      invalidateGPUNode = TR::Node::create(firstNode, addressCallOpCode, 2);
      helper = comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_invalidateGPU, false, false, false);
      helper->getSymbol()->castToMethodSymbol()->setLinkage(_helperLinkage/*@*/);
      invalidateGPUNode->setSymbolReference(helper);

      // *cudaInfo
      invalidateGPUNode->setAndIncChild(0, TR::Node::createWithSymRef(firstNode, addressLoadOpCode, 0, scopeSymRef));

      // **hostRef
      invalidateGPUNode->setAndIncChild(1, TR::Node::createWithSymRef(firstNode, TR::loadaddr, 0, hostSymRef));

      TR::TreeTop *initTreeTop = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, invalidateGPUNode));
      invalidateGPUBlock->append(initTreeTop);
      }
   }

void TR_SPMDKernelParallelizer::insertGPUErrorHandler(TR::Node *firstNode, TR::Block *errorHandleBlock, TR::SymbolReference *scopeSymRef, TR::Block* recoveryBlock)
   {
   TR::SymbolReference *helper;
   TR::ILOpCodes addressLoadOpCode = TR::lload;
   TR::ILOpCodes addressCallOpCode = TR::lcall;
   TR::CFG *cfg = comp()->getFlowGraph();

   TR::Node *getStateGPUNode = TR::Node::create(firstNode, TR::icall, 2);
   helper = comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_getStateGPU, false, false, false);
   helper->getSymbol()->castToMethodSymbol()->setLinkage(_helperLinkage);
   getStateGPUNode->setSymbolReference(helper);

   // *cudaInfo
   getStateGPUNode->setAndIncChild(0, TR::Node::createWithSymRef(firstNode, addressLoadOpCode, 0, scopeSymRef));

   // startPC
   getStateGPUNode->setAndIncChild(1, TR::Node::createWithSymRef(firstNode, TR::loadaddr, 0, comp()->getSymRefTab()->findOrCreateStartPCSymbolRef()));

   TR::TreeTop *errorHandleTreeTop = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, getStateGPUNode));
   errorHandleBlock->append(errorHandleTreeTop);

   TR::Node *cmpNode = TR::Node::createif(TR::ificmpne,
                                            getStateGPUNode,
                                            TR::Node::create(firstNode, TR::iconst, 0, 0 /*GPU_EXEC_SUCCESS*/), recoveryBlock->getEntry());

   errorHandleBlock->append(TR::TreeTop::create(comp(), cmpNode));
   cfg->addEdge(errorHandleBlock,recoveryBlock);
   }

// generates a copyfrom sequence.
void TR_SPMDKernelParallelizer::insertGPUCopyFromSequence(TR::Node *firstNode, TR::Block *copyFromGPUBlock, TR::SymbolReference *scopeSymRef, TR::SymbolReference *launchSymRef, TR_PrimaryInductionVariable *piv)
   {
   CS2::ArrayOf<gpuMapElement, TR::Allocator> &gpuSymbolMap = comp()->cg()->_gpuSymbolMap;
   CS2::ArrayOf<gpuMapElement, TR::Allocator>::Cursor nc(gpuSymbolMap);
   TR::ILOpCodes addressLoadOpCode = TR::lload;
   TR::SymbolReference *helper;

   TR::SymbolReference *startAddress = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Address);
   TR::SymbolReference *endAddress = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Address);

   for (nc.SetToFirst(); nc.Valid(); nc.SetToNext())
      {
      TR::Node *node = gpuSymbolMap[nc]._node;
      TR::SymbolReference *hostSymRef = gpuSymbolMap[nc]._hostSymRef;
      TR::SymbolReference *tempDevicePtr = gpuSymbolMap[nc]._devSymRef;
      int32_t parmSlot = gpuSymbolMap[nc]._parmSlot;
      int32_t elementSize = gpuSymbolMap[nc]._elementSize;
      bool hoistAccess = gpuSymbolMap[nc]._hoistAccess;

      if (!hostSymRef) continue;
      if (parmSlot == -1) continue;

      hostSymRef = gpuSymbolMap[nc]._hostSymRefTemp;

      if (!hostSymRef->getSymbol()->getType().isAddress()) continue;
      if (hoistAccess) continue;

      if (gpuSymbolMap[nc]._lhsAddrExpr && gpuSymbolMap[nc]._lhsAddrExpr != INVALID_ADDR)
         {
         copyFromGPUBlock->append(TR::TreeTop::create(comp(),
                                      TR::Node::createStore(piv->getSymRef(), piv->getEntryValue()->duplicateTree())));

         copyFromGPUBlock->append(TR::TreeTop::create(comp(),
                                      TR::Node::createStore(startAddress, gpuSymbolMap[nc]._lhsAddrExpr->duplicateTree())));

         copyFromGPUBlock->append(TR::TreeTop::create(comp(),
                                      TR::Node::createStore(piv->getSymRef(), piv->getExitBound()->duplicateTree())));

         copyFromGPUBlock->append(TR::TreeTop::create(comp(),
                                      TR::Node::createStore(endAddress, gpuSymbolMap[nc]._lhsAddrExpr->duplicateTree())));
         }
      else
         {
         copyFromGPUBlock->append(TR::TreeTop::create(comp(),
                                      TR::Node::createStore(startAddress, TR::Node::createAddressNode(firstNode, TR::aconst, 0))));
         copyFromGPUBlock->append(TR::TreeTop::create(comp(),
                                      TR::Node::createStore(endAddress, TR::Node::createAddressNode(firstNode, TR::aconst, 0))));
         }

      TR::Node *copyFromGPUNode = TR::Node::create(firstNode, TR::icall, 7);
      helper = comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_copyFromGPU, false, false, false);
      helper->getSymbol()->castToMethodSymbol()->setLinkage(_helperLinkage);
      copyFromGPUNode->setSymbolReference(helper);

      // *cudaInfo
      copyFromGPUNode->setAndIncChild(0, TR::Node::createWithSymRef(firstNode, addressLoadOpCode, 0, scopeSymRef));

      // **hostRef
      copyFromGPUNode->setAndIncChild(1, TR::Node::createWithSymRef(firstNode, TR::loadaddr, 0, hostSymRef));

      // deviceArray
      copyFromGPUNode->setAndIncChild(2, TR::Node::createLoad(firstNode, tempDevicePtr));

      // elementSize
      copyFromGPUNode->setAndIncChild(3, TR::Node::create(firstNode, TR::iconst, 0, elementSize));

      // isNoCopyDtoH
      int isNoCopyDtoH = ((gpuSymbolMap[nc]._accessKind & TR::CodeGenerator::WriteAccess) == 0) ? 1 : 0;
      copyFromGPUNode->setAndIncChild(4, TR::Node::create(firstNode, TR::iconst, 0, isNoCopyDtoH));

      // startAddress
      copyFromGPUNode->setAndIncChild(5, TR::Node::createLoad(firstNode, startAddress));

      // endAddress
      copyFromGPUNode->setAndIncChild(6, TR::Node::createLoad(firstNode, endAddress));

      TR::TreeTop *copyFromGPUTreeTop = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, copyFromGPUNode));

      copyFromGPUBlock->append(copyFromGPUTreeTop);
      }
   }

// generates a copyto sequence. A copy sequence consists of an initializer and 0 or more copies
void TR_SPMDKernelParallelizer::insertGPUCopyToSequence(TR::Node *firstNode, TR::Block *copyToGPUBlock, TR::SymbolReference *scopeSymRef, TR_PrimaryInductionVariable *piv)
   {
   CS2::ArrayOf<gpuMapElement, TR::Allocator> &gpuSymbolMap = comp()->cg()->_gpuSymbolMap;
   CS2::ArrayOf<gpuMapElement, TR::Allocator>::Cursor nc(gpuSymbolMap);
   TR::ILOpCodes addressCallOpCode = TR::lcall;
   TR::ILOpCodes addressLoadOpCode = TR::lload;
   TR::SymbolReference *helper;
   TR::DataType addressType = TR::Int64;
   TR::Block *lastCopyToGPUBlock = copyToGPUBlock;

   TR::SymbolReference *startAddressHtoD = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Address);
   TR::SymbolReference *endAddressHtoD   = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Address);

   TR::SymbolReference *startAddressDtoH = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Address);
   TR::SymbolReference *endAddressDtoH   = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Address);

   for (nc.SetToFirst(); nc.Valid(); nc.SetToNext())
      {
      TR::Node *node = gpuSymbolMap[nc]._node;
      TR::SymbolReference *hostSymRef = gpuSymbolMap[nc]._hostSymRef;
      TR::SymbolReference *tempDevicePtr = gpuSymbolMap[nc]._devSymRef;
      int32_t parmSlot = gpuSymbolMap[nc]._parmSlot;
      int32_t elementSize = gpuSymbolMap[nc]._elementSize;
      bool hoistAccess = gpuSymbolMap[nc]._hoistAccess;

      if (!hostSymRef) continue;
      if (parmSlot == -1) continue;

      hostSymRef = gpuSymbolMap[nc]._hostSymRefTemp;
      if (!hostSymRef->getSymbol()->getType().isAddress()) continue;

      bool useLoopBoundsHtoD = (gpuSymbolMap[nc]._rhsAddrExpr && gpuSymbolMap[nc]._rhsAddrExpr != INVALID_ADDR);
      bool useLoopBoundsDtoH = (gpuSymbolMap[nc]._lhsAddrExpr && gpuSymbolMap[nc]._lhsAddrExpr != INVALID_ADDR);
      TR::SymbolReference *savePiv;

      if (useLoopBoundsHtoD || useLoopBoundsDtoH)
         {
         savePiv = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), piv->getSymRef()->getSymbol()->getDataType());
         copyToGPUBlock->append(TR::TreeTop::create(comp(),
                                TR::Node::createStore(savePiv, TR::Node::createLoad(piv->getSymRef()))));
         }

      if (useLoopBoundsHtoD)
         {
         copyToGPUBlock->append(TR::TreeTop::create(comp(),
                                TR::Node::createStore(piv->getSymRef(), piv->getEntryValue()->duplicateTree())));

         copyToGPUBlock->append(TR::TreeTop::create(comp(),
                                TR::Node::createStore(startAddressHtoD, gpuSymbolMap[nc]._rhsAddrExpr->duplicateTree())));

         copyToGPUBlock->append(TR::TreeTop::create(comp(),
                                TR::Node::createStore(piv->getSymRef(), piv->getExitBound()->duplicateTree())));

         copyToGPUBlock->append(TR::TreeTop::create(comp(),
                                TR::Node::createStore(endAddressHtoD, gpuSymbolMap[nc]._rhsAddrExpr->duplicateTree())));
         }

      if (useLoopBoundsDtoH)
         {
         copyToGPUBlock->append(TR::TreeTop::create(comp(),
                                      TR::Node::createStore(piv->getSymRef(), piv->getEntryValue()->duplicateTree())));

         copyToGPUBlock->append(TR::TreeTop::create(comp(),
                                      TR::Node::createStore(startAddressDtoH, gpuSymbolMap[nc]._lhsAddrExpr->duplicateTree())));

         copyToGPUBlock->append(TR::TreeTop::create(comp(),
                                      TR::Node::createStore(piv->getSymRef(), piv->getExitBound()->duplicateTree())));

         copyToGPUBlock->append(TR::TreeTop::create(comp(),
                                      TR::Node::createStore(endAddressDtoH, gpuSymbolMap[nc]._lhsAddrExpr->duplicateTree())));
         }

      if (useLoopBoundsHtoD || useLoopBoundsDtoH)
         {
         copyToGPUBlock->append(TR::TreeTop::create(comp(),
                                TR::Node::createStore(piv->getSymRef(), TR::Node::createLoad(savePiv))));
         }

      TR::Node *copyToGPUNode = TR::Node::create(firstNode, addressCallOpCode, 10);

      helper = comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_copyToGPU, false, false, false);
      helper->getSymbol()->castToMethodSymbol()->setLinkage(_helperLinkage);
      copyToGPUNode->setSymbolReference(helper);

      // *cudaInfo
      copyToGPUNode->setAndIncChild(0, TR::Node::createWithSymRef(firstNode, addressLoadOpCode, 0, scopeSymRef));

      // **hostRef
      copyToGPUNode->setAndIncChild(1, TR::Node::createWithSymRef(firstNode, TR::loadaddr, 0, hostSymRef));

      // elementSize
      copyToGPUNode->setAndIncChild(2, TR::Node::create(firstNode, TR::iconst, 0, elementSize));

      // accessMode
      copyToGPUNode->setAndIncChild(3, TR::Node::create(firstNode, TR::iconst, 0, gpuSymbolMap[nc]._accessKind & TR::CodeGenerator::ReadWriteAccesses));

      // isNoCopyHtoD
      int isNoCopyHtoD = ((gpuSymbolMap[nc]._accessKind & TR::CodeGenerator::ReadAccess) == 0 &&
                           gpuSymbolMap[nc]._lhsAddrExpr != NULL && gpuSymbolMap[nc]._lhsAddrExpr != INVALID_ADDR)
                          ? 1 : 0;
      copyToGPUNode->setAndIncChild(4, TR::Node::create(firstNode, TR::iconst, 0, isNoCopyHtoD));

      // isNoCopyDtoH
      int isNoCopyDtoH = ((gpuSymbolMap[nc]._accessKind & TR::CodeGenerator::WriteAccess) == 0) ? 1 : 0;
      copyToGPUNode->setAndIncChild(5, TR::Node::create(firstNode, TR::iconst, 0, isNoCopyDtoH));

      // startAddressHtoD and endAddressHtoD
      if (useLoopBoundsHtoD)
         {
         copyToGPUNode->setAndIncChild(6, TR::Node::createLoad(firstNode, startAddressHtoD));
         copyToGPUNode->setAndIncChild(7, TR::Node::createLoad(firstNode, endAddressHtoD));
         }
      else
         {
         TR::Node *aconst0Node = TR::Node::createAddressNode(firstNode, TR::aconst, 0);
         copyToGPUNode->setAndIncChild(6, aconst0Node);
         copyToGPUNode->setAndIncChild(7, aconst0Node);
         }

      // pass startAddress and endAddress for DtoH transfer
      if (useLoopBoundsDtoH)
         {
         copyToGPUNode->setAndIncChild(8, TR::Node::createLoad(firstNode, startAddressDtoH));
         copyToGPUNode->setAndIncChild(9, TR::Node::createLoad(firstNode, endAddressDtoH));
         }
      else
         {
         TR::Node *aconst0Node = TR::Node::createAddressNode(firstNode, TR::aconst, 0);
         copyToGPUNode->setAndIncChild(8, aconst0Node);
         copyToGPUNode->setAndIncChild(9, aconst0Node);
         }

      TR::TreeTop *copyToGPUTreeTop = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, copyToGPUNode));
      copyToGPUBlock->append(copyToGPUTreeTop);

      TR::TreeTop::create(comp(), copyToGPUTreeTop, TR::Node::createStore(tempDevicePtr, copyToGPUNode));
      }
   }

void TR_SPMDKernelParallelizer::insertGPUKernelLaunch(TR::SymbolReference *allocSymRef, TR::SymbolReference *scopeSymRef,
                                                      TR::Block *launchBlock, TR::Node *firstNode,
                                                      TR_PrimaryInductionVariable *piv, TR::TreeTop *loopTestTree, int kernelID,
                                                      bool hasExceptionChecks)
   {
   TR::ILOpCodes addressLoadOpCode = TR::lload;
   TR::SymbolReference *helper;
   TR::Node *launchGPUKernelNode = TR::Node::create(firstNode, TR::icall, 8);

   helper = comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_launchGPUKernel, false, false, false);
   helper->getSymbol()->castToMethodSymbol()->setLinkage(_helperLinkage);
   launchGPUKernelNode->setSymbolReference(helper);

   // *cudaInfo
   launchGPUKernelNode->setAndIncChild(0, TR::Node::createWithSymRef(firstNode, addressLoadOpCode, 0, scopeSymRef));

   // startInclusive
   TR::Node *entryNode = TR::Node::createLoad(firstNode, piv->getSymRef());
   launchGPUKernelNode->setAndIncChild(1, entryNode);

   // endExclusive
   TR::Node *loopLimit = loopTestTree->getNode()->getSecondChild()->duplicateTree();
   launchGPUKernelNode->setAndIncChild(2, loopLimit);

   // argCount
   launchGPUKernelNode->setAndIncChild(3, TR::Node::create(firstNode, TR::iconst, 0, _parmSlot));

   // **args
   launchGPUKernelNode->setAndIncChild(4, TR::Node::createWithSymRef(firstNode, addressLoadOpCode, 0, allocSymRef));

   // kernelID
   launchGPUKernelNode->setAndIncChild(5, TR::Node::create(firstNode, TR::iconst, 0, kernelID));

   // startPC
   launchGPUKernelNode->setAndIncChild(6, TR::Node::createWithSymRef(firstNode, TR::loadaddr, 0, comp()->getSymRefTab()->findOrCreateStartPCSymbolRef()));

   // hasExceptionChecks
   launchGPUKernelNode->setAndIncChild(7, TR::Node::create(firstNode, TR::iconst, 0, hasExceptionChecks));

   TR::TreeTop *launchGPUKernelTreeTop = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, launchGPUKernelNode));

   launchBlock->append(launchGPUKernelTreeTop);
   }


void TR_SPMDKernelParallelizer::insertGPURegionEntry(TR::Block * loopInvariantBlock, TR::SymbolReference * scopeSymRef, int gpuPtxCount, TR_SPMDScopeInfoType scopeType)
   {
   TR::CFG *cfg = comp()->getFlowGraph();

   TR::SymbolReference *helper;
   TR::ILOpCodes addressCallOpCode = TR::lcall;

   TR::TreeTop *insertionPoint = loopInvariantBlock->getEntry();

   TR::Node* regionEntryGPUNode = TR::Node::create(insertionPoint->getNode(), addressCallOpCode, 5);

   helper = comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_regionEntryGPU, false, false, false);
   helper->getSymbol()->castToMethodSymbol()->setLinkage(_helperLinkage/*@*/);
   regionEntryGPUNode->setSymbolReference(helper);

   // _verboseTrace
   regionEntryGPUNode->setAndIncChild(0, TR::Node::create(insertionPoint->getNode(), TR::iconst, 0, _verboseTrace));
   // ptxSourceID
   regionEntryGPUNode->setAndIncChild(1, TR::Node::create(insertionPoint->getNode(), TR::iconst, 0, gpuPtxCount));
   // startPC
   regionEntryGPUNode->setAndIncChild(2, TR::Node::createWithSymRef(insertionPoint->getNode(), TR::loadaddr, 0, comp()->getSymRefTab()->findOrCreateStartPCSymbolRef()));

   switch(scopeType)
   {
   case scopeSingleKernel:
      regionEntryGPUNode->setAndIncChild(3, TR::Node::create(insertionPoint->getNode(), TR::iconst, 0, TR::CodeGenerator::singleKernelScope));
      break;
   case scopeNaturalLoop:
      regionEntryGPUNode->setAndIncChild(3, TR::Node::create(insertionPoint->getNode(), TR::iconst, 0, TR::CodeGenerator::naturalLoopScope));
      break;
   }

   regionEntryGPUNode->setAndIncChild(4, TR::Node::create(insertionPoint->getNode(), TR::iconst, 0, comp()->getOptions()->getEnableGPU(TR_EnableGPUForce)));

   TR::Node *treetopNode = TR::Node::create(TR::treetop, 1, regionEntryGPUNode);
   TR::TreeTop *regionEntryTreeTop = TR::TreeTop::create(comp(), treetopNode, 0, 0);

   insertionPoint->insertAfter(regionEntryTreeTop);
   TR::TreeTop::create(comp(), regionEntryTreeTop, TR::Node::createStore(scopeSymRef, regionEntryGPUNode));
   }


void TR_SPMDKernelParallelizer::insertFlushGPU(TR_BitVector *flushGPUBlocks, TR::Block **cfgBlocks, TR::SymbolReference *scopeSymRef)
   {
   TR_BitVectorIterator bvi(*flushGPUBlocks);

   while (bvi.hasMoreElements())
      {
      int i = bvi.getNextElement();
      TR::Block *flushGPUBlock = cfgBlocks[i];
      TR::Node* flushGPUNode = insertFlushGPU(flushGPUBlock, scopeSymRef);
      traceMsg(comp(), "Inserted flushGPU %p in block %d\n", flushGPUNode, i);
      }
   }

TR::Node* TR_SPMDKernelParallelizer::insertFlushGPU(TR::Block* flushGPUBlock, TR::SymbolReference *scopeSymRef)
   {
   TR::ILOpCodes addressCallOpCode = TR::lcall;
   TR::ILOpCodes addressLoadOpCode = TR::lload;
   TR::SymbolReference *helper;

   TR::TreeTop *insertionPoint = flushGPUBlock->getEntry();

   TR::Node* flushGPUNode = TR::Node::create(insertionPoint->getNode(), TR::icall, 2);
   helper = comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_flushGPU, false, false, false);
   helper->getSymbol()->castToMethodSymbol()->setLinkage(_helperLinkage/*@*/);
   flushGPUNode->setSymbolReference(helper);

   // *cudaInfo
   flushGPUNode->setAndIncChild(0, TR::Node::createWithSymRef(insertionPoint->getNode(), addressLoadOpCode, 0, scopeSymRef));
   // blockId
   flushGPUNode->setAndIncChild(1, TR::Node::create(insertionPoint->getNode(), TR::iconst, 0, flushGPUBlock->getNumber()));

   TR::Node *treetopNode = TR::Node::create(TR::treetop, 1, flushGPUNode);
   TR::TreeTop *initTreeTop = TR::TreeTop::create(comp(), treetopNode, 0, 0);
   insertionPoint->insertAfter(initTreeTop);

   return flushGPUNode;
   }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Inserts fake code to keep GPU SymRefs alive for the duration of the GPU execution, if this code is not inserted
// various optimizations will try to remove these variables since they look dead to the JIT - but they are still being
// used by the various GPU helpers. The JIT doesn't account for variables that are live only within the GPU helpers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void TR_SPMDKernelParallelizer::insertGPUTemporariesLivenessCode(List<TR::TreeTop> *exitPointsList, TR::SymbolReference *liveSymRef, bool firstKernel)
   {
   CS2::ArrayOf<gpuMapElement, TR::Allocator> &gpuSymbolMap = comp()->cg()->_gpuSymbolMap;
   CS2::ArrayOf<gpuMapElement, TR::Allocator>::Cursor nc(gpuSymbolMap);

   ListIterator<TR::TreeTop> lit(exitPointsList);

   for (TR::TreeTop *insertionPoint = lit.getFirst(); insertionPoint; insertionPoint = lit.getNext())
      {
      TR::Node* currentNode = NULL;

      if (firstKernel)
         {
         currentNode = TR::Node::lconst(insertionPoint->getNode(), 0);
         }
      else
         {
         currentNode = TR::Node::create(TR::a2l, 1, TR::Node::createWithSymRef(insertionPoint->getNode(), TR::loadaddr, 0, liveSymRef));
         }

      for (nc.SetToFirst(); nc.Valid(); nc.SetToNext())
         {
         TR::SymbolReference *hostSymRef = gpuSymbolMap[nc]._hostSymRef;
         TR::SymbolReference *devSymRef = gpuSymbolMap[nc]._devSymRef;
         int32_t parmSlot = gpuSymbolMap[nc]._parmSlot;

         if (!hostSymRef) continue;
         if (parmSlot == -1) continue;

         hostSymRef = gpuSymbolMap[nc]._hostSymRefTemp;

         currentNode = TR::Node::create(TR::ladd, 2, TR::Node::create(TR::a2l, 1, TR::Node::createWithSymRef(insertionPoint->getNode(), TR::loadaddr, 0, hostSymRef)), currentNode);

         if (!hostSymRef->getSymbol()->getType().isAddress()) continue;

         currentNode = TR::Node::create(TR::ladd, 2, TR::Node::create(TR::a2l, 1, TR::Node::createWithSymRef(insertionPoint->getNode(), TR::loadaddr, 0, devSymRef)), currentNode);
         }

      TR::Node *treetopNode = TR::Node::create(TR::treetop, 1, currentNode);
      TR::TreeTop *initTreeTop = TR::TreeTop::create(comp(), treetopNode, 0, 0);
      insertionPoint->insertBefore(initTreeTop);

      TR::TreeTop::create(comp(), initTreeTop, TR::Node::createStore(liveSymRef, currentNode));
      }
   }

void TR_SPMDKernelParallelizer::insertGPURegionExits(List<TR::Block>* exitBlocks, TR::SymbolReference *scopeSymRef, int gpuPtxCount, TR::SymbolReference *liveSymRef, List<TR::TreeTop> *exitPointsList)
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   ListIterator<TR::Block> si(exitBlocks);

   TR::ILOpCodes addressCallOpCode = TR::lcall;
   TR::ILOpCodes addressLoadOpCode = TR::lload;
   TR::SymbolReference *helper;
   TR::Block *exitBlock = 0;


   for (exitBlock = si.getCurrent(); exitBlock; exitBlock = si.getNext())
      {
      TR::TreeTop *insertionPoint = exitBlock->getEntry();
      
      TR::Node* regionExitGPUNode = TR::Node::create(insertionPoint->getNode(), TR::icall, 4);
      helper = comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_regionExitGPU, false, false, false);
      helper->getSymbol()->castToMethodSymbol()->setLinkage(_helperLinkage/*@*/);
      regionExitGPUNode->setSymbolReference(helper);

      // *cudaInfo
      regionExitGPUNode->setAndIncChild(0, TR::Node::createWithSymRef(insertionPoint->getNode(), addressLoadOpCode, 0, scopeSymRef));

      // startPC
      regionExitGPUNode->setAndIncChild(1, TR::Node::createWithSymRef(insertionPoint->getNode(), TR::loadaddr, 0, comp()->getSymRefTab()->findOrCreateStartPCSymbolRef()));

      // ptxSourceID
      regionExitGPUNode->setAndIncChild(2, TR::Node::create(insertionPoint->getNode(), TR::iconst, 0, gpuPtxCount));

      // **liveSymRef
      regionExitGPUNode->setAndIncChild(3, TR::Node::createWithSymRef(insertionPoint->getNode(), TR::loadaddr, 0, liveSymRef));

      TR::Node *treetopNode = TR::Node::create(TR::treetop, 1, regionExitGPUNode);
      TR::TreeTop *initTreeTop = TR::TreeTop::create(comp(), treetopNode, 0, 0);

      insertionPoint->insertAfter(initTreeTop);
      exitPointsList->add(initTreeTop);
      }
   }


void TR_SPMDKernelParallelizer::insertGPURegionExitInRegionExits(List<TR::Block> *exitBlocks, List<TR::Block> *blocksInRegion, TR::SymbolReference *scopeSymRef, TR::SymbolReference *liveSymRef, List<TR::TreeTop> *exitPointsList)
   {
   List<TR::CFGEdge> exitEdges(comp()->trMemory());

   TR::CFG *cfg = comp()->getFlowGraph(); 
   TR_BitVector *blocksInsideLoop = new (trStackMemory()) TR_BitVector(cfg->getNextNodeNumber(), trMemory(), stackAlloc);

   TR::Block *blockInLoop = 0;
   ListIterator<TR::Block> si(blocksInRegion);
   for (blockInLoop = si.getCurrent(); blockInLoop; blockInLoop = si.getNext())
      blocksInsideLoop->set(blockInLoop->getNumber());

   TR::ILOpCodes addressCallOpCode = TR::lcall;
   TR::ILOpCodes addressLoadOpCode = TR::lload;
   TR::SymbolReference *helper = 0;
   int gpuPtxId = comp()->getGPUPtxCount() - 1; //gpuPtxId starts at 0 so we need to subtract 1 to get the id from the current count

   TR::Block *exitBlock = 0;
   si.set(exitBlocks);
   for (exitBlock = si.getCurrent(); exitBlock; exitBlock = si.getNext())
      {
      for (auto edge = exitBlock->getSuccessors().begin(); edge != exitBlock->getSuccessors().end(); ++edge)
         {
         TR::Block* next = toBlock((*edge)->getTo());

         if (!blocksInsideLoop->get(next->getNumber()))
            {
            exitEdges.add(*edge);
            }
         }
      }

   TR::CFGEdge *edge = 0;
   ListIterator<TR::CFGEdge> ei(&exitEdges);

   for (edge = ei.getCurrent(); edge; edge = ei.getNext())
      {
      TR::Block* exitBlock = toBlock(edge->getFrom());
      TR::Block* next = toBlock(edge->getTo());

      TR::Block* regionExitGPUBlock = exitBlock->splitEdge(exitBlock, next, comp());

      TR::TreeTop *insertionPoint = regionExitGPUBlock->getEntry();

      TR::Node* regionExitGPUNode = TR::Node::create(insertionPoint->getNode(), TR::icall, 4);
      helper = comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_regionExitGPU, false, false, false);
      helper->getSymbol()->castToMethodSymbol()->setLinkage(_helperLinkage/*@*/);
      regionExitGPUNode->setSymbolReference(helper);

      // *cudaInfo
      regionExitGPUNode->setAndIncChild(0, TR::Node::createWithSymRef(insertionPoint->getNode(), addressLoadOpCode, 0, scopeSymRef));

      // startPC
      regionExitGPUNode->setAndIncChild(1, TR::Node::createWithSymRef(insertionPoint->getNode(), TR::loadaddr, 0, comp()->getSymRefTab()->findOrCreateStartPCSymbolRef()));

      // ptxSourceID
      regionExitGPUNode->setAndIncChild(2, TR::Node::create(insertionPoint->getNode(), TR::iconst, 0, gpuPtxId));

      // **liveSymRef
      regionExitGPUNode->setAndIncChild(3, TR::Node::createWithSymRef(insertionPoint->getNode(), TR::loadaddr, 0, liveSymRef));

      TR::Node *treetopNode = TR::Node::create(TR::treetop, 1, regionExitGPUNode);
      TR::TreeTop *initTreeTop = TR::TreeTop::create(comp(), treetopNode, 0, 0);

      insertionPoint->insertAfter(initTreeTop);
      exitPointsList->add(initTreeTop);
      }
   }

bool TR_SPMDKernelParallelizer::addRegionCost(TR_RegionStructure *region, TR_RegionStructure *loop, TR::Block *estimateGPUBlock, TR::SymbolReference *lambdaCost)
   {
   if (region->getEntryBlock()->isCold()) return false;

   bool addedCost = false;

   TR_PrimaryInductionVariable *piv = region->getPrimaryInductionVariable();

   if (region != loop &&
       region->isNaturalLoop() &&
       piv &&
       piv->getIncrement() == 1 &&
       piv->getEntryValue() &&
       piv->getExitBound() &&
       // since these nodes are not used in the loop, we need to recalculate their invariance
       loop->isExprInvariant(piv->getEntryValue(), false) &&
       loop->isExprInvariant(piv->getExitBound(), false))
      {
      TR::Node *addNode;

      traceMsg(comp(), "adding cost of loop %d with piv %p entry %p exit %p %d %d\n", region->getNumber(), piv, piv->getEntryValue(), piv->getExitBound(), loop->isExprInvariant(piv->getEntryValue(), false), loop->isExprInvariant(piv->getExitBound(), false));

      addNode = TR::Node::create(estimateGPUBlock->getLastRealTreeTop()->getNode(), TR::iadd, 2);
      addNode->setAndIncChild(0, TR::Node::createLoad(lambdaCost));

      TR::Node *costNode = TR::Node::create(estimateGPUBlock->getLastRealTreeTop()->getNode(), TR::isub, 2);
      costNode->setAndIncChild(0, piv->getExitBound());
      costNode->setAndIncChild(1, piv->getEntryValue());

      addNode->setAndIncChild(1, costNode);

      TR::TreeTop *treetop = estimateGPUBlock->getLastRealTreeTop()->getNode()->getOpCode().isBranch() ?
                                 estimateGPUBlock->getLastRealTreeTop()->getPrevTreeTop()
                                 : estimateGPUBlock->getLastRealTreeTop();

      TR::TreeTop::create(comp(), treetop, TR::Node::createStore(lambdaCost, addNode));

      addedCost = true;
      }

   TR_RegionStructure::Cursor it(*region);
   for (TR_StructureSubGraphNode *node = it.getFirst(); node; node = it.getNext())
      {
      if (node->getStructure()->asRegion())
         {
         addedCost |= addRegionCost(node->getStructure()->asRegion(), loop, estimateGPUBlock, lambdaCost);
         }
      }

   return addedCost;
   }

bool TR_SPMDKernelParallelizer::estimateGPUCost(TR_RegionStructure *loop, TR::Block *estimateBlock, TR::SymbolReference *lambdaCost)
   {
   return addRegionCost(loop, loop, estimateBlock, lambdaCost);
   }

bool TR_SPMDKernelParallelizer::processGPULoop(TR_RegionStructure *loop, TR_SPMDScopeInfo* gpuScope)
   {
   _verboseTrace = 0;
   if (comp()->getOptions()->getEnableGPU(TR_EnableGPUVerbose)) _verboseTrace = 1;
   if (comp()->getOptions()->getEnableGPU(TR_EnableGPUDetails)) _verboseTrace = 2;

   static bool disableJNIbasedGPUHelpers = feGetEnv("TR_disableJNIbasedGPUHelpers") ? true : false;
   _helperLinkage = disableJNIbasedGPUHelpers ? TR_System : TR_J9JNILinkage;

   TR_PrimaryInductionVariable *piv = loop->getPrimaryInductionVariable();

   TR::Block *branchBlock = piv->getBranchBlock();
   TR::Block *origLoopBlock = loop->getEntryBlock();
   TR::TreeTop *lastTree = branchBlock->getLastRealTreeTop();
   TR::TreeTop *firstTree = origLoopBlock->getEntry();
   TR::TreeTop *loopTestTree = branchBlock->getLastRealTreeTop();
   int32_t lineNumber = comp()->getLineNumberInCurrentMethod(firstTree->getNode());
   TR::Node *firstNode = firstTree->getNode();

   TR::Block *loopInvariantBlock = findLoopInvariantBlock(comp(), loop);

   if (gpuScope->getScopeType() == scopeNaturalLoop)
      {
      TR_RegionStructure *parentLoop = gpuScope->getEnvelopingLoop();

      if (!findLoopInvariantBlock(comp(), parentLoop)) return false; // TODO: create preheader
      }

   if (!loopInvariantBlock) return false; // TODO: create preheader

   // GPU_TODO: make sure there are no other loop exits
   TR::Block *blockAfterLoop = branchBlock->getExit()->getNextTreeTop()->getNode()->getBlock();

   traceMsg(comp(), "GPU loop %d: first tree = %p last tree = %p loopTest = %p block after loop = %d\n", loop->getNumber(), firstNode, lastTree->getNode(), loopTestTree->getNode(), blockAfterLoop->getNumber());

   if (gpuScope->getScopeType() == scopeNaturalLoop)
      {
      traceMsg(comp(), "GPU Scope: natural loop\n");
      }
   else
      {
      traceMsg(comp(), "GPU Scope: single kernel\n");
      }

   if (loopInvariantBlock == NULL)
      {
      traceMsg(comp(), "Stop processing since %d doesn't have a no-preheader\n", loop->getNumber());
      return false;
      }

   TR::CFG *cfg = comp()->getFlowGraph();
   TR::Block *predBlock =  loopInvariantBlock;
   TR::Block *ibmTryGPUBlock = NULL;
   TR::Block *lambdaCPUBlock = NULL;

   while (predBlock->getPredecessors().size() == 1)
       {
       //traceMsg(comp(), "predBlock %d\n", predBlock->getNumber());

       predBlock = toBlock(predBlock->getPredecessors().front()->getFrom());

       if (predBlock->getNumber() == 0) break;

       TR::Node *node = predBlock->getLastRealTreeTop()->getNode();

       if ((node->getOpCodeValue() == TR::ificmpeq ||
            node->getOpCodeValue() == TR::ificmpne) &&
            node->getFirstChild()->getOpCode().isLoadDirect() &&
            node->getFirstChild()->getSymbolReference()->getSymbol()->isStatic() &&
            node->getSecondChild()->getOpCode().isLoadConst() &&
            node->getSecondChild()->getInt() == 0)
          {
          TR::SymbolReference *symRef = node->getFirstChild()->getSymbolReference();
          const char *sig = symRef->getOwningMethod(comp())->fieldName(symRef->getCPIndex(), comp()->trMemory());

          if (strcmp("java/util/stream/IntPipeline.ibmTryGPU Z", sig) != 0) continue;

          ibmTryGPUBlock = predBlock;
          lambdaCPUBlock = ((node->getOpCodeValue() == TR::ificmpeq) != _reversedBranchNodes->isSet(node->getGlobalIndex())) ? node->getBranchDestination()->getNode()->getBlock() : predBlock->getNextBlock();
          break;
          }
       }

   if (!ibmTryGPUBlock || !lambdaCPUBlock)
      {
          traceMsg(comp(), "Stop processing since could not find ibmTryGPU or lambdaCPU Block\n");
      return false;
      }

   traceMsg(comp(), "ibmTryGPU block %d\n", ibmTryGPUBlock->getNumber());
   traceMsg(comp(), "lambdaCPU block %d\n", lambdaCPUBlock->getNumber());
   traceMsg(comp(), "loopInvariantBlock %d\n", loopInvariantBlock->getNumber());



#if 0 // GPU_TODO: will enable after some testing
   // replace a target BB to slow loop with that to the original lambda block
   predBlock = loopInvariantBlock;
   while (predBlock->getPredecessors().size() == 1)
      {
      predBlock = toBlock(predBlock->getPredecessors().front()->getFrom());
      if (predBlock == ibmTryGPUBlock) break;
      if (predBlock->getNumber() == 0) break;

      TR::Node *node = predBlock->getLastRealTreeTop()->getNode();
      if (node->getOpCode().isBranch())
         {
         TR::Block *blockSlowloop = node->getBranchDestination()->getNode()->getBlock();
         if (blockSlowloop->isCold() || blockSlowloop->isSuperCold())
            {
            TR::BlockStructure *blockStructure = blockSlowloop->getStructureOf();
            if (blockStructure && blockStructure->isLoopInvariantBlock())
               predBlock->changeBranchDestination(lambdaCPUBlock->getEntry(), cfg);
            }
         }
      }
#endif

   char *programSource;
   TR::Node *errorNode;
   List<TR::AutomaticSymbol> autos(trMemory());
   ListAppender<TR::AutomaticSymbol> aa(&autos);
   aa.add((TR::AutomaticSymbol*)piv->getSymRef()->getSymbol());

   List<TR::ParameterSymbol> parms(trMemory());
   ListAppender<TR::ParameterSymbol> pa(&parms);

   comp()->cg()->_gpuSymbolMap.MakeEmpty();

   loop->resetInvariance();
   loop->computeInvariantExpressions();

   // create bitvector of all blocks in loop
   SharedSparseBitVector blocksInLoop(comp()->allocator());
   TR_ScratchList<TR::Block> blocksInLoopList(comp()->trMemory());
   loop->getBlocks(&blocksInLoopList);
   ListIterator<TR::Block> blocksIt(&blocksInLoopList);
   TR::Block *nextBlock;
   for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      {
      blocksInLoop[nextBlock->getNumber()] = true;
      }

   // analysis pass
   int32_t visitCount = comp()->incVisitCount();
   int32_t currentBlock = 0;
   _parmSlot = 0;
   for (TR::TreeTop *tree = comp()->getStartTree(); tree != comp()->findLastTree(); tree = tree->getNextTreeTop())
      {
      if (tree->getNode()->getOpCodeValue() == TR::BBStart)
         currentBlock = tree->getNode()->getBlock()->getNumber();

      if (!blocksInLoop.ValueAt(currentBlock))
         continue;
      if (tree == loopTestTree)
         continue;

      if (!visitNodeToMapSymbols(tree->getNode(), pa, aa, loop, piv, lineNumber, visitCount))
         {
         return false;
         }
      }

   traceMsg(comp(), "#parms = %d\n", parms.getSize());

   visitCount = comp()->incVisitCount();
   currentBlock = 0;
   bool arrayNotFound = false;

   TR_Dominators *postDominators = new (comp()->trStackMemory()) TR_Dominators(comp(), true);
   bool postDominates = false;

   for (TR::TreeTop *tree = comp()->getStartTree(); tree != comp()->findLastTree(); tree = tree->getNextTreeTop())
      {
      if (tree->getNode()->getOpCodeValue() == TR::BBStart)
         {
         postDominates = false;
         currentBlock = tree->getNode()->getBlock()->getNumber();
         if (postDominators && postDominators->dominates(tree->getNode()->getBlock(), loopInvariantBlock))
            postDominates = true;
         }

      if (!blocksInLoop.ValueAt(currentBlock))
         continue;

      if (tree == loopTestTree)
         continue;

      if (!visitNodeToDetectArrayAccesses(tree->getNode(), loop, piv, visitCount, lineNumber, arrayNotFound, _verboseTrace, postDominates) &&
          !comp()->getOptions()->getEnableGPU(TR_EnableSafeMT))
         return false;
      }

   static bool disableDataTransferElimination = (feGetEnv("TR_disableGPUDataTransferElimination") != NULL);

   if (disableDataTransferElimination || arrayNotFound)
      {
      CS2::ArrayOf<gpuMapElement, TR::Allocator> &gpuSymbolMap = comp()->cg()->_gpuSymbolMap;
      CS2::ArrayOf<gpuMapElement, TR::Allocator>::Cursor nc(gpuSymbolMap);
      for (nc.SetToFirst(); nc.Valid(); nc.SetToNext())
         {
         TR::SymbolReference *hostSymRef = gpuSymbolMap[nc]._hostSymRef;
         int32_t parmSlot = gpuSymbolMap[nc]._parmSlot;
         if (!hostSymRef) continue;
         if (parmSlot == -1) continue;
         gpuSymbolMap[nc]._accessKind |= TR::CodeGenerator::ReadWriteAccesses;
         }
      }

   TR::SymbolReference *helper;
   TR::SymbolReference *lambdaCost = NULL;
   TR::SymbolReference *dataCost = NULL;
   TR::Block *estimateGPUBlock = loopInvariantBlock;

   if (!comp()->getOptions()->getEnableGPU(TR_EnableGPUForce))
      {
      // initialize lambdaCost and dataCost
      lambdaCost = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Int32);
      dataCost = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), TR::Int32);

      TR::TreeTop *lastTree = estimateGPUBlock->getLastRealTreeTop()->getNode()->getOpCode().isBranch() ?
                                 estimateGPUBlock->getLastRealTreeTop()->getPrevTreeTop()
                                 : estimateGPUBlock->getLastRealTreeTop();

      TR::TreeTop::create(comp(), lastTree,
                           TR::Node::createStore(lambdaCost,
                           TR::Node::create(loopInvariantBlock->getLastRealTreeTop()->getNode(), TR::iconst, 0, 0)));

      TR::TreeTop::create(comp(), lastTree,
                           TR::Node::createStore(dataCost,
                           TR::Node::create(loopInvariantBlock->getLastRealTreeTop()->getNode(), TR::iconst, 0, 0)));

      traceMsg(comp(), "lambdaCost #%d\n", lambdaCost->getReferenceNumber());
      traceMsg(comp(), "dataCost #%d\n", dataCost->getReferenceNumber());

      if (!estimateGPUCost(loop, estimateGPUBlock, lambdaCost))
         {
         traceMsg(comp(), "lambda is too small\n");
         if (_verboseTrace > 1)
            TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tforEach in %s at line %d does not have any loops", comp()->signature(), lineNumber);
         // return false;  // TransposeDouble seems to run faster on GPU
         }
      }

   char *ptxSource = NULL;
   int gpuPtxCount = 0;
   bool hasExceptionChecks = false;

#ifdef ENABLE_GPU
   gpuPtxCount = comp()->getGPUPtxCount();

   TR::CodeGenerator::GPUResult gpuResult = comp()->cg()->dumpNVVMIR(comp()->getStartTree(), comp()->findLastTree(), loop, &blocksInLoop,
                                                                    &autos, &parms, true, programSource, errorNode, gpuPtxCount,
                                                                    &hasExceptionChecks);

   if (gpuResult != TR::CodeGenerator::GPUSuccess)
       return false;

   char *generatePTX(int tracing, const char *programSource, int deviceId, TR::PersistentInfo * persistentInfo, TR_Memory* m, bool enableMath);

   bool enableMath = comp()->getOptions()->getEnableGPU(TR_EnableGPUEnableMath);
   int deviceId = 0;
   ptxSource = generatePTX(_verboseTrace, programSource, deviceId, comp()->getPersistentInfo(), comp()->trMemory(), enableMath);

   if (!ptxSource)
      {
      traceMsg(comp(), "generatePTX function returned 0.\n");
      return false;
      }

   char** ptxSourcePtr = (char**)comp()->trMemory()->allocateHeapMemory(sizeof(char*));

   *ptxSourcePtr = ptxSource;
   comp()->getGPUPtxList().append(ptxSourcePtr);

   int32_t* kernelLineNumber = (int32_t*)comp()->trMemory()->allocateHeapMemory(sizeof(int32_t));

   *kernelLineNumber = lineNumber;
   comp()->getGPUKernelLineNumberList().append(kernelLineNumber);

   comp()->incGPUPtxCount();
#endif

   /////////////////////////////////////////////////////////////////////////////////
   // GPU_TODO: handle 32-bit
   TR::ILOpCodes addressCallOpCode = TR::lcall;
   TR::ILOpCodes addressCmpeqOpCode = TR::iflcmpeq;
   TR::ILOpCodes addressLoadOpCode = TR::lload;
   TR::DataType addressType = TR::Int64;

   if (gpuScope->getScopeType() == scopeNaturalLoop)
      {
      traceMsg(comp(), "GPU Data Transfer optimization for loop %d\n",loop->getNumber());

      CS2::ArrayOf<gpuMapElement, TR::Allocator> &gpuSymbolMap = comp()->cg()->_gpuSymbolMap;
      CS2::ArrayOf<gpuMapElement, TR::Allocator>::Cursor nc(gpuSymbolMap);

      for (nc.SetToFirst(); nc.Valid(); nc.SetToNext())
         {
         TR::Node *node = gpuSymbolMap[nc]._node;
         TR::SymbolReference *hostSymRef = gpuSymbolMap[nc]._hostSymRef;
         int32_t parmSlot = gpuSymbolMap[nc]._parmSlot;
         int32_t elementSize = gpuSymbolMap[nc]._elementSize;
         bool to_hoist = gpuSymbolMap[nc]._hoistAccess;

         if (!hostSymRef) continue;
         if (parmSlot == -1) continue;
         if (!hostSymRef->getSymbol()->getType().isAddress()) continue;

         gpuSymbolMap[nc]._hoistAccess = true;
         }
      }

   //////////////////////////////////////////////////////////////////////////////////////
   // Construct block structure for GPU session
   //////////////////////////////////////////////////////////////////////////////////////

   // Important to insert some tree tops into loopInvariantBlock before splitting
   TR::Node *lastNode = loopInvariantBlock->getLastRealTreeTop()->getNode();
   if (lastNode->getOpCode().isGoto())
      {
      traceMsg(comp(), "Changing destination of goto at the end of block_%d\n", loopInvariantBlock->getNumber());
      lastNode->setBranchDestination(blockAfterLoop->getEntry());
      }
   else
      {
      traceMsg(comp(), "Inserting goto at the end of block_%d\n", loopInvariantBlock->getNumber());
      TR::Node *gotoNode = TR::Node::create(firstNode, TR::Goto, 0, blockAfterLoop->getEntry());
      TR::TreeTop *gotoTreeTop = TR::TreeTop::create(comp(), gotoNode);
      loopInvariantBlock->append(gotoTreeTop);
      }

   TR::Block *gotoBlock = loopInvariantBlock->split(loopInvariantBlock->getLastRealTreeTop(), cfg, true, false);

   TR::Block *estimateBlock = insertBlock(comp(), cfg, loopInvariantBlock, gotoBlock);

   if (!comp()->getOptions()->getEnableGPU(TR_EnableGPUForce))
   {
   cfg->addEdge(estimateBlock, origLoopBlock);
   }

   cfg->addEdge(gotoBlock, blockAfterLoop);
   cfg->removeEdge(gotoBlock, origLoopBlock);

   TR::Block *copyToGPUBlock      = insertBlock(comp(), cfg, estimateBlock,      gotoBlock);
   TR::Block *allocateParmsBlock  = insertBlock(comp(), cfg, copyToGPUBlock,     gotoBlock);
   TR::Block *populateParmsBlock  = insertBlock(comp(), cfg, allocateParmsBlock, gotoBlock);
   TR::Block *launchBlock         = insertBlock(comp(), cfg, populateParmsBlock, gotoBlock);
   TR::Block *copyFromGPUBlock    = insertBlock(comp(), cfg, launchBlock,        gotoBlock);
   TR::Block *kernelExitBlock     = insertBlock(comp(), cfg, copyFromGPUBlock, gotoBlock);
   TR::Block *errorHandleBlock    = insertBlock(comp(), cfg, kernelExitBlock,  gotoBlock);


   //////////////////////////////////////////////////////////////////////////////////////
   // Create symbols for GPU session
   //////////////////////////////////////////////////////////////////////////////////////

   // contains the return CudaInfo from invalidateGPU
   TR::SymbolReference *scopeSymRef;
   TR::SymbolReference *allocSymRef = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), addressType);
   TR::SymbolReference *liveSymRef;

   CS2::ArrayOf<gpuMapElement, TR::Allocator> &gpuSymbolMap = comp()->cg()->_gpuSymbolMap;
   CS2::ArrayOf<gpuMapElement, TR::Allocator>::Cursor nc(gpuSymbolMap);
   for (nc.SetToFirst(); nc.Valid(); nc.SetToNext())
      {
      TR::Node *node = gpuSymbolMap[nc]._node;
      TR::SymbolReference *hostSymRef = gpuSymbolMap[nc]._hostSymRef;
      int32_t parmSlot = gpuSymbolMap[nc]._parmSlot;

      if (!hostSymRef) continue;
      if (parmSlot == -1) continue;

      if (hostSymRef->getSymbol()->isShadow())
         {
         // GPU_TODO: we did artificial PRE -- check for exceptions!!
         TR::SymbolReference *temp = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(),
                                                                            hostSymRef->getSymbol()->getDataType());
         loopInvariantBlock->append(TR::TreeTop::create(comp(), TR::Node::createStore(temp, node)));
         hostSymRef = temp;
         }
      gpuSymbolMap[nc]._hostSymRefTemp = hostSymRef;

      if (!hostSymRef->getSymbol()->getType().isAddress()) continue;

      gpuSymbolMap[nc]._devSymRef = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), addressType);
      }

   //////////////////////////////////////////////////////////////////////////////////////
   // Check if the enveloping loop was transformed, if not transform it
   //////////////////////////////////////////////////////////////////////////////////////

   TR_RegionStructure *parentLoop;
   TR::Block *hoistRegionInvariantBlock;
   TR_ScratchList<TR::Block> exitBlocks(comp()->trMemory());

   switch(gpuScope->getScopeType())
      {
      case scopeSingleKernel:

         gpuScope->_scopeSymRef = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), addressType);
         scopeSymRef = gpuScope->_scopeSymRef;

         gpuScope->_liveSymRef  = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), addressType);
         liveSymRef = gpuScope->_liveSymRef;

         parentLoop = loop;
         hoistRegionInvariantBlock = loopInvariantBlock;

         exitBlocks.add(kernelExitBlock);

         insertGPURegionEntry(hoistRegionInvariantBlock, gpuScope->_scopeSymRef, gpuPtxCount, gpuScope->getScopeType());
         insertGPURegionExits(&exitBlocks, gpuScope->_scopeSymRef, gpuPtxCount, liveSymRef, gpuScope->getExitLocationsList()); 

         insertGPUTemporariesLivenessCode(gpuScope->getExitLocationsList(), liveSymRef, true);
         break;

      case scopeNaturalLoop:
         if (!gpuScope->_envelopeLoopTransformed)
            {
            List<TR::Block> envelopeExitBlocks(comp()->trMemory());
            List<TR::Block> blocksInRegion(comp()->trMemory());

            parentLoop = gpuScope->getEnvelopingLoop();
            parentLoop->collectExitBlocks(&envelopeExitBlocks);
            parentLoop->getBlocks(&blocksInRegion);

            hoistRegionInvariantBlock = findLoopInvariantBlock(comp(), parentLoop);

            gpuScope->_scopeSymRef = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), addressType);
            scopeSymRef = gpuScope->_scopeSymRef;

            gpuScope->_liveSymRef  = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), addressType);
            liveSymRef = gpuScope->_liveSymRef;

            insertGPURegionEntry(hoistRegionInvariantBlock, scopeSymRef, gpuPtxCount, gpuScope->getScopeType());
            insertGPURegionExitInRegionExits(&envelopeExitBlocks, &blocksInRegion, scopeSymRef, liveSymRef, gpuScope->getExitLocationsList());
            insertGPUTemporariesLivenessCode(gpuScope->getExitLocationsList(), liveSymRef, true);
            insertFlushGPU(gpuScope->getFlushGPUBlocks(), _origCfgBlocks, scopeSymRef);

            gpuScope->_envelopeLoopTransformed = true;
            }
         else
            {
            scopeSymRef = gpuScope->_scopeSymRef;      
            liveSymRef  = gpuScope->_liveSymRef;
            insertGPUTemporariesLivenessCode(gpuScope->getExitLocationsList(), liveSymRef, false);
            }
       break;
      }

   //////////////////////////////////////////////////////////////////////////////////////
   // Inserting code for handling the GPU session
   //////////////////////////////////////////////////////////////////////////////////////

   insertGPUParmsAllocate(firstNode,allocateParmsBlock,allocSymRef, scopeSymRef);
   generateGPUParmsBlock(allocSymRef,populateParmsBlock,firstNode);
   insertGPUKernelLaunch(allocSymRef, scopeSymRef, launchBlock, firstNode, piv, loopTestTree, gpuPtxCount, hasExceptionChecks);

   insertGPUCopyToSequence(firstNode,copyToGPUBlock,scopeSymRef, piv);
   //insertGPUInvalidateSequence(firstNode,copyToGPUBlock,scopeSymRef);

   //insertGPUCopyFromSequence(firstNode,copyFromGPUBlock,scopeSymRef,scopeSymRef/*launchSymRef*/,piv);

   insertGPUErrorHandler(firstNode, errorHandleBlock, scopeSymRef, lambdaCPUBlock);

   if (!comp()->getOptions()->getEnableGPU(TR_EnableGPUForce))
      {
      insertGPUEstimate(firstNode, estimateBlock, lambdaCost, dataCost, piv, loopTestTree, origLoopBlock, loopInvariantBlock, scopeSymRef);
      }


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

   TR::Node *ibmTryGPUNode = ibmTryGPUBlock->getLastRealTreeTop()->getNode();

   if(!(_reversedBranchNodes->isSet(ibmTryGPUNode->getGlobalIndex()))) //check if not set
      {
      traceMsg(comp(), "Reversing branch in node %p, Global Index %u\n", ibmTryGPUNode, ibmTryGPUNode->getGlobalIndex());
      ibmTryGPUNode->reverseBranch(ibmTryGPUNode->getBranchDestination());
      _reversedBranchNodes->set(ibmTryGPUNode->getGlobalIndex());
      }

   return true;
   }

TR_SPMDKernelParallelizer::TR_SPMDKernelParallelizer(TR::OptimizationManager *manager)
    : TR_LoopTransformer(manager), _pivList(comp()->allocator(), NULL), _visitedNodes(comp()->getNodeCount(), comp()->trMemory(), heapAlloc, growable)
   {
   _loopDataType = new (trStackMemory()) TR_HashTab(comp()->trMemory(), stackAlloc, comp()->getFlowGraph()->getNextNodeNumber());

#if 0 // TODO: do we need to set these flags somewhere ?
   if (comp()->getMethodHotness() > warm)
      _flags.set(requiresLocalsUseDefInfo | doesNotRequireLoadsAsDefs | requiresLocalsValueNumbering);
#endif
   _reversedBranchNodes = new (trStackMemory()) TR_BitVector(comp()->getNodeCount(), trMemory(), stackAlloc, growable);

   };

int32_t
TR_SPMDKernelParallelizer::perform()
   {
   if (optimizer()->optsThatCanCreateLoopsDisabled())
      return 0;

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   TR_UseDefInfo *useDefInfo = optimizer()->getUseDefInfo();
   if (!useDefInfo) return 1;
   useDefInfo->buildDefUseInfo();

   TR_RegionStructure *root = comp()->getFlowGraph()->getStructure()->asRegion();

   //_haveProfilingInfo = true; //comp()->getFlowGraph()->setFrequencies();

   _fpreductionAnnotation = comp()->getOption(TR_EnableFpreductionAnnotation)
                            && currentMethodHasFpreductionAnnotation(comp(), trace());

   _origCfgBlocks = comp()->getFlowGraph()->createArrayOfBlocks();

   List<TR_SPMDScopeInfo>   gpuScopes(trMemory());
   List<TR_RegionStructure> gpuKernels(trMemory());

#ifdef ENABLE_GPU
   if (comp()->getOptions()->getEnableGPU(TR_EnableGPU))
      collectGPUScopes(root, gpuKernels, gpuScopes);
#endif

   List<TR_RegionStructure> simdLoops(trMemory());
   TR_HashTab* reductionOperationsHashTab = new (comp()->trStackMemory()) TR_HashTab(comp()->trMemory(), stackAlloc);

  //TODO: make independent of GPU
   if ((!comp()->getOption(TR_DisableAutoSIMD) &&
        comp()->cg()->getSupportsAutoSIMD()) ||
        comp()->getOptions()->getEnableGPU(TR_EnableGPU))
      collectParallelLoops(root, simdLoops, reductionOperationsHashTab, useDefInfo);


   ListIterator<TR_SPMDScopeInfo> scopeit(&gpuScopes);
   ListIterator<TR_RegionStructure> kit;
   TR_RegionStructure *kernel;

   for (TR_SPMDScopeInfo *scopeInfo = scopeit.getFirst(); scopeInfo; scopeInfo = scopeit.getNext())
      {
      switch(scopeInfo->getScopeType())
         {
         case scopeNaturalLoop:
            kit.set(scopeInfo->getKernelList());
            for (kernel = kit.getFirst(); kernel; kernel = kit.getNext())
               {
               TR_ASSERT(kernel->getPrimaryInductionVariable(), "forEach loop should have PIV");
               processGPULoop(kernel, scopeInfo);
               }
            break;
         case scopeSingleKernel:
            kernel = scopeInfo->getSingleKernelRegion();
            TR_ASSERT(kernel->getPrimaryInductionVariable(), "forEach loop should have PIV");
            processGPULoop(kernel, scopeInfo);
            break;
         }
      }

   // vectorize after GPU transformations
   TR_HashId id = 0;
   ListIterator<TR_RegionStructure> sit(&simdLoops);
   for (TR_RegionStructure *loop = sit.getFirst(); loop; loop = sit.getNext())
      {
      /*
       * The GPU transformation might make the block we are trying to vectorize unreachable due
       * to creating and using a new GPU path. We check for this case by checking if the loop's
       * parent is NULL or not. If it is NULL, vectorization of the unreachable block is not
       * necessary and can be skipped.
       */
      if (loop->getPrimaryInductionVariable() && (NULL != loop->getParent()))
         {
         if (reductionOperationsHashTab->locate(loop, id))
            {
            TR_HashTab* reductionHashTab = (TR_HashTab*)reductionOperationsHashTab->getData(id);
            vectorize(comp(), loop, loop->getPrimaryInductionVariable(), reductionHashTab, 0, optimizer());
            }
         }
      }

   return 1;
   }

const char *
TR_SPMDKernelParallelizer::optDetailString() const throw()
   {
   return "O^O SPMD KERNEL PARALLELIZER: ";
   }

void
TR_SPMDKernelParallelizer::collectGPUKernels(TR_RegionStructure *region,
                                                List<TR_RegionStructure> &gpuKernels)
   {
   if (isParallelForEachLoop(region, comp()))
      {
      gpuKernels.add(region);
      }

   TR_RegionStructure::Cursor it(*region);
   for (TR_StructureSubGraphNode *node = it.getFirst(); node; node = it.getNext())
      {
      if (node->getStructure()->asRegion())
         collectGPUKernels(node->getStructure()->asRegion(), gpuKernels);
      }
   }


bool TR_SPMDKernelParallelizer::visitCPUNode(TR::Node *node, int32_t visitCount, TR::Block *block, TR_BitVector *flushGPUBlocks)
   {
   if (node->getVisitCount() == visitCount) return true;

   node->setVisitCount(visitCount);

   TR::ILOpCode opcode = node->getOpCode();

   if ((opcode.isLoadVarOrStore() && opcode.isIndirect() &&
       node->getSymbolReference()->getSymbol()->isArrayShadowSymbol()) ||
       node->getOpCodeValue() == TR::arraycopy ||
       opcode.isCall())
      {
      traceMsg(comp(), "Found %s in non-cold CPU node %p\n", opcode.isCall() ? "a call" : "array access", node);

      TR_ResolvedMethod  *method;

      if (node->getInlinedSiteIndex() != -1)
         method = comp()->getInlinedResolvedMethod(node->getInlinedSiteIndex());
      else
         method = comp()->getCurrentMethod();

      while (method)
         {
         if (method->getRecognizedMethod() == TR::java_util_stream_IntPipeline_forEach ||
             method->getRecognizedMethod() == TR::java_util_stream_IntPipelineHead_forEach) break;
         method = method->owningMethod();
         }

      if (method)
         {
         traceMsg(comp(), "inside IntPipeline%s.forEach\n",
             method->getRecognizedMethod() == TR::java_util_stream_IntPipelineHead_forEach ? "$Head" : "");

         traceMsg(comp(), "need to insert flush\n");
         flushGPUBlocks->set(block->getNumber());
         }
      else if (opcode.isCall())
         {
         if (!node->getSymbolReference() ||
             !node->getSymbolReference()->getSymbol() ||
             !node->getSymbolReference()->getSymbol()->castToMethodSymbol() ||
             !node->getSymbolReference()->getSymbol()->castToMethodSymbol()->getMethod())
            {
            traceMsg(comp(), "can't hoist due to a call\n");
            return false;
            }

         TR_Method * method = node->getSymbolReference()->getSymbol()->castToMethodSymbol()->getMethod();
         const char * signature = method->signature(comp()->trMemory(), stackAlloc);

         traceMsg(comp(), "signature: %s\n", signature ? signature : "NULL");

         if (!(signature && strlen(signature) >= 10 && strncmp(signature, "java/lang/", 10) == 0) &&
             !(signature && strlen(signature) >= 10 && strncmp(signature, "java/util/", 10) == 0))
            {
            traceMsg(comp(), "can't hoist due to a call\n");
            return false;
            }
         }
      else
         {
         traceMsg(comp(), "can't hoist due do array access\n");
         return false;
         }
      }

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      if (!visitCPUNode(node->getChild(i), visitCount, block, flushGPUBlocks))
         return false;
      }

   return true;
   }

void
TR_SPMDKernelParallelizer::calculateNonColdCPUBlocks(TR_RegionStructure *region,
                                                TR_ScratchList<TR::Block> *kernelBlocks,
                                                TR_ScratchList<TR::Block> *coldLoopBlocks,
                                                SharedSparseBitVector *nonColdCPUBlocksVector)
   {
   TR_ScratchList<TR::Block> allBlocks(trMemory());
   region->getBlocks(&allBlocks);

   SharedSparseBitVector allBlocksVector(comp()->allocator());
   ListIterator<TR::Block> abit(&allBlocks);

   for (TR::Block *nextBlock = abit.getCurrent(); nextBlock; nextBlock=abit.getNext())
      {
      allBlocksVector[nextBlock->getNumber()] = true;
      }

   SharedSparseBitVector kernelBlocksVector(comp()->allocator());
   ListIterator<TR::Block> kbit(kernelBlocks);

   for (TR::Block *nextBlock = kbit.getCurrent(); nextBlock; nextBlock=kbit.getNext())
      {
      kernelBlocksVector[nextBlock->getNumber()] = true;
      }

   SharedSparseBitVector coldBlocksVector(comp()->allocator());
   ListIterator<TR::Block> cbit(coldLoopBlocks);

   for (TR::Block *nextBlock = cbit.getCurrent(); nextBlock; nextBlock=cbit.getNext())
      {
      coldBlocksVector[nextBlock->getNumber()] = true;
      }

   *nonColdCPUBlocksVector = allBlocksVector;
   *nonColdCPUBlocksVector -= kernelBlocksVector;
   *nonColdCPUBlocksVector -= coldBlocksVector;
   }

void
TR_SPMDKernelParallelizer::collectColdLoops(TR_RegionStructure *region, List<TR_RegionStructure> &coldLoops)
   {
   if (isParallelForEachLoop(region, comp()))
      return;

   if (region->isNaturalLoop() &&
       region->getEntryBlock()->isCold())
      {
      coldLoops.add(region);
      }

   TR_RegionStructure::Cursor it(*region);
   for (TR_StructureSubGraphNode *node = it.getFirst(); node; node = it.getNext())
      {
      if (node->getStructure()->asRegion())
         collectColdLoops(node->getStructure()->asRegion(), coldLoops);
      }
   }


bool
TR_SPMDKernelParallelizer::analyzeGPUScope(TR_SPMDScopeInfo* pScopeInfo)
   {
   ListIterator<TR_RegionStructure> kit(pScopeInfo->getKernelList());
   TR_RegionStructure *kernel;
   TR_ScratchList<TR::Block> kernelBlocks(trMemory());

    for (kernel = kit.getFirst(); kernel; kernel = kit.getNext())
       {
       traceMsg(comp(), "GPU kernel: %d\n", kernel->getNumber());
       kernel->getBlocks(&kernelBlocks);
       }

   TR_RegionStructure::Cursor rit(*pScopeInfo->getEnvelopingLoop());

   for (TR_StructureSubGraphNode *node = rit.getFirst(); node; node = rit.getNext())
       {
       if (node->getStructure()->asRegion())
          collectColdLoops(node->getStructure()->asRegion(), *pScopeInfo->getColdLoops());
       }

   ListIterator<TR_RegionStructure> lit(pScopeInfo->getColdLoops());
   TR_ScratchList<TR::Block> coldLoopBlocks(trMemory());

   for (TR_RegionStructure *loop = lit.getFirst(); loop; loop = lit.getNext())
      {
      traceMsg(comp(), "cold loop: %d\n", loop->getNumber());
      loop->getBlocks(&coldLoopBlocks);
      }

   SharedSparseBitVector nonColdCPUBlocksVector(comp()->allocator());

   calculateNonColdCPUBlocks(pScopeInfo->getEnvelopingLoop(), &kernelBlocks, &coldLoopBlocks, &nonColdCPUBlocksVector);

   SharedSparseBitVector::Cursor nbit(nonColdCPUBlocksVector);

   int visitCount = comp()->getVisitCount();

   for (nbit.SetToFirstOne(); nbit.Valid(); nbit.SetToNextOne())
      {
      TR::Block *nextBlock = _origCfgBlocks[nbit];
      traceMsg(comp(), "non-cold CPU block %d\n", nextBlock->getNumber());

      for (TR::TreeTop *tt = nextBlock->getEntry() ; tt != nextBlock->getExit() ; tt = tt->getNextTreeTop())
         {
         if (!visitCPUNode(tt->getNode(), visitCount, nextBlock, pScopeInfo->getFlushGPUBlocks()))
            {
            return false;
            }
         }
      }

   lit.set(pScopeInfo->getColdLoops());

   for (TR_RegionStructure *loop = lit.getFirst(); loop; loop = lit.getNext())
      {
      TR::Block *block = findLoopInvariantBlock(comp(), loop);

      if (!block)
         block = loop->getEntryBlock(); // TODO: improve performance

      pScopeInfo->getFlushGPUBlocks()->set(block->getNumber());
      }

   return true;
   }

void
TR_SPMDKernelParallelizer::collectGPUScopes(TR_RegionStructure *region, List<TR_RegionStructure> &gpuKernels, List<TR_SPMDScopeInfo> &gpuScopes)
   {
   if (!comp()->getOptions()->getEnableGPU(TR_EnableGPU))  return;

   if (region->getEntryBlock()->isCold()) return;

   int trace = 0;
   if (comp()->getOptions()->getEnableGPU(TR_EnableGPUVerbose)) trace = 1;
   if (comp()->getOptions()->getEnableGPU(TR_EnableGPUDetails)) trace = 2;

   bool isRegionGPUScope = false;

#ifdef ENABLE_GPU
   int getGpuDeviceCount(TR::PersistentInfo * persistentInfo, int tracing);

   if (getGpuDeviceCount(comp()->getPersistentInfo(), trace) == 0) return;
#endif

   if (region->isNaturalLoop() && !comp()->getOptions()->getEnableGPU(TR_EnableGPUDisableTransferHoist))
      {
      gpuKernels.deleteAll();

      TR_RegionStructure::Cursor it(*region);
      for (TR_StructureSubGraphNode *node = it.getFirst(); node; node = it.getNext())
         {
         if (node->getStructure()->asRegion())
            collectGPUKernels(node->getStructure()->asRegion(), gpuKernels);
         }

      if (gpuKernels.getSize() > 0)
         {
         TR_SPMDScopeInfo* pScopeInfo = new (comp()->trStackMemory()) TR_SPMDScopeInfo(comp(),region,scopeNaturalLoop);
         pScopeInfo->setKernelList(gpuKernels);

         traceMsg(comp(), "Found GPU scope %d in %s (natural loop type) with kernels:\n", region->getNumber(), comp()->signature());

         if (analyzeGPUScope(pScopeInfo))
            {
            isRegionGPUScope = true;
            gpuScopes.add(pScopeInfo);
            }
         else
            {
            traceMsg(comp(), "Discarding GPU scope due to negative analysis\n");
            }
         }
      }

   if (!isRegionGPUScope)
      if (isParallelForEachLoop(region,comp()))
         {
         TR_SPMDScopeInfo* pScopeInfo = new (comp()->trStackMemory()) TR_SPMDScopeInfo(comp(),region,scopeSingleKernel);
         gpuScopes.add(pScopeInfo);
         if (trace == 2)
            TR_VerboseLog::writeLine(TR_Vlog_GPU, "Found GPU scope %d in %s (single kernel type)", region->getNumber(), comp()->signature());
         }

   if (!isRegionGPUScope)
      {
      TR_RegionStructure::Cursor it(*region);
      for (TR_StructureSubGraphNode *node = it.getFirst(); node; node = it.getNext())
         {
         if (node->getStructure()->asRegion())
            collectGPUScopes(node->getStructure()->asRegion(), gpuKernels, gpuScopes);
         }
      }
   }

bool TR_SPMDKernelParallelizer::isSPMDKernelLoop(TR_RegionStructure *region, TR::Compilation *comp)
   {
   if (region->isNaturalLoop() &&
       region->getPrimaryInductionVariable())
      {
      if (!SPMDPreCheck::isSPMDCandidate(comp, region))
         {
         if (trace())
            traceMsg(comp, "Natural loop %d has failed SPMD pre-check - skipping consideration\n", region->getNumber());
         return false;
         }

      TR_PrimaryInductionVariable *piv = region->getPrimaryInductionVariable();
      TR::Block *branchBlock = piv->getBranchBlock();
      TR::Node *node = branchBlock->getLastRealTreeTop()->getNode();
      TR_ResolvedMethod  *method;

      if (node->getInlinedSiteIndex() != -1)
         method = comp->getInlinedResolvedMethod(node->getInlinedSiteIndex());
      else
         method = comp->getCurrentMethod();

#if defined(ENABLE_SPMD_SIMD)
      if (method->getRecognizedMethod() == com_ibm_simt_SPMDKernel_execute)

         {
         traceMsg(comp, "identified SPMD kernel execution loop in %s block = %d stride = %d symbol = %d\n", comp->signature(), branchBlock->getNumber(), piv->getDeltaOnBackEdge(), piv->getSymRef()->getReferenceNumber());//method->nameChars());
         return true;
         }
#endif
      }
   return false;
   }

bool TR_SPMDKernelParallelizer::isParallelForEachLoop(TR_RegionStructure *region, TR::Compilation *comp)
   {
   TR_PrimaryInductionVariable *piv = region->getPrimaryInductionVariable();

   if (region->isNaturalLoop() && piv)
      {
      TR::Block *entryBlock = region->getEntryBlock();
      TR::Block *branchBlock = piv->getBranchBlock();
      TR::Node *node = branchBlock->getLastRealTreeTop()->getNode();
      TR_ResolvedMethod  *method;

      if (node->getInlinedSiteIndex() != -1)
         method = comp->getInlinedResolvedMethod(node->getInlinedSiteIndex());
      else
         method = comp->getCurrentMethod();

      if (method->getRecognizedMethod() == TR::java_util_stream_IntPipeline_forEach)
         {
         traceMsg(comp, "Found forEach loop %d in %s piv=%s\n", entryBlock->getNumber(), comp->signature(), piv ? "yes" : "no");

         if (comp->getOptions()->getEnableGPU(TR_EnableGPUDetails))
            TR_VerboseLog::writeLine(TR_Vlog_GPU, "Found forEach in %s", comp->signature());

         int32_t inc = piv->getDeltaOnBackEdge();
         traceMsg(comp, "branchBlock = %d inc = %d piv = %d\n", branchBlock->getNumber(), inc , piv->getSymRef()->getReferenceNumber());

         if (inc == 1) return true;

         }
      }
   return false;
   }

bool TR_SPMDKernelParallelizer::isPerfectNest(TR_RegionStructure *region, TR::Compilation *comp)
   {
   if (region->isNaturalLoop() &&
       region->getPrimaryInductionVariable())
      {
      _pivList[_pivList.NumberOfElements()] = region->getPrimaryInductionVariable();
      TR_RegionStructure::Cursor it(*region);
      bool hasInnerLoop = false;

      TR_PrimaryInductionVariable *piv = region->getPrimaryInductionVariable();
      TR::Block *branchBlock = piv->getBranchBlock();

      for (TR_StructureSubGraphNode *node = it.getFirst(); node; node = it.getNext())
         {
         if (node->getStructure()->asBlock())
            {
            // no branches for now, except for the backedge
            TR::Block *block = node->getStructure()->asBlock()->getBlock();
            if (block != branchBlock && block->getSuccessors().size() > 1)
               return false;
            }
         else if (hasInnerLoop)   // only one inner loop
            return false;
         else if (node->getStructure()->asRegion() && isPerfectNest(node->getStructure()->asRegion(),comp))
            hasInnerLoop = true;
         else
            return false;
         }
      return true;
      }
   return false;
   }

bool TR_SPMDKernelParallelizer::checkDataLocality(TR_RegionStructure *loop, CS2::ArrayOf<TR::Node *, TR::Allocator> &useNodesOfDefsInLoop, SharedSparseBitVector &defsInLoop, TR::Compilation *comp, TR_UseDefInfo *useDefInfo, TR_HashTab* reductionHashTab)
   {
   traceMsg(comp, "Checking data locality in loop %d piv = %d\n", loop->getNumber(), loop->getPrimaryInductionVariable()->getSymRef()->getReferenceNumber());

   for (int32_t idx = 0; idx < _pivList.NumberOfElements(); idx++)
      {
      traceMsg(comp, "   iv = %d\n", _pivList[idx]->getSymRef()->getReferenceNumber());
      }

   setLoopDataType(loop,comp);

   _visitedNodes.empty();

   loop->resetInvariance();
   loop->computeInvariantExpressions();

   TR_SPMDKernelInfo *pSPMDInfo = new (comp->trStackMemory()) TR_SPMDKernelInfo(comp, loop->getPrimaryInductionVariable());

   TR_ScratchList<TR::Block> blocksInLoopList(comp->trMemory());
   loop->getBlocks(&blocksInLoopList);
   ListIterator<TR::Block> blocksIt(&blocksInLoopList);

   SharedSparseBitVector usesOfDefsInLoop(comp->allocator());
   SharedSparseBitVector usesInLoop(comp->allocator());

   for (TR::Block *nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      {
      for (TR::TreeTop *tt = nextBlock->getEntry() ; tt != nextBlock->getExit() ; tt = tt->getNextTreeTop())
         {
         if (!visitTreeTopToSIMDize(tt, pSPMDInfo, true, loop, useNodesOfDefsInLoop, comp, useDefInfo, defsInLoop, &usesInLoop, reductionHashTab))
            return false;
         }
       }

   CS2::ArrayOf<TR::Node *, TR::Allocator>::Cursor uc(useNodesOfDefsInLoop);
   for (uc.SetToFirst(); uc.Valid(); uc.SetToNext())
      {
      usesOfDefsInLoop[useNodesOfDefsInLoop[uc]->getGlobalIndex()]=true;
      }

   usesOfDefsInLoop.Andc(usesInLoop);
   if (!usesOfDefsInLoop.IsZero())
      {
      traceMsg(comp, "   loop defines temps that are used outside: ");
      (*comp) << usesOfDefsInLoop << "\n";
      return false;
      }

   return true;
   }

bool TR_SPMDKernelParallelizer::checkLoopIteration(TR_RegionStructure *loop,TR::Compilation *comp)
   {
   TR_PrimaryInductionVariable * piv = loop->getPrimaryInductionVariable();
   TR::Block *branchBlock = piv->getBranchBlock();
   TR::Node *branch = branchBlock->getLastRealTreeTop()->getNode();

   if (!TR::ILOpCode::isLessCmp(branch->getOpCodeValue())) //SIMD_TODO add support for it. Unroller already has it.
      return false;

   bool goodLoopBounds= false;
   traceMsg(comp,"checking loop iteration pattern on loop %d \n",loop->getNumber());
   for (TR::TreeTop *tt = branchBlock->getEntry() ; tt != branchBlock->getExit() ; tt = tt->getNextTreeTop())
      {
      TR::Node *storeNode = tt->getNode();
      if (storeNode->getOpCodeValue() == TR::istore &&
          storeNode->getSymbolReference() == piv->getSymRef() &&
          piv->getIncrement() == 1 &&
          storeNode->getFirstChild()->getOpCodeValue() == TR::isub &&
          (storeNode->getFirstChild() == branch->getFirstChild() ||
          storeNode->getFirstChild() == branch->getSecondChild()))
         {
         TR::Node *indexChild = storeNode->getFirstChild()->getFirstChild();
         TR::Node *incrementNode = storeNode->getFirstChild()->getSecondChild();

         if (indexChild->getOpCode().isLoadVarDirect() &&
             indexChild->getSymbolReference() == piv->getSymRef() &&
             incrementNode->getOpCode().isLoadConst() &&
             incrementNode->getInt() == -1)
            goodLoopBounds = true;
         }
      }

   return goodLoopBounds;
   }

bool TR_SPMDKernelParallelizer::areNodesEquivalent(TR::Compilation *comp, TR::Node *node1, TR::Node *node2)
   {
   if(!node1 && !node2)
      return true;

   if(!node1 || !node2)
      return false;

   TR_ValueNumberInfo *valueNumberInfo = optimizer()->getValueNumberInfo();

   return valueNumberInfo->getValueNumber(node1) == valueNumberInfo->getValueNumber(node2);

   }

void TR_SPMDKernelParallelizer::collectDefsAndUses(TR::Node *node, vcount_t visitCount, CS2::ArrayOf<TR::Node *, TR::Allocator> &defs, CS2::ArrayOf<TR::Node *, TR::Allocator> &uses, TR::Compilation *comp)
   {
   if (node->getVisitCount() >= visitCount)
      return;
   node->setVisitCount(visitCount);

   if (node->getOpCode().isLikeDef() &&
       !node->getOpCode().isStoreDirect() &&
       node->getOpCodeValue() != TR::asynccheck)
      defs[defs.NumberOfElements()] = node;
   else if (node->getOpCode().isLikeUse() &&
       !node->getOpCode().isLoadDirect())
      uses[uses.NumberOfElements()] = node;

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      collectDefsAndUses(node->getChild(i), visitCount, defs, uses, comp);
   }

int TR_SPMDKernelParallelizer::symbolicEvaluateTree(TR::Node *node)
   {
   int c1 = 0;
   int c2 = 0;

   switch(node->getNumChildren())
      {
      case 0:
         if (node->getOpCodeValue() == TR::lconst || node->getOpCodeValue() == TR::iconst) return node->getInt();
         return 0; //else
      case 1:
         // only ilop that will do this is TR::i2l 
         return symbolicEvaluateTree(node->getFirstChild());
      case 2:
         c1 = symbolicEvaluateTree(node->getFirstChild());
         c2 = symbolicEvaluateTree(node->getSecondChild());
      }

   switch(node->getOpCodeValue()) // support either add, sub or mul
      {
      case TR::imul: 
      case TR::lmul: 
         return (c1*c2);
      case TR::iadd:
      case TR::ladd:
         return (c1+c2);
      case TR::isub:
      case TR::lsub:
         return (c1-c2);
      }

   return 0;
   }

TR::Node* TR_SPMDKernelParallelizer::findSingleLoopVariant(TR::Node *node, TR_RegionStructure *loop, int* sign, int* constantsOnly)
   {
   TR::Node* node1 = NULL;
   TR::Node* node2 = NULL;
   if (node->getNumChildren() >= 1) node1 = findSingleLoopVariant(node->getFirstChild(), loop, sign, constantsOnly);
   if (node->getNumChildren() == 2) 
      {
      *sign = (*sign)^(node->getOpCode().isSub());
      node2 = findSingleLoopVariant(node->getSecondChild(), loop, sign, constantsOnly);
      if (node2 == NULL && node1 == NULL) *sign = (*sign)^(node->getOpCode().isSub());
      }

   if (*constantsOnly == 1 || (node->getNumChildren() == 0 && node->getOpCodeValue() != TR::lconst && node->getOpCodeValue() != TR::iconst && loop->isExprInvariant(node)))
      {
      *constantsOnly = 1;
      }

   if (node->getNumChildren() == 0 && !loop->isExprInvariant(node)) return node; 
   if (node->getNumChildren() == 0 && loop->isExprInvariant(node)) return NULL; 
   if (node1 != NULL && node2 != NULL) return NULL;
   if (node1 == NULL && node2 == NULL) return NULL;
   if (node1 != NULL && node2 == NULL) return node1;
   if (node1 == NULL && node2 != NULL) return node2;

   return NULL;
   }

// type 0 - node1 - def, node2 - use
// type 1 - node1 - def, node2 - def
bool TR_SPMDKernelParallelizer::checkConstantDistanceDependence(TR_RegionStructure *loop, TR::Node *node1, TR::Node *node2, TR::Compilation *comp, int type)
   {
   // do checks for constant distance dependence analysis
   // need to ensure first children of address calculations are invariants and the same
   if (!(loop->isExprInvariant(node1->getFirstChild()->getFirstChild()) &&
       loop->isExprInvariant(node2->getFirstChild()->getFirstChild()) &&
       areNodesEquivalent(comp,node1->getFirstChild()->getFirstChild(),node2->getFirstChild()->getFirstChild())))
      {
      return false;
      }
 
   TR::Node* invariantNodedefs; 
   TR::Node* invariantNodeuses; 
   int sign1 = 0;
   int sign2 = 0;
   int constantsOnly1 = 0;
   int constantsOnly2 = 0;

   // traverse use and def - ensure there loop variant nodes are the same
   // and there is only one loop variant node - the induction variable
   // and ensure the signs on the loop variant nodes is correct
   invariantNodedefs = findSingleLoopVariant(node1->getFirstChild()->getSecondChild(), loop, &sign1, &constantsOnly1);
   invariantNodeuses = findSingleLoopVariant(node2->getFirstChild()->getSecondChild(), loop, &sign2, &constantsOnly2);
   if (!areNodesEquivalent(comp,invariantNodedefs,invariantNodeuses) && 
       sign1 == sign2) 
      {
      return false;
      }

   if (constantsOnly1 != 0 | constantsOnly2 != 0)
      {
      // there are loop invariant parameters in the distance - need to generate
      // a runtime check
      traceMsg(comp, "SPMD DEPENDENCE ANALYSIS: def %p and %s %p distance depends on parameters\n", node1, type == 0 ? "use" : "def", node2);
      return false;
      }
   else
      {
      // there are no loop invariant parameters in the distance - evaluate symbolically
      int s1 = symbolicEvaluateTree(node1->getFirstChild()->getSecondChild());
      int s2 = symbolicEvaluateTree(node2->getFirstChild()->getSecondChild());
      int distance = (s1-s2);
      traceMsg(comp, "SPMD DEPENDENCE ANALYSIS: def %p, %s %p. Constant distance dependence of %d bytes\n", node1, type == 0 ? "use" : "def", node2, distance);

      if (type == 0 && (distance >= VECTOR_SIZE || distance <= 0))
         { // permissable flow dependence
         return true; 
         }
      if (type == 1 && (distance >= 0 || distance <= -VECTOR_SIZE))
         { // permissable output dependence
         return true;
         }
      return false;
      }

      return false;
   }

bool TR_SPMDKernelParallelizer::checkIndependence(TR_RegionStructure *loop, TR_UseDefInfo *useDefInfo, CS2::ArrayOf<TR::Node *, TR::Allocator> &useNodesOfDefsInLoop, SharedSparseBitVector &defsInLoop, TR::Compilation *comp)
   {
   traceMsg(comp, "Checking independence in loop %d piv = %d\n", loop->getNumber(), loop->getPrimaryInductionVariable()->getSymRef()->getReferenceNumber());
   CS2::ArrayOf<TR::Node *, TR::Allocator> defs(comp->allocator(), NULL);
   CS2::ArrayOf<TR::Node *, TR::Allocator> uses(comp->allocator(), NULL);

   int visitCount = comp->incVisitCount();

   TR_ScratchList<TR::Block> blocksInLoop(comp->trMemory());
   loop->getBlocks(&blocksInLoop);
   ListIterator<TR::Block> blocksIt(&blocksInLoop);
   TR::Block *nextBlock;
   for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      {
      for (TR::TreeTop *tt = nextBlock->getEntry() ; tt != nextBlock->getExit() ; tt = tt->getNextTreeTop())
         {
         TR::Node *node = tt->getNode();
         collectDefsAndUses(node, visitCount, defs, uses, comp);
         }
       }

   CS2::ArrayOf<TR::Node *, TR::Allocator>::Cursor dc(defs);
   for (dc.SetToFirst(); dc.Valid(); dc.SetToNext())
      {
      CS2::ArrayOf<TR::Node *, TR::Allocator>::Cursor dc2(defs);
      for (dc2.SetTo(dc+1); dc2.Valid(); dc2.SetToNext())
         {
         if (defs[dc2]->getOpCode().hasSymbolReference() &&
             defs[dc]->mayKill().contains(defs[dc2]->getSymbolReference(), comp))
            {
            traceMsg(comp, "SPMD DEPENDENCE ANALYSIS: Testing (def %p, def %p) for dependence\n", defs[dc], defs[dc2]);
            if (!loop->isExprInvariant(defs[dc]->getFirstChild()) &&
                !loop->isExprInvariant(defs[dc2]->getFirstChild()) &&
                areNodesEquivalent(comp,defs[dc]->getFirstChild(), defs[dc2]->getFirstChild()))
               {
               continue;
               }

            if (checkConstantDistanceDependence(loop,defs[dc],defs[dc2],comp,1))
               { 
               continue;
               }

            traceMsg(comp, "SPMD DEPENDENCE ANALYSIS: def %p and def %p are dependent\n", defs[dc], defs[dc2]);
            traceMsg(comp, "SPMD DEPENDENCE ANALYSIS: will not vectorize\n"); 
            return false;
            }
         }

      CS2::ArrayOf<TR::Node *, TR::Allocator>::Cursor uc(uses);
      for (uc.SetToFirst(); uc.Valid(); uc.SetToNext())
         {
         if (uses[uc]->getOpCode().hasSymbolReference() &&
             defs[dc]->mayKill().contains(uses[uc]->getSymbolReference(), comp))
            {
            traceMsg(comp, "SPMD DEPENDENCE ANALYSIS: Testing (def %p, use %p) for dependence\n", defs[dc], uses[uc]);
            if (!loop->isExprInvariant(defs[dc]->getFirstChild()) &&
                !loop->isExprInvariant(uses[uc]->getFirstChild()) &&
                areNodesEquivalent(comp,defs[dc]->getFirstChild(),uses[uc]->getFirstChild()))
               {
               continue;
               }
            
            if (checkConstantDistanceDependence(loop,defs[dc],uses[uc],comp,0))
               { 
               continue;
               }

            traceMsg(comp, "SPMD DEPENDENCE ANALYSIS: def %p and use %p are dependent\n", defs[dc], uses[uc]);
            traceMsg(comp, "SPMD DEPENDENCE ANALYSIS: will not vectorize\n"); 
            return false;
            }
         }
      }

   SharedSparseBitVector defsOfDefsInLoop(comp->allocator());
   for (int32_t useIndex = 0; useIndex < useNodesOfDefsInLoop.NumberOfElements(); useIndex++)
      {
      TR::Node *useNode = useNodesOfDefsInLoop[useIndex];
      int32_t useDefIndex = useNode->getUseDefIndex();
      TR_UseDefInfo::BitVector defsOfThisUse(comp->allocator());
      useDefInfo->getUseDef(defsOfThisUse, useDefIndex);

      TR_UseDefInfo::BitVector::Cursor cursor(defsOfThisUse);

      for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
         {
         int32_t defIndex = (int32_t) cursor ;
         defsOfDefsInLoop[useDefInfo->getNode(defIndex)->getGlobalIndex()]=true;
         }

      }

   defsOfDefsInLoop.Andc(defsInLoop);
   if(!defsOfDefsInLoop.IsZero())
      {
      traceMsg(comp, "Both a definition from inside the loop and outside the loop reaches a temp used inside the loop.\n");
      return false;
      }

   return true;
   }

void
TR_SPMDKernelParallelizer::collectParallelLoops(TR_RegionStructure *region,
                                                List<TR_RegionStructure> &simdLoops,
                                                TR_HashTab* reductionOperationsHashTab,
                                                TR_UseDefInfo *useDefInfo)
   {
   if (region->getEntryBlock()->isCold()) return;

   CS2::ArrayOf<TR::Node *, TR::Allocator> useNodesOfDefsInLoop(comp()->allocator(), NULL);
   SharedSparseBitVector defsInLoop(comp()->allocator());
   TR_HashTab* reductionHashTab = new (comp()->trStackMemory()) TR_HashTab(comp()->trMemory(), stackAlloc);
   TR_HashId id = 0;

   if (isSPMDKernelLoop(region, comp()) ||
       (!comp()->getOption(TR_DisableAutoSIMD) &&
        comp()->cg()->getSupportsAutoSIMD() &&
        isPerfectNest(region, comp()) &&
        checkDataLocality(region, useNodesOfDefsInLoop, defsInLoop, comp(), useDefInfo, reductionHashTab) &&
        checkIndependence(region, useDefInfo, useNodesOfDefsInLoop, defsInLoop, comp()) &&
        checkLoopIteration(region,comp())))
      {
      traceMsg(comp(), "Loop %d and piv = %d collected for Auto-Vectorization\n", region->getNumber(), region->getPrimaryInductionVariable()->getSymRef()->getReferenceNumber());
      simdLoops.add(region);
      reductionOperationsHashTab->add(region, id, reductionHashTab);
      return;
      }

   TR_RegionStructure::Cursor it(*region);
   for (TR_StructureSubGraphNode *node = it.getFirst(); node; node = it.getNext())
      {
      if (node->getStructure()->asRegion())
         collectParallelLoops(node->getStructure()->asRegion(), simdLoops, reductionOperationsHashTab, useDefInfo);
      }
   }

bool
TR_SPMDKernelParallelizer::vectorize(TR::Compilation *comp, TR_RegionStructure *loop, TR_PrimaryInductionVariable *piv, TR_HashTab* reductionHashTab, int32_t peelCount, TR::Optimizer *optimizer)
   {
   /* copy of TR_LoopUnroller::unroll for counted loop */
   // We dont do peeling as of yet
   //
   if (peelCount != 0)
      {
      traceMsg(comp, "Cannot unroll loop %d: peeling not supported yet\n", loop->getNumber());
      return false;
      }

   TR::Block *loopInvariantBlock = NULL;
   if (!TR_LoopUnroller::isWellFormedLoop(loop, comp, loopInvariantBlock))
      {
      traceMsg(comp, "Cannot unroll loop %d: not a well formed loop\n", loop->getNumber());
      return false;
      }

   if (TR_LoopUnroller::isTransactionStartLoop(loop, comp))
      {
      traceMsg(comp, "Cannot unroll loop %d: it is a transaction start loop\n",loop->getNumber());
      return false;
      }

   bool vl = processSPMDKernelLoopForSIMDize(comp, optimizer, loop, piv, reductionHashTab, peelCount, loopInvariantBlock);
   return vl;
   }

