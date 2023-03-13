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

#ifndef IA32LINKAGE_INCL
#define IA32LINKAGE_INCL

#include "codegen/X86PrivateLinkage.hpp"
#include "env/jittypes.h"

namespace TR { class UnresolvedDataSnippet; }

namespace J9
{

namespace X86
{

namespace I386
{

class PrivateLinkage : public J9::X86::PrivateLinkage
   {
   public:

   PrivateLinkage(TR::CodeGenerator *cg);

   TR::Register *pushThis(TR::Node *child);
   TR::Register *pushIntegerWordArg(TR::Node *child);

   TR::UnresolvedDataSnippet *generateX86UnresolvedDataSnippetWithCPIndex(TR::Node *child, TR::SymbolReference *symRef, int32_t cpIndex);

   /**
    * @brief J9 private linkage override of OMR function
    */
   virtual intptr_t entryPointFromCompiledMethod();

   protected:

   virtual TR::Instruction *savePreservedRegisters(TR::Instruction *cursor);
   virtual TR::Instruction *restorePreservedRegisters(TR::Instruction *cursor);
   virtual int32_t buildCallArguments(TR::Node *callNode, TR::RegisterDependencyConditions *dependencies){ return buildArgs(callNode, dependencies); }

   virtual int32_t buildArgs(TR::Node *callNode, TR::RegisterDependencyConditions *dependencies);

   virtual void buildVirtualOrComputedCall(TR::X86CallSite &site, TR::LabelSymbol *entryLabel, TR::LabelSymbol *doneLabel, uint8_t *thunk);
   virtual TR::Instruction *buildPICSlot(TR::X86PICSlot picSlot, TR::LabelSymbol *mismatchLabel, TR::LabelSymbol *doneLabel, TR::X86CallSite &site);
   virtual void buildIPIC(TR::X86CallSite &site, TR::LabelSymbol *entryLabel, TR::LabelSymbol *doneLabel, uint8_t *thunk);
   };

}

}

}


/**
 * The following is only required to assist with refactoring because they are
 * referenced in J9_PROJECT_SPECIFIC parts of OMR.  Once the IA32PrivateLinkage
 * class moves into the J9 namespace they can be easily removed in OMR and will
 * be subsequently deleted here.
 */
namespace J9
{

typedef J9::X86::I386::PrivateLinkage IA32PrivateLinkage;

}

#endif
