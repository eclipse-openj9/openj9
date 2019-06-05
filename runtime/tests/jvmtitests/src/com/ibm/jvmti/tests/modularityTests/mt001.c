/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
#include <stdlib.h>

#include "jvmti_test.h"

static agentEnv * env;
#if JAVA_SPEC_VERSION >= 9

jint JNICALL
mt001(agentEnv * agent_env, char * args)
{
	env = agent_env;

	return JNI_OK;
}

jint JNICALL
Java_com_ibm_jvmti_tests_modularityTests_mt001_addModuleReads(JNIEnv * jni_env, jclass klass, jobject fromMod, jstring toMod)
{
	JVMTI_ACCESS_FROM_AGENT(env);

	return (jint) (*jvmti_env)->AddModuleReads(jvmti_env, fromMod, toMod);;
}

jint JNICALL
Java_com_ibm_jvmti_tests_modularityTests_mt001_addModuleExports(JNIEnv * jni_env, jclass klass, jobject fromMod, jstring pkgName, jobject toMod)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err;
	char *utfPackageName = NULL;

	if (NULL != pkgName) {
		utfPackageName = (char *)(*jni_env)->GetStringUTFChars(jni_env, pkgName, NULL);
		if (NULL == utfPackageName) {
			return (jint) JVMTI_ERROR_INTERNAL;
		}
	}


	err = (*jvmti_env)->AddModuleExports(jvmti_env, fromMod, utfPackageName, toMod);

	(*jni_env)->ReleaseStringUTFChars(jni_env, pkgName, utfPackageName);

	return (jint) err;
}

jint JNICALL
Java_com_ibm_jvmti_tests_modularityTests_mt001_addModuleOpens(JNIEnv * jni_env, jclass klass, jobject fromMod, jstring pkgName, jobject toMod)
{
	JVMTI_ACCESS_FROM_AGENT(env);
	jvmtiError err;
	char *utfPackageName = NULL;

	if (NULL != pkgName) {
		utfPackageName = (char *)(*jni_env)->GetStringUTFChars(jni_env, pkgName, NULL);
		if (NULL == utfPackageName) {
			return (jint) JVMTI_ERROR_INTERNAL;
		}
	}


	err = (*jvmti_env)->AddModuleOpens(jvmti_env, fromMod, utfPackageName, toMod);

	(*jni_env)->ReleaseStringUTFChars(jni_env, pkgName, utfPackageName);


	return (jint) err;
}

jint JNICALL
Java_com_ibm_jvmti_tests_modularityTests_mt001_addModuleUses(JNIEnv * jni_env, jclass klass, jobject module, jclass service)
{
	JVMTI_ACCESS_FROM_AGENT(env);

	return (jint) (*jvmti_env)->AddModuleUses(jvmti_env, module, service);
}

jint JNICALL
Java_com_ibm_jvmti_tests_modularityTests_mt001_addModuleProvides(JNIEnv * jni_env, jclass klass, jobject module, jclass service, jclass implClass)
{
	JVMTI_ACCESS_FROM_AGENT(env);

	return (jint) (*jvmti_env)->AddModuleProvides(jvmti_env, module, service, implClass);;
}
#endif /* JAVA_SPEC_VERSION >= 9 */
