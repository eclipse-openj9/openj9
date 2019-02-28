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

import static org.testng.AssertJUnit.assertTrue;
import static org.openj9.test.util.FileUtilities.deleteRecursive;
import static org.openj9.test.util.PlatformInfo.isWindows;

import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Properties;

import org.openj9.test.util.PlatformInfo;
import org.openj9.test.util.StringPrintStream;
import org.testng.AssertJUnit;
import org.testng.annotations.AfterMethod;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;

import com.sun.tools.attach.AgentInitializationException;
import com.sun.tools.attach.AgentLoadException;
import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.AttachPermission;
import com.sun.tools.attach.VirtualMachine;
import com.sun.tools.attach.VirtualMachineDescriptor;
import com.sun.tools.attach.spi.AttachProvider;

import static org.testng.Assert.fail;

@Test(groups = { "level.extended" })
@SuppressWarnings({"nls", "boxing"})
public class TestAttachAPI extends AttachApiTest {
	private static final String AGENT_ONATTACH_NO_OPTS = "called Agent_OnAttach with no options";
	private static final String JVMTITST = "jvmtitest";
	private static final String LISTVMS = "j9vm.test.attach.ListVms";
	final String JVMTITSTNAME = "test:gmhc001";
	private final String DUMP_LOGS = "j9vm.test.attach.dumplogs";
	private String myProcessId;
	private String mvVmId;
	final boolean dumpLogs = Boolean.getBoolean(DUMP_LOGS);
	private File commonDir;
	@BeforeMethod
	protected void setUp(Method testMethod) {
		/*
		 * Wait for attachAPI to initialize otherwise these tests may start
		 * before they should.
		 * 
		 * waitForAttachApiInitialization() will return false when a default
		 * timeout is reached.
		 */
		testName = testMethod.getName();
		logger.debug("starting " + testName);
		if (null == commonDir) { /* lazy initialization */
			commonDir = new File(System.getProperty("java.io.tmpdir"),
					".com_ibm_tools_attach");
		}
		if (false == TargetManager.waitForAttachApiInitialization()) {
			TargetManager.dumpLogs(true);
			myProcessId = Long.toString(TargetManager.getProcessId());
			File myAdvert = new File(commonDir, myProcessId);
			if (myAdvert.exists()) {
				logger.error(myAdvert.getAbsolutePath()
						+ " already exists and cannot be deleted");
			}
			fail("main process did not initialize attach API, PID = "
					+ myProcessId);
		}
		logger.debug("\n------------------------------------------------------------\ntime = "+System.currentTimeMillis());
		mvVmId = TargetManager.getVmId();
		if ((null == mvVmId) || (mvVmId.length() == 0)) {
			logger.error("attach API failed to initialize");
			if (!commonDir.exists()) {
				TargetManager.dumpLogs();
				fail("Could not create common directory "
						+ commonDir.getAbsolutePath());
			} else if (!commonDir.canWrite()) {
				TargetManager.dumpLogs();
				fail("Could not write common directory "
						+ commonDir.getAbsolutePath());
			} else {
				TargetManager.dumpLogs();
				fail("Could not initialize attach API");
			}
		}
	}

	@AfterMethod
	protected void tearDown() {
		if (dumpLogs) {
			TargetManager.dumpLogs();
		}
		TargetManager.setLogging(false);
	}

	@Test
	public void test_agntld01() {
		logger.debug("*****************************\nMy VMID = "
				+ mvVmId + "\n*****************************");
		logger.debug("starting " + testName);
		if (isWindows()) {
			logger.debug("skipping  " + testName + " on Windows");
			return;
		}
		TargetManager target = launchTarget(testName);
		VirtualMachine vm = null;
		try {
			 vm = VirtualMachine.attach(testName);
			vm.loadAgentLibrary(JVMTITST);
		} catch (AttachNotSupportedException | IOException | AgentLoadException | AgentInitializationException e) {
			listIpcDir();
			logExceptionInfoAndFail(e);
		} finally {
			if (null != vm) {
				try {
					vm.detach();
				} catch (IOException e) {
					logger.error("Error detaching vm", e);
				}
			}
			String errOutput = target.getErrOutput();
			String outOutput = target.getOutOutput();
			logger.debug(outOutput);
			logger.debug(errOutput);

			target.terminateTarget();
			errOutput = target.getErrOutput();
			outOutput = target.getOutOutput();
			logger.debug(outOutput);
			assertTrue("missing Agent_OnAttach string in " + errOutput,
					checkLoadAgentOutput(errOutput, AGENT_ONATTACH_NO_OPTS));
			logger.debug("ending " + testName);
		}
	}

	@Test
	public void test_agntld02() {
		logger.debug("*****************************\nMy VMID = "
				+ mvVmId + "\n*****************************");
		
		logger.debug("starting " + testName);
		if (isWindows()) {
			logger.debug("skipping  " + testName + " on Windows");
			return;
		}
		TargetManager target = launchTarget(testName);
		VirtualMachine vm = null;
		try {
			vm = VirtualMachine.attach(testName);
			vm.loadAgentLibrary(JVMTITST, "test=aot001");
		} catch (AttachNotSupportedException | IOException e) {
			listIpcDir();
			logExceptionInfoAndFail(e);
		} catch (AgentLoadException | AgentInitializationException e) {
			logger.debug("ignoring AgentLoadException");
		} catch (NullPointerException e) {
			fail("NullPointerException");
		} finally {
			if (null != vm) {
				try {
					vm.detach();
				} catch (IOException e) {
					logger.error("Error detaching vm", e);
				}
			}
			String errOutput = target.getErrOutput();
			String outOutput = target.getOutOutput();
			logger.debug(outOutput);
			logger.debug(errOutput);

			target.terminateTarget();
			errOutput = target.getErrOutput();
			outOutput = target.getOutOutput();
			logger.debug(outOutput);
			assertTrue("missing Agent_OnAttach string in " + errOutput,
					checkLoadAgentOutput(errOutput, "JVMTI TestRunner"));
			logger.debug("ending " + testName);
		}

	}

