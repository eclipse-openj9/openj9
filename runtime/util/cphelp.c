/*******************************************************************************
 * Copyright IBM Corp. and others 2001
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
#include "hashtable_api.h"
#include "util_api.h"
#include "ut_j9vmutil.h"

UDATA
getClassPathEntry(J9VMThread * currentThread, J9ClassLoader * classLoader, IDATA cpIndex, J9ClassPathEntry * cpEntry)
{
	UDATA rc;
	UDATA vmAccess = currentThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS;

	if (vmAccess == 0) {
		/* The only callers without VM access are in the JNI context */
		currentThread->javaVM->internalVMFunctions->internalEnterVMFromJNI(currentThread);
	}
	if ((cpIndex < 0) || ((UDATA)cpIndex >= classLoader->classPathEntryCount)) {
		rc = 1;
	} else {
		Assert_VMUtil_true(classLoader == currentThread->javaVM->systemClassLoader);
		omrthread_rwmutex_enter_read(classLoader->cpEntriesMutex);
		*cpEntry = *(classLoader->classPathEntries[cpIndex]);
		omrthread_rwmutex_exit_read(classLoader->cpEntriesMutex);
		rc = 0;
	}
	if (vmAccess == 0) {
		currentThread->javaVM->internalVMFunctions->internalExitVMToJNI(currentThread);
	}
	return rc;
}

U_8 *
getClassLocation(J9VMThread * currentThread, J9Class * clazz, UDATA *length)
{
	J9ClassPathEntry entry = {0};
	J9JavaVM *javaVM = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = javaVM->internalVMFunctions;
	J9ClassLoader *classLoader = clazz->classLoader;
	U_8 *path = NULL;
	UDATA rc = 0;
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	Assert_VMUtil_true(NULL != length);

	*length = 0; /* Set it to 0 for error cases */

	omrthread_monitor_enter(javaVM->classLoaderModuleAndLocationMutex);

	if (NULL != classLoader->classLocationHashTable) {
		J9ClassLocation *classLocation = vmFuncs->findClassLocationForClass(currentThread, clazz);

		if (NULL != classLocation) {
			switch(classLocation->locationType) {
			case LOAD_LOCATION_PATCH_PATH_NON_GENERATED:
			case LOAD_LOCATION_PATCH_PATH:
				{
					J9ModuleExtraInfo *moduleInfo = vmFuncs->findModuleInfoForModule(currentThread, classLoader, clazz->module);

					/* If the class is loaded from patch path, then an entry should be present in classloader's moduleExtraInfoHashTable */
					Assert_VMUtil_true(NULL != moduleInfo);

					entry = *(moduleInfo->patchPathEntries[classLocation->entryIndex]);
					*length = entry.pathLength;
					path = entry.path;
				}
				break;

			case LOAD_LOCATION_CLASSPATH_NON_GENERATED:
			case LOAD_LOCATION_CLASSPATH:
				rc = getClassPathEntry(currentThread, classLoader, classLocation->entryIndex, &entry);
				if (0 == rc) {
					*length = entry.pathLength;
					path = entry.path;
				}
				break;

			case LOAD_LOCATION_MODULE_NON_GENERATED:
			case LOAD_LOCATION_MODULE:
				{
					J9UTF8 *jrtURL = NULL;
					Assert_VMUtil_true(NULL != clazz->module);

					jrtURL = getModuleJRTURL(currentThread, classLoader, clazz->module);
					if (NULL != jrtURL) {
						*length = J9UTF8_LENGTH(jrtURL);
						path = J9UTF8_DATA(jrtURL);
					}
				}
				break;

			default:
				break;
			}
		}
	}

	omrthread_monitor_exit(javaVM->classLoaderModuleAndLocationMutex);

	return path;
}

