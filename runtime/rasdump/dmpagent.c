/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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


/*
 * Note: remove this when portlib supports launching of executables
 */
#ifdef WIN32
#include <windows.h>
#endif
#ifdef J9ZOS390
#include <spawn.h>
#include <errno.h>
#include "atoe.h"
#endif
#ifdef AIXPPC
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#endif
#ifdef LINUX
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include "ut_j9dmp.h"

#include "dmpsup.h"
#include "j9dmpnls.h"
#include <string.h>
#include <limits.h>
#include "ute.h"
#include "jvminit.h"

#include "rasdump_internal.h"
#include "j9dump.h"
#include "omrthread.h"


/* Possible command line actions */
enum
{
	CREATE_DUMP_AGENT       = 0x01,
	UPDATE_DUMP_SETTINGS    = 0x02,
	BOGUS_DUMP_OPTION       = 0x04,
	REMOVE_DUMP_AGENTS      = 0x08,
};

/* Describe known event */
typedef struct J9RASdumpEvent
{
	char *name;
	char *detail;
	UDATA bits;
}\
J9RASdumpEvent;

/* Describe known VM request */
typedef struct J9RASdumpRequest
{
	char *name;
	char *detail;
	UDATA bits;
}\
J9RASdumpRequest;

/* Dump settings */
typedef struct J9RASdumpSettings
{
	UDATA eventMask;
	char *detailFilter;
	UDATA startOnCount;
	UDATA stopOnCount;
	char *labelTemplate;
	char *dumpOptions;
	UDATA priority;
	UDATA requestMask;
	char *subFilter;
}\
J9RASdumpSettings;

/* String cache */
typedef struct J9RASdumpStrings
{
	char **table;
	U_32 length;
	U_32 capacity;
	U_32 vmCount;
}\
J9RASdumpStrings;

/* Describe known dump specifications */
typedef struct J9RASdumpSpec
{
	char *name;
	char *summary;
	char *labelTag;
	char *labelHint;
	char *labelDescription;
	J9RASdumpFn dumpFn;
	J9RASdumpSettings settings;
}\
J9RASdumpSpec;

typedef struct J9RASprotectedDumpData
{
	J9RASdumpAgent *agent;
	char *label;
	J9RASdumpContext *context;
}\
J9RASprotectedDumpData;

/* Known dump events */
static const J9RASdumpEvent rasDumpEvents[] =
{
	{ "gpf",         "ON_GP_FAULT",             J9RAS_DUMP_ON_GP_FAULT },
	{ "user",        "ON_USER_SIGNAL",          J9RAS_DUMP_ON_USER_SIGNAL },
	{ "abort",       "ON_ABORT_SIGNAL",         J9RAS_DUMP_ON_ABORT_SIGNAL },
	{ "vmstart",     "ON_VM_STARTUP",           J9RAS_DUMP_ON_VM_STARTUP },
	{ "vmstop",      "ON_VM_SHUTDOWN",          J9RAS_DUMP_ON_VM_SHUTDOWN },
	{ "load",        "ON_CLASS_LOAD",           J9RAS_DUMP_ON_CLASS_LOAD },
	{ "unload",      "ON_CLASS_UNLOAD",         J9RAS_DUMP_ON_CLASS_UNLOAD },
	{ "throw",       "ON_EXCEPTION_THROW",      J9RAS_DUMP_ON_EXCEPTION_THROW },
	{ "catch",       "ON_EXCEPTION_CATCH",      J9RAS_DUMP_ON_EXCEPTION_CATCH },
	{ "thrstart",    "ON_THREAD_START",         J9RAS_DUMP_ON_THREAD_START },
	{ "blocked",     "ON_THREAD_BLOCKED",       J9RAS_DUMP_ON_THREAD_BLOCKED },
	{ "thrstop",     "ON_THREAD_END",           J9RAS_DUMP_ON_THREAD_END },
	{ "fullgc",      "ON_GLOBAL_GC",            J9RAS_DUMP_ON_GLOBAL_GC },
	{ "uncaught",    "ON_EXCEPTION_DESCRIBE",   J9RAS_DUMP_ON_EXCEPTION_DESCRIBE },
	{ "slow",        "ON_SLOW_EXCLUSIVE_ENTER", J9RAS_DUMP_ON_SLOW_EXCLUSIVE_ENTER },
	{ "systhrow",    "ON_EXCEPTION_SYSTHROW",   J9RAS_DUMP_ON_EXCEPTION_SYSTHROW },
	{ "traceassert", "ON_TRACE_ASSERT",         J9RAS_DUMP_ON_TRACE_ASSERT },
	/* J9RAS_DUMP_ON_USER_REQUEST cannot be triggered via the command-line */
	{ "allocation",  "ON_OBJECT_ALLOCATION",    J9RAS_DUMP_ON_OBJECT_ALLOCATION },
	{ "corruptcache","ON_CORRUPT_CACHE",        J9RAS_DUMP_ON_CORRUPT_CACHE },
	{ "excessivegc", "ON_EXCESSIVE_GC",         J9RAS_DUMP_ON_EXCESSIVE_GC },
};
#define J9RAS_DUMP_KNOWN_EVENTS  ( sizeof(rasDumpEvents) / sizeof(J9RASdumpEvent) )

/* Known VM requests */
static const J9RASdumpRequest rasDumpRequests[] =
{
	{ "exclusive", "DO_EXCLUSIVE_VM_ACCESS",     J9RAS_DUMP_DO_EXCLUSIVE_VM_ACCESS },
	{ "compact",   "DO_COMPACT_HEAP",            J9RAS_DUMP_DO_COMPACT_HEAP },
	{ "prepwalk",  "DO_PREPARE_HEAP_FOR_WALK",   J9RAS_DUMP_DO_PREPARE_HEAP_FOR_WALK },
	{ "serial",    "DO_SUSPEND_OTHER_DUMPS",     J9RAS_DUMP_DO_SUSPEND_OTHER_DUMPS },
	{ "attach",    "DO_ATTACH_THREAD",           J9RAS_DUMP_DO_ATTACH_THREAD },
	{ "preempt",   "DO_PREEMPT_THREADS",         J9RAS_DUMP_DO_PREEMPT_THREADS }
};

#define J9RAS_DUMP_KNOWN_REQUESTS  ( sizeof(rasDumpRequests) / sizeof(J9RASdumpRequest) )

static void updatePercentLastToken(J9JavaVM *vm, char *label);
static omr_error_t makePath (J9JavaVM *vm, char *label);
static char* allocString (J9JavaVM *vm, UDATA numBytes);
static omr_error_t mergeAgent (J9JavaVM *vm, J9RASdumpAgent *agent, const J9RASdumpSettings *settings);
static IDATA fixDumpLabel (J9JavaVM *vm, const J9RASdumpSpec *spec, char **labelPtr, IDATA newLabel);
static UDATA processSettings (J9JavaVM *vm, IDATA kind, char *optionString, J9RASdumpSettings *settings);
static J9RASdumpAgent* findAgent (J9JavaVM *vm, IDATA kind, const J9RASdumpSettings *settings);
static char* scanString (J9JavaVM *vm, const char **cursor);
#if defined(J9ZOS390)
static omr_error_t doCEEDump (J9RASdumpAgent *agent, char *label, J9RASdumpContext *context);
#endif /* J9ZOS390 */
static omr_error_t doConsoleDump (J9RASdumpAgent *agent, char *label, J9RASdumpContext *context);
static omr_error_t doStackDump (J9RASdumpAgent *agent, char *label, J9RASdumpContext *context);
omr_error_t doSilentDump(J9RASdumpAgent *agent, char *label, J9RASdumpContext *context);
static J9RASdumpAgent* createAgent (J9JavaVM *vm, IDATA kind, const J9RASdumpSettings *settings);
#if (defined(J9VM_RAS_DUMP_AGENTS)) 
static UDATA protectedDumpFunction (struct J9PortLibrary *portLibrary, void *userData);
#endif /* J9VM_RAS_DUMP_AGENTS */
static omr_error_t freeAgent (J9JavaVM *vm, J9RASdumpAgent **agentPtr);
#if (defined(J9VM_RAS_DUMP_AGENTS)) 
static UDATA signalHandler (struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData);
#endif /* J9VM_RAS_DUMP_AGENTS */
static UDATA scanRequests (J9JavaVM *vm, char **cursor, UDATA *actionPtr);
static omr_error_t doJavaDump (J9RASdumpAgent *agent, char *label, J9RASdumpContext *context);
static UDATA scanEvents (J9JavaVM *vm, char **cursor, UDATA *actionPtr);
extern UDATA parseAllocationRange(char *range, UDATA *min, UDATA *max);
static char * scanFilter(J9JavaVM *vm, const J9RASdumpSettings *settings, const char **cursor, UDATA *actionPtr);
omr_error_t doSystemDump (J9RASdumpAgent *agent, char *label, J9RASdumpContext *context);
omr_error_t doHeapDump (J9RASdumpAgent *agent, char *label, J9RASdumpContext *context);
static omr_error_t doSnapDump (J9RASdumpAgent *agent, char *label, J9RASdumpContext *context);
static char scanSign (char **cursor);
omr_error_t doToolDump (J9RASdumpAgent *agent, char *label, J9RASdumpContext *context);
static omr_error_t doJitDump(J9RASdumpAgent *agent, char *label, J9RASdumpContext *context);
static char * scanSubFilter(J9JavaVM *vm, const J9RASdumpSettings *settings, const char **cursor, UDATA *actionPtr);


/* Known dump specifications */
static const J9RASdumpSpec rasDumpSpecs[] =
{
	{
		"console",
		"Basic thread dump to stderr",
		"file=",
		NULL,
		"Output file",
		doConsoleDump,
		{ J9RAS_DUMP_ON_GP_FAULT | J9RAS_DUMP_ON_USER_SIGNAL,
		  NULL,
		  1, 0,
		  NULL,
		  NULL,
		  5,
		  J9RAS_DUMP_DO_EXCLUSIVE_VM_ACCESS, 
		  NULL }
	},
	{
		"stack",
		"Print thread information to stderr",
		"file=",
		NULL,
		"Output file",
		doStackDump,
		{ J9RAS_DUMP_ON_GP_FAULT | J9RAS_DUMP_ON_USER_SIGNAL,
		  NULL,
		  1, 0,
		  NULL,
		  NULL,
		  5,
		  J9RAS_DUMP_DO_EXCLUSIVE_VM_ACCESS,
		  NULL }
	},
	{
		"system",
		"Capture raw process image",
#if defined(J9ZOS390)
		"dsn=",
		"JAVA_DUMP_TDUMP_PATTERN",
		" Output name",
#else
		"file=",
		"IBM_COREDIR",
		"Output file",
#endif
		doSystemDump,
		{ J9RAS_DUMP_ON_GP_FAULT | J9RAS_DUMP_ON_ABORT_SIGNAL |
				J9RAS_DUMP_ON_TRACE_ASSERT|J9RAS_DUMP_ON_CORRUPT_CACHE,
		  NULL,
		  1, 0,
#if defined(J9ZOS390)
#if defined(J9ZOS39064)
		  "%uid.JVM.%job.D%y%m%d.T%H" "%M" "%S.X&DS",
		  NULL,
#else
		  "%uid.JVM.TDUMP.%job.D%y%m%d.T%H" "%M" "%S",
		  NULL,		  
#endif
#else
		  "core.%Y" "%m%d.%H" "%M" "%S.%pid.%seq.dmp",
		  NULL,
#endif
		  999,
		  J9RAS_DUMP_DO_SUSPEND_OTHER_DUMPS,
		  NULL }
	},
	{
		"tool",
		"Run command line program",
		"exec=",
		"JAVA_DUMP_TOOL",
		"Command line (special characters should be escaped)",
		doToolDump,
		{ J9RAS_DUMP_ON_GP_FAULT | J9RAS_DUMP_ON_USER_SIGNAL,
		  NULL,
		  1, 1,
#if defined(WIN32)
		  "windbg -p %pid -c \".setdll %vmbin\\j9windbg\"",
#elif defined(LINUX)
		  "gdb -p %pid",
#elif defined(AIXPPC)
		  "dbx -a %pid",
#elif defined(J9ZOS390)
		  "dbx -a %pid",
#else
		  NULL,
#endif
		  NULL,
		  0,
		  J9RAS_DUMP_DO_SUSPEND_OTHER_DUMPS,
		  NULL }
	},
	{
		"java",
		"Write application summary",
		"file=",
#if defined(J9ZOS390)
		"_CEE_DMPTARG",
#else
		"IBM_JAVACOREDIR",
#endif
		"Output file",
		doJavaDump,
		{ J9RAS_DUMP_ON_GP_FAULT | J9RAS_DUMP_ON_USER_SIGNAL | J9RAS_DUMP_ON_ABORT_SIGNAL |
				J9RAS_DUMP_ON_TRACE_ASSERT|J9RAS_DUMP_ON_CORRUPT_CACHE,
		  NULL,
		  1, 0,
		  "javacore.%Y" "%m%d.%H" "%M" "%S.%pid.%seq.txt",
		  NULL,
		  400,

		  J9RAS_DUMP_DO_EXCLUSIVE_VM_ACCESS
#ifndef J9ZOS390
			| J9RAS_DUMP_DO_PREEMPT_THREADS
#endif
		  , NULL
		}
	},
	{
		"heap",
		"Capture raw heap image",
		"file=",
#if defined(J9ZOS390)
		"_CEE_DMPTARG",
#else
		"IBM_HEAPDUMPDIR",
#endif
		"Output file",
		doHeapDump,
		{ J9RAS_DUMP_ON_GP_FAULT | J9RAS_DUMP_ON_USER_SIGNAL,
		  NULL,
		  1, 0,
		  "heapdump.%Y" "%m%d.%H" "%M" "%S.%pid.%seq.phd",
		  "PHD",
		  500,
		  J9RAS_DUMP_DO_EXCLUSIVE_VM_ACCESS | J9RAS_DUMP_DO_PREPARE_HEAP_FOR_WALK | J9RAS_DUMP_DO_COMPACT_HEAP,
		  NULL }
	},
	{
		"snap",
		"Take a snap of the trace buffers",
		"file=",
#if defined(J9ZOS390)
		"_CEE_DMPTARG",
#else
		"IBM_COREDIR",
#endif
		"Output file",
		doSnapDump,
		{ J9RAS_DUMP_ON_GP_FAULT | J9RAS_DUMP_ON_ABORT_SIGNAL |
				J9RAS_DUMP_ON_TRACE_ASSERT | J9RAS_DUMP_ON_CORRUPT_CACHE,
		  NULL,
		  1, 0,
		  "Snap.%Y" "%m%d.%H" "%M" "%S.%pid.%seq.trc",
		  NULL,
		  300,
		 J9RAS_DUMP_DO_SUSPEND_OTHER_DUMPS,
		 NULL }
	},
#if defined(J9ZOS390)
	{
		"ceedump",
		"Take standard LE dump",
		"dsn=",
		"JAVA_DUMP_TDUMP_PATTERN",
		" Output name",
		doCEEDump,
		{ J9RAS_DUMP_ON_GP_FAULT | J9RAS_DUMP_ON_USER_SIGNAL,
		  NULL,
		  1, 0,
		  "%uid.JVM.CEEDUMP.%job.D%y%m%d.T%H" "%M" "%S",
		  NULL,
		  888,
		  J9RAS_DUMP_DO_SUSPEND_OTHER_DUMPS,
		  NULL }
	},
#endif
	{
		"jit",
		"Capture JIT diagnostic information",
		"file=",
#if defined(J9ZOS390)
		"_CEE_DMPTARG",
#else
		"IBM_COREDIR",
#endif
		"Output file",
		doJitDump,
		{ J9RAS_DUMP_ON_GP_FAULT | J9RAS_DUMP_ON_ABORT_SIGNAL,
		  NULL,
		  1, 0,
		  "jitdump.%Y" "%m%d.%H" "%M" "%S.%pid.%seq.dmp",
		  NULL,
		  200,
		  J9RAS_DUMP_DO_SUSPEND_OTHER_DUMPS,
		  NULL }
	},
	{
		"silent",
		"Dummy dump agent which does nothing",
		"file=",
		NULL,
		"Output file",
		doSilentDump,
		{ 0,
		  NULL,
		  1, 0,
		  NULL,
		  NULL,
		  5,
		  0,
		  NULL }
	}
};

