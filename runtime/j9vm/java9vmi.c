/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include <assert.h>
#include "vm_api.h"
#include "j2sever.h"
#include "j9.h"
#include "hashtable_api.h"
#include "../util/ut_module.h"
#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_j9scar.h"
#include "jclprots.h"
#include "jvminit.h"
#include "util_api.h"
#include "j9vmnls.h"

#define J9TIME_NANOSECONDS_PER_SECOND         ((jlong) 1000000000)
/* Need to do a |currentSecondsTime - secondsOffset| < (2^32) check to ensure that the
 * resulting time fits into a long so it doesn't overflow. This is equivalent to doing
 * |currentNanoTime - nanoTimeOffset| <  4294967295000000000.
 */
#define TIME_LONG_MAX     ((jlong) 4294967295000000000LL)
#define TIME_LONG_MIN     ((jlong) -4294967295000000000LL)
#define OFFSET_MAX        ((jlong) 0x225C17D04LL)         /*  2^63/10^9 */
#define OFFSET_MIN        ((jlong) 0xFFFFFFFDDA3E82FCLL)  /* -2^63/10^9 */

#define HASHTABLE_ATPUT_GENERAL_FAILURE     ((UDATA) 1U)
#define HASHTABLE_ATPUT_COLLISION_FAILURE   ((UDATA) 2U)
#define HASHTABLE_ATPUT_SUCCESS             ((UDATA) 0U)

#define INITIAL_INTERNAL_MODULE_HASHTABLE_SIZE      1
#define INITIAL_INTERNAL_PACKAGE_HASHTABLE_SIZE		1

/* Ensure J9VM_JAVA9_BUILD is always defined to simplify conditions. */
#ifndef J9VM_JAVA9_BUILD
#define J9VM_JAVA9_BUILD 0
#endif /* J9VM_JAVA9_BUILD */

/* All the helper functions below assume that:
 * a) If VMAccess is required, it assumes the caller has already done so
 * b) If performing a hash operation, it assumes the caller has already locked vm->classLoaderModuleAndLocationMutex
 */
#if J9VM_JAVA9_BUILD >= 156
static UDATA hashPackageTableDelete(J9VMThread * currentThread, J9ClassLoader * classLoader, const char *packageName);
static J9Package * createPackage(J9VMThread * currentThread, J9Module * fromModule, const char *package);
static void freePackageDefinition(J9VMThread * currentThread, J9ClassLoader * classLoader, const char *packageName);
static BOOLEAN removePackageDefinition(J9VMThread * currentThread, J9Module * fromModule, const char *packageName);
static BOOLEAN addPackageDefinition(J9VMThread * currentThread, J9Module * fromModule, const char *package);
static UDATA addMulPackageDefinitions(J9VMThread * currentThread, J9Module * fromModule, const char* const* packages, U_32 numPackages);
static void removeMulPackageDefinitions(J9VMThread * currentThread, J9Module * fromModule, const char* const* packages, U_32 packagesIndex);
static UDATA addModuleDefinition(J9VMThread * currentThread, J9Module * fromModule, const char* const* packages, U_32 numPackages, jstring version);
static BOOLEAN isPackageDefined(J9VMThread * currentThread, J9ClassLoader * classLoader, const char *packageName);
static BOOLEAN areNoPackagesDefined(J9VMThread * currentThread, J9ClassLoader * classLoader, const char* const* packages, U_32 numPackages);
static UDATA exportPackageToAll(J9VMThread * currentThread, J9Module * fromModule, const char *package);
static UDATA exportPackageToAllUnamed(J9VMThread * currentThread, J9Module * fromModule, const char *package);
static UDATA exportPackageToModule(J9VMThread * currentThread, J9Module * fromModule, const char *package, J9Module * toModule);
static void trcModulesCreationPackage(J9VMThread * currentThread, J9Module * fromModule, const char *package);
static void trcModulesAddModuleExportsToAll(J9VMThread * currentThread, J9Module * fromModule, const char *package);
static void trcModulesAddModuleExportsToAllUnnamed(J9VMThread * currentThread, J9Module * fromModule, const char *package);
static void trcModulesAddModuleExports(J9VMThread * currentThread, J9Module * fromModule, const char *package, J9Module * toModule);
static void trcModulesAddModulePackage(J9VMThread *currentThread, J9Module *j9mod, const char *package);
#else /* J9VM_JAVA9_BUILD >= 156 */
static UDATA hashPackageTableDelete(J9VMThread * currentThread, J9ClassLoader * classLoader, j9object_t packageName);
static J9Package * createPackage(J9VMThread * currentThread, J9Module * fromModule, j9object_t package);
static void freePackageDefinition(J9VMThread * currentThread, J9ClassLoader * classLoader, j9object_t packageName);
static BOOLEAN removePackageDefinition(J9VMThread * currentThread, J9Module * fromModule, j9object_t packageName);
static BOOLEAN addPackageDefinition(J9VMThread * currentThread, J9Module * fromModule, j9object_t package);
static UDATA addMulPackageDefinitions(J9VMThread * currentThread, J9Module * fromModule, jobjectArray packages);
static void removeMulPackageDefinitions(J9VMThread * currentThread, J9Module * fromModule, jobjectArray packages, U_32 packagesIndex);
static UDATA addModuleDefinition(J9VMThread * currentThread, J9Module * fromModule, jobjectArray packages, jstring version);
static BOOLEAN isPackageDefined(J9VMThread * currentThread, J9ClassLoader * classLoader, j9object_t packageName);
static BOOLEAN areNoPackagesDefined(J9VMThread * currentThread, J9ClassLoader * classLoader, jobjectArray packages);
static UDATA exportPackageToAll(J9VMThread * currentThread, J9Module * fromModule, jstring package);
static UDATA exportPackageToAllUnamed(J9VMThread * currentThread, J9Module * fromModule, jstring package);
static UDATA exportPackageToModule(J9VMThread * currentThread, J9Module * fromModule, jstring package, J9Module * toModule);
static void trcModulesCreationPackage(J9VMThread * currentThread, J9Module * fromModule, j9object_t package);
static void trcModulesAddModuleExportsToAll(J9VMThread * currentThread, J9Module * fromModule, jstring package);
static void trcModulesAddModuleExportsToAllUnnamed(J9VMThread * currentThread, J9Module * fromModule, jstring package);
static void trcModulesAddModuleExports(J9VMThread * currentThread, J9Module * fromModule, jstring package, J9Module * toModule);
static void trcModulesAddModulePackage(J9VMThread *currentThread, J9Module *j9mod, jstring package);
#endif /* J9VM_JAVA9_BUILD >= 156 */
static UDATA hashTableAtPut(J9HashTable * table, void * value, BOOLEAN collisionIsFailure);
static void throwExceptionHelper(J9VMThread * currentThread, UDATA errCode);
static void freePackage(J9VMThread * currentThread, J9Package * j9package);
static J9ClassLoader * getModuleObjectClassLoader(J9VMThread * currentThread, j9object_t moduleObject);
static j9object_t getModuleObjectName(J9VMThread * currentThread, j9object_t moduleObject);
static J9Module * createModule(J9VMThread * currentThread, j9object_t moduleObject, J9ClassLoader * classLoader, j9object_t moduleName);
static J9Module * getJ9Module(J9VMThread * currentThread, jobject module);
static BOOLEAN isModuleNameValid(j9object_t moduleName);
static BOOLEAN isModuleJavaBase(j9object_t moduleName);
static BOOLEAN isModuleNameGood(j9object_t moduleName);
static UDATA allowReadAccessToModule(J9VMThread * currentThread, J9Module * fromModule, J9Module * toModule);
static void trcModulesAddReadsModule(J9VMThread *currentThread, jobject toModule, J9Module *j9FromMod, J9Module *j9ToMod);

#if !defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
/* These come from jvm.c */
extern IDATA (*f_monitorEnter)(omrthread_monitor_t monitor);
extern IDATA (*f_monitorExit)(omrthread_monitor_t monitor);
#endif /* !defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */

static UDATA
hashTableAtPut(J9HashTable * table, void * value, BOOLEAN collisionIsFailure)
{
	UDATA retval = HASHTABLE_ATPUT_GENERAL_FAILURE;
	void * node = NULL;

	/* hashTableAdd() will return the conflicting entry found in the hash in case of collision. Therefore,
	 * we can't use it to figure out whether our value is already found in the has
	 */
	node = hashTableFind(table, value);

	/* If no conflicting entry is found ... */
	if (NULL == node) {
		node = hashTableAdd(table, value);

		if (NULL != node) {
			retval = HASHTABLE_ATPUT_SUCCESS;
		}
	} else if (collisionIsFailure) {
		retval = HASHTABLE_ATPUT_COLLISION_FAILURE;
	} else {
		retval = HASHTABLE_ATPUT_SUCCESS;
	}

	return retval;
}

static UDATA
#if J9VM_JAVA9_BUILD >= 156
hashPackageTableDelete(J9VMThread * currentThread, J9ClassLoader * classLoader, const char *packageName)
#else /* J9VM_JAVA9_BUILD >= 156 */
hashPackageTableDelete(J9VMThread * currentThread, J9ClassLoader * classLoader, j9object_t packageName)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	J9HashTable * table = classLoader->packageHashTable;
	J9Package package = {0};
	J9Package * packagePtr = &package;
	PORT_ACCESS_FROM_VMC(currentThread);
	UDATA rc = 1; /* hashTableRemove failure code */
	U_8 buf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];

	if (!addUTFNameToPackage(currentThread, packagePtr, packageName, buf, J9VM_PACKAGE_NAME_BUFFER_LENGTH)) {
		return rc;
	}

	rc = hashTableRemove(table, &package);

	if ((U_8 *) package.packageName != (U_8 *)buf) {
		j9mem_free_memory((void *) package.packageName);
	}
	return rc;
}

