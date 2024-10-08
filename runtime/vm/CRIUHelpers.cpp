/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>

#include "objhelp.h"
#include "j9.h"
#include "j9comp.h"
#include "j9protos.h"
#include "j9vmnls.h"
#include "jvminit.h"
#include "jvmtinls.h"
#include "ut_j9vm.h"
#include "util_api.h"
#include "vm_api.h"
#include "vmargs_api.h"

#include "CRIUHelpers.hpp"
#include "HeapIteratorAPI.h"
#include "ObjectAccessBarrierAPI.hpp"
#include "VMHelpers.hpp"

extern "C" {

J9_DECLARE_CONSTANT_UTF8(runPostRestoreHooks_sig, "()V");
J9_DECLARE_CONSTANT_UTF8(runPostRestoreHooks_name, "runPostRestoreHooksSingleThread");
J9_DECLARE_CONSTANT_UTF8(runPreCheckpointHooks_sig, "()V");
J9_DECLARE_CONSTANT_UTF8(runPreCheckpointHooks_name, "runPreCheckpointHooksSingleThread");
J9_DECLARE_CONSTANT_UTF8(j9InternalCheckpointHookAPI_name, "openj9/internal/criu/J9InternalCheckpointHookAPI");

static void addInternalJVMCheckpointHook(J9VMThread *currentThread, BOOLEAN isRestore, J9Class *instanceType, BOOLEAN includeSubClass, hookFunc hookFunc);
static void cleanupCriuHooks(J9VMThread *currentThread);
static BOOLEAN fillinHookRecords(J9VMThread *currentThread, j9object_t object);
static void initializeCriuHooks(J9VMThread *currentThread);
static BOOLEAN juRandomReseed(J9VMThread *currentThread, void *userData, const char **nlsMsgFormat);
static BOOLEAN criuRestoreInitializeTrace(J9VMThread *currentThread, void *userData, const char **nlsMsgFormat);
static BOOLEAN criuRestoreInitializeXrs(J9VMThread *currentThread, void *userData, const char **nlsMsgFormat);
static BOOLEAN criuRestoreDisableSharedClassCache(J9VMThread *currentThread, void *userData, const char **nlsMsgFormat);
static BOOLEAN criuRestoreInitializeDump(J9VMThread *currentThread, void *userData, const char **nlsMsgFormat);
static jvmtiIterationControl objectIteratorCallback(J9JavaVM *vm, J9MM_IterateObjectDescriptor *objectDesc, void *userData);
#if defined(OMR_GC_FULL_POINTERS)
UDATA debugBytecodeLoopFull(J9VMThread *currentThread);
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
UDATA debugBytecodeLoopCompressed(J9VMThread *currentThread);
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */

#define STRING_BUFFER_SIZE 256
#define ENV_FILE_BUFFER 1024

#define SUSPEND_THEADS_NO_DELAY_HALT_FLAG 0
#define SUSPEND_THEADS_WITH_DELAY_HALT_FLAG 1
#define RESUME_THREADS_WITH_DELAY_HALT_FLAG 2
#define RESUME_THREADS_NO_DELAY_HALT_FLAG 3

#define RESTORE_ARGS_RETURN_OK 0
#define RESTORE_ARGS_RETURN_OOM 1
#define RESTORE_ARGS_RETURN_OPTIONS_FILE_FAILED 2
#define RESTORE_ARGS_RETURN_ENV_VAR_FILE_FAILED 3

#define J9VM_DELAYCHECKPOINT_NOTCHECKPOINTSAFE 0x1
#define J9VM_DELAYCHECKPOINT_CLINIT 0x2

#define OPENJ9_RESTORE_OPTS_VAR "OPENJ9_RESTORE_JAVA_OPTIONS="

BOOLEAN
jvmCheckpointHooks(J9VMThread *currentThread)
{
	BOOLEAN result = TRUE;
	J9NameAndSignature nas = {0};
	nas.name = (J9UTF8 *)&runPreCheckpointHooks_name;
	nas.signature = (J9UTF8 *)&runPreCheckpointHooks_sig;

	/* initialize before running checkpoint hooks */
	initializeCriuHooks(currentThread);
	/* make sure Java hooks are the first thing run when initiating checkpoint */
	runStaticMethod(currentThread, J9UTF8_DATA(&j9InternalCheckpointHookAPI_name), &nas, 0, NULL);

	if (VM_VMHelpers::exceptionPending(currentThread)) {
		result = FALSE;
	}

	return result;
}

BOOLEAN
jvmRestoreHooks(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	BOOLEAN result = TRUE;
	J9NameAndSignature nas = {0};
	nas.name = (J9UTF8 *)&runPostRestoreHooks_name;
	nas.signature = (J9UTF8 *)&runPostRestoreHooks_sig;

	Assert_VM_true(isCRaCorCRIUSupportEnabled(vm));

	/* make sure Java hooks are the last thing run before restore */
	runStaticMethod(currentThread, J9UTF8_DATA(&j9InternalCheckpointHookAPI_name), &nas, 0, NULL);

	if (VM_VMHelpers::exceptionPending(currentThread)) {
		result = FALSE;
	}

	return result;
}

BOOLEAN
isCRaCorCRIUSupportEnabled(J9JavaVM *vm)
{
	return J9_IS_CRIU_OR_CRAC_CHECKPOINT_ENABLED(vm);
}

BOOLEAN
isCRIUSupportEnabled(J9VMThread *currentThread)
{
	return J9_ARE_ALL_BITS_SET(currentThread->javaVM->checkpointState.flags, J9VM_CRIU_IS_CHECKPOINT_ENABLED);
}

BOOLEAN
isCheckpointAllowed(J9JavaVM *vm)
{
	BOOLEAN result = FALSE;

	if (isCRaCorCRIUSupportEnabled(vm)) {
		result = J9_ARE_ALL_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_IS_CHECKPOINT_ALLOWED);
	}

	return result;
}

BOOLEAN
enableCRIUSecProvider(J9VMThread *currentThread)
{
	BOOLEAN result = FALSE;
	J9JavaVM *vm = currentThread->javaVM;

	if (isCRaCorCRIUSupportEnabled(vm)) {
		result = J9_ARE_ANY_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_ENABLE_CRIU_SEC_PROVIDER);
	}

	return result;
}

BOOLEAN
isNonPortableRestoreMode(J9VMThread *currentThread)
{
	return J9_ARE_ALL_BITS_SET(currentThread->javaVM->checkpointState.flags, J9VM_CRIU_IS_NON_PORTABLE_RESTORE_MODE);
}

BOOLEAN
isJVMInPortableRestoreMode(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	return (!isNonPortableRestoreMode(currentThread) || J9_ARE_ALL_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_IS_PORTABLE_JVM_RESTORE_MODE)) && isCRaCorCRIUSupportEnabled(vm);
}

BOOLEAN
isDebugOnRestoreEnabled(J9JavaVM *vm)
{
	return J9_ARE_NO_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_IS_JDWP_ENABLED)
			&& J9_ARE_ALL_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_SUPPORT_DEBUG_ON_RESTORE)
			&& isCRaCorCRIUSupportEnabled(vm);
}

void
setRequiredGhostFileLimit(J9VMThread *currentThread, U_32 ghostFileLimit)
{
	J9JavaVM *vm = currentThread->javaVM;

	/* Different components (VM, JIT, GC, Java code) may require different limits
	 * for the CRIU ghost files. This function ensures that the set limit is the
	 * maximum of all the required limits.
	 */
	if (ghostFileLimit > vm->checkpointState.requiredGhostFileLimit) {
		if (vm->checkpointState.criuSetGhostFileLimitFunctionPointerType) {
			vm->checkpointState.requiredGhostFileLimit = ghostFileLimit;
			vm->checkpointState.criuSetGhostFileLimitFunctionPointerType(ghostFileLimit);
		}
	}
}

/**
 * This adds an internal CRIU hook to trace all heap objects of instanceType and its subclasses if specified.
 *
 * @param[in] currentThread vmThread token
 * @param[in] isRestore If FALSE, run the hook at checkpoint, otherwise run at restore
 * @param[in] instanceType J9Class of type to be tracked
 * @param[in] includeSubClass If TRUE subclasses of instanceType should be included
 * @param[in] hookFunc The hook function to be invoked for the internal hook record
 *
 * @return void
 */
static void
addInternalJVMCheckpointHook(J9VMThread *currentThread, BOOLEAN isRestore, J9Class *instanceType, BOOLEAN includeSubClass, hookFunc hookFunc)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalHookRecord *newHook = (J9InternalHookRecord*)pool_newElement(vm->checkpointState.hookRecords);
	if (NULL == newHook) {
		setNativeOutOfMemoryError(currentThread, 0, 0);
	} else {
		newHook->isRestore = isRestore;
		newHook->instanceType = instanceType;
		newHook->includeSubClass = includeSubClass;
		newHook->hookFunc = hookFunc;
		/* newHook->instanceObjects is to be lazily initialized */
	}
}

/**
 * An internal JVM checkpoint hook is to re-seed java.util.Random.seed.value.
 *
 * @param[in] currentThread vmThread token
 * @param[in] userData J9InternalHookRecord pointer
 * @param[in/out] nlsMsgFormat an NLS message
 *
 * @return BOOLEAN TRUE if no error, otherwise FALSE
 */
static BOOLEAN
juRandomReseed(J9VMThread *currentThread, void *userData, const char **nlsMsgFormat)
{
	BOOLEAN result = TRUE;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalHookRecord *hookRecord = (J9InternalHookRecord*)userData;
	MM_ObjectAccessBarrierAPI objectAccessBarrier = MM_ObjectAccessBarrierAPI(currentThread);
	PORT_ACCESS_FROM_VMC(currentThread);

	/* Assuming this hook record is to re-seed java.util.Random.seed.value. */
	IDATA seedOffset = VM_VMHelpers::findinstanceFieldOffset(currentThread, hookRecord->instanceType, "seed", "Ljava/util/concurrent/atomic/AtomicLong;");
	if (-1 != seedOffset) {
#define JUCA_ATOMICLONG "java/util/concurrent/atomic/AtomicLong"
		J9Class *jucaAtomicLongClass = peekClassHashTable(currentThread, vm->systemClassLoader, (U_8 *)JUCA_ATOMICLONG, LITERAL_STRLEN(JUCA_ATOMICLONG));
#undef JUCA_ATOMICLONG
		if (NULL != jucaAtomicLongClass) {
			IDATA valueOffset = VM_VMHelpers::findinstanceFieldOffset(currentThread, jucaAtomicLongClass, "value", "J");
			if (-1 != valueOffset) {
				PORT_ACCESS_FROM_JAVAVM(vm);
				pool_state walkState = {0};
				j9object_t *objectRecord = (j9object_t*)pool_startDo(hookRecord->instanceObjects, &walkState);
				while (NULL != objectRecord) {
					j9object_t object = *objectRecord;
					j9object_t seedObject = objectAccessBarrier.inlineMixedObjectReadObject(currentThread, object, seedOffset, FALSE);
					I_64 valueLong = objectAccessBarrier.inlineMixedObjectReadI64(currentThread, seedObject, valueOffset, TRUE);

					/* Algorithm used in Random.seedUniquifier(). */
					valueLong *= 1181783497276652981;
					valueLong ^= j9time_nano_time();
					objectAccessBarrier.inlineMixedObjectStoreI64(currentThread, seedObject, valueOffset, valueLong, TRUE);

					objectRecord = (j9object_t*)pool_nextDo(&walkState);
				}
			} else {
				Trc_VM_criu_jur_invalid_valueOffset(currentThread, jucaAtomicLongClass);
				result = FALSE;
				*nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
						J9NLS_VM_CRIU_RESTORE_RESEED_INVALID_VALUEOFFSET, NULL);
			}
		} else {
			Trc_VM_criu_jur_AtomicLong_CNF(currentThread);
			result = FALSE;
			*nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
					J9NLS_VM_CRIU_RESTORE_RESEED_ATOMICLONG_CNF, NULL);
		}
	} else {
		Trc_VM_criu_jur_invalid_seedOffset(currentThread, hookRecord->instanceType);
		result = FALSE;
		*nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
				J9NLS_VM_CRIU_RESTORE_RESEED_INVALID_SEEDOFFSET, NULL);
	}

	return result;
}