#define J9RAS_DUMP_KNOWN_SPECS  ( sizeof(rasDumpSpecs) / sizeof(J9RASdumpSpec) )
const UDATA j9RasDumpKnownSpecs = J9RAS_DUMP_KNOWN_SPECS;


/* Shared strings (to avoid bloat during option processing) */
static J9RASdumpStrings rasDumpStrings = {NULL, 0, 0, 0};
static UDATA rasDumpStringLock = 0;

extern void runJavadump(char *label, J9RASdumpContext *context, J9RASdumpAgent* agent);
extern void runHeapdump(char *label, J9RASdumpContext *context, J9RASdumpAgent* agent);

static void
updatePercentLastToken(J9JavaVM *vm, char *label)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	
	struct J9StringTokens* stringTokens;

	/* write the label into the %last token */ 
	if (NULL != vm->j9rasdumpGlobalStorage) {
		RasDumpGlobalStorage* dump_storage = (RasDumpGlobalStorage*)vm->j9rasdumpGlobalStorage;

        /* lock access to the tokens */
        omrthread_monitor_enter(dump_storage->dumpLabelTokensMutex);
        stringTokens = dump_storage->dumpLabelTokens;
        
        j9str_set_token(PORTLIB, stringTokens, "last", "%s", label);
        
        /* release access to the tokens */
        omrthread_monitor_exit(dump_storage->dumpLabelTokensMutex);
	}
}

static omr_error_t
makePath(J9JavaVM *vm, char *label)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	char *cursor = label;
	char tmp_byte = '*';
	IDATA fd;

	/* No path specified. */
	if (label[0] == '\0') {
		return OMR_ERROR_INTERNAL;
	}
	/* disallow stderr */
	if (label[0] == '-' && label[1] == '\0') {
		return OMR_ERROR_INTERNAL;
	}
	/* A directory is not a valid path */
	if( label[strlen(label)-1] == DIR_SEPARATOR ) {
		return OMR_ERROR_INTERNAL;
	}

	/* Try to create suggested dump file */
	fd = j9file_open(label, EsOpenWrite | EsOpenCreateNew | EsOpenCreateNoTag, 0666);

	/* No luck - perhaps path hasn't been created... */
	if ( fd == -1 ) {
		/* attempt to create intermediate folders */
		while ( (cursor = strchr(++cursor, DIR_SEPARATOR)) != NULL ) {
			*cursor = '\0';
			j9file_mkdir(label);
			*cursor = DIR_SEPARATOR;
		}

		/* Try again */
		fd = j9file_open(label, EsOpenWrite | EsOpenCreateNew | EsOpenCreateNoTag, 0666);
	}

	/* attempt to write a byte to the file */
	if (fd >= 0) {
		if (j9file_write(fd, &tmp_byte, 1) < 0) {
			j9file_close(fd);
			j9file_unlink(label);
			fd = -1;
		}
	}

	if ( fd == -1 ) {
		RasDumpGlobalStorage *dumpGlobal = vm->j9rasdumpGlobalStorage;
		char fileName[J9_MAX_DUMP_PATH];
		char *pFileName;

		/* If failover is disabled (-Xdump:nofailover) then bail out now with an error message */
		if (dumpGlobal->noFailover) {
			j9nls_printf(PORTLIB, J9NLS_ERROR,
						J9NLS_DMP_MAKEPATH_CANNOT_USE_DMP_LABEL,
						label,
						j9error_last_error_message());
			return OMR_ERROR_INTERNAL;
		}

		/* Issue warning message and carry on to try the failover locations */
		j9nls_printf(PORTLIB, J9NLS_WARNING,
					J9NLS_DMP_MAKEPATH_CANNOT_USE_DMP_LABEL,
					label,
					j9error_last_error_message());

		/* locate and store the filename */
		pFileName = strrchr(label, DIR_SEPARATOR);
		if (NULL == pFileName) {
			strcpy(fileName, label);
		} else {
			pFileName++;
			strcpy(fileName, pFileName);
		}

		/* first option is to use TMPDIR, if set */
		if ( j9sysinfo_get_env("TMPDIR", label, J9_MAX_DUMP_PATH - strlen(fileName) - 2) == 0 ) {

			/* add filename to the path */
			strcat(label, DIR_SEPARATOR_STR);
			strcat(label, fileName);
			/* Try to create default dump file */
			fd = j9file_open(label, EsOpenWrite | EsOpenCreateNew | EsOpenCreateNoTag, 0666);
			if (fd >= 0) {
				/* attempt to write a byte to the file */
				if (j9file_write(fd, &tmp_byte, 1) < 0) {
					j9file_close(fd);
					j9file_unlink(label);
					fd = -1;
				}
			}
		}

		/* TMPDIR not writable, so default to static temp dir as this is our last option */
		if ( fd == -1 ) {
			/* add basename */
			strcpy(label, J9_TMP_DUMP_NAME);
			strcat(label, DIR_SEPARATOR_STR);
			strcat(label, fileName);
			/* Try to create default dump file */
			fd = j9file_open(label, EsOpenWrite | EsOpenCreateNew | EsOpenCreateNoTag, 0666);
			if (fd >= 0) {
				/* attempt to write a byte to the file */
				if (j9file_write(fd, &tmp_byte, 1) < 0) {
					j9file_close(fd);
					j9file_unlink(label);
					fd = -1;
				}
			}
		}
	}

	/* write the path to the target dump file into the %last token */
	updatePercentLastToken(vm, label);

	/* avoid leaking handles */
	if ( fd != -1 ) {
		j9file_close(fd);
		j9file_unlink(label);
	} else {
		return OMR_ERROR_INTERNAL;
	}

	return OMR_ERROR_NONE;
}



static omr_error_t
doConsoleDump(J9RASdumpAgent *agent, char *label, J9RASdumpContext *context)
{
	J9RASdumpQueue *queue;

	J9JavaVM *vm = context->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);

	j9tty_err_printf(PORTLIB, "-------- Console dump --------\n");

	/* Fatal dumps to stderr default to the old-style gpThreadDump */
	if ( (context->eventFlags & J9RAS_DUMP_ON_GP_FAULT) && *label == '-' && FIND_DUMP_QUEUE(vm, queue) ) {
		queue->oldFacade->triggerDumpAgents( vm, context->onThread, context->eventFlags, context->eventData );
	} else {
		J9VMThread *self = context->onThread;
		if (label[0] == '-' && label[1] == '\0') {
			/* No checks needed, output to stderr. */
		} else {
			if (makePath(vm, label) == OMR_ERROR_INTERNAL) {
				/* Nowhere available to write the dump, we are done, makePath() will have issued error message */
				return OMR_ERROR_INTERNAL;
			}
		}
		if (self == NULL){
			self = vm->mainThread;
		}
		/* Label of "-" means stderr */
		vm->internalVMFunctions->printThreadInfo(vm, self, *label == '-' ? NULL : label, TRUE);
	}

	j9tty_err_printf(PORTLIB, "\n^^^^^^^^ Console dump ^^^^^^^^\n");

	return OMR_ERROR_NONE;
}


static omr_error_t
doStackDump(J9RASdumpAgent *agent, char *label, J9RASdumpContext *context)
{
	J9JavaVM *vm = context->javaVM;
	J9VMThread *self = context->onThread;

	if (label[0] == '-' && label[1] == '\0') {
		/* No checks needed, output to stderr. */
	} else {
		if (makePath(vm, label) == OMR_ERROR_INTERNAL) {
			/* Nowhere available to write the dump, we are done, makePath() will have issued error message */
			return OMR_ERROR_INTERNAL;
		}
	}
	if (self == NULL) {
		self = vm->mainThread;
	}
	/* Label of "-" means stderr */
	vm->internalVMFunctions->printThreadInfo(vm, self, *label == '-' ? NULL : label, FALSE);

	return OMR_ERROR_NONE;
}


omr_error_t
doSilentDump(J9RASdumpAgent *agent, char *label, J9RASdumpContext *context)
{
	/* Do nothing */
	return OMR_ERROR_NONE;
}

static UDATA
protectedUpdateJ9RAS(struct J9PortLibrary *portLibrary, void *userData)
{
	J9JavaVM *vm = userData;
	J9RAS *rasStruct = vm->j9ras;
	PORT_ACCESS_FROM_PORT(portLibrary);

	/* Store the current TID in the J9RAS structure */
	if (compareAndSwapUDATA(&rasStruct->tid, 0, omrthread_get_ras_tid()) == 0) {
		/* this thread got the lock, so can safely store the current millisecond and high-resolution timers */
		rasStruct->dumpTimeMillis = j9time_current_time_millis();
		rasStruct->dumpTimeNanos = j9time_nano_time();
	}
	return 0;
}

omr_error_t
doSystemDump(J9RASdumpAgent *agent, char *label, J9RASdumpContext *context)
{
	J9JavaVM *vm = context->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);
	const char* cacheDir = NULL;
	J9RAS* rasStruct = vm->j9ras;

#if defined(J9VM_OPT_SHARED_CLASSES) && defined(LINUX)
	J9SharedClassJavacoreDataDescriptor sharedClassData;
	
	/* set up cacheDir with the Shared Classes Cache file if it is in use. */
	if ((NULL != vm->sharedClassConfig) && (NULL != vm->sharedClassConfig->getJavacoreData)) {
		if (1 == vm->sharedClassConfig->getJavacoreData(vm, &sharedClassData)) {
			/* test for memory mapped file */
			if (sharedClassData.shmid == -2) {
				cacheDir = sharedClassData.cacheDir;
			}
		}
	}
#endif

	reportDumpRequest(privatePortLibrary,context,"System",label);
	
	if ( *label != '-' ) {
		UDATA retVal;

#ifndef J9ZOS390
		if (makePath(vm, label) == OMR_ERROR_INTERNAL) {
			/* Nowhere available to write the dump, we are done, makePath() will have issued error message */
			return OMR_ERROR_INTERNAL;
		}
#endif

		/* set the tid if it is unassigned inside a sig protect, in case vm->j9ras is a bad pointer */
		j9sig_protect(
			protectedUpdateJ9RAS, vm,
			signalHandler, NULL,
			J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGALLSYNC,
			&retVal);

#ifdef J9ZOS390
		retVal = j9dump_create( label, "IEATDUMP", (void*)cacheDir );
#else
		retVal = j9dump_create( label, agent->dumpOptions, (void*)cacheDir );
#endif

		if ( retVal == 0 ) {
			if ( label[0] != '\0' ) {
#if defined(J9ZOS39064)
				j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_WRITTEN_DUMP_STR_ZOS, "System", label);
#else
				j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_WRITTEN_DUMP_STR, "System", label);
#endif
				Trc_dump_reportDumpEnd_Event2("System", label);
			} else {
				j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_WRITTEN_DUMP_STR, "System", "{unable to determine dump name}");
				Trc_dump_reportDumpEnd_Event2("System", "{unable to determine dump name}");
			}
		} else {
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_ERROR_IN_DUMP_STR, "System", label);
			Trc_dump_reportDumpError_Event2("System", label);
		}
	} else {
		j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_MISSING_FILENAME_STR);
		return OMR_ERROR_INTERNAL;
	}

	/* If this thread set the TID in the J9RAS structure, reset the timestamps and lastly reset the TID */
	if (rasStruct->tid == omrthread_get_ras_tid()) {
		rasStruct->dumpTimeMillis = 0;
		rasStruct->dumpTimeNanos = 0;
		compareAndSwapUDATA(&rasStruct->tid, omrthread_get_ras_tid(), 0);
	}

	return OMR_ERROR_NONE;
}
/*
 * Function: doToolDump - launches a tool command as specified via a -Xdump:tool agent
 * 
 * Parameters:
 *  agent [in]	 - dump agent structure
 *  label [in]	 - tool command to be executed
 *  context [in] - dump context (what triggered the dump)
 *
 * Returns: OMR_ERROR_NONE, OMR_ERROR_INTERNAL
 */
