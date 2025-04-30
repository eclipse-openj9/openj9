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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
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
#include "env/VerboseLog.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/Runtime.hpp"
#include "runtime/RuntimeAssumptions.hpp"

uint8_t *
TR::S390J9CallSnippet::generateVIThunk(TR::Node * callNode, int32_t argSize, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   int32_t lengthOfVIThunk = (comp->target().is64Bit()) ? 18 : 12;
   int32_t codeSize = instructionCountForArguments(callNode, cg) + lengthOfVIThunk;
   uint32_t rEP = (uint32_t) cg->getEntryPointRegister() - 1;

   // make it double-word aligned
   codeSize = (codeSize + 7) / 8 * 8 + 8; // Additional 4 bytes to hold size of thunk
   uint8_t * thunk, * cursor, * returnValue;
   TR::SymbolReference *dispatcherSymbol = NULL;

   if (fej9->storeOffsetToArgumentsInVirtualIndirectThunks())
      thunk = (uint8_t *)comp->trMemory()->allocateMemory(codeSize, heapAlloc);
   else
      thunk = (uint8_t *)cg->allocateCodeMemory(codeSize, true);

   cursor = returnValue = thunk + 8;

   switch (callNode->getDataType())
      {
      case TR::NoType:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390icallVMprJavaSendVirtual0);
         break;
      case TR::Int32:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390icallVMprJavaSendVirtual1);
         break;
      case TR::Address:
         if (comp->target().is64Bit())
            {
            dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390icallVMprJavaSendVirtualJ);
            }
         else
            {
            dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390icallVMprJavaSendVirtual1);
            }

         break;
      case TR::Int64:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390icallVMprJavaSendVirtualJ);
         break;
      case TR::Float:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390icallVMprJavaSendVirtualF);
         break;
      case TR::Double:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390icallVMprJavaSendVirtualD);
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
   if (comp->target().is64Bit())
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

   *(uintptr_t *) cursor = (uintptr_t) dispatcherSymbol->getMethodAddress();

   cursor += sizeof(uintptr_t);

   *(int32_t *)thunk = cursor - returnValue; // patch size of thunk

   return returnValue;
   }

TR_MHJ2IThunk *
TR::S390J9CallSnippet::generateInvokeExactJ2IThunk(TR::Node * callNode, int32_t argSize, char* signature, TR::CodeGenerator * cg)
   {
   uint32_t rEP = (uint32_t) cg->getEntryPointRegister() - 1;
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   bool verbose = comp->getOptions()->getVerboseOption(TR_VerboseJ2IThunks);
   int32_t finalCallLength = verbose? 6 : 2;
   int32_t lengthOfIEThunk = finalCallLength + (comp->target().is64Bit() ? 16 : 10);
   int32_t codeSize = instructionCountForArguments(callNode, cg) + lengthOfIEThunk;
   // TODO:JSR292: VI thunks have code to ensure they are double-word aligned.  Do we need that here?

   TR_MHJ2IThunkTable *thunkTable = comp->getPersistentInfo()->getInvokeExactJ2IThunkTable();
   TR_MHJ2IThunk      *thunk      = TR_MHJ2IThunk::allocate(codeSize, signature, cg, thunkTable);
   uint8_t          *cursor     = thunk->entryPoint();

   TR::SymbolReference *dispatcherSymbol = NULL;
   switch (callNode->getDataType())
      {
      case TR::NoType:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExact0);
         break;
      case TR::Int32:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExact1);
         break;
      case TR::Address:
         if (comp->target().is64Bit())
            dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactJ);
         else
            dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExact1);
         break;
      case TR::Int64:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactJ);
         break;
      case TR::Float:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactF);
         break;
      case TR::Double:
         dispatcherSymbol = cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactD);
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
   if (comp->target().is64Bit())
      {
      *(uint32_t *) cursor = 0xe3000006 + finalCallLength + (rEP << 12) + (rEP << 20);  // LG      rEP,8(,rEP)
      cursor += 4;
      *(uint16_t *) cursor = 0x0004;
      cursor += sizeof(int16_t);
      }
   else
      {
      *(int32_t *) cursor = 0x58000004 + finalCallLength + (rEP << 12) + (rEP << 20);   // L      rEP,6(,rEP)
      cursor += 4;
      }

   uintptr_t helperAddress = (uintptr_t)dispatcherSymbol->getMethodAddress();
   if (verbose)
      {
      *(int16_t *) cursor = 0xC0F4;   // BRCL   <Helper Addr>
      cursor += 2;

      TR::SymbolReference *helper = cg->symRefTab()->findOrCreateRuntimeHelper(TR_methodHandleJ2IGlue);
      intptr_t destAddr = (intptr_t)helper->getMethodAddress();
#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
      if (comp->getOption(TR_EnableRMODE64))
#endif
         {
         if (cg->directCallRequiresTrampoline(destAddr, reinterpret_cast<intptr_t>(cursor)))
            destAddr = TR::CodeCacheManager::instance()->findHelperTrampoline(helper->getReferenceNumber(), (void *)cursor);
         }
#endif
      TR_ASSERT(CHECK_32BIT_TRAMPOLINE_RANGE(destAddr, cursor), "Helper Call must be reachable");
      *(int32_t *) cursor = (int32_t)((destAddr - (intptr_t)(cursor - 2)) / 2);
      cursor += 4;
      }
   else
      {
      *(int16_t *) cursor = 0x07f0 + (int16_t) rEP;                      // BCR    rEP
      cursor += 2;
      }

   *(uintptr_t *) cursor = (uintptr_t) cg->fej9()->getInvokeExactThunkHelperAddress(comp, dispatcherSymbol, callNode->getDataType());
   cursor += sizeof(uintptr_t);

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

   uint8_t * cursor = cg()->getBinaryBufferCursor();
   TR::Node * callNode = getNode();
   TR::SymbolReference * methodSymRef =  getRealMethodSymbolReference();

   if (!methodSymRef)
      methodSymRef = callNode->getSymbolReference();

   TR::MethodSymbol * methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

   getSnippetLabel()->setCodeLocation(cursor);

   // Flush in-register arguments back to the stack for interpreter
   cursor = S390flushArgumentsToStack(cursor, callNode, getSizeOfArguments(), cg());

   TR_RuntimeHelper runtimeHelper = getInterpretedDispatchHelper(methodSymRef, callNode->getDataType());
   TR::SymbolReference * glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(runtimeHelper);

   // Generate RIOFF if RI is supported.
   cursor = generateRuntimeInstrumentationOnOffInstruction(cg(), cursor, TR::InstOpCode::RIOFF);

   // data area start address
   uintptr_t dataStartAddr = (uintptr_t) (getPICBinaryLength() + cursor);

   // calculate pad bytes to get the data area aligned
   int32_t pad_bytes = (dataStartAddr + (sizeof(uintptr_t) - 1)) / sizeof(uintptr_t) * sizeof(uintptr_t) - dataStartAddr;

   setPadBytes(pad_bytes);

   //  branch to the glueRef
   //
   //  0d40        BASR  rEP,0
   //  5840 4006   L     rEP,6(,rEP)   LG   rEP, 8(rEP) for 64bit
   //  0de4        BASR  r14,rEP

   cursor = generatePICBinary(cursor, glueRef);

   // add NOPs to make sure the data area is aligned
   if (pad_bytes == 2)
      {
      *(int16_t *) cursor = 0x0000;                     // padding 2-bytes
      cursor += 2;
      }
   else if (comp->target().is64Bit())
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

   pad_bytes = (((uintptr_t) cursor + (sizeof(uintptr_t) - 1)) / sizeof(uintptr_t) * sizeof(uintptr_t) - (uintptr_t) cursor);
   TR_ASSERT( pad_bytes == 0, "Method address field must be aligned for patching");

   // Method address
   *(uintptr_t *) cursor = (uintptr_t) glueRef->getMethodAddress();
   cg()->addExternalRelocation(
      TR::ExternalRelocation::create(
         cursor,
         (uint8_t *)glueRef,
         TR_AbsoluteHelperAddress,
         cg()),
      __FILE__,
      __LINE__,
      callNode);
   cursor += sizeof(uintptr_t);

   // Store the code cache RA
   *(uintptr_t *) cursor = (uintptr_t) getCallRA();
   cg()->addExternalRelocation(
      TR::ExternalRelocation::create(
         cursor,
         NULL,
         TR_AbsoluteMethodAddress,
         cg()),
      __FILE__,
      __LINE__,
      callNode);
   cursor += sizeof(uintptr_t);

   //induceOSRAtCurrentPC is implemented in the VM, and it knows, by looking at the current PC, what method it needs to
   //continue execution in interpreted mode. Therefore, it doesn't need the method pointer.
   if (!glueRef->isOSRInductionHelper())
      {
      // Store the method pointer: it is NULL for unresolved
      // This field must be doubleword aligned for 64-bit and word aligned for 32-bit
      if (methodSymRef->isUnresolved() || (comp->compileRelocatableCode() && !comp->getOption(TR_UseSymbolValidationManager)))
         {
         pad_bytes = (((uintptr_t) cursor + (sizeof(uintptr_t) - 1)) / sizeof(uintptr_t) * sizeof(uintptr_t) - (uintptr_t) cursor);
         TR_ASSERT( pad_bytes == 0, "Method Pointer field must be aligned for patching");
         *(uintptr_t *) cursor = 0;
         if (comp->getOption(TR_EnableHCR))
            {
            //TODO check what happens when we pass -1 to jitAddPicToPatchOnClassRedefinition an dif it's correct in this case
            cg()->jitAddPicToPatchOnClassRedefinition((void*)-1, (void *)cursor, true);
            cg()->addExternalRelocation(
               TR::ExternalRelocation::create(
                  (uint8_t *)cursor,
                  NULL,
                  TR_HCR,
                  cg()),
               __FILE__,
               __LINE__,
               getNode());
            }
         }
      else
         {
         uintptr_t ramMethod = (uintptr_t)methodSymRef->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod()->getPersistentIdentifier();
         *(uintptr_t *) cursor = ramMethod;
         if (comp->getOption(TR_EnableHCR))
            cg()->jitAddPicToPatchOnClassRedefinition((void *)methodSymbol->getMethodAddress(), (void *)cursor);

         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            TR_ASSERT_FATAL(ramMethod, "cursor = %x, ramMethod can not be null", cursor);
            cg()->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t *)ramMethod,
                  (uint8_t *)TR::SymbolType::typeMethod,
                  TR_SymbolFromManager,
                  cg()),
               __FILE__,
               __LINE__,
               callNode);
            }
