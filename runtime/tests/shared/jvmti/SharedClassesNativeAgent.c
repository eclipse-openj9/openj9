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
/*
 * A simple test JVMTI agent that can exercise the JVM appropriately with the features
 * of the JVMTI 1.1 spec that impact shared classes.
 */

#include <string.h>
#include <stdlib.h>

#ifdef WIN32
#include <windows.h>
#include <sys/stat.h> 
#include <time.h>
#endif  
 
#include "j9comp.h"
#include "ibmjvmti.h"
#include "j9.h"

jboolean jvmti11 = JNI_FALSE;
JavaVM * globalVM;
J9PortLibrary * PORTLIB;
UDATA portlibInitialized = 0; /* 0=No   1=Yes,from VM   2=Yes,created from scratch */
jvmtiEnv * globalJvmtiEnv;
char *testcaseArguments;

static jint configureForTestingRetransform(JavaVM * vm, jvmtiEnv * jvmti_env);
static void JNICALL retransformClassFileLoadHook (jvmtiEnv *jvmti_env, JNIEnv* jni_env, jclass class_being_redefined, jobject loader, const char* name, jobject protection_domain, jint class_data_len, const unsigned char* class_data, jint* new_class_data_len, unsigned char** new_class_data);
static char * errorName(jvmtiEnv * jvmti_env, jthread currentThread, jvmtiError err, char * str);


void JNICALL
Agent_OnUnload(JavaVM * vm)
{
	free(testcaseArguments);
}

/* Grab or build the PORTLIB if it hasnt been done already */
static void 
ensurePortLibrarySetup(JavaVM *vm) 
{
    if (portlibInitialized!=0) {
    	return;
    }
	if (vm!=NULL && ((J9JavaVM *) vm)->reserved1_identifier == (void*)J9VM_IDENTIFIER) {
		PORTLIB = ((J9JavaVM *) vm)->portLibrary;
		portlibInitialized = 1;
	} else {
		J9PortLibraryVersion portLibraryVersion;
		I_32 rc;

		J9PORT_SET_VERSION(&portLibraryVersion, J9PORT_CAPABILITY_MASK);
		rc = j9port_allocate_library(&portLibraryVersion, &PORTLIB);
		if (0 != rc) {
			j9tty_printf(PORTLIB, "Agent: Failed to create port library (%d)\n", rc);
			return;
		}
		rc = j9port_startup_library(PORTLIB);
		if (0 != rc) {
			j9tty_printf(PORTLIB, "Agent: Failed to start port library (%d)\n", rc);
			return;
		}
		portlibInitialized = 2;
	}
}
	


jint JNICALL
Agent_OnLoad(JavaVM * vm, char * options, void * reserved)
{
	jvmtiEnv * jvmti_env;
	jint rc; 
	jvmtiError err;
	jint version;

	globalVM = vm;
    ensurePortLibrarySetup(vm);	
	j9tty_printf(PORTLIB,"Agent: INFO: Agent_OnLoad running\n");

	if (options == NULL) {
		options = "";
	}
    j9tty_printf(PORTLIB, "Agent: INFO: -agentlib:SharedClassesNativeAgent[=retransform]\n");
	
	j9tty_printf(PORTLIB, "Agent: INFO: options=%s\n", options);

	rc = (*vm)->GetEnv(vm, (void **) &jvmti_env, JVMTI_VERSION_1_1);
	if (rc != JNI_OK) {
		if ((rc != JNI_EVERSION) || ((rc = (*vm)->GetEnv(vm, (void **) &jvmti_env, JVMTI_VERSION_1_0)) != JNI_OK)) {
			j9tty_printf(PORTLIB, "Agent: ERROR: Failed to get env %d\n", rc);
			goto done;
		}
	}

	
	err = (*jvmti_env)->GetVersionNumber(jvmti_env, &version);
	if (err != JVMTI_ERROR_NONE) {
		j9tty_printf(PORTLIB, "%s\n", errorName(jvmti_env, NULL, err, "Agent: ERROR: Failed to get JVMTI version"));
		rc = JNI_ERR;
		goto done;
	}
	/*j9tty_printf(PORTLIB, "JVMTI version: %p\n", version);*/
	jvmti11 = (version & ~JVMTI_VERSION_MASK_MICRO) == JVMTI_VERSION_1_1;

    if (NULL != strstr(options, "retransform")) {
    	j9tty_printf(PORTLIB,"Agent: INFO: registering as retransform capable, will retransform 'SimpleClass'\n");
		rc = configureForTestingRetransform(vm, jvmti_env);
    }

done:

	if (rc == JNI_OK) {
		globalJvmtiEnv = jvmti_env;
	} else if (portlibInitialized==2) {
		j9port_shutdown_library();
	}

	return rc;
}
#define BUFFER_SIZE 65536
typedef struct {
	char buffer[BUFFER_SIZE];
	char errorStr[1024];
	char threadNameStr[1024];
	char methodNameStr[1024];
	char classNameStr[1024];
	char fieldNameStr[1024];
	char typeStr[1024];
} threadData;

