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
gtgc001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);                                

	env = agent_env;
			
	return JNI_OK;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_getThreadGroupChildren_gtgc001_checkAssignment(JNIEnv *jni_env, 
		jclass klass, 
		jthreadGroup group, 
		jint expectedThreadCount, 
		jint expectedGroupCount, 
		jstring groupName) 
{
	JVMTI_ACCESS_FROM_AGENT(env);
    jvmtiError err;
    
    jint thread_count_ptr;
    jthread* threads_ptr;
    jint group_count_ptr;
    jthreadGroup* groups_ptr;

    const char *name;
    jboolean isCopy;
    
    name = (const char *) (*jni_env)->GetStringUTFChars(jni_env, groupName, &isCopy);
       
    
    err = (*jvmti_env)->GetThreadGroupChildren(jvmti_env, group,
    										&thread_count_ptr, &threads_ptr,
    										&group_count_ptr, &groups_ptr);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "Failed to GetThreadGroupChildren");
        return JNI_FALSE;
    }
    
    if (thread_count_ptr != expectedThreadCount) {
    	error(env, err, "Wrong [%s] group thread count. Expected %d got %d", 
    			name, expectedThreadCount, thread_count_ptr);
    	return JNI_FALSE;
    }
    
    if (group_count_ptr != expectedGroupCount) {
    	error(env, err, "Wrong [%s] group thread group count. Expected %d got %d", 
    			name, expectedGroupCount, group_count_ptr);
    	return JNI_FALSE;
    }
     
    return JNI_TRUE;
}