/**
 * An internal JVM checkpoint hook to initialize trace after restore.
 *
 * @param[in] currentThread vmThread token
 * @param[in] userData J9InternalHookRecord pointer
 * @param[in/out] nlsMsgFormat an NLS message
 *
 * @return BOOLEAN TRUE if no error, otherwise FALSE
 */
static BOOLEAN
criuRestoreInitializeTrace(J9VMThread *currentThread, void *userData, const char **nlsMsgFormat)
{
	BOOLEAN result = TRUE;
	J9JavaVM *vm = currentThread->javaVM;

	if (NULL != vm->checkpointState.restoreArgsList) {
		if (FIND_ARG_IN_ARGS_FORWARD(vm->checkpointState.restoreArgsList, OPTIONAL_LIST_MATCH, VMOPT_XTRACE, NULL) >= 0) {
			RasGlobalStorage *j9ras = (RasGlobalStorage *)vm->j9rasGlobalStorage;
			if ((NULL == j9ras)
				|| (NULL == j9ras->criuRestoreInitializeTrace)
				|| !j9ras->criuRestoreInitializeTrace(currentThread)
			) {
				PORT_ACCESS_FROM_VMC(currentThread);
				*nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
						J9NLS_VM_CRIU_RESTORE_INITIALIZE_TRACE_FAILED, NULL);
				result = FALSE;
			} else {
				vm->checkpointState.flags |=  J9VM_CRIU_TRANSITION_TO_DEBUG_INTERPRETER;
			}
		}
	}
	return result;
}

/**
 * An internal JVM checkpoint hook to initialize -Xrs after restore. This only
 * supports -Xrs options with "onRestore" appended to it.
 *
 * @param[in] currentThread vmThread token
 * @param[in] userData J9InternalHookRecord pointer
 * @param[in/out] nlsMsgFormat an NLS message
 *
 * @return Always returns TRUE
 */
static BOOLEAN
criuRestoreInitializeXrs(J9VMThread *currentThread, void *userData, const char **nlsMsgFormat)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9VMInitArgs *args = vm->checkpointState.restoreArgsList;

	if (NULL != args) {
		IDATA argIndex = 0;
		if ((argIndex = FIND_ARG_IN_ARGS(args, OPTIONAL_LIST_MATCH, VMOPT_XRS, NULL)) >= 0) {
			bool argProcessed = false;
			char* optionValue = NULL;
			U_32 sigOptions = 0;

			GET_OPTION_VALUE_ARGS(args, argIndex, ':', &optionValue);
			if (NULL != optionValue) {
				if (0 == strcmp(optionValue, "syncOnRestore")) {
					vm->sigFlags |= J9_SIG_XRS_SYNC;
					sigOptions |= J9PORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS;
					argProcessed = true;
				} else if (0 == strcmp(optionValue, "asyncOnRestore")) {
					vm->sigFlags |= (J9_SIG_XRS_ASYNC | J9_SIG_NO_SIG_QUIT | J9_SIG_NO_SIG_USR2);
					sigOptions |= J9PORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS;
					argProcessed = true;
				} else if (0 == strcmp(optionValue, "onRestore")) {
					vm->sigFlags |= (J9_SIG_XRS_SYNC | J9_SIG_XRS_ASYNC | J9_SIG_NO_SIG_QUIT | J9_SIG_NO_SIG_USR2);
					sigOptions |= (J9PORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS | J9PORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS);
					argProcessed = true;
				}
			}

			if (argProcessed) {
				PORT_ACCESS_FROM_VMC(currentThread);
				CONSUME_ARG(args, argIndex);
				j9sig_set_options(sigOptions);
			}
		}
	}

	/* In cases of error (arg options are not supported) the entire args is not consumed.
	 * Return TRUE as checkpointJVMImpl will throw an exception if any arg is not consumed.
	 */
	return TRUE;
}

/**
 * An internal JVM checkpoint hook to disable the shared class cache if the -Xshareclasses:disableOnRestore is present
 *
 * @param[in] currentThread vmThread token
 * @param[in] userData J9InternalHookRecord pointer
 *
 * @return TRUE (there will never be an error in this function)
*/
static BOOLEAN
criuRestoreDisableSharedClassCache(J9VMThread *currentThread, void *userData, const char **nlsMsgFormat)
{
	J9JavaVM *vm = currentThread->javaVM;

	if (NULL != vm->checkpointState.restoreArgsList) {
		if (FIND_AND_CONSUME_ARG(vm->checkpointState.restoreArgsList, EXACT_MATCH, VMOPT_XSHARECLASSES_DISABLEONRESTORE, NULL) >= 0) {
			if (NULL != vm->sharedClassConfig) {
				vm->sharedClassConfig->disableSharedClassCacheForCriuRestore(vm);
			}
		}
	}

	return TRUE;
}

/**
 * An internal JVM checkpoint hook to initialize dump after restore.
 *
 * @param[in] currentThread vmThread token
 * @param[in] userData J9InternalHookRecord pointer
 * @param[in/out] nlsMsgFormat an NLS message
 *
 * @return BOOLEAN TRUE if no error, otherwise FALSE
 */
static BOOLEAN
criuRestoreInitializeDump(J9VMThread *currentThread, void *userData, const char **nlsMsgFormat)
{
	BOOLEAN result = TRUE;
	J9JavaVM *vm = currentThread->javaVM;

	if (NULL != vm->checkpointState.restoreArgsList) {
		if (FIND_ARG_IN_ARGS_FORWARD(vm->checkpointState.restoreArgsList, OPTIONAL_LIST_MATCH, VMOPT_XDUMP, NULL) >= 0) {
			if (J9VMDLLMAIN_OK != vm->j9rasDumpFunctions->criuReloadXDumpAgents(vm, vm->checkpointState.restoreArgsList)) {
				PORT_ACCESS_FROM_VMC(currentThread);
				*nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
						J9NLS_VM_CRIU_RESTORE_INITIALIZE_DUMP_FAILED, NULL);
				result = FALSE;
			} else {
				vm->checkpointState.flags |=  J9VM_CRIU_TRANSITION_TO_DEBUG_INTERPRETER;
			}
		}
	}
	return result;
}

/**
 * This cleans up the instanceObjects associated with each J9JavaVM->checkpointState.hookRecords,
 * the hookRecords and classIterationRestoreHookRecords as well if checkpointState.isNonPortableRestoreMode is TRUE.
 *
 * @param[in] currentThread vmThread token
 *
 * @return void
 */
static void
cleanupCriuHooks(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9Pool *hookRecords = vm->checkpointState.hookRecords;
	if (NULL != hookRecords) {
		pool_state walkState = {0};
		J9InternalHookRecord *hookRecord = (J9InternalHookRecord*)pool_startDo(hookRecords, &walkState);
		while (NULL != hookRecord) {
			pool_kill(hookRecord->instanceObjects);
			hookRecord->instanceObjects = NULL;
			hookRecord = (J9InternalHookRecord*)pool_nextDo(&walkState);
		}

		if (J9_ARE_ALL_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_IS_NON_PORTABLE_RESTORE_MODE)) {
			/* No more checkpoint, cleanup hook records. */
			pool_kill(vm->checkpointState.hookRecords);
			vm->checkpointState.hookRecords = NULL;
		}
	}

	J9Pool *classIterationRestoreHookRecords = vm->checkpointState.classIterationRestoreHookRecords;
	if ((NULL != classIterationRestoreHookRecords) && J9_ARE_ALL_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_IS_NON_PORTABLE_RESTORE_MODE)) {
		/* No more checkpoint, cleanup hook records. */
		pool_kill(vm->checkpointState.classIterationRestoreHookRecords);
		vm->checkpointState.classIterationRestoreHookRecords = NULL;
	}
}

/**
 * This initializes J9JavaVM->checkpointState.hookRecords, classIterationRestoreHookRecords and
 * delayedLockingOperationsRecords, adds an internal JVMCheckpointHook to re-seed java.util.Random.seed.value.
 *
 * @param[in] currentThread vmThread token
 *
 * @return void
 */
static void
initializeCriuHooks(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;

	Trc_VM_criu_initHooks_Entry(currentThread);
	cleanupCriuHooks(currentThread);
	if (NULL == vm->checkpointState.hookRecords) {
		vm->checkpointState.hookRecords = pool_new(sizeof(J9InternalHookRecord), 0, 0, 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(vm->portLibrary));
		if (NULL == vm->checkpointState.hookRecords) {
			setNativeOutOfMemoryError(currentThread, 0, 0);
			goto done;
		}
	}

	if (NULL == vm->checkpointState.classIterationRestoreHookRecords) {
		vm->checkpointState.classIterationRestoreHookRecords = pool_new(sizeof(J9InternalClassIterationRestoreHookRecord), 0, 0, 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(vm->portLibrary));
		if (NULL == vm->checkpointState.classIterationRestoreHookRecords) {
			setNativeOutOfMemoryError(currentThread, 0, 0);
			goto done;
		}
	}

	if (NULL == vm->checkpointState.delayedLockingOperationsRecords) {
		vm->checkpointState.delayedLockingOperationsRecords = pool_new(sizeof(J9DelayedLockingOpertionsRecord), 0, 0, 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(vm->portLibrary));
		if (NULL == vm->checkpointState.delayedLockingOperationsRecords) {
			setNativeOutOfMemoryError(currentThread, 0, 0);
			goto done;
		}
	}

#if JAVA_SPEC_VERSION >= 20
	{
		J9VMSystemProperty *vtParallelism = NULL;
		PORT_ACCESS_FROM_VMC(currentThread);
		if (J9SYSPROP_ERROR_NONE != getSystemProperty(vm, "jdk.virtualThreadScheduler.parallelism", &vtParallelism)) {
			/* This system property only affects j.l.VirtualThread.ForkJoinPool.parallelism at VM startup. */
			UDATA cpuCount = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_TARGET);
			if (cpuCount < 1) {
				cpuCount = 1;
			}
			vm->checkpointState.checkpointCPUCount = cpuCount;
			Trc_VM_criu_initializeCriuHooks_checkpointCPUCount(currentThread, cpuCount);
		}
	}
#endif /* JAVA_SPEC_VERSION >= 20 */

	{
		/* Add restore hook to re-seed java.uti.Random.seed.value */
#define JAVA_UTIL_RANDOM "java/util/Random"
		J9Class *juRandomClass = peekClassHashTable(currentThread, vm->systemClassLoader, (U_8 *)JAVA_UTIL_RANDOM, LITERAL_STRLEN(JAVA_UTIL_RANDOM));
#undef JAVA_UTIL_RANDOM
		if (NULL != juRandomClass) {
			addInternalJVMCheckpointHook(currentThread, TRUE, juRandomClass, FALSE, juRandomReseed);
		} else {
			Trc_VM_criu_initializeCriuHooks_Random_CNF(currentThread);
		}
		addInternalJVMCheckpointHook(currentThread, TRUE, NULL, FALSE, criuRestoreInitializeTrace);
		addInternalJVMCheckpointHook(currentThread, TRUE, NULL, FALSE, criuRestoreInitializeXrs);
		addInternalJVMCheckpointHook(currentThread, TRUE, NULL, FALSE, criuRestoreInitializeDump);
	}

	addInternalJVMCheckpointHook(currentThread, TRUE, NULL, FALSE, criuRestoreDisableSharedClassCache);

done:
	Trc_VM_criu_initHooks_Exit(currentThread, vm->checkpointState.hookRecords,
		vm->checkpointState.classIterationRestoreHookRecords, vm->checkpointState.delayedLockingOperationsRecords);
}

/**
 * After initializeCriuHooks() and before invoking JVM CRIU hooks, j9mm_iterate_all_objects iterates the heap,
 * for each object discovered, this will be called to iterate CRIU internal hook records, and fills in all instances of
 * instanceType and its subclasses if specified.
 *
 * @param[in] currentThread vmThread token
 * @param[in] object an instance to be checked and filled in if it is the instanceType specified
 *
 * @return BOOLEAN TRUE if no error, otherwise FALSE
 */
