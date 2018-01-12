/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

#include "ClasspathItem.hpp"
#include "sharedconsts.h"
#include "j9port.h"
#include "main.h"
#include "j9.h"
#include <string.h>

#define _UTE_STATIC_
#include "ut_j9shr.h"

#define CPLENGTH 6

extern "C" 
{ 
	IDATA testClasspathItem(J9JavaVM* vm); 
}

class ClasspathItemTest {
	public:
		static IDATA classpathEntryTest(J9JavaVM* vm);
		static IDATA createTest(J9JavaVM* vm, ClasspathItem** cpArray, char* stringBuf, IDATA entryLen);
		static IDATA serializeTest(J9JavaVM* vm, ClasspathItem** cpArray, ClasspathItem** serialArray);
		static IDATA compareTest(J9JavaVM* vm, ClasspathItem** cpArray, ClasspathItem** serialArray);
		static IDATA itemsTest(J9JavaVM* vm, ClasspathItem** cpArray, ClasspathItem** serialArray);
	private:
		static ClasspathEntryItem* createClasspathEntryItem(J9JavaVM* vm, const char* path, U_16 pathlen, UDATA protocol);
		static ClasspathItem* createClasspathItem(J9JavaVM* vm, IDATA entries, IDATA entryLen, IDATA fromHelperID, U_16 cpType, UDATA protocols, char** stringCursor);
		static IDATA compareGetters(J9JavaVM* vm, ClasspathItem* local, ClasspathItem* serial, UDATA cntr);
};

ClasspathEntryItem* 
ClasspathItemTest::createClasspathEntryItem(J9JavaVM* vm, const char* path, U_16 pathlen, UDATA protocol)
{
	ClasspathEntryItem* memForCPEI;
	ClasspathEntryItem* returnVal;

	PORT_ACCESS_FROM_JAVAVM(vm);

	memForCPEI = (ClasspathEntryItem*)j9mem_allocate_memory(sizeof(ClasspathEntryItem), J9MEM_CATEGORY_CLASSES);

	if (!memForCPEI) {
		return NULL;
	}
	memset(memForCPEI, 0, sizeof(ClasspathEntryItem));
	returnVal = ClasspathEntryItem::newInstance(path, pathlen, protocol, memForCPEI);
	return returnVal;
}

