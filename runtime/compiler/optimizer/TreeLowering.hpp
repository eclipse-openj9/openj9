/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#ifndef TREELOWERING_INCL
#define TREELOWERING_INCL

#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
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

   /**
    * @brief Moves a node down to the end of a block
    *
    * Any stores of the value of the node are also moved down.
    *
    * This can be useful to do after a call to splitPostGRA where, as part of un-commoning,
    * it is possible that code to store the anchored node into a register or temp-slot is
    * appended to the original block.
    *
    * @param block is the block containing the TreeTop to be moved
    * @param tt is a pointer to the TreeTop to be moved
    */
   void moveNodeToEndOfBlock(TR::Block* const block, TR::TreeTop* const tt, TR::Node* const node);

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

   // helpers related to Valhalla value type lowering
   void lowerValueTypeOperations(TR::PreorderNodeIterator& nodeIter, TR::Node* node, TR::TreeTop* tt);
   void fastpathAcmpHelper(TR::PreorderNodeIterator& nodeIter, TR::Node* const node, TR::TreeTop* const tt);
   void lowerArrayStoreCHK(TR::Node* node, TR::TreeTop* tt);
   };

}

#endif
