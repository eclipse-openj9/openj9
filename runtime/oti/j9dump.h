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

#ifndef j9dump_h
#define j9dump_h

#define J9DMP_TRIGGER( vm, self, eventFlags ) \
	(vm)->j9rasDumpFunctions->triggerDumpAgents(vm, self, eventFlags, NULL)

/* Rasdump Global storage block */
typedef struct RasDumpGlobalStorage {
	void* dumpLabelTokens;
	omrthread_monitor_t dumpLabelTokensMutex;
	
	UDATA allocationRangeMin;
	UDATA allocationRangeMax;
	
	UDATA noProtect; /* If set, do not take dumps under their own signal handler */
	UDATA noFailover; /* If set, do not failover to /tmp etc if unable to write dump */
} RasDumpGlobalStorage;

struct J9RASdumpAgent; /* Forward struct declaration */
struct J9RASdumpContext; /* Forward struct declaration */

/* @ddr_namespace: map_to_type=J9RASdumpAgent */

typedef struct J9RASdumpAgent {
	struct J9RASdumpAgent* nextPtr;
	omr_error_t  (*shutdownFn)(struct J9JavaVM *vm, struct J9RASdumpAgent **agentPtr) ;
	UDATA eventMask;
	char* detailFilter;
	UDATA startOnCount;
	UDATA stopOnCount;
	UDATA count;
	char* labelTemplate;
	omr_error_t  (*dumpFn)(struct J9RASdumpAgent *agent, char *label, struct J9RASdumpContext *context) ;
	char* dumpOptions;
	void* userData;
	UDATA priority;
	UDATA requestMask;
	UDATA prepState;
	char* subFilter;
} J9RASdumpAgent;

#define J9RAS_DUMP_ON_VM_STARTUP_BIT  0
#define J9RAS_DUMP_ON_VM_SHUTDOWN_BIT  1
#define J9RAS_DUMP_ON_CLASS_LOAD_BIT  2
#define J9RAS_DUMP_ON_CLASS_UNLOAD_BIT  3
#define J9RAS_DUMP_ON_EXCEPTION_THROW_BIT  4
#define J9RAS_DUMP_ON_EXCEPTION_CATCH_BIT  5
#define J9RAS_DUMP_ON_BREAKPOINT_BIT  6
#define J9RAS_DUMP_ON_DEBUG_FRAME_POP_BIT  7
#define J9RAS_DUMP_ON_THREAD_START_BIT  8
#define J9RAS_DUMP_ON_THREAD_BLOCKED_BIT  9
#define J9RAS_DUMP_ON_THREAD_END_BIT  10
#define J9RAS_DUMP_ON_HEAP_EXPAND_BIT  11
#define J9RAS_DUMP_ON_GLOBAL_GC_BIT  12
#define J9RAS_DUMP_ON_GP_FAULT_BIT  13
#define J9RAS_DUMP_ON_USER_SIGNAL_BIT  14
#define J9RAS_DUMP_ON_EXCEPTION_DESCRIBE_BIT  15
#define J9RAS_DUMP_ON_SLOW_EXCLUSIVE_ENTER_BIT  16
#define J9RAS_DUMP_ON_ABORT_SIGNAL_BIT  17
#define J9RAS_DUMP_ON_EXCEPTION_SYSTHROW_BIT  18
#define J9RAS_DUMP_ON_TRACE_ASSERT_BIT  19
#define J9RAS_DUMP_ON_USER_REQUEST_BIT  20
#define J9RAS_DUMP_ON_OBJECT_ALLOCATION_BIT  21
#define J9RAS_DUMP_ON_CORRUPT_CACHE_BIT  22
#define J9RAS_DUMP_ON_EXCESSIVE_GC_BIT  23
#define J9RAS_DUMP_HOOK_TABLE_SIZE  24  /* 1+ the last _BIT */

