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

public class SystemDumpTest2ArrayOfPackedPrimitivesChecker {
	
	/**
	 * Check through the output looking for the output from "info class com/ibm/dump/tests/types/packed/PackedPrimitives;".
	 * Check that the "This class is packed" line is present.
	 * Check that there are two instances.
	 * Check that the memory on the heap is right.
	 * 
	 *  The expected output is as follows:
	 *  <pre>
> info class com/ibm/dump/tests/types/packed/PackedPrimitives;
name = com/ibm/dump/tests/types/packed/PackedPrimitives;

        ID = 0xf0764f0    superID = 0xf010650
        classLoader = <data unavailable>    modifiers: No modifiers available

        This is a packed class

        number of instances:     2
        total size of instances on the heap: 112 bytes
		</pre>
	 * @throws IOException 
	 */
	
	public static boolean checkInfoClassCommand(String fileName) throws IOException {
		String[] validTotalSizes = {"352","384", "368"}; // 32, 64

		
		String command = "info class com/ibm/dump/tests/types/packed/PackedPrimitives$Array";
		System.err.println("\nChecking the output from the command \"" + command + "\"");
		
		OutputFile o = new OutputFile(fileName);
		if (! o.skipUnlimitedToLineContaining(command) ) {
			// if we cannot find this line, don't bother going any further
			return false;
		}
		
		boolean passed = true;
		passed &= o.skipLimitedToLineContaining(6, "This is a packed class");
		passed &= o.skipLimitedToLineContaining(2, "number of instances");
		passed &= o.linePassesCheckForStringAtWord("2",4);
		passed &= o.skipLimitedToLineContaining(1, "total size of instances on the heap");
		passed &= o.linePassesCheckForStringsAtWord(validTotalSizes,8);
		return passed;
	}
	
	/**
	 * Check through the output looking for the output from "x/j com/ibm/dump/tests/types/packed/PackedPrimitives;".
	 * 
	 *  The expected output is as follows:
	 *  <pre>
> x/j com/ibm/dump/tests/types/packed/PackedPrimitives;
	 heap #1 - name: Generational@538c90

	  com/ibm/dump/tests/types/packed/PackedPrimitives; @ 0x2ef47d18
	    This is an on-heap packed object occupying 336 bytes on the heap
	   0:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x2ef47d28
	   1:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x2ef47d48
	   2:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x2ef47d68
	   3:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x2ef47d88
	   4:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x2ef47da8
	   5:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x2ef47dc8
	   6:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x2ef47de8
	   7:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x2ef47e08
	   8:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x2ef47e28
	   9:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x2ef47e48

	    references:
 	      0x2ef47d18
	  com/ibm/dump/tests/types/packed/PackedPrimitives; @ 0x2ef47e68
	    This is an off-heap packed object occupying 16 bytes on the heap
	    The native memory is allocated at 0x3afa760
	   0:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x3afa760
	   1:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x3afa780
	   2:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x3afa7a0
	   3:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x3afa7c0
	   4:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x3afa7e0
	   5:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x3afa800
	   6:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x3afa820
	   7:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x3afa840
	   8:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x3afa860
	   9:	<packed object> com/ibm/dump/tests/types/packed/PackedPrimitives packed @ 0x3afa880

	    references:
              [blank line]
		</pre>
	 * @throws IOException 
	 */
	
	public static boolean checkXJClassCommand(String fileName) throws IOException {
		String[] validSizes = {"336","16","352","32", "344", "24"}; // on-heap 32, off-heap 32, on-heap 64, off-heap 64
		
		String command = "x/j com/ibm/dump/tests/types/packed/PackedPrimitives$Array";
		System.err.println("\nChecking the output from the command \"" + command + "\"");
		
		OutputFile o = new OutputFile(fileName);
		if (! o.skipUnlimitedToLineContaining(command) ) {
			// if we cannot find this line, don't bother going any further
			return false;
		}
		
		// Check for two packed primitive arrays, one of which is on-heap and has one reference, the other has no references
		boolean passed = true;
		int refCount = 0;
		
		passed &= o.skipUnlimitedToLineContaining("com/ibm/dump/tests/types/packed/PackedPrimitives$Array @");
		passed &= o.skipLimitedToLineContaining(1, "packed object");
		passed &= o.linePassesCheckForStringsAtWord(validSizes,8);
		passed &= o.skipLimitedToReferencesLine(14);
		refCount += o.skipToNextLineAndCountReferences();

		passed &= o.skipUnlimitedToLineContaining("com/ibm/dump/tests/types/packed/PackedPrimitives$Array @");
		passed &= o.skipLimitedToLineContaining(1, "packed object");
		passed &= o.linePassesCheckForStringsAtWord(validSizes,8);
		passed &= o.skipLimitedToReferencesLine(14);
		refCount += o.skipToNextLineAndCountReferences();

		passed &= (refCount == 1); // expect one reference found in total
		
		if (!passed) {
			System.err.println("SystemDumpTest2ArrayOfPackedPrimitivesChecker failed");
		}

		return passed;
	}

}