omr_error_t
doToolDump(J9RASdumpAgent *agent, char *label, J9RASdumpContext *context)
{
	J9JavaVM *vm = context->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);

	UDATA msec = 400;
	IDATA async = FALSE;

	j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_REQUESTING_DUMP_STR, "Tool", label);

	if ( agent->dumpOptions ) {
		/* Some tools take longer to run => user-specified pause time */
		char *buf = strstr(agent->dumpOptions, "WAIT");
		if ( buf ) { buf += 4; scan_udata(&buf, &msec); }

		/* Don't wait for the tool to finish? */
		async = ( strstr(agent->dumpOptions, "ASYNC") != NULL );
	}

	/* Spawn process: this really should be moved into portlib... */
	if ( *label != '-' ) {
#if defined(WIN32)
		{
			BOOL retVal;
			STARTUPINFOW sinfo;
			PROCESS_INFORMATION pinfo;
			wchar_t localUnicodePath[J9_MAX_DUMP_PATH];
			wchar_t *unicodePath = localUnicodePath;
	
			memset( &sinfo, 0, sizeof(sinfo) );
			memset( &pinfo, 0, sizeof(pinfo) );
			sinfo.cb = sizeof(sinfo);
			
			/* Copy the tool command line into a unicode string, allocating additional memory if needed */
			if (strlen(label) >= J9_MAX_DUMP_PATH) {
				unicodePath = j9mem_allocate_memory((strlen(label) + 1) * sizeof(wchar_t), OMRMEM_CATEGORY_VM);
				if (!unicodePath) {
					return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
				}
			}
			MultiByteToWideChar(OS_ENCODING_CODE_PAGE, OS_ENCODING_MB_FLAGS, label, -1, unicodePath, (int)strlen(label) + 1);
	
			retVal = CreateProcessW(NULL, unicodePath, NULL, NULL, TRUE, 0, NULL, NULL, &sinfo, &pinfo);
	
			if (retVal) {
				j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_SPAWNED_DUMP_STR, "Tool", pinfo.dwProcessId);
	
				/* Give it a chance to start */
				if (async == FALSE) {
					WaitForSingleObject(pinfo.hProcess, INFINITE);
				}
				omrthread_sleep(msec);
	
				/* Clean up unused handles */
				CloseHandle(pinfo.hThread);
				CloseHandle(pinfo.hProcess);
			} else {
				j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_ERROR_IN_DUMP_STR_RC, "Tool", "CreateProcessW()", GetLastError());
			}
			
			if (unicodePath != localUnicodePath) {
				j9mem_free_memory(unicodePath);
			}
		}
#elif (defined(LINUX) && !defined(J9ZTPF)) || defined(AIXPPC)
		{
			IDATA retVal;

			if ( (retVal = fork()) == 0 ) {
	
				retVal = execl("/bin/sh", "/bin/sh", "-c", label, NULL);
	
				j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_ERROR_IN_DUMP_STR_RC, "Tool", "execl()", errno);
				exit( (int)retVal );
	
			} else {
				j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_SPAWNED_DUMP_STR, "Tool", retVal);
	
				/* Give it a chance to start */
				if (async == FALSE) {
					waitpid(retVal, NULL, 0);
				}
				omrthread_sleep(msec);
			}
		}
#elif defined(J9ZOS390)
		{
			const char *argv[] = {"/bin/sh", "-c", NULL, NULL};
			extern const char **environ;
			IDATA retVal;
	
			struct inheritance inherit;
			memset( &inherit, 0, sizeof(inherit) );
	
			/* Set actual command */
			argv[2] = label;
	
			/* Use spawn instead of fork on z/OS */
			retVal = spawnp("/bin/sh", 0, NULL, &inherit, argv, environ);
	
			if ( retVal == -1) {
				j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_ERROR_IN_DUMP_STR_RC, "Tool", "spawnp()", errno);
			} else {
				j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_SPAWNED_DUMP_STR, "Tool", retVal);
	
				/* Give it a chance to start */
				if (async == FALSE) {
					waitpid(retVal, NULL, 0);
				}
				omrthread_sleep(msec);
			}
		}
#else
		j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_DUMP_NOT_AVAILABLE_STR, "Tool");
#endif
	} else {
		j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_MISSING_EXECUTABLE_STR);
	}

	return OMR_ERROR_NONE;
}


static omr_error_t
doJavaDump(J9RASdumpAgent *agent, char *label, J9RASdumpContext *context)
{
	J9JavaVM *vm = context->javaVM;

	if (makePath(vm, label) == OMR_ERROR_INTERNAL) {
		/* Nowhere available to write the dump, we are done, makePath() will have issued error message */
		return OMR_ERROR_INTERNAL;
	}
	runJavadump(label, context, agent);

	return OMR_ERROR_NONE;
}


omr_error_t
doHeapDump(J9RASdumpAgent *agent, char *label, J9RASdumpContext *context)
{
	J9JavaVM *vm = context->javaVM;

	if (makePath(vm, label) == OMR_ERROR_INTERNAL) {
		/* Nowhere available to write the dump, we are done, makePath() will have issued error message */
		return OMR_ERROR_INTERNAL;
	}
	runHeapdump(label, context, agent);

	return OMR_ERROR_NONE;
}



static omr_error_t
doSnapDump(J9RASdumpAgent *agent, char *label, J9RASdumpContext *context)
{
	J9JavaVM *vm = context->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);

	RasGlobalStorage * j9ras = (RasGlobalStorage *)vm->j9rasGlobalStorage;
	UtInterface * uteInterface = (UtInterface *)(j9ras ? j9ras->utIntf : NULL);

	reportDumpRequest(privatePortLibrary,context,"Snap",label);

	/* Is trace running? */
	if ( uteInterface && uteInterface->server ) {
		char *snapResponse = "";
		omr_error_t result = OMR_ERROR_NONE;
		/* USER_REQUEST is synchronous to prevent Dump API calls returning before the file is written. (USER_SIGNAL is kill -3)
		 * J9RAS_DUMP_ON_GP_FAULT, J9RAS_DUMP_ON_ABORT_SIGNAL and J9RAS_DUMP_ON_TRACE_ASSERT are synchronous to prevent us
		 * starting a thread under those conditions.
		 */
		I_32 sync = (context->eventFlags & (J9RAS_DUMP_ON_USER_REQUEST | J9RAS_DUMP_ON_GP_FAULT | J9RAS_DUMP_ON_ABORT_SIGNAL | J9RAS_DUMP_ON_TRACE_ASSERT )) != 0;

		if (makePath(vm, label) == OMR_ERROR_INTERNAL) {
			/* Nowhere available to write the dump, we are done, makePath() will have issued error message */
			return OMR_ERROR_INTERNAL;
		}

		/* Asynchronous request - may return before snap is complete */
		result = uteInterface->server->TraceSnapWithPriority(
			(context->onThread ? UT_THREAD_FROM_VM_THREAD(context->onThread) : NULL),
			label, J9THREAD_PRIORITY_MAX, &snapResponse, sync );

		if (OMR_ERROR_NONE == result) {
			j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_WRITTEN_DUMP_STR, "Snap", snapResponse);
			Trc_dump_reportDumpEnd_Event2("Snap", snapResponse);
		} else {
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_ERROR_IN_DUMP_STR, "Snap", snapResponse);
			Trc_dump_reportDumpError_Event2("Snap", snapResponse);
		}
	} else {
		j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_DUMP_NOT_AVAILABLE_STR, "Snap");
		Trc_dump_reportDumpError_Event2("Snap", "{no trace engine}");
	}

	return OMR_ERROR_NONE;
}



#ifdef J9ZOS390
static omr_error_t
doCEEDump(J9RASdumpAgent *agent, char *label, J9RASdumpContext *context)
{
	J9JavaVM *vm = context->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);

	/* For CEEDUMPs use the short form of the start dump message, with no label */
	j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_REQUESTING_DUMP_STR_NOFILE, "CEEDUMP");
	Trc_dump_reportDumpStart_Event1("CEEDUMP", label);

	if ( *label != '-' ) {
		UDATA retVal;

		/* Note that the port library updates label with the actual CEEDUMP file name */
		retVal = j9dump_create( label, "CEEDUMP", NULL );

		if ( retVal == 0 ) {
			if ( label[0] != '\0' ) {
				j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_WRITTEN_DUMP_STR, "CEEDUMP", label);
				Trc_dump_reportDumpEnd_Event2("CEEDUMP", label);
			} else {
				j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_WRITTEN_DUMP_STR, "CEEDUMP", "{unable to determine dump name}");
				Trc_dump_reportDumpEnd_Event2("CEEDUMP", "{unable to determine dump name}");
			}
		} else {
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_ERROR_IN_DUMP_STR, "CEEDUMP", label);
			Trc_dump_reportDumpError_Event2("CEEDUMP", label);
		}
	} else {
		j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_MISSING_FILENAME_STR);
	}

	return OMR_ERROR_NONE;
}


#endif

/*
 * Function: doJitDump - runs a JIT dump
 * 
 * Parameters:
 *  agent [in]	 - dump agent structure
 *  label [in]	 - tool command to be executed
 *  context [in] - dump context (what triggered the dump)
 *
 * Returns: OMR_ERROR_NONE, OMR_ERROR_INTERNAL
 */
static omr_error_t
doJitDump(J9RASdumpAgent *agent, char *label, J9RASdumpContext *context)
{
	J9JavaVM *vm = context->javaVM;
	omr_error_t result = OMR_ERROR_INTERNAL;
	
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	PORT_ACCESS_FROM_JAVAVM(vm);

	/* If the JIT configuration structure and JIT dump function pointer are set, run the JIT dump */
	J9JITConfig *jitConfig = vm->jitConfig;
	if ((jitConfig != NULL) && (jitConfig->dumpJitInfo != NULL)) {
		IDATA rc = 0;
		if (makePath(vm, label) == OMR_ERROR_INTERNAL) {
			/* Nowhere available to write the dump, we are done, makePath() will have issued error message */
			return OMR_ERROR_INTERNAL;
		}
		j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_REQUESTING_DUMP_STR, "JIT", label);
		rc = jitConfig->dumpJitInfo(vm->internalVMFunctions->currentVMThread(vm), label, context);
		if (0 == rc) {
			result = OMR_ERROR_NONE;
			j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_WRITTEN_DUMP_STR, "JIT", label);
		} else {
			result = OMR_ERROR_INTERNAL;
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_ERROR_IN_DUMP_STR, "JIT", label);
		}
	}

#endif /* J9VM_INTERP_NATIVE_SUPPORT */
	return result;
}


static char*
allocString(J9JavaVM *vm, UDATA numBytes)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	char *buf = (char *)j9mem_allocate_memory(numBytes, OMRMEM_CATEGORY_VM);

	/* Lock string table */
	while (compareAndSwapUDATA(&rasDumpStringLock, 0, 1) != 0) {
		omrthread_sleep(200);
	}

	if ( rasDumpStrings.table && rasDumpStrings.length >= rasDumpStrings.capacity ) {

		/* Heuristic: grow by 50% */
		rasDumpStrings.length = rasDumpStrings.capacity;
		rasDumpStrings.capacity += ( rasDumpStrings.length / 2 );

		rasDumpStrings.table = (char **)j9mem_reallocate_memory( rasDumpStrings.table, rasDumpStrings.capacity * sizeof(char *), OMRMEM_CATEGORY_VM );
	}

	if ( rasDumpStrings.table == NULL ) {
		rasDumpStrings.length = 0;
	} else if ( buf ) {
		rasDumpStrings.table[ rasDumpStrings.length++ ] = buf;
	}

	/* Unlock string table */
	compareAndSwapUDATA(&rasDumpStringLock, 1, 0);

	return buf;
}


static char*
scanString(J9JavaVM *vm, const char **cursor)
{
	UDATA len = strcspn(*cursor, ",");
	char *buf = NULL;

	U_32 i;

	/* Lock string table */
	while (compareAndSwapUDATA(&rasDumpStringLock, 0, 1) != 0) {
		omrthread_sleep(200);
	}

	if ( rasDumpStrings.table != NULL ) {
		for (i = 0; i < rasDumpStrings.length; i++) {
			char *ptr = rasDumpStrings.table[i];

			/* Re-use cached string! */
			if ( strlen(ptr) == len && strncmp(ptr, *cursor, len) == 0 ) {
				buf = ptr;
				break;
			}
		}
	}

	/* Unlock string table */
	compareAndSwapUDATA(&rasDumpStringLock, 1, 0);

	if ( buf == NULL ) {
		/* Cache new string */
		buf = allocString(vm, len + 1);

		if ( buf ) {
			strncpy(buf, *cursor, len); buf[len] = '\0';
		}
	}

	/* Skip past string */
	(*cursor) += len;

	return buf;
}


