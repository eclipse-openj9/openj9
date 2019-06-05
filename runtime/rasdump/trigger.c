/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

/*
 * Note: remove this when portlib can supply the current user name
 */
#ifdef WIN32
#include <windows.h>
#include <lmcons.h>
#else
#ifdef J9ZOS390
#include "atoe.h"
#endif
#endif

/* _GNU_SOURCE forces GLIBC_2.0 sscanf/vsscanf/fscanf for RHEL5 compatibility */
#if defined(LINUX) && !defined(J9ZTPF)
#define _GNU_SOURCE
#endif /* defined(__GNUC__) */

#include "dmpsup.h"
#include "j9dmpnls.h"
#include "j9consts.h"
#include "vmaccess.h"
#include "vmhook.h"
#include "mmhook.h"
#include "mmomrhook.h"
#include "rasdump_internal.h"
#include <stdlib.h>
#include <string.h>
#include "j9dump.h"
#include "j9cp.h"
#include "rommeth.h"
#include "objhelp.h"
#include "jvminit.h"

#include "ute.h"

typedef enum J9RASdumpMatchResult
{
	J9RAS_DUMP_NO_MATCH 				= 0,
	J9RAS_DUMP_MATCH                    = 1,
	J9RAS_DUMP_FILTER_MISMATCH          = 2
} J9RASdumpMatchResult;

#define J9_MAX_JOBNAME  16
#define J9_MAX_DUMP_DETAIL_LENGTH  512

/* Lock words, used to suspend other dumps */
static UDATA rasDumpSuspendKey = 0;
static UDATA rasDumpFirstThread = 0;

/* Postpone GC and thread event hooks until later phases of VM initialization. */
UDATA rasDumpPostponeHooks = \
	J9RAS_DUMP_ON_CLASS_UNLOAD | \
	J9RAS_DUMP_ON_GLOBAL_GC | \
	J9RAS_DUMP_ON_SLOW_EXCLUSIVE_ENTER | \
	J9RAS_DUMP_ON_OBJECT_ALLOCATION | \
	J9RAS_DUMP_ON_EXCESSIVE_GC | \
	J9RAS_DUMP_ON_THREAD_START | \
	J9RAS_DUMP_ON_THREAD_BLOCKED | \
	J9RAS_DUMP_ON_THREAD_END;

/* Outstanding hook requests - flushed after VM init */
UDATA rasDumpPendingHooks = 0;

/* Cached VM event handlers for use by J9VMRASdumpHooks */
UDATA rasDumpUnhookedEvents = J9RAS_DUMP_ON_ANY;
void *rasDumpOldHooks[J9RAS_DUMP_HOOK_TABLE_SIZE];


