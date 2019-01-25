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

#include "jvmtiHelpers.h"
#include "jvmti_internal.h"
#include "j2sever.h"

typedef struct {
	const char *errorName;
	jvmtiError errorValue;
} J9JvmtiErrorMapping;

static const J9JvmtiErrorMapping errorMap[] = {
	{ "JVMTI_ERROR_NONE" , 0 },
	{ "JVMTI_ERROR_INVALID_THREAD" , 10 },
	{ "JVMTI_ERROR_INVALID_THREAD_GROUP" , 11 },
	{ "JVMTI_ERROR_INVALID_PRIORITY" , 12 },
	{ "JVMTI_ERROR_THREAD_NOT_SUSPENDED" , 13 },
	{ "JVMTI_ERROR_THREAD_SUSPENDED" , 14 },
	{ "JVMTI_ERROR_THREAD_NOT_ALIVE" , 15 },
	{ "JVMTI_ERROR_INVALID_OBJECT" , 20 },
	{ "JVMTI_ERROR_INVALID_CLASS" , 21 },
	{ "JVMTI_ERROR_CLASS_NOT_PREPARED" , 22 },
	{ "JVMTI_ERROR_INVALID_METHODID" , 23 },
	{ "JVMTI_ERROR_INVALID_LOCATION" , 24 },
	{ "JVMTI_ERROR_INVALID_FIELDID" , 25 },
	{ "JVMTI_ERROR_INVALID_MODULE" , 26 },
	{ "JVMTI_ERROR_NO_MORE_FRAMES" , 31 },
	{ "JVMTI_ERROR_OPAQUE_FRAME" , 32 },
	{ "JVMTI_ERROR_TYPE_MISMATCH" , 34 },
	{ "JVMTI_ERROR_INVALID_SLOT" , 35 },
	{ "JVMTI_ERROR_DUPLICATE" , 40 },
	{ "JVMTI_ERROR_NOT_FOUND" , 41 },
	{ "JVMTI_ERROR_INVALID_MONITOR" , 50 },
	{ "JVMTI_ERROR_NOT_MONITOR_OWNER" , 51 },
	{ "JVMTI_ERROR_INTERRUPT" , 52 },
	{ "JVMTI_ERROR_INVALID_CLASS_FORMAT" , 60 },
	{ "JVMTI_ERROR_CIRCULAR_CLASS_DEFINITION" , 61 },
	{ "JVMTI_ERROR_FAILS_VERIFICATION" , 62 },
	{ "JVMTI_ERROR_UNSUPPORTED_REDEFINITION_METHOD_ADDED" , 63 },
	{ "JVMTI_ERROR_UNSUPPORTED_REDEFINITION_SCHEMA_CHANGED" , 64 },
	{ "JVMTI_ERROR_INVALID_TYPESTATE" , 65 },
	{ "JVMTI_ERROR_UNSUPPORTED_REDEFINITION_HIERARCHY_CHANGED" , 66 },
	{ "JVMTI_ERROR_UNSUPPORTED_REDEFINITION_METHOD_DELETED" , 67 },
	{ "JVMTI_ERROR_UNSUPPORTED_VERSION" , 68 },
	{ "JVMTI_ERROR_NAMES_DONT_MATCH" , 69 },
	{ "JVMTI_ERROR_UNSUPPORTED_REDEFINITION_CLASS_MODIFIERS_CHANGED" , 70 },
	{ "JVMTI_ERROR_UNSUPPORTED_REDEFINITION_METHOD_MODIFIERS_CHANGED" , 71 },
#if defined(J9VM_OPT_VALHALLA_NESTMATES)
	{ "JVMTI_ERROR_UNSUPPORTED_REDEFINITION_CLASS_ATTRIBUTE_CHANGED" , 72 },
#endif /* defined(J9VM_OPT_VALHALLA_NESTMATES) */
	{ "JVMTI_ERROR_UNMODIFIABLE_CLASS" , 79 },
	{ "JVMTI_ERROR_UNMODIFIABLE_MODULE" , 80 },
	{ "JVMTI_ERROR_NOT_AVAILABLE" , 98 },
	{ "JVMTI_ERROR_MUST_POSSESS_CAPABILITY" , 99 },
	{ "JVMTI_ERROR_NULL_POINTER" , 100 },
	{ "JVMTI_ERROR_ABSENT_INFORMATION" , 101 },
	{ "JVMTI_ERROR_INVALID_EVENT_TYPE" , 102 },
	{ "JVMTI_ERROR_ILLEGAL_ARGUMENT" , 103 },
	{ "JVMTI_ERROR_NATIVE_METHOD" , 104 },
	{ "JVMTI_ERROR_CLASS_LOADER_UNSUPPORTED" , 106 },
	{ "JVMTI_ERROR_OUT_OF_MEMORY" , 110 },
	{ "JVMTI_ERROR_ACCESS_DENIED" , 111 },
	{ "JVMTI_ERROR_WRONG_PHASE" , 112 },
	{ "JVMTI_ERROR_INTERNAL" , 113 },
	{ "JVMTI_ERROR_UNATTACHED_THREAD" , 115 },
	{ "JVMTI_ERROR_INVALID_ENVIRONMENT" , 116 },
	{ NULL, 0 }
};


jvmtiError JNICALL
jvmtiGetPhase(jvmtiEnv* env,
	jvmtiPhase* phase_ptr)
{
	jvmtiError rc;
	jvmtiPhase rv_phase = JVMTI_PHASE_DEAD;

	Trc_JVMTI_jvmtiGetPhase_Entry(env);

	ENSURE_NON_NULL(phase_ptr);

	rv_phase = (jvmtiPhase) J9JVMTI_DATA_FROM_ENV(env)->phase;
	rc = JVMTI_ERROR_NONE;

done:
	if (NULL != phase_ptr) {
		*phase_ptr = rv_phase;
	}
	TRACE_JVMTI_RETURN(jvmtiGetPhase);
}


