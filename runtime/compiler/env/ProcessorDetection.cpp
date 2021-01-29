/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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

#define J9_EXTERNAL_TO_VM

#include "env/VMJ9.h"

#include <ctype.h>
#include <string.h>
#include "bcnames.h"
#include "cache.h"
#include "fastJNI.h"
#include "j9cfg.h"
#include "j9comp.h"
#include "j9consts.h"
#include "j9jitnls.h"
#include "j9list.h"
#include "j9modron.h"
#include "j9port.h"
#include "j9protos.h"
#include "jilconsts.h"
#include "jitprotos.h"
#include "locknursery.h"
#include "stackwalk.h"
#include "thrtypes.h"
#include "vmaccess.h"
#undef min
#undef max
#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/CodeGenerator.hpp"
#include "env/KnownObjectTable.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Snippet.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/OptionsUtil.hpp"
#include "control/CompilationController.hpp"
#include "env/StackMemoryRegion.hpp"
#include "compile/CompilationException.hpp"
#include "env/exports.h"
#include "env/CompilerEnv.hpp"
#include "env/CHTable.hpp"
#include "env/ClassLoaderTable.hpp"
#include "env/IO.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/PersistentInfo.hpp"
#include "env/jittypes.h"
#include "infra/Monitor.hpp"
#include "infra/MonitorTable.hpp"
#include "il/DataTypes.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/NodePool.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/IlGenRequest.hpp"
#include "infra/SimpleRegex.hpp"
#include "optimizer/Optimizer.hpp"
#include "runtime/asmprotos.h"
#include "runtime/CodeCache.hpp"
#include "runtime/codertinit.hpp"
#include "runtime/DataCache.hpp"
#include "runtime/HookHelpers.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "control/MethodToBeCompiled.hpp"
#include "env/J2IThunk.hpp"
#include "env/j9fieldsInfo.h"
#include "env/j9method.h"
#include "env/ut_j9jit.h"
#include "optimizer/J9EstimateCodeSize.hpp"
#include "runtime/J9VMAccess.hpp"
#include "runtime/IProfiler.hpp"
#include "runtime/HWProfiler.hpp"

#ifdef LINUX
#include <signal.h>
#endif

#if defined(_MSC_VER)
#include <malloc.h>
#endif

#if SOLARIS || AIXPPC || LINUX
#include <strings.h>
#endif

#if defined(J9ZOS390)
extern "C" bool _isPSWInProblemState();  /* 390 asm stub */
#endif

void
TR_J9VMBase::initializeSystemProperties()
   {
   initializeProcessorType();

   #if defined(TR_TARGET_POWER) || defined(TR_TARGET_S390) || defined(TR_TARGET_X86) || defined(TR_TARGET_ARM64)
   initializeHasResumableTrapHandler();
   initializeHasFixedFrameC_CallingConvention();
   #endif
   }


static bool
portLibCall_sysinfo_has_floating_point_unit()
   {
   #if defined(J9VM_ENV_HAS_FPU)
      return true;
   #else
      #if defined(TR_HOST_ARM) && defined(TR_TARGET_ARM) && defined(__VFP_FP__) && !defined(__SOFTFP__)
         return true;
      #else
         return false;
      #endif
   #endif
   }


bool
TR_J9VMBase::hasFPU()
   {
   return portLibCall_sysinfo_has_floating_point_unit();
   }


static TR_Processor
portLibCall_getX86ProcessorType(const char *vendor, uint32_t processorSignature)
   {
   uint32_t familyCode = (processorSignature & 0x00000f00) >> 8;
   if (!strncmp(vendor, "GenuineIntel", 12))
      {
      switch (familyCode)
         {
         case 0x05:
            return TR_X86ProcessorIntelPentium;

         case 0x06:
            {
            uint32_t modelCode  = (processorSignature & 0x000000f0) >> 4;
            if (modelCode == 0xf)
               return TR_X86ProcessorIntelCore2;
            return TR_X86ProcessorIntelP6;
            }

         case 0x0f:
            return TR_X86ProcessorIntelPentium4;
         }
      }
   else if (!strncmp(vendor, "AuthenticAMD", 12))
      {
      switch (familyCode) // pull out the family code
         {
         case 0x05:
            {
            uint32_t modelCode  = (processorSignature & 0x000000f0) >> 4;
            if (modelCode < 0x04)
               return TR_X86ProcessorAMDK5;
            return TR_X86ProcessorAMDK6;
            }

         case 0x06:
            return TR_X86ProcessorAMDAthlonDuron;

         case 0x0f:
            return TR_X86ProcessorAMDOpteron;
         }
      }

   return TR_DefaultX86Processor;
   }

