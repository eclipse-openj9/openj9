/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#include "j9protos.h"
#include "jvminit.h"

#include "j9rastrace.h"
#include "ute.h"

#define  _UTE_STATIC_
#include "jvmri.h"
#include "j9trcnls.h"

#include "ut_j9trc.h"

#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_mt.h"

#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_j9trc_aux.h"

#include "j2sever.h"

#include <string.h>

#define VMOPT_XTRACE            "-Xtrace"

#define UT_MAX_OPTS 55

#define DUMP_CALLER_NAME "-Xtrace:trigger"

static void displayTraceHelp (J9JavaVM *vm);
static IDATA initializeTraceOptions (J9JavaVM *vm, char* opts[]);
static UtInterface *initializeUtInterface(void);
jint JNICALL JVM_OnUnload(JavaVM *vm, void *reserved);
static void requestTraceCleanup(J9JavaVM* vm, J9VMThread * vmThread);
static void reportVMTermination (J9JavaVM* vm, J9VMThread * vmThread);
jint JNICALL JVM_OnLoad(JavaVM *vm, char *options, void *reserved);
static IDATA parseTraceOptions (J9JavaVM *vm,const char *optionString, IDATA optionsLength);
static omr_error_t runtimeSetTraceOptions(J9VMThread * thr,const char * traceOptions);