#if defined(J9VM_OPT_JITSERVER)
         else if (!comp->isOutOfProcessCompilation()) // Since we query this information from the client, remote compilations don't need to add relocation records for TR_MethodObject
#else
         else
#endif /* defined(J9VM_OPT_JITSERVER) */
            {
            cg()->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t *) callNode->getSymbolReference(),
                  getNode() ? (uint8_t *)(intptr_t)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                  TR_MethodObject,
                  cg()),
               __FILE__,
               __LINE__,
               callNode);
            }
         }
      }

   return cursor + sizeof(uintptr_t);
   }

TR_RuntimeHelper TR::S390J9CallSnippet::getInterpretedDispatchHelper(TR::SymbolReference *methodSymRef, TR::DataType type)
   {
   TR::Compilation *comp = cg()->comp();
   TR::MethodSymbol * methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   bool isJitInduceOSRCall = false;
   if (methodSymbol->isHelper() &&
       methodSymRef->isOSRInductionHelper())
      {
      isJitInduceOSRCall = true;
      }

   if (methodSymRef->isUnresolved() || (comp->compileRelocatableCode() && !comp->getOption(TR_UseSymbolValidationManager)))
      {
      TR_ASSERT(!isJitInduceOSRCall || !comp->compileRelocatableCode(), "calling jitInduceOSR is not supported yet under AOT\n");
      if (methodSymbol->isStatic())
         return TR_S390interpreterUnresolvedStaticGlue;
      else
         return TR_S390interpreterUnresolvedSpecialGlue;
      }
   else if (isJitInduceOSRCall)
      {
      return (TR_RuntimeHelper) methodSymRef->getReferenceNumber();
      }
   else if (methodSymbol->isVMInternalNative() || methodSymbol->isJITInternalNative())
      {
      return TR_S390nativeStaticHelper;
      }
   else
      {
      return TR_S390interpreterStaticSpecialCallGlue;
      }
   }

uint32_t
TR::S390J9CallSnippet::getPICBinaryLength()
   {
   if (self()->getKind() == TR::Snippet::IsUnresolvedCall)
      return 14; /* LARL + LG/LGF + BCR */
   else
      return 6;
   }

uint8_t *
TR::S390J9CallSnippet::generatePICBinary(uint8_t * cursor, TR::SymbolReference* glueRef)
   {
   // Branch to the dispatcher.
   // Since N3 instructions are supported, we can use relative long instructions.
   // i.e.:
   //              BRASL r14, <target Addr>.
   //  - or -
   //              LARL   r14, <target Addr>.   // Unresolved Calls only.
   //              LG/LGF rEP, 0(r14).
   //              BCR    rEP
   uint32_t rEP = (uint32_t) cg()->getEntryPointRegister() - 1;

   if (self()->getKind() == TR::Snippet::IsUnresolvedCall)
      {
      // Generate LARL r14, <Start of Data Const>
      *(int16_t *) cursor = 0xC0E0;
      cursor += sizeof(int16_t);
      intptr_t destAddr = (intptr_t)(cursor + getPICBinaryLength() + self()->getPadBytes() - 2);
      *(int32_t *) cursor = (int32_t)((destAddr - (intptr_t)(cursor - 2)) / 2);
      cursor += sizeof(int32_t);

      *(int32_t *) cursor = 0xe300e000 + (rEP << 20);           // LG/F  rEP, 0(r14)
      cursor += sizeof(int32_t);
      *(int16_t *) cursor = cg()->comp()->target().is64Bit() ? 0x0004 : 0x0014;
      cursor += sizeof(int16_t);

      // BCR   rEP
      *(int16_t *) cursor = 0x07F0 + rEP;
      cursor += sizeof(int16_t);
      }
   else
      {
      // Generate BRASL instruction.
      intptr_t instructionStartAddress = (intptr_t)cursor;
      *(int16_t *) cursor = 0xC0E5;
      cursor += sizeof(int16_t);

      // Calculate the relative offset to get to helper method.
      // If MCC is not supported, everything should be reachable.
      // If MCC is supported, we will look up the appropriate trampoline, if
      //     necessary.
      intptr_t destAddr = (intptr_t)(glueRef->getSymbol()->castToMethodSymbol()->getMethodAddress());

#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
      if (cg()->comp()->getOption(TR_EnableRMODE64))
#endif
         {
         if (cg()->directCallRequiresTrampoline(destAddr, instructionStartAddress))
            {
            // Destination is beyond our reachable jump distance, we'll find the
            // trampoline.
            destAddr = TR::CodeCacheManager::instance()->findHelperTrampoline(glueRef->getReferenceNumber(), (void *)cursor);
            self()->setUsedTrampoline(true);
            }
         }
#endif

      TR_ASSERT_FATAL(cg()->comp()->target().cpu.isTargetWithinBranchRelativeRILRange(destAddr, instructionStartAddress),
                      "Helper Call is not reachable.");
      self()->setSnippetDestAddr(destAddr);

      *(int32_t *) cursor = (int32_t)((destAddr - instructionStartAddress) / 2);
      cg()->addProjectSpecializedRelocation(cursor, (uint8_t*) glueRef, NULL, TR_HelperAddress,
                                      __FILE__, __LINE__, self()->getNode());
      cursor += sizeof(int32_t);
      }
   return cursor;
   }

