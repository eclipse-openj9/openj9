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

#ifndef rasdump_api_h
#define rasdump_api_h

/**
* @file rasdump_api.h
* @brief Public API for the RASDUMP module.
*
* This file contains public function prototypes and
* type definitions for the RASDUMP module.
*/

#include "j9.h"
#include "j9comp.h"
#include "jni.h"
#include "j9dump.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct J9JavaVM;
struct J9RASdumpSettings;
struct J9RASdumpAgent;
struct J9PortLibrary;
struct J9RASdumpEventData;
struct J9VMThread;

/* ---------------- dmpagent.c ---------------- */

/**
* @brief
* @param *vm
* @param *settings
* @return omr_error_t
*/
omr_error_t
freeDumpSettings(struct J9JavaVM *vm, struct J9RASdumpSettings *settings);


/**
* @brief
* @param *vm
* @param *src
* @param *dst
* @return omr_error_t
*/
omr_error_t
copyDumpSettings(struct J9JavaVM *vm, struct J9RASdumpSettings *src, struct J9RASdumpSettings *dst);


/**
* @brief
* @param *vm
* @param *src
* @param *dst
* @return IDATA
*/
struct J9RASdumpSettings *
copyDumpSettingsQueue(struct J9JavaVM *vm, struct J9RASdumpSettings *toCopy);


/**
* @brief
* @param *vm
* @return struct J9RASdumpSettings *
*/
struct J9RASdumpSettings *
initDumpSettings(struct J9JavaVM *vm);


/**
* @brief
* @param *vm
* @param kind
* @param *optionString
* @return omr_error_t
*/
omr_error_t
loadDumpAgent(struct J9JavaVM *vm, IDATA kind, char *optionString);


/**
* @brief
* @param *vm
* @param kind
* @param *optionString
* @return omr_error_t
*/
omr_error_t
deleteMatchingAgents(struct J9JavaVM *vm, IDATA kind, char *optionString);


/**
* @brief
* @param *vm
* @param kind
* @param *optionString
* @return IDATA
*/
J9RASdumpAgent *
copyDumpAgentsQueue(J9JavaVM *vm, J9RASdumpAgent *toCopy);


/**
* @brief
* @param *vm
* @param *src
* @param *dst
* @return omr_error_t
*/
omr_error_t
copyDumpAgent(struct J9JavaVM *vm, J9RASdumpAgent *src, J9RASdumpAgent *dst);


/**
* @brief
* @param eventFlag
* @return const char*
*/
const char*
mapDumpEvent(UDATA eventFlag);


/**
* @brief
* @param *vm
* @param *agent
* @return IDATA
*/
omr_error_t
printDumpAgent(struct J9JavaVM *vm, struct J9RASdumpAgent *agent);


/**
* @brief
* @param *vm
* @param *agent
* @param buffer_size
* @param *options_buffer
* @param *index
* @return IDATA
*/
IDATA
queryAgent(struct J9JavaVM *vm, struct J9RASdumpAgent *agent, IDATA buffer_size, void* options_buffer, IDATA* index);


/**
* @brief
* @param *vm
* @param bits
* @param verbose
* @return omr_error_t
*/
omr_error_t
printDumpEvents(struct J9JavaVM *vm, UDATA bits, IDATA verbose);


/**
* @brief
* @param *vm
* @param bits
* @param verbose
* @return omr_error_t
*/
omr_error_t
printDumpRequests(struct J9JavaVM *vm, UDATA bits, IDATA verbose);


/**
* @brief
* @param *vm
* @param kind
* @param verboseLevel
* @return omr_error_t
*/
omr_error_t
printDumpSpec(struct J9JavaVM *vm, IDATA kind, IDATA verboseLevel);


/**
 * runDumpAgent - executes a single dump agent
 * 
 * Takes care of acquiring exclusive, performing prepwalk & compact and some final validation and warning
 * messages. 
 * 
 * @param vm [in] VM pointer
 * @param agent [in] Agent the be executed
 * @param context [in] Dump context (what triggered the dump)
 * @param state [inout] State bit flags. Used to maintain state between multiple calls of runDumpAgent. 
 *                 When you've performed all runDumpAgent calls you must call unwindAfterDump to make 
 *                 sure all locks are cleaned up. The first time runDumpAgent is called, state should 
 *                 be initialized to 0.
 * @param detail [in] Detail string for dump cause
 * @param timeNow [in] Time value as returned from j9time_current_time_millis. Used to timestamp the dumps.
 *
 * @return OMR_ERROR_NONE on success, OMR_ERROR_INTERNAL if there was a problem. 
 */
omr_error_t
runDumpAgent(struct J9JavaVM *vm, J9RASdumpAgent * agent, J9RASdumpContext * context, UDATA * state, char * detail, U_64 timeNow);


/**
 * createAndRunOneOffDumpAgent - creates a temporary dump agent and runs it
 * 
 * A wrapper around runDumpAgent used for triggering one-off dumps. 
 * 
 * Parameters:
 * 
 * @param vm [in] VM pointer
 * @param context [in] dump context
 * @param kind [in] type code for dump being produced
 * 
 * Returns: OMR_ERROR_NONE on success, OMR_ERROR_INTERNAL or OMR_ERROR_OUT_OF_NATIVE_MEMORY if there was a problem.
 */
omr_error_t
createAndRunOneOffDumpAgent(struct J9JavaVM *vm,J9RASdumpContext * context,IDATA kind,char * optionString);