static void hookThreadAboutToStart (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void hookVmInitialized (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static char *threadName(OMR_VMThread* vmThread);
static omr_error_t populateTraceHeaderInfo(J9JavaVM *vm);
static BOOLEAN checkAndSetL2EnabledFlag(void);
static void hookThreadEnd (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static omr_error_t reportTraceEvent(J9JavaVM *vm, J9VMThread *self, UDATA eventFlags);
static IDATA splitCommandLineOption(J9JavaVM *vm, const char *optionString, IDATA optionLength, char *opt[]);
static omr_error_t setJ9VMTraceOption(const OMR_VM *omr_vm, const char *optName, const char* optValue, BOOLEAN atRuntime);
static void reportJ9VMCommandLineError(J9PortLibrary* portLibrary, const char* detailStr, va_list args);
static void printTraceWhat(J9PortLibrary* portLibrary);

static void doTriggerActionJavadump(OMR_VMThread *);
static void doTriggerActionCoredump(OMR_VMThread *);
static void doTriggerActionHeapdump(OMR_VMThread *);
static void doTriggerActionSnap(OMR_VMThread *);
static void doTriggerActionCeedump(OMR_VMThread *);
static void doTriggerActionAssertDumpEvent(OMR_VMThread *omrThr);

static void trcStartupComplete(J9VMThread* thr);

/* Trace functions for Java version of UtModuleInterface
 * Used by modules that declare Environment=Java at the top of
 * their TDF files.
 */
static void javaTrace(void *env, UtModuleInfo *modInfo, U_32 traceId, const char *spec, ...);
static void javaTraceMem (void *env, UtModuleInfo *modInfo, U_32 traceId, UDATA length, void *memptr);
static void javaTraceState (void *env, UtModuleInfo *modInfo, U_32 traceId, const char *spec, ...);
static void javaTraceInit (void* env, UtModuleInfo *modInfo);
static void javaTraceTerm (void* env, UtModuleInfo *modInfo);

typedef omr_error_t (*traceOptionFunction)(J9JavaVM *vm,const char * value,BOOLEAN atRuntime);

/**
 * Structure representing a trace option such as stackdepth
 * or trigger
 */
struct traceOption {
	const char * name;
	const BOOLEAN runtimeModifiable;
	const traceOptionFunction optionFunction;
};

/**
 * Additional trace options supported by the Java layer to
 * supply method trace functionality.
 * (Set the methods to be traced, set the depth and the compression options
 * for the stacks produced by the jstacktrace trigger action.)
 */
const struct traceOption TRACE_OPTIONS[] =
{
	/* Name, Can be modified at runtime, option function */
	{RAS_METHODS_KEYWORD, FALSE, setMethod},
	{RAS_STACKDEPTH_KEYWORD, TRUE, setStackDepth},
	{RAS_COMPRESSION_LEVEL_KEYWORD, TRUE, setStackCompressionLevel},
};

#define NUMBER_OF_TRACE_OPTIONS ( sizeof(TRACE_OPTIONS) / sizeof(struct traceOption))

/**
 * The list of trigger actions defined by J9VM passed to the
 * OMR trace engine so that non-Java trace events (like a basic
 * trace point hit) can fire Java dependent events (like creating
 * a javacore).
 */
static struct RasTriggerAction j9vmTriggerActions[] =
{
	{ "jstacktrace", AFTER_TRACEPOINT, doTriggerActionJstacktrace},
	{ "javadump", AFTER_TRACEPOINT, doTriggerActionJavadump},
	{ "coredump", AFTER_TRACEPOINT, doTriggerActionCoredump},
	{ "sysdump", AFTER_TRACEPOINT, doTriggerActionCoredump},
	{ "heapdump", AFTER_TRACEPOINT, doTriggerActionHeapdump},
	{ "snap", AFTER_TRACEPOINT, doTriggerActionSnap},
	{ "ceedump", AFTER_TRACEPOINT, doTriggerActionCeedump},
	/*
	 * Temporary action to allow asserts to trigger the assert
	 * dump event without the OMR'd trace code being able to
	 * access the dump code.
	 * Will be removed when the dump code moves to OMR.
	 * * See work item: 64106
	 */
	{ "assert", AFTER_TRACEPOINT, doTriggerActionAssertDumpEvent},
};

#define NUM_J9VM_TRIGGER_ACTIONS (sizeof(j9vmTriggerActions) / sizeof(j9vmTriggerActions[0]))

static const struct RasTriggerType methodTriggerType = { "method", processTriggerMethodClause, FALSE };

/* Global lock for J9VM trace operations. */
omrthread_monitor_t j9TraceLock = NULL;

/* Structures for trace interfaces. */
static J9UtServerInterface  javaUtServerIntfS;
static UtModuleInterface    javaUtModuleIntfS;

static UtModuleInterface    omrUtModuleIntfS;
/* Required to look up the current thread if we are passed a null current thread on a trace point.
 * (NoEnv trace points always pass null.)
 */
J9JavaVM* globalVM;

IDATA
J9VMDllMain(J9JavaVM *vm, IDATA stage, void *reserved)
{
	IDATA returnVal = J9VMDLLMAIN_OK;
	omr_error_t rc = OMR_ERROR_NONE;

	char *ignore[] = { "INITIALIZATION", "METHODS", "WHAT", "STACKDEPTH", "STACKCOMPRESSIONLEVEL", NULL };
	char *opts[UT_MAX_OPTS];
	int i;
	UtThreadData **tempThr = NULL;
	J9VMThread *thr;
	RasGlobalStorage *tempRasGbl;
	void ** tempGbl;
	char tempPath[MAX_IMAGE_PATH_LENGTH];
	char *javahome = NULL;
	J9VMSystemProperty * javaHomeProperty = NULL;
	UDATA getPropertyResult;
	J9HookInterface** hook = vm->internalVMFunctions->getVMHookInterface(vm);
	OMRTraceLanguageInterface languageIntf;

	PORT_ACCESS_FROM_JAVAVM( vm );

	switch(stage) {
	case ALL_LIBRARIES_LOADED:
		if (vm->j9rasGlobalStorage == NULL) {
			/* RAS init may happen in either dump or trace */
			vm->j9rasGlobalStorage = j9mem_allocate_memory(sizeof(RasGlobalStorage), OMRMEM_CATEGORY_TRACE);
			if (vm->j9rasGlobalStorage != NULL) {
				memset (vm->j9rasGlobalStorage, '\0', sizeof(RasGlobalStorage));
			}
		}

		if (vm->j9rasGlobalStorage != NULL) {
			vm->runtimeFlags |= J9_RUNTIME_EXTENDED_METHOD_BLOCK;
			RAS_GLOBAL_FROM_JAVAVM(stackdepth,vm) = -1;
			((RasGlobalStorage *)vm->j9rasGlobalStorage)->configureTraceEngine = (ConfigureTraceFunction)runtimeSetTraceOptions;
		}

		break;
	case TRACE_ENGINE_INITIALIZED:
		if (vm->j9rasGlobalStorage == NULL) {
			 /* Storage for RasGlobalStorage not available, trace not enabled */
			 j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_RAS_GLOBAL_STORAGE);
			 return J9VMDLLMAIN_FAILED;
		}
		/*
		 *  Ensure that method entry and exit hooks are honoured
		 */
		vm->globalEventFlags |= J9_METHOD_ENTER_RASTRACE;
		thr = vm->mainThread;
		do {
				thr->eventFlags |= J9_METHOD_ENTER_RASTRACE;
				thr = thr->linkNext;
		} while (thr != vm->mainThread);

		if (0 != omrthread_monitor_init_with_name(&j9TraceLock, 0, "J9VM Trace Lock")) {
			dbg_err_printf(1, PORTLIB, "<UT> Initialization of j9TraceLock failed\n");
			return J9VMDLLMAIN_FAILED;
		}

		/*
		 *  Find the java.home property
		 */
		getPropertyResult = vm->internalVMFunctions->getSystemProperty(vm, "java.home", &javaHomeProperty);

		if ( getPropertyResult != J9SYSPROP_ERROR_NOT_FOUND
				&& javaHomeProperty != NULL
				&& javaHomeProperty->value != NULL
			) {
			javahome = javaHomeProperty->value;
		} else {
			javahome = ".";
		}

		/*
		 *  Setup the interfaces, any functions we wish to substitute can be done here before assigning
		 *  utIntf to our global pointer.
		 */
		globalVM = vm;
		((RasGlobalStorage *)vm->j9rasGlobalStorage)->utIntf = initializeUtInterface();

		/*
		 *  Set up the early options
		 *  These will be copied when the options
		 *  are processed so it's Ok to use stack space.
		 */
		opts[0] = UT_FORMAT_KEYWORD;

		j9str_printf(PORTLIB, tempPath, sizeof(tempPath), "%s" DIR_SEPARATOR_STR "lib;.", javahome);

		opts[1] = tempPath;
		opts[2] = NULL;

		/*
		 * Find UTE thread slot inside VM thread structure
		 */
		tempThr = UT_THREAD_FROM_VM_THREAD(vm->mainThread);
		tempRasGbl = (RasGlobalStorage *)vm->j9rasGlobalStorage;
		tempGbl = &tempRasGbl->utGlobalData;


		/* Add Java trace trigger types to the default OMR set */
		rc = addTriggerType(thr->omrVMThread, &methodTriggerType);
		if (OMR_ERROR_NONE != rc) {
			/* Trace engine failed to initialize properly, RC = %d */
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_TRACE_INIT_FAILED_RC, returnVal);
			return J9VMDLLMAIN_FAILED;
		}

		/* Add Java trace trigger actions to the default OMR set */
		for (i = 0; i < NUM_J9VM_TRIGGER_ACTIONS; i++ ) {
			rc = addTriggerAction(thr->omrVMThread, &j9vmTriggerActions[i]);
			if (OMR_ERROR_NONE != rc) {
				/* Trace engine failed to initialize properly, RC = %d */
				j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_TRACE_INIT_FAILED_RC, returnVal);
				return J9VMDLLMAIN_FAILED;
			}
		}

		/* init the language interface */
		languageIntf.AttachCurrentThreadToLanguageVM = OMR_Glue_BindCurrentThread;
		languageIntf.DetachCurrentThreadFromLanguageVM = OMR_Glue_UnbindCurrentThread;
		languageIntf.SetLanguageTraceOption = setJ9VMTraceOption;
		languageIntf.ReportCommandLineError = reportJ9VMCommandLineError;

		/* UT_DEBUG option parsed in here. UT_DEBUG can be used from here onwards. */
		rc = initializeTrace(tempThr, tempGbl, (const char **)opts, vm->omrVM, (const char **)ignore, &languageIntf);
		if (OMR_ERROR_NONE != rc) {
			/* Trace engine failed to initialize properly, RC = %d */
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_TRACE_INIT_FAILED_RC, returnVal);
			return J9VMDLLMAIN_FAILED;
		}

		/* Clear the early options set above before we re-use opts */
		opts[0] = NULL;
		opts[1] = NULL;

		if (OMR_ERROR_NONE != (threadStart(tempThr, vm->mainThread, "Initialization thread", vm->mainThread->osThread, vm->mainThread->omrVMThread))) {
			dbg_err_printf(1, PORTLIB, "<UT> Trace engine failed to register initialization thread, trace not enabled\n");
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_TRACE_INIT_FAILED);
			return J9VMDLLMAIN_FAILED;
		}

		if ((returnVal = initializeTraceOptions(vm, opts)) != J9VMDLLMAIN_OK) {
			return returnVal;
		}


		for (i = 0; opts[i] != NULL; i += 2) {
			if (j9_cmdla_stricmp(opts[i], "HELP") == 0) {
				displayTraceHelp(vm);
				return J9VMDLLMAIN_SILENT_EXIT_VM;
			}
		}


		registerj9trc_auxWithTrace( ((RasGlobalStorage *)vm->j9rasGlobalStorage)->utIntf, NULL);
		if( NULL == j9trc_aux_UtModuleInfo.intf ) {
			dbg_err_printf(1, PORTLIB, "<UT> Trace engine failed to register j9trc_aux module, trace not enabled\n");
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_TRACE_INIT_FAILED);
			return J9VMDLLMAIN_FAILED;
		}

		if (OMR_ERROR_NONE != setOptions(tempThr, (const char **)opts, FALSE)) {
			displayTraceHelp(vm);
			return J9VMDLLMAIN_SILENT_EXIT_VM;
		}

		/* needs to occur after setOptions because setOptions sets the default opts! */
		for (i = 0; opts[i] != NULL; i += 2) {
			if (j9_cmdla_stricmp(opts[i], "WHAT") == 0) {
				printTraceWhat(PORTLIB);
				/* just print this out once then carry on, do not exit vm */
				break;
			}
		}

		/*
		 *  Free up allocated options
		 */
		for (i = 0; opts[i] != NULL; i += 2) {
			j9mem_free_memory(opts[i]);
			if (opts[i + 1] != NULL) {
				j9mem_free_memory(opts[i + 1]);
			}
		}

		if (((RasGlobalStorage *)vm->j9rasGlobalStorage)->jvmriInterface == NULL) {
			/* JVMRI init may happen in either dump or trace */
			((RasGlobalStorage *)vm->j9rasGlobalStorage)->jvmriInterface = j9mem_allocate_memory(sizeof(DgRasInterface), OMRMEM_CATEGORY_TRACE);
			if (((RasGlobalStorage *)vm->j9rasGlobalStorage)->jvmriInterface == NULL) {
				dbg_err_printf(1, PORTLIB, "<UT> Storage for jvmri interface not available, trace not enabled\n");
				j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_TRACE_INIT_FAILED);
				return J9VMDLLMAIN_FAILED;
			}

			if (JNI_OK != vm->internalVMFunctions->fillInDgRasInterface( ((RasGlobalStorage *)vm->j9rasGlobalStorage)->jvmriInterface )) {
				dbg_err_printf(1, PORTLIB, "<UT> Error initializing jvmri interface not available, trace not enabled\n");
				j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_TRACE_INIT_FAILED);
				return J9VMDLLMAIN_FAILED;
			}
		}

		if (JNI_OK != vm->internalVMFunctions->initJVMRI(vm)) {
			dbg_err_printf(1, PORTLIB, "<UT> Error initializing jvmri interface, trace not enabled\n");
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_TRACE_INIT_FAILED);
			return J9VMDLLMAIN_FAILED;
		}

		registerj9trcWithTrace(((RasGlobalStorage *)vm->j9rasGlobalStorage)->utIntf, NULL);
		if( NULL == j9trc_UtModuleInfo.intf ) {
			dbg_err_printf(1, PORTLIB, "<UT> Trace engine failed to register main module, trace not enabled\n");
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_TRACE_INIT_FAILED);
			return J9VMDLLMAIN_FAILED;
		}
		Trc_trcengine_J9VMDllMain_Event1(vm);

		registermtWithTrace(((RasGlobalStorage *)vm->j9rasGlobalStorage)->utIntf, NULL);
		if( NULL == mt_UtModuleInfo.intf ) {
			dbg_err_printf(1, PORTLIB, "<UT> Trace engine failed to register method trace module, trace not enabled\n");
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_TRACE_INIT_FAILED);
			return J9VMDLLMAIN_FAILED;
		}

		/* new hook interface */
		if ((*hook)->J9HookRegisterWithCallSite(hook, J9HOOK_VM_INITIALIZED, hookVmInitialized, OMR_GET_CALLSITE(), NULL)) {
			dbg_err_printf(1, PORTLIB, "<UT> Trace engine failed to hook VM initialized event, trace not enabled\n");
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_TRACE_INIT_FAILED);
			return J9VMDLLMAIN_FAILED;
		}

		if ((*hook)->J9HookRegisterWithCallSite(hook, J9HOOK_THREAD_ABOUT_TO_START, hookThreadAboutToStart, OMR_GET_CALLSITE(), NULL)) {
			dbg_err_printf(1, PORTLIB, "<UT> Trace engine failed to hook VM thread start event, trace not enabled\n");
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_TRACE_INIT_FAILED);
			return J9VMDLLMAIN_FAILED;
		}

		if ((*hook)->J9HookRegisterWithCallSite(hook, J9HOOK_VM_THREAD_END, hookThreadEnd, OMR_GET_CALLSITE(), NULL)) {
			dbg_err_printf(1, PORTLIB, "<UT> Trace engine failed to hook VM thread end event, trace not enabled\n");
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_TRACE_INIT_FAILED);
			return J9VMDLLMAIN_FAILED;
		}

		if ( ((RasGlobalStorage *)vm->j9rasGlobalStorage)->traceMethodTable != NULL ||
		     ((RasGlobalStorage *)vm->j9rasGlobalStorage)->triggerOnMethods != NULL) {
			if (OMR_ERROR_INTERNAL == enableMethodTraceHooks(vm)) {
				dbg_err_printf(1, PORTLIB, "<UT> Trace engine failed to hook VM Method events, trace not enabled\n");
				j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_TRACE_INIT_FAILED);
				return J9VMDLLMAIN_FAILED;
			}
		}

		 /*
		  * Set up an early version of trace header info
		  */
		if (OMR_ERROR_NONE != populateTraceHeaderInfo(vm)) {
			return J9VMDLLMAIN_FAILED;
		}

		/* initialize tracing in the port library as soon as possible */
		j9port_control(J9PORT_CTLDATA_TRACE_START, (UDATA) ((RasGlobalStorage *)vm->j9rasGlobalStorage)->utIntf);

		/* initialize tracing in the thread library */
		omrthread_lib_control(J9THREAD_LIB_CONTROL_TRACE_START, (UDATA) ((RasGlobalStorage *)vm->j9rasGlobalStorage)->utIntf);

		/* Load module for hookable library tracepoints */
		omrhook_lib_control(J9HOOK_LIB_CONTROL_TRACE_START, (UDATA) ((RasGlobalStorage *)vm->j9rasGlobalStorage)->utIntf);

		break;

	case VM_INITIALIZATION_COMPLETE:
		tempThr = UT_THREAD_FROM_VM_THREAD(vm->mainThread);
