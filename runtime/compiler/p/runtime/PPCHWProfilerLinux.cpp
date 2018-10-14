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
#include "p/runtime/PPCHWProfiler.hpp"
#include "p/runtime/PPCHWProfilerPrivate.hpp"
#include "j9port_generated.h"
#include "control/CompilationThread.hpp"
#include "env/VMJ9.h"

#include <errno.h>
#include <gnu/libc-version.h>
#include <syscall.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <sys/stat.h>
#include "env/CompilerEnv.hpp"

#define PERF_CONFIG_EBB                 0x8000000000000000ULL
#define PERF_CONFIG_BHRB                0x4000000000000000ULL
#define PERF_CONFIG_MARK                0x0000000000000100ULL
#define PERF_CONFIG_SM_RIS              (0<<27)
#define PERF_CONFIG_SM_RLS              (1<<27)
#define PERF_CONFIG_SM_RBS              (2<<27)
#define PERF_CONFIG_ES_RIS_ALL          (0<<24)
#define PERF_CONFIG_ES_RIS_LS           (1<<24)
#define PERF_CONFIG_ES_RIS_PNOP         (2<<24)
#define PERF_CONFIG_ES_RLS_CMISS        (0<<24)
#define PERF_CONFIG_ES_RBS_MPRED        (0<<24)
#define PERF_CONFIG_ES_RBS_MPRED_CR     (1<<24)
#define PERF_CONFIG_ES_RBS_MPRED_TA     (2<<24)
#define PERF_CONFIG_ES_RBS_TAKEN        (3<<24)
#define PERF_CONFIG_PMC_SHIFT           16
#define PERF_CONFIG_IFM_SHIFT           60

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

TR_PPCHWProfiler *
TR_PPCHWProfiler::allocate(J9JITConfig *jitConfig)
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

   TR_PPCHWProfiler *profiler = new (PERSISTENT_NEW) TR_PPCHWProfiler(jitConfig);
   VERBOSE("HWProfiler initialized.");

   return profiler;
   }

struct tr_perf_event_attr {

        __u32                   type;
        __u32                   size;
        __u64                   config;

        union {
                __u64           sample_period;
                __u64           sample_freq;
        };

        __u64                   sample_type;
        __u64                   read_format;

        __u64                   disabled       :  1,
                                inherit        :  1,
                                pinned         :  1,
                                exclusive      :  1,
                                exclude_user   :  1,
                                exclude_kernel :  1,
                                exclude_hv     :  1,
                                exclude_idle   :  1,
                                mmap           :  1,
                                comm           :  1,
                                freq           :  1,
                                inherit_stat   :  1,
                                enable_on_exec :  1,
                                task           :  1,
                                watermark      :  1,
                                precise_ip     :  2,
                                mmap_data      :  1,
                                sample_id_all  :  1,

                                exclude_host   :  1,
                                exclude_guest  :  1,

                                exclude_callchain_kernel : 1,
                                exclude_callchain_user   : 1,

                                __reserved_1   : 41;

        union {
                __u32           wakeup_events;
                __u32           wakeup_watermark;
        };

        __u32                   bp_type;
        union {
                __u64           bp_addr;
                __u64           config1;
        };
        union {
                __u64           bp_len;
                __u64           config2;
        };
        __u64   branch_sample_type;

        __u64   sample_regs_user;
        __u32   sample_stack_user;
        __u32   __reserved_2;
};

