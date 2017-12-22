/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

/**
 * @file
 * @ingroup VMChk
 */

#include "j9.h"
#include "j9port.h"
#include "vmcheck.h"

/*
 *	J9ROMClass sanity:
 *		SRP check:
 *			Ensure J9ROMClass->interfaces SRP is in the same segment if J9ROMClass->interfaceCount != 0.
 *			Ensure J9ROMClass->romMethods SRP is in the same segment if J9ROMClass->romMethodCount != 0.
 *			Ensure J9ROMClass->romFields SRP is in the same segment if J9ROMClass->romFieldCount != 0.
 *			Ensure J9ROMClass->innerClasses SRP is in the same segment if J9ROMClass->innerClasseCount != 0.
 *			Ensure cpShapeDescription in the same segment.
 *			Ensure all SRPs are in range on 64 bit platforms (including className, superclassName, and outerClassName).
 *
 *		ConstantPool count check:
 *			Ensure ramConstantPoolCount <= romConstantPoolCount
 */

static void verifyJ9ROMClass(J9JavaVM *vm, J9ROMClass *romClass, J9ClassLoader *classLoader);
static void verifyAddressInSegment(J9JavaVM *vm, J9MemorySegment *segment, U_8 *address, const char *description);


void
checkJ9ROMClassSanity(J9JavaVM *vm)
{
	UDATA count = 0;
	J9ClassWalkState walkState;
	J9Class *clazz;

	vmchkPrintf(vm, "  %s Checking ROM classes>\n", VMCHECK_PREFIX);

	clazz = vmchkAllClassesStartDo(vm, &walkState);
	while (NULL != clazz) {
		J9ROMClass *romClass = (J9ROMClass *)DBG_ARROW(clazz, romClass);
		J9ClassLoader *classLoader = (J9ClassLoader *)DBG_ARROW(clazz, classLoader);

		verifyJ9ROMClass(vm, romClass, classLoader);

		count++;
		clazz = vmchkAllClassesNextDo(vm, &walkState);
	}
	vmchkAllClassesEndDo(vm, &walkState);

	vmchkPrintf(vm, "  %s Checking %d ROM classes done>\n", VMCHECK_PREFIX, count);
}

static void
verifyJ9ROMClass(J9JavaVM *vm, J9ROMClass *romClass, J9ClassLoader *classLoader)
{
	J9MemorySegment *segment;
	VMCHECK_PORT_ACCESS_FROM_JAVAVM(vm);

	vmchkMonitorEnter(vm, vm->classMemorySegments->segmentMutex);

	segment = findSegmentInClassLoaderForAddress(classLoader, (U_8*)romClass);
	if (segment != NULL) {
		U_8 *address;

		if (0 != DBG_ARROW(romClass, interfaceCount)) {
			address = (U_8*)VMCHECK_J9ROMCLASS_INTERFACES(romClass);
			verifyAddressInSegment(vm, segment, address, "romClass->interfaces");
		}

		if (0 != DBG_ARROW(romClass, romMethodCount)) {
			address = (U_8*)VMCHECK_J9ROMCLASS_ROMMETHODS(romClass);
			verifyAddressInSegment(vm, segment, address, "romClass->romMethods");
		}

		if (0 != DBG_ARROW(romClass, romFieldCount)) {
			address = (U_8*)VMCHECK_J9ROMCLASS_ROMFIELDS(romClass);
			verifyAddressInSegment(vm, segment, address, "romClass->romFields");
		}

		if (0 != DBG_ARROW(romClass, innerClassCount)) {
			address = (U_8*)VMCHECK_J9ROMCLASS_INNERCLASSES(romClass);
			verifyAddressInSegment(vm, segment, address, "romClass->innerClasses");
		}

		address = (U_8*)VMCHECK_J9ROMCLASS_CPSHAPEDESCRIPTION(romClass);
		verifyAddressInSegment(vm, segment, address, "romClass->cpShapeDescription");
	}

	vmchkMonitorExit(vm, vm->classMemorySegments->segmentMutex);

	{
		J9UTF8 *className = VMCHECK_J9ROMCLASS_CLASSNAME(romClass);
		J9UTF8 *superclassName = VMCHECK_J9ROMCLASS_SUPERCLASSNAME(romClass);
		J9UTF8 *outerclassName = VMCHECK_J9ROMCLASS_OUTERCLASSNAME(romClass);

		if (FALSE == verifyUTF8(className)) {
			vmchkPrintf(vm, " %s - Invalid className=0x%p utf8 for romClass=0x%p>\n",
				VMCHECK_FAILED, className, romClass);
		}

		if ((NULL != superclassName) && (FALSE == verifyUTF8(superclassName))) {
			vmchkPrintf(vm, " %s - Invalid superclassName=0x%p utf8 for romClass=0x%p>\n",
				VMCHECK_FAILED, superclassName, romClass);
		}

		if ((NULL != outerclassName) && (FALSE == verifyUTF8(outerclassName))) {
			vmchkPrintf(vm, " %s - Invalid outerclassName=0x%p utf8 for romClass=0x%p>\n",
				VMCHECK_FAILED, outerclassName, romClass);
		}
	}

	if (DBG_ARROW(romClass, ramConstantPoolCount) > DBG_ARROW(romClass, romConstantPoolCount)) {
		vmchkPrintf(vm, "%s - Error ramConstantPoolCount=%d > romConstantPoolCount=%d for romClass=0x%p>\n",
			VMCHECK_FAILED, DBG_ARROW(romClass, ramConstantPoolCount),
			DBG_ARROW(romClass, romConstantPoolCount), romClass);
	}
}

static void
verifyAddressInSegment(J9JavaVM *vm, J9MemorySegment *segment, U_8 *address, const char *description)
{
	U_8 *heapBase = (U_8 *)DBG_ARROW(segment, heapBase);
	U_8 *heapAlloc = (U_8 *)DBG_ARROW(segment, heapAlloc);

	if ((address < heapBase) || (address >= heapAlloc)) {
		vmchkPrintf(vm, "%s - Address 0x%p (%s) not in segment [heapBase=0x%p, heapAlloc=0x%p]>\n",
			VMCHECK_FAILED, address, description, heapBase, heapAlloc);
	}
}
