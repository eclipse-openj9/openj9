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
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.net.URL;
import java.util.Properties;

import CustomClassloaders.CustomPartitioningURLClassLoader;
import CustomClassloaders.CustomPartitioningURLLoader;
import Utilities.StringManipulator;
import Utilities.TestClass;
import Utilities.URLClassPathCreator;

/**
 * @author Matthew Kilner
 */
public class PartitioningURLHelperURLClassPathHelperStaleEntryCompatibilityTest {

	StringManipulator manipulator = new StringManipulator();
	
	public static void main(String[] args) {
	
		if(args.length != 4){
			System.out.println("\n Incorrect usage");
			System.out.println("\n Please specifiy -testfile <filename> -javacdir <path to javac>");
		}
		
		PartitioningURLHelperURLClassPathHelperStaleEntryCompatibilityTest test = new PartitioningURLHelperURLClassPathHelperStaleEntryCompatibilityTest();
		
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
		
		String numberOfUrlsString = props.getProperty("NumberOfUrls");
		Integer tempNumberOfUrls = new Integer(numberOfUrlsString);
		int numberOfUrls = tempNumberOfUrls.intValue();
		
		int maxClassesToLoad = 0;		
		String[] urls = new String[numberOfUrls];
		String [] partitionStrings = new String[numberOfUrls];
		for(int index = 0; index < numberOfUrls; index++){
			urls[index] = props.getProperty("Url"+index);
			partitionStrings[index] = props.getProperty("urlPartition"+index);
			String ctl = props.getProperty("NumberOfClassesToLoad"+index);
			Integer intctl = new Integer(ctl);
			maxClassesToLoad = ((intctl.intValue() > maxClassesToLoad) ? intctl.intValue() : maxClassesToLoad);
		}
		
		String[][] classesToLoad = new String[numberOfUrls][maxClassesToLoad];
		
		for(int urlIndex = 0; urlIndex < numberOfUrls; urlIndex++){
			String loadClasses = props.getProperty("LoadClasses"+urlIndex);
			String ctl = props.getProperty("NumberOfClassesToLoad"+urlIndex);
			Integer intctl = new Integer(ctl);
			int numberOfClassesToLoad = intctl.intValue();
			for(int classToLoadIndex = 0; classToLoadIndex < numberOfClassesToLoad; classToLoadIndex++){
				classesToLoad[urlIndex][classToLoadIndex] = manipulator.getStringElement(classToLoadIndex, loadClasses);
			}
		}
		
		String classPath = props.getProperty("Classpath");
		String partition = props.getProperty("Partition");
		
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
		
		String batchFile = props.getProperty("BatchFileToRun");

		boolean passed = executeTest(urls, partitionStrings, classesToLoad, classPath, partition, classesToFind, results, batchFile, foundAt, javacpath);
		
		if(passed){
			System.out.println("\nTEST PASSED");
		} else {
			System.out.println("\nTEST FAILED");
		}
	}

	private boolean executeTest(String[] urls, String[] partitionStrings, String[][] classesToLoad, String classPath, String partition, String[] classesToFind, String[] results, String batchFile, String[] foundAt, String javacpath) {
		
		String urlsString = urls[0];
		for(int index = 1; index < urls.length; index++){
			urlsString = new StringBuffer(urls[index].length() + 1).append(urlsString).append(urls[index]).append(File.pathSeparatorChar).toString();
		}
		System.out.println("\n** urlsString: "+urlsString);
		
		URLClassPathCreator creator = new URLClassPathCreator(urlsString);
		URL[] urlPath;
		urlPath = creator.createURLClassPath();
		CustomPartitioningURLLoader[] loaderArray = new CustomPartitioningURLLoader[urls.length];
		
		for(int urlIndex = 0; urlIndex < urls.length; urlIndex++){
			loaderArray[urlIndex] = new CustomPartitioningURLLoader(urlPath, this.getClass().getClassLoader());
			loaderArray[urlIndex].setPartition(partitionStrings[urlIndex]);
			for(int classIndex = 0; classIndex < classesToLoad[urlIndex].length; classIndex++){
				String classToLoad = classesToLoad[urlIndex][classIndex];
				if(classToLoad != null){
					loaderArray[urlIndex].loadClassFrom(classToLoad, urlIndex);
				}
			}
		}
		
		if(0 != batchFile.length()){
			runBatchFile(batchFile, javacpath);
		}
		
		boolean result = true;
		
		URLClassPathCreator creator2 = new URLClassPathCreator(classPath);
		URL[] urlPath2;
		urlPath2 = creator2.createURLClassPath();
		
		CustomPartitioningURLClassLoader cl = new CustomPartitioningURLClassLoader(urlPath2, this.getClass().getClassLoader());
		cl.setPartition(partition);
		for(int classIndex = 0; classIndex < classesToLoad.length; classIndex++){
			String classToFind = classesToFind[classIndex];
			String expectedResult = results[classIndex];
			if (classToFind != null){
				String testResult = String.valueOf(cl.isClassInSharedCache(classToFind));
				if(!(expectedResult.equals(testResult))){
					System.out.println("\nFailure finding class: "+classToFind+" result: "+testResult+" expecting: "+expectedResult);
					result = false;
				} else {
					if(testResult.equals("true")){
						result = validateReturnedClass(classToFind, foundAt[classIndex], cl);
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
	
	boolean validateReturnedClass(String className, String foundAt, CustomPartitioningURLClassLoader loader){
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
