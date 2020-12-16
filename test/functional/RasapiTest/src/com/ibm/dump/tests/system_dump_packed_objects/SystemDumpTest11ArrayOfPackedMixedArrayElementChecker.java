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
package com.ibm.dump.tests.system_dump_packed_objects;

import java.io.IOException;

import com.ibm.dump.tests.OutputFile;

public class SystemDumpTest11ArrayOfPackedMixedArrayElementChecker {	
	
	/**
	 * Check through the output looking for the output from "x/j com/ibm/dump/tests/types/packed/PackedMixedArrayElement".
	 * 
	 * The essential point of this test is to check that there is one reference and one only. This will be a 
	 * reference to the one element in the array. The odd extra self-reference that 
	 * all on-heap packed objects have will have been suppressed 
	 * 
	 *  The expected output is as follows:
	 *  <pre>
x/j [Lcom/ibm/dump/tests/types/packed/PackedMixedArrayElement;
	 heap #1 - name: Generational@3960f0

	  [Lcom/ibm/dump/tests/types/packed/PackedMixedArrayElement; @ 0x2ef48b98
	   This is an on-heap packed object occupying 24 bytes on the heap
	   0:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedArrayElement packed @ 0x2ef48ba8

	    references:
 	      0x2ef48b98 0x2ef48bb0
        
		</pre>
	 * @throws IOException 
	 */
	
	public static boolean checkXJClassCommand(String fileName) throws IOException {
		String[] validArraySizes = {"24", "32", "40", "48"}; // on-heap 32, on-heap 64, 64 nocr Z, nocr
		
		String command = "x/j com/ibm/dump/tests/types/packed/PackedMixedArrayElement$Array";
		System.err.println("\nChecking the output from the command \"" + command + "\"");
		
		OutputFile o = new OutputFile(fileName);
		if (! o.skipUnlimitedToLineContaining(command) ) {
			// if we cannot find this line, don't bother going any further
			return false;
		}

		boolean passed = true;
		passed &= o.skipUnlimitedToLineContaining("com/ibm/dump/tests/types/packed/PackedMixedArrayElement$Array @");
		passed &= o.skipLimitedToLineContaining(1, "This is an on-heap packed object");
		passed &= o.linePassesCheckForStringsAtWord(validArraySizes,8);
		passed &= o.skipLimitedToLineContaining(1, "0:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedArrayElement packed @ 0x");
		
		System.err.println("Checking that the second reference from the array of PackedMixedArrayElement is to the element");
		passed &= o.skipToLineContainingListOfReferencesAndCheckCount(2, 2);
		String secondReference = o.getSecondReference();	
		
		// check the second reference is to the element
		passed &= OutputFile.checkReferenceIsToObject(fileName, secondReference, "com/ibm/dump/tests/types/packed/PackedMixedArrayElement");		

		return passed;
	}





}