static void rasDumpHookVmInit (J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData);
static void rasDumpHookGCInitialized(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData);
static void rasDumpHookAllocationThreshold(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData);
static void rasDumpHookSlowExclusive (J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData);
static void rasDumpHookThreadStart (J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData);
static void rasDumpHookExceptionDescribe (J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData);
#if (defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)) 
static void rasDumpHookClassesUnload (J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
static void rasDumpHookVmShutdown (J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData);
static void rasDumpHookExceptionThrow (J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData);
static void rasDumpHookGlobalGcStart (J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData);
static void rasDumpHookThreadEnd (J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData);
static J9RASdumpMatchResult matchesFilter (J9VMThread *vmThread, J9RASdumpEventData *eventData, UDATA eventFlags, char *filter, char *subFilter);
static void rasDumpHookExceptionSysthrow PROTOTYPE((J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData));
static void rasDumpHookClassLoad (J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData);
static void rasDumpHookExceptionCatch (J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData);
static void rasDumpHookMonitorContendedEnter (J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData);
static void rasDumpHookCorruptCache(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData);
static void rasDumpHookExcessiveGC(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData);

extern omr_error_t doHeapDump (J9RASdumpAgent *agent, char *label, J9RASdumpContext *context);
extern omr_error_t doSystemDump (J9RASdumpAgent *agent, char *label, J9RASdumpContext *context);
extern omr_error_t doSilentDump(J9RASdumpAgent *agent, char *label, J9RASdumpContext *context);
extern omr_error_t doCEEDump (J9RASdumpAgent *agent, char *label, J9RASdumpContext *context);
extern omr_error_t doToolDump (J9RASdumpAgent *agent, char *label, J9RASdumpContext *context);

extern void setAllocationThreshold(J9VMThread *vmThread, UDATA min, UDATA max);

struct ExceptionStackFrame
{
	J9ROMClass  *romClass;
	J9ROMMethod *romMethod;
	int callStackOffset;
	int desiredOffset;
};

static UDATA
countExceptionStackFrame(J9VMThread *vmThread, void *userData, J9ROMClass *romClass, J9ROMMethod *romMethod, J9UTF8 *fileName, UDATA lineNumber, J9ClassLoader* classLoader)
{
	struct ExceptionStackFrame *frame = (struct ExceptionStackFrame *) userData;

	/* Stop and fill in the struct when we have reached the required frame. */
	if (frame->callStackOffset++ == frame->desiredOffset) {
		frame->romClass  = romClass;
		frame->romMethod = romMethod;
		return FALSE;
	}

	return TRUE;
}

/**
 * Multiply the given 'val' by a suffix character. Supports 'k' and 'm'.
 * 
 * @param[in/out] val - value to update
 * @param[in]  suffix - suffix character to process
 * 
 * @return one on success, zero on failure
 */
static UDATA multiplyBySuffix(UDATA *val, char suffix)
{
	switch (suffix) {
	case 'k':
	case 'K':
		*val *= 1024;
		return 1;
	case 'm':
	case 'M':
		*val *= 1024 * 1024;
		return 1;
	}
	
	return 0;
}

/**
 * Parse an allocation range of the form "#5m" or "#5m..6m".
 * 
 * @param[in]  range - string containing the range
 * @param[out] min   - lower bound of the range
 * @param[out] max   - upper bound of the range (optional).
 * 
 * @return zero on failure, one on success.
 */
UDATA
parseAllocationRange(char *range, UDATA *min, UDATA *max)
{
	if (*range != '#') {
		return 0;
	}
	range++;
	
	if (scan_udata(&range, min) != 0) {
		/* No matching numeric value */
		return 0;
	}
	if (multiplyBySuffix(min, *range)) {
		range++;
	}
	
	if (try_scan(&range, "..")) {
		if (scan_udata(&range, max) != 0) {
			/* No matching numeric value */
			return 0;
		}
		multiplyBySuffix(max, *range);
	} else {
		*max = UDATA_MAX;
	}
	
	if (*min > *max) {
		return 0;
	}
	
	return 1;
}

static J9RASdumpMatchResult
matchesObjectAllocationFilter(J9RASdumpEventData *eventData, char *filter)
{
	char *message = eventData->detailData;
	char* msgPtr;
	UDATA msgValue;
	char msgText[20];
	char* fltPtr;
	UDATA fltValueMin, fltValueMax;
	char fltText[20];
	
	if (!filter) {
		/* Must have a filter for matching object allocation */
		return J9RAS_DUMP_NO_MATCH;
	}
	
	strncpy(msgText, message, sizeof(msgText));
	strncpy(fltText, filter, sizeof(fltText));

	/* Convert the message to a number */
	msgPtr = msgText;
	if (scan_udata(&msgPtr, &msgValue) != 0) {
		/* No matching numeric value */
		return J9RAS_DUMP_NO_MATCH;
	}
	
	/* Convert the filter range to two numbers */
	fltPtr = fltText;
	if (!parseAllocationRange(fltPtr, &fltValueMin, &fltValueMax)) {
		return J9RAS_DUMP_NO_MATCH;
	}
	
	/* Do the range check */
	if (msgValue >= fltValueMin && msgValue <= fltValueMax) {
		return J9RAS_DUMP_MATCH;
	}

	return J9RAS_DUMP_NO_MATCH;
}

static J9RASdumpMatchResult
matchesSlowExclusiveEnterFilter(J9RASdumpEventData *eventData, char *filter)
{
	char *message = eventData->detailData;
	char* msgPtr;
	IDATA msgValue;
	char msgText[20];
	char* fltPtr;
	IDATA fltValue;
	char fltText[20];

	strncpy(msgText, message, sizeof(msgText));
	strncpy(fltText, filter, sizeof(fltText));

	/* convert the message value to a number */
	msgPtr = msgText;
	if (scan_idata(&msgPtr, &msgValue) != 0) {
		/* No matching numeric value */
		return J9RAS_DUMP_NO_MATCH;
	}

	/* convert the filter value to a number */
	fltPtr = fltText;
	if (*fltPtr == '#') {
		/* Skip over the leading #, if any. See defect 196215, as well as allowing a leading # (as documented) we are 
		 * deliberately preserving the previous behaviour, which allowed the user to specify filter=<nn>ms, without the #
		 */
		fltPtr++; 
	}
	if (scan_idata(&fltPtr, &fltValue) != 0) {
		/* No matching numeric value */
		return J9RAS_DUMP_NO_MATCH;
	}

	if (strcmp(fltPtr, "ms") != 0) {
		/* No matching range */
		return J9RAS_DUMP_NO_MATCH;
	}

	/* compare the filter with the message */
	if (msgValue >= fltValue) {
		return J9RAS_DUMP_MATCH;
	} else {
		return J9RAS_DUMP_NO_MATCH;
	}
}

static J9RASdumpMatchResult
matchesVMShutdownFilter(J9RASdumpEventData *eventData, char *filter)
{
	char *message = eventData->detailData;
	IDATA value;
	
	/* Numeric range comparison? */
	if (*message != '#') {
		return J9RAS_DUMP_NO_MATCH;
	}

	if (filter && *filter != '#') {
		/* Special case: text filter has been applied to a numeric message (ie. vmstop event) */
		return J9RAS_DUMP_FILTER_MISMATCH;
	}

	message++;

	/* Number detail encoded as null-terminated hex */
	scan_hex(&message, (UDATA *)&value);

	/* Match to number ranges encoded in filter string */
	while (try_scan(&filter, "#")) {
		IDATA lhs, rhs;
		
		scan_idata(&filter, &lhs);

		if (try_scan(&filter, "..")) {
			scan_idata(&filter, &rhs);
		} else {
			rhs = lhs;
		}

		if (lhs <= value && value <= rhs) {
			return J9RAS_DUMP_MATCH;
		}
	}

	/* No matching range */
	return J9RAS_DUMP_NO_MATCH;
}

static J9RASdumpMatchResult
matchesExceptionFilter(J9VMThread *vmThread, J9RASdumpEventData *eventData, UDATA eventFlags, char *filter, char *subFilter)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	char *message = eventData->detailData;
	UDATA nbytes = eventData->detailLength;
	UDATA buflen = 0;
	char *buf = NULL;
	const char *needleString = NULL;
	UDATA needleLength;
	U_32 matchFlag;
	UDATA retCode = J9RAS_DUMP_NO_MATCH;

   	if (eventData->exceptionRef && filter != NULL) {
		j9object_t exception = *((j9object_t *) eventData->exceptionRef);
		char *hashSignInFilter = NULL;
		char *stackOffsetFilter = NULL;
		struct ExceptionStackFrame throwSite;
		
		throwSite.romClass = NULL;
		throwSite.romMethod = NULL;
		throwSite.callStackOffset = 0;
		throwSite.desiredOffset = 0;

		/* Filter an exception event on throw/catch site if the new filter syntax is used */
		hashSignInFilter = strrchr((const char *) filter, '#');
		if (NULL != hashSignInFilter) {
			hashSignInFilter++;
			if (*hashSignInFilter >= '0' && *hashSignInFilter <= '9') {
				stackOffsetFilter = hashSignInFilter;
				sscanf(hashSignInFilter, "%d", &throwSite.desiredOffset);
			}

			if (eventFlags & J9RAS_DUMP_ON_EXCEPTION_CATCH) {
				J9StackWalkState * walkState = vmThread->stackWalkState;
				if (NULL != walkState) {
					walkState->walkThread = vmThread;
					walkState->flags = J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_COUNT_SPECIFIED;
					walkState->skipCount = 0;
					walkState->maxFrames = 1;
					vmThread->javaVM->walkStackFrames(vmThread, walkState);
					if (NULL != walkState->method) {
						throwSite.romClass  = J9_CLASS_FROM_METHOD(walkState->method)->romClass;
						throwSite.romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(walkState->method);
					}
				}
			} else {
				/* For other events, walk the stack to find the desired frame */
				vmThread->javaVM->internalVMFunctions->iterateStackTrace(vmThread, (j9object_t*) eventData->exceptionRef, countExceptionStackFrame, &throwSite, TRUE);
			}
		}

		if (throwSite.romClass && throwSite.romMethod) {
			J9UTF8 *exceptionClassName = J9ROMCLASS_CLASSNAME(J9OBJECT_CLAZZ(vmThread, exception)->romClass);
			J9UTF8 *throwClassName  = J9ROMCLASS_CLASSNAME(throwSite.romClass);
			J9UTF8 *throwMethodName = J9ROMMETHOD_GET_NAME(throwSite.romClass, throwSite.romMethod);
			
			if (throwClassName && throwMethodName) {
				if (stackOffsetFilter) {
					buflen = J9UTF8_LENGTH(exceptionClassName) + J9UTF8_LENGTH(throwClassName) + J9UTF8_LENGTH(throwMethodName) + strlen(stackOffsetFilter) + 3;
				} else {
					buflen = J9UTF8_LENGTH(exceptionClassName) + J9UTF8_LENGTH(throwClassName) + J9UTF8_LENGTH(throwMethodName) + 2;
				}
				buf = j9mem_allocate_memory(buflen + 1, OMRMEM_CATEGORY_VM);

				if (buf != NULL) {
					int end = J9UTF8_LENGTH(exceptionClassName);
					memcpy(buf, J9UTF8_DATA(exceptionClassName), J9UTF8_LENGTH(exceptionClassName));
					buf[end] = '#';
					memcpy(buf + end + 1, J9UTF8_DATA(throwClassName), J9UTF8_LENGTH(throwClassName));
					end += J9UTF8_LENGTH(throwClassName) + 1;
					buf[end] = '.';
					memcpy(buf + end + 1, J9UTF8_DATA(throwMethodName), J9UTF8_LENGTH(throwMethodName));
					if (stackOffsetFilter) {
						end += J9UTF8_LENGTH(throwMethodName) + 1;
						buf[end] = '#';
						j9str_printf(PORTLIB, buf + end + 1, buflen - end, "%d", throwSite.desiredOffset);
					}
					buf[buflen] = '\0';
				}
			}
		}
	}

	if (buf && buflen) {
		message = buf;
		nbytes  = buflen;
	}

	/* Apply standard text filter */
	if (filter && parseWildcard(filter, strlen(filter), &needleString, &needleLength, &matchFlag) == 0) {
		if (wildcardMatch(matchFlag, needleString, needleLength, message, nbytes)) {
			retCode = J9RAS_DUMP_MATCH;
		} else {
			if (buf != NULL) {
				j9mem_free_memory(buf);
			}
			return retCode;			
		}
	}
	
	if (buf != NULL) {
		j9mem_free_memory(buf);
		buf = NULL;
		buflen = 0;
	}	

	if (subFilter && parseWildcard(subFilter, strlen(subFilter), &needleString, &needleLength, &matchFlag) == 0) {
		if (eventData->exceptionRef && *eventData->exceptionRef) {
			char stackBuffer[256];
			j9object_t emessage = J9VMJAVALANGTHROWABLE_DETAILMESSAGE(vmThread, *eventData->exceptionRef);

			if (NULL != emessage) {
				buf = vmThread->javaVM->internalVMFunctions->copyStringToUTF8WithMemAlloc(vmThread, emessage, J9_STR_NULL_TERMINATE_RESULT, "", 0, stackBuffer, 256, &buflen);

				if (NULL != buf) {
					if (wildcardMatch(matchFlag, needleString, needleLength, buf, buflen)) {
						retCode = J9RAS_DUMP_MATCH;
					} else {
						retCode = J9RAS_DUMP_NO_MATCH;
					}
				}
			}

			if (buf != stackBuffer) {
				j9mem_free_memory(buf);
			}
		}
	}

	return retCode;
}

