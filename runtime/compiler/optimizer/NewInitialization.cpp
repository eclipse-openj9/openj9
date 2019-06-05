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

#include "optimizer/NewInitialization.hpp"

#include <stdint.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"
#include "env/TRMemory.hpp"
#include "env/VMJ9.h"
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
#include "il/symbol/ParameterSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "optimizer/CallInfo.hpp"
#include "optimizer/Inliner.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/TransformUtil.hpp"
#include "optimizer/ValueNumberInfo.hpp"
#include "ras/Debug.hpp"

class TR_OpaqueClassBlock;
namespace TR { class MethodSymbol; }

#define OPT_DETAILS "O^O EXPLICIT NEW INITIALIZATION: "

TR_NewInitialization::TR_NewInitialization(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}

TR_LocalNewInitialization::TR_LocalNewInitialization(TR::OptimizationManager *manager)
   : TR_NewInitialization(manager)
   {}

int32_t TR_LocalNewInitialization::perform()
   {
   return performAnalysis(false);
   }

const char *
TR_LocalNewInitialization::optDetailString() const throw()
   {
   return "O^O EXPLICIT NEW INITIALIZATION: ";
   }

int32_t TR_NewInitialization::performAnalysis(bool doGlobalAnalysis)
   {
   // AOT does not support TR_New optimizations at this time
   if(comp()->getOption(TR_AOT))
      return 0;

   //
   // 64 bit platforms are not doing correct thing till the change
   // to make int fields 4 bytes in size in a J9 object is made at the
   // FE side. Remember to allow new init opt if the target is 64 bit.
   //
   if (TR::Compiler->target.is64Bit() && !comp()->useCompressedPointers())
      return 0;

   // When TLH is batch cleared, explicit initialization should be disabled.
   if (comp()->fej9()->tlhHasBeenCleared())
      return 0;

   // Debug option to only do new initialization for methods that don't have
   // the "quiet" option set in the limit file
   //
   static char *nonQuiet = feGetEnv("TR_NonQuietNew");
   if (nonQuiet && comp()->getOutFile() == NULL)
      return 0;

   if (trace())
      traceMsg(comp(), "Starting Explicit Initialization for New\n");

   TR_Hotness methodHotness = comp()->getMethodHotness();

   // Don't merge allocations if we are generating JVMPI hooks, since
   // JVMPI needs to know about each allocation.
   //
   _allowMerge = cg()->getSupportsMergedAllocations() &&
                 !comp()->getOption(TR_DisableMergeNew) &&
                 !comp()->suppressAllocationInlining() &&
                 !comp()->compileRelocatableCode() &&
                 !comp()->getOptions()->realTimeGC();
   if (_allowMerge)
      {
      static const char *p = feGetEnv("TR_MergeNew");
      if (p)
         {
         if (*p == 's')
            _allowMerge = (methodHotness >= scorching);
         else if (*p == 'h')
            _allowMerge = (methodHotness >= hot);
         else if (*p >= '0' && *p <= '9')
            {
            static int32_t methodCount = 0;
            int32_t low = 0, high = 0;
            while (*p >= '0' && *p <= '9')
               {
               low = low * 10 + *p -'0';
               p++;
               }
            if (*p == '-')
               {
               p++;
               while (*p >= '0' && *p <= '9')
                  {
                  high = high * 10 + *p -'0';
                  p++;
                  }
               }
            else
               high = low;
            _allowMerge = (methodCount >= low && methodCount <= high);
            methodCount++;
            }
         }
      else
         {
         // Default setting
         //
         _allowMerge = (methodHotness >= scorching);
         }
      }

   static const char *q = feGetEnv("TR_Sniff");
   _sniffConstructorsOnly = false;
   _sniffCalls = false;
   if (q)
      {
      if (*q == 's')
         _sniffCalls = _allowMerge && methodHotness >= scorching;
      else if (*q == 'h')
         _sniffCalls = _allowMerge && methodHotness >= hot;
      else if (*q == 'n')
         _sniffCalls = false;
      else if (*q == 'c')
         {
         _sniffCalls = true;
         _sniffConstructorsOnly = true;
         }
      else
         _sniffCalls = true;
      }

   // Default setting
   //
   else if (_allowMerge && methodHotness >= scorching)
      {
      _sniffCalls = true;
      _sniffConstructorsOnly = true;
      }

   int32_t nodeCount = 0;
   if (_sniffCalls)
      {
      // Find the current number of nodes so that we can limit the size that
      // inlining grows to.
      //
      vcount_t visitCount = comp()->incVisitCount();
      TR::TreeTop *tt = comp()->getStartTree();
      for (; tt; tt = tt->getNextTreeTop())
         nodeCount += tt->getNode()->countNumberOfNodesInSubtree(visitCount);
      }

   _removeZeroStores  = true;

   if (methodHotness >= scorching)
      {
      _maxIterations               = 10;
      _maxInlinedBytecodeSize      = 600;
      _maxTotalInlinedBytecodeSize = 6000 - nodeCount;
      }
   else if (methodHotness >= hot)
      {
      _maxIterations               = 5;
      _maxInlinedBytecodeSize      = 400;
      _maxTotalInlinedBytecodeSize = 3000 - nodeCount;
      }
   else
      {
      _maxIterations               = 3;
      _maxInlinedBytecodeSize      = 200;
      _maxTotalInlinedBytecodeSize = 1000 - nodeCount;
      }
   _totalInlinedBytecodeSize    = 0;

   _invalidateUseDefInfo        = false;
   int32_t cost                 = 0;
   int32_t iteration            = 0;

   bool doItAgain = true;
   while (doItAgain)
      {
      cost++;
      doItAgain = doAnalysisOnce(iteration);
      if (iteration++ == _maxIterations)
         _sniffCalls = false;
      }

   // If any stores were introduced, invalidate value number info and
   // use/def info.
   //
   if (_invalidateUseDefInfo)
      {
      optimizer()->setValueNumberInfo(NULL);
      optimizer()->setUseDefInfo(NULL);
      }

   return cost;
   }


bool TR_NewInitialization::doAnalysisOnce(int32_t iteration)
   {
   if (trace())
      traceMsg(comp(), "\nStarting iteration %d\n", iteration);

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   // Get block frequencies if available
   //
   //if (comp()->getMethodHotness() == scorching)
   //   comp()->getFlowGraph()->setFrequencies();

   _inlinedCallSites.setFirst(NULL);

   // First walk the trees to find allocation nodes, initializations, and merge
   // opportunities
   //
   findNewCandidates();

   // Second, either:
   //    1) inline the top-level methods that we deemed useful to inline,
   //       and ask for another iteration, or
   //    2) change the allocations that can reasonably be changed into
   //       no-zero-init versions or merged versions, and insert the necessary
   //       zero-initialization information.
   //
   bool doItAgain = changeNewCandidates();
   return doItAgain;
   }

