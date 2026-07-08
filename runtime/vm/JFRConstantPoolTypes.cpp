/*******************************************************************************
 * Copyright IBM Corp. and others 2024
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
#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "j9vmconstantpool.h"
#include "pool_api.h"

#if defined(J9VM_OPT_JFR)

#include "JFRConstantPoolTypes.hpp"

UDATA
VM_JFRConstantPoolTypes::jfrClassHashFn(void *key, void *userData)
{
	ClassEntry *classEntry = (ClassEntry *) key;

	return (UDATA) classEntry->clazz;
}

UDATA
VM_JFRConstantPoolTypes::jfrClassHashEqualFn(void *tableNode, void *queryNode, void *userData)
{
	ClassEntry *tableEntry = (ClassEntry *) tableNode;
	ClassEntry *queryEntry = (ClassEntry *) queryNode;

	return tableEntry->clazz == queryEntry->clazz;
}

UDATA
VM_JFRConstantPoolTypes::jfrPackageHashFn(void *key, void *userData)
{
	PackageEntry *packageEntry = (PackageEntry *) key;

	return *(UDATA*)&packageEntry->romClass;
}

UDATA
VM_JFRConstantPoolTypes::jfrPackageHashEqualFn(void *tableNode, void *queryNode, void *userData)
{
	PackageEntry *tableEntry = (PackageEntry *) tableNode;
	PackageEntry *queryEntry = (PackageEntry *) queryNode;

	return tableEntry->romClass == queryEntry->romClass;
}

UDATA
VM_JFRConstantPoolTypes::classloaderNameHashFn(void *key, void *userData)
{
	ClassloaderEntry *classLoaderEntry = (ClassloaderEntry *) key;

	return (UDATA) classLoaderEntry->classLoader;
}

UDATA
VM_JFRConstantPoolTypes::VM_JFRConstantPoolTypes::classloaderNameHashEqualFn(void *tableNode, void *queryNode, void *userData)
{
	ClassloaderEntry *tableEntry = (ClassloaderEntry *) tableNode;
	ClassloaderEntry *queryEntry = (ClassloaderEntry *) queryNode;

	return tableEntry->classLoader == queryEntry->classLoader;
}

UDATA
VM_JFRConstantPoolTypes::methodNameHashFn(void *key, void *userData)
{
	MethodEntry *methodEntry = (MethodEntry *) key;

	return (UDATA) methodEntry->romMethod;
}

UDATA
VM_JFRConstantPoolTypes::methodNameHashEqualFn(void *tableNode, void *queryNode, void *userData)
{
	MethodEntry *tableEntry = (MethodEntry *) tableNode;
	MethodEntry *queryEntry = (MethodEntry *) queryNode;

	return tableEntry->romMethod == queryEntry->romMethod;
}

UDATA
VM_JFRConstantPoolTypes::stackTraceHashFn(void *key, void *userData)
{
	StackTraceEntry *entry = (StackTraceEntry*) key;

	return (UDATA)(entry->currentThreadID ^ entry->ticks);
}

UDATA
VM_JFRConstantPoolTypes::stackTraceHashEqualFn(void *tableNode, void *queryNode, void *userData)
{
	StackTraceEntry *tableEntry = (StackTraceEntry *) tableNode;
	StackTraceEntry *queryEntry = (StackTraceEntry *) queryNode;

	return tableEntry->vmThread == queryEntry->vmThread && tableEntry->ticks == queryEntry->ticks;
}

UDATA
VM_JFRConstantPoolTypes::threadGroupHashFn(void *key, void *userData)
{
	J9JavaVM *vm = (J9JavaVM*) userData;
	ThreadGroupEntry *entry = (ThreadGroupEntry *) key;

	void *name = entry->threadGroupName;

	return vm->memoryManagerFunctions->j9gc_stringHashFn(&name, vm);
}

UDATA
VM_JFRConstantPoolTypes::threadGroupHashEqualFn(void *tableNode, void *queryNode, void *userData)
{
	J9JavaVM *vm = (J9JavaVM*) userData;
	ThreadGroupEntry *tableEntry = (ThreadGroupEntry *) tableNode;
	ThreadGroupEntry *queryEntry = (ThreadGroupEntry *) queryNode;

	j9object_t tableName = tableEntry->threadGroupName;

	j9object_t queryName = queryEntry->threadGroupName;

	return vm->memoryManagerFunctions->j9gc_stringHashEqualFn(&tableName, &queryName, vm);
}


UDATA
VM_JFRConstantPoolTypes::jfrStringUTF8HashFn(void *key, void *userData)
{
	StringUTF8Entry *stringEntry = (StringUTF8Entry *) key;

	J9UTF8 *name = stringEntry->string;

	return (UDATA) name;
}

UDATA
VM_JFRConstantPoolTypes::jfrStringUTF8HashEqualFn(void *tableNode, void *queryNode, void *userData)
{
	StringUTF8Entry *tableEntry = (StringUTF8Entry *) tableNode;
	StringUTF8Entry *queryEntry = (StringUTF8Entry *) queryNode;

	J9UTF8 *tableName = tableEntry->string;

	J9UTF8 *queryName = queryEntry->string;

	return tableName == queryName;
}

UDATA
VM_JFRConstantPoolTypes::jfrModuleHashFn(void *key, void *userData)
{
	ModuleEntry *moduleEntry = (ModuleEntry *) key;

	return (UDATA) moduleEntry->module;
}

UDATA
VM_JFRConstantPoolTypes::jfrModuleHashEqualFn(void *tableNode, void *queryNode, void *userData)
{
	ModuleEntry *tableEntry = (ModuleEntry *) tableNode;
	ModuleEntry *queryEntry = (ModuleEntry *) queryNode;

	J9Module *tableName = tableEntry->module;

	J9Module *queryName = queryEntry->module;

	return tableName == queryName;
}

UDATA
VM_JFRConstantPoolTypes::walkStringUTF8TablePrint(void *entry, void *userData)
{
	StringUTF8Entry *tableEntry = (StringUTF8Entry *) entry;
	J9VMThread *currentThread = (J9VMThread *)userData;
	PORT_ACCESS_FROM_VMC(currentThread);

	j9tty_printf(PORTLIB, "%u) name=%.*s\n", tableEntry->index, J9UTF8_LENGTH(tableEntry->string), (char*) J9UTF8_DATA(tableEntry->string));

	return FALSE;
}

UDATA
VM_JFRConstantPoolTypes::walkClassesTablePrint(void *entry, void *userData)
{
	ClassEntry *tableEntry = (ClassEntry *) entry;
	J9VMThread *currentThread = (J9VMThread *)userData;
	PORT_ACCESS_FROM_VMC(currentThread);

	j9tty_printf(PORTLIB, "%u) classLoaderIndex=%u nameStringUTF8Index=%u packageIndex=%u modifiers=%i \n", tableEntry->index, tableEntry->classLoaderIndex, tableEntry->nameStringUTF8Index, tableEntry->packageIndex, tableEntry->modifiers);

	return FALSE;
}

UDATA
VM_JFRConstantPoolTypes::walkClassLoadersTablePrint(void *entry, void *userData)
{
	ClassloaderEntry *tableEntry = (ClassloaderEntry *) entry;
	J9VMThread *currentThread = (J9VMThread *)userData;
	PORT_ACCESS_FROM_VMC(currentThread);

	j9tty_printf(PORTLIB, "%u) classIndex=%u nameIndex=%u\n", tableEntry->index, tableEntry->classIndex, tableEntry->nameIndex);

	return FALSE;
}

UDATA
VM_JFRConstantPoolTypes::walkMethodTablePrint(void *entry, void *userData)
{
	MethodEntry *tableEntry = (MethodEntry *) entry;
	J9VMThread *currentThread = (J9VMThread *)userData;
	PORT_ACCESS_FROM_VMC(currentThread);

	j9tty_printf(PORTLIB, "%u) classIndex=%u nameIndex=%u descriptorStringUTF8Index=%u modifiers=%u\n", tableEntry->index, tableEntry->classIndex, tableEntry->nameStringUTF8Index, tableEntry->descriptorStringUTF8Index, tableEntry->modifiers);

	return FALSE;
}

UDATA
VM_JFRConstantPoolTypes::walkModuleTablePrint(void *entry, void *userData)
{
	ModuleEntry *tableEntry = (ModuleEntry *) entry;
	J9VMThread *currentThread = (J9VMThread *)userData;
	PORT_ACCESS_FROM_VMC(currentThread);

	j9tty_printf(PORTLIB, "%u) nameStringIndex=%u versionStringIndex=%u locationStringUTF8Index=%u\n", tableEntry->index, tableEntry->nameStringIndex, tableEntry->versionStringIndex, tableEntry->locationStringUTF8Index);

	return FALSE;
}

UDATA
VM_JFRConstantPoolTypes::walkPackageTablePrint(void *entry, void *userData)
{
	PackageEntry *tableEntry = (PackageEntry *) entry;
	J9VMThread *currentThread = (J9VMThread *)userData;
	PORT_ACCESS_FROM_VMC(currentThread);

	j9tty_printf(PORTLIB, "%u) moduleIndex=%u packageName=%.*s exported=%u\n", tableEntry->index, tableEntry->moduleIndex, tableEntry->packageNameLength, (char*)tableEntry->packageName, tableEntry->exported);

	return FALSE;
}

void
VM_JFRConstantPoolTypes::walkThreadTablePrint(void *entry, void *userData)
{
	ThreadEntry *tableEntry = (ThreadEntry *) entry;
	J9VMThread *currentThread = (J9VMThread *)userData;
	PORT_ACCESS_FROM_VMC(currentThread);

	j9tty_printf(PORTLIB, "%u) osTID=%lld javaTID=%lld javaThreadName=%.*s osThreadName=%.*s threadGroupIndex=%u\n", tableEntry->index, tableEntry->osTID, tableEntry->javaTID, J9UTF8_LENGTH(tableEntry->javaThreadName), J9UTF8_DATA(tableEntry->javaThreadName), J9UTF8_LENGTH(tableEntry->osThreadName), J9UTF8_DATA(tableEntry->osThreadName), tableEntry->threadGroupIndex);
}

void
VM_JFRConstantPoolTypes::walkThreadStartTablePrint(void *entry, void *userData)
{
	ThreadStartEntry *tableEntry = (ThreadStartEntry *) entry;
	J9VMThread *currentThread = (J9VMThread *)userData;
	PORT_ACCESS_FROM_VMC(currentThread);

	j9tty_printf(PORTLIB, "ticks=%lld stackTraceIndex=%u threadIndex=%llu eventThreadIndex=%llu parentThreadIndex=%llu\n", tableEntry->ticks, tableEntry->stackTraceIndex, tableEntry->threadIndex, tableEntry->eventThreadIndex, tableEntry->parentThreadIndex);
}

void
VM_JFRConstantPoolTypes::walkThreadParkTablePrint(void *entry, void *userData)
{
	ThreadParkEntry *tableEntry = (ThreadParkEntry *) entry;
	J9VMThread *currentThread = (J9VMThread *)userData;
	PORT_ACCESS_FROM_VMC(currentThread);

	j9tty_printf(PORTLIB, "ticks=%lld duration=%lld threadIndex=%llu eventThreadIndex=%llu stackTraceIndex=%u parkedClass=%u timeOut=%lld untilTime=%lld parkedAddress=%llx\n", tableEntry->ticks, tableEntry->duration, tableEntry->threadIndex, tableEntry->eventThreadIndex, tableEntry->stackTraceIndex, tableEntry->parkedClass, tableEntry->timeOut, tableEntry->untilTime, tableEntry->parkedAddress);
}

UDATA
VM_JFRConstantPoolTypes::walkThreadGroupTablePrint(void *entry, void *userData)
{
	ThreadGroupEntry *tableEntry = (ThreadGroupEntry *) entry;
	J9VMThread *currentThread = (J9VMThread *)userData;
	PORT_ACCESS_FROM_VMC(currentThread);

	j9tty_printf(PORTLIB, "%u) parentIndex=%u name=%.*s\n", tableEntry->index, tableEntry->parentIndex,  J9UTF8_LENGTH(tableEntry->name), J9UTF8_DATA(tableEntry->name));

	return FALSE;
}

UDATA
VM_JFRConstantPoolTypes::walkStackTraceTablePrint(void *entry, void *userData)
{
	StackTraceEntry *tableEntry = (StackTraceEntry *) entry;
	J9VMThread *currentThread = (J9VMThread *)userData;
	PORT_ACCESS_FROM_VMC(currentThread);

	j9tty_printf(PORTLIB, "%u) time=%li numOfFrames=%u frames=%p curr=%p next=%p \n", tableEntry->index, tableEntry->ticks, tableEntry->numOfFrames, tableEntry->frames, tableEntry, tableEntry->next);

	return FALSE;
}


UDATA
VM_JFRConstantPoolTypes::findShallowEntries(void *entry, void *userData)
{
	ClassEntry *tableEntry = (ClassEntry *) entry;
	J9Pool *shallowEntries = (J9Pool*) userData;

	if (tableEntry->shallow) {
		ClassEntry **newEntry = (ClassEntry**)pool_newElement(shallowEntries);
		*newEntry = tableEntry;
	}

	return FALSE;
}

void
VM_JFRConstantPoolTypes::fixupShallowEntries(void *entry, void *userData)
{
	ClassEntry *tableEntry = *(ClassEntry **) entry;
	VM_JFRConstantPoolTypes *cp = (VM_JFRConstantPoolTypes*) userData;

	cp->getClassEntry(tableEntry->clazz, false);
}

UDATA
VM_JFRConstantPoolTypes::mergeStringUTF8EntriesToGlobalTable(void *entry, void *userData)
{
	StringUTF8Entry *tableEntry = (StringUTF8Entry *) entry;
	VM_JFRConstantPoolTypes *cp = (VM_JFRConstantPoolTypes*) userData;

	cp->_globalStringTable[tableEntry->index] = tableEntry;
	cp->_requiredBufferSize += J9UTF8_LENGTH(tableEntry->string);
	return FALSE;
}

UDATA
VM_JFRConstantPoolTypes::mergePackageEntriesToGlobalTable(void *entry, void *userData)
{
	PackageEntry *tableEntry = (PackageEntry *) entry;
	VM_JFRConstantPoolTypes *cp = (VM_JFRConstantPoolTypes*) userData;

	cp->_globalStringTable[tableEntry->index + cp->_stringUTF8Count] = tableEntry;
	cp->_requiredBufferSize += tableEntry->packageNameLength;
	return FALSE;
}

U_32
VM_JFRConstantPoolTypes::getMethodEntry(J9ROMMethod *romMethod, J9Class *ramClass)
{
	U_32 index = U_32_MAX;
	MethodEntry *entry = NULL;
	MethodEntry entryBuffer = {0};

	entry = &entryBuffer;
	entry->romMethod = romMethod;

	entry = (MethodEntry *) hashTableFind(_methodTable, entry);
	if (NULL != entry) {
		index = entry->index;
		goto done;
	} else {
		entry = &entryBuffer;
	}

	entry->classIndex = getClassEntry(ramClass);
	if (isResultNotOKay()) goto done;

	entry->nameStringUTF8Index = addStringUTF8Entry(J9ROMMETHOD_NAME(romMethod));
	if (isResultNotOKay()) goto done;

	entry->descriptorStringUTF8Index = addStringUTF8Entry(J9ROMMETHOD_SIGNATURE(romMethod));
	if (isResultNotOKay()) goto done;

	entry->modifiers = romMethod->modifiers;
	entry->index = _methodCount;
	entry->hidden = FALSE;

	entry = (MethodEntry*) hashTableAdd(_methodTable, entry);
	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	if (NULL == _firstMethodEntry) {
		_firstMethodEntry = entry;
	}

	if (NULL != _previousMethodEntry) {
		_previousMethodEntry->next = entry;
	}
	_previousMethodEntry = entry;

	index = entry->index;
	_methodCount++;

done:
	return index;
}

U_32
VM_JFRConstantPoolTypes::getClassEntry(J9Class *clazz, bool shallow)
{
	U_32 index = U_32_MAX;
	ClassEntry *entry = NULL;
	ClassEntry entryBuffer = {0};

	entry = &entryBuffer;
	entry->clazz = clazz;

	entry = (ClassEntry *) hashTableFind(_classTable, entry);
	if (NULL != entry) {
		index = entry->index;
		if (!entry->shallow) {
			goto done;
		}
	} else {
		entry = &entryBuffer;
	}

	entry->nameStringUTF8Index = addStringUTF8Entry(J9ROMCLASS_CLASSNAME(clazz->romClass));
	if (isResultNotOKay()) goto done;

	entry->classLoaderIndex = addClassLoaderEntry(clazz->classLoader, shallow);
	if (isResultNotOKay()) goto done;

	entry->packageIndex = addPackageEntry(clazz);
	if (isResultNotOKay()) goto done;

	entry->modifiers = clazz->romClass->modifiers;
	entry->hidden = FALSE; //TODO

	if (NULL != entry && !entry->shallow) {
		entry->index = clazz->classID;
		_classCount++;

		entry = (ClassEntry*) hashTableAdd(_classTable, entry);
		if (NULL == entry) {
			_buildResult = OutOfMemory;
			goto done;
		}

		if (NULL == _firstClassEntry) {
			_firstClassEntry = entry;
		}

		if (NULL != _previousClassEntry) {
			_previousClassEntry->next = entry;
		}
		_previousClassEntry = entry;
	}

	entry->shallow = NULL;

	index = entry->index;

done:
	return index;
}

U_32
VM_JFRConstantPoolTypes::addPackageEntry(J9Class *clazz)
{
	U_32 index = U_32_MAX;
	J9PackageIDTableEntry *pkgID =  NULL;
	PackageEntry *entry = NULL;
	UDATA packageNameLength = 0;
	const U_8 *packageName = NULL;
	PackageEntry entryBuffer = {0};

	entry = &entryBuffer;

	pkgID = hashPkgTableAt(clazz->classLoader, clazz->romClass);
	entry->romClass = clazz->romClass;

	if (NULL == pkgID) {
		/* default pacakge */
		index = 0;
		goto done;
	}

	entry = (PackageEntry *) hashTableFind(_packageTable, entry);
	if (NULL != entry) {
		index = entry->index;
		goto done;
	} else {
		entry = &entryBuffer;
	}

	entry->moduleIndex = addModuleEntry(clazz->module);
	if (isResultNotOKay()) goto done;

	packageName = getPackageName(pkgID, &packageNameLength);
	if (NULL == packageName) {
		_buildResult = InternalVMError;
		goto done;
	}

	entry->packageName = packageName;
	entry->packageNameLength = (U_32)packageNameLength;

	entry->exported = FALSE; //TODO

	entry->index = _packageCount;
	_packageCount++;

	entry = (PackageEntry*) hashTableAdd(_packageTable, entry);
	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	if (NULL == _firstPackageEntry) {
		_firstPackageEntry = entry;
	}

	if (NULL != _previousPackageEntry) {
		_previousPackageEntry->next = entry;
	}
	_previousPackageEntry = entry;

	index = entry->index;
