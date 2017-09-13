/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#include "z/codegen/S390J9CallSnippet.hpp"

#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/InstOpCode.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/J2IThunk.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "z/codegen/TRSystemLinkage.hpp"

uint8_t *
TR::S390J9CallSnippet::generateVIThunk(TR::Node * callNode, int32_t argSize, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   int32_t lengthOfVIThunk = (TR::Compiler->target.is64Bit()) ? 18 : 12;
   int32_t codeSize = instructionCountForArguments(callNode, cg) + lengthOfVIThunk;
   uint32_t rEP = (uint32_t) cg->getEntryPointRegister() - 1;

   // make it double-word aligned
   codeSize = (codeSize + 7) / 8 * 8 + 8; // Additional 4 bytes to hold size of thunk
   uint8_t * thunk, * cursor, * returnValue;
   TR::SymbolReference *dispatcherSymbol;

   if (comp->compileRelocatableCode())
      thunk = (uint8_t *)comp->trMemory()->allocateMemory(codeSize, heapAlloc);
   else
      thunk = (uint8_t *)cg->allocateCodeMemory(codeSize, true);

   cursor = returnValue = thunk + 8;

   switch (callNode->getDataType())
      {
      case TR::NoType:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390icallVMprJavaSendVirtual0, false, false, false);
         break;
      case TR::Int32:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390icallVMprJavaSendVirtual1, false, false, false);
         break;
      case TR::Address:
         if (TR::Compiler->target.is64Bit())
            {
            dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390icallVMprJavaSendVirtualJ, false, false, false);
            }
         else
            {
            dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390icallVMprJavaSendVirtual1, false, false, false);
            }

         break;
      case TR::Int64:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390icallVMprJavaSendVirtualJ, false, false, false);
         break;
      case TR::Float:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390icallVMprJavaSendVirtualF, false, false, false);
         break;
      case TR::Double:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390icallVMprJavaSendVirtualD, false, false, false);
         break;
      default:
         TR_ASSERT(0, "Bad return data type for a call node.  DataType was %s\n",
            comp->getDebug()->getName(callNode->getDataType()));
      }

   cursor = S390flushArgumentsToStack(cursor, callNode, argSize, cg);

   // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   //  if you change the following code,
   //  make sure 'lengthOfVIThunk' and hence 'codeSize' defined above
   //  are large enough to cover the VIThunk code.
   // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   //  branch to the dispatcher
   //
   //  0d40        BASR  rEP,0
   //  5840 400a   L     rEP,6(,rEP)  or     LG   rEP, 6(,rEP) for 64bit
   //  0de4        BCR  ,rEP
   //              <method address>

   *(int16_t *) cursor = 0x0d00 + (((int16_t) rEP) << 4);             // BASR   rEP,0
   cursor += 2;
   if (TR::Compiler->target.is64Bit())
      {
      *(uint32_t *) cursor = 0xe3000008 + (rEP << 12) + (rEP << 20);  // LG      rEP,8(,rEP)
      cursor += 4;
      *(uint16_t *) cursor = 0x0004;
      cursor += 2;
      }
   else
      {
      *(int32_t *) cursor = 0x58000006 + (rEP << 12) + (rEP << 20);   // L      rEP,6(,rEP)
      cursor += 4;
      }

   *(int16_t *) cursor = 0x07f0 + (int16_t) rEP;                      // BCR    rEP
   cursor += 2;

   *((int32_t *)thunk + 1) = cursor - returnValue;  // patch offset for AOT relocation

   *(uintptrj_t *) cursor = (uintptrj_t) dispatcherSymbol->getMethodAddress();

   cursor += sizeof(uintptrj_t);

   *(int32_t *)thunk = cursor - returnValue; // patch size of thunk

   return returnValue;
   }

