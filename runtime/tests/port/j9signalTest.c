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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/*
 * $RCSfile: j9signalTest.c,v $
 * $Revision: 1.66 $
 * $Date: 2013-03-21 14:42:10 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library signal handling.
 * 
 * Exercise the API for port library signal handling.  These functions 
 * can be found in the file @ref j9signal.c  
 * 
 */
 
#if !defined(WIN32) 
#include <signal.h>
#endif

#if defined(J9ZOS390)
#include "atoe.h"
#endif

#ifdef J9ZTPF
#include <tpf/sysapi.h>
#endif
#include <string.h>
#include <stdlib.h>

#include "j9cfg.h"
#include "fltconst.h"
#include "omrthread.h"


#include "testHelpers.h"
#include "testProcessHelpers.h"
#include "j9port.h"

#if !defined(WIN32)
#define J9SIGNAL_TEST_RUN_ASYNC_UNIX_TESTS
#endif

#define SIG_TEST_SIZE_EXENAME 1024


#if defined(J9SIGNAL_TEST_RUN_ASYNC_UNIX_TESTS)

struct {
	U_32 portLibSignalNo;
	char *portLibSignalString ;
	int unixSignalNo;
	char *unixSignalString;
} signalMap[] =
			{
					{J9PORT_SIG_FLAG_SIGQUIT, "J9PORT_SIG_FLAG_SIGQUIT", SIGQUIT, "SIGQUIT"}
					,{J9PORT_SIG_FLAG_SIGABRT, "J9PORT_SIG_FLAG_SIGABRT", SIGABRT, "SIGABRT"}
					,{J9PORT_SIG_FLAG_SIGTERM, "J9PORT_SIG_FLAG_SIGTERM", SIGTERM, "SIGTERM"}
#if defined(AIXPPC)
					,{J9PORT_SIG_FLAG_SIGRECONFIG, "J9PORT_SIG_FLAG_SIGRECONFIG", SIGRECONFIG, "SIGRECONFIG"}
#endif
					/* {J9PORT_SIG_FLAG_SIGINT, "J9PORT_SIG_FLAG_SIGINT", SIGINT, "SIGINT"} */
			};

typedef struct AsyncHandlerInfo {
	U_32 expectedType;
	const char* testName;
	omrthread_monitor_t *monitor;
	U_32 controlFlag;
} AsyncHandlerInfo;

static UDATA asyncTestHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData);
static void injectUnixSignal(struct J9PortLibrary *portLibrary, int pid, int unixSignal);

#endif /* J9SIGNAL_TEST_RUN_ASYNC_UNIX_TESTS */

typedef struct SigProtectHandlerInfo {
	U_32 expectedType;
	UDATA returnValue;
	const char* testName;
} SigProtectHandlerInfo;


static U_32 portTestOptionsGlobal;

static I_32 j9sig_test0(struct J9PortLibrary *portLibrary);
static I_32 j9sig_test1(struct J9PortLibrary *portLibrary);
static I_32 j9sig_test2(struct J9PortLibrary *portLibrary);
static I_32 j9sig_test3(struct J9PortLibrary *portLibrary);
static I_32 j9sig_test4(struct J9PortLibrary *portLibrary);
static I_32 j9sig_test5(struct J9PortLibrary *portLibrary);
static I_32 j9sig_test6(struct J9PortLibrary *portLibrary);
static I_32 j9sig_test7(struct J9PortLibrary *portLibrary);
static I_32 j9sig_test8(struct J9PortLibrary *portLibrary);

#if defined(J9SIGNAL_TEST_RUN_ASYNC_UNIX_TESTS)

static I_32 j9sig_test_async_unix_handler(struct J9PortLibrary *portLibrary, char *exeName);

/*
 * Sets  the controlFlag to 1 such that we can test that it was actually invoked
 *
 * Synchronizes using AsyncHandlerInfo->monitor
 *
 * */
static UDATA
asyncTestHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* handlerInfo, void* userData)
{
	AsyncHandlerInfo* info = (AsyncHandlerInfo *) userData;
	const char* testName = info->testName;
	omrthread_monitor_t monitor = *(info->monitor);

	PORT_ACCESS_FROM_PORT(portLibrary);

	omrthread_monitor_enter(monitor);

	outputComment(portLibrary, "\t\tasyncTestHandler invoked (type = 0x%x)\n", gpType);
	if (info->expectedType != gpType) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "asyncTestHandler -- incorrect type. Expecting 0x%x, got 0x%x\n", info->expectedType, gpType);
	}

	info->controlFlag = 1;
	omrthread_monitor_notify(monitor);
	omrthread_monitor_exit(monitor);
	return 0;

}

static void
injectUnixSignal(struct J9PortLibrary *portLibrary, int pid, int unixSignal) {

	/* this is run by the child process. To see the tty_printf output on the console, set J9PORT_PROCESS_INHERIT_STDOUT | J9PORT_PROCESS_INHERIT_STDIN
	 * in the options passed into j9process_create() in launchChildProcess (in testProcessHelpers.c). 
	 */
	PORT_ACCESS_FROM_PORT(portLibrary);
	j9tty_printf(PORTLIB, "\t\tCHILD:	 calling kill: %i %i\n", pid, unixSignal);
	kill(pid, unixSignal);
	return;
}

#endif /* J9SIGNAL_TEST_RUN_ASYNC_UNIX_TESTS */

/**
 * Verify port library signal handling.
 * 
 * @param[in] portLibrary 	the port library under test
 * @param[in] exeName 		the executable that invoked the test
 * @param[in  arg			either NULL or of the form j9sig_injectSignal_<pid>_<signal>
 * 
 * @return 0 on success, -1 on failure
 */
I_32 
j9sig_runTests(struct J9PortLibrary *portLibrary, char* exeName, char* argument)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc;
	
	portTestOptionsGlobal = j9sig_get_options();

