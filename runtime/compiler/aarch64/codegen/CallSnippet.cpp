/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#include "codegen/CallSnippet.hpp"
#include "codegen/ARM64AOTRelocation.hpp"
#include "codegen/ARM64Instruction.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Register.hpp"
#include "env/CompilerEnv.hpp"
#include "env/J2IThunk.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

static uint8_t *storeArgumentItem(TR::InstOpCode::Mnemonic op, uint8_t *buffer, TR::RealRegister *reg, int32_t offset, TR::CodeGenerator *cg)
   {
   TR::RealRegister *stackPtr = cg->getStackPointerRegister();
   TR::InstOpCode opCode(op);
   uint32_t enc = (uint32_t)opCode.getOpCodeBinaryEncoding();
   TR_ASSERT((enc & 0x3b200000) == 0x39000000, "Instruction not supported in storeArgumentItem()");

   uint32_t size = (enc >> 30) & 3; /* b=0, h=1, w=2, x=3 */
   uint32_t shifted = offset >> size;
   uint32_t *wcursor = (uint32_t *)buffer;

   opCode.copyBinaryToBuffer(buffer);
   reg->setRegisterFieldRT(wcursor);
   stackPtr->setRegisterFieldRN(wcursor);
   *wcursor |= (shifted & 0xfff) << 10; /* imm12 */

   return buffer+4;
   }

static uint8_t *flushArgumentsToStack(uint8_t *buffer, TR::Node *callNode, int32_t argSize, TR::CodeGenerator *cg)
   {
   uint32_t intArgNum=0, floatArgNum=0, offset;
   TR::Machine *machine = cg->machine();
   TR::Linkage* linkage = cg->getLinkage(callNode->getSymbol()->castToMethodSymbol()->getLinkageConvention());
   const TR::ARM64LinkageProperties &linkageProperties = linkage->getProperties();
   int32_t argStart = callNode->getFirstArgumentIndex();

   if (linkageProperties.getRightToLeft())
      offset = linkage->getOffsetToFirstParm();
   else
      offset = argSize+linkage->getOffsetToFirstParm();

   for (int32_t i=argStart; i<callNode->getNumChildren();i++)
      {
      TR::Node *child = callNode->getChild(i);
      switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
            if (!linkageProperties.getRightToLeft())
               offset -= TR::Compiler->om.sizeofReferenceAddress();
            if (intArgNum < linkageProperties.getNumIntArgRegs())
               {
               buffer = storeArgumentItem(TR::InstOpCode::strimmw, buffer, machine->getRealRegister(linkageProperties.getIntegerArgumentRegister(intArgNum)), offset, cg);
               }
            intArgNum++;
            if (linkageProperties.getRightToLeft())
               offset += TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Address:
            if (!linkageProperties.getRightToLeft())
               offset -= TR::Compiler->om.sizeofReferenceAddress();
            if (intArgNum < linkageProperties.getNumIntArgRegs())
               {
               buffer = storeArgumentItem(TR::InstOpCode::strimmx, buffer, machine->getRealRegister(linkageProperties.getIntegerArgumentRegister(intArgNum)), offset, cg);
               }
            intArgNum++;
            if (linkageProperties.getRightToLeft())
               offset += TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Int64:
            if (!linkageProperties.getRightToLeft())
               offset -= 2*TR::Compiler->om.sizeofReferenceAddress();
            if (intArgNum < linkageProperties.getNumIntArgRegs())
               {
               buffer = storeArgumentItem(TR::InstOpCode::strimmx, buffer, machine->getRealRegister(linkageProperties.getIntegerArgumentRegister(intArgNum)), offset, cg);
               }
            intArgNum++;
            if (linkageProperties.getRightToLeft())
               offset += 2*TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Float:
            if (!linkageProperties.getRightToLeft())
               offset -= TR::Compiler->om.sizeofReferenceAddress();
            if (floatArgNum < linkageProperties.getNumFloatArgRegs())
               {
               buffer = storeArgumentItem(TR::InstOpCode::vstrimms, buffer, machine->getRealRegister(linkageProperties.getFloatArgumentRegister(floatArgNum)), offset, cg);
               }
            floatArgNum++;
            if (linkageProperties.getRightToLeft())
               offset += TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Double:
            if (!linkageProperties.getRightToLeft())
               offset -= 2*TR::Compiler->om.sizeofReferenceAddress();
            if (floatArgNum < linkageProperties.getNumFloatArgRegs())
               {
               buffer = storeArgumentItem(TR::InstOpCode::vstrimmd, buffer, machine->getRealRegister(linkageProperties.getFloatArgumentRegister(floatArgNum)), offset, cg);
               }
            floatArgNum++;
            if (linkageProperties.getRightToLeft())
               offset += 2*TR::Compiler->om.sizeofReferenceAddress();
            break;
         }
      }

   return buffer;
   }