/** @todo The strings below need to be NLS and also provide more debug info */
static void
throwExceptionHelper(J9VMThread * currentThread, UDATA errCode)
{
	J9JavaVM const * const vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;

	J9_DECLARE_CONSTANT_UTF8(errGeneralFailure, "general failure");
	J9_DECLARE_CONSTANT_UTF8(errPackageAlreadyDefined, "a package in the list has already been define");
	J9_DECLARE_CONSTANT_UTF8(errModuleAlreadyDefined, "the module has already been defined");
	J9_DECLARE_CONSTANT_UTF8(errHashTableOperationFailed, "hash operation failed");
	J9_DECLARE_CONSTANT_UTF8(errDuplicatePackageInList, "the list contains duplicate packages");
	J9_DECLARE_CONSTANT_UTF8(errModuleWasntFound, "module was not found");
	J9_DECLARE_CONSTANT_UTF8(errPackageWasntFound, "package was not found");

	static J9UTF8 const * const errMessages[] = {
		NULL,                                  /* ERRCODE_SUCCESS */
		(J9UTF8*)&errGeneralFailure,           /* ERRCODE_GENERAL_FAILURE */
		(J9UTF8*)&errPackageAlreadyDefined,    /* ERRCODE_PACKAGE_ALREADY_DEFINED */
		(J9UTF8*)&errModuleAlreadyDefined,     /* ERRCODE_MODULE_ALREADY_DEFINED */
		(J9UTF8*)&errHashTableOperationFailed, /* ERRCODE_HASHTABLE_OPERATION_FAILED */
		(J9UTF8*)&errDuplicatePackageInList,   /* ERRCODE_DUPLICATE_PACKAGE_IN_LIST */
		(J9UTF8*)&errModuleWasntFound,         /* ERRCODE_MODULE_WASNT_FOUND */
		(J9UTF8*)&errPackageWasntFound         /* ERRCODE_PACKAGE_WASNT_FOUND */
	};

	if (ERRCODE_SUCCESS != errCode) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, (const char*)errMessages[errCode]);
	}
}

static void
freePackage(J9VMThread * currentThread, J9Package * j9package)
{
	if (NULL != j9package) {
		J9JavaVM * const vm = currentThread->javaVM;
		PORT_ACCESS_FROM_JAVAVM(vm);

		if (NULL != j9package->exportsHashTable) {
			hashTableFree(j9package->exportsHashTable);
		}
		j9mem_free_memory((void *) j9package->packageName);
		pool_removeElement(vm->modularityPool, j9package);
	}
}

static J9Package *
#if J9VM_JAVA9_BUILD >= 156
createPackage(J9VMThread * currentThread, J9Module * fromModule, const char *package)
#else /* J9VM_JAVA9_BUILD >= 156 */
createPackage(J9VMThread * currentThread, J9Module * fromModule, j9object_t package)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	J9JavaVM * const vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;
	J9Package * retval = NULL;

	J9ClassLoader * const classLoader = fromModule->classLoader;
	J9Package * j9package = pool_newElement(vm->modularityPool);

	if (NULL != j9package) {
		j9package->module = fromModule;
		j9package->classLoader = fromModule->classLoader;
		if (!addUTFNameToPackage(currentThread, j9package, package, NULL, 0)) {
			freePackage(currentThread, j9package);
			return retval;
		}
		j9package->exportsHashTable = vmFuncs->hashModuleTableNew(vm, INITIAL_INTERNAL_MODULE_HASHTABLE_SIZE);
		if (NULL != j9package->exportsHashTable) {
			retval = j9package;
		}
	}

	/* if we failed to create the package */	
	if (NULL == retval) {
		if (NULL != j9package) {
			freePackage(currentThread, j9package);
		}
		vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
	}

	return retval;
}

/** @note It assumes moduleObject is guaranteed not to be NULL
 *  @return Pointer to module's classloader
 */
static J9ClassLoader *
getModuleObjectClassLoader(J9VMThread * currentThread, j9object_t moduleObject)
{
	j9object_t classLoader = NULL;
	if(J2SE_SHAPE(currentThread->javaVM) < J2SE_SHAPE_B165) {
		classLoader = J9VMJAVALANGREFLECTMODULE_LOADER(currentThread, moduleObject);
	} else {
		classLoader = J9VMJAVALANGMODULE_LOADER(currentThread, moduleObject);
	}
	if (NULL == classLoader) {
		return currentThread->javaVM->systemClassLoader;
	} else {
		J9ClassLoader *loader = J9VMJAVALANGCLASSLOADER_VMREF(currentThread, classLoader);
		if (NULL == loader) {
			J9JavaVM * const vm = currentThread->javaVM;
			loader = vm->internalVMFunctions->internalAllocateClassLoader(vm, classLoader);
		}
		return loader;
	}
}

/** @note It assumes moduleObject is guaranteed not to be NULL
 *  @return Java String containing module name. 
 */
static j9object_t
getModuleObjectName(J9VMThread * currentThread, j9object_t moduleObject)
{
	if(J2SE_SHAPE(currentThread->javaVM) < J2SE_SHAPE_B165) {
		return J9VMJAVALANGREFLECTMODULE_NAME(currentThread, moduleObject);
	} else {
		return J9VMJAVALANGMODULE_NAME(currentThread, moduleObject);
	}
}

/** @throws OutOfMemory exception if memory cannot be allocated */
static J9Module *
createModule(J9VMThread * currentThread, j9object_t moduleObject, J9ClassLoader * classLoader, j9object_t moduleName)
{
	J9JavaVM * const vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;
	J9Module * j9mod = NULL;
	J9Module * retval = NULL;

	if (J9_ARE_ALL_BITS_SET(vm->runtimeFlags, J9_RUNTIME_JAVA_BASE_MODULE_CREATED)) {
		j9mod = pool_newElement(vm->modularityPool);
	} else {
		if (NULL == moduleName) {
			/* moduleName is passed as NULL for the unnamed module for bootloader created by JVM_SetBootLoaderUnnamedModule() */
			j9mod = vm->unamedModuleForSystemLoader;
		} else {
			j9mod = vm->javaBaseModule;
			j9mod->isLoose = TRUE;
		}
	}
	if (NULL != j9mod) {
		j9mod->moduleName = moduleName;

		j9mod->readAccessHashTable = vmFuncs->hashModuleTableNew(vm, INITIAL_INTERNAL_MODULE_HASHTABLE_SIZE);

		if (NULL != j9mod->readAccessHashTable) {
			j9mod->classLoader = classLoader;
			/* The GC is expected to update pointer below if it moves the object */
			j9mod->moduleObject = moduleObject;

			/* Bind J9Module and module object via the hidden field */
			J9OBJECT_ADDRESS_STORE(currentThread, moduleObject, vm->modulePointerOffset, j9mod);

			retval = j9mod;
		}
	}

	/* If we failed to create the module */
	if (NULL == retval) {
		if (NULL != j9mod) {
			vmFuncs->freeJ9Module(vm, j9mod);
		}
		vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
	}

	return retval;
}

static void
#if J9VM_JAVA9_BUILD >= 156
freePackageDefinition(J9VMThread * currentThread, J9ClassLoader * classLoader, const char *packageName)
#else /* J9VM_JAVA9_BUILD >= 156 */
freePackageDefinition(J9VMThread * currentThread, J9ClassLoader * classLoader, j9object_t packageName)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	J9Package * j9package = hashPackageTableAt(currentThread, classLoader, packageName);

	if (NULL != j9package) {
		freePackage(currentThread, j9package);
	}
}

static BOOLEAN
#if J9VM_JAVA9_BUILD >= 156
removePackageDefinition(J9VMThread * currentThread, J9Module * fromModule, const char *packageName)
#else /* J9VM_JAVA9_BUILD >= 156 */
removePackageDefinition(J9VMThread * currentThread, J9Module * fromModule, j9object_t packageName)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	J9ClassLoader * const classLoader = fromModule->classLoader;

	BOOLEAN const retval = (0 == hashPackageTableDelete(currentThread, classLoader, packageName));

	freePackageDefinition(currentThread, classLoader, packageName);

	return retval;
}

static void
#if J9VM_JAVA9_BUILD >= 156
trcModulesCreationPackage(J9VMThread * currentThread, J9Module * fromModule, const char *package)
#else /* J9VM_JAVA9_BUILD >= 156 */
trcModulesCreationPackage(J9VMThread * currentThread, J9Module * fromModule, j9object_t package)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	PORT_ACCESS_FROM_VMC(currentThread);
	J9InternalVMFunctions const * const vmFuncs = currentThread->javaVM->internalVMFunctions;
	char moduleNameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	char *moduleNameUTF = vmFuncs->copyStringToUTF8WithMemAlloc(
		currentThread, fromModule->moduleName, J9_STR_NONE, "", moduleNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
#if J9VM_JAVA9_BUILD < 156
	char packageNameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	char *packageNameUTF = vmFuncs->copyStringToUTF8WithMemAlloc(
		currentThread, package, J9_STR_NONE, "", packageNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
#endif /* J9VM_JAVA9_BUILD < 156 */

	if (NULL != moduleNameUTF) {
		if (0 == strcmp(moduleNameUTF, JAVA_BASE_MODULE)) {
#if J9VM_JAVA9_BUILD >= 156
			Trc_MODULE_creation_package(currentThread, package, "java.base");
#else
			if (NULL != packageNameUTF) {
				Trc_MODULE_creation_package(currentThread, packageNameUTF, "java.base");
			}
#endif /* J9VM_JAVA9_BUILD >= 156 */
		} else {
#if J9VM_JAVA9_BUILD >= 156
			Trc_MODULE_creation_package(currentThread, package, moduleNameUTF);
#else
			if (NULL != packageNameUTF) {
				Trc_MODULE_creation_package(currentThread, packageNameUTF, moduleNameUTF);
			}
#endif /* J9VM_JAVA9_BUILD >= 156 */
		}
		if (moduleNameBuf != moduleNameUTF) {
			j9mem_free_memory(moduleNameUTF);
		}
	}
#if J9VM_JAVA9_BUILD < 156
	if (packageNameBuf != packageNameUTF) {
		j9mem_free_memory(packageNameUTF);
	}
#endif /* J9VM_JAVA9_BUILD < 156 */
}

static BOOLEAN
#if J9VM_JAVA9_BUILD >= 156
addPackageDefinition(J9VMThread * currentThread, J9Module * fromModule, const char *package)
#else /* J9VM_JAVA9_BUILD >= 156 */
addPackageDefinition(J9VMThread * currentThread, J9Module * fromModule, j9object_t package)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	J9ClassLoader * const classLoader = fromModule->classLoader;

	BOOLEAN retval = FALSE;

	J9Package * j9package = createPackage(currentThread, fromModule, package);

	if (NULL != j9package) {
		retval = (0 == hashTableAtPut(classLoader->packageHashTable, (void*)&j9package, TRUE));
	}

	if (!retval) {
		freePackage(currentThread, j9package);
	} else {
		if (TrcEnabled_Trc_MODULE_creation_package) {
			trcModulesCreationPackage(currentThread, fromModule, package);
		}
	}

	return retval;
}

