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
#include "control/CompilationThread.hpp"

#include <dlfcn.h>
#include <pmapi.h>
#include <pthread.h>

// Defined by later versions of pmapi.h
typedef union {
  int   w;
  struct {
    unsigned threshold   :6;
    unsigned thresh_res  :1;
    unsigned runlatch    :1;
    unsigned hypervisor  :1;
    unsigned thresh_hres :1;
    unsigned nointerrupt :1;
    unsigned wpar_all    :1;
    unsigned no_inherit  :1;
    unsigned spare       :13;
    unsigned is_group    :1;
    unsigned process     :1;
    unsigned kernel      :1;
    unsigned user        :1;
    unsigned count       :1;
    unsigned proctree    :1;
  } b;
} tr_pm_mode_t;

typedef struct {
  tr_pm_mode_t  mode;
  int           events[MAX_COUNTERS];
  long          reserved;
} tr_pm_prog_t;

struct pmapiInterface
   {
   void *dlHandle;

   int (*pm_initialize)(int filter, pm_info2_t *pminfo, pm_groups_info_t *pmgroups, int proctype);
   int (*pm_set_program_mythread)(tr_pm_prog_t *prog);
   int (*pm_delete_program_mythread)();
   int (*pm_start_mythread)();
   int (*pm_stop_mythread)();
   char* (*pm_strerror)(char *where, int error);
   int (*pm_get_data_mythread)(pm_data_t *data);
   // Private, not declared in pmapi.h
   int (*pm_set_ebb_handler)(void *handler_address, void *data_area);
   // This function expects an array of MAX_COUNTERS values
   int (*pm_set_counter_frequency_mythread)(unsigned int *freqs);
   int (*pm_clear_ebb_handler)(void **old_handler, void **old_data_area);
   int (*pm_enable_bhrb)(tr_pm_bhrb_ifm_t ifm_mode);
   int (*pm_disable_bhrb)();

   pm_info2_t pminfo;
   pm_groups_info_t pmgroups;
   };

// Not thread-safe, but we only ever expect to initialize this once, from the thread that creates the HWP.
// All threads can read it freely after it's initialized.
static pmapiInterface pmapi;

static void pmapiClose()
   {
   dlclose(pmapi.dlHandle);
   pmapi.dlHandle = NULL;
   }

