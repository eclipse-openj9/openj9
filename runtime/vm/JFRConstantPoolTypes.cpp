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
VM_JFRConstantPoolTypes::threadHashFn(void *key, void *userData)
{
	ThreadEntry *threadEntry = (ThreadEntry *) key;

	return (UDATA) threadEntry->vmThread;
}

UDATA
VM_JFRConstantPoolTypes::threadHashEqualFn(void *tableNode, void *queryNode, void *userData)
{
	ThreadEntry *tableEntry = (ThreadEntry *) tableNode;
	ThreadEntry *queryEntry = (ThreadEntry *) queryNode;

	return tableEntry->vmThread == queryEntry->vmThread;
}

UDATA
VM_JFRConstantPoolTypes::stackTraceHashFn(void *key, void *userData)
{
	StackTraceEntry *entry = (StackTraceEntry*) key;

	return (U_64)(UDATA)entry->vmThread ^ (U_64)entry->ticks;
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

UDATA
VM_JFRConstantPoolTypes::walkThreadTablePrint(void *entry, void *userData)
{
	ThreadEntry *tableEntry = (ThreadEntry *) entry;
	J9VMThread *currentThread = (J9VMThread *)userData;
	PORT_ACCESS_FROM_VMC(currentThread);

	j9tty_printf(PORTLIB, "%u) osTID=%lu javaTID=%lu javaThreadName=%.*s osThreadName=%.*s threadGroupIndex=%u\n", tableEntry->index, tableEntry->osTID, tableEntry->javaTID, J9UTF8_LENGTH(tableEntry->javaThreadName), J9UTF8_DATA(tableEntry->javaThreadName), J9UTF8_LENGTH(tableEntry->osThreadName), J9UTF8_DATA(tableEntry->osThreadName), tableEntry->threadGroupIndex);

	return FALSE;
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

	ClassEntry **newEntry = (ClassEntry**)pool_newElement(shallowEntries);
	*newEntry = tableEntry;

	return FALSE;
}

void
VM_JFRConstantPoolTypes::fixupShallowEntries(void *entry, void *userData)
{
	ClassEntry *tableEntry = *(ClassEntry **) entry;
	VM_JFRConstantPoolTypes *cp = (VM_JFRConstantPoolTypes*) userData;

	cp->getClassEntry(tableEntry->clazz);
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
	_buildResult = OK;

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
VM_JFRConstantPoolTypes::getClassEntry(J9Class *clazz)
{
	U_32 index = U_32_MAX;
	ClassEntry *entry = NULL;
	ClassEntry entryBuffer = {0};

	entry = &entryBuffer;
	entry->clazz = clazz;
	_buildResult = OK;

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

	entry->classLoaderIndex = addClassLoaderEntry(clazz->classLoader);
	if (isResultNotOKay()) goto done;

	entry->packageIndex = addPackageEntry(clazz);
	if (isResultNotOKay()) goto done;

	entry->modifiers = clazz->romClass->modifiers;
	entry->hidden = FALSE; //TODO

	if (NULL != entry && !entry->shallow) {
		entry->index = _classCount;
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
	const char *packageName = NULL;
	PackageEntry entryBuffer = {0};

	entry = &entryBuffer;
	_buildResult = OK;

	pkgID = hashPkgTableAt(clazz->classLoader, clazz->romClass);
	entry->romClass = clazz->romClass;
	entry->ramClass = clazz;

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

	packageName = (const char *) getPackageName(pkgID, &packageNameLength);
	if (NULL == packageName) {
		_buildResult = InternalVMError;
		goto done;
	}

	entry->packageName = (U_8*) packageName;
	entry->packageNameLength = packageNameLength;

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
VM_JFRConstantPoolTypes::addModuleEntry(J9Module *module)
{
	U_32 index = U_32_MAX;
	ModuleEntry *entry = NULL;
	ModuleEntry entryBuffer = {0};

	entry = &entryBuffer;
	entry->module = module;
	_buildResult = OK;

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
VM_JFRConstantPoolTypes::addClassLoaderEntry(J9ClassLoader *classLoader)
{
	U_32 index = U_32_MAX;
	ClassloaderEntry *entry = NULL;
	ClassloaderEntry entryBuffer = {0};
	j9object_t loaderName = NULL;

	entry = &entryBuffer;
	entry->classLoader = classLoader;
	_buildResult = OK;

	entry = (ClassloaderEntry *) hashTableFind(_classLoaderTable, entry);
	if (NULL != entry) {
		index = entry->index;
		goto done;
	} else {
		entry = &entryBuffer;
	}

	entry->classIndex = getShallowClassEntry(J9OBJECT_CLAZZ(_currentThread, classLoader->classLoaderObject));
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
	_buildResult = OK;

	entry = (ClassEntry *) hashTableFind(_classTable, entry);
	if (NULL != entry) {
		index = entry->index;
		goto done;
	} else {
		entry = &entryBuffer;
	}

	entry->index = _classCount;
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
	_buildResult = OK;

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

U_32
VM_JFRConstantPoolTypes::addThreadEntry(J9VMThread *vmThread)
{
	U_32 index = U_32_MAX;
	ThreadEntry *entry = NULL;
	ThreadEntry entryBuffer = {0};

	entry = &entryBuffer;
	entry->vmThread = vmThread;
	_buildResult = OK;
	omrthread_t osThread = vmThread->osThread;
	j9object_t threadObject = vmThread->threadObject;

	entry = (ThreadEntry *) hashTableFind(_threadTable, entry);
	if (NULL != entry) {
		index = entry->index;
		goto done;
	} else {
		entry = &entryBuffer;
	}

	entry->osTID = ((J9AbstractThread*)osThread)->tid;
	if (NULL != threadObject) {
		entry->javaTID = J9VMJAVALANGTHREAD_TID(_currentThread, threadObject);

		entry->javaThreadName = copyStringToJ9UTF8WithMemAlloc(_currentThread, J9VMJAVALANGTHREAD_NAME(_currentThread, threadObject), J9_STR_NONE, "", 0, NULL, 0);

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

	entry->index = _threadCount;
	_threadCount++;

	entry = (ThreadEntry*) hashTableAdd(_threadTable, &entryBuffer);
	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	if (NULL == _firstThreadEntry) {
		_firstThreadEntry = entry;
	}

	if (NULL != _previousThreadEntry) {
		_previousThreadEntry->next = entry;
	}
	_previousThreadEntry = entry;

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
	_buildResult = OK;

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
VM_JFRConstantPoolTypes::addStackTraceEntry(J9VMThread *vmThread, I_64 ticks, U_32 numOfFrames)
{
	U_32 index = U_32_MAX;
	StackTraceEntry *entry = NULL;
	StackTraceEntry entryBuffer = {0};

	entry = &entryBuffer;
	entry->vmThread = vmThread;
	entry->ticks = ticks;
	_buildResult = OK;

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

	entry->index = _stackTraceCount;
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

U_32
VM_JFRConstantPoolTypes::addExecutionSampleEntry(J9JFRExecutionSample *executionSampleData)
{
	ExecutionSampleEntry *entry = (ExecutionSampleEntry*)pool_newElement(_executionSampleTable);
	U_32 index = U_32_MAX;

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->vmThread = executionSampleData->vmThread;
	entry->ticks = executionSampleData->startTicks;
	entry->state = RUNNABLE; //TODO

	entry->threadIndex = addThreadEntry(entry->vmThread);
	if (isResultNotOKay()) goto done;

	entry->stackTraceIndex = consumeStackTrace(entry->vmThread, J9JFREXECUTIONSAMPLE_STACKTRACE(executionSampleData), executionSampleData->stackTraceSize);
	if (isResultNotOKay()) goto done;

	index = _executionSampleCount++;
	entry->index = index;

done:
	return index;
}

U_32
VM_JFRConstantPoolTypes::addThreadStartEntry(J9JFRThreadStart *threadStartData)
{
	ThreadStartEntry *entry = (ThreadStartEntry*)pool_newElement(_threadStartTable);
	U_32 index = U_32_MAX;

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = threadStartData->startTicks;

	entry->threadIndex = addThreadEntry(threadStartData->thread);
	if (isResultNotOKay()) goto done;

	entry->eventThreadIndex = addThreadEntry(threadStartData->thread);
	if (isResultNotOKay()) goto done;

	entry->parentThreadIndex = addThreadEntry(threadStartData->parentThread);
	if (isResultNotOKay()) goto done;

	entry->stackTraceIndex = consumeStackTrace(threadStartData->parentThread, J9JFRTHREADSTART_STACKTRACE(threadStartData), threadStartData->stackTraceSize);
	if (isResultNotOKay()) goto done;

	index = _threadStartCount;
	_threadStartCount += 1;

done:
	return index;
}

U_32
VM_JFRConstantPoolTypes::addThreadEndEntry(J9JFREvent *threadEndData)
{
	ThreadEndEntry *entry = (ThreadEndEntry*)pool_newElement(_threadEndTable);
	U_32 index = U_32_MAX;

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = threadEndData->startTicks;

	entry->threadIndex = addThreadEntry(threadEndData->vmThread);
	if (isResultNotOKay()) goto done;

	entry->eventThreadIndex = addThreadEntry(threadEndData->vmThread);
	if (isResultNotOKay()) goto done;

	index = _threadEndCount;
	_threadEndCount += 1;

done:
	return index;
}

U_32
VM_JFRConstantPoolTypes::addThreadSleepEntry(J9JFRThreadSlept *threadSleepData)
{
	ThreadSleepEntry *entry = (ThreadSleepEntry*)pool_newElement(_threadSleepTable);
	U_32 index = U_32_MAX;

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = threadSleepData->startTicks;
	entry->duration = threadSleepData->duration;
	entry->sleepTime = threadSleepData->time;

	entry->threadIndex = addThreadEntry(threadSleepData->vmThread);
	if (isResultNotOKay()) goto done;

	entry->eventThreadIndex = addThreadEntry(threadSleepData->vmThread);
	if (isResultNotOKay()) goto done;

	entry->stackTraceIndex = consumeStackTrace(threadSleepData->vmThread, J9JFRTHREADSLEPT_STACKTRACE(threadSleepData), threadSleepData->stackTraceSize);
	if (isResultNotOKay()) goto done;

	index = _threadEndCount;
	_threadEndCount += 1;

done:
	return index;
}

U_32
VM_JFRConstantPoolTypes::addMonitorWaitEntry(J9JFRMonitorWaited* threadWaitData)
{
	MonitorWaitEntry *entry = (MonitorWaitEntry*)pool_newElement(_monitorWaitTable);
	U_32 index = U_32_MAX;

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = threadWaitData->startTicks;
	entry->duration = threadWaitData->duration;
	entry->timeOut = threadWaitData->time;
	entry->monitorAddress = (I_64)(U_64)threadWaitData->monitorAddress;
	entry->timedOut = threadWaitData->timedOut;

	entry->threadIndex = addThreadEntry(threadWaitData->vmThread);
	if (isResultNotOKay()) goto done;

	entry->eventThreadIndex = addThreadEntry(threadWaitData->vmThread);
	if (isResultNotOKay()) goto done;

	entry->stackTraceIndex = consumeStackTrace(threadWaitData->vmThread, J9JFRMonitorWaitedED_STACKTRACE(threadWaitData), threadWaitData->stackTraceSize);
	if (isResultNotOKay()) goto done;

	entry->monitorClass = getClassEntry(threadWaitData->monitorClass);
	if (isResultNotOKay()) goto done;

	entry->notifierThread = 0; //Need a way to find the notifiying thread

	index = _threadEndCount;
	_threadEndCount += 1;

done:
	return index;
}

U_32
VM_JFRConstantPoolTypes::addCPULoadEntry(J9JFRCPULoad *cpuLoadData)
{
	CPULoadEntry *entry = (CPULoadEntry *)pool_newElement(_cpuLoadTable);
	U_32 index = U_32_MAX;

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = cpuLoadData->startTicks;
	entry->jvmUser = cpuLoadData->jvmUser;
	entry->jvmSystem = cpuLoadData->jvmSystem;
	entry->machineTotal = cpuLoadData->machineTotal;

	index = _cpuLoadCount;
	_cpuLoadCount += 1;

done:
	return index;
}

U_32
VM_JFRConstantPoolTypes::addThreadCPULoadEntry(J9JFRThreadCPULoad *threadCPULoadData)
{
	ThreadCPULoadEntry *entry = (ThreadCPULoadEntry *)pool_newElement(_threadCPULoadTable);
	U_32 index = U_32_MAX;

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = threadCPULoadData->startTicks;
	entry->userCPULoad = threadCPULoadData->userCPULoad;
	entry->systemCPULoad = threadCPULoadData->systemCPULoad;

	entry->threadIndex = addThreadEntry(threadCPULoadData->vmThread);
	if (isResultNotOKay()) {
		goto done;
	}

	index = _threadCPULoadCount;
	_threadCPULoadCount += 1;

done:
	return index;
}

U_32
VM_JFRConstantPoolTypes::addClassLoadingStatisticsEntry(J9JFRClassLoadingStatistics *classLoadingStatisticsData)
{
	ClassLoadingStatisticsEntry *entry = (ClassLoadingStatisticsEntry *)pool_newElement(_classLoadingStatisticsTable);
	U_32 index = U_32_MAX;

	if (NULL == entry) {
		_buildResult = OutOfMemory;
		goto done;
	}

	entry->ticks = classLoadingStatisticsData->startTicks;
	entry->loadedClassCount = classLoadingStatisticsData->loadedClassCount;
	entry->unloadedClassCount = classLoadingStatisticsData->unloadedClassCount;

	index = _classLoadingStatisticsCount;
	_classLoadingStatisticsCount += 1;

done:
	return index;
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
	hashTableForEachDo(_threadTable, &walkThreadTablePrint, _currentThread);

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
		j9tty_printf(PORTLIB, "%u) moduleIndex=%u packageName=%.*s exported=%u\n", tableEntry->index, tableEntry->moduleIndex, tableEntry->packageNameLength, (char*)tableEntry->packageName, tableEntry->exported);
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

UDATA
VM_JFRConstantPoolTypes::freeThreadNameEntries(void *entry, void *userData)
{
	ThreadEntry *tableEntry = (ThreadEntry *) entry;
	J9VMThread *currentThread = (J9VMThread *)userData;
	PORT_ACCESS_FROM_VMC(currentThread);

	/* Name of the unknown thread entry cannot be freed */
	if (0 != tableEntry->index) {
		j9mem_free_memory(tableEntry->javaThreadName);
	}
	tableEntry->javaThreadName = NULL;

	return FALSE;
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


#endif /* defined(J9VM_OPT_JFR) */