#if defined(J9SIGNAL_TEST_RUN_ASYNC_UNIX_TESTS)
	if (argument != NULL) {
		if (1 == startsWith(argument, "j9sig_injectSignal")) {
			char *scratch;
			int pid, unixSignal;
			char *comma;
			int numCharsBeforeComma;
			char *strTokSavePtr;

			/* consume "j9sig" */
			(void)strtok_r(argument, "_", &strTokSavePtr);

			/* consume "injectSignal" */
			(void)strtok_r(NULL, "_", &strTokSavePtr);

			/* consume PID */
			scratch = strtok_r(NULL, "_", &strTokSavePtr);
			pid = atoi(scratch);

			/* consume signal number */
			scratch = strtok_r(NULL, "_", &strTokSavePtr);
			unixSignal = atoi(scratch);

			injectUnixSignal(portLibrary, pid, unixSignal);
			return TEST_PASS /* doesn't matter what we return here */;
		}
	}
#endif

	/* Display unit under test */
	HEADING(portLibrary, "Signal test");

	rc = j9sig_test0(portLibrary);
	rc |= j9sig_test1(portLibrary);
	rc |= j9sig_test2(portLibrary);
	rc |= j9sig_test3(portLibrary);
	rc |= j9sig_test4(portLibrary);
	rc |= j9sig_test5(portLibrary);
	rc |= j9sig_test6(portLibrary);
	rc |= j9sig_test7(portLibrary);
	rc |= j9sig_test8(portLibrary);

#if defined(J9SIGNAL_TEST_RUN_ASYNC_UNIX_TESTS)
	rc |= j9sig_test_async_unix_handler(portLibrary, exeName);
#endif

	/* Output results */
	j9tty_printf(PORTLIB, "\nSignal test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;
}

/*
 * Test that a named register has the correct type, and exists correctly.
 * If platform is not NULL and is not the current platform, the register MUST be undefined.
 * Otherwise the register must be expectedKind, or it may be undefined if optional is TRUE.
 */
static void
validatePlatformRegister(struct J9PortLibrary* portLibrary, void* gpInfo, U_32 category, I_32 index, U_32 expectedKind, UDATA optional, const char* platform, const char* testName)
{
	U_32 infoKind;
	void* value;
	const char* name;
	PORT_ACCESS_FROM_PORT(portLibrary);
		
	infoKind = j9sig_info(gpInfo, category, index, &name, &value);
	if ( (platform == NULL) || (strcmp(platform, j9sysinfo_get_CPU_architecture()) == 0)) {
		if (infoKind == J9PORT_SIG_VALUE_UNDEFINED) {
			if (optional == FALSE) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Required platform register %d is undefined\n", index);
			}
		} else if (infoKind != expectedKind) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Platform register %d type is not %u. It is %u\n", index, expectedKind, infoKind);
		}
	} else {
		if (infoKind != J9PORT_SIG_VALUE_UNDEFINED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "J9PORT_SIG_GPR_X86_EDI type is not J9PORT_SIG_VALUE_UNDEFINED. It is %u\n", infoKind);
		}
	}
}