static void
#if J9VM_JAVA9_BUILD >= 156
removeMulPackageDefinitions(J9VMThread * currentThread, J9Module * fromModule, const char* const* packages, U_32 packagesIndex)
#else /* J9VM_JAVA9_BUILD >= 156 */
removeMulPackageDefinitions(J9VMThread * currentThread, J9Module * fromModule, jobjectArray packages, U_32 packagesIndex)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	BOOLEAN stopLoop = FALSE;
	U_32 i = packagesIndex;

	while (!stopLoop) {
#if J9VM_JAVA9_BUILD >= 156
		const char *packageName = packages[i];
#else /* J9VM_JAVA9_BUILD >= 156 */
		j9object_t packageName = J9JAVAARRAYOFOBJECT_LOAD(currentThread, J9_JNI_UNWRAP_REFERENCE(packages), i);
#endif /* J9VM_JAVA9_BUILD >= 156 */

		Assert_SC_true(removePackageDefinition(currentThread, fromModule, packageName));

		stopLoop = (0 == i);
		--i;
	}
}

static UDATA
#if J9VM_JAVA9_BUILD >= 156
addMulPackageDefinitions(J9VMThread * currentThread, J9Module * fromModule, const char* const* packages, U_32 numPackages)
#else /* J9VM_JAVA9_BUILD >= 156 */
addMulPackageDefinitions(J9VMThread * currentThread, J9Module * fromModule, jobjectArray packages)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	UDATA retval = ERRCODE_SUCCESS;

	if (NULL != packages) {
#if J9VM_JAVA9_BUILD >= 156
		U_32 const arrayLength = numPackages;
#else /* J9VM_JAVA9_BUILD >= 156 */
		j9object_t packageArray = J9_JNI_UNWRAP_REFERENCE(packages);
		U_32 const arrayLength = J9INDEXABLEOBJECT_SIZE(currentThread, packageArray);
#endif /* J9VM_JAVA9_BUILD >= 156 */
		if (0 != arrayLength) {
			U_32 i = 0;

			for (i = 0; i < arrayLength; i++) {
#if J9VM_JAVA9_BUILD >= 156
				const char *packageName = packages[i];
#else /* J9VM_JAVA9_BUILD >= 156 */
				j9object_t packageName = J9JAVAARRAYOFOBJECT_LOAD(currentThread, packageArray, i);
#endif /* J9VM_JAVA9_BUILD >= 156 */
				if (!addPackageDefinition(currentThread, fromModule, packageName)) {
					J9ClassLoader * const classLoader = fromModule->classLoader;

					if (isPackageDefined(currentThread, classLoader, packageName)) {
						retval = ERRCODE_DUPLICATE_PACKAGE_IN_LIST;
					}
					break;
				}
			}

			/* Remove from the hash table the entries that made through. Note that the last entry (the one we are 
			 * processing right now) was the one that failed so we don't need to worry about that one.
			 */
			if (ERRCODE_SUCCESS != retval) {
				if (i > 0) {
					--i; 
					removeMulPackageDefinitions(currentThread, fromModule, packages, i);
				}
			}
		}
	}

	return retval;
}

static UDATA
#if J9VM_JAVA9_BUILD >= 156
addModuleDefinition(J9VMThread * currentThread, J9Module * fromModule, const char* const* packages, U_32 numPackages, jstring version)
#else /* J9VM_JAVA9_BUILD >= 156 */
addModuleDefinition(J9VMThread * currentThread, J9Module * fromModule, jobjectArray packages, jstring version)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	J9ClassLoader * const classLoader = fromModule->classLoader;

	UDATA retval = ERRCODE_GENERAL_FAILURE;
#if J9VM_JAVA9_BUILD >= 156
	if (!areNoPackagesDefined(currentThread, classLoader, packages, numPackages)) {
#else /* J9VM_JAVA9_BUILD >= 156 */
	if (!areNoPackagesDefined(currentThread, classLoader, packages)) {
#endif /* J9VM_JAVA9_BUILD >= 156 */
		retval = ERRCODE_PACKAGE_ALREADY_DEFINED;
	} else if (isModuleDefined(currentThread, fromModule)) {
		retval = ERRCODE_MODULE_ALREADY_DEFINED;
	} else {
#if J9VM_JAVA9_BUILD >= 156
		retval = addMulPackageDefinitions(currentThread, fromModule, packages, numPackages);
#else /* J9VM_JAVA9_BUILD >= 156 */
		retval = addMulPackageDefinitions(currentThread, fromModule, packages);
#endif /* J9VM_JAVA9_BUILD >= 156 */
		if (ERRCODE_SUCCESS == retval) {
			BOOLEAN const success = (0 == hashTableAtPut(classLoader->moduleHashTable, (void*)&fromModule, TRUE));
			if (NULL != version) {
				fromModule->version = J9_JNI_UNWRAP_REFERENCE(version);
			}
			if (!success) {
				/* If we failed to add the module to the hash table */
				if (NULL != packages) {
#if J9VM_JAVA9_BUILD >= 156
					removeMulPackageDefinitions(currentThread, fromModule, packages, numPackages);
#else /* J9VM_JAVA9_BUILD >= 156 */
					U_32 const arrayLength = J9INDEXABLEOBJECT_SIZE(currentThread, J9_JNI_UNWRAP_REFERENCE(packages));
					removeMulPackageDefinitions(currentThread, fromModule, packages, arrayLength);
#endif /* J9VM_JAVA9_BUILD >= 156 */
				}

				retval = ERRCODE_HASHTABLE_OPERATION_FAILED;
			}
		}
	}

	return retval;
}

static BOOLEAN
#if J9VM_JAVA9_BUILD >= 156
isPackageDefined(J9VMThread * currentThread, J9ClassLoader * classLoader, const char *packageName)
#else /* J9VM_JAVA9_BUILD >= 156 */
isPackageDefined(J9VMThread * currentThread, J9ClassLoader * classLoader, j9object_t packageName)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	J9Package const * target = NULL;

	target = hashPackageTableAt(currentThread, classLoader, packageName);

	return (NULL != target);
}

static BOOLEAN
#if J9VM_JAVA9_BUILD >= 156
areNoPackagesDefined(J9VMThread * currentThread, J9ClassLoader * classLoader, const char* const* packages, U_32 numPackages)
#else /* J9VM_JAVA9_BUILD >= 156 */
areNoPackagesDefined(J9VMThread * currentThread, J9ClassLoader * classLoader, jobjectArray packages)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	BOOLEAN success = TRUE;

	if (NULL != packages) {
#if J9VM_JAVA9_BUILD >= 156
		U_32 const arrayLength = numPackages;
#else /* J9VM_JAVA9_BUILD >= 156 */
		j9object_t packageArray = J9_JNI_UNWRAP_REFERENCE(packages);
		U_32 const arrayLength = J9INDEXABLEOBJECT_SIZE(currentThread, packageArray);
#endif /* J9VM_JAVA9_BUILD >= 156 */
		if (0 != arrayLength) {
			U_32 i = 0;
			for (i = 0; i < arrayLength; i++) {
#if J9VM_JAVA9_BUILD >= 156
				const char *packageName = packages[i];
#else /* J9VM_JAVA9_BUILD >= 156 */
				j9object_t packageName = J9JAVAARRAYOFOBJECT_LOAD(currentThread, packageArray, i);
#endif /* J9VM_JAVA9_BUILD >= 156 */
				if (isPackageDefined(currentThread, classLoader, packageName)) {
					success = FALSE;
					break;
				}
			}
		}
	}

	return success;
}