#ifdef J9VM_OPT_SIDECAR
		if (J2SE_VERSION(vm)) {
			/* Force loading of the Trace class. See defect 162723, this is required because of some nuance
			 * in the early initialization of DB/2 (which uses the Trace class).
			 */
			JNIEnv * env = (JNIEnv *)vm->mainThread;
			(*env)->FindClass(env, "com/ibm/jvm/Trace");
			/* fail silently if can't load - probably a small platform */
			(*env)->ExceptionClear(env);
		}
#endif
		/* overwrite the header settings now that we have full vm->j2seVersion info */
		if (OMR_ERROR_NONE != populateTraceHeaderInfo(vm)) {
			return J9VMDLLMAIN_FAILED;
		}

		/* now force the main thread to start */
		reportTraceEvent(vm, vm->mainThread, J9RAS_TRACE_ON_THREAD_CREATE);
		break;

	case JVM_EXIT_STAGE:
		/* CMVC 108664 - do not free trace resources on System.exit() */
		if (NULL != ((RasGlobalStorage *)vm->j9rasGlobalStorage)->utIntf) {
			thr = vm->internalVMFunctions->currentVMThread(vm);
			/* stop tracing the portlib */
			j9port_control(J9PORT_CTLDATA_TRACE_STOP, (UDATA) ((RasGlobalStorage *)vm->j9rasGlobalStorage)->utIntf);
			/* stop tracing in the thread library */
			omrthread_lib_control(J9THREAD_LIB_CONTROL_TRACE_STOP, (UDATA) ((RasGlobalStorage *)vm->j9rasGlobalStorage)->utIntf);
			/* stop tracing in the hookable library */
			omrhook_lib_control(J9HOOK_LIB_CONTROL_TRACE_STOP, (UDATA) ((RasGlobalStorage *)vm->j9rasGlobalStorage)->utIntf);
			reportVMTermination(vm, thr);
		}
		break;

	case INTERPRETER_SHUTDOWN:

		thr = vm->internalVMFunctions->currentVMThread(vm);

		/* if command line argument parsing failed we don't want to try to use an uninitialized trace engine */
		if (((RasGlobalStorage *)vm->j9rasGlobalStorage != NULL) &&
			(((RasGlobalStorage *)vm->j9rasGlobalStorage)->utIntf != NULL)) {
			/* stop tracing the portlib */
			j9port_control(J9PORT_CTLDATA_TRACE_STOP, (UDATA) ((RasGlobalStorage *)vm->j9rasGlobalStorage)->utIntf);
			/* stop tracing in the thread library */
			omrthread_lib_control(J9THREAD_LIB_CONTROL_TRACE_STOP, (UDATA) ((RasGlobalStorage *)vm->j9rasGlobalStorage)->utIntf);
			/* stop tracing in the hookable library */
			omrhook_lib_control(J9HOOK_LIB_CONTROL_TRACE_STOP, (UDATA) ((RasGlobalStorage *)vm->j9rasGlobalStorage)->utIntf);

			reportVMTermination(vm, thr);
			requestTraceCleanup(vm, thr);
		}

		{
			J9VMDllLoadInfo *loadInfo = FIND_DLL_TABLE_ENTRY( J9_RAS_TRACE_DLL_NAME );
			if ( loadInfo && (IS_STAGE_COMPLETED(loadInfo->completedBits, VM_INITIALIZATION_COMPLETE)) ) {
				/* now force the main thread to end */
				reportTraceEvent(vm, thr, J9RAS_TRACE_ON_THREAD_END);
			}
		}

		/* Normal exit - its not safe to free this stuff if we are ab'ending.
		 * Blank global pointer before freeing struct (safer?)
		 */
		if (vm->j9rasGlobalStorage != NULL) {
			tempRasGbl = (RasGlobalStorage *)vm->j9rasGlobalStorage;
			vm->j9rasGlobalStorage = NULL;

			if ( tempRasGbl->jvmriInterface != NULL ) {
				j9mem_free_memory( tempRasGbl->jvmriInterface );
			}
			j9mem_free_memory( tempRasGbl );
		}

		if( j9TraceLock != NULL ) {
			omrthread_monitor_destroy(j9TraceLock);
		}

		vm->internalVMFunctions->shutdownJVMRI(vm);

		break;
	case LIBRARIES_ONUNLOAD:
		/*
		 * Unhook thread events before terminating trace
		 */
		(*hook)->J9HookUnregister(hook, J9HOOK_THREAD_ABOUT_TO_START, hookThreadAboutToStart, NULL);
		(*hook)->J9HookUnregister(hook, J9HOOK_VM_THREAD_END, hookThreadEnd, NULL);

		/* Disable tracing for thread library since the tracing engine will be unloaded. */
		omrthread_lib_clear_flags(J9THREAD_LIB_FLAG_TRACING_ENABLED);

		break;
	}

	return returnVal;
}

