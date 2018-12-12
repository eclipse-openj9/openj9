/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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

#include "arm/runtime/ARMRelocationTarget.hpp"

#include <stdint.h>
#include "jilconsts.h"
#include "jitprotos.h"
#include "jvminit.h"
#include "j9.h"
#include "j9consts.h"
#include "j9cfg.h"
#include "j9cp.h"
#include "j9protos.h"
#include "rommeth.h"
#include "codegen/FrontEnd.hpp"
#include "codegen/PicHelpers.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "infra/Bit.hpp"
#include "runtime/MethodMetaData.h"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/J9Runtime.hpp"

void armCodeSync(unsigned char *codeStart, unsigned int codeSize);

uint8_t *
TR_ARMRelocationTarget::eipBaseForCallOffset(uint8_t *reloLocation)
   {
   // reloLocation points at the start of the call offset
   return reloLocation;
   }

void
TR_ARMRelocationTarget::storeCallTarget(uintptr_t callTarget, uint8_t *reloLocation)
   {
   // reloLocation points at the start of the call offset, so just store the uint8_t * at reloLocation
   storePointer((uint8_t *)callTarget, reloLocation);
   }

uint32_t
TR_ARMRelocationTarget::loadCPIndex(uint8_t *reloLocation)
   {
   // reloLocation points at the cpAddress in snippet, need to find cpIndex location;
   reloLocation -= sizeof(uint32_t);
   return (loadUnsigned32b(reloLocation) & 0xFFFFFF); // Mask out bottom 24 bits for index
   }

uintptr_t
TR_ARMRelocationTarget::loadThunkCPIndex(uint8_t *reloLocation)
   {
   // reloLocation points at the cpAddress in snippet, need to find cpIndex location
   reloLocation += sizeof(uintptr_t);
   return (uintptr_t )loadPointer(reloLocation);
   }

void
TR_ARMRelocationTarget::performThunkRelocation(uint8_t *thunkBase, uintptr_t vmHelper)
   {
   int32_t *thunkRelocationData = (int32_t *)(thunkBase - sizeof(int32_t));
   *(UDATA *) (thunkBase + *thunkRelocationData) = vmHelper;

   armCodeSync(thunkBase, *((int32_t*)(thunkBase - 2*sizeof(int32_t))));
   }

uint8_t *
TR_ARMRelocationTarget::loadAddressSequence(uint8_t *reloLocation)
   {
   TR_ASSERT(0, "AOT New Relo Runtime: don't expect loadAddressSequence to be called!\n");
   uintptr_t value = 0;
   return (uint8_t *)value;
   }

void
TR_ARMRelocationTarget::storeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber)
   {
   intParts localVal((uint32_t)address);
   uint16_t *patchAddr1, *patchAddr2, *patchAddr3, *patchAddr4;
   uint32_t *patchAddr = (uint32_t *)reloLocation;
   patchAddr1 = (uint16_t *)&patchAddr[0];
   patchAddr2 = (uint16_t *)&patchAddr[1];
   patchAddr3 = (uint16_t *)&patchAddr[2];
   patchAddr4 = (uint16_t *)&patchAddr[3];

   uint16_t orig1 = loadUnsigned16b((uint8_t *)patchAddr1);
   uint16_t orig2 = loadUnsigned16b((uint8_t *)patchAddr2);
   uint16_t orig3 = loadUnsigned16b((uint8_t *)patchAddr3);
   uint16_t orig4 = loadUnsigned16b((uint8_t *)patchAddr4);
   *patchAddr1 = (orig1 & 0xf000) | 0x400 | localVal.getByte3(); // 0x400 means rotate right by 8bit
   *patchAddr2 = (orig2 & 0xf000) | 0x800 | localVal.getByte2();
   *patchAddr3 = (orig3 & 0xf000) | 0xc00 | localVal.getByte1();
   *patchAddr4 = (orig4 & 0xf000) | localVal.getByte0();
   }

void
TR_ARMRelocationTarget::storeRelativeTarget(uintptr_t callTarget, uint8_t *reloLocation)
   {
   // reloLocation points at the start of the call offset, so just store the uint8_t * at reloLocation
   int32_t instruction = *((int32_t *)reloLocation);

   instruction &= (int32_t)0xFF000000;
   // labels will always be 4 aligned, CPU shifts # left by 2
   // during execution (24 bits -> 26 bit branch reach)
   instruction |= ((callTarget - 8) >> 2) & 0x00FFFFFF;

   storeSigned32b(instruction, (uint8_t *)reloLocation);
   }

bool TR_ARMRelocationTarget::useTrampoline(uint8_t * helperAddress, uint8_t *baseLocation)
   {
   return !(((IDATA)(helperAddress - baseLocation) <=BRANCH_FORWARD_LIMIT) && ((IDATA)(helperAddress - baseLocation) >=BRANCH_BACKWARD_LIMIT));
   }

uint8_t *
TR_ARMRelocationTarget::arrayCopyHelperAddress(J9JavaVM *javaVM)
   {
   uintptr_t *funcdescrptr = (UDATA *)javaVM->memoryManagerFunctions->referenceArrayCopy;
   uintptr_t value = (UDATA)funcdescrptr;

   return (uint8_t *)value;
   }

void
TR_ARMRelocationTarget::flushCache(uint8_t *codeStart, unsigned long size)
   {
   armCodeSync((unsigned char *)codeStart, (unsigned int) size);
   }

