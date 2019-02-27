/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
#include "VMHelpers.hpp"

extern "C" {

/* java.lang.Class: public native boolean isAssignableFrom(Class<?> cls); */
jboolean JNICALL
Fast_java_lang_Class_isAssignableFrom(J9VMThread *currentThread, j9object_t receiverObject, j9object_t parmObject)
{
	jboolean result = JNI_FALSE;

	if (NULL == parmObject) {
		setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		J9Class *parmClazz = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, parmObject);
		J9Class *receiverClazz = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, receiverObject);
		if (J9ROMCLASS_IS_PRIMITIVE_TYPE(parmClazz->romClass) || J9ROMCLASS_IS_PRIMITIVE_TYPE(receiverClazz->romClass)) {
			if (parmClazz == receiverClazz) {
				result = JNI_TRUE;
			}
		} else {
			result = (jboolean)VM_VMHelpers::inlineCheckCast(parmClazz, receiverClazz);
		}

	}
	return result;
}

/* java.lang.Class: private static native Class forNameImpl(String className, boolean initializeBoolean, ClassLoader classLoader) throws ClassNotFoundException; */
j9object_t JNICALL
Fast_java_lang_Class_forNameImpl(J9VMThread *currentThread, j9object_t classNameObject, jboolean initializeBoolean, j9object_t classLoaderObject)
{
	J9JavaVM * vm = currentThread->javaVM;
	J9ClassLoader * classLoader = NULL;
	j9object_t result = NULL;
	J9Class *foundClass = NULL;

	/* Because the JIT is passing direct object pointers, this native cannot depend
	 * on the classLoaderObject being kept alive by a stack reference.  Preserve it
	 * now for the duration of the native.
	 */
	PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, classLoaderObject);

	if (NULL == classNameObject) {
		setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
		goto done;
	}

	/* Fetch the J9ClasLoader, creating it if need be */
	if (NULL == classLoaderObject) {
		classLoader = vm->systemClassLoader;
	} else {
		classLoader = J9VMJAVALANGCLASSLOADER_VMREF(currentThread, classLoaderObject);
		if (NULL == classLoader) {
			PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, classNameObject);
			classLoader = internalAllocateClassLoader(vm, classLoaderObject);
			classNameObject = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
			if (NULL == classLoader) {
				goto done;
			}
		}
	}

	/* Make sure the name is legal */
	if (CLASSNAME_INVALID == verifyQualifiedName(currentThread, classNameObject)) {
		goto throwCNFE;
	}

	/* Find the class */
	PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, classNameObject);
	foundClass = internalFindClassString(currentThread, NULL, classNameObject, classLoader, 0);
	classNameObject = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);

	if (NULL == foundClass) {
		/* Not found - if no exception is pending, throw ClassNotFoundException */
		if (NULL == currentThread->currentException) {
throwCNFE:
			setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGCLASSNOTFOUNDEXCEPTION, (UDATA*)classNameObject);
		}
		goto done;
	}


	/* Initialize the class if requested */
	if (initializeBoolean) {
		if (VM_VMHelpers::classRequiresInitialization(currentThread, foundClass)) {
			initializeClass(currentThread, foundClass);
		}
	}
	result = (j9object_t)foundClass->classObject;
done:
	/* Discard the saved classLoaderObject before returning */
	DROP_OBJECT_IN_SPECIAL_FRAME(currentThread);
	return result;
}

/* java.lang.Class: private native boolean isArray(); */
jboolean JNICALL
Fast_java_lang_Class_isArray(J9VMThread *currentThread, j9object_t classObject)
{
	J9Class *receiverClazz = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, classObject);
	return J9ROMCLASS_IS_ARRAY(receiverClazz->romClass) ? JNI_TRUE : JNI_FALSE;
}

/* java.lang.Class: private native boolean isPrimitive(); */
jboolean JNICALL
Fast_java_lang_Class_isPrimitive(J9VMThread *currentThread, j9object_t classObject)
{
	J9Class *receiverClazz = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, classObject);
	return J9ROMCLASS_IS_PRIMITIVE_TYPE(receiverClazz->romClass) ? JNI_TRUE : JNI_FALSE;
}

