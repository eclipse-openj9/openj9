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

JVMImage *JVMImage::_jvmInstance = NULL;
const UDATA JVMImage::INITIAL_HEAP_SIZE = 1024 * 1024; //TODO: reallocation will fail so initial heap size is large (Should be 8 byte aligned)
const UDATA JVMImage::INITIAL_IMAGE_SIZE = sizeof(JVMImageData) + JVMImage::INITIAL_HEAP_SIZE;

JVMImage::JVMImage(J9JavaVM *javaVM) :
	_vm(javaVM),
	_jvmImageData(NULL),
	_heap(NULL)
{
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	_portLibrary = (OMRPortLibrary *) j9mem_allocate_memory(sizeof(OMRPortLibrary), OMRMEM_CATEGORY_PORT_LIBRARY);

	OMRPORT_ACCESS_FROM_J9PORT(javaVM->portLibrary);
	memset(_portLibrary, 0, sizeof(OMRPortLibrary));
	memcpy(_portLibrary, privateOmrPortLibrary, sizeof(OMRPortLibrary));
	_portLibrary->mem_allocate_memory = image_mem_allocate_memory;
	_portLibrary->mem_free_memory = image_mem_free_memory;

	_dumpFileName = javaVM->ramStateFilePath;
	_isWarmRun = J9_ARE_ALL_BITS_SET(javaVM->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_RAMSTATE_WARM_RUN);
}

JVMImage::~JVMImage()
{
	PORT_ACCESS_FROM_JAVAVM(_vm);

	j9mem_free_memory((void*)_jvmImageData);
	j9mem_free_memory((void*)_portLibrary);
	_jvmInstance = NULL;
}

bool
JVMImage::initializeMonitor()
{
	if (omrthread_monitor_init_with_name(&_jvmImageMonitor, 0, "JVM Image Heap Monitor") != 0) {
		return false;
	}

	return true;
}

void
JVMImage::destroyMonitor()
{
	omrthread_monitor_destroy(_jvmImageMonitor);
}

JVMImage *
JVMImage::createInstance(J9JavaVM *vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	_jvmInstance = (JVMImage *)j9mem_allocate_memory(sizeof(JVMImage), J9MEM_CATEGORY_CLASSES);
	if (_jvmInstance != NULL) {
		new(_jvmInstance) JVMImage(vm);   
	}

	return _jvmInstance;
}

JVMImage *
JVMImage::getInstance()
{
	return _jvmInstance;
}

//TODO: This will be expanded once warm run set up is finished
ImageRC
JVMImage::setupWarmRun()
{
	if (!readImageFromFile()) {
		return IMAGE_ERROR;
	}

	return IMAGE_OK;
}

ImageRC
JVMImage::setupColdRun()
{
	if (allocateImageMemory(INITIAL_IMAGE_SIZE) == NULL) {
		return IMAGE_ERROR;
	}

	if (initializeHeap() == NULL) {
		return IMAGE_ERROR;
	}

	if (!initializeMonitor()) {
		return IMAGE_ERROR;
	}

	if (!allocateImageTableHeaders()) {
		return IMAGE_ERROR;
	}

	if (allocateTable(getClassLoaderTable(), INITIAL_CLASSLOADER_TABLE_SIZE) == NULL
		|| allocateTable(getClassSegmentTable(), INITIAL_CLASS_TABLE_SIZE) == NULL
		|| allocateTable(getClassPathEntryTable(), INITIAL_CLASSPATH_TABLE_SIZE) == NULL) {
		return IMAGE_ERROR;
	}

	return IMAGE_OK;
}

void *
JVMImage::allocateImageMemory(UDATA size)
{
	PORT_ACCESS_FROM_JAVAVM(_vm);

	_jvmImageData = (JVMImageData *)j9mem_allocate_memory(size, J9MEM_CATEGORY_CLASSES); //TODO: change category
	if (_jvmImageData == NULL) {
		return NULL;
	}

	_jvmImageData->imageSize = size;

	return _jvmImageData;
}

//TODO: Currently reallocating image memory is broken since all references to memory inside of heap will fail (i.e. vm->classLoadingPool)
void *
JVMImage::reallocateImageMemory(UDATA size)
{
	return NULL;
}

void * 
JVMImage::initializeHeap()
{
	PORT_ACCESS_FROM_JAVAVM(_vm);
	
	_heap = j9heap_create((J9Heap *)(_jvmImageData + 1), JVMImage::INITIAL_HEAP_SIZE, 0);
	if (_heap == NULL) {
		return NULL;
	}

	return _heap;
}

