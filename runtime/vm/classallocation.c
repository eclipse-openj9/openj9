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

#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "j9consts.h"
#include "vm_internal.h"
#include "ut_j9vm.h"
#include "j9modron.h"
#include "j2sever.h"
#include "objhelp.h"
#include "vmaccess.h"

#include <string.h>

#ifdef J9VM_THR_PREEMPTIVE
#define ACQUIRE_CLASS_LOADER_BLOCKS_MUTEX(javaVM) (omrthread_monitor_enter((javaVM)->classLoaderBlocksMutex))
#define RELEASE_CLASS_LOADER_BLOCKS_MUTEX(javaVM) (omrthread_monitor_exit((javaVM)->classLoaderBlocksMutex))
#else
#define ACQUIRE_CLASS_LOADER_BLOCKS_MUTEX(javaVM)
#define RELEASE_CLASS_LOADER_BLOCKS_MUTEX(javaVM)
#endif

#define INITIAL_CLASSHASHTABLE_SIZE	16
#define INITIAL_MODULE_HASHTABLE_SIZE	1
#define INITIAL_PACKAGE_HASHTABLE_SIZE	1
#define INITIAL_CLASSLOCATION_HASHTABLE_SIZE 64
#define J9STATIC_ONUNLOAD               "JNI_OnUnload_"
#define J9STATIC_ONUNLOAD_LENGTH        sizeof(J9STATIC_ONUNLOAD)
#define J9DYNAMIC_ONUNLOAD              "JNI_OnUnload"

static void cleanPackage(J9Package *package);

UDATA
initializePackageID(J9ClassLoader *classLoader, J9ROMClass *romClass, J9VMThread *vmThread, IDATA entryIndex, I_32 locationType)
{
	UDATA id = hashPkgTableIDFor(vmThread, classLoader, romClass, entryIndex, locationType);

	return id;
}

#define CLASS_PROPAGATION_TABLE_SIZE 3

U_32 classPropagationTable[CLASS_PROPAGATION_TABLE_SIZE] = {
		J9VMCONSTANTPOOL_JAVALANGSTACKOVERFLOWERROR,
		J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR,
		J9VMCONSTANTPOOL_JAVALANGREFLECTINVOCATIONTARGETEXCEPTION,
};

/* Prevent any possible native out of memory during propagation */
#if CLASS_PROPAGATION_TABLE_SIZE >= INITIAL_CLASSHASHTABLE_SIZE
#error Class propagation size exceeds initial hash table size
#endif

static void
cleanPackage(J9Package *package)
{
	J9HashTableState walkState;

	J9Module** modulePtr = (J9Module**)hashTableStartDo(package->exportsHashTable, &walkState);
	while (NULL != modulePtr) {
		if (NULL != (*modulePtr)->removeExportsHashTable) {
			hashTableRemove((*modulePtr)->removeExportsHashTable, &package);
		}
		modulePtr = (J9Module**)hashTableNextDo(&walkState);
	}
}

J9ClassLoader*
internalAllocateClassLoader(J9JavaVM *javaVM, j9object_t classLoaderObject)
{
	J9ClassLoader *classLoader = NULL;
	J9VMThread *vmThread = currentVMThread(javaVM);
	UDATA i = 0;
	BOOLEAN allocRetried = FALSE;
	J9Class *knownClasses[CLASS_PROPAGATION_TABLE_SIZE];

	Assert_VM_mustHaveVMAccess(vmThread);

	/* Lookup known classes, which may require locking javaVM->classTableMutex */
	for (i = 0; i < CLASS_PROPAGATION_TABLE_SIZE; i++) {
		knownClasses[i] = internalFindKnownClass(vmThread, classPropagationTable[i], J9_FINDKNOWNCLASS_FLAG_EXISTING_ONLY);
	}

retry:
	ACQUIRE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);

	classLoader = J9VMJAVALANGCLASSLOADER_VMREF(vmThread, classLoaderObject);

	if (NULL != classLoader) {
		RELEASE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);
		return classLoader;
	}

	classLoader = allocateClassLoader(javaVM);

	if (NULL == classLoader) {
		if (!allocRetried) {
			allocRetried = TRUE;
			RELEASE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);
			PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, classLoaderObject);
			javaVM->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides(vmThread, J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY);
			classLoaderObject = POP_OBJECT_IN_SPECIAL_FRAME(vmThread);
			goto retry;
		}
		setNativeOutOfMemoryError(vmThread, 0, 0);
		return NULL;
	}

	for (i = 0; i < CLASS_PROPAGATION_TABLE_SIZE; i++) {
		J9Class *knownClass = knownClasses[i];
		if (NULL != knownClass) {
			J9UTF8 *className = J9ROMCLASS_CLASSNAME(knownClass->romClass);
			UDATA hashTableResult = hashClassTableAtPut(vmThread, classLoader, J9UTF8_DATA(className), J9UTF8_LENGTH(className), knownClass);

			/* Because the initial table size is larger than the propagation table size,
			 * there should be no way the addition should fail.
			 */
			Assert_VM_true(0 == hashTableResult);
		}
	}

	if (J9VMJAVALANGCLASSLOADER_ISPARALLELCAPABLE(vmThread, classLoaderObject)) {
		classLoader->flags |= J9CLASSLOADER_PARALLEL_CAPABLE;
	}
	J9CLASSLOADER_SET_CLASSLOADEROBJECT(vmThread, classLoader, classLoaderObject);

	/* All writes into the J9ClassLoader struct must be complete by this point.
	 *
	 * Issue write barrier before storing back into the object due to the unmutexed
	 * fetch and NULL check that takes place when reading the vmRef.
	 */
	issueWriteBarrier();
	J9VMJAVALANGCLASSLOADER_SET_VMREF(vmThread, classLoaderObject, classLoader);

	RELEASE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);
	TRIGGER_J9HOOK_VM_CLASS_LOADER_INITIALIZED(javaVM->hookInterface, vmThread, classLoader);

	return classLoader;
}