	@Test
	public void test_agntld03() {
		
		if (isWindows()) {
			logger.debug("skipping  " + testName + " on Windows");
			return;
		}
		logger.debug("starting " + testName);
		String lipPath = System.getProperty("java.library.path");
		String[] libDirs = lipPath.split(File.pathSeparator);
		char fs = File.separatorChar;
		String decoration = isWindows()? "": "lib";
		String librarySuffix = PlatformInfo.getLibrarySuffix();
		int pIndex = 0;
		String outOutput = null;
		TargetManager target = launchTarget();
		VirtualMachine vm = null;
		try {
			logger.debug("trying to attach " + target.getTargetPid());
			vm = VirtualMachine.attach(target.getTargetPid());
		} catch (AttachNotSupportedException | IOException e) {
			logExceptionInfoAndFail(e);
		}
		if (null == vm) {
			fail("vm is null");
			return;
		}
		do {
			try {
				String libPath = libDirs[pIndex] + fs + decoration + JVMTITST
						+ librarySuffix;
				++pIndex;
				logger.debug("trying to load " + libPath);
				File lib = new File(libPath);
				if (!lib.exists()) {
					continue;
				}
				try {
					vm.loadAgentPath(libPath);
				} catch (AgentLoadException e) {
					logger.error("AgentLoadException ignored");
					continue;
				} catch (AgentInitializationException e) {
					logger.error("AgentInitializationException ignored");
					continue;
				} catch (IOException e) {
					logExceptionInfoAndFail(e);
				}
			} catch (IllegalArgumentException e) {
				logger.debug("IllegalArgumentException (ignored)");
			}
		} while (pIndex < libDirs.length);
		try {
			vm.detach();
		} catch (IOException e) {
			logExceptionInfoAndFail(e);
		}
		target.terminateTarget();
		String errOutput = target.getErrOutput();
		outOutput = target.getOutOutput();
		logger.debug(outOutput);
		assertTrue("missing Agent_OnAttach string",
				checkLoadAgentOutput(errOutput, AGENT_ONATTACH_NO_OPTS));
		logger.debug("ending " + testName);
	}

	@Test
	public void test_agntld04() {
		
		logger.debug("starting " + testName);
		listIpcDir();
		TargetManager.setLogging(true);
		TargetManager target = launchTarget();
		VirtualMachine vm = null;
		boolean agentLoadExceptionThrown = false;
		try {
			logger.debug("trying to attach " + target.getTargetPid());
			vm = VirtualMachine.attach(target.getTargetPid());
		} catch (AttachNotSupportedException | IOException e) {
			logExceptionInfoAndFail(e);
		}
		if (null == vm) {
			fail("vm is null");
			return;
		}

		try {
			vm.loadAgentLibrary("boguslibraryname");
		} catch (AgentLoadException e) {
			logger.debug("AgentLoadException ignored");
			agentLoadExceptionThrown = true;
		} catch (AgentInitializationException e) {
			logger.debug("AgentInitializationException ignored");
		} catch (IOException e) {
			logExceptionInfoAndFail(e);
		}

		try {
			vm.detach();
		} catch (IOException e) {
			logExceptionInfoAndFail(e);
		}
		target.terminateTarget();
		String errOutput = target.getErrOutput();
		logger.debug("target stderr=" + errOutput);
		assertTrue("agentLoadExceptionThrown", agentLoadExceptionThrown);
		logger.debug(errOutput);
		logger.debug("ending " + testName);
	}

	/**
	 * @testcase attprops01
	 * @procedure launch a targt VM which sets agent properties, get the agent
	 *            properties
	 * @expect correct value
	 * @variations none
	 */
	@Test
	public void test_attprops01() {
		
		logger.debug("starting " + testName);
		TargetManager target = launchTarget(testName);
		assertTrue(vmIdExists(testName));
		logger.debug("launched " + testName);
		VirtualMachine vm = null;
		try {
			 vm = VirtualMachine.attach(testName);
			Properties props = vm.getAgentProperties();
			/* properties should not be null */
			AssertJUnit.assertNotNull("AgentProperties is null", props);
			logger.debug("AgentProperties");
			PrintStream printBuffer = StringPrintStream.factory();
			props.list(printBuffer);
			logger.debug(printBuffer.toString());

		} catch (AttachNotSupportedException | IOException e) {
			logExceptionInfoAndFail(e);
		} finally {
			try {
				if (null != vm) {
					vm.detach();
				}
			} catch (IOException e) {
				logExceptionInfoAndFail(e);
			}
			target.terminateTarget();
		}
		logger.debug("ending " + testName);
	}

	@Test
	public void test_atthanl01() {
		
		logger.debug("starting " + testName);
		TargetManager target = launchTarget(testName);
		logger.debug("launched target");
		if (!vmIdExists(testName)) {
			target.terminateTarget();
			fail("target VM " + testName + " not found");
		}
		String errOutput = target.getErrOutput();
		String outOutput = target.getOutOutput();
		logger.debug(outOutput);
		logger.debug(errOutput);
		target.terminateTarget();
		errOutput = target.getErrOutput();
		outOutput = target.getOutOutput();
		logger.debug(outOutput);
		logger.debug(errOutput);
	}