static bool pmapiInit()
   {
   if (pmapi.dlHandle)
      return true;

#ifdef TR_HOST_64BIT
   pmapi.dlHandle = dlopen("/usr/lib/libpmapi.a(shr_64.o)", RTLD_LAZY | RTLD_MEMBER);
#else /* TR_HOST_32BIT */
   pmapi.dlHandle = dlopen("/usr/lib/libpmapi.a(shr.o)", RTLD_LAZY | RTLD_MEMBER);
#endif /* TR_HOST_32BIT */
   if (!pmapi.dlHandle)
      {
      VERBOSE("Failed to open libpmapi.");
      goto fail;
      }

   char *curSym;
   curSym = "pm_initialize";
   pmapi.pm_initialize = (int (*)(int, pm_info2_t *, pm_groups_info_t *, int))dlsym(pmapi.dlHandle, curSym);
   if (!pmapi.pm_initialize)
      goto pmapiClose;
   curSym = "pm_set_program_mythread";
   pmapi.pm_set_program_mythread = (int (*)(tr_pm_prog_t *))dlsym(pmapi.dlHandle, curSym);
   if (!pmapi.pm_set_program_mythread)
      goto pmapiClose;
   curSym = "pm_delete_program_mythread";
   pmapi.pm_delete_program_mythread = (int (*)())dlsym(pmapi.dlHandle, curSym);
   if (!pmapi.pm_delete_program_mythread)
      goto pmapiClose;
   curSym = "pm_start_mythread";
   pmapi.pm_start_mythread = (int (*)())dlsym(pmapi.dlHandle, curSym);
   if (!pmapi.pm_start_mythread)
      goto pmapiClose;
   curSym = "pm_stop_mythread";
   pmapi.pm_stop_mythread = (int (*)())dlsym(pmapi.dlHandle, curSym);
   if (!pmapi.pm_stop_mythread)
      goto pmapiClose;
   curSym = "pm_set_ebb_handler";
   pmapi.pm_set_ebb_handler = (int (*)(void *, void *))dlsym(pmapi.dlHandle, curSym);
   if (!pmapi.pm_set_ebb_handler)
      goto pmapiClose;
   curSym = "pm_set_counter_frequency_mythread";
   pmapi.pm_set_counter_frequency_mythread = (int (*)(unsigned int *))dlsym(pmapi.dlHandle, curSym);
   if (!pmapi.pm_set_counter_frequency_mythread)
      goto pmapiClose;
   curSym = "pm_clear_ebb_handler";
   pmapi.pm_clear_ebb_handler = (int (*)(void **, void **))dlsym(pmapi.dlHandle, curSym);
   if (!pmapi.pm_clear_ebb_handler)
      goto pmapiClose;
   curSym = "pm_enable_bhrb";
   pmapi.pm_enable_bhrb = (int (*)(tr_pm_bhrb_ifm_t))dlsym(pmapi.dlHandle, curSym);
   if (!pmapi.pm_enable_bhrb)
      goto pmapiClose;
   curSym = "pm_disable_bhrb";
   pmapi.pm_disable_bhrb = (int (*)())dlsym(pmapi.dlHandle, curSym);
   if (!pmapi.pm_disable_bhrb)
      goto pmapiClose;
   curSym = "pm_strerror";
   pmapi.pm_strerror = (char* (*)(char *, int))dlsym(pmapi.dlHandle, curSym);
   if (!pmapi.pm_strerror)
      goto pmapiClose;
   curSym = "pm_get_data_mythread";
   pmapi.pm_get_data_mythread = (int (*)(pm_data_t *))dlsym(pmapi.dlHandle, curSym);
   if (!pmapi.pm_get_data_mythread)
      goto pmapiClose;

   return true;

pmapiClose:
   VERBOSE("Failed to find required function symbol %s.", curSym);
   pmapiClose();
fail:
   return false;
   }

int findEventGroup(const pm_info2_t& pminfo, const pm_groups_info_t& pmgroups, const char* const events[], int filter)
   {
   for (int g = 0; g < pmgroups.maxgroups; ++g)
      {
      int c;
      for (c = 0; c < MAX_PMCS; ++c)
         {
         // If null, caller doesn't care what this PMC counts
         if (!events[c])
            continue;
         // Check if this PMC is functional
         if (pminfo.maxevents[c] == 0)
            break;
         int ev = pmgroups.event_groups[g].events[c];
         if (ev == COUNT_NOTHING)
            break;
         const pm_events2_t *event = &pminfo.list_events[c][ev];
         // Check the event against our filter
         if (!(event->status.b.verified && (filter & PM_VERIFIED) ||
               event->status.b.unverified && (filter & PM_UNVERIFIED) ||
               event->status.b.caveat && (filter & PM_CAVEAT)))
            break;
         // Check if this groups counts the event the user is interested in on this PMC
         if (strcmp(event->short_name, events[c]))
            break;
         }
      if (c == MAX_PMCS)
         return g;
      }
   return -1;
   }

