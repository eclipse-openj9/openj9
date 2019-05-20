/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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

#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>

#include "ute_core.h"
#include "rastrace_internal.h"
#include "omrutil.h"
#include "j9trcnls.h"
#include "j9.h"

#define UT_TRACE_SLEEPTIME_DEFAULT_MILLIS 30000

/* Timeout value for trace shutdown. This is the maximum length of time we wait for trace subscribers
 * to finish. Set as a long safety of 2 minutes, on the basis that a hang is likely by then, and we
 * can issue a diagnostic message and exit before other hang detection in the stack kills the process.
 */
#define UT_TRACE_SHUTDOWN_TIMEOUT_MILLIS 120000

static const char* UT_NO_THREAD_NAME = "MISSING";

/* Structures for trace interfaces.
 * The public interface is returned to the language layer, and may be
 * modified. The private interface is not and can be considered safe.
 */
static UtInterface			externalUtIntfS;
UtInterface			internalUtIntfS;

/* Structures for trace interfaces. */
static UtServerInterface	utServerIntfS;
static UtModuleInterface	utModuleIntfS;

UtGlobalData       *utGlobal = NULL;
extern J9JavaVM    *globalVM;
omrthread_tls_key_t j9rasTLSKey;
omrthread_tls_key_t j9uteTLSKey;

static omr_error_t trcFlushTraceData(UtThreadData **thr, UtTraceBuffer **first, UtTraceBuffer **last, int32_t pause);
static omr_error_t trcGetTraceMetadata(void **data, int32_t *length);
static omr_error_t trcRegisterTracePointSubscriber(UtThreadData **thr, char *description, utsSubscriberCallback subscriber, utsSubscriberAlarmCallback alarm, void *userData, UtSubscription **subscriptionReference);
static omr_error_t trcDeregisterTracePointSubscriber(UtThreadData **thr, UtSubscription *subscriptionID);
static omr_error_t trcTraceSnap(UtThreadData **thr, char *label, char **response);
static omr_error_t trcTraceSnapWithPriority(UtThreadData **thr, char *label, int32_t snapPriority, char **response, int32_t sync);
static omr_error_t internalTraceSnapWithPriority(UtThreadData **thr, char *label, int32_t snapPriority, char **response, int32_t sync);
static omr_error_t trcAddComponent(UtModuleInfo *modInfo, const char **format) ;
static omr_error_t trcGetComponents(UtThreadData **thr, char ***list, int32_t *number);
static omr_error_t trcGetComponent(char *name, unsigned char **bitMap, int32_t *first, int32_t *last);
static omr_error_t trcTraceRegister(UtThreadData **thr, UtListenerWrapper func, void *userData);
static omr_error_t trcTraceDeregister(UtThreadData **thr, UtListenerWrapper func, void *userData);
static void trcDisableTrace(int32_t type);
static void trcEnableTrace(int32_t type);
static omr_error_t trcSetOptions(UtThreadData **thr, const char *opts[]);
static void omrTraceInit (void* env, UtModuleInfo *modInfo);
static void omrTraceTerm (void* env, UtModuleInfo *modInfo);

static void setStartTime(void);

/*******************************************************************************
 * name        - startTraceWorkerThread
 * description - Start a worker thread to write data to disk
 * parameters  - UtThreadData
 * returns     - OMR_ERROR_INTERNAL if failure, OMR_ERROR_NONE if success
 ******************************************************************************/
omr_error_t
startTraceWorkerThread(UtThreadData **thr)
{
	/* This function's present to keep the separation of function and interface
	 * between ut_main.c and ut_trace.c
	 */
	omr_error_t rc = OMR_ERROR_NONE;

	if (!UT_GLOBAL(traceInCore)) {
		rc = setupTraceWorkerThread(thr);
	}

	if (OMR_ERROR_NONE == rc) {
		UT_GLOBAL(traceInitialized) = TRUE;
	}
	return rc;
}

/**
 * Register an external trace listener
 * @param[in] thr UtThreadData
 * @param[in] func Listener function pointer
 * @param[in] userData Data passed to func
 * @return OMR error code
 */