done:
	return index;
}

U_32
VM_JFRConstantPoolTypes::addPackageEntry(J9Module *fromModule, J9Package *package, BOOLEAN exported)
{
	U_32 index = U_32_MAX;
	J9PackageIDTableEntry *pkgID = NULL;
	PackageEntry *entry = NULL;
	PackageEntry entryBuffer = {0};

	const U_16 NAME_BUFFER_SIZE = 256;
	U_8 nameBuffer[NAME_BUFFER_SIZE];
	J9UTF8 *pkgNameUTF8 = (J9UTF8 *)nameBuffer;

	J9ROMClass tempClass = {0};
	J9ROMClass *queryROMClass = &tempClass;

	const U_16 pkgNameLen = J9UTF8_LENGTH(package->packageName);

	/* Account for the length field. */
	if (pkgNameLen > (NAME_BUFFER_SIZE - sizeof(J9UTF8))) {
		UDATA allocationSize = sizeof(J9ROMClass) + sizeof(J9UTF8) + pkgNameLen;
		queryROMClass = (J9ROMClass *)j9mem_allocate_memory(allocationSize, J9MEM_CATEGORY_JFR);
		if (NULL == queryROMClass) {
			_buildResult = OutOfMemory;
			goto done;
		}
		pkgNameUTF8 = (J9UTF8 *)(queryROMClass + 1);
	}

	memcpy(J9UTF8_DATA(pkgNameUTF8), J9UTF8_DATA(package->packageName), pkgNameLen);
	J9UTF8_SET_LENGTH(pkgNameUTF8, pkgNameLen);

	memset(queryROMClass, 0, sizeof(*queryROMClass));
	NNSRP_SET(queryROMClass->className, pkgNameUTF8);

	entry = &entryBuffer;

	pkgID = hashPkgTableAt(_vm->systemClassLoader, queryROMClass);

	if (NULL == pkgID) {
		index = 0;
		goto done;
	}

	entry->romClass = (J9ROMClass *)(pkgID->taggedROMClass & ~(UDATA)(J9PACKAGE_ID_TAG | J9PACKAGE_ID_GENERATED));

	entry = (PackageEntry *)hashTableFind(_packageTable, entry);

	if (NULL != entry) {
		index = entry->index;
		goto done;
	} else {
		entry = &entryBuffer;
	}

	entry->moduleIndex = addModuleEntry(fromModule);
	if (isResultNotOKay()) goto done;

	entry->packageName = J9UTF8_DATA(package->packageName);
	entry->packageNameLength = pkgNameLen;

	entry->exported = exported;
	entry->index = _packageCount;
	_packageCount++;

	entry = (PackageEntry *)hashTableAdd(_packageTable, entry);
	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	if (NULL == _firstPackageEntry) {
		_firstPackageEntry = entry;
	}

	if (NULL != _previousPackageEntry) {
		_previousPackageEntry->next = entry;
	}
	_previousPackageEntry = entry;

	index = entry->index;
done:
	if ((NULL != queryROMClass) && (&tempClass != queryROMClass)) {
		j9mem_free_memory(queryROMClass);
	}
	return index;
}

