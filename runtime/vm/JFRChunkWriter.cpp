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
	U_8 *dataStart = _bufferWriter->getAndIncCursor(sizeof(U_32));

	if (_debug) {
		j9tty_printf(PORTLIB, "Metadata frame start offset = 0x%lX\n", _bufferWriter->getFileOffsetFromStart(dataStart));
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
		j9tty_printf(PORTLIB, "Metadata blob start offset = 0x%lX, size = %d\n", _bufferWriter->getFileOffsetFromStart(_bufferWriter->getCursor()), (int) _vm->jfrState.metaDataBlobFileSize);
	}

	/* metadata blob file */
	_bufferWriter->writeData(_vm->jfrState.metaDataBlobFile, _vm->jfrState.metaDataBlobFileSize);

	if (_debug) {
		j9tty_printf(PORTLIB, "Metadata blob size from LEB128 = %u\n",  VM_BufferWriter::convertFromLEB128ToU32(blobStart));
	}

	/* add metadata size */
	_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);

	if (_debug) {
		j9tty_printf(PORTLIB, "Metadata size = %d, fromLEB128 =%u\n", (int)(_bufferWriter->getCursor() - dataStart), VM_BufferWriter::convertFromLEB128ToU32(dataStart));
	}

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writeCheckpointEventHeader(CheckpointTypeMask typeMask, U_32 cpCount)
{
	/* write delta offset in previous checkpoint event */
	if (NULL != _previousCheckpointDelta) {
		_bufferWriter->writeLEB128PaddedU64(_previousCheckpointDelta,(U_64) _bufferWriter->getFileOffset(_bufferWriter->getCursor(), _lastDataStart));
	}

	/* reserve size field */
	U_8 *dataStart = _bufferWriter->getAndIncCursor(sizeof(U_32));
	_lastDataStart = dataStart;

	if (_debug) {
		j9tty_printf(PORTLIB, "Checkpoint event frame start offset = 0x%lX\n", _bufferWriter->getFileOffsetFromStart(dataStart));
	}

	/* write checkpoint header */
	_bufferWriter->writeU8(EventCheckpoint);

	/* start time */
	_bufferWriter->writeLEB128(j9time_nano_time());

	/* duration */
	_bufferWriter->writeLEB128((U_64)0);

	/* reserve delta offset to next checkpoint event */
	_previousCheckpointDelta = _bufferWriter->getCursor();
	_bufferWriter->writeLEB128PaddedU64(0);

	if (_debug) {
		j9tty_printf(PORTLIB, "next pointer=%d val=%d\n",(int) _bufferWriter->getFileOffsetFromStart(_previousCheckpointDelta), (int) VM_BufferWriter::convertFromLEB128ToU64(_previousCheckpointDelta));
	}

	/* type mask */
	_bufferWriter->writeU8(typeMask);

	/* constant pool count */
	_bufferWriter->writeLEB128(cpCount);

	return dataStart;
}

void
VM_JFRChunkWriter::writeUTF8String(J9UTF8* string)
{
	if (NULL == string) {
		_bufferWriter->writeLEB128(NullString);
	} else {
		writeUTF8String(J9UTF8_DATA(string), J9UTF8_LENGTH(string));
	}
}