TR_J2IThunk *
TR::S390J9CallSnippet::generateInvokeExactJ2IThunk(TR::Node * callNode, int32_t argSize, char* signature, TR::CodeGenerator * cg)
   {
   uint32_t rEP = (uint32_t) cg->getEntryPointRegister() - 1;
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   bool verbose = comp->getOptions()->getVerboseOption(TR_VerboseJ2IThunks);
   int32_t finalCallLength = verbose? 6 : 2;
   int32_t lengthOfIEThunk = finalCallLength + (TR::Compiler->target.is64Bit() ? 16 : 10);
   int32_t codeSize = instructionCountForArguments(callNode, cg) + lengthOfIEThunk;
   // TODO:JSR292: VI thunks have code to ensure they are double-word aligned.  Do we need that here?

   TR_J2IThunkTable *thunkTable = comp->getPersistentInfo()->getInvokeExactJ2IThunkTable();
   TR_J2IThunk      *thunk      = TR_J2IThunk::allocate(codeSize, signature, cg, thunkTable);
   uint8_t          *cursor     = thunk->entryPoint();

   TR::SymbolReference *dispatcherSymbol;
   switch (callNode->getDataType())
      {
      case TR::NoType:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExact0, false, false, false);
         break;
      case TR::Int32:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExact1, false, false, false);
         break;
      case TR::Address:
         if (TR::Compiler->target.is64Bit())
            dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactJ, false, false, false);
         else
            dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExact1, false, false, false);
         break;
      case TR::Int64:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactJ, false, false, false);
         break;
      case TR::Float:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactF, false, false, false);
         break;
      case TR::Double:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactD, false, false, false);
         break;
      default:
         TR_ASSERT(0, "Bad return data type for a call node.  DataType was %s\n",
            comp->getDebug()->getName(callNode->getDataType()));
      }

   cursor = S390flushArgumentsToStack(cursor, callNode, argSize, cg);

   // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   //  if you change the following code,
   //  make sure 'lengthOfVIThunk' and hence 'codeSize' defined above
   //  are large enough to cover the VIThunk code.
   // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   //  branch to the dispatcher
   //
   //  0d40        BASR  rEP,0
   //  5840 400a   L     rEP,6(,rEP)  or     LG   rEP, 8(,rEP) for 64bit
   //  0de4        BCR  ,rEP    -- OR CALL TO methodHandleJ2IGlue --
   //              <method address>

   *(int16_t *) cursor = 0x0d00 + (((int16_t) rEP) << 4);             // BASR   rEP,0
   cursor += 2;
   if (TR::Compiler->target.is64Bit())
      {
      *(uint32_t *) cursor = 0xe3000006 + finalCallLength + (rEP << 12) + (rEP << 20);  // LG      rEP,8(,rEP)
      cursor += 4;
      *(uint16_t *) cursor = 0x0004;
      cursor += 2;sizeof(int16_t);
      }
   else
      {
      *(int32_t *) cursor = 0x58000004 + finalCallLength + (rEP << 12) + (rEP << 20);   // L      rEP,6(,rEP)
      cursor += 4;
      }

   uintptrj_t helperAddress = (uintptrj_t)dispatcherSymbol->getMethodAddress();
   if (verbose)
      {
      *(int16_t *) cursor = 0xC0F4;   // BRCL   <Helper Addr>
      cursor += 2;

      TR::SymbolReference *helper = cg->symRefTab()->findOrCreateRuntimeHelper(TR_methodHandleJ2IGlue, false, false, false);
      uintptrj_t destAddr = (uintptrj_t)helper->getMethodAddress();
#if defined(TR_TARGET_64BIT) 
#if defined(J9ZOS390) 
      if (comp->getOption(TR_EnableRMODE64))
#endif
         {
         if (NEEDS_TRAMPOLINE(destAddr, cursor, cg))
            destAddr = fej9->indexedTrampolineLookup(helper->getReferenceNumber(), (void *)cursor);
         }
#endif
      TR_ASSERT(CHECK_32BIT_TRAMPOLINE_RANGE(destAddr, cursor), "Helper Call must be reachable");
      *(int32_t *) cursor = (int32_t)((destAddr - (intptrj_t)(cursor - 2)) / 2);
      cursor += 4;
      }
   else
      {
      *(int16_t *) cursor = 0x07f0 + (int16_t) rEP;                      // BCR    rEP
      cursor += 2;
      }

   *(uintptrj_t *) cursor = (uintptrj_t) dispatcherSymbol->getMethodAddress();
   cursor += sizeof(uintptrj_t);

   diagnostic("\n-- ( Created invokeExact J2I thunk " POINTER_PRINTF_FORMAT " for node " POINTER_PRINTF_FORMAT " )", thunk, callNode);

   TR_ASSERT(cursor == thunk->entryPoint() + codeSize, "Must allocate correct amount of memory for invokeExact J2I thunk %p.  Allocated %d bytes but consumed %d = %d plus argument spills", thunk->entryPoint(), codeSize, cursor - thunk->entryPoint(), lengthOfIEThunk);

   if (verbose)
      TR_VerboseLog::writeLineLocked(TR_Vlog_J2I, "Created J2I thunk %s @ %p while compiling %s", thunk->terseSignature(), thunk->entryPoint(), comp->signature());

   return thunk;
   }


