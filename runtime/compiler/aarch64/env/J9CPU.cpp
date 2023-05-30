/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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

#include "env/CompilerEnv.hpp"
#include "env/CPU.hpp"

TR::CPU
J9::ARM64::CPU::detectRelocatable(OMRPortLibrary * const omrPortLib)
   {
   if (omrPortLib == NULL)
      return TR::CPU();

   OMRPORT_ACCESS_FROM_OMRPORT(omrPortLib);
   OMRProcessorDesc portableProcessorDescription;
   omrsysinfo_get_processor_description(&portableProcessorDescription);

   return TR::CPU::customize(portableProcessorDescription);
   }

bool
J9::ARM64::CPU::isCompatible(const OMRProcessorDesc& processorDescription)
   {
   for (int i = 0; i < OMRPORT_SYSINFO_FEATURES_SIZE; i++)
      {
      if ((processorDescription.features[i] & self()->getProcessorDescription().features[i]) != processorDescription.features[i])
         return false;
      }
   return true;
   }

void
J9::ARM64::CPU::enableFeatureMasks()
   {
   // Only enable the features that compiler currently uses
   const uint32_t utilizedFeatures [] = {OMR_FEATURE_ARM64_FP, OMR_FEATURE_ARM64_ASIMD, OMR_FEATURE_ARM64_CRC32, OMR_FEATURE_ARM64_LSE};

   memset(_supportedFeatureMasks.features, 0, OMRPORT_SYSINFO_FEATURES_SIZE*sizeof(uint32_t));
   OMRPORT_ACCESS_FROM_OMRPORT(TR::Compiler->omrPortLib);
   for (size_t i = 0; i < sizeof(utilizedFeatures)/sizeof(uint32_t); i++)
      {
      omrsysinfo_processor_set_feature(&_supportedFeatureMasks, utilizedFeatures[i], TRUE);
      }

   _isSupportedFeatureMasksEnabled = true;
   }