	@Test
	public void test_mkcommondir01() throws IOException {
		logger.debug("starting " + testName);
		String tmpdir = System.getProperty("java.io.tmpdir")
				+ File.separatorChar + testName;
		File ipcDir = new File(tmpdir);
		if (ipcDir.exists()) {
			deleteRecursive(ipcDir);
		}
		ipcDir.mkdirs();
		ArrayList<String> vmArgs = new ArrayList<String>();
		vmArgs.add("-Dcom.ibm.tools.attach.directory="
				+ ipcDir.getAbsolutePath());
		TargetManager target = new TargetManager(TestConstants.TARGET_VM_CLASS,
				testName, vmArgs, null);
		logger.debug("launched target");
		TargetManager lister = new TargetManager(LISTVMS, LISTVMS, vmArgs, null);
		logger.debug("launched listvms");
		String vmList = "";
		boolean found = false;
		try {
			while (!found && ((vmList = lister.getTgtOut().readLine()) != null)) {
				logger.debug("listvms output:" + vmList);
				found = vmList.contains(testName);
			}
		} catch (IOException e) {
			logExceptionInfoAndFail(e);
		} finally {
			target.terminateTarget();
			deleteRecursive(ipcDir);
		}
		logger.debug("completed " + testName);
	}

	@Test
	public void test_mkcommondir01a() throws IOException {
		
		logger.debug("starting " + testName);
		String tmpdir = System.getProperty("java.io.tmpdir")
				+ File.separatorChar + testName;
		String ipcSubDir = tmpdir + File.separatorChar + "blah";
		File ipcDir = new File(tmpdir);
		if (ipcDir.exists()) {
			deleteRecursive(ipcDir);
		}
		ipcDir.mkdirs();
		ArrayList<String> vmArgs = new ArrayList<String>();
		vmArgs.add("-Dcom.ibm.tools.attach.directory=" + ipcSubDir);
		TargetManager target = new TargetManager(TestConstants.TARGET_VM_CLASS,
				testName, vmArgs, null);
		target.syncWithTarget();
		logger.debug("launched target");
		TargetManager lister = new TargetManager(LISTVMS, LISTVMS, vmArgs, null);
		String vmList = "";
		boolean found = false;
		try {
			while (!found && ((vmList = lister.getTgtOut().readLine()) != null)) {
				logger.debug("listvms output:" + vmList);
				found = vmList.contains(testName);
			}
		} catch (IOException e) {
			logExceptionInfoAndFail(e);
		} finally {
			target.terminateTarget();
			deleteRecursive(ipcDir);
		}
	}

	@Test
	public void test_mkcommondir02() throws IOException {
		
		logger.debug("starting " + testName);
		if (isWindows()) {
			logger.debug("skipping  "
							+ testName
							+ " on Windows because File.setReadOnly() does not work on directories (CMVC 177983)");
			return;
		}
		String tmpdir = System.getProperty("java.io.tmpdir")
				+ File.separatorChar + testName;
		File ipcDir = new File(tmpdir);
		if (ipcDir.exists()) {
			ipcDir.setWritable(true);
			deleteRecursive(ipcDir);
		}
		ipcDir.mkdirs();
		ipcDir.setReadOnly();
		ArrayList<String> vmArgs = new ArrayList<String>();
		vmArgs.add("-Dcom.ibm.tools.attach.directory="
				+ ipcDir.getAbsolutePath());
		TargetManager target = new TargetManager(TestConstants.TARGET_VM_CLASS,
				testName, vmArgs, null);
		TargetManager lister = new TargetManager(LISTVMS, LISTVMS, vmArgs, null);
		String vmList = "";
		try {
			while ((vmList = lister.getTgtOut().readLine()) != null) {
				AssertJUnit.assertFalse("found unexpected ID "+testName+" in list of VMs", vmList.contains(testName));
				if (vmList.contains(ListVms.LIST_VMS_EXIT)) {
					break;
				}
			}
		} catch (IOException e) {
			logExceptionInfoAndFail(e);
		} finally {
			logger.debug("terminating " + testName);
			target.terminateTarget();
			logger.debug("terminated " + testName);
			ipcDir.setWritable(true);
			deleteRecursive(ipcDir);
		}
	}
	
	@Test
	public void testDeleteTargetDirectory() {
		TargetManager.setLogging(true);
		logger.debug("starting " + testName);
		TargetManager target = launchTarget();
		String targetPid = target.getTargetPid();
		logger.debug("verifying existence of " + targetPid);
		assertTrue(vmIdExists(targetPid));
		File targetDirectory = new File(commonDir, targetPid);
		logger.debug("verifying target directory " + targetDirectory.getPath());
		assertTrue("target directory "+targetDirectory.getPath()+" missing", targetDirectory.exists());
		logger.debug("terminating " + targetPid);
		target.terminateTarget();
		AssertJUnit.assertFalse("target directory "+targetDirectory.getPath()+" not deleted", targetDirectory.exists());
	}