J9ClassLoader*
allocateClassLoader(J9JavaVM *javaVM)
{
	J9ClassLoader* classLoader = NULL;

	ACQUIRE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);

	classLoader = pool_newElement(javaVM->classLoaderBlocks);

	if (NULL != classLoader) {
		/* memset not required as the classLoaderBlocks pool returns zero'd memory */

		classLoader->classHashTable = hashClassTableNew(javaVM, INITIAL_CLASSHASHTABLE_SIZE);
#if JAVA_SPEC_VERSION > 8
		classLoader->moduleHashTable = hashModuleNameTableNew(javaVM, INITIAL_MODULE_HASHTABLE_SIZE);
		classLoader->packageHashTable = hashPackageTableNew(javaVM, INITIAL_PACKAGE_HASHTABLE_SIZE);
#endif /* JAVA_SPEC_VERSION > 8 */
		/* Allocate classLocationHashTable only for bootloader which is the first classloader to be allocated.
		 * The classLoader being allocated must be the bootloader if javaVM->systemClassLoader is NULL.
		 */
		if (NULL == javaVM->systemClassLoader) {
			classLoader->classLocationHashTable = hashClassLocationTableNew(javaVM, INITIAL_CLASSLOCATION_HASHTABLE_SIZE);
		}

		if ((NULL == classLoader->classHashTable)
#if JAVA_SPEC_VERSION > 8
			|| (NULL == classLoader->moduleHashTable)
			|| (NULL == classLoader->packageHashTable)
#endif /* JAVA_SPEC_VERSION > 8 */
			|| ((NULL == javaVM->systemClassLoader) && (NULL == classLoader->classLocationHashTable))
		) {
			freeClassLoader(classLoader, javaVM, NULL, TRUE);
			classLoader = NULL;
		} else {
			TRIGGER_J9HOOK_VM_CLASS_LOADER_CREATED(javaVM->hookInterface, javaVM, classLoader);
		}
	}

	RELEASE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);

	return classLoader;
}


void  
freeClassLoader(J9ClassLoader *classLoader, J9JavaVM *javaVM, J9VMThread *vmThread, UDATA needsFrameBuild)
{
#ifdef J9VM_GC_DYNAMIC_CLASS_UNLOADING
	J9VMThread* threadList;
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	PORT_ACCESS_FROM_JAVAVM(javaVM);

	Trc_VM_freeClassLoader_Entry(classLoader);

#ifdef J9VM_GC_DYNAMIC_CLASS_UNLOADING
	/* Check if the class loader is being unloaded already - if not, mark it as such and do the work */
	ACQUIRE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);
	if (classLoader->gcFlags & J9_GC_CLASS_LOADER_UNLOADING) {
		RELEASE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);
		Trc_VM_freeClassLoader_Exit();
		return;
	}
	classLoader->gcFlags |= J9_GC_CLASS_LOADER_UNLOADING;
	RELEASE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);
#endif

	/* free the class path entries allocated ofr system and non-system class loaders */
	if (NULL != classLoader->classPathEntries) {
		if (javaVM->systemClassLoader == classLoader) {
			freeClassLoaderEntries(vmThread, classLoader->classPathEntries, classLoader->classPathEntryCount);
		} else {
			freeSharedCacheCLEntries(vmThread, classLoader);
		}
	}