static J9RASdumpMatchResult
matchesFilter(J9VMThread *vmThread, J9RASdumpEventData *eventData, UDATA eventFlags, char *filter, char *subFilter)
{
	if (eventFlags & J9RAS_DUMP_ON_OBJECT_ALLOCATION) {
		/* This comes before the default filter because object allocation MUST have a filter */
		return matchesObjectAllocationFilter(eventData, filter);
	}
	
	/* For exception specific events the filter and subfilter default(null) matches to all  
	 * For non exception specific events the filter default(null) matches to all
	 */	
    if (((0 != (eventFlags & J9RAS_DUMP_EXCEPTION_EVENT_GROUP)) && NULL == filter && NULL == subFilter) ||
        ((0 == (eventFlags & J9RAS_DUMP_EXCEPTION_EVENT_GROUP)) && NULL == filter))
    {
    	return J9RAS_DUMP_MATCH;
    }
    	
	if (eventFlags & J9RAS_DUMP_ON_SLOW_EXCLUSIVE_ENTER) {
		return matchesSlowExclusiveEnterFilter(eventData, filter);
	} else if (eventFlags & J9RAS_DUMP_ON_VM_SHUTDOWN) {
		return matchesVMShutdownFilter(eventData, filter);
	} else if (0 != (eventFlags & (J9RAS_DUMP_EXCEPTION_EVENT_GROUP | J9RAS_DUMP_ON_CLASS_LOAD))) {
		return matchesExceptionFilter(vmThread, eventData, eventFlags, filter, subFilter);
	}
	
	return J9RAS_DUMP_NO_MATCH;
}

#if (defined(J9VM_RAS_DUMP_AGENTS)) 
omr_error_t
printLabelSpec(struct J9JavaVM *vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	/* Since j9tty_err_printf() is a function macro, #ifdefs can't be used in
	 * its argument list.
	 */
	const char *labelSpec =
			"  %%Y     year    1900..????\n"
			"  %%y     century   00..99\n"
			"  %%m     month 	01..12\n"
			"  %%d     day   	01..31\n"
			"  %%H     hour  	00..23\n"
			"  %%M     minute	00..59\n"
			"  %%S     second	00..59\n"
			"\n"
			"  %%pid   process id\n"
			"  %%uid   user name\n"
#ifdef J9ZOS390
			"  %%job   job name\n"
			"  %%jobid job ID\n"
			"  %%asid  ASID\n"
#endif
			"  %%seq   dump counter\n"
			"  %%tick  msec counter\n"
			"  %%home  java home\n"
			"  %%last  last dump\n"
			"  %%event dump event\n"
			"\n";
	j9tty_err_printf(PORTLIB, labelSpec);
	return OMR_ERROR_NONE;
}
#endif /* J9VM_RAS_DUMP_AGENTS */


#if (defined(J9VM_RAS_DUMP_AGENTS)) 
UDATA
prepareForDump(struct J9JavaVM *vm, struct J9RASdumpAgent *agent, struct J9RASdumpContext *context, UDATA state)
{
	UDATA dumpKey = 1 + (UDATA)omrthread_self();
	J9VMThread *vmThread = context->onThread;
	UDATA newState = state;
	RasGlobalStorage * j9ras = (RasGlobalStorage *)vm->j9rasGlobalStorage;
	UtInterface * uteInterface = (UtInterface *)(j9ras ? j9ras->utIntf : NULL);
	BOOLEAN exclusiveHeld = J9_XACCESS_NONE != vm->exclusiveAccessState;

	/* Is trace running? */
	if( uteInterface && uteInterface->server ) {
		/* Disable trace while taking a dump. */
		uteInterface->server->DisableTrace(UT_DISABLE_GLOBAL);
		newState |= J9RAS_DUMP_TRACE_DISABLED;
	}

	if ((context->eventFlags & (J9RAS_DUMP_ON_GP_FAULT | J9RAS_DUMP_ON_ABORT_SIGNAL | J9RAS_DUMP_ON_TRACE_ASSERT)) == 0) {
		/*
		 * The following actions may deadlock, so don't use them
		 * if this is a crash situation or a trace assertion.
		 */

		/* Share exclusive access when it's a "slow entry" or "user" event, as there may be a deadlock */
		UDATA shareVMAccess = exclusiveHeld &&
				( (context->eventFlags & J9RAS_DUMP_ON_USER_SIGNAL) ||
				(context->eventFlags & J9RAS_DUMP_ON_SLOW_EXCLUSIVE_ENTER) );

		if ( shareVMAccess == 0 ) {

			/* Deferred attach of SigQuit thread, needed if we're preparing to walk the heap (GC pre-req) */
			if ( (agent->requestMask & (J9RAS_DUMP_DO_PREPARE_HEAP_FOR_WALK | J9RAS_DUMP_DO_COMPACT_HEAP | J9RAS_DUMP_DO_ATTACH_THREAD)) &&
			(context->eventFlags & J9RAS_DUMP_ON_USER_SIGNAL ) ) {

				JavaVMAttachArgs attachArgs;

				attachArgs.version = JNI_VERSION_1_2;
				attachArgs.name = "SIGQUIT Thread";
				attachArgs.group = NULL;

				if (!vmThread) {
					vm->internalVMFunctions->AttachCurrentThreadAsDaemon((JavaVM *)vm, (void **)&vmThread, &attachArgs);
					context->onThread = vmThread;
					newState |= J9RAS_DUMP_ATTACHED_THREAD;
				} else {
					/* already attached, don't set flag to detach us on way out of heapdump! */
				}
			}

			if ( (agent->requestMask & J9RAS_DUMP_DO_EXCLUSIVE_VM_ACCESS) &&
			(state & J9RAS_DUMP_GOT_EXCLUSIVE_VM_ACCESS) == 0 ) {

				if (vmThread) {
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
					if (vmThread->inNative) {
						vm->internalVMFunctions->internalEnterVMFromJNI(vmThread);
						newState |= J9RAS_DUMP_GOT_JNI_VM_ACCESS;
					} else
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
					if ((vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) == 0) {
						vm->internalVMFunctions->internalAcquireVMAccess(vmThread);
						newState |= J9RAS_DUMP_GOT_VM_ACCESS;
					}
					vm->internalVMFunctions->acquireExclusiveVMAccess(vmThread);
				} else {
					vm->internalVMFunctions->acquireExclusiveVMAccessFromExternalThread(vm);
				}

				newState |= J9RAS_DUMP_GOT_EXCLUSIVE_VM_ACCESS;
			}
		}
	}

	if ( (agent->requestMask & J9RAS_DUMP_DO_COMPACT_HEAP) &&
	((state & J9RAS_DUMP_HEAP_COMPACTED) == 0 ) ) {
		/* If exclusive access has been obtained, do the requested compaction */
		if ((newState & J9RAS_DUMP_GOT_EXCLUSIVE_VM_ACCESS) && (vmThread != 0)) {
			J9RASdumpEventData *eventData = context->eventData;

			/* Don't try and compact the heap if it may cause recursion in GC */
			UDATA gcEvent =
			(context->eventFlags & J9RAS_DUMP_ON_GLOBAL_GC) ||
			(context->eventFlags & J9RAS_DUMP_ON_CLASS_UNLOAD) ||
			(context->eventFlags & J9RAS_DUMP_ON_EXCESSIVE_GC) ||
			(eventData && matchesFilter(vmThread, eventData, context->eventFlags, "*OutOfMemoryError", NULL) == J9RAS_DUMP_MATCH) ||
			/* Tracepoint trigger when exclusive is held also indicates we may be in GC */
			(eventData && eventData->detailData && strcmp(eventData->detailData,"-Xtrace:trigger") == 0 && exclusiveHeld);

			/*
			 * The extra check of this runtime flag is to defer invoking GC till class objects are assigned during startup;
			 * otherwise NULL class objects would be captured by GC assertion.
			 */
			if ( J9_ARE_ALL_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_CLASS_OBJECT_ASSIGNED) && !gcEvent ) {
				vm->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides(vmThread, J9MMCONSTANT_EXPLICIT_GC_RASDUMP_COMPACT);
				newState |= J9RAS_DUMP_HEAP_COMPACTED;
			}
		}
	}

	if ( (agent->requestMask & J9RAS_DUMP_DO_PREPARE_HEAP_FOR_WALK) &&
	((state & J9RAS_DUMP_HEAP_PREPARED) == 0 ) ) {
		/* If exclusive access has been obtained, do the requested preparation */
		if (newState & J9RAS_DUMP_GOT_EXCLUSIVE_VM_ACCESS) {
			vm->memoryManagerFunctions->j9gc_flush_caches_for_walk(vm);
			newState |= J9RAS_DUMP_HEAP_PREPARED;
		}
	}

	if ( (agent->requestMask & J9RAS_DUMP_DO_HALT_ALL_THREADS) &&
	(state & J9RAS_DUMP_THREADS_HALTED) == 0 ) {

		/**** NOT YET IMPLEMENTED (removed from -Xdump:request) ****/

		newState |= J9RAS_DUMP_THREADS_HALTED;
	}

	/*
	 * The following actions are considered safe to call during a crash situation...
	 */

	/* For fatal events, the first failing thread sets the global rasDumpFirstThread. It then gets higher priority on the
	 * serial dump lock, see below. This allows the first failing thread to complete its dumps and exit the VM, reducing
	 * the number of dumps written and out-time if multiple threads crash.
	 */
	if (0 != (context->eventFlags & (J9RAS_DUMP_ON_GP_FAULT | J9RAS_DUMP_ON_ABORT_SIGNAL | J9RAS_DUMP_ON_TRACE_ASSERT))) {
		compareAndSwapUDATA(&rasDumpFirstThread, 0, dumpKey);
	}

	if (rasDumpSuspendKey == dumpKey) {
		/* We already have the lock */
	} else {
		UDATA newKey = 0;

		/* Grab the dump lock? */
		if (agent->requestMask & J9RAS_DUMP_DO_SUSPEND_OTHER_DUMPS) {
			newState |= J9RAS_DUMP_GOT_LOCK;
			newKey = dumpKey;
		}

		/* Always wait for the lock, but only grab it when requested */
		while (compareAndSwapUDATA(&rasDumpSuspendKey, 0, newKey) != 0) {
			if (rasDumpFirstThread == dumpKey) {
				/* First failing thread gets a simple priority boost over other threads waiting for lock */
				omrthread_sleep(20);
			} else {
				omrthread_sleep(200);
			}
		}
	}

	return newState;
}
#endif /* J9VM_RAS_DUMP_AGENTS */


