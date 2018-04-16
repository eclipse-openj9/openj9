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
gcvn001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	char * jar = agent_env->testArgs;

	env = agent_env; 

	if (!ensureVersion(agent_env, JVMTI_VERSION_1_1)) {
		return JNI_ERR;
	}   	
       
	printf("jar: [%s]\n", jar);       
       
	if (jar == NULL) {
		error(agent_env, JVMTI_ERROR_NONE, "Must specify getClassVersions.jar path and name in args");
		return JNI_ERR;
	}

	return JNI_OK;
}



jboolean JNICALL
Java_com_ibm_jvmti_tests_getClassVersionNumbers_gcvn001_checkVersionNumbers(JNIEnv * jni_env, jobject foo, jint minor, jint major)
{
	jvmtiError err;
	JVMTI_ACCESS_FROM_AGENT(env);
	jint major_ptr, minor_ptr;
	jclass klass;
	char * jar = env->testArgs;   

	/* add the jar file specified via ,args:/path/getClassVersion.jar to the CP */
	err = (*jvmti_env)->AddToSystemClassLoaderSearch(jvmti_env, jar);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add \"%s\" to the classpath", jar);
		return JNI_FALSE;
	}		
		
	klass = (*jni_env)->FindClass(jni_env, "com/ibm/jvmti/tests/getClassVersionNumbers/VersionedClass/VersionedClass");
	if (klass == NULL) {
		error(env, JVMTI_ERROR_INTERNAL, "Unable to load com/ibm/jvmti/tests/getClassVersionNumbers/VersionedClass/VersionedClass");
		return JNI_FALSE;
	} 
	
	err = (*jvmti_env)->GetClassVersionNumbers(jvmti_env, klass, &minor_ptr, &major_ptr);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to GetClassVersionNumbers");
		return JNI_FALSE;
	} 
	
	if (minor_ptr != minor || major_ptr != major) {
		return JNI_FALSE;
	}
		
	return JNI_TRUE;
}

