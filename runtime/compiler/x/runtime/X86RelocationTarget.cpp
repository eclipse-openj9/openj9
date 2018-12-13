/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include "x/runtime/X86RelocationTarget.hpp"

#include <stdint.h>
#include "jilconsts.h"
#include "jitprotos.h"
#include "jvminit.h"
#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "j9cp.h"
#include "j9protos.h"
#include "rommeth.h"
#include "codegen/FrontEnd.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "runtime/J9Runtime.hpp"
#include "runtime/MethodMetaData.h"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationTarget.hpp"

uint8_t *
TR_X86RelocationTarget::eipBaseForCallOffset(uint8_t *reloLocation)
   {
   // reloLocation points at the start of the call offset, return address is 4
   return (uint8_t *) (reloLocation+4);
   }

void
TR_X86RelocationTarget::storeCallTarget(uintptr_t callTarget, uint8_t *reloLocation)
   {
   // reloLocation points at the start of the call offset, so just store the uint8_t * at reloLocation
   storeUnsigned32b((uint32_t)callTarget, reloLocation);
   }

void
TR_X86RelocationTarget::storeRelativeTarget(uintptr_t callTarget, uint8_t *reloLocation)
   {
   storeCallTarget(callTarget, reloLocation);
   }

uint8_t *
TR_X86RelocationTarget::loadAddressSequence(uint8_t *reloLocation)
   {
   // reloLocation points at the start of the address, so just need to dereference as uint8_t *
   return loadPointer(reloLocation);
   }

void
TR_X86RelocationTarget::storeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber)
   {
   // reloLocation points at the start of the address, so just store the uint8_t * at reloLocation
   storePointer(address, reloLocation);
   }

uint32_t
TR_X86RelocationTarget::loadCPIndex(uint8_t *reloLocation)
   {
   // reloLocation points at the cpAddress in snippet, need to find cpIndex location
   reloLocation += sizeof(uintptr_t);
   return loadUnsigned32b(reloLocation);
   }

uintptr_t
TR_X86RelocationTarget::loadThunkCPIndex(uint8_t *reloLocation)
   {
   return (uintptr_t)loadCPIndex(reloLocation);
   }

void
TR_X86RelocationTarget::performThunkRelocation(uint8_t *thunkAddress, uintptr_t vmHelper)
   {
   int32_t *thunkRelocationData = (int32_t *)(thunkAddress - sizeof(int32_t));
   *(UDATA *) (thunkAddress + *thunkRelocationData + 2) = vmHelper;
   }

bool TR_AMD64RelocationTarget::useTrampoline(uint8_t * helperAddress, uint8_t *baseLocation)
   {
   return !IS_32BIT_RIP_JUMP(helperAddress, baseLocation);
   }

uint8_t *
TR_X86RelocationTarget::arrayCopyHelperAddress(J9JavaVM *javaVM)
   {
   uintptr_t *funcdescrptr = (UDATA *)javaVM->memoryManagerFunctions->referenceArrayCopy;
   return (uint8_t *)funcdescrptr[0];
   }

