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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "j9.h"
#include "rommeth.h"
#include "ut_map.h"

/*
 * 
 * Loop on all the methods in a class.
 * Calculate the conservative maximum mapping buffer required by stackmap/localmap/debuglocalmap.
 * Update the global buffer size if necessary. 
 * 
 */

UDATA
j9maxmap_setMapMemoryBuffer(J9JavaVM * vm, J9ROMClass * romClass) 
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	UDATA i;
	J9ROMMethod *romMethod;
	UDATA branches = (UDATA) romClass->maxBranchCount;
	UDATA stackmapSize;
	UDATA localmapSize;
	UDATA debuglocalmapSize;
	UDATA maxSize = vm->mapMemoryBufferSize;
	UDATA tempMaxSize;
	U_8 * newBuffer;
	UDATA result = 0;
#define MAX_POINTER_SIZE 8
	
	if (vm->mapMemoryBufferSize != 0) {
		/* Only look to growing the buffer if it already exists */
		romMethod = (J9ROMMethod *) J9ROMCLASS_ROMMETHODS(romClass);

		/* For each method in the class, find the largest map needed */
		for (i = 0; i < (UDATA) romClass->romMethodCount; i++) {

			/* If native or abstract method, do nothing */
			if (!((romMethod->modifiers & J9AccNative) || (romMethod->modifiers & J9AccAbstract))) {
				UDATA maxStack = (UDATA) J9_MAX_STACK_FROM_ROM_METHOD(romMethod);
				UDATA length = (UDATA) J9_ROUNDED_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
				
				stackmapSize = (((maxStack + 2) * branches) * MAX_POINTER_SIZE) + length;
				localmapSize = (branches * 2 * sizeof(U_32)) + (length * sizeof(U_32));
				debuglocalmapSize = ((branches + 2) * sizeof(U_32)) + (length * sizeof(U_32)) + length;
				
				tempMaxSize = (localmapSize > stackmapSize) ? localmapSize : stackmapSize;
				tempMaxSize = (tempMaxSize > debuglocalmapSize) ? tempMaxSize : debuglocalmapSize;
				tempMaxSize += MAP_MEMORY_RESULTS_BUFFER_SIZE;

				if (tempMaxSize > maxSize) {
					maxSize = tempMaxSize;
					Trc_Map_j9maxmap_setMapMemoryBuffer_Larger_Method(tempMaxSize, 
							(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)),
							(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_GET_NAME(romClass, romMethod)), J9UTF8_DATA(J9ROMMETHOD_GET_NAME(NULL,romMethod)),
							(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_GET_SIGNATURE(romClass, romMethod)), J9UTF8_DATA(J9ROMMETHOD_GET_SIGNATURE(NULL,romMethod)));
#if 0
					printf("[][][] maxStack = %d, maxBranchCount = %d, length = %d\n", maxStack, romClass->maxBranchCount, length);
					printf("[][][] new unrounded maxSize = %d from stk = %d, lcl = %d, dbg = %d\n", tempMaxSize, stackmapSize + 8192, localmapSize + 8192, debuglocalmapSize + 8192);
					printf("[][][] Class = %.*s\n", (UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)));
					printf("[][][] Method[%d] = %.*s\n", i, (UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)), J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)));
#endif
				}
			}
			romMethod = J9_NEXT_ROM_METHOD(romMethod);
		}
		
		if (maxSize > vm->mapMemoryBufferSize) {
			/* Round up maxSize to the nearest 4K - possible thrash reduction */
			maxSize += (4 * 1024);
			maxSize = maxSize & ~((UDATA) ((4 * 1024) - 1));
			omrthread_monitor_enter(vm->mapMemoryBufferMutex);
			/* Confirm the size is still larger while in the mutex - allows later parallelization */
			if (maxSize > vm->mapMemoryBufferSize) {
				newBuffer = (U_8 *) j9mem_allocate_memory(maxSize, OMRMEM_CATEGORY_VM);
				if (newBuffer != NULL) {
					j9mem_free_memory(vm->mapMemoryResultsBuffer);
					vm->mapMemoryBufferSize = maxSize;
					vm->mapMemoryResultsBuffer = newBuffer;
					vm->mapMemoryBuffer = newBuffer + MAP_MEMORY_RESULTS_BUFFER_SIZE;
				} else {
					/* Return failure - VM will reject class at RAMClass creation time */
					Trc_Map_j9maxmap_setMapMemoryBuffer_AllocationFailure(maxSize);
					result = 1;
				}
			}
			omrthread_monitor_exit(vm->mapMemoryBufferMutex);
		}
	}
	return result;
}
