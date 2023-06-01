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
#include <string.h>

#include "objhelp.h"
#include "j9.h"
#include "j9comp.h"
#include "j9protos.h"
#include "jvminit.h"
#include "ut_j9vm.h"
#include "vm_api.h"
#include "vmargs_core_api.h"

#include "CRIUHelpers.hpp"
#include "HeapIteratorAPI.h"
#include "ObjectAccessBarrierAPI.hpp"
#include "VMHelpers.hpp"

extern "C" {

J9_DECLARE_CONSTANT_UTF8(runPostRestoreHooks_sig, "()V");
J9_DECLARE_CONSTANT_UTF8(runPostRestoreHooks_name, "runPostRestoreHooks");
J9_DECLARE_CONSTANT_UTF8(runPreCheckpointHooks_sig, "()V");
J9_DECLARE_CONSTANT_UTF8(runPreCheckpointHooks_name, "runPreCheckpointHooks");
J9_DECLARE_CONSTANT_UTF8(j9InternalCheckpointHookAPI_name, "org/eclipse/openj9/criu/J9InternalCheckpointHookAPI");

static void addInternalJVMCheckpointHook(J9VMThread *currentThread, BOOLEAN isRestore, J9Class *instanceType, BOOLEAN includeSubClass, hookFunc hookFunc);
static void cleanupCriuHooks(J9VMThread *currentThread);
static BOOLEAN fillinHookRecords(J9VMThread *currentThread, j9object_t object);
static IDATA findinstanceFieldOffsetHelper(J9VMThread *currentThread, J9Class *instanceType, const char *fieldName, const char *fieldSig);
static void initializeCriuHooks(J9VMThread *currentThread);
static BOOLEAN juRandomReseed(J9VMThread *currentThread, void *userData, const char **nlsMsgFormat);
static BOOLEAN criuRestoreInitializeTrace(J9VMThread *currentThread, void *userData, const char **nlsMsgFormat);
static BOOLEAN criuRestoreInitializeXrs(J9VMThread *currentThread, void *userData, const char **nlsMsgFormat);
static BOOLEAN criuRestoreDisableSharedClassCache(J9VMThread *currentThread, void *userData, const char **nlsMsgFormat);
static BOOLEAN criuRestoreInitializeDump(J9VMThread *currentThread, void *userData, const char **nlsMsgFormat);
static jvmtiIterationControl objectIteratorCallback(J9JavaVM *vm, J9MM_IterateObjectDescriptor *objectDesc, void *userData);

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

	Assert_VM_true(vm->checkpointState.isCheckPointEnabled);

	if (vm->checkpointState.isNonPortableRestoreMode) {
		PORT_ACCESS_FROM_JAVAVM(vm);
		vm->checkpointState.isCheckPointAllowed = FALSE;
		vm->portLibrary->isCheckPointAllowed = FALSE;
		j9port_control(J9PORT_CTLDATA_CRIU_SUPPORT_FLAGS, OMRPORT_CRIU_SUPPORT_ENABLED | J9OMRPORT_CRIU_SUPPORT_FINAL_RESTORE);
	}

	TRIGGER_J9HOOK_VM_PREPARING_FOR_RESTORE(vm->hookInterface, currentThread);

	/* make sure Java hooks are the last thing run before restore */
	runStaticMethod(currentThread, J9UTF8_DATA(&j9InternalCheckpointHookAPI_name), &nas, 0, NULL);

	if (VM_VMHelpers::exceptionPending(currentThread)) {
		result = FALSE;
	}

	return result;
}

BOOLEAN
isCRIUSupportEnabled(J9VMThread *currentThread)
{
	return isCRIUSupportEnabled_VM(currentThread->javaVM);
}

BOOLEAN
isCRIUSupportEnabled_VM(J9JavaVM *vm)
{
	return vm->checkpointState.isCheckPointEnabled;
}

BOOLEAN
isCheckpointAllowed(J9VMThread *currentThread)
{
	BOOLEAN result = FALSE;

	if (isCRIUSupportEnabled(currentThread)) {
		result = currentThread->javaVM->checkpointState.isCheckPointAllowed;
	}

	return result;
}

