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

public class ClassicHeapdumpTest4ArrayOfPackedMixedOneRefChecker {
	
	/**
	 * Check through the classic heapdump looking for the array of PackedMixed and check the length.
	 * Each PackedMixed object contains a reference to a non-packed object within it.
	 * When we iterate the references in the array we expect to see 10 references. These are in fact references
	 * to the non-packed objects that each PackedMixed contains, not to the PackedMixed objects themselves as 
	 * would be the case for an array of non-packed objects. 
	 *
	 * The expected output is similar to:
	 *  <pre>
    0x2EF486E8 [96] OBJ [Lcom/ibm/dump/tests/types/packed/PackedMixedOneRef;
        0x2EF486E8 0x2EF488F8 0x2EF488C8 0x2EF48898 0x2EF48868 0x2EF48838 0x2EF48808 0x2EF487D8 0x2EF487A8 0x2EF48778 0x2EF48748 <-------------- one self-ref then 10 references to NotPackedPrimitives objects
    0x2EF48748 [48] OBJ com/ibm/dump/tests/types/notpacked/NotPackedPrimitives
      [blank line]
		</pre>
	 * @throws IOException 
	 */
	
	public static boolean check(String fileName) throws IOException {
		
		String[] validSizes = {
				"[96]",   // on heap 32
				"[104]",  // on heap 64cr
				"[192]"}; // on heap 64
		
		String objectLine = " OBJ com/ibm/dump/tests/types/packed/PackedMixedOneRef$Array";
		System.err.println("\nChecking the output for \"" + objectLine + "\"");

		OutputFile o = new OutputFile(fileName);
		boolean passed = true;
	
		passed &= o.skipUnlimitedToLineContaining(objectLine);
		passed &= o.linePassesCheckForStringsAtWord(validSizes,2);		
		passed &= o.skipToNextLineAndCheckReferencesCount(11);
		String secondReference = o.getSecondReference();		

		passed &= OutputFile.checkReferenceInClassicHeapdumpIsToObject(fileName, secondReference, "com/ibm/dump/tests/types/notpacked/NotPackedPrimitives");

		return passed;
	}
}
