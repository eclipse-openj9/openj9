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

#ifndef LOOPALIASREFINER_INCL
#define LOOPALIASREFINER_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"
#include "il/Node.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/List.hpp"
#include "optimizer/LoopVersioner.hpp"
#include "optimizer/OptimizationManager.hpp"

class TR_InductionVariable;
class TR_RegionStructure;
namespace TR { class Block; }
namespace TR { class Compilation; }
namespace TR { class Optimization; }
namespace TR { class TreeTop; }


/*
 * Class TR_LoopAliasRefiner
 * =========================
 *
 * Loop alias refiner is an optimization that aims to avoid aliasing array accesses 
 * that are based on arrays known to be different.
 *
 * The IL generator in Java only uses a single array shadow symbol per datatype 
 * and so all the IL trees for array accesses of that datatype use that symbol. 
 * This coveys the possibility to the optimizer that two different array accesses 
 * could in fact be affecting the same memory location even if different variables 
 * are used to do the accesses, e.g. a[i] and b[j] could both be referring to the 
 * same location if the variables a and b point at the same array and i and j 
 * have the same value. i.e. using the same symbol is the simplest way to convey 
 * that the accesses may be aliased. However, this is also quite conservative in that 
 * we may have been able to use two completely independent symbol references for 
 * the two array accesses if we knew the arrays were different for example; 
 * this may then have given greater flexibility to the optimizer to move those 
 * array accesses since they would no longer appear to interfere with each other.
 * 
 * Loop alias refiner creates versioning tests outside the loop to check if 
 * the arrays used in array accesses are different, i.e. the optimization emits 
 * a test akin to : if (a != b) where a and b are two loop invariant array 
 * variables involved in array accesses of the same datatype inside the loop. 
 * In the "fast" version of the loop (executed if a != b) the array 
 * accesses are changed to use new array shadow symbol references that 
 * are not aliased to each other (but aliased to the original single array 
 * shadow symbol of that datatype) whereas the "slow" version of the loop 
 * is left unchanged (and therefore has the original conservative aliasing 
 * by virtue of all array accesses using the original array shadow symbol).
 * 
 * Thus, loop alias refiner acts as an enabler for later optimizations to 
 * take advantage of the transformations it does to refine the aliasing 
 * on array accesses inside loops.
 */

