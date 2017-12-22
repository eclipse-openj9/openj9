/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9;

import java.util.Arrays;

/**
 * @author andhall
 *
 */
public class ArgBits
{

	public static void 
	argBitsFromSignature(String signature, int[] resultArray, int arraySize, boolean isStatic) 
	{
		int argBit = 1;
		int writePtr = 0;

		/* clear the result map as we won't write all of it */
		Arrays.fill(resultArray, 0, arraySize, 0);

		if (!isStatic) {
			/* not static - arg 0 is object */
			resultArray[writePtr] |= argBit;
			argBit <<= 1;
		}
		
		int stringPtr = 0;
		/* Parse the signature inside the ()'s */
		char thisChar;
		while ((thisChar = signature.charAt(++stringPtr)) != ')') {
			if ((thisChar == '[') || (thisChar == 'L')) {
				/* Mark a bit for objects or arrays */
				resultArray[writePtr] |= argBit;
				
				/* Walk past the brackets (cope with multi-dimensional arrays */
				while ((thisChar = signature.charAt(stringPtr)) == '[') {
					stringPtr++;
				}

				if (thisChar == 'L' ) {
					/* Walk past the name of the object class */
					while ((thisChar = signature.charAt(stringPtr)) != ';') {
						stringPtr++;
					}
				}
			} else if ((thisChar == 'J') || (thisChar == 'D')) {
				/* Skip an extra slot for longs and doubles */
				argBit <<= 1;
				if (argBit == 0) {
					argBit = 1;
					writePtr++;
				}
			}

			argBit <<= 1;
			if (argBit == 0) {
				argBit = 1;
				writePtr++;
			}
		}
	}


}
