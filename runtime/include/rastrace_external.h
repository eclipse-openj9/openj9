/*******************************************************************************
 * Copyright (c) 2014, 2019 IBM Corp. and others
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

#ifndef RASTRACE_EXTERNAL_H
#define RASTRACE_EXTERNAL_H

/* @ddr_namespace: default */
#include "ute_core.h"
#include "j9port.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define UT_THREAD_FROM_OMRVM_THREAD(thr) ((thr) ? &(thr)->_trace.uteThread : NULL)

/*
 * =============================================================================
 * Language interface - embedded in UtGlobalData
 *
 * This interface supplies callback functions for language-specific behaviour.
 * A callback may be NULL. We will NULL-check each function pointer before invoking it.
 * =============================================================================
 */
typedef omr_error_t (*SetLanguageTraceOptionFunc)(const OMR_VM *omr_vm, const char *optName, const char* optValue, BOOLEAN atRuntime);
typedef void (*ReportCommandLineErrorFunc)(J9PortLibrary* portLibrary, const char* detailStr, va_list args);

typedef struct OMRTraceLanguageInterface {
	/**
	 * Attach the current thread to the language VM.
	 *
	 * The trace engine invokes this callback to attach internal threads to the VM.
	 * A sample implementation is omr_trc_defaultAttachCurrentThreadToLanguageVM().
	 * Attached threads will be detached using the DetachCurrentThreadFromLanguageVM() callback.
	 *
	 * @pre The current thread is completely unattached to the VM.
	 *
	 * @param[in] omrVM The OMR VM.
	 * @param[in] name The name of the current thread.
	 * @param[out] omrVMThread Location where a pointer to a new OMR VM thread should be stored. Must not be NULL.
	 *
	 * @return an OMR error code
	 */
	omr_error_t	(*AttachCurrentThreadToLanguageVM)(OMR_VM *omrVM, const char *name, OMR_VMThread **omrVMThread);

	/**
	 * Detach the current thread from the language VM.
	 *
	 * The trace engine invokes this callback to detach internal threads to the VM.
	 * A sample implementation is omr_trc_defaultDetachThreadFromLanguageVM().
	 * The current thread should have been attached using the AttachCurrentThreadToLanguageVM() callback.
	 *
	 * @pre The current thread is attached to the VM.
	 *
	 * @param[in,out] omrVMThread The OMR VM thread.
	 * @return an OMR error code
	 */
	omr_error_t (*DetachCurrentThreadFromLanguageVM)(OMR_VMThread *omrVMThread);

	/**
	 * Pass unhandled trace options for implementer to handle.
	 */
	SetLanguageTraceOptionFunc SetLanguageTraceOption;

	/**
	 * Report errors with trace options to the implementer's error stream.
	 */
	ReportCommandLineErrorFunc ReportCommandLineError;
} OMRTraceLanguageInterface;

/*
 * =============================================================================
 *  Functions called by users of the trace library at initialisation/shutdown time.
 *  (Runtime functions are called vi UtInterface->UtServerInterface once initialized)
 * =============================================================================
 */

/**
 * @brief Fill in the interfaces the runtime will use to access trace functions.
 *
 * Fill in the interfaces the runtime will use to access trace functions.
 * This function fills in the function tables that an application can use to access
 * the trace engine functions.
 *
 * UtServerInterface contains the functions the application can use to access and control
 * the trace engine.
 *
 * UtModuleInterface contains the functions used by a module (library) to initialize
 * it's trace setup and to take trace points. These functions are usually accessed via
 * the macros generated from the TDF files.
 *
 * @param[in,out] utIntf Will be returned a pointer to a newly initialized UtInterface structure.
 * @param[in,out] utServerIntf
 * @param[in,out] utModuleIntf
 *
 * @return OMR_ERROR_NONE if successful, an error code describing the failure otherwise.
 */
omr_error_t fillInUTInterfaces(UtInterface **utIntf, UtServerInterface *utServerIntf, UtModuleInterface *utModuleIntf);

