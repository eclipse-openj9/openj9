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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
#include <stdlib.h>
#include <string.h>

#include "ibmjvmti.h"
#include "jvmti_test.h"

static agentEnv * env;
static jboolean   loadedClassArray    = JNI_FALSE;
static jboolean   loadedNonArrayClass = JNI_FALSE;

static void JNICALL cbClassLoad(jvmtiEnv * jvmti, JNIEnv  * env, jthread thread, jclass klass, ...);

jint JNICALL
ecla001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jint                         extensionCount;
	jint                         extensionEventIndex = -1;
	jvmtiExtensionEventInfo    * extensions;
	jvmtiError                   err;
	jint                         i;

	env = agent_env;

   /* Find the ClassLoadIncludingArrays extension event */

	err = (*jvmti_env)->GetExtensionEvents(jvmti_env, &extensionCount, &extensions);
	if ( JVMTI_ERROR_NONE != err ) {
		error(env, err, "Failed GetExtensionEvents");
		return JNI_ERR;
	}

	for (i = 0; i < extensionCount; i++) {
		if (strcmp(extensions[i].id, COM_IBM_ARRAY_CLASS_LOAD) == 0) {
			extensionEventIndex = extensions[i].extension_event_index;
		}
	}

	if (extensionEventIndex == -1) {
		error(env, err, "COM_IBM_CLASS_LOAD_ARRAYS extension event not found");
		return JNI_ERR;
	}

	err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*) extensions);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to Deallocate extension events");
		return JNI_ERR;
	}

	/*  Set the calback */

	err = (*jvmti_env)->SetExtensionEventCallback(jvmti_env, extensionEventIndex, (jvmtiExtensionEvent)cbClassLoad); 
	if ( err != JVMTI_ERROR_NONE ) {
		error(env, err, "Failed SetExtensionEventCallback");
		return JNI_ERR;
	}

	return JNI_OK;
}

static void JNICALL cbClassLoad(jvmtiEnv * jvmti_env, JNIEnv  * jni_env, jthread thread, jclass klass, ...)
{
	jvmtiError   err;
    char       * sig;
	jint         classStatus;

	err = (*jvmti_env)->GetClassSignature(jvmti_env, klass, &sig, 0);;
	if ( err != JVMTI_ERROR_NONE ) {
		error(env, err, "GetClassSignature() failed");
		return;
	}

	err = (*jvmti_env)->GetClassStatus(jvmti_env, klass, &classStatus);
	if ( err != JVMTI_ERROR_NONE ) {
		error(env, err, "GetClassStatus() failed");
	}

	if (classStatus & JVMTI_CLASS_STATUS_ARRAY) {
		loadedClassArray = JNI_TRUE;
		tprintf(env, 1, "Loaded an array class: %s\n", sig);
	} else {
		loadedNonArrayClass = JNI_TRUE;
	}

	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)sig);
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_eventClassLoadArrays_ecla001_checkEventClassLoadArrays(JNIEnv * jni_env, jclass clazz)
{
	if (loadedClassArray == JNI_FALSE) {
		return JNI_FALSE;
	}

	if (loadedNonArrayClass) {
		return JNI_FALSE;
	}

	return JNI_TRUE;
}