uint8_t *
TR::S390J9CallSnippet::emitSnippetBody()
   {
   TR::Compilation *comp = cg()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   // *this   swipeable for debugger
   uint8_t * cursor = cg()->getBinaryBufferCursor();
   TR::Node * callNode = getNode();
   TR::SymbolReference * methodSymRef =  getRealMethodSymbolReference();

   if (!methodSymRef)
      methodSymRef = callNode->getSymbolReference();

   TR::MethodSymbol * methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

   AOTcgDiag1(comp, "TR::S390CallSnippet::emitSnippetBody cursor=%x\n", cursor);
   getSnippetLabel()->setCodeLocation(cursor);

   // Flush in-register arguments back to the stack for interpreter
   cursor = S390flushArgumentsToStack(cursor, callNode, getSizeOfArguments(), cg());

   TR_RuntimeHelper runtimeHelper = getInterpretedDispatchHelper(methodSymRef, callNode->getDataType());
   TR::SymbolReference * glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(runtimeHelper, false, false, false);

#if !defined(PUBLIC_BUILD)
   // Generate RIOFF if RI is supported.
   cursor = generateRuntimeInstrumentationOnOffInstruction(cg(), cursor, TR::InstOpCode::RIOFF);
#endif

   // data area start address
   uintptrj_t dataStartAddr = (uintptrj_t) (getPICBinaryLength(cg()) + cursor);

   // calculate pad bytes to get the data area aligned
   int32_t pad_bytes = (dataStartAddr + (sizeof(uintptrj_t) - 1)) / sizeof(uintptrj_t) * sizeof(uintptrj_t) - dataStartAddr;

   setPadBytes(pad_bytes);

   //  branch to the glueRef
   //
   //  0d40        BASR  rEP,0
   //  5840 4006   L     rEP,6(,rEP)   LG   rEP, 8(rEP) for 64bit
   //  0de4        BASR  r14,rEP

   cursor = generatePICBinary(cg(), cursor, glueRef);

   // add NOPs to make sure the data area is aligned
   if (pad_bytes == 2)
      {
      *(int16_t *) cursor = 0x0000;                     // padding 2-bytes
      cursor += 2;
      }
   else if (TR::Compiler->target.is64Bit())
      {
      if (pad_bytes == 4)                                // padding 4-bytes
      {
         *(int32_t *) cursor = 0x00000000;
         cursor += 4;
         }
      else if (pad_bytes == 6)                          // padding 6-bytes
      {
         *(int32_t *) cursor = 0x00000000;
         cursor += 4;
         *(uint16_t *) cursor = 0x0000;
         cursor += 2;
         }
      }

   // Data Area
   //              <method address>
   //              code cache RA
   //              method pointer

   pad_bytes = (((uintptrj_t) cursor + (sizeof(uintptrj_t) - 1)) / sizeof(uintptrj_t) * sizeof(uintptrj_t) - (uintptrj_t) cursor);
   TR_ASSERT( pad_bytes == 0, "Method address field must be aligned for patching");

   // Method address
   *(uintptrj_t *) cursor = (uintptrj_t) glueRef->getMethodAddress();
   AOTcgDiag1(comp, "add TR_AbsoluteHelperAddress cursor=%x\n", cursor);
   cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)glueRef, TR_AbsoluteHelperAddress, cg()),
                             __FILE__, __LINE__, callNode);
   cursor += sizeof(uintptrj_t);

   // Store the code cache RA
   *(uintptrj_t *) cursor = (uintptrj_t) getCallRA();
   AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", cursor);
   cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, NULL, TR_AbsoluteMethodAddress, cg()),
                          __FILE__, __LINE__, callNode);
   cursor += sizeof(uintptrj_t);

   //induceOSRAtCurrentPC is implemented in the VM, and it knows, by looking at the current PC, what method it needs to
   //continue execution in interpreted mode. Therefore, it doesn't need the method pointer.
   if (runtimeHelper != TR_induceOSRAtCurrentPC)
      {
      // Store the method pointer: it is NULL for unresolved
      // This field must be doubleword aligned for 64-bit and word aligned for 32-bit
      if (methodSymRef->isUnresolved() || comp->compileRelocatableCode())
         {
         pad_bytes = (((uintptrj_t) cursor + (sizeof(uintptrj_t) - 1)) / sizeof(uintptrj_t) * sizeof(uintptrj_t) - (uintptrj_t) cursor);
         TR_ASSERT( pad_bytes == 0, "Method Pointer field must be aligned for patching");
         *(uintptrj_t *) cursor = 0;
         if (comp->getOption(TR_EnableHCR))
            {
            //TODO check what happens when we pass -1 to jitAddPicToPatchOnClassRedefinition an dif it's correct in this case
            cg()->jitAddPicToPatchOnClassRedefinition((void*)-1, (void *)cursor, true);
            cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation((uint8_t *)cursor, NULL,
                                    TR_HCR, cg()),__FILE__, __LINE__, getNode());
            }
         }
      else
         {
         *(uintptrj_t *) cursor = (uintptrj_t) methodSymbol->getMethodAddress();
         if (comp->getOption(TR_EnableHCR))
            cg()->jitAddPicToPatchOnClassRedefinition((void *)methodSymbol->getMethodAddress(), (void *)cursor);
         AOTcgDiag1(comp, "add TR_MethodObject cursor=%x\n", cursor);
         cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) callNode->getSymbolReference(), getNode() ? (uint8_t *)(intptr_t)getNode()->getInlinedSiteIndex() : (uint8_t *)-1, TR_MethodObject, cg()),
                                   __FILE__, __LINE__, callNode);
         }
      }

   return cursor + sizeof(uintptrj_t);
   }

