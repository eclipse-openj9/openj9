/*******************************************************************************
 * Copyright (c) 1998, 2016 IBM Corp. and others
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

UDATA
compareJavaStringToPartialUTF8(J9VMThread * vmThread, j9object_t string, U_8 * utfData, UDATA utfLength)
{
	UDATA unicodeLength = J9VMJAVALANGSTRING_LENGTH(vmThread, string);
	j9object_t unicodeBytes = J9VMJAVALANGSTRING_VALUE(vmThread, string);
	UDATA i;

	if (IS_STRING_COMPRESSED(vmThread, string)) {
		for (i = 0; i < unicodeLength; i++) {
			U_16 utfChar;
			U_32 count;

			/* If the String is longer than the UTF, then they don't match */

			if (utfLength == 0) {
				return FALSE;
			}

			count = decodeUTF8CharN(utfData, &utfChar, utfLength);
			if (count == 0) {
				return FALSE;
			}
			utfData += count;
			utfLength -= count;

			if (utfChar == '/') {
				utfChar = '.';
			}
			if (utfChar != J9JAVAARRAYOFBYTE_LOAD(vmThread, unicodeBytes, i)) {
				return FALSE;
			}
		}
	} else {
		for (i = 0; i < unicodeLength; i++) {
			U_16 utfChar;
			U_32 count;

			/* If the String is longer than the UTF, then they don't match */

			if (utfLength == 0) {
				return FALSE;
			}

			count = decodeUTF8CharN(utfData, &utfChar, utfLength);
			if (count == 0) {
				return FALSE;
			}
			utfData += count;
			utfLength -= count;

			if (utfChar == '/') {
				utfChar = '.';
			}
			if (utfChar != J9JAVAARRAYOFCHAR_LOAD(vmThread, unicodeBytes, i)) {
				return FALSE;
			}
		}
	}

	return TRUE;
}