void
validateGPInfo(struct J9PortLibrary* portLibrary, U_32 gpType, j9sig_handler_fn handler, void* gpInfo, const char* testName)
{
	U_32 category;
	void* value;
	const char* name;
	U_32 index;
	U_32 infoKind;
	const char* arch;
	PORT_ACCESS_FROM_PORT(portLibrary);
	
	for (category=0 ; category<J9PORT_SIG_NUM_CATEGORIES ; category++) {
		U_32 infoCount;
		
		infoCount = j9sig_info_count(gpInfo, category) ;

		for (index=0 ; index < infoCount ; index++) {
			infoKind = j9sig_info(gpInfo, category, index, &name, &value);
			
			switch (infoKind) {
				case J9PORT_SIG_VALUE_16:
					outputComment(portLibrary, "info %u:%u: %s=%04X\n", category, index, name, *(U_16 *)value);
					break;
				case J9PORT_SIG_VALUE_32:
					outputComment(portLibrary, "info %u:%u: %s=%08.8x\n", category, index, name, *(U_32 *)value);
					break;
				case J9PORT_SIG_VALUE_64:
					outputComment(portLibrary, "info %u:%u: %s=%016.16llx\n", category, index, name, *(U_64 *)value);
					break;
				case J9PORT_SIG_VALUE_STRING:
					outputComment(portLibrary, "info %u:%u: %s=%s\n", category, index, name, (const char *)value);
					break;
				case J9PORT_SIG_VALUE_ADDRESS:
					outputComment(portLibrary, "info %u:%u: %s=%p\n", category, index, name, *(void**)value);
					break;
				case J9PORT_SIG_VALUE_FLOAT_64:
					/* make sure when casting to a float that we get least significant 32-bits. */
					outputComment(portLibrary, 
						"info %u:%u: %s=%016.16llx (f: %f, d: %e)\n", 
						category, index, name, 
						*(U_64 *)value, (float)LOW_U32_FROM_DBL_PTR(value), *(double *)value);
					break;
				case J9PORT_SIG_VALUE_128:
					{
						const U_128 * const v = (U_128 *) value;
						const U_64 h = v->high64;
						const U_64 l = v->low64;

						outputComment(portLibrary, "info %u:%u: %s=%016.16llx%016.16llx\n", category, index, name, h, l);
					}
					break;
				case J9PORT_SIG_VALUE_UNDEFINED:
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Undefined value in info (%u:%u, name=%s)\n", category, index, name);
					break;
			 	default:
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Unrecognized type in info (%u:%u, name=%s, type=%u)\n", category, index, name, infoKind);
					break;
			}					
		}
		
		infoKind = j9sig_info(gpInfo, category, index, &name, &value);
		if (infoKind != J9PORT_SIG_VALUE_UNDEFINED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected data after end of info (%u:%u, name=%s, type=%u)\n", category, index, name, infoKind);
		}
	}		
	
	/* test the known names */
	infoKind = j9sig_info(gpInfo, J9PORT_SIG_SIGNAL, J9PORT_SIG_SIGNAL_TYPE, &name, &value);
	if (infoKind != J9PORT_SIG_VALUE_32) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "J9PORT_SIG_SIGNAL_TYPE type is not J9PORT_SIG_VALUE_32. It is %u\n", infoKind);
	} else if ( *(U_32*)value != gpType) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "J9PORT_SIG_SIGNAL_TYPE is incorrect. Expecting %u, got %u\n", gpType, *(U_32*)value);
	}

	infoKind = j9sig_info(gpInfo, J9PORT_SIG_SIGNAL, J9PORT_SIG_SIGNAL_HANDLER, &name, &value);
	if (infoKind != J9PORT_SIG_VALUE_ADDRESS) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "J9PORT_SIG_SIGNAL_HANDLER type is not J9PORT_SIG_VALUE_ADDRESS. It is %u\n", infoKind);
	} else if ( *(j9sig_handler_fn*)value != handler) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "J9PORT_SIG_SIGNAL_HANDLER is incorrect. Expecting %p, got %p\n", handler, *(j9sig_handler_fn*)value);
	}

	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_MODULE, J9PORT_SIG_MODULE_NAME, J9PORT_SIG_VALUE_STRING, TRUE, NULL, testName);
	
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_SIGNAL, J9PORT_SIG_SIGNAL_ERROR_VALUE, J9PORT_SIG_VALUE_32, TRUE, NULL, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_SIGNAL, J9PORT_SIG_SIGNAL_ADDRESS, J9PORT_SIG_VALUE_ADDRESS, TRUE, NULL, testName);

	if (0 != (J9PORT_SIG_OPTIONS_ZOS_USE_CEEHDLR & portTestOptionsGlobal)) {
		/* The LE condition handler message number is a U_16 */
		validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_SIGNAL, J9PORT_SIG_SIGNAL_PLATFORM_SIGNAL_TYPE, J9PORT_SIG_VALUE_16, TRUE, NULL, testName);
	} else {
		validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_SIGNAL, J9PORT_SIG_SIGNAL_PLATFORM_SIGNAL_TYPE, J9PORT_SIG_VALUE_32, TRUE, NULL, testName);
	}

	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_SIGNAL, J9PORT_SIG_SIGNAL_INACCESSIBLE_ADDRESS, J9PORT_SIG_VALUE_ADDRESS, TRUE, NULL, testName);

	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_PC, J9PORT_SIG_VALUE_ADDRESS, FALSE, NULL, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_SP, J9PORT_SIG_VALUE_ADDRESS, TRUE, NULL, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_BP, J9PORT_SIG_VALUE_ADDRESS, TRUE, NULL, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_S390_BEA, J9PORT_SIG_VALUE_ADDRESS, TRUE, NULL, testName);


	/*
	 * x86 registers 
	 */
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_X86_EDI, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_X86, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_X86_ESI, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_X86, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_X86_EAX, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_X86, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_X86_EBX, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_X86, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_X86_ECX, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_X86, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_X86_EDX, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_X86, testName);
	
	/*
	 * AMD64 registers 
	 */
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_AMD64_RDI, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_HAMMER, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_AMD64_RSI, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_HAMMER, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_AMD64_RAX, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_HAMMER, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_AMD64_RBX, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_HAMMER, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_AMD64_RCX, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_HAMMER, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_AMD64_RDX, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_HAMMER, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_AMD64_R8, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_HAMMER, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_AMD64_R9, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_HAMMER, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_AMD64_R10, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_HAMMER, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_AMD64_R11, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_HAMMER, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_AMD64_R12, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_HAMMER, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_AMD64_R13, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_HAMMER, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_AMD64_R14, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_HAMMER, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_GPR, J9PORT_SIG_GPR_AMD64_R15, J9PORT_SIG_VALUE_ADDRESS, FALSE, J9PORT_ARCH_HAMMER, testName);

	/*
	 * PowerPC registers 
	 */

	/* these registers are valid on both PPC32 and PPC64, so use whichever we're running on if we are on a PPC */
	arch = J9PORT_ARCH_PPC;
	if (0 == strcmp(J9PORT_ARCH_PPC64, j9sysinfo_get_CPU_architecture())) {
		arch = J9PORT_ARCH_PPC64;
	} else if (0 == strcmp(J9PORT_ARCH_PPC64LE, j9sysinfo_get_CPU_architecture())) {
		arch = J9PORT_ARCH_PPC64LE;
	}

	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_POWERPC_LR, J9PORT_SIG_VALUE_ADDRESS, FALSE, arch, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_POWERPC_MSR, J9PORT_SIG_VALUE_ADDRESS, TRUE, arch, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_POWERPC_CTR, J9PORT_SIG_VALUE_ADDRESS, TRUE, arch, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_POWERPC_CR, J9PORT_SIG_VALUE_ADDRESS, TRUE, arch, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_POWERPC_FPSCR, J9PORT_SIG_VALUE_ADDRESS, TRUE, arch, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_POWERPC_XER, J9PORT_SIG_VALUE_ADDRESS, TRUE, arch, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_POWERPC_DAR, J9PORT_SIG_VALUE_ADDRESS, TRUE, arch, testName);
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_POWERPC_DSIR, J9PORT_SIG_VALUE_ADDRESS, TRUE, arch, testName);
	
	/* this one is PPC32 only */
	validatePlatformRegister(portLibrary, gpInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_POWERPC_MQ, J9PORT_SIG_VALUE_ADDRESS, TRUE, J9PORT_ARCH_PPC, testName);
}

static UDATA
sampleFunction(J9PortLibrary* portLibrary, void* arg)
{
	return 8096;
}


