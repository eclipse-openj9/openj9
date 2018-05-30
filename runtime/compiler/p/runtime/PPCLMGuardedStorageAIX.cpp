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
#include "runtime/HWProfiler.hpp"

#include <dlfcn.h>
#include <pmapi.h>
#include <pthread.h>

// Defined by later versions of ebbapi.h
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

struct ebbapiInterface
   {
   void *dlHandle;

   int (*pm_initialize)(int filter, pm_info2_t *pminfo, pm_groups_info_t *pmgroups, int proctype);
   int (*pm_set_program_mythread)(tr_pm_prog_t *prog);
   int (*pm_delete_program_mythread)();
   int (*pm_start_mythread)();
   int (*pm_stop_mythread)();
   char* (*pm_strerror)(char *where, int error);
   int (*pm_get_data_mythread)(pm_data_t *data);
   // Private, not declared in ebbapi.h
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
static ebbapiInterface ebbapi;

static void ebbapiClose()
   {
   dlclose(ebbapi.dlHandle);
   ebbapi.dlHandle = NULL;
   }

static bool ebbapiInit()
   {
   if (ebbapi.dlHandle)
      return true;

#ifdef TR_HOST_64BIT
   ebbapi.dlHandle = dlopen("/usr/lib/libebbapi.a(shr_64.o)", RTLD_LAZY | RTLD_MEMBER);
#else /* TR_HOST_32BIT */
   ebbapi.dlHandle = dlopen("/usr/lib/libebbapi.a(shr.o)", RTLD_LAZY | RTLD_MEMBER);
#endif /* TR_HOST_32BIT */
   if (!ebbapi.dlHandle)
      {
      VERBOSE("Failed to open libebbapi.");
      goto fail;
      }

   char *curSym;
   curSym = "pm_initialize";
   ebbapi.pm_initialize = (int (*)(int, pm_info2_t *, pm_groups_info_t *, int))dlsym(ebbapi.dlHandle, curSym);
   if (!ebbapi.pm_initialize)
      goto ebbapiClose;
   curSym = "pm_set_program_mythread";
   ebbapi.pm_set_program_mythread = (int (*)(tr_pm_prog_t *))dlsym(ebbapi.dlHandle, curSym);
   if (!ebbapi.pm_set_program_mythread)
      goto ebbapiClose;
   curSym = "pm_delete_program_mythread";
   ebbapi.pm_delete_program_mythread = (int (*)())dlsym(ebbapi.dlHandle, curSym);
   if (!ebbapi.pm_delete_program_mythread)
      goto ebbapiClose;
   curSym = "pm_start_mythread";
   ebbapi.pm_start_mythread = (int (*)())dlsym(ebbapi.dlHandle, curSym);
   if (!ebbapi.pm_start_mythread)
      goto ebbapiClose;
   curSym = "pm_stop_mythread";
   ebbapi.pm_stop_mythread = (int (*)())dlsym(ebbapi.dlHandle, curSym);
   if (!ebbapi.pm_stop_mythread)
      goto ebbapiClose;
   curSym = "pm_set_ebb_handler";
   ebbapi.pm_set_ebb_handler = (int (*)(void *, void *))dlsym(ebbapi.dlHandle, curSym);
   if (!ebbapi.pm_set_ebb_handler)
      goto ebbapiClose;
   curSym = "pm_set_counter_frequency_mythread";
   ebbapi.pm_set_counter_frequency_mythread = (int (*)(unsigned int *))dlsym(ebbapi.dlHandle, curSym);
   if (!ebbapi.pm_set_counter_frequency_mythread)
      goto ebbapiClose;
   curSym = "pm_clear_ebb_handler";
   ebbapi.pm_clear_ebb_handler = (int (*)(void **, void **))dlsym(ebbapi.dlHandle, curSym);
   if (!ebbapi.pm_clear_ebb_handler)
      goto ebbapiClose;
   curSym = "pm_enable_bhrb";
   ebbapi.pm_enable_bhrb = (int (*)(tr_pm_bhrb_ifm_t))dlsym(ebbapi.dlHandle, curSym);
   if (!ebbapi.pm_enable_bhrb)
      goto ebbapiClose;
   curSym = "pm_disable_bhrb";
   ebbapi.pm_disable_bhrb = (int (*)())dlsym(ebbapi.dlHandle, curSym);
   if (!ebbapi.pm_disable_bhrb)
      goto ebbapiClose;
   curSym = "pm_strerror";
   ebbapi.pm_strerror = (char* (*)(char *, int))dlsym(ebbapi.dlHandle, curSym);
   if (!ebbapi.pm_strerror)
      goto ebbapiClose;
   curSym = "pm_get_data_mythread";
   ebbapi.pm_get_data_mythread = (int (*)(pm_data_t *))dlsym(ebbapi.dlHandle, curSym);
   if (!ebbapi.pm_get_data_mythread)
      goto ebbapiClose;

   return true;

ebbapiClose:
   VERBOSE("Failed to find required function symbol %s.", curSym);
   ebbapiClose();
fail:
   return false;
   }

TR_PPCLMGuardedStorage *
TR_PPCLMGuardedStorage::allocate(J9JITConfig *jitConfig, bool ebbSetupDone)
   {
   if (!ebbSetupDone)
      {
      if (!ebbapiInit())
         {
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
   TR_PPCHWProfilerEBBContext *context;
   if (!IS_THREAD_RI_INITIALIZED(vmThread))
      {
      return false;
      }
   else
      {
      context = (TR_PPCHWProfilerEBBContext*)(vmThread->riParameters->controlBlock);
      context->lmEventHandler = lmEventHandler;
      // clear both LME and LMEO bits
      //MTSPR(BESCRR, BESCR_LMEO);
      //MTSPR(BESCRRU, BESCRU_LME);
      return true;
      }
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
   TR_PPCHWProfilerEBBContext *context = (TR_PPCHWProfilerEBBContext*)(vmThread->riParameters->controlBlock);
   if (context)
      {
      VERBOSE("Retrieved context=%p for terminating J9VMThread=%p.", context, vmThread);
      jitPersistentFree(context);
      }
#endif
   return true;
   }