// -----------------------------------------------------------------------------
/**
   This routine is currently parsing the "model name" line of /proc/cpuinfo

   It returns TR_DefaultARMProcessor for any case where it fails to parse the file
   It only looks at the first processor entry

   Example data:
     model name      : ARMv7 Processor rev 4 (v7l)

  The current implementation finds the line by searching for "Processor", then finds the ':'
  to define the start of the target string, and finally searches for ARMvx to determine the
  type.

  Maybe we could just use the architecture line:
    CPU architecture: 7

  Or perhaps read the cpuid directly.
*/
static TR_Processor
portLib_getARMLinuxProcessor()
   {
   FILE * fp ;
   char buffer[120];
   char *line_p;
   char *cpu_name = NULL;
   char *position_l, *position_r;
   size_t n=120;
   int i=0;

   fp = fopen("/proc/cpuinfo","r");

   if ( fp == NULL )
      return TR_DefaultARMProcessor;

   line_p = buffer;

   while (!feof(fp))
      {
      fgets(line_p, n, fp);
      // note the capital P, this isn't searching for the processor: line, it's
      // searching for part of the model name value
      position_l = strstr(line_p, "Processor");
      // found the model name line, position_l is to the right of the target data
      if (position_l)
         {
         position_l = strchr(line_p, ':');
         if (position_l==NULL) return TR_DefaultARMProcessor;
         position_l++;
         while (*(position_l) == ' ') position_l++;

         position_r = strchr(line_p, '\n');
         if (position_r==NULL) return TR_DefaultARMProcessor;
         while (*(position_r-1) == ' ') position_r--;

         if (position_l>=position_r) return TR_DefaultARMProcessor;

         /* localize the cpu name */
         cpu_name = position_l;
         *position_r = '\000';

         break;
         }
      }
   if (cpu_name==NULL) return TR_DefaultARMProcessor;

   fclose(fp);

   if (strstr(cpu_name, "ARMv7"))
      return TR_ARMv7;
   else if (strstr(cpu_name, "ARMv6"))
      return TR_ARMv6;
   else
      return TR_DefaultARMProcessor;
   }


static TR_Processor
portLibCall_getARMProcessorType()
   {
   TR_Processor tp;
#if defined(LINUX)
   tp = portLib_getARMLinuxProcessor();
#else
   tp = TR_DefaultARMProcessor;
#endif
   return tp;
   }

static TR_Processor
portLibCall_getARM64ProcessorType()
   {
   // ToDo: Add code for detecting processor type (Issue #6637)
   return TR_DefaultARM64Processor;
   }

// -----------------------------------------------------------------------------