static BOOLEAN
fillinHookRecords(J9VMThread *currentThread, j9object_t object)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9Pool *hookRecords = vm->checkpointState.hookRecords;
	pool_state walkState = {0};
	BOOLEAN result = TRUE;

	J9InternalHookRecord *hookRecord = (J9InternalHookRecord*)pool_startDo(hookRecords, &walkState);
	J9Class *clazz = J9OBJECT_CLAZZ_VM(vm, object);
	while (NULL != hookRecord) {
		if ((clazz == hookRecord->instanceType)
			|| (hookRecord->includeSubClass && isSameOrSuperClassOf(hookRecord->instanceType, clazz))
		) {
			if (NULL == hookRecord->instanceObjects) {
				hookRecord->instanceObjects = pool_new(sizeof(j9object_t), 0, 0, 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(vm->portLibrary));
				if (NULL == hookRecord->instanceObjects) {
					setNativeOutOfMemoryError(currentThread, 0, 0);
					result = FALSE;
					break;
				}
			}
			j9object_t *objectRecord = (j9object_t*)pool_newElement(hookRecord->instanceObjects);
			if (NULL == objectRecord) {
				result = FALSE;
				setNativeOutOfMemoryError(currentThread, 0, 0);
			} else {
				*objectRecord = object;
			}
		}

		hookRecord = (J9InternalHookRecord*)pool_nextDo(&walkState);
	}

	return result;
}

static jvmtiIterationControl
objectIteratorCallback(J9JavaVM *vm, J9MM_IterateObjectDescriptor *objectDesc, void *userData)
{
	J9VMThread *currentThread = (J9VMThread *)userData;
	j9object_t object = objectDesc->object;

	fillinHookRecords(currentThread, object);

	return JVMTI_ITERATION_CONTINUE;
}

BOOLEAN
runInternalJVMCheckpointHooks(J9VMThread *currentThread, const char **nlsMsgFormat)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9Pool *hookRecords = vm->checkpointState.hookRecords;
	pool_state walkState = {0};
	BOOLEAN result = TRUE;

	Trc_VM_criu_runCheckpointHooks_Entry(currentThread);

	/* Iterate heap objects to prepare internal hooks at checkpoint. */
	vm->memoryManagerFunctions->j9mm_iterate_all_objects(vm, vm->portLibrary, 0, objectIteratorCallback, currentThread);

	J9InternalHookRecord *hookRecord = (J9InternalHookRecord*)pool_startDo(hookRecords, &walkState);
	while (NULL != hookRecord) {
		if (!hookRecord->isRestore) {
			if (FALSE == hookRecord->hookFunc(currentThread, hookRecord, nlsMsgFormat)) {
				result = FALSE;
				break;
			}
		}
		hookRecord = (J9InternalHookRecord*)pool_nextDo(&walkState);
	}
	Trc_VM_criu_runCheckpointHooks_Exit(currentThread);

	return result;
}

BOOLEAN
runInternalJVMRestoreHooks(J9VMThread *currentThread, const char **nlsMsgFormat)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9Pool *hookRecords = vm->checkpointState.hookRecords;
	J9Pool *classIterationRestoreHookRecords = vm->checkpointState.classIterationRestoreHookRecords;
	pool_state walkState = {0};
	BOOLEAN result = TRUE;

	Trc_VM_criu_runRestoreHooks_Entry(currentThread);
	J9InternalHookRecord *hookRecord = (J9InternalHookRecord*)pool_startDo(hookRecords, &walkState);
	while (NULL != hookRecord) {
		if (hookRecord->isRestore) {
			result = hookRecord->hookFunc(currentThread, hookRecord, nlsMsgFormat);
			if (!result) {
				break;
			}
		}
		hookRecord = (J9InternalHookRecord*)pool_nextDo(&walkState);
	}

	if (result) {
		if ((NULL != classIterationRestoreHookRecords) && (pool_numElements(classIterationRestoreHookRecords)) > 0) {
			J9ClassWalkState j9ClassWalkState = {0};
			J9Class *clazz = allClassesStartDo(&j9ClassWalkState, vm, NULL);
			while (NULL != clazz) {
				J9InternalClassIterationRestoreHookRecord *hookRecord = (J9InternalClassIterationRestoreHookRecord*)pool_startDo(classIterationRestoreHookRecords, &walkState);
				while (NULL != hookRecord) {
					result = hookRecord->hookFunc(currentThread, clazz, nlsMsgFormat);
					if (!result) {
						break;
					}
					hookRecord = (J9InternalClassIterationRestoreHookRecord*)pool_nextDo(&walkState);
				}
				clazz = vm->internalVMFunctions->allClassesNextDo(&j9ClassWalkState);
			}
			vm->internalVMFunctions->allClassesEndDo(&j9ClassWalkState);
		}
	}

#if JAVA_SPEC_VERSION >= 20
	if (0 != vm->checkpointState.checkpointCPUCount) {
		/* Only reset j.l.VirtualThread.ForkJoinPool.parallelism if jdk.virtualThreadScheduler.parallelism is not set. */
		PORT_ACCESS_FROM_VMC(currentThread);
		UDATA cpuCount = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_TARGET);
		if (cpuCount < 1) {
			/* This matches ForkJoinPool default constructor with Runtime.getRuntime().availableProcessors(). */
			cpuCount = 1;
		}
		if (cpuCount != vm->checkpointState.checkpointCPUCount) {
			j9object_t fjpObject = VM_VMHelpers::getStaticFieldObject(currentThread, "java/lang/VirtualThread", "DEFAULT_SCHEDULER", "Ljava/util/concurrent/ForkJoinPool;");
			if (NULL != fjpObject) {
				result = VM_VMHelpers::resetJUCForkJoinPoolParallelism(currentThread, fjpObject, cpuCount);
				Trc_VM_criu_jlVirtualThreadForkJoinPoolResetParallelism_reset_parallelism(currentThread, fjpObject, cpuCount);
			} else {
				Trc_VM_criu_jlVirtualThreadForkJoinPoolResetParallelism_DEFAULT_SCHEDULER_NULL(currentThread);
			}
		} else {
			Trc_VM_criu_jlVirtualThreadForkJoinPoolResetParallelism_same_cpucount(currentThread, cpuCount);
		}
	}
#endif /* JAVA_SPEC_VERSION >= 20 */

	if (J9_ARE_ALL_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_IS_NON_PORTABLE_RESTORE_MODE)) {
		PORT_ACCESS_FROM_JAVAVM(vm);
		vm->portLibrary->isCheckPointAllowed = FALSE;
		vm->checkpointState.flags &= ~J9VM_CRIU_IS_CHECKPOINT_ALLOWED;
		j9port_control(J9PORT_CTLDATA_CRIU_SUPPORT_FLAGS, OMRPORT_CRIU_SUPPORT_ENABLED | J9OMRPORT_CRIU_SUPPORT_FINAL_RESTORE);
	}

	TRIGGER_J9HOOK_VM_PREPARING_FOR_RESTORE(vm->hookInterface, currentThread);

	/* Cleanup at restore. */
	cleanupCriuHooks(currentThread);
	Trc_VM_criu_runRestoreHooks_Exit(currentThread);

	return result;
}

BOOLEAN
runDelayedLockRelatedOperations(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9DelayedLockingOpertionsRecord *delayedLockingOperation = static_cast<J9DelayedLockingOpertionsRecord*>(J9_LINKED_LIST_START_DO(vm->checkpointState.delayedLockingOperationsRoot));
	BOOLEAN rc = TRUE;

	Assert_VM_true(vm->checkpointState.checkpointThread == currentThread);

	while (NULL != delayedLockingOperation) {
		j9object_t instance = J9_JNI_UNWRAP_REFERENCE(delayedLockingOperation->globalObjectRef);
		omrthread_monitor_t monitorPtr = NULL;
		if (J9_SINGLE_THREAD_MODE_OP_INTERRUPT == delayedLockingOperation->operation) {
			VM_VMHelpers::threadInterruptImpl(currentThread, instance);
			Trc_VM_criu_runDelayedLockRelatedOperations_runDelayedInterrupt(currentThread, J9_SINGLE_THREAD_MODE_OP_INTERRUPT, instance);
			goto next;
		}

		if (!VM_ObjectMonitor::inlineFastObjectMonitorEnter(currentThread, instance)) {
			rc = objectMonitorEnterNonBlocking(currentThread, instance);
			if (J9_OBJECT_MONITOR_BLOCKING == rc) {
				/* owned by another thread */
				Trc_VM_criu_runDelayedLockRelatedOperations_contendedMonitorEnter(currentThread, instance);
				rc = FALSE;
				goto done;
			} else if (J9_OBJECT_MONITOR_ENTER_FAILED(rc)) {
				/* not possible if the the application was able to call notify earlier */
				Assert_VM_unreachable();
			}
		}
		if (!VM_ObjectMonitor::getMonitorForNotify(currentThread, instance, &monitorPtr, true)) {
			if (NULL != monitorPtr) {
				/* another thread owns the lock, shouldn't be possible */
				Trc_VM_criu_runDelayedLockRelatedOperations_contendedMonitorEnter2(currentThread, monitorPtr);
				rc = FALSE;
				goto done;
			} else {
				/* no waiters */
				goto exitMonitor;
			}
		}

		Trc_VM_criu_runDelayedLockRelatedOperations_runDelayedOperation(currentThread, delayedLockingOperation->operation, instance, monitorPtr);

		switch(delayedLockingOperation->operation) {
		case J9_SINGLE_THREAD_MODE_OP_NOTIFY:
			rc = 0 == omrthread_monitor_notify(monitorPtr);
			break;
		case J9_SINGLE_THREAD_MODE_OP_NOTIFY_ALL:
			rc = 0 == omrthread_monitor_notify_all(monitorPtr);
			break;
		default:
			Assert_VM_unreachable();
			break;
		}

		if (!rc) {
			goto done;
		}

exitMonitor:
		if (!VM_ObjectMonitor::inlineFastObjectMonitorExit(currentThread, instance)) {
			if (0 != objectMonitorExit(currentThread, instance)) {
				Assert_VM_unreachable();
			}
		}

next:
		j9jni_deleteGlobalRef((JNIEnv*) currentThread, delayedLockingOperation->globalObjectRef, JNI_FALSE);
		J9DelayedLockingOpertionsRecord *lastOperation = delayedLockingOperation;
		delayedLockingOperation = J9_LINKED_LIST_NEXT_DO(vm->checkpointState.delayedLockingOperationsRoot, delayedLockingOperation);
		pool_removeElement(vm->checkpointState.delayedLockingOperationsRecords, lastOperation);
	}

done:
	vm->checkpointState.delayedLockingOperationsRoot = NULL;

	return rc;
}

BOOLEAN
delayedLockingOperation(J9VMThread *currentThread, j9object_t instance, UDATA operation)
{
	return VM_CRIUHelpers::delayedLockingOperation(currentThread, instance, operation);
}

void
addInternalJVMClassIterationRestoreHook(J9VMThread *currentThread, classIterationRestoreHookFunc hookFunc)
{
	VM_CRIUHelpers::addInternalJVMClassIterationRestoreHook(currentThread, hookFunc);
}