jint JNICALL JVM_OnLoad(JavaVM *vm, char *options, void *reserved)
{
	return JNI_OK;
}

jint JNICALL JVM_OnUnload(JavaVM *vm, void *reserved)
{
	return JNI_OK;
}

/**
 * Initializes and returns the UtInterface.
 *
 * @return The external UtInterface. UtModuleInterface APIs expect J9VMThread* as the "env" parameter.
 */
static UtInterface *
initializeUtInterface(void)
{
	UtInterface *retVal = NULL;
	omr_error_t rc = OMR_ERROR_NONE;

	/* initialize OMR layer */
	rc = fillInUTInterfaces(&retVal, (UtServerInterface *)&javaUtServerIntfS, &javaUtModuleIntfS);


	/* Update the interface we want to present to the Java side with extra calls on UtServerInterface
	 * and Trace functions that use J9VMThread for the env parameter.
	 */
	if (OMR_ERROR_NONE == rc) {

		/* Back up the original OMR interface so we can access it later if we need to call through. */
		memcpy( &omrUtModuleIntfS, &javaUtModuleIntfS, sizeof( UtModuleInterface) );

		/* initialize Java layer */
		javaUtServerIntfS.TraceMethodEnter = trcTraceMethodEnter;
		javaUtServerIntfS.TraceMethodExit = trcTraceMethodExit;
		javaUtServerIntfS.StartupComplete = trcStartupComplete;

		/*
		 * Initialize the direct module interface, these
		 * are versions of the trace functions that expect
		 * a J9VMThread as their env parameter rather than
		 * an OMRVMThread.
		 */
		javaUtModuleIntfS.Trace           = javaTrace;
		javaUtModuleIntfS.TraceMem        = javaTraceMem;
		javaUtModuleIntfS.TraceState      = javaTraceState;
		javaUtModuleIntfS.TraceInit       = javaTraceInit;
		javaUtModuleIntfS.TraceTerm       = javaTraceTerm;
	}
	return retVal;
}

static IDATA
processTraceOptionString(J9JavaVM *vm, char * opts[], IDATA * optIndex, const char *option, IDATA n)
{
	IDATA rc = J9VMDLLMAIN_OK;
	const char *optionString = option;
	IDATA optionsLength = n;

	while (optionsLength > 0 && rc == J9VMDLLMAIN_OK) {
		IDATA thisOptionLength = parseTraceOptions(vm, optionString, optionsLength);
		if (thisOptionLength < 0) {
			PORT_ACCESS_FROM_JAVAVM(vm);
			vaReportJ9VMCommandLineError(PORTLIB, "Unmatched braces encountered in trace options");
			rc = J9VMDLLMAIN_FAILED;
		} else if (thisOptionLength == 0) {
			PORT_ACCESS_FROM_JAVAVM(vm);
			vaReportJ9VMCommandLineError(PORTLIB, "Null option encountered in trace options");
			rc = J9VMDLLMAIN_FAILED;
		} else {
			rc = splitCommandLineOption(vm, optionString, thisOptionLength, opts + *optIndex);
			if (rc == J9VMDLLMAIN_OK) {
				optionString += ++thisOptionLength;
				optionsLength -= thisOptionLength;
				*optIndex += 2;
				if (*optIndex >= UT_MAX_OPTS - 1) {
					PORT_ACCESS_FROM_JAVAVM(vm);
					vaReportJ9VMCommandLineError(PORTLIB, "Maximum number of trace options exceeded - use a trace properties file");
					rc = J9VMDLLMAIN_FAILED;
				}
			}
		}
	}

	return rc;
}

/*
 * Initialize the trace options from the defaults for the J9VM and any
 * options that were passed in on the command line.
 *
 * Returns them as newly allocated string key/value pairs in the even/odd elements of opts.
 * The strings in opts will need to be freed.
 */
