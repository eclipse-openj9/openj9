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

public class SystemDumpTest10PackedIntsArrayArrayChecker {
	
	/**
	 * Check through the output looking for the output from "info class com/ibm/dump/tests/types/packed/PackedIntsArray".
	 * 
	 *  The expected output is as follows:
	 *  <pre>
info class com/ibm/dump/tests/types/packed/PackedIntsArrayArray
name = com/ibm/dump/tests/types/packed/PackedIntsArrayArray

	ID = 0x3f96c00    superID = 0x3cc2700    
	classLoader = 0x2ef39e78    modifiers: public final 

	This is a packed class

	number of instances:     1
	total size of instances on the heap: 96 bytes

Inheritance chain....

	com/ibm/jvm/packed/PackedObject
	   com/ibm/dump/tests/types/packed/PackedIntsArrayArray

		</pre>
	 * @throws IOException 
	 */
	
	public static boolean checkInfoClassCommand(String fileName) throws IOException {
		String[] validTotalSizes = {"96","192", "104", "112", "136"}; // 96 is 32bit, 136 is 64nocr, not sure about the others and can't run packed and compressed refs anyway 


		String command = "info class com/ibm/dump/tests/types/packed/PackedIntsArrayArray";
		System.err.println("\nChecking the output from the command \"" + command + "\"");
		
		OutputFile o = new OutputFile(fileName);
		if (! o.skipUnlimitedToLineContaining(command) ) {
			// if we cannot find this line, don't bother going any further
			return false;
		}

		boolean passed = true;
		passed &= o.skipLimitedToLineContaining(6, "This is a packed class");
		passed &= o.skipLimitedToLineContaining(2, "number of instances");
		passed &= o.linePassesCheckForStringAtWord("2", 4);						
		passed &= o.skipLimitedToLineContaining(1, "total size of instances on the heap");
		passed &= o.linePassesCheckForStringsAtWord(validTotalSizes, 8);							

		return passed;
	}
	
	/**
	 * Check through the output looking for the output from "x/j com/ibm/dump/tests/types/packed/PackedIntsArray".
	 * 
	 *  The expected output is as follows:
	 *  <pre>
x/j com/ibm/dump/tests/types/packed/PackedIntsArrayArray
	 heap #1 - name: Generational@83d0f8

	  "com/ibm/dump/tests/types/packed/PackedIntsArrayArray" has no static fields

	  com/ibm/dump/tests/types/packed/PackedIntsArrayArray @ 0x2ef49758
	   This is an on-heap packed object occupying 96 bytes on the heap
	   fields inherited from "com/ibm/jvm/packed/PackedObject":
	   declared fields:
	    @Length(3) public com.ibm.dump.tests.types.packed.PackedIntsArray[] piArrayArray1 = <nested packed object> com/ibm/dump/tests/types/packed/PackedIntsArray[3] packed @ 0x2ef49764

	    references:
 	      0x2ef49758
		</pre>
	 * @throws IOException 
	 */
	
	public static boolean checkXJClassCommand(String fileName) throws IOException {
		String[] validSizes = {"96","192", "104", "112", "32"}; // 32 , 64, nocr 

		
		String command = "x/j com/ibm/dump/tests/types/packed/PackedIntsArrayArray";
		System.err.println("\nChecking the output from the command \"" + command + "\"");
		
		OutputFile o = new OutputFile(fileName);
		if (! o.skipUnlimitedToLineContaining(command) ) {
			// if we cannot find this line, don't bother going any further
			return false;	
		}

		boolean passed = true;
		passed &= o.skipLimitedToLineContaining(6, "com/ibm/dump/tests/types/packed/PackedIntsArrayArray @");
		passed &= o.skipLimitedToLineContaining(1, "This is an on-heap packed object");
		passed &= o.linePassesCheckForStringsAtWord(validSizes,8);
		passed &= o.skipLimitedToLineContaining(3, "@Length(3) public com.ibm.dump.tests.types.packed.PackedIntsArray$Array piArrayArray1 = <nested packed object> com/ibm/dump/tests/types/packed/PackedIntsArray$Array[3] packed @ 0x");
		
		return passed;
	}





}