uint8_t *
TR::S390UnresolvedCallSnippet::emitSnippetBody()
   {
   TR::Compilation *comp = cg()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   // *this   swipeable for debugger
   uint8_t * cursor = TR::S390J9CallSnippet::emitSnippetBody();

   TR::SymbolReference * methodSymRef = getNode()->getSymbolReference();
   TR::MethodSymbol * methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   int32_t helperLookupOffset;

   switch (getNode()->getDataType())
      {
      case TR::NoType:
         helperLookupOffset = 0;
         break;
      case TR::Int32:
         helperLookupOffset = TR::Compiler->om.sizeofReferenceAddress();
         break;
      case TR::Address:
         if (TR::Compiler->target.is64Bit())
            {
            helperLookupOffset = 2 * TR::Compiler->om.sizeofReferenceAddress();
            }
         else
            {
            helperLookupOffset = TR::Compiler->om.sizeofReferenceAddress();
            }

         break;
      case TR::Int64:
         helperLookupOffset = 2 * TR::Compiler->om.sizeofReferenceAddress();
         break;
      case TR::Float:
         helperLookupOffset = 3 * TR::Compiler->om.sizeofReferenceAddress();
         break;
      case TR::Double:
         helperLookupOffset = 4 * TR::Compiler->om.sizeofReferenceAddress();
         break;
      }

   // Constant Pool
   *(uintptrj_t *) cursor = (uintptrj_t) methodSymRef->getOwningMethod(comp)->constantPool();
   AOTcgDiag1(comp, "add TR_ConstantPool cursor=%x\n", cursor);

#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390) 
   if (comp->getOption(TR_EnableRMODE64))
#endif
      {
      cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, *(uint8_t **)cursor, getNode() ? (uint8_t *)(intptr_t)getNode()->getInlinedSiteIndex() : (uint8_t *)-1, 
         TR_Trampolines, cg()),
         __FILE__, __LINE__,getNode());
      }
