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

#include "j9port.h"
#include "j9.h"

#include "jnitest_internal.h"
#include <string.h>
#define OVERRUN_UNDERRUN_TEST_BLOCK_SIZE_BYTES 16

/**
 * The block that isn't being freed should be detected.
 *
*/
void JNICALL 
Java_j9vm_test_memchk_NoFree_test(JNIEnv * env, jclass clazz)
{
	char * charArray;
	PORT_ACCESS_FROM_ENV(env);

	charArray = j9mem_allocate_memory(11, OMRMEM_CATEGORY_VM);
    strcpy(charArray, "nofree");
}

void JNICALL 
Java_j9vm_test_memchk_BlockOverrun_test(JNIEnv * env, jclass clazz)
{
	char * charArray;
	PORT_ACCESS_FROM_ENV(env);

	/* TODO - for mprotect, install a handler here that checks that we failed because we tried to write to a locked page */ 
	charArray = j9mem_allocate_memory(OVERRUN_UNDERRUN_TEST_BLOCK_SIZE_BYTES, OMRMEM_CATEGORY_VM);
	j9tty_err_printf(((J9VMThread *) (env))->javaVM->portLibrary, "j9mem_allocate_memory(%i) returned %p\n", OVERRUN_UNDERRUN_TEST_BLOCK_SIZE_BYTES, charArray);
    strcpy(charArray, "overrun");
	charArray[16] = 'b';
	charArray[17] = 'e';
	charArray[18] = 'r';
	charArray[19] = 'z';
	charArray[20] = 'e';
	charArray[21] = 'r';
	charArray[22] = 'k';
	charArray[23] = 'e';
	charArray[24] = 'r';
}

void JNICALL 
Java_j9vm_test_memchk_BlockUnderrun_test(JNIEnv * env, jclass clazz)
{
	char * charArray;
	PORT_ACCESS_FROM_ENV(env);

	charArray = j9mem_allocate_memory(OVERRUN_UNDERRUN_TEST_BLOCK_SIZE_BYTES, OMRMEM_CATEGORY_VM);
	j9tty_err_printf(((J9VMThread *) (env))->javaVM->portLibrary, "j9mem_allocate_memory(%i) returned %p\n", OVERRUN_UNDERRUN_TEST_BLOCK_SIZE_BYTES, charArray);
    strcpy(charArray, "underrun");
	charArray[-9] = 'b';
	charArray[-8] = 'e';
	charArray[-7] = 'r';
	charArray[-6] = 'z';
	charArray[-5] = 'e';
	charArray[-4] = 'r';
	charArray[-3] = 'k';
	charArray[-2] = 'e';
	charArray[-1] = 'r';
}

void JNICALL 
Java_j9vm_test_memchk_Generic_test(JNIEnv * env, jclass clazz)
{
	char * charArray;
	PORT_ACCESS_FROM_ENV(env);

	charArray = j9mem_allocate_memory(16, OMRMEM_CATEGORY_VM);
    strcpy(charArray, "generic");
	j9mem_free_memory(charArray);
}
