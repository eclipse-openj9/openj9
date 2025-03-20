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
#include "JFRUtils.hpp"
#include "vm_internal.h"

#if defined(J9VM_OPT_JFR)

#include "JFRChunkWriter.hpp"
#include "JFRConstantPoolTypes.hpp"

void
VM_JFRChunkWriter::writeJFRHeader()
{
	_bufferWriter->setCursor(_jfrHeaderCursor);

	/* Magic number "FLR\0" in ASCII */
	_bufferWriter->writeU32(0x464c5200); // 0

	/* Major and Minor version numbers */
	_bufferWriter->writeU16(2); // 4
	_bufferWriter->writeU16(1);

	/* chunk size */
	_bufferWriter->writeU64(_bufferWriter->getSize()); // 8

	/* checkpoint offset */
	_bufferWriter->writeU64(_bufferWriter->getFileOffsetFromStart(_checkPointEventOffset)); // 16

	/* metadata offset */
	_bufferWriter->writeU64(_bufferWriter->getFileOffsetFromStart(_metadataOffset)); // 24

	/* start time */
	_bufferWriter->writeU64(_vm->jfrState.chunkStartTime); // 32

	/* duration */
	_bufferWriter->writeU64(0); // 40

	/* start ticks */
	_bufferWriter->writeU64(_vm->jfrState.chunkStartTicks); // 48

	/* ticks per second - 1000_000_000 ticks per second means that we are reporting nanosecond timestamps */
	_bufferWriter->writeU64(1000000000); // 56

	/* file state or generation */
	_bufferWriter->writeU8(0); // 64 // TODO ???

	/* pad */
	_bufferWriter->writeU16(0); // 65

	U_8 flags = JFR_HEADER_SPECIALFLAGS_COMPRESSED_INTS;
	if (_finalWrite) {
		flags |= JFR_HEADER_SPECIALFLAGS_LAST_CHUNK;
	}
	_bufferWriter->writeU8(flags);
}

