/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
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

#if defined(J9ZOS390)
#include <sys/utsname.h>
#endif

#if defined(J9ZTPF)
#include <tpf/c_pi1dt.h> // for access to machine type.
#include <tpf/c_cinfc.h> // for access to cinfc table.
#endif

#include "control/Options_inlines.hpp"

extern J9JITConfig * jitConfig;

namespace J9
{

namespace Z
{

////////////////////////////////////////////////////////////////////////////////
// The following are the 2 versions of the /proc/cpuinfo file for Linux/390.
//
// When Gallileo (TREX) comes out, we will need to update this routine to support
// TREX and not end up generating default 9672 code!
// vendor_id       : IBM/S390
// # processors    : 2
// bogomips per cpu: 838.86
// processor 0: version = FF,  identification = 100003,  machine = 2064
// processor 1: version = FF,  identification = 200003,  machine = 2064
//
// vs.
//
// vendor_id       : IBM/S390
// # processors    : 3
// bogomips per cpu: 493.15
// processor 0: version = FF,  identification = 100003,  machine = 9672
// processor 1: version = FF,  identification = 200003,  machine = 9672
// processor 2: version = FF,  identification = 300003,  machine = 9672
////////////////////////////////////////////////////////////////////////////////

/* Keep in sync with enum TR_S390MachineType in env/ProcessorInfo.hpp */
static const int S390MachineTypes[] =
   {
   TR_FREEWAY, TR_Z800, TR_MIRAGE, TR_MIRAGE2, TR_TREX, TR_Z890, TR_GOLDEN_EAGLE, TR_DANU_GA2, TR_Z9BC,
   TR_Z10, TR_Z10BC, TR_ZG, TR_ZGMR, TR_ZEC12, TR_ZEC12MR, TR_ZG_RESERVE, TR_ZEC12_RESERVE,
   TR_Z13, TR_Z13s, TR_Z14, TR_Z14s, TR_ZNEXT, TR_ZNEXTs,
   TR_ZH, TR_DATAPOWER, TR_ZH_RESERVE1, TR_ZH_RESERVE2
   };

static const int S390UnsupportedMachineTypes[] =
   {
   TR_G5, TR_MULTIPRISE7000
   };


bool
CPU::TO_PORTLIB_get390zLinux_cpuinfoFeatureFlag(const char *feature)
   {
   char line[256];
   const int LINE_SIZE = sizeof(line) - 1;
   const char procHeader[] = "features";
   const int PROC_HEADER_SIZE = sizeof(procHeader) - 1;
#ifdef J9ZTPF
   /*
    * z/TPF platform knows "stfle" is supported;
    * for other zLinux features, simply return false.
   */
   return false;
#endif

   FILE * fp = fopen("/proc/cpuinfo", "r");
   if (fp)
      {
      while (fgets(line, LINE_SIZE, fp) > 0)
         {
         if (!memcmp(line, procHeader, PROC_HEADER_SIZE))
            {
            if (strstr(line, feature))
               {
               fclose(fp);
               return true;
               }
            }
         }
      fclose(fp);
      }
   return false;
   }


bool
CPU::TO_PORTLIB_get390zLinux_supportsStoreExtendedFacilityList()
   {
   char line[80];
   const int LINE_SIZE = sizeof(line) - 1;
   const char typeHeader[] = "Type: ";
   const int TYPE_LINE_SIZE = 27;
   const int TYPE_HEADER_SIZE = sizeof(typeHeader) - 1;
   const char zVMHeader[] = "VM00 Control Program: z/VM    ";
   const int zVM_LINE_SIZE = 39;
   const int zVM_HEADER_SIZE = sizeof(zVMHeader) - 1;
   bool supportedType = false;
   bool supportedzVM = true;

#ifdef J9ZTPF
   /*
    * z/TPF knows "stfle" is supported, but z/TPF platform cannot confirm this on procfs.
   */
   return true;
#else

   if (TR::Compiler->target.cpu.TO_PORTLIB_get390zLinux_cpuinfoFeatureFlag("stfle"))
      return true;

   FILE *fp = fopen("/proc/sysinfo", "r");
   if (fp)
      {
      while (fgets(line, LINE_SIZE, fp) > 0)
         {
         int len = strlen(line);
         if (len > TYPE_HEADER_SIZE &&
             !memcmp(line, typeHeader, TYPE_HEADER_SIZE) &&
             !supportedType)
            {
            if (len == TYPE_LINE_SIZE)
               {
               int type;
               sscanf(line, "%*s %d",&type);

               if (type == TR_GOLDEN_EAGLE || type == TR_DANU_GA2 || type == TR_Z9BC ||
                   type == TR_Z10 || type == TR_Z10BC || type == TR_ZG || type == TR_ZGMR ||
                   type == TR_ZEC12 || type == TR_ZEC12MR || type == TR_Z13 || type == TR_Z13s ||
                   type == TR_Z14 || type == TR_Z14s)
                  {
                  supportedType = true;
                  }
               }
            }
         else if (len > zVM_HEADER_SIZE &&
                  !memcmp(line, zVMHeader, zVM_HEADER_SIZE))
            {
            if (len == zVM_LINE_SIZE)
               {
               int ver=0;
               int major=0;
               sscanf(line, "%*s %*s %*s %*s %d%*c%d%*c%*d%*c", &ver, &major);

               if (ver < 5 || (ver == 5 && major < 2))
                  {
                  supportedzVM = false;
                  }
               }
            }
         }
      fclose(fp);
      }
   return (supportedType && supportedzVM);
#endif
   }


TR_S390MachineType
CPU::TO_PORTLIB_get390zLinuxMachineType()
   {
   char line[80];
   const int LINE_SIZE = sizeof(line) - 1;
   const char procHeader[] = "processor ";
   const int PROC_LINE_SIZE = 69;
   const int PROC_HEADER_SIZE = sizeof(procHeader) - 1;

#if defined(J9ZTPF)
   TR_S390MachineType ret_machine = TR_Z10;  /* return value, z/TPF default */
   const int pi1modArraySize = 5;
   char processorName[pi1modArraySize] = {0};
   struct pi1dt *pid;
   /* machine hardware name */
   pid = (struct pi1dt *)cinfc_fast(CINFC_CMMPID);
   sprintf(processorName, "%4.X", pid->pi1pids.pi1mslr.pi1mod);
   int machine = atoi(processorName);

   // Scan list of unsupported machines - We do not initialize the JIT for such hardware.
   for (int i = 0; i < sizeof(S390UnsupportedMachineTypes) / sizeof(int); ++i)
      {
      if (machine == S390UnsupportedMachineTypes[i])
         {
         PORT_ACCESS_FROM_ENV(jitConfig->javaVM);
         j9nls_printf(jitConfig->javaVM->portLibrary, J9NLS_ERROR, J9NLS_J9JIT_390_UNSUPPORTED_HARDWARE, machine);
         TR_ASSERT(0,"Hardware is not supported.");
         throw TR::CompilationException();
         }
      }

   // Scan list of supported machines.
   for (int i = 0; i < sizeof(S390MachineTypes) / sizeof(int); ++i)
      {
      if (machine == S390MachineTypes[i])
         {
         ret_machine = (TR_S390MachineType)machine;
         }
      }

#else
   TR_S390MachineType ret_machine = TR_UNDEFINED_S390_MACHINE;  /* return value */

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
               int i;
               int machine;
               sscanf(line, "%*s %*d%*c %*s %*c %*s %*s %*c %*s %*s %*c %d", &machine);

               // Scan list of unsupported machines - We do not initialize the JIT for such hardware.
               for (i = 0; i < sizeof(S390UnsupportedMachineTypes) / sizeof(int); ++i)
                  {
                  if (machine == S390UnsupportedMachineTypes[i])
                     {
                     PORT_ACCESS_FROM_ENV(jitConfig->javaVM);
                     j9nls_printf(jitConfig->javaVM->portLibrary, J9NLS_ERROR, J9NLS_J9JIT_390_UNSUPPORTED_HARDWARE, machine);
                     TR_ASSERT(0,"Hardware is not supported.");
                     throw TR::CompilationException();
                     }
                  }

               // Scan list of supported machines.
               for (i = 0; i < sizeof(S390MachineTypes) / sizeof(int); ++i)
                  {
                  if (machine == S390MachineTypes[i])
                     {
                     ret_machine = (TR_S390MachineType)machine;
                     }
                  }
               }
            }
         }
      fclose(fp);
      }
