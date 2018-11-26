/*******************************************************************************
 * Copyright (c) 2008, 2018 IBM Corp. and others
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
#include "jnitest_internal.h"
#include "j9protos.h"

#if defined(AIXPPC) || defined(LINUX) || defined(J9ZOS390) || defined(OSX)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/*
 * Regression test for CMVC 142759. Test that a thread can reattach to the JVM while exiting. 
 */

typedef struct TestData_t {
	JavaVM *vm;
	pthread_key_t key;
} TestData_t;

static void destructor(void *keyval);

static void
destructor(void *keyval)
{
	TestData_t *tdata = (TestData_t *)keyval;
	JNIEnv *env = NULL;
	jint rc;
	
	rc = (*tdata->vm)->AttachCurrentThread(tdata->vm, (void **)&env, NULL);
	if (rc != JNI_OK) {
		fprintf(stderr, "attach failed, rc=%d\n", rc);
		goto done;
	}
	fprintf(stderr, " AttachCurrentThread() passed\n");
	
	rc = (*tdata->vm)->DetachCurrentThread(tdata->vm);
	if (rc != JNI_OK) {
		fprintf(stderr, "detach failed, rc=%d\n", rc);
		goto done;
	}
	fprintf(stderr, " DetachCurrentThread() passed\n");
	
done:
	pthread_key_delete(tdata->key);
	free(tdata);
}

jboolean JNICALL 
Java_org_openj9_test_osthread_ReattachAfterExit_createTLSKeyDestructor(JNIEnv *env, jobject obj)
{
	TestData_t *tdata = NULL;
	int rc;
	jint jrc;
	
	tdata = malloc(sizeof(TestData_t));
	if (!tdata) {
		fprintf(stderr, "Failed to allocate tdata\n");
		return JNI_FALSE;
	}

	if (0 != (rc = pthread_key_create(&tdata->key, destructor))) {
		free(tdata);
		fprintf(stderr, "pthread_key_create failed: %d (%s)\n", rc, strerror(rc));
		return JNI_FALSE;
	}
	
	if (0 != (jrc = (*env)->GetJavaVM(env, &tdata->vm))) {
		pthread_key_delete(tdata->key);
		free(tdata);
		fprintf(stderr, "GetJavaVM failed: %d\n", jrc);
		return JNI_FALSE;
	}
	
	if (0 != (rc = pthread_setspecific(tdata->key, tdata))) {
		pthread_key_delete(tdata->key);
		free(tdata);
		fprintf(stderr, "pthread_setspecific failed: %d (%s)\n", rc, strerror(rc));
		return JNI_FALSE;
	}
	
	return JNI_TRUE;
}
#else /* defined(AIXPPC) || defined(LINUX) || defined(J9ZOS390) || defined(OSX) */
jboolean JNICALL 
Java_org_openj9_test_osthread_ReattachAfterExit_createTLSKeyDestructor(JNIEnv *env, jobject obj)
{
	/* This is a stub. This test is not applicable for non-pthread platforms. */
	return JNI_FALSE;
}
#endif /* defined(AIXPPC) || defined(LINUX) || defined(J9ZOS390) || defined(OSX) */
