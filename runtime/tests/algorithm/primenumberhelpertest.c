/*******************************************************************************
 * Copyright (c) 20010, 20010 IBM Corp. and others
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

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "util_api.h"

/**
 * This is simplest algorithm to find whether a number is prime or not.
 * It tries to divide the given number with every number starting from 2 to square root of that number.
 * We stop at the square root because if it can not be divided by square root or any number before,
 * then it is definitely a prime number.
 *
 *@param	number	A number to be checked whether it is prime or not. 
 *@return	TRUE, if the number is prime, FALSE otherwise. 
 */
static BOOLEAN
isPrime(UDATA number)
{
	UDATA sqrtNumber;
	UDATA i;

	if (number < 2) {
		return FALSE;
	}

	sqrtNumber = (UDATA)sqrt((double)number);
	for (i = 2; i <= sqrtNumber; i++) {
		if ((number % i) == 0) {
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * For the given size, it finds closest prime number which is not bigger than size.
 * If size is prime itself, then size is returned.
 * Otherwise, closest smaller prime number is found and returned.
 *
 * @param	 number 	The number that this function finds the closest smaller or equal prime number for
 * @return	 UDATA	 	Closest equal or smaller prime number.
 *
 * Returns 0 if number is smaller than 2,
 * otherwise returns biggest prime number smaller than or equal to number passed to this function.
 *
 */
static UDATA
testFindPreviousOrEqualPrime(UDATA number)
{
	if (number < 2) {
		return 0;
	}

	while (!isPrime(number)) {
		number--;
	}
	return number;

}

/**
 * For the given size, it finds closest prime number which is not smaller than size.
 * If size is prime itself, then size is returned.
 * Otherwise, closest bigger prime number is found and returned.
 *
 * @param 	number		The number that this function finds the closest bigger prime number for
 * @return 	UDATA 		Closest equal or bigger prime number
 *
 * Return smallest prime number which is bigger than or equal to number passed to this function.
 */
static UDATA
testFindNextOrEqualPrime(UDATA number)
{
	while (!isPrime(number)) {
		number++;
	}
	return number;
}

/**
 *	It tries to test the function findLargestPrimeLessThanOrEqualTo for every number in the supported range of primeNumberHelper
 *	If it fails, then it increments the failCount and continue testing the function for other numbers instead of returning.
 *
 *	@param portLib					Pointer to the port library.
 *	@param id						Pointer to the test name.
 *	@param passCount				Pointer to the passed tests counter.
 *	@param failCount				Pointer to the failed tests counter.
 *	@param supportedUpperRange		Max number that is supported by primeNumberHelper.
 *	@return void
 *
 */
static void
testFindLargestPrimeLessThanOrEqualTo(J9PortLibrary *portLib, char * id, UDATA *passCount, UDATA *failCount, UDATA supportedUpperRange)
{
	PORT_ACCESS_FROM_PORT(portLib);
	UDATA previousOrEqualPrime;
	UDATA previousOrEqualPrime2;
	UDATA i;

	j9tty_printf(PORTLIB, "\tTesting findLargestPrimeLessThanOrEqualTo(number) for every number in the range 0 - %u\n", supportedUpperRange);

	for (i = 0; i < supportedUpperRange; i++ ) {
		previousOrEqualPrime = testFindPreviousOrEqualPrime(i);
		previousOrEqualPrime2 = findLargestPrimeLessThanOrEqualTo(i);
		if (previousOrEqualPrime != previousOrEqualPrime2) {
			j9tty_printf(PORTLIB, "\t%s failure. Number = %u. LargestPrimeLessThanOrEqualTo = %u. Found = %u\n", id, i, previousOrEqualPrime, previousOrEqualPrime2);
			(*failCount)++;
		} else {
			(*passCount)++;
		}
	}
}

/**
 *	It tries to test the function findSmallestPrimeGreaterThanOrEqualTo for every number in the supported range of primeNumberHelper
 *	If it fails, then it increments the failCount and continue testing the function for other numbers instead of returning.
 *
 *	@param portLib					Pointer to the port library.
 *	@param id						Pointer to the test name.
 *	@param passCount				Pointer to the passed tests counter.
 *	@param failCount				Pointer to the failed tests counter.
 *	@param supportedUpperRange		Max number that is supported by primeNumberHelper.
 *	@return void
 *
 */
static void
testFindSmallestPrimeGreaterThanOrEqualTo(J9PortLibrary *portLib, char * id, UDATA *passCount, UDATA *failCount, UDATA supportedUpperRange)
{
	PORT_ACCESS_FROM_PORT(portLib);
	UDATA nextOrEqualPrime;
	UDATA nextOrEqualPrime2;
	UDATA i;

	j9tty_printf(PORTLIB, "\tTesting findSmallestPrimeGreaterThanOrEqualTo(number) for every number in the range 0 - %u\n", supportedUpperRange);
	for (i = 0; i < supportedUpperRange; i++ ) {
		nextOrEqualPrime = testFindNextOrEqualPrime(i);
		/**
		 * If the found prime number is not in the supported range of primeNumberHelper,
		 * then  we expect primeNumberHelper to return PRIMENUMBERHELPER_OUTOFRANGE
		 *
		 */
		if (nextOrEqualPrime > supportedUpperRange) {
			nextOrEqualPrime = PRIMENUMBERHELPER_OUTOFRANGE;
		}
		nextOrEqualPrime2 = findSmallestPrimeGreaterThanOrEqualTo(i);
		if (nextOrEqualPrime != nextOrEqualPrime2) {
			j9tty_printf(PORTLIB, "\t%s failure. Number = %u. FindSmallestPrimeGreaterThanOrEqualTo = %u. Found = %u\n", id, i, nextOrEqualPrime, nextOrEqualPrime2);
			(*failCount)++;
		} else {
			(*passCount)++;
		}
	}
}

/**
 *	Checks that the upper range supported by the prime number helper is acceptable.
 *
 *	@param portLib					Pointer to the port library.
 *	@param id						Pointer to the test name.
 *	@param passCount				Pointer to the passed tests counter.
 *	@param failCount				Pointer to the failed tests counter.
 *	@param supportedUpperRange		Max number that is supported by primeNumberHelper.
 *
 */
static void
testPrimeUpperRange(J9PortLibrary *portLib, char * id, UDATA *passCount, UDATA *failCount, UDATA supportedUpperRange)
{
	PORT_ACCESS_FROM_PORT(portLib);
	/* Check that the supported upper range is at least twice the size of max U_16, as required for checkDuplicateMembers() in cfreader.c. */
	UDATA minimumNeededUpperRange = 65535 * 2;

	j9tty_printf(PORTLIB, "\tTesting testPrimeUpperRange(%u >= %u)\n", supportedUpperRange, minimumNeededUpperRange);
	if (supportedUpperRange < minimumNeededUpperRange) {
		j9tty_printf(PORTLIB, "\t%s failure. supportedUpperRange=%u is less than %u\n", id, supportedUpperRange, minimumNeededUpperRange);
		(*failCount)++;
	} else {
		(*passCount)++;
	}
}


/**
 * It verifies the functionality of primeNumberHelper.
 * It basically tests two functionality pf primeNumberhelper:
 * 		-findSmallestPrimeGreaterThanOrEqualTo
 * 		-findLargestPrimeLessThanOrEqualTo
 *
 * @param 	portlib 	Pointer to the port library.
 * @param	passCount	Pointer to the passed tests counter.
 * @param 	failCount  	Pointer to the failed tests counter.
 * @return 	0
 */
I_32
verifyPrimeNumberHelper(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount)
{
	I_32 rc = 0;
	UDATA upperRange;
	UDATA start, end;
	PORT_ACCESS_FROM_PORT(portLib);

	j9tty_printf(PORTLIB, "Testing primeNumberHelper functions...\n");

	start = j9time_usec_clock();
	upperRange = getSupportedBiggestNumberByPrimeNumberHelper();
	testPrimeUpperRange(portLib, "testPrimeUpperRange", passCount, failCount, upperRange);
	testFindLargestPrimeLessThanOrEqualTo(portLib, "testFindLargestPrimeLessThanOrEqualTo", passCount, failCount, upperRange);
	testFindSmallestPrimeGreaterThanOrEqualTo(portLib, "testFindSmallestPrimeGreaterThanOrEqualTo", passCount, failCount, upperRange);
	end = j9time_usec_clock();
	j9tty_printf(PORTLIB, "Finished testing primeNumberHelper functions.\n");
	j9tty_printf(PORTLIB, "Testing primeNumberHelper functions execution time was %d (usec).\n", (end-start));

	return rc;
}
