/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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
class NeedsPeekingHeuristic;

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
    *     The inliner weight adjustment factor used for `java/lang/reflect/Method.invoke`.
    */
   static const float METHOD_INVOKE_ADJUSTMENT_FACTOR;

   /** \brief
    *     Adjusts the estimated \p value by a \p factor for string compression related methods.
    *  \param method
    *     The method we are trying to make an estimate adjustment for.
    *  \param value
    *     The estimated value we are trying to adjust.
    *  \param factor
    *     The factor multiplier to adjust the value by.
    *  \return
    *     true if the \p value was adjusted by the \p factor for the specific \p method; false otherwise.
    *  \note
    *     If an adjustment is performed the formula used to calculate the new value is:
    *     \code
    *     value *= factor;
    *     \endcode
    */
   static bool adjustEstimateForStringCompression(TR_ResolvedMethod* method, int32_t& value, float factor);

   /** \brief
    *     Adjusts the estimated \p value by a \p factor for `java/lang/reflect/Method.invoke`.
    *  \param method
    *     The method we are trying to make an estimate adjustment for.
    *  \param value
    *     The estimated value we are trying to adjust.
    *  \param factor
    *     The factor multiplier to adjust the value by.
    *  \return
    *     true if the \p value was adjusted by the \p factor for the specific \p method; false otherwise.
    *  \note
    *     If an adjustment is performed the formula used to calculate the new value is:
    *     \code
    *     value *= factor;
    *     \endcode
    */
   static bool adjustEstimateForMethodInvoke(TR_ResolvedMethod* method, int32_t& value, float factor);

   static TR::Block *getBlock(TR::Compilation *comp, TR::Block * * blocks, TR_ResolvedMethod *feMethod, int32_t i, TR::CFG & cfg);

   static void setupNode(TR::Node *node, uint32_t bcIndex, TR_ResolvedMethod *feMethod, TR::Compilation *comp);
   static void setupLastTreeTop(TR::Block *currentBlock, TR_J9ByteCode bc,
                             uint32_t bcIndex, TR::Block *destinationBlock, TR_ResolvedMethod *feMethod,
                             TR::Compilation *comp);

   protected:
      bool estimateCodeSize(TR_CallTarget *, TR_CallStack * , bool recurseDown = true);

     /** \brief
      *     Generates a CFG for the calltarget->_calleeMethod.
      *
      *  \param calltarget
      *     The calltarget which we wish to generate a CFG for.
      *
      *  \param cfgRegion
      *     The memory region where the cfg is going to be stored
      *
      *  \param bci
      *     The bytecode iterator. Must be instantiated in the following way:
      *     \code
      *        bci(0, static_cast<TR_ResolvedJ9Method *> (calltarget->_calleeMethod), ...)
      *     \endcode
      *
      *  \param nph
      *     Pointer to NeedsPeekingHeuristic.
      *
      *  \param blocks
      *     Array of block pointers. Size of array must be equal to the maximum
      *     bytecode index in calltarget->_calleeMethod
      *
      *  \param flags
      *     Array of flags8_t. Size of array must be equal to maximum bytecode
      *     index in calltarget->_calleeMethod
      *
      *  \return
      *     Reference to cfg
      */
      TR::CFG &processBytecodeAndGenerateCFG(TR_CallTarget *calltarget, TR::Region &cfgRegion, TR_J9ByteCodeIterator &bci, NeedsPeekingHeuristic &nph, TR::Block** blocks, flags8_t * flags);
      bool realEstimateCodeSize(TR_CallTarget *calltarget, TR_CallStack *prevCallStack, bool recurseDown, TR::Region &cfgRegion);

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

#define NUM_PREV_BC 5
class TR_prevArgs
{
   public:
      TR_prevArgs() { for (int32_t i = 0 ; i < NUM_PREV_BC ; i++ ) { _prevBC[i] = J9BCunknown ; } }

      void printIndexes(TR::Compilation *comp)
         {
         for (int32_t i = 0 ; i < NUM_PREV_BC ; i++)
            {
            if(comp->getDebug())
               traceMsg(comp,"_prevBC[%d] = %s\n" ,i,((TR_J9VM*)(comp->fej9()))->getByteCodeName(_prevBC[i]));
            }
         }

      void updateArg(TR_J9ByteCode bc )
      {
      for(int32_t i=NUM_PREV_BC-2 ; i>=0 ; i-- )
         {
         _prevBC[i+1] = _prevBC[i];
         }
      _prevBC[0] = bc;
      }

      bool isArgAtIndexReceiverObject (int32_t index)
         {
         if (  index < NUM_PREV_BC && _prevBC[index] == J9BCaload0)
            {
            return true;
            }
         else
            return false;
         }

      int32_t getNumPrevConstArgs(int32_t numparms)
      {
      int32_t count=0;

      for(int32_t i=0 ; i < NUM_PREV_BC && i < numparms ; i++)
         {
         switch (_prevBC[i])
             {
             case J9BCaconstnull:
             case J9BCiconstm1:
             case J9BCiconst0:
             case J9BCiconst1:
             case J9BCiconst2:
             case J9BCiconst3:
             case J9BCiconst4:
             case J9BCiconst5:
             case J9BClconst0:
             case J9BClconst1:
             case J9BCfconst0:
             case J9BCfconst1:
             case J9BCfconst2:
             case J9BCdconst0:
             case J9BCdconst1:
             case J9BCldc: case J9BCldcw: case J9BCldc2lw: case J9BCldc2dw:
             case J9BCbipush: case J9BCsipush:
                count++;
                break;
             default:
            	 break;
             }
         }
      return count;
      }


   protected:
      TR_J9ByteCode _prevBC[NUM_PREV_BC];
};




#endif
