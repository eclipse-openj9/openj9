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

#ifndef NEWINITIALIZATION_INCL
#define NEWINITIALIZATION_INCL

#include <stddef.h>
#include <stdint.h>
#include "infra/Link.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"

class TR_BitVector;
namespace TR { class Node; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class TreeTop; }
template <class T> class TR_Array;
template <class T> class TR_ScratchList;

/// Attempts to skip zero initializations of objects where possible. Basically
/// scans forward in the basic block from the point of the allocation and tries
/// to find explicit definitions of fields (say in constructors) that occur
/// before any use of the field. In these cases the zero initialization of
/// fields (required according to Java spec, so no field is uninitialized) can
/// be skipped. Note that for reference fields the presence of a GC point
/// before any user-specified definition means that the field must be zero
/// initialized. Peeking inside calls is done to maximize the number of explicit
/// field initializations seen by the analysis. In addition, for some special
/// array allocations (e.g. char arrays that are used by String, often do not
/// need zero initialization as they are filled in by arraycopy calls
/// immediately), we can skip zero initializations completely.
class TR_NewInitialization : public TR::Optimization
   {
   public:
   TR_NewInitialization(TR::OptimizationManager *manager);

   protected:

   struct NodeEntry : public TR_Link<NodeEntry>
      {
      TR::Node   *node;
      };

   struct TreeTopEntry : public TR_Link<TreeTopEntry>
      {
      TR::TreeTop *treeTop;
      };

   struct Candidate : public TR_Link<Candidate>
      {
      TR::TreeTop               *treeTop;
      TR::Node                  *node;
      TR_BitVector             *uninitializedWords;
      TR_BitVector             *initializedBytes;
      TR_BitVector             *uninitializedBytes;
      TR::TreeTop               *firstGCTree;
      TR::Node                  *offsetReference;

      TR_LinkHead<NodeEntry>    localStores;
      TR_LinkHead<NodeEntry>    localLoads;
      TR_LinkHead<TreeTopEntry> inlinedCalls;

      int32_t                   size;
      int32_t                   startOffset;
      int32_t                   numUninitializedWords;
      int32_t                   numInitializedBytes;
      int32_t                   numUninitializedBytes;

      bool                      canBeMerged;
      bool                      firstMergeCandidate;
      bool                      GCPointFoundBeforeNextCandidate;
      bool                      isArrayNew;
      bool                      isDoubleSizeArray;
      bool                      isInSniffedMethod;
      };

   int32_t performAnalysis(bool doGlobalAnalysis);
   bool    doAnalysisOnce(int32_t iteration);

   // Methods for finding the allocation node candidates and for scanning the
   // trees for explicit initializations and merge opportunities
   //
   //
   void     findNewCandidates();
   bool     findNewCandidatesInBlock(TR::TreeTop *startTree, TR::TreeTop *endTree);
   bool     findAllocationNode(TR::TreeTop *treeTop, TR::Node *node);
   bool     sniffCall(TR::TreeTop *callTree);
   TR::ResolvedMethodSymbol *findInlinableMethod(TR::TreeTop *callTree);

   void     setGCPoint(TR::TreeTop *treeTop, TR::Node *node = NULL);
   TR::Node *resolveNode(TR::Node *node);
   bool     matchLocalLoad(TR::Node *node, Candidate *c);
   Candidate *findBaseOfIndirection(TR::Node *directBase);
   bool     isNewObject(TR::Node *node, Candidate *c);
   Candidate *findCandidateReference(TR::Node *node);
   Candidate *findCandidateReferenceInSubTree(TR::Node *node, TR_ScratchList<TR::Node> *seenNodes);
   bool     visitNode(TR::Node *node);
   void     setAffectedCandidate(Candidate *c);
   void     escapeToUserCode(Candidate *c, TR::Node *cause);
   void     escapeToUserCodeAllCandidates(TR::Node *cause, bool onlyArrays = false);
   void     escapeToGC(Candidate *c, TR::Node *cause);
   void     escapeToGC(TR::Node *cause);
   void     escapeViaCall(TR::Node *callNode);
   void     escapeViaArrayCopyOrArraySet(TR::Node *arrayCopyNode);
   void     findUninitializedWords();

   // Methods for actually changing the trees
   //
   bool    changeNewCandidates();
   void    inlineCalls();
   void    modifyTrees(Candidate *candidate);
   int32_t buildInitializationInfo(Candidate *c, TR_BitVector *wordsToBeInitialized, int32_t startWord);
   void    modifyReferences(Candidate *candidate, Candidate *startOfNextMergeSequence, Candidate *, TR::TreeTop *mergeTree);
   void    modifyReferences(Candidate *candidate, Candidate *startOfNextMergeSequence, Candidate *, TR::Node *node);

   virtual int32_t getValueNumber(TR::Node *n) = 0;

   TR::TreeTop                   *_outermostCallSite;
   TR_Array<TR::Node*>           *_parms;
   Candidate                    *_firstMergeCandidate;
   Candidate                    *_firstActiveCandidate;

   TR_LinkHead<TreeTopEntry>     _inlinedCallSites;
   TR_LinkHeadAndTail<Candidate> _candidates;

   int32_t                       _maxIterations;
   int32_t                       _maxInlinedBytecodeSize;
   int32_t                       _maxTotalInlinedBytecodeSize;
   int32_t                       _totalInlinedBytecodeSize;

   bool                          _allowMerge;
   bool                          _sniffConstructorsOnly;
   bool                          _sniffCalls;
   bool                          _removeZeroStores;
   bool                          _invalidateUseDefInfo;
   };

class TR_LocalNewInitialization : public TR_NewInitialization
   {
   public:

   TR_LocalNewInitialization(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_LocalNewInitialization(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();
   virtual int32_t getValueNumber(TR::Node *n);
   };

#endif


