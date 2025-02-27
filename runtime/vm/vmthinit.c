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

/* #define J9VM_DBG */

#include <string.h>
#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "omrthread.h"
#include "jni.h"
#include "j9consts.h"
#include "vmaccess.h"
#include "vm_internal.h"
#include "omrlinkedlist.h"
#include "j2sever.h"

/* processReferenceMonitor is only used for Java 9 and later */
#define J9_IS_PROCESS_REFERENCE_MONITOR_ENABLED(vm) (J2SE_VERSION(vm) >= J2SE_V11)

UDATA initializeVMThreading(J9JavaVM *vm)
{
	if (
		omrthread_monitor_init_with_name(&vm->vmThreadListMutex, 0, "VM thread list") ||
		omrthread_monitor_init_with_name(&vm->exclusiveAccessMutex, 0, "VM exclusive access") ||
		omrthread_monitor_init_with_name(&vm->runtimeFlagsMutex, 0, "VM Runtime flags Mutex") ||
		omrthread_monitor_init_with_name(&vm->extendedMethodFlagsMutex, 0, "VM Extended method block flags Mutex") ||
		omrthread_monitor_init_with_name(&vm->asyncEventMutex, 0, "Async event mutex") ||
#if defined(J9VM_JIT_CLASS_UNLOAD_RWMONITOR)
		omrthread_rwmutex_init(&vm->classUnloadMutex, 0, "JIT/GC class unload mutex") ||
#else
		omrthread_monitor_init_with_name(&vm->classUnloadMutex, 0, "JIT/GC class unload mutex") ||
#endif
		omrthread_monitor_init_with_name(&vm->bindNativeMutex, 0, "VM bind native") ||
		omrthread_monitor_init_with_name(&vm->jclCacheMutex, 0, "JCL cache mutex") ||
		omrthread_monitor_init_with_name(&vm->statisticsMutex, 0, "VM Statistics List Mutex") ||
		omrthread_monitor_init_with_name(&vm->fieldIndexMutex, 0, "Field Index Hashtable Mutex") ||
		omrthread_monitor_init_with_name(&vm->jniCriticalLock, 0, "JNI critical region mutex") ||

#ifdef J9VM_THR_PREEMPTIVE
		omrthread_monitor_init_with_name(&vm->classLoaderModuleAndLocationMutex, 0, "VM class loader modules") ||
		omrthread_monitor_init_with_name(&vm->classLoaderBlocksMutex, 0, "VM class loader blocks") ||
		omrthread_monitor_init_with_name(&vm->classTableMutex, 0, "VM class table") ||
		omrthread_monitor_init_with_name(&vm->segmentMutex, 0 ,"VM segment") ||
		omrthread_monitor_init_with_name(&vm->jniFrameMutex, 0, "VM JNI frame") ||
#endif

#ifdef J9VM_GC_FINALIZATION
		omrthread_monitor_init_with_name(&vm->finalizeMainMonitor, 0, "VM GC finalize main") ||
		omrthread_monitor_init_with_name(&vm->finalizeRunFinalizationMutex, 0, "VM GC finalize run finalization") ||
		(J9_IS_PROCESS_REFERENCE_MONITOR_ENABLED(vm) && omrthread_monitor_init_with_name(&vm->processReferenceMonitor, 0, "VM GC process reference")) ||
#endif

		omrthread_monitor_init_with_name(&vm->aotRuntimeInitMutex, 0, "VM AOT runtime init") ||

		omrthread_monitor_init_with_name(&vm->osrGlobalBufferLock, 0, "OSR global buffer lock") ||

		omrthread_monitor_init_with_name(&vm->nativeLibraryMonitor, 0, "JNI native library loading lock") ||

		omrthread_monitor_init_with_name(&vm->vmRuntimeStateListener.runtimeStateListenerMutex, 0, "VM state notification mutex") ||

		omrthread_monitor_init_with_name(&vm->constantDynamicMutex, 0, "Wait mutex for constantDynamic during resolve") ||

#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
		omrthread_monitor_init_with_name(&vm->valueTypeVerificationMutex, 0, "Wait mutex for verifying valuetypes") ||
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */

#if JAVA_SPEC_VERSION >= 16
		omrthread_monitor_init_with_name(&vm->cifNativeCalloutDataCacheMutex, 0, "CIF cache mutex") ||
		omrthread_monitor_init_with_name(&vm->cifArgumentTypesCacheMutex, 0, "CIF argument types mutex") ||
		omrthread_monitor_init_with_name(&vm->thunkHeapListMutex, 0, "Wait mutex for allocating the upcall thunk memory") ||
#endif /* JAVA_SPEC_VERSION >= 16 */

#if JAVA_SPEC_VERSION >= 19
		/* Held when adding or removing a virtual thread from the list at virtual thread start or terminate. */
		omrthread_monitor_init_with_name(&vm->tlsFinalizersMutex, 0, "TLS finalizers mutex") ||
		omrthread_monitor_init_with_name(&vm->tlsPoolMutex, 0, "TLS pool mutex") ||
#endif /* JAVA_SPEC_VERSION >= 19 */
#if defined(J9VM_OPT_CRIU_SUPPORT)
		omrthread_monitor_init_with_name(&vm->delayedLockingOperationsMutex, 0, "Delayed locking operations mutex") ||
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

#if JAVA_SPEC_VERSION >= 22
		omrthread_monitor_init_with_name(&vm->closeScopeMutex, 0, "ScopedMemoryAccess closeScope0 mutex") ||
#endif /* JAVA_SPEC_VERSION >= 22 */

		initializeMonitorTable(vm)
	)
	{
		return 1;
	}
	return 0;
}

