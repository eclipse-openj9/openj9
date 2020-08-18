/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef J9_VALUEPROPAGATION_INCL
#define J9_VALUEPROPAGATION_INCL

#include "optimizer/OMRValuePropagation.hpp"
#include "infra/List.hpp"

namespace TR { class VP_BCDSign; }

namespace J9
{

class ValuePropagation : public OMR::ValuePropagation
   {
   public:

   ValuePropagation(TR::OptimizationManager *manager);

 #if (defined(LINUX) && ( defined(TR_TARGET_X86) || defined(TR_TARGET_S390)))
  #if __GNUC__ > 4 || \
   (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)
   TR::VP_BCDSign **getBCDSignConstraints(TR::DataType dt) __attribute__((optimize(1)));
  #else
   TR::VP_BCDSign **getBCDSignConstraints(TR::DataType dt);
  #endif
 #else
   TR::VP_BCDSign **getBCDSignConstraints(TR::DataType dt);
 #endif

   virtual void constrainRecognizedMethod(TR::Node *node);
   virtual bool transformUnsafeCopyMemoryCall(TR::Node *arrayCopyNode);
   virtual bool transformDirectLoad(TR::Node *node);
   virtual void doDelayedTransformations();
   void transformCallToNodeWithHCRGuard(TR::TreeTop *callTree, TR::Node *result);
   void transformCallToIconstInPlaceOrInDelayedTransformations(TR::TreeTop *callTree, int32_t result, bool isGlobal, bool inPlace = true, bool requiresGuard = false);
   void transformCallToNodeDelayedTransformations(TR::TreeTop *callTree, TR::Node *result, bool requiresGuard = false);
   uintptr_t* getObjectLocationFromConstraint(TR::VPConstraint *constraint);
   bool isKnownStringObject(TR::VPConstraint *constraint);
   TR_YesNoMaybe isStringObject(TR::VPConstraint *constraint);

   virtual void getParmValues();

   /**
    * @brief Supplemental functionality for constraining an acall node.  Projects
    *        consuming OMR can implement this function to provide project-specific
    *        functionality.
    *
    * @param[in] node : TR::Node of the call to constrain
    *
    * @return Resulting node with constraints applied.
    */
   virtual TR::Node *innerConstrainAcall(TR::Node *node);

   private:

   /**
    * \brief
    *    Transforms a call to a String indexOf method when the source string is a
    *    KnownObject or ConstString.
    *
    * \parm indexOfNode
    *    Node corresponding to a call to an indexOf method.
    *
    * \parm sourceStringNode
    *    Node corresponding to the source string (the string to search through).
    *
    * \parm targetCharNode
    *    Node corresponding to the target character (the character to search for).
    *
    * \parm startNode
    *    The index in the source string at which to start searching from.
    *    If NULL, search will start at index zero.
    *
    * \parm lengthNode
    *    The length in characters of the source string.
    *    If lengthNode is NULL, will attempt to determine length from constraint
    *    on sourceStringNode.
    *
    * \parm is16Bit
    *    True if each character in the source string is 16 bits. If false, assumed
    *    to be 8 bits.
    *
    * \return
    *    Return true if a transformation was performed, false otherwise.
    */
   bool transformIndexOfKnownString(
      TR::Node *indexOfNode,
      TR::Node *sourceStringNode,
      TR::Node *targetCharNode,
      TR::Node *startNode,
      TR::Node *lengthNode,
      bool is16Bit = true);

   struct TreeNodeResultPair {
      TR_ALLOC(TR_Memory::ValuePropagation)
      TR::TreeTop *_tree;
      TR::Node *_result;
      bool _requiresHCRGuard;
      TreeNodeResultPair(TR::TreeTop *tree, TR::Node *result, bool requiresHCRGuard) 
         : _tree(tree), _result(result), _requiresHCRGuard(requiresHCRGuard) {}
   };

   TR::VP_BCDSign **_bcdSignConstraints;
   List<TreeNodeResultPair> _callsToBeFoldedToNode;
   };


}

#endif
