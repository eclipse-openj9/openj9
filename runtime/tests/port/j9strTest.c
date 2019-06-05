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

/*
 * $RCSfile: j9strTest.c,v $
 * $Revision: 1.46 $
 * $Date: 2011-12-30 14:15:00 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library string operations.
 * 
 * Exercise the API for port library string operations.  These functions 
 * can be found in the file @ref j9str.c and in file @ref j9strftime.c 
 * 
 * @note port library string operations are not optional in the port library table.
 * 
 */
#if defined(WIN32)
#include <windows.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "testHelpers.h"
#include "j9port.h"
#include "omrstdarg.h"

#define J9STR_BUFFER_SIZE 128 /**< @internal A buffer size to play with */
#define J9STR_PRIVATE_RETURN_VALUE ((U_32) 0xBADC0DE) /**< @internal A known return for @ref fake_j9str_printf */

#if 0
/* some useful numbers */
#define NUM_MSECS_IN_YEAR 31536000000
#define NUM_MSECS_IN_THIRTY_DAYS 2592000000

/* 1192224606740 -> 2007/10/12 17:30:06 */
/* 1150320606740 -> 2006/06/14 17:30:06 */
/* 1160688606740 -> 2006/10/12 17:30:06 */
/* 1165872606740 -> 2006/12/11 16:30:06 */
/* 1139952606740 -> 2006/02/14 16:30:06 */
#endif


/**
 * @internal
 * @typdef
 * Function prototype for verifying j9str_printf calls j9str_vprintf
 */
typedef UDATA (* J9STR_VPRINTF_FUNC) (struct OMRPortLibrary *portLibrary, char *buf, UDATA bufLen, const char* format, va_list args);

/**
 * Verify helpers functions call correct functions.
 * 
 * Override @ref j9str.c::j9str_vprintf "j9str_vprintf()" and return a known value to verify any helper
 * functions that  should be calling this function truly are.
 * 
 * @param[in] portLibrary The port library.
 * @param[in, out] buf The string buffer to be written.
 * @param[in] bufLen The size of the string buffer to be written.
 * @param[in] format The format of the string.
 * @param[in] args Arguments for the format string.
 *
 * @return J9STR_PRIVATE_RETURN_VALUE.
 */
static UDATA
fake_j9str_vprintf(struct OMRPortLibrary *portLibrary, char *buf, UDATA bufLen, const char* format, va_list args)
{
	return J9STR_PRIVATE_RETURN_VALUE;
}

/** 
 * @internal
 * Helper function for string verification.
 * 
 * Given a format string and it's arguments create the requested output message and
 * put it in the provided buffer.
 * 
 * @param[in] portLibrary The port library under test
 * @param[in] testName The name of the test requesting this functionality
 * @param[in,out] buffer buffer to write to
 * @param[in] bufLength Size of buffer
 * @param[in] expectedResult Expected output
 * @param[in] format The format string to use
 * @param[in] ... Arguments for format string
 */
static void
validate_j9str_vprintf(struct J9PortLibrary *portLibrary, const char *testName, char *buffer, UDATA bufLength, const char *expectedResult, const char *format, va_list args)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA rc;

	rc = j9str_vprintf(buffer, bufLength, format, args);
	if (strlen(expectedResult) != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9str_vprintf(\"%s\") with \"%s\" returned %d, expected %d\n", format, expectedResult, rc, strlen(expectedResult));
	}

	if (strlen(buffer) != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9str_vprintf(\"%s\") with \"%s\" returned buffer with length %d, expected %d\n", format, expectedResult, strlen(buffer), rc);
	}

	if (strcmp(buffer, expectedResult) != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9str_vprintf(\"%s\") with \"%s\" returned \"%s\" expected \"%s\"\n", format, expectedResult, buffer, expectedResult);
	}
	memset(buffer, ' ', bufLength);
}

/**
 * @internal
 * Helper function for string verification.
 * 
 * Given a format string and it's arguments create the requested output message and
 * try to store it in a null buffer.
 * 
 * @param[in] portLibrary The port library under test
 * @param[in] testName The name of the test requesting this functionality
 * @param[in,out] buffer buffer to write to
 * @param[in] bufLength Size of buffer
 * @param[in] expectedLength Size of buffer required for format string
 * @param[in] format The format string to use
 * @param[in] args Arguments for format string
 */
static void
validate_j9str_vprintf_with_NULL(struct J9PortLibrary *portLibrary, const char *testName, U_32 bufLength, U_32 expectedLength, const char *format, va_list args)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA rc;

	/* Return the length of buffer required for the format string */
	rc = j9str_vprintf(NULL, bufLength, format, args);
	if (expectedLength != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9str_vprintf(\"%s\") returned %d expected %d\n", format, rc, expectedLength);
	}
}

/**
 * @internal
 * Helper function for string verification.
 * 
 * Given a format string and it's arguments verify the following behaviour
 * \args Given a larger buffer formatting is correct
 * \args Given a buffer of exact size the formatting is correct
 * \args Given a smaller buffer the formatting is truncated
 * 
 * @param[in] portLibrary The port library under test
 * @param[in] testName The name of the test requesting this functionality
 * @param[in] format The format string
 * @param[in] expectedResult The expected result
 * @param[in] ... arguments for format
 */
static void
test_j9str_vprintf(struct J9PortLibrary *portLibrary, const char *testName, const char *format, const char *expectedResult, ...) 
{
	char truncatedExpectedResult[512];
	char actualResult[512]; 
	va_list args;

	/* Buffer larger than required */
	va_start(args, expectedResult);
	validate_j9str_vprintf(portLibrary, testName, actualResult, sizeof(actualResult), expectedResult, format, args);
	va_end(args);

	/* Exact size buffer (+1 for NUL)*/
	va_start(args, expectedResult);
	validate_j9str_vprintf(portLibrary, testName, actualResult, strlen(expectedResult) + 1, expectedResult, format, args);
	va_end(args);

	/* Truncated buffer - use strlen(expectedResult) as length of buffer thus reducing the size by 1*/
	if (strlen(expectedResult) > 1) {
		strcpy(truncatedExpectedResult, expectedResult);
		truncatedExpectedResult[strlen(expectedResult)-1] = '\0';

		va_start(args, expectedResult);
		validate_j9str_vprintf(portLibrary, testName, actualResult, strlen(expectedResult), truncatedExpectedResult, format, args);
		va_end(args);


		/* Truncated buffer - use strlen(expectedResult)-1 as length of buffer thus reducing the size by 2*/
		if (strlen(expectedResult) > 2) {
			truncatedExpectedResult[strlen(expectedResult)-2] = '\0';

			va_start(args, expectedResult);
			validate_j9str_vprintf(portLibrary, testName, actualResult, strlen(expectedResult)-1, truncatedExpectedResult, format, args);
			va_end(args);
		}
	}

	/* Some NULL test, size of buffer does not matter */
	/* NULL, 0, expect size of buffer required */
	va_start(args, expectedResult);
	validate_j9str_vprintf_with_NULL(portLibrary, testName, 0, (U_32)strlen(expectedResult)+1, format, args);
	va_end(args);

	/* NULL, -1, expect size of buffer required */
	va_start(args, expectedResult);
	validate_j9str_vprintf_with_NULL(portLibrary, testName, (U_32)-1, (U_32)strlen(expectedResult)+1, format, args);
	va_end(args);

	/* NULL, truncated buffer, use strlen(expectedResult) as length of buffer thus reducing the size by 1 */
	va_start(args, expectedResult);
	validate_j9str_vprintf_with_NULL(portLibrary, testName, (U_32)strlen(expectedResult), (U_32)strlen(expectedResult)+1, format, args);
	va_end(args);
}

/**
 * @internal
 * Helper function for string verification.
 * 
 * @param[in] portLibrary The port library under test
 * @param[in] testName The name of the test requesting this functionality
 * @param[in] ... arguments for format string
 * 
 * @note Pretty bogus to pass an argument for a format string you don't know about
 */
static void
test_j9str_vprintfNulChar(struct J9PortLibrary *portLibrary, const char* testName, ...)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA rc;
	char actualResult[1024];
	const char* format = "ab%cde";
	const char* expectedResult = "ab";
	va_list args;
	
	va_start(args, testName);
	rc = j9str_vprintf(actualResult, sizeof(actualResult), format, args);
	va_end(args);
	if (5 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9str_vprintf(\"%s\") returned %d expected 5\n", format, rc);
	}
	
	if (strlen(actualResult) != 2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9str_vprintf(\"%s\") returned %d expected 2\n", format, rc);
	}
	
	if (strcmp(actualResult, expectedResult) != 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9str_vprintf(\"%s\") returned \"%s\" expected \"%s\"\n", format, expectedResult, actualResult);
	}
}

/**
 * @internal
 * Helper function for strftime verification.
 *
 * @param[in] portLibrary The port library under test
 * @param[in] testName The name of the test invoking this function
 * @param[out] buf The formatted time string
 * @param[in] bufLen The length of buf
 * @param[in] format Specifies the format of the time that will be written to buf
 * @param[in] timeMillis The time to format in ms
 * @param[in] expectedBuf The value that is to be expected following the call to j9str_ftime
 *
 * @return TEST_PASS if buf and expectedBuf are identical, TEST_FAIL if not
 */