#if (defined(J9VM_RAS_DUMP_AGENTS)) 
UDATA
unwindAfterDump(struct J9JavaVM *vm, struct J9RASdumpContext *context, UDATA state)
{
	UDATA dumpKey = 1 + (UDATA)omrthread_self();
	J9VMThread *vmThread = context->onThread;

	UDATA newState = state;

	/*
	 * Must be in reverse order to the requested actions
	 */

	if (state & J9RAS_DUMP_GOT_LOCK) {

		/* Should work unless omrthread_self returns a different value than before, which is unlikely */
		compareAndSwapUDATA(&rasDumpSuspendKey, dumpKey, 0);
		newState &= ~J9RAS_DUMP_GOT_LOCK;
	}

	if (state & J9RAS_DUMP_THREADS_HALTED) {

		/**** NOT YET IMPLEMENTED (removed from -Xdump:request) ****/

		newState &= ~J9RAS_DUMP_THREADS_HALTED;
	}

	if (state & J9RAS_DUMP_GOT_EXCLUSIVE_VM_ACCESS) {

		if (vmThread) {
			vm->internalVMFunctions->releaseExclusiveVMAccess(vmThread);
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
			if (state & J9RAS_DUMP_GOT_JNI_VM_ACCESS) {
				vm->internalVMFunctions->internalExitVMToJNI(vmThread);
				newState &= ~J9RAS_DUMP_GOT_JNI_VM_ACCESS;
			} else
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
			if (state & J9RAS_DUMP_GOT_VM_ACCESS) {
				vm->internalVMFunctions->internalReleaseVMAccess(vmThread);
				newState &= ~J9RAS_DUMP_GOT_VM_ACCESS;
			}
		} else {
			vm->internalVMFunctions->releaseExclusiveVMAccessFromExternalThread(vm);
		}

		/* Releasing exclusive access potentially invalidates the state of the heap... */
		newState &= ~( J9RAS_DUMP_GOT_EXCLUSIVE_VM_ACCESS | J9RAS_DUMP_HEAP_COMPACTED | J9RAS_DUMP_HEAP_PREPARED );
	}

	if (state & J9RAS_DUMP_ATTACHED_THREAD) {

		(*((JavaVM *)vm))->DetachCurrentThread((JavaVM *)vm);
		context->onThread = NULL;
		newState &= ~J9RAS_DUMP_ATTACHED_THREAD;
	}

	if( state & J9RAS_DUMP_TRACE_DISABLED) {
		RasGlobalStorage * j9ras = (RasGlobalStorage *)vm->j9rasGlobalStorage;
		UtInterface * uteInterface = (UtInterface *)(j9ras ? j9ras->utIntf : NULL);
		/* Is trace running? */
		if( uteInterface && uteInterface->server ) {
			/* Re-enable trace now we are out of the dump code.*/
			uteInterface->server->EnableTrace(UT_ENABLE_GLOBAL);
			newState &= ~J9RAS_DUMP_TRACE_DISABLED;
		}
	}

	return newState;
}
#endif /* J9VM_RAS_DUMP_AGENTS */


#if (defined(J9VM_RAS_DUMP_AGENTS))
/*
 * Function : dumpLabel()
 * Convert a dump label template into an actual dump label by expanding all the tokens.
 *
 * Parameters:
 *  vm [in] 	 - VM structure pointer
 *  agent		 - dump agent pointer
 *  context		 - dump context pointer
 *  buf [in/out] - memory buffer for expanded label
 *  len [in]	 - length of supplied buffer
 *  reqLen [out] - length of buffer required, if expansion would have overflowed buf
 *  now [in]	 - current time
 * 
 * Returns: OMR_ERROR_NONE, OMR_ERROR_INTERNAL, OMR_ERROR_OUT_OF_NATIVE_MEMORY
 */
