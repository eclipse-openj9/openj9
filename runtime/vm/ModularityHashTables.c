/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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
#include "j9protos.h"
#include "ut_j9vm.h"

static j9object_t moduleHashGetName(const void *entry);
static UDATA moduleNameHashFn(void *key, void *userData);
static UDATA moduleNameHashEqualFn(void *leftKey, void *rightKey, void *userData);
static UDATA modulePointerHashFn(void *key, void *userData);
static UDATA modulePointerHashEqualFn(void *leftKey, void *rightKey, void *userData);
static UDATA packageHashFn(void *key, void *userData);
static UDATA packageHashEqualFn(void *leftKey, void *rightKey, void *userData);
static UDATA moduleExtraInfoHashFn(void *key, void *userData);
static UDATA moduleExtraInfoHashEqualFn(void *tableNode, void *queryNode, void *userData);

static j9object_t
moduleHashGetName(const void *entry)
{
	const J9Module** const modulePtr = (const J9Module**)entry;
	const J9Module* const module = *modulePtr;
	j9object_t moduleName = module->moduleName;

	return moduleName;
}
static UDATA 
moduleNameHashFn(void *key, void *userData) 
{
	J9JavaVM *javaVM = (J9JavaVM*) userData;
	j9object_t name = moduleHashGetName(key);

	return javaVM->memoryManagerFunctions->j9gc_stringHashFn(&name, userData);
}
static UDATA  
moduleNameHashEqualFn(void *tableNode, void *queryNode, void *userData)
{
	J9JavaVM *javaVM = (J9JavaVM*) userData;

	const J9Module* const tableNodeModule = *((J9Module**)tableNode);
	j9object_t tableNodeModuleName = tableNodeModule->moduleName;

	const J9Module* const queryNodeModule = *((J9Module**)queryNode);
	j9object_t queryNodeModuleName = queryNodeModule->moduleName;

	Assert_VM_true(tableNodeModule->classLoader == queryNodeModule->classLoader);
	
	return javaVM->memoryManagerFunctions->j9gc_stringHashEqualFn(&tableNodeModuleName, &queryNodeModuleName, userData);
}

static UDATA 
modulePointerHashFn(void *key, void *userData) 
{
	const J9Module** const modulePtr = (const J9Module**)key;
	
	return (UDATA)(*modulePtr);
}
static UDATA  
modulePointerHashEqualFn(void *tableNode, void *queryNode, void *userData)
{
	const J9Module* const tableNodeModule = *((J9Module**)tableNode);
	const J9Module* const queryNodeModule = *((J9Module**)queryNode);

	return tableNodeModule == queryNodeModule;
}

static UDATA 
packageHashFn(void *key, void *userData) 
{
	J9JavaVM* javaVM = (J9JavaVM*) userData;
	J9Package *entry = *(J9Package**) key;

	return javaVM->internalVMFunctions->computeHashForUTF8(J9UTF8_DATA(entry->packageName), J9UTF8_LENGTH(entry->packageName));
}

static UDATA
moduleExtraInfoHashFn(void *key, void *userData)
{
	J9ModuleExtraInfo *entry = (J9ModuleExtraInfo *)key;

	return (UDATA)entry->j9module;
}

static UDATA  
packageHashEqualFn(void *tableNode, void *queryNode, void *userData)
{
	const J9Package* const tableNodePackage = *((J9Package**)tableNode);
	const J9Package* const queryNodePackage = *((J9Package**)queryNode);

	return J9UTF8_EQUALS(tableNodePackage->packageName, queryNodePackage->packageName) && (tableNodePackage->classLoader == queryNodePackage->classLoader);
}

static UDATA
moduleExtraInfoHashEqualFn(void *tableNode, void *queryNode, void *userData)
{
	J9ModuleExtraInfo *tableNodePatchPath = (J9ModuleExtraInfo *)tableNode;
	J9ModuleExtraInfo *queryNodePatchPath = (J9ModuleExtraInfo *)queryNode;

	return tableNodePatchPath->j9module == queryNodePatchPath->j9module;
}

