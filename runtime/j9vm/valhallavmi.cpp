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

#include <assert.h>
#include <jni.h>
#include "j9.h"
#include "VMHelpers.hpp"

extern "C" {

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)

JNIEXPORT jarray JNICALL
JVM_CopyOfSpecialArray(JNIEnv *env, jarray orig, jint from, jint to)
{
	j9object_t origObj = NULL;
	j9object_t newArrayObj = NULL;
	J9Class *origClass = NULL;
	UDATA origLength = 0;
	jint len = 0;
	jarray out = NULL;
	I_32 rc = 0;
	J9VMThread *currentThread = (J9VMThread *)env;
	J9InternalVMFunctions *vmFuncs = currentThread->javaVM->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);

	if (NULL == orig) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
		goto done;
	}
	origObj = J9_JNI_UNWRAP_REFERENCE(orig);
	origClass = J9OBJECT_CLAZZ(currentThread, origObj);
	origLength = J9INDEXABLEOBJECT_SIZE(currentThread, origObj);
	if ((from < 0) || (to > (jint)origLength)) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGARRAYINDEXOUTOFBOUNDSEXCEPTION, NULL);
		goto done;
	}
	if (from >= to) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		goto done;
	}

	len = to - from;
	newArrayObj = currentThread->javaVM->memoryManagerFunctions->J9AllocateIndexableObject(currentThread, origClass, len, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	if (!newArrayObj) {
		vmFuncs->setHeapOutOfMemoryError(currentThread);
		goto done;
	}

	VM_VMHelpers::pushObjectInSpecialFrame(currentThread, newArrayObj);
	origObj = J9_JNI_UNWRAP_REFERENCE(orig);

	rc = vmFuncs->copyFlattenableArray(currentThread, origObj, newArrayObj, from, 0, len);
	if (-1 == rc) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGARRAYSTOREEXCEPTION, NULL);
		goto done;
	} else if (-2 == rc) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
		goto done;
	}

	newArrayObj = VM_VMHelpers::popObjectInSpecialFrame(currentThread);
	out = (jarray)vmFuncs->j9jni_createLocalRef(env, newArrayObj);
done:
	vmFuncs->internalExitVMToJNI(currentThread);
	return out;
}

JNIEXPORT jboolean JNICALL
JVM_IsAtomicArray(JNIEnv *env, jobject obj)
{
	/* J9 doesn't currently support non-atomic arrays. */
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
JVM_IsFlatArray(JNIEnv *env, jobject obj)
{
	jboolean result = JNI_FALSE;
	J9VMThread *currentThread = (J9VMThread *)env;
	J9InternalVMFunctions *vmFuncs = currentThread->javaVM->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	if (NULL == obj) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		J9Class *j9clazz = J9OBJECT_CLAZZ(currentThread, J9_JNI_UNWRAP_REFERENCE(obj));
		if (J9CLASS_IS_ARRAY(j9clazz) && J9_IS_J9CLASS_FLATTENED(j9clazz)) {
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

JNIEXPORT jboolean JNICALL
JVM_IsValhallaEnabled()
{
	return JNI_TRUE;
}

static jarray
newArrayHelper(JNIEnv *env, jclass componentType, jint length, bool isNullRestricted, jobject initialValueJni)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = currentThread->javaVM->internalVMFunctions;
	J9Class *ramClass = NULL;
	J9Class *arrayClass = NULL;
	UDATA options = 0;
	j9object_t newArray = NULL;
	jarray arrayRef = NULL;
	j9object_t initialValue = NULL;

	vmFuncs->internalEnterVMFromJNI(currentThread);
	ramClass = J9VMJAVALANGCLASS_VMREF(currentThread, J9_JNI_UNWRAP_REFERENCE(componentType));

	if ((NULL == ramClass) || (length < 0)) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		goto done;
	}

	if (!J9_IS_J9CLASS_VALUETYPE(ramClass)) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		goto done;
	}

	if (isNullRestricted) {
		J9Class *storeValueClass = NULL;
		if (NULL == initialValueJni) {
			vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
			goto done;
		}
		initialValue = J9_JNI_UNWRAP_REFERENCE(initialValueJni);
		storeValueClass = J9OBJECT_CLAZZ(currentThread, initialValue);
		/* The initial value class must be the same as the array type since
		 * value classes can't be extended and the superclass
		 * java/lang/Object can't be assigned.
		 */
		if (storeValueClass != ramClass) {
			vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
			goto done;
		}
	}

	arrayClass = ramClass->arrayClass;
	if (isNullRestricted) {
		arrayClass = J9CLASS_GET_NULLRESTRICTED_ARRAY(ramClass);
		options = J9_FINDCLASS_FLAG_CLASS_OPTION_NULL_RESTRICTED_ARRAY;
	}

	if (NULL == arrayClass) {
		J9ROMArrayClass *arrayOfObjectsROMClass = (J9ROMArrayClass *)J9ROMIMAGEHEADER_FIRSTCLASS(vm->arrayROMClasses);
		arrayClass = vmFuncs->internalCreateArrayClassWithOptions(currentThread, arrayOfObjectsROMClass, ramClass, options);
		if (NULL != currentThread->currentException) {
			goto done;
		}
		ramClass = VM_VMHelpers::currentClass(ramClass);
	}

	newArray = vm->memoryManagerFunctions->J9AllocateIndexableObject(
			currentThread, arrayClass, length, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	if (NULL == newArray) {
		vmFuncs->setHeapOutOfMemoryError(currentThread);
		goto done;
	}

	/* Set initial values. */
	if (isNullRestricted) {
		for (jint index = 0; index < length; index++) {
			vmFuncs->storeFlattenableArrayElement(currentThread, newArray, index, initialValue);
		}
	}

	arrayRef = (jarray)vmFuncs->j9jni_createLocalRef(env, newArray);
done:
	vmFuncs->internalExitVMToJNI(currentThread);
	return arrayRef;
}

JNIEXPORT jarray JNICALL
JVM_NewNullableAtomicArray(JNIEnv *env, jclass componentType, jint length)
{
	return newArrayHelper(env, componentType, length, false, NULL);
}

JNIEXPORT jarray JNICALL
JVM_NewNullRestrictedAtomicArray(JNIEnv *env, jclass componentType, jint length, jobject initialValue)
{
	return newArrayHelper(env, componentType, length, true, initialValue);
}

JNIEXPORT jarray JNICALL
JVM_NewNullRestrictedNonAtomicArray(JNIEnv *env, jclass componentType, jint length, jobject initialValue)
{
	/* J9 doesn't currently support flattened arrays that accesss fields
	 * non-atomically. For now use the same implementation as
	 * JVM_NewNullRestrictedAtomicArray.
	 */
	return newArrayHelper(env, componentType, length, true, initialValue);
}

#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */

} /* extern "C" */
