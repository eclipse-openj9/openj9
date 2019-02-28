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

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.Properties;
import java.util.Random;

import org.testng.Assert;
import org.testng.AssertJUnit;
import org.testng.annotations.AfterMethod;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;

import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.VirtualMachine;
import com.sun.tools.attach.spi.AttachProvider;

import static org.openj9.test.util.FileUtilities.deleteRecursive;

@Test(groups = { "level.extended" })
@SuppressWarnings({"nls"})
public class TestAttachErrorHandling extends AttachApiTest implements TestConstants {
	private static final String CLONE_PREFIX = "clone";
	final File commonDirectory = new File(System.getProperty("java.io.tmpdir"), DEFAULT_IPC_DIR);
	File tempStorage = new File(commonDirectory, "_temp");
	String vmId = "foo";

	@BeforeMethod
	protected void setUp(Method testMethod) throws Exception {
		if (!TargetManager.waitForAttachApiInitialization()) {
			TargetManager.dumpLogs(true);
			Assert.fail("main process did not initialize attach API");
		}
		tempStorage = new File(commonDirectory, "_temp");
		testName = testMethod.getName();
		logger.debug("starting " + testName);
	}

	@AfterMethod
	protected void tearDown() throws Exception {
		tempStorage = new File(commonDirectory, "_temp");
		if (tempStorage.exists()) {
			tempStorage.renameTo(new File("0"));
		}
	}
	
	@Test
	public void testTracepoint() {
		ArrayList<String> vmArgs = new ArrayList<String>();
		vmArgs.add("-Dcom.ibm.tools.attach.logging=yes");
		vmArgs.add("-Xtrace:print={j9jcl.345}");
		TargetManager tgt = new TargetManager(TARGET_VM_CLASS, null, vmArgs, null);
		ProcessStreamReader stderrReader = new ProcessStreamReader(tgt.getTargetErrReader());
		stderrReader.start();
		tgt.syncWithTarget();
		tgt.terminateTarget();
		String errOutput = stderrReader.getStreamOutput();
		logger.debug("target stderr=" + errOutput);
		AssertJUnit.assertTrue("Tracepoint output missing: ", errOutput.contains(" Attach API: status=1 (STATUS_LOGGING), message ="));
	}
	
	@Test
	public void test_startupShutdownRobustness() throws IOException {
		File ipcDir=new File(System.getProperty("java.io.tmpdir"), testName);
		ipcDir.mkdir();
		File logDir=new File(System.getProperty("java.io.tmpdir"), testName+"_logs");
		logDir.mkdir();
		
		ArrayList<TargetManager> targets = new ArrayList<TargetManager>();
		ArrayList<String> vmArgs = new ArrayList<String>();
		vmArgs.add("-Dcom.ibm.tools.attach.directory="+ ipcDir.getAbsolutePath());
		vmArgs.add("-Dcom.ibm.tools.attach.logging=yes");
		vmArgs.add("-Dcom.ibm.tools.attach.log.name="+logDir.getAbsolutePath()+File.separatorChar);
		for (int i = 0; i < 10; ++i)  {
			TargetManager tgt = new TargetManager(TARGET_VM_CLASS, null, vmArgs, null);
			targets.add(tgt);
			tgt.syncWithTarget();
		}
		logger.debug("Removing directories in "+ipcDir.getAbsolutePath());
		deleteIpcDirectories(ipcDir, targets);
		TargetManager newTgt = new TargetManager(TARGET_VM_CLASS, null, vmArgs, null);
		newTgt.syncWithTarget();
		terminateAndCheckLogs(logDir, newTgt);
		for (TargetManager tgt: targets) {
			deleteIpcDirectories(ipcDir, targets);
			terminateAndCheckLogs(logDir, tgt);
		}
		deleteRecursive(ipcDir);
		deleteRecursive(logDir);
	}

	private void deleteIpcDirectories(File ipcDir,
			ArrayList<TargetManager> targets) throws IOException {
		for (TargetManager victim: targets) {
			if (Objects.isNull(victim)) {
				continue;
			}
			final String targetId = victim.targetId;
			if (Objects.isNull(targetId)) {
				continue;
			}
			File targetDir = new File(ipcDir, targetId);
			if (targetDir.exists()) {
				logger.debug("delete "+victim.getTargetPid());
				/* note that the error recovery mechanisms may recreate files */
				deleteRecursive(targetDir, false);
			}
		}
	}

