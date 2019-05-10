/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
#include "jni.h"
#include "j9port.h"
#include "j9lib.h"
#include "exelib_api.h"
#include "omrthread.h"
#include "j9memcategories.h"
#include "j9.h"
#include "j9cfg.h"
#include "errno.h"


#define JNI_VERSION_1_3 0x00010003
#define SHRTEST_MAX_CMD_OPTS 15
#define SHRTEST_MAX_PATH 1024
#if defined WIN32
#define	J9JAVA_DIR_SEPARATOR "\\"
#else
#define	J9JAVA_DIR_SEPARATOR "/"
#endif
#define PASS 0
#define FAIL 1

jint (JNICALL *CreateJavaVM)(JavaVM**, JNIEnv**, JavaVMInitArgs*);
jint (JNICALL *GetCreatedJavaVMs)(JavaVM**, jsize bufLen, jsize *nVMs);

static omrthread_monitor_t monitor;
static UDATA done;
static UDATA failCode;
static jint exitCode;

JavaVM* vmBuff[1];
jsize numVms;
UDATA handle;

IDATA setupArguments(struct j9cmdlineOptions* startupOptions,JavaVMInitArgs* vm_args,void **vmOptionsTable, jint version);
void cleanupArguments(void **vmOptionsTable);
IDATA setupInvocationAPIMethods(struct j9cmdlineOptions* startupOptions);
static void JNICALL exitHook (jint rc);
static int testIgnoreUnrecognized (J9PortLibrary* portLib, struct j9cmdlineOptions* args, UDATA handle);
static int testAttachWithArgs (JavaVM* jvm, JavaVMAttachArgs* args);
static int testInvocation (J9PortLibrary* portLib,struct j9cmdlineOptions* args, int argc, char** argv, UDATA handle, jint version, jboolean ignoreUnrecognized, int expectResult);
static int J9THREAD_PROC attachHelperThread (void *entryarg);
static int test12Behaviour (J9PortLibrary* portLib, struct j9cmdlineOptions* args, int argc, char** argv, UDATA handle);
static int testAttachBehaviour (J9PortLibrary* portLib, struct j9cmdlineOptions* args, int argc, char** argv, UDATA handle);
static int testAttachScenarios (JavaVM* jvm);
static int J9THREAD_PROC destroyHelperThread (void *entryarg);
static int isIFAOffloadEnabled();
static int testIFAOffload(J9PortLibrary* portLib, struct j9cmdlineOptions* args, int argc, char** argv, UDATA handle);

