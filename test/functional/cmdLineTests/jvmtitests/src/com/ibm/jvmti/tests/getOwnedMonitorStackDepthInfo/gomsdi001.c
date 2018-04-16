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

jint JNICALL
gomsdi001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);                                
	jvmtiCapabilities capabilities;
	jvmtiError err;                                

	env = agent_env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_get_owned_monitor_stack_depth_info = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add capabilities");
		return JNI_ERR;
	}				
			
	return JNI_OK;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_getOwnedMonitorStackDepthInfo_ThreadNDepth_verify(
		JNIEnv *jni_env, 
		jclass klass, 
		jthread thread,		
		jint expectedMonitorCount,
		jint expectedDepth,
		jobject monitorA) 
{
	JVMTI_ACCESS_FROM_AGENT(env);
    jvmtiError err;
    jint infoCount;
    jvmtiMonitorStackDepthInfo *info;
    
    
    err = (*jvmti_env)->GetOwnedMonitorStackDepthInfo(jvmti_env, thread, &infoCount, &info);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "Failed to GetOwnedMonitorStackDepthInfo");
        return JNI_FALSE;
    }
      
    err = JVMTI_ERROR_INTERNAL;
    
    if (infoCount != expectedMonitorCount) {
    	error(env, err, "Incorrect count of owned monitors. Expected %d got [%d]", expectedMonitorCount, infoCount);
        return JNI_FALSE;
    }
    
    if (info[0].stack_depth != expectedDepth) {
    	error(env, err, "Incorrect stack depth owned monitor. Expected %d got [%d]", expectedDepth, info[0].stack_depth);
        return JNI_FALSE;    	
    }
    
    
    if ((*jni_env)->IsSameObject(jni_env, info[0].monitor, monitorA) != JNI_TRUE) {
    	error(env, err, "The returned monitor object does not match the expected one");
    	return JNI_FALSE;
    }
    
    return JNI_TRUE;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_getOwnedMonitorStackDepthInfo_ThreadNDepth_verifyDeeper(
		JNIEnv *jni_env, 
		jclass klass, 
		jthread thread,
		jint expectedMonitorCount,
		jint expectedDepthA,
		jint expectedDepthB,
		jobject monitorA,
		jobject monitorB) 
{
	JVMTI_ACCESS_FROM_AGENT(env);
    jvmtiError err;
    jint infoCount;
    jvmtiMonitorStackDepthInfo *info;
    
    
    err = (*jvmti_env)->GetOwnedMonitorStackDepthInfo(jvmti_env, thread, &infoCount, &info);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "Failed to GetOwnedMonitorStackDepthInfo");
        return JNI_FALSE;
    }
      
    if (infoCount != expectedMonitorCount) {
    	error(env, err, "Incorrect count of owned monitors. Expected %d got [%d]", expectedMonitorCount, infoCount);
        return JNI_FALSE;
    }
    
    if (info[0].stack_depth != expectedDepthA) {
    	error(env, err, "Incorrect stack depth owned monitorA. Expected %d got [%d]", expectedDepthA, info[0].stack_depth);
        return JNI_FALSE;    	
    }

    if (info[1].stack_depth != expectedDepthB) {
    	error(env, err, "Incorrect stack depth owned monitorB. Expected %d got [%d]", expectedDepthB, info[1].stack_depth);
        return JNI_FALSE;    	
    }
            
    if ((*jni_env)->IsSameObject(jni_env, info[1].monitor, monitorA) != JNI_TRUE) {
    	error(env, err, "The returned monitor object does not match the expected one (monitorA)");
    	return JNI_FALSE;
    }
    
    if ((*jni_env)->IsSameObject(jni_env, info[0].monitor, monitorB) != JNI_TRUE) {
    	error(env, err, "The returned monitor object does not match the expected one (monitorB)");
    	return JNI_FALSE;
    }
    
    
    return JNI_TRUE;
}



jboolean JNICALL
Java_com_ibm_jvmti_tests_getOwnedMonitorStackDepthInfo_gomsdi001_nDepth(
		JNIEnv *jni_env, 
		jclass klass, 
		jthread thread,
		jint expectedMonitorCount,
		jint expectedDepth) 
{
	JVMTI_ACCESS_FROM_AGENT(env);
    jvmtiError err;
    jint infoCount;
    jvmtiMonitorStackDepthInfo *info;
    
    
    err = (*jvmti_env)->GetOwnedMonitorStackDepthInfo(jvmti_env, thread, &infoCount, &info);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "Failed to GetOwnedMonitorStackDepthInfo");
        return JNI_FALSE;
    }

    if (infoCount != expectedMonitorCount) {
    	error(env, err, "Incorrect count of owned monitors. Expected %d got [%d]", expectedMonitorCount, infoCount);
        return JNI_FALSE;
    }
    
    if (info[0].stack_depth != expectedDepth) {
    	error(env, err, "Incorrect stack depth owned monitor. Expected 2 got [%d]", info[0].stack_depth);
        return JNI_FALSE;    	
    }
        
    return JNI_TRUE;
}