static IDATA
initializeTraceOptions(J9JavaVM *vm, char* opts[])
{
	IDATA xtraceIndex;
	char *optionString;
	IDATA i = 0;
	IDATA optionsLength;
	IDATA rc =  J9VMDLLMAIN_OK;

#define trace_option_helper(option) \
		splitCommandLineOption(vm, option, LITERAL_STRLEN(option), opts + i)

	/* set up the default options */
	rc = trace_option_helper(UT_MAXIMAL_KEYWORD "=all{level1}");
	i += 2;
	if (rc != J9VMDLLMAIN_FAILED) {
		rc = trace_option_helper(UT_EXCEPTION_KEYWORD "=j9mm{gclogger}");
		i += 2;
	}

	/*
	 *  Find all the trace options specified in the command line and pass
	 *  them to the trace engine. Note that trace args are formatted forwards
	 *  (left to right) so as to be consistent with dump.
	 */
	xtraceIndex = FIND_ARG_IN_VMARGS_FORWARD(OPTIONAL_LIST_MATCH, VMOPT_XTRACE, NULL);
	/* A trace option has been set. We need to check if the level 2 trace points have been
	 * enabled yet and if not enable them *before* processing the users trace options.
	 * (If we switch on level 2 after doing the users options then they won't get the
	 * trace they selected but their selection + level 2.)
	 */
	if( xtraceIndex >= 0 && !checkAndSetL2EnabledFlag() ) {
		rc = trace_option_helper(UT_MAXIMAL_KEYWORD "=all{level2}");
		i += 2;
	}
	while (xtraceIndex >= 0) {
		optionString = vm->vmArgsArray->actualVMArgs->options[xtraceIndex].optionString;
		optionsLength = strlen(optionString) - strlen(VMOPT_XTRACE);
		if (optionsLength > 0) {
			if (optionString[strlen(VMOPT_XTRACE)] == ':') {
				if (--optionsLength > 0) {
					optionString += strlen(VMOPT_XTRACE) + 1;
					rc = processTraceOptionString(vm,opts,&i,optionString,optionsLength);
				}
			} else {
				PORT_ACCESS_FROM_JAVAVM(vm);
				vaReportJ9VMCommandLineError(PORTLIB, "Syntax error in -Xtrace, expecting \":\"; received \"%c\"", optionString[strlen(VMOPT_XTRACE)]);
				rc = J9VMDLLMAIN_FAILED;
			}
		}
		xtraceIndex = FIND_NEXT_ARG_IN_VMARGS_FORWARD(OPTIONAL_LIST_MATCH, VMOPT_XTRACE, NULL, xtraceIndex);
	}
	opts[i++] = NULL;
	return rc;

#undef trace_option_helper
}

/*
 * Utility function to split trace options strings into two parts and put them in
 * the option name in opt[0] and the option value in opt[1]
 * If there is no value to go with the name opt[1] will be null.
 */
static IDATA
splitCommandLineOption(J9JavaVM *vm, const char *optionString, IDATA optionLength, char *opt[])
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA length;

	/* Split option and copy into opt array */
	for (length = 0; length < optionLength; length++) {
		if (optionString[length] == '=') {
			break;
		}
	}

	opt[0] = j9mem_allocate_memory(length + 1, OMRMEM_CATEGORY_TRACE);
	if (opt[0] == NULL) {
		return J9VMDLLMAIN_FAILED;
	}
	memcpy(opt[0], optionString, length);
	opt[0][length] = '\0';
	if (length < optionLength &&
			optionString[length + 1] != '\0' &&
			optionString[length + 1] != ',') {
		opt[1] = j9mem_allocate_memory(optionLength - length, OMRMEM_CATEGORY_TRACE);
		if (opt[1] == NULL) {
			return J9VMDLLMAIN_FAILED;
		}
		memcpy(opt[1], optionString + length + 1, optionLength - length - 1);
		opt[1][optionLength - length - 1] = '\0';
	} else {
		opt[1] = NULL;
	}

	return J9VMDLLMAIN_OK;
}

/*
 * Call back for OMR to set J9VM specific trace options.
 */
static omr_error_t
setJ9VMTraceOption(const OMR_VM *omr_vm, const char *optName, const char* optValue, BOOLEAN atRuntime)
{
	int i = 0;
	J9JavaVM* vm = (J9JavaVM*) omr_vm->_language_vm;
	omr_error_t rc = OMR_ERROR_NONE;
	IDATA optionNameLength = 0;

	PORT_ACCESS_FROM_JAVAVM(vm);

	optionNameLength = strlen(optName);

	for (i=0; i!=NUMBER_OF_TRACE_OPTIONS; i++) {
		if (optionNameLength == strlen(TRACE_OPTIONS[i].name) && j9_cmdla_stricmp(optName, (char *)TRACE_OPTIONS[i].name) == 0) {
			if (atRuntime && ! TRACE_OPTIONS[i].runtimeModifiable) {
				dbg_err_printf(1, PORTLIB, "<UT> Trace option %s cannot be configured at run-time."
						" Configure it at start-up with the command-line or a properties file\n",optName);
				rc = OMR_ERROR_ILLEGAL_ARGUMENT;
			} else {
				rc = TRACE_OPTIONS[i].optionFunction(vm,optValue,atRuntime);
			}

			break;
		}
	}

	return rc;
}

static omr_error_t
reportTraceEvent(J9JavaVM *vm, J9VMThread *self, UDATA eventFlags)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	char * name = NULL;
	UtThreadData **tempThr = NULL;

	/*
	 * No global storage!  -  probably shutting down...
	 */
	if (vm->j9rasGlobalStorage == NULL) {
		return OMR_ERROR_INTERNAL;
	}

	/*
	 * Find UTE thread slot inside VM thread structure
	 */
	tempThr = UT_THREAD_FROM_VM_THREAD(self);

	switch (eventFlags) {

	case J9RAS_TRACE_ON_THREAD_CREATE:
		/*
		 * If this is the main thread initializing again, stop the initialization pseudo thread
		 */
		if ((self == vm->mainThread) && (*tempThr != NULL)) {
			/* let trace engine know that threading is started up so it is safe to create it's trace write thread */
			if (OMR_ERROR_NONE != startTraceWorkerThread(tempThr)) {
				dbg_err_printf(1, PORTLIB, "<UT> Trace engine can't start trace thread\n");
				return OMR_ERROR_INTERNAL;
			}

			if (OMR_ERROR_NONE != threadStop(tempThr)) {
				dbg_err_printf(1, PORTLIB, "<UT> UTE thread stop failed for thread %p\n", self);
			}
		}
		name = threadName(self->omrVMThread);

		if (OMR_ERROR_NONE != threadStart(tempThr, self, name, self->osThread, self->omrVMThread)) {
			dbg_err_printf(1, PORTLIB, "<UT> UTE thread start failed for thread %p\n", self);
		}

		Trc_trcengine_reportThreadStart(self, self, name, self->osThread);
		if (name != NULL) {
			j9mem_free_memory(name);
		}
		break;

	case J9RAS_TRACE_ON_THREAD_END:

		if (self == NULL || *tempThr == NULL) {
			/* no point even trying - main thread probably encountered early problem */
			break;
		}
		/*
		 * Use UTE cached name (don't de-allocate it!)
		 */
		name = (char *)(*tempThr)->name;
		Trc_trcengine_reportThreadEnd(self, self, name, self->osThread);

		if (OMR_ERROR_NONE != threadStop(tempThr)) {
			dbg_err_printf(1, PORTLIB, "<UT> UTE thread stop failed for thread %p\n", self);
		}

		break;

	default:
		dbg_err_printf(1, PORTLIB, "<UT> Trace engine received an unknown trace event: [0x%04X]\n", eventFlags);
	}
	return OMR_ERROR_NONE;
}

