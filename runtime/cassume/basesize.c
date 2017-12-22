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

#include "ctest.h"

void verifyJNISizes(void) {
	PORT_ACCESS_FROM_PORT(cTestPortLib);
	j9tty_printf(PORTLIB, "Verifying JNI Sizes\n");
	j9_assume(sizeof(jbyte), 1);
	j9_assume(sizeof(jchar), 2);
	j9_assume(sizeof(jshort), 2);
	j9_assume(sizeof(jint), 4);
	j9_assume(sizeof(jlong), 8);
	j9_assume(sizeof(jfloat), 4);
	j9_assume(sizeof(jdouble), 8);
}



void 
verifyUDATASizes(void) 
{
	PORT_ACCESS_FROM_PORT(cTestPortLib);
	/* 64 bit are a bit harder to test since we can't encode the constants easily without causing warnings */
	U_64 testU64Max = ((U_64)0xffffffff) << 32 | (U_64)0xffffffff;
	I_64 testI64Max = (I_64)(((U_64)0x7fffffff) << 32 | (U_64)0xffffffff);
	I_64 testI64Min = (I_64)(((U_64)0x80000000) << 32 | (U_64)0x00000000);
	U_32 testU32Max = 0xffffffff;
	I_32 testI32Max = 0x7fffffff;
	I_32 testI32Min = 0x80000000;
	
	j9tty_printf(PORTLIB, "Verifying VM Type Sizes\n");
	/* check VM type sizes */
	j9_assume(sizeof(U_8), 1);
	j9_assume(sizeof(I_8), 1);
	j9_assume(sizeof(U_16), 2);
	j9_assume(sizeof(I_16), 2);
	j9_assume(sizeof(U_32), 4);
	j9_assume(sizeof(I_32), 4);
	j9_assume(sizeof(U_64), 8);
	j9_assume(sizeof(I_64), 8);
	j9_assume(sizeof(UDATA), sizeof(void*));
	j9_assume(sizeof(IDATA), sizeof(void*));

	/* the VM j9_assumes that a fn pointer can be stored in a void pointer */
	j9_assume(sizeof(void*), sizeof( void(*)(void) ));
	
	/* check constants */
	j9_assume(U_8_MAX, 0xff);
	j9_assume(I_8_MAX, 0x7f);
	j9_assume(I_8_MIN, 0x80);

	j9_assume(U_16_MAX, 0xffff);
	j9_assume(I_16_MAX, 0x7fff);
	j9_assume(I_16_MIN, 0x8000);

	j9_assume(U_32_MAX, testU32Max);
	j9_assume(I_32_MAX, testI32Max);
	j9_assume(I_32_MIN, testI32Min);
	
	j9_assume(U_64_MAX, testU64Max);
	j9_assume(I_64_MAX, testI64Max);
	j9_assume(I_64_MIN, testI64Min);
}