omr_error_t
dumpLabel(struct J9JavaVM *vm, J9RASdumpAgent *agent, J9RASdumpContext *context, char *buf, size_t len, UDATA *reqLen, I_64 now)
{
	/* Monotonic counter */
	static UDATA seqNum = 0;
	struct J9StringTokens *stringTokens;
	RasDumpGlobalStorage *dump_storage = (RasDumpGlobalStorage *)vm->j9rasdumpGlobalStorage;

	PORT_ACCESS_FROM_JAVAVM(vm);

	/* access the rasdump global storage */
	if (NULL == dump_storage) {
		return OMR_ERROR_INTERNAL;
	}
	
	/* lock access to the tokens */
	omrthread_monitor_enter(dump_storage->dumpLabelTokensMutex);

	stringTokens = dump_storage->dumpLabelTokens;

	j9str_set_time_tokens(stringTokens, now);
	
	seqNum += 1; /* Atomicity guaranteed as we are inside the dumpLabelTokensMutex */

	if (j9str_set_token(PORTLIB, stringTokens, "seq", "%04u", seqNum)) {
		omrthread_monitor_exit(dump_storage->dumpLabelTokensMutex);
		return OMR_ERROR_INTERNAL;
	}

	if (j9str_set_token(PORTLIB, stringTokens, "home", "%s", (vm->javaHome == NULL) ? "" : (char *)vm->javaHome)) {
		omrthread_monitor_exit(dump_storage->dumpLabelTokensMutex);
		return OMR_ERROR_INTERNAL;
	}

	if (j9str_set_token(PORTLIB, stringTokens, "event", "%s", mapDumpEvent(context->eventFlags))) {
		omrthread_monitor_exit(dump_storage->dumpLabelTokensMutex);
		return OMR_ERROR_INTERNAL;
	}

	if (j9str_set_token(PORTLIB, stringTokens, "list", "%s", (context->dumpList == NULL) ? "" : context->dumpList)) {
		omrthread_monitor_exit(dump_storage->dumpLabelTokensMutex);
		return OMR_ERROR_INTERNAL;
	}

	/* %vmbin is not listed in printLabelSpec as it is only useful for loading internal tools that live in the vm directory. */
	if (j9str_set_token(PORTLIB, stringTokens, "vmbin", "%s", (vm->j2seRootDirectory == NULL) ? "" : (char *)vm->j2seRootDirectory)) {
		omrthread_monitor_exit(dump_storage->dumpLabelTokensMutex);
		return OMR_ERROR_INTERNAL;
	}

	/* Default label is "-", ie. stderr */
	if (agent->labelTemplate == NULL) {
		agent->labelTemplate = "-";
	}

	/* Check the return value here to see if token expansion fitted in the buffer */
	*reqLen = j9str_subst_tokens(buf, len, agent->labelTemplate, stringTokens);
	if (*reqLen > len) {
		omrthread_monitor_exit(dump_storage->dumpLabelTokensMutex);
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	 if (agent->dumpFn != doToolDump ) {
		 /* Cache last dump label (but not for tool dumps!) */
		 if (j9str_set_token(PORTLIB, stringTokens, "last", "%s", buf)) {
			 omrthread_monitor_exit(dump_storage->dumpLabelTokensMutex);
			 return OMR_ERROR_INTERNAL;
		 }
	}

	/* release access to the tokens */
	omrthread_monitor_exit(dump_storage->dumpLabelTokensMutex);

	return OMR_ERROR_NONE;
}
#endif /* J9VM_RAS_DUMP_AGENTS */



#if (defined(J9VM_RAS_DUMP_AGENTS)) 
omr_error_t
triggerOneOffDump(struct J9JavaVM *vm, char *optionString, char *caller, char *fileName, size_t fileNameLength)
{
	IDATA kind;
	omr_error_t retVal = OMR_ERROR_INTERNAL;
	size_t len;

	if( optionString == NULL ) {
		return OMR_ERROR_INTERNAL;
	}

	kind = scanDumpType(&optionString);

	if ( kind >= 0 ) {
		J9RASdumpContext context;
		J9RASdumpEventData eventData;
		
		/* we lock the dump configuration here so that the agent and setting queues can't be
		 * changed underneath us while we're producing the dumps
		 */
		lockConfigForUse();

		/* Construct a pseudo-context */
		context.javaVM = vm;
		context.onThread = vm->internalVMFunctions->currentVMThread(vm);
		context.eventFlags = J9RAS_DUMP_ON_USER_REQUEST;
		context.eventData = &eventData;
		context.dumpList = fileName;
		context.dumpListSize = fileNameLength;
		context.dumpListIndex = 0;
		
		eventData.detailData = caller;
		if (caller != NULL) {
			eventData.detailLength = strlen(caller);
		} else {
			eventData.detailLength = 0;
		}
		eventData.exceptionRef = NULL;
		
		retVal = createAndRunOneOffDumpAgent(vm,&context,kind,optionString);

		/* Remove the trailing tab added to the filename as a separator, it's only
		 * used for multiple dumps and will confuse the caller.
		 */
		if( fileName ) {
			len = strlen(fileName);
		} else {
			len = 0;
		}
		if( len > 0 && len <= fileNameLength) {
			if( fileName[len-1] == '\t') {
				fileName[len-1] = '\0';
			}
		}

		/* Allow configuration updates again */
		unlockConfig();
	}

	return retVal;
}
#endif /* J9VM_RAS_DUMP_AGENTS */


#if (defined(J9VM_RAS_DUMP_AGENTS)) 
omr_error_t
triggerDumpAgents(struct J9JavaVM *vm, struct J9VMThread *self, UDATA eventFlags, struct J9RASdumpEventData *eventData)
{
	J9RASdumpQueue *queue;

	/* we lock the dump configuration here so that the agent and setting queues can't be
	 * changed underneath us while we're producing the dumps
	 */
	lockConfigForUse();

	/*
	 * Sanity check
	 */
	if ( FIND_DUMP_QUEUE(vm, queue) ) {
		J9RASdumpAgent *node;
		PORT_ACCESS_FROM_JAVAVM(vm);
		U_32 dumpTaken = 0;
		U_32 printed = 0;
		BOOLEAN toolDumpFound = FALSE;
		IDATA dumpAgentCount = 0;
		UDATA state = 0;

		U_64 now = j9time_current_time_millis();

		UDATA detailLength = eventData ? eventData->detailLength : 0;
		char *detailData = detailLength ? eventData->detailData : "";
		char detailBuf[J9_MAX_DUMP_DETAIL_LENGTH + 1];

		J9RASdumpContext context;

		context.javaVM = vm;
		context.onThread = self;
		context.eventFlags = eventFlags;
		context.eventData = eventData;
		context.dumpList = NULL;
		context.dumpListSize = 0;
		context.dumpListIndex = 0;

		if (detailLength > J9_MAX_DUMP_DETAIL_LENGTH) {
			detailLength = J9_MAX_DUMP_DETAIL_LENGTH;
		}
		
		strncpy(detailBuf, detailData, detailLength);
		detailBuf[detailLength] = '\0';

		/* Scan the dump agents first to see if we need to provide a dump list for tool agents */
		for ( node = queue->agents; node != NULL; node = node->nextPtr ) {
			if ( eventFlags & node->eventMask ) {
				if ( node->dumpFn == doToolDump ) {
					toolDumpFound = TRUE;
				} else {
					/* count number of agents for this event, not including tool dumps themselves */
					dumpAgentCount++;

					if (node->dumpFn == doHeapDump && strstr(node->dumpOptions, "CLASSIC") && strstr(node->dumpOptions, "PHD")) {
						/* fake up a slot for the dual dump */
						dumpAgentCount++;
					}
				}
			}
		}
		if (toolDumpFound && (dumpAgentCount > 0)) {
			/* there is a tool dump, so allocate a buffer for the list of dump labels. Need to account for the \t separators and \0 */
			context.dumpListSize = ((J9_MAX_DUMP_PATH +1) * dumpAgentCount) + 1;
			context.dumpList = j9mem_allocate_memory(context.dumpListSize, OMRMEM_CATEGORY_VM);
			if (context.dumpList) {
				memset(context.dumpList, 0, context.dumpListSize);
			}
		}

		/* Trigger agents for this event, in priority order */
		for ( node = queue->agents; node != NULL; node = node->nextPtr ) {
			if ( eventFlags & node->eventMask ) {

				/* NOTE: we allow trigger on filter mismatch (ie. exception text filter applied to vmstop exit code) */
				if (NULL == eventData || matchesFilter(self, eventData, eventFlags, node->detailFilter, node->subFilter) != J9RAS_DUMP_NO_MATCH) {
					/* increment count, but don't go past stopOnCount for a finite range */
					UDATA oldCount = node->count;
					UDATA newCount = oldCount + 1;

					while ((newCount <= node->stopOnCount) || (node->stopOnCount < node->startOnCount)) {
						UDATA current = compareAndSwapUDATA(&node->count, oldCount, newCount);

						if (current == oldCount) {
							/* increment was successful */
							break;
						}

						oldCount = current;
						newCount = current + 1;
					}

					/* Now check if the updated count is within the trigger range. */
					if ((newCount >= node->startOnCount) &&
							((node->stopOnCount < node->startOnCount) || (newCount <= node->stopOnCount))) {
						if (printed == 0) {
							if (node->dumpFn != doSilentDump) {
								char dateStamp[64];
								j9str_ftime(dateStamp, sizeof(dateStamp), "%Y/%m/%d %H:%M:%S", now);
								j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR | J9NLS_VITAL, J9NLS_DMP_PROCESSING_EVENT_TIME, mapDumpEvent(eventFlags), detailLength, detailData, dateStamp);
								printed = 1;
							}
						}

						runDumpAgent(vm,node,&context,&state,detailBuf,now);
						dumpTaken = 1;
					}
				}
			}
		}

		if (dumpTaken == 1) {
			/* Release accumulated locks */
			state = unwindAfterDump(vm, &context, state);
			if (printed == 1) {
				j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_PROCESSED_EVENT_STR, mapDumpEvent(eventFlags), detailLength, detailData);
			}
		}

		if (context.dumpList) {
			j9mem_free_memory(context.dumpList);
		}

		/* Allow configuration updates again */
		unlockConfig();

		return OMR_ERROR_NONE;
	}

	/* Allow configuration updates again */
	unlockConfig();

	return OMR_ERROR_INTERNAL;
}
#endif /* J9VM_RAS_DUMP_AGENTS */