static void
#if J9VM_JAVA9_BUILD >= 156
trcModulesAddModuleExportsToAll(J9VMThread * currentThread, J9Module * fromModule, const char *package)
#else /* J9VM_JAVA9_BUILD >= 156 */
trcModulesAddModuleExportsToAll(J9VMThread * currentThread, J9Module * fromModule, jstring package)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	PORT_ACCESS_FROM_VMC(currentThread);
	J9InternalVMFunctions const * const vmFuncs = currentThread->javaVM->internalVMFunctions;
	char fromModuleNameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	char *fromModuleNameUTF = vmFuncs->copyStringToUTF8WithMemAlloc(
		currentThread, fromModule->moduleName, J9_STR_NONE, "", fromModuleNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
	if (NULL != fromModuleNameUTF) {
#if J9VM_JAVA9_BUILD >= 156
		Trc_MODULE_add_module_exports_to_all(currentThread, package, fromModuleNameUTF);
#else
		char packageNameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
		char *packageNameUTF = vmFuncs->copyStringToUTF8WithMemAlloc(
			currentThread, J9_JNI_UNWRAP_REFERENCE(package), J9_STR_NONE, "", packageNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
		if (NULL != packageNameUTF) {
			Trc_MODULE_add_module_exports_to_all(currentThread, packageNameUTF, fromModuleNameUTF);
			if (packageNameBuf != packageNameUTF) {
				j9mem_free_memory(packageNameUTF);
			}
		}
#endif /* J9VM_JAVA9_BUILD >= 156 */
		if (fromModuleNameBuf != fromModuleNameUTF) {
			j9mem_free_memory(fromModuleNameUTF);
		}
	}
}

static UDATA
#if J9VM_JAVA9_BUILD >= 156
exportPackageToAll(J9VMThread * currentThread, J9Module * fromModule, const char *package)
#else /* J9VM_JAVA9_BUILD >= 156 */
exportPackageToAll(J9VMThread * currentThread, J9Module * fromModule, jstring package)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	UDATA retval = ERRCODE_GENERAL_FAILURE;
#if J9VM_JAVA9_BUILD >= 156
	J9Package * const j9package = getPackageDefinition(currentThread, fromModule, package, &retval);
#else /* J9VM_JAVA9_BUILD >= 156 */
	J9Package * const j9package = getPackageDefinition(currentThread, fromModule, J9_JNI_UNWRAP_REFERENCE(package), &retval);
#endif /* J9VM_JAVA9_BUILD >= 156 */
	if (NULL != j9package) {
		j9package->exportToAll = TRUE;
		if (TrcEnabled_Trc_MODULE_add_module_exports_to_all) {
			trcModulesAddModuleExportsToAll(currentThread, fromModule, package);
		}
	}

	return retval;
}

static void
#if J9VM_JAVA9_BUILD >= 156
trcModulesAddModuleExportsToAllUnnamed(J9VMThread * currentThread, J9Module * fromModule, const char *package)
#else /* J9VM_JAVA9_BUILD >= 156 */
trcModulesAddModuleExportsToAllUnnamed(J9VMThread * currentThread, J9Module * fromModule, jstring package)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	PORT_ACCESS_FROM_VMC(currentThread);
	J9InternalVMFunctions const * const vmFuncs = currentThread->javaVM->internalVMFunctions;
	char fromModuleNameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	char *fromModuleNameUTF = vmFuncs->copyStringToUTF8WithMemAlloc(
		currentThread, fromModule->moduleName, J9_STR_NONE, "", fromModuleNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
	if (NULL != fromModuleNameUTF) {
#if J9VM_JAVA9_BUILD >= 156
		Trc_MODULE_add_module_exports_to_all_unnamed(currentThread, package, fromModuleNameUTF);
#else
		char packageNameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
		char *packageNameUTF = vmFuncs->copyStringToUTF8WithMemAlloc(
			currentThread, J9_JNI_UNWRAP_REFERENCE(package), J9_STR_NONE, "", packageNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
		if (NULL != packageNameUTF) {
			Trc_MODULE_add_module_exports_to_all_unnamed(currentThread, packageNameUTF, fromModuleNameUTF);
			if (packageNameBuf != packageNameUTF) {
				j9mem_free_memory(packageNameUTF);
			}
		}
#endif /* J9VM_JAVA9_BUILD >= 156 */
		if (fromModuleNameBuf != fromModuleNameUTF) {
			j9mem_free_memory(fromModuleNameUTF);
		}
	}
}

static UDATA
#if J9VM_JAVA9_BUILD >= 156
exportPackageToAllUnamed(J9VMThread * currentThread, J9Module * fromModule, const char *package)
#else /* J9VM_JAVA9_BUILD >= 156 */
exportPackageToAllUnamed(J9VMThread * currentThread, J9Module * fromModule, jstring package)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	UDATA retval = ERRCODE_GENERAL_FAILURE;
#if J9VM_JAVA9_BUILD >= 156
	J9Package * const j9package = getPackageDefinition(currentThread, fromModule, package, &retval);
#else /* J9VM_JAVA9_BUILD >= 156 */
	J9Package * const j9package = getPackageDefinition(currentThread, fromModule, J9_JNI_UNWRAP_REFERENCE(package), &retval);
#endif /* J9VM_JAVA9_BUILD >= 156 */
	if (NULL != j9package) {
		j9package->exportToAllUnnamed = TRUE;
		if (TrcEnabled_Trc_MODULE_add_module_exports_to_all_unnamed) {
			trcModulesAddModuleExportsToAllUnnamed(currentThread, fromModule, package);
		}
	}

	return retval;
}

/** @return the J9Module associated with a Module object */
static J9Module *
getJ9Module(J9VMThread * currentThread, jobject module)
{
	J9JavaVM const * const vm = currentThread->javaVM;

	j9object_t modObj = J9_JNI_UNWRAP_REFERENCE(module);

	/* Get J9Module* via the hidden field */
	return J9OBJECT_ADDRESS_LOAD(currentThread, modObj, vm->modulePointerOffset);
}

static BOOLEAN
isModuleJavaBase(j9object_t moduleName)
{
	/** @todo compare against string 'java.base' */
	return FALSE;
}

static BOOLEAN
isModuleNameGood(j9object_t moduleName)
{
	/** @todo implement this */
	return TRUE;
}

static BOOLEAN
isModuleNameValid(j9object_t moduleName)
{
	BOOLEAN retval = FALSE;

	if (NULL != moduleName) {
		if (!isModuleJavaBase(moduleName)) {
			retval = isModuleNameGood(moduleName);
		}
	}

	return retval;
}

