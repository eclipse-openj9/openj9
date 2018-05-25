/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#ifndef J9ESTIMATECS_INCL
#define J9ESTIMATECS_INCL

#include "il/Block.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "compile/Compilation.hpp"
#include "optimizer/Inliner.hpp"
#include "optimizer/J9Inliner.hpp"
#include "il/Node.hpp"
#include "infra/Stack.hpp"
#include "il/TreeTop.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/TRMemory.hpp"
#include "optimizer/EstimateCodeSize.hpp"

class TR_ResolvedMethod;

class TR_J9EstimateCodeSize : public TR_EstimateCodeSize
   {
   public:

      TR_J9EstimateCodeSize() : TR_EstimateCodeSize(), _optimisticSize(0), _lastCallBlockFrequency(-1) { }

      int32_t getOptimisticSize()       { return _optimisticSize; }
      
   /** \brief
    *     The inliner weight adjustment factor used for java/lang/String* compression related methods.
    */
   static const float STRING_COMPRESSION_ADJUSTMENT_FACTOR;

   /** \brief
    *     Adjusts the estimated \p value by a \p factor for string compression related methods.
    *
    *  \param method
    *     The method we are trying to make an estimate adjustment for.
    *
    *  \param value
    *     The estimated value we are trying to adjust.
    *
    *  \param factor
    *     The factor multiplier to adjust the value by.
    *
    *  \return
    *     true if the \p value was adjusted by the \p factor for the specific \p method; false otherwise.
    *
    *  \note
    *     If an adjustment is performed the formula used to calculate the new value is:
    *
    *     \code
    *     value *= factor;
    *     \endcode
    */
   static bool adjustEstimateForStringCompression(TR_ResolvedMethod* method, int32_t& value, float factor);

   protected:
      bool estimateCodeSize(TR_CallTarget *, TR_CallStack * , bool recurseDown = true);
      bool realEstimateCodeSize(TR_CallTarget *calltarget, TR_CallStack *prevCallStack, bool recurseDown);

      bool reduceDAAWrapperCodeSize(TR_CallTarget* target);

      // Partial Inlining Logic
      bool isInExceptionRange(TR_ResolvedMethod * feMethod, int32_t bcIndex);
      bool trimBlocksForPartialInlining (TR_CallTarget *calltarget, TR_Queue<TR::Block> *);
      bool isPartialInliningCandidate(TR_CallTarget *calltarget, TR_Queue<TR::Block> *);
      bool graphSearch( TR::CFG *, TR::Block *, TR::Block::partialFlags, TR::Block::partialFlags);
      int32_t labelGraph( TR::CFG *, TR_Queue<TR::Block> *, TR_Queue<TR::Block> *);
      void processGraph(TR_CallTarget * );


      int32_t _lastCallBlockFrequency;
      int32_t _optimisticSize;          // size if we assume we are doing a partial inline
   };


#endif