/**
 * @brief Initialize the trace engine.
 *
 * Initialize the trace engine.
 *
 * The gbl pointer is intended to allow the trace global data structure
 * to be linked to somewhere public so that it can be found by debugging
 * tools, this allows them to do things like extract trace buffers from
 * core files.
 * It is not intended to give read/write access to the trace global data
 * at runtime and is deliberately a void** for a structure that is not
 * publicly defined.
 *
 * The opts field is used for the same purpose as the opts field in
 * setOptions. See the documentation for that function for full details.
 *
 * The ignore field specifies options to ignore in the properties file.
 *
 * The function pointer types for the fields on OMRTraceLanguageInterface are
 * documented with the function pointer definitions.
 *
 * @param[out]    thr A UtThreadData pointer. *thr can point to NULL, initialize trace will initialize it.
 * @param[in,out] gbl A pointer for the trace global data structure.
 * @param[in]     opts A NULL-terminated array of options to be passed to the trace engine.
 * @param[in]     vm The OMR_VM the trace engine will be running inside.
 * @param[in]     ignore Options to ignore in the properties file.
 * @param[in]     languageIntf A set of function pointers for accessing function in the OMR based runtime.
 *
 * @return OMR_ERROR_NONE if successful, an error code describing the failure otherwise.
 */
omr_error_t initializeTrace(UtThreadData **thr, void **gbl,const char **opts, const OMR_VM *vm, const char **ignore, const OMRTraceLanguageInterface *languageIntf);

/**
 * @brief Inform the trace engine the application has reached a point that trace can start it's own threads.
 *
 * Inform the trace engine the application has reached a point that trace can start it's own threads.
 * The trace engine may (depending on the options it has been passed) start it's own internal threads
 * at this point.
 *
 * @param[in] thr UtThreadData for the current thread.
 *
 * @return OMR_ERROR_NONE if successful, an error code describing the failure otherwise.
 */
omr_error_t startTraceWorkerThread(UtThreadData **thr);

/**
 * @brief Terminate the trace engine.
 *
 * Terminate the trace engine. This call will shut down the trace engine,
 * waiting for all tracing threads to terminate and flush trace data to
 * any subscribers.
 *
 * It will wait until all the threads not listed in daemonThreadNames have
 * terminated or it's internal timeout has expired (1 second) before
 * flushing the trace data to subscribers.
 *
 * @param[in] thr UtThreadData for the current thread.
 * @param[in] daemonThreadNames A list of threads that should not be waited for before timing out.
 *
 * @return OMR_ERROR_NONE if successful, an error code describing the failure otherwise.
 */
omr_error_t utTerminateTrace(UtThreadData **thr, char** daemonThreadNames);

/**
 * @brief Free the trace engines internal data structures.
 *
 * This must only be called after utTerminateTrace and will free any remaining
 * memory held by the trace engine.
 *
 * After calling this no further calls may be made to the trace engine.
 *
 * @param[in] thr UtThreadData for the current thread.
 *
 */
void freeTrace(UtThreadData **thr);

/**
 * @brief Notify trace that a new thread has started.
 *
 * Notify the trace engine that a new thread has started. Trace will
 * allocate a new UtThreadData and store a pointer to it in the thr
 * parameter.
 *
 * A thread *must not* attempt to take a trace point or invoke trace
 * functions until this function has been called.
 *
 * Note that passing another threads UtThreadData when taking a
 * trace point may work - but will lead to the event represented
 * by that trace point appearing to have occurred on the other thread
 * once the trace is formatted. This is extremely undesirable when
 * using trace for problem determination.
 *
 * NOTE: Currently thrSynonym1 must be the omrthread_t for this thread and
 * thrSynonym2 must be the OMR_VMThread.
 *
 * @param[out] thr The UtThreadData structure for this thread.
 * @param[in]  threadId The id of this thread. Must be unique. (The address of a per thread structure is recommended)
 * @param[in]  threadName A name for this thread. Will be displayed when trace data is formatted. May be NULL.
 * @param[in]  thrSynonym1 A synonym for this threads id to aid cross referencing in problem diagnosis.
 * @param[in]  thrSynonym2 A synonym for this threads id to aid cross referencing in problem diagnosis.
 *
 * @return OMR_ERROR_NONE if successful, an error code describing the failure otherwise.
 */
omr_error_t threadStart(UtThreadData **thr, const void *threadId, const char *threadName, const void *thrSynonym1, const void *thrSynonym2);

/**
 * @brief Notify trace that a thread has stopped.
 *
 * Inform the trace engine that the thread represented by this
 * UtThreadData has terminated and free it's allocated UtThreadData.
 *
 * @param[in,out] thr The UtThreadData structure for this thread.
 *
 * @return OMR_ERROR_NONE if successful, an error code describing the failure otherwise.
 */
omr_error_t threadStop(UtThreadData **thr);

