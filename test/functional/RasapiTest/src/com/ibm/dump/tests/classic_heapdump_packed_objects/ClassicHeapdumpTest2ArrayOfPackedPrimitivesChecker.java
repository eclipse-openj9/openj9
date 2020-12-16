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

public class ClassicHeapdumpTest2ArrayOfPackedPrimitivesChecker {
	
	/**
	 * Check through the classic heapdump looking for two arrays of PackedPrimitives and check the length.
	 * Check there are no references from the arrays - this will check that the odd self-reference
	 * that on-heap packed objects have has been suppressed correctly. 
	 *
	 * The expected output is similar to:
	 *  <pre>
    0x2EF48FE0 [336] OBJ [Lcom/ibm/dump/tests/types/packed/PackedPrimitives;
        0x2EF48FE0
    0x2EF49130 [16] OBJ [Lcom/ibm/dump/tests/types/packed/PackedPrimitives;
      [blank line]	
		</pre>
	 * @throws IOException 
	 */
	
	public static boolean check(String fileName) throws IOException {
		
		String[] validSizes = {
				"[336]", //  on heap 32
				"[16]",  // off heap 32
				"[344]", //  on heap 64cr
				"[24]",  // off heap 64cr
				"[352]", //  on heap 64
				"[32]"}; // off heap 64

		String objectLine = "OBJ com/ibm/dump/tests/types/packed/PackedPrimitives$Array";
		System.err.println("\nChecking the output for \"" + objectLine + "\"");
		
		OutputFile o = new OutputFile(fileName);
		boolean passed = true;
		int refCount = 0;
		passed &= o.skipUnlimitedToLineContaining(objectLine);
		passed &= o.linePassesCheckForStringsAtWord(validSizes,2);
		refCount += o.skipToNextLineAndCountReferences();

		passed &= o.skipUnlimitedToLineContaining(objectLine);
		passed &= o.linePassesCheckForStringsAtWord(validSizes,2);
		refCount += o.skipToNextLineAndCountReferences();

		passed &= (refCount == 1); // expect one reference found in the two objects
		
		if (!passed) {
			System.err.println("ClassicHeapdumpTest2ArrayOfPackedPrimitivesChecker failed");
		}
		return passed;
	}
}