jobject
getRestoreSystemProperites(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	jobject returnProperties = NULL;

	if (NULL != vm->checkpointState.restoreArgsList) {
		JavaVMInitArgs *restorArgs = vm->checkpointState.restoreArgsList->actualVMArgs;
		 J9CmdLineOption *j9Options = vm->checkpointState.restoreArgsList->j9Options;
		J9MemoryManagerFunctions *mmfns = vm->memoryManagerFunctions;
		PORT_ACCESS_FROM_JAVAVM(vm);

		UDATA count = 0;
		JavaVMOption *restorArgOptions = restorArgs->options;

		for (IDATA i = 0; i < restorArgs->nOptions; ++i) {
			char *optionString = restorArgOptions[i].optionString;

			if (strncmp("-D", optionString, 2) == 0) {
				count++;
			}
		}

		internalEnterVMFromJNI(currentThread);

		UDATA arrayEntries = count * 2;
		j9object_t newArray = mmfns->J9AllocateIndexableObject(currentThread, ((J9Class*) J9VMJAVALANGSTRING_OR_NULL(vm))->arrayClass, arrayEntries, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);

		if (NULL == newArray) {
			setHeapOutOfMemoryError(currentThread);
			goto done;
		}

		for (IDATA i = 0, index = 0; i < restorArgs->nOptions; ++i) {
			char *optionString = restorArgOptions[i].optionString;

			if (0 == strncmp("-D", optionString, 2)) {
				char *propValue = NULL;
				char *propValueCopy = NULL;
				char *propNameCopy = NULL;
				UDATA propNameLen = 0;

				propValue = strchr(optionString + 2, '=');
				if (NULL == propValue) {
					propNameLen = strlen(optionString) - 2;
					propValue = optionString + 2 + propNameLen;
				} else {
					propNameLen = propValue - (optionString + 2);
					++propValue;
				}

				UDATA valueLength = strlen(propValue);

				propNameCopy = (char *)getMUtf8String(vm, optionString + 2, propNameLen);
				if (NULL == propNameCopy) {
					setNativeOutOfMemoryError(currentThread, 0, 0);
					goto done;
				}
				propValueCopy = (char *)getMUtf8String(vm, propValue, valueLength);
				if (NULL == propValueCopy) {
					j9mem_free_memory(propNameCopy);
					setNativeOutOfMemoryError(currentThread, 0, 0);
					goto done;
				}
				PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, (j9object_t)newArray);
				j9object_t newObject = mmfns->j9gc_createJavaLangString(currentThread, (U_8 *)propNameCopy, propNameLen, J9_STR_TENURE);
				if (NULL == newObject) {
					j9mem_free_memory(propNameCopy);
					j9mem_free_memory(propValueCopy);
					setHeapOutOfMemoryError(currentThread);
					goto done;
				}
				newArray = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);

				J9JAVAARRAYOFOBJECT_STORE(currentThread, newArray, index, newObject);
				index += 1;

				PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, (j9object_t)newArray);
				newObject = mmfns->j9gc_createJavaLangString(currentThread, (U_8 *)propValueCopy, valueLength, J9_STR_TENURE);
				if (NULL == newObject) {
					j9mem_free_memory(propNameCopy);
					j9mem_free_memory(propValueCopy);
					setHeapOutOfMemoryError(currentThread);
					goto done;
				}
				newArray = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);

				J9JAVAARRAYOFOBJECT_STORE(currentThread, newArray, index, newObject);
				index += 1;

				j9mem_free_memory(propNameCopy);
				j9mem_free_memory(propValueCopy);

				j9Options[i].flags |= ARG_CONSUMED;
			}
		}

		returnProperties = j9jni_createLocalRef((JNIEnv*)currentThread, newArray);
		if (NULL == returnProperties) {
			setNativeOutOfMemoryError(currentThread, 0, 0);
		}

done:
		internalExitVMToJNI(currentThread);
	}

	return returnProperties;
}

BOOLEAN
setupJNIFieldIDsAndCRIUAPI(JNIEnv *env, jclass *currentExceptionClass, IDATA *systemReturnCode, const char **nlsMsgFormat)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9CRIUCheckpointState *vmCheckpointState = &vm->checkpointState;
	jclass criuJVMCheckpointExceptionClass = NULL;
	jclass criuSystemCheckpointExceptionClass = NULL;
	jclass criuJVMRestoreExceptionClass = NULL;
	jclass criuSystemRestoreExceptionClass = NULL;
	bool returnCode = true;
	PORT_ACCESS_FROM_VMC(currentThread);
	IDATA libCRIUReturnCode = 0;

	criuJVMCheckpointExceptionClass = env->FindClass("openj9/internal/criu/JVMCheckpointException");
	Assert_VM_criu_notNull(criuJVMCheckpointExceptionClass);
	vmCheckpointState->criuJVMCheckpointExceptionClass = (jclass)env->NewGlobalRef(criuJVMCheckpointExceptionClass);
	vmCheckpointState->criuJVMCheckpointExceptionInit = env->GetMethodID(criuJVMCheckpointExceptionClass, "<init>", "(Ljava/lang/String;I)V");

	criuSystemCheckpointExceptionClass = env->FindClass("openj9/internal/criu/SystemCheckpointException");
	Assert_VM_criu_notNull(criuSystemCheckpointExceptionClass);
	vmCheckpointState->criuSystemCheckpointExceptionClass = (jclass)env->NewGlobalRef(criuSystemCheckpointExceptionClass);
	vmCheckpointState->criuSystemCheckpointExceptionInit = env->GetMethodID(criuSystemCheckpointExceptionClass, "<init>", "(Ljava/lang/String;I)V");

	criuJVMRestoreExceptionClass = env->FindClass("openj9/internal/criu/JVMRestoreException");
	Assert_VM_criu_notNull(criuJVMRestoreExceptionClass);
	vmCheckpointState->criuJVMRestoreExceptionClass = (jclass)env->NewGlobalRef(criuJVMRestoreExceptionClass);
	vmCheckpointState->criuJVMRestoreExceptionInit = env->GetMethodID(criuJVMRestoreExceptionClass, "<init>", "(Ljava/lang/String;I)V");

	criuSystemRestoreExceptionClass = env->FindClass("openj9/internal/criu/SystemRestoreException");
	Assert_VM_criu_notNull(criuSystemRestoreExceptionClass);
	vmCheckpointState->criuSystemRestoreExceptionClass = (jclass)env->NewGlobalRef(criuSystemRestoreExceptionClass);
	vmCheckpointState->criuSystemRestoreExceptionInit = env->GetMethodID(criuSystemRestoreExceptionClass, "<init>", "(Ljava/lang/String;I)V");

	if ((NULL == vmCheckpointState->criuSystemRestoreExceptionInit)
		|| (NULL == vmCheckpointState->criuJVMRestoreExceptionInit)
		|| (NULL == vmCheckpointState->criuSystemCheckpointExceptionInit)
		|| (NULL == vmCheckpointState->criuJVMCheckpointExceptionInit)
	) {
		/* pending exception already set */
		Trc_VM_criu_setupJNIFieldIDsAndCRIUAPI_null_init(currentThread,
				vmCheckpointState->criuSystemRestoreExceptionInit, vmCheckpointState->criuJVMRestoreExceptionInit,
				vmCheckpointState->criuSystemCheckpointExceptionInit, vmCheckpointState->criuJVMCheckpointExceptionInit);
		returnCode = false;
		goto done;
	}

	if ((NULL == vmCheckpointState->criuJVMCheckpointExceptionClass)
		|| (NULL == vmCheckpointState->criuSystemCheckpointExceptionClass)
		|| (NULL == vmCheckpointState->criuJVMRestoreExceptionClass)
		|| (NULL == vmCheckpointState->criuSystemRestoreExceptionClass)
	) {
		internalEnterVMFromJNI(currentThread);
		setNativeOutOfMemoryError(currentThread, 0, 0);
		internalExitVMToJNI(currentThread);
		Trc_VM_criu_setupJNIFieldIDsAndCRIUAPI_null_exception_class(currentThread,
				vmCheckpointState->criuJVMCheckpointExceptionClass, vmCheckpointState->criuSystemCheckpointExceptionClass,
				vmCheckpointState->criuJVMRestoreExceptionClass, vmCheckpointState->criuSystemRestoreExceptionClass);
		returnCode = false;
		goto done;
	}

	libCRIUReturnCode = j9sl_open_shared_library((char*)"criu", &vmCheckpointState->libCRIUHandle, OMRPORT_SLOPEN_DECORATE | OMRPORT_SLOPEN_LAZY);
	if (libCRIUReturnCode != OMRPORT_SL_FOUND) {
		*currentExceptionClass = criuSystemCheckpointExceptionClass;
		*systemReturnCode = libCRIUReturnCode;
		*nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_CRIU_LOADING_LIBCRIU_FAILED, NULL);
		Trc_VM_criu_setupJNIFieldIDsAndCRIUAPI_load_criu_failure(currentThread, systemReturnCode);
		returnCode = false;
		goto done;
	}

	/* the older criu libraries do not contain the criu_set_unprivileged function so we can do a NULL check before calling it */
	j9sl_lookup_name(vmCheckpointState->libCRIUHandle, (char*)"criu_set_unprivileged", (UDATA*)&vmCheckpointState->criuSetUnprivilegedFunctionPointerType, "VZ");

	if (j9sl_lookup_name(vmCheckpointState->libCRIUHandle, (char*)"criu_set_images_dir_fd", (UDATA*)&vmCheckpointState->criuSetImagesDirFdFunctionPointerType, "VI")
		|| j9sl_lookup_name(vmCheckpointState->libCRIUHandle, (char*)"criu_set_shell_job", (UDATA*)&vmCheckpointState->criuSetShellJobFunctionPointerType, "VZ")
		|| j9sl_lookup_name(vmCheckpointState->libCRIUHandle, (char*)"criu_set_log_level", (UDATA*)&vmCheckpointState->criuSetLogLevelFunctionPointerType, "VI")
		|| j9sl_lookup_name(vmCheckpointState->libCRIUHandle, (char*)"criu_set_log_file", (UDATA*)&vmCheckpointState->criuSetLogFileFunctionPointerType, "IP")
		|| j9sl_lookup_name(vmCheckpointState->libCRIUHandle, (char*)"criu_set_leave_running", (UDATA*)&vmCheckpointState->criuSetLeaveRunningFunctionPointerType, "VZ")
		|| j9sl_lookup_name(vmCheckpointState->libCRIUHandle, (char*)"criu_set_ext_unix_sk", (UDATA*)&vmCheckpointState->criuSetExtUnixSkFunctionPointerType, "VZ")
		|| j9sl_lookup_name(vmCheckpointState->libCRIUHandle, (char*)"criu_set_file_locks", (UDATA*)&vmCheckpointState->criuSetFileLocksFunctionPointerType, "VZ")
		|| j9sl_lookup_name(vmCheckpointState->libCRIUHandle, (char*)"criu_set_tcp_established", (UDATA*)&vmCheckpointState->criuSetTcpEstablishedFunctionPointerType, "VZ")
		|| j9sl_lookup_name(vmCheckpointState->libCRIUHandle, (char*)"criu_set_auto_dedup", (UDATA*)&vmCheckpointState->criuSetAutoDedupFunctionPointerType, "VZ")
		|| j9sl_lookup_name(vmCheckpointState->libCRIUHandle, (char*)"criu_set_track_mem", (UDATA*)&vmCheckpointState->criuSetTrackMemFunctionPointerType, "VZ")
		|| j9sl_lookup_name(vmCheckpointState->libCRIUHandle, (char*)"criu_set_work_dir_fd", (UDATA*)&vmCheckpointState->criuSetWorkDirFdFunctionPointerType, "VI")
		|| j9sl_lookup_name(vmCheckpointState->libCRIUHandle, (char*)"criu_init_opts", (UDATA*)&vmCheckpointState->criuInitOptsFunctionPointerType, "IV")
		|| j9sl_lookup_name(vmCheckpointState->libCRIUHandle, (char*)"criu_set_ghost_limit", (UDATA*)&vmCheckpointState->criuSetGhostFileLimitFunctionPointerType, "Vi")
		|| j9sl_lookup_name(vmCheckpointState->libCRIUHandle, (char*)"criu_dump", (UDATA*)&vmCheckpointState->criuDumpFunctionPointerType, "IV")
	) {
		*currentExceptionClass = criuSystemCheckpointExceptionClass;
		*systemReturnCode = 1;
		*nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_CRIU_LOADING_LIBCRIU_FUNCTIONS_FAILED, NULL);
		Trc_VM_criu_setupJNIFieldIDsAndCRIUAPI_not_find_criu_methods(currentThread, 1);
		returnCode = false;
		goto done;
	}

done:
	return returnCode;
}


#define J9_NATIVE_STRING_NO_ERROR 0
#define J9_NATIVE_STRING_OUT_OF_MEMORY (-1)
#define J9_NATIVE_STRING_FAIL_TO_CONVERT (-2)

#define J9_CRIU_UNPRIVILEGED_NO_ERROR 0
#define J9_CRIU_UNPRIVILEGED_DLSYM_ERROR (-1)
#define J9_CRIU_UNPRIVILEGED_DLSYM_NULL_SYMBOL (-2)

/**
 * Converts the given java string to native representation. The nativeString parameter should point
 * to a buffer with its size specified by nativeStringBufSize. If the java string length exceeds the buffer
 * size, nativeString will be set to allocated memory.
 *
 * @note If successful, the caller is responsible for freeing any memory allocated and stored in nativeString.
 *
 * @param[in] currentThread current thread
 * @param[in] javaString java string object
 * @param[out] nativeString the native representation of the java string
 * @param[in] nativeStringBufSize size of the nativeString buffer
 *
 * @return return code indicating success, allocation failure, or string conversion failure
 */
