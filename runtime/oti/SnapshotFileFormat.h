/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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

#ifndef snapshotfileformat_h
#define snapshotfileformat_h

#include "j9cfg.h"

#if defined(J9VM_OPT_SNAPSHOTS)

#include "j9nonbuilder.h"

/**
 * TODO: This will not be needed once the J9JavaVM
 * structure is properly persisted
 */
typedef struct SavedJ9JavaVMStructures {
	J9Pool *classLoaderBlocks;
	J9MemorySegmentList *classMemorySegments;
	J9MemorySegmentList *memorySegments;
	J9Class *voidReflectClass;
	J9Class *booleanReflectClass;
	J9Class *charReflectClass;
	J9Class *floatReflectClass;
	J9Class *doubleReflectClass;
	J9Class *byteReflectClass;
	J9Class *shortReflectClass;
	J9Class *intReflectClass;
	J9Class *longReflectClass;
	J9Class *booleanArrayClass;
	J9Class *charArrayClass;
	J9Class *floatArrayClass;
	J9Class *doubleArrayClass;
	J9Class *byteArrayClass;
	J9Class *shortArrayClass;
	J9Class *intArrayClass;
	J9Class *longArrayClass;
	J9ClassLoader *systemClassLoader;
	J9ClassLoader *extensionClassLoader;
	J9ClassLoader *applicationClassLoader;
	J9ClassLoader *anonClassLoader;
	J9HiddenInstanceField *hiddenInstanceFields;
	VMSnapshotImplPortLibrary *vmSnapshotImplPortLibrary;
} SavedJ9JavaVMStructures;

/*
 * The memoryRegion header entry. Currently there are
 * a fix number of memory regions, this will change in
 * the future.
 *
 * Fields:
 * fileOffset: The offset in the snapshot file where the region resides. Currently this
 * 			must be page aligned in order to map the memory in. In the future this restriction
 * 			will be removed once shared libraries are generated.
 * startAddr: The address in which the memoryRegion is located
 * alignedStartAddr: The address in which the memoryRegion is located shifted up to the page boundary.
 * 			This is the address at which the memoryRegion will be mapped in on the restore run.
 * totalSize: Size of the memoryRegion including padding
 * mappableSize: Size of the memoryRegion excluding padding
 * permissions: Protection flags for the memoryRegion
 *
 * type: This is the type of  memory region. Right now there are two , this will increase in the future.
 * 			If we decide to be more thrifty on footprint we can bundle this with permissions.
 *
 * TODO Note: Page alignment will be less of a concern when we start writing to shared libraries. In
 * 			future mappableSize and alignedStartAddr may not be needed.
 *
 */
typedef struct J9MemoryRegion {
	UDATA fileOffset;
	void *startAddr;
	void *alignedStartAddr;
	UDATA totalSize;
	UDATA mappableSize;
	UDATA permissions;
	J9MemoryRegionType type;
} J9MemoryRegion;

/*
 * Struct containing data about image heap and quick access variables
 *
 * Allows us to dump this struct into file and reload easier
 * Allocated space for image data is longer than sizeof(JVMImageHeader) and includes heap. see @ref JVMImage::allocateImageMemory
 * All quick access variables are J9WSRP to allow reallocation in future. TODO: Allow heap reallocation
 */
typedef struct J9SnapshotHeader {
	UDATA imageSize; /* image size in bytes */
	J9JavaVM *vm;
	SavedJ9JavaVMStructures savedJavaVMStructs;
	UDATA numOfMemorySections;
	J9JITConfig *jitConfig;
	J9GCSnapshotProperties gcProperties;
} J9SnapshotHeader;

/*
 * Snapshot file layout
 *
 * --------------------
 *   SnapshotHeader
 * --------------------
 *   MemoryRegion header
 * --------------------
 *   MemoryRegion 1
 * --------------------
 *         ...
 * --------------------
 *   MemoryRegion N
 * --------------------
 */

#endif /* defined(J9VM_OPT_SNAPSHOTS) */
#endif /* snapshotfileformat_h */
