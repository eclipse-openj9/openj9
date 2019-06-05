/*******************************************************************************
 * Copyright (c) 2008, 2019 IBM Corp. and others
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

#include "jni.h"
#include "j9cfg.h"
#include "j9comp.h"
#include "j9port.h"
#include "j9lib.h"
#include "exelib_api.h"
#include "thrstatetest.h"

/* test cases return 0 on success, non-0 otherwise */
#define TESTCASE(x) { #x, x, 0 }
#define THRTEST_MAX_PATH 256
#if defined WIN32
#define	J9JAVA_DIR_SEPARATOR "\\"
#else
#define	J9JAVA_DIR_SEPARATOR "/"
#endif

typedef struct testcase_t {
	const char *name;
	UDATA (*func)(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
	UDATA retVal;
} testcase_t;

testcase_t testCases[] = {
		TESTCASE(test_setup),

		/* testcases1 */
		TESTCASE(test_vNull),
		TESTCASE(test_v0_nNull),

		TESTCASE(test_vBfVC_nNull),
		TESTCASE(test_vBaVC_nNull),
		TESTCASE(test_vWaoc_nNull),
		TESTCASE(test_vWTaoc_nNull),
		TESTCASE(test_vPMoc_nNull),
		TESTCASE(test_vPTm_nNull),
		TESTCASE(test_vS_nNull),

		TESTCASE(test_v0_nStm),
		TESTCASE(test_v0_nStMoc),
		TESTCASE(test_v0_nStMoC),
		TESTCASE(test_v0_nStMOc),
		TESTCASE(test_v0_nStMOC),
		TESTCASE(test_v0_nStMNc),
		TESTCASE(test_v0_nStMNC),
		TESTCASE(test_v0_nStMVc),
		TESTCASE(test_v0_nStMVC),

		TESTCASE(test_v0_nDm),
		TESTCASE(test_v0_nDMoc),
		TESTCASE(test_v0_nDMoC),
		TESTCASE(test_v0_nDMOc),
		TESTCASE(test_v0_nDMOC),
		TESTCASE(test_v0_nDMNc),
		TESTCASE(test_v0_nDMNC),
		TESTCASE(test_v0_nDMVc),
		TESTCASE(test_v0_nDMVC),

		TESTCASE(test_v0_nZm),
		TESTCASE(test_v0_nZMoc),
		TESTCASE(test_v0_nZMoC),
		TESTCASE(test_v0_nZMOc),
		TESTCASE(test_v0_nZMOC),
		TESTCASE(test_v0_nZMNc),
		TESTCASE(test_v0_nZMNC),
		TESTCASE(test_v0_nZMVc),
		TESTCASE(test_v0_nZMVC),

		TESTCASE(test_v0_nBm),
		TESTCASE(test_v0_nBMoc),
		TESTCASE(test_v0_nBMoC),
		TESTCASE(test_v0_nBMOc),
		TESTCASE(test_v0_nBMOC),
		TESTCASE(test_v0_nBMNc),
		TESTCASE(test_v0_nBMNC),
		TESTCASE(test_v0_nBMVc),
		TESTCASE(test_v0_nBMVC),
		TESTCASE(test_v0_nBMVC_2),

		TESTCASE(test_v0_nZBm),
		TESTCASE(test_v0_nZBMoc),
		TESTCASE(test_v0_nZBMoC),
		TESTCASE(test_v0_nZBMOc),
		TESTCASE(test_v0_nZBMOC),
		TESTCASE(test_v0_nZBMNc),
		TESTCASE(test_v0_nZBMNC),
		TESTCASE(test_v0_nZBMVc),
		TESTCASE(test_v0_nZBMVC),

		TESTCASE(test_v0_nWm),
		TESTCASE(test_v0_nWMoc),
		TESTCASE(test_v0_nWMoC),
		TESTCASE(test_v0_nWMOc),
		TESTCASE(test_v0_nWMOC),
		TESTCASE(test_v0_nWMNc),
		TESTCASE(test_v0_nWMNC),
		TESTCASE(test_v0_nWMVc),
		TESTCASE(test_v0_nWMVC),
		TESTCASE(test_v0_nWMVC_2),

		TESTCASE(test_v0_nBNm),
		TESTCASE(test_v0_nBNMoc),
		TESTCASE(test_v0_nBNMoC),
		TESTCASE(test_v0_nBNMOc),
		TESTCASE(test_v0_nBNMOC),
		TESTCASE(test_v0_nBNMNc),
		TESTCASE(test_v0_nBNMNC),
		TESTCASE(test_v0_nBNMVc),
		TESTCASE(test_v0_nBNMVC),

		TESTCASE(test_v0_nS),
		TESTCASE(test_v0_nSI),
		TESTCASE(test_v0_nSII),

		/* testcases2 */
		TESTCASE(test_vBz_nSt),

		TESTCASE(test_vBfoc_nSt),
		TESTCASE(test_vBfoC_nSt),
		TESTCASE(test_vBfOc_nSt),
		TESTCASE(test_vBfOC_nSt),
		TESTCASE(test_vBfVc_nSt),
		TESTCASE(test_vBfVC_nSt),

		TESTCASE(test_vBaoc_nSt),
		TESTCASE(test_vBaoC_nSt),
		TESTCASE(test_vBaOc_nSt),
		TESTCASE(test_vBaOC_nSt),
		TESTCASE(test_vBaVc_nSt),
		TESTCASE(test_vBaVC_nSt),
		TESTCASE(test_vBaNc_nSt),
		TESTCASE(test_vBaNC_nSt),

		TESTCASE(test_vBaoc_nB),
		TESTCASE(test_vBaoC_nB),
		TESTCASE(test_vBaOc_nB),
		TESTCASE(test_vBaOC_nB),
		TESTCASE(test_vBaVc_nB),
		TESTCASE(test_vBaVC_nB),
		TESTCASE(test_vBaNc_nB),
		TESTCASE(test_vBaNC_nB),

		TESTCASE(test_vBaoc_nB2),
		TESTCASE(test_vBaoC_nB2),
		TESTCASE(test_vBaOc_nB2),
		TESTCASE(test_vBaOC_nB2),
		TESTCASE(test_vBaVc_nB2),
		TESTCASE(test_vBaVC_nB2),
		TESTCASE(test_vBaNc_nB2),
		TESTCASE(test_vBaNC_nB2),

		TESTCASE(test_vBaoc_nB2_2),
		TESTCASE(test_vBaoC_nB2_2),
		TESTCASE(test_vBaOc_nB2_2),
		TESTCASE(test_vBaOC_nB2_2),
		TESTCASE(test_vBaVc_nB2_2),
		TESTCASE(test_vBaVC_nB2_2),
		TESTCASE(test_vBaNc_nB2_2),
		TESTCASE(test_vBaNC_nB2_2),

		TESTCASE(test_vBaoc_nW),
		TESTCASE(test_vBaoC_nW),
		TESTCASE(test_vBaOc_nW),
		TESTCASE(test_vBaOC_nW),
		TESTCASE(test_vBaVc_nW),
		TESTCASE(test_vBaVC_nW),
		TESTCASE(test_vBaNc_nW),
		TESTCASE(test_vBaNC_nW),

		TESTCASE(test_vBaoc_nW2),
		TESTCASE(test_vBaoC_nW2),
		TESTCASE(test_vBaOc_nW2),
		TESTCASE(test_vBaOC_nW2),
		TESTCASE(test_vBaVc_nW2),
		TESTCASE(test_vBaVC_nW2),
		TESTCASE(test_vBaNc_nW2),
		TESTCASE(test_vBaNC_nW2),

		TESTCASE(test_vBaoc_nW2_2),
		TESTCASE(test_vBaoC_nW2_2),
		TESTCASE(test_vBaOc_nW2_2),
		TESTCASE(test_vBaOC_nW2_2),
		TESTCASE(test_vBaVc_nW2_2),
		TESTCASE(test_vBaVC_nW2_2),
		TESTCASE(test_vBaNc_nW2_2),
		TESTCASE(test_vBaNC_nW2_2),

		/* testcases3 */
		TESTCASE(test_vBfOC_nBMoc),
		TESTCASE(test_vBfOC_nBMOC),
		TESTCASE(test_vBfOC_nBMVC),
		TESTCASE(test_vBfOC_nBMNC),

		TESTCASE(test_vBaOC_nBMoc),
		TESTCASE(test_vBaOC_nBMOC),
		TESTCASE(test_vBaOC_nBMVC),
		TESTCASE(test_vBaOC_nBMNC),

		TESTCASE(test_vBfOC_nWMoc),
		TESTCASE(test_vBfOC_nWMOC),
		TESTCASE(test_vBfOC_nWMVC),
		TESTCASE(test_vBfOC_nWMNC),

		TESTCASE(test_vBaOC_nWMoc),
		TESTCASE(test_vBaOC_nWMOC),
		TESTCASE(test_vBaOC_nWMVC),
		TESTCASE(test_vBaOC_nWMNC),

		TESTCASE(test_vBfVC_nStMOC),
		TESTCASE(test_vBfVC_nWMOC),
		TESTCASE(test_vBfVC_nBMOC),

		TESTCASE(test_vBfVC_nStMVC),
		TESTCASE(test_vBfVC_nWMVC),
		TESTCASE(test_vBfVC_nBMVC),

		/* testcases4 */
		TESTCASE(test_vWaoc_nSt),
		TESTCASE(test_vWaoc_nB),
		TESTCASE(test_vWaoc_nW),
		TESTCASE(test_vWaoc_nBN),

		TESTCASE(test_vWaVC_nSt),
		TESTCASE(test_vWaVC_nB),
		TESTCASE(test_vWaVC_nW),
		TESTCASE(test_vWaVC_nBN),

		TESTCASE(test_vWaOC_nSt),
		TESTCASE(test_vWaOC_nB),
		TESTCASE(test_vWaOC_nW),
		TESTCASE(test_vWaOC_nBN),

		TESTCASE(test_vWTaoc_nSt),
		TESTCASE(test_vWTaoc_nB),
		TESTCASE(test_vWTaoc_nW),
		TESTCASE(test_vWTaoc_nBN),

		TESTCASE(test_vWTaVC_nSt),
		TESTCASE(test_vWTaVC_nB),
		TESTCASE(test_vWTaVC_nW),
		TESTCASE(test_vWTaVC_nBN),

		TESTCASE(test_vWTaOC_nSt),
		TESTCASE(test_vWTaOC_nB),
		TESTCASE(test_vWTaOC_nW),
		TESTCASE(test_vWTaOC_nBN),

		TESTCASE(test_vWaoc_nBMVC),
		TESTCASE(test_vWaoc_nBMOC),
		TESTCASE(test_vWaoc_nBMoc),

		TESTCASE(test_vWaoc_nWMVC),
		TESTCASE(test_vWaoc_nWMOC),
		TESTCASE(test_vWaoc_nWMoc),

		TESTCASE(test_vWaoc_nBNMVC),
		TESTCASE(test_vWaoc_nBNMOC),
		TESTCASE(test_vWaoc_nBNMoc),

		TESTCASE(test_vWaVC_nBMVC),
		TESTCASE(test_vWaVC_nBMOC),
		TESTCASE(test_vWaVC_nBMoc),

		TESTCASE(test_vWaVC_nWMVC),
		TESTCASE(test_vWaVC_nWMOC),
		TESTCASE(test_vWaVC_nWMoc),

		TESTCASE(test_vWaVC_nBNMVC),
		TESTCASE(test_vWaVC_nBNMOC),
		TESTCASE(test_vWaVC_nBNMoc),

		TESTCASE(test_vWTaoc_nBMVC),
		TESTCASE(test_vWTaoc_nBMOC),
		TESTCASE(test_vWTaoc_nBMoc),

		TESTCASE(test_vWTaoc_nWMVC),
		TESTCASE(test_vWTaoc_nWMOC),
		TESTCASE(test_vWTaoc_nWMoc),

		TESTCASE(test_vWTaoc_nBNMVC),
		TESTCASE(test_vWTaoc_nBNMOC),
		TESTCASE(test_vWTaoc_nBNMoc),

		TESTCASE(test_vWTaVC_nBMVC),
		TESTCASE(test_vWTaVC_nBMOC),
		TESTCASE(test_vWTaVC_nBMoc),

		TESTCASE(test_vWTaVC_nWMVC),
		TESTCASE(test_vWTaVC_nWMOC),
		TESTCASE(test_vWTaVC_nWMoc),

		TESTCASE(test_vWTaVC_nBNMVC),
		TESTCASE(test_vWTaVC_nBNMOC),
		TESTCASE(test_vWTaVC_nBNMoc),

		/* testcases5 */
		TESTCASE(test_vPm_nSt),
		TESTCASE(test_vPm_nP),
		TESTCASE(test_vPm_nBMoc),
		TESTCASE(test_vPm_nBMOC),
		TESTCASE(test_vPm_nBMVC),
		TESTCASE(test_vPm_nWMoc),
		TESTCASE(test_vPm_nWMOC),
		TESTCASE(test_vPm_nWMVC),
		TESTCASE(test_vPm_nZP),
		TESTCASE(test_vPm_nZBMVC),
		TESTCASE(test_vPm_nZWMVC),

		TESTCASE(test_vPTm_nSt),
		TESTCASE(test_vPTm_nPT),
		TESTCASE(test_vPTm_nBMoc),
		TESTCASE(test_vPTm_nBMOC),
		TESTCASE(test_vPTm_nBMVC),
		TESTCASE(test_vPTm_nWMoc),
		TESTCASE(test_vPTm_nWMOC),
		TESTCASE(test_vPTm_nWMVC),


		TESTCASE(test_vPMoc_nSt),
		TESTCASE(test_vPMoc_nP),
		TESTCASE(test_vPMoc_nBMoc),
		TESTCASE(test_vPMoc_nBMOC),
		TESTCASE(test_vPMoc_nBMVC),
		TESTCASE(test_vPMoc_nWMoc),
		TESTCASE(test_vPMoc_nWMOC),
		TESTCASE(test_vPMoc_nWMVC),

		TESTCASE(test_vPTMoc_nSt),
		TESTCASE(test_vPTMoc_nPT),
		TESTCASE(test_vPTMoc_nBMoc),
		TESTCASE(test_vPTMoc_nBMOC),
		TESTCASE(test_vPTMoc_nBMVC),
		TESTCASE(test_vPTMoc_nWMoc),
		TESTCASE(test_vPTMoc_nWMOC),
		TESTCASE(test_vPTMoc_nWMVC),

		TESTCASE(test_vPMOC_nSt),
		TESTCASE(test_vPMOC_nP),
		TESTCASE(test_vPMOC_nBMoc),
		TESTCASE(test_vPMOC_nBMOC),
		TESTCASE(test_vPMOC_nBMVC),
		TESTCASE(test_vPMOC_nWMoc),
		TESTCASE(test_vPMOC_nWMOC),
		TESTCASE(test_vPMOC_nWMVC),

		TESTCASE(test_vPTMOC_nSt),
		TESTCASE(test_vPTMOC_nPT),
		TESTCASE(test_vPTMOC_nBMoc),
		TESTCASE(test_vPTMOC_nBMOC),
		TESTCASE(test_vPTMOC_nBMVC),
		TESTCASE(test_vPTMOC_nWMoc),
		TESTCASE(test_vPTMOC_nWMOC),
		TESTCASE(test_vPTMOC_nWMVC),

		TESTCASE(test_vPMVC_nSt),
		TESTCASE(test_vPMVC_nP),
		TESTCASE(test_vPMVC_nBMoc),
		TESTCASE(test_vPMVC_nBMOC),
		TESTCASE(test_vPMVC_nBMVC),
		TESTCASE(test_vPMVC_nWMoc),
		TESTCASE(test_vPMVC_nWMOC),
		TESTCASE(test_vPMVC_nWMVC),

		TESTCASE(test_vPTMVC_nSt),
		TESTCASE(test_vPTMVC_nPT),
		TESTCASE(test_vPTMVC_nBMoc),
		TESTCASE(test_vPTMVC_nBMOC),
		TESTCASE(test_vPTMVC_nBMVC),
		TESTCASE(test_vPTMVC_nWMoc),
		TESTCASE(test_vPTMVC_nWMOC),
		TESTCASE(test_vPTMVC_nWMVC),

		/* testcases6 */
		TESTCASE(test_vS_nSt),
		TESTCASE(test_vS_nBMoc),
		TESTCASE(test_vS_nBMOC),
		TESTCASE(test_vS_nBMVC),
		TESTCASE(test_vS_nWMoc),
		TESTCASE(test_vS_nWMOC),
		TESTCASE(test_vS_nWMVC),
		TESTCASE(test_vS_nWTMoc),
		TESTCASE(test_vS_nWTMOC),
		TESTCASE(test_vS_nWTMVC),
		TESTCASE(test_vS_nST),

		TESTCASE(test_vZ_nSt),
		TESTCASE(test_vZ_nBMVC),
		TESTCASE(test_vZ_nWMVC),
		TESTCASE(test_vZ_nWMoc),

		TESTCASE(test_cleanup)
};

static UDATA testPassed = 0, testTotal = 0, ignoredFailures = 0, expectedFailures = 0;

static UDATA
countTest(UDATA status)
{
	testTotal++;
	if (0 == (status & FAILURE_MASK)) {
		testPassed++;
		if (status & IGNORED_FAILURE_MASK) {
			ignoredFailures++;
		}
		if (status & EXPECTED_FAILURE_MASK) {
			expectedFailures++;
		}
	}
	return status;
}

static UDATA
runTest(JNIEnv *env, const char *testname, UDATA (*func)(JNIEnv *, BOOLEAN), BOOLEAN ignoreExpectedFailures)
{
	PORT_ACCESS_FROM_ENV(env);
	UDATA retVal;

	j9tty_printf(PORTLIB, "Test %d. %s started\n", testTotal+1, testname);
	retVal = func(env, ignoreExpectedFailures);
	j9tty_printf(PORTLIB, "... %s: %s %08x\n\n", testname, (retVal & FAILURE_MASK)? "failed": "passed", retVal);

	return countTest(retVal);
}

static void
printFailedTests(JNIEnv *env)
{
	PORT_ACCESS_FROM_ENV(env);
	UDATA numTests;
	UDATA i;

	numTests = sizeof(testCases) / sizeof(testCases[0]);
	for (i = 0; i < numTests; i++) {
		if (testCases[i].retVal & FAILURE_MASK) {
			j9tty_printf(PORTLIB, "Test %d/%d Failed: %s Code: %08x\n", i+1, numTests, testCases[i].name, testCases[i].retVal);
		}
	}
}

static UDATA
runAllTests(JNIEnv *env)
{
	PORT_ACCESS_FROM_ENV(env);
	BOOLEAN ignoreExpectedFailures = TRUE;
	UDATA rc;
	UDATA numTests;
	UDATA i;

	numTests = sizeof(testCases) / sizeof(testCases[0]);
	for (i = 0; i < numTests; i++) {
		testCases[i].retVal = runTest(env, testCases[i].name, testCases[i].func, ignoreExpectedFailures);
	}

	j9tty_printf(PORTLIB, "%d/%d Tests passed. %d Failures ignored. %d Failures expected.\n",
			testPassed, testTotal, ignoredFailures, expectedFailures);

	if (testPassed == testTotal) {
		j9tty_printf(PORTLIB, "ALL TESTS COMPLETED AND PASSED\n");
		rc = 0;
	} else {
		j9tty_printf(PORTLIB, "TESTS FAILED\n");
	}

	if ((testPassed != testTotal) || (ignoredFailures > 0)) {
		printTestData(env);
	}
	cleanupTestData(env);

	if (testPassed != testTotal) {
		printFailedTests(env);
		rc = 1;
	}

	return rc;
}

static UDATA
createVMArgs(J9PortLibrary *portLib, int argc, char **argv, UDATA handle, jint version,
		jboolean ignoreUnrecognized, JavaVMInitArgs *vm_args,
		jint (JNICALL **CreateJavaVM)(JavaVM **, JNIEnv **, JavaVMInitArgs *))
{
	int i, j;
	JavaVMOption *options;
	PORT_ACCESS_FROM_PORT(portLib);

	options = j9mem_allocate_memory(argc * sizeof(*options), OMRMEM_CATEGORY_THREADS);
	if (options == NULL) {
		j9tty_err_printf (PORTLIB, "Failed to allocate memory for options\n");
		return 1;
	}

	for (i = 1, j = 0; i < argc; i++) {
		options[j].optionString = argv[i];
		options[j].extraInfo = NULL;
		j++;
	}
	options[j].optionString = "-Xint";
	options[j].extraInfo = NULL;
	j++;

	vm_args->version = version;
	vm_args->nOptions = j;
	vm_args->options = options;
	vm_args->ignoreUnrecognized = ignoreUnrecognized;

	if (j9sl_lookup_name(handle, "JNI_CreateJavaVM", (UDATA*)CreateJavaVM, "iLLL")) {
		j9tty_printf (PORTLIB, "Failed to find JNI_CreateJavaVM in DLL\n");
		return 1;
	}

	return 0;
}

static void
freeVMArgs(J9PortLibrary *portLib, JavaVMInitArgs *vm_args)
{
	PORT_ACCESS_FROM_PORT(portLib);
	j9mem_free_memory(vm_args->options);
}

UDATA
signalProtectedMain(struct J9PortLibrary *portLibrary, void *arg)
{
	struct j9cmdlineOptions * args = arg;
	int argc = args->argc;
	char **argv = args->argv;
	UDATA handle;
	UDATA rc;
	JavaVMInitArgs vm_args;
	JavaVM *jvm;
	JNIEnv *env;
	PORT_ACCESS_FROM_PORT(args->portLibrary);
	jint (JNICALL *CreateJavaVM)(JavaVM **, JNIEnv **, JavaVMInitArgs *);
	const char * j9vmDir = "j9vm";
	const char * jvmLibName = "jvm";
	char * exename = NULL;
	char libjvmPath[THRTEST_MAX_PATH];
	char * libjvmPathOffset = NULL;
	int lastDirSep = -1;
	int found = 0;
	int i = 0;
	const char * directorySep = J9JAVA_DIR_SEPARATOR;
	void *vmOptionsTable = NULL;

#ifdef J9VM_OPT_REMOTE_CONSOLE_SUPPORT
	remoteConsole_parseCmdLine(PORTLIB, argc-1, argv);
#endif

#ifdef J9VM_OPT_MEMORY_CHECK_SUPPORT
	/* This should happen before anybody allocates memory.
	 * Otherwise, shutdown will not work properly.
	 */
	memoryCheck_parseCmdLine(PORTLIB, argc-1, argv);
#endif

	vmOptionsTableInit(PORTLIB, &vmOptionsTable, 15);
	if (NULL == vmOptionsTable) {
		return 1;
	}

	if ((vmOptionsTableAddOption(&vmOptionsTable, "_port_library", (void *) PORTLIB) != J9CMDLINE_OK)
		|| (vmOptionsTableAddOption(&vmOptionsTable, "-Xint", NULL) != J9CMDLINE_OK)
#if defined (J9VM_GC_REALTIME)
		/*Soft realtime options*/
		|| (vmOptionsTableAddOption(&vmOptionsTable, "-Xgcpolicy:metronome", NULL) != J9CMDLINE_OK)
#endif /* defined (J9VM_GC_REALTIME) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
		|| (vmOptionsTableAddOption(&vmOptionsTable, "-Xcompressedrefs", NULL) != J9CMDLINE_OK)
#else
		|| (vmOptionsTableAddOption(&vmOptionsTable, "-Xnocompressedrefs", NULL) != J9CMDLINE_OK)
#endif
		|| (vmOptionsTableAddOption(&vmOptionsTable, "-Xits0", NULL) != J9CMDLINE_OK)) {
		return 1;
	}

	if (vmOptionsTableAddExeName(&vmOptionsTable, argv[0]) != J9CMDLINE_OK) {
		return 1;
	}

	vm_args.version = JNI_VERSION_1_2;
	vm_args.nOptions = vmOptionsTableGetCount(&vmOptionsTable);
	vm_args.options = vmOptionsTableGetOptions(&vmOptionsTable);
	vm_args.ignoreUnrecognized = JNI_FALSE;

	if (cmdline_fetchRedirectorDllDir(arg, libjvmPath) == FALSE) {
		printf("Please provide Java home directory (eg. /home/[user]/sdk/jre)\n");
		return 1;
	}
	strcat(libjvmPath, jvmLibName);

	if (j9sl_open_shared_library(libjvmPath, &handle, J9PORT_SLOPEN_DECORATE)) {
		j9tty_printf(PORTLIB, "Failed to open JVM DLL: %s (%s)\n", libjvmPath,
				j9error_last_error_message());
		return 1;
	}

	if (createVMArgs(PORTLIB, argc, argv, handle, JNI_VERSION_1_6, JNI_FALSE, &vm_args,
				&CreateJavaVM)) {
		return 1;
	}

	if (CreateJavaVM(&jvm, &env, &vm_args)) {
		j9tty_printf (PORTLIB, "Failed to create JVM\n");
		return 1;
	}

	rc = runAllTests(env);

	j9mem_free_memory(vm_args.options);

	if ((*jvm)->DestroyJavaVM(jvm)) {
		j9tty_printf (PORTLIB, "Failed to destroy JVM\n");
		return 1;
	}

	if (j9sl_close_shared_library(handle)) {
		j9tty_printf(PORTLIB, "Failed to close JVM DLL: %s\n", J9_VM_DLL_NAME);
		return 1;
	}

	return rc;
}