#endif
   return ret_machine;
  }


TR_S390MachineType
CPU::TO_PORTLIB_get390zOSMachineType()
   {
   TR_S390MachineType ret_machine = TR_UNDEFINED_S390_MACHINE;
#if defined(J9ZOS390)
   struct utsname info;
   if (::uname(&info))
      return ret_machine;

   uint32_t type = atoi(info.machine);
   // Scan list of supported machines.
   for (int32_t i = 0; i < sizeof(S390MachineTypes) / sizeof(int); ++i)
      {
      if (type == S390MachineTypes[i])
         {
         ret_machine = (TR_S390MachineType)type;
         break;
         }
      }
#endif
   return ret_machine;
   }

bool
CPU::TO_PORTLIB_get390_supportsZNext()
   {
#if defined(TR_HOST_S390) && (defined(J9ZOS390) || defined(LINUX))
   J9ProcessorDesc *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   //check for ZNext. To be moved to portlib.
   return (processorDesc->processor >= PROCESSOR_S390_GP13);
#endif
   return false;
   }

bool
CPU::TO_PORTLIB_get390_supportsZ14()
   {
#if defined(TR_HOST_S390) && (defined(J9ZOS390) || defined(LINUX))
   J9ProcessorDesc *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   //check for Z14. To be moved to portlib.
   return (processorDesc->processor >= PROCESSOR_S390_GP12);
#endif
   return false;
   }

