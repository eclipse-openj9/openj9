/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef J9_TRANSFORMUTIL_INCL
#define J9_TRANSFORMUTIL_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_TRANSFORMUTIL_CONNECTOR
#define J9_TRANSFORMUTIL_CONNECTOR
namespace J9 { class TransformUtil; }
namespace J9 { typedef J9::TransformUtil TransformUtilConnector; }
#endif

#include "optimizer/OMRTransformUtil.hpp"
#include "optimizer/Optimization.hpp"
#include "control/RecompilationInfo.hpp"

namespace TR { class Compilation; }
namespace TR { class Node; }
namespace TR { class Block; }

namespace J9
{

class OMR_EXTENSIBLE TransformUtil : public OMR::TransformUtilConnector
   {
public:
   static TR::TreeTop *generateRetranslateCallerWithPrepTrees(TR::Node *node, TR_PersistentMethodInfo::InfoBits reason, TR::Compilation *comp);
   static int32_t getLoopNestingDepth(TR::Compilation *comp, TR::Block *block);
   static bool foldFinalFieldsIn(TR_OpaqueClassBlock *clazz, char *className, int32_t classNameLength, bool isStatic, TR::Compilation *comp);

   static TR::Node *generateArrayElementShiftAmountTrees(
         TR::Compilation *comp,
         TR::Node *object);
   
   static TR::Node *transformIndirectLoad(TR::Compilation *, TR::Node *node);
   static bool transformDirectLoad(TR::Compilation *, TR::Node *node);
   /**
    * \brief
    *    Fold direct load of a reliable static final field. A reliable static final field
    *    is a field on which modification after initialization is not expected because it's
    *    initial value critical to VM functionality and performance.
    *
    *    See J9::TransformUtil::canFoldStaticFinalField for the list of reliable static final
    *    field.
    *
    * \parm comp
    *    The compilation object
    *
    * \parm node
    *    The node which is a direct load
    *
    * \return
    *    True if the field has been folded
    */
   static bool foldReliableStaticFinalField(TR::Compilation *, TR::Node *node);
   /**
    * \brief
    *    Fold direct load of a static final field assuming there is protection for the folding.
    *    This will fold both the reliable ones as well as those we have no prior knowledge about.
    *    Folding of the latter requires protection in order to get out of the jitted body when
    *    the field gets modified. The onus is on the caller to ensure such protection is added.
    *
    * \parm comp
    *    The compilation object
    *
    * \parm node
    *    The node which is a direct load
    *
    * \return
    *    True if the field has been folded
    */
   static bool foldStaticFinalFieldAssumingProtection(TR::Compilation *, TR::Node *node);

   /**
    * \brief
    *    Answers if a static final field can be folded
    *
    * \parm comp
    *    The compilation object
    *
    * \parm node
    *    The node which is a load direct of static final field
    *
    * \return
    *    TR_yes    If the final field is reliable, then we can trust it. Modifying such field
    *              after initialization is not expected and is very likely to cause VM
    *              functional or performance issue.
    *    TR_no     If the final field is known to change, e.g. fields in java/lang/System
    *    TR_maybe  If we don't have any prior knowledge about this field, it can be folded
    *              with guard
    */
   static TR_YesNoMaybe canFoldStaticFinalField(TR::Compilation *comp, TR::Node *node);

   static bool transformIndirectLoadChain(TR::Compilation *, TR::Node *node, TR::Node *baseExpression, TR::KnownObjectTable::Index baseKnownObject, TR::Node **removedNode);
   static bool transformIndirectLoadChainAt(TR::Compilation *, TR::Node *node, TR::Node *baseExpression, uintptrj_t *baseReferenceLocation, TR::Node **removedNode);
   static bool transformIndirectLoadChainImpl( TR::Compilation *, TR::Node *node, TR::Node *baseExpression, void *baseAddress, TR::Node **removedNode);

   static bool fieldShouldBeCompressed(TR::Node *node, TR::Compilation *comp);

   static TR::Block *insertNewFirstBlockForCompilation(TR::Compilation *comp);
   static TR::Node *calculateOffsetFromIndexInContiguousArray(TR::Compilation *, TR::Node * index, TR::DataType type);
   static TR::Node *calculateElementAddress(TR::Compilation *, TR::Node *array, TR::Node * index, TR::DataType type);
   static TR::Node *calculateIndexFromOffsetInContiguousArray(TR::Compilation *, TR::Node * offset, TR::DataType type);

   static TR::Node* saveNodeToTempSlot(TR::Compilation* comp, TR::Node* node, TR::TreeTop* insertTreeTop);
   static void createTempsForCall(TR::Optimization* opt, TR::TreeTop *callTree);
   static void createDiamondForCall(TR::Optimization* opt, TR::TreeTop *callTree, TR::TreeTop *compareTree, TR::TreeTop *ifTree, TR::TreeTop *elseTree, bool changeBlockExtensions = false, bool markCold = false);

   static void prohibitOSROverRange(TR::Compilation* comp, TR::TreeTop* start, TR::TreeTop* end);
   static void removePotentialOSRPointHelperCalls(TR::Compilation* comp, TR::TreeTop* start, TR::TreeTop* end);
   /**
    *   \brief
    *       Move NULLCHK to a separate tree
    *
    *   \param comp  The compilation Object
    *   \param tree  The tree containing the null check
    *   \param trace Bool to enable tracing
    */
   static void separateNullCheck(TR::Compilation* comp, TR::TreeTop* tree, bool trace = false);
   /*
    * \brief
    *    Generate a tree to report modification to a static final field after initialization.
    *
    * \param node The java/lang/Class object
    *
    * \return Tree with the call to `jitReportFinalFieldModified`
    */
   static TR::TreeTop* generateReportFinalFieldModificationCallTree(TR::Compilation *comp, TR::Node *node);

protected:
   /**
    * \brief
    *    Fold a load of static final field to a constant or improve its symbol reference
    *
    * \parm comp
    *    The compilation object
    *
    * \parm node
    *    The node which is a load direct of static final field
    *
    * \return
    *    True if the field is folded
    */
   static bool foldStaticFinalFieldImpl(TR::Compilation *, TR::Node *node);

   };

}

#endif
