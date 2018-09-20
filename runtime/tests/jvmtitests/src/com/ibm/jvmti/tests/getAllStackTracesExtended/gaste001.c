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

#include "ibmjvmti.h"
#include "jvmti_test.h"

static agentEnv * env;                                                    

static void JNICALL
compiledMethodLoad(jvmtiEnv *jvmti_env, jmethodID method, jint code_size, const void* code_addr,
					jint map_length, const jvmtiAddrLocationMap* map, const void* compile_info);
 
static jboolean testMethodJitted = JNI_FALSE;


jint JNICALL
gaste001(agentEnv * agent_env, char * args)
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



#define JVMTI_TEST_GASTE001_MAX_FRAMES 50

jvmtiExtensionFunction   getAllStackTracesExtended = NULL;

static jint testGetAllStackTracesExtended(JavaVM * vm, jvmtiEnv * jvmti_env, jint high)
{
	int          err, i, th;
	int          num_threads = 0;
	int          num_frames  = 0;
	int          num_jitted  = 0;
	jint         rc          = JNI_OK;
	
	jint                        extensionCount;
	jvmtiExtensionFunctionInfo *extensionFunctions;
	jvmtiStackInfoExtended     *psiExt;
	
	if (getAllStackTracesExtended == NULL) {
		/* First call, let's set it */
		err = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensionFunctions);
		if ( JVMTI_ERROR_NONE != err ) {
			error(env, err, "Failed GetExtensionFunctions");
			return JNI_ERR;
		}
	
		for (i = 0; i < extensionCount; i++) {
			if (strcmp(extensionFunctions[i].id, COM_IBM_GET_ALL_STACK_TRACES_EXTENDED) == 0) {
				getAllStackTracesExtended = extensionFunctions[i].func;
			}			
		}
		
		err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)extensionFunctions);
		if (err != JVMTI_ERROR_NONE) {
			error(env, err, "Failed to Deallocate extension functions");
			return JNI_ERR;
		}
		
		if (getAllStackTracesExtended == NULL) {
			error(env, JVMTI_ERROR_NOT_FOUND, "GetAllStackTracesExtended extension was not found");
			return JNI_ERR;
		}
	}
				
	err = (getAllStackTracesExtended)(jvmti_env,
									  COM_IBM_GET_STACK_TRACE_EXTRA_FRAME_INFO,
                                 	  JVMTI_TEST_GASTE001_MAX_FRAMES,
                                 	  &psiExt,
                                 	  &num_threads);
	if (err != JVMTI_ERROR_NONE) {
		if (err == JVMTI_ERROR_WRONG_PHASE) {  /* Most likely called at Agent_OnLoad, just return */			
			return JNI_OK;
		}		
		error(env, err, "getAllStackTracesExtended failed");
		rc = JNI_ERR;
	} else {
		/* Count the number of jitted frames */
		
		for ( th = 0; th < num_threads; th++ ) {
			num_frames += psiExt[th].frame_count;
			for ( i = 0; i < psiExt[th].frame_count; i++ ) {
				if (psiExt[th].frame_buffer[i].type != COM_IBM_STACK_FRAME_EXTENDED_NOT_JITTED)
					++num_jitted;       
			}
        }                     
        (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *)psiExt);
	  
		if ( high && (num_jitted == 0) ) {
			rc = JNI_ERR; 
			error(env, 
			      JVMTI_ERROR_INTERNAL, 
			      "getAllStackTracesExtended: Error when high iteration count");
		}		   
	}	
	return(rc);
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_getAllStackTracesExtended_gaste001_anyJittedFrame(JNIEnv * jni_env, jclass clazz, jint high)
{
	jvmtiEnv * jvmti_env = env->jvmtiEnv;
	
	if (testMethodJitted == JNI_FALSE) {
		return 2;	
	}
	
	if (testGetAllStackTracesExtended(NULL, jvmti_env, high) == JNI_OK)
		return 1;
	else
		return 0;	
}
