/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
#include "SCStoreTransactionTests.hpp"
#include "ClassDebugDataProvider.hpp"
#include "SCQueryFunctions.h"
#include "SCTestCommon.h"

#define TEST_PASS 0
#define TEST_ERROR -1
/* A size greater than the shared cache size (or softmx size if it is set) */
#define ROMCLASS_LARGE_SIZE (70 * 1024 *1024)

#if defined(J9SHR_CACHELET_SUPPORT)
IDATA
testSCStoreTransaction(J9JavaVM* vm)
{
	const char * testName = "testSCStoreTransaction";
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

static IDATA AddClassToCacheHelper(J9VMThread* currentThread, J9ClassLoader* classloader, UDATA entryIndex, UDATA loadType, U_16 classnameLength, U_8 * classnameData, U_32 romclassSize);
static IDATA test1(J9JavaVM* vm);
static IDATA test2(J9JavaVM* vm);
static IDATA test3(J9JavaVM* vm);
static IDATA test4(J9JavaVM* vm);
static IDATA test5(J9JavaVM* vm);
static IDATA test6(J9JavaVM* vm);
static IDATA test7(J9JavaVM* vm);
static IDATA test8(J9JavaVM* vm);
static IDATA test9(J9JavaVM* vm);
static IDATA test10(J9JavaVM* vm);
static IDATA test11(J9JavaVM* vm);
static IDATA test12(J9JavaVM* vm);
static IDATA test13(J9JavaVM* vm);
static IDATA test14(J9JavaVM* vm);
static IDATA test15(J9JavaVM* vm);
static IDATA test16(J9JavaVM* vm);
static IDATA test17(J9JavaVM* vm);
static IDATA test18(J9JavaVM* vm);
static IDATA test19(J9JavaVM* vm);
static IDATA test20(J9JavaVM* vm);
static IDATA test21(J9JavaVM* vm);
static IDATA test22(J9JavaVM* vm);
static IDATA test23(J9JavaVM* vm);
static IDATA test24(J9JavaVM* vm);
static IDATA test25(J9JavaVM* vm);
static IDATA test26(J9JavaVM* vm);
static IDATA test27(J9JavaVM* vm);
static IDATA test29(J9JavaVM* vm);

IDATA
testSCStoreTransaction(J9JavaVM* vm)
{
	const char * testName = "testSCStoreTransaction";
	if (NULL == vm) {
		/*vm is null*/
		return TEST_ERROR;
	}
	PORT_ACCESS_FROM_JAVAVM(vm);

	IDATA rc = TEST_PASS;

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
	rc |= test2(vm);
	rc |= test3(vm);
	rc |= test4(vm);
	rc |= test5(vm);
	rc |= test6(vm);
	rc |= test7(vm);
	rc |= test8(vm);
	rc |= test9(vm);
	rc |= test10(vm);
	rc |= test11(vm);
	rc |= test12(vm);
	rc |= test13(vm);
	rc |= test14(vm);
	rc |= test15(vm);
	rc |= test16(vm);
	rc |= test17(vm);
	rc |= test18(vm);
	rc |= test19(vm);
	rc |= test20(vm);
	rc |= test21(vm);
	rc |= test22(vm);
	

	/*The tests below exercise the class debug area of the cache*/
	if (0 < ClassDebugDataProvider::getRecommendedPercentage()) {
		rc |= test23(vm);
		rc |= test24(vm);
		rc |= test25(vm);
		rc |= test26(vm);
		rc |= test27(vm);
		rc |= test29(vm);
	} else {
		j9tty_printf(PORTLIB, "%s: Skipping some tests b/c the cache has class debug area size of 0 bytes.\n", testName);
	}
	
	vm->internalVMFunctions->internalExitVMToJNI(vm->mainThread);

	j9tty_printf(PORTLIB, "%s: %s\n", testName, TEST_PASS==rc?"PASS":"FAIL");
	if (rc == TEST_ERROR) {
		return TEST_ERROR;
	} else {
		return TEST_PASS;
	}
}

static IDATA
AddClassToCacheHelper(J9VMThread* currentThread, J9ClassLoader* classloader, UDATA entryIndex, UDATA loadType, U_16 classnameLength, U_8 * classnameData, U_32 romclassSize)
{
	const char * testName = "AddClassToCacheHelper";
	void * romclassmem = NULL;
	J9ROMClass * romclass;
	PORT_ACCESS_FROM_JAVAVM(currentThread->javaVM);
	const int maxName = 128;
	char romclassName[maxName];
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));

	if(maxName<classnameLength+1) {
		ERRPRINTF(("classnameLength is too long (fix the test)\n"));
		return TEST_ERROR;
	}
	strncpy(romclassName,(char *)classnameData,classnameLength);
	romclassName[classnameLength]='\0';

	SCStoreTransaction transaction(currentThread->javaVM->mainThread, (J9ClassLoader*) currentThread->javaVM->systemClassLoader, entryIndex, loadType, classnameLength, classnameData, false);
	if (transaction.isOK() == false) {
		j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
		return TEST_ERROR;
	}

	sizes.romClassSizeFullSize = romclassSize;
	sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
	if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == false) {
		ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
		return TEST_ERROR;
	} else {
		romclassmem = transaction.getRomClass();
	}
	
	if (NULL == romclassmem) {
		ERRPRINTF(("romclassmem == NULL\n"));
		return TEST_ERROR;
	}

	romclass = (J9ROMClass*) romclassmem;
	romclass->romSize = romclassSize;
	SetRomClassName(currentThread->javaVM, romclass, romclassName);

	return TEST_PASS;
}

static IDATA
test1(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	const char * testName = "test1";
	const char * romclassName = "test1ClassName";

	j9tty_printf(PORTLIB, "%s: test constructor destructor\n", testName);

	SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, 0, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
	if (transaction.isOK() == false) {
		j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
		return TEST_ERROR;
	}
	return TEST_PASS;
}

static IDATA
test2(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	J9ROMClass * romclass = NULL;
	BOOLEAN foundMatch = FALSE;
	const char * testName = "test2";
	const char * romclassName = "test2ClassName";

	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: test compare functions (no matches)\n", testName);

	SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, 0, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
	if (transaction.isOK() == false) {
		j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
		return TEST_ERROR;
	}

	for (romclass = transaction.nextSharedClassForCompare(); romclass != NULL; romclass = transaction.nextSharedClassForCompare()) {
		foundMatch = TRUE;
	}

	if (foundMatch == TRUE) {
		ERRPRINTF(("found ROM Classes when none where expected\n"));
		retval = TEST_ERROR;
		goto done;
	}

	done:
	return retval;
}