// For Verbose Log
int32_t TR_J9VM::getCompInfo(char *processorName, int32_t stringLength)
   {
   _jitConfig->jitLevelName;
   int32_t returnValue = -1;
   char *sourceString = NULL;

   if (TR::Compiler->target.cpu.isPower())
      {
      switch (TR::Compiler->target.cpu.getProcessorDescription().processor)
         {
         case OMR_PROCESSOR_PPC_PWR604:
            sourceString = "PPCPWR604";
            break;

         case OMR_PROCESSOR_PPC_PWR630:
            sourceString = "PPCpwr630 ";
            break;

         case OMR_PROCESSOR_PPC_GP:
            sourceString = "PPCgp";
            break;

         case OMR_PROCESSOR_PPC_GR:
            sourceString = "PPCgr";
            break;

         case OMR_PROCESSOR_PPC_P6:
            sourceString = "PPCp6";
            break;

         case OMR_PROCESSOR_PPC_P7:
            sourceString = "PPCp7";
            break;

         case OMR_PROCESSOR_PPC_P8:
            sourceString = "PPCp8";
            break;

         case OMR_PROCESSOR_PPC_P9:
            sourceString = "PPCp9";
            break;

         case OMR_PROCESSOR_PPC_P10:
            sourceString = "PPCp10";
            break;

         case OMR_PROCESSOR_PPC_PULSAR:
            sourceString = "PPCpulsar";
            break;

         case OMR_PROCESSOR_PPC_NSTAR:
            sourceString = "PPCnstar";
            break;

         case OMR_PROCESSOR_PPC_PWR403:
            sourceString = "PPCPWR403";
            break;

         case OMR_PROCESSOR_PPC_PWR601:
            sourceString = "PPCPWR601";
            break;

         case OMR_PROCESSOR_PPC_PWR603:
            sourceString = "PPCPWR603";
            break;

         case OMR_PROCESSOR_PPC_82XX:
            sourceString = "PPCP82xx";
            break;

         case OMR_PROCESSOR_PPC_7XX:
            sourceString = "PPC7xx";
            break;

         case OMR_PROCESSOR_PPC_PWR440:
            sourceString = "PPCPWR440";
            break;

         default:
            sourceString = "Unknown PPC processor";
            break;
         }
      returnValue = strlen(sourceString);
      strncpy(processorName, sourceString, stringLength);
      return returnValue;
      }

   if (TR::Compiler->target.cpu.isARM())
      {
      sourceString = "Unknown ARM processor";
      returnValue = strlen(sourceString);
      strncpy(processorName, sourceString, stringLength);
      return returnValue;
      }

   if (TR::Compiler->target.cpu.isZ())
      {
      switch (TR::Compiler->target.cpu.getProcessorDescription().processor)
         {
         case OMR_PROCESSOR_S390_Z900:
            sourceString = "z900";
            break;

         case OMR_PROCESSOR_S390_Z990:
            sourceString = "z990";
            break;

         case OMR_PROCESSOR_S390_Z9:
            sourceString = "z9";
            break;

         case OMR_PROCESSOR_S390_Z10:
            sourceString = "z10";
            break;

         case OMR_PROCESSOR_S390_Z196:
            sourceString = "z196";
            break;

         case OMR_PROCESSOR_S390_ZEC12:
            sourceString = "zec12";
            break;

         case OMR_PROCESSOR_S390_Z13:
            sourceString = "z13";
            break;

         case OMR_PROCESSOR_S390_Z14:
            sourceString = "z14";
            break;

         case OMR_PROCESSOR_S390_Z15:
            sourceString = "z15";
            break;

         case OMR_PROCESSOR_S390_ZNEXT:
            sourceString = "zNext";
            break;
         }
      returnValue = strlen(sourceString);
      strncpy(processorName, sourceString, stringLength);
      return returnValue;
      }

   if (TR::Compiler->target.cpu.isX86())
      {
      switch (TR::Compiler->target.cpu.getProcessorDescription().processor)
         {
         case OMR_PROCESSOR_X86_INTELPENTIUM:
            sourceString = "X86 Intel Pentium";
            break;

         case OMR_PROCESSOR_X86_INTELP6:
            sourceString = "X86 Intel P6";
            break;

         case OMR_PROCESSOR_X86_INTELPENTIUM4:
            sourceString = "X86 Intel Netburst Microarchitecture";
            break;

         case OMR_PROCESSOR_X86_INTELCORE2:
            sourceString = "X86 Intel Core2 Microarchitecture";
            break;

         case OMR_PROCESSOR_X86_INTELTULSA:
            sourceString = "X86 Intel Tulsa";
            break;

         case OMR_PROCESSOR_X86_INTELNEHALEM:
            sourceString = "X86 Intel Nehalem";
            break;

         case OMR_PROCESSOR_X86_INTELWESTMERE:
            sourceString = "X86 Intel Westmere";
            break;

         case OMR_PROCESSOR_X86_INTELSANDYBRIDGE:
            sourceString = "X86 Intel Sandy Bridge";
            break;

         case OMR_PROCESSOR_X86_INTELIVYBRIDGE:
            sourceString = "X86 Intel Ivy Bridge";
            break;

         case OMR_PROCESSOR_X86_INTELHASWELL:
            sourceString = "X86 Intel Haswell";
            break;

         case OMR_PROCESSOR_X86_INTELBROADWELL:
            sourceString = "X86 Intel Broadwell";
            break;

         case OMR_PROCESSOR_X86_INTELSKYLAKE:
            sourceString = "X86 Intel Skylake";
            break;

         case OMR_PROCESSOR_X86_AMDK5:
            sourceString = "X86 AMDK5";
            break;

         case OMR_PROCESSOR_X86_AMDK6:
            sourceString = "X86 AMDK6";
            break;

         case OMR_PROCESSOR_X86_AMDATHLONDURON:
            sourceString = "X86 AMD Athlon-Duron";
            break;

         case OMR_PROCESSOR_X86_AMDOPTERON:
            sourceString = "X86 AMD Opteron";
            break;

         case OMR_PROCESSOR_X86_AMDFAMILY15H:
            sourceString = "X86 AMD Family 15h";
            break;

         default:
            sourceString = "Unknown X86 Processor";
            break;
         }
      returnValue = strlen(sourceString);
      strncpy(processorName, sourceString, stringLength);
      return returnValue;
      }
   sourceString = "Unknown Processor";
   returnValue = strlen(sourceString);
   strncpy(processorName, sourceString, stringLength);
   return returnValue;
   }


