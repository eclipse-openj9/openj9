/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/CPU.hpp"
#include "env/VMJ9.h"

namespace J9
{

namespace X86
{

const char *
CPU::getX86ProcessorVendorId(TR::Compilation *comp)
   {
   return comp->fej9()->getX86ProcessorVendorId();
   }

uint32_t
CPU::getX86ProcessorSignature(TR::Compilation *comp)
   {
   return comp->fej9()->getX86ProcessorSignature();
   }

uint32_t
CPU::getX86ProcessorFeatureFlags(TR::Compilation *comp)
   {
   return comp->fej9()->getX86ProcessorFeatureFlags();
   }

uint32_t
CPU::getX86ProcessorFeatureFlags2(TR::Compilation *comp)
   {
   return comp->fej9()->getX86ProcessorFeatureFlags2();
   }

uint32_t
CPU::getX86ProcessorFeatureFlags8(TR::Compilation *comp)
   {
   return comp->fej9()->getX86ProcessorFeatureFlags8();
   }

bool
CPU::testOSForSSESupport(TR::Compilation *comp)
   {
   return comp->fej9()->testOSForSSESupport();
   }

}
}