static UDATA 
simpleHandlerFunction(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* handler_arg)
{
	SigProtectHandlerInfo* info = handler_arg;
	const char* testName = info->testName;
	
	outputComment(portLibrary, "simpleHandlerFunction invoked (type = 0x%x)\n", gpType);

	if (info->expectedType != gpType) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "simpleHandlerFunction -- incorrect type. Expecting 0x%x, got 0x%x\n", info->expectedType, gpType);
	}
	validateGPInfo(portLibrary, gpType, simpleHandlerFunction, gpInfo, testName);

	return info->returnValue;
}

static UDATA 
currentSigNumHandlerFunction(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* handler_arg)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	SigProtectHandlerInfo* info = handler_arg;
	IDATA currentSigNum = j9sig_get_current_signal();
	const char* testName = info->testName;

	if (currentSigNum < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "currentSigNumHandlerFunction -- j9sig_get_current_signal call failed\n");
		return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
	}
	if (gpType != (U_32) currentSigNum) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "currentSigNumHandlerFunction -- unexpected value for current signal. Expecting 0x%x, got 0x%x\n",gpType, (U_32)currentSigNum);
		return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
	}
	return J9PORT_SIG_EXCEPTION_RETURN;
}

static UDATA 
crashingHandlerFunction(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* handler_arg)
{
	const char* testName = handler_arg;
	static int recursive = 0;
	
	outputComment(portLibrary, "crashingHandlerFunction invoked (type = 0x%x)\n", gpType);

	if (recursive++) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "handler invoked recursively\n");
		return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
	}

	raiseSEGV(portLibrary, handler_arg);
	
	outputErrorMessage(PORTTEST_ERROR_ARGS, "unreachable\n");

	return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
}

static UDATA
nestedTestCatchSEGV(J9PortLibrary* portLibrary, void* arg)
{
	U_32 flags = J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGSEGV;
	const char* testName = arg;
	int i;
	U_32 protectResult;
	UDATA result;
	SigProtectHandlerInfo handlerInfo;
	PORT_ACCESS_FROM_PORT(portLibrary);
	
	if (!j9sig_can_protect(flags)) {
		operationNotSupported(portLibrary, "the specified signals are");
	} else {
		for (i = 0; i < 3; i++) {
			handlerInfo.expectedType = J9PORT_SIG_FLAG_SIGSEGV;
			handlerInfo.returnValue = J9PORT_SIG_EXCEPTION_RETURN;
			handlerInfo.testName = testName;
			protectResult = j9sig_protect(
				raiseSEGV, (void*)testName, 
				simpleHandlerFunction, &handlerInfo, 
				flags, 
				&result);
			
			if (protectResult != J9PORT_SIG_EXCEPTION_OCCURRED) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected J9PORT_SIG_EXCEPTION_OCCURRED return value\n");
			}
			
			if (result != 0) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 in *result\n");
			}
		}
	}
	
	return 8096;
}

static UDATA
nestedTestCatchNothing(J9PortLibrary* portLibrary, void* arg)
{
	U_32 flags = 0;
	const char* testName = arg;
	UDATA result;
	SigProtectHandlerInfo handlerInfo;
	PORT_ACCESS_FROM_PORT(portLibrary);
	
	if (!j9sig_can_protect(flags)) {
		operationNotSupported(portLibrary, "the specified signals are");
	} else {
		handlerInfo.expectedType = 0;
		handlerInfo.returnValue = J9PORT_SIG_EXCEPTION_RETURN;
		handlerInfo.testName = testName;
		(void)j9sig_protect(
			raiseSEGV, (void*)testName, 
			simpleHandlerFunction, &handlerInfo, 
			flags, 
			&result);

		outputErrorMessage(PORTTEST_ERROR_ARGS, "this should be unreachable\n");
	}
	
	return 8096;
}

static UDATA
nestedTestCatchAndIgnore(J9PortLibrary* portLibrary, void* arg)
{
	U_32 flags = J9PORT_SIG_FLAG_SIGSEGV;
	const char* testName = arg;
	UDATA result;
	SigProtectHandlerInfo handlerInfo;
	PORT_ACCESS_FROM_PORT(portLibrary);
	
	if (!j9sig_can_protect(flags)) {
		operationNotSupported(portLibrary, "the specified signals are");
	} else {
		handlerInfo.expectedType = J9PORT_SIG_FLAG_SIGSEGV;
		handlerInfo.returnValue = J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		handlerInfo.testName = testName;
		(void)j9sig_protect(
			raiseSEGV, (void*)testName, 
			simpleHandlerFunction, &handlerInfo, 
			flags, 
			&result);

		outputErrorMessage(PORTTEST_ERROR_ARGS, "this should be unreachable\n");
	}
	
	return 8096;
}

static UDATA
nestedTestCatchAndContinueSearch(J9PortLibrary* portLibrary, void* arg)
{
	U_32 flags = J9PORT_SIG_FLAG_SIGSEGV | J9PORT_SIG_FLAG_MAY_RETURN;
	const char* testName = arg;
	U_32 protectResult;
	UDATA result;
	SigProtectHandlerInfo handlerInfo;
	PORT_ACCESS_FROM_PORT(portLibrary);

	if (!j9sig_can_protect(flags)) {
		operationNotSupported(portLibrary, "the specified signals are");
	} else {
		handlerInfo.expectedType = J9PORT_SIG_FLAG_SIGSEGV;
		handlerInfo.returnValue = J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		handlerInfo.testName = testName;
		protectResult = j9sig_protect(
				raiseSEGV, (void*)testName,
				simpleHandlerFunction, &handlerInfo,
				flags,
				&result);
		if (protectResult != J9PORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected J9PORT_SIG_EXCEPTION_OCCURRED return value\n");
		}
	}

	return 0;
}

