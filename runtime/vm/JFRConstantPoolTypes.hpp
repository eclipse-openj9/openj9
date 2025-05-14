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

#if defined(J9VM_OPT_JFR)

#include "j9hypervisor.h"
#include "j9.h"
#include "omr.h"
#include "omrlinkedlist.h"
#include "vm_api.h"
#include "vm_internal.h"
#include "ut_j9vm.h"
#include "BufferWriter.hpp"
#include "JFRUtils.hpp"
#include "ObjectAccessBarrierAPI.hpp"
#include "VMHelpers.hpp"

#include <cstring>

J9_DECLARE_CONSTANT_UTF8(nullString, "(nullString)");
J9_DECLARE_CONSTANT_UTF8(unknownClass, "(defaultPackage)/(unknownClass)");
J9_DECLARE_CONSTANT_UTF8(nativeMethod, "(nativeMethod)");
J9_DECLARE_CONSTANT_UTF8(nativeMethodSignature, "()");
J9_DECLARE_CONSTANT_UTF8(defaultPackage, "");
J9_DECLARE_CONSTANT_UTF8(bootLoaderName, "boostrapClassLoader");
J9_DECLARE_CONSTANT_UTF8(unknownThread, "unknown thread");

enum JFRStringConstants {
	DefaultString = 0,
	UnknownClass = 1,
	NativeMethod = 2,
	NativeMethodSignature = 3,
};

enum FrameType {
	Interpreted = J9VM_STACK_FRAME_INTERPRETER,
	JIT = J9VM_STACK_FRAME_JIT,
	JIT_Inline = J9VM_STACK_FRAME_JIT_INLINE,
	Native = J9VM_STACK_FRAME_NATIVE,
	FrameTypeCount,
};

enum OOPModeType {
	ZeroBased = 0,
	OOPModeTypeCount,
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
	J9ROMClass *romClass;
	J9Class *ramClass;
	U_32 moduleIndex;
	BOOLEAN exported;
	U_32 packageNameLength;
	const U_8 *packageName;
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
	I_64 ticks;
	ThreadState state;
	U_32 stackTraceIndex;
	U_32 threadIndex;
};

struct ThreadStartEntry {
	I_64 ticks;
	U_32 stackTraceIndex;
	U_32 threadIndex;
	U_32 eventThreadIndex;
	U_32 parentThreadIndex;
};

struct ThreadEndEntry {
	I_64 ticks;
	U_32 threadIndex;
	U_32 eventThreadIndex;
};

struct ThreadSleepEntry {
	I_64 ticks;
	I_64 duration;
	I_64 sleepTime;
	U_32 threadIndex;
	U_32 eventThreadIndex;
	U_32 stackTraceIndex;
};

struct MonitorWaitEntry {
	I_64 ticks;
	I_64 duration;
	I_64 timeOut;
	I_64 monitorAddress;
	U_32 monitorClass;
	U_32 notifierThread;
	U_32 threadIndex;
	U_32 eventThreadIndex;
	U_32 stackTraceIndex;
	BOOLEAN timedOut;
};

struct MonitorEnterEntry {
	I_64 ticks;
	I_64 duration;
	I_64 monitorAddress;
	U_32 monitorClass;
	U_32 previousOwnerThread;
	U_32 threadIndex;
	U_32 eventThreadIndex;
	U_32 stackTraceIndex;
};

struct ThreadParkEntry {
	I_64 ticks;
	I_64 duration;
	U_32 threadIndex;
	U_32 eventThreadIndex;
	U_32 stackTraceIndex;
	U_32 parkedClass;
	I_64 timeOut;
	I_64 untilTime;
	U_64 parkedAddress;
};

struct StackTraceEntry {
	J9VMThread *vmThread;
	I_64 ticks;
	U_32 numOfFrames;
	U_32 index;
	StackFrame *frames;
	BOOLEAN truncated;
	StackTraceEntry *next;
};

struct CPULoadEntry {
	I_64 ticks;
	float jvmUser;
	float jvmSystem;
	float machineTotal;
};

struct ThreadCPULoadEntry {
	I_64 ticks;
	U_32 threadIndex;
	float userCPULoad;
	float systemCPULoad;
};

struct ClassLoadingStatisticsEntry {
	I_64 ticks;
	I_64 loadedClassCount;
	I_64 unloadedClassCount;
};

struct ThreadContextSwitchRateEntry {
	I_64 ticks;
	float switchRate;
};

struct ThreadStatisticsEntry {
	I_64 ticks;
	U_64 activeThreadCount;
	U_64 daemonThreadCount;
	U_64 accumulatedThreadCount;
	U_64 peakThreadCount;
};

struct JVMInformationEntry {
	const char *jvmName;
	const char *jvmVersion;
	char *jvmArguments;
	const char *jvmFlags;
	const char *javaArguments;
	I_64 jvmStartTime;
	I_64 pid;
};

struct SystemProcessEntry {
	I_64 ticks;
	UDATA pid;
	char *commandLine;
};

struct CPUInformationEntry {
	const char *cpu;
	char *description;
	U_32 sockets;
	U_32 cores;
	U_32 hwThreads;
};

struct GCHeapConfigurationEntry {
	U_64 minSize;
	U_64 maxSize;
	U_64 initialSize;
	BOOLEAN usesCompressedOops;
	OOPModeType compressedOopsMode;
	U_64 objectAlignment;
	UDATA heapAddressBits;
};

struct YoungGenerationConfigurationEntry {
	U_64 minSize;
	U_64 maxSize;
	U_64 newRatio;
};

struct VirtualizationInformationEntry {
	const char *name;
};

struct OSInformationEntry {
	char *osVersion;
};

struct NativeLibraryEntry {
	I_64 ticks;
	char *name;
	UDATA addressLow;
	UDATA addressHigh;
	NativeLibraryEntry *next;
};