UDATA signalProtectedMain(struct J9PortLibrary *portLibrary, void *arg)
{
	struct j9cmdlineOptions* args = (struct j9cmdlineOptions*) arg;
	int argc = args->argc;
	char **argv = args->argv;
	PORT_ACCESS_FROM_PORT(args->portLibrary);

	if (argc <= 1) {
		j9file_printf(PORTLIB, J9PORT_TTY_OUT, "jniinv test program\n");
		j9file_printf(PORTLIB, J9PORT_TTY_OUT, J9_COPYRIGHT_STRING "\n");
		j9file_printf(PORTLIB, J9PORT_TTY_OUT, "usage: jniinv [-D... [-X...]] [-io:\n");
		j9file_printf(PORTLIB, J9PORT_TTY_OUT, "minimally, you need to used -Djava.home=..\n");
		return 0;
	}

#ifdef J9VM_OPT_REMOTE_CONSOLE_SUPPORT
	remoteConsole_parseCmdLine( PORTLIB, argc - 1, argv );
#endif

#ifdef J9VM_OPT_MEMORY_CHECK_SUPPORT
	/* This should happen before anybody allocates memory!  Otherwise, shutdown will not work properly. */
	memoryCheck_parseCmdLine( PORTLIB, argc-1, argv );
#endif /* J9VM_OPT_MEMORY_CHECK_SUPPORT */

#define DLL_NAME J9_VM_DLL_NAME

	handle = 0;

	/*
	j9tty_printf (PORTLIB, "Testing invocation with JNI_VERSION_1_2\n--modified");
	if (testInvocation(PORTLIB, args, argc, argv, handle, JNI_VERSION_1_2, JNI_FALSE, PASS)) return FAIL;

	j9tty_printf (PORTLIB, "Testing invocation with JNI_VERSION_1_4\n");
	if (testInvocation(PORTLIB, args, argc, argv, handle, JNI_VERSION_1_4, JNI_FALSE, PASS)) return FAIL;

	j9tty_printf (PORTLIB, "Testing invocation with JNI_VERSION_1_6\n");
	if (testInvocation(PORTLIB, args, argc, argv, handle, JNI_VERSION_1_6, JNI_FALSE, PASS)) return FAIL;

	now test the ones which ought to fail
	j9tty_printf (PORTLIB, "Testing invocation with JNI_VERSION_1_1\n");
	if (testInvocation(PORTLIB, args, argc, argv, handle, JNI_VERSION_1_1, JNI_FALSE, FAIL)) return FAIL;

	j9tty_printf (PORTLIB, "Testing invocation with JNI_VERSION_1_3\n");
	if (testInvocation(PORTLIB, args, argc, argv, handle, JNI_VERSION_1_3, JNI_FALSE, FAIL)) return FAIL;

	j9tty_printf (PORTLIB, "Testing AttachCurrentThread/AttAchCurrentThreadAsDaemon behaviour\n");
	if (testAttachBehaviour(PORTLIB, args, argc, argv, handle)) return FAIL;

	j9tty_printf (PORTLIB, "Testing JNI 1.2 destroy/detach behaviour\n");
	if (test12Behaviour(PORTLIB, args, argc, argv, handle)) return FAIL;

	j9tty_printf (PORTLIB, "Testing ignoreUnrecognized\n");
	if (testIgnoreUnrecognized(PORTLIB,args, handle)) return FAIL;
	 */
	#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
		j9tty_printf (PORTLIB, "Testing iIFAOffload\n");
		if (testIFAOffload(PORTLIB, args, argc, argv, handle)) return FAIL;
	#endif

	j9tty_printf (PORTLIB, "No Errors\n");

	return PASS;
}

static int testInvocation(J9PortLibrary* portLib, struct j9cmdlineOptions* args, int argc, char** argv, UDATA handle, jint version, jboolean ignoreUnrecognized, int expectResult)
{
	JavaVMInitArgs vm_args;
	JavaVM *jvm;
	JNIEnv *env;

	void *vmOptionsTable = NULL;
	int rc = PASS;

	PORT_ACCESS_FROM_PORT(portLib);

	/* first make sure we can load the function pointers */
	if (setupInvocationAPIMethods(args) != 0) {
		j9tty_printf(PORTLIB, "\nCound not do required setup...\n");
		rc = FAIL;
		goto done;
	}

	/* now set up the arguments for JNI_CreateJavaVM */
	if (setupArguments(args,&vm_args,&vmOptionsTable, version) != 0){
		j9tty_printf(PORTLIB, "\nCound not create required arguments for JNI_CreateJavaVM...\n");
		rc = FAIL;
		goto done;
	}

	if (expectResult == PASS) {
		exitCode = -1;
	
		if (CreateJavaVM(&jvm, &env,(JavaVMInitArgs*)(&vm_args))) {
			j9tty_printf (PORTLIB, "Failed to create JVM\n");
			return FAIL;
		}

		if (exitCode != -1) {
			j9tty_printf (PORTLIB, "exitHook invoked too early (%d)\n", exitCode);
			return FAIL;
		}

		if ((*jvm)->DestroyJavaVM(jvm) != JNI_OK) {
			j9tty_printf (PORTLIB, "Failed to destroy JVM\n");
			return FAIL;
		}

		/* validate that after the vm is destroyed, we return
		 * that there are no created vms */
		if (GetCreatedJavaVMs(&vmBuff[0], 1, &numVms)!=0) {
			j9tty_printf(PORTLIB, "\nFailed to get created Java VMs after DestroyJavaVM\n");
			rc = FAIL;
			goto done;
		}
		if (numVms != 0){
			j9tty_printf(PORTLIB, "\nGetCreatedJavaVMs returned %d, when it should have been 0 after java vm destroyed\n",numVms);
			rc = FAIL;
			goto done;
		}

		if (exitCode != 0) {
			j9tty_printf (PORTLIB, "exitHook not called, or called with incorrect code (%d)\n", exitCode);
			return FAIL;
		}

	} else {
		if (!CreateJavaVM(&jvm, &env, &vm_args)) {
			j9tty_printf (PORTLIB, "Unexpected success creating JVM\n");
			return FAIL;
		}
	}

	printf("vm_args[%p].options[%p]\n", &vm_args, vm_args.options);
	j9mem_free_memory(vm_args.options);

	done:
	if (rc != PASS) {
		j9tty_printf(PORTLIB, "\ntestInvocation FAILED (rc==%d)\n",rc);
	} else {
		j9tty_printf(PORTLIB, "\ntestInvocation PASSED (rc==%d)\n",rc);
	}

	if (j9sl_close_shared_library(handle)) {
		j9tty_printf (PORTLIB, "Failed to close JVM DLL: %s\n", argv[1]);
		return FAIL;
	}

	cleanupArguments(&vmOptionsTable);

	return rc;
}