static int32_t instructionCountForArguments(TR::Node *callNode, TR::CodeGenerator *cg)
   {
   uint32_t intArgNum=0, floatArgNum=0, count=0;
   const TR::ARM64LinkageProperties &linkage = cg->getLinkage(callNode->getSymbol()->castToMethodSymbol()->getLinkageConvention())->getProperties();
   int32_t argStart = callNode->getFirstArgumentIndex();

   for (int32_t i = argStart; i < callNode->getNumChildren(); i++)
      {
      TR::Node *child = callNode->getChild(i);
      switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Int64:
         case TR::Address:
            if (intArgNum < linkage.getNumIntArgRegs())
               {
               count++;
               }
            intArgNum++;
            break;
         case TR::Float:
         case TR::Double:
            if (floatArgNum < linkage.getNumFloatArgRegs())
               {
               count++;
               }
            floatArgNum++;
            break;
         }
      }

   return count;
   }

static int32_t
getBLDistance(uint8_t *cursor)
   {
   // branch distance of BL instruction
   int32_t distance;
   distance = *((int32_t *)cursor) & 0x03ffffff; // imm26
   distance = (distance << 6) >> 4; // sign extend and add two 0 bits
   return distance;
   }

TR_RuntimeHelper TR::ARM64CallSnippet::getHelper()
   {
   TR::Compilation * comp = cg()->comp();
   TR::Node *callNode = getNode();
   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   TR::SymbolReference *glueRef = NULL;
   bool isJitInduceOSRCall = false;
   if (methodSymbol->isHelper() &&
       methodSymRef->isOSRInductionHelper())
      {
      isJitInduceOSRCall = true;
      }

   if (methodSymRef->isUnresolved() || comp->compileRelocatableCode())
      {
      if (methodSymbol->isSpecial())
         return TR_ARM64interpreterUnresolvedSpecialGlue;
      if (methodSymbol->isStatic())
         return TR_ARM64interpreterUnresolvedStaticGlue;
      return TR_ARM64interpreterUnresolvedDirectVirtualGlue;
      }

   if (methodSymbol->isVMInternalNative() || methodSymbol->isJITInternalNative())
      return TR_ARM64nativeStaticHelper;

   if (isJitInduceOSRCall)
      return (TR_RuntimeHelper) methodSymRef->getReferenceNumber();

   bool synchronised = methodSymbol->isSynchronised();

   TR::DataType dataType = callNode->getDataType();
   switch (dataType)
      {
      case TR::NoType:
         return synchronised ? TR_ARM64interpreterSyncVoidStaticGlue : TR_ARM64interpreterVoidStaticGlue;
      case TR::Int32:
         return synchronised ? TR_ARM64interpreterSyncIntStaticGlue : TR_ARM64interpreterIntStaticGlue;
      case TR::Int64:
      case TR::Address:
         return synchronised ? TR_ARM64interpreterSyncLongStaticGlue : TR_ARM64interpreterLongStaticGlue;
      case TR::Float:
         return synchronised ? TR_ARM64interpreterSyncFloatStaticGlue : TR_ARM64interpreterFloatStaticGlue;
      case TR::Double:
         return synchronised ? TR_ARM64interpreterSyncDoubleStaticGlue : TR_ARM64interpreterDoubleStaticGlue;
      default:
         TR_ASSERT_FATAL(false, "Bad return data type '%s' for a call node.\n",
                         cg()->getDebug()->getName(dataType));
         return (TR_RuntimeHelper)0;
      }
   }

