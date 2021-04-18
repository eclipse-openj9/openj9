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

#pragma csect(CODE,"J9ZJ9CPU#C")
#pragma csect(STATIC,"J9ZJ9CPU#S")
#pragma csect(TEST,"J9ZJ9CPU#T")


#include <ctype.h>
#include <string.h>
#include "control/Options.hpp"
#include "control/CompilationRuntime.hpp"
#include "env/CompilerEnv.hpp"
#include "env/CPU.hpp"
#include "infra/Assert.hpp"
#include "j9.h"
#include "j9port.h"
#include "j9jitnls.h"

#if defined(J9ZOS390) || defined (J9ZTPF)
#include <sys/utsname.h>
#endif /* defined(J9ZOS390) || defined (J9ZTPF) */

#include "control/Options_inlines.hpp"

extern J9JITConfig * jitConfig;

TR::CPU
J9::Z::CPU::detectRelocatable(OMRPortLibrary * const omrPortLib)
   {
   OMRPORT_ACCESS_FROM_OMRPORT(omrPortLib);
   OMRProcessorDesc processorDescription;
   omrsysinfo_get_processor_description(&processorDescription);

   if (processorDescription.processor > OMR_PROCESSOR_S390_Z10)
      { 
      processorDescription.processor = OMR_PROCESSOR_S390_Z10;
      processorDescription.physicalProcessor = OMR_PROCESSOR_S390_Z10;
      }

   return TR::CPU::customize(processorDescription);
   }

TR::CPU
J9::Z::CPU::customize(OMRProcessorDesc processorDescription)
   {
   OMRPORT_ACCESS_FROM_OMRPORT(TR::Compiler->omrPortLib);
   if (processorDescription.processor < OMR_PROCESSOR_S390_Z10)
      {
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_DFP, FALSE);
      }

   if (processorDescription.processor < OMR_PROCESSOR_S390_Z196)
      {
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_HIGH_WORD, FALSE);
      }

   if (processorDescription.processor < OMR_PROCESSOR_S390_ZEC12)
      {
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_TE, FALSE);
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_RI, FALSE);
      }

   if (processorDescription.processor < OMR_PROCESSOR_S390_Z13)
      {
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_VECTOR_FACILITY, FALSE);
      }

   if (processorDescription.processor < OMR_PROCESSOR_S390_Z14)
      {
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_2, FALSE);
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL, FALSE);
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_1, FALSE);
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_GUARDED_STORAGE, FALSE);
      }

   if (processorDescription.processor < OMR_PROCESSOR_S390_Z15)
      {
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_3, FALSE);
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_2, FALSE);
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY, FALSE);
      }

   // This variable is used internally by the j9sysinfo macros below and cannot be folded away
   J9PortLibrary* privatePortLibrary = TR::Compiler->portLib;

#if defined(LINUX)
   if (TRUE == omrsysinfo_processor_has_feature(&processorDescription, OMR_FEATURE_S390_RI))
      {
      if (0 != j9ri_enableRISupport())
         {
         omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_RI, FALSE);
         }
      }
#endif

   if (TRUE == omrsysinfo_processor_has_feature(&processorDescription, OMR_FEATURE_S390_GUARDED_STORAGE))
      {
      if (TR::Compiler->javaVM->memoryManagerFunctions->j9gc_software_read_barrier_enabled(TR::Compiler->javaVM))
         {
         omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_GUARDED_STORAGE, FALSE);
         }
      }

   if (_isSupportedFeatureMasksEnabled)
      {
      // mask out any cpu features that the compiler doesn't care about
      for (size_t i = 0; i < OMRPORT_SYSINFO_FEATURES_SIZE; i++)
         {
         processorDescription.features[i] &= _supportedFeatureMasks.features[i];
         }
      }

   return TR::CPU(processorDescription);
   }


void
J9::Z::CPU::enableFeatureMasks()
   {
   // Only enable the features that compiler currently uses
   const uint32_t utilizedFeatures [] = {OMR_FEATURE_S390_DFP, OMR_FEATURE_S390_TE, OMR_FEATURE_S390_FPE,
                                         OMR_FEATURE_S390_RI, OMR_FEATURE_S390_VECTOR_FACILITY, OMR_FEATURE_S390_HIGH_WORD,
                                         OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_2,
                                         OMR_FEATURE_S390_GUARDED_STORAGE, OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL,
                                         OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_1,
                                         OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_2,
                                         OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_3,
                                         OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY};

   memset(_supportedFeatureMasks.features, 0, OMRPORT_SYSINFO_FEATURES_SIZE*sizeof(uint32_t));
   OMRPORT_ACCESS_FROM_OMRPORT(TR::Compiler->omrPortLib);
   for (size_t i = 0; i < sizeof(utilizedFeatures)/sizeof(uint32_t); i++)
      {
      omrsysinfo_processor_set_feature(&_supportedFeatureMasks, utilizedFeatures[i], TRUE);
      }
   _isSupportedFeatureMasksEnabled = true;
   }

bool
J9::Z::CPU::isCompatible(const OMRProcessorDesc& processorDescription)
   {
   if (!self()->isAtLeast(processorDescription.processor))
      {
      return false;
      }
   for (int i = 0; i < OMRPORT_SYSINFO_FEATURES_SIZE; i++)
      {
      // Check to see if the current processor contains all the features that code cache's processor has
      if ((processorDescription.features[i] & self()->getProcessorDescription().features[i]) != processorDescription.features[i])
         return false;
      }
   return true;
   }