static bool pmapiInitPM()
   {
   TR_PPCHWProfilerPMUConfig *configs = TR_PPCHWProfilerPMUConfig::getPMUConfigs();

   const int filter = PM_VERIFIED | PM_UNVERIFIED | PM_CAVEAT;
   int       rc;
   if (rc = pmapi.pm_initialize(filter | PM_GET_GROUPS, &pmapi.pminfo, &pmapi.pmgroups, PM_CURRENT))
      {
      VERBOSE("Failed to initialize PMAPI, rc: %d, %s", rc, pmapi.pm_strerror("pm_initialize", rc));
      goto fail;
      }

   for (int i = 0; i < NumProfilingConfigs; ++i)
      {
      int group = findEventGroup(pmapi.pminfo, pmapi.pmgroups, configs[i].config, filter);
      if (group < 0)
         {
         VERBOSE("Config %u: Could not find a group containing the requested events.", i);
         goto fail;
         }

      TR_ASSERT(group < pmapi.pmgroups.maxgroups, "Group out of range");

      configs[i].group = group;

      VERBOSE("Config %u mapped to group %u:", i, group);
      for (int c = 0; c < MAX_PMCS; ++c)
         {
         const pm_events2_t *event = &pmapi.pminfo.list_events[c][pmapi.pmgroups.event_groups[group].events[c]];
         VERBOSE("PMC[%u]: %s (%s)", c + 1, event->short_name, event->status.b.verified ? "verified" : event->status.b.unverified ? "unverified" : event->status.b.caveat ? "caveat" : "unknown status");
         }
      }

   return true;

fail:
   pmapiClose();
   return false;
   }

TR_PPCHWProfiler *
TR_PPCHWProfiler::allocate(J9JITConfig *jitConfig)
   {
   if (!pmapiInit())
      {
      return NULL;
      }
   if (!pmapiInitPM())
      {
      return NULL;
      }

   TR_PPCHWProfiler *profiler = new (PERSISTENT_NEW) TR_PPCHWProfiler(jitConfig);
   VERBOSE("HWProfiler initialized.");
   return profiler;
   }

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
   tr_pm_prog_t                prog;
   TR_PPCHWProfilingConfigs    currentConfig = MethodHotness;
   bool                        setUnavailableOnFail = true;

#if defined(DEBUG) || defined(PPCRI_VERBOSE)
   VERBOSE("Initializing PMU on J9VMThread=%p, tid=%u.", vmThread, thread_self());
#else /* !(DEBUG || PPCRI_VERBOSE) */
   VERBOSE("Initializing PMU on J9VMThread=%p.", vmThread);