bool
CPU::TO_PORTLIB_get390_supportsZ13()
   {
#if defined(TR_HOST_S390) && (defined(J9ZOS390) || defined(LINUX))
   J9ProcessorDesc *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   //check for Z13. To be moved to portlib.
   return (processorDesc->processor >= PROCESSOR_S390_GP11);
#endif
   return false;
   }


bool
CPU::TO_PORTLIB_get390_supportsZ6()
   {
#if defined(TR_HOST_S390) && (defined(J9ZOS390) || defined(LINUX))
   J9ProcessorDesc  *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   //check for Z6, ZGryphon, ZHelix or Z13. To be moved to portlib.
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
   //check for ZGryphon, ZHelix or Z13. To be moved to portlib.
   return (processorDesc->processor >= PROCESSOR_S390_GP9);
#endif
   return false;
   }


bool
CPU::TO_PORTLIB_get390_supportsZHelix()
   {
#if defined(TR_HOST_S390) && (defined(J9ZOS390) || defined(LINUX))
   J9ProcessorDesc  *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   //check for ZHelix or Z13. To be moved to portlib.
   return (processorDesc->processor >= PROCESSOR_S390_GP10);
#endif
   return false;
   }


////////////////////////////////////////////////////////////////////////////////
// zLinux specific checks
////////////////////////////////////////////////////////////////////////////////


/* check for RAS highword register support on zLinux */
bool
CPU::TO_PORTLIB_get390zLinux_supportsHPRDebug()
   {
#if defined(TR_TARGET_32BIT)
   return TR::Compiler->target.cpu.TO_PORTLIB_get390zLinux_cpuinfoFeatureFlag("highgprs");
#else
   /* the 64-bit kernel always has RAS support for highword registers */
   return true;
#endif
   }


/* check for transactional memory support on zLinux */
bool
CPU::TO_PORTLIB_get390zLinux_SupportTM()
   {
   return TR::Compiler->target.cpu.TO_PORTLIB_get390zLinux_cpuinfoFeatureFlag(" te ");
   }