/* bit flags corresponding to the _BIT values above. Definitions must be simple so that DDR can process them. */
#define J9RAS_DUMP_ON_VM_STARTUP  1
#define J9RAS_DUMP_ON_VM_SHUTDOWN  2
#define J9RAS_DUMP_ON_CLASS_LOAD  4
#define J9RAS_DUMP_ON_CLASS_UNLOAD  8
#define J9RAS_DUMP_ON_EXCEPTION_THROW  16
#define J9RAS_DUMP_ON_EXCEPTION_CATCH  32
#define J9RAS_DUMP_ON_BREAKPOINT  64
#define J9RAS_DUMP_ON_DEBUG_FRAME_POP  0x80
#define J9RAS_DUMP_ON_THREAD_START  0x100
#define J9RAS_DUMP_ON_THREAD_BLOCKED  0x200
#define J9RAS_DUMP_ON_THREAD_END  0x400
#define J9RAS_DUMP_ON_HEAP_EXPAND  0x800
#define J9RAS_DUMP_ON_GLOBAL_GC  0x1000
#define J9RAS_DUMP_ON_GP_FAULT  0x2000
#define J9RAS_DUMP_ON_USER_SIGNAL  0x4000
#define J9RAS_DUMP_ON_EXCEPTION_DESCRIBE  0x8000
#define J9RAS_DUMP_ON_SLOW_EXCLUSIVE_ENTER  0x10000
#define J9RAS_DUMP_ON_ABORT_SIGNAL  0x20000
#define J9RAS_DUMP_ON_EXCEPTION_SYSTHROW  0x40000
#define J9RAS_DUMP_ON_TRACE_ASSERT  0x80000
#define J9RAS_DUMP_ON_USER_REQUEST  0x100000
#define J9RAS_DUMP_ON_OBJECT_ALLOCATION  0x200000
#define J9RAS_DUMP_ON_CORRUPT_CACHE  0x400000
#define J9RAS_DUMP_ON_EXCESSIVE_GC 0x800000
#define J9RAS_DUMP_ON_ANY 0x0FFFFFF /* mask of all bit flags above */

/* ...additional VM requests... */
#define J9RAS_DUMP_DO_EXCLUSIVE_VM_ACCESS  1
#define J9RAS_DUMP_DO_COMPACT_HEAP  2
#define J9RAS_DUMP_DO_PREPARE_HEAP_FOR_WALK  4
#define J9RAS_DUMP_DO_SUSPEND_OTHER_DUMPS  8
#define J9RAS_DUMP_DO_HALT_ALL_THREADS  16
#define J9RAS_DUMP_DO_ATTACH_THREAD  32
#define J9RAS_DUMP_DO_MULTIPLE_HEAPS  64
#define J9RAS_DUMP_DO_PREEMPT_THREADS  0x80

typedef struct J9RASdumpContext {
	struct J9JavaVM* javaVM;
	struct J9VMThread* onThread;
	UDATA eventFlags;
	struct J9RASdumpEventData* eventData;
	char* dumpList;
	UDATA dumpListSize;
	UDATA dumpListIndex;
} J9RASdumpContext;

typedef struct J9RASdumpEventData {
	UDATA detailLength;
	char* detailData;
	j9object_t* exceptionRef;
} J9RASdumpEventData;

typedef omr_error_t (*J9RASdumpFn)(struct J9RASdumpAgent *agent, char *label, struct J9RASdumpContext *context);

typedef struct J9RASdumpFunctions {
	void* reserved;
	omr_error_t  (*triggerOneOffDump)(struct J9JavaVM *vm, char *optionString, char *caller, char *fileName, size_t fileNameLength) ;
	omr_error_t  (*insertDumpAgent)(struct J9JavaVM *vm, struct J9RASdumpAgent *agent) ;
	omr_error_t  (*removeDumpAgent)(struct J9JavaVM *vm, struct J9RASdumpAgent *agent) ;
	omr_error_t  (*seekDumpAgent)(struct J9JavaVM *vm, struct J9RASdumpAgent **agentPtr, J9RASdumpFn dumpFn) ;
	omr_error_t  (*triggerDumpAgents)(struct J9JavaVM *vm, struct J9VMThread *self, UDATA eventFlags, struct J9RASdumpEventData *eventData) ;
	omr_error_t  (*setDumpOption)(struct J9JavaVM *vm, char *optionString) ;
	omr_error_t  (*resetDumpOptions)(struct J9JavaVM *vm) ;
	omr_error_t  (*queryVmDump)(struct J9JavaVM *vm, int buffer_size, void* options_buffer, int* data_size) ;
} J9RASdumpFunctions;

#endif /* j9dump_h */
