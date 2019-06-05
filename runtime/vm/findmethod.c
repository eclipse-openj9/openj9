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

#include "j9.h"
#include "j9protos.h"
#include "rommeth.h"
#include "vm_internal.h"


J9ROMClass * 
findROMClassInSegment(J9VMThread *vmThread, J9MemorySegment *memorySegment, UDATA methodPC)
{
	UDATA currentClass = (UDATA) memorySegment->heapBase;
	UDATA usedEnd = (UDATA) memorySegment->heapAlloc;

	/* linear walk of the segment until we find the class that contains methodPC */

	while (currentClass < usedEnd) {
		UDATA currentSize = ((J9ROMClass *) currentClass)->romSize;
		UDATA nextClass = currentClass + currentSize;

		if ((methodPC >= currentClass) && (methodPC < nextClass)) {
			return (J9ROMClass *) currentClass;
		}

		currentClass = nextClass;
	}

	return NULL;
}

J9ROMMethod * 
findROMMethodInROMClass(J9VMThread *vmThread, J9ROMClass *romClass, UDATA methodPC)
{
	J9ROMMethod *currentMethod = J9ROMCLASS_ROMMETHODS(romClass);
	U_32 i;

	/* walk the romClass and find the method */

	for (i = 0; i < romClass->romMethodCount; i++) {
		if ((methodPC >= (UDATA)currentMethod) && (methodPC < (UDATA)J9_BYTECODE_END_FROM_ROM_METHOD(currentMethod))) {
			 /* found the method */
			return currentMethod;
		}
		currentMethod = nextROMMethod(currentMethod);
	}

	return NULL;
}

J9ROMClass * 
findROMClassFromPC(J9VMThread *vmThread, UDATA methodPC, J9ClassLoader **resultClassLoader)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	J9MemorySegment *segmentForClass;
	J9ROMClass *romClass = NULL;

	omrthread_monitor_enter(javaVM->classTableMutex);
	omrthread_monitor_enter(javaVM->classMemorySegments->segmentMutex);

	segmentForClass = findMemorySegment(javaVM, javaVM->classMemorySegments, methodPC);
	if (segmentForClass != NULL && (segmentForClass->type & MEMORY_TYPE_ROM_CLASS) != 0) {
		romClass = findROMClassInSegment(vmThread, segmentForClass, methodPC);
		*resultClassLoader = segmentForClass->classLoader;
	}

	omrthread_monitor_exit(javaVM->classMemorySegments->segmentMutex);
	omrthread_monitor_exit(javaVM->classTableMutex);

	return romClass;
}

