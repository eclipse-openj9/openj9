/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package j9vm.test.ddrext;

import j9vm.test.ddrext.junit.DDRTestSuite;
import j9vm.test.ddrext.junit.TestCallsites;
import j9vm.test.ddrext.junit.TestClassExt;
import j9vm.test.ddrext.junit.TestCollisionResilientHashtable;
import j9vm.test.ddrext.junit.TestDDRExtensionGeneral;
import j9vm.test.ddrext.junit.TestFindExt;
import j9vm.test.ddrext.junit.TestJITExt;
import j9vm.test.ddrext.junit.TestMonitors;
import j9vm.test.ddrext.junit.TestRTSpecificDDRExt;
import j9vm.test.ddrext.junit.TestSharedClassesExt;
import j9vm.test.ddrext.junit.TestStackMap;
import j9vm.test.ddrext.junit.TestTenants;
import j9vm.test.ddrext.junit.TestThread;
import j9vm.test.ddrext.junit.TestTypeResolution;
import j9vm.test.ddrext.junit.deadlock.*;

import java.io.File;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.HashMap;

import javax.xml.parsers.DocumentBuilderFactory;

import junit.framework.AssertionFailedError;
import junit.framework.Test;
import junit.framework.TestListener;
import junit.framework.TestResult;
import junit.framework.TestSuite;

import org.testng.log4testng.Logger;
import org.w3c.dom.Document;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import com.ibm.j9ddr.tools.ddrinteractive.Context;

public class AutoRun {

	private static Logger log = Logger.getLogger(AutoRun.class);
	private static int dumpCount = 0;
	private static int successDump = 0;
	private static int failDump = 0;
	private static int errCount;
	private static int failCount;
	private static HashMap<String, Enumeration<?>> failedCoreFileResult = new HashMap<String, Enumeration<?>>();
	private static ArrayList<String> successCoreFile = new ArrayList<String>();
	public static String ddrPluginJar = null;
	private static int totalExcluded = 0;
	private static String excludesForCurrentSuite = null;

	/*
	 * system exit code -1: no dump found system exit code -2: at least one test
	 * cases fail system exit code -3: DDR initialization failed system exit
	 * code -4: wrong test case name system exit code 0: all test cases pass
	 */
	public static void main(String[] args) {

		File dir = null;
		String testCaseList = null;
		
		if (args.length == 1) {
			dir = new File(args[0]);
			testCaseList = "ALL";
		} else if (args.length == 2) {
			dir = new File(args[0]);
			testCaseList = args[1];
		} else if (args.length == 3) {
			dir = new File(args[0]);
			testCaseList = args[1];
			ddrPluginJar = args[2];
		} else {
			log.error("AutoRun usage:");
			log.error(" args[0]---required, a dump file e.g. /bluebird/javatest/j9unversioned/dump4ddrext/Java7_GA/linux_x86-64.7ga.core.dmp");
			log.error(" args[1]---optional, ALL or specific test case name(s) e.g.TestClassExt,TestThread. default to ALL ");
			log.error(" args[2]---optional, javatestVMHome which is used for getting the path of ddr plugins, default to NULL");
			System.exit(-1);
		}

		if (dir.isFile()) {
			if (dir.getName().contains(".dmp")
					|| dir.getName().contains(".DMP")) {
				if (testCaseList == "" || testCaseList == null) {
					testCaseList = "ALL";
				}
				runTest(dir.getAbsolutePath(), testCaseList);
				if (failDump > 0) {
					for (String failFile : failedCoreFileResult.keySet()) {
						Enumeration<?> failures = failedCoreFileResult
								.get(failFile);
						while (failures.hasMoreElements()) {
							String f = failures.nextElement().toString();
							String testCaseName = f
									.substring(0, f.indexOf(':'));
							log.error(testCaseName);
							log.debug(f);
						}
					}
				}
			} else {
				log.error(dir.getName() + " is not .dmp file!");
				System.exit(-1);
			}
		}// end of if (dir.isFile()
			// if args[0] is directory, recursively search for .dmp or .DMP
			// files under dir and its subdir, run test for each dump.
		else {
			log.debug("search for .dmp files under " + args[0]);
			searchAllDumps(dir);
			if (dumpCount == 0) {
				log.error("No dump file under " + args[0]);
				System.exit(-1);
			} else {
				log.info("Total number of dump file under " + args[0] + ":"
						+ dumpCount);
				log.info("Total number of success dump file under " + args[0]
						+ ":" + successDump);
				log.info("Total number of failed dump file under " + args[0]
						+ ":" + failDump);
				log.info("==========List of success dump file under " + args[0]
						+ "==========");
				if (successDump > 0) {
					for (String sucFile : successCoreFile) {
						log.info(sucFile);
					}
				}
				if (failDump > 0) {
					log.error("==========List of failed dump file under "
							+ args[0] + "==========");
					for (String failFile : failedCoreFileResult.keySet()) {
						log.error("--Failures in " + failFile + "--");
						Enumeration<?> failures = failedCoreFileResult
								.get(failFile);
						while (failures.hasMoreElements()) {
							String f = failures.nextElement().toString();
							String testCaseName = f
									.substring(0, f.indexOf(':'));
							log.error(testCaseName);
							log.debug(f);
						}
					}
					// TO-DO: provide instruction to rerun failed test cases
					// log.error("You can rerun failed test cases in Eclipse.");
				}
			}
		}
		// print out success/failure info
		log.info("Total Error/Failures: " + (errCount + failCount));
		if ((errCount + failCount) > 0) {
			System.exit(-2);
		} else {
			System.exit(0);
		}
	}

