/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
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

	U_32 noProtect; /* If set, do not take dumps under their own signal handler. */
	U_32 noFailover; /* If set, do not failover to /tmp etc if unable to write dump. */

	U_32 dumpFlags; /* Flags to control java dump behaviour. */
} RasDumpGlobalStorage;

/* Flags on how to handle resolving native stack symbols. */
#define J9RAS_JAVADUMP_SHOW_NATIVE_STACK_SYMBOLS_BASIC 0x1
#define J9RAS_JAVADUMP_SHOW_NATIVE_STACK_SYMBOLS_ALL   0x2
/* Flag to show unmounted Thread stacktrace in java dump. */
#define J9RAS_JAVADUMP_SHOW_UNMOUNTED_THREAD_STACKS    0x4

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

/* Dump flags. Definitions must be simple so that DDR can process them. */
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
#define J9RAS_DUMP_ON_USER2_SIGNAL  0x1000000
#define J9RAS_DUMP_ON_VM_CRIU_CHECKPOINT  0x2000000
#define J9RAS_DUMP_ON_VM_CRIU_RESTORE  0x4000000
#define J9RAS_DUMP_ON_ANY 0x7FFFFFF /* mask of all bit flags above */

/* ...additional VM requests... */
#define J9RAS_DUMP_DO_EXCLUSIVE_VM_ACCESS  0x01
#define J9RAS_DUMP_DO_COMPACT_HEAP  0x02
#define J9RAS_DUMP_DO_PREPARE_HEAP_FOR_WALK  0x04
#define J9RAS_DUMP_DO_SUSPEND_OTHER_DUMPS  0x08
#define J9RAS_DUMP_DO_ATTACH_THREAD  0x10
#define J9RAS_DUMP_DO_MULTIPLE_HEAPS  0x020
#define J9RAS_DUMP_DO_PREEMPT_THREADS  0x40

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
#if defined(J9VM_OPT_CRIU_SUPPORT)
	IDATA  (*criuReloadXDumpAgents)(struct J9JavaVM *vm, struct J9VMInitArgs *j9vm_args) ;
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
} J9RASdumpFunctions;

#endif /* j9dump_h */