U_32
VM_JFRConstantPoolTypes::addModuleEntry(J9Module *module)
{
	U_32 index = U_32_MAX;
	ModuleEntry *entry = NULL;
	ModuleEntry entryBuffer = {0};

	entry = &entryBuffer;
	entry->module = module;

	if (NULL == entry->module) {
		/* unnamed module */
		index = 0;
		goto done;
	}

	entry = (ModuleEntry *) hashTableFind(_moduleTable, entry);
	if (NULL != entry) {
		index = entry->index;
		goto done;
	} else {
		entry = &entryBuffer;
	}

	entry->nameStringIndex = addStringUTF8Entry(entry->module->moduleName);
	if (isResultNotOKay()) goto done;

	entry->versionStringIndex = addStringEntry(entry->module->version);
	if (isResultNotOKay()) goto done;

	entry->locationStringUTF8Index = addStringUTF8Entry(getModuleJRTURL(_currentThread, entry->module->classLoader, entry->module));
	if (isResultNotOKay()) goto done;

	entry->classLoaderIndex = addClassLoaderEntry(entry->module->classLoader);
	if (isResultNotOKay()) goto done;

	entry->index = _moduleCount;
	_moduleCount++;

	entry = (ModuleEntry*) hashTableAdd(_moduleTable, entry);
	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	if (NULL == _firstModuleEntry) {
		_firstModuleEntry = entry;
	}

	if (NULL != _previousModuleEntry) {
		_previousModuleEntry->next = entry;
	}
	_previousModuleEntry = entry;

	index = entry->index;
done:
	return index;
}

