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

jint JNICALL
cma001(agentEnv * agent_env, char * args)
{
	jvmtiError err;
	jvmtiCapabilities capabilities;
	JVMTI_ACCESS_FROM_AGENT(agent_env);

	env = agent_env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_redefine_classes = 1;
	capabilities.can_retransform_classes = 1;

	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Failed to AddCapabilities");
		return JNI_ERR;
	}

	return JNI_OK;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_classModificationAgent_cma001_retransformClass(JNIEnv * jni_env, jobject obj, jclass originalClass)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err;

	err = (*jvmti_env)->RetransformClasses(jvmti_env, 1, &originalClass);
    if (JVMTI_ERROR_NONE != err) {
    	error(env, err, "RetransformClasses failed");
    	return JNI_FALSE;
    }

	return JNI_TRUE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_classModificationAgent_cma001_redefineClass(JNIEnv * jni_env, jobject obj, jclass originalClass, jbyteArray redefinedClassBytes)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err;
	jvmtiClassDefinition classdef;
	jbyte *redefinedClassData;
	jsize classDataSize;

	/* Create a new class definition to be used for re-definition */
	classDataSize = (*jni_env)->GetArrayLength(jni_env, redefinedClassBytes);

	err = (*jvmti_env)->Allocate(jvmti_env, classDataSize, (unsigned char **) &redefinedClassData);
	if (JVMTI_ERROR_NONE != err) {
		error(env, err, "Unable to allocate temp buffer for the class file");
		return JNI_FALSE;
	}

	(*jni_env)->GetByteArrayRegion(jni_env, redefinedClassBytes, 0, classDataSize, redefinedClassData);

	classdef.class_bytes = (unsigned char *) redefinedClassData;
	classdef.class_byte_count = classDataSize;
	classdef.klass = originalClass;

    err = (*jvmti_env)->RedefineClasses(jvmti_env, 1, &classdef);
    if (JVMTI_ERROR_NONE != err) {
    	error(env, err, "RedefineClasses failed");
    	return JNI_FALSE;
    }

	return JNI_TRUE;
}