/* Note - ClasspathEntryItems are only normally created by ClasspathItems */
IDATA
ClasspathItemTest::classpathEntryTest(J9JavaVM* vm)
{
	ClasspathEntryItem *cpei1, *cpei2, *cpei3, *cpei4;
	ClasspathEntryItem *serial1, *serial2, *serial3, *serial4;
	const char* PATH1 = "";
	const char* PATH2 = "path2";
	const char* PATH3 = "abcdefghijklmnopqrstuvwxyz/abcdefghijklmnopqrstuvwxyz/abcdefghijklmnopqrstuvwxyz/abcdefghijklmnopqrstuvwxyz/jar.jar";
	const char* PATH4 = "/123456789012345678901234567890123456789012345678901234567890/";
	char *compare1, *compare2, *compare3, *compare4;
	char *compare1a, *compare2a, *compare3a, *compare4a;
	UDATA hash1, hash2, hash3, hash4;
	U_16 compareLen1, compareLen2, compareLen3, compareLen4;
	U_16 compareLen1a, compareLen2a, compareLen3a, compareLen4a;
	char *tempBlock, *nextBlock;
	IDATA size;
	
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (!(cpei1 = createClasspathEntryItem(vm, PATH1, (U_16)strlen(PATH1), 0))) {
		return 1;
	}
	if (!(cpei2 = createClasspathEntryItem(vm, PATH2, (U_16)strlen(PATH2), PROTO_JAR))) {
		return 2;
	}
	if (!(cpei3 = createClasspathEntryItem(vm, PATH3, (U_16)strlen(PATH3), PROTO_DIR))) {
		return 3;
	}
	if (!(cpei4 = createClasspathEntryItem(vm, PATH4, (U_16)strlen(PATH4), PROTO_TOKEN))) {
		return 4;
	}

	compare1 = (char*)cpei1->getPath(&compareLen1);
	compare2 = (char*)cpei2->getPath(&compareLen2);
	compare3 = (char*)cpei3->getPath(&compareLen3);
	compare4 = (char*)cpei4->getPath(&compareLen4);

	if (compareLen1 != strlen(PATH1)) return 5;
	if (compareLen2 != strlen(PATH2)) return 6;
	if (compareLen3 != strlen(PATH3)) return 7;
	if (compareLen4 != strlen(PATH4)) return 8;

	if (strncmp(PATH1, compare1, compareLen1)) return 9;
	if (strncmp(PATH2, compare2, compareLen2)) return 10;
	if (strncmp(PATH3, compare3, compareLen3)) return 11;
	if (strncmp(PATH4, compare4, compareLen4)) return 12;

	if ((size = cpei1->getSizeNeeded())) {
		if (!(tempBlock = (char*)j9mem_allocate_memory(size, J9MEM_CATEGORY_CLASSES))) {
			return 13;
		}
		memset(tempBlock, 0, size);
		nextBlock = cpei1->writeToAddress(tempBlock);
		serial1 = (ClasspathEntryItem*)tempBlock;
		if (nextBlock != (tempBlock + size)) {
			return 14;
		}
	} else {
		return 15;
	}

	if ((size = cpei2->getSizeNeeded())) {
		if (!(tempBlock = (char*)j9mem_allocate_memory(size, J9MEM_CATEGORY_CLASSES))) {
			return 16;
		}
		memset(tempBlock, 0, size);
		nextBlock = cpei2->writeToAddress(tempBlock);
		serial2 = (ClasspathEntryItem*)tempBlock;
		if (nextBlock != (tempBlock + size)) {
			return 17;
		}
	} else {
		return 18;
	}

	if ((size = cpei3->getSizeNeeded())) {
		if (!(tempBlock = (char*)j9mem_allocate_memory(size, J9MEM_CATEGORY_CLASSES))) {
			return 19;
		}
		memset(tempBlock, 0, size);
		nextBlock = cpei3->writeToAddress(tempBlock);
		serial3 = (ClasspathEntryItem*)tempBlock;
		if (nextBlock != (tempBlock + size)) {
			return 20;
		}
	} else {
		return 21;
	}

	if ((size = cpei4->getSizeNeeded())) {
		if (!(tempBlock = (char*)j9mem_allocate_memory(size, J9MEM_CATEGORY_CLASSES))) {
			return 22;
		}
		memset(tempBlock, 0, size);
		nextBlock = cpei4->writeToAddress(tempBlock);
		serial4 = (ClasspathEntryItem*)tempBlock;
		if (nextBlock != (tempBlock + size)) {
			return 23;
		}
	} else {
		return 24;
	}

	hash1 = cpei1->hash(vm->internalVMFunctions);
	hash2 = cpei2->hash(vm->internalVMFunctions);
	hash3 = cpei3->hash(vm->internalVMFunctions);
	hash4 = cpei4->hash(vm->internalVMFunctions);

	if (hash1==0 || (hash1 == hash2) || (hash1 == hash3) || (hash1==hash4)) {
		return 25;
	}
	if (hash2==0 || (hash2 == hash3) || (hash2 == hash4)) {
		return 26;
	}
	if (hash3==0 || (hash3 == hash4)) {
		return 27;
	}
	if (hash4==0) {
		return 28;
	}

	/* check that serial versions return same values as non-serial versions */

	compare1a = (char*)serial1->getPath(&compareLen1a);	
	compare2a = (char*)serial2->getPath(&compareLen2a);	
	compare3a = (char*)serial3->getPath(&compareLen3a);	
	compare4a = (char*)serial4->getPath(&compareLen4a);

	if ((compareLen1a != compareLen1) || strncmp(compare1a, compare1, compareLen1a)) {
		return 29;
	}
	if ((compareLen2a != compareLen2) || strncmp(compare2a, compare2, compareLen2a)) {
		return 30;
	}
	if ((compareLen3a != compareLen3) || strncmp(compare3a, compare3, compareLen3a)) {
		return 31;
	}
	if ((compareLen4a != compareLen4) || strncmp(compare4a, compare4, compareLen4a)) {
		return 32;
	}

	if (hash1 != serial1->hash(vm->internalVMFunctions)) return 33;
	if (hash2 != serial2->hash(vm->internalVMFunctions)) return 34;
	if (hash3 != serial3->hash(vm->internalVMFunctions)) return 35;
	if (hash4 != serial4->hash(vm->internalVMFunctions)) return 36;

	if (!ClasspathItem::compare(vm->internalVMFunctions, cpei1, serial1)) return 37;
	if (!ClasspathItem::compare(vm->internalVMFunctions, cpei2, serial2)) return 38;
	if (!ClasspathItem::compare(vm->internalVMFunctions, cpei3, serial3)) return 39;
	if (!ClasspathItem::compare(vm->internalVMFunctions, cpei4, serial4)) return 40;

	if (ClasspathItem::compare(vm->internalVMFunctions, cpei1, serial2)) return 41;
	if (ClasspathItem::compare(vm->internalVMFunctions, cpei2, serial3)) return 42;
	if (ClasspathItem::compare(vm->internalVMFunctions, cpei3, serial4)) return 43;
	if (ClasspathItem::compare(vm->internalVMFunctions, cpei4, serial1)) return 44;
	
	if (ClasspathItem::compare(vm->internalVMFunctions, cpei1, cpei4)) return 45;
	if (ClasspathItem::compare(vm->internalVMFunctions, cpei2, cpei3)) return 46;
	if (ClasspathItem::compare(vm->internalVMFunctions, cpei3, cpei2)) return 47;
	if (ClasspathItem::compare(vm->internalVMFunctions, cpei4, cpei1)) return 48;

	if (!ClasspathItem::compare(vm->internalVMFunctions, serial1, serial1)) return 49;
	if (!ClasspathItem::compare(vm->internalVMFunctions, serial2, serial2)) return 50;
	if (!ClasspathItem::compare(vm->internalVMFunctions, serial3, serial3)) return 51;
	if (!ClasspathItem::compare(vm->internalVMFunctions, serial4, serial4)) return 52;

	if (!ClasspathItem::compare(vm->internalVMFunctions, cpei1, cpei1)) return 53;
	if (!ClasspathItem::compare(vm->internalVMFunctions, cpei2, cpei2)) return 54;
	if (!ClasspathItem::compare(vm->internalVMFunctions, cpei3, cpei3)) return 55;
	if (!ClasspathItem::compare(vm->internalVMFunctions, cpei4, cpei4)) return 56;
	
	j9mem_free_memory(serial1);
	j9mem_free_memory(serial2);
	j9mem_free_memory(serial3);
	j9mem_free_memory(serial4);
	j9mem_free_memory(cpei1);
	j9mem_free_memory(cpei2);
	j9mem_free_memory(cpei3);
	j9mem_free_memory(cpei4);

	/* Hash values with same path but different protocols should be different */

	if (!(cpei1 = createClasspathEntryItem(vm, PATH2, (U_16)strlen(PATH2), PROTO_JAR))) {
		return 57;
	}
	if (!(cpei2 = createClasspathEntryItem(vm, PATH2, (U_16)strlen(PATH2), PROTO_DIR))) {
		return 58;
	}
	if (!(cpei3 = createClasspathEntryItem(vm, PATH2, (U_16)strlen(PATH2), PROTO_TOKEN))) {
		return 59;
	}

	hash1 = cpei1->hash(vm->internalVMFunctions);
	hash2 = cpei2->hash(vm->internalVMFunctions);
	hash3 = cpei3->hash(vm->internalVMFunctions);

	if ((hash1 == hash2) || (hash1 == hash3) || (hash2 == hash3)) {
		return 60;
	}

	if (ClasspathItem::compare(vm->internalVMFunctions, cpei1, cpei2)) return 61;
	if (ClasspathItem::compare(vm->internalVMFunctions, cpei2, cpei3)) return 62;
	if (ClasspathItem::compare(vm->internalVMFunctions, cpei3, cpei1)) return 63;

	j9mem_free_memory(cpei1);
	j9mem_free_memory(cpei2);
	j9mem_free_memory(cpei3);
	return PASS;
}