void TR_NewInitialization::findNewCandidates()
   {
   // Walk the trees to find allocation nodes that may be inlined and/or merged,
   // and explicit initializations.
   //
   _candidates.set(NULL, NULL);
   _outermostCallSite = NULL;
   _parms             = NULL;

   int32_t savedTotalInlinedBytecodeSize = _totalInlinedBytecodeSize;

   comp()->incVisitCount();

   if (trace())
      traceMsg(comp(), "\n\nFinding candidates\n\n");

   TR::CFG *cfg = comp()->getFlowGraph();
   bool saveAllowMerge = _allowMerge;
   bool saveSniffCalls = _sniffCalls;

   TR::TreeTop *treeTop;
   for (treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      TR_ASSERT(node->getOpCodeValue() == TR::BBStart, "Expected start of block");
      TR::Block *block = node->getBlock();
      _firstActiveCandidate = NULL;
      _firstMergeCandidate = NULL;

      // If this is a cold block, don't bother inlining or merging
      //
      //TR_Hotness hotness = block->getHotness(cfg);
      //if (hotness != unknownHotness && hotness < hot)
      if ((block->getFrequency() >= 0) &&
          (block->getFrequency() <= MAX_WARM_BLOCK_COUNT))
         {
         _allowMerge = false;
         _sniffCalls = false;
         }

      findNewCandidatesInBlock(treeTop, block->getExit());

      treeTop = block->getExit();
      escapeToUserCodeAllCandidates(treeTop->getNode());
      setGCPoint(treeTop); // This is a GC point (at least, it may be)

      _allowMerge = saveAllowMerge;
      _sniffCalls = saveSniffCalls;
      }

   findUninitializedWords();

   _totalInlinedBytecodeSize = savedTotalInlinedBytecodeSize;
   }

bool TR_NewInitialization::findNewCandidatesInBlock(TR::TreeTop *startTree, TR::TreeTop *endTree)
   {
   // Walk the trees for a block to find allocation nodes that may be inlined
   // and/or merged. Sniff into initializer calls to see if they do any
   // allocations or initializations.
   //
   vcount_t visitCount = comp()->getVisitCount();

   TR::TreeTop *treeTop;
   for (treeTop = startTree; treeTop != endTree; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      if (node->getVisitCount() == visitCount)
         continue;

      // We will look at the child only if it hasn't been seen before.
      // However, calling "visitNode" will mark it as having been seen so we
      // must remember if it has been seen before calling "visitNode".
      //
      TR::Node *child = NULL;
      if (node->getNumChildren() && node->getFirstChild()->getVisitCount() != visitCount)
         child = node->getFirstChild();

      if (visitNode(node))
         {
         // Remove this treetop
         //
         if (performTransformation(comp(), "%s Removing zero initialization at [%p]\n", OPT_DETAILS, node))
            {
            TR::TreeTop *prev = treeTop->getPrevTreeTop();
            TR::TransformUtil::removeTree(comp(), treeTop);
            treeTop = prev;
            _invalidateUseDefInfo = true;
            }
         continue;
         }

      if (node->getNumChildren() == 0)
         continue;

      // See if this is a candidate allocation node
      //
      Candidate *prevCandidate = _candidates.getLast();
      if (findAllocationNode(treeTop, child))
         {
         // See if this candidate can be merged with others
         //
         TR_OpaqueClassBlock *classInfo;
         Candidate *candidate = _candidates.getLast();
         if (_allowMerge &&
             (treeTop->getNode()->getOpCodeValue() != TR::MergeNew) &&
             comp()->canAllocateInline(candidate->node, classInfo) > 0)
            {
            if (_firstMergeCandidate)
               {
               if (_firstMergeCandidate->GCPointFoundBeforeNextCandidate)
                  {
                  // This candidate's reference slots must be initialized at the
                  // allocation point, since they must be valid at the GC point
                  //
                  escapeToGC(candidate, child);
                  }
               setAffectedCandidate(_firstMergeCandidate);
               candidate->canBeMerged = true;
               if (trace())
                  traceMsg(comp(), "Candidate [%p] can be merged\n", candidate->node);
               }
            else
               {
               if (!candidate->isInSniffedMethod)
                  {
                  _firstMergeCandidate = candidate;
                  candidate->firstMergeCandidate = true;
                  candidate->canBeMerged = true;
                  if (trace())
                     traceMsg(comp(), "Candidate [%p] can be merged\n", candidate->node);
                  }
               }
            }

         else
            {
            // If this allocation node can't be merged with others, any
            // previous ones must all have their reference fields
            // completely initialized here by this point.
            // Take care not to mark the candidate just found.
            //
            if (prevCandidate)
               {
               prevCandidate->setNext(NULL);
               escapeToGC(child);
               prevCandidate->setNext(candidate);
               }

            setGCPoint(treeTop); // This is a GC point
            }

         // Activate the candidate
         //
         if (!_firstActiveCandidate)
            _firstActiveCandidate = candidate;
         if (trace())
            {
            traceMsg(comp(), "   Active candidates are now [%p]-[%p]\n",_firstActiveCandidate->node, candidate->node);
            }
         continue;
         }

      // Skip "TR::treetop" node since it may only be causing GC because of its
      // child which may have already been seen
      //
      if (node->getOpCodeValue() == TR::treetop)
         node = child;

      if (node && node->canCauseGC())
         {
         // See if this call is a candidate for inlining - if so, sniff it
         // to see if it would be useful to inline.
         //
         if (child &&
             ((child->getOpCodeValue() == TR::arraycopy) ||
              (child->getOpCodeValue() == TR::arrayset)))
            {
            escapeViaArrayCopyOrArraySet(child);
            }
         else if (child && child->getOpCode().isCall())
            {
            if (_sniffCalls)
               {
               // If we didn't scan the whole of the method while sniffing and
               // the call site is in a sniffed method, stop sniffing this
               // method.
               //
               if (!sniffCall(treeTop))
                  {
                  escapeViaCall(child);
                  setGCPoint(treeTop); // This is a GC point
                  if (_outermostCallSite)
                     return false;
                  }

               // If we did scan the whole of the method while sniffing we can
               // continue looking at this block. If this is the outermost block
               // the sniffed method is significant to all the active candidates
               //
               else
                  {
                  if (!_outermostCallSite)
                     {
                     _outermostCallSite = treeTop;
                     for (Candidate *c = _firstActiveCandidate; c; c = c->getNext())
                        setAffectedCandidate(c);
                     _outermostCallSite = NULL;
                     }
                  }
               }
            else
               {
               // Candidates that are arguments to the call must be completely
               // initialized since they escape to the call.
               // Other active candidates must have their reference fields
               // initialized since a GC may happen during the call.
               //
               escapeViaCall(child);
               setGCPoint(treeTop); // This is a GC point
               }
            continue;
            }

         // Now all of the active candidates must have all their reference
         // slots marked as uninitialized.
         //
         escapeToGC(node);
         setGCPoint(treeTop, node); // This is a GC point
         }
      }

   return (endTree->getNextTreeTop() == NULL);
   }

