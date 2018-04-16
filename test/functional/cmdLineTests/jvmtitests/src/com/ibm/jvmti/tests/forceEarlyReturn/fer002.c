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
static jmethodID interuptedMethodId = NULL;
static jint methodExitState = JNI_ERR;



jint JNICALL
fer002(agentEnv * env, char * args)
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
	capabilities.can_generate_method_exit_events = 1;
	capabilities.can_suspend = 1;
	capabilities.can_access_local_variables = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add capabilities");
		return JNI_ERR;
	}						
		 


	return JNI_OK;
}

jboolean static
setupFER(JNIEnv * jni_env, jvmtiEventMethodExit methodExitCB, jthread t, jclass c, char * methodName, char * methodSig)
{
	jvmtiError err;
	jvmtiEventCallbacks callbacks;
	agentEnv * env = _agentEnv;	
	JVMTI_ACCESS_FROM_AGENT(env);

	methodExitState = JNI_ERR;
	
	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
 	callbacks.MethodExit = methodExitCB;
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (err != JVMTI_ERROR_NONE) {
		error(_agentEnv, err, "Failed to set callbacks");
		return JNI_ERR;		
	}

	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_METHOD_EXIT, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to enable method exit event");
		return JNI_ERR;
	}

    interuptedMethodId = (*jni_env)->GetMethodID(jni_env, c, methodName, methodSig);
    if (interuptedMethodId == NULL) {
    	error(env, err, "Failed to GetMethodID of the method to be ForcedEarlyReturn on");        
        return JNI_ERR;
    }
	
    err = (*jvmti_env)->SuspendThread(jvmti_env, t);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "Failed to SuspendThread");        
        return JNI_ERR;
    }

	
	return JNI_OK;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_forceEarlyReturn_fer002_methodExitState(JNIEnv * jni_env, jclass klass)
{
	return (methodExitState == JNI_OK) ? JNI_TRUE : JNI_FALSE;
}




static void JNICALL 
testForceEarlyReturn_methodExitInt(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, 
                                   jboolean was_popped_by_exception, jvalue return_value)
{
	if (method == interuptedMethodId) {
		if (return_value.i == 4096) {
			methodExitState = JNI_OK;
		}
	}
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_forceEarlyReturn_fer002_forceEarlyReturnInt(
		JNIEnv * jni_env, jclass klass, jint retVal, jthread t, jclass c)
{
	jvmtiError err;
	agentEnv * env = _agentEnv;	
	JVMTI_ACCESS_FROM_AGENT(env);
			
	if (setupFER(jni_env, testForceEarlyReturn_methodExitInt, t, c, "workInt", "()I") != JNI_OK) {
		return JNI_FALSE;
	}
		
    err = (*jvmti_env)->ForceEarlyReturnInt(jvmti_env, t, retVal);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "Failed to ForceEarlyReturn");        
        return JNI_FALSE;
    }
    
    err = (*jvmti_env)->ResumeThread(jvmti_env, t);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "Failed to ResumeThread"); 
        return JNI_FALSE;
    }

    return JNI_TRUE;
}




static void JNICALL 
testForceEarlyReturn_methodExitObject(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, 
                                   jboolean was_popped_by_exception, jvalue return_value)
{
	agentEnv * env = _agentEnv;
	const char *ferString;
	jboolean isCopy;
	jvmtiError err = JVMTI_ERROR_INTERNAL;
	
	methodExitState = JNI_ERR;
	
	if (method == interuptedMethodId) {
		char * name;
		
        jclass objClass = (*jni_env)->GetObjectClass(jni_env, return_value.l);
        if (objClass == NULL) {
        	error(env, err, "GetObjectClass failed");
        	return;
        }
        
        err = (*jvmti_env)->GetClassSignature(jvmti_env, objClass, &name, NULL);
        if (err == JVMTI_ERROR_NONE) {     
        	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) name);
        } else {
        	error(env, err, "GetClassSignature failed");
        	return;
        }
         
        ferString = (const char *) (*jni_env)->GetStringUTFChars(jni_env, (jstring) return_value.l, &isCopy);
        if (NULL == ferString || strcmp(ferString, "fer")) {
        	error(env, err, "MethodExit event returned an invalid return value [%s].", ferString);
        	return;
        }

        methodExitState = JNI_OK;		
	}
	
	return;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_forceEarlyReturn_fer002_forceEarlyReturnObject(
		JNIEnv * jni_env, jclass klass, jobject retVal, jthread t, jclass c)
{
	jvmtiError err;
	agentEnv * env = _agentEnv;	
	JVMTI_ACCESS_FROM_AGENT(env);
	
	if (setupFER(jni_env, testForceEarlyReturn_methodExitObject, t, c, "workObject", "()Ljava/lang/String;") != JNI_OK) {
		return JNI_FALSE;
	}
	
    err = (*jvmti_env)->ForceEarlyReturnObject(jvmti_env, t, retVal);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "Failed to ForceEarlyReturnObject");        
        return JNI_FALSE;
    }
    
    err = (*jvmti_env)->ResumeThread(jvmti_env, t);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "Failed to ResumeThread"); 
        return JNI_FALSE;
    }

    return JNI_TRUE;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_forceEarlyReturn_fer002_forceEarlyReturnBoolean(
		JNIEnv * jni_env, jclass klass, jobject retVal, jthread t, jclass c)
{
	return JNI_FALSE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_forceEarlyReturn_fer002_forceEarlyReturnChar(
		JNIEnv * jni_env, jclass klass, jobject retVal, jthread t, jclass c)
{
	return JNI_FALSE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_forceEarlyReturn_fer002_forceEarlyReturnShort(
		JNIEnv * jni_env, jclass klass, jobject retVal, jthread t, jclass c)
{
	return JNI_FALSE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_forceEarlyReturn_fer002_forceEarlyReturnLong(
		JNIEnv * jni_env, jclass klass, jobject retVal, jthread t, jclass c)
{
	return JNI_FALSE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_forceEarlyReturn_fer002_forceEarlyReturnDouble(
		JNIEnv * jni_env, jclass klass, jobject retVal, jthread t, jclass c)
{
	return JNI_FALSE;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_forceEarlyReturn_fer002_forceEarlyReturnVoid(
		JNIEnv * jni_env, jclass klass, jobject retVal, jthread t, jclass c)
{
	return JNI_FALSE;
}
