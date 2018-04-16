package com.ibm.j9.offload.tests.mapping;

/*******************************************************************************
 * Copyright (c) 2009, 2012 IBM Corp. and others
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

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintWriter;
import junit.framework.Test;
import junit.framework.TestSuite;
import junit.framework.TestCase;

/**
 * This test must be run with two arguments.  
 * 
 * The first is the directory in which the binaries for the vm are located.  
 * For example
 * 	../jre/lib/ppc64 - linux ppc 64
 *  ../jre/bin - windows
 *  
 * The second is the location of the jars that will be used for the tests.  The contents of this directory 
 * should be as follows:
 * 
 * 	version1\mapjar.jar
 *  version2\mapjar.jar
 *  noClass\mapjar.jar
 *  
 *  mapjar.jar in version1 should contain only the class com.ibm.j9.offload.testing.mapping.TestClass where the version returned by toString is TestClass[VERSION1]
 *  mapjar.jar in version1 should contain only the class com.ibm.j9.offload.testing.mapping.TestClass where the version returned by toString is TestClass[VERSION2]
 *  mapjar.jar in version1 should contain only the class com.ibm.j9.offload.testing.mapping.NoClass
 *  
 *  These were made manually and added to cs.opensource using the tree in the j9vm directory.  The version returned by 
 *  com.ibm.j9.offload.testing.mapping.TestClass in j9vm.jar must be TestClass[VERSION0]
 *  
 */
public class RunMappingTest extends TestCase {
	
	static String[] args = null;
	static String testOutput;

	/**
	 * This is used to capture the output from an invocation of the jvm with a given classpath and 
     * directory mapping 
     */
	class ProcessOutputThread extends Thread {
		InputStream theInput;
		
		ProcessOutputThread(InputStream theOutput){
			this.theInput = theOutput;
		}
		
		public void run() {
			try {
				byte buffer[] = new byte[1000];
				while(true){
					int length = theInput.read(buffer, 0,1000);
					if (length >0){
						String receivedData = new String(buffer,0,length);
						testOutput = testOutput + receivedData;
					} else {
						break;
					}
				}
			} catch (Exception e) {
				e.printStackTrace();
			}
		}
	}
	
	/**
	 * method that can be used to run the test by itself
	 * 
	 * @param args the arguments passed to the test as outlined in the comments above
	 */
	public static void main (String[] args) {
		RunMappingTest.args = args;
		if (args.length != 2){
			System.out.println("Invalid Arguments!");
			System.out.println("First argument must be the directory in which the binaries for the vm are located");
			System.out.println("Second argument must be the directory in which the different versions of the jars to be mapped are located");
		}
		junit.textui.TestRunner.run(suite());
	}
	
	public static Test suite(){
		return new TestSuite(RunMappingTest.class);
	}
	
	
	/** 
     * this method is called to set the contents of the mapping file 
     * @param mappingLines array of strings, each string will be written as one line in the mapping file 
     */
	public static void setMappingFile(String mappingLines[]) throws FileNotFoundException, IOException {
		File mappingFile = new File(getVMdir() + File.separator + "mappedDirectories.properties");
		System.out.println("Going to set mapping file:" + mappingFile.getAbsolutePath());
		if (mappingFile.exists()){
			mappingFile.delete();
		}
		mappingFile.createNewFile();
		PrintWriter printer = new PrintWriter(mappingFile);
		for(int i=0;i<mappingLines.length;i++){
			System.out.println("Wrote:" + mappingLines[i]);
			printer.println(mappingLines[i]);
		}
		printer.flush();
		printer.close();
	}
	
	/**
     * returns the directory in which the binaries for the vm are located which is also where
     * the mappedDirectories.properties file is located
     * 
     * @return the path to the directory that contains the binaries for the vm
     */
	public static String getVMdir(){
		return args[0];
	}
	
