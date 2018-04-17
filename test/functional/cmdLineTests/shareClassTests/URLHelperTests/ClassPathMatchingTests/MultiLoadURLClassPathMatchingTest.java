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
package ClassPathMatchingTests;

import java.io.File;
import java.io.FileInputStream;
import java.net.URL;
import java.util.Properties;

import CustomClassloaders.CustomURLLoader;
import Utilities.StringManipulator;
import Utilities.URLClassPathCreator;

/**
 * @author Matthew Kilner
 */
public class MultiLoadURLClassPathMatchingTest {
	
	StringManipulator manipulator = new StringManipulator();

	public static void main(String[] args) {
		
		if(args.length != 2){
			System.out.println("\n Incorrect usage");
			System.out.println("\n Please specifiy -testfile <filename>");
		}
		
		MultiLoadURLClassPathMatchingTest test = new MultiLoadURLClassPathMatchingTest();
		
		String testFile = args[1];
		
		test.run(testFile);
	}
	
	public void run(String testFileName){
		
		Properties props = new Properties();
		try{
			FileInputStream PropertiesFile = new FileInputStream(testFileName);
			props.load(PropertiesFile);
			
			PropertiesFile.close();
		} catch (Exception e){
			e.printStackTrace();
		}

		String numberOfUrlsString = props.getProperty("NumberOfUrls");
		Integer tempNumberOfUrls = new Integer(numberOfUrlsString);
		int numberOfUrls = tempNumberOfUrls.intValue();
		
		int maxClassesToLoad = 0;
		int maxClassesToFind = 0;		
		String[] urls = new String[numberOfUrls];
		for(int index = 0; index < numberOfUrls; index++){
			urls[index] = props.getProperty("Url"+index);
			String ctl = props.getProperty("NumberOfClassesToLoad"+index);
			Integer intctl = new Integer(ctl);
			maxClassesToLoad = ((intctl.intValue() > maxClassesToLoad) ? intctl.intValue() : maxClassesToLoad);
			String ctf = props.getProperty("NumberOfClassesToFind"+index);
			Integer intctf = new Integer(ctf);
			maxClassesToFind = ((intctf.intValue() > maxClassesToFind) ? intctf.intValue() : maxClassesToFind);
		}
		
		String[][] classesToLoad = new String[numberOfUrls][maxClassesToLoad];
		String[][] classesToFind = new String[numberOfUrls][maxClassesToFind];
		String[][] results = new String[numberOfUrls][maxClassesToFind];
		
		for(int urlIndex = 0; urlIndex < numberOfUrls; urlIndex++){
			String loadClasses = props.getProperty("LoadClasses"+urlIndex);
			String findClasses = props.getProperty("FindClasses"+urlIndex);
			String result = props.getProperty("Results"+urlIndex);
			String ctl = props.getProperty("NumberOfClassesToLoad"+urlIndex);
			Integer intctl = new Integer(ctl);
			int numberOfClassesToLoad = intctl.intValue();
			String ctf = props.getProperty("NumberOfClassesToFind"+urlIndex);
			Integer intctf = new Integer(ctf);
			int numberOfClassesToFind = intctf.intValue();
			for(int classToLoadIndex = 0; classToLoadIndex < numberOfClassesToLoad; classToLoadIndex++){
				classesToLoad[urlIndex][classToLoadIndex] = manipulator.getStringElement(classToLoadIndex, loadClasses);
			}
			for(int classToFindIndex = 0; classToFindIndex < numberOfClassesToFind; classToFindIndex++){
				classesToFind[urlIndex][classToFindIndex] = manipulator.getStringElement(classToFindIndex, findClasses);
				results[urlIndex][classToFindIndex] = manipulator.getStringElement(classToFindIndex, result);
			}
		}
	
		boolean passed = executeTest(urls, classesToLoad, classesToFind, results);
		
		if(passed){
			System.out.println("\nTEST PASSED");
		} else {
			System.out.println("\nTEST FAILED");
		}	
	}
	
	public boolean executeTest(String[] urls, String[][] classesToLoad, String[][] classesToFind, String[][] results){
	
		String urlsString = urls[0];
		for(int index = 0; index < urls.length; index++){
			urlsString = new StringBuffer(urls[index].length() + 1).append(urlsString).append(urls[index]).append(File.pathSeparatorChar).toString();
		}
		
		URLClassPathCreator creator = new URLClassPathCreator(urlsString);
		URL[] urlPath;
		urlPath = creator.createURLClassPath();
		CustomURLLoader[] loaderArray = new CustomURLLoader[urls.length];
		
		for(int urlIndex = 0; urlIndex < urls.length; urlIndex++){
			for(int classIndex = 0; classIndex < classesToLoad[urlIndex].length; classIndex++){
				loaderArray[urlIndex] = new CustomURLLoader(urlPath, this.getClass().getClassLoader());
				String classToLoad = classesToLoad[urlIndex][classIndex];
				if(classToLoad != null){
					loaderArray[urlIndex].loadClassFrom(classToLoad, urlIndex);
				}
			}
		}
		
		boolean result = true;
		
		for(int urlIndex = 0; urlIndex < urls.length; urlIndex++){
			for(int classIndex = 0; classIndex < classesToFind[urlIndex].length; classIndex++){
				String classToFind = classesToFind[urlIndex][classIndex];
				String expectedResult = results[urlIndex][classIndex];
				if(classToFind != null){
					String testResult = String.valueOf(loaderArray[urlIndex].isClassInSharedCache(urlIndex, classToFind));
					if(!(expectedResult.equals(testResult))){
						System.out.println("\nFailure finding class: "+classToFind+" on path: "+urls[urlIndex]+" which is index: "+urlIndex+" result: "+testResult+" expecting: "+expectedResult);
						result = false;
					}
				}
			}
		}
		return result;
	}
}