#if defined(J9ZOS390)
   else
      {
      cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, *(uint8_t **)cursor, getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1, TR_ConstantPool, cg()),
         __FILE__, __LINE__, getNode());
      }
#endif
#else
   cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, *(uint8_t **)cursor, getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1, TR_ConstantPool, cg()),
      __FILE__, __LINE__, getNode());
#endif

   cursor += sizeof(uintptrj_t);

   // Constant Pool Index
   *(uint32_t *) cursor = (helperLookupOffset << 24) | methodSymRef->getCPIndexForVM();

#if defined(TR_TARGET_64BIT) 
#if defined(J9ZOS390)
   if (comp->getOption(TR_EnableRMODE64))
#endif
      {
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
      }
#endif

   return cursor + sizeof(int32_t);
   }

uint32_t
TR::S390UnresolvedCallSnippet::getLength(int32_t  estimatedSnippetStart)
   {
   // *this   swipeable for debugger
   return TR::S390CallSnippet::getLength(estimatedSnippetStart) + sizeof(uintptrj_t) + sizeof(int32_t);
   }

uint8_t *
TR::S390VirtualSnippet::emitSnippetBody()
   {
   // *this   swipeable for debugger
   return NULL;
   }

uint32_t
TR::S390VirtualSnippet::getLength(int32_t  estimatedSnippetStart)
   {
   return 0;
   }

uint8_t *
TR::S390VirtualUnresolvedSnippet::emitSnippetBody()
   {
   // *this   swipeable for debugger
   uint8_t * cursor = cg()->getBinaryBufferCursor();
   TR::Node * callNode = getNode();
   TR::Compilation *comp = cg()->comp();
   TR::SymbolReference * glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390virtualUnresolvedHelper, false, false, false);

   getSnippetLabel()->setCodeLocation(cursor);

   // Generate RIOFF if RI is supported.
#if !defined(PUBLIC_BUILD)
   cursor = generateRuntimeInstrumentationOnOffInstruction(cg(), cursor, TR::InstOpCode::RIOFF);
#endif

   cursor = generatePICBinary(cg(), cursor, glueRef);

   // Method address
   *(uintptrj_t *) cursor = (uintptrj_t) glueRef->getMethodAddress();
   AOTcgDiag1(comp, "add TR_AbsoluteHelperAddress cursor=%x\n", cursor);
   cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)glueRef, TR_AbsoluteHelperAddress, cg()),
                             __FILE__, __LINE__, callNode);
   cursor += sizeof(uintptrj_t);

   // Store the code cache RA
   *(uintptrj_t *) cursor = (uintptrj_t) getCallRA();
   AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", cursor);
   cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, NULL, TR_AbsoluteMethodAddress, cg()),
                             __FILE__, __LINE__, callNode);
   cursor += sizeof(uintptrj_t);

   *(uintptrj_t *) cursor = (uintptrj_t) callNode->getSymbolReference()->getOwningMethod(comp)->constantPool();

   AOTcgDiag1(comp, "add TR_Thunks cursor=%x\n", cursor);
   cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, *(uint8_t **)cursor, getNode() ? (uint8_t *)(intptr_t)getNode()->getInlinedSiteIndex() : (uint8_t *)-1, TR_Thunks, cg()),
                             __FILE__, __LINE__, callNode);

   cursor += sizeof(uintptrj_t);

   //  save CPIndex as sign extended 8 byte value on 64bit as it's assumed in J9 helpers -- def#63837
   *(uintptrj_t *) cursor = (uintptrj_t) callNode->getSymbolReference()->getCPIndexForVM();
   cursor += TR::Compiler->om.sizeofReferenceAddress();

   *(uintptrj_t *) cursor = (uintptrj_t) (getPatchVftInstruction()->getBinaryEncoding());
   AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", cursor);
   cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, NULL, TR_AbsoluteMethodAddress, cg()),
                          __FILE__, __LINE__, callNode);
   cursor += sizeof(uintptrj_t);

   return cursor;
   }

