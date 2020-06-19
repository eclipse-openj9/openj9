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

// supportsFeature and supports_feature_test will be removed when old_apis are no longer needed
bool
J9::Power::CPU::supportsFeature(uint32_t feature)
   {
   if (TR::Compiler->omrPortLib == NULL)
      {
      return false;
      }

#if defined(J9VM_OPT_JITSERVER)
  if (TR::CompilationInfo::getStream())
#endif
     {
     TR_ASSERT_FATAL(self()->supports_feature_test(feature), "feature test %d failed", feature);
     }

   OMRPORT_ACCESS_FROM_OMRPORT(TR::Compiler->omrPortLib);
   return (TRUE == omrsysinfo_processor_has_feature(&_processorDescription, feature));
   }

bool
J9::Power::CPU::supports_feature_test(uint32_t feature)
   {
   bool ans_old = false;
   bool ans_new = false;

#if !defined(TR_HOST_POWER) || (defined(J9OS_I5) && defined(J9OS_I5_V5R4))
   ans_old = false;
#else
   J9ProcessorDesc *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   J9PortLibrary *privatePortLibrary = TR::Compiler->portLib;
   ans_old = (TRUE == j9sysinfo_processor_has_feature(processorDesc, feature));
#endif
   
   OMRPORT_ACCESS_FROM_OMRPORT(TR::Compiler->omrPortLib);
   ans_new = (TRUE == omrsysinfo_processor_has_feature(&_processorDescription, feature));

   return ans_new == ans_old; 
   }

bool
J9::Power::CPU::is(OMRProcessorArchitecture p)
   {
   if (TR::Compiler->omrPortLib == NULL)
      return self()->id() == self()->get_old_processor_type_from_new_processor_type(p);

#if defined(J9VM_OPT_JITSERVER)
  if (TR::CompilationInfo::getStream())
#endif
     {
     TR_ASSERT_FATAL((_processorDescription.processor == p) == (self()->id() == self()->get_old_processor_type_from_new_processor_type(p)), "is test %d failed, id() %d, _processorDescription.processor %d", p, self()->id(), _processorDescription.processor);
     }

   return _processorDescription.processor == p;
   }

bool
J9::Power::CPU::isAtLeast(OMRProcessorArchitecture p)
   {
   if (TR::Compiler->omrPortLib == NULL)
      return self()->id() >= self()->get_old_processor_type_from_new_processor_type(p);

#if defined(J9VM_OPT_JITSERVER)
  if (TR::CompilationInfo::getStream())
#endif
     {
     TR_ASSERT_FATAL((_processorDescription.processor >= p) == (self()->id() >= self()->get_old_processor_type_from_new_processor_type(p)), "is at least test %d failed, id() %d, _processorDescription.processor %d", p, self()->id(), _processorDescription.processor);
     }

   return _processorDescription.processor >= p;
   }

bool
J9::Power::CPU::isAtMost(OMRProcessorArchitecture p)
   {
   if (TR::Compiler->omrPortLib == NULL)
      return self()->id() <= self()->get_old_processor_type_from_new_processor_type(p);

#if defined(J9VM_OPT_JITSERVER)
   if (TR::CompilationInfo::getStream())
#endif
      {
      TR_ASSERT_FATAL((_processorDescription.processor <= p) == (self()->id() <= self()->get_old_processor_type_from_new_processor_type(p)), "is at most test %d failed, id() %d, _processorDescription.processor %d", p, self()->id(), _processorDescription.processor);
      }

   return _processorDescription.processor <= p;
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

bool
J9::Power::CPU::getPPCSupportsVSX()
   {
   return self()->supportsFeature(OMR_FEATURE_PPC_HAS_VSX);
   }
