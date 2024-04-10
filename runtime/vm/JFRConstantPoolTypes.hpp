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
#if !defined(JFRCONSTANTPOOLTYPES_HPP_)
#define JFRCONSTANTPOOLTYPES_HPP_

#include "j9cfg.h"
#include "j9.h"
#include "omrlinkedlist.h"
#include "vm_api.h"
#include "vm_internal.h"
#include "ut_j9vm.h"

#if defined(J9VM_OPT_JFR)

#include "BufferWriter.hpp"
#include "JFRUtils.hpp"
#include "ObjectAccessBarrierAPI.hpp"
#include "VMHelpers.hpp"

J9_DECLARE_CONSTANT_UTF8(nullString, "(nullString)");
J9_DECLARE_CONSTANT_UTF8(defaultPackage, "(defaultPackage)");
J9_DECLARE_CONSTANT_UTF8(bootLoaderName, "boostrapClassLoader");

enum FrameType {
	Interpreted = 0,
	JIT,
	JIT_Inline,
	Native,
	FrameTypeCount,
};

enum ThreadState {
	NEW = 0,
	TERMINATED,
	RUNNABLE,
	SLEEPING,
	WAITING,
	TIMED_WAITING,
	PARKED,
	TIMED_PARKED,
	BLOCKED,
	THREADSTATE_COUNT,
};

struct ClassEntry {
	J9Class *clazz;
	U_32 classLoaderIndex;
	U_32 nameStringUTF8Index;
	U_32 packageIndex;
	I_32 modifiers;
	BOOLEAN hidden;
	U_32 index;
	J9Class *shallow;
	ClassEntry *next;
};

struct PackageEntry {
	J9PackageIDTableEntry *pkgID;
	U_32 moduleIndex;
	BOOLEAN exported;
	U_32 packageNameLength;
	U_8* packageName;
	U_32 index;
	PackageEntry *next;
};

struct ModuleEntry {
	J9Module *module;
	U_32 nameStringIndex;
	U_32 versionStringIndex;
	U_32 locationStringUTF8Index;
	U_32 classLoaderIndex;
	U_32 index;
	ModuleEntry *next;
};

struct ClassloaderEntry {
	J9ClassLoader *classLoader;
	U_32 classIndex;
	U_32 nameIndex;
	U_32 index;
	ClassloaderEntry *next;
};

struct MethodEntry {
	J9ROMMethod *romMethod;
	U_32 classIndex;
	U_32 nameStringUTF8Index;
	U_32 descriptorStringUTF8Index;
	U_32 modifiers;
	BOOLEAN hidden;
	U_32 index;
	MethodEntry *next;
};

struct StringUTF8Entry {
	J9UTF8 *string;
	BOOLEAN free;
	U_32 index;
};

struct ThreadEntry {
	J9VMThread *vmThread;
	U_32 index;
	U_64 osTID;
	U_64 javaTID;
	J9UTF8 *javaThreadName;
	J9UTF8 *osThreadName;
	U_32 threadGroupIndex;
	ThreadEntry *next;
};

struct ThreadGroupEntry {
	j9object_t threadGroupName;
	U_32 index;
	U_32 parentIndex;
	J9UTF8* name;
	ThreadGroupEntry *next;
};

struct StackFrame {
	U_32 methodIndex;
	I_32 lineNumber;
	I_32 bytecodeIndex;
	FrameType frameType;
};

struct ExecutionSampleEntry {
	J9VMThread *vmThread;
	I_64 time;
	ThreadState state;
	U_32 stackTraceIndex;
	U_32 threadIndex;
	U_32 index;
};

struct ThreadStartEntry {
	I_64 time;
	U_32 stackTraceIndex;
	U_32 threadIndex;
	U_32 eventThreadIndex;
	U_32 parentThreadIndex;
};

struct ThreadEndEntry {
	I_64 time;
	U_32 threadIndex;
	U_32 eventThreadIndex;
};

struct ThreadSleepEntry {
	I_64 time;
	I_64 duration;
	U_32 threadIndex;
	U_32 eventThreadIndex;
	U_32 stackTraceIndex;
};