/* check for vector facility support on zLinux */
bool
CPU::TO_PORTLIB_get390zLinux_supportsVectorFacility()
   {
   return TR::Compiler->target.cpu.TO_PORTLIB_get390zLinux_cpuinfoFeatureFlag(" vx ");
   }


/**
 zOS hardware detection checks a word at byte 200 of PSA.

 The following is a map of the bits in this word (zOS V1.6):
     200     (C8)    BITSTRING        1    FLCFACL0     Byte 0 of FLCFACL
                      1...  ....           FLCFN3       "X'80'" - N3
                                                        installed
                      .1..  ....           FLCFZARI     "X'40'" -
                                                        z/Architecture
                                                        installed
                      ..1.  ....           FLCFZARA     "X'20'" -
                                                        z/Architecture
                                                        activce
                      ....  ..1.           FLCFASLX     "X'02'" - ASN & LX
                                                        reuse facility
                                                        installed
     201     (C9)    BITSTRING        1    FLCFACL1     Byte 1 of FLCFACL
     202     (CA)    BITSTRING        1    FLCFACL2     Byte 2 of FLCFACL
                      1...  ....           FLCFETF2     "X'80'" Extended
                                                        Translation facility
                                                        2
                      .1..  ....           FLCFCRYA     "X'40'" Cryptographic
                                                        assist
                      ..1.  ....           FLCFLD       "X'20'" Long
                                                        Displacement facility
                      ...1  ....           FLCFLDHP     "X'10'" Long
                                                        Displacement High
                                                        Performance
                      ....  1...           FLCFHMAS     "X'08'" HFP Multiply
                                                        Add/Subtract
                      ....  .1..           FLCFEIMM     "X'04'" Extended
                                                        immediate when z/Arch
                      ....  .1..           FLCFETF3     "X'04'" Extended
                                                        Translation Facility
                                                        3 when z/Arch
                      ....  ...1           FLCFHUN      "X'01'" HFP
                                                        unnormalized
                                                        extension
     203     (CB)    BITSTRING        1    FLCFACL3     Byte 3 of FLCFACL
 */

bool
CPU::TO_PORTLIB_get390zOS_N3Support()
   {
   //  Location 200 is architected such that bit 0 is ON if N3 instructions
   //  are available
   return (((*(int*) 200) & 0x80000000) != 0);
   }


bool
CPU::TO_PORTLIB_get390zOS_ZArchSupport()
   {
   // Location 200 is architected such that
   //    bit 1 is ON if z/Architecture is installed
   //    bit 2 is ON if z/Architecture is active
   // We check both bits to ensure that z/Architecture is available.
   //
   // Alternative solution - Test if FLCARCH is not 0.
   //    2 FLCARCH     CHAR(1),               /* Architecture info    @LSA*/
   //      4 *        BIT(7),                 /*                      @LSA*/
   //      4 PSAZARCH BIT(1),                 /* z/Architecture       @LSA*/
   //        5 PSAESAME BIT(1),               /* z/Architecture       @LSA*/
   // FLCARCH is at offset x'A3'
   //
   return (((*(int*) 200) & 0x60000000) == 0x60000000);
   }


bool
CPU::TO_PORTLIB_get390zOS_TrexSupport()
   {
   // Location 200 is architected such that bit 0x00001000 is ON if Trex
   // instructons are available
   // The 0x'10' bit at byte 202 is the long displacement high performance bit
   // supported on TRex or higher hardware.
   return (((*(int*) 200) & 0x00001000) != 0);
   }


bool
CPU::TO_PORTLIB_get390zOS_GoldenEagleSupport()
   {
   // Location 200 is architected such that bit 21 is ON if Golden Eagle
   // instructons are available
   //  Location 202  - 0x4 (bit 21) - Extended Immediate
   //  Location 202  - 0x2 (bit 22) - Translation Facility 3
   return (((*(int*) 200) & 0x00000400) != 0) &&
          (((*(int*) 200) & 0x00000200) != 0);
   }


