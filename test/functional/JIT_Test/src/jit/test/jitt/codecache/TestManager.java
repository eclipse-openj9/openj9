/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
package jit.test.jitt.codecache;

import java.io.File;
import java.lang.management.ManagementFactory;
import java.lang.management.RuntimeMXBean;
import java.util.List;

import jit.runner.JarTesterMT;
import junit.framework.AssertionFailedError;
import junit.framework.Test;
import junit.framework.TestListener;
import junit.framework.TestResult;
import junit.framework.TestSuite;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.cli.PosixParser;

/**
 * The TestManager orchestrates JarTester based tasks related to class compilation via Compiler.compile API,
 * monitoring of JIT verbose log, and making notifications related to code cache events consumable to test
 * implementations. The test harness also provides the integration point to all the code cache Junit tests
 * designed and provide external flags to execute the tests separately or in a suite by Axxon jobs.
 *
 * @author mesbah
 */
public class TestManager {

	/*Full path to the JIT verbose log file to use*/
	private static String jitLogFileName = null;

	/*Full path to the directory containing source Jar files to compile during test run*/
	private static String jarDir = null;

	/*Handle to Log Parser Thread*/
	private static LogParserThread logParserDriver = null;

	/*Handle to JarTester thread*/
	private static JarTesterMTDriver jarTesterMTDriver = null;

	/*Offset value at which the parser starts parsing the log file. Use of this offset
	 * is important when we use fresh compilation for each Trampoline tests.*/
	private static int logParserStartingOffset = 0 ;

	/*Total number of code caches being used at startup, this is equivalent to -Xjit:numOfCodeCachesOnStartup*/
	private static int numCodeCacheOnStartup = 0;

	/*This is the entry point to JIT code cache Test harness.*/
	public static void main( String args[] ) {

		/*Obtain the JIT log file being used*/
		jitLogFileName = getLogFileName();

		if ( jitLogFileName == null ) {
			System.out.println( "Can not find JIT log file path, please ensure your JIT verbose log file name is of format <filename>.log" );
			return;
		} else {
			System.out.println( "JIT verbose log file being used : " + jitLogFileName );
		}

		/*Obtain the basic command line arguments*/

		Options options = getOptions();
		CommandLineParser parser = new PosixParser();
		CommandLine cmdLine = null;

		try {
			 cmdLine = parser.parse( options, args );
		} catch ( ParseException e ) {
			e.printStackTrace();
			System.out.println( "Error parsing command line input" );
			return;
		}

		if ( cmdLine.hasOption( Constants.JAR_DIR ) ) {
			jarDir = cmdLine.getOptionValue( Constants.JAR_DIR );
			if ( new File( jarDir ).exists() == false ) {
				System.out.println( "Invalid location provided for jar directories : "+jarDir );
				return;
			} else {
				System.out.println( "Jar location : " + jarDir );
			}
		} else {
			System.out.println( "Please provide Jar Directory path" );
			printUsage( options );
			return;
		}

		/*Prepare Junit test suite according to command line user options*/

		TestSuite ts = new TestSuite();
		if ( cmdLine.hasOption( Constants.CCTEST_BASIC ) ) {
			ts.addTestSuite( BasicTest.class );
		}

		if ( cmdLine.hasOption( Constants.CCTEST_BOUNDARY ) ) {
			if ( ts.testCount() == 0 ) {
				ts.addTestSuite( BoundaryTest.class );
			} else {
				System.out.println( "Boundary test must not be run with other tests" );
				return;
			}
		}

		if ( cmdLine.hasOption( Constants.CCTEST_SPACE_REUSE ) ) {
			ts.addTestSuite( SpaceReuseTest.class );
		}

		if ( cmdLine.hasOption( Constants.CCTEST_TRAMPOLINE_BASIC ) ) {
			ts.addTestSuite( TrampolineTest_Basic.class );
		}

		if ( cmdLine.hasOption( Constants.CCTEST_TRAMPOLINE_ADVANCED ) ) {
			ts.addTestSuite( TrampolineTest_Advanced.class );
		}

		if ( cmdLine.hasOption( Constants.CCTEST_TRAMPOLINE_IPIC ) ) {
			ts.addTestSuite( TrampolineTest_IPIC.class );
		}

		if ( cmdLine.hasOption( Constants.CCTEST_AOT_PHASE1 ) ) {
			loadAot();
		}

		if ( cmdLine.hasOption( Constants.CCTEST_AOT_PHASE2 ) ) {
			ts.addTestSuite( AOTTest.class );
		}

		TestResult result = new TestResult();
		result.addListener( new TestListener() {
			public void addError( Test test, Throwable t ) {
				t.printStackTrace( System.err );
			}

			public void addFailure( Test test, AssertionFailedError t ) {
				t.printStackTrace( System.err );
			}

			public void endTest( Test test ) {
				System.out.println( "Finished " + test + System.getProperty( "line.separator" ) );
			}

			public void startTest( Test test ) {
				System.out.println( "Starting " + test );
			}
		} );

		/*Start child threads with appropriate options*/
		if ( cmdLine.hasOption( Constants.CCTEST_BASIC ) || cmdLine.hasOption( Constants.CCTEST_SPACE_REUSE )  ) {
			startLogParserInBackground( Constants.BASIC_TEST );
			// Basic tests requires class unloading and method reclamation, so use multiple class loaders.
			// Use a single class compilation thread .
			startCompilationInBackground( true, false );
		}

		/*Start code cache tests*/
		ts.run( result );

		if ( cmdLine.hasOption( Constants.CCTEST_BASIC ) || cmdLine.hasOption( Constants.CCTEST_SPACE_REUSE ) ) {
			/*Shutdown child threads*/
			if ( shutdownBackgroundProcesses() ) {
				System.out.println("Info : TestManager: Code Cache Test Harness shutdown complete");
			} else {
				System.out.println("WARNING: TestManager: Failed to shut down Code Cache Test harness");
				//TODO: We should have a Junit failure at this point
			}
		}

		/*Show result statistics*/
		System.out.println( "================Test Result for ==================" );
		System.out.println( "Errors:   " + result.errorCount() + " out of " + ts.countTestCases() + " test cases." );
		System.out.println( "Failures: " + result.failureCount() + " out of " + ts.countTestCases()  + " test cases." );
		System.out.println( "Total Error/Failures: " + ( result.errorCount() + result.failureCount() ) );

		if ((result.errorCount() + result.failureCount() ) > 0) {
			System.exit(-2);
		} else {
			System.exit(0);
		}
	}

