/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
snmp001(agentEnv * agent_env, char * args)
{
	jvmtiError err;
	jvmtiCapabilities capabilities;
	JVMTI_ACCESS_FROM_AGENT(agent_env);

	env = agent_env; 

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_set_native_method_prefix = 1;	
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to AddCapabilities");
		return JNI_ERR;
	}						

	return JNI_OK;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_setNativeMethodPrefix_snmp001_setPrefix(JNIEnv * jni_env, jclass klass)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err; 
	jvmtiEnv *disposeEnv;
	JavaVM *vm;
	
	err = (*jvmti_env)->SetNativeMethodPrefix(jvmti_env, "$$J9$$");
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Unable to set prefix");
		return JNI_FALSE;
	}

	if (JNI_OK != (*jni_env)->GetJavaVM(jni_env, &vm)) {
		error(env, err, "Unable to get VM");
		return JNI_FALSE;
	}
	if (JNI_OK != (*vm)->GetEnv(vm, (void **) &disposeEnv, JVMTI_VERSION_1_2)) {
		error(env, err, "Unable to get new JVMTI env");
		return JNI_FALSE;
	}
	err = (*disposeEnv)->DisposeEnvironment(disposeEnv);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Unable dispose JVMTI env");
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

void JNICALL
Java_com_ibm_jvmti_tests_setNativeMethodPrefix_snmp001_nat(JNIEnv * jni_env, jclass klass)
{
}