static IDATA
test3(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	const char * testName = "test3";
	J9ROMClass * romclass = NULL;
	U_32 romclassSize = 1024;
	const char * romclassName = "J9ROMClassOrphan";
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));

	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: allocate a orphan rom class that is 1K in size\n", testName);

	SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
	if (transaction.isOK() == false) {
		j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
		return TEST_ERROR;
	}

	sizes.romClassSizeFullSize = romclassSize;
	sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
	if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == false) {
		ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
		return TEST_ERROR;
	} else {
		romclassmem = transaction.getRomClass();
	}

	if (NULL == romclassmem) {
		ERRPRINTF(("romclassmem == NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}

	romclass = (J9ROMClass*) romclassmem;
	romclass->romSize = romclassSize;

	SetRomClassName(vm, romclass, romclassName);

	if (transaction.updateSharedClassSize(0) == -1) {
		ERRPRINTF(("could not update allocated ROMClass size\n"));
		retval = TEST_ERROR;
		goto done;
	}

	done:
	return retval;
}

static IDATA
test4(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	const char * testName = "test4";
	J9ROMClass * romclass = NULL;
	U_32 romclassSize = 1024;
	const char * romclassName = "J9ROMClassNonOrphan";
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));

	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: allocate a non-orphan orphan rom class that is 1K in size\n", testName);

	SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
	if (transaction.isOK() == false) {
		j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
		return TEST_ERROR;
	}

	sizes.romClassSizeFullSize = romclassSize;
	sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
	if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == false) {
		ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
		return TEST_ERROR;
	} else {
		romclassmem = transaction.getRomClass();
	}

	if (NULL == romclassmem) {
		ERRPRINTF(("romclassmem == NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}

	romclass = (J9ROMClass*) romclassmem;
	romclass->romSize = romclassSize;

	SetRomClassName(vm, romclass, romclassName);

	done:
	return retval;
}

static IDATA
test5(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	const char * testName = "test5";
	J9ROMClass * romclass = NULL;
	U_32 romclassSize = 1024;
	U_32 romclassResize = 512;
	const char * romclassName = "J9ROMClassResizedNonOrphan";
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));

	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: allocate 1K class, the resize to 512 bytes\n", testName);

	SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
	if (transaction.isOK() == false) {
		j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
		return TEST_ERROR;
	}

	sizes.romClassSizeFullSize = romclassSize;
	sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
	if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == false) {
		ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
		return TEST_ERROR;
	} else {
		romclassmem = transaction.getRomClass();
	}

	if (NULL == romclassmem) {
		ERRPRINTF(("romclassmem == NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}

	romclass = (J9ROMClass*) romclassmem;
	romclass->romSize = romclassSize;
	SetRomClassName(vm, romclass, romclassName);

	if (transaction.updateSharedClassSize(romclassResize) == -1) {
		ERRPRINTF(("could not update allocated ROMClass size\n"));
		retval = TEST_ERROR;
		goto done;
	}
	/*Update ROMClass size*/
	romclass->romSize = romclassResize;

	done:
	return retval;
}

static IDATA
test6(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	BOOLEAN foundMatch = FALSE;
	const char * testName = "test6";
	J9ROMClass * romclass = NULL;
	const char * test5RomclassName = "J9ROMClassResizedNonOrphan";

	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: find the ROMClasses added by previous tests\n", testName);

	SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(test5RomclassName), (U_8 *)test5RomclassName, false);
	if (transaction.isOK() == false) {
		j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
		return TEST_ERROR;
	}

	for (romclass = transaction.nextSharedClassForCompare(); romclass != NULL; romclass = transaction.nextSharedClassForCompare()) {
		J9UTF8 * classnameUTF8 = NULL;
		classnameUTF8 = J9ROMCLASS_CLASSNAME(romclass);
		j9tty_printf(PORTLIB, "\tInfo: Found J9ROMClass named: %.*s\n", J9UTF8_LENGTH(classnameUTF8), J9UTF8_DATA(classnameUTF8));
		foundMatch = TRUE;
	}

	if (foundMatch == FALSE) {
		ERRPRINTF(("found ROM Classes when none where expected\n"));
		retval = TEST_ERROR;
		goto done;
	}

	done:
	return retval;
}



static IDATA
test7(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	const char * testName = "test7";
	J9ROMClass * romclass = NULL;
	U_32 romclassSize = 512;
	const char * romclassName = "test7ROMClass";
	int numClassesToStore = 5;
	int foundCount = 0;
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));

	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: add 5 classes with the same name\n", testName);


	for (int i = 0; i < numClassesToStore; i+=1) {
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			return TEST_ERROR;
		}
		sizes.romClassSizeFullSize = romclassSize;
		sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
		if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == false) {
			ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
			return TEST_ERROR;
		} else {
			romclassmem = transaction.getRomClass();
		}

		if (NULL == romclassmem) {
			ERRPRINTF(("romclassmem == NULL\n"));
			return TEST_ERROR;
		}

		romclass = (J9ROMClass*) romclassmem;
		romclass->romSize = romclassSize;
		SetRomClassName(vm, romclass, romclassName);
	}

	SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
	if (transaction.isOK() == false) {
		j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
		return TEST_ERROR;
	}

	for (romclass = transaction.nextSharedClassForCompare(); romclass != NULL; romclass = transaction.nextSharedClassForCompare()) {
		J9UTF8 * classnameUTF8 = NULL;
		classnameUTF8 = J9ROMCLASS_CLASSNAME(romclass);
		foundCount+=1;
		j9tty_printf(PORTLIB, "\t%d.) Found J9ROMClass named: %.*s\n", foundCount, J9UTF8_LENGTH(classnameUTF8), J9UTF8_DATA(classnameUTF8));

	}

	/*I use % instead of == b/c someone running this test by hand may not clean up the cache before calling this test*/
	if ((foundCount % numClassesToStore) != 0) {
		ERRPRINTF(("Bad count of J9ROMClass's"));
		retval = TEST_ERROR;
		return TEST_ERROR;
	}

	return retval;
}


static IDATA
test8(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	BOOLEAN foundMatch = FALSE;
	const char * testName = "test8";
	J9ROMClass * romclass = NULL;
	U_32 romclassSize = 512;
	const char * romclassName = "test8ROMClass";
	UDATA entryIndex1 = 0;
	UDATA entryIndex2 = 1;
	UDATA entryIndex3 = 2;

	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: test adding same class against different class path index\n", testName);

	/* **Add 1st class** */
	j9tty_printf(PORTLIB, "\tAdding %s against index %d\n", romclassName, entryIndex1);
	if (AddClassToCacheHelper(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, entryIndex1, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, romclassSize) == TEST_ERROR) {
		retval = TEST_ERROR;
		goto done;
	}

	/* **Add 2nd class** */
	do {
		/*NOTE: doing this in a do/while to ensure destructor/commit runs ...*/
		j9tty_printf(PORTLIB, "\tAdding %s against index %d\n", romclassName, entryIndex2);
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, entryIndex2, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *) romclassName, false);
		if (transaction.isOK() == false) {
			ERRPRINTF(("could not allocate the transaction object\n"));
			retval = TEST_ERROR;
			goto done;
		}

		for (romclass = transaction.nextSharedClassForCompare(); romclass != NULL; romclass = transaction.nextSharedClassForCompare()) {
			J9UTF8 * classnameUTF8 = NULL;
			classnameUTF8 = J9ROMCLASS_CLASSNAME(romclass);
			j9tty_printf(PORTLIB, "\t\tInfo: Using J9ROMClass named: %.*s at addr 0x%lx\n", J9UTF8_LENGTH(classnameUTF8), J9UTF8_DATA(classnameUTF8), romclass);
			foundMatch = TRUE;
			break;/*stop after first match*/
		}

		if (foundMatch == FALSE) {
			ERRPRINTF(("found ROM Classes when none where expected\n"));
			retval = TEST_ERROR;
			goto done;
		}
	} while(0);

	/* **Verify we have two hits in the cache that return the same rom class pointer** */
	do {
			/*entryIndex3 is any old value*/
			int foundCount = 0;
			const int expectedCount = 1;
			/*NOTE: doing this in a do/while to ensure destructor/commit runs ...*/
			j9tty_printf(PORTLIB, "\tChecking cache contents\n");
			SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, entryIndex3, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *) romclassName, false);
			if (transaction.isOK() == false) {
				j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
				retval = TEST_ERROR;
				goto done;
			}

			for (romclass = transaction.nextSharedClassForCompare(); romclass != NULL; romclass = transaction.nextSharedClassForCompare()) {
				J9UTF8 * classnameUTF8 = NULL;
				classnameUTF8 = J9ROMCLASS_CLASSNAME(romclass);
				j9tty_printf(PORTLIB, "\t\tInfo: Found J9ROMClass named: %.*s at addr: 0x%lx\n", J9UTF8_LENGTH(classnameUTF8), J9UTF8_DATA(classnameUTF8), (UDATA)romclass);
				foundMatch = TRUE;
				foundCount+=1;
			}

			if (foundMatch == FALSE) {
				ERRPRINTF(("found ROM Classes when none where expected\n"));
				retval = TEST_ERROR;
				goto done;
			}

			if (foundCount != expectedCount) {
				ERRPRINTF(("wrong number of rom classes returned\n"));
				retval = TEST_ERROR;
				goto done;
			}

		} while(0);

	done:
	return retval;
}