static IDATA
getNativeString(J9VMThread *currentThread, j9object_t javaString, char **nativeString, IDATA nativeStringBufSize)
{
	char mutf8StringBuf[STRING_BUFFER_SIZE];
	char *mutf8String = NULL;
	UDATA mutf8StringSize = 0;
	IDATA requiredConvertedStringSize = 0;
	char *localNativeString = *nativeString;
	IDATA res = J9_NATIVE_STRING_NO_ERROR;
	PORT_ACCESS_FROM_VMC(currentThread);
	OMRPORT_ACCESS_FROM_J9PORT(PORTLIB);

	mutf8String = copyStringToUTF8WithMemAlloc(currentThread, javaString, J9_STR_NULL_TERMINATE_RESULT, "", 0, mutf8StringBuf, STRING_BUFFER_SIZE, &mutf8StringSize);
	if (NULL == mutf8String) {
		res = J9_NATIVE_STRING_OUT_OF_MEMORY;
		goto free;
	}

	/* get the required size */
	requiredConvertedStringSize = omrstr_convert(J9STR_CODE_MUTF8, J9STR_CODE_PLATFORM_RAW,
					mutf8String,
					mutf8StringSize,
					localNativeString,
					0);

	if (requiredConvertedStringSize < 0) {
		Trc_VM_criu_getNativeString_getStringSizeFail(currentThread, mutf8String, mutf8StringSize);
		res = J9_NATIVE_STRING_FAIL_TO_CONVERT;
		goto free;
	}

	/* Add 1 for NUL terminator */
	requiredConvertedStringSize += 1;

	if (requiredConvertedStringSize > nativeStringBufSize) {
		localNativeString = (char*) j9mem_allocate_memory(requiredConvertedStringSize, OMRMEM_CATEGORY_VM);
		if (NULL == localNativeString) {
			res = J9_NATIVE_STRING_OUT_OF_MEMORY;
			goto free;
		}
	}

	localNativeString[requiredConvertedStringSize - 1] = '\0';

	/* convert the string */
	requiredConvertedStringSize = omrstr_convert(J9STR_CODE_MUTF8, J9STR_CODE_PLATFORM_RAW,
					mutf8String,
					mutf8StringSize,
					localNativeString,
					requiredConvertedStringSize);

	if (requiredConvertedStringSize < 0) {
		Trc_VM_criu_getNativeString_convertFail(currentThread, mutf8String, mutf8StringSize, requiredConvertedStringSize);
		res = J9_NATIVE_STRING_FAIL_TO_CONVERT;
		goto free;
	}

free:
	if (mutf8String != mutf8StringBuf) {
		j9mem_free_memory(mutf8String);
	}
	if (localNativeString != *nativeString) {
		if (J9_NATIVE_STRING_NO_ERROR == res) {
			*nativeString = localNativeString;
		} else {
			j9mem_free_memory(localNativeString);
			localNativeString = NULL;
		}
	}

	return res;
}

/**
 * Suspends or resumes Java threads for checkpoint and restore.
 * Some threads are marked with a flag (J9_PRIVATE_FLAGS2_DELAY_HALT_FOR_CHECKPOINT) to delay suspending them
 * until after all checkpoint hooks are run and to resume them before any restore hooks are run.
 * This function toggles suspend on either those threads marked with that flag or those without the flag, but not all threads.
 *
 * Current usages:
 * Step 1 - SUSPEND_THEADS_NO_DELAY_HALT_FLAG: suspend only those threads NOT marked with the delay halt flag
 * Step 2 - SUSPEND_THEADS_WITH_DELAY_HALT_FLAG: suspend only those threads marked with the delay halt flag
 *     previously suspended other threads stay the same
 * Step 3 - RESUME_THREADS_WITH_DELAY_HALT_FLAG: resume only those threads marked with the delay halt flag
 *     previously suspended other threads stay the same
 * Step 4 - RESUME_THREADS_NO_DELAY_HALT_FLAG: resume only those threads NOT marked with the delay halt flag
 *     previously resumed other threads stay the same
 *     now all previously suspended threads are resumed
 * This only affects threads that threadCanRunJavaCode() returns TRUE.
 * No impact on the current thread.
 *
 * Caller must first acquire exclusive safepoint or exclusive VMAccess.
 *
 * @param[in] currentThread vmThread token
 * @param[in] suspendResumeFlag four states as steps above
 */
static void
toggleSuspendOnJavaThreads(J9VMThread *currentThread, U_8 suspendResumeFlag)
{
	J9JavaVM *vm = currentThread->javaVM;

	Assert_VM_criu_true((J9_XACCESS_EXCLUSIVE == vm->exclusiveAccessState) || (J9_XACCESS_EXCLUSIVE == vm->safePointState));
	if (TrcEnabled_Trc_VM_criu_toggleSuspendOnJavaThreads_start) {
		char *currentThreadName = getOMRVMThreadName(currentThread->omrVMThread);
		Trc_VM_criu_toggleSuspendOnJavaThreads_start(currentThread, currentThreadName, suspendResumeFlag);
		releaseOMRVMThreadName(currentThread->omrVMThread);
	}
	J9VMThread *walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
	while (NULL != walkThread) {
		if (VM_VMHelpers::threadCanRunJavaCode(walkThread)
			&& (currentThread != walkThread)
		) {
			bool isDelayHaltFlagSet = J9_ARE_ANY_BITS_SET(walkThread->privateFlags2, J9_PRIVATE_FLAGS2_DELAY_HALT_FOR_CHECKPOINT);
			if ((!isDelayHaltFlagSet && (SUSPEND_THEADS_NO_DELAY_HALT_FLAG == suspendResumeFlag))
				|| (isDelayHaltFlagSet && (SUSPEND_THEADS_WITH_DELAY_HALT_FLAG == suspendResumeFlag))
			) {
				Trc_VM_criu_toggleSuspendOnJavaThreads_walkStatus(currentThread, "suspend", walkThread);
				setHaltFlag(walkThread, J9_PUBLIC_FLAGS_HALT_THREAD_FOR_CHECKPOINT);
			} else if ((isDelayHaltFlagSet && (RESUME_THREADS_WITH_DELAY_HALT_FLAG == suspendResumeFlag))
				|| (!isDelayHaltFlagSet && (RESUME_THREADS_NO_DELAY_HALT_FLAG == suspendResumeFlag))
			) {
				Trc_VM_criu_toggleSuspendOnJavaThreads_walkStatus(currentThread, "clearHaltFlag", walkThread);
				clearHaltFlag(walkThread, J9_PUBLIC_FLAGS_HALT_THREAD_FOR_CHECKPOINT);
			} else {
				Trc_VM_criu_toggleSuspendOnJavaThreads_walkStatus(currentThread, "skipped", walkThread);
			}
			if (TrcEnabled_Trc_VM_criu_toggleSuspendOnJavaThreads_walkThread) {
				char *walkThreadName = getOMRVMThreadName(walkThread->omrVMThread);
				Trc_VM_criu_toggleSuspendOnJavaThreads_walkThread(currentThread, walkThreadName, suspendResumeFlag, walkThread, currentThread);
				releaseOMRVMThreadName(walkThread->omrVMThread);
			}
		}
		walkThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, walkThread);
	}
}

static UDATA
notCheckpointSafeOrClinitFrameWalkFunction(J9VMThread *vmThread, J9StackWalkState *walkState)
{
	J9Method *method = walkState->method;
	UDATA returnCode = J9_STACKWALK_KEEP_ITERATING;

	if (NULL != method) {
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9ClassLoader *methodLoader = J9_CLASS_FROM_METHOD(method)->classLoader;
		J9UTF8 *romMethodName = J9ROMMETHOD_NAME(romMethod);
		/* only method names that start with '<' are <init> and <clinit>  */
		if (0 == strncmp((char*)J9UTF8_DATA(romMethodName), "<c", 2)) {
			*(UDATA*)walkState->userData1 = J9VM_DELAYCHECKPOINT_CLINIT;
			goto fail;
		}

		/* we only enforce this in methods loaded by the bootloader */
		if (methodLoader == vmThread->javaVM->systemClassLoader) {
			if (J9ROMMETHOD_HAS_EXTENDED_MODIFIERS(romMethod)) {
				U_32 extraModifiers = getExtendedModifiersDataFromROMMethod(romMethod);
				if (J9ROMMETHOD_HAS_NOT_CHECKPOINT_SAFE_ANNOTATION(extraModifiers)) {
					*(UDATA*)walkState->userData1 = J9VM_DELAYCHECKPOINT_NOTCHECKPOINTSAFE;
					goto fail;
				}
			}
		}
	}

done:
	return returnCode;

fail:
	walkState->userData2 = (void *)vmThread;
	walkState->userData3 = (void *)method;
	returnCode = J9_STACKWALK_STOP_ITERATING;
	goto done;
}

static bool
checkIfSafeToCheckpoint(J9VMThread *currentThread)
{
	UDATA notSafeToCheckpoint = 0;
	J9JavaVM *vm = currentThread->javaVM;

	Assert_VM_criu_true((J9_XACCESS_EXCLUSIVE == vm->exclusiveAccessState) || (J9_XACCESS_EXCLUSIVE == vm->safePointState));

	J9VMThread *walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
	while (NULL != walkThread) {
		if (VM_VMHelpers::threadCanRunJavaCode(walkThread)
		&& (currentThread != walkThread)
		) {
			J9StackWalkState walkState;
			walkState.walkThread = walkThread;
			walkState.flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_INCLUDE_NATIVES;
			walkState.skipCount = 0;
			walkState.userData1 = (void *)&notSafeToCheckpoint;
			walkState.frameWalkFunction = notCheckpointSafeOrClinitFrameWalkFunction;

			vm->walkStackFrames(walkThread, &walkState);
			if (0 != notSafeToCheckpoint) {
				Trc_VM_criu_checkpointJVMImpl_checkIfSafeToCheckpointBlockedVer(currentThread, walkState.userData2, walkState.userData3, *(UDATA*)walkState.userData1);
				break;
			}
		}
		walkThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, walkThread);
	}

	return notSafeToCheckpoint;
}

static VMINLINE void
acquireSafeOrExcusiveVMAccess(J9VMThread *currentThread, bool isSafe)
{
	if (isSafe) {
		acquireSafePointVMAccess(currentThread);
	} else {
		acquireExclusiveVMAccess(currentThread);
	}
}

static VMINLINE void
releaseSafeOrExcusiveVMAccess(J9VMThread *currentThread, bool isSafe)
{
	if (isSafe) {
		releaseSafePointVMAccess(currentThread);
	} else {
		releaseExclusiveVMAccess(currentThread);
	}
}

/**
 * Read buffer pointed to by cursor and add replace the characters sequences "\r\n"
 * and "\n" with a null terminator "\0". The input buffer must be NULL terminated.
 *
 * @param cursor pointer to buffer
 *
 * @return cursor position at the end of the line
 */
static char*
endLine(char *cursor)
{
	while(true) {
		switch (*cursor) {
		case '\r':
			cursor[0] = '\0';
			if (cursor[1] == '\n') {
				cursor++;
				cursor[0] = '\0';
			}
			goto done;
		case '\n':
			cursor[0] = '\0';
			goto done;
		case '\0':
			goto done;
		default:
			break;
		}
		cursor++;
	}
done:
	return cursor;
}

static UDATA
parseEnvVarsForOptions(J9VMThread *currentThread, J9JavaVMArgInfoList *vmArgumentsList, char *envFileChars, I_64 fileLength)
{
	UDATA returnCode = RESTORE_ARGS_RETURN_OK;
	char *cursor = envFileChars;
	char *optString = NULL;
	bool keepSearching = false;
	IDATA result = 0;

	do {
		keepSearching = false;

		optString = strstr(cursor, OPENJ9_RESTORE_OPTS_VAR);

		if (NULL != optString) {
			/* Check that variable name is not a substring of another variable */
			if (envFileChars != optString) {
				char preceedingChar = *(optString - 1);
				if ('\n' != preceedingChar) {
					keepSearching = true;
				}
			}
			optString += LITERAL_STRLEN(OPENJ9_RESTORE_OPTS_VAR);
			cursor = optString;
		}
	} while (keepSearching);

	if (NULL != optString) {
		PORT_ACCESS_FROM_VMC(currentThread);
		UDATA len = endLine(optString) - optString;
		/* +1 for the \0 */
		len++;
		char *restoreArgsChars = (char*) j9mem_allocate_memory(len, OMRMEM_CATEGORY_VM);

		if (NULL == restoreArgsChars) {
			returnCode = RESTORE_ARGS_RETURN_OOM;
			goto done;
		}

		memcpy(restoreArgsChars, optString, len);
		currentThread->javaVM->checkpointState.restoreArgsChars = restoreArgsChars;

		/* parseOptionsBuffer() will free buffer if no options were added, otherwise free at shutdown */
		result = parseOptionsBuffer(currentThread->javaVM->portLibrary, restoreArgsChars, vmArgumentsList, 0, FALSE);
		if (0 == result) {
			 /* no options found, buffer free'd */
			currentThread->javaVM->checkpointState.restoreArgsChars = NULL;
		} else if (result < 0) {
			j9mem_free_memory(restoreArgsChars);
			currentThread->javaVM->checkpointState.restoreArgsChars = NULL;
			returnCode = RESTORE_ARGS_RETURN_ENV_VAR_FILE_FAILED;
		}
	}
done:
	return returnCode;
}

