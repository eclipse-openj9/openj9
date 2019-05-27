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

#include "exelib_api.h"
#include "jni.h"
#include "j9.h"
#include <string.h>
#include "j9cfg.h"

#define SHRTEST_MAX_CMD_OPTS 15
#define SHRTEST_MAX_PATH 1024
#if defined WIN32
#define	J9JAVA_DIR_SEPARATOR "\\"
#else
#define	J9JAVA_DIR_SEPARATOR "/"
#endif

#define PASS 0
#define FAIL 1
#define MAX_CHILD_PROCESS 15
#define CHILD_PROCESS "-child"
#define JAVAHOMEDIR "-Djava.home="
#define JAVAHOMEDIR_LEN strlen(JAVAHOMEDIR)

jint (JNICALL *CreateJavaVM)(JavaVM**, JNIEnv**, JavaVMInitArgs*);
jint (JNICALL *GetCreatedJavaVMs)(JavaVM**, jsize bufLen, jsize *nVMs);

static int startsWith(char *s, char *prefix);
UDATA childProcessMain(struct J9PortLibrary *portLibrary, void * vargs);
I_32 testMultipleProcessEachWithAJVM(struct J9PortLibrary *portLibrary, struct j9cmdlineOptions* args);
J9ProcessHandle launchChildProcess (J9PortLibrary* portLibrary, const char* testname, char * newargv[SHRTEST_MAX_CMD_OPTS], UDATA newargc);
IDATA waitForTestProcess (J9PortLibrary* portLibrary, J9ProcessHandle processHandle);
UDATA buildChildCmdlineOption(int argc, char **argv, char *options, char * newargv[SHRTEST_MAX_CMD_OPTS]);

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
	UDATA handle;

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
IDATA
setupArguments(struct j9cmdlineOptions* startupOptions,JavaVMInitArgs* vm_args,void **vmOptionsTable)
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
		if (startsWith(argv[counter],CHILD_PROCESS)!=0) {
			/*go to next argument*/
			continue;
		}
		if (vmOptionsTableAddOption(vmOptionsTable, argv[counter], NULL) != J9CMDLINE_OK) {

			rc = FAIL;
			goto cleanup;
		}
	}

	vm_args->version = JNI_VERSION_1_2;
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

/* mainline for the test */
UDATA
signalProtectedMain(struct J9PortLibrary *portLibrary, void * vargs)
{

	I_32 rc = PASS;
	IDATA i = 0;
	struct j9cmdlineOptions* args = (struct j9cmdlineOptions*) vargs;
	int argc = args->argc;
	char **argv = args->argv;

	PORT_ACCESS_FROM_PORT(args->portLibrary);

	/*RUN CHILD PROCESS*/
	for(i=0;i<argc;i++){
		if (startsWith(argv[i],CHILD_PROCESS)!=0) {
			rc |= childProcessMain(portLibrary, vargs);
			return rc;
		}
	}

	/*RUN PARENT PROCESS ... aka MAIN TEST CODE*/
	rc = testMultipleProcessEachWithAJVM(portLibrary, args);

	if (rc != PASS) {
		j9tty_printf(PORTLIB, "\nFAILURES DETECTED\n");
	} else {
		j9tty_printf(PORTLIB, "\nALL TESTS COMPLETED AND PASSED\n");
	}

	return rc;
}


