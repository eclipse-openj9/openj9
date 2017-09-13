/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef IA32LINKAGE_INCL
#define IA32LINKAGE_INCL

#include "x/codegen/X86PrivateLinkage.hpp"

namespace TR { class UnresolvedDataSnippet; }

namespace TR {

class IA32PrivateLinkage : public TR::X86PrivateLinkage
   {
   public:

   IA32PrivateLinkage(TR::CodeGenerator *cg);

   virtual TR::Register *buildJNIDispatch(TR::Node *callNode);

   TR::Register *pushThis(TR::Node *child);
   TR::Register *pushIntegerWordArg(TR::Node *child);
   TR::Register *pushJNIReferenceArg (TR::Node *child);

   TR::UnresolvedDataSnippet *generateX86UnresolvedDataSnippetWithCPIndex(TR::Node *child, TR::SymbolReference *symRef, int32_t cpIndex);

   protected:

   virtual TR::Instruction *savePreservedRegisters(TR::Instruction *cursor);
   virtual TR::Instruction *restorePreservedRegisters(TR::Instruction *cursor);
   virtual TR::Register *buildDirectJNIDispatch(TR::Node *callNode){ return buildJNIDispatch(callNode); }
   virtual int32_t buildCallArguments(TR::Node *callNode, TR::RegisterDependencyConditions *dependencies){ return buildArgs(callNode, dependencies); }

   virtual TR::Instruction *savePreservedRegister(TR::Instruction *cursor, int32_t regIndex, int32_t offset);
   virtual TR::Instruction *restorePreservedRegister(TR::Instruction *cursor, int32_t regIndex, int32_t offset);
   virtual int32_t buildArgs(TR::Node *callNode, TR::RegisterDependencyConditions *dependencies);

   virtual void buildVirtualOrComputedCall(TR::X86CallSite &site, TR::LabelSymbol *entryLabel, TR::LabelSymbol *doneLabel, uint8_t *thunk);
   virtual TR::Instruction *buildPICSlot(TR::X86PICSlot picSlot, TR::LabelSymbol *mismatchLabel, TR::LabelSymbol *doneLabel, TR::X86CallSite &site);
   virtual void buildIPIC(TR::X86CallSite &site, TR::LabelSymbol *entryLabel, TR::LabelSymbol *doneLabel, uint8_t *thunk);
   };

inline TR::IA32PrivateLinkage *toIA32PrivateLinkage(TR::Linkage *linkage)
   {
   return (TR::IA32PrivateLinkage*)linkage;
   }

}

#endif