struct JFRConstantEvents {
	JVMInformationEntry JVMInfoEntry;
	CPUInformationEntry CPUInfoEntry;
	VirtualizationInformationEntry VirtualizationInfoEntry;
	OSInformationEntry OSInfoEntry;
	GCHeapConfigurationEntry GCHeapConfigEntry;
	YoungGenerationConfigurationEntry YoungGenConfigEntry;
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
	J9Pool *_monitorWaitTable;
	UDATA _monitorWaitCount;
	J9Pool *_monitorEnterTable;
	UDATA _monitorEnterCount;
	J9Pool *_threadParkTable;
	UDATA _threadParkCount;
	J9Pool *_cpuLoadTable;
	UDATA _cpuLoadCount;
	J9Pool *_threadCPULoadTable;
	UDATA _threadCPULoadCount;
	J9Pool *_classLoadingStatisticsTable;
	UDATA _classLoadingStatisticsCount;
	J9Pool *_threadContextSwitchRateTable;
	UDATA _threadContextSwitchRateCount;
	J9Pool *_threadStatisticsTable;
	UDATA _threadStatisticsCount;
	J9Pool *_systemProcessTable;
	UDATA _systemProcessCount;
	UDATA _systemProcessStringSizeTotal;
	J9Pool *_nativeLibrariesTable;
	UDATA _nativeLibrariesCount;
	UDATA _nativeLibraryPathSizeTotal;

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
	NativeLibraryEntry *_firstNativeLibraryEntry;
	NativeLibraryEntry *_previousNativeLibraryEntry;

	/* default values */
	ThreadGroupEntry _defaultThreadGroup;
	StringUTF8Entry _defaultStringUTF8Entry;
	StringUTF8Entry _unknownClassStringUTF8Entry;
	StringUTF8Entry _nativeMethodStringUTF8Entry;
	StringUTF8Entry _nativeMethodSignatureStringUTF8Entry;
	PackageEntry _defaultPackageEntry;
	ModuleEntry _defaultModuleEntry;
	ClassEntry _defaultClassEntry;
	MethodEntry _defaultMethodEntry;
	StackTraceEntry _defaultStackTraceEntry;

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

	static UDATA findShallowEntries(void *entry, void *userData);

	static void fixupShallowEntries(void *anElement, void *userData);

	static UDATA walkMethodTablePrint(void *entry, void *userData);

	static UDATA walkModuleTablePrint(void *entry, void *userData);

	static UDATA walkPackageTablePrint(void *entry, void *userData);

	static UDATA mergeStringEntriesToGlobalTable(void *entry, void *userData);

	static UDATA mergeStringUTF8EntriesToGlobalTable(void *entry, void *userData);

	static UDATA mergePackageEntriesToGlobalTable(void *entry, void *userData);

	static UDATA freeUTF8Strings(void *entry, void *userData);

	static UDATA freeStackStraceEntries(void *entry, void *userData);

	static UDATA freeThreadNameEntries(void *entry, void *userData);

	static UDATA freeThreadGroupNameEntries(void *entry, void *userData);

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

	U_32 addStackTraceEntry(J9VMThread *vmThread, I_64 ticks, U_32 numOfFrames);

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

	static UDATA stackTraceCallback(J9VMThread *vmThread, void *userData, UDATA bytecodeOffset, J9ROMClass *romClass, J9ROMMethod *romMethod, J9UTF8 *fileName, UDATA lineNumber, J9ClassLoader *classLoader, J9Class *ramClass, UDATA frameType)
	{
		VM_JFRConstantPoolTypes *cp = (VM_JFRConstantPoolTypes*) userData;
		StackFrame *frame = &cp->_currentStackFrameBuffer[cp->_currentFrameCount];

		if ((NULL == ramClass) || (NULL == romMethod)) {
			goto skipFrame;
		} else {
			frame->methodIndex = cp->getMethodEntry(romMethod, ramClass);
			frame->frameType = (FrameType) frameType;
		}

		if ((UDATA)-1 == bytecodeOffset) {
			frame->bytecodeIndex = 0;
		} else {
			frame->bytecodeIndex = (I_32)bytecodeOffset;
		}

		if ((UDATA)-1 == lineNumber) {
			frame->lineNumber = 0;
		} else {
			frame->lineNumber = (I_32)lineNumber;
		}

		cp->_currentFrameCount++;

skipFrame:
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
		_globalStringTable[1] = &_unknownClassStringUTF8Entry;
		_globalStringTable[2] = &_nativeMethodStringUTF8Entry;
		_globalStringTable[3] = &_nativeMethodSignatureStringUTF8Entry;
		_globalStringTable[_stringUTF8Count] = &_defaultPackageEntry;

		hashTableForEachDo(_stringUTF8Table, &mergeStringUTF8EntriesToGlobalTable, this);
		hashTableForEachDo(_packageTable, &mergePackageEntriesToGlobalTable, this);

		if (_debug) {
			printMergedStringTables();
		}
done:
		return;
	}

	/**
	* Helper to get the value of a system property.
	*
	* @param vm[in] the J9JavaVM
	* @param propName[in] the system property name
	*/
	static const char *getSystemProp(J9JavaVM *vm, const char *propName)
	{
		J9VMSystemProperty *jvmInfoProperty = NULL;
		const char *value = "";
		UDATA getPropertyResult = vm->internalVMFunctions->getSystemProperty(vm, propName, &jvmInfoProperty);
		if (J9SYSPROP_ERROR_NOT_FOUND != getPropertyResult) {
			value = jvmInfoProperty->value;
		}
		return value;
	}

