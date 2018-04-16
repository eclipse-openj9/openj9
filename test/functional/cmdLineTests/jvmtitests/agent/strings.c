/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
#include <string.h>

#include "jvmti_test.h"

/**
 * \brief	Get name of a class
 * \ingroup 	jvmti.test.utils
 *
 * @param[in] env     jvmti test environment
 * @param[in] klass   jni ref to the class
 * @return dynamically allocated class signature
 *
 * The caller is responsible for freeing the returned class name using
 * <code>(*jvmti_env)->Deallocate</code>
 */
char *
className(agentEnv * env, jclass klass)
{
	jvmtiError err;
	char *name;
	JVMTI_ACCESS_FROM_AGENT(env);

	err = (*jvmti_env)->GetClassSignature(jvmti_env, klass, &name, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to retrieve class name");
		return NULL;
	}

	return name;
}


/**
 * \brief	Get name of a method
 * \ingroup     jvmti.test.utils
 *
 * @param[in] env     jvmti test environment
 * @param[in] method  jni ref to the method
 * @return  dynamically allocated method signature
 *
 *  The caller is responsible for freeing the returned method name using
 *  <code>(*jvmti_env)->Deallocate</code>
 */
char *
methodName(agentEnv * env, jmethodID method)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err;
	jclass declaringClass;
	IDATA    methodNameLen;
	char * methodName;
	char * methodSig;
	char * declaringClassName;
	char * methodNameStr;
	char   methodNameBuf[JVMTI_TEST_NAME_MAX];

	err = (*jvmti_env)->GetMethodDeclaringClass(jvmti_env, method, &declaringClass);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to get declaring class");
		return NULL;
	}
	declaringClassName = className(env, declaringClass);
	if (NULL == declaringClassName)
		return NULL;

	err = (*jvmti_env)->GetMethodName(jvmti_env, method, &methodName, &methodSig, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to get method name");
		return NULL;
	}

	strcpy(methodNameBuf, declaringClassName);
	strcat(methodNameBuf, ".");
	strcat(methodNameBuf, methodName);
	strcat(methodNameBuf, methodSig);

	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) methodName);
	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) methodSig);

	methodNameLen = strlen(methodNameBuf);
	err = (*jvmti_env)->Allocate(jvmti_env, methodNameLen, (unsigned char **) &methodNameStr);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to allocate return buffer for method name and signature");
		return NULL;
	}

	return methodNameStr;
}

 /**
 * \brief	Get name of a field
 * \ingroup     jvmti.test.utils
 *
 * @param[in] env     jvmti test environment
 * @param[in] field   jni ref to the field
 * @return  dynamically allocated field signature
 *
 *  The caller is responsible for freeing the returned field name using
 *  <code>(*jvmti_env)->Deallocate</code>
 */
char *
fieldName(agentEnv * env, JNIEnv* jni_env, jclass klass, jfieldID field)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err;
	jclass declaringClass;
	IDATA  fieldNameLen;
	char * fieldName;
	char * fieldSig;
	char * declaringClassName;
	char * fieldNameStr;
	char   fieldNameBuf[JVMTI_TEST_NAME_MAX];


	err = (*jvmti_env)->GetFieldDeclaringClass(jvmti_env, klass, field, &declaringClass);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to get declaring class");
		return NULL;
	}
	declaringClassName = className(env, declaringClass);
	err = (*jvmti_env)->GetFieldName(jvmti_env, klass, field, &fieldName, &fieldSig, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to get field name");
		return NULL;
	}

	strcpy(fieldNameBuf, declaringClassName);
	strcat(fieldNameBuf, ".");
	strcat(fieldNameBuf, fieldName);
	strcat(fieldNameBuf, " ");
	strcat(fieldNameBuf, fieldSig);

	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) fieldName);
	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) fieldSig);

	fieldNameLen = strlen(fieldNameBuf);
	err = (*jvmti_env)->Allocate(jvmti_env, fieldNameLen, (unsigned char **) &fieldNameStr);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to allocate return buffer for field name and signature");
		return NULL;
	}

	return fieldNameStr;
}