static IDATA
parseTraceOptions(J9JavaVM *vm, const char *optionString, IDATA optionsLength)
{
	IDATA length;
	IDATA inBraces = 0;

	for (length = 0; length < optionsLength; length++) {
		 if (optionString[length] == '{') {
			 inBraces++;
			 continue;
		 }
		 if (optionString[length] == '}') {
			 if (--inBraces < 0) {
				 break;
			 }
			 continue;
		 }
		 if (inBraces == 0 &&
			 optionString[length] == ',') {
			 break;
		 }
	}
	return inBraces == 0 ? length : -1;
}

static char *
threadName(OMR_VMThread* vmThread)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(vmThread);
	size_t nameLength = 0;
	char *name = NULL;
	char* allocatedName;

	name = getOMRVMThreadName(vmThread);
	nameLength = strlen(name);
	allocatedName = omrmem_allocate_memory(nameLength + 1, OMRMEM_CATEGORY_TRACE);
	if (allocatedName != NULL) {
		strncpy(allocatedName, name, nameLength + 1);
	}
	releaseOMRVMThreadName(vmThread);
	return allocatedName;
}

static void displayTraceHelp(J9JavaVM *vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	j9tty_err_printf(PORTLIB, "\nUsage:\n\n");
	j9tty_err_printf(PORTLIB, "  java -Xtrace[:option,...]\n\n");
	j9tty_err_printf(PORTLIB, "  Valid options are:\n\n");
	j9tty_err_printf(PORTLIB, "     help                                Print general trace help\n");
	j9tty_err_printf(PORTLIB, "     what                                Print current trace configuration\n");
	j9tty_err_printf(PORTLIB, "     none[=tp_spec[,...]]                Ignore all previous/default trace options\n");
	j9tty_err_printf(PORTLIB, "     properties[=filespec]               Use file for trace options\n");
	j9tty_err_printf(PORTLIB, "     buffers=nnk|nnm|dynamic|nodynamic[,...] Buffer size and nature\n\n");
	j9tty_err_printf(PORTLIB, "     minimal=[!]tp_spec[,...]            Minimal trace data (time and id)\n");
	j9tty_err_printf(PORTLIB, "     maximal=[!]tp_spec[,...]            Time,id and parameters traced\n");
	j9tty_err_printf(PORTLIB, "     count=[!]tp_spec[,...]              Count tracepoints\n");
	j9tty_err_printf(PORTLIB, "     print=[!]tp_spec[,...]              Direct unindented trace data to stderr\n");
	j9tty_err_printf(PORTLIB, "     iprint=[!]tp_spec[,...]             Indented version of print option\n");
	j9tty_err_printf(PORTLIB, "     external=[!]tp_spec[,...]           Direct trace data to a JVMRI listener\n");
	j9tty_err_printf(PORTLIB, "     exception=[!]tp_spec[,...]          Use reserved in-core buffer\n");
	j9tty_err_printf(PORTLIB, "     methods=method_spec[,..]            Trace specified class(es) and methods\n\n");
	j9tty_err_printf(PORTLIB, "     trigger=[!]clause[,clause]...       Enables triggering events (including dumps) on tracepoints\n");
	j9tty_err_printf(PORTLIB, "     suspend                             Global trace suspend used with trigger\n");
	j9tty_err_printf(PORTLIB, "     resume                              Global trace resume used with trigger\n");
	j9tty_err_printf(PORTLIB, "     suspendcount=nn                     Trigger count for \"trace suspend\"\n");
	j9tty_err_printf(PORTLIB, "     resumecount=nn                      Trigger count for \"trace resume\"\n");
	j9tty_err_printf(PORTLIB, "     output=filespec[,nnm[,generations]] Sends maximal and minimal trace to a file\n");
	j9tty_err_printf(PORTLIB, "     exception.output=filespec[,nnnm]    Sends exception trace to a file\n");
	j9tty_err_printf(PORTLIB, "     stackdepth=nn                       Set number of frames output by jstacktrace trigger action\n");
	j9tty_err_printf(PORTLIB, "     sleeptime=nnt                       Time delay for sleep trigger action\n");
	j9tty_err_printf(PORTLIB, "                                         Recognised suffixes: ms (milliseconds), s (seconds). Default: ms\n");
	j9tty_err_printf(PORTLIB, "\n     where tp_spec is, for example, j9vm.111 or {j9vm.111-114,j9trc.5}\n");
	j9tty_err_printf(PORTLIB, "\n     IMPORTANT: Where an option value contains one or more commas, it must\n");
	j9tty_err_printf(PORTLIB, "     be enclosed in curly braces, for example:\n\n");
	j9tty_err_printf(PORTLIB, "         -Xtrace:maximal={j9vm,mt},methods={*.*,!java/lang/*},output=trace\n");
	j9tty_err_printf(PORTLIB, "\n     You may need to enclose options in quotation marks to prevent the shell\n");
	j9tty_err_printf(PORTLIB, "     intercepting and fragmenting comma-separated command lines, for example:\n\n");
	j9tty_err_printf(PORTLIB, "         \"-Xtrace:methods={java/lang/*,java/util/*},print=mt\"\n\n");
}

static void
hookThreadAboutToStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMThread* vmThread = ((J9ThreadAboutToStartEvent *)eventData)->vmThread;

	reportTraceEvent(vmThread->javaVM, vmThread, J9RAS_TRACE_ON_THREAD_CREATE);
}

static void
hookThreadEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMThreadEndEvent* event = eventData;
	J9VMThread* vmThread = event->currentThread;

	if (event->destroyingJavaVM) {
		/* don't report this event. We want this thread to survive in UTE until the VM has finished shutting down */
	} else {
		reportTraceEvent(vmThread->javaVM, vmThread, J9RAS_TRACE_ON_THREAD_END);
	}
}

static void
hookVmInitialized(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMThread* vmThread = ((J9VMInitEvent *)eventData)->vmThread;

	vmThread->javaVM->internalVMFunctions->rasStartDeferredThreads(vmThread->javaVM);
}

static void
reportVMTermination(J9JavaVM* vm, J9VMThread * vmThread)
{
	UtThreadData ** traceThread = UT_THREAD_FROM_VM_THREAD(vmThread);

	if (traceThread && *traceThread) {

		/* Threads that should be ignored when deciding if all tracing has finished. */
		char *ignoreThreads[] = { "Finalizer", "Signal dispatcher", "JIT PProfiler thread", "Reference Handler", NULL };

		utTerminateTrace(traceThread, ignoreThreads);
	}
}