	void addUnknownThreadEntry() {
		ThreadEntry unknownThreadEntry = {0};
		unknownThreadEntry.vmThread = NULL;
		unknownThreadEntry.index = 0;
		unknownThreadEntry.osTID = 0;
		unknownThreadEntry.javaTID = 0;
		unknownThreadEntry.javaThreadName = (J9UTF8 *)&unknownThread;
		unknownThreadEntry.osThreadName = (J9UTF8 *)&unknownThread;
		unknownThreadEntry.threadGroupIndex = 0;

		ThreadEntry *entry = (ThreadEntry *)hashTableAdd(_threadTable, &unknownThreadEntry);
		_firstThreadEntry = entry;
		_previousThreadEntry = entry;
	}

protected:

public:

	void addExecutionSampleEntry(J9JFRExecutionSample *executionSampleData);

	void addThreadStartEntry(J9JFRThreadStart *threadStartData);

	void addThreadEndEntry(J9JFREvent *threadEndData);

	void addThreadSleepEntry(J9JFRThreadSlept *threadSleepData);

	void addMonitorWaitEntry(J9JFRMonitorWaited* threadWaitData);

	void addMonitorEnterEntry(J9JFRMonitorEntered *monitorEnterData);

	void addThreadParkEntry(J9JFRThreadParked* threadParkData);

	void addCPULoadEntry(J9JFRCPULoad *cpuLoadData);

	void addThreadCPULoadEntry(J9JFRThreadCPULoad *threadCPULoadData);

	void addClassLoadingStatisticsEntry(J9JFRClassLoadingStatistics *classLoadingStatisticsData);

	void addThreadContextSwitchRateEntry(J9JFRThreadContextSwitchRate *threadContextSwitchRateData);

	void addThreadStatisticsEntry(J9JFRThreadStatistics *threadStatisticsData);

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

	J9Pool *getMonitorWaitTable()
	{
		return _monitorWaitTable;
	}

	J9Pool *getMonitorEnterTable()
	{
		return _monitorEnterTable;
	}

	J9Pool *getThreadParkTable()
	{
		return _threadParkTable;
	}

	J9Pool *getCPULoadTable()
	{
		return _cpuLoadTable;
	}

	J9Pool *getThreadCPULoadTable()
	{
		return _threadCPULoadTable;
	}

	J9Pool *getClassLoadingStatisticsTable()
	{
		return _classLoadingStatisticsTable;
	}

	J9Pool *getThreadContextSwitchRateTable()
	{
		return _threadContextSwitchRateTable;
	}

	J9Pool *getThreadStatisticsTable()
	{
		return _threadStatisticsTable;
	}

	J9Pool *getSystemProcessTable()
	{
		return _systemProcessTable;
	}

	J9Pool *getNativeLibraryTable()
	{
		return _nativeLibrariesTable;
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

	UDATA getMonitorWaitCount()
	{
		return _monitorWaitCount;
	}

	UDATA getMonitorEnterCount()
	{
		return _monitorEnterCount;
	}

	UDATA getThreadParkCount()
	{
		return _threadParkCount;
	}

	UDATA getCPULoadCount()
	{
		return _cpuLoadCount;
	}

	UDATA getThreadCPULoadCount()
	{
		return _threadCPULoadCount;
	}

	UDATA getClassLoadingStatisticsCount()
	{
		return _classLoadingStatisticsCount;
	}

	UDATA getThreadContextSwitchRateCount()
	{
		return _threadContextSwitchRateCount;
	}

	UDATA getThreadStatisticsCount()
	{
		return _threadStatisticsCount;
	}

	UDATA getSystemProcessCount()
	{
		return _systemProcessCount;
	}

	UDATA getSystemProcessStringSizeTotal()
	{
		return _systemProcessStringSizeTotal;
	}

	UDATA getNativeLibraryCount()
	{
		return _nativeLibrariesCount;
	}

	UDATA getNativeLibraryPathSizeTotal()
	{
		return _nativeLibraryPathSizeTotal;
	}

	ClassloaderEntry *getClassloaderEntry()
	{
		return _firstClassloaderEntry;
	}

	U_32 getClassloaderCount()
	{
		return _classLoaderCount;
	}

	ClassEntry *getClassEntry()
	{
		return _firstClassEntry;
	}

	U_32 getClassCount()
	{
		return _classCount;
	}

	ModuleEntry *getModuleEntry()
	{
		return _firstModuleEntry;
	}

	U_32 getModuleCount()
	{
		return _moduleCount;
	}

	MethodEntry *getMethodEntry()
	{
		return _firstMethodEntry;
	}

	U_32 getMethodCount()
	{
		return _methodCount;
	}

	void *getSymbolTableEntry(UDATA index)
	{
		return _globalStringTable[index];
	}

	U_32 getSymbolTableCount()
	{
		return _stringUTF8Count + _packageCount;
	}

	U_32 getStringUTF8Count()
	{
		return _stringUTF8Count;
	}

	PackageEntry *getPackageEntry()
	{
		return _firstPackageEntry;
	}

	U_32 getPackageCount()
	{
		return _packageCount;
	}

	UDATA getRequiredBufferSize()
	{
		return _requiredBufferSize;
	}

	U_32 getThreadGroupCount()
	{
		return _threadGroupCount;
	}

	ThreadGroupEntry *getThreadGroupEntry()
	{
		return _firstThreadGroupEntry;
	}

	U_32 getThreadCount()
	{
		return _threadCount;
	}

	ThreadEntry *getThreadEntry()
	{
		return _firstThreadEntry;
	}

	U_32 getStackTraceCount()
	{
		return _stackTraceCount;
	}

	StackTraceEntry *getStackTraceEntry()
	{
		return _firstStackTraceEntry;
	}

	U_32 getStackFrameCount()
	{
		return _stackFrameCount;
	}

	/**
	* Helper to get JFR constantEvents field.
	*
	* @param vm[in] the J9JavaVM
	*/
	static JFRConstantEvents *getJFRConstantEvents(J9JavaVM *vm)
	{
		return (JFRConstantEvents *)vm->jfrState.constantEvents;
	}


	void printTables();

	BuildResult getBuildResult() { return _buildResult; };

	void loadEvents(bool dumpCalled)
	{
		J9JFRBufferWalkState walkstate = {0};
		J9Pool *shallowEntries = NULL;
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
				addThreadSleepEntry((J9JFRThreadSlept*) event);
				break;
			case J9JFR_EVENT_TYPE_OBJECT_WAIT:
				addMonitorWaitEntry((J9JFRMonitorWaited*) event);
				break;
			case J9JFR_EVENT_TYPE_MONITOR_ENTER:
				addMonitorEnterEntry((J9JFRMonitorEntered *) event);
				break;
			case J9JFR_EVENT_TYPE_THREAD_PARK:
				addThreadParkEntry((J9JFRThreadParked*) event);
				break;
			case J9JFR_EVENT_TYPE_CPU_LOAD:
				addCPULoadEntry((J9JFRCPULoad *)event);
				break;
			case J9JFR_EVENT_TYPE_THREAD_CPU_LOAD:
				addThreadCPULoadEntry((J9JFRThreadCPULoad *)event);
				break;
			case J9JFR_EVENT_TYPE_CLASS_LOADING_STATISTICS:
				addClassLoadingStatisticsEntry((J9JFRClassLoadingStatistics *)event);
				break;
			case J9JFR_EVENT_TYPE_THREAD_CONTEXT_SWITCH_RATE:
				addThreadContextSwitchRateEntry((J9JFRThreadContextSwitchRate *)event);
				break;
			case J9JFR_EVENT_TYPE_THREAD_STATISTICS:
				addThreadStatisticsEntry((J9JFRThreadStatistics *)event);
				break;
			default:
				Assert_VM_unreachable();
				break;
			}
			event = jfrBufferNextDo(&walkstate);
		}

