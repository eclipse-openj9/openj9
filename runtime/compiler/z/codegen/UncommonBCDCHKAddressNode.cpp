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

      if(node && node->getOpCodeValue() == TR::BCDCHK && node->getSymbol() && node->getSymbol()->getResolvedMethodSymbol())
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
            TR::Node* pdOpNode = node->getFirstChild();
            TR::Node* oldAddressNode = node->getSecondChild();
            TR_ASSERT(pdOpNode && oldAddressNode, "Unexpected null PD opNode or address node under BCDCHK");
            TR_ASSERT(oldAddressNode->getNumChildren() == 2, "Expecting 2 children under address node of BCDCHK.");

            TR::ILOpCodes addressOp = oldAddressNode->getOpCodeValue();
            TR_ASSERT((addressOp == TR::aladd || addressOp == TR::aiadd), "Unexpected addressNode opcode");

            if(oldAddressNode->getReferenceCount() > 1)
               {
               TR::Node* newAddressNode = TR::Node::create(node, addressOp, 2,
                                                           oldAddressNode->getFirstChild(),
                                                           oldAddressNode->getSecondChild());
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
   }

