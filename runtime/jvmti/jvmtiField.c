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

jvmtiError JNICALL
jvmtiGetFieldName(jvmtiEnv* env,
	jclass klass,
	jfieldID field,
	char** name_ptr,
	char** signature_ptr,
	char** generic_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	PORT_ACCESS_FROM_JAVAVM(vm);
	char *rv_name = NULL;
	char *rv_signature = NULL;
	char *rv_generic = NULL;

	Trc_JVMTI_jvmtiGetFieldName_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9ROMFieldShape * romFieldShape;
		char * name = NULL;
		char * signature = NULL;
		char * generic = NULL;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_JFIELDID_NON_NULL(field);

		romFieldShape = ((J9JNIFieldID *) field)->field;

		if (name_ptr != NULL) {
			J9UTF8 * utf = J9ROMFIELDSHAPE_NAME(romFieldShape);
			UDATA length = J9UTF8_LENGTH(utf);

			name = j9mem_allocate_memory(length + 1, J9MEM_CATEGORY_JVMTI_ALLOCATE);
			if (name == NULL) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
				goto done;
			}
			memcpy(name, J9UTF8_DATA(utf), length);
			name[length] = '\0';
			rv_name = name;
		}

		if (signature_ptr != NULL) {
			J9UTF8 * utf = J9ROMFIELDSHAPE_SIGNATURE(romFieldShape);
			UDATA length = J9UTF8_LENGTH(utf);

			signature = j9mem_allocate_memory(length + 1, J9MEM_CATEGORY_JVMTI_ALLOCATE);
			if (signature == NULL) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
				goto done;
			}
			memcpy(signature, J9UTF8_DATA(utf), length);
			signature[length] = '\0';
			rv_signature = signature;
		}

		if (generic_ptr != NULL) {
			J9UTF8 * utf = romFieldGenericSignature(romFieldShape);

			if (utf == NULL) {
				generic = NULL;
			} else {
				UDATA length = J9UTF8_LENGTH(utf);

				generic = j9mem_allocate_memory(length + 1, J9MEM_CATEGORY_JVMTI_ALLOCATE);
				if (generic == NULL) {
					rc = JVMTI_ERROR_OUT_OF_MEMORY;
					goto done;
				}
				memcpy(generic, J9UTF8_DATA(utf), length);
				generic[length] = '\0';
			}

			rv_generic = generic;
		}	

done:
		if (rc != JVMTI_ERROR_NONE) {
			j9mem_free_memory(name);
			j9mem_free_memory(signature);
			j9mem_free_memory(generic);
		}
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != name_ptr) {
		*name_ptr = rv_name;
	}
	if (NULL != signature_ptr) {
		*signature_ptr = rv_signature;
	}
	if (NULL != generic_ptr) {
		*generic_ptr = rv_generic;
	}
	TRACE_JVMTI_RETURN(jvmtiGetFieldName);
}


jvmtiError JNICALL
jvmtiGetFieldDeclaringClass(jvmtiEnv* env,
	jclass klass,
	jfieldID field,
	jclass* declaring_class_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	jclass rv_declaring_class = NULL;

	Trc_JVMTI_jvmtiGetFieldDeclaringClass_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9Class * fieldClass;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_JFIELDID_NON_NULL(field);
		ENSURE_NON_NULL(declaring_class_ptr);

		fieldClass = getCurrentClass(((J9JNIFieldID *) field)->declaringClass);
		rv_declaring_class = (jclass) vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, J9VM_J9CLASS_TO_HEAPCLASS(fieldClass));

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != declaring_class_ptr) {
		*declaring_class_ptr = rv_declaring_class;
	}
	TRACE_JVMTI_RETURN(jvmtiGetFieldDeclaringClass);
}


jvmtiError JNICALL
jvmtiGetFieldModifiers(jvmtiEnv* env,
	jclass klass,
	jfieldID field,
	jint* modifiers_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	jint rv_modifiers = 0;

	Trc_JVMTI_jvmtiGetFieldModifiers_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9ROMFieldShape * romField;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_JFIELDID_NON_NULL(field);
		ENSURE_NON_NULL(modifiers_ptr);

		romField = ((J9JNIFieldID *) field)->field;
		rv_modifiers = (jint) (romField->modifiers &
			(J9AccPublic | J9AccPrivate | J9AccProtected | J9AccStatic | J9AccFinal | J9AccVolatile | J9AccTransient | J9AccEnum));
		rc = JVMTI_ERROR_NONE;

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != modifiers_ptr) {
		*modifiers_ptr = rv_modifiers;
	}
	TRACE_JVMTI_RETURN(jvmtiGetFieldModifiers);
}


jvmtiError JNICALL
jvmtiIsFieldSynthetic(jvmtiEnv* env,
	jclass klass,
	jfieldID field,
	jboolean* is_synthetic_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;
	jboolean rv_is_synthetic = JNI_FALSE;

	Trc_JVMTI_jvmtiIsFieldSynthetic_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9ROMFieldShape * romFieldShape;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);
		ENSURE_CAPABILITY(env, can_get_synthetic_attribute);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_JFIELDID_NON_NULL(field);
		ENSURE_NON_NULL(is_synthetic_ptr);

		romFieldShape = ((J9JNIFieldID *) field)->field;
		rv_is_synthetic = (romFieldShape->modifiers & J9AccSynthetic) ? JNI_TRUE : JNI_FALSE;

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != is_synthetic_ptr) {
		*is_synthetic_ptr = rv_is_synthetic;
	}
	TRACE_JVMTI_RETURN(jvmtiIsFieldSynthetic);
}



