/*******************************************************************************
 * Copyright (c) 2005, 2018 IBM Corp. and others
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
package Utilities;

import java.io.FileInputStream;
import java.net.URL;
import java.util.Properties;

import CustomClassloaders.CustomURLClassLoader;

/**
 * @author Matthew Kilner
 */
public class Verifier {
	
	StringManipulator manipulator = new StringManipulator();

	public static void main(String[] args) {
		
		if(args.length != 2){
			System.out.println("\n Incorrect usage");
			System.out.println("\n Please specifiy -testfile <filename>");
		}
		
		Verifier test = new Verifier();
		
		String testFile = args[1];
		
		test.testWrapper(testFile);	
		
	}
	
	public void testWrapper(String testFileName){
		
		System.out.println("\n** Running Verifier for properties: "+testFileName+"\n");
		
		Properties props = new Properties();
		try{
			FileInputStream PropertiesFile = new FileInputStream(testFileName);
			props.load(PropertiesFile);
			
			PropertiesFile.close();
		} catch (Exception e){
			e.printStackTrace();
		}
		
		String classPath = props.getProperty("ClassPath");
		
		String nctls = props.getProperty("NumberOfClassesToVerify");
		Integer i = new Integer(nctls);
		int classesToVerifyCount = i.intValue();
		
		String[] classesToVerify = new String[classesToVerifyCount];
		String classesString = props.getProperty("ClassesToVerify");
		for(int index = 0; index < classesToVerifyCount; index ++){
			classesToVerify[index] = manipulator.getStringElement(index, classesString);
		}
		
		String[] results = new String[classesToVerifyCount];
		String verifiersResults = props.getProperty("Results");
		for(int index = 0; index < classesToVerifyCount; index++){
			results[index] = manipulator.getStringElement(index, verifiersResults);
		}
		
		boolean passed = executeTest(classPath, classesToVerify, results);
		
		if(passed){
			System.out.println("\nTEST PASSED");
		} else {
			System.out.println("\nTEST FAILED");
		}
	}
	
	public boolean executeTest(String classpath, String[] classesToVerify, String[] results){
		
		System.out.println("\nCreating Verifier.....");
		URLClassPathCreator vfPathCreator = new URLClassPathCreator(classpath);
		URL[] vfPath;
		vfPath = vfPathCreator.createURLClassPath();
		CustomURLClassLoader verifier = new CustomURLClassLoader(vfPath, this.getClass().getClassLoader());
		
		System.out.println("\nRunning Verifier.....");
		boolean result = true;
		for(int cIndex = 0; cIndex < classesToVerify.length; cIndex++){
			String className = classesToVerify[cIndex];
			if(className != null){
				String expectedResult = results[cIndex];
				String testResult = String.valueOf(verifier.isClassInSharedCache(className));
				if(!(expectedResult.equals(testResult))){
					System.out.println("\nFailure testing class: "+className+" result: "+testResult+" expecting: "+expectedResult);
					result = false;
				}
			}
		}
		return result;
	}
}