static void
requestTraceCleanup(J9JavaVM* vm, J9VMThread * vmThread)
{
	UtThreadData ** traceThread = UT_THREAD_FROM_VM_THREAD(vmThread);

	if (traceThread && *traceThread) {
		freeTrace(traceThread);
	}
}

/*
 * Reconfigure trace at runtime.
 *
 * Returns an OMR return code.
 */
static omr_error_t
runtimeSetTraceOptions(J9VMThread * thr,const char * traceOptions)
{
	PORT_ACCESS_FROM_JAVAVM(thr->javaVM);
	RasGlobalStorage * j9ras = (RasGlobalStorage *)thr->javaVM->j9rasGlobalStorage;
	UtInterface * uteInterface = (UtInterface *)(j9ras ? j9ras->utIntf : NULL);

	/* Is trace running? */
	if ( uteInterface && uteInterface->server ) {
		char *opts[UT_MAX_OPTS];
		IDATA optIndex = 0;
		IDATA rastraceError = 0;
		omr_error_t rc = OMR_ERROR_NONE;
		int i = 0;
		/* Pass option to the trace engine */

		memset(opts,0,sizeof(opts));

		/* A trace option has been set. We need to check if the level 2 trace points have been
		 * enabled yet and if not enable them *before* processing the users trace options.
		 * (If we switch on level 2 after doing the users options then they won't get the
		 * trace they selected but their selection + level 2.)
		 */
		if (!checkAndSetL2EnabledFlag()) {
			const char* L2_OPTION[] = { UT_MAXIMAL_KEYWORD, "all{level2}", NULL };
			rc = setOptions(UT_THREAD_FROM_VM_THREAD(thr), L2_OPTION, TRUE );
			if (rc != OMR_ERROR_NONE) {
				return rc;
			}
		}

		rastraceError = processTraceOptionString(thr->javaVM,opts,&optIndex,traceOptions,strlen(traceOptions));

		if (rastraceError) {
			/* Clients expect UTE error codes */
			rc = OMR_ERROR_ILLEGAL_ARGUMENT;
		} else {
			rc = setOptions(UT_THREAD_FROM_VM_THREAD(thr), (const char**)opts, TRUE);
		}

		/*
		 *  Free up allocated options.
		 *  Option strings are allocated in setOption when it is called directly above
		 *  and also when it is called inside processTraceOptionString.
		 *  (We must free on error to prevent invalid options or options that cannot
		 *  be set at runtime causing memory leaks.)
		 */
		for (i = 0; opts[i] != NULL; i += 2) {
			j9mem_free_memory(opts[i]);
			if (opts[i + 1] != NULL) {
				j9mem_free_memory(opts[i + 1]);
			}
		}

		return rc;
	} else {
		return OMR_ERROR_INTERNAL;
	}
}

static omr_error_t
populateTraceHeaderInfo(J9JavaVM *vm)
{
	JavaVMInitArgs  *vmInitArgs;
	char *options;
	char *fullversion;
	omr_error_t ret = OMR_ERROR_NONE;

	PORT_ACCESS_FROM_JAVAVM( vm );

	/* overwrite the header settings now that we have full vm->j2seVersion info */
	vmInitArgs = (JavaVMInitArgs  *) vm->vmArgsArray->actualVMArgs;
	if (vmInitArgs) {
		int i;
		UDATA len = 0;
		char *nextOption;
		JavaVMOption *option = vmInitArgs->options;

		for (i = 0; i < vmInitArgs->nOptions; i++) {
			char *optionValue = option->optionString;
			len += strlen(optionValue) + 1;
			option++;
		}

		options = j9mem_allocate_memory(len + 1, OMRMEM_CATEGORY_TRACE);

		fullversion = "";

		if (options != NULL) {
			nextOption = options;
			option = vmInitArgs->options;
			for (i = 0; i < vmInitArgs->nOptions; i++) {
				char *optionValue = option->optionString;

				strcpy(nextOption, optionValue);
				nextOption += strlen(optionValue);
				*nextOption = '\n';
				nextOption++;
				option++;
			}
			*nextOption = '\0';

			if( vm->j9ras->serviceLevel != NULL ) {
				fullversion = vm->j9ras->serviceLevel;
			}

			if (OMR_ERROR_NONE != (ret = setTraceHeaderInfo(fullversion, options))) {
				dbg_err_printf(1, PORTLIB, "<UT> Trace engine failed to update trace header\n");
				j9mem_free_memory(options);
				return ret; /* don't propagate failure to load to JVM yet */
			}
		}

		if (options != NULL) {
			j9mem_free_memory(options);
		}
	}
	return OMR_ERROR_NONE;
}

/*
 * Return whether we have enabled L2 tracepoints yet.
 * Also mark them as now enabled.
 * (Caller should set maximal={level2} if true is returned.)
 */
static BOOLEAN
checkAndSetL2EnabledFlag(void)
{
	/* Flag to record whether we have enabled L2 trace points yet or
	 * are still in the early startup phase of the JVM's life.
	 */
	static BOOLEAN l2Enabled = FALSE;
	BOOLEAN currentStatus = FALSE;
	/* Take the local lock while setting these flags. */
	omrthread_monitor_enter(j9TraceLock);
	currentStatus = l2Enabled;
	l2Enabled = TRUE;
	omrthread_monitor_exit(j9TraceLock);
	return currentStatus;
}

/* External entry point for startup notification, exposed via J9UtServerInterface. */
static void
trcStartupComplete(J9VMThread* thr)
{
	if( !checkAndSetL2EnabledFlag() ) {
		runtimeSetTraceOptions(thr, UT_MAXIMAL_KEYWORD "=all{level2}");
	}
}

void
dbg_err_printf(int level, J9PortLibrary* portLibrary, const char * template, ...)
{
	if( getDebugLevel() >= level ) {
		PORT_ACCESS_FROM_PORT(portLibrary);
		va_list arg_ptr;
		va_start(arg_ptr, template);
		j9tty_err_vprintf(template, arg_ptr);
		va_end(arg_ptr);

	}
}

static void
printTraceWhat(J9PortLibrary* portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	void* cursor_ptr = NULL;
	void** cursor = &cursor_ptr;
	const char* option = walkTraceConfig(cursor);
	j9tty_err_printf(PORTLIB, "Trace engine configuration\n");
	j9tty_err_printf(PORTLIB, "--------------------------\n");
	if( NULL != option) {
		while ( NULL != *cursor ) {
			option = walkTraceConfig(cursor);
			if (0 == strcmp (option,"none")) break;
		}
		if (0 != strcmp (option,"none")) {
			cursor_ptr = NULL;
			cursor = &cursor_ptr;
			option = walkTraceConfig(cursor);
		}
		j9tty_err_printf(PORTLIB, "-Xtrace:%s\n", option);
		while ( NULL != *cursor ) {
			option = walkTraceConfig(cursor);
			j9tty_err_printf(PORTLIB, "-Xtrace:%s\n", option);
		}
	}
	j9tty_err_printf(PORTLIB, "--------------------------\n");
}

/**************************************************************************
 * name        - reportJ9VMCommandLineError
 * description - Report an error in a command line option and put the given
 *             - string in as detail in an NLS enabled message.
 * parameters  - portLibrary, detailStr, args for formatting into detailStr.
 *************************************************************************/
