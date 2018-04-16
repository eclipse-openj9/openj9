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
jvmtiExtensionFunction queryVmDump = NULL;
jvmtiExtensionFunction setVmDump = NULL;
jvmtiExtensionFunction resetVmDump = NULL;

jint JNICALL
vmd001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jint extensionCount;
	jvmtiExtensionFunctionInfo* extensionFunctions;
	int err;
	int i;

	env = agent_env;

	err = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensionFunctions);
	if (JVMTI_ERROR_NONE != err) {
		return JNI_ERR;
	}

	for (i = 0; i < extensionCount; i++) {
		if (strcmp(extensionFunctions[i].id, COM_IBM_QUERY_VM_DUMP) == 0) {
			queryVmDump = extensionFunctions[i].func;
		}
		if (strcmp(extensionFunctions[i].id, COM_IBM_SET_VM_DUMP) == 0) {
			setVmDump = extensionFunctions[i].func;
		}
		if (strcmp(extensionFunctions[i].id, COM_IBM_RESET_VM_DUMP) == 0) {
			resetVmDump = extensionFunctions[i].func;
		}
	}

	if (NULL == queryVmDump) {
		error(env, JVMTI_ERROR_NOT_FOUND, "vmd001:queryVmDump extension was not found");
		return FALSE;
	}

	if (NULL == setVmDump) {
		error(env, JVMTI_ERROR_NOT_FOUND, "vmd001:setVmDump extension was not found");
		return FALSE;
	}

	if (NULL == resetVmDump) {
		error(env, JVMTI_ERROR_NOT_FOUND, "vmd001:resetVmDump extension was not found");
		return FALSE;
	}

	err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)extensionFunctions);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "vmd001:Failed to Deallocate extension functions");
		return FALSE;
	}

	return JNI_OK;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_vmDump_vmd001_tryQueryDumpSmallBuffer(JNIEnv* jni_env, jclass cls)
{
	int err;
	jvmtiEnv* jvmti_env = env->jvmtiEnv;

	int data_size = 0;
	char* buffer = NULL;
	int buffer_size = 0;

	err = (resetVmDump)(jvmti_env);
	err = (queryVmDump)(jvmti_env, buffer_size, buffer, &data_size);
	if (err == 0) {
		error(env, err, "tryQueryDumpSmallBuffer:queryVmDump returned success when it should have failed");
		return FALSE;
	} else {
		/* need to allocate the correct amount of memory and try again */
		buffer = (char*)malloc(data_size);
		if (buffer) {
			err = (queryVmDump)(jvmti_env, data_size, buffer, &data_size);
			free(buffer);
			if (err != 0) {
				/* error occurred on second attempt */
				error(env, err, "tryQueryDumpSmallBuffer:queryVmDump failed to return data on second attempt");
				return FALSE;
			}
		}
	}

	return TRUE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_vmDump_vmd001_tryQueryDumpBigBuffer(JNIEnv* jni_env, jclass cls)
{
	int err;
	jvmtiEnv* jvmti_env = env->jvmtiEnv;

	int data_size = 0;
	int buffer_size = 4096;
	char buffer[4096];

	err = (queryVmDump)(jvmti_env, buffer_size, buffer, &data_size);
	if (err != 0) {
		error(env, err, "tryQueryDumpBigBuffer:queryVmDump failed to return data");
		return FALSE;
	}

	return TRUE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_vmDump_vmd001_tryQueryDumpInvalidBufferSize(JNIEnv* jni_env, jclass cls)
{
	int err;
	jvmtiEnv* jvmti_env = env->jvmtiEnv;

	int data_size = 0;
	char buffer[4096];

	err = (queryVmDump)(jvmti_env, 0, buffer, &data_size);
	if (err == 0) {
		error(env, err, "tryQueryDumpInvalidBufferSize:queryVmDump failed to detect 0 length buffer");
		return FALSE;
	} else if (err != JVMTI_ERROR_ILLEGAL_ARGUMENT){
		error(env, err, "tryQueryDumpInvalidBufferSize:queryVmDump failed to return JVMTI_ERROR_ILLEGAL_ARGUMENT");
		return FALSE;
	}

	return TRUE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_vmDump_vmd001_tryQueryDumpInvalidBuffer(JNIEnv* jni_env, jclass cls)
{
	int err;
	jvmtiEnv* jvmti_env = env->jvmtiEnv;

	int data_size = 0;

	err = (queryVmDump)(jvmti_env, 4096, NULL, &data_size);
	if (err == 0) {
		error(env, err, "tryQueryDumpInvalidBuffer:queryVmDump failed to detect null buffer");
		return FALSE;
	} else if (err != JVMTI_ERROR_NULL_POINTER){
		error(env, err, "tryQueryDumpInvalidBuffer:queryVmDump failed to return JVMTI_ERROR_NULL_POINTER");
		return FALSE;
	}

	return TRUE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_vmDump_vmd001_tryQueryDumpInvalidDataSize(JNIEnv* jni_env, jclass cls)
{
	int err;
	jvmtiEnv* jvmti_env = env->jvmtiEnv;

	int buffer_size = 4096;
	char buffer[4096];

	err = (queryVmDump)(jvmti_env, buffer_size, buffer, NULL);
	if (err == 0) {
		error(env, err, "tryQueryDumpInvalidDataSize:queryVmDump failed to detect null data size");
		return FALSE;
	} else if (err != JVMTI_ERROR_NULL_POINTER){
		error(env, err, "tryQueryDumpInvalidDataSize:queryVmDump failed to return JVMTI_ERROR_NULL_POINTER");
		return FALSE;
	}

	return TRUE;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_vmDump_vmd001_trySetVmDump(JNIEnv* jni_env, jclass cls)
{
	int err;
	jvmtiEnv* jvmti_env = env->jvmtiEnv;

	int data_size = 0;
	int buffer_size = 4096;
	char buffer[4096];
	char* optend;
	char* test;

	err = (queryVmDump)(jvmti_env, buffer_size, buffer, &data_size);
	if (err != 0) {
		error(env, err, "trySetVmDump:queryVmDump failed to return data");
		return FALSE;
	}

#if defined(J9ZOS390)
	/* alter the %uid.JVM. within the label:
	 * -Xdump:system:events=gpf+user+abort+traceassert+corruptcache,label=%uid.JVM.%job.D%y%m%d.T%H%M%S.X&DS,range=1..0,priority=999,request=serial
	 */
	test = strstr(buffer, "%uid.JVM.");

#else /* defined(J9ZOS390) */
	/* alter the %pid.%seq insert within the label:
	 * system:events=gpf+abort+traceassert,label=C:\2012\jre\bin\core.%Y%m%d.%H%M%S.%pid.%seq.dmp,range=1..0,priority=999,request=serial
	 */
	test = strstr(buffer, "%pid.%seq");
#endif /* defined(J9ZOS390) */

	optend = strstr(buffer, "\n");
	/* null terminate the system agent details */
	if (NULL == optend) {
		/* no data returned, so error */
		error(env, err, "trySetVmDump:queryVmDump failed to find newline in query data");
		return FALSE;
	}
	*optend = 0x00;

	if (NULL == test) {
#if defined(J9ZOS390)
		error(env, err, "trySetVmDump:queryVmDump failed to find string \"%%uid.JVM.\" in query data");
#else /* defined(J9ZOS390) */
		error(env, err, "trySetVmDump:queryVmDump failed to find string \"%%pid.%%seq\" in query data");
#endif /* defined(J9ZOS390) */
		return FALSE;
	}

	/* change %pid.%seq or %uid.JVM. to testset!! */
	*test++ = 't';
	*test++ = 'e';
	*test++ = 's';
	*test++ = 't';
	*test++ = 's';
	*test++ = 'e';
	*test++ = 't';
	*test++ = '!';
	*test++ = '!';

	err = (setVmDump)(jvmti_env, buffer);
	if (err != 0) {
		error(env, err, "trySetVmDump:setVmDump failed to set data");
		return FALSE;
	}

	err = (queryVmDump)(jvmti_env, buffer_size, buffer, &data_size);
	if (err != 0) {
		error(env, err, "trySetVmDump:queryVmDump failed to return data");
		return FALSE;
	}

	test = strstr(buffer, "testset!!");
	if (NULL == test) {
		error(env, err, "trySetVmDump:queryVmDump data does not match the data that was set with setVmDump");
		return FALSE;
	}

	return TRUE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_vmDump_vmd001_tryResetVmDump(JNIEnv* jni_env, jclass cls)
{
	int err;
	jvmtiEnv* jvmti_env = env->jvmtiEnv;

	char* optend;
	char* test;
	int data_size = 0;
	int buffer1_size = 4096;
	char buffer1[4096];
	int buffer2_size = 4096;
	char buffer2[4096];

	/* alter the %pid insert within the label */
	/* system:events=gpf+abort+traceassert,label=C:\2012\jre\bin\core.%Y%m%d.%H%M%S.%pid.%seq.dmp,range=1..0,priority=999,request=serial */

	err = (queryVmDump)(jvmti_env, buffer1_size, buffer1, &data_size);
	if (err != 0) {
		error(env, err, "tryResetVmDump:queryVmDump failed to return data");
		return FALSE;
	}

	memcpy(buffer2, buffer1, sizeof(buffer1));

	/* now change one of the options returned and fire it back into the JVM */
	optend = strstr(buffer2, "\n");

	/* null terminate the system agent details */
	if (NULL == optend) {
		/* no data returned, so error */
		error(env, err, "tryResetVmDump:queryVmDump failed to find newline in query data");
		return FALSE;
	}

#if defined(J9ZOS390)
	/* alter the %uid.JVM. within the label:
	 * -Xdump:system:events=gpf+user+abort+traceassert+corruptcache,label=%uid.JVM.%job.D%y%m%d.T%H%M%S.X&DS,range=1..0,priority=999,request=serial
	 */
	test = strstr(buffer2, "%uid.JVM.");

#else /* defined(J9ZOS390) */
	/* alter the %pid.%seq insert within the label:
	 * system:events=gpf+abort+traceassert,label=C:\2012\jre\bin\core.%Y%m%d.%H%M%S.%pid.%seq.dmp,range=1..0,priority=999,request=serial
	 */
	test = strstr(buffer2, "%pid.%seq");
#endif /* defined(J9ZOS390) */

	if (NULL == test) {
#if defined(J9ZOS390)
		error(env, err, "tryResetVmDump:queryVmDump failed to find string \"%%uid.JVM.\" in query data");
#else /* defined(J9ZOS390) */
		error(env, err, "tryResetVmDump:queryVmDump failed to find string \"%%pid.%%seq\" in query data");
#endif /* defined(J9ZOS390) */
		return FALSE;
	}

	*optend = 0x00;

	/* change %pid.%seq or %uid.JVM. to testreset */
	*test++ = 't';
	*test++ = 'e';
	*test++ = 's';
	*test++ = 't';
	*test++ = 'r';
	*test++ = 'e';
	*test++ = 's';
	*test++ = 'e';
	*test++ = 't';

	err = (setVmDump)(jvmti_env, buffer2);
	if (err != 0) {
		error(env, err, "tryResetVmDump:setVmDump failed to set data");
		return FALSE;
	}

	err = (resetVmDump)(jvmti_env);
	if (err != 0) {
		error(env, err, "tryResetVmDump:resetVmDump failed to reset data");
		return FALSE;
	}

	err = (queryVmDump)(jvmti_env, buffer2_size, buffer2, &data_size);
	if (err != 0) {
		error(env, err, "tryResetVmDump:queryVmDump failed to return data");
		return FALSE;
	}

	if (strcmp(buffer1, buffer2) != 0) {
		error(env, err, "tryResetVmDump:resetVmDump data does not match the data that was in use before setVmDump");
		return FALSE;
	}

	return TRUE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_vmDump_vmd001_tryDisableVmDump(JNIEnv* jni_env, jclass cls)
{
	int err;
	jvmtiEnv* jvmti_env = env->jvmtiEnv;

	int data_size = 0;
	int buffer_size = 4096;
	char buffer[4096];
	char* optend;
	int initialEventCount = 0;
	int resetEventCount = 0;
	int noGpfEventCount = 0;
	const char * newOption = "system:events=systhrow,filter=EYECATCHER";
	const char * newOptionOff = "system:none:events=systhrow,filter=EYECATCHER";
	const char * sysGpfOff = "system:none:events=gpf";
	const char * javaGpfOff = "java:none:events=gpf";
	const char * snapGpfOff = "snap:none:events=gpf";
	const char * jitGpfOff = "jit:none:events=gpf";

	/* Get the initial set of vm dump options. */
	err = (queryVmDump)(jvmti_env, buffer_size, buffer, &data_size);
	if (err != 0) {
		error(env, err, "tryDisableVmDump:queryVmDump failed to return data");
		return FALSE;
	}

	/* Count the number of agents setup by looking for the count of events= strings */
	optend = strstr(buffer, "events=");
	while( optend != NULL ) {
		optend = optend + strlen("events=");
		initialEventCount++;
		optend = strstr(optend, "events=");
	}

	/* Try setting and disabling a specific dump agent, leaving all others enabled. */

	err = (setVmDump)(jvmti_env, newOption);
	if (err != 0) {
		error(env, err, "tryDisableVmDump:setVmDump failed to set data");
		return FALSE;
	}

	/* Get the updated set of vm dump options. */
	err = (queryVmDump)(jvmti_env, buffer_size, buffer, &data_size);
	if (err != 0) {
		error(env, err, "tryDisableVmDump:queryVmDump failed to return data");
		return FALSE;
	}
	if( strstr(buffer, "EYECATCHER") == NULL ) {
		error(env, err, "tryDisableVmDump:failed to set new option");
		return FALSE;
	}

	err = (setVmDump)(jvmti_env, newOptionOff);
	if (err != 0) {
		error(env, err, "tryDisableVmDump:setVmDump failed to set data");
		return FALSE;
	}

	/* Get the updated set of vm dump options. */
	err = (queryVmDump)(jvmti_env, buffer_size, buffer, &data_size);
	if (err != 0) {
		error(env, err, "tryDisableVmDump:queryVmDump failed to return data");
		return FALSE;
	}
	if( strstr(buffer, "EYECATCHER") != NULL ) {
		error(env, err, "tryDisableVmDump:failed to delete new option");
		return FALSE;
	}
	/* Count the number of agents setup by looking for the count of events= strings */
	optend = strstr(buffer, "events=");
	while( optend != NULL ) {
		optend = optend + strlen("events=");
		resetEventCount++;
		optend = strstr(optend, "events=");
	}
	if( resetEventCount != initialEventCount ) {
		error(env, err, "tryDisableVmDump:deleted more options than expected");
		return FALSE;
	}

	/* Now try turning off all dumps on a certain event. */

	err = (setVmDump)(jvmti_env, sysGpfOff);
	if (err != 0) {
		error(env, err, "tryDisableVmDump:setVmDump failed to set data");
		return FALSE;
	}
	err = (setVmDump)(jvmti_env, javaGpfOff);
	if (err != 0) {
		error(env, err, "tryDisableVmDump:setVmDump failed to set data");
		return FALSE;
	}
	err = (setVmDump)(jvmti_env, snapGpfOff);
	if (err != 0) {
		error(env, err, "tryDisableVmDump:setVmDump failed to set data");
		return FALSE;
	}
	err = (setVmDump)(jvmti_env, jitGpfOff);
	if (err != 0) {
		error(env, err, "tryDisableVmDump:setVmDump failed to set data");
		return FALSE;
	}
	
	/* Get the updated set of vm dump options. */
	err = (queryVmDump)(jvmti_env, buffer_size, buffer, &data_size);
	if (err != 0) {
		error(env, err, "tryDisableVmDump:queryVmDump failed to return data");
		return FALSE;
	}

	/* Check all gpf's off */
	if( strstr(buffer, "gpf") != NULL ) {
		error(env, err, "tryDisableVmDump:failed to remove dumps on gpf");
		return FALSE;
	}

	/* Check we still have the same number of agents as none should have been set to only fire on gpf. */
	optend = strstr(buffer, "events=");
	while( optend != NULL ) {
		optend = optend + strlen("events=");
		noGpfEventCount++;
		optend = strstr(optend, "events=");
	}

	if( noGpfEventCount != initialEventCount ) {
		error(env, err, "tryDisableVmDump:deleted agents unexpectedly");
		return FALSE;
	}

	return TRUE;
}