bool
CPU::TO_PORTLIB_get390zOS_supportsStoreExtendedFacilityList()
   {
   // Location 200 is architected such that bit 7 is ON if STFLE
   // instruction is installed
   return (((*(int*) 200) & (0x80000000 >> 7)) != 0);
   }

/////////////////////////////////////////

bool
CPU::getS390SupportsFPE()
   {
   static char *Z10FPE = feGetEnv("TR_disableFPE");
   if (Z10FPE != NULL)
      return false;

   return _flags.testAny(S390SupportsFPE);
   }


bool
CPU::getS390SupportsTM()
   {
   return (_flags.testAny(S390SupportsTM) &&
           !(TR::Options::getCmdLineOptions()->getOption(TR_DisableZHelix)));
   }


bool
CPU::getS390SupportsRI()
   {
   return ( _flags.testAny(S390SupportsRI) &&
           !TR::Options::getCmdLineOptions()->getOption(TR_DisableZHelix)  &&
           !TR::Options::getCmdLineOptions()->getOption(TR_DisableZ13));
   }


bool
CPU::getS390SupportsVectorFacility()
   {
   return (_flags.testAny(S390SupportsVectorFacility) &&
           !(TR::Options::getCmdLineOptions()->getOption(TR_DisableZ13)));
   }

bool
CPU::getS390SupportsVectorPackedDecimalFacility()
   {
   return (_flags.testAny(S390SupportsVectorPackedDecimalFacility) &&
           !(TR::Options::getCmdLineOptions()->getOption(TR_DisableZ14)));
   }

bool
CPU::getS390SupportsGuardedStorageFacility()
   {
   return (_flags.testAny(S390SupportsGuardedStorageFacility) &&
           _flags.testAny(S390SupportsSideEffectAccessFacility));
   }

////////////////////////////////////////////////////////////////////////////////