static void
#if J9VM_JAVA9_BUILD >= 156
trcModulesAddModuleExports(J9VMThread *currentThread, J9Module *fromModule, const char *package, J9Module *toModule)
#else /* J9VM_JAVA9_BUILD >= 156 */
trcModulesAddModuleExports(J9VMThread *currentThread, J9Module *fromModule, jstring package, J9Module *toModule)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	PORT_ACCESS_FROM_VMC(currentThread);
	J9InternalVMFunctions const * const vmFuncs = currentThread->javaVM->internalVMFunctions;
	char fromModuleNameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	char toModuleNameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	char *fromModuleNameUTF = vmFuncs->copyStringToUTF8WithMemAlloc(
		currentThread, fromModule->moduleName, J9_STR_NONE, "", fromModuleNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
	char *toModuleNameUTF = vmFuncs->copyStringToUTF8WithMemAlloc(
		currentThread, toModule->moduleName, J9_STR_NONE, "", toModuleNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
	if ((NULL != fromModuleNameUTF) && (NULL != toModuleNameUTF)) {
#if J9VM_JAVA9_BUILD >= 156
		Trc_MODULE_add_module_exports(currentThread, package, fromModuleNameUTF, toModuleNameUTF);
#else
		char packageNameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
		char *packageNameUTF = vmFuncs->copyStringToUTF8WithMemAlloc(
			currentThread, J9_JNI_UNWRAP_REFERENCE(package), J9_STR_NONE, "", packageNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
		if (NULL != packageNameUTF) {
			Trc_MODULE_add_module_exports(currentThread, packageNameUTF, fromModuleNameUTF, toModuleNameUTF);
			if (packageNameBuf != packageNameUTF) {
				j9mem_free_memory(packageNameUTF);
			}
		}
#endif /* J9VM_JAVA9_BUILD >= 156 */
	}
	if (fromModuleNameBuf != fromModuleNameUTF) {
		j9mem_free_memory(fromModuleNameUTF);
	}
	if (toModuleNameBuf != toModuleNameUTF) {
		j9mem_free_memory(toModuleNameUTF);
	}
}

static UDATA
#if J9VM_JAVA9_BUILD >= 156
exportPackageToModule(J9VMThread * currentThread, J9Module * fromModule, const char *package, J9Module * toModule)
#else /* J9VM_JAVA9_BUILD >= 156 */
exportPackageToModule(J9VMThread * currentThread, J9Module * fromModule, jstring package, J9Module * toModule)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	UDATA retval = ERRCODE_GENERAL_FAILURE;
#if J9VM_JAVA9_BUILD >= 156
	J9Package * const j9package = getPackageDefinition(currentThread, fromModule, package, &retval);
#else /* J9VM_JAVA9_BUILD >= 156 */
	J9Package * const j9package = getPackageDefinition(currentThread, fromModule, J9_JNI_UNWRAP_REFERENCE(package), &retval);
#endif /* J9VM_JAVA9_BUILD >= 156 */
	if (NULL != j9package) {
		if (isModuleDefined(currentThread, toModule)) {
			if (0 == hashTableAtPut(j9package->exportsHashTable, (void*)&toModule, FALSE)) {
				retval = ERRCODE_SUCCESS;
				/*
				 * Need to keep track of package that is exported to toModule in case toModule gets unloaded
				 * before fromModule. An unloaded module in packageHashtable will result in a crash when doing hashtable lookups.
				 * We use this hashtable to remove instances of toModule in packages when unloading toModule.
				 * We only need to worry about modules in different layers as modules in the same layer are unloaded
				 * at the same time.
				 */
				if (NULL == toModule->removeExportsHashTable) {
					J9JavaVM *vm = currentThread->javaVM;
					toModule->removeExportsHashTable = vm->internalVMFunctions->hashPackageTableNew(vm, INITIAL_INTERNAL_PACKAGE_HASHTABLE_SIZE);
				}
				if (NULL != toModule->removeExportsHashTable) {
					if (0 != hashTableAtPut(toModule->removeExportsHashTable, (void*)&j9package, FALSE)) {
						retval = ERRCODE_HASHTABLE_OPERATION_FAILED;
					}
				} else {
					retval = ERRCODE_HASHTABLE_OPERATION_FAILED;
				}
			} else {
				retval = ERRCODE_HASHTABLE_OPERATION_FAILED;
			}
		} else {
			retval = ERRCODE_MODULE_WASNT_FOUND;
		}
	}
	if (ERRCODE_SUCCESS == retval && TrcEnabled_Trc_MODULE_add_module_exports) {
		trcModulesAddModuleExports(currentThread, fromModule, package, toModule);
	}

	return retval;
}

static UDATA
allowReadAccessToModule(J9VMThread * currentThread, J9Module * fromModule, J9Module * toModule)
{
	UDATA retval = ERRCODE_MODULE_WASNT_FOUND;

	if (isModuleDefined(currentThread, fromModule)) {
		J9JavaVM *vm = currentThread->javaVM;

		if (J9_IS_J9MODULE_UNNAMED(vm, toModule)) {
			fromModule->isLoose = TRUE;
			retval = ERRCODE_SUCCESS;
		} else if (isModuleDefined(currentThread, toModule)) {
			BOOLEAN success = FALSE;
			if (0 == hashTableAtPut(toModule->readAccessHashTable, (void*)&fromModule, FALSE)) {
				success = TRUE;
				/*
				 * Need to keep track of toModule that can read fromModule in case fromModule gets unloaded
				 * before toModule. An unloaded module in toModule will result in a crash when doing hashtable lookups.
				 * We use removeAccessHashTable to remove instances of fromModule in toModules when unloading fromModule.
				 * We only need to worry about modules in different layers as modules in the same layer are unloaded
				 * at the same time.
				 */
				if (NULL == fromModule->removeAccessHashTable) {
					fromModule->removeAccessHashTable = vm->internalVMFunctions->hashModuleTableNew(vm, INITIAL_INTERNAL_MODULE_HASHTABLE_SIZE);
				}
				if (NULL != fromModule->removeAccessHashTable) {
					if (0 != hashTableAtPut(fromModule->removeAccessHashTable, (void*)&toModule, FALSE)) {
						success = FALSE;
					}
				} else {
					retval = ERRCODE_HASHTABLE_OPERATION_FAILED;
				}
			}

			if (success) {
				retval = ERRCODE_SUCCESS;
			} else {

				retval = ERRCODE_HASHTABLE_OPERATION_FAILED;
			}
		}
	}

	return retval;
}

/**
 * Define a module containing the specified packages. It will create the module record in the  ClassLoader's module hash table and
 * create package records in the class loader's package hash table if necessary.
 *  
 * @throws NullPointerExceptions a if module is null.
 * @throws IllegalArgumentExceptions if
 *     - Class loader already has a module with that name
 *     - Class loader has already defined types for any of the module's packages
 *     - Module_name is 'java.base'
 *     - Module_name is syntactically bad
 *     - Packages contains an illegal package name
 *     - Packages contains a duplicate package name
 *     - A package already exists in another module for this class loader
 *     - Class loader is not a subclass of java.lang.ClassLoader
 *     - Module is an unnamed module
 *
 * @return If successful, returns a java.lang.reflect.Module object. Otherwise, returns NULL.
 */
jobject JNICALL
#if J9VM_JAVA9_BUILD >= 156
JVM_DefineModule(JNIEnv * env, jobject module, jboolean isOpen, jstring version, jstring location, const char* const* packages, jsize numPackages)
#elif J9VM_JAVA9_BUILD >= 148 /* J9VM_JAVA9_BUILD >= 156 */
JVM_DefineModule(JNIEnv * env, jobject module, jboolean isOpen, jstring version, jstring location, jobjectArray packages)
#else /* J9VM_JAVA9_BUILD >= 148 */
JVM_DefineModule(JNIEnv * env, jobject module, jstring version, jstring location, jobjectArray packages)
#endif /* J9VM_JAVA9_BUILD >= 148 */
{
	J9VMThread * const currentThread = (J9VMThread*)env;
	J9JavaVM * vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);
#if defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
	omrthread_monitor_enter(vm->classLoaderModuleAndLocationMutex);
#else
	f_monitorEnter(vm->classLoaderModuleAndLocationMutex);
#endif /* defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */
	if (NULL == module) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, "module is null");
	} else {
		j9object_t modObj = J9_JNI_UNWRAP_REFERENCE(module);

		J9ClassLoader * const classLoader = getModuleObjectClassLoader(currentThread, modObj);
		j9object_t moduleName = getModuleObjectName(currentThread, modObj);

		if (NULL == moduleName) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, "module is unnamed");
		} else if (!isModuleNameValid(moduleName)) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, "module name is invalid");
		} else if (NULL != classLoader) {
			BOOLEAN success = FALSE;
			J9Module *j9mod = createModule(currentThread, modObj, classLoader, moduleName);
			UDATA rc = ERRCODE_GENERAL_FAILURE;

			if (NULL != j9mod) {
#if J9VM_JAVA9_BUILD >= 156
				rc = addModuleDefinition(currentThread, j9mod, packages, (U_32) numPackages, version);
#else /* J9VM_JAVA9_BUILD >= 156 */
				rc = addModuleDefinition(currentThread, j9mod, packages, version);
#endif /* J9VM_JAVA9_BUILD >= 156 */

#if J9VM_JAVA9_BUILD >= 148
				j9mod->isOpen = isOpen;
#else /* J9VM_JAVA9_BUILD >= 148 */
				j9mod->isOpen = FALSE;
#endif /* J9VM_JAVA9_BUILD >= 148 */

				success = (ERRCODE_SUCCESS == rc);

				if (!success) {
					vmFuncs->freeJ9Module(vm, j9mod);
					throwExceptionHelper(currentThread, rc);
				} else {
					PORT_ACCESS_FROM_VMC(currentThread);
					char buf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
					char *nameUTF = buf;

					nameUTF = vmFuncs->copyStringToUTF8WithMemAlloc(
						currentThread, moduleName, J9_STR_NONE, "", buf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
					if (NULL == nameUTF) {
						success = FALSE;
						goto nativeOOM;
					}

					/* For "java.base" module setting of jrt URL and patch paths is already done during startup. Avoid doing it here. */
					if (J9_ARE_ALL_BITS_SET(vm->runtimeFlags, J9_RUNTIME_JAVA_BASE_MODULE_CREATED)) {
						Trc_MODULE_definition(currentThread, nameUTF);
						if (classLoader == vm->systemClassLoader) {
							success = vmFuncs->setBootLoaderModulePatchPaths(vm, j9mod, (const char *)nameUTF);
							if (FALSE == success) {
								goto nativeOOM;
							} else {
								const char* moduleName = "openj9.sharedclasses";

								if (0 == strcmp(nameUTF, moduleName)) {
									J9VMDllLoadInfo *entry = FIND_DLL_TABLE_ENTRY(J9_SHARED_DLL_NAME);

									if ((NULL == entry)
										|| (J9_ARE_ALL_BITS_SET(entry->loadFlags, FAILED_TO_LOAD))
									) {
										j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_VM_FAILED_TO_LOAD_MODULE_REQUIRED_DLL, J9_SHARED_DLL_NAME, moduleName);
									}
								}
							}
						}
					} else {
						/* first module; must be "java.base" */
						J9ClassWalkState classWalkState;
						J9Class* clazz = NULL;

						Assert_SC_true(0 == strcmp(nameUTF, JAVA_BASE_MODULE));

						clazz = vmFuncs->allClassesStartDo(&classWalkState, vm, vm->systemClassLoader);
						while (NULL != clazz) {
							J9VMJAVALANGCLASS_SET_MODULE(currentThread, clazz->classObject, modObj);
							clazz = vmFuncs->allClassesNextDo(&classWalkState);
						}
						vmFuncs->allClassesEndDo(&classWalkState);
						vm->runtimeFlags |= J9_RUNTIME_JAVA_BASE_MODULE_CREATED;
						TRIGGER_J9HOOK_JAVA_BASE_LOADED(vm->hookInterface, currentThread);
						Trc_MODULE_definition(currentThread, "java.base");
					}
nativeOOM:
					if (nameUTF != buf) {
						j9mem_free_memory(nameUTF);
					}
					if (FALSE == success) {
						vmFuncs->freeJ9Module(vm, j9mod);
						vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
					}
				}
			}
		} else {
			/* An exception should be pending if classLoader is null */
			assert(NULL != currentThread->currentException);
		}
	}
#if defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
	omrthread_monitor_exit(vm->classLoaderModuleAndLocationMutex);
#else
	f_monitorExit(vm->classLoaderModuleAndLocationMutex);
#endif /* defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */
	vmFuncs->internalExitVMToJNI(currentThread);

	return module;
}

/**
 * Qualified export of package in fromModule to toModule. 
 *
 * @todo the null toModule case is not outlined in the spec but the spec does not specify what to do in this case
 * @throws NullPointerExceptions a if toModule is null.
 * @throws IllegalArgumentExceptions if
 * 1) Module fromModule does not exist
 * 2) Module toModule is not null and does not exist
 * 3) Package is not syntactically correct
 * 4) Package is not defined for fromModule's class loader
 * 5) Package is not in module fromModule.
 */
void JNICALL
#if J9VM_JAVA9_BUILD >= 156
JVM_AddModuleExports(JNIEnv * env, jobject fromModule, const char *package, jobject toModule)
#else /* J9VM_JAVA9_BUILD >= 156 */
JVM_AddModuleExports(JNIEnv * env, jobject fromModule, jstring package, jobject toModule)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	J9VMThread * const currentThread = (J9VMThread*)env;
	J9JavaVM const * const vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);
#if defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
	omrthread_monitor_enter(vm->classLoaderModuleAndLocationMutex);
#else
	f_monitorEnter(vm->classLoaderModuleAndLocationMutex);