static void
test_j9str_ftime(struct J9PortLibrary *portLibrary, const char *testName, char *buf, UDATA bufLen, const char *format, I_64 timeMillis, const char *expectedBuf)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	j9str_ftime(buf, bufLen, format, timeMillis);
	if (0 != strncmp(buf, expectedBuf, bufLen)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Expected \"%s\", Got \"%s\"\n", expectedBuf, buf);
	}
}

/**
 * @internal
 * Helper function for subst_tokens verification.
 *
 * @param[in] portLibrary The port library under test
 * @param[in] testName The name of the test invoking this function
 * @param[out] buf The formatted time string
 * @param[in] bufLen The length of buf
 * @param[in] format Specifies the format of string written to buf
 * @param[in] tokens The tokens to be used to expand the format string
 * @param[in] expectedBuf The expected value following the call to j9str_subst_tokens
 * @param[in] expectedRet The return value expected from the call to j9str_subst_tokens
 */
static void
test_j9str_tokens(struct J9PortLibrary *portLibrary, const char *testName, char *buf, UDATA bufLen, const char *format, struct J9StringTokens *tokens, const char *expectedBuf, UDATA expectedRet)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA rc;

	rc = j9str_subst_tokens(buf, bufLen, format, tokens);
	if (rc != expectedRet) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Expected rc = %d, Got rc = %d\n", expectedRet, rc);
	}

	if (buf && (0 != strncmp(buf, expectedBuf, bufLen))) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Expected \"%s\", Got \"%s\"\n", expectedBuf, buf);
	}
}