U_32
VM_JFRConstantPoolTypes::addClassLoaderEntry(J9ClassLoader *classLoader, bool shallow)
{
	U_32 index = U_32_MAX;
	ClassloaderEntry *entry = NULL;
	ClassloaderEntry entryBuffer = {0};
	j9object_t loaderName = NULL;

	entry = &entryBuffer;
	entry->classLoader = classLoader;

	entry = (ClassloaderEntry *) hashTableFind(_classLoaderTable, entry);
	if (NULL != entry) {
		index = entry->index;
		goto done;
	} else {
		entry = &entryBuffer;
	}

	if (shallow) {
		entry->classIndex = getShallowClassEntry(J9OBJECT_CLAZZ(_currentThread, classLoader->classLoaderObject));
	} else {
		entry->classIndex = getClassEntry(J9OBJECT_CLAZZ(_currentThread, classLoader->classLoaderObject), false);
	}
	if (isResultNotOKay()) goto done;

#if JAVA_SPEC_VERSION > 8
	loaderName = J9VMJAVALANGCLASSLOADER_CLASSLOADERNAME(_currentThread, classLoader->classLoaderObject);
	if (NULL == loaderName) {
		entry->nameIndex = addStringUTF8Entry((J9UTF8*)&bootLoaderName);
	} else
#endif /* JAVA_SPEC_VERSION > 8 */
	{
		entry->nameIndex = addStringEntry(loaderName);
	}

	if (isResultNotOKay()) goto done;

	entry->index = _classLoaderCount;
	_classLoaderCount++;

	entry = (ClassloaderEntry*) hashTableAdd(_classLoaderTable, entry);
	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	if (NULL == _firstClassloaderEntry) {
		_firstClassloaderEntry = entry;
	}

	if (NULL != _previousClassloaderEntry) {
		_previousClassloaderEntry->next = entry;
	}
	_previousClassloaderEntry = entry;

	index = entry->index;

done:
	return index;
}

U_32
VM_JFRConstantPoolTypes::getShallowClassEntry(J9Class *clazz)
{
	U_32 index = U_32_MAX;
	ClassEntry *entry = NULL;
	ClassEntry entryBuffer = {0};

	entry = &entryBuffer;
	entry->clazz = clazz;

	entry = (ClassEntry *) hashTableFind(_classTable, entry);
	if (NULL != entry) {
		index = entry->index;
		goto done;
	} else {
		entry = &entryBuffer;
	}

	entry->index = clazz->classID;
	_classCount++;

	entry = (ClassEntry*)hashTableAdd(_classTable, entry);
	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}
	index = entry->index;

	entry->classLoaderIndex = U_32_MAX;
	entry->nameStringUTF8Index = U_32_MAX;
	entry->packageIndex = U_32_MAX;
	entry->modifiers = U_32_MAX;
	entry->hidden = FALSE;
	entry->shallow = clazz;

	if (NULL == _firstClassEntry) {
		_firstClassEntry = entry;
	}

	if (NULL != _previousClassEntry) {
		_previousClassEntry->next = entry;
	}
	_previousClassEntry = entry;

done:
	return index;
}

U_32
VM_JFRConstantPoolTypes::addStringUTF8Entry(J9UTF8 *string, bool free)
{
	U_32 index = U_32_MAX;
	StringUTF8Entry *entry = NULL;
	StringUTF8Entry entryBuffer = {0};

	entry = &entryBuffer;
	entry->string = string;

	if (NULL == string) {
		/* default null string */
		index = 0;
		goto done;
	}

	entry = (StringUTF8Entry *) hashTableFind(_stringUTF8Table, entry);
	if (NULL != entry) {
		index = entry->index;
		if (free) {
			j9mem_free_memory(string);
		}
		goto done;
	} else {
		entry = &entryBuffer;
	}

	entry->index = _stringUTF8Count;
	entry->free = free;
	_stringUTF8Count++;

	if (NULL == hashTableAdd(_stringUTF8Table, &entryBuffer)) {
		_buildResult = OutOfMemory;
		goto done;
	}
	index = entry->index;

done:
	return index;
}

