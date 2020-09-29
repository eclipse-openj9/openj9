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

bool
J9::Power::CPU::isCompatible(const OMRProcessorDesc& processorDescription)
   {
   OMRProcessorArchitecture targetProcessor = self()->getProcessorDescription().processor;
   OMRProcessorArchitecture processor = processorDescription.processor;
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
J9::Power::CPU::applyUserOptions()
   {
   // P10 support is not yet well-tested, so it's currently gated behind an environment
   // variable to prevent it from being used by accident by users who use old versions of
   // OMR once P10 chips become available.
   if (_processorDescription.processor == OMR_PROCESSOR_PPC_P10)
      {
      static bool enableP10 = feGetEnv("TR_EnableExperimentalPower10Support");
      if (!enableP10)
         {
         _processorDescription.processor = OMR_PROCESSOR_PPC_P9;
         _processorDescription.physicalProcessor = OMR_PROCESSOR_PPC_P9;
         }
      }

   if (debug("rios1"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_RIOS1;
   else if (debug("rios2"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_RIOS2;
   else if (debug("pwr403"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_PWR403;
   else if (debug("pwr405"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_PWR405;
   else if (debug("pwr601"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_PWR601;
   else if (debug("pwr603"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_PWR603;
   else if (debug("pwr604"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_PWR604;
   else if (debug("pwr630"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_PWR630;
   else if (debug("pwr620"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_PWR620;
   else if (debug("nstar"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_NSTAR;
   else if (debug("pulsar"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_PULSAR;
   else if (debug("gp"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_GP;
   else if (debug("gpul"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_GPUL;
   else if (debug("gr"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_GR;
   else if (debug("p6"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_P6;
   else if (debug("p7"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_P7;
   else if (debug("p8"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_P8;
   else if (debug("p9"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_P9;
   else if (debug("440GP"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_PWR440;
   else if (debug("750FX"))
      _processorDescription.processor = OMR_PROCESSOR_PPC_7XX;
   }