ClasspathItem*
ClasspathItemTest::createClasspathItem(J9JavaVM* vm, IDATA entries, IDATA entryLen, IDATA fromHelperID, U_16 cpType, UDATA protocols, char** stringCursor)
{
	UDATA sizeNeeded;
	ClasspathItem *cpPtr, *returnVal;

	PORT_ACCESS_FROM_JAVAVM(vm);

	sizeNeeded = ClasspathItem::getRequiredConstrBytes(entries);
	cpPtr = (ClasspathItem*)j9mem_allocate_memory(sizeNeeded, J9MEM_CATEGORY_CLASSES);

	if (!cpPtr) {
		return NULL;
	}
	memset(cpPtr, 0, sizeNeeded);
	returnVal = ClasspathItem::newInstance(vm, entries, fromHelperID, cpType, cpPtr);

	/* Try to add entries+1 - last add should fail */
	for (IDATA i=0; i<entries+1; i++) {
		IDATA result = 0;
		char* cursor = *stringCursor;
		char* name = cursor;
		UDATA cntr = entryLen;
		UDATA current = 0;

		/* Create unique name */
		while (cntr>0) {
			current = ((cntr+i) % 26);
			*(cursor++) = (char)('a' + current);
			--cntr;
		}
		*cursor = '\0';
		*stringCursor = ++cursor;

		if (i >= entries) {
			/* Negative test. Disable Trc_SHR_Assert_ShouldNeverHappen() in ClasspathItem::addItem() */
			j9shr_UtActive[1009] = 0;
		}
		result = returnVal->addItem(vm->internalVMFunctions, (const char*)name, (U_16)strlen(name), protocols);
		if (i<entries) {
			if (result != i+1) {
				j9tty_printf(PORTLIB, "addItem returned %d expected %d\n", result, i+1);
				return NULL;
			}
		} else {
			/* Negative test done. Enable Trc_SHR_Assert_ShouldNeverHappen() back */
			j9shr_UtActive[1009] = 1;
			if (result != -1) {
				j9tty_printf(PORTLIB, "addItem returned %d expected -1\n", result);
				return NULL;
			}
		}
	}

	return returnVal;
}

