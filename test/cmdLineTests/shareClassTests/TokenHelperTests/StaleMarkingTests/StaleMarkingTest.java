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

package StaleMarkingTests;

import java.io.FileInputStream;
import java.net.URL;
import java.util.Properties;

import CustomClassloaders.CustomTokenClassLoader;
import Utilities.StringManipulator;
import Utilities.TestClass;
import Utilities.URLClassPathCreator;

public class StaleMarkingTest {
	
	StringManipulator manipulator = new StringManipulator();

	public static void main(String[] args) {
		
		if(args.length != 2){
			System.out.println("\n Incorrect usage");
			System.out.println("\n Please specifiy -testfile <filename>");
		}
		
		StaleMarkingTest test = new StaleMarkingTest();
		
		String testFile = args[1];
		
		test.run(testFile);
	}
	
	public void run(String testFile){
		
		Properties props = new Properties();
		try{
			FileInputStream PropertiesFile = new FileInputStream(testFile);
			props.load(PropertiesFile);
			
			PropertiesFile.close();
		} catch (Exception e){
			e.printStackTrace();
		}
		
		String clc = props.getProperty("NumberOfClassloaders");
		Integer c = new Integer(clc);
		int classloaderCount = c.intValue();
		String [] classLoaderPaths = new String[classloaderCount];
		int maxClassesToLoad =0;
		for(int index = 0; index < classloaderCount; index++){
			classLoaderPaths[index] = props.getProperty("ClassPath"+index);
			String ctl = props.getProperty("NumberOfClassesToLoad"+index);
			Integer intctl = new Integer(ctl);
			maxClassesToLoad = ((intctl.intValue() > maxClassesToLoad) ? intctl.intValue() : maxClassesToLoad);
		}

		String [][] classesToLoad = new String[classloaderCount][maxClassesToLoad];
		for(int loaderIndex = 0; loaderIndex < classloaderCount; loaderIndex++){
			String nctls = props.getProperty("NumberOfClassesToLoad"+loaderIndex);
			Integer i = new Integer(nctls);
			int classesToLoadCount = i.intValue();
			String classesString = props.getProperty("ClassesToLoad"+loaderIndex);
			for (int classesIndex = 0; classesIndex < classesToLoadCount; classesIndex++){
				classesToLoad[loaderIndex][classesIndex] = manipulator.getStringElement(classesIndex, classesString);
			}
		}
		
		String findPath = props.getProperty("FindPath");
		String ctf = props.getProperty("NumberOfClassesToFind");
		Integer intctf = new Integer(ctf);
		int numberOfClassesToFind = intctf.intValue();
		
		String classesString = props.getProperty("FindClasses");
		String [] classesToFind = new String[numberOfClassesToFind];
		String resultsString = props.getProperty("Results");
		String[] results = new String[numberOfClassesToFind];
		String foundAtString = props.getProperty("FoundAt");
		String[] foundAt = new String[numberOfClassesToFind];
		for(int index = 0; index < numberOfClassesToFind; index++){
			classesToFind[index] = manipulator.getStringElement(index, classesString);
			results[index] = manipulator.getStringElement(index, resultsString);
			foundAt[index] = manipulator.getStringElement(index, foundAtString);
		}
		
		boolean passed = executeTest(classLoaderPaths, classesToLoad, findPath, classesToFind, results, foundAt);
		
		if(passed){
			System.out.println("\nTEST PASSED");
		} else {
			System.out.println("\nTEST FAILED");
		}
	}
	
	public boolean executeTest(String[] loaderPaths, String[][] classesToLoad, String findPath, String[] classesToFind, String[] results, String[] foundAt){
		
		for(int index = 0; index < loaderPaths.length; index++){
			URLClassPathCreator creator = new URLClassPathCreator(loaderPaths[index]);
			URL[] urlPath;
			urlPath = creator.createURLClassPath();
			
			CustomTokenClassLoader cl = new CustomTokenClassLoader(urlPath, this.getClass().getClassLoader());
			cl.setToken("StaleMarking");
			for(int classIndex = 0; classIndex < classesToLoad[index].length; classIndex++){
				String classToLoad = classesToLoad[index][classIndex];
				if (classToLoad != null){
					try{
						cl.loadClassNoCache(classToLoad);
					} catch (Exception e){
						e.printStackTrace();
					}
				}
			}
		}
		
		boolean result = true;
		
		URLClassPathCreator creator = new URLClassPathCreator(findPath);
		URL[] findPathURLS;
		findPathURLS = creator.createURLClassPath();
		
		CustomTokenClassLoader vf = new CustomTokenClassLoader(findPathURLS, this.getClass().getClassLoader());
		vf.setToken("StaleMarking");
		for(int classIndex = 0; classIndex < classesToFind.length; classIndex++){
			String classToFind = classesToFind[classIndex];
			String expectedResult = results[classIndex];
			if (classToFind != null){
				String testResult = String.valueOf(vf.isClassInSharedCache("StaleMarking", classToFind));
				if(!(expectedResult.equals(testResult))){
					System.out.println("\nFailure finding class: "+classToFind+" result: "+testResult+" expecting: "+expectedResult);
					result = false;
				} else {
					if(testResult.equals("true")){
						result = validateReturnedClass(classToFind, foundAt[classIndex], vf);
					}
				}
			}
		}
		return result;
	}
	
	boolean validateReturnedClass(String className, String foundAt, CustomTokenClassLoader loader){
		boolean result = false;
		Class classData = null;
		classData = loader.getClassFromCache(className);
		if(null != classData){
			Object o = null;
			try{
				o = classData.newInstance();
			} catch(Exception e){
				e.printStackTrace();
			}
			TestClass tc = (TestClass)o;
			String classLocation = tc.getLocation();
			if(classLocation.equals(foundAt)){
				result = true;
			} else {
				System.out.println("\nClass location: "+classLocation+" expecting: "+foundAt);
			}
		}
		return result;
	}
}
