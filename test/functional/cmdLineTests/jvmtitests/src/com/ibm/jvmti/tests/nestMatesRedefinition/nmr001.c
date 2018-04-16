/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

static agentEnv *env;


jint JNICALL
nmr001(agentEnv *agent_env, char *args)
{
	jvmtiError err = JVMTI_ERROR_NONE;
	jvmtiCapabilities capabilities = {0};
	JVMTI_ACCESS_FROM_AGENT(agent_env);

	env = agent_env; 

	memset(&capabilities, 0, sizeof(jvmtiCapabilities));
	capabilities.can_redefine_classes = 1;
	/* Following capability is required to enable FSD */
	capabilities.can_generate_breakpoint_events = 1;
	err = (*jvmti_env)->AddCapabilities(jvmti_env, &capabilities);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to AddCapabilities");
		return JNI_ERR;
	}						

	return JNI_OK;
}

jint JNICALL
Java_com_ibm_jvmti_tests_nestMatesRedefinition_nmr001_redefineClass(JNIEnv *jni_env, jclass klass, jclass originalClass, jint classBytesSize, jbyteArray redefinedClassByteData)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jbyte *classByteDataRegion = NULL;
	char *classFileName = env->testArgs;
	jvmtiClassDefinition classdef = {0};
	jvmtiError err = JVMTI_ERROR_NONE;

	err = (*jvmti_env)->Allocate(jvmti_env, classBytesSize, (unsigned char **) &classByteDataRegion);
	if (err != JVMTI_ERROR_NONE) {
		goto done;
	}

	(*jni_env)->GetByteArrayRegion(jni_env, redefinedClassByteData, 0, classBytesSize, classByteDataRegion);
	if ((*jni_env)->ExceptionCheck(jni_env)) {
		err = JVMTI_ERROR_INTERNAL;
		goto freeMem;
	}

	classdef.class_bytes = (unsigned char *) classByteDataRegion;
	classdef.class_byte_count = classBytesSize;
	classdef.klass = originalClass;

	err = (*jvmti_env)->RedefineClasses(jvmti_env, 1, &classdef);

freeMem:
	(*jvmti_env)->Deallocate(jvmti_env, (unsigned char *) classByteDataRegion);
done:
	return (jint) err;
}