	/**
	 * Given the path to a folder containing core dumps, this method iterates through the dump files and invokes runTest()
	 * on each core dump to run the DDR test suites on each dump. 
	 * @param dir - Full path to the folder containing core dumps, if path to a core dump is given instead, that dump only is used
	 */
	public static void searchAllDumps(File dir) {
		if (dir.isDirectory()) {
			String[] subDirFiles = dir.list();
			if (subDirFiles != null) {
				for (String sub : subDirFiles) {
					searchAllDumps(new File(dir, sub));
				}
			} else {
				log.warn("Warning: " + dir.getPath() + " is empty!");
				return;
			}
		} else if (dir.isFile()) {
			if (dir.getName().contains(".dmp")
					|| dir.getName().contains(".DMP")) {
				runTest(dir.getAbsolutePath(), "ALL");
				dumpCount++;
			} else
				log.info(dir.getName() + " is not .dmp file!");
		} else {
			log.info("Warning: " + dir.getPath() + " is not directory or file!");
		}
	}
	
	/**
	 * This method is responsible for running the test suite in regular DDR testing
	 * @param coreFilePath - Full path to the core file with with to Initialize DDR
	 * @param testCaseList - Comma separated list of test suite classes to execute
	 */
	public static void runTest(String coreFilePath, String testCaseList) {

		// initiate DDRInteractive instance
		boolean setupSuccess = SetupConfig.init(coreFilePath);
		if (setupSuccess == false) {
			log.error("DDR initialization failed, can not proceed with tests");
			System.exit(-3);
		}

		Test suite = null;
		
		//Get the overall list of tests to ignore for regular DDR tests as defined in ddrext_excludes.xml 
		String excludeList = getExcludesFor(Constants.TESTDOMAIN_REGULAR);
		
		if (coreFilePath.contains(Constants.RT_PREFIX)) {
			suite = getTestSuiteRT(excludeList);
		} else {
			suite = getTestSuite(testCaseList,excludeList);
		}

		if (suite == null) {
			log.error("At least one test cases in your test case list "
					+ testCaseList + " doesn't exist in DDRTestSuite.");
			log.error("Please check your test case list and DDRTestSuite.java.");
			System.exit(-4);
		}

		//Based on the excludes defined in ddrext_excludes.xml, construct the list of tests to ignore from the full list of tests in 
		//the current test suite.
		getExcludesForCurrentSuites(suite,excludeList);
		
		int numberOfExcludesForCurrentSuite = 0 ; 
		
		if ( excludesForCurrentSuite != null ) {
			if ( excludesForCurrentSuite.contains(",") == false ) {
				numberOfExcludesForCurrentSuite = 1;
			} else {
				numberOfExcludesForCurrentSuite = excludesForCurrentSuite.split(",").length;
			}
		}
		
		log.info("Running " + (suite.countTestCases() - numberOfExcludesForCurrentSuite ) + " tests for "
				+ coreFilePath);

		TestResult result = new TestResult();
		result.addListener(new TestListener() {
			public void addError(Test test, Throwable t) {
				t.printStackTrace(System.err);
			}

			public void addFailure(Test test, AssertionFailedError t) {
				t.printStackTrace(System.err);
			}

			public void endTest(Test test) {
				log.info("Finished " + test + Constants.NL);
			}

			public void startTest(Test test) {
				log.info("Starting " + test);
			}
		});
		suite.run(result);
		errCount = errCount + result.errorCount();
		failCount = failCount + result.failureCount();
		// log test result
		if (result.errorCount() == 0 & result.failureCount() == 0) {
			successCoreFile.add(coreFilePath);
			successDump++;
		} else {
			failedCoreFileResult.put(coreFilePath, result.failures());
			failDump++;
		}
		log.info("================Test Result for " + coreFilePath
				+ "==================");
		log.info("Errors:   " + result.errorCount() + " out of "
				+ (suite.countTestCases() - numberOfExcludesForCurrentSuite) + " test cases.");
		log.info("Failures: " + result.failureCount() + " out of "
				+ (suite.countTestCases() - numberOfExcludesForCurrentSuite) + " test cases.");

		if ( numberOfExcludesForCurrentSuite != 0 ){
			log.info("Total tests excluded = " + numberOfExcludesForCurrentSuite + " [" + excludesForCurrentSuite + "]");
		}
		
		// After run test on the core file, reset DDRInteractive,outputString
		// and ddrResultCache to null
		SetupConfig.reset();
		DDRExtTesterBase.resetList();

	}

