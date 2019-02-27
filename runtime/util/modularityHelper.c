/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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

#include "j2sever.h"
#include "j9protos.h"
#include "ut_j9util.h"
#include "j9java8packages.h"

static J9Package* hashPackageTableAtWithUTF8Name(J9VMThread *currentThread, J9ClassLoader *classLoader, J9UTF8 *packageName);
static BOOLEAN isPackageExportedToModuleHelper(J9VMThread *currentThread, J9Module *fromModule, J9Package *j9package, J9Module *toModule, BOOLEAN toUnnamed);

/* All the helper functions below assume that:
 * a) If VMAccess is required, it assumes the caller has already done so
 * b) If performing a hash operation, it assumes the caller has already locked vm->classLoaderModuleAndLocationMutex
 */

BOOLEAN
isModuleUnnamed(J9VMThread *currentThread, j9object_t moduleObject)
{
	return (NULL == J9VMJAVALANGMODULE_NAME(currentThread, moduleObject));
}

BOOLEAN
isModuleDefined(J9VMThread *currentThread, J9Module *fromModule)
{
	/* Needs to hold the classLoaderModuleAndLocationMutex when being called */
	BOOLEAN result = FALSE;

	if (!J9_IS_J9MODULE_UNNAMED(currentThread->javaVM, fromModule)) {
		result = (NULL != hashTableFind(fromModule->classLoader->moduleHashTable, &fromModule));
	}

	return result;
}

BOOLEAN
isAllowedReadAccessToModule(J9VMThread *currentThread, J9Module *fromModule, J9Module *toModule, UDATA *errCode)
{
	J9JavaVM * vm = currentThread->javaVM;
	BOOLEAN canRead = FALSE;
	if ((fromModule == toModule) /* modules are the same, no need to check */
		|| (toModule == vm->javaBaseModule) /* java.base is implicitly read by every module, no need to check */
		|| (J9_IS_J9MODULE_UNNAMED(vm, fromModule)) /* unnamed Modules can read all other modules */
	) {
		canRead = TRUE;
	} else if (isModuleDefined(currentThread, fromModule)) {
		*errCode = ERRCODE_SUCCESS;
		if (J9_IS_J9MODULE_UNNAMED(vm, toModule)) {
			canRead = fromModule->isLoose;
		} else {
			/** @todo We must make sure the class loader that owns toModule is still loaded. At this point here,
			 *  I would not get a request for a module whose class loader is no longer loaded but perhaps I should
			 *  be testing that assumption here.
			 */
			J9Module **targetPtr = NULL;
			Assert_Util_notNull(toModule->moduleName);

			targetPtr = hashTableFind(toModule->readAccessHashTable, &fromModule);
			if (NULL != targetPtr) {
				canRead = (fromModule == *targetPtr);
			}
		}
	} else {
		*errCode = ERRCODE_MODULE_WASNT_FOUND;
	}
	return canRead;
}

J9Package *
getPackageDefinition(J9VMThread *currentThread, J9Module *fromModule, const char *packageName, UDATA *errCode)
{
	J9Package *retval = NULL;
	if (isModuleDefined(currentThread, fromModule)) {
		retval = hashPackageTableAt(currentThread, fromModule->classLoader, packageName);
		if (NULL == retval) {
			*errCode = ERRCODE_PACKAGE_WASNT_FOUND;
		} else {
			*errCode = ERRCODE_SUCCESS;
		}
	} else {
		*errCode = ERRCODE_MODULE_WASNT_FOUND;
	}
	return retval;
}

J9Package*
hashPackageTableAt(J9VMThread *currentThread, J9ClassLoader *classLoader, const char *packageName)
{
	J9Package package = {0};
	J9Package *packagePtr = &package;
	J9Package **targetPtr = NULL;
	U_8 buf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];

	if (!addUTFNameToPackage(currentThread, packagePtr, packageName, buf, J9VM_PACKAGE_NAME_BUFFER_LENGTH)) {
		return NULL;
	}
	package.classLoader = classLoader;
	Assert_Util_notNull(packagePtr->packageName);
	targetPtr = hashTableFind(classLoader->packageHashTable, &packagePtr);
	if ((U_8*)package.packageName != (U_8*)buf) {
		PORT_ACCESS_FROM_VMC(currentThread);
		j9mem_free_memory(package.packageName);
	}
	return (NULL != targetPtr) ? *targetPtr : NULL;
}

BOOLEAN
addUTFNameToPackage(J9VMThread *currentThread, J9Package *j9package, const char *packageName, U_8 *buf, UDATA bufLen)
{
	J9JavaVM * const vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;
	UDATA packageNameLength = 0;

	PORT_ACCESS_FROM_JAVAVM(vm);
	j9package->packageName = (J9UTF8*)buf;
	packageNameLength = (UDATA) strlen(packageName);
	if ((NULL == j9package->packageName) || ((packageNameLength + sizeof(j9package->packageName->length) + 1) > bufLen)) {
		j9package->packageName = j9mem_allocate_memory(packageNameLength + sizeof(j9package->packageName->length) + 1, OMRMEM_CATEGORY_VM);
		if (NULL == j9package->packageName) {
			vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
			return FALSE;
		}
	}

	memcpy(J9UTF8_DATA(j9package->packageName), (void *)packageName, packageNameLength);
	J9UTF8_DATA(j9package->packageName)[packageNameLength] = '\0';
	J9UTF8_SET_LENGTH(j9package->packageName, (U_16)packageNameLength);

	return TRUE;
}