IDATA
ClasspathItemTest::createTest(J9JavaVM* vm, ClasspathItem** cpArray, char* stringBuf, IDATA entryLen)
{
	UDATA cntr = 0;

	cpArray[cntr] = createClasspathItem(vm, 0, entryLen, cntr, CP_TYPE_CLASSPATH, PROTO_DIR, &stringBuf);
	if (NULL == cpArray[cntr]) {
		return cntr + 1;
	}
	cntr++;
	cpArray[cntr] = createClasspathItem(vm, 1, entryLen, cntr, CP_TYPE_CLASSPATH, PROTO_JAR, &stringBuf);
	if (NULL == cpArray[cntr]) {
		return cntr + 1;
	}
	cntr++;
	cpArray[cntr] = createClasspathItem(vm, 10, entryLen, cntr, CP_TYPE_CLASSPATH, PROTO_DIR, &stringBuf);
	if (NULL == cpArray[cntr]) {
		return cntr + 1;
	}
	cntr++;
	cpArray[cntr] = createClasspathItem(vm, 1000, entryLen, cntr, CP_TYPE_CLASSPATH, PROTO_JAR, &stringBuf);
	if (NULL == cpArray[cntr]) {
		return cntr + 1;
	}
	cntr++;
	cpArray[cntr] = createClasspathItem(vm, 1, entryLen, cntr, CP_TYPE_URL, PROTO_JAR, &stringBuf);
	if (NULL == cpArray[cntr]) {
		return cntr + 1;
	}
	cntr++;
	cpArray[cntr] = createClasspathItem(vm, 1, entryLen, cntr, CP_TYPE_TOKEN, PROTO_TOKEN, &stringBuf);
	if (NULL == cpArray[cntr]) {
		return cntr + 1;
	}
	cntr++;

	return PASS;
}

