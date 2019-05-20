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

#include "p/runtime/PPCRelocationTarget.hpp"

#include "j9cp.h"
#include "j9cfg.h"
#include "j9.h"
#include "j9consts.h"
#include "jilconsts.h"
#include "rommeth.h"
#include "j9protos.h"
#include "jvminit.h"
#include "jitprotos.h"
#include "codegen/FrontEnd.hpp"
#include "codegen/PicHelpers.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "runtime/J9Runtime.hpp"
#include "runtime/MethodMetaData.h"
#include "runtime/RelocationRuntime.hpp"

void ppcCodeSync(unsigned char *codeStart, unsigned int codeSize);

uint8_t *
TR_PPCRelocationTarget::eipBaseForCallOffset(uint8_t *reloLocation)
   {
   // reloLocation points at the start of the call offset, return address is 4
   return (uint8_t *) (reloLocation);
   }

void
TR_PPCRelocationTarget::storeCallTarget(uintptr_t callTarget, uint8_t *reloLocation)
   {
   // reloLocation points at the start of the call offset, so just store the uint8_t * at reloLocation
   storePointer((uint8_t *)callTarget, reloLocation);
   }

uint32_t
TR_PPCRelocationTarget::loadCPIndex(uint8_t *reloLocation)
   {
   // reloLocation points at the cpAddress in snippet, need to find cpIndex location // KEN
   reloLocation -= sizeof(uint32_t);
   return (loadUnsigned32b(reloLocation) & 0xFFFFFF); // Mask out bottom 24 bits for index
   }

uintptr_t
TR_PPCRelocationTarget::loadThunkCPIndex(uint8_t *reloLocation)
   {
   // reloLocation points at the cpAddress in snippet, need to find cpIndex location // KEN
   reloLocation += sizeof(uintptr_t);
   return (uintptr_t )loadPointer(reloLocation);
   }

void
TR_PPCRelocationTarget::performThunkRelocation(uint8_t *thunkBase, uintptr_t vmHelper)
   {
#if defined(TR_HOST_64BIT)
   //printf("TR_PPCRelocationTarget::performThunkRelocation thunkBase: %p\n", thunkBase);fflush(stdout);
   uintptr_t sendTarget = vmHelper;
   int32_t *thunkRelocationData;

   thunkRelocationData = (int32_t *)(thunkBase - sizeof(int32_t));

   *(int32_t *) (thunkBase + *thunkRelocationData) = 0x3c800000 | ( (sendTarget>>48) & 0x0000FFFF);
   *(int32_t *) (thunkBase + *thunkRelocationData + 4) = 0x60840000 | ( (sendTarget>>32) & 0x0000FFFF);
   *(int32_t *) (thunkBase + *thunkRelocationData + 12) = 0x64840000 | ( (sendTarget>>16) & 0x0000FFFF);
   *(int32_t *) (thunkBase + *thunkRelocationData + 16) = 0x60840000 | (sendTarget & 0x0000FFFF);

   ppcCodeSync(thunkBase, *((int32_t*)(thunkBase - 2*sizeof(int32_t))));
#else
   int32_t sendTarget = (int32_t)vmHelper;

   uint8_t *cursor;
   int32_t *thunkRelocationData;
   thunkRelocationData = (int32_t *)(thunkBase - sizeof(int32_t));
   cursor = thunkBase + *thunkRelocationData;

   if (!(sendTarget & 0x00008000))
      {
      // li r4, lower
      *(int32_t *) (cursor) = 0x38800000 | (sendTarget & 0x0000FFFF);
      cursor += 4;
      // oris r4, r4, upper
      *(int32_t *) (cursor) = 0x64840000 | ( (sendTarget>>16) & 0x0000FFFF);
      cursor += 4;
      }
   else
      {
      // lis gr4, upper
      *(int32_t *) (cursor) = 0x3c800000 | ( ((sendTarget>>16) + ((sendTarget&(1<<15)) ? 1 : 0) )
& 0x0000FFFF );
      cursor += 4;
      // addi gr4, gr4, lower
      *(int32_t *) (cursor) = 0x38840000 | ( sendTarget & 0x0000FFFF );
      cursor += 4;

      if(sendTarget & 0x80000000)
         {
         // rlwinm r4,r4,sh=0,mb=0,me=31
         *(int32_t *)cursor = 0x5484003e;
         cursor += 4;
         }
      }

   // mtctr gr4
   *(int32_t *)cursor = 0x7c8903a6;
   cursor += 4;

   // bcctr
   *(int32_t *)cursor = 0x4e800420;
   cursor += 4;

   ppcCodeSync(thunkBase, *((int32_t*)(thunkBase - 2*sizeof(int32_t))));
#endif
   }

