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

#ifndef ESCAPEANALYSISTOOLS_INCL
#define ESCAPEANALYSISTOOLS_INCL

#include <stdint.h>                           // for int32_t
#include "optimizer/Optimization.hpp"         // for Optimization
#include "optimizer/OptimizationManager.hpp"  // for OptimizationManager
#include "optimizer/EscapeAnalysis.hpp"
#include "il/SymbolReference.hpp"

namespace TR { class Block; class Node; }

class TR_EscapeAnalysisTools
   {
   public:
   TR_EscapeAnalysisTools(TR::Compilation *comp);

// TODO:  Bring Doxygen comments up to date
   /**
    * Create a "fake" call to the \c escapeHelper gathering references to all
    * live autos and pending pushes.
    *
    * \param[in] block An OSR induce block containing the call represented by
    *            \c induceCall.  The fake \c escapeHelper call that is
    *            generated will be added at the end of this block.
    * \param[in] induceCall The call to the OSR induction helper in \c block.
    *            Used as the source of byte code info for the fake
    *            \c escapeHelper
    */
   void insertFakeEscapeForOSR(TR::Block *block, TR::Node *induceCall);


   void insertFakeEscapeForLoads(TR::Block *block, TR::Node *node, TR_BitVector &symRefsToLoad);
   static bool isFakeEscape(TR::Node *node) { return node->isEAEscapeHelperCall(); }
   private:
   TR::Compilation *_comp;

   /**
    * Gather live autos and pending pushes at the point of a call to the OSR
    * induction helper in the context of an inlined method or the outermost
    * method.
    *
    * The symbol reference numbers are gathered in \ref symRefsToLoad.
    *
    * \param[in] rms The method whose live autos and pending pushes are to be
    *                processed
    * \param[in] induceDefiningMap \ref DefiningMap mapping from live symbol
    *                references captured during the OSR def liveness analysis
    *                to symbol references defining those symbols in the current
    *                trees
    * \param[in] methodData \ref TR_OSRMethodData at this point in the
    *                relevant method
    * \param[in] byteCodeIndex The bytecode index at this point inside the
    *                relevant method
    * \param[inout] symRefsToLoad A \ref TR_BitVector of the symbol reference
    *                numbers of live autos and pending pushes
    */
   void processAutosAndPendingPushes(TR::ResolvedMethodSymbol *rms, DefiningMap *induceDefiningMap, TR_OSRMethodData *methodData, int32_t byteCodeIndex, TR_BitVector &symRefsToLoad);

   /**
    * Gather the set of live symbol reference among those listed in the
    * \c symbolReferences argument.
    *
    * The symbol reference numbers are gathered in \ref symRefsToLoad.
    *
    * \param[in] symbolReferences \ref TR_Array<> of symbol references for
    *               autos or pending pushes
    * \param[in] induceDefiningMap \ref DefiningMap mapping from live symbol
    *               references captured during the OSR def liveness analysis
    *               to symbol references defining those symbols in the current
    *               trees
    * \param[in] deadSymRefs Liveness info for \c symbolReferences - a
    *               \ref TR_BitVector of symbols that are not live at the
    *               relevant point
    * \param[inout] symRefsToLoad A \ref TR_BitVector of the symbol reference
    *                numbers of live autos and pending pushes
    */
   void processSymbolReferences(TR_Array<List<TR::SymbolReference>> *symbolReferences, DefiningMap *induceDefiningMap, TR_BitVector *deadSymRefs, TR_BitVector &symRefsToLoad);
   };

#endif
