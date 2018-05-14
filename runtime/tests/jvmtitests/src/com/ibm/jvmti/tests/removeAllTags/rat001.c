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
static jvmtiExtensionFunction removeAllTags  = NULL;

jint JNICALL
rat001(agentEnv * agent_env, char * args)
{
	jvmtiCapabilities  caps;
	jvmtiError         err;

	JVMTI_ACCESS_FROM_AGENT(agent_env);
	env = agent_env;

	/* Add can_tag_objects capability */

	memset(&caps, 0, sizeof(jvmtiCapabilities));
	caps.can_tag_objects = 1;

	err = (*jvmti_env)->AddCapabilities(jvmti_env, &caps);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "AddCapabilities failed");
		return JNI_ERR;
	}

	return JNI_OK;
}


jboolean JNICALL
Java_com_ibm_jvmti_tests_removeAllTags_rat001_tryRemoveAllTags(JNIEnv * jni_env,
																		   jclass clazz,
																		   jclass klass,
																		   jobject klassObject, 
																		   jchar className)
{
	jint                         extensionCount;
	jvmtiExtensionFunctionInfo * extensionFunctions;
	jvmtiError                   err;
	int                          i;
	jvmtiEnv                   * jvmti_env = env->jvmtiEnv;
	jlong                        tag;
	

	err = (*jvmti_env)->GetExtensionFunctions(jvmti_env, &extensionCount, &extensionFunctions);
	if ( JVMTI_ERROR_NONE != err ) {
		error(env, err, "Failed GetExtensionFunctions");
		return JNI_FALSE;
	}

	for (i = 0; i < extensionCount; i++) {
		if (strcmp(extensionFunctions[i].id, COM_IBM_REMOVE_ALL_TAGS) == 0) {
			removeAllTags = extensionFunctions[i].func;
		}
	}

	err = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)extensionFunctions);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to Deallocate extension functions");
		return JNI_FALSE;
	}

	if (removeAllTags == NULL) {
		error(env, JVMTI_ERROR_NOT_FOUND, "removeAllTags extension was not found");            
		return JNI_FALSE;
	}


	/* Let's set & get the object tag */

	err = (*jvmti_env)->SetTag(jvmti_env, klassObject, 111);
	if ( err != JVMTI_ERROR_NONE ) {
		error(env, err, "SetTag() failed");
		return JNI_FALSE;
	}

	err = (*jvmti_env)->GetTag(jvmti_env, klassObject, &tag);
	if ( err != JVMTI_ERROR_NONE ) {
		error(env, err, "GetTag() failed");
		return JNI_FALSE;
	}

	if (tag != 111) {
		error(env, JVMTI_ERROR_NONE, "GetTag() failed");
		return JNI_FALSE;
	}

	/* Now remove all tags */

	err = (removeAllTags)(jvmti_env);

	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "RemoveAllTags failed");
		return JNI_FALSE;
	} 

	/* Try to get the tag again - should be 0 */

	tag = 555;

	err = (*jvmti_env)->GetTag(jvmti_env, klassObject, &tag);
	if ( err != JVMTI_ERROR_NONE ) {
		error(env, err, "GetTag() failed");
		return JNI_FALSE;
	}

	tprintf(env, 1, "tag %lld\n", tag);

	if (tag != 0) {
		error(env, JVMTI_ERROR_NONE, "RemoveAllTags did not remove tags");
		return JNI_FALSE;
	}


	return JNI_TRUE;
}