threadData globalData;

static threadData * getThreadData(jvmtiEnv * jvmti_env, jthread currentThread)
{
	if (currentThread != NULL) {
		threadData * localData = NULL;

		(*jvmti_env)->GetThreadLocalStorage(jvmti_env, currentThread, (void **) &localData);
		if (localData) {
			return localData;
		}
	}
	return &globalData;
}


static char * errorName(jvmtiEnv * jvmti_env, jthread currentThread, jvmtiError err, char * str)
{
	char * errorString;
	threadData * data = getThreadData(jvmti_env, currentThread);
	char * errorStr = data->errorStr;

	(*jvmti_env)->GetErrorName(jvmti_env, err, &errorString);
	sprintf(errorStr, "%s <%s>", str, errorString);
	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) errorString);
	return errorStr;
}

/**
 * Configure the agent for testing retransformation.
 */
static jint
configureForTestingRetransform(JavaVM * vm, jvmtiEnv * jvmti_env)
{
	jvmtiCapabilities capabilities;
	jvmtiError err;
	jvmtiEventCallbacks callbacks;

	if (!jvmti11) {
		j9tty_printf(PORTLIB, "Test requires JVMTI 1.1\n");
		return JNI_ERR;
	}


	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_retransform_classes = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		j9tty_printf(PORTLIB, "%s\n", errorName(jvmti_env, NULL, err, "Failed to add capabilities"));
		return JNI_ERR;
	}

	memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
	callbacks.ClassFileLoadHook = retransformClassFileLoadHook;
	err = (*jvmti_env)->SetEventCallbacks(jvmti_env, &callbacks, sizeof(jvmtiEventCallbacks));
	if (err != JVMTI_ERROR_NONE) {
		j9tty_printf(PORTLIB, "%s\n", errorName(jvmti_env, NULL, err, "Failed to set callbacks"));
		return JNI_ERR;
	}

	err = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL);
	if (err != JVMTI_ERROR_NONE) {
		j9tty_printf(PORTLIB, "%s\n", errorName(jvmti_env, NULL, err, "Failed to enable class load hook"));
		return JNI_ERR;
	}

	return JNI_OK;
}

#define CLASS_FOR_RETRANSFORM "design1474/resources/SimpleClass"

static void JNICALL
retransformClassFileLoadHook(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jclass class_being_redefined, jobject loader, const char* name, jobject protection_domain, jint class_data_len, const unsigned char* class_data, jint* new_class_data_len, unsigned char** new_class_data)
{
	static int count = 0;
	jvmtiError err;

    /*j9tty_printf(PORTLIB,"Loading %s\n",name);*/
	if (strstr(name,CLASS_FOR_RETRANSFORM) != NULL) {
		++count;
		if (count != 2) {
			j9tty_printf(PORTLIB, "Agent: INFO: intercepting loadclass for %s, not transforming at this time\n",name);	
		} else {
			unsigned char * copy;
			jint i;
			char replacedString[] = "ASC12345ASC";
			jint replaceLength = sizeof(replacedString) - 1;
			j9tty_printf(PORTLIB, "Agent: INFO: intercepting loadclass for %s, attempting transformation\n",name);	

			err = (*jvmti_env)->Allocate(jvmti_env, class_data_len, &copy);
			if (err == JVMTI_ERROR_NONE) {
				memcpy(copy, class_data, class_data_len);
				for (i = 0; i < class_data_len - replaceLength; ++i) {
					if (memcmp(class_data + i, replacedString, replaceLength) == 0) {
						memcpy(copy + i + 3, "67890", 5);
						j9tty_printf(PORTLIB, "Agent: INFO: returning modified class\n");
						*new_class_data_len = class_data_len;
						*new_class_data = copy;
						return;						
					}
				}
				j9tty_printf(PORTLIB, "Agent: ERROR: string not found\n");
				(*jvmti_env)->Deallocate(jvmti_env, copy);
			} else {
				j9tty_printf(PORTLIB, "Agent: ERROR: allocate failed\n");
			}
		}
		j9tty_printf(PORTLIB, "Agent: INFO: returning unmodified class data\n");
	}
}

