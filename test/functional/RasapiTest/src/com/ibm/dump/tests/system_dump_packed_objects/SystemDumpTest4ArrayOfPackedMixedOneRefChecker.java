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

public class SystemDumpTest4ArrayOfPackedMixedOneRefChecker {
	
	/**
	 * Check through the output looking for the output from "info class com/ibm/dump/tests/types/packed/PackedMixedOneRef;".
	 * 
	 *  The expected output is as follows:
	 *  <pre>
> info class com/ibm/dump/tests/types/packed/PackedMixed;
name = com/ibm/dump/tests/types/packed/PackedMixed;

        ID = 0xf0767f8    superID = 0xf010650
        classLoader = <data unavailable>    modifiers: No modifiers available

        This is a packed class

        number of instances:     1
        total size of instances on the heap: 56 bytes

		</pre>
	 * @throws IOException 
	 */
	
	public static boolean checkInfoClassCommand(String fileName) throws IOException {
		String[] validTotalSizes = {"96","192", "104"}; // 32 , 64

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
	 * Check through the output looking for the output from "x/j com/ibm/dump/tests/types/packed/PackedMixed;".
	 * 
	 *  The expected output is as follows:
	 *  <pre>
x/j com/ibm/dump/tests/types/packed/PackedMixedOneRef;
	 heap #1 - name: Generational@5860f0

	  com/ibm/dump/tests/types/packed/PackedMixedOneRef; @ 0x2ef48350
	   This is an on-heap packed object occupying 96 bytes on the heap
	   0:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedOneRef packed @ 0x2ef48360
	   1:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedOneRef packed @ 0x2ef48368
	   2:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedOneRef packed @ 0x2ef48370
	   3:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedOneRef packed @ 0x2ef48378
	   4:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedOneRef packed @ 0x2ef48380
	   5:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedOneRef packed @ 0x2ef48388
	   6:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedOneRef packed @ 0x2ef48390
	   7:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedOneRef packed @ 0x2ef48398
	   8:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedOneRef packed @ 0x2ef483a0
	   9:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedOneRef packed @ 0x2ef483a8

	    references:
 	      0x2ef48350 0x2ef48560 0x2ef48530 0x2ef48500 0x2ef484d0 0x2ef484a0 0x2ef48470 0x2ef48440 0x2ef48410 0x2ef483e0 0x2ef483b0
		</pre>
	 * @throws IOException 
	 */
	
	public static boolean checkXJClassCommand(String fileName) throws IOException {
		String[] validSizes = {"96","192", "104"}; // 32 , 64

		
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

		System.err.println("Checking the array contents show packed objects");
		passed &= o.skipLimitedToLineContaining(1, "0:	<nested packed object> com/ibm/dump/tests/types/packed/PackedMixedOneRef packed @ ");
		passed &= o.skipToLineContainingListOfReferencesAndCheckCount(12, 11);
		String firstReference = o.getFirstReference();		

		passed &= OutputFile.checkReferenceIsToObject(fileName, firstReference, "com/ibm/dump/tests/types/packed/PackedMixedOneRef$Array");

		
		return passed;
	}





}