#endif /* defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */
	if (NULL == toModule) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, "module is null");
	} else {
		UDATA rc = ERRCODE_GENERAL_FAILURE;
		J9Module * const j9FromMod = getJ9Module(currentThread, fromModule);
		J9Module * const j9ToMod = getJ9Module(currentThread, toModule);

		if (isModuleUnnamed(currentThread, J9_JNI_UNWRAP_REFERENCE(toModule))) {
			rc = exportPackageToAllUnamed(currentThread, j9FromMod, package);
		} else {
			rc = exportPackageToModule(currentThread, j9FromMod, package, j9ToMod);
		}

		if (ERRCODE_SUCCESS != rc) {
			throwExceptionHelper(currentThread, rc);
		}
	}
#if defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
	omrthread_monitor_exit(vm->classLoaderModuleAndLocationMutex);
#else
	f_monitorExit(vm->classLoaderModuleAndLocationMutex);
#endif /* defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */
	vmFuncs->internalExitVMToJNI(currentThread);
}

/**
 * Unqualified export of package in fromModule
 *      
 * @throws IllegalArgumentExceptions if
 * 1) Module fromModule does not exist
 * 2) Package is not syntactically correct
 * 3) Package is not defined for fromModule's class loader
 * 4) Package is not in module fromModule.
 */
void JNICALL
#if J9VM_JAVA9_BUILD >= 156
JVM_AddModuleExportsToAll(JNIEnv * env, jobject fromModule, const char *package)
#else /* J9VM_JAVA9_BUILD >= 156 */
JVM_AddModuleExportsToAll(JNIEnv * env, jobject fromModule, jstring package)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	J9VMThread * const currentThread = (J9VMThread*)env;
	J9JavaVM const * const vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);
#if defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
	omrthread_monitor_enter(vm->classLoaderModuleAndLocationMutex);
#else
	f_monitorEnter(vm->classLoaderModuleAndLocationMutex);
#endif /* defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */
	{
		UDATA rc = ERRCODE_GENERAL_FAILURE;

		J9Module * const j9FromMod = getJ9Module(currentThread, fromModule);

		rc = exportPackageToAll(currentThread, j9FromMod, package);

		if (ERRCODE_SUCCESS != rc) {
			throwExceptionHelper(currentThread, rc);
		}
	}
#if defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
	omrthread_monitor_exit(vm->classLoaderModuleAndLocationMutex);
#else
	f_monitorExit(vm->classLoaderModuleAndLocationMutex);
#endif /* defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */
	vmFuncs->internalExitVMToJNI(currentThread);
}

static void
trcModulesAddReadsModule(J9VMThread *currentThread, jobject toModule, J9Module *j9FromMod, J9Module *j9ToMod)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	J9InternalVMFunctions const * const vmFuncs = currentThread->javaVM->internalVMFunctions;
	char fromModuleNameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	char toModuleNameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	char *fromModuleNameUTF = vmFuncs->copyStringToUTF8WithMemAlloc(
		currentThread, j9FromMod->moduleName, J9_STR_NONE, "", fromModuleNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
	char *toModuleNameUTF = NULL;

	if (NULL != toModule) {
		toModuleNameUTF = vmFuncs->copyStringToUTF8WithMemAlloc(
			currentThread, j9ToMod->moduleName, J9_STR_NONE, "", toModuleNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
	} else {
#define LOOSE_MODULE   "loose "
		PORT_ACCESS_FROM_VMC(currentThread);
		Assert_SC_true(J9VM_PACKAGE_NAME_BUFFER_LENGTH > sizeof(LOOSE_MODULE));
		memcpy(toModuleNameBuf, LOOSE_MODULE, sizeof(LOOSE_MODULE));
		toModuleNameUTF = toModuleNameBuf;
#undef	LOOSE_MODULE
	}
	if ((NULL != fromModuleNameUTF) && (NULL != toModuleNameUTF)) {
		Trc_MODULE_add_reads_module(currentThread, fromModuleNameUTF, toModuleNameUTF);
	}
	if (fromModuleNameBuf != fromModuleNameUTF) {
		j9mem_free_memory(fromModuleNameUTF);
	}
	if (toModuleNameBuf != toModuleNameUTF) {
		j9mem_free_memory(toModuleNameUTF);
	}
}

/**
 * Add toModule to the list of modules that fromModule can read. If fromModule is the same as toModule, do nothing.
 * If toModule is null then fromModule is marked as a loose module (i.e., fromModule can read all current and future unnamed modules).
 *
 * @throws IllegalArgumentExceptions if
 * 1) if fromModule is null or if modules do not exist.
 */
void JNICALL
JVM_AddReadsModule(JNIEnv * env, jobject fromModule, jobject toModule)
{
	if (fromModule != toModule) {
		J9VMThread * const currentThread = (J9VMThread*)env;
		J9JavaVM const * const vm = currentThread->javaVM;
		J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;

		vmFuncs->internalEnterVMFromJNI(currentThread);
#if defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
		omrthread_monitor_enter(vm->classLoaderModuleAndLocationMutex);
#else
		f_monitorEnter(vm->classLoaderModuleAndLocationMutex);
#endif /* defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */
		{
			UDATA rc = ERRCODE_GENERAL_FAILURE;

			J9Module * const j9FromMod = getJ9Module(currentThread, fromModule);
			J9Module * const j9ToMod = (NULL != toModule) ? getJ9Module(currentThread, toModule) : NULL;

			/* Slightly different then check above since above I was dealing with the stack addr */
			if (j9FromMod != j9ToMod) {
				rc = allowReadAccessToModule(currentThread, j9FromMod, j9ToMod);

				if (ERRCODE_SUCCESS != rc) {
					throwExceptionHelper(currentThread, rc);
				} else {
					if (TrcEnabled_Trc_MODULE_add_reads_module) {
						trcModulesAddReadsModule(currentThread, toModule, j9FromMod, j9ToMod);
					}
				}
			}
		}
#if defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
		omrthread_monitor_exit(vm->classLoaderModuleAndLocationMutex);
#else
		f_monitorExit(vm->classLoaderModuleAndLocationMutex);
#endif /* defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */
		vmFuncs->internalExitVMToJNI(currentThread);
	}
}

/**
 * @return TRUE if:
 * 1. askModule can read srcModule or 
 * 2. if both are the same module or 
 * 3. if askModule is loose and srcModule is null. 
 * FALSE otherwise
 *   
 * @throws IllegalArgumentExceptions if
 * 1) either askModule or srcModule is not a java.lang.reflect.Module
 */
jboolean JNICALL
JVM_CanReadModule(JNIEnv * env, jobject askModule, jobject srcModule)
{
	J9VMThread * const currentThread = (J9VMThread*)env;
	J9JavaVM const * const vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;
	BOOLEAN canRead = FALSE;

	if (askModule == srcModule) {
		canRead = TRUE;
	} else {
		vmFuncs->internalEnterVMFromJNI(currentThread);
#if defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
		omrthread_monitor_enter(vm->classLoaderModuleAndLocationMutex);
#else
		f_monitorEnter(vm->classLoaderModuleAndLocationMutex);
#endif /* defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */
		{
			UDATA rc = ERRCODE_GENERAL_FAILURE;

			J9Module * const j9FromMod = getJ9Module(currentThread, askModule);
			J9Module * const j9ToMod = getJ9Module(currentThread, srcModule);

			canRead = isAllowedReadAccessToModule(currentThread, j9FromMod, j9ToMod, &rc);

			if (ERRCODE_SUCCESS != rc) {
				throwExceptionHelper(currentThread, rc);
			}
		}
#if defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
		omrthread_monitor_exit(vm->classLoaderModuleAndLocationMutex);
#else
		f_monitorExit(vm->classLoaderModuleAndLocationMutex);
#endif /* defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */
		vmFuncs->internalExitVMToJNI(currentThread);
	}

	return (jboolean)canRead;
}

/**
 * Check whether a module is exporting a package to another module
 *      
 * @throws IllegalArgumentException is thrown if
 * 1. Either toModule or fromModule does not exist
 * 2. Package is syntactically incorrect
 * 3. Package is not in fromModule
 *
 * @param env Java env pointer
 * @param fromModule module that may have exported the package
 * @param package name of the package in question
 * @param toModule module for which package may have been exported to
 * 
 * @return If package is valid then this returns TRUE if fromModule exports 
 * package to toModule, if fromModule and toModule are the same module, or if 
 * package is exported without qualification.
 */
jboolean JNICALL
JVM_IsExportedToModule(JNIEnv * env, jobject fromModule, jstring package, jobject toModule)
{
	J9VMThread * const currentThread = (J9VMThread*)env;
	J9JavaVM const * const vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;
	BOOLEAN isExported = FALSE;

	vmFuncs->internalEnterVMFromJNI(currentThread);
#if defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
	omrthread_monitor_enter(vm->classLoaderModuleAndLocationMutex);
#else
	f_monitorEnter(vm->classLoaderModuleAndLocationMutex);
#endif /* defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */
	{
		UDATA rc = ERRCODE_GENERAL_FAILURE;

		J9Module * const j9FromMod = getJ9Module(currentThread, fromModule);
		J9Module * const j9ToMod = getJ9Module(currentThread, toModule);
		BOOLEAN const toUnnamed = isModuleUnnamed(currentThread, J9_JNI_UNWRAP_REFERENCE(toModule));
		j9object_t pkgObject = J9_JNI_UNWRAP_REFERENCE(package);
#if J9VM_JAVA9_BUILD >= 156
		PORT_ACCESS_FROM_VMC(currentThread);
		char buf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
		char *nameUTF = buf;
		nameUTF = vmFuncs->copyStringToUTF8WithMemAlloc(
			currentThread, pkgObject, J9_STR_NONE, "", buf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
		if (NULL == nameUTF) {
			vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
			goto done;
		}
		isExported = isPackageExportedToModule(currentThread, j9FromMod, nameUTF, j9ToMod, toUnnamed, &rc);
		if (nameUTF != buf) {
			j9mem_free_memory(nameUTF);
		}
#else /* J9VM_JAVA9_BUILD >= 156 */
		isExported = isPackageExportedToModule(currentThread, j9FromMod, pkgObject, j9ToMod, toUnnamed, &rc);
#endif /* J9VM_JAVA9_BUILD >= 156 */
		if (ERRCODE_SUCCESS != rc) {
			throwExceptionHelper(currentThread, rc);
		}
	}
#if J9VM_JAVA9_BUILD >= 156
done:
#endif
#if defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
	omrthread_monitor_exit(vm->classLoaderModuleAndLocationMutex);
#else
	f_monitorExit(vm->classLoaderModuleAndLocationMutex);
#endif /* defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */
	vmFuncs->internalExitVMToJNI(currentThread);

	return (jboolean)isExported;
}

static void
#if J9VM_JAVA9_BUILD >= 156
trcModulesAddModulePackage(J9VMThread *currentThread, J9Module *j9mod, const char *package)
#else /* J9VM_JAVA9_BUILD >= 156 */
trcModulesAddModulePackage(J9VMThread *currentThread, J9Module *j9mod, jstring package)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	PORT_ACCESS_FROM_VMC(currentThread);
	J9InternalVMFunctions const * const vmFuncs = currentThread->javaVM->internalVMFunctions;
	char moduleNameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	char *moduleNameUTF = vmFuncs->copyStringToUTF8WithMemAlloc(
		currentThread, j9mod->moduleName, J9_STR_NONE, "", moduleNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
	if (NULL != moduleNameUTF) {
#if J9VM_JAVA9_BUILD >= 156
		Trc_MODULE_add_module_package(currentThread, package, moduleNameUTF);
#else
		char packageNameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
		char *packageNameUTF = vmFuncs->copyStringToUTF8WithMemAlloc(
			currentThread, J9_JNI_UNWRAP_REFERENCE(package), J9_STR_NONE, "", packageNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
		if (NULL != packageNameUTF) {
			Trc_MODULE_add_module_package(currentThread, packageNameUTF, moduleNameUTF);
			if (packageNameBuf != packageNameUTF) {
				j9mem_free_memory(packageNameUTF);
			}
		}
#endif /* J9VM_JAVA9_BUILD >= 156 */
		if (moduleNameBuf != moduleNameUTF) {
			j9mem_free_memory(moduleNameUTF);
		}
	}
}
/**
 * Adds a package to a module
 *
 * @throws IllegalArgumentException is thrown if
 * 1) Module is bad
 * 2) Module is unnamed
 * 3) Package is not syntactically correct
 * 4) Package is already defined for module's class loader.
 */
void JNICALL
#if J9VM_JAVA9_BUILD >= 156
JVM_AddModulePackage(JNIEnv * env, jobject module, const char *package)
#else /* J9VM_JAVA9_BUILD >= 156 */
JVM_AddModulePackage(JNIEnv * env, jobject module, jstring package)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	J9VMThread * const currentThread = (J9VMThread*)env;
	J9JavaVM const * const vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);
#if defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
	omrthread_monitor_enter(vm->classLoaderModuleAndLocationMutex);
#else
	f_monitorEnter(vm->classLoaderModuleAndLocationMutex);
#endif /* defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */
	{
		UDATA rc = ERRCODE_GENERAL_FAILURE;

		J9Module * const j9mod = getJ9Module(currentThread, module);

#if J9VM_JAVA9_BUILD >= 156
		rc = addPackageDefinition(currentThread, j9mod, package);
#else /* J9VM_JAVA9_BUILD >= 156 */
		rc = addPackageDefinition(currentThread, j9mod, J9_JNI_UNWRAP_REFERENCE(package));
#endif /* J9VM_JAVA9_BUILD >= 156 */
		if (ERRCODE_SUCCESS != rc) {
			throwExceptionHelper(currentThread, rc);
		} else {
			if (TrcEnabled_Trc_MODULE_add_module_package) {
				trcModulesAddModulePackage(currentThread, j9mod, package);
			}
		}
	}
#if defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
	omrthread_monitor_exit(vm->classLoaderModuleAndLocationMutex);
#else
	f_monitorExit(vm->classLoaderModuleAndLocationMutex);
#endif /* defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */
	vmFuncs->internalExitVMToJNI(currentThread);
}

/**
 * Marks the specified package as exported to all unnamed modules. 
 *
 * @throws NullPointerExceptions if either module or package is null.
 * @throws IllegalArgumentExceptions if 
 * 1) module or package is bad or
 * 2) module is unnamed or
 * 3) package is not in module
 */
void JNICALL
#if J9VM_JAVA9_BUILD >= 156
JVM_AddModuleExportsToAllUnnamed(JNIEnv * env, jobject fromModule, const char *package)
#else /* J9VM_JAVA9_BUILD >= 156 */
JVM_AddModuleExportsToAllUnnamed(JNIEnv * env, jobject fromModule, jstring package)
#endif /* J9VM_JAVA9_BUILD >= 156 */
{
	J9VMThread * const currentThread = (J9VMThread*)env;
	J9JavaVM const * const vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);
#if defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
	omrthread_monitor_enter(vm->classLoaderModuleAndLocationMutex);
#else
	f_monitorEnter(vm->classLoaderModuleAndLocationMutex);
#endif /* defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */
	{
		UDATA rc = ERRCODE_GENERAL_FAILURE;

		J9Module * const j9FromMod = getJ9Module(currentThread, fromModule);

		rc = exportPackageToAllUnamed(currentThread, j9FromMod, package);

		if (ERRCODE_SUCCESS != rc) {
			throwExceptionHelper(currentThread, rc);
		}
	}
#if defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
	omrthread_monitor_exit(vm->classLoaderModuleAndLocationMutex);
#else
	f_monitorExit(vm->classLoaderModuleAndLocationMutex);
#endif /* defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */
	vmFuncs->internalExitVMToJNI(currentThread);
}