U_32
VM_JFRConstantPoolTypes::addStringUTF8Entry(J9UTF8 *string)
{
	return addStringUTF8Entry(string, false);
}

U_32
VM_JFRConstantPoolTypes::addStringEntry(j9object_t string)
{
	if (NULL == string) {
		/* default NULL string entry */
		return 0;
	} else {
		J9UTF8 *stringUTF8 = copyStringToJ9UTF8WithMemAlloc(_currentThread, string, J9_STR_NONE, "", 0, NULL, 0);
		return addStringUTF8Entry(stringUTF8, true);
	}
}

void
VM_JFRConstantPoolTypes::addAllThreads()
{
	omrthread_monitor_enter(_vm->jfrState.threadObjectsMutex);
	J9HashTableState hashTableState = {0};
	J9JFRThreadObject *entry = (J9JFRThreadObject *)hashTableStartDo(_vm->jfrState.threadObjectJNIRefTable, &hashTableState);
	while (NULL != entry) {
		addThreadObjectEntry(entry);
		entry = (J9JFRThreadObject *)hashTableNextDo(&hashTableState);
	}
	omrthread_monitor_exit(_vm->jfrState.threadObjectsMutex);
}

void
VM_JFRConstantPoolTypes::addThreadObjectEntry(J9JFRThreadObject *tableEntry)
{
	ThreadEntry *entry = NULL;
	j9object_t threadObject = J9_JNI_UNWRAP_REFERENCE(tableEntry->threadObject);

	entry = (ThreadEntry *)pool_newElement(_threadTable);
	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	if ((NULL != threadObject) && J9VMJAVALANGTHREAD_STARTED(_currentThread, threadObject)) {
		entry->javaThreadName = copyStringToJ9UTF8WithMemAlloc(_currentThread, J9VMJAVALANGTHREAD_NAME(_currentThread, threadObject), J9_STR_NONE, "", 0, NULL, 0);
		entry->freeName = TRUE;
		if (isResultNotOKay()) goto done;
#if JAVA_SPEC_VERSION >= 19
		entry->threadGroupIndex = addThreadGroupEntry(J9VMJAVALANGTHREADFIELDHOLDER_GROUP(_currentThread, (J9VMJAVALANGTHREAD_HOLDER(_currentThread, threadObject))));
#else /* JAVA_SPEC_VERSION >= 19 */
		entry->threadGroupIndex = addThreadGroupEntry(J9VMJAVALANGTHREAD_GROUP(_currentThread, threadObject));
#endif /* JAVA_SPEC_VERSION >= 19 */
		if (isResultNotOKay()) goto done;
	}

	/* TODO is this always true? */
	entry->osThreadName = entry->javaThreadName;

	entry->javaTID = tableEntry->javaTID;
	entry->osTID = tableEntry->nativeTID;
	entry->index = entry->javaTID;

	_threadCount++;

	if (NULL == _firstThreadEntry) {
		_firstThreadEntry = entry;
	}

	if (NULL != _previousThreadEntry) {
		_previousThreadEntry->next = entry;
	}
	_previousThreadEntry = entry;

done:
	return;
}

U_32
VM_JFRConstantPoolTypes::addNetworkInterfaceNameEntry(const char *networkInterfaceName)
{
	U_32 index = U_32_MAX;
	NetworkInterfaceNameEntry *entry = NULL;
	size_t nameLength = 0;

	Assert_VM_notNull(networkInterfaceName);

	/* Search through linked list for existing entry. */
	entry = _firstNetworkInterfaceNameEntry;
	while (NULL != entry) {
		if (0 == strcmp(entry->networkInterfaceName, networkInterfaceName)) {
			index = entry->index;
			goto done;
		}
		entry = entry->next;
	}

	nameLength = strlen(networkInterfaceName);

	/* Not found, create new entry. */
	entry = (NetworkInterfaceNameEntry *)j9mem_allocate_memory(sizeof(*entry) + nameLength + 1, J9MEM_CATEGORY_JFR);
	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	memset(entry, 0, sizeof(*entry));
	memcpy(entry->networkInterfaceName, networkInterfaceName, nameLength + 1);
	entry->index = _networkInterfaceNameCount;
	_networkInterfaceNameCount += 1;

	if (NULL == _firstNetworkInterfaceNameEntry) {
		_firstNetworkInterfaceNameEntry = entry;
	}

	if (NULL != _previousNetworkInterfaceNameEntry) {
		_previousNetworkInterfaceNameEntry->next = entry;
	}
	_previousNetworkInterfaceNameEntry = entry;

	index = entry->index;

done:
	return index;
}

U_32
VM_JFRConstantPoolTypes::addThreadGroupEntry(j9object_t threadGroup)
{
	U_32 index = U_32_MAX;
	ThreadGroupEntry *entry = NULL;
	ThreadGroupEntry entryBuffer = {0};

	entry = &entryBuffer;

	if (NULL == threadGroup) {
		return 0;
	}

	entry->threadGroupName = J9VMJAVALANGTHREADGROUP_NAME(_currentThread, threadGroup);

	entry = (ThreadGroupEntry *) hashTableFind(_threadGroupTable, entry);
	if (NULL != entry) {
		index = entry->index;
		goto done;
	} else {
		entry = &entryBuffer;
	}

	entry->parentIndex = addThreadGroupEntry(J9VMJAVALANGTHREADGROUP_PARENT(_currentThread, threadGroup));
	if (isResultNotOKay()) goto done;

	/* Check again to see if the Threadgroup was added recursively. */
	entry = (ThreadGroupEntry *) hashTableFind(_threadGroupTable, entry);
	if (NULL != entry) {
		index = entry->index;
		goto done;
	} else {
		entry = &entryBuffer;
	}

	entry->name = copyStringToJ9UTF8WithMemAlloc(_currentThread, J9VMJAVALANGTHREADGROUP_NAME(_currentThread, threadGroup), J9_STR_NONE, "", 0, NULL, 0);

	entry->index = _threadGroupCount;
	_threadGroupCount++;

	entry = (ThreadGroupEntry*) hashTableAdd(_threadGroupTable, &entryBuffer);
	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	if (NULL == _firstThreadGroupEntry) {
		_firstThreadGroupEntry = entry;
	}

	if (NULL != _previousThreadGroupEntry) {
		_previousThreadGroupEntry->next = entry;
	}
	_previousThreadGroupEntry = entry;

	index = entry->index;

done:
	return index;
}