bool
TR_PPCHWProfiler::initializeThread(J9VMThread *vmThread)
   {
   if (IS_THREAD_RI_INITIALIZED(vmThread))
      return true;

#if 0
   PORT_ACCESS_FROM_VMC(vmThread);

   j9ri_initialize(vmThread->riParameters);
   if (IS_THREAD_RI_INITIALIZED(vmThread))
      {
      VERBOSE("J9VMThread=%p initialized for HW profiling.", vmThread);
      return true;
      }
#else
   // If we've already hit our memory budget don't even try to go further
   if (_ppcHWProfilerBufferMemoryAllocated >= _ppcHWProfilerBufferMaximumMemory)
      return false;

   const TR_PPCHWProfilerPMUConfig *configs = TR_PPCHWProfilerPMUConfig::getPMUConfigs();

   TR_PPCHWProfilerEBBContext *context;
   struct tr_perf_event_attr   pe;
   int                         fds[MAX_PMCS];
   int                         groupfd = -1;
   TR_PPCHWProfilingConfigs    currentConfig = MethodHotness;
   bool                        setUnavailableOnFail = true;

   memset(&pe, 0, sizeof(struct tr_perf_event_attr));
   memset(fds, -1, sizeof(int) * MAX_PMCS);

#if defined(DEBUG) || defined(PPCRI_VERBOSE)
   VERBOSE("Initializing PMU on J9VMThread=%p, tid=%u.", vmThread, syscall(SYS_gettid));
#else /* !(DEBUG || PPCRI_VERBOSE) */
   VERBOSE("Initializing PMU on J9VMThread=%p.", vmThread);
#endif /* !(DEBUG || PPCRI_VERBOSE) */

   for (int i = 0; i < MAX_PMCS; ++i)
      {
      if (!configs[currentConfig].eventHandlerTable[i])
         continue;

      pe.type = PERF_TYPE_RAW;
      pe.size = sizeof(struct tr_perf_event_attr);
      // Note that BHRBs require kernel support that was added after general EBB support, so if you
      // ask for BHRB access and the kernel doesn't support it perf_event_open() will return an error
      pe.config = configs[currentConfig].config[i] | PERF_CONFIG_EBB |
#ifdef HAVE_BHRBES
                  PERF_CONFIG_BHRB | (uint64_t)configs[currentConfig].bhrbFilter << PERF_CONFIG_IFM_SHIFT |
#endif /* HAVE_BHRBES */
                  (i + 1) << PERF_CONFIG_PMC_SHIFT;
      pe.pinned = 1;
      pe.exclusive = 1;
      pe.exclude_kernel = 1;
      pe.exclude_hv = 1;
      pe.exclude_idle = 1;

      fds[i] = syscall(SYS_perf_event_open, &pe, 0, -1, groupfd, 0);
      if (fds[i] < 0)
         {
         VERBOSE("Failed to open perf interface for counter %u for J9VMThread=%p, errno: %d, perf_event_open : %s.", i + 1, vmThread, errno, strerror(errno));
         goto closefds;
         }

      // First initialized event is the group leader
      if (groupfd < 0)
         groupfd = fds[i];
      }

   void *bufs[MAX_PMCS];
   memset(bufs, 0, sizeof(void*) * MAX_PMCS);
   for (int i = 0; i < MAX_PMCS; ++i)
      {
      if (!configs[currentConfig].eventHandlerTable[i])
         continue;
      bufs[i] = allocateBuffer(configs[currentConfig].bufferElementSize[i] * configs[currentConfig].bufferSize[i]);
      if (!bufs[i])
         {
         VERBOSE("Failed to allocate buffer %u for J9VMThread=%p.", i + 1, vmThread);
         // Don't have enough memory now, but might in the future, so don't disable HWP completely
         setUnavailableOnFail = false;
         goto freebufs;
         }
      VERBOSE("Allocated initial buffer %p for J9VMThread=%p.", bufs[i], vmThread);
      }

   context = (TR_PPCHWProfilerEBBContext*)jitPersistentAlloc(sizeof(TR_PPCHWProfilerEBBContext));
   if (!context)
      {
      VERBOSE("Failed to allocate context for J9VMThread=%p.", vmThread);
      goto freebufs;
      }
   VERBOSE("Allocated context=%p for J9VMThread=%p.", context, vmThread);

   context->eventHandler = configs[currentConfig].eventHandler;
   context->vmThread = vmThread;
   context->currentConfig = currentConfig;
   for (int i = 0; i < MAX_PMCS; ++i)
      {
      context->eventCounts[i] = 0;
      context->sampleRates[i] = (configs[currentConfig].sampleRates[i] == INVALID_SAMPLERATE ? INVALID_SAMPLERATE : TR::Options::_hwprofilerPRISamplingRate);
      context->fds[i] = fds[i];
      context->buffers[i].start = bufs[i];
      context->buffers[i].spaceLeft = configs[currentConfig].bufferSize[i];
      context->counterBitMask |= (bufs[i] ? (1 << i) : 0);
      }
   context->unknownEventCount = 0;
   context->lostPMU = false;

   setTCBContext(context);

   MTSPR(EBBHR, FUNCTION_ADDRESS(ebbHandler));
   MTSPR(BESCRR, BESCR_PMEO);
   MTSPR(BESCRSU, BESCRU_GE | BESCRU_PME);
   MTSPR(PMC1, configs[currentConfig].config[0] ? 0x80000000 - context->sampleRates[0] : 0);
   MTSPR(PMC2, configs[currentConfig].config[1] ? 0x80000000 - context->sampleRates[1] : 0);
   MTSPR(PMC3, configs[currentConfig].config[2] ? 0x80000000 - context->sampleRates[2] : 0);
   MTSPR(PMC4, configs[currentConfig].config[3] ? 0x80000000 - context->sampleRates[3] : 0);
   MTSPR(PMC5, configs[currentConfig].config[4] ? 0x80000000 - context->sampleRates[4] : 0);
   MTSPR(PMC6, configs[currentConfig].config[5] ? 0x80000000 - context->sampleRates[5] : 0);
   // Freeze the counters that we're not using
   uint64_t mmcr2;
   TR_PPCHWProfilerPMUConfig::calcMMCR2ForConfig(mmcr2, currentConfig);
   MTSPR64(MMCR2, mmcr2);

   vmThread->riParameters->controlBlock = context;
   vmThread->riParameters->flags |= J9PORT_RI_INITIALIZED;

   if (configs[currentConfig].vmFlags)
      {
      vmThread->privateFlags |= J9_PRIVATE_FLAGS_RI_INITIALIZED;
      vmThread->jitCurrentRIFlags |= configs[currentConfig].vmFlags;
      }
   else
      {
      vmThread->riParameters->flags |= J9PORT_RI_ENABLED;
      MTSPR(MMCR0, MMCR0_PMAE);
      }

   if (IS_THREAD_RI_INITIALIZED(vmThread))
      {
      VERBOSE("J9VMThread=%p, initialized for HW profiling.", vmThread);
      return true;
      }
freectx:
   jitPersistentFree(context);
freebufs:
   for (int i = 0; i < MAX_PMCS; ++i)
      {
      if (bufs[i])
         freeBuffer(bufs[i], configs[currentConfig].bufferElementSize[i] * configs[currentConfig].bufferSize[i]);(bufs[i]);
      }
closefds:
   for (int i = 0; i < MAX_PMCS; ++i)
      {
      if (fds[i] >= 0 && close(fds[i]))
         VERBOSE("Failed to close perf interface on J9VMThread=%p, errno: %d, close : %s.", vmThread, errno, strerror(errno));
      }
fail:
   // Prevent any future threads from trying to initialize if we hit a failure that is not transient
   if (setUnavailableOnFail)
      {
      VERBOSE("Failure on J9VMThread=%p was critical. HW profiling will be unavailable from now on.", vmThread);
      setHWProfilingAvailable(false);
      }
#endif
   return false;
   }

