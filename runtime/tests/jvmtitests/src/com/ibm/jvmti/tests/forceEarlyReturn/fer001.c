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
 
static void JNICALL testForceEarlyReturn_methodEntry(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method);
static char * JNICALL testForceEarlyReturn_getMethodName(agentEnv * env, jmethodID method);
static testForceEarlyReturn_testTrigger * JNICALL testForceEarlyReturn_triggerName(const char *methodName);
static jobject JNICALL testForceEarlyReturn_getTestObject(agentEnv * env, JNIEnv * jni_env, jthread thread); 
static jboolean JNICALL testForceEarlyReturn_isSuspendable(const char *methodName);
static jvmtiError JNICALL testForceEarlyReturn_waitForTestThread(agentEnv * env, jthread thread, jmethodID method, jthread *suspendedThread);



jint JNICALL
fer001(agentEnv * env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiEventCallbacks callbacks;
	jvmtiCapabilities capabilities;
	jvmtiError err;                                

	if (!ensureVersion(env, JVMTI_VERSION_1_1)) {
		return JNI_ERR;
	}   
       
	_agentEnv = env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_force_early_return = 1;
	capabilities.can_generate_method_entry_events = 1;
	capabilities.can_suspend = 1;
	capabilities.can_access_local_variables = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add capabilities");
		return JNI_ERR;
	}						
		 
	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
 	callbacks.MethodEntry = testForceEarlyReturn_methodEntry;
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to set callbacks");
		return JNI_ERR;		
	}

	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_METHOD_ENTRY, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to enable method entry");
		return JNI_ERR;
	}

	return JNI_OK;
}


#define JVMTI_TEST_FER_CLASS_NAME "Lcom/ibm/jvmti/tests/forceEarlyReturn/TestForceEarlyReturn"

static void JNICALL 
testForceEarlyReturn_methodEntry(jvmtiEnv * jvmti_env, JNIEnv * jni_env, jthread thread, jmethodID method)
{
	jvmtiError err;
	testForceEarlyReturn_testTrigger *trigger;
	char *methName = NULL;
	char *threadName = NULL;
	char *suspThreadName = NULL;
	agentEnv *env = _agentEnv;

	methName = testForceEarlyReturn_getMethodName(env, method);

	if (strncmp(JVMTI_TEST_FER_CLASS_NAME, methName, strlen(JVMTI_TEST_FER_CLASS_NAME))) {
		goto done;
	}

	threadName = getThreadName(env, thread, thread);
	tprintf(env, 100, "MethodEntry ENTR (thread %s): %s\n", threadName, methName);
 
	/* Check if the thread running the method should be suspended. We want to suspend the methods that
	 * we wish to force early return on. */
	if (testForceEarlyReturn_isSuspendable(methName) == JNI_TRUE) {
		tprintf(env, 100, "\t%s Suspending  thread running: [%s] [0x%x]\n", threadName, methName, thread);
		
		err = (*jvmti_env)->SuspendThread(jvmti_env, thread);
		
		tprintf(env, 300, "\t%s UnSuspended thread running: [%s] [0x%x]\n", threadName, methName, thread);
		if (err != JVMTI_ERROR_NONE) {
			tprintf(env, 1, "Failed to suspend current thread.\n");
		}
		goto done;
	}
	

	/* The main thread executes a trigger method that we intercept here. Once detected, we wait for Thread-Runner to
	 * be suspended AND subsequently force a return AND unsuspend it */
	trigger = testForceEarlyReturn_triggerName(methName);
	if (trigger) {
		jobject testObject;
		jthread suspendedThread = NULL;

		tprintf(env, 100, "\tMethodEntry (thread %s): %s  -- TRIGGER DETECTED\n", threadName, methName);

		err = testForceEarlyReturn_waitForTestThread(env, thread, method, &suspendedThread);
		if (err != JVMTI_ERROR_NONE) {
			goto done;
		}
                                               
		suspThreadName = getThreadName(env, thread, suspendedThread);
                tprintf(env, 300, "\t%s calling ForceEarlyReturn\n", suspThreadName);

		/* Force the suspended thread to return, we know exactly which method it was supposed to have
		 * been suspended in */
		switch (trigger->returnType) {
			case JVMTI_TYPE_JINT:
				err = (*jvmti_env)->ForceEarlyReturnInt(jvmti_env, suspendedThread, 10002000);
				break;

			case JVMTI_TYPE_JFLOAT:
				err = (*jvmti_env)->ForceEarlyReturnFloat(jvmti_env, suspendedThread, 10002000.0);
				break;

			case JVMTI_TYPE_JLONG:
				err = (*jvmti_env)->ForceEarlyReturnLong(jvmti_env, suspendedThread, 100020003000LL);
				break;

			case JVMTI_TYPE_JDOUBLE:
				err = (*jvmti_env)->ForceEarlyReturnDouble(jvmti_env, suspendedThread, 100020003000.0);
				break;

			case JVMTI_TYPE_JOBJECT:

				testObject = testForceEarlyReturn_getTestObject(env, jni_env, thread);
				if (NULL == testObject) {
					err = JVMTI_ERROR_INTERNAL;
					goto done;
				}
				err = (*jvmti_env)->ForceEarlyReturnObject(jvmti_env, suspendedThread, testObject);
				break;

			case JVMTI_TYPE_CVOID:
				err = (*jvmti_env)->ForceEarlyReturnVoid(jvmti_env, suspendedThread);
				break;

			default:
				err = JVMTI_ERROR_ILLEGAL_ARGUMENT;
				break;
		}

		if (err != JVMTI_ERROR_NONE) {
			error(env, err, "Failed to ForceEarlyReturn");
		}

		tprintf(env, 300, "\tCalling ResumeThread on thread [%s] [0x%x]\n", suspThreadName, suspendedThread);

		/* Resume so we can get on with its processing */
		err = (*jvmti_env)->ResumeThread(jvmti_env, suspendedThread);
		if (err != JVMTI_ERROR_NONE) {
			errorName(env, err, "Failed to ResumeThread");
			goto done;
		} 
	}

	/* tstDumpStackTrace(jvmti_env, thread, jni_env, thread, 0); */
done:
	tprintf(env, 200, "MethodEntry EXIT (thread %s): %s\n", threadName, methName);

	if (methName)
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) methName);
 
	if (threadName)
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) threadName);
 
	if (suspThreadName)
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) suspThreadName);
 
	return;
}


