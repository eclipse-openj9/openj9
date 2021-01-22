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

public class PortableHeapdumpTest5ArrayOfPackedMixedTwoRefsChecker {
	

	
	/**
	 * Check through the output looking for the output from "x/j [Lcom/ibm/dump/tests/types/packed/PackedMixedTwoRefs;".
	 * 
	 * The essential point is that in the array we will see the references, not to the PackedMixedTwoRefs objects 
	 * as we would if this were a normal array, but the references to the objects to which the PackedMixed objects point. 
	 *  Hence we expect to see 20 references, not 10.
	 *  <pre>
x/j [Lcom/ibm/dump/tests/types/packed/PackedMixedTwoRefs;
	 heap #1 - name: Java heap

	  [Lcom/ibm/dump/tests/types/packed/PackedMixedTwoRefs; @ 0x2ef49608
	    This is an on-heap packed object occupying 136 bytes on the heap
	    This is an array of size 10
	    references:
 	      0x2ef496c0 0x2ef49690 0x2ef49720 0x2ef496f0 0x2ef49780 0x2ef49750 0x2ef497e0 0x2ef497b0 0x2ef49840 0x2ef49810 0x2ef498a0 0x2ef49870 0x2ef49900 0x2ef498d0 0x2ef49960 0x2ef49930 0x2ef499c0 0x2ef49990 0x2ef49a20 0x2ef499f0 0x2ef49608
		</pre>
	 * @throws IOException 
	 */
	
	public static boolean checkXJClassCommand(String fileName) throws IOException {
		String[] validSizes = {
				"136", //  on heap 32
				"144", // 64cr
				"272"  // 64
				};

		
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
		
		System.err.println("Checking that the first reference from the array of PackedMixedTwoRefs is to an instance of com/ibm/dump/tests/types/notpacked/NotPackedPrimitives");
		passed &= o.skipToLineContainingListOfReferencesAndCheckCount(2, 21);
		String secondReference = o.getSecondReference();	
		
		passed &= OutputFile.checkReferenceIsToObject(fileName, secondReference, "com/ibm/dump/tests/types/notpacked/NotPackedPrimitives");

		
		return passed;
	}





}
