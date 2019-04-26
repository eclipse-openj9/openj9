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

#include "dmpsup.h"
#include "rasdump_internal.h"
#include "jvminit.h"
#include "j9consts.h"
#include "j9dump.h"
#include "j9dmpnls.h"
#include <string.h>
#include "omrmutex.h"
#include "j9port.h"
#include "jvmri.h"
#include "omrthread.h"
#include "omrlinkedlist.h"


#if defined(J9VM_PORT_OMRSIG_SUPPORT)
#include "omrsig.h"
#else /* defined(J9VM_PORT_OMRSIG_SUPPORT) */
#include <signal.h>
#endif /* defined(J9VM_PORT_OMRSIG_SUPPORT) */

#define _UTE_STATIC_
#include "ut_j9dmp.h"
#undef _UTE_STATIC_

/* Abort data */
static J9JavaVM *cachedVM = NULL;

/* config starts locked and we unlock it once the initial configuration is in place */
static UDATA rasDumpLockConfig = -1;

/* GLOBAL: dump agent bitvectors */
UDATA rasDumpAgentEnabled = (UDATA)-1;

char* dumpDirectoryPrefix = NULL;

#define MAX_DUMP_OPTS  128

#if defined(J9ZOS390)
#if defined(J9VM_ENV_DATA64)
#include <__le_api.h>
#else
#include <leawi.h>
#include <ceeedcct.h>
#endif
#endif

/* Default -Xdump agent definitions. To allow env var modifications we don't merge these here, they
 * will get merged later when the agents are loaded
 */
static const J9RASdefaultOption defaultAgents[] = {
	{ "heap",    "events=systhrow,range=1..4,filter=java/lang/OutOfMemoryError" },
	{ "java",    "events=gpf,range=1..0" },
	{ "java",    "events=user,range=1..0" },
	{ "java",    "events=abort,range=1..0" },
	{ "java",    "events=traceassert,range=1..0" },
	{ "java",    "events=systhrow,range=1..4,filter=java/lang/OutOfMemoryError" },
	{ "java",    "events=corruptcache,range=1..0" },
	{ "snap",    "events=gpf,range=1..0" },
	{ "snap",    "events=abort,range=1..0" },
	{ "snap",    "events=traceassert,range=1..0" },
	{ "snap",    "events=systhrow,range=1..4,filter=java/lang/OutOfMemoryError" },
	{ "snap",    "events=corruptcache,range=1..0"},
	{ "system",  "events=gpf,range=1..0" },
#ifdef J9ZOS390
	{ "system",  "events=user,range=1..0" },
#endif
	{ "system",  "events=abort,range=1..0" },
	{ "system",  "events=traceassert,range=1..0" },
	{ "system",  "events=corruptcache,range=1..0" },
	/* System dumps added for OOM, all platforms. JTC-JAT LIR #17406 */
	{ "system",  "events=systhrow,range=1..1,filter=java/lang/OutOfMemoryError,request=exclusive+compact+prepwalk" },
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	{ "jit",    "events=gpf,range=1..0" },
	{ "jit",    "events=abort,range=1..0" }
#endif /* J9VM_INTERP_NATIVE_SUPPORT */
};
static const int numDefaultAgents = ( sizeof(defaultAgents) / sizeof(J9RASdefaultOption) );


static omr_error_t shutdownDumpAgents (J9JavaVM *vm);
static omr_error_t popDumpFacade (J9JavaVM *vm);
static omr_error_t installAbortHandler (J9JavaVM *vm);
static omr_error_t showDumpAgents (J9JavaVM *vm);
static IDATA configureDumpAgents (J9JavaVM *vm);
static omr_error_t printDumpUsage (J9JavaVM *vm);
static omr_error_t pushDumpFacade (J9JavaVM *vm);
static void abortHandler (int sig);
static void initRasDumpGlobalStorage(J9JavaVM *vm);
static void freeRasDumpGlobalStorage(J9JavaVM *vm);
static void hookVmInitialized PROTOTYPE((J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData));
#if defined(LINUX)
static void appendSystemInfoFromFile(J9JavaVM *vm, U_32 key, const char *fileName );
#endif /* defined(LINUX) */
#ifdef J9ZOS390
static IDATA processZOSDumpOptions(J9JavaVM *vm, J9RASdumpOption* agentOpts, int optIndex);
static void triggerAbend(void);
#endif

#if defined(J9VM_PORT_OMRSIG_SUPPORT) && defined(WIN32)

/*
 * this code causes omrsig to be looked up dynamically and for the local functions to pass through to it.
 */
typedef sig_handler_t (*omrsig_primary_signal_Type)(int sig, sig_handler_t disp);
typedef int (*omrsig_handler_Type)(int sig, void *siginfo, void *uc);

static omrsig_primary_signal_Type omrsig_primary_signal_Static;
static omrsig_handler_Type omrsig_handler_Static;
static UDATA omrsigHandle;

BOOLEAN
loadOMRSIG(J9JavaVM *vm)
{
	J9VMDllLoadInfo omrsigLoadInfo;

	PORT_ACCESS_FROM_JAVAVM( vm );

	memset(&omrsigLoadInfo, 0, sizeof(J9VMDllLoadInfo));
	omrsigLoadInfo.loadFlags |= XRUN_LIBRARY;
	strcpy((char *) &omrsigLoadInfo.dllName, "omrsig");
	if (vm->internalVMFunctions->loadJ9DLL(vm, &omrsigLoadInfo) != TRUE) {
		j9tty_err_printf(PORTLIB, "Can't open OMRSIG library\n");
		return FALSE;
	}
	omrsigHandle = omrsigLoadInfo.descriptor;
	
	j9sl_lookup_name(omrsigHandle, "omrsig_primary_signal", (UDATA *) &omrsig_primary_signal_Static, "IP");
	j9sl_lookup_name(omrsigHandle, "omrsig_handler", (UDATA *) &omrsig_handler_Static, "IPP");
	return TRUE;
}

void
unloadOMRSIG(J9PortLibrary *portLib)
{
	PORT_ACCESS_FROM_PORT(portLib);
	
	j9sl_close_shared_library(omrsigHandle);
	omrsigHandle = 0;
}

sig_handler_t
omrsig_primary_signal(int sig, sig_handler_t disp)
{
	if (NULL == omrsig_primary_signal_Static)
		return NULL;
	return omrsig_primary_signal_Static(sig, disp);
}

int
omrsig_handler(int sig, void *siginfo, void *uc)
{
	if (NULL == omrsig_handler_Static)
		return 0;
	return omrsig_handler_Static(sig, siginfo, uc);
}

#endif