	private void terminateAndCheckLogs(File logDir, TargetManager tgt) {
		String targetPid = tgt.getTargetPid();
		logger.debug("terminate "+targetPid);
		tgt.terminateTarget(true);
		File logfile = new File(logDir, "_"+targetPid+".log");
		String logfilePath = logfile.getAbsolutePath();

		AssertJUnit.assertTrue("cannot find "+logfilePath, logfile.exists());
		try {
			BufferedReader logReader = new BufferedReader(new FileReader(logfile));
			String line;
			while (null != (line = logReader.readLine())) {
				boolean error = line.contains("Attach API not terminated");
				AssertJUnit.assertFalse("target "+ targetPid + ": wait loop not terminated", error);
			}
			logReader.close();
		} catch (IOException e) {
			Assert.fail("Cannot read "+logfile.getPath());
			Assert.fail("Cannot read "+logfile.getPath());
		}
	}

	@Test
	public void testMemoryExhaustion() {
		final String EXHAUSTER_CLASS = MemoryExhauster.class.getCanonicalName();

		ArrayList<String> vmArgs = new ArrayList<String>();
		vmArgs.add("-Xmx4M");
		vmArgs.add("-Xtrace:print={j9jcl.345}");
		vmArgs.add("-Xdump:none");
		TargetManager tgt = new TargetManager(EXHAUSTER_CLASS, null, vmArgs, null);
		BufferedWriter tgtIn = tgt.getTargetInWriter();
		ProcessStreamReader stdoutReader = new ProcessStreamReader(tgt.getTargetOutReader());
		stdoutReader.start();
		ProcessStreamReader stderrReader = new ProcessStreamReader(tgt.getTargetErrReader());
		stderrReader.start();
		try {
			tgtIn.write(TargetManager.TARGETVM_START);
			int status = tgt.terminateTarget(true);
			stdoutReader.dumpStream("target stdout:");
			stderrReader.dumpStream("target stderr:");
			String stderrOutput = stderrReader.getStreamOutput();
			/* 
			 * Shutdown hook can throw an OOM and exit with status 1. 
			 * Make sure this doesn't happen in attach API code.
			 */
			if (stderrOutput.contains("java.lang.OutOfMemoryError")) {
				AssertJUnit.assertFalse("OOM in attach API", stderrOutput.contains("com.ibm.tools.attach"));
			} else {
				AssertJUnit.assertEquals("OOM not caught", MemoryExhauster.OOM_ERROR_STATUS, status);
			}
		} catch (IOException e) {
			logExceptionInfoAndFail(e);
		}
	}
	
	@Test
	public void testInitializationDeletesStaleDirectoryWithNumericId() {

		TargetManager tgt = new TargetManager(TARGET_VM_CLASS);
		File staleDirectory = createStaleDirectory(tgt, null);
		tgt.terminateTarget(true);
		launchVmAndCheckStaleDeleted(staleDirectory);
	}

	@Test
	public void testInitializationDeletesStaleDirectoryWithTextId() {
		
		TargetManager tgt = new TargetManager(TARGET_VM_CLASS,
				vmId);
		File staleDirectory = createStaleDirectory(tgt, null);
		launchVmAndCheckStaleDeleted(staleDirectory);
	}

	@Test
	public void testInitializationDeletesStaleDirectoryWithPseudoNumericId() {
		
		TargetManager tgt = new TargetManager(TARGET_VM_CLASS);
		File staleDirectory = createStaleDirectory(tgt, "1234_5");
		launchVmAndCheckStaleDeleted(staleDirectory);
	}

	@Test
	public void testInitializationDeletesManyStaleDirectories() {
		if (TestUtil.isWindows()) {
			return;
		}
		
		TargetManager tgt = new TargetManager(TARGET_VM_CLASS);
		int cloneCount = 256;
		int maxIterations = 100; /*
								 * we should delete ~16 stale directories per
								 * iteration
								 */
		cloneTargetDirectory(tgt, cloneCount);

		while (maxIterations-- > 0) {
			tgt = new TargetManager(TARGET_VM_CLASS);
			tgt.terminateTarget(true);
			if (!clonesRemain()) {
				break;
			}
		}
		AssertJUnit.assertTrue("VM launches did not clean up all stale directories",
				maxIterations > 0);
	}

