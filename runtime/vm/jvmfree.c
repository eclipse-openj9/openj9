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

#include <string.h>

#include "j9.h"
#include "j9consts.h"
#include "omrlinkedlist.h"
#include "j9protos.h"
#include "j9port.h"
#if defined(J9VM_OPT_SNAPSHOTS)
#include "j9port_generated.h"
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
#include "omrthread.h"
#include "j9user.h"
#include "jimage.h"
#include "jni.h"

#include "../util/ut_module.h"
#include "jvmstackusage.h"
#include "omrutilbase.h"
#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_j9vm.h"
#include "vm_internal.h"
#include "vmaccess.h"
#include "vmhook_internal.h"

#if defined(J9VM_INTERP_VERBOSE)
/* verbose messages for displaying stack info */
#include "verbosenls.h"
#endif

static void trcModulesFreeJ9ModuleEntry(J9JavaVM *javaVM, J9Module *j9module);

void 
freeClassLoaderEntries(J9VMThread * vmThread, J9ClassPathEntry **entries, UDATA count, UDATA initCount)
{
	/* free memory allocated to class path entries */
	J9JavaVM *vm = vmThread->javaVM;
	J9TranslationBufferSet *dynLoadBuffers = vm->dynamicLoadBuffers;
	U_32 i = 0;
	J9ClassPathEntry *cpEntry = NULL;
	PORT_ACCESS_FROM_VMC(vmThread);

	Trc_VM_freeClassLoaderEntries_Entry(vmThread, entries, count);

	for (i = 0; i < count; i++) {
		cpEntry = entries[i];
		if (NULL != cpEntry->extraInfo) {
			switch(cpEntry->type) {
#if defined(J9VM_OPT_ZIP_SUPPORT) && defined(J9VM_OPT_DYNAMIC_LOAD_SUPPORT)
			/* If there is a J9ZipFile allocated -- free it too */
			case CPE_TYPE_JAR:
				dynLoadBuffers->closeZipFileFunction(&vm->vmInterface, (void *) (cpEntry->extraInfo));
				j9mem_free_memory(cpEntry->extraInfo);
				break;
#endif
			case CPE_TYPE_JIMAGE:
				vm->jimageIntf->jimageClose(vm->jimageIntf, (UDATA)cpEntry->extraInfo);
				break;
			default:
				/* Do nothing */
				break;
			}
			cpEntry->extraInfo = NULL;
		}
		cpEntry->path = NULL;
		cpEntry->pathLength = 0;
		if (i >= initCount) {
			/* Additional entries are appended after initial entries, allocated separately. */
			j9mem_free_memory(cpEntry);
		}
	}
	/* Initial entries are allocated together, free them together. */
	if (count > 0) {
		j9mem_free_memory(entries[0]);
	}

	Trc_VM_freeClassLoaderEntries_Exit(vmThread);
}

/**
 * For every non-system class loaders' classpath entries,
 * an entry is added to shared cache's CPPool to be able to reuse it.
 * When classloader is freed, this function is being called to remove the CPPool entry
 * from shared cache associated with the freed classloader's classpath entries.
 */
void
freeSharedCacheCLEntries(J9VMThread * vmThread, J9ClassLoader * classloader)
{
	/* free memory allocated to class path entries */
	J9JavaVM * vm = vmThread->javaVM;
	J9SharedClassConfig *sharedClassConfig = vm->sharedClassConfig;
	J9Pool* cpCachePool;
	PORT_ACCESS_FROM_VMC(vmThread);

	Trc_VM_freeSharedCacheCLEntries_Entry(vmThread, classloader);

	omrthread_monitor_enter(sharedClassConfig->jclCacheMutex);
	cpCachePool = sharedClassConfig->jclClasspathCache;
	if (cpCachePool) {
		J9GenericByID *cachePoolItem = (J9GenericByID *)classloader->classPathEntries[0]->extraInfo;
		if (NULL != cachePoolItem->cpData) {
			sharedClassConfig->freeClasspathData(vm, cachePoolItem->cpData);
		}
		pool_removeElement(cpCachePool, (void *)cachePoolItem);
	}
	j9mem_free_memory(classloader->classPathEntries);
	classloader->classPathEntries = NULL;
	classloader->classPathEntryCount = 0;
	omrthread_monitor_exit(sharedClassConfig->jclCacheMutex);

	Trc_VM_freeSharedCacheCLEntries_Exit(vmThread);
}