jstring JNICALL
JVM_GetSimpleBinaryName(JNIEnv *env, jclass arg1) {
	assert(!"JVM_GetSimpleBinaryName unimplemented"); /* Jazz 108925: Revive J9JCL raw pConfig build */
	return NULL;
}

void JNICALL
JVM_SetMethodInfo(JNIEnv *env, jobject arg1) {
	assert(!"JVM_SetMethodInfo unimplemented"); /* Jazz 108925: Revive J9JCL raw pConfig build */
}

jint JNICALL
JVM_ConstantPoolGetNameAndTypeRefIndexAt(JNIEnv *env, jobject arg1, jobject arg2, jint arg3) {
	assert(!"JVM_ConstantPoolGetNameAndTypeRefIndexAt unimplemented"); /* Jazz 108925: Revive J9JCL raw pConfig build */
    return -1;
}

jint JNICALL
JVM_MoreStackWalk(JNIEnv *env, jobject arg1, jlong arg2, jlong arg3, jint arg4, jint arg5, jobjectArray arg6, jobjectArray arg7) {
	assert(!"JVM_MoreStackWalk unimplemented"); /* Jazz 108925: Revive J9JCL raw pConfig build */
    return -1;
}

jint JNICALL
JVM_ConstantPoolGetClassRefIndexAt(JNIEnv *env, jobject arg1, jlong arg2, jint arg3) {
	assert(!"JVM_ConstantPoolGetClassRefIndexAt unimplemented"); /* Jazz 108925: Revive J9JCL raw pConfig build */
    return -1;
}

jobjectArray JNICALL
JVM_GetVmArguments(JNIEnv *env) {
	assert(!"JVM_GetVmArguments unimplemented"); /* Jazz 108925: Revive J9JCL raw pConfig build */
    return NULL;
}

void JNICALL
JVM_FillStackFrames(JNIEnv *env, jclass arg1, jint arg2, jobjectArray arg3, jint arg4, jint arg5) {
	assert(!"JVM_FillStackFrames unimplemented"); /* Jazz 108925: Revive J9JCL raw pConfig build */
}

jclass JNICALL
JVM_FindClassFromCaller(JNIEnv* env, const char* arg1, jboolean arg2, jobject arg3, jclass arg4) {
	assert(!"JVM_FindClassFromCaller unimplemented"); /* Jazz 108925: Revive J9JCL raw pConfig build */
    return NULL;
}

jobjectArray JNICALL
JVM_ConstantPoolGetNameAndTypeRefInfoAt(JNIEnv *env, jobject arg1, jobject arg2, jint arg3) {
    assert(!"JVM_ConstantPoolGetNameAndTypeRefInfoAt unimplemented"); /* Jazz 108925: Revive J9JCL raw pConfig build */
    return NULL;
}

jbyte JNICALL
JVM_ConstantPoolGetTagAt(JNIEnv *env, jobject arg1, jobject arg2, jint arg3) {
    assert(!"JVM_ConstantPoolGetTagAt unimplemented"); /* Jazz 108925: Revive J9JCL raw pConfig build */
    return 0;
}

jobject JNICALL
JVM_CallStackWalk(JNIEnv *env, jobject arg1, jlong arg2, jint arg3, jint arg4, jint arg5, jobjectArray arg6, jobjectArray arg7) {
    assert(!"JVM_CallStackWalk unimplemented"); /* Jazz 108925: Revive J9JCL raw pConfig build */
    return NULL;
}