static IDATA
test9(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	BOOLEAN foundMatch = FALSE;
	const char * testName = "test9";
	J9ROMClass * romclass = NULL;
	U_32 romclassSize = 512;
	const char * romclassName = "test9ROMClass";
	UDATA entryIndex1 = 0;
	UDATA entryIndex2 = 1;
	UDATA entryIndex3 = 2;

	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: test adding same class against NULL class path (aka add orphans)\n", testName);

	/* **Add 1st class** */
	j9tty_printf(PORTLIB, "\tAdding %s against index %d\n", romclassName, entryIndex1);
	if (AddClassToCacheHelper(vm->mainThread, (J9ClassLoader*) NULL, entryIndex1, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, romclassSize) == TEST_ERROR) {
		retval = TEST_ERROR;
		goto done;
	}

	/* **Add 2nd class** */
	do {
		/*NOTE: doing this in a do/while to ensure destructor/commit runs ...*/
		j9tty_printf(PORTLIB, "\tAdding %s against index %d\n", romclassName, entryIndex2);
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, entryIndex2, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *) romclassName, false);
		if (transaction.isOK() == false) {
			ERRPRINTF(("could not allocate the transaction object\n"));
			retval = TEST_ERROR;
			goto done;
		}

		for (romclass = transaction.nextSharedClassForCompare(); romclass != NULL; romclass = transaction.nextSharedClassForCompare()) {
			J9UTF8 * classnameUTF8 = NULL;
			classnameUTF8 = J9ROMCLASS_CLASSNAME(romclass);
			j9tty_printf(PORTLIB, "\t\tInfo: Using J9ROMClass named: %.*s at addr 0x%lx\n", J9UTF8_LENGTH(classnameUTF8), J9UTF8_DATA(classnameUTF8), romclass);
			foundMatch = TRUE;
			break;/*stop after first match*/
		}

		if (foundMatch == FALSE) {
			ERRPRINTF(("found ROM Classes when none where expected\n"));
			retval = TEST_ERROR;
			goto done;
		}
	} while(0);

	/* **Verify we have two hits in the cache that return the same rom class pointer** */
	do {
			/*entryIndex3 is any old value*/
			int foundCount = 0;
			const int expectedCount = 1;
			/*NOTE: doing this in a do/while to ensure destructor/commit runs ...*/
			j9tty_printf(PORTLIB, "\tChecking cache contents\n");
			SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, entryIndex3, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *) romclassName, false);
			if (transaction.isOK() == false) {
				j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
				retval = TEST_ERROR;
				goto done;
			}

			for (romclass = transaction.nextSharedClassForCompare(); romclass != NULL; romclass = transaction.nextSharedClassForCompare()) {
				J9UTF8 * classnameUTF8 = NULL;
				classnameUTF8 = J9ROMCLASS_CLASSNAME(romclass);
				j9tty_printf(PORTLIB, "\t\tInfo: Found J9ROMClass named: %.*s at addr: 0x%lx\n", J9UTF8_LENGTH(classnameUTF8), J9UTF8_DATA(classnameUTF8), (UDATA)romclass);
				foundMatch = TRUE;
				foundCount+=1;
			}

			if (foundMatch == FALSE) {
				ERRPRINTF(("found ROM Classes when none where expected\n"));
				retval = TEST_ERROR;
				goto done;
			}

			if (foundCount != expectedCount) {
				ERRPRINTF(("wrong number of rom classes returned\n"));
				retval = TEST_ERROR;
				goto done;
			}

		} while(0);

	done:
	return retval;
}

static IDATA
test10(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	const char * testName = "test10";
	J9ROMClass * romclass = NULL;
	U_32 romclassSize = 1024;
	const char * romclassName = "test10J9ROMClassNonOrphan";
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));

	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: allocate a non-orphan orphan rom class (with pretend modified bytecodes and no context)\n", testName);


	j9tty_printf(PORTLIB, "\t%s: store class %s\n", testName, romclassName);
	SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(romclassName), (U_8 *) romclassName, true);
	if (transaction.isOK() == false) {
		j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
		return TEST_ERROR;
	}

	sizes.romClassSizeFullSize = romclassSize;
	sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
	if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == false) {
		ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
		return TEST_ERROR;
	} else {
		romclassmem = transaction.getRomClass();
	}
	
	if (NULL == romclassmem) {
		ERRPRINTF(("romclassmem == NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}

	romclass = (J9ROMClass*) romclassmem;
	romclass->romSize = romclassSize;

	SetRomClassName(vm, romclass, romclassName);

	done:
	return retval;
}

static IDATA
test11(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	BOOLEAN foundMatch = FALSE;
	const char * testName = "test11";
	const char * romclassName = "test11J9ROMClassNonOrphan";
	J9ROMClass * romclass = NULL;
	U_32 romclassSize = 1024;
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));

	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: allocate a non-orphan orphan rom class, then store the same class again but pretend byte codes are modified\n", testName);

	/*Step 1: store 1st class*/
	{
		j9tty_printf(PORTLIB, "\t%s: store class %s\n", testName, romclassName);
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(romclassName), (U_8 *) romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			return TEST_ERROR;
		}

		sizes.romClassSizeFullSize = romclassSize;
		sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
		if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == false) {
			ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
			return TEST_ERROR;
		} else {
			romclassmem = transaction.getRomClass();
		}
		
		if (NULL == romclassmem) {
			ERRPRINTF(("romclassmem == NULL\n"));
			retval = TEST_ERROR;
			goto done;
		}

		romclass = (J9ROMClass*) romclassmem;
		romclass->romSize = romclassSize;

		SetRomClassName(vm, romclass, romclassName);
	}

	/*Step 2: Re-store ... but as modified*/
	{
		int foundCount = 0;
		int expectedCount = 1;
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(romclassName), (U_8 *) romclassName, true);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}

		for (romclass = transaction.nextSharedClassForCompare(); romclass != NULL; romclass = transaction.nextSharedClassForCompare()) {
			J9UTF8 * classnameUTF8 = NULL;
			classnameUTF8 = J9ROMCLASS_CLASSNAME(romclass);
			j9tty_printf(PORTLIB, "\t\tInfo: Found J9ROMClass named: %.*s at addr: 0x%lx\n", J9UTF8_LENGTH(classnameUTF8), J9UTF8_DATA(classnameUTF8), (UDATA) romclass);
			foundMatch = TRUE;
			foundCount += 1;
			break;
		}

		if (foundMatch == FALSE) {
			ERRPRINTF(("found ROM Classes when none where expected\n"));
			retval = TEST_ERROR;
			goto done;
		}

		if (foundCount != expectedCount) {
			ERRPRINTF(("wrong number of rom classes returned\n"));
			retval = TEST_ERROR;
			goto done;
		}
	}

	/*Step 3: check only 1 rom class in the cache ...*/
	{
		int foundCount = 0;
		int expectedCount = 1;
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(romclassName), (U_8 *) romclassName, true);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}

		for (romclass = transaction.nextSharedClassForCompare(); romclass != NULL; romclass = transaction.nextSharedClassForCompare()) {
			J9UTF8 * classnameUTF8 = NULL;
			classnameUTF8 = J9ROMCLASS_CLASSNAME(romclass);
			j9tty_printf(PORTLIB, "\t\tInfo: Found J9ROMClass named: %.*s at addr: 0x%lx\n", J9UTF8_LENGTH(classnameUTF8), J9UTF8_DATA(classnameUTF8), (UDATA) romclass);
			foundMatch = TRUE;
			foundCount += 1;
		}

		if (foundMatch == FALSE) {
			ERRPRINTF(("found ROM Classes when none where expected\n"));
			retval = TEST_ERROR;
			goto done;
		}

		if (foundCount != expectedCount) {
			ERRPRINTF(("wrong number of rom classes returned\n"));
			retval = TEST_ERROR;
			goto done;
		} else {
			j9tty_printf(PORTLIB, "\t\tInfo:correct ROMClass count\n");
		}
	}

	done:
	return retval;
}

static IDATA
test12(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	const char * testName = "test12";
	J9ROMClass * romclass = NULL;
	U_32 romclassSize = 1024;
	const char * romclassName = "CacheFullBitTestClassNonOrphan";
	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: set cache full bit, then try to allocate a non-orphan rom class\n", testName);
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));

	/* Step 1:
	 * Set cache full bit.
	 */
	vm->mainThread->javaVM->sharedClassConfig->runtimeFlags |= J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
	{
		/* Step 2:
		 * Allocate a non-orphan J9ROMClass in the cache.
		 */
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}

		sizes.romClassSizeFullSize = romclassSize;
		sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
		if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == true) {
			ERRPRINTF("should not be able to alloc memory for ROMClass in shared cache\n");
			return TEST_ERROR;
		} else {
			romclassmem = transaction.getRomClass();
		}
		
		if (romclassmem != NULL) {
			ERRPRINTF(("romclassmem != NULL\n"));
			retval = TEST_ERROR;
			goto done;
		}

		if (transaction.updateSharedClassSize(0) != -1) {
			ERRPRINTF("should not be able to update allocated ROMClass size\n");
			retval = TEST_ERROR;
			goto done;
		}
	}

	{
		/* Step 3:
		 * Make sure the new J9ROMClass is not in the cache.
		 */
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
		goto done;
		}

		for (; romclass != NULL; romclass = transaction.nextSharedClassForCompare()) {
			J9UTF8 * classnameUTF8 = NULL;
			classnameUTF8 = J9ROMCLASS_CLASSNAME(romclass);
			j9tty_printf(PORTLIB, "\t\tError: Found J9ROMClass named: %.*s at addr: 0x%lx\n", J9UTF8_LENGTH(classnameUTF8), J9UTF8_DATA(classnameUTF8), (UDATA) romclass);
			if (strcmp((char*)J9UTF8_DATA(classnameUTF8), romclassName) == 0){
				retval = TEST_ERROR;
			goto done;
			}
		}
	}

	done:
	/* Step 4:
	 * Clear the cache full bit
 */
	vm->mainThread->javaVM->sharedClassConfig->runtimeFlags &= ~J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
	return retval;
}