static UDATA
loadRestoreArgsFromEnvVariable(J9VMThread *currentThread, J9JavaVMArgInfoList *vmArgumentsList, char *envFile)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	IDATA fileDescriptor = -1;
	UDATA returnCode = RESTORE_ARGS_RETURN_OK;
	char envFileBuf[ENV_FILE_BUFFER];
	char *envFileChars = envFileBuf;
	UDATA allocLength = 0;
	IDATA bytesRead = 0;
	IDATA readResult = 0;

	I_64 fileLength = j9file_length(envFile);
	/* Restrict file size to < 2G */
	if (fileLength > J9CONST64(0x7FFFFFFF)) {
		returnCode = RESTORE_ARGS_RETURN_ENV_VAR_FILE_FAILED;
		goto done;
	}

	if ((fileDescriptor = j9file_open(envFile, EsOpenRead, 0)) == -1) {
		returnCode = RESTORE_ARGS_RETURN_ENV_VAR_FILE_FAILED;
		goto done;
	}

	allocLength = fileLength + 1;
	if (allocLength > ENV_FILE_BUFFER) {
		envFileChars = (char*) j9mem_allocate_memory(allocLength, OMRMEM_CATEGORY_VM);
		if (NULL == envFileChars) {
			returnCode = RESTORE_ARGS_RETURN_OOM;
			goto close;
		}
	}

	/* read all data in a loop */
	while (bytesRead < fileLength) {
		readResult = j9file_read(fileDescriptor, envFileChars + bytesRead, (IDATA)(fileLength - bytesRead));
		if (-1 == readResult) {
			returnCode = RESTORE_ARGS_RETURN_ENV_VAR_FILE_FAILED;
			goto freeEnvFile;
		}
		bytesRead += readResult;
	}
	envFileChars[fileLength] = '\0';

	returnCode = parseEnvVarsForOptions(currentThread, vmArgumentsList, envFileChars, fileLength);

freeEnvFile:
	if (envFileBuf != envFileChars) {
		j9mem_free_memory(envFileChars);
	}
close:
	j9file_close(fileDescriptor);
done:
	return returnCode;
}

static void
traceRestoreArgs(J9VMThread *currentThread, J9VMInitArgs *restoreArgs)
{
	JavaVMInitArgs *args = restoreArgs->actualVMArgs;

	for (IDATA i = 0; i < args->nOptions; i++) {
		Trc_VM_criu_restoreArg(currentThread, args->options[i].optionString);
	}
}

static UDATA
loadRestoreArguments(J9VMThread *currentThread, const char *optionsFile, char *envFile)
{
	UDATA result = RESTORE_ARGS_RETURN_OK;
	J9JavaVM *vm = currentThread->javaVM;
	J9JavaVMArgInfoList vmArgumentsList;
	UDATA ignored = 0;
	J9VMInitArgs *previousArgs = vm->checkpointState.restoreArgsList;

	vmArgumentsList.pool = pool_new(sizeof(J9JavaVMArgInfo), 0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_CLASSES, POOL_FOR_PORT(vm->portLibrary));
	if (NULL == vmArgumentsList.pool) {
		vm->internalVMFunctions->setNativeOutOfMemoryError(currentThread, 0, 0);
		result = RESTORE_ARGS_RETURN_OOM;
		goto done;
	}
	vmArgumentsList.head = NULL;
	vmArgumentsList.tail = NULL;

	if (NULL != optionsFile) {
		/* addXOptionsFile() adds the options file name to the args list, if its null it adds a null character to the args list */
		if (0 != addXOptionsFile(vm->portLibrary, optionsFile, &vmArgumentsList, 0)) {
			result = RESTORE_ARGS_RETURN_OPTIONS_FILE_FAILED;
			goto done;
		}
	}

	if (NULL != envFile) {
		result = loadRestoreArgsFromEnvVariable(currentThread, &vmArgumentsList, envFile);
		if (RESTORE_ARGS_RETURN_OK != result) {
			goto done;
		}
	}

	vm->checkpointState.restoreArgsList = createJvmInitArgs(vm->portLibrary, vm->vmArgsArray->actualVMArgs, &vmArgumentsList, &ignored);
	vm->checkpointState.restoreArgsList->previousArgs = previousArgs;

	if (TrcEnabled_Trc_VM_criu_restoreArg) {
		traceRestoreArgs(currentThread, vm->checkpointState.restoreArgsList);
	}
done:
	pool_kill(vmArgumentsList.pool);
	return result;
}

static VMINLINE BOOLEAN
isTransitionToDebugInterpreterRequested(J9JavaVM *vm)
{
	return J9_ARE_ALL_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_TRANSITION_TO_DEBUG_INTERPRETER);
}

static void
transitionToDebugInterpreter(J9JavaVM *vm)
{
	if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm)) {
#if defined(OMR_GC_COMPRESSED_POINTERS)
		vm->bytecodeLoop = debugBytecodeLoopCompressed;
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
	} else {
#if defined(OMR_GC_FULL_POINTERS)
		vm->bytecodeLoop = debugBytecodeLoopFull;
#endif /* defined(OMR_GC_FULL_POINTERS) */
	}
	J9VMThread *walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
	while (NULL != walkThread) {
		VM_VMHelpers::requestInterpreterReentry(walkThread);
		walkThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, walkThread);
	}
}

static void
checkTransitionToDebugInterpreter(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;

	if (J9_ARE_ALL_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_IS_JDWP_ENABLED)) {
		vm->checkpointState.flags |= J9VM_CRIU_TRANSITION_TO_DEBUG_INTERPRETER;
	} else if (NULL != vm->checkpointState.restoreArgsList) {
		J9VMInitArgs *restoreArgsList = vm->checkpointState.restoreArgsList;
		IDATA debugOn = FIND_AND_CONSUME_ARG(restoreArgsList, EXACT_MATCH, VMOPT_XXDEBUGINTERPRETER, NULL);
		IDATA debugOff = FIND_AND_CONSUME_ARG(restoreArgsList, EXACT_MATCH, VMOPT_XXNODEBUGINTERPRETER, NULL);
		if (debugOn > debugOff) {
			vm->checkpointState.flags |= J9VM_CRIU_TRANSITION_TO_DEBUG_INTERPRETER;
		}
	}

	if (isTransitionToDebugInterpreterRequested(vm)) {
		transitionToDebugInterpreter(vm);
	}
}