/**
 * Verify port library string operations.
 * 
 * Ensure the port library is properly setup to run string operations.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_test0(struct J9PortLibrary *portLibrary)
{
	const char* testName = "j9str_test0";

	reportTestEntry(portLibrary, testName);
	 
	/* Verify that the string function pointers are non NULL */
	
	/* Not tested, implementation dependent.  No known functionality.
	 * Startup is private to the portlibrary, it is not re-entrant safe
	 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->str_startup) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->str_startup is NULL\n");
	}

	/*  Not tested, implementation dependent.  No known functionality */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->str_shutdown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->str_shutdown is NULL\n");
	}

	/* j9str_test1 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->str_printf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->str_printf is NULL\n");
	}

	/* j9str_test2 */
	if (NULL == OMRPORT_FROM_J9PORT(portLibrary)->str_vprintf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->str_vprintf is NULL\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library string operations.
 *
 * @ref j9str.c::j9str_printf "j9str_printf()" is a helper function for 
 * j9str.c::j9str_vprintf "j9str_vprintf().  It only makes sense to implement
 * one in terms of the other.  To verify this has indeed been done, replace 
 * j9str_printf with a fake one that returns a known value.  If that value is
 * not returned then fail the test.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_test1(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9str_test1";
	UDATA j9strRC;
	J9STR_VPRINTF_FUNC realVprintf;
	
	reportTestEntry(portLibrary, testName);

	/* Save the real function, put in a fake one, call it, restore old one */
	realVprintf = OMRPORT_FROM_J9PORT(portLibrary)->str_vprintf;
	OMRPORT_FROM_J9PORT(portLibrary)->str_vprintf = fake_j9str_vprintf;
	j9strRC = j9str_printf(PORTLIB, NULL, 0, "Simple test");
	OMRPORT_FROM_J9PORT(portLibrary)->str_vprintf = realVprintf;

	if (J9STR_PRIVATE_RETURN_VALUE != j9strRC) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9str_printf() does not call j9str_vprintf()\n");
	}

	j9strRC = j9str_printf(PORTLIB, NULL, 0, "Simple test");
	if ((strlen("Simple test")+1) != j9strRC) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9str_vprintf() not restored\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library string operations.
 *
 * Run various format strings through @ref j9str.c::j9str_vprintf "j9str_vprintf()".
 * Tests include basic printing of characters, strings, numbers and Unicode characters.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_test2(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9str_test2";
	U_16 unicodeString[32];

	reportTestEntry(portLibrary, testName);

	test_j9str_vprintf(portLibrary, testName, "Simple test", "Simple test");

	test_j9str_vprintf(portLibrary, testName, "%d", "123", 123);
	test_j9str_vprintf(portLibrary, testName, "%d", "-123", -123);
	test_j9str_vprintf(portLibrary, testName, "%+d", "-123", -123);
	test_j9str_vprintf(portLibrary, testName, "%+d", "+123", 123);
	test_j9str_vprintf(portLibrary, testName, "%+d", "+0", 0);
	test_j9str_vprintf(portLibrary, testName, "%x", "123", 0x123);
	test_j9str_vprintf(portLibrary, testName, "%5.5d", "00123", 123);
	test_j9str_vprintf(portLibrary, testName, "%5.5d", "-00123", -123);
	test_j9str_vprintf(portLibrary, testName, "%4.5d", "00123", 123);
	test_j9str_vprintf(portLibrary, testName, "%4.5d", "-00123", -123);
	test_j9str_vprintf(portLibrary, testName, "%5.4d", " 0123", 123);
	test_j9str_vprintf(portLibrary, testName, "%5.4d", "-0123", -123);
	test_j9str_vprintf(portLibrary, testName, "%5d", "  123", 123);
	test_j9str_vprintf(portLibrary, testName, "%5d", " -123", -123);
	test_j9str_vprintf(portLibrary, testName, "%-5d", "123  ", 123);
	test_j9str_vprintf(portLibrary, testName, "%-5d", "-123 ", -123);
	test_j9str_vprintf(portLibrary, testName, "%-5.4d", "0123 ", 123);
	test_j9str_vprintf(portLibrary, testName, "%-5.4d", "-0123", -123);
	test_j9str_vprintf(portLibrary, testName, "%03d", "-03", -3);

	/* test some simple floating point cases */
	test_j9str_vprintf(portLibrary, testName, "%-5g", "1    ", 1.0);

	/* validate NUL char */
	test_j9str_vprintfNulChar(portLibrary, testName, 0);

	test_j9str_vprintf(portLibrary, testName, "%5c", "    x", 'x');
	test_j9str_vprintf(portLibrary, testName, "%5lc", "    x", 'x');
	test_j9str_vprintf(portLibrary, testName, "%5lc", "   \xc2\x80", 0x80);
	test_j9str_vprintf(portLibrary, testName, "%5lc", "   \xc4\xa8", 0x128);
	test_j9str_vprintf(portLibrary, testName, "%5lc", "   \xdf\xbf", 0x7ff);
	test_j9str_vprintf(portLibrary, testName, "%5lc", "  \xe0\xa0\x80", 0x800);
	test_j9str_vprintf(portLibrary, testName, "%5lc", "  \xef\x84\xa3", 0xf123);
	test_j9str_vprintf(portLibrary, testName, "%5lc", "  \xef\xbf\xbf", 0xffff);

#ifdef J9VM_ENV_DATA64
	test_j9str_vprintf(portLibrary, testName, "%p", "BAADF00DDEADBEEF", (void*)0xBAADF00DDEADBEEF);
#else
	test_j9str_vprintf(portLibrary, testName, "%p", "DEADBEEF", (void*)0xDEADBEEF);
#endif

	test_j9str_vprintf(portLibrary, testName, "%s", "foo", "foo");
	test_j9str_vprintf(portLibrary, testName, "%.2s", "fo", "foo");
	test_j9str_vprintf(portLibrary, testName, "%5s", "  foo", "foo");
	test_j9str_vprintf(portLibrary, testName, "%-5s", "foo  ", "foo");

	unicodeString[0] = 'f';
	unicodeString[1] = 'o';
	unicodeString[2] = 'o';
	unicodeString[3] = '\0';
	test_j9str_vprintf(portLibrary, testName, "%ls", "foo", unicodeString);
	test_j9str_vprintf(portLibrary, testName, "%.2ls", "fo", unicodeString);
	test_j9str_vprintf(portLibrary, testName, "%5ls", "  foo", unicodeString);
	test_j9str_vprintf(portLibrary, testName, "%-5ls", "foo  ", unicodeString);

	unicodeString[0] = 'f';
	unicodeString[1] = 'o';
	unicodeString[2] = 'o';
	unicodeString[3] = 0x80;
	unicodeString[4] = 0x800;
	unicodeString[5] = 0xffff;
	unicodeString[6] = 0x128;
	unicodeString[7] = '\0';
	test_j9str_vprintf(portLibrary, testName, "%ls", "foo\xc2\x80\xe0\xa0\x80\xef\xbf\xbf\xc4\xa8", unicodeString);
	test_j9str_vprintf(portLibrary, testName, "%15ls", "  foo\xc2\x80\xe0\xa0\x80\xef\xbf\xbf\xc4\xa8", unicodeString);
	test_j9str_vprintf(portLibrary, testName, "%4.2ls", "  fo", unicodeString);

	unicodeString[0] = 0x80;
	unicodeString[1] = 0x800;
	unicodeString[2] = 0xffff;
	unicodeString[3] = 0x128;
	unicodeString[4] = '\0';
	test_j9str_vprintf(portLibrary, testName, "%ls", "\xc2\x80\xe0\xa0\x80\xef\xbf\xbf\xc4\xa8", unicodeString);
	test_j9str_vprintf(portLibrary, testName, "%-12ls", "\xc2\x80\xe0\xa0\x80\xef\xbf\xbf\xc4\xa8  ", unicodeString);
	test_j9str_vprintf(portLibrary, testName, "%-8.2ls", "\xc2\x80\xe0\xa0\x80   ", unicodeString);

	/* test argument re-ordering */
	test_j9str_vprintf(portLibrary, testName, "%1$d %2$d", "123 456", 123, 456);
	test_j9str_vprintf(portLibrary, testName, "%2$d %1$d", "456 123", 123, 456);
	test_j9str_vprintf(portLibrary, testName, "%*.*d", "00123", 4, 5, 123);
	test_j9str_vprintf(portLibrary, testName, "%1$*2$.*3$d", "00123", 123, 4, 5);
	test_j9str_vprintf(portLibrary, testName, "%1$*3$.*2$d", "00123", 123, 5, 4);
	test_j9str_vprintf(portLibrary, testName, "%2$*1$.*3$d", "00123", 4, 123, 5);
	test_j9str_vprintf(portLibrary, testName, "%2$*3$.*1$d", "00123", 5, 123, 4);
	test_j9str_vprintf(portLibrary, testName, "%3$*1$.*2$d", "00123", 4, 5, 123);
	test_j9str_vprintf(portLibrary, testName, "%3$*2$.*1$d", "00123", 5, 4, 123);

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library string operations.
 *
 * Exercise @ref j9strftime.c::j9strftime "j9strftime()" with various format strings and times.
 * Tests include 
 * 	1. too short a dest buffer
 * 	2. ime 0 
 * 	3. a known time (February 29th 2004 01:23:45)
 * 	4. Tokens that are not valid for str_ftime but are set by default in j9str_create_tokens. 
 * 		Check that these are not substituted.
 *  5. Tokens that are not valid by default anywhere.
 * 
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_test3(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char *testName = "j9str_test3";
	char buf[J9STR_BUFFER_SIZE];
	char expected[J9STR_BUFFER_SIZE];
	U_64 timeMillis;
	UDATA ret;
	
	reportTestEntry(portLibrary, testName);
	
	/* First test: The epoch */
	j9tty_printf(PORTLIB, "\t This test could fail if you abut the international dateline (westside of dateline)\n"); 
	timeMillis = 0;
	strncpy(expected, "1970 01 Jan 01 XX:00:00", J9STR_BUFFER_SIZE);
	test_j9str_ftime(portLibrary, testName, buf, J9STR_BUFFER_SIZE, "%Y %m %b %d XX:%M:%S", timeMillis, expected);


	
	/* Second Test: February 29th, 2004, 12:00:00, UTC. */
	timeMillis = J9CONST64(1078056000000);
	strncpy(expected, "2004 02 Feb 29 00", J9STR_BUFFER_SIZE);
	test_j9str_ftime(portLibrary, testName, buf, J9STR_BUFFER_SIZE, "%Y %m %b %d %S", timeMillis, expected);

	/* Third test: Too short a buffer */
	ret = j9str_ftime(buf, 3, "%Y", timeMillis);
	if (ret < 3) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Undersized buffer not detected. Expected %d, got %d\n", 5, ret);
	}

	/* Fourth Test: Pass in Tokens that are not valid for str_ftime but are valid in the rest of the token API.
	 * Use the time February 29th, 2004, 12:00:00, UTC. */
	timeMillis = J9CONST64(1078056000000);
	strncpy(expected, "2004 02 Feb 29 00 %pid %uid %job %home %last %seq", J9STR_BUFFER_SIZE);
	test_j9str_ftime(portLibrary, testName, buf, J9STR_BUFFER_SIZE, "%Y %m %b %d %S %pid %uid %job %home %last %seq", timeMillis, expected);
		
	/* Fifth Test: Pass in Tokens that are not valid by default anywhere.
	 * Use the time February 29th, 2004, 12:00:00, UTC. */
	timeMillis = J9CONST64(1078056000000);
	strncpy(expected, "2004 02 Feb 29 00 %zzz = %zzz", J9STR_BUFFER_SIZE);
	test_j9str_ftime(portLibrary, testName, buf, J9STR_BUFFER_SIZE, "%Y %m %b %d %S %zzz = %%zzz", timeMillis, expected);
		
	/* Sixth Test: Pass in all time tokens.
	 * Use the time February 29th, 2004, 12:00:00, UTC. */
	timeMillis = J9CONST64(1078056000000);
	strncpy(expected, "04,(2004) 02,(Feb) 29 XX:00:00 %", J9STR_BUFFER_SIZE);
	test_j9str_ftime(portLibrary, testName, buf, J9STR_BUFFER_SIZE, "%y,(%Y) %m,(%b) %d XX:%M:%S %", timeMillis, expected);
			
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library token operations.
 *
 * Exercise @ref j9str.c::j9str_subst_tokens "j9str_subst_tokens" with various tokens.
 * Tests include simple token tests, too short a dest buffer and token precedence
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_test4(struct J9PortLibrary *portLibrary)
{
#define TEST_BUF_LEN 1024
#define TEST_OVERFLOW_LEN 8
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char *testName = "j9str_test4";
	I_64 timeMillis;
	struct J9StringTokens *tokens;
	char buf[TEST_BUF_LEN];
	char bufOverflow[TEST_OVERFLOW_LEN];

	
	reportTestEntry(portLibrary, testName);
	
	/* February 29th, 2004, 12:00:00, UTC */
	timeMillis = J9CONST64(1078056000000);
	
	j9tty_printf(PORTLIB, "\t This test will fail if you abut the international dateline\n");
	tokens = j9str_create_tokens(timeMillis);
	j9str_set_token(portLibrary, tokens, "longtkn", "Long Token Value");
	j9str_set_token(portLibrary, tokens, "yyy", "nope nope nope");
	j9str_set_token(portLibrary, tokens, "yyy", "yup yup yup");
	j9str_set_token(portLibrary, tokens, "empty", "");
	if (NULL == tokens) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to create tokens\n");
	} else {
		/* Test 1: No tokens */
		test_j9str_tokens(portLibrary, testName, buf, TEST_BUF_LEN,
		                  "Teststring", tokens,
		                  "Teststring", 10);
		/* Test 2: Single token */
		test_j9str_tokens(portLibrary, testName, buf, TEST_BUF_LEN,
		                  "%Y", tokens,
		                  "2004", 4);
		/* Test 3: End with a token */
		test_j9str_tokens(portLibrary, testName, buf, TEST_BUF_LEN,
		                  "Teststring %Y", tokens,
		                  "Teststring 2004", 15);
		/* Test 4: Start with a token */
		test_j9str_tokens(portLibrary, testName, buf, TEST_BUF_LEN,
		                  "%y Teststring", tokens,
		                  "04 Teststring", 13);
		/* Test 5: Many tokens */
		test_j9str_tokens(portLibrary, testName, buf, TEST_BUF_LEN,
		                  "%Y/%m/%d %S seconds %longtkn", tokens,
		                  "2004/02/29 00 seconds Long Token Value", 38);
		/* Test 6: Tokens and strings combined */
		test_j9str_tokens(portLibrary, testName, buf, TEST_BUF_LEN,
		                  "Test1 %Y Test2-%m%d%y-%S %longtkn, %longtkn", tokens,
		                  "Test1 2004 Test2-022904-00 Long Token Value, Long Token Value",
		                  61);
		/* Test 7: %% and end with % */
		test_j9str_tokens(portLibrary, testName, buf, TEST_BUF_LEN,
		                  "99%% is the same as 99%", tokens,
		                  "99% is the same as 99%", 22);
		/* Test 8: Invalid tokens */
		test_j9str_tokens(portLibrary, testName, buf, TEST_BUF_LEN,
		                  "%zzz = %%zzz", tokens,
		                  "%zzz = %zzz", 11);
		/* Test 9: Excessive % stuff */
		test_j9str_tokens(portLibrary, testName, buf, TEST_BUF_LEN,
		                  "%Y%%%m%%%d %zzz% %%%%%%", tokens,
		                  "2004%02%29 %zzz% %%%", 20);
		/* Test 10: Simple string, buffer too short */
		test_j9str_tokens(portLibrary, testName, buf, 7, "Teststring", tokens, "Testst", 11);
		/* Test 11: Single token, buffer too short */
		test_j9str_tokens(portLibrary, testName, buf, 3, "%Y", tokens, "20", 5);	
		/* Test 12: Test for overflow with an actual short buffer (simple string) */
		test_j9str_tokens(portLibrary, testName, bufOverflow, TEST_OVERFLOW_LEN,
		                  "Teststring", tokens,
		                  "Teststr", 11);
		/* Test 13: Test for overflow with an actual short buffer (token) */
		test_j9str_tokens(portLibrary, testName, bufOverflow, TEST_OVERFLOW_LEN,
		                  "Test %Y", tokens,
		                  "Test 20", 10);
		/* Test 14: Test for token precedence based on length */
		test_j9str_tokens(portLibrary, testName, buf, TEST_BUF_LEN,
		                  "%yyy is not %yy is not %y", tokens,
		                  "yup yup yup is not 04y is not 04", 32);
		/* Test 15: Test the read only mode (should return the required buf len) */
		test_j9str_tokens(portLibrary, testName, NULL, 0,
		                  "%yyy is not %yy is not %y", tokens,
		                  NULL, 33); /* 33 because must include \0 */
		/* Test 16: Test an empty token */
		test_j9str_tokens(portLibrary, testName, buf, TEST_BUF_LEN,
		                  "x%emptyx == xx", tokens,
		                  "xx == xx", 8);

		/* Test 17: All tokens that we get for free in j9str_create_tokens (and that the user may be relying on) */
		{

			char expected[J9STR_BUFFER_SIZE];
			char *default_tokens[] = { " %pid", " %home", " %last", " %seq"
					," %uid"
#if defined(J9ZOS390)
					, "%job"
#endif
					};

			UDATA i = 0;
			UDATA rc;
			char *timePortionOfFormatString = "%y,(%Y) %m,(%b) %d XX:%M:%S %"; 
			char *fullFormatString = NULL;
			UDATA sizeOfFullFormatString = 0;
		
			strncpy(expected, "04,(2004) 02,(Feb) 29 XX:00:00 %", J9STR_BUFFER_SIZE);
			
			/* build the format string (tack on the default tokens) */
			
			/* how large does it need to be? */
			sizeOfFullFormatString = strlen(timePortionOfFormatString);
			for (i = 0 ; i < (sizeof(default_tokens)/sizeof(char *)) ; i++) {
				sizeOfFullFormatString = sizeOfFullFormatString + strlen(default_tokens[i]);
			}	
			sizeOfFullFormatString = sizeOfFullFormatString + 1 /* null terminator */;

			/* ok, now build the format string */
			fullFormatString = j9mem_allocate_memory(sizeOfFullFormatString, OMRMEM_CATEGORY_PORT_LIBRARY);
			strncpy(fullFormatString, timePortionOfFormatString, sizeOfFullFormatString);
			for (i = 0 ; i < (sizeof(default_tokens)/sizeof(char *)) ; i++) {
				strncat(fullFormatString, default_tokens[i], sizeOfFullFormatString - strlen(fullFormatString));
			}		
			
			rc = j9str_subst_tokens(buf, TEST_BUF_LEN, fullFormatString, tokens);
			
			/* We don't know how long the returned string will be, 
			 * but make sure that something has been substituted for each token and that the time is substituted properly .
			 * Do this by making sure that %<token> is not present in the output buffer*/
			if (rc > TEST_BUF_LEN) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Test buffer (%i) is too small. Required size: %i\n", TEST_BUF_LEN, rc);
			}
			if (0 == strstr(buf, expected)) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "[%s] was not found in [%s]\n", expected, buf);
			}
			for (i = 0 ; i < (sizeof(default_tokens)/sizeof(char *)) ; i++) {
				if (NULL != strstr(buf, default_tokens[i])) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "%s was not substituted\n", default_tokens[i]);
				}
			}
			j9mem_free_memory(fullFormatString);
			fullFormatString = NULL;
		}
	
		/* We're done, let's cleanup */
		j9str_free_tokens(tokens);
	}

	return reportTestExit(portLibrary, testName);