#endif /* !(DEBUG || PPCRI_VERBOSE) */

   memset(&prog, 0, sizeof(tr_pm_prog_t));
   prog.mode.b.user = 1;
   prog.mode.b.is_group = 1;
   prog.mode.b.runlatch = 1;
   prog.mode.b.nointerrupt = 1;
   prog.mode.b.no_inherit = 1;
   prog.events[0] = configs[currentConfig].group;

   int rc = pmapi.pm_set_program_mythread(&prog);
   if (rc != 0 && rc != 59)
      {
      VERBOSE("Failed to set PMAPI program for J9VMThread=%p, rc: %d, %s", vmThread, rc, pmapi.pm_strerror("pm_set_program_mythread", rc));
      goto fail;
      }

   // Check if the thread inherited a PM context and try to stop it
   // This shouldn't happen if PMAPI supports the no_inherit flag, but earlier versions don't and need this workaround
   if (rc == 59)
      {
      VERBOSE("PMAPI program already exists for J9VMThread=%p.");
      rc = pmapi.pm_stop_mythread();
      // If the return code is 85, counting was never started on the thread and we can still proceed
      if (rc != 0 && rc != 85)
         {
         VERBOSE("Failed to stop counting on J9VMThread=%p, rc: %d, %s", vmThread, rc, pmapi.pm_strerror("pm_stop_mythread", rc));
         goto fail;
         }
      if (rc = pmapi.pm_delete_program_mythread())
         {
         VERBOSE("Failed to delete PMAPI program for J9VMThread=%p, rc: %d, %s", vmThread, rc, pmapi.pm_strerror("pm_delete_program_mythread", rc));
         goto fail;
         }
      if (rc = pmapi.pm_set_program_mythread(&prog))
         {
         VERBOSE("Failed to set PMAPI program for J9VMThread=%p, rc: %d, %s", vmThread, rc, pmapi.pm_strerror("pm_set_program_mythread", rc));
         goto fail;
         }
      }

   void *bufs[MAX_PMCS];
   memset(bufs, 0, sizeof(void*) * MAX_PMCS);
   for (int i = 0; i < MAX_PMCS; ++i)
      {
      if (configs[currentConfig].sampleRates[i] == INVALID_SAMPLERATE)
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
      context->buffers[i].start = bufs[i];
      context->buffers[i].spaceLeft = configs[currentConfig].bufferSize[i];
      context->counterBitMask |= (bufs[i] ? (1 << i) : 0);
      }
   context->unknownEventCount = 0;
   context->lostPMU = false;

   uint32_t sampleRates[MAX_COUNTERS];
   for (int i = 0; i < MAX_PMCS; ++i)
      sampleRates[i] = 0x80000000;
   if (rc = pmapi.pm_set_counter_frequency_mythread(sampleRates))
      {
      VERBOSE("Failed to set PMU counter frequencies for J9VMThread=%p, rc: %d, %s", vmThread, rc, pmapi.pm_strerror("pm_set_counter_frequency_mythread", rc));
      goto freecontext;
      }
   if (rc = pmapi.pm_set_ebb_handler(FUNCTION_ADDRESS(ebbHandler), context))
      {
      VERBOSE("Failed to set EBB handler for J9VMThread=%p, rc: %d, %s", vmThread, rc, pmapi.pm_strerror("pm_set_ebb_handler", rc));
      goto freecontext;
      }
   if (rc = pmapi.pm_enable_bhrb(configs[currentConfig].bhrbFilter))
      {
      VERBOSE("Failed to enable BHRB for J9VMThread=%p, rc: %d, %s", vmThread, rc, pmapi.pm_strerror("pm_enable_bhrb", rc));
      goto clearebbh;
      }
   if (rc = pmapi.pm_start_mythread())
      {
      VERBOSE("Failed to start PMU counting on J9VMThread=%p, rc: %d, %s", vmThread, rc, pmapi.pm_strerror("pm_start_mythread", rc));
      goto disablebhrb;
      }

   // Failures from this point on can only be due to temporary loss of the PMU, so don't disable HWP completely
   setUnavailableOnFail = false;

   // goto bypasses decl of freeze
      {
      const uint64_t freeze = MMCR0_FC;
      if (tr_mmcr_write(MMCR0, &freeze)) goto stopcounting;
      }

   for (int i = 0; i < MAX_PMCS; ++i)
      sampleRates[i] = configs[currentConfig].sampleRates[i] != INVALID_SAMPLERATE ? 0x80000000 - context->sampleRates[i] : 0;
   if (tr_pmc_write(PMC1, &sampleRates[0])) goto stopcounting;
   if (tr_pmc_write(PMC2, &sampleRates[1])) goto stopcounting;
   if (tr_pmc_write(PMC3, &sampleRates[2])) goto stopcounting;
   if (tr_pmc_write(PMC4, &sampleRates[3])) goto stopcounting;
   if (tr_pmc_write(PMC5, &sampleRates[4])) goto stopcounting;
   if (tr_pmc_write(PMC6, &sampleRates[5])) goto stopcounting;
   // Freeze the counters that we're not using
   uint64_t mmcr2;
   TR_PPCHWProfilerPMUConfig::calcMMCR2ForConfig(mmcr2, currentConfig);
   if (tr_mmcr_write(MMCR2, &mmcr2)) goto stopcounting;

   if (configs[currentConfig].vmFlags)
      {
      vmThread->privateFlags |= J9_PRIVATE_FLAGS_RI_INITIALIZED;
      vmThread->jitCurrentRIFlags |= configs[currentConfig].vmFlags;
      }
   else
      {
      const uint64_t unfreeze = MMCR0_PMAE;
      if (tr_mmcr_write(MMCR0, &unfreeze)) goto stopcounting;
      vmThread->riParameters->flags |= J9PORT_RI_ENABLED;
      }

   vmThread->riParameters->controlBlock = context;
   vmThread->riParameters->flags |= J9PORT_RI_INITIALIZED;

   if (IS_THREAD_RI_INITIALIZED(vmThread))
      {
      VERBOSE("J9VMThread=%p, initialized for HW profiling.", vmThread);
      return true;
      }