void
CPU::initializeS390zLinuxProcessorFeatures()
   {
   J9ProcessorDesc *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   J9PortLibrary *privatePortLibrary = TR::Compiler->portLib;

   // The following conditionals are dependent on each other and must occur in this order

   TR::Compiler->target.cpu.setS390SupportsZ900();

   // Check for Facility bits, which can detect z6/z10 or higher.
   // If STFLE is supported, we must rely on these bits.
   // zVM can spoof as a newer machine model, without really providing the support.
   // However, the facility bits don't lie.
   //
   if (TR::Compiler->target.cpu.getS390SupportsZ900() &&
       TR::Compiler->target.cpu.TO_PORTLIB_get390zLinux_supportsStoreExtendedFacilityList())
      {
      // Check facility bits for support of hardware

      if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZNext())
         TR::Compiler->target.cpu.setS390SupportsZNext();
      else if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZ14())
         TR::Compiler->target.cpu.setS390SupportsZ14();
      else if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZ13())
         TR::Compiler->target.cpu.setS390SupportsZ13();
      else if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZHelix())
         TR::Compiler->target.cpu.setS390SupportsZEC12();
      else if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZGryphon())
         TR::Compiler->target.cpu.setS390SupportsZ196();
      else if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZ6())
         TR::Compiler->target.cpu.setS390SupportsZ10();

      // z9 (DANU) supports DFP in millicode - so do not check
      // for DFP support unless z10 or higher.
      if (TR::Compiler->target.cpu.getS390SupportsZ10() && j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_DFP))
         TR::Compiler->target.cpu.setS390SupportsDFP();

      if (j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_FPE))
         TR::Compiler->target.cpu.setS390SupportsFPE();
      }

   // Either z6/z10 wasn't detected or STFLE is not supported.
   // Use machine model.
   if (!TR::Compiler->target.cpu.getS390SupportsZ10() || !TR::Compiler->target.cpu.TO_PORTLIB_get390zLinux_supportsStoreExtendedFacilityList())
      {
      TR_S390MachineType machineType = TR::Compiler->target.cpu.getS390MachineType();

      if (machineType == TR_GOLDEN_EAGLE || machineType == TR_DANU_GA2 || machineType == TR_Z9BC)
         {
         TR::Compiler->target.cpu.setS390SupportsZ9();
         }
      else if (machineType == TR_TREX || machineType == TR_MIRAGE || machineType == TR_MIRAGE2 || machineType == TR_Z890)
         {
         TR::Compiler->target.cpu.setS390SupportsZ990();
         }

      // For z10+ only use machine model if STFLE is not supported. Otherwise, we should have detected it above.
      if (!TR::Compiler->target.cpu.TO_PORTLIB_get390zLinux_supportsStoreExtendedFacilityList())
         {
         if (machineType == TR_ZNEXT || machineType == TR_ZNEXTs)
            {
            TR::Compiler->target.cpu.setS390SupportsZNext();
            }
         else if (machineType == TR_Z14 || machineType == TR_Z14s)
            {
            TR::Compiler->target.cpu.setS390SupportsZ14();
            }
         else if (machineType == TR_Z13 || machineType == TR_Z13s)
            {
            TR::Compiler->target.cpu.setS390SupportsZ13();
            }
         else if (machineType == TR_ZEC12 || machineType == TR_ZEC12MR || machineType == TR_ZEC12_RESERVE)
            {
            TR::Compiler->target.cpu.setS390SupportsZEC12();
            }
         else if (machineType == TR_ZG || machineType == TR_ZGMR || machineType == TR_ZG_RESERVE)
            {
            TR::Compiler->target.cpu.setS390SupportsZ196();
            }
         else if (machineType == TR_Z10 || machineType == TR_Z10BC)
            {
            TR::Compiler->target.cpu.setS390SupportsZ10();
            }
         }
      }

   // Only SLES11 SP1 supports HPR debugging facilities, which is
   // required for RAS support for HPR (ZGryphon or higher).
   if (TR::Compiler->target.cpu.getS390SupportsZ196() && TR::Compiler->target.cpu.TO_PORTLIB_get390zLinux_supportsHPRDebug())
      TR::Compiler->target.cpu.setS390SupportsHPRDebug();

   // Check for OS support of TM.
   if (TR::Compiler->target.cpu.getS390SupportsZEC12() && TR::Compiler->target.cpu.TO_PORTLIB_get390zLinux_SupportTM())
      TR::Compiler->target.cpu.setS390SupportsTM();


   if (TR::Compiler->target.cpu.getS390SupportsZEC12() && (0 == j9ri_enableRISupport()))
      TR::Compiler->target.cpu.setS390SupportsRI();

   if (TR::Compiler->target.cpu.getS390SupportsZ13() && TR::Compiler->target.cpu.TO_PORTLIB_get390zLinux_supportsVectorFacility())
      TR::Compiler->target.cpu.setS390SupportsVectorFacility();

   if (TR::Compiler->target.cpu.getS390SupportsZ14())
      {
      // Check for vector packed decimal facility as indicated by bit 129 and 134.
      if (TR::Compiler->target.cpu.getS390SupportsVectorFacility() &&
              j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_VECTOR_PACKED_DECIMAL))
         {
         TR::Compiler->target.cpu.setS390SupportsVectorPackedDecimalFacility();
         }

      // Check for guarded storage facility as indicated by bit 131 and 133
      if (j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_GUARDED_STORAGE) &&
              j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_SIDE_EFFECT_ACCESS))
         {
         TR::Compiler->target.cpu.setS390SupportsGuardedStorageFacility();
         }
      }
   }


