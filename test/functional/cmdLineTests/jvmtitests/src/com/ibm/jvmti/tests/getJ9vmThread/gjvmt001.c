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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ibmjvmti.h"
#include "jvmti_test.h"
#include "j9.h"


static jvmtiExtensionFunction getJ9vmThread  = NULL;
static agentEnv * env;

jboolean JNICALL
Java_com_ibm_jvmti_tests_getJ9vmThread_gjvmt001_validateGJVMT001(JNIEnv * jni_env, jclass clazz){

	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err = JVMTI_ERROR_NONE;
	J9VMThread *j9vmthread_ptr;
	jthread thread_ptr = NULL;
	jboolean rc = JNI_FALSE;

	if (NULL == getJ9vmThread)	{
		fprintf(stderr,"gjvmt001: getJ9vmThread is NULL");
		return JNI_FALSE;
	}

	fprintf(stderr, "Validating getj9vmThread works with good parameter\n");
	thread_ptr = getCurrentThread(jni_env);
	err = getJ9vmThread(jvmti_env, thread_ptr, &j9vmthread_ptr);
	if ( JVMTI_ERROR_NONE == err ) {
		j9object_t threadObject = *((j9object_t*) thread_ptr);

		if (j9vmthread_ptr->threadObject == threadObject) {
			fprintf(stderr, "\tgetJ9vmThread successfully returned good J9VMThread %p\n", j9vmthread_ptr);
			rc = JNI_TRUE;
		} else {
			fprintf(stderr, "\tgetJ9vmThread fails to returned good J9VMThread %p\n", j9vmthread_ptr);
			rc = JNI_FALSE;
		}
	}

	fprintf(stderr, "Validating getj9vmThread fails with NULL jthread\n");
	err = getJ9vmThread(jvmti_env, NULL, &j9vmthread_ptr);
	if ( JVMTI_ERROR_NONE != err )	{
		fprintf(stderr, "\tgetJ9vmThread successfully detected NULL pointer\n");
		rc = JNI_TRUE;
	}

	fprintf(stderr, "Validating getj9vmThread works with NULL return parameter\n");
	err = getJ9vmThread(jvmti_env, thread_ptr, NULL);
	if ( JVMTI_ERROR_NONE != err )	{
		fprintf(stderr, "\tgetJ9vmThread successfully detected NULL return parameter\n");
		rc = JNI_TRUE;
	}


	return rc;
}

jint JNICALL
gjvmt001(agentEnv * agent_env, char * args){
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jvmtiError err;
	jint extensionCount;
	jvmtiExtensionFunctionInfo *extensionFunctions;
	int i;


	env = agent_env;

	err = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensionFunctions);
	if ( JVMTI_ERROR_NONE != err ) {
		error(env, err, "Failed GetExtensionFunctions");
		return JNI_ERR;
	}

	for (i = 0; i < extensionCount; i++) {
		if (0 == strcmp(extensionFunctions[i].id, COM_IBM_GET_J9VMTHREAD)) {
			getJ9vmThread = extensionFunctions[i].func;
		}
	}

	err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)extensionFunctions);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to Deallocate extension functions");
		return FALSE;
	}

	if (getJ9vmThread == NULL) {
		error(env, JVMTI_ERROR_NOT_FOUND, "getJ9vmThread extension functions were not found");
		return FALSE;
	}

	return JNI_OK;
}