	@Test
	public void testDeleteTargetDirectoryWithSpuriousFile() {
		TargetManager.setLogging(true);
		logger.debug("starting " + testName);
		TargetManager target = launchTarget();
		String targetPid = target.getTargetPid();
		logger.debug("verifying existence of " + targetPid);
		assertTrue(vmIdExists(targetPid));
		File targetDirectory = new File(commonDir, targetPid);
		assertTrue("target directory "+targetDirectory.getPath()+" missing", targetDirectory.exists());
		logger.debug("creating " + testName);
		File spuriousFile = new File(targetDirectory, "spurious_file");
		try {
			spuriousFile.createNewFile();
		} catch (IOException e) {
			fail("file creation failed");
		}
		assertTrue(spuriousFile.getPath()+" missing", spuriousFile.exists());
		logger.debug("terminating " + targetPid);
		assertTrue("target not terminated", target.terminateTarget() >= 0);
		AssertJUnit.assertFalse("target directory "+targetDirectory.getPath()+" not deleted", targetDirectory.exists());
	}

	@Test
	public void test_vmnotify01() {
		logger.debug("starting " + testName);
		TargetManager target = launchTarget(testName);
		assertTrue(vmIdExists(testName));
		VirtualMachine vm = null;
		try {
			vm = VirtualMachine.attach(testName);
			Properties props = vm.getSystemProperties();
			final String IDPROP = "com.ibm.tools.attach.id";
			AssertJUnit.assertEquals(IDPROP, testName, props.getProperty(IDPROP));
			final String CMDPROP = "sun.java.command";
			AssertJUnit.assertEquals(CMDPROP, TestConstants.TARGET_VM_CLASS,
					props.getProperty(CMDPROP));
		} catch (AttachNotSupportedException | IOException e) {
			listIpcDir();
			logExceptionInfoAndFail(e);
		} finally {

			if (null != vm) {
				try {
					vm.detach();
				} catch (IOException e) {
					logExceptionInfoAndFail(e);
				}
			}

		}
		target.terminateTarget();
	}

	@Test
	public void test_vmname02() {
		
		logger.debug("starting " + testName);
		final int NUM_TARGETS = 4;
		TargetManager targets[] = new TargetManager[NUM_TARGETS];
		try {
			for (int i = 0; i < NUM_TARGETS; ++i) {
				targets[i] = new TargetManager(TestConstants.TARGET_VM_CLASS,
						testName, testName, null, null);
				targets[i].syncWithTarget();
				checkTargetPid(targets[i]);
			}
			assertTrue(vmIdExists(testName));
			for (int i = 1; i < NUM_TARGETS; ++i) {
				String tgtId = testName + "-" + (i - 1);
				String tgtId2 = testName + "_" + (i - 1);
				String tgtName = testName;
				logger.debug("check for " + tgtId + " or " + tgtId2);
				if (!vmIdExists(tgtId, tgtName) && !vmIdExists(tgtId2, tgtName)) {
					targets[i].terminateTarget();
					fail("target VM id " + tgtId + " or " + tgtId2 + " name "
							+ tgtName + " not found");
				}
				assertTrue(vmIdExists(tgtId, tgtId2, tgtName));
			}
		} finally {
			for (int i = 0; i < NUM_TARGETS; ++i) {
				targets[i].terminateTarget();
			}
		}
	}

	/**
	 * @testcase vmname03
	 * @procedure start a process without displayName specified
	 * @expect the id is an integer (PID), the displayName is the command
	 * @variations
	 */
	@Test
	public void test_vmname03() {
		
		logger.debug("starting " + testName);
		TargetManager target = launchTarget();
		try {
			List<AttachProvider> providers = com.sun.tools.attach.spi.AttachProvider.providers();
			AttachProvider myProvider = providers.get(0);
			List<VirtualMachineDescriptor> vmdList = myProvider
					.listVirtualMachines();
			if (vmdList.size() < 1) {
				fail("test_vmname03 ERROR: no target VMs found");
			}
			boolean found = false;
			Iterator<VirtualMachineDescriptor> vmi = vmdList.iterator();
			VirtualMachineDescriptor vm = null;
			while (!found && vmi.hasNext()) {
				vm = vmi.next();
				if (TestConstants.TARGET_VM_CLASS.equals(vm.displayName())) {
					found = true;
				}
			}
			if (null == vm) {
				fail("vm is null");
				return;
			}
			if (found) {

				try {
					Integer.parseInt(vm.id());
				} catch (NumberFormatException e) {
					fail("vm ID = " + vm.id());
				}
			} else {
				fail("displayName " + TestConstants.TARGET_VM_CLASS
						+ " not found");
			}
		} finally {
			target.terminateTarget();
		}
	}

	@Test
	public void test_vmname04() {
		
		logger.debug("starting " + testName);
		TargetManager target = launchTarget();
		VirtualMachine vm = null;
		VirtualMachineDescriptor vmd = null;
		try {
			String tgtId = target.getTargetPid();
			List<AttachProvider> providers = AttachProvider.providers();
			AttachProvider myProvider = providers.get(0);
			vmd = findVmdForPid(myProvider, tgtId);
			vm = VirtualMachine.attach(vmd);
			String vmId = vm.id();
			String vmdId = vmd.id();
			AssertJUnit.assertEquals(
					"VirtualMachineDescriptor.id() == VirtualMachine.id()",
					vmId, vmdId);
		} catch (AttachNotSupportedException | IOException e) {
			logger.error("Trying to attach " + vmd.id() + " "
					+ vmd.displayName());
			logExceptionInfoAndFail(e);
		} finally {
			if (null != vm) {
				try {
					vm.detach();
				} catch (IOException e) {
					logExceptionInfoAndFail(e);
				}
			}
			target.terminateTarget();
		}
	}

