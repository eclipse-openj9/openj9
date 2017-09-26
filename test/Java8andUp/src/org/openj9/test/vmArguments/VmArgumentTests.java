/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package org.openj9.test.vmArguments;

import static org.openj9.test.util.StringPrintStream.logStackTrace;
import static org.openj9.test.vmArguments.ArgumentDumper.USERARG_TAG;
import static org.testng.Assert.fail;
import static org.testng.AssertJUnit.assertEquals;
import static org.testng.AssertJUnit.assertFalse;
import static org.testng.AssertJUnit.assertNotNull;
import static org.testng.AssertJUnit.assertNull;
import static org.testng.AssertJUnit.assertTrue;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintStream;
import java.lang.Thread.State;
import java.lang.reflect.Method;
import java.net.URISyntaxException;
import java.net.URL;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

import org.openj9.test.util.StringPrintStream;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

/*
 * Jazz 60902: Rewrite JVM options constructor code to fix PMR 13464,422,000.
 * Test argument processing by running the VM with various combinations of command line arguments, options files, and environment variables.
 */
@Test(groups = { "level.extended" })
public class VmArgumentTests {
	private static final String FORCE_COMMON_CLEANER_SHUTDOWN = "-Dibm.java9.forceCommonCleanerShutdown=true";
	private static final String XCHECK_MEMORY = "-Xcheck:memory";
	private static final String XCOMPRESSEDREFS = "-Xcompressedrefs";
	private static final String XNOCOMPRESSEDREFS = "-Xnocompressedrefs";
	private static final String[] XARGENCODING = new String[] {"-Xargencoding"};
	private static final String DJAVA_LIBRARY_PATH = "-D"+"java.library.path";
	private static final String FILE_SEPARATOR = System.getProperty("file.separator");
	private static final String USER_DIR = System.getProperty("user.dir");
	private static final String OS_NAME_PROPERTY = System.getProperty("os.name");
	private static final String vmargsJarFilename;
	private static final String XJIT = "-Xjit";
	private static final String XINT = "-Xint";
	private static final String XPROD = "-Xprod";
	private static final String HELLO_WORLD_SCRIPT = "#!/bin/sh\n"+ "echo hello, world\n";

	private static final String IBM_JAVA_OPTIONS = "IBM_JAVA_OPTIONS";
	private static final String JAVA_TOOL_OPTIONS = "JAVA_TOOL_OPTIONS";
	private static final String ENV_LD_LIBRARY_PATH = "LD_LIBRARY_PATH";
	private static final String ENV_LIBPATH = "LIBPATH";
	private static final String DJAVA_HOME = "-Djava.home";
	private static final String XOPTIONSFILE = "-Xoptionsfile=";
	private static final String CLASSPATH=System.getProperty("java.class.path");
	private static final String JAVA_COMMAND=System.getProperty("java.home")+File.separatorChar+"bin"+File.separatorChar+"java";
	private static final String APPLICATION_NAME = ArgumentDumper.class.getCanonicalName();

	private static final String[] mandatoryArgumentsJava8;

	private static final String[] mandatoryArguments;
	private static final String[] TEST_ARG_LIST = {"-Dtest.option1=testJavaToolOptions1", "-Dtest.option2=testJavaToolOptions2", 
			"-Dtest.option3=testJavaToolOptions3", "-Dtest.option4=testJavaToolOptions4"};
	private static final String OPTIONS_FILE_SUFFIX = ".test_options_file";
	private static final String DASH_D_CMDLINE_ARG = "-DcmdlineArg";

	private static final String MIXED_MODE_THRESHOLD_VALUE = "1";
	private static final String IBM_MIXED_MODE_THRESHOLD = "IBM_MIXED_MODE_THRESHOLD";
	private static final String  MAPOPT_XJIT_COUNT_EQUALS = "-Xjit:count="+MIXED_MODE_THRESHOLD_VALUE;

	private static final String JAVA_COMPILER = "JAVA_COMPILER";
	private static final String JAVA_COMPILER_VALUE=System.getProperty("java.compiler");
	private static final String SYSPROP_DJAVA_COMPILER_EQUALS = "-Djava.compiler="+JAVA_COMPILER_VALUE;

	private static final String JAVA_VERSION;
	private static final boolean isJava8;

	private static final String IBM_NOSIGHANDLER = "IBM_NOSIGHANDLER";
	private static final String VMOPT_XRS = "-Xrs";

	private ProcessStreamReader stdoutReader;
	private ProcessStreamReader stderrReader;
	private String testName;

	protected static Logger logger = Logger.getLogger(VmArgumentTests.class);

	static {
		boolean isIbm = System.getProperty("java.vm.vendor").equals("IBM Corporation");
		mandatoryArgumentsJava8 = new String[] {
				isIbm?XOPTIONSFILE:null,
						"-Xlockword:mode",
						"-Xjcl:",
						"-Dcom.ibm.oti.vm.bootstrap.library.path",
						"-Dsun.boot.library.path",
						"-Djava.library.path",
						"-Djava.ext.dirs",
						DJAVA_HOME,
						"-Duser.dir",
						/* "-Djava.runtime.version", doesn't show up if -Xcheck:memory is used */
						"-Djava.class.path",
						"-Dsun.java.command",
						"-Dsun.java.launcher"
		};

		mandatoryArguments = new String[] {
				isIbm?XOPTIONSFILE:null,
						"-Xlockword:mode",
						"-Xjcl:",
						"-Dcom.ibm.oti.vm.bootstrap.library.path",
						"-Dsun.boot.library.path",
						"-Djava.library.path",
						DJAVA_HOME,
						"-Duser.dir",
						/* "-Djava.runtime.version", doesn't show up if -Xcheck:memory is used */
						"-Djava.class.path",
						"-Dsun.java.command",
						"-Dsun.java.launcher"
		};	
		JAVA_VERSION=System.getProperty("java.version");
		isJava8 = JAVA_VERSION.startsWith("1.8.0");
		vmargsJarFilename = isJava8? "vmargs_SE80.jar": "vmargs_SE90.jar";
	}

	/* 
	 * set the method name in case the test needs it.
	 * Delineate the tests in the log file.
	 */
	@BeforeMethod
	protected void setUp(Method testMethod) throws Exception {
		testName = testMethod.getName(); 
		logger.debug("\n------------------------------------------------------\nstarting " + testName);
	}

	/* sanity test */
	public void testNoOptions() {
		ArrayList<String> actualArguments = null;
		try {
			ProcessBuilder pb = makeProcessBuilder(new String[] {}, CLASSPATH);
			actualArguments = runAndGetArgumentList(pb);
			checkArguments(actualArguments, new String[] {});
		} catch (AssertionError e) {
			dumpDiagnostics(e, actualArguments);
			throw e;
		}
	}

	
	public void testWindowsPath() {
		if (isWindows()) {
			ArrayList<String> actualArguments = null;
			boolean foundPath = false;
			try {
				ProcessBuilder pb = makeProcessBuilder(new String[] {}, CLASSPATH);
				actualArguments = runAndGetArgumentList(pb);
				for (String arg: actualArguments) {
					if (arg.startsWith("-Djava.library.path")) {
						foundPath = true;
						assertTrue("Windows java.library.path missing '.'", arg.endsWith("."));
					}
				}
				assertTrue("java.library.path not found", foundPath);
			} catch (AssertionError e) {
				dumpDiagnostics(e, actualArguments);
				throw e;
			}
		}
	}

	/* PMR 83349,500,624: Command line property "-Djava.util.prefs.PreferencesFactory" value not set or overwritten with default property value */

	public void testPreferencesFactory() {
		final String PREFS_FACTORY="java.util.prefs.PreferencesFactory";
		final String D_PREFS_FACTORY = "-D"+PREFS_FACTORY + '=';
		final String PREFS_FACTORY_VALUE="testPreferencesFactory";

		ArrayList<String> cmdlineArguments = new ArrayList<String>(1);
		HashMap<String, String> expectedProperties = new HashMap<String, String>();
		expectedProperties.put(PREFS_FACTORY, PREFS_FACTORY_VALUE);
		cmdlineArguments.add(D_PREFS_FACTORY + PREFS_FACTORY_VALUE);
		final String[] cmdlineArgs = cmdlineArguments.toArray(new String[cmdlineArguments.size()]);
		ProcessBuilder pb = makeProcessBuilder(cmdlineArgs, CLASSPATH);
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		try {
			checkArguments(actualArguments, cmdlineArgs);
			checkSystemPropertyValues(expectedProperties);
		} catch (AssertionError e) {
			dumpDiagnostics(e, actualArguments);
			throw e;
		}
	}

	
	public void testSystemProperties() {
		ArrayList<String> cmdlineArguments = new ArrayList<String>(1);
		HashMap<String, String> expectedProperties = new HashMap<String, String>();
		String bfuProperties[] = {
				"java.awt.graphicsenv",
				"java.awt.fonts",
				"java.util.prefs.PreferencesFactory",
				"awt.toolkit",
				"java.awt.printerjob",
				"java.awt.graphicsenv",
				"java.awt.fonts",
				"awt.toolkit",
				"java.awt.printerjob",
				"java.util.prefs.PreferencesFactory",
				"sun.arch.data.model",
				"sun.io.unicode.encoding"
		};
		int count = 1;
		for (String s: bfuProperties) {
			final String propValue = s+"_testValue"+count;
			expectedProperties.put(s, propValue);
			cmdlineArguments.add("-D"+s+'='+propValue);
			++count;
		}
		final String[] cmdlineArgs = cmdlineArguments.toArray(new String[cmdlineArguments.size()]);
		ProcessBuilder pb = makeProcessBuilder(cmdlineArgs, CLASSPATH);
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		try {
			checkArguments(actualArguments, cmdlineArgs);
			checkSystemPropertyValues(expectedProperties);
		} catch (AssertionError e) {
			dumpDiagnostics(e, actualArguments);
			throw e;
		}
	}