struct StackTraceEntry {
	J9VMThread *vmThread;
	I_64 time;
	U_32 numOfFrames;
	U_32 index;
	StackFrame *frames;
	BOOLEAN truncated;
	StackTraceEntry *next;
};

class VM_JFRConstantPoolTypes {
		/*
	 * Data members
	 */
private:
	J9VMThread *_currentThread;
	J9JavaVM *_vm;
	BuildResult _buildResult;
	bool _debug;
	J9PortLibrary *privatePortLibrary;

	/* Constantpool types */
	J9HashTable *_classTable;
	J9HashTable *_packageTable;
	J9HashTable *_moduleTable;
	J9HashTable *_classLoaderTable;
	J9HashTable *_methodTable;
	J9HashTable *_stackTraceTable;
	J9HashTable *_stringUTF8Table;
	J9HashTable *_threadTable;
	J9HashTable *_threadGroupTable;
	U_32 _classCount;
	U_32 _packageCount;
	U_32 _moduleCount;
	U_32 _classLoaderCount;
	U_32 _methodCount;
	U_32 _stackTraceCount;
	U_32 _stackFrameCount;
	U_32 _stringUTF8Count;
	U_32 _threadCount;
	U_32 _threadGroupCount;
	U_32 _packageNameCount;

	/* Event Types */
	J9Pool *_executionSampleTable;
	UDATA _executionSampleCount;
	J9Pool *_threadStartTable;
	UDATA _threadStartCount;
	J9Pool *_threadEndTable;
	UDATA _threadEndCount;
	J9Pool *_threadSleepTable;
	UDATA _threadSleepCount;

	/* Processing buffers */
	StackFrame *_currentStackFrameBuffer;
	StackTraceEntry *_previousStackTraceEntry;
	StackTraceEntry *_firstStackTraceEntry;
	ThreadEntry *_previousThreadEntry;
	ThreadEntry *_firstThreadEntry;
	ThreadGroupEntry *_previousThreadGroupEntry;
	ThreadGroupEntry *_firstThreadGroupEntry;
	ModuleEntry *_previousModuleEntry;
	ModuleEntry *_firstModuleEntry;
	MethodEntry *_previousMethodEntry;
	MethodEntry *_firstMethodEntry;
	ClassEntry *_previousClassEntry;
	ClassEntry *_firstClassEntry;
	ClassloaderEntry *_previousClassloaderEntry;
	ClassloaderEntry *_firstClassloaderEntry;
	PackageEntry *_previousPackageEntry;
	PackageEntry *_firstPackageEntry;

	/* default values */
	ThreadGroupEntry _defaultThreadGroup;
	StringUTF8Entry _defaultStringUTF8Entry;
	PackageEntry _defaultPackageEntry;
	ModuleEntry _defaultModuleEntry;

	UDATA _requiredBufferSize;
	U_32 _currentFrameCount;
	void **_globalStringTable;

protected:

public:
	static constexpr int STRING_BUFFER_LENGTH = 128;

	/*
	 * Function members
	 */
private:
	static UDATA classloaderNameHashFn(void *key, void *userData);

	static UDATA classloaderNameHashEqualFn(void *tableNode, void *queryNode, void *userData);

	static UDATA methodNameHashFn(void *key, void *userData);

	static UDATA methodNameHashEqualFn(void *tableNode, void *queryNode, void *userData);

	static UDATA jfrStringHashFn(void *key, void *userData);

	static UDATA jfrStringHashEqualFn(void *tableNode, void *queryNode, void *userData);

	static UDATA threadHashFn(void *key, void *userData);

	static UDATA threadHashEqualFn(void *tableNode, void *queryNode, void *userData);

	static UDATA jfrClassHashFn(void *key, void *userData);

	static UDATA jfrClassHashEqualFn(void *tableNode, void *queryNode, void *userData);

	static UDATA jfrPackageHashFn(void *key, void *userData);

	static UDATA jfrPackageHashEqualFn(void *tableNode, void *queryNode, void *userData);

	static UDATA jfrStringUTF8HashEqualFn(void *tableNode, void *queryNode, void *userData);

	static UDATA jfrStringUTF8HashFn(void *key, void *userData);

