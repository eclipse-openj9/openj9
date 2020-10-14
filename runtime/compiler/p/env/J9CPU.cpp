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
#include "j9.h"
#include "j9port.h"

TR::CPU
J9::Power::CPU::detectRelocatable(OMRPortLibrary * const omrPortLib)
   {
   if (omrPortLib == NULL)
      return TR::CPU();

   OMRPORT_ACCESS_FROM_OMRPORT(omrPortLib);
   OMRProcessorDesc portableProcessorDescription;
   omrsysinfo_get_processor_description(&portableProcessorDescription);
   
   if (portableProcessorDescription.processor > OMR_PROCESSOR_PPC_P8)
      {
      portableProcessorDescription.processor = OMR_PROCESSOR_PPC_P8;
      portableProcessorDescription.physicalProcessor = OMR_PROCESSOR_PPC_P8;
      }

   return TR::CPU::customize(portableProcessorDescription);
   }

bool
J9::Power::CPU::isCompatible(const OMRProcessorDesc& processorDescription)
   {
   const OMRProcessorArchitecture& targetProcessor = self()->getProcessorDescription().processor;
   const OMRProcessorArchitecture& processor = processorDescription.processor;

   for (int i = 0; i < OMRPORT_SYSINFO_FEATURES_SIZE; i++)
      {
      if ((processorDescription.features[i] & self()->getProcessorDescription().features[i]) != processorDescription.features[i])
         return false;
      }

   // Backwards compatibility only applies to p4,p5,p6,p7 and onwards
   // Looks for equality otherwise
   if ((processor == OMR_PROCESSOR_PPC_GP
       || processor == OMR_PROCESSOR_PPC_GR 
       || processor == OMR_PROCESSOR_PPC_P6 
       || (processor >= OMR_PROCESSOR_PPC_P7 && processor <= OMR_PROCESSOR_PPC_LAST))
       && (targetProcessor == OMR_PROCESSOR_PPC_GP 
        || targetProcessor == OMR_PROCESSOR_PPC_GR 
        || targetProcessor == OMR_PROCESSOR_PPC_P6 
        || targetProcessor >= OMR_PROCESSOR_PPC_P7 && targetProcessor <= OMR_PROCESSOR_PPC_LAST))
      {
      return targetProcessor >= processor;
      }
   return targetProcessor == processor;
   }

void
J9::Power::CPU::enableFeatureMasks()
   {
   // Only enable the features that compiler currently uses
   const uint32_t utilizedFeatures [] = {OMR_FEATURE_PPC_HAS_ALTIVEC, OMR_FEATURE_PPC_HAS_DFP,
                                        OMR_FEATURE_PPC_HTM, OMR_FEATURE_PPC_HAS_VSX};


   memset(_supportedFeatureMasks.features, 0, OMRPORT_SYSINFO_FEATURES_SIZE*sizeof(uint32_t));
   OMRPORT_ACCESS_FROM_OMRPORT(TR::Compiler->omrPortLib);
   for (size_t i = 0; i < sizeof(utilizedFeatures)/sizeof(uint32_t); i++)
      {
      omrsysinfo_processor_set_feature(&_supportedFeatureMasks, utilizedFeatures[i], TRUE);
      }
   
   _isSupportedFeatureMasksEnabled = true;
   }