uint32_t
TR::S390J9CallSnippet::getLength(int32_t  estimatedSnippetStart)
   {
   // *this   swipeable for debugger
   // length = instructionCountForArgsInBytes + (BASR + L(or LG) + BASR +3*sizeof(uintptr_t)) + NOPs
   // number of pad bytes has not been set when this method is called to
   // estimate codebuffer size, so -- i'll put an conservative number here...
   return (instructionCountForArguments(getNode(), cg()) +
      getPICBinaryLength() +
      3 * sizeof(uintptr_t) +
      getRuntimeInstrumentationOnOffInstructionLength(cg()) +
      sizeof(uintptr_t));  // the last item is for padding
   }

void
TR::S390J9CallSnippet::print(TR::FILE *pOutFile, TR_Debug *debug)
   {
   uint8_t * bufferPos = getSnippetLabel()->getCodeLocation();
   TR::Node * callNode = getNode();
   TR::SymbolReference * methodSymRef = getRealMethodSymbolReference();
   if(!methodSymRef)
      methodSymRef = callNode->getSymbolReference();

   TR::MethodSymbol * methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   TR::SymbolReference * glueRef;
   int8_t padbytes = getPadBytes();

   debug->printSnippetLabel(pOutFile, getSnippetLabel(), bufferPos,
      methodSymRef->isUnresolved() ? "Unresolved Call Snippet" : "Call Snippet");

   bufferPos = debug->printS390ArgumentsFlush(pOutFile, callNode, bufferPos, getSizeOfArguments());

   if (methodSymRef->isUnresolved() || (cg()->comp()->compileRelocatableCode() && !cg()->comp()->getOption(TR_UseSymbolValidationManager)))
      {
      if (methodSymbol->isStatic())
         glueRef = cg()->getSymRef(TR_S390interpreterUnresolvedStaticGlue);
      else
         glueRef = cg()->getSymRef(TR_S390interpreterUnresolvedSpecialGlue);
      }
   else if ((methodSymbol->isVMInternalNative() || methodSymbol->isJITInternalNative()))
      {
      glueRef = cg()->getSymRef(TR_S390nativeStaticHelper);
      }
   else
      {
      glueRef = cg()->getSymRef(TR_S390interpreterStaticSpecialCallGlue);
      }

   bufferPos = debug->printRuntimeInstrumentationOnOffInstruction(pOutFile, bufferPos, false); // RIOFF

   if (getKind() == TR::Snippet::IsUnresolvedCall)
      {
      debug->printPrefix(pOutFile, NULL, bufferPos, 6);
      trfprintf(pOutFile, "LARL \tGPR14, *+%d <%p>\t# Start of Data Const.",
                        8 + 6 + padbytes,
                        bufferPos + 8 + 6 + padbytes);
      bufferPos += 6;
      debug->printPrefix(pOutFile, NULL, bufferPos, 6);
      trfprintf(pOutFile, cg()->comp()->target().is64Bit() ? "LG   \tGPR_EP, 0(,GPR14)" : "LGF   \tGPR_EP, 0(,GPR14");
      bufferPos += 6;

      debug->printPrefix(pOutFile, NULL, bufferPos, 2);
      trfprintf(pOutFile, "BCR    \tGPR_EP");
      bufferPos += 2;
      }
   else
      {
      debug->printPrefix(pOutFile, NULL, bufferPos, 6);
      trfprintf(pOutFile, "BRASL \tGPR14, <%p>\t# Branch to Helper Method %s",
                    getSnippetDestAddr(),
                    usedTrampoline() ? "- Trampoline Used.":"");
      bufferPos += 6;
      }

   if (padbytes == 2)
      {
      debug->printPrefix(pOutFile, NULL, bufferPos, 2);
      trfprintf(pOutFile, "DC   \t0x0000 \t\t\t# 2-bytes padding for alignment");
      bufferPos += 2;
      }
   else if (padbytes == 4)
      {
      debug->printPrefix(pOutFile, NULL, bufferPos, 4) ;
      trfprintf(pOutFile, "DC   \t0x00000000 \t\t# 4-bytes padding for alignment");
      bufferPos += 4;
      }
   else if (padbytes == 6)
      {
      debug->printPrefix(pOutFile, NULL, bufferPos, 6) ;
      trfprintf(pOutFile, "DC   \t0x000000000000 \t\t# 6-bytes padding for alignment");
      bufferPos += 6;
      }

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# Method Address", glueRef->getMethodAddress());
   bufferPos += sizeof(intptr_t);


   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# Call Site RA", getCallRA());
   bufferPos += sizeof(intptr_t);

   if (methodSymRef->isUnresolved())
      {
      debug->printPrefix(pOutFile, NULL, bufferPos, 0);
      }
   else
      {
      debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
      }
   trfprintf(pOutFile, "DC   \t%p \t\t# Method Pointer", methodSymRef->isUnresolved() ? 0 : methodSymbol->getMethodAddress());
   }

