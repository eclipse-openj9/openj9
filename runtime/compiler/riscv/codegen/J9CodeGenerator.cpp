/*******************************************************************************
 * Copyright (c) 2019, 2022 IBM Corp. and others
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
#include "codegen/RVSystemLinkage.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "runtime/CodeCacheManager.hpp"

extern void TEMPORARY_initJ9RVTreeEvaluatorTable(TR::CodeGenerator *cg);

J9::RV::CodeGenerator::CodeGenerator(TR::Compilation *comp) :
      J9::CodeGenerator(comp)
   {
   /**
     * Do not add CodeGenerator initialization logic here.
     * Use the \c initialize() method instead.
     */
   }

void
J9::RV::CodeGenerator::initialize()
   {
   self()->J9::CodeGenerator::initialize();

   TR::CodeGenerator *cg = self();

   cg->setAheadOfTimeCompile(new (cg->trHeapMemory()) TR::AheadOfTimeCompile(cg));

   /*
    * "Statically" initialize the FE-specific tree evaluator functions.
    * This code only needs to execute once per JIT lifetime.
    */
   static bool initTreeEvaluatorTable = false;
   if (!initTreeEvaluatorTable)
      {
      TEMPORARY_initJ9RVTreeEvaluatorTable(cg);
      initTreeEvaluatorTable = true;
      }

   cg->setSupportsInliningOfTypeCoersionMethods();
   cg->setSupportsDivCheck();
   if (!comp()->getOption(TR_FullSpeedDebug))
      cg->setSupportsDirectJNICalls();
   }

TR::Linkage *
J9::RV::CodeGenerator::createLinkage(TR_LinkageConventions lc)
   {
   TR::Linkage *linkage;
   switch (lc)
      {
      case TR_System:
         linkage = new (self()->trHeapMemory()) TR::RVSystemLinkage(self());
         break;
#if 0
      case TR_Private:
         linkage = new (self()->trHeapMemory()) J9::RV::PrivateLinkage(self());
         break;
      case TR_CHelper:
      case TR_Helper:
         linkage = new (self()->trHeapMemory()) J9::RV::HelperLinkage(self(), lc);
         break;
      case TR_J9JNILinkage:
         linkage = new (self()->trHeapMemory()) J9::RV::JNILinkage(self());
         break;
#endif
      default :
         linkage = new (self()->trHeapMemory()) TR::RVSystemLinkage(self());
         TR_ASSERT_FATAL(false, "Unexpected linkage convention");
      }

   self()->setLinkage(lc, linkage);
   return linkage;
   }

TR::Recompilation *
J9::RV::CodeGenerator::allocateRecompilationInfo()
   {
   TR_UNIMPLEMENTED();
   //return TR_RVRecompilation::allocate(self()->comp());
   return NULL; // to make compiler happy
   }

uint32_t
J9::RV::CodeGenerator::encodeHelperBranchAndLink(TR::SymbolReference *symRef, uint8_t *cursor, TR::Node *node, bool omitLink)
   {
   TR_UNIMPLEMENTED();
   return 0; // to make compiler happy
   }

void
J9::RV::CodeGenerator::generateBinaryEncodingPrePrologue(TR_RVBinaryEncodingData &data)
   {
   TR_UNIMPLEMENTED();
   }

TR::Instruction *
J9::RV::CodeGenerator::generateSwitchToInterpreterPrePrologue(TR::Instruction *cursor, TR::Node *node)
   {
   TR_UNIMPLEMENTED();
   return cursor; // to make compiler happy
   }