	@Test
	public void test_vmname05() {
		
		final int NUM_TARGETS = 3;

		logger.debug("starting " + testName);
		listIpcDir();
		TargetManager.setLogging(true);
		TargetManager mainTarget = launchTarget();
		TargetManager otherTarget = launchTarget();
		VirtualMachine mainVms[] = new VirtualMachine[NUM_TARGETS], otherVms[] = new VirtualMachine[NUM_TARGETS];
		VirtualMachineDescriptor vmd = null;
		VirtualMachineDescriptor otherVmd = null;
		try {
			List<AttachProvider> providers = com.sun.tools.attach.spi.AttachProvider.providers();
			AttachProvider myProvider = providers.get(0);
			List<VirtualMachineDescriptor> vmdList = myProvider
					.listVirtualMachines();
			listIpcDir();
			if (vmdList.size() < 2) {
				fail("ERROR " + testName + ": no target VMs found");
			}
			for (int i = 0; i < NUM_TARGETS; ++i) {
				String pid = mainTarget.getTargetPid();
				logger.debug("connecting to  " + pid);
				mainVms[i] = VirtualMachine.attach(pid);
			}
			for (int i = 0; i < NUM_TARGETS; ++i) {
				String pid = otherTarget.getTargetPid();
				logger.debug("connecting to  " + pid);
				otherVms[i] = VirtualMachine.attach(pid);
			}
			for (int i = 0; i < NUM_TARGETS; ++i) {
				if (!mainVms[0].equals(mainVms[i])) {
					logger.error("mainVms[0]=" + mainVms[0].id()
							+ " mainVms[" + i + "]" + mainVms[i].id());
					fail("VirtualMachine equals for same VM");
				}
				if (!otherVms[0].equals(otherVms[i])) {
					logger.error("otherVms[0]=" + otherVms[0].id()
							+ "otherVms[i]" + otherVms[i].id());
					fail("VirtualMachine equals for same VM");
				}
			}
			for (int i = 0; i < NUM_TARGETS; ++i) {
				AssertJUnit.assertFalse("VirtualMachine unequal for other VM",
						mainVms[0].equals(otherVms[i]));
			}
		} catch (AttachNotSupportedException e) {

			if ((null != vmd) && (null != otherVmd)) {
				logger.error("Trying to attach " + vmd.id() + " "
						+ vmd.displayName() + " or " + otherVmd.id() + " "
						+ otherVmd.displayName());
			}
			listIpcDir();
			logExceptionInfoAndFail(e);
		} catch (IOException e) {	
			logExceptionInfoAndFail(e);
		} finally {
			try {
				for (int i = 0; i < NUM_TARGETS; ++i) {
					if (null != mainVms[i]) {
						mainVms[i].detach();
					}
				}
				for (int i = 0; i < NUM_TARGETS; ++i) {
					if (null != otherVms[i]) {
						otherVms[i].detach();
					}
				}
				String errOutput = mainTarget.getErrOutput();
				String outOutput = mainTarget.getOutOutput();
				logger.debug(outOutput);
				logger.debug(errOutput);
				errOutput = otherTarget.getErrOutput();
				outOutput = otherTarget.getOutOutput();
				logger.debug(outOutput);
				logger.debug(errOutput);

				mainTarget.terminateTarget();
				otherTarget.terminateTarget();
				errOutput = mainTarget.getErrOutput();
				outOutput = mainTarget.getOutOutput();
				logger.debug(outOutput);
				logger.debug(errOutput);
				errOutput = otherTarget.getErrOutput();
				outOutput = otherTarget.getOutOutput();
				logger.debug(outOutput);
			} catch (IOException e) {
				logExceptionInfoAndFail(e);
			}
		}
	}

	@Test
	public void test_vmname06() {
		

		logger.debug("starting " + testName);
		TargetManager mainTarget = launchTarget();
		try {
			List<AttachProvider> providers = com.sun.tools.attach.spi.AttachProvider.providers();
			AttachProvider myProvider = providers.get(0);
			List<VirtualMachineDescriptor> vmdList = myProvider
					.listVirtualMachines();
			List<VirtualMachineDescriptor> vmdList2 = myProvider
					.listVirtualMachines();
			if (vmdList.size() < 2) {
				fail("ERROR " + testName + ": no target VMs found");
			}
			VirtualMachineDescriptor mainDesc = vmdList.get(0);
			boolean found = false;
			Iterator<VirtualMachineDescriptor> vmdi = vmdList2.iterator();
			while (vmdi.hasNext()) {
				VirtualMachineDescriptor vmd = vmdi.next();
				if (vmd.equals(mainDesc)) {
					found = true;
					break;
				}
			}
			assertTrue("VirtualMachineDescriptor.equals", found);

		} finally {
			mainTarget.terminateTarget();
		}
	}

	@Test
	public void test_vmname07() {

		logger.debug("starting " + testName);
		TargetManager target = null;
		VirtualMachine vm = null;
		try {
			target = new TargetManager(TestConstants.TARGET_VM_CLASS, testName,
					testName, null, null);
			target.syncWithTarget();
			assertTrue(vmIdExists(testName));
			List<AttachProvider> providers = com.sun.tools.attach.spi.AttachProvider.providers();
			AttachProvider myProvider = providers.get(0);
			vm = myProvider.attachVirtualMachine(testName);
			Properties props = vm.getSystemProperties();
			String vendor = props.getProperty(TestConstants.VENDOR_NAME_PROP);
			String myVendor = System
					.getProperty(TestConstants.VENDOR_NAME_PROP);
			AssertJUnit.assertEquals(myVendor, vendor);
		} catch (AttachNotSupportedException | IOException e) {
			logExceptionInfoAndFail(e);
		} finally {
			if (null != target) {
				target.terminateTarget();
			}
		}
	}