static UDATA
nestedTestCatchAndCrash(J9PortLibrary* portLibrary, void* arg)
{
	U_32 flags = J9PORT_SIG_FLAG_SIGSEGV;
	const char* testName = arg;
	UDATA result;
	PORT_ACCESS_FROM_PORT(portLibrary);
	
	if (!j9sig_can_protect(flags)) {
		operationNotSupported(portLibrary, "the specified signals are");
	} else {
		(void)j9sig_protect(
			raiseSEGV, (void*)testName, 
			crashingHandlerFunction, (void*)testName, 
			flags, 
			&result);

		outputErrorMessage(PORTTEST_ERROR_ARGS, "this should be unreachable\n");
	}
	
	return 8096;
}

/*
 * Test that we can protect against the flags:
 * 	0,
 * 	0 | J9PORT_SIG_FLAG_MAY_RETURN,
 * 	0 | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION,
 * 	0 | J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION
 *
*/
static I_32
j9sig_test0(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sig_test0";
	
	reportTestEntry(portLibrary, testName);

	if (!j9sig_can_protect(0)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->can_protect() must always support 0\n");
	}

	if (!j9sig_can_protect(J9PORT_SIG_FLAG_MAY_RETURN)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->can_protect() must always support 0 | J9PORT_SIG_FLAG_MAY_RETURN\n");
	}



	if (!j9sig_can_protect(J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->can_protect() must always support 0 | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION\n");
# if defined(J9ZOS390)
		/* z/OS does not always support this, however, pltest should never be run under a configuration that does not support it. 
		 * See implementation of j9sig_can_proctect() and j9sig_startup() */
		j9tty_printf(PORTLIB, "J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION is not supported if the default settings for TRAP(ON,SPIE) have been changed. The should both be set to 1\n");
#endif	
	}

	
	if (!j9sig_can_protect(J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->can_protect() must always support 0 | J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION\n");
# if defined(J9ZOS390)
		j9tty_printf(PORTLIB, "J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION is not supported if the default settings for TRAP(ON,SPIE) have been changed. The should both be set to 1\n");
#endif	

	}
	
	return reportTestExit(portLibrary, testName);
}


/*
 * Test protecting (and running) a function that does not fault
 * 
*/
static I_32
j9sig_test1(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA result = 1;
	U_32 protectResult;
	const char* testName = "j9sig_test1";
	SigProtectHandlerInfo handlerInfo;
	
	reportTestEntry(portLibrary, testName);

	if (!j9sig_can_protect(0)) {
		operationNotSupported(portLibrary, "the specified signals are");
	} else {

		/* test running a function which does not fault */
		handlerInfo.expectedType = 0;
		handlerInfo.returnValue = J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
		handlerInfo.testName = testName;
		protectResult = j9sig_protect(
			sampleFunction, NULL, 
			simpleHandlerFunction, &handlerInfo, 
			0, /* protect from nothing */
			&result);
		
		if (protectResult != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 return value\n");
		}
		
		if (result != 8096) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- protected function did not execute correctly\n");
		}
	}
	
	return reportTestExit(portLibrary, testName);
}

/* 
 * Test protecting (and running) a function that raises a segv 
 * 
*/
static I_32
j9sig_test2(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA result = 1;
	U_32 protectResult;
	U_32 flags = J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGSEGV;
	const char* testName = "j9sig_test2";
	SigProtectHandlerInfo handlerInfo;

	reportTestEntry(portLibrary, testName);

	if (!j9sig_can_protect(flags)) {
		operationNotSupported(portLibrary, "the specified signals are");
	} else {
			
		/* test running a function that raises a segv */
		handlerInfo.expectedType = J9PORT_SIG_FLAG_SIGSEGV;
		handlerInfo.returnValue = J9PORT_SIG_EXCEPTION_RETURN;
		handlerInfo.testName = testName;
		protectResult = j9sig_protect(
			raiseSEGV, (void*)testName, 
			simpleHandlerFunction, &handlerInfo, 
			flags, 
			&result);
		
		if (protectResult != J9PORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected J9PORT_SIG_EXCEPTION_OCCURRED return value\n");
		}
		
		if (result != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 in *result\n");
		}
	}
	
	return reportTestExit(portLibrary, testName);
}

/*
 * Test protecting Function A against a SEGV with handler HA. A in turn protects Function B
 * against a SEGV with handler HB. Function B raises a sigsegv. HB should handle it, returning 
 * J9PORT_SIG_EXCEPTION_RETURN, control ends up after the call to j9sig_protect in B.
 * B then returns normally to A and then j9sigTest3.
 * A checks:
 * 	j9sig_protect returns J9PORT_SIG_EXCEPTION_OCCURRED
 *  *result parameter to j9sig_protect call is 0 after call.
 * 	   
 * 		->SEGV
 * 		|	 |
 * 		|	 |
 * 		|  	 -->HB
 * 		B		|
 *  	^   ---<-
 * 		|	|
 * 		A<--	HA
 * 		^
 * 		|
 * 		|
 *	j9sig_test3
*/
static I_32
j9sig_test3(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA result = 1;
	U_32 protectResult;
	U_32 flags = J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGSEGV;
	const char* testName = "j9sig_test3";
	SigProtectHandlerInfo handlerInfo;

	reportTestEntry(portLibrary, testName);

	if (!j9sig_can_protect(flags)) {
		operationNotSupported(portLibrary, "the specified signals are");
	} else {
		handlerInfo.expectedType = 0;
		handlerInfo.returnValue = 0;
		handlerInfo.testName = testName;
		protectResult = j9sig_protect(
			nestedTestCatchSEGV, (void*)testName, 
			simpleHandlerFunction, &handlerInfo, 
			flags, 
			&result);
		
		if (protectResult != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 return value\n");
		}
		
		if (result != 8096) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 8096 in *result\n");
		}
	}
	
	return reportTestExit(portLibrary, testName);
}