void
TR_J9VM::initializeProcessorType()
   {
   TR_ASSERT(_compInfo,"compInfo not defined");
   
   if (TR::Compiler->target.cpu.isZ())
      {
      OMRProcessorDesc processorDescription = TR::Compiler->target.cpu.getProcessorDescription();
      if (processorDescription.processor >= OMR_PROCESSOR_S390_Z10 && TR::Options::getCmdLineOptions()->getOption(TR_DisableZ10))
         processorDescription.processor = OMR_PROCESSOR_S390_FIRST;
      else if (processorDescription.processor >= OMR_PROCESSOR_S390_Z196 && TR::Options::getCmdLineOptions()->getOption(TR_DisableZ196))
         processorDescription.processor = OMR_PROCESSOR_S390_Z10;
      else if (processorDescription.processor >= OMR_PROCESSOR_S390_ZEC12 && TR::Options::getCmdLineOptions()->getOption(TR_DisableZEC12))
         processorDescription.processor = OMR_PROCESSOR_S390_Z196;
      else if (processorDescription.processor >= OMR_PROCESSOR_S390_Z13 && TR::Options::getCmdLineOptions()->getOption(TR_DisableZ13))
         processorDescription.processor = OMR_PROCESSOR_S390_ZEC12;
      else if (processorDescription.processor >= OMR_PROCESSOR_S390_Z14 && TR::Options::getCmdLineOptions()->getOption(TR_DisableZ14))
         processorDescription.processor = OMR_PROCESSOR_S390_Z13;
      else if (processorDescription.processor >= OMR_PROCESSOR_S390_Z15 && TR::Options::getCmdLineOptions()->getOption(TR_DisableZ15))
         processorDescription.processor = OMR_PROCESSOR_S390_Z14;
      else if (processorDescription.processor >= OMR_PROCESSOR_S390_ZNEXT && TR::Options::getCmdLineOptions()->getOption(TR_DisableZNext))
         processorDescription.processor = OMR_PROCESSOR_S390_Z15;

      TR::Compiler->target.cpu = TR::CPU::customize(processorDescription);
#if defined(J9ZOS390)
      // Cache whether current process is running in Supervisor State (i.e. Control Region of WAS).
      if (!_isPSWInProblemState())
         _compInfo->setIsInZOSSupervisorState();
#endif
      }
   else if (TR::Compiler->target.cpu.isPower())
      {
      OMRProcessorDesc processorDescription = TR::Compiler->target.cpu.getProcessorDescription();
      // P10 support is not yet well-tested, so it's currently gated behind an environment
      // variable to prevent it from being used by accident by users who use old versions of
      // OMR once P10 chips become available.
      if (processorDescription.processor == OMR_PROCESSOR_PPC_P10)
         {
         static bool enableP10 = feGetEnv("TR_EnableExperimentalPower10Support");
         if (!enableP10)
            {
            processorDescription.processor = OMR_PROCESSOR_PPC_P9;
            processorDescription.physicalProcessor = OMR_PROCESSOR_PPC_P9;
            }
         }

      if (debug("rios1"))
         processorDescription.processor = OMR_PROCESSOR_PPC_RIOS1;
      else if (debug("rios2"))
         processorDescription.processor = OMR_PROCESSOR_PPC_RIOS2;
      else if (debug("pwr403"))
         processorDescription.processor = OMR_PROCESSOR_PPC_PWR403;
      else if (debug("pwr405"))
         processorDescription.processor = OMR_PROCESSOR_PPC_PWR405;
      else if (debug("pwr601"))
         processorDescription.processor = OMR_PROCESSOR_PPC_PWR601;
      else if (debug("pwr603"))
         processorDescription.processor = OMR_PROCESSOR_PPC_PWR603;
      else if (debug("pwr604"))
         processorDescription.processor = OMR_PROCESSOR_PPC_PWR604;
      else if (debug("pwr630"))
         processorDescription.processor = OMR_PROCESSOR_PPC_PWR630;
      else if (debug("pwr620"))
         processorDescription.processor = OMR_PROCESSOR_PPC_PWR620;
      else if (debug("nstar"))
         processorDescription.processor = OMR_PROCESSOR_PPC_NSTAR;
      else if (debug("pulsar"))
         processorDescription.processor = OMR_PROCESSOR_PPC_PULSAR;
      else if (debug("gp"))
         processorDescription.processor = OMR_PROCESSOR_PPC_GP;
      else if (debug("gpul"))
         processorDescription.processor = OMR_PROCESSOR_PPC_GPUL;
      else if (debug("gr"))
         processorDescription.processor = OMR_PROCESSOR_PPC_GR;
      else if (debug("p6"))
         processorDescription.processor = OMR_PROCESSOR_PPC_P6;
      else if (debug("p7"))
         processorDescription.processor = OMR_PROCESSOR_PPC_P7;
      else if (debug("p8"))
         processorDescription.processor = OMR_PROCESSOR_PPC_P8;
      else if (debug("p9"))
         processorDescription.processor = OMR_PROCESSOR_PPC_P9;
      else if (debug("440GP"))
         processorDescription.processor = OMR_PROCESSOR_PPC_PWR440;
      else if (debug("750FX"))
         processorDescription.processor = OMR_PROCESSOR_PPC_7XX;

      TR::Compiler->target.cpu = TR::CPU::customize(processorDescription);
      }
   else if (TR::Compiler->target.cpu.isX86())
      {
      OMRProcessorDesc processorDescription = TR::Compiler->target.cpu.getProcessorDescription();
      OMRPORT_ACCESS_FROM_OMRPORT(TR::Compiler->omrPortLib);
      if (feGetEnv("TR_DisableAVX"))
         {
         omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_X86_OSXSAVE, FALSE);
         }
      
      TR::Compiler->target.cpu = TR::CPU::customize(processorDescription);

      const char *vendor = TR::Compiler->target.cpu.getProcessorVendorId();
      uint32_t processorSignature = TR::Compiler->target.cpu.getProcessorSignature();

      TR::Compiler->target.cpu.setProcessor(portLibCall_getX86ProcessorType(vendor, processorSignature));

      TR_ASSERT(TR::Compiler->target.cpu.id() >= TR_FirstX86Processor
             && TR::Compiler->target.cpu.id() <= TR_LastX86Processor, "Not a valid X86 Processor Type");
      }
   else if (TR::Compiler->target.cpu.isARM())
      {
      TR::Compiler->target.cpu.setProcessor(portLibCall_getARMProcessorType());

      TR_ASSERT(TR::Compiler->target.cpu.id() >= TR_FirstARMProcessor
             && TR::Compiler->target.cpu.id() <= TR_LastARMProcessor, "Not a valid ARM Processor Type");
      }
   else if (TR::Compiler->target.cpu.isARM64())
      {
      TR::Compiler->target.cpu.setProcessor(portLibCall_getARM64ProcessorType());

      TR_ASSERT(TR::Compiler->target.cpu.id() >= TR_FirstARM64Processor
             && TR::Compiler->target.cpu.id() <= TR_LastARM64Processor, "Not a valid ARM64 Processor Type");
      }
   else
      {
      TR_ASSERT(0,"Unknown target");
      }

   _jitConfig->targetProcessor = TR::Compiler->target.cpu.getProcessorDescription();
   _jitConfig->relocatableTargetProcessor = TR::Compiler->relocatableTarget.cpu.getProcessorDescription();
   }
