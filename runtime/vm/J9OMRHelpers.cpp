/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include "omr.h"
#include "j9protos.h"
#include "OMR_Runtime.hpp"
#include "OMR_RuntimeConfiguration.hpp"
#include "OMR_VM.hpp"
#include "OMR_VMConfiguration.hpp"
#include "OMR_VMThread.hpp"

extern "C" {

/* Generic rounding macro - result is a UDATA */
#define ROUND_TO(granularity, number) (((UDATA)(number) + (granularity) - 1) & ~((UDATA)(granularity) - 1))

jint
initOMRVMThread(J9JavaVM *vm, J9VMThread *vmThread)
{
	jint rc = JNI_OK;
	/* vm contains segregatedAllocationCacheSize, which is needed to determine location of OMR_VMThread */
	OMR_VMThread *omrVMThread = (OMR_VMThread*)(((UDATA)vmThread) + J9_VMTHREAD_SEGREGATED_ALLOCATION_CACHE_OFFSET + vm->segregatedAllocationCacheSize);
	if (OMR_ERROR_NONE != omr_vmthread_init(omrVMThread)) {
		rc = JNI_ERR;
	}
	return rc;
}

void
destroyOMRVMThread(J9JavaVM *vm, J9VMThread *vmThread)
{
	/* vm contains segregatedAllocationCacheSize, which is needed to determine location of OMR_VMThread */
	/* Because the vmThread may be detached, don't rely on vmThread->omrVMThread */
	OMR_VMThread *omrVMThread = (OMR_VMThread*)(((UDATA)vmThread) + J9_VMTHREAD_SEGREGATED_ALLOCATION_CACHE_OFFSET + vm->segregatedAllocationCacheSize);
	omr_vmthread_destroy(omrVMThread);
}

jint
attachVMThreadToOMR(J9JavaVM *vm, J9VMThread *vmThread, omrthread_t osThread)
{
	jint rc = JNI_ERR;
	OMR_VM *omrVM = vm->omrVM;
	OMR_VMThread *omrVMThread = (OMR_VMThread*)(((UDATA)vmThread) + J9_VMTHREAD_SEGREGATED_ALLOCATION_CACHE_OFFSET + vm->segregatedAllocationCacheSize);
	omrVMThread->_vm = omrVM;
	omrVMThread->_language_vmthread = vmThread;
	omrVMThread->_os_thread = osThread;
#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS)
	omrVMThread->_compressObjectReferences = J9VMTHREAD_COMPRESS_OBJECT_REFERENCES(vmThread);
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) */
	if (OMR_ERROR_NONE == omr_attach_vmthread_to_vm(omrVMThread)) {
		vmThread->omrVMThread = omrVMThread;
		rc = JNI_OK;
	}
	return rc;
}

void
detachVMThreadFromOMR(J9JavaVM *vm, J9VMThread *vmThread)
{
	OMR_VMThread *omrVMThread = (OMR_VMThread*)vmThread->omrVMThread;
	if (NULL != omrVMThread) {
		omr_detach_vmthread_from_vm(omrVMThread);
		vmThread->omrVMThread = NULL;
	}
}

/* The J9JavaVM structure is followed (in the same allocation) by one OMR_Runtime and
 * one OMR_VM, each of which is at least 8-byte aligned wrt the start of the J9JavaVM.
 */

J9JavaVM *
allocateJavaVMWithOMR(J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA omrRuntimeOffset = ROUND_TO(8, sizeof(J9JavaVM));
	UDATA omrVMOffset = ROUND_TO(8, omrRuntimeOffset + sizeof(OMR_Runtime));
	UDATA vmAllocationSize = omrVMOffset + sizeof(OMR_VM);
	J9JavaVM *vm = (J9JavaVM *)j9mem_allocate_memory(vmAllocationSize, OMRMEM_CATEGORY_VM);
	if (vm != NULL) {
		memset(vm, 0, vmAllocationSize);
	}
	return vm;
}

jint
attachVMToOMR(J9JavaVM *vm)
{
	jint rc = JNI_ERR;
	UDATA omrRuntimeOffset = ROUND_TO(8, sizeof(J9JavaVM));
	UDATA omrVMOffset = ROUND_TO(8, omrRuntimeOffset + sizeof(OMR_Runtime));
	OMR_Runtime *omrRuntime = (OMR_Runtime*)(((UDATA)vm) + omrRuntimeOffset);
	omrRuntime->_configuration._maximum_vm_count = 1;
	omrRuntime->_portLibrary = OMRPORT_FROM_J9PORT(vm->portLibrary);
	if (OMR_ERROR_NONE == omr_initialize_runtime(omrRuntime)) {
		OMR_VM *omrVM = (OMR_VM*)(((UDATA)vm) + omrVMOffset);
		omrVM->_configuration._maximum_thread_count = 0;
		omrVM->_language_vm = (void*)vm;
		omrVM->_runtime = omrRuntime;
#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS)
		omrVM->_compressObjectReferences = J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm);
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) */
		if (OMR_ERROR_NONE == omr_attach_vm_to_runtime(omrVM)) {
			vm->omrRuntime = omrRuntime;
			vm->omrVM = omrVM;
			rc = JNI_OK;
		} else {
			omr_destroy_runtime(omrRuntime);
		}
	}

	return rc;
}

void
detachVMFromOMR(J9JavaVM *vm)
{
	OMR_Runtime *omrRuntime = vm->omrRuntime;
	if (NULL != omrRuntime) {
		OMR_VM *omrVM = vm->omrVM;
		if (NULL != omrVM) {
			omr_detach_vm_from_runtime(omrVM);
			vm->omrVM = NULL;
		}
		omr_destroy_runtime(omrRuntime);
		vm->omrRuntime = NULL;
	}
}

}
