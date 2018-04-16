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
#include "ibmjvmti.h"


static agentEnv * env;
static jvmtiExtensionFunction getMethodAndClassNames = NULL;


jint JNICALL
gmcpn001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	                                		
	jint                         extensionCount;
	jvmtiExtensionFunctionInfo	*extensionFunctions;
	int                          i;
	jvmtiError 					 err;
		
	env = agent_env;

	err = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensionFunctions);
	if ( JVMTI_ERROR_NONE != err ) {
		error(env, err, "Failed GetExtensionFunctions");
		return JNI_FALSE;
	}

	for (i = 0; i < extensionCount; i++) {
		if (strcmp(extensionFunctions[i].id, COM_IBM_GET_METHOD_AND_CLASS_NAMES) == 0) {
			getMethodAndClassNames = extensionFunctions[i].func;
		} 
	}

	err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) extensionFunctions);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to Deallocate extension functions");
		return JNI_FALSE;
	}
	
	return JNI_OK;
}



jboolean
check_single(JNIEnv *jni_env, jclass cls)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err = JVMTI_ERROR_NONE;
	jvmtiError deallocErr = JVMTI_ERROR_NONE;
	UDATA * ramMethods = NULL;
	UDATA   ramMethodsCount = 1;
	UDATA ** ramMethodsPtr = NULL;
	UDATA * descriptorBuffer = NULL;
	jchar * stringBuffer = NULL;
	jint stringBufferSize = 1024;
	jmethodID mid1;
	jvmtiExtensionRamMethodData * descriptors;
	
	

	err = (*jvmti_env)->Allocate(jvmti_env, sizeof(UDATA) * ramMethodsCount, (unsigned char **) &ramMethods);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to Allocate ramMethods");
		return JNI_FALSE;		
	}
	
	err = (*jvmti_env)->Allocate(jvmti_env, sizeof(jvmtiExtensionRamMethodData) * ramMethodsCount, (unsigned char **) &descriptorBuffer);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to Allocate descriptorBuffer");
		return JNI_FALSE;		
	}
	
	err = (*jvmti_env)->Allocate(jvmti_env, stringBufferSize, (unsigned char **) &stringBuffer);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to Allocate descriptorBuffer");
		return JNI_FALSE;		
	}

	ramMethodsPtr = (UDATA **) ramMethods;

	mid1 = (*jni_env)->GetMethodID(jni_env, cls, "singleMethod1","()V");
	*((UDATA *) ramMethodsPtr++) =  *((UDATA *) mid1);
	 	
	err = getMethodAndClassNames(jvmti_env, 
			(void *) ramMethods, 
			ramMethodsCount, 
			descriptorBuffer,
			stringBuffer, 
			&stringBufferSize);  

	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "getMethodAndClassNames failed");
		goto done;
	}
	
	descriptors = (jvmtiExtensionRamMethodData *) descriptorBuffer;
	
	if (descriptors->reasonCode != JVMTI_ERROR_NONE) {
		error(env, JVMTI_ERROR_ILLEGAL_ARGUMENT, "Unexpected reason code (jvmti err): %d", descriptors->reasonCode);
		goto done;				
	}
	
	if (descriptors->className == NULL) {
		error(env, JVMTI_ERROR_ILLEGAL_ARGUMENT, "Invalid className [NULL]");
		goto done;		
	}
	
	if (descriptors->methodName == NULL) {
		error(env, JVMTI_ERROR_ILLEGAL_ARGUMENT, "Invalid methodName [NULL]");
		goto done;		
	}
	
	if (strcmp("com/ibm/jvmti/tests/getMethodAndClassNames/gmcpn001", (char *) descriptors->className)) {
		error(env, JVMTI_ERROR_ILLEGAL_ARGUMENT, "Invalid className [%s]", descriptors->className);
		goto done;
	}

	if (strcmp("singleMethod1()V", (char *)  descriptors->methodName)) {
		error(env, JVMTI_ERROR_ILLEGAL_ARGUMENT, "Invalid methodName [%s]", descriptors->methodName);
		goto done;
	}
				
done:

	if (descriptorBuffer) {
		deallocErr = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*) descriptorBuffer);
		if (deallocErr != JVMTI_ERROR_NONE) {
			error(env, deallocErr, "Failed to Deallocate descriptorBuffer");
		}
	}

	if (stringBuffer) {
		deallocErr = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*) stringBuffer);
		if (deallocErr != JVMTI_ERROR_NONE) {
			error(env, deallocErr, "Failed to Deallocate stringBuffer");
		}
	}

	if (ramMethods) {
		deallocErr = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*) ramMethods);
		if (deallocErr != JVMTI_ERROR_NONE) {
			error(env, deallocErr, "Failed to Deallocate ramMethods");
		}
	}
	
	if (err == JVMTI_ERROR_NONE) {
		return JNI_TRUE;
	} 
	
	return JNI_FALSE;
}