    /**
     * returns the directory that hold the different versions of the jars that will be 
     * mapped
     * 
     * @return the path to the directory that contains the different versions of the jars that will be mapped
     */
	public static String getMappedFileRoot(){
		if (!File.separator.equals("/")){
			// because windows uses the other path separator but our scripts run in a shell that uses the other
			// we have to make sure we fix up any which are not right
			return args[1].replace('/', '\\');
		} else {
			return args[1];
		}
	}
	
	
	/* test case were we ask for a class that cannot be found */
	public void testClassNotFound(){
		try {
			testOutput = "";
			String javaExecuteCommand = System.getProperty("user.dir") + File.separator + "java -Xint -Xproxy:auto -cp " +  System.getProperty("java.class.path") + " com.ibm.j9.offload.tests.mapping.LoadClass" + " notAClass";
			System.out.println("javaExecuteCommand:"+ javaExecuteCommand);
			Process p1 = Runtime.getRuntime().exec(javaExecuteCommand);
			ProcessOutputThread t = new ProcessOutputThread(p1.getInputStream());
			t.start();
			assertTrue("Did not get expected 0 response code", p1.waitFor()==0);
			assertTrue("Found class that we should not have",testOutput.indexOf("NOT FOUND") != -1);
		} catch (Exception e) {
			fail("Unexpected Exception:" + e);
		}
	}
	
	/* test case were we ask for a class  when there is mapping present but the class in not in the mapped file*/
	public void testClassFoundMappingPresent(){
		try {
			testOutput = "";
			String mapping[] = {new String(System.getProperty("user.dir")+ File.separator + "nojar.jar" + "=" + System.getProperty("user.dir")+ File.separator + "nojar.jar")};
			setMappingFile(mapping);
			String javaExecuteCommand = System.getProperty("user.dir") + File.separator + "java -Xint -Xproxy:auto -cp " +  getMappedFileRoot() + File.separator + "version1" + File.separator + "mapjar.jar" + System.getProperty("path.separator") + System.getProperty("java.class.path") + " com.ibm.j9.offload.tests.mapping.LoadClass" + " com.ibm.j9.offload.tests.mapping.TestClass";
			System.out.println("javaExecuteCommand:"+ javaExecuteCommand);
			Process p1 = Runtime.getRuntime().exec(javaExecuteCommand);
			ProcessOutputThread t = new ProcessOutputThread(p1.getInputStream());
			t.start();
			assertTrue("Did not get expected 0 response code", p1.waitFor()==0);
			assertTrue("Did not find class :" + testOutput,testOutput.indexOf("!FOUND!") != -1);
			assertTrue("Did not load right class when jar was mapped:" + testOutput,testOutput.indexOf("TestClass[VERSION1]") != -1);
		} catch (Exception e) {
			e.printStackTrace();
			fail("Unexpected Exception:" + e);
		}
	}
	
	/* test case were we ask for a class that was mapped and is found 
	 * when different versions of the class exist in the base jar and the jar it is mapped to 
	 * make sure we load the version from the mapped file
	 * this version tests when the mapping is specified as a jar file*/
	public void testClassFoundWhenMapped(){
		try {
			testOutput = "";
			String mapping[] = {new String(getMappedFileRoot()+ File.separator + "version1" + File.separator + "mapjar.jar" + "=" + getMappedFileRoot()+ File.separator + "version2" + File.separator + "mapjar.jar")};
			setMappingFile(mapping);
			String javaExecuteCommand = System.getProperty("user.dir") + File.separator + "java -Xint -Xproxy:auto -cp " +  getMappedFileRoot() + File.separator + "version1" + File.separator + "mapjar.jar" + System.getProperty("path.separator") + System.getProperty("java.class.path") + " com.ibm.j9.offload.tests.mapping.LoadClass" + " com.ibm.j9.offload.tests.mapping.TestClass";
			System.out.println("javaExecuteCommand:"+ javaExecuteCommand);
			Process p1 = Runtime.getRuntime().exec(javaExecuteCommand);
			ProcessOutputThread t = new ProcessOutputThread(p1.getInputStream());
			t.start();
			assertTrue("Did not get expected 0 response code", p1.waitFor()==0);
			assertTrue("Did not find class :" + testOutput,testOutput.indexOf("!FOUND!") != -1);
			assertTrue("Did not load right class when jar was mapped:" + testOutput,testOutput.indexOf("TestClass[VERSION2]") != -1);
		} catch (Exception e) {
			e.printStackTrace();
			fail("Unexpected Exception:" + e);
		}
	}
	