	/* -Xint test */
	
	public void testXint() {
		ArrayList<String> actualArguments = new ArrayList<String>(1);
		try {
			actualArguments.add(XINT);
			final String[] actualArgs = actualArguments.toArray(new String[actualArguments.size()]);
			ProcessBuilder pb = makeProcessBuilder(actualArgs, CLASSPATH);
			actualArguments = runAndGetArgumentList(pb);
			checkArguments(actualArguments, actualArgs);
		} catch (AssertionError e) {
			dumpDiagnostics(e, actualArguments);
			throw e;
		}
	}

	/* -Xcheck:memory test */
	
	public void testXcheckMemory() {
		ArrayList<String> actualArguments = new ArrayList<String>(1);
		try {
			actualArguments.add(XINT);
			actualArguments.add(XCHECK_MEMORY);
			actualArguments.add(FORCE_COMMON_CLEANER_SHUTDOWN);
			final String[] actualArgs = actualArguments.toArray(new String[actualArguments.size()]);
			ProcessBuilder pb = makeProcessBuilder(actualArgs, CLASSPATH);
			actualArguments = runAndGetArgumentList(pb);
			checkArguments(actualArguments, actualArgs);
		} catch (AssertionError e) {
			dumpDiagnostics(e, actualArguments);
			throw e;
		}
	}

	/* test IBM_JAVA_OPTIONS environment variable */
	
