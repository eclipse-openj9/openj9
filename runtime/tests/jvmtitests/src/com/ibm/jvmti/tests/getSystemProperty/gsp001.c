/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

/* Following system properties are recommended to be provided by JVM as per JVMTI spec
 * https://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html#GetSystemProperties.
 */
typedef enum {
	JAVA_VM_VENDOR = 0, /* java.vm.vendor */
	JAVA_VM_VERSION, /* java.vm.version */
	JAVA_VM_NAME, /* java.vm.name */
	JAVA_VM_INFO, /* java.vm.info */
	JAVA_LIBRARY_PATH, /* java.library.path */
	JAVA_CLASS_PATH, /* java.class.path */
	JAVA_VM_SPECIFICATION_VERSION, /* java.vm.specification.version Note: not in Java 8 JVMTI list */
	PROPERTY_NUMBER, /* the number of system properties in this enum */
} System_Property_Names;

typedef struct sysPropMapping {
	const char *sysPropName;
	const int sysPropIndex;
} sysPropMapping;

static sysPropMapping sysPropMappingList[] =
{
	{ "java.vm.vendor", JAVA_VM_VENDOR },
	{ "java.vm.version", JAVA_VM_VERSION },
	{ "java.vm.name", JAVA_VM_NAME },
	{ "java.vm.info", JAVA_VM_INFO },
	{ "java.library.path", JAVA_LIBRARY_PATH },
	{ "java.class.path", JAVA_CLASS_PATH },
	{ "java.vm.specification.version", JAVA_VM_SPECIFICATION_VERSION }
};

char *sysPropValues[PROPERTY_NUMBER];

static agentEnv *localAgentEnv;

jint JNICALL
gsp001(agentEnv *agent_env, char *args)
{
	JVMTI_ACCESS_FROM_AGENT(agent_env);
	jint result = JNI_OK;
	jint counter = 0;

	localAgentEnv = agent_env;
	for (counter = 0; counter < PROPERTY_NUMBER; counter++) {
		jvmtiError err = (*jvmti_env)->GetSystemProperty(jvmti_env, sysPropMappingList[counter].sysPropName, &sysPropValues[counter]);
		if (JVMTI_ERROR_NONE != err) {
			fprintf(stderr, "JVMTI GetSystemProperty(%s) failed with rc = %x\n", sysPropMappingList[counter].sysPropName, err);
			result = JNI_ERR;
		} else {
			printf("gsp001 sysPropValues[%d] %s \n", counter, sysPropValues[counter]);
		}
	}

	return result;
}

jstring JNICALL
Java_com_ibm_jvmti_tests_getSystemProperty_gsp001_getSystemProperty(JNIEnv *jni_env, jclass klass, jstring propName)
{
	const char *propNameStr = NULL;
	jstring propValue = NULL;
	
	propNameStr = (*jni_env)->GetStringUTFChars(jni_env, propName, NULL);
	if (NULL != propNameStr) {
		jint counter = 0;
		for (counter = 0; counter < PROPERTY_NUMBER; counter++) {
			if (0 == strcmp(propNameStr, sysPropMappingList[counter].sysPropName)) {
				if (NULL != sysPropValues[counter]) {
					propValue = (*jni_env)->NewStringUTF(jni_env, sysPropValues[counter]);
				}
			}
		}
		(*jni_env)->ReleaseStringUTFChars(jni_env, propName, NULL);
	}
	
	return propValue;
}

void JNICALL
Java_com_ibm_jvmti_tests_getSystemProperty_gsp001_cleanup(JNIEnv *jni_env, jclass klass)
{
	jint counter = 0;
	jvmtiEnv *jvmti_env = localAgentEnv->jvmtiEnv;
	
	for (counter = 0; counter < PROPERTY_NUMBER; counter++) {
		printf("gsp001_cleanup sysPropValues[%d] %s \n", counter, sysPropValues[counter]);
		(*jvmti_env)->Deallocate(jvmti_env, (unsigned char*) sysPropValues[counter]);
	}
}