uint8_t *
TR::S390UnresolvedCallSnippet::emitSnippetBody()
   {
   TR::Compilation *comp = cg()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

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
         if (comp->target().is64Bit())
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
   *(uintptr_t *) cursor = (uintptr_t) methodSymRef->getOwningMethod(comp)->constantPool();

#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
   if (comp->getOption(TR_EnableRMODE64))
#endif
      {
      cg()->addExternalRelocation(
         TR::ExternalRelocation::create(
            cursor,
            *(uint8_t **)cursor,
            getNode() ? (uint8_t *)(intptr_t)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
            TR_Trampolines,
            cg()),
         __FILE__,
         __LINE__,
         getNode());
      }
#if defined(J9ZOS390)
   else
      {
      cg()->addExternalRelocation(
         TR::ExternalRelocation::create(
            cursor,
            *(uint8_t **)cursor,
            getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
            TR_ConstantPool,
            cg()),
         __FILE__,
         __LINE__,
         getNode());
      }
#endif
#else
   cg()->addExternalRelocation(
      TR::ExternalRelocation::create(
         cursor,
         *(uint8_t **)cursor,
         getNode() ? (uint8_t *)(intptr_t)getNode()->getInlinedSiteIndex() : (uint8_t *)(intptr_t)-1,
         TR_ConstantPool,
         cg()),
      __FILE__,
      __LINE__,
      getNode());
#endif

   cursor += sizeof(uintptr_t);

   // Constant Pool Index
   *(uint32_t *) cursor = (helperLookupOffset << 24) | methodSymRef->getCPIndexForVM();

#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
   if (comp->getOption(TR_EnableRMODE64))
#endif
      {
      if ((comp->compileRelocatableCode() || comp->isOutOfProcessCompilation()) && comp->getOption(TR_TraceRelocatableDataDetailsCG))
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
   return TR::S390J9CallSnippet::getLength(estimatedSnippetStart) + sizeof(uintptr_t) + sizeof(int32_t);
   }

uint8_t *
TR::S390VirtualSnippet::emitSnippetBody()
   {
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
   uint8_t * cursor = cg()->getBinaryBufferCursor();
   TR::Node * callNode = getNode();
   TR::Compilation *comp = cg()->comp();
   TR::SymbolReference * glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390virtualUnresolvedHelper);

   getSnippetLabel()->setCodeLocation(cursor);

   // Generate RIOFF if RI is supported.
   cursor = generateRuntimeInstrumentationOnOffInstruction(cg(), cursor, TR::InstOpCode::RIOFF);

   cursor = generatePICBinary(cursor, glueRef);


   // Method address
   *(uintptr_t *) cursor = (uintptr_t) glueRef->getMethodAddress();
   cg()->addExternalRelocation(
      TR::ExternalRelocation::create(
         cursor,
         (uint8_t *)glueRef,
         TR_AbsoluteHelperAddress,
         cg()),
      __FILE__,
      __LINE__,
      callNode);
   cursor += sizeof(uintptr_t);

   // Store the code cache RA
   *(uintptr_t *) cursor = (uintptr_t) getCallRA();
   cg()->addExternalRelocation(
      TR::ExternalRelocation::create(
         cursor,
         NULL,
         TR_AbsoluteMethodAddress,
         cg()),
      __FILE__,
      __LINE__,
      callNode);
   cursor += sizeof(uintptr_t);

   // CP addr
   *(uintptr_t *) cursor = (uintptr_t) callNode->getSymbolReference()->getOwningMethod(comp)->constantPool();

   // J2I relocation information for private nestmate calls
   auto j2iRelocInfo = reinterpret_cast<TR_RelocationRecordInformation*>(comp->trMemory()->allocateMemory(sizeof(TR_RelocationRecordInformation), heapAlloc));
   j2iRelocInfo->data1 = *(uintptr_t *) cursor;                                             // CP address
   j2iRelocInfo->data2 = (uintptr_t)(callNode ? callNode->getInlinedSiteIndex() : -1);      // inlined site index
   uintptr_t cpAddrPosition = (uintptr_t)cursor;                                  // for data3 calculation
   cursor += sizeof(uintptr_t);

   //  save CPIndex as sign extended 8 byte value on 64bit as it's assumed in J9 helpers -- def#63837
   *(uintptr_t *) cursor = (uintptr_t) callNode->getSymbolReference()->getCPIndexForVM();
   cursor += TR::Compiler->om.sizeofReferenceAddress();

   // instruction to be patched
   *(uintptr_t *) cursor = (uintptr_t) (getPatchVftInstruction()->getBinaryEncoding());
   cg()->addExternalRelocation(
      TR::ExternalRelocation::create(
         cursor,
         NULL,
         TR_AbsoluteMethodAddress,
         cg()),
      __FILE__,
      __LINE__,
      callNode);
   cursor += sizeof(uintptr_t);

   // Field used by nestmate private calls
   // J9Method pointer of the callee. Initialized to 0.
   *(uintptr_t *) cursor = 0;
   cursor += sizeof(uintptr_t);

   // Field used by nestmate private calls
   // J2I thunk address
   // No explicit call to `addExternalRelocation` because its relocation info is passed to CP_addr `addExternalRelocation` call.
   *(uintptr_t *) cursor = (uintptr_t) thunkAddress;
   j2iRelocInfo->data3 = (uintptr_t)cursor - cpAddrPosition;    // data3 is the offset of CP_addr to J2I thunk
   cg()->addExternalRelocation(
      TR::ExternalRelocation::create(
         (uint8_t*)cpAddrPosition,
         (uint8_t*)j2iRelocInfo,
         NULL,
         TR_J2IVirtualThunkPointer,
         cg()),
      __FILE__,
      __LINE__,
      callNode);
   cursor += sizeof(uintptr_t);

   // Field used by nestmate private calls
   // For private functions, this is the return address.
   // Add a 2 bytes off set because it's the instruction after the BASR, which is 2-byte long
   // The assumption here is that S390 J9 private linkage indirect dispatch is emitting BASR.
   TR_ASSERT_FATAL(getIndirectCallInstruction() && getIndirectCallInstruction()->getOpCodeValue() == TR::InstOpCode::BASR,
                   "Unexpected branch instruction in VirtualUnresolvedSnippet.\n");

   *(uintptr_t *) cursor = (uintptr_t) (getIndirectCallInstruction()->getBinaryEncoding() + getIndirectCallInstruction()->getBinaryLength());
   cg()->addExternalRelocation(
      TR::ExternalRelocation::create(
         cursor,
         NULL,
         TR_AbsoluteMethodAddress,
         cg()),
      __FILE__,
      __LINE__,
      callNode);

   cursor += sizeof(uintptr_t);

   return cursor;
   }

uint32_t
TR::S390VirtualUnresolvedSnippet::getLength(int32_t  estimatedSnippetStart)
   {
   TR::Compilation* comp = cg()->comp();
   uint32_t length = getPICBinaryLength() + 7 * sizeof(uintptr_t) + TR::Compiler->om.sizeofReferenceAddress();
   length += getRuntimeInstrumentationOnOffInstructionLength(cg());
   return length;
   }

TR::S390InterfaceCallSnippet::S390InterfaceCallSnippet(
      TR::CodeGenerator *cg,
      TR::Node *c,
      TR::LabelSymbol *lab,
      int32_t s,
      int8_t n,
      void *thunkPtr,
      bool useCLFIandBRCL)
   : TR::S390VirtualSnippet(cg, c, lab, s),
      _numInterfaceCallCacheSlots(n),
      _useCLFIandBRCL(useCLFIandBRCL)
   {
   _dataSnippet = new (cg->trHeapMemory()) TR::J9S390InterfaceCallDataSnippet(cg,c,n,thunkPtr);
   cg->addDataConstantSnippet(_dataSnippet);
   }

uint8_t *
TR::S390InterfaceCallSnippet::emitSnippetBody()
   {
   uint8_t * cursor = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(cursor);
   TR::Compilation *comp = cg()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   uint32_t rEP = (uint32_t) cg()->getEntryPointRegister() - 1;
   TR::SymbolReference * glueRef ;

   if (getNumInterfaceCallCacheSlots() == 0)
      {
      glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390interfaceCallHelper);
      }
   else if (comp->getOption(TR_enableInterfaceCallCachingSingleDynamicSlot))
      {
      glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390interfaceCallHelperSingleDynamicSlot);
      }
   else
      {
      glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_S390interfaceCallHelperMultiSlots);
      }

   // Set up the start of data constants and jump to helper.
   //    LARL  r14, <Start of DC>
   //    BRCL  <Helper Addr>

   // LARL - Add Relocation the data constants to this LARL.
   cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, getDataConstantSnippet()->getSnippetLabel()));

   *(int16_t *) cursor = 0xC0E0;
   cursor += sizeof(int16_t);

   // Place holder.  Proper offset will be calculated by relocation.
   *(int32_t *) cursor = 0xDEADBEEF;
   cursor += sizeof(int32_t);

   // BRCL
   *(int16_t *) cursor = 0xC0F4;
   cursor += sizeof(int16_t);

   // Calculate the relative offset to get to helper method.
   // If MCC is not supported, everything should be reachable.
   // If MCC is supported, we will look up the appropriate trampoline, if
   //     necessary.
   intptr_t destAddr = (intptr_t)(glueRef->getMethodAddress());

#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
   if (comp->getOption(TR_EnableRMODE64))
#endif
      {
      if (cg()->directCallRequiresTrampoline(destAddr, reinterpret_cast<intptr_t>(cursor)))
         {
         // Destination is beyond our reachable jump distance, we'll find the
         // trampoline.
         destAddr = TR::CodeCacheManager::instance()->findHelperTrampoline(glueRef->getReferenceNumber(), (void *)cursor);
         this->setUsedTrampoline(true);
         }
      }