/* mainline for the test */
UDATA
childProcessMain(struct J9PortLibrary *portLibrary, void * vargs)
{
	struct j9cmdlineOptions* args = (struct j9cmdlineOptions*) vargs;
	int argc = args->argc;
	char **argv = args->argv;
	I_32 rc = PASS;
	JavaVM *jvm = NULL;
	JavaVM* vmBuff[1];
	jsize numVms;
	JNIEnv* env = NULL;
	JavaVMInitArgs vm_args;
	void *vmOptionsTable = NULL;

	PORT_ACCESS_FROM_PORT(args->portLibrary);

#if defined(J9VM_OPT_MEMORY_CHECK_SUPPORT)
	/* This should happen before anybody allocates memory!  Otherwise, shutdown will not work properly. */
	memoryCheck_parseCmdLine( PORTLIB, argc-1, argv );
#endif /* J9VM_OPT_MEMORY_CHECK_SUPPORT */

	main_setNLSCatalog(PORTLIB, argv);

	/* first make sure we can load the function pointers */
	if (setupInvocationAPIMethods(args) != 0) {
		j9tty_printf(PORTLIB, "\nCound not do required setup...\n");
		rc = FAIL;
		goto done;
	}

	/* now set up the arguments for JNI_CreateJavaVM */
	if (setupArguments(args,&vm_args,&vmOptionsTable) != 0){
		j9tty_printf(PORTLIB, "\nCound not create required arguments for JNI_CreateJavaVM...\n");
		rc = FAIL;
		goto done;
	}

	/* validate that before any vms are created that the answer returned is
	 * that there are no created vms */
	if (GetCreatedJavaVMs(&vmBuff[0], 1, &numVms)!=0) {
		j9tty_printf(PORTLIB, "\nFailed to get created Java VMs before creation\n");
		rc = FAIL;
		goto done;
	}
	if (numVms != 0){
		j9tty_printf(PORTLIB, "\nGetCreatedJavaVMs returned %d, when it should have been 0\n",numVms);
		rc = FAIL;
		goto done;
	}

	if (CreateJavaVM(&jvm, &env, (JavaVMInitArgs*)(&vm_args))) {
		rc = FAIL;
		goto done;
	}

	/* now validate that after we have created a vm that we get the right answer */
	if (GetCreatedJavaVMs(&vmBuff[0], 1, &numVms)!=0) {
		j9tty_printf(PORTLIB, "\nFailed to get created Java VMs after creation\n");
		rc = FAIL;
		goto done;
	}

	if (numVms != 1){
		j9tty_printf(PORTLIB, "\nGetCreatedJavaVMs returned %d vms, when it should have been 1\n",numVms);
		rc = FAIL;
		goto done;
	}

	if (vmBuff[0] != jvm){
		printf("%p,%p\n",vmBuff[0],jvm);
		j9tty_printf(PORTLIB, "\nGetCreatedJavaVMs returned a vm that did not match the one created\n",numVms);
		rc = FAIL;
		goto done;
	}

	/* destroy the vm */
	if ((*jvm)->DestroyJavaVM((JavaVM*) jvm) != JNI_OK) {
		j9tty_printf(PORTLIB, "\nThe JVM was not shutdown properly\n");
		rc = FAIL;
		goto done;
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

	done:
	if (rc != PASS) {
		j9tty_printf(PORTLIB, "\nchildProcessMain FAILED (rc==%d)\n",rc);
	} else {
		j9tty_printf(PORTLIB, "\nchildProcessMain PASSED (rc==%d)\n",rc);
	}

	cleanupArguments(&vmOptionsTable);

	return rc;
}

/**
 * testMultipleProcessEachWithAJVM launches MAX_CHILD_PROCESS JVM's and makes sure it returns properly.
 */
I_32
testMultipleProcessEachWithAJVM(struct J9PortLibrary *portLibrary, struct j9cmdlineOptions* args) {

	I_32 rc = PASS;
	UDATA i = 0;
	J9ProcessHandle pid[MAX_CHILD_PROCESS];
	char * childargv[SHRTEST_MAX_CMD_OPTS];
	UDATA childargc = 0;
	int argc = args->argc;
	char **argv = args->argv;
	PORT_ACCESS_FROM_PORT(portLibrary);

	childargc= buildChildCmdlineOption(argc, argv, CHILD_PROCESS, childargv);
	for(i = 0; i < MAX_CHILD_PROCESS; i++) {
		pid[i] = launchChildProcess(PORTLIB, "testMultipleProcessEachWithAJVM", childargv, childargc);
		if(NULL== pid[i]) {
			j9tty_printf(PORTLIB, "testMultipleProcessEachWithAJVM: Failed to launch child process\n");
			rc = FAIL;
			goto done;
		}
	}

	for(i = 0; i < MAX_CHILD_PROCESS; i++) {
		IDATA procrc = waitForTestProcess(PORTLIB, pid[i]);
		switch(procrc) {
			case 0:
				break;
			default:
				j9tty_printf(PORTLIB, "testMultipleCreate: Unknown return value from WaitForProcess(): failed (procrc = %p)\n",procrc);
				rc = FAIL;
				/*Wait on every process that was started*/
				break;
		}
	}

	done:
	if (rc != PASS) {
		j9tty_printf(PORTLIB, "\ntestMultipleProcessEachWithAJVM FAILED (rc==%d)\n",rc);
	} else {
		j9tty_printf(PORTLIB, "\ntestMultipleProcessEachWithAJVM PASSED for %d processes (rc==%d)\n",i,rc);
	}

	return rc;
}

/**
 * Launches a child process
 * @param portLibrary
 * @param testname name of test
 * @param argv0 exe name
 * @param options options for the exe e.g. -child
 * @returns return code of process
 */
J9ProcessHandle
launchChildProcess (J9PortLibrary* portLibrary, const char* testname, char * newargv[SHRTEST_MAX_CMD_OPTS], UDATA newargc)
{
	J9ProcessHandle processHandle = NULL;
	char **env = NULL;
	UDATA envSize = 0;
	char *dir = ".";
	U_32 exeoptions = (J9PORT_PROCESS_INHERIT_STDOUT | J9PORT_PROCESS_INHERIT_STDERR);
	IDATA retVal;
	PORT_ACCESS_FROM_PORT(portLibrary);

	retVal = j9sysinfo_get_executable_name((char*)newargv[0], &(newargv[0]));

	if(retVal != 0) {
		I_32 portableErrno = j9error_last_error_number();
		const char * errMsg = j9error_last_error_message();
		if (NULL == errMsg) {
			j9tty_printf(portLibrary, "%s: launchChildProcess: j9sysinfo_get_executable_name failed!\n\tportableErrno = %d\n", testname, portableErrno);
		} else {
			j9tty_printf(portLibrary, "%s: launchChildProcess: j9sysinfo_get_executable_name failed!\n\tportableErrno = %d portableErrMsg = %s\n", testname, portableErrno, errMsg);
		}
		goto done;

	}

	retVal = j9process_create((const char **)newargv, newargc, env, envSize, dir, exeoptions, J9PORT_INVALID_FD, J9PORT_INVALID_FD, J9PORT_INVALID_FD, &processHandle);

	if (0 != retVal) {
		I_32 portableErrno = j9error_last_error_number();
		const char * errMsg = j9error_last_error_message();
		if (NULL == errMsg) {
			j9tty_printf(portLibrary, "%s: launchChildProcess: Failed to start process '%s %s'\n\tportableErrno = %d\n", testname, newargv[0], newargv[1],portableErrno);
		} else {
			j9tty_printf(portLibrary, "%s: launchChildProcess: Failed to start process '%s %s'\n\tportableErrno = %d portableErrMsg = %s\n", testname, newargv[0], newargv[1],portableErrno, errMsg);
		}

		goto done;
	}

	done:
		return processHandle;

}

/**
 * Wait on a 'processHandle'
 * @param portLibrary
 * @param processHandle
 * @returns return code of process
 */
IDATA
waitForTestProcess (J9PortLibrary* portLibrary, J9ProcessHandle processHandle)
{
	IDATA retval = -1;
	PORT_ACCESS_FROM_PORT(portLibrary);

	if (NULL == processHandle) {
		j9tty_printf(portLibrary, "waitForTestProcess: processHandle == NULL\n");
		goto done;
	}

	if (0 != j9process_waitfor(processHandle)) {
		I_32 portableErrno = j9error_last_error_number();
		const char * errMsg = j9error_last_error_message();
		if (NULL == errMsg) {
			j9tty_printf(portLibrary, "waitForTestProcess: j9process_waitfor() failed\n\tportableErrno = %d\n" , portableErrno);
		} else {
			j9tty_printf(portLibrary, "waitForTestProcess: j9process_waitfor() failed\n\tportableErrno = %d portableErrMsg = %s\n" , portableErrno, errMsg);
		}

		goto done;
	}

	retval = j9process_get_exitCode(processHandle);

	if (0 != j9process_close(&processHandle, 0)) {
		I_32 portableErrno = j9error_last_error_number();
		const char * errMsg = j9error_last_error_message();
		if (NULL == errMsg) {
			j9tty_printf(portLibrary, "waitForTestProcess: j9process_close() failed\n\tportableErrno = %d\n" , portableErrno);
		} else {
			j9tty_printf(portLibrary, "waitForTestProcess: j9process_close() failed\n\tportableErrno = %d portableErrMsg = %s\n" , portableErrno, errMsg);
		}
		goto done;
	}

	done:
	return retval;
}

/**
 * This method parses a looks for a prefix match in a command line option
 * @param s the option
 * @param prefix what to match
 * @returns 1 when there is a match
 */
static int
startsWith(char *s, char *prefix)
{
	int sLen=(int)strlen(s);
	int prefixLen=(int)strlen(prefix);
	int i;

	if (sLen < prefixLen) {
		return 0;
	}
	for (i=0; i<prefixLen; i++){
		if (s[i] != prefix[i]) {
			return 0;
		}
	}
	return 1; /*might want to make sure s is not NULL or something*/
}

/**
 * Add a new argument 'options' to 'argc'
 * @param argc argument count pass to this exe
 * @param argv arguments to look for -Djavahome in
 * @param options to use
 * @param newargv new args to use for process launch 'argc + options'
 * @returns number of arguments in newargv, or 0 on error
 */
UDATA
buildChildCmdlineOption(int argc, char **argv, char *options, char * newargv[SHRTEST_MAX_CMD_OPTS]) {
	int i;
	IDATA argsadded = 0;

	if ((argc+1) > SHRTEST_MAX_CMD_OPTS) {
		return 0;
	}

	memset(newargv, 0, SHRTEST_MAX_CMD_OPTS * sizeof(char*));

	for (i=0; i < argc;i++) {
		newargv[i] = argv[i];
		argsadded+=1;
	}
	newargv[i] = options;
	argsadded+=1;

	return argsadded;
}