J9UTF8 *
getModuleJRTURL(J9VMThread *currentThread, J9ClassLoader *classLoader, J9Module *module)
{
	J9JavaVM *javaVM = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = javaVM->internalVMFunctions;
	J9ModuleExtraInfo *moduleInfo = NULL;
	BOOLEAN newModuleInfo = FALSE;
	J9UTF8 *jrtURL = NULL;
	J9ModuleExtraInfo info = {0};
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	if (NULL == classLoader->moduleExtraInfoHashTable) {
		classLoader->moduleExtraInfoHashTable = vmFuncs->hashModuleExtraInfoTableNew(javaVM, 1);
		if (NULL == classLoader->moduleExtraInfoHashTable) {
			goto _exit;
		}
	} else {
		moduleInfo = vmFuncs->findModuleInfoForModule(currentThread, classLoader, module);
	}

	if (NULL == moduleInfo) {
		moduleInfo = &info;
		moduleInfo->j9module = module;
		newModuleInfo = TRUE;
	} else {
		jrtURL = moduleInfo->jrtURL;
	}

	if (NULL == jrtURL) {
		if (J9_ARE_ALL_BITS_SET(javaVM->runtimeFlags, J9_RUNTIME_JAVA_BASE_MODULE_CREATED)) {
			if (NULL == module->moduleName) {
				goto _exit;
			} else {
				/* Set jrt URL for the module. */
				const char *prependStr = "jrt:/";
				const size_t prependStrLen = strlen(prependStr);
				jrtURL = vmFuncs->copyJ9UTF8WithMemAlloc(
						currentThread, module->moduleName, J9_STR_NONE, prependStr, prependStrLen, NULL, 0);
			}
			if (NULL == jrtURL) {
				goto _exit;
			}
		} else {
			/* It is the java.base module. */
			J9_DECLARE_CONSTANT_UTF8(jrtJavaBaseUrl, "jrt:/java.base");
			const U_16 length = J9UTF8_LENGTH(&jrtJavaBaseUrl);
			const UDATA jrtURLSize = length + sizeof(J9UTF8);
			jrtURL = (J9UTF8 *)j9mem_allocate_memory(jrtURLSize, OMRMEM_CATEGORY_VM);
			if (NULL == jrtURL) {
				goto _exit;
			}
			memcpy(J9UTF8_DATA(jrtURL), J9UTF8_DATA(&jrtJavaBaseUrl), length);
			J9UTF8_SET_LENGTH(jrtURL, length);
		}
		moduleInfo->jrtURL = jrtURL;
	}

	if (TRUE == newModuleInfo) {
		/* Add moduleInfo to the hashtable */
		void *node = hashTableAdd(classLoader->moduleExtraInfoHashTable, (void *)moduleInfo);
		if (NULL == node) {
			/* If we fail to add new moduleInfo to the hashtable, then free up jrtURL */
			j9mem_free_memory(moduleInfo->jrtURL);
			goto _exit;
		}
	}

_exit:
	return jrtURL;
}

/**
 * Append a single path segment to the bootstrap class loader
 *
 * @param [in] vm Java VM
 * @param [in] filename a jar file to be added
 *
 * @return the number of bootstrap classloader classPathEntries afterwards
 * 			Returning value 0 indicates an error has occurred.
 */