/*
 * Test protecting Function A against a SEGV with handler HA. A in turn protects Function B against 
 * nothing with handler HB. Function B raises a sigsegv. HB won't get invoked because it was not protecting against anything
 * so HA will be invoked, and return control to A (and then j9sig_test4).
 *   
 * 		->SEGV--->--
 * 		|			|		 
 * 		|	 		|
 * 		B  	 	HB	|
 * 		^			|
 *  	|			|
 * 		|			|
 * 		A<------HA-<-
 * 		^
 * 		|
 *		| 
 *	j9sig_test4
*/
static I_32
j9sig_test4(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA result = 1;
	U_32 protectResult;
	U_32 flags = J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGSEGV;
	const char* testName = "j9sig_test4";
	SigProtectHandlerInfo handlerInfo;

	reportTestEntry(portLibrary, testName);

	if (!j9sig_can_protect(flags)) {
		operationNotSupported(portLibrary, "the specified signals are");
	} else {
			
		/* test running a function which does not fault */
		handlerInfo.expectedType = J9PORT_SIG_FLAG_SIGSEGV;
		handlerInfo.returnValue = J9PORT_SIG_EXCEPTION_RETURN;
		handlerInfo.testName = testName;
		protectResult = j9sig_protect(
			nestedTestCatchNothing, (void*)testName, 
			simpleHandlerFunction, &handlerInfo, 
			flags, 
			&result);
		
		if (protectResult != J9PORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected J9PORT_SIG_EXCEPTION_OCCURRED return value\n");
		}
		
		if (result != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 in *result\n");
		}
	}
	
	return reportTestExit(portLibrary, testName);
}

/*
 * Test protecting Function A against a SEGV with handler HA. A in turn protects Function B
 * against a SEGV with handler HB. Function B raises a sigsegv. HB gets invoked, returns J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH
 * so that HA gets invoked, which then returns control to A (and then j9sig_test5).
 * 
 * 		->SEGV
 * 		|	 |
 * 		|	 |
 * 		B  	 -->HB
 * 		^		|
 *  	|	    |
 * 		|       |
 * 		A<----<-HA
 * 		^
 * 		|
 * 		|
 * 	j9sig_test5
 *   
*/
static I_32
j9sig_test5(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA result = 1;
	U_32 protectResult;
	U_32 flags = J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGSEGV;
	const char* testName = "j9sig_test5";
	SigProtectHandlerInfo handlerInfo;

	reportTestEntry(portLibrary, testName);

	if (!j9sig_can_protect(flags)) {
		operationNotSupported(portLibrary, "the specified signals are");
	} else {
				
		handlerInfo.expectedType = J9PORT_SIG_FLAG_SIGSEGV;
		handlerInfo.returnValue = J9PORT_SIG_EXCEPTION_RETURN;
		handlerInfo.testName = testName;
		protectResult = j9sig_protect(
			nestedTestCatchAndIgnore, (void*)testName, 
			simpleHandlerFunction, &handlerInfo, 
			flags, 
			&result);
		
		if (protectResult != J9PORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected J9PORT_SIG_EXCEPTION_OCCURRED return value\n");
		}
		
		if (result != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 in *result\n");
		}
	}
	
	return reportTestExit(portLibrary, testName);
}

/*
 * 		->SEGV->-
 * 		|	    |
 * 		|	    |
 * 		B  	    -->HB-->--SEGV->-
 * 		^		    			|
 *  	|		  				|
 *		|	 					|
 * 		|						|
 * 		|						|
 * 		A<---<--HA----<---------
 * 		^
 * 		|
 * 		|
 * 	j9sig_test6
*/
static I_32
j9sig_test6(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA result = 1;
	U_32 protectResult;
	U_32 flags = J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGSEGV;
	const char* testName = "j9sig_test6";
	SigProtectHandlerInfo handlerInfo;

	reportTestEntry(portLibrary, testName);

	if (!j9sig_can_protect(flags)) {
		operationNotSupported(portLibrary, "the specified signals are");
	} else {
			
		handlerInfo.expectedType = J9PORT_SIG_FLAG_SIGSEGV;
		handlerInfo.returnValue = J9PORT_SIG_EXCEPTION_RETURN;
		handlerInfo.testName = testName;
		protectResult = j9sig_protect(
			nestedTestCatchAndCrash, (void*)testName, 
			simpleHandlerFunction, &handlerInfo, 
			flags, 
			&result);
		
		if (protectResult != J9PORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected J9PORT_SIG_EXCEPTION_OCCURRED return value\n");
		}
		
		if (result != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 in *result\n");
		}
		
#if 0
		/* CMVC 126838
		 * Verify that the current signal is still valid. There's no public API, so 
		 * to run this test you need to port/unix/j9signal.c (remove static from declaration of tlsKeyCurrentSignal)
		 * and add "<export name="tlsKeyCurrentSignal" />" to port/module.xml, to export tlsKeyCurrentSignal.
		 */
#if ! defined(WIN32) 
		{
			extern omrthread_tls_key_t tlsKeyCurrentSignal;
			int signo = (int)(UDATA)omrthread_tls_get(omrthread_self(), tlsKeyCurrentSignal);
			int expectedSigno = 0;
			
			j9tty_printf(portLibrary, "\n testing current signal\n");
			if (signo != expectedSigno) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "currentSignal corrupt -- got: %d expected: %d\n", signo, expectedSigno);
			}
		}
#endif
#endif 

	}
	
	return reportTestExit(portLibrary, testName);
}

