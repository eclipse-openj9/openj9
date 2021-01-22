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

public class SystemDumpTest3PackedMixedOneRefChecker {
	
	/**
	 * Check through the output "info class com/ibm/dump/tests/types/packed/PackedMixedOneRef".
	 * 
	 *  The expected output is as follows:
	 *  <pre>
> info class com/ibm/dump/tests/types/packed/PackedMixedOneRef
name = com/ibm/dump/tests/types/packed/PackedMixedOneRef

	ID = 0xf020708    superID = 0xf010150    
	classLoader = <data unavailable>    modifiers: No modifiers available

	This is a packed class

	number of instances:     1
	total size of instances on the heap: 24 bytes
		</pre>

	 */
	
	public static boolean checkInfoClassCommand(String fileName) throws IOException {
		String[] validTotalSizes = {"24","40"}; // 32 , 64

		
		String command = "info class com/ibm/dump/tests/types/packed/PackedMixedOneRef";
		System.err.println("\nChecking the output from the command \"" + command + "\"");
		
		OutputFile o = new OutputFile(fileName);
		if (! o.skipUnlimitedToLineContaining(command) ) {
			// if we cannot find this line, don't bother going any further
			return false;
		}
		boolean passed = true;	
		passed &= o.skipLimitedToLineContaining(6, "This is a packed class");
		passed &= o.skipLimitedToLineContaining(2, "number of instances");
		passed &= o.linePassesCheckForStringAtWord("1", 4); // number of instances 1
		passed &= o.skipLimitedToLineContaining(1, "total size of instances on the heap");
		passed &= o.linePassesCheckForStringsAtWord(validTotalSizes, 8);
		return passed;

	}
	
	/**
	 * Check through the output looking for the output from "x/j com/ibm/dump/tests/types/packed/PackedMixedOneRef".
	 * 
	 *  The expected output is as follows:
	 *  <pre>
x/j com/ibm/dump/tests/types/packed/PackedMixedOneRef
	 heap #1 - name: Generational@298c90

	  "com/ibm/dump/tests/types/packed/PackedMixedOneRef" has no static fields

	  com/ibm/dump/tests/types/packed/PackedMixedOneRef @ 0x2ef47f30
	    This is an on-heap packed object occupying 24 bytes on the heap
	   fields inherited from "com/ibm/jvm/packed/PackedObject":
	   declared fields:
	    public int intField = 99 (0x63)
	    public com.ibm.dump.tests.types.notpacked.NotPackedPrimitives npp = <object> @ 0x2ef47f48

	    references:
 	      0x2ef47f30 0x2ef47f48
		</pre>
	 * @throws IOException 
	 */
	
	public static boolean checkXJClassCommand(String fileName) throws IOException {
		String[] validSizes = {"24","40"}; // 32 , 64
		
		String command = "x/j com/ibm/dump/tests/types/packed/PackedMixedOneRef";
		System.err.println("\nChecking the output from the command \"" + command + "\"");
		
		OutputFile o = new OutputFile(fileName);
		if (! o.skipUnlimitedToLineContaining(command) ) {
			// if we cannot find this line, don't bother going any further
			return false;
		}
		boolean passed = true;
		passed &= o.skipUnlimitedToLineContaining("com/ibm/dump/tests/types/packed/PackedMixedOneRef @");
		passed &= o.skipLimitedToLineContaining(1, "This is an on-heap packed object");
		passed &= o.linePassesCheckForStringsAtWord(validSizes,8);

		System.err.println("Checking the second reference from the PackedMixedOneRef is to a com/ibm/dump/tests/types/notpacked/NotPackedPrimitives");
		passed &= o.skipToLineContainingListOfReferencesAndCheckCount(6, 2);
		String secondReference = o.getSecondReference();		

		passed &= OutputFile.checkReferenceIsToObject(fileName, secondReference, "com/ibm/dump/tests/types/notpacked/NotPackedPrimitives");
		return passed;
		
	}





}
