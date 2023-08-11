/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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

#ifndef TREELOWERING_INCL
#define TREELOWERING_INCL

#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/deque.hpp"
#include "infra/ILWalk.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"

namespace TR
{

/**
 * @brief An optimization to lower trees post-GRA in the optimizer
 *
 * This optimization is designed to perform lowering in the optimizer after
 * GRA has run. As such, any introduction of new control flow must use
 * TR::Block::splitPostGRA() and related methods. It should be fairly early
 * on after GRA in order to allow other late optimizations to clean up the
 * lowered trees. In particular, it should be run before optimizations such
 * as globalLiveVariablesForGC that compute information that can be affected
 * by the introduction of new control flow.
 */
class TreeLowering : public TR::Optimization
   {
   public:

   /**
    * @brief Abstract class serving as interface for callbacks that apply a transformation.
    *
    * Transformations use this class as an interface to invoke Transformer callbacks. Callbacks
    * should be implemented by extending this class and overriding lower().
    */
   class Transformer
      {
      public:
      explicit Transformer(TR::TreeLowering* treeLoweringOpt)
         : _comp(treeLoweringOpt->comp())
         , _treeLoweringOpt(treeLoweringOpt)
         {}

      TR::Compilation* comp() { return _comp; }

      TR::Optimizer* optimizer() { return _treeLoweringOpt->optimizer(); }

      bool trace() { return _treeLoweringOpt->trace(); }

      const char* optDetailString() { return _treeLoweringOpt->optDetailString(); }

      void prepareToReplaceNode(TR::Node * node) { _treeLoweringOpt->prepareToReplaceNode(node); }

      /**
       * @brief Main callback method to apply a transformation.
       *
       * Derived classes must override this method with the appropriate code
       * to apply the transformation given some input.
       *
       * @param node the node where the transformation will happen
       * @param tt the \ref TR::TreeTop instance at the root of the tree containing the node
       */
      virtual void lower(TR::Node* const node, TR::TreeTop* const tt) = 0;

      protected:
       /**
       * @brief Moves a TreeTop down to the end of a block
       *
       * Any stores of the value of the node are also moved down.
       *
       * This can be useful to do after a call to splitPostGRA where, as part of un-commoning,
       * it is possible that code to store the anchored node into a register or temp-slot is
       * appended to the original block.
       *
       * @param block is the block containing the TreeTop to be moved
       * @param tt is a pointer to the TreeTop to be moved
       * @param node is a pointer to the node that stores a value
       * @param isAddress shows if the node is address, otherwise it is assumed to be an integer
       */
      void moveNodeToEndOfBlock(TR::Block* const block, TR::TreeTop* const tt, TR::Node* const node, bool isAddress);

      /**
       * @brief Split a block after having inserted a fastpath branch
       *
       * The function should be used to split a block after a branch has been inserted.
       * After the split, the resulting fall-through block is marked as an extension of
       * the previous block (the original block that was split), which implies that
       * uncommoning is not required. The cfg is also updated with an edge going from
       * the original block to some target block, which should be the same as the
       * target of the branch inserted before the split.
       *
       * Note that this function does not call TR::CFG::invalidateStructure() as it assumes
       * the caller is using this function in a context where TR::CFG::invalidateStructure()
       * is likely to have already been called.
       *
       * @param block is the block that will be split
       * @param splitPoint is the TreeTop within block at which the split must happen
       * @param targetBlock is the target block of the branch inserted before the split point
       * @return TR::Block* the (fallthrough) block created from the split
       */
      TR::Block* splitForFastpath(TR::Block* const block, TR::TreeTop* const splitPoint, TR::Block* const targetBlock);

      private:
      TR::Compilation* _comp;
      TR::TreeLowering* _treeLoweringOpt;
      };

   private:
   /**
    * @brief A class for collecting transformations to be performed.
    *
    * This class encapsulates the basic functionality for "delaying"
    * transformations in TreeLowering. It allows "future transformations"
    * to be collected and then performed consecutively in bulk later on.
    */
   class TransformationManager
      {
      public:

      /**
       * @brief Construct a new TransformationManager object
       *
       * @param allocator is the \ref TR::Region instance used to do allocations internally
       */
      explicit TransformationManager(TR::Region& allocator) : _transformationQueue(allocator) {}

      /**
       * @brief Add a transformation to be performed
       *
       * @param transformer the Transformer object that acts as callback for the transformation
       * @param node the node where the transformation will happen
       * @param tt the \ref TR::TreeTop instance at the root of the tree containing the node
       */
      void addTransformation(Transformer* transformer, TR::Node* const node, TR::TreeTop* const tt)
         {
         _transformationQueue.push_back(Transformation(transformer, node, tt));
         }

      /**
       * @brief Perform all accumulated transformations.
       *
       * The transformations are performed in sequence but no guarantees are made
       * about the exact order in which it happens.
       */
      void doTransformations()
         {
         while (!_transformationQueue.empty())
            {
            auto transformation = _transformationQueue.front();
            _transformationQueue.pop_front();
            transformation.doTransformation();
            }
         }

      private:

      /**
       * @brief A class representing an IL transformation.
       *
       * This class encapsulates the different pieces needed to represent
       * and perform a transformation.
       *
       * Conceptually, a transformation is made up of two parts:
       *
       * 1. A function (callback) that applies the transformation
       *    given some input.
       * 2. The set of input arguments for the given transformation.
       *
       * Collectively, these pieces form a closure that will perform the
       * transformation when invoked.
       *
       * In this implementation, the callback is represented by an instance
       * of Transformer. The arguments are the \ref TR::Node and
       * \ref TR::TreeTop instances. The transformation is performed by
       * invoking doTransformation().
       */
      struct Transformation
         {
         Transformer* transformer;
         TR::Node* node;
         TR::TreeTop* tt;

         Transformation(Transformer* transform, TR::Node* n, TR::TreeTop* t)
           : transformer(transform), node(n), tt(t)
           {}
         inline void doTransformation();
         };

      TR::deque<Transformation, TR::Region&> _transformationQueue;
      };

   public:

   explicit TreeLowering(TR::OptimizationManager* manager)
      : TR::Optimization(manager)
      {}

   static TR::Optimization* create(TR::OptimizationManager* manager)
      {
      return new (manager->allocator()) TreeLowering(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   private:
   // helpers related to Valhalla value type lowering
   void lowerValueTypeOperations(TransformationManager& transformation, TR::Node* node, TR::TreeTop* tt);

   template <typename T>
   Transformer* getTransformer() { return new (comp()->region()) T(this); }
   };

void
TR::TreeLowering::TransformationManager::Transformation::doTransformation()
   {
   transformer->lower(node, tt);
   }

}

#endif
