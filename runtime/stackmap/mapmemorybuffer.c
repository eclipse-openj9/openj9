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

#include <stdlib.h>
#include <stdio.h>
#include "j9.h"
#include "thread_api.h"
#include "ut_map.h"
#include "vm_api.h"

/* 
 * Check the existence of the vm to make it transparent to out-of process usage.
 * This will return NULL if either the vm is NULL, or the buffers referenced in the vm are NULL.
 * There are routines for both getting and releasing the intermediate data buffer and the results buffer.
 */


/*
 * 
 * Acquire the global vm buffer for a stack/local mapping function
 * 
 */

UDATA *
j9mapmemory_GetBuffer(void * userData) 
{
#ifndef J9VM_OUT_OF_PROCESS
	J9JavaVM* vm = (J9JavaVM *) userData;
	
	if (vm && vm->mapMemoryBuffer) {
		JavaVM* jniVM = (JavaVM*)vm;
	    J9ThreadEnv* threadEnv;

		/* Get the thread functions */
		(*jniVM)->GetEnv(jniVM, (void**)&threadEnv, J9THREAD_VERSION_1_1);

		threadEnv->monitor_enter(vm->mapMemoryBufferMutex);
		Trc_Map_j9mapmemory_GetBuffer();
		return (UDATA *) vm->mapMemoryBuffer;
	}
#endif
	return NULL;
}


/*
 * 
 * Release the global vm buffer for a stack/local mapping function
 * 
 */

void
j9mapmemory_ReleaseBuffer(void * userData) 
{
#ifndef J9VM_OUT_OF_PROCESS
	J9JavaVM* vm = (J9JavaVM *) userData;

	if (vm && vm->mapMemoryBuffer) {
		JavaVM* jniVM = (JavaVM*)vm;
	    J9ThreadEnv* threadEnv;

		/* Get the thread functions */
		(*jniVM)->GetEnv(jniVM, (void**)&threadEnv, J9THREAD_VERSION_1_1);

		Trc_Map_j9mapmemory_ReleaseBuffer();
		threadEnv->monitor_exit(vm->mapMemoryBufferMutex);
	}
#endif
}

/*
 * 
 * Acquire the global vm results buffer for a stack/local mapping function
 * 
 */

U_32 *
j9mapmemory_GetResultsBuffer(void * userData) 
{
#ifndef J9VM_OUT_OF_PROCESS
	J9JavaVM* vm = (J9JavaVM *) userData;

	if (vm && vm->mapMemoryResultsBuffer) {
		JavaVM* jniVM = (JavaVM*)vm;
	    J9ThreadEnv* threadEnv;
		(*jniVM)->GetEnv(jniVM, (void**)&threadEnv, J9THREAD_VERSION_1_1);

		threadEnv->monitor_enter(vm->mapMemoryBufferMutex);
		Trc_Map_j9mapmemory_GetResultsBuffer();
		return (U_32 *) vm->mapMemoryResultsBuffer;
	}
#endif
	return NULL;
}


/*
 * 
 * Release the global vm results buffer for a stack/local mapping function
 * 
 */

void
j9mapmemory_ReleaseResultsBuffer(void * userData) 
{
#ifndef J9VM_OUT_OF_PROCESS
	J9JavaVM* vm = (J9JavaVM *) userData;

	if (vm && vm->mapMemoryResultsBuffer) {
		JavaVM* jniVM = (JavaVM*)vm;
	    J9ThreadEnv* threadEnv;

		Trc_Map_j9mapmemory_ReleaseResultsBuffer();

		/* Get the thread functions */
		(*jniVM)->GetEnv(jniVM, (void**)&threadEnv, J9THREAD_VERSION_1_1);
		threadEnv->monitor_exit(vm->mapMemoryBufferMutex);
	}
#endif
}

