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

#include "ibmjvmti.h"
#include "jvmti_test.h"

static agentEnv * env;                                                    

void testThreadState(jint state, jint correctState, jint *stateCount);
void checkThreadState(JNIEnv *jni_env, jthread currThread, jint state, jint *stateCount);

jint JNICALL
gts001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);                                

	env = agent_env;
			
	return JNI_OK;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_getThreadState_gts001_getThreadStates(JNIEnv *jni_env, 
		jclass klass, jint threadStateEnumCount) 
{
	jvmtiError err;
	jthread * threads;
	jint threadCount;
	jint i;
	jint stateCount = 0;
	
	JVMTI_ACCESS_FROM_AGENT(env);

	/* Get all the threads */
	err = (*jvmti_env)->GetAllThreads(jvmti_env, &threadCount, &threads);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to GetAllThreads");
		return err;
	}

	for (i = 0; i < threadCount; ++i) {
		jint state;
		
		/* Get the thread state of a thread */
		err = (*jvmti_env)->GetThreadState(jvmti_env, threads[i], &state);
		if (err != JVMTI_ERROR_NONE) {
			error(env, err, "Failed to GetThreadState");
			return err;
		}

		/* check that the state of this thread is correct */
		checkThreadState(jni_env, threads[i], state, &stateCount);
	}

	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) threads);
	
	/* Fail if number of thread state was incorrect and did not increment count */
	if (threadStateEnumCount !=  stateCount) {
		return JNI_FALSE;
	}
     
    return JNI_TRUE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_getThreadState_gts001_getThreadState(JNIEnv *jni_env,
		jclass klass, jthread currThread)
{
	jvmtiError err;
	jint stateCount = 0;
	jint state;


	JVMTI_ACCESS_FROM_AGENT(env);

	/* Get the thread state of a thread */
	err = (*jvmti_env)->GetThreadState(jvmti_env, currThread, &state);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to GetThreadState");
		return err;
	}

	/* check that the state of the thread is correct */
	checkThreadState(jni_env, currThread, state, &stateCount);

	/* Fail if number of thread state was incorrect and did not increment stateCount */
	if (1 !=  stateCount) {
		return JNI_FALSE;
	}

    return JNI_TRUE;
}

void 
testThreadState(jint state, jint correctState, jint *stateCount)
{
	if (state == correctState) {
		/* Increment count if thread state is correct */
		++(*stateCount);
	} else {
		/* FAILED */
		tprintf(env, 300, "!= %x, FAILED", correctState);
	}
}

void checkThreadState(JNIEnv *jni_env, jthread currThread, jint state, jint *stateCount){

	char *thrName;

	JVMTI_ACCESS_FROM_AGENT(env);

	thrName = getThreadName(env, NULL, currThread);
	tprintf(env, 300, "\n\t%-10s state 0x%x \t", thrName, state);

	/* If the thread name matches one we defined in gts001.java,
	 * test if the thread state is correct.
	 */
	if (!strcmp(thrName, "RUNNING")) {
		testThreadState(state, (JVMTI_THREAD_STATE_RUNNABLE | JVMTI_THREAD_STATE_ALIVE), stateCount);
	} else if (!strcmp(thrName, "BLOCKED")) {
		testThreadState(state, (JVMTI_THREAD_STATE_BLOCKED_ON_MONITOR_ENTER | JVMTI_THREAD_STATE_ALIVE), stateCount);
	} else if (!strcmp(thrName, "WAITING")) {
		testThreadState(state, (JVMTI_THREAD_STATE_IN_OBJECT_WAIT | JVMTI_THREAD_STATE_WAITING_INDEFINITELY | JVMTI_THREAD_STATE_WAITING | JVMTI_THREAD_STATE_ALIVE), stateCount);
	} else if (!strcmp(thrName, "WAITING_TIMED")) {
		testThreadState(state, (JVMTI_THREAD_STATE_IN_OBJECT_WAIT | JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT | JVMTI_THREAD_STATE_WAITING | JVMTI_THREAD_STATE_ALIVE), stateCount);
	} else if (!strcmp(thrName, "PARKED")) {
		testThreadState(state, (JVMTI_THREAD_STATE_PARKED | JVMTI_THREAD_STATE_WAITING_INDEFINITELY | JVMTI_THREAD_STATE_WAITING | JVMTI_THREAD_STATE_ALIVE), stateCount);
	} else if (!strcmp(thrName, "PARKED_TIMED")) {
		testThreadState(state, (JVMTI_THREAD_STATE_PARKED | JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT | JVMTI_THREAD_STATE_WAITING | JVMTI_THREAD_STATE_ALIVE), stateCount);
	} else if (!strcmp(thrName, "SLEEPING")) {
		testThreadState(state, (JVMTI_THREAD_STATE_SLEEPING | JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT | JVMTI_THREAD_STATE_WAITING | JVMTI_THREAD_STATE_ALIVE), stateCount);
	} else if (!strcmp(thrName, "SUSPENDED")) {
		testThreadState(state, (JVMTI_THREAD_STATE_SUSPENDED | JVMTI_THREAD_STATE_RUNNABLE | JVMTI_THREAD_STATE_ALIVE), stateCount);
	} else if (!strcmp(thrName, "INTERRUPTED_BLOCKED")) {
		testThreadState(state, (JVMTI_THREAD_STATE_INTERRUPTED | JVMTI_THREAD_STATE_BLOCKED_ON_MONITOR_ENTER | JVMTI_THREAD_STATE_ALIVE), stateCount);
	} else if (!strcmp(thrName, "INTERRUPTED_SUSPENDED")) {
		testThreadState(state, (JVMTI_THREAD_STATE_INTERRUPTED | JVMTI_THREAD_STATE_SUSPENDED | JVMTI_THREAD_STATE_RUNNABLE | JVMTI_THREAD_STATE_ALIVE), stateCount);
	} else if (!strcmp(thrName, "NEW")) {
		testThreadState(state, 0, stateCount);
	} else if (!strcmp(thrName, "DEAD")) {
		testThreadState(state, (JVMTI_THREAD_STATE_TERMINATED), stateCount);
	}

	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) thrName);
}
