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

#include "classpathcache.h"
#include "j9port.h"
#include "main.h"
#include "j9.h"
#include <string.h>

#define ITEMS_OFFSET 10
#define MAIN_ARRAY_SIZE 10

#define CP_OFFSET1 0xFF
#define CP_OFFSET2 0xFFF
#define CP_OFFSET3 0xFFFF
#define CP_OFFSET4 0xFFFFF
#define CP_OFFSET5 0xFFFFFF

#define PARTITION1 "partition1"
#define PARTITION2 "partition2"
#define PARTITION3 "partition3"
#define PARTITION4 "partition4"

extern "C" 
{ 
	IDATA testClasspathCache(J9JavaVM* vm); 
}

class ClasspathCacheTest {
	public:
		static IDATA initializeTest(J9PortLibrary* portLibrary, J9ClasspathByIDArray** cp1, J9ClasspathByIDArray** cp2, J9ClasspathByIDArray** cp3);
		static IDATA setTest(J9JavaVM* vm, J9ClasspathByIDArray** cp1, J9ClasspathByIDArray** cp2, J9ClasspathByIDArray** cp3);
		static IDATA getTest(J9JavaVM* vm, J9ClasspathByIDArray** cp1, J9ClasspathByIDArray** cp2, J9ClasspathByIDArray** cp3);
		static IDATA getIDTest(J9PortLibrary* portLibrary, J9ClasspathByIDArray** cp1, J9ClasspathByIDArray** cp2, J9ClasspathByIDArray** cp3);
		static IDATA clearTest(J9JavaVM* vm, J9ClasspathByIDArray** cp1, J9ClasspathByIDArray** cp2, J9ClasspathByIDArray** cp3);
		static IDATA freeTest(J9PortLibrary* portLibrary, J9ClasspathByIDArray** cp1, J9ClasspathByIDArray** cp2, J9ClasspathByIDArray** cp3);
		static IDATA matchFailedTest(J9JavaVM* vm);
		static void setClasspathByIDInt(J9ClasspathByID* cp, UDATA i, UDATA elements);
		static UDATA isClasspathByIDSetToInt(J9ClasspathByID* cp, UDATA i, UDATA elements);
		static UDATA isClasspathArrayOK(J9ClasspathByIDArray* array);
};

void
ClasspathCacheTest::setClasspathByIDInt(J9ClasspathByID* cp, UDATA i, UDATA arraySize)
{
	J9GenericByID* generic = (J9GenericByID*)cp;
	UDATA cntr;

	generic->magic = (U_8)i;
	generic->type = (U_8)i;
	generic->id = (U_16)i;
	generic->jclData = (struct J9ClassPathEntry*)i;
	generic->cpData = (void*)i;
	cp->entryCount = i;
	for (cntr=0; cntr<arraySize; cntr++) {
		cp->failedMatches[cntr] = (U_8)(cntr + i);
	}
}

UDATA
ClasspathCacheTest::isClasspathByIDSetToInt(J9ClasspathByID* cp, UDATA i, UDATA arraySize)
{
	J9GenericByID* generic = (J9GenericByID*)cp;

	if (
		(generic->magic == (U_8)i) &&
		(generic->type == (U_8)i) &&
		(generic->id == (U_16)i) &&
		(generic->jclData == (struct J9ClassPathEntry*)i) &&
		(generic->cpData == (void*)i) &&
		(cp->entryCount == i)
		) 
	{
		UDATA cntr;

		for (cntr=0; cntr<arraySize; cntr++) {
			if (cp->failedMatches[cntr] != (cntr + i)) {
				return 0;
			}
		}
		return 1;
	} else {
		return 0;
	}
}

UDATA 
ClasspathCacheTest::isClasspathArrayOK(J9ClasspathByIDArray* array)
{
	UDATA i = 0;

	for (i=0; i<array->size; i++) {
		ClasspathCacheTest::setClasspathByIDInt(array->array[i], (i + 1), array->size);
	}
	for (i=0; i<array->size; i++) {
		if (!(ClasspathCacheTest::isClasspathByIDSetToInt(array->array[i], (i + 1), array->size))) {
			return 0;
		}
	}
	return 1;
}