static IDATA
test13(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	const char * testName = "test13";
	J9ROMClass * romclass = NULL;
	U_32 romclassSize = 1024;
	const char * romclassName = "CacheFullBitTestClassOrphan";
	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: set cache full bit, then try to allocate an orphan rom class\n", testName);
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));

/* Step 1:
	 * Set cache full bit.
	 */
	vm->mainThread->javaVM->sharedClassConfig->runtimeFlags |= J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
	{
		/* Step 2:
		 * Allocate an orphan J9ROMClass in the cache.
		 */
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(romclassName), (U_8 *) romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}

		sizes.romClassSizeFullSize = romclassSize;
		sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
		if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == true) {
			ERRPRINTF("should not be able to alloc memory for ROMClass in shared cache\n");
			return TEST_ERROR;
		} else {
			romclassmem = transaction.getRomClass();
		}

		if (romclassmem != NULL) {
			ERRPRINTF(("getRomClass() != NULL\n"));
			retval = TEST_ERROR;
			goto done;
		}

		if (transaction.updateSharedClassSize(0) != -1) {
			ERRPRINTF("should not be able to update allocated ROMClass size\n");
			retval = TEST_ERROR;
			goto done;
		}
	}

	{
		/* Step 3:
		 * Make sure the new J9ROMClass is not in the cache.
		 */
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(romclassName), (U_8 *) romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}

		for (romclass = transaction.nextSharedClassForCompare(); romclass != NULL; romclass = transaction.nextSharedClassForCompare()) {
			J9UTF8 * classnameUTF8 = NULL;
			classnameUTF8 = J9ROMCLASS_CLASSNAME(romclass);
			j9tty_printf(PORTLIB, "\t\tError: Found J9ROMClass named: %.*s at addr: 0x%lx\n", J9UTF8_LENGTH(classnameUTF8), J9UTF8_DATA(classnameUTF8), (UDATA) romclass);
			if (strcmp((char*) J9UTF8_DATA(classnameUTF8), romclassName) == 0) {
				retval = TEST_ERROR;
				goto done;
			}
		}
	}

	done:

	/* Step 4:
	 * Clear the cache full bit
	 */
	vm->mainThread->javaVM->sharedClassConfig->runtimeFlags &= ~J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
	return retval;
}

static IDATA
test14(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	const char * testName = "test14";
	J9ROMClass * romclass = NULL;
	U_32 romclassSize = 1024;
	const char * romclassName = "NonOrphanClassCacheFullBitTest";
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));
	j9tty_printf(PORTLIB, "%s: allocate a non-orphan class, set the cache full bit, then try to find the rom class in the cache\n", testName);

	/* Step 1:
	 * Allocate a non-orphan J9ROMClass in the cache.
	 */
	{
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
		if (transaction.isOK() == false) {
		j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}

		sizes.romClassSizeFullSize = romclassSize;
		sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
		if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == false) {
			ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
			return TEST_ERROR;
		} else {
			romclassmem = transaction.getRomClass();
		}
		
		if (NULL == romclassmem) {
			ERRPRINTF(("romclassmem == NULL\n"));
			retval = TEST_ERROR;
			goto done;
		}

		romclass = (J9ROMClass*) romclassmem;
		romclass->romSize = romclassSize;
		SetRomClassName(vm, romclass, romclassName);
	}

	/* Step 2:
     * Set the cache full bit.
     */
	vm->mainThread->javaVM->sharedClassConfig->runtimeFlags |= J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;

	/* Step 3:
    * Try to find the allocated J9ROMClass.
     */
	{
		SCStoreTransaction transaction(vm->mainThread,(J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}

		retval = TEST_ERROR;
		for (; romclass != NULL; romclass = transaction.nextSharedClassForCompare()) {
			J9UTF8 * classnameUTF8 = NULL;
			classnameUTF8 = J9ROMCLASS_CLASSNAME(romclass);
			j9tty_printf(PORTLIB, "\t\tInfo: Found J9ROMClass named: %.*s at addr: 0x%lx\n", J9UTF8_LENGTH(classnameUTF8), J9UTF8_DATA(classnameUTF8), (UDATA) romclass);
			if (strcmp((char*)J9UTF8_DATA(classnameUTF8), romclassName) == 0){
				retval = TEST_PASS;
				goto done;
			}
		}
		ERRPRINTF(("could not find the rom class\n"));
	}

	done:
	/* Step 4:
     * Clear the cache full bit.
     */
	vm->mainThread->javaVM->sharedClassConfig->runtimeFlags &= ~J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
	return retval;
}

static IDATA
test15(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	J9ROMClass* newROMClass = NULL;
	void * romclassmem = NULL;
	const char * testName = "test15";
	J9ROMClass * romclass = NULL;
	U_32 romclassSize = 1024;
	const char * romclassName = "CacheFullBitMetadataUpdateClassOrphan";
	SCAbstractAPI * sharedapi = (SCAbstractAPI *)(vm->mainThread->javaVM->sharedClassConfig->sharedAPIObject);
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));
	j9tty_printf(PORTLIB, "%s: allocate an orphan rom class, set cache full bit, then try to update the orphan rom class's metadata\n", testName);

	/* Step 1:
	 * Allocate an orphan J9ROMClass in the cache.
	 */
	{
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}

		sizes.romClassSizeFullSize = romclassSize;
		sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
		if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == false) {
			ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
			return TEST_ERROR;
		} else {
			romclassmem = transaction.getRomClass();
		}
		
		if (NULL == romclassmem) {
			ERRPRINTF(("romclassmem == NULL\n"));
			retval = TEST_ERROR;
			goto done;
		}

		romclass = (J9ROMClass*) romclassmem;
		romclass->romSize = romclassSize;
		SetRomClassName(vm, romclass, romclassName);
	}

	/* Step 2:
	 * Set the cache full bit.
	 */
	vm->mainThread->javaVM->sharedClassConfig->runtimeFlags |= J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;

	/* Step 3:
	 * Update the orphan J9ROMClass's metadata.
	 */
	omrthread_monitor_enter(vm->classTableMutex);
	newROMClass = sharedapi->jclUpdateROMClassMetaData(vm->mainThread, (J9ClassLoader*) vm->applicationClassLoader, NULL, 1, 0/*entryIndex*/, NULL, romclass);
	omrthread_monitor_exit(vm->classTableMutex);

	if (newROMClass != NULL){
		ERRPRINTF(("should not be able to update rom class metadata\n"));
		retval = TEST_ERROR;
		goto done;
	}

	/* Step 4:
	 * Look for the allocated orphan J9ROMClass with a NULL classloader.
	 */
	{
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}

		retval = TEST_ERROR;
		for (; romclass != NULL; romclass = transaction.nextSharedClassForCompare()) {
			J9UTF8 * classnameUTF8 = NULL;
			classnameUTF8 = J9ROMCLASS_CLASSNAME(romclass);
			j9tty_printf(PORTLIB, "\t\tInfo: Found J9ROMClass named: %.*s at addr: 0x%lx\n", J9UTF8_LENGTH(classnameUTF8), J9UTF8_DATA(classnameUTF8), (UDATA) romclass);
			if (strcmp((char*)J9UTF8_DATA(classnameUTF8), romclassName) == 0){
				retval = TEST_PASS;
				goto done;
			}
		}
		ERRPRINTF(("could not find the rom class\n"));
	}

	done:
	/* Step 5:
	 * Clear the cache full bit.
	 */
	vm->mainThread->javaVM->sharedClassConfig->runtimeFlags &= ~J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
	return retval;
}

static IDATA
test16(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	const char * testName = "test16";
	J9ROMClass * romclass = NULL;
	U_32 romclassSize = ROMCLASS_LARGE_SIZE;
	const char * romclassName = "CacheFilledTestClassNonOrphan";
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));
	j9tty_printf(PORTLIB,"%s: fill the cache with non-orphan rom class larger than the cache, verify cache full bit is not set, allocate non-orphan rom class larger than the cache\n", testName);

	/* Step 1:
	 * Make sure the cache full bit is cleared.
	 */
	if ((vm->mainThread->javaVM->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL) != 0){
		vm->mainThread->javaVM->sharedClassConfig->runtimeFlags &= ~J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
	}

