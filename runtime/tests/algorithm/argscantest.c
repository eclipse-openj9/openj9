/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#include <string.h>
#include "j9protos.h"
#include "j9port.h"

static void verify_scan_udata (J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount);
static void verify_scan_idata (J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount);


I_32 
verifyArgScan(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount) 
{
	PORT_ACCESS_FROM_PORT(portLib);

	j9tty_printf(PORTLIB, "Testing scan_ functions...\n");

	verify_scan_udata(portLib, passCount, failCount);

	verify_scan_idata(portLib, passCount, failCount);

	j9tty_printf(PORTLIB, "Finished testing scan_ functions.\n");

	return 0;
}


static void 
verify_scan_udata(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount) 
{
	UDATA rc = 0;
	char* cursor;
	char* start;
	UDATA result;
	PORT_ACCESS_FROM_PORT(portLib);

	start = cursor = "2147483647";
	rc = scan_udata(&cursor, &result);
	if (rc != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected error parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} else if (result != (UDATA)I_32_MAX) {
		j9tty_err_printf(PORTLIB, "Unexpected result parsing \"%s\": %zd\n", start, result);
		(*failCount)++;
	} else if (cursor != start + strlen(start)) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "4294967295 ";
	rc = scan_udata(&cursor, &result);
	if (rc != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected error parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} else if (result != U_32_MAX) {
		j9tty_err_printf(PORTLIB, "Unexpected result parsing \"%s\": %zu\n", start, result);
		(*failCount)++;
	} else if (cursor != start + strlen(start) - 1) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "0";
	rc = scan_udata(&cursor, &result);
	if (rc != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected error parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} else if (result != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected result parsing \"%s\": %zu\n", start, result);
		(*failCount)++;
	} else if (cursor != start + strlen(start)) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "0000";
	rc = scan_udata(&cursor, &result);
	if (rc != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected error parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} else if (result != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected result parsing \"%s\": %zu\n", start, result);
		(*failCount)++;
	} else if (cursor != start + strlen(start)) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "";
	rc = scan_udata(&cursor, &result);
	if (rc != 1) {
		j9tty_err_printf(PORTLIB, "Unexpected rc parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} if (cursor != start) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

#ifdef J9VM_ENV_DATA64
	start = cursor = "18446744073709551615";
	rc = scan_udata(&cursor, &result);
	if (rc != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected error parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} else if (result != U_64_MAX) {
		j9tty_err_printf(PORTLIB, "Unexpected result parsing \"%s\": %zu\n", start, result);
		(*failCount)++;
	} else if (cursor != start + strlen(start)) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "18446744073709551615 ";
	rc = scan_udata(&cursor, &result);
	if (rc != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected error parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} else if (result != U_64_MAX) {
		j9tty_err_printf(PORTLIB, "Unexpected result parsing \"%s\": %zu\n", start, result);
		(*failCount)++;
	} else if (cursor != start + strlen(start) - 1) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "18446744073709551616";
	rc = scan_udata(&cursor, &result);
	if (rc != 2) {
		j9tty_err_printf(PORTLIB, "Unexpected rc parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} if (cursor != start) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}
	start = cursor = "111111111111111111111";
	rc = scan_udata(&cursor, &result);
	if (rc != 2) {
		j9tty_err_printf(PORTLIB, "Unexpected rc parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} if (cursor != start) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}
#else
	start = cursor = "4294967296";
	rc = scan_udata(&cursor, &result);
	if (rc != 2) {
		j9tty_err_printf(PORTLIB, "Unexpected rc parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} if (cursor != start) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}
	start = cursor = "11111111111";
	rc = scan_udata(&cursor, &result);
	if (rc != 2) {
		j9tty_err_printf(PORTLIB, "Unexpected rc parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} if (cursor != start) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}
#endif

}


static void 
verify_scan_idata(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount) 
{
	UDATA rc = 0;
	char* cursor;
	char* start;
	IDATA result;
	PORT_ACCESS_FROM_PORT(portLib);

	start = cursor = "2147483647";
	rc = scan_idata(&cursor, &result);
	if (rc != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected error parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} else if (result != I_32_MAX) {
		j9tty_err_printf(PORTLIB, "Unexpected result parsing \"%s\": %zd\n", start, result);
		(*failCount)++;
	} else if (cursor != start + strlen(start)) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "2147483647 ";
	rc = scan_idata(&cursor, &result);
	if (rc != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected error parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} else if (result != I_32_MAX) {
		j9tty_err_printf(PORTLIB, "Unexpected result parsing \"%s\": %zd\n", start, result);
		(*failCount)++;
	} else if (cursor != start + strlen(start) - 1) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "0";
	rc = scan_idata(&cursor, &result);
	if (rc != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected error parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} else if (result != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected result parsing \"%s\": %zu\n", start, result);
		(*failCount)++;
	} else if (cursor != start + strlen(start)) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "0000";
	rc = scan_idata(&cursor, &result);
	if (rc != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected error parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} else if (result != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected result parsing \"%s\": %zu\n", start, result);
		(*failCount)++;
	} else if (cursor != start + strlen(start)) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "";
	rc = scan_idata(&cursor, &result);
	if (rc != 1) {
		j9tty_err_printf(PORTLIB, "Unexpected rc parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} if (cursor != start) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "-";
	rc = scan_idata(&cursor, &result);
	if (rc != 1) {
		j9tty_err_printf(PORTLIB, "Unexpected rc parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} if (cursor != start) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "+";
	rc = scan_idata(&cursor, &result);
	if (rc != 1) {
		j9tty_err_printf(PORTLIB, "Unexpected rc parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} if (cursor != start) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "--0";
	rc = scan_idata(&cursor, &result);
	if (rc != 1) {
		j9tty_err_printf(PORTLIB, "Unexpected rc parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} if (cursor != start) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "++0";
	rc = scan_idata(&cursor, &result);
	if (rc != 1) {
		j9tty_err_printf(PORTLIB, "Unexpected rc parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} if (cursor != start) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "-0";
	rc = scan_idata(&cursor, &result);
	if (rc != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected error parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} else if (result != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected result parsing \"%s\": %zu\n", start, result);
		(*failCount)++;
	} else if (cursor != start + strlen(start)) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "+0";
	rc = scan_idata(&cursor, &result);
	if (rc != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected error parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} else if (result != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected result parsing \"%s\": %zu\n", start, result);
		(*failCount)++;
	} else if (cursor != start + strlen(start)) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "-2147483647";
	rc = scan_idata(&cursor, &result);
	if (rc != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected error parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} else if (result != -I_32_MAX) {
		j9tty_err_printf(PORTLIB, "Unexpected result parsing \"%s\": %zd\n", start, result);
		(*failCount)++;
	} else if (cursor != start + strlen(start)) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "-2147483648";
	rc = scan_idata(&cursor, &result);
	if (rc != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected error parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} else if (result != I_32_MIN) {
		j9tty_err_printf(PORTLIB, "Unexpected result parsing \"%s\": %zd\n", start, result);
		(*failCount)++;
	} else if (cursor != start + strlen(start)) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

#ifdef J9VM_ENV_DATA64
	start = cursor = "9223372036854775807";
	rc = scan_idata(&cursor, &result);
	if (rc != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected error parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} else if (result != I_64_MAX) {
		j9tty_err_printf(PORTLIB, "Unexpected result parsing \"%s\": %zd\n", start, result);
		(*failCount)++;
	} else if (cursor != start + strlen(start)) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "-9223372036854775808";
	rc = scan_idata(&cursor, &result);
	if (rc != 0) {
		j9tty_err_printf(PORTLIB, "Unexpected error parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} else if (result != I_64_MIN) {
		j9tty_err_printf(PORTLIB, "Unexpected result parsing \"%s\": %zd\n", start, result);
		(*failCount)++;
	} else if (cursor != start + strlen(start)) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "9223372036854775808";
	rc = scan_idata(&cursor, &result);
	if (rc != 2) {
		j9tty_err_printf(PORTLIB, "Unexpected rc parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} if (cursor != start) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "-9223372036854775809";
	rc = scan_idata(&cursor, &result);
	if (rc != 2) {
		j9tty_err_printf(PORTLIB, "Unexpected rc parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} if (cursor != start) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}
#else
	start = cursor = "2147483648";
	rc = scan_idata(&cursor, &result);
	if (rc != 2) {
		j9tty_err_printf(PORTLIB, "Unexpected rc parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} if (cursor != start) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	start = cursor = "-2147483649";
	rc = scan_idata(&cursor, &result);
	if (rc != 2) {
		j9tty_err_printf(PORTLIB, "Unexpected rc parsing \"%s\": %d\n", start, rc);
		(*failCount)++;
	} if (cursor != start) {
		j9tty_err_printf(PORTLIB, "Unexpected length parsing \"%s\": %zu\n", start, cursor - start);
		(*failCount)++;
	} else {
		(*passCount)++;
	}
#endif

}