omr_error_t
rasDumpEnableHooks(J9JavaVM *vm, UDATA eventFlags)
{
	omr_error_t retVal = OMR_ERROR_NONE;
	PORT_ACCESS_FROM_JAVAVM(vm);

	const UDATA hookFlags = J9RAS_DUMP_ON_ANY & ~J9RAS_DUMP_ON_GP_FAULT & ~J9RAS_DUMP_ON_USER_SIGNAL;

	if (eventFlags & hookFlags) {
		J9HookInterface** vmHooks = vm->internalVMFunctions->getVMHookInterface(vm);
		J9HookInterface** gcOmrHooks = vm->memoryManagerFunctions ? vm->memoryManagerFunctions->j9gc_get_omr_hook_interface(vm->omrVM) : NULL;

		UDATA* postponeHooks = GLOBAL_DATA(rasDumpPostponeHooks);
		UDATA* pendingHooks = GLOBAL_DATA(rasDumpPendingHooks);
		UDATA* unhooked = GLOBAL_DATA(rasDumpUnhookedEvents);
		UDATA skip;

		IDATA rc = 0;

		/* Exclude and record any hooks that should be postponed */
		skip = eventFlags & *postponeHooks;
		eventFlags -= skip;
		*pendingHooks |= skip;

		/* Exclude already hooked events */
		eventFlags &= *unhooked;

		if (eventFlags & J9RAS_DUMP_ON_VM_STARTUP) {
			rc = (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_INITIALIZED, rasDumpHookVmInit, OMR_GET_CALLSITE(), NULL);
		}
		if (eventFlags & J9RAS_DUMP_ON_VM_SHUTDOWN) {
			rc = (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_SHUTTING_DOWN, rasDumpHookVmShutdown, OMR_GET_CALLSITE(), NULL);
		}
		if (eventFlags & J9RAS_DUMP_ON_CLASS_LOAD) {
			rc = (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASS_LOAD, rasDumpHookClassLoad, OMR_GET_CALLSITE(), NULL);
		}
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		if (eventFlags & J9RAS_DUMP_ON_CLASS_UNLOAD) {
			rc = (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASSES_UNLOAD, rasDumpHookClassesUnload, OMR_GET_CALLSITE(), NULL);
		}
#endif
		if (eventFlags & J9RAS_DUMP_ON_EXCEPTION_SYSTHROW) {
			rc = (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_EXCEPTION_SYSTHROW, rasDumpHookExceptionSysthrow, OMR_GET_CALLSITE(), NULL);
		}
		if (eventFlags & J9RAS_DUMP_ON_EXCEPTION_THROW) {
			rc = (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_EXCEPTION_THROW, rasDumpHookExceptionThrow, OMR_GET_CALLSITE(), NULL);
		}
		if (eventFlags & J9RAS_DUMP_ON_EXCEPTION_CATCH) {
			rc = (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_EXCEPTION_CATCH, rasDumpHookExceptionCatch, OMR_GET_CALLSITE(), NULL);
		}
		if (eventFlags & J9RAS_DUMP_ON_THREAD_START) {
			rc = (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_THREAD_STARTED, rasDumpHookThreadStart, OMR_GET_CALLSITE(), NULL);
		}
		if (eventFlags & J9RAS_DUMP_ON_THREAD_BLOCKED) {
			rc = (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_MONITOR_CONTENDED_ENTER, rasDumpHookMonitorContendedEnter, OMR_GET_CALLSITE(), NULL);
		}
		if (eventFlags & J9RAS_DUMP_ON_THREAD_END) {
			rc = (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_THREAD_END, rasDumpHookThreadEnd, OMR_GET_CALLSITE(), NULL);
		}
		if (eventFlags & J9RAS_DUMP_ON_GLOBAL_GC) {
			rc = (*gcOmrHooks)->J9HookRegisterWithCallSite(gcOmrHooks, J9HOOK_MM_OMR_GLOBAL_GC_START, rasDumpHookGlobalGcStart, OMR_GET_CALLSITE(), NULL);
		}
		if (eventFlags & J9RAS_DUMP_ON_EXCEPTION_DESCRIBE) {
			rc = (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_EXCEPTION_DESCRIBE, rasDumpHookExceptionDescribe, OMR_GET_CALLSITE(), NULL);
		}
		if (eventFlags & J9RAS_DUMP_ON_SLOW_EXCLUSIVE_ENTER) {
			/* wouldn't this event be better handled by a tracepoint? */
			rc = (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_SLOW_EXCLUSIVE, rasDumpHookSlowExclusive, OMR_GET_CALLSITE(), NULL);
		}
		if (eventFlags & J9RAS_DUMP_ON_OBJECT_ALLOCATION) {
			rc = (*gcOmrHooks)->J9HookRegisterWithCallSite(gcOmrHooks, J9HOOK_MM_OMR_INITIALIZED, rasDumpHookGCInitialized, OMR_GET_CALLSITE(), NULL);
			rc = (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_OBJECT_ALLOCATE_WITHIN_THRESHOLD, rasDumpHookAllocationThreshold, OMR_GET_CALLSITE(), NULL);
		}
		if (eventFlags & J9RAS_DUMP_ON_CORRUPT_CACHE) {
			rc = (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CORRUPT_CACHE, rasDumpHookCorruptCache, OMR_GET_CALLSITE(), NULL);
		}
		if (eventFlags & J9RAS_DUMP_ON_EXCESSIVE_GC) {
			rc = (*gcOmrHooks)->J9HookRegisterWithCallSite(gcOmrHooks, J9HOOK_MM_OMR_EXCESSIVEGC_RAISED, rasDumpHookExcessiveGC, OMR_GET_CALLSITE(), NULL);
		}
		if ( rc == -1 ) {
			j9nls_printf(PORTLIB, J9NLS_WARNING | J9NLS_STDERR, J9NLS_DMP_HOOK_IS_DISABLED_STR);
		}

		/* Map hook error code to OMR error code */
		switch (rc) {
		case 0:
			retVal = OMR_ERROR_NONE;
			break;
		case J9HOOK_ERR_NOMEM:
			retVal = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			break;
		default:
			retVal = OMR_ERROR_INTERNAL;
			break;
		}
	}
	return retVal;
}