/**
 * @brief Set trace options
 *
 * Set trace options, options are supplied in an array of strings.
 * For options that are key/value pairs (for example maximal=all)
 * the key is passed in position n and the value in n+1:
 * opts[n] = "maximal";
 * opts[n+1] = "all";
 * For simple flag options (for example nofatalassert) the flag is
 * passed in opts[n] and opts[n+1] is null.
 * opts[n] = "nofatalassert";
 * opts[n+1] = NULL;
 *
 * The atRuntime flag is set to TRUE if trace startup is complete
 * (startTraceWorkerThread has been called) and FALSE if it
 * setOptions is called before that. Certain options may only be
 * set during startup. If atRuntime is TRUE setOptions will not attempt
 * to set those options.
 *
 * The SetOptions call available on UtServerInterface is identical but
 * is only intended to be called once trace startup is complete and
 * effectively is the same call with atRuntime set to TRUE.
 *
 * @param[in] thr The UtThreadData for the currently executing thread. Must not be NULL.
 * @param[in] opts An array of options to be passed to the trace engine.
 * @param[in] atRuntime A flag indicating whether trace startup is complete.
 *
 * @return The integer debug level the trace engine is using.
 */
omr_error_t setOptions(UtThreadData **thr, const char **opts, BOOLEAN atRuntime);

/**
 * @brief Set the information to be included in the trace file header.
 *
 * Set the information to be included in the trace file header. This is
 * information which will be included in the header of all binary trace
 * files generated by this application and formatted into the header
 * information when that trace file is formatted.
 *
 * Typically serviceInfo is the unique version string for the application
 * and startupInfo is the formatted command line.
 *
 * The strings are copied so may be freed.
 *
 * @param[in] serviceInfo The service level of the application.
 * @param[in] startupInfo The startup information for the application.
 *
 * @return OMR_ERROR_NONE if successful, an error code describing the failure otherwise.
 */
omr_error_t setTraceHeaderInfo(const char * serviceInfo, const char * startupInfo);

/**
 * @brief Obtain the debug level for the trace engine.
 *
 * Obtain the debug level for the trace engine. This is set by passing trace the
 * option debug=N (where N is a positive integer).
 *
 * This function allows trace implementers to write statements of the form:
 * if ( getDebugLevel > N ) {
 * 		ReportCommandLineErrorFunc(....)
 * }
 * where ReportCommandLineErrorFunc is the same function (or the same underlying functionality)
 * as passed to the trace engine in field OMRTraceLanguageInterface.ReportCommandLineError when trace
 * was initialized. This allows trace implementers to integrate their own debugging messages
 * with those from the trace engine without having to create a separate debugging mechanism.
 *
 * @return The integer debug level the trace engine is using.
 */
int32_t getDebugLevel(void);

/**
 * @brief Creates a trace point
 *
 * Creates a trace point to record a trace event for the specified thread (generally the
 * currently running thread). The trace point is in the module described by modInfo and
 * has it's trace id masked with it's activation information. (This is generated as part
 * of the trace macros and should simply be passed through.)
 *
 * The spec parameter describes the arguments to the trace point (and is also generated)
 * and those arguments (if any) are contained in the va_list.
 *
 * This function exists to allow users to override the default implementation of
 * UtModuleInterface.Trace(void *env, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, ...);
 * and pass in the var args arguments (since the use of var args prevents simply calling
 * the original UtModuleInterface.Trace function). This allows trace implementers to
 * map from a per thread env parameter that is convenient for their application to a
 * UtThreadData. All the other parameters should be left unchanged.
 *
 * @param[in] thr The UtThreadData for the currently executing thread. Must not be NULL.
 * @param[in] modInfo A pointer to the UtModuleInfo for the module this trace point belongs to.
 * @param[in] traceId The trace point id for this trace point.
 * @param[in] spec    The trace point format specification for this trace point
 * @param[in] varArgs The va_list of parameters for this trace point.
 */
void doTracePoint(UtThreadData **thr, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, va_list varArgs);

/**
 * @brief Walk the current trace configuration options.
 *
 * Returns each of the trace options that have been set and updates **cursor to
 * track it's position. Passing *cursor = NULL begins a new walk.
 *
 * Cursor is updated during the walk and will be NULL when the end of the options
 * is reached. The returned char* will also be NULL at the end of the walk.
 *
 * @param cursor A cursor to record the current walk position with.
 *
 * @return a pointer to an option string or NULL when there are no more options.
 */
const char* walkTraceConfig(void **cursor);

/*
 * =============================================================================
 * Keywords for trace command line options.
 * =============================================================================
 */