static char
scanSign(char **cursor)
{
	char sign = *cursor[0];

	if (sign == '+' || sign == '-') {
		(*cursor)++;
	}

	return sign;
}


static UDATA
scanEvents(J9JavaVM *vm, char **cursor, UDATA *actionPtr)
{
	UDATA i, mask = 0;
	char sign = '+';

	PORT_ACCESS_FROM_JAVAVM(vm);

	while ( sign == '+' || sign == '-' )
	{
		/* Map symbolic names onto dump bits */
		for (i = 0; i < J9RAS_DUMP_KNOWN_EVENTS; i++)
		{
			if ( try_scan(cursor, rasDumpEvents[i].name) ) {
				mask = (sign == '+')
					? (mask |  rasDumpEvents[i].bits)
					: (mask & ~rasDumpEvents[i].bits);

				break;
			}
		}
		sign = scanSign(cursor);
	}

	/* Leftover symbols? */
	if ( *cursor[0] != ',' && *cursor[0] != '\0' ) {
		j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_UNRECOGNISED_EVENT_STR, *cursor);
		if (actionPtr) {*actionPtr = BOGUS_DUMP_OPTION;}
	}

	/* Skip past leftover symbols */
	(*cursor) += strcspn(*cursor, ",");

	return mask;
}


static UDATA
scanRequests(J9JavaVM *vm, char **cursor, UDATA *actionPtr)
{
	UDATA i, mask = 0;
	char sign = '+';

	PORT_ACCESS_FROM_JAVAVM(vm);

	while ( sign == '+' || sign == '-' )
	{
		/* Map symbolic names onto dump bits */
		for (i = 0; i < J9RAS_DUMP_KNOWN_REQUESTS; i++)
		{
			if ( try_scan(cursor, rasDumpRequests[i].name) ) {
				mask = (sign == '+')
					? (mask |  rasDumpRequests[i].bits)
					: (mask & ~rasDumpRequests[i].bits);

				break;
			}
		}
		sign = scanSign(cursor);
	}

	/* Leftover symbols? */
	if ( *cursor[0] != ',' && *cursor[0] != '\0' ) {
		j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_UNRECOGNISED_REQUEST_STR, *cursor);
		if (actionPtr) {*actionPtr = BOGUS_DUMP_OPTION;}
	}

	/* Skip past leftover symbols */
	(*cursor) += strcspn(*cursor, ",");

	return mask;
}

void
setAllocationThreshold(J9VMThread *vmThread, UDATA min, UDATA max)
{
	J9MemoryManagerFunctions *fns;
	
	if (vmThread == NULL) {
		return;
	}
	
	fns = vmThread->javaVM->memoryManagerFunctions;
	if (fns) {
		fns->j9gc_set_allocation_threshold(vmThread, min, max);
	}
}

static char *
scanFilter(J9JavaVM *vm, const J9RASdumpSettings *settings, const char **cursor, UDATA *actionPtr)
{
	UDATA eventMask = settings->eventMask;
	char *filter = NULL;
	
	filter = scanString(vm, cursor);
	
	if (eventMask & J9RAS_DUMP_ON_OBJECT_ALLOCATION) {
		RasDumpGlobalStorage *dumpGlobal = vm->j9rasdumpGlobalStorage;
		UDATA min, max;
		
		if (!filter) {
			/* Filter must be supplied for this event */
			goto err;
		}
		
		if (eventMask != J9RAS_DUMP_ON_OBJECT_ALLOCATION) {
			/* Filter cannot be applied to more than one event type */
			goto err;
		}
		
		/* Parse the filter */
		if (!parseAllocationRange(filter, &min, &max)) {
			goto err;
		}
		
		if (dumpGlobal->allocationRangeMin || dumpGlobal->allocationRangeMax) {
			/* A range has already been set. Widen it if necessary */
			if (min < dumpGlobal->allocationRangeMin) {
				dumpGlobal->allocationRangeMin = min;
			}
			if (max > dumpGlobal->allocationRangeMax) {
				dumpGlobal->allocationRangeMax = max;
			}
		} else {
			/* Just set the range */
			dumpGlobal->allocationRangeMin = min;
			dumpGlobal->allocationRangeMax = max;
		}

		setAllocationThreshold(vm->mainThread, min, max);
	}

	return filter;

err:
	*actionPtr = BOGUS_DUMP_OPTION;
	return NULL;
}


/*
 * Function: fixDumpLabel() - function to prepend to a dump filename either a) a directory path specified via one
 *     of the dump env vars (eg IBM_JAVACOREDIR) or b) the current working directory. If the user has specified a
 *     full pathname for the dump it is left unchanged.
 *
 *     The env var associated with a dump agent is held in the 'labelHint' field in the dump agent specification.
 *
 *     For tool dump agents and zOS core dump agents only the env var is processed (the current working directory
 *     is not relevant).
 *
 * Parameters:
 *   vm [in] - pointer to the VM data structure
 *   spec [in] - pointer to the dump agent specification
 *   labelPtr [inout] - pointer to the dump label string
 *   newLabel [in] - boolean, controls whether tool and dsn labels are processed
 *
 * Returns: TRUE if label was changed, FALSE if not.
 */
static IDATA
fixDumpLabel(J9JavaVM *vm, const J9RASdumpSpec *spec, char **labelPtr, IDATA newLabel)
{
	IDATA wasFixed = FALSE;

	PORT_ACCESS_FROM_JAVAVM(vm);

	/* Apply file hints */
	if ( strcmp(spec->labelTag, "file=") == 0 ) {

		char *path = *labelPtr;

		/* Test whether label is already a full file path, or stderr (i.e. '-'). In those cases we are done, no
		 * need to fix up the label.
		 * If the user has specified a path starting with %home or %tenantwd we will add a fully qualified path
		 * at dump time.
		 * Otherwise to detect a full path we check for a path separator as the first character.
	     * On Windows we also check for <drive letter>:<path separator>. UNIX style forward slash separators are
		 * allowed on Windows, since CMVC 200061.
		 */
		if ( path && ((strncmp(path, "%home", strlen("%home") ) == 0) || (strncmp(path, "%tenantwd", strlen("%tenantwd") ) == 0)) ) {
			/* This path does not need fixing. */
#ifdef WIN32
		} else if ( path && path[0] != '\0' && path[0] != '-' &&
			path[0] != DIR_SEPARATOR && path[0] != ALT_DIR_SEPARATOR &&
			( path[1] != ':' || (path[2] != DIR_SEPARATOR && path[2] != ALT_DIR_SEPARATOR) ) ) {
#else
		} else if ( path && path[0] != '\0' && path[0] != '-' && path[0] != DIR_SEPARATOR ) {
#endif
			char prefix[J9_MAX_DUMP_PATH];
			char *prefixPtr = prefix;
#ifdef WIN32
			wchar_t unicodeTemp[J9_MAX_DUMP_PATH];
#endif
			UDATA baseLen, pathLen;

			/* -Xdump:directory takes precedence over the hint settings. */
			if ( dumpDirectoryPrefix != NULL ) {
				/* Use dump filename prefix supplied via -Xdump:directory */
				prefixPtr = dumpDirectoryPrefix;
			} else if ( (spec->labelHint != NULL) && (j9sysinfo_get_env(spec->labelHint, prefix, J9_MAX_DUMP_PATH) == 0) ) {
				/* Check for hint setting */
				/* Ensure null-terminated */
				prefix[J9_MAX_DUMP_PATH-1] = '\0';
			} else {
				/* Just convert to absolute path. */
				int ok = 0;

				/* Get absolute name */
#if defined (WIN32)
				ok = (GetCurrentDirectoryW(J9_MAX_DUMP_PATH, unicodeTemp) != 0);
				if (ok) {
					WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, unicodeTemp, -1,  prefix, J9_MAX_DUMP_PATH, NULL, NULL);
				}
#elif defined(LINUX) || defined(AIXPPC)
				ok = (getcwd(prefix, J9_MAX_DUMP_PATH) != 0);
#elif defined(J9ZOS390)
				ok = (atoe_getcwd(prefix, J9_MAX_DUMP_PATH) != 0);
#endif

				if (ok) {
					prefix[J9_MAX_DUMP_PATH-1] = '\0';
				} else {
					char *exePath = NULL;
					if (j9sysinfo_get_executable_name(NULL, &exePath) == 0){
						char *sepChar = strrchr(exePath, DIR_SEPARATOR);
						if (sepChar-exePath > J9_MAX_DUMP_PATH-1) {
							strcpy(prefix, ".");
						}
						else {
							strncpy(prefix, exePath, sepChar-exePath);
							prefix[sepChar-exePath] = '\0';
						}
						/* Do /not/ delete executable name (system-onwed string). */
					}
					else {
						strcpy(prefix, ".");
					}
				}
			}

			baseLen = strcspn(path, ",");
			pathLen = strlen(prefixPtr) + 1 + baseLen;

			path = allocString(vm, pathLen + 1);

			if ( path ) {
				strcpy(path, prefixPtr);
				strcat(path, DIR_SEPARATOR_STR);
				strncat(path, *labelPtr, baseLen);
				path[pathLen] = '\0';

				*labelPtr = path;
				wasFixed = TRUE;
			}
		}
	}
	/* Apply tool hints */
	else if ( newLabel == FALSE && strcmp(spec->labelTag, "exec=") == 0 ) {

		char cmdLine[J9_MAX_DUMP_PATH];
		char *toolCmd = NULL;

		/* Check for hint setting */
		if ( j9sysinfo_get_env(spec->labelHint, cmdLine, J9_MAX_DUMP_PATH) == 0 ) {
			/* Ensure null-terminated */
			cmdLine[J9_MAX_DUMP_PATH-1] = '\0';

			toolCmd = allocString(vm, strlen(cmdLine) + 1);

			if ( toolCmd ) {
				strcpy(toolCmd, cmdLine);

				*labelPtr = toolCmd;
				wasFixed = TRUE;
			}
		}
	}
#ifdef J9ZOS390
	/* Apply DSN pattern hints */
	else if ( newLabel == FALSE && strcmp(spec->labelTag, "dsn=") == 0 ) {

		char pattern[J9_MAX_DUMP_PATH];
		char *dsnName = NULL;

		/* Check for hint setting */
		if ( j9sysinfo_get_env(spec->labelHint, pattern, J9_MAX_DUMP_PATH) == 0 ) {
			/* Ensure null-terminated */
			pattern[J9_MAX_DUMP_PATH-1] = '\0';

			dsnName = allocString(vm, strlen(pattern) + 1);

			if ( dsnName ) {
				strcpy(dsnName, pattern);

				*labelPtr = dsnName;
				wasFixed = TRUE;
			}
		}
	}
#endif /* J9ZOS390 */

	return wasFixed;
}


static UDATA
processSettings(J9JavaVM *vm, IDATA kind, char *optionString, J9RASdumpSettings *settings)
{
	const J9RASdumpSpec *spec = &rasDumpSpecs[kind];
	char **cursor = &optionString;

	PORT_ACCESS_FROM_JAVAVM(vm);

	/* presume user intent => create agent _and_ update defaults if no events given */
	UDATA action = CREATE_DUMP_AGENT | UPDATE_DUMP_SETTINGS;

	/* Robustness check? */
	if ( optionString == NULL ) {return 0;}

	if ( try_scan(cursor, "defaults:") ) {
		action = UPDATE_DUMP_SETTINGS;
	} else if ( strcmp(*cursor, "defaults") == 0 ) {
		/* We will have printed the defaults out already. */
		return 0;
	}

	/* Allow option removal with or without more settings. */
	if ( try_scan(cursor, "none:") ) {
		action = REMOVE_DUMP_AGENTS;
	} else if ( strcmp(*cursor, "none") == 0 ) {
		*cursor +=strlen("none");
		action = REMOVE_DUMP_AGENTS;
	}

	do {
		if ( try_scan(cursor, "events=") ) {
			settings->eventMask = scanEvents(vm, cursor, &action);
			if ( (action & CREATE_DUMP_AGENT) != 0 ) {
				action &= ~UPDATE_DUMP_SETTINGS;
			}
		}
		if ( try_scan(cursor, "filter=") ) {
			settings->detailFilter = scanFilter(vm, settings, (const char **)cursor, &action);
		}

		if ( try_scan(cursor, "range=") ) {
			scan_udata(cursor, &settings->startOnCount);
			try_scan(cursor, "..");
			scan_udata(cursor, &settings->stopOnCount);
		}
		if ( spec->labelTag && (try_scan(cursor, spec->labelTag) || try_scan(cursor, "label=")) ) {
			settings->labelTemplate = *cursor;

			/* If we don't fix (ie. re-allocate) the label then we have to scan it */
			if ( fixDumpLabel(vm, spec, &settings->labelTemplate, TRUE) == FALSE ) {
				settings->labelTemplate = scanString(vm, (const char **)cursor);
			} else {
				(*cursor) += strcspn(*cursor, ",");
			}
		}
		if ( try_scan(cursor, "opts=") ) {
			settings->dumpOptions = scanString(vm, (const char **)cursor);
		}
		if ( try_scan(cursor, "priority=") ) {
			scan_udata(cursor, &settings->priority);
		}
		if ( try_scan(cursor, "request=") ) {
			settings->requestMask = scanRequests(vm, cursor, &action);
		}
		if ( try_scan(cursor, "msg_filter=") ) {
			settings->subFilter = scanSubFilter(vm, settings, (const char **)cursor, &action);
		}
	} while ( try_scan(cursor, ",") );
	
	if( action == REMOVE_DUMP_AGENTS ) {
		if( settings->eventMask == 0 ) {
			/* For removal, we want to mask out *all* events if none were specified.*/
			settings->eventMask = ~0; 
		}
	}

	if ( action != REMOVE_DUMP_AGENTS && (settings->eventMask & J9RAS_DUMP_ON_OBJECT_ALLOCATION) && (settings->detailFilter == NULL)) {
		j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_INVALID_OR_MISSING_FILTER);
		action = BOGUS_DUMP_OPTION;
	}
	
	if ( action != REMOVE_DUMP_AGENTS && (0 == (settings->eventMask & J9RAS_DUMP_EXCEPTION_EVENT_GROUP)) && (settings->subFilter != NULL)) {
        j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_INCORRECT_USE_MSG_FILTER);
        action = BOGUS_DUMP_OPTION;
    }	

	/* verify start..stop range (allow n..n-1 to indicate open-ended range) */
	if ( settings->stopOnCount < settings->startOnCount ) {
		settings->stopOnCount = settings->startOnCount - 1;
	}

	/* Leftover options? */
	if ( *cursor[0] != '\0' ) {
		char buf[J9_MAX_DUMP_PATH];

		strcpy(buf, spec->name);
		strcat(buf, ":...");
		strcat(buf, *cursor);

		j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_UNRECOGNISED_OPTION_STR, buf);

		action = BOGUS_DUMP_OPTION;
	}

	return action;
}



