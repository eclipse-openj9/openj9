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

public class SystemDumpTest71NestedPacked1Checker {
	
	/**
	 * Check through the output looking for the output from "x/j com/ibm/dump/tests/types/packed/NestedPacked1".
	 * 
	 * We specifically want to find the derived object
	 *  The expected output is as follows:
	 *  
	 *  BUT  can appear in either order :-(
	 *  
	 *  <pre>
x/j com/ibm/dump/tests/types/packed/NestedPacked1
	 heap #1 - name: Generational@709b98

	  static fields for "com/ibm/dump/tests/types/packed/NestedPacked1"
	    public static int staticIntField = 98 (0x62)

	  com/ibm/dump/tests/types/packed/NestedPacked1 @ 0x2ef4a7e0
	   This is an on-heap packed object occupying 16 bytes on the heap
	   This is a derived packed object:
	    target object: com/ibm/dump/tests/types/packed/NestedPacked @ 0x2ef4a7c8
	    target offset: 0x10
	  showing fields for <nested packed object> com/ibm/dump/tests/types/packed/NestedPacked1 packed @ 0x2ef4a7d8
	   fields inherited from "com/ibm/jvm/packed/PackedObject":
	   declared fields:
	    public int intField = 98 (0x62)
	    public com.ibm.dump.tests.types.packed.NestedPacked2 nestedPacked2Field = <nested packed object> com/ibm/dump/tests/types/packed/NestedPacked2 packed @ 0x2ef4a7dc

	    references:
 	      0x2ef4a7c8

	  com/ibm/dump/tests/types/packed/NestedPacked1 @ 0x2ef4a810
	   This is an off-heap packed object occupying 16 bytes on the heap
	    The native memory is allocated at 0x35de1cc
	    The packed data is 8 bytes long
	   fields inherited from "com/ibm/jvm/packed/PackedObject":
	   declared fields:
	    public int intField = 98 (0x62)
	    public com.ibm.dump.tests.types.packed.NestedPacked2 nestedPacked2Field = <nested packed object> com/ibm/dump/tests/types/packed/NestedPacked2 packed @ 0x35de1d0

	    references: <none>
		</pre>
	 * @throws IOException 
	 */
	
	public static boolean checkXJClassCommand(String fileName) throws IOException {
		String[] validSizes = {"16","24"}; // object header size on 32, 64

		String command = "x/j com/ibm/dump/tests/types/packed/NestedPacked1";
		System.err.println("\nChecking the output from the command \"" + command + "\"");
		
		OutputFile o = new OutputFile(fileName);
		if (! o.skipUnlimitedToLineContaining(command) ) {
			// if we cannot find this line, don't bother going any further
			return false;
		}

		boolean passed = true;
//		passed &= o.skipLimitedToLineContaining(7, "This is an on-heap packed object");
		passed &= o.skipLimitedToLineContaining(7, "This is an ");
		passed &= o.linePassesCheckForStringsAtWord(validSizes,8);		
//		passed &= o.skipLimitedToLineContaining(1, "This is a derived packed object");
//		passed &= o.skipLimitedToLineContaining(1, "target object: com/ibm/dump/tests/types/packed/NestedPacked @ 0x");
//		passed &= o.skipLimitedToLineContaining(1, "target offset: 0x");
//		passed &= o.skipLimitedToLineContaining(1, "showing fields for <nested packed object> com/ibm/dump/tests/types/packed/NestedPacked1 packed @ 0x");
//		passed &= o.skipToLineContainingListOfReferencesAndCheckCount(6, 1);

//		passed &= o.skipLimitedToLineContaining(6, "com/ibm/dump/tests/types/packed/NestedPacked1 @");
		passed &= o.skipLimitedToLineContaining(14, "com/ibm/dump/tests/types/packed/NestedPacked1 @");
//		passed &= o.skipLimitedToLineContaining(1, "This is an off-heap packed object");
		passed &= o.skipLimitedToLineContaining(1, "This is an ");
		passed &= o.linePassesCheckForStringsAtWord(validSizes,8);
//		passed &= o.skipLimitedToLineContaining(1, "The native memory is allocated at 0x");
//		passed &= o.skipLimitedToLineContaining(1, "The packed data is ");
//		passed &= o.skipToLineContainingListOfReferencesAndCheckCount(6, 0);
		
		return passed;
	}





}