	/**
	 * This method is responsible for running the test suite in Native Debugger based DDR testing
	 * @param ctx - The context object 
	 * @param out - The PrintStream object to be used to output logs  
	 * @param testCaseList - Comma separated list of test suite classes to run
	 */
	public static void runTest(Context ctx, PrintStream out, String testCaseList) {

		//initiate DDRInteractive instance
		SetupConfig.init(ctx, out);

		log.info("Executing test cases : " + testCaseList);
		
		//Get the overall list of tests to ignore for native DDR tests as found in ddrext_excludes.xml
		String excludeList = getExcludesFor(Constants.TESTDOMAIN_NATIVE);
		
		Test suite = getTestSuite(testCaseList,excludeList);
		
		//Based on the excludes defined in ddrext_excludes.xml, construct the list of tests to ignore from the full list of tests in 
		//the current test suite.
		getExcludesForCurrentSuites(suite,excludeList);
		
		int numberOfExcludesForCurrentSuite = 0 ; 
		
		if ( excludesForCurrentSuite != null ) {
			if ( excludesForCurrentSuite.contains(",") == false ) {
				numberOfExcludesForCurrentSuite = 1;
			} else {
				numberOfExcludesForCurrentSuite = excludesForCurrentSuite.split(",").length;
			}
		}
				
		log.info("Running " + (suite.countTestCases() - numberOfExcludesForCurrentSuite ) 
				+ " tests from ddrjunit plugin");

		TestResult result = new TestResult();
		result.addListener(new TestListener() {
			public void addError(Test test, Throwable t) {
				t.printStackTrace(System.err);
			}

			public void addFailure(Test test, AssertionFailedError t) {
				t.printStackTrace(System.err);
			}

			public void endTest(Test test) {
				log.info("Finished " + test + Constants.NL);
			}

			public void startTest(Test test) {
				log.info("Starting " + test);
			}
		});
		
		suite.run(result);

		errCount = errCount + result.errorCount();
		failCount = failCount + result.failureCount();

		log.info("================Test Result==================");
		log.info("Errors:   " + result.errorCount() + " out of "
				+ (suite.countTestCases() - numberOfExcludesForCurrentSuite) + " test cases.");
		log.info("Failures: " + result.failureCount() + " out of "
				+ (suite.countTestCases() - numberOfExcludesForCurrentSuite) + " test cases.");

		if ( numberOfExcludesForCurrentSuite !=0 ){
			log.info("Total tests excluded = " + numberOfExcludesForCurrentSuite + " [" + excludesForCurrentSuite + "]");
		}
		
		// After run test on the core file, reset DDRInteractive,outputString
		// and ddrResultCache to null
		SetupConfig.reset();
		DDRExtTesterBase.resetList();
		// print out success/failure info
		log.info("Total Error/Failures: " + (errCount + failCount));
		if ((errCount + failCount) > 0) {
			System.exit(-2);
		} else {
			System.exit(0);
		}
	}
	
