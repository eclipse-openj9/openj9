/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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

#ifndef STATICFINALFIELDFOLDING_INCL
#define STATICFINALFIELDFOLDING_INCL

#include <stdint.h>
#include "il/Node.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"

namespace TR { class NodeChecklist; }

/** \brief
 *
 * This opt walks the trees and folds direct loads of static final fields.
 *
 * \details
 *
 * This opt walks the trees and tries to fold direct loads of static final fields.
 * For a primitive type, it will be folded into a primitive constant. For a reference
 * type, a known object index is assigned to the referenced object, the symref on
 * the load node is improved to one with the known object index.
 *
 * There are two types of static final fields this opt is trying to fold:
 *  1. Fields in special JCL classes that are closely tied to VM core implementation,
 *     these fields are not expected to be modified once initialized.
 *     `J9::TransformUtil::foldFinalFieldsIn` contains a list of such classes.
 *
 *  2. Other fields that may be modified through reflection or Unsafe APIs.
 *
 * For 1, they're always directly folded into a constant. The folding doesn't require any
 * analysis or protection.
 *
 * For 2, they can only be folded with the assumption that they're not to be modified.
 * However, when modification happens, the JIT code with the invalid folded value must
 * not be executed. Thus, the folding of type 2 final statics requires guards. We use
 * OSR guards to protect ourselves in such situations. When modification happens, the
 * OSR guard will be patched to induce OSR transition to the interpreter.
 *
 * To be able to do guarded static final field folding (to fold the second type of fields),
 * we have to make sure that an OSR guard can be added after any yield point (place where
 * invaliation of assumptions can happen) that can reach the folded field. This is because
 * if invalidation happens, we have to get out of the JIT body before running into the code
 * with the invalid folded value.
 *
 * However, only yield points with OSR support can have OSR guard added after. They are places
 * where we have the bookkeeping to reconstruct the operand stack and local slots such that
 * the interpreter can resume the execution from where the JIT code left. If a type 2 static
 * final field can be reached by a yield point without OSR support, it can't be folded, as
 * there exists a path where invalidation happens but we can't avoid running into the invalid
 * code.
 *
 * Whether a load of type 2 static final field can be folded can be determined by data flow
 * analysis, which is expensive. To avoid doing the analysis, there are a few simple checks we
 * do, which is done in `safeToAddFearPointAt` in J9TransformUtil.cpp. The result of these
 * checks is more conservative than that from a data flow analysis, but it will guarantee
 * functional correctness.
 *
 * After a type 2 field is folded, a call to `osrFearPointHelper` is inserted before the tree
 * with the load of the field. The helper call marks where a type 2 field is folded and is
 * a fear point in FearPointAnalysis, which runs in OSRGuardInsertion to decide the placement
 * of OSR guards.
 *
 * The folding is done in a transform util `J9::TransformUtil::attemptGenericStaticFinalFieldFolding`.
 * It will always fold type 1 final statics, and type 2 final statics when it is safe to do so.
 * Type 2 field folding requires OSR guards to be inserted, so it can only run before OSRGuardInsertion.
 *
 * Type 1 final statics are folded by default in all compilations at any opt level.
 *
 * For type 2 final statics folding, it is only enabled when voluntary OSR is on, which is turned
 * on by nextGenHCR. nextGenHCR is on by default except in a small number of compiles when the
 * compiled method has been redefined too many times or during startup time where nextGenHCR
 * is considered too expensive.
 */

class TR_StaticFinalFieldFolding : public TR::Optimization
   {
   TR::NodeChecklist *_checklist;
   void visitNode(TR::TreeTop * currentTree, TR::Node *node);

   public:

   TR_StaticFinalFieldFolding(TR::OptimizationManager *manager)
      : TR::Optimization(manager)
      {}
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_StaticFinalFieldFolding(manager);
      }
   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   };
#endif
