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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "x/codegen/CallSnippet.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/SnippetGCMap.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "x/codegen/X86PrivateLinkage.hpp"

uint8_t *TR::X86PicDataSnippet::encodeConstantPoolInfo(uint8_t *cursor)
   {
   intptrj_t cpAddr = (intptrj_t)_methodSymRef->getOwningMethod(cg()->comp())->constantPool();
   *(intptrj_t *)cursor = cpAddr;

   if (_thunkAddress)
      {
      TR_ASSERT(TR::Compiler->target.is64Bit(), "expecting a 64-bit target for thunk relocations");
      cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                             *(uint8_t **)cursor,
                                                                             _startOfPicInstruction->getNode() ? (uint8_t *)_startOfPicInstruction->getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                             TR_Thunks, cg()),
                             __FILE__,
                             __LINE__,
                             _startOfPicInstruction->getNode());
      }
   else
      {
      cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                                  (uint8_t *)cpAddr,
                                                                                   _startOfPicInstruction->getNode() ? (uint8_t *)_startOfPicInstruction->getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                                   TR_ConstantPool,
                                                                                   cg()),
                             __FILE__,
                             __LINE__,
                             _startOfPicInstruction->getNode());
      }

   // DD/DQ cpIndex
   //
   cursor += sizeof(intptrj_t);
   *(uintptrj_t *)cursor = (uintptrj_t)_methodSymRef->getCPIndexForVM();
   cursor += sizeof(intptrj_t);

   return cursor;
   }


