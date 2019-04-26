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

#include "runtime/RelocationTarget.hpp"

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
#include "codegen/PicHelpers.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/jittypes.h"
#include "runtime/J9CodeCache.hpp"
#include "runtime/J9Runtime.hpp"
#include "runtime/MethodMetaData.h"
#include "runtime/RelocationRecord.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationRuntimeLogger.hpp"

bool
TR_RelocationTarget::isOrderedPairRelocation(TR_RelocationRecord *reloRecord, TR_RelocationTarget *reloTarget)
   {
   switch (reloRecord->type(reloTarget))
      {
      case TR_AbsoluteMethodAddressOrderedPair :
      case TR_ConstantPoolOrderedPair :
         return true;
      default:
      	return false;
      }

   return false;
   }

// These functions must be implemented by subclasses for specific targets

uint8_t *
TR_RelocationTarget::eipBaseForCallOffset(uint8_t *reloLocation)
   {
   TR_ASSERT(0, "Error: eipBaseForCallOffset not implemented in relocation target base class");
   return NULL;
   }


uint8_t *
TR_RelocationTarget::loadCallTarget(uint8_t *reloLocation)
   {
   return loadPointer(reloLocation);
   }

void
TR_RelocationTarget:: storeCallTarget(uintptr_t callTarget, uint8_t *reloLocation)
   {
   TR_ASSERT(0, "Error: storeCallTarget not implemented in relocation target base class");
   }

void
TR_RelocationTarget:: storeRelativeTarget(uintptr_t callTarget, uint8_t *reloLocation)
   {
   TR_ASSERT(0, "Error: storeRelativeTarget not implemented in relocation target base class");
   }

uint8_t *
TR_RelocationTarget::loadBranchOffset(uint8_t *reloLocation)
   {
   // reloLocation points at the start of the branch offset, so just need to dereference as uint8_t *
   return loadPointer(reloLocation);
   }

void
TR_RelocationTarget::storeBranchOffset(uint8_t *branchOffset, uint8_t *reloLocation)
   {
   storePointer(branchOffset, reloLocation);
   }


uint8_t *
TR_RelocationTarget::loadAddress(uint8_t *reloLocation)
   {
   return loadPointer(reloLocation);
   }

void
TR_RelocationTarget::storeAddress(uint8_t *address, uint8_t *reloLocation)
   {
   // reloLocation points at the start of the address, so just store the uint8_t * at reloLocation
   storePointer(address, reloLocation);
   }

uint8_t *
TR_RelocationTarget::loadAddressSequence(uint8_t *reloLocation)
   {
   TR_ASSERT(0, "Error: loadAddressSequence not implemented in relocation target base class");
   return NULL;
   }

void
TR_RelocationTarget::storeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber)
   {
   TR_ASSERT(0, "Error: storeAddressSequence not implemented in relocation target base class");
   }


uint8_t *
TR_RelocationTarget::loadClassAddressForHeader(uint8_t *reloLocation)
   {
   // reloLocation points at the start of the address, so just need to dereference as uint8_t *
#ifdef OMR_GC_COMPRESSED_POINTERS
   return (uint8_t *) (uintptr_t) loadUnsigned32b(reloLocation);
#else
   return (uint8_t *) loadPointer(reloLocation);
#endif
   }

void
TR_RelocationTarget::storeClassAddressForHeader(uint8_t *clazz, uint8_t *reloLocation)
   {
   // reloLocation points at the start of the address, so just store the uint8_t * at reloLocation
#ifdef OMR_GC_COMPRESSED_POINTERS
   uintptr_t clazzPtr = (uintptr_t)clazz;
   storeUnsigned32b((uint32_t)clazzPtr, reloLocation);
#else
   storePointer(clazz, reloLocation);
#endif
   }

uint32_t
TR_RelocationTarget::loadCPIndex(uint8_t *reloLocation)
   {
   TR_ASSERT(0, "Error: loadCPIndex not implemented in relocation target base class");
   return 0;
   }

uintptrj_t
TR_RelocationTarget::loadThunkCPIndex(uint8_t *reloLocation)
   {
   TR_ASSERT(0, "Error: loadThunkCPIndex not implemented in relocation target base class");
   return 0;
   }

uint8_t *
TR_RelocationTarget::eipBaseForCallOffset(uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "Error: eipBaseForCallOffset not implemented in relocation target base class");
   return NULL;
   }


uint8_t *
TR_RelocationTarget::loadCallTarget(uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "Error: loadCallTarget not implemented in relocation target base class");
   return NULL;
   }

void
TR_RelocationTarget::storeCallTarget(uint8_t *callTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "Error: storeCallTarget not implemented in relocation target base class");
   }


uint8_t *
TR_RelocationTarget::loadBranchOffset(uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "Error: loadBranchOffset not implemented in relocation target base class");
   return NULL;
   }

void
TR_RelocationTarget::storeBranchOffset(uint8_t *branchOffset, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "Error: storeBranchOffset not implemented in relocation target base class");
   }


uint8_t *
TR_RelocationTarget::loadAddress(uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "Error: loadAddress not implemented in relocation target base class");
   return NULL;
   }

void
TR_RelocationTarget::storeAddress(uint8_t *address, uint8_t *reloLocationHigh, uint8_t *reloLocationLow, uint32_t seqNumber)
   {
   TR_ASSERT(0, "Error: storeAddress not implemented in relocation target base class");
   }


uint8_t *
TR_RelocationTarget::loadClassAddressForHeader(uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "Error: loadClassAddressForHeader not implemented in relocation target base class");
   return NULL;
   }

void
TR_RelocationTarget::storeClassAddressForHeader(uint8_t *address, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "Error: storeClassAddressForHeader not implemented in relocation target base class");
   }

uint32_t
TR_RelocationTarget::loadCPIndex(uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "Error: loadCPIndex not implemented in relocation target base class");
   return 0;
   }

void
TR_RelocationTarget::performThunkRelocation(uint8_t *thunkAddress, uintptr_t vmHelper)
   {
   TR_ASSERT(0, "Error: performThunkRelocation not implemented in relocation target base class");
   }

uint8_t *
TR_RelocationTarget::arrayCopyHelperAddress(J9JavaVM *javaVM)
   {
   TR_ASSERT(0, "Error: arrayCopyHelperAddress not implemented in relocation target base class");
   return 0;
   }

void
TR_RelocationTarget::patchNonVolatileFieldMemoryFence(J9ROMFieldShape* resolvedField, UDATA cpAddr, U_8 descriptorByte, U_8 *instructionAddress, U_8 *snippetStartAddress, J9JavaVM *javaVM)
   {
   TR_ASSERT(0, "Error: patchNonVolatileFieldMemoryFence not implemented in relocation target base class");
   }

void
TR_RelocationTarget::patchMTIsolatedOffset(uint32_t offset, uint8_t *reloLocation)
   {
   TR_ASSERT(0, "Error: patchMTIsolatedOffset not implemented in relocation target base class");
   }

void
TR_RelocationTarget::addPICtoPatchPtrOnClassUnload(TR_OpaqueClassBlock *classKey, void *ptr)
   {
   platformAddPICtoPatchPtrOnClassUnload(classKey, ptr);
   _reloRuntime->comp()->setHasClassUnloadAssumptions();
   }

void
TR_RelocationTarget::platformAddPICtoPatchPtrOnClassUnload(TR_OpaqueClassBlock *classKey, void *ptr)
   {
   createClassUnloadPicSite(classKey, ptr, sizeof(uintptr_t), _reloRuntime->comp()->getMetadataAssumptionList());
   }
