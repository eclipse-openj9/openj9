/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.dump.tests.portable_heapdump_packed_objects;

import java.io.IOException;

import com.ibm.dump.tests.OutputFile;

public class PortableHeapdumpTest6PackedMixedWithReferenceToSelfChecker {	
	
	/**
	 * Check through the output looking for the output from "x/j com/ibm/dump/tests/types/packed/PackedMixedWithReferenceToSelf".
	 * 
	 *  The expected output is as follows:
	 *  <pre>
x/j com/ibm/dump/tests/types/packed/PackedMixedWithReferenceToSelf
	 heap #1 - name: Java heap

	  "com/ibm/dump/tests/types/packed/PackedMixedWithReferenceToSelf" has no static fields

	  com/ibm/dump/tests/types/packed/PackedMixedWithReferenceToSelf @ 0x2ef49a50
	    This is an on-heap packed object occupying 24 bytes on the heap

	    references:
 	      0x2ef49a50 0x2ef49a50

		</pre>
	 * @throws IOException 
	 */
	
	public static boolean checkXJClassCommand(String fileName) throws IOException {
		String[] validSizes = {
				"24",  // 32 and 64cr
				"40"}; // 64
		
		String command = "x/j com/ibm/dump/tests/types/packed/PackedMixedWithReferenceToSelf";
		System.err.println("\nChecking the output from the command \"" + command + "\"");
		
		OutputFile o = new OutputFile(fileName);
		if (! o.skipUnlimitedToLineContaining(command) ) {
			// if we cannot find this line, don't bother going any further
			return false;
		}

		boolean passed = true;
		passed &= o.skipUnlimitedToLineContaining("com/ibm/dump/tests/types/packed/PackedMixedWithReferenceToSelf @");
		passed &= o.skipLimitedToLineContaining(1, "This is an on-heap packed object");
		passed &= o.linePassesCheckForStringsAtWord(validSizes,8);

		
		System.err.println("Checking that the first (and only) reference from the PackedMixedWithReferenceToSelf is indeed to itself");
		passed &= o.skipToLineContainingListOfReferencesAndCheckCount(8, 2);
		String secondReference = o.getSecondReference();	
		
		passed &= OutputFile.checkReferenceIsToObject(fileName, secondReference, "com/ibm/dump/tests/types/packed/PackedMixedWithReferenceToSelf");

		
		return passed;
	}





}