/**
* @brief
* @param *agent
* @param *label
* @param *context
* @return omr_error_t
*/
omr_error_t
runDumpFunction(J9RASdumpAgent *agent, char *label, J9RASdumpContext *context);


/**
* @brief
* @param *vm
* @param **optionStringPtr
* @return IDATA
*/
IDATA
scanDumpType(char **optionStringPtr);


/**
* @brief
* @param *vm
* @param kind
* @return omr_error_t
*/
omr_error_t
unloadDumpAgent(struct J9JavaVM *vm, IDATA kind);


/**
 * Writes the appropriate "we're about to write a dump" message to the console depending on whether the
 * dump was event driven or user requested.
 * 
 * @param *portLibrary Port library to use to write dump
 * @param *context Context that dump was taken in
 * @param *dumpType Type of dump being written - e.g. "Java"
 * @param fileName Name of the file being used to write out. Can be NULL.
 */
void
reportDumpRequest(struct J9PortLibrary *portLibrary, J9RASdumpContext *context, const char * const dumpType, const char * const fileName);

/* ---------------- dmpqueue.c ---------------- */

/**
* @brief
* @param *vm
* @param *agent
* @return omr_error_t
*/
omr_error_t
insertDumpAgent(struct J9JavaVM *vm, struct J9RASdumpAgent *agent);


/**
* @brief
* @param *vm
* @param *agent
* @return omr_error_t
*/
omr_error_t
removeDumpAgent(struct J9JavaVM *vm, struct J9RASdumpAgent *agent);


/**
* @brief
* @param *vm
* @param **agentPtr
* @param dumpFn
* @return omr_error_t
*/
omr_error_t
seekDumpAgent(struct J9JavaVM *vm, struct J9RASdumpAgent **agentPtr, J9RASdumpFn dumpFn);


/* ---------------- dmpsup.c ---------------- */

/**
* @brief
* @param *vm
* @param stage
* @param *reserved
* @return IDATA
*/
IDATA 
J9VMDllMain(J9JavaVM *vm, IDATA stage, void *reserved);


/**
* @brief
* @param *vm
* @param *options
* @param *reserved
* @return jint
*/
jint JNICALL 
JVM_OnLoad(JavaVM *vm, char *options, void *reserved);


/**
* @brief
* @param *vm
* @param *reserved
* @return jint
*/
jint JNICALL 
JVM_OnUnload(JavaVM *vm, void *reserved);


/**
* @brief
* @param *vm
* @param *optionString
* @return omr_error_t
*/
omr_error_t
setDumpOption(struct J9JavaVM *vm, char *optionString);


/**
* @brief
* @param *vm
* @return omr_error_t
*/
omr_error_t
resetDumpOptions(struct J9JavaVM *vm);


/* ---------------- trigger.c ---------------- */

/**
* @brief
* @param *vm
* @param *agent
* @param *context
* @param state
* @return UDATA
*/
UDATA
prepareForDump(struct J9JavaVM *vm, struct J9RASdumpAgent *agent, struct J9RASdumpContext *context, UDATA state);


/**
* @brief
* @param *vm
* @return omr_error_t
*/
omr_error_t
printLabelSpec(struct J9JavaVM *vm);


/**
 * Triggers a single dump. This is the path for all dumps NOT triggered by events.
 * 
 * Function used to be called runNamedDump
 * 
 * @param *vm VM pointer
 * @param *optionString Name of the dump to take. E.g. "heap"
 * @param *caller String describing how the user triggered the dump. E.g. "-Xtrace:trigger"
 */
omr_error_t
triggerOneOffDump(struct J9JavaVM *vm, char *optionString, char *caller, char *fileName, size_t fileNameLength);


/**
 * Query the settings of the dump agents.
 * 
 * @param *vm VM pointer
 * @param buffer_size Size of the buffer passed in
 * @param *options_buffer pointer to the buffer to be populated with data
 * @param *data_size pointer to integer to receive the required size of buffer.
 */
omr_error_t
queryVmDump(struct J9JavaVM *vm, int buffer_size, void* options_buffer, int* data_size);


/**
* @brief
* @param *vm
* @param *agent
* @param *context
* @param *buf
* @param len
* @param *reqLen
* @param now
* @return omr_error_t
*/
omr_error_t
dumpLabel(struct J9JavaVM *vm, J9RASdumpAgent *agent, J9RASdumpContext *context, char *buf, size_t len, UDATA *reqLen, I_64 now);


/**
* @brief
* @param *vm
* @param *self
* @param eventFlags
* @param *eventData
* @return omr_error_t
*/
omr_error_t
triggerDumpAgents(struct J9JavaVM *vm, struct J9VMThread *self, UDATA eventFlags, struct J9RASdumpEventData *eventData);


/**
* @brief
* @param *vm
* @param *agent
* @param *context
* @param state
* @return UDATA
*/
UDATA
unwindAfterDump(struct J9JavaVM *vm, struct J9RASdumpContext *context, UDATA state);

/* ---------------- rasdump.c ---------------- */

#if defined(J9VM_RAS_EYECATCHERS)

/**
 * j9rasSetServiceLevel - sets or updates RAS service level to appear in dumps, etc.
 *
 * Parameters:
 *
 * @param vm [in] VM pointer
 * @param runtimeVersion [in] value of System property 'java.runtime.version' or NULL
 */
void
j9rasSetServiceLevel(J9JavaVM *vm, const char *runtimeVersion);

#endif /* J9VM_RAS_EYECATCHERS */

#ifdef __cplusplus
}
#endif

#endif /* rasdump_api_h */