char *
getTypeString(agentEnv * env, jthread currentThread, JNIEnv * jni_env, char signatureType, jvalue * jvaluePtr)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	threadData * data = getThreadData(env, currentThread);
	char * typeStr = data->typeStr;

	switch (signatureType) {
		case 'Z':
			if (jvaluePtr->z) {
				strcpy(typeStr, "(jboolean) true");
			} else {
				strcpy(typeStr, "(jboolean) false");
			}
			break;
		case 'B':
			sprintf(typeStr, "(jbyte) %d", jvaluePtr->b);
			break;
		case 'C':
			sprintf(typeStr, "(jchar) %d", jvaluePtr->c);
			break;
		case 'S':
			sprintf(typeStr, "(jshort) %d", jvaluePtr->s);
			break;
		case 'I':
			sprintf(typeStr, "(jint) %d", jvaluePtr->i);
			break;
		case 'J':
			sprintf(typeStr, "(jlong) %lld", jvaluePtr->j);
			break;
		case 'F':
			sprintf(typeStr, "(jfloat) %f", jvaluePtr->f);
			break;
		case 'D':
			sprintf(typeStr, "(jdouble) %f", jvaluePtr->d);
			break;
		case 'L':
			if (jvaluePtr->l == NULL) {
				sprintf(typeStr, "(jobject) null");
			} else {
				jclass objClass;

				objClass = (*jni_env)->GetObjectClass(jni_env, jvaluePtr->l);
				if (objClass == NULL) {
					(*jni_env)->ExceptionClear(jni_env);
					strcpy(typeStr, "(jobject) <failed to get object class via JNI>");
				} else {
					char * name;
					jvmtiError err = (*jvmti_env)->GetClassSignature(jvmti_env, objClass, &name, NULL);

					if (err == JVMTI_ERROR_NONE) {
						sprintf(typeStr, "(jobject) instance of %s", name);
						(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) name);
					} else {
						error(env, err, "Failed to get class signature");
						return NULL;
					}
				}
			}
			break;
		default:
			sprintf(typeStr, "!!! unknown type %c", signatureType);
			break;
	}

	return typeStr;
}


void
getParamKindName(agentEnv * env, jvmtiParamKind paramKind, char * name)
{
	switch (paramKind) {
		case JVMTI_KIND_IN:
			strcpy(name, "JVMTI_KIND_IN");
			break;
		case JVMTI_KIND_IN_PTR:
			strcpy(name, "JVMTI_KIND_IN_PTR");
			break;
		case JVMTI_KIND_IN_BUF:
			strcpy(name, "JVMTI_KIND_IN_BUF");
			break;
		case JVMTI_KIND_ALLOC_BUF:
			strcpy(name, "JVMTI_KIND_ALLOC_BUF");
			break;
		case JVMTI_KIND_ALLOC_ALLOC_BUF:
			strcpy(name, "JVMTI_KIND_ALLOC_ALLOC_BUF");
			break;
		case JVMTI_KIND_OUT:
			strcpy(name, "JVMTI_KIND_OUT");
			break;
		case JVMTI_KIND_OUT_BUF:
			strcpy(name, "JVMTI_KIND_BUF");
			break;
		default:
			sprintf(name, "**UNKNOWN(%d)** ", paramKind);
	}
}



