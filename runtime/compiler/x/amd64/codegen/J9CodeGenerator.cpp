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
#include "codegen/AMD64PrivateLinkage.hpp"
#include "codegen/AMD64JNILinkage.hpp"
#include "codegen/AMD64J9SystemLinkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "env/CompilerEnv.hpp"

TR::Linkage *
J9::X86::AMD64::CodeGenerator::createLinkage(TR_LinkageConventions lc)
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
         p = new (self()->trHeapMemory()) TR::AMD64PrivateLinkage(self());
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

         if (TR::Compiler->target.isWindows())
            {
            systemLinkage = new (self()->trHeapMemory()) TR::AMD64J9Win64FastCallLinkage(self());
            linkage = new (self()->trHeapMemory()) TR::AMD64JNILinkage(systemLinkage, self());
            }
         else if (TR::Compiler->target.isLinux() || TR::Compiler->target.isOSX())
            {
            systemLinkage = new (self()->trHeapMemory()) TR::AMD64J9ABILinkage(self());
            linkage = new (self()->trHeapMemory()) TR::AMD64JNILinkage(systemLinkage, self());
            }
         else
            {
            TR_ASSERT(0, "linkage not supported: %d\n", lc);
            linkage = NULL;
            }
         }
         break;

      case TR_System:
         if (TR::Compiler->target.isWindows())
            {
            linkage = new (self()->trHeapMemory()) TR::AMD64J9Win64FastCallLinkage(self());
            }
         else if (TR::Compiler->target.isLinux() || TR::Compiler->target.isOSX())
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