uint8_t *TR::X86PicDataSnippet::emitSnippetBody()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   uint8_t *startOfSnippet = cg()->getBinaryBufferCursor();

   uint8_t *cursor = startOfSnippet;

   TR::X86PrivateLinkage *x86Linkage = toX86PrivateLinkage(cg()->getLinkage());

   int32_t disp32;

   TR_RuntimeHelper resolveSlotHelper, populateSlotHelper;
   int32_t sizeofPicSlot;

   if (isInterface())
      {
      // IPIC
      //
      // Slow interface lookup dispatch.
      //
      getSnippetLabel()->setCodeLocation(startOfSnippet);

      // Slow path lookup dispatch
      //
      _dispatchSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_X86IPicLookupDispatch, false, false, false);

      *cursor++ = 0xe8;  // CALL
      disp32 = cg()->branchDisplacementToHelperOrTrampoline(cursor+4, _dispatchSymRef);
      *(int32_t *)cursor = disp32;

      cg()->addAOTRelocation(new (cg()->trHeapMemory())
         TR::ExternalRelocation(cursor,
                                    (uint8_t *)_dispatchSymRef,
                                    TR_HelperAddress,
                                    cg()), __FILE__, __LINE__, _startOfPicInstruction->getNode());
      cursor += 4;

      // Lookup dispatch needs its stack map here.
      //
      gcMap().registerStackMap(cursor, cg());

      // Restart jump (always long for predictable size).
      //
      disp32 = _doneLabel->getCodeLocation() - (cursor + 5);
      *cursor++ = 0xe9;
      *(int32_t *)cursor = disp32;
      cursor += 4;

      // DD/DQ constantPool address
      // DD/DQ cpIndex
      //
      if (_methodSymRef->isUnresolved())
         {
         cursor = encodeConstantPoolInfo(cursor);
         }
      else
         {
         TR_ASSERT(0, "Can't handle resolved IPICs here yet!");
         }

      // Reserve space for resolved interface class and itable index.
      // These slots will be populated during interface class resolution.
      //
      // DD/DQ  0x00000000
      // DD/DQ  0x00000000
      //
      cursor += (2 * sizeof(intptrj_t));

      if (TR::Compiler->target.is64Bit())
         {
         // REX+MOV of MOVRegImm64 instruction
         //
         uint16_t *slotPatchInstructionBytes = (uint16_t *)_slotPatchInstruction->getBinaryEncoding();
         *(uint16_t *)cursor = *slotPatchInstructionBytes;
         cursor += 2;
         }
      else
         {
         // ModRM byte of CMPMemImm4 instruction
         //
         uint8_t *slotPatchInstructionBytes = _slotPatchInstruction->getBinaryEncoding();
         *cursor = *(slotPatchInstructionBytes+1);
         cursor++;
         }

      resolveSlotHelper = TR_X86resolveIPicClass;
      populateSlotHelper = TR_X86populateIPicSlotClass;
      sizeofPicSlot = x86Linkage->IPicParameters.roundedSizeOfSlot;
      }
   else
      {
      // VPIC
      //
      // Slow path dispatch through vtable
      //

      uint8_t callModRMByte = 0;

      // DD/DQ constantPool address
      // DD/DQ cpIndex
      //
      if (_methodSymRef->isUnresolved() || fej9->forceUnresolvedDispatch())
         {
         // Align the real snippet entry point because it will be patched with
         // the vtable dispatch when the method is resolved.
         //
         intptrj_t entryPoint = ((intptrj_t)cursor +
                                 ((2 * sizeof(intptrj_t)) +
                                  (TR::Compiler->target.is64Bit() ? 4 : 1)));

         intptrj_t requiredEntryPoint =
            (entryPoint + (cg()->getLowestCommonCodePatchingAlignmentBoundary()-1) &
            (intptrj_t)(~(cg()->getLowestCommonCodePatchingAlignmentBoundary()-1)));

         cursor += (requiredEntryPoint - entryPoint);

         // DD/DQ cpAddr
         // DD/DQ cpIndex
         //
         cursor = encodeConstantPoolInfo(cursor);

         if (TR::Compiler->target.is64Bit())
            {
            // REX prefix of MOVRegImm64 instruction
            //
            uint8_t *slotPatchInstructionBytes = (uint8_t *)_slotPatchInstruction->getBinaryEncoding();
            *cursor++ = *slotPatchInstructionBytes++;

            // MOV op of MOVRegImm64 instruction
            //
            *cursor++ = *slotPatchInstructionBytes;

            // REX prefix for the CALLMem instruction.
            //
            *cursor++ = *(slotPatchInstructionBytes+9);

            // Convert the CMP ModRM byte into the ModRM byte for the CALLMem instruction.
            //
            slotPatchInstructionBytes += 11;
            callModRMByte = (*slotPatchInstructionBytes & 7) + 0x90;
            *cursor++ = callModRMByte;

            // SIB  49ff9424feffffff call    qword ptr [r12-2]

            }
         else
            {
            // CMP ModRM byte
            //
            uint8_t *slotPatchInstructionBytes = (uint8_t *)_slotPatchInstruction->getBinaryEncoding();
            *cursor++ = *(slotPatchInstructionBytes+1);
            }
         }
      else
         {
         TR_ASSERT(0, "Can't handle resolved VPICs here yet!");
         }

      _dispatchSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_X86populateVPicVTableDispatch, false, false, false);

      getSnippetLabel()->setCodeLocation(cursor);

      if (!isInterface() && _methodSymRef->isUnresolved())
         {
         TR_ASSERT((((intptrj_t)cursor & (cg()->getLowestCommonCodePatchingAlignmentBoundary()-1)) == 0),
                 "Mis-aligned VPIC snippet");
         }

      *cursor++ = 0xe8;  // CALL
      disp32 = cg()->branchDisplacementToHelperOrTrampoline(cursor+4, _dispatchSymRef);
      *(int32_t *)cursor = disp32;

      cg()->addAOTRelocation(new (cg()->trHeapMemory())
         TR::ExternalRelocation(cursor,
                                    (uint8_t *)_dispatchSymRef,
                                    TR_HelperAddress,
                                    cg()), __FILE__, __LINE__, _startOfPicInstruction->getNode());
      cursor += 4;

      // Populate vtable dispatch needs its stack map here.
      //
      gcMap().registerStackMap(cursor, cg());

      // Add padding after the call to snippet to hold the eventual indirect call instruction.
      //
      if (TR::Compiler->target.is64Bit())
         {
         *(uint16_t *)cursor = 0;
         cursor += 2;

         if (callModRMByte == 0x94)
            {
            // SIB byte required for CMP
            //
            *(uint8_t *)cursor = 0;
            cursor++;
            }
         }
      else
         {
         *(uint8_t *)cursor = 0;
         cursor++;
         }

      // Restart jump (always long for predictable size).
      //
      // TODO: no longer the case since data moved before call.
      //
      disp32 = _doneLabel->getCodeLocation() - (cursor + 5);
      *cursor++ = 0xe9;
      *(int32_t *)cursor = disp32;
      cursor += 4;

      resolveSlotHelper = TR_X86resolveVPicClass;
      populateSlotHelper = TR_X86populateVPicSlotClass;
      sizeofPicSlot = x86Linkage->VPicParameters.roundedSizeOfSlot;
      }

   if (_numberOfSlots >= 1)
      {
      // Patch each Pic slot to route through the population helper
      //
      int32_t numPicSlots = _numberOfSlots;
      uint8_t *picSlotCursor = _startOfPicInstruction->getBinaryEncoding();

      TR::SymbolReference *resolveSlotHelperSymRef =
         cg()->symRefTab()->findOrCreateRuntimeHelper(resolveSlotHelper, false, false, false);
      TR::SymbolReference *populateSlotHelperSymRef =
         cg()->symRefTab()->findOrCreateRuntimeHelper(populateSlotHelper, false, false, false);

      // Patch first slot test with call to resolution helper.
      //
      *picSlotCursor++ = 0xe8;    // CALL
      disp32 = cg()->branchDisplacementToHelperOrTrampoline(picSlotCursor+4, resolveSlotHelperSymRef);
      *(int32_t *)picSlotCursor = disp32;

      cg()->addAOTRelocation(new (cg()->trHeapMemory())
         TR::ExternalRelocation(picSlotCursor,
                                    (uint8_t *)resolveSlotHelperSymRef,
                                    TR_HelperAddress,
                                    cg()),  __FILE__, __LINE__, _startOfPicInstruction->getNode());

         picSlotCursor = (uint8_t *)(picSlotCursor - 1 + sizeofPicSlot);

         // Patch remaining slots with call to populate helper.
         //
         while (--numPicSlots)
            {
            *picSlotCursor++ = 0xe8;    // CALL
            disp32 = cg()->branchDisplacementToHelperOrTrampoline(picSlotCursor+4, populateSlotHelperSymRef);
            *(int32_t *)picSlotCursor = disp32;

            cg()->addAOTRelocation(new (cg()->trHeapMemory())
               TR::ExternalRelocation(picSlotCursor,
                                          (uint8_t *)populateSlotHelperSymRef,
                                          TR_HelperAddress,
                                          cg()), __FILE__, __LINE__, _startOfPicInstruction->getNode());
            picSlotCursor = (uint8_t *)(picSlotCursor - 1 + sizeofPicSlot);
            }
      }

   return cursor;
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::X86PicDataSnippet *snippet)
   {
   if (pOutFile == NULL)
      return;

   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();

   // Account for VPic data appearing before the actual entry label.
   //
   if (!snippet->isInterface())
      {
      // TODO: clean this up!
      //
      if (TR::Compiler->target.is64Bit())
         bufferPos -= (4 + 16);
      else
         bufferPos -= (1 + 8);

      uint32_t offset = bufferPos - _cg->getCodeStart();
      trfprintf(pOutFile, "\n\n" POINTER_PRINTF_FORMAT " %08x %*s", bufferPos, offset, 65, " <<< VPic Data >>>");
      }
   else
      {
      printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet));
      }

   TR::SymbolReference *methodSymRef = snippet->getMethodSymRef();
   TR::SymbolReference *dispatchSymRef = snippet->getDispatchSymRef();

   if (snippet->isInterface())
      {
      // Call lookup dispatch.
      //
      printPrefix(pOutFile, NULL, bufferPos, 5);
      trfprintf(pOutFile, "call\t%s \t\t%s " POINTER_PRINTF_FORMAT,
                    getName(dispatchSymRef),
                    commentString(),
                    dispatchSymRef->getMethodAddress());
      bufferPos += 5;

      // Restart JMP (always 5 bytes).
      //
      printPrefix(pOutFile, NULL, bufferPos, 5);
      printLabelInstruction(pOutFile, "jmp", snippet->getDoneLabel());
      bufferPos += 5;

      if (methodSymRef->isUnresolved())
         {
         const char *op = (sizeof(intptrj_t) == 4) ? "DD" : "DQ";

         printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
         trfprintf(pOutFile, "%s\t" POINTER_PRINTF_FORMAT "\t\t%s owning method cpAddr",
                       op, (intptrj_t)methodSymRef->getOwningMethod(_comp)->constantPool(), commentString());
         bufferPos += sizeof(intptrj_t);

         printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
         trfprintf(pOutFile, "%s\t" POINTER_PRINTF_FORMAT "\t\t%s cpIndex", op, (intptrj_t)methodSymRef->getCPIndexForVM(), commentString());
         bufferPos += sizeof(intptrj_t);

         printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
         trfprintf(pOutFile, "%s\t" POINTER_PRINTF_FORMAT "\t\t%s interface class", op, 0, commentString());
         bufferPos += sizeof(intptrj_t);

         printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
         trfprintf(pOutFile, "%s\t" POINTER_PRINTF_FORMAT "\t\t%s interface method index", op, 0, commentString());
         bufferPos += sizeof(intptrj_t);

         if (TR::Compiler->target.is64Bit())
            {
            // REX+MOV of MOVRegImm64 instruction
            //
            printPrefix(pOutFile, NULL, bufferPos, 1);
            trfprintf(pOutFile, "%s\t%s%02x%s\t\t\t\t\t\t\t\t%s REX of MOVRegImm64",
                          dbString(),
                          hexPrefixString(),
                          *bufferPos,
                          hexSuffixString(),
                          commentString());
            bufferPos += 1;

            printPrefix(pOutFile, NULL, bufferPos, 1);
            trfprintf(pOutFile, "%s\t%02x\t\t\t\t\t\t\t\t%s MOV opcode of MOVRegImm64",
                          dbString(),
                          *bufferPos,
                          commentString());
            bufferPos += 1;
            }
         else
            {
            // REX+MOV of MOVRegImm64 instruction
            //
                    printPrefix(pOutFile, NULL, bufferPos, 1);
            trfprintf(pOutFile, "%s\t%s%02x%s\t\t\t\t\t\t\t\t%s ModRM of CMP",
                          dbString(),
                          hexPrefixString(),
                          *bufferPos,
                          hexSuffixString(),
                          commentString());
            bufferPos += 1;
            }
         }
      }
   else
      {
      uint8_t callModRM = 0;

      if (methodSymRef->isUnresolved())
         {
         const char *op = (sizeof(intptrj_t) == 4) ? "DD" : "DQ";

         printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
         trfprintf(pOutFile, "%s\t" POINTER_PRINTF_FORMAT "\t\t%s owning method cpAddr",
                       op, (intptrj_t)methodSymRef->getOwningMethod(_comp)->constantPool(),
                       commentString());
         bufferPos += sizeof(intptrj_t);

         printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
         trfprintf(pOutFile, "%s\t" POINTER_PRINTF_FORMAT "\t\t%s cpIndex", op, (intptrj_t)methodSymRef->getCPIndexForVM(),
                       commentString());
         bufferPos += sizeof(intptrj_t);

         if (TR::Compiler->target.is64Bit())
            {
            printPrefix(pOutFile, NULL, bufferPos, 1);
            trfprintf(pOutFile, "%s\t%02x\t\t\t\t\t\t\t\t%s REX of MOVRegImm64",
                          dbString(),
                          *bufferPos,
                          commentString());
            bufferPos += 1;

            printPrefix(pOutFile, NULL, bufferPos, 1);
            trfprintf(pOutFile, "%s\t%02x\t\t\t\t\t\t\t\t%s MOV opcode of MOVRegImm64",
                          dbString(),
                          *bufferPos,
                          commentString());
            bufferPos += 1;

            printPrefix(pOutFile, NULL, bufferPos, 1);
            trfprintf(pOutFile, "%s\t%02x\t\t\t\t\t\t\t\t%s REX of CallMem",
                          dbString(),
                          *bufferPos,
                          commentString());
            bufferPos += 1;

            callModRM = *bufferPos;
            printPrefix(pOutFile, NULL, bufferPos, 1);
            trfprintf(pOutFile, "%s\t%02x\t\t\t\t\t\t\t\t%s ModRM for CALLMem",
                          dbString(),
                          callModRM,
                          commentString());
            bufferPos += 1;
            }
         else
            {
            printPrefix(pOutFile, NULL, bufferPos, 1);
            trfprintf(pOutFile, "%s\t%02x\t\t\t\t\t\t\t\t%s ModRM for CMPRegImm4",
                          dbString(),
                          *bufferPos,
                          commentString());
            bufferPos += 1;
            }
         }

      if (TR::Compiler->target.is64Bit())
         printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet));

      // Call through vtable.
      //
      int32_t length;

      if (TR::Compiler->target.is64Bit())
         {
         length = 7;
         if (callModRM == 0x94)
            length++;
         }
      else
         {
         length = 6;
         }

      printPrefix(pOutFile, NULL, bufferPos, length);
      trfprintf(pOutFile, "call\t%s \t\t%s " POINTER_PRINTF_FORMAT "\tpatched with vtable call",
                    getName(dispatchSymRef),
                    commentString(),
                    dispatchSymRef->getMethodAddress());
      bufferPos += length;

      // Restart JMP (always 5 bytes).
      //
      printPrefix(pOutFile, NULL, bufferPos, 5);
      printLabelInstruction(pOutFile, "jmp", snippet->getDoneLabel());
      bufferPos += 5;

      }
   }



