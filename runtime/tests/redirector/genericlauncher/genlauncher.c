/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
#include <errno.h>
#include <string.h>
#include "jni.h"

#if defined(WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#if defined(J9ZOS390)
#include <dll.h>
#include "atoe.h"
#define dlsym   dllqueryfn
#define dlopen(a,b)     dllload(a)
#define dlclose dllfree
#endif

#if defined(WIN32)
#define JVMLIB "jvm.dll"
#define PATHSEP "\\"
#else
#define JVMLIB "libjvm.so"
#define PATHSEP "/"
#endif

static struct {
	char *optionName;
	char *optionDescription;
} genlauncherOptStr[] = {
#define GLAUNCH_OPT_GPF_BEFORE_VMINIT "-GgpfBeforeVMInit"
		{GLAUNCH_OPT_GPF_BEFORE_VMINIT, "for gp test: trigger gpf before vm is initialized"},
#define GLAUNCH_OPT_GPF_AFTER_MAIN "-GgpfAfterMain"
		{GLAUNCH_OPT_GPF_AFTER_MAIN, "for gp test: trigger gpf after main is called"},
#define GLAUNCH_OPT_ABORT_HOOK "-GabortHook"
		{GLAUNCH_OPT_ABORT_HOOK, "pass in the abort hook function"},
#define GLAUNCH_OPT_EXIT_HOOK "-GexitHook"
		{GLAUNCH_OPT_EXIT_HOOK, "pass in the exit hook function"}
};

typedef jint (JNICALL *CreateVM)(JavaVM**, JNIEnv**, void*);

static void myAbortHook(void);
static void myExitHook(int status);
static void printUsage(void);

/* Example JVM abort hook function */
static void myAbortHook(void)
{
	fprintf(stderr, "myAbortHook called. JVM terminated abnormally\n");
}

/* Example JVM exit hook function */
static void myExitHook(int status)
{
	fprintf(stdout, "myExitHook called with status %d\n", status);
}

static void printUsage(void)
{
	int i;
	fprintf(stderr, "Usage: glauncher jvmlibpath [-genlauncher-specific options] [-java options] class [args...]\n\n"
					"where -genlauncher-specific options include:\n");
	for (i = 0; i < sizeof(genlauncherOptStr) / sizeof(genlauncherOptStr[0]); i++) {
		fprintf(stderr, "\t%20s: %s\n", genlauncherOptStr[i].optionName, genlauncherOptStr[i].optionDescription);
	}
	fprintf(stderr, "\n");
}