IDATA 
ClasspathCacheTest::initializeTest(J9PortLibrary* portLibrary, J9ClasspathByIDArray** cp1, J9ClasspathByIDArray** cp2, J9ClasspathByIDArray** cp3)
{
	J9ClasspathByIDArray* current;

	PORT_ACCESS_FROM_PORT(portLibrary);

	if (initializeIdentifiedClasspathArray(portLibrary, 0, NULL, 0, 0)) {
		return 1;
	}
	if (!(current = initializeIdentifiedClasspathArray(portLibrary, 1, NULL, 0, 0))) {
		return 2;
	}
	if (!ClasspathCacheTest::isClasspathArrayOK(current)) {
		return 3;
	}
	j9mem_free_memory(current);
	if (!(current = initializeIdentifiedClasspathArray(portLibrary, 100, NULL, 0, 0))) {
		return 4;
	}
	if (!ClasspathCacheTest::isClasspathArrayOK(current)) {
		return 5;
	}
	j9mem_free_memory(current);
	if (!(current = initializeIdentifiedClasspathArray(portLibrary, 10, PARTITION1, strlen(PARTITION1), 99))) {
		return 6;
	}
	if (!ClasspathCacheTest::isClasspathArrayOK(current)) {
		return 7;
	}
	j9mem_free_memory(current);

	*cp1 = initializeIdentifiedClasspathArray(portLibrary, MAIN_ARRAY_SIZE, NULL, 0, 0);
	*cp2 = initializeIdentifiedClasspathArray(portLibrary, MAIN_ARRAY_SIZE, NULL, 0, 0);
	*cp3 = initializeIdentifiedClasspathArray(portLibrary, MAIN_ARRAY_SIZE, NULL, 0, 0);

	return PASS;
}

IDATA 
ClasspathCacheTest::matchFailedTest(J9JavaVM* vm)
{
	J9ClasspathByIDArray* current;
	UDATA i, j;
	UDATA prevSize;

	if (!(current = initializeIdentifiedClasspathArray(vm->portLibrary, 10, NULL, 0, 0))) {
		return 1;
	}
	for (i=0; i<current->size; i++) {
		(current->array[i])->header.cpData = (void*)1;
		for (j=0; j<(current->size+1); j++) {		/* The +1th element should not cause a crash */
			registerFailedMatch(vm->mainThread, current, j, i, (j+i), NULL, 0); 
		}
	}
	prevSize = current->size;
	setIdentifiedClasspath(vm->mainThread, &current, 100, 0, PARTITION1, strlen(PARTITION1), NULL);
	for (i=0; i<prevSize; i++) {
		for (j=0; j<prevSize; j++) {
			if (!hasMatchFailedBefore(vm->mainThread, current, j, i, (j+i), NULL, 0)) {
				return 2;
			}
		}
	}
	for (i=0; i<current->size; i++) {
		(current->array[i])->header.cpData = (void*)1;
		(current->next->array[i])->header.cpData = (void*)1;
		for (j=0; j<current->size; j++) {
			registerFailedMatch(vm->mainThread, current, j, i, (j+i), NULL, 0); 
			registerFailedMatch(vm->mainThread, current, j, i, (j+i), PARTITION1, strlen(PARTITION1)); 
		}
	}
	for (i=0; i<current->size; i++) {
		for (j=0; j<current->size; j++) {
			if (!hasMatchFailedBefore(vm->mainThread, current, j, i, (j+i), NULL, 0)) {
				return 3;
			}
			if (!hasMatchFailedBefore(vm->mainThread, current, j, i, (j+i), PARTITION1, strlen(PARTITION1))) {
				return 4;
			}
		}
	}
	freeIdentifiedClasspathArray(vm->portLibrary, current);
	return PASS;
}