	/**
	 * Given a test suite, the method constructs the list of excluded test cases which may be a subset of the total 
	 * list of tests excluded in ddrext_excludes.xml for a given domain (regular or native).
	 * @param suite - The test suite to run in this given run.
	 * @param excludeList - A comma separated list of excluded tests from the current test suite to run.
	 */
	@SuppressWarnings("rawtypes")
	private static void getExcludesForCurrentSuites( Test suite, String excludeList ) {
		TestSuite s = (TestSuite)suite;
		
		Enumeration en = s.tests();
		while (en.hasMoreElements()) {
			junit.framework.Test test = (junit.framework.Test)en.nextElement();
			if (test instanceof junit.framework.TestSuite) {
				getExcludesForCurrentSuites((junit.framework.TestSuite)test, excludeList);
			} else {
				junit.framework.TestCase testCase = (junit.framework.TestCase)test;
				if ( excludeList.contains(testCase.getName()) ){
					if ( excludesForCurrentSuite == null ) {
						excludesForCurrentSuite = testCase.getName(); 
					} else {
						excludesForCurrentSuite = excludesForCurrentSuite + "," + testCase.getName();
					}
				}
			}
		}
	}

	/**
	 * Creates a new j9vm.test.ddr.ext.junit.DDRTestSuite using a given list of tests 
	 * @param testCaseList - A comma separated list of test suites to run. To run everything, pass the string - "ALL"
	 * @param ignoreTestList - A comma separated list of test cases to ignore
	 * @return A new instance of DDRTestSuite object containing DDR test suites
	 * */
	public static Test getTestSuite(String testCaseList, String ignoreTestList) {
		DDRTestSuite suite = new DDRTestSuite("Test for j9vm.test.ddrext.junit", ignoreTestList);
		// $JUnit-BEGIN$
		if (testCaseList.equalsIgnoreCase("ALL")) {
			suite.addTestSuite(TestThread.class);
			//suite.addTestSuite(TestJITExt.class);
			suite.addTestSuite(TestSharedClassesExt.class);
			suite.addTestSuite(TestClassExt.class);
			suite.addTestSuite(TestCallsites.class);
			suite.addTestSuite(TestDDRExtensionGeneral.class);
			suite.addTestSuite(TestFindExt.class);
			suite.addTestSuite(TestTypeResolution.class);
		} else {
			String[] testCases = testCaseList.split(",");
			for (String aTest : testCases) {
				if (aTest.trim().equalsIgnoreCase("TestThread")) {
					suite.addTestSuite(TestThread.class);
				} else if (aTest.trim().equalsIgnoreCase("TestJITExt")) {
					suite.addTestSuite(TestJITExt.class);
				} else if (aTest.trim()
						.equalsIgnoreCase("TestSharedClassesExt")) {
					suite.addTestSuite(TestSharedClassesExt.class);
				} else if (aTest.trim().equalsIgnoreCase("TestClassExt")) {
					suite.addTestSuite(TestClassExt.class);
				} else if (aTest.trim().equalsIgnoreCase("CollisionResilientHashtable")) {
					suite.addTestSuite(TestCollisionResilientHashtable.class);
				} else if (aTest.trim().equalsIgnoreCase("TestCallsites")) {
					suite.addTestSuite(TestCallsites.class);
				} else if (aTest.trim().equalsIgnoreCase(
						"TestDDRExtensionGeneral")) {
					suite.addTestSuite(TestDDRExtensionGeneral.class);
				} else if (aTest.trim().equalsIgnoreCase("TestFindExt")) {
					suite.addTestSuite(TestFindExt.class);
				} else if (aTest.trim().equalsIgnoreCase("TestStackMap")) {
					suite.addTestSuite(TestStackMap.class);
				} else if (aTest.trim().equalsIgnoreCase("TestTenants")) {
					suite.addTestSuite(TestTenants.class);
				} else if (aTest.trim().equalsIgnoreCase("TestTypeResolution")) {
					suite.addTestSuite(TestTypeResolution.class);
				} else if (aTest.trim().equalsIgnoreCase("TestMonitors")) {
					suite.addTestSuite(TestMonitors.class);
				} else if (aTest.trim().equalsIgnoreCase("TestDeadlockCase1")) {
					suite.addTestSuite(TestDeadlockCase1.class);
				} else if (aTest.trim().equalsIgnoreCase("TestDeadlockCase2")) {
					suite.addTestSuite(TestDeadlockCase2.class);
				} else if (aTest.trim().equalsIgnoreCase("TestDeadlockCase3")) {
					suite.addTestSuite(TestDeadlockCase3.class);
				} else if (aTest.trim().equalsIgnoreCase("TestDeadlockCase4")) {
					suite.addTestSuite(TestDeadlockCase4.class);
				} else if (aTest.trim().equalsIgnoreCase("TestDeadlockCase5")) {
					suite.addTestSuite(TestDeadlockCase5.class);
				} else if (aTest.trim().equalsIgnoreCase("TestDeadlockCase6")) {
					suite.addTestSuite(TestDeadlockCase6.class);
				} else {
					return null;
				}
			}
		}
		// $JUnit-END$
		return suite;
	}
	