bool TR_NewInitialization::findAllocationNode(TR::TreeTop *treeTop, TR::Node *node)
   {
   if (!node)
      return false;

   int32_t    size;
   bool       isArrayNew;
   bool       doubleSizeArray = false;

   if (node->getOpCodeValue() == TR::New)
      {
      TR::Node *classNode = node->getFirstChild();
      TR_ASSERT(classNode->getOpCodeValue() == TR::loadaddr, "Expected class load as child of TR::New");

      // We can't do anything if we can't find the size of the class
      //
      TR::SymbolReference *classSymRef = classNode->getSymbolReference();
      if (classSymRef->isUnresolved())
         return false;

      TR::StaticSymbol *classSym = classSymRef->getSymbol()->castToStaticSymbol();
      size = TR::Compiler->cls.classInstanceSize((TR_OpaqueClassBlock *)classSym->getStaticAddress());
      isArrayNew = false;
      }

   else if (node->getOpCodeValue() == TR::newarray || node->getOpCodeValue() == TR::anewarray)
      {
      // We can't do anything if we can't find the size of the class
      // or if the size is not valid for inlining
      //
      if (node->getFirstChild()->getOpCodeValue() != TR::iconst)
         return false;

      size = node->getFirstChild()->getInt();

      // Ignore large arrays so we don't have to worry about size
      // overflow. Make sure that illegal sizes are not inlined.
      //
      if (size < 0 || size > 10000)
         return false;

      if (node->getOpCodeValue() == TR::anewarray)
         {
         TR::Node *classNode = node->getSecondChild();
         size *= TR::Compiler->om.sizeofReferenceField(); //_cg->sizeOfJavaPointer();
         }
      else
         {
         TR_ASSERT(node->getSecondChild()->getOpCodeValue() == TR::iconst, "Expected type as second child of newarray");
         switch (node->getSecondChild()->getInt())
            {
            case 4:
               size *= TR::Compiler->om.elementSizeOfBooleanArray();
               break;
            case 5:
            case 9:
               size *= 2;
               break;
            case 6:
            case 10:
               size *= 4;
               break;
            case 7:
            case 11:
               size *= 8;
               doubleSizeArray = true;
               break;
            }
         }
      isArrayNew = true;
      }

   else
      return false;

   if (!performTransformation(comp(), "%s add allocation candidate [%p]\n", OPT_DETAILS, node))
      return false;

   // An allocation node has been found. Create a new candidate entry.
   //
   Candidate *candidate   = new (trStackMemory()) Candidate;
   memset(candidate, 0, sizeof(Candidate));
   candidate->treeTop     = treeTop;
   candidate->node        = node;
   candidate->size        = size;
   if (_outermostCallSite)
      candidate->isInSniffedMethod = true;
   if (isArrayNew)
      {
      candidate->startOffset = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
      candidate->isArrayNew  = true;
      if (trace())
         {
         traceMsg(comp(), "\nFound new array candidate at node [%p]\n", candidate->node);
         traceMsg(comp(), "   Number of instance bytes = %d\n", candidate->size);
         }
      }
   else
      {
      candidate->startOffset = comp()->fej9()->getObjectHeaderSizeInBytes();
      if (trace())
         {
         traceMsg(comp(), "\nFound new object candidate at node [%p]\n", candidate->node);
         traceMsg(comp(), "   Number of instance bytes = %d\n", candidate->size);
         }
      }
   if (size)
      {
      candidate->initializedBytes = new (trStackMemory()) TR_BitVector(size, trMemory(), stackAlloc);
      candidate->uninitializedBytes = new (trStackMemory()) TR_BitVector(size, trMemory(), stackAlloc);
      }
   candidate->isDoubleSizeArray = doubleSizeArray;
   _candidates.append(candidate);
   return true;
   }

bool TR_NewInitialization::sniffCall(TR::TreeTop *callTree)
   {
   // See if this call is a candidate for inlining - if so, sniff it
   // to see if it would be useful to inline.
   //
   TR::ResolvedMethodSymbol *calleeSymbol = findInlinableMethod(callTree);
   if (!calleeSymbol)
      return false;

   // Build the argument list
   //
   TR::Node *callNode = callTree->getNode()->getFirstChild();

   if (trace())
      traceMsg(comp(), "Sniffing into call at [%p]\n", callNode);

   TR_Array<TR::Node*> *newParms =  new (trStackMemory()) TR_Array<TR::Node*>(trMemory(), callNode->getNumChildren(), false, stackAlloc);
   for (int32_t i = 0; i < callNode->getNumChildren(); ++i)
      newParms->add(resolveNode(callNode->getChild(i)));

   // Sniff the first block of the callee
   //
   TR::Block *firstCalleeBlock = calleeSymbol->getFirstTreeTop()->getNode()->getBlock();
   bool isOutermostCallSite = (_outermostCallSite == NULL);
   if (isOutermostCallSite)
      _outermostCallSite = callTree;

   TR_Array<TR::Node*> *oldParms = _parms;
   _parms = newParms;

   bool sniffedCompleteMethod = findNewCandidatesInBlock(firstCalleeBlock->getEntry(), firstCalleeBlock->getExit());

   _parms = oldParms;

   if (isOutermostCallSite)
      _outermostCallSite = NULL;

   if (trace())
      traceMsg(comp(), "Finished sniffing into call at [%p]\n", callNode);

   return sniffedCompleteMethod;
   }

