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

#include "vm_api.h"
#include "vm_internal.h"

#include "ArrayCopyHelpers.hpp"
#include "VMAccess.hpp"

extern "C" {

void *
memcpyFromHeapArray(J9VMThread *currentThread, j9object_t arrayObject, jboolean ensureMem32)
{
	UDATA logElementSize = ((J9ROMArrayClass *)J9OBJECT_CLAZZ(currentThread, arrayObject)->romClass)->arrayShape & 0x0000FFFF;
	UDATA byteCount = (UDATA)J9INDEXABLEOBJECT_SIZE(currentThread, arrayObject) << logElementSize;
	void *elems = NULL;

	if (ensureMem32) {
		elems = jniArrayAllocateMemory32FromThread(currentThread, ROUND_UP_TO_POWEROF2(byteCount, sizeof(UDATA)));
	} else {
		elems = jniArrayAllocateMemoryFromThread(currentThread, ROUND_UP_TO_POWEROF2(byteCount, sizeof(UDATA)));
	}
	if (NULL == elems) {
		gpCheckSetNativeOutOfMemoryError(currentThread, 0, 0);
	} else {
		JAVA_OFFLOAD_SWITCH_ON_WITH_REASON_IF_LIMIT_EXCEEDED(currentThread, J9_JNI_OFFLOAD_SWITCH_GET_ARRAY_ELEMENTS, byteCount);
		/* No guarantee of native memory alignment, so copy byte-wise. */
		VM_ArrayCopyHelpers::memcpyFromArray(currentThread, arrayObject, (UDATA)0, (UDATA)0, byteCount, elems);
		JAVA_OFFLOAD_SWITCH_OFF_WITH_REASON_IF_LIMIT_EXCEEDED(currentThread, J9_JNI_OFFLOAD_SWITCH_GET_ARRAY_ELEMENTS, byteCount);
	}

	return elems;
}

void
memcpyToHeapArray(J9VMThread *currentThread, j9object_t arrayObject, void *elems, jint mode, jboolean ensureMem32)
{
	/* Abort means do not copy the buffer, but do free it. */
	if (JNI_ABORT != mode) {
		UDATA logElementSize = ((J9ROMArrayClass *)J9OBJECT_CLAZZ(currentThread, arrayObject)->romClass)->arrayShape  & 0x0000FFFF;
		UDATA byteCount = (UDATA)J9INDEXABLEOBJECT_SIZE(currentThread, arrayObject) << logElementSize;
		JAVA_OFFLOAD_SWITCH_ON_WITH_REASON_IF_LIMIT_EXCEEDED(currentThread, J9_JNI_OFFLOAD_SWITCH_RELEASE_ARRAY_ELEMENTS, byteCount);
		/* No guarantee of native memory alignment, so copy byte-wise. */
		VM_ArrayCopyHelpers::memcpyToArray(currentThread, arrayObject, (UDATA)0, (UDATA)0, byteCount, elems);
		JAVA_OFFLOAD_SWITCH_OFF_WITH_REASON_IF_LIMIT_EXCEEDED(currentThread, J9_JNI_OFFLOAD_SWITCH_RELEASE_ARRAY_ELEMENTS, byteCount);
	}
	/* Commit means copy the data but do not free the buffer - all other modes free the buffer. */
	if (JNI_COMMIT != mode) {
		if (ensureMem32) {
			jniArrayFreeMemory32FromThread(currentThread, elems);
		} else {
			jniArrayFreeMemoryFromThread(currentThread, elems);
		}
	}
}

} /* extern "C" */