static int
test12Behaviour(J9PortLibrary* portLib, struct j9cmdlineOptions* args, int argc, char** argv, UDATA handle)
{
	JavaVMInitArgs vm_args;
	JavaVM *jvm;
	JNIEnv *env;
	omrthread_t currentThread;
	omrthread_t helperThread;

	void *vmOptionsTable = NULL;
	int rc = PASS;

	PORT_ACCESS_FROM_PORT(portLib);

	/* first make sure we can load the function pointers */
	if (setupInvocationAPIMethods(args) != 0) {
		j9tty_printf(PORTLIB, "\nCound not do required setup...\n");
		rc = FAIL;
		goto done2;
	}

	printf("1\n");
	/* now set up the arguments for JNI_CreateJavaVM */
	if (setupArguments(args,&vm_args,&vmOptionsTable, JNI_VERSION_1_2) != 0){
		j9tty_printf(PORTLIB, "\nCound not create required arguments for JNI_CreateJavaVM...\n");
		rc = FAIL;
		goto done2;
	}

	if (CreateJavaVM(&jvm, &env, &vm_args)) {
		j9tty_printf (PORTLIB, "Failed to create JVM\n");
		return FAIL;
	}

	if ((*jvm)->DetachCurrentThread(jvm)) {
		j9tty_printf (PORTLIB, "Failed to detach main thread\n");
		return FAIL;
	}
	if (omrthread_attach_ex(&currentThread, J9THREAD_ATTR_DEFAULT)) {
		j9tty_printf (PORTLIB, "Failed to OS attach main thread after java detach\n");
		return FAIL;
	}
	if (omrthread_monitor_init(&monitor, 0)) {
		j9tty_printf (PORTLIB, "Failed to create monitor\n");
		return FAIL;
	}
	done = FALSE;
	if (omrthread_create(&helperThread, 0, J9THREAD_PRIORITY_MAX, FALSE, destroyHelperThread, (void *) jvm)) {
		j9tty_printf (PORTLIB, "Failed to create destroy helper thread\n");
		return FAIL;
	}
	omrthread_monitor_enter(monitor);
	if (!done) {
		if (omrthread_monitor_wait_timed(monitor, 100000, 0)) {
			if (!done) {
				j9tty_printf (PORTLIB, "Destroy helper thread failed to exit in a reasonable amount of time\n");
				return FAIL;
			}
		}
	}
	omrthread_monitor_exit(monitor);
	if (failCode == 1) {
		j9tty_printf (PORTLIB, "Failed to attach in helper thread\n");
		return FAIL;
	}
	if (failCode == 2) {
		j9tty_printf (PORTLIB, "Failed to destroy JVM in helper thread\n");
		return FAIL;
	}
	if (omrthread_monitor_destroy(monitor)) {
		j9tty_printf (PORTLIB, "Failed to destroy monitor\n");
		return FAIL;
	}
	omrthread_detach(NULL);

	printf("vm_args[%p].options[%p]\n", &vm_args, vm_args.options);
	j9mem_free_memory(vm_args.options);

	done2:
	printf("done!\n");

	if (j9sl_close_shared_library(handle)) {
		j9tty_printf (PORTLIB, "Failed to close JVM DLL: %s\n", argv[1]);
		return FAIL;
	}

	cleanupArguments(&vmOptionsTable);

	return rc;
}