static void
abortHandler(int sig)
{
	J9VMThread *vmThread = cachedVM ? cachedVM->internalVMFunctions->currentVMThread(cachedVM) : NULL;

#if defined(J9ZOS390)
	BOOLEAN doTriggerAbend = FALSE;
#endif

#if defined(J9VM_PORT_OMRSIG_SUPPORT)
	/* Chain to application handler */
	if ( !vmThread || (vmThread && (cachedVM->sigFlags & J9_SIG_NO_SIG_CHAIN) == 0) ) {
		omrsig_handler(sig, 0, 0);
	}
#endif /* defined(J9VM_PORT_OMRSIG_SUPPORT) */

	/* Re-install application abort handler */
	OMRSIG_SIGNAL(SIGABRT, SIG_DFL);

	/* To get the dumps we must ensure that this thread is attached to the vm. */
	/* Also, we must have a valid cachedVM in order to get the attach to work. */
	if (cachedVM && !vmThread) {
		J9JavaVM* vm = cachedVM; /* local variable required by FIND_DLL_TABLE_ENTRY macro below */
		J9VMDllLoadInfo *loadInfo = FIND_DLL_TABLE_ENTRY(J9_RAS_DUMP_DLL_NAME);

		/* Only attempt to attach thread while JVM is up and running. JTC-JAT PR 86446 + PR 98920 */
		if (loadInfo
			&& (IS_STAGE_COMPLETED(loadInfo->completedBits, VM_INITIALIZATION_COMPLETE))
			&& (!IS_STAGE_COMPLETED(loadInfo->completedBits, INTERPRETER_SHUTDOWN))
		) {
			JavaVMAttachArgs attachArgs;

			attachArgs.version = JNI_VERSION_1_2;
			attachArgs.name = "SIGABRT Thread";
			attachArgs.group = NULL;

			cachedVM->internalVMFunctions->AttachCurrentThreadAsDaemon((JavaVM *)cachedVM, (void **)&vmThread, &attachArgs);
		}
	}

#if defined(J9ZOS390)
	if (NULL != cachedVM) {
		if (J9_SIG_POSIX_COOPERATIVE_SHUTDOWN == (J9_SIG_POSIX_COOPERATIVE_SHUTDOWN & cachedVM->sigFlags))  {
			doTriggerAbend = TRUE;
		}
	}
#endif /* defined(J9ZOS390) */

	if ( vmThread ) {
		PORT_ACCESS_FROM_JAVAVM(cachedVM);
		/* Check if we are running on the Java stack, by comparing the address of a local variable against the lower
		 * and upper bounds of the Java stack. If we are on the Java stack it is not safe to run the RAS dump agents,
		 * so just issue a message here and drop out to let the OS handle the abort. JTC-JAT Problem Report 77991.
		 */
		J9JavaStack* javaStack = vmThread->stackObject;
		UDATA* lowestSlot =  J9_LOWEST_STACK_SLOT(vmThread);
		UDATA* highestSlot = javaStack ? javaStack->end : NULL;
		UDATA* localAddress = (UDATA*)&highestSlot;

		if ((localAddress >= lowestSlot) && (localAddress < highestSlot)) {
			/* Running on Java stack, do not attempt to produce RAS dumps */
			j9nls_printf(PORTLIB, J9NLS_WARNING | J9NLS_STDERR, J9NLS_DMP_ABORT_ON_JAVA_STACK);
		} else {
			if (OMR_ERROR_NONE == J9DMP_TRIGGER(cachedVM, vmThread, J9RAS_DUMP_ON_ABORT_SIGNAL)) {

#if defined(J9ZOS390)
				if (doTriggerAbend) {
					triggerAbend();
					/* unreachable */
				}
#endif

				/* RAS dump agents triggered OK, call exit not abort to avoid extra OS dumps, defect 148334 */
				j9exit_shutdown_and_exit(1);
			}
		}
	}

#if defined(J9ZOS390)
	if (doTriggerAbend) {
		triggerAbend();
		/* unreachable */
	}
#endif

	/* Re-send abort signal (needed if it was an asynchronous request) */
	abort();
}

#if defined(J9ZOS390)
static void
triggerAbend(void)
{
	sigrelse(SIGABND); /* CMVC 191934: need to unblock sigabnd before issuing the abend call */
#if defined(J9VM_ENV_DATA64)
	__cabend(PORT_ABEND_CODE, PORT_ABEND_REASON_CODE, PORT_ABEND_CLEANUP_CODE);
	/* unreachable */
#else
	/* 31-bit z/OS */
	{
		_INT4 code = PORT_ABEND_CODE;
		_INT4 reason = PORT_ABEND_REASON_CODE;
		_INT4 cleanup = PORT_ABEND_CLEANUP_CODE;	 /* normal termination processing */

		CEE3AB2(&code, &reason, &cleanup);
		/* unreachable */
	}
#endif
}
#endif /* J9ZOS390 */

static omr_error_t
installAbortHandler(J9JavaVM *vm)
{
	/* Handler can only map to one VM */
	if ( cachedVM ) {
		return OMR_ERROR_INTERNAL;
	}

	cachedVM = vm;

	/* Install one-shot dump trigger */
	OMRSIG_SIGNAL(SIGABRT, abortHandler);
	return OMR_ERROR_NONE;
}


UDATA
lockConfigForUse(void)
{
	while (1) {
		IDATA currentValue = rasDumpLockConfig;
		if (currentValue >= 0) {
			if (compareAndSwapUDATA(&rasDumpLockConfig, currentValue, currentValue + 1) == currentValue) {
				break;
			}
		}

		omrthread_yield();
	}

	/* we block until we succeed so we only ever return true */
	return 1;
}

UDATA
lockConfigForUpdate(void)
{
	/* if the config is not currently in use then we can update it, otherwise let the caller know that
	 * the config is in use and they should try again later
	 */
	return compareAndSwapUDATA(&rasDumpLockConfig, 0, -1) == 0;
}


UDATA
unlockConfig(void)
{
	while (1) {
		IDATA currentValue = rasDumpLockConfig;
		IDATA newValue = 0;

		if (currentValue < 0) {
			/* We're undoing an update lock or correcting a mangled state.
			 * We gamble on releasing with mangled state so that dumps don't become completely non functional
			 */
			newValue = 0;
		} else if (currentValue > 0) {
			newValue = currentValue - 1;
		}

		if (compareAndSwapUDATA(&rasDumpLockConfig, currentValue, newValue) == currentValue) {
			break;
		}

		omrthread_yield();
	}

	/* we block until we succeed so we only ever return true */
	return 1;
}

static omr_error_t
showDumpAgents(J9JavaVM *vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9RASdumpAgent *agent = NULL;

	j9tty_err_printf(PORTLIB, "\nRegistered dump agents\n----------------------\n");

	while (seekDumpAgent(vm, &agent, NULL) == OMR_ERROR_NONE)
	{
		printDumpAgent(vm, agent);
		j9tty_err_printf(PORTLIB, "----------------------\n");
	}

	j9tty_err_printf(PORTLIB, "\n");

	return OMR_ERROR_NONE;
}

static omr_error_t
storeDefaultData(J9JavaVM *vm)
{
	J9RASdumpQueue *queue = (J9RASdumpQueue *)vm->j9rasDumpFunctions;

	queue->defaultAgents = copyDumpAgentsQueue(vm, queue->agents);
	if (queue->defaultAgents == NULL){
		return OMR_ERROR_INTERNAL;
	}
	queue->defaultSettings = copyDumpSettingsQueue(vm, queue->settings);
	if (queue->defaultSettings  == NULL){
		return OMR_ERROR_INTERNAL;
	}
	return OMR_ERROR_NONE;
}

/* Function for configuring the RAS dump agents. Since Java 6 SR2 (VMDESIGN 1477), in increasing 
 * order of precedence:
 * 		Default agents
 * 		DISABLE_JAVADUMP, IBM_HEAPDUMP, IBM_HEAP_DUMP
 * 		IBM_JAVADUMP_OUTOFMEMORY, IBM_HEAPDUMP_OUTOFMEMORY
 * 		JAVA_DUMP_OPTS environment variable (including dump count parameter)
 * 		-Xdump command-line options
 */