uint8_t *TR::ARM64CallSnippet::emitSnippetBody()
   {
    uint8_t            *cursor = cg()->getBinaryBufferCursor();
   TR::Node            *callNode = getNode();
   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   TR::SymbolReference *glueRef;
   TR::Compilation *comp = cg()->comp();
   void               *trmpln = NULL;

   getSnippetLabel()->setCodeLocation(cursor);

   // Flush in-register arguments back to the stack for interpreter
   cursor = flushArgumentsToStack(cursor, callNode, getSizeOfArguments(), cg());

   glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(getHelper(), false, false, false);

   // 'b glueRef' for jitInduceOSRAtCurrentPC, 'bl glueRef' otherwise
   // we use "b" for induceOSR because we want the helper to think that it's been called from the mainline code and not from the snippet.
   *(int32_t *)cursor = cg()->encodeHelperBranchAndLink(glueRef, cursor, callNode, glueRef->isOSRInductionHelper());
   cursor += 4;

   // Store the code cache RA
   *(intptr_t *)cursor = (intptr_t)getCallRA();
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
                               cursor,
                               NULL,
                               TR_AbsoluteMethodAddress, cg()),
                               __FILE__, __LINE__, getNode());
   cursor += 8;

   //induceOSRAtCurrentPC is implemented in the VM, and it knows, by looking at the current PC, what method it needs to
   //continue execution in interpreted mode. Therefore, it doesn't need the method pointer.
   if (!glueRef->isOSRInductionHelper())
      {
      // Store the method pointer: it is NULL for unresolved
      if (methodSymRef->isUnresolved() || comp->compileRelocatableCode())
         {
         *(intptr_t *)cursor = 0;
         if (comp->getOption(TR_EnableHCR))
            {
            cg()->jitAddPicToPatchOnClassRedefinition((void*)-1, (void *)cursor, true);
            cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation((uint8_t *)cursor, NULL,(uint8_t *)needsFullSizeRuntimeAssumption,
                                    TR_HCR, cg()),
                                       __FILE__, __LINE__, getNode());
            }
         }
      else
         {
         *(intptr_t *)cursor = (intptr_t)methodSymbol->getMethodAddress();
         if (comp->getOption(TR_EnableHCR))
            {
            cg()->jitAddPicToPatchOnClassRedefinition((void *)methodSymbol->getMethodAddress(), (void *)cursor);
            cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)methodSymRef,
                                                                                    getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                                    TR_MethodObject, cg()),
                                       __FILE__, __LINE__, callNode);
            }
         }
      }
   cursor += 8;

   // Lock word initialized to 0
   *(int32_t *)cursor = 0;

   return cursor+4;
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARM64CallSnippet * snippet)
   {
   TR::Node            *callNode     = snippet->getNode();
   TR::SymbolReference *glueRef      = _cg->getSymRef(snippet->getHelper());
   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet), getName(methodSymRef));

   //TODO print snippet body
   }

uint32_t TR::ARM64CallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return (instructionCountForArguments(getNode(), cg()) + 6) * 4;
   }

uint8_t *TR::ARM64VirtualSnippet::emitSnippetBody()
   {
   return NULL;
   }

uint32_t TR::ARM64VirtualSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return 0;
   }