TR::ResolvedMethodSymbol *TR_NewInitialization::findInlinableMethod(TR::TreeTop *callTree)
   {
   // See if this call is a candidate for inlining.
   //
   if (!_sniffCalls || !_firstActiveCandidate)
      return NULL;

   TR::Node *callNode = callTree->getNode()->getFirstChild();
   if (callNode->getOpCode().isCallIndirect())
      return NULL;

   TR::ResolvedMethodSymbol *calleeSymbol = callNode->getSymbol()->getResolvedMethodSymbol();
   if (!calleeSymbol)
      return NULL;

   TR_ResolvedMethod *method = calleeSymbol->getResolvedMethod();
   if (!method)
      return NULL;

   if (_sniffConstructorsOnly)
      {
      if (!calleeSymbol->isSpecial() || !method->isConstructor())
      return NULL;
      }

    uint32_t bytecodeSize = method->maxBytecodeIndex();
    if (bytecodeSize > (uint32_t) _maxInlinedBytecodeSize)
      {
      /////if (trace())
      /////   printf("secs Single inline too big in %s\n", comp()->signature());
      return NULL;
      }
    if (_totalInlinedBytecodeSize + bytecodeSize > (uint32_t) _maxTotalInlinedBytecodeSize)
      {
      /////if (trace())
      /////   printf("secs Total inline too big in %s\n", comp()->signature());
      return NULL;
      }

   // Make sure the inliner will be able to inline this call
   //
   vcount_t visitCount = comp()->getVisitCount();
   //comp()->setVisitCount(1);

   //TR_VirtualGuardKind guardKind = TR_NoGuard;
   TR_VirtualGuardSelection *guard = 0;
   TR_InlineCall newInlineCall(optimizer(), this);
   newInlineCall.setSizeThreshold(_maxInlinedBytecodeSize);
   TR_OpaqueClassBlock * thisClass = 0;
   TR::SymbolReference *symRef = callNode->getSymbolReference();
   TR::MethodSymbol *calleeMethodSymbol = symRef->getSymbol()->castToMethodSymbol();
   TR::Node * parent = callTree->getNode();

   //TR_CallSite *callsite = TR_CallSite::create(callTree, parent, callNode, thisClass, symRef,  0, comp(), trStackMemory());

   TR_CallSite *callsite = TR_CallSite::create(callTree, parent, callNode, thisClass, symRef,  0, comp(), comp()->trMemory(), stackAlloc);

	//TR_CallSite *callsite = new (trStackMemory()) TR_CallSite (symRef->getOwningMethod(comp()),callTree,parent,callNode,calleeMethodSymbol->getMethod(),thisClass,(int32_t)symRef->getOffset(),symRef->getCPIndex(),0,calleeMethodSymbol->getResolvedMethodSymbol(),callNode->getOpCode().isCallIndirect(),calleeMethodSymbol->isInterface(),callNode->getByteCodeInfo(),comp());

   newInlineCall.getSymbolAndFindInlineTargets(NULL,callsite);
   bool canSniff = callsite->numTargets() ? true : false;

// (newInlineCall.isInlineable(NULL, callNode, guard, thisClass, callTree) != NULL);
   //comp()->setVisitCount(visitCount);

   if (!canSniff)
      {
      if (trace())
         traceMsg(comp(), "\nCall at [%p] to %s is NOT inlineable\n", callTree->getNode()->getFirstChild(), calleeSymbol->getResolvedMethod()->signature(trMemory()));
      return NULL;
      }

   if (trace())
      {
      traceMsg(comp(), "\nGenerating trees for call at [%p] to %s\n", callTree->getNode()->getFirstChild(), calleeSymbol->getResolvedMethod()->signature(trMemory()));
      }

   dumpOptDetails(comp(), "O^O NEW INITIALIZATION: Peeking into the IL to check for inlineable calls \n");

   //comp()->setVisitCount(1);

   canSniff = (calleeSymbol->getResolvedMethod()->genMethodILForPeeking(calleeSymbol, comp()) != NULL);
   //comp()->setVisitCount(visitCount);

   if (!canSniff)
      {
      if (trace())
         traceMsg(comp(), "   (IL generation failed)\n");
      return NULL;
      }

   if (trace())
      {
      //comp()->setVisitCount(1);
      for (TR::TreeTop *tt = calleeSymbol->getFirstTreeTop(); tt; tt = tt->getNextTreeTop())
         comp()->getDebug()->print(comp()->getOutFile(), tt);
      //comp()->setVisitCount(visitCount);
      }

   _totalInlinedBytecodeSize += bytecodeSize;
   return calleeSymbol;
   }

// Process a GC point
//
void TR_NewInitialization::setGCPoint(TR::TreeTop *treeTop, TR::Node *node)
   {
   // If this GC point can GC and return see if it is the first GC point for
   // the previous merge sequence.
   // If so, save it with the first candidate of the sequence.
   // If this GC point is in a sniffed method, save the outermost call point.
   //
   if (_firstMergeCandidate)
      {
      if (!_firstMergeCandidate->firstGCTree &&
          (node == NULL || node->canGCandReturn()))
         {
         TR::TreeTop * GCTree = _outermostCallSite ? _outermostCallSite : treeTop;
         _firstMergeCandidate->firstGCTree = GCTree;
         }

      // Remember that a GC point was found before the next candidate
      //
      _firstMergeCandidate->GCPointFoundBeforeNextCandidate = true;
      }
   }

// Resolve the node if it is a parameter reference
//
TR::Node *TR_NewInitialization::resolveNode(TR::Node *node)
   {
   if (_parms == NULL)
      return node;
   if (!node->getOpCode().isLoadVarOrStore())
      return node;
   TR::Symbol *sym = node->getSymbol();
   if (!sym->isParm())
      return node;
   int32_t parmNum = sym->getParmSymbol()->getOrdinal();
   if (_parms->element(parmNum) == NULL)
      return node;

   // If this is a load of a parm, return the underlying argument node.
   // If this is a store of a parm, it no longer represents an underlying
   // argument, so remove it from the parm list.
   //
   if (node->getOpCode().isLoadVar())
      return _parms->element(parmNum);
   _parms->element(parmNum) = NULL;
   return node;
   }

int32_t TR_LocalNewInitialization::getValueNumber(TR::Node *node)
   {
   if (node->getOpCode().isStore())
      {
      if (node->getOpCode().isIndirect())
         return node->getSecondChild()->getGlobalIndex();
      else
         return node->getFirstChild()->getGlobalIndex();
      }
   else
      return node->getGlobalIndex();
   }


bool TR_NewInitialization::matchLocalLoad(TR::Node *node, Candidate *c)
   {
   // See if the node is a local load that matches any of the local loads or
   // stores that represent this allocation
   //
   if (node->getOpCodeValue() != TR::aload)
      return false;
   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::Symbol           *sym    = symRef->getSymbol();
   int32_t             offset = symRef->getOffset();

   if (!sym->isAutoOrParm())
      return false;

   // See if this is a load that has already been matched with the allocation
   //
   NodeEntry *entry;
   for (entry = c->localLoads.getFirst(); entry; entry = entry->getNext())
      {
      if (node == entry->node)
         return true;
      }

   // See if this is a load that matches a store for the allocation. If so, it
   // becomes a matching local load
   //
   for (entry = c->localStores.getFirst(); entry; entry = entry->getNext())
      {
      if (sym == entry->node->getSymbol() &&
          offset == entry->node->getSymbolReference()->getOffset() &&
          getValueNumber(node) == getValueNumber(entry->node))
         {
         entry = new (trStackMemory()) NodeEntry;
         entry->node = node;
         c->localLoads.add(entry);
         return true;
         }
      }

   return false;
   }

bool TR_NewInitialization::isNewObject(TR::Node *node, Candidate *c)
   {
   TR::Node *resolvedNode = resolveNode(node);
   if (resolvedNode == c->node)
      return true;

   // See if the node is a local load that matches any of the local loads or
   // stores that represent this allocation
   //
   if (matchLocalLoad(node, c))
      return true;

   // See if the node is a parm that resolves to a local load that matches any
   // of the local loads or stores that represent this allocation
   //
   if (resolvedNode != node && matchLocalLoad(resolvedNode, c))
      return true;

   return false;
   }