IDATA 
ClasspathCacheTest::setTest(J9JavaVM* vm, J9ClasspathByIDArray** cp1, J9ClasspathByIDArray** cp2, J9ClasspathByIDArray** cp3)
{
	UDATA oldSize = 0;
	J9ClasspathByIDArray* current;
	J9ClasspathByIDArray* previous;
	UDATA i = 0;
	UDATA arraySize = 5;

	PORT_ACCESS_FROM_JAVAVM(vm);

	if (!(current = initializeIdentifiedClasspathArray(vm->portLibrary, arraySize, NULL, 0, 0))) {
		return 1;
	}
	for (i=0; i<arraySize; i++) {
		ClasspathCacheTest::setClasspathByIDInt(current->array[i], (i + 1), arraySize);
	}

	/* Test that array doesn't grow for helper ID arraySize-1 */
	previous = current;
	setIdentifiedClasspath(vm->mainThread, &current, arraySize-1, 0, NULL, 0, NULL);
	if ((current->size != arraySize) || (previous != current)) {
		return 2;
	}
	/* Check that data is not corrupted for elements 0 to arraySize-2 (we just set arraySize-1) */
	for (i=0; i<arraySize-1; i++) {
		if (!(ClasspathCacheTest::isClasspathByIDSetToInt(current->array[i], (i + 1), arraySize))) {
			return 3;
		}
	}

	/* Test that size grows to size arrayLength+ID if the array isn't big enough. Helper ID arraySize. */
	setIdentifiedClasspath(vm->mainThread, &current, arraySize, 0, NULL, 0, NULL);
	if (current->size != (arraySize+arraySize) || previous==current) {
		return 4;
	}
	/* Check that data has been copied for elements 0 to arraySize-2 */
	for (i=0; i<arraySize-1; i++) {
		if (!(ClasspathCacheTest::isClasspathByIDSetToInt(current->array[i], (i + 1), arraySize))) {
			return 5;
		}
	}
	j9mem_free_memory(current);

	arraySize = MAIN_ARRAY_SIZE;

	/* Populate cp1 with null partitions */
	for (i=0; i<arraySize+1; i++) {
		previous = *cp1;
		setIdentifiedClasspath(vm->mainThread, cp1, i, ITEMS_OFFSET+i, NULL, 0, (void*)(CP_OFFSET1+i));
		if (i<arraySize) {
			/* Check that array hasn't grown and that it hasn't been replaced */
			if ((*cp1)->size != arraySize || previous != *cp1) {
				return 6;
			}
		} else {
			/* Check that array HAS grown and that it HAS been replaced */
			if ((*cp1)->size == arraySize || previous == *cp1) {
				return 7;
			}
		}
	}

	/* Populate cp2 with single partitions */
	for (i=0; i<arraySize+1; i++) {
		previous = *cp2;
		setIdentifiedClasspath(vm->mainThread, cp2, i, ITEMS_OFFSET+i, PARTITION1, strlen(PARTITION1), (void*)(CP_OFFSET2+i));
		if (i<arraySize) {
			/* Check that array hasn't grown and that it hasn't been replaced */
			if ((*cp2)->size != arraySize || previous != *cp2) {
				return 8;
			}
		} else {
			/* Check that array HAS grown and that it HAS been replaced */
			if ((*cp2)->size == arraySize || previous == *cp2) {
				return 9;
			}
		}
	}
	
	/* Populate cp3 with multiple partitions */
	for (i=0; i<arraySize+1; i++) {
		previous = *cp3;
		setIdentifiedClasspath(vm->mainThread, cp3, i, ITEMS_OFFSET+i, PARTITION2, strlen(PARTITION2), (void*)(CP_OFFSET3+i));
		if (i<arraySize) {
			/* Check that array hasn't grown and that it hasn't been replaced */
			if ((*cp3)->size != arraySize || previous != *cp3) {
				return 10;
			}
		} else {
			/* Check that array HAS grown and that it HAS been replaced */
			if ((*cp3)->size == arraySize || previous == *cp3) {
				return 11;
			}
		}
		oldSize = (*cp3)->size;
		setIdentifiedClasspath(vm->mainThread, cp3, i, ITEMS_OFFSET+i, PARTITION3, strlen(PARTITION3), (void*)(CP_OFFSET4+i));
		/* Check that array hasn't grown even further */
		if ((*cp3)->size != oldSize) {
			return 12;
		}
		setIdentifiedClasspath(vm->mainThread, cp3, i, ITEMS_OFFSET+i, PARTITION4, strlen(PARTITION4), (void*)(CP_OFFSET5+i));
		/* Check that array hasn't grown even further */
		if ((*cp3)->size != oldSize) {
			return 13;
		}
	}

	return PASS;
}