/**
 * This method setups up the invocation API methods so that we can use then in the test
 * @param startupOptions the command line options passed to the test
 * @returns 0 on success, non-zero otherwise
 */
IDATA
setupInvocationAPIMethods(struct j9cmdlineOptions* startupOptions)
{
	char libjvmPath[SHRTEST_MAX_PATH];
	char * libjvmPathOffset = NULL;
	const char * j9vmDir = "j9vm";
	const char * jvmLibName = "jvm";
	char * exename = NULL;
	int lastDirSep = -1;
	int i = 0;
	int found = 0;
	const char * directorySep = J9JAVA_DIR_SEPARATOR;
	char **argv = startupOptions->argv;
	void *vmOptionsTable = NULL;
	IDATA rc = PASS;

	PORT_ACCESS_FROM_PORT(startupOptions->portLibrary);

	/*Get the exe name*/
	if (j9sysinfo_get_executable_name(argv[0], &exename) != 0) {
		j9tty_printf(PORTLIB, "Failed to get executable name\n");
		rc = FAIL;
		goto cleanup;
	}

	if (cmdline_fetchRedirectorDllDir(startupOptions, libjvmPath) == FALSE) {
		printf("Please provide Java home directory (eg. /home/[user]/sdk/jre)\n");
		rc = FAIL;
		goto cleanup;
	}
	strcat(libjvmPath, jvmLibName);
	printf("libjvmPath is %s\n", libjvmPath);

	if (j9sl_open_shared_library(libjvmPath, &handle, J9PORT_SLOPEN_DECORATE)) {
		j9tty_printf(PORTLIB, "Failed to open JVM DLL: %s (%s)\n", libjvmPath, j9error_last_error_message());
		rc = FAIL;
		goto cleanup;
	}

	if (j9sl_lookup_name(handle, "JNI_CreateJavaVM", (UDATA*) &CreateJavaVM, "iLLL")) {
		j9tty_printf(PORTLIB, "Failed to find JNI_CreateJavaVM in DLL\n");
		rc = FAIL;
		goto cleanup;
	}

	if (j9sl_lookup_name(handle, "JNI_GetCreatedJavaVMs", (UDATA*) &GetCreatedJavaVMs, "iLii")) {
		j9tty_printf(PORTLIB, "Failed to find JNI_GetCreatedJavaVMs in DLL\n");
		rc = FAIL;
		goto cleanup;
	}

	cleanup: if (vmOptionsTable) {
		vmOptionsTableDestroy(&vmOptionsTable);
	}

	return rc;
}


/**
 * This function setups up the arguments that will be passed to JNI_CreateJavaVM, they attempt to set the
 * right options for the spec in which the test is being compiled.  cleanupArgument should later be called
 * to free the memory allocated
 *
 * @param startupOptions the command line options passed to the test
 * @param vm_argsm, the JavaVMInitArgs structure into which the arguments will be populated
 * @param vmOptionsTable options table that can be used by the function to generate the options, this should be passed to
 * cleanupArguments once the test is complete to free allocated memory
 * @returns 0 on success, non-zero otherwise
 *
 */