	/**
	 * Creates a new j9vm.test.ddr.ext.junit.DDRTestSuite using the real-time specific test suite
	 * @param ignoreTestList - A comma separated list of test cases to ignore 
	 * @return A new instance of DDRTestSuite object containing the real-time specific DDR test suite 
	 */
	public static Test getTestSuiteRT(String ignoreTestList) {
		DDRTestSuite suite = new DDRTestSuite("Test for j9vm.test.ddrext.junit", ignoreTestList);
		// $JUnit-BEGIN$
		suite.addTestSuite(TestRTSpecificDDRExt.class);
		// $JUnit-END$
		return suite;
	}
	
	/**
	 * Given a test type, returns a comma separated list of test cases to ignore by parsing the excludes.xml
	 * @param testType - regular or native 
	 * @return A comma separated list of tests cases to ignore for the given type
	 */
	public static String getExcludesFor(String testType){
		try{
			Document doc = DocumentBuilderFactory.newInstance().newDocumentBuilder().parse(new AutoRun().getClass().getResourceAsStream("/" + Constants.DDREXT_EXCLUDES));
			Node mainBlock = doc.getChildNodes().item(0);
			NodeList excludeList = mainBlock.getChildNodes();
			String excludeListStr = "";
			for ( int i = 0 ; i < excludeList.getLength() ; i++ ){
				Node anExclude = excludeList.item(i);
				if(anExclude.getNodeName().equals(Constants.DDREXT_EXCLUDES_PROPERTY_EXCLUDE)){
					NamedNodeMap attributes = anExclude.getAttributes();
					String name = attributes.getNamedItem(Constants.DDREXT_EXCLUDES_PROPERTY_NAME).getNodeValue();
					String type = attributes.getNamedItem(Constants.DDREXT_EXCLUDES_PROPERTY_DOMAIN).getNodeValue();
					if ( type.equalsIgnoreCase( testType ) ){
						if ( excludeListStr.length() == 0){
							excludeListStr = name;
						}
						else{
							excludeListStr = excludeListStr + "," + name;
						}
						totalExcluded++;
					}
				}
			}
			return excludeListStr;
		}
		catch(Exception error){
			error.printStackTrace();
			return null;
		}
	}
}