JNIEXPORT jobject JNICALL
JVM_GetAndClearReferencePendingList(JNIEnv *env) {
	assert(!"JVM_GetAndClearReferencePendingList unimplemented"); /* Jazz 108925: Revive J9JCL raw pConfig build */
	return NULL;
}
JNIEXPORT jboolean JNICALL
JVM_HasReferencePendingList(JNIEnv *env) {
	assert(!"JVM_HasReferencePendingList unimplemented"); /* Jazz 108925: Revive J9JCL raw pConfig build */
	return JNI_FALSE;
}
JNIEXPORT void JNICALL
JVM_WaitForReferencePendingList(JNIEnv *env) {
	assert(!"JVM_WaitForReferencePendingList unimplemented"); /* Jazz 108925: Revive J9JCL raw pConfig build */
	return;
}

/**
 * Adds an unnamed module to the bootLoader
 *
 * @param module module
 *
 * @throws IllegalArgumentException is thrown if
 * 1) Module is named
 * 2) Module is not j.l.r.Module or subclass of
 * 3) Module is not loaded by the bootLoader
 *
 * @throws NullPointerException if module is NULL
 */
void JNICALL
JVM_SetBootLoaderUnnamedModule(JNIEnv *env, jobject module) {
	J9VMThread * const currentThread = (J9VMThread*)env;
	J9JavaVM * vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);
	if (module == NULL) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, "module is null");
	} else {
		j9object_t modObj = J9_JNI_UNWRAP_REFERENCE(module);
		J9ClassLoader *systemClassLoader = vm->systemClassLoader;
		J9Class *instanceClazz = J9OBJECT_CLAZZ(currentThread, modObj);
		J9Class *moduleClass = NULL;
		if (J2SE_SHAPE(vm) < J2SE_SHAPE_B165) {
			moduleClass = vmFuncs->internalFindKnownClass(currentThread,
				J9VMCONSTANTPOOL_JAVALANGREFLECTMODULE,
				J9_FINDKNOWNCLASS_FLAG_INITIALIZE);
		} else {
			moduleClass = vmFuncs->internalFindKnownClass(currentThread,
				J9VMCONSTANTPOOL_JAVALANGMODULE,
				J9_FINDKNOWNCLASS_FLAG_INITIALIZE);
		}
		if (!currentThread->currentException) {
			if (!isModuleUnnamed(currentThread, modObj)) {
				vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, "named module was supplied");
			} else if (!isSameOrSuperClassOf(moduleClass, instanceClazz)) {
				vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, "module supplied is not same or sub class of java/lang/Module");
			} else if (instanceClazz->classLoader != systemClassLoader) {
				vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, "module was not loaded by the bootclassloader");
			} else {
				if (NULL == J9VMJAVALANGCLASSLOADER_UNNAMEDMODULE(currentThread, systemClassLoader->classLoaderObject)) {
					J9Module *j9mod = createModule(currentThread, modObj, systemClassLoader, NULL /* NULL name field */);
					J9VMJAVALANGCLASSLOADER_SET_UNNAMEDMODULE(currentThread, systemClassLoader->classLoaderObject, modObj);
					Trc_MODULE_set_bootloader_unnamed_module(currentThread);
				} else {
					vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, "module is already set in the bootclassloader");
				}
			}
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);
}

void JNICALL
JVM_ToStackTraceElement(JNIEnv* env, jobject arg1, jobject arg2)
{
    assert(!"JVM_ToStackTraceElement unimplemented");
}

void JNICALL
JVM_GetStackTraceElements(JNIEnv *env, jobject throwable, jobjectArray elements)
{
    assert(!"JVM_GetStackTraceElements unimplemented");
}

void JNICALL
JVM_InitStackTraceElementArray(JNIEnv *env, jobjectArray elements, jobject throwable)
{
    assert(!"JVM_InitStackTraceElementArray unimplemented");
}

 void JNICALL
JVM_InitStackTraceElement(JNIEnv* env, jobject element, jobject stackFrameInfo)
 {
     assert(!"JVM_InitStackTraceElement unimplemented");
 }


/* JVM_GetModuleByPackageName is to be deleted for b156 */
/**
 * Returns the j.l.r.Module object for this classLoader and package. If the classLoader
 * has not loaded any classes in the package, method returns NULL. The package should contain /'s,
 * not .'s, ex: java/lang, not java.lang. Throws NPE if package is null. throws
 * IllegalArgumentException, if loader is not null and a subtype of j.l.Classloader.
 *
 * @param [in] env pointer to JNIEnv
 * @param [in] classLoader ClassLoader object
 * @param [in] packageName name of package
 *
 * @return a module, NULL on failure
 */
jobject JNICALL
JVM_GetModuleByPackageName(JNIEnv *env, jobject classLoader, jstring packageName)
{
	J9VMThread * const currentThread = (J9VMThread*)env;
	J9JavaVM * vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;
	jobject module = NULL;

	vmFuncs->internalEnterVMFromJNI(currentThread);
#if defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
	omrthread_monitor_enter(vm->classLoaderModuleAndLocationMutex);
#else
	f_monitorEnter(vm->classLoaderModuleAndLocationMutex);
#endif /* defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */
	if (NULL == packageName) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, "package name is null");
	} else {
		J9ClassLoader *thisVMClassLoader = NULL;
		J9UTF8 *packageUTF8 = NULL;
		char buf[J9VM_PACKAGE_NAME_BUFFER_LENGTH + sizeof(packageUTF8->length) + 1];
		j9object_t packageObj = J9_JNI_UNWRAP_REFERENCE(packageName);
		j9object_t moduleObj = NULL;
		J9Module *vmModule = NULL;
		PORT_ACCESS_FROM_VMC(currentThread);
		UDATA length = 0;

		if (NULL == classLoader) {
			thisVMClassLoader = vm->systemClassLoader;
		} else {
			j9object_t thisClassloader = NULL;
			J9Class *thisClassLoaderClass = NULL;
			J9Class *classLoaderClass = vmFuncs->internalFindKnownClass(currentThread,
																		J9VMCONSTANTPOOL_JAVALANGCLASSLOADER,
																		J9_FINDKNOWNCLASS_FLAG_INITIALIZE
																	   );
			thisClassloader = J9_JNI_UNWRAP_REFERENCE(classLoader);
			thisClassLoaderClass = J9OBJECT_CLAZZ(currentThread, thisClassloader);

			if (!isSameOrSuperClassOf(classLoaderClass, thisClassLoaderClass)) {
				vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, "classLoader is not same or subclass of java/lang/ClassLoader");
				goto exit;
			}

			thisVMClassLoader = J9VMJAVALANGCLASSLOADER_VMREF(currentThread, thisClassloader);
		}

		length = vmFuncs->getStringUTF8Length(currentThread, packageObj);
		packageUTF8 = (J9UTF8 *) buf;

		if (length > J9VM_PACKAGE_NAME_BUFFER_LENGTH) {
			packageUTF8 = j9mem_allocate_memory(length + sizeof(packageUTF8->length) + 1, OMRMEM_CATEGORY_VM);
			if (NULL == packageUTF8) {
				vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
				goto exit;
			}
		}
		vmFuncs->copyStringToUTF8Helper(currentThread, packageObj, TRUE, J9_STR_NONE, J9UTF8_DATA(packageUTF8), length + 1);
		J9UTF8_SET_LENGTH(packageUTF8, (U_16) length);
		if (NULL != strchr((const char*)J9UTF8_DATA(packageUTF8), '.')) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, "package name contains '.' instead of '/'");
		} else {
			if (vmFuncs->isAnyClassLoadedFromPackage(thisVMClassLoader, J9UTF8_DATA(packageUTF8), J9UTF8_LENGTH(packageUTF8))) {
				vmModule = vmFuncs->findModuleForPackageUTF8(currentThread, thisVMClassLoader, packageUTF8);
				if (NULL == vmModule) {
					moduleObj = J9VMJAVALANGCLASSLOADER_UNNAMEDMODULE(currentThread, thisVMClassLoader->classLoaderObject);
				} else {
					moduleObj = vmModule->moduleObject;
				}
				module = vmFuncs->j9jni_createLocalRef((JNIEnv *) currentThread, moduleObj);
				if (NULL == module) {
					vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
				}
			}
		}
		if (packageUTF8 != (J9UTF8 *) buf) {
			j9mem_free_memory(packageUTF8);
		}
	}
exit:
#if defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY)
	omrthread_monitor_exit(vm->classLoaderModuleAndLocationMutex);
#else
	f_monitorExit(vm->classLoaderModuleAndLocationMutex);
#endif /* defined(CALL_BUNDLED_FUNCTIONS_DIRECTLY) */
	vmFuncs->internalExitVMToJNI(currentThread);

	return module;
}


/**
 * Return the clock time in nanoseconds at given offset
 *
 * @param [in] env pointer to JNIEnv
 * @param [in] clazz Class object
 * @param [in] offsetSeconds offset in seconds
 *
 * @return nanoSeconds, -1 on failure
 */
jlong JNICALL
JVM_GetNanoTimeAdjustment(JNIEnv *env, jclass clazz, jlong offsetSeconds)
{
	PORT_ACCESS_FROM_ENV(env);
	jlong offsetNanoSeconds = 0;
	jlong currentTimeNano = 0;
	jlong result = -1;

	/* 2^63/10^9 is the largest number offsetSeconds can be such that multiplying it
	 * by J9TIME_NANOSECONDS_PER_SECOND (10^9) will not result in an overflow
	 */
	if ((offsetSeconds <= OFFSET_MAX) && (offsetSeconds >= OFFSET_MIN)) {
		UDATA success = 0;
		offsetNanoSeconds = offsetSeconds * J9TIME_NANOSECONDS_PER_SECOND;
		currentTimeNano = (jlong) j9time_current_time_nanos(&success);
		if (success) {
			if ((offsetNanoSeconds >= (currentTimeNano - TIME_LONG_MAX))
				&& (offsetNanoSeconds <= (currentTimeNano - TIME_LONG_MIN))
			) {
				result = currentTimeNano - offsetNanoSeconds;
			}
		}
	}
	
	return result;
}