	/**
	 * This method implements the first phase of the AOT test
	 * Here we invoke the target method sufficiently for it to be AOT compiled and placed in shared class cache.
	 */
	private static void loadAot() {

		/*VM option required in this case is :
		 * -Xjit:code=1024,numCodeCachesOnStartup=1,verbose={"codecache|compileEnd"},vlog=<logdir>/<name>.log
		 * -Xaot:count=5 -Xshareclasses:name=<cachename>,reset
		 * */
		if ( !isAOTVMOptionsSet() ){
			System.out.println("Required VM options not set for AOT load operation");
			return;
		}

		try {
			Thread.sleep(5000);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}

		/*Make sure the target class is loaded and AOT'ed.
		 *This is a prerequisite for the Code Cache AOT test*/
		Caller1_AotTest caller1 = new Caller1_AotTest();

		for ( int i = 0 ; i < 10 ; i++ ) {
			caller1.caller();
		}
	}

	/**
	 * Helper method to verify if all required VM option has been set for the code cache AOT test
	 * @return
	 */
	private static boolean isAOTVMOptionsSet() {

		boolean XaotSet = false;
		boolean XshareclassesSet = false;

		try {
			RuntimeMXBean RuntimemxBean = ManagementFactory.getRuntimeMXBean();
			List<String> aList=RuntimemxBean.getInputArguments();
			for( int i=0;i<aList.size();i++ ) {
			    if ( aList.get( i ).contains( "-Xaot" ) ) {
			    	String xjitOptions = aList.get( i );
			    	if ( xjitOptions.contains( "count=5" ) ) {
			    		XaotSet = true;
			    	} else {
			    		System.out.println("Please set -Xaot:count=5 in VM options");
			    	}
			    } else if ( aList.get( i ).contains( "-Xshareclasses" ) ) {
			    	XshareclassesSet = true;
			    }

			    if ( XaotSet && XshareclassesSet ) {
			    	break;
			    }
			}

			if ( !XaotSet ) {
				System.out.println("Please set -Xaot:count=5 in VM argument");
			}

			if ( !XshareclassesSet ) {
				System.out.println("Please set -Xshareclasses,reset in VM argument");
			}

			return XaotSet && XshareclassesSet;

		} catch ( StringIndexOutOfBoundsException e ) {
			e.printStackTrace();
			return false;
		}
	}

