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

/**
 * @file
 * @ingroup VMChk
 */

#include "j9.h"
#include "j9port.h"
#include "rommeth.h"
#include "vmcheck.h"

/*
 *	J9Method sanity:
 *		Bytecode check:
 *			Ensure (bytecodes - sizeof(J9ROMMethod) is in the right "area" to be a ROM Method.
 *			Use ROMClass to determine where ROM methods for that class begin.
 *		Constant pool check:
 *			Ensure method's constant pool is the same as that of its J9Class.
 *			Useful to validate that HCR doesn't violate this invariant.
 *		VTable check:
 *			If method of a non-interface class has modifier J9AccMethodVTable,
 *			ensure it exists in the VTable of its J9Class.
 *			Useful to validate that HCR doesn't violate this invariant.
 */


static U_32 verifyClassMethods(J9JavaVM *vm, J9Class *clazz);
static BOOLEAN findMethodInVTable(J9Method *method, J9Class *clazz);
static BOOLEAN findROMMethodInClass(J9JavaVM *vm, J9ROMClass *romClass, J9ROMMethod *romMethodToFind, U_32 methodCount);


void
checkJ9MethodSanity(J9JavaVM *vm)
{
	UDATA count = 0;
	J9ClassWalkState walkState;
	J9Class *clazz;

	vmchkPrintf(vm, "  %s Checking methods>\n", VMCHECK_PREFIX);

	clazz = vmchkAllClassesStartDo(vm, &walkState);
	while (NULL != clazz) {

		if (!VMCHECK_IS_CLASS_OBSOLETE(clazz)) {
			count += verifyClassMethods(vm, clazz);
		}

		clazz = vmchkAllClassesNextDo(vm, &walkState);
	}
	vmchkAllClassesEndDo(vm, &walkState);

	vmchkPrintf(vm, "  %s Checking %d methods done>\n", VMCHECK_PREFIX, count);
}

static U_32
verifyClassMethods(J9JavaVM *vm, J9Class *clazz)
{
	J9ROMClass *romClass = (J9ROMClass *)DBG_ARROW(clazz, romClass);
	UDATA romClassModifiers = (UDATA)DBG_ARROW(romClass, modifiers);
	BOOLEAN isInterfaceClass = (J9AccInterface == (romClassModifiers & J9AccInterface));
	J9ConstantPool *ramConstantPool = (J9ConstantPool *)DBG_ARROW(clazz, ramConstantPool);
	U_32 methodCount = (U_32)DBG_ARROW(romClass, romMethodCount);
	J9Method *methods = (J9Method *)DBG_ARROW(clazz, ramMethods);
	U_32 i;

	for (i = 0; i < methodCount; i++) {
		J9Method *method = &methods[i];
		U_8 *bytecodes = (U_8*)DBG_ARROW(method, bytecodes);
		J9ROMMethod *romMethod = (J9ROMMethod *)(bytecodes - sizeof(J9ROMMethod));
		UDATA romMethodModifiers = (UDATA)DBG_ARROW(romMethod, modifiers);
		BOOLEAN methodInVTable = (J9AccMethodVTable == (romMethodModifiers & J9AccMethodVTable));

		if (FALSE == findROMMethodInClass(vm, romClass, romMethod, methodCount)) {
			vmchkPrintf(vm, "%s - Error romMethod=0x%p (ramMethod=0x%p) not found in romClass=0x%p>\n",
				VMCHECK_FAILED, romMethod, method, romClass);
		}

		if (!isInterfaceClass && methodInVTable) {
			if (FALSE == findMethodInVTable(method, clazz)) {
				vmchkPrintf(vm, "%s - Error romMethod=0x%p (ramMethod=0x%p) not found in vTable of ramClass=0x%p>\n",
					VMCHECK_FAILED, romMethod, method, clazz);
			}
		}

		if (ramConstantPool != VMCHECK_J9_CP_FROM_METHOD(method)) {
			vmchkPrintf(vm, "%s - Error ramConstantPool=0x%p on ramMethod=0x%p not equal to ramConstantPool=0x%p on ramClass=0x%p>\n",
				VMCHECK_FAILED, VMCHECK_J9_CP_FROM_METHOD(method), method, ramConstantPool, clazz);
		}
	}

	return methodCount;
}

static BOOLEAN
findMethodInVTable(J9Method *method, J9Class *clazz)
{
	UDATA vTableIndex;
	J9VTableHeader *vTableHeader = J9VTABLE_HEADER_FROM_RAM_CLASS(clazz);
	UDATA vTableSize = vTableHeader->size;
	J9Method **vTable = J9VTABLE_FROM_HEADER(vTableHeader);

	for (vTableIndex = 0; vTableIndex < vTableSize; vTableIndex++) {
		if (method == vTable[vTableIndex]) {
			return TRUE;
		}
	}

	return FALSE;
}

static BOOLEAN
findROMMethodInClass(J9JavaVM *vm, J9ROMClass *romClass, J9ROMMethod *romMethodToFind, U_32 methodCount)
{
	BOOLEAN found = FALSE;
	J9ROMMethod *romMethod;
	U_32 i;

	for (i = 0; i < methodCount; i++) {
		if (0 == i) {
			romMethod = VMCHECK_J9ROMCLASS_ROMMETHODS(romClass);
		} else {
			romMethod = VMCHECK_J9_NEXT_ROM_METHOD(romMethod);
		}
		if (romMethodToFind == romMethod) {
			return TRUE;
		}
	}

	return FALSE;
}