/* Step 2:
	 * Allocate a non-orphan J9ROMClass larger than the cache.
	 */
	{
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(romclassName), (U_8 *) romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}

		sizes.romClassSizeFullSize = romclassSize;
		sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
		if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == true) {
			ERRPRINTF("should not be able to alloc memory for ROMClass with size exceeding the size of cache\n");
			return TEST_ERROR;
		} else {
			romclassmem = transaction.getRomClass();
		}
		
		if (romclassmem != NULL) {
			ERRPRINTF(("getRomClass() != NULL\n"));
			return TEST_ERROR;
		}

		if (transaction.updateSharedClassSize(0) != -1) {
			ERRPRINTF("should not be able to update allocated ROMClass size\n");
			retval = TEST_ERROR;
			goto done;
		}
	}

	/* Step 3:
	 * Flag for BLOCK_SPACE_FULL is not set if SH_CompositeCacheImpl::allocate() fails.
	 * It is set only when data is written in last CC_MIN_SPACE_BEFORE_CACHE_FULL free block bytes.
	 */
	if ((vm->mainThread->javaVM->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL) != 0){
		ERRPRINTF("cache full bit is set\n");
		retval = TEST_ERROR;
		goto done;
	}

	/* Step 4:
	 * Make sure the non-orphan J9ROMClass is not in the cache.
	 */
	{
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(romclassName), (U_8 *) romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}

		for (romclass = transaction.nextSharedClassForCompare(); romclass != NULL; romclass = transaction.nextSharedClassForCompare()) {
			J9UTF8 * classnameUTF8 = NULL;
			classnameUTF8 = J9ROMCLASS_CLASSNAME(romclass);
			j9tty_printf(PORTLIB, "\t\tInfo: Found J9ROMClass named: %.*s at addr: 0x%lx\n", J9UTF8_LENGTH(classnameUTF8), J9UTF8_DATA(classnameUTF8), (UDATA) romclass);
			if (strcmp((char*) J9UTF8_DATA(classnameUTF8), romclassName) == 0) {
				retval = TEST_ERROR;
				goto done;
			}
		}
	}

	done:
	/* Step 5:
	 * Clear the cache full bit.
	 */
	vm->mainThread->javaVM->sharedClassConfig->runtimeFlags	&= ~J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
	return retval;
}

static IDATA
test17(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	const char * testName = "test17";
	J9ROMClass * romclass = NULL;
	U_32 romclassSize = ROMCLASS_LARGE_SIZE;
	const char * romclassName = "CacheFilledTestClassOrphan";
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));
	j9tty_printf(PORTLIB,"%s: fill the cache with orphan rom class larger than the cache, verify cache full bit is not set, allocate orphan rom class larger than the cache\n", testName);

	/* Step 1:
	 * Make sure the cache full bit is cleared.
	 */
	if ((vm->mainThread->javaVM->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL) != 0){
		vm->mainThread->javaVM->sharedClassConfig->runtimeFlags &= ~J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
	}

	/* Step 2:
	 * Allocate an orphan J9ROMClass larger than the cache.
	 */
	{
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(romclassName), (U_8 *) romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}

		sizes.romClassSizeFullSize = romclassSize;
		sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
		if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == true) {
			ERRPRINTF("should not be able to alloc memory for ROMClass with size exceeding the size of cache\n");
			return TEST_ERROR;
		} else {
			romclassmem = transaction.getRomClass();
		}
		
		if (romclassmem != NULL) {
			ERRPRINTF(("romclassmem != NULL\n"));
			return TEST_ERROR;
		}

		if (transaction.updateSharedClassSize(0) != -1) {
			ERRPRINTF("should not be able to update allocated ROMClass size\n");
			retval = TEST_ERROR;
			goto done;
		}
	}

	/* Step 3:
	 * Flag for BLOCK_SPACE_FULL is not set if SH_CompositeCacheImpl::allocate() fails.
	 * It is set only when data is written in last CC_MIN_SPACE_BEFORE_CACHE_FULL free block bytes.
	 */
	if ((vm->mainThread->javaVM->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL) != 0){
		ERRPRINTF("cache full bit is set\n");
		retval = TEST_ERROR;
		goto done;
	}

	/* Step 4:
	 * Make sure the orphan J9ROMClass is not in the cache.
	 */
	{
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(romclassName), (U_8 *) romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}

		for (romclass = transaction.nextSharedClassForCompare(); romclass != NULL; romclass = transaction.nextSharedClassForCompare()) {
			J9UTF8 * classnameUTF8 = NULL;
			classnameUTF8 = J9ROMCLASS_CLASSNAME(romclass);
		j9tty_printf(PORTLIB, "\t\tInfo: Found J9ROMClass named: %.*s at addr: 0x%lx\n", J9UTF8_LENGTH(classnameUTF8), J9UTF8_DATA(classnameUTF8), (UDATA) romclass);
			if (strcmp((char*) J9UTF8_DATA(classnameUTF8), romclassName) == 0) {
			retval = TEST_ERROR;
				goto done;
			}
		}
	}

	done:
/* Step 5:
	 * Clear the cache full bit.
	 */
	vm->mainThread->javaVM->sharedClassConfig->runtimeFlags	&= ~J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
	return retval;
}

static IDATA
test18(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	const char * testName = "test18";
	J9ROMClass * romclass = NULL;
	U_32 romclassSize = 1024;
	U_32 romclassSizeLarge = ROMCLASS_LARGE_SIZE;
	const char * romclassName = "NonOrphanClassCacheFilledTest";
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));
	j9tty_printf(PORTLIB, "%s: allocate non-orphan rom class, fill the cache with non-orphan rom class larger than the size of cache\n", testName);

	/* Step 1:
	 * Allocate a non-orphan J9ROMClass in the cache.
	 */
	{
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(romclassName), (U_8 *) romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			return TEST_ERROR;
		}

		sizes.romClassSizeFullSize = romclassSize;
		sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
		if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == false) {
			ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
			return TEST_ERROR;
		} else {
			romclassmem = transaction.getRomClass();
		}
		
		if (NULL == romclassmem) {
			ERRPRINTF(("romclassmem == NULL\n"));
			return TEST_ERROR;
		}

		romclass = (J9ROMClass*) romclassmem;
		romclass->romSize = romclassSize;
		SetRomClassName(vm, romclass, romclassName);
	}

	/* Step 2:
	 * Make sure the cache full bit is cleared.
	 */
	if ((vm->mainThread->javaVM->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL) != 0){
		vm->mainThread->javaVM->sharedClassConfig->runtimeFlags &= ~J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
	}

	/* Step 3:
	 * Allocate a J9ROMClass larger than the size of cache
	 */
	{
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(romclassName), (U_8 *) romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}

		sizes.romClassSizeFullSize = romclassSizeLarge;
		sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
		if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == true) {
			ERRPRINTF("should not be able to alloc memory for ROMClass with size exceeding the size of cache\n");
			return TEST_ERROR;
		} else {
			romclassmem = transaction.getRomClass();
		}
		
		if (romclassmem != NULL) {
			ERRPRINTF(("getRomClass() != NULL\n"));
			return TEST_ERROR;
		}

	}

	/* Step 4:
	 * Flag for BLOCK_SPACE_FULL is not set if SH_CompositeCacheImpl::allocate() fails.
	 * It is set only when data is written in last CC_MIN_SPACE_BEFORE_CACHE_FULL free block bytes.
	 */
	if ((vm->mainThread->javaVM->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL) != 0){
		ERRPRINTF("cache full bit is set\n");
		retval = TEST_ERROR;
		goto done;
	}

	/* Step 5:
	 * Make sure the first non-orphan J9ROMClass is in the cache.
	 */
	{
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(romclassName), (U_8 *) romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}

		retval = TEST_ERROR;
		for (; romclass != NULL; romclass = transaction.nextSharedClassForCompare()) {
			J9UTF8 * classnameUTF8 = NULL;
			classnameUTF8 = J9ROMCLASS_CLASSNAME(romclass);
			j9tty_printf(PORTLIB, "\t\tInfo: Found J9ROMClass named: %.*s at addr: 0x%lx\n", J9UTF8_LENGTH(classnameUTF8), J9UTF8_DATA(classnameUTF8), (UDATA) romclass);
			if (strcmp((char*) J9UTF8_DATA(classnameUTF8), romclassName) == 0) {
				retval = TEST_PASS;
				goto done;
			}
		}
		ERRPRINTF(("could not find the rom class\n"));
	}

	done:
	/* Step 6:
	 * Clear the cache full bit.
	 */
	vm->mainThread->javaVM->sharedClassConfig->runtimeFlags &= ~J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
	return retval;
}