void JNICALL
criuCheckpointJVMImpl(JNIEnv *env,
		jstring imagesDir,
		jboolean leaveRunning,
		jboolean shellJob,
		jboolean extUnixSupport,
		jint logLevel,
		jstring logFile,
		jboolean fileLocks,
		jstring workDir,
		jboolean tcpEstablished,
		jboolean autoDedup,
		jboolean trackMemory,
		jboolean unprivileged,
		jstring optionsFile,
		jstring environmentFile,
		jlong ghostFileLimit)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9PortLibrary *portLibrary = vm->portLibrary;

	jclass currentExceptionClass = NULL;
	char *exceptionMsg = NULL;
	const char *nlsMsgFormat = NULL;
	UDATA msgCharLength = 0;
	IDATA systemReturnCode = 0;
	const char *dlerrorReturnString = NULL;
	bool setupCRIU = true;
	PORT_ACCESS_FROM_VMC(currentThread);

	Trc_VM_criu_checkpointJVMImpl_Entry(currentThread);
	if (NULL == vm->checkpointState.criuJVMCheckpointExceptionClass) {
		setupCRIU = setupJNIFieldIDsAndCRIUAPI(env, &currentExceptionClass, &systemReturnCode, &nlsMsgFormat);
	}

	vm->checkpointState.checkpointThread = currentThread;

	if (isCheckpointAllowed(vm) && setupCRIU) {
#if defined(LINUX)
		j9object_t cpDir = NULL;
		j9object_t log = NULL;
		j9object_t wrkDir = NULL;
		j9object_t optFile = NULL;
		j9object_t envFile = NULL;
		IDATA dirFD = 0;
		IDATA workDirFD = 0;
		char directoryBuf[STRING_BUFFER_SIZE];
		char *directoryChars = directoryBuf;
		char logFileBuf[STRING_BUFFER_SIZE];
		char *logFileChars = logFileBuf;
		char workDirBuf[STRING_BUFFER_SIZE];
		char *workDirChars = workDirBuf;
		char optionsFileBuf[STRING_BUFFER_SIZE];
		char *optionsFileChars = optionsFileBuf;
		char envFileBuf[STRING_BUFFER_SIZE];
		char *envFileChars = envFileBuf;
		bool hasDumpSucceeded = false;
		I_64 checkpointNanoTimeMonotonic = 0;
		I_64 restoreNanoTimeMonotonic = 0;
		U_64 checkpointNanoUTCTime = 0;
		U_64 restoreNanoUTCTime = 0;
		UDATA success = 0;
		bool safePoint = J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_OSR_SAFE_POINT);
		UDATA maxRetries = vm->checkpointState.maxRetryForNotCheckpointSafe;
		UDATA sleepMilliseconds = vm->checkpointState.sleepMillisecondsForNotCheckpointSafe;
		bool syslogFlagNone = true;
		char *syslogOptions = NULL;
		I_32 syslogBufferSize = 0;
		UDATA oldVMState = VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_CHECKPOINT_PHASE_START);
		UDATA notSafeToCheckpoint = 0;
		U_32 intGhostFileLimit = 0;
		IDATA criuDumpReturnCode = 0;
		bool restoreFailure = false;

		internalEnterVMFromJNI(currentThread);

		Assert_VM_criu_notNull(imagesDir);
		cpDir = J9_JNI_UNWRAP_REFERENCE(imagesDir);
		systemReturnCode = getNativeString(currentThread, cpDir, &directoryChars, STRING_BUFFER_SIZE);
		switch (systemReturnCode) {
		case J9_NATIVE_STRING_NO_ERROR:
			break;
		case J9_NATIVE_STRING_OUT_OF_MEMORY:
			setNativeOutOfMemoryError(currentThread, 0, 0);
			goto freeDir;
		case J9_NATIVE_STRING_FAIL_TO_CONVERT:
			currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_CRIU_FAILED_TO_CONVERT_JAVA_STRING, NULL);
			goto freeDir;
		}

		if (NULL != logFile) {
			log = J9_JNI_UNWRAP_REFERENCE(logFile);
			systemReturnCode = getNativeString(currentThread, log, &logFileChars, STRING_BUFFER_SIZE);
			switch (systemReturnCode) {
			case J9_NATIVE_STRING_NO_ERROR:
				break;
			case J9_NATIVE_STRING_OUT_OF_MEMORY:
				setNativeOutOfMemoryError(currentThread, 0, 0);
				goto freeLog;
			case J9_NATIVE_STRING_FAIL_TO_CONVERT:
				currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_CRIU_FAILED_TO_CONVERT_JAVA_STRING, NULL);
				goto freeLog;
			}
		}

		if (NULL != optionsFile) {
			optFile = J9_JNI_UNWRAP_REFERENCE(optionsFile);
			systemReturnCode = getNativeString(currentThread, optFile, &optionsFileChars, STRING_BUFFER_SIZE);
			switch (systemReturnCode) {
			case J9_NATIVE_STRING_NO_ERROR:
				break;
			case J9_NATIVE_STRING_OUT_OF_MEMORY:
				setNativeOutOfMemoryError(currentThread, 0, 0);
				goto freeOptionsFile;
			case J9_NATIVE_STRING_FAIL_TO_CONVERT:
				currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_CRIU_FAILED_TO_CONVERT_JAVA_STRING, NULL);
				goto freeOptionsFile;
			}
		} else {
			optionsFileChars = NULL;
		}

		if (NULL != environmentFile) {
			envFile = J9_JNI_UNWRAP_REFERENCE(environmentFile);
			systemReturnCode = getNativeString(currentThread, envFile, &envFileChars, STRING_BUFFER_SIZE);
			switch (systemReturnCode) {
			case J9_NATIVE_STRING_NO_ERROR:
				break;
			case J9_NATIVE_STRING_OUT_OF_MEMORY:
				setNativeOutOfMemoryError(currentThread, 0, 0);
				goto freeEnvFile;
			case J9_NATIVE_STRING_FAIL_TO_CONVERT:
				currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_CRIU_FAILED_TO_CONVERT_JAVA_STRING, NULL);
				goto freeEnvFile;
			}
		} else {
			envFileChars = NULL;
		}

		if (NULL != workDir) {
			wrkDir = J9_JNI_UNWRAP_REFERENCE(workDir);
			systemReturnCode = getNativeString(currentThread, wrkDir, &workDirChars, STRING_BUFFER_SIZE);
			switch (systemReturnCode) {
			case J9_NATIVE_STRING_NO_ERROR:
				break;
			case J9_NATIVE_STRING_OUT_OF_MEMORY:
				setNativeOutOfMemoryError(currentThread, 0, 0);
				goto freeWorkDir;
			case J9_NATIVE_STRING_FAIL_TO_CONVERT:
				currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_CRIU_FAILED_TO_CONVERT_JAVA_STRING, NULL);
				goto freeWorkDir;
			}
		}

		dirFD = open(directoryChars, O_DIRECTORY);
		if (dirFD < 0) {
			systemReturnCode = errno;
			currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_CRIU_FAILED_TO_OPEN_DIR, NULL);
			goto freeWorkDir;
		}

		if (NULL != workDir) {
			workDirFD = open(workDirChars, O_DIRECTORY);
			if (workDirFD < 0) {
				systemReturnCode = errno;
				currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_CRIU_FAILED_TO_OPEN_WORK_DIR, NULL);
				goto closeDirFD;
			}
		}

		systemReturnCode = vm->checkpointState.criuInitOptsFunctionPointerType();
		if (0 != systemReturnCode) {
			currentExceptionClass = vm->checkpointState.criuSystemCheckpointExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_CRIU_INIT_FAILED, NULL);
			goto closeWorkDirFD;
		}

		if (JNI_TRUE == unprivileged) {
			if (NULL != vm->checkpointState.criuSetUnprivilegedFunctionPointerType) {
				systemReturnCode = J9_CRIU_UNPRIVILEGED_NO_ERROR;
				vm->checkpointState.criuSetUnprivilegedFunctionPointerType(JNI_FALSE != unprivileged);
			} else {
				currentExceptionClass = vm->checkpointState.criuSystemCheckpointExceptionClass;
				systemReturnCode = (NULL != dlerrorReturnString) ? J9_CRIU_UNPRIVILEGED_DLSYM_ERROR : J9_CRIU_UNPRIVILEGED_DLSYM_NULL_SYMBOL;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_CRIU_CANNOT_SET_UNPRIVILEGED, NULL);
				goto closeWorkDirFD;
			}
		}

		vm->checkpointState.criuSetImagesDirFdFunctionPointerType(dirFD);
		vm->checkpointState.criuSetShellJobFunctionPointerType(JNI_FALSE != shellJob);
		if (logLevel > 0) {
			vm->checkpointState.criuSetLogLevelFunctionPointerType((int)logLevel);
		}
		if (NULL != logFile) {
			vm->checkpointState.criuSetLogFileFunctionPointerType(logFileChars);
		}
		vm->checkpointState.criuSetLeaveRunningFunctionPointerType(JNI_FALSE != leaveRunning);
		vm->checkpointState.criuSetExtUnixSkFunctionPointerType(JNI_FALSE != extUnixSupport);
		vm->checkpointState.criuSetFileLocksFunctionPointerType(JNI_FALSE != fileLocks);
		vm->checkpointState.criuSetTcpEstablishedFunctionPointerType(JNI_FALSE != tcpEstablished);
		vm->checkpointState.criuSetAutoDedupFunctionPointerType(JNI_FALSE != autoDedup);
		vm->checkpointState.criuSetTrackMemFunctionPointerType(JNI_FALSE != trackMemory);

		if (-1 != ghostFileLimit) {
			intGhostFileLimit = (U_32)(U_64)ghostFileLimit;
			if (0 != intGhostFileLimit) {
				vm->internalVMFunctions->setRequiredGhostFileLimit(currentThread, intGhostFileLimit);
			}
		}

		if (NULL != workDir) {
			vm->checkpointState.criuSetWorkDirFdFunctionPointerType(workDirFD);
		}

		acquireSafeOrExcusiveVMAccess(currentThread, safePoint);

		notSafeToCheckpoint = checkIfSafeToCheckpoint(currentThread);

		for (UDATA i = 0; (0 != notSafeToCheckpoint) && (i <= maxRetries); i++) {
			releaseSafeOrExcusiveVMAccess(currentThread, safePoint);
			internalExitVMToJNI(currentThread);
			omrthread_sleep(sleepMilliseconds);
			internalEnterVMFromJNI(currentThread);
			acquireSafeOrExcusiveVMAccess(currentThread, safePoint);
			notSafeToCheckpoint = checkIfSafeToCheckpoint(currentThread);
		}

		if ((J9VM_DELAYCHECKPOINT_NOTCHECKPOINTSAFE == notSafeToCheckpoint)
			|| ((J9VM_DELAYCHECKPOINT_CLINIT == notSafeToCheckpoint) && J9_ARE_ALL_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_IS_THROW_ON_DELAYED_CHECKPOINT_ENABLED))
		) {
			releaseSafeOrExcusiveVMAccess(currentThread, safePoint);
			currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
			systemReturnCode = vm->checkpointState.maxRetryForNotCheckpointSafe;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_CRIU_MAX_RETRY_FOR_NOTCHECKPOINTSAFE_REACHED, NULL);
			omr_error_t rc = vm->j9rasDumpFunctions->triggerOneOffDump(vm, (char*)"java", (char*)"openj9.internal.criu.CRIUSupport.checkpointJVMImpl", NULL, 0);
			Trc_VM_criu_checkpointJVMImpl_triggerOneOffJavaDump(currentThread, rc);
			goto closeWorkDirFD;
		} else {
			Trc_VM_criu_checkpointJVMImpl_checkpointWithActiveCLinit(currentThread);
		}

		toggleSuspendOnJavaThreads(currentThread, SUSPEND_THEADS_NO_DELAY_HALT_FLAG);

		vm->extendedRuntimeFlags2 |= J9_EXTENDED_RUNTIME2_CRIU_SINGLE_THREAD_MODE|J9_EXTENDED_RUNTIME2_CRIU_SINGLE_THROW_BLOCKING_EXCEPTIONS;
		releaseSafeOrExcusiveVMAccess(currentThread, safePoint);

		VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_CHECKPOINT_PHASE_JAVA_HOOKS);

		if (FALSE == jvmCheckpointHooks(currentThread)) {
			/* throw the pending exception */
			goto wakeJavaThreads;
		}

		/* At this point, Java threads are all finished, and JVM is considered paused before taking the checkpoint. */
		vm->checkpointState.checkpointRestoreTimeDelta = 0;
		portLibrary->nanoTimeMonotonicClockDelta = 0;
		checkpointNanoTimeMonotonic = j9time_nano_time();
		checkpointNanoUTCTime = j9time_current_time_nanos(&success);
		if (0 == success) {
			systemReturnCode = errno;
			currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_CRIU_J9_CURRENT_TIME_NANOS_FAILURE, NULL);
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}
		Trc_VM_criu_checkpoint_nano_times(currentThread, checkpointNanoTimeMonotonic, checkpointNanoUTCTime);
		TRIGGER_J9HOOK_VM_PREPARING_FOR_CHECKPOINT(vm->hookInterface, currentThread);

		/* GC releases threads in a multi-thread fashion. Threads will need to remove
		 * themselves from the threadgroup which requires a lock. Disable deadlock detection
		 * temporarily while this happens.
		 */
		vm->extendedRuntimeFlags2 &= ~J9_EXTENDED_RUNTIME2_CRIU_SINGLE_THROW_BLOCKING_EXCEPTIONS;
		vm->memoryManagerFunctions->j9gc_prepare_for_checkpoint(currentThread);
		vm->extendedRuntimeFlags2 |= J9_EXTENDED_RUNTIME2_CRIU_SINGLE_THROW_BLOCKING_EXCEPTIONS;

		acquireSafeOrExcusiveVMAccess(currentThread, safePoint);

		VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_CHECKPOINT_PHASE_INTERNAL_HOOKS);

		/* Run internal checkpoint hooks, after iterating heap objects */
		if (FALSE == runInternalJVMCheckpointHooks(currentThread, &nlsMsgFormat)) {
			currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}

		if (J9_ARE_ANY_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_IS_JDWP_ENABLED)) {
			/* Suspending the threads marked with J9_PRIVATE_FLAGS2_DELAY_HALT_FOR_CHECKPOINT
			 * is only required for JDWP threads.
			 */
			toggleSuspendOnJavaThreads(currentThread, SUSPEND_THEADS_WITH_DELAY_HALT_FLAG);
		}

		syslogOptions = (char *)j9mem_allocate_memory(STRING_BUFFER_SIZE, J9MEM_CATEGORY_VM);
		if (NULL == syslogOptions) {
			systemReturnCode = J9_NATIVE_STRING_OUT_OF_MEMORY;
			setNativeOutOfMemoryError(currentThread, 0, 0);
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}
		systemReturnCode = queryLogOptions(vm, STRING_BUFFER_SIZE, syslogOptions, &syslogBufferSize);
		if (JVMTI_ERROR_NONE != systemReturnCode) {
			currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JVMTI_COM_IBM_LOG_QUERY_OPT_ERROR, NULL);
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}
		Trc_VM_criu_checkpointJVMImpl_syslogOptions(currentThread, syslogOptions);
		if (0 != strcmp(syslogOptions, "none")) {
			/* Not -Xsyslog:none, close the system logger handle before checkpoint. */
			j9port_control(OMRPORT_CTLDATA_SYSLOG_CLOSE, 0);
			syslogFlagNone = false;
		}

		TRIGGER_J9HOOK_VM_CRIU_CHECKPOINT(vm->hookInterface, currentThread);

		malloc_trim(0);
		Trc_VM_criu_before_checkpoint(currentThread, j9time_nano_time(), j9time_current_time_nanos(&success));

		VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_CHECKPOINT_PHASE_END);

		/* Pre-checkpoint */
		criuDumpReturnCode = vm->checkpointState.criuDumpFunctionPointerType();

		/* The dump succeeded if criuDumpReturnCode is >= 0 */
		if (criuDumpReturnCode >= 0) {
			/* Set this if the dump succeeded, it means we are in the restore process.
			 * If it doesnt succeed we still need to run some of the restore code as
			 * some threads are waiting to be notified.
			 */
			hasDumpSucceeded = true;
		}

		if (hasDumpSucceeded) {
			VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_RESTORE_PHASE_START);
		} else {
			VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_CHECKPOINT_FAILED_PHASE_START);
		}

		/* Need to get restore time for trace point. */
		restoreNanoTimeMonotonic = j9time_nano_time();
		restoreNanoUTCTime = j9time_current_time_nanos(&success);
		if (0 == success) {
			systemReturnCode = errno;
			currentExceptionClass = vm->checkpointState.criuSystemRestoreExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(
				J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
				J9NLS_VM_CRIU_J9_CURRENT_TIME_NANOS_FAILURE,
				NULL);
			restoreFailure = true;
		}

		/* Set total restore time for InternalCRIUSupport API. */
		if (hasDumpSucceeded) {
			vm->checkpointState.lastRestoreTimeInNanoseconds = (I_64)restoreNanoUTCTime;
		}

		Trc_VM_criu_after_dump(currentThread, restoreNanoTimeMonotonic, vm->checkpointState.lastRestoreTimeInNanoseconds);

		if (!syslogFlagNone) {
			/* Re-open the system logger, and set options with saved string value. */
			j9port_control(J9PORT_CTLDATA_SYSLOG_OPEN, 0);
			setLogOptions(vm, syslogOptions);
		}

		if (hasDumpSucceeded) {
			/* Calculate restore time for CRaC MXBean API. */
			if (J9_ARE_ALL_BITS_SET(vm->checkpointState.flags, J9VM_CRAC_IS_CHECKPOINT_ENABLED)) {
				UDATA cracRestorePid = j9sysinfo_get_ppid();
				systemReturnCode = j9sysinfo_get_process_start_time(cracRestorePid, &restoreNanoUTCTime);
				if (0 != systemReturnCode) {
					currentExceptionClass = vm->checkpointState.criuSystemRestoreExceptionClass;
					nlsMsgFormat = j9nls_lookup_message(
						J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
						J9NLS_VM_CRIU_J9_GET_PROCESS_START_TIME_FAILURE,
						NULL);
					restoreFailure = true;
				}
				vm->checkpointState.processRestoreStartTimeInNanoseconds = (I_64)restoreNanoUTCTime;
				Trc_VM_criu_process_restore_start_after_dump(currentThread, cracRestorePid, vm->checkpointState.processRestoreStartTimeInNanoseconds);
			}

			/* Load restore arguments from restore file or env vars. */
			switch (loadRestoreArguments(currentThread, optionsFileChars, envFileChars)) {
			case RESTORE_ARGS_RETURN_OPTIONS_FILE_FAILED:
				currentExceptionClass = vm->checkpointState.criuJVMRestoreExceptionClass;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
					J9NLS_VM_CRIU_LOADING_OPTIONS_FILE_FAILED, NULL);
					/* fallthrough */
			case RESTORE_ARGS_RETURN_OOM:
				/* exception is already set */
				restoreFailure = true;
				break;
			case RESTORE_ARGS_RETURN_OK:
				break;
			}
		}

		if (hasDumpSucceeded) {
			VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_RESTORE_PHASE_INTERNAL_HOOKS);
		} else {
			VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_CHECKPOINT_FAILED_PHASE_INTERNAL_HOOKS);
		}

		/* Run internal restore hooks, and cleanup. Must be run even if we fail restore to re-enable system threads. */
		if (FALSE == runInternalJVMRestoreHooks(currentThread, &nlsMsgFormat)) {
			currentExceptionClass = vm->checkpointState.criuJVMRestoreExceptionClass;
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}
		if (hasDumpSucceeded) {
			checkTransitionToDebugInterpreter(currentThread);
		}
		if (J9_ARE_ANY_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_IS_JDWP_ENABLED)) {
			/* Resuming the threads marked with J9_PRIVATE_FLAGS2_DELAY_HALT_FOR_CHECKPOINT
			 * is only required for JDWP threads.
			 */
			toggleSuspendOnJavaThreads(currentThread, RESUME_THREADS_WITH_DELAY_HALT_FLAG);
		}
		releaseSafeOrExcusiveVMAccess(currentThread, safePoint);
		TRIGGER_J9HOOK_VM_CRIU_RESTORE(vm->hookInterface, currentThread);

		if (FALSE == vm->memoryManagerFunctions->j9gc_reinitialize_for_restore(currentThread, &nlsMsgFormat)) {
			currentExceptionClass = vm->checkpointState.criuJVMRestoreExceptionClass;
			goto wakeJavaThreads;
		}

		restoreNanoTimeMonotonic = j9time_nano_time();
		restoreNanoUTCTime = j9time_current_time_nanos(&success);
		if (0 == success) {
			systemReturnCode = errno;
			currentExceptionClass = vm->checkpointState.criuJVMRestoreExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_CRIU_J9_CURRENT_TIME_NANOS_FAILURE, NULL);
			goto wakeJavaThreads;
		}
		Trc_VM_criu_after_checkpoint(currentThread, restoreNanoTimeMonotonic, restoreNanoUTCTime);

		/* JVM downtime between checkpoint and restore is calculated with j9time_current_time_nanos()
		 * which is expected to be accurate in scenarios such as host rebooting, CRIU image moving across timezones.
		 */
		vm->checkpointState.checkpointRestoreTimeDelta = (I_64)(restoreNanoUTCTime - checkpointNanoUTCTime);
		if (vm->checkpointState.checkpointRestoreTimeDelta < 0) {
			/* A negative value was calculated for checkpointRestoreTimeDelta,
			 * Trc_VM_criu_checkpoint_nano_times & Trc_VM_criu_restore_nano_times can be used for further investigation.
			 * Currently OpenJ9 CRIU only supports 64-bit systems, and IDATA is equivalent to int64_t here.
			 */
			systemReturnCode = (IDATA)vm->checkpointState.checkpointRestoreTimeDelta;
			currentExceptionClass = vm->checkpointState.criuJVMRestoreExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
					J9NLS_VM_CRIU_NEGATIVE_CHECKPOINT_RESTORE_TIME_DELTA, NULL);
			restoreFailure = true;
		}

		/* j9time_nano_time() might be based on a different starting point in scenarios
		 * such as host rebooting, CRIU image moving across timezones.
		 * The adjustment calculated below is expected to be same as checkpointRestoreTimeDelta
		 * if there is no change for j9time_nano_time() start point.
		 * This value might be negative.
		 */
		portLibrary->nanoTimeMonotonicClockDelta = restoreNanoTimeMonotonic - checkpointNanoTimeMonotonic;
		Trc_VM_criu_restore_nano_times(currentThread, restoreNanoUTCTime, checkpointNanoUTCTime, vm->checkpointState.checkpointRestoreTimeDelta,
				restoreNanoTimeMonotonic, checkpointNanoTimeMonotonic, portLibrary->nanoTimeMonotonicClockDelta);

		if (hasDumpSucceeded) {
			VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_RESTORE_PHASE_JAVA_HOOKS);
		} else {
			VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_CHECKPOINT_FAILED_PHASE_JAVA_HOOKS);
		}

		if (restoreFailure) {
			goto wakeJavaThreads;
		}

		/* Run Java restore hooks. */
		if (hasDumpSucceeded && (FALSE == jvmRestoreHooks(currentThread))) {
			/* throw the pending exception */
			goto wakeJavaThreads;
		}

		if (hasDumpSucceeded && (NULL != vm->checkpointState.restoreArgsList)) {
			J9VMInitArgs *restoreArgsList = vm->checkpointState.restoreArgsList;
			/* mark -Xoptionsfile= as consumed */
			FIND_AND_CONSUME_ARG(restoreArgsList, STARTSWITH_MATCH, VMOPT_XOPTIONSFILE_EQUALS, NULL);
			IDATA ignoreEnabled = FIND_AND_CONSUME_ARG(restoreArgsList, EXACT_MATCH, VMOPT_XXIGNOREUNRECOGNIZEDRESTOREOPTIONSENABLE, NULL);
			IDATA ignoreDisabled = FIND_AND_CONSUME_ARG(restoreArgsList, EXACT_MATCH, VMOPT_XXIGNOREUNRECOGNIZEDRESTOREOPTIONSDISABLE, NULL);

			bool dontIgnoreUnsupportedRestoreOptions = (ignoreEnabled < 0) || (ignoreEnabled < ignoreDisabled);

			if ((FALSE == checkArgsConsumed(vm, vm->portLibrary, restoreArgsList)) && dontIgnoreUnsupportedRestoreOptions) {
				currentExceptionClass = vm->checkpointState.criuJVMRestoreExceptionClass;
				systemReturnCode = 0;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
					J9NLS_VM_CRIU_FAILED_TO_ENABLE_ALL_RESTORE_OPTIONS, NULL);
			}
		}

