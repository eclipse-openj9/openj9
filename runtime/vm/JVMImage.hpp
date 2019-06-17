/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
/*
* JVMImage.hpp
*/

#ifndef JVMIMAGE_HPP_
#define JVMIMAGE_HPP_

#include "j9.h"
#include "j9comp.h"
#include "j9protos.h"
#include "ut_j9vm.h"

#include <sys/mman.h>

class JVMImage
{
public:
	JVMImage(J9JavaVM *vm);
	~JVMImage();

	static JVMImage* createInstance(J9JavaVM *javaVM);
	static JVMImage* getInstance();

	ImageRC setupWarmRun();
	ImageRC setupColdRun();

	void* subAllocateMemory(uintptr_t byteAmount);
	void* reallocateMemory(void *address, uintptr_t byteAmount); //TODO: Extension functions for heap (not used currently)
	void freeSubAllocatedMemory(void *memStart); //TODO: Extension functions for heap (not used currently)

	void registerEntryInTable(ImageTableHeader *table, UDATA entry);

	void destroyMonitor();

	OMRPortLibrary* getPortLibrary() { return _portLibrary; }
	ImageTableHeader* getClassLoaderTable() { return WSRP_GET(_jvmImageData->classLoaderTable, ImageTableHeader*); }
	ImageTableHeader* getClassSegmentTable() { return WSRP_GET(_jvmImageData->classSegmentTable, ImageTableHeader*); }
	ImageTableHeader* getClassPathEntryTable() { return WSRP_GET(_jvmImageData->classPathEntryTable, ImageTableHeader*); }
protected:
	void *operator new(size_t size, void *memoryPointer) { return memoryPointer; }
public
	static const UDATA INITIAL_HEAP_SIZE;
	static const UDATA INITIAL_IMAGE_SIZE;
private:
	static JVMImage *_jvmInstance;
	
	J9JavaVM *_vm;

	JVMImageData *_jvmImageData;
	J9Heap *_heap;

	OMRPortLibrary *_portLibrary;
	omrthread_monitor_t _jvmImageMonitor;

	char *_dumpFileName;
	bool _isWarmRun;

	bool initializeMonitor();

	void* allocateImageMemory(UDATA size);
	void* reallocateImageMemory(UDATA size);
	void* initializeHeap();

	bool allocateImageTableHeaders();
	void* allocateTable(ImageTableHeader *table, uintptr_t tableSize);
	void* reallocateTable(ImageTableHeader *table, uintptr_t tableSize);

	bool readImageFromFile();
	bool writeImageToFile();
};

#endif /* JVMIMAGE_H_ */