	@Test
	public void test_vmname08() {
		
		logger.debug("starting " + testName);
		TargetManager target = null;
		VirtualMachine vm = null;
		try {
			String vmId = testName;
			target = new TargetManager(TestConstants.TARGET_VM_CLASS, vmId,
					vmId, null, null);
			target.syncWithTarget();
			assertTrue(vmIdExists(testName));
			List<AttachProvider> providers = com.sun.tools.attach.spi.AttachProvider.providers();
			AttachProvider myProvider = providers.get(0);
			logger.debug("attaching to " + vmId);
			vm = myProvider.attachVirtualMachine(vmId);
			Properties props = vm.getSystemProperties();
			String vendor = props.getProperty(TestConstants.VENDOR_NAME_PROP);
			String myVendor = System
					.getProperty(TestConstants.VENDOR_NAME_PROP);
			AssertJUnit.assertEquals(myVendor, vendor);
		} catch (AttachNotSupportedException | IOException e) {
			listIpcDir();
			logExceptionInfoAndFail(e);
		} finally {
			if (null != target) {
				target.terminateTarget();
			}
		}
	}

	@Test
	public void test_vmlist01() {
		
		logger.debug("starting " + testName);
		final int NUM_TARGETS = 4;
		TargetManager targets[] = new TargetManager[NUM_TARGETS];
		String vmIds[] = new String[NUM_TARGETS];
		String vmNames[] = new String[NUM_TARGETS];
		boolean testComplete = false;
		try {
			for (int i = 0; i < NUM_TARGETS; ++i) {
				vmNames[i] = "my_" + testName + i;
				vmIds[i] = testName + i;
				targets[i] = new TargetManager(TestConstants.TARGET_VM_CLASS,
						vmIds[i], vmNames[i], null, null);
				logger.debug("vmIds[i] = " + vmIds[i] + ", vmNames[i]="
						+ vmNames[i]);
				targets[i].syncWithTarget();
			}
			List<VirtualMachineDescriptor> vmdList = VirtualMachine.list();
			for (int i = 0; i < NUM_TARGETS; ++i) {
				boolean found = false;
				logger.debug("looking for id = " + vmIds[i] + ", name="
						+ vmNames[i]);
				Iterator<VirtualMachineDescriptor> di = vmdList.iterator();
				while (di.hasNext()) {
					VirtualMachineDescriptor d = di.next();
					logger.debug("checking id = " + d.id() + ", name="
							+ d.displayName());

					if (d.id().equals(vmIds[i])
							&& d.displayName().equals(vmNames[i])) {
						found = true;
						break;
					}
				}
				assertTrue("VirtualMachine.list contains " + vmIds[i] + " "
						+ vmNames[i], found);
				testComplete = true;
			}
		} catch (Exception e) {
			logger.warn(testName + " exception");
			logStackTrace(e);
		} finally {
			assertTrue(testName + " incomplete", testComplete);
			for (int i = 0; i < NUM_TARGETS; ++i) {
				targets[i].terminateTarget();
			}
		}
	}

	@Test
	public void test_attach01_02() {
		final int NUM_TARGETS = 4;
		TargetManager targets[] = new TargetManager[NUM_TARGETS];
		try {
			for (int i = 0; i < NUM_TARGETS; ++i) {
				targets[i] = launchTarget();
			}
			assertTrue(checkHashes(true, targets));
		} finally {
			logger.debug("Teardown " + testName);
			for (int i = 0; i < NUM_TARGETS; ++i) {
				targets[i].terminateTarget();
			}
		}
	}

	@Test
	public void test_attach03() {
		final int NUM_TARGETS = 4;
		TargetManager targets[] = new TargetManager[NUM_TARGETS];
		try {
			for (int i = 0; i < NUM_TARGETS; ++i) {
				targets[i] = launchTarget();
			}
			listIpcDir();
			assertTrue(checkHashes(false, targets));
		} finally {

			logger.debug("Teardown " + testName);
			for (int i = 0; i < NUM_TARGETS; ++i) {
				targets[i].terminateTarget();
			}
		}
	}

	@Test
	public void test_attach04() {
		logger.debug("starting " + testName);
		TargetManager target = launchTarget();
		VirtualMachine vm = null;
		VirtualMachineDescriptor vmd = null;
		try {
			List<AttachProvider> providers = com.sun.tools.attach.spi.AttachProvider	.providers();
			AttachProvider myProvider = providers.get(0);
			vmd = findVmdForPid(myProvider, target.getTargetPid());
			try {
				vm = VirtualMachine.attach(vmd);
			} catch (IOException e) {
				logExceptionInfoAndFail(e);
			}
			if (null == vm) {
				fail("VirtualMachine.attach returned null");
			}
			String vmToString = vm.toString();
			String vmdToString = vmd.toString();
			logger.debug(testName+": vm.toString()=" + vmToString);
			logger.debug(testName+": vmd.toString()=" + vmdToString);
			String vmdId = vmd.id();
			assertTrue(vmdToString.contains(vmdId));
		} catch (AttachNotSupportedException e) {
			listIpcDir();
			logExceptionInfoAndFail(e);
		} finally {
			if (null != vm) {
				try {
					vm.detach();
				} catch (IOException e) {
					logExceptionInfoAndFail(e);
				}
			}
			target.terminateTarget();
		}
	}

