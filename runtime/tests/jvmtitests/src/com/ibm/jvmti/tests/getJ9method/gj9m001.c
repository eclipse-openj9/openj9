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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ibmjvmti.h"
#include "jvmti_test.h"
#include "j9.h"

static jvmtiExtensionFunction getJ9method = NULL;
static agentEnv * env;

jboolean JNICALL
Java_com_ibm_jvmti_tests_getJ9method_gj9m001_validateGJ9M001(JNIEnv * jni_env, jclass clazz) {

	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err = JVMTI_ERROR_NONE;
	J9Method *j9method_ptr;
	jthread thread_ptr = NULL;
	jboolean rc = JNI_TRUE;
	jclass clazz_arg;
	jmethodID mid_arg;

	if (NULL == getJ9method) {
		fprintf(stderr,"gj9m001: getJ9method is NULL");
		return JNI_FALSE;
	}

	clazz_arg = (*jni_env)->FindClass(jni_env, "java/lang/Thread");
	if (clazz_arg == NULL) {
		fprintf(stderr,"gj9m001: Cannot find java/lang/Thread");
		return JNI_FALSE;
	}

	mid_arg = (*jni_env)->GetStaticMethodID(jni_env, clazz_arg, "currentThread", "()Ljava/lang/Thread;");
	if (mid_arg == NULL) {
		fprintf(stderr,"gj9m001: Cannot find mid for currentThread()");
		return JNI_FALSE;
	}

	fprintf(stderr, "Validating getJ9method works with good parameter\n");
	err = getJ9method(jvmti_env, mid_arg, &j9method_ptr);
	if ( JVMTI_ERROR_NONE == err ) {
		if (((J9JNIMethodID *)mid_arg)->method == j9method_ptr) {
			fprintf(stderr, "\tgetJ9method successfully returned J9Method %p\n", j9method_ptr);
		} else {
			fprintf(stderr, "\tgetJ9method failed to returned J9Method %p\n", j9method_ptr);
			rc = JNI_FALSE;
		}
	}

	fprintf(stderr, "Validating getJ9method fails with NULL jmid\n");
	err = getJ9method(jvmti_env, NULL, &j9method_ptr);
	if ( JVMTI_ERROR_NONE != err ) {
		fprintf(stderr, "\tgetJ9method successfully detected NULL jmid\n");
	} else {
		fprintf(stderr, "\tgetJ9method failed to detect NULL jmid\n");
		rc = JNI_FALSE;
	}

	fprintf(stderr, "Validating getJ9method works with NULL return argument\n");
	err = getJ9method(jvmti_env, mid_arg, NULL);
	if ( JVMTI_ERROR_NONE != err ) {
		fprintf(stderr, "\tgetJ9method successfully detected NULL return parameter\n");
	} else {
		fprintf(stderr, "\tgetJ9method failed to detect NULL return parameter\n");
		rc = JNI_FALSE;
	}

	return rc;
}

jint JNICALL
gj9m001(agentEnv * agent_env, char * args)
{
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
		if (0 == strcmp(extensionFunctions[i].id, COM_IBM_GET_J9METHOD)) {
			getJ9method = extensionFunctions[i].func;
		}
	}

	err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)extensionFunctions);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to Deallocate extension functions");
		return FALSE;
	}

	if (getJ9method == NULL) {
		error(env, JVMTI_ERROR_NOT_FOUND, "getJ9method extension functions were not found");
		return FALSE;
	}

	return JNI_OK;
}