void
TR_X86RelocationTarget::patchNonVolatileFieldMemoryFence(J9ROMFieldShape* resolvedField, UDATA cpAddr, U_8 descriptorByte, U_8 *instructionAddress, U_8 *snippetStartAddress, J9JavaVM *javaVM)
   {
/*
; Spare bits in the cpIndex passed in are used to specify behaviour based on the
; kind of resolution.  The anatomy of a cpIndex:
;
;       byte 3   byte 2   byte 1   byte 0
;
;      3 222  2 222    1 1      0 0      0
;      10987654 32109876 54321098 76543210
;       ||||__| ||| ||||_________________|
;       ||| |   ||| |||         |
;       ||| |   ||| |||         +---------------- cpIndex (0-16)
;       ||| |   ||| |+--------------------------- upper long dword resolution (17)
;       ||| |   ||| |+--------------------------- lower long dword resolution (18)
;       ||| |   ||| +---------------------------- check volatility (19)
;       ||| |   ||+------------------------------ resolve, but do not patch snippet template (21)
;       ||| |   |+------------------------------- resolve, but do not patch mainline code (22)
;       ||| |   +-------------------------------- long push instruction (23)
;       ||| +------------------------------------ number of live X87 registers across this resolution (24-27)
;       ||+-------------------------------------- has live XMM registers (28)
;       |+--------------------------------------- static resolution (29)
;       +---------------------------------------- 64-bit: resolved value is 8 bytes (30)
;                                                 32-bit: resolved value is high word of long pair (30)
*/

   //Determine if a volatility check is needed
   U_32 cpIndexDoubleWord = *(U_32*)(snippetStartAddress+1);

   //The volatility check falg is at bit 20 of the 32 cpIndex
   if ( !(cpIndexDoubleWord & VolCheckMask))
      return;

   //If the field is volatile, exit the function.
   if(resolvedField->modifiers & J9AccVolatile)
      {
      return;
      }

   //Is this the upper double word of a long store?
   if (cpIndexDoubleWord & VolLowerLongCheckMask)
      {
      //return;

      // change the mov reg mem opcode (8b) into a mov mem reg opcode (89)
      instructionAddress[0] = instructionAddress[0] & 0xfd;

      //change EAX to EBX
      instructionAddress[1] = instructionAddress[1] | 0x18;
      }
   //Is this the lower double word of a long store?
   else if(cpIndexDoubleWord & VolUpperLongCheckMask)
      {
      //return;

      //change the mov reg mem opcode (8b) into a mov mem reg opcode (89)
      instructionAddress[0] = instructionAddress[0] & 0xfd;

      //change EDX to ECX
      instructionAddress[1] = instructionAddress[1] & 0xef;
      instructionAddress[1] = instructionAddress[1] | 0x08;
      }
   //Is this a LCMPXCHG of a long store? ( f00f are the first two bytes of a LCMPXCHG instruction)
   else if( *((U_16*)instructionAddress) == LCMPXCHGCheckMaskWord)
      {
      //return;

      ((U_32*)instructionAddress)[0] = LCMPXCHGNOPPatchDWord;
      ((U_32*)instructionAddress)[1] = LCMPXCHGNOPPatchDWord;
      }
   else
      {
      //The reference is not volatile, patch over the write barrier with a nop.

      //Get the instruction length.
      I_8 instructionLength = (descriptorByte >> 4) & 0xf;

      //The length of the write barrier is different depending on if the processor support SSE2.
      I_16 barrierOffset;

#if defined(TR_HOST_X86) && !defined(J9HAMMER)
      if (TR::Options::getAOTCmdLineOptions() && TR::Options::getAOTCmdLineOptions()->getOption(TR_X86UseMFENCE))
         {
         //The write barrier is 3 bytes long.
         //Find the offset of the four byte aligned mfence from the start of the patched instruction.
         barrierOffset = (instructionLength + 3) & 0xfc;

         TR_ASSERT(*((U_16*)(instructionAddress+barrierOffset)) == MFenceWordCheck, "Expecting mfence, not Lock or [esp]");


         *((U_32*)(instructionAddress+barrierOffset)) |= 0x00ffffff;
         *((U_32*)(instructionAddress+barrierOffset)) &= MFenceNOPPatch;

         }
      else
#endif // defined(TR_HOST_X86) && !defined(J9HAMMER)
         {
         //The write barrier is five bytes long.
         //Find the offset of the eight byte aligned lor from the start of the patched instruction.
         barrierOffset = (instructionLength + 7) & 0xf8;

         TR_ASSERT(*((U_16*)(instructionAddress + barrierOffset)) != MFenceWordCheck, "Expecting Lock or [esp], not Mfence");

         *((U_32*)(instructionAddress + barrierOffset)) |=0xffffffff;
         *((U_32*)(instructionAddress + barrierOffset)) &= LORNOPPatchDWord;
         *((U_8*)(instructionAddress + barrierOffset + sizeof(UDATA))) |= 0xff;
         *((U_8*)(instructionAddress + barrierOffset + sizeof(UDATA))) &= LORNOPPatchTrailingByte;
         }
      }
   }

void
TR_X86RelocationTarget::patchMTIsolatedOffset(uint32_t offset, uint8_t *reloLocation)
   {
   storeUnsigned32b(offset, reloLocation);
   }