static IDATA
test19(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	J9ROMClass* newROMClass = NULL;
	void * romclassmem = NULL;
	const char * testName = "test19";
	J9ROMClass * romclass = NULL;
	U_32 romclassSize = 1024;
	U_32 romclassSizeLarge = ROMCLASS_LARGE_SIZE;
	const char * romclassName = "CacheFilledMetadataUpdateClassOrphan";
	const char * testclassName = "TestROMClass";
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));
	j9tty_printf(PORTLIB, "%s: allocate orphan rom class, then allocate another rom class of size large than the cache, update metadata of first orphan rom class\n", testName);

	/* Step 1:
	 * Allocate an orphan J9ROMClass in the cache.
	 */
	{
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen( romclassName), (U_8 *) romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}

		sizes.romClassSizeFullSize = romclassSize;
		sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
		if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == false) {
			ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
			return TEST_ERROR;
		} else {
			romclassmem = transaction.getRomClass();
		}

		if (NULL == romclassmem) {
			ERRPRINTF(("romclassmem == NULL\n"));
			retval = TEST_ERROR;
			goto done;
		}

		romclass = (J9ROMClass*) romclassmem;
		romclass->romSize = romclassSize;
		SetRomClassName(vm, romclass, romclassName);
	}

	/* Step 2:
	 * Make sure the cache full bit is cleared.
	 */
	if ((vm->mainThread->javaVM->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL) != 0){
		vm->mainThread->javaVM->sharedClassConfig->runtimeFlags &= ~J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
	}

	/* Step 3:
	 * Allocate a J9ROMClass larger than the size of cache
	 */
	{
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(testclassName), (U_8 *) testclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}

		sizes.romClassSizeFullSize = romclassSizeLarge;
		sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
		if (transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes) == true) {
			ERRPRINTF("should not be able to alloc memory for ROMClass with size exceeding the size of cache\n");
			return TEST_ERROR;
		} else {
			romclassmem = transaction.getRomClass();
		}

		if (romclassmem != NULL) {
			ERRPRINTF(("getRomClass() != NULL\n"));
			retval = TEST_ERROR;
			goto done;
		}
	}

	/* Step 4:
	 * Flag for BLOCK_SPACE_FULL is not set if SH_CompositeCacheImpl::allocate() fails.
	 * It is set only when data is written in last CC_MIN_SPACE_BEFORE_CACHE_FULL free block bytes.
	 */
	if ((vm->mainThread->javaVM->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL) != 0){
		ERRPRINTF("cache full bit is set\n");
		retval = TEST_ERROR;
		goto done;
	}

	/* Step 5:
	 * Update the orphan J9ROMClass's metadata.
	 */
	{
		SCAbstractAPI * sharedapi = (SCAbstractAPI *) (vm->mainThread->javaVM->sharedClassConfig->sharedAPIObject);
		omrthread_monitor_enter(vm->classTableMutex);
		newROMClass = sharedapi->jclUpdateROMClassMetaData(vm->mainThread, (J9ClassLoader*) vm->applicationClassLoader, NULL, 1, 0/*entryIndex*/, NULL, romclass);
		omrthread_monitor_exit(vm->classTableMutex);
		if (newROMClass != NULL) {
			ERRPRINTF(("should not be able to update rom class metadata\n"));
			retval = TEST_ERROR;
			goto done;
		}
	}

	/* Step 6:
	 * Look for the allocated orphan J9ROMClass with a NULL classloader.
	 */
	{
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(romclassName), (U_8 *) romclassName, false);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}
		for (; romclass != NULL; romclass = transaction.nextSharedClassForCompare()) {
			retval = TEST_ERROR;
			J9UTF8 * classnameUTF8 = NULL;
			classnameUTF8 = J9ROMCLASS_CLASSNAME(romclass);
			j9tty_printf(PORTLIB, "\t\tInfo: Found J9ROMClass named: %.*s at addr: 0x%lx\n", J9UTF8_LENGTH(classnameUTF8), J9UTF8_DATA(classnameUTF8), (UDATA) romclass);
			if (strcmp((char*) J9UTF8_DATA(classnameUTF8), romclassName) == 0) {
				retval = TEST_PASS;
				goto done;
			}
		}
		ERRPRINTF(("could not find the rom class\n"));
	}

	done:
	/* Step 7:
	 * Clear the cache full bit.
	 */
	vm->mainThread->javaVM->sharedClassConfig->runtimeFlags &= ~J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL;
	return retval;
}

static IDATA
test20(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	BOOLEAN foundMatch = FALSE;
	const char * testName = "test20";
	J9ROMClass * romclass = NULL;
	U_32 romclassSize = 1024;
	const char * romclassName = "test20J9ROMClassNonOrphan";
	SCAbstractAPI * sharedapi = (SCAbstractAPI *)(vm->sharedClassConfig->sharedAPIObject);
	J9SharedClassTransaction tobj;
	BOOLEAN takeStringTableLock = FALSE;
	J9RomClassRequirements sizes;
	J9SharedRomClassPieces pieces;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));
	memset((void *)&pieces,0,sizeof(J9SharedRomClassPieces));

	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: allocate a non-orphan orphan rom class in the cache, but WITHOUT the string table lock\n", testName);


	/* Step 1:
	 * Allocate a J9ROMClass in the cache without using the string table mutex.
	 * We use the C interface so we can explicitly request not to use the string table lock.
	 */
	sharedapi->classStoreTransaction_start((void *) &tobj,
			vm->mainThread,
			(J9ClassLoader*) vm->systemClassLoader,
			vm->systemClassLoader->classPathEntries,
			vm->systemClassLoader->classPathEntryCount,
			0,
			J9SHR_LOADTYPE_NORMAL,
			NULL,
			(U_16) strlen(romclassName),
			(U_8 *) romclassName,
			FALSE,
			takeStringTableLock);

	if (sharedapi->classStoreTransaction_isOK((void *) &tobj) == FALSE) {
		ERRPRINTF(("The transaction is not ok\n"));
		retval = TEST_ERROR;
		goto done;
	}

	sizes.romClassSizeFullSize = romclassSize;
	sizes.romClassMinimalSize = sizes.romClassSizeFullSize;
	if (sharedapi->classStoreTransaction_createSharedClass((void *) &tobj, (const J9RomClassRequirements *)&sizes, &pieces) != 0) {
		ERRPRINTF(("The J9ROMClass could not be allocated\n"));
		return TEST_ERROR;
	} else {
		romclass = (J9ROMClass *)pieces.romClass;
	}
	
	if (NULL == romclass) {
		ERRPRINTF(("romclassmem == NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}
	
	romclass->romSize = romclassSize;
	SetRomClassName(vm, romclass, romclassName);

	sharedapi->classStoreTransaction_stop((void *) &tobj);

	/* Step 2:
	 * Make sure the new J9ROMClass is in the cache.
	 */
	{
		int foundCount = 0;
		int expectedCount = 1;
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(romclassName), (U_8 *) romclassName, true);
		if (transaction.isOK() == false) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			retval = TEST_ERROR;
			goto done;
		}

		for (romclass = transaction.nextSharedClassForCompare(); romclass != NULL; romclass = transaction.nextSharedClassForCompare()) {
			J9UTF8 * classnameUTF8 = NULL;
			classnameUTF8 = J9ROMCLASS_CLASSNAME(romclass);
			j9tty_printf(PORTLIB, "\t\tInfo: Found J9ROMClass named: %.*s at addr: 0x%lx\n", J9UTF8_LENGTH(classnameUTF8), J9UTF8_DATA(classnameUTF8), (UDATA) romclass);
			foundMatch = TRUE;
			foundCount += 1;
			break;
		}

		if (foundMatch == FALSE) {
			ERRPRINTF(("found ROM Classes when none where expected\n"));
			retval = TEST_ERROR;
			goto done;
		}

		if (foundCount != expectedCount) {
			ERRPRINTF(("wrong number of rom classes returned\n"));
			retval = TEST_ERROR;
			goto done;
		}
	}
	done:
	return retval;
}

static IDATA
test21(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	const char * testName = "test21";
	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: Exercise isCacheFull() function\n", testName);
	BOOLEAN cacheIsFull = ((vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL) != 0);
	BOOLEAN apiResult = j9shr_Query_IsCacheFull(vm);

	/*This test is a very simple sanity check that isCacheFull() can be called.*/
	/*This test doesn't really belong in this file ... but its fine here for now.*/

	j9tty_printf(PORTLIB, "\t\tInfo:j9shr_Query_IsCacheFull()=%d and %d was expected\n", (IDATA)apiResult,(IDATA)cacheIsFull);
	if (apiResult != cacheIsFull) {
		ERRPRINTF(("j9shr_Query_IsCacheFull() failed\n"));
		retval = TEST_ERROR;
		goto done;
	}
	done:
	return retval;
}

