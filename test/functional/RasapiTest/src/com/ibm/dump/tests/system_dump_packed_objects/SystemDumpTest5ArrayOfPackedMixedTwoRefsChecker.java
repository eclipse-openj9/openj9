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

public class SystemDumpTest5ArrayOfPackedMixedTwoRefsChecker {
	

	
	/**
	 * Check through the output looking for the output from "x/j com/ibm/dump/tests/types/packed/PackedMixedTwoRefs;".
	 * 
	 *  <pre>
x/j com/ibm/dump/tests/types/packed/PackedMixedTwoRefs;
	 heap #1 - name: Generational@5860f0

	  com/ibm/dump/tests/types/packed/PackedMixedTwoRefs; @ 0x2ef485a0
	   This is an on-heap packed object occupying 136 bytes on the heap
	   0:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedTwoRefs packed @ 0x2ef485b0
	   1:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedTwoRefs packed @ 0x2ef485bc
	   2:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedTwoRefs packed @ 0x2ef485c8
	   3:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedTwoRefs packed @ 0x2ef485d4
	   4:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedTwoRefs packed @ 0x2ef485e0
	   5:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedTwoRefs packed @ 0x2ef485ec
	   6:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedTwoRefs packed @ 0x2ef485f8
	   7:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedTwoRefs packed @ 0x2ef48604
	   8:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedTwoRefs packed @ 0x2ef48610
	   9:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedTwoRefs packed @ 0x2ef4861c

	    references:
 	      0x2ef485a0 0x2ef48988 0x2ef489b8 0x2ef48928 0x2ef48958 0x2ef488c8 0x2ef488f8 0x2ef48868 0x2ef48898 0x2ef48808 0x2ef48838 0x2ef487a8 0x2ef487d8 0x2ef48748 0x2ef48778 0x2ef486e8 0x2ef48718 0x2ef48688 0x2ef486b8 0x2ef48628 0x2ef48658

 	   </pre>
	 * @throws IOException 
	 */
	
	public static boolean checkXJClassCommand(String fileName) throws IOException {
		String[] validSizes = {"136","272", "144"}; // 32 , 64

		
		String command = "x/j com/ibm/dump/tests/types/packed/PackedMixedTwoRefs$Array";
		System.err.println("\nChecking the output from the command \"" + command + "\"");
		
		OutputFile o = new OutputFile(fileName);
		if (! o.skipUnlimitedToLineContaining(command) ) {
			// if we cannot find this line, don't bother going any further
			return false;
		}
		
		boolean passed = true;
		passed &= o.skipUnlimitedToLineContaining("com/ibm/dump/tests/types/packed/PackedMixedTwoRefs$Array @");
		passed &= o.skipLimitedToLineContaining(1, "This is an on-heap packed object");
		passed &= o.linePassesCheckForStringsAtWord(validSizes,8);

		System.err.println("Checking the array contents show packed objects");
		passed &= o.skipLimitedToLineContaining(1, "0:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedTwoRefs packed @ ");
		passed &= o.skipToLineContainingListOfReferencesAndCheckCount(12, 21);
		String firstReference = o.getFirstReference();		

		passed &= OutputFile.checkReferenceIsToObject(fileName, firstReference, "com/ibm/dump/tests/types/packed/PackedMixedTwoRefs$Array");

		
		return passed;
	}





}
