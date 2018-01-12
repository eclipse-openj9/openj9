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

#ifndef RELOCATION_TARGET_INCL
#define RELOCATION_TARGET_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "runtime/RelocationRuntime.hpp"

class TR_OpaqueClassBlock;
class TR_RelocationRecord;
class TR_RelocationRuntimeLogger;

// TR_RelocationTarget defines how a platform target implements the individual steps of processing
//    relocation records.
// This is intended to be a base class that should not be itself instantiated
class TR_RelocationTarget
   {
   public:
      TR_ALLOC(TR_Memory::Relocation)
      void * operator new(size_t, J9JITConfig *);
      TR_RelocationTarget(TR_RelocationRuntime *reloRuntime)
         {
         _reloRuntime = reloRuntime;
         }

      TR_RelocationRuntime *reloRuntime()                                   { return _reloRuntime; }
      TR_RelocationRuntimeLogger *reloLogger()                              { return _reloRuntime->reloLogger(); }

      virtual void flushCache(uint8_t *codeStart, unsigned long size)       {} // default impl is empty

      virtual void preRelocationsAppliedEvent()                             {} // default impl is empty

      virtual bool isOrderedPairRelocation(TR_RelocationRecord *reloRecord, TR_RelocationTarget *reloTarget);

      virtual uintptrj_t loadRelocationRecordValue(uintptrj_t *address)     { return *address; }
      virtual void storeRelocationRecordValue(uintptrj_t value, uintptrj_t *address) { *address = value; }

      virtual uint8_t loadUnsigned8b(uint8_t *address)                      { return *address; }
      virtual void storeUnsigned8b(uint8_t value, uint8_t *address)	    { *address = value; }

      virtual int8_t loadSigned8b(uint8_t *address)                         { return *(int8_t *) address; }
      virtual void storeSigned8b(int8_t value, uint8_t *address)            { *(int8_t *)address = value; }

      virtual uint16_t loadUnsigned16b(uint8_t *address)                    { return *(uint16_t *) address; }
      virtual void storeUnsigned16b(uint16_t value, uint8_t *address)        { *(uint16_t *)address = value; }

      virtual int16_t loadSigned16b(uint8_t *address)                       { return *(int16_t *) address; }
      virtual void storeSigned16b(int16_t value, uint8_t *address)          { *(int16_t *)address = value; }

      virtual uint32_t loadUnsigned32b(uint8_t *address)                    { return *(uint32_t *) address; }
      virtual void storeUnsigned32b(uint32_t value, uint8_t *address)       { *(uint32_t *)address = value; }

      virtual int32_t loadSigned32b(uint8_t *address)                       { return *(int32_t *) address; }
      virtual void storeSigned32b(int32_t value, uint8_t *address)          { *(int32_t *)address = value; }

      virtual uint8_t *loadPointer(uint8_t *address)                        { return *(uint8_t**) address; }
      virtual void storePointer(uint8_t *value, uint8_t *address)           { *(uint8_t**)address = value; }

      virtual bool useTrampoline(uint8_t * helperAddress, uint8_t *baseLocation) { return false; }

      // The following functions should be overridden by subclasses for specific targets
      virtual uint8_t *eipBaseForCallOffset(uint8_t *reloLocation);

      virtual uint8_t *loadCallTarget(uint8_t *reloLocation);
      virtual void storeCallTarget(uintptr_t callTarget, uint8_t *reloLocation);
      virtual void storeRelativeTarget(uintptr_t callTarget, uint8_t *reloLocation);

      virtual uint8_t *loadBranchOffset(uint8_t *reloLocation);
      virtual void storeBranchOffset(uint8_t *branchOffset, uint8_t *reloLocation);

      virtual uint8_t *loadAddress(uint8_t *reloLocation);
      virtual void storeAddress(uint8_t *address, uint8_t *reloLocation);

      virtual void storeAddressRAM(uint8_t *address, uint8_t *reloLocation)
         {
         storeAddress(address, reloLocation);
         }

      virtual uint8_t *loadAddressSequence(uint8_t *reloLocation);
      virtual void storeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber);

         
      virtual void storeRelativeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber) 
         {
         storeAddressSequence(address, reloLocation, seqNumber);
         }


      virtual uint8_t *loadClassAddressForHeader(uint8_t *reloLocation);
      virtual void storeClassAddressForHeader(uint8_t *address, uint8_t *reloLocation);

      virtual uint32_t loadCPIndex(uint8_t *reloLocation);
      virtual uintptr_t loadThunkCPIndex(uint8_t *reloLocation);


      virtual uint8_t *eipBaseForCallOffset(uint8_t *reloLocationHigh, uint8_t *reloLocationLow);

      virtual uint8_t *loadCallTarget(uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
      virtual void storeCallTarget(uint8_t *callTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);

      virtual uint8_t *loadBranchOffset(uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
      virtual void storeBranchOffset(uint8_t *branchOffset, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);

      virtual uint8_t *loadAddress(uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
      virtual void storeAddress(uint8_t *address, uint8_t *reloLocationHigh, uint8_t *reloLocationLow, uint32_t seqNumber);

      virtual uint8_t *loadClassAddressForHeader(uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
      virtual void storeClassAddressForHeader(uint8_t *address, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);

      virtual uint32_t loadCPIndex(uint8_t *reloLocationHigh, uint8_t *reloLocationLow);

      virtual void performThunkRelocation(uint8_t *thunkAddress, uintptr_t vmHelper);
      virtual uint8_t *arrayCopyHelperAddress(J9JavaVM *javaVM);

      virtual void patchNonVolatileFieldMemoryFence(J9ROMFieldShape* resolvedField, UDATA cpAddr, U_8 descriptorByte, U_8 *instructionAddress, U_8 *snippetStartAddress, J9JavaVM *javaVM);

      virtual void patchMTIsolatedOffset(uint32_t offset, uint8_t *reloLocation);

      /**
       * @brief Adds a PIC guard that will invalidate a pointer when the class it is dependant on is unloaded.  Marks metadata as having class unload assumptions.
       *
       * @param classKey The class upon which the pointer to be updated is dependant.
       * @param ptr The address to be updated.
       */
      void addPICtoPatchPtrOnClassUnload(TR_OpaqueClassBlock *classKey, void *ptr);
   private:
      virtual void platformAddPICtoPatchPtrOnClassUnload(TR_OpaqueClassBlock *classKey, void *ptr);
      TR_RelocationRuntime *_reloRuntime;
   };

#endif   // RELOCATION_TARGET_INCL