#undef TEST_BUF_LEN
#undef TEST_OVERFLOW_LEN
}

/**
 * Verify port library token operations.
 *
 * j9str_test5 isn't really a test. Just want to do the normal use case of j9time_current_time_millis() followed 
 *  by j9str_create_tokens().
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_test5(struct J9PortLibrary *portLibrary)
{
#define TEST_BUF_LEN 1024
#define TEST_OVERFLOW_LEN 8
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char *testName = "j9str_test5";
	I_64 timeMillis;
	struct J9StringTokens *tokens;
	char buf[TEST_BUF_LEN];
		
	reportTestEntry(portLibrary, testName);
	
	j9tty_printf(PORTLIB, "\n\tThe results of j9str_test5 are not evaluated as time conversions are specific to a time zone.\n");
	j9tty_printf(PORTLIB, "\tTherefore, this is for your viewing pleasure only.\n");
	
	timeMillis = j9time_current_time_millis();
	j9tty_printf(PORTLIB, "\n\tj9time_current_time_millis returned: %lli\n", timeMillis);
	j9tty_printf(PORTLIB, "\t...creating and substituting tokens...\n");
	tokens = j9str_create_tokens(timeMillis);
	(void)j9str_subst_tokens(buf, TEST_BUF_LEN, "%Y/%m/%d %H:%M:%S", tokens);
	j9tty_printf(PORTLIB, "\tThe current time was converted to: %s\n",buf);
	
 	timeMillis = J9CONST64(1139952606740);
	j9tty_printf(PORTLIB, "\n\t using UTC timeMillis = %lli. \n\t", timeMillis);
	j9tty_printf(PORTLIB, "\t...creating and substituting tokens...\n");
	tokens = j9str_create_tokens(timeMillis);
	(void)j9str_subst_tokens(buf, TEST_BUF_LEN, "%Y/%m/%d %H:%M:%S", tokens);
	j9tty_printf(PORTLIB, "\tExpecting local time (in Ottawa, Eastern Standard Time): 2006/02/14 16:30:06 \n", timeMillis);
	j9tty_printf(PORTLIB, "\t                                       ... converted to: %s\n",buf);
	
	timeMillis = J9CONST64(1150320606740);
	j9tty_printf(PORTLIB, "\n\t using UTC timeMillis = %lli. \n\t ", timeMillis);
	j9tty_printf(PORTLIB, "\t...creating and substituting tokens...\n");
	tokens = j9str_create_tokens(timeMillis);
	(void)j9str_subst_tokens(buf, TEST_BUF_LEN, "%Y/%m/%d %H:%M:%S", tokens);
	j9tty_printf(PORTLIB, "\tExpecting local time (in Ottawa, Eastern Daylight Time): 2006/06/14 17:30:06 \n", timeMillis);
	j9tty_printf(PORTLIB, "\t                                       ... converted to: %s\n",buf);
	
	timeMillis = J9CONST64(1160688606740);
	j9tty_printf(PORTLIB, "\n\t using UTC timeMillis = %lli. \n\t ", timeMillis);
	j9tty_printf(PORTLIB, "\t...creating and substituting tokens...\n");
	tokens = j9str_create_tokens(timeMillis);
	(void)j9str_subst_tokens(buf, TEST_BUF_LEN, "%Y/%m/%d %H:%M:%S", tokens);
	j9tty_printf(PORTLIB, "\tExpecting local time (in Ottawa, Eastern Daylight Time): 2006/10/12 17:30:06 \n", timeMillis);
	j9tty_printf(PORTLIB, "\t                                       ... converted to: %s\n",buf);

	timeMillis = J9CONST64(1165872606740);
	j9tty_printf(PORTLIB, "\n\t using UTC timeMillis = %lli. \n\t", timeMillis);
	j9tty_printf(PORTLIB, "\t...creating and substituting tokens...\n");
	tokens = j9str_create_tokens(timeMillis);
	(void)j9str_subst_tokens(buf, TEST_BUF_LEN, "%Y/%m/%d %H:%M:%S", tokens);
	j9tty_printf(PORTLIB, "\tExpecting local time (in Ottawa, Eastern Standard Time): 2006/12/11 16:30:06 \n", timeMillis);
	j9tty_printf(PORTLIB, "\t                                       ... converted to: %s\n",buf);
	
	j9tty_printf(PORTLIB, "\n");
	return reportTestExit(portLibrary, testName);
	
}

/**
 * Verify port library token operations.
 *
 * Test  (-)ive value of  0-12 hours and make sure we get UTC time in millis a
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_test6(struct J9PortLibrary *portLibrary)
{
#define TEST_BUF_LEN 1024
#define TEST_OVERFLOW_LEN 8
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char *testName = "j9str_test6";
	I_64 timeMillis;
	struct J9StringTokens *tokens;
	char buf[TEST_BUF_LEN];
		
	reportTestEntry(portLibrary, testName);
	
	/* anything less than 12 hours before Epoch should come back as Epoch */
	timeMillis = 0 - 12*60*60*1000;
	tokens = j9str_create_tokens(timeMillis);
	test_j9str_tokens(portLibrary, testName, buf, TEST_BUF_LEN,
			"%Y/%m/%d %H:%M:%S", tokens,
	                  "1970/01/01 00:00:00", 19);	
	return reportTestExit(portLibrary, testName);
}

#define TEST_BUF_LEN 1024
static const char utf8String[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
'y', 'z'};
static const U_32 utf8StringLength = sizeof(utf8String);
static const U_16 utf16Data[] = {
		0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
		0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,
		0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
		0x79, 0x7a};
static const char *utf16String = (const char *) utf16Data;
static const U_32 utf16StringLength = sizeof(utf16Data);
#if defined(J9ZOS390)
#pragma convlit(suspend)
#endif
static const char platformString[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
'y', 'z', '\0'};

static const U_32 platformStringLength = sizeof(platformString)-1;
#if defined(J9ZOS390)
#pragma convlit(resume)
#endif

static void
dumpStringBytes(const char* message, const char *buffer, U_32 length) {
	U_32 index = 0;
	fprintf(stderr, "%s", message);
	for (index = 0; index < length; ++index) {
		if ((index % 16) == 0) {
			fprintf(stderr, "\n");
		}
		fprintf(stderr, "0x%x ", (unsigned char) buffer[index]);
	}
	fprintf(stderr, "\n");
}