TR_NewInitialization::Candidate *TR_NewInitialization::findCandidateReferenceInSubTree(TR::Node *node, TR_ScratchList<TR::Node> *seenNodes)
   {
   if (!node ||
       seenNodes->find(node))
      return NULL;

   seenNodes->add(node);

   // look in subtree for a new object, return first one that matches a candidate
   for (Candidate *c = _firstActiveCandidate; c; c = c->getNext())
      {
      if (isNewObject(node, c))
         return c;
      }

   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      Candidate *c = findCandidateReferenceInSubTree(node->getChild(i), seenNodes);
      if (c)
         return c;
      }
   return NULL;
   }

TR_NewInitialization::Candidate *TR_NewInitialization::findCandidateReference(TR::Node *node)
   {

   for (Candidate *c = _firstActiveCandidate; c; c = c->getNext())
      {
      if (isNewObject(node, c))
         return c;
      }
   return NULL;
   }


TR_NewInitialization::Candidate *TR_NewInitialization::findBaseOfIndirection(TR::Node *directBase)
   {
   for (Candidate *c = _firstActiveCandidate; c; c = c->getNext())
      {
      if (c->isArrayNew)
         {
         if (directBase->getOpCode().isArrayRef() && isNewObject(directBase->getFirstChild(), c))
            return c;
         }
      else
         {
         if (isNewObject(directBase, c))
            return c;
         }
      }
   return NULL;
   }

bool TR_NewInitialization::visitNode(TR::Node *node)
   {
   if (node->getVisitCount() == comp()->getVisitCount())
      return false;
   node->setVisitCount(comp()->getVisitCount());

   // Look at the children first, they will be processed before this node
   //
   int32_t i;
   for (i = node->getNumChildren()-1; i >= 0; --i)
      {
      if (visitNode(node->getChild(i)))
         return true;
      }

   Candidate *c;

   if (node->getOpCode().isLoadVarOrStore() && node->getOpCode().isIndirect())
      {
      TR::Node *base = node->getFirstChild();
      c = findBaseOfIndirection(base);
      if (c && c->numInitializedBytes+c->numUninitializedBytes < c->size)
         {
         int32_t offset = -1;

         TR::Node* targetNode =
            (node->getOpCode().isStore()) ?
               node->getSecondChild() :
               node;
         int32_t size =
            targetNode->getDataType() == TR::Address ?
               TR::Compiler->om.sizeofReferenceField() :
               node->getOpCode().getSize();

         if (!c->isArrayNew)
            {
            // Object reference
            //
            offset = node->getSymbolReference()->getOffset() - c->startOffset;
            }
         else if (base->getSecondChild()->getOpCodeValue() == TR::iconst)
            {
            // Array new reference with constant index
            //
            offset = node->getSymbolReference()->getOffset() + base->getSecondChild()->getInt() - c->startOffset;
            }
         else if (node->getOpCode().isLoadVar())
            {
            // Array new reference with unknown index
            //
            escapeToUserCode(c, node);
            }

        if (offset >= 0 && offset < c->size &&
             !c->initializedBytes->get(offset) &&
             !c->uninitializedBytes->get(offset))
            {
            // This is the first reference to this part of the object
            //
            #if DEBUG
               for (i = size-1; i >= 0; --i)
                  {
                  TR_ASSERT(!c->initializedBytes->get(offset+i) &&
                            !c->uninitializedBytes->get(offset+i), "assertion failure");
                  }
            #endif

            // real-time write barriers read before they write, so treat them as loads
            if (node->getOpCode().isStore() && (!comp()->getOptions()->realTimeGC() || !node->getOpCode().isWrtBar()))
               {
               // If this is a store of a zero value and we are allowed to
               // remove it, do so. It is useless since the allocation will
               // zero-initialize anyway.
               //
               if (_removeZeroStores)
                  {
                  TR::Node *value = node->getSecondChild();
                  if (value->isConstZeroBytes())
                     {
                         setAffectedCandidate(c);
                         return true;
                     }
                  }

               if (trace())
                  traceMsg(comp(), "Node [%p]: Initialize bytes %d-%d for candidate [%p]\n", node, offset, offset+size-1, c->node);

               for (i = size-1; i >= 0; --i)
                 {
                 c->initializedBytes->set(offset+i);
                 }
               c->numInitializedBytes += size;

               if (trace())
                  traceMsg(comp(), "Node [%p]: Uninitialized %d Initialized %d\n", node, c->numUninitializedBytes, c->numInitializedBytes);

               setAffectedCandidate(c);
               }
            else
               {
               for (i = size-1; i >= 0; --i)
                  c->uninitializedBytes->set(offset+i);
               c->numUninitializedBytes += size;
               if (trace())
                  traceMsg(comp(), "Node [%p]: Uninitialize bytes %d-%d for candidate [%p]\n", node, offset, offset+size-1, c->node);
               }
            }
         }
      }

   // Keep track of stores of allocation nodes to locals and parms.
   // Otherwise if the object reference escapes via a store, stop looking.
   //
   if (node->getOpCode().isStore())
      {
      //if (node->getOpCode().isIndirect())
      if (!node->getSymbolReference()->getSymbol()->isAutoOrParm())
         {
         if (node->getOpCode().isIndirect())
            c = findCandidateReference(node->getSecondChild());
         else
            c = findCandidateReference(node->getFirstChild());

         if (c)
            {
            // Allocation reference is being indirectly stored. If it is being
            // stored into another part of the same merged allocation, that is
            // OK, since it can't escape without the whole merged allocation
            // being initialized first.
            // Otherwise this allocation could escape and it must be completely
            // initialized by this point.
            //
            if (c->canBeMerged && node->getOpCode().isIndirect())
               {
               Candidate *c1 = findBaseOfIndirection(node->getFirstChild());
               if (!(c1 && c1->canBeMerged))
                  escapeToUserCode(c, node);
               }
            else
               escapeToUserCode(c, node);
            }
         }
      else if (node->getOpCodeValue() == TR::astore)
         {
         // If a candidate reference was previously stored into this local,
         // it is not stored there any more so remove it from the localStores
         // list for the candidate.
         // Note that there may be more than one store to the local in the list
         // so we can't stop when one is found.
         //
         if (node->getSymbol()->isAutoOrParm())
            {
            for (c = _firstActiveCandidate; c; c = c->getNext())
               {
               NodeEntry *entry, *prev = NULL;
               for (entry = c->localStores.getFirst(); entry; entry = entry->getNext())
                  {
                  if (entry->node->getSymbol() == node->getSymbol() &&
                      entry->node->getSymbolReference()->getOffset() == node->getSymbolReference()->getOffset())
                     {
                     if (prev)
                        prev->setNext(entry->getNext());
                     else
                        c->localStores.setFirst(entry->getNext());
                     }
                  else
                     prev = entry;
                  }
               }
            }

         // If this is a store into a parm, it no longer represents the
         // underlying argument
         //
         TR::Symbol *sym = node->getSymbol();
         if (_parms && sym->isParm())
            {
            int32_t parmNum = sym->getParmSymbol()->getOrdinal();
            _parms->element(parmNum) = NULL;
            }

         // See if it is one of the candidates that is being stored
         //
         c = findCandidateReference(node->getFirstChild());
         if (c)
            {
            if (node->getSymbol()->isAutoOrParm())
               {
               // Object reference is stored into a local - keep track of it
               //
               NodeEntry *entry = new (trStackMemory()) NodeEntry;
               entry->node = node;
               c->localStores.add(entry);
               }
            else
               // Object reference escapes via an indirect store
               //
               escapeToUserCode(c, node);
            }
         }
      }

   return false;
   }