uint32_t TR::X86PicDataSnippet::getLength(int32_t estimatedSnippetStart)
   {
   if (isInterface())
      {
      return   5                                 // Lookup dispatch
             + 5                                 // JMP done
             + (4 * sizeof(intptrj_t))           // Resolve slots
             + (TR::Compiler->target.is64Bit() ? 2 : 1);  // ModRM or REX+MOV
      }
   else
      {
      return   6                                 // CALL [Mem] (pessimistically assume a SIB is needed)
             + (TR::Compiler->target.is64Bit() ? 2 : 0)   // REX for CALL + SIB for CALL (64-bit)
             + 5                                 // JMP done
             + (2 * sizeof(intptrj_t))           // cpAddr, cpIndex

             // 64-bit Data
             // -----------
             //  2 (REX+MOV)
             // +2 (REX+ModRM for CALL)
             //
             // 32-bit Data
             // -----------
             //  1 (ModRM for CMP)
             //
             + (TR::Compiler->target.is64Bit() ? 4 : 1)
             + cg()->getLowestCommonCodePatchingAlignmentBoundary()-1;
      }
   }


uint8_t *
TR::X86CallSnippet::alignCursorForCodePatching(
      uint8_t *cursor,
      bool alignWithNOPs)
   {
   intptrj_t alignedCursor =
      ((intptrj_t)cursor + (cg()->getLowestCommonCodePatchingAlignmentBoundary()-1) &
      (intptrj_t)(~(cg()->getLowestCommonCodePatchingAlignmentBoundary()-1)));

   intptrj_t paddingLength = alignedCursor - (intptrj_t)cursor;

   if (alignWithNOPs && (paddingLength > 0))
      {
      return (uint8_t *)(cg()->generatePadding(cursor, paddingLength));
      }
   else
      {
      return (uint8_t *)alignedCursor;
      }
   }