bool
TR_PPCHWProfiler::deinitializeThread(J9VMThread *vmThread)
   {
   if (!IS_THREAD_RI_INITIALIZED(vmThread))
      return true;

#if 0
   PORT_ACCESS_FROM_VMC(vmThread);

   j9ri_deinitialize(vmThread->riParameters);
#else
   const TR_PPCHWProfilerPMUConfig *configs = TR_PPCHWProfilerPMUConfig::getPMUConfigs();

   MTSPR(MMCR0, MMCR0_FC);
   MTSPR(BESCRRU, BESCRU_GE | BESCRU_PME | BESCRU_LME);
   TR_PPCHWProfilerEBBContext *context = (TR_PPCHWProfilerEBBContext*)getTCBContext();
   VERBOSE("Retrieved context=%p for terminating J9VMThread=%p.", context, vmThread);
   for (int i = 0; i < MAX_PMCS; ++i)
      {
#if defined(DEBUG) || defined(PPCRI_VERBOSE)
      VERBOSE("\tPMC[%u] count: %u", i + 1, context->eventCounts[i]);
#endif /* DEBUG || PPCRI_VERBOSE */
      if (context->buffers[i].start)
         freeBuffer(context->buffers[i].start, configs[context->currentConfig].bufferElementSize[i] * configs[context->currentConfig].bufferSize[i]);
      if (context->fds[i] >= 0 && close(context->fds[i]))
         VERBOSE("Failed to close perf interface for counter %u (fd=%d) on J9VMThread=%p, errno: %d, close : %s.", i + 1, context->fds[i], vmThread, errno, strerror(errno));
      }
#if defined(DEBUG) || defined(PPCRI_VERBOSE)
   VERBOSE("\tUnknown events: %u", context->unknownEventCount);
#endif /* DEBUG || PPCRI_VERBOSE */
   jitPersistentFree(context);
   setTCBContext(NULL);
   vmThread->riParameters->flags &= ~(J9PORT_RI_INITIALIZED | J9PORT_RI_ENABLED);
#endif
   vmThread->riParameters->controlBlock = NULL;

   return !IS_THREAD_RI_INITIALIZED(vmThread);
   }