#endif

   TR_ASSERT(CHECK_32BIT_TRAMPOLINE_RANGE(destAddr, cursor), "Helper Call is not reachable.");
   this->setSnippetDestAddr(destAddr);

   *(int32_t *) cursor = (int32_t)((destAddr - (intptr_t)(cursor - 2)) / 2);
   cg()->addExternalRelocation(
      TR::ExternalRelocation::create(
         cursor,
         (uint8_t*) glueRef,
         TR_HelperAddress,
         cg()),
      __FILE__,
      __LINE__,
      getNode());
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
                         - sizeof(intptr_t) - sizeof(int32_t)            // 2 DC's at end of this snippet.
                         - (sizeof(intptr_t) - snippet->getPadBytes());    // padding

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

   snippet->print(pOutFile, this);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(uintptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# Address Of Constant Pool", getOwningMethod(methodSymRef)->constantPool());
   bufferPos += sizeof(uintptr_t);

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
   trfprintf(pOutFile, "DC   \t%p\t\t# Method Address", *((uintptr_t *)bufferPos));
   bufferPos += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# Call Site RA", snippet->getCallRA());
   bufferPos += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# Address Of Constant Pool", (intptr_t) getOwningMethod(methodSymRef)->constantPool());
   bufferPos += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t0x%08x \t\t# CP Index", methodSymRef->getCPIndexForVM());
   bufferPos += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# Instruction to be patched with vft offset", snippet->getPatchVftInstruction()->getBinaryEncoding());
   bufferPos += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# Private J9Method pointer", 0);
   bufferPos += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# J2I thunk address for private", snippet->getJ2IThunkAddress());
   bufferPos += sizeof(intptr_t);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# RA for private", snippet->getBranchInstruction()->getBinaryEncoding() + snippet->getIndirectCallInstruction()->getBinaryLength());
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
                                (intptr_t) snippet->getDataConstantSnippet()->getSnippetLabel()->getCodeLocation());
   bufferPos += 6;

   printPrefix(pOutFile, NULL, bufferPos, 6);
   trfprintf(pOutFile, "BRCL \t<%p>\t\t# Branch to Helper %s",
                   snippet->getSnippetDestAddr(),
                   snippet->usedTrampoline()?"- Trampoline Used.":"");
   bufferPos += 6;
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::J9S390InterfaceCallDataSnippet * snippet)
   {
   uint8_t * bufferPos = snippet->getSnippetLabel()->getCodeLocation();

   // This follows the snippet format in TR::J9S390InterfaceCallDataSnippet::emitSnippetBody
   uint8_t refSize = TR::Compiler->om.sizeofReferenceAddress();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "Interface call cache data snippet");
   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t0x%016lx  # Call site RA", *(intptr_t*)bufferPos);
   bufferPos += refSize;

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t0x%016lx  # Address of constant pool", *(intptr_t*)bufferPos);
   bufferPos += refSize;

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t0x%016lx  # CP index", *(intptr_t*)bufferPos);
   bufferPos += refSize;

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t0x%016lx  # Interface class", *(intptr_t*)bufferPos);
   bufferPos += refSize;

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t0x%016lx  # Method index", *(intptr_t*)bufferPos);
   bufferPos += refSize;

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t0x%016lx  # J2I thunk address ", *(intptr_t*)bufferPos);
   bufferPos += refSize;

   // zero cache slot
   if (snippet->getNumInterfaceCallCacheSlots() == 0)
      {
      printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
      trfprintf(pOutFile, "DC   \t0x%016lx  # flags", *(intptr_t*)bufferPos);
      bufferPos += refSize;
      }

   // non-single cache slots
   bool isSingleDynamicSlot = comp()->getOption(TR_enableInterfaceCallCachingSingleDynamicSlot);
   if (!isSingleDynamicSlot)
      {
      // flags
      printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
      trfprintf(pOutFile, "DC   \t0x%016lx  # flags", *(intptr_t*)bufferPos);
      bufferPos += refSize;

      // lastCachedSlot
      printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
      trfprintf(pOutFile, "DC   \t0x%016lx  # last cached slot", *(intptr_t*)bufferPos);
      bufferPos += refSize;

      // firstSlot
      printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
      trfprintf(pOutFile, "DC   \t0x%016lx  # first slot", *(intptr_t*)bufferPos);
      bufferPos += refSize;

      // lastSlot
      printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
      trfprintf(pOutFile, "DC   \t0x%016lx  # last slot", *(intptr_t*)bufferPos);
      bufferPos += refSize;
      }

   // print profiled class list
   int32_t numInterfaceCallCacheSlots = snippet->getNumInterfaceCallCacheSlots();
   bool isUseCLFIandBRCL = snippet->isUseCLFIandBRCL();
   TR::list<TR_OpaqueClassBlock*> * profiledClassesList = comp()->cg()->getPICsListForInterfaceSnippet(snippet);

   if (profiledClassesList)
      {
      for (auto iter = profiledClassesList->begin(); iter != profiledClassesList->end(); ++iter)
         {
         numInterfaceCallCacheSlots--;
         printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
         trfprintf(pOutFile, "DC   \t0x%016lx  # profiled class", *(intptr_t*)bufferPos);
         bufferPos += refSize;

         printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
         trfprintf(pOutFile, "DC   \t0x%016lx  # profiled method", *(intptr_t*)bufferPos);
         bufferPos += refSize;
         }
      }

   // print remaining class list
   for (uint32_t i = 0; i < numInterfaceCallCacheSlots; i++)
      {
      if (isUseCLFIandBRCL)
         {
         // address of CLFI's immediate field
         printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
         trfprintf(pOutFile, "DC   \t0x%016lx  # address of CLFI's immediate field", *(intptr_t*)bufferPos);
         bufferPos += refSize;
         }
      else
         {
         // class pointer
         printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
         trfprintf(pOutFile, "DC   \t0x%016lx  # class pointer %d", *(intptr_t*)bufferPos, i);
         bufferPos += refSize;

         // method pointer
         printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
         trfprintf(pOutFile, "DC   \t0x%016lx  # method pointer %d", *(intptr_t*)bufferPos, i);
         bufferPos += refSize;
         }
      }

   if (isSingleDynamicSlot)
      {
      // flags
      printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
      trfprintf(pOutFile, "DC   \t0x%016lx # method pointer", *(intptr_t*)bufferPos);
      bufferPos += refSize;
      }
   return;
   }

///////////////////////////////////////////////////////////////////////////////
// TR::J9S390InterfaceCallDataSnippet member functions
///////////////////////////////////////////////////////////////////////////////
TR::J9S390InterfaceCallDataSnippet::J9S390InterfaceCallDataSnippet(TR::CodeGenerator * cg,
                                                                 TR::Node * node,
                                                                 uint8_t n,
                                                                 void *thunkPtr,
                                                                 bool useCLFIandBRCL)
   : TR::S390ConstantDataSnippet(cg,node,generateLabelSymbol(cg),0),
     _numInterfaceCallCacheSlots(n),
     _codeRA(NULL),
     _thunkAddress(thunkPtr),
     _useCLFIandBRCL(useCLFIandBRCL)
   {
   }