static void
reportJ9VMCommandLineError(J9PortLibrary* portLibrary, const char* detailStr, va_list args)
{
	char buffer[1024];
	PORT_ACCESS_FROM_PORT(portLibrary);

	j9str_vprintf(buffer, sizeof(buffer), detailStr, args);

	j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_TRC_ERROR_DETAIL_STR, buffer);
}

/**************************************************************************
 * name        - vaReportJ9VMCommandLineError
 * description - Wrapper for reportJ9VMCommandLineError so we can use it
 *             - locally without creating a va_list
 * parameters  - detailStr, args for formatting into detailStr.
 *************************************************************************/
void
vaReportJ9VMCommandLineError(J9PortLibrary* portLibrary, const char* detailStr, ...)
{
	va_list arg_ptr;
	va_start(arg_ptr, detailStr);
	reportJ9VMCommandLineError(portLibrary, detailStr, arg_ptr);
	va_end(arg_ptr);
}

/**
 * Dump triggering actions for Java
 * (Some of these can move back to OMR when the dump component moves to OMR)
 */

/**************************************************************************
 * name        - doTriggerActionJavadump
 * description - a javadump action has been triggered, do it
 * parameters  - thr
 * returns     - void
 *************************************************************************/
static void
doTriggerActionJavadump(OMR_VMThread *thr)
{
#if defined(J9VM_RAS_DUMP_AGENTS)
	J9JavaVM *vm = (J9JavaVM *)thr->_vm->_language_vm;

	vm->j9rasDumpFunctions->triggerOneOffDump(vm, "java", DUMP_CALLER_NAME, NULL, 0);
#endif
}

/**************************************************************************
 * name        - doTriggerActionCoredump
 * description - a coredump action has been triggered, do it
 * parameters  - thr
 * returns     - void
 *************************************************************************/
static void
doTriggerActionCoredump(OMR_VMThread *thr)
{
#if defined(J9VM_RAS_DUMP_AGENTS)
	J9JavaVM *vm = (J9JavaVM *)thr->_vm->_language_vm;

	vm->j9rasDumpFunctions->triggerOneOffDump(vm, "system", DUMP_CALLER_NAME, NULL, 0);
#endif
}

/**************************************************************************
 * name        - doTriggerActionAssertEvent
 * description - an assertion has been triggered, fire the dump assertion event.
 * parameters  - thr
 * returns     - void
 *************************************************************************/
/*
 * Temporary action to allow asserts to trigger the assert
 * dump event without the OMR'd trace code being able to
 * access the dump code.
 * Will be removed when the dump code moves to OMR.
 * See work item: 64106
 */
static void
doTriggerActionAssertDumpEvent(OMR_VMThread *omrThr)
{
#if defined(J9VM_RAS_DUMP_AGENTS)
	J9JavaVM *vm = (J9JavaVM *)omrThr->_vm->_language_vm;
	J9VMThread *thr = (J9VMThread *)omrThr->_language_vmthread;

	(vm)->j9rasDumpFunctions->triggerDumpAgents(vm, thr, J9RAS_DUMP_ON_TRACE_ASSERT, NULL);
#endif
}

/**************************************************************************
 * name        - doTriggerActionHeapdump
 * description - a heapdump action has been triggered, do it
 * parameters  - thr
 * returns     - void
 *************************************************************************/
static void
doTriggerActionHeapdump(OMR_VMThread *thr)
{
#if defined(J9VM_RAS_DUMP_AGENTS)
	J9JavaVM *vm = (J9JavaVM *)thr->_vm->_language_vm;

	vm->j9rasDumpFunctions->triggerOneOffDump(vm, "heap", DUMP_CALLER_NAME, NULL, 0);
#endif
}

/**************************************************************************
 * name        - doTriggerActionSnap
 * description - a snap action has been triggered, do it
 * parameters  - thr
 * returns     - void
 *************************************************************************/
static void
doTriggerActionSnap(OMR_VMThread *thr)
{
	J9JavaVM *vm = (J9JavaVM *)thr->_vm->_language_vm;
	vm->j9rasDumpFunctions->triggerOneOffDump(vm, "snap", DUMP_CALLER_NAME, NULL, 0);
}

/**************************************************************************
 * name        - doTriggerActionCeedump
 * description - a ceedump action has been triggered, do it
 * parameters  - thr
 * returns     - void
 *************************************************************************/
static void
doTriggerActionCeedump(OMR_VMThread *thr)
{
#if defined(J9VM_RAS_DUMP_AGENTS)
	J9JavaVM *vm = (J9JavaVM *)thr->_vm->_language_vm;

	vm->j9rasDumpFunctions->triggerOneOffDump(vm, "ceedump", DUMP_CALLER_NAME, NULL, 0);
#endif
}

static void
javaTrace(void *env, UtModuleInfo *modInfo, U_32 traceId, const char *spec, ...)
{
	va_list var;
	UtThreadData **utThr;
	J9VMThread* vmThr = (J9VMThread*) env;

	va_start(var, spec);
	/* TODO - Decide whether currentVMThread is fast enough for hot NoEnv trace points. */
	if( NULL == vmThr ) {
		vmThr = globalVM->internalVMFunctions->currentVMThread(globalVM);
	}
	utThr = UT_THREAD_FROM_VM_THREAD(vmThr);
	doTracePoint( utThr, modInfo, traceId, spec, var);
	va_end(var);
}

static void
javaTraceMem(void *env, UtModuleInfo *modInfo, U_32 traceId, UDATA length, void *memptr)
{
	/* There's no need to improve this to be cleaner, the function is obsolete and will assert when called. */
	omrUtModuleIntfS.TraceMem( (env?((J9VMThread*)env)->omrVMThread:NULL), modInfo, traceId, length, memptr);
}

static void
javaTraceState(void *env, UtModuleInfo *modInfo, U_32 traceId, const char *spec, ...)
{
	/* There's no need to improve this to be cleaner, the function is obsolete and will assert when called.
	 * Also since the function just asserts the varargs are never used and we can avoid handling them.
	 */
	omrUtModuleIntfS.TraceState( (env?((J9VMThread*)env)->omrVMThread:NULL), modInfo, traceId, spec, NULL);
}

static void
javaTraceInit(void *env, UtModuleInfo *modInfo)
{
	/* Registering is a one time event, rather than a high performance task,
	 * call through to the base omr version.
	 */
	omrUtModuleIntfS.TraceInit( (env?((J9VMThread*)env)->omrVMThread:NULL), modInfo );

	/* Replace the default OMR interface with the Java version that expects a J9VMThread
	 * for env rather than and OMR_VMThread.
	 */
	modInfo->intf = &javaUtModuleIntfS;
}

static void
javaTraceTerm(void* env, UtModuleInfo *modInfo)
{
	/* Registering is a one time event, rather than a high performance task,
	 * call through to the base omr version.
	 */
	omrUtModuleIntfS.TraceTerm( (env?((J9VMThread*)env)->omrVMThread:NULL), modInfo);
}