static J9RASdumpAgent*
findAgent(J9JavaVM *vm, IDATA kind, const J9RASdumpSettings *settings)
{
	J9RASdumpFn dumpFn = rasDumpSpecs[kind].dumpFn;

	J9RASdumpAgent *agent = NULL;

	/* Search for compatible agent */
	while (seekDumpAgent(vm, &agent, dumpFn) == OMR_ERROR_NONE) {

		/* Event mask can always be merged UNLESS there is no overlap and agent has a limited range, otherwise */
		/*   -Xdump:java:events=load,range=1..4  and  -Xdump:java:events=thrstart,range=1..4  would get merged */
		if ( (agent->eventMask != settings->eventMask) &&
		     (agent->startOnCount <= agent->stopOnCount) ) {
			continue;
		}

		/* Can't merge different detail filters */
		if ( ((agent->detailFilter != NULL) && (settings->detailFilter == NULL)                                                              ) || 
         ((agent->detailFilter == NULL) && (settings->detailFilter != NULL)                                                              ) || 
		     ((agent->detailFilter != NULL) && (settings->detailFilter != NULL) && (strcmp(agent->detailFilter, settings->detailFilter) != 0))    ) {
			continue;
		}
		
		/* Can't merge different sub filters */
		if ( ((agent->subFilter != NULL) && (settings->subFilter == NULL)) ||
		     ((agent->subFilter == NULL) && (settings->subFilter != NULL)) ||
		     ((agent->subFilter != NULL) && (settings->subFilter != NULL) && (strcmp(agent->subFilter, settings->subFilter) != 0))) {
		     	continue;
		}		 

		/* Can't merge different ranges */
		if ( agent->startOnCount != settings->startOnCount ) {
			continue;
		}

		/* Can't merge different ranges */
		if ( agent->stopOnCount != settings->stopOnCount ) {
			continue;
		}

		/* Can't merge different labels */
		if ( ((agent->labelTemplate != NULL) && (settings->labelTemplate == NULL)                                                                ) || 
         ((agent->labelTemplate == NULL) && (settings->labelTemplate != NULL)                                                                ) || 
		     ((agent->labelTemplate != NULL) && (settings->labelTemplate != NULL) && (strcmp(agent->labelTemplate, settings->labelTemplate) != 0))    ) {
			continue;
		}

		/* Can't merge different dump options */
		if ( ((agent->dumpOptions != NULL) && (settings->dumpOptions == NULL)                                                            ) || 
         ((agent->dumpOptions == NULL) && (settings->dumpOptions != NULL)                                                            ) || 
		     ((agent->dumpOptions != NULL) && (settings->dumpOptions != NULL) && (strcmp(agent->dumpOptions, settings->dumpOptions) != 0))    ) {
			continue;
		}

		/* Can't merge different priorities */
		if ( agent->priority != settings->priority ) {
			continue;
		}

		/* Can't merge different request masks (especially 'multiple' and 'exclusive') */
		if (agent->requestMask != settings->requestMask) {
			continue;
		}

		/* Candidate! */
		break;
	}

	return agent;
}

static J9RASdumpAgent*
findAgentToDelete(J9JavaVM *vm, IDATA kind, J9RASdumpAgent *agent, const J9RASdumpSettings *settings)
{
	J9RASdumpFn dumpFn = rasDumpSpecs[kind].dumpFn;
	J9RASdumpAgent *matchingAgent = NULL;

	/* Search for compatible agent to disable */
	while (seekDumpAgent(vm, &agent, dumpFn) == OMR_ERROR_NONE) {

		/* If the event mask contains is set it needs to match. */
		if( (settings->eventMask == 0) ||
				(settings->eventMask & agent->eventMask) == 0 ) {
			continue;
		}

		/* If detail filter is set, it needs to match. */
		if ( ((settings->detailFilter != NULL) && (agent->detailFilter == NULL) ) ||
			 ((settings->detailFilter != NULL) &&
			 (agent->detailFilter != NULL) &&
			 (strcmp(settings->detailFilter, agent->detailFilter) != 0)) ) {
			continue;
		}

		/* If sub filter is set, it needs to match. */
		if ( ((settings->subFilter != NULL) && (agent->subFilter == NULL) ) ||
			 ((settings->subFilter != NULL) &&
			 (agent->subFilter != NULL) &&
			 (strcmp(settings->subFilter, agent->subFilter) != 0)) ) {
			continue;
		}

		/* If any options are set, they need to match. */
		if ( ((settings->dumpOptions != NULL) && (agent->dumpOptions == NULL) ) ||
			 ((settings->dumpOptions != NULL) &&
			 (agent->dumpOptions != NULL) &&
			 (strcmp(settings->dumpOptions, agent->dumpOptions) != 0)) ) {
			continue;
		}

		/* If any options are set, they need to match. */
		if ( ((settings->labelTemplate != NULL) && (agent->labelTemplate == NULL) ) ||
			 ((settings->labelTemplate != NULL) &&
			 (agent->labelTemplate != NULL) &&
			 (strcmp(settings->labelTemplate, agent->labelTemplate) != 0)) ) {
			continue;
		}

		/* Check the event mask contains the same bits. */
		if( settings->requestMask != 0 && (settings->requestMask != agent->requestMask) ) {
			continue;
		}

		/* Check the ranges are set and match */
		if ( settings->startOnCount != 0 && agent->startOnCount != settings->startOnCount ) {
			continue;
		}

		/* Check the ranges are set and match */
		if ( settings->startOnCount != 0 && agent->stopOnCount != settings->stopOnCount ) {
			continue;
		} 

		/* Check the priority is set and matches */
		if ( settings->priority != 0 && agent->priority != settings->priority ) {
			continue;
		} 

		/* Passed all tests, matched. */
		matchingAgent = agent;

		/* Candidate! */
		break;
	}

	return matchingAgent;
}

static J9RASdumpAgent*
createAgent(J9JavaVM *vm, IDATA kind, const J9RASdumpSettings *settings)
{
	J9RASdumpAgent *node;

	PORT_ACCESS_FROM_JAVAVM(vm);

	node = (J9RASdumpAgent *)j9mem_allocate_memory(sizeof(J9RASdumpAgent), OMRMEM_CATEGORY_VM);

	if (node) {
		memset(node, 0, sizeof(*node));

		/* Hook up functions */
		node->dumpFn			= rasDumpSpecs[kind].dumpFn;
		node->shutdownFn		= freeAgent;

		/* Apply given settings */
		node->eventMask		= settings->eventMask;
		node->detailFilter		= settings->detailFilter;
		node->startOnCount	= settings->startOnCount;
		node->stopOnCount		= settings->stopOnCount;
		node->labelTemplate	= settings->labelTemplate;
		node->dumpOptions	= settings->dumpOptions;
		node->priority				= settings->priority;
		node->requestMask	= settings->requestMask;
		node->subFilter         = settings->subFilter;
	}

	return node;
}


static omr_error_t
mergeAgent(J9JavaVM *vm, J9RASdumpAgent *agent, const J9RASdumpSettings *settings)
{
	/* Merge event bitmask */
	agent->eventMask |= settings->eventMask;

	/* Install any necessary dump hooks */
	if (rasDumpEnableHooks(vm, agent->eventMask) != OMR_ERROR_NONE) {
		return OMR_ERROR_INTERNAL;
	}

	/* Merge strings */
	if (settings->detailFilter) {
		agent->detailFilter = settings->detailFilter;
	}
	if (settings->labelTemplate) {
		agent->labelTemplate = settings->labelTemplate;
	}
	if (settings->dumpOptions) {
		agent->dumpOptions = settings->dumpOptions;
	}
	if (settings->subFilter) {
        agent->subFilter = settings->subFilter;
    }

	/* Merge request bitmask */
	agent->requestMask |= settings->requestMask;

	return OMR_ERROR_NONE;
}


static omr_error_t
freeAgent(J9JavaVM *vm, J9RASdumpAgent **agentPtr)
{
	J9RASdumpAgent *node = *agentPtr;
	PORT_ACCESS_FROM_JAVAVM(vm);

	/* Remove self from subscriber queue (if there) */
	*agentPtr = NULL; removeDumpAgent(vm, node);

	/* Note: strings are cleaned up via cache */
	j9mem_free_memory(node);

	return OMR_ERROR_NONE;
}



#if (defined(J9VM_RAS_DUMP_AGENTS)) 
omr_error_t
printDumpSpec(struct J9JavaVM *vm, IDATA kind, IDATA verboseLevel)
{
	if ( kind < J9RAS_DUMP_KNOWN_SPECS ) {

		const J9RASdumpSpec *spec = &rasDumpSpecs[kind];
		J9RASdumpSettings *settings = ((J9RASdumpQueue *)vm->j9rasDumpFunctions)->settings;
		J9RASdumpSettings tmpSettings = (settings ? settings[kind] : rasDumpSpecs[kind].settings);

		PORT_ACCESS_FROM_JAVAVM(vm);

		if ( verboseLevel > 0 ) {

			if ( verboseLevel > 1 ) {

				j9tty_err_printf(PORTLIB,
					"\n%s:\n\n"
					"  -Xdump:%s[:defaults][:<option>=<value>, ...]\n",
					spec->summary,
					spec->name
				);

				j9tty_err_printf(PORTLIB, "\nDump options:\n\n");

				j9tty_err_printf(PORTLIB, "  events=<name>        Trigger dump on named events\n"
					"       [+<name>...]      (see -Xdump:events)\n\n");
				j9tty_err_printf(PORTLIB, "  filter=[*]<name>[*]  Filter on class (for load)\n"
					"         [*]<name>[*]  Filter on exception (for throw,systhrow,uncaught)\n"
					"         [*]<name>#<class>.<method>[*]  with throwing class and method\n"
					"         [*]<name>#<class>.<method>#<offset>  with throwing class stack offset\n"
					"         [*]<name>[*]  Filter on exception (for catch)\n"
					"         [*]<name>#<class>.<method>[*]  with catching class and method\n\n"
					"         #<n>[..<m>]            Filter on exit codes (for vmstop)\n"
					"         #<msecs>ms             Filter on time (for slow)\n"
					"         #<i>[k|m][..<j>[k|m]]  Filter on object size (for allocation)\n\n");
				j9tty_err_printf(PORTLIB, "  msg_filter=[*]<string>[*] Filter based on the exception message string\n");
				j9tty_err_printf(PORTLIB, "  %s<label>         %s\n",
					spec->labelTag, spec->labelDescription);
				j9tty_err_printf(PORTLIB, "  range=<n>..<m>       Limit dumps\n");
				j9tty_err_printf(PORTLIB, "  priority=<n>         Highest first\n");
				j9tty_err_printf(PORTLIB, "  request=<name>       Request additional VM actions\n"
					"        [+<name>...]     (see -Xdump:request)\n");

				if (strcmp(spec->name, "heap") == 0) {
					j9tty_err_printf(PORTLIB, "\n  opts=PHD|CLASSIC\n");
				} else if (strcmp(spec->name, "tool") == 0) {
					j9tty_err_printf(PORTLIB, "\n  opts=WAIT<msec>|ASYNC\n");
#ifdef J9ZOS390
				} else if (strcmp(spec->name, "system") == 0) {
					j9tty_err_printf(PORTLIB, "\n  opts=IEATDUMP|CEEDUMP\n");
#endif
				} else {
					j9tty_err_printf(PORTLIB, "\n  opts=<NONE>\n");
				}
			}

			j9tty_err_printf(PORTLIB,
				"\nDefault -Xdump:%s settings:\n\n",
				spec->name);

			/* Use compact form */
			j9tty_err_printf(PORTLIB, "  events=");
			printDumpEvents(vm, tmpSettings.eventMask, 0);

			j9tty_err_printf(PORTLIB,
				"\n"
				"  filter=%s\n"
				"  %s%s\n"
				"  range=%d..%d\n"
				"  priority=%d\n",
				tmpSettings.detailFilter ? tmpSettings.detailFilter : "",
				spec->labelTag, tmpSettings.labelTemplate ? tmpSettings.labelTemplate : "-",
				tmpSettings.startOnCount, tmpSettings.stopOnCount,
				tmpSettings.priority
			);

			/* Use compact form */
			j9tty_err_printf(PORTLIB, "  request=");
			printDumpRequests(vm, tmpSettings.requestMask, 0);

			j9tty_err_printf(PORTLIB, "\n  opts=%s\n\n",
				tmpSettings.dumpOptions ? tmpSettings.dumpOptions : "");

		} else {
			j9tty_err_printf(PORTLIB, "  -Xdump:%s%*c%s\n", spec->name, 17-strlen(spec->name), ' ', spec->summary);
		}

		return OMR_ERROR_NONE;
	}

	return OMR_ERROR_INTERNAL;
}
#endif /* J9VM_RAS_DUMP_AGENTS */


