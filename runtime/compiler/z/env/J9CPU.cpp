/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

namespace J9
{

namespace Z
{

int32_t
CPU::TO_PORTLIB_get390MachineId()
  {
#if defined(J9ZTPF) || defined(J9ZOS390)
   // Note we cannot use utsname on Linux as it simply returns "s390x" in info.machine. On z/OS we can indeed use it [1].
   //
   // [1] https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.1.0/com.ibm.zos.v2r1.bpxbd00/rtuna.htm
   struct utsname info;

   if (::uname(&info) >= 0)
      {
      return atoi(info.machine);
      }

   return 2098;
#else
   char line[80];
   const int LINE_SIZE = sizeof(line) - 1;
   const char procHeader[] = "processor ";
   const int PROC_LINE_SIZE = 69;
   const int PROC_HEADER_SIZE = sizeof(procHeader) - 1;

   FILE * fp = fopen("/proc/cpuinfo", "r");
   if (fp)
      {
      while (fgets(line, LINE_SIZE, fp) > 0)
         {
         int len = strlen(line);
         if (len > PROC_HEADER_SIZE && !memcmp(line, procHeader, PROC_HEADER_SIZE))
            {
            if (len == PROC_LINE_SIZE)
               {
               int32_t id;

               // Match the following pattern to extract the machine id:
               //
               // processor 0: version = FF,  identification = 100003,  machine = 9672
               sscanf(line, "%*s %*d%*c %*s %*c %*s %*s %*c %*s %*s %*c %d", &id);

               return id;
               }
            }
         }
      fclose(fp);
      }

   return 2098;
#endif
   }

bool
CPU::TO_PORTLIB_get390_supportsZNext()
   {
#if defined(TR_HOST_S390) && (defined(J9ZOS390) || defined(LINUX))
   J9ProcessorDesc *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   return (processorDesc->processor >= PROCESSOR_S390_GP14);
#endif
   return false;
   }

bool
CPU::TO_PORTLIB_get390_supportsZ15()
   {
#if defined(TR_HOST_S390) && (defined(J9ZOS390) || defined(LINUX))
   J9ProcessorDesc *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   return (processorDesc->processor >= PROCESSOR_S390_GP13);
#endif
   return false;
   }

bool
CPU::TO_PORTLIB_get390_supportsZ14()
   {
#if defined(TR_HOST_S390) && (defined(J9ZOS390) || defined(LINUX))
   J9ProcessorDesc *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   return (processorDesc->processor >= PROCESSOR_S390_GP12);
#endif
   return false;
   }

bool
CPU::TO_PORTLIB_get390_supportsZ13()
   {
#if defined(TR_HOST_S390) && (defined(J9ZOS390) || defined(LINUX))
   J9ProcessorDesc *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   return (processorDesc->processor >= PROCESSOR_S390_GP11);
#endif
   return false;
   }


bool
CPU::TO_PORTLIB_get390_supportsZ6()
   {
#if defined(TR_HOST_S390) && (defined(J9ZOS390) || defined(LINUX))
   J9ProcessorDesc  *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   return (processorDesc->processor >= PROCESSOR_S390_GP8);
#endif
   return false;
   }


bool
CPU::TO_PORTLIB_get390_supportsZGryphon()
   {
   // Location 200 is architected such that bit 45 is ON if zG
   // instruction is installed
#if defined(TR_HOST_S390) && (defined(J9ZOS390) || defined(LINUX))
   J9ProcessorDesc  *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   return (processorDesc->processor >= PROCESSOR_S390_GP9);
#endif
   return false;
   }


bool
CPU::TO_PORTLIB_get390_supportsZHelix()
   {
#if defined(TR_HOST_S390) && (defined(J9ZOS390) || defined(LINUX))
   J9ProcessorDesc  *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   return (processorDesc->processor >= PROCESSOR_S390_GP10);
#endif
   return false;
   }

void
CPU::initializeS390ProcessorFeatures()
   {
   // The following nested if statements cascade so as to have the effect of only enabling the least common denominator
   // of disable CPU architectures. For example if the user specified to disable z13 when running on a z15 machine the
   // logic below will ensure we only reach the `setSupportsArch` call for zEC12.
   if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZ6() &&
         !TR::Options::getCmdLineOptions()->getOption(TR_DisableZ10))
      {
      TR::Compiler->target.cpu.setSupportsArch(TR::CPU::z10);

      if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZGryphon() &&
            !TR::Options::getCmdLineOptions()->getOption(TR_DisableZ196))
         {
         TR::Compiler->target.cpu.setSupportsArch(TR::CPU::z196);

         if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZHelix() &&
               !TR::Options::getCmdLineOptions()->getOption(TR_DisableZEC12))
            {
            TR::Compiler->target.cpu.setSupportsArch(TR::CPU::zEC12);

            if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZ13() &&
                  !TR::Options::getCmdLineOptions()->getOption(TR_DisableZ13))
               {
               TR::Compiler->target.cpu.setSupportsArch(TR::CPU::z13);

               if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZ14() &&
                     !TR::Options::getCmdLineOptions()->getOption(TR_DisableZ14))
                  {
                  TR::Compiler->target.cpu.setSupportsArch(TR::CPU::z14);

                  if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZ15() &&
                        !TR::Options::getCmdLineOptions()->getOption(TR_DisableZ15))
                     {
                     TR::Compiler->target.cpu.setSupportsArch(TR::CPU::z15);

                     if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZNext() &&
                           !TR::Options::getCmdLineOptions()->getOption(TR_DisableZNext))
                        {
                        TR::Compiler->target.cpu.setSupportsArch(TR::CPU::zNext);
                        }
                     }
                  }
               }
            }
         }
      }
   
   J9ProcessorDesc* processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();

   // This variable is used internally by the j9sysinfo macros below and cannot be folded away
   J9PortLibrary* privatePortLibrary = TR::Compiler->portLib;

   if (j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_FPE))
      {
      TR::Compiler->target.cpu.setSupportsFloatingPointExtensionFacility(true);
      }

   // z9 supports DFP in millicode so do not check for DFP support unless we are z10+
   if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10) &&
         j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_DFP))
      {
      TR::Compiler->target.cpu.setSupportsDecimalFloatingPointFacility(true);
      }

   if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z196) && 
         j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_HIGH_WORD))
      {
      TR::Compiler->target.cpu.setSupportsHighWordFacility(true);
      }

   if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::zEC12))
      {
      if (j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_TE))
         {
         TR::Compiler->target.cpu.setSupportsTransactionalMemoryFacility(true);
         }

      if (j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_RI))
         {
#if defined(LINUX)
         if (0 == j9ri_enableRISupport())
#endif
         TR::Compiler->target.cpu.setSupportsRuntimeInstrumentationFacility(true);
         }
      }

   if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z13) &&
         j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_VECTOR_FACILITY))
      {
      TR::Compiler->target.cpu.setSupportsVectorFacility(true);
      }

   if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z14))
      {
      if (j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_VECTOR_PACKED_DECIMAL))
         {
         TR::Compiler->target.cpu.setSupportsVectorPackedDecimalFacility(true);
         }

      if (j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_GUARDED_STORAGE))
         {
         // turn off GS facility if GC has -XXgc:softwareRangeCheckReadBarrier
         J9MemoryManagerFunctions * mmf = TR::Compiler->javaVM->memoryManagerFunctions;
         TR::Compiler->target.cpu.setSupportsGuardedStorageFacility(!(mmf->j9gc_software_read_barrier_enabled(TR::Compiler->javaVM)));
         }
      }

   if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z15))
      {
      if (j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_MISCELLANEOUS_INSTRUCTION_EXTENSION_3))
         {
         TR::Compiler->target.cpu.setSupportsMiscellaneousInstructionExtensions3Facility(true);
         }

      if (j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_VECTOR_FACILITY_ENHANCEMENT_2))
         {
         TR::Compiler->target.cpu.setSupportsVectorFacilityEnhancement2(true);
         }

      if (j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY))
         {
         TR::Compiler->target.cpu.setSupportsVectorPackedDecimalEnhancementFacility(true);
         }
      }
   }

TR_ProcessorFeatureFlags
CPU::getProcessorFeatureFlags()
   {
   TR_ProcessorFeatureFlags processorFeatureFlags = { {_flags.getValue()} };
   return processorFeatureFlags;
   }

bool
CPU::isCompatible(TR_Processor processorSignature, TR_ProcessorFeatureFlags processorFeatureFlags)
   {
   if (!self()->isAtLeast(processorSignature))
      {
      return false;
      }
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