// Determine the appropriate interpreted dispatch helper based on the return
// type of the target method.
//
TR_RuntimeHelper TR::X86CallSnippet::getInterpretedDispatchHelper(
   TR::SymbolReference  *methodSymRef,
   TR::DataType         type,
   bool                  isSynchronized,
   TR::CodeGenerator    *cg)
   {
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

   if (methodSymbol->isVMInternalNative() || methodSymbol->isJITInternalNative())
      return TR_icallVMprJavaSendNativeStatic;

   if (methodSymbol->isHelper() &&
       methodSymRef == cg->symRefTab()->element(TR_induceOSRAtCurrentPC))
      {
      return TR_induceOSRAtCurrentPC;
      }

   TR_RuntimeHelper helper;

   switch (type)
      {
      case TR::NoType:
         helper = TR_X86interpreterVoidStaticGlue; break;

      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
         helper = TR_X86interpreterIntStaticGlue; break;

      case TR::Address:
         helper = TR_X86interpreterAddressStaticGlue; break;

      case TR::Int64:
         helper = TR_X86interpreterLongStaticGlue; break;

      case TR::Float:
         helper = TR_X86interpreterFloatStaticGlue; break;

      case TR::Double:
         helper = TR_X86interpreterDoubleStaticGlue; break;

      default:
         TR_ASSERT(0, "Bad return data type for a call node.  DataType was %s\n",
               cg->comp()->getDebug()->getName(type));
         return (TR_RuntimeHelper)0;
      }

   if (isSynchronized)
      {
      // Choose the synchronized form of dispatch helper.
      //
      helper = (TR_RuntimeHelper)(((int32_t)helper)+1);
      }

   return helper;
   }


