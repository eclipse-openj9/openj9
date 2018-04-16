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
gomi001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);                                
	jvmtiCapabilities capabilities;
	jvmtiError err = JVMTI_ERROR_NONE;

	env = agent_env;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_get_owned_monitor_info = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "gomi001 failed to add capabilities");
		return JNI_ERR;
	}				
			
	return JNI_OK;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_getOwnedMonitorInfo_ThreadMonitorInfoTest_verifyMonitors(
		JNIEnv *jni_env, 
		jclass klass, 
		jthread thread,
		jobject monitorA,
		jobject monitorB)
{
	JVMTI_ACCESS_FROM_AGENT(env);
    jvmtiError err = JVMTI_ERROR_NONE;
    jint monitorCount = 0;
    jobject *monitors = NULL;
    
    
    err = (*jvmti_env)->GetOwnedMonitorInfo(jvmti_env, thread, &monitorCount, &monitors);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "gomi001 failed to GetOwnedMonitorInfo");
        return JNI_FALSE;
    }

    err = JVMTI_ERROR_INTERNAL;

    if (NULL == monitorB) {
        if (monitorCount != 1) {
        	error(env, err, "Incorrect count of owned monitors. Expected 1 got [%d]",  monitorCount);
            return JNI_FALSE;
        }
    } else {
    	if (monitorCount != 2) {
    		error(env, err, "Incorrect count of owned monitors. Expected 2 got [%d]", monitorCount);
    		return JNI_FALSE;
    	}

    }

    if ((*jni_env)->IsSameObject(jni_env, monitors[0], monitorA) != JNI_TRUE) {
    	error(env, err, "The returned monitor object does not match the expected one (monitorA)");
    	return JNI_FALSE;
    }

    if (NULL != monitorB) {
    	if ((*jni_env)->IsSameObject(jni_env, monitors[1], monitorB) != JNI_TRUE) {
    		error(env, err, "The returned monitor object does not match the expected one (monitorB)");
    		return JNI_FALSE;
    	}
    }

    return JNI_TRUE;
}