stopcounting:
   VERBOSE("Lost PMU access during initialization on J9VMThread=%p, will retry later.", vmThread);
   if (rc = pmapi.pm_stop_mythread())
      VERBOSE("Failed to stop counting on J9VMThread=%p, rc: %d, %s", vmThread, rc, pmapi.pm_strerror("pm_stop_mythread", rc));
disablebhrb:
   if (rc = pmapi.pm_disable_bhrb())
      VERBOSE("Failed to disable BHRB for J9VMThread=%p, rc: %d, %s", vmThread, rc, pmapi.pm_strerror("pm_delete_program_mythread", rc));
clearebbh:
   if (rc = pmapi.pm_clear_ebb_handler(NULL, NULL))
      VERBOSE("Failed to clear EBB handler for J9VMThread=%p, rc: %d, %s", vmThread, rc, pmapi.pm_strerror("pm_delete_program_mythread", rc));
freecontext:
   jitPersistentFree(context);
freebufs:
   for (int i = 0; i < MAX_PMCS; ++i)
      {
      if (bufs[i])
         freeBuffer(bufs[i], configs[currentConfig].bufferElementSize[i] * configs[currentConfig].bufferSize[i]);
      }
deleteprog:
   if (rc = pmapi.pm_delete_program_mythread())
      VERBOSE("Failed to delete PMAPI program for J9VMThread=%p, rc: %d, %s", vmThread, rc, pmapi.pm_strerror("pm_delete_program_mythread", rc));
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

   const uint64_t freeze = MMCR0_FC;
   tr_mmcr_write(MMCR0, &freeze);
   int rc;
   if (rc = pmapi.pm_stop_mythread())
      VERBOSE("Failed to stop counting on J9VMThread=%p, rc: %d, %s", vmThread, rc, pmapi.pm_strerror("pm_stop_mythread", rc));
   TR_PPCHWProfilerEBBContext *context;
   if (rc = pmapi.pm_clear_ebb_handler(NULL, (void**)&context))
      VERBOSE("Failed to clear EBB handler for J9VMThread=%p, rc: %d, %s", vmThread, rc, pmapi.pm_strerror("pm_clear_ebb_handler", rc));
   else
      VERBOSE("Retrieved context=%p for terminating J9VMThread=%p.", context, vmThread);
   for (int i = 0; i < MAX_PMCS; ++i)
      {
#if defined(DEBUG) || defined(PPCRI_VERBOSE)
      VERBOSE("\tPMC[%u] count: %u", i + 1, context->eventCounts[i]);
#endif /* DEBUG || PPCRI_VERBOSE */
      if (context->buffers[i].start)
         freeBuffer(context->buffers[i].start, configs[context->currentConfig].bufferElementSize[i] * configs[context->currentConfig].bufferSize[i]);
      }
#if defined(DEBUG) || defined(PPCRI_VERBOSE)
   VERBOSE("\tUnknown events: %u", context->unknownEventCount);
#endif /* DEBUG || PPCRI_VERBOSE */
   jitPersistentFree(context);
   if (rc = pmapi.pm_delete_program_mythread())
      VERBOSE("Failed to delete PMAPI program for J9VMThread=%p, rc: %d, %s", vmThread, rc, pmapi.pm_strerror("pm_delete_program_mythread", rc));

   vmThread->riParameters->flags &= ~J9PORT_RI_INITIALIZED;
#endif
   vmThread->riParameters->controlBlock = NULL;

   return !IS_THREAD_RI_INITIALIZED(vmThread);
   }

