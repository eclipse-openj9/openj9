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

public class ClassicHeapdumpTest5ArrayOfPackedMixedTwoRefsChecker {
	

	
	/**
	 * Check through the classic heapdump looking for the array of PackedMixedTwoRefs and 
	 * check the length. Also check the references. 
	 * 
	 * The essential point is that in the array we will see the references, not to the PackedMixedTwoRefs objects 
	 * as we would if this were a normal array, but the references to the objects to which the PackedMixed objects point. 
	 *  Hence we expect to see 20 references, not 10.
	 *  
	 * The expected output is similar to:
	 *  <pre>
    0x2EF48938 [136] OBJ [Lcom/ibm/dump/tests/types/packed/PackedMixedTwoRefs;
        0x2EF48938 0x2EF48D20 [ ... 18 references removed for brevity ... ] 0x2EF489F0 <-------------------- one self-ref then 20 references to NotPackedPrimitives objects
    0x2EF489C0 [48] OBJ com/ibm/dump/tests/types/notpacked/NotPackedPrimitives
      [blank line]
		</pre>
	 * @throws IOException 
	 */
	
	public static boolean check(String fileName) throws IOException {
				
		String[] validSizes = {
				"[136]",  // on heap 32 
				"[144]",  // on heap 64cr
				"[272]"}; // on heap 64
		
		String objectLine = " OBJ com/ibm/dump/tests/types/packed/PackedMixedTwoRefs$Array";
		System.err.println("\nChecking the output for \"" + objectLine + "\"");

		OutputFile o = new OutputFile(fileName);
		boolean passed = true;
	
		passed &= o.skipUnlimitedToLineContaining(objectLine);
		passed &= o.linePassesCheckForStringsAtWord(validSizes,2);		
		passed &= o.skipToNextLineAndCheckReferencesCount(21);
		String secondReference = o.getSecondReference();	
		
		passed &= OutputFile.checkReferenceInClassicHeapdumpIsToObject(fileName, secondReference, "com/ibm/dump/tests/types/notpacked/NotPackedPrimitives");

		return passed;
		
	}





}
