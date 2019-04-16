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

#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "compile/Compilation.hpp"
#include "x/codegen/X86PrivateLinkage.hpp"
#include "x/codegen/X86HelperLinkage.hpp"
#include "codegen/IA32PrivateLinkage.hpp"
#include "codegen/IA32J9SystemLinkage.hpp"
#include "codegen/IA32JNILinkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "il/Node_inlines.hpp"

TR::Linkage *
J9::X86::i386::CodeGenerator::createLinkage(TR_LinkageConventions lc)
   {
   TR::Linkage *linkage = NULL;

   switch (lc)
      {
      case TR_CHelper:
         linkage = new (self()->trHeapMemory()) TR::X86HelperLinkage(self());
         break;
      case TR_Helper:
      case TR_Private:
         {
         TR::X86PrivateLinkage *p = NULL;
         p = new (self()->trHeapMemory()) TR::IA32PrivateLinkage(self());
         p->IPicParameters.roundedSizeOfSlot = 6+2+5+2+1;
         p->IPicParameters.defaultNumberOfSlots = 2;
         p->IPicParameters.defaultSlotAddress = -1;
         p->VPicParameters.roundedSizeOfSlot = 6+2+5+2+1;
         p->VPicParameters.defaultNumberOfSlots = 1;
         p->VPicParameters.defaultSlotAddress = -1;
         linkage = p;
         }
         break;

      case TR_J9JNILinkage:
         if (TR::Compiler->target.isWindows() || TR::Compiler->target.isLinux())
            {
            linkage = new (self()->trHeapMemory()) TR::IA32JNILinkage(self());
            }
         else
            {
            TR_ASSERT(0, "linkage not supported: %d\n", lc);
            linkage = NULL;
            }
         break;

      case TR_System:
         if (TR::Compiler->target.isWindows() || TR::Compiler->target.isLinux())
            {
            linkage = new (self()->trHeapMemory()) TR::IA32J9SystemLinkage(self());
            }
         else
            {
            TR_ASSERT(0, "linkage not supported: %d\n", lc);
            linkage = NULL;
            }
         break;

      default :
         TR_ASSERT(0, "\nTestarossa error: Illegal linkage convention %d\n", lc);
      }

   self()->setLinkage(lc, linkage);
   return linkage;
   }


void
J9::X86::i386::CodeGenerator::lowerTreesPreTreeTopVisit(TR::TreeTop *tt, vcount_t visitCount)
   {
   J9::X86::CodeGenerator::lowerTreesPreTreeTopVisit(tt, visitCount);

   TR::Node *node = tt->getNode();

   // On IA32 there are a reduced number of registers available on system
   // linkage dispatch sequences, so some kinds of expressions can't be
   // evaluated at that point.  We must extract them into their own treetops
   // to satisfy the required register dependencies.
   //
   if (node->getOpCodeValue() == TR::treetop)
      {
      TR::Node *child = node->getFirstChild();

      if ((child->getOpCode().isCall() && child->getSymbol()->getMethodSymbol() &&
          (child->isPreparedForDirectJNI() ||
           child->getSymbol()->getMethodSymbol()->isSystemLinkageDispatch())))
         {
         self()->setRemoveRegisterHogsInLowerTreesWalk();
         }
      }

   }


void
J9::X86::i386::CodeGenerator::lowerTreesPostTreeTopVisit(TR::TreeTop *tt, vcount_t visitCount)
   {
   J9::X86::CodeGenerator::lowerTreesPostTreeTopVisit(tt, visitCount);

   TR::Node *node = tt->getNode();

   // On IA32 there are a reduced number of registers available on system
   // linkage dispatch sequences, so some kinds of expressions can't be
   // evaluated at that point.  We must extract them into their own treetops
   // to satisfy the required register dependencies.
   //
   if (node->getOpCodeValue() == TR::treetop)
      {
      TR::Node *child = node->getFirstChild();

      if ((child->getOpCode().isCall() && child->getSymbol()->getMethodSymbol() &&
          (child->isPreparedForDirectJNI() ||
           child->getSymbol()->getMethodSymbol()->isSystemLinkageDispatch())))
         {
         self()->resetRemoveRegisterHogsInLowerTreesWalk();
         }
      }

   }


void
J9::X86::i386::CodeGenerator::lowerTreesPreChildrenVisit(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount)
   {
   J9::X86::CodeGenerator::lowerTreesPreChildrenVisit(parent, treeTop, visitCount);

   }


void
J9::X86::i386::CodeGenerator::lowerTreesPostChildrenVisit(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount)
   {
   J9::X86::CodeGenerator::lowerTreesPostChildrenVisit(parent, treeTop, visitCount);

   }