static IDATA
configureDumpAgents(J9JavaVM *vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	J9VMInitArgs *j9vm_args = vm->vmArgsArray;
	IDATA i;
	IDATA xdumpIndex = 0;
	IDATA showAgents = 0;
	RasDumpGlobalStorage *dumpGlobal = (RasDumpGlobalStorage *)vm->j9rasdumpGlobalStorage;

	/* Record recognized agent options (R...L) */
	J9RASdumpOption* agentOpts = NULL;
	IDATA agentNum = 0;
	IDATA kind = 0;
	char *optionString = NULL;

	/*
	 * -XX:[+-]HeapDumpOnOutOfMemoryError.
	 */
	IDATA heapDumpIndex = -1; /* index of the rightmost HeapDumpOnOutOfMemoryError option */
	BOOLEAN processXXHeapDump = FALSE;  /* either -XX:[+-]HeapDumpOnOutOfMemoryError is specified */
	BOOLEAN enableXXHeapDump = FALSE; /* -XX:+HeapDumpOnOutOfMemoryError is selected */

	/* -Xdump:help */
	if ( FIND_AND_CONSUME_ARG(EXACT_MATCH, VMOPT_XDUMP ":help", NULL) >= 0 )
	{
		printDumpUsage(vm);
		return J9VMDLLMAIN_SILENT_EXIT_VM;
	}

	/* -Xdump:events */
	if ( FIND_AND_CONSUME_ARG(EXACT_MATCH, VMOPT_XDUMP ":events", NULL) >= 0 )
	{
		j9tty_err_printf(PORTLIB, "\nTrigger events:\n\n");
		printDumpEvents( vm, J9RAS_DUMP_ON_ANY, 1 );
		return J9VMDLLMAIN_SILENT_EXIT_VM;
	}

	/* -Xdump:request */
	if ( FIND_AND_CONSUME_ARG(EXACT_MATCH, VMOPT_XDUMP ":request", NULL) >= 0 )
	{
		j9tty_err_printf(PORTLIB, "\nAdditional VM requests:\n\n");
		printDumpRequests( vm, (UDATA)-1, 1 );
		return J9VMDLLMAIN_SILENT_EXIT_VM;
	}

	/* -Xdump:tokens */
	if ( FIND_AND_CONSUME_ARG(EXACT_MATCH, VMOPT_XDUMP ":tokens", NULL) >= 0 )
	{
		j9tty_err_printf(PORTLIB, "\nLabel tokens:\n\n");
		printLabelSpec( vm );
		return J9VMDLLMAIN_SILENT_EXIT_VM;
	}

	/* -Xdump:what */
	if ( FIND_AND_CONSUME_ARG(EXACT_MATCH, VMOPT_XDUMP ":what", NULL) >= 0 )
	{
		showAgents = 1;
	}

	/* -Xdump:noprotect */
	if ( FIND_AND_CONSUME_ARG(EXACT_MATCH, VMOPT_XDUMP ":noprotect", NULL) >= 0 )
	{
		dumpGlobal->noProtect = 1;
	}

	/* -Xdump:nofailover */
	if ( FIND_AND_CONSUME_ARG(EXACT_MATCH, VMOPT_XDUMP ":nofailover", NULL) >= 0 )
	{
		dumpGlobal->noFailover = 1;
	}

	/* -Xdump:dynamic ... grab hooks before the JIT turns them off */
	if ( FIND_AND_CONSUME_ARG(EXACT_MATCH, VMOPT_XDUMP ":dynamic", NULL) >= 0 )
	{
		rasDumpEnableHooks(vm, J9RAS_DUMP_ON_EXCEPTION_THROW | J9RAS_DUMP_ON_EXCEPTION_CATCH);
	}

#if defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL)
	/* -Xdump:suspendwith */
	xdumpIndex = FIND_AND_CONSUME_ARG(STARTSWITH_MATCH, VMOPT_XDUMP ":suspendwith", NULL);
	if (xdumpIndex >= 0) {
		/* Get value of -Xdump:suspendwith */
		const char *optName = VMOPT_XDUMP ":suspendwith=";
		UDATA suspendwith = 0;
		UDATA parseError = GET_INTEGER_VALUE(xdumpIndex, optName, suspendwith);
		if (OPTION_OK != parseError) {
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_INVALID_OR_RESERVED, optName);
			printDumpUsage(vm);
			return J9VMDLLMAIN_SILENT_EXIT_VM;
		} else {
			OMRPORT_ACCESS_FROM_OMRPORT(OMRPORT_FROM_J9PORT(privatePortLibrary));
			int32_t result = omrintrospect_set_suspend_signal_offset((int32_t)suspendwith);
			if (0 != result) {
				if (J9PORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM == result) {
					j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_UNSUPPORTED_ON_PLATFORM, "suspendwith");
				} else {
					j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_INVALID_OR_RESERVED, "suspendwith");
				}
				printDumpUsage(vm);
				return J9VMDLLMAIN_SILENT_EXIT_VM;
			}
		}
	}
