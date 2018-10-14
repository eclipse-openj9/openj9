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
#include "SCStringTransaction.hpp"
#include "SCStringTransactionTests.hpp"
#include "SCTestCommon.h"

#define TEST_PASS 0
#define TEST_ERROR -1

#if defined(J9SHR_CACHELET_SUPPORT)
IDATA
testSCStringTransaction(J9JavaVM* vm)
{
	char * testName = "testSCStringTransaction";
	if (vm == NULL) {
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
static IDATA test2(J9JavaVM* vm);
static IDATA test3(J9JavaVM* vm);

IDATA
testSCStringTransaction(J9JavaVM* vm)
{
	const char * testName = "testSCStringTransaction";
	if (vm == NULL) {
		/*vm is null*/
		return TEST_ERROR;
	}
	PORT_ACCESS_FROM_JAVAVM(vm);

	IDATA rc = TEST_PASS;

	if (vm->sharedClassConfig == NULL) {
		ERRPRINTF(("vm->sharedClassConfig == NULL\n"));
		return TEST_ERROR;
	}

	if (vm->systemClassLoader == NULL) {
		ERRPRINTF(("vm->systemClassLoader == NULL\n"));
		return TEST_ERROR;
	}

	vm->internalVMFunctions->internalEnterVMFromJNI(vm->mainThread);
	rc |= test1(vm);
	rc |= test2(vm);
	rc |= test3(vm);
	vm->internalVMFunctions->internalExitVMToJNI(vm->mainThread);


	j9tty_printf(PORTLIB, "%s: %s\n", testName, TEST_PASS==rc?"PASS":"FAIL");
	if (rc == TEST_ERROR) {
		return TEST_ERROR;
	} else {
		return TEST_PASS;
	}
}

static IDATA
test1(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	const char * testName = "test1";

	j9tty_printf(PORTLIB, "%s: create/destroy a SCStringTransaction object\n", testName);

	SCStringTransaction transaction(vm->mainThread);
	if (transaction.isOK() == false) {
		j9tty_printf(PORTLIB, "\tcould not allocate the transaction object\n");
		return TEST_ERROR;
	}
	return TEST_PASS;
}

static IDATA
test2(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	PORT_ACCESS_FROM_JAVAVM(vm);
	const char * testName = "test2";
	j9tty_printf(PORTLIB, "%s: isOK check when cache full bit is set\n", testName);

	vm->mainThread->javaVM->sharedClassConfig->runtimeFlags |= J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;

	SCStringTransaction transaction(vm->mainThread);
	if (transaction.isOK() == false) {
		j9tty_printf(PORTLIB, "\tisOK returns false\n");
		retval = TEST_ERROR;
		goto done;
}

	done:
	vm->mainThread->javaVM->sharedClassConfig->runtimeFlags &= ~J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
	return retval;
}

static IDATA
test3(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	PORT_ACCESS_FROM_JAVAVM(vm);
	const char * testName = "test3";
	const char * romclassName = "FilledCacheStringTransactionObject";
	U_32 romclassSizeLarge = 20971520;
	j9tty_printf(PORTLIB, "%s: isOK check when cache is filled\n", testName);

	if ((vm->mainThread->javaVM->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL) != 0){
		vm->mainThread->javaVM->sharedClassConfig->runtimeFlags &= ~J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
	}

	/*Fill the cache*/
	{
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(romclassName), (U_8 *) romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}
		transaction.createSharedClass(romclassSizeLarge);
	}

	/* Flag for BLOCK_SPACE_FULL is not set if SH_CompositeCacheImpl::allocate() fails.
	 * It is set only when data is written in last CC_MIN_SPACE_BEFORE_CACHE_FULL free block bytes.
	 */
	if ((vm->mainThread->javaVM->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL) != 0){
		ERRPRINTF("cache full bit is set\n");
		retval = TEST_ERROR;
		goto done;
	}

	{
		SCStringTransaction transaction(vm->mainThread);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "\tisOK returns false\n");
			retval = TEST_ERROR;
			goto done;
		}
	}

	done:
	vm->mainThread->javaVM->sharedClassConfig->runtimeFlags &= ~J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
	return retval;
}

#endif /*defined(J9SHR_CACHELET_SUPPORT)*/