void
getPrimitiveTypeName(agentEnv * env, jvmtiParamTypes baseType, char * name)
{
	switch (baseType) {
		case JVMTI_TYPE_JBYTE:
			strcpy(name, "JVMTI_TYPE_JBYTE");
			break;
		case JVMTI_TYPE_JCHAR:
			strcpy(name, "JVMTI_TYPE_JCHAR");
			break;
		case JVMTI_TYPE_JSHORT:
			strcpy(name, "JVMTI_TYPE_JSHORT");
			break;
		case JVMTI_TYPE_JINT:
			strcpy(name, "JVMTI_TYPE_JINT");
			break;
		case JVMTI_TYPE_JLONG:
			strcpy(name, "JVMTI_TYPE_JLONG");
			break;
		case JVMTI_TYPE_JFLOAT:
			strcpy(name, "JVMTI_TYPE_JFLOAT");
			break;
		case JVMTI_TYPE_JDOUBLE:
			strcpy(name, "JVMTI_TYPE_JDOUBLE");
			break;
		case JVMTI_TYPE_JBOOLEAN:
			strcpy(name, "JVMTI_TYPE_JBOOLEAN");
			break;
		case JVMTI_TYPE_JOBJECT:
			strcpy(name, "JVMTI_TYPE_JOBJECT");
			break;
		case JVMTI_TYPE_JTHREAD:
			strcpy(name, "JVMTI_TYPE_JTHREAD");
			break;
		case JVMTI_TYPE_JCLASS:
			strcpy(name, "JVMTI_TYPE_JCLASS");
			break;
		case JVMTI_TYPE_JVALUE:
			strcpy(name, "JVMTI_TYPE_JVALUE");
			break;
		case JVMTI_TYPE_JFIELDID:
			strcpy(name, "JVMTI_TYPE_JFIELDID");
			break;
		case JVMTI_TYPE_JMETHODID:
			strcpy(name, "JVMTI_TYPE_JMETHODID");
			break;
		case JVMTI_TYPE_CCHAR:
			strcpy(name, "JVMTI_TYPE_CCHAR");
			break;
		case JVMTI_TYPE_CVOID:
			strcpy(name, "JVMTI_TYPE_CVOID");
			break;
		case JVMTI_TYPE_JNIENV:
			strcpy(name, "JVMTI_TYPE_JNIENV");
			break;
		default:
			sprintf(name, "**UNKNOWN(%d)** ", baseType);
	}
}



void
getObjectReferenceKindName(jvmtiObjectReferenceKind refKind, char * name)
{
	switch (refKind) {
		case JVMTI_REFERENCE_CLASS:
			strcpy(name, "JVMTI_REFERENCE_CLASS");
			break;
		case JVMTI_REFERENCE_FIELD:
			strcpy(name, "JVMTI_REFERENCE_FIELD");
			break;
		case JVMTI_REFERENCE_ARRAY_ELEMENT:
			strcpy(name, "JVMTI_REFERENCE_ARRAY_ELEMENT");
			break;
		case JVMTI_REFERENCE_CLASS_LOADER:
			strcpy(name, "JVMTI_REFERENCE_CLASS_LOADER");
			break;
		case JVMTI_REFERENCE_SIGNERS:
			strcpy(name, "JVMTI_REFERENCE_SIGNERS");
			break;
		case JVMTI_REFERENCE_PROTECTION_DOMAIN:
			strcpy(name, "JVMTI_REFERENCE_PROTECTION_DOMAIN");
			break;
		case JVMTI_REFERENCE_INTERFACE:
			strcpy(name, "JVMTI_REFERENCE_INTERFACE");
			break;
		case JVMTI_REFERENCE_STATIC_FIELD:
			strcpy(name, "JVMTI_REFERENCE_STATIC_FIELD");
			break;
		case JVMTI_REFERENCE_CONSTANT_POOL:
			strcpy(name, "JVMTI_REFERENCE_CONSTANT_POOL");
			break;

		default:
			sprintf(name, "**UNKNOWN(%d)**", refKind);
			break;
	}

	return;
}


void
dumpJVMTIHeapRootKind(jvmtiHeapRootKind refKind, char * name)
{
	switch (refKind) {
		case JVMTI_HEAP_ROOT_JNI_GLOBAL:
			strcpy(name, "JVMTI_HEAP_ROOT_JNI_GLOBAL");
			break;
		case JVMTI_HEAP_ROOT_SYSTEM_CLASS:
			strcpy(name, "JVMTI_HEAP_ROOT_SYSTEM_CLASS");
			break;
		case JVMTI_HEAP_ROOT_MONITOR:
			strcpy(name, "JVMTI_HEAP_ROOT_MONITOR");
			break;
		case JVMTI_HEAP_ROOT_STACK_LOCAL:
			strcpy(name, "JVMTI_HEAP_ROOT_STACK_LOCAL");
			break;
		case JVMTI_HEAP_ROOT_JNI_LOCAL:
			strcpy(name, "JVMTI_HEAP_ROOT_JNI_LOCAL");
			break;
		case JVMTI_HEAP_ROOT_THREAD:
			strcpy(name, "JVMTI_HEAP_ROOT_THREAD");
			break;
		case JVMTI_HEAP_ROOT_OTHER:
			strcpy(name, "JVMTI_HEAP_ROOT_OTHER");
			break;

		default:
			sprintf(name, "**UNKNOWN(%d)**", refKind);
			break;
	}

	return;
}