IDATA 
ClasspathCacheTest::getTest(J9JavaVM* vm, J9ClasspathByIDArray** cp1, J9ClasspathByIDArray** cp2, J9ClasspathByIDArray** cp3)
{
	UDATA i, j;
	UDATA arraySize = MAIN_ARRAY_SIZE;
	void* cpToFree = NULL;
	void* result = NULL;
	J9ClasspathByIDArray* current = NULL;
	const char* partition = NULL;
	UDATA partitionLen = 0;
	UDATA offset = 0;

	/* Firstly, test that correct data is returned for populated arrays */

	for (j=0; j<3; j++) {
		current = NULL;

		if (j==0) current = *cp1;
		else if (j==1) current = *cp2;
		else if (j==2) current = *cp3;

		for (i=0; i<arraySize; i++) {
			result = getIdentifiedClasspath(vm->mainThread, current, i, ITEMS_OFFSET+i, NULL, 0, &cpToFree);
			if (j==0) {
				if (result != (void*)(CP_OFFSET1+i)) return 1;
			} else {
				if (result) return 2;
			}
			if (cpToFree) return 3;

			result = getIdentifiedClasspath(vm->mainThread, current, i, ITEMS_OFFSET+i, PARTITION1, strlen(PARTITION1), &cpToFree);
			if (j==1) {
				if (result != (void*)(CP_OFFSET2+i)) return 4;
			} else {
				if (result) return 5;
			}
			if (cpToFree) return 6;

			result = getIdentifiedClasspath(vm->mainThread, current, i, ITEMS_OFFSET+i, PARTITION2, strlen(PARTITION2), &cpToFree);
			if (j==2) {
				if (result != (void*)(CP_OFFSET3+i)) return 7;
			} else {
				if (result) return 8;
			}
			if (cpToFree) return 9;

			result = getIdentifiedClasspath(vm->mainThread, current, i, ITEMS_OFFSET+i, PARTITION3, strlen(PARTITION3), &cpToFree);
			if (j==2) {
				if (result != (void*)(CP_OFFSET4+i)) return 10;
			} else {
				if (result) return 11;
			}
			if (cpToFree) return 12;

			result = getIdentifiedClasspath(vm->mainThread, current, i, ITEMS_OFFSET+i, PARTITION4, strlen(PARTITION4), &cpToFree);
			if (j==2) {
				if (result != (void*)(CP_OFFSET5+i)) return 13;
			} else {
				if (result) return 14;
			}
			if (cpToFree) return 15;
		}
	}

	/* Secondly, test that items are reset if the helper ID or items added change */

	current = initializeIdentifiedClasspathArray(vm->portLibrary, arraySize, NULL, 0, 0);
	for (i=0; i<arraySize; i++) {
		setIdentifiedClasspath(vm->mainThread, &current, i, ITEMS_OFFSET+i, NULL, 0, (void*)(CP_OFFSET1+i));
		setIdentifiedClasspath(vm->mainThread, &current, i, ITEMS_OFFSET+i, PARTITION1, strlen(PARTITION1), (void*)(CP_OFFSET2+i));
		setIdentifiedClasspath(vm->mainThread, &current, i, ITEMS_OFFSET+i, PARTITION2, strlen(PARTITION2), (void*)(CP_OFFSET3+i));
		setIdentifiedClasspath(vm->mainThread, &current, i, ITEMS_OFFSET+i, PARTITION3, strlen(PARTITION3), (void*)(CP_OFFSET4+i));
		setIdentifiedClasspath(vm->mainThread, &current, i, ITEMS_OFFSET+i, PARTITION4, strlen(PARTITION4), (void*)(CP_OFFSET5+i));
	}

	offset = CP_OFFSET1;

	/* Go through each partition */
	for (j=0; j<5; j++) {

			/* Go through each array element */
			for (i=0; i<arraySize; i++) {

			/* Check that the correct item is returned in normal case */
			result = getIdentifiedClasspath(vm->mainThread, current, i, ITEMS_OFFSET+i, partition, partitionLen, &cpToFree);
			if (result != (void*)(offset+i)) return 16;
			if (cpToFree) return 17;

			/* Check that the correct item is returned for freeing */
			result = getIdentifiedClasspath(vm->mainThread, current, i, (ITEMS_OFFSET + i + 1), partition, partitionLen, &cpToFree);
			if (cpToFree != (void*)(offset+i)) return 18;
			if (result) return 19;

			/* Check that the element cannot now be accessed */
			result = getIdentifiedClasspath(vm->mainThread, current, i, ITEMS_OFFSET+i, partition, partitionLen, &cpToFree);
			if (result) return 20;
			if (cpToFree) return 21;

			/* Reset and check that reset succeeded */
			setIdentifiedClasspath(vm->mainThread, &current, i, (ITEMS_OFFSET + i + 1), partition, partitionLen, (void*)(offset+i));
			result = getIdentifiedClasspath(vm->mainThread, current, i, (ITEMS_OFFSET + i + 1), partition, partitionLen, &cpToFree);
			if (result != (void*)(offset+i)) return 22;
			if (cpToFree) return 23;
		}
		if (j==0) {
			partition = PARTITION1;
			offset = CP_OFFSET2;
		}
		else if (j==1) {
			partition = PARTITION2;
			offset = CP_OFFSET3;
		}
		else if (j==2) {
			partition = PARTITION3;
			offset = CP_OFFSET4;
		}
		else if (j==3) {
			partition = PARTITION4;
			offset = CP_OFFSET5;
		}
		partitionLen = strlen(partition);
	}
	freeIdentifiedClasspathArray(vm->portLibrary, current);

	return PASS;
}

