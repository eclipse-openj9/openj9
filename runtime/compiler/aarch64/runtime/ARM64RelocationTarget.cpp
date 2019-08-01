/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
   TR_UNIMPLEMENTED();
   return NULL;
   }

void
TR_ARM64RelocationTarget::storeCallTarget(uintptr_t callTarget, uint8_t *reloLocation)
   {
   TR_UNIMPLEMENTED();
   }

uint32_t
TR_ARM64RelocationTarget::loadCPIndex(uint8_t *reloLocation)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

uintptr_t
TR_ARM64RelocationTarget::loadThunkCPIndex(uint8_t *reloLocation)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

void
TR_ARM64RelocationTarget::performThunkRelocation(uint8_t *thunkBase, uintptr_t vmHelper)
   {
   TR_UNIMPLEMENTED();
   }

uint8_t *
TR_ARM64RelocationTarget::loadAddressSequence(uint8_t *reloLocation)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

void
TR_ARM64RelocationTarget::storeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber)
   {
   TR_UNIMPLEMENTED();
   }

void
TR_ARM64RelocationTarget::storeRelativeTarget(uintptr_t callTarget, uint8_t *reloLocation)
   {
   TR_UNIMPLEMENTED();
   }

bool
TR_ARM64RelocationTarget::useTrampoline(uint8_t * helperAddress, uint8_t *baseLocation)
   {
   TR_UNIMPLEMENTED();
   return false;
   }

uint8_t *
TR_ARM64RelocationTarget::arrayCopyHelperAddress(J9JavaVM *javaVM)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

void
TR_ARM64RelocationTarget::flushCache(uint8_t *codeStart, unsigned long size)
   {
   arm64CodeSync((unsigned char *)codeStart, (unsigned int)size);
   }