	/* test case were we ask for a class that was mapped and is found 
	 * when the class exists in the base jar but does not exist in the mapped jar validate that 
	 * the class is not loaded from either the mapped jar or base jar.  For the test the class does exist
	 * in the test jars so it should be loaded from there and the version should be 0*/
	public void testClassNotFoundWhenInUnmappedButNotMapped(){
		try {
			testOutput = "";
			String mapping[] = {new String(getMappedFileRoot()+ File.separator + "version1" + File.separator + "mapjar.jar" + "=" + getMappedFileRoot()+ File.separator + "noClass" + File.separator + "mapjar.jar")};
			setMappingFile(mapping);
			String javaExecuteCommand = System.getProperty("user.dir") + File.separator + "java -Xint -Xproxy:auto -cp " +  getMappedFileRoot() + File.separator + "version1" + File.separator + "mapjar.jar" + System.getProperty("path.separator") + System.getProperty("java.class.path") + " com.ibm.j9.offload.tests.mapping.LoadClass" + " com.ibm.j9.offload.tests.mapping.TestClass";
			System.out.println("javaExecuteCommand:"+ javaExecuteCommand);
			Process p1 = Runtime.getRuntime().exec(javaExecuteCommand);
			ProcessOutputThread t = new ProcessOutputThread(p1.getInputStream());
			t.start();
			assertTrue("Did not get expected 0 response code", p1.waitFor()==0);
			assertTrue("Did not find class :" + testOutput,testOutput.indexOf("!FOUND!") != -1);
			assertTrue("Did not load right class when jar was mapped:" + testOutput,testOutput.indexOf("TestClass[VERSION0]") != -1);
		} catch (Exception e) {
			e.printStackTrace();
			fail("Unexpected Exception:" + e);
		}
	}
	
	/* test case were we ask for a class that was mapped and is found 
	 * when different versions of the class exist in the base jar and the jar it is mapped to 
	 * make sure we load the version from the mapped file
	 * This version tests when there are multiple entries in the mapping file */
	public void testClassFoundWhenMappedMultipleEntries(){
		try {
			testOutput = "";
			String mapping[] = {new String(getMappedFileRoot()+ File.separator + "whatever1" + File.separator + "anotherjar.jar" + "=" + getMappedFileRoot()+ File.separator + "whatever2" + File.separator + "anotherjar.jar"),
								new String(getMappedFileRoot()+ File.separator + "version1" + File.separator + "mapjar.jar" + "=" + getMappedFileRoot()+ File.separator + "version2" + File.separator + "mapjar.jar")};
			setMappingFile(mapping);
			String javaExecuteCommand = System.getProperty("user.dir") + File.separator + "java -Xint -Xproxy:auto -cp " +  getMappedFileRoot() + File.separator + "version1" + File.separator + "mapjar.jar" + System.getProperty("path.separator") + System.getProperty("java.class.path") + " com.ibm.j9.offload.tests.mapping.LoadClass" + " com.ibm.j9.offload.tests.mapping.TestClass";
			System.out.println("javaExecuteCommand:"+ javaExecuteCommand);
			Process p1 = Runtime.getRuntime().exec(javaExecuteCommand);
			ProcessOutputThread t = new ProcessOutputThread(p1.getInputStream());
			t.start();
			assertTrue("Did not get expected 0 response code", p1.waitFor()==0);
			assertTrue("Did not find class :" + testOutput,testOutput.indexOf("!FOUND!") != -1);
			assertTrue("Did not load right class when jar was mapped:" + testOutput,testOutput.indexOf("TestClass[VERSION2]") != -1);
		} catch (Exception e) {
			e.printStackTrace();
			fail("Unexpected Exception:" + e);
		}
	}
	