U_32
VM_JFRConstantPoolTypes::addStackTraceEntry(U_64 threadTID, I_64 ticks, U_32 numOfFrames, U_32 stackTraceID)
{
	U_32 index = U_32_MAX;
	StackTraceEntry *entry = NULL;
	StackTraceEntry entryBuffer = {0};

	entry = &entryBuffer;
	entry->currentThreadID = threadTID;
	entry->ticks = ticks;

	entry = (StackTraceEntry *) hashTableFind(_stackTraceTable, entry);
	if (NULL != entry) {
		index = entry->index;
		goto done;
	} else {
		entry = &entryBuffer;
	}

	entry->frames = _currentStackFrameBuffer;
	entry->numOfFrames = numOfFrames;

	_currentStackFrameBuffer = NULL;

	entry->index = stackTraceID;
	entry->truncated = FALSE;

	entry->next = NULL;
	_stackTraceCount++;

	entry = (StackTraceEntry*) hashTableAdd(_stackTraceTable, entry);
	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	if (NULL == _firstStackTraceEntry) {
		_firstStackTraceEntry = entry;
	}

	if (NULL != _previousStackTraceEntry) {
		_previousStackTraceEntry->next = entry;
	}
	_previousStackTraceEntry = entry;

	index = entry->index;

done:
	return index;
}