/*
 * Frees memory allocated for the J9VMThread
 */
void freeVMThread(J9JavaVM *vm, J9VMThread *vmThread)
{
	PORT_ACCESS_FROM_PORT(vm->portLibrary);
#if defined(J9VM_PORT_RUNTIME_INSTRUMENTATION)
	if (NULL != vmThread->riParameters) {
		/* Free J9RIParameters. */
		j9mem_free_memory(vmThread->riParameters);
	}
#endif /* defined(J9VM_PORT_RUNTIME_INSTRUMENTATION) */
	if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm)) {
		j9mem_free_memory32(vmThread->startOfMemoryBlock);
	} else {
		j9mem_free_memory(vmThread->startOfMemoryBlock);
	}
}

void terminateVMThreading(J9JavaVM *vm)
{
	J9VMThread *vmThread;

	while (!J9_LINKED_LIST_IS_EMPTY(vm->deadThreadList)) {
		J9_LINKED_LIST_REMOVE_FIRST(vm->deadThreadList, vmThread);

		if (NULL != vmThread->publicFlagsMutex) {
			omrthread_monitor_destroy(vmThread->publicFlagsMutex);
		}
		destroyOMRVMThread(vm, vmThread);
		freeVMThread(vm, vmThread);
	}

#ifdef J9VM_THR_PREEMPTIVE
	if (vm->segmentMutex) omrthread_monitor_destroy(vm->segmentMutex);
	if (vm->classTableMutex) omrthread_monitor_destroy(vm->classTableMutex);
	if (vm->classLoaderModuleAndLocationMutex) omrthread_monitor_destroy(vm->classLoaderModuleAndLocationMutex);
	if (vm->classLoaderBlocksMutex) omrthread_monitor_destroy(vm->classLoaderBlocksMutex);
	if (vm->jniFrameMutex) omrthread_monitor_destroy(vm->jniFrameMutex);
#endif

	if (vm->runtimeFlagsMutex) omrthread_monitor_destroy(vm->runtimeFlagsMutex);
	if (vm->extendedMethodFlagsMutex) omrthread_monitor_destroy(vm->extendedMethodFlagsMutex);
	if (vm->vmThreadListMutex) omrthread_monitor_destroy(vm->vmThreadListMutex);
	if (vm->exclusiveAccessMutex) omrthread_monitor_destroy(vm->exclusiveAccessMutex);
	if (vm->statisticsMutex) omrthread_monitor_destroy(vm->statisticsMutex);
	if (vm->jniCriticalLock) omrthread_monitor_destroy(vm->jniCriticalLock);
#if defined(J9VM_JIT_CLASS_UNLOAD_RWMONITOR)
	if (vm->classUnloadMutex) omrthread_rwmutex_destroy(vm->classUnloadMutex);
#else
	if (vm->classUnloadMutex) omrthread_monitor_destroy(vm->classUnloadMutex);
#endif
	if (vm->bindNativeMutex) omrthread_monitor_destroy(vm->bindNativeMutex);

#ifdef J9VM_GC_FINALIZATION
	if (vm->finalizeMainMonitor) omrthread_monitor_destroy(vm->finalizeMainMonitor);
	if (NULL != vm->processReferenceMonitor) omrthread_monitor_destroy(vm->processReferenceMonitor);
	if (vm->finalizeRunFinalizationMutex) omrthread_monitor_destroy(vm->finalizeRunFinalizationMutex);
#endif


	if (vm->aotRuntimeInitMutex) omrthread_monitor_destroy(vm->aotRuntimeInitMutex );
	if (vm->jclCacheMutex) omrthread_monitor_destroy(vm->jclCacheMutex);
	if (vm->asyncEventMutex) omrthread_monitor_destroy(vm->asyncEventMutex);
	if (vm->osrGlobalBufferLock) omrthread_monitor_destroy(vm->osrGlobalBufferLock);
	if (vm->nativeLibraryMonitor) omrthread_monitor_destroy(vm->nativeLibraryMonitor);
	if (vm->vmRuntimeStateListener.runtimeStateListenerMutex) omrthread_monitor_destroy(vm->vmRuntimeStateListener.runtimeStateListenerMutex);
	if (vm->constantDynamicMutex) omrthread_monitor_destroy(vm->constantDynamicMutex);
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
	if (vm->valueTypeVerificationMutex) omrthread_monitor_destroy(vm->valueTypeVerificationMutex);
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
#if JAVA_SPEC_VERSION >= 16
	if (NULL != vm->cifNativeCalloutDataCacheMutex) {
		omrthread_monitor_destroy(vm->cifNativeCalloutDataCacheMutex);
		vm->cifNativeCalloutDataCacheMutex = NULL;
	}

	if (NULL != vm->cifArgumentTypesCacheMutex) {
		omrthread_monitor_destroy(vm->cifArgumentTypesCacheMutex);
		vm->cifArgumentTypesCacheMutex = NULL;
	}

	if (NULL != vm->thunkHeapListMutex) {
		omrthread_monitor_destroy(vm->thunkHeapListMutex);
		vm->thunkHeapListMutex = NULL;
	}
#endif /* JAVA_SPEC_VERSION >= 16 */

#if JAVA_SPEC_VERSION >= 19
	if (NULL != vm->tlsFinalizersMutex) {
		omrthread_monitor_destroy(vm->tlsFinalizersMutex);
		vm->tlsFinalizersMutex = NULL;
	}
	if (NULL != vm->tlsPoolMutex) {
		omrthread_monitor_destroy(vm->tlsPoolMutex);
		vm->tlsPoolMutex = NULL;
	}
#endif /* JAVA_SPEC_VERSION >= 19 */

	destroyMonitorTable(vm);
}