/**
 * rasDumpFlushHooks() - enable hooks for events that were postponed from the initial dump agent 
 * initialization. There are now two phases: GC event hooks are enabled at TRACE_ENGINE_INITIALIZED,
 * and thread event hooks are enabled at VM_INITIALIZATION_COMPLETE. See CMVC 199853 and CMVC 200360.
 * 
 * @param[in] vm - pointer to J9JavaVM structure
 * @param[in] stage - VM initialization stage
 * @return void
 */
void 
rasDumpFlushHooks(J9JavaVM *vm, IDATA stage)
{
	UDATA* postponeHooks = GLOBAL_DATA(rasDumpPostponeHooks);
	UDATA* pendingHooks = GLOBAL_DATA(rasDumpPendingHooks);
	UDATA eventFlags;

	if (stage == TRACE_ENGINE_INITIALIZED) {
		/* Intermediate stage, enable all remaining hooks except the thread event ones */
		*postponeHooks = J9RAS_DUMP_ON_THREAD_START | J9RAS_DUMP_ON_THREAD_BLOCKED | J9RAS_DUMP_ON_THREAD_END;
	} else {
		/* Final stage, enable all remaining hooks */
		*postponeHooks = 0;
	}

	if (*pendingHooks) {
		/* There are pending hook registrations, enable them now */
		eventFlags = *pendingHooks;
		*pendingHooks = 0;
		rasDumpEnableHooks(vm, eventFlags);
	}
}

static void
rasDumpHookVmInit(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	J9VMInitEvent *data = eventData;
	J9VMThread* vmThread = data->vmThread;

	vmThread->javaVM->j9rasDumpFunctions->triggerDumpAgents(
		vmThread->javaVM, 
		vmThread, 
		J9RAS_DUMP_ON_VM_STARTUP, 
		NULL);
}


static void
rasDumpHookVmShutdown(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	J9RASdumpEventData dumpData;
	J9VMShutdownEvent *data = eventData;
	J9VMThread* vmThread = data->vmThread;
	UDATA length;
	char details[32];
	PORT_ACCESS_FROM_VMC(vmThread);

	length = j9str_printf(PORTLIB, details, sizeof(details), "#%0*zx", sizeof(UDATA) * 2, data->exitCode);

	dumpData.detailLength = length;
	dumpData.detailData = details;
	dumpData.exceptionRef = NULL;

	vmThread->javaVM->j9rasDumpFunctions->triggerDumpAgents(
		vmThread->javaVM, 
		vmThread, 
		J9RAS_DUMP_ON_VM_SHUTDOWN, 
		&dumpData);
}


static void
rasDumpHookClassLoad(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	J9RASdumpEventData dumpData;
	J9VMClassLoadEvent *data = eventData;
	J9VMThread* vmThread = data->currentThread;
	J9Class* clazz = data->clazz;
	J9UTF8* className = J9ROMCLASS_CLASSNAME(clazz->romClass);

	dumpData.detailLength = J9UTF8_LENGTH(className);
	dumpData.detailData = (char *)J9UTF8_DATA(className);
	dumpData.exceptionRef = NULL;

	vmThread->javaVM->j9rasDumpFunctions->triggerDumpAgents(
		vmThread->javaVM, 
		vmThread, 
		J9RAS_DUMP_ON_CLASS_LOAD, 
		&dumpData);
}



static void
rasDumpHookExceptionThrow(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	J9RASdumpEventData dumpData;
	J9VMExceptionThrowEvent *data = eventData;
	J9VMThread* vmThread = data->currentThread;
	J9JavaVM * vm = vmThread->javaVM;
	j9object_t exception = data->exception;
	jobject globalRef;

	globalRef = vm->internalVMFunctions->j9jni_createGlobalRef((JNIEnv *) vmThread, exception, JNI_FALSE);
	if (globalRef != NULL) {
		J9UTF8* className = J9ROMCLASS_CLASSNAME(J9OBJECT_CLAZZ(vmThread, exception)->romClass);

		dumpData.detailLength = J9UTF8_LENGTH(className);
		dumpData.detailData = (char *)J9UTF8_DATA(className);
		dumpData.exceptionRef = (j9object_t*) globalRef;

		vm->j9rasDumpFunctions->triggerDumpAgents(
			vm, 
			vmThread, 
			J9RAS_DUMP_ON_EXCEPTION_THROW, 
			&dumpData);

		data->exception = *((j9object_t*) globalRef);
		vm->internalVMFunctions->j9jni_deleteGlobalRef((JNIEnv *) vmThread, globalRef, JNI_FALSE);
	}
}


static void
rasDumpHookExceptionCatch(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	J9RASdumpEventData dumpData;
	J9VMExceptionCatchEvent *data = eventData;
	J9VMThread* vmThread = data->currentThread;
	J9JavaVM * vm = vmThread->javaVM;
	j9object_t exception = data->exception;
	jobject globalRef;

	globalRef = vm->internalVMFunctions->j9jni_createGlobalRef((JNIEnv *) vmThread, exception, JNI_FALSE);
	if (globalRef != NULL) {
		J9UTF8* className = J9ROMCLASS_CLASSNAME(J9OBJECT_CLAZZ(vmThread, exception)->romClass);

		dumpData.detailLength = J9UTF8_LENGTH(className);
		dumpData.detailData = (char *)J9UTF8_DATA(className);
		dumpData.exceptionRef = (j9object_t*) globalRef;

		vm->j9rasDumpFunctions->triggerDumpAgents(
			vm, 
			vmThread, 
			J9RAS_DUMP_ON_EXCEPTION_CATCH, 
			&dumpData);

		data->exception = *((j9object_t*) globalRef);
		vm->internalVMFunctions->j9jni_deleteGlobalRef((JNIEnv *) vmThread, globalRef, JNI_FALSE);
	}
}


static void
rasDumpHookThreadStart(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	J9VMThreadStartedEvent *data = eventData;
	J9VMThread* vmThread = data->currentThread;

	vmThread->javaVM->j9rasDumpFunctions->triggerDumpAgents(
		vmThread->javaVM, 
		vmThread, 
		J9RAS_DUMP_ON_THREAD_START, 
		NULL);
}


static void
rasDumpHookThreadEnd(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	J9VMThreadEndEvent *data = eventData;
	J9VMThread* vmThread = data->currentThread;

	vmThread->javaVM->j9rasDumpFunctions->triggerDumpAgents(
		vmThread->javaVM, 
		vmThread, 
		J9RAS_DUMP_ON_THREAD_END, 
		NULL);
}


static void
rasDumpHookMonitorContendedEnter(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	J9VMMonitorContendedEnterEvent *data = eventData;
	J9VMThread* vmThread = data->currentThread;

	vmThread->javaVM->j9rasDumpFunctions->triggerDumpAgents(
		vmThread->javaVM, 
		vmThread, 
		J9RAS_DUMP_ON_THREAD_BLOCKED, 
		NULL);
}