/* 
 * Test the exposure of the currently handled signal 
 * 
*/
static I_32
j9sig_test7(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA result = 1;
	U_32 protectResult;
	U_32 flags = J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGSEGV;
	SigProtectHandlerInfo handlerInfo;
	const char* testName = "j9sig_test7";
	IDATA currentSigNum = -1;

	handlerInfo.expectedType = J9PORT_SIG_FLAG_SIGSEGV;
	handlerInfo.returnValue = J9PORT_SIG_EXCEPTION_RETURN;
	handlerInfo.testName = testName;
	reportTestEntry(portLibrary, testName);

	if (!j9sig_can_protect(flags)) {
		operationNotSupported(portLibrary, "the specified signals are");
	} else {
		currentSigNum = j9sig_get_current_signal();

		if (0 != currentSigNum) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 for current signal, received 0x%X\n", currentSigNum);
		}
		
		protectResult = j9sig_protect(
			raiseSEGV, (void*)testName, 
			currentSigNumHandlerFunction, &handlerInfo,
			flags, 
			&result);
		
		if (protectResult != J9PORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected J9PORT_SIG_EXCEPTION_OCCURRED return value\n");
		}
		
		if (result != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 in *result\n");
		}
		
		currentSigNum = j9sig_get_current_signal();

		if (0 != currentSigNum) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 for current signal, received 0x%X\n", currentSigNum);
		}
	}
	
	return reportTestExit(portLibrary, testName);
}

/*
 * Caller indicates that this may return J9PORT_SIG_EXCEPTION_RETURN,
 * but we actually return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH.
 */
static UDATA
callSigProtectWithContinueSearch(J9PortLibrary* portLibrary, void* arg)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	SigProtectHandlerInfo handlerInfo;
	U_32 protectResult;
	U_32 flags = J9PORT_SIG_FLAG_SIGSEGV | J9PORT_SIG_FLAG_MAY_RETURN;
	const char* testName = "j9sig_test8";
	UDATA result = 1;

	handlerInfo.expectedType = J9PORT_SIG_FLAG_SIGSEGV;
	handlerInfo.returnValue = J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
	handlerInfo.testName = testName;
	protectResult = j9sig_protect(
		nestedTestCatchAndContinueSearch, (void*)testName,
		simpleHandlerFunction, &handlerInfo,
		flags,
		&result);
	if (protectResult != J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH return value\n");
	}
	return result;
}

/*
 * Test handling of J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH.
 * invoke A with hander HA
 * 	invoke B with handler HB
 * 		invoke C with handler HC
 * 			C causes exception
 * 			HC returns J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH
 * 		HB returns J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH
 * 	HA returns J9PORT_SIG_EXCEPTION_RETURN
 *
*/

static I_32
j9sig_test8(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA result = 1;
	U_32 protectResult;
	U_32 flags = J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGSEGV;
	const char* testName = "j9sig_test8";
	SigProtectHandlerInfo handlerInfo;

	reportTestEntry(portLibrary, testName);

	if (!j9sig_can_protect(flags)) {
		operationNotSupported(portLibrary, "the specified signals are");
	} else {

		handlerInfo.expectedType = J9PORT_SIG_FLAG_SIGSEGV;
		handlerInfo.returnValue = J9PORT_SIG_EXCEPTION_RETURN;
		handlerInfo.testName = testName;
		protectResult = j9sig_protect(
			callSigProtectWithContinueSearch, (void*)testName,
			simpleHandlerFunction, &handlerInfo,
			flags,
			&result);
		outputComment(portLibrary, "j9sig_test8 protectResult=%d\n", protectResult);
#if defined(WIN64)
		if (protectResult != J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH in protectResult\n");
		}
#else
		if (protectResult != J9PORT_SIG_EXCEPTION_OCCURRED) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH in protectResult\n");
		}
#endif
		if (result != 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sig_protect -- expected 0 in *result\n");
		}
	}
	return reportTestExit(portLibrary, testName);
}


#if defined(J9SIGNAL_TEST_RUN_ASYNC_UNIX_TESTS)
/*
 * Tests the async signals to verify that our handler is called both when the signal is raised and injected by another process
 *
 * To test that the handler was called, j9sig_test_async_unix_handler sets controlFlag to 0, then expects the handler to have set it to 1
 * 
 * The child process is another instance of pltest with the argument -child_j9sig_injectSignal_<PID>_<signal to inject>.
 * In the child process:
 * 	- main.c peels off -child_
 * 	- j9sig_run_tests looks for j9sig_injectSignal, and if found, reads the PID and signal number and injects it
 * 
 * NOTE : Assumes 0.5 seconds is long enough for a child process to be kicked off, inject a signal and have our handler kick in
 * */
static I_32
j9sig_test_async_unix_handler(struct J9PortLibrary *portLibrary, char *exeName)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sig_test_async_unix_handler";
	AsyncHandlerInfo handlerInfo;
	UDATA result = 1;
	U_32 setAsyncRC;
	int index;
	omrthread_monitor_t asyncMonitor;
	IDATA monitorRC;
	int pid = getpid();

	reportTestEntry(portLibrary, testName);
	j9tty_printf(portLibrary, "\tpid: %i\n", pid);

	handlerInfo.testName = testName;

	monitorRC = omrthread_monitor_init_with_name( &asyncMonitor, 0, "j9signalTest_async_monitor" );

	if (0 != monitorRC) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrthread_monitor_init_with_name failed with %i\n", monitorRC);
		return TEST_FAIL;
	}

	handlerInfo.monitor = &asyncMonitor;

	setAsyncRC = j9sig_set_async_signal_handler(asyncTestHandler, &handlerInfo, J9PORT_SIG_FLAG_SIGQUIT | J9PORT_SIG_FLAG_SIGABRT | J9PORT_SIG_FLAG_SIGTERM | J9PORT_SIG_FLAG_SIGINT);
	if (setAsyncRC == J9PORT_SIG_ERROR) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sig_set_async_signal_handler returned: J9PORT_SIG_ERROR\n");
		goto exit;
	}

#if defined(AIXPPC)
	setAsyncRC = j9sig_set_async_signal_handler(asyncTestHandler, &handlerInfo, J9PORT_SIG_FLAG_SIGRECONFIG);
	if (setAsyncRC == J9PORT_SIG_ERROR) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sig_set_async_signal_handler returned: J9PORT_SIG_ERROR\n");
		goto exit;
	}