#ifdef J9VM_NEEDS_JNI_REDIRECTION
	if (classLoader->jniRedirectionBlocks != NULL) {
		J9JNIRedirectionBlock* block = classLoader->jniRedirectionBlocks;

		do {
			J9JNIRedirectionBlock* next = block->next;

			TRIGGER_J9HOOK_VM_DYNAMIC_CODE_UNLOAD(javaVM->hookInterface, vmThread, NULL, block);
			j9vmem_free_memory(block, J9JNIREDIRECT_BLOCK_SIZE, &block->vmemID);

			block = next;
		} while (block != NULL);
	}
#endif

	if (classLoader->sharedLibraries != NULL) {
		pool_state sharedLibrariesWalkState;
		J9NativeLibrary* nativeLibrary = pool_startDo(classLoader->sharedLibraries, &sharedLibrariesWalkState);

		if (NULL != nativeLibrary) {
			Assert_VM_mustHaveVMAccess(vmThread);
		}
		
		while (NULL != nativeLibrary) {
			BOOLEAN unloadPerformed = FALSE;

			if (needsFrameBuild) {
				/* equivalent builder code:
				 *		self
				 *			buildSpecialStackFrame: J9SFJNINativeMethodFrame forThread: aVMThread flags: 0 visible: false;
				 *			push: nil "method"
				 */
				pushEventFrame(vmThread, TRUE, 0);
			}

			internalReleaseVMAccess(vmThread);

			/* If a statically linked library L exports a function called JNI_OnUnLoad_L and a function
			 * called JNI_OnUnLoad, the JNI_OnUnLoad function will be ignored.
			 */
			if (J9NATIVELIB_LINK_MODE_STATIC == nativeLibrary->linkMode) {
				UDATA rc = 0;
				char* onUnloadRtnName = NULL;
				UDATA nameLength = J9STATIC_ONUNLOAD_LENGTH + strlen(nativeLibrary->logicalName) + 1;

				onUnloadRtnName = j9mem_allocate_memory(nameLength, J9MEM_CATEGORY_CLASSES);
				if (NULL == onUnloadRtnName) {
					Trc_VM_freeClassLoader_outOfMemory(vmThread, nameLength);
					/* Prevent call to JNI_OnUnload; can't invoke JNI_OnUnload based on VM oom. */
					unloadPerformed = TRUE;
				} else {
					j9str_printf(PORTLIB, onUnloadRtnName, nameLength, "%s%s", J9STATIC_ONUNLOAD, nativeLibrary->logicalName);

					/* Invoke the JNI_OnUnLoad_L routine, if present. */
					rc = (*nativeLibrary->send_lifecycle_event)(vmThread, nativeLibrary, onUnloadRtnName, (UDATA) -1);
					if ((UDATA) -1 != rc) {
						unloadPerformed = TRUE;		/* Ensure one-time OnUnload for the library. */
						Trc_VM_freeClassLoader_unloadLibrary(vmThread, onUnloadRtnName, nativeLibrary->handle);
					}
					j9mem_free_memory(onUnloadRtnName);
				}
			}

			/* If the library were linked statically and JNI_OnUnLoad_L /not/ provided, run JNI_OnUnLoad, if
			 * this is defined, so as to ensure cleanup.  Note: don't check linkMode for dynamic; JNI_OnUnLoad
			 * may be invoked even for static linking case, i.e., when JNI_OnUnLoad_L is missing.
			 */
			if (!unloadPerformed) {
				(*nativeLibrary->send_lifecycle_event)(vmThread, nativeLibrary, J9DYNAMIC_ONUNLOAD, 0);
				Trc_VM_freeClassLoader_unloadLibrary(vmThread, J9DYNAMIC_ONUNLOAD, nativeLibrary->handle);
			}

			enterVMFromJNI(vmThread);

			if (needsFrameBuild) {
				/* equivalent builder code:
				 *		bp := (J9VMThread on: aVMThread) _arg0EA.
				 *		self
				 *			recordJNIReturn: bp;
				 *			restoreJNICallOutStackFrameForThread: aVMThread bp: bp
				 */
				popEventFrame(vmThread, TRUE);
			} else {
				jniResetStackReferences((JNIEnv*)vmThread);
			}

			(*nativeLibrary->close)(vmThread, nativeLibrary);
			nativeLibrary = pool_nextDo(&sharedLibrariesWalkState);
		}
		/* Can't free the library names or the pool without holding the lock as another thread may be
		 * trying to load a shared library at the same time this thread is unloading.  See
		 * vmbootlib.c:findLoadedSharedLibrary() for the library finding code which requires the
		 * sharedLibraries pool not be released and the library name not be freed without the lock.
		 */
		ACQUIRE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);
		nativeLibrary = pool_startDo(classLoader->sharedLibraries, &sharedLibrariesWalkState);
		while (nativeLibrary != NULL) {
			j9mem_free_memory(nativeLibrary->name);
			j9mem_free_memory(nativeLibrary->logicalName);
			nativeLibrary = pool_nextDo(&sharedLibrariesWalkState);
		}
		pool_kill(classLoader->sharedLibraries);
		classLoader->sharedLibraries = NULL;
		RELEASE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);
	}

	if (classLoader->jniIDs != NULL) {
		pool_kill(classLoader->jniIDs);
	}

	if (NULL != classLoader->classHashTable) {
		hashClassTableFree(classLoader);
	}
	if (NULL != classLoader->moduleExtraInfoHashTable) {
		J9HashTableState moduleExtraInfoWalkState;

		J9ModuleExtraInfo *moduleExtraInfoPtr = (J9ModuleExtraInfo *)hashTableStartDo(classLoader->moduleExtraInfoHashTable, &moduleExtraInfoWalkState);
		while (NULL != moduleExtraInfoPtr) {
			freeClassLoaderEntries(vmThread, moduleExtraInfoPtr->patchPathEntries, moduleExtraInfoPtr->patchPathCount);
			if (NULL != moduleExtraInfoPtr->jrtURL) {
				j9mem_free_memory(moduleExtraInfoPtr->jrtURL);
			}
			moduleExtraInfoPtr = (J9ModuleExtraInfo *)hashTableNextDo(&moduleExtraInfoWalkState);
		}
		hashTableFree(classLoader->moduleExtraInfoHashTable);
		classLoader->moduleExtraInfoHashTable = NULL;	
	}
	if (NULL != classLoader->classLocationHashTable) {
		hashTableFree(classLoader->classLocationHashTable);
		classLoader->classLocationHashTable = NULL;
	}
	if (NULL != classLoader->moduleHashTable) {
		J9HashTableState moduleWalkState;

		J9Module** modulePtr = (J9Module**)hashTableStartDo(classLoader->moduleHashTable, &moduleWalkState);
		while (NULL != modulePtr) {
			J9Module *moduleDel = *modulePtr;
			modulePtr = (J9Module**)hashTableNextDo(&moduleWalkState);
			freeJ9Module(javaVM, moduleDel);
		}

		hashTableFree(classLoader->moduleHashTable);
		classLoader->moduleHashTable = NULL;
	}
	if (NULL != classLoader->packageHashTable) {
		J9HashTableState packageWalkState;

		J9Package** packagePtr = (J9Package**)hashTableStartDo(classLoader->packageHashTable, &packageWalkState);
		while (NULL != packagePtr) {
			J9Package* packageDel = *packagePtr;
			packagePtr = (J9Package**)hashTableNextDo(&packageWalkState);
			cleanPackage(packageDel);
			hashTableFree(packageDel->exportsHashTable);
			j9mem_free_memory(packageDel->packageName);
			pool_removeElement(javaVM->modularityPool, packageDel);
		}

		hashTableFree(classLoader->packageHashTable);
		classLoader->packageHashTable = NULL;
	}

	/* Free the ROM class orphans class table */
	if (NULL != classLoader->romClassOrphansHashTable) {
		hashTableFree(classLoader->romClassOrphansHashTable);
		classLoader->romClassOrphansHashTable = NULL;
	}

	TRIGGER_J9HOOK_VM_CLASS_LOADER_DESTROY(javaVM->hookInterface, javaVM, classLoader);
	ACQUIRE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);

#ifdef J9VM_GC_DYNAMIC_CLASS_UNLOADING
	threadList = classLoader->gcThreadNotification;
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	pool_removeElement(javaVM->classLoaderBlocks, classLoader);

#ifdef J9VM_GC_DYNAMIC_CLASS_UNLOADING
	/* Notify all threads pending that the class loader has completed unloading */
	while (threadList != NULL) {
		J9VMThread* threadListNext = threadList->gcClassUnloadingThreadNext;

		threadList->gcClassUnloadingThreadPrevious = NULL;
		threadList->gcClassUnloadingThreadNext = NULL;

		omrthread_monitor_enter(threadList->gcClassUnloadingMutex);
		omrthread_monitor_notify_all(threadList->gcClassUnloadingMutex);
		omrthread_monitor_exit(threadList->gcClassUnloadingMutex);

		threadList = threadListNext;
	}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

	RELEASE_CLASS_LOADER_BLOCKS_MUTEX(javaVM);

	Trc_VM_freeClassLoader_Exit();
}