	static UDATA jfrModuleHashEqualFn(void *tableNode, void *queryNode, void *userData);

	static UDATA jfrModuleHashFn(void *key, void *userData);

	static UDATA stackTraceHashEqualFn(void *tableNode, void *queryNode, void *userData);

	static UDATA stackTraceHashFn(void *key, void *userData);

	static UDATA threadGroupHashEqualFn(void *tableNode, void *queryNode, void *userData);

	static UDATA threadGroupHashFn(void *key, void *userData);

	static UDATA walkStringTablePrint(void *entry, void *userData);

	static UDATA walkStringUTF8TablePrint(void *entry, void *userData);

	static UDATA walkClassesTablePrint(void *entry, void *userData);

	static UDATA walkClassLoadersTablePrint(void *entry, void *userData);

	static UDATA walkThreadTablePrint(void *entry, void *userData);

	static UDATA walkThreadGroupTablePrint(void *entry, void *userData);

	static UDATA walkStackTraceTablePrint(void *entry, void *userData);

	static UDATA fixupShallowEntries(void *entry, void *userData);

	static UDATA walkMethodTablePrint(void *entry, void *userData);

	static UDATA walkModuleTablePrint(void *entry, void *userData);

	static UDATA walkPackageTablePrint(void *entry, void *userData);

	static UDATA mergeStringEntriesToGlobalTable(void *entry, void *userData);

	static UDATA mergeStringUTF8EntriesToGlobalTable(void *entry, void *userData);

	static UDATA mergePackageEntriesToGlobalTable(void *entry, void *userData);

	static UDATA freeUTF8Strings(void *entry, void *userData);

	U_32 getMethodEntry(J9ROMMethod *romMethod, J9Class *ramClass);

	U_32 getClassEntry(J9Class *clazz);

	U_32 addPackageEntry(J9Class *clazz);

	U_32 addModuleEntry(J9Module *module);

	U_32 addClassLoaderEntry(J9ClassLoader *classLoader);

	/*
	 * Adds class to the table but doesnt fill out fields to avoid
	 * circularities.
	 */
	U_32 getShallowClassEntry(J9Class *clazz);

	U_32 addStringEntry(j9object_t string);

	U_32 addStringUTF8Entry(J9UTF8 *string);

	U_32 addStringUTF8Entry(J9UTF8 *string, bool free);

	U_32 addThreadEntry(J9VMThread *vmThread);

	U_32 addThreadGroupEntry(j9object_t threadGroup);

	U_32 addStackTraceEntry(J9VMThread *vmThread, I_64 time, U_32 numOfFrames);

	void printMergedStringTables();

	bool isResultNotOKay() {
		if (_buildResult != OK) {
			if (_debug) {
				printf("failure!!!\n");
			}
			return true;
		}
		return false;
	}

	static UDATA stackTraceCallback(J9VMThread *vmThread, void *userData, UDATA bytecodeOffset, J9ROMClass *romClass, J9ROMMethod *romMethod, J9UTF8 *fileName, UDATA lineNumber, J9ClassLoader *classLoader, J9Class *ramClass)
	{
		VM_JFRConstantPoolTypes *cp = (VM_JFRConstantPoolTypes*) userData;
		StackFrame *frame = &cp->_currentStackFrameBuffer[cp->_currentFrameCount];

		if ((UDATA)-1 != bytecodeOffset) {
			cp->_currentFrameCount++;

			frame->methodIndex = cp->getMethodEntry(romMethod, ramClass);
			frame->lineNumber = lineNumber;
			frame->bytecodeIndex = bytecodeOffset;
			frame->frameType = Interpreted; /* TODO need a way to know if its JIT'ed and inlined */
		}
		return J9_STACKWALK_KEEP_ITERATING;
	}

