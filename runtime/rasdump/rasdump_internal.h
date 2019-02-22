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

#ifndef rasdump_internal_h
#define rasdump_internal_h

/* @ddr_namespace: map_to_type=RasdumpInternalConstants */

/**
* @file rasdump_internal.h
* @brief Internal prototypes used within the OMR RASDUMP module.
*
* This file contains implementation-private function prototypes and
* type definitions for the OMR RASDUMP module.
*
*/

#include "j9comp.h"
#include "rasdump_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Extended dump facade */
typedef struct J9RASdumpQueue
{
	/* facade _must_ be first */
	J9RASdumpFunctions facade;
	J9RASdumpFunctions *oldFacade;
	struct J9RASdumpSettings *settings;
	struct J9RASdumpAgent *agents;
	struct J9RASdumpSettings *defaultSettings;
	struct J9RASdumpAgent *defaultAgents;
	struct J9RASdumpAgent *agentShutdownQueue;
}\
J9RASdumpQueue;

#define DUMP_FACADE_KEY ((void*)(long)0xFacadeDA)

/* Checks facade and returns dump queue, NULL if no match */
#define FIND_DUMP_QUEUE( vm, Q ) \
	( ( (Q) = (J9RASdumpQueue *) (vm)->j9rasDumpFunctions ) && \
	  ( (Q)->facade.reserved == DUMP_FACADE_KEY || ((Q) = NULL) ) )

/* Structure definition for a recognized/parsed dump option */
typedef struct J9RASdumpOption {
	IDATA kind;
	IDATA flags;
	char *args;
	IDATA pass;
} J9RASdumpOption;

#define J9RAS_DUMP_INVALID_TYPE -1
#define J9RAS_DUMP_OPT_DISABLED -1
#define J9RAS_DUMP_OPT_ARGS_STATIC 0x0
#define J9RAS_DUMP_OPT_ARGS_ALLOC 0x1

#define J9RAS_DUMP_OPTS_PASS_ONE 0x1

/* Structure definition for a default dump option */
typedef struct J9RASdefaultOption {
	char *type;
	char *args;
} J9RASdefaultOption;

typedef enum J9RASdumpRequestState
{
	J9RAS_DUMP_GOT_LOCK                    = 1,
	J9RAS_DUMP_GOT_VM_ACCESS               = 2,
	J9RAS_DUMP_GOT_EXCLUSIVE_VM_ACCESS     = 4,
	J9RAS_DUMP_HEAP_COMPACTED              = 8,
	J9RAS_DUMP_HEAP_PREPARED               = 16,
	J9RAS_DUMP_THREADS_HALTED              = 32,
	J9RAS_DUMP_ATTACHED_THREAD             = 64,
	J9RAS_DUMP_PREEMPT_THREADS             = 128,
	J9RAS_DUMP_TRACE_DISABLED              = 256,
	J9RAS_DUMP_GOT_JNI_VM_ACCESS           = 512, 
} J9RASdumpRequestState;

/* Internal function prototypes for rasdump module */ 
omr_error_t mapDumpSwitches(J9JavaVM *vm, J9RASdumpOption agentOpts[], IDATA *agentNum);
omr_error_t mapDumpOptions(J9JavaVM *vm, J9RASdumpOption agentOpts[], IDATA *agentNum);
omr_error_t mapDumpActions(J9JavaVM *vm, J9RASdumpOption agentOpts[], IDATA *agentNum, char *buf, IDATA condition);
omr_error_t mapDumpDefaults(J9JavaVM *vm, J9RASdumpOption agentOpts[], IDATA *agentNum);
omr_error_t mapDumpSettings(J9JavaVM *vm, J9RASdumpOption agentOpts[], IDATA *agentNum);
void disableDumpOnOutOfMemoryError(J9RASdumpOption agentOpts[], IDATA agentNum);
void enableDumpOnOutOfMemoryError(J9RASdumpOption agentOpts[], IDATA *agentNum);
UDATA parseAllocationRange(char *range, UDATA *min, UDATA *max);
omr_error_t rasDumpEnableHooks(J9JavaVM *vm, UDATA eventFlags);
void rasDumpFlushHooks(J9JavaVM *vm, IDATA stage);
void setAllocationThreshold(J9VMThread *vmThread, UDATA min, UDATA max);

/* Constants used with the RASDumpSystemInfo structures (linked list off J9RAS.systemInfo) */
#define J9RAS_SYSTEMINFO_SCHED_COMPAT_YIELD 1
#define J9RAS_SYSTEMINFO_HYPERVISOR         2
#define J9RAS_SYSTEMINFO_CORE_PATTERN       3
#define J9RAS_SYSTEMINFO_CORE_USES_PID      4

#define J9RAS_SCHED_COMPAT_YIELD_FILE "/proc/sys/kernel/sched_compat_yield"
#define J9RAS_CORE_PATTERN_FILE "/proc/sys/kernel/core_pattern"
#define J9RAS_CORE_USES_PID_FILE "/proc/sys/kernel/core_uses_pid"

#if defined(WIN32)
#define ALT_DIR_SEPARATOR '/'
#endif

#define J9RAS_DUMP_EXCEPTION_EVENT_GROUP (J9RAS_DUMP_ON_EXCEPTION_THROW | J9RAS_DUMP_ON_EXCEPTION_SYSTHROW | J9RAS_DUMP_ON_EXCEPTION_CATCH | J9RAS_DUMP_ON_EXCEPTION_DESCRIBE)

#define J9RAS_STDOUT_NAME "/STDOUT/"
#define J9RAS_STDERR_NAME "/STDERR/"

#ifdef __cplusplus
}
#endif

#endif /* rasdump_internal_h */