J9HashTable *
hashModuleNameTableNew(J9JavaVM *javaVM, U_32 initialSize)
{
	U_32 flags = J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION;

	return hashTableNew(OMRPORT_FROM_J9PORT(javaVM->portLibrary), J9_GET_CALLSITE(), initialSize, sizeof(void*), sizeof(void*), flags, J9MEM_CATEGORY_MODULES, moduleNameHashFn, moduleNameHashEqualFn, NULL, javaVM);
}

J9HashTable *
hashModulePointerTableNew(J9JavaVM *javaVM, U_32 initialSize)
{
	U_32 flags = J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION;

	return hashTableNew(OMRPORT_FROM_J9PORT(javaVM->portLibrary), J9_GET_CALLSITE(), initialSize, sizeof(void*), sizeof(void*), flags, J9MEM_CATEGORY_MODULES, modulePointerHashFn, modulePointerHashEqualFn, NULL, javaVM);
}

J9HashTable *
hashPackageTableNew(J9JavaVM *javaVM, U_32 initialSize)
{
	U_32 flags = J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION;

	return hashTableNew(OMRPORT_FROM_J9PORT(javaVM->portLibrary), J9_GET_CALLSITE(), initialSize, sizeof(void*), sizeof(void*), flags, J9MEM_CATEGORY_MODULES, packageHashFn, packageHashEqualFn, NULL, javaVM);
}

J9HashTable *
hashModuleExtraInfoTableNew(J9JavaVM *javaVM, U_32 initialSize)
{
	U_32 flags = J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION;

	return hashTableNew(OMRPORT_FROM_J9PORT(javaVM->portLibrary), J9_GET_CALLSITE(), initialSize, sizeof(J9ModuleExtraInfo), sizeof(char *), flags, J9MEM_CATEGORY_MODULES, moduleExtraInfoHashFn, moduleExtraInfoHashEqualFn, NULL, javaVM);
}

J9Module *
findModuleForPackageUTF8(J9VMThread *currentThread, J9ClassLoader *classLoader, J9UTF8 *packageName)
{
	J9Package package = {0};
	J9Package * packagePtr = &package;
	J9Package ** targetPtr = NULL;
	J9Module * foundModule = NULL;

	package.packageName = packageName;
	package.classLoader = classLoader;

	targetPtr = hashTableFind(classLoader->packageHashTable, &packagePtr);
	if (NULL != targetPtr) {
		foundModule = (*targetPtr)->module;
	}

	/* TODO add asserts here once the package definitions have been fixed */

	return foundModule;
}

J9Module *
findModuleForPackage(J9VMThread *currentThread, J9ClassLoader *classLoader, U_8 *packageName, U_32 packageNameLen)
{
	U_8 buf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	J9UTF8 *packageNameUTF8 = (J9UTF8 *) buf;
	J9Module *foundModule = NULL;
	J9JavaVM *javaVM = currentThread->javaVM;

	PORT_ACCESS_FROM_JAVAVM(javaVM);

	if ((packageNameLen + sizeof(packageNameUTF8->length) + 1) > sizeof(buf)) {
		packageNameUTF8 = (J9UTF8 *) j9mem_allocate_memory(packageNameLen + 1 + sizeof(packageNameUTF8->length), J9MEM_CATEGORY_CLASSES);
	}
	if (NULL != packageNameUTF8) {
		memcpy(J9UTF8_DATA(packageNameUTF8), packageName, packageNameLen);
		J9UTF8_DATA(packageNameUTF8)[packageNameLen] = '\0';
		J9UTF8_SET_LENGTH(packageNameUTF8, (U_16) packageNameLen);

		foundModule = findModuleForPackageUTF8(currentThread, classLoader, packageNameUTF8);

		if (packageNameUTF8 != (J9UTF8 *)buf) {
			j9mem_free_memory(packageNameUTF8);
		}
	}
	return foundModule;
}

J9ModuleExtraInfo *
findModuleInfoForModule(J9VMThread *currentThread, J9ClassLoader *classLoader, J9Module *j9module)
{
	J9ModuleExtraInfo moduleInfo = {0};
	J9ModuleExtraInfo *targetPtr = NULL;

	if (NULL == classLoader->moduleExtraInfoHashTable) {
		return NULL;
	}

	moduleInfo.j9module = j9module;

	targetPtr = (J9ModuleExtraInfo *)hashTableFind(classLoader->moduleExtraInfoHashTable, (void *)&moduleInfo);
	return targetPtr;
}