static IDATA
test22(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	const char * testName = "test22";
	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: Exercise isAddressInCache() function\n", testName);
	J9SharedClassCacheDescriptor *cache = vm->sharedClassConfig->cacheDescriptorList;

	struct {
		void *address;
		UDATA length;
		BOOLEAN expectedResult;
	} tests[] = {
		{ cache->cacheStartAddress, 0, TRUE },
		{ cache->cacheStartAddress, cache->cacheSizeBytes, TRUE },
		{ cache->cacheStartAddress, cache->cacheSizeBytes - 1, TRUE },
		{ cache->cacheStartAddress, cache->cacheSizeBytes + 1, FALSE },
		{ NULL, cache->cacheSizeBytes, FALSE }
	};
	/* numTests is the number of elements in the above array tests. */
	const UDATA numTests = 5;

	for (UDATA i = 0; i < numTests; i++) {
		BOOLEAN addressInCache = j9shr_Query_IsAddressInCache(vm, tests[i].address, tests[i].length);

		if (addressInCache != tests[i].expectedResult) {
			ERRPRINTF4("%u. j9shr_Query_IsAddressInCache(vm, %p, %u) returned %s\n",
					i, tests[i].address, tests[i].length,
					addressInCache ? "TRUE" : "FALSE");
			retval = TEST_ERROR;
			goto done;
		}
	}

	done:
	return retval;
}

