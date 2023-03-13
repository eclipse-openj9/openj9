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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/X86PrivateLinkage.hpp"
#include "compile/Compilation.hpp"
#include "x/codegen/X86HelperLinkage.hpp"
#include "codegen/AMD64PrivateLinkage.hpp"
#include "codegen/AMD64JNILinkage.hpp"
#include "codegen/AMD64J9SystemLinkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "env/CompilerEnv.hpp"

void
J9::X86::AMD64::CodeGenerator::initialize()
   {
   self()->J9::X86::CodeGenerator::initialize();
   }


TR::Linkage *
J9::X86::AMD64::CodeGenerator::createLinkage(TR_LinkageConventions lc)
   {
   TR::Compilation *comp = self()->comp();
   TR::Linkage *linkage = NULL;

   switch (lc)
      {
      case TR_CHelper:
         linkage = new (self()->trHeapMemory()) J9::X86::HelperLinkage(self());
         break;
      case TR_Helper:
      case TR_Private:
         {
         J9::X86::PrivateLinkage *p = NULL;
         p = new (self()->trHeapMemory()) J9::X86::AMD64::PrivateLinkage(self());
         p->IPicParameters.roundedSizeOfSlot = 10+3+2+5+2+2;
         p->IPicParameters.defaultNumberOfSlots = 2;
         p->IPicParameters.defaultSlotAddress = 0;
         p->VPicParameters.roundedSizeOfSlot = 10+3+2+5+2+2;
         p->VPicParameters.defaultNumberOfSlots = 1;
         p->VPicParameters.defaultSlotAddress = 0;
         linkage = p;
         }
         break;

      case TR_J9JNILinkage:
         {
         TR::AMD64SystemLinkage *systemLinkage;

         if (comp->target().isWindows())
            {
            systemLinkage = new (self()->trHeapMemory()) TR::AMD64J9Win64FastCallLinkage(self());
            linkage = new (self()->trHeapMemory()) J9::X86::AMD64::JNILinkage(systemLinkage, self());
            }
         else if (comp->target().isLinux() || comp->target().isOSX())
            {
            systemLinkage = new (self()->trHeapMemory()) TR::AMD64J9ABILinkage(self());
            linkage = new (self()->trHeapMemory()) J9::X86::AMD64::JNILinkage(systemLinkage, self());
            }
         else
            {
            TR_ASSERT(0, "linkage not supported: %d\n", lc);
            linkage = NULL;
            }
         }
         break;

      case TR_System:
         if (comp->target().isWindows())
            {
            linkage = new (self()->trHeapMemory()) TR::AMD64J9Win64FastCallLinkage(self());
            }
         else if (comp->target().isLinux() || comp->target().isOSX())
            {
            linkage = new (self()->trHeapMemory()) TR::AMD64J9ABILinkage(self());
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
