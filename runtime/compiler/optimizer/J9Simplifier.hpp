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

#ifndef J9_SIMPLIFIER_INCL
#define J9_SIMPLIFIER_INCL

#include "optimizer/OMRSimplifier.hpp"

namespace J9
{
class Simplifier : public OMR::Simplifier
   {
   public:

   Simplifier(TR::OptimizationManager *manager) : OMR::Simplifier(manager)
      {}

   virtual bool isLegalToUnaryCancel(TR::Node *node, TR::Node *firstChild, TR::ILOpCodes opcode);
   virtual TR::Node *unaryCancelOutWithChild(TR::Node * node, TR::Node * firstChild, TR::TreeTop* anchorTree, TR::ILOpCodes opcode, bool anchorChildren=true);

   virtual bool isLegalToFold(TR::Node *node, TR::Node *firstChild);

   virtual bool isBoundDefinitelyGELength(TR::Node *boundChild, TR::Node *lengthChild);

   virtual TR::Node *simplifyiCallMethods(TR::Node * node, TR::Block * block);
   virtual TR::Node *simplifylCallMethods(TR::Node * node, TR::Block * block);
   virtual TR::Node *simplifyaCallMethods(TR::Node * node, TR::Block * block);

   virtual TR::Node *simplifyiOrPatterns(TR::Node *node);
   virtual TR::Node *simplifyi2sPatterns(TR::Node *node);
   virtual TR::Node *simplifyd2fPatterns(TR::Node *node);
   virtual TR::Node *simplifyIndirectLoadPatterns(TR::Node *node);

   virtual void setNodePrecisionIfNeeded(TR::Node *baseNode, TR::Node *node, TR::NodeChecklist& visited);

   private:

   bool isRecognizedPowMethod(TR::Node *node);
   bool isRecognizedAbsMethod(TR::Node *node);

   TR::Node *getUnsafeIorByteChild(TR::Node * child, TR::ILOpCodes b2iOpCode, int32_t mulConst);
   TR::Node *getLastUnsafeIorByteChild(TR::Node * child);

   TR::Node *getArrayByteChildWithMultiplier(TR::Node * child, TR::ILOpCodes b2iOpCode, int32_t mulConst);
   TR::Node *getLastArrayByteChild(TR::Node * child);
   TR::Node *getArrayBaseAddr(TR::Node * node);
   TR::Node *getArrayOffset(TR::Node * node, int32_t isubConst);

   TR::Node *getOrOfTwoConsecutiveBytes(TR::Node * ior);

   TR::Node *foldAbs(TR::Node *node);

   /*! \brief Convert System.currentTimeMillis() into currentTimeMaxPrecision
    *
    * \param node The "System.currentTimeMillis()" lcall node
    * \param block The corresponding basic block
    *
    * \return Node that points to the transformed subtree
    */
   TR::Node *convertCurrentTimeMillis(TR::Node * node, TR::Block * block);

   /*! \brief Convert System.nanoTime() into currentTimeMaxPrecision
    *
    * \param node The "System.nanoTime" lcall node
    * \param block The corresponding basic block
    *
    * \return Node that points to the transformed subtree
    */
   TR::Node *convertNanoTime(TR::Node * node, TR::Block * block);
   };

}

#endif