uint8_t *TR::ARM64UnresolvedCallSnippet::emitSnippetBody()
   {
   uint8_t *cursor = TR::ARM64CallSnippet::emitSnippetBody();

   TR::SymbolReference *methodSymRef = getNode()->getSymbolReference();
   TR::MethodSymbol *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   intptr_t helperLookupOffset;

   TR::Compilation* comp = cg()->comp();

   // CP
   *(intptr_t *)cursor = (intptr_t)methodSymRef->getOwningMethod(comp)->constantPool();

   if (comp->compileRelocatableCode() && comp->getOption(TR_TraceRelocatableDataDetailsCG))
      {
      traceMsg(comp, "<relocatableDataTrampolinesCG>\n");
      traceMsg(comp, "%s\n", comp->signature());
      traceMsg(comp, "%-8s", "cpIndex");
      traceMsg(comp, "cp\n");
      traceMsg(comp, "%-8x", methodSymRef->getCPIndexForVM());
      traceMsg(comp, "%x\n", methodSymRef->getOwningMethod(comp)->constantPool());
      traceMsg(comp, "</relocatableDataTrampolinesCG>\n");
      }

   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
                               cursor,
                               *(uint8_t **)cursor,
                               getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                               TR_Trampolines, cg()),
                               __FILE__, __LINE__, getNode());
   cursor += 8;

   switch (getNode()->getDataType())
      {
      case TR::NoType:
         helperLookupOffset = 0;
         break;
      case TR::Int32:
         helperLookupOffset = 8;
         break;
      case TR::Int64:
      case TR::Address:
         helperLookupOffset = 16;
         break;
      case TR::Float:
         helperLookupOffset = 24;
         break;
      case TR::Double:
         helperLookupOffset = 32;
         break;
      }

   // CP index and helper offset
   *(intptr_t *)cursor = (helperLookupOffset<<56) | methodSymRef->getCPIndexForVM();

   return cursor + 8;
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARM64UnresolvedCallSnippet * snippet)
   {
   print(pOutFile, (TR::ARM64CallSnippet *) snippet);

   //TODO print snippet body
   }

uint32_t TR::ARM64UnresolvedCallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return TR::ARM64CallSnippet::getLength(estimatedSnippetStart) + 16;
   }

