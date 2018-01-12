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

#include "jvmtiHelpers.h"
#include "jvmti_internal.h"
#include "j9port.h"

jvmtiError JNICALL
jvmtiAllocate(jvmtiEnv* env,
	jlong size,
	unsigned char** mem_ptr)
{
	void * allocatedMemory = NULL;
	jvmtiError rc;

	Trc_JVMTI_jvmtiAllocate_Entry(env, mem_ptr);

	ENSURE_NON_NEGATIVE(size);
	ENSURE_NON_NULL(mem_ptr);

#ifndef J9VM_ENV_DATA64
	if (size > (jlong)0xFFFFFFFF) {
		JVMTI_ERROR(JVMTI_ERROR_OUT_OF_MEMORY);
	}
#endif

	if (size != 0) {
		PORT_ACCESS_FROM_JVMTI(env);

		allocatedMemory = j9mem_allocate_memory((UDATA) size, J9MEM_CATEGORY_JVMTI_ALLOCATE);
		if (allocatedMemory == NULL) {
			JVMTI_ERROR(JVMTI_ERROR_OUT_OF_MEMORY);
		}
	}

	*mem_ptr = (unsigned char *) allocatedMemory;
	rc = JVMTI_ERROR_NONE;

done:
	Trc_JVMTI_jvmtiAllocate_Exit(rc, allocatedMemory);
	return rc;
}


jvmtiError JNICALL
jvmtiDeallocate(jvmtiEnv* env,
	unsigned char* mem)
{
	jvmtiError rc = JVMTI_ERROR_NONE;

	Trc_JVMTI_jvmtiDeallocate_Entry(env, mem);

	if (mem != NULL) {
		PORT_ACCESS_FROM_JVMTI(env);

		j9mem_free_memory(mem);
	}

	TRACE_JVMTI_RETURN(jvmtiDeallocate);
}