static void
recycleVMThread(J9VMThread * vmThread)
{
	J9JavaVM * vm = vmThread->javaVM;

	/* Preserve J9VMThread->startOfMemoryBlock and J9VMThread->J9RIParameters */
	void *startOfMemoryBlock = vmThread->startOfMemoryBlock;
#if defined(J9VM_PORT_RUNTIME_INSTRUMENTATION)
	J9RIParameters *riParameters = vmThread->riParameters;
#endif /* defined(J9VM_PORT_RUNTIME_INSTRUMENTATION) */

	/* Determine the region of the vmThread to preserve: from publicFlagsMutex to threadObject */
	size_t startRegion = offsetof(J9VMThread, publicFlagsMutex);
	size_t endRegion = offsetof(J9VMThread, threadObject);

	/* Indicate that the vmThread is dying */
	vmThread->threadObject = NULL;
#if JAVA_SPEC_VERSION >= 19
	vmThread->carrierThreadObject = NULL;
#endif /* JAVA_SPEC_VERSION >= 19 */

	issueWriteBarrier();

	/* Selectively clear the vmThread */
	memset((U_8 *) vmThread, 0, startRegion);
	memset(((U_8 *) vmThread) + endRegion, 0, J9_VMTHREAD_SEGREGATED_ALLOCATION_CACHE_OFFSET + vm->segregatedAllocationCacheSize - endRegion);

	/* Restore J9VMThread->startOfMemoryBlock and J9VMThread->J9RIParameters */
	vmThread->startOfMemoryBlock = startOfMemoryBlock;
#if defined(J9VM_PORT_RUNTIME_INSTRUMENTATION)
	vmThread->riParameters = riParameters;
	memset(vmThread->riParameters, 0, sizeof(J9RIParameters));
#endif /* defined(J9VM_PORT_RUNTIME_INSTRUMENTATION) */

	/* Clear the public flags except for those related to halting */
	clearEventFlag(vmThread, ~(UDATA)J9_PUBLIC_FLAGS_HALT_THREAD_INSPECTION);

	/* dead threads are stored in "halted for inspection mode" */
	omrthread_monitor_enter(vmThread->publicFlagsMutex);
	if (++vmThread->inspectionSuspendCount == 1) {
		setHaltFlag(vmThread, J9_PUBLIC_FLAGS_HALT_THREAD_INSPECTION);
	}
	omrthread_monitor_exit(vmThread->publicFlagsMutex);

	J9_LINKED_LIST_ADD_LAST(vm->deadThreadList, vmThread);
}


