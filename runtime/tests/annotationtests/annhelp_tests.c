/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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

#include "vmaccess.h"

/**
 * Checks if a field or method contains the runtime annotation specified.
 *
 * @param env The JNI environment.
 * @param jlClass The class that the fieldref or method belongs to.
 * @param cpIndex The constant pool index of the fieldref or methodref.
 * @param annotationNameString The name of the annotation to check for.
 * @param isField True if checking for a field, false if checking a method.
 * @return JNI_TRUE if the annotation is found, JNI_FALSE otherwise.
 */
jboolean JNICALL
Java_org_openj9_test_annotation_ContainsRuntimeAnnotationTest_containsRuntimeAnnotation(JNIEnv *env, jclass unused, jclass jlClass, jint cpIndex, jstring annotationNameString, jboolean isField, jint type)
{
	jboolean result = JNI_FALSE;
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (NULL == annotationNameString) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, "annotation name is null");
	} else {
		vmFuncs->internalEnterVMFromJNI(currentThread);

		j9object_t annotationNameObj = J9_JNI_UNWRAP_REFERENCE(annotationNameString);
		char annotationNameStackBuffer[J9VM_PACKAGE_NAME_BUFFER_LENGTH] = {0};
		J9UTF8 *annotationNameUTF8 = vmFuncs->copyStringToJ9UTF8WithMemAlloc(
										currentThread,
										annotationNameObj,
										J9_STR_NULL_TERMINATE_RESULT,
										"",
										0,
										annotationNameStackBuffer,
										0);

		if (NULL == annotationNameUTF8) {
			vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
		} else {
			J9Class *clazz = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, J9_JNI_UNWRAP_REFERENCE(jlClass));

			if (NULL == clazz) {
				vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, "class cannot be found");
			} else {
				if (isField) {
					result = (jboolean)vmFuncs->fieldContainsRuntimeAnnotation(currentThread, clazz, (UDATA)cpIndex, annotationNameUTF8);
				} else {
					/* will add method support later */
				}
			}

			if ((J9UTF8 *)annotationNameStackBuffer != annotationNameUTF8) {
				j9mem_free_memory(annotationNameUTF8);
			}
		}

		vmFuncs->internalExitVMToJNI(currentThread);
	}

	return result;
}

/**
 * Checks if a method contains the runtime annotation specified.
 *
 * @param env The JNI environment.
 * @param method The reflect method to query
 * @param annotationNameString The name of the annotation to check for.
 * @return JNI_TRUE if the annotation is found, JNI_FALSE otherwise.
 */
jboolean JNICALL
Java_org_openj9_test_annotation_ContainsRuntimeAnnotationTest_methodContainsRuntimeAnnotation(JNIEnv *env, jclass unused, jobject method, jstring annotationNameString)
{
	jboolean result = JNI_FALSE;
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (NULL == annotationNameString) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, "annotation name is null");
	} else if (NULL == method) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, "method is null");
	} else {
		vmFuncs->internalEnterVMFromJNI(currentThread);

		j9object_t annotationNameObj = J9_JNI_UNWRAP_REFERENCE(annotationNameString);
		char annotationNameStackBuffer[J9VM_PACKAGE_NAME_BUFFER_LENGTH] = {0};
		J9UTF8 *annotationNameUTF8 = vmFuncs->copyStringToJ9UTF8WithMemAlloc(
										currentThread,
										annotationNameObj,
										J9_STR_NULL_TERMINATE_RESULT,
										"",
										0,
										annotationNameStackBuffer,
										0);

		if (NULL == annotationNameUTF8) {
			vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
		} else {
			j9object_t methodObject = J9_JNI_UNWRAP_REFERENCE(method);
			J9JNIMethodID *methodID = vm->reflectFunctions.idFromMethodObject(currentThread, methodObject);
			J9Method *ramMethod = methodID->method;

			result = (jboolean)vmFuncs->methodContainsRuntimeAnnotation(currentThread, ramMethod, annotationNameUTF8);

			if ((J9UTF8 *)annotationNameStackBuffer != annotationNameUTF8) {
				j9mem_free_memory(annotationNameUTF8);
			}
		}

		vmFuncs->internalExitVMToJNI(currentThread);
	}

	return result;
}
