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

#include "j9cfg.h"
#include "rastrace_internal.h"
#include "omrutilbase.h"
#include "omrstdarg.h"
#include "j9trcnls.h"
#include "trctrigger.h"
#include "j9rastrace.h"

#define MAX_QUALIFIED_NAME_LENGTH 16


static UtProcessorInfo *getProcessorInfo(void);
static void traceExternal(UtThreadData **thr, UtListenerWrapper func, void *userData, const char *modName, uint32_t traceId, const char *spec, va_list varArgs);
static void raiseAssertion(UtThreadData **thread, UtModuleInfo *modInfo, uint32_t traceId);
static void fireTriggerHit(UtThreadData **thread, char *compName, uint32_t traceId, TriggerPhase phase);

static void callSubscriber(UtThreadData **thr, UtSubscription *subscription, UtModuleInfo *modInfo, uint32_t traceId, va_list args);

char pointerSpec[2] = {(char)sizeof(char *), '\0'};
extern omrthread_tls_key_t j9rasTLSKey;

#define UNKNOWN_SERVICE_LEVEL "Unknown version"


/*******************************************************************************
 * name        - initEvent
 * description - Initializes a monitor. Takes a monitor name that will be copied.
 * parameters  - UtEventSem
 * returns     - int32_t
 ******************************************************************************/
omr_error_t
initEvent(UtEventSem **sem, char* name) 
{
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));
	omr_error_t ret = OMR_ERROR_NONE;
	UtEventSem *newSem;

	UT_DBGOUT(2, ("<UT> initEvent called\n"));
	newSem = j9mem_allocate_memory(sizeof(UtEventSem), OMRMEM_CATEGORY_TRACE);
	if (newSem !=NULL) {
		omrthread_monitor_t monitor;

		memset(newSem, '\0', sizeof(UtEventSem));
		initHeader(&newSem->header, "UTES", sizeof(UtEventSem));
		ret = (int32_t)omrthread_monitor_init_with_name(&monitor, 0, name);
		if (ret == 0) {
			newSem->pfmInfo.sem = monitor;
			*sem = newSem;
		}
	} else {
		ret = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
	UT_DBGOUT(2, ("<UT> initEvent returned %d for semaphore %p\n", ret, newSem));
	return ret;
}

/*******************************************************************************
 * name        - initEvent
 * description - Frees a monitor.
 * parameters  - UtEventSem
 * returns     - void
 ******************************************************************************/
void
destroyEvent(UtEventSem *sem)
{
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));
	intptr_t ret = 0;

	UT_DBGOUT(2, ("<UT> destroyEvent called for %p\n", sem));

	ret = omrthread_monitor_destroy(sem->pfmInfo.sem);
	if (0 == ret) {
		/* Prevent users that keep a stale pointer to the sem from
		 * getting into the omrthread library with it.
		 */
		sem->pfmInfo.sem = NULL;
		j9mem_free_memory(sem);
	}
}

/*******************************************************************************
 * name        - waitEvent
 * description - Waits for an event to occur
 * parameters  - UtEventSem
 * returns     - void
 ******************************************************************************/
void
waitEvent(UtEventSem * sem)
{
	omrthread_monitor_enter(sem->pfmInfo.sem);
	if (sem->pfmInfo.flags != UT_SEM_POSTED) {
		sem->pfmInfo.flags = UT_SEM_WAITING;
		omrthread_monitor_wait(sem->pfmInfo.sem);
		if (omrthread_monitor_num_waiting(sem->pfmInfo.sem) == 0) {
			sem->pfmInfo.flags = 0;
		}
	} else {
		sem->pfmInfo.flags = 0;
	}

	omrthread_monitor_exit(sem->pfmInfo.sem);
}

/*******************************************************************************
 * name        - postEvent
 * description - Wakes up the trace write thread
 * parameters  - UtEventSem
 * returns     - void
 ******************************************************************************/
void
postEvent(UtEventSem * sem)
{
	omrthread_monitor_enter(sem->pfmInfo.sem);
	if (sem->pfmInfo.flags == UT_SEM_WAITING) {
		omrthread_monitor_notify(sem->pfmInfo.sem);
	} else {
		sem->pfmInfo.flags = UT_SEM_POSTED;
	}
	omrthread_monitor_exit(sem->pfmInfo.sem);
}

/*******************************************************************************
 * name        - postEventAll
 * description - Wakes all threads waiting on this event
 * parameters  - UtEventSem
 * returns     - void
 ******************************************************************************/
void
postEventAll(UtEventSem * sem) 
{

	UT_DBGOUT(2, ("<UT> postEventAll called for semaphore %p\n", sem));
	omrthread_monitor_enter(sem->pfmInfo.sem);
	if (omrthread_monitor_num_waiting(sem->pfmInfo.sem) == 0) {
		sem->pfmInfo.flags = UT_SEM_POSTED;
	} else {
		sem->pfmInfo.flags = 0;
		omrthread_monitor_notify_all(sem->pfmInfo.sem);
	}
	omrthread_monitor_exit(sem->pfmInfo.sem);
	UT_DBGOUT(2, ("<UT> postEventAll for semaphore %p done\n", sem));
}

/*******************************************************************************
 * name        - initTraceHeader
 * description - Initializes the trace header.
 * parameters  - void
 * returns     - OMR error code
 ******************************************************************************/
