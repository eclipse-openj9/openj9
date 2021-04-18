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
package com.ibm.dump.tests.classic_heapdump_packed_objects;

import java.io.IOException;

import com.ibm.dump.tests.OutputFile;

public class ClassicHeapdumpTest9ArrayOfPackedMixedArrayElementChecker {

	/**
	 * Check through the classic heapdump looking for the derived object representing the 
	 * element in the array of PackedMixedWithReferenceToSelf and
	 * check that it points back to the array.
	 * 
	 *  The expected output is as follows:
	 *  <pre>
0x2EF49020 [24] OBJ [Lcom/ibm/dump/tests/types/packed/PackedMixedWithReferenceToSelf;
	0x2EF49020 0x2EF49038 
0x2EF49038 [16] OBJ com/ibm/dump/tests/types/packed/PackedMixedWithReferenceToSelf
	0x2EF49020  			
        </pre>
	 * @throws IOException 
	 */
	
	public static boolean check(String fileName) throws IOException {
		String[] validArraySizes = {
				"[24]",  // on heap 32
				"[32]",  // on heap 64cr
				"[48]"}; // on heap 64

		String arrayLine = " OBJ com/ibm/dump/tests/types/packed/PackedMixedArrayElement$Array";
		System.err.println("\nChecking the output for \"" + arrayLine + "\"");

		OutputFile o = new OutputFile(fileName);
		boolean passed = true;
	
		passed &= o.skipUnlimitedToLineContaining(arrayLine);
		passed &= o.linePassesCheckForStringsAtWord(validArraySizes,2);		
		passed &= o.skipToNextLineAndCheckReferencesCount(2);
		String secondReference = o.getSecondReference();	
		
		// check the array points to the element
		passed &= OutputFile.checkReferenceInClassicHeapdumpIsToObject(fileName, secondReference, "com/ibm/dump/tests/types/packed/PackedMixedArrayElement");


		String[] validElementSizes = {"[16]","[24]"}; // on-heap 32 and 64cr, on-heap 64

		String className = "com/ibm/dump/tests/types/packed/PackedMixedArrayElement";
		System.err.println("\nLooking for object at " + secondReference + " and checking that it is a PackedMixedArrayElement");

		passed &= o.skipUnlimitedToLineContaining(secondReference);
		passed &= o.linePassesCheckForStringAtWord(className,4);		
		passed &= o.linePassesCheckForStringsAtWord(validElementSizes,2);		
		passed &= o.skipToNextLineAndCheckReferencesCount(1);
		secondReference = o.getFirstReference();	
		
		// check the element points to the array
		passed &= OutputFile.checkReferenceInClassicHeapdumpIsToObject(fileName, secondReference, "com/ibm/dump/tests/types/packed/PackedMixedArrayElement$Array");

		return passed;
	}

}