		if (isResultNotOKay()) {
			goto done;
		}

		if (dumpCalled) {
			loadSystemProcesses(_currentThread);
			loadNativeLibraries(_currentThread);
		}

		shallowEntries = pool_new(sizeof(ClassEntry**), 0, sizeof(U_64), 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(privatePortLibrary));
		if (NULL == shallowEntries) {
			_buildResult = OutOfMemory;
			goto done;
		}

		hashTableForEachDo(_classTable, findShallowEntries, shallowEntries);
		pool_do(shallowEntries, fixupShallowEntries, this);

		pool_kill(shallowEntries);

		mergeStringTables();

done:
		return;
	}

	U_32 consumeStackTrace(J9VMThread *walkThread, UDATA *walkStateCache, UDATA numberOfFrames) {
		U_32 index = U_32_MAX;
		UDATA expandedStackTraceCount = 0;

		if (0 == numberOfFrames) {
			index = 0;
			goto done;
		}

		expandedStackTraceCount = iterateStackTraceImpl(_currentThread, (j9object_t*)walkStateCache, NULL, NULL, FALSE, FALSE, numberOfFrames, FALSE);

		_currentStackFrameBuffer = (StackFrame*) j9mem_allocate_memory(sizeof(StackFrame) * expandedStackTraceCount, J9MEM_CATEGORY_CLASSES);
		_currentFrameCount = 0;
		if (NULL == _currentStackFrameBuffer) {
			_buildResult = OutOfMemory;
			goto done;
		}

		iterateStackTraceImpl(_currentThread, (j9object_t*)walkStateCache, &stackTraceCallback, this, FALSE, FALSE, numberOfFrames, FALSE);

		index = addStackTraceEntry(walkThread, j9time_nano_time(), _currentFrameCount);
		_stackFrameCount += (U_32)expandedStackTraceCount;
		_currentStackFrameBuffer = NULL;

done:
		return index;
	}

	/**
	 * Initialize constantEvents.
	 *
	 * @param vm[in] the J9JavaVM
	 */
	static void initializeJFRConstantEvents(J9JavaVM *vm, J9VMThread *currentThread, BuildResult *result)
	{
		initializeJVMInformationEvent(vm, currentThread, result);
		initializeCPUInformationEvent(vm, currentThread, result);
		initializeVirtualizationInformation(vm);
		initializeOSInformation(vm, result);
		initializeGCHeapConfigurationEvent(vm, result);
		initializeYoungGenerationConfigurationEvent(vm);
	}

	/**
	 * Free constantEvents.
	 *
	 * @param vm[in] the J9JavaVM
	 */
	static void freeJFRConstantEvents(J9JavaVM *vm)
	{
		PORT_ACCESS_FROM_JAVAVM(vm);

		freeJVMInformationEvent(vm);
		freeCPUInformationEvent(vm);
		freeOSInformation(vm);

		j9mem_free_memory(vm->jfrState.constantEvents);
	}

	/**
	 * Initialize JVMInformationEntry.
	 *
	 * @param vm[in] the J9JavaVM
	 */
	static void initializeJVMInformationEvent(J9JavaVM *vm, J9VMThread *currentThread, BuildResult *result)
	{
		PORT_ACCESS_FROM_JAVAVM(vm);
		/* Initialize JVM Information */
		JVMInformationEntry *jvmInformation = &(getJFRConstantEvents(vm)->JVMInfoEntry);

		/* Set JVM name */
		jvmInformation->jvmName = getSystemProp(vm, "java.vm.name");

		/* Set JVM version */
		jvmInformation->jvmVersion = getSystemProp(vm, "java.vm.info");

		/* Set Java arguments */
		jvmInformation->javaArguments = getSystemProp(vm, "sun.java.command");

		/* Set JVM arguments by concatenating actualVMArgs */
		JavaVMInitArgs *vmArgs = vm->vmArgsArray->actualVMArgs;
		UDATA vmArgsLen = vmArgs->nOptions;
		for (IDATA i = 0; i < vmArgs->nOptions; i++) {
			vmArgsLen += strlen(vmArgs->options[i].optionString);
		}

		jvmInformation->jvmArguments = (char *)j9mem_allocate_memory(sizeof(char) * vmArgsLen, OMRMEM_CATEGORY_VM);
		char *cursor = jvmInformation->jvmArguments;

		if (NULL != cursor) {
			for (IDATA i = 0; i < vmArgs->nOptions; i++) {
				UDATA len = strlen(vmArgs->options[i].optionString);
				memcpy(cursor, vmArgs->options[i].optionString, len);
				cursor += len;

				if (i == vmArgs->nOptions - 1) {
					*cursor = '\0';
				} else {
					*cursor = ' ';
				}
				cursor += 1;
			}
		} else {
			*result = OutOfMemory;
		}
		/* Ignoring jvmFlags for now */
		jvmInformation->jvmFlags = NULL;
		jvmInformation->jvmStartTime = vm->j9ras->startTimeMillis;
		jvmInformation->pid = vm->j9ras->pid;
	}

	/**
	 * Free JVMInfoEntry.
	 *
	 * @param vm[in] the J9JavaVM
	 */
	static void freeJVMInformationEvent(J9JavaVM *vm)
	{
		JFRConstantEvents *jfrConstantEvents = getJFRConstantEvents(vm);
		if (NULL != jfrConstantEvents) {
			PORT_ACCESS_FROM_JAVAVM(vm);
			j9mem_free_memory(jfrConstantEvents->JVMInfoEntry.jvmArguments);
			jfrConstantEvents->JVMInfoEntry.jvmArguments = NULL;
		}
	}

	/**
	 * Initialize CPUInformationEntry.
	 *
	 * @param vm[in] the J9JavaVM
	 */
	static void initializeCPUInformationEvent(J9JavaVM *vm, J9VMThread *currentThread, BuildResult *result)
	{
		PORT_ACCESS_FROM_JAVAVM(vm);
		OMRPORT_ACCESS_FROM_J9PORT(privatePortLibrary);

		CPUInformationEntry *cpuInformation = &(getJFRConstantEvents(vm)->CPUInfoEntry);

		/* Set CPU type */
		cpuInformation->cpu = j9sysinfo_get_CPU_architecture();

		/* Set CPU description */
		OMRProcessorDesc desc = {};
		omrsysinfo_get_processor_description(&desc);
		char buffer[512];
		omrsysinfo_get_processor_feature_string(&desc, buffer, sizeof(buffer));
		UDATA len = strlen(buffer) + 1;
		cpuInformation->description = (char *)j9mem_allocate_memory(sizeof(char) * len, OMRMEM_CATEGORY_VM);
		if (NULL != cpuInformation->description) {
			memcpy(cpuInformation->description, buffer, len);
		} else {
			*result = OutOfMemory;
		}

		cpuInformation->cores = (U_32)j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_PHYSICAL);
		/* Setting number of sockets to number of cores for now as there's no easy way to get this info.
		 * TODO: fix this when we can query number of sockets from OMR
		 */
		cpuInformation->sockets = cpuInformation->cores;
		cpuInformation->hwThreads = (U_32)j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_TARGET);
	}

	/**
	 * Free CPUInfoEntry.
	 *
	 * @param vm[in] the J9JavaVM
	 */
	static void freeCPUInformationEvent(J9JavaVM *vm)
	{
		JFRConstantEvents *jfrConstantEvents = getJFRConstantEvents(vm);
		if (NULL != jfrConstantEvents) {
			PORT_ACCESS_FROM_JAVAVM(vm);
			j9mem_free_memory(jfrConstantEvents->CPUInfoEntry.description);
			jfrConstantEvents->CPUInfoEntry.description = NULL;
		}
	}

	/**
	 * Initialize VirtualizationInformationEntry.
	 *
	 * @param vm[in] the J9JavaVM
	 */
	static void initializeVirtualizationInformation(J9JavaVM *vm)
	{
		PORT_ACCESS_FROM_JAVAVM(vm);

		VirtualizationInformationEntry *virtualizationInfo = &(getJFRConstantEvents(vm)->VirtualizationInfoEntry);

		intptr_t rc = j9hypervisor_hypervisor_present();
		J9HypervisorVendorDetails vendorDetails = {0};

		switch(rc) {
		case J9HYPERVISOR_NOT_PRESENT:
			virtualizationInfo->name = "No virtualization detected";
			break;
		case J9PORT_ERROR_HYPERVISOR_UNSUPPORTED:
			virtualizationInfo->name = "Virtualization detection not supported";
			break;
		case J9HYPERVISOR_PRESENT:
			j9hypervisor_get_hypervisor_info(&vendorDetails);
			virtualizationInfo->name = vendorDetails.hypervisorName;
			break;
		default:
			virtualizationInfo->name = "Error getting virtualization information";
			break;
		};
	}

	/**
	 * Initialize OSInformationEntry.
	 *
	 * @param vm[in] the J9JavaVM
	 */
	static void initializeOSInformation(J9JavaVM *vm, BuildResult *result)
	{
		PORT_ACCESS_FROM_JAVAVM(vm);

		/* Build OS Version string from os.name, os.version and os.arch properties */
		const char *osName = getSystemProp(vm, "os.name");
		const char *osVersion = j9sysinfo_get_OS_version();
		const char *osArch = getSystemProp(vm, "os.arch");

		UDATA len = 3 + strlen(osName) + strlen(osVersion) + strlen(osArch);

		getJFRConstantEvents(vm)->OSInfoEntry.osVersion = (char *)j9mem_allocate_memory(sizeof(char) * len, OMRMEM_CATEGORY_VM);
		char *buffer = getJFRConstantEvents(vm)->OSInfoEntry.osVersion;
		if (NULL == buffer) {
			*result = OutOfMemory;
			return;
		}

		memcpy(buffer, osName, strlen(osName));
		buffer += strlen(osName);
		*buffer = ' ';
		buffer++;

		memcpy(buffer, osVersion, strlen(osVersion));
		buffer += strlen(osVersion);
		*buffer = ' ';
		buffer++;

		memcpy(buffer, osArch, strlen(osArch));
		buffer += strlen(osArch);
		*buffer = '\0';
	}

	/**
	 * Free OSInformationEntry.
	 *
	 * @param vm[in] the J9JavaVM
	 */
	static void freeOSInformation(J9JavaVM *vm)
	{
		JFRConstantEvents *jfrConstantEvents = getJFRConstantEvents(vm);
		if (NULL != jfrConstantEvents) {
			PORT_ACCESS_FROM_JAVAVM(vm);
			j9mem_free_memory(jfrConstantEvents->OSInfoEntry.osVersion);
			jfrConstantEvents->OSInfoEntry.osVersion = NULL;
		}
	}

	/**
	 * Initialize GCHeapConfigurationEntry
	 *
	 * @param vm[in] the J9JavaVM
	 */
	static void initializeGCHeapConfigurationEvent(J9JavaVM *vm, BuildResult *result)
	{
		J9MemoryManagerFunctions *mmFuncs = vm->memoryManagerFunctions;
		GCHeapConfigurationEntry *gcConfiguration = &(getJFRConstantEvents(vm)->GCHeapConfigEntry);

		gcConfiguration->minSize = mmFuncs->j9gc_get_initial_heap_size(vm);
		gcConfiguration->maxSize = mmFuncs->j9gc_get_maximum_heap_size(vm);
		gcConfiguration->initialSize = gcConfiguration->minSize;
		uintptr_t value;
		gcConfiguration->usesCompressedOops = mmFuncs->j9gc_modron_getConfigurationValueForKey(vm, j9gc_modron_configuration_compressObjectReferences, &value) ? value : 0;
		gcConfiguration->compressedOopsMode = ZeroBased;
		gcConfiguration->objectAlignment = vm->objectAlignmentInBytes;
		gcConfiguration->heapAddressBits = J9JAVAVM_REFERENCE_SIZE(vm) * 8;
	}

	/**
	 * Initialize YoungGenerationConfigurationEntry
	 *
	 * @param vm[in] the J9JavaVM
	 */
	static void initializeYoungGenerationConfigurationEvent(J9JavaVM *vm)
	{
		J9MemoryManagerFunctions *mmFuncs = vm->memoryManagerFunctions;
		YoungGenerationConfigurationEntry *youngGenConfiguration = &(getJFRConstantEvents(vm)->YoungGenConfigEntry);

		youngGenConfiguration->minSize = mmFuncs->j9gc_get_minimum_young_generation_size(vm);
		youngGenConfiguration->maxSize = mmFuncs->j9gc_get_maximum_young_generation_size(vm);
		if (0 != mmFuncs->j9gc_get_maximum_young_generation_size(vm)) {
			youngGenConfiguration->newRatio = mmFuncs->j9gc_get_maximum_heap_size(vm)/mmFuncs->j9gc_get_maximum_young_generation_size(vm) - 1;
		} else {
			youngGenConfiguration->newRatio = 0;
		}
	}



	static uintptr_t recordSystemProcessEvent(uintptr_t pid, const char *commandLine, void *userData)
	{
		VM_JFRConstantPoolTypes *constantPoolTypes = reinterpret_cast<VM_JFRConstantPoolTypes *>(userData);
		PORT_ACCESS_FROM_JAVAVM(constantPoolTypes->_vm);
		J9Pool *systemProcessTable = constantPoolTypes->getSystemProcessTable();
		UDATA cmdLength = strlen(commandLine);
		char *commandLineCopy = reinterpret_cast<char *>(j9mem_allocate_memory(cmdLength + 1, OMRMEM_CATEGORY_VM));
		if (NULL == commandLineCopy) {
			constantPoolTypes->_buildResult = OutOfMemory;
			return ~(uintptr_t)0;
		}
		SystemProcessEntry *entry = reinterpret_cast<SystemProcessEntry *>(pool_newElement(systemProcessTable));
		if (NULL == entry) {
			j9mem_free_memory(commandLineCopy);
			constantPoolTypes->_buildResult = OutOfMemory;
			return ~(uintptr_t)0;
		}
		memcpy(commandLineCopy, commandLine, cmdLength + 1);
		entry->ticks = j9time_nano_time();
		entry->pid = pid;
		entry->commandLine = commandLineCopy;
		constantPoolTypes->_systemProcessStringSizeTotal += cmdLength;
		constantPoolTypes->_systemProcessCount += 1;
		return 0;
	}

	void loadSystemProcesses(J9VMThread *currentThread)
	{
		OMRPORT_ACCESS_FROM_J9VMTHREAD(currentThread);
		omrsysinfo_get_processes(recordSystemProcessEvent, this);
	}

	static uintptr_t processNativeLibrariesCallback(const char *libraryName, void *lowAddress, void *highAddress, void *userData)
	{
		VM_JFRConstantPoolTypes *constantPoolTypes = reinterpret_cast<VM_JFRConstantPoolTypes *>(userData);
		J9Pool *nativeLibrariesTable = constantPoolTypes->_nativeLibrariesTable;
		NativeLibraryEntry *firstNativeLibraryEntry = constantPoolTypes->_firstNativeLibraryEntry;
		NativeLibraryEntry *previousNativeLibraryEntry = constantPoolTypes->_previousNativeLibraryEntry;
		NativeLibraryEntry *entry = firstNativeLibraryEntry;
		PORT_ACCESS_FROM_JAVAVM(constantPoolTypes->_vm);
		for (; NULL != entry; entry = entry->next) {
			if (0 == strcmp(entry->name, libraryName)) {
				if (entry->addressLow > (UDATA)lowAddress) {
					entry->addressLow = (UDATA)lowAddress;
				}
				if (entry->addressHigh < (UDATA)highAddress) {
					entry->addressHigh = (UDATA)highAddress;
				}
				return 0;
			}
		}
		size_t libraryNameLength = strlen(libraryName);
		char *libraryNameCopy = (char *)j9mem_allocate_memory(libraryNameLength + 1, OMRMEM_CATEGORY_VM);
		if (NULL == libraryNameCopy) {
			/* Allocation for library name failed. */
			constantPoolTypes->_buildResult = OutOfMemory;
			return 1;
		}
		NativeLibraryEntry *newEntry = reinterpret_cast<NativeLibraryEntry *>(pool_newElement(nativeLibrariesTable));
		if (NULL == newEntry) {
			/* Allocation failed. */
			j9mem_free_memory(libraryNameCopy);
			constantPoolTypes->_buildResult = OutOfMemory;
			return 1;
		}
		memcpy(libraryNameCopy, libraryName, libraryNameLength + 1);
		newEntry->ticks = j9time_nano_time();
		newEntry->name = libraryNameCopy;
		newEntry->addressLow = (UDATA)lowAddress;
		newEntry->addressHigh = (UDATA)highAddress;
		newEntry->next = NULL;
		constantPoolTypes->_nativeLibrariesCount += 1;
		constantPoolTypes->_nativeLibraryPathSizeTotal += libraryNameLength;
		if (NULL != previousNativeLibraryEntry) {
			previousNativeLibraryEntry->next = newEntry;
		} else {
			constantPoolTypes->_firstNativeLibraryEntry = newEntry;
		}
		constantPoolTypes->_previousNativeLibraryEntry = newEntry;
		return 0;
	}

	void loadNativeLibraries(J9VMThread *currentThread)
	{
		OMRPORT_ACCESS_FROM_J9VMTHREAD(currentThread);
		omrsl_get_libraries(processNativeLibrariesCallback, this);
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
		, _monitorWaitTable(NULL)
		, _monitorWaitCount(0)
		, _monitorEnterTable(NULL)
		, _monitorEnterCount(0)
		, _threadParkTable(NULL)
		, _threadParkCount(0)
		, _cpuLoadTable(NULL)
		, _cpuLoadCount(0)
		, _threadCPULoadTable(NULL)
		, _threadCPULoadCount(0)
		, _classLoadingStatisticsTable(NULL)
		, _classLoadingStatisticsCount(0)
		, _threadContextSwitchRateTable(NULL)
		, _threadContextSwitchRateCount(0)
		, _threadStatisticsTable(NULL)
		, _threadStatisticsCount(0)
		, _systemProcessTable(NULL)
		, _systemProcessCount(0)
		, _systemProcessStringSizeTotal(0)
		, _nativeLibrariesTable(NULL)
		, _nativeLibrariesCount(0)
		, _nativeLibraryPathSizeTotal(0)
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
		, _firstNativeLibraryEntry(NULL)
		, _previousNativeLibraryEntry(NULL)
		, _requiredBufferSize(0)
	{
		_classTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(ClassEntry), sizeof(ClassEntry *), 0, J9MEM_CATEGORY_CLASSES, jfrClassHashFn, jfrClassHashEqualFn, NULL, _vm);
		if (NULL == _classTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_packageTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(PackageEntry), sizeof(PackageEntry *), 0, J9MEM_CATEGORY_CLASSES, jfrPackageHashFn, jfrPackageHashEqualFn, NULL, _vm);
		if (NULL == _packageTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_classLoaderTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(ClassloaderEntry), sizeof(J9ClassLoader*), 0, J9MEM_CATEGORY_CLASSES, classloaderNameHashFn, classloaderNameHashEqualFn, NULL, _vm);
		if (NULL == _classLoaderTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_methodTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(MethodEntry), sizeof(J9ROMMethod*), 0, J9MEM_CATEGORY_CLASSES, methodNameHashFn, methodNameHashEqualFn, NULL, _vm);
		if (NULL == _methodTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_stringUTF8Table = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(StringUTF8Entry), sizeof(StringUTF8Entry*), 0, J9MEM_CATEGORY_CLASSES, jfrStringUTF8HashFn, jfrStringUTF8HashEqualFn, NULL, _vm);
		if (NULL == _stringUTF8Table) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_moduleTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(ModuleEntry), sizeof(ModuleEntry*), 0, J9MEM_CATEGORY_CLASSES, jfrModuleHashFn, jfrModuleHashEqualFn, NULL, _vm);
		if (NULL == _moduleTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_threadTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(ThreadEntry), sizeof(U_64), 0, J9MEM_CATEGORY_CLASSES, threadHashFn, threadHashEqualFn, NULL, _currentThread);
		if (NULL == _threadTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_stackTraceTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(StackTraceEntry), sizeof(U_64), 0, J9MEM_CATEGORY_CLASSES, stackTraceHashFn, stackTraceHashEqualFn, NULL, _vm);
		if (NULL == _stackTraceTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_threadGroupTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(ThreadGroupEntry), sizeof(U_64), 0, J9MEM_CATEGORY_CLASSES, threadGroupHashFn, threadGroupHashEqualFn, NULL, _vm);
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

		_monitorWaitTable = pool_new(sizeof(MonitorWaitEntry), 0, sizeof(U_64), 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(privatePortLibrary));
		if (NULL == _monitorWaitTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_monitorEnterTable = pool_new(sizeof(MonitorEnterEntry), 0, sizeof(U_64), 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(privatePortLibrary));
		if (NULL == _monitorEnterTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_threadParkTable = pool_new(sizeof(ThreadParkEntry), 0, sizeof(U_64), 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(privatePortLibrary));
		if (NULL == _threadParkTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_cpuLoadTable = pool_new(sizeof(CPULoadEntry), 0, sizeof(U_64), 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(privatePortLibrary));
		if (NULL == _cpuLoadTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_threadCPULoadTable = pool_new(sizeof(ThreadCPULoadEntry), 0, sizeof(U_64), 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(privatePortLibrary));
		if (NULL == _threadCPULoadTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_classLoadingStatisticsTable = pool_new(sizeof(ClassLoadingStatisticsEntry), 0, sizeof(U_64), 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(privatePortLibrary));
		if (NULL == _classLoadingStatisticsTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_threadContextSwitchRateTable = pool_new(sizeof(ThreadContextSwitchRateEntry), 0, sizeof(U_64), 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(privatePortLibrary));
		if (NULL == _threadContextSwitchRateTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_threadStatisticsTable = pool_new(sizeof(ThreadStatisticsEntry), 0, sizeof(U_64), 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(privatePortLibrary));
		if (NULL == _threadStatisticsTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_systemProcessTable = pool_new(sizeof(SystemProcessEntry), 0, sizeof(U_64), 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(privatePortLibrary));
		if (NULL == _systemProcessTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		_nativeLibrariesTable = pool_new(sizeof(NativeLibraryEntry), 0, sizeof(U_64), 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(privatePortLibrary));
		if (NULL == _nativeLibrariesTable) {
			_buildResult = OutOfMemory;
			goto done;
		}

		/* Add reserved index for default entries. For strings zero is the empty or NUll string.
		 * For package zero is the deafult package, for Module zero is the unnamed module. ThreadGroup
		 * zero is NULL threadGroup.
		 */
		_stringUTF8Count += 1;
		memset(&_defaultStringUTF8Entry, 0, sizeof(_defaultStringUTF8Entry));
		_defaultStringUTF8Entry.string = (J9UTF8*)&nullString;

		_stringUTF8Count += 1;
		memset(&_unknownClassStringUTF8Entry , 0, sizeof(_unknownClassStringUTF8Entry));
		_unknownClassStringUTF8Entry.string = (J9UTF8*)&unknownClass;

		_stringUTF8Count += 1;
		memset(&_nativeMethodStringUTF8Entry, 0, sizeof(_nativeMethodStringUTF8Entry));
		_nativeMethodStringUTF8Entry.string = (J9UTF8*)&nativeMethod;

		_stringUTF8Count += 1;
		memset(&_nativeMethodSignatureStringUTF8Entry, 0, sizeof(_nativeMethodSignatureStringUTF8Entry));
		_nativeMethodSignatureStringUTF8Entry.string = (J9UTF8*)&nativeMethodSignature;

		_moduleCount += 1;
		memset(&_defaultModuleEntry, 0, sizeof(_defaultModuleEntry));
		_firstModuleEntry = &_defaultModuleEntry;
		_previousModuleEntry = _firstModuleEntry;

		_packageCount += 1;
		memset(&_defaultPackageEntry, 0, sizeof(_defaultPackageEntry));
		_defaultPackageEntry.exported = TRUE;
		_defaultPackageEntry.packageName = J9UTF8_DATA((J9UTF8*) &defaultPackage);
		_defaultPackageEntry.packageNameLength = J9UTF8_LENGTH((J9UTF8*) &defaultPackage);
		_firstPackageEntry = &_defaultPackageEntry;
		_previousPackageEntry = _firstPackageEntry;

		_threadGroupCount += 1;
		memset(&_defaultThreadGroup, 0, sizeof(_defaultThreadGroup));
		_firstThreadGroupEntry = &_defaultThreadGroup;
		_previousThreadGroupEntry = _firstThreadGroupEntry;

		_classCount += 1;
		memset(&_defaultClassEntry, 0, sizeof(_defaultClassEntry));
		_defaultClassEntry.nameStringUTF8Index = (U_32)UnknownClass;
		_firstClassEntry = &_defaultClassEntry;
		_previousClassEntry = _firstClassEntry;

		_methodCount += 1;
		memset(&_defaultMethodEntry, 0, sizeof(_defaultMethodEntry));
		_defaultMethodEntry.nameStringUTF8Index = (U_32)NativeMethod;
		_defaultMethodEntry.descriptorStringUTF8Index = (U_32)NativeMethodSignature;
		/* default class */
		_defaultMethodEntry.classIndex = 0;
		_firstMethodEntry = &_defaultMethodEntry;
		_previousMethodEntry = _firstMethodEntry;

		_stackTraceCount += 1;
		memset(&_defaultStackTraceEntry, 0, sizeof(_defaultStackTraceEntry));
		_firstStackTraceEntry = &_defaultStackTraceEntry;
		_previousStackTraceEntry = _firstStackTraceEntry;

		/* Leave index 0 as a NULL entry for unknown notifier thread. */
		_threadCount += 1;
		addUnknownThreadEntry();

done:
		return;
	}

	~VM_JFRConstantPoolTypes()
	{
		hashTableForEachDo(_stringUTF8Table, &freeUTF8Strings, _currentThread);
		hashTableForEachDo(_stackTraceTable, &freeStackStraceEntries, _currentThread);
		hashTableForEachDo(_threadTable, &freeThreadNameEntries, _currentThread);
		hashTableForEachDo(_threadGroupTable, &freeThreadGroupNameEntries, _currentThread);
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
		pool_kill(_monitorWaitTable);
		pool_kill(_monitorEnterTable);
		pool_kill(_threadParkTable);
		pool_kill(_cpuLoadTable);
		pool_kill(_threadCPULoadTable);
		pool_kill(_classLoadingStatisticsTable);
		pool_kill(_threadContextSwitchRateTable);
		pool_kill(_threadStatisticsTable);
		pool_kill(_systemProcessTable);
		pool_kill(_nativeLibrariesTable);
		j9mem_free_memory(_globalStringTable);
	}

};
#endif /* defined(J9VM_OPT_JFR) */
#endif /* !defined(JFRCONSTANTPOOLTYPES_HPP_) */
