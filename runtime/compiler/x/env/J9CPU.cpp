/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/CPU.hpp"
#include "env/VMJ9.h"
#include "x/runtime/X86Runtime.hpp"
#include "env/JitConfig.hpp"

// This is a workaround to avoid J9_PROJECT_SPECIFIC macros in x/env/OMRCPU.cpp
// Without this definition, we get an undefined symbol of JITConfig::instance() at runtime
TR::JitConfig * TR::JitConfig::instance() { return NULL; }

namespace J9
{

namespace X86
{

TR_X86CPUIDBuffer *
CPU::queryX86TargetCPUID()
   {
   static TR_X86CPUIDBuffer buf = { {'U','n','k','n','o','w','n','B','r','a','n','d'} };
   jitGetCPUID(&buf);
   return &buf;
   }

const char *
CPU::getProcessorVendorId()
   {
   return self()->getX86ProcessorVendorId();
   }

uint32_t
CPU::getProcessorSignature()
   {
   return self()->getX86ProcessorSignature();
   }

bool
CPU::testOSForSSESupport()
   {
   return true; // VM guarantees SSE/SSE2 are available
   }

bool
CPU::hasPopulationCountInstruction()
   {
   if ((self()->getX86ProcessorFeatureFlags2() & TR_POPCNT) != 0x00000000)
      return true;
   else
      return false;
   }

TR_ProcessorFeatureFlags
CPU::getProcessorFeatureFlags()
   {
   TR_ProcessorFeatureFlags processorFeatureFlags = { {self()->getX86ProcessorFeatureFlags(), self()->getX86ProcessorFeatureFlags2(), self()->getX86ProcessorFeatureFlags8()} };
   return processorFeatureFlags;
   }

bool
CPU::isCompatible(TR_Processor processorSignature, TR_ProcessorFeatureFlags processorFeatureFlags)
   {
   for (int i = 0; i < PROCESSOR_FEATURES_SIZE; i++)
      {
      // Check to see if the current processor contains all the features that code cache's processor has
      if ((processorFeatureFlags.featureFlags[i] & self()->getProcessorFeatureFlags().featureFlags[i]) != processorFeatureFlags.featureFlags[i])
         return false;
      }
   return true;
   }

}
}