JNIEXPORT jboolean JNICALL
Java_design1474_tests_TestRetransformation_retransform(JNIEnv *jni_env, jobject clazz)
{
       /* JVMTI_ACCESS_FROM_AGENT(_agentEnv);*/
        jvmtiError err;

	jint count = 1;
	jclass fooClazz;
	
	j9tty_printf(PORTLIB,"Agent: requesting retransform of '%s'\n",CLASS_FOR_RETRANSFORM);
	fooClazz = (*jni_env)->FindClass(jni_env, CLASS_FOR_RETRANSFORM);
	
	
	err = (*globalJvmtiEnv)->RetransformClasses(globalJvmtiEnv,count,&fooClazz);
	if (err!=JVMTI_ERROR_NONE) {
	  char *errString;
      (*globalJvmtiEnv)->GetErrorName(globalJvmtiEnv, err, &errString);
	  j9tty_printf(PORTLIB,"Agent:Failed to retransform:%s\n",errString);
	  return JNI_FALSE;
	}
	
    return JNI_TRUE;
}


static JavaVM *getJ9JavaVM(JNIEnv * env)
{
	JavaVM *jvm;

	if ((*env)->GetJavaVM(env, &jvm)) {
		return NULL;
	}

	if (((struct JavaVM_ *)jvm)->reserved1 != (void*)J9VM_IDENTIFIER) {
		return NULL;
	}

	return (JavaVM *)jvm;
}

JNIEXPORT jboolean JNICALL 
Java_design1474_tests_TestChangingBootclasspath_addToBootstrapClassLoaderSearch(JNIEnv *env, jobject jobj, jstring jAdditionalElement) 
{
	jvmtiError err;       
	char * errorString;
	const char* additionalElement;
	
	ensurePortLibrarySetup(getJ9JavaVM(env));
	if (globalJvmtiEnv==NULL) {
		j9tty_printf(PORTLIB,"Agent: ERROR: Please make sure you include the shared library as an agent with -agentlib\n");
		return JNI_FALSE;
	}
	additionalElement = (const char *) (*env)->GetStringUTFChars(env, jAdditionalElement, NULL);
	j9tty_printf(PORTLIB,"Agent: INFO: Attempting to add this entry '%s' to the boot classpath\n",additionalElement);
  	err = (*globalJvmtiEnv)->AddToBootstrapClassLoaderSearch(globalJvmtiEnv,additionalElement);
	if (err != JVMTI_ERROR_NONE) {
       	(*globalJvmtiEnv)->GetErrorName(globalJvmtiEnv, err, &errorString);
       	j9tty_printf(PORTLIB,"Agent: ERROR: Failed to add to the bootclasspath:%s\n",errorString);
       	(*env)->ReleaseStringUTFChars(env,jAdditionalElement,additionalElement);
		return JNI_FALSE;
	}						
   	(*env)->ReleaseStringUTFChars(env,jAdditionalElement,additionalElement);
	return JNI_TRUE;
}


JNIEXPORT jboolean JNICALL 
Java_design1474_tests_TestChangingSystemclasspath_addToSystemClassLoaderSearch(JNIEnv *env, jobject jobj, jstring jAdditionalElement) 
{
	jvmtiError err;       
	char * errorString;
	const char* additionalElement;
	ensurePortLibrarySetup(getJ9JavaVM(env));
	if (globalJvmtiEnv==NULL) {
		j9tty_printf(PORTLIB,"Agent: ERROR: Please make sure you include the shared library as an agent with -agentlib\n");
		return JNI_FALSE;
	}
	additionalElement = (const char *) (*env)->GetStringUTFChars(env, jAdditionalElement, NULL);
	j9tty_printf(PORTLIB,"Agent: INFO: Attempting to add this entry '%s' to the system classpath\n",additionalElement);
  	err = (*globalJvmtiEnv)->AddToSystemClassLoaderSearch(globalJvmtiEnv,additionalElement);
	if (err != JVMTI_ERROR_NONE) {
       	(*globalJvmtiEnv)->GetErrorName(globalJvmtiEnv, err, &errorString);
       	j9tty_printf(PORTLIB,"Agent: ERROR: Failed to add to the systemclasspath:%s\n",errorString);
       	(*env)->ReleaseStringUTFChars(env,jAdditionalElement,additionalElement);
		return JNI_FALSE;
	}						
       	(*env)->ReleaseStringUTFChars(env,jAdditionalElement,additionalElement);
	return JNI_TRUE;
}