bool
JVMImage::allocateImageTableHeaders()
{
	WSRP_SET(_jvmImageData->classLoaderTable, subAllocateMemory(sizeof(ImageTableHeader)));
	WSRP_SET(_jvmImageData->classSegmentTable, subAllocateMemory(sizeof(ImageTableHeader)));
	WSRP_SET(_jvmImageData->classPathEntryTable, subAllocateMemory(sizeof(ImageTableHeader)));

	if (_jvmImageData->classLoaderTable == 0
		|| _jvmImageData->classSegmentTable == 0
		|| _jvmImageData->classPathEntryTable == 0) {
		return false;
	}

	return true;
}

void * 
JVMImage::allocateTable(ImageTableHeader *table, uintptr_t tableSize)
{
	void *firstSlot = subAllocateMemory(tableSize * sizeof(UDATA));
	if (firstSlot == NULL) {
		return NULL;
	}

	table->tableSize = tableSize;
	table->currentSize = 0;
	WSRP_SET(table->tableHead, firstSlot);
	WSRP_SET(table->tableTail, firstSlot);
	
	return firstSlot;
}

void *
JVMImage::reallocateTable(ImageTableHeader *table, uintptr_t tableSize)
{
	UDATA *firstSlot = WSRP_GET(table->tableHead, UDATA*);
	firstSlot = (UDATA *)reallocateMemory((void *)firstSlot, tableSize * sizeof(UDATA));
	if (firstSlot == NULL) {
		return NULL;
	}

	table->tableSize = tableSize;
	WSRP_SET(table->tableHead, firstSlot);
	WSRP_SET(table->tableTail, firstSlot + table->currentSize - 1);

	return firstSlot;
}

void *
JVMImage::subAllocateMemory(uintptr_t byteAmount)
{
	Trc_VM_SubAllocateImageMemory_Entry(_jvmInstance, _jvmImageData, byteAmount);

	omrthread_monitor_enter(_jvmImageMonitor);

	void *memStart = _portLibrary->heap_allocate(_portLibrary, _heap, byteAmount);	
	// image memory is not large enough and needs to be reallocated
	if (memStart == NULL) {
		reallocateImageMemory(_jvmImageData->imageSize * 2 + byteAmount);
		memStart = _portLibrary->heap_allocate(_portLibrary, _heap, byteAmount);
	}

	omrthread_monitor_exit(_jvmImageMonitor);

	Trc_VM_SubAllocateImageMemory_Exit(memStart);

	return memStart;
}

void*
JVMImage::reallocateMemory(void *address, uintptr_t byteAmount)
{
	omrthread_monitor_enter(_jvmImageMonitor);

	void *memStart = _portLibrary->heap_reallocate(_portLibrary, _heap, address, byteAmount);
	// image memory is not large enough and needs to be reallocated
	if (memStart == NULL) {
		reallocateImageMemory(_jvmImageData->imageSize * 2 + byteAmount);
		memStart = _portLibrary->heap_reallocate(_portLibrary, _heap, address, byteAmount);
	}

	omrthread_monitor_exit(_jvmImageMonitor);

	return memStart;
}

void
JVMImage::freeSubAllocatedMemory(void *address)
{
	omrthread_monitor_enter(_jvmImageMonitor);

	_portLibrary->heap_free(_portLibrary, _heap, address);

	omrthread_monitor_exit(_jvmImageMonitor);
}

void
JVMImage::registerEntryInTable(ImageTableHeader *table, UDATA entry)
{
	Trc_VM_RegisterInTable_Entry(table, table->currentSize, *(WSRP_GET(table->tableTail, UDATA*)), entry);

	// table is not large enough and needs to be reallocated
	if (table->currentSize >= table->tableSize) {
		reallocateTable(table, table->tableSize * 2); //TODO: Introduce error handling for table reallocation
	}

	UDATA *tail = WSRP_GET(table->tableTail, UDATA*);

	// initial state of every table
	if (table->currentSize != 0) {
		tail += 1;
		WSRP_SET(table->tableTail, tail);
	}

	*(tail) = entry;
	table->currentSize += 1;

	Trc_VM_RegisterInTable_Exit(table, table->currentSize, *(WSRP_GET(table->tableTail, UDATA*)));
}

bool
JVMImage::readImageFromFile()
{
	Trc_VM_ReadImageFromFile_Entry(_heap, _dumpFileName);

	OMRPORT_ACCESS_FROM_OMRPORT(getPortLibrary());

	intptr_t fileDescriptor = omrfile_open(_dumpFileName, EsOpenRead, 0444);
	if (fileDescriptor == -1) {
		return false;
	}

	// Read image header then mmap the rest of the image (heap) into memory
	JVMImageHeader imageHeader;
	omrfile_read(fileDescriptor, &imageHeader, sizeof(JVMImageHeader));
	uint64_t fileSize = omrfile_flength(fileDescriptor);
	if (fileSize != sizeof(JVMImageHeader) + imageHeader.imageSize) {
		return false;
	}

	// Mmap our file into virtual memory
	JVMImageHeader *block = (JVMImageHeader*)mmap((void*)imageHeader.heapAddress, sizeof(JVMImageHeader) + imageHeader.imageSize, PROT_READ, MAP_PRIVATE, fileDescriptor, 0);
	_heap = (J9Heap*)(block + 1);

	omrfile_close(fileDescriptor);

	Trc_VM_ReadImageFromFile_Exit();

	return true;
}