/**
 * \brief
 * Emits interface call data snippet, which can take three layouts: 0-slot, single-dynamic-slot, and 4-slot layouts.
 * The default layout has 4-slots.
*/
uint8_t *
TR::J9S390InterfaceCallDataSnippet::emitSnippetBody()
   {

/*
 * 4-slot Layout (example showing 64-bit fields). 4-slot is the default case.
 *
 * Note: some fields and PIC slots will be unused if interface call turns out to be a direct dispatch.
 *
 * Snippet Label L0049:          ; Interface call cache data snippet
 *   DC          0x0000000084e00244  # Call site RA
 *   DC          0x0000000000202990  # Address of constant pool
 *   DC          0x0000000000000008  # CP index
 *   DC          0x0000000000000000  # Interface class   (vm helper will fill it up with call resolution class info)
 *   DC          0x0000000000000000  # Method index      (vm helper will fill it up with call resolution method info. This contains
 *                                                           J9method if it's direct dispatch. It's I-table offset if it's a normal interface call)
 *   DC          0x0000000084ffd408  # J2I thunk         (J2I thunk address)
 *   DC          0x0000000000000000  # flags             (this 64-bit number is initially 0, meaning the call is unresolved. It'll be 1 once resolved)
 *   DC          0x0000000084e00408  # last cached slot
 *   DC          0x0000000084e00418  # first slot        (address of class pointer 0)
 *   DC          0x0000000084e00448  # last slot         (address of class pointer 3)
 *   DC          0x0000000000000000  # class pointer 0   (8 or 16 byte aligned PIC slots. all PICs starting from this point are
 *                                                        un-used if the call is resolved as private, which is direct dispatch)
 *   DC          0x0000000000000000  # method pointer 0
 *   DC          0x0000000000000000  # class pointer 1
 *   DC          0x0000000000000000  # method pointer 1
 *   DC          0x0000000000000000  # class pointer 2
 *   DC          0x0000000000000000  # method pointer 2
 *   DC          0x0000000000000000  # class pointer 3
 *   DC          0x0000000000000000  # method pointer 3
 *
 * If we use CLFI / BRCL
 *  DC  0x0000000000000000       ; Patch slot of the first CLFI (Class 1)
 *  DC  0x0000000000000000       ; Patch slot of the second CLFI (Class 2)
 *  DC  0x0000000000000000       ; Patch slot of the third CLFI (Class 3)
 * and so on ...
 *
 *
 * Single dynamic slot layout
 *
 *  Snippet Label L0049:          ; Interface call cache data snippet
 *    DC          0x00000000529941ca  # Call site RA
 *    DC          0x0000000000202990  # Address of constant pool
 *    DC          0x0000000000000008  # CP index
 *    DC          0x0000000000000000  # Interface class
 *    DC          0x0000000000000000  # Method index
 *    DC          0x0000000052b91408  # J2I thunk
 *    DC          0x0000000000000000  # class pointer 0
 *    DC          0x0000000000000000  # method pointer 0
 *    DC          0x0000000000000000 # flags
 *
 *
 *     0-slot example
 *
 * Snippet Label L0041:          ; Interface call cache data snippet
 *   DC          0x0000000065a7f156  # Call site RA
 *   DC          0x0000000000202990  # Address of constant pool
 *   DC          0x000000000000000e  # CP index
 *   DC          0x0000000000000000  # Interface class
 *   DC          0x0000000000000000  # Method index
 *   DC          0x0000000065c7c408  # Interpreter thunk
 *   DC          0x0000000000000000  # flags
*/
   TR::Compilation *comp = cg()->comp();
   int32_t i = 0;
   uint8_t * cursor = cg()->getBinaryBufferCursor();

   // Class Pointer must be double word aligned.
   // For 64-bit single dynamic slot, LPQ and STPQ instructions will be emitted; and they require quadword alignment
   int32_t alignment = comp->getOption(TR_enableInterfaceCallCachingSingleDynamicSlot) ? 16 : 8;
   intptr_t startOfPIC = (intptr_t)cursor + 6 * sizeof(intptr_t);
   int32_t padSize = 0;

   if ((startOfPIC % alignment) != 0)
      {
      padSize = (alignment - (startOfPIC % alignment));
      }

   cursor += padSize;

   getSnippetLabel()->setCodeLocation(cursor);
   TR::Node * callNode = getNode();


   intptr_t snippetStart = (intptr_t)cursor;

   // code cache RA
   TR_ASSERT_FATAL(_codeRA != NULL, "Interface Call Data Constant's code return address not initialized.\n");
   *(uintptr_t *) cursor = (uintptr_t)_codeRA;
   cg()->addExternalRelocation(
      TR::ExternalRelocation::create(
         cursor,
         NULL,
         TR_AbsoluteMethodAddress,
         cg()),
      __FILE__,
      __LINE__,
      callNode);

   cursor += TR::Compiler->om.sizeofReferenceAddress();

   // constant pool
   *(uintptr_t *) cursor = (uintptr_t) callNode->getSymbolReference()->getOwningMethod(comp)->constantPool();

   // J2I relocation information for private nestmate calls
   auto j2iRelocInfo = reinterpret_cast<TR_RelocationRecordInformation*>(comp->trMemory()->allocateMemory(sizeof(TR_RelocationRecordInformation), heapAlloc));
   j2iRelocInfo->data1 = *(uintptr_t *) cursor;                                             // CP address
   j2iRelocInfo->data2 = (uintptr_t)(callNode ? callNode->getInlinedSiteIndex() : -1);      // inlined site index
   uintptr_t cpAddrPosition = (uintptr_t)cursor;                                  // for data3 calculation

   cursor += TR::Compiler->om.sizeofReferenceAddress();

   //  save CPIndex as sign extended 8 byte value on 64bit as it's assumed in J9 helpers
   // cp index
   *(intptr_t *) cursor = (intptr_t) callNode->getSymbolReference()->getCPIndex();
   cursor += TR::Compiler->om.sizeofReferenceAddress();

   //interface class
   *(uintptr_t *) cursor = 0;
   cursor += TR::Compiler->om.sizeofReferenceAddress();

   // method index
   *(intptr_t *) cursor = 0;
   cursor += TR::Compiler->om.sizeofReferenceAddress();

   // J2I thunk address.
   // Note that J2I thunk relocation is associated with CP addr and the call to `addExternalRelocation` uses CP-Addr cursor.
   *(intptr_t *) cursor = (intptr_t)_thunkAddress;
   j2iRelocInfo->data3 = (uintptr_t)cursor - cpAddrPosition;  // data3 is the offset of CP_addr to J2I thunk

   cg()->addExternalRelocation(
      TR::ExternalRelocation::create(
         (uint8_t*)cpAddrPosition,
         (uint8_t*)j2iRelocInfo,
         NULL,
         TR_J2IVirtualThunkPointer,
         cg()),
      __FILE__,
      __LINE__,
      callNode);

   cursor += TR::Compiler->om.sizeofReferenceAddress();

   if (getNumInterfaceCallCacheSlots() == 0)
      {
      // Flags
      *(intptr_t *) (cursor) = 0;
      cursor += TR::Compiler->om.sizeofReferenceAddress();
      return cursor;
      }
   uint8_t * cursorlastCachedSlot = 0;
   bool isSingleDynamic = comp->getOption(TR_enableInterfaceCallCachingSingleDynamicSlot);

   if (!isSingleDynamic)
      {
      // Flags
      *(intptr_t *) (cursor) = 0;
      cursor += TR::Compiler->om.sizeofReferenceAddress();

      // lastCachedSlot
      cursorlastCachedSlot = cursor;
      *(intptr_t *) (cursor) =  snippetStart + getFirstSlotOffset() - (2 * TR::Compiler->om.sizeofReferenceAddress());
      cg()->addExternalRelocation(
         TR::ExternalRelocation::create(
            cursor,
            NULL,
            TR_AbsoluteMethodAddress,
            cg()),
         __FILE__,
         __LINE__,
         callNode);

      cursor += TR::Compiler->om.sizeofReferenceAddress();

      // firstSlot
      *(intptr_t *) (cursor) = snippetStart + getFirstSlotOffset();
      cg()->addExternalRelocation(
         TR::ExternalRelocation::create(
            cursor,
            NULL,
            TR_AbsoluteMethodAddress,
            cg()),
         __FILE__,
         __LINE__,
         callNode);
      cursor += TR::Compiler->om.sizeofReferenceAddress();

      // lastSlot
      *(intptr_t *) (cursor) =  snippetStart + getLastSlotOffset();
      cg()->addExternalRelocation(
         TR::ExternalRelocation::create(
            cursor,
            NULL,
            TR_AbsoluteMethodAddress,
            cg()),
         __FILE__,
         __LINE__,
         callNode);
      cursor += TR::Compiler->om.sizeofReferenceAddress();
      }

    // Cursor must be double word aligned by this point
    // so that 64-bit single dynamic slot can use LPQ to concurrently load a quadword.
    TR_ASSERT_FATAL( (!isSingleDynamic && (intptr_t)cursor % 8 == 0)
                     || (comp->target().is64Bit() && isSingleDynamic && (intptr_t)cursor % 16 == 0),
                     "Interface Call Data Snippet Class Ptr is not double word aligned.");

    bool updateField = false;
     int32_t numInterfaceCallCacheSlots = getNumInterfaceCallCacheSlots();
     TR::list<TR_OpaqueClassBlock*> * profiledClassesList = cg()->getPICsListForInterfaceSnippet(this);
     if (profiledClassesList)
        {
        for (auto valuesIt = profiledClassesList->begin(); valuesIt != profiledClassesList->end(); ++valuesIt)
           {
           TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
           TR_ResolvedMethod * profiledMethod = methodSymRef->getOwningMethod(comp)->getResolvedInterfaceMethod(comp,
                 (TR_OpaqueClassBlock *)(*valuesIt), methodSymRef->getCPIndex());
           numInterfaceCallCacheSlots--;
           updateField = true;
#if defined(TR_TARGET_64BIT)
           if (comp->target().is64Bit() && TR::Compiler->om.generateCompressedObjectHeaders())
              *(uintptr_t *) cursor = (uintptr_t) (*valuesIt) << 32;
           else
#endif /* defined(TR_TARGET_64BIT) */
              *(uintptr_t *) cursor = (uintptr_t) (*valuesIt);

           if (comp->getOption(TR_EnableHCR))
              {
              cg()->jitAddPicToPatchOnClassRedefinition(*valuesIt, (void *) cursor);
              }

           if (cg()->fe()->isUnloadAssumptionRequired((TR_OpaqueClassBlock *)(*valuesIt), comp->getCurrentMethod()))
              {
              cg()->jitAddPicToPatchOnClassUnload(*valuesIt, (void *) cursor);
              }

           cursor += TR::Compiler->om.sizeofReferenceAddress();

           // Method Pointer
           *(uintptr_t *) (cursor) = (uintptr_t)profiledMethod->startAddressForJittedMethod();
           cursor += TR::Compiler->om.sizeofReferenceAddress();
           }

        }

     // Skip the top cache slots that are filled with IProfiler data by setting the cursorlastCachedSlot to point to the fist dynamic cache slot
     if (updateField)
        {
        *(intptr_t *) (cursorlastCachedSlot) =  snippetStart + getFirstSlotOffset() + ((getNumInterfaceCallCacheSlots()-numInterfaceCallCacheSlots-1)*2 * TR::Compiler->om.sizeofReferenceAddress());
        }

     for (i = 0; i < numInterfaceCallCacheSlots; i++)
      {
      if (isUseCLFIandBRCL())
         {
         // Get Address of CLFI's immediate field (2 bytes into CLFI instruction).
         // jitAddPicToPatchOnClassUnload is called by PicBuilder code when the cache is populated.
         *(intptr_t*) cursor = (intptr_t)(getFirstCLFI()->getBinaryEncoding() ) + (intptr_t)(i * 12 + 2);
         cursor += TR::Compiler->om.sizeofReferenceAddress();
         }
      else
         {
         // Class Pointer - jitAddPicToPatchOnClassUnload is called by PicBuilder code when the cache is populated.
         *(intptr_t *) cursor = 0;
         cursor += TR::Compiler->om.sizeofReferenceAddress();

         // Method Pointer
         *(intptr_t *) (cursor) = 0;
         cursor += TR::Compiler->om.sizeofReferenceAddress();
         }
      }

   if (isSingleDynamic)
      {
      // Flags
      *(intptr_t *) (cursor) = 0;
      cursor += TR::Compiler->om.sizeofReferenceAddress();
      }

   return cursor;
   }


