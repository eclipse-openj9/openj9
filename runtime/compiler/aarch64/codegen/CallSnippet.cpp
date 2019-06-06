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

#include "codegen/CallSnippet.hpp"
#include "codegen/ARM64AOTRelocation.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Register.hpp"
#include "env/CompilerEnv.hpp"
#include "env/J2IThunk.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"

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
   const TR::ARM64LinkageProperties &linkage = cg->getLinkage(callNode->getSymbol()->castToMethodSymbol()->getLinkageConvention())->getProperties();
   int32_t argStart = callNode->getFirstArgumentIndex();

   if (linkage.getRightToLeft())
      offset = linkage.getOffsetToFirstParm();
   else
      offset = argSize+linkage.getOffsetToFirstParm();

   for (int32_t i=argStart; i<callNode->getNumChildren();i++)
      {
      TR::Node *child = callNode->getChild(i);
      switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
            if (!linkage.getRightToLeft())
               offset -= TR::Compiler->om.sizeofReferenceAddress();
            if (intArgNum < linkage.getNumIntArgRegs())
               {
               buffer = storeArgumentItem(TR::InstOpCode::strimmw, buffer, machine->getRealRegister(linkage.getIntegerArgumentRegister(intArgNum)), offset, cg);
               }
            intArgNum++;
            if (linkage.getRightToLeft())
               offset += TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Address:
            if (!linkage.getRightToLeft())
               offset -= TR::Compiler->om.sizeofReferenceAddress();
            if (intArgNum < linkage.getNumIntArgRegs())
               {
               buffer = storeArgumentItem(TR::InstOpCode::strimmx, buffer, machine->getRealRegister(linkage.getIntegerArgumentRegister(intArgNum)), offset, cg);
               }
            intArgNum++;
            if (linkage.getRightToLeft())
               offset += TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Int64:
            if (!linkage.getRightToLeft())
               offset -= 2*TR::Compiler->om.sizeofReferenceAddress();
            if (intArgNum < linkage.getNumIntArgRegs())
               {
               buffer = storeArgumentItem(TR::InstOpCode::strimmx, buffer, machine->getRealRegister(linkage.getIntegerArgumentRegister(intArgNum)), offset, cg);
               }
            intArgNum++;
            if (linkage.getRightToLeft())
               offset += 2*TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Float:
            if (!linkage.getRightToLeft())
               offset -= TR::Compiler->om.sizeofReferenceAddress();
            if (floatArgNum < linkage.getNumFloatArgRegs())
               {
               buffer = storeArgumentItem(TR::InstOpCode::vstrimms, buffer, machine->getRealRegister(linkage.getFloatArgumentRegister(floatArgNum)), offset, cg);
               }
            floatArgNum++;
            if (linkage.getRightToLeft())
               offset += TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Double:
            if (!linkage.getRightToLeft())
               offset -= 2*TR::Compiler->om.sizeofReferenceAddress();
            if (floatArgNum < linkage.getNumFloatArgRegs())
               {
               buffer = storeArgumentItem(TR::InstOpCode::vstrimmd, buffer, machine->getRealRegister(linkage.getFloatArgumentRegister(floatArgNum)), offset, cg);
               }
            floatArgNum++;
            if (linkage.getRightToLeft())
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

TR_RuntimeHelper TR::ARM64CallSnippet::getHelper()
   {
   TR::Compilation * comp = cg()->comp();
   TR::Node *callNode = getNode();
   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   TR::SymbolReference *glueRef = NULL;

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

   // bl glueRef
   *(int32_t *)cursor = cg()->encodeHelperBranchAndLink(glueRef, cursor, callNode);
   cursor += 4;

   // Store the code cache RA
   *(intptrj_t *)cursor = (intptrj_t)getCallRA();
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
                               cursor,
                               NULL,
                               TR_AbsoluteMethodAddress, cg()),
                               __FILE__, __LINE__, getNode());
   cursor += 8;

   // Store the method pointer: it is NULL for unresolved
   if (methodSymRef->isUnresolved() || comp->compileRelocatableCode())
      {
      *(intptrj_t *)cursor = 0;
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
      *(intptrj_t *)cursor = (intptrj_t)methodSymbol->getMethodAddress();
      if (comp->getOption(TR_EnableHCR))
         {
         cg()->jitAddPicToPatchOnClassRedefinition((void *)methodSymbol->getMethodAddress(), (void *)cursor);
         cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)methodSymRef,
                                                                                 getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                                 TR_MethodObject, cg()),
                                     __FILE__, __LINE__, callNode);
         }
      }
   cursor += 8;

   // Lock word initialized to 0
   *(int32_t *)cursor = 0;

   return cursor+4;
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
   int32_t helperLookupOffset;

   TR::Compilation* comp = cg()->comp();

   // CP
   *(intptrj_t *)cursor = (intptrj_t)methodSymRef->getOwningMethod(comp)->constantPool();

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
   *(int32_t *)cursor = (helperLookupOffset<<24) | methodSymRef->getCPIndexForVM();

   return cursor + 4;
   }

uint32_t TR::ARM64UnresolvedCallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return TR::ARM64CallSnippet::getLength(estimatedSnippetStart) + 12;
   }

