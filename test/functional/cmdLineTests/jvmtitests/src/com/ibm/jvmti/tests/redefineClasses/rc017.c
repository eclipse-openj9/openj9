/*******************************************************************************
 * Copyright (c) 2013, 2013 IBM Corp. and others
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
rc017(agentEnv * agent_env, char * args)
{
	jvmtiError err;
	jvmtiCapabilities capabilities;
	JVMTI_ACCESS_FROM_AGENT(agent_env);

	env = agent_env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_redefine_classes = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to AddCapabilities");
		return JNI_ERR;
	}

	return JNI_OK;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_redefineClasses_rc017_redefineMultipleClass(JNIEnv * jni_env, jclass klass, jobjectArray originalClasses, jint classCount, jobjectArray classBytes, jintArray classBytesLength)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jbyte * class_bytes;
	jbyteArray class_bytes_arr;
	char * classFileName = env->testArgs;
	jvmtiClassDefinition *classdef = NULL;
	jvmtiError err = JVMTI_ERROR_NONE;
	jint *classBytesSizes = NULL;
	jint i = 0;
	jboolean rc = JNI_TRUE;

	if (0 >= classCount) {
		error(env, err, "Invalid class count paremeter received");
		return JNI_FALSE;
	}

	err = (*jvmti_env)->Allocate(jvmti_env, sizeof(jvmtiClassDefinition) * classCount,  (unsigned char **)&classdef);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "cannot allocate memory for internal usage");
		return JNI_FALSE;
	}

	err = (*jvmti_env)->Allocate(jvmti_env, sizeof(jint) * classCount,  (unsigned char **)&classBytesSizes);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "cannot allocate memory for internal usage");
		return JNI_FALSE;
	}

	memset(classdef, 0, sizeof(jvmtiClassDefinition) * classCount);

	(*jni_env)->GetIntArrayRegion(jni_env, classBytesLength, 0, classCount, classBytesSizes);

	for (i = 0; i < classCount; i++) {
		class_bytes_arr = (*jni_env)->GetObjectArrayElement(jni_env, classBytes, i);

		err = (*jvmti_env)->Allocate(jvmti_env, classBytesSizes[i], (unsigned char **) &class_bytes);
		if (err != JVMTI_ERROR_NONE) {
			error(env, err, "Unable to allocate temp buffer for the class file");
			rc = JNI_FALSE;
			goto fail;
		}

		(*jni_env)->GetByteArrayRegion(jni_env, class_bytes_arr, 0, classBytesSizes[i], class_bytes);

		classdef[i].class_bytes = (unsigned char *) class_bytes;
		classdef[i].class_byte_count = classBytesSizes[i];
		classdef[i].klass = (*jni_env)->GetObjectArrayElement(jni_env, originalClasses, i);
	}

	(*jvmti_env)->RedefineClasses(jvmti_env, classCount, classdef);

fail:


	while (--i >=0 && classdef[i].class_bytes != NULL) {
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) classdef[i].class_bytes);
		i--;
	}

	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)classdef);
	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)classBytesSizes);

	return rc;
}
