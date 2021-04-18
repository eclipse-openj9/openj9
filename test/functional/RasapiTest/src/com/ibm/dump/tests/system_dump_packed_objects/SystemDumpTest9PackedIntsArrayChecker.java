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

public class SystemDumpTest9PackedIntsArrayChecker {
	
	/**
	 * Check through the output looking for the output from "info class com/ibm/dump/tests/types/packed/PackedIntsArray".
	 * 
	 *  The expected output is as follows:
	 *  <pre>
> info class com/ibm/dump/tests/types/packed/PackedIntsArray
name = com/ibm/dump/tests/types/packed/PackedIntsArray

        ID = 0xf076cf0    superID = 0xf010650
        classLoader = <data unavailable>    modifiers: No modifiers available

        This is a packed class

        number of instances:     2
        total size of instances on the heap: 56 bytes

		</pre>
	 * @throws IOException 
	 */
	
	public static boolean checkInfoClassCommand(String fileName) throws IOException {
		String[] validTotalSizes = {"56","80", "64"}; // 32 , 64 


		String command = "info class com/ibm/dump/tests/types/packed/PackedIntsArray";
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
x/j com/ibm/dump/tests/types/packed/PackedIntsArray
	 heap #1 - name: Generational@689b98

	  "com/ibm/dump/tests/types/packed/PackedIntsArray" has no static fields

	  com/ibm/dump/tests/types/packed/PackedIntsArray @ 0x2ef4a9c0
	   This is an on-heap packed object occupying 40 bytes on the heap
	   fields inherited from "com/ibm/jvm/packed/PackedObject":
	   declared fields:
	    @Length(2) public com.ibm.jvm.packed.types.PackedInt[] piArray1 = <nested packed object> com/ibm/jvm/packed/types/PackedInt[2] packed @ 0x2ef4a9cc
	    @Length(5) public com.ibm.jvm.packed.types.PackedInt[] piArray2 = <nested packed object> com/ibm/jvm/packed/types/PackedInt[5] packed @ 0x2ef4a9d4

	    references:
 	      0x2ef4a9c0

	  com/ibm/dump/tests/types/packed/PackedIntsArray @ 0x2ef4a9e8
	   This is an off-heap packed object occupying 16 bytes on the heap
	    The native memory is allocated at 0x68a170
	    The packed data is 28 bytes long
	   fields inherited from "com/ibm/jvm/packed/PackedObject":
	   declared fields:
	    @Length(2) public com.ibm.jvm.packed.types.PackedInt[] piArray1 = <nested packed object> com/ibm/jvm/packed/types/PackedInt[2] packed @ 0x68a170
	    @Length(5) public com.ibm.jvm.packed.types.PackedInt[] piArray2 = <nested packed object> com/ibm/jvm/packed/types/PackedInt[5] packed @ 0x68a178

	    references: <none>
		</pre>
	 * @throws IOException 
	 */
	
	public static boolean checkXJClassCommand(String fileName) throws IOException {
		String[] validSizes = {"40","16", "48", "56", "24"}; // on-heap 32, on-heap 64, nocr

		
		String command = "x/j com/ibm/dump/tests/types/packed/PackedIntsArray";
		System.err.println("\nChecking the output from the command \"" + command + "\"");
		
		OutputFile o = new OutputFile(fileName);
		if (! o.skipUnlimitedToLineContaining(command) ) {
			// if we cannot find this line, don't bother going any further
			return false;
		}

		boolean passed = true;
		passed &= o.skipLimitedToLineContaining(6, "com/ibm/dump/tests/types/packed/PackedIntsArray @");
		passed &= o.skipLimitedToLineContaining(1, "This is an on-heap packed object");
		passed &= o.linePassesCheckForStringsAtWord(validSizes,8);
		passed &= o.skipLimitedToLineContaining(3, "@Length(2) public com.ibm.jvm.packed.types.PackedInt$Array piArray1 = <nested packed object> com/ibm/jvm/packed/types/PackedInt$Array[2] packed @ 0x");
		passed &= o.skipLimitedToLineContaining(1, "@Length(5) public com.ibm.jvm.packed.types.PackedInt$Array piArray2 = <nested packed object> com/ibm/jvm/packed/types/PackedInt$Array[5] packed @ 0x");
		
		passed &= o.skipLimitedToLineContaining(6, "com/ibm/dump/tests/types/packed/PackedIntsArray @");
		passed &= o.skipLimitedToLineContaining(1, "This is an off-heap packed object");
		passed &= o.linePassesCheckForStringsAtWord(validSizes,8);
		passed &= o.skipLimitedToLineContaining(1, "The native memory is allocated at 0x");
		passed &= o.skipLimitedToLineContaining(1, "The packed data is ");
		passed &= o.skipLimitedToLineContaining(3, "@Length(2) public com.ibm.jvm.packed.types.PackedInt$Array piArray1 = <nested packed object> com/ibm/jvm/packed/types/PackedInt$Array[2] packed @ 0x");
		passed &= o.skipLimitedToLineContaining(1, "@Length(5) public com.ibm.jvm.packed.types.PackedInt$Array piArray2 = <nested packed object> com/ibm/jvm/packed/types/PackedInt$Array[5] packed @ 0x");

		return passed;
	}





}
