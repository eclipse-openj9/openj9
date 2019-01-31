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
package org.openj9.test.vmcheck;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeMethod;
import org.testng.Assert;
import org.testng.AssertJUnit;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.stream.Collectors;


@Test(groups = { "level.extended" })
public class TestLoadingClassesFromJarfile {

	private static final Logger logger = Logger.getLogger(TestLoadingClassesFromJarfile.class);

	private static final int MAX_CLASSES = Integer.getInteger("j9vm.test.vmcheck.maxclasses", 100);

	String[] jarList;
	private boolean verbose = Boolean.getBoolean("j9vm.test.vmcheck.verbose");

	public static String TEST_INFO = "TEST_INFO: ";
	public static String TEST_FAILED = "TEST_FAILED";
	public static String TEST_PASSED = "TEST_PASSED";
	public static String TEST_STATUS = "TEST_STATUS=";

	/**
	 * @note Test for VM errors when loading classes. Ignores errors in
	 *       initialization.
	 */
	@BeforeMethod
	protected void setUp() throws Exception {
		if (null == jarList) {
			String classPath = System.getProperty("sun.boot.class.path");
			//For Java9 and up, replace the "sun.boot.class.path" to "java.class.path".
			if (null == classPath) {
				classPath = System.getProperty("java.class.path");
			}
			String classpathSeparator = System.getProperty("path.separator");
			logger.debug("classpath is : " + classPath);
			jarList = classPath.split(classpathSeparator);
		}
	}

	@Test
	public void testLoadingClasses() {
		for (String jarfilePath : jarList) {
			loadClassesFromJarfile(jarfilePath);
		}
	}

	private void loadClassesFromJarfile(String jarfilePath) {
		if(!jarfilePath.endsWith(".jar")) {
			logger.warn(jarfilePath + " are not end with .jar");
			return;
		}
		File testFile = new File(jarfilePath);
		if (!testFile.exists()) {
			logger.warn(jarfilePath + " does not exist");
			return;
		}
		logger.debug("loading classes from " + jarfilePath);
		Enumeration<JarEntry> jarfileEntries = null;
		try {
			JarFile testJar = new JarFile(jarfilePath);
			jarfileEntries = testJar.entries();
			while (jarfileEntries.hasMoreElements()) {
				ArrayList<String> classNames = new ArrayList<String>();
				while ((classNames.size() < MAX_CLASSES) && jarfileEntries.hasMoreElements()) {
					JarEntry entry = jarfileEntries.nextElement();
					String jarEntryName = entry.getName();
					if (includeFile(jarEntryName)) {
						String testClassName = convertFileNameToClassName(jarEntryName);
						/* Prevent spurious macro expansion by the shell */
						if (testClassName.contains("$")) { //$NON-NLS-1$
							testClassName = "'"+testClassName+"'"; //$NON-NLS-1$ //$NON-NLS-2$
						}
						classNames.add(testClassName);
					}
				}
				if (classNames.size() > 0) {
					logger.debug("loading starting with " + classNames.get(0));
					createAndRunNewClassloadingTester(jarfilePath, classNames);
				}
			}
		} catch (IOException e) {
			logger.error("Unexpected exception: ", e);
			Assert.fail("Unexpected exception: "+e);
		}
	}

	/**
	 * Certain packages cannot be loaded via Class.forName()
	 * as they may require certain machine setups or permissions,
	 * e.g. access to a display.
	 */
	@SuppressWarnings("nls")
	private static final HashSet<String> excludedPackages = 
	new HashSet<String>(Arrays.asList(new String[] {
			"com.apple.eawt",
			"com.apple.laf",
			"com.sun.beans.editors",
			"com.sun.imageio.plugins",
			"com.sun.java.swing",
			"com.sun.naming.internal",
			"com.sun.xml.internal",
			"java.applet",
			"java.awt",
			"java.lang.invoke",
			"java.util.PropertyPermission",
			"javax.imageio.ImageIO",
			"javax.swing",
			"sun.applet",
			"sun.awt",
			"sun.font",
			"sun.java2d",
			"sun.lwawt",
			"sun.print",
			"sun.reflect.misc.Trampoline",
			"sun.security.tools.policytool",
			"sun.swing"
	}).stream().map(s -> s.replace('.', '/')).collect(Collectors.toList()));

	private static boolean includeFile(String jarEntryName) {
		return jarEntryName.endsWith(".class")  //$NON-NLS-1$
				&& !excludedPackages.stream().anyMatch(p -> jarEntryName.startsWith(p));
	}

