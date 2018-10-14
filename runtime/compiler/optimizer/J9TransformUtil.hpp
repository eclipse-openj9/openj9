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

   static TR::Node *transformStringIndexOfCall( TR::Compilation *, TR::Node *callNode);
   
   static TR::Node *transformIndirectLoad(TR::Compilation *, TR::Node *node);
   static bool transformDirectLoad(TR::Compilation *, TR::Node *node);

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

   static TR::Node* calculateUnsafeAddress(TR::TreeTop* treetop, TR::Node* objectNode, TR::Node* offsetNode, TR::Compilation* comp, bool needsNullCheck, bool isNotStaticField);
   };

}

#endif