uint32_t
TR::S390VirtualUnresolvedSnippet::getLength(int32_t  estimatedSnippetStart)
   {
   TR::Compilation* comp = cg()->comp();
   uint32_t length = getPICBinaryLength(cg()) + 4 * sizeof(uintptrj_t) + TR::Compiler->om.sizeofReferenceAddress();
#if !defined(PUBLIC_BUILD)
   length += getRuntimeInstrumentationOnOffInstructionLength(cg());
#endif
   return length;
   }

TR::S390InterfaceCallSnippet::S390InterfaceCallSnippet(
      TR::CodeGenerator *cg,
      TR::Node *c,
      TR::LabelSymbol *lab,
      int32_t s,
      int8_t n,
      bool useCLFIandBRCL)
   : TR::S390VirtualSnippet(cg, c, lab, s),
      _numInterfaceCallCacheSlots(n),
      _useCLFIandBRCL(useCLFIandBRCL)
   {
   _dataSnippet = new (cg->trHeapMemory()) TR::S390InterfaceCallDataSnippet(cg,c,n);
   cg->addDataConstantSnippet(_dataSnippet);
   }


TR::S390InterfaceCallSnippet::S390InterfaceCallSnippet(
      TR::CodeGenerator *cg,
      TR::Node *c,
      TR::LabelSymbol *lab,
      int32_t s,
      int8_t n,
      uint8_t *thunkPtr,
      bool useCLFIandBRCL)
   : TR::S390VirtualSnippet(cg, c, lab, s),
      _numInterfaceCallCacheSlots(n),
      _useCLFIandBRCL(useCLFIandBRCL)
   {
   _dataSnippet = new (cg->trHeapMemory()) TR::S390InterfaceCallDataSnippet(cg,c,n,thunkPtr);
   cg->addDataConstantSnippet(_dataSnippet);
   }


uint8_t *
TR::S390InterfaceCallSnippet::emitSnippetBody()
   {
   // *this   swipeable for debugger
   uint8_t * cursor = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(cursor);
   TR::Compilation *comp = cg()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   uint32_t rEP = (uint32_t) cg()->getEntryPointRegister() - 1;
   TR::SymbolReference * glueRef ;

   if (getNumInterfaceCallCacheSlots() == 0)
      {
      glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390interfaceCallHelper, false, false, false);
      }
   else if (comp->getOption(TR_enableInterfaceCallCachingSingleDynamicSlot))
      {
      glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390interfaceCallHelperSingleDynamicSlot, false, false, false);
      }
   else
      {
      glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390interfaceCallHelperMultiSlots, false, false, false);
      }

   // Set up the start of data constants and jump to helper.
   //    LARL  r14, <Start of DC>
   //    BRCL  <Helper Addr>

   // LARL - Add Relocation the data constants to this LARL.
   cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, getDataConstantSnippet()->getSnippetLabel()));

   *(int16_t *) cursor = 0xC0E0;
   cursor += sizeof(int16_t);

   // Place holder.  Proper offset will be calulated by relocation.
   *(int32_t *) cursor = 0xDEADBEEF;
   cursor += sizeof(int32_t);

   // BRCL
   *(int16_t *) cursor = 0xC0F4;
   cursor += sizeof(int16_t);

   // Calculate the relative offset to get to helper method.
   // If MCC is not supported, everything should be reachable.
   // If MCC is supported, we will look up the appropriate trampoline, if
   //     necessary.
   intptrj_t destAddr = (intptrj_t)(glueRef->getMethodAddress());

#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
   if (comp->getOption(TR_EnableRMODE64))
#endif
      {
      if (NEEDS_TRAMPOLINE(destAddr, cursor, cg()))
         {
         // Destination is beyond our reachable jump distance, we'll find the
         // trampoline.
         destAddr = fej9->indexedTrampolineLookup(glueRef->getReferenceNumber(), (void *)cursor);
         this->setUsedTrampoline(true);
         }
      }
#endif

   TR_ASSERT(CHECK_32BIT_TRAMPOLINE_RANGE(destAddr, cursor), "Helper Call is not reachable.");
   this->setSnippetDestAddr(destAddr);

   *(int32_t *) cursor = (int32_t)((destAddr - (intptrj_t)(cursor - 2)) / 2);
   AOTcgDiag1(comp, " add TR_HelperAddress cursor=%x\n", cursor);
   cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t*) glueRef, TR_HelperAddress, cg()),
            __FILE__, __LINE__, getNode());
   cursor += sizeof(int32_t);

   // Set up the code RA to the data snippet.
   getDataConstantSnippet()->setCodeRA(getCallRA());

   return cursor;
   }