uint32_t
TR::J9S390InterfaceCallDataSnippet::getCallReturnAddressOffset()
   {
   return 0;
   }

uint32_t
TR::J9S390InterfaceCallDataSnippet::getSingleDynamicSlotOffset()
   {
   return getCallReturnAddressOffset() + 6 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::J9S390InterfaceCallDataSnippet::getLastCachedSlotFieldOffset()
   {
   return getCallReturnAddressOffset() + 7 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::J9S390InterfaceCallDataSnippet::getFirstSlotFieldOffset()
   {
   return getLastCachedSlotFieldOffset() + TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::J9S390InterfaceCallDataSnippet::getLastSlotFieldOffset()
   {
   return getFirstSlotFieldOffset() + TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::J9S390InterfaceCallDataSnippet::getFirstSlotOffset()
   {
   return getLastSlotFieldOffset() + TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::J9S390InterfaceCallDataSnippet::getLastSlotOffset()
   {
   return (getFirstSlotOffset() +
          (getNumInterfaceCallCacheSlots() - 1) *
             ((isUseCLFIandBRCL()?1:2) * TR::Compiler->om.sizeofReferenceAddress()));
   }

uint32_t
TR::J9S390InterfaceCallDataSnippet::getLength(int32_t)
   {
   TR::Compilation *comp = cg()->comp();
   // the 1st item is for padding...
   if (getNumInterfaceCallCacheSlots()== 0)
      return 3 * TR::Compiler->om.sizeofReferenceAddress() + 8 * TR::Compiler->om.sizeofReferenceAddress();

   if (comp->getOption(TR_enableInterfaceCallCachingSingleDynamicSlot))
      return 3 * TR::Compiler->om.sizeofReferenceAddress() + getSingleDynamicSlotOffset() + 4 * TR::Compiler->om.sizeofReferenceAddress();

   if (getNumInterfaceCallCacheSlots() > 0)
      return 3 * TR::Compiler->om.sizeofReferenceAddress() + getLastSlotOffset() + TR::Compiler->om.sizeofReferenceAddress();

   TR_ASSERT(0,"Interface Call Data Snippet has 0 size.");
   return 0;
   }


uint32_t
TR::S390JNICallDataSnippet::getJNICallOutFrameFlagsOffset()
   {
   return TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getReturnFromJNICallOffset()
   {
   return 2 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getSavedPCOffset()
   {
   return 3 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getTagBitsOffset()
   {
   return 4 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getPCOffset()
   {
   return 5 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getLiteralsOffset()
   {
   return 6 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getJitStackFrameFlagsOffset()
   {
   return 7 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getConstReleaseVMAccessMaskOffset()
   {
   return 8 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getConstReleaseVMAccessOutOfLineMaskOffset()
   {
   return 9 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getTargetAddressOffset()
   {
   return 10 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return 12 * TR::Compiler->om.sizeofReferenceAddress(); /*one ptr more for possible padding */
   }


TR::S390JNICallDataSnippet::S390JNICallDataSnippet(TR::CodeGenerator * cg,
                               TR::Node * node)
: TR::S390ConstantDataSnippet(cg, node, generateLabelSymbol(cg),0),
 _baseRegister(0),
 _ramMethod(0),
 _JNICallOutFrameFlags(0),
 _returnFromJNICallLabel(0),
 _savedPC(0),
 _tagBits(0),
 _pc(0),
 _literals(0),
 _jitStackFrameFlags(0),
 _constReleaseVMAccessMask(0),
 _constReleaseVMAccessOutOfLineMask(0),
 _targetAddress(0)
   {
   return;
   }

uint8_t *
TR::S390JNICallDataSnippet::emitSnippetBody()
   {
   uint8_t * cursor = cg()->getBinaryBufferCursor();

   /* TR::S390JNICallDataSnippet Layout: all fields are pointer sized
       ramMethod
       JNICallOutFrameFlags
       returnFromJNICall
       savedPC
       tagBits
       pc
       literals
       jitStackFrameFlags
       constReleaseVMAccessMask
       constReleaseVMAccessOutOfLineMask
       targetAddress
   */
      TR::Compilation *comp = cg()->comp();

      // Ensure pointer sized alignment
      int32_t alignSize = TR::Compiler->om.sizeofReferenceAddress();
      int32_t padBytes = ((intptr_t)cursor + alignSize -1) / alignSize * alignSize - (intptr_t)cursor;
      cursor += padBytes;

      getSnippetLabel()->setCodeLocation(cursor);
      TR::Node * callNode = getNode();

      intptr_t snippetStart = (intptr_t)cursor;

      //  JNI Callout Frame data
      // _ramMethod
      *(intptr_t *) cursor = (intptr_t) _ramMethod;

      TR_ExternalRelocationTargetKind reloType;
      if (getNode()->getSymbol()->castToResolvedMethodSymbol()->isSpecial())
         reloType = TR_SpecialRamMethodConst;
      else if (getNode()->getSymbol()->castToResolvedMethodSymbol()->isStatic())
         reloType = TR_StaticRamMethodConst;
      else if (getNode()->getSymbol()->castToResolvedMethodSymbol()->isVirtual())
         reloType = TR_VirtualRamMethodConst;
      else
         {
         reloType = TR_NoRelocation;
         TR_ASSERT(0,"JNI relocation not supported.");
         }

      if (cg()->needClassAndMethodPointerRelocations())
         {
         cg()->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               (uint8_t *) callNode->getSymbolReference(),
               callNode  ? (uint8_t *)(intptr_t)callNode->getInlinedSiteIndex() : (uint8_t *)-1,
               reloType,
               cg()),
            __FILE__,
            __LINE__,
            callNode);
         }

      cursor += TR::Compiler->om.sizeofReferenceAddress();

      // _JNICallOutFrameFlags
       *(intptr_t *) cursor = (intptr_t) _JNICallOutFrameFlags;
       cursor += TR::Compiler->om.sizeofReferenceAddress();
       // _returnFromJNICall
       *(intptr_t *) cursor = (intptr_t) (_returnFromJNICallLabel->getCodeLocation());

       cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelAbsoluteRelocation(cursor, _returnFromJNICallLabel));
       cg()->addExternalRelocation(
          TR::ExternalRelocation::create(
             cursor,
             NULL,
             TR_AbsoluteMethodAddress,
             cg()),
          __FILE__,
          __LINE__,
          getNode());

       cursor += TR::Compiler->om.sizeofReferenceAddress();
       // _savedPC
       *(intptr_t *) cursor = (intptr_t) _savedPC;
       cursor += TR::Compiler->om.sizeofReferenceAddress();
       // _tagBits
       *(intptr_t *) cursor = (intptr_t) _tagBits;
       cursor += TR::Compiler->om.sizeofReferenceAddress();

       //VMThread data
       // _pc
       *(intptr_t *) cursor = (intptr_t) _pc;
       cursor += TR::Compiler->om.sizeofReferenceAddress();
       // _literals
       *(intptr_t *) cursor = (intptr_t) _literals;
       cursor += TR::Compiler->om.sizeofReferenceAddress();
       // _jitStackFrameFlags
       *(intptr_t *) cursor = (intptr_t) _jitStackFrameFlags;
       cursor += TR::Compiler->om.sizeofReferenceAddress();

       // _constReleaseVMAccessMask
      *(intptr_t *) cursor = (intptr_t) _constReleaseVMAccessMask;
      cursor += TR::Compiler->om.sizeofReferenceAddress();
      // _constReleaseVMAccessOutOfLineMask
      *(intptr_t *) cursor = (intptr_t) _constReleaseVMAccessOutOfLineMask;
      cursor += TR::Compiler->om.sizeofReferenceAddress();

      // _targetAddress/function pointer of native method
      *(intptr_t *) cursor = (intptr_t) _targetAddress;
      TR_OpaqueMethodBlock *method = getNode()->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod()->getPersistentIdentifier();

#ifdef J9VM_OPT_JITSERVER
      if (comp->isOutOfProcessCompilation())
         {
         // For JITServer we need to build a list of assumptions that will be sent to client at end of compilation
         intptr_t offset = cursor - cg()->getBinaryBufferStart();
         SerializedRuntimeAssumption* sar = new (cg()->trHeapMemory()) SerializedRuntimeAssumption(RuntimeAssumptionOnRegisterNative, (uintptr_t)method, offset);
         comp->getSerializedRuntimeAssumptions().push_front(sar);
         }
      else
#endif // J9VM_OPT_JITSERVER
         {
         TR_PatchJNICallSite::make(cg()->fe(), cg()->trPersistentMemory(), (uintptr_t) method, cursor, comp->getMetadataAssumptionList());
         }

      if (getNode()->getSymbol()->castToResolvedMethodSymbol()->isSpecial())
         reloType = TR_JNISpecialTargetAddress;
      else if (getNode()->getSymbol()->castToResolvedMethodSymbol()->isStatic())
         reloType = TR_JNIStaticTargetAddress;
      else if (getNode()->getSymbol()->castToResolvedMethodSymbol()->isVirtual())
         reloType = TR_JNIVirtualTargetAddress;
      else
         {
         reloType = TR_NoRelocation;
         TR_ASSERT(0,"JNI relocation not supported.");
         }

      if (cg()->needClassAndMethodPointerRelocations())
         {
         TR_RelocationRecordInformation *info = new (comp->trHeapMemory()) TR_RelocationRecordInformation();
         info->data1 = 0;
         info->data2 = reinterpret_cast<uintptr_t>(callNode->getSymbolReference());
         int16_t inlinedSiteIndex = callNode ? callNode->getInlinedSiteIndex() : -1;
         info->data3 = static_cast<uintptr_t>(inlinedSiteIndex);

         cg()->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               reinterpret_cast<uint8_t *>(info),
               reloType,
               cg()),
            __FILE__,
            __LINE__,
            callNode);
         }

      cursor += TR::Compiler->om.sizeofReferenceAddress();

   return cursor;
   }

void
TR::S390JNICallDataSnippet::print(TR::FILE *pOutFile, TR_Debug *debug)
   {
/*
       ramMethod
       JNICallOutFrameFlags
       returnFromJNICall
       savedPC
       tagBits
       pc
       literals
       jitStackFrameFlags
       constReleaseVMAccessMask
       constReleaseVMAccessOutOfLineMask
       targetAddress

*/
   TR_FrontEnd *fe = cg()->comp()->fe();
   uint8_t * bufferPos = getSnippetLabel()->getCodeLocation();

   int i = 0;

   debug->printSnippetLabel(pOutFile, getSnippetLabel(), bufferPos, "JNI Call Data Snippet");

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# ramMethod",*((intptr_t*)bufferPos));
   bufferPos += sizeof(intptr_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# JNICallOutFrameFlags",*((intptr_t*)bufferPos));
   bufferPos += sizeof(intptr_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# returnFromJNICall", *((intptr_t*)bufferPos));
   bufferPos += sizeof(intptr_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# savedPC", *((intptr_t*)bufferPos));
   bufferPos += sizeof(intptr_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# tagBits", *((intptr_t*)bufferPos));
   bufferPos += sizeof(intptr_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# pc", *((intptr_t*)bufferPos));
   bufferPos += sizeof(intptr_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# literals", *((intptr_t*)bufferPos));
   bufferPos += sizeof(intptr_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# jitStackFrameFlags", *((intptr_t*)bufferPos));
   bufferPos += sizeof(intptr_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# constReleaseVMAccessMask",*((intptr_t*)bufferPos));
   bufferPos += sizeof(intptr_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# constReleaseVMAccessOutOfLineMask",*((intptr_t*)bufferPos));
   bufferPos += sizeof(intptr_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptr_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# targetAddress",*((intptr_t*)bufferPos));
   bufferPos += sizeof(intptr_t);
}