	/**
	 * @return true if -Xshareclasscache VM option is in use, false otherwise
	 */
	public static boolean isShareClassesSet() {
		RuntimeMXBean RuntimemxBean = ManagementFactory.getRuntimeMXBean();
		List<String> aList=RuntimemxBean.getInputArguments();
		for( int i=0;i<aList.size();i++ ) {
		    if ( aList.get( i ).contains( "-Xshareclasses" ) ) {
		    	return true;
		    }
		}
		return false;
	}

	public static JarTesterMTDriver getJarTesterMTDriver() {
		return jarTesterMTDriver;
	}

	public static LogParserThread getLogParserDriver() {
		return logParserDriver;
	}

	/**
	 * Starts a new instance of LogParserThread.
	 * @param testType : Flag indicating level of parsing to perform.
	 * If running basic or trampoline test, use Constants.BASIC, for boundary test, use Constants.BOUNDARY
	 */
	public static void startLogParserInBackground( int testType ) {
		System.out.println( "Starting log parser thread..." );
		logParserDriver = new LogParserThread( jitLogFileName, testType, logParserStartingOffset );
		logParserDriver.start();
	}

	/**
	 * Starts a new instance of JarTesterMT utility in a child thread.
	 * @param multipleClassLoaderRequired - true only if multiple class loader is required ( used for basic and trampoline tests )
	 * @param multiThreadedCompilationRequired - true only if multiple class compilation threads are to be used ( true by default )
	 */
	public static void startCompilationInBackground( boolean multipleClassLoaderRequired, boolean multiThreadedCompilationRequired ) {
		/*Start class compilation*/
		System.out.println( "Starting JarTesterMT driver thread. This will initiate class compilation..." );
		jarTesterMTDriver = new JarTesterMTDriver( multipleClassLoaderRequired, multiThreadedCompilationRequired, jarDir );
		jarTesterMTDriver.start();

		//TODO: Do we need to give the compilation thread this time to index the jar?
		try {
			Thread.sleep(2000);
		} catch (InterruptedException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}

	/**
	 * Sends requests for the class compilation and thread parsing child threads to exit.
	 * If the child threads do not exit right away, the method sends a maximum of 4 stop requests
	 * each separated by a 1 minute interval. If at least 1 child thread is running after 4 seconds, the
	 * method indicates a failure.
	 * @return - True if all child threads have exited. False if at least 1 child thread has not exited after 4 seconds of wait.
	 */
	public static boolean shutdownBackgroundProcesses() {
		/*Shut down log monitor*/
		boolean stopped =  stopCompilation() && stopLogParser();
		return stopped;
	}

	public static boolean stopLogParser() {
		logParserDriver.requestStop();

		boolean logParserStopped = false;

		for ( int i = 0 ; i < 16 ; i ++ ) {
			if ( logParserDriver.isAlive() ) {
				try {
					Thread.sleep( 1000 );
				}
				catch( InterruptedException e ) {
					e.printStackTrace();
				}
			} else {
				logParserStopped = true;
				logParserDriver.clearCache();
				logParserStartingOffset = logParserDriver.getLastCursorPosition();
				logParserDriver = null;
				break;
			}
		}

		if ( !logParserStopped ) {
			System.out.println( "WARNING: TestManager: LogParserThread taking longer than expected to stop." );
		} else {
			System.out.println( "INFO: TestManager: Log parser Thread stopped." );
		}

		return logParserStopped;
	}

	public static boolean stopCompilation() {
		/*Stop class compilation*/
		JarTesterMT jarTesterMT = jarTesterMTDriver.getJartesterMT();
		jarTesterMT.stopComopilation();

		boolean jarTesterStopped = false;

		for ( int i = 0 ; i < 16 ; i ++ ) {
			if ( jarTesterMTDriver.isAlive() ) {
				try {
					Thread.sleep( 1000 );
				}
				catch( InterruptedException e ) {
					e.printStackTrace();
				}
			} else {
				jarTesterStopped = true;
				jarTesterMTDriver = null;
				break;
			}
		}

		if ( !jarTesterStopped ) {
			System.out.println( "WARNING: TestManager: JarTesterMT Driver thread taking longer than expected to stop." );
		} else {
			System.out.println( "INFO: TestManager: JarTesterMT Driver Thread stopped." );
		}

		return jarTesterStopped;
	}

	public static void pauseCompilation() {
		/*Stop class compilation*/
		JarTesterMT jarTesterMT = jarTesterMTDriver.getJartesterMT();
		jarTesterMT.pauseCompilation();
	}

	public static void resumeCompilation() {
		JarTesterMT jarTesterMT = jarTesterMTDriver.getJartesterMT();
		jarTesterMT.resumeCompilation();
	}

	/**
	 * Returns the full path to the JIT verbose log directory path.
	 * @return
	 */
	private static String getLodDirPath() {
		String logDirPath = null;
		try {
			RuntimeMXBean RuntimemxBean = ManagementFactory.getRuntimeMXBean();
			List<String> aList=RuntimemxBean.getInputArguments();
			for( int i=0;i<aList.size();i++ ) {
			    if ( aList.get( i ).contains( "-Xjit" ) ) {
			    	String xjitOptions = aList.get( i );
			    	if ( xjitOptions.contains( "vlog=" ) ) {
			    		if ( xjitOptions.contains("jit.log") == false ) {
			    			System.out.println("The name of the JIT verbose log must be jit.log");
			    			return null;
			    		}
			    		int s = xjitOptions.indexOf( "vlog=" )+"vlog=".length();
			    		int e = xjitOptions.indexOf( ".log" )+".log".length();
			    		logDirPath = xjitOptions.substring( s,e );
			    		logDirPath = logDirPath.substring(0,logDirPath.lastIndexOf("/")); // Java file.separator does not match with make file $(D) on Windows
			    		break;
			    	}
			    }
			}
		} catch ( StringIndexOutOfBoundsException e ) {
			e.printStackTrace();
			return null;
		}
		return logDirPath;
	}

	/**
	 * @return true if only user has set -Xjit:numCodeCacheOnStartup=1
	 */
	public static boolean isSingleCodeCacheStartup() {

		/*If we have already parsed the -Xjit option once and stored the value for numCodeCacheOnStartup, use the stored value*/
		if ( numCodeCacheOnStartup == 1 ) {
			return true;
		}

		else if ( numCodeCacheOnStartup > 1 ) {
			return false;
		}

		/*If we have never parsed the -Xjit options for numCodeCacheOnStartup, parse it and store the value for future use*/
		else {
			boolean returnVal = false;
			int num = -1;
			try {
				RuntimeMXBean RuntimemxBean = ManagementFactory.getRuntimeMXBean();
				List<String> aList=RuntimemxBean.getInputArguments();
				for( int i=0;i<aList.size();i++ ) {
				    if ( aList.get( i ).contains( "-Xjit" ) ) {
				    	String xjitOptions = aList.get( i );
				    	if ( xjitOptions.contains( "numCodeCachesOnStartup=" ) ) {
				    		int s = xjitOptions.indexOf( "numCodeCachesOnStartup=" ) + ("numCodeCachesOnStartup=").length();
				    		int e = s + 1;
				    		String substring = xjitOptions.substring( s, e );
				    		num = Integer.parseInt(substring);
				    		numCodeCacheOnStartup = num;
				    		break;
				    	}
				    }
				}
			} catch ( StringIndexOutOfBoundsException e ) {
				e.printStackTrace();
			} catch ( NumberFormatException e ) {
				e.printStackTrace();
			}
			returnVal = ( num == 1 );
			return returnVal;
		}
	}

	/**
	 * @return true if -Xaot:forceaot is set
	 */
	public static boolean isForceAOT() {
		try {
			RuntimeMXBean RuntimemxBean = ManagementFactory.getRuntimeMXBean();
			List<String> aList=RuntimemxBean.getInputArguments();
			for( int i=0;i<aList.size();i++ ) {
			    if ( aList.get( i ).contains( "-Xaot" ) ) {
			    	String xaotOption = aList.get( i );
			    	if ( xaotOption.contains( "forceaot" ) ) {
			    		return true;
			    	}
			    }
			}
		} catch ( StringIndexOutOfBoundsException e ) {
			e.printStackTrace();
		} catch ( NumberFormatException e ) {
			e.printStackTrace();
		}
		return false;
	}

	/**
	 * Driver thread for JarTesterMT utility. If started, a new instance of JarTesterMT will be run in a separate child thread
	 * @author mesbah
	 *
	 */
	public static class JarTesterMTDriver extends Thread {
		JarTesterMT jarTester = null;
		String jarDir = null;

		public JarTesterMTDriver( boolean multipleClassLoaderRequired, boolean multiThreadedCompilationRequired, String jarDir ) {
			jarTester = new JarTesterMT( multipleClassLoaderRequired, multiThreadedCompilationRequired );
			this.jarDir = jarDir;
		}

		public void run () {
			jarTester.run( new String[]{jarDir} );
		}

		public JarTesterMT getJartesterMT() {
			return this.jarTester;
		}
	}

	/**
	 * Returns all supported POSIX standard options for JIT Code Cache Test Harness
	 * @return
	 */
	private static Options getOptions() {
		Options options = new Options();
		options.addOption( Constants.JAR_DIR, true, "Full path to the directory containing Jar files to use in the test" );
		options.addOption( Constants.CCTEST_BASIC, false, "Switch to perform basic Code Cache test" );
		options.addOption( Constants.CCTEST_BOUNDARY, false, "Switch to perform Code Cache Boundary test" );
		options.addOption( Constants.CCTEST_SPACE_REUSE, false, "Switch to perform Code Cache Space Reuse test" );
		options.addOption( Constants.CCTEST_TRAMPOLINE_BASIC, false, "Switch to perform Basic Code Cache Trampoline test" );
		options.addOption( Constants.CCTEST_TRAMPOLINE_ADVANCED, false, "Switch to perform Advanced Code Cache Trampoline test" );
		options.addOption( Constants.CCTEST_AOT_PHASE1, false, "Switch to perform Code Cache AOT test - Phase 1");
		options.addOption( Constants.CCTEST_AOT_PHASE2, false, "Switch to perform Code Cache AOT test - Phase 2");
		options.addOption( Constants.CCTEST_JITHELPER, false, "Switch to perform Code Cache JIT Helpers test");
		options.addOption( Constants.CCTEST_TRAMPOLINE_IPIC, false, "Switch to perform Code Cache Trampoline IPIC test");
		return options;
	}

	private static void printUsage( Options options ) {
		HelpFormatter formatter = new HelpFormatter();
		formatter.printHelp( "Code Cache Tester", options );
	}

	/**
	 * Returns the current JIT verbose log from the directory path of vlog being used.
	 * It is useful if timestamp based JIT verbose logs are being generated.
	 * @return
	 */
	private static String getLogFileName() {
		String latestJITLogFilename = null;

		String dirPath = getLodDirPath();

		if ( dirPath == null ) {
			return null;
		}

		File logDirFolder = new File( dirPath );
		String [] fileNames = null;

		while( true ) {
			fileNames = logDirFolder.list();
			if ( fileNames == null || fileNames.length == 0 ) {
				System.out.println( "No JIT verbose log file under "+ dirPath+ ". Sleeping for 5 seconds.." );
				try {
					Thread.sleep( 5000 );
				}catch ( InterruptedException e ) {
					e.printStackTrace();
				}
				continue;
			}
			break;
		}

		long lastUpdateTime = 0;

		for ( int i = 0 ; i < fileNames.length ; i++ ) {
			if ( fileNames[i].startsWith( "jit.log" ) ) {
				String fullJitLogFileName = dirPath + File.separator + fileNames [i];
				File f = new File ( fullJitLogFileName );
				if ( lastUpdateTime < f.lastModified() ) {
					lastUpdateTime = f.lastModified();
					latestJITLogFilename = fullJitLogFileName;
				}
			}
		}
		return latestJITLogFilename;
	}
}