	@Test
	public void testListVmsDeletesStaleDirectoriesWithNumericId() {
		
		TargetManager tgt = new TargetManager(TARGET_VM_CLASS);
		File staleDirectory = createStaleDirectory(tgt, null);
		listVmsAndCheckStaleDeleted(staleDirectory);

	}

	/**
	 * Ensure listVirtualMachines clears the stale directories
	 */
	@Test
	public void testListVmsDeletesStaleDirectoryWithTextId() {
		
		TargetManager tgt = new TargetManager(TARGET_VM_CLASS,
				vmId);
		File staleDirectory = createStaleDirectory(tgt, null);
		listVmsAndCheckStaleDeleted(staleDirectory);
	}

	/**
	 * Ensure listVirtualMachines clears the stale Directory
	 */
	@Test
	public void testListVmsDeletesStaleDirectoryWithPseudoNumericId() {
		
		TargetManager tgt = new TargetManager(TARGET_VM_CLASS);
		File staleDirectory = createStaleDirectory(tgt, "1234_5");
		listVmsAndCheckStaleDeleted(staleDirectory);
	}

	@Test
	public void testPrematureSocketClose() {
		

		TargetManager tgt = connectAndVerifyConnection(false);
		AssertJUnit.assertNotNull("target did not launch", tgt);
		/* kill the socket connection without detaching */
		tgt.terminateTarget(true);
		try {
			/* reconnect to the target */
			tgt = connectAndVerifyConnection(true);
			AssertJUnit.assertNotNull("target did not launch", tgt);
		} catch (Exception e) {
			Assert.fail("unhandled exception: " + e.toString() + "\n" + e.getMessage());
		} finally {
			tgt.terminateTarget(true);
		}
	}

	@Test
	public void testAttachWithCorruptAdvertisementFile() {
		

		TargetManager badTarget = new TargetManager(
				TARGET_VM_CLASS, "badVm");
		badTarget.syncWithTarget();
		info("process " + badTarget.getTargetPid()
				+ " running.  Corrupting its advertisement");
		corruptAdvertisementFile(badTarget);
		try {
			TargetManager goodTarget = connectAndVerifyConnection(true);
			info("connecting to " + goodTarget.getTargetPid());
			goodTarget.terminateTarget(true);
		} catch (Exception e) {
			Assert.fail("Exception while attaching: " + e.toString());
		}
		badTarget.terminateTarget(true);
	}

	@Test
	public void testListVirtualMachinesWithCorruptAdvertisementFile() {
		

		TargetManager badTarget = new TargetManager(
				TARGET_VM_CLASS, "badVm");
		badTarget.syncWithTarget();
		corruptAdvertisementFile(badTarget);
		try {
			@SuppressWarnings("unused")
			List<AttachProvider> providers = com.sun.tools.attach.spi.AttachProvider.providers();
		} catch (Exception e) {
			Assert.fail("Exception while listing: " + e.toString());
		}
	}

	@SuppressWarnings("null")
	@Test
	public void testVmShutdownWhileAttached() {
		
		TargetManager tgt = new TargetManager(TARGET_VM_CLASS);
		tgt.syncWithTarget();
		String tgtId = tgt.getTargetPid();
		boolean iOExceptionThrown = false;

		VirtualMachine tgtVm = null;
		try {
			tgtVm = VirtualMachine.attach(tgtId);
			info("attached to " + tgtId);
		} catch (IOException | AttachNotSupportedException e) {
			logExceptionInfoAndFail(e);
		}
		try {
			tgt.terminateTarget(true);
			info("terminated " + tgtId);
			tgtVm.getSystemProperties();
			info("completed getSystemProperties to " + tgtId);
		} catch (IOException e) {
			iOExceptionThrown = true;
		}
		AssertJUnit.assertTrue(
				"Did not receive IOException when communicating with terminated target",
				iOExceptionThrown);
	}

	
	private void corruptAdvertisementFile(TargetManager badTarget) {
		File targetDir = new File(commonDirectory, badTarget.targetId);
		File advertisement = new File(targetDir, ADVERT_FILENAME);
		try {
			OutputStreamWriter advertisementWriter = new OutputStreamWriter(
					new FileOutputStream(advertisement));
			Random dataGenerator = new Random(12345);
			for (int i = 0; i < 42; ++i) {
				advertisementWriter.write(dataGenerator.nextInt());
			}
			advertisementWriter.close();
		} catch (FileNotFoundException e) {
			Assert.fail("FileNotFoundException opening "
					+ advertisement.getAbsolutePath());
		} catch (IOException e) {
			Assert.fail("IOException opening " + advertisement.getAbsolutePath());
		}
	}