BOOLEAN
isNonPortableRestoreMode(J9VMThread *currentThread)
{
	return currentThread->javaVM->checkpointState.isNonPortableRestoreMode;
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

static IDATA
findinstanceFieldOffsetHelper(J9VMThread *currentThread, J9Class *instanceType, const char *fieldName, const char *fieldSig)
{
	IDATA offset = (UDATA)instanceFieldOffset(
		currentThread, instanceType,
		(U_8*)fieldName, strlen(fieldName),
		(U_8*)fieldSig, strlen(fieldSig),
		NULL, NULL, 0);

	if (-1 != offset) {
		offset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
	}

	return offset;
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
	IDATA seedOffset = findinstanceFieldOffsetHelper(currentThread, hookRecord->instanceType, "seed", "Ljava/util/concurrent/atomic/AtomicLong;");
	if (-1 != seedOffset) {
#define JUCA_ATOMICLONG "java/util/concurrent/atomic/AtomicLong"
		J9Class *jucaAtomicLongClass = hashClassTableAt(vm->systemClassLoader, (U_8 *)JUCA_ATOMICLONG, LITERAL_STRLEN(JUCA_ATOMICLONG));
#undef JUCA_ATOMICLONG
		if (NULL != jucaAtomicLongClass) {
			IDATA valueOffset = findinstanceFieldOffsetHelper(currentThread, jucaAtomicLongClass, "value", "J");
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

		if (vm->checkpointState.isNonPortableRestoreMode) {
			/* No more checkpoint, cleanup hook records. */
			pool_kill(vm->checkpointState.hookRecords);
			vm->checkpointState.hookRecords = NULL;
		}
	}

	J9Pool *classIterationRestoreHookRecords = vm->checkpointState.classIterationRestoreHookRecords;
	if ((NULL != classIterationRestoreHookRecords) && (vm->checkpointState.isNonPortableRestoreMode)) {
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

	{
		/* Add restore hook to re-seed java.uti.Random.seed.value */
#define JAVA_UTIL_RANDOM "java/util/Random"
		J9Class *juRandomClass = peekClassHashTable(currentThread, vm->systemClassLoader, (U_8 *)JAVA_UTIL_RANDOM, LITERAL_STRLEN(JAVA_UTIL_RANDOM));
#undef JAVA_UTIL_RANDOM
		if (NULL != juRandomClass) {
			addInternalJVMCheckpointHook(currentThread, TRUE, juRandomClass, FALSE, juRandomReseed);
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
	while (NULL != hookRecord) {
		J9Class *clazz = J9OBJECT_CLAZZ_VM(vm, object);
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
			J9Class *clazz = vm->internalVMFunctions->allClassesStartDo(&j9ClassWalkState, vm, NULL);
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

	/* Cleanup at restore. */
	cleanupCriuHooks(currentThread);
	Trc_VM_criu_runRestoreHooks_Exit(currentThread);

	return result;
}

BOOLEAN
runDelayedLockRelatedOperations(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions* vmFuncs = vm->internalVMFunctions;
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
		vmFuncs->j9jni_deleteGlobalRef((JNIEnv*) currentThread, delayedLockingOperation->globalObjectRef, JNI_FALSE);
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
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
		PORT_ACCESS_FROM_JAVAVM(vm);

		UDATA count = 0;
		JavaVMOption *restorArgOptions = restorArgs->options;

		for (IDATA i = 0; i < restorArgs->nOptions; ++i) {
			char *optionString = restorArgOptions[i].optionString;

			if (strncmp("-D", optionString, 2) == 0) {
				count++;
			}
		}

		vmFuncs->internalEnterVMFromJNI(currentThread);

		UDATA arrayEntries = count * 2;
		j9object_t newArray = mmfns->J9AllocateIndexableObject(currentThread, ((J9Class*) J9VMJAVALANGSTRING_OR_NULL(vm))->arrayClass, arrayEntries, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);

		if (NULL == newArray) {
			vmFuncs->setHeapOutOfMemoryError(currentThread);
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
					vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
					goto done;
				}
				propValueCopy = (char *)getMUtf8String(vm, propValue, valueLength);
				if (NULL == propValueCopy) {
					j9mem_free_memory(propNameCopy);
					vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
					goto done;
				}
				PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, (j9object_t)newArray);
				j9object_t newObject = mmfns->j9gc_createJavaLangString(currentThread, (U_8 *)propNameCopy, propNameLen, J9_STR_TENURE);
				if (NULL == newObject) {
					j9mem_free_memory(propNameCopy);
					j9mem_free_memory(propValueCopy);
					vmFuncs->setHeapOutOfMemoryError(currentThread);
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
					vmFuncs->setHeapOutOfMemoryError(currentThread);
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

		returnProperties = vmFuncs->j9jni_createLocalRef((JNIEnv*)currentThread, newArray);
		if (NULL == returnProperties) {
			vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
		}

done:
		vmFuncs->internalExitVMToJNI(currentThread);
	}

	return returnProperties;
}


} /* extern "C" */