IDATA setupArguments(struct j9cmdlineOptions* startupOptions,JavaVMInitArgs* vm_args,void **vmOptionsTable, jint version)
{
	char **argv = startupOptions->argv;
	int argc = startupOptions->argc;
	int counter = 0;
	PORT_ACCESS_FROM_PORT(startupOptions->portLibrary);
	IDATA rc = 0;

	vmOptionsTableInit(PORTLIB, vmOptionsTable, 15);
	if (NULL == *vmOptionsTable)
		goto cleanup;

	if ((vmOptionsTableAddOption(vmOptionsTable, "_port_library", (void *) PORTLIB) != J9CMDLINE_OK)
	|| (vmOptionsTableAddOption(vmOptionsTable, "-Xint", NULL) != J9CMDLINE_OK)
	|| (vmOptionsTableAddOption(vmOptionsTable, "-Xmx64m", NULL) != J9CMDLINE_OK)
	|| (vmOptionsTableAddOption(vmOptionsTable, "-verbose:shutdown", NULL) != J9CMDLINE_OK)
	|| (vmOptionsTableAddOption(vmOptionsTable, "exit", (void *)exitHook) != J9CMDLINE_OK)
	#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
    || (vmOptionsTableAddOption(vmOptionsTable, "-Xifa:force", NULL) != J9CMDLINE_OK)
	#endif
	) {
		rc = FAIL;
		goto cleanup;
	}

	if (vmOptionsTableAddExeName(vmOptionsTable, argv[0]) != J9CMDLINE_OK) {
		rc = -1;
		goto cleanup;
	}

	/* add any other command line options */
	for (counter=1;counter<argc;counter++){
		if (vmOptionsTableAddOption(vmOptionsTable, argv[counter], NULL) != J9CMDLINE_OK) {

			rc = FAIL;
			goto cleanup;
		}
	}

	vm_args->version = version;
	vm_args->nOptions = vmOptionsTableGetCount(vmOptionsTable);
	vm_args->options = vmOptionsTableGetOptions(vmOptionsTable);
	vm_args->ignoreUnrecognized = JNI_FALSE;

	cleanup:

	return rc;
}

/**
 * This function frees any memory that was allocated when setupArguments was called
 *
 * @param vmOptionsTable options table passed to setupArguments
 * @returns 0 on success, non-zero otherwise
 */
void
cleanupArguments(void **vmOptionsTable)
{
	if (*vmOptionsTable) {
		vmOptionsTableDestroy(vmOptionsTable);
	}
}



static int J9THREAD_PROC 
destroyHelperThread(void *entryarg)
{
	JavaVM *jvm = (JavaVM *) entryarg;
	JNIEnv * env;

	if ((*jvm)->AttachCurrentThread(jvm, (void **) &env, NULL)) {
		failCode = 1;
		goto done;
	}
	if ((*jvm)->DestroyJavaVM(jvm)) {
		failCode = 2;
		goto done;
	}
	failCode = 0;

	done:
	omrthread_monitor_enter(monitor);
	done = TRUE;
	omrthread_monitor_notify(monitor);
	omrthread_exit(monitor);
	/* NEVER GETS HERE*/

	return 0;
}

static void JNICALL exitHook(jint rc) {
	exitCode = rc;
}

static int testIgnoreUnrecognized(J9PortLibrary* portLib, struct j9cmdlineOptions* args, UDATA handle)
{
	char* allRecognized[6] = {"", "-Xint", "-Dfoo=bar", "-verbose", "-verbose:class", "-verbose:gc"};
	char* unRecognizedX[3] = {"", "-Xint", "-Xwibble"};
	char* unRecognizedUS[3] = {"", "-Xint", "_wibble"};
	char* unRecognizedBogus[3] = {"", "-Xint", "chimpanzee"};
	char* testName, *ignoreUnrecStr = NULL;
	jboolean ignoreUnrec;

	PORT_ACCESS_FROM_PORT(portLib);

	j9tty_printf(PORTLIB, "\n****** VM OUTPUT START ******\n");

	testName = "all recognized";
	for (ignoreUnrec = 0; ignoreUnrec<=1; ignoreUnrec++) {
		if (testInvocation(PORTLIB, args, 6, allRecognized, handle, JNI_VERSION_1_2, ignoreUnrec, 1)) goto error;
	}

	testName = "unrecognized -X args";
	for (ignoreUnrec = 0; ignoreUnrec<=1; ignoreUnrec++) {
		if (testInvocation(PORTLIB, args, 3, unRecognizedX, handle, JNI_VERSION_1_2, ignoreUnrec, ignoreUnrec)) goto error;
	}

	testName = "unrecognized underscore args";
	for (ignoreUnrec = 0; ignoreUnrec<=1; ignoreUnrec++) {
		if (testInvocation(PORTLIB, args, 3, unRecognizedUS, handle, JNI_VERSION_1_2, ignoreUnrec, ignoreUnrec)) goto error;
	}

	testName = "completely invalid arguments";
	for (ignoreUnrec = 0; ignoreUnrec<=1; ignoreUnrec++) {
		if (testInvocation(PORTLIB, args, 3, unRecognizedBogus, handle, JNI_VERSION_1_2, ignoreUnrec, 0)) goto error;
	}

	j9tty_printf(PORTLIB, "****** VM OUTPUT END ******\n\n");

	return PASS;

error :
	ignoreUnrecStr = ignoreUnrec ? "JNI_TRUE" : "JNI_FALSE";
	j9tty_printf(PORTLIB, "****** VM OUTPUT END ******\n\n");
	j9tty_printf(PORTLIB, "FAILED: IgnoreUnrecognized test \"%s\" failed with ignoreUnrecognized=%d", testName, ignoreUnrec);

	return FAIL;
}


