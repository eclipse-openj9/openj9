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
package com.ibm.dump.tests.classic_heapdump_packed_objects;

import java.io.IOException;

import com.ibm.dump.tests.OutputFile;

public class ClassicHeapdumpTest1PackedPrimitivesChecker {
	
	/**
	 * Check through the classic heapdump looking for the lines containing "OBJ com/ibm/dump/tests/types/packed/PackedPrimitives".
	 * 
	 * Check there are no references from the objects - this will check that the curious and irritating self-reference
	 * that gc returns for on-heap packed objects has been suppressed correctly when the dump was written. 
	 * 
	 *  The expected output is lines such as follows:
	 *  <pre>
	 *  
    0x2EF48F80 [48] OBJ com/ibm/dump/tests/types/packed/PackedPrimitives <-------------- java packed
        0x2EF48F80                       <-------------- one self-reference for a java packed object
    0x2EF48FB0 [16] OBJ com/ibm/dump/tests/types/packed/PackedPrimitives <-- --------- native packed
      [blank line]
      	</pre>
	 * @throws IOException 
	 */
	
	public static boolean check(String fileName) throws IOException {
		String[] validSizes = {
				"[48]",  //  on heap 32 and 64cr
				"[16]",  // off heap 32 and 64cr
				"[56]",  //  on heap 64
				"[24]"}; // off heap 64
		
		String objectLine = "OBJ com/ibm/dump/tests/types/packed/PackedPrimitives";
		System.err.println("\nChecking the output for \"" + objectLine + "\"");
		
		OutputFile o = new OutputFile(fileName);
		
		// check for three OBJ lines, one of which will have a single reference, the others no references
		boolean passed = true;
		int refCount = 0;
		passed &= o.skipUnlimitedToLineEnding(objectLine);
		passed &= o.linePassesCheckForStringsAtWord(validSizes,2);
		refCount += o.skipToNextLineAndCountReferences();
		
		passed &= o.skipUnlimitedToLineEnding(objectLine);
		passed &= o.linePassesCheckForStringsAtWord(validSizes,2);
		refCount += o.skipToNextLineAndCountReferences();
		
		passed &= o.skipUnlimitedToLineEnding(objectLine);
		passed &= o.linePassesCheckForStringsAtWord(validSizes,2);
		refCount += o.skipToNextLineAndCountReferences();

		passed &= (refCount == 1); // expect one reference found in the three objects
		
		if (!passed) {
			System.err.println("ClassicHeapdumpTest1PackedPrimitivesChecker failed");
		}
		return passed;
	}
}