UDATA
addJarToSystemClassLoaderClassPathEntries(J9JavaVM *vm, const char *filename)
{
	J9ClassLoader *classLoader = vm->systemClassLoader;
	UDATA newCount = 0;
	UDATA jarPathSize = strlen(filename);
	UDATA classPathLength = jarPathSize + 1; /* add space for a terminating null character */
	UDATA newMemSize = sizeof(J9ClassPathEntry) + classPathLength;
	J9ClassPathEntry *cpEntry = NULL;

	PORT_ACCESS_FROM_JAVAVM(vm);
#if defined(J9VM_OPT_SNAPSHOTS)
	VMSNAPSHOTIMPLPORT_ACCESS_FROM_JAVAVM(vm);
	if (IS_SNAPSHOTTING_ENABLED(vm)) {
		cpEntry = (J9ClassPathEntry *)vmsnapshot_allocate_memory(newMemSize, OMRMEM_CATEGORY_VM);
	} else
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
	{
		cpEntry = (J9ClassPathEntry *)j9mem_allocate_memory(newMemSize, OMRMEM_CATEGORY_VM);
	}
	if (NULL != cpEntry) {
		J9ClassPathEntry **cpePtrArray = NULL;
		UDATA entryCount = 0;
		U_8 *stringCursor = (U_8 *)(cpEntry + 1);
		memset(cpEntry, 0, newMemSize);
		/* Create the new entry */
		memcpy(stringCursor, filename, jarPathSize);
		cpEntry->pathLength = (U_32) jarPathSize;
		cpEntry->path = stringCursor;
		cpEntry->path[cpEntry->pathLength] = 0; /* null character terminated */
		cpEntry->extraInfo = NULL;
		cpEntry->type = CPE_TYPE_UNKNOWN;
		cpEntry->flags = CPE_FLAG_BOOTSTRAP;

		if (J9_ARE_ALL_BITS_SET(classLoader->flags, J9CLASSLOADER_SHARED_CLASSES_ENABLED) || IS_RESTORE_RUN(vm)) {
			/*
			 * Warm up the classpath entry so that the Classpath stored in the cache has the correct info.
			 * This is required because when we are finding classes in the cache, initializeClassPathEntry is not called
			 * */
			if (vm->internalVMFunctions->initializeClassPathEntry(vm, cpEntry) != CPE_TYPE_JAR) {
				goto done;
			}
		}
		omrthread_rwmutex_enter_write(classLoader->cpEntriesMutex);
		entryCount = classLoader->classPathEntryCount;
		cpePtrArray = classLoader->classPathEntries;
		if ((NULL == cpePtrArray)
			|| (0 == entryCount % CPE_COUNT_INCREMENT)
		) {
			/* class path entry pointer array needs to be incremented */
			UDATA count = ROUND_UP_TO(CPE_COUNT_INCREMENT, entryCount + 1);
			newMemSize = sizeof(J9ClassPathEntry*) * count;
#if defined(J9VM_OPT_SNAPSHOTS)
			if (IS_SNAPSHOTTING_ENABLED(vm)) {
				cpePtrArray = (J9ClassPathEntry **)vmsnapshot_reallocate_memory(cpePtrArray, newMemSize, OMRMEM_CATEGORY_VM);
			} else
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
			{
				cpePtrArray = (J9ClassPathEntry **)j9mem_reallocate_memory(cpePtrArray, newMemSize, OMRMEM_CATEGORY_VM);
			}
			if (NULL == cpePtrArray) {
				goto done;
			} else {
				memset(cpePtrArray + entryCount, 0, (count - entryCount) * sizeof(J9ClassPathEntry*));
			}
		}
		/* Everything OK, install the new array and discard the old one. */
		cpePtrArray[entryCount] = cpEntry;
		classLoader->classPathEntries = cpePtrArray;
		issueWriteBarrier();
		newCount = entryCount + 1;
		classLoader->classPathEntryCount = newCount;
		omrthread_rwmutex_exit_write(classLoader->cpEntriesMutex);
	}
done:
	/* If any error occurred, discard any allocated memory and throw OutOfMemoryError */
	if (0 == newCount) {
#if defined(J9VM_OPT_SNAPSHOTS)
		if (IS_SNAPSHOTTING_ENABLED(vm)) {
			vmsnapshot_free_memory(cpEntry);
		} else
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
		{
			j9mem_free_memory(cpEntry);
		}
	} else {
		TRIGGER_J9HOOK_VM_CLASS_LOADER_CLASSPATH_ENTRY_ADDED(vm->hookInterface, vm, classLoader, cpEntry);
	}
	return newCount;
}
