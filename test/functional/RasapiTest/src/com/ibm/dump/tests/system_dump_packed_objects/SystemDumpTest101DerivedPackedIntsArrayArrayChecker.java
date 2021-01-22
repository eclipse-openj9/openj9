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

public class SystemDumpTest101DerivedPackedIntsArrayArrayChecker {
	
	/**
	 * Check through the output looking for the output from "x/j [Lcom/ibm/dump/tests/types/packed/PackedIntsArray;".
	 * 
	 *  The expected output is as follows:
	 *  <pre>
> x/j [Lcom/ibm/dump/tests/types/packed/PackedIntsArray;
	 heap #1 - name: Generational@2d22b0

	  [Lcom/ibm/dump/tests/types/packed/PackedIntsArray; @ 0x2ef48e58
	   This is an on-heap packed object occupying 16 bytes on the heap
	   This is a derived packed object:
	    target object: com/ibm/dump/tests/types/packed/PackedIntsArrayArray @ 0x2ef48df8
	    target offset: 0xc
	  showing fields for <nested packed object> [Lcom/ibm/dump/tests/types/packed/PackedIntsArray; packed @ 0x2ef48e04
	   0:	<nested packed object> com/ibm/dump/tests/types/packed/PackedIntsArray packed @ 0x2ef48e04
	   1:	<nested packed object> com/ibm/dump/tests/types/packed/PackedIntsArray packed @ 0x2ef48e20
	   2:	<nested packed object> com/ibm/dump/tests/types/packed/PackedIntsArray packed @ 0x2ef48e3c

	    references:
 	      0x2ef48df8
		</pre>
	 * @throws IOException 
	 */
	
	public static boolean checkXJClassCommand(String fileName) throws IOException {
		String[] validSizes = {"16","24", "32"}; // object header on 32 , 64, 64 nocr 

		
		String command = "x/j com/ibm/dump/tests/types/packed/PackedIntsArray$Array";
		System.err.println("\nChecking the output from the command \"" + command + "\"");
		
		OutputFile o = new OutputFile(fileName);
		if (! o.skipUnlimitedToLineContaining(command) ) {
			// if we cannot find this line, don't bother going any further
			return false;	
		}

		boolean passed = true;
		passed &= o.skipLimitedToLineContaining(7, "This is an on-heap packed object");
		passed &= o.linePassesCheckForStringsAtWord(validSizes,8);		
		passed &= o.skipLimitedToLineContaining(1, "This is a derived packed object");
		passed &= o.skipLimitedToLineContaining(1, "target object: com/ibm/dump/tests/types/packed/PackedIntsArrayArray @ 0x");
		passed &= o.skipLimitedToLineContaining(1, "target offset: 0x");
		passed &= o.skipLimitedToLineContaining(1, "showing fields for <nested packed object> com/ibm/dump/tests/types/packed/PackedIntsArray$Array packed @ 0x");
		passed &= o.skipToLineContainingListOfReferencesAndCheckCount(6, 1);
		return passed;
	}





}
