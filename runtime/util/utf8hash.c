/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

#include "util_api.h"

/**
 * Hash function for UTF8 strings which works from the front of the string to the back.  Mathematically equivalent to
 * computeJavaHashForExpandedString, which works back to front on Unicode.
 *
 * @param data points to raw UTF8 bytes, assumed to be a valid encoding
 * @param length is the number of bytes
 *
 * @return hash code for the UTF8 string
 */

UDATA
computeHashForUTF8(const U_8 * data, UDATA length)
{
	UDATA hash = 0;
	const U_8 * end = data + length;

	while (data < end) {
		U_16 c;

		data += decodeUTF8Char(data, &c);
		hash = (hash << 5) - hash + c;
	}
	return hash;
}
