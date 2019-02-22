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

#include "j9comp.h"
#include "jvmti.h"
#include "jvmti_test.h"


static agentEnv agent;
static agentEnv *env = NULL;
static jvmtiTest *test;

static jint runTest(agentEnv *env, char * options);



JNIEXPORT void JNICALL
Agent_OnUnload(JavaVM * vm)
{
	if (env != NULL) {
		tprintf(env, 100, "Agent_OnUnload\n");
		freeArguments(env);
	}
}


JNIEXPORT jint JNICALL
Agent_Prepare(JavaVM * vm, char *phase, char * options, void * reserved)
{
	jvmtiEnv * jvmti_env;
	jint rc;
	jvmtiError err;
	jint version;

	memset(&agent, 0x00, sizeof(agentEnv));

	env = &agent;
	env->vm = vm;
	env->outputLevel = 1;

	err = initializeErrorLogger(env);
	if (err != JVMTI_ERROR_NONE) {
		rc = JNI_ERR;
		printf("Unable to initialize the JVMTI Agent error logger facility\n");
		goto done;
	}

	if (options == NULL) {
		options = "";
	}
	tprintf(env, 100, "%s options: [%s]\n", phase, options);

	rc = (*vm)->GetEnv(vm, (void **) &jvmti_env, JVMTI_VERSION);
	if (rc != JNI_OK) {
		if ((rc != JNI_EVERSION) || ((rc = (*vm)->GetEnv(vm, (void **) &jvmti_env, JVMTI_VERSION_1_2)) != JNI_OK)) {
			error(env, err, "Failed to GetEnv %d\n", rc);
			goto done;
		}
	}

	env->jvmtiEnv = jvmti_env;

	err = (*jvmti_env)->GetVersionNumber(jvmti_env, &version);
	if (err != JVMTI_ERROR_NONE) {
		error(env, err, "Failed to GetVersionNumber");
		rc = JNI_ERR;
		goto done;
	}
	tprintf(env, 100, "JVMTI version: %p\n", version, getVersionName(env, version));
	env->jvmtiVersion = version;

	/* Take the options and run the testcase */
	rc = runTest(env, options);

done:

	if (rc == JNI_OK) {
		env->jvmtiEnv = jvmti_env;
	}

	if (hasErrors() == JNI_TRUE) {
		printErrorMessages(env);
	}

	return rc;
}

JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM * vm, char * options, void * reserved)
{
	return Agent_Prepare(vm, "Agent_OnLoad", options, reserved);
}

JNIEXPORT jint JNICALL
Agent_OnAttach(JavaVM * vm, char * options, void * reserved)
{
	if (0 == strlen(options)) { /* allow us to test agent loading with no options */
		fprintf(stderr, "called Agent_OnAttach with no options\n");
		printHelp();
		return JNI_OK;
	}
	return Agent_Prepare(vm, "Agent_OnAttach", options, reserved);
}

static jint
runTest(agentEnv *env, char * options)
{
	jint rc;

	tprintf(env, 200, "Command line options [%s]\n", options);

	parseArguments(env, options);

	/* Get and Run the requested testcase */
	test = getTest(env->testName);

	if (NULL != test) {
		rc = test->fn(env, env->testArgs);
	} else {
		error(env, JVMTI_ERROR_ILLEGAL_ARGUMENT, "Unknown testcase: [%s]\n", env->testName);
		rc = JNI_ERR;
		printHelp();
	}

	return rc;

}

/**
 * \brief return name of the class under testing
 *
 * @return  String containing name of testcase class
 *
 * The user specifies the testcase to be run by providing the test case name as an argument
 * to the agent (jvmtitest=testname). Return the name of the class containing the testcase
 */
jstring
Java_com_ibm_jvmti_tests_util_TestSuite_getSelectedTestClassName(JNIEnv * jni_env, jclass klazz)
{
	jstring testClassName = (*jni_env)->NewStringUTF(jni_env, test->klass);

	return testClassName;
}

jstring
Java_com_ibm_jvmti_tests_util_TestCase_getSelectedTestArguments(JNIEnv * jni_env, jclass klazz)
{
	jstring testArgs;

	if (env->testArgs) {
		testArgs = (*jni_env)->NewStringUTF(jni_env, env->testArgs);
	} else {
		testArgs = (*jni_env)->NewStringUTF(jni_env, "");
	}

	return testArgs;
}

jstring
Java_com_ibm_jvmti_tests_util_TestCase_getSpecificSubTestName(JNIEnv * jni_env, jclass klazz)
{
	jstring subtest;

	if (env->subtest) {
		subtest = (*jni_env)->NewStringUTF(jni_env, env->subtest);
	} else {
		subtest = (*jni_env)->NewStringUTF(jni_env, "");
	}

	return subtest;
}
