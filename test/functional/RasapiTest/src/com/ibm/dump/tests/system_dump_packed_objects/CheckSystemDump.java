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


import java.io.FileNotFoundException;
import java.io.IOException;

/**
 * This class checks the output from jdmpview run against a PHD containing packed objects
 * 
 * @author matthew
 * 
 */
public class CheckSystemDump {

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
			passed &= SystemDumpTest1PackedPrimitivesChecker.checkInfoClassCommand(fileName);		
			passed &= SystemDumpTest1PackedPrimitivesChecker.checkXJClassCommand(fileName);
			
			/**
			 * Test 2. Check array of PackedPrimitives
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     2. Check array of PackedPrimitives\n");
			System.err.println("**************************************************");
			passed &= SystemDumpTest2ArrayOfPackedPrimitivesChecker.checkInfoClassCommand(fileName);
			passed &= SystemDumpTest2ArrayOfPackedPrimitivesChecker.checkXJClassCommand(fileName);
			
			/**
			 * Test 3. Check PackedMixed
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     3. Check PackedMixedOneRef\n");
			System.err.println("**************************************************");
			passed &= SystemDumpTest3PackedMixedOneRefChecker.checkInfoClassCommand(fileName);
			passed &= SystemDumpTest3PackedMixedOneRefChecker.checkXJClassCommand(fileName);
			
			
			/**
			 * Test 4. Check array of PackedMixed
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     4. Check array of PackedMixed\n");
			System.err.println("**************************************************");
			passed &= SystemDumpTest4ArrayOfPackedMixedOneRefChecker.checkInfoClassCommand(fileName);
			passed &= SystemDumpTest4ArrayOfPackedMixedOneRefChecker.checkXJClassCommand(fileName);
			
			/**
			 * Test 5. Check array of PackedMixedTwoRefs
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     5. Check PackedMixedTwoRefs\n");
			System.err.println("**************************************************");
			passed &= SystemDumpTest5ArrayOfPackedMixedTwoRefsChecker.checkXJClassCommand(fileName);
			
			/**
			 * Test 6. Check PackedMixedWithReferenceToSelf
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     6. Check PackedMixedWithReferenceToSelf\n");
			System.err.println("**************************************************");
			passed &= SystemDumpTest6PackedMixedWithReferenceToSelfChecker.checkXJClassCommand(fileName);
						
			/**
			 * Test 7. Check NestedPacked
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     7. Check NestedPacked and NestedPacked1\n");
			System.err.println("**************************************************");
			passed &= SystemDumpTest7NestedPackedChecker.checkInfoClassCommand(fileName);
			passed &= SystemDumpTest7NestedPackedChecker.checkXJClassCommand(fileName);
			passed &= SystemDumpTest71NestedPacked1Checker.checkXJClassCommand(fileName);
			
			/**
			 * Test 8. Check NestedPackedMixed
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     8. Check NestedPackedMixed\n");
			System.err.println("**************************************************");
			passed &= SystemDumpTest8NestedPackedMixedChecker.checkInfoClassCommand(fileName);
			passed &= SystemDumpTest8NestedPackedMixedChecker.checkXJClassCommand(fileName);
			
			/**
			 * Test 9. Check PackedIntsArray
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     9. Check PackedIntsArray\n");
			System.err.println("**************************************************");
			passed &= SystemDumpTest9PackedIntsArrayChecker.checkInfoClassCommand(fileName);			
			passed &= SystemDumpTest9PackedIntsArrayChecker.checkXJClassCommand(fileName);

			/**
			 * Test 10. Check PackedIntsArrayArray and derived object
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     10. Check PackedIntsArrayArray and derived object\n");
			System.err.println("**************************************************");
			passed &= SystemDumpTest10PackedIntsArrayArrayChecker.checkInfoClassCommand(fileName);			
			passed &= SystemDumpTest10PackedIntsArrayArrayChecker.checkXJClassCommand(fileName);
			passed &= SystemDumpTest101DerivedPackedIntsArrayArrayChecker.checkXJClassCommand(fileName);
			
			/**
			 * Test 11. Check array of PackedMixedWithReferenceToSelf
			 */
			System.err.println("\n**************************************************\n");
			System.err.println("*     11. Check array of PackedMixedWithReferenceToSelf\n");
			System.err.println("**************************************************");		
			passed &= SystemDumpTest11ArrayOfPackedMixedArrayElementChecker.checkXJClassCommand(fileName);
			
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
