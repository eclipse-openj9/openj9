/*******************************************************************************
 * Copyright (c) 2014, 2018 IBM Corp. and others
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
 * @file testjep178_static.c
 * Executable provides definitions for JNI routines statically, that is, as if
 * it were linked against the static library <library>, allowing the JVM to "see" 
 * JNI_OnLoad_testlib<library>() or Agent_OnLoad_testjvmti<library>().
 */

#include "testjep178.h"
#ifdef LINUX
/* kludge to work around known issue in glibc pre 2.25
 * https://sourceware.org/bugzilla/show_bug.cgi?id=16628
 * https://github.com/eclipse/openj9/issues/3672
 * Add a dummy pthread call to force libpthread to initialize.
 */
#include <pthread.h>

static  pthread_key_t key;
void unused_function()
{
    pthread_key_create(&key, NULL);
}
#endif

/* The JVM launcher. */
int main(int argc, char ** argv, char ** envp)
{
	JavaVMInitArgs vm_args;
	JavaVMOption* args = NULL;
	JavaVM* vm = NULL;
	JNIEnv* env = NULL;
	I_32 i = 0;
	I_32 result = 0;
	I_32 mainIndex = 0;
	I_32 javaArgc = 0;
	I_32 optionCount = 0;
	jmethodID mid;
	jclass cls;
	jobject javaArgv;
	typedef jint (JNICALL *createJVMFP)(JavaVM** pvm, void** penv, void* vm_args);
	createJVMFP createJavaVM = NULL;
	char* agentlib[J9JEP178_MAXIMUM_JVM_OPTIONS] = { NULL };
	I_32 agentCounter = 0;
	char* jvmPath = NULL;
	char* bootclasspath = NULL;
	char* classpath = NULL;
	char* compressedrefs = NULL;
	char* optionsBuffer[J9JEP178_MAXIMUM_JVM_OPTIONS] = {NULL};
	char buffer[J9PATH_LENGTH_MAXIMUM] = {0};
	jclass stringClass;
#if defined(WIN32)
	HINSTANCE handle = NULL;
#else
	void* handle = NULL;
#endif

	fprintf(stdout, "[MSG] Starting up JEP 178 test.\n");
	if (argc < 2) {
		fprintf(stderr, "[ERR] Insufficient arguments passed to test.\n");
		fprintf(stderr, "[MSG] Usage: testjep178 <jvm options> Class <program arguments>\n");
		result = 1;
		goto fail;
	}
	fflush(stdout);

	/* Strings specified on the command terminal are EBCDIC.  Convert these to ASCII
	 * for use in the program.  Free these strings at the end.
	 */
#if defined(J9ZOS390)
	iconv_init();
	/* Translate EBCDIC argv strings to ascii */
	for (i = 0; i < argc; i++) {
		argv[i] = e2a_string(argv[i]);
	}
#endif /* defined(J9ZOS390) */

	/* Find the index of the main class in list of arguments to the driver. */
	for (mainIndex = 1, i = 1; i < argc; i++, mainIndex++) {
		/* If this is a JVM option, iterate past it. */
		if (*argv[i] != '-') break;

		if (strncmp(argv[i], "-jvmpath:", sizeof("-jvmpath:") - 1) == 0) {
			jvmPath = argv[i] + sizeof("-jvmpath:") - 1;
		} else if (strncmp(argv[i], "-agentlib:", sizeof("-agentlib:") - 1) == 0) {
			/* Pass this option as it is (-agentlib:<lib>=<options>) to the JVM. */
			agentlib[agentCounter] = argv[i]; 
			agentCounter++;
		} else if (strncmp(argv[i], "-classpath:", sizeof("-classpath:") - 1) == 0) {
			classpath = argv[i] + sizeof("-classpath:") - 1;
		} else if (strncmp(argv[i], "-Xbootclasspath", sizeof("-Xbootclasspath") - 1) == 0) {
			bootclasspath = argv[i];
		} else if (strncmp(argv[i], "-Xnocompressedrefs", sizeof("-Xnocompressedrefs") - 1) == 0) {
			compressedrefs = "-Xnocompressedrefs";
		} else if (strncmp(argv[i], "-Xcompressedrefs", sizeof("-Xcompressedrefs") - 1) == 0) {
			compressedrefs = "-Xcompressedrefs";
		}
		/* No more options required by the test driver. */
	}

	/* Pass the -agentlib, if specified. */
	if (0 != agentCounter) {
		I_32 j;
		/* Allocate space for all of the -agentlib options passed to launcher. */
		for (j = 0; j < agentCounter; optionCount++, j++) {
			optionsBuffer[j] = (char*) malloc(J9VM_OPTION_MAXIMUM_LENGTH);
			if (NULL == optionsBuffer[j]) {
				result = 3;
				goto fail;
			}
			/* Copy out the options. */
			memset(optionsBuffer[j], 0, J9VM_OPTION_MAXIMUM_LENGTH);
			strncpy(optionsBuffer[j], agentlib[j], strlen(agentlib[j]));
		}
	}

	/* Add the -Xbootclasspath, if this was specified. */
	if ((NULL != bootclasspath) && (strlen(bootclasspath) != 0)) {
		optionsBuffer[optionCount] = (char*) malloc(J9VM_OPTION_MAXIMUM_LENGTH);
		if (NULL == optionsBuffer[optionCount]) {
			result = 2;
			goto fail;
		}
		memset(optionsBuffer[optionCount], 0, J9VM_OPTION_MAXIMUM_LENGTH);
		strcpy(optionsBuffer[optionCount], bootclasspath);
		optionCount++; /* Increment this only for JVM options we recognize. */
	}

	/* Pass the classpath argument, if this was specified. */
	if ((NULL != classpath) && (strlen(classpath) != 0)) {
		optionsBuffer[optionCount] = (char*) malloc(J9VM_OPTION_MAXIMUM_LENGTH);
		if (NULL == optionsBuffer[optionCount]) {
			result = 2;
			goto fail;
		}
		memset(optionsBuffer[optionCount], 0, J9VM_OPTION_MAXIMUM_LENGTH);
		sprintf(optionsBuffer[optionCount], "-Djava.class.path=%s", classpath);
		optionCount++; /* Increment this only for JVM options we recognize. */
	}

#if defined(J9ZOS390)
	optionsBuffer[optionCount] = (char*) malloc(J9VM_OPTION_MAXIMUM_LENGTH);
	if (NULL == optionsBuffer[optionCount]) {
		result = 2;
		goto fail;
	}
	memset(optionsBuffer[optionCount], 0, J9VM_OPTION_MAXIMUM_LENGTH);
	sprintf(optionsBuffer[optionCount], "-Dcom.ibm.tools.attach.enable=yes");
	optionCount++; /* Increment this only for JVM options we recognize. */
#endif /* defined(J9ZOS390) */

	/* Pass the -Xcompressedrefs or -Xnocompressedrefs, if specified. */
	if (NULL != compressedrefs) {
		optionsBuffer[optionCount] = (char*) malloc(J9VM_OPTION_MAXIMUM_LENGTH);
		if (NULL == optionsBuffer[optionCount]) {
			result = 3;
			goto fail;
		}
		memset(optionsBuffer[optionCount], 0, J9VM_OPTION_MAXIMUM_LENGTH);
		strncpy(optionsBuffer[optionCount], compressedrefs, strlen(compressedrefs));
		optionCount++; /* Increment this only for JVM options we recognize. */
	}

	/* Allocate memory for only "optionCount" structures since our simplified driver 
	 * does /not/ need, nor understand many/all JVM options.
	 */
	if (0 != optionCount) {
		I_32 counter = 0;

		args = (JavaVMOption*) malloc(sizeof(JavaVMOption) * optionCount);
		if (NULL == args) {
			fprintf(stderr, "[ERR] Failed allocating for jvm arguments.\n");
			result = 4;
			goto fail;
		}
		for (; counter < optionCount; counter++) {
			args[counter].optionString = optionsBuffer[counter];
		}
	}

	vm_args.version = JNI_VERSION_1_8;
	vm_args.nOptions = optionCount;
	vm_args.options = args;
	vm_args.ignoreUnrecognized = JNI_FALSE;

	if ((NULL != jvmPath) && (0 != strlen(jvmPath))) {
		sprintf(buffer, "%s%c%s", jvmPath, J9PATH_DIRECTORY_SEPARATOR, J9PATH_JVM_LIBRARY);
	} else {
		fprintf(stderr, "[ERR] Invalid path for jvm specified.\n");
		result = 5;
		goto fail;
	}

	fprintf(stdout, "[MSG] Opening jvm from %s\n", buffer);
	fflush(stdout);

	/* Open a handle to the jvm's shared library. */
#if defined(WIN32)
	handle = LoadLibrary(buffer);
#elif defined(LINUX) || defined(AIXPPC) || defined(OSX)
	handle = dlopen(buffer, RTLD_NOW);
#else /* ZOS */
	handle = dllload(buffer);
#endif
	if (NULL == handle) {
		fprintf(stderr, "[ERR] Failed opening virtual machine.\n");
		result = 6;
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
		result = 7;
		goto fail;
	}

	/* Actually launch the virtual machine. */
	if (createJavaVM(&vm, (void**)&env, &vm_args)) {
		fprintf(stderr, "[ERR] Failed launching virtual machine.\n");
		result = 8;
		goto fail;
	}

	/* Obtain a class ID for the main class (at index mainIndex). */
	cls = (*env)->FindClass(env, argv[mainIndex]);
	if (NULL == cls) {
		fprintf(stderr, "[ERR] Cannot find main class.  FindClass() failed.\n");
		(*env)->ExceptionDescribe(env);
		result = 9;
		goto fail;
	}

	stringClass = (*env)->FindClass(env, "java/lang/String");
	if (!stringClass) {
		fprintf(stderr, "[ERR] Cannot find String class.  FindClass() failed.\n");
		(*env)->ExceptionDescribe(env);
		result = 10;
		goto fail;
	}

	/* Create an array sufficiently large for the arguments to the Java program.
	javaArgc is the number of arguments to the Java program. */
	javaArgc = (argc - 1) - mainIndex;
	javaArgv = (*env)->NewObjectArray(env, javaArgc, stringClass, NULL);
	if (NULL == javaArgv) {
		fprintf(stderr, "[ERR] Could not create an argument array.  Allocation failed.\n");
		(*env)->ExceptionDescribe(env);
		result = 11;
		goto fail;
	}

	/* Create UTF strings for each Java argument and set this into the argument
	array to be passed to main. */
	for (i = 0; i < javaArgc; i++) {
		jstring arg;
		arg = (*env)->NewStringUTF(env, argv[mainIndex + 1 + i]);
		if (!arg) {
			fprintf(stderr, "[ERR] Could not create argument String.  Allocation failed.\n");
			(*env)->ExceptionDescribe(env);
			result = 12;
			goto fail;
		}
		(*env)->SetObjectArrayElement(env, javaArgv, i, arg);
	}

	/* Get the method ID for the main class using the class ID of the main class. */
	mid = (*env)->GetStaticMethodID(env, cls, "main", "([Ljava/lang/String;)V");
	if (NULL == mid) {
		fprintf(stderr, "[ERR] Cannot get main method.  GetStaticMethodID() failed.\n");
		(*env)->ExceptionDescribe(env);
		result = 13;
		goto fail;
	}

	/* Invoke the "main" method. */
	(*env)->CallStaticVoidMethod(env, cls, mid, javaArgv);

	/* Check for any pending exceptions. */
	(*env)->ExceptionDescribe(env);
	(*vm)->DestroyJavaVM(vm);

fail:
	if (NULL != handle) {
#if defined(WIN32)
		FreeLibrary(handle);
#elif defined(LINUX) || defined(AIXPPC) || defined(OSX)
		dlclose(handle);
#else /* Z/OS */
		dllfree(handle);
#endif
	}

#if defined(J9ZOS390)
	for (i = 0; i < argc; i++) {
		free(argv[i]);
	}
#endif /* defined(J9ZOS390) */

	if (NULL != args) {
		free(args);
	}
	
	{
		I_32 counter = 0;
		for (; counter < optionCount; counter++) {
			free(optionsBuffer[counter]);
		}
	}

	fprintf(stdout, "[MSG] Test jep178 %s with error code: %d\n",
			(0 == result) ? "passed" : "failed", 
			result);
	fflush(stdout);
	return result;
}