#if (defined(J9VM_RAS_DUMP_AGENTS)) 
omr_error_t
printDumpEvents(struct J9JavaVM *vm, UDATA bits, IDATA verbose)
{
	char *separator = "";
	UDATA maxNameLength = 0;
	UDATA maxDetailLength = 0;
	UDATA i;

	PORT_ACCESS_FROM_JAVAVM(vm);
	
	if (verbose) {
		/* Find the lengths of the longest dump event name and detail */
		for (i = 0; i < J9RAS_DUMP_KNOWN_EVENTS; i++) {
			UDATA nameLength = strlen(rasDumpEvents[i].name);
			UDATA detailLength = strlen(rasDumpEvents[i].detail);
			if (nameLength > maxNameLength) {
				maxNameLength = nameLength;
			}
			if (detailLength > maxDetailLength) {
				maxDetailLength = detailLength;
			}
		}
	}

	/* Header */
	if (verbose) {
		j9tty_err_printf(PORTLIB, "  Name%*cEvent hook\n  ", maxNameLength - 2, ' ');
		for (i = 0; i < maxNameLength; i++) {
			j9tty_err_printf(PORTLIB, "-");
		}
		j9tty_err_printf(PORTLIB, "  ");
		for (i = 0; i < maxDetailLength; i++) {
			j9tty_err_printf(PORTLIB, "-");
		}
		j9tty_err_printf(PORTLIB, "\n");
	}

	/* Events */
	for (i = 0; i < J9RAS_DUMP_KNOWN_EVENTS; i++) {
		if (bits & rasDumpEvents[i].bits) {
			/* Switch between multi-line and single-line styles */
			if (verbose) {
				j9tty_err_printf(PORTLIB, "  %s%*c%s\n", rasDumpEvents[i].name, maxNameLength - strlen(rasDumpEvents[i].name) + 2, ' ', rasDumpEvents[i].detail);
			} else {
				j9tty_err_printf(PORTLIB, "%s%s", separator, rasDumpEvents[i].name);
			}

			separator = "+";
		}
	}

	/* Footer */
	if (verbose) {
		j9tty_err_printf(PORTLIB, "\n");
	}

	return OMR_ERROR_NONE;
}
#endif /* J9VM_RAS_DUMP_AGENTS */


#if (defined(J9VM_RAS_DUMP_AGENTS)) 
omr_error_t
printDumpRequests(struct J9JavaVM *vm, UDATA bits, IDATA verbose)
{
	int i; char *separator = "";

	PORT_ACCESS_FROM_JAVAVM(vm);

	/* Header */
	if (verbose) {j9tty_err_printf( PORTLIB, "  Name      VM action\n  --------  -----------------------\n" );}

	for (i = 0; i < J9RAS_DUMP_KNOWN_REQUESTS; i++)
	{
		if ( bits & rasDumpRequests[i].bits )
		{
			/* Switch between multi-line and single-line styles */
			if (verbose) {
				j9tty_err_printf( PORTLIB, "  %s%*c%s\n", rasDumpRequests[i].name, 10-strlen(rasDumpRequests[i].name), ' ', rasDumpRequests[i].detail );
			} else {
				j9tty_err_printf( PORTLIB, "%s%s", separator, rasDumpRequests[i].name );
			}

			separator = "+";
		}
	}

	/* Footer */
	if (verbose) {j9tty_err_printf( PORTLIB, "\n" );}

	return OMR_ERROR_NONE;
}
#endif /* J9VM_RAS_DUMP_AGENTS */

#if (defined(J9VM_RAS_DUMP_AGENTS)) 
static IDATA
writeIntoBuffer(void* buffer, IDATA buffer_length, IDATA* index, char* data) {
	IDATA len;
	IDATA next_char = *index;
	char* cbuffer = (char*)buffer;
	
	len = strlen(data);
	if ((next_char + len) < buffer_length) {
		strcpy(&cbuffer[next_char], data);
		*index = next_char + len;
		return TRUE;
	}
	return FALSE;
}

static const char *
getLabelTag(struct J9RASdumpAgent *agent)
{
	const char *labelTag = "file=";
	IDATA i = 0;

	for (i = 0; i < J9RAS_DUMP_KNOWN_SPECS; ++i) {
		if (agent->dumpFn == rasDumpSpecs[i].dumpFn) {
			labelTag = rasDumpSpecs[i].labelTag;
			break;
		}
	}

	return labelTag;
}

IDATA
queryAgent(struct J9JavaVM *vm, struct J9RASdumpAgent *agent, IDATA buffer_size, void* buffer, IDATA* index)
{
	IDATA next_char = *index;
	IDATA i;
	IDATA len;
	IDATA rc = 0;
	char* separator = "";
	char temp_buf[1024];
	PORT_ACCESS_FROM_JAVAVM(vm);

	for (i = 0; i < J9RAS_DUMP_KNOWN_SPECS; i++) {
		if( agent->dumpFn == rasDumpSpecs[i].dumpFn ) {
			rc = writeIntoBuffer(buffer, buffer_size, &next_char, rasDumpSpecs[i].name);
			break;
		}
	}

	if (rc == FALSE) {
		/* no space in buffer, so abandon at this point */
		return rc;
	}
	
	/* copy in the events */
	separator = "";
	len = j9str_printf(PORTLIB, temp_buf, sizeof(temp_buf), "%s", ":events=");
	for (i = 0; i < J9RAS_DUMP_KNOWN_EVENTS; i++)
	{
		if ( agent->eventMask & rasDumpEvents[i].bits )
		{
			/* Switch between multi-line and single-line styles */
			len += j9str_printf(PORTLIB, &temp_buf[len], sizeof(temp_buf) - len, "%s%s", separator, rasDumpEvents[i].name);
			separator = "+";
		}
	}
	if (len > 0) {
		strcat(temp_buf, ","); /* temp_buf will have space, there aren't enough events to fill 1024 characters. */
		rc = writeIntoBuffer(buffer, buffer_size, &next_char, temp_buf);
		if (rc == FALSE) {
			/* no space in buffer, so abandon at this point */
			return rc;
		}
	}

	/* copy in the filters */
	len = 0;
	if (agent->detailFilter != NULL) {
		/* Limit filter to 1000 characters so we don't overflow temp_buf and lose the "," */
		len = j9str_printf(PORTLIB, temp_buf, sizeof(temp_buf), "filter=%.1000s,", agent->detailFilter);
	}
	if (len > 0) {
		/*increment buf here if it needs to be used further*/
		rc = writeIntoBuffer(buffer, buffer_size, &next_char, temp_buf);
		if (rc == FALSE) {
			/* no space in buffer, so abandon at this point */
			return rc;
		}
	}

	/* copy in the subfilters */
	len = 0;
	if (agent->subFilter != NULL) {                
		len = j9str_printf(PORTLIB, temp_buf, sizeof(temp_buf), "msg_filter=%.1000s,", agent->subFilter);
	}
	if (len > 0) {
		rc = writeIntoBuffer(buffer, buffer_size, &next_char, temp_buf);
		if (rc == FALSE) {                        
			return rc;
		}
	}	

	/* copy in the label, range and priority */
	len = 0;
	len += j9str_printf(PORTLIB, temp_buf, sizeof(temp_buf),
			"%s%s,"
			"range=%d..%d,"
			"priority=%d,",
			getLabelTag(agent),
			agent->labelTemplate ? agent->labelTemplate : "-",
			agent->startOnCount, agent->stopOnCount,
			agent->priority
	);
	if (len > 0) {
		rc = writeIntoBuffer(buffer, buffer_size, &next_char, temp_buf);
		if (rc == FALSE) {
			/* no space in buffer, so abandon at this point */
			return rc;
		}
	}

	/* copy in the requests */
	separator = "";
	len = j9str_printf(PORTLIB, temp_buf, sizeof(temp_buf), "%s", "request=");
	for (i = 0; i < J9RAS_DUMP_KNOWN_REQUESTS; i++)
	{
		if ( agent->requestMask & rasDumpRequests[i].bits )
		{
			/* Switch between multi-line and single-line styles */
			len += j9str_printf(PORTLIB, &temp_buf[len], sizeof(temp_buf) - len, "%s%s", separator, rasDumpRequests[i].name);
			separator = "+";
		}
	}

	/* copy in the options */
	if ( agent->dumpOptions != NULL ){
		 /* Switch between multi-line and single-line styles */
		len += j9str_printf(PORTLIB, &temp_buf[len], sizeof(temp_buf) - len, ",%s=%s", "opts", agent->dumpOptions);
	}

	len += j9str_printf(PORTLIB, &temp_buf[len], sizeof(temp_buf) - len, "\n");
	if (len > 0) {
		rc = writeIntoBuffer(buffer, buffer_size, &next_char, temp_buf);
		if (rc == FALSE) {
			/* no space in buffer, so abandon at this point */
			return rc;
		}
	}

	*index = next_char;
	return rc;
}
#endif /* J9VM_RAS_DUMP_AGENTS */

#if (defined(J9VM_RAS_DUMP_AGENTS)) 
omr_error_t
printDumpAgent(struct J9JavaVM *vm, struct J9RASdumpAgent *agent)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	j9tty_err_printf(PORTLIB, "-Xdump:");

	if (agent->dumpFn == doSystemDump) {
		j9tty_err_printf(PORTLIB, "system:\n");
	} else if (agent->dumpFn == doHeapDump) {
		j9tty_err_printf(PORTLIB, "heap:\n");
	} else if (agent->dumpFn == doJavaDump) {
		j9tty_err_printf(PORTLIB, "java:\n");
	} else if (agent->dumpFn == doToolDump) {
		j9tty_err_printf(PORTLIB, "tool:\n");
	} else if (agent->dumpFn == doJitDump) {
		j9tty_err_printf(PORTLIB, "jit:\n");
	} else if (agent->dumpFn == doConsoleDump) {
		j9tty_err_printf(PORTLIB, "console:\n");
	} else if (agent->dumpFn == doSilentDump) {
		j9tty_err_printf(PORTLIB, "silent:\n");
#if defined(J9ZOS390)
	} else if (agent->dumpFn == doCEEDump) {
		j9tty_err_printf(PORTLIB, "ceedump:\n");
#endif
	} else if(agent->dumpFn == doSnapDump) {
		j9tty_err_printf(PORTLIB, "snap:\n");
	} else if (agent->dumpFn == doStackDump) {
		j9tty_err_printf(PORTLIB, "stack:\n");
	} else {
		j9tty_err_printf(PORTLIB, "dumpFn=%p\n", agent->dumpFn);
	}

	j9tty_err_printf(PORTLIB, "    events=");
	printDumpEvents(vm, agent->eventMask, 0);
	j9tty_err_printf(PORTLIB, ",");

	if (agent->detailFilter != NULL) {
		j9tty_err_printf(PORTLIB, "\n    filter=%s,",	agent->detailFilter);
	}
	
	if (agent->subFilter != NULL) {
		j9tty_err_printf(PORTLIB, "\n    msg_filter=%s,", agent->subFilter);
	}

	j9tty_err_printf(PORTLIB,
		"\n"
		"    %s%s,\n"
		"    range=%d..%d,\n"
		"    priority=%d,\n",
		getLabelTag(agent),
		agent->labelTemplate ? agent->labelTemplate : "-",
		agent->startOnCount, agent->stopOnCount,
		agent->priority
	);

	j9tty_err_printf(PORTLIB, "    request=");
	printDumpRequests(vm, agent->requestMask, 0);

	if (agent->dumpOptions != NULL) {
		j9tty_err_printf(PORTLIB, ",");
		j9tty_err_printf(PORTLIB, "\n    opts=%s",
			agent->dumpOptions ? agent->dumpOptions : "");
	}
	j9tty_err_printf(PORTLIB, "\n");

	return OMR_ERROR_NONE;
}
#endif /* J9VM_RAS_DUMP_AGENTS */


#if (defined(J9VM_RAS_DUMP_AGENTS)) 
const char*
mapDumpEvent(UDATA eventFlag)
{
	int i;

	for (i = 0; i < J9RAS_DUMP_KNOWN_EVENTS; i++)
	{
		if ( eventFlag & rasDumpEvents[i].bits )
		{
			return rasDumpEvents[i].name;
		}
	}
	if ( eventFlag & J9RAS_DUMP_ON_USER_REQUEST )
	{
		return "api";
	}

	return "unknown";
}
#endif /* J9VM_RAS_DUMP_AGENTS */