void TR_NewInitialization::setAffectedCandidate(Candidate *c)
   {
   // If the current position is inside sniffed methods, remember the outermost
   // call site as being useful to inline for this candidate.
   //
   if (_outermostCallSite)
      {
      TreeTopEntry *callEntry = new (trStackMemory()) TreeTopEntry;
      callEntry->treeTop = _outermostCallSite;
      c->inlinedCalls.add(callEntry);
      }
   }


void TR_NewInitialization::escapeToUserCode(Candidate *c, TR::Node *cause)
   {
   if (c->numInitializedBytes + c->numUninitializedBytes < c->size)
      {
      if (c->numInitializedBytes == 0)
         {
         c->numUninitializedBytes = c->size;
         c->uninitializedBytes->setAll(c->size);
         }
      else
         {
         c->uninitializedBytes->setAll(c->size);
         *c->uninitializedBytes -= *c->initializedBytes;
         c->numUninitializedBytes = c->size - c->numInitializedBytes;
         }

      if (trace())
         traceMsg(comp(), "Node [%p]: Make the rest of candidate [%p] uninitialized\n", cause, c->node);
      }
   }

void TR_NewInitialization::escapeToUserCodeAllCandidates(TR::Node *cause, bool onlyArrays)
   {
   for (Candidate *c = _firstActiveCandidate; c; c = c->getNext())
      {
      if (!onlyArrays ||
          (c->node->getOpCodeValue() == TR::newarray ||
           c->node->getOpCodeValue() == TR::anewarray))
         escapeToUserCode(c, cause);
      }
   }

void TR_NewInitialization::escapeToGC(Candidate *c, TR::Node *cause)
   {
   if (c->numUninitializedBytes + c->numInitializedBytes == c->size)
      {
      // Candidate's initialization status is complete
      //
      return;
      }

   if (c->node->getOpCodeValue() == TR::newarray)
      {
      // No GC slots to initialize for a primitive array
      //
      return;
      }

   if (c->node->getOpCodeValue() == TR::New)
      {
      int32_t *referenceSlots = comp()->fej9()->getReferenceSlotsInClass(comp(), (TR_OpaqueClassBlock*)c->node->getFirstChild()->getSymbol()->getStaticSymbol()->getStaticAddress());
      if (referenceSlots)
         {
         for (int32_t i = 0; referenceSlots[i]; i++)
            {
            //int32_t byteOffset = referenceSlots[i]*_cg->sizeOfJavaPointer() - c->startOffset;
            int32_t byteOffset = referenceSlots[i]*TR::Compiler->om.sizeofReferenceField() - c->startOffset;
            //for (int32_t j = byteOffset; j < byteOffset+_cg->sizeOfJavaPointer(); j++)
            for (int32_t j = byteOffset; j < byteOffset+TR::Compiler->om.sizeofReferenceField(); j++)
               {
               if (c->uninitializedBytes->get(j))
                  continue;
               if (c->initializedBytes->get(j))
                  continue;
               c->uninitializedBytes->set(j);
               c->numUninitializedBytes++;
               }
            }
         if (trace())
            traceMsg(comp(), "Node [%p]: Make reference slots of candidate [%p] uninitialized\n", cause, c->node);
         }
      }
   else // (c->node->getOpCodeValue() == TR::anewarray)
      {
      // All of the slots of a reference array must be initialized at this
      // point
      //
      escapeToUserCode(c, cause);
      }
   }

void TR_NewInitialization::escapeToGC(TR::Node *cause)
   {
   for (Candidate *c = _firstActiveCandidate; c; c = c->getNext())
      escapeToGC(c, cause);
   }

void TR_NewInitialization::escapeViaCall(TR::Node *callNode)
   {
   // For each candidate that can escape to the called method via argument,
   // completely initialize it.
   //
   int32_t i;
   for (i = callNode->getFirstArgumentIndex(); i < callNode->getNumChildren(); i++)
      {
      Candidate *c = findCandidateReference(callNode->getChild(i));
      if (c)
         escapeToUserCode(c, callNode);
      }

   // For all of the other active candidates, their reference fields must be
   // initialized since the call can cause a GC
   //
   escapeToGC(callNode);
   }

void TR_NewInitialization::escapeViaArrayCopyOrArraySet(TR::Node *arrayCopyNode)
   {
   // array copies cause complete or incomplete initialization of candidates
   // determine which, parameters to array copy are:
   // (scrObj + hdrSize), dstObj + hdrSize), copyLen, [arrayType]
   //
   // optimization would be to filter only the incomplete elements, for now
   // treat as completely initialized

   // For each candidate that can escape to the called method via argument,
   // completely initialize it.
   //
   TR_ScratchList<TR::Node> seenNodes(trMemory());
   Candidate *c = findCandidateReferenceInSubTree(arrayCopyNode->getFirstChild(), &seenNodes);
   if (c)
      escapeToUserCode(c, arrayCopyNode);
   else
      escapeToUserCodeAllCandidates(arrayCopyNode, true);

   if (arrayCopyNode->getOpCodeValue() == TR::arraycopy)
      {
      seenNodes.deleteAll();
      c = findCandidateReferenceInSubTree(arrayCopyNode->getSecondChild(), &seenNodes);
      if (c)
         escapeToUserCode(c, arrayCopyNode);
      else
         escapeToUserCodeAllCandidates(arrayCopyNode, true);
      }

   // For all of the other active candidates, their reference fields must be
   // initialized since the call can cause a GC
   //
   escapeToGC(arrayCopyNode);
   }

// Convert uninitialized bytes to uninitialized words
//
void TR_NewInitialization::findUninitializedWords()
   {
   // Find out how many and which words need to be zero-initialized and
   // store this information in the candidate entry.
   //
   Candidate *c;
   for (c = _candidates.getFirst(); c; c = c->getNext())
      {
      if (c->numUninitializedBytes == 0)
         c->numUninitializedWords = 0;
      else if (c->numUninitializedBytes == c->size)
         c->numUninitializedWords = (c->size+3)/4;
      else
         {
         c->numUninitializedWords = 0;
         int32_t numWords = (c->size+3)/4;
         c->uninitializedWords = new (trStackMemory()) TR_BitVector(numWords, trMemory(), stackAlloc);
         for (int32_t j = 0; j < numWords; j++)
            {
            for (int32_t k = 0; k < 4; ++k)
               {
               if (c->uninitializedBytes->get(4*j+k))
                  {
                  c->uninitializedWords->set(j);
                  c->numUninitializedWords++;
                  break;
                  }
               }
            }
         }

      if (trace())
         {
         traceMsg(comp(), "Uninitialized words for candidate [%p] = %d/%d : ", c->node, c->numUninitializedWords, c->size/4);
         if (c->uninitializedWords)
            {
            c->uninitializedWords->print(comp());
            traceMsg(comp(), "\n");
            }
         else if (c->numUninitializedWords)
            traceMsg(comp(), "{all}\n");
         else
            traceMsg(comp(), "{}\n");
         }
      }
   }

