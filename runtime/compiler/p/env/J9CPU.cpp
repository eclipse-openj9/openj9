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

#include "control/CompilationRuntime.hpp"
#include "env/CompilerEnv.hpp"
#include "env/CPU.hpp"
#include "j9.h"
#include "j9port.h"


bool
J9::Power::CPU::getPPCSupportsVMX()
   {
#if defined(J9OS_I5) && defined(J9OS_I5_V5R4)
   return FALSE;
#else
   J9ProcessorDesc   *processorDesc       = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   J9PortLibrary     *privatePortLibrary  = TR::Compiler->portLib;
   BOOLEAN feature = j9sysinfo_processor_has_feature(processorDesc, J9PORT_PPC_FEATURE_HAS_ALTIVEC);
   return (TRUE == feature);
#endif
   }

bool
J9::Power::CPU::getPPCSupportsVSX()
   {
#if defined(J9OS_I5) && defined(J9OS_I5_V5R4)
   return FALSE;
#else
   J9ProcessorDesc   *processorDesc       = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   J9PortLibrary     *privatePortLibrary  = TR::Compiler->portLib;
   BOOLEAN feature = j9sysinfo_processor_has_feature(processorDesc, J9PORT_PPC_FEATURE_HAS_VSX);
   return (TRUE == feature);
#endif
   }

bool
J9::Power::CPU::getPPCSupportsAES()
   {
#if defined(J9OS_I5) && defined(J9OS_I5_V5R4)
   return FALSE;
#else
   J9ProcessorDesc   *processorDesc       = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   J9PortLibrary     *privatePortLibrary  = TR::Compiler->portLib;
   BOOLEAN hasVMX  = j9sysinfo_processor_has_feature(processorDesc, J9PORT_PPC_FEATURE_HAS_ALTIVEC);
   BOOLEAN hasVSX  = j9sysinfo_processor_has_feature(processorDesc, J9PORT_PPC_FEATURE_HAS_VSX);
   BOOLEAN isP8    = (processorDesc->processor >= PROCESSOR_PPC_P8);
   return (isP8 && hasVMX && hasVSX);
#endif
   }

bool
J9::Power::CPU::getPPCSupportsTM()
   {
#if defined(J9OS_I5) && defined(J9OS_I5_V5R4)
   return FALSE;
#else
   J9ProcessorDesc   *processorDesc       = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   J9PortLibrary     *privatePortLibrary  = TR::Compiler->portLib;
   BOOLEAN feature = j9sysinfo_processor_has_feature(processorDesc, J9PORT_PPC_FEATURE_HTM);
   return (TRUE == feature);
#endif
   }

// Double check with os400 team to see if we can enable popcnt on I
//
bool 
J9::Power::CPU::hasPopulationCountInstruction()
   {
#if defined(J9OS_I5)
   return false;
#else
   return self()->id() >= TR_PPCp7;
#endif
   }

bool
J9::Power::CPU::supportsDecimalFloatingPoint()
   {
#if !defined(TR_HOST_POWER) || (defined(J9OS_I5) && defined(J9OS_I5_V5R4))
   return FALSE;
#else
   J9ProcessorDesc *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   J9PortLibrary *privatePortLibrary = TR::Compiler->portLib;
   BOOLEAN feature = j9sysinfo_processor_has_feature(processorDesc, J9PORT_PPC_FEATURE_HAS_DFP);
   return (TRUE == feature);
#endif
   }

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

OMRProcessorDesc
J9::Power::CPU::getProcessorDescription()
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