/*
 * Test various attach scenarios.
 */
static int
testAttachWithArgs(JavaVM* jvm, JavaVMAttachArgs* args)
{
	JNIEnv * env;
	JNIEnv * env2;
	int asDaemon;

	/* first make sure that we're not attached */
	if ((*jvm)->DetachCurrentThread(jvm) != JNI_EDETACHED) {
		return -1;
	}

	for (asDaemon = 0; asDaemon <= 1; asDaemon++) {
		/* attach the thread */
		if (asDaemon) {
			if ((*jvm)->AttachCurrentThreadAsDaemon(jvm, (void **) &env, args) != 0) {
				return -1;
			}
		} else {
			if ((*jvm)->AttachCurrentThread(jvm, (void **) &env, args) != 0) {
				return -1;
			}
		}

		/* we should be able to attach again and get back the same env */
		if ((*jvm)->AttachCurrentThread(jvm, (void **) &env2, NULL) != 0) {
			return -1;
		}
		if (env != env2) {
			return -1;
		}

		/* we should be able to attach again and get back the same env */
		if ((*jvm)->AttachCurrentThreadAsDaemon(jvm, (void **) &env2, NULL) != 0) {
			return -1;
		}
		if (env != env2) {
			return -1;
		}


		/* now detach */
		if ((*jvm)->DetachCurrentThread(jvm) != 0) {
			return -1;
		}

		/* detach should fail if it's called again */
		if ((*jvm)->DetachCurrentThread(jvm) != JNI_EDETACHED) {
			return -1;
		}
	}

	/* make sure we're detached before we return */
	if ((*jvm)->DetachCurrentThread(jvm) != JNI_EDETACHED) {
		return -1;
	}

	return 0;
}


static int J9THREAD_PROC
attachHelperThread(void *entryarg)
{
	JavaVM *jvm = (JavaVM *) entryarg;

	failCode = testAttachScenarios(jvm);

	omrthread_monitor_enter(monitor);
	done = TRUE;
	omrthread_monitor_notify(monitor);
	omrthread_exit(monitor);

	/* NEVER GETS HERE */

	return 0;
}

