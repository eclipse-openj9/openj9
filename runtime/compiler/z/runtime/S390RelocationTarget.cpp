/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include "z/runtime/S390RelocationTarget.hpp"

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
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "runtime/J9Runtime.hpp"
#include "runtime/MethodMetaData.h"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationTarget.hpp"

uint8_t *
TR_S390RelocationTarget::eipBaseForCallOffset(uint8_t *reloLocation)
   {
   // reloLocation points at the start of the call offset, return address is 4
   return (uint8_t *) (reloLocation);
   }

void
TR_S390RelocationTarget::storeCallTarget(uint8_t *callTarget, uint8_t *reloLocation)
   {
   // reloLocation points at the start of the call offset, so just store the uint8_t * at reloLocation
   storePointer(callTarget, reloLocation);
   }

void
TR_S390RelocationTarget::storeRelativeTarget(uintptr_t callTarget, uint8_t *reloLocation)
   {
   // reloLocation points at the start of the call offset, so just store the uint8_t * at reloLocation
   uintptr_t newOffset = ((uintptr_t)callTarget+2)/2;

   storeSigned32b((int32_t)newOffset, (uint8_t *)reloLocation);
   }

uint8_t *
TR_S390RelocationTarget::loadAddressSequence(uint8_t *reloLocation)
   {
   // reloLocation points at the start of the address, so just need to dereference as uint8_t *
   return loadPointer(reloLocation);
   }

void
TR_S390RelocationTarget::storeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber)
   {
   // reloLocation points at the start of the address, so just store the uint8_t * at reloLocation
   storePointer(address, reloLocation);
   }

uint32_t
TR_S390RelocationTarget::loadCPIndex(uint8_t *reloLocation)
   {
   // reloLocation points at the cpAddress in snippet, need to find cpIndex location // KEN
   reloLocation += sizeof(uintptr_t);
   return (loadUnsigned32b(reloLocation) & 0xFFFFFF); // Mask out bottom 24 bits for index
   }

uintptr_t
TR_S390RelocationTarget::loadThunkCPIndex(uint8_t *reloLocation)
   {
   // reloLocation points at the cpAddress in snippet, need to find cpIndex location // KEN
   reloLocation += sizeof(uintptr_t);
   return (uintptr_t )loadPointer(reloLocation);
   }

void
TR_S390RelocationTarget::performThunkRelocation(uint8_t *thunkAddress, uintptr_t vmHelper)
   {
   int32_t *thunkRelocationData = (int32_t *)(thunkAddress - sizeof(int32_t));
   *(uintptr_t *) (thunkAddress + *thunkRelocationData) = vmHelper;
   }

bool TR_S390RelocationTarget::useTrampoline(uint8_t * helperAddress, uint8_t *baseLocation)
   {
#if defined(TR_HOST_S390) && defined(TR_TARGET_64BIT) && !defined(J9ZOS390)
   return !CHECK_32BIT_TRAMPOLINE_RANGE((intptr_t)helperAddress, baseLocation);
#else
   TR::Compilation* comp = TR::comp();
   if (comp->getOption(TR_EnableRMODE64))
      return !CHECK_32BIT_TRAMPOLINE_RANGE((intptr_t)helperAddress, baseLocation);
   else
      return false;
#endif
   }

uint8_t *
TR_S390RelocationTarget::arrayCopyHelperAddress(J9JavaVM *javaVM)
   {
   return (uint8_t *)javaVM->memoryManagerFunctions->referenceArrayCopy;
   }

void
TR_S390RelocationTarget::patchMTIsolatedOffset(uint32_t offset, uint8_t *reloLocation)
  {
   /* for Z platform, isolated field row/col offset is recorded in long displacement
      Long displacement Format
       ________ ____ ____ ____ __ ________ ____________ _______
      |Op Code | R1 | X2 | B2 |DL2        | DH2        |Op Code|
      |________|____|____|____|___________|____________|_______|
      0         8   12   16   20          32           40      47

      put new offset into DL2 and DH2
   */
   TR_ASSERT(offset < 0x7FFFF, "new offset excceed Z long displacmet range\n");
   uint32_t tmpOffset = loadUnsigned32b(reloLocation);
   // clear DL2 and DH2 bits
   tmpOffset = tmpOffset & 0xf00000ff;
   // add DL2
   tmpOffset = tmpOffset | ((offset & 0xfff) << 16);
   // add DH2
   tmpOffset = tmpOffset | ((offset & 0xff000) >>4);
   storeUnsigned32b(tmpOffset, reloLocation);
  }