wakeJavaThreads:
		/* Needs to be run unconditionally as there may have been locking events in the checkpoint side. */
		if (FALSE == runDelayedLockRelatedOperations(currentThread)) {
			currentExceptionClass = vm->checkpointState.criuJVMRestoreExceptionClass;
			systemReturnCode = 0;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
					J9NLS_VM_CRIU_FAILED_DELAY_LOCK_RELATED_OPS, NULL);
		}

		acquireSafeOrExcusiveVMAccess(currentThread, safePoint);

wakeJavaThreadsWithExclusiveVMAccess:

		vm->extendedRuntimeFlags2 &= ~(J9_EXTENDED_RUNTIME2_CRIU_SINGLE_THROW_BLOCKING_EXCEPTIONS|J9_EXTENDED_RUNTIME2_CRIU_SINGLE_THREAD_MODE);
		toggleSuspendOnJavaThreads(currentThread, RESUME_THREADS_NO_DELAY_HALT_FLAG);

		releaseSafeOrExcusiveVMAccess(currentThread, safePoint);

		if (hasDumpSucceeded) {
			VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_RESTORE_PHASE_END);
		} else {
			VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_CHECKPOINT_FAILED_PHASE_END);
		}

closeWorkDirFD:
		if ((0 != close(workDirFD)) && (NULL == currentExceptionClass)) {
			systemReturnCode = errno;
			if (hasDumpSucceeded) {
				currentExceptionClass = vm->checkpointState.criuSystemRestoreExceptionClass;
			} else {
				currentExceptionClass = vm->checkpointState.criuSystemCheckpointExceptionClass;
			}
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_CRIU_FAILED_TO_CLOSE_WORK_DIR, NULL);
		}
closeDirFD:
		if ((0 != close(dirFD)) && (NULL == currentExceptionClass)) {
			systemReturnCode = errno;
			if (hasDumpSucceeded) {
				currentExceptionClass = vm->checkpointState.criuSystemRestoreExceptionClass;
			} else {
				currentExceptionClass = vm->checkpointState.criuSystemCheckpointExceptionClass;
			}
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_CRIU_FAILED_TO_CLOSE_DIR, NULL);
		}
		if (criuDumpReturnCode < 0) {
			/* CRIU dump failure overrides all others */
			systemReturnCode = criuDumpReturnCode;
			currentExceptionClass = vm->checkpointState.criuSystemCheckpointExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_CRIU_DUMP_FAILED, NULL);
		}
freeWorkDir:
		if (workDirBuf != workDirChars) {
			j9mem_free_memory(workDirChars);
		}
freeEnvFile:
		if (envFileBuf != envFileChars) {
			j9mem_free_memory(envFileChars);
		}
freeOptionsFile:
		if (optionsFileBuf != optionsFileChars) {
			j9mem_free_memory(optionsFileChars);
		}
freeLog:
		if (logFileBuf != logFileChars) {
			j9mem_free_memory(logFileChars);
		}
freeDir:
		if (directoryBuf != directoryChars) {
			j9mem_free_memory(directoryChars);
		}

		j9mem_free_memory(syslogOptions);

		VM_VMHelpers::setVMState(currentThread, oldVMState);
		internalExitVMToJNI(currentThread);
#endif /* defined(LINUX) */
	}

	/*
	 * Pending exceptions will be set by the JVM hooks, these exception will take precedence.
	 */
	if ((NULL != currentExceptionClass) && (NULL == currentThread->currentException)) {
		msgCharLength = j9str_printf(PORTLIB, NULL, 0, nlsMsgFormat, systemReturnCode);
		exceptionMsg = (char*) j9mem_allocate_memory(msgCharLength, J9MEM_CATEGORY_VM);

		j9str_printf(PORTLIB, exceptionMsg, msgCharLength, nlsMsgFormat, systemReturnCode);

		jmethodID init = NULL;
		if (vm->checkpointState.criuJVMCheckpointExceptionClass == currentExceptionClass) {
			init = vm->checkpointState.criuJVMCheckpointExceptionInit;
		} else if (vm->checkpointState.criuSystemCheckpointExceptionClass == currentExceptionClass) {
			init = vm->checkpointState.criuSystemCheckpointExceptionInit;
		} else if (vm->checkpointState.criuSystemRestoreExceptionClass == currentExceptionClass) {
			init = vm->checkpointState.criuSystemRestoreExceptionInit;
		} else {
			init = vm->checkpointState.criuJVMRestoreExceptionInit;
		}
		jstring jExceptionMsg = env->NewStringUTF(exceptionMsg);

		if (JNI_FALSE == env->ExceptionCheck()) {
			jobject exception = env->NewObject(currentExceptionClass, init, jExceptionMsg, (jint)systemReturnCode);
			if (NULL != exception) {
				env->Throw((jthrowable)exception);
			}
		}

		if (NULL != exceptionMsg) {
			j9mem_free_memory(exceptionMsg);
		}
	}

	vm->checkpointState.checkpointThread = NULL;

	Trc_VM_criu_checkpointJVMImpl_Exit(currentThread);
}

} /* extern "C" */