void
VM_JFRConstantPoolTypes::addExecutionSampleEntry(J9JFRExecutionSample *executionSampleData)
{
	ExecutionSampleEntry *entry = (ExecutionSampleEntry*)pool_newElement(_executionSampleTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = executionSampleData->startTicks;
	entry->state = RUNNABLE; //TODO

	/* Use the TID directly as the thread index */
	entry->threadIndex = executionSampleData->currentThreadTID;

	entry->stackTraceIndex = consumeStackTrace(executionSampleData->currentThreadTID, J9JFREXECUTIONSAMPLE_STACKTRACE(executionSampleData), executionSampleData->stackTraceSize, executionSampleData->stackTraceID);
	if (isResultNotOKay()) goto done;

	_executionSampleCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addThreadStartEntry(J9JFRThreadStart *threadStartData)
{
	ThreadStartEntry *entry = (ThreadStartEntry*)pool_newElement(_threadStartTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = threadStartData->startTicks;

	/* Use the TIDs directly as thread indices */
	entry->threadIndex = threadStartData->threadTID;
	entry->eventThreadIndex = threadStartData->threadTID;
	entry->parentThreadIndex = threadStartData->parentThreadTID;
	entry->stackTraceIndex = consumeStackTrace(threadStartData->currentThreadTID, J9JFRTHREADSTART_STACKTRACE(threadStartData), threadStartData->stackTraceSize, threadStartData->stackTraceID);
	if (isResultNotOKay()) goto done;

	_threadStartCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addThreadEndEntry(J9JFREvent *threadEndData)
{
	ThreadEndEntry *entry = (ThreadEndEntry*)pool_newElement(_threadEndTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = threadEndData->startTicks;

	/* Use the TID directly as the thread index */
	entry->threadIndex = threadEndData->currentThreadTID;
	entry->eventThreadIndex = threadEndData->currentThreadTID;

	_threadEndCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addThreadSleepEntry(J9JFRThreadSlept *threadSleepData)
{
	ThreadSleepEntry *entry = (ThreadSleepEntry*)pool_newElement(_threadSleepTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = threadSleepData->startTicks;
	entry->duration = threadSleepData->duration;
	entry->sleepTime = threadSleepData->time;

	/* Use the TID directly as the thread index */
	entry->threadIndex = threadSleepData->currentThreadTID;
	entry->eventThreadIndex = threadSleepData->currentThreadTID;

	entry->stackTraceIndex = consumeStackTrace(threadSleepData->currentThreadTID, J9JFRTHREADSLEPT_STACKTRACE(threadSleepData), threadSleepData->stackTraceSize, threadSleepData->stackTraceID);
	if (isResultNotOKay()) goto done;

	_threadSleepCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addMonitorWaitEntry(J9JFRMonitorWaited* threadWaitData)
{
	MonitorWaitEntry *entry = (MonitorWaitEntry*)pool_newElement(_monitorWaitTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = threadWaitData->startTicks;
	entry->duration = threadWaitData->duration;
	entry->timeOut = threadWaitData->time;
	entry->monitorAddress = (I_64)(U_64)threadWaitData->monitorAddress;
	entry->timedOut = threadWaitData->timedOut;

	/* Use the TIDs directly as thread indices */
	entry->threadIndex = threadWaitData->currentThreadTID;
	entry->eventThreadIndex = threadWaitData->currentThreadTID;

	entry->stackTraceIndex = consumeStackTrace(threadWaitData->currentThreadTID, J9JFRMonitorWaitedED_STACKTRACE(threadWaitData), threadWaitData->stackTraceSize, threadWaitData->stackTraceID);
	if (isResultNotOKay()) goto done;

	entry->monitorClass = getClassEntry(threadWaitData->monitorClass);
	if (isResultNotOKay()) goto done;

	entry->notifierThread = 0; //Need a way to find the notifiying thread

	_monitorWaitCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addMonitorEnterEntry(J9JFRMonitorEntered *monitorEnterData)
{
	MonitorEnterEntry *entry = (MonitorEnterEntry *)pool_newElement(_monitorEnterTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}
	entry->ticks = monitorEnterData->startTicks;
	entry->duration = monitorEnterData->duration;
	entry->monitorAddress = monitorEnterData->monitorAddress;

	/* Use the TIDs directly as thread indices */
	entry->threadIndex = monitorEnterData->currentThreadTID;
	entry->previousOwnerThread = monitorEnterData->previousOwnerTID;
	entry->eventThreadIndex = monitorEnterData->currentThreadTID;

	entry->stackTraceIndex = consumeStackTrace(monitorEnterData->currentThreadTID, J9JFRMONITORENTERED_STACKTRACE(monitorEnterData), monitorEnterData->stackTraceSize, monitorEnterData->stackTraceID);
	if (isResultNotOKay()) goto done;

	entry->monitorClass = getClassEntry(monitorEnterData->monitorClass);
	if (isResultNotOKay()) goto done;

	_monitorEnterCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addThreadParkEntry(J9JFRThreadParked* threadParkData)
{
	ThreadParkEntry *entry = (ThreadParkEntry*)pool_newElement(_threadParkTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = threadParkData->startTicks;
	entry->duration = threadParkData->duration;

	entry->parkedAddress = (U_64)threadParkData->parkedAddress;
	entry->threadIndex = threadParkData->currentThreadTID;
	entry->eventThreadIndex = threadParkData->currentThreadTID;

	entry->stackTraceIndex = consumeStackTrace(threadParkData->currentThreadTID, J9JFRTHREADPARKED_STACKTRACE(threadParkData), threadParkData->stackTraceSize, threadParkData->stackTraceID);
	if (isResultNotOKay()) goto done;

	entry->parkedClass = getClassEntry(threadParkData->parkedClass);
	if (isResultNotOKay()) goto done;

	entry->timeOut = threadParkData->timeOut;
	entry->untilTime = threadParkData->untilTime;

	_threadParkCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addCPULoadEntry(J9JFRCPULoad *cpuLoadData)
{
	CPULoadEntry *entry = (CPULoadEntry *)pool_newElement(_cpuLoadTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = cpuLoadData->startTicks;
	entry->jvmUser = cpuLoadData->jvmUser;
	entry->jvmSystem = cpuLoadData->jvmSystem;
	entry->machineTotal = cpuLoadData->machineTotal;

	_cpuLoadCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addThreadCPULoadEntry(J9JFRThreadCPULoad *threadCPULoadData)
{
	ThreadCPULoadEntry *entry = (ThreadCPULoadEntry *)pool_newElement(_threadCPULoadTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = threadCPULoadData->startTicks;
	entry->userCPULoad = threadCPULoadData->userCPULoad;
	entry->systemCPULoad = threadCPULoadData->systemCPULoad;

	/* Use the TID directly as the thread index */
	entry->threadIndex = threadCPULoadData->currentThreadTID;

	_threadCPULoadCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addClassLoadingStatisticsEntry(J9JFRClassLoadingStatistics *classLoadingStatisticsData)
{
	ClassLoadingStatisticsEntry *entry = (ClassLoadingStatisticsEntry *)pool_newElement(_classLoadingStatisticsTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = classLoadingStatisticsData->startTicks;
	entry->loadedClassCount = classLoadingStatisticsData->loadedClassCount;
	entry->unloadedClassCount = classLoadingStatisticsData->unloadedClassCount;

	_classLoadingStatisticsCount += 1;
done:
	return;
}

void
VM_JFRConstantPoolTypes::addThreadContextSwitchRateEntry(J9JFRThreadContextSwitchRate *threadContextSwitchRateData)
{
	ThreadContextSwitchRateEntry *entry = (ThreadContextSwitchRateEntry *)pool_newElement(_threadContextSwitchRateTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		return;
	}

	entry->ticks = threadContextSwitchRateData->startTicks;
	entry->switchRate = threadContextSwitchRateData->switchRate;

	_threadContextSwitchRateCount += 1;
}

void
VM_JFRConstantPoolTypes::addThreadStatisticsEntry(J9JFRThreadStatistics *threadStatisticsData)
{
	ThreadStatisticsEntry *entry = (ThreadStatisticsEntry *)pool_newElement(_threadStatisticsTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = threadStatisticsData->startTicks;
	entry->activeThreadCount = threadStatisticsData->activeThreadCount;
	entry->daemonThreadCount = threadStatisticsData->daemonThreadCount;
	entry->accumulatedThreadCount = threadStatisticsData->accumulatedThreadCount;
	entry->peakThreadCount = threadStatisticsData->peakThreadCount;

	_threadStatisticsCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addSystemGCEntry(J9JFRSystemGC *systemGCData)
{
	SystemGCEntry *entry = (SystemGCEntry *)pool_newElement(_systemGCTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = systemGCData->startTicks;
	entry->duration = systemGCData->duration;

	/* Use the TID directly as the thread index */
	entry->eventThreadIndex = systemGCData->currentThreadTID;

	entry->stackTraceIndex = consumeStackTrace(systemGCData->currentThreadTID, J9JFRSYSTEMGC_STACKTRACE(systemGCData), systemGCData->stackTraceSize, systemGCData->stackTraceID);
	if (isResultNotOKay()) goto done;

	_systemGCCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addOldGarbageCollectionEntry(J9JFROldGarbageCollection *oldGarbageCollectionData)
{
	OldGarbageCollectionEntry *entry = (OldGarbageCollectionEntry *)pool_newElement(_oldGarbageCollectionTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = oldGarbageCollectionData->startTicks;
	entry->duration = oldGarbageCollectionData->duration;
	entry->gcID = oldGarbageCollectionData->gcID;

	_oldGarbageCollectionCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addYoungGarbageCollectionEntry(J9JFRYoungGarbageCollection *youngGarbageCollectionData)
{
	YoungGarbageCollectionEntry *entry = (YoungGarbageCollectionEntry *)pool_newElement(_youngGarbageCollectionTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = youngGarbageCollectionData->startTicks;
	entry->duration = youngGarbageCollectionData->duration;
	entry->gcID = youngGarbageCollectionData->gcID;
	entry->tenureThreshold = youngGarbageCollectionData->tenureThreshold;

	_youngGarbageCollectionCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addGarbageCollectionEntry(J9JFRGarbageCollection *garbageCollectionData)
{
	GarbageCollectionEntry *entry = (GarbageCollectionEntry *)pool_newElement(_garbageCollectionTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = garbageCollectionData->startTicks;
	entry->duration = garbageCollectionData->duration;
	entry->gcID = garbageCollectionData->gcID;

	entry->gcNameID = static_cast<GCNameType>(garbageCollectionData->gcNameID);
	entry->gcCauseID = static_cast<GCCauseType>(garbageCollectionData->gcCauseID);

	entry->sumOfPauses = garbageCollectionData->sumOfPauses;
	entry->longestPause = garbageCollectionData->longestPause;

	_garbageCollectionCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addGCHeapSummaryEntry(J9JFRGCHeapSummary *gcHeapSummaryData)
{
	GCHeapSummaryEntry *entry = (GCHeapSummaryEntry *)pool_newElement(_gcHeapSummaryTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = gcHeapSummaryData->startTicks;
	entry->gcID = gcHeapSummaryData->gcID;

	entry->gcWhenID = gcHeapSummaryData->gcWhenID;

	entry->heapSpace.start = gcHeapSummaryData->vStart;
	entry->heapSpace.committedEnd = gcHeapSummaryData->vCommittedEnd;
	entry->heapSpace.committedSize = gcHeapSummaryData->vCommittedSize;
	entry->heapSpace.reservedEnd = gcHeapSummaryData->vReservedEnd;
	entry->heapSpace.reservedSize = gcHeapSummaryData->vReservedSize;

	entry->heapUsed = gcHeapSummaryData->heapUsed;

	_gcHeapSummaryCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addNetworkUtilizationEntry(J9JFRNetworkUtilization *networkUtilizationData)
{
	NetworkUtilizationEntry *entry = (NetworkUtilizationEntry *)pool_newElement(_networkUtilizationTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = networkUtilizationData->startTicks;
	entry->networkInterfaceIndex = addNetworkInterfaceNameEntry(networkUtilizationData->networkInterface);
	entry->readRate = networkUtilizationData->readRate;
	entry->writeRate = networkUtilizationData->writeRate;

	_networkUtilizationCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addDataLossEntry(J9JFRDataLoss *dataLossData)
{
	DataLossEntry *entry = (DataLossEntry *)pool_newElement(_dataLossTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = dataLossData->startTicks;
	entry->amount = dataLossData->amount;
	entry->total = dataLossData->total;

	_dataLossCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addThreadDumpEntry(J9JFRThreadDump *threadDumpData)
{
	ThreadDumpEntry *entry = (ThreadDumpEntry *)pool_newElement(_threadDumpTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = threadDumpData->startTicks;
	entry->result = threadDumpData->result;
	entry->resultLength = threadDumpData->resultLength;

	_threadDumpCount += threadDumpData->resultLength;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addThreadAllocationStatistics(J9JFRThreadAllocationStatistics *threadAlocationData)
{
	ThreadAllocationStatisticsEntry *entry = (ThreadAllocationStatisticsEntry *)pool_newElement(_threadAllocationStatisticsTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = threadAlocationData->startTicks;
	entry->threadIndex = threadAlocationData->currentThreadTID;
	entry->allocated = threadAlocationData->allocated;

	_threadAllocationStatisticsCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::addPhysicalMemoryEntry(J9JFRPhysicalMemory *physicalMemoryData)
{
	PhysicalMemoryEntry *entry = (PhysicalMemoryEntry *)pool_newElement(_physicalMemoryTable);

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = physicalMemoryData->startTicks;
	entry->totalSize = physicalMemoryData->totalSize;
	entry->usedSize = physicalMemoryData->usedSize;

	_physicalMemoryCount += 1;

done:
	return;
}

void
VM_JFRConstantPoolTypes::printTables()
{
	j9tty_printf(PORTLIB, "--------------- StringUTF8Table ---------------\n");
	hashTableForEachDo(_stringUTF8Table, &walkStringUTF8TablePrint, _currentThread);

	j9tty_printf(PORTLIB, "--------------- Classes Table ---------------\n");
	hashTableForEachDo(_classTable, &walkClassesTablePrint, _currentThread);

	j9tty_printf(PORTLIB, "--------------- ClassLoader Table ---------------\n");
	hashTableForEachDo(_classLoaderTable, &walkClassLoadersTablePrint, _currentThread);

	j9tty_printf(PORTLIB, "--------------- Method Table ---------------\n");
	hashTableForEachDo(_methodTable, &walkMethodTablePrint, _currentThread);

	j9tty_printf(PORTLIB, "--------------- Module Table ---------------\n");
	hashTableForEachDo(_moduleTable, &walkModuleTablePrint, _currentThread);

	j9tty_printf(PORTLIB, "--------------- Package Table ---------------\n");
	hashTableForEachDo(_packageTable, &walkPackageTablePrint, _currentThread);

	j9tty_printf(PORTLIB, "--------------- Thread Table ---------------\n");
	pool_do(_threadTable, &walkThreadTablePrint, _currentThread);
	j9tty_printf(PORTLIB, "total count %d\n", (int)_threadCount);

	j9tty_printf(PORTLIB, "--------------- ThreadStart Table ---------------\n");
	pool_do(_threadStartTable, &walkThreadStartTablePrint, _currentThread);
	j9tty_printf(PORTLIB, "total count %d\n", (int)_threadStartCount);

	j9tty_printf(PORTLIB, "--------------- ThreadPark Table ---------------\n");
	pool_do(_threadParkTable, &walkThreadParkTablePrint, _currentThread);
	j9tty_printf(PORTLIB, "total count %d\n", (int)_threadParkCount);

	j9tty_printf(PORTLIB, "--------------- ThreadGroup Table ---------------\n");
	hashTableForEachDo(_threadGroupTable, &walkThreadGroupTablePrint, _currentThread);

	j9tty_printf(PORTLIB, "--------------- StackTrace Table ---------------\n");
	hashTableForEachDo(_stackTraceTable, &walkStackTraceTablePrint, _currentThread);
}

void
VM_JFRConstantPoolTypes::printMergedStringTables()
{
	IDATA i = 1;
	IDATA max = 0;

	j9tty_printf(PORTLIB, "--------------- Global String Table ---------------\n");

	max += _stringUTF8Count;
	for (; i < max; i++) {
		StringUTF8Entry *tableEntry = (StringUTF8Entry *) _globalStringTable[i];

		j9tty_printf(PORTLIB, "%li -> ", i);
		j9tty_printf(PORTLIB, "%u) name=%.*s\n", tableEntry->index, J9UTF8_LENGTH(tableEntry->string), (char*) J9UTF8_DATA(tableEntry->string));
	}

	max += _packageCount;
	for (; i < max; i++) {
		PackageEntry *tableEntry = (PackageEntry *) _globalStringTable[i];

		j9tty_printf(PORTLIB, "%li -> ", i);
		j9tty_printf(
				PORTLIB,
				"%u) moduleIndex=%u packageName=%.*s exported=%u\n",
				tableEntry->index,
				tableEntry->moduleIndex,
				tableEntry->packageNameLength,
				(const char *)tableEntry->packageName,
				tableEntry->exported);
	}
}

UDATA
VM_JFRConstantPoolTypes::freeUTF8Strings(void *entry, void *userData)
{
	StringUTF8Entry *tableEntry = (StringUTF8Entry *) entry;
	J9VMThread *currentThread = (J9VMThread *)userData;
	PORT_ACCESS_FROM_VMC(currentThread);

	if (tableEntry->free) {
		j9mem_free_memory(tableEntry->string);
		tableEntry->string = NULL;
	}
	return FALSE;
}

UDATA
VM_JFRConstantPoolTypes::freeStackStraceEntries(void *entry, void *userData)
{
	StackTraceEntry *tableEntry = (StackTraceEntry *) entry;
	J9VMThread *currentThread = (J9VMThread *)userData;
	PORT_ACCESS_FROM_VMC(currentThread);

	j9mem_free_memory(tableEntry->frames);
	tableEntry->frames = NULL;

	return FALSE;
}

void
VM_JFRConstantPoolTypes::freeThreadNameEntries(void *entry, void *userData)
{
	ThreadEntry *tableEntry = (ThreadEntry *) entry;
	J9VMThread *currentThread = (J9VMThread *)userData;
	PORT_ACCESS_FROM_VMC(currentThread);

	/* Name of the unknown thread entry cannot be freed */
	if (tableEntry->freeName) {
		j9mem_free_memory(tableEntry->javaThreadName);
	}
	tableEntry->javaThreadName = NULL;
}

UDATA
VM_JFRConstantPoolTypes::freeThreadGroupNameEntries(void *entry, void *userData)
{
	ThreadGroupEntry *tableEntry = (ThreadGroupEntry *) entry;
	J9VMThread *currentThread = (J9VMThread *)userData;
	PORT_ACCESS_FROM_VMC(currentThread);

	j9mem_free_memory(tableEntry->name);
	tableEntry->name = NULL;

	return FALSE;
}

void
VM_JFRConstantPoolTypes::freeNetworkInterfaceNames()
{
	NetworkInterfaceNameEntry *entry = _firstNetworkInterfaceNameEntry;

	while (NULL != entry) {
		NetworkInterfaceNameEntry *next = entry->next;
		j9mem_free_memory(entry);
		entry = next;
	}
}

#endif /* defined(J9VM_OPT_JFR) */