static void
rasDumpHookExceptionDescribe(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	J9RASdumpEventData dumpData;
	J9VMExceptionDescribeEvent *data = eventData;
	J9VMThread* vmThread = data->currentThread;
	J9JavaVM * vm = vmThread->javaVM;
	j9object_t exception = data->exception;
	jobject globalRef;

	globalRef = vm->internalVMFunctions->j9jni_createGlobalRef((JNIEnv *) vmThread, exception, JNI_FALSE);
	if (globalRef != NULL) {
		J9UTF8* className = J9ROMCLASS_CLASSNAME(J9OBJECT_CLAZZ(vmThread, exception)->romClass);

		dumpData.detailLength = J9UTF8_LENGTH(className);
		dumpData.detailData = (char *)J9UTF8_DATA(className);
		dumpData.exceptionRef = (j9object_t*) globalRef;

		vm->j9rasDumpFunctions->triggerDumpAgents(
			vm,
			vmThread, 
			J9RAS_DUMP_ON_EXCEPTION_DESCRIBE, 
			&dumpData);

		data->exception = *((j9object_t*) globalRef);
		vm->internalVMFunctions->j9jni_deleteGlobalRef((JNIEnv *) vmThread, globalRef, JNI_FALSE);
	}
}


static void
rasDumpHookGCInitialized(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	MM_InitializedEvent *data = eventData;
	J9VMThread* vmThread = (J9VMThread*) data->currentThread->_language_vmthread;
	J9JavaVM *vm = vmThread->javaVM;
	RasDumpGlobalStorage *dumpGlobal = vm->j9rasdumpGlobalStorage;
	
	setAllocationThreshold(vmThread, dumpGlobal->allocationRangeMin, dumpGlobal->allocationRangeMax);
}


static void
rasDumpHookAllocationThreshold(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	J9RASdumpEventData dumpData;
	J9VMAllocationThresholdEvent *data = eventData;
	J9VMThread* vmThread = data->currentThread;
	UDATA length;
	char details[1024];
	PORT_ACCESS_FROM_VMC(vmThread);
	J9Class* clazz = J9OBJECT_CLAZZ(vmThread, data->object);
	J9ROMClass* romClass = clazz->romClass;
	char * p;
	UDATA hadVMAccess;

	hadVMAccess = pushEventFrame(vmThread, TRUE, 0);
	PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, data->object);

	if (J9ROMCLASS_IS_ARRAY(romClass)) {
		J9ArrayClass* arrayClass = (J9ArrayClass*)clazz;
		J9Class* leafClass = arrayClass->leafComponentType;
		J9UTF8* leafClassName = J9ROMCLASS_CLASSNAME(leafClass->romClass);
		UDATA i;

		/* For arrays we print the name of the leaf class then add enough '[]' to describe the arity */

		length = j9str_printf(PORTLIB, details, sizeof(details), "%zu bytes, type %.*s", data->size, J9UTF8_LENGTH(leafClassName), J9UTF8_DATA(leafClassName));

		for (i=0;i<arrayClass->arity;i++) {
			length += j9str_printf(PORTLIB, details + length, sizeof(details) - length, "[]");
		}
	} else {
		J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);

		/* For regular objects, we just print the name from the ROMclass */

		length = j9str_printf(PORTLIB, details, sizeof(details), "%zu bytes, type %.*s", data->size, J9UTF8_LENGTH(className), J9UTF8_DATA(className));
	}

	/* Change any /s to .s so class names are in customer-friendly format */
	p = details;

	while(*p && p < (details + sizeof(details))) {
		if (*p == '/') {
			*p = '.';
		}
		p++;
	}

	dumpData.detailLength = length;
	dumpData.detailData = details;
	dumpData.exceptionRef = NULL;

	vmThread->javaVM->j9rasDumpFunctions->triggerDumpAgents(
		vmThread->javaVM, 
		vmThread, 
		J9RAS_DUMP_ON_OBJECT_ALLOCATION, 
		&dumpData);

	data->object = POP_OBJECT_IN_SPECIAL_FRAME(vmThread);
	popEventFrame(vmThread, hadVMAccess);
}


static void
rasDumpHookSlowExclusive(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	J9RASdumpEventData dumpData;
	J9VMSlowExclusiveEvent *data = eventData;
	J9VMThread* vmThread = data->currentThread;
	UDATA length;
	char details[32];
	PORT_ACCESS_FROM_VMC(vmThread);

	length = j9str_printf(PORTLIB, details, sizeof(details), "%zums", data->timeTaken);

	dumpData.detailLength = length;
	dumpData.detailData = details;
	dumpData.exceptionRef = NULL;

	vmThread->javaVM->j9rasDumpFunctions->triggerDumpAgents(
		vmThread->javaVM, 
		vmThread, 
		J9RAS_DUMP_ON_SLOW_EXCLUSIVE_ENTER, 
		&dumpData);
}


static void
rasDumpHookGlobalGcStart(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	MM_GlobalGCStartEvent *data = eventData;
	J9VMThread* vmThread = (J9VMThread *)data->currentThread->_language_vmthread;

	vmThread->javaVM->j9rasDumpFunctions->triggerDumpAgents(
		vmThread->javaVM, 
		vmThread, 
		J9RAS_DUMP_ON_GLOBAL_GC, 
		NULL);
}


#if (defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)) 
static void
rasDumpHookClassesUnload(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	J9VMClassesUnloadEvent *data = eventData;
	J9VMThread* vmThread = data->currentThread;

	vmThread->javaVM->j9rasDumpFunctions->triggerDumpAgents(
		vmThread->javaVM, 
		vmThread, 
		J9RAS_DUMP_ON_CLASS_UNLOAD, 
		NULL);
}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

#define CDEV_CURRENT_FUNCTION rasDumpHookExceptionSysthrow
static void
rasDumpHookExceptionSysthrow(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	J9RASdumpEventData dumpData;
	J9VMExceptionThrowEvent *data = eventData;
	J9VMThread* vmThread = data->currentThread;
	J9JavaVM * vm = vmThread->javaVM;
	J9Object* exception = data->exception;
	jobject globalRef;
 
	globalRef = vm->internalVMFunctions->j9jni_createGlobalRef((JNIEnv *) vmThread, exception, JNI_FALSE);
	if (globalRef != NULL) {
		J9UTF8* className = J9ROMCLASS_CLASSNAME(J9OBJECT_CLAZZ(vmThread, exception)->romClass);
 
		dumpData.detailLength = J9UTF8_LENGTH(className);
		dumpData.detailData = (char *)J9UTF8_DATA(className);
		dumpData.exceptionRef = (J9Object **) globalRef;
 
		vm->j9rasDumpFunctions->triggerDumpAgents(
				vm, 
				vmThread, 
				J9RAS_DUMP_ON_EXCEPTION_SYSTHROW, 
				&dumpData);
 
		data->exception = *((J9Object **) globalRef);
		vm->internalVMFunctions->j9jni_deleteGlobalRef((JNIEnv *) vmThread, globalRef, JNI_FALSE);
	}
}
 
#undef CDEV_CURRENT_FUNCTION

static void
rasDumpHookCorruptCache(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	J9VMCorruptCache *data = eventData;
	J9VMThread* vmThread = data->vmThread;

	vmThread->javaVM->j9rasDumpFunctions->triggerDumpAgents(
		vmThread->javaVM,
		vmThread,
		J9RAS_DUMP_ON_CORRUPT_CACHE,
		NULL);
}

/**
 * rasDumpHookExcessiveGC() Hook registration function for excessive GC event. If the excessivegc
 * dump event is triggered, this calls into the RAS dump support to generate the required dumps.
 *
 * @param hookInterface[in] pointer to the hook interface function table
 * @param eventNum[in] hook event number (not used)
 * @param eventData[in] pointer to GC event data structure, specific to the excessive GC event
 * @param userData[in] pointer to additional callee data (not used)
 */
static void
rasDumpHookExcessiveGC(J9HookInterface** hookInterface, UDATA eventNum, void* eventData, void* userData)
{
	MM_ExcessiveGCRaisedEvent *data = eventData;
	J9VMThread* vmThread = (J9VMThread*) data->currentThread->_language_vmthread;

	vmThread->javaVM->j9rasDumpFunctions->triggerDumpAgents(
		vmThread->javaVM,
		vmThread,
		J9RAS_DUMP_ON_EXCESSIVE_GC,
		NULL);
}