uint8_t *
TR_PPCRelocationTarget::loadAddress(uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   uint16_t highValue = loadUnsigned16b(reloLocationHigh);
   uint16_t lowValue = loadUnsigned16b(reloLocationLow);
   uintptr_t value = ((uintptr_t) highValue << 16) + (uintptr_t) lowValue; // Only 32-bit should use this so pointer is 32-bit
   return (uint8_t *)value;
   }

void
TR_PPCRelocationTarget::storeAddress(uint8_t *address, uint8_t *reloLocationHigh, uint8_t *reloLocationLow, uint32_t seqNumber)
   {
   uintptr_t value = (uintptr_t)address;
   uint16_t highValue;

   if (seqNumber == 1)
      {
      highValue = (uint16_t) (((value >> 16) + ((value & (1 << 15)) ? 1 : 0)) & 0x0000FFFF);
      }
   else
      {
      highValue = (uint16_t) ((value >> 16) & 0x0000FFFF);
      }

   uint16_t lowValue = (uint16_t) (value & 0x0000FFFF);

   storeUnsigned16b(highValue, reloLocationHigh);
   storeUnsigned16b(lowValue, reloLocationLow);
   }

uint8_t *
TR_PPCRelocationTarget::loadAddressSequence(uint8_t *reloLocation)
   {
   TR_ASSERT(0, "AOT New Relo Runtime: don't expect loadAddressSequence to be called!\n");
   uintptr_t value = 0;
   return (uint8_t *)value;
   }

void
TR_PPCRelocationTarget::storeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber)
   {
   //TR_ASSERT(0, "Error: storeAddressSequence not implemented in relocation target base class");
   storePointer(address, reloLocation);
   }

void
TR_PPC32RelocationTarget::storeRelativeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber)
   {
   uint16_t value1, value2;
   uint16_t *patchAddr1, *patchAddr2;

   uintptr_t value = (uintptr_t)address;
   uint32_t *patchAddr = (uint32_t *)((U_8 *)reloLocation+ (TR::Compiler->target.cpu.isBigEndian()?2:0));

   value1 = ((value>>16) & 0xffff);
   value2 = (value & 0xffff);
   patchAddr1 = (uint16_t *)&patchAddr[0];
   patchAddr2 = (uint16_t *)&patchAddr[1];

   *patchAddr1 |= value1;
   *patchAddr2 |= value2;
   }

void
TR_PPC64RelocationTarget::storeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber)
   {
   uint16_t value1, value2, value3, value4;
   uint16_t *patchAddr1, *patchAddr2, *patchAddr3, *patchAddr4;
   uintptr_t highValue;

   uintptr_t value = (uintptr_t)address;
   uint32_t *patchAddr = (uint32_t *)((U_8 *)reloLocation+ (TR::Compiler->target.cpu.isBigEndian()?2:0));

   if (seqNumber % 2 == 0)
      highValue = HI_VALUE(value);
   else
      highValue = value>>16;

   switch (seqNumber)
      {
      case 1:
      case 2:
         {
         value1 = ((highValue>>32) & 0xffff);
         value2 = ((highValue>>16) & 0xffff);
         value3 = (highValue & 0xffff);
         value4 = (value & 0xffff);
         patchAddr1 = (uint16_t *)&patchAddr[0];
         patchAddr2 = (uint16_t *)&patchAddr[1];
         patchAddr3 = (uint16_t *)&patchAddr[3];
         patchAddr4 = (uint16_t *)&patchAddr[4];
         break;
         }
      case 3:
      case 4:
         {
         value1 = ((highValue>>32) & 0xffff);
         value2 = (highValue & 0xffff);
         value3 = ((highValue>>16) & 0xffff);
         value4 = (value & 0xffff);
         patchAddr1 = (uint16_t *)&patchAddr[0];
         patchAddr2 = (uint16_t *)&patchAddr[1];
         patchAddr3 = (uint16_t *)&patchAddr[2];
         patchAddr4 = (uint16_t *)&patchAddr[4];
         break;
         }
      case 5:
      case 6:
         {
         value1 = ((highValue>>32) & 0xffff);
         value2 = (highValue & 0xffff);
         value3 = ((highValue>>16) & 0xffff);
         value4 = (value & 0xffff);
         patchAddr1 = (uint16_t *)&patchAddr[0];
         patchAddr2 = (uint16_t *)&patchAddr[1];
         patchAddr3 = (uint16_t *)&patchAddr[2];
         patchAddr4 = (uint16_t *)&patchAddr[3];
         break;
         }

      default:
         TR_ASSERT_FATAL(false, "unrecognized sequence number %d\n", seqNumber);
      }

   *patchAddr1 |= value1;
   *patchAddr2 |= value2;
   *patchAddr3 |= value3;
   *patchAddr4 |= value4;
   }