	void mergeStringTables() {
		_buildResult = OK;

		_globalStringTable = (void**)j9mem_allocate_memory(sizeof(void*) * (_stringUTF8Count + _packageCount), J9MEM_CATEGORY_CLASSES);
		if (NULL == _globalStringTable) {
			_buildResult = OutOfMemory;
			goto done;
		}
		_globalStringTable[0] = &_defaultStringUTF8Entry;
		_globalStringTable[_stringUTF8Count] = &_defaultPackageEntry;

		hashTableForEachDo(_stringUTF8Table, &mergeStringUTF8EntriesToGlobalTable, this);
		hashTableForEachDo(_packageTable, &mergePackageEntriesToGlobalTable, this);

		if (_debug) {
			printMergedStringTables();
		}
done:
		return;
	}

protected:

public:

	U_32 addExecutionSampleEntry(J9JFRExecutionSample *executionSampleData);

	U_32 addThreadStartEntry(J9JFRThreadStart *threadStartData);

	U_32 addThreadEndEntry(J9JFREvent *threadEndData);

	U_32 addThreadSleepEntry(J9JFRThreadSleep *threadSleepData);

	J9Pool *getExecutionSampleTable()
	{
		return _executionSampleTable;
	}

	J9Pool *getThreadStartTable()
	{
		return _threadStartTable;
	}

	J9Pool *getThreadEndTable()
	{
		return _threadEndTable;
	}

	J9Pool *getThreadSleepTable()
	{
		return _threadSleepTable;
	}

	UDATA getExecutionSampleCount()
	{
		return _executionSampleCount;
	}

	UDATA getThreadStartCount()
	{
		return _threadStartCount;
	}

	UDATA getThreadEndCount()
	{
		return _threadEndCount;
	}

	UDATA getThreadSleepCount()
	{
		return _threadSleepCount;
	}

	ClassloaderEntry *getClassloaderEntry()
	{
		return _firstClassloaderEntry;
	}

	UDATA getClassloaderCount()
	{
		return _classLoaderCount;
	}

	ClassEntry *getClassEntry()
	{
		return _firstClassEntry;
	}

	UDATA getClassCount()
	{
		return _classCount;
	}

	ModuleEntry *getModuleEntry()
	{
		return _firstModuleEntry;
	}

	UDATA getModuleCount()
	{
		return _moduleCount;
	}

	MethodEntry *getMethodEntry()
	{
		return _firstMethodEntry;
	}

	UDATA getMethodCount()
	{
		return _methodCount;
	}

	void *getSymbolTableEntry(UDATA index)
	{
		return _globalStringTable[index];
	}

	UDATA getSymbolTableCount()
	{
		return _stringUTF8Count + _packageCount;
	}

	UDATA getStringUTF8Count()
	{
		return _stringUTF8Count;
	}

	PackageEntry *getPackageEntry()
	{
		return _firstPackageEntry;
	}

	UDATA getPackageCount()
	{
		return _packageCount;
	}

	UDATA getRequiredBufferSize()
	{
		return _requiredBufferSize;
	}

	UDATA getThreadGroupCount()
	{
		return _threadGroupCount;
	}

	ThreadGroupEntry *getThreadGroupEntry()
	{
		return _firstThreadGroupEntry;
	}

	UDATA getThreadCount()
	{
		return _threadCount;
	}

	ThreadEntry *getThreadEntry()
	{
		return _firstThreadEntry;
	}

	UDATA getStackTraceCount()
	{
		return _stackTraceCount;
	}

	StackTraceEntry *getStackTraceEntry()
	{
		return _firstStackTraceEntry;
	}

	UDATA getStackFrameCount()
	{
		return _stackFrameCount;
	}

	void printTables();

	BuildResult getBuildResult() { return _buildResult; };

	void loadEvents()
	{
		J9JFRBufferWalkState walkstate = {0};
		J9JFREvent *event = jfrBufferStartDo(&_vm->jfrBuffer, &walkstate);

		while (NULL != event) {
			switch (event->eventType) {
			case J9JFR_EVENT_TYPE_EXECUTION_SAMPLE:
				addExecutionSampleEntry((J9JFRExecutionSample*) event);
				break;
			case J9JFR_EVENT_TYPE_THREAD_START:
				addThreadStartEntry((J9JFRThreadStart*) event);
				break;
			case J9JFR_EVENT_TYPE_THREAD_END:
				addThreadEndEntry((J9JFREvent*) event);
				break;
			case J9JFR_EVENT_TYPE_THREAD_SLEEP:
				addThreadSleepEntry((J9JFRThreadSleep*) event);
				break;
			default:
				Assert_VM_unreachable();
			break;
			}
			event = jfrBufferNextDo(&walkstate);
		}


		hashTableForEachDo(_classTable, &fixupShallowEntries, this);
		mergeStringTables();
	}