void
VM_JFRChunkWriter::writeUTF8String(U_8 *data, UDATA len)
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
		U_32 len = strlen(threadStateNames[i]);
		_bufferWriter->writeLEB128(len);

		/* write string */ // need to think about UTF8 enconding
		_bufferWriter->writeData((U_8 *)threadStateNames[i], len);
	}

	/* write size */
	_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);

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
			_bufferWriter->writeU8(entry->exported);

			entry = entry->next;
		}

		/* write size */
		_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);
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
			_bufferWriter->writeU8(entry->hidden);

			entry = entry->next;
		}

		/* write size */
		_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);
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
		_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);
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
			_bufferWriter->writeU8(entry->hidden);

			entry = entry->next;
		}

		/* write size */
		_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);
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
		_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);
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
		_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);
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
		_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);
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
		U_32 len = strlen(frameTypeNames[i]);
		_bufferWriter->writeLEB128(len);

		/* write string */
		/* TODO need to think about UTF8 enconding */
		_bufferWriter->writeData((U_8 *)frameTypeNames[i], len);
	}

	/* write size */
	_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);

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
		_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);
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
			_bufferWriter->writeU8(entry->truncated);

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
		_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);
	}

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writeJVMInformationEvent()
{
	JVMInformationEntry *jvmInfo= &(VM_JFRConstantPoolTypes::getJFRConstantEvents(_vm)->JVMInfoEntry);

	/* reserve size field */
	U_8 *dataStart = _bufferWriter->getAndIncCursor(sizeof(U_32));

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
	_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writePhysicalMemoryEvent()
{
	/* reserve size field */
	U_8 *dataStart = _bufferWriter->getAndIncCursor(sizeof(U_32));

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
	_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writeCPUInformationEvent()
{
	CPUInformationEntry *cpuInfo= &(VM_JFRConstantPoolTypes::getJFRConstantEvents(_vm)->CPUInfoEntry);

	/* reserve size field */
	U_8 *dataStart = _bufferWriter->getAndIncCursor(sizeof(U_32));

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
	_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writeVirtualizationInformationEvent()
{
	VirtualizationInformationEntry *virtualizationInfo= &(VM_JFRConstantPoolTypes::getJFRConstantEvents(_vm)->VirtualizationInfoEntry);

	/* reserve size field */
	U_8 *dataStart = _bufferWriter->getAndIncCursor(sizeof(U_32));

	/* write event type */
	_bufferWriter->writeLEB128(VirtualizationInformationID);

	/* write start time */
	_bufferWriter->writeLEB128(j9time_nano_time());

	/* write virtualization name */
	writeStringLiteral(virtualizationInfo->name);

	/* write size */
	_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);

	return dataStart;
}

U_8 *
VM_JFRChunkWriter::writeOSInformationEvent()
{
	OSInformationEntry *osInfo = &(VM_JFRConstantPoolTypes::getJFRConstantEvents(_vm)->OSInfoEntry);

	/* reserve size field */
	U_8 *dataStart = _bufferWriter->getAndIncCursor(sizeof(U_32));

	/* write event type */
	_bufferWriter->writeLEB128(OSInformationID);

	/* write start time */
	_bufferWriter->writeLEB128(j9time_nano_time());

	/* write OS version */
	writeStringLiteral(osInfo->osVersion);

	/* write size */
	_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);

	return dataStart;
}

void
VM_JFRChunkWriter::writeInitialSystemPropertyEvents(J9JavaVM *vm)
{
	pool_state walkState;

	J9VMSystemProperty *property = (J9VMSystemProperty *)pool_startDo(vm->systemProperties, &walkState);
	while (property != NULL) {
		/* reserve size field */
		U_8 *dataStart = _bufferWriter->getAndIncCursor(sizeof(U_32));

		/* write event type */
		_bufferWriter->writeLEB128(InitialSystemPropertyID);

		/* write start time */
		_bufferWriter->writeLEB128(j9time_nano_time());

		/* write key */
		writeStringLiteral(property->name);

		/* write value */
		writeStringLiteral(property->value);

		/* write size */
		_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);

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
						U_8 *dataStart = _bufferWriter->getAndIncCursor(sizeof(U_32));
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
						_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);
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
	U_8 *dataStart = _bufferWriter->getAndIncCursor(sizeof(U_32));

	/* write event type */
	_bufferWriter->writeLEB128(ClassLoadingStatisticsID);

	/* write start time */
	_bufferWriter->writeLEB128(entry->ticks);

	/* write user loaded class count */
	_bufferWriter->writeLEB128(entry->loadedClassCount);

	/* write system unloaded class count */
	_bufferWriter->writeLEB128(entry->unloadedClassCount);

	/* write size */
	_bufferWriter->writeLEB128PaddedU32(dataStart, _bufferWriter->getCursor() - dataStart);
}
#endif /* defined(J9VM_OPT_JFR) */
