/*******************************************************************************
 * Copyright IBM Corp. and others 2018
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "env/CompilerEnv.hpp"
#include "env/CPU.hpp"

bool
J9::ARM::CPU::isCompatible(const OMRProcessorDesc& processorDescription)
   {
   return self()->getProcessorDescription().processor == processorDescription.processor;
   }

OMRProcessorDesc
J9::ARM::CPU::getProcessorDescription()
   {
   static bool initialized = false;
   if (!initialized)
      {
      memset(_processorDescription.features, 0, OMRPORT_SYSINFO_FEATURES_SIZE*sizeof(uint32_t));
      switch (self()->id())
         {
         case TR_DefaultARMProcessor:
            _processorDescription.processor = OMR_PROCESSOR_ARM_UNKNOWN;
            break;
         case TR_ARMv6:
            _processorDescription.processor = OMR_PROCESSOR_ARM_V6;
            break;
         case TR_ARMv7:
            _processorDescription.processor = OMR_PROCESSOR_ARM_V7;
            break;
         default:
            TR_ASSERT_FATAL(false, "Invalid ARM64 Processor ID");
         }
      _processorDescription.physicalProcessor = _processorDescription.processor;
      initialized = true;
      }
   return _processorDescription;
   }


