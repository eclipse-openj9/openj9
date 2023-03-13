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

#ifndef AMD64LINKAGE_INCL
#define AMD64LINKAGE_INCL

#ifdef TR_TARGET_64BIT

#include "codegen/X86PrivateLinkage.hpp"

class J2IThunk;


namespace J9
{

namespace X86
{

namespace AMD64
{

class PrivateLinkage : public J9::X86::PrivateLinkage
   {
   public:

   PrivateLinkage(TR::CodeGenerator *cg);
   virtual TR::Register *buildJNIDispatch(TR::Node *callNode);

   protected:

   virtual TR::Instruction *savePreservedRegisters(TR::Instruction *cursor);
   virtual TR::Instruction *restorePreservedRegisters(TR::Instruction *cursor);

   virtual int32_t buildCallArguments(
         TR::Node *callNode,
         TR::RegisterDependencyConditions *dependencies)
      {
      return buildArgs(callNode, dependencies);
      }

   int32_t argAreaSize(TR::ResolvedMethodSymbol *methodSymbol);
   int32_t argAreaSize(TR::Node *callNode);

   virtual uint8_t *flushArguments(TR::Node *callNode, uint8_t *cursor, bool calculateSizeOnly, int32_t *sizeOfFlushArea, bool isReturnAddressOnStack, bool isLoad);
   virtual TR::Instruction *flushArguments(TR::Instruction *prev, TR::ResolvedMethodSymbol *methodSymbol, bool isReturnAddressOnStack, bool isLoad);

   // Handles the precondition side of the call, which arises from arguments,
   // and includes generating movs for arguments passed on the stack.
   // Returns the number of bytes of the outgoing argument area actually used
   // for this call.
   //
   virtual int32_t buildArgs(
         TR::Node *callNode,
         TR::RegisterDependencyConditions *dependencies);

   int32_t buildPrivateLinkageArgs(
         TR::Node *callNode,
         TR::RegisterDependencyConditions *dependencies,
         bool rightToLeftHelperLinkage,
         bool passArgsOnStack = false);

   virtual void buildVirtualOrComputedCall(TR::X86CallSite &site, TR::LabelSymbol *entryLabel, TR::LabelSymbol *doneLabel, uint8_t *thunk);
   virtual TR::Instruction *buildPICSlot(TR::X86PICSlot picSlot, TR::LabelSymbol *mismatchLabel, TR::LabelSymbol *doneLabel, TR::X86CallSite &site);
   virtual void buildIPIC(TR::X86CallSite &site, TR::LabelSymbol *entryLabel, TR::LabelSymbol *doneLabel, uint8_t *thunk);
   virtual uint8_t *generateVirtualIndirectThunk(TR::Node *callNode);
   virtual TR_MHJ2IThunk *generateInvokeExactJ2IThunk(TR::Node *callNode, char *signature);

   TR::Instruction *generateFlushInstruction(
         TR::Instruction *prev,
         TR_MovOperandTypes operandType,
         TR::DataType dataType,
         TR::RealRegister::RegNum regIndex,
         TR::Register *espReg,
         int32_t offset,
         TR::CodeGenerator *cg);
   };

}

}

}

#endif

#endif