	/**
	 * Attach to the target and do a simple continuity test.
	 * 
	 * @param detach
	 *            detach from the VM if true
	 * @return TargetManager object representing the target.
	 */
	private TargetManager connectAndVerifyConnection(boolean detach) {
		TargetManager targetManager = null;
		try {
			targetManager = new TargetManager(TARGET_VM_CLASS);
			targetManager.syncWithTarget();
			String targetId = targetManager.targetId;
			AssertJUnit.assertNotNull("target did not launch");
			VirtualMachine vm = VirtualMachine.attach(targetId);
			/* verify the connection */
			Properties vmProps = vm.getSystemProperties();
			String vendor = vmProps.getProperty(VENDOR_NAME_PROP);
			String myVendor = System
					.getProperty(VENDOR_NAME_PROP);
			AssertJUnit.assertEquals("my vendor: " + myVendor + " vendor from properties:"
					+ vendor, myVendor, vendor);
			if (detach) {
				vm.detach();
			}
		} catch (AttachNotSupportedException | IOException e) {
			logExceptionInfoAndFail(e);
		}
		return targetManager;
	}

	private File createStaleDirectory(TargetManager tgt, String staleName) {
		AssertJUnit.assertTrue("target process did not launch",tgt.syncWithTarget());

		File targetDir = new File(commonDirectory, tgt.targetId);
		AssertJUnit.assertTrue(
				"target directory not found: " + targetDir.getAbsolutePath(),
				targetDir.exists());
		String originalPath = targetDir.getAbsolutePath();
		targetDir.renameTo(tempStorage);
		logger.debug("rename old target directory to "
				+ tempStorage.getAbsolutePath());
		File oldTarget = new File(originalPath);
		tgt.terminateTarget(true);

		AssertJUnit.assertFalse("old target directory not removed", oldTarget.exists());
		AssertJUnit.assertTrue(
				"old target directory not moved "
						+ tempStorage.getAbsolutePath(), tempStorage.exists());
		logger.debug("Copy the old target directory back");
		String stalePath;
		if (null == staleName) {
			stalePath = originalPath;
		} else {
			stalePath = (new File(commonDirectory, staleName))
					.getAbsolutePath();
		}
		File staleDirectory = new File(stalePath);
		AssertJUnit.assertTrue("restoring rename " + tempStorage.getAbsolutePath() + " to "
				+ staleDirectory.getAbsolutePath() + " failed",
				tempStorage.renameTo(staleDirectory));
		AssertJUnit.assertTrue(
				"old target directory not restored "
						+ staleDirectory.getAbsolutePath(),
				staleDirectory.exists());
		return staleDirectory;

	}

	/**
	 * Create clones of a running process's directory, then kill the process.
	 * That will remove the original process's directory but leave the clones.
	 * This copies only the files. Attach API does not create subdirectories in
	 * the target directory.
	 * 
	 * @param tgt
	 *            Target process
	 * @param cloneCount
	 *            Number of clones
	 */
	private void cloneTargetDirectory(TargetManager tgt, int cloneCount) {
		AssertJUnit.assertTrue("target process did not launch",tgt.syncWithTarget());

		File targetDir = new File(commonDirectory, tgt.targetId);
		AssertJUnit.assertTrue(
				"target directory not found: " + targetDir.getAbsolutePath(),
				targetDir.exists());
		File myCommonDirectory = targetDir.getParentFile();
		File[] originalFiles = targetDir.listFiles();

		for (int i = 0; i < cloneCount; ++i) {
			File cloneDir = new File(myCommonDirectory, CLONE_PREFIX + i);
			AssertJUnit.assertTrue("could not create " + cloneDir.getAbsolutePath(),
					cloneDir.mkdir());
			for (File srcFile : originalFiles) {
				copyFile(cloneDir, srcFile);
			}
		}
		tgt.terminateTarget(true);
	}

