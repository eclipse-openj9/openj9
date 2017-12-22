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

#include <stdarg.h>

#include "ctest.h"

static void test2 (va_list bar);
static void test3 (va_list* args);
static void test (int arg1, ...);


void verifyVAList(void) {
	PORT_ACCESS_FROM_PORT(cTestPortLib);
	j9tty_printf(PORTLIB, "Verifying va_list\n");

	test(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);	

}



static void test(int arg1, ...) {
	va_list myList;
	va_start(myList, arg1);
	test2(myList);
	va_end(myList);
}


static void test2(va_list bar) {
	/* some platforms (like most GNU PPC platforms) use an array for va_lists.
	 * Ensure that these platforms define VA_PTR correctly.
	 * If they don't, then this test will crash or fail
	 */
	test3(VA_PTR(bar));
}


static void test3 (va_list* args) {
	PORT_ACCESS_FROM_PORT(cTestPortLib);
	j9_assume(va_arg(*args, int), 1);
	j9_assume(va_arg(*args, int), 2);
	j9_assume(va_arg(*args, int), 3);
	j9_assume(va_arg(*args, int), 4);
	j9_assume(va_arg(*args, int), 5);
	j9_assume(va_arg(*args, int), 6);
	j9_assume(va_arg(*args, int), 7);
	j9_assume(va_arg(*args, int), 8);
	j9_assume(va_arg(*args, int), 9);
	j9_assume(va_arg(*args, int), 10);
}
