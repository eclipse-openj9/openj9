/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

extern "C"
{
#include "shrinit.h"
}
#include "SCStoreTransaction.hpp"
#include "SCStoreWithBCITests.hpp"
#include "ClassDebugDataProvider.hpp"
//#include "SCQueryFunctions.h"
#include "SCTestCommon.h"

#define TEST_PASS 0
#define TEST_ERROR -1

#if defined(J9SHR_CACHELET_SUPPORT)
IDATA
testSCStoreWithBCITests(J9JavaVM* vm)
{
	const char * testName = "testSCStoreWithBCITests";
	if (NULL == vm) {
		/*vm is null*/
		return TEST_ERROR;
	}
	PORT_ACCESS_FROM_JAVAVM(vm);

	/*Note: we do this b/c test fails on realtime currently unless there is an existing cache*/
	j9tty_printf(PORTLIB, "Skip these tests on realtime b/c cache is readonly\n", testName);
	return TEST_PASS;
}
#else

static IDATA test1(J9JavaVM* vm);
static IDATA testRunner(J9JavaVM* vm, struct TestInfo testinfo);

typedef struct TestInfo {
	const char * testName;
	const char * romclassName;
	U_32 romclassSize;
	U_32 debugDataSize;
	bool useSysClassloader;
} TestInfo;

TestInfo tests[] =
{
		{ "test2", "test2RomClass",  1024, 	0, 		false},
		{ "test3", "test3RomClass",  1024, 	512, 	false},
		{ "test4", "test4RomClass",  1024, 	0, 		false},
		{ "test5", "test5RomClass",  1024, 	256, 	false},
		{ "test6", "test2RomClass",  1024, 	0, 		true},
		{ "test7", "test3RomClass",  1024, 	512, 	true},
		{ "test8", "test4RomClass",  1024, 	0, 		true},
		{ "test9", "test5RomClass",  1024, 	256, 	true},
};


IDATA
testSCStoreWithBCITests(J9JavaVM* vm)
{
	const char * testName = "testSCStoreWithBCITests";
	IDATA rc = TEST_PASS;
	UDATA i = 0;
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (NULL == vm) {
		/*vm is null*/
		return TEST_ERROR;
	}

	if (NULL == vm->sharedClassConfig) {
		ERRPRINTF(("vm->sharedClassConfig == NULL\n"));
		return TEST_ERROR;
	}

	if (NULL == vm->systemClassLoader) {
		ERRPRINTF(("vm->systemClassLoader == NULL\n"));
		return TEST_ERROR;
	}

	vm->internalVMFunctions->internalEnterVMFromJNI(vm->mainThread);
	
	rc |= test1(vm);
	for (i = 0; i < sizeof(tests)/sizeof(TestInfo); i += 1) {
		rc |= testRunner(vm, tests[i]);
	}
	
	vm->internalVMFunctions->internalExitVMToJNI(vm->mainThread);

	j9tty_printf(PORTLIB, "%s: %s\n", testName, TEST_PASS==rc?"PASS":"FAIL");
	if (rc == TEST_ERROR) {
		return TEST_ERROR;
	} else {
		return TEST_PASS;
	}
}

/**
 * This test only asserts that we created the cache with enablebci. It's 
 * not really all that useful.
 */
static IDATA
test1(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	const char * testName = "test1";

	j9tty_printf(PORTLIB, "%s: ensure cache is created with enablebci\n", testName);
	
	/* Check if BCI is enabled just as a sanity check. We know for these tests
	 * that shared classes was started with -Xshareclasses:enablebci,rcdSize=2m
	 */
	UDATA bcienabled = vm->sharedClassConfig->isBCIEnabled(vm);
	if (bcienabled == 0) {
		j9tty_printf(PORTLIB, "Shared cache was created without Xshareclasses:enablebci\n");
		return TEST_ERROR;
	}
	return TEST_PASS;
}

/**
 * Run a test specified by a TestInfo structure
 */
static IDATA
testRunner(J9JavaVM* vm, struct TestInfo testinfo)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	const char * testName = testinfo.testName;
	J9ROMClass * romclass = NULL;
	const char * romclassName = testinfo.romclassName;
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));
	bool useFullSize = (0 != testinfo.romclassSize) && (0 == testinfo.debugDataSize);
	J9ClassLoader* classloader = NULL;

	PORT_ACCESS_FROM_JAVAVM(vm);

	if (true == testinfo.useSysClassloader) {
		j9tty_printf(PORTLIB, "%s: allocate a rom class with %u bytes of debug data\n", testName, testinfo.debugDataSize);
		classloader = (J9ClassLoader*) vm->systemClassLoader;
	} else {
		j9tty_printf(PORTLIB, "%s: allocate a orphan rom class with %u bytes of debug data\n", testName, testinfo.debugDataSize);
	}
	SCStoreTransaction transaction(vm->mainThread, classloader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
	if (transaction.isOK() == false) {
		j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
		return TEST_ERROR;
	}

	sizes.romClassSizeFullSize = testinfo.romclassSize;
	sizes.lineNumberTableSize = testinfo.debugDataSize/2;
	sizes.localVariableTableSize = testinfo.debugDataSize/2;
	sizes.romClassMinimalSize = sizes.romClassSizeFullSize - testinfo.debugDataSize;

	if (false == transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes)) {
		ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
		retval = TEST_ERROR;
		goto done;
	} else {
		romclassmem = transaction.getRomClass();
	}

	if (NULL == romclassmem) {
		ERRPRINTF("romclassmem == NULL\n");
		retval = TEST_ERROR;
		goto done;
	}

	romclass = (J9ROMClass*) romclassmem;
	if (true == transaction.hasAllocatedFullSizeRomClass()) {
		romclass->romSize = sizes.romClassSizeFullSize;
	} else {
		romclass->romSize = sizes.romClassMinimalSize;
	}

	SetRomClassName(vm, romclass, romclassName);

	if (transaction.updateSharedClassSize(romclass->romSize) == -1) {
		ERRPRINTF(("could not update allocated ROMClass size\n"));
		retval = TEST_ERROR;
		goto done;
	}

	if (useFullSize != transaction.hasAllocatedFullSizeRomClass()) {
		ERRPRINTF1("::hasAllocatedFullSizeRomClass() returned %s\n", useFullSize ? "false" : "true");
		retval = TEST_ERROR;
		goto done;
	}

	if ((0 < testinfo.debugDataSize) != transaction.debugDataAllocatedOutOfLine()) {
		ERRPRINTF1("::debugDataAllocatedOutOfLine() returned %s\n", (0 < testinfo.debugDataSize) ? "false" : "true");
		retval = TEST_ERROR;
		goto done;
	}

	if ((0 < testinfo.debugDataSize) && (NULL == transaction.getLineNumberTable())) {
		ERRPRINTF("(0 < testinfo.debugDataSize) && (NULL == transaction.getLineNumberTable())\n");
		retval = TEST_ERROR;
		goto done;
	}

	if ((0 < testinfo.debugDataSize) && (NULL == transaction.getLocalVariableTable())) {
		ERRPRINTF("(0 < testinfo.debugDataSize) && (NULL == transaction.getLocalVariableTable())\n");
		retval = TEST_ERROR;
		goto done;
	}

done:
	return retval;
}

#endif /*defined(J9SHR_CACHELET_SUPPORT)*/