	/**
	 * Copy a text file to a specified directory.
	 * 
	 * @param cloneDir
	 * @param srcFile
	 */
	private void copyFile(File cloneDir, File srcFile) {
		final int BUFF_SIZE = 1024;
		char[] cbuf = new char[BUFF_SIZE];
		try {
			File dstFile = new File(cloneDir, srcFile.getName());
			InputStreamReader srcReader = new InputStreamReader(
					new FileInputStream(srcFile));
			OutputStreamWriter dstWriter = new OutputStreamWriter(
					new FileOutputStream(dstFile));

			int nRead = 0;
			while ((nRead = srcReader.read(cbuf, 0, BUFF_SIZE)) != -1) {
				dstWriter.write(cbuf, 0, nRead);
			}
			srcReader.close();
			dstWriter.close();
		} catch (IOException e) {
			Assert.fail("could not open " + srcFile.getAbsolutePath());
		}
	}

	private boolean clonesRemain() {
		File targetDirs[] = commonDirectory.listFiles();

		for (File f : targetDirs) {
			if (f.getName().startsWith(CLONE_PREFIX)) {
				logger.debug("clonesRemain: " + f.getName());
				return true;
			}
		}
		return false;
	}

	private void launchVmAndCheckStaleDeleted(File staleDirectory) {
		ArrayList<String> vmArgs = new ArrayList<String>();
		vmArgs.add("-Dcom.ibm.tools.attach.logging=yes");
		TargetManager tgt;
		logger.debug("start a new process and verify it cleans up");
		tgt = new TargetManager(TARGET_VM_CLASS, null, vmArgs,
				null);
		tgt.syncWithTarget();
		long staleDirectoryPid = getPidFromDirectory(staleDirectory);
		if (!TargetManager.isProcessRunning(staleDirectoryPid)) {
			AssertJUnit.assertFalse(
					"stale target directory not removed "
							+ staleDirectory.getAbsolutePath(),
					staleDirectory.exists());
		} else {
			logger.warn(staleDirectory.getPath()
					+ " not removed because " + staleDirectoryPid
					+ " still alive");
		}
		tgt.terminateTarget(true);
	}

	private long getPidFromDirectory(File dir) {
		if (!dir.exists()) {
			return -1;
		}
		File attachInfo = new File(dir, "attachInfo");
		Properties props = new Properties();
		try {
			props.load(new FileInputStream(attachInfo));
		} catch (FileNotFoundException e) {
			return -1;
		} catch (IOException e) {
			return -1;
		}
		long pid = Long.parseLong(props.getProperty("processId"));
		return pid;
	}

	private void listVmsAndCheckStaleDeleted(File staleDirectory) {
		AssertJUnit.assertTrue("stale directory missing", staleDirectory.exists());
		List<AttachProvider> providers = com.sun.tools.attach.spi.AttachProvider.providers();
		AttachProvider myProvider = providers.get(0);
		myProvider.listVirtualMachines();
		long staleDirectoryPid = getPidFromDirectory(staleDirectory);
		if (!TargetManager.isProcessRunning(staleDirectoryPid)) {
			AssertJUnit.assertFalse("stale directory not removed", staleDirectory.exists());
		}
	}

	private void info(String msg) {
		logger.debug("TEST_INFO: " + msg);
	}

	/**
	 * read and buffer an input stream, e.g. stderr from a process, as soon as it is produced.
	 * CMVC 194775 - child process blocked writing tracepoint output.
	 *
	 */
	class ProcessStreamReader extends Thread {
		BufferedReader processStream;
		StringBuffer psBuffer = new StringBuffer();
	
		public ProcessStreamReader(BufferedReader psReader) {
			this.processStream = psReader;
		}
	
		public void dumpStream(String msg) {
			logger.debug("---------------------------------");
			logger.debug(msg);
			logger.debug(getStreamOutput());
		}

		@Override
		public void run() {
			try {
				String psLine;
				while ((psLine = processStream.readLine()) != null) {
					psBuffer.append(psLine);
					psBuffer.append('\n');
				}
			} catch (IOException e) {
				logExceptionInfoAndFail(e);
			}
		}
		
		String getStreamOutput() {
			return psBuffer.toString();
		}
	}

}