void
TR_PPC32RelocationTarget::storeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber)
   {
   storePointer(address, reloLocation);
   }

void
TR_PPCRelocationTarget::storeRelativeTarget(uintptr_t callTarget, uint8_t *reloLocation)
   {
   // reloLocation points at the start of the call offset, so just store the uint8_t * at reloLocation
   int32_t instruction = *((int32_t *)reloLocation);

   instruction &= (int32_t)0xFC000003;
   instruction |= (callTarget & 0x03FFFFFC);
   storeSigned32b(instruction, (uint8_t *)reloLocation);
   }

bool
TR_PPCRelocationTarget::isOrderedPairRelocation(TR_RelocationRecord *reloRecord, TR_RelocationTarget *reloTarget)
   {
   switch (reloRecord->type(reloTarget))
      {
      case TR_AbsoluteMethodAddressOrderedPair:
      case TR_ConstantPoolOrderedPair:
      case TR_MethodObject:
         return true;
      }

   return false;
   }

bool
TR_PPC32RelocationTarget::isOrderedPairRelocation(TR_RelocationRecord *reloRecord, TR_RelocationTarget *reloTarget)
   {
   switch (reloRecord->type(reloTarget))
      {
      case TR_AbsoluteMethodAddressOrderedPair:
      case TR_ConstantPoolOrderedPair:
      case TR_ClassAddress:
      case TR_ArbitraryClassAddress:
      case TR_MethodObject:
      case TR_ArrayCopyHelper:
      case TR_ArrayCopyToc:
      case TR_GlobalValue:
      case TR_RamMethodSequence:
      case TR_BodyInfoAddressLoad:
      case TR_DataAddress:
      case TR_DebugCounter:
         return true;
      }

   return false;
   }

bool TR_PPCRelocationTarget::useTrampoline(uint8_t * helperAddress, uint8_t *baseLocation)
   {
   return
      !TR::Compiler->target.cpu.isTargetWithinIFormBranchRange((intptrj_t)helperAddress, (intptrj_t)baseLocation) ||
      TR::Options::getCmdLineOptions()->getOption(TR_StressTrampolines);
   }

uint8_t *
TR_PPCRelocationTarget::arrayCopyHelperAddress(J9JavaVM *javaVM)
   {
   uintptr_t *funcdescrptr = (UDATA *)javaVM->memoryManagerFunctions->referenceArrayCopy;
#if defined(AIXPPC) || (defined(LINUXPPC64) && !defined(__LITTLE_ENDIAN__))
   uintptr_t value = funcdescrptr[0];
#else
   uintptr_t value = (UDATA)funcdescrptr;
#endif
   return (uint8_t *)value;
   }

void
TR_PPCRelocationTarget::flushCache(uint8_t *codeStart, unsigned long size)
   {
   ppcCodeSync((unsigned char *)codeStart, (unsigned int) size);
   }

void
TR_PPCRelocationTarget::patchMTIsolatedOffset(uint32_t offset, uint8_t *reloLocation)
   {
   /* apply constant to lis and ori instructions
      lis  r0, high_16 bits
      ori  r0, r0, low_16 bits
    */
   uint16_t highBits = offset >> 16;
   uint16_t lowBits = offset &0xffff;
   storeUnsigned16b(highBits, reloLocation+(TR::Compiler->target.cpu.isBigEndian()?2:0));
   storeUnsigned16b(lowBits, reloLocation+(TR::Compiler->target.cpu.isBigEndian()?6:4));
   }

void
TR_PPC64RelocationTarget::platformAddPICtoPatchPtrOnClassUnload(TR_OpaqueClassBlock *classKey, void *ptr)
   {
   /*
    * On POWER, PICs always treat the value guarded as 4 bytes wide.  Taking the 4 bytes, they set the least significant bit to 1.
    * Because the platform is (currently) big-endian, if we send them the full pointer, it will set 33rd bit one, which is not what we want.
    * Instead, we will create the PIC site on the low bits directly so that it flags the right bit.
    */
   uint8_t *sitePointer = static_cast<uint8_t *>(ptr) + (TR::Compiler->target.cpu.isBigEndian()?4:0);
   createClassUnloadPicSite(classKey, sitePointer, sizeof(int32_t), reloRuntime()->comp()->getMetadataAssumptionList());
   }

#ifdef ENABLE_SIMD_LIB
#ifndef LINUX
#include "logd2.i"
#else
extern "C" {
vector double __logd2 (vector double val)
{
   vector double ret = { log(val[0]), log(val[1]) };
   return ret;
}
}
#endif
#endif