uint8_t *TR::ARM64VirtualUnresolvedSnippet::emitSnippetBody()
   {
   TR::Compilation* comp = cg()->comp();
   uint8_t *cursor = cg()->getBinaryBufferCursor();
   TR::Node *callNode = getNode();
   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::SymbolReference *glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_ARM64virtualUnresolvedHelper, false, false, false);
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   void *thunk = fej9->getJ2IThunk(callNode->getSymbolReference()->getSymbol()->castToMethodSymbol()->getMethod(), comp);

   // for alignment of intptr_t data
   if (((uint64_t)cursor % sizeof(intptr_t)) == 0)
      {
      *(int32_t *)cursor = 0xdeadc0de;
      cursor += sizeof(int32_t);
      }

   getSnippetLabel()->setCodeLocation(cursor);

   // bl glueRef
   *(int32_t *)cursor = cg()->encodeHelperBranchAndLink(glueRef, cursor, callNode);
   cursor += ARM64_INSTRUCTION_LENGTH;
   TR_ASSERT(((uint64_t)cursor % sizeof(intptr_t)) == 0, "Snippet data is not aligned");

   // Store the code cache RA
   *(intptr_t *)cursor = (intptr_t)getReturnLabel()->getCodeLocation();
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
                               cursor,
                               NULL,
                               TR_AbsoluteMethodAddress, cg()),
                               __FILE__, __LINE__, callNode);
   cursor += sizeof(intptr_t);

   // CP
   intptr_t cpAddr = (intptr_t)methodSymRef->getOwningMethod(comp)->constantPool();
   *(intptr_t *)cursor = cpAddr;
   uint8_t *j2iThunkRelocationPoint = cursor;
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
                               cursor,
                               *(uint8_t **)cursor,
                               getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                               TR_Thunks, cg()),
                               __FILE__, __LINE__, getNode());
   cursor += sizeof(intptr_t);

   // CP index
   *(intptr_t *)cursor = methodSymRef->getCPIndexForVM();
   cursor += sizeof(intptr_t);

   // Reserved spot to hold J9Method pointer of the callee
   // This is used for private nestmate calls
   // Initial value is 0
   *(intptr_t *)cursor = 0;
   cursor += sizeof(intptr_t);

   // J2I thunk address
   // This is used for private nestmate calls
   *(intptr_t*)cursor = (intptr_t)thunk;

   auto info =
      (TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(
         sizeof (TR_RelocationRecordInformation),
         heapAlloc);

   // data1 = constantPool
   info->data1 = cpAddr;

   // data2 = inlined site index
   info->data2 = callNode ? callNode->getInlinedSiteIndex() : (uintptr_t)-1;

   // data3 = distance in bytes from Constant Pool Pointer to J2I Thunk
   info->data3 = (intptr_t)cursor - (intptr_t)j2iThunkRelocationPoint;

   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
                               j2iThunkRelocationPoint,
                               (uint8_t *)info,
                               NULL,
                               TR_J2IVirtualThunkPointer, cg()),
                               __FILE__, __LINE__, callNode);

   cursor += sizeof(intptr_t);

   // Lock word
   *(int32_t *)cursor = 0;

   return cursor + sizeof(int32_t);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARM64VirtualUnresolvedSnippet * snippet)
   {
   TR::SymbolReference *callSymRef = snippet->getNode()->getSymbolReference();
   uint8_t *cursor = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), cursor, getName(snippet), getName(callSymRef));

   int32_t distance = getBLDistance(cursor);
   printPrefix(pOutFile, NULL, cursor, ARM64_INSTRUCTION_LENGTH);
   trfprintf(pOutFile, "bl \t" POINTER_PRINTF_FORMAT "\t\t; %s",
             (intptr_t)cursor + distance, getRuntimeHelperName(TR_ARM64virtualUnresolvedHelper));
   cursor += ARM64_INSTRUCTION_LENGTH;

   printPrefix(pOutFile, NULL, cursor, sizeof(intptr_t));
   trfprintf(pOutFile, ".dword \t" POINTER_PRINTF_FORMAT "\t\t; Code cache return address", *(intptr_t *)cursor);
   cursor += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptr_t));
   trfprintf(pOutFile, ".dword \t" POINTER_PRINTF_FORMAT "\t\t; Constant pool address", *(intptr_t *)cursor);
   cursor += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptr_t));
   trfprintf(pOutFile, ".dword \t0x%08x\t\t; cpIndex", *(intptr_t *)cursor);
   cursor += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptr_t));
   trfprintf(pOutFile, ".dword \t" POINTER_PRINTF_FORMAT "\t\t; Private J9Method pointer", *(intptr_t *)cursor);
   cursor += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptr_t));
   trfprintf(pOutFile, ".dword \t" POINTER_PRINTF_FORMAT "\t\t; J2I thunk address for private", *(intptr_t *)cursor);
   cursor += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(int32_t));
   trfprintf(pOutFile, ".word \t0x%08x\t\t; Lock Word For Resolution", *(int32_t *)cursor);
   }

uint32_t TR::ARM64VirtualUnresolvedSnippet::getLength(int32_t estimatedSnippetStart)
   {
   /*
    * (1 word for alignment)
    * 1 instruction
    * 5 address fields:
    *   - Code cache RA
    *   - CP pointer
    *   - CP index
    *   - Private J9Method pointer
    *   - J2I thunk address
    * 1 lock word
    */
   return ARM64_INSTRUCTION_LENGTH + sizeof(intptr_t)*5 + sizeof(int32_t)*2;
   }

