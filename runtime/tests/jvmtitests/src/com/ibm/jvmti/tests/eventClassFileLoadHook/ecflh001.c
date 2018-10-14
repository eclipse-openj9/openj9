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

#define CLASS_NAME_LEN 256

typedef enum ModType {
	NO_MODIFY = 0,
	MODIFY_BOOTSTRAP,
	MODIFY_NON_BOOTSTRAP,
	MODIFY_ALL,
	MOD_NUM_TYPE
} ModType;

static const char *ModTypeStrings[] = {
	"noModify",
	"modifyBootstrap",
	"modifyNonBootstrap",
	"modifyAll",
	"modStringEnd"
};

typedef struct ArgumentSet {
	ModType modType;
	char className[CLASS_NAME_LEN];
	int printClassFileLoadMsg;
} ArgumentSet;

static ArgumentSet argSet;

static agentEnv * env;

static void JNICALL
classFileLoadHook(jvmtiEnv *jvmti_env,
            JNIEnv* jni_env,
            jclass class_being_redefined,
            jobject loader,
            const char* name,
            jobject protection_domain,
            jint class_data_len,
            const unsigned char* class_data,
            jint* new_class_data_len,
            unsigned char** new_class_data)
{
	unsigned char *newClassData;
	int modifyClass = 0;
	unsigned char classMagicBytes[] = { 0xca, 0xfe, 0xba, 0xbe };
	jvmtiError err;

	if (argSet.printClassFileLoadMsg) {
		fprintf(stderr, "ClassFileLoadHook event called for class: %s\n", name);
	}

	/* check for 0xcafebabe magic bytes at the start of class data */
	if ((4 > class_data_len) || (0 != memcmp(class_data, classMagicBytes, 4))){
		fprintf(stderr, "Invalid class file bytes for class: %s\n", name);
		return;
	}

	if (NO_MODIFY == argSet.modType) {
		return;
	} else if (MODIFY_BOOTSTRAP == argSet.modType) {
		if (0 == strcmp(name, argSet.className)) {
			modifyClass = 1;
		}
	} else if (MODIFY_NON_BOOTSTRAP == argSet.modType) {
		if (0 == strcmp(name, argSet.className)) {
			modifyClass = 1;
		}
	} else if (MODIFY_ALL == argSet.modType) {
		modifyClass = 1;
	}
	if (1 == modifyClass) {
		err = (*jvmti_env)->Allocate(jvmti_env, class_data_len, &newClassData);
		if (err != JVMTI_ERROR_NONE) {
			error(env, err, "Failed to allocate memory for new class data");
			return;
		}
		memcpy(newClassData, class_data, class_data_len);

		*new_class_data = newClassData;
		*new_class_data_len = class_data_len;
	}
}

/*
 * Note: Arguments accepted by this agent are -
 * args:<modtype>[+printClassFileLoadMsg]
 * 	where <modtype> is one of the strings in the array ModTypeStrings,
 * 		  and printClassFileLoadMsg prints a message when ClassFileLoadHook event is triggered.
 *
 * For modtype = modifyBootstrap or modifyNonBootstrap, name of class to be modified should be specified, as in
 * 			args:modifyBootstrap=java/lang/Class or args:modifyNonBootstrap=SimpleApp
 * 	If class name is not specified with these options, they are silently ignored.
 */
jint JNICALL
ecflh001(agentEnv * agent_env, char * args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jvmtiEventCallbacks callbacks;
	jvmtiCapabilities capabilities;
	jvmtiError err;
	char *testArgs;
	int i;

	env = agent_env;

	testArgs = agent_env->testArgs;

	/* parse agent arguments if available */
	argSet.modType = -1;
	memset(argSet.className, 0, CLASS_NAME_LEN);
	if (NULL != testArgs) {
		for (i = 0; i < MOD_NUM_TYPE; i++) {
			if (!strncmp(testArgs, ModTypeStrings[i], strlen(ModTypeStrings[i]))) {
				argSet.modType = i;
				if ((MODIFY_BOOTSTRAP == i) || (MODIFY_NON_BOOTSTRAP == i)) {
					/* get the classname to be modified */
					char *classIndex, *nextArgIndex;
					int classNameLength;
					classIndex = strchr(testArgs, '=');
					if (NULL != classIndex) {
						nextArgIndex = strchr(testArgs+strlen(ModTypeStrings[i]), '+');
						if (NULL != nextArgIndex) {
							/* there is another argument, possibly "+printClassFileLoadMsg". */
							classNameLength = (int) (nextArgIndex - classIndex - 1);
						} else {
							/* this is the only argument, className must be the last string. */
							classNameLength = (int) (testArgs + strlen(testArgs) - classIndex - 1);
						}
						strncpy(argSet.className, classIndex + 1, classNameLength);
						argSet.className[classNameLength] = '\0';
					}
				}
				break;
			}
		}
		if (strstr(testArgs, "printClassFileLoadMsg")) {
			argSet.printClassFileLoadMsg = 1;
		}
	}
	
	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_retransform_classes = 0;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to add capabilities");
		return JNI_ERR;
	}	
	
	/* Set the ClassFileLoadHook event callback */
	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	callbacks.ClassFileLoadHook = classFileLoadHook;
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to set callback for ClassFileLoadHook events");
		return JNI_ERR;
	}

	/* Enable the JVMTI_EVENT_CLASS_FILE_LOAD_HOOK callback */
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to enable ClassFileLoadHook event");
		return JNI_ERR;
	} 

	/* Disable the JVMTI_EVENT_CLASS_FILE_LOAD_HOOK callback */
	if (NULL == testArgs) {
		err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_DISABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL);
		if (err != JVMTI_ERROR_NONE) {
			error(env, err, "Failed to disable ClassFileLoadHook event");
			return JNI_ERR;
		}
	}
	
	return JNI_OK;
}

jboolean JNICALL
Java_com_ibm_jvmti_tests_eventClassFileLoadHook_ecflh001_checkReenableInLivePhase(JNIEnv *jni_env, jclass cls)
{
	jvmtiError err; 
	jvmtiPhase phase;
	jvmtiCapabilities capabilities;
	
	JVMTI_ACCESS_FROM_AGENT(env);
	
	printf("reenable\n");
	
    err = (*jvmti_env)->GetPhase(jvmti_env, &phase);
    if (err != JVMTI_ERROR_NONE) {
        error(env, err, "Failed to GetPhase");
        return JNI_FALSE;
    }

    if (phase != JVMTI_PHASE_LIVE) {
        error(env, JVMTI_ERROR_WRONG_PHASE, "Wrong phase [%d], expected JVMTI_PHASE_LIVE");
        return JNI_FALSE;
    }

    
	err = (*jvmti_env)->GetPotentialCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to GetPotentialCapabilities");
		return JNI_ERR;
	}
    
	
	/* Enable the JVMTI_EVENT_CLASS_FILE_LOAD_HOOK callback */
	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to enable ClassFileLoadHook event");
		return JNI_FALSE;
	} 
	
	return JNI_TRUE;
}

