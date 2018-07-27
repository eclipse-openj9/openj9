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
import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.net.URL;
import java.util.Properties;

import CustomClassloaders.CustomURLClassLoader;
import CustomClassloaders.CustomURLLoader;
import Utilities.StringManipulator;
import Utilities.URLClassPathCreator;

/**
 * @author Matthew Kilner
 */
public class URLClassPathHelperURLHelperStaleEntryCompatibilityTest {

	StringManipulator manipulator = new StringManipulator();
	
	public static void main(String[] args) {
	
		if(args.length != 4){
			System.out.println("\n Incorrect usage");
			System.out.println("\n Please specifiy -testfile <filename> -javacdir <path to javac>");
		}
		
		URLClassPathHelperURLHelperStaleEntryCompatibilityTest test = new URLClassPathHelperURLHelperStaleEntryCompatibilityTest();
		
		String testFile = args[1];
		String javacdir = args[3];
		
		test.run(testFile, javacdir);
		
	}
	
	public void run(String testFile, String javacpath){
		
		Properties props = new Properties();
		try{
			FileInputStream PropertiesFile = new FileInputStream(testFile);
			props.load(PropertiesFile);
			
			PropertiesFile.close();
		} catch (Exception e){
			e.printStackTrace();
		}
		
		String classPath = props.getProperty("Classpath");
		
		String ctl = props.getProperty("NumberOfClassesToLoad");
		Integer intctl = new Integer(ctl);
		int numberOfClassesToLoad = intctl.intValue();
		
		String classesString = props.getProperty("LoadClasses");
		String [] classesToLoad = new String[numberOfClassesToLoad];
		for(int index = 0; index < numberOfClassesToLoad; index++){
			classesToLoad[index] = manipulator.getStringElement(index, classesString);
		}
		
		String numberOfUrlsString = props.getProperty("NumberOfUrls");
		Integer tempNumberOfUrls = new Integer(numberOfUrlsString);
		int numberOfUrls = tempNumberOfUrls.intValue();
		
		int maxClassesToFind = 0;		
		String[] urls = new String[numberOfUrls];
		for(int index = 0; index < numberOfUrls; index++){
			urls[index] = props.getProperty("Url"+index);
			String ctf = props.getProperty("NumberOfClassesToFind"+index);
			Integer intctf = new Integer(ctf);
			maxClassesToFind = ((intctf.intValue() > maxClassesToFind) ? intctf.intValue() : maxClassesToFind);
		}
		
		String[][] classesToFind = new String[numberOfUrls][maxClassesToFind];
		String[][] results = new String[numberOfUrls][maxClassesToFind];
		
		for(int urlIndex = 0; urlIndex < numberOfUrls; urlIndex++){
			String findClasses = props.getProperty("FindClasses"+urlIndex);
			String result = props.getProperty("Results"+urlIndex);
			String ctf = props.getProperty("NumberOfClassesToFind"+urlIndex);
			Integer intctf = new Integer(ctf);
			int numberOfClassesToFind = intctf.intValue();
			for(int classToFindIndex = 0; classToFindIndex < numberOfClassesToFind; classToFindIndex++){
				classesToFind[urlIndex][classToFindIndex] = manipulator.getStringElement(classToFindIndex, findClasses);
				results[urlIndex][classToFindIndex] = manipulator.getStringElement(classToFindIndex, result);
			}
		}
		
		String batchFile = props.getProperty("BatchFileToRun");
		
		boolean passed = executeTest(classPath, classesToLoad, urls, classesToFind, results, batchFile, javacpath);
		
		if(passed){
			System.out.println("\nTEST PASSED");
		} else {
			System.out.println("\nTEST FAILED");
		}
	}

	private boolean executeTest(String classPath, String[] classesToLoad, String[] urls, String[][] classesToFind, String[][] results, String batchFile, String javacpath) {
		
		URLClassPathCreator creator = new URLClassPathCreator(classPath);
		URL[] urlPath;
		urlPath = creator.createURLClassPath();
		
		CustomURLClassLoader cl = new CustomURLClassLoader(urlPath, this.getClass().getClassLoader());
		for(int classIndex = 0; classIndex < classesToLoad.length; classIndex++){
			String classToLoad = classesToLoad[classIndex];
			if (classToLoad != null){
				try{
					cl.loadClass(classToLoad);
				} catch (Exception e){
					e.printStackTrace();
				}
			}
		}
		
		runBatchFile(batchFile, javacpath);
		
		String urlsString = urls[0];
		for(int index = 1; index < urls.length; index++){
			urlsString = new StringBuffer(urls[index].length() + 1).append(urlsString).append(urls[index]).toString();
		}
		System.out.println("\n** urlsString: "+urlsString);
		URLClassPathCreator creator2 = new URLClassPathCreator(urlsString);
		URL[] urlPath2;
		urlPath2 = creator2.createURLClassPath();
		CustomURLLoader urlcl = new CustomURLLoader(urlPath2, this.getClass().getClassLoader());
		
		boolean result = true;
		
		for(int urlIndex = 0; urlIndex < urls.length; urlIndex++){
			for(int classIndex = 0; classIndex < classesToFind[urlIndex].length; classIndex++){
				String classToFind = classesToFind[urlIndex][classIndex];
				String expectedResult = results[urlIndex][classIndex];
				if(classToFind != null){
					String testResult = String.valueOf(urlcl.isClassInSharedCache(urlIndex, classToFind));
					if(!(expectedResult.equals(testResult))){
						System.out.println("\nFailure finding class: "+classToFind+" on path: "+urls[urlIndex]+" which is index: "+urlIndex+" result: "+testResult+" expecting: "+expectedResult);
						result = false;
					}
				}
			}
		}
		return result;
	}
	
	private void runBatchFile(String batch, String javacpath){
		String command = new StringBuffer(batch.length()+javacpath.length()+1).append(batch).append(" ").append(javacpath).toString();
		System.out.println("\n** Running: "+command);
		String s = null;
		try{
			Process p = Runtime.getRuntime().exec(command);
			
			BufferedReader stdInput = new BufferedReader(new InputStreamReader(p.getInputStream()));
			BufferedReader stdError = new BufferedReader(new InputStreamReader(p.getErrorStream()));
			
			System.out.println("Here is the standard output of the command:\n");
            while ((s = stdInput.readLine()) != null) {
                System.out.println(s);
            }
            
            System.out.println("Here is the standard error of the command (if any):\n");
            while ((s = stdError.readLine()) != null) {
                System.out.println(s);
            }
			
		} catch (Exception e){
			e.printStackTrace();
		}
	}
}