uint8_t *TR::ARM64InterfaceCallSnippet::emitSnippetBody()
   {
   TR::Compilation *comp = cg()->comp();
   uint8_t *cursor = cg()->getBinaryBufferCursor();
   TR::Node *callNode = getNode();
   TR::SymbolReference *methodSymRef = getNode()->getSymbolReference();
   TR::SymbolReference *glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_ARM64interfaceCallHelper, false, false, false);
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   void* thunk = fej9->getJ2IThunk(callNode->getSymbolReference()->getSymbol()->castToMethodSymbol()->getMethod(), comp);

   // for alignment of intptr_t data
   if (((uint64_t)cursor % sizeof(intptr_t)) == 0)
      {
      *(int32_t *)cursor = 0xdeadc0de;
      cursor += sizeof(int32_t);
      }

   getSnippetLabel()->setCodeLocation(cursor);

   // bl glueRef
   *(int32_t *)cursor = cg()->encodeHelperBranchAndLink(glueRef, cursor, callNode);
   cursor += ARM64_INSTRUCTION_LENGTH;
   TR_ASSERT(((uint64_t)cursor % sizeof(intptr_t)) == 0, "Snippet data is not aligned");

   // Store the code cache RA
   *(intptr_t *)cursor = (intptr_t)getReturnLabel()->getCodeLocation();
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
                               cursor,
                               NULL,
                               TR_AbsoluteMethodAddress, cg()),
                               __FILE__, __LINE__, callNode);
   cursor += sizeof(intptr_t);

   // CP
   intptr_t cpAddr = (intptr_t)methodSymRef->getOwningMethod(comp)->constantPool();
   *(intptr_t *)cursor = cpAddr;
   uint8_t *j2iThunkRelocationPoint = cursor;
   cursor += sizeof(intptr_t);

   // CP index
   *(intptr_t *)cursor = methodSymRef->getCPIndexForVM();
   cursor += sizeof(intptr_t);

   // 2 slots for resolved values (interface class and iTable index)
   *(intptr_t *)cursor = 0;
   cursor += sizeof(intptr_t);
   *(intptr_t *)cursor = 0;
   cursor += sizeof(intptr_t);

   /*
    * J2I thunk address.
    * This is used for private nestmate calls.
    */
   *(intptr_t*)cursor = (intptr_t)thunk;
   if (comp->compileRelocatableCode())
      {
      auto info =
         (TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(
            sizeof(TR_RelocationRecordInformation),
            heapAlloc);

      // data1 = constantPool
      info->data1 = cpAddr;

      // data2 = inlined site index
      info->data2 = callNode ? callNode->getInlinedSiteIndex() : (uintptr_t)-1;

      // data3 = distance in bytes from Constant Pool Pointer to J2I Thunk
      info->data3 = (intptr_t)cursor - (intptr_t)j2iThunkRelocationPoint;

      cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
                                  j2iThunkRelocationPoint,
                                  (uint8_t *)info,
                                  NULL,
                                  TR_J2IVirtualThunkPointer, cg()),
                                  __FILE__, __LINE__, callNode);
      }
   cursor += sizeof(intptr_t);

   return cursor;
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::ARM64InterfaceCallSnippet * snippet)
   {
   TR::SymbolReference *callSymRef = snippet->getNode()->getSymbolReference();
   uint8_t *cursor = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), cursor, getName(snippet), getName(callSymRef));

   int32_t distance = getBLDistance(cursor);
   printPrefix(pOutFile, NULL, cursor, ARM64_INSTRUCTION_LENGTH);
   trfprintf(pOutFile, "bl \t" POINTER_PRINTF_FORMAT "\t\t; %s",
             (intptr_t)cursor + distance, getRuntimeHelperName(TR_ARM64interfaceCallHelper));
   cursor += ARM64_INSTRUCTION_LENGTH;

   printPrefix(pOutFile, NULL, cursor, sizeof(intptr_t));
   trfprintf(pOutFile, ".dword \t" POINTER_PRINTF_FORMAT "\t\t; Code cache return address", *(intptr_t *)cursor);
   cursor += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptr_t));
   trfprintf(pOutFile, ".dword \t" POINTER_PRINTF_FORMAT "\t\t; Constant pool address", *(intptr_t *)cursor);
   cursor += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptr_t));
   trfprintf(pOutFile, ".dword \t0x%08x\t\t; cpIndex", *(intptr_t *)cursor);
   cursor += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptr_t));
   trfprintf(pOutFile, ".dword \t" POINTER_PRINTF_FORMAT "\t\t; Interface class", *(intptr_t *)cursor);
   cursor += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptr_t));
   trfprintf(pOutFile, ".dword \t0x%08x\t\t; itable index", *(intptr_t *)cursor);
   cursor += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptr_t));
   trfprintf(pOutFile, ".dword \t" POINTER_PRINTF_FORMAT "\t\t; J2I thunk address for private", *(intptr_t *)cursor);
   }

