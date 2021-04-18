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
 *******************************************************************************/package com.ibm.dump.tests.system_dump_packed_objects;

import java.io.IOException;

import com.ibm.dump.tests.OutputFile;

public class SystemDumpTest7NestedPackedChecker {
	
	/**
	 * Check through the output looking for the output from "info class com/ibm/dump/tests/types/packed/NestedPacked".
	 * 
	 *  The expected output is as follows:
	 *  <pre>
info class com/ibm/dump/tests/types/packed/NestedPacked
name = com/ibm/dump/tests/types/packed/NestedPacked

	ID = 0x3d8fb00    superID = 0x3bd3f00    
	classLoader = 0x2ef39d70    modifiers: public final 

	This is a packed class

	number of instances:     2
	total size of instances on the heap: 40 bytes

Inheritance chain....

	com/ibm/jvm/packed/PackedObject
	   com/ibm/dump/tests/types/packed/NestedPacked

Fields......

	  static fields for "com/ibm/dump/tests/types/packed/NestedPacked"
	    public static int staticIntField = 99 (0x63)

	  non-static fields for "com/ibm/dump/tests/types/packed/NestedPacked"
	    public int intField
	    public com.ibm.dump.tests.types.packed.NestedPacked1 nestedPacked1Field
</pre>
	 * @throws IOException 
	 */
	
	public static boolean checkInfoClassCommand(String fileName) throws IOException {
		String[] validTotalSizes = {"40","64", "48"}; // on- + off-heap 32, on- + off-heap 64


		boolean passed = true;
		
		String command = "info class com/ibm/dump/tests/types/packed/NestedPacked";
		System.err.println("\nChecking the output from the command \"" + command + "\"");
		
		OutputFile o = new OutputFile(fileName);
		if (! o.skipUnlimitedToLineContaining(command) ) {
			// if we cannot find this line, don't bother going any further
			return false;
		}
		passed &= o.skipLimitedToLineContaining(6, "This is a packed class");
		passed &= o.skipLimitedToLineContaining(2, "number of instances");
		passed &= o.linePassesCheckForStringAtWord("2",4);
		passed &= o.skipLimitedToLineContaining(1, "total size of instances on the heap");
		passed &= o.linePassesCheckForStringsAtWord(validTotalSizes,8);

		return passed;
	}
	
	/**
	 * Check through the output looking for the output from "x/j com/ibm/dump/tests/types/packed/NestedPacked".
	 * 
	 *  The expected output is as follows:
	 *  
	 *  BUT can appear in either order :-(
	 *  <pre>
x/j com/ibm/dump/tests/types/packed/NestedPacked
	 heap #1 - name: Generational@689b98

	  static fields for "com/ibm/dump/tests/types/packed/NestedPacked"
	    public static int staticIntField = 99 (0x63)

	  com/ibm/dump/tests/types/packed/NestedPacked @ 0x2ef4a840
	   This is an on-heap packed object occupying 24 bytes on the heap
	   fields inherited from "com/ibm/jvm/packed/PackedObject":
	   declared fields:
	    public int intField = 99 (0x63)
	    public com.ibm.dump.tests.types.packed.NestedPacked1 nestedPacked1Field = <nested packed object> com/ibm/dump/tests/types/packed/NestedPacked1 packed @ 0x2ef4a850

	    references:
 	      0x2ef4a840

	  com/ibm/dump/tests/types/packed/NestedPacked @ 0x2ef4a878
	   This is an off-heap packed object occupying 16 bytes on the heap
	    The native memory is allocated at 0x359e1c8
	    The packed data is 12 bytes long
	   fields inherited from "com/ibm/jvm/packed/PackedObject":
	   declared fields:
	    public int intField = 99 (0x63)
	    public com.ibm.dump.tests.types.packed.NestedPacked1 nestedPacked1Field = <nested packed object> com/ibm/dump/tests/types/packed/NestedPacked1 packed @ 0x359e1cc

	    references: <none>

		</pre>
	 * @throws IOException 
	 */
	
	public static boolean checkXJClassCommand(String fileName) throws IOException {
		String[] validSizes = {"24","16","40","24", "32"}; // on-heap 32, off-heap 32, on-heap 64, off-heap 64

		String command = "x/j com/ibm/dump/tests/types/packed/NestedPacked";
		System.err.println("\nChecking the output from the command \"" + command + "\"");
		
		OutputFile o = new OutputFile(fileName);
		if (! o.skipUnlimitedToLineContaining(command) ) {
			// if we cannot find this line, don't bother going any further
			return false;
		}

		boolean passed = true;
		passed &= o.skipLimitedToLineContaining(7, "This is an ");
		passed &= o.linePassesCheckForStringsAtWord(validSizes,8);		
//		passed &= o.skipToLineContainingListOfReferencesAndCheckCount(6, 1);

		passed &= o.skipLimitedToLineContaining(12, "com/ibm/dump/tests/types/packed/NestedPacked @");
		passed &= o.skipLimitedToLineContaining(1, "This is an ");
		passed &= o.linePassesCheckForStringsAtWord(validSizes,8);
//		passed &= o.skipLimitedToLineContaining(1, "The native memory is allocated at 0x");
//		passed &= o.skipLimitedToLineContaining(1, "The packed data is ");
//		passed &= o.skipToLineContainingListOfReferencesAndCheckCount(6, 0);
		
		return passed;
	}





}