#endif

	agentOpts = j9mem_allocate_memory(sizeof(J9RASdumpOption)*MAX_DUMP_OPTS, OMRMEM_CATEGORY_VM);
	if( NULL == agentOpts ) {
		j9tty_err_printf(PORTLIB, "Storage for dump options not available, unable to process dump options\n");
		return J9VMDLLMAIN_FAILED;
	}
	memset(agentOpts,0,sizeof(J9RASdumpOption)*MAX_DUMP_OPTS);

	/* Load up the default agents */
	for (i = 0; i < numDefaultAgents; i++) {
		char *typeString = defaultAgents[i].type;
		agentOpts[agentNum].kind = scanDumpType(&typeString);
		agentOpts[agentNum].flags = J9RAS_DUMP_OPT_ARGS_STATIC;
		agentOpts[agentNum].args = defaultAgents[i].args;
		agentNum++;
	}
	
	/* Process DISABLE_JAVADUMP IBM_HEAPDUMP IBM_JAVADUMP_OUTOFMEMORY and IBM_HEAPDUMP_OUTOFMEMORY */
	mapDumpSwitches(vm, agentOpts, &agentNum);
	
	/* Process JAVA_DUMP_OPTS */
	mapDumpOptions(vm, agentOpts, &agentNum);
	
	/* Process IBM_JAVA_HEAPDUMP_TEXT and IBM_JAVA_HEAPDUMP_TEST */ 
	mapDumpDefaults(vm, agentOpts, &agentNum);

	/* Process IBM_XE_COE_NAME */
	mapDumpSettings(vm, agentOpts, &agentNum);
	

	/*
	 * Process -XX:[+-]HeapDumpOnOutOfMemoryError.
	 * Set heapDumpIndex to the index of the rightmost option
	 * and indicate whether enable or disable wins.
	 */
	heapDumpIndex = FIND_AND_CONSUME_ARG(EXACT_MATCH, VMOPT_XXNOHEAPDUMPONOOM, NULL);
	xdumpIndex = FIND_AND_CONSUME_ARG(EXACT_MATCH, VMOPT_XXHEAPDUMPONOOM, NULL);
	processXXHeapDump = ((xdumpIndex >= 0) || (heapDumpIndex >= 0));
	if (xdumpIndex > heapDumpIndex) {
		enableXXHeapDump = TRUE;
		heapDumpIndex = xdumpIndex;
	}

	/*
	 * Process -Xdump command-line options (L..R).
	 * Treat -XX:[+-]HeapDumpOnOutOfMemoryError as an alias of -Xdump.
	 */

	xdumpIndex = FIND_ARG_IN_VMARGS_FORWARD(OPTIONAL_LIST_MATCH, VMOPT_XDUMP, NULL);
	while (xdumpIndex >= 0)
	{
		if (agentNum >= MAX_DUMP_OPTS) {
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_TOO_MANY_DUMP_OPTIONS, MAX_DUMP_OPTS);
			return J9VMDLLMAIN_FAILED;
		}
		/* HeapDumpOnOutOfMemoryError before current -Xdump. */
		if (processXXHeapDump && (heapDumpIndex < xdumpIndex)) {
			/* process the -XX:[+-]HeapDumpOnOutOfMemoryError option first */
			if (enableXXHeapDump) {
				enableDumpOnOutOfMemoryError(agentOpts, &agentNum);
			} else {
				disableDumpOnOutOfMemoryError(agentOpts, agentNum);
			}
			processXXHeapDump = FALSE;
		}
		if ( IS_CONSUMABLE(j9vm_args, xdumpIndex) && !IS_CONSUMED(j9vm_args, xdumpIndex) )
		{
			BOOLEAN isMappedToolDump = FALSE;
			/* Handle mapped tool dump options */
			if (HAS_MAPPING(j9vm_args, xdumpIndex)) {
				char *mappingJ9Name = MAPPING_J9NAME(j9vm_args, xdumpIndex);
				char *toolString = ":tool:";
				char *toolCursor = strstr(mappingJ9Name, toolString);
				if (NULL != toolCursor) {
					char *optionValue = NULL;
					/* The mapped option specifies the tool command to run after the equals */
					GET_OPTION_VALUE(xdumpIndex, '=', &optionValue);
					
					/* Move toolCursor past ":tool:" */
					toolCursor += strlen(toolString);

					if (NULL != optionValue) {
						size_t toolCursorLength = strlen(toolCursor);
						size_t optionValueLength = strlen(optionValue);
						size_t optionStringMemAlloc = toolCursorLength + optionValueLength + 1;

						/* Construct optionString by combining the J9 tool dump command with the mapped option */
						optionString = (char *) j9mem_allocate_memory(optionStringMemAlloc, OMRMEM_CATEGORY_VM);
						
						if (NULL != optionString) {
							strcpy(optionString, toolCursor);
							strcat(optionString + toolCursorLength, optionValue);
							isMappedToolDump = TRUE;
						} else {
							char *mappingMapName = MAPPING_MAPNAME(j9vm_args, xdumpIndex);
							j9tty_err_printf(PORTLIB, "Unable to map %s to J9 %s - Could not allocate the requested size of memory %zu for optionString\n", mappingMapName, mappingJ9Name, optionStringMemAlloc);
							return J9VMDLLMAIN_FAILED;
						}
					}
				}
			} else {
				GET_OPTION_VALUE(xdumpIndex, ':', &optionString);
			}
			if (!optionString) {
				/* ... silent option ... */
			} else if( strncmp(optionString, "none", strlen("none") ) == 0 ){
				/* "none" found without any agent type, pretend we found all agents. */
				for (kind = 0; kind < ( (IDATA)j9RasDumpKnownSpecs ); kind++) {
					agentOpts[agentNum].kind = kind;
					agentOpts[agentNum].flags = J9RAS_DUMP_OPT_ARGS_STATIC;
					agentOpts[agentNum].args = optionString;
					agentOpts[agentNum].pass = J9RAS_DUMP_OPTS_PASS_ONE;
					agentNum++;
				}
			} else if (isMappedToolDump) {
				char * toolString = "tool";
				agentOpts[agentNum].kind = scanDumpType(&toolString);
				agentOpts[agentNum].flags = J9RAS_DUMP_OPT_ARGS_ALLOC;
				agentOpts[agentNum].args = optionString;
				agentOpts[agentNum].pass = J9RAS_DUMP_OPTS_PASS_ONE;
				agentNum++;
			} else {
				char *typeString = optionString;

				/* Find group dump settings */
				optionString += strcspn(typeString, ":");
				if (*optionString == ':') {optionString++;}

				/* Handle multiple dump types */
				while ( typeString < optionString && (kind = scanDumpType(&typeString)) >= 0 ) {
					if ( strcmp(optionString, "help") == 0 ) {
						printDumpSpec(vm, kind, 2);
						return J9VMDLLMAIN_SILENT_EXIT_VM;
					}
					agentOpts[agentNum].kind = kind;
					agentOpts[agentNum].flags = J9RAS_DUMP_OPT_ARGS_STATIC;
					agentOpts[agentNum].args = optionString;
					agentOpts[agentNum].pass = J9RAS_DUMP_OPTS_PASS_ONE;
					agentNum++;
				}

				/* Unprocessed dump type(s) remaining */
				if ( typeString < optionString ) {
					j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_UNRECOGNISED_OPTION_STR, typeString);
					printDumpUsage(vm);
					return J9VMDLLMAIN_SILENT_EXIT_VM;
				}
			}

			CONSUME_ARG(j9vm_args, xdumpIndex);
		}

		xdumpIndex = FIND_NEXT_ARG_IN_VMARGS_FORWARD(OPTIONAL_LIST_MATCH, VMOPT_XDUMP, NULL, xdumpIndex);
	}
	/* handle the case of no -Xdump options */
	if (processXXHeapDump) {
		if (enableXXHeapDump) {
			enableDumpOnOutOfMemoryError(agentOpts, &agentNum);
		} else {
			disableDumpOnOutOfMemoryError(agentOpts, agentNum);
		}
	}

	/* Process active agent options (L..R) */
	for (i = 0; i < agentNum; i++) {
		if (agentOpts[i].kind == J9RAS_DUMP_OPT_DISABLED) continue;
		if (agentOpts[i].pass != J9RAS_DUMP_OPTS_PASS_ONE) continue;

		/*j9tty_err_printf(PORTLIB, "configureDumpAgents() loading agent for %d %s\n",agentOpts[i].kind, agentOpts[i].args); */
		if ( (strncmp(agentOpts[i].args, "none", strlen("none")) == 0)) {
			if (deleteMatchingAgents(vm, agentOpts[i].kind, agentOpts[i].args) == OMR_ERROR_INTERNAL) {
				printDumpSpec(vm, agentOpts[i].kind, 2);
				return J9VMDLLMAIN_SILENT_EXIT_VM;
			}
		} else if ( strcmp(agentOpts[i].args, "defaults") == 0 ) {
			/* Matches "defaults" not "defaults:" */
			printDumpSpec(vm, agentOpts[i].kind, 1);
		} else {
#ifdef J9ZOS390
			processZOSDumpOptions(vm, agentOpts, i);
#else
			if (loadDumpAgent(vm, agentOpts[i].kind, agentOpts[i].args) == OMR_ERROR_INTERNAL) {
				printDumpSpec(vm, agentOpts[i].kind, 2);
				return J9VMDLLMAIN_SILENT_EXIT_VM;
			}
#endif
		}
	}

	/* Process active DEFAULT agent options (L..R) */
	for (i = 0; i < agentNum; i++) {
		if (agentOpts[i].kind == J9RAS_DUMP_OPT_DISABLED) continue;
		if (agentOpts[i].pass == J9RAS_DUMP_OPTS_PASS_ONE) continue;

		/*j9tty_err_printf(PORTLIB, "configureDumpAgents() loading agent for %d %s\n",agentOpts[i].kind, agentOpts[i].args); */
		if ( (strncmp(agentOpts[i].args, "none", strlen("none")) == 0)) {
			if (deleteMatchingAgents(vm, agentOpts[i].kind, agentOpts[i].args) == OMR_ERROR_INTERNAL) {
				printDumpSpec(vm, agentOpts[i].kind, 2);
				return J9VMDLLMAIN_SILENT_EXIT_VM;
			}
		} else {
			if (loadDumpAgent(vm, agentOpts[i].kind, agentOpts[i].args) == OMR_ERROR_INTERNAL) {
				printDumpSpec(vm, agentOpts[i].kind, 2);
				return J9VMDLLMAIN_SILENT_EXIT_VM;
			}
		}
	}

	/* Re-process active agent options (L..R) to do deletes and replace any options killed by a delete that
	 * preceded them. */
	for (i = 0; i < agentNum; i++) {
		if (agentOpts[i].kind == J9RAS_DUMP_OPT_DISABLED) continue;
		if (agentOpts[i].pass != J9RAS_DUMP_OPTS_PASS_ONE) continue;

		/*j9tty_err_printf(PORTLIB, "configureDumpAgents() loading agent for %d %s\n",agentOpts[i].kind, agentOpts[i].args); */
		if ( (strncmp(agentOpts[i].args, "none", strlen("none")) == 0)) {
			if (deleteMatchingAgents(vm, agentOpts[i].kind, agentOpts[i].args) == OMR_ERROR_INTERNAL) {
				printDumpSpec(vm, agentOpts[i].kind, 2);
				return J9VMDLLMAIN_SILENT_EXIT_VM;
			}
		} else {
#ifdef J9ZOS390
			processZOSDumpOptions(vm, agentOpts, i);
#else
			if (loadDumpAgent(vm, agentOpts[i].kind, agentOpts[i].args) == OMR_ERROR_INTERNAL) {
				printDumpSpec(vm, agentOpts[i].kind, 2);
				return J9VMDLLMAIN_SILENT_EXIT_VM;
			}
#endif
		}
	}

	if (showAgents) {
		showDumpAgents(vm);
	}
	
	storeDefaultData(vm);
	
	/* Free any allocated argument strings (used when we have a variable dump count) */
	for (i = 0; i < agentNum; i++) {
		if (agentOpts[i].flags == J9RAS_DUMP_OPT_ARGS_ALLOC) {
			j9mem_free_memory(agentOpts[i].args);
		}
	}
	j9mem_free_memory(agentOpts);

	return J9VMDLLMAIN_OK;
}


static omr_error_t
shutdownDumpAgents(J9JavaVM *vm)
{
	J9RASdumpQueue *queue;

	if ( FIND_DUMP_QUEUE(vm, queue) ) {
		J9RASdumpAgent * current = queue->agents;
		
		while (current) {
			J9RASdumpAgent * next = current->nextPtr;
			
			if (current->shutdownFn) {
				current->shutdownFn(vm, &current);	/* agent will remove itself */
			} else {
				removeDumpAgent(vm, current);
			}
			
			current = next;
		}
	}

	return OMR_ERROR_NONE;
}