#if (defined(J9VM_RAS_DUMP_AGENTS)) 
omr_error_t
copyDumpSettings(struct J9JavaVM *vm, J9RASdumpSettings *src, J9RASdumpSettings *dst)
{
	memset(dst, 0, sizeof(*dst));

	dst->eventMask = src->eventMask;
	if (src->detailFilter != NULL){
		dst->detailFilter = allocString(vm, strlen(src->detailFilter) + 1);
		if (dst->detailFilter == NULL){
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
		strcpy(dst->detailFilter, src->detailFilter);
	} else {
		dst->detailFilter = NULL;
	}
	
	if (src->subFilter != NULL){
		dst->subFilter = allocString(vm, strlen(src->subFilter) + 1);
		if (dst->subFilter == NULL){
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
		strcpy(dst->subFilter, src->subFilter);
	} else {
		dst->subFilter = NULL;
	}
        
	dst->startOnCount = src->startOnCount;
	dst->stopOnCount = src->stopOnCount;
	
	if (src->labelTemplate != NULL){
		dst->labelTemplate  = allocString(vm, strlen(src->labelTemplate ) + 1);
		if (dst->labelTemplate  == NULL){
			/* previously allocated strings are part of the dump string table
			 *  and will be freed on shutdown by freeDumpSettings() */
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
		strcpy(dst->labelTemplate , src->labelTemplate );
	} else {
		dst->labelTemplate = NULL;
	}
	
	if (src->dumpOptions != NULL){
		dst->dumpOptions = allocString(vm, strlen(src->dumpOptions) + 1);
		if (dst->dumpOptions == NULL){
			/* previously allocated strings are part of the dump string table
			 *  and will be freed on shutdown by freeDumpSettings() */
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
		strcpy(dst->dumpOptions, src->dumpOptions);
	} else {
		dst->dumpOptions = NULL;
	}
	
	dst->priority = src->priority;
	dst->requestMask = src->requestMask;

	return OMR_ERROR_NONE;
}
#endif /* J9VM_RAS_DUMP_AGENTS */

#if (defined(J9VM_RAS_DUMP_AGENTS))
J9RASdumpSettings *
copyDumpSettingsQueue(J9JavaVM *vm, J9RASdumpSettings *toCopy)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	int i;
	omr_error_t retVal = OMR_ERROR_NONE;
	J9RASdumpSettings *queue = (J9RASdumpSettings *)j9mem_allocate_memory( sizeof(J9RASdumpSettings) * J9RAS_DUMP_KNOWN_SPECS , OMRMEM_CATEGORY_VM);
	
	if (queue == NULL){
		return NULL;
	}
	for (i = 0; i < J9RAS_DUMP_KNOWN_SPECS; i++){
		retVal = copyDumpSettings(vm, &toCopy[i], &queue[i]);
		if (OMR_ERROR_NONE != retVal) {
			return NULL;
		}
	}
	
	return queue;
}
#endif /* J9VM_RAS_DUMP_AGENTS */

#if (defined(J9VM_RAS_DUMP_AGENTS)) 
struct J9RASdumpSettings *
initDumpSettings(struct J9JavaVM *vm)
{
	J9RASdumpSettings *settings = NULL;
	int i;


	PORT_ACCESS_FROM_JAVAVM(vm);

	/* Lock string table */
	while (compareAndSwapUDATA(&rasDumpStringLock, 0, 1) != 0) {
		omrthread_sleep(200);
	}

	/* Initialize table on first request */
	if (0 == rasDumpStrings.vmCount++) {
		rasDumpStrings.length = 0;
		rasDumpStrings.capacity = 16;

		/* Table records allocated dump strings */
		rasDumpStrings.table = (char **)j9mem_allocate_memory( rasDumpStrings.capacity * sizeof(char *) , OMRMEM_CATEGORY_VM);
	}

	/* Unlock string table */
	compareAndSwapUDATA(&rasDumpStringLock, 1, 0);

	/* Allocate default settings for this VM */
	settings = (J9RASdumpSettings *)j9mem_allocate_memory( sizeof(J9RASdumpSettings) * J9RAS_DUMP_KNOWN_SPECS , OMRMEM_CATEGORY_VM);


	if (settings != NULL) {

		for (i = 0; i < J9RAS_DUMP_KNOWN_SPECS; i++)
		{
			/* cache default settings */
			settings[i] = rasDumpSpecs[i].settings;

			/* apply hint location (requires string table) */
			fixDumpLabel(vm, &rasDumpSpecs[i], &(settings[i].labelTemplate), FALSE);
		}
	}

	return settings;
}
#endif /* J9VM_RAS_DUMP_AGENTS */


#if (defined(J9VM_RAS_DUMP_AGENTS)) 
omr_error_t
freeDumpSettings(struct J9JavaVM *vm, struct J9RASdumpSettings *settings)
{
	U_32 i;

	PORT_ACCESS_FROM_JAVAVM(vm);

	if (settings) {
		j9mem_free_memory(settings);
	}

	/* Lock string table */
	while (compareAndSwapUDATA(&rasDumpStringLock, 0, 1) != 0) {
		omrthread_sleep(200);
	}

	/* Free table on last request */
	if (0 == --rasDumpStrings.vmCount) {
		if ( rasDumpStrings.table ) {

			/* Free saved strings */
			for (i = 0; i < rasDumpStrings.length; i++) {
				j9mem_free_memory(rasDumpStrings.table[i]);
			}

			j9mem_free_memory( rasDumpStrings.table );
		}
	}

	/* Unlock string table */
	compareAndSwapUDATA(&rasDumpStringLock, 1, 0);

	return OMR_ERROR_NONE;
}
#endif /* J9VM_RAS_DUMP_AGENTS */


#if (defined(J9VM_RAS_DUMP_AGENTS)) 
IDATA
scanDumpType(char **optionStringPtr)
{
	IDATA retVal = J9RAS_DUMP_INVALID_TYPE;
	IDATA kind = 0;

	char *startString = *optionStringPtr;

	for (kind = 0; kind < J9RAS_DUMP_KNOWN_SPECS; kind++) {
		if ( try_scan(optionStringPtr, rasDumpSpecs[kind].name) ) {

			/* Check well-formed dump option */
			if ( try_scan(optionStringPtr, "+") ||
			     try_scan(optionStringPtr, ":") ||
			     *optionStringPtr[0] == '\0' ) {
				retVal = kind;
			/* Not well-formed, so backtrack */
			} else {
				*optionStringPtr = startString;
			}

			break;
		}
	}

	return retVal;
}
#endif /* J9VM_RAS_DUMP_AGENTS */

#if (defined(J9VM_RAS_DUMP_AGENTS)) 
omr_error_t
copyDumpAgent(struct J9JavaVM *vm, J9RASdumpAgent *src, J9RASdumpAgent *dst)
{
	memset(dst, 0, sizeof(*dst));

    dst->nextPtr = NULL;
    dst->shutdownFn = src->shutdownFn;
    dst->eventMask = src->eventMask;
    
    if (src->detailFilter != NULL){
    	dst->detailFilter = allocString(vm, strlen(src->detailFilter) + 1);
    	if (dst->detailFilter == NULL){
    		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
    	}
    	strcpy(dst->detailFilter, src->detailFilter);
    } else {
    	dst->detailFilter = NULL;
    }
    
	if (src->subFilter != NULL){
		dst->subFilter = allocString(vm, strlen(src->subFilter) + 1);
		if (dst->subFilter == NULL){
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
		strcpy(dst->subFilter, src->subFilter);
    } else {
        dst->subFilter = NULL;
    }

    dst->startOnCount = src->startOnCount;
    dst->stopOnCount = src->stopOnCount;

    if (src->labelTemplate != NULL){
		dst->labelTemplate  = allocString(vm, strlen(src->labelTemplate ) + 1);
		if (dst->labelTemplate  == NULL){
			/* previous strings alloc'ed in this func are stored in the dump string 
			 * table and so will be freed automatically at shutdown */
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
		strcpy(dst->labelTemplate , src->labelTemplate );
    } else {
    	dst->labelTemplate = NULL;
    }
	
    dst->dumpFn = src->dumpFn;
    
    if (src->dumpOptions != NULL){
	    dst->dumpOptions = allocString(vm, strlen(src->dumpOptions) + 1);
		if (dst->dumpOptions == NULL){
			/* previous strings alloc'ed in this func are stored in the dump string 
			 * table and so will be freed automatically at shutdown */
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
		strcpy(dst->dumpOptions, src->dumpOptions);
    } else {
    	dst->dumpOptions = NULL;
    }
    
    dst->userData = src->userData;
    dst->priority = src->priority;
    dst->requestMask = src->requestMask;
    
	return OMR_ERROR_NONE;
}
#endif /* J9VM_RAS_DUMP_AGENTS */

#if (defined(J9VM_RAS_DUMP_AGENTS))
static void 
freeQueueWithoutRunningShutdown(J9JavaVM *vm, J9RASdumpAgent *toFree)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	
	J9RASdumpAgent *currentAgent = toFree;
	if (currentAgent != NULL){
		J9RASdumpAgent *nextAgent = currentAgent->nextPtr;
		j9mem_free_memory(currentAgent);
		currentAgent = nextAgent;
	}
}

#endif /* J9VM_RAS_DUMP_AGENTS */

#if (defined(J9VM_RAS_DUMP_AGENTS))
J9RASdumpAgent *
copyDumpAgentsQueue(J9JavaVM *vm, J9RASdumpAgent *toCopy)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9RASdumpAgent *queue = NULL;
	J9RASdumpAgent **queueNextPtr = &queue;
	
	while (toCopy != NULL) {
		omr_error_t retVal = OMR_ERROR_NONE;
		J9RASdumpAgent *newAgent = (J9RASdumpAgent *)j9mem_allocate_memory(sizeof(J9RASdumpAgent), OMRMEM_CATEGORY_VM);
		if (newAgent == NULL){
			freeQueueWithoutRunningShutdown(vm, queue);
			return NULL;
		}
		
		retVal = copyDumpAgent(vm, toCopy, newAgent);
		if (OMR_ERROR_NONE != retVal) {
			freeQueueWithoutRunningShutdown(vm, queue);
			return NULL;
		}
		
		newAgent->nextPtr = NULL;
		*queueNextPtr = newAgent;
		queueNextPtr = &newAgent->nextPtr;
		toCopy = toCopy->nextPtr;
	}
	
	return queue;
}
#endif /* J9VM_RAS_DUMP_AGENTS */

#if (defined(J9VM_RAS_DUMP_AGENTS)) 
omr_error_t
loadDumpAgent(struct J9JavaVM *vm, IDATA kind, char *optionString)
{
	J9RASdumpSettings *settings = ((J9RASdumpQueue *)vm->j9rasDumpFunctions)->settings;
	J9RASdumpSettings tmpSettings = (settings ? settings[kind] : rasDumpSpecs[kind].settings);
	J9RASdumpAgent *agent = NULL;

	omr_error_t retVal = OMR_ERROR_NONE;
	UDATA action;

	/* Check and classify options */
	action = processSettings(vm, kind, optionString, &tmpSettings);

	if ( action & BOGUS_DUMP_OPTION ) {
		retVal = OMR_ERROR_INTERNAL;
	}
	if ( action & UPDATE_DUMP_SETTINGS ) {
		if (settings) {
			settings[kind] = tmpSettings;
		}
	}
	if ( action & CREATE_DUMP_AGENT ) {
		/* Try and coalesce agents */
		agent = findAgent(vm, kind, &tmpSettings);

		if ( agent ) {
			retVal = mergeAgent(vm, agent, &tmpSettings);
		} else {
			agent = createAgent(vm, kind, &tmpSettings);
			if ( agent ) {
				/* Add to subscriber queue */
				retVal = insertDumpAgent(vm, agent);
			} else {
				retVal = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			}
		}
	} else if ( action & REMOVE_DUMP_AGENTS ) {
		/* Should not see this on a load. */
		return OMR_ERROR_INTERNAL;
	}

	return retVal;
}
#endif /* J9VM_RAS_DUMP_AGENTS */

#if (defined(J9VM_RAS_DUMP_AGENTS))
omr_error_t
deleteMatchingAgents(struct J9JavaVM *vm, IDATA kind, char *optionString)
{
	J9RASdumpSettings tmpSettings;
	J9RASdumpAgent *agent = NULL;

	omr_error_t retVal = OMR_ERROR_NONE;
	UDATA action;

	memset(&tmpSettings, 0, sizeof(J9RASdumpSettings));

	/* Check and classify options */
	action = processSettings(vm, kind, optionString, &tmpSettings);

	/* Should only have been passed :none options. */
	if ( action != REMOVE_DUMP_AGENTS ) {
		retVal = OMR_ERROR_INTERNAL;
		return retVal;
	}

	agent = findAgentToDelete(vm, kind, NULL, &tmpSettings);
	while( agent != NULL ) {
		agent->eventMask &= ~tmpSettings.eventMask;
		/* If we've turned off this agent for all events, remove it. */
		if( agent->eventMask == 0 ) {
			removeDumpAgent(vm, agent);
			/* Need to restart the search as we've edited the list we're walking. */
			agent = NULL;
		}
		agent = findAgentToDelete(vm, kind, agent, &tmpSettings);
	}

	return retVal;
}
#endif /* J9VM_RAS_DUMP_AGENTS */


#if (defined(J9VM_RAS_DUMP_AGENTS)) 
omr_error_t
unloadDumpAgent(struct J9JavaVM *vm, IDATA kind)
{
	J9RASdumpAgent *agent = NULL;

	/*
	 * Give attached dump agents a chance to flush buffers, etc.
	 */
	while (seekDumpAgent(vm, &agent, rasDumpSpecs[kind].dumpFn) == OMR_ERROR_NONE)
	{
		if (agent->shutdownFn) {
			agent->shutdownFn(vm, &agent);	/* agent will remove itself */
		} else {
			removeDumpAgent(vm, agent);
		}
	}

	return OMR_ERROR_NONE;
}
#endif /* J9VM_RAS_DUMP_AGENTS */

#if (defined(J9VM_RAS_DUMP_AGENTS))

/*
 * Function: createAndRunOneOffDumpAgent - creates a temporary dump agent and runs it
 * 
 * A wrapper around runDumpAgent used for triggering one-off dumps. 
 * 
 * Parameters:
 * 
 * vm [in] - VM pointer
 * context [in] - dump context
 * kind [in] - type code for dump being produced
 * 
 * Returns: OMR_ERROR_NONE on success, OMR_ERROR_INTERNAL or OMR_ERROR_OUT_OF_NATIVE_MEMORY if there was a problem.
 */
omr_error_t
createAndRunOneOffDumpAgent(struct J9JavaVM *vm,J9RASdumpContext * context,IDATA kind,char * optionString)
{
	J9RASdumpSettings *settings = ((J9RASdumpQueue *)vm->j9rasDumpFunctions)->settings;
	J9RASdumpSettings tmpSettings = (settings ? settings[kind] : rasDumpSpecs[kind].settings);
	J9RASdumpAgent *agent = NULL;
	UDATA state = 0;
	UDATA action = 0;
	PORT_ACCESS_FROM_JAVAVM(vm);
	U_64 now = j9time_current_time_millis();
	omr_error_t rc = OMR_ERROR_NONE;
	
	/* Need temporary agent */
	action = processSettings(vm, kind, optionString, &tmpSettings);
	if( action == BOGUS_DUMP_OPTION ) {
		return OMR_ERROR_INTERNAL;
	}
	agent = createAgent(vm, kind, &tmpSettings);

	if ( agent ) {
		rc = runDumpAgent(vm,agent,context,&state,"",now);
					
		/*Undo state, release locks*/
		state = unwindAfterDump(vm, context, state);
					
		/* Clean up temporary agent */
		agent->shutdownFn(vm, &agent);
		
		return rc;
	} else {
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
}

#endif /* J9VM_RAS_DUMP_AGENTS */


#if (defined(J9VM_RAS_DUMP_AGENTS))
/*
 * Function: runDumpAgent - executes a single dump agent
 * 
 * Takes care of acquiring exclusive, performing prepwalk & compact and some final validation/warning
 * messages. 
 * 
 * Parameters:
 * vm [in] -       VM pointer
 * agent [in] -    Agent to be executed
 * context [in] -  Dump context (what triggered the dump)
 * state [inout] - State bit flags. Used to maintain state between multiple calls of runDumpAgent. 
 *                 When you've performed all runDumpAgent calls you must call unwindAfterDump passing
 *                 the state variable to make sure all locks are cleaned up. The first time runDumpAgent 
 *                 is called, state should be initialised to 0.
 * detail    -     Detail string for dump cause
 * timeNow [in] -  Time value as returned from j9time_current_time_millis. Used to timestamp the dumps.
 *
 * Returns: OMR_ERROR_NONE on success, OMR_ERROR_INTERNAL or OMR_ERROR_OUT_OF_NATIVE_MEMORY if there was a problem.
 */
omr_error_t
runDumpAgent(struct J9JavaVM *vm, J9RASdumpAgent * agent, J9RASdumpContext * context, UDATA * state, char * detail, U_64 timeNow)
{
	char localLabel[J9_MAX_DUMP_PATH];
	char *label = localLabel;
	UDATA reqLen;
	PORT_ACCESS_FROM_JAVAVM(vm);
	omr_error_t retVal = OMR_ERROR_INTERNAL;

	/* Convert the dump label template into an actual label, by expanding any tokens */
	retVal = dumpLabel(vm, agent, context, label, J9_MAX_DUMP_PATH, &reqLen, timeNow);
	if ((OMR_ERROR_OUT_OF_NATIVE_MEMORY == retVal) && (agent->dumpFn == doToolDump)) {
		/* For tool agent only, support longer labels, as it's actually a complete tool command line */
		label = j9mem_allocate_memory(reqLen, OMRMEM_CATEGORY_VM);
		if (label) {
			/* retry label template expansion with increased (allocated) memory */
			retVal = dumpLabel(vm, agent, context, label, reqLen, &reqLen, timeNow);
		} else {
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
	}

	if (OMR_ERROR_NONE == retVal) {
		BOOLEAN gotExclusive;
		BOOLEAN userRequestedExclusive = agent->requestMask & J9RAS_DUMP_DO_EXCLUSIVE_VM_ACCESS;
		BOOLEAN userRequestedPrepwalkOrCompact = agent->requestMask & (J9RAS_DUMP_DO_PREPARE_HEAP_FOR_WALK | J9RAS_DUMP_DO_COMPACT_HEAP);

		/* Accumulate locks and actions - only unwind major locks once all dumps are complete */
		/* Trace here as prepareForDump will disable trace. */
		if (agent->dumpFn == doSilentDump) {
			Trc_dump_prepareForSilentDump_Event1();
		} else {
			Trc_dump_prepareForDump_Event1(label?label:"null");
		}
		*state = prepareForDump(vm, agent, context, *state);

		gotExclusive = *state & J9RAS_DUMP_GOT_EXCLUSIVE_VM_ACCESS;

		/* If the dump is a system dump and the customer either requested exclusive and we couldn't get it, or they requested
		 * prepwalk or compact without exclusive, print a warning message (although, unlike heapdump, we still take the dump). 
		 */
		if (agent->dumpFn == doSystemDump) {
			if (userRequestedExclusive && !gotExclusive) {
				j9nls_printf(PORTLIB, J9NLS_WARNING | J9NLS_STDERR, J9NLS_DMP_SYSTEM_DUMP_EXCLUSIVE_FAILED);
			}
									
			if (userRequestedPrepwalkOrCompact && !userRequestedExclusive) {
				j9nls_printf(PORTLIB, J9NLS_WARNING | J9NLS_STDERR, J9NLS_DMP_SYSTEM_DUMP_COMPACT_PREPWALK_WITHOUT_EXCLUSIVE);
			}
		}

		/* If the dump is a heap dump and exclusive access hasn't been obtained, refuse to do the dump */
		/* This might be encapsulated more neatly if the triggering code were moved down into the dump functions themselves */
		if (gotExclusive || (agent->dumpFn != doHeapDump)) {
			agent->prepState = *state;
			TRIGGER_J9HOOK_VM_DUMP_START(vm->hookInterface, vm->internalVMFunctions->currentVMThread(vm), label, detail);
			retVal = runDumpFunction( agent, label, context );
			TRIGGER_J9HOOK_VM_DUMP_END(vm->hookInterface, vm->internalVMFunctions->currentVMThread(vm), label, detail);
			
			if (context->dumpList) {
				if (agent->dumpFn == doHeapDump) {
					if (agent->dumpOptions && strstr(agent->dumpOptions, "PHD")) {
						writeIntoBuffer(context->dumpList, context->dumpListSize, (IDATA*)&(context->dumpListIndex), label);
						writeIntoBuffer(context->dumpList, context->dumpListSize, (IDATA*)&(context->dumpListIndex), "\t");
					}

					if (agent->dumpOptions && strstr(agent->dumpOptions, "CLASSIC")) {
						/* do label hackery for classic, see writeClassicHeapdump */
						if (reqLen >= 4 && strcmp(&label[reqLen - 4], ".phd") == 0) {
							strcpy(&label[reqLen - 4], ".txt");
						}
						writeIntoBuffer(context->dumpList, context->dumpListSize, (IDATA*)&(context->dumpListIndex), label);
						writeIntoBuffer(context->dumpList, context->dumpListSize, (IDATA*)&(context->dumpListIndex), "\t");
					}
				} else if (agent->dumpFn != doToolDump) {
					/* Safe write into the dump list buffer (to make dump label available to tool dumps) */
					writeIntoBuffer(context->dumpList, context->dumpListSize, (IDATA*)&(context->dumpListIndex), label);
					writeIntoBuffer(context->dumpList, context->dumpListSize, (IDATA*)&(context->dumpListIndex), "\t");
				}
			}
		} else if (userRequestedExclusive){
			/*The user is doing a heapdump, asked for exclusive but we couldn't get it*/
			j9nls_printf(PORTLIB, J9NLS_WARNING | J9NLS_STDERR, J9NLS_DMP_HEAP_DUMP_EXCLUSIVE_FAILED);
		} else {
			/*The user is doing a heapdump and didn't ask for exclusive*/
			j9nls_printf(PORTLIB, J9NLS_WARNING | J9NLS_STDERR, J9NLS_DMP_HEAP_DUMP_EXCLUSIVE_NOT_REQUESTED);
		}

		*state = unwindAfterDump(vm, context, *state);
		/* Trace here as unwindAfterDump will re-enable trace if it was turned off. */
		if (agent->dumpFn == doSilentDump) {
			Trc_dump_unwindAfterSilentDump_Event1();
		} else {
			Trc_dump_unwindAfterDump_Event1(label?label:"null");
		}

	}
	
	/* If we allocated a longer label (actually only for tool dumps) then free it now */
	if (label != localLabel) {
		j9mem_free_memory(label);
	}
	return retVal;
}
#endif /* J9VM_RAS_DUMP_AGENTS */



#if (defined(J9VM_RAS_DUMP_AGENTS)) 
/*
 * Execute the dump function from agent under signal protection.
 * If a signal occurs OMR_ERROR_INTERNAL is returned.
 */
omr_error_t
runDumpFunction(J9RASdumpAgent *agent, char *label, J9RASdumpContext *context)
{
	UDATA rc;
	I_32 protectedResult;
	J9RASprotectedDumpData dumpData;
	J9JavaVM *vm = context->javaVM;
	RasDumpGlobalStorage *dumpGlobal = (RasDumpGlobalStorage *)vm->j9rasdumpGlobalStorage;
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (dumpGlobal->noProtect) {
		return agent->dumpFn(agent, label, context);
	} else {
		dumpData.agent = agent;
		dumpData.label = label;
		dumpData.context = context;
		
		protectedResult = j9sig_protect(
			protectedDumpFunction, &dumpData, 
			signalHandler, NULL, 
			J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGALLSYNC, 
			&rc);
		
		if (protectedResult != 0) {
			return OMR_ERROR_INTERNAL;
		} else {
			return (omr_error_t)rc;
		}
	}
}

#endif /* J9VM_RAS_DUMP_AGENTS */


#if (defined(J9VM_RAS_DUMP_AGENTS)) 
/*
 * See runDumpFunction()
 */
static UDATA
protectedDumpFunction(struct J9PortLibrary *portLibrary, void *userData)
{
	J9RASprotectedDumpData* dumpData = userData;

	return (UDATA)dumpData->agent->dumpFn(dumpData->agent, dumpData->label, dumpData->context);
}

#endif /* J9VM_RAS_DUMP_AGENTS */


#if (defined(J9VM_RAS_DUMP_AGENTS)) 
/*
 * See runDumpFunction()
 */
static UDATA
signalHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
	return J9PORT_SIG_EXCEPTION_RETURN;
}

#endif /* J9VM_RAS_DUMP_AGENTS */


#if (defined(J9VM_RAS_DUMP_AGENTS))

/**
 * Writes the appropriate "we're about to write a dump" message to the console depending on whether the
 * dump was event driven or user requested.
 * 
 * Parameters:
 * portLibrary [in] library to use to write dump
 * context [in] context that dump was taken in
 * dumpType [in] type of dump being written - e.g. "Java"
 * fileName [in] name of the file being used to write out. Can be NULL.
 */
void
reportDumpRequest(struct J9PortLibrary* portLibrary, J9RASdumpContext * context, const char * const dumpType, const char * const fileName)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	/*There are two sets of messages - one when the filename is specified, one when it isn't*/
	if (fileName != NULL) {
		if (context->eventFlags & J9RAS_DUMP_ON_USER_REQUEST) {
			/*User driven dump*/
			j9nls_printf(PORTLIB,
				J9NLS_INFO | J9NLS_STDERR | J9NLS_VITAL,
				J9NLS_DMP_USER_REQUESTED_DUMP_STR, 
				dumpType, 
				fileName, 
				context->eventData != NULL ? context->eventData->detailData : NULL);
			
			Trc_dump_reportDumpStart_FromUser(dumpType,fileName,context->eventData != NULL ? context->eventData->detailData : NULL);
		} else {
			/*Event driven dump*/
			j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR | J9NLS_VITAL, J9NLS_DMP_EVENT_TRIGGERED_DUMP_STR, dumpType, fileName);
			
			Trc_dump_reportDumpStart_FromEvent(dumpType,fileName);
		}
	} else {
		if (context->eventFlags & J9RAS_DUMP_ON_USER_REQUEST) {
			/*User driven dump*/
			j9nls_printf(PORTLIB,
				J9NLS_INFO | J9NLS_STDERR | J9NLS_VITAL,
				J9NLS_DMP_USER_REQUESTED_DUMP_STR_NOFILE, 
				dumpType,  
				context->eventData != NULL ? context->eventData->detailData : NULL);
			
			Trc_dump_reportDumpStart_FromUser_NoFile(dumpType,context->eventData != NULL ? context->eventData->detailData : NULL);
		} else {
			/*Event driven dump*/
			j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR | J9NLS_VITAL, J9NLS_DMP_EVENT_TRIGGERED_DUMP_STR_NOFILE, dumpType);
			
			Trc_dump_reportDumpStart_FromEvent_NoFile(dumpType);
		}
	}
}

#endif /* J9VM_RAS_DUMP_AGENTS */

#if (defined(J9VM_RAS_DUMP_AGENTS))

static char *
scanSubFilter(J9JavaVM *vm, const J9RASdumpSettings *settings, const char **cursor, UDATA *actionPtr)
{
        UDATA eventMask = settings->eventMask;
        char *subFilter = NULL;

        subFilter = scanString(vm, cursor);

        if (0 == (eventMask & J9RAS_DUMP_EXCEPTION_EVENT_GROUP)) {
            *actionPtr = BOGUS_DUMP_OPTION;
        }

        return subFilter;
}

#endif /* J9VM_RAS_DUMP_AGENTS */
