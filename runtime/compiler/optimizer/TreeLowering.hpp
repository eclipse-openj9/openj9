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

   // helpers related to Valhalla value type lowering
   void lowerValueTypeOperations(TR::Node* node, TR::TreeTop* tt);
   void fastpathAcmpHelper(TR::Node* node, TR::TreeTop* tt);
   void lowerArrayStoreCHK(TR::Node* node, TR::TreeTop* tt);
   };

}

#endif
