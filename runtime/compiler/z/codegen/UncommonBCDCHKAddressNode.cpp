/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#include "codegen/UncommonBCDCHKAddressNode.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "compile/Compilation.hpp"
#include "env/VMJ9.h"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"


#define OPT_DETAILS "O^O UNCOMMON BCDCHK ADDRESS NODE: "

void
UncommonBCDCHKAddressNode::perform()
   {
   traceMsg(comp(), "Performing UncommonBCDCHKAddressNode\n");

   for(TR::TreeTop* tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      TR::Node* node = tt->getNode();

      if(node && node->getOpCodeValue() == TR::BCDCHK &&
         node->getSymbol() &&
         node->getSymbol()->getResolvedMethodSymbol() &&
         node->getNumChildren() >= 2 &&
         node->getFirstChild() &&
         node->getFirstChild()->getType().isBCD() &&
         node->getSecondChild() &&
         node->getSecondChild()->getDataType().isAddress() &&
         node->getSecondChild()->getReferenceCount() > 1)
         {
         TR::RecognizedMethod method = node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod();
         if(method == TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal_ ||
            method == TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal_ ||
            method == TR::com_ibm_dataaccess_PackedDecimal_addPackedDecimal_ ||
            method == TR::com_ibm_dataaccess_PackedDecimal_subtractPackedDecimal_ ||
            method == TR::com_ibm_dataaccess_PackedDecimal_multiplyPackedDecimal_ ||
            method == TR::com_ibm_dataaccess_PackedDecimal_dividePackedDecimal_ ||
            method == TR::com_ibm_dataaccess_PackedDecimal_remainderPackedDecimal_||
            method == TR::com_ibm_dataaccess_PackedDecimal_shiftLeftPackedDecimal_ ||
            method == TR::com_ibm_dataaccess_PackedDecimal_shiftRightPackedDecimal_)
            {
            TR::Node* oldAddressNode = node->getSecondChild();
            TR::ILOpCodes addressOp = oldAddressNode->getOpCodeValue();
            /* Expected tree structures for oldAddressNode
             * 1. non off-heap mode
             *     aladd/aiadd
             *         load array_object
             *         array_element_offset
             *
             * 2. off-heap enabled: loading first array element because offset is 0
             *     aloadi <contiguousArrayDataAddrFieldSymbol>
             *         load array_object
             *
             * 3. off-heap enabled
             *     aladd/aiadd
             *         aloadi <contiguousArrayDataAddrFieldSymbol>
             *             load array_object
             *         array_element_offset
             */
            TR_ASSERT_FATAL_WITH_NODE(node,
                                      (oldAddressNode->isDataAddrPointer() && oldAddressNode->getNumChildren() == 1)
                                          || ((addressOp == TR::aladd || addressOp == TR::aiadd) && oldAddressNode->getNumChildren() == 2),
                                      "Expecting either a dataAddrPointer node with 1 child or a aladd/aiadd with 2 children under address node of BCDCHK."
                                          "But the address node %s had %d children and dataAddrPointer flag set to %s\n",
                                          oldAddressNode->getOpCode().getName(), oldAddressNode->getNumChildren(), oldAddressNode->isDataAddrPointer() ? "true" : "false");
            TR::Node* newAddressNode = NULL;
            if (oldAddressNode->getOpCodeValue() == TR::aloadi)
               {
               // handles expected tree structure 2
               newAddressNode = TR::TransformUtil::generateArrayElementAddressTrees(comp(), oldAddressNode->getFirstChild());
               }
            else
               {
               // handles expected tree structure 1 and 3
               newAddressNode = TR::Node::create(node, addressOp, 2,
                                                 oldAddressNode->getFirstChild(),
                                                 oldAddressNode->getSecondChild());
               }

            node->setAndIncChild(1, newAddressNode);
            oldAddressNode->decReferenceCount();
            traceMsg(comp(), "%sReplacing node %s [%p] with [%p]\n",
                     OPT_DETAILS,
                     oldAddressNode->getOpCode().getName(),
                     oldAddressNode,
                     newAddressNode);
            }
         }
      }
   }

