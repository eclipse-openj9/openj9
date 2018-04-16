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
package org.openj9.test.attachAPI;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;

import org.testng.Assert;
import org.testng.AssertJUnit;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;

import com.ibm.tools.attach.target.AttachHandler;

/**
 * This test must be invoked via testng.  Running the main method will return only the status of the attach API.
 *
 */
@SuppressWarnings({"nls","boxing"})
public class TestAttachAPIEnabling extends AttachApiTest {
	private static final Integer ATTACH_API_ENABLED_CODE = 12;
	private static final Integer ATTACH_API_DISABLED_CODE = 13;
	private static boolean verbose = false;
	static final String ATTACH_ENABLE_PROPERTY = "-Dcom.ibm.tools.attach.enable=";

	private ArrayList<String> defaultVmArgs;
	@BeforeMethod(alwaysRun=true)
	protected void setUp() throws Exception {
		defaultVmArgs = new ArrayList<String>();
		logger.debug("\n-------------------------------------------------------------------------------\n");
	}

	@Test(groups = { "level.extended" })
	public void testImplicitEnableSetting() {
		Integer expectedExitCode;
		String osName = System.getProperty("os.name");
		if (osName.equals("z/OS")) {
			expectedExitCode = ATTACH_API_DISABLED_CODE;
			logger.debug("verify attach API disabled by default on z/OS");
		} else {
			expectedExitCode = ATTACH_API_ENABLED_CODE;
			logger.debug("verify attach API enabled by default on non-z/OS");
		}
		runAndCheckChildProcess(expectedExitCode);
	}

	@Test(groups = { "level.extended" })
	public void testExplicitEnable() {
		Integer expectedExitCode = ATTACH_API_ENABLED_CODE;
		defaultVmArgs.add(ATTACH_ENABLE_PROPERTY+"yes");
		logger.debug("verify attach API enabled by system property");
		runAndCheckChildProcess(expectedExitCode);
	}

	@Test(groups = { "level.extended" })
	public void testExplicitDisable() {
		Integer expectedExitCode = ATTACH_API_DISABLED_CODE;
		defaultVmArgs.add(ATTACH_ENABLE_PROPERTY+"no");
		logger.debug("verify attach API disabled by system property");
		runAndCheckChildProcess(expectedExitCode);
	}

	private void runAndCheckChildProcess(Integer expectedExitCode) {
		Process tgt = launchTarget(defaultVmArgs);
		try {
			Integer rc = tgt.waitFor();
			AssertJUnit.assertEquals("attach API not enabled by system property", expectedExitCode, rc);
		} catch (InterruptedException e) {
			logExceptionInfoAndFail(e);
		}
	}

	/**
	 * Exits with status 1 if attach API is enabled, 0 otherwise.
	 * @param args not used.
	 */
	public static void main(String[] args) {

		boolean isAttachApiEnabled = AttachHandler.waitForAttachApiInitialization();
		System.exit(isAttachApiEnabled? ATTACH_API_ENABLED_CODE:ATTACH_API_DISABLED_CODE);
	}

	/**
	 * @param vmArgs JVM arguments
	 * @return Process object for an instance of this class invoked via main()
	 */
	private Process launchTarget(ArrayList<String> vmArgs) {
		ArrayList<String> argBuffer = new ArrayList<String>();
		String[] args = {};
		Runtime me = Runtime.getRuntime();
		char fs = File.separatorChar;

		String javaExec = System.getProperty("java.home") + fs + "bin" + fs + "java";
		argBuffer.add(javaExec);

		argBuffer.add("-classpath");
		String myClasspath = System.getProperty("java.class.path");
		argBuffer.add(myClasspath);

		String sideCar = System.getProperty("java.sidecar");
		if ((null != sideCar) && (sideCar.length() > 0)) {
			String sidecarArgs[] = sideCar.split(" +");
			for (String s : sidecarArgs) {
				argBuffer.add(s);
			}
		}

		argBuffer.addAll(vmArgs);

		argBuffer.add(this.getClass().getName());
		args = new String[argBuffer.size()];
		argBuffer.toArray(args);
		if (verbose ) {
			System.out.print("\n");
			for (int i = 0; i < args.length; ++i) {
				System.out.print(args[i] + " ");
			}
			System.out.print("\n");
		}
		try {
			Process target = me.exec(args);
			return target;
		} catch (IOException e) {
			logStackTrace(e);
			Assert.fail("target failed to launch");
			return null;
		}
	}

}
