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
#include "util_internal.h"
#include "ut_j9vmutil.h"

UDATA
packageNameLength(J9ROMClass* romClass)
{
	const J9UTF8* className = J9ROMCLASS_CLASSNAME(romClass);
	const BOOLEAN isAnonClass = J9_ARE_ANY_BITS_SET(romClass->extraModifiers, J9AccClassAnonClass);
	BOOLEAN foundFirstSlash = FALSE;
	UDATA result = 0;
	IDATA i = J9UTF8_LENGTH(className) - 1;

	for (; i >= 0; i--) {
		if (J9UTF8_DATA(className)[i] == '/') {
			/* Lambda names contain a '/'. If romClass is an anonymous class, find the second last '/'. */
			if (!isAnonClass || foundFirstSlash) {
				result = (UDATA)i;
				break;
			}
			foundFirstSlash = TRUE;
		}
	}

	return result;
}

const U_8*
getPackageName(J9PackageIDTableEntry* key, UDATA* nameLength)
{
	if (key->taggedROMClass & J9PACKAGE_ID_TAG) {
		J9ROMClass *romClass = (J9ROMClass*)(key->taggedROMClass & ~(UDATA)(J9PACKAGE_ID_TAG | J9PACKAGE_ID_GENERATED));
		*nameLength = packageNameLength(romClass);
		return J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass));
	} else {
		*nameLength = 0;
		return NULL;
	}
}