static int
testAttachBehaviour(J9PortLibrary* portLib, struct j9cmdlineOptions* args, int argc, char** argv, UDATA handle)
{
	JavaVMInitArgs vm_args;
	JavaVM *jvm;
	JNIEnv *env;
	omrthread_t currentThread;
	omrthread_t helperThread;

	void *vmOptionsTable = NULL;
	int rc = PASS;

	PORT_ACCESS_FROM_PORT(portLib);

	/* first make sure we can load the function pointers */
	if (setupInvocationAPIMethods(args) != 0) {
		j9tty_printf(PORTLIB, "\nCound not do required setup...\n");
		rc = FAIL;
		goto done2;
	}

	/* now set up the arguments for JNI_CreateJavaVM */
	if (setupArguments(args,&vm_args,&vmOptionsTable, JNI_VERSION_1_2) != 0){
		j9tty_printf(PORTLIB, "\nCound not create required arguments for JNI_CreateJavaVM...\n");
		rc = FAIL;
		goto done2;
	}

	if (CreateJavaVM(&jvm, &env, &vm_args)) {
		j9tty_printf (PORTLIB, "Failed to create JVM\n");
		return FAIL;
	}

	if (omrthread_attach_ex(&currentThread, J9THREAD_ATTR_DEFAULT)) {
		j9tty_printf (PORTLIB, "Failed to OS attach main thread after CreateJavaVM\n");
		return FAIL;
	}
	if (omrthread_monitor_init(&monitor, 0)) {
		j9tty_printf (PORTLIB, "Failed to create monitor\n");
		return FAIL;
	}

	done = FALSE;
	if (omrthread_create_ex(&helperThread, J9THREAD_ATTR_DEFAULT, FALSE, attachHelperThread, (void*)jvm)) {
		j9tty_printf (PORTLIB, "Failed to create attach helper thread\n");
		return FAIL;
	}
	omrthread_monitor_enter(monitor);
	if (!done) {
		if (omrthread_monitor_wait_timed(monitor, 100000, 0)) {
			if (!done) {
				j9tty_printf (PORTLIB, "Attach helper thread failed to exit in a reasonable amount of time\n");
				return FAIL;
			}
		}
	}
	omrthread_monitor_exit(monitor);
	if (failCode != 0) {
		j9tty_printf (PORTLIB, "Failed in testAttach. FailCode = %d\n", failCode);
		return FAIL;
	}

	if (omrthread_monitor_destroy(monitor)) {
		j9tty_printf (PORTLIB, "Failed to destroy monitor\n");
		return FAIL;
	}

	printf("vm_args[%p].options[%p]\n", &vm_args, vm_args.options);
	j9mem_free_memory(vm_args.options);

	if ((*jvm)->DestroyJavaVM(jvm)) {
		j9tty_printf (PORTLIB, "Failed to destroy JVM\n");
		return FAIL;
	}

	omrthread_detach(NULL);

	done2:
	printf("done!\n");

	if (j9sl_close_shared_library(handle)) {
		j9tty_printf (PORTLIB, "Failed to close JVM DLL: %s\n", argv[1]);
		return FAIL;
	}

	cleanupArguments(&vmOptionsTable);

	return rc;
}

/*
 * Test various attach scenarios.
 */
static int
testAttachScenarios(JavaVM* jvm)
{
	JavaVMAttachArgs args;

	if (testAttachWithArgs(jvm, NULL)) {
		return 1;
	}

	args.version = JNI_VERSION_1_2;
	args.name = NULL;
	args.group = NULL;
	if (testAttachWithArgs(jvm, &args)) {
		return 2;
	}

	args.version = JNI_VERSION_1_4;
	args.name = NULL;
	args.group = NULL;
	if (testAttachWithArgs(jvm, &args)) {
		return 3;
	}

	args.version = JNI_VERSION_1_2;
	args.name = "My test thread";
	args.group = NULL;
	if (testAttachWithArgs(jvm, &args)) {
		return 4;
	}

	return 0;
}

static int isIFAOffloadEnabled()
{
        /* (IHAPSA) PSATOLD contains address of current TCB (offset x'21c')
         * (IKJTCB) TCBSTCB contains address of STCB (offset x'138')
         * (IHASTCB) STCBWEB contains address of WEB (offset x'E8')
         * (no mapping macro) WEB offset x'54' 2 byte field that is the eligible-processor class.
         * Non-0 is offload-eligible: 2 is zAAP-eligible, 4 is zIIP-eligible;
         * There are equates for these values in IHAPSA (PSAProcClass_zAAP/zIIP) */

        /* all offsets are byte offsets */
        U_32* PSATOLD_ADDR = (U_32 *)(UDATA) 0x21c;  /* z/OS Pointer to current TCB or zero if in SRB mode. Field fixed by architecture. */
        U_32 tcbBase; /* base of the z/OS Task Control Block */
        const U_32 TCBSTCB_OFFSET = 0x138; /* offset of the TCBSTCB field in the TCB.  This field contains a pointer to the STCB. */
        U_32 stcbBaseAddr; /* Address of a control block field which contains a pointer to the base of the Secondary Task Control Block (STCB) */
        U_32 stcbBase; /* absolute address of the start of the STCB */
        const U_32 STCBWEB_OFFSET = 0xe8; /* offset of the STCBWEB field in the STCB.  This field contains a pointer to the WEB. */
        U_32 webBaseAddr; /* Address of a control block field which contains a pointer to the base of a Control Block owned by Task Management (WEB) */
        U_32 webBase; /* absolute address of the start of the WEB */
        const U_32 WEBEPC_OFFSET = 0x54; /* offset of the "eligible-processor class" field in the WEB. */
        U_32 eligibleProcessorClassAddr; /* absolute address of the "eligible-processor class" field of the WEB */
        U_16 eligibleProcessorClassValue; /* contents of the "eligible-processor class" field of the WEB */
        const U_16 PSAPROCCLASS_CP = 0;
        /* const U_16 PSAPROCCLASS_ZAAP = 2; */
        /* const U_16 PSAPROCCLASS_ZIIP = 4; */

        tcbBase = *PSATOLD_ADDR;
        stcbBaseAddr = tcbBase+TCBSTCB_OFFSET;
        stcbBase = *((U_32 *)(UDATA) stcbBaseAddr);
        webBaseAddr = stcbBase+STCBWEB_OFFSET;
        webBase = *((U_32 *)(UDATA) webBaseAddr);
        eligibleProcessorClassAddr = webBase+WEBEPC_OFFSET;
        eligibleProcessorClassValue = *((U_16 *)(UDATA) eligibleProcessorClassAddr);
        return PSAPROCCLASS_CP != eligibleProcessorClassValue;
}

