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

public class PortableHeapdumpTest4ArrayOfPackedMixedOneRefChecker {
	
	/**
	 * Check through the output looking for the output from "info class [Lcom/ibm/dump/tests/types/packed/PackedMixedOneRef;".
	 * 
	 *  The expected output is as follows:
	 *  <pre>
> info class [Lcom/ibm/dump/tests/types/packed/PackedMixedOneRef;
name = [Lcom/ibm/dump/tests/types/packed/PackedMixedOneRef;

        ID = 0xf0767f8    superID = 0xf010650
        classLoader = <data unavailable>    modifiers: No modifiers available

        This is a packed class

        number of instances:     1
        total size of instances on the heap: 56 bytes

		</pre>
	 * @throws IOException 
	 */
	
	public static boolean checkInfoClassCommand(String fileName) throws IOException {
		String[] validTotalSizes = {
				"96",   // 32
				"104",  // 64cr
				"192"}; // 64

		String command = "info class com/ibm/dump/tests/types/packed/PackedMixedOneRef$Array";
		System.err.println("\nChecking the output from the command \"" + command + "\"");
		
		OutputFile o = new OutputFile(fileName);
		if (! o.skipUnlimitedToLineContaining(command) ) {
			// if we cannot find this line, don't bother going any further
			return false;
		}

		boolean passed = true;
		passed &= o.skipLimitedToLineContaining(6, "This is a packed class");
		passed &= o.skipLimitedToLineContaining(2, "number of instances");
		passed &= o.linePassesCheckForStringAtWord("1",4);
		passed &= o.skipLimitedToLineContaining(1, "total size of instances on the heap");
		passed &= o.linePassesCheckForStringsAtWord(validTotalSizes,8);
		return passed;
	}

	/**
	 * Check through the output looking for the output from "x/j [Lcom/ibm/dump/tests/types/packed/PackedMixedOneRef;".
	 * 
	 * The essential point is that in the array we will see the references, not to the PackedMixedOneRef objects as we would if this were 
	 * a normal array, but the references to the objects to which the PackedMixedOneRef objects point. 
	 *  The expected output is as follows:
	 *  <pre>
x/j [Lcom/ibm/dump/tests/types/packed/PackedMixedOneRef;
	 heap #1 - name: Java heap

	  [Lcom/ibm/dump/tests/types/packed/PackedMixedOneRef; @ 0x2ef493b8
	    This is an on-heap packed object occupying 96 bytes on the heap
	    This is an array of size 10
	    references:
 	      0x2ef49418 0x2ef49448 0x2ef49478 0x2ef494a8 0x2ef494d8 0x2ef49508 0x2ef49538 0x2ef49568 0x2ef49598 0x2ef495c8 0x2ef493b8
 	      </pre>
	 * @throws IOException 
	 */
	
	public static boolean checkXJClassCommand(String fileName) throws IOException {
		String[] validSizes = {
				"96",  //  on heap 32
				"104", //  on heap 64cr
				"192"  //  on heap 64
				};

		
		String command = "x/j com/ibm/dump/tests/types/packed/PackedMixedOneRef$Array";
		System.err.println("\nChecking the output from the command \"" + command + "\"");
		
		OutputFile o = new OutputFile(fileName);
		if (! o.skipUnlimitedToLineContaining(command) ) {
			// if we cannot find this line, don't bother going any further
			return false;
		}

		boolean passed = true;
		passed &= o.skipUnlimitedToLineContaining("com/ibm/dump/tests/types/packed/PackedMixedOneRef$Array @");
		passed &= o.skipLimitedToLineContaining(1, "This is an on-heap packed object");
		passed &= o.linePassesCheckForStringsAtWord(validSizes,8);

		System.err.println("Checking the first reference from the array of PackedMixedOneRef is to a com/ibm/dump/tests/types/notpacked/NotPackedPrimitives");
		passed &= o.skipToLineContainingListOfReferencesAndCheckCount(12, 11);
		String secondReference = o.getSecondReference();		

		passed &= OutputFile.checkReferenceIsToObject(fileName, secondReference, "com/ibm/dump/tests/types/notpacked/NotPackedPrimitives");

		
		return passed;
	}





}
