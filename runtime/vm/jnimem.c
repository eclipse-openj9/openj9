/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#include "j9.h"
#include "j9port.h"
#include "ut_j9vm.h"
#include "vm_internal.h"


void* jniArrayAllocateMemoryFromThread(J9VMThread* vmThread, UDATA sizeInBytes) {
#ifdef J9VM_GC_JNI_ARRAY_CACHE
	void* cache = vmThread->jniArrayCache;
	UDATA actualSize = sizeInBytes + sizeof(U_64);
	if (cache != NULL && *(UDATA*)cache >= actualSize ) {
		Trc_VM_jniArrayCache_hit(vmThread, actualSize);
		vmThread->jniArrayCache = NULL;
	} else {
		PORT_ACCESS_FROM_VMC(vmThread);
		if (cache == NULL) {
			Trc_VM_jniArrayCache_missUsed(vmThread, actualSize);
		} else {
			Trc_VM_jniArrayCache_missBigger(vmThread, actualSize);
		}
		cache = j9mem_allocate_memory(actualSize, J9MEM_CATEGORY_JNI);
		if (cache == NULL) return NULL;
		*(UDATA*)cache = actualSize;
	}
	return (U_64*)cache + 1;	/* make sure that the memory returned is 8-aligned (or at least as 8-aligned as the allocate function returned) */
#else
	PORT_ACCESS_FROM_VMC(vmThread);
	return j9mem_allocate_memory(sizeInBytes, J9MEM_CATEGORY_JNI);
#endif
}


void jniArrayFreeMemoryFromThread(J9VMThread* vmThread, void* location) {
	PORT_ACCESS_FROM_VMC(vmThread);

#ifdef J9VM_GC_JNI_ARRAY_CACHE
	UDATA* actualLocation = (UDATA*)((U_64*)location - 1);
	void* memToFree = actualLocation;
	J9JavaVM* vm = vmThread->javaVM;
	IDATA maxSize = vm->jniArrayCacheMaxSize;

	if (maxSize == -1 || *actualLocation < (UDATA)maxSize ) {
		UDATA* cache = vmThread->jniArrayCache;
		if (cache == NULL) {
			vmThread->jniArrayCache = actualLocation;
			return;
		} else if ( *cache < *actualLocation ) {
			vmThread->jniArrayCache = actualLocation;
			memToFree = cache;
		}
	}

	j9mem_free_memory(memToFree);

#else
	j9mem_free_memory(location);
#endif
}


#if (defined(J9VM_GC_JNI_ARRAY_CACHE)) 
void cleanupVMThreadJniArrayCache(J9VMThread *vmThread) {
	if (vmThread->jniArrayCache) {
		PORT_ACCESS_FROM_VMC(vmThread);
		j9mem_free_memory(vmThread->jniArrayCache);
		vmThread->jniArrayCache = NULL;
	}
}
#endif /* J9VM_GC_JNI_ARRAY_CACHE */



