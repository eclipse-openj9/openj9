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
package org.openj9.test.attachAPI;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Properties;

import static org.testng.Assert.fail;

import static org.openj9.test.util.PlatformInfo.getLibrarySuffix;
import org.testng.AssertJUnit;
import org.testng.annotations.AfterMethod;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;

import com.sun.tools.attach.AgentInitializationException;
import com.sun.tools.attach.AgentLoadException;
import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.VirtualMachine;

@Test(groups = { "level.extended" })
@SuppressWarnings({"nls"})
public class TestSunAttachClasses extends AttachApiTest {
	private boolean printLogs = true;
	private static ArrayList<String> vmArgs;

	@Test
	public void testAttachToSelf() {
		logger.debug("starting " + testName);
		String myVmid = TargetManager.getVmId();
		setVmOptions("-Dcom.ibm.tools.attach.logging=yes");
		
		try {
			VirtualMachine selfVm = VirtualMachine.attach(myVmid);
			Properties vmProps = selfVm.getSystemProperties();
			String vendor = vmProps.getProperty(TestUtil.VENDOR_NAME_PROP);
			String myVendor = System.getProperty(TestUtil.VENDOR_NAME_PROP);
			AssertJUnit.assertEquals("my vendor: "+myVmid+" vendor from properties:"+vendor, myVendor, vendor);
			selfVm.detach();
		} catch (AttachNotSupportedException | IOException e) {
			logExceptionInfoAndFail(e);
		}
	}

	@Test
	public void testAttachToOtherProcess() {
		logger.debug("starting " + testName);
		launchAndTestTargets(1);
	}
	
	@Test
	public void testAgentLoading() {
		logger.debug("starting " + testName);
		if (TestUtil.isWindows()) {
			logger.debug("skipping on Windows");
			return;
		}
		String lipPath = System.getProperty("java.library.path");
		String[] libDirs = lipPath.split(File.pathSeparator);
		char fs = File.separatorChar;
		String decoration = "lib";
		String librarySuffix = getLibrarySuffix();
		String errOutput = "";
		String outOutput = "";
		TargetManager target = new TargetManager(TestConstants.TARGET_VM_CLASS, null, vmArgs, null);
		AssertJUnit.assertTrue("target VM failed to launch", target.syncWithTarget());
		VirtualMachine vm = null;
		String targetPid = target.getTargetPid();
		AssertJUnit.assertNotNull("target VM ID is null", targetPid);
		logger.debug("trying to attach "+ targetPid);
		try {
			vm = VirtualMachine.attach(targetPid);
		} catch (AttachNotSupportedException | IOException e) {
			logExceptionInfoAndFail(e);
		}

		if (null == vm) {
			fail("vm is null");
			return;
		}
		for (String libElement: libDirs) {
			try {
				String libPath = libElement+fs+decoration+TestUtil.JVMTITST+librarySuffix;
				logger.debug("trying to load "+ libPath);
				File lib = new File(libPath);
				if (!lib.exists()) {
					continue;
				}
				try {
					vm.loadAgentPath(libPath);
				} catch (AgentLoadException e) {
					logger.warn("AgentLoadException ignored");
					continue;
				} catch (AgentInitializationException e) {
					logger.warn("AgentInitializationException ignored");
					continue;
				} catch (IOException e) {
					logExceptionInfoAndFail(e);
				}
			} catch (IllegalArgumentException e) {
				logger.warn("IllegalArgumentException (ignored)");
			}
		}
		try {
			vm.detach();
		} catch (IOException e) {
			logExceptionInfoAndFail(e);
		}

		target.terminateTarget();
		errOutput = target.getErrOutput();
		outOutput = target.getOutOutput();
		logger.debug(outOutput);
		AssertJUnit.assertTrue("missing Agent_OnAttach string", checkLoadAgentOutput(errOutput, TestUtil.AGENT_ONATTACH_NO_OPTS));
	}

	@AfterMethod
	protected void tearDown() {
		if (printLogs ) {
			TargetManager.dumpLogs(printLogs); /* print the logs if test failed, then delete them */
		}
	}

	@BeforeMethod
	protected void setUp(Method testMethod) {
		testName = testMethod.getName();
		if (!TargetManager.waitForAttachApiInitialization()) {
			TargetManager.dumpLogs(true);
			fail("main process did not initialize attach API");
		}
		TargetManager.dumpLogs(false); /* delete the old logs */
		printLogs = true; /* if test passes, it resets this flag */
	}
}
