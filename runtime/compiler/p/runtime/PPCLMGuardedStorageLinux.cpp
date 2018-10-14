/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include "j9cfg.h"
#include "p/runtime/PPCLMGuardedStorage.hpp"
#include "p/runtime/PPCHWProfilerPrivate.hpp"
#include "j9port_generated.h"
#include "runtime/HWProfiler.hpp"
#include "control/CompilationThread.hpp"

#include <errno.h>
#include <gnu/libc-version.h>
#include <syscall.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <sys/stat.h>
#include "env/CompilerEnv.hpp"

#ifdef LINUXPPC64
#define TCB "13"
#define TCB_EBB_CONTEXT -28720
#else /* LINUXPPC */
#define TCB "2"
#define TCB_EBB_CONTEXT -28696
#endif /* LINUXPPC */

static inline
__attribute__((always_inline))
void* getTCBContext()
   {
   void *ret;
   asm(laddr " %0, %1(" TCB ")"
       : "=r" (ret)
       : "i" (TCB_EBB_CONTEXT));
   return ret;
   }

static inline
__attribute__((always_inline))
void setTCBContext(void *context)
   {
   asm(staddr " %0, %1(" TCB ")"
       :
       : "r" (context), "i" (TCB_EBB_CONTEXT));
   }

static bool isRHEL()
   {
   // Note: We only check for RHEL, rather than RHEL7 (which is the first release
   // to support P8), because RHEL6 and earlier will have an older glibc version
   // and don't have any kernel P8 support either, so an EBB feature check and
   // any attempt to acquire the PMU with EBB support would fail anyhow.

   // If this file exists assume the distro is RHEL.
   struct stat s;
   return stat("/etc/redhat-release", &s) == 0;
   }

TR_PPCLMGuardedStorage *
TR_PPCLMGuardedStorage::allocate(J9JITConfig *jitConfig, bool ebbSetupDone)
   {
   if (!ebbSetupDone)
      {
      const unsigned int  requiredMajor = 2;
      const unsigned int  requiredMinor = 18;
      const char         *glibcVersion = gnu_get_libc_version();
      unsigned int        major, minor;

      // Our EBB code assumes and requires that we have space for at least 1 context pointer
      // available in the TCB. Glibc 2.18 added 4 such pointers to the TCB, but unfortunately
      // is no API available to detect this at runtime, the only thing we can do is check
      // if glibc >= 2.18.
      if (sscanf(glibcVersion, "%u.%u", &major, &minor) != 2)
         {
         VERBOSE("Failed to parse glibc version string '%s'.", glibcVersion);
         return NULL;
         }
      if (major < requiredMajor || major == requiredMajor && minor < requiredMinor)
         {
         // Even worse, RHEL7 has glibc 2.17, but they did backport the patch. However, since
         // we can't check for it we check if we're running on RHEL and glibc 2.17 and, if so,
         // assume it's there.
         const unsigned int requiredMajorRHEL = 2;
         const unsigned int requiredMinorRHEL = 17;
         if (!isRHEL() || major < requiredMajorRHEL || major == requiredMajorRHEL && minor < requiredMinorRHEL)
            {
            VERBOSE("glibc version %s does not support EBB context in TCB, need %u.%u or later.", glibcVersion, requiredMajor, requiredMinor);
            return NULL;
            }
         else
            VERBOSE("OS appears to be RHEL with glibc %d.%d, assuming glibc supports EBB context in TCB.", major, minor);
         }
      J9ProcessorDesc *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
      J9PortLibrary   *privatePortLibrary = TR::Compiler->portLib;
      if (!j9sysinfo_processor_has_feature(processorDesc, J9PORT_PPC_FEATURE_EBB))
         {
         VERBOSE("Failed to detect kernel EBB support.");
         return NULL;
         }
      }

   TR_PPCLMGuardedStorage *LMGuardedStorage = new (PERSISTENT_NEW) TR_PPCLMGuardedStorage(jitConfig);
   VERBOSE("LMGuardedStorage initialized.");
   return LMGuardedStorage;
   }

bool
TR_PPCLMGuardedStorage::initializeThread(J9VMThread *vmThread)
   {
#if 0
   PORT_ACCESS_FROM_VMC(vmThread);

   j9ri_initialize(vmThread->riParameters);
   if (IS_THREAD_RI_INITIALIZED(vmThread))
      {
      VERBOSE("J9VMThread=%p initialized for HW profiling.", vmThread);
      return true;
      }
#else

#if defined(DEBUG) || defined(PPCRI_VERBOSE)
   VERBOSE("Initializing PMU on J9VMThread=%p, tid=%u.", vmThread, syscall(SYS_gettid));
#else /* !(DEBUG || PPCRI_VERBOSE) */
   VERBOSE("Initializing PMU on J9VMThread=%p.", vmThread);
#endif /* !(DEBUG || PPCRI_VERBOSE) */

   TR_PPCHWProfilerEBBContext *context;
   if (!IS_THREAD_RI_INITIALIZED(vmThread))
      {
      TR_PPCHWProfilerEBBContext *context = (TR_PPCHWProfilerEBBContext*)jitPersistentAlloc(sizeof(TR_PPCHWProfilerEBBContext));
         if (!context)
            {
            VERBOSE("Failed to allocate context for J9VMThread=%p.", vmThread);
            return false;
            }
         VERBOSE("Allocated context=%p for J9VMThread=%p.", context, vmThread);
         setTCBContext(context);
         MTSPR(EBBHR, FUNCTION_ADDRESS(ebbHandler));
      }
   else
      {
      context = (TR_PPCHWProfilerEBBContext*)getTCBContext();
      // set global enable bit
      MTSPR(BESCRSU, BESCRU_GE);
      }
   context->lmEventHandler = lmEventHandler;
   // clear both LME and LMEO bits
   //MTSPR(BESCRR, BESCR_LMEO);
   //MTSPR(BESCRRU, BESCRU_LME);

   VERBOSE("J9VMThread=%p, initialized for LM guarded storage.", vmThread);
   return true;
#endif
   }

bool
TR_PPCLMGuardedStorage::deinitializeThread(J9VMThread *vmThread)
   {
#if 0
   PORT_ACCESS_FROM_VMC(vmThread);
   j9ri_deinitialize(vmThread->riParameters);
#else
   //MTSPR(BESCRR, BESCR_LMEO);
   //MTSPR(BESCRSU, BESCRU_LME);
   TR_PPCHWProfilerEBBContext *context = (TR_PPCHWProfilerEBBContext*)getTCBContext();
   if (context)
      {
      VERBOSE("Retrieved context=%p for terminating J9VMThread=%p.", context, vmThread);
      jitPersistentFree(context);
      }
#endif
   return true;
   }