TR_RuntimeHelper
TR::X86CallSnippet::getDirectToInterpreterHelper(
   TR::MethodSymbol   *methodSymbol,
   TR::DataType        type,
   bool                synchronised)
   {
   return methodSymbol->getVMCallHelper();
   }


uint8_t *TR::X86CallSnippet::emitSnippetBody()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   uint8_t            *cursor = cg()->getBinaryBufferCursor();
   TR::SymbolReference *methodSymRef = _realMethodSymbolReference ? _realMethodSymbolReference : getNode()->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   int32_t             disp32;
   TR::Compilation *comp = cg()->comp();

   bool needToSetCodeLocation = true;
   bool isJitInduceOSRCall = false;

   if (TR::Compiler->target.is64Bit() &&
       methodSymbol->isHelper() &&
       methodSymRef == cg()->symRefTab()->element(TR_induceOSRAtCurrentPC))
      {
      isJitInduceOSRCall = true;
      }

   if (TR::Compiler->target.is64Bit())
      {
      // Backspill register linkage arguments to the stack.
      //
      TR::Linkage *linkage = cg()->getLinkage(methodSymbol->getLinkageConvention());
      getSnippetLabel()->setCodeLocation(cursor);
      cursor = linkage->storeArguments(getNode(), cursor, false, NULL);
      needToSetCodeLocation = false;

      if (comp->getCodeCacheSwitched() &&
          (methodSymRef->getReferenceNumber()>=TR_AMD64numRuntimeHelpers))
         {
         fej9->reserveTrampolineIfNecessary(comp, methodSymRef, true);
         }
      }

   if (methodSymRef->isUnresolved() || fej9->forceUnresolvedDispatch())
      {
      // Unresolved interpreted dispatch snippet shape:
      //
      // 64-bit
      // ======
      //       align 8
      // (10)  call  interpreterUnresolved{Static|Special}Glue  ; replaced with "mov rdi, 0x0000aabbccddeeff"
      // (5)   call  updateInterpreterDispatchGlueSite          ; replaced with "JMP disp32"
      // (2)   dw    2-byte glue method helper index
      // (8)   dq    cpAddr
      // (4)   dd    cpIndex
      //
      // 32-bit
      // ======
      //       align 8
      // (5)   call  interpreterUnresolved{Static|Special}Glue  ; replaced with "mov edi, 0xaabbccdd"
      // (3)   NOP
      // (5)   call  updateInterpreterDispatchGlueSite          ; replaced with "JMP disp32"
      // (2)   dw    2-byte glue method helper index
      // (4)   dd    cpAddr
      // (4)   dd    cpIndex
      //

      TR_ASSERT(!isJitInduceOSRCall || !fej9->forceUnresolvedDispatch(), "calling jitInduceOSR is not supported yet under AOT\n");
      cursor = alignCursorForCodePatching(cursor, TR::Compiler->target.is64Bit());

      if (comp->getOption(TR_EnableHCR))
         {
         cg()->jitAddUnresolvedAddressMaterializationToPatchOnClassRedefinition(cursor);
         }

      if (needToSetCodeLocation)
         {
         getSnippetLabel()->setCodeLocation(cursor);
         }

      TR_ASSERT((methodSymbol->isStatic() || methodSymbol->isSpecial() || fej9->forceUnresolvedDispatch()), "Unexpected unresolved dispatch");

      // CALL interpreterUnresolved{Static|Special}Glue
      //
      TR_RuntimeHelper resolutionHelper = methodSymbol->isStatic() ?
         TR_X86interpreterUnresolvedStaticGlue : TR_X86interpreterUnresolvedSpecialGlue;

      TR::SymbolReference *helperSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(resolutionHelper, false, false, false);

      *cursor++ = 0xe8;    // CALL
      disp32 = cg()->branchDisplacementToHelperOrTrampoline(cursor+4, helperSymRef);
      *(int32_t *)cursor = disp32;

      cg()->addAOTRelocation(new (cg()->trHeapMemory())
         TR::ExternalRelocation(cursor,
                                    (uint8_t *)helperSymRef,
                                    TR_HelperAddress,
                                    cg()),  __FILE__, __LINE__, getNode());
      cursor += 4;

      if (TR::Compiler->target.is64Bit())
         {
         // 5 bytes of zeros to fill out the MOVRegImm64 instruction.
         //
         *(int32_t *)cursor = 0;
         cursor += 4;
         *cursor++ = 0x00;
         }
      else
         {
         // 3-byte NOP (executable).
         //
         cursor = cg()->generatePadding(cursor, 3);
         }

      // CALL updateInterpreterDispatchGlueSite
      //
      helperSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_X86updateInterpreterDispatchGlueSite, false, false, false);

      *cursor++ = 0xe8;    // CALL
      disp32 = cg()->branchDisplacementToHelperOrTrampoline(cursor+4, helperSymRef);
      *(int32_t *)cursor = disp32;

      cg()->addAOTRelocation(new (cg()->trHeapMemory())
         TR::ExternalRelocation(cursor,
                                    (uint8_t *)helperSymRef,
                                    TR_HelperAddress,
                                    cg()),  __FILE__, __LINE__, getNode());
      cursor += 4;

      // DW dispatch helper index for the method's return type.
      //
      TR_RuntimeHelper dispatchHelper = getInterpretedDispatchHelper(methodSymRef, getNode()->getDataType(), false, cg());
      *(uint16_t *)cursor = (uint16_t)dispatchHelper;
      cursor += 2;

      // DD/DQ cpAddr
      //
      intptrj_t cpAddr = (intptrj_t)methodSymRef->getOwningMethod(comp)->constantPool();
      *(intptrj_t *)cursor = cpAddr;

      TR::Relocation *relocation;

      relocation = new (cg()->trHeapMemory()) TR::ExternalRelocation(
            cursor,
            *(uint8_t **)cursor,
             getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
            TR_ConstantPool,
            cg());

      cg()->addAOTRelocation(relocation, __FILE__, __LINE__, getNode());
      cursor += sizeof(intptrj_t);

      // DD cpIndex
      //
      *(uint32_t *)cursor = methodSymRef->getCPIndexForVM();
      cursor += 4;
      }
   else
      {
      // Resolved method dispatch.
      //
      // 64-bit
      // ======

      // (10)  MOV rdi, 0x0000aabbccddeeff                      ; load RAM method
      // (5)   JMP interpreter{*}StaticGlue
      //
      // 32-bit
      // ======
      //
      // (5)   mov edi, 0xaabbccdd                              ; load RAM method
      // (5)   JMP interpreter{*}StaticGlue
      //

      if (needToSetCodeLocation)
         {
         getSnippetLabel()->setCodeLocation(cursor);
         }

      //SD: for jitInduceOSR we don't need to set the RAM method (the method that the VM needs to start executing)
      //because VM is going to figure what method to execute by looking up the the jitPC in the GC map and finding
      //the desired invoke bytecode.
      if (!isJitInduceOSRCall)
         {
         intptrj_t ramMethod = (intptr_t)methodSymbol->getMethodAddress();

         if (TR::Compiler->target.is64Bit())
            {
            // MOV rdi, Imm64
            //
            *(uint16_t *)cursor = 0xbf48;
            cursor += 2;
            }
         else
            {
            // MOV edi, Imm32
            //
            *cursor++ = 0xbf;
            }

         *(intptrj_t *)cursor = ramMethod;

         // HCR in TR::X86CallSnippet::emitSnippetBody register the method address
         //
         if (comp->getOption(TR_EnableHCR))
            cg()->jitAddPicToPatchOnClassRedefinition((void *)ramMethod, (void *)cursor);

         cursor += sizeof(intptrj_t);
         }

      // JMP imm32
      //
      *cursor++ = 0xe9;

      TR_RuntimeHelper dispatchHelper;

      dispatchHelper = getInterpretedDispatchHelper(
         methodSymRef,
         getNode()->getDataType(),
         methodSymbol->isSynchronised(),
         cg());
      TR::SymbolReference *dispatchSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(dispatchHelper, false, false, false);

      disp32 = cg()->branchDisplacementToHelperOrTrampoline(cursor+4, dispatchSymRef);
      *(int32_t *)cursor = disp32;

      cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                            (uint8_t *)dispatchSymRef,
                                                            TR_HelperAddress,
                                                            cg()),  __FILE__, __LINE__, getNode());
      cursor += 4;

      }

   return cursor;
   }


