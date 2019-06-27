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

/**
 * @file testnativevmargs.c
 * Executable provides a native framework to create a VM with specified VM arguments.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jni.h"

#if defined(WIN32)
#include <windows.h>
#elif defined(LINUX) || defined(AIXPPC) || defined(OSX)
#include <dlfcn.h>
#else
#include <dll.h>
#include "atoe.h"
#endif

#define J9PATH_LENGTH_MAXIMUM 1024
#if defined(WIN32)
#define J9PATH_DIRECTORY_SEPARATOR '\\'
#define J9PATH_JVM_LIBRARY "jvm.dll"
#else /* defined(WIN32) */
#define J9PATH_DIRECTORY_SEPARATOR '/'
#if defined(OSX)
#define J9PATH_JVM_LIBRARY "libjvm.dylib"
#else /* defined(OSX) */
#define J9PATH_JVM_LIBRARY "libjvm.so"
#endif /* defined(OSX) */
#endif /* defined(WIN32) */

/* The JVM launcher. */
int main(int argc, char **argv)
{
	JavaVMInitArgs vm_args;
	JavaVMOption * args = NULL;
	JavaVM * vm = NULL;
	JNIEnv * env = NULL;
	jint result;
	jboolean ignoreUnrecognized = JNI_FALSE;
	int i;
	int jvmOptionCount = 0;
	int optionCount = 0;
	char * jvmPath = NULL;
	int rc = 0;
	char buffer[J9PATH_LENGTH_MAXIMUM] = {0};
	typedef jint (JNICALL * createJVMFP)(JavaVM ** pvm, void ** penv, void * vm_args);
	createJVMFP createJavaVM = NULL;
#if defined(WIN32)
	HINSTANCE handle = NULL;
#else
	void * handle = NULL;
#endif

	fprintf(stdout, "[MSG] Starting up native VM options test.\n");
	if (argc < 2) {
		fprintf(stderr, "[ERR] Insufficient arguments passed to test.\n");
		fprintf(stderr, "[MSG] Usage: nativevmargs <jvmlibpath> <options>\n");
		rc = 1;
		goto fail;
	}
	fflush(stdout);

#if defined(J9ZOS390)
	iconv_init();
	/* Translate EBCDIC argv strings to ascii */
	for (i = 0; i < argc; i++) {
		argv[i] = e2a_string(argv[i]);
	}
#endif /* defined(J9ZOS390) */

	jvmPath = argv[1];
	optionCount = argc - 2;

	args = malloc(sizeof(JavaVMOption) * optionCount);
	if (!args) {
		fprintf(stderr, "[ERR] Failed memory allocation for JVM arguments.\n");
		rc = 2;
		goto fail;
	}

	/* Create the JVM options */
	for (i = 2; jvmOptionCount < optionCount; i++) {
		if (0 == strncmp(argv[i], "-Custom:", 8)) {
			/* convert custom options to JVM options */
			if (0 == strcmp(argv[i], "-Custom:WhitespaceOption")) {
				args[jvmOptionCount].optionString = " \t ";
			} else if (0 == strcmp(argv[i], "-Custom:EmptyOption")) {
				args[jvmOptionCount].optionString = "";
			} else if (0 == strcmp(argv[i], "-Custom:IgnoreUnrecognizedOptions")) {
				ignoreUnrecognized = JNI_TRUE;
				optionCount--;
				continue;
			} else {
				fprintf(stderr, "[ERR] Invalid option: %s\n", argv[i]);
				rc = 3;
				goto fail;
			}
		} else {
			args[jvmOptionCount].optionString = argv[i];
		}
		jvmOptionCount++;
	}

	vm_args.version = JNI_VERSION_1_8;
	vm_args.nOptions = jvmOptionCount;
	vm_args.options = args;
	vm_args.ignoreUnrecognized = ignoreUnrecognized;

	if ((NULL != jvmPath) && (0 != strlen(jvmPath))) {
		sprintf(buffer, "%s%c%s", jvmPath, J9PATH_DIRECTORY_SEPARATOR, J9PATH_JVM_LIBRARY);
	} else {
		fprintf(stderr, "[ERR] Invalid path for JVM specified.\n");
		rc = 4;
		goto fail;
	}

	fprintf(stdout, "[MSG] Opening JVM from %s\n", buffer);
	fflush(stdout);

	/* Open a handle to the JVM's shared library. */
#if defined(WIN32)
	handle = LoadLibrary(buffer);
#elif defined(LINUX) || defined(AIXPPC) || defined(OSX)
	handle = dlopen(buffer, RTLD_NOW);
#else /* ZOS */
	handle = dllload(buffer);
#endif
	if (NULL == handle) {
		fprintf(stderr, "[ERR] Failed opening virtual machine.\n");
		rc = 5;
		goto fail;
	}

	/* Lookup for the virtual machine entry point routine. */
#if defined(WIN32)
	createJavaVM = (createJVMFP) GetProcAddress((HINSTANCE)handle, "JNI_CreateJavaVM");
#elif defined(LINUX) || defined(AIXPPC) || defined(OSX)
	createJavaVM = (createJVMFP) dlsym(handle, "JNI_CreateJavaVM");
#else /* Z/OS */
	createJavaVM = (createJVMFP) dllqueryfn(handle, "JNI_CreateJavaVM");
#endif
	if (NULL == createJavaVM) {
		fprintf(stderr, "[ERR] Failed locating virtual machine entry.\n");
		rc = 6;
		goto fail;
	}

	/* Actually launch the virtual machine. */
	result = createJavaVM(&vm, (void **)&env, &vm_args);
	if (JNI_OK != result) {
		fprintf(stderr, "[ERR] Failed launching JVM.\n");
		rc = 7;
		goto fail;
	}

fail:
	if (NULL != args) {
		free(args);
	}

#if defined(J9ZOS390)
	for (i = 0; i < argc; i++) {
		free(argv[i]);
	}
#endif /* defined(J9ZOS390) */

	fprintf(stdout, "[MSG] Test nativevmargs concluded with code: %d\n", rc);
	fflush(stdout);

	return rc;
}