/* java.lang.Class: public native boolean isInstance(Object object); */
jboolean JNICALL
Fast_java_lang_Class_isInstance(J9VMThread *currentThread, j9object_t receiverObject, j9object_t object)
{
	jboolean result = JNI_FALSE;
	/* null is not an instance of anything */
	if (NULL != object) {
		J9Class *receiverClazz = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, receiverObject);
		J9Class *objectClazz = J9OBJECT_CLAZZ(currentThread, object);
		result = (jboolean)VM_VMHelpers::inlineCheckCast(objectClazz, receiverClazz);
	}
	return result;
}

/* java.lang.Class: private native int getModifiersImpl(); */
jint JNICALL
Fast_java_lang_Class_getModifiersImpl(J9VMThread *currentThread, j9object_t receiverObject)
{
	J9Class *receiverClazz = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, receiverObject);
	J9ROMClass *romClass = NULL;
	bool isArray = J9CLASS_IS_ARRAY(receiverClazz);
	if (isArray) {
		/* Use underlying type for arrays */
		romClass = ((J9ArrayClass*)receiverClazz)->leafComponentType->romClass;
	} else {
		romClass = receiverClazz->romClass;
	}
	
	U_32 modifiers = 0;
	if (J9_ARE_NO_BITS_SET(romClass->extraModifiers, J9AccClassInnerClass)) {
		/* Not an inner class - use the modifiers field */
		modifiers = romClass->modifiers;
	} else {
		/* Use memberAccessFlags if the receiver is an inner class */
		modifiers = romClass->memberAccessFlags;
	}
	
	if (isArray) {
		/* OR in the required Sun bits */
		modifiers |= (J9AccAbstract + J9AccFinal);
	}
	return (jint)modifiers;
}

/* java.lang.Class: private native Class<?> arrayTypeImpl(); */
j9object_t JNICALL
Fast_java_lang_Class_arrayTypeImpl(J9VMThread *currentThread, j9object_t receiverObject)
{
	j9object_t arrayObject = NULL;
	J9Class *componentClazz = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, receiverObject);
	J9Class *arrayClazz = componentClazz->arrayClass;
	if (NULL == arrayClazz) {
		arrayClazz = internalCreateArrayClass(currentThread, 
			(J9ROMArrayClass *) J9ROMIMAGEHEADER_FIRSTCLASS(currentThread->javaVM->arrayROMClasses), 
			componentClazz);
	}
	if (NULL != arrayClazz) {
		arrayObject = J9VM_J9CLASS_TO_HEAPCLASS(arrayClazz);
	}
	return arrayObject;
}

/* java.lang.Class: public native Class<?> getComponentType(); */
j9object_t JNICALL
Fast_java_lang_Class_getComponentType(J9VMThread *currentThread, j9object_t receiverObject)
{
	J9Class *receiverClazz = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, receiverObject);
	j9object_t componentType = NULL;
	if (J9CLASS_IS_ARRAY(receiverClazz)) {
		componentType = J9VM_J9CLASS_TO_HEAPCLASS(((J9ArrayClass*)receiverClazz)->componentType);
	}
	return componentType;
}

J9_FAST_JNI_METHOD_TABLE(java_lang_Class)
	J9_FAST_JNI_METHOD("isAssignableFrom", "(Ljava/lang/Class;)Z", Fast_java_lang_Class_isAssignableFrom,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
	J9_FAST_JNI_METHOD("forNameImpl", "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;", Fast_java_lang_Class_forNameImpl,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("isArray", "()Z", Fast_java_lang_Class_isArray,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
	J9_FAST_JNI_METHOD("isPrimitive", "()Z", Fast_java_lang_Class_isPrimitive,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
	J9_FAST_JNI_METHOD("isInstance", "(Ljava/lang/Object;)Z", Fast_java_lang_Class_isInstance,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
	J9_FAST_JNI_METHOD("getModifiersImpl", "()I", Fast_java_lang_Class_getModifiersImpl,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
	J9_FAST_JNI_METHOD("getComponentType", "()Ljava/lang/Class;", Fast_java_lang_Class_getComponentType,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
	J9_FAST_JNI_METHOD("arrayTypeImpl", "()Ljava/lang/Class;", Fast_java_lang_Class_arrayTypeImpl,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
J9_FAST_JNI_METHOD_TABLE_END

}
