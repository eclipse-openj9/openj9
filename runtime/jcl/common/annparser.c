/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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

#include "jni.h"
#include "jcl.h"
#include "jclprots.h"
#include "jcl_internal.h"

#include "vmaccess.h"

typedef enum METHOD_OBJECT_TYPE {
	OBJECT_TYPE_METHOD,
	OBJECT_TYPE_CONSTRUCTOR,
	OBJECT_TYPE_UNKNOWN
} METHOD_OBJECT_TYPE;

jobject JNICALL
Java_com_ibm_oti_reflect_AnnotationParser_getAnnotationsData__Ljava_lang_reflect_Field_2(JNIEnv *env, jclass unusedClass, jobject jlrField)
{
	jobject result = NULL;
	j9object_t fieldObject = NULL;
	J9VMThread *vmThread = (J9VMThread *) env;

	enterVMFromJNI(vmThread);
	fieldObject = J9_JNI_UNWRAP_REFERENCE(jlrField);
	if (NULL != fieldObject) {
		J9JNIFieldID *fieldID = vmThread->javaVM->reflectFunctions.idFromFieldObject(vmThread, NULL, fieldObject);

		j9object_t annotationsData = getFieldAnnotationData(vmThread, fieldID->declaringClass, fieldID);
		if (NULL != annotationsData) {
			result = vmThread->javaVM->internalVMFunctions->j9jni_createLocalRef(env, annotationsData);
		}
	}
	exitVMToJNI(vmThread);
	return result;
}

jobject JNICALL
Java_com_ibm_oti_reflect_TypeAnnotationParser_getTypeAnnotationsDataImpl__Ljava_lang_reflect_Field_2(JNIEnv *env, jclass unusedClass, jobject jlrField)
{
	return getFieldTypeAnnotationsAsByteArray(env, jlrField);
}

static jobject
getExecutableAnnotationDataHelper(JNIEnv *env, jobject jlrMethod, METHOD_OBJECT_TYPE objType,
		j9object_t (*getAnnotationData)(struct J9VMThread *vmThread, struct J9Class *declaringClass, J9Method *ramMethod))
{
	jobject result = NULL;
	j9object_t methodObject = NULL;
	J9VMThread *vmThread = (J9VMThread *) env;

	enterVMFromJNI(vmThread);
	methodObject = J9_JNI_UNWRAP_REFERENCE(jlrMethod);
	if (NULL != methodObject) {
        J9JNIMethodID *methodID = NULL;
		J9Method *ramMethod = NULL;
		J9Class *declaringClass = NULL;
		j9object_t annotationsData = NULL;

		if ((OBJECT_TYPE_UNKNOWN == objType)) {
			objType = (J9OBJECT_CLAZZ(vmThread, methodObject) == J9VMJAVALANGREFLECTCONSTRUCTOR_OR_NULL(vmThread->javaVM)) ? OBJECT_TYPE_CONSTRUCTOR: OBJECT_TYPE_METHOD;
		}
		if (OBJECT_TYPE_CONSTRUCTOR == objType) {
			methodID = vmThread->javaVM->reflectFunctions.idFromConstructorObject(vmThread, methodObject);
		} else {
			methodID = vmThread->javaVM->reflectFunctions.idFromMethodObject(vmThread, methodObject);
		}

		ramMethod = methodID->method;
		declaringClass = J9_CURRENT_CLASS(J9_CLASS_FROM_METHOD(ramMethod));

		annotationsData = getAnnotationData(vmThread, declaringClass, ramMethod);
		if (NULL != annotationsData) {
			result = vmThread->javaVM->internalVMFunctions->j9jni_createLocalRef(env, annotationsData);
		}
	}
	exitVMToJNI(vmThread);
	return result;
}

jbyteArray getMethodTypeAnnotationsAsByteArray(JNIEnv *env, jobject jlrMethod) {
	return getExecutableAnnotationDataHelper(env, jlrMethod, OBJECT_TYPE_UNKNOWN, getMethodTypeAnnotationData);
}

jobject JNICALL
Java_com_ibm_oti_reflect_AnnotationParser_getAnnotationsData__Ljava_lang_reflect_Constructor_2(JNIEnv *env, jclass unusedClass, jobject jlrConstructor)
{
	return getExecutableAnnotationDataHelper(env, jlrConstructor, OBJECT_TYPE_CONSTRUCTOR, getMethodAnnotationData);
}

jobject JNICALL
Java_com_ibm_oti_reflect_TypeAnnotationParser_getTypeAnnotationsDataImpl__Ljava_lang_reflect_Constructor_2(JNIEnv *env, jclass unusedClass, jobject jlrConstructor)
{
	return getExecutableAnnotationDataHelper(env, jlrConstructor, OBJECT_TYPE_CONSTRUCTOR, getMethodTypeAnnotationData);
}

jobject JNICALL
Java_com_ibm_oti_reflect_AnnotationParser_getAnnotationsData__Ljava_lang_reflect_Method_2(JNIEnv *env, jclass unusedClass, jobject jlrMethod)
{
	return getExecutableAnnotationDataHelper(env, jlrMethod, OBJECT_TYPE_METHOD, getMethodAnnotationData);
}

jobject JNICALL
Java_com_ibm_oti_reflect_TypeAnnotationParser_getTypeAnnotationsDataImpl__Ljava_lang_reflect_Method_2(JNIEnv *env, jclass unusedClass, jobject jlrMethod)
{
	return getExecutableAnnotationDataHelper(env, jlrMethod, OBJECT_TYPE_UNKNOWN, getMethodTypeAnnotationData);
}

jobject JNICALL
Java_com_ibm_oti_reflect_AnnotationParser_getAnnotationsDataImpl__Ljava_lang_Class_2(JNIEnv *env, jclass unusedClass, jclass jlClass)
{
	return Java_java_lang_Class_getDeclaredAnnotationsData(env, jlClass);
}

jobject JNICALL
Java_com_ibm_oti_reflect_TypeAnnotationParser_getTypeAnnotationsDataImpl__Ljava_lang_Class_2(JNIEnv *env, jclass unusedClass, jclass jlClass)
{
	return getClassTypeAnnotationsAsByteArray(env, jlClass);
}

jobject JNICALL
Java_com_ibm_oti_reflect_AnnotationParser_getParameterAnnotationsData__Ljava_lang_reflect_Constructor_2(JNIEnv *env, jclass unusedClass, jobject jlrConstructor)
{
	return getExecutableAnnotationDataHelper(env, jlrConstructor, OBJECT_TYPE_CONSTRUCTOR, getMethodParametersAnnotationData);
}

jobject JNICALL
Java_com_ibm_oti_reflect_AnnotationParser_getParameterAnnotationsData__Ljava_lang_reflect_Method_2(JNIEnv *env, jclass unusedClass, jobject jlrMethod)
{
	return getExecutableAnnotationDataHelper(env, jlrMethod, OBJECT_TYPE_METHOD, getMethodParametersAnnotationData);
}

jobject JNICALL
Java_com_ibm_oti_reflect_AnnotationParser_getDefaultValueData(JNIEnv *env, jclass unusedClass, jobject jlrMethod)
{
	return getExecutableAnnotationDataHelper(env, jlrMethod, OBJECT_TYPE_METHOD, getMethodDefaultAnnotationData);
}

jobject JNICALL
Java_com_ibm_oti_reflect_AnnotationParser_getConstantPool(JNIEnv *env, jclass unusedClass, jobject classToIntrospect)
{
	return Java_java_lang_Access_getConstantPool(env, unusedClass, classToIntrospect);
}
