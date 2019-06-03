/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/ARM64JNILinkage.hpp"
#include "codegen/ARM64PrivateLinkage.hpp"
#include "codegen/ARM64Recompilation.hpp"
#include "codegen/ARM64SystemLinkage.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "runtime/CodeCacheManager.hpp"

J9::ARM64::CodeGenerator::CodeGenerator() :
      J9::CodeGenerator()
   {
   TR::CodeGenerator *cg = self();

   cg->setAheadOfTimeCompile(new (cg->trHeapMemory()) TR::AheadOfTimeCompile(cg));
   }

TR::Linkage *
J9::ARM64::CodeGenerator::createLinkage(TR_LinkageConventions lc)
   {
   TR::Linkage *linkage;
   switch (lc)
      {
      case TR_Private:
         linkage = new (self()->trHeapMemory()) TR::ARM64PrivateLinkage(self());
         break;
      case TR_System:
         linkage = new (self()->trHeapMemory()) TR::ARM64SystemLinkage(self());
         break;
      case TR_CHelper:
      case TR_Helper:
         linkage = new (self()->trHeapMemory()) TR::ARM64HelperLinkage(self());
         break;
      case TR_J9JNILinkage:
         linkage = new (self()->trHeapMemory()) TR::ARM64JNILinkage(self());
         break;
      default :
         linkage = new (self()->trHeapMemory()) TR::ARM64SystemLinkage(self());
         TR_ASSERT_FATAL(false, "Unexpected linkage convention");
      }

   self()->setLinkage(lc, linkage);
   return linkage;
   }

TR::Recompilation *
J9::ARM64::CodeGenerator::allocateRecompilationInfo()
   {
   return TR_ARM64Recompilation::allocate(self()->comp());
   }

uint32_t
J9::ARM64::CodeGenerator::encodeHelperBranchAndLink(TR::SymbolReference *symRef, uint8_t *cursor, TR::Node *node)
   {
   TR::CodeGenerator *cg = self();
   uintptrj_t target = (uintptrj_t)symRef->getMethodAddress();

   if (cg->directCallRequiresTrampoline(target, (intptrj_t)cursor))
      {
      target = TR::CodeCacheManager::instance()->findHelperTrampoline(symRef->getReferenceNumber(), (void *)cursor);

      TR_ASSERT_FATAL(TR::Compiler->target.cpu.isTargetWithinUnconditionalBranchImmediateRange(target, (intptrj_t)cursor),
                      "Target address is out of range");
      }

   cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(
                             cursor,
                             (uint8_t *)symRef,
                             TR_HelperAddress, cg),
                             __FILE__, __LINE__, node);

   uintptr_t distance = target - (uintptr_t)cursor;
   return TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::bl) | ((distance >> 2) & 0x3ffffff); /* imm26 */
   }
