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

#include "runtime/ARM64RelocationTarget.hpp"

void arm64CodeSync(unsigned char *codeStart, unsigned int codeSize);

uint8_t *
TR_ARM64RelocationTarget::eipBaseForCallOffset(uint8_t *reloLocation)
   {
   // reloLocation points at the start of the call offset
   return reloLocation;
   }

void
TR_ARM64RelocationTarget::storeCallTarget(uintptr_t callTarget, uint8_t *reloLocation)
   {
   // reloLocation points at the start of the call offset, so just store the uint8_t * at reloLocation
   storePointer((uint8_t *)callTarget, reloLocation);
   }

uint32_t
TR_ARM64RelocationTarget::loadCPIndex(uint8_t *reloLocation)
   {
   // reloLocation points at the cpAddress in snippet, need to find cpIndex location
   reloLocation += sizeof(uintptr_t);
   return (uint32_t)(((uintptr_t )loadPointer(reloLocation)) & 0xFFFFFFFFUL); // Mask out lower 32 bits for index
   }

uintptr_t
TR_ARM64RelocationTarget::loadThunkCPIndex(uint8_t *reloLocation)
   {
   // reloLocation points at the cpAddress in snippet, need to find cpIndex location
   reloLocation += sizeof(uintptr_t);
   return (uintptr_t )loadPointer(reloLocation);
   }

void
TR_ARM64RelocationTarget::performThunkRelocation(uint8_t *thunkBase, uintptr_t vmHelper)
   {
   uintptr_t sendTarget = vmHelper;
   int32_t *thunkRelocationData = (int32_t *)(thunkBase - sizeof(int32_t));

   storeAddressSequence((uint8_t *)vmHelper, thunkBase + *thunkRelocationData, 1);

   arm64CodeSync(thunkBase, *((int32_t*)(thunkBase - 2*sizeof(int32_t))));
   }

uint8_t *
TR_ARM64RelocationTarget::loadAddressSequence(uint8_t *reloLocation)
   {
   TR_ASSERT(0, "AOT New Relo Runtime: don't expect loadAddressSequence to be called!\n");
   uintptr_t value = 0;
   return (uint8_t *)value;
   }

void
TR_ARM64RelocationTarget::storeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber)
   {
   uintptr_t value = (uintptr_t)address;
   uint32_t *movInstruction = (uint32_t *) reloLocation;
   // immediate offset is at bit 20:5
   *movInstruction = (*movInstruction & (~((uint32_t)0xFFFF << 5))) | ((value & 0x0000FFFF) << 5);
   *(movInstruction + 1) = (*(movInstruction + 1) & (~((uint32_t)0xFFFF << 5))) | (((value >> 16) & 0x0000FFFF) << 5);
   *(movInstruction + 2) = (*(movInstruction + 2) & (~((uint32_t)0xFFFF << 5))) | (((value >> 32) & 0x0000FFFF) << 5);
   *(movInstruction + 3) = (*(movInstruction + 3) & (~((uint32_t)0xFFFF << 5))) | (((value >> 48) & 0x0000FFFF) << 5);
   }

void
TR_ARM64RelocationTarget::storeRelativeTarget(uintptr_t callTarget, uint8_t *reloLocation)
   {
   // reloLocation points at the start of the instruction. We store imm26 offset to the instruction.
   int32_t instruction = *((int32_t *)reloLocation);

   instruction &= (int32_t)0xFC000000;
   // labels will always be 4 aligned, CPU shifts # left by 2
   // during execution (26 bits -> 28 bit branch reach)
   instruction |= (callTarget >> 2) & 0x03FFFFFF;

   storeSigned32b(instruction, (uint8_t *)reloLocation);
   }

bool
TR_ARM64RelocationTarget::useTrampoline(uint8_t *helperAddress, uint8_t *baseLocation)
   {
   return
      !reloRuntime()->comp()->target().cpu.isTargetWithinUnconditionalBranchImmediateRange((intptr_t)helperAddress, (intptr_t)baseLocation) ||
      TR::Options::getCmdLineOptions()->getOption(TR_StressTrampolines);
   }

uint8_t *
TR_ARM64RelocationTarget::arrayCopyHelperAddress(J9JavaVM *javaVM)
   {
   return reinterpret_cast<uint8_t *>(javaVM->memoryManagerFunctions->referenceArrayCopy);
   }

void
TR_ARM64RelocationTarget::flushCache(uint8_t *codeStart, unsigned long size)
   {
   arm64CodeSync((unsigned char *)codeStart, (unsigned int)size);
   }