static J9Package*
hashPackageTableAtWithUTF8Name(J9VMThread *currentThread, J9ClassLoader *classLoader, J9UTF8 *packageName)
{
	J9Package package = {0};
	J9Package *packagePtr = &package;
	J9Package **targetPtr = NULL;

	packagePtr->packageName = packageName;
	packagePtr->classLoader = classLoader;
	Assert_Util_notNull(packagePtr->packageName);
	targetPtr = hashTableFind(classLoader->packageHashTable, &packagePtr);
	return (NULL != targetPtr) ? *targetPtr : NULL;
}

J9Package *
getPackageDefinitionWithName(J9VMThread *currentThread, J9Module *fromModule, U_8 *packageName, U_16 len, UDATA *errCode)
{
	J9Package *retval = NULL;
	char buf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	PORT_ACCESS_FROM_VMC(currentThread);
	J9UTF8 *packageNameBuf = (J9UTF8 *) buf;
	UDATA J9UTF8len = len + sizeof(packageNameBuf->length);

	if (J9VM_PACKAGE_NAME_BUFFER_LENGTH < J9UTF8len) {
		packageNameBuf = j9mem_allocate_memory(J9UTF8len, OMRMEM_CATEGORY_VM);
		if (NULL == packageNameBuf) {
			*errCode = ERRCODE_GENERAL_FAILURE;
			goto done;
		}
	}

	memcpy(J9UTF8_DATA(packageNameBuf), packageName, len);
	J9UTF8_SET_LENGTH(packageNameBuf, len);

	if (isModuleDefined(currentThread, fromModule)) {
		retval = hashPackageTableAtWithUTF8Name(currentThread, fromModule->classLoader, packageNameBuf);
		if (NULL == retval) {
			*errCode = ERRCODE_PACKAGE_WASNT_FOUND;
		} else {
			*errCode = ERRCODE_SUCCESS;
		}
	} else {
		*errCode = ERRCODE_MODULE_WASNT_FOUND;
	}

	if ((J9UTF8 *)buf != packageNameBuf) {
		j9mem_free_memory(packageNameBuf);
	}

done:
	return retval;
}

static BOOLEAN
isPackageExportedToModuleHelper(J9VMThread *currentThread, J9Module *fromModule, J9Package *j9package, J9Module *toModule, BOOLEAN toUnnamed)
{
	J9JavaVM *vm = currentThread->javaVM;
	BOOLEAN isExported = FALSE;

	if (J9_IS_J9MODULE_UNNAMED(vm, fromModule) || J9_IS_J9MODULE_OPEN(fromModule)) {
		/* unnamed & open modules export all packages */
		isExported = TRUE;
	} else if (NULL != j9package) {
		/* First try the general export rules */
		BOOLEAN const isExportedAll = j9package->exportToAll || (toUnnamed ? j9package->exportToAllUnnamed : FALSE);
		/* ... then look for an specific export rule */
		if (isExportedAll) {
			isExported = TRUE;
		} else if (!toUnnamed) {
			J9Module **targetPtr = NULL;

			Assert_Util_notNull(toModule->moduleName);
			targetPtr = hashTableFind(j9package->exportsHashTable, &toModule);
			if (NULL != targetPtr) {
				/** @todo We must make sure the class loader that owns toModule is still loaded. At this point here,
				 *  I would not get a request for a module whose class loader is no longer loaded but perhaps I should
				 *  be testing that assumption here.
				 */
				isExported = (*targetPtr == toModule);
			}
		} else if (J9_ARE_NO_BITS_SET(vm->runtimeFlags, J9_RUNTIME_DENY_ILLEGAL_ACCESS)) {
			/* in Java9 --illegal-access=permit is turned on by default. This opens
			 * each package (that existed in java8) to all-unnamed modules unless
			 * illegal-access=deny is specified.
			 */
			J9UTF8 *pkgName = j9package->packageName;
			if (NULL != lookupJava8Package((const char*) J9UTF8_DATA(pkgName), J9UTF8_LENGTH(pkgName))) {
				isExported = TRUE;
			}
		}
	}
	return isExported;
}

BOOLEAN
isPackageExportedToModuleWithName(J9VMThread *currentThread, J9Module *fromModule, U_8 *packageName, U_16 len, J9Module *toModule, BOOLEAN toUnnamed, UDATA *errCode)
{
	return isPackageExportedToModuleHelper(currentThread, fromModule, getPackageDefinitionWithName(currentThread, fromModule, packageName, len, errCode), toModule, toUnnamed);
}