	/* test case were we ask for a class that was mapped and is found 
	 * when different versions of the class exist in the base jar and the jar it is mapped to 
	 * make sure we load the version from the mapped file
	 * This version tests when the mapping is specified as a directory */
	public void testClassFoundWhenMappedDirectory() {
		try {
			testOutput = "";
			String mapping[] = {new String(getMappedFileRoot()+ File.separator + "version1" + "=" + getMappedFileRoot()+ File.separator + "version2")};
			setMappingFile(mapping);
			String javaExecuteCommand = System.getProperty("user.dir") + File.separator + "java -Xint -Xproxy:auto -cp " +  getMappedFileRoot() + File.separator + "version1" + File.separator + "mapjar.jar" + System.getProperty("path.separator") + System.getProperty("java.class.path") + " com.ibm.j9.offload.tests.mapping.LoadClass" + " com.ibm.j9.offload.tests.mapping.TestClass";
			System.out.println("javaExecuteCommand:"+ javaExecuteCommand);
			Process p1 = Runtime.getRuntime().exec(javaExecuteCommand);
			ProcessOutputThread t = new ProcessOutputThread(p1.getInputStream());
			t.start();
			assertTrue("Did not get expected 0 response code", p1.waitFor()==0);
			assertTrue("Did not find class :" + testOutput,testOutput.indexOf("!FOUND!") != -1);
			assertTrue("Did not load right class when jar was mapped:" + testOutput,testOutput.indexOf("TestClass[VERSION2]") != -1);
		} catch (Exception e) {
			e.printStackTrace();
			fail("Unexpected Exception:" + e);
		}
	}
	
	/* test case were we ask for a class that was mapped and is found 
	 * when different versions of the class exist in the base jar and the jar it is mapped to 
	 * make sure we load the version from the mapped file
	 * This version tests when the mapping is specified as a parent directory */
	public void testClassFoundWhenMappedParentDirectory() {
		try {
			testOutput = "";
			String mapping[] = {new String(getMappedFileRoot() + "=" + getMappedFileRoot())};
			setMappingFile(mapping);
			String javaExecuteCommand = System.getProperty("user.dir") + File.separator + "java -Xint -Xproxy:auto -cp " +  getMappedFileRoot() + File.separator + "version1" + File.separator + "mapjar.jar" + System.getProperty("path.separator") + System.getProperty("java.class.path") + " com.ibm.j9.offload.tests.mapping.LoadClass" + " com.ibm.j9.offload.tests.mapping.TestClass";
			System.out.println("javaExecuteCommand:"+ javaExecuteCommand);
			Process p1 = Runtime.getRuntime().exec(javaExecuteCommand);
			ProcessOutputThread t = new ProcessOutputThread(p1.getInputStream());
			t.start();
			assertTrue("Did not get expected 0 response code", p1.waitFor()==0);
			assertTrue("Did not find class :" + testOutput,testOutput.indexOf("!FOUND!") != -1);
			assertTrue("Did not load right class when jar was mapped:" + testOutput,testOutput.indexOf("TestClass[VERSION1]") != -1);
		} catch (Exception e) {
			e.printStackTrace();
			fail("Unexpected Exception:" + e);
		}
	}
	
	/* test that classloading is done locally which is the main use case for the mapping */ 
	public void testMappingResultsInLocalClassloading() {
		try {
			testOutput = "";
			String mapping[] = {new String("*=*")};
			setMappingFile(mapping);
			String javaExecuteCommand = System.getProperty("user.dir") + File.separator + "java -Xint -Xproxy:auto -Dcom.ibm.proxy.verboseIsolation -cp " + System.getProperty("java.class.path") + " -version";
			System.out.println("javaExecuteCommand:"+ javaExecuteCommand);
			Process p1 = Runtime.getRuntime().exec(javaExecuteCommand);
			ProcessOutputThread t = new ProcessOutputThread(p1.getInputStream());
			t.start();
			assertTrue("Did not get expected 0 response code", p1.waitFor()==0);
			assertTrue("Did not find class :" + testOutput,testOutput.indexOf("JNI Intercept: Dispatching method=java/util/zip/ZipFile.openlocal(") != -1);
			assertTrue("Did not find class :" + testOutput,testOutput.indexOf("Remote Invocation: Dispatching method=java/util/zip/ZipFile.open(") == -1);
		} catch (Exception e) {
			e.printStackTrace();
			fail("Unexpected Exception:" + e);
		}
	}
	
}
