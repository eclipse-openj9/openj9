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

#if !defined(JFRUTILS_HPP_)
#define JFRUTILS_HPP_

#include "j9.h"
#include "vm_api.h"
#include "VMHelpers.hpp"

#if defined(J9VM_OPT_JFR)

enum JfrBuildResult {
	OK,
	OutOfMemory,
	InternalVMError,
	FileIOError,
	EnvVarsNotSet,
	TimeFailure,
	MetaDataFileNotLoaded,
};

class VM_JFRUtils {
	/*
	 * Data members
	 */
private:

protected:

public:
	static constexpr int THREAD_DUMP_EVENT_SIZE_PER_THREAD = 1000;

	/*
	 * Function members
	 */
private:

protected:

public:

	static U_64 getCurrentTimeNanos(J9PortLibrary *privatePortLibrary, JfrBuildResult &buildResult)
	{
		UDATA success = 0;
		U_64 result = (U_64) j9time_current_time_nanos(&success);

		if (success == 0) {
			buildResult = TimeFailure;

		}

		return result;
	}

	static void
	getClassloaderStats(J9JavaVM *vm, J9ClassLoader *classLoader, I_64 *classCount, U_64 *chunkSize, I_64 *blockSize)
	{
		if (classLoader == vm->anonClassLoader) {
			*classCount = vm->anonClassCount;
		} else {
			*classCount = classLoader->loadedClassCount;
		}

		J9MemorySegment *segment = classLoader->classSegments;
		while (NULL != segment) {
			*chunkSize += segment->heapAlloc - segment->baseAddress;
			segment = segment->nextSegmentInClassLoader;
		}

		*blockSize = (I_64)(*chunkSize);

		J9RAMClassFreeLists *sub4gBlockPtr = &classLoader->sub4gBlock;
		J9RAMClassFreeLists *frequentlyAccessedBlockPtr = &classLoader->frequentlyAccessedBlock;
		J9RAMClassFreeLists *inFrequentlyAccessedBlockPtr = &classLoader->inFrequentlyAccessedBlock;
		UDATA *ramClassSub4gUDATABlockFreeListPtr = sub4gBlockPtr->ramClassUDATABlockFreeList;
		UDATA *ramClassFreqUDATABlockFreeListPtr = frequentlyAccessedBlockPtr->ramClassUDATABlockFreeList;
		UDATA *ramClassInFreqUDATABlockFreeListPtr = inFrequentlyAccessedBlockPtr->ramClassUDATABlockFreeList;

		VM_VMHelpers::subtractUDATABlockChain(ramClassSub4gUDATABlockFreeListPtr, blockSize);
		VM_VMHelpers::subtractUDATABlockChain(ramClassFreqUDATABlockFreeListPtr, blockSize);
		VM_VMHelpers::subtractUDATABlockChain(ramClassInFreqUDATABlockFreeListPtr, blockSize);

		VM_VMHelpers::subtractFreeListBlocks(sub4gBlockPtr, blockSize);
		VM_VMHelpers::subtractFreeListBlocks(frequentlyAccessedBlockPtr, blockSize);
		VM_VMHelpers::subtractFreeListBlocks(inFrequentlyAccessedBlockPtr, blockSize);
	}


};

#endif /* defined(J9VM_OPT_JFR) */

#endif /* JFRCHUNKWRITER_HPP_ */