jvmtiError JNICALL
jvmtiDisposeEnvironment(jvmtiEnv* env)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_ENV(env);

	Trc_JVMTI_jvmtiDisposeEnvironment_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);
		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);
		omrthread_monitor_enter(jvmtiData->mutex);

		disposeEnvironment((J9JVMTIEnv *) env, FALSE);

		omrthread_monitor_exit(jvmtiData->mutex);
		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiDisposeEnvironment);
}


jvmtiError JNICALL
jvmtiSetEnvironmentLocalStorage(jvmtiEnv* env,
	const void* data)
{
	jvmtiError rc = JVMTI_ERROR_NONE;

	Trc_JVMTI_jvmtiSetEnvironmentLocalStorage_Entry(env);

	((J9JVMTIEnv *) env)->environmentLocalStorage = (void *) data;

	TRACE_JVMTI_RETURN(jvmtiSetEnvironmentLocalStorage);
}


jvmtiError JNICALL
jvmtiGetEnvironmentLocalStorage(jvmtiEnv* env,
	void** data_ptr)
{
	jvmtiError rc;
	void *rv_data = NULL;

	Trc_JVMTI_jvmtiGetEnvironmentLocalStorage_Entry(env);

	ENSURE_NON_NULL(data_ptr);

	rv_data = ((J9JVMTIEnv *) env)->environmentLocalStorage;
	rc = JVMTI_ERROR_NONE;

done:
	if (NULL != data_ptr) {
		*data_ptr = rv_data;
	}
	TRACE_JVMTI_RETURN(jvmtiGetEnvironmentLocalStorage);
}


jvmtiError JNICALL
jvmtiGetVersionNumber(jvmtiEnv* env,
	jint* version_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	jint rv_version = JVMTI_1_2_3_SPEC_VERSION;

	Trc_JVMTI_jvmtiGetVersionNumber_Entry(env);

	ENSURE_NON_NULL(version_ptr);

	if (J2SE_VERSION(vm) >= J2SE_V11) {
		rv_version = JVMTI_VERSION_11;
	}

	rc = JVMTI_ERROR_NONE;

done:
	if (NULL != version_ptr) {
		*version_ptr = rv_version;
	}
	TRACE_JVMTI_RETURN(jvmtiGetVersionNumber);
}


jvmtiError JNICALL
jvmtiGetErrorName(jvmtiEnv* env,
	jvmtiError error,
	char** name_ptr)
{
	const J9JvmtiErrorMapping *mapping = NULL;
	jvmtiError rc = JVMTI_ERROR_ILLEGAL_ARGUMENT;
	PORT_ACCESS_FROM_JVMTI(env);
	char *rv_name = NULL;

	Trc_JVMTI_jvmtiGetErrorName_Entry(env);

	ENSURE_NON_NULL(name_ptr);

	mapping = errorMap;
	while (mapping->errorName != NULL) {
		if (mapping->errorValue == error) {
			rv_name = j9mem_allocate_memory(strlen(mapping->errorName) + 1, J9MEM_CATEGORY_JVMTI_ALLOCATE);
			if (rv_name == NULL) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			} else {
				strcpy(rv_name, mapping->errorName);
				rc = JVMTI_ERROR_NONE;
			}
			break;
		}
		++mapping;
	}
done:

	if (NULL != name_ptr) {
		*name_ptr = rv_name;
	}
	TRACE_JVMTI_RETURN(jvmtiGetErrorName);
}


jvmtiError JNICALL
jvmtiSetVerboseFlag(jvmtiEnv* env,
	jvmtiVerboseFlag flag,
	jboolean value)
{
	J9JVMTIData * jvmtiData = J9JVMTI_DATA_FROM_ENV(env);
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VerboseSettings verboseOptions;

	Trc_JVMTI_jvmtiSetVerboseFlag_Entry(env);

	omrthread_monitor_enter(jvmtiData->mutex);

	memset(&verboseOptions, 0, sizeof(J9VerboseSettings));
	switch (flag) {
		case JVMTI_VERBOSE_GC:
			verboseOptions.gc = value? VERBOSE_SETTINGS_SET: VERBOSE_SETTINGS_CLEAR;
			break;

		case JVMTI_VERBOSE_CLASS:
			verboseOptions.vclass = value? VERBOSE_SETTINGS_SET: VERBOSE_SETTINGS_CLEAR;
			break;

		case JVMTI_VERBOSE_OTHER:
		case JVMTI_VERBOSE_JNI:
			/* Neither of these are supported yet */
			break;

		default:
			rc = JVMTI_ERROR_ILLEGAL_ARGUMENT;
			break;
	}
	if (vm->setVerboseState != NULL) {
		vm->setVerboseState(vm, &verboseOptions, NULL);
	}
	omrthread_monitor_exit(jvmtiData->mutex);

	TRACE_JVMTI_RETURN(jvmtiSetVerboseFlag);
}


jvmtiError JNICALL
jvmtiGetJLocationFormat(jvmtiEnv* env,
	jvmtiJlocationFormat* format_ptr)
{
	jvmtiError rc;
	jint rv_format = JVMTI_JLOCATION_JVMBCI;

	Trc_JVMTI_jvmtiGetJLocationFormat_Entry(env);

	ENSURE_NON_NULL(format_ptr);

	rc = JVMTI_ERROR_NONE;

done:
	if (NULL != format_ptr) {
		*format_ptr = rv_format;
	}
	TRACE_JVMTI_RETURN(jvmtiGetJLocationFormat);
}



