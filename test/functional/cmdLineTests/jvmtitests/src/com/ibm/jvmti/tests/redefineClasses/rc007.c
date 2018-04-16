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
rc007(agentEnv * agent_env, char * args)
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
Java_com_ibm_jvmti_tests_redefineClasses_rc007_redefineClasses(JNIEnv * jni_env, jclass klass,
		jclass originalClass1, jint classBytesSize1, jbyteArray classBytes1,
		jclass originalClass2, jint classBytesSize2, jbyteArray classBytes2)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jbyte * class_bytes0;
	jbyte * class_bytes1;
	jvmtiClassDefinition classdef[2];
	jvmtiError err; 

	err = (*jvmti_env)->Allocate(jvmti_env, classBytesSize1, (unsigned char **) &class_bytes0);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Unable to allocate temp buffer for the class file");
		return JNI_FALSE;
	}
	
	(*jni_env)->GetByteArrayRegion(jni_env, classBytes1, 0, classBytesSize1, class_bytes0); 

	classdef[0].class_bytes = (unsigned char *) class_bytes0;
	classdef[0].class_byte_count = classBytesSize1;
	classdef[0].klass = originalClass1;
	
	err = (*jvmti_env)->Allocate(jvmti_env, classBytesSize2, (unsigned char **) &class_bytes1);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Unable to allocate temp buffer for the class file");
		return JNI_FALSE;
	}
	
	(*jni_env)->GetByteArrayRegion(jni_env, classBytes2, 0, classBytesSize2, class_bytes1); 

	classdef[1].class_bytes = (unsigned char *) class_bytes1;
	classdef[1].class_byte_count = classBytesSize2;
	classdef[1].klass = originalClass2;

	/* Redefine both classes in a single call. */
    err = (*jvmti_env)->RedefineClasses(jvmti_env, 2, classdef);
   
    (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) class_bytes0);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "RedefineClasses failed");
    	return JNI_FALSE;
    }
    (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) class_bytes1);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "RedefineClasses failed");
    	return JNI_FALSE;
    }
   
	return JNI_TRUE;
}