IDATA
ClasspathItemTest::compareGetters(J9JavaVM* vm, ClasspathItem* local, ClasspathItem* serial, UDATA cntr)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (local->getItemsAdded() != serial->getItemsAdded()) {
		j9tty_printf(PORTLIB, "getItemsAdded() results are different for test %d\n", cntr);
		return 1;
	}
	if (serial->getHelperID() != HELPERID_NOT_APPLICABLE) {
		j9tty_printf(PORTLIB, "getHelperID() for serial should be HELPERID_NOT_APPLICABLE for test %d\n", cntr);
		return 2;
	}
	if (local->getFirstDirIndex() != serial->getFirstDirIndex()) {
		j9tty_printf(PORTLIB, "getFirstDirIndex() results are different for test %d\n", cntr);
		return 3;
	}
	if (local->getHashCode() != serial->getHashCode()) {
		j9tty_printf(PORTLIB, "getFirstDirIndex() results are different for test %d\n", cntr);
		return 4;
	}
	if (local->getType() != serial->getType()) {
		j9tty_printf(PORTLIB, "getType() results are different for test %d\n", cntr);
		return 5;
	}
	if (local->isInCache()) {
		j9tty_printf(PORTLIB, "isInCache() should not have returned true for test %d\n", cntr);
		return 10;
	}
	if (!(serial->isInCache())) {
		j9tty_printf(PORTLIB, "isInCache() should have returned true for test %d\n", cntr);
		return 11;
	}

	return PASS;
}

IDATA
ClasspathItemTest::serializeTest(J9JavaVM* vm, ClasspathItem** cpArray, ClasspathItem** serialArray)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	j9tty_printf(PORTLIB, "Running tests: ");
	for (UDATA i=0; i<CPLENGTH; i++) {
		UDATA size = cpArray[i]->getSizeNeeded();
		char* tempBlock;

		j9tty_printf(PORTLIB, ".");
		if (!(tempBlock = (char*)j9mem_allocate_memory(size, J9MEM_CATEGORY_CLASSES))) {
			j9tty_printf(PORTLIB, "failed to allocate memory for item %d\n", i);
			return i+1;
		}
		memset(tempBlock, 0, size);
		if (cpArray[i]->writeToAddress(tempBlock)) {
			j9tty_printf(PORTLIB, "writeToAddress() failed for item %d\n", i);
			return CPLENGTH+i+1;
		}
		serialArray[i] = (ClasspathItem*)tempBlock;

		if (compareGetters(vm, cpArray[i], serialArray[i], i)) {
			/* message already reported */
			return (CPLENGTH*2)+i+1;
		}
	}
	j9tty_printf(PORTLIB, "\n");

	return PASS;
}

IDATA
ClasspathItemTest::compareTest(J9JavaVM* vm, ClasspathItem** cpArray, ClasspathItem** serialArray)
{
	UDATA i, j;
	PORT_ACCESS_FROM_JAVAVM(vm);

	j9tty_printf(PORTLIB, "Running tests: ");
	for (i = 0; i<CPLENGTH; i++) {
		for (j = 0; j<CPLENGTH; j++) {
			bool result1 = ClasspathItem::compare(vm->internalVMFunctions, cpArray[i], cpArray[j]);
			bool result2 = ClasspathItem::compare(vm->internalVMFunctions, cpArray[i], serialArray[j]);
			bool result3 = ClasspathItem::compare(vm->internalVMFunctions, serialArray[i], serialArray[j]);

			j9tty_printf(PORTLIB, ".");
			/* Comparing 1 and 4 (and the partition/context versions) should pass because only difference is URL/Classpath type, 
				which is not tested by compare, quite intentionally */
			if (i == j || ((i % 6)==1 && (j % 6==4) && (i/6 == j/6)) || ((i % 6)==4 && (j % 6==1) && (i/6 == j/6))) {
				if (!result1) {
					j9tty_printf(PORTLIB, "Compare failed unexpectedly when comparing local %d with local %d\n", i, j);
					return 1;
				}
				if (!result2) {
					j9tty_printf(PORTLIB, "Compare failed unexpectedly when comparing local %d with serial %d\n", i, j);
					return 2;
				}
				if (!result3) {
					j9tty_printf(PORTLIB, "Compare failed unexpectedly when comparing serial %d with serial %d\n", i, j);
					return 3;
				}
			} else {
				if (result1) {
					j9tty_printf(PORTLIB, "Compare should have failed when comparing local %d with local %d\n", i, j);
					return 4;
				}
				if (result2) {
					j9tty_printf(PORTLIB, "Compare should have failed when comparing local %d with serial %d\n", i, j);
					return 5;
				}
				if (result3) {
					j9tty_printf(PORTLIB, "Compare should have failed when comparing serial %d with serial %d\n", i, j);
					return 6;
				}
			}
		}
	}
	j9tty_printf(PORTLIB, "\n");

	return PASS;
}