	private void createAndRunNewClassloadingTester(String jarfilePath, ArrayList<String> classNames)
			throws IOException {
		ArrayList<String> argBuffer = new ArrayList<String>();
		char fs = File.separatorChar;

		String javaExec = System.getProperty("java.home")+fs+"bin"+fs+ "java";
		argBuffer.add(javaExec);

		argBuffer.add("-classpath");
		argBuffer.add(System.getProperty("java.class.path")+System.getProperty("path.separator")+jarfilePath);

		String sideCar = System.getProperty("java.sidecar");
		if ((null != sideCar) && (sideCar.length() > 0)) {
			String sidecarArgs[] = sideCar.split(" +");
			for (String s : sidecarArgs) {
				argBuffer.add(s);
			}
		}

		argBuffer.add("-Xverify:all");

		argBuffer.add("-Xcheck:vm");

		argBuffer.add(ClassloadingTester.class.getName());

		argBuffer.addAll(classNames);

		String cmdLine[] = new String[argBuffer.size()];
		argBuffer.toArray(cmdLine);
		if (verbose) {
			logger.debug(Arrays.toString(cmdLine));
		}
		Process child = Runtime.getRuntime().exec(cmdLine);
		AssertJUnit.assertNotNull("failed to launch child process", child);

		StreamDumper inStreamDumper = new StreamDumper(child.getInputStream());
		StreamDumper errorStreamDumper = new StreamDumper(child.getErrorStream());
		errorStreamDumper.start();
		inStreamDumper.start();

		try {
			ChildWatchdog watchdog = new ChildWatchdog(child);
			AssertJUnit.assertEquals("Child exited with non-zero stats", 0, child.waitFor());
			watchdog.interrupt(); /* kill the watchdog process */
			AssertJUnit.assertTrue("Child reported error", inStreamDumper.isTestPassed() || errorStreamDumper.isTestPassed());
			if (verbose) {
				dumpCommandlineAndOutput(cmdLine, inStreamDumper, errorStreamDumper);
			}
		} catch (IllegalThreadStateException e) {
			Assert.fail("Child process has not terminated");
		} catch (InterruptedException e) {
			e.printStackTrace();
			Assert.fail("unexpected exception");
		}
	}

	private void dumpCommandlineAndOutput(String[] cmdLine,
			StreamDumper inStreamDumper, StreamDumper errorStreamDumper) {
		logger.debug("Command line is:");
		logger.debug(Arrays.toString(cmdLine));
		logger.debug("standard output:");
		logger.debug(inStreamDumper.getProcessOutput());
		logger.debug("standard error:");
		logger.debug(errorStreamDumper.getProcessOutput());
	}

	private static String convertFileNameToClassName(String jarEntryName) {
		return jarEntryName.replace('/', '.').substring(0, jarEntryName.length() - 6);
	}

	/**
	 * Asynchronously print the output from the process. Detect and
	 * report state changes using keywords in the text.
	 *
	 */
	private class StreamDumper extends Thread {
		private BufferedReader inputReader;
		boolean testPassed = false;
		StringBuffer outputBuffer = new StringBuffer();
		StreamDumper(InputStream is) {
			this.inputReader = new BufferedReader(new InputStreamReader(is));
		}
		public void run() {
			String inLine;
			try {
				while (null != (inLine = inputReader.readLine())) {
					outputBuffer.append(inLine);
					outputBuffer.append("\n");
					if (inLine.contains(TEST_STATUS)) {
						if (inLine.contains(TEST_FAILED)) {
							Assert.fail(inLine);
						} else if (inLine.contains(TEST_PASSED)) {
							setTestPassed(true);
						}
					}
				}
			} catch (IOException e) {
				e.printStackTrace();
				Assert.fail("unexpected IOException");
			}
		}
		public synchronized boolean isTestPassed() {
			try {
				this.join(); /* ensure the stream is flushed */
			} catch (InterruptedException e) {
				return false;
			}
			return testPassed;
		}
		public synchronized void setTestPassed(boolean testPassed) {
			this.testPassed = testPassed;
		}

		public String getProcessOutput() {
			return outputBuffer.toString();
		}
	}

	private class ChildWatchdog extends Thread {
		Process child;

		public ChildWatchdog(Process child) {
			super();
			this.child = child;
		}

		@Override
		public void run() {
			try {
				sleep(30000);
				child.destroy();
				Assert.fail("child hung");
			} catch (InterruptedException e) {
				return; //Expected Exception
			}
		}
	}
}