static IDATA
test23(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	const char * testName = "test23";
	J9ROMClass * romclass = NULL;
	const char * romclassName = "test23J9ROMClassOrphan";
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));

	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: Alloc 4096 B orphan rom class, 4096 B of line number info, and 4096 B of var table info\n", testName);

	SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
	if (false == transaction.isOK()) {
		j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
		return TEST_ERROR;
	}
	
	/*Choose sizes that will force memory to be protected atleast on linux hammer*/
	sizes.romClassSizeFullSize      = (4096*3);
	sizes.romClassMinimalSize  = 4096;
	sizes.lineNumberTableSize       = 4096;
	sizes.localVariableTableSize    = 4096;
	
	if (false == transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes)) {
		ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
		return TEST_ERROR;
	} else {
		romclassmem = transaction.getRomClass();
	}

	if (NULL == romclassmem) {
		ERRPRINTF(("romclassmem == NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}
	
	if (true == transaction.hasAllocatedFullSizeRomClass()) {
		ERRPRINTF(("No debug data was allocated\n"));
		retval = TEST_ERROR;
		goto done;
	} else {
		romclass = (J9ROMClass*) romclassmem;
		romclass->romSize = sizes.romClassMinimalSize;
	}
	
	if (FALSE == j9shr_isAddressInCache(vm, romclassmem, sizes.romClassMinimalSize)) {
		ERRPRINTF("Allocated memory doesn't appear to be in the cache\n");
		retval = TEST_ERROR;
		goto done;
	}

	if (NULL == transaction.getLineNumberTable()) {
		ERRPRINTF(("getLineNumberTable()== NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}
	
	if (NULL == transaction.getLocalVariableTable()) {
		ERRPRINTF(("getLocalVariableTable()== NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}

	/* Set the allocated memory to ensure we don't overlap with metadata, or mprotect.
	 * If we do have a problem likely tests following this one will fail
	 */
	memset(transaction.getLocalVariableTable(),0xff,sizes.localVariableTableSize);
	memset(transaction.getLineNumberTable(),0xff,sizes.lineNumberTableSize);

	SetRomClassName(vm, romclass, romclassName);

	done:
	return retval;
}

static IDATA
test24(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	const char * testName = "test24";
	J9ROMClass * romclass = NULL;
	const char * romclassName = "test24J9ROMClassNonOrphan";
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));

	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: Alloc 4096 B non-orphan rom class, 4096 B of line number info, and 4096 B of var table info\n", testName);

	SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) vm->systemClassLoader, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
	if (false == transaction.isOK()) {
		j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
		return TEST_ERROR;
	}

	/*Choose sizes that will force memory to be protected atleast on linux hammer*/
	sizes.romClassSizeFullSize      = (4096*3);
	sizes.romClassMinimalSize  = 4096;
	sizes.lineNumberTableSize       = 4096;
	sizes.localVariableTableSize    = 4096;

	if (false == transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes)) {
		ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
		return TEST_ERROR;
	} else {
		romclassmem = transaction.getRomClass();
	}

	if (NULL == romclassmem) {
		ERRPRINTF(("romclassmem == NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}

	if (transaction.hasAllocatedFullSizeRomClass() == true) {
		ERRPRINTF(("No debug data was allocated\n"));
		retval = TEST_ERROR;
		goto done;
	} else {
		romclass = (J9ROMClass*) romclassmem;
		romclass->romSize = sizes.romClassMinimalSize;
	}

	if (FALSE == j9shr_isAddressInCache(vm, romclassmem, sizes.romClassMinimalSize)) {
		ERRPRINTF("Allocated memory doesn't appear to be in the cache\n");
		retval = TEST_ERROR;
		goto done;
	}

	if (NULL == transaction.getLineNumberTable()) {
		ERRPRINTF(("getLineNumberTable()== NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}

	if (NULL == transaction.getLocalVariableTable()) {
		ERRPRINTF(("getLocalVariableTable()== NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}

	/* Set the allocated memory to ensure we don't overlap with metadata, or mprotect.
	 * If we do have a problem likely tests following this one will fail
	 */
	memset(transaction.getLocalVariableTable(),0xff,sizes.localVariableTableSize);
	memset(transaction.getLineNumberTable(),0xff,sizes.lineNumberTableSize);

	SetRomClassName(vm, romclass, romclassName);

	done:
	return retval;
}

static IDATA
test25(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	const char * testName = "test25";
	J9ROMClass * romclass = NULL;
	const char * romclassName = "test25J9ROMClassOrphan";
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));

	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: Verify hasAllocatedFullSizeRomClass(), getLineNumberTable(), and getLocalVariableTable() with 0 bytes of debug attribute data\n", testName);

	SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
	if (false == transaction.isOK()) {
		j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
		return TEST_ERROR;
	}

	sizes.romClassSizeFullSize      = 1024;
	sizes.romClassMinimalSize  = 512;
	sizes.lineNumberTableSize       = 0;
	sizes.localVariableTableSize    = 0;

	if (false == transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes)) {
		ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
		return TEST_ERROR;
	} else {
		romclassmem = transaction.getRomClass();
	}

	if (NULL == romclassmem) {
		ERRPRINTF(("romclassmem == NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}

	if (true == transaction.hasAllocatedFullSizeRomClass()) {
		romclass = (J9ROMClass*) romclassmem;
		romclass->romSize = sizes.romClassSizeFullSize;
	} else {
		ERRPRINTF(("No debug data was allocated\n"));
		retval = TEST_ERROR;
		goto done;
	}

	if (NULL != transaction.getLineNumberTable()) {
		ERRPRINTF(("getLineNumberTable() != NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}

	if (NULL != transaction.getLocalVariableTable()) {
		ERRPRINTF(("getLocalVariableTable() != NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}

	SetRomClassName(vm, romclass, romclassName);

	done:
	return retval;
}

static IDATA
test26(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	const char * testName = "test26";
	J9ROMClass * romclass = NULL;
	const char * romclassName = "test26J9ROMClassOrphan";
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));

	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: Allocate orphan with only LineNumberTable debug data\n", testName);

	SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
	if (false == transaction.isOK()) {
		j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
		return TEST_ERROR;
	}

	sizes.romClassSizeFullSize      = 1024;
	sizes.romClassMinimalSize  = 512;
	sizes.lineNumberTableSize       = 512;
	sizes.localVariableTableSize    = 0;

	if (false == transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes)) {
		ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
		return TEST_ERROR;
	} else {
		romclassmem = transaction.getRomClass();
	}

	if (NULL == romclassmem) {
		ERRPRINTF(("romclassmem == NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}

	if (true == transaction.hasAllocatedFullSizeRomClass()) {
		ERRPRINTF(("No debug data was allocated\n"));
		retval = TEST_ERROR;
		goto done;
	} else {
		romclass = (J9ROMClass*) romclassmem;
		romclass->romSize = sizes.romClassSizeFullSize;
	}

	if (NULL == transaction.getLineNumberTable()) {
		ERRPRINTF(("getLineNumberTable() == NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}

	if (NULL != transaction.getLocalVariableTable()) {
		ERRPRINTF(("getLocalVariableTable() != NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}

	/* Set the allocated memory to ensure we don't overlap with metadata, or mprotect.
	 * If we do have a problem likely tests following this one will fail
	 */
	memset(transaction.getLineNumberTable(),0xff,sizes.lineNumberTableSize);

	SetRomClassName(vm, romclass, romclassName);

	done:
	return retval;
}

static IDATA
test27(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	const char * testName = "test27";
	J9ROMClass * romclass = NULL;
	const char * romclassName = "test27J9ROMClassOrphan";
	J9RomClassRequirements sizes;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));

	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: Allocate orphan with only LocalVariableTable debug data\n", testName);

	SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16)strlen(romclassName), (U_8 *)romclassName, false);
	if (false == transaction.isOK()) {
		j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
		return TEST_ERROR;
	}

	sizes.romClassSizeFullSize      = 1024;
	sizes.romClassMinimalSize  = 512;
	sizes.lineNumberTableSize       = 0;
	sizes.localVariableTableSize    = 512;

	if (false == transaction.allocateSharedClass((const J9RomClassRequirements *)&sizes)) {
		ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
		return TEST_ERROR;
	} else {
		romclassmem = transaction.getRomClass();
	}

	if (NULL == romclassmem) {
		ERRPRINTF(("romclassmem == NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}

	if (true == transaction.hasAllocatedFullSizeRomClass()) {
		ERRPRINTF(("No debug data was allocated\n"));
		retval = TEST_ERROR;
		goto done;
	} else {
		romclass = (J9ROMClass*) romclassmem;
		romclass->romSize = sizes.romClassSizeFullSize;
	}

	if (NULL != transaction.getLineNumberTable()) {
		ERRPRINTF(("getLineNumberTable() != NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}

	if (NULL == transaction.getLocalVariableTable()) {
		ERRPRINTF(("getLocalVariableTable() == NULL\n"));
		retval = TEST_ERROR;
		goto done;
	}

	/* Set the allocated memory to ensure we don't overlap with metadata, or mprotect.
	 * If we do have a problem likely tests following this one will fail
	 */
	memset(transaction.getLocalVariableTable(),0xff,sizes.localVariableTableSize);

	SetRomClassName(vm, romclass, romclassName);

	done:
	return retval;
}

static IDATA
test29(J9JavaVM* vm)
{
	IDATA retval = TEST_PASS;
	void * romclassmem = NULL;
	const char * testName = "test29";
	J9ROMClass * romclass = NULL;
	const char * romclassName1 = "test29J9ROMClassOrphan1";
	const char * romclassName2 = "test29J9ROMClassOrphan2";
	J9RomClassRequirements sizes;
	void * firstLineNumberTableAddr = NULL;
	void * firstLocalVariableTableAddr = NULL;
	memset((void *)&sizes,0,sizeof(J9RomClassRequirements));



	PORT_ACCESS_FROM_JAVAVM(vm);
	j9tty_printf(PORTLIB, "%s: Allocate two ROM Classes and ensure debug data does not overlap\n", testName);

	/*New scope to ensure ~SCStoreTransaction() runs*/
	{
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(romclassName1), (U_8 *) romclassName1, false);
		if (false == transaction.isOK()) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			return TEST_ERROR;
		}

		sizes.romClassSizeFullSize = 1024;
		sizes.romClassMinimalSize = 512;
		sizes.lineNumberTableSize = 128;
		sizes.localVariableTableSize = 128;

		if (false == transaction.allocateSharedClass((const J9RomClassRequirements *) &sizes)) {
			ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
			return TEST_ERROR;
		} else {
			romclassmem = transaction.getRomClass();
		}

		if (NULL == romclassmem) {
			ERRPRINTF(("romclassmem == NULL\n"));
			retval = TEST_ERROR;
			goto done;
		}

		if (true == transaction.hasAllocatedFullSizeRomClass()) {
			ERRPRINTF(("No debug data was allocated\n"));
			retval = TEST_ERROR;
			goto done;
		} else {
			romclass = (J9ROMClass*) romclassmem;
			romclass->romSize = sizes.romClassMinimalSize;
		}

		if (FALSE == j9shr_isAddressInCache(vm, romclassmem, sizes.romClassMinimalSize)) {
			ERRPRINTF("Allocated memory doesn't appear to be in the cache\n");
			retval = TEST_ERROR;
			goto done;
		}

		if (NULL == transaction.getLineNumberTable()) {
			ERRPRINTF(("getLineNumberTable()== NULL\n"));
			retval = TEST_ERROR;
			goto done;
		}

		if (NULL == transaction.getLocalVariableTable()) {
			ERRPRINTF(("getLocalVariableTable()== NULL\n"));
			retval = TEST_ERROR;
			goto done;
		}

		firstLineNumberTableAddr = transaction.getLineNumberTable();
		firstLocalVariableTableAddr = transaction.getLocalVariableTable();

		/* Set the allocated memory to ensure we don't overlap with metadata, or mprotect.
		 * If we do have a problem likely tests following this one will fail
		 */
		memset(transaction.getLocalVariableTable(),0xff,sizes.localVariableTableSize);
		memset(transaction.getLineNumberTable(),0xff,sizes.lineNumberTableSize);

		SetRomClassName(vm, romclass, romclassName1);
	}

	/*New scope to ensure ~SCStoreTransaction() runs*/
	{
		SCStoreTransaction transaction(vm->mainThread, (J9ClassLoader*) NULL, 0/*entryIndex*/, J9SHR_LOADTYPE_NORMAL, (U_16) strlen(romclassName2), (U_8 *) romclassName2, false);
		if (false == transaction.isOK()) {
			j9tty_printf(PORTLIB, "could not allocate the transaction object\n");
			return TEST_ERROR;
		}

		sizes.romClassSizeFullSize = 1024;
		sizes.romClassMinimalSize = 512;
		sizes.lineNumberTableSize = 128;
		sizes.localVariableTableSize = 128;

		if (false == transaction.allocateSharedClass((const J9RomClassRequirements *) &sizes)) {
			ERRPRINTF(("failure to alloc memory for ROMClass in shared cache\n"));
			return TEST_ERROR;
		} else {
			romclassmem = transaction.getRomClass();
		}

		if (NULL == romclassmem) {
			ERRPRINTF(("romclassmem == NULL\n"));
			retval = TEST_ERROR;
			goto done;
		}

		if (true == transaction.hasAllocatedFullSizeRomClass()) {
			ERRPRINTF(("No debug data was allocated\n"));
			retval = TEST_ERROR;
			goto done;
		} else {
			romclass = (J9ROMClass*) romclassmem;
			romclass->romSize = sizes.romClassMinimalSize;
		}

		if (FALSE == j9shr_isAddressInCache(vm, romclassmem, sizes.romClassMinimalSize)) {
			ERRPRINTF("Allocated memory doesn't appear to be in the cache\n");
			retval = TEST_ERROR;
			goto done;
		}

		if (NULL == transaction.getLineNumberTable()) {
			ERRPRINTF(("getLineNumberTable()== NULL\n"));
			retval = TEST_ERROR;
			goto done;
		}

		if (NULL == transaction.getLocalVariableTable()) {
			ERRPRINTF(("getLocalVariableTable()== NULL\n"));
			retval = TEST_ERROR;
			goto done;
		}

		if (firstLineNumberTableAddr == transaction.getLineNumberTable()) {
			ERRPRINTF("Allocated debug data memory is incorrect: firstLineNumberTableAddr == transaction.getLineNumberTable()\n");
			retval = TEST_ERROR;
			goto done;
		}
		if (firstLocalVariableTableAddr == transaction.getLocalVariableTable()) {
			ERRPRINTF("Allocated debug data memory is incorrect: firstLocalVariableTableAddr == transaction.getLocalVariableTable()\n");
			retval = TEST_ERROR;
			goto done;
		}

		/* Set the allocated memory to ensure we don't overlap with metadata, or mprotect.
		 * If we do have a problem likely tests following this one will fail
		 */
		memset(transaction.getLocalVariableTable(),0xff,sizes.localVariableTableSize);
		memset(transaction.getLineNumberTable(),0xff,sizes.lineNumberTableSize);

		SetRomClassName(vm, romclass, romclassName2);
	}

	done:
	return retval;
}

#endif /*defined(J9SHR_CACHELET_SUPPORT)*/
