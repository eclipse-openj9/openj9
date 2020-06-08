/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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
#if defined(J9VM_OPT_JITSERVER)
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "runtime/JITClientSession.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */

// This is a workaround to avoid J9_PROJECT_SPECIFIC macros in x/env/OMRCPU.cpp
// Without this definition, we get an undefined symbol of JITConfig::instance() at runtime
TR::JitConfig * TR::JitConfig::instance() { return NULL; }

TR_X86CPUIDBuffer *
J9::X86::CPU::queryX86TargetCPUID()
   {
   static TR_X86CPUIDBuffer buf = { {'U','n','k','n','o','w','n','B','r','a','n','d'} };
   jitGetCPUID(&buf);
   return &buf;
   }

const char *
J9::X86::CPU::getProcessorVendorId()
   {
   return self()->getX86ProcessorVendorId();
   }

uint32_t
J9::X86::CPU::getProcessorSignature()
   {
   return self()->getX86ProcessorSignature();
   }

bool
J9::X86::CPU::hasPopulationCountInstruction()
   {
   if ((self()->getX86ProcessorFeatureFlags2() & TR_POPCNT) != 0x00000000)
      return true;
   else
      return false;
   }

bool
J9::X86::CPU::isCompatible(const OMRProcessorDesc& processorDescription)
   {
   for (int i = 0; i < OMRPORT_SYSINFO_FEATURES_SIZE; i++)
      {
      // Check to see if the current processor contains all the features that code cache's processor has
      if ((processorDescription.features[i] & self()->getProcessorDescription().features[i]) != processorDescription.features[i])
         return false;
      }
   return true;
   }

OMRProcessorDesc
J9::X86::CPU::getProcessorDescription()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return vmInfo->_processorDescription;
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return _processorDescription;
   }

uint32_t
J9::X86::CPU::getX86ProcessorFeatureFlags()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return vmInfo->_processorDescription.features[0];
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return self()->queryX86TargetCPUID()->_featureFlags;
   }

uint32_t
J9::X86::CPU::getX86ProcessorFeatureFlags2()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return vmInfo->_processorDescription.features[1];
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return self()->queryX86TargetCPUID()->_featureFlags2;
   }

uint32_t
J9::X86::CPU::getX86ProcessorFeatureFlags8()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (auto stream = TR::CompilationInfo::getStream())
      {
      auto *vmInfo = TR::compInfoPT->getClientData()->getOrCacheVMInfo(stream);
      return vmInfo->_processorDescription.features[3];
      }
#endif /* defined(J9VM_OPT_JITSERVER) */
   return self()->queryX86TargetCPUID()->_featureFlags8;
   }