static int testIFAOffload(J9PortLibrary* portLib, struct j9cmdlineOptions* args, int argc, char** argv, UDATA handle)
{
	JavaVMInitArgs vm_args;
	JavaVM *jvm;
	JNIEnv *env;

	void *vmOptionsTable = NULL;
	int rc = PASS;

	PORT_ACCESS_FROM_PORT(portLib);

	/* first make sure we can load the function pointers */
	if (setupInvocationAPIMethods(args) != 0) {
		j9tty_printf(PORTLIB, "\nCound not do required setup...\n");
		rc = FAIL;
		goto done;
	}

	/* now set up the arguments for JNI_CreateJavaVM */
	if (setupArguments(args,&vm_args,&vmOptionsTable, JNI_VERSION_1_2) != 0){
		j9tty_printf(PORTLIB, "\nCound not create required arguments for JNI_CreateJavaVM...\n");
		rc = FAIL;
		goto done;
	}

	exitCode = -1;

	fprintf(stderr, "testIFAOffload : creating VM\n");
	if (CreateJavaVM(&jvm, &env,(JavaVMInitArgs*)(&vm_args))) {
		j9tty_printf (PORTLIB, "Failed to create JVM\n");
		return FAIL;
	}

	fprintf(stderr, "testIFAOffload : checking exit code\n");
	if (exitCode != -1) {
			j9tty_printf (PORTLIB, "exitHook invoked too early (%d)\n", exitCode);
			return FAIL;
	}

	fprintf(stderr, "testIFAOffload : invoking isIFAOffloadEnabled()\n");

	if (isIFAOffloadEnabled()) {
			j9tty_printf (PORTLIB, "Failed to disable IFA offload in JVM creation\n");
			return FAIL;
	}

	fprintf(stderr, "testIFAOffload : destroying VM\n");
	if ((*jvm)->DestroyJavaVM(jvm)) {
			j9tty_printf (PORTLIB, "Failed to destroy JVM\n");
			return FAIL;
	}

	fprintf(stderr, "testIFAOffload : checking exit code again\n");

	if (exitCode != 0) {
			j9tty_printf (PORTLIB, "exitHook not called, or called with incorrect code (%d)\n", exitCode);
			return FAIL;
	}

	fprintf(stderr, "testIFAOffload : invoking isIFAOffloadEnabled() again\n");

	if (isIFAOffloadEnabled()) {
			j9tty_printf (PORTLIB, "Failed to disable IFA offload in JVM destruction\n");
			return FAIL;
	}
	/* TODO create a thread and check IFA offload state? */

	done:
	if (rc != PASS) {
		j9tty_printf(PORTLIB, "\ntestIFAOffload FAILED (rc==%d)\n",rc);
	} else {
		j9tty_printf(PORTLIB, "\ntestIFAOffload PASSED (rc==%d)\n",rc);
	}
	/* comment it out for now because close lib hits error
	if (j9sl_close_shared_library(handle)) {
		j9tty_printf (PORTLIB, "Failed to close JVM DLL: %s\n", argv[1]);
		return 1;
	}*/

	cleanupArguments(&vmOptionsTable);

	return rc;
}
