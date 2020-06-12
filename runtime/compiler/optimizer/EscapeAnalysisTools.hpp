/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#ifndef ESCAPEANALYSISTOOLS_INCL
#define ESCAPEANALYSISTOOLS_INCL

#include <stdint.h>                           // for int32_t
#include "optimizer/Optimization.hpp"         // for Optimization
#include "optimizer/OptimizationManager.hpp"  // for OptimizationManager
#include "optimizer/EscapeAnalysis.hpp"
#include "il/SymbolReference.hpp"

namespace TR { class Block; class Node; }

//typedef TR::typed_allocator<TR::Node *, TR::Region &> NodeDequeAllocator;
//typedef std::deque<TR::Node *, NodeDequeAllocator> NodeDeque;

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


   void insertFakeEscapeForLoads(TR::Block *block, TR::Node *node, NodeDeque *loads);
   //static bool isFakeEscape(TR::Node *node) { return node->getSymbolReference()->getReferenceNumber() == TR_prepareForOSR;}
   static bool isFakeEscape(TR::Node *node) { return node->isEAEscapeHelperCall(); }
   private:
   TR::Compilation *_comp;

   /**
    * Used by \ref insertFakePrepareForOSR to gather \c aloads of autos and
    * pending pushes.
    */
   NodeDeque *_loads;

   /**
    * Gather live autos and pending pushes at the point of a call to the OSR
    * induction helper in the context of an inlined method or the outermost
    * method.  Create an \c aload reference to each such auto or pending push.
    *
    * \c aloads are gathered in \ref _loads.
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
    */
   void processAutosAndPendingPushes(TR::ResolvedMethodSymbol *rms, DefiningMap *induceDefiningMap, TR_OSRMethodData *methodData, int32_t byteCodeIndex);

   /**
    * Create \c aload references to each live symbol reference among those
    * listed in the \c symbolReferences argument.
    *
    * \c aloads are gathered in \ref _loads.
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
    */
   void processSymbolReferences(TR_Array<List<TR::SymbolReference>> *symbolReferences, DefiningMap *induceDefiningMap, TR_BitVector *deadSymRefs);
   };

#endif

