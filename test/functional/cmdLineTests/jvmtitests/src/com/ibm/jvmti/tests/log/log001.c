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
#include <stdlib.h>

#include "jvmti_test.h"
#include "ibmjvmti.h"

static agentEnv * env;
jvmtiExtensionFunction queryLogOptions = NULL;
jvmtiExtensionFunction setLogOptions = NULL;

jint JNICALL
log001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jint extensionCount;
	jvmtiExtensionFunctionInfo* extensionFunctions;
	int err;
	int i;

	env = agent_env;

	err = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensionFunctions);
	if (JVMTI_ERROR_NONE != err) {
		error(env, JVMTI_ERROR_NOT_FOUND, "log001:JVMTI extension functions could not be loaded");
		return JNI_ERR;
	}

	for (i = 0; i < extensionCount; i++) {
		if (strcmp(extensionFunctions[i].id, COM_IBM_QUERY_VM_LOG_OPTIONS) == 0) {
			queryLogOptions = extensionFunctions[i].func;
		}
		if (strcmp(extensionFunctions[i].id, COM_IBM_SET_VM_LOG_OPTIONS) == 0) {
			setLogOptions = extensionFunctions[i].func;
		}
	}

	if (NULL == queryLogOptions) {
		error(env, JVMTI_ERROR_NOT_FOUND, "log001:queryLogOptions extension was not found");
		return FALSE;
	}

	if (NULL == setLogOptions) {
		error(env, JVMTI_ERROR_NOT_FOUND, "log001:setLogOptions extension was not found");
		return FALSE;
	}

	err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)extensionFunctions);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "log001:Failed to Deallocate extension functions");
		return FALSE;
	}

	return JNI_OK;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_log_log001_tryQueryLogOptions(JNIEnv* jni_env, jclass cls)
{
	int err;
	jvmtiEnv* jvmti_env = env->jvmtiEnv;

	int data_size = 0;
	char buffer[30];

	printf("in Java_com_ibm_jvmti_tests_log_log001_tryQueryLogOptions\n");

	strcpy(buffer, "");
	err = (queryLogOptions)(jvmti_env, 0, NULL, NULL);
	if (err != JVMTI_ERROR_NULL_POINTER) {
		error(env, err, "tryQueryLogOptions:queryLogOptions(jvmti_env, 0, NULL, NULL) failed to return JVMTI_ERROR_NULL_POINTER");
		return FALSE;
	}

	strcpy(buffer, "");
	err = (queryLogOptions)(jvmti_env, 20, buffer, NULL);
	if (err != JVMTI_ERROR_NULL_POINTER) {
		error(env, err, "tryQueryLogOptions:queryLogOptions(jvmti_env, 20, buffer, NULL) failed to return JVMTI_ERROR_NULL_POINTER");
		return FALSE;
	}

	err = (setLogOptions)(jvmti_env, "warn,error");
	strcpy(buffer, "");
	err = (queryLogOptions)(jvmti_env, 20, buffer, &data_size);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "tryQueryLogOptions:queryLogOptions(jvmti_env, 20, buffer, &data_size) failed to return JVMTI_ERROR_NONE");
		return FALSE;
	}
	if (strcmp(buffer, "error,warn") != 0) {
		error(env, err, "tryQueryLogOptions:queryLogOptions(jvmti_env, sizeof(buffer), buffer, &data_size) returned invalid result string. Expecting '%s' got '%s'","error,warn",buffer);
		return FALSE;
	}

	err = (setLogOptions)(jvmti_env, "all");
	if (err == JVMTI_ERROR_NONE) {
		strcpy(buffer, "");
		err = (queryLogOptions)(jvmti_env, sizeof(buffer), buffer, &data_size);
		if (err != JVMTI_ERROR_NONE) {
			error(env, err, "tryQueryLogOptions:queryLogOptions(jvmti_env, sizeof(buffer), buffer, &data_size) failed to return JVMTI_ERROR_NONE");
			return FALSE;
		}
		if (strcmp(buffer, "error,warn,info,config,vital") != 0) {
			error(env, err, "tryQueryLogOptions:queryLogOptions(jvmti_env, sizeof(buffer), buffer, &data_size) returned invalid result string. Expecting '%s' got '%s'","error,warn,info,config,vital",buffer);
			return FALSE;
		}
	} else {
		error(env, err, "tryQueryLogOptions:queryLogOptions(jvmti_env, sizeof(buffer), buffer, &data_size) failed to return JVMTI_ERROR_NONE");
		return FALSE;
	}

	err = (setLogOptions)(jvmti_env, "none");
	if (err == JVMTI_ERROR_NONE) {
		err = (setLogOptions)(jvmti_env, "error");
		if (err == JVMTI_ERROR_NONE) {
			strcpy(buffer, "");
			err = (queryLogOptions)(jvmti_env, sizeof(buffer), buffer, &data_size);
			if (err != JVMTI_ERROR_NONE) {
				error(env, err, "tryQueryLogOptions:queryLogOptions(jvmti_env, sizeof(buffer), buffer, &data_size) failed to return JVMTI_ERROR_NONE");
				return FALSE;
			}
			if (strcmp(buffer, "error") != 0) {
				error(env, err, "tryQueryLogOptions:queryLogOptions(jvmti_env, sizeof(buffer), buffer, &data_size) returned invalid result string");
				return FALSE;
			}
		} else {
			error(env, err, "tryQueryLogOptions:queryLogOptions(jvmti_env, sizeof(buffer), buffer, &data_size) failed to return JVMTI_ERROR_NONE");
			return FALSE;
		}
	} else {
		error(env, err, "tryQueryLogOptions:queryLogOptions(jvmti_env, sizeof(buffer), buffer, &data_size) failed to return JVMTI_ERROR_NONE");
		return FALSE;
	}

	strcpy(buffer, "");
	err = (queryLogOptions)(jvmti_env, sizeof(buffer)+ 2, NULL, &data_size);
	if (err != JVMTI_ERROR_NULL_POINTER) {
		error(env, err, "tryQueryLogOptions:queryLogOptions(jvmti_env, sizeof(buffer)+ 2, NULL, &data_size) failed to return JVMTI_ERROR_NULL_POINTER");
		return FALSE;
	}

	strcpy(buffer, "");
	err = (queryLogOptions)(jvmti_env, 2, buffer, &data_size);
	if (err != JVMTI_ERROR_ILLEGAL_ARGUMENT) {
		error(env, err, "tryQueryLogOptions:queryLogOptions(jvmti_env, 2, buffer, &data_size) failed to return JVMTI_ERROR_ILLEGAL_ARGUMENT");
		return FALSE;
	}

	return TRUE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_log_log001_trySetLogOptions(JNIEnv* jni_env, jclass cls)
{
	int err = 0;
	jvmtiEnv* jvmti_env = env->jvmtiEnv;

	printf("in Java_com_ibm_jvmti_tests_log_log001_trySetLogOptions\n");

	err = (setLogOptions)(jvmti_env, NULL);
	if (err != JVMTI_ERROR_NULL_POINTER) {
		error(env, err, "trySetLogOptions:setLogOptions(NULL) failed to return JVMTI_ERROR_NULL_POINTER");
		return FALSE;
	}

	err = (setLogOptions)(jvmti_env, "");
	if (err != JVMTI_ERROR_ILLEGAL_ARGUMENT) {
		error(env, err, "trySetLogOptions:setLogOptions(\"\") failed to return JVMTI_ERROR_ILLEGAL_ARGUMENT");
		return FALSE;
	}

	err = (setLogOptions)(jvmti_env, "none");
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "trySetLogOptions:setLogOptions(\"none\") failed to return JVMTI_ERROR_NONE");
		return FALSE;
	}

	err = (setLogOptions)(jvmti_env, "all");
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "trySetLogOptions:setLogOptions(\"all\") failed to return JVMTI_ERROR_NONE");
		return FALSE;
	}

	err = (setLogOptions)(jvmti_env, "none,all");
	if (err != JVMTI_ERROR_ILLEGAL_ARGUMENT) {
		error(env, err, "trySetLogOptions:setLogOptions(\"none,all\") failed to return JVMTI_ERROR_ILLEGAL_ARGUMENT");
		return FALSE;
	}

	err = (setLogOptions)(jvmti_env, "cake");
	if (err != JVMTI_ERROR_ILLEGAL_ARGUMENT) {
		error(env, err, "trySetLogOptions:setLogOptions(\"cake\") failed to return JVMTI_ERROR_ILLEGAL_ARGUMENT");
		return FALSE;
	}

	err = (setLogOptions)(jvmti_env, "ferror     ,config");
	if (err != JVMTI_ERROR_ILLEGAL_ARGUMENT) {
		error(env, err, "trySetLogOptions:setLogOptions(\"ferror     ,config\") failed to return JVMTI_ERROR_ILLEGAL_ARGUMENT");
		return FALSE;
	}

	err = (setLogOptions)(jvmti_env, "error     ,   config   ");
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "trySetLogOptions:setLogOptions(\"error     ,   config   \") failed to return JVMTI_ERROR_NONE");
		return FALSE;
	}

	err = (setLogOptions)(jvmti_env, "  error,info");
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "trySetLogOptions:setLogOptions(\"  error,info\") failed to return JVMTI_ERROR_NONE");
		return FALSE;
	}

	err = (setLogOptions)(jvmti_env, "help");
	if (err != JVMTI_ERROR_ILLEGAL_ARGUMENT) {
		error(env, err, "trySetLogOptions:setLogOptions(\"help\") failed to return JVMTI_ERROR_ILLEGAL_ARGUMENT");
		return FALSE;
	}

	err = (setLogOptions)(jvmti_env, "help,error");
	if (err != JVMTI_ERROR_ILLEGAL_ARGUMENT) {
		error(env, err, "trySetLogOptions:setLogOptions(\"help,error\") failed to return JVMTI_ERROR_ILLEGAL_ARGUMENT");
		return FALSE;
	}

	err = (setLogOptions)(jvmti_env, "none,error");
	if (err != JVMTI_ERROR_ILLEGAL_ARGUMENT) {
		error(env, err, "trySetLogOptions:setLogOptions(\"none,error\") failed to return JVMTI_ERROR_ILLEGAL_ARGUMENT");
		return FALSE;
	}

	err = (setLogOptions)(jvmti_env, "error,none");
	if (err != JVMTI_ERROR_ILLEGAL_ARGUMENT) {
		error(env, err, "trySetLogOptions:setLogOptions(\"error,none\") failed to return JVMTI_ERROR_ILLEGAL_ARGUMENT");
		return FALSE;
	}

	err = (setLogOptions)(jvmti_env, "error,all");
	if (err != JVMTI_ERROR_ILLEGAL_ARGUMENT) {
		error(env, err, "trySetLogOptions:setLogOptions(\"error,all\") failed to return JVMTI_ERROR_ILLEGAL_ARGUMENT");
		return FALSE;
	}

	return TRUE;
}