IDATA
ClasspathItemTest::itemsTest(J9JavaVM* vm, ClasspathItem** cpArray, ClasspathItem** serialArray)
{
	I_16 i, j;
	PORT_ACCESS_FROM_JAVAVM(vm);

	j9tty_printf(PORTLIB, "Running tests: ");
	for (i = 0; i<CPLENGTH; i++) {
		IDATA result = cpArray[i]->getItemsAdded();
		IDATA result2 = serialArray[i]->getItemsAdded();
		UDATA type = (i % 6);
		I_16 expectedResult;

		switch (type) {
			case 0:
				expectedResult = 0;
				break;
			case 1:
			case 4:
			case 5:
				expectedResult = 1;
				break;
			case 2:
				expectedResult = 10;
				break;
			case 3:
				continue;		/* Can't check the 1000 length array as entries are not unique */
				break;
			default:
				expectedResult = -1;
		}

		if (result != expectedResult) {
			j9tty_printf(PORTLIB, "getItemsAdded() failed for local classpath #%d. Expected %d, got %d.\n", i, expectedResult, result);
			return i+1;
		}
		if (result2 != expectedResult) {
			j9tty_printf(PORTLIB, "getItemsAdded() failed for serial classpath #%d. Expected %d, got %d.\n", i, expectedResult, result2);
			return CPLENGTH+i+1;
		}

		/* Run through each element in each classpath and check itemAt and find */
		for (j=0; j<expectedResult+1; j++) {

			if (j >= expectedResult) {
				/* Negative test. Disable Trc_SHR_Assert_ShouldNeverHappen() in ClasspathItem::itemAt() */
				j9shr_UtActive[1009] = 0;
			}

			ClasspathEntryItem* item1 = cpArray[i]->itemAt(j);
			ClasspathEntryItem* item2 = serialArray[i]->itemAt(j);
			IDATA find1, find2, find3, find4, find5, find6, find7;

			j9tty_printf(PORTLIB, ".");

			/* Positive tests... */
			if (j<expectedResult) {
				if (!ClasspathItem::compare(vm->internalVMFunctions, item1, item2)) {
					j9tty_printf(PORTLIB, "Comparing results of itemAt() failed for classpath #%d and item %d\n", i, j);
					return 1111;
				}

				find1 = cpArray[i]->find(vm->internalVMFunctions, item1);
				find2 = cpArray[i]->find(vm->internalVMFunctions, item2);
				find3 = serialArray[i]->find(vm->internalVMFunctions, item1);
				find4 = serialArray[i]->find(vm->internalVMFunctions, item2);
				find5 = serialArray[i]->find(vm->internalVMFunctions, item1, j);		/* Should succeed */
				find6 = serialArray[i]->find(vm->internalVMFunctions, item1, j-1);		/* Should fail */
				find7 = serialArray[i]->find(vm->internalVMFunctions, item1, 30000);	/* Should succeed */

				if (find1 != j) {
					j9tty_printf(PORTLIB, "find() failed for local classpath #%d and item %d\n", i, j);
					return 3333;
				}
				if (find2 != j) {
					j9tty_printf(PORTLIB, "find() failed for local classpath #%d and item %d\n", i, j);
					return 4444;
				}
				if (find3 != j) {
					j9tty_printf(PORTLIB, "find() failed for serial classpath #%d and item %d\n", i, j);
					return 4444;
				}
				if (find4 != j) {
					j9tty_printf(PORTLIB, "find() failed for serial classpath #%d and item %d\n", i, j);
					return 5555;
				}
				if (find5 != j) {
					j9tty_printf(PORTLIB, "find() with stopAtIndex failed for local classpath #%d and item %d\n", i, j);
					return 6666;
				}
				/* if j==0 then j-1 will cause find to ignore the stopAtIndex */
				if (j>0 && find6 != -1) {
					j9tty_printf(PORTLIB, "find() with stopAtIndex should have returned -1 for local classpath #%d and item %d\n", i, j);
					return 7777;
				}
				if (find7 != j) {
					j9tty_printf(PORTLIB, "find() with stopAtIndex>itemsAdded failed for local classpath #%d and item %d\n", i, j);
					return 8888;
				}

			/* Negative tests... */
			} else {
				/* Negative test done. Enable Trc_SHR_Assert_ShouldNeverHappen() back */
				j9shr_UtActive[1009] = 1;
				if (item1 || item2) {
					j9tty_printf(PORTLIB, "itemAt() should have returned NULL for classpath #%d and item %d\n", i, j);
					return 2222;
				}
			}
		}
	}

	j9tty_printf(PORTLIB, "\n");

	return PASS;
}

