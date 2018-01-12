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

#ifndef PPCRELOCATION_TARGET_INCL
#define PPCRELOCATION_TARGET_INCL

#include "runtime/RelocationRecord.hpp"
#include "runtime/RelocationTarget.hpp"

// TR_RelocationTarget defines how a platform target implements the individual steps of processing
//    relocation records.
// This is intended to be a base class that should not be itself instantiated

class TR_PPCRelocationTarget : public TR_RelocationTarget
   {
   public:
      TR_ALLOC(TR_Memory::Relocation)
      void * operator new(size_t, J9JITConfig *);
      TR_PPCRelocationTarget(TR_RelocationRuntime *reloRuntime) : TR_RelocationTarget(reloRuntime) {}

      virtual uint8_t *eipBaseForCallOffset(uint8_t *reloLocation);
      virtual void storeCallTarget(uintptr_t callTarget, uint8_t *reloLocation);
      virtual void storeRelativeTarget(uintptr_t callTarget, uint8_t *reloLocation);
      virtual uint32_t loadCPIndex(uint8_t *reloLocation);
      virtual uintptr_t loadThunkCPIndex(uint8_t *reloLocation);

      virtual void performThunkRelocation(uint8_t *thunkAddress, uintptr_t vmHelper);

      virtual uint8_t *loadAddress(uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
      virtual void storeAddress(uint8_t *address, uint8_t *reloLocationHigh, uint8_t *reloLocationLow, uint32_t seqNumber);

      virtual uint8_t *loadAddressSequence(uint8_t *reloLocation);
      virtual void storeAddressSequence(uint8_t *computedAddress, uint8_t *reloLocation, uint32_t seqNumber);

      virtual bool isOrderedPairRelocation(TR_RelocationRecord *reloRecord, TR_RelocationTarget *reloTarget);

      virtual bool useTrampoline(uint8_t * helperAddress, uint8_t *baseLocation);

      virtual uint8_t *arrayCopyHelperAddress(J9JavaVM *javaVM);
      virtual void flushCache(uint8_t *codeStart, unsigned long size);

      virtual void patchMTIsolatedOffset(uint32_t offset, uint8_t *reloLocation);

   };

class TR_PPC64RelocationTarget : public TR_PPCRelocationTarget
   {
   public:
      TR_ALLOC(TR_Memory::Relocation)
      void * operator new(size_t, J9JITConfig *);
      TR_PPC64RelocationTarget(TR_RelocationRuntime *reloRuntime) : TR_PPCRelocationTarget(reloRuntime) {}

      virtual void storeAddressSequence(uint8_t *computedAddress, uint8_t *reloLocation, uint32_t seqNumber);

      virtual void storeAddressRAM(uint8_t *address, uint8_t *reloLocation)
         {
         storeAddressSequence(address, reloLocation, fixedSequence1);
         }

   private:
         virtual void platformAddPICtoPatchPtrOnClassUnload(TR_OpaqueClassBlock *classKey, void *ptr);

   };

class TR_PPC32RelocationTarget : public TR_PPCRelocationTarget
   {
   public:
      TR_ALLOC(TR_Memory::Relocation)
      void * operator new(size_t, J9JITConfig *);
      TR_PPC32RelocationTarget(TR_RelocationRuntime *reloRuntime) : TR_PPCRelocationTarget(reloRuntime) {}

      virtual void storeAddressSequence(uint8_t *computedAddress, uint8_t *reloLocation, uint32_t seqNumber);

      virtual bool isOrderedPairRelocation(TR_RelocationRecord *reloRecord, TR_RelocationTarget *reloTarget);

      virtual void storeRelativeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber);

      virtual void storeAddressRAM(uint8_t *address, uint8_t *reloLocation)
         {
         storeRelativeAddressSequence(address, reloLocation, fixedSequence1);
         }
   };

#endif   // PPCRELOCATION_TARGET_INCL
