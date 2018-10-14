/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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
#if defined(J9ZOS390)
#include <dll.h>
#include "atoe.h"
#define dlsym dllqueryfn
#define dlopen(a, b) dllload(a)
#define dlclose dllfree
#endif /* defined(J9ZOS390) */

#if defined(WIN32)
#include <windows.h>
#else /* defined(WIN32) */
#include <dlfcn.h>
#endif /* defined(WIN32) */

#include <errno.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "jni.h"
#include "jsig.h"

#define JVMLIBOPTION "-Djava.library.path="
#if defined(WIN32)
#define JVMLIB "jvm.dll"
#define PATHSEP "\\"
#define sigsetjmp(env, savesigs) setjmp(env)
#define siglongjmp(env, val) longjmp(env, val)
#else /* defined(WIN32) */
#if defined(OSX)
#define JVMLIB "libjvm.dylib"
#else /* OSX */
#define JVMLIB "libjvm.so"
#endif /* OSX */
#define PATHSEP "/"
#endif /* defined(WIN32) */

#define PRIMARY_CALLED 1
#define SECONDARY_CALLED 2
static volatile sig_atomic_t handlerCalled = 0;

typedef jint (JNICALL *CreateVM)(JavaVM**, JNIEnv**, void*);

static jmp_buf jmpenv;
void sighandlerPrimary(int);
void sighandlerSecondary(int);
static void runTest(JNIEnv *env);

int
main(int argc, char **argv, char **envp)
{
	int jvmOptionCount = 0;
	int i = 0;
	JavaVMInitArgs vm_args;
	JavaVMOption *args = NULL;
	JavaVM *vm = NULL;
	JNIEnv *env = NULL;
	CreateVM create;
	void *handle = NULL;
	char *pathtojvm = NULL;
	char *jvmlibpath = NULL;
	char *jvmlib = NULL;
	char *tmp = NULL;
	int rc = 0;

#if defined(J9ZOS390)
	iconv_init();
	/* translate argv strings to ascii */
	for (i = 0; i < argc; i += 1) {
		argv[i] = e2a_string(argv[i]);
	}
#endif /* defined(J9ZOS390) */

	/* Print usage at start up. */
	fprintf(stderr, "Usage: jvmlibpath[-java options]\n\n");

	if (2 > argc) {
		fprintf(stderr, "no path to jvm lib specified\n");
		rc = 22;
		goto fail;
	}
	pathtojvm = argv[1];

	jvmOptionCount = 0;

	/* Figure out the size of the array of jvm options. */
	for (i = 2; i < argc; i += 1) {
		if (0 == strncmp(argv[i], "-", 1)) {
			jvmOptionCount++;
		}
	}
	jvmOptionCount++; /* for "-Djava.library.path" */

	args = malloc(sizeof(JavaVMOption) * jvmOptionCount);
	if (NULL == args) {
		fprintf(stderr, "arg malloc failure\n");
		rc = 2;
		goto fail;
	}

	vm_args.version = JNI_VERSION_1_2;
	vm_args.nOptions = jvmOptionCount;
	vm_args.options = args;
	vm_args.ignoreUnrecognized = JNI_FALSE;

	/* Create the JVM options */
	for (i = 1; i < jvmOptionCount; i += 1) {
		args[i].optionString = argv[i + 1];
	}

	jvmlibpath = malloc(strlen(pathtojvm) + strlen(JVMLIBOPTION) + 2);
	if (NULL == jvmlibpath) {
		fprintf(stderr, "jvmlibpath malloc failure\n");
		rc = 21;
		goto fail;
	}
	jvmlibpath[0] = '\0';
	strcpy(jvmlibpath, JVMLIBOPTION);
	strcat(jvmlibpath, pathtojvm);
	args[0].optionString = jvmlibpath;

	jvmlib = malloc(strlen(pathtojvm) + strlen(JVMLIB) + 2);
	if (NULL == jvmlib) {
		fprintf(stderr, "jvmlib malloc failure\n");
		rc = 21;
		goto fail;
	}
	jvmlib[0] = '\0';
	strcpy(jvmlib, pathtojvm);
	strcat(jvmlib, PATHSEP);
	strcat(jvmlib, JVMLIB);
	fprintf(stdout, "Attempting to open %s\n", jvmlib);
#if defined(WIN32)
	handle = LoadLibrary(jvmlib);
#else /* defined(WIN32) */
	handle = (void *)dlopen(jvmlib, RTLD_NOW);
#endif /* defined(WIN32) */

	if (NULL == handle) {
#if defined(WIN32)
		fprintf(stderr, "Could not open %s: %d\n", jvmlib, GetLastError());
#else /* defined(WIN32) */
		fprintf(stderr, "Could not open %s: %s\n", jvmlib, dlerror());
#endif /* defined(WIN32) */
		rc = 10;
		goto fail;
	}

#if defined(WIN32)
	create = (CreateVM)GetProcAddress((HINSTANCE)handle, "JNI_CreateJavaVM");
#else /* defined(WIN32) */
	create = (CreateVM)dlsym(handle, "JNI_CreateJavaVM");
#endif /* defined(WIN32) */
	if (NULL == create) {
		fprintf(stderr, "Could not lookup JNI_CreateJavaVM.\n");
		rc = 11;
		goto fail;
	}

	fprintf(stdout, "About to invoke JNI_CreateJavaVM with %d option(s).\n", jvmOptionCount);

	if (create(&vm, &env, &vm_args)) {
		fprintf(stderr, "JNI_CreateJavaVM failed\n");
		rc = 3;
		goto fail;
	}

	runTest(env);
	(*vm)->DestroyJavaVM(vm);

fail:
	if (NULL != tmp) {
		free(tmp);
	}
	if (NULL != args) {
		free(args);
	}
	if (NULL != args) {
		free(jvmlib);
	}
	if (NULL != args) {
		free(jvmlibpath);
	}
#if defined(J9ZOS390)
	for (i = 0; i < argc; i+=1) {
		free(argv[i]);
	}
#endif /* defined(J9ZOS390) */
	if (NULL != handle) {
#if defined(WIN32)
		FreeLibrary(handle);
#else /* defined(WIN32) */
		dlclose(handle);
#endif /* defined(WIN32) */
	}
	return rc;
}