static BOOLEAN
compareBytes(const char* expected, const char* actual, U_32 length) {
	U_32 index = 0;
	for (index = 0; index < length; ++index) {
		if (actual[index] != expected[index]) {
			fprintf(stderr, "Error at position %d \nexpected %0x actual %0x\n", index,
					(unsigned char) expected[index], (unsigned char) actual[index]);
			dumpStringBytes("Expected", expected, length);
			dumpStringBytes("Actual", actual, length);
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * Verify string platform->MUTF8 conversion, basic sanity
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
/**
 * Verify string platform->MUTF8 conversion, basic sanity
 */
int
j9str_convPlatToU8(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char *testName = "j9str_convPlatToU8";
	char outBuff[TEST_BUF_LEN];
	I_32 originalStringLength = platformStringLength;
	I_32 expectedStringLength = utf8StringLength;
	I_32 convertedStringLength = 0;

	reportTestEntry(portLibrary, testName);
	memset(outBuff, 0, sizeof(outBuff));
	convertedStringLength = j9str_convert(J9STR_CODE_PLATFORM_RAW, J9STR_CODE_MUTF8,
			platformString, originalStringLength,  outBuff, sizeof(outBuff));
	if (convertedStringLength != expectedStringLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "buffer length wrong.  Expected %d actual %d\n", expectedStringLength, convertedStringLength);
	}
	if (!compareBytes(utf8String, outBuff, expectedStringLength)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Converted string wrong.");
	}
	/* test string length */
	convertedStringLength = j9str_convert(J9STR_CODE_PLATFORM_RAW, J9STR_CODE_MUTF8,
			platformString, originalStringLength, NULL, 0);
	if (convertedStringLength != expectedStringLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "buffer length wrong.  Expected %d actual %d\n", expectedStringLength, convertedStringLength);
	}
	return reportTestExit(portLibrary, testName);
}


int
j9str_convLongString(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char *testName = "j9str_convLongString";
	char mutf8Buff[TEST_BUF_LEN];
	char returnBuff[TEST_BUF_LEN];
	char longPlatformString[300];
	I_32 originalStringLength = sizeof(longPlatformString);
	I_32 mutf8StringLength = 0;
	I_32 returnStringLength = 0;
	I_32 i = 0;

	reportTestEntry(portLibrary, testName);
	memset(mutf8Buff, 0, sizeof(mutf8Buff));
	memset(returnBuff, 0, sizeof(returnBuff));
	for (i=0; i < originalStringLength; ++i) {
		longPlatformString[i] = (i %127) + 1; /* stick to ASCII */
	}
	/* test string length */
	mutf8StringLength = j9str_convert(J9STR_CODE_PLATFORM_RAW, J9STR_CODE_MUTF8,
			longPlatformString, originalStringLength,  NULL, 0);
	if (mutf8StringLength < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Length calculation for modified UTF8 failed, error code %d\n", mutf8StringLength);
	}
	/* do the actual conversion */
	mutf8StringLength = j9str_convert(J9STR_CODE_PLATFORM_RAW, J9STR_CODE_MUTF8,
			longPlatformString, originalStringLength,  mutf8Buff, sizeof(mutf8Buff));
	if (mutf8StringLength < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Conversion to modified UTF8 failed, error code %d\n", mutf8StringLength);
	}

	/* test string length in the other direction */
	returnStringLength = j9str_convert(J9STR_CODE_MUTF8, J9STR_CODE_PLATFORM_RAW,
			mutf8Buff, mutf8StringLength, NULL, 0);
	if (returnStringLength < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Length calculation for platform failed, error code %d\n", mutf8StringLength);
	}

	/* convert back and verify that it matches the original */
	returnStringLength = j9str_convert(J9STR_CODE_MUTF8, J9STR_CODE_PLATFORM_RAW,
			mutf8Buff, mutf8StringLength, returnBuff, sizeof(returnBuff));
	if (returnStringLength < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Conversion to platform failed, error code %d\n", mutf8StringLength);
	}
	if (!compareBytes(longPlatformString, returnBuff, originalStringLength)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Converted string wrong.");
	}
	if (returnStringLength != originalStringLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "buffer length wrong.  Expected %d actual %d\n", originalStringLength, returnStringLength);
	}
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify string MUTF8->platform, basic sanity
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_convU8ToPlat(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char *testName = "j9str_convU8ToPlat";
	char outBuff[TEST_BUF_LEN];
	I_32 originalStringLength = utf8StringLength;
	I_32 expectedStringLength = platformStringLength;
	I_32 convertedStringLength = 0;

	reportTestEntry(portLibrary, testName);
	memset(outBuff, 0, sizeof(outBuff));
	convertedStringLength = j9str_convert(J9STR_CODE_MUTF8, J9STR_CODE_PLATFORM_RAW,
			utf8String, originalStringLength,  outBuff, sizeof(outBuff));
	if (convertedStringLength != expectedStringLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "buffer length wrong.  Expected %d actual %d\n", originalStringLength, convertedStringLength);
	}
	if (!compareBytes(platformString, outBuff, expectedStringLength)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Converted string wrong.");
	}
	/* test string length */
	convertedStringLength = j9str_convert(J9STR_CODE_MUTF8, J9STR_CODE_PLATFORM_RAW,
			utf8String, originalStringLength, NULL, 0);
	if (convertedStringLength != expectedStringLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "buffer length wrong.  Expected %d actual %d\n", originalStringLength, convertedStringLength);
	}
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify string MUTF8->wide, basic sanity
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_convU8ToWide(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char *testName = "j9str_convU8ToWide";
	char outBuff[TEST_BUF_LEN];
	I_32 originalStringLength = utf8StringLength;
	I_32 expectedStringLength = utf16StringLength;
	I_32 convertedStringLength = 0;

	reportTestEntry(portLibrary, testName);
	memset(outBuff, 0, sizeof(outBuff));
	convertedStringLength = j9str_convert(J9STR_CODE_MUTF8, J9STR_CODE_WIDE,
			utf8String, originalStringLength,  outBuff, sizeof(outBuff));
	if (convertedStringLength != expectedStringLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "buffer length wrong.  Expected %d actual %d\n", expectedStringLength, convertedStringLength);
	}
	if (!compareBytes(utf16String, outBuff, expectedStringLength)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Converted string wrong.");
	}
	/* test string length */
	convertedStringLength = j9str_convert(J9STR_CODE_MUTF8, J9STR_CODE_WIDE,
			utf8String, originalStringLength, NULL, 0);
	if (convertedStringLength != expectedStringLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "buffer length wrong.  Expected %d actual %d\n", originalStringLength, convertedStringLength);
	}
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify string wide->MUTF8 conversion, basic sanity
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_convWideToU8(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char *testName = "j9str_convWideToU8";
	char outBuff[TEST_BUF_LEN];
	I_32 originalStringLength = utf16StringLength;
	I_32 expectedStringLength = utf8StringLength;
	I_32 convertedStringLength = 0;

	reportTestEntry(portLibrary, testName);
	memset(outBuff, 0, sizeof(outBuff));
	convertedStringLength = j9str_convert(J9STR_CODE_WIDE, J9STR_CODE_MUTF8,
			utf16String, utf16StringLength,  outBuff, sizeof(outBuff));
	if (convertedStringLength != expectedStringLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "buffer length wrong.  Expected %d actual %d\n", expectedStringLength, convertedStringLength);
	}
	if (!compareBytes(utf8String, outBuff, expectedStringLength)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Converted string wrong.");
	}
	/* test string length */
	convertedStringLength = j9str_convert(J9STR_CODE_WIDE, J9STR_CODE_MUTF8,
			utf16String, utf16StringLength, NULL, 0);
	if (convertedStringLength != expectedStringLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "buffer length wrong.  Expected %d actual %d\n", expectedStringLength, convertedStringLength);
	}
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify string wide->mutf8 conversion null-terminates the sequence if there is sufficient space
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_convWideToU8_Null(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char *testName = "j9str_convWideToU8_Null";
	char outBuff[TEST_BUF_LEN];
	I_32 convertedStringLength = 0;

	reportTestEntry(portLibrary, testName);

	(void) memset(outBuff, '^', sizeof(outBuff)); /* initialize to non-zero */
	convertedStringLength =
			j9str_convert(J9STR_CODE_WIDE, J9STR_CODE_MUTF8,
					utf16String, utf16StringLength,  outBuff, sizeof(outBuff));

	if (0 != outBuff[convertedStringLength]) {
		outputErrorMessage(PORTTEST_ERROR_ARGS,
				"Converted string  not null terminated: expected 0, got %0x", outBuff[convertedStringLength]);
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify string MUTF8->wide conversion null-terminates the sequence if there is sufficient space
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_convU8ToWide_Null(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char *testName = "j9str_convU8ToWide_Null";
	char outBuff[TEST_BUF_LEN];
	const char * const inputString = "This is a test!";
	const size_t inputStringLength = strlen(inputString);
	U_16 *convertedStringTerminator = NULL;
	I_32 convertedStringLength = 0;

	reportTestEntry(portLibrary, testName);

	(void) memset(outBuff, '^', sizeof(outBuff)); /* initialize to non-zero */

	convertedStringLength =
			j9str_convert(J9STR_CODE_MUTF8, J9STR_CODE_WIDE,
					inputString, inputStringLength,  outBuff, sizeof(outBuff));
	convertedStringTerminator = (U_16 *) (outBuff+convertedStringLength);
	if (0 != *convertedStringTerminator) {

		outputErrorMessage(PORTTEST_ERROR_ARGS,
				"Converted string  not null terminated: expected 0, got %0x", *convertedStringTerminator);
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify string MUTF8->wide conversion null-terminates the sequence if there is sufficient space
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_convU8ToPlat_Null(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char *testName = "j9str_convU8ToPlat_Null";
	char outBuff[TEST_BUF_LEN];
	const char * const inputString = "This is a test!";
	const size_t inputStringLength = strlen(inputString);
	U_32 *convertedStringTerminator = NULL;
	I_32 convertedStringLength = 0;

	reportTestEntry(portLibrary, testName);

	(void) memset(outBuff, '^', sizeof(outBuff)); /* initialize to non-zero */
	convertedStringLength =
			j9str_convert(J9STR_CODE_MUTF8, J9STR_CODE_PLATFORM_RAW,
					inputString, inputStringLength,  outBuff, sizeof(outBuff));

	convertedStringTerminator = (U_32 *) (outBuff+convertedStringLength);
	if (0 != *convertedStringTerminator) {
		outputErrorMessage(PORTTEST_ERROR_ARGS,
				"Converted string  not null terminated: expected 0, got %0x", *convertedStringTerminator);
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify string wide->MUTF8 conversion, source string has no byte order mark
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_convWideToU8NoBom(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char *testName = "j9str_convWideToU8NoBom";
	I_32 originalStringLength = utf16StringLength;
	I_32 expectedStringLength = utf8StringLength;
	I_32 convertedStringLength = 0;

	reportTestEntry(portLibrary, testName);
	outputErrorMessage(PORTTEST_ERROR_ARGS, "test not implemented");
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify string wide->MUTF8 conversion, source string is in little-endian order
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_convWideToU8NoLittleEndian(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char *testName = "j9str_convWideToU8NoLittleEndian";
	I_32 originalStringLength = utf16StringLength;
	I_32 expectedStringLength = utf8StringLength;
	I_32 convertedStringLength = 0;

	reportTestEntry(portLibrary, testName);
	outputErrorMessage(PORTTEST_ERROR_ARGS, "test not implemented");
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify string UTF8->MUTF8 conversion, source string has no byte order mark
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_convUtf8ToMUtf8(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	char outBuff[TEST_BUF_LEN];
	const char *testName = "j9str_convUtf8ToMUtf8";
	const char utf8Data[] = { 0, /* embedded null */
			0x41, 0x42, 0x43, 0x44, /* ASCII */
			0,
			0xD0, 0xB0, 0xD0, 0xB1, 0xD0, 0xB2,  /* 2-byte UTF-8 */
			0xE4, 0xBA, 0x8C, 0xE4, 0xBA, 0x8D, 0xE4, 0xBA, 0x8E,  /* 3-byte UTF-8 */
			0xf4, 0x8f, 0xbf, 0xbf, /* Maximum code point */
			0xF0, 0x90, 0x8C, 0x82, 0xF0, 0x90, 0x8C, 0x83, 0xF0, 0x90, 0x8C, 0x84,  /* 4 byte UTF-8 */
			0x44, 0x45, 0x46, /* ASCII */
			0xbb, /* invalid character */
			0 /* null at end */
			};
	const char mutf8Data[] = {0xc0, 0x80, /* embedded null */
			0x41, 0x42, 0x43, 0x44, /* ASCII */
			0xc0, 0x80, /* embedded null */
			0xD0, 0xB0, 0xD0, 0xB1, 0xD0, 0xB2,  /* 2-byte UTF-8 */
			0xE4, 0xBA, 0x8C, 0xE4, 0xBA, 0x8D, 0xE4, 0xBA, 0x8E,  /* 3-byte UTF-8 */
			0xed, 0xaf, 0xbf, 0xed, 0xbf, 0xbf, /* Maximum code point */
			0xed, 0xa0, 0x80, 0xed, 0xbc, 0x82, 0xed, 0xa0, 0x80, 0xed, 0xbc, 0x83, 0xed, 0xa0, 0x80, 0xed, 0xbc, 0x84,
			0x44, 0x45, 0x46,
			0xef, 0xbf, 0xbd,
			0xc0, 0x80};
	I_32 utf8DataLength = sizeof(utf8Data); /* skip the terminating null */
	I_32 expectedStringLength = sizeof(mutf8Data);
	I_32 convertedStringLength = 0;

	reportTestEntry(portLibrary, testName);
	memset(outBuff, 0, sizeof(outBuff));
	convertedStringLength = j9str_convert(J9STR_CODE_UTF8, J9STR_CODE_MUTF8,
			utf8Data, utf8DataLength,  outBuff, sizeof(outBuff));
	if (convertedStringLength != expectedStringLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "buffer length wrong.  Expected %d actual %d\n", expectedStringLength, convertedStringLength);
	}
	if (!compareBytes(mutf8Data, outBuff, expectedStringLength)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Converted string wrong.");
	}
	/* test string length */
	convertedStringLength = j9str_convert(J9STR_CODE_UTF8, J9STR_CODE_MUTF8,
			utf8Data, utf8DataLength, NULL, 0);
	if (convertedStringLength != expectedStringLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "buffer length wrong.  Expected %d actual %d\n", expectedStringLength, convertedStringLength);
	}
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify string UTF8->MUTF8 conversion, source string has no byte order mark
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_convRandomUtf8ToMUtf8(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	char outBuff[TEST_BUF_LEN];
	const char *testName = "j9str_convRandomUtf8ToMUtf8";
	const char utf8Data[] = {
			0x41, 0xf2, 0x86, 0x84, 0xaf, 0xf0, 0xbc, 0x8f, 0x9b, 0xee, 0xb5, 0xa0, 0xf4, 0x86, 0xbf, 0xae,
			0xf0, 0xbe, 0xa7, 0xac, 0xf4, 0x86, 0x81, 0x84, 0xf0, 0xb3, 0x93, 0x99, 0xf2, 0xb6, 0x91, 0xae,
			0xf2, 0x96, 0xba, 0xa9, 0xf0, 0xbc, 0xb6, 0x92, 0xf1, 0x8c, 0x9c, 0xa4, 0xe4, 0x99, 0xad, 0xf2,
			0xb3, 0xa4, 0x95, 0xf0, 0xa6, 0x85, 0xab, 0xf1, 0xaa, 0xbb, 0xa4, 0xf3, 0x8c, 0x9e, 0xb8, 0xf2,
			0x98, 0x9e, 0xb8, 0xf3, 0xb8, 0x8c, 0xa5, 0xf3, 0x84, 0x8b, 0x8a, 0xf3, 0x94, 0x95, 0xb3, 0xf0,
			0x9b, 0xa3, 0xa6, 0xf1, 0xbb, 0x9b, 0xa7, 0xf1, 0x88, 0x9a, 0x81, 0xf4, 0x83, 0xa8, 0x83, 0xf2,
			0xb4, 0x88, 0xb7, 0xf1, 0xa0, 0xa2, 0xb6, 0xf3, 0x97, 0x88, 0xb7, 0xf2, 0x88, 0x83, 0xb8, 0xf0,
			0xa9, 0x9b, 0x9e, 0xf0, 0xa7, 0x92, 0xab, 0x5a
};
	const char mutf8Data[] = {
			0x41, 0xed, 0xa7, 0x98, 0xed, 0xb4, 0xaf, 0xed, 0xa2, 0xb0, 0xed, 0xbf, 0x9b, 0xee,
			0xb5, 0xa0, 0xed, 0xaf, 0x9b, 0xed, 0xbf, 0xae, 0xed, 0xa2, 0xba, 0xed, 0xb7, 0xac, 0xed, 0xaf,
			0x98, 0xed, 0xb1, 0x84, 0xed, 0xa2, 0x8d, 0xed, 0xb3, 0x99, 0xed, 0xaa, 0x99, 0xed, 0xb1, 0xae,
			0xed, 0xa8, 0x9b, 0xed, 0xba, 0xa9, 0xed, 0xa2, 0xb3, 0xed, 0xb6, 0x92, 0xed, 0xa3, 0xb1, 0xed,
			0xbc, 0xa4, 0xe4, 0x99, 0xad, 0xed, 0xaa, 0x8e, 0xed, 0xb4, 0x95, 0xed, 0xa1, 0x98, 0xed, 0xb5,
			0xab, 0xed, 0xa5, 0xab, 0xed, 0xbb, 0xa4, 0xed, 0xab, 0xb1, 0xed, 0xbe, 0xb8, 0xed, 0xa8, 0xa1,
			0xed, 0xbe, 0xb8, 0xed, 0xae, 0xa0, 0xed, 0xbc, 0xa5, 0xed, 0xab, 0x90, 0xed, 0xbb, 0x8a, 0xed,
			0xac, 0x91, 0xed, 0xb5, 0xb3, 0xed, 0xa0, 0xae, 0xed, 0xb3, 0xa6, 0xed, 0xa6, 0xad, 0xed, 0xbb,
			0xa7, 0xed, 0xa3, 0xa1, 0xed, 0xba, 0x81, 0xed, 0xaf, 0x8e, 0xed, 0xb8, 0x83, 0xed, 0xaa, 0x90,
			0xed, 0xb8, 0xb7, 0xed, 0xa5, 0x82, 0xed, 0xb2, 0xb6, 0xed, 0xac, 0x9c, 0xed, 0xb8, 0xb7, 0xed,
			0xa7, 0xa0, 0xed, 0xb3, 0xb8, 0xed, 0xa1, 0xa5, 0xed, 0xbb, 0x9e, 0xed, 0xa1, 0x9d, 0xed, 0xb2,
			0xab, 0x5a
	};
	I_32 utf8DataLength = sizeof(utf8Data); /* skip the terminating null */
	I_32 expectedStringLength = sizeof(mutf8Data);
	I_32 convertedStringLength = 0;

	reportTestEntry(portLibrary, testName);
	memset(outBuff, 0, sizeof(outBuff));
	convertedStringLength = j9str_convert(J9STR_CODE_UTF8, J9STR_CODE_MUTF8,
			utf8Data, utf8DataLength,  outBuff, sizeof(outBuff));
	if (convertedStringLength != expectedStringLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "buffer length wrong.  Expected %d actual %d\n", expectedStringLength, convertedStringLength);
	}
	if (!compareBytes(mutf8Data, outBuff, expectedStringLength)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Converted string wrong.");
	}
	/* test string length */
	convertedStringLength = j9str_convert(J9STR_CODE_UTF8, J9STR_CODE_MUTF8,
			utf8Data, utf8DataLength, NULL, 0);
	if (convertedStringLength != expectedStringLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "buffer length wrong.  Expected %d actual %d\n", expectedStringLength, convertedStringLength);
	}
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify string Unicode->MUTF8->Unicode conversion for all 16-bit code points
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_convRoundTrip(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char *testName = "j9str_convRoundTrip";
	U_32 unicodeEnd = 0x10000; /* overestimate */
	U_32 numCodePoints = 0;
	size_t bufferSize = (6 * unicodeEnd) + 2; /* loose upper bound */
	U_16 *unicodeData = j9mem_allocate_memory(unicodeEnd * 2, OMRMEM_CATEGORY_PORT_LIBRARY);
	U_8 * mutf8Buffer = j9mem_allocate_memory(bufferSize, OMRMEM_CATEGORY_PORT_LIBRARY);
	U_8 * unicodeResult = j9mem_allocate_memory(bufferSize, OMRMEM_CATEGORY_PORT_LIBRARY);
	U_32 i;
	U_16 codePoint = 1;
	I_32 convertedStringLength = 0;

	if ((NULL == unicodeData) || (NULL == mutf8Buffer) || (NULL == unicodeResult)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "could not allocate working buffers");
	}

	/* generate all 16-bit Unicode points */
	for (i = 0; i < unicodeEnd; ++i) {
		unicodeData[numCodePoints] = codePoint;
		++codePoint;
		++numCodePoints;
	}
	j9tty_printf(PORTLIB, "Testing %d code points\n", numCodePoints);

	convertedStringLength = j9str_convert(J9STR_CODE_WIDE, J9STR_CODE_MUTF8,
			(const char*) unicodeData, 2*numCodePoints, (char*)mutf8Buffer, bufferSize);
	if (convertedStringLength < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "unicode to MUTF8 test failed: %d", convertedStringLength);
	}
	j9tty_printf(PORTLIB, "mutf string length = %d\n", convertedStringLength);

	convertedStringLength = j9str_convert(J9STR_CODE_MUTF8, J9STR_CODE_WIDE,
			(const char*)mutf8Buffer, convertedStringLength, (char*)unicodeResult, bufferSize);
	if (convertedStringLength < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "MUTF8 to  unicode test failed: %d", convertedStringLength);
	}
	j9tty_printf(PORTLIB, "mutf string length = %d\n", convertedStringLength);
	if (!compareBytes((const char *) unicodeData, (const char*)unicodeResult, 2*numCodePoints)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Converted string wrong.");
	}
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify string Latin-1 -> MUTF8 conversion for all 8-bit code points
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_Latin1ToMutf8(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	char expectedMutf8[] = {0xc0, 0x80, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc,
			0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c,
			0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c,
			0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c,
			0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c,
			0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c,
			0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c,
			0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c,
			0x7d, 0x7e, 0x7f, 0xc2, 0x80, 0xc2, 0x81, 0xc2, 0x82, 0xc2, 0x83, 0xc2, 0x84, 0xc2, 0x85, 0xc2,
			0x86, 0xc2, 0x87, 0xc2, 0x88, 0xc2, 0x89, 0xc2, 0x8a, 0xc2, 0x8b, 0xc2, 0x8c, 0xc2, 0x8d, 0xc2,
			0x8e, 0xc2, 0x8f, 0xc2, 0x90, 0xc2, 0x91, 0xc2, 0x92, 0xc2, 0x93, 0xc2, 0x94, 0xc2, 0x95, 0xc2,
			0x96, 0xc2, 0x97, 0xc2, 0x98, 0xc2, 0x99, 0xc2, 0x9a, 0xc2, 0x9b, 0xc2, 0x9c, 0xc2, 0x9d, 0xc2,
			0x9e, 0xc2, 0x9f, 0xc2, 0xa0, 0xc2, 0xa1, 0xc2, 0xa2, 0xc2, 0xa3, 0xc2, 0xa4, 0xc2, 0xa5, 0xc2,
			0xa6, 0xc2, 0xa7, 0xc2, 0xa8, 0xc2, 0xa9, 0xc2, 0xaa, 0xc2, 0xab, 0xc2, 0xac, 0xc2, 0xad, 0xc2,
			0xae, 0xc2, 0xaf, 0xc2, 0xb0, 0xc2, 0xb1, 0xc2, 0xb2, 0xc2, 0xb3, 0xc2, 0xb4, 0xc2, 0xb5, 0xc2,
			0xb6, 0xc2, 0xb7, 0xc2, 0xb8, 0xc2, 0xb9, 0xc2, 0xba, 0xc2, 0xbb, 0xc2, 0xbc, 0xc2, 0xbd, 0xc2,
			0xbe, 0xc2, 0xbf, 0xc3, 0x80, 0xc3, 0x81, 0xc3, 0x82, 0xc3, 0x83, 0xc3, 0x84, 0xc3, 0x85, 0xc3,
			0x86, 0xc3, 0x87, 0xc3, 0x88, 0xc3, 0x89, 0xc3, 0x8a, 0xc3, 0x8b, 0xc3, 0x8c, 0xc3, 0x8d, 0xc3,
			0x8e, 0xc3, 0x8f, 0xc3, 0x90, 0xc3, 0x91, 0xc3, 0x92, 0xc3, 0x93, 0xc3, 0x94, 0xc3, 0x95, 0xc3,
			0x96, 0xc3, 0x97, 0xc3, 0x98, 0xc3, 0x99, 0xc3, 0x9a, 0xc3, 0x9b, 0xc3, 0x9c, 0xc3, 0x9d, 0xc3,
			0x9e, 0xc3, 0x9f, 0xc3, 0xa0, 0xc3, 0xa1, 0xc3, 0xa2, 0xc3, 0xa3, 0xc3, 0xa4, 0xc3, 0xa5, 0xc3,
			0xa6, 0xc3, 0xa7, 0xc3, 0xa8, 0xc3, 0xa9, 0xc3, 0xaa, 0xc3, 0xab, 0xc3, 0xac, 0xc3, 0xad, 0xc3,
			0xae, 0xc3, 0xaf, 0xc3, 0xb0, 0xc3, 0xb1, 0xc3, 0xb2, 0xc3, 0xb3, 0xc3, 0xb4, 0xc3, 0xb5, 0xc3,
			0xb6, 0xc3, 0xb7, 0xc3, 0xb8, 0xc3, 0xb9, 0xc3, 0xba, 0xc3, 0xbb, 0xc3, 0xbc, 0xc3, 0xbd, 0xc3,
			0xbe, 0xc3, 0xbf, 0xc0, 0x80, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};
	const char *testName = "j9str_Latin1ToMutf8";
	char outBuff[TEST_BUF_LEN];
	char inBuff[264]; /* ensure we overflow the port library's temporary buffer */
	U_32 i = 0;
	I_32 originalStringLength = sizeof(inBuff);
	I_32 expectedStringLength = sizeof(expectedMutf8);
	I_32 convertedStringLength = 0;

	reportTestEntry(portLibrary, testName);
	memset(outBuff, 0, sizeof(outBuff));
	for (i = 0; i < sizeof(inBuff); ++i) {
		inBuff[i] = (char) i; /* create all Latin-1 code points */
	}
	convertedStringLength = j9str_convert(J9STR_CODE_LATIN1, J9STR_CODE_MUTF8,
			inBuff, originalStringLength,  outBuff, sizeof(outBuff));
	if (convertedStringLength != expectedStringLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "buffer length wrong.  Expected %d actual %d\n", expectedStringLength, convertedStringLength);
	}
	if (!compareBytes(expectedMutf8, outBuff, expectedStringLength)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Converted string wrong.");
	}
	/* test string length */
	convertedStringLength = j9str_convert(J9STR_CODE_LATIN1, J9STR_CODE_MUTF8,
			inBuff, originalStringLength, NULL, 0);
	if (convertedStringLength != expectedStringLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "buffer length wrong.  Expected %d actual %d\n", expectedStringLength, convertedStringLength);
	}
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify string Latin-1 -> MUTF8 conversion for all 8-bit code points
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9str_WinacpToMutf8(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	char winacpData[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8,
			0x81, 0x91, 0xa1, 0xb1, 0xc1, 0xd1, 0xe1, 0xf1};
	char expectedMutf8[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8,
			0xc2, 0x81, 0xe2, 0x80, 0x98, 0xc2, 0xa1, 0xc2, 0xb1, 0xc3, 0x81, 0xc3, 0x91, 0xc3, 0xa1, 0xc3, 0xb1};
	const char *testName = "j9str_WinacpToMutf8";
	char outBuff[TEST_BUF_LEN];
	U_32 i = 0;
	I_32 originalStringLength = sizeof(winacpData);
	I_32 expectedStringLength = sizeof(expectedMutf8);
	I_32 convertedStringLength = 0;

	reportTestEntry(portLibrary, testName);
#if defined(WIN32)
	{
		U_32 defaultACP = GetACP();
		if (1252 != defaultACP) {
			j9tty_printf(PORTLIB, "Default ANSI code page = %d, expecting 1252.  Skipping test.\n", defaultACP);
			return reportTestExit(portLibrary, testName);
		}
	}
#endif
	memset(outBuff, 0, sizeof(outBuff));
	convertedStringLength = j9str_convert(J9STR_CODE_WINDEFAULTACP, J9STR_CODE_MUTF8,
			winacpData, originalStringLength,  outBuff, sizeof(outBuff));
#if defined(WIN32)
	if (convertedStringLength != expectedStringLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "buffer length wrong.  Expected %d actual %d\n", expectedStringLength, convertedStringLength);
	}
	if (!compareBytes(expectedMutf8, outBuff, expectedStringLength)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Converted string wrong.");
	}
#else
	if (J9PORT_ERROR_STRING_UNSUPPORTED_ENCODING != convertedStringLength) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to detect invalid conversion");
	}
#endif
	return reportTestExit(portLibrary, testName);
}

#if defined(J9ZOS390)
/**
 * Check whether a null/non-null terminated string is properly handled by fstring().
 * It calls atoe_vsnprintf() to verify the functionalities.
 * @param[in] portLibrary - The port library under test
 * @param[in] precision - flag for precision specifier
 * @param[in] buffer - pointer to the buffer intended for atoe_vsnprintf()
 * @param[in] bufferLength - the buffer length
 * @param[in] expectedOutput - The expected string
 * @param[in] format - The string format
 *
 * @return TEST_PASS on success
 *         TEST_FAIL on error
 */
static int
printToBuffer(struct J9PortLibrary *portLibrary, BOOLEAN precision, char *buffer, size_t bufferLength, const char *expectedOutput, const char *format, ...)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int stringLength = 0;
	va_list args;

	va_start(args, format);
	stringLength = atoe_vsnprintf(buffer, bufferLength, format, args);
	va_end (args);

	if (stringLength >= 0) {
		if (precision) {
			j9tty_printf(PORTLIB, "\n\tFinish Testing: String in buffer is: \"%s\", length = %d\n", buffer, stringLength);
		} else {
			j9tty_printf(PORTLIB, "\n\tFinish Testing without precision specifier: String in buffer is: \"%s\", length = %d\n", buffer, stringLength);
		}

		if ((stringLength == strlen(expectedOutput))
			&& (0 == memcmp(buffer, expectedOutput, stringLength))
		) {
			j9tty_printf(PORTLIB, "\n\tComparing against the expected output: PASSED.\n");
			return TEST_PASS;
		} else {
			j9tty_printf(PORTLIB, "\n\tComparing against the expected output: FAILED. Expected string: \"%s\", length = %d\n", expectedOutput, strlen(expectedOutput));
			return TEST_FAIL;
		}
	} else {
		j9tty_printf(PORTLIB, "\n\tComparing against the expected output: FAILED. stringLength < 0, Expected string: \"%s\", length = %d\n", expectedOutput, strlen(expectedOutput));
	}
}