bool TR_NewInitialization::changeNewCandidates()
   {
   // First look through the candidates to see if we should be doing any
   // inlining
   //
   Candidate *c;
   for (c = _candidates.getFirst(); c; c = c->getNext())
      {
      if (c->canBeMerged || !c->isInSniffedMethod)
         {
         while (!c->inlinedCalls.isEmpty())
            {
            TreeTopEntry *inlinedCall = c->inlinedCalls.pop();
            TreeTopEntry *tt;
            for (tt = _inlinedCallSites.getFirst(); tt; tt = tt->getNext())
               {
               if (tt->treeTop == inlinedCall->treeTop)
                  break;
               }
            if (!tt)
               _inlinedCallSites.add(inlinedCall);
            }
         }
      }

   // If changes to candidates can be done but it involves inlining some
   // methods, just do the inlining and wait until the next iteration to
   // actually do the work.
   //
   if (!_inlinedCallSites.isEmpty())
      {
      inlineCalls();
      return true;
      }

   // Now go through the candidates again to see if any can either be merged
   // with other candidates or have explicit zero-initialization information
   // added.
   //
   for (c = _candidates.getFirst(); c; c = c->getNext())
      {
      // If the candidate has already been processed, skip it
      //
      if (c->treeTop == NULL)
         continue;
      if (c->firstMergeCandidate || !c->isInSniffedMethod)
         modifyTrees(c);
      }

   return false;
   }

// Inline any calls that need to be inlined for the analysis to make progress
//
void TR_NewInitialization::inlineCalls()
   {
   while (!_inlinedCallSites.isEmpty())
      {
      TreeTopEntry            *entry     = _inlinedCallSites.pop();
      TR::TreeTop              *treeTop   = entry->treeTop;
      TR::ResolvedMethodSymbol *methodSym = treeTop->getNode()->getFirstChild()->getSymbol()->getResolvedMethodSymbol();
      TR_ResolvedMethod     *method    = methodSym->getResolvedMethod();
      if (trace())
         {
         traceMsg(comp(), "\nInlining method %s into treetop at [%p], total inlined size = %d\n", method->signature(trMemory()), treeTop->getNode(), _totalInlinedBytecodeSize+method->maxBytecodeIndex());
         /////printf("secs Inlining method %s in %s\n", method->signature(trMemory()), comp()->signature());
         }

      // Now inline the call
      //
      TR_InlineCall newInlineCall(optimizer(), this);
      newInlineCall.setSizeThreshold(_maxInlinedBytecodeSize);
      bool inlineOK = newInlineCall.inlineCall(treeTop);

      // If the inlining failed for some reason, prohibit inlining
      // next time through.
      //
      if (!inlineOK)
         _sniffCalls = false;

      // Otherwise, usedef and value number info will now be invalid
      //
      else
         {
         _invalidateUseDefInfo = true;
         _totalInlinedBytecodeSize += method->maxBytecodeIndex();
         }
      }
   }

// Insert the explicit initializations for the given candidate
//
void TR_NewInitialization::modifyTrees(Candidate *candidate)
   {
   Candidate  *c;
   TR::TreeTop *treeTop = candidate->treeTop;
   int32_t     i;//, j;

   int32_t numExtraCandidates      = 0;
   bool    hasDoubleSizeArrays     = candidate->isDoubleSizeArray;
   int32_t allocationSize          = (candidate->size + candidate->startOffset + 3) & ~3;
   int32_t numWordsToBeInitialized = candidate->numUninitializedWords;

   Candidate *startOfNextMergeSequence = NULL;

   TR_ExtraInfoForNew *extraInfo;
   TR::SymbolReference *symRef;

   // If this may be the start of a merge sequence, find the others and
   // accumulate size information
   //
   if (candidate->firstMergeCandidate)
      {
      for (c = candidate->getNext(); c && !c->firstMergeCandidate; c = c->getNext())
         {
         if (c->canBeMerged)
            {
            numExtraCandidates++;
            allocationSize += (c->size + c->startOffset + 3) & ~3;
            numWordsToBeInitialized += c->numUninitializedWords;
            hasDoubleSizeArrays |= c->isDoubleSizeArray;
            }
         }
      startOfNextMergeSequence = c;
      }

   // If multiple allocation nodes are to be merged, set up the tree for the
   // merged allocation.
   //
   if (numExtraCandidates)
      {
      if (trace())
         traceMsg(comp(), "Found %d news to be merged, %d words to be initialized in %s\n", numExtraCandidates+1, numWordsToBeInitialized, comp()->signature());

      // Set up a bit vector representing the words within the multiple
      // allocation that are to be initialized and put it into the symbol
      // reference associated with the mergenew node.
      //
      extraInfo = new (trHeapMemory()) TR_ExtraInfoForNew;
      extraInfo->numZeroInitSlots = 0;
      extraInfo->zeroInitSlots = new (trHeapMemory()) TR_BitVector(allocationSize/4, trMemory());

      symRef = new (trHeapMemory()) TR::SymbolReference(comp()->getSymRefTab(), *candidate->node->getSymbolReference(), 0);
      symRef->setReferenceNumber(candidate->node->getSymbolReference()->getReferenceNumber());
      symRef->setExtraInfo(extraInfo);

      TR::Node *mergeNode = TR::Node::createWithSymRef(candidate->node, TR::MergeNew, numExtraCandidates+1, symRef);

      if (performTransformation(comp(), "%s Merging %d allocations starting at [%p] into merged new at [%p]\n", OPT_DETAILS, numExtraCandidates+1, candidate->node, mergeNode))
         {
         treeTop = TR::TreeTop::create(comp(), treeTop->getPrevTreeTop(), mergeNode);

         // Go through the candidates to be merged twice. Process the double-size
         // arrays first and then the rest. This puts the double-size arrays at
         // the start of the merged allocation.
         //
         int32_t startWord = 0;
         i = 0;
         Candidate * startCandidate = 0;
         if (hasDoubleSizeArrays)
            {
            for (c = candidate; c != startOfNextMergeSequence; c = c->getNext())
               {
               if (c->canBeMerged && c->isDoubleSizeArray)
                  {
                  if (!startCandidate)
                     startCandidate = c;
                  mergeNode->setAndIncChild(i++, c->node);
                  TR::TransformUtil::removeTree(comp(), c->treeTop);

                  extraInfo->numZeroInitSlots += buildInitializationInfo(c, extraInfo->zeroInitSlots, startWord);
                  int32_t objectSize = (c->startOffset + c->size + 3) / 4;
                  c->startOffset = startWord*4;
                  startWord += objectSize;
                  c->treeTop = NULL; // This candidate is now processed
                  }
               }
            }
         for (c = candidate; c != startOfNextMergeSequence; c = c->getNext())
            {
            if (c->canBeMerged && !c->isDoubleSizeArray)
               {
               if (!startCandidate)
                  startCandidate = c;
               mergeNode->setAndIncChild(i++, c->node);
               TR::TransformUtil::removeTree(comp(), c->treeTop);

               extraInfo->numZeroInitSlots += buildInitializationInfo(c, extraInfo->zeroInitSlots, startWord);
               int32_t objectSize = (c->startOffset + c->size + 3) / 4;
               c->startOffset = startWord*4;
               startWord += objectSize;
               c->treeTop = NULL; // This candidate is now processed
               }
            }

         // Look through the trees and see if any references to merged allocations
         // can be changed into references to offsets from the start of the
         // merged object. This will relieve register pressure.
         // This can only be done until the first GC point after the merged
         // allocation, which has been saved in the first merged candidate.
         //
         modifyReferences(candidate, startOfNextMergeSequence, startCandidate, treeTop);

         _invalidateUseDefInfo = true;
         }
      }

   else
      {
      // Change the symbol reference on the allocation node so that it can hold
      // the zero-initialization information.
      //
      extraInfo = new (trHeapMemory()) TR_ExtraInfoForNew;
      if (candidate->node->canSkipZeroInitialization())
         {
         extraInfo->numZeroInitSlots = 0;
         //printf("Skip zero init for new in %s\n", comp()->signature());
         }
      else
         extraInfo->numZeroInitSlots = candidate->numUninitializedWords;
      if (candidate->uninitializedWords &&
          !candidate->node->canSkipZeroInitialization())
         {
         extraInfo->zeroInitSlots = new (trHeapMemory()) TR_BitVector(allocationSize, trMemory());
         *extraInfo->zeroInitSlots = *candidate->uninitializedWords;
         }
      else
         extraInfo->zeroInitSlots = NULL;

      symRef = new (trHeapMemory()) TR::SymbolReference(comp()->getSymRefTab(), *candidate->node->getSymbolReference(), 0);
      symRef->setReferenceNumber(candidate->node->getSymbolReference()->getReferenceNumber());
      symRef->setExtraInfo(extraInfo);
      candidate->node->setSymbolReference(symRef);
      candidate->treeTop = NULL;
      }
   }