void 
deallocateVMThread(J9VMThread * vmThread, UDATA decrementZombieCount, UDATA sendThreadDestroyEvent)
{
	J9JavaVM * vm = vmThread->javaVM;
	J9PortLibrary * portLibrary = vm->portLibrary;
	J9JavaStack * currentStack;
	PORT_ACCESS_FROM_PORT(portLibrary);

	/* If any exclusive access is in progress, do not let this thread die,
	 * as it may have stored its pointer into the exclusiveAccessStats (which verbose
	 * GC may read).  As soon as the state is NONE, the exclusiveAccessStats are invalid,
	 * so we can be sure that this thread will not (validly) be read from them.
	 */
	omrthread_monitor_enter(vm->exclusiveAccessMutex);
	while (J9_XACCESS_NONE != vm->exclusiveAccessState) {
		omrthread_monitor_wait(vm->exclusiveAccessMutex);
	}
	omrthread_monitor_exit(vm->exclusiveAccessMutex);

	/* If this thread is being inspected, do not allow it to die */

	omrthread_monitor_enter(vm->vmThreadListMutex);
	while (vmThread->inspectorCount != 0) {
		omrthread_monitor_wait(vm->vmThreadListMutex);
	}

	/* Unlink the thread from the list */

	J9_LINKED_LIST_REMOVE(vm->mainThread, vmThread);

	/* This must be called before the GC cleans up, as the cleanup deletes the gc extensions.  The
	 * extensions are used by the RT vm's when calling getVMThreadName because it must go through
	 * the access barrier.
	 */
#if defined(J9VM_INTERP_VERBOSE)
	if ((vm->runtimeFlags & J9_RUNTIME_REPORT_STACK_USE) && vmThread->stackObject) {
		print_verbose_stackUsage(vmThread, FALSE);
	}
#endif
	
	/* vm->memoryManagerFunctions will be NULL if we failed to load the gc dll */
	if (NULL != vm->memoryManagerFunctions) {
		/* Make sure the memory manager does anything needed before shutting down */
		/* Holding the vmThreadListMutex ensures that no heap walking will occur, ergo heap manipulation is safe */
		vm->memoryManagerFunctions->cleanupMutatorModelJava(vmThread);
	}

	/* Call destroy hook if requested */
	if (sendThreadDestroyEvent) {
		TRIGGER_J9HOOK_VM_THREAD_DESTROY(vm->hookInterface, vmThread);
	}

#if JAVA_SPEC_VERSION >= 19
	if (NULL != vmThread->threadObject) {
		/* Deallocate thread object's tls array. */
		freeTLS(vmThread, vmThread->threadObject);
	}

	/* Cleanup Continuation cache */
	if (NULL != vmThread->continuationT1Cache) {
		for (U_32 i = 0; i < vm->continuationT1Size; i++) {
			if (NULL != vmThread->continuationT1Cache[i]) {
				vm->internalVMFunctions->recycleContinuation(vm, NULL, vmThread->continuationT1Cache[i], TRUE);
			}
		}
		j9mem_free_memory(vmThread->continuationT1Cache);
	}
#endif /* JAVA_SPEC_VERSION >= 19 */

	/* freeing the per thread buffers in the portlibrary */
	j9port_tls_free();

	if (vmThread->stackObject) {
		/* Free all stacks that were used by this thread */

		currentStack = vmThread->stackObject;
		do {
			J9JavaStack * previous = currentStack->previous;

			freeJavaStack(vm, currentStack);
			currentStack = previous;
		} while (currentStack);
	}
	
	if (vmThread->privateFlags & J9_PRIVATE_FLAGS_DAEMON_THREAD) {
		--(vm->daemonThreadCount);
	}
	if (vmThread->jniLocalReferences && ((J9JNIReferenceFrame*)vmThread->jniLocalReferences)->references) {
		pool_kill(((J9JNIReferenceFrame*)vmThread->jniLocalReferences)->references);
	}
#if defined(J9VM_GC_JNI_ARRAY_CACHE)
	cleanupVMThreadJniArrayCache(vmThread);
#endif

	if (vmThread->jniReferenceFrames) {
		pool_kill(vmThread->jniReferenceFrames);
	}

	if (NULL != vmThread->monitorEnterRecordPool) {
		pool_kill(vmThread->monitorEnterRecordPool);
	}

	j9mem_free_memory(vmThread->lastDecompilation);

#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
	if (vmThread->dltBlock.temps != vmThread->dltBlock.inlineTempsBuffer) {
		j9mem_free_memory(vmThread->dltBlock.temps);
	}
#endif

	if (NULL != vmThread->utfCache) {
		hashTableFree(vmThread->utfCache);
	}

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	if (NULL != vm->javaOffloadSwitchOffWithReasonFunc) {
		vmThread->javaOffloadState = 0;
		vm->javaOffloadSwitchOffWithReasonFunc(vmThread, J9_JNI_OFFLOAD_SWITCH_DEALLOCATE_VM_THREAD);
	}
#endif

#if JAVA_SPEC_VERSION >= 16
	j9mem_free_memory(vmThread->ffiArgs);
	vmThread->ffiArgs = NULL;
#endif /* JAVA_SPEC_VERSION >= 16 */

	/* Detach the thread from OMR */
	setOMRVMThreadNameWithFlagNoLock(vmThread->omrVMThread, NULL, 0);
	detachVMThreadFromOMR(vm, vmThread);

	recycleVMThread(vmThread); /* Make sure there are no references to vmThread after this line! */
	--(vm->totalThreadCount);
	/* If this thread was not forked by the VM (i.e. it was attached), then decrement the zombie count as deallocating the vmThread is as far as we can track this thread */
	if (decrementZombieCount) {
		--(vm->zombieThreadCount);
	}
	omrthread_monitor_notify_all(vm->vmThreadListMutex);
	omrthread_monitor_exit(vm->vmThreadListMutex);
}

static void
trcModulesFreeJ9ModuleEntry(J9JavaVM *javaVM, J9Module *j9module)
{
	J9VMThread *currentThread = javaVM->mainThread;
	PORT_ACCESS_FROM_VMC(currentThread);
	char moduleNameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	char *moduleNameUTF = copyStringToUTF8WithMemAlloc(
		currentThread, j9module->moduleName, J9_STR_NULL_TERMINATE_RESULT, "", 0, moduleNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH, NULL);
	if (NULL != moduleNameUTF) {
		Trc_MODULE_freeJ9ModuleV2_entry(currentThread, moduleNameUTF, j9module);
		if (moduleNameBuf != moduleNameUTF) {
			j9mem_free_memory(moduleNameUTF);
		}
	}
}