uint32_t TR::X86CallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg()->fe());
   uint32_t length = 0;

   TR::SymbolReference *methodSymRef = _realMethodSymbolReference ? _realMethodSymbolReference : getNode()->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

   if (TR::Compiler->target.is64Bit())
      {
      TR::Linkage *linkage = cg()->getLinkage(methodSymbol->getLinkageConvention());

      int32_t codeSize;
      (void)linkage->storeArguments(getNode(), NULL, true, &codeSize);
      length += codeSize;
      }

   if (methodSymRef->isUnresolved() || fej9->forceUnresolvedDispatch())
      {
      // +7 accounts for maximum length alignment padding.
      //
      if (TR::Compiler->target.is64Bit())
         length += (7+10+5+2+8+4);
      else
         length += (7+5+3+5+2+4+4);
      }
   else
      {
      if (TR::Compiler->target.is64Bit())
         length += (10+5);
      else
         length += (5+5);
      }

   return length;
   }


uint8_t *TR::X86UnresolvedVirtualCallSnippet::emitSnippetBody()
   {
   TR_ASSERT(TR::Compiler->target.is32Bit(), "TR::X86UnresolvedVirtualCallSnippet only available on 32-bit");
   TR::Compilation *comp = cg()->comp();
   uint8_t *cursor = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(cursor);

   // Preserve edx
   //
   *cursor++ = 0x52;    // PUSH edx to preserve

   // Call the runtime helper
   //
   *cursor++ = 0xe8;    // CALL resolveAndPopulateVTableDispatch

   TR::SymbolReference *glueSymRef =
      cg()->symRefTab()->findOrCreateRuntimeHelper(TR_IA32interpreterUnresolvedVTableSlotGlue, false, false, false);
   uint8_t *glueAddress = (uint8_t *)glueSymRef->getMethodAddress();
   cg()->addAOTRelocation(new (comp->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                                     (uint8_t *)glueSymRef,
                                                                                     TR_HelperAddress,
                                                                                     cg()),
                             __FILE__, __LINE__, getNode());

   *(int32_t *)cursor = (int32_t)((uint8_t *)glueAddress - cursor - 4);
   cursor += 4;

   // needs a stack map at this location because populateAndResolveVTableSlot provides this address as the return
   // address in this frame
   gcMap().registerStackMap(cursor, cg());

   // Lay down constant pool and cpindex for jitResolveVirtualMethod helper to use
   //
   uintptrj_t cpAddr = (uintptrj_t)_methodSymRef->getOwningMethod(comp)->constantPool();
   *(intptrj_t *)cursor = cpAddr;
   cg()->addAOTRelocation(
      new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                            (uint8_t *)cpAddr,
                                                            getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                            TR_ConstantPool,
                                                            cg()),
                 __FILE__, __LINE__, getNode());


   cursor += sizeof(intptrj_t);
   *(uintptrj_t *)cursor = (uintptrj_t)_methodSymRef->getCPIndexForVM();
   cursor += sizeof(intptrj_t);

   // Squirrel away the first two encoded bytes of the original call instruction.
   //
   uint8_t *callInstruction = _callInstruction->getBinaryEncoding();
   *cursor++ = *(callInstruction);
   *cursor++ = *(callInstruction+1);

   // Write a call to this snippet over the original call instruction.
   //
   *callInstruction = 0xe8;     // CALLImm4
   *(uint32_t *)(callInstruction+1) = (uint32_t)(cg()->getBinaryBufferCursor() - (callInstruction + 5));

   return cursor;
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::X86UnresolvedVirtualCallSnippet *snippet)
   {
   if (pOutFile == NULL)
      return;

   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet));
   printPrefix(pOutFile, NULL, bufferPos, snippet->getLength(bufferPos - (uint8_t*)0));
   trfprintf(pOutFile, "\t\t\t\t%s mysterious new unresolved virtual call snippet code",
                 commentString());
   return;

   }



uint32_t TR::X86UnresolvedVirtualCallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return
        1   // push EDX
      + 5   // CALL resolveAndPopulateVTableDispatch
      + 4   // cpAddr
      + 4   // cpIndex
      + 2;  // CALL opcode + modRM
   }