	@Test
	public void test_attach05() {
		TargetManager target = launchTarget();
		VirtualMachine vm = null;
		try {
			List<AttachProvider> providers = com.sun.tools.attach.spi.AttachProvider.providers();
			AttachProvider myProvider = providers.get(0);
			VirtualMachineDescriptor vmd;
			if (null == (vmd = findVmdForPid(myProvider, target.getTargetPid()))) {
				fail("ERROR " + testName + ": no target VMs found");
			}
			vm = VirtualMachine.attach(vmd);
			if (null == vm) {
				fail("vm is null");
				return;
			}
			if (null == vmd) {
				fail("vmd is null");
				return;
			}
			AttachProvider vmdProvider = vmd.provider();
			AssertJUnit.assertEquals(
					"VirtualMachineDescriptor.provider() == main provider",
					vmdProvider, myProvider);
			AttachProvider vmProvider = vm.provider();
			AssertJUnit.assertEquals("VirtualMachine.provider() == main provider",
					vmProvider, myProvider);
		} catch (AttachNotSupportedException | IOException e) {
			listIpcDir();
			logExceptionInfoAndFail(e);
		} finally {
			if (null != vm) {
				try {
					vm.detach();
				} catch (IOException e) {
					logExceptionInfoAndFail(e);
					}
			}
			target.terminateTarget();
		}
	}

	@Test
	public void test_detach02_03() {
		final String vmid = testName;
		boolean exceptionThrown = false;
		TargetManager target = launchTarget(testName);
		logger.debug("attaching to " + testName);
		try {
			VirtualMachine vm = VirtualMachine.attach(vmid);
			Properties props = vm.getSystemProperties();
			String vendor = props.getProperty(TestConstants.VENDOR_NAME_PROP);
			String myVendor = System
					.getProperty(TestConstants.VENDOR_NAME_PROP);
			AssertJUnit.assertEquals(myVendor, vendor);

		} catch (AttachNotSupportedException | IOException e) {
			listIpcDir();

			logExceptionInfoAndFail(e);
		}
		target.terminateTarget();
		try {
			VirtualMachine vm = VirtualMachine.attach(vmid);
			Properties props = vm.getSystemProperties();
			String vendor = props.getProperty(TestConstants.VENDOR_NAME_PROP);
			String myVendor = System
					.getProperty(TestConstants.VENDOR_NAME_PROP);
			AssertJUnit.assertEquals(myVendor, vendor);
		} catch (AttachNotSupportedException e) {
			logger.debug("expected AttachNotSupportedException thrown");
			exceptionThrown = true;
		} catch (IOException e) {
			logExceptionInfoAndFail(e);
		}
		assertTrue(exceptionThrown);
	}

	private static final String ATTACH_VIRTUAL_MACHINE = "attachVirtualMachine";

	@Test
	public void test_attachperm01_02_03() {
		String legalNames[] = { ATTACH_VIRTUAL_MACHINE, "createAttachProvider" };
		boolean gotException = false;

		for (String n : legalNames) {
			try {
				AttachPermission ap = new AttachPermission(
						n);
				String m = ap.getName();

				AssertJUnit.assertEquals("compare result of getName with original name", n,
						m);
			} catch (Exception e) {
				gotException = true;
			}
		}

		AssertJUnit.assertFalse("test legal names", gotException);
		gotException = false;
		try {
			AttachPermission ap = new AttachPermission(
					"foobar");
			AssertJUnit.assertEquals(ap.getClass().getName(),
					"com.ibm.tools.attach.AttachPermission");
		} catch (IllegalArgumentException e) {
			gotException = true;
		}
		assertTrue("test illegal name", gotException);

	}

	/**
	 * @testcase attachperm04
	 * @procedure call checkGuard
	 * @expect no exception
	 * @variations
	 */

	/**
	 * @testcase attachperm05
	 * @procedure calltoString
	 * @expect correct string
	 * @variations
	 */

	@Test
	public void test_attachperm04_05() {
		AttachPermission ap = new AttachPermission(ATTACH_VIRTUAL_MACHINE);
		try {
			ap.checkGuard(this);
		} catch (SecurityException unwantedException) {
			fail("attachperm04: checkGuard: unexpected exception: "
					+ unwantedException.getMessage());
		}
		String testString = ap.toString();
		assertTrue(
				"attachperm05: error in AttachPermission.toString() output: actual output = "
						+ testString,
				testString.contains("com.sun.tools.attach.AttachPermission")
						&& testString.contains(ATTACH_VIRTUAL_MACHINE));
	}

	/*
	 * ***********************************************************************************************************
	 * Utility methods
	 * **********************************************************
	 * *************************************************
	 */

	private void listRecursive(String prefix, File root) {
		String myPath = prefix + "/" + root.getName();
		logger.debug(myPath);
		if (root.isDirectory()) {
			File[] children = root.listFiles();
			if (null != children) {
				for (File c : children) {
					listRecursive(myPath, c);
				}
			}
		}
	}

	private void listIpcDir() {
		String tmpdir = System.getProperty("java.io.tmpdir")
				+ File.separatorChar + TestConstants.DEFAULT_IPC_DIR;
		File ipcDir = new File(tmpdir);
		if (ipcDir.exists()) {
			listRecursive(System.getProperty("java.io.tmpdir"), ipcDir);
		}
	}

	private VirtualMachineDescriptor findVmdForPid(AttachProvider provider,
			String pid) {
		List<VirtualMachineDescriptor> vmdList = provider.listVirtualMachines();
		if (vmdList.size() < 1) {
			return null;
		}
		Iterator<VirtualMachineDescriptor> vmdi = vmdList.iterator();
		VirtualMachineDescriptor vmd = null;
		while (vmdi.hasNext()) {
			VirtualMachineDescriptor tempVmd = vmdi.next();
			if (tempVmd.id().equals(pid)) {
				vmd = tempVmd;
				break;
			}
		}
		return vmd;
	}