omr_error_t
initTraceHeader(void)
{
	int                 size;
	UtTraceFileHdr     *trcHdr;
	char               *ptr;
	UtTraceCfg         *cfg;
	int                 actSize, srvSize, startSize;
	UtProcessorInfo			*procinfo;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	/*
	 *  Return if it already exists
	 */
	if (UT_GLOBAL(traceHeader) != NULL) {
		return OMR_ERROR_NONE;
	}

	size = offsetof(UtTraceFileHdr, traceSection);
	size += sizeof(UtTraceSection);

	/*
	 *  Calculate length of service section
	 */
	srvSize = offsetof(UtServiceSection, level);
	if (UT_GLOBAL(serviceInfo) == NULL) {

		/* Make sure we allocate this so we can free it on shutdown. */
		UT_GLOBAL(serviceInfo) = j9mem_allocate_memory(sizeof(UNKNOWN_SERVICE_LEVEL) + 1, OMRMEM_CATEGORY_TRACE);
		if( NULL == UT_GLOBAL(serviceInfo) ) {
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
		strcpy(UT_GLOBAL(serviceInfo), UNKNOWN_SERVICE_LEVEL);
	}
	srvSize += (int)strlen(UT_GLOBAL(serviceInfo)) + 1;
	srvSize = ((srvSize +
				UT_STRUCT_ALIGN - 1) / UT_STRUCT_ALIGN) * UT_STRUCT_ALIGN;
	size += srvSize;


	/*
	 *  Calculate length of startup options
	 */
	startSize = offsetof(UtStartupSection, options);
	if (UT_GLOBAL(properties) != NULL){
		startSize += (int)strlen(UT_GLOBAL(properties)) + 1;
	}
	startSize = ((startSize +
				UT_STRUCT_ALIGN - 1) / UT_STRUCT_ALIGN) * UT_STRUCT_ALIGN;
	startSize = ((startSize + 3) / 4) * 4;
	size += startSize;

	/*
	 *  Calculate length of trace activation commands
	 */
	actSize = offsetof(UtActiveSection, active);

	for (cfg = UT_GLOBAL(config);
		 cfg != NULL;
		 cfg = cfg->next) {
		actSize += (int)strlen(cfg->command) + 1;
	}
	actSize = ((actSize +
				UT_STRUCT_ALIGN - 1) / UT_STRUCT_ALIGN) * UT_STRUCT_ALIGN;
	actSize = ((actSize + 3) / 4) * 4;
	size += actSize;


	/*
	 *  Add length of UtProcSection
	 */
	size += sizeof(UtProcSection);

	if ((trcHdr = j9mem_allocate_memory(size, OMRMEM_CATEGORY_TRACE )) == NULL) {
		UT_DBGOUT(1, ("<UT> Out of memory in initTraceHeader\n"));
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	memset(trcHdr, '\0', size);
	initHeader(&trcHdr->header, UT_TRACE_HEADER_NAME, size);

	trcHdr->bufferSize       = UT_GLOBAL(bufferSize);
	trcHdr->endianSignature  = UT_ENDIAN_SIGNATURE;
	trcHdr->traceStart       = offsetof(UtTraceFileHdr, traceSection);
	trcHdr->serviceStart     = trcHdr->traceStart + sizeof(UtTraceSection);
	trcHdr->startupStart     = trcHdr->serviceStart + srvSize;
	trcHdr->activeStart      = trcHdr->startupStart + startSize;
	trcHdr->processorStart   = trcHdr->activeStart + actSize;


	/*
	 * Initialize trace section
	 */
	ptr = (char *)trcHdr + trcHdr->traceStart;
	initHeader((UtDataHeader *)ptr, UT_TRACE_SECTION_NAME,
				   sizeof(UtTraceSection));
	((UtTraceSection *)ptr)->startPlatform = UT_GLOBAL(startPlatform);
	((UtTraceSection *)ptr)->startSystem = UT_GLOBAL(startSystem);
	((UtTraceSection *)ptr)->type = UT_GLOBAL(traceInCore) ? UT_TRACE_INTERNAL : UT_TRACE_EXTERNAL;
	((UtTraceSection *)ptr)->generations = UT_GLOBAL(traceGenerations);
	((UtTraceSection *)ptr)->pointerSize = sizeof(void *);

	/*
	 * Initialize service level section
	 */
	ptr = (char *)trcHdr + trcHdr->serviceStart;
	initHeader((UtDataHeader *)ptr, UT_SERVICE_SECTION_NAME, srvSize);
	ptr += offsetof(UtServiceSection, level);
	strcpy(ptr, UT_GLOBAL(serviceInfo));

	/*
	 * Initialize startup option section
	 */
	ptr = (char *)trcHdr + trcHdr->startupStart;
	initHeader((UtDataHeader *)ptr, UT_STARTUP_SECTION_NAME, startSize);
	ptr += offsetof(UtStartupSection, options);
	if (UT_GLOBAL(properties) != NULL) {
		strcpy(ptr, UT_GLOBAL(properties));
		/*ptr += strlen(UT_GLOBAL(properties)) + 1;*/
	}

	/*
	 *  Fill in UtActiveSection with trace activation commands
	 */
	ptr = (char *)trcHdr + trcHdr->activeStart;
	initHeader((UtDataHeader *)ptr, UT_ACTIVE_SECTION_NAME, actSize);
	ptr += offsetof(UtActiveSection, active);
	for (cfg = UT_GLOBAL(config);
		 cfg != NULL;
		 cfg = cfg->next) {
		strcpy(ptr, cfg->command);
		ptr += strlen(cfg->command) + 1;
	}

	/*
	 *  Initialize UtProcSection
	 */
	ptr = (char *)trcHdr + trcHdr->processorStart;
	initHeader((UtDataHeader *)ptr, UT_PROC_SECTION_NAME,
				   sizeof(UtProcSection));
	ptr += offsetof(UtProcSection, processorInfo);
	procinfo = getProcessorInfo();
	if (procinfo == NULL){
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	} else {
		memcpy(ptr, procinfo, sizeof(UtProcessorInfo));
		j9mem_free_memory(procinfo);
	}

	UT_GLOBAL(traceHeader) = trcHdr;
	return OMR_ERROR_NONE;
}

/*******************************************************************************
 * name        - setTraceType
 * description - Sets the trace file header as internal or external
 * parameters  - UtThreadData
 * returns     - void
 ******************************************************************************/
static void
setTraceType(int bufferType)
{
	UtTraceFileHdr     *trcHdr = UT_GLOBAL(traceHeader);
	char               *ptr;

	/*
	 * Initialize trace section
	 */
	ptr = (char *)trcHdr + trcHdr->traceStart;

	((UtTraceSection *)ptr)->type = UT_GLOBAL(traceInCore) ? UT_TRACE_INTERNAL : UT_TRACE_EXTERNAL;

	if (bufferType == UT_NORMAL_BUFFER) {
		((UtTraceSection *)ptr)->generations = UT_GLOBAL(traceGenerations);
	} else if (bufferType == UT_EXCEPTION_BUFFER ) {
		((UtTraceSection *)ptr)->generations = 1;
	}
}

/*******************************************************************************
 * name        - freeBuffers
 * description - Place trace buffer(s) on global free queue
 * parameters  - thr, UtTraceBuffer *
 * returns     - Nothing
 ******************************************************************************/
void
freeBuffers(qMessage *msg)
{
	UtTraceBuffer *nextBuf, *trcBuf;
	uint32_t newFlags = 0;
	uint32_t oldFlags = 0;

	if (msg == NULL || msg->data == NULL){
		return;
	}

	trcBuf = (UtTraceBuffer*)msg->data;

	do {
		oldFlags = trcBuf->flags;
		newFlags = ~UT_TRC_BUFFER_WRITE & ~UT_TRC_BUFFER_PURGE & ~UT_TRC_BUFFER_ACTIVE & oldFlags;
	} while (!twCompareAndSwap32((unsigned int *)(&trcBuf->flags),
								 oldFlags,
								 newFlags));

	if (oldFlags & UT_TRC_BUFFER_PURGE) {
		if (UT_GLOBAL(traceInCore)) {
			UtTraceBuffer *lastQueued = NULL;
			UtTraceBuffer *nextQueued = NULL;

			nextBuf = trcBuf->next;

			/* Most of the time we'll only have one buffer to deal with if we're using
			 * in core trace. The exception is when we've moved from external trace
			 * to in core trace where our last subscriber has deregistered.
			 * In this case it's possible that not all buffers in the queue have been
			 * written out by the time we reach here (buffers will be processed in order
			 * from the queue, but we call freeBuffers directly under some circumstances).
			 */

			for (nextQueued = nextBuf; nextQueued != NULL && nextQueued != trcBuf; nextQueued = nextQueued->next) {
				if ((nextQueued->flags & UT_TRC_BUFFER_WRITE)) {
					lastQueued = nextQueued;
				}
			}

			if (lastQueued != NULL) {
				UT_DBGOUT(5, ("<UT> found a queued buffer in in-core trace mode: " UT_POINTER_SPEC "\n", lastQueued));

				/* if there is a buffer queued for writing then make that purge instead of this one */
				do {
					oldFlags = lastQueued->flags;
					newFlags = UT_TRC_BUFFER_PURGE | oldFlags;
				} while ((oldFlags & UT_TRC_BUFFER_WRITE) && !twCompareAndSwap32((unsigned int *)(&lastQueued->flags),
											 oldFlags,
											 newFlags));

				/* check to make sure that state didn't change under us */
				if (oldFlags & UT_TRC_BUFFER_WRITE) {
					/* this chain will be purged when lastQueued is written so bail */
					return;
				}
			}
		}

		nextBuf = trcBuf->next;

		/*
		 *  If this is a circular chain of buffers then break the circle
		 */
		if (nextBuf != NULL) {
			trcBuf->next = NULL;
		} else {
			nextBuf = trcBuf;
		}

		UT_DBGOUT(5, ("<UT> adding buffer " UT_POINTER_SPEC " to free list\n", nextBuf));

		/* sanity check the chain */
		if (UT_GLOBAL(traceDebug) > 0) {
			UtTraceBuffer *buf;

			for (buf = nextBuf; buf != NULL; buf = buf->next) {
				DBG_ASSERT((UT_GLOBAL(traceInCore) || buf->queueData.next == CLEANING_MSG_FLAG || buf->flags & UT_TRC_BUFFER_NEW) && buf->queueData.referenceCount == 0 && buf->queueData.subscriptions == 0 && buf->queueData.pauseCount == 0);
			}
		}

		omrthread_monitor_enter(UT_GLOBAL(freeQueueLock));
		
		trcBuf->next = UT_GLOBAL(freeQueue);
		UT_GLOBAL(freeQueue) = nextBuf;

		omrthread_monitor_exit(UT_GLOBAL(freeQueueLock));
	}
}

/*******************************************************************************
 * name        - openTraceFile
 * description - Open a trace file
 * parameters  - UtThreadData, filename or NULL for external trace
 * returns     - The file handle or -1 if any errors encountered
 ******************************************************************************/
static intptr_t
openTraceFile(char *filename)
{
	intptr_t trcFile;
	char  replaceChar[36] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	/*
	 *  Check for external trace file
	 */

	if (filename == NULL) {
		filename = UT_GLOBAL(traceFilename);
		/*
		 *  Check for multi-generation mode
		 */
		if (UT_GLOBAL(traceGenerations) > 1) {
			*(UT_GLOBAL(generationChar)) =
				replaceChar[UT_GLOBAL(nextGeneration)];
			(UT_GLOBAL(nextGeneration))++;
			if (UT_GLOBAL(nextGeneration) >=
				   UT_GLOBAL(traceGenerations)) {
				UT_GLOBAL(nextGeneration) = 0;
			}
		}
	}

	UT_DBGOUT(1, ("<UT> Opening trace file \"%s\"\n", filename));
	/*
	 *  Try opening an existing file, and if that fails, create one
	 */
	if ((trcFile = j9file_open(filename, EsOpenWrite | EsOpenTruncate | EsOpenCreateNoTag, 0)) == -1) {
		if ((trcFile = j9file_open(filename, EsOpenWrite | EsOpenCreate | EsOpenCreateNoTag, 0666)) == -1) {
			/* Error opening tracefile: %s */
			j9nls_printf(PORTLIB, J9NLS_WARNING | J9NLS_STDERR, J9NLS_TRC_FILE_OPEN_FAIL_STR, filename);
			trcFile = -1;
		}
	}

	/*
	 * If the open worked, write out the header
	 */
	if (trcFile != -1) {
		if (j9file_write(trcFile, UT_GLOBAL(traceHeader),
						 UT_GLOBAL(traceHeader->header.length)) !=
		   (int)UT_GLOBAL(traceHeader->header.length)) {
			/* Error writing header to tracefile: %s */
			j9nls_printf(PORTLIB, J9NLS_WARNING | J9NLS_STDERR, J9NLS_TRC_HEADER_WRITE_FAIL_STR, filename);
			j9file_close(trcFile);
			trcFile = -1;
		}
	}
	return trcFile;
}

/*******************************************************************************
 * name        - closeTraceFile
 * description - Close a trace file
 * parameters  - UtThreadData, file handle, filename, filesize
 * returns     - Nothing
 ******************************************************************************/
static void
closeTraceFile(intptr_t trcFile, char *filename,
			   int64_t maxFileSize)
{
	int rc;
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	rc = j9file_set_length(trcFile, maxFileSize);
	if (0 != rc) {
		UT_DBGOUT(1, ("<UT> Error from j9file_set_length for tracefile: %s\n", filename));
	}
	j9file_close(trcFile);
}


/*******************************************************************************
 * name        - queueWrite
 * description - Place a buffer on the write queue if safe to do so
 * parameters  - thr, buffer address, flag
 * returns     - a pointer to the buffer queued or NULL if not queued
 ******************************************************************************/
UtTraceBuffer *
queueWrite(UtTraceBuffer *trcBuf, int flags)
{
	uint32_t newFlags;
	uint32_t oldFlags;
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_DBGOUT(5, ("<UT> QueueWrite entered for buffer "UT_POINTER_SPEC", flags 0x%x, existing flags 0x%x\n", trcBuf, flags, trcBuf->flags));

	/*
	 * First, set the write flag. Reset buffer active flag for external trace
	 */
	do {
		oldFlags = trcBuf->flags;
		newFlags = ~UT_TRC_BUFFER_ACTIVE & ((uint32_t)flags | oldFlags);
	} while (!twCompareAndSwap32((unsigned int *)(&trcBuf->flags),
								 oldFlags,
								 newFlags));

	/* Only put on queue if the buffer was active */
	if ((oldFlags & UT_TRC_BUFFER_ACTIVE) && !(oldFlags & UT_TRC_BUFFER_NEW)) {
		trcBuf->record.writePlatform = j9time_hires_clock();
		trcBuf->record.writeSystem = ((uint64_t) j9time_current_time_millis());
		trcBuf->record.writePlatform = (trcBuf->record.writePlatform >> 1) + (j9time_hires_clock() >> 1);

		if (publishMessage(&UT_GLOBAL(outputQueue), &trcBuf->queueData) == TRUE) {
			return trcBuf;
		}
	} else if (oldFlags & UT_TRC_BUFFER_PURGE) {
		UT_DBGOUT(1, ("<UT> skipping queue write for buffer "UT_POINTER_SPEC" with purge set, flags 0x%x, belonging to UT thread "UT_POINTER_SPEC"\n", trcBuf, oldFlags, trcBuf->thr));
	}

	return NULL;
}


/*******************************************************************************
 * name        - openSnap
 * description - Open a file in order to snap internal trace buffers
 * parameters  - UtThreadData, filename
 * returns     - handle to snap file
 ******************************************************************************/

intptr_t
openSnap(char *label)
{
#define FILENAMELEN 64
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	static char fileName[FILENAMELEN];

	UT_DBGOUT(1, ("<UT> Trace snap requested\n"));

	if (initTraceHeader() != OMR_ERROR_NONE){
		return OMR_ERROR_INTERNAL;
	}

	UT_GLOBAL(snapSequence)++;

	if ( label == NULL ) {
		uintptr_t pid = j9sysinfo_get_pid();
		int64_t curTime = j9time_current_time_millis();
		struct J9StringTokens* stringTokens = j9str_create_tokens(curTime);

		j9str_set_token(PORTLIB, stringTokens, "pid", "%lld", pid);
		j9str_set_token(PORTLIB, stringTokens, "sid", "%04.4d", UT_GLOBAL(snapSequence));

		j9str_subst_tokens(fileName, FILENAMELEN, "Snap%sid.%Y%m%d%H%M%S.%pid.trc", stringTokens);
		j9str_free_tokens(stringTokens);

#undef FILENAMELEN
		label = fileName;
	}
	/*
	 *  Open external trace file
	 */
	setTraceType(UT_NORMAL_BUFFER);
	return openTraceFile(label);
}



/*******************************************************************************
 * name        - subscriptionHandler
 * description - Wrapper thread for a buffer subscriber
 * parameters  - arg: a UtSubscription pointer with userdata->description set
 * returns     - non zero for unclean exit
 ******************************************************************************/
int
subscriptionHandler(void *arg)
{
	UtSubscription *subscription = (UtSubscription*)arg;
	UtThreadData thrData;
	UtThreadData *thrSlot = &thrData;
	UtThreadData **thr = &thrSlot;
	char *description = subscription->description;
	qMessage *trcBuf = NULL;
	int32_t detachThread = FALSE;
	omr_error_t rc = OMR_ERROR_NONE;
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	J9JavaVM *vm = (J9JavaVM *)(UT_GLOBAL(vm)->_language_vm);
#endif

	subscription->thr = thr;
	subscription->dataLength = UT_GLOBAL(bufferSize);

	if (subscription->threadAttach) {
		if (OMR_ERROR_NONE != twThreadAttach(thr, description)) {
			goto cleanup;
		}
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
		if (NULL != vm->javaOffloadSwitchOnWithReasonFunc) {
			J9VMThread *vmThread = (J9VMThread *)OMR_VM_THREAD_FROM_UT_THREAD(thr)->_language_vmthread;
			(*vm->javaOffloadSwitchOnWithReasonFunc)(vmThread, J9_JNI_OFFLOAD_SWITCH_TRACE_SUBSCRIBER_THREAD);
		}
#endif
	}

	/* Make sure this thread isn't traced */
	incrementRecursionCounter(*thr);

	UT_DBGOUT(1, ("<UT> Trace subscriber thread \"%s\" started\n", description));

	if (OMR_ERROR_NONE != initTraceHeader()) {
		goto cleanup;
	}

	/* handler main loop */
	do {
		utsSubscriberCallback subscriber;

		if (subscription->threadAttach) {
			if (((int32_t)omrthread_get_priority(OS_THREAD_FROM_UT_THREAD(thr))) != subscription->threadPriority) {
				omrthread_set_priority(OS_THREAD_FROM_UT_THREAD(thr), subscription->threadPriority);
			}
		}

		trcBuf = acquireNextMessage(subscription->queueSubscription);
		subscriber = subscription->subscriber;

		if (trcBuf == NULL) {
			UT_DBGOUT(5, ("<UT> Subscription handler exiting from NULL message for subscription " UT_POINTER_SPEC "\n", subscription));
			break;
		}

		if (UT_SUBSCRIPTION_KILLED == subscription->state) {
			UT_DBGOUT(5, ("<UT> Subscription handler exiting due to deregistration of subscription " UT_POINTER_SPEC "\n", subscription));
			releaseCurrentMessage(subscription->queueSubscription);
			break;
		}

		if (subscription->description == NULL) {
			UT_DBGOUT(5, ("<UT> Passing buffer " UT_POINTER_SPEC " to " UT_POINTER_SPEC "\n", trcBuf, subscription->subscriber));
		} else {
			UT_DBGOUT(5, ("<UT> Passing buffer " UT_POINTER_SPEC " to \"%s\"\n", trcBuf, subscription->description));
		}

		/* TODO: j9sig_protect and mprotect */
		subscription->data = &((UtTraceBuffer*)trcBuf->data)->record;
		rc = subscriber(subscription);

		releaseCurrentMessage(subscription->queueSubscription);

		/* checking the return code from the subscriber callback */
		if (OMR_ERROR_NONE != rc) {
			UT_DBGOUT(1, ("<UT> Removing trace subscription for \"%s\" due to subscriber error %i\n", description, rc));
			break;
		}

	} while (UT_SUBSCRIPTION_KILLED != subscription->state);

cleanup:
	UT_DBGOUT(1, ("<UT> Trace subscriber thread \"%s\" stopping\n", description));

	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Acquiring lock for handler cleanup\n", thr));
	omrthread_monitor_enter(UT_GLOBAL(subscribersLock));
	getTraceLock(thr);
	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Lock acquired for handler cleanup\n", thr));

	if (subscription->alarm != NULL) {
		UT_DBGOUT(3, ("<UT> Calling alarm function " UT_POINTER_SPEC " for \"%s\"\n", subscription->alarm, description));
		subscription->alarm(subscription);
		UT_DBGOUT(3, ("<UT> Returned from alarm function " UT_POINTER_SPEC "\n", subscription->alarm, description));
	}

	if (thrSlot != &thrData) {
		detachThread = TRUE;
	}

	if (UT_SUBSCRIPTION_KILLED == subscription->state) {
		subscription->state = UT_SUBSCRIPTION_DEAD;
		/* The killer destroys the subscription */
	} else {
		destroyRecordSubscriber(thr, subscription);
	}
	
	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Releasing lock for cleanup on handler exit\n", thr));

	/* Need to exit the trace lock first as detaching a thread needs the vm thread list lock.
	 * This can be held by another thread (especially one doing GC) that is trying to write a tracepoint
	 * and waiting for the trace lock itself (for example to write to the global gc trace buffer).
	 * See CMVC 194605 for details.
	 */
	/* @alin-todo Why don't we decrement the recursion counter, which
	 * was incremented by getTraceLock() above?
	 * Not very important since the thread is about to exit anyway.
	 */
	omrthread_monitor_exit(UT_GLOBAL(traceLock));
	omrthread_monitor_notify_all(UT_GLOBAL(subscribersLock));
	omrthread_monitor_exit(UT_GLOBAL(subscribersLock));

	if (detachThread) {
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
		if (NULL != vm->javaOffloadSwitchOffWithReasonFunc) {
			J9VMThread *vmThread = (J9VMThread *)OMR_VM_THREAD_FROM_UT_THREAD(thr)->_language_vmthread;
			(vm->javaOffloadSwitchOffWithReasonFunc)(vmThread, J9_JNI_OFFLOAD_SWITCH_TRACE_SUBSCRIBER_THREAD);
		}
#endif
		twThreadDetach(thr);
	}
	return 0;
}


/*******************************************************************************
 * name        - writeBuffer
 * description - Trace Writer main function to write buffers to disk
 * parameters  - UtSubscription *
 * returns     - OMR_ERROR_NONE on success, otherwise error
 ******************************************************************************/
omr_error_t
writeBuffer(UtSubscription *subscription)
{
	TraceWorkerData *state = subscription->userData;
	UtThreadData **thr = NULL;
	UtTraceBuffer *trcBuf;
	intptr_t outputFile = -1;
	int64_t *fileSize;
	int64_t *maxFileSize;
	int32_t *wrap;
	int32_t bufferType;
	char *filename;
	int32_t rc;
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	thr = subscription->thr;

	/* using subscription->queueSubscription->current like this is ugly, but we're constrained to only pass the
	 * subscription, so the buffer type isn't accessible via in-band means.
	 */
	trcBuf = (UtTraceBuffer*)subscription->queueSubscription->current->data;
	bufferType = trcBuf->bufferType;
	switch (bufferType) {
		case UT_NORMAL_BUFFER:
			UT_DBGOUT(5, ("<UT thr=" UT_POINTER_SPEC "> processing TraceRecord " UT_POINTER_SPEC " of type UT_NORMAL_BUFFER\n", thr, trcBuf));
			outputFile = state->trcFile;
			fileSize = &state->trcSize;
			maxFileSize = &state->maxTrc;
			filename = UT_GLOBAL(traceFilename);
			wrap = &UT_GLOBAL(traceWrap);
			break;
		case UT_EXCEPTION_BUFFER:
			UT_DBGOUT(5, ("<UT thr=" UT_POINTER_SPEC "> processing TraceRecord " UT_POINTER_SPEC " of type UT_EXCEPTION_BUFFER\n", thr, trcBuf));
			outputFile = state->exceptFile;
			fileSize = &state->exceptSize;
			maxFileSize = &state->maxExcept;
			filename = UT_GLOBAL(exceptFilename);
			wrap = &UT_GLOBAL(exceptTraceWrap);
			break;
		default:
			/* not a buffer type we know about so skip it */
			return OMR_ERROR_NONE;
			break;
	}

	if (outputFile != -1) {
		UT_DBGOUT(5, ("<UT thr=" UT_POINTER_SPEC "> writeBuffer writing buffer " UT_POINTER_SPEC " to %s\n", thr, trcBuf, filename));

		/*
		 *  Write the record
		 */
		*fileSize += subscription->dataLength;
		rc = (int32_t)j9file_write(outputFile, subscription->data, (int32_t)subscription->dataLength);
		if (rc != subscription->dataLength) {
			/* Error writing %d bytes to tracefile: %s rc: %d */
			j9nls_printf(PORTLIB, J9NLS_WARNING | J9NLS_STDERR, J9NLS_TRC_TRACE_WRITE_FAIL_STR, subscription->dataLength, filename, rc);
			*fileSize = -1;
			return OMR_ERROR_INTERNAL;
		}

		/*
		 * Check for file wrap
		 */
		if (*wrap != 0 && *fileSize >= *wrap) {
			/* Trace options may have changed, re-initialize the trace file header data if necessary */
			initTraceHeader();
			
			if ((bufferType == UT_NORMAL_BUFFER) && (UT_GLOBAL(traceGenerations) > 1)) {
				/* For multiple-generation file mode, open the next file */
				j9file_close(outputFile);
				setTraceType(UT_NORMAL_BUFFER);
				state->trcFile = openTraceFile(NULL);
				if (state->trcFile > 0) {
					*fileSize = UT_GLOBAL(traceHeader->header.length);
					*maxFileSize = *fileSize;
					outputFile = state->trcFile;
				} else {
					/* Error opening next generation: %s */
					j9nls_printf(PORTLIB, J9NLS_WARNING | J9NLS_STDERR, J9NLS_TRC_NEXT_GEN_FILE_OPEN_FAIL_STR, filename);
					*fileSize = -1;
					return OMR_ERROR_INTERNAL;
				}
			} else {
				/* For single file wrap mode, seek back to start of file */ 
				*maxFileSize = *fileSize;
				*fileSize = j9file_seek(outputFile, 0, SEEK_SET);
				if (*fileSize != 0 ) {
					/* Error performing seek in trace file: %s */
					j9nls_printf(PORTLIB, J9NLS_WARNING | J9NLS_STDERR, J9NLS_TRC_FILE_SEEK_FAIL_STR, filename);
					*fileSize = -1;
					return OMR_ERROR_INTERNAL;
				}
				*fileSize = j9file_write(outputFile, UT_GLOBAL(traceHeader), UT_GLOBAL(traceHeader->header.length));
				if (*fileSize != UT_GLOBAL(traceHeader->header.length)) {
					/* Error writing %d bytes to trace file: %s rc: %d */
					j9nls_printf(PORTLIB, J9NLS_WARNING | J9NLS_STDERR, J9NLS_TRC_TRACE_WRITE_FAIL_STR, UT_GLOBAL(traceHeader->header.length), filename, rc);
					*fileSize = -1;
					return OMR_ERROR_INTERNAL;
				}
			} 
		}

		if (*fileSize > *maxFileSize) {
			*maxFileSize = *fileSize;
		}
	}

	return OMR_ERROR_NONE;
}


/*******************************************************************************
 * name        - getTrcBuf
 * description - Get and initialize a TraceBuffer
 * parameters  - thr, current buffer pointer or null, buffer type
 * returns     - TraceBuffer pointer or NULL
 ******************************************************************************/
static UtTraceBuffer *
getTrcBuf(UtThreadData **thr, UtTraceBuffer * oldBuf, int bufferType)
{
	UtTraceBuffer *nextBuf = NULL;
	UtTraceBuffer *trcBuf;
	int32_t      newBuffer = FALSE;
	uint32_t       typeFlags = UT_TRC_BUFFER_ACTIVE | UT_TRC_BUFFER_NEW;
	uint64_t writePlatform, writeSystem;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	writePlatform = j9time_hires_clock();
	writeSystem = ((uint64_t) j9time_current_time_millis());
	writePlatform = (writePlatform >> 1) + (j9time_hires_clock() >> 1);

	if (oldBuf != NULL) {
		/*
		 *  Update write timestamp in case of a dump being taken before
		 *  the buffer is ever written to disk for any reason
		 */
		oldBuf->record.writeSystem = writeSystem;
		oldBuf->record.writePlatform = writePlatform;

		/* Null the thread reference as there's no guaranty it's valid after this point */
		oldBuf->thr = NULL;

		if (UT_GLOBAL(traceInCore)) {
			/*
			 *  In core trace mode so reuse existing buffer, wrapping to the top
			 */
			trcBuf = oldBuf;

			goto out;

		} else {
			/*
			 *  External trace mode
			 */
			nextBuf = oldBuf->next;

			/* Is there a new buffer we can switch to? */
			if (nextBuf && ((nextBuf->flags & UT_TRC_BUFFER_WRITE) == 0)) {
				DBG_ASSERT(nextBuf->queueData.next == CLEANING_MSG_FLAG && nextBuf->queueData.referenceCount == 0 && nextBuf->queueData.subscriptions == 0 && nextBuf->queueData.pauseCount == 0);

				/* YES - put the full buffer on the write thread */
				if (queueWrite(oldBuf, UT_TRC_BUFFER_FULL) != NULL) {
					notifySubscribers(&UT_GLOBAL(outputQueue));
				}

				/* Set up nextBuf */
				/* it's okay to set the global buffers here and initialize later because the entire
				 * function call's under the trace lock so nothing can be written to it until we release.
				 */
				if (bufferType == UT_NORMAL_BUFFER) {
					(*thr)->trcBuf = nextBuf;
				} else if (bufferType == UT_EXCEPTION_BUFFER) {
					UT_GLOBAL(exceptionTrcBuf) = nextBuf;
				}

				trcBuf = nextBuf;
				/* make sure that if we've dropped buffers that we don't double account */
				trcBuf->lostCount = 0;

				goto out;
			}

			/* In nodynamic mode we allow up to 3 buffers per thread then we spill tracepoints */
			if (nextBuf && !UT_GLOBAL(dynamicBuffers) && nextBuf->next != oldBuf) {
				/*
				 * We're using "nodynamic" buffering, so we won't be simply
				 * mallocing another buffer. Reuse the current buffer and flag
				 * the fact that we have lost tracepoints.
				 */
				if (UT_GLOBAL(lostRecords) == 0) {
					UT_DBGOUT(1, ("<UT> Trace buffer discarded. Count of discarded buffers will be printed at VM shutdown\n"));
				}

				UT_DBGOUT(4, ("<UT> discarding buffer because "UT_POINTER_SPEC" queued\n", nextBuf));

				UT_ATOMIC_INC((volatile uint32_t*)&UT_GLOBAL(lostRecords));

				oldBuf->lostCount += 1;

				/* it's okay to set the global buffers here and initialize later because the entire
				 * function calls under the trace lock so nothing can be written to it until we release.
				 */
				if (bufferType == UT_NORMAL_BUFFER) {
					(*thr)->trcBuf = oldBuf;
				} else if (bufferType == UT_EXCEPTION_BUFFER) {
					UT_GLOBAL(exceptionTrcBuf) = oldBuf;
				}

				trcBuf = oldBuf;

				goto out;
			}

			if (queueWrite(oldBuf, UT_TRC_BUFFER_FULL) != NULL) {
				notifySubscribers(&UT_GLOBAL(outputQueue));
			}
		}
	}

	/*
	 * Reuse buffer if there is one
	 */
	omrthread_monitor_enter(UT_GLOBAL(freeQueueLock));
	
	trcBuf = UT_GLOBAL(freeQueue);
	if (NULL != trcBuf) {
		UT_GLOBAL(freeQueue) = trcBuf->next;
	}
	
	omrthread_monitor_exit(UT_GLOBAL(freeQueueLock));
	
	if (trcBuf != NULL) {
		DBG_ASSERT(trcBuf->queueData.next == NULL || trcBuf->queueData.next == CLEANING_MSG_FLAG);
		DBG_ASSERT(trcBuf->queueData.referenceCount == 0);
		DBG_ASSERT(trcBuf->queueData.subscriptions == 0);
		DBG_ASSERT(trcBuf->queueData.pauseCount == 0);
	}

	/*
	 *  If no buffers, try to obtain one
	 */
	if (trcBuf == NULL) {
		PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

		/*
		 * CMVC 164637: Don't use memory allocation routines when handling signals.
		 * Returning NULL will make us spill some trace points.
		 */
		if (j9sig_get_current_signal()) {
			return NULL;
		}

		newBuffer = TRUE;
		trcBuf = j9mem_allocate_memory(UT_GLOBAL(bufferSize) +
							 offsetof(UtTraceBuffer, record), OMRMEM_CATEGORY_TRACE );

		if (trcBuf == NULL) {
			if (UT_GLOBAL(dynamicBuffers) == TRUE) {
				UT_GLOBAL(dynamicBuffers) = FALSE;
				/* Native memory allocation failure, falling back to nodynamic trace settings. */
				j9nls_printf(PORTLIB, J9NLS_WARNING | J9NLS_STDERR, J9NLS_TRC_NODYNAMIC_FALLBACK );
				trcBuf = getTrcBuf(thr, oldBuf, bufferType);
			}

			return trcBuf;
		}

		UT_ATOMIC_INC((volatile uint32_t*)&UT_GLOBAL(allocatedTraceBuffers));
		UT_DBGOUT(1, ("<UT> Allocated buffer %i " UT_POINTER_SPEC ", queue tail is "UT_POINTER_SPEC"\n", UT_GLOBAL(allocatedTraceBuffers), trcBuf, UT_GLOBAL(outputQueue).tail));
	}


	/*
	 *  Initialize buffer for this thread
	 */
	UT_DBGOUT(5, ("<UT> Buffer " UT_POINTER_SPEC " obtained for thread " UT_POINTER_SPEC "\n",
			   trcBuf, thr));
	initHeader(&trcBuf->header, UT_TRACE_BUFFER_NAME,
			   UT_GLOBAL(bufferSize) + offsetof(UtTraceBuffer, record));
	trcBuf->next = NULL;
	trcBuf->lostCount = 0;
	trcBuf->bufferType = bufferType;
	trcBuf->queueData.data = trcBuf;
	trcBuf->queueData.next = CLEANING_MSG_FLAG;
	trcBuf->queueData.referenceCount = 0;
	trcBuf->queueData.pauseCount = 0;
	trcBuf->queueData.nextSecondary = NULL;
	trcBuf->queueData.subscriptions = 0;
	trcBuf->thr = NULL;


	if (bufferType == UT_NORMAL_BUFFER) {
		trcBuf->record.threadId   = (uint64_t)(uintptr_t)(*thr)->id;
		trcBuf->record.threadSyn1 = (uint64_t)(uintptr_t)(*thr)->synonym1;
		trcBuf->record.threadSyn2 = (uint64_t)(uintptr_t)(*thr)->synonym2;
		strncpy(trcBuf->record.threadName, (*thr)->name,
				UT_MAX_THREAD_NAME_LENGTH);
		trcBuf->record.threadName[UT_MAX_THREAD_NAME_LENGTH] = '\0';
		(*thr)->trcBuf = trcBuf;
	} else if (bufferType == UT_EXCEPTION_BUFFER) {
		trcBuf->record.threadId = 0;
		trcBuf->record.threadSyn1 = 0;
		trcBuf->record.threadSyn2 = 0;

		strcpy(trcBuf->record.threadName, UT_EXCEPTION_THREAD_NAME);
		UT_GLOBAL(exceptionTrcBuf) = trcBuf;
	}

	trcBuf->record.firstEntry = offsetof(UtTraceRecord, threadName) +
							  (int32_t)strlen(trcBuf->record.threadName) + 1;

	/*
	 * Update circular buffer list for external trace
	 */

	if (oldBuf != NULL) {
		trcBuf->next = oldBuf->next;
		oldBuf->next = trcBuf;
		if (trcBuf->next == NULL) {
			trcBuf->next = oldBuf;
		}
	}


out:

	trcBuf->flags = typeFlags;

	/* we reset the contents of the buffer first so that even if we're in the middle of flushing/writing this buffer
	 * the record maintains it's consistency.
	 */
	trcBuf->record.nextEntry = trcBuf->record.firstEntry;
	issueReadBarrier();
	*((char *)&trcBuf->record + trcBuf->record.firstEntry) = '\0';

	if (trcBuf->lostCount > 0) {
		/* moved from traceV because if we're wrapping data there's no guarantee that
		 * will be able to put this at the start of a trace record.
		 */
		char *p = (char *)&trcBuf->record + trcBuf->record.firstEntry;
		*p = '\0';
		*(p + 1) = UT_TRC_LOST_COUNT_ID[0];
		*(p + 2) = UT_TRC_LOST_COUNT_ID[1];
		*(p + 3) = UT_TRC_LOST_COUNT_ID[2];
		memcpy(p + 4, (char *)&(trcBuf->lostCount), sizeof(int32_t));
		*(p + 8) = 8;
		trcBuf->record.nextEntry = trcBuf->record.firstEntry + 8;
		UT_ATOMIC_OR((volatile uint32_t *)(&trcBuf->flags), UT_TRC_BUFFER_ACTIVE);
	}

	trcBuf->record.sequence = writePlatform;
	trcBuf->record.wrapSequence = trcBuf->record.sequence;

	/* in case this buffer is dumped rather than queued we want non zero values for the write times */
	trcBuf->record.writeSystem = writeSystem;
	trcBuf->record.writePlatform = writePlatform;

	/*
	 * Finally, place buffer on global queue if it is newly allocated
	 */
	if (newBuffer) {
		do {
			trcBuf->globalNext = UT_GLOBAL(traceGlobal);
		} while (!twCompareAndSwapPtr((uintptr_t *)&UT_GLOBAL(traceGlobal),
									   (uintptr_t)trcBuf->globalNext,
									   (uintptr_t)trcBuf));
	}

	return trcBuf;
}

/*******************************************************************************
 * name        - copyToBuffer
 * description - Routine to copy data to trace buffer if it may wrap
 * parameters  - UtThreadData,  buffer type, trace data, current tracebuffer
 *               pointer, length of the data to copy, traceentry length and
 *               start of the buffer
 * returns     - void
 ******************************************************************************/
static void
copyToBuffer(UtThreadData **thr,
			 int bufferType,
			 char *var,
			 char ** p,
			 int length,
			 int * entryLength,
			 UtTraceBuffer ** trcBuf)
{
	int bufLeft = (int)((char *)&(*trcBuf)->record +
					  UT_GLOBAL(bufferSize) - *p);
	int remainder;
	char *str;


	/*
	 * Will this exceed maximum trace record length ?
	 */
	if (*entryLength + length > UT_MAX_EXTENDED_LENGTH) {
		length = UT_MAX_EXTENDED_LENGTH - *entryLength;
		if (length <= 0) {
			return;
		}
	}

	/*
	 *  Will the record fit in the buffer ?
	 */
	if (length < bufLeft) {
		memcpy(*p, var, length);
		*entryLength += length;
		*p += length;
	} else {
		/*
		 *  Otherwise, copy what will fit...
		 */
		str = var;
		remainder = length;
		if (bufLeft > 0) {
			memcpy(*p, str, bufLeft);
			remainder -= bufLeft;
			*entryLength += bufLeft;
			*p += bufLeft;
			str += bufLeft;
		}
		/*
		 *  ... and get another buffer
		 */
		while (remainder > 0) {
			int lostCount = (*trcBuf)->lostCount;
			/* getTrcBuf() can return NULL in error situations (eg if we are out of native memory), so use a local
			 * buffer pointer here, and only update the caller's buffer pointer after the NULL check. JTC-JAT 87840
			 */
			UtTraceBuffer *nextBuf = getTrcBuf(thr, *trcBuf, bufferType);

			if (nextBuf != NULL) {
				*trcBuf = nextBuf; /* update caller's trace buffer pointer to point to next buffer */
				
				/* we're about to copy stuff into it so mark the buffer as not new so it can be queued */
				UT_ATOMIC_AND((volatile uint32_t *)(&(*trcBuf)->flags), ~UT_TRC_BUFFER_NEW);
				(*trcBuf)->thr = thr;

				/* we need to update p and bufLeft irrespective of what happens next */
				*p = (char *)&(*trcBuf)->record + (*trcBuf)->record.nextEntry;
				bufLeft = UT_GLOBAL(bufferSize) - (*trcBuf)->record.nextEntry;

				/* if there's already a tracepoint in the buffer we need to use nextEntry +1 */
				if ((*trcBuf)->record.nextEntry != (*trcBuf)->record.firstEntry) {
					(*p)++;
					bufLeft--;
				} else {
					/* there is a possibility that we will span this entire buffer with the current trace point
					 * but not with the data we are currently copying so we need to set this here as a precaution.
					 * nextEntry is always reconstructed at the end of the trace point from the cursor so this is
					 * overwritten if it's not needed.
					 */
					(*trcBuf)->record.nextEntry = -1;
				}

				/* have we just wrapped the buffer with the start of this trace point in it? */
				if ((*trcBuf)->lostCount == (lostCount +1)) {
					/* we've wrapped on our last buffer, so bail here */
					return;
				}

				if (remainder < bufLeft) {
					memcpy(*p, str, remainder);
					*p += remainder;
					*entryLength += remainder;
					remainder = 0;
				} else {
					memcpy(*p, str, bufLeft);
					*entryLength += bufLeft;
					*p += bufLeft;
					remainder -= bufLeft;
					str += bufLeft;
				}
			} else {
				(*trcBuf)->lostCount += 1; /* indicate to caller that we lost trace data */
				remainder = 0;
			}
		}
	}
}

static char * appFormatVPrintf(UtThreadData **thr, const char *template, va_list varArgs)
{
	J9rasTLS *tls;
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_DBGOUT(2, ("<UT> appFormatVPrintf called\n"));
	tls = (J9rasTLS *)omrthread_tls_get(OS_THREAD_FROM_UT_THREAD(thr), j9rasTLSKey);
	/* Lazy allocation... */
	if (tls == NULL) {
		tls = (J9rasTLS *)j9mem_allocate_memory(sizeof(J9rasTLS), OMRMEM_CATEGORY_TRACE);
		if (tls == NULL) {
			return "Thread local storage unavailable for application trace";
		} else {
			memset(tls, '\0', sizeof(J9rasTLS));
			omrthread_tls_set(OS_THREAD_FROM_UT_THREAD(thr), j9rasTLSKey, tls);
		}
	}
	if (tls->appTrace == NULL) {
		tls->appTrace = j9mem_allocate_memory(J9RAS_APPLICATION_TRACE_BUFFER, OMRMEM_CATEGORY_TRACE);

		if (tls->appTrace == NULL) {
			return "Cannot allocate thread local storage for application trace";
		}
	}
	j9str_vprintf(tls->appTrace, J9RAS_APPLICATION_TRACE_BUFFER, template, varArgs);
	return tls->appTrace;
}

/*******************************************************************************
 * name        - appFormat
 * description - Format application tracepoint
 * parameters  - UtThreadData, comp, id and trace data.
 * returns     - formatted tracepoint
 *
 ******************************************************************************/
static char *
appFormat(UtThreadData **thr, char *moduleName, int tpId, va_list var)
{
	char  *format;
	char	*tpTemplate;
	int		id;

	id   = (tpId >> 8) & UT_TRC_ID_MASK;
	/*
	 * Find the appropriate format for this tracepoint
	 */
	tpTemplate = getFormatString(moduleName, id );

	format = appFormatVPrintf(thr, tpTemplate, var);
	return format;
}

/*******************************************************************************
 * name        - tracePrint
 * description - Print a tracepoint directly
 * parameters  - UtThreadData, tracepoint identifier and trace data.
 * returns     - void
 *
 ******************************************************************************/
static void
tracePrint(UtThreadData **thr, UtModuleInfo *modInfo, uint32_t traceId, va_list var)
{
	int             id;
	char           *format;
	uint32_t          hh, mm, ss, millis;
	char            threadSwitch = ' ';
	char							entryexit = ' ';
	char							excpt = ' ';
	static char     blanks[] = "                    "
							   "                    "
							   "                    "
							   "                    "
							   "                    ";
	char qualifiedModuleName[MAX_QUALIFIED_NAME_LENGTH+1];
	char *moduleName = NULL;
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	memset(qualifiedModuleName, '\0', sizeof(qualifiedModuleName));
	if (modInfo == NULL){
		strcpy(qualifiedModuleName, "dg");
		moduleName = "dg";
	} else 	if ( modInfo->traceVersionInfo->traceVersion >= 7 && modInfo->containerModule != NULL ) {
		j9str_printf(PORTLIB, qualifiedModuleName, MAX_QUALIFIED_NAME_LENGTH,"%s(%s)", modInfo->name, modInfo->containerModule->name);
		moduleName = modInfo->name;
	} else {
		strncpy(qualifiedModuleName, modInfo->name, MAX_QUALIFIED_NAME_LENGTH);
		moduleName = modInfo->name;
	}

	/*
	 *  Split tracepoint id into component and tracepoint within component
	 */

	id   = (traceId >> 8) & UT_TRC_ID_MASK;

	/*
	 * Find the appropriate format for this tracepoint
	 */
	format = getFormatString(moduleName, id );
	if (format == NULL) {
		return;
	}

	/*
	 *  Get the time in GMT
	 */
	getTimestamp(j9time_current_time_millis(), &hh, &mm, &ss, &millis);

	/*
	 *  Hold the traceLock to avoid a trace data corruption
	 */
	getTraceLock(thr);

	/*
	 *  Check for thread switch since last tracepoint
	 */
	if (UT_GLOBAL(lastPrint) != *thr) {
		UT_GLOBAL(lastPrint) = *thr;
		threadSwitch = '*';
	}

	if (UT_GLOBAL(indentPrint)) {
		/*
		 *  If indented print was requested
		 */
		char *indent;
		excpt = format[0];      /* Pick up exception flag, if any */
		entryexit = format[1];

		/*
		 *  If this is an exit type tracepoint, decrease the indentation,
		 *  but make sure it doesn't go negative
		 */
		if (format[1] == '<' &&
			(*thr)->indent > 0) {
			(*thr)->indent--;
		}

		/*
		 *  Set indent, limited by the size of the array of blanks above
		 */
		indent = blanks + sizeof(blanks) - 1 - (*thr)->indent;
		indent = (indent < blanks) ? blanks : indent;

		/*
		 *  If this is an entry type tracepoint, increase the indentation
		 */
		if (format[1] == '>') {
			(*thr)->indent++;
		}

		if (format[1] == ' ') {
			entryexit = '-';
		}

		/*
		 *  Indented print
		 */
		j9tty_err_printf(PORTLIB, "%02d:%02d:%02d.%03d%c" UT_POINTER_SPEC
				"%16s.%-6d %c %s %c ", hh, mm, ss, millis, threadSwitch, (*thr)->id, qualifiedModuleName,
				id, excpt, indent, entryexit);
		j9tty_err_vprintf(format + 2, var);

	} else {
		/*
		 *  Non-indented print
		 */
		excpt = format[0];
		format[1] == ' ' ? (entryexit = '-') : (entryexit = format[1]);

		j9tty_err_printf(PORTLIB, "%02d:%02d:%02d.%03d%c" UT_POINTER_SPEC
				   "%16s.%-6d %c %c ", hh, mm, ss, millis, threadSwitch, (*thr)->id, qualifiedModuleName,
				   id, excpt, entryexit);
		j9tty_err_vprintf(format + 2, var);

	}
	j9tty_err_printf(PORTLIB, "\n");
	freeTraceLock(thr);
}

/*******************************************************************************
 * name        - traceAssertion
 * description - Print a tracepoint directly
 * parameters  - UtThreadData, tracepoint identifier and trace data.
 * returns     - void
 *
 ******************************************************************************/
static void
traceAssertion(UtThreadData **thr, UtModuleInfo *modInfo, uint32_t traceId, va_list var)
{
	int             id;
	char           *format;
	uint32_t   hh, mm, ss, millis;
	char qualifiedModuleName[MAX_QUALIFIED_NAME_LENGTH+1];
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	memset(qualifiedModuleName, '\0', sizeof(qualifiedModuleName));
	if (modInfo == NULL){
		strcpy(qualifiedModuleName, "dg");
	} else 	if ( modInfo->traceVersionInfo->traceVersion >= 7 && modInfo->containerModule != NULL ) {
		j9str_printf(PORTLIB, qualifiedModuleName, MAX_QUALIFIED_NAME_LENGTH,"%s(%s)", modInfo->name, modInfo->containerModule->name);
	} else {
		strncpy(qualifiedModuleName, modInfo->name, MAX_QUALIFIED_NAME_LENGTH);
	}

	getTimestamp(j9time_current_time_millis(), &hh, &mm, &ss, &millis);

	/*
	 *  Split tracepoint id into component and tracepoint within component
	 */

	id   = (traceId >> 8) & UT_TRC_ID_MASK;

	format = "* ** ASSERTION FAILED ** at %s:%d: %s";

	/*
	 *  Hold the traceLock to avoid a trace data corruption
	 */
	getTraceLock(thr);

	/*
	 *  Non-indented print
	 */
	j9tty_err_printf(PORTLIB, "%02d:%02d:%02d.%03d " UT_POINTER_SPEC
			   "%8s.%-6d *   ", hh, mm, ss, millis, (*thr)->id, qualifiedModuleName,
			   id);
	j9tty_err_vprintf(format + 2, var);

	j9tty_err_printf(PORTLIB, "\n");

	freeTraceLock(thr);
}

/*******************************************************************************
 * name        - traceCount
 * description - Increment the count for a tracepoint
 * parameters  - UtThreadData, tracepoint identifier
 * returns     - void
 ******************************************************************************/
static void
traceCount(UtModuleInfo *moduleInfo, uint32_t traceId)
{
	int traceNum = (traceId >> 8) & UT_TRC_ID_MASK;
	incrementTraceCounter(moduleInfo, UT_GLOBAL(componentList), traceNum);
}

/*******************************************************************************
 * name        - utTraceV
 * description - Make a tracepoint
 * parameters  - UtThreadData, tracepoint identifier and trace data.
 * returns     - void
 *
 ******************************************************************************/
static void
traceV(UtThreadData **thr, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, va_list var,
		int bufferType)
{
	UtTraceBuffer     *trcBuf;
	int                lastSequence;
	int                entryLength;
	int                length;
	int                i;
	int                lostCount;
	char              *p;
	const signed char *str;
	char              *format = NULL;
	uint32_t             passedId = traceId;
	int32_t             intVar;
	char               charVar;
	unsigned short     shortVar;
	int64_t             i64Var;
	double             doubleVar;
	char              *ptrVar;
	char              *stringVar;
	size_t             stringVarLen;
	char              *containerModuleVar = NULL;
	size_t             containerModuleVarLen = 0;
	char               temp[3];
	static char        lengthConversion[] = {0,
										 sizeof(char),
										 sizeof(short),
										 0,
										 sizeof(int32_t),
										 sizeof(float),
										 sizeof(char *),
										 sizeof(double),
										 sizeof(int64_t),
										 sizeof(long double),
										 0};
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	/* modInfo == NULL means this is a tracepoint for the trace formatter */
	if (modInfo != NULL){
		traceId += (257 << 8); /* this will be translated back by the formatter */
		stringVar = modInfo->name;
		stringVarLen = length = modInfo->namelength;
		if (modInfo->traceVersionInfo->traceVersion >= 7 && modInfo->containerModule != NULL){
				containerModuleVar = modInfo->containerModule->name;
				containerModuleVarLen = modInfo->containerModule->namelength;
				length += (int)containerModuleVarLen + sizeof( "()" ) -1;
		}
	} else {
		stringVar = "INTERNALTRACECOMPONENT";
		stringVarLen = length = sizeof("INTERNALTRACECOMPONENT") - 1;
	}

	/*
	 *  Trace buffer available ?
	 */
	if (bufferType == UT_NORMAL_BUFFER) {
		if ((trcBuf = (*thr)->trcBuf) == NULL
			  &&  (trcBuf = getTrcBuf(thr, NULL, bufferType)) == NULL) {
			return;
		}
	} else if (bufferType == UT_EXCEPTION_BUFFER) {
		if ((trcBuf = UT_GLOBAL(exceptionTrcBuf)) == NULL
			  &&  (trcBuf = getTrcBuf(thr, NULL, bufferType)) == NULL) {
			return;
		}
	} else {
		return;
	}

	lostCount = trcBuf->lostCount;

	if (trcBuf->flags & UT_TRC_BUFFER_NEW) {
		UT_ATOMIC_AND((volatile uint32_t *)(&trcBuf->flags), ~UT_TRC_BUFFER_NEW);
		trcBuf->thr = thr;
	}
	lastSequence = (int32_t)(trcBuf->record.sequence >> 32);
	trcBuf->record.sequence = j9time_hires_clock();
	p = (char *)&trcBuf->record + trcBuf->record.nextEntry + 1;

	/* additional sanity check */
	if (p < (char*)&trcBuf->record || p > ((char*)&trcBuf->record + UT_GLOBAL(bufferSize))) {
		/* the buffer's been mangled so free it for reuse and acquire another */
		UT_DBGOUT(1, ("<UT> invalid nextEntry value in record. Freed trace buffer for thread "UT_POINTER_SPEC" and reinitialized\n", thr));

		freeBuffers(&trcBuf->queueData);
		(*thr)->trcBuf = NULL;
		trcBuf = getTrcBuf(thr, NULL, bufferType);
		if (trcBuf == NULL) {
			return;
		}

		p = (char *)&trcBuf->record + trcBuf->record.nextEntry + 1;
	}

	if ((UT_GLOBAL(bufferSize) - trcBuf->record.nextEntry) >
					(UT_FASTPATH + length + 4) ) {
		/* there is space in the buffer for a minimal entry, so avoid the overhead of
				copyToBuffer func */
		if (lastSequence != (int32_t)(trcBuf->record.sequence>>32)) {
			*p       = UT_TRC_SEQ_WRAP_ID[0];
			*(p + 1) = UT_TRC_SEQ_WRAP_ID[1];
			*(p + 2) = UT_TRC_SEQ_WRAP_ID[2];
			memcpy(p + 3, &lastSequence, sizeof(int32_t));
			*(p + 7) = UT_TRC_SEQ_WRAP_LENGTH;
			trcBuf->record.nextEntry += UT_TRC_SEQ_WRAP_LENGTH;
			p += UT_TRC_SEQ_WRAP_LENGTH;
		}
		*p = (char)(traceId>>24);
		*(p + 1) = (char)(traceId>>16);
		*(p + 2) = (char)(traceId>>8);
		{
			int32_t temp = (int32_t)(trcBuf->record.sequence & 0xFFFFFFFF);
			memcpy(p + 3, &temp , sizeof(int32_t));
		}
		p += 7;

		/* write name length into buffer */
		memcpy(p, &length, 4);

		/* write name into buffer */
		memcpy(p + 4, stringVar, stringVarLen);

		p += (stringVarLen + 4);

		/* if tracepoint is part of other component - write container's name into buffer */
		if (containerModuleVar != NULL){
			*p++ = '(';
			memcpy(p, containerModuleVar, containerModuleVarLen);
			p += containerModuleVarLen;
			*p++ = ')';
		}

		entryLength = length + 12;
		*p = 12 + length;

	} else {
		/* approaching end of buffer, use copyToBuffer to handle the wrap */
		/*
		 *  Handle sequence counter wrap
		 *
		 *  It's not a problem if the sequence counter write is aborted/discarded
		 *  by nodynamic because it applies to the *preceding* tracepoints and
		 *  they'll have been discarded as well.
		 */
		if (lastSequence != (int32_t)(trcBuf->record.sequence>>32)) {
			char len = UT_TRC_SEQ_WRAP_LENGTH;
			entryLength = 0;
			copyToBuffer(thr, bufferType, UT_TRC_SEQ_WRAP_ID, &p, 3,
						 &entryLength, &trcBuf);
			copyToBuffer(thr, bufferType, (char *)&lastSequence, &p, 4,
						 &entryLength, &trcBuf);
			copyToBuffer(thr, bufferType, &len, &p, 1,
						 &entryLength, &trcBuf);
			if (trcBuf->lostCount != lostCount && trcBuf->lostCount != 0) {
				return;
			}
			/* copyToBuffer increments p past the last byte written, but nextEntry
			 * needs to point to the length byte so we need -1 here.
			 */
			trcBuf->record.nextEntry = (int32_t)(p - (char*)&trcBuf->record - 1);
		}
		entryLength = 1;
		temp[0] = (char)(traceId>>24);
		temp[1] = (char)(traceId>>16);
		temp[2] = (char)(traceId>>8);
		copyToBuffer(thr, bufferType, temp, &p, 3, &entryLength, &trcBuf);
		intVar = (int32_t)trcBuf->record.sequence;
		copyToBuffer(thr, bufferType, (char *)&intVar,
					 &p, 4, &entryLength, &trcBuf);

		/* write name length to buffer */
		copyToBuffer(thr, bufferType, (char *)&length,
					  &p, 4, &entryLength, &trcBuf);

		/* write name to buffer */
		copyToBuffer(thr, bufferType, stringVar, &p,
				 	 (int)stringVarLen, &entryLength, &trcBuf);

		if (containerModuleVar != NULL){
			copyToBuffer(thr, bufferType, "(", &p, 1, &entryLength, &trcBuf);
			copyToBuffer(thr, bufferType, containerModuleVar, &p, (int)containerModuleVarLen, &entryLength, &trcBuf);
			copyToBuffer(thr, bufferType, ")", &p, 1, &entryLength, &trcBuf);
		}

		charVar = (char)entryLength;
		copyToBuffer(thr, bufferType, &charVar, &p, 1, &entryLength,
					 &trcBuf);

		if (trcBuf->lostCount != lostCount && trcBuf->lostCount != 0) {
			return;
		}
		p--;
		entryLength--;
	}

	/*
	 * Process maximal trace
	 */
	if (((*thr)->currentOutputMask & (UT_MAXIMAL|UT_EXCEPTION)) != 0 && (str = (const signed char *)spec) != NULL) {

		if(!(modInfo == NULL)){
			if(UT_APPLICATION_TRACE_MODULE == modInfo->moduleId){
				format = appFormat(thr, modInfo->name, passedId, var);
				str = (const signed char *)"\xff";
			}
		}

		/*
		 * Calculate the length
		 */
		length = 0;
		for (i = 0; str[i] > UT_TRACE_DATA_TYPE_END; i++) {
			length += lengthConversion[str[i]];
		}
		if (str[i] != UT_TRACE_DATA_TYPE_END) {
			length = -1;
		}
		/*
		 *  If we know the length and it will fit without a wrap
		 */
		if (length > 0  &&
			p + length + 1 <
			   (char *)&trcBuf->record + UT_GLOBAL(bufferSize)) {
			while (*str != '\0') {
				switch (*str) {
					/*
					 *  Words
					 */
				case UT_TRACE_DATA_TYPE_INT32:
					{
						intVar = va_arg(var, int32_t);
						memcpy(p, &intVar, sizeof(int32_t));
						p += sizeof(int32_t);
						entryLength += sizeof(int32_t);
						break;
					}
					/*
					 *  Pointers
					 */
				case UT_TRACE_DATA_TYPE_POINTER:
					{
						ptrVar = va_arg(var, char *);
						memcpy(p, &ptrVar, sizeof(char *));
						p += sizeof(char *);
						entryLength += sizeof(char *);
						break;
					}
					/*
					 *  64 bit integers
					 */
				case UT_TRACE_DATA_TYPE_INT64:
					{
						i64Var = va_arg(var, int64_t);
						memcpy(p, &i64Var, sizeof(int64_t));
						p += sizeof(int64_t);
						entryLength += sizeof(int64_t);
						break;
					}
					/*
					 *  Doubles
					 */
		        	 /* CMVC 164940 All %f tracepoints are internally promoted to double.
		        	  * Affects:
		        	  *  TraceFormat  com/ibm/jvm/format/Message.java
		        	  *  TraceFormat  com/ibm/jvm/trace/format/api/Message.java
		        	  *  runtime    ute/ut_trace.c
		        	  *  TraceGen     OldTrace.java
		        	  * Intentional fall through to next case. 
		        	  */

				case UT_TRACE_DATA_TYPE_DOUBLE:
					{
						doubleVar = va_arg(var, double);
						memcpy(p, &doubleVar, sizeof(double));
						p += sizeof(double);
						entryLength += sizeof(double);
						break;
					}
					/*
					 *  Bytes
					 */
				case UT_TRACE_DATA_TYPE_CHAR:
					{
						charVar = (char)va_arg(var, int32_t);
						*p = charVar;
						p += sizeof(char);
						entryLength += sizeof(char);
						break;
					}
					/*
					 *  Shorts
					 */
				case UT_TRACE_DATA_TYPE_SHORT:
					{
						shortVar = (short)va_arg(var, int32_t);
						memcpy(p, &shortVar, sizeof(short));
						p += sizeof(short);
						entryLength += sizeof(short);
						break;
					}
				}
				str++;
			}
			*p = (unsigned char)entryLength;
			/*
			 *  There's a chance of a wrap, so take it slow..
			 */
		} else {
			int32_t left;
			while (*str != '\0' &&
				   (trcBuf->lostCount == lostCount || trcBuf->lostCount == 0)) {
				left = (int32_t)((char *)&trcBuf->record + UT_GLOBAL(bufferSize) - p);

				 switch (*str) {
						/*
						 *  Words
						 */
					case UT_TRACE_DATA_TYPE_INT32:
						{
							intVar = va_arg(var, int32_t);
							if (left > sizeof(int32_t)) {
								memcpy(p, &intVar, sizeof(int32_t));
								p += sizeof(int32_t);
								entryLength += sizeof(int32_t);
							} else {
								copyToBuffer(thr, bufferType, (char *)&intVar,
											 &p, sizeof(int32_t), &entryLength,
											 &trcBuf);
							}
							break;
						}
						/*
						 *  Pointers
						 */
					case UT_TRACE_DATA_TYPE_POINTER:
						{
							ptrVar = va_arg(var, char *);
							if (left > sizeof(char *)) {
								memcpy(p, &ptrVar, sizeof(char *));
								p += sizeof(char *);
								entryLength += sizeof(char *);
							} else {
								copyToBuffer(thr, bufferType, (char *)&ptrVar,
											 &p, sizeof(char *), &entryLength,
											 &trcBuf);
							}
							break;
						}
						/*
						 *  Strings
						 */
					case UT_TRACE_DATA_TYPE_STRING:
						{
							if((modInfo != NULL) && (UT_APPLICATION_TRACE_MODULE == modInfo->moduleId)){
								stringVar = format;
							} else {
								stringVar = va_arg(var, char *);
							}
							if (stringVar == NULL) {
								stringVar = UT_NULL_POINTER;
							}
							length = (int32_t)strlen(stringVar) + 1;
							if (left > length) {
								memcpy(p, stringVar, length);
								p += length;
								entryLength += length;
							} else {
								copyToBuffer(thr, bufferType, stringVar, &p,
											 length, &entryLength, &trcBuf);
							}
							break;
						}
						/*
						 *  UTF8 strings
						 */
					case UT_TRACE_DATA_TYPE_PRECISION:
						{
							length = va_arg(var, int32_t);
							str++;
							if (*str == UT_TRACE_DATA_TYPE_STRING) {
								stringVar = va_arg(var, char *);
								if (stringVar == NULL) {
									length = 0;
								}
								shortVar = (unsigned short)length;
								if (left > sizeof(short)) {
									memcpy(p, &shortVar, sizeof(short));
									p += sizeof(short);
									entryLength += sizeof(short);
									left -= sizeof(short);
								} else {
									copyToBuffer(thr, bufferType, (char *)&shortVar,
												 &p, sizeof(short), &entryLength,
												 &trcBuf);
									left = (int32_t)((char *)&trcBuf->record +
									   UT_GLOBAL(bufferSize) - p);
								}
								if (length > 0) {
									if (left > length) {
										memcpy(p, stringVar, length);
										p += length;
										entryLength += length;
									} else {
										copyToBuffer(thr, bufferType, stringVar, &p,
													 length, &entryLength, &trcBuf);
									}
								}
							}
							break;
						}
						/*
						 *  64 bit integers
						 */
					case UT_TRACE_DATA_TYPE_INT64:
						{
							i64Var = va_arg(var, int64_t);
							if (left > sizeof(int64_t)) {
								memcpy(p, &i64Var,sizeof(int64_t));
								p += sizeof(int64_t);
								entryLength += sizeof(int64_t);
							} else {
								copyToBuffer(thr, bufferType, (char *)&i64Var,
											 &p, sizeof(int64_t), &entryLength,
											 &trcBuf);
							}
							break;
						}
						/*
						 *  Bytes
						 */
					case UT_TRACE_DATA_TYPE_CHAR:
						{
							charVar = (char)va_arg(var, int32_t);
							if (left > sizeof(char)) {
								*p = charVar;
								p += sizeof(char);
								entryLength += sizeof(char);
							} else {
								copyToBuffer(thr, bufferType, &charVar, &p,
										   sizeof(char), &entryLength, &trcBuf);
						}
							break;
						}
						/*
						 *  Doubles
						 */
					case UT_TRACE_DATA_TYPE_DOUBLE:
						{
							doubleVar = va_arg(var, double);
							if (left > sizeof(double)) {
								memcpy(p, &doubleVar,sizeof(double));
								p += sizeof(double);
								entryLength += sizeof(double);
							} else {
								copyToBuffer(thr, bufferType, (char *)&doubleVar,
											 &p, sizeof(double), &entryLength,
											 &trcBuf);
							}
							break;
						}
						/*
						 *  Shorts
						 */
					case UT_TRACE_DATA_TYPE_SHORT:
						{
							shortVar = (short)va_arg(var, int32_t);
							if (left > sizeof(short)) {
								memcpy(p, &shortVar, sizeof(short));
								p += sizeof(short);
								entryLength += sizeof(short);
							} else {
								copyToBuffer(thr, bufferType, (char *)&shortVar,
											 &p, sizeof(short), &entryLength,
											 &trcBuf);
							}
							break;
						}
					}
					str++;
				}
				if (trcBuf->lostCount != lostCount && trcBuf->lostCount != 0) {
					return;
				}
				/*
				 *  Set the entry length
				 */
				if ((char *)&trcBuf->record + UT_GLOBAL(bufferSize) - p >
					sizeof(char)) {
					*p = (unsigned char)entryLength;
				} else {
					charVar = (unsigned char)entryLength;
					copyToBuffer(thr, bufferType, &charVar, &p, sizeof(char),
								 &entryLength, &trcBuf);
					if (trcBuf->lostCount != lostCount && trcBuf->lostCount != 0) {
						return;
					}
					entryLength--;
					p--;
				}
			}
		}

		/*
		 *  Most tracepoints should now be complete, so we might bail out now.
		 *  We don't need a -1 in the nextEntry assignment as we do elsewhere when
		 *  copyToBuffer's been involved because p is decremented above.
		 */
		if (entryLength <= UT_MAX_TRC_LENGTH) {
			trcBuf->record.nextEntry =
						 (int32_t)(p - (char*)&trcBuf->record);
			return;
		} else {

		/*
		 *  Handle long trace records
		 */

			char temp[4];
			p++;
			temp[0] = 0;
			temp[1] = 0;
			temp[2] = (char)(entryLength>>8);
			temp[3] = UT_TRC_EXTENDED_LENGTH;
			copyToBuffer(thr, bufferType, temp, &p, 4,
						 &entryLength, &trcBuf);
			if (trcBuf->lostCount == lostCount || trcBuf->lostCount == 0) {
				/* copyToBuffer increments p past the last byte written, but nextEntry
				 * needs to point to the length byte so we need -1 here.
				 */
				trcBuf->record.nextEntry =
					(int32_t)(p - (char *)&trcBuf->record - 1);
		}
	}
}

/*******************************************************************************
 * name        - trace
 * description - Call traceV passing trace data as a va_list
 * parameters  - UtThreadData, tracepoint identifier and trace data.
 * returns     - void
 *
 ******************************************************************************/
static void
trace(UtThreadData **thr, UtModuleInfo *modinfo, uint32_t traceId, int bufferType, const char *spec, ...)
{
	va_list      var;

	va_start(var, spec);
	traceV(thr, modinfo, traceId, spec, var, bufferType);
	va_end(var);

}

/**
 * Called by utsTraceV to write the tracepoint to the appropriate place (buffer, screen, externals etc.)
 *
 */
static void logTracePoint(UtThreadData **thr, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, va_list varArgs)
{
	va_list var;

	if (((*thr)->currentOutputMask & (UT_MINIMAL | UT_MAXIMAL)) != 0) {
		COPY_VA_LIST(var, varArgs);
		traceV(thr, modInfo, traceId, spec, var, UT_NORMAL_BUFFER);
	}

	if (((*thr)->currentOutputMask & UT_COUNT) != 0) {
		traceCount(modInfo, traceId);
	}

	if ((traceId & UT_SPECIAL_ASSERTION) != 0) {
		COPY_VA_LIST(var, varArgs);
		traceAssertion(thr, modInfo, traceId, var);
	} else if (((*thr)->currentOutputMask & UT_PRINT) != 0) {
		COPY_VA_LIST(var, varArgs);
		tracePrint(thr, modInfo, traceId, var);
	}

	if (((*thr)->currentOutputMask & UT_PLATFORM) != 0) {
		/* Unimplemented. */
	}

	if (((*thr)->currentOutputMask & UT_EXTERNAL) != 0 && modInfo != NULL) {
		UtTraceListener *listener;
		UtSubscription *subscription;

		/* Find and call all register 'external' subscribers (legacy JVMRI interface) */
		for (listener = UT_GLOBAL(traceListeners); listener != NULL; listener = listener->next) {
			if (listener->listener != NULL) {
				COPY_VA_LIST(var, varArgs);
				traceExternal(thr, listener->listener, listener->userData, modInfo->name, traceId >> 8, spec, var);
			}
		}
		
		/* Find and call all registered tracepoint subscribers (public JVMTI interface) */
		getTraceLock(thr);
		for (subscription = UT_GLOBAL(tracePointSubscribers); subscription != NULL; subscription = subscription->next) {
			if (subscription->subscriber != NULL) {
				COPY_VA_LIST(var, varArgs);
				callSubscriber(thr, subscription, modInfo, traceId, var);
			}
		}
		freeTraceLock(thr);
	}

	/* Write tracepoint to the global exception buffer. (Usually GC History) */
	if (((*thr)->currentOutputMask & UT_EXCEPTION) != 0) {
		COPY_VA_LIST(var, varArgs);
		getTraceLock(thr);
		if (*thr != UT_GLOBAL(exceptionContext)) {
			/* This tracepoint is going to the global exception buffer not to a per
			 * thread buffer so record if we aren't on the same thread as the last
			 * time we wrote a tracepoint to this buffer by writing an extra
			 * tracepoint, dg.259 and update the exceptionContext in UtGlobal
			 * for the next time we check this.
			 */
			UT_GLOBAL(exceptionContext) = *thr;
			/* Write Trc_TraceContext_Event1, dg.259 */
			trace(thr, NULL, (UT_TRC_CONTEXT_ID << 8) | UT_MAXIMAL, UT_EXCEPTION_BUFFER, pointerSpec, thr );
		}
		traceV(thr, modInfo, traceId, spec, var, UT_EXCEPTION_BUFFER);
		freeTraceLock(thr);
	}

	if ((traceId & UT_SPECIAL_ASSERTION) != 0) {
		raiseAssertion(thr, modInfo, traceId);
	}
}

void omrTrace(void *env, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, ...)
{
	va_list var;

	va_start(var, spec);
	doTracePoint(UT_THREAD_FROM_ENV(env), modInfo, traceId, spec, var);
	va_end(var);
}

/*******************************************************************************
 * name        - doTracePoint
 * description - Make a tracepoint, not called directly outside of rastrace
 * parameters  - UtThreadData, tracepoint identifier and trace data.
 * returns     - void
 *
 ******************************************************************************/
void
doTracePoint(UtThreadData **thr, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, va_list varArgs)
{
	unsigned char savedOutputMask = '\0';
	if (utGlobal == NULL || UT_GLOBAL(traceFinalized)) {
		return;
	}

	if ((thr == NULL) ||
		(*thr == NULL)) {
		return;
	}

	/* Global trace "off" switch, turns trace off when it is unsafe to use.
	 * Mainly used by rasdump to make thread introspection safe, taking tracepoints
	 * inside the signal handlers used for introspection can have unfortunate
	 * consequences. See defect 178808.
	 */
	if ( UT_GLOBAL(traceDisable) != 0 ) {
		return;
	}

	/* Recursion protection. Only applies to regular (not auxiliary) tracepoints
	 * modInfo==NULL is for internal ute tracepoints
	 */
	if (NULL == modInfo || !MODULE_IS_AUXILIARY(modInfo)) {
		/* This block only executed for regular tracepoints (including NULL modInfo)*/
		if ((*thr)->recursion) {
			return;
		}
		incrementRecursionCounter(*thr);

		/* Auxiliary tracepoints go where the regular tracepoint that created them went. Regular tracepoints
		 * need to record where they are about to go in case they generate auxiliary tracepoints.
		 */
		(*thr)->currentOutputMask = (unsigned char)(traceId & 0xFF);

		if ( (traceId & (UT_TRIGGER)) != 0 ) {
			fireTriggerHit(thr, (NULL != modInfo)?modInfo->name:"dg", (traceId >> 8) & UT_TRC_ID_MASK, BEFORE_TRACEPOINT);
		}
	} else {
		/* This block only executed for auxiliary tracepoints*/

		/* The primary user of auxiliary tracepoints is jstacktrace. Sending jstacktrace data to 
		 * minimal (i.e. throwing away all the stack data) makes no sense - so it is converted to 
		 * maximal. currentOutputMask is reset below.
		 */
		savedOutputMask = (*thr)->currentOutputMask;
		if ((*thr)->currentOutputMask & UT_MINIMAL) {
			(*thr)->currentOutputMask = (savedOutputMask & ~UT_MINIMAL) | UT_MAXIMAL;
		}
	}

	if (UT_GLOBAL(traceSuspend) == 0 &&
		(*thr)->suspendResume >= 0) {
		/*logTracePoint writes the trace point to the appropriate location*/
		logTracePoint(thr,modInfo,traceId,spec,varArgs);
	}

	if (NULL == modInfo || !MODULE_IS_AUXILIARY(modInfo)) {
		/* This block only executed for regular tracepoints*/
		/* Auxiliary trace points can't trigger events */
		if ( (traceId & (UT_TRIGGER)) != 0 ) {
			fireTriggerHit(thr, (NULL != modInfo)?modInfo->name:"dg", (traceId >> 8) & UT_TRC_ID_MASK, AFTER_TRACEPOINT);
		}

		decrementRecursionCounter(*thr);
	} else {
		/* This block only executed for auxiliary tracepoints*/
		(*thr)->currentOutputMask = savedOutputMask;
	}
}

/*******************************************************************************
 * name        - utsTraceV
 * description - Make a tracepoint
 * parameters  - UtThreadData, tracepoint identifier and trace data.
 * returns     - void
 *
 ******************************************************************************/
void
trcTraceVNoThread(UtModuleInfo *modInfo, uint32_t traceId, const char *spec, va_list varArgs)
{
	/*
	 * When we remove the server interface this will likely be the only visible utsTrace* function
	 * on the server interface.
	 */
	doTracePoint(twThreadSelf(), modInfo, traceId, spec, varArgs);
}

/*******************************************************************************
 * name        - internalTrace
 * description - Make an tracepoint, not called outside rastrace
 * parameters  - UtThreadData, tracepoint identifier and trace data.
 * returns     - void
 ******************************************************************************/
void
internalTrace(UtThreadData **thr, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, ...)
{
	va_list      var;

	va_start(var, spec);
	doTracePoint(thr, modInfo, traceId, spec, var);
	va_end(var);
}

/*******************************************************************************
 * name        - setTraceHeaderInfo
 * description - Adds the supplied information to the trace file header, or
 * 						overwrites the current values with the supplied values.
 * parameters  - UtThreadData **thr, service information, startup information
 * returns     - OMR_ERROR_NONE if all went well, OMR_ERROR_OUT_OF_NATIVE_MEMORY if alloc failure.
 ******************************************************************************/
omr_error_t
setTraceHeaderInfo(const char * serviceInfo, const char * startupInfo)
{
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_DBGOUT(1, ("<UT> Update trace header information\n"));

	/* if this is a re-initialisation, free the previous values */
	if (UT_GLOBAL(properties) != NULL){
		j9mem_free_memory( UT_GLOBAL(properties));
	}
	if (UT_GLOBAL(serviceInfo) != NULL){
		j9mem_free_memory( UT_GLOBAL(serviceInfo));
	}

	UT_GLOBAL(properties)  = (char *) j9mem_allocate_memory( strlen(startupInfo) + 1, OMRMEM_CATEGORY_TRACE );
	UT_GLOBAL(serviceInfo) = (char *) j9mem_allocate_memory( strlen(serviceInfo) + 1, OMRMEM_CATEGORY_TRACE );

	if (UT_GLOBAL(properties) != NULL){
		strcpy(UT_GLOBAL(properties), startupInfo);
	} else {
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	if (UT_GLOBAL(serviceInfo) != NULL){
		strcpy(UT_GLOBAL(serviceInfo), serviceInfo);
	} else {
		j9mem_free_memory( UT_GLOBAL(properties) );
		UT_GLOBAL(properties) = NULL;
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	return OMR_ERROR_NONE;
}

/*******************************************************************************
 * name        - cleanupTraceWorkerThread
 * description - Utility function to clean up after the trace worker thread
 * parameters  - void
 * returns     - void
 ******************************************************************************/
static void
cleanupTraceWorkerThread(UtSubscription *subscription)
{
	/* find the subscriber we're cleaning up after */
	TraceWorkerData *data = (TraceWorkerData*)subscription->userData;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_GLOBAL(traceWriteStarted) = FALSE;
	UT_GLOBAL(traceInitialized) = FALSE;

	if (data->trcFile != -1) {
		closeTraceFile(data->trcFile, UT_GLOBAL(traceFilename),
				data->maxTrc);
	}

	if (data->exceptFile != -1) {
		closeTraceFile(data->exceptFile, UT_GLOBAL(exceptFilename),
				data->maxExcept);
	}

	j9mem_free_memory( subscription->userData );
}

/*******************************************************************************
 * name        - setupTraceWorkerThread
 * description - Start a worker thread to write data to disk
 * parameters  - UtThreadData
 * returns     - OMR_ERROR_NONE if success
 ******************************************************************************/
omr_error_t
setupTraceWorkerThread(UtThreadData **thr)
{
	omr_error_t result = OMR_ERROR_NONE;
	TraceWorkerData *data;
	UtSubscription *subscription;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	if (OMR_ERROR_NONE != initTraceHeader()) {
		return OMR_ERROR_INTERNAL;
	}

	data = (TraceWorkerData*)j9mem_allocate_memory( sizeof(TraceWorkerData), OMRMEM_CATEGORY_TRACE );

	if (data == NULL) {
		UT_DBGOUT(1, ("<UT> Out of memory registering trace write subscriber\n"));
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	/* set up the output files. An alternative is that we open/close for each
	 * buffer, meaning files can be cleared/moved/deleted while trace is running
	 * however we'll stick with existing behaviour for the moment
	 */
	data->trcFile = -1;
	data->trcSize = 0;
	data->maxTrc = 0;
	if (UT_GLOBAL(externalTrace)) {
		setTraceType(UT_NORMAL_BUFFER);
		data->trcFile = openTraceFile(NULL);
		if (data->trcFile != -1) {
			data->trcSize = UT_GLOBAL(traceHeader->header.length);
			data->maxTrc = data->trcSize;
		}
	}

	data->exceptFile = -1;
	data->exceptSize = 0;
	data->maxExcept = 0;
	if (UT_GLOBAL(extExceptTrace)) {
		setTraceType(UT_EXCEPTION_BUFFER);
		data->exceptFile = openTraceFile(UT_GLOBAL(exceptFilename));
		if (data->exceptFile != -1) {
			data->exceptSize = UT_GLOBAL(traceHeader->header.length);
			data->maxExcept = data->exceptSize;
		}
	}

	UT_DBGOUT(1, ("<UT> Registering trace write subscriber\n"));
	result = trcRegisterRecordSubscriber(thr, "Trace Engine Thread", writeBuffer, cleanupTraceWorkerThread, data, NULL, NULL, &subscription, TRUE);

	if (OMR_ERROR_NONE != result) {
		j9mem_free_memory( data);
		/* Error registering trace write subscriber */
		j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_REGISTER_SUBSCRIBER_FAILED);
	} else {
		subscription->threadPriority = UT_TRACE_WRITE_PRIORITY;
		UT_GLOBAL(traceWriteStarted) = TRUE;
	}

	return result;
}


/*******************************************************************************
 * name        - cleanupSnapDumpThread
 * description - Utility function to clean up after the snap dump thread
 * parameters  - UtSubscription
 * returns     - void
 ******************************************************************************/
void
cleanupSnapDumpThread(UtSubscription *subscription)
{
	uint32_t         oldFlags;
	uint32_t         newFlags;
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	j9file_close(UT_GLOBAL(snapFile));

	/* Reset the 'snap in progress' flag to show we are done */
	do {
		oldFlags = UT_GLOBAL(traceSnap);
		newFlags = oldFlags & ~TRUE;
	} while (!twCompareAndSwap32(&UT_GLOBAL(traceSnap), oldFlags, newFlags));

}

/*******************************************************************************
 * name        - writeSnapBuffer
 * description - Worker thread to write snap files
 * parameters  - UtSubscription *
 * returns     - OMR_ERROR_NONE on success, otherwise error
 ******************************************************************************/
omr_error_t
writeSnapBuffer(UtSubscription *subscription)
{
	omr_error_t result = OMR_ERROR_NONE;
	UtThreadData  **thr;
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	thr = subscription->thr;

	UT_DBGOUT(5, ("<UT thr=" UT_POINTER_SPEC "> write buffer " UT_POINTER_SPEC " to snap dump file\n", thr, subscription->data));

	/*
	 *  Write the record
	 */
	if ((uint32_t)j9file_write( UT_GLOBAL(snapFile), subscription->data, (intptr_t)subscription->dataLength) != subscription->dataLength) {
		/* Error writing to snap file */
		j9nls_printf(PORTLIB, J9NLS_WARNING | J9NLS_STDERR, J9NLS_TRC_SNAP_WRITE_FAIL);
		result = OMR_ERROR_INTERNAL;
	}

	return result;
}

static UtProcessorInfo * getProcessorInfo(void)
{
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));
	UtProcessorInfo *ret = NULL;
	const char *osarch;

	UT_DBGOUT(1, ("<UT> GetProcessorInfo called\n"));

	ret = (UtProcessorInfo *) j9mem_allocate_memory(sizeof(UtProcessorInfo), OMRMEM_CATEGORY_TRACE);
	if (ret == NULL) {
		return NULL;
	}
	memset(ret, '\0', sizeof(UtProcessorInfo));
	initHeader(&ret->header, "PINF", sizeof(UtProcessorInfo));
	initHeader(&ret->procInfo.header, "PIN", sizeof(UtProcInfo));
	osarch = j9sysinfo_get_CPU_architecture();
	if (NULL != osarch) {
		if ((strcmp(osarch, J9PORT_ARCH_PPC) == 0)
		|| (strcmp(osarch, J9PORT_ARCH_PPC64) == 0)
		|| (strcmp(osarch, J9PORT_ARCH_PPC64LE) == 0)
		) {
			ret->architecture = UT_POWER;
			ret->procInfo.subtype = UT_POWERPC;
		} else if (strcmp(osarch, J9PORT_ARCH_S390) == 0) {
			ret->architecture = UT_S390;
			ret->procInfo.subtype = UT_ESA;
		} else if (strcmp(osarch, J9PORT_ARCH_S390X) == 0) {
			ret->architecture = UT_S390X;
			ret->procInfo.subtype = UT_TREX;
		} else if (strcmp(osarch, J9PORT_ARCH_HAMMER) == 0) {
			ret->architecture = UT_AMD64;
			ret->procInfo.subtype = UT_OPTERON;
		} else if (strcmp(osarch, J9PORT_ARCH_X86) == 0) {
			ret->architecture = UT_X86;
			ret->procInfo.subtype = UT_PIV;
		} else {
			ret->architecture = UT_UNKNOWN;
		}
	} else {
		ret->architecture = UT_UNKNOWN;
	}
#ifdef J9VM_ENV_LITTLE_ENDIAN
	ret->isBigEndian =  FALSE;
#else
	ret->isBigEndian =  TRUE;
#endif
	ret->procInfo.traceCounter = UT_J9_TIMER;
	ret->wordsize = sizeof(uintptr_t) * 8;
	ret->onlineProcessors = (uint32_t)j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);

	return ret;
}

static void
traceExternal(UtThreadData **thr, UtListenerWrapper func, void *userData,
	const char *modName, uint32_t traceId, const char *spec, va_list varArgs)
{
	J9rasTLS *tls;
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_DBGOUT(2, ("<UT> traceExternal called\n"));
	tls = (J9rasTLS *)omrthread_tls_get(OS_THREAD_FROM_UT_THREAD(thr), j9rasTLSKey);
	/* Lazy allocation... */
	if (tls == NULL) {
		tls = (J9rasTLS *)j9mem_allocate_memory(sizeof(J9rasTLS), OMRMEM_CATEGORY_TRACE);
		if (tls != NULL) {
			memset(tls, '\0', sizeof(J9rasTLS));
			omrthread_tls_set(OS_THREAD_FROM_UT_THREAD(thr), j9rasTLSKey, tls);
		}
	}
	if (tls == NULL) {
		func(userData, OMR_VM_THREAD_FROM_UT_THREAD(thr), NULL, modName, traceId, spec, varArgs);
		return;
	} else {
		func(userData, OMR_VM_THREAD_FROM_UT_THREAD(thr), &tls->external, modName, traceId, spec, varArgs);
	}
	return;

}

static void
raiseAssertion(UtThreadData **thread, UtModuleInfo *modInfo, uint32_t traceId)
{
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

/* Disabled until the dump functionality moves to OMR.
 * Assertion event triggered by "assert" trigger action
 * in trcengine.c and set to be triggered on asserts by
 * the default settings in trcengine.c
 * See work item: 64106
#if defined(J9VM_RAS_DUMP_AGENTS)
	J9DMP_TRIGGER(vm, thr, J9RAS_DUMP_ON_TRACE_ASSERT);
#endif
*/
	if (UT_GLOBAL(fatalassert))
	{
		char triggerClause[128];

		/* Set and fire the trigger to fire the dump assertion event. */
		memset(triggerClause, 0, sizeof(triggerClause));
		j9str_printf(PORTLIB, triggerClause, sizeof(triggerClause), "tpnid{%s.%d,assert}", (NULL != modInfo)?modInfo->name:"dg", (traceId >> 8) & UT_TRC_ID_MASK);
		setTriggerActions(thread, triggerClause, TRUE);
		fireTriggerHit(thread, (NULL != modInfo)?modInfo->name:"dg", (traceId >> 8) & UT_TRC_ID_MASK, AFTER_TRACEPOINT);
		j9exit_shutdown_and_exit(-1);
	}

	return;
}

static void fireTriggerHit(UtThreadData **thread, char *compName, uint32_t traceId, TriggerPhase phase)
{
	OMR_VMThread *thr = OMR_VM_THREAD_FROM_UT_THREAD(thread);

	UT_DBGOUT(2, ("<UT> fireTriggerHit called\n"));
	triggerHit(thr,compName,traceId,phase);
	return;
}

/*******************************************************************************
 * name        - j9TraceMem
 * description - Stub function for unused TraceMem call.
 * parameters  - N/A
 * returns     - Function asserts, should never be called.
 ******************************************************************************/
void omrTraceMem(void *env, UtModuleInfo *modInfo, uint32_t traceId, uintptr_t length, void *memptr)
{
	int	id;
	UtThreadData **thr = UT_THREAD_FROM_ENV(env);
	char qualifiedModuleName[MAX_QUALIFIED_NAME_LENGTH+1];

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	id   = (traceId >> 8) & UT_TRC_ID_MASK;

	memset(qualifiedModuleName, '\0', sizeof(qualifiedModuleName));
	if (modInfo == NULL) {
		strcpy(qualifiedModuleName, "dg");
	} else 	if ( modInfo->traceVersionInfo->traceVersion >= 7 && modInfo->containerModule != NULL ) {
		j9str_printf(PORTLIB, qualifiedModuleName, MAX_QUALIFIED_NAME_LENGTH,"%s(%s)", modInfo->name, modInfo->containerModule->name);
	} else {
		strncpy(qualifiedModuleName, modInfo->name, MAX_QUALIFIED_NAME_LENGTH);
	}

	j9tty_err_printf(PORTLIB, "* ** ASSERTION FAILED ** Obsolete trace function TraceMem called for trace point %s.%-6d", qualifiedModuleName, id);
	j9tty_err_printf(PORTLIB, "\n");

	raiseAssertion(thr, modInfo, traceId);
}

/*******************************************************************************
 * name        - j9TraceState
 * description - Stub function for unused TraceState call.
 * parameters  - N/A
 * returns     - Function asserts, should never be called.
 ******************************************************************************/

void omrTraceState(void *env, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, ...)
{
	int	id;
	UtThreadData **thr = UT_THREAD_FROM_ENV(env);
	char qualifiedModuleName[MAX_QUALIFIED_NAME_LENGTH+1];

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	id   = (traceId >> 8) & UT_TRC_ID_MASK;

	memset(qualifiedModuleName, '\0', sizeof(qualifiedModuleName));
	if (modInfo == NULL) {
		strcpy(qualifiedModuleName, "dg");
	} else 	if ( modInfo->traceVersionInfo->traceVersion >= 7 && modInfo->containerModule != NULL ) {
		j9str_printf(PORTLIB, qualifiedModuleName, MAX_QUALIFIED_NAME_LENGTH,"%s(%s)", modInfo->name, modInfo->containerModule->name);
	} else {
		strncpy(qualifiedModuleName, modInfo->name, MAX_QUALIFIED_NAME_LENGTH);
	}

	j9tty_err_printf(PORTLIB, "* ** ASSERTION FAILED ** Obsolete trace function TraceState called for trace point %s.%-6d", qualifiedModuleName, id);
	j9tty_err_printf(PORTLIB, "\n");

	raiseAssertion(thr, modInfo, traceId);
}

/*******************************************************************************
 * name        - callSubscriber
 * description - Call a registered JVMTI formatted tracepoint subscriber function
 * parameters  - UtThreadData, subscription pointer, tracepoint module,
 *               tracepoint identifier and tracepoint parameters
 * returns     - void
 ******************************************************************************/
static void
callSubscriber(UtThreadData **thr, UtSubscription *subscription, UtModuleInfo *modInfo, uint32_t traceId, va_list args)
{
	char qualifiedModuleName[MAX_QUALIFIED_NAME_LENGTH+1];
	char *moduleName = NULL;
	char *format;
	uint32_t hours, mins, secs, msecs;
	uintptr_t headerSize, dataSize;
	char *buffer;
	char *cursor;
	va_list argsCopy;
	int32_t id;
	omr_error_t rc = OMR_ERROR_NONE;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	getTimestamp(j9time_current_time_millis(), &hours, &mins, &secs, &msecs);
	
	/* Format the tracepoint module name, includes support for submodules */
	memset(qualifiedModuleName, '\0', sizeof(qualifiedModuleName));
	if (modInfo == NULL){
		strcpy(qualifiedModuleName, "dg");
		moduleName = "dg";
	} else if ( modInfo->traceVersionInfo->traceVersion >= 7 && modInfo->containerModule != NULL ) {
		j9str_printf(PORTLIB, qualifiedModuleName, MAX_QUALIFIED_NAME_LENGTH,"%s(%s)", modInfo->name, modInfo->containerModule->name);
		moduleName = modInfo->name;
	} else {
		strncpy(qualifiedModuleName, modInfo->name, MAX_QUALIFIED_NAME_LENGTH);
		moduleName = modInfo->name;
	}

	/* Find the corresponding format string for this tracepoint */
	id = (traceId >> 8) & UT_TRC_ID_MASK;
	format = getFormatString(moduleName, id);
	if (format == NULL) {
		UT_DBGOUT(1, ("<UT> Error getting tracepoint format string for tracepoint subscriber\n"));
		return;
	}

	/* Calculate the size of buffer we need to hold the tracepoint header plus formatted tracepoint data */
	headerSize = j9str_printf(PORTLIB, NULL, 0, "%02d:%02d:%02d.%03d 0x%x %s.%3d", hours, mins, secs, msecs, (*thr)->id, qualifiedModuleName, id);
	COPY_VA_LIST(argsCopy, args);
	dataSize = j9str_vprintf(NULL, 0, format, argsCopy); 

	/* Allocate a temporary buffer for the formatted tracepoint */
	buffer = j9mem_allocate_memory(headerSize + dataSize + 1, OMRMEM_CATEGORY_TRACE);
	if (buffer == NULL ) {
		UT_DBGOUT(1, ("<UT> Out of memory allocating tracepoint buffer for tracepoint subscriber\n"));
		return;
	}
	memset(buffer, '\0', headerSize + dataSize + 1);

	/* Format the tracepoint header information, followed by the tracepoint data, into the buffer */
	cursor = buffer;
	j9str_printf(PORTLIB, cursor, headerSize, "%02d:%02d:%02d.%03d 0x%x %s.%3d", hours, mins, secs, msecs, (*thr)->id, qualifiedModuleName, id);
	cursor += headerSize - 1;
	COPY_VA_LIST(argsCopy, args);
	j9str_vprintf(cursor, dataSize, format, argsCopy); 

	/* Update the subscription data with the buffer location and length */
	subscription->data = buffer;
	subscription->dataLength = (uint32_t)(headerSize + dataSize);
	/* Call out to the subscriber via the JVMTI wrapper function */
	rc = subscription->subscriber(subscription);
	/* Free the temporary formatted tracepoint buffer */
	j9mem_free_memory(buffer);

	if (OMR_ERROR_NONE != rc) {
		UT_DBGOUT(1, ("<UT> Error from tracepoint subscriber, calling alarm function and disconnecting subscriber\n"));
		subscription->alarm(subscription);
		/* disconnect this subscriber, no further tracepoints will be sent to it */
		subscription->subscriber = NULL;
	}
}