uint32_t
TR::S390InterfaceCallSnippet::getLength(int32_t  estimatedSnippetStart)
   {
   return 12;  // LARL + BRCL
   }



void
TR_Debug::print(TR::FILE *pOutFile, TR::S390UnresolvedCallSnippet * snippet)
   {
   uint8_t * bufferPos = snippet->getSnippetLabel()->getCodeLocation() + snippet->getLength(0)
                         - sizeof(intptrj_t) - 4            // 2 DC's at end of this snippet.
                         - (4 - snippet->getPadBytes());    // padding

   TR::SymbolReference * methodSymRef = snippet->getNode()->getSymbolReference();

   int32_t helperLookupOffset;
   switch (snippet->getNode()->getDataType())
      {
      case TR::NoType:
         helperLookupOffset = 0;
         break;
      case TR::Int32:
      case TR::Address:
         helperLookupOffset = 4;
         break;
      case TR::Int64:
         helperLookupOffset = 8;
         break;
      case TR::Float:
         helperLookupOffset = 12;
         break;
      case TR::Double:
         helperLookupOffset = 16;
         break;
      }
   helperLookupOffset <<= 24;

   print(pOutFile, (TR::S390CallSnippet *) snippet);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(uintptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# Address Of Constant Pool", getOwningMethod(methodSymRef)->constantPool());
   bufferPos += sizeof(uintptrj_t);

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "DC   \t0x%08x \t\t# Offset | Flag | CP Index", helperLookupOffset | methodSymRef->getCPIndexForVM());
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::S390VirtualSnippet * snippet)
   {
   uint8_t * bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "Virtual Call Snippet");
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::S390VirtualUnresolvedSnippet * snippet)
   {
   uint8_t * bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   TR::SymbolReference * methodSymRef = snippet->getNode()->getSymbolReference();
   TR::SymbolReference * glueRef = _cg->getSymRef(TR_S390virtualUnresolvedHelper);

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "Virtual Unresolved Call Snippet");

   bufferPos = printRuntimeInstrumentationOnOffInstruction(pOutFile, bufferPos, false); // RIOFF

   printPrefix(pOutFile, NULL, bufferPos, 6);
   trfprintf(pOutFile, "BRASL \tGPR14, <%p>\t# Branch to Helper Method %s",
                    snippet->getSnippetDestAddr(),
                    snippet->usedTrampoline()?"- Trampoline Used.":"");
   bufferPos += 6;

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "DC   \t%p\t\t# Method Address", *((uintptrj_t *)bufferPos));
   bufferPos += 4;

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# Call Site RA", snippet->getCallRA());
   bufferPos += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# Address Of Constant Pool", (intptrj_t) getOwningMethod(methodSymRef)->constantPool());
   bufferPos += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t0x%08x \t\t# CP Index", methodSymRef->getCPIndexForVM());
   bufferPos += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# Instruction to be patched with vft offset", snippet->getPatchVftInstruction()->getBinaryEncoding());
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::S390InterfaceCallSnippet * snippet)
   {
   uint8_t * bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   TR::SymbolReference * methodSymRef = snippet->getNode()->getSymbolReference();
   TR::SymbolReference * glueRef = _cg->getSymRef(TR_S390interfaceCallHelperMultiSlots);
   int i;

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "Interface Call Snippet");

   printPrefix(pOutFile, NULL, bufferPos, 6);
   trfprintf(pOutFile, "LARL \tGPR14, <%p>\t# Addr of DataConst",
                                (intptrj_t) snippet->getDataConstantSnippet()->getSnippetLabel()->getCodeLocation());
   bufferPos += 6;

   printPrefix(pOutFile, NULL, bufferPos, 6);
   trfprintf(pOutFile, "BRCL \t<%p>\t\t# Branch to Helper %s",
                   snippet->getSnippetDestAddr(),
                   snippet->usedTrampoline()?"- Trampoline Used.":"");
   bufferPos += 6;
   }
