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

#ifndef UNCOMMON_BCDCHK_ADDRESS_NODE
#define UNCOMMON_BCDCHK_ADDRESS_NODE

#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/Node.hpp"
#include "infra/List.hpp"

/**
 * \brief The Uncommon BCDCHK Address Node codegen phase is designed to cope with incorrect OOL PD copy problem
 * that arises due to canonicalization/restructuring of BCDCHK tree.
 *
 * This codegen phase happens right before instruction selection phase to uncommon certain address nodes,
 * done by optimizations such as LocalCSE, under BCDCHK node.
 *
 *
 * The new and canonical BCDCHK tree should be of the following format:
 * ------------------------------------------------------------------------------------------
 * BCDCHK
 *      pdOpNode            (e.g. pdshloverflow)
 *      addressNode         (address into a byte[] object)
 *          arrayNode       (byte[] itself)
 *          offset          (offset into the byte[]. usually the header size plus another offset)
 *      callParam-1
 *      ...
 *      callParam-n
 *
 *
 * pdstorei
 *      =>addressNode       (same addressNode as above)
 *      =>pdOpNode          (same pdOpNode as above)
 * ------------------------------------------------------------------------------------------
 *
 * For the BCDCHK tree, the only node being evaluated in the mainline code is the pdOpNode, which is the
 * only one that can cause exceptions. For example, pdshlOverflow can be evaluated to SRP which can cause HW
 * decimal overflow exception.
 *
 * We make a function call in the out-of-line section to the original DAA API function to handle exceptions in the mainline.
 * To achieve this, we make use of the addressNode and the callParam-x nodes attached to the BCDCHK node.
 * In OOL section evaluation, we make a callNode with callParam 1-n attached to it; after the call, the correct pdOpNode
 * results need to be copied from the addressNode location to the pdOpNode storage reference. This copying makes
 * sure its following pdstorei (usually right after the BCDCHK node) is copying the correct content; and is only done correctly
 * if the BCDCHK's addressNode and pdstorei's addressNode are uncommoned (decoupled).
 *
 *
*/
class UncommonBCDCHKAddressNode
   {
   public:
   TR_ALLOC(TR_Memory::CodeGenerator)
   UncommonBCDCHKAddressNode(TR::CodeGenerator* cg): cg(cg)
   {
   _comp = cg->comp();
   }

   void perform();

   private:

   TR::CodeGenerator * cg;
   TR::Compilation *_comp;
   TR::Compilation *comp() { return _comp; }
   };

#endif
