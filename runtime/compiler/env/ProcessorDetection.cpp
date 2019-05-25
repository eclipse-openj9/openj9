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
#include "exceptions/JITShutDown.hpp"
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
#include "il/Node.hpp"
#include "il/NodePool.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ParameterSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
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

   #if defined(TR_TARGET_POWER) || defined(TR_TARGET_S390) || defined(TR_TARGET_X86)
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


// -----------------------------------------------------------------------------

TR_Processor
portLibCall_getProcessorType()
   {
   J9ProcessorDesc *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   return mapJ9Processor(processorDesc->processor);
   }

TR_Processor
portLibCall_getPhysicalProcessorType()
   {
   J9ProcessorDesc *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   return mapJ9Processor(processorDesc->physicalProcessor);
   }

TR_Processor mapJ9Processor(J9ProcessorArchitecture j9processor)
   {
   TR_Processor tp = TR_NullProcessor;

   switch(j9processor)
      {
      case PROCESSOR_UNDEFINED:
         tp = TR_NullProcessor;
         break;

      case PROCESSOR_S390_UNKNOWN:
         tp = TR_Default390Processor;
         break;
      case PROCESSOR_S390_GP6:
         tp = TR_s370gp6;
         break;
      case PROCESSOR_S390_GP7:
         tp = TR_s370gp7;
         break;
      case PROCESSOR_S390_GP8:
         tp = TR_s370gp8;
         break;
      case PROCESSOR_S390_GP9:
         tp = TR_s370gp9;
         break;
      case PROCESSOR_S390_GP10:
         tp = TR_s370gp10;
         break;
      case PROCESSOR_S390_GP11:
         tp = TR_s370gp11;
         break;
      case PROCESSOR_S390_GP12:
         tp = TR_s370gp12;
         break;

      case PROCESSOR_PPC_UNKNOWN:
         tp = TR_DefaultPPCProcessor;
         break;
      case PROCESSOR_PPC_7XX:
         tp = TR_PPC7xx;
         break;
      case PROCESSOR_PPC_GP:
         tp = TR_PPCgp;
         break;
      case PROCESSOR_PPC_GR:
         tp = TR_PPCgr;
         break;
      case PROCESSOR_PPC_NSTAR:
         tp = TR_PPCnstar;
         break;
      case PROCESSOR_PPC_PULSAR:
         tp = TR_PPCpulsar;
         break;
      case PROCESSOR_PPC_PWR403:
         tp = TR_PPCpwr403;
         break;
      case PROCESSOR_PPC_PWR405:
         tp = TR_PPCpwr405;
         break;
      case PROCESSOR_PPC_PWR440:
         tp = TR_PPCpwr440;
         break;
      case PROCESSOR_PPC_PWR601:
         tp = TR_PPCpwr601;
         break;
      case PROCESSOR_PPC_PWR602:
         tp = TR_PPCpwr602;
         break;
      case PROCESSOR_PPC_PWR603:
         tp = TR_PPCpwr603;
         break;
      case PROCESSOR_PPC_PWR604:
         tp = TR_PPCpwr604;
         break;
      case PROCESSOR_PPC_PWR620:
         tp = TR_PPCpwr620;
         break;
      case PROCESSOR_PPC_PWR630:
         tp = TR_PPCpwr630;
         break;
      case PROCESSOR_PPC_RIOS1:
         tp = TR_PPCrios1;
         break;
      case PROCESSOR_PPC_RIOS2:
         tp = TR_PPCrios2;
         break;
      case PROCESSOR_PPC_P6:
         tp = TR_PPCp6;
         break;
      case PROCESSOR_PPC_P7:
         tp = TR_PPCp7;
         break;
      case PROCESSOR_PPC_P8:
         tp = TR_PPCp8;
         break;
      case PROCESSOR_PPC_P9:
         tp = TR_PPCp9;
         break;

      case PROCESSOR_X86_UNKNOWN:
         tp = TR_DefaultX86Processor;
         break;
      case PROCESSOR_X86_INTELPENTIUM:
         tp = TR_X86ProcessorIntelPentium;
         break;
      case PROCESSOR_X86_INTELP6:
         tp = TR_X86ProcessorIntelP6;
         break;
      case PROCESSOR_X86_INTELPENTIUM4:
         tp = TR_X86ProcessorIntelPentium4;
         break;
      case PROCESSOR_X86_INTELCORE2:
         tp = TR_X86ProcessorIntelCore2;
         break;
      case PROCESSOR_X86_INTELTULSA:
         tp = TR_X86ProcessorIntelTulsa;
         break;
      case PROCESSOR_X86_AMDK5:
         tp = TR_X86ProcessorAMDK5;
         break;
      case PROCESSOR_X86_AMDK6:
         tp = TR_X86ProcessorAMDK6;
         break;
      case PROCESSOR_X86_AMDATHLONDURON:
         tp = TR_X86ProcessorAMDAthlonDuron;
         break;
      case PROCESSOR_X86_AMDOPTERON:
         tp = TR_X86ProcessorAMDOpteron;
         break;
      case PROCESSOR_X86_INTELNEHALEM:             //Not yet mapped in TR_Processor.
      case PROCESSOR_X86_INTELWESTMERE:
      case PROCESSOR_X86_INTELSANDYBRIDGE:
      case PROCESSOR_X86_INTELHASWELL:
         tp = TR_X86ProcessorAMDOpteron;
         break;
      default:
        return tp;
      }

   return tp;
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

TR_Processor
TR_J9VMBase::getPPCProcessorType()
   {
   return portLibCall_getProcessorType();
   }

bool
TR_J9VMBase::getPPCSupportsVSXRegisters()
   {
#if defined(TR_TARGET_POWER)
   return TR::Compiler->target.cpu.getPPCSupportsVSX();
#else
   return false;
#endif // TR_TARGET_POWER
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
      switch(portLibCall_getProcessorType())
         {
         case TR_PPCpwr604:
            sourceString = "PPCPWR604";
            break;

         case TR_PPCpwr630:
            sourceString = "PPCpwr630 ";
            break;

         case TR_PPCgp:
            sourceString = "PPCgp";
            break;

         case TR_PPCgr:
            sourceString = "PPCgr";
            break;

         case TR_PPCp6:
            sourceString = "PPCp6";
            break;

         case TR_PPCp7:
            sourceString = "PPCp7";
            break;

         case TR_PPCp8:
            sourceString = "PPCp8";
            break;

         case TR_PPCp9:
            sourceString = "PPCp9";
            break;

         case TR_PPCpulsar:
            sourceString = "PPCpulsar";
            break;

         case TR_PPCnstar:
            sourceString = "PPCnstar";
            break;

         case TR_PPCpwr403:
            sourceString = "PPCPWR403";
            break;

         case TR_PPCpwr601:
            sourceString = "PPCPWR601";
            break;

         case TR_PPCpwr603:
            sourceString = "PPCPWR603";
            break;

         case  TR_PPC82xx:
            sourceString = "PPCP82xx";
            break;

         case  TR_PPC7xx:
            sourceString = "PPC7xx";
            break;

         case TR_PPCpwr440:
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

#if defined(TR_TARGET_S390)
   if (TR::Compiler->target.cpu.isZ())
      {
      int32_t machineId = TR::Compiler->target.cpu.TO_PORTLIB_get390MachineId();

      return snprintf(processorName, stringLength, "%s (%d)", TR::Compiler->target.cpu.getProcessorName(machineId), machineId);
      }
#endif

   if (TR::Compiler->target.cpu.isX86())
      {
      switch(TR::Compiler->target.cpu.id())
         {
         case TR_X86ProcessorIntelPentium:
            sourceString = "X86 Intel Pentium";
            break;

         case TR_X86ProcessorIntelP6:
            sourceString = "X86 Intel P6";
            break;

         case TR_X86ProcessorIntelPentium4:
            sourceString = "X86 Intel Netburst Microarchitecture";
            break;

         case TR_X86ProcessorIntelCore2:
            sourceString = "X86 Intel Core2 Microarchitecture";
            break;

         case TR_X86ProcessorIntelTulsa:
            sourceString = "X86 Intel Tulsa";
            break;

         case TR_X86ProcessorAMDK5:
            sourceString = "X86 AMDK5";
            break;

         case TR_X86ProcessorAMDAthlonDuron:
            sourceString = "X86 AMD Athlon-Duron";
            break;

         case TR_X86ProcessorAMDOpteron:
            sourceString = "X86 AMD Opteron";
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
#if defined(TR_HOST_S390)
         initializeS390ProcessorFeatures();

#if defined(J9ZOS390)
         // Cache whether current process is running in Supervisor State (i.e. Control Region of WAS).
         if (!_isPSWInProblemState())
             _compInfo->setIsInZOSSupervisorState();
#endif
#endif

#ifdef TR_TARGET_S390
      // For AOT shared classes cache processor compatibility purposes, the following
      // processor settings should not be modified.
      if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::zNext))
         {
         TR::Compiler->target.cpu.setProcessor(TR_s370gp14);
         }
      else if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z15))
         {
         TR::Compiler->target.cpu.setProcessor(TR_s370gp13);
         }
      else if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z14))
         {
         TR::Compiler->target.cpu.setProcessor(TR_s370gp12);
         }
      else if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z13))
         {
         TR::Compiler->target.cpu.setProcessor(TR_s370gp11);
         }
      else if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::zEC12))
         {
         TR::Compiler->target.cpu.setProcessor(TR_s370gp10);
         }
      else if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z196))
         {
         TR::Compiler->target.cpu.setProcessor(TR_s370gp9);
         }
      else if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z10))
         {
         TR::Compiler->target.cpu.setProcessor(TR_s370gp8);
         }
      else if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z9))
         {
         TR::Compiler->target.cpu.setProcessor(TR_s370gp7);
         }
      else if (TR::Compiler->target.cpu.getSupportsArch(TR::CPU::z990))
         {
         TR::Compiler->target.cpu.setProcessor(TR_s370gp6);
         }
      else
         {
         TR::Compiler->target.cpu.setProcessor(TR_s370gp5);
         }
