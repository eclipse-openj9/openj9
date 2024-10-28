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

#include <jni.h>
#include "j9.h"
#include "VMHelpers.hpp"

extern "C" {
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
JNIEXPORT jboolean JNICALL
JVM_IsValhallaEnabled()
{
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
JVM_IsImplicitlyConstructibleClass(JNIEnv *env, jclass cls)
{
	jboolean result = JNI_FALSE;
	J9VMThread *currentThread = (J9VMThread *)env;
	J9InternalVMFunctions const * const vmFuncs = currentThread->javaVM->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	if (NULL == cls) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		J9Class *clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, cls);
		J9ROMClass *romClass = clazz->romClass;
		if (J9_ARE_ALL_BITS_SET(romClass->optionalFlags, J9_ROMCLASS_OPTINFO_IMPLICITCREATION_ATTRIBUTE)
			&& J9_ARE_ALL_BITS_SET(getImplicitCreationFlags(romClass), J9AccImplicitCreateHasDefaultValue)
		) {
			result = JNI_TRUE;
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);
	return result;
}

JNIEXPORT jboolean JNICALL
JVM_IsNullRestrictedArray(JNIEnv *env, jobject obj)
{
	jboolean result = JNI_FALSE;
	J9VMThread *currentThread = (J9VMThread *)env;
	J9InternalVMFunctions *vmFuncs = currentThread->javaVM->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	if (NULL == obj) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		J9Class *j9clazz = J9OBJECT_CLAZZ(currentThread, J9_JNI_UNWRAP_REFERENCE(obj));
		if (J9_IS_J9ARRAYCLASS_NULL_RESTRICTED(j9clazz)) {
			result = JNI_TRUE;
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);
	return result;
}

JNIEXPORT jarray JNICALL
JVM_NewNullRestrictedArray(JNIEnv *env, jclass componentType, jint length)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = currentThread->javaVM->internalVMFunctions;
	J9Class *ramClass = NULL;
	j9object_t newArray = NULL;
	jarray arrayRef = NULL;

	vmFuncs->internalEnterVMFromJNI(currentThread);
	ramClass = J9VMJAVALANGCLASS_VMREF(currentThread, J9_JNI_UNWRAP_REFERENCE(componentType));

	if (length < 0) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		goto done;
	}

	if (!(J9_IS_J9CLASS_VALUETYPE(ramClass) && J9_IS_J9CLASS_ALLOW_DEFAULT_VALUE(ramClass))) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		goto done;
	}

	if (NULL == J9CLASS_GET_NULLRESTRICTED_ARRAY(ramClass)) {
		J9ROMArrayClass *arrayOfObjectsROMClass = (J9ROMArrayClass *)J9ROMIMAGEHEADER_FIRSTCLASS(vm->arrayROMClasses);
		vmFuncs->internalCreateArrayClassWithOptions(
			currentThread, arrayOfObjectsROMClass, ramClass, J9_FINDCLASS_FLAG_CLASS_OPTION_NULL_RESTRICTED_ARRAY);
		if (NULL != currentThread->currentException) {
			goto done;
		}
		ramClass = VM_VMHelpers::currentClass(ramClass);
	}

	newArray = vm->memoryManagerFunctions->J9AllocateIndexableObject(
			currentThread, J9CLASS_GET_NULLRESTRICTED_ARRAY(ramClass), length, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);

	if (NULL == newArray) {
		vmFuncs->setHeapOutOfMemoryError(currentThread);
		goto done;
	}

	arrayRef = (jarray)vmFuncs->j9jni_createLocalRef(env, newArray);
done:
	vmFuncs->internalExitVMToJNI(currentThread);
	return arrayRef;
}
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
} /* extern "C" */