static omr_error_t
printDumpUsage(J9JavaVM *vm)
{
	IDATA kind = 0;

	PORT_ACCESS_FROM_JAVAVM(vm);

	j9tty_err_printf(PORTLIB, "\nUsage:\n\n");

	j9tty_err_printf(PORTLIB, "  -Xdump:help             Print general dump help\n");
	j9tty_err_printf(PORTLIB, "  -Xdump:none             Ignore all previous/default dump options\n");
	j9tty_err_printf(PORTLIB, "  -Xdump:events           List available trigger events\n");
	j9tty_err_printf(PORTLIB, "  -Xdump:request          List additional VM requests\n");
	j9tty_err_printf(PORTLIB, "  -Xdump:tokens           List recognized label tokens\n");
	j9tty_err_printf(PORTLIB, "  -Xdump:dynamic          Enable support for pluggable agents\n");
	j9tty_err_printf(PORTLIB, "  -Xdump:what             Show registered agents on startup\n");
	j9tty_err_printf(PORTLIB, "  -Xdump:nofailover       Disable dump failover to temporary directory\n");
	j9tty_err_printf(PORTLIB, "  -Xdump:directory=<path> Set the default directory path for dump files to be written to\n");
#if defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL)
	j9tty_err_printf(PORTLIB, "  -Xdump:suspendwith=<num> Use SIGRTMIN+<num> to suspend threads\n");
#endif
	j9tty_err_printf(PORTLIB, "\n");
	j9tty_err_printf(PORTLIB, "  -Xdump:<type>:help      Print detailed dump help\n");
	j9tty_err_printf(PORTLIB, "  -Xdump:<type>:none      Ignore previous dump options of this type\n");
	j9tty_err_printf(PORTLIB, "  -Xdump:<type>:defaults  Print/update default settings for this type\n");
	j9tty_err_printf(PORTLIB, "  -Xdump:<type>           Request this type of dump (using defaults)\n");

	j9tty_err_printf(PORTLIB, "\nDump types:\n\n");

	/* Print dump specifications until all done */
	while (printDumpSpec(vm, kind++, 0) == OMR_ERROR_NONE) {}

	j9tty_err_printf(PORTLIB, "\nExample:\n\n");

	j9tty_err_printf(PORTLIB, "  java -Xdump:heap:none -Xdump:heap:events=fullgc class [args...]\n\n");
	j9tty_err_printf(PORTLIB, "Turns off default heapdumps, then requests a heapdump on every full GC.\n\n");

	return OMR_ERROR_NONE;
}


#if (defined(J9VM_RAS_DUMP_AGENTS))
omr_error_t
queryVmDump(struct J9JavaVM *vm, int buffer_size, void* options_buffer, int* data_size)
{
	J9RASdumpAgent* agent = NULL;
	char* tempBuf = NULL;
	IDATA numBytes = 1024;
	IDATA numBytesWritten = 0;
	IDATA writtenToBuffer = FALSE;
	IDATA foundDumpAgent = FALSE;
	omr_error_t rc = OMR_ERROR_NONE;
	
	PORT_ACCESS_FROM_JAVAVM(vm);
	
	if (NULL == data_size) {
		/* cannot write the data_size so abandon at this point. */
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	/* block until the config is available for use */
	lockConfigForUse();

	do {
		/* allocate an internal buffer that can hold the output */
		tempBuf = (char *)j9mem_allocate_memory(numBytes, OMRMEM_CATEGORY_VM);
		if (NULL == tempBuf) {
			/* memory allocation error has occurred */
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		} else {
			while (seekDumpAgent(vm, &agent, NULL) == OMR_ERROR_NONE)
			{
				foundDumpAgent = TRUE;
				writtenToBuffer = queryAgent(vm, agent, numBytes, tempBuf, &numBytesWritten);
				if (!writtenToBuffer) {
					break;
				}
			}
		}
		
		if (!foundDumpAgent) {
			/* failed to find a dump agent in the queue, so clean up and return */

			/* free our internal buffer */
			j9mem_free_memory(tempBuf);
			*data_size = 0;

			unlockConfig();
			return OMR_ERROR_NONE;
		}

		if (!writtenToBuffer) {
			/* double the allocation amount and try again */
			numBytes *= 2;
			numBytesWritten = 0;
			agent = NULL;
		} else {
			/* copy the memory into the user's buffer and then free our internal buffer */
			numBytesWritten++;
			
			if (buffer_size >= numBytesWritten && options_buffer != NULL) {
				/* do the copy */
				memcpy(options_buffer, tempBuf, numBytesWritten);
			} else {
				/* options_buffer is null or buffer_size too low */
				if (NULL == options_buffer) {
					rc = OMR_ERROR_ILLEGAL_ARGUMENT;
				} else {
					/* buffer_size too low */
					rc = OMR_ERROR_INTERNAL;
				}
			}
		}
		/* free our internal buffer */
		j9mem_free_memory(tempBuf);
	} while (!writtenToBuffer);

	*data_size = (int)numBytesWritten;

	unlockConfig();
	return rc;
}


omr_error_t
setDumpOption(struct J9JavaVM *vm, char *optionString)
{
	PORT_ACCESS_FROM_JAVAVM(vm);


	/* -Xdump:what */
	if ( strcmp(optionString, "what") == 0 )
	{
		/* prevent the configuration from changing under us while we inspect it */
		lockConfigForUse();
		showDumpAgents(vm);
	}
	/* -Xdump:none */
	else if ( strcmp(optionString, "none") == 0 )
	{
		if (lockConfigForUpdate()) {
			shutdownDumpAgents(vm);
		} else {
			return OMR_ERROR_NOT_AVAILABLE;
		}
	}
	else if (lockConfigForUpdate())
	{
		char *typeString = optionString;
		char *checkTypeString = typeString;
		IDATA kind;

		/* Find group dump settings */
		optionString += strcspn(typeString, ":");
		if (*optionString == ':') {optionString++;}

		/* Check all dump types are valid before processing each one. */
		while ( checkTypeString < optionString  )
		{
			kind = scanDumpType(&checkTypeString);
			/* Block bad dump types. (We can't do this later as we may get
			 * half way through setting up the dump agents before we find an
			 * invalid one and have partially set the configuration we were
			 * passed.
			 */
			if (J9RAS_DUMP_INVALID_TYPE == kind) {
				unlockConfig();
				return OMR_ERROR_INTERNAL;  /* Return unrecognized dump type error code. */
			}
		}

		/* Handle multiple dump types */
		while ( typeString < optionString && (kind = scanDumpType(&typeString)) >= 0 )
		{
			/* -Xdump:<agent>:none */
			if ( strcmp(optionString, "none") == 0 )
			{
				unloadDumpAgent(vm, kind);
			}
			else
			{
				omr_error_t rc = OMR_ERROR_NONE;
				if ( (strncmp(optionString, "none", strlen("none")) == 0)) {
					if (deleteMatchingAgents(vm, kind, optionString) != OMR_ERROR_NONE) {
						unlockConfig();
						return OMR_ERROR_INTERNAL;
					}
				} else if ((rc = loadDumpAgent(vm, kind, optionString)) != OMR_ERROR_NONE) {
					unlockConfig();
					return rc;
				}
			}
		}

		/* Unprocessed dump type(s) remaining */
		if ( typeString < optionString ) {
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_UNRECOGNISED_OPTION_STR, typeString);

			unlockConfig();
			return OMR_ERROR_INTERNAL;
		}
	} else {
		/* JVMTI and com.ibm.jvm.Dump use OMR_ERROR_NOT_AVAILABLE to know that a
		 * dump is in progress so the setting can't be changed. */
		return OMR_ERROR_NOT_AVAILABLE;
	}

	unlockConfig();

	return OMR_ERROR_NONE;
}
#endif /* J9VM_RAS_DUMP_AGENTS */