static omr_error_t
trcTraceRegister(UtThreadData **thr, UtListenerWrapper func, void *userData)
{
	UtTraceListener *this;
	UtTraceListener *next;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_DBGOUT(1, ("<UT> TraceRegister entered. Func: " UT_POINTER_SPEC "\n", func));
	this = (UtTraceListener *)j9mem_allocate_memory(sizeof(UtTraceListener), OMRMEM_CATEGORY_TRACE);
	if (this == NULL) {
		UT_DBGOUT(1, ("<UT> Out of memory in trcTraceRegister\n"));
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
	initHeader(&this->header, UT_TRACE_LISTENER_NAME, sizeof(UtTraceListener));
	this->listener = func;
	this->userData = userData;
	this->next = NULL;
	getTraceLock(thr);
	if (UT_GLOBAL(traceListeners) == NULL) {
		UT_GLOBAL(traceListeners) = this;
	} else {
		for (next = UT_GLOBAL(traceListeners); next != NULL;
			next = next->next) {
			if (next->next == NULL) {
				next->next = this;
				break;
			}
		}
	}
	freeTraceLock(thr);
	return OMR_ERROR_NONE;
}

/**
 * Deregister an external trace listener
 * @param[in] thr UtThreadData
 * @param[in] func Listener function pointer
 * @param[in] userData Data passed to func
 * @return OMR error code
 */
static omr_error_t
trcTraceDeregister(UtThreadData **thr, UtListenerWrapper func, void *userData)
{
	UtTraceListener **prev;
	UtTraceListener *next;
	omr_error_t	rc = OMR_ERROR_NONE;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_DBGOUT(1, ("<UT> TraceDeregister entered. Func: " UT_POINTER_SPEC "\n", func));
	getTraceLock(thr);
	prev = &UT_GLOBAL(traceListeners);
	for (next = UT_GLOBAL(traceListeners); next != NULL;
		 next = next->next) {
		if ((next->listener == func) && (next->userData == userData)) {
			*prev = next->next;
			j9mem_free_memory(next);
			break;
		}
		prev = &next->next;
	}
	freeTraceLock(thr);
	rc = (next == NULL) ? OMR_ERROR_ILLEGAL_ARGUMENT : OMR_ERROR_NONE;
	return rc;
}

static void
omrTraceInit(void *env, UtModuleInfo *modInfo)
{
	moduleLoaded(UT_THREAD_FROM_ENV(env), modInfo);
}

/*******************************************************************************
 * name        - moduleLoaded
 * description - Initialize tracing for a loaded module
 * parameters  - UtThreadData,  UtModuleInfo pointer
 * returns     - int32_t
 ******************************************************************************/
omr_error_t
moduleLoaded(UtThreadData **thr, UtModuleInfo *modInfo)
{
	UtComponentData *compData = NULL;
	omr_error_t rc = OMR_ERROR_NONE;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	/*
	 * Ensure we have a thread pointer
	 */
	if (thr == NULL) {
		thr = twThreadSelf();
	}
	if (*thr == NULL || modInfo == NULL) {
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	UT_DBGOUT(1, ("<UT> ModuleLoaded: %s\n", modInfo->name));

	if (modInfo->traceVersionInfo == NULL){
		/* this is a pre 142 module - not compatible with this trace engine fail silently to register this module */
		UT_DBGOUT(1, ("<UT> ModuleLoaded refusing registration to %s because it's version is less than the supported UT version %d\n", modInfo->name, UT_VERSION));
		return OMR_ERROR_NONE;
	} /* else {
		this field contains the version number and can be used to modify behaviour based on level of module loaded
	} */

	getTraceLock(thr);

	if (modInfo->intf == NULL) {
		modInfo->intf = internalUtIntfS.module;

		rc = initializeComponentData(&compData, modInfo, modInfo->name);
		if (OMR_ERROR_NONE == rc) {
			rc = addComponentToList(compData, UT_GLOBAL(componentList));
		}
		if (OMR_ERROR_NONE == rc) {
			rc = processComponentDefferedConfig(compData, UT_GLOBAL(componentList));
		}
		if (OMR_ERROR_NONE != rc) {
			/* Module not configured for trace: %s */
			j9nls_printf(PORTLIB, J9NLS_WARNING | J9NLS_STDERR, J9NLS_TRC_MODULE_NOT_LOADED, modInfo->name);
			freeTraceLock(thr);
			return OMR_ERROR_INTERNAL;
		}
	} else {
		/* guard against stale pointers */
		modInfo->intf = internalUtIntfS.module;
		/* this module has already been registered. Just increment the reference count */
		modInfo->referenceCount += 1;
	}

	freeTraceLock(thr);

	UT_DBGOUT(1, ("<UT> ModuleLoaded: %s, interface: "
					UT_POINTER_SPEC "\n", modInfo->name, modInfo->intf));
	return OMR_ERROR_NONE;
}

static void
omrTraceTerm(void* env, UtModuleInfo *modInfo)
{
	UtThreadData **thr = UT_THREAD_FROM_ENV(env);
	if (thr == NULL){
		/* unloading a dead module - safe to ignore this */
		return;
	}
	/* moduleUnLoading() handles the case when *thr is NULL */
	moduleUnLoading(thr, modInfo);
}

/*******************************************************************************
 * name        - moduleUnLoading
 * description - Terminate tracing for an module
 * parameters  - UtThreadData, UtModuleInfo pointer
 * returns     - void
 ******************************************************************************/
omr_error_t
moduleUnLoading(UtThreadData **thr, UtModuleInfo *modInfo)
{
	int32_t i;
	omr_error_t rc = OMR_ERROR_NONE;

	if (utGlobal == NULL || UT_GLOBAL(traceFinalized)){
		return OMR_ERROR_INTERNAL;
	}
	/*
	 */
	if (thr == NULL) {
		thr = twThreadSelf();
	}
	if (*thr == NULL) {
		if( modInfo != NULL ) {
			/* Unloading a module that was not initialized correctly */

			/* This is a little dangerous because we reach for ->count and ->active
			 * here and all other uses of those fields is in the ut_runtimedata.c file.
			 *
			 * The trace code is generally carefully wrapped to check for *thr == NULL,
			 * so having trace enabled, but *thr == NULL is not a problem.
			 *
			 * During runtime shutdown - when we unload DLLs, the *thr check is not
			 * sufficient, we need to avoid trying to call any functions in the trace DLL.
			 *
			 * Forcefully clearing the active array to zero ensures that any module calling
			 * this function will not have trace enabled - even if the *thr == NULL, usually
			 * indicating some sort of problem with initialization of that field.
			 */
			for(i=0;i<modInfo->count;i++) {
				modInfo->active[i] = 0;	/* force disable of trace */
			}
		}
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	if (modInfo == NULL) {
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	UT_DBGOUT(1, ("<UT> ModuleUnloading: %s\n", modInfo->name));

	if (modInfo->traceVersionInfo == NULL){
		/* this is a pre 142 module - not compatible with this trace engine fail silently to register this module */
		UT_DBGOUT(1, ("<UT> ModuleLoaded refusing deregistration to %s because it's version is less than the supported UT version %d\n", modInfo->name, UT_VERSION));
		return OMR_ERROR_NONE;
	} /* else {
		this field contains the version number and can be used to modify behaviour based on level of module loaded
	} */


	getTraceLock(thr);

	if (modInfo->referenceCount > 0) {
		modInfo->referenceCount -= 1;
	} else {
		rc = setTracePointsTo(modInfo->name, UT_GLOBAL(componentList), TRUE, 0, 0, 0, -1, NULL, FALSE, TRUE);
		if (OMR_ERROR_NONE != rc) {
			UT_DBGOUT(1, ("<UT> problem turning off trace in %s as it unloads\n", modInfo->name));
			/* proceed to remove it from list in case it has gone and we try and manipulate it's control array */
		}
		rc = removeModuleFromList(modInfo, UT_GLOBAL(componentList));
	}

	freeTraceLock(thr);

	return  rc;
}

/*******************************************************************************
 * name        - internalTraceSnapWithPriority
 * description - Take a snapshot of the current trace buffers
 * parameters  - UtThreadData, label, priority, response, sync
 * returns     - OMR_ERROR_NONE or OMR_ERROR_INTERNAL
 ******************************************************************************/
static omr_error_t
internalTraceSnapWithPriority(UtThreadData **thr, char *label, int32_t snapPriority, char **response, int32_t sync)
{
	uint32_t		 	oldFlags;
	uint32_t		 	newFlags;
	omr_error_t		result = OMR_ERROR_NONE;
	char			*sink = "";
	/* These UtThreadData locals are only used if we're passed a null thr or *thr */
	UtThreadData thrData;
	UtThreadData *thrSlot = &thrData;

	if (response == NULL) {
		response = &sink;
	}

	if (thr == NULL || *thr == NULL) {
		/* fake up a UtThreadData just for use in snapping */
		thr = &thrSlot;
		thrData.recursion = 1;
	}

	UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> entered snap\n", thr));

	/* if trace has finalized the trace write thread will have gone away and any
	 * attempt at a snap is futile. (there won't be anything to snap either).
	 */
	if (UT_GLOBAL(traceFinalized) == TRUE){
		UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> not snapping because trace is terminated\n", thr));
		*response = "{trace terminated - snap not available}";
		return OMR_ERROR_INTERNAL;
	}

	/*
	 *  If there are any buffers at all
	 */
	if (UT_GLOBAL(traceGlobal) != NULL) {

		/*
		 *  Set the snap flag to true,
		 */
		do {
			oldFlags = UT_GLOBAL(traceSnap);
			newFlags = oldFlags | TRUE;
		} while (!twCompareAndSwap32(&UT_GLOBAL(traceSnap),
									  oldFlags,
									  newFlags));
		/*
		 * If the snap flag wasn't already on....
		 */
		if (!oldFlags) {
			UtTraceBuffer *start = NULL;
			UtTraceBuffer *stop = NULL;

			UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> flushing trace data for snap\n", thr));
			trcFlushTraceData(thr, &start, &stop, TRUE);

			/*
			 *  Only schedule snap if an active buffer was found
			 */
			if (start != NULL) {
				/* inform subscribers that buffers were queued */
				notifySubscribers(&UT_GLOBAL(outputQueue));

				if (!UT_GLOBAL(externalTrace)) {
					omr_error_t result = OMR_ERROR_NONE;
					UtSubscription *subscription;

					UT_GLOBAL(snapFile) = openSnap(label);

					UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> Starting Snap write thread, start: " UT_POINTER_SPEC ", stop: " UT_POINTER_SPEC "\n", thr, start, stop));

					/* Spawn a snap writer with it's subscription as its userData*/
					/* TODO: if a crash occurs during registration/deregistration the the trace lock is already held and this
					 * can hang. We need to check to see if this thread is already the owner of the trace lock before trying
					 * to snap dump in this fashion.
					 */
					result = trcRegisterRecordSubscriber(thr, "Snap Dump Thread", writeSnapBuffer, cleanupSnapDumpThread, NULL, start, stop, &subscription, FALSE);

					if (OMR_ERROR_NONE == result) {
						subscription->threadPriority = snapPriority;
						subscription->userData = sync ? (void *)1 : NULL;
					} else {
						/* Snap thread could not be started, need to clean up. */
						PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

						/* Mark snap as not in progress otherwise we may wait for it to terminate below */
						do {
							oldFlags = UT_GLOBAL(traceSnap);
							newFlags = oldFlags & ~TRUE;
						} while (!twCompareAndSwap32(&UT_GLOBAL(traceSnap), oldFlags, newFlags));

						/* Close the file we opened */
						j9file_close(UT_GLOBAL(snapFile));
					}

					/* Wrote snap to the provided filename */
					*response = label;
				} else {
					do {
						oldFlags = UT_GLOBAL(traceSnap);
						newFlags = oldFlags & ~TRUE;
					} while (!twCompareAndSwap32(&UT_GLOBAL(traceSnap), oldFlags, newFlags));

					/* tell them it was flushed to the external trace file */
					*response = UT_GLOBAL(traceFilename);
				}

				/* unblock the start buffer */
				UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> snap unpausing write queue at "UT_POINTER_SPEC"\n", thr, &start->queueData));
				resumeDequeueAtMessage(&start->queueData);

				if (sync) {
					/* Wait for the snap dump to finish (e.g. for fatal events, when the JVM is terminating) */
					while (!twCompareAndSwap32(&UT_GLOBAL(traceSnap), FALSE, FALSE)) {
						UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> waiting for snap dump thread to complete\n", thr));
						omrthread_sleep(100);
					}
				}
			} else {
				do {
					oldFlags = UT_GLOBAL(traceSnap);
					newFlags = oldFlags & ~TRUE;
				} while (!twCompareAndSwap32(&UT_GLOBAL(traceSnap), oldFlags, newFlags));

				*response = "{nothing to snap}";
				result = OMR_ERROR_NONE;
			}
		} else {
			UT_DBGOUT(1, ("<UT> Snap requested when one is already in progress, therefore ignoring it (no data will be lost)\n"));

			/*
			 * Already snapping to another filename
			 */
			*response = "{snap already in progress}";
			result = OMR_ERROR_INTERNAL;
		}
	} else {
		/*
		 * Nothing to snap => no filename
		 */
		*response = "{nothing to snap}";
		result = OMR_ERROR_NONE;
	}

	return result;
}

/*******************************************************************************
 * name        - trcTraceSnap
 * description - Take a snapshot of the current trace buffers
 * parameters  - UtThreadData, label
 * returns     - label used to snap buffers
 ******************************************************************************/
static omr_error_t
trcTraceSnap(UtThreadData **thr, char *label, char **response)
{
	return internalTraceSnapWithPriority(thr, label, UT_TRACE_WRITE_PRIORITY, response, FALSE);
}

/*******************************************************************************
 * name        - trcTraceSnapWithPriority
 * description - Take a snapshot of the current trace buffers
 * parameters  - UtThreadData, label, priority, response, sync
 * returns     - OMR_ERROR_NONE or OMR_ERROR_INTERNAL
 ******************************************************************************/
static omr_error_t
trcTraceSnapWithPriority(UtThreadData **thr, char *label, int32_t snapPriority, char **response, int32_t sync)
{
	int32_t detachRequired = FALSE;
	omr_error_t ret = OMR_ERROR_NONE;
	UtThreadData *thrSlot = NULL;

	/* Temporarily attach non-VM threads to UTE (ie. SigQuit thread) */
	if (NULL == thr) {
		thr = &thrSlot;
		twThreadAttach(thr, "UTE snap thread");
		detachRequired = TRUE;
	}

	ret = internalTraceSnapWithPriority(thr, label, snapPriority, response, sync);

	if (detachRequired) {
		twThreadDetach(thr);
	}

	return ret;
}

/*******************************************************************************
 * name        - threadStop
 * description - Handle Thread termination
 * parameters  - UtThreadData
 * returns     - void
 ******************************************************************************/
omr_error_t
threadStop(UtThreadData **thr)
{
	UtTraceBuffer *trcBuf;
	UtThreadData  *tempThr = *thr;
	UtThreadData   savedThr;
    UtThreadData  *stackThrP = &savedThr;
	uint32_t         oldCount;
	uint32_t         newCount;
	J9rasTLS * tls;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	if (utGlobal == NULL) {			/* very fatal! */
		if (thr != NULL) {
			*thr = NULL;
		}
		return OMR_ERROR_INTERNAL;
	}

	UT_DBGOUT(3, ("<UT> ThreadStop entered for thread anchor " UT_POINTER_SPEC "\n", thr));

	/*
	 *  Ensure a valid thread pointer has been passed
	 */
	if (*thr == NULL) {
		UT_DBGOUT(1, ("<UT> Bad thread passed to ThreadStop\n" ));
		return OMR_ERROR_INTERNAL;
	}

	/*
	 *  Let any trace listeners know that this thread is terminating
	 */
	if ((UT_GLOBAL(traceDests) & UT_EXTERNAL) != 0) {
		internalTrace(thr, NULL, UT_EXTERNAL, NULL);
	}

	/*
	 *  Write out active buffers, where they'll be freed, or just free them
	 */
	omrthread_monitor_enter(UT_GLOBAL(threadLock));

	if ((trcBuf = (*thr)->trcBuf) != NULL) {
		if (!UT_GLOBAL(traceInCore)) {
			/*
			 *  Lost record mode for this thread ?
			 */
			if (trcBuf->lostCount != 0) {
				uint64_t endTime;
				incrementRecursionCounter(*thr);
				endTime = ((uint64_t) j9time_current_time_millis()) + UT_PURGE_BUFFER_TIMEOUT;
				while ((trcBuf->flags & UT_TRC_BUFFER_FULL) != 0 &&
					(((uint64_t) j9time_current_time_millis()) < endTime)) {
					omrthread_sleep(1);
				}
				decrementRecursionCounter(*thr);
			}
			/*
			 * Trace the thread purge unconditionally
			 * Write Trc_Purge, dg.262
			 */
			internalTrace(thr, NULL, (UT_TRC_PURGE_ID << 8) | UT_MINIMAL, NULL);

			(*thr)->trcBuf = NULL;
			incrementRecursionCounter(*thr);

			/*
			 *  Flag buffer to be purged and queue it
			 */

			UT_DBGOUT(3, ("<UT> Purging buffer " UT_POINTER_SPEC " for thread "
							UT_POINTER_SPEC"\n", trcBuf, thr));
			if (queueWrite(trcBuf, UT_TRC_BUFFER_PURGE) != NULL) {
				notifySubscribers(&UT_GLOBAL(outputQueue));
			}
		} else {
			uint32_t oldFlags = 0, newFlags = 0;
			UT_DBGOUT(5, ("<UT> freeing buffer " UT_POINTER_SPEC " for thread "
							UT_POINTER_SPEC"\n", trcBuf, thr));

			do {
				oldFlags = trcBuf->flags;
				newFlags = UT_TRC_BUFFER_PURGE | oldFlags;
			} while (!twCompareAndSwap32((unsigned int *)(&trcBuf->flags), oldFlags, newFlags));
			freeBuffers(&trcBuf->queueData);
		}
	}

	/*
	 * Revert to stack storage for UtThreadData
	 */
	savedThr = *tempThr;
	savedThr.name = UT_NO_THREAD_NAME;
    *thr = NULL;
    thr = &stackThrP;

	omrthread_monitor_exit(UT_GLOBAL(threadLock));

	omrthread_tls_set(OS_THREAD_FROM_UT_THREAD(thr), j9uteTLSKey, NULL);		/* Wipe _slot_ address from TLS */

	/*
	 * Free up any threadlocal storage
	 */
	tls = (J9rasTLS *)omrthread_tls_get(OS_THREAD_FROM_UT_THREAD(thr), j9rasTLSKey);
	if (tls != NULL) {
		omrthread_tls_set(OS_THREAD_FROM_UT_THREAD(thr), j9rasTLSKey, NULL);
		if (tls->appTrace != NULL) {
			j9mem_free_memory(tls->appTrace);
		}
		j9mem_free_memory(tls);
	}

	/*
	 *  Free the UtThreadData
	 */
	if (tempThr->name != NULL && tempThr->name != UT_NO_THREAD_NAME) {
		char *tempName = (char *)tempThr->name;
		j9mem_free_memory( tempName);
	}
	j9mem_free_memory( tempThr);

	do {
		oldCount = UT_GLOBAL(threadCount);
		newCount = oldCount - 1;
	} while (!twCompareAndSwap32(&UT_GLOBAL(threadCount), oldCount, newCount));

	/*
	 * Last thread out frees the UtGlobalData and the list of available trace buffers
	 */
	if ( newCount == 0 && UT_GLOBAL(traceFinalized) ){
		UtTraceBuffer *next, *current;
		UtGlobalData *global = utGlobal;
		
		omrthread_monitor_enter(global->freeQueueLock);
		
		current = UT_GLOBAL(freeQueue);

		utGlobal = NULL;

		/* Cannot use UT_DEBUG macro as utGlobal has been set to NULL */
		if (global->traceDebug >= 2) {
			j9tty_err_printf(PORTLIB, "<UT> ThreadStop entered for final thread " UT_POINTER_SPEC ", freeing buffers\n", thr);
		}

		while (current != NULL){
			UtTraceBuffer *gNext = NULL;
			if (global->traceDebug >= 2) {
				j9tty_err_printf(PORTLIB, "<UT>   ThreadStop freeing buffer " UT_POINTER_SPEC "\n", current);
			}
			next = current->next;

			/* remove current from traceGlobal if running with debug */
			if (global->traceDebug >= 1) {
				gNext = global->traceGlobal;
				if (gNext == NULL) {
					if (global->traceDebug >= 1) {
						j9tty_err_printf(PORTLIB, "<UT> NULL global buffer list! " UT_POINTER_SPEC " not found in global list\n", current);
					}
				} else if (gNext == current) {
					global->traceGlobal = gNext->globalNext;
				} else {
					for (;gNext != NULL && gNext->globalNext != current; gNext = gNext->globalNext);
					if (gNext != NULL && gNext->globalNext == current) {
						gNext->globalNext = current->globalNext;
					} else {
						if (global->traceDebug >= 1) {
							j9tty_err_printf(PORTLIB, "<UT> trace buffer " UT_POINTER_SPEC " not found in global list\n", current);
						}
					}
				}
			}

			j9mem_free_memory( current);
			current = next;
		}
		
		global->freeQueue = NULL;
		
		omrthread_monitor_exit(global->freeQueueLock);

		/* output anything left on global if running with debug */
		if (global->traceDebug >= 1) {
			for (current = global->traceGlobal; current != NULL; current = current->globalNext) {
				j9tty_err_printf(PORTLIB, "<UT> trace buffer " UT_POINTER_SPEC " not freed!\n", current);
				j9tty_err_printf(PORTLIB, "<UT> owner: " UT_POINTER_SPEC " - %s\n", current->thr, current->record.threadName);
			}
		}

		if (global->exceptionTrcBuf != NULL) {
			j9mem_free_memory( global->exceptionTrcBuf);
		}


		omrthread_monitor_destroy(global->threadLock);
		omrthread_monitor_destroy(global->freeQueueLock);
		omrthread_monitor_destroy(global->traceLock);
		omrthread_monitor_destroy(global->triggerOnTpidsWriteMutex);
		omrthread_monitor_destroy(global->triggerOnGroupsWriteMutex);
		j9mem_free_memory(global);
	}

	return OMR_ERROR_NONE;
}


omr_error_t
utTerminateTrace(UtThreadData **thr, char** daemonThreadNames)
{
	UtTraceBuffer *trcBuf;
	uint64_t        endTime;
	int32_t        notPurged = TRUE;
	int32_t        bufferQueued = FALSE;
	omr_error_t result = OMR_ERROR_NONE;
	int         i = 0;
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	if (utGlobal == NULL) {			/* very fatal! */
		return OMR_ERROR_INTERNAL;
	}

	UT_GLOBAL(traceFinalized) = TRUE;

	/*
	 *  Trace initialized ?
	 */

	if (!UT_GLOBAL(traceInitialized)) {
		result = OMR_ERROR_INTERNAL;
		goto out;
	}

	/*
	 *  Ensure a valid thread pointer has been passed
	 */
	if (*thr == NULL) {
		result = OMR_ERROR_INTERNAL;
		goto out;
	}

	UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> Trace terminate entered\n", thr));

	if (!UT_GLOBAL(traceInCore)) {
		/* TODO: replace this block with a call to trcFlushTraceData */

		/*
		 *  Purge any buffers for this thread and give the other threads
		 *  a chance to purge theirs. Ignore never ending threads.
		 */
		incrementRecursionCounter(*thr);
		endTime = ((uint64_t) j9time_current_time_millis()) + UT_PURGE_BUFFER_TIMEOUT;
		while (notPurged && ((uint64_t) j9time_current_time_millis()) < endTime) {
			notPurged = FALSE;
			for (trcBuf = UT_GLOBAL(traceGlobal); trcBuf != NULL; trcBuf = trcBuf->globalNext) {
				if ((trcBuf->flags & UT_TRC_BUFFER_ACTIVE) != 0 ) {
					notPurged = TRUE;
					if( daemonThreadNames != NULL ) {
						i = 0;
						/* Check if this is a thread we expect to keep running. */
						while( NULL != daemonThreadNames[i] ) {
							if( 0 == strcmp(trcBuf->record.threadName, daemonThreadNames[i]) ) {
								notPurged = FALSE;
								break;
							}
							i++;
						}
					}
					if( notPurged ) {
						break;
					}
				}
			}
			omrthread_sleep(1);
		}

		/*
		 *  Flush active buffers
		 */
		for (trcBuf = UT_GLOBAL(traceGlobal); trcBuf != NULL; trcBuf = trcBuf->globalNext) {
			if ((trcBuf->flags & UT_TRC_BUFFER_ACTIVE) != 0) {
				UT_DBGOUT(2, ("<UT> Flushing buffer " UT_POINTER_SPEC
							  " for thr " UT_POINTER_SPEC "\n",
						  (uintptr_t) trcBuf, (uintptr_t)trcBuf->record.threadId));
				if (queueWrite(trcBuf, UT_TRC_BUFFER_FLUSH) != NULL) {
					bufferQueued = TRUE;
				}
			}
		}

		/* let the subscribers have a chance to process any final buffers before we clean up */
		if (bufferQueued == TRUE) {
			notifySubscribers(&UT_GLOBAL(outputQueue));
		}
	}

	/* stops anything new being queued and prompts subscribers to exit */
	destroyQueue(&UT_GLOBAL(outputQueue));

	/* Subscribers are deleting themselves in parallel with this. Don't access the contents of the subscribers list. */
	omrthread_monitor_enter(UT_GLOBAL(subscribersLock));
	while (NULL != UT_GLOBAL(subscribers)) {
		IDATA rc;
		UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> Trace terminated, waiting for subscribers to complete\n", thr));
		rc = omrthread_monitor_wait_timed(UT_GLOBAL(subscribersLock), UT_TRACE_SHUTDOWN_TIMEOUT_MILLIS, 0);
		if (J9THREAD_TIMED_OUT == rc) {
			UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> Trace termination timed out waiting for subscribers to complete\n", thr));
			j9nls_printf(PORTLIB, J9NLS_WARNING | J9NLS_STDERR, J9NLS_TRC_SHUTDOWN_TIMEOUT);
			break; /* bail out now */
		}
	}
	omrthread_monitor_exit(UT_GLOBAL(subscribersLock));
	omrthread_monitor_destroy(UT_GLOBAL(subscribersLock));
	UT_GLOBAL(subscribersLock) = NULL;

	UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> Trace terminated\n", thr));
	result = OMR_ERROR_NONE;

out:
	if ( UT_GLOBAL(traceCount) ){
		listCounters();
	}

	if (UT_GLOBAL(lostRecords) != 0) {
		UT_DBGOUT(1, ("<UT> Discarded %d trace buffers\n", UT_GLOBAL(lostRecords)));
	}
	return result;
}

/*******************************************************************************
 * name        - cleanUpTrace
 * description - Free up all trace structures
 * parameters  - UtThreadData struct
 * returns     - void
 ******************************************************************************/
void
freeTrace(UtThreadData **thr)
{
	UtTraceCfg     *config, *tmpconfig;
	int i;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> cleanUpTrace Entered\n", thr));

	if (UT_GLOBAL(traceFinalized) != TRUE) {
		/* shut everything down before freeing everything */
		UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> Error: cleanUpTrace called before trace has been finalized\n", thr));
	}

	/* so that we get cleaned up even in the case where trace hasn't initialized yet */
	destroyQueue(&UT_GLOBAL(outputQueue));

	config = UT_GLOBAL(config);
	while ( config != NULL){
		tmpconfig = config;
		config    = config->next;
		j9mem_free_memory( tmpconfig);
	}

	if (UT_GLOBAL(ignore) != NULL){
		for (i = 0; UT_GLOBAL(ignore[i]) != NULL; i++){
			j9mem_free_memory( UT_GLOBAL(ignore[i]));
		}
		j9mem_free_memory( UT_GLOBAL(ignore));
		UT_GLOBAL(ignore) = NULL;
	}

	/* free all trace component list storage */
	freeComponentList(UT_GLOBAL(componentList));
	freeComponentList(UT_GLOBAL(unloadedComponentList));

	if (UT_GLOBAL(traceFormatSpec) != NULL){
		j9mem_free_memory( UT_GLOBAL(traceFormatSpec));
		UT_GLOBAL(traceFormatSpec) = NULL;
	}

	if (UT_GLOBAL(properties)  != NULL){
		j9mem_free_memory( UT_GLOBAL(properties));
		UT_GLOBAL(properties) = NULL;
	}

	if (UT_GLOBAL(serviceInfo) != NULL){
		j9mem_free_memory( UT_GLOBAL(serviceInfo));
		UT_GLOBAL(serviceInfo) = NULL;
	}

	if (UT_GLOBAL(traceHeader) != NULL){
		j9mem_free_memory( UT_GLOBAL(traceHeader));
		UT_GLOBAL(traceHeader) = NULL;
	}

	if (UT_GLOBAL(traceFilename) != NULL){
		j9mem_free_memory( UT_GLOBAL(traceFilename));
		UT_GLOBAL(traceFilename) = NULL;
	}
	
	if (UT_GLOBAL(exceptFilename) != NULL){
		j9mem_free_memory( UT_GLOBAL(exceptFilename));
		UT_GLOBAL(exceptFilename) = NULL;
	}

	freeTriggerOptions(UT_GLOBAL(portLibrary));

	UT_DBGOUT(1, ("<UT> cleanUpTrace complete\n"));

	/* NOTE: utGlobal will be freed by the last thread... */
	return;
}

omr_error_t
threadStart(UtThreadData **thr, const void *threadId, const char *threadName, const void *threadSynonym1, const void *threadSynonym2)
{
	omr_error_t   rc = OMR_ERROR_NONE;
	UtThreadData *newThr = NULL;
	UtThreadData tempThr;
	uint32_t        oldCount;
	uint32_t        newCount;
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	/* Set up a temporary UtThreadData structure.
	 * TODO: Investigate whether we really need this, given that we can't trigger tracepoints at this stage. See the
	 * additional comments below. The 2.9 OMR implementation avoids doing it (and allocates UtThreadData from a pool).
	 */
	memset(&tempThr, 0, sizeof(UtThreadData));
	initHeader(&tempThr.header, UT_THREAD_DATA_NAME, sizeof(UtThreadData));
	tempThr.id       = threadId;
	tempThr.synonym1 = threadSynonym1;
	tempThr.synonym2 = threadSynonym2;
	tempThr.suspendResume = UT_GLOBAL(initialSuspendResume);
	/* Trace points must not be triggered for this thread before we have switched to the final (portlib allocated)
	 * UtThreadData. This is because trcFlushTraceData() will update the owning UtThreadData when it flushes trace
	 * buffers (as it moves them to the freeQueue). The first tracepoint acquires and attaches a buffer. See PR 79176.
	 */
	tempThr.recursion = 1;

	if (NULL == threadName) {
		tempThr.name = UT_NO_THREAD_NAME;
	} else {
		tempThr.name = threadName;
	}

	do {
		oldCount = UT_GLOBAL(threadCount);
		newCount = oldCount + 1;
	} while (!twCompareAndSwap32(&UT_GLOBAL(threadCount), oldCount, newCount));

	UT_DBGOUT(2, ("<UT> Thread started , thread anchor " UT_POINTER_SPEC "\n", thr));
	UT_DBGOUT(2, ("<UT> thread Id " UT_POINTER_SPEC ", thread name \"%s\", syn1 " UT_POINTER_SPEC ", syn2 "
				UT_POINTER_SPEC " \n", threadId, threadName, threadSynonym1, threadSynonym2));

	/*
	 * Setup the current UtThread with the temporary.
	 */
	*thr = &tempThr;

	/*
	 * Now we can allocate memory using the port library.
	 */
	newThr = j9mem_allocate_memory(sizeof(UtThreadData), OMRMEM_CATEGORY_TRACE);
	if (NULL == newThr) {
		UT_DBGOUT(1, ("<UT> Unable to obtain storage for thread control block \n"));
		*thr = NULL;
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
	memcpy(newThr, &tempThr, sizeof(tempThr));
	
	if (NULL != threadName) {
		/* Obtain storage, copy the thread name, and reference it off UtThreadData */
		char *tempName = j9mem_allocate_memory(strlen(threadName) + 1, OMRMEM_CATEGORY_TRACE);
		if (NULL != tempName) {
			strcpy(tempName, threadName);
			newThr->name = tempName;
		} else {
			UT_DBGOUT(1, ("<UT> Unable to obtain storage for thread name\n"));
			newThr->name = UT_NO_THREAD_NAME;
		}
	}

	/* Update the caller's UtThreadData pointer to reference the new (portlib allocated) UtThreadData */
	*thr = newThr;
	/* Allow tracepoints to be triggered from this point onwards */
	decrementRecursionCounter(*thr);
	omrthread_tls_set(OS_THREAD_FROM_UT_THREAD(thr), j9uteTLSKey, thr); /* Set UtThreadData address in TLS */
	return rc;
}

omr_error_t 
initializeTrace(UtThreadData **thr, void **gbl,
				 const char **opts, const OMR_VM *vm,
				 const char ** ignore, const OMRTraceLanguageInterface *languageIntf)

{
	omr_error_t   rc = OMR_ERROR_NONE;
	int           i;
	UtThreadData  tempThr;
	UtGlobalData  tempGbl;
	UtGlobalData *newGbl;
	qQueue       *outputQueue = NULL;

	PORT_ACCESS_FROM_JAVAVM((J9JavaVM *)vm->_language_vm);
	/*
	 *  Bootstrap UtThreadData first using stack variable (in case we call utcFprintf below)
	 */
	memset(&tempThr, 0, sizeof(UtThreadData));
	initHeader(&tempThr.header, UT_THREAD_DATA_NAME, sizeof(UtThreadData));

	*thr = &tempThr;

	if (utGlobal != NULL){
		if (UT_GLOBAL(traceEnabled) == TRUE){
			if (UT_GLOBAL(traceFinalized) && UT_GLOBAL(traceInCore) == TRUE){
				/* we can get away with this as the old process no longer has any interest in
				   the old (stale) trace engine */
				utGlobal = NULL;
			} else {
				/* the old utGlobal is still in use */
				UT_DBGOUT(1, ("<UT> Error, utGlobal already in use.\n"));
				return OMR_ERROR_INTERNAL;
			}
		}
	}

	/*
	 *  Bootstrap UtGlobalData using stack variable
	 */
	memset(&tempGbl, 0, sizeof(UtGlobalData));
	initHeader(&tempGbl.header, UT_GLOBAL_DATA_NAME, sizeof(UtGlobalData));
	tempGbl.vm = vm;
	tempGbl.portLibrary = PORTLIB;

	tempGbl.dynamicBuffers = TRUE;
	tempGbl.bufferSize = UT_DEFAULT_BUFFERSIZE;

	/* Make the trace functions available to the rest of OMR */
	((OMR_VM *)vm)->utIntf = &internalUtIntfS;

	/*
	 * Attach global data to thread data
	 */
	if( NULL != gbl ) {
		*gbl = &tempGbl;
	}
	utGlobal = &tempGbl;
	{
		char *envString = getenv(UT_DEBUG);
		if (envString != NULL) {
			if (hexStringLength(envString) == 1 &&
				*envString >= '0' &&
				*envString <= '9' ) {
				UT_GLOBAL(traceDebug) = atoi(envString);
			} else {
				UT_GLOBAL(traceDebug) = 9;
			}
		}
	}

	/*
	 *  Check options for the debug switch. Options are in name / value pairs
	 *  so only look at the first of each pair
	 */

	for(i = 0; opts[i] != NULL; i += 2) {
		if (0 == j9_cmdla_strnicmp(opts[i], UT_DEBUG_KEYWORD, strlen(UT_DEBUG_KEYWORD))) {
			if (opts[i + 1] != NULL &&
				strlen(opts[i + 1]) == 1 &&
				*opts[i + 1] >= '0' &&
				*opts[i + 1] <= '9') {
				UT_GLOBAL(traceDebug) = atoi(opts[i + 1]);
			} else {
				UT_GLOBAL(traceDebug) = 9;
			}
			UT_DBGOUT(1, ("<UT> Debug information requested\n"));
		}
	}

	UT_DBGOUT(1, ("<UT> Initialization for thread anchor " UT_POINTER_SPEC "\n", thr));
	UT_DBGOUT(1, ("<UT> Client Id " UT_POINTER_SPEC "\n", vm));

	/*
	 *  Check for options to ignore in the properties file. This allows the
	 *  client to support its own set of commands in addition to the UTE set.
	 */

	if (ignore != NULL) {
		for(i = 0; ignore[i] != NULL; i++);
		if (i != 0) {
			UT_GLOBAL(ignore) = j9mem_allocate_memory(sizeof(char *) * (i + 1), OMRMEM_CATEGORY_TRACE);
			if (UT_GLOBAL(ignore) == NULL) {
				UT_DBGOUT(1, ("<UT> Unable to obtain storage for excluded command list\n"));
				rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
				goto fail;
			}
			for(i = 0; ignore[i] != NULL; i++) {
				UT_GLOBAL(ignore)[i] = j9mem_allocate_memory(strlen(ignore[i]) + 1, OMRMEM_CATEGORY_TRACE);
				if (UT_GLOBAL(ignore)[i] == NULL) {
					UT_DBGOUT(1, ("<UT> Unable to obtain storage for excluded command\n"));
					rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
					goto fail;
				}
				strcpy(UT_GLOBAL(ignore)[i], ignore[i]);
			}
			UT_GLOBAL(ignore)[i] = NULL;
		}
	}

	rc = initializeComponentList(&UT_GLOBAL(componentList));
	if (OMR_ERROR_NONE != rc) {
		UT_DBGOUT(1, ("<UT> Error initializing component list\n"));
		goto fail;
	}

	rc = initializeComponentList(&UT_GLOBAL(unloadedComponentList));
	if (OMR_ERROR_NONE != rc) {
		UT_DBGOUT(1, ("<UT> Error initializing unloaded component list\n"));
		goto fail;
	}

	/*
	 *  Initialize the semaphores in the global data area
	 */
	if (0 != omrthread_monitor_init_with_name(&UT_GLOBAL(traceLock), 0, "Global Trace")) {
		UT_DBGOUT(1, ("<UT> Initialization of traceLock failed\n"));
		rc = OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR;
		goto fail;
	}
	if (0 != omrthread_monitor_init_with_name(&UT_GLOBAL(subscribersLock), 0, "Global Record Subscribers")) {
		UT_DBGOUT(1, ("<UT> Initialization of subscribersLock failed\n"));
		rc = OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR;
		goto fail;
	}
	if (0 != omrthread_monitor_init_with_name(&UT_GLOBAL(threadLock), 0, "Global Trace Thread")) {
		UT_DBGOUT(1, ("<UT> Initialization of threadLock failed\n"));
		rc = OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR;
		goto fail;
	}
	if (0 != omrthread_monitor_init_with_name(&UT_GLOBAL(triggerOnTpidsWriteMutex), 0, "Trace Trigger on tpid")) {
		UT_DBGOUT(1, ("<UT> Initialization of triggerOnTpidsWriteMutex failed\n"));
		rc = OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR;
		goto fail;
	}
	if (0 != omrthread_monitor_init_with_name(&UT_GLOBAL(triggerOnGroupsWriteMutex), 0, "Trace Trigger on groups")) {
		UT_DBGOUT(1, ("<UT> Initialization of triggerOnGroupsWriteMutex failed\n"));
		rc = OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR;
		goto fail;
	}
	if (0 != omrthread_monitor_init_with_name(&UT_GLOBAL(freeQueueLock), 0, "Global Trace Free Queue")) {
		UT_DBGOUT(1, ("<UT> Initialization of freeQueueLock failed\n"));
		rc = OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR;
		goto fail;
	}

	/* initialize the buffer output queue */
	outputQueue = &UT_GLOBAL(outputQueue);
	rc = createQueue(&outputQueue);
	if (OMR_ERROR_NONE != rc) {
		UT_DBGOUT(1, ("<UT> Unable to initialize buffer output queue\n"));
		goto fail;
	}

	/*
	 *  Obtain storage for the real UtGlobalData
	 */

	newGbl = j9mem_allocate_memory(sizeof(UtGlobalData), OMRMEM_CATEGORY_TRACE);
	if (newGbl == NULL) {
		UT_DBGOUT(1, ("<UT> Unable to obtain storage for global control block \n"));
		rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		goto fail;
	}

	memcpy(newGbl, &tempGbl, sizeof(UtGlobalData));
	if( NULL != gbl ) {
		*gbl = newGbl;
	};
	utGlobal = newGbl;

	/*
	* Get TLS keys
	*/
	if (omrthread_tls_alloc(&j9rasTLSKey)) {
		/* Unable to allocate RAS thread local storage key */
		j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_RAS_TLS_ALLOC_FAILED);
		rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		goto fail;
	}
	
	if (omrthread_tls_alloc(&j9uteTLSKey)) {
		/* Unable to allocate UTE thread local storage key */
		j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_UTE_TLS_ALLC_FAILED);
		rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		goto fail;
	}

	if (NULL != languageIntf) {
		memcpy(&UT_GLOBAL(languageIntf), languageIntf, sizeof(UT_GLOBAL(languageIntf)));
	} else {
		memset(&UT_GLOBAL(languageIntf), 0, sizeof(UT_GLOBAL(languageIntf)));
	}

	/*
	 *  Set the trace start time
	 */
	setStartTime();

	/*
	 * Set the default sleep time for the "sleep" trigger action.
	 */
	UT_GLOBAL(sleepTimeMillis) = UT_TRACE_SLEEPTIME_DEFAULT_MILLIS;

	/* Enable fatal assertions by default */
	UT_GLOBAL(fatalassert) = 1;

	  /*
	 *  Process early options
	 */
	rc = processEarlyOptions(opts);
	if (OMR_ERROR_NONE != rc) {
		UT_DBGOUT(1, ("<UT> Error processing early options \n"));
		*thr = NULL;
		return rc;
	}

	UT_GLOBAL(traceEnabled) =TRUE;
	UT_GLOBAL(traceInCore) = TRUE;
	/*
	 *  Process options
	 */
	rc = processOptions(thr, opts, FALSE);
	if (OMR_ERROR_NONE != rc) {
		UT_DBGOUT(1, ("<UT> Error processing options \n"));
		*thr = NULL;
		return rc;
	}

	*thr = NULL;

	return OMR_ERROR_NONE;

fail:

	/*
	 * Unable to initialize, J9VMDllMain will report this failure.
	 */
	utGlobal = NULL;
	if (NULL != gbl) {
		*gbl = NULL;
	}
	*thr = NULL;

	return rc;
}

/*******************************************************************************
 * name        - trcAddComponent
 * description - Add a trace component
 * parameters  - component's module, format array
 * returns     - UTE return code
 ******************************************************************************/
static omr_error_t
trcAddComponent(UtModuleInfo *modInfo, const char **format) 
{
	omr_error_t rc = OMR_ERROR_NONE;
	UtComponentData *compData = NULL;
	UtThreadData **thr = twThreadSelf();
	char **tmp_formatStrings;
	int i;
	int arrayElements = 0;
	unsigned char *types;
	size_t overhead;
	char *thisFormat;
	char *formatTemplate;
	char type[4];
	size_t stringSize = 0;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_DBGOUT(1, ("<UT> AddComponent entered for %s\n", modInfo->name));

	for (arrayElements = 0; format[arrayElements] != NULL; arrayElements++);

	if ((rc = moduleLoaded(thr, modInfo)) != OMR_ERROR_NONE) {
		UT_DBGOUT(1, ("<UT> Trace engine failed to register module: %s, trace not enabled\n", modInfo->name));
		return OMR_ERROR_INTERNAL;
	}

	if (NULL == (compData = getComponentData(modInfo->name, UT_GLOBAL(componentList)))) {
		UT_DBGOUT(1, ("<UT> Unable to retrieve component data for module: %s, trace not enabled\n", modInfo->name));
		return OMR_ERROR_INTERNAL;
	}

	types = (unsigned char *) j9mem_allocate_memory(arrayElements, OMRMEM_CATEGORY_TRACE);
	if (types == NULL) {
		UT_DBGOUT(1, ("<UT> Unable to allocate types memory for trace module: %s, trace not enabled\n", modInfo->name));
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
	tmp_formatStrings = (char **)j9mem_allocate_memory((sizeof(char *) * (arrayElements + 1)), OMRMEM_CATEGORY_TRACE);
	if (tmp_formatStrings == NULL) {
		UT_DBGOUT(1, ("<UT> Unable to allocate formatStrings memory for trace module: %s, trace not enabled\n", modInfo->name));
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
	overhead =  2;
	for (i = 0; i < arrayElements; i++) {
		/*
		 * Required space following the trace type ?
		 */
		if ((thisFormat = strchr(format[i], ' ')) == NULL) {
			rc = OMR_ERROR_ILLEGAL_ARGUMENT;
			break;
		}
		/*
		 * Reasonable length for the type ? Must fit in a byte.
		 */
		if (thisFormat - format[i] == 0 || thisFormat - format[i] > 3) {
			rc = OMR_ERROR_ILLEGAL_ARGUMENT;
			break;
		}

		/*
		 *  Convert it to a byte and validity check the number
		 */
		memcpy(type, format[i], thisFormat - format[i]);
		type[(thisFormat - format[i])] = '\0';
		types[i] = (unsigned char)atoi(type);
		if (types[i] > UT_MAX_TYPES) {
			rc = OMR_ERROR_ILLEGAL_ARGUMENT;
			break;
		}
		/*
		 * Skip forward to the start of the formatting template and save it
		 */
		while (*thisFormat == ' ') {
			thisFormat++;
		}
		tmp_formatStrings[i] = thisFormat;

		stringSize = strlen(thisFormat) + 1 + overhead;

		formatTemplate = (char *)j9mem_allocate_memory(stringSize, OMRMEM_CATEGORY_TRACE);
		if (formatTemplate == NULL) {
			UT_DBGOUT(1, ("<UT> trcAddComponent cannot allocate memory for app trace module: %s, trace not enabled\n", modInfo->name));
			rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			break;
		}

		*formatTemplate++ = (types[i] & UT_EXCEPTION_TYPE) ? '*':' ';
		*formatTemplate++ = UT_FORMAT_TYPES[types[i]];
		strcpy(formatTemplate, tmp_formatStrings[i]);
		tmp_formatStrings[i] = formatTemplate - overhead;
	}
	compData->tracepointFormattingStrings = tmp_formatStrings;
	return rc;
}

/*******************************************************************************
 * name        - trcGetComponents
 * description - Return a list of trace components
 * parameters  - thr, component name, format array
 * returns     - OMR error code
 ******************************************************************************/

static omr_error_t
trcGetComponents(UtThreadData **thr, char ***list, int32_t *number)
{
	int i = 0;
	UtComponentData *tempCompData = UT_GLOBAL(componentList)->head;
	char **tempList = NULL;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	if (list == NULL){
		UT_DBGOUT(1, ("<UT> trcGetComponents called with NULL list, should be valid pointer\n"));
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	getTraceLock(thr);

	while(tempCompData != NULL){
		/* don;t count deferred config components */
		if (tempCompData->moduleInfo != NULL){
			i++;
		}
		tempCompData = tempCompData->next;
	}

	*number = i;
	/* write the component names into the list */

	tempList = (char **) j9mem_allocate_memory(sizeof(char *) * i, OMRMEM_CATEGORY_TRACE);
	if (tempList == NULL){
		UT_DBGOUT(1, ("<UT> trcGetComponents can't allocate list.\n"));
		freeTraceLock(thr);
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	tempCompData = UT_GLOBAL(componentList)->head;
	i = 0;
	while(tempCompData != NULL){
		/* don;t count deferred config components */
		if (tempCompData->moduleInfo != NULL){
			if ( i > (*number)){
				UT_DBGOUT(1, ("<UT> trcGetComponents internal error - state of component list changed.\n"));
				freeTraceLock(thr);
				return OMR_ERROR_INTERNAL;
			}
			tempList[i] = (char *)j9mem_allocate_memory(strlen(tempCompData->componentName) + 1, OMRMEM_CATEGORY_TRACE);
			if (tempList[i] == NULL) {
				UT_DBGOUT(1, ("<UT> trcGetComponents can't allocate name.\n"));
				freeTraceLock(thr);
				return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			}
			strcpy(tempList[i], tempCompData->componentName);
			i++;
		}
		tempCompData = tempCompData->next;
	}

	*list = tempList;

	freeTraceLock(thr);

	return OMR_ERROR_NONE;
}

/*******************************************************************************
 * name        - trcGetComponent
 * description - Return a bitmap tracepoints included in the build for a given
 *               component.
 * parameters  - component name, format array
 * returns     - OMR error code
 ******************************************************************************/

static omr_error_t
trcGetComponent(char *name, unsigned char **bitMap,
				int32_t *first, int32_t *last)
{
	omr_error_t rc = OMR_ERROR_ILLEGAL_ARGUMENT;

	UtComponentData *cd = getComponentData(name, UT_GLOBAL(componentList) );

	if ( cd == NULL ){
		UT_DBGOUT(2, ("<UT> trcGetComponent requested data area for component: \"%s\"  that is not currently loaded\n", name));
		*bitMap = NULL;
		*first = 0;
		*last = 0;
	} else {
		UT_DBGOUT(2, ("<UT> trcGetComponent found data area for component: \"%s\"\n", name));
		*bitMap = cd->moduleInfo->active;
		*first = 0;
		*last = cd->moduleInfo->count;
		rc = OMR_ERROR_NONE;
	}
	return rc;
}

/*******************************************************************************
 * name        - getComponentGroup
 * description - Return the tracepoint ids for a trace group
 * parameters  - component name, group name ... tp count & array (out)
 * returns     - OMR Error code
 ******************************************************************************/

omr_error_t
getComponentGroup(char *compName, char *groupName,
					int32_t *count, int32_t **tracePts)
{

	UtComponentData *compData = NULL;
	UtGroupDetails *groupDetails = NULL;

	/*
	 *  Find the component
	 */
	compData = getComponentData(compName, UT_GLOBAL(componentList));
	if (compData != NULL && compData->moduleInfo != NULL) {
		groupDetails = compData->moduleInfo->groupDetails;
	}

	/*
	 *  Now look for the group
	 */
	while ( groupDetails != NULL ) {
		if ( !j9_cmdla_strnicmp(groupName, groupDetails->groupName, strlen(groupDetails->groupName))  ){
			*count = groupDetails->count;
			*tracePts = groupDetails->tpids;
			return OMR_ERROR_NONE;
		}
		groupDetails = groupDetails->next;
	}

	*count = 0;
	*tracePts = NULL;

	return OMR_ERROR_ILLEGAL_ARGUMENT;
}

/*******************************************************************************
 * name        - trcSuspend
 * description - Sets the suspend count and returns the current value
 * parameters  - thr, type
 * returns     - Suspend value
 ******************************************************************************/

int32_t
trcSuspend(UtThreadData **thr, int32_t type)
{

	switch (type) {
	case UT_SUSPEND_GLOBAL:
		UT_ATOMIC_OR((volatile uint32_t*)&UT_GLOBAL(traceSuspend), UT_SUSPEND_USER);
		return (int32_t)UT_GLOBAL(traceSuspend);
		break;
	case UT_SUSPEND_THREAD:
		(*thr)->suspendResume -= 1;
		return (*thr)->suspendResume;
		break;
	}
	return 0;
}

/*******************************************************************************
 * name        - trcResume
 * description - Sets the suspend count and returns the current value
 * parameters  - thr, type
 * returns     - Suspend value
 ******************************************************************************/

int32_t
trcResume(UtThreadData **thr, int32_t type) {
	switch (type) {
	case UT_RESUME_GLOBAL:
		UT_ATOMIC_AND((volatile uint32_t*)&UT_GLOBAL(traceSuspend), ~UT_SUSPEND_USER);
		return (int32_t)UT_GLOBAL(traceSuspend);
		break;
	case UT_RESUME_THREAD:
		(*thr)->suspendResume += 1;
		return (*thr)->suspendResume;
		break;
	}
	return 0;
}


/*******************************************************************************
 * name        - trcRegisterRecordSubscriber
 * description - Registers a subscriber that consumes trace records from the
 * 				 write queue.
 * parameters  - thr, description, subscriber, alarm,, userData, start, stop, subscription
 * 				 start: -1 means head, NULL means tail, anything else is a buffer
 *               stop: NULL means don't stop, anything else is a buffer
 * returns     - Success or error code
 ******************************************************************************/
omr_error_t
trcRegisterRecordSubscriber(UtThreadData **thr, const char *description, utsSubscriberCallback subscriber, utsSubscriberAlarmCallback alarm, void *userData, UtTraceBuffer *start, UtTraceBuffer *stop, UtSubscription **subscriptionReference, int32_t attach)
{
	omr_error_t result = OMR_ERROR_NONE;
	UtSubscription *subscription;
	qSubscription *qSub;
	qMessage *qStart, *qStop;
	size_t descriptionLen = 0;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	if (subscriber == NULL) {
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	subscription = j9mem_allocate_memory(sizeof(UtSubscription), OMRMEM_CATEGORY_TRACE);
	if (subscription == NULL) {
		UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> Out of memory allocating subscription\n", thr));
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
	subscription->queueSubscription = j9mem_allocate_memory(sizeof(qSubscription), OMRMEM_CATEGORY_TRACE);
	if (subscription->queueSubscription == NULL) {
		UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> Out of memory allocating queueSubscription\n", thr));
		j9mem_free_memory(subscription);
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Acquiring lock for registration\n", thr));
	omrthread_monitor_enter(UT_GLOBAL(subscribersLock));
	getTraceLock(thr);
	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Lock acquired for registration\n", thr));

	if (subscriptionReference != NULL) {
		*subscriptionReference = subscription;
	}

	subscription->subscriber = subscriber;
	subscription->userData = userData;
	subscription->alarm = alarm;
	subscription->next = NULL;
	subscription->prev = NULL;
	subscription->threadAttach = attach;
	subscription->threadPriority = J9THREAD_PRIORITY_USER_MAX;
	subscription->state = UT_SUBSCRIPTION_ALIVE;
	qSub = subscription->queueSubscription;

	if (description == NULL) {
		description = "Trace Subscriber [unnamed]";
	}
	descriptionLen = strlen(description);
	subscription->description = j9mem_allocate_memory( sizeof(char) * (descriptionLen +1), OMRMEM_CATEGORY_TRACE);
	if (subscription->description == NULL) {
		UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> Out of memory allocating description\n", thr));
		result = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		goto out;
	}
	strcpy(subscription->description, description);

	UT_DBGOUT(2, ("<UT> Creating subscription\n"));
	if (start == NULL || start == (UtTraceBuffer*)-1) {
		qStart = (qMessage*)start;
	} else {
		qStart = &start->queueData;
	}

	qStop = NULL;
	if (stop != NULL) {
		qStop = &stop->queueData;
	}

	result = subscribe(&UT_GLOBAL(outputQueue), &qSub, qStart, qStop);

	if (OMR_ERROR_NONE != result) {
		UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> Failed to subscribe %s to queue "UT_POINTER_SPEC"\n", thr, description, UT_GLOBAL(outputQueue)));
		goto out;
	}

	enlistRecordSubscriber(subscription);

	UT_DBGOUT(2, ("<UT thr="UT_POINTER_SPEC"> Starting trace subscriber thread\n", thr));
	if (0 != omrthread_create(NULL, UT_STACK_SIZE, J9THREAD_PRIORITY_NORMAL, 0, subscriptionHandler, subscription)) {
		result = OMR_ERROR_INTERNAL;
	}

out:
	/* we don't consider bounded subscribers, for example snap dumps, to be in core as they inherently only operate
	 * on currently queued data.
	 */
	if ((OMR_ERROR_NONE == result) && (NULL == stop)) {
		UT_GLOBAL(traceInCore) = FALSE;
	}

	if (OMR_ERROR_NONE != result) {
		UT_DBGOUT(1, ("<UT> Error starting trace thread for \"%s\": %i\n", description, result));
		destroyRecordSubscriber(thr, subscription);
	}

	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Releasing lock for registration\n", thr));
	freeTraceLock(thr);
	omrthread_monitor_exit(UT_GLOBAL(subscribersLock));
	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Lock released for registration\n", thr));
	return result;
}


/*******************************************************************************
 * name        - trcDeregisterRecordSubscriber
 * description - Deregisters a subscriber currently consuming trace buffers from the
 * 				 write queue
 * parameters  - thr, subscriptionID
 * returns     - Success or error code
 ******************************************************************************/
omr_error_t
trcDeregisterRecordSubscriber(UtThreadData **thr, UtSubscription *subscriptionID)
{
	omr_error_t result = OMR_ERROR_NONE;

	if (subscriptionID == NULL) {
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Acquiring lock for deregistration\n", thr));
	omrthread_monitor_enter(UT_GLOBAL(subscribersLock));
	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Lock acquired for deregistration\n", thr));

	if (findRecordSubscriber(subscriptionID)) {
		UtSubscription *subscription = subscriptionID;

		getTraceLock(thr);

		/*
		 * The subscription may be concurrently attempting to destroy itself, e.g. due to a queue error.
		 * Setting state to KILLED prevents the subscription thread from destroying the subscription.
		 *
		 * Delisting and deletion should always be performed atomically, so if the subscriber already
		 * destroyed itself, we can't pass the findRecordSubscriber() test above.
		 */
		subscription->state = UT_SUBSCRIPTION_KILLED;
		
		/* wake subscriptionHandler from waiting for next message */
		result = unsubscribe(subscription->queueSubscription);

		/*
		 * Delisting is required to prevent multiple threads from attempting to Deregister the subscription.
		 * If multiple callers attempt to deregister the subscriber, only one of them
		 * passes the findRecordSubscriber() test.
		 */
		delistRecordSubscriber(subscription);

		if (UT_GLOBAL(subscribers) == NULL ) {
			UT_GLOBAL(traceInCore) = TRUE;
			UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> DeregisterRecordSubscriber: set traceInCore to TRUE\n", thr));
		}

		UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Releasing lock for deregistration\n", thr));
		freeTraceLock(thr);
		UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Lock released for deregistration\n", thr));

		do {
			UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Waiting for subscriptionHandler to deregister itself\n", thr));
			omrthread_monitor_wait(UT_GLOBAL(subscribersLock));
		} while (UT_SUBSCRIPTION_DEAD != subscription->state);
		deleteRecordSubscriber(subscription);
	} else {
		UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> Failed to find subscriber to deregister\n", thr));
		result = OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Releasing lock for deregistration\n", thr));
	omrthread_monitor_exit(UT_GLOBAL(subscribersLock));
	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Lock released for deregistration\n", thr));
	return result;
}


/*******************************************************************************
 * name        - trcFlushTraceData
 * description - Places in use trace buffers on the write queue and prompts queue
 * 				 processing
 * parameters  - thr, first, last, pause
 * returns     - Success or error code
 ******************************************************************************/
static omr_error_t
trcFlushTraceData(UtThreadData **thr, UtTraceBuffer **first, UtTraceBuffer **last, int32_t pause)
{
	static int32_t flushing = FALSE;
	UtTraceBuffer *start = NULL, *stop = NULL, *current = NULL, *trcBuf = NULL;

	if (UT_GLOBAL(traceInCore) && !UT_GLOBAL(traceSnap)) {
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	if (!twCompareAndSwap32((uint32_t*)&flushing, FALSE, TRUE)) {
		/* there's a flush in progress */
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	/* Disable re-entering the trace engine to log tracepoints */
	incrementRecursionCounter(*thr);

	for (trcBuf = UT_GLOBAL(traceGlobal); trcBuf != NULL; trcBuf = trcBuf->globalNext) {
		/* If this buffer is currently in use by the thread that last used it then yank it away.
		 * This loses the reference to the circular list of buffers for the thread so they get purged.
		 *
		 * We use atomic swap because the buffer could be in the process of being swapped out by getTrcBuf.
		 */
		if (!(trcBuf->flags & UT_TRC_BUFFER_ACTIVE)) {
			/* if the buffer's not active we don't care about it */
			continue;
		}

		if (trcBuf == UT_GLOBAL(exceptionTrcBuf)) {
			getTraceLock(thr);

			UT_GLOBAL(exceptionTrcBuf) = NULL;

			freeTraceLock(thr);
		} else {
			int32_t live = FALSE;

			omrthread_monitor_enter(UT_GLOBAL(threadLock));

			if (trcBuf->thr && *(trcBuf->thr) && (*(trcBuf->thr))->trcBuf == trcBuf) {
				twCompareAndSwapPtr((uintptr_t*)&((*(trcBuf->thr))->trcBuf), (uintptr_t)trcBuf, (uintptr_t)NULL);
				live = TRUE;
			}

			omrthread_monitor_exit(UT_GLOBAL(threadLock));

			if (!live) {
				continue;
			}
		}

		UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Flushing buffer " UT_POINTER_SPEC
					  " for thread " UT_POINTER_SPEC "\n", thr, trcBuf, (uintptr_t)trcBuf->record.threadId));

		if (start == NULL && pause) {
			/* make this buffer block cleaning */
			pauseDequeueAtMessage(&trcBuf->queueData);
		}

		current = queueWrite(trcBuf, UT_TRC_BUFFER_PURGE);

		if (start == NULL) {
			if (current == NULL && pause) {
				/* unblock cleaning */
				resumeDequeueAtMessage(&trcBuf->queueData);
			} else {
				start = current;
				UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> "UT_POINTER_SPEC" is start of flush\n", thr, start));
			}
		}

		if (current != NULL) {
			stop = current;
			UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> "UT_POINTER_SPEC" is end of flush\n", thr, stop));
		} else {
			/* we've discarded the reference to the circular list of buffers, but the final buffer wasn't queued, so free
			 * by hand.
			 */
			freeBuffers(&trcBuf->queueData);
		}
	}

	/* let new flushing commence! */
	flushing = FALSE;

	notifySubscribers(&UT_GLOBAL(outputQueue));

	/* Re-enable tracepoint logging for this thread */
	decrementRecursionCounter(*thr);

	if (first != NULL) {
		*first = start;
	}
	if (last != NULL) {
		*last = stop;
	}

	return OMR_ERROR_NONE;
}


/*******************************************************************************
 * name        - trcGetTraceMetadata
 * description - Retrieves the metadata needed to initialize the formatter
 * parameters  - data, length
 * returns     - Success or error code
 ******************************************************************************/
static omr_error_t
trcGetTraceMetadata(void **data, int32_t *length)
{
	UtTraceFileHdr *traceHeader;

	/* Ensure we have initialized the trace header */
	if (initTraceHeader() != OMR_ERROR_NONE) {
		return OMR_ERROR_INTERNAL;
	}

	traceHeader = UT_GLOBAL(traceHeader);

	if (traceHeader == NULL) {
		return OMR_ERROR_INTERNAL;
	}

	*data = traceHeader;
	*length = traceHeader->header.length;

	return OMR_ERROR_NONE;
}

/*******************************************************************************
 * name        - trcDisableTrace
 * description - Disables tracepoints for all threads, or for the current thread.
 * 	             Trace disable/enable is the internal mechanism that the trace,
 * 	             dump and memcheck use to switch trace off and on. Suspend/resume
 * 	             is the the external user facility available via -Xtrace and JVMTI.
 * parameters  - type (UT_DISABLE_GLOBAL or UT_DISABLE_THREAD)
 * returns     - void
 ******************************************************************************/
static void
trcDisableTrace(int32_t type)
{
	switch (type) {
	case UT_DISABLE_GLOBAL:
		/* Disable all tracepoints via the trace engine global traceDisable flag */
		UT_ATOMIC_INC(&UT_GLOBAL(traceDisable));
		break;

	case UT_DISABLE_THREAD:
		/* Disable tracepoints for this thread only, via the UtThread recursion counter */
		if (NULL != globalVM) {
			UtThreadData **thr = UT_THREAD_FROM_VM_THREAD(globalVM->internalVMFunctions->currentVMThread(globalVM));
			if ((NULL != thr) && (NULL != *thr)) {
				incrementRecursionCounter(*thr);
			}
		}
		break;
	}
}

/*******************************************************************************
 * name        - trcEnableTrace
 * description - Enables tracepoints for all threads, or for the current thread.
 * parameters  - type (UT_DISABLE_GLOBAL or UT_DISABLE_THREAD)
 * returns     - void
 ******************************************************************************/
static void
trcEnableTrace(int32_t type)
{
	switch (type) {
	case UT_ENABLE_GLOBAL:
		/* Re-enable all tracepoints via the trace engine global traceDisable flag */
		UT_ATOMIC_DEC(&UT_GLOBAL(traceDisable));
		break;
		
	case UT_ENABLE_THREAD:
		/* Re-enable tracepoints for this thread only, via the UtThread recursion counter */
		if (NULL != globalVM) {
			UtThreadData **thr = UT_THREAD_FROM_VM_THREAD(globalVM->internalVMFunctions->currentVMThread(globalVM));
			if ((NULL != thr) && (NULL != *thr)) {
				decrementRecursionCounter(*thr);
			}
		}
		break;
	}
}

/*******************************************************************************
 * name        - trcRegisterTracePointSubscriber
 * description - Register a tracepoint subscriber
 * parameters  - UtThreadData, subscriber description, subscriber function, alarm function, user data pointer, subscription pointer
 * returns     - JNI return codes
 ******************************************************************************/
static omr_error_t
trcRegisterTracePointSubscriber(UtThreadData **thr, char *description, utsSubscriberCallback subscriber, utsSubscriberAlarmCallback alarm, void *wrapper, UtSubscription **subscriptionReference)
{
	UtSubscription *subscription;
	UtSubscription *newSubscription;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));
	UT_DBGOUT(1, ("<UT> trcRegisterTracePointSubscriber entered\n"));
	
	/* allocate a new subscription data structure */
	newSubscription = (UtSubscription *)j9mem_allocate_memory(sizeof(UtSubscription), OMRMEM_CATEGORY_TRACE);
	if (newSubscription == NULL) {
		UT_DBGOUT(1, ("<UT> Out of memory in trcRegisterTracePointSubscriber\n"));
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
	
	/* initialize the new subscription data structure */
	newSubscription->subscriber = subscriber;
	newSubscription->userData = wrapper;
	newSubscription->alarm = alarm;
	newSubscription->next = NULL;
	newSubscription->prev = NULL;
	if (description != NULL) {
		/* obtain storage and copy the user-supplied description string */
		newSubscription->description = j9mem_allocate_memory(strlen(description) + 1, OMRMEM_CATEGORY_TRACE);
		if (newSubscription->description == NULL) {
			UT_DBGOUT(1, ("<UT> Out of memory in trcRegisterTracePointSubscriber\n"));
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
		strcpy(newSubscription->description, description);
	} else {
		newSubscription->description = NULL;
	}
	
	getTraceLock(thr);
	if (UT_GLOBAL(tracePointSubscribers) == NULL) {
		/* no existing subscriptions in the linked list, simply add the new one */
		UT_GLOBAL(tracePointSubscribers) = newSubscription;
	} else {
		/* add the new subscription to the end of the linked list */
		for (subscription = UT_GLOBAL(tracePointSubscribers); subscription != NULL; subscription = subscription->next) {
			if (subscription->next == NULL) {
				subscription->next = newSubscription;
				newSubscription->prev = subscription;
				break;
			}
		}
	}
	/* pass the new subscription pointer back to the user */
	*subscriptionReference = newSubscription;
	freeTraceLock(thr);
	
	UT_DBGOUT(1, ("<UT> trcRegisterTracePointSubscriber normal exit, wrapper = %p\n",newSubscription->userData));
	return OMR_ERROR_NONE;
}

/*******************************************************************************
 * name        - trcDeregisterTraceDeregister
 * description - Deregister an external trace subscriber
 * parameters  - UtThreadData, subscription pointer.
 * returns     - JNI return codes
 ******************************************************************************/
static omr_error_t
trcDeregisterTracePointSubscriber(UtThreadData **thr, UtSubscription *subscriptionID)
{
	UtSubscription *subscription;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_DBGOUT(1, ("<UT> trcDeregisterTracePointSubscriber entered\n"));

	getTraceLock(thr);
	/* Locate the subscription in the linked list */
	subscription = UT_GLOBAL(tracePointSubscribers);
	while (subscription != NULL && subscription != subscriptionID) {
		subscription = subscription->next;
	}
	if (subscription == NULL) {
		UT_DBGOUT(1, ("<UT> trcDeregisterTracePointSubscriber, failed to find subscriber to deregister\n"));
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	
	UT_DBGOUT(1, ("<UT> trcDeregisterTracePointSubscriber found subscription, wrapper is %p\n",subscription->userData));
	
	/* Remove the subscription data structure from the linked list */
	if (subscription->prev != NULL) {
		subscription->prev->next = subscription->next;
	}
	if (subscription->next != NULL) {
		subscription->next->prev = subscription->prev;
	}
	if (subscription->prev == NULL) {
		UT_GLOBAL(tracePointSubscribers) = subscription->next;
	}
	if (subscription->subscriber != NULL) {
		/* subscriber still active, free the userData wrapper (already freed if alarm was called) */
		j9mem_free_memory(subscription->userData);
	}
	if (subscription->description != NULL ) {
		j9mem_free_memory(subscription->description);
	}
	j9mem_free_memory(subscription);
	freeTraceLock(thr);

	UT_DBGOUT(1, ("<UT> trcDeregisterTracePointSubscriber normal exit, tracePointSubscribers global = %p\n",UT_GLOBAL(tracePointSubscribers)));
	return OMR_ERROR_NONE;
}

/* TODO should be combined with runtimeSetTraceOptions()? */
static omr_error_t
trcSetOptions(UtThreadData **thr, const char *opts[])
{
	return setOptions(thr, opts, TRUE);
}

static void
setStartTime(void)
{

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));
	uint64_t hires[2], millis[2];
	int i = 0;

	UT_DBGOUT(1, ("<UT> SetStartTime called\n"));

	/*
	 * Try to get the time of the next millisecond rollover
	 */
	millis[0] = (uint64_t)j9time_current_time_millis();
	hires[0] = (uint64_t)j9time_hires_clock();
	do {
		i ^= 1;
		millis[i] = (uint64_t)j9time_current_time_millis();
		hires[i] = (uint64_t)j9time_hires_clock();
	} while (millis[0] == millis[1]);

	UT_GLOBAL(startPlatform) = ((hires[0] >> 1) + (hires[1] >> 1));
	UT_GLOBAL(startSystem) = millis[i];

	UT_DBGOUT(1, ("<UT> Start time initialized\n"));
}

omr_error_t
fillInUTInterfaces(UtInterface **utIntf, UtServerInterface *utServerIntf, UtModuleInterface *utModuleIntf)
{
	omr_error_t rc = OMR_ERROR_NONE;

	if ((NULL == utIntf) || (NULL == utServerIntf) || (NULL == utModuleIntf)) {
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	} else {
		/*
		 * Initialize the server interface...
		 */
		utServerIntfS.TraceVNoThread				= trcTraceVNoThread;
		utServerIntfS.TraceSnap						= trcTraceSnap;
		utServerIntfS.TraceSnapWithPriority 		= trcTraceSnapWithPriority;
		utServerIntfS.AddComponent					= trcAddComponent;
		utServerIntfS.GetComponents					= trcGetComponents;
		utServerIntfS.GetComponent					= trcGetComponent;
		utServerIntfS.Suspend						= trcSuspend;
		utServerIntfS.Resume						= trcResume;
		utServerIntfS.GetTracePointIteratorForBuffer= trcGetTracePointIteratorForBuffer;
		/* formatNextTracePoint function is also publicly exported for offline formatting. */
		utServerIntfS.FormatNextTracePoint			= omr_trc_formatNextTracePoint;
		utServerIntfS.FreeTracePointIterator		= trcFreeTracePointIterator;
		utServerIntfS.RegisterRecordSubscriber		= trcRegisterRecordSubscriber;
		utServerIntfS.DeregisterRecordSubscriber	= trcDeregisterRecordSubscriber;
		utServerIntfS.FlushTraceData				= trcFlushTraceData;
		utServerIntfS.GetTraceMetadata				= trcGetTraceMetadata;
		utServerIntfS.DisableTrace					= trcDisableTrace;
		utServerIntfS.EnableTrace					= trcEnableTrace;
		utServerIntfS.RegisterTracePointSubscriber	= trcRegisterTracePointSubscriber;
		utServerIntfS.DeregisterTracePointSubscriber= trcDeregisterTracePointSubscriber;
		utServerIntfS.TraceRegister					= trcTraceRegister;
		utServerIntfS.TraceDeregister				= trcTraceDeregister;
		utServerIntfS.SetOptions					= trcSetOptions;

		memcpy(utServerIntf, &utServerIntfS, sizeof(UtServerInterface));

		/*
		 * Initialize the direct module interface, these are
		 * wrappers to calls within trace that obtain a UtThreadData
		 * before calling the real function.
		 */
		utModuleIntfS.Trace           = omrTrace;
		utModuleIntfS.TraceMem        = omrTraceMem;
		utModuleIntfS.TraceState      = omrTraceState;
		utModuleIntfS.TraceInit       = omrTraceInit;
		utModuleIntfS.TraceTerm       = omrTraceTerm;

		memcpy(utModuleIntf, &utModuleIntfS, sizeof(UtModuleInterface));

		/*
		 * Make the interfaces available.
		 */

		/* Setup the pure OMR version of the UtInterface. */
		internalUtIntfS.server = &utServerIntfS;
		internalUtIntfS.client_unused = NULL;
		internalUtIntfS.module = &utModuleIntfS;

		externalUtIntfS.server = utServerIntf;
		externalUtIntfS.client_unused = NULL;
		externalUtIntfS.module = utModuleIntf;
		*utIntf = &externalUtIntfS;
	}
	return rc;
}

int32_t
getDebugLevel(void)
{
	return UT_GLOBAL(traceDebug);
}

/**
 * Add a subscription to UT_GLOBAL(subscribers).
 *
 * @pre hold UT_GLOBAL(subscribersLock)
 */
void
enlistRecordSubscriber(UtSubscription *subscription)
{
	if (UT_GLOBAL(subscribers) == NULL) {
		UT_GLOBAL(subscribers) = subscription;
	} else {
		subscription->next = (UtSubscription *)UT_GLOBAL(subscribers);
		UT_GLOBAL(subscribers)->prev = subscription;
		UT_GLOBAL(subscribers) = subscription;
	}
}

/**
 * Remove a subscription from UT_GLOBAL(subscribers).
 *
 * @pre hold UT_GLOBAL(subscribersLock)
 */
void
delistRecordSubscriber(UtSubscription *subscription)
{
	if (subscription->prev != NULL) {
		subscription->prev->next = subscription->next;
	}
	if (subscription->next != NULL) {
		subscription->next->prev = subscription->prev;
	}
	if (subscription->prev == NULL) {
		UT_GLOBAL(subscribers) = subscription->next;
	}
	subscription->next = NULL;
	subscription->prev = NULL;
}

/**
 * Free the memory for a subscription.
 *
 * @post subscription is freed, and must not be accessed anymore.
 */
void
deleteRecordSubscriber(UtSubscription *subscription)
{
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));
	j9mem_free_memory(subscription->description);
	j9mem_free_memory(subscription->queueSubscription);
	j9mem_free_memory(subscription);
}

/**
 * Find a subscription in UT_GLOBAL(subscribers).
 *
 * @pre hold UT_GLOBAL(subscribersLock)
 */
BOOLEAN
findRecordSubscriber(UtSubscription *subscription)
{
	if (NULL != subscription) {
		volatile UtSubscription *subscriberIter = UT_GLOBAL(subscribers);
		for (; NULL != subscriberIter; subscriberIter = subscriberIter->next) {
			if (subscriberIter == subscription) {
				return TRUE;
			}
		}
	}
	return FALSE;
}

/**
 * Destroy a record subscriber.
 *
 * The subscriber must be removed from UT_GLOBAL(subscribers) and deleted atomically,
 * with respect to other operations on UT_GLOBAL(subscribers), to resolve race conditions
 * between multiple threads attempting to destroy the same subscription.
 *
 * @pre hold UT_GLOBAL(traceLock)
 * @pre hold UT_GLOBAl(subscribersLock)
 * @post subscription is freed, and must not be accessed anymore
 */
omr_error_t
destroyRecordSubscriber(UtThreadData **thr, UtSubscription *subscription)
{
	omr_error_t result = unsubscribe(subscription->queueSubscription);

	/* No need to check whether subscription was enlisted, because next and prev ptrs will be NULL
	 * if it was not enlisted.
	 */
	delistRecordSubscriber(subscription);

	if (UT_GLOBAL(subscribers) == NULL) {
		UT_GLOBAL(traceInCore) = TRUE;
		UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Set traceInCore to TRUE\n", thr));
	}

	deleteRecordSubscriber(subscription);
	return result;
}
