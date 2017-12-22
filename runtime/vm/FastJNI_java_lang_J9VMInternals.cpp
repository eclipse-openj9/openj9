/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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

#include "fastJNI.h"

#include "j9protos.h"
#include "j9consts.h"
#include "objhelp.h"
#include "vm_internal.h"
#include "ObjectHash.hpp"
#include "VMHelpers.hpp"
#include "ObjectAllocationAPI.hpp"
#include "ObjectAccessBarrierAPI.hpp"

extern "C" {

/* java.lang.J9VMInternals: static native Class getSuperclass(Class clazz); */
j9object_t JNICALL
Fast_java_lang_J9VMInternals_getSuperclass(J9VMThread *currentThread, j9object_t classObject)
{
	J9Class *j9clazz = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, classObject);
	J9ROMClass *romClass = j9clazz->romClass;
	j9object_t superClazz = NULL;
	if (!(J9ROMCLASS_IS_INTERFACE(romClass) || J9ROMCLASS_IS_PRIMITIVE_TYPE(romClass))) {
		superClazz = J9VM_J9CLASS_TO_HEAPCLASS(VM_VMHelpers::getSuperclass(j9clazz));
	}
	return superClazz;
}

/* java.lang.J9VMInternals: static native int identityHashCode(Object anObject); */
jint JNICALL
Fast_java_lang_J9VMInternals_identityHashCode(J9VMThread *currentThread, j9object_t objectPointer)
{
	return VM_ObjectHash::inlineObjectHashCode(currentThread->javaVM, objectPointer);
}

/* java.lang.J9VMInternals: static native Object primitiveClone(Object anObject) throws CloneNotSupportedException; */
j9object_t JNICALL
Fast_java_lang_J9VMInternals_primitiveClone(J9VMThread *currentThread, j9object_t original)
{
	MM_ObjectAllocationAPI objectAllocate(currentThread);
	MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
	j9object_t copy = NULL;
	/* Check to see if the original object is cloneable */
	J9Class *objectClass = J9OBJECT_CLAZZ(currentThread, original);
	UDATA flags = J9CLASS_FLAGS(objectClass);
	if (J9_UNEXPECTED(0 == (flags & J9AccClassCloneable))) {
		setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGCLONENOTSUPPORTEDEXCEPTION, NULL);
		goto done;
	}
	if (flags & J9AccClassArray) {
		U_32 size = J9INDEXABLEOBJECT_SIZE(currentThread, original);
		copy = objectAllocate.inlineAllocateIndexableObject(currentThread, objectClass, size, false, false, false);
		if (NULL == copy) {
			VM_VMHelpers::pushObjectInSpecialFrame(currentThread, original);
			copy = currentThread->javaVM->memoryManagerFunctions->J9AllocateIndexableObject(currentThread, objectClass, size, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
			original = VM_VMHelpers::popObjectInSpecialFrame(currentThread);
			if (J9_UNEXPECTED(NULL == copy)) {
				setHeapOutOfMemoryError(currentThread);
				goto done;
			}
		}
		objectAccessBarrier.cloneArray(currentThread, original, copy, objectClass, size);
	} else {
		copy = objectAllocate.inlineAllocateObject(currentThread, objectClass, false, false);
		if (NULL == copy) {
			VM_VMHelpers::pushObjectInSpecialFrame(currentThread, original);
			copy = currentThread->javaVM->memoryManagerFunctions->J9AllocateObject(currentThread, objectClass, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
			original = VM_VMHelpers::popObjectInSpecialFrame(currentThread);
			if (J9_UNEXPECTED(NULL == copy)) {
				setHeapOutOfMemoryError(currentThread);
				goto done;
			}
		}
		objectAccessBarrier.cloneObject(currentThread, original, copy, objectClass);
		VM_VMHelpers::checkIfFinalizeObject(currentThread, copy);
	}
done:
	return copy;
}

/* java.lang.J9VMInternals: private static native Class[] getInterfaces(Class clazz); */
j9object_t JNICALL
Fast_java_lang_J9VMInternals_getInterfaces(J9VMThread *currentThread, j9object_t clazz)
{
	return getInterfacesHelper(currentThread, clazz);
}

/* java.lang.J9VMInternals: private static native void prepareClassImpl(Class clazz); */
void JNICALL
Fast_java_lang_J9VMInternals_prepareClassImpl(J9VMThread *currentThread, j9object_t classObject)
{
	prepareClass(currentThread, J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, classObject));
}


J9_FAST_JNI_METHOD_TABLE(java_lang_J9VMInternals)
	J9_FAST_JNI_METHOD("getSuperclass", "(Ljava/lang/Class;)Ljava/lang/Class;", Fast_java_lang_J9VMInternals_getSuperclass,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("identityHashCode", "(Ljava/lang/Object;)I", Fast_java_lang_J9VMInternals_identityHashCode,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("primitiveClone", "(Ljava/lang/Object;)Ljava/lang/Object;", Fast_java_lang_J9VMInternals_primitiveClone,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("getInterfaces", "(Ljava/lang/Class;)[Ljava/lang/Class;", Fast_java_lang_J9VMInternals_getInterfaces,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("prepareClassImpl", "(Ljava/lang/Class;)V", Fast_java_lang_J9VMInternals_prepareClassImpl,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
J9_FAST_JNI_METHOD_TABLE_END

}