void
CPU::initializeS390zOSProcessorFeatures()
   {
   J9PortLibrary *privatePortLibrary = TR::Compiler->portLib;

   if (!TR::Compiler->target.cpu.TO_PORTLIB_get390zOS_N3Support() || !TR::Compiler->target.cpu.TO_PORTLIB_get390zOS_ZArchSupport())
      {
      j9nls_printf(privatePortLibrary, J9NLS_ERROR, J9NLS_J9JIT_390_UNSUPPORTED_HARDWARE, TR_G5);
      TR_ASSERT(0,"Hardware is not supported.");
      throw TR::CompilationException();
      }

    // JIT pre-req's zArchitecture (and N3).
    TR::Compiler->target.cpu.setS390SupportsZ900();

   if (TR::Compiler->target.cpu.TO_PORTLIB_get390zOS_GoldenEagleSupport())
      TR::Compiler->target.cpu.setS390SupportsZ9();
   else if(TR::Compiler->target.cpu.TO_PORTLIB_get390zOS_TrexSupport())
      TR::Compiler->target.cpu.setS390SupportsZ990();

   J9ProcessorDesc *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();

   // On z10 or higher architectures, we should check for facility bits.
   if (TR::Compiler->target.cpu.getS390SupportsZ900() &&
       TR::Compiler->target.cpu.TO_PORTLIB_get390zOS_supportsStoreExtendedFacilityList())
      {
      if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZNext())
          TR::Compiler->target.cpu.setS390SupportsZNext();
      else if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZ14())
         TR::Compiler->target.cpu.setS390SupportsZ14();
      else if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZ13())
         TR::Compiler->target.cpu.setS390SupportsZ13();
      else if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZHelix())
         TR::Compiler->target.cpu.setS390SupportsZEC12();
      else if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZGryphon())
         TR::Compiler->target.cpu.setS390SupportsZ196();
      else if (TR::Compiler->target.cpu.TO_PORTLIB_get390_supportsZ6())
         TR::Compiler->target.cpu.setS390SupportsZ10();

      // z9 (DANU) supports DFP in millicode - so do not check
      // for DFP support unless z10 or higher.
      if (TR::Compiler->target.cpu.getS390SupportsZ10() && j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_DFP))
         TR::Compiler->target.cpu.setS390SupportsDFP();

      if (j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_FPE))
         TR::Compiler->target.cpu.setS390SupportsFPE();

      // zOS supports HPR debugging facilities on ZGryphon or higher.
      if (TR::Compiler->target.cpu.getS390SupportsZ196())
         TR::Compiler->target.cpu.setS390SupportsHPRDebug();

      if (TR::Compiler->target.cpu.getS390SupportsZEC12())
         {
         // check for zOS TM support
         U_8 * cvtptr = (U_8 *)(*(uint32_t *)16);   // pointer to CVT is at offset +16 of PSA (at address 0)
         U_8 cvttxj = *(cvtptr+0x17b);              // CVTTX and CVTTXC are at offset +0x17B of CVT, bits 8 and 4, indicating full support
         if ((cvttxj & 0xc) == 0xc)
            {
            TR::Compiler->target.cpu.setS390SupportsTM();
            }
         // check for zOS RI support
         if ((cvttxj & 0x2) == 0x2)                  // CVTRI bit is X'02' bit at byte X'17B' off CVT
            {
            TR::Compiler->target.cpu.setS390SupportsRI();
            }
         }

      if (TR::Compiler->target.cpu.getS390SupportsZ13() &&
              j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_VECTOR_FACILITY))
         {
         TR::Compiler->target.cpu.setS390SupportsVectorFacility();
         }

      if (TR::Compiler->target.cpu.getS390SupportsZ14())
         {
         // Check for vector packed decimal facility as indicated by bit 129 and 134.
         if(TR::Compiler->target.cpu.getS390SupportsVectorFacility() &&
                 j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_VECTOR_PACKED_DECIMAL))
            {
            TR::Compiler->target.cpu.setS390SupportsVectorPackedDecimalFacility();
            }

         // Check for guarded storage facility as indicated by bit 131 and 133
         if (j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_GUARDED_STORAGE) &&
                 j9sysinfo_processor_has_feature(processorDesc, J9PORT_S390_FEATURE_SIDE_EFFECT_ACCESS))
            {
            TR::Compiler->target.cpu.setS390SupportsGuardedStorageFacility();
            }
         }
      }
   }


}

}
