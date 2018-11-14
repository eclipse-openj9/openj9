/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
#include "util_internal.h"

/* there is no error checking: the signature MUST be well-formed */
UDATA
getSendSlotsFromSignature(const U_8* signature)
{
	UDATA sendArgs = 0;
	UDATA i = 1; /* 1 to skip the opening '(' */

	for (; ; i++) {
		switch (signature[i]) {
		case ')':
			return sendArgs;
		case '[':
			/* skip all '['s */
			for (i++; signature[i] == '['; i++);
			if ((signature[i] == 'L')
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
				|| (signature[i] == 'Q')
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */
			) {
				/* FALL THRU */
			} else {
				sendArgs++;
				break;
			}
		case 'L':
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		case 'Q':
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */
			for (i++; signature[i] != ';'; i++);
			sendArgs++;
			break;
		case 'D':
		case 'J':
			sendArgs += 2;
			break;
		default:
			/* any other primitive type */
			sendArgs++;
			break;
		}
	}
}

