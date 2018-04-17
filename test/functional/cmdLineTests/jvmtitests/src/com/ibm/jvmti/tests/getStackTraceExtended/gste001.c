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

static void JNICALL
compiledMethodLoad(jvmtiEnv *jvmti_env, jmethodID method, jint code_size, const void* code_addr,
					jint map_length, const jvmtiAddrLocationMap* map, const void* compile_info);
 
static jboolean testMethodJitted = JNI_FALSE;                                

jint JNICALL
gste001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jvmtiEventCallbacks callbacks;
	jvmtiCapabilities capabilities;
	jvmtiError err;                                

	env = agent_env;

	testMethodJitted = JNI_FALSE;

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_tag_objects = 1;
	capabilities.can_generate_compiled_method_load_events = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add capabilities");
		return JNI_ERR;
	}						

	/* Set the CompiledMethodLoad event callback */
	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	callbacks.CompiledMethodLoad = compiledMethodLoad;
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to set callback for CompiledMethodLoad events");
		return JNI_ERR;
	}

	/* Enable the CompiledMethodLoad callback */
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_COMPILED_METHOD_LOAD, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to enable CompiledMethodLoad event");
		return JNI_ERR;
	} 

	return JNI_OK;
}




static void JNICALL
compiledMethodLoad(jvmtiEnv *jvmti_env,
				jmethodID method,
            	jint code_size,
            	const void* code_addr,
            	jint map_length,
            	const jvmtiAddrLocationMap* map,
            	const void* compile_info)
{
	jvmtiError err;
	char *name_ptr, *signature_ptr, *generic_ptr;
	
	
		
	err = (*jvmti_env)->GetMethodName(jvmti_env, method, &name_ptr, &signature_ptr, &generic_ptr);
    if (err != JVMTI_ERROR_NONE) {
    	error(env, err, "Failed to GetMethodName");
		return;	
    }       
    
    /* printf("load %s %s\n", name_ptr, signature_ptr); */
    
    if (!strcmp(name_ptr, "myB") && !strcmp(signature_ptr, "()V")) {
    	testMethodJitted = JNI_TRUE;	
    }
            
	return;	
}




#define JVMTI_TEST_GSTE001_MAX_FRAMES 50

jvmtiExtensionFunction   getStackTraceExtended  = NULL;

static jint 
testGetStackTraceExtended(JavaVM * vm, jvmtiEnv * jvmti_env, jint high)
{
	int          err, i;
	int          num_frames = 0;
	int          num_jitted = 0;
	jint         rc         = JNI_OK;

	jvmtiFrameInfoExtended     * frame_buffer   = NULL;
	jthread                      currentThread;
	jint                         extensionCount;
	jvmtiExtensionFunctionInfo * extensionFunctions;
	
	if (getStackTraceExtended == NULL) {
		/* First call, let's set it */
		err = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensionFunctions);
		if ( JVMTI_ERROR_NONE != err ) {
			error(env, err, "Failed GetExtensionFunctions");
			return JNI_ERR;
		}
	
		for (i = 0; i < extensionCount; i++) {
			if (strcmp(extensionFunctions[i].id, COM_IBM_GET_STACK_TRACE_EXTENDED) == 0) {
				getStackTraceExtended = extensionFunctions[i].func;
			}			
		}
		
		err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)extensionFunctions);
		if (err != JVMTI_ERROR_NONE) {
			error(env, err, "Failed to Deallocate extension functions");
			return JNI_ERR;
		}
		
		if (getStackTraceExtended == NULL) {
			error(env, JVMTI_ERROR_NOT_FOUND, "GetStackTraceExtended extension was not found");			
			return JNI_ERR;
		}
	}
	
	err = (*jvmti_env)->GetCurrentThread(jvmti_env, &currentThread);
	if (err != JVMTI_ERROR_NONE) {
		if (err == JVMTI_ERROR_WRONG_PHASE) {  /* Most likely called at Agent_OnLoad, just return */			
			return JNI_OK;
		}
		error(env, err, "GetCurrentThread failed");
		return JNI_ERR;
	}
	
	frame_buffer = (jvmtiFrameInfoExtended *)malloc( JVMTI_TEST_GSTE001_MAX_FRAMES * sizeof( jvmtiFrameInfoExtended ) );

	if (frame_buffer == NULL) {
		error(env, JVMTI_ERROR_OUT_OF_MEMORY, "Failed to allocate array of jvmtiFrameInfo" );
		return JNI_ERR;
	}	
				
	err = (getStackTraceExtended)(jvmti_env,
								 COM_IBM_GET_STACK_TRACE_EXTRA_FRAME_INFO,
                                 currentThread,
                                 0,
                                 JVMTI_TEST_GSTE001_MAX_FRAMES,
                                 (void*)frame_buffer,
                                 &num_frames);
	
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "getStackTraceExtended failed");
		rc = JNI_ERR;
	} else {
		/* Count the number of jitted frames */
		for (i = 0; i < num_frames; ++i) { 
			if (frame_buffer[i].type != COM_IBM_STACK_FRAME_EXTENDED_NOT_JITTED)
				++num_jitted;
		}		
		if (high && (num_jitted == 0))  {
			rc = JNI_ERR;
			error(env, 
			      JVMTI_ERROR_INTERNAL, 
			      "getStackTraceExtended: Error when high iteration count");
		}		   
	}
	
	free(frame_buffer);
	return(rc);
}

jint JNICALL
Java_com_ibm_jvmti_tests_getStackTraceExtended_gste001_anyJittedFrame(JNIEnv * jni_env, jclass clazz, jint high)
{
	jvmtiEnv * jvmti_env = env->jvmtiEnv;
	
	if (testMethodJitted == JNI_FALSE) {
		return 2;	
	}
	
	if (testGetStackTraceExtended(NULL, jvmti_env, high) == JNI_OK)
		return 1;
	else
		return 0;	
}