#if (defined(J9VM_RAS_DUMP_AGENTS)) 
omr_error_t
resetDumpOptions(struct J9JavaVM *vm)
{
	J9RASdumpQueue *queue = (J9RASdumpQueue *)vm->j9rasDumpFunctions;
    struct J9RASdumpSettings *origSettings = queue->settings;
    struct J9RASdumpAgent *origAgents = queue->agents;
    struct J9RASdumpSettings *origDefaultSettings = queue->defaultSettings;
    struct J9RASdumpAgent *origDefaultAgents = queue->defaultAgents;    

	PORT_ACCESS_FROM_JAVAVM(vm);

	if (!lockConfigForUpdate()) {
		/* can't obtain lock at this point in time */
		/* JVMTI and com.ibm.jvm.Dump use OMR_ERROR_NOT_AVAILABLE to know that a
		 * dump is in progress so the setting can't be changed. */
		return OMR_ERROR_NOT_AVAILABLE;
	}

	/* store the original queue on the shutdown queue */
	queue->settings = copyDumpSettingsQueue(vm, origDefaultSettings);
	if (queue->settings == NULL){
		unlockConfig();
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
	
	/* queue->defaultSettings and queue->defaultAgents do not change! */
	
	queue->agents = copyDumpAgentsQueue(vm, origDefaultAgents);
	if (queue->agents == NULL){
		struct J9RASdumpSettings *newSettings = queue->settings;
		/* restore the changes */
		queue->settings = origSettings;
		j9mem_free_memory(newSettings);
		queue->agents = origAgents;

		unlockConfig();
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
	
	/* add the old agents to the shutdown queue */
	if (queue->agentShutdownQueue == NULL){
		queue->agentShutdownQueue = origAgents;
	} else {
		struct J9RASdumpAgent *agent = queue->agentShutdownQueue;
		while (agent->nextPtr != NULL){
			agent = agent->nextPtr;
		}
		agent->nextPtr = origAgents;
	}
	
	/* free the old settings  */
	j9mem_free_memory(origSettings);

	unlockConfig();
	return OMR_ERROR_NONE;
}
#endif /* J9VM_RAS_DUMP_AGENTS */

static omr_error_t
pushDumpFacade(J9JavaVM *vm)
{
	J9RASdumpQueue *queue;
	omr_error_t retVal = OMR_ERROR_NONE;

	PORT_ACCESS_FROM_JAVAVM(vm);

	if ( (queue = (J9RASdumpQueue *)j9mem_allocate_memory(sizeof(J9RASdumpQueue), OMRMEM_CATEGORY_VM)) ) {

		memset( queue, 0, sizeof(*queue) );

		/* Add eyecatcher */
		queue->facade.reserved = DUMP_FACADE_KEY;

		queue->facade.triggerOneOffDump	= triggerOneOffDump;
		queue->facade.insertDumpAgent	= insertDumpAgent;
		queue->facade.removeDumpAgent	= removeDumpAgent;
		queue->facade.seekDumpAgent		= seekDumpAgent;
		queue->facade.triggerDumpAgents	= triggerDumpAgents;
		queue->facade.setDumpOption		= setDumpOption;
		queue->facade.resetDumpOptions	= resetDumpOptions;
		queue->facade.queryVmDump		= queryVmDump;

		/* Initialize default settings */
		queue->settings = initDumpSettings(vm);
		queue->defaultSettings = NULL;
		queue->defaultAgents = NULL;
		queue->agentShutdownQueue = NULL;

		/* Chain old facade */
		queue->oldFacade = vm->j9rasDumpFunctions;

		/* Install new facade */
		vm->j9rasDumpFunctions = (J9RASdumpFunctions *)queue;

		/* Note that we need to do the same check in popDumpFacade()
		 *
		 * According to the port library SIGABRT (J9PORT_SIG_FLAG_SIGABRT) is an ASYNC signal, so you'd think we should be checking for J9PORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS here.
		 *  However we want to disable the abortHandler when either of -Xrs / -Xrs:sync are present, so check for J9PORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS instead
         */
		if ( (j9sig_get_options() & J9PORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS) == 0 ) {
			/* Install RAS abort handler */
			installAbortHandler( vm );
		}

	} else {
		retVal = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	return retVal;
}


static omr_error_t
popDumpFacade(J9JavaVM *vm)
{
	J9RASdumpQueue *queue;

	if ( FIND_DUMP_QUEUE(vm, queue) )
	{
		PORT_ACCESS_FROM_JAVAVM(vm);
		/* Note that we need to do the same check in pushDumpFacade()
		 *
		 * According to the port library SIGABRT (J9PORT_SIG_FLAG_SIGABRT) is an ASYNC signal, so you'd think we should be checking for J9PORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS here.
		 *  However we want to disable the abortHandler when either of -Xrs / -Xrs:sync are present, so check for J9PORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS instead
         */
		if ( (j9sig_get_options() & J9PORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS) == 0 ) {
			/* Re-install application abort handler only if we installed our own */
			OMRSIG_SIGNAL(SIGABRT, SIG_DFL);
		}
		/* De-install new facade */
		vm->j9rasDumpFunctions = queue->oldFacade;

		/* Free settings */
		freeDumpSettings(vm, queue->settings);
		
		/* free our stored queue */
		if (queue->defaultSettings != NULL){
			j9mem_free_memory(queue->defaultSettings);
			queue->defaultSettings = NULL;
		}

		if (queue->defaultAgents != NULL){
			struct J9RASdumpAgent *currentAgent = queue->defaultAgents;
			while (currentAgent != NULL){
				struct J9RASdumpAgent *nextAgent = currentAgent->nextPtr;
				currentAgent->shutdownFn = NULL;
				j9mem_free_memory(currentAgent);
				currentAgent = nextAgent;
			}
			queue->defaultAgents = NULL;
		}
		
		if (queue->agentShutdownQueue != NULL){
			struct J9RASdumpAgent *currentAgent = queue->agentShutdownQueue;
			while (currentAgent != NULL){
				struct J9RASdumpAgent *nextAgent = currentAgent->nextPtr;
				currentAgent->shutdownFn = NULL;
				j9mem_free_memory(currentAgent);
				currentAgent = nextAgent;
			}
			queue->agentShutdownQueue = NULL;
		}
		
		j9mem_free_memory(queue);
	}

	return OMR_ERROR_NONE;
}

static void
initRasDumpGlobalStorage(J9JavaVM *vm)
{
	/* Create global storage for rasdump and populate it */
	
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA rc = 0;

	/* allocate the global storage */
	vm->j9rasdumpGlobalStorage = j9mem_allocate_memory(sizeof(RasDumpGlobalStorage), OMRMEM_CATEGORY_VM);

	if (NULL != vm->j9rasdumpGlobalStorage) {
		RasDumpGlobalStorage* dump_storage = (RasDumpGlobalStorage*)vm->j9rasdumpGlobalStorage;
		
		/* ensure that the storage is all NULLs to start with */
		memset (dump_storage, '\0', sizeof(RasDumpGlobalStorage));
		
		/* now allocate the mutex and the tokens */
		rc = omrthread_monitor_init_with_name(&dump_storage->dumpLabelTokensMutex, 0, "dump tokens mutex");
		if (0 == rc) {
			/* created the mutex */
			I_64 now = j9time_current_time_millis();
			
			omrthread_monitor_enter(dump_storage->dumpLabelTokensMutex);
			
			/* create the tokens */
			dump_storage->dumpLabelTokens = j9str_create_tokens(now);
			
			omrthread_monitor_exit(dump_storage->dumpLabelTokensMutex);
		} 
	}
}
	
static void
freeRasDumpGlobalStorage(J9JavaVM *vm)
{
	/* Free global storage for rasdump and also free its contents */
	PORT_ACCESS_FROM_JAVAVM(vm);
	RasDumpGlobalStorage *dump_storage = (RasDumpGlobalStorage *)vm->j9rasdumpGlobalStorage;
	vm->j9rasdumpGlobalStorage = NULL;
	
	if (NULL != dump_storage) {
		/* global storage exists. */

		/* free the contents */
		if (NULL != dump_storage->dumpLabelTokensMutex) {
			omrthread_monitor_destroy(dump_storage->dumpLabelTokensMutex);
		}
		
		if (NULL != dump_storage->dumpLabelTokens) {
			j9str_free_tokens(dump_storage->dumpLabelTokens);
		}

		/* now free the rasdump global storage */
		j9mem_free_memory(dump_storage);
	}
}
	
static void
hookVmInitialized(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMThread* vmThread = ((J9VMInitEvent *)eventData)->vmThread;

	vmThread->javaVM->internalVMFunctions->rasStartDeferredThreads(vmThread->javaVM);
}

jint JNICALL
JVM_OnLoad(JavaVM *vm, char *options, void *reserved)
{
	return JNI_OK;
}


jint JNICALL
JVM_OnUnload(JavaVM *vm, void *reserved)
{
	return JNI_OK;
}

/**
 * On Linux and OSX the first call to get a backtrace can cause some initialization
 * work. If this is called in a signal handler with other threads paused then one of
 * those can hold a lock required for the initialization to complete. This causes
 * a hang. Therefore we do one redundant call to backtrace at startup to prevent
 * java dumps hanging the VM.
 * 
 * See defect 183803
 */
static void
initBackTrace(J9JavaVM *vm)
{
#if defined(LINUX) || defined(OSX)
	J9PlatformThread threadInfo;
	J9Heap *heap;
	char backingStore[8096];
	
	PORT_ACCESS_FROM_JAVAVM(vm);
	
	/* Use a local heap so the memory used for the backtrace is freed automatically. */
	heap = j9heap_create(backingStore, sizeof(backingStore), 0);
	if( j9introspect_backtrace_thread(&threadInfo, heap, NULL) != 0 ) {
		j9introspect_backtrace_symbols(&threadInfo, heap);
	}
#endif /* defined(LINUX) || defined(OSX) */
}

/**
 * Gather system info that can be used in java dumps or retrieved from the J9RAS structure in
 * core dumps. The systemInfo field in J9RAS points to a linked list of J9RASSystemInfo structures
 * which store optional, platform specific information. This avoids cluttering J9RAS with fields
 * that are rarely used or only exist on some platforms.
 * 
 * @param vm[in] pointer to the JVM
 */
static void
initSystemInfo(J9JavaVM *vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9RASSystemInfo* systemInfo;

	J9RAS* rasStruct = vm->j9ras;
	if( NULL == rasStruct ) {
		return;
	}

	/* All platforms, store the OS hypervisor name, if any */
	{
		J9HypervisorVendorDetails details;
		IDATA rc = j9hypervisor_get_hypervisor_info(&details);
		if (rc == 0) {
			/* allocate an item for the linked list */
			systemInfo = (J9RASSystemInfo *) j9mem_allocate_memory(sizeof(J9RASSystemInfo), OMRMEM_CATEGORY_VM);
			if (NULL != systemInfo) {
				memset(systemInfo, '\0', sizeof(J9RASSystemInfo));
				systemInfo->key = J9RAS_SYSTEMINFO_HYPERVISOR;
				/* see j9hypervisor_get_hypervisor_info(), save the name string ptr (but don't free it later) */
				systemInfo->data = (void *)details.hypervisorName;
				J9_LINKED_LIST_ADD_LAST(rasStruct->systemInfo, systemInfo);
			}
		}
	}

#if defined(LINUX)
	/* On Linux, store the startup value of /proc/sys/kernel/sched_compat_yield if it's set */
	{
		char schedCompatYieldValue = j9util_sched_compat_yield_value(vm);
		/* See j9util_sched_compat_yield_value(), a space char is the null or error return */
		if (' ' != schedCompatYieldValue) {
			/* allocate an item for the linked list */
			systemInfo = (J9RASSystemInfo *) j9mem_allocate_memory(sizeof(J9RASSystemInfo), OMRMEM_CATEGORY_VM);
			if (NULL != systemInfo) {
				/* populate the J9RASSystemInfo item and add it to the linked list */
				memset(systemInfo, '\0', sizeof(J9RASSystemInfo));
				systemInfo->key = J9RAS_SYSTEMINFO_SCHED_COMPAT_YIELD;
				((char *)&systemInfo->data)[0] = schedCompatYieldValue;
				J9_LINKED_LIST_ADD_LAST(rasStruct->systemInfo, systemInfo);
			}
		}
	}
	appendSystemInfoFromFile(vm, J9RAS_SYSTEMINFO_CORE_PATTERN, J9RAS_CORE_PATTERN_FILE);
	appendSystemInfoFromFile(vm, J9RAS_SYSTEMINFO_CORE_USES_PID, J9RAS_CORE_USES_PID_FILE);
#endif /* defined(LINUX) */
}

/**
 * We need to read the -Xdump:directory option before we start initializing the
 * default dump agents.
 */
static omr_error_t
initDumpDirectory(J9JavaVM *vm)
{
	IDATA xdumpIndex = 0;
	omr_error_t retVal = OMR_ERROR_NONE;
	PORT_ACCESS_FROM_JAVAVM(vm);

	/* -Xdump:directory */
	xdumpIndex = FIND_AND_CONSUME_ARG(STARTSWITH_MATCH, VMOPT_XDUMP ":directory", NULL);
	if ( xdumpIndex >= 0 )
	{
		char *optionString = NULL;
		GET_OPTION_VALUE(xdumpIndex, '=', &optionString);
		if( !optionString ) {
			printDumpUsage(vm);
			return OMR_ERROR_INTERNAL;
		} else {
			dumpDirectoryPrefix = (char *)j9mem_allocate_memory(strlen(optionString)+1, OMRMEM_CATEGORY_VM);
			if( dumpDirectoryPrefix != NULL ) {
				j9str_printf(PORTLIB, dumpDirectoryPrefix, strlen(optionString)+1, "%s", optionString);
			} else {
				retVal = OMR_ERROR_INTERNAL;
			}
		}
	} else {
		dumpDirectoryPrefix = NULL;
		retVal = OMR_ERROR_NONE;
	}
	return retVal;
}

#if defined(LINUX)
/* Adds a J9RASSystemInfo to the end of the system info list using the key
 * specified as the key and the data from the specified file in /proc if
 * it exists.
 *
 * @param[in]	vm			pointer to J9JavaVM
 * @param[out]	key			J9RAS_SYSTEMINFO_ key from rasdump_internal.h
 * @param[in]	procFileName	the file in /proc to read the value from
 *
 * @return:
 *	nothing
 */
static void
appendSystemInfoFromFile(J9JavaVM *vm, U_32 key, const char *fileName )
{

	IDATA fd = -1;
	J9RAS* rasStruct = vm->j9ras;
	PORT_ACCESS_FROM_JAVAVM(vm);

	if( NULL == rasStruct ) {
		return;
	}

	fd = j9file_open(fileName, EsOpenRead, 0);
	if (fd != -1) {
		/* Files in /proc report length as 0 but we don't expect the contents to be more than 1 line.
		 * We take the first line or 80 characters that should be enough information to put in javacore */
		char buf[80];
		char* read = NULL;
		read = j9file_read_text(fd, &buf[0], 80);
		if( read == &buf[0] ) {
			J9RASSystemInfo* systemInfo;
			size_t bufLen = 0;
			/* Make sure the string is only one line and null terminated. */
			for( bufLen = 0; bufLen < 80; bufLen++) {
				if( read[bufLen] == '\n') {
					read[bufLen] = '\0';
					break;
				}
			}
			/* Allocate systemInfo and systemInfo->data in one go so we can free them in rasdump.c:J9RASShutdown
			 * without having to track whether or not we did an allocation for systemInfo->data.
			 */
			systemInfo = (J9RASSystemInfo *) j9mem_allocate_memory(sizeof(J9RASSystemInfo) + bufLen + 1, OMRMEM_CATEGORY_VM);
			if( systemInfo != NULL ) {
				memset(systemInfo, '\0', sizeof(J9RASSystemInfo) + bufLen + 1);
				systemInfo->key = key;
				/* Allocated with systemInfo, data is right after systemInfo. */
				systemInfo->data = &systemInfo[1];
				memcpy(systemInfo->data, read, bufLen + 1 );
				J9_LINKED_LIST_ADD_LAST(rasStruct->systemInfo, systemInfo);
			}
		}
		j9file_close(fd);
	}
}
#endif /* defined(LINUX) */

IDATA
J9VMDllMain(J9JavaVM *vm, IDATA stage, void *reserved)
{
	IDATA retVal = J9VMDLLMAIN_OK;
	omr_error_t rc = OMR_ERROR_NONE;
	J9VMDllLoadInfo* loadInfo;
	RasGlobalStorage *tempRasGbl;
	J9HookInterface** hook = vm->internalVMFunctions->getVMHookInterface(vm);
	
	PORT_ACCESS_FROM_JAVAVM(vm);

	switch (stage)
	{
		case PORT_LIBRARY_GUARANTEED :

#if defined(J9VM_PORT_OMRSIG_SUPPORT) && defined(WIN32)
			loadOMRSIG(vm);
#endif /* defined(J9VM_PORT_OMRSIG_SUPPORT) && defined(WIN32) */
			initBackTrace(vm);
			initSystemInfo(vm);
			/* The default dump paths are set by fixDumpLabel in initDumpSettings called
			 * from pushDumpFacade so we need to process -Xdump:directory first and the
			 * error handling is best done here.
			 */
			if (OMR_ERROR_NONE != initDumpDirectory(vm)) {
				retVal = J9VMDLLMAIN_FAILED;
			} else {
				/* Defer enabling dump agents until initRasDumpGlobalStorage() is finished
				 * so as to avoid triggering dump agent before RAS is ready for use.
				 */
				initRasDumpGlobalStorage(vm);

				/* Swap in new dump facade */
				if (OMR_ERROR_NONE == pushDumpFacade(vm)) {
					retVal = configureDumpAgents(vm);
					/* Allow configuration updates now that we've done initial setup */
					unlockConfig();
				} else {
					freeRasDumpGlobalStorage(vm);
					retVal = J9VMDLLMAIN_FAILED;
				}
			}
			break;

		case ALL_LIBRARIES_LOADED:

			if (vm->j9rasGlobalStorage == NULL) {
				/* RAS init may happen in either dump or trace */
				vm->j9rasGlobalStorage = j9mem_allocate_memory(sizeof(RasGlobalStorage), OMRMEM_CATEGORY_VM);
				if (vm->j9rasGlobalStorage != NULL) {
					memset (vm->j9rasGlobalStorage, '\0', sizeof(RasGlobalStorage));
				}
			}
			break;

		case TRACE_ENGINE_INITIALIZED:

			if (((RasGlobalStorage *)vm->j9rasGlobalStorage)->jvmriInterface == NULL) {
				/* JVMRI init may happen in either dump or trace */
				((RasGlobalStorage *)vm->j9rasGlobalStorage)->jvmriInterface = j9mem_allocate_memory(sizeof(DgRasInterface), OMRMEM_CATEGORY_VM);
				if (((RasGlobalStorage *)vm->j9rasGlobalStorage)->jvmriInterface == NULL) {
					j9tty_err_printf(PORTLIB, "Storage for jvmri interface not available, trace not enabled\n");
					return J9VMDLLMAIN_FAILED;
				}

				if ((vm->internalVMFunctions->fillInDgRasInterface( ((RasGlobalStorage *)vm->j9rasGlobalStorage)->jvmriInterface )) != JNI_OK){
					j9tty_err_printf(PORTLIB, "Error initializing jvmri interface not available, trace not enabled\n");
					return J9VMDLLMAIN_FAILED;
				}

				if ((vm->internalVMFunctions->initJVMRI(vm)) != JNI_OK){
					j9tty_err_printf(PORTLIB, "Error initializing jvmri interface, trace not enabled\n");
					return J9VMDLLMAIN_FAILED;
				}
				
				if ((*hook)->J9HookRegisterWithCallSite(hook, J9HOOK_VM_INITIALIZED, hookVmInitialized, OMR_GET_CALLSITE(), NULL)) {
					j9tty_err_printf(PORTLIB, "Trace engine failed to hook VM events, trace not enabled\n");
					return J9VMDLLMAIN_FAILED;
				}
			}
			/* Register GC event hooks that were deferred from earlier dump agent processing */
			rasDumpFlushHooks(vm, stage);
			break;
			
		case JIT_INITIALIZED :
				/* Register this module with trace */
				UT_MODULE_LOADED(J9_UTINTERFACE_FROM_VM(vm));
				Trc_dump_J9VMDllMain_Event1(vm);
				break;
			
		case VM_INITIALIZATION_COMPLETE:
			/* Register thread event hooks that were deferred from earlier dump agent processing */
			rasDumpFlushHooks(vm, stage);
			break;
		
		case GC_SHUTDOWN_COMPLETE :

			/* Replace old dump facade */
			rc = shutdownDumpAgents(vm);
			if (OMR_ERROR_NONE == rc) {
				retVal = J9VMDLLMAIN_OK;
			} else {
				retVal = J9VMDLLMAIN_FAILED;
			}
			popDumpFacade(vm);
#if defined(J9VM_PORT_OMRSIG_SUPPORT) && defined(WIN32)
			unloadOMRSIG(vm->portLibrary);
#endif /* defined(J9VM_PORT_OMRSIG_SUPPORT) && defined(WIN32) */
			break;

		case ABOUT_TO_BOOTSTRAP :
			break;

		case INTERPRETER_SHUTDOWN:
			Trc_dump_J9VMDllMain_Event2(vm);
			freeRasDumpGlobalStorage(vm);
			loadInfo = FIND_DLL_TABLE_ENTRY( J9_RAS_TRACE_DLL_NAME );
			if((loadInfo->loadFlags & LOADED) == 0) {
				/* If RasTrace created the JVMRI struct, let it destroy it. Otherwise it's our job. */ 
				if (vm->j9rasGlobalStorage != NULL) {
					tempRasGbl = (RasGlobalStorage *)vm->j9rasGlobalStorage;
					vm->j9rasGlobalStorage = NULL;

					if ( tempRasGbl->jvmriInterface != NULL ){
						j9mem_free_memory( tempRasGbl->jvmriInterface );
					}
					j9mem_free_memory( tempRasGbl );
				}
			}
			break;
			
		default :
			break;
	}

	return retVal;
}

#ifdef J9ZOS390
static IDATA
processZOSDumpOptions(J9JavaVM *vm, J9RASdumpOption* agentOpts, int optIndex)
{
	char* ceedump = NULL;
	char* ieatdump = NULL;
	char* typeString = "system";
	IDATA i = optIndex;
	IDATA kind;
	char* opts;
	int argsLen;


	kind = scanDumpType(&typeString);
	if (agentOpts[i].kind != kind) {
		/* not the -Xdump:system case */
		if (loadDumpAgent(vm, agentOpts[i].kind, agentOpts[i].args) == OMR_ERROR_INTERNAL) {
			printDumpSpec(vm, agentOpts[i].kind, 2);
			return J9VMDLLMAIN_SILENT_EXIT_VM;
		} else {
			return J9VMDLLMAIN_OK;
		}
	}
	
	/* handling the -Xdump:system case */
	argsLen = strlen(agentOpts[i].args);

	/* check for the options */
	opts = strstr(agentOpts[i].args, "opts=");
	if (NULL != opts) {
		/* just strip out the opts from the arguments */
		char* endOpts;
		int optsLen;
		int moveLen;

		ceedump = strstr(opts, "CEEDUMP");
		ieatdump = strstr(opts, "IEATDUMP");

		endOpts = strstr(opts, ",");

		if (NULL != endOpts) {
			/* something comes after this value */
			optsLen = endOpts - opts + 1;
			moveLen = strlen(endOpts);

			/* now remove the opts with a memmove */
			memmove(opts, opts + optsLen, moveLen);
			memset(opts + moveLen, '\0', optsLen);
		} else {
			/* this option is the last one */
			/* adjust opts to contain the previous separator */
			opts--;
			optsLen = strlen(opts);

			/* now remove the opts by overwriting with nulls */
			memset(opts, '\0', optsLen);
		}
	}
	
	if (NULL == ceedump && NULL == ieatdump) {
		ieatdump = (char*)-1;
	}
	
	if (ceedump) {
		int ceedumpKind;
		
		typeString = "ceedump";
		ceedumpKind = scanDumpType(&typeString);
		
		if (loadDumpAgent(vm, ceedumpKind, agentOpts[i].args) == OMR_ERROR_INTERNAL) {
			printDumpSpec(vm, ceedumpKind, 2);
			return J9VMDLLMAIN_SILENT_EXIT_VM;
		}
	}

	if (ieatdump) {
		if (loadDumpAgent(vm, agentOpts[i].kind, agentOpts[i].args) == OMR_ERROR_INTERNAL) {
			printDumpSpec(vm, agentOpts[i].kind, 2);
			return J9VMDLLMAIN_SILENT_EXIT_VM;
		}
	}

	return J9VMDLLMAIN_OK;
}
#endif