	private boolean vmIdExists(String id, String displayName) {
		List<AttachProvider> providers = AttachProvider.providers();
		AttachProvider ap = providers.get(0);
		AssertJUnit.assertNotNull("no default attach provider", ap);
		List<VirtualMachineDescriptor> vmds = ap.listVirtualMachines();
		Iterator<VirtualMachineDescriptor> vmi = vmds.iterator();
		while (vmi.hasNext()) {
			VirtualMachineDescriptor vm = vmi.next();
			if (id.equals(vm.id())
					&& ((null == displayName) || displayName.equals(vm
							.displayName()))) {
				return true;
			}
		}
		return false;
	}

	private boolean vmIdExists(String id, String id2, String displayName) {
		List<AttachProvider> providers = AttachProvider.providers();
		AttachProvider ap = providers.get(0);
		AssertJUnit.assertNotNull("no default attach provider", ap);
		List<VirtualMachineDescriptor> vmds = ap.listVirtualMachines();
		Iterator<VirtualMachineDescriptor> vmi = vmds.iterator();
		while (vmi.hasNext()) {
			VirtualMachineDescriptor vm = vmi.next();
			if ((id.equals(vm.id()) || id2.equals(vm.id()))
					&& ((null == displayName) || displayName.equals(vm
							.displayName()))) {
				return true;
			}
		}
		return false;
	}

	private boolean checkHashes(boolean detach, TargetManager[] tgts) {
		final int iterations = 4;
		List<VirtualMachineDescriptor> vmdList;
		int vmHashes[], vmdHashes[];
		VirtualMachine[] vms;
		boolean failed = false;

		List<AttachProvider> providers = com.sun.tools.attach.spi.AttachProvider.providers();
		AttachProvider myProvider = providers.get(0);
		vmdList = myProvider.listVirtualMachines();
		if (vmdList.size() < tgts.length) {
			fail("checkHashes ERROR: no target VMs found");
		}
		vmHashes = new int[vmdList.size()];
		vmdHashes = new int[vmdList.size()];
		vms = new VirtualMachine[vmdList.size()];
		HashMap<String, Integer> id2hash = new HashMap<String, Integer>(
				vmdList.size());
		Iterator<VirtualMachineDescriptor> vmdI = vmdList.iterator();
		while (vmdI.hasNext()) {
			VirtualMachineDescriptor vmd = vmdI.next();
			id2hash.put(vmd.id(), vmd.hashCode());
		}
		ArrayList<VirtualMachine> attachments = new ArrayList<VirtualMachine>();
		for (int iter = 0; iter < iterations; ++iter) {
			int vmNum = 0;
			for (TargetManager tm : tgts) {
				String vmId = tm.getTargetPid();
				logger.debug("checkHashes attaching to " + vmId);
				try {
					VirtualMachine vm = VirtualMachine.attach(vmId);
					int vhc = vm.hashCode();
					int dhc = id2hash.get(vmId);
					if (0 == iter) {
						vms[vmNum] = vm;
						vmHashes[vmNum] = vhc;
						vmdHashes[vmNum] = dhc;
						logger.debug("hashcodes for " + vmId + ": vm = "
								+ vhc + " descriptor = " + dhc);
					} else {
						if (!vm.equals(vms[vmNum])) {
							logger.error("checkHashes ERROR: equals failed "
											+ vmId);
							failed = true;
						}
						if (dhc != vmdHashes[vmNum]) {
							logger.error("checkHashes ERROR: " + vmId
									+ " old descriptor hash "
									+ vmdHashes[vmNum] + " new hash " + dhc);
							failed = true;
						}
						if (vhc != vmHashes[vmNum]) {
							logger.error("checkHashes ERROR: " + vmId
									+ " old vm hash " + vmHashes[vmNum]
									+ " new hash " + vhc);
							failed = true;
						}
					}
					if (detach) {
						vm.detach();
					} else {
						attachments.add(vm);
					}
				} catch (AttachNotSupportedException | IOException e) {
					logger.error(e.getMessage() + "\nfailure at " + vmId);
					listIpcDir();
					logExceptionInfoAndFail(e);
				}
				++vmNum;

			}
		}
		for (VirtualMachine vm : attachments) {
			try {
				logger.debug("teardown detach " + vm.hashCode());
				vm.detach();
			} catch (IOException e) {
				logStackTrace(e);
				logger.error("IOException when detaching (permissible)");
			}
		}
		logger.debug("checkHashes verify that hashes are unique");
		for (int i = 0; i < vmHashes.length; ++i) {
			if (vms[i] == null) {
				continue;
			}
			for (int j = i + 1; j < i; ++j) {
				if (vms[i].equals(vms[j])) {
					logger.error("checkHashes ERROR: vm "
							+ vmdList.get(i).id() + " equals "
							+ vmdList.get(j).id());
					failed = true;
				}
				if (vmHashes[i] == vmHashes[j]) {
					logger.error("checkHashes ERROR: vm hashcode "
							+ vmHashes[i] + " for " + vmdList.get(i).id()
							+ " same as " + vmdList.get(j).id());
					failed = true;
				}
				if (vmdHashes[i] == vmdHashes[j]) {
					logger.error("checkHashes ERROR: descriptor hashcode "
									+ vmdHashes[i] + " for "
									+ vmdList.get(i).id() + " same as "
									+ vmdList.get(j).id());
					failed = true;
				}
			}
		}
		return !failed;
	}

	private boolean vmIdExists(String id) {
		return vmIdExists(id, null);
	}
}