#endif

      TR_ASSERT(TR::Compiler->target.cpu.id() >= TR_First390Processor
             && TR::Compiler->target.cpu.id() <= TR_Last390Processor, "Not a valid 390 Processor Type");
      }
   else if (TR::Compiler->target.cpu.isARM())
      {
      TR::Compiler->target.cpu.setProcessor(portLibCall_getARMProcessorType());

      TR_ASSERT(TR::Compiler->target.cpu.id() >= TR_FirstARMProcessor
             && TR::Compiler->target.cpu.id() <= TR_LastARMProcessor, "Not a valid ARM Processor Type");
      }
   else if (TR::Compiler->target.cpu.isPower())
      {
      //We can have a "reported" processor and a "underlying" processor.
      //See design 28980
      TR_Processor tp_reported;
      TR_Processor tp_underlying;

      if( isAOT_DEPRECATED_DO_NOT_USE() )
         TR::Compiler->target.cpu.setProcessor( TR_DefaultPPCProcessor );
      else
         {
         tp_reported    = portLibCall_getProcessorType();
         tp_underlying  = portLibCall_getPhysicalProcessorType();
#if defined(TR_TARGET_POWER)
         TR::Compiler->target.cpu.setProcessor(tp_reported);
#endif
         }

      TR_ASSERT(TR::Compiler->target.cpu.id() >= TR_FirstPPCProcessor
             && TR::Compiler->target.cpu.id() <= TR_LastPPCProcessor, "Not a valid PPC Processor Type");

      #if defined(DEBUG)
      _compInfo->setProcessorByDebugOption();
      #endif
      }
   else if (TR::Compiler->target.cpu.isX86())
      {
      const char *vendor = TR::Compiler->target.cpu.getProcessorVendorId();
      uint32_t processorSignature = TR::Compiler->target.cpu.getProcessorSignature();

      TR::Compiler->target.cpu.setProcessor(portLibCall_getX86ProcessorType(vendor, processorSignature));

      TR_ASSERT(TR::Compiler->target.cpu.id() >= TR_FirstX86Processor
             && TR::Compiler->target.cpu.id() <= TR_LastX86Processor, "Not a valid X86 Processor Type");
      }
   else
      {
      TR_ASSERT(0,"Unknown target");
      }
   }

// -----------------------------------------------------------------------------

#if defined(TR_TARGET_S390)
void
TR_J9VMBase::initializeS390ProcessorFeatures()
   {
   TR::Compiler->target.cpu.initializeS390ProcessorFeatures();
   }

void
TR_J9SharedCacheVM::initializeS390ProcessorFeatures()
   {
   return;
   }
#endif
