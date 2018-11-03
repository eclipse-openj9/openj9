/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

static void patchTable (jniNativeInterface * to, const jniNativeInterface * from);


jvmtiError JNICALL
jvmtiSetJNIFunctionTable(jvmtiEnv* env,
	const jniNativeInterface* function_table)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
	jvmtiError rc;

	Trc_JVMTI_jvmtiSetJNIFunctionTable_Entry(env);

	ENSURE_PHASE_START_OR_LIVE(env);

	ENSURE_NON_NULL(function_table);

	/* Get the JVMTI mutex to ensure this operation is atomic with GetJNIFunctionTable */

	omrthread_monitor_enter(jvmtiData->mutex);

	/* See if the table has been copied already */

	if (jvmtiData->copiedJNITable == NULL) {
		PORT_ACCESS_FROM_JAVAVM(vm);
		J9VMThread * vmThread;

		/* Not copied yet: copy the table into newly-allocated memory (this is strictly internal, so use the port library, not Allocate) */

		jvmtiData->copiedJNITable = j9mem_allocate_memory(sizeof(jniNativeInterface), J9MEM_CATEGORY_JVMTI);
		if (jvmtiData->copiedJNITable == NULL) {
			omrthread_monitor_exit(jvmtiData->mutex);
			JVMTI_ERROR(JVMTI_ERROR_OUT_OF_MEMORY);
		}

		patchTable(jvmtiData->copiedJNITable, function_table);

		/* Set the global table in the VM (for new threads) and fix all existing threads - need thread list mutex to walk threads */

		omrthread_monitor_enter(vm->vmThreadListMutex);
		vm->jniFunctionTable = jvmtiData->copiedJNITable;
		vmThread = vm->mainThread;
		do {
			vmThread->functions = (struct JNINativeInterface_*) jvmtiData->copiedJNITable;
		} while ((vmThread = vmThread->linkNext) != vm->mainThread);
		omrthread_monitor_exit(vm->vmThreadListMutex);
	} else {

		/* Already copied: must re-use the existing copy to avoid freeing something cached by the C compiler in a local in a JNI native.
		   Also, must copy the elements a pointer at a time to avoid corrupting the function pointers (memcpy may copy byte-wise).
		*/

		patchTable(jvmtiData->copiedJNITable, function_table);
	}

	omrthread_monitor_exit(jvmtiData->mutex);
	rc = JVMTI_ERROR_NONE;

done:
	TRACE_JVMTI_RETURN(jvmtiSetJNIFunctionTable);
}


jvmtiError JNICALL
jvmtiGetJNIFunctionTable(jvmtiEnv* env,
	jniNativeInterface** function_table)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_VM(vm);
	jvmtiError rc = JVMTI_ERROR_NONE;
	PORT_ACCESS_FROM_JAVAVM(vm);
	jniNativeInterface *rv_function_table = NULL;

	Trc_JVMTI_jvmtiGetJNIFunctionTable_Entry(env);

	ENSURE_PHASE_START_OR_LIVE(env);

	ENSURE_NON_NULL(function_table);

	/* Copy the table into newly-allocated memory */

	rv_function_table = j9mem_allocate_memory(sizeof(jniNativeInterface), J9MEM_CATEGORY_JVMTI_ALLOCATE);
	if (rv_function_table == NULL) {
		rc = JVMTI_ERROR_OUT_OF_MEMORY;
	} else {
		/* Get the JVMTI mutex to ensure this operation is atomic with SetJNIFunctionTable */

		omrthread_monitor_enter(jvmtiData->mutex);
		memcpy(rv_function_table, vm->jniFunctionTable, sizeof(jniNativeInterface));
		omrthread_monitor_exit(jvmtiData->mutex);
	}

done:
	if (NULL != function_table) {
		*function_table = rv_function_table;
	}
	TRACE_JVMTI_RETURN(jvmtiGetJNIFunctionTable);
}



static void patchTable(jniNativeInterface * to, const jniNativeInterface * from)
{
	void ** source = (void **) from;
	void ** dest = (void **) to;

	do {
		*dest++ = J9_COMPATIBLE_FUNCTION_POINTER( *source++ );
	} while (source != (void **) (from + 1));
}



