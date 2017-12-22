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

#include "j9protos.h"
#include "util_internal.h"


void
argBitsFromSignature(U_8 * signature, U_32 * resultArrayBase, UDATA resultArraySize, UDATA isStatic) 
{
	U_32 argBit = 1;

	/* clear the result map as we won't write all of it */
	memset ((U_8 *)resultArrayBase, 0, resultArraySize * sizeof (U_32));

	if (!isStatic) {
		/* not static - arg 0 is object */
		*resultArrayBase |= argBit;
		argBit <<= 1;
	}
	
	/* Parse the signature inside the ()'s */
	while (*(++signature) != ')') {
		if ((*signature == '[') || (*signature == 'L')) {
			*resultArrayBase |= argBit;
			while (*signature == '[') {
				signature++;
			}
			if (*signature == 'L' ) {
				while (*signature != ';') {
					signature++;
				}
			}
		} else if ((*signature == 'J') || (*signature == 'D')) {
			argBit <<= 1;
			if (argBit == 0) {
				argBit = 1;
				resultArrayBase++;
			}
		}
		
		argBit <<= 1;
		if (argBit == 0) {
			argBit = 1;
			resultArrayBase++;
		}
	}
}