/**
 * Verify that null/non-null terminated strings are properly handled in fstring().
 * It actually calls invoke atoe_vsnprintf() to run the tests through sub-functions
 * @param[in] portLibrary - The port library under test
 *
 * @return TEST_PASS on success
 *         TEST_FAIL on error
 */
static int
j9str_test_atoe_vsnprintf(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char *testName = "j9str_test_atoe_vsnprintf";
	const char expectedOutput1[] = {'T','E','S','T','\0'};
	const char expectedOutput2[] = {'T','E','S','\0'};
	const char expectedOutput3[] = {' ',' ',' ',' ',' ',' ','T','E','S','T','\0'};
	const char expectedOutput4[] = {'T','E', '\0'};
	const char nullTerminatedString[] = {'T','E','S','T','\0'};
	const char nonNullTerminatedString[] = {'T','E','S','T'};
	const size_t bufferLength0 = 50;
	const size_t bufferLength1 = strlen(expectedOutput1) + 1;
	const size_t bufferLength2 = strlen(expectedOutput2) + 1;
	const size_t bufferLength3 = strlen(expectedOutput3) + 1;
	char buffer0[bufferLength0];
	char buffer1[bufferLength1];
	char buffer2[bufferLength2];
	char buffer3[bufferLength3];
	int rc = TEST_PASS;

	reportTestEntry(portLibrary, testName);

	/* Neither min_width nor precision is specified for a null terminated input string*/
	j9tty_printf(PORTLIB, "\n\tTesting case 1\n");
	rc |= printToBuffer(portLibrary, TRUE, buffer0, bufferLength0, expectedOutput1, "%s", nullTerminatedString);

	/* min_width is less than the length of inputString */
	j9tty_printf(PORTLIB, "\n\tTesting case 2\n");
	rc |= printToBuffer(portLibrary, FALSE, buffer2, bufferLength2, expectedOutput2, "%*s", 2, nonNullTerminatedString);

	/* min_width is equal to the length of inputString (buffer length > min_width) */
	j9tty_printf(PORTLIB, "\n\tTesting case 3\n");
	rc |= printToBuffer(portLibrary, FALSE, buffer1, bufferLength1, expectedOutput1, "%*s", 4, nullTerminatedString);

	/* min_width is greater than the length of inputString (buffer length > min_width) */
	j9tty_printf(PORTLIB, "\n\tTesting case 4\n");
	rc |= printToBuffer(portLibrary, FALSE, buffer3, bufferLength3, expectedOutput3, "%*s", 10, nullTerminatedString);

	/* precision is equal to the length of inputString */
	j9tty_printf(PORTLIB, "\n\tTesting case 5\n");
	rc |= printToBuffer(portLibrary, TRUE, buffer0, bufferLength0, expectedOutput1, "%.*s", 4, nonNullTerminatedString);

	/* precision is less than the length of inputString */
	j9tty_printf(PORTLIB, "\n\tTesting case 6\n");
	rc |= printToBuffer(portLibrary, TRUE, buffer0, bufferLength0, expectedOutput4, "%.*s", 2, nonNullTerminatedString);

	/* both min_width and precision are equal to the length of inputString */
	j9tty_printf(PORTLIB, "\n\tTesting case 7\n");
	rc |= printToBuffer(portLibrary, TRUE, buffer0, bufferLength0, expectedOutput1, "%*.*s", 4, 4, nonNullTerminatedString);

	/* the length of inputString is equal to precision but less than min_width */
	j9tty_printf(PORTLIB, "\n\tTesting case 8\n");
	rc |= printToBuffer(portLibrary, TRUE, buffer0, bufferLength0, expectedOutput3, "%*.*s", 10, 4, nonNullTerminatedString);

	if (TEST_PASS != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\n\tTEST FAILED.\n");
	}

	j9tty_printf(PORTLIB, "\n");
	return reportTestExit(portLibrary, testName);
}
#endif /* defined(J9ZOS390) */

