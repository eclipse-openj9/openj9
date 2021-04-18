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
package com.ibm.dump.tests.portable_heapdump_packed_objects;


import java.io.FileNotFoundException;
import java.io.IOException;

/**
 * This class checks the output from jdmpview run against a PHD containing packed objects
 * 
 * @author matthew
 * 
 */
public class CheckPortableHeapdump {

	/**
	 * @param args
	 */
	public static void main(String[] args) {

		if (args.length < 1) {
			System.err.println("A file containing jdmpview output is required to check.");
			System.exit(1);
		}
		String fileName = args[0];	
		
		System.err.println("Portable Heapdump Checker");
		System.err.println("Checking " + fileName);

		boolean passed = true;

		try {
			/**
			 * Test 1. Check PackedPrimitives
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     1. Check PackedPrimitives\n");
			System.err.println("**************************************************");
			passed &= PortableHeapdumpTest1PackedPrimitivesChecker.checkInfoClassCommand(fileName);		
			passed &= PortableHeapdumpTest1PackedPrimitivesChecker.checkXJClassCommand(fileName);
			
			/**
			 * Test 2. Check array of PackedPrimitives
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     2. Check array of PackedPrimitives\n");
			System.err.println("**************************************************");
			passed &= PortableHeapdumpTest2ArrayOfPackedPrimitivesChecker.checkInfoClassCommand(fileName);
			passed &= PortableHeapdumpTest2ArrayOfPackedPrimitivesChecker.checkXJClassCommand(fileName);
			
			/**
			 * Test 3. Check PackedMixed
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     3. Check PackedMixed\n");
			System.err.println("**************************************************");
			passed &= PortableHeapdumpTest3PackedMixedChecker.checkInfoClassCommand(fileName);
			passed &= PortableHeapdumpTest3PackedMixedChecker.checkXJClassCommand(fileName);
			
			
			/**
			 * Test 4. Check array of PackedMixed
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     4. Check array of PackedMixed\n");
			System.err.println("**************************************************");
			passed &= PortableHeapdumpTest4ArrayOfPackedMixedOneRefChecker.checkInfoClassCommand(fileName);
			passed &= PortableHeapdumpTest4ArrayOfPackedMixedOneRefChecker.checkXJClassCommand(fileName);
			
			/**
			 * Test 5. Check array of PackedMixedTwoRefs
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     5. Check PackedMixedTwoRefs\n");
			System.err.println("**************************************************");
			passed &= PortableHeapdumpTest5ArrayOfPackedMixedTwoRefsChecker.checkXJClassCommand(fileName);
			
			/**
			 * Test 6. Check PackedMixedWithReferenceToSelf
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     6. Check PackedMixedWithReferenceToSelf\n");
			System.err.println("**************************************************");
			passed &= PortableHeapdumpTest6PackedMixedWithReferenceToSelfChecker.checkXJClassCommand(fileName);
						
			/**
			 * Test 7. Check NestedPacked
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     7. Check NestedPacked\n");
			System.err.println("**************************************************");
			passed &= PortableHeapdumpTest7NestedPackedChecker.checkInfoClassCommand(fileName);
			passed &= PortableHeapdumpTest7NestedPackedChecker.checkXJClassCommand(fileName);
			
			/**
			 * Test 8. Check PackedIntsArray
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     8. Check PackedIntsArray\n");
			System.err.println("**************************************************");
			passed &= PortableHeapdumpTest8PackedIntsArrayChecker.checkInfoClassCommand(fileName);			
			passed &= PortableHeapdumpTest8PackedIntsArrayChecker.checkXJClassCommand(fileName);

			/**
			 * Test 9. Check array of PackedMixedArrayElement
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     9. Check array of PackedMixedArrayElement\n");
			System.err.println("**************************************************");		
			passed &= PortableHeapdumpTest9ArrayOfPackedMixedArrayElementChecker.checkXJClassCommand(fileName);
			
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
			System.err.println("Test passed.");
			System.exit(0);
		} else {
			System.err.println("Test failed.");
			System.exit(1);
		}
	}





}
