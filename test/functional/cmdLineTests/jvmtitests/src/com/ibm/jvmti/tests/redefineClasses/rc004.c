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

static agentEnv * env;

static jfieldID intid;
static jfieldID strid;

jint JNICALL
rc004(agentEnv * agent_env, char * args)
{
	jvmtiError err;
	jvmtiCapabilities capabilities;
	JVMTI_ACCESS_FROM_AGENT(agent_env);

	env = agent_env; 

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_redefine_classes = 1;
	capabilities.can_generate_breakpoint_events = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to AddCapabilities");
		return JNI_ERR;
	}						

	return JNI_OK;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_redefineClasses_rc004_redefineClass(JNIEnv * jni_env, jclass klass, jclass originalClass, jint classBytesSize, jbyteArray classBytes)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jbyte * class_bytes;
	char * classFileName = env->testArgs;
	jvmtiClassDefinition classdef;
	jvmtiError err;

	err = (*jvmti_env)->Allocate(jvmti_env, classBytesSize, (unsigned char **) &class_bytes);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Unable to allocate temp buffer for the class file");
		return JNI_FALSE;
	}

	(*jni_env)->GetByteArrayRegion(jni_env, classBytes, 0, classBytesSize, class_bytes);

	classdef.class_bytes = (unsigned char *) class_bytes;
	classdef.class_byte_count = classBytesSize;
	classdef.klass = originalClass;

	err = (*jvmti_env)->RedefineClasses(jvmti_env, 1, &classdef);
	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) class_bytes);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "RedefineClasses failed");
		return JNI_FALSE;
	}

	return JNI_TRUE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_redefineClasses_rc004_accessStoredIDs(JNIEnv * jni_env, jclass klass, jclass originalClass)
{
	jobject obj;
	jstring str;
	const char* chars;
	jint intval;

	/* Despite the fact that both static field have been redefined
	 * with initializers that change their values, they should keep
	 * their old values as per the spec.
	 * */
	
	/* Verify that the field ID is still valid by checking its value. */
	intval = (*jni_env)->GetStaticIntField(jni_env, originalClass, intid);
	if (intval != 123) {
		error(env, 0, "Value of field int1 was not 123 as expected");
		return JNI_FALSE;
	}

	/* Verify the same for the String field. */
	obj = (*jni_env)->GetStaticObjectField(jni_env, originalClass, strid);
	str = (jstring) obj;
	chars = (const char *) (*jni_env)->GetStringUTFChars(jni_env, str, NULL);
	if (chars == NULL || strcmp(chars, "abc")) {
		error(env, 0, "Value of field f1 was not 'def' as expected");
		return JNI_FALSE;
	}

	return JNI_TRUE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_redefineClasses_rc004_redefineClassAndTestFieldIDs(JNIEnv * jni_env, jclass klass, jclass originalClass, jint classBytesSize, jbyteArray classBytes)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jbyte * class_bytes;
	char * classFileName = env->testArgs;
	jvmtiClassDefinition classdef;
	jvmtiError err;
	jobject obj;
	jstring str;
	const char* chars;
	jint intval;
	jclass stringClass;

	/* Retrieve the field ID for int1. */
	intid = (*jni_env)->GetStaticFieldID(jni_env, originalClass, "int1", "I");
	if (intid == NULL) {
		error(env, 0, "Unable to retrieve original jfieldID for field int1");
		return JNI_FALSE;
	}

	/* Verify the value of the field. */
	intval = (*jni_env)->GetStaticIntField(jni_env, originalClass, intid);
	if (intval != 123) {
		error(env, 0, "Value of field int1 was not 123 as expected");
		return JNI_FALSE;
	}

	/* Retrieve the field ID for f1. */
	strid = (*jni_env)->GetStaticFieldID(jni_env, originalClass, "f1", "Ljava/lang/String;");
	if (strid == NULL) {
		error(env, 0, "Unable to retrieve original jfieldID for field f1");
		return JNI_FALSE;
	}

	/* Verify the value of the String field. */
	obj = (*jni_env)->GetStaticObjectField(jni_env, originalClass, strid);
	if (obj == NULL) {
		error(env, 0, "Could not retrieve jobject for Field f1");
		return JNI_FALSE;
	}
	stringClass = (*jni_env)->FindClass(jni_env, "java/lang/String");
	if (stringClass == NULL) {
		error(env, 0, "Could not retrieve String class");
		return JNI_FALSE;
	}
	if (!(*jni_env)->IsInstanceOf(jni_env, obj, stringClass)) {
		error(env, 0, "The Field f1 is not an instance of the String class");
		return JNI_FALSE;
	}
	str = (jstring) obj;
	chars = (const char *) (*jni_env)->GetStringUTFChars(jni_env, str, NULL);
	if (chars == NULL || strcmp((const char *) chars, "abc")) {
		error(env, 0, "Value of String Field f1 was not 'abc' as expected");
		return JNI_FALSE;
	}

	err = (*jvmti_env)->Allocate(jvmti_env, classBytesSize, (unsigned char **) &class_bytes);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Unable to allocate temp buffer for the class file");
		return JNI_FALSE;
	}

	(*jni_env)->GetByteArrayRegion(jni_env, classBytes, 0, classBytesSize, class_bytes); 

	classdef.class_bytes = (unsigned char *) class_bytes;
	classdef.class_byte_count = classBytesSize;
	classdef.klass = originalClass;

	err = (*jvmti_env)->RedefineClasses(jvmti_env, 1, &classdef);
	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) class_bytes);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "RedefineClasses failed");
		return JNI_FALSE;
	}

	/* Access stored IDs here to test them, while still in this method. It will be called again
	 * from Java, to do the same thing, but outside of this method, with a different reference
	 * to originalClass. Both should work.
	 * */
	return Java_com_ibm_jvmti_tests_redefineClasses_rc004_accessStoredIDs(jni_env, klass, originalClass);
}