/*
	ClasspathItem Combinations:	
	(classpath 0 entry | classpath 1 entry | classpath 10 entries | classpath 1000 entries | URL 1 entry | Token 1 entry)
	= 6 ClasspathItems
*/
IDATA 
testClasspathItem(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_PORT(vm->portLibrary);

	UDATA success = PASS;
	UDATA rc = 0;
	ClasspathItem* cpArray[CPLENGTH];
	ClasspathItem** cpArrayPtr = (ClasspathItem**)cpArray;
	ClasspathItem* serialArray[CPLENGTH];
	ClasspathItem** serialArrayPtr = (ClasspathItem**)serialArray;
	UDATA i, j;
	char* stringBuf;
	UDATA stringBufSize = 256 * 1024;

	REPORT_START("ClasspathItem");

	for (j=1; j<30; j++) {

	for (i=0; i<CPLENGTH; i++) {
		cpArrayPtr[i] = serialArrayPtr[i] = 0;
	}
	if (!(stringBuf = (char*)j9mem_allocate_memory(stringBufSize, J9MEM_CATEGORY_CLASSES))) {
		return 9999;
	}
	memset(stringBuf, 0, stringBufSize);

	if (j==0) {
		SHC_TEST_ASSERT("classpathEntryTest", ClasspathItemTest::classpathEntryTest(vm), success, rc);
	}
	SHC_TEST_ASSERT("createTest", ClasspathItemTest::createTest(vm, cpArrayPtr, stringBuf, j), success, rc);
	SHC_TEST_ASSERT("serializeTest", ClasspathItemTest::serializeTest(vm, cpArrayPtr, serialArrayPtr), success, rc);
	SHC_TEST_ASSERT("compareTest", ClasspathItemTest::compareTest(vm, cpArrayPtr, serialArrayPtr), success, rc);
	SHC_TEST_ASSERT("itemsTest", ClasspathItemTest::itemsTest(vm, cpArrayPtr, serialArrayPtr), success, rc);

	for (i=0; i<CPLENGTH; i++) {
		if (cpArrayPtr[i]) j9mem_free_memory(cpArrayPtr[i]);
		if (serialArrayPtr[i]) j9mem_free_memory(serialArrayPtr[i]);
	}
	j9mem_free_memory(stringBuf);
	
	}

	REPORT_SUMMARY("ClasspathItem", success);

	return success;
}

