/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef REDUCE_SYNCHRONIZED_FIELD_LOAD_NODE
#define REDUCE_SYNCHRONIZED_FIELD_LOAD_NODE

#include "codegen/CodeGenerator.hpp"  // for CodeGenerator
#include "compile/Compilation.hpp"    // for Compilation
#include "env/TRMemory.hpp"           // for TR_Memory, etc
#include "il/Node.hpp"                // for vcount_t
#include "infra/List.hpp"             // for List
#include "infra/ILWalk.hpp"

/** \brief
 *     Reduces synchronized regions around a single field load into a codegen inlined recognized method call which
 *     generates a hardware optimized instruction sequence which is semantically equivalent to the synchronized field
 *     load.
 *
 *  \details
 *     This codegen phase pass pattern matches the following tree sequences (modulo NOP trees such as aRegLoad):
 *
 *     \code
 *     monent
 *       object
 *     aloadi / iloadi / i2a
 *       ==> object
 *     monexitfence
 *     monexit
 *       ==> object
 *     \endcode
 *
 *     And replaces the entire treetop sequence, excluding the monexitfence, with a call to a codegen inlined method
 *     <synchronizedFieldLoad>.
 */
class ReduceSynchronizedFieldLoad
   {
   public:

   TR_ALLOC(TR_Memory::CodeGenerator)

   /** \brief
    *     Initializes the ReduceSynchronizedFieldLoad codegen phase.
    *
    *  \param cg
    *     The code generator used to generate the instructions.
    */
   ReduceSynchronizedFieldLoad(TR::CodeGenerator* cg)
      : cg(cg)
      {
      // Void
      }

   /** \brief
    *     Inlines a fast synhronized field load sequence using a load pair disjoint (LPD) instruction.
    *
    *  \param node
    *     The node which encapsulates the reduced tree sequence via an inlined call node. The node must have the
    *     following shape:
    *
    *     call <synchronizedFieldLoad>
    *       object
    *       aloadi / iloadi / lloadi / l2a
    *       iconst
    *       call jitMonitorEntry
    *          ==>object
    *       call jitMonitorExit
    *          ==>object
    *
    *     It carries five children:
    *
    *     1. The object on which we are synchronizing
    *     2. The field which we are to load
    *     3. The offset (in bytes) from the synchronized object to its lock word
    *     4. The symbol reference of the original monent in the synchronized region
    *     5. The symbol reference of the original monexit in the synchronized region
    *
    *     If the child is an l2a it signifies a compressedRefs reference field load.
    *
    *  \param cg
    *     The code generator used to generate the instructions.
    *
    */
   static void inlineSynchronizedFieldLoad(TR::Node* node, TR::CodeGenerator* cg);

   /** \brief
    *     Performs the optimization on this compilation unit.
    *
    *  \return
    *     true if any transformation was performed; false otherwise.
    */
   bool perform();

   /** \brief
    *     Performs the optimization on this compilation unit.
    *
    *  \param startTreeTop
    *     The tree top on which to begin looking for opportunities to perform this optimization.
    *
    *  \param endTreeTop
    *     The tree top on which to end looking for opportunities to perform this optimization.
    *
    *  \return
    *     true if any transformation was performed; false otherwise.
    */
   bool performOnTreeTops(TR::TreeTop* startTreeTop, TR::TreeTop* endTreeTop);

   private:

   /** \brief
    *     Advances \p iter one tree top forward, ignoring extended basic block boundaries by skipping over
    *     <c>TR::BBStart</c> and <c>TR::BBEnd</c> nodes, and while taking into account the \p endTreeTop.
    *
    *  \param iter
    *     The iterator used to look for opportunities to advance forward.
    *
    *  \param endTreeTop
    *     The tree top on which to end looking for opportunities to perform this optimization.
    *
    *  \return
    *     true if the iterator was successfully advanced and the search for optimization opportunity should continue;
    *     false otherwise.
    */
   bool advanceIterator(TR::TreeTopIterator& iter, TR::TreeTop* endTreeTop);

   /** \brief
    *     The cached code generator used to generate the instructions.
    */
   TR::CodeGenerator * cg;
   };

#endif