static jboolean JNICALL 
testForceEarlyReturn_isSuspendable(const char *methodName)
{
	int idx = 0;
	static const char *methodNames[] = {
		"Lcom/ibm/jvmti/tests/forceEarlyReturn/TestForceEarlyReturnRunner;.earlyReturnInt(II)I",
		"Lcom/ibm/jvmti/tests/forceEarlyReturn/TestForceEarlyReturnRunner;.earlyReturnLong(II)J",
		"Lcom/ibm/jvmti/tests/forceEarlyReturn/TestForceEarlyReturnRunner;.earlyReturnFloat(II)F",
		"Lcom/ibm/jvmti/tests/forceEarlyReturn/TestForceEarlyReturnRunner;.earlyReturnDouble(II)D",
		"Lcom/ibm/jvmti/tests/forceEarlyReturn/TestForceEarlyReturnRunner;.earlyReturnObject(II)Lcom/ibm/jvmti/tests/forceEarlyReturn/TestForceEarlyReturnRunner;",
		"Lcom/ibm/jvmti/tests/forceEarlyReturn/TestForceEarlyReturnRunner;.earlyReturnVoid(II)V",
		NULL
	};

        while (methodNames[idx]) {
		if (strcmp(methodNames[idx++], methodName) == 0) {
			return JNI_TRUE;
		}
	}

	return JNI_FALSE;
}

static const testForceEarlyReturn_testTrigger triggerNames[] = {
	{ "Lcom/ibm/jvmti/tests/forceEarlyReturn/TestForceEarlyReturnTrigger;.triggerForceEarlyReturnInt()V", JVMTI_TYPE_JINT },
	{ "Lcom/ibm/jvmti/tests/forceEarlyReturn/TestForceEarlyReturnTrigger;.triggerForceEarlyReturnLong()V", JVMTI_TYPE_JLONG },
	{ "Lcom/ibm/jvmti/tests/forceEarlyReturn/TestForceEarlyReturnTrigger;.triggerForceEarlyReturnFloat()V", JVMTI_TYPE_JFLOAT },
	{ "Lcom/ibm/jvmti/tests/forceEarlyReturn/TestForceEarlyReturnTrigger;.triggerForceEarlyReturnDouble()V", JVMTI_TYPE_JDOUBLE },
	{ "Lcom/ibm/jvmti/tests/forceEarlyReturn/TestForceEarlyReturnTrigger;.triggerForceEarlyReturnObject()V", JVMTI_TYPE_JOBJECT },
	{ "Lcom/ibm/jvmti/tests/forceEarlyReturn/TestForceEarlyReturnTrigger;.triggerForceEarlyReturnVoid()V", JVMTI_TYPE_CVOID },
	{ NULL, 0 }
};

static testForceEarlyReturn_testTrigger * JNICALL
testForceEarlyReturn_triggerName(const char *methodName)
{
	int idx = 0;
        
        while (triggerNames[idx].methodName) {
		if (strcmp(triggerNames[idx].methodName, methodName) == 0) {
			return (testForceEarlyReturn_testTrigger *) &triggerNames[idx];
		}
		idx++;
	}

	return NULL; 
}