/**
 * Verify port library string operations.
 * 
 * Exercise all API related to port library string operations found in
 * @ref j9str.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return 0 on success, -1 on failure
 */
I_32 
j9str_runTests(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc;

	/* Display unit under test */
	HEADING(portLibrary, "String test");

	/* verify sanity of port library for these tests.  If this fails there is no
	 * point in continuing
	 */
	rc = j9str_test0(portLibrary);
	if (TEST_PASS == rc) {
		rc |= j9str_convPlatToU8(portLibrary);
		rc |= j9str_convU8ToPlat(portLibrary);
		rc |= j9str_convU8ToPlat_Null(portLibrary);
		rc |= j9str_convU8ToWide_Null(portLibrary);
		rc |= j9str_convWideToU8_Null(portLibrary);
		rc |= j9str_convWideToU8(portLibrary);
		rc |= j9str_convU8ToWide(portLibrary);
		rc |= j9str_convUtf8ToMUtf8(portLibrary);
		rc |= j9str_convRandomUtf8ToMUtf8(portLibrary);
		rc |= j9str_convRoundTrip(portLibrary);
		rc |= j9str_Latin1ToMutf8(portLibrary);
		rc |= j9str_convLongString(portLibrary);
		rc |= j9str_WinacpToMutf8(portLibrary);
		rc |= j9str_test1(portLibrary);
		rc |= j9str_test2(portLibrary);
		rc |= j9str_test3(portLibrary);
		rc |= j9str_test4(portLibrary);
		rc |= j9str_test5(portLibrary);
		rc |= j9str_test6(portLibrary);
#if defined(J9ZOS390)
		rc |= j9str_test_atoe_vsnprintf(portLibrary);
#endif /* defined(J9ZOS390) */
	}

	/* Output results */
	j9tty_printf(PORTLIB, "\nString test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;
}
