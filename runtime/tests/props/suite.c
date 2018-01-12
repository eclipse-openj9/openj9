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
#include "CuTest.h"
#include <string.h>
#include "j9.h"
#include "util_api.h"

#define TESTDATA(x) "./props_test/" x

extern J9PortLibrary* g_PortLibrary;

void Test_Get_00(CuTest *tc) {
	PORT_ACCESS_FROM_PORT(g_PortLibrary);
	const char* val1;
	const char* val2;
	const char* val3;
	
	j9props_file_t f = props_file_open(PORTLIB, TESTDATA("well-formed.props"), NULL, 0);
	CuAssertPtrNotNull(tc, f);

	val1 = props_file_get(f, "foo");
	CuAssertPtrNotNull(tc, val1);

	val2 = props_file_get(f, "answer");
	CuAssertPtrNotNull(tc, val2);
	
	val3 = props_file_get(f, "this");
	CuAssertPtrNotNull(tc, val3);

	CuAssertTrue(tc, strcmp(val1,"bar") == 0);
	CuAssertTrue(tc, strcmp(val2,"42") == 0);
	CuAssertTrue(tc, strcmp(val3,"is	a line continuation") == 0);

	props_file_close(f);
}

BOOLEAN sawFoo = FALSE;
BOOLEAN sawAnswer = FALSE;

BOOLEAN 
walk_all_iterator(j9props_file_t file, const char* key, const char* value, void* userData)
{
	CuTest *tc = (CuTest*)userData;
	
	if (!strcmp(key,"foo")) {
		sawFoo = TRUE;
		CuAssertTrue(tc, strcmp(value,"bar") == 0);
	}
	if (!strcmp(key,"answer")) {
		sawAnswer = TRUE;
		CuAssertTrue(tc, strcmp(value,"42") == 0);
	}
	return TRUE;
}


void Test_Iterate_00(CuTest *tc) {
	PORT_ACCESS_FROM_PORT(g_PortLibrary);
	
	j9props_file_t f = props_file_open(PORTLIB, TESTDATA("well-formed.props"), NULL, 0);
	CuAssertPtrNotNull(tc, f);

	props_file_do(f, walk_all_iterator, tc);	
	CuAssertTrue(tc, sawFoo);
	CuAssertTrue(tc, sawAnswer);

	props_file_close(f);
}



CuSuite* 
GetTestSuite(void) 
{
	CuSuite* suite = CuSuiteNew();
	SUITE_ADD_TEST(suite, Test_Get_00);
	SUITE_ADD_TEST(suite, Test_Iterate_00);
	return suite;
}