#endif

	/* iterate through all the known signals */
	for (index = 0; index < sizeof(signalMap)/sizeof(signalMap[0]); index++) {

		J9ProcessHandle childProcess = NULL;
		char options[SIG_TEST_SIZE_EXENAME];
		int signum = signalMap[index].unixSignalNo;

		handlerInfo.expectedType = signalMap[index].portLibSignalNo;


		j9tty_printf(portLibrary, "\n\tTesting %s\n", signalMap[index].unixSignalString );

		/* asyncTestHandler notifies the monitor once it has set controlFlag to 0; */
		omrthread_monitor_enter(asyncMonitor);

		/* asyncTestHandler will change controlFlag to 1 */
		handlerInfo.controlFlag = 0;


		/* test that we handle the signal when it is raise()'d */
		if (0 != (J9PORT_SIG_OPTIONS_ZOS_USE_CEEHDLR & portTestOptionsGlobal)) {
#if defined(J9ZOS390)
			/* This prevents LE from sending the corresponding LE condition to our thread */
 			sighold(signum);
			raise(signum);
			sigrelse(signum);
#endif
		} else {
			raise(signum);
		}

		/* we don't want to hang - if we haven't been notified in 20 seconds it doesn't work */
		(void)omrthread_monitor_wait_timed(asyncMonitor, 20000, 0);

#ifdef J9ZTPF
		tpf_process_signals();
		(void)j9thread_monitor_wait_timed(asyncMonitor, 1, 0);
#endif /* defined(J9ZTPF) */

		if (1 != handlerInfo.controlFlag) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s handler was not invoked when %s was raised\n", signalMap[index].portLibSignalString, signalMap[index].unixSignalString);
		}

		handlerInfo.controlFlag = 0;
		omrthread_monitor_exit(asyncMonitor);

		/* Test that we handle the signal when it is injected */

		/* build the pid and signal number in the form "-child_j9sig_injectSignal_<PID>_<signal>" */
		j9str_printf(portLibrary, options, SIG_TEST_SIZE_EXENAME, "-child_j9sig_injectSignal_%i_%i", pid, signum);
		j9tty_printf(portLibrary, "\t\tlaunching child process with arg %s\n", options);
		
		omrthread_monitor_enter(asyncMonitor);

		childProcess = launchChildProcess(PORTLIB, testName, exeName, options);

		if (NULL == childProcess) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot launch test process! Can not perform test!");
			omrthread_monitor_exit(asyncMonitor);
			goto exit;
		}

		/* we don't want to hang - if we haven't been notified in 20 seconds it doesn't work */
		(void)omrthread_monitor_wait_timed(asyncMonitor, 20000, 0);

		if (1 != handlerInfo.controlFlag) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s handler was NOT invoked when %s was injected by another process\n", signalMap[index].portLibSignalString, signalMap[index].unixSignalString);
		}

		omrthread_monitor_exit(asyncMonitor);

		j9process_close(&childProcess, 0);

#if defined(J9ZOS390)
		/*
		 * Verify that async signals can't be received by the asyncSignalReporter thread.
		 * pltest runs with 2 threads only, this thread and the asyncSignalReporter thread.
		 * We will mask the signal on this thread, inject it via kill, and verify that it does
		 * not get received.
		 */
		j9tty_printf(portLibrary, "\t\tmasking signal %i on this thread\n", signum);
		sighold(signum);

		/* build the pid and signal number in the form "-child_j9sig_injectSignal_<PID>_<signal>_unhandled" */
		j9str_printf(portLibrary, options, SIG_TEST_SIZE_EXENAME, "-child_j9sig_injectSignal_%i_%i_unhandled", pid, signum);
		j9tty_printf(portLibrary, "\t\tlaunching child process with arg %s\n", options);

		omrthread_monitor_enter(asyncMonitor);
		handlerInfo.controlFlag = 0;

		childProcess = launchChildProcess(PORTLIB, testName, exeName, options);

		if (NULL == childProcess) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Cannot launch test process! Can not perform test!");
			omrthread_monitor_exit(asyncMonitor);
			goto exit;
		}

		/* wait 5 seconds for the signal to be received */
		(void)omrthread_monitor_wait_timed(asyncMonitor, 5000, 0);

		/* verify that the signal was NOT received */
		if (0 != handlerInfo.controlFlag) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s handler was unexpectedly invoked when %s was injected by another process\n",
					signalMap[index].portLibSignalString, signalMap[index].unixSignalString);
		}

		/* unblock the signal and handle it */
		j9tty_printf(portLibrary, "\t\tunmasking signal %i on this thread\n", signum);
		sigrelse(signum);
		(void)omrthread_monitor_wait_timed(asyncMonitor, 20000, 0);
		if (1 != handlerInfo.controlFlag) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "%s handler was NOT invoked when %s was injected by another process\n", signalMap[index].portLibSignalString, signalMap[index].unixSignalString);
		}

		omrthread_monitor_exit(asyncMonitor);

		j9process_close(&childProcess, 0);
#endif

	} /* for */

exit:
	j9sig_set_async_signal_handler(asyncTestHandler, &handlerInfo, 0);
	omrthread_monitor_destroy(asyncMonitor);
	return reportTestExit(portLibrary, testName);

}
#endif /* J9SIGNAL_TEST_RUN_ASYNC_UNIX_TESTS */


/*
 * 
 * 
 *		C->-SEGV->-HC--->---|
 * 		|					|
   		------<---<-|		|
        			|		|
        			|		|
 * 		->SEGV->-   |		|
 * 		|	    |   |		|
 * 		|	    |	|		|
 * 		B  	    -->HB--<-----
 * 		^			|
 *  	|			|
 *		|	 		|
 * 		|	--<-----|
 * 		|	|		
 * 		A<--- 	   HA
 * 		^
 * 		|
 * 		|
 * 	j9sigTest<future>
*/