U_8 *
VM_JFRChunkWriter::writeJFRMetadata()
{
	/* reserve metadata size field */
	U_8 *dataStart = reserveEventSize();

	if (_debug) {
		j9tty_printf(
				PORTLIB,
				"Metadata frame start offset = 0x%llX\n",
				_bufferWriter->getFileOffsetFromStart(dataStart));
	}

	/* write metadata header */
	_bufferWriter->writeU8(EventMetadata);

	/* start time */
	_bufferWriter->writeLEB128(VM_JFRUtils::getCurrentTimeNanos(privatePortLibrary, _buildResult));

	/* duration */
	_bufferWriter->writeLEB128((U_64)0);

	/* metadata ID */
	_bufferWriter->writeLEB128((U_64)METADATA_ID);


	U_8 *blobStart = _bufferWriter->getCursor();
	if (_debug) {
		j9tty_printf(
				PORTLIB,
				"Metadata blob start offset = 0x%llX, size = %zu\n",
				_bufferWriter->getFileOffsetFromStart(_bufferWriter->getCursor()),
				_vm->jfrState.metaDataBlobFileSize);
	}

	/* metadata blob file */
	_bufferWriter->writeData(_vm->jfrState.metaDataBlobFile, _vm->jfrState.metaDataBlobFileSize);

	if (_debug) {
		j9tty_printf(
				PORTLIB,
				"Metadata blob size from LEB128 = %llu\n",
				VM_BufferWriter::convertFromLEB128ToU64(blobStart));
	}

	/* add metadata size */
	writeEventSize(dataStart);

	if (_debug) {
		j9tty_printf(
				PORTLIB,
				"Metadata size = %zu, fromLEB128 = %llu\n",
				_bufferWriter->getCursor() - dataStart,
				VM_BufferWriter::convertFromLEB128ToU64(dataStart));
	}

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writeCheckpointEventHeader(CheckpointTypeMask typeMask, U_32 cpCount)
{
	/* write delta offset in previous checkpoint event */
	if (NULL != _previousCheckpointDelta) {
		_bufferWriter->writeLEB128PaddedU72(
				_previousCheckpointDelta,
				_bufferWriter->getFileOffset(_bufferWriter->getCursor(), _lastDataStart));
	}

	/* reserve size field */
	U_8 *dataStart = reserveEventSize();
	_lastDataStart = dataStart;

	if (_debug) {
		j9tty_printf(
				PORTLIB,
				"Checkpoint event frame start offset = 0x%llX\n",
				_bufferWriter->getFileOffsetFromStart(dataStart));
	}

	/* write checkpoint header */
	_bufferWriter->writeU8(EventCheckpoint);

	/* start time */
	_bufferWriter->writeLEB128(j9time_nano_time());

	/* duration */
	_bufferWriter->writeLEB128((U_64)0);

	/* reserve delta offset to next checkpoint event */
	_previousCheckpointDelta = _bufferWriter->getCursor();
	_bufferWriter->writeLEB128PaddedU72(0);

	if (_debug) {
		j9tty_printf(
				PORTLIB,
				"next pointer=0x%llX val=%llu\n",
				_bufferWriter->getFileOffsetFromStart(_previousCheckpointDelta),
				VM_BufferWriter::convertFromLEB128ToU64(_previousCheckpointDelta));
	}

	/* type mask */
	_bufferWriter->writeU8(typeMask);

	/* constant pool count */
	_bufferWriter->writeLEB128(cpCount);

	return dataStart;
}

void
VM_JFRChunkWriter::writeUTF8String(const J9UTF8 *string)
{
	if (NULL == string) {
		_bufferWriter->writeLEB128(NullString);
	} else {
		writeUTF8String(J9UTF8_DATA(string), J9UTF8_LENGTH(string));
	}
}

void
VM_JFRChunkWriter::writeUTF8String(const U_8 *data, UDATA len)
{
	_bufferWriter->writeLEB128(UTF8);
	_bufferWriter->writeLEB128(len);
	_bufferWriter->writeData(data, len);
}

void
VM_JFRChunkWriter::writeStringLiteral(const char *string)
{
	if (NULL == string) {
		_bufferWriter->writeLEB128(NullString);
	} else {
		const UDATA length = strlen(string);
		writeUTF8String((U_8 *)string, length);
	}
}

U_8 *
VM_JFRChunkWriter::writeThreadStateCheckpointEvent()
{
	U_8 *dataStart = writeCheckpointEventHeader(Generic, 1);

	/* class ID */
	_bufferWriter->writeLEB128(ThreadStateID);

	/* number of states */
	_bufferWriter->writeLEB128(THREADSTATE_COUNT);

	for (int i = 0; i < THREADSTATE_COUNT; i++) {
		/* constant index */
		_bufferWriter->writeLEB128(i);

		/* string encoding */
		_bufferWriter->writeLEB128(UTF8);

		/* string length */
		UDATA len = strlen(threadStateNames[i]);
		_bufferWriter->writeLEB128(len);

		/* write string */ // need to think about UTF8 enconding
		_bufferWriter->writeData((U_8 *)threadStateNames[i], len);
	}

	/* write size */
	writeEventSize(dataStart);

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writePackageCheckpointEvent()
{
	U_8 *dataStart = NULL;

	if (_constantPoolTypes.getPackageCount() > 0) {
		dataStart = writeCheckpointEventHeader(Generic, 1);

		/* class ID */
		_bufferWriter->writeLEB128(PackageID);

		/* number of states */
		_bufferWriter->writeLEB128(_constantPoolTypes.getPackageCount());

		PackageEntry *entry = _constantPoolTypes.getPackageEntry();
		while (NULL != entry) {
			/* write index */
			_bufferWriter->writeLEB128(entry->index);

			/* name index */
			_bufferWriter->writeLEB128(_constantPoolTypes.getStringUTF8Count() + entry->index);

			/* module index */
			_bufferWriter->writeLEB128(entry->moduleIndex);

			/* exported index */
			_bufferWriter->writeBoolean(entry->exported);

			entry = entry->next;
		}

		/* write size */
		writeEventSize(dataStart);
	}

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writeMethodCheckpointEvent()
{
	U_8 *dataStart = NULL;

	if (_constantPoolTypes.getMethodCount() > 0) {
		dataStart = writeCheckpointEventHeader(Generic, 1);

		/* class ID */
		_bufferWriter->writeLEB128(MethodID);

		/* number of states */
		_bufferWriter->writeLEB128(_constantPoolTypes.getMethodCount());

		MethodEntry *entry = _constantPoolTypes.getMethodEntry();
		while (NULL != entry) {
			/* write index */
			_bufferWriter->writeLEB128(entry->index);

			/* type index */
			_bufferWriter->writeLEB128(entry->classIndex);

			/* name index */
			_bufferWriter->writeLEB128(entry->nameStringUTF8Index);

			/* descriptor index */
			_bufferWriter->writeLEB128(entry->descriptorStringUTF8Index);

			/* modifiers */
			_bufferWriter->writeLEB128(entry->modifiers);

			/* hidden */
			_bufferWriter->writeBoolean(entry->hidden);

			entry = entry->next;
		}

		/* write size */
		writeEventSize(dataStart);
	}

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writeClassloaderCheckpointEvent()
{
	U_8 *dataStart = NULL;

	if (_constantPoolTypes.getClassloaderCount() > 0) {
		dataStart = writeCheckpointEventHeader(Generic, 1);

		/* class ID */
		_bufferWriter->writeLEB128(ClassLoaderID);

		/* number of states */
		_bufferWriter->writeLEB128(_constantPoolTypes.getClassloaderCount());

		ClassloaderEntry *entry = _constantPoolTypes.getClassloaderEntry();
		while (NULL != entry) {
			/* write index */
			_bufferWriter->writeLEB128(entry->index);

			/* class index */
			_bufferWriter->writeLEB128(entry->classIndex);

			/* class name index */
			_bufferWriter->writeLEB128(entry->nameIndex);

			entry = entry->next;
		}

		/* write size */
		writeEventSize(dataStart);
	}

	return dataStart;
}

U_8 *
VM_JFRChunkWriter:: writeClassCheckpointEvent()
{
	U_8 *dataStart = NULL;

	if (_constantPoolTypes.getClassCount() > 0) {
		dataStart = writeCheckpointEventHeader(Generic, 1);

		/* class ID */
		_bufferWriter->writeLEB128(ClassID);

		/* number of states */
		_bufferWriter->writeLEB128(_constantPoolTypes.getClassCount());

		ClassEntry *entry = _constantPoolTypes.getClassEntry();
		while (NULL != entry) {
			/* write index */
			_bufferWriter->writeLEB128(entry->index);

			/* classloader index */
			_bufferWriter->writeLEB128(entry->classLoaderIndex);

			/* class name index */
			_bufferWriter->writeLEB128(entry->nameStringUTF8Index);

			/* package index */
			_bufferWriter->writeLEB128(entry->packageIndex);

			/* class modifiers */
			_bufferWriter->writeLEB128(entry->modifiers);

			/* class hidden */
			_bufferWriter->writeBoolean(entry->hidden);

			entry = entry->next;
		}

		/* write size */
		writeEventSize(dataStart);
	}

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writeModuleCheckpointEvent()
{
	U_8 *dataStart = NULL;

	if (_constantPoolTypes.getModuleCount() > 0) {
		dataStart = writeCheckpointEventHeader(Generic, 1);

		/* class ID */
		_bufferWriter->writeLEB128(ModuleID);

		/* number of states */
		_bufferWriter->writeLEB128(_constantPoolTypes.getModuleCount());

		ModuleEntry *entry = _constantPoolTypes.getModuleEntry();
		while (NULL != entry) {
			/* write index */
			_bufferWriter->writeLEB128(entry->index);

			/* module name index */
			_bufferWriter->writeLEB128(entry->nameStringIndex);

			/* module version index */
			_bufferWriter->writeLEB128(entry->versionStringIndex);

			/* module location index */
			_bufferWriter->writeLEB128(entry->locationStringUTF8Index);

			/* module location index */
			_bufferWriter->writeLEB128(entry->classLoaderIndex);

			entry = entry->next;
		}

		/* write size */
		writeEventSize(dataStart);
	}

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writeThreadCheckpointEvent()
{
	U_8 *dataStart = NULL;

	if (_constantPoolTypes.getThreadCount() > 0) {
		dataStart = writeCheckpointEventHeader(Generic, 1);

		/* class ID */
		_bufferWriter->writeLEB128(ThreadID);

		/* number of states */
		_bufferWriter->writeLEB128(_constantPoolTypes.getThreadCount());

		ThreadEntry *entry = _constantPoolTypes.getThreadEntry();
		while (NULL != entry) {
			/* write index */
			_bufferWriter->writeLEB128(entry->index);

			/* write OS thread name */
			writeUTF8String(entry->osThreadName);

			/* write OS Thread ID */
			_bufferWriter->writeLEB128(entry->osTID);

			/* write Java thread name */
			writeUTF8String(entry->javaThreadName);

			/* write Java Thread ID */
			_bufferWriter->writeLEB128(entry->javaTID);

			/* write Thread group index */
			_bufferWriter->writeLEB128(entry->threadGroupIndex);

			entry = entry->next;
		}

		/* write size */
		writeEventSize(dataStart);
	}

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writeThreadGroupCheckpointEvent()
{
	U_8 *dataStart = NULL;

	if (_constantPoolTypes.getThreadGroupCount() > 0) {
		dataStart = writeCheckpointEventHeader(Generic, 1);

		/* class ID */
		_bufferWriter->writeLEB128(ThreadGroupID);

		/* number of states */
		_bufferWriter->writeLEB128(_constantPoolTypes.getThreadGroupCount());

		ThreadGroupEntry *entry = _constantPoolTypes.getThreadGroupEntry();
		while (NULL != entry) {
			/* write index */
			_bufferWriter->writeLEB128(entry->index);

			/* write parent index */
			_bufferWriter->writeLEB128(entry->parentIndex);

			/* thread group name */
			writeUTF8String(entry->name);

			entry = entry->next;
		}

		/* write size */
		writeEventSize(dataStart);
	}

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writeFrameTypeCheckpointEvent()
{
	U_8 *dataStart = writeCheckpointEventHeader(Generic, 1);

	/* class ID */
	_bufferWriter->writeLEB128(FrameTypeID);

	/* number of states */
	_bufferWriter->writeLEB128(FrameTypeCount);

	const char* frameTypeNames[] = {
		"INTERPRETED",
		"JIT",
		"JIT_INLINED",
		"NATIVE"
	};

	for (int i = 0; i < FrameTypeCount; i++) {
		/* constant index */
		_bufferWriter->writeLEB128(i);

		/* string encoding */
		_bufferWriter->writeLEB128(UTF8);

		/* string length */
		UDATA len = strlen(frameTypeNames[i]);
		_bufferWriter->writeLEB128(len);

		/* write string */
		/* TODO need to think about UTF8 enconding */
		_bufferWriter->writeData((U_8 *)frameTypeNames[i], len);
	}

	/* write size */
	writeEventSize(dataStart);

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writeSymbolTableCheckpointEvent()
{
	U_8 *dataStart = NULL;

	if (_constantPoolTypes.getPackageCount() > 0) {
		dataStart = writeCheckpointEventHeader(Generic, 1);

		/* class ID */
		_bufferWriter->writeLEB128(SymbolID);

		/* number of states */
		UDATA stringCount = _constantPoolTypes.getStringUTF8Count();
		UDATA packageCount = _constantPoolTypes.getPackageCount();
		_bufferWriter->writeLEB128(stringCount + packageCount);

		for (UDATA i = 0; i < stringCount; i++) {
			StringUTF8Entry *stringEntry = (StringUTF8Entry*)_constantPoolTypes.getSymbolTableEntry(i);

			/* write index */
			_bufferWriter->writeLEB128(i);

			/* write string */
			writeUTF8String(stringEntry->string);
		}

		for (UDATA i = stringCount; i < (stringCount + packageCount); i++) {
			PackageEntry *packageEntry = (PackageEntry*)_constantPoolTypes.getSymbolTableEntry(i);

			/* write index */
			_bufferWriter->writeLEB128(i);

			/* write string */
			writeUTF8String(packageEntry->packageName, packageEntry->packageNameLength);
		}

		/* write size */
		writeEventSize(dataStart);
	}

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writeStacktraceCheckpointEvent()
{
	U_8 *dataStart = NULL;

	if (_constantPoolTypes.getStackTraceCount() > 0) {
		dataStart = writeCheckpointEventHeader(Generic, 1);

		/* class ID */
		_bufferWriter->writeLEB128(StackTraceID);

		/* number of states */
		_bufferWriter->writeLEB128(_constantPoolTypes.getStackTraceCount());

		StackTraceEntry *entry = _constantPoolTypes.getStackTraceEntry();
		while (NULL != entry) {
			/* write index */
			_bufferWriter->writeLEB128(entry->index);

			/* is truncated */
			_bufferWriter->writeBoolean(entry->truncated);

			/* number of stack frames */
			UDATA framesCount = entry->numOfFrames;
			_bufferWriter->writeLEB128(framesCount);

			for (UDATA i = 0; i < framesCount; i++) {
				StackFrame *frame = entry->frames + i;

				/* method index */
				_bufferWriter->writeLEB128(frame->methodIndex);

				/* line number */
				_bufferWriter->writeLEB128(frame->lineNumber);

				/* bytecode index */
				_bufferWriter->writeLEB128(frame->bytecodeIndex);

				/* frame type index */
				_bufferWriter->writeLEB128(frame->frameType);
			}

			entry = entry->next;
		}

		/* write size */
		writeEventSize(dataStart);
	}

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writeJVMInformationEvent()
{
	JVMInformationEntry *jvmInfo= &(VM_JFRConstantPoolTypes::getJFRConstantEvents(_vm)->JVMInfoEntry);

	/* reserve size field */
	U_8 *dataStart = reserveEventSize();

	/* write event type */
	_bufferWriter->writeLEB128(JVMInformationID);

	/* write start time */
	_bufferWriter->writeLEB128(j9time_nano_time());

	/* write JVM name */
	writeStringLiteral(jvmInfo->jvmName);

	/* write JVM version */
	writeStringLiteral(jvmInfo->jvmVersion);

	/* write JVM arguments */
	writeStringLiteral(jvmInfo->jvmArguments);

	/* write JVM flags */
	writeStringLiteral(jvmInfo->jvmFlags);

	/* write Java arguments */
	writeStringLiteral(jvmInfo->javaArguments);

	/* write JVM start time */
	_bufferWriter->writeLEB128(jvmInfo->jvmStartTime);

	/* write pid */
	_bufferWriter->writeLEB128(jvmInfo->pid);

	/* write size */
	writeEventSize(dataStart);

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writePhysicalMemoryEvent()
{
	/* reserve size field */
	U_8 *dataStart = reserveEventSize();

	_bufferWriter->writeLEB128(PhysicalMemoryID);

	/* write start time */
	_bufferWriter->writeLEB128(j9time_nano_time());

	J9MemoryInfo memInfo = {0};
	I_32 rc = j9sysinfo_get_memory_info(&memInfo);
	if (0 == rc) {
		/* write total size */
		_bufferWriter->writeLEB128(memInfo.totalPhysical);
		/* write used size */
		_bufferWriter->writeLEB128(memInfo.totalPhysical - memInfo.availPhysical);
	} else {
		_buildResult = InternalVMError;
	}
	/* write size */
	writeEventSize(dataStart);

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writeCPUInformationEvent()
{
	CPUInformationEntry *cpuInfo= &(VM_JFRConstantPoolTypes::getJFRConstantEvents(_vm)->CPUInfoEntry);

	/* reserve size field */
	U_8 *dataStart = reserveEventSize();

	/* write event type */
	_bufferWriter->writeLEB128(CPUInformationID);

	/* write start time */
	_bufferWriter->writeLEB128(j9time_nano_time());

	/* write CPU type */
	writeStringLiteral(cpuInfo->cpu);

	/* write CPU description */
	writeStringLiteral(cpuInfo->description);

	/* write CPU sockets */
	_bufferWriter->writeLEB128(cpuInfo->sockets);

	/* write CPU cores */
	_bufferWriter->writeLEB128(cpuInfo->cores);

	/* write CPU hardware threads */
	_bufferWriter->writeLEB128(cpuInfo->hwThreads);

	/* write size */
	writeEventSize(dataStart);

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writeVirtualizationInformationEvent()
{
	VirtualizationInformationEntry *virtualizationInfo= &(VM_JFRConstantPoolTypes::getJFRConstantEvents(_vm)->VirtualizationInfoEntry);

	/* reserve size field */
	U_8 *dataStart = reserveEventSize();

	/* write event type */
	_bufferWriter->writeLEB128(VirtualizationInformationID);

	/* write start time */
	_bufferWriter->writeLEB128(j9time_nano_time());

	/* write virtualization name */
	writeStringLiteral(virtualizationInfo->name);

	/* write size */
	writeEventSize(dataStart);

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writeOSInformationEvent()
{
	OSInformationEntry *osInfo = &(VM_JFRConstantPoolTypes::getJFRConstantEvents(_vm)->OSInfoEntry);

	/* reserve size field */
	U_8 *dataStart = reserveEventSize();

	/* write event type */
	_bufferWriter->writeLEB128(OSInformationID);

	/* write start time */
	_bufferWriter->writeLEB128(j9time_nano_time());

	/* write OS version */
	writeStringLiteral(osInfo->osVersion);

	/* write size */
	writeEventSize(dataStart);

	return dataStart;
}

void
VM_JFRChunkWriter::writeInitialSystemPropertyEvents(J9JavaVM *vm)
{
	pool_state walkState;

	J9VMSystemProperty *property = (J9VMSystemProperty *)pool_startDo(vm->systemProperties, &walkState);
	while (property != NULL) {
		/* reserve size field */
		U_8 *dataStart = reserveEventSize();

		/* write event type */
		_bufferWriter->writeLEB128(InitialSystemPropertyID);

		/* write start time */
		_bufferWriter->writeLEB128(j9time_nano_time());

		/* write key */
		writeStringLiteral(property->name);

		/* write value */
		writeStringLiteral(property->value);

		/* write size */
		writeEventSize(dataStart);

		property = (J9VMSystemProperty *)pool_nextDo(&walkState);
	}
}

void
VM_JFRChunkWriter::writeInitialEnvironmentVariableEvents()
{
	J9SysinfoEnvIteratorState envState;
	memset(&envState, 0, sizeof(envState));

	/* Call init with zero length buffer to get the required buffer size */
	int32_t result = j9sysinfo_env_iterator_init(&envState, NULL, 0);

	if (result >= 0) {
		int32_t bufferSize = result;
		void *buffer = j9mem_allocate_memory(bufferSize, OMRMEM_CATEGORY_VM);

		if (NULL != buffer) {
			J9SysinfoEnvElement envElement = {NULL};

			result = j9sysinfo_env_iterator_init(&envState, buffer, bufferSize);

			if (result >= 0) {
				while (j9sysinfo_env_iterator_hasNext(&envState)) {
					result = j9sysinfo_env_iterator_next(&envState, &envElement);

					if (0 == result) {
						/* reserve size field */
						U_8 *dataStart = reserveEventSize();
						const char *equalChar = strchr(envElement.nameAndValue, '=');

						/* write event type */
						_bufferWriter->writeLEB128(InitialEnvironmentVariableID);

						/* write start time */
						_bufferWriter->writeLEB128(j9time_nano_time());
						/* write key */
						writeUTF8String((U_8 *)envElement.nameAndValue, equalChar - envElement.nameAndValue);

						/* write value */
						writeStringLiteral(equalChar + 1);

						/* write size */
						writeEventSize(dataStart);
					}
				}
			}

			j9mem_free_memory(buffer);
		}
	}
}

void
VM_JFRChunkWriter::writeClassLoadingStatisticsEvent(void *anElement, void *userData)
{
	ClassLoadingStatisticsEntry *entry = (ClassLoadingStatisticsEntry  *)anElement;
	VM_BufferWriter *_bufferWriter = (VM_BufferWriter *)userData;

	/* reserve size field */
	U_8 *dataStart = reserveEventSize(_bufferWriter );

	/* write event type */
	_bufferWriter->writeLEB128(ClassLoadingStatisticsID);

	/* write start time */
	_bufferWriter->writeLEB128(entry->ticks);

	/* write user loaded class count */
	_bufferWriter->writeLEB128(entry->loadedClassCount);

	/* write system unloaded class count */
	_bufferWriter->writeLEB128(entry->unloadedClassCount);

	/* write size */
	writeEventSize(_bufferWriter, dataStart);
}

void
VM_JFRChunkWriter::writeThreadContextSwitchRateEvent(void *anElement, void *userData)
{
	ThreadContextSwitchRateEntry *entry = (ThreadContextSwitchRateEntry *)anElement;
	VM_BufferWriter *_bufferWriter = (VM_BufferWriter *)userData;

	/* reserve size field */
	U_8 *dataStart = reserveEventSize(_bufferWriter);

	/* write event type */
	_bufferWriter->writeLEB128(ThreadContextSwitchRateID);

	/* write start time */
	_bufferWriter->writeLEB128(entry->ticks);

	/* write switch rate */
	_bufferWriter->writeFloat(entry->switchRate);

	/* write size */
	writeEventSize(_bufferWriter, dataStart);
}

void
VM_JFRChunkWriter::writeThreadStatisticsEvent(void *anElement, void *userData)
{
	ThreadStatisticsEntry *entry = (ThreadStatisticsEntry *)anElement;
	VM_BufferWriter *_bufferWriter = (VM_BufferWriter *)userData;

	/* reserve event size */
	U_8 *dataStart = reserveEventSize(_bufferWriter);

	/* write event type */
	_bufferWriter->writeLEB128(ThreadStatisticsID);

	/* write start ticks */
	_bufferWriter->writeLEB128(entry->ticks);

	/* write active thread count */
	_bufferWriter->writeLEB128(entry->activeThreadCount);

	/* write daemon thread count */
	_bufferWriter->writeLEB128(entry->daemonThreadCount);

	/* write accumulated thread count */
	_bufferWriter->writeLEB128(entry->accumulatedThreadCount);

	/* write peak thread count */
	_bufferWriter->writeLEB128(entry->peakThreadCount);

	/* write size */
	writeEventSize(_bufferWriter, dataStart);
}

static void
writeObject(J9JavaVM *vm, j9object_t obj, VM_BufferWriter *bufferWriter)
{
	J9ROMClass *romClass = NULL;
	if (J9VM_IS_INITIALIZED_HEAPCLASS_VM(vm, obj)) {
		romClass = J9VM_J9CLASS_FROM_HEAPCLASS_VM(vm, obj)->romClass;
	} else {
		romClass = J9OBJECT_CLAZZ_VM(vm, obj)->romClass;
	}

	J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);
	bufferWriter->writeFormattedString("%.*s@%p", J9UTF8_LENGTH(className), J9UTF8_DATA(className), obj);
}

static UDATA
stackWalkCallback(J9VMThread *vmThread, J9StackWalkState *state)
{
	J9JavaVM *vm = vmThread->javaVM;
	J9ObjectMonitorInfo *monitorInfo = (J9ObjectMonitorInfo *)state->userData2;
	IDATA *monitorCount = (IDATA *)(&state->userData3);
	J9Method *method = state->method;
	J9Class *methodClass = J9_CLASS_FROM_METHOD(method);
	J9UTF8 *className = J9ROMCLASS_CLASSNAME(methodClass->romClass);
	J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	J9UTF8 *methodName = J9ROMMETHOD_NAME(romMethod);

	VM_BufferWriter *bufferWriter = (VM_BufferWriter *)state->userData1;

	bufferWriter->writeFormattedString(
			"at %.*s.%.*s",
			J9UTF8_LENGTH(className), J9UTF8_DATA(className),
			J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName));

	if (J9_ARE_ANY_BITS_SET(romMethod->modifiers, J9AccNative)) {
		bufferWriter->writeFormattedString("(Native Method)\n");
	} else {
		UDATA offsetPC = state->bytecodePCOffset;
		bool compiledMethod = (NULL != state->jitInfo);
		J9UTF8 *sourceFile = getSourceFileNameForROMClass(vm, methodClass->classLoader, methodClass->romClass);
		if (NULL != sourceFile) {
			bufferWriter->writeFormattedString(
					"(%.*s", J9UTF8_LENGTH(sourceFile), J9UTF8_DATA(sourceFile));

			UDATA lineNumber = getLineNumberForROMClass(vm, method, offsetPC);

			if ((UDATA)-1 != lineNumber) {
				bufferWriter->writeFormattedString(":%zu", lineNumber);
			}

			if (compiledMethod) {
				bufferWriter->writeFormattedString("(Compiled Code)");
			}

			bufferWriter->writeFormattedString(")\n");
		} else {
			bufferWriter->writeFormattedString("(Bytecode PC: %zu", offsetPC);
			if (compiledMethod) {
				bufferWriter->writeFormattedString("(Compiled Code)");
			}
			bufferWriter->writeFormattedString(")\n");
		}

		/* Use a while loop as there may be more than one lock taken in a stack frame. */
		while ((0 != *monitorCount) && ((UDATA)monitorInfo->depth == state->framesWalked)) {
			bufferWriter->writeFormattedString("\t(entered lock: ");
			writeObject(vm, monitorInfo->object, bufferWriter);
			bufferWriter->writeFormattedString(")\n");

			monitorInfo += 1;
			state->userData2 = monitorInfo;

			(*monitorCount) -= 1;
		}
	}

	return J9_STACKWALK_KEEP_ITERATING;
}

static void
writeThreadInfo(J9VMThread *currentThread, J9VMThread *walkThread, VM_BufferWriter *bufferWriter)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	UDATA javaTID = J9VMJAVALANGTHREAD_TID(currentThread, walkThread->threadObject);
	UDATA osTID = ((J9AbstractThread *)walkThread->osThread)->tid;
	UDATA javaPriority = vmFuncs->getJavaThreadPriority(vm, walkThread);
	UDATA state = J9VMTHREAD_STATE_UNKNOWN;
	const char *stateStr = "?";
	j9object_t monitorObject = NULL;
	char *threadName = NULL;

	/* Get thread state and monitor */
	state = getVMThreadObjectState(walkThread, &monitorObject, NULL, NULL);
	switch (state) {
	case J9VMTHREAD_STATE_RUNNING:
		stateStr = "R";
		break;
	case J9VMTHREAD_STATE_BLOCKED:
		stateStr = "B";
		break;
	case J9VMTHREAD_STATE_WAITING:
	case J9VMTHREAD_STATE_WAITING_TIMED:
	case J9VMTHREAD_STATE_SLEEPING:
		stateStr = "CW";
		break;
	case J9VMTHREAD_STATE_PARKED:
	case J9VMTHREAD_STATE_PARKED_TIMED:
		stateStr = "P";
		break;
	case J9VMTHREAD_STATE_SUSPENDED:
		stateStr = "S";
		break;
	case J9VMTHREAD_STATE_DEAD:
		stateStr = "Z";
		break;
	case J9VMTHREAD_STATE_INTERRUPTED:
		stateStr = "I";
		break;
	case J9VMTHREAD_STATE_UNKNOWN:
		stateStr = "?";
		break;
	default:
		stateStr = "??";
		break;
	}

/* Get thread name */
#if JAVA_SPEC_VERSION >= 21
	if (IS_JAVA_LANG_VIRTUALTHREAD(currentThread, walkThread->threadObject)) {
		/* For VirtualThread, get name from threadObject directly. */
		j9object_t nameObject = J9VMJAVALANGTHREAD_NAME(currentThread, walkThread->threadObject);
		threadName = getVMThreadNameFromString(currentThread, nameObject);
	} else
#endif /* JAVA_SPEC_VERSION >= 21 */
	{
		threadName = getOMRVMThreadName(walkThread->omrVMThread);
		releaseOMRVMThreadName(walkThread->omrVMThread);
	}
	bufferWriter->writeFormattedString(
			"\"%s\" J9VMThread: %p tid: %zd nid: %zd prio: %zd state: %s raw state: 0x%zX",
			threadName,
			walkThread,
			javaTID,
			osTID,
			javaPriority,
			stateStr,
			state);

	if (J9VMTHREAD_STATE_BLOCKED == state) {
		bufferWriter->writeFormattedString(" blocked on: ");
	} else if ((J9VMTHREAD_STATE_WAITING == state) || (J9VMTHREAD_STATE_WAITING_TIMED == state)) {
		bufferWriter->writeFormattedString(" waiting on: ");
	} else if ((J9VMTHREAD_STATE_PARKED == state) || (J9VMTHREAD_STATE_PARKED_TIMED == state)) {
		bufferWriter->writeFormattedString(" parked on: ");
	} else {
		bufferWriter->writeFormattedString("\n");
		return;
	}

	if (NULL != monitorObject) {
		writeObject(vm, monitorObject, bufferWriter);
	} else {
		bufferWriter->writeFormattedString("<unknown>");
	}
	bufferWriter->writeFormattedString("\n");
}

static void
writeStacktrace(J9VMThread *currentThread, J9VMThread *walkThread, VM_BufferWriter *bufferWriter)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9StackWalkState stackWalkState = {0};
	const size_t maxMonitorInfosPerThread = 32;
	J9ObjectMonitorInfo monitorInfos[maxMonitorInfosPerThread];
	memset(monitorInfos, 0, sizeof(monitorInfos));

	IDATA monitorCount = vmFuncs->getOwnedObjectMonitors(currentThread, walkThread, monitorInfos, maxMonitorInfosPerThread, FALSE);

	stackWalkState.walkThread = walkThread;
	stackWalkState.flags =
			J9_STACKWALK_ITERATE_FRAMES
			| J9_STACKWALK_INCLUDE_NATIVES
			| J9_STACKWALK_VISIBLE_ONLY
			| J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET
			| J9_STACKWALK_NO_ERROR_REPORT;
	stackWalkState.skipCount = 0;
	stackWalkState.frameWalkFunction = stackWalkCallback;
	stackWalkState.userData1 = bufferWriter;
	stackWalkState.userData2 = monitorInfos;
	stackWalkState.userData3 = (void *)monitorCount;

	vmFuncs->haltThreadForInspection(currentThread, walkThread);
	vm->walkStackFrames(currentThread, &stackWalkState);
	vmFuncs->resumeThreadForInspection(currentThread, walkThread);

	bufferWriter->writeFormattedString("\n");
}

U_8 *
VM_JFRChunkWriter::writeThreadDumpEvent()
{
	/* reserve size field */
	U_8 *dataStart = reserveEventSize();

	_bufferWriter->writeLEB128(ThreadDumpID);

	/* write start time */
	_bufferWriter->writeLEB128(j9time_current_time_millis());

	const U_64 bufferSize = THREAD_DUMP_EVENT_SIZE_PER_THREAD * _vm->peakThreadCount;
	U_8 *resultBuffer = (U_8 *)j9mem_allocate_memory(sizeof(U_8) * bufferSize, OMRMEM_CATEGORY_VM);

	if (NULL != resultBuffer) {
		VM_BufferWriter resultWriter(privatePortLibrary, resultBuffer, bufferSize);
		J9VMThread *walkThread = J9_LINKED_LIST_START_DO(_vm->mainThread);
		UDATA numThreads = 0;
		J9InternalVMFunctions *vmFuncs = _vm->internalVMFunctions;

		Assert_VM_mustHaveVMAccess(_currentThread);
		vmFuncs->acquireExclusiveVMAccess(_currentThread);

		while (NULL != walkThread) {
			writeThreadInfo(_currentThread, walkThread, &resultWriter);
			writeStacktrace(_currentThread, walkThread, &resultWriter);

			walkThread = J9_LINKED_LIST_NEXT_DO(_vm->mainThread, walkThread);
			numThreads += 1;
		}
		resultWriter.writeFormattedString("Number of threads: %zd", numThreads);

		vmFuncs->releaseExclusiveVMAccess(_currentThread);

		writeUTF8String(resultWriter.getBufferStart(), resultWriter.getSize());
		j9mem_free_memory(resultBuffer);
	} else {
		_buildResult = OutOfMemory;
	}

	/* write size */
	writeEventSize(dataStart);

	return dataStart;
}
#endif /* defined(J9VM_OPT_JFR) */