IDATA 
ClasspathCacheTest::getIDTest(J9PortLibrary* portLibrary, J9ClasspathByIDArray** cp1, J9ClasspathByIDArray** cp2, J9ClasspathByIDArray** cp3)
{
	UDATA i, j;
	UDATA arraySize = MAIN_ARRAY_SIZE;
	J9ClasspathByIDArray* current = NULL;
	IDATA result = 0;

	for (j=0; j<3; j++) {
		current = NULL;

		if (j==0) current = *cp1;
		else if (j==1) current = *cp2;
		else if (j==2) current = *cp3;

		for (i=0; i<arraySize; i++) {

			result = getIDForIdentified(portLibrary, current, (void*)(CP_OFFSET1+i), 0);
			if (j==0) {
				if (((UDATA)result) != i) return 1;
			} else {
				if (result!=ID_NOT_FOUND) return 2;
			}

			result = getIDForIdentified(portLibrary, current, (void*)(CP_OFFSET2+i), 0);
			if (j==1) {
				if (((UDATA)result) != i) return 3;
			} else {
				if (result!=ID_NOT_FOUND) return 4;
			}

			result = getIDForIdentified(portLibrary, current, (void*)(CP_OFFSET3+i), 0);
			if (j==2) {
				if (((UDATA)result) != i) return 5;
			} else {
				if (result!=ID_NOT_FOUND) return 6;
			}

			result = getIDForIdentified(portLibrary, current, (void*)(CP_OFFSET4+i), 0);
			if (j==2) {
				if (((UDATA)result) != i) return 7;
			} else {
				if (result!=ID_NOT_FOUND) return 8;
			}

			result = getIDForIdentified(portLibrary, current, (void*)(CP_OFFSET5+i), 0);
			if (j==2) {
				if (((UDATA)result) !=i ) return 9;
			} else {
				if (result!=ID_NOT_FOUND) return 10;
			}
		}
	
	}
	return PASS;
}