uint32_t TR::ARM64InterfaceCallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   /*
    * (1 word for alignment)
    * 1 instruction
    * 6 address fields:
    *   - Code cache RA
    *   - CP Pointer
    *   - CP Index
    *   - Interface Class Pointer
    *   - ITable Index (may also contain a tagged J9Method* when handling nestmates)
    *   - J2I thunk address
    */
   return ARM64_INSTRUCTION_LENGTH + sizeof(intptr_t)*6 + sizeof(int32_t);
   }

uint8_t *TR::ARM64CallSnippet::generateVIThunk(TR::Node *callNode, int32_t argSize, TR::CodeGenerator *cg)
   {
   int32_t codeSize = 4 * (instructionCountForArguments(callNode, cg) + 5) + 8; // 5 instructions for branch, Additional 8 bytes to hold size of thunk
   uint8_t *thunk, *buffer, *returnValue;
   uintptr_t dispatcher;

   if (cg->comp()->compileRelocatableCode())
      thunk = (uint8_t *)cg->comp()->trMemory()->allocateMemory(codeSize, heapAlloc);
   else
      thunk = (uint8_t *)cg->allocateCodeMemory(codeSize, true, false);
   buffer = returnValue = thunk + 8;
   TR_RuntimeHelper helper;
   TR::DataType dataType = callNode->getDataType();

   switch (dataType)
      {
      case TR::NoType:
         helper = TR_ARM64icallVMprJavaSendVirtual0;
         break;
      case TR::Int32:
         helper = TR_ARM64icallVMprJavaSendVirtual1;
         break;
      case TR::Int64:
      case TR::Address:
         helper = TR_ARM64icallVMprJavaSendVirtualJ;
         break;
      case TR::Float:
         helper = TR_ARM64icallVMprJavaSendVirtualF;
         break;
      case TR::Double:
         helper = TR_ARM64icallVMprJavaSendVirtualD;
         break;
      default:
         TR_ASSERT_FATAL(false, "Bad return data type '%s' for a call node.\n",
                         cg->getDebug()->getName(dataType));
      }

   dispatcher = (uintptr_t)cg->symRefTab()->findOrCreateRuntimeHelper(helper, false, false, false)->getMethodAddress();

   buffer = flushArgumentsToStack(buffer, callNode, argSize, cg);

   TR::RealRegister *x15reg = cg->machine()->getRealRegister(TR::RealRegister::x15);

   *((int32_t *)thunk + 1) = buffer - returnValue;  // patch offset for AOT relocation
   // movz x15, low 16 bits
   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::movzx) | ((dispatcher & 0xFFFF) << 5);
   x15reg->setRegisterFieldRD((uint32_t *)buffer);
   buffer += ARM64_INSTRUCTION_LENGTH;
   // movk x15, next 16 bits, lsl #16
   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::movkx) | (TR::MOV_LSL16 << 5) | (((dispatcher >> 16) & 0xFFFF) << 5);
   x15reg->setRegisterFieldRD((uint32_t *)buffer);
   buffer += ARM64_INSTRUCTION_LENGTH;
   // movk x15, next 16 bits, lsl #32
   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::movkx) | (TR::MOV_LSL32 << 5) | (((dispatcher >> 32) & 0xFFFF) << 5);
   x15reg->setRegisterFieldRD((uint32_t *)buffer);
   buffer += ARM64_INSTRUCTION_LENGTH;
   // movk x15, next 16 bits, lsl #48
   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::movkx) | (TR::MOV_LSL48 << 5) | (((dispatcher >> 48) & 0xFFFF) << 5);
   x15reg->setRegisterFieldRD((uint32_t *)buffer);
   buffer += ARM64_INSTRUCTION_LENGTH;
   // br x15
   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::br);
   x15reg->setRegisterFieldRN((uint32_t *)buffer);
   buffer += ARM64_INSTRUCTION_LENGTH;

   *(int32_t *)thunk = buffer - returnValue;        // patch size of thunk

