/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/CPU.hpp"
#include "env/VMJ9.h"
#include "x/runtime/X86Runtime.hpp"
#include "codegen/CodeGenerator.hpp"
#if defined(J9VM_OPT_JITSERVER)
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "runtime/JITClientSession.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */

TR::CPU
J9::X86::CPU::detectRelocatable(OMRPortLibrary * const omrPortLib)
   {
   // For our portable processor description we allow only features present in the Sandybridge Architecture
   const uint32_t customFeatures [] = {OMR_FEATURE_X86_FPU, OMR_FEATURE_X86_CX8, OMR_FEATURE_X86_CMOV,
                                       OMR_FEATURE_X86_MMX, OMR_FEATURE_X86_SSE, OMR_FEATURE_X86_SSE2,
                                       OMR_FEATURE_X86_SSSE3, OMR_FEATURE_X86_SSE4_1, OMR_FEATURE_X86_POPCNT,
                                       OMR_FEATURE_X86_SSE3, OMR_FEATURE_X86_AESNI, OMR_FEATURE_X86_AVX
                                      };

   OMRPORT_ACCESS_FROM_OMRPORT(omrPortLib);
   OMRProcessorDesc customProcessorDescription;
   memset(customProcessorDescription.features, 0, OMRPORT_SYSINFO_FEATURES_SIZE*sizeof(uint32_t));
   for (size_t i = 0; i < sizeof(customFeatures)/sizeof(uint32_t); i++)
      {
      omrsysinfo_processor_set_feature(&customProcessorDescription, customFeatures[i], TRUE);
      }

   OMRProcessorDesc hostProcessorDescription;
   omrsysinfo_get_processor_description(&hostProcessorDescription);

   // Pick the older processor between our hand-picked processor and host processor to be the actual portable processor
   OMRProcessorDesc portableProcessorDescription;
   portableProcessorDescription.processor = OMR_PROCESSOR_X86_FIRST;
   portableProcessorDescription.physicalProcessor = portableProcessorDescription.processor;
   memset(portableProcessorDescription.features, 0, OMRPORT_SYSINFO_FEATURES_SIZE*sizeof(uint32_t));

   for (size_t i = 0; i < OMRPORT_SYSINFO_FEATURES_SIZE; i++)
      {
      portableProcessorDescription.features[i] = hostProcessorDescription.features[i] & customProcessorDescription.features[i];
      }

   return TR::CPU::customize(portableProcessorDescription);
   }

TR::CPU
J9::X86::CPU::detect(OMRPortLibrary * const omrPortLib)
   {
   if (omrPortLib == NULL)
      return TR::CPU();

   TR::CPU::enableFeatureMasks();
   return OMR::X86::CPU::detect(omrPortLib);
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
J9::X86::CPU::isCompatible(const OMRProcessorDesc& processorDescription)
   {
   for (int i = 0; i < OMRPORT_SYSINFO_FEATURES_SIZE; i++)
      {
      // Check to see if the current processor contains all the features that code cache's processor has
      if ((processorDescription.features[i] & _processorDescription.features[i]) != processorDescription.features[i])
         return false;
      }
   return true;
   }
