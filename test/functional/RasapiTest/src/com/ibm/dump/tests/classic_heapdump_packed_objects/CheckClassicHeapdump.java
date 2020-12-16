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


import java.io.BufferedReader;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;

/**
 * This class checks a classic heapdump containing packed objects
 * 
 * @author matthew
 * 
 */
public class CheckClassicHeapdump {

	/**
	 * @param args
	 */
	public static void main(String[] args) {

		if (args.length < 1) {
			System.err.println("A file containing a classic heapdump is required to check.");
			System.exit(1);
		}

		String fileName = args[0];						
		boolean passed = true;
		
		System.err.println("Classic Heapdump Checker");
		System.err.println("Checking " + fileName);

		try{	
			/**
			 * Test 1. Check PackedPrimitives
			 */
			System.err.println("\n**************************************************");
			System.err.println("*     1. Check PackedPrimitives");
			System.err.println("**************************************************");
			passed &= ClassicHeapdumpTest1PackedPrimitivesChecker.check(fileName);

			/**
			 * Test 2. Check array of PackedPrimitives
			 */
			System.err.println("\n**************************************************");
			System.err.println("*     2. Check array of PackedPrimitives");
			System.err.println("**************************************************");
			passed &= ClassicHeapdumpTest2ArrayOfPackedPrimitivesChecker.check(fileName);
			
			/**
			 * Test 3. Check PackedMixed
			 */
			System.err.println("\n**************************************************");
			System.err.println("*     3. Check PackedMixed");
			System.err.println("**************************************************");
			passed &= ClassicHeapdumpTest3PackedMixedOneRefChecker.check(fileName);
			
			/**
			 * Test 4. Check array of PackedMixed
			 */
			System.err.println("\n**************************************************");
			System.err.println("*     4. Check array of PackedMixed");
			System.err.println("**************************************************");
			passed &= ClassicHeapdumpTest4ArrayOfPackedMixedOneRefChecker.check(fileName);
			
			/**
			 * Test 5. Check array of PackedMixedTwoRefs
			 */
			System.err.println("\n**************************************************");
			System.err.println("*     5. Check PackedMixedTwoRefs");
			System.err.println("**************************************************");
			passed &= ClassicHeapdumpTest5ArrayOfPackedMixedTwoRefsChecker.check(fileName);
			
			/**
			 * Test 6. Check PackedMixedWithReferenceToSelf
			 */
			System.err.println("\n**************************************************");
			System.err.println("*     6. Check PackedMixedWithReferenceToSelf");
			System.err.println("**************************************************");
			passed &= ClassicHeapdumpTest6PackedMixedWithReferenceToSelfChecker.check(fileName);

			/**
			 * Test 7. Check NestedPacked
			 */
			System.err.println("\n**************************************************");
			System.err.println("*     7. Check NestedPacked");
			System.err.println("**************************************************");
			passed &= ClassicHeapdumpTest7NestedPackedChecker.check(fileName);
			
			/**
			 * Test 8. Check PackedIntsArray
			 */
			System.err.println("\n**************************************************");
			System.err.println("*     8. Check PackedIntsArray");
			System.err.println("**************************************************");
			passed &= ClassicHeapdumpTest8PackedIntsArrayChecker.check(fileName);
			
			/**
			 * Test 9. Check array of PackedMixedWithReferenceToSelf and the derived object 
			 */
			System.err.println("\n**************************************************");
			System.err.println("*     9. Check PackedMixedArrayElement");
			System.err.println("**************************************************");
			passed &= ClassicHeapdumpTest9ArrayOfPackedMixedArrayElementChecker.check(fileName);
						
		} catch (FileNotFoundException e) {
			System.err.println("FileNotFoundException occured reading " + fileName);
			e.printStackTrace(System.err);
			System.exit(1);
		} catch (IOException e) {
			System.err.println("IOException occured reading " + fileName);
			e.printStackTrace(System.err);
			System.exit(1);
		}

		if( passed ) {
			System.err.println("\n**************************************************");
			System.err.println("CheckClassicHeapdump: All tests passed.");
			System.exit(0);
		} else {
			System.err.println("\n**************************************************");
			System.err.println("CheckClassicHeapdump: Some or all tests failed, see messages in tests 1 to 9 above.");
			System.exit(1);
		}
	}





}
