/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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

#ifndef J9RASTRACE_H
#define J9RASTRACE_H
/* @ddr_namespace: map_to_type=J9RastraceConstants */

#include "j9.h"
#include "rastrace_external.h"

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * =============================================================================
 *  Miscellaneous macros
 * =============================================================================
 */
#define J9RAS_TRACE_ON_THREAD_CREATE_BIT  0
#define J9RAS_TRACE_ON_THREAD_END_BIT  1
#define J9RAS_TRACE_HOOK_TABLE_SIZE  2 /* Must be 1 greater than the number of _BITs */
#define J9RAS_TRACE_ON_THREAD_CREATE  (1<<J9RAS_TRACE_ON_THREAD_CREATE_BIT)
#define J9RAS_TRACE_ON_THREAD_END  (1<<J9RAS_TRACE_ON_THREAD_END_BIT)
#define J9RAS_TRACE_ON_ANY  ((1<<J9RAS_TRACE_HOOK_TABLE_SIZE) - 1) /* Mask containing all of the bits */

/*
 * =============================================================================
 *  Keywords for options added by J9VM layer.
 * =============================================================================
 */
#define RAS_METHODS_KEYWORD             "METHODS"
#define RAS_DEBUG_KEYWORD               "DEBUG"
#define RAS_TRIGGER_KEYWORD             "TRIGGER"
#define RAS_STACKDEPTH_KEYWORD          "STACKDEPTH"
#define RAS_SLEEPTIME_KEYWORD           "SLEEPTIME"
#define RAS_COMPRESSION_LEVEL_KEYWORD   "STACKCOMPRESSIONLEVEL"

/*
 * ======================================================================
 * Method trace
 * ======================================================================
 */

#define RAS_CLASS_METHOD_SEPARATOR       '.'
#define RAS_DO_NOT_TRACE                 '!'
#define RAS_INPUT_RETURN_CHAR_START      '('
#define RAS_INPUT_RETURN_CHAR_END        ')'

typedef struct RasMethodTable {
	J9UTF8 *className;
	J9UTF8 *methodName;
	BOOLEAN includeFlag;
	BOOLEAN traceInputRetVals;
	int classMatchFlag;
	int methodMatchFlag;
	struct RasMethodTable *next;
} RasMethodTable;

/*
 * ======================================================================
 *  Triggering Methods Rule (RSTM)
 * ======================================================================
 */
typedef struct RasTriggerMethodRule {
	struct RasTriggerMethodRule *next;
	struct RasTriggeredMethodBlock *tmbChain;
	struct RasMethodTable *methodTable;
	U_32 delay;
	I_32 match;
	const struct RasTriggerAction *entryAction;
	const struct RasTriggerAction *exitAction;
} RasTriggerMethodRule;

/*
 * ======================================================================
 *  Triggered Method Block Entry (RSMB)
 * ======================================================================
 */
typedef struct RasTriggeredMethodBlock {
	struct RasTriggeredMethodBlock *next;
	J9Method *mb;
} RasTriggeredMethodBlock;


/*
 * =============================================================================
 *  Method trace functions
 * =============================================================================
 */
omr_error_t enableMethodTraceHooks (J9JavaVM *vm);
void trcTraceMethodEnter(J9VMThread *thr, J9Method *method, void *receiverAddress, UDATA methodType);
void trcTraceMethodExit(J9VMThread *thr, J9Method *method, void *exceptionPtr, void *returnValuePtr, UDATA methodType);
omr_error_t setMethodSpec(J9JavaVM *vm, char * value, J9UTF8 ** utf8Address, int * matchFlag);
omr_error_t setMethod(J9JavaVM *vm, const char * value, BOOLEAN atRuntime);
U_8 rasSetTriggerTrace(J9VMThread *thr, J9Method *method);
void rasTriggerMethod(J9VMThread *thr, J9Method *mb, I_32 entry, const TriggerPhase phase);
BOOLEAN matchMethod (RasMethodTable * methodTable, J9Method *method);
omr_error_t processTriggerMethodClause(OMR_VMThread *, char *, BOOLEAN atRuntime);
void doTriggerActionJstacktrace(OMR_VMThread *thr);
omr_error_t setStackDepth(J9JavaVM *thr, const char * value, BOOLEAN atRuntime);
omr_error_t setStackCompressionLevel(J9JavaVM * vm, const char *str, BOOLEAN atRuntime);

/*
 * =============================================================================
 *  Functions shared between the Java trace code.
 * =============================================================================
 */
void dbg_err_printf(int level, J9PortLibrary* portLibrary, const char * template, ...);
void vaReportJ9VMCommandLineError(J9PortLibrary* portLibrary, const char* detailStr, ...);

#ifdef  __cplusplus
}
#endif

#endif /* J9RASTRACE_H */