void
freeJ9Module(J9JavaVM *javaVM, J9Module *j9module) {
	/* Removed the module from all other modules readAccessHashTable and removeAccessHashtables */
	J9HashTableState walkState;

	if (TrcEnabled_Trc_MODULE_freeJ9ModuleV2_entry) {
		trcModulesFreeJ9ModuleEntry(javaVM, j9module);
	}

	TRIGGER_J9HOOK_VM_MODULE_UNLOAD(javaVM->hookInterface, javaVM->mainThread, j9module);

	if (NULL != j9module->removeAccessHashTable) {
		J9Module **modulePtr = (J9Module**)hashTableStartDo(j9module->removeAccessHashTable, &walkState);
		while (NULL != modulePtr) {
			hashTableRemove((*modulePtr)->readAccessHashTable, &j9module);
			modulePtr = (J9Module**)hashTableNextDo(&walkState);
		}
		hashTableFree(j9module->removeAccessHashTable);
	}

	if (NULL != j9module->readAccessHashTable) {
		J9Module **modulePtr = (J9Module**)hashTableStartDo(j9module->readAccessHashTable, &walkState);
		while (NULL != modulePtr) {
			if (NULL != (*modulePtr)->removeAccessHashTable) {
				hashTableRemove((*modulePtr)->removeAccessHashTable, &j9module);
			}
			modulePtr = (J9Module**)hashTableNextDo(&walkState);
		}
		hashTableFree(j9module->readAccessHashTable);
	}

	if (NULL != j9module->removeExportsHashTable) {
		J9Package **packagePtr = (J9Package**)hashTableStartDo(j9module->removeExportsHashTable, &walkState);
		while (NULL != packagePtr) {
			hashTableRemove((*packagePtr)->exportsHashTable, &j9module);
			packagePtr = (J9Package**)hashTableNextDo(&walkState);
		}

		hashTableFree(j9module->removeExportsHashTable);
	}

	pool_removeElement(javaVM->modularityPool, j9module);

	Trc_MODULE_freeJ9Module_exit(j9module);
}

#if (defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)) 
/**
 * Perform classloader-specific cleanup.  The current thread has exclusive access.
 * J9HOOK_VM_CLASS_LOADER_UNLOAD is triggered.
 *
 * @note The classLoader's classLoaderObject, classHashTable and classPathEntries are all NULL upon return of this function.
 *
 * @param classLoader the classloader to cleanup
 */
void
cleanUpClassLoader(J9VMThread *vmThread, J9ClassLoader* classLoader) 
{
	J9JavaVM *javaVM = vmThread->javaVM;
	Trc_VM_cleanUpClassLoaders_Entry(vmThread, classLoader);

	Trc_VM_triggerClassLoaderUnloadHook_Entry(vmThread, classLoader);
	TRIGGER_J9HOOK_VM_CLASS_LOADER_UNLOAD(javaVM->hookInterface, vmThread, classLoader);
	Trc_VM_triggerClassLoaderUnloadHook_Exit(vmThread);

	/* NULL the object out to avoid confusion */
	classLoader->classLoaderObject = NULL;

	/* Free the class table */
	if (NULL != classLoader->classHashTable) {
		hashClassTableFree(classLoader);
	}

	/* Free the rom class orphans class table */
	if (NULL != classLoader->romClassOrphansHashTable) {
		hashTableFree(classLoader->romClassOrphansHashTable);
		classLoader->romClassOrphansHashTable = NULL;
	}

	if (classLoader == javaVM->systemClassLoader) {
		if (NULL != classLoader->classPathEntries) {
			PORT_ACCESS_FROM_VMC(vmThread);
			/* Free the class path entries  in system class loader */
			freeClassLoaderEntries(vmThread, classLoader->classPathEntries, classLoader->classPathEntryCount, classLoader->initClassPathEntryCount);
			j9mem_free_memory(classLoader->classPathEntries);
			classLoader->classPathEntryCount = 0;
			classLoader->classPathEntries = NULL;
			if (NULL != classLoader->cpEntriesMutex) {
				j9thread_rwmutex_destroy(classLoader->cpEntriesMutex);
				classLoader->cpEntriesMutex = NULL;
			}
		}
	} else {
		/* Free the class path entries in non-system class loaders.
		 * classLoader->classPathEntries is set to NULL inside freeSharedCacheCLEntries().
		 */
		if (NULL != classLoader->classPathEntries) {
			freeSharedCacheCLEntries(vmThread, classLoader);
		}
	}

	Trc_VM_cleanUpClassLoaders_Exit(vmThread);
}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

