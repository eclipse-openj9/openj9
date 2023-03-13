/*******************************************************************************
 * Copyright IBM Corp. and others 2016
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package com.ibm.dump.tests.classic_heapdump_packed_objects;

import java.io.IOException;

import com.ibm.dump.tests.OutputFile;

public class ClassicHeapdumpTest6PackedMixedWithReferenceToSelfChecker {
	
	/**
	 * Check through the classic heapdump looking for the PackedMixedWithReferenceToSelf object, and check that it has two 
	 * references to itself.
	 * 
	 *  The expected output is lines such as follows:
	 *  <pre>
    0x2EF48D80 [24] OBJ com/ibm/dump/tests/types/packed/PackedMixedWithReferenceToSelf
        0x2EF48D80 0x2EF48D80
		</pre>
	 * @throws IOException 
	 */
	
	public static boolean check(String fileName) throws IOException {
		
		
		String[] validSizes = {
				"[24]",  // on heap 32 and 64cr
				"[40]"}; // on heap 64
		
		String objectLine = " OBJ com/ibm/dump/tests/types/packed/PackedMixedWithReferenceToSelf";
		System.err.println("\nChecking the output for \"" + objectLine + "\"");

		OutputFile o = new OutputFile(fileName);
		boolean passed = true;
	
		passed &= o.skipUnlimitedToLineContaining(objectLine);
		passed &= o.linePassesCheckForStringsAtWord(validSizes,2);	
		
		passed &= o.skipToNextLineAndCheckReferencesCount(2);
		
		String firstReference = o.getFirstReference();			
		passed &= OutputFile.checkReferenceInClassicHeapdumpIsToObject(fileName, firstReference, "com/ibm/dump/tests/types/packed/PackedMixedWithReferenceToSelf");

		String secondReference = o.getSecondReference();			
		passed &= OutputFile.checkReferenceInClassicHeapdumpIsToObject(fileName, secondReference, "com/ibm/dump/tests/types/packed/PackedMixedWithReferenceToSelf");
		

		return passed;
	}
}
