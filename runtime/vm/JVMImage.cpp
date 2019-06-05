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

#include "JVMImage.hpp"

#define OMRPORT_FROM_IMAGE JVMImage::getJVMImage()->getPortLibrary();

JVMImage::JVMImage(J9JavaVM *javaVM) :
	_memoryStart(0),
	_size(0),
	_isImageAllocated(false)
{
	OMRPORT_ACCESS_FROM_J9PORT(javaVM->portLibrary);
	memset(&_portLibrary, 0, sizeof(OMRPortLibrary));
	memcpy(&_portLibrary, privateOmrPortLibrary, sizeof(OMRPortLibrary));
	_portLibrary.mem_allocate_memory = image_mem_allocate_memory;
	_portLibrary.mem_free_memory = image_mem_free_memory;
}

//should be called once the image memory is allocated
bool
JVMImage::initializeMonitor()
{
	if (omrthread_monitor_init_with_name(&_jvmImageMonitor, 0, "JVM Image Heap Monitor") != 0) {
		return false;
	}
	return true;
}

void *
JVMImage::subAllocateMemory(uintptr_t byteAmount)
{
	omrthread_monitor_enter(_jvmImageMonitor);

	void* memStart = _portLibrary.heap_allocate(&_portLibrary, _heap, byteAmount);	
	// image memory is not large enough and needs to be reallocated
	if (memStart == NULL) {
		reallocateImageMemory(_size * 2 + byteAmount);
		memStart = _portLibrary.heap_allocate(&_portLibrary, _heap, byteAmount);
	}

	omrthread_monitor_exit(_jvmImageMonitor);

	return memStart;
}

void
JVMImage::freeSubAllocatedMemory(void* address)
{
	omrthread_monitor_enter(_jvmImageMonitor);

	_portLibrary.heap_free(&_portLibrary, _heap, address);

	omrthread_monitor_exit(_jvmImageMonitor);
}

void
JVMImage::reallocateImageMemory(UDATA size)
{
	return;
}

JVMImage *
JVMImage::getJVMImage() {
	return NULL;
}


extern "C" void *
image_mem_allocate_memory(struct OMRPortLibrary* portLibrary, uintptr_t byteAmount, const char* callSite, uint32_t category)
{
	void *pointer = NULL;
	
	JVMImage *jvmImage = JVMImage::getJVMImage();
	pointer = jvmImage->subAllocateMemory(byteAmount);
	return pointer;
}

extern "C" void
image_mem_free_memory(struct OMRPortLibrary* portLibrary, void* memoryPointer)
{
	JVMImage* jvmImage = JVMImage::getJVMImage();
	jvmImage->freeSubAllocatedMemory(memoryPointer);
}