	public void testIbmJavaOptions() {
		ProcessBuilder pb = makeProcessBuilder(new String[] {}, CLASSPATH);
		Map<String, String> env = pb.environment();
		String ibmJavaOptionsArg = "-Dtest.name=testIbmJavaOptions";
		env.put(IBM_JAVA_OPTIONS, ibmJavaOptionsArg);
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, new String[] {ibmJavaOptionsArg});
		assertTrue("missing argument: "+ibmJavaOptionsArg, argumentPositions.containsKey(ibmJavaOptionsArg));
		/* environment variables should come after implicit arguments */
		assertTrue(IBM_JAVA_OPTIONS+ " should come last", argumentPositions.get(ibmJavaOptionsArg).intValue() > argumentPositions.get(DJAVA_HOME).intValue()); 
	}

	/* test IBM_JAVA_OPTIONS environment variable */
	
	public void testArgEncodingInIbmJavaOptions() {
		ProcessBuilder pb = makeProcessBuilder(new String[] {}, CLASSPATH);
		Map<String, String> env = pb.environment();
		String ibmJavaOptionsArg = "-Xargencoding";
		env.put(IBM_JAVA_OPTIONS, ibmJavaOptionsArg);
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, new String[] {ibmJavaOptionsArg});
		assertTrue("missing argument: "+ibmJavaOptionsArg, argumentPositions.containsKey(ibmJavaOptionsArg));
		/* environment variables should come after implicit arguments */
		assertTrue(IBM_JAVA_OPTIONS+ " should come last", argumentPositions.get(ibmJavaOptionsArg).intValue() > argumentPositions.get(DJAVA_HOME).intValue());
	}


	
	public void testCrNocr() {
		/* 76036: OTT:PPCLE Command-line option unrecognised: -Xcompressedrefs */
		String noCrArg = XNOCOMPRESSEDREFS;
		ProcessBuilder pb = makeProcessBuilder(new String[] {noCrArg}, CLASSPATH);
		Map<String, String> env = pb.environment();
		String crArg = XCOMPRESSEDREFS;
		env.put(IBM_JAVA_OPTIONS, crArg);
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, new String[] {XCOMPRESSEDREFS, XNOCOMPRESSEDREFS});
		assertTrue("missing argument: "+crArg, argumentPositions.containsKey(crArg));
		assertTrue("missing argument: "+noCrArg, argumentPositions.containsKey(noCrArg));
		assertTrue(noCrArg+ " should come after "+DJAVA_HOME, argumentPositions.get(crArg).intValue() < argumentPositions.get(noCrArg).intValue());
	}

	/* test options in runnable jar files */
	
	public void testJarArguments() throws URISyntaxException {
		URL jarUrl = ClassLoader.getSystemClassLoader().getResource(vmargsJarFilename);
		assertNotNull(vmargsJarFilename+" not found", jarUrl);
		Path jarPath = null;
			jarPath = Paths.get(jarUrl.toURI()).toFile().toPath();
			checkJarArgs(jarPath.toString());
	}

	/**
	 * Test getting VM arguments from a JAR file prefixed by a shell script.
	 */
	public void testExecutableJarArguments() {
		final String HWVMARGS = "hwvmargs.jar";
		URL jarUrl = ClassLoader.getSystemClassLoader().getResource(vmargsJarFilename);
		final String pathString = HWVMARGS;
		assertNotNull(pathString+" not found", jarUrl);		
		Path jarPath = null;
		try {
			jarPath = Paths.get(jarUrl.toURI()).toFile().toPath();
		} catch (URISyntaxException e) {
			fail("cannot convert "+jarUrl.toString()+" to path", e);
		}		
		
		try {			
			File outFile = new File(pathString);
			outFile.delete();
			FileOutputStream outStream = new FileOutputStream(outFile);
			outStream.write(HELLO_WORLD_SCRIPT.getBytes());
			Files.copy(jarPath, outStream);
			outStream.close();
		} catch (IOException e) {
			logStackTrace(e, logger);
			fail("Error creating "+pathString+" from "+jarPath, e);
		}		
		checkJarArgs(pathString);
	}

	/* test JAVA_TOOL_OPTIONS environment variable */
	
	public void testJavaToolOptions() {
		ProcessBuilder pb = makeProcessBuilder(new String[] {}, CLASSPATH);
		Map<String, String> env = pb.environment();
		String javaToolOptionsArg = "-Dtest.name=testJavaToolOptions";
		env.put(JAVA_TOOL_OPTIONS, javaToolOptionsArg);
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, new String[] {javaToolOptionsArg});
		assertTrue("missing argument: "+javaToolOptionsArg, argumentPositions.containsKey(javaToolOptionsArg));
		/* environment variables should come after implicit arguments */
		assertTrue(JAVA_TOOL_OPTIONS+ " should come last", argumentPositions.get(javaToolOptionsArg).intValue() > argumentPositions.get(DJAVA_HOME).intValue());
	}

	/* test JAVA_TOOL_OPTIONS environment variable */
	
	public void testOptionsWithLeadingSpacesInEnvVars() {
		ArrayList<String> actualArguments = null;
		try {
			final String javaToolOptionsArg = "-Dtest.name=testOptionsWithLeadingSpacesInEnvVars";
			ProcessBuilder pb = makeProcessBuilder(new String[] {}, CLASSPATH);
			Map<String, String> env = pb.environment();
			env.put(JAVA_TOOL_OPTIONS, "          "+javaToolOptionsArg+"               ");
			actualArguments = runAndGetArgumentList(pb);
			final String[] expectedArguments = new String[] {javaToolOptionsArg};
			HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArguments);
			checkArgumentSequence(expectedArguments, argumentPositions, true);
		} catch (AssertionError e) {
			dumpDiagnostics(e,actualArguments);
			throw e;
		}
	}

	/* test arguments containing newlines in the environment variables */
	
	public void testMultipleArgumentsInEnvVars() {
		ArrayList<String> actualArguments = null;
		try {
			final String JTO1 = "-Dtest.javatooloption1=foo";
			final String JTO2 = "-Dtest.javatooloption2=bar";
			final String javaToolOptionsArg = JTO1+'\n' + JTO2; 
			final String IJO1 = "-Dtest.ibmjavaoption1=hello";
			final String IJO2 = "-Dtest.ibmjavaoption2=goodbye";
			final String ibmJavaOptionsArg = IJO1+'\n' + IJO2;
			ProcessBuilder pb = makeProcessBuilder(new String[] {}, CLASSPATH);
			Map<String, String> env = pb.environment();
			env.put(JAVA_TOOL_OPTIONS, javaToolOptionsArg);
			env.put(IBM_JAVA_OPTIONS, ibmJavaOptionsArg);
			actualArguments = runAndGetArgumentList(pb);
			final String[] expectedArguments = new String[] {JTO1, JTO2, IJO1, IJO2};
			HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArguments);
			checkArgumentSequence(expectedArguments, argumentPositions, true);
		} catch (AssertionError e) {
			dumpDiagnostics(e,actualArguments);
			throw e;
		}
	}

	/* test environment variables which map to JVM options */
	
	public void testMappedOptions() {
		ArrayList<String> actualArguments = null;
		try {
			ProcessBuilder pb = makeProcessBuilder(new String[] {}, CLASSPATH);
			Map<String, String> env = pb.environment();
			env.put(IBM_MIXED_MODE_THRESHOLD, MIXED_MODE_THRESHOLD_VALUE);
			env.put(JAVA_COMPILER, JAVA_COMPILER_VALUE);
			env.put(IBM_NOSIGHANDLER, "true"); /* CMVC 196206 - on some Windows versions, the value must be non-null */
			final String ibmToolOpt = "-Dtest.name=testMappedOptions";
			String ibmJavaOptionsArg = ibmToolOpt;
			env.put(IBM_JAVA_OPTIONS, ibmJavaOptionsArg);

			actualArguments = runAndGetArgumentList(pb);
			final String[] expectedArguments = new String[] {MAPOPT_XJIT_COUNT_EQUALS, SYSPROP_DJAVA_COMPILER_EQUALS, VMOPT_XRS, ibmJavaOptionsArg};
			HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArguments);
			checkArgumentSequence(expectedArguments, argumentPositions, true);
			/* mapped options in environment variables should come before other non-implicit arguments */
			assertTrue("Mapped options should come at the beginning of the list", argumentPositions.get(MAPOPT_XJIT_COUNT_EQUALS).intValue() < argumentPositions.get(VMOPT_XRS).intValue());				
		} catch (AssertionError e) {
			dumpDiagnostics(e,actualArguments);
			throw e;
		}
	}

	/* test environment variables which map to JVM options. Jazz 75469 */
	
	public void testNullMappedOptions() {
		ArrayList<String> actualArguments = null;
		try {
			ProcessBuilder pb = makeProcessBuilder(new String[] {}, CLASSPATH);
			Map<String, String> env = pb.environment();
			env.put(IBM_MIXED_MODE_THRESHOLD, "");
			env.put(JAVA_COMPILER, "");
			env.put(IBM_NOSIGHANDLER, ""); /* CMVC 196206 - on some Windows versions, the value must be non-null */
			final String ibmToolOpt = "-Dtest.name=testMappedOptions";
			String ibmJavaOptionsArg = ibmToolOpt;
			env.put(IBM_JAVA_OPTIONS, ibmJavaOptionsArg);

			actualArguments = runAndGetArgumentList(pb);
			final String[] expectedArguments = new String[] {ibmJavaOptionsArg};
			HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArguments);
			checkArgumentSequence(expectedArguments, argumentPositions, true);
		} catch (AssertionError e) {
			dumpDiagnostics(e,actualArguments);
			throw e;
		}
	}

	/* concatenate several VM options into a single environment variable */
	
	public void testMultipleOptionsInEnvironmentVariable() {
		ProcessBuilder pb = makeProcessBuilder(new String[] {}, CLASSPATH);
		Map<String, String> env = pb.environment();
		String javaToolOptionsArg = TEST_ARG_LIST[0]+' '+TEST_ARG_LIST[1]+' '+TEST_ARG_LIST[2]+' '+TEST_ARG_LIST[3];
		env.put(JAVA_TOOL_OPTIONS, javaToolOptionsArg);
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, TEST_ARG_LIST);

		checkArgumentSequence(TEST_ARG_LIST, argumentPositions, true);
	}

	/* sanity test for plain old command line tests */
	
	public void testCommandlineArguments() {
		ProcessBuilder pb = makeProcessBuilder(TEST_ARG_LIST, CLASSPATH);
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, TEST_ARG_LIST);
		checkArgumentSequence(TEST_ARG_LIST, argumentPositions, true);
	}

	
	public void testExtremelyLongArgument() {
		StringBuilder longArgBuilder = new StringBuilder(20000);
		String longArg = "Ax";
		while (longArgBuilder.length() < 10000) {
			longArgBuilder.append('A');
			longArgBuilder.append(longArg);
			longArg = longArgBuilder.toString();
		}

		String[] vmArgs = new String[] {"-Dtest.preamble", "-Dtest.data="+longArg, "-Dtest.postamble"};
		ProcessBuilder pb = makeProcessBuilder(vmArgs, CLASSPATH);
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, vmArgs);
		checkArgumentSequence(vmArgs, argumentPositions, true);
	}

	/* verify that -Xprod is removed from the command line */
	
	public void testXprod() {

		ArrayList<String> cmdlineArgsBuffer = new ArrayList<String>(1);
		try {
			String[] vmArgs = new String[] {XINT, XJIT};
			cmdlineArgsBuffer.add(XPROD);
			cmdlineArgsBuffer.add(XINT);
			cmdlineArgsBuffer.add(XPROD);
			cmdlineArgsBuffer.add(XPROD);
			cmdlineArgsBuffer.add(XJIT);
			final String[] cmdlineArgs = cmdlineArgsBuffer.toArray(new String[cmdlineArgsBuffer.size()]);
			ProcessBuilder pb = makeProcessBuilder(cmdlineArgs, CLASSPATH);
			ArrayList<String> actualArgs = runAndGetArgumentList(pb);
			HashMap<String, Integer> argumentPositions = checkArguments(actualArgs, cmdlineArgs);
			checkArgumentSequence(vmArgs, argumentPositions, true);
		} catch (AssertionError e) {
			dumpDiagnostics(e, cmdlineArgsBuffer);
			throw e;
		}
	}

	
	public void testDisableXcheckMemory() {

		ArrayList<String> actualArguments = null;
		try {
			ArrayList<String> cmdlineArgBuffer = new ArrayList<String>();
			ArrayList<String> expectedArgBuffer = new ArrayList<String>();
			String xCheckMemory=XCHECK_MEMORY;
			cmdlineArgBuffer.add(xCheckMemory);
			expectedArgBuffer.add(xCheckMemory);

			String xCheckClasspath="-Xcheck:none";
			cmdlineArgBuffer.add(xCheckClasspath);
			expectedArgBuffer.add(xCheckClasspath);

			ProcessBuilder pb = makeProcessBuilder(cmdlineArgBuffer.toArray(new String[cmdlineArgBuffer.size()]), CLASSPATH);

			actualArguments = runAndGetArgumentList(pb);
			final String[] expectedArgs = expectedArgBuffer.toArray(new String[expectedArgBuffer.size()]);
			HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArgs);
			checkArgumentSequence(expectedArgs, argumentPositions, false);
			String stderrText = stderrReader.getStreamOutput();
			assertFalse("All allocated blocks were freed.", stderrText.contains( "-Xcheck:memory failed"));
		} catch (AssertionError e) {
			dumpDiagnostics(e,actualArguments);
			throw e;
		}
	}

	
	public void testVerboseInitOptions() {
		ArrayList<String> actualArguments = new ArrayList<String>(1);
		try {
			actualArguments.add("-verbose:init");
			final String[] actualArgs = actualArguments.toArray(new String[actualArguments.size()]);
			ProcessBuilder pb = makeProcessBuilder(actualArgs, CLASSPATH);
			actualArguments = runAndGetArgumentList(pb);
			checkArguments(actualArguments, actualArgs);
			String stderrText = stderrReader.getStreamOutput();
			assertTrue("-verbose:init failed", stderrText.contains("Checking results for stage"));
		} catch (AssertionError e) {
			dumpDiagnostics(e, actualArguments);
			throw e;
		}
	}

	
	public void testCheckMemoryViaEnvironmentVariable() {

		ArrayList<String> cmdlineArguments = new ArrayList<String>(1);
		try {
			cmdlineArguments.add(XINT);
			final String[] actualArgs = cmdlineArguments.toArray(new String[cmdlineArguments.size()]);
			ProcessBuilder pb = makeProcessBuilder(actualArgs, CLASSPATH);
			Map<String, String> env = pb.environment();
			env.put("IBM_MALLOCTRACE", "true");
			cmdlineArguments = runAndGetArgumentList(pb);
			/* Ensure the child process has finished and flushed its stderr */
			try {
				stderrReader.join(10000);
			} catch (InterruptedException e) {
				fail("stderr reader join was interrupted");
			}
			assertTrue("stderr reader join failed", stderrReader.getState() == State.TERMINATED);
			checkArguments(cmdlineArguments, actualArgs);
			String stderrText = stderrReader.getStreamOutput();
			assertTrue("check memory using environment variable failed: stderr = "+stderrText, stderrText.contains("Memory checker statistics:"));
		} catch (AssertionError e) {
			dumpDiagnostics(e, cmdlineArguments);
			throw e;
		}

	}

	/* test whitespace before and after  */
	
	public void testCommandlineArgumentsWithLeadingAndTrailingSpaces() {
		ArrayList<String> actualArguments=null;
		try {

			String cmdlineArg1 = "-Dtest.name=cmdlineArg1";
			String cmdlineArg2 = "-Dtest.name=cmdlineArg2";
			String[] vmArgs = new String[] {cmdlineArg1+"                 ", "                "+cmdlineArg2};
			ProcessBuilder pb = makeProcessBuilder(vmArgs, CLASSPATH);
			int exitStatus = runAndGetExitStatus(pb);
			assertFalse("VM did not fail on empty argument", exitStatus == 0);
		} catch (AssertionError e) {
			dumpDiagnostics(e,actualArguments);
			throw e;
		}
	}

	/* IBM_JAVA_OPTIONS should take priority over JAVA_TOOL_OPTIONS */
	
	public void testEnvironmentVariableOrdering() {
		ProcessBuilder pb = makeProcessBuilder(new String[] {}, CLASSPATH);
		Map<String, String> env = pb.environment();
		String javaToolOptionsArg = "-Dtest.name1=javaToolOptionsArg";
		String ibmJavaOptionsArg = "-Dtest.name2=ibmJavaOptionsArg";
		env.put(JAVA_TOOL_OPTIONS, javaToolOptionsArg);
		env.put(IBM_JAVA_OPTIONS, ibmJavaOptionsArg);
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, new String[] {ibmJavaOptionsArg, javaToolOptionsArg});
		assertTrue("missing argument: "+ibmJavaOptionsArg, argumentPositions.containsKey(ibmJavaOptionsArg));
		assertTrue("missing argument: "+javaToolOptionsArg, argumentPositions.containsKey(javaToolOptionsArg));
		assertTrue(IBM_JAVA_OPTIONS+ " should come after "+JAVA_TOOL_OPTIONS, argumentPositions.get(ibmJavaOptionsArg).intValue() > argumentPositions.get(javaToolOptionsArg).intValue());
	}

	/*
	 * OS names:
	 * Linux
	 * AIX
	 * z/OS
	 */
	
	public void testLD_LIBRARY_PATH () {
		if (!OS_NAME_PROPERTY.startsWith("Linux")) {
			logger.debug("Skipping "+testName+" on non-Linux systems");
			return;
		}
		if ((null == USER_DIR) || (USER_DIR.length() == 0)) {
			fail("user.dir property not set");
		}

		ProcessBuilder pb = makeProcessBuilder(new String[] {}, CLASSPATH);
		Map<String, String> env = pb.environment();
		String ldLibraryPath = USER_DIR+FILE_SEPARATOR+testName;
		env.put(ENV_LD_LIBRARY_PATH, ldLibraryPath);
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		String javaPathDir = getArgumentValue(actualArguments, DJAVA_LIBRARY_PATH);
		assertTrue(DJAVA_LIBRARY_PATH+"=\n\""+javaPathDir+"\"\ndoes not contain value from "+ENV_LD_LIBRARY_PATH,
				javaPathDir.contains(ldLibraryPath));
	}

	
	public void testLD_LIBRARY_PATH_And_LIBPATH () {
		if (!OS_NAME_PROPERTY.startsWith("AIX")) {
			logger.debug("Skipping "+testName+" on non-AIX systems");
			return;
		}
		if ((null == USER_DIR) || (USER_DIR.length() == 0)) {
			fail("user.dir property not set");
		}
		String tmpDir = System.getProperty("java.io.tmpdir");
		if ((null == tmpDir) || (tmpDir.length() == 0)) {
			fail("java.io.tmpdir property not set");
		}
		ProcessBuilder pb = makeProcessBuilder(new String[] {}, CLASSPATH);
		Map<String, String> env = pb.environment();
		String libpath = tmpDir+FILE_SEPARATOR+testName;
		String ldLibraryPath = USER_DIR+FILE_SEPARATOR+testName;
		env.put(ENV_LD_LIBRARY_PATH, ldLibraryPath);
		env.put(ENV_LIBPATH, libpath);
		String expectedSubstring = libpath+File.pathSeparatorChar+ldLibraryPath;
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		String javaPathDir = getArgumentValue(actualArguments, DJAVA_LIBRARY_PATH);
		assertTrue(DJAVA_LIBRARY_PATH+"=\n\""+javaPathDir+"\"\ndoes not contain value from "+ENV_LD_LIBRARY_PATH,
				javaPathDir.contains(expectedSubstring));
	}

	
	public void testLIBPATH () {
		if (!OS_NAME_PROPERTY.startsWith("z/OS")) {
			logger.debug("Skipping "+testName+" on non-z/OS systems");
			return;
		}
		if ((null == USER_DIR) || (USER_DIR.length() == 0)) {
			fail("user.dir property not set");
		}
		ProcessBuilder pb = makeProcessBuilder(new String[] {}, CLASSPATH);
		Map<String, String> env = pb.environment();
		String libpath = USER_DIR+FILE_SEPARATOR+testName;
		env.put(ENV_LIBPATH, libpath);
		String expectedSubstring = libpath;
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		String javaPathDir = getArgumentValue(actualArguments, DJAVA_LIBRARY_PATH);
		assertTrue(DJAVA_LIBRARY_PATH+"=\n\""+javaPathDir+"\"\ndoes not contain value from "+ENV_LD_LIBRARY_PATH,
				javaPathDir.contains(expectedSubstring));
	}

	/* -Xservice=<arg> puts <arg> at the end of the processed argument list */
	
	public void testServiceOptions() {
		String svcArg = "-Dtest.name=testServiceOptions";
		String xServiceArgString = "-Xservice=" + svcArg;
		String[] vmArgs = new String[] {xServiceArgString};
		ProcessBuilder pb = makeProcessBuilder(vmArgs, CLASSPATH);
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, new String[] {svcArg, xServiceArgString});
		Integer scvArgPosn = argumentPositions.get(svcArg);
		assertNotNull("argument missing: "+svcArg, scvArgPosn);
		int numArgs = actualArguments.size();
		assertEquals(xServiceArgString+" not last argument", numArgs-1, scvArgPosn.intValue());
	}

	/* test multiple instances of -Xservice=<arg>.  The last one takes priority. */
	
	public void testMultipleServiceArgs() {
		ArrayList<String> actualArguments=null;
		try {

			String svcArg1 = "-Dtest.name=testServiceOptions1";
			String xServiceArg1String = "-Xservice=" + svcArg1;
			String svcArg2 = "-Dtest.name=testServiceOptions2";
			String xServiceArg2String = "-Xservice=" + svcArg2;
			String[] vmArgs = new String[] {xServiceArg1String, xServiceArg2String};
			ProcessBuilder pb = makeProcessBuilder(vmArgs, CLASSPATH);
			actualArguments = runAndGetArgumentList(pb);
			final String[] expectedArgs = new String[] {xServiceArg1String, xServiceArg2String, svcArg2};
			HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArgs);
			checkArgumentSequence(expectedArgs, argumentPositions, false);
		} catch (AssertionError e) {
			dumpDiagnostics(e,actualArguments);
			throw e;
		}
	}

	/* -Xservice argument is not parsed if it is in an environment variable */
	
	public void testServiceArgsInEnvironmentVariable() {
		ArrayList<String> actualArguments=null;
		try {
			String svcArg = "-Dtest.name=testServiceOptionsInEnvironmentVariable";
			String xServiceArgString = "-Xservice=" + svcArg;
			String[] expectedArgs = new String[] {xServiceArgString};
			ProcessBuilder pb = makeProcessBuilder(new String[] {}, CLASSPATH);
			Map<String, String> env = pb.environment();
			env.put(IBM_JAVA_OPTIONS, xServiceArgString);

			actualArguments = runAndGetArgumentList(pb);
			HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArgs);
			assertNull("Argument to -Xservice should not be placed in command line if -Xservice is in environment variable: "+svcArg, argumentPositions.get(svcArg));
		} catch (AssertionError e) {
			dumpDiagnostics(e,actualArguments);
			throw e;
		}
	}

	/* test multiple arguments in the same -Xservice argument */
	
	public void testServiceMultipleArgs() {
		String serviceFoo = "-Dtest.xservice.opt1=foo";
		String serviceBar = "-Dtest.xservice.opt1=bar";
		String xServiceArg = "-Xservice="+serviceFoo+' '+serviceBar;
		String[] vmArgs = new String[] {xServiceArg};
		ProcessBuilder pb = makeProcessBuilder(vmArgs, CLASSPATH);
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, new String[] {serviceFoo, serviceBar, xServiceArg});
		assertNotNull("argument missing: "+xServiceArg, argumentPositions.get(xServiceArg));
		int numArgs = actualArguments.size();
		int fooPos = argumentPositions.get(serviceFoo).intValue();
		int barPos = argumentPositions.get(serviceBar).intValue();
		assertEquals(fooPos+" not penultimate argument", numArgs-2, fooPos);
		assertEquals(barPos+" not last argument", numArgs-1, barPos);
	}

	/* -Xservice arguments should come after cmdline arguments */
	
	public void testCmdlineServiceArgsOrdering() {
		ArrayList<String> actualArguments=null;
		try {
			final String svcArgContents = "-Dtest.name=testCmdlineServiceOptionsOrdering";
			String xServiceArg = "-Xservice="+svcArgContents;
			String cmdlineArg = "-Dtest.cmdline.arg1";
			String[] vmArgs = new String[] {xServiceArg, cmdlineArg};
			String[] expectedArgs = new String[] {xServiceArg, svcArgContents, cmdlineArg};
			ProcessBuilder pb = makeProcessBuilder(vmArgs, CLASSPATH);
			actualArguments = runAndGetArgumentList(pb);
			HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArgs);

			Integer cmdlineArgPosn = argumentPositions.get(cmdlineArg);
			Integer xServicePosn = argumentPositions.get(xServiceArg);
			Integer svcArgContentsPosn = argumentPositions.get(svcArgContents);
			assertNotNull("argument missing: "+xServiceArg, xServicePosn);
			assertNotNull("argument missing: "+svcArgContents, svcArgContentsPosn);
			final int cmdlineArgPosnIntValue = cmdlineArgPosn.intValue();
			assertTrue(xServiceArg+ " should come before "+cmdlineArg, xServicePosn.intValue() < cmdlineArgPosnIntValue);
			assertTrue(svcArgContents+ " should come after "+cmdlineArg, svcArgContentsPosn.intValue() > cmdlineArgPosnIntValue);
		} catch (AssertionError e) {
			dumpDiagnostics(e,actualArguments);
			throw e;
		}
	}

	/* environment variable arguments should come before cmdline arguments */
	
	public void testEnvironmentCmdlineOrdering() {
		String cmdlineArg = "-Dtest.cmdline.arg1";
		ProcessBuilder pb = makeProcessBuilder(new String[] {cmdlineArg}, CLASSPATH);
		Map<String, String> env = pb.environment();
		String ibmJavaOptionsArg = "-Dtest.name=ibmJavaOptionsArg";
		env.put(IBM_JAVA_OPTIONS, ibmJavaOptionsArg);
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, new String[] {ibmJavaOptionsArg, cmdlineArg});
		assertTrue("missing argument: "+ibmJavaOptionsArg, argumentPositions.containsKey(ibmJavaOptionsArg));
		assertTrue(cmdlineArg+ " should come after "+DJAVA_HOME, argumentPositions.get(ibmJavaOptionsArg).intValue() < argumentPositions.get(cmdlineArg).intValue());
	}

	
	public void testOptionsFile() {
		String optionFilePath = makeOptionsFile("test1", TEST_ARG_LIST);
		String optionFileArg=XOPTIONSFILE+optionFilePath;
		ProcessBuilder pb = makeProcessBuilder(new String[] {optionFileArg}, CLASSPATH);
		String[] expectedArgs = new String[TEST_ARG_LIST.length+1];
		expectedArgs[0]=optionFileArg;
		System.arraycopy(TEST_ARG_LIST, 0, expectedArgs, 1, TEST_ARG_LIST.length);

		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArgs);
		checkArgumentSequence(expectedArgs, argumentPositions, true);
	}

	
	public void testBadOptionsFile() {
		String badOptionFileArg=XOPTIONSFILE+"bogus";
		String missingOptionFileArg=XOPTIONSFILE;
		ProcessBuilder pb = makeProcessBuilder(new String[] {badOptionFileArg, missingOptionFileArg}, CLASSPATH);
		String[] expectedArgs = new String[0];
		if (false) { /* fix this when Issue #399 is fixed.  Change length of expectedArgs */
			expectedArgs[0]=badOptionFileArg;
			expectedArgs[1]=missingOptionFileArg;
		}		
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArgs);
		checkArgumentSequence(expectedArgs, argumentPositions, true);
	}

	
	public void testXargencodingInOptionsFile() {
		String optionFilePath = makeOptionsFile("test1", XARGENCODING);
		String optionFileArg=XOPTIONSFILE+optionFilePath;
		ProcessBuilder pb = makeProcessBuilder(new String[] {optionFileArg}, CLASSPATH);
		String[] expectedArgs = new String[XARGENCODING.length+1];
		expectedArgs[0]=optionFileArg;
		System.arraycopy(XARGENCODING, 0, expectedArgs, 1, XARGENCODING.length);

		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArgs);
		checkArgumentSequence(expectedArgs, argumentPositions, true);
	}

	/* -Xservice in an options file should not be used */
	
	public void testXserviceInOptionsFile() {
		String xServiceArg = "-Xservice="+XINT;
		String optionFilePath = makeOptionsFile("testXserviceInOptionsFile", new String[] {xServiceArg});
		String optionFileArg=XOPTIONSFILE+optionFilePath;
		ProcessBuilder pb = makeProcessBuilder(new String[] {optionFileArg}, CLASSPATH);
		ArrayList<String> expectedArguments=new ArrayList<String>();
		expectedArguments.add(optionFileArg);
		expectedArguments.add(xServiceArg);

		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, 
				expectedArguments.toArray(new String[expectedArguments.size()]));
		assertNull("-Xservice options in options files should not take effect", argumentPositions.get(XINT));
	}

	
	public void testOptionsFileAndNormalArguments() {
		ArrayList<String> expectedArgsBuffer = new ArrayList<String>();
		final String PROLOGUE_ARG="-Dtest.prologue";
		expectedArgsBuffer.add(PROLOGUE_ARG);

		String optionFilePath = makeOptionsFile("test1", TEST_ARG_LIST);
		String optionFileArg=XOPTIONSFILE+optionFilePath;
		expectedArgsBuffer.add(optionFileArg);
		for (String a: TEST_ARG_LIST) {
			expectedArgsBuffer.add(a);
		}

		final String EPILOGUE_ARG="-Dtest.epilogue";
		expectedArgsBuffer.add(EPILOGUE_ARG);

		ProcessBuilder pb = makeProcessBuilder(new String[] {PROLOGUE_ARG, optionFileArg, EPILOGUE_ARG}, CLASSPATH);
		String[] expectedArgs = expectedArgsBuffer.toArray(new String[expectedArgsBuffer.size()]);

		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArgs);
		checkArgumentSequence(expectedArgs, argumentPositions, true);
	}

	
	public void testOptionsFileWithLeadingAndTrailingBlanks() {
		String testArgs[] = new String[TEST_ARG_LIST.length]; 
		System.arraycopy(TEST_ARG_LIST, 0, testArgs, 0, TEST_ARG_LIST.length);
		testArgs[0] = "               "+TEST_ARG_LIST[0];
		testArgs[1] = TEST_ARG_LIST[1]+"               ";
		String optionFilePath = makeOptionsFile("test1", testArgs);
		String optionFileArg=XOPTIONSFILE+optionFilePath;
		ProcessBuilder pb = makeProcessBuilder(new String[] {optionFileArg}, CLASSPATH);
		String[] expectedArgs = new String[TEST_ARG_LIST.length+1];
		expectedArgs[0]=optionFileArg;
		System.arraycopy(TEST_ARG_LIST, 0, expectedArgs, 1, TEST_ARG_LIST.length);

		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArgs);
		checkArgumentSequence(expectedArgs, argumentPositions, true);
	}

	
	public void testMultipleOptionsFiles() {
		String optionFile1Path = makeOptionsFile("test1", TEST_ARG_LIST);
		String optionFile1Arg=XOPTIONSFILE+optionFile1Path;
		String cmdlineArg = "-Dtest.cmdline.arg1";
		String optFile2Content = "-Dtest.postamble";
		String optionFile2Path = makeOptionsFile("test2", new String[] {optFile2Content});
		String optionFile2Arg=XOPTIONSFILE+optionFile2Path;
		ProcessBuilder pb = makeProcessBuilder(new String[] {optionFile1Arg, cmdlineArg, optionFile2Arg}, CLASSPATH);

		ArrayList<String> expectedArgsBuffer = new ArrayList<String>();
		expectedArgsBuffer.add(optionFile1Arg);
		for (String s: TEST_ARG_LIST) {
			expectedArgsBuffer.add(s);
		}
		expectedArgsBuffer.add(cmdlineArg);
		expectedArgsBuffer.add(optionFile2Arg);
		expectedArgsBuffer.add(optFile2Content);

		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		String[] expectedArgs = expectedArgsBuffer.toArray(new String[expectedArgsBuffer.size()]);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArgs);
		checkArgumentSequence(expectedArgs, argumentPositions, true);
	}

	
	public void testXintAndOtherVmArguments() {
		String[] argList = isJava8 ? (new String[] {"-Dtest.preamble", XINT, "-verbose:gc", "-showversion", "-Xmx10M", "-Dtest.postamble"})
		:  (new String[] {"-Dtest.preamble", XINT, "-verbose:gc","-Xmx10M", "-Dtest.postamble"});
		ProcessBuilder pb = makeProcessBuilder(argList, CLASSPATH);
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, argList);
		checkArgumentSequence(argList, argumentPositions, true);
	}

	
	public void testXXArguments() {
		String[] argList = new String[] {"-Dtest.preamble", "-XXallowvmshutdown:true", "-XX:-StackTraceInThrowable", "-XX:undefinedOption", "-Dtest.postamble"};
		ProcessBuilder pb = makeProcessBuilder(argList, CLASSPATH);
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, argList);
		checkArgumentSequence(argList, argumentPositions, true);
	}

	/* specifying -Xoptionsfile=foo inside an options file does not cause foo to be treated as an options file */
	
	public void testOptionsFileInOptionsFile() {
		String optFile2Content = "-Dtest.postamble";
		String optionFile2Path = makeOptionsFile("test2", new String[] {optFile2Content});
		String optionFile2Arg=XOPTIONSFILE+optionFile2Path;

		String optionFile1Path = makeOptionsFile("test1", new String[] {optionFile2Arg});
		String optionFile1Arg=XOPTIONSFILE+optionFile1Path;
		ProcessBuilder pb = makeProcessBuilder(new String[] {optionFile1Arg}, CLASSPATH);

		ArrayList<String> expectedArgsBuffer = new ArrayList<String>();
		expectedArgsBuffer.add(optionFile1Arg);
		expectedArgsBuffer.add(optionFile2Arg);

		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		String[] expectedArgs = expectedArgsBuffer.toArray(new String[expectedArgsBuffer.size()]);
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArgs);
		checkArgumentSequence(expectedArgs, argumentPositions, true);
		assertNull("option file specified in option file should not be processed", argumentPositions.get(optFile2Content));
	}

	/* 
	 * specifying -Xoptionsfile=foo inside an environment variable results in the contents of the options file 
	 * appearing in the argument list, but the -Xoptionsfile= argument itself does not appear in the argument list. 
	 */
	
	public void testOptionsFileInEnvironmentVariable() {
		ArrayList<String> actualArguments = null;
		try {
			String ibmOptFileContent = "-Dtest=1";
			String ibmOptFilePath = makeOptionsFile("test1", new String[] {ibmOptFileContent});
			String ibmOptFileArg=XOPTIONSFILE+ibmOptFilePath;

			String javaOptFileContent = "-Dtest=2";
			String javaOptFilePath = makeOptionsFile("test2", new String[] {javaOptFileContent});
			String javaOptFileArg=XOPTIONSFILE+javaOptFilePath;

			ProcessBuilder pb = makeProcessBuilder(new String[] {}, CLASSPATH);
			Map<String, String> env = pb.environment();
			env.put(IBM_JAVA_OPTIONS, ibmOptFileArg);
			env.put(JAVA_TOOL_OPTIONS, javaOptFileArg);

			actualArguments = runAndGetArgumentList(pb);
			String[] expectedArgs = null;
			/* new VM args processing puts the options file argument in the list for consistency */
			expectedArgs = new String[] {javaOptFileArg, javaOptFileContent, ibmOptFileArg, ibmOptFileContent};
			HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArgs);
			checkArgumentSequence(expectedArgs, argumentPositions, true);
		} catch (AssertionError e) {
			dumpDiagnostics(e,actualArguments);
			throw e;
		}
	}

	
	public void testEmptyArgument() {
		String cmdlineArg = " ";
		String[] vmArgs = new String[] {cmdlineArg};
		ProcessBuilder pb = makeProcessBuilder(vmArgs, CLASSPATH);
		int exitStatus = runAndGetExitStatus(pb);
		assertFalse("VM did not fail on empty argument", exitStatus == 0);
	}

	/* options specified in IBM_JAVA_OPTIONS or JAVA_TOOL_OPTIONS should take precedence over inferred arguments created by the VM */
	
	public void testOverrideExtDirs() {
		ArrayList<String> actualArguments = null;
		try {
			ProcessBuilder pb = makeProcessBuilder(new String[] {}, CLASSPATH);
			Map<String, String> env = pb.environment();
			String ibmJavaOptionsArg = "-Dtest.name=testIbmJavaOptions";
			env.put(IBM_JAVA_OPTIONS, ibmJavaOptionsArg);
			String javaToolOptionsArg = "-Dtest.name=testJavaToolOptions";
			env.put(JAVA_TOOL_OPTIONS, javaToolOptionsArg);
			actualArguments = runAndGetArgumentList(pb);
			HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, new String[] {javaToolOptionsArg, ibmJavaOptionsArg});
			assertTrue("missing argument: "+ibmJavaOptionsArg, argumentPositions.containsKey(ibmJavaOptionsArg));
			assertTrue("missing argument: "+javaToolOptionsArg, argumentPositions.containsKey(javaToolOptionsArg));
			int extDirsPosn = argumentPositions.get(DJAVA_HOME).intValue();
			assertTrue(JAVA_TOOL_OPTIONS+ " should come after "+DJAVA_HOME, argumentPositions.get(javaToolOptionsArg).intValue() > extDirsPosn); /* this will fail on current VMs */
			assertTrue(IBM_JAVA_OPTIONS+ " should come after "+DJAVA_HOME, argumentPositions.get(ibmJavaOptionsArg).intValue() > extDirsPosn); /* this will fail on current VMs */
		} catch (AssertionError e) {
			dumpDiagnostics(e,actualArguments);
			throw e;
		}
	}

	/* Test the following together:
	 * IBM_JAVA_OPTIONS
	 * JAVA_TOOL_OPTIONS
	 * command line options
	 * options file 
	 * -XX options
	 * -Xservice options
	 */
	
	public void testEverything() {
		ArrayList<String> actualArguments = null;
		try {
			ArrayList<String> cmdlineArgBuffer = new ArrayList<String>();
			ArrayList<String> expectedArgBuffer = new ArrayList<String>();
			final String envOptionsFileContent = "-Dtest.env.options.file=test1";
			String envOptionFilePath = makeOptionsFile("test2", new String[] {envOptionsFileContent});
			String envOptionFileArg=XOPTIONSFILE+envOptionFilePath;

			String javaToolOptionsArg = "-Dtest.name=testJavaToolOptions";
			String ibmJavaOptionsArg = "-Dtest.name=testIbmJavaOptions";
			final String envOptionsList = ibmJavaOptionsArg+' '+envOptionFileArg; /* keep this separate because the -Xoptionsfile arg is deleted */
			expectedArgBuffer.add(javaToolOptionsArg);
			expectedArgBuffer.add(ibmJavaOptionsArg);
			expectedArgBuffer.add(envOptionsFileContent);

			String svcArg = "-Dtest.name=testServiceOptions";
			String xServiceArgString = "-Xservice=" + svcArg;
			cmdlineArgBuffer.add(xServiceArgString);
			String cmdlineOption = "-Dtest.cmdlineOption=1";
			cmdlineArgBuffer.add(cmdlineOption);

			String xxArgument="-XXallowvmshutdown:true";
			cmdlineArgBuffer.add(xxArgument);
			expectedArgBuffer.add(xxArgument);

			final String optionsFileContent = "-Dtest.options.file=test1";
			String optionFilePath = makeOptionsFile("test1", new String[] {optionsFileContent});
			String optionFileArg=XOPTIONSFILE+optionFilePath;
			cmdlineArgBuffer.add(optionFileArg);
			cmdlineArgBuffer.add(optionsFileContent);
			expectedArgBuffer.add(optionFileArg);
			expectedArgBuffer.add(optionsFileContent);
			expectedArgBuffer.add(svcArg);

			ProcessBuilder pb = makeProcessBuilder(cmdlineArgBuffer.toArray(new String[cmdlineArgBuffer.size()]), CLASSPATH);
			Map<String, String> env = pb.environment();
			env.put(IBM_JAVA_OPTIONS, envOptionsList);
			env.put(JAVA_TOOL_OPTIONS, javaToolOptionsArg);

			actualArguments = runAndGetArgumentList(pb);
			final String[] expectedArgs = expectedArgBuffer.toArray(new String[expectedArgBuffer.size()]);
			HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArgs);
			checkArgumentSequence(expectedArgs, argumentPositions, false);
		} catch (AssertionError e) {
			dumpDiagnostics(e,actualArguments);
			throw e;
		}
	}

	/* 
	 * Test multiple options with -Xcheck:memory
	 */
	
	public void testMemoryLeaks() {
		ArrayList<String> actualArguments = null;
		try {
			ArrayList<String> cmdlineArgBuffer = new ArrayList<String>();
			ArrayList<String> expectedArgBuffer = new ArrayList<String>();
			final String envOptionsFileContent = "-Dtest.env.options.file=test1";
			String envOptionFilePath = makeOptionsFile("test2", new String[] {envOptionsFileContent});
			String envOptionFileArg=XOPTIONSFILE+envOptionFilePath;

			String javaToolOptionsArg = "-Dtest.name=testJavaToolOptions";
			String ibmJavaOptionsArg = "-Dtest.name=testIbmJavaOptions";
			final String envOptionsList = ibmJavaOptionsArg+' '+envOptionFileArg; /* keep this separate because the -Xoptionsfile arg is deleted */
			expectedArgBuffer.add(javaToolOptionsArg);
			expectedArgBuffer.add(ibmJavaOptionsArg);
			expectedArgBuffer.add(envOptionsFileContent);

			String svcArg = "-Dtest.name=testServiceOptions";
			String xServiceArgString = "-Xservice=" + svcArg;
			cmdlineArgBuffer.add(xServiceArgString);
			String cmdlineOption = "-Dtest.cmdlineOption=1";
			cmdlineArgBuffer.add(cmdlineOption);

			String xxArgument="-XXallowvmshutdown:true";
			cmdlineArgBuffer.add(xxArgument);
			expectedArgBuffer.add(xxArgument);

			String xInt=XINT;
			cmdlineArgBuffer.add(xInt);
			expectedArgBuffer.add(xInt);

			cmdlineArgBuffer.add(XCHECK_MEMORY);
			expectedArgBuffer.add(XCHECK_MEMORY);
			cmdlineArgBuffer.add(FORCE_COMMON_CLEANER_SHUTDOWN);
			expectedArgBuffer.add(FORCE_COMMON_CLEANER_SHUTDOWN);

			String xCheckClasspath="-Xcheck:classpath";
			cmdlineArgBuffer.add(xCheckClasspath);
			expectedArgBuffer.add(xCheckClasspath);

			final String optionsFileContent = "-Dtest.options.file=test1";
			String optionFilePath = makeOptionsFile("test1", new String[] {optionsFileContent});
			String optionFileArg=XOPTIONSFILE+optionFilePath;
			cmdlineArgBuffer.add(optionFileArg);
			cmdlineArgBuffer.add(optionsFileContent);
			expectedArgBuffer.add(optionFileArg);
			expectedArgBuffer.add(optionsFileContent);
			expectedArgBuffer.add(svcArg);

			ProcessBuilder pb = makeProcessBuilder(cmdlineArgBuffer.toArray(new String[cmdlineArgBuffer.size()]), CLASSPATH);
			Map<String, String> env = pb.environment();
			env.put(IBM_JAVA_OPTIONS, envOptionsList);
			env.put(JAVA_TOOL_OPTIONS, javaToolOptionsArg);

			actualArguments = runAndGetArgumentList(pb);
			/* Ensure the child process has finished and flushed its stderr */
			try {
				stderrReader.join(10000);
			} catch (InterruptedException e) {
				fail("stderr reader join was interrupted");
			}
			assertTrue("stderr reader join failed", stderrReader.getState() == State.TERMINATED);
			final String[] expectedArgs = expectedArgBuffer.toArray(new String[expectedArgBuffer.size()]);
			HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArgs);
			checkArgumentSequence(expectedArgs, argumentPositions, false);
			String stderrText = stderrReader.getStreamOutput();
			assertTrue("-Xcheck:memory failed: stderr = "+stderrText, stderrText.contains("All allocated blocks were freed."));
		} catch (AssertionError e) {
			dumpDiagnostics(e,actualArguments);
			throw e;
		}
	}

	
	public void testWAS80Cmdline() {
		if (!OS_NAME_PROPERTY.startsWith("Windows") && (Integer.getInteger("com.ibm.vm.bitmode").intValue() == 64)) {
			ArrayList<String> actualArguments = null;
			final String WAS_CLASSPATH=
					"/opt/IBM/WebSphere/AppServer80/profiles/AppSrv01/properties:/opt/IBM/WebSphere/AppServer80/properties:/opt/IBM/WebSphere/AppServer80/lib/startup.jar" +
							":/opt/IBM/WebSphere/AppServer80/lib/bootstrap.jar:/opt/IBM/WebSphere/AppServer80/lib/jsf-nls.jar:/opt/IBM/WebSphere/AppServer80/lib/lmproxy.jar" +
							":/opt/IBM/WebSphere/AppServer80/lib/urlprotocols.jar:/opt/IBM/WebSphere/AppServer80/deploytool/itp/batchboot.jar:/opt/IBM/WebSphere/AppServer80/deploytool/itp/batch2.jar" +
							":/opt/IBM/WebSphere/AppServer80/java/lib/tools.jar"+":"+CLASSPATH;

			String[] initalCmdLineArgs = {"-Declipse.security",
					"-Dosgi.install.area=/opt/IBM/WebSphere/AppServer80",
					"-Dosgi.configuration.area=/opt/IBM/WebSphere/AppServer80/profiles/AppSrv01/servers/server1/configuration",
					"-Djava.awt.headless=true",
					"-Dosgi.framework.extensions=com.ibm.cds,com.ibm.ws.eclipse.adaptors",
					"-Xshareclasses:name=webspherev80_%g,groupAccess,nonFatal",
					"-classpath",
					WAS_CLASSPATH,
					"-Dibm.websphere.internalClassAccessMode=allow",
					"-Xms50m",
					"-Xmx256m",
					XCOMPRESSEDREFS,
					"-Xscmaxaot4M",
					"-Xscmx60M",
					"-Dws.ext.dirs=/opt/IBM/WebSphere/AppServer80/java/lib:/opt/IBM/WebSphere/AppServer80/profiles/AppSrv01/classes:/opt/IBM/WebSphere/AppServer80/classes:/opt/IBM/WebSphere/AppServer80/lib:/opt/IBM/WebSphere/AppServer80/installedChannels:/opt/IBM/WebSphere/AppServer80/lib/ext:/opt/IBM/WebSphere/AppServer80/web/help:/opt/IBM/WebSphere/AppServer80/deploytool/itp/plugins/com.ibm.etools.ejbdeploy/runtime",
					"-Dderby.system.home=/opt/IBM/WebSphere/AppServer80/derby",
					"-Dcom.ibm.itp.location=/opt/IBM/WebSphere/AppServer80/bin",
					"-Djava.util.logging.configureByServer=true",
					"-Duser.install.root=/opt/IBM/WebSphere/AppServer80/profiles/AppSrv01",
					"-Djavax.management.builder.initial=com.ibm.ws.management.PlatformMBeanServerBuilder",
					"-Dpython.cachedir=/opt/IBM/WebSphere/AppServer80/profiles/AppSrv01/temp/cachedir",
					"-Dwas.install.root=/opt/IBM/WebSphere/AppServer80",
					"-Djava.util.logging.manager=com.ibm.ws.bootstrap.WsLogManager",
					"-Dserver.root=/opt/IBM/WebSphere/AppServer80/profiles/AppSrv01",
					"-Dcom.ibm.security.jgss.debug=off",
					"-Dcom.ibm.security.krb5.Krb5Debug=off",
					"-Djava.library.path=/opt/IBM/WebSphere/AppServer80/lib/native/linux/x86_64/:/opt/IBM/WebSphere/AppServer80/java/jre/lib/amd64/default:/opt/IBM/WebSphere/AppServer80/java/jre/lib/amd64:/opt/IBM/WebSphere/AppServer80/lib/native/linux/x86_64/:/opt/IBM/WebSphere/AppServer80/bin:.:/usr/lib:",
					"-Djava.security.auth.login.config=/opt/IBM/WebSphere/AppServer80/profiles/AppSrv01/properties/wsjaas.conf",
			"-Djava.security.policy=/opt/IBM/WebSphere/AppServer80/profiles/AppSrv01/properties/server.policy"};

			String[] initalExpectedArgs = {"-Declipse.security",
					"-Dosgi.install.area=/opt/IBM/WebSphere/AppServer80",
					"-Dosgi.configuration.area=/opt/IBM/WebSphere/AppServer80/profiles/AppSrv01/servers/server1/configuration",
					"-Djava.awt.headless=true",
					"-Dosgi.framework.extensions=com.ibm.cds,com.ibm.ws.eclipse.adaptors",
					"-Xshareclasses:name=webspherev80_%g,groupAccess,nonFatal",
					"-Djava.class.path="+WAS_CLASSPATH,
					"-Dibm.websphere.internalClassAccessMode=allow",
					"-Xms50m",
					"-Xmx256m",
					XCOMPRESSEDREFS,
					"-Xscmaxaot4M",
					"-Xscmx60M",
					"-Dws.ext.dirs=/opt/IBM/WebSphere/AppServer80/java/lib:/opt/IBM/WebSphere/AppServer80/profiles/AppSrv01/classes:/opt/IBM/WebSphere/AppServer80/classes:/opt/IBM/WebSphere/AppServer80/lib:/opt/IBM/WebSphere/AppServer80/installedChannels:/opt/IBM/WebSphere/AppServer80/lib/ext:/opt/IBM/WebSphere/AppServer80/web/help:/opt/IBM/WebSphere/AppServer80/deploytool/itp/plugins/com.ibm.etools.ejbdeploy/runtime",
					"-Dderby.system.home=/opt/IBM/WebSphere/AppServer80/derby",
					"-Dcom.ibm.itp.location=/opt/IBM/WebSphere/AppServer80/bin",
					"-Djava.util.logging.configureByServer=true",
					"-Duser.install.root=/opt/IBM/WebSphere/AppServer80/profiles/AppSrv01",
					"-Djavax.management.builder.initial=com.ibm.ws.management.PlatformMBeanServerBuilder",
					"-Dpython.cachedir=/opt/IBM/WebSphere/AppServer80/profiles/AppSrv01/temp/cachedir",
					"-Dwas.install.root=/opt/IBM/WebSphere/AppServer80",
					"-Djava.util.logging.manager=com.ibm.ws.bootstrap.WsLogManager",
					"-Dserver.root=/opt/IBM/WebSphere/AppServer80/profiles/AppSrv01",
					"-Dcom.ibm.security.jgss.debug=off",
					"-Dcom.ibm.security.krb5.Krb5Debug=off",
					"-Djava.library.path=/opt/IBM/WebSphere/AppServer80/lib/native/linux/x86_64/:/opt/IBM/WebSphere/AppServer80/java/jre/lib/amd64/default:/opt/IBM/WebSphere/AppServer80/java/jre/lib/amd64:/opt/IBM/WebSphere/AppServer80/lib/native/linux/x86_64/:/opt/IBM/WebSphere/AppServer80/bin:.:/usr/lib:",
					"-Djava.security.auth.login.config=/opt/IBM/WebSphere/AppServer80/profiles/AppSrv01/properties/wsjaas.conf",
			"-Djava.security.policy=/opt/IBM/WebSphere/AppServer80/profiles/AppSrv01/properties/server.policy"};


			/*
			 * This test should only run for Java 1.8.0. For Java
			 * 1.9.0 and above, we do not support java.ext.dirs property.
			 */
			String[] cmdLineArgs;
			String[] expectedArgs;
			if (isJava8) {
				cmdLineArgs = new String[initalCmdLineArgs.length + 1];
				System.arraycopy(initalCmdLineArgs, 0, cmdLineArgs, 0, initalCmdLineArgs.length);
				cmdLineArgs[initalCmdLineArgs.length] = "-Djava.ext.dirs=/opt/IBM/WebSphere/AppServer80/tivoli/tam:/opt/IBM/WebSphere/AppServer80/java/jre/lib/ext";

				expectedArgs = new String[initalExpectedArgs.length + 1];
				System.arraycopy(initalExpectedArgs, 0, expectedArgs, 0, initalExpectedArgs.length);
				expectedArgs[initalExpectedArgs.length] = "-Djava.ext.dirs=/opt/IBM/WebSphere/AppServer80/tivoli/tam:/opt/IBM/WebSphere/AppServer80/java/jre/lib/ext";
			} else {
				cmdLineArgs = initalCmdLineArgs.clone();
				expectedArgs = initalExpectedArgs.clone();
			}

			try {
				ProcessBuilder pb = makeProcessBuilder(cmdLineArgs, null); /* classpath set in the arguments already */
				actualArguments = runAndGetArgumentList(pb);
				HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, expectedArgs);
				checkArgumentSequence(expectedArgs, argumentPositions, true);
			} catch (AssertionError e) {
				dumpDiagnostics(e,actualArguments);
				throw e;
			}
		} else {
			logger.debug("Skipping "+testName+" on Windows and non 64-bit systems");
		}

	}

	/* ===================================== Utility methods ===================================================== */
	private String makeOptionsFile(String fileName, String[] argList) {
		String filePath = fileName+OPTIONS_FILE_SUFFIX;
		try {
			PrintStream optFileWriter = new PrintStream(filePath);
			for (String a: argList) {
				optFileWriter.println(a);
			}
			optFileWriter.close();
		} catch (IOException e) {
			logStackTrace(e, logger);
			fail("could not write options file "+fileName+" "+e.getMessage());
		}
		return filePath;
	}

	private ProcessBuilder makeProcessBuilder(String[] vmArgs, String targetClasspath) {
		String appName = APPLICATION_NAME;
		return makeProcessBuilder(appName, vmArgs, targetClasspath);
	}

	private ProcessBuilder makeProcessBuilder(String appName, String[] vmArgs, String targetClasspath) {
		ArrayList<String> vmArgsList = new ArrayList<String>(vmArgs.length+5);
		vmArgsList.add(JAVA_COMMAND);
		if (null != targetClasspath) {
			vmArgsList.add("-classpath");
			vmArgsList.add(CLASSPATH);
		}

		for (String s: vmArgs) {
			vmArgsList.add(s);
		}
		if (null != appName) {
			vmArgsList.add(appName);
		}
		ProcessBuilder pb = new ProcessBuilder(vmArgsList);
		return pb;
	}

	private ArrayList<String> runAndGetArgumentList(ProcessBuilder pb) {
		ArrayList<String> actualArguments = new ArrayList<String>();

		try {
			List<String> cmd = pb.command();

			dumpStrings(cmd);
			Process p = pb.start();
			BufferedReader targetOutReader = new BufferedReader(new InputStreamReader(p.getInputStream()));
			stdoutReader = new ProcessStreamReader(targetOutReader);
			BufferedReader targetErrReader = new BufferedReader(new InputStreamReader(p.getErrorStream()));
			stderrReader = new ProcessStreamReader(targetErrReader);
			stdoutReader.start();
			stderrReader.start();
			int rc = p.waitFor();
			if (0 != rc) {
				ArrayList<String> outputLines = stdoutReader.getOutputLines();
				logger.debug("---------------------------------\nstdout");
				dumpStrings(outputLines);
				ArrayList<String> errLines = stderrReader.getOutputLines();
				logger.debug("---------------------------------\nstderr");
				dumpStrings(errLines);
				fail("Target process failed");

			}
			ArrayList<String> outputLines = stdoutReader.getOutputLines();
			Iterator<String> olIterator = outputLines.iterator();
			String l = "";
			while (olIterator.hasNext() && isNotTag(l)) {
				l = olIterator.next();
				if (isNotTag(l)) {
					logger.debug(l);
				}
			}
			while (olIterator.hasNext() && !l.isEmpty()) {
				actualArguments.add(l);
				l = olIterator.next();
			} 
		} catch (IOException | InterruptedException e) {
			/* 
			 * Catch the checked exceptions so callers of this method don't need to declare them as thrown.
			 */
			fail("unexpected Exception: "+e.getMessage(), e);
		}
		return actualArguments;
	}

	private boolean isNotTag(String l) {
		return !l.startsWith(USERARG_TAG);
	}

	private void dumpStrings(List<String> cmd) {
		for (String s: cmd) {
			logger.debug(s);
		}
	}

	private void dumpStdoutStderr(PrintStream er) {
		er.println("\n------------------------------------------------------\nstdout:");
		er.print(stdoutReader.getStreamOutput());
		er.println("\n------------------------------------------------------\nstderr:");
		er.print(stderrReader.getStreamOutput());
	}

	private int runAndGetExitStatus(ProcessBuilder pb) {
		try {
			List<String> cmd = pb.command();

			dumpStrings(cmd);
			Process p = pb.start();
			int rc = p.waitFor();
			return rc;
		} catch (IOException | InterruptedException e) {
			logStackTrace(e, logger);
			fail("unexpected InterruptedException: "+e.getMessage());
		}
		return -1;
	}

	private HashMap<String, Integer> checkArguments(ArrayList<String> actualArguments,
			String[] optionalArguments) {
		HashMap<String, Integer> argPositions = new HashMap<String, Integer>();
		String[] mArgs;
		if (isJava8) {
			mArgs = mandatoryArgumentsJava8.clone();
		} else {
			mArgs = mandatoryArguments.clone();
		}
		for (String m: mArgs) {
			if (null == m) {
				continue;
			}
			boolean found = false;
			int p = 0;
			for (String a: actualArguments) {
				if (a.startsWith(m)) {
					argPositions.put(m, Integer.valueOf(p));
					found = true;
					break;
				} else {
					++p;
				}
			}
			assertTrue("mandatory argument not found: "+m, found);
		}

		for (String op: optionalArguments) {
			int p = 0;
			for (String a: actualArguments) {
				if (a.startsWith(op)) {
					argPositions.put(op, new Integer(p));
					break;
				} else {
					++p;
				}
			}
		}

		return argPositions;
	}

	/* verify that the arguments appear in the correct order*/
	/**
	 * @param expectedArguments list of arguments in the order in which they are expected
	 * @param argumentPositions listy of actual argument positions
	 * @param contiguous true of the arguments are expected to be contiguous
	 */
	private void checkArgumentSequence(String[] expectedArguments,
			HashMap<String, Integer> argumentPositions, boolean contiguous) {
		int lastPosition=-1;
		String lastArg = null;
		for (int i=0;i<expectedArguments.length;++i) {
			String arg = expectedArguments[i];
			Integer optPos = argumentPositions.get(arg);
			assertNotNull("argument missing: "+arg, optPos);
			final int optPosInt = optPos.intValue();
			if (i > 0) {
				if (contiguous) {
					assertEquals("argument in wrong position: "+arg, lastPosition+1, optPosInt);			
				} else { 	
					assertTrue("argument "+arg+" in wrong order to "+lastArg, lastPosition < optPosInt);			
				}
			}
			lastPosition = optPosInt;
			lastArg = arg;
		}
	}

	private void checkJarArgs(final String pathString) {
		ProcessBuilder pb = makeProcessBuilder(null, new String[] {DASH_D_CMDLINE_ARG, "-jar", pathString}, null);
		Map<String, String> env = pb.environment();
		String javaToolOptionsArg = "-DjavaToolOptionsArg";
		env.put(JAVA_TOOL_OPTIONS, javaToolOptionsArg );
		String ibmJavaOptionsArg = "-DibmJavaOptionsArg";
		env.put(IBM_JAVA_OPTIONS, ibmJavaOptionsArg );
		ArrayList<String> actualArguments = runAndGetArgumentList(pb);
		String fooBar = "-Dfoo=bar";
		String longProp = "-Da.long.system.property=this is a long system property value to demonstrate long JVM arguments in the manifest file";
		HashMap<String, Integer> argumentPositions = checkArguments(actualArguments, 
				new String[] {javaToolOptionsArg, ibmJavaOptionsArg, DASH_D_CMDLINE_ARG, fooBar, longProp});
		assertTrue("missing argument: "+fooBar, argumentPositions.containsKey(fooBar));
		assertTrue("missing argument: "+longProp, argumentPositions.containsKey(longProp));
		/* environment variables should come after implicit arguments */
		int toolsPosn = argumentPositions.get(javaToolOptionsArg).intValue();
		int ibmPropPosn = argumentPositions.get(ibmJavaOptionsArg).intValue();
		int fooBarPropPosn = argumentPositions.get(fooBar).intValue();
		int longPropPosn = argumentPositions.get(longProp).intValue();
		int cmdlineArgPosn = argumentPositions.get(DASH_D_CMDLINE_ARG).intValue();
		assertTrue("Wrong order of properties", ibmPropPosn == toolsPosn + 1);
		assertTrue("Wrong order of properties", fooBarPropPosn < ibmPropPosn);
		assertTrue("Wrong order of properties", fooBarPropPosn < cmdlineArgPosn);
		assertTrue("Wrong order of properties", longPropPosn == fooBarPropPosn + 1);
	}

	private void checkSystemPropertyValues(HashMap<String, String> expectedPropertyValues) throws AssertionError {
		HashMap<String, String> actualPropertyValues = new HashMap<String, String>();
		final String stdoutContents = stdoutReader.getStreamOutput();
		String[] outputLines = stdoutContents.split("\n");
		for (String s: outputLines) {
			String[] keyValue = s.split("=");
			if (keyValue.length == 2) {
				actualPropertyValues.put(keyValue[0], keyValue[1]);
			}
		}
		for (String s: expectedPropertyValues.keySet()) {
			assertEquals("wrong value for property "+s, expectedPropertyValues.get(s), actualPropertyValues.get(s));
		}
	}

	private String getArgumentValue(ArrayList<String> actualArguments,
			String propertyName) {
		for (String s: actualArguments) {
			String[] keyValue = s.split("=");
			if (!keyValue[0].equals(propertyName)) {
				continue;
			}
			if (keyValue.length == 2) {
				return keyValue[1];
			}
		}
		return null;
	}

	private void dumpVmArgs(PrintStream er, ArrayList<String> actualArguments) {
		if (null != actualArguments) {
			er.println("\n\nActual VM arguments:");
			for (String a: actualArguments) {
				er.println(a);
			}
		}
	}
	private void dumpDiagnostics(Throwable e, ArrayList<String> actualArguments) {
		PrintStream diagStream = StringPrintStream.factory();
		if (null != e) {
			diagStream.print(e.getMessage());
			diagStream.println("\n----------------------------------------------------------");
		}
		dumpVmArgs(diagStream, actualArguments);
		dumpStdoutStderr(diagStream);
		logger.debug(diagStream.toString());
	}
	/**
	 * read and buffer an input stream, e.g. stderr from a process, as soon as it is produced.
	 * CMVC 194775 - child process blocked writing tracepoint output.
	 *
	 */
	class ProcessStreamReader extends Thread {
		BufferedReader processStream;
		StringBuffer psBuffer = new StringBuffer();
		ArrayList<String> outputLines = new ArrayList<>();

		public ArrayList<String> getOutputLines() {
			return outputLines;
		}

		public ProcessStreamReader(BufferedReader psReader) {
			this.processStream = psReader;
		}

		@Override
		public void run() {
			try {
				String psLine;
				while ((psLine = processStream.readLine()) != null) {
					psBuffer.append(psLine);
					psBuffer.append('\n');
					outputLines.add(psLine);
				}
			} catch (IOException e) {
				logStackTrace(e, logger);
				fail("unexpected exception");
			}
		}

		String getStreamOutput() {
			return psBuffer.toString();
		}
	}

	private boolean isWindows() {
		String osName = System.getProperty("os.name");
		return ((null != osName) && osName.startsWith("Windows"));
	}

	/* 
	 * Re-use this class as a dummy application.
	 * Note that stdout and stderr go to a parent process, not the console.
	 */
}