IDATA 
ClasspathCacheTest::clearTest(J9JavaVM* vm, J9ClasspathByIDArray** cp1, J9ClasspathByIDArray** cp2, J9ClasspathByIDArray** cp3)
{
	UDATA i, j;
	const char *partition1 = NULL, *partition2 = NULL, *partition3 = NULL, *temp = NULL;
	UDATA plen1 = 0, plen2 = 0, plen3 = 0, tempLen = 0;
	UDATA offset1 = 0, offset2 = 0, offset3 = 0, tempOffset = 0;
	UDATA arraySize = MAIN_ARRAY_SIZE;
	J9ClasspathByIDArray* current = NULL;
	IDATA iresult = 0;
	void* vresult = NULL;
	void* cpToFree = NULL;

	for (j=0; j<3; j++) {
		current = NULL;

		if (j==0) {
			current = *cp1;
			partition1 = NULL;
			plen1 = 0;
			offset1 = CP_OFFSET1;
		}
		else if (j==1) {
			current = *cp2;
			partition1 = PARTITION1;
			plen1 = strlen(partition1);
			offset1 = CP_OFFSET2;
		}
		else if (j==2) {
			current = *cp3;
			partition1 = PARTITION2;
			partition2 = PARTITION3;
			partition3 = PARTITION4;
			offset1 = CP_OFFSET3;
			offset2 = CP_OFFSET4;
			offset3 = CP_OFFSET5;
			plen1 = strlen(partition1);
			plen2 = strlen(partition2);
			plen3 = strlen(partition3);
		}

		/* Clear elements of array without partitions */
		for (i=0; i<arraySize; i++) {
			clearIdentifiedClasspath(vm->portLibrary, current, (void*)(offset1+i));

			/* Check that identified array is now clear */
			iresult = getIDForIdentified(vm->portLibrary, current, (void*)(offset1+i), 0);
			if (iresult!=ID_NOT_FOUND) return 1;

			/* Check that identified array is now clear (2) */
			vresult = getIdentifiedClasspath(vm->mainThread, current, i, ITEMS_OFFSET+i, partition1, plen1, &cpToFree);
			if (vresult) return 2;
			if (cpToFree) return 3;

			/* Ensure that all partitions have gone for multi-partition */
			if (j==2) {
				vresult = getIdentifiedClasspath(vm->mainThread, current, i, ITEMS_OFFSET+i, partition2, plen2, &cpToFree);
				if (vresult) return 4;
				if (cpToFree) return 5;

				vresult = getIdentifiedClasspath(vm->mainThread, current, i, ITEMS_OFFSET+i, partition3, plen3, &cpToFree);
				if (vresult) return 6;
				if (cpToFree) return 7;
			}

			/* Check that next element in the array has not been affected*/
			if (i<(arraySize-1)) {
				vresult = getIdentifiedClasspath(vm->mainThread, current, i+1, ITEMS_OFFSET+i+1, partition1, plen1, &cpToFree);
				if (vresult!=(void*)(offset1+i+1)) return 8;
				if (cpToFree) return 9;
			}

			/* cycle the partitions (for multi-partition) so that not always the same one is cleared */
			if (j==2) {
				temp = partition1;
				partition1 = partition2;
				partition2 = partition3;
				partition3 = temp;
				tempLen = plen1;
				plen1 = plen2;
				plen2 = plen3;
				plen3 = tempLen;
				tempOffset = offset1;
				offset1 = offset2;
				offset2 = offset3;
				offset3 = tempOffset;
			}
		}
	}	

	return PASS;
}

IDATA 
ClasspathCacheTest::freeTest(J9PortLibrary* portLibrary, J9ClasspathByIDArray** cp1, J9ClasspathByIDArray** cp2, J9ClasspathByIDArray** cp3)
{
	freeIdentifiedClasspathArray(portLibrary, *cp1);
	freeIdentifiedClasspathArray(portLibrary, *cp2);
	freeIdentifiedClasspathArray(portLibrary, *cp3);

	/* very little we can actually test here... except that it doesn't crash */
	return PASS;
}

IDATA 
testClasspathCache(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_PORT(vm->portLibrary);
	UDATA success = PASS;
	UDATA rc = 0;
	J9ClasspathByIDArray *cp1, *cp2, *cp3;


	REPORT_START("ClasspathCache");

	SHC_TEST_ASSERT("initializeIdentifiedClasspathArray", ClasspathCacheTest::initializeTest(PORTLIB, &cp1, &cp2, &cp3), success, rc);
	SHC_TEST_ASSERT("setIdentifiedClasspath", ClasspathCacheTest::setTest(vm, &cp1, &cp2, &cp3), success, rc);
	SHC_TEST_ASSERT("getIdentifiedClasspath", ClasspathCacheTest::getTest(vm, &cp1, &cp2, &cp3), success, rc);
	SHC_TEST_ASSERT("getIDForIdentified", ClasspathCacheTest::getIDTest(PORTLIB, &cp1, &cp2, &cp3), success, rc);
	SHC_TEST_ASSERT("clearIdentifiedClasspath", ClasspathCacheTest::clearTest(vm, &cp1, &cp2, &cp3), success, rc);
	SHC_TEST_ASSERT("freeIdentifiedClasspathArray", ClasspathCacheTest::freeTest(PORTLIB, &cp1, &cp2, &cp3), success, rc);
	SHC_TEST_ASSERT("registerMatchFailed", ClasspathCacheTest::matchFailedTest(vm), success, rc);

	REPORT_SUMMARY("ClaspathCache", success);

	return success;
}