bool
JVMImage::writeImageToFile()
{
	Trc_VM_WriteImageToFile_Entry(_heap, _dumpFileName);

	OMRPORT_ACCESS_FROM_OMRPORT(getPortLibrary());

	omrthread_monitor_enter(_jvmImageMonitor);

	intptr_t fileDescriptor = omrfile_open(_dumpFileName, EsOpenCreate | EsOpenWrite | EsOpenTruncate, 0666);
	if (fileDescriptor == -1) {
		return false;
	}

	// Write header followed by the heap
	JVMImageHeader imageHeader;
	imageHeader.imageSize = _currentImageSize;
	imageHeader.heapAddress = (uintptr_t)_heap;
	
	if (omrfile_write(fileDescriptor, (void *)&imageHeader, sizeof(JVMImageHeader)) != sizeof(JVMImageHeader)
		|| omrfile_write(fileDescriptor, (void *)_heap, _currentImageSize) != (intptr_t)_currentImageSize) {
		return false;
	}

	omrfile_close(fileDescriptor);

	omrthread_monitor_exit(_jvmImageMonitor);

	Trc_VM_WriteImageToFile_Exit();

	return true;
}

extern "C" UDATA
initializeJVMImage(J9JavaVM *vm)
{
	JVMImage *jvmImage = JVMImage::createInstance(vm);
	if (jvmImage == NULL) {
		goto _error;
	}

	if (IS_COLD_RUN(vm)
		&& jvmImage->setupColdRun() == IMAGE_ERROR) {
		goto _error;
	}

	if(IS_WARM_RUN(vm)
		&& jvmImage->setupWarmRun() == IMAGE_ERROR) {
		goto _error;
	}

	return 1;

_error:
	shutdownJVMImage(vm);
	return 0;
}

extern "C" void
registerClassLoader(J9ClassLoader *classLoader)
{
	JVMImage *jvmImage = JVMImage::getInstance();

	Assert_VM_notNull(jvmImage);

	jvmImage->registerEntryInTable(jvmImage->getClassLoaderTable(), (UDATA)classLoader);
}

extern "C" void
registerClassSegment(J9Class *clazz)
{
	JVMImage *jvmImage = JVMImage::getInstance();

	Assert_VM_notNull(jvmImage);

	jvmImage->registerEntryInTable(jvmImage->getClassSegmentTable(), (UDATA)clazz);
}

extern "C" void
registerCPEntry(J9ClassPathEntry *cpEntry)
{
	JVMImage *jvmImage = JVMImage::getInstance();

	Assert_VM_notNull(jvmImage);
	
	jvmImage->registerEntryInTable(jvmImage->getClassPathEntryTable(), (UDATA)cpEntry);
}

extern "C" OMRPortLibrary *
getImagePortLibrary()
{
	JVMImage *jvmImage = JVMImage::getInstance();

	if (jvmImage == NULL) {
		return NULL;
	}

	return jvmImage->getPortLibrary();
}

extern "C" void
shutdownJVMImage(J9JavaVM *vm)
{
	JVMImage *jvmImage = JVMImage::getInstance();

	if (jvmImage != NULL) {
		PORT_ACCESS_FROM_JAVAVM(vm);

		jvmImage->destroyMonitor();
		jvmImage->~JVMImage();
		j9mem_free_memory(jvmImage);
	}
}

extern "C" void *
image_mem_allocate_memory(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount, const char *callSite, uint32_t category)
{
	void *pointer = NULL;
	
	JVMImage *jvmImage = JVMImage::getInstance();
	pointer = jvmImage->subAllocateMemory(byteAmount);
	return pointer;
}

extern "C" void *
mem_allocate_memory(uintptr_t byteAmount)
{
    JVMImage *jvmImage = JVMImage::getInstance();
    return image_mem_allocate_memory(jvmImage->getPortLibrary(), byteAmount, J9_GET_CALLSITE(), J9MEM_CATEGORY_CLASSES);
}

extern "C" void
image_mem_free_memory(struct OMRPortLibrary *portLibrary, void *memoryPointer)
{
	JVMImage *jvmImage = JVMImage::getInstance();
	jvmImage->freeSubAllocatedMemory(memoryPointer);
}

extern "C" void
mem_free_memory(void *memoryPointer)
{
    JVMImage *jvmImage = JVMImage::getInstance();
    image_mem_free_memory(jvmImage->getPortLibrary(), memoryPointer);
}