int32_t TR_NewInitialization::buildInitializationInfo(Candidate *c, TR_BitVector *wordsToBeInitialized, int32_t startWord)
   {
   int32_t numWordsInitialized = 0;

   if (c->numUninitializedWords && !c->uninitializedWords)
      {
      // All words in the allocation must be zero-initialized
      //
      for (int32_t i = ((c->size+3)/4)-1; i >= 0; i--)
         {
         wordsToBeInitialized->set(startWord + (c->startOffset/4) + i);
         numWordsInitialized++;
         }
      }
   else if (c->numUninitializedWords)
      {
      TR_BitVectorIterator bvi(*c->uninitializedWords);
      while (bvi.hasMoreElements())
         {
         wordsToBeInitialized->set(startWord + (c->startOffset/4) + bvi.getNextElement());
         numWordsInitialized++;
         }
      }
   return numWordsInitialized;
   }

void TR_NewInitialization::modifyReferences(Candidate *candidate, Candidate *startOfNextMergeSequence, Candidate * startCandidate, TR::TreeTop *mergeTree)
   {
   TR_ASSERT(candidate->firstGCTree, "Merged candidates have no following GC tree");
   TR::TreeTop *treeTop;
   for (treeTop = mergeTree->getNextTreeTop(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      modifyReferences(candidate, startOfNextMergeSequence, startCandidate, treeTop->getNode());
      if (treeTop == candidate->firstGCTree)
         break;
      }
   }

void TR_NewInitialization::modifyReferences(Candidate *candidate, Candidate *startOfNextMergeSequence, Candidate * startCandidate, TR::Node *node)
   {
   int32_t i;//, j;
   bool firstChildIsCandidate = false;
   bool secondChildIsCandidate = false;
   Candidate *c;

   // Look at the children of this node to see if they contain any references
   // to allocation nodes that are part of this merge sequence
   //
   for (i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      if (child->getOpCodeValue() == TR::New ||
          child->getOpCodeValue() == TR::newarray ||
          child->getOpCodeValue() == TR::anewarray)
         {
         if (child == startCandidate->node)
            {
            if (i == 0)
               firstChildIsCandidate = true;
            else if (i == 1)
               secondChildIsCandidate = true;
            continue;
            }
         for (c = candidate; c != startOfNextMergeSequence; c = c->getNext())
            {
            if (c != startCandidate && c->canBeMerged && child == c->node)
               {
               if (((TR::Compiler->target.is64Bit()
                     ) ?
                  dumpOptDetails(comp(), "%s Changing child %d of node [%p] into a TR::aladd\n", OPT_DETAILS, i, node)
                  : dumpOptDetails(comp(), "%s Changing child %d of node [%p] into a TR::aiadd\n", OPT_DETAILS, i, node)))
                  {
                  // Replace the node reference with an address calculation off
                  // the first candidate
                  //
                  if (!c->offsetReference)
                     {
                     // build a TR::aladd node instead if required
                     if (TR::Compiler->target.is64Bit())
                        {
                        TR::Node *offsetNode = TR::Node::create(child, TR::lconst, 0);
                        offsetNode->setLongInt((int64_t)c->startOffset);
                        c->offsetReference = TR::Node::create(TR::aladd, 2, startCandidate->node, offsetNode);
                        }
                     else
                        c->offsetReference = TR::Node::create(TR::aiadd, 2, startCandidate->node, TR::Node::create(child, TR::iconst, 0, c->startOffset));
                     c->offsetReference->setIsNonNull(true);
                     }
                  node->setAndIncChild(i, c->offsetReference);
                  child->decReferenceCount();
                  TR_ASSERT(child->getReferenceCount() > 0, "Lost a merged allocation node");

                  if (i == 0)
                     firstChildIsCandidate = true;
                  else if (i == 1)
                     secondChildIsCandidate = true;
                  }
               break;
               }
            }
         }
      else if (child->getNumChildren())
         {
         // Look at the child's children
         //
         modifyReferences(candidate, startOfNextMergeSequence, startCandidate, child);
         }
      }

   // If this is a write barrier store of one candidate into a field of another
   // the write barrier store can be changed into a simple indirect store.
   //
   if (node->getOpCodeValue() == TR::awrtbari && firstChildIsCandidate && secondChildIsCandidate && !comp()->getOptions()->realTimeGC())
      {
      if (performTransformation(comp(), "%sChanging write barrier store into iastore [%p]\n", OPT_DETAILS, node))
         {
         TR::Node::recreate(node, TR::astorei);
         node->getChild(2)->recursivelyDecReferenceCount();
         node->setNumChildren(2);
         }
      }
   }