#ifdef TR_HOST_ARM64
   arm64CodeSync(thunk, codeSize);
#endif

   return returnValue;
   }

TR_J2IThunk *TR::ARM64CallSnippet::generateInvokeExactJ2IThunk(TR::Node *callNode, int32_t argSize, TR::CodeGenerator *cg, char *signature)
   {
   int32_t codeSize = 4 * (instructionCountForArguments(callNode, cg) + 5); // 5 instructions for branch
   uintptr_t dispatcher;
   TR::Compilation *comp = cg->comp();
   TR_J2IThunkTable *thunkTable = comp->getPersistentInfo()->getInvokeExactJ2IThunkTable();
   TR_J2IThunk *thunk = TR_J2IThunk::allocate(codeSize, signature, cg, thunkTable);
   uint8_t *buffer = thunk->entryPoint();

   TR_RuntimeHelper helper;
   TR::DataType dataType = callNode->getDataType();

   switch (dataType)
      {
      case TR::NoType:
         helper = TR_icallVMprJavaSendInvokeExact0;
         break;
      case TR::Int32:
         helper = TR_icallVMprJavaSendInvokeExact1;
         break;
      case TR::Int64:
      case TR::Address:
         helper = TR_icallVMprJavaSendInvokeExactJ;
         break;
      case TR::Float:
         helper = TR_icallVMprJavaSendInvokeExactF;
         break;
      case TR::Double:
         helper = TR_icallVMprJavaSendInvokeExactD;
         break;
      default:
         TR_ASSERT(false, "Bad return data type '%s' for a call node.\n",
                  cg->getDebug()->getName(dataType));
      }

   dispatcher = (intptr_t)cg->symRefTab()->findOrCreateRuntimeHelper(helper, false, false, false)->getMethodAddress();

   buffer = flushArgumentsToStack(buffer, callNode, argSize, cg);

   TR::RealRegister *x15reg = cg->machine()->getRealRegister(TR::RealRegister::x15);

   // movz x15, low 16 bits
   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::movzx) | ((dispatcher & 0xFFFF) << 5);
   x15reg->setRegisterFieldRD((uint32_t *)buffer);
   buffer += ARM64_INSTRUCTION_LENGTH;
   // movk x15, next 16 bits, lsl #16
   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::movkx) | (TR::MOV_LSL16 << 5) | (((dispatcher >> 16) & 0xFFFF) << 5);
   x15reg->setRegisterFieldRD((uint32_t *)buffer);
   buffer += ARM64_INSTRUCTION_LENGTH;
   // movk x15, next 16 bits, lsl #32
   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::movkx) | (TR::MOV_LSL32 << 5) | (((dispatcher >> 32) & 0xFFFF) << 5);
   x15reg->setRegisterFieldRD((uint32_t *)buffer);
   buffer += ARM64_INSTRUCTION_LENGTH;
   // movk x15, next 16 bits, lsl #48
   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::movkx) | (TR::MOV_LSL48 << 5) | (((dispatcher >> 48) & 0xFFFF) << 5);
   x15reg->setRegisterFieldRD((uint32_t *)buffer);
   buffer += ARM64_INSTRUCTION_LENGTH;
   // br x15
   *(int32_t *)buffer = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::br);
   x15reg->setRegisterFieldRN((uint32_t *)buffer);
   buffer += ARM64_INSTRUCTION_LENGTH;

#ifdef TR_HOST_ARM64
   arm64CodeSync(thunk->entryPoint(), codeSize);
#endif

   return thunk;
   }