class TR_LoopAliasRefiner: public TR_LoopVersioner 
   {
   public: 
   TR_LoopAliasRefiner(TR::OptimizationManager *manager);

   virtual const char * optDetailString() const throw();
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_LoopAliasRefiner(manager);
      }

   bool processArrayAliasCandidates();
   void collectArrayAliasCandidates(TR::Node *node, vcount_t visitCount);
   void buildAliasRefinementComparisonTrees(List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, List<TR::TreeTop> *, TR_ScratchList<TR::Node> *, TR::Block *);
   void initAdditionalDataStructures();
   void refineArrayAliases(TR_RegionStructure *);
   bool hasMulShadowTypes(TR_ScratchList<TR_NodeParentBlockTuple> *candList);

   /* 
    * Used to represent expression trees in terms of IVs.
    * A given index expression will result in an ordered list of
    * IVExpr nodes.  e.g.
    * intArray[dimsize1*(j-1+dimsize2*(i+1))-k] (aka intArray[i+1, j-1, -k]
    * would result in a list like:
    *  element       node                               equiv expr
    *   1       [iv:i, scale:dimsize1*dimsize2, invar:+1]    (i+1) * dimsize1* dimsize2
    *   2       [iv:j, scale:dimsize1, invar:-1, invar:-1]     (j-1) *dimsize1
    *   3       [iv:k, scale:NULL, invar:NULL, isSub=true]      -k

    * note that the scaling factor for integer (4) and any constant offset will be represented in
    * CanonicalArrayReference,  since the basic form of the array must be followed when 
    * reconstructing the full array expression.
    */
   class IVExpr 
      {
      public:
      TR_ALLOC(TR_Memory::LoopAliasRefiner);

      IVExpr():_ivRef(NULL), _scalingExpr(NULL), _invariantExpr(NULL), _isSub(false){}

      IVExpr(TR::SymbolReference *ref, TR::Node *scale, TR::Node *constExpr, bool b):
         _ivRef(ref), _scalingExpr(scale), _invariantExpr(constExpr), _isSub(b){}
      TR::SymbolReference *getIVReference() { return _ivRef; }

      void dump(TR::Compilation *comp){
	 traceMsg(comp, "\tiv:#%d scalingNode:%p invariantExpr:%p %s\n", 
		  _ivRef?_ivRef->getReferenceNumber():-1, 
		  _scalingExpr, 
		  _invariantExpr, 
		  (_isSub?"is-subtract":"is-add"));
      }

      TR::Node * generateExpr(TR::Compilation *comp, TR::Node *ivExpr);
      TR::SymbolReference * getIVRef() { return _ivRef; }
      TR::Node * getScalingExpr() { return _scalingExpr; }
      TR::Node *getInvariantExpr() { return _invariantExpr; }
      void setInvariantExpr(TR::Node *n){ _invariantExpr = n;}
      bool isSub() { return _isSub; }
      
      private:
      TR::SymbolReference * _ivRef;
      TR::Node *            _invariantExpr;
      TR::Node *            _scalingExpr;
      bool                 _isSub;
      };


   class IVValueRange
      {
      public:
      TR_ALLOC(TR_Memory::LoopAliasRefiner);
      IVValueRange(TR_InductionVariable *iv, TR::Node *smallest, TR::Node *largest) :
         _iv(iv), _smallestValue(smallest), _largestValue(largest){}
      TR::Node * getSmallestValue() { return _smallestValue;}
      TR::Node *getLargestValue() { return _largestValue; }
      TR_InductionVariable * getIV(){ return _iv; }
      private:
      TR_InductionVariable *_iv;
      TR::Node * _largestValue;
      TR::Node * _smallestValue;
      };

   class ArrayRangeLimits
      {
      public:
      TR_ALLOC(TR_Memory::LoopAliasRefiner);

      ArrayRangeLimits( TR_ScratchList<TR_NodeParentBlockTuple> *candList, TR::SymbolReference *baseSymRef, 
                        TR::SymbolReference *memberSymRef, TR::SymbolReference *arrayAccessSymRef):
                        _arrayAddrRef(baseSymRef), _arrayDerefSym(memberSymRef), _candidateList(candList), _arrayAccessSymRef(arrayAccessSymRef) {}

      TR_ScratchList<TR_NodeParentBlockTuple> *getCandidateList() { return _candidateList;}
      TR::SymbolReference * getBaseSymRef() { return _arrayAddrRef; }
      TR::SymbolReference * getMemberSymRef() { return _arrayDerefSym; }
      TR::SymbolReference * getArrayAccessSymRef() { return _arrayAccessSymRef; }
      TR::Node * getMinValueExpr() { return _minValue; }
      TR::Node *getMaxValueExpr() { return _maxValue;}
      TR::Node *createRangeTestExpr(TR::Compilation *, ArrayRangeLimits *other, TR::Block *, bool trace);

      private:
      TR::SymbolReference * _arrayAddrRef;
      union 
         {
         TR::SymbolReference * _arrayDerefSym;
         TR::Node *_minValue;
         };
      TR::Node *_maxValue; 
      TR_ScratchList<TR_NodeParentBlockTuple> *_candidateList;
      TR::SymbolReference * _arrayAccessSymRef;
      };

   class CanonicalDimension 
      { 
      public:
      TR_ALLOC(TR_Memory::LoopAliasRefiner);
      
      };
   class CanonicalArrayReference
      {  
      public:
      TR_ALLOC(TR_Memory::LoopAliasRefiner);
      CanonicalArrayReference() :_arrayAddrRef(NULL), _nonIVExpr(NULL), 
                                 _origExpr(NULL){}

      CanonicalArrayReference(const CanonicalArrayReference &src,  TR::Compilation *comp);
          

      void dump(TR::Compilation* comp) 
            {
            traceMsg(comp, "Ref:#%d nonIVExpr:%p OrigExpr:%p\n", 
                    _arrayAddrRef->getReferenceNumber(), 
                    _nonIVExpr, 
                    _origExpr);
            TR_ASSERT(_ivExprs, "_ivexprs is null");
            ListIterator<IVExpr> rangeIterator(_ivExprs);
            IVExpr *ar;
            for(ar = rangeIterator.getFirst();ar;ar= rangeIterator.getNext())
               {
               ar->dump(comp);
               }
            }

      TR::SymbolReference *        _arrayAddrRef;
      TR::Node *                   _origExpr;
      TR::Node *                   _nonIVExpr;
      TR_ScratchList<IVExpr>     *_ivExprs;
      };

   private:
   void collectArrayAliasCandidates(TR::Node *, TR::Node*,  vcount_t visitCount, bool isStore);

   void markAsProcessed(int32_t loopID){ _processedLoops->set(loopID);}
   bool isAlreadyProcessed(int32_t loopID) { return _processedLoops->isSet(loopID);}

   TR_ScratchList<ArrayRangeLimits>   *_arrayRanges;
   TR_ScratchList<TR::SymbolReference> *_independentArrays;
   TR_BitVector                       *_processedLoops;
   bool                                _supportArrayMembers;

   };
#endif
