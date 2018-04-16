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
#include "fer001.h"

static agentEnv * _agentEnv;

jint JNICALL
fer003(agentEnv * env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiCapabilities capabilities;
	jvmtiError err;

	if (!ensureVersion(env, JVMTI_VERSION_1_1)) {
		return JNI_ERR;
	}

	_agentEnv = env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_force_early_return = 1;
	capabilities.can_suspend = 1;
	capabilities.can_access_local_variables = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add capabilities");
		return JNI_ERR;
	}

	return JNI_OK;
}





jboolean JNICALL
Java_com_ibm_jvmti_tests_forceEarlyReturn_fer003_forceEarlyReturn(JNIEnv * jni_env, jclass klass, jthread thread, jint returnType, jobject testObject)
{
	jvmtiError err;
	agentEnv * env = _agentEnv;
	JVMTI_ACCESS_FROM_AGENT(env);
	jboolean ret;

	err = (*jvmti_env)->SuspendThread(jvmti_env, thread);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to suspend current thread");
		return JNI_FALSE;
	}

	switch (returnType) {

		case JVMTI_TYPE_JINT:
			err = (*jvmti_env)->ForceEarlyReturnInt(jvmti_env, thread, 1000);
			break;

		case JVMTI_TYPE_JFLOAT:
			err = (*jvmti_env)->ForceEarlyReturnFloat(jvmti_env, thread, 1000.0);
			break;

		case JVMTI_TYPE_JLONG:
			err = (*jvmti_env)->ForceEarlyReturnLong(jvmti_env, thread, 100020003000LL);
			break;

		case JVMTI_TYPE_JDOUBLE:
			err = (*jvmti_env)->ForceEarlyReturnDouble(jvmti_env, thread, 100020003000.0);
			break;

		case JVMTI_TYPE_JOBJECT:
			err = (*jvmti_env)->ForceEarlyReturnObject(jvmti_env, thread, testObject);
			break;

		case JVMTI_TYPE_CVOID:
			err = (*jvmti_env)->ForceEarlyReturnVoid(jvmti_env, thread);
			break;

		default:
			err = JVMTI_ERROR_ILLEGAL_ARGUMENT;
			break;
	}

	ret = JNI_TRUE;

	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to force return for type %d", returnType);
		ret = JNI_FALSE;
	}

	err = (*jvmti_env)->ResumeThread(jvmti_env, thread);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to resume current thread");
		ret = JNI_FALSE;
	}

	return ret;
}