void
sighandlerPrimary(int signum)
{
	handlerCalled = PRIMARY_CALLED;
	jsig_handler(signum, NULL, NULL);
	siglongjmp(jmpenv, 1);
}

void
sighandlerSecondary(int signum)
{
	handlerCalled |= SECONDARY_CALLED;
}

static void
runTest(JNIEnv *env)
{
	sighandler_t rc = SIG_ERR;
	fprintf(stdout, "Starting test.\n");

	rc = jsig_primary_signal(SIGABRT, sighandlerPrimary);
	if (SIG_ERR == rc) {
		fprintf(stderr, "Error setting primary handler.\n");
	} else {
		fprintf(stdout, "Primary handler set.\n");
	}

	rc = signal(SIGABRT, sighandlerSecondary);
	if (SIG_ERR == rc) {
		fprintf(stderr, "Error setting secondary handler.\n");
	} else {
		fprintf(stdout, "Secondary handler set.\n");
	}

	fprintf(stdout, "Raising signal.\n");
	if (0 == sigsetjmp(jmpenv, 1)) {
		raise(SIGABRT);
	}

	if ((PRIMARY_CALLED | SECONDARY_CALLED) == handlerCalled) {
		fprintf(stdout, "Test passed. Both signal handlers invoked.\n");
	} else {
		fprintf(stderr, "Test failed. Primary handler %s and secondary handler %s.\n",
			0 != (handlerCalled & PRIMARY_CALLED) ? "called" : "not called",
			0 != (handlerCalled & SECONDARY_CALLED) ? "called" : "not called");
	}
}