static jvmtiError JNICALL 
testForceEarlyReturn_waitForTestThread(agentEnv * env, jthread thread, jmethodID method, jthread *suspendedThread)
{
	jvmtiError err;
	jthread * threads;
	jint threadCount;
	jint i;
	jint itterations = 1000000;
	JVMTI_ACCESS_FROM_AGENT(env);

	*suspendedThread = NULL;

	/* Get a list of all threads */
	err = (*jvmti_env)->GetAllThreads(jvmti_env, &threadCount, &threads);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to GetAllThreads while searching for the suspended thread");
		return err;
	}

	/* Look for the suspended thread */
	while (NULL == *suspendedThread && itterations > 0) {
		for (i = 0; i < threadCount; ++i) {
			jint state;
			char *thrName;

			err = (*jvmti_env)->GetThreadState(jvmti_env, threads[i], &state);
			if (err != JVMTI_ERROR_NONE) {
				error(env, err, "Failed to GetThreadState");
				return err;
			}

			thrName = getThreadName(env, thread, threads[i]);

			tprintf(env, 300, "\t%-10s state 0x%x\n", thrName, state);

			if (state & JVMTI_THREAD_STATE_SUSPENDED && 
			    !strcmp(thrName, "Thread-Runner")) {
				unsigned char * suspendedThreadName;
				*suspendedThread = threads[i];
				suspendedThreadName = (unsigned char *)getThreadName(env, thread, *suspendedThread),
				tprintf(env, 300, 
					  "\t%-10s state 0x%x thr 0x%x susThr 0x%p i%d is SUSPENDED. returning\n", 
					  suspendedThreadName, state, thread, threads[i], i);
				(*jvmti_env)->Deallocate(jvmti_env, suspendedThreadName);
				return JVMTI_ERROR_NONE;
			}

			(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) thrName);
		}
		itterations--;
	}

	return JVMTI_ERROR_NONE;
}


static jobject JNICALL
testForceEarlyReturn_getTestObject(agentEnv * env, JNIEnv* jni_env, jthread thread)
{
 	jclass testClass;
	jobject testObject;
	jmethodID methodId; 

 	testClass = (*jni_env)->FindClass(jni_env, "com/ibm/jvmti/tests/forceEarlyReturn/TestForceEarlyReturnRunner");
	if (testClass == NULL) {
		(*jni_env)->ExceptionClear(jni_env);
		tprintf(env, 1, "!!! Failed to get testClass\n");
		return NULL;
	} 

	methodId = (*jni_env)->GetMethodID(jni_env, testClass, "<init>", "()V");
	if(methodId == 0) {
		(*jni_env)->ExceptionClear(jni_env);
		tprintf(env, 1, "!!! Failed to GetMethodID\n");
		return NULL;
	}
	
	testObject = (*jni_env)->NewObject(jni_env, testClass, methodId);
	if (testObject == NULL) {
		(*jni_env)->ExceptionClear(jni_env);
		tprintf(env, 1, "!!! Failed trying to call NewObject for TestForceEarlyReturnRunner\n");
		return NULL;
	}

	return testObject;
} 

static char * JNICALL
testForceEarlyReturn_getMethodName(agentEnv * env, jmethodID method)
{
	jvmtiError err;
	jclass declaringClass;
	char * methodName = NULL;
	char * methodSig = NULL;
	char * declaringClassName = NULL;
 	unsigned char *name = NULL;
	JVMTI_ACCESS_FROM_AGENT(env);


	err = (*jvmti_env)->Allocate(jvmti_env, 1024, &name);
	if (err != JVMTI_ERROR_NONE) {
		name = NULL;
		goto done;
	}

	err = (*jvmti_env)->GetMethodDeclaringClass(jvmti_env, method, &declaringClass);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to GetMethodDeclaringClass");
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) name);
		goto done;
	}
         
	err = (*jvmti_env)->GetClassSignature(jvmti_env, declaringClass, &declaringClassName, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to GetClassSignature");
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) name);
		goto done;        
	}

	err = (*jvmti_env)->GetMethodName(jvmti_env, method, &methodName, &methodSig, NULL);
	if (err != JVMTI_ERROR_NONE) {
 		error(env, err, "Failed to GetMethodName");
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) name);
		goto done;                
	}

	strcpy((char *)name, declaringClassName);
	strcat((char *)name, ".");
	strcat((char *)name, methodName);
	strcat((char *)name, methodSig);

done:

	if (methodName) 
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) methodName);

	if (methodSig)
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) methodSig); 

	if (declaringClassName)
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) declaringClassName); 
 

	return (char *)name;
}