int main(int argc, char ** argv, char ** envp)
{
	int jvmOptionCount = 0;
	int i = 0;
	JavaVMInitArgs vm_args;
	JavaVMOption * args = NULL;
	JavaVM * vm = NULL;
	JNIEnv * env = NULL;
	int java_argc = 0;
	int mainIndex = 0;
	jmethodID mid;
	jclass cls;
	jclass stringClass;
	jobject java_argv;
	CreateVM create;
	void * handle = NULL;
	char * pathtojvm = NULL;
	char * jvmlibpath = NULL;
	typedef unsigned long UDATA;
	char * tmp = NULL;
	int gpfBeforeVMInit = 0;
	int gpfAfterMain = 0;
	int rc = 0;

#if defined(J9ZOS390)
	iconv_init();
	/* translate argv strings to ascii */
	for (i = 0; i < argc; i++) {
		argv[i] = e2a_string(argv[i]);
	}
#endif

	/* print usage at the start up */
	printUsage();

	if (argc < 2) {
		fprintf(stderr, "no path to jvm lib specified\n");
		rc = 22;
		goto fail;
	}
	pathtojvm = argv[1];

	jvmOptionCount = 0;

	/* Figure out the size of the array of jvm options
	 * At the same time:
	 * 	-parse the -G options that don't get converted to a JVM option
	 * 	figure out where the main class is
	 * 
	 */
	for (i = 2; i < argc; i++) {
		if (0 == strncmp(argv[i], "-", 1)) {
			/* parse the -G options that don't get converted to a JVM option */
			if (0 == strncmp(argv[i], "-Ggpf", 4)) {
				if (0 == strcmp(argv[i], GLAUNCH_OPT_GPF_BEFORE_VMINIT)) {
					gpfBeforeVMInit = 1;
				} else if (0 == strcmp(argv[i], GLAUNCH_OPT_GPF_AFTER_MAIN)) {
					gpfAfterMain = 1;
				}
			} else if (0 == strcmp(argv[i], "-cp")) {
				jvmOptionCount++;
				i++; /* the next option is the classpath itself */
			} else {
				jvmOptionCount++;
			}
		} else {
			/* this should be the main class */
			break;
		}
	}

	if (i == argc) {
		fprintf(stderr, "No class name specified\n");
		rc = 1;
		goto fail;
	}

	mainIndex = i;
	java_argc = argc - (i + 1);

	jvmOptionCount++; /* for "-Dinvokeviajava" */

	args = malloc(sizeof(JavaVMOption) * jvmOptionCount);
	if (!args) {
		fprintf(stderr, "arg malloc failure\n");
		rc = 2;
		goto fail;
	}

	vm_args.version = JNI_VERSION_1_2;
	vm_args.nOptions = jvmOptionCount;
	vm_args.options = args;
	vm_args.ignoreUnrecognized = JNI_FALSE;

	args[0].optionString = "-Dinvokeviajava";

	jvmOptionCount = 1;
	/* Create the JVM options */
	for (i = 2; i < mainIndex; i++) {

		/* skip options which are not converted to JVM options */
		if (0 == strncmp(argv[i], "-Ggpf", 4)) {
			continue;
		}

		if (0 == strncmp(argv[i], "-G", 2)) {
			/* convert genlauncher options (except -Ggpf options) to JVM options */
			if (0 == strcmp(argv[i], GLAUNCH_OPT_ABORT_HOOK)) {
				args[jvmOptionCount].optionString = "abort";
				args[jvmOptionCount].extraInfo = (void *)&myAbortHook;
			} else if (0 == strcmp(argv[i], GLAUNCH_OPT_EXIT_HOOK)) {
				args[jvmOptionCount].optionString = "exit";
				args[jvmOptionCount].extraInfo = (void *)&myExitHook;
			} else {
				fprintf(stderr, "Invalid option: %s\n", argv[i]);
				rc = 23;
				goto fail;
			}
		} else if (0 == strcmp(argv[i], "-cp")) {
			/* convert "-cp classpath" to "-Djava.class.path=classpath" */
			i++; /* the next option is the classpath itself */
			tmp = malloc(strlen("-Djava.class.path=") + strlen(argv[i]));
			sprintf(tmp, "%s%s", "-Djava.class.path=", argv[i]);
			args[jvmOptionCount].optionString = tmp;
		} else {
			args[jvmOptionCount].optionString = argv[i];
		}
		jvmOptionCount++;
	}

	jvmlibpath = malloc(strlen(pathtojvm) + strlen(JVMLIB) + 2);
	if (!jvmlibpath) {
		fprintf(stderr, "jvmlibpath malloc failure\n");
		rc = 21;
		goto fail;
	}
	jvmlibpath[0] = '\0';
	strcpy(jvmlibpath, pathtojvm);
	strcat(jvmlibpath, PATHSEP);
	strcat(jvmlibpath, JVMLIB);
	fprintf(stdout, "Attempting to open %s\n", jvmlibpath);
#if defined(WIN32)
	handle = LoadLibrary(jvmlibpath);
#else
	handle = (void*)dlopen(jvmlibpath, RTLD_NOW);
#endif

	if (NULL == handle) {
#if defined(WIN32)
		fprintf(stderr, "could not open %s: %d\n", jvmlibpath, GetLastError());
#else
		fprintf(stderr, "could not open %s: %s\n", jvmlibpath, dlerror());
#endif
		rc = 10;
		goto fail;
	}

#if defined(WIN32)
	create = (CreateVM)GetProcAddress((HINSTANCE)handle, "JNI_CreateJavaVM");
#else
	create = (CreateVM) dlsym(handle, "JNI_CreateJavaVM");
#endif
	if (!create) {
		fprintf(stderr, "could not lookup JNI_CreateJavaVM\n");
		rc = 11;
		goto fail;
	}

	if (1 == gpfBeforeVMInit) {
		*(UDATA*)-1 = -1;
	}

	fprintf(stdout, "About to invoke JNI_CreateJavaVM\n");

	if (create(&vm, &env, &vm_args)) {
		fprintf(stderr, "JNI_CreateJavaVM failed\n");
		rc = 3;
		goto fail;
	}

	cls = (*env)->FindClass(env, argv[mainIndex]);
	if (!cls) {
		(*env)->ExceptionDescribe(env);
		rc = 4;
		goto fail;
	}

	stringClass = (*env)->FindClass(env, "java/lang/String");
	if (!stringClass) {
		fprintf(stderr, "cannot find String\n");
		(*env)->ExceptionDescribe(env);
		rc = 6;
		goto fail;
	}
	java_argv = (*env)->NewObjectArray(env, java_argc, stringClass, NULL);
	if (!java_argv) {
		fprintf(stderr, "could not create arg array\n");
		(*env)->ExceptionDescribe(env);
		rc = 7;
		goto fail;
	}

	/* pass the java arguments*/
	for (i = 0; i < java_argc; i++) {
		jstring arg;
		arg = (*env)->NewStringUTF(env, argv[mainIndex + 1 + i]);
		if (!arg) {
			fprintf(stderr, "could not create arg string\n");
			(*env)->ExceptionDescribe(env);
			rc = 8;
			goto fail;
		}
		(*env)->SetObjectArrayElement(env, java_argv, i, arg);
	}

	mid = (*env)->GetStaticMethodID(env, cls, "main", "([Ljava/lang/String;)V");
	if (!mid) {
		fprintf(stderr, "cannot get main method\n");
		(*env)->ExceptionDescribe(env);
		rc = 5;
		goto fail;
	}

	(*env)->CallStaticVoidMethod(env, cls, mid, java_argv);

	if (1 == gpfAfterMain) {
		*(UDATA*)-1 = -1;
	}

	(*env)->ExceptionDescribe(env);
	(*vm)->DestroyJavaVM(vm);

fail:
	if (NULL != tmp) {
		free(tmp);
	}
	if (NULL != args) {
		free(args);
	}
	if (NULL != args) {
		free(jvmlibpath);
	}
#if defined(J9ZOS390)
	for (i = 0; i < argc; i++) {
		free(argv[i]);
	}
#endif
	if (NULL != handle) {
#if defined(WIN32)
		FreeLibrary(handle);
#else
		dlclose(handle);
#endif
	}
	return rc;

	return 0;
}