	U_32 consumeStackTrace(J9VMThread *walkThread, UDATA *walkStateCache, UDATA numberOfFrames) {
		U_32 index = U_32_MAX;
		_currentStackFrameBuffer = (StackFrame*) j9mem_allocate_memory(sizeof(StackFrame) * numberOfFrames, J9MEM_CATEGORY_CLASSES);
		_currentFrameCount = 0;
		if (NULL == _currentStackFrameBuffer) {
			_buildResult = OutOfMemory;
			goto done;
		}

		iterateStackTraceImpl(_currentThread, (j9object_t*)walkStateCache, &stackTraceCallback, this, FALSE, FALSE, numberOfFrames, FALSE);

		index = addStackTraceEntry(walkThread, VM_JFRUtils::getCurrentTimeNanos(privatePortLibrary, _buildResult), _currentFrameCount);
		_stackFrameCount += numberOfFrames;

done:
		return index;
	}

	VM_JFRConstantPoolTypes(J9VMThread *currentThread)
		: _currentThread(currentThread)
		, _vm(currentThread->javaVM)
		, _buildResult(OK)
		, _debug(false)
		, privatePortLibrary(_vm->portLibrary)
		, _classTable(NULL)
		, _packageTable(NULL)
		, _moduleTable(NULL)
		, _classLoaderTable(NULL)
		, _methodTable(NULL)
		, _stackTraceTable(NULL)
		, _stringUTF8Table(NULL)
		, _threadTable(NULL)
		, _threadGroupTable(NULL)
		, _classCount(0)
		, _packageCount(0)
		, _moduleCount(0)
		, _classLoaderCount(0)
		, _methodCount(0)
		, _stackTraceCount(0)
		, _stackFrameCount(0)
		, _stringUTF8Count(0)
		, _threadCount(0)
		, _threadGroupCount(0)
		, _executionSampleTable(NULL)
		, _executionSampleCount(0)
		, _threadStartTable(NULL)
		, _threadStartCount(0)
		, _threadEndTable(NULL)
		, _threadEndCount(0)
		, _threadSleepTable(NULL)
		, _threadSleepCount(0)
		, _previousStackTraceEntry(NULL)
		, _firstStackTraceEntry(NULL)
		, _previousThreadEntry(NULL)
		, _firstThreadEntry(NULL)
		, _previousThreadGroupEntry(NULL)
		, _firstThreadGroupEntry(NULL)
		, _previousModuleEntry(NULL)
		, _firstModuleEntry(NULL)
		, _previousMethodEntry(NULL)
		, _firstMethodEntry(NULL)
		, _previousClassEntry(NULL)
		, _firstClassEntry(NULL)
		, _previousClassloaderEntry(NULL)
		, _firstClassloaderEntry(NULL)
		, _previousPackageEntry(NULL)
		, _firstPackageEntry(NULL)
		, _requiredBufferSize(0)
	{
		_classTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(ClassEntry), sizeof(ClassEntry *), J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION, J9MEM_CATEGORY_CLASSES, jfrClassHashFn, jfrClassHashEqualFn, NULL, _vm);
		if (NULL == _classTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_packageTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(PackageEntry), sizeof(PackageEntry *), J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION, J9MEM_CATEGORY_CLASSES, jfrPackageHashFn, jfrPackageHashEqualFn, NULL, _vm);
		if (NULL == _packageTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_classLoaderTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(ClassloaderEntry), sizeof(J9ClassLoader*), J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION, J9MEM_CATEGORY_CLASSES, classloaderNameHashFn, classloaderNameHashEqualFn, NULL, _vm);
		if (NULL == _classLoaderTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_methodTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(MethodEntry), sizeof(J9ROMMethod*), J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION, J9MEM_CATEGORY_CLASSES, methodNameHashFn, methodNameHashEqualFn, NULL, _vm);
		if (NULL == _methodTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_stringUTF8Table = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(StringUTF8Entry), sizeof(StringUTF8Entry*), J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION, J9MEM_CATEGORY_CLASSES, jfrStringUTF8HashFn, jfrStringUTF8HashEqualFn, NULL, _vm);
		if (NULL == _stringUTF8Table) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_moduleTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(ModuleEntry), sizeof(ModuleEntry*), J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION, J9MEM_CATEGORY_CLASSES, jfrModuleHashFn, jfrModuleHashEqualFn, NULL, _vm);
		if (NULL == _moduleTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_threadTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(ThreadEntry), sizeof(U_64), J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION, J9MEM_CATEGORY_CLASSES, threadHashFn, threadHashEqualFn, NULL, _currentThread);
		if (NULL == _threadTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_stackTraceTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(StackTraceEntry), sizeof(U_64), J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION, J9MEM_CATEGORY_CLASSES, stackTraceHashFn, stackTraceHashEqualFn, NULL, _vm);
		if (NULL == _stackTraceTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_threadGroupTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(ThreadGroupEntry), sizeof(U_64), J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION, J9MEM_CATEGORY_CLASSES, threadGroupHashFn, threadGroupHashEqualFn, NULL, _vm);
		if (NULL == _threadGroupTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_executionSampleTable = pool_new(sizeof(ExecutionSampleEntry), 0, sizeof(U_64), 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(privatePortLibrary));
		if (NULL == _executionSampleTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_threadStartTable = pool_new(sizeof(ThreadStartEntry), 0, sizeof(U_64), 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(privatePortLibrary));
		if (NULL == _threadStartTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_threadEndTable = pool_new(sizeof(ThreadEndEntry), 0, sizeof(U_64), 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(privatePortLibrary));
		if (NULL == _threadEndTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_threadSleepTable = pool_new(sizeof(ThreadSleepEntry), 0, sizeof(U_64), 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(privatePortLibrary));
		if (NULL == _threadEndTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		/* Add reserved index for default entries. For strings zero is the empty or NUll string.
		 * For package zero is the deafult package, for Module zero is the unnamed module. ThreadGroup
		 * zero is NULL threadGroup.
		 */
		_stringUTF8Count++;
		_defaultStringUTF8Entry = {0};
		_defaultStringUTF8Entry.string = (J9UTF8*)&nullString;

		_moduleCount++;
		_defaultModuleEntry = {0};
		_firstModuleEntry = &_defaultModuleEntry;
		_previousModuleEntry = _firstModuleEntry;

		_packageCount++;
		_defaultPackageEntry = {0};
		_defaultPackageEntry.exported = TRUE;
		_defaultPackageEntry.packageName = J9UTF8_DATA((J9UTF8*) &defaultPackage);
		_defaultPackageEntry.packageNameLength = J9UTF8_LENGTH((J9UTF8*) &defaultPackage);
		_firstPackageEntry = &_defaultPackageEntry;
		_previousPackageEntry = _firstPackageEntry;

		_threadGroupCount++;
		_defaultThreadGroup = {0};
		_firstThreadGroupEntry = &_defaultThreadGroup;
		_previousThreadGroupEntry = _firstThreadGroupEntry;


done:
		return;
	}

	~VM_JFRConstantPoolTypes()
	{
		hashTableForEachDo(_stringUTF8Table, &freeUTF8Strings, _currentThread);
		hashTableFree(_classTable);
		hashTableFree(_packageTable);
		hashTableFree(_moduleTable);
		hashTableFree(_classLoaderTable);
		hashTableFree(_methodTable);
		hashTableFree(_stackTraceTable);
		hashTableFree(_threadTable);
		hashTableFree(_threadGroupTable);
		hashTableFree(_stringUTF8Table);
		pool_kill(_executionSampleTable);
		pool_kill(_threadStartTable);
		pool_kill(_threadEndTable);
		pool_kill(_threadSleepTable);
		j9mem_free_memory(_globalStringTable);
	}

};
#endif /* defined(J9VM_OPT_JFR) */
#endif /* JFRCONSTANTPOOLTYPES_HPP_ */