uint8_t *TR::ARM64VirtualUnresolvedSnippet::emitSnippetBody()
   {
   uint8_t *cursor = cg()->getBinaryBufferCursor();
   TR::SymbolReference *methodSymRef = getNode()->getSymbolReference();
   TR::SymbolReference *glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_ARM64virtualUnresolvedHelper, false, false, false);

   TR::Compilation* comp = cg()->comp();

   getSnippetLabel()->setCodeLocation(cursor);

   // bl glueRef
   *(int32_t *)cursor = cg()->encodeHelperBranchAndLink(glueRef, cursor, getNode());
   cursor += 4;

   // Store the code cache RA
   *(intptrj_t *)cursor = (intptrj_t)getReturnLabel()->getCodeLocation();
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
                               cursor,
                               NULL,
                               TR_AbsoluteMethodAddress, cg()),
                               __FILE__, __LINE__, getNode());
   cursor += 8;

   // CP
   *(intptrj_t *)cursor = (intptrj_t)methodSymRef->getOwningMethod(comp)->constantPool();
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
                               cursor,
                               *(uint8_t **)cursor,
                               getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                               TR_Thunks, cg()),
                               __FILE__, __LINE__, getNode());
   cursor += 8;

   // CP index
   *(int32_t *)cursor = methodSymRef->getCPIndexForVM();

   return cursor + 4;
   }

uint32_t TR::ARM64VirtualUnresolvedSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return 24;
   }

uint8_t *TR::ARM64InterfaceCallSnippet::emitSnippetBody()
   {
   uint8_t *cursor = cg()->getBinaryBufferCursor();
   TR::SymbolReference *methodSymRef = getNode()->getSymbolReference();
   TR::SymbolReference *glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_ARM64interfaceCallHelper, false, false, false);

   getSnippetLabel()->setCodeLocation(cursor);

   // bl glueRef
   *(int32_t *)cursor = cg()->encodeHelperBranchAndLink(glueRef, cursor, getNode());
   cursor += 4;

   // Store the code cache RA
   *(intptrj_t *)cursor = (intptrj_t)getReturnLabel()->getCodeLocation();
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
                               cursor,
                               NULL,
                               TR_AbsoluteMethodAddress, cg()),
                               __FILE__, __LINE__, getNode());
   cursor += 8;

   // CP
   *(intptrj_t *)cursor = (intptrj_t)methodSymRef->getOwningMethod(cg()->comp())->constantPool();
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
                               cursor,
                               *(uint8_t **)cursor,
                               getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                               TR_Thunks, cg()),
                               __FILE__, __LINE__, getNode());
   cursor += 8;

   // CP index
   *(int32_t *)cursor = methodSymRef->getCPIndexForVM();
   cursor += 4;

   // Add 2 more slots for resolved values
   *(int32_t *)cursor = 0;
   cursor += 4;
   *(int32_t *)cursor = 0;
   cursor += 4;

   return cursor;
   }

uint32_t TR::ARM64InterfaceCallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return 32;
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

   // movz x15, low 16 bits
   *(int32_t *)buffer = 0xD280000F | ((dispatcher & 0xFFFF) << 5);
   buffer += 4;
   // movk x15, next 16 bits, lsl #16
   *(int32_t *)buffer = 0xF2A0000F | (((dispatcher >> 16) & 0xFFFF) << 5);
   buffer += 4;
   // movk x15, next 16 bits, lsl #32
   *(int32_t *)buffer = 0xF2C0000F | (((dispatcher >> 32) & 0xFFFF) << 5);
   buffer += 4;
   // movk x15, next 16 bits, lsl #48
   *(int32_t *)buffer = 0xF2E0000F | (((dispatcher >> 48) & 0xFFFF) << 5);
   buffer += 4;
   // br x15
   *(int32_t *)buffer = 0xD61F01E0;
   buffer += 4;

   *((int32_t *)thunk + 1) = buffer - returnValue;  // patch offset for AOT relocation
   *(int32_t *)thunk = buffer - returnValue;        // patch size of thunk

#ifdef TR_HOST_ARM64
   arm64CodeSync(thunk, codeSize);
#endif

   return returnValue;
   }

TR_J2IThunk *TR::ARM64CallSnippet::generateInvokeExactJ2IThunk(TR::Node *callNode, int32_t argSize, TR::CodeGenerator *cg, char *signature)
   {
   int32_t codeSize = 4 * (instructionCountForArguments(callNode, cg) + 5); // 5 instructions for branch
   intptr_t dispatcher;
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

   // movz x15, low 16 bits
   *(int32_t *)buffer = 0xD280000F | (((UDATA)dispatcher & 0xFFFF) << 5);
   buffer += 4;
   // movk x15, next 16 bits, lsl #16
   *(int32_t *)buffer = 0xF2A0000F | ((((UDATA)dispatcher >> 16) & 0xFFFF) << 5);
   buffer += 4;
   // movk x15, next 16 bits, lsl #32
   *(int32_t *)buffer = 0xF2C0000F | ((((UDATA)dispatcher >> 32) & 0xFFFF) << 5);
   buffer += 4;
   // movk x15, next 16 bits, lsl #48
   *(int32_t *)buffer = 0xF2E0000F | ((((UDATA)dispatcher >> 48) & 0xFFFF) << 5);
   buffer += 4;
   // br x15
   *(int32_t *)buffer = 0xD61F01E0;
   buffer += 4;

#ifdef TR_HOST_ARM64
   arm64CodeSync(thunk->entryPoint(), codeSize);
#endif

   return thunk;
   }