jboolean
check_multiple(JNIEnv *jni_env, jclass cls)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err = JVMTI_ERROR_NONE;
	jvmtiError deallocErr = JVMTI_ERROR_NONE;
	UDATA * ramMethods = NULL;
	UDATA   ramMethodsCount = 3;
	UDATA ** ramMethodsPtr = NULL;
	UDATA * descriptorBuffer = NULL;
	jchar * stringBuffer = NULL;
	jint stringBufferSize = 16 * 1024;
	jmethodID mid1, mid2, mid3;
	jvmtiExtensionRamMethodData * descriptors;
	int i;
	

	err = (*jvmti_env)->Allocate(jvmti_env, sizeof(UDATA) * ramMethodsCount, (unsigned char **) &ramMethods);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to Allocate ramMethods");
		return JNI_FALSE;		
	}
	
	err = (*jvmti_env)->Allocate(jvmti_env, sizeof(jvmtiExtensionRamMethodData) * ramMethodsCount, (unsigned char **) &descriptorBuffer);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to Allocate descriptorBuffer");
		return JNI_FALSE;		
	}
	
	err = (*jvmti_env)->Allocate(jvmti_env, stringBufferSize, (unsigned char **) &stringBuffer);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to Allocate descriptorBuffer");
		return JNI_FALSE;		
	}

	ramMethodsPtr = (UDATA **) ramMethods;

	mid1 = (*jni_env)->GetMethodID(jni_env, cls, "singleMethod1","()V");
	*((UDATA *) ramMethodsPtr++) =  *((UDATA *) mid1);
	 
	mid2 = (*jni_env)->GetMethodID(jni_env, cls, "singleMethod2","()V");
	*((UDATA *) ramMethodsPtr++) =  *((UDATA *) mid2);
	
	mid3 = (*jni_env)->GetMethodID(jni_env, cls, "singleMethod3","()V");
	*((UDATA *) ramMethodsPtr++) =  *((UDATA *) mid3);
	
	err = getMethodAndClassNames(jvmti_env, 
			(void *) ramMethods, 
			ramMethodsCount, 
			descriptorBuffer,
			stringBuffer, 
			&stringBufferSize);  

	
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "getMethodAndClassNames failed");
		goto done;
	}
	
	descriptors = (jvmtiExtensionRamMethodData *) descriptorBuffer;
	
	for (i = 1; i <= 3; i++) {
		char buf[128];
		
		if (descriptors->reasonCode != JVMTI_ERROR_NONE) {
			error(env, JVMTI_ERROR_ILLEGAL_ARGUMENT, "Unexpected reason code (jvmti err): %d", descriptors->reasonCode);
			goto done;				
		}
		
		if (descriptors->className == NULL) {
			error(env, JVMTI_ERROR_ILLEGAL_ARGUMENT, "Invalid className [NULL]");
			goto done;		
		}
		
		if (descriptors->methodName == NULL) {
			error(env, JVMTI_ERROR_ILLEGAL_ARGUMENT, "Invalid methodName [NULL]");
			goto done;		
		}
		
			
		if (strcmp("com/ibm/jvmti/tests/getMethodAndClassNames/gmcpn001", (char *) descriptors->className)) {
			error(env, JVMTI_ERROR_ILLEGAL_ARGUMENT, "Invalid className [%s] expected: [%s]", 
					descriptors->className, buf);
			goto done;
		}
	
		sprintf(buf, "singleMethod%d()V", i);
		if (strcmp(buf, (char *) descriptors->methodName)) {
			error(env, JVMTI_ERROR_ILLEGAL_ARGUMENT, "Invalid methodName [%s] expected: [%s]", 
					descriptors->methodName, buf);
			goto done;
		}
		
		descriptors = (jvmtiExtensionRamMethodData *) (((char *) descriptors) + sizeof(jvmtiExtensionRamMethodData));
	}
				
		
done:

	if (descriptorBuffer) {
		deallocErr = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*) descriptorBuffer);
		if (deallocErr != JVMTI_ERROR_NONE) {
			error(env, deallocErr, "Failed to Deallocate descriptorBuffer");
		}
	}

	if (stringBuffer) {
		deallocErr = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*) stringBuffer);
		if (deallocErr != JVMTI_ERROR_NONE) {
			error(env, deallocErr, "Failed to Deallocate stringBuffer");
		}
	}

	if (ramMethods) {
		deallocErr = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*) ramMethods);
		if (deallocErr != JVMTI_ERROR_NONE) {
			error(env, deallocErr, "Failed to Deallocate ramMethods");
		}
	}
	

	if (err == JVMTI_ERROR_NONE) {
		return JNI_TRUE;
	} 
	
	return JNI_FALSE;
}

jboolean
check_invalid_single(JNIEnv *jni_env, jclass cls)
{
	return JNI_FALSE;
}

typedef enum {
	type_single = 1,
	type_multiple = 2,
	type_invalid_single = 3	
} check_type_t;

jboolean JNICALL
Java_com_ibm_jvmti_tests_getMethodAndClassNames_gmcpn001_check(JNIEnv *jni_env, jclass cls, jint check_type)
{
	switch(check_type) {
		case type_single:
			return check_single(jni_env, cls);
			
		case type_multiple:
			return check_multiple(jni_env, cls);
			
		case type_invalid_single:
			return check_invalid_single(jni_env, cls);
			
		default:
			error(env, JVMTI_ERROR_ILLEGAL_ARGUMENT, "unknown check_type %d", check_type);
			return JNI_FALSE;
	
	}
	
	return JNI_FALSE;
}