#define UT_DEBUG_KEYWORD              "DEBUG"
#define UT_FORMAT_KEYWORD             "FORMAT"
#define UT_SUFFIX_KEYWORD             "SUFFIX"
#define UT_LIBPATH_KEYWORD            "LIBPATH"
#define UT_PROPERTIES_KEYWORD         "PROPERTIES"
#define UT_BUFFERS_KEYWORD            "BUFFERS"
#define UT_INIT_KEYWORD               "INITIALIZATION"
#define UT_RESET_KEYWORD              "RESETTABLE"
#define UT_MINIMAL_KEYWORD            "MINIMAL"
#define UT_MAXIMAL_KEYWORD            "MAXIMAL"
#define UT_COUNT_KEYWORD              "COUNT"
#define UT_PRINT_KEYWORD              "PRINT"
#define UT_IPRINT_KEYWORD             "IPRINT"
#define UT_PLATFORM_KEYWORD           "PLATFORM"
#define UT_EXTERNAL_KEYWORD           "EXTERNAL"
#define UT_EXCEPTION_KEYWORD          "EXCEPTION"
#define UT_EXCEPT_OUT_KEYWORD         "EXCEPTION.OUTPUT"
#define UT_STATE_OUT_KEYWORD          "STATE.OUTPUT"
#define UT_NONE_KEYWORD               "NONE"
#define UT_OUTPUT_KEYWORD             "OUTPUT"
#define UT_LEVEL_KEYWORD              "LEVEL"
#define UT_TRIGGER_KEYWORD            "TRIGGER"
#define UT_SUSPEND_KEYWORD            "SUSPEND"
#define UT_RESUME_KEYWORD             "RESUME"
#define UT_RESUME_COUNT_KEYWORD       "RESUMECOUNT"
#define UT_SUSPEND_COUNT_KEYWORD      "SUSPENDCOUNT"
#define UT_ALL                        "ALL"
#define UT_BACKTRACE                  "BACKTRACE"
#define UT_FATAL_ASSERT_KEYWORD       "FATALASSERT"
#define UT_NO_FATAL_ASSERT_KEYWORD    "NOFATALASSERT"
#define UT_SLEEPTIME_KEYWORD          "SLEEPTIME"

/*
 * =============================================================================
 * Types for trace trigger functionality.
 * =============================================================================
 */

/**
 * When a trigger was fired - before or after a tracepoint
 */
typedef enum TriggerPhase {BEFORE_TRACEPOINT, AFTER_TRACEPOINT} TriggerPhase;

typedef void (*TriggerActionFunc)(OMR_VMThread *thr); /* Function to call when performing a trigger action */

typedef struct RasTriggerAction {
	const char *name; /* String specified on the cmd-line to select this action */
	TriggerPhase phase; /* When this trigger action should be fired */
	TriggerActionFunc fn; /* Function to call when performing the action */
} RasTriggerAction;

typedef omr_error_t (*ParseTriggerTypeFunc)(OMR_VMThread *thr, char *opt, BOOLEAN atRuntime); /* Function to parse the options of a trigger type */

typedef struct RasTriggerType {
	const char *name; /* String specified on the cmd-line to select this type */
	ParseTriggerTypeFunc parse; /* Function to parse the options of this trigger type */
	BOOLEAN runtimeModifiable; /* Whether this trigger type can be modified at runtime */
} RasTriggerType;

/**
 * @brief Add a type of trace trigger
 *
 * Add a type of trace trigger. This allows additional trace trigger types
 * (the base types are "group" and "tpnid") to be added to the trace engine
 *
 * The application can define it's own logic for triggering these new types
 * of triggers and they can call any of the available trigger actions.
 *
 * @param[in] thr The UtThreadData for the currently executing thread.
 * @param[in] newType The new trigger type to add.
 *
 * @return OMR_ERROR_NONE if successful, an error code describing the failure otherwise.
 */
omr_error_t addTriggerType(OMR_VMThread *thr, const struct RasTriggerType *newType);

/**
 * @brief Add a type of trace trigger action.
 *
 * Add a type of trace trigger. This allows additional trace trigger actions
 * to be added to the trace engine These can be triggered by any trigger type.
 *
 * This allows the application to define additional actions or call backs that
 * can occur when a trace point fires.
 *
 * @param[in] thr The UtThreadData for the currently executing thread.
 * @param[in] newAction The new trigger action to add.
 *
 * @return OMR_ERROR_NONE if successful, an error code describing the failure otherwise.
 */
omr_error_t addTriggerAction(OMR_VMThread *thr, const struct RasTriggerAction *newAction);

/**
 * Convert the name of a trigger action into its entry in the rasTriggerActions array.
 *
 * @param thr
 * @param name A trigger name string.
 *
 * @return Trigger action or NULL
 */
const struct RasTriggerAction *parseTriggerAction(OMR_VMThread *thr, const char *name, BOOLEAN atRuntime);

#ifdef  __cplusplus
}
#endif

#endif /* !RASTRACE_EXTERNAL_H */
