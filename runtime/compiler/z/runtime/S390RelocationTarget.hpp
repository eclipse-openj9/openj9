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

#ifndef S390RELOCATION_TARGET_INCL
#define S390RELOCATION_TARGET_INCL

#include <stdint.h>
#include "j9.h"
#include "env/TRMemory.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationTarget.hpp"

namespace TR { class Compilation; }

// TR_RelocationTarget defines how a platform target implements the individual steps of processing
//    relocation records.
// This is intended to be a base class that should not be itself instantiated

class TR_S390RelocationTarget : public TR_RelocationTarget
   {
   public:
      TR_ALLOC(TR_Memory::Relocation)
      void * operator new(size_t, J9JITConfig *);
      TR_S390RelocationTarget(TR_RelocationRuntime *reloRuntime) : TR_RelocationTarget(reloRuntime) {}

      virtual uint8_t *eipBaseForCallOffset(uint8_t *reloLocation);
      virtual void storeCallTarget(uint8_t *callTarget, uint8_t *reloLocation);
      virtual void storeRelativeTarget(uintptr_t callTarget, uint8_t *reloLocation);
      virtual uint8_t *loadAddressSequence(uint8_t *reloLocation);
      virtual void storeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber);
      virtual uint32_t loadCPIndex(uint8_t *reloLocation);
      virtual uintptr_t loadThunkCPIndex(uint8_t *reloLocation);

      virtual void performThunkRelocation(uint8_t *thunkAddress, uintptr_t vmHelper);

      virtual bool useTrampoline(uint8_t * helperAddress, uint8_t *baseLocation);
      virtual uint8_t *arrayCopyHelperAddress(J9JavaVM *javaVM);
      virtual void patchMTIsolatedOffset(uint32_t offset, uint8_t *reloLocation);
   };

#endif   // S390RELOCATION_TARGET_INCL
