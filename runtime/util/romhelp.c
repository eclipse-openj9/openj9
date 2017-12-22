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

#include "j9protos.h"
#include "rommeth.h"
#include "j9cp.h"
#include "util_internal.h"
#include "ut_j9vmutil.h"

J9ROMMethod *
getOriginalROMMethodUnchecked(J9Method * method)
{
	J9Class * methodClass = J9_CLASS_FROM_METHOD(method);
	J9ROMClass * romClass = methodClass->romClass;
	J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	U_8 * bytecodes = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);

	Trc_VMUtil_getOriginalROMMethodUnchecked_Entry(method);

	if ((bytecodes < (U_8 *) romClass) || (bytecodes >= ((U_8 *) romClass + romClass->romSize))) {
		UDATA methodIndex = getMethodIndexUnchecked(method);
		if (methodIndex < UDATA_MAX) {
			romMethod = J9ROMCLASS_ROMMETHODS(romClass);
			while (methodIndex) {
				romMethod = nextROMMethod(romMethod);
				--methodIndex;
			}
		} else {
			romMethod = NULL;
			Trc_VMUtil_getOriginalROMMethodUnchecked_getMethodIndexFailed(method);
		}
	}

	Trc_VMUtil_getOriginalROMMethodUnchecked_Exit(romMethod);

	return romMethod;
}

J9ROMMethod *
getOriginalROMMethod(J9Method * method)
{
	J9ROMMethod * romMethod = NULL;

	Trc_VMUtil_getOriginalROMMethod_Entry(method);

	romMethod = getOriginalROMMethodUnchecked(method);
	/* ROM method not found in ROM class, just use the one provided by the RAM method */
	if (NULL == romMethod) {
		romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	}

	Trc_VMUtil_getOriginalROMMethod_Exit(romMethod);

	return romMethod;
}



