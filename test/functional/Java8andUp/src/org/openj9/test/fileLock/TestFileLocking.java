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
package org.openj9.test.fileLock;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintStream;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

import org.testng.Assert;
import org.testng.AssertJUnit;
import org.testng.annotations.AfterMethod;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@SuppressWarnings("nls")
@Test(groups = { "level.extended" })
public class TestFileLocking {

	public static final String LOCK_BLOCKING = "doblocking";
	public static final String LOCK_NONBLOCKING = "nonblocking";
	static final String DOMAIN_NATIVE = "native";
	static final String DOMAIN_JAVA = "java";
	static final File lockDirRoot = getLockDirRoot();
	static final String STOP_CMD = "stop";
	static final String LOCKER_PKG = "org.openj9.test.fileLock";
	static final String LOCKER_CMD = LOCKER_PKG + '.' + "Locker";
	static final String lockFileName = "lockFile";
	private File lockFile;
	private String classpath = System.getProperty("java.class.path");
	static final File lockDir = new File(lockDirRoot, "lockDir");
	private Process lockProc;
	static Logger logger = Logger.getLogger(TestFileLocking.class);

	public final static int TARGET_DIRECTORY_PERMISSIONS = 01777;
	private GenericFileLock myFileLock;
	private String testName;

	private static File getLockDirRoot() {
		String tmpDir = System.getProperty("java.io.tmpdir");
		return new File(tmpDir);
	}

	@BeforeMethod
	protected void setUp(Method testMethod) throws Exception {
		testName = testMethod.getName();
		logger.debug(System.currentTimeMillis()
				+ ": >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"+
				testName);
		if (!lockDir.mkdir() && !lockDir.exists()) {
			logger.error("could not create " + lockDir.getAbsolutePath());
		}
		lockFile = new File(lockDir, lockFileName);
		deleteLockFile(lockFile);
		try {
			logger.debug("creating " + lockFile.getAbsolutePath());
			AssertJUnit.assertTrue("could not create lock file", lockFile.createNewFile());
			logger.debug("lockfile created");
		} catch (IOException e) {
			e.printStackTrace();
			logger.error("error creating lockfile " + lockFile.getAbsolutePath());
			Assert.fail();
		}
		logger.debug("Check lock directory contents on setUp");
		dumpLockDirectory(lockDir); /* look for bogus files */
	}

	@AfterMethod
	protected void tearDown() throws Exception {
		myFileLock.close();
		emptyLockDirectory(lockDir);
		logger.debug(System.currentTimeMillis()
				+ ": <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");

	}

	/*
	 * First the thread locks the file, then the process.
	 */
	private void contendForLock(boolean mainProcessNonblocking,
			boolean mainProcessJavaLocking, boolean otherProcessNonblocking,
			boolean otherProcessJavaLocking) {
		String lockDomain = otherProcessJavaLocking ? DOMAIN_JAVA
				: DOMAIN_NATIVE;
		String lockBlock = otherProcessNonblocking ? LOCK_NONBLOCKING
				: LOCK_BLOCKING;
		String lockType = lockDomain + '_' + lockBlock;

		myFileLock = GenericFileLock.lockerFactory(mainProcessJavaLocking,
				lockFile, TARGET_DIRECTORY_PERMISSIONS);
		LinkedBlockingQueue<String> lockerProcessStatus = new LinkedBlockingQueue<String>();
		logger.debug("Main process locking file"
				+ (mainProcessNonblocking ? " non-" : " ") + "blocking"
				+ (mainProcessJavaLocking ? " Java" : " native") + " locking");

		/* the lock is uncontended. Should not block or fail */
		boolean locked = false;
		try {
			locked = myFileLock.lockFile(mainProcessNonblocking);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("unexpected exception locking file in main process");
		}
		AssertJUnit.assertTrue("main process did not lock the file", locked);

		String[] jvmArgs_1 = { "-Xint", "-Dcom.ibm.tools.attach.enable=yes" };
		startLockerProcess(lockType, jvmArgs_1, lockerProcessStatus);
		String currentState = readStatus(lockerProcessStatus);
		if (otherProcessNonblocking) {
			logger.debug("verify locker process not blocked");
			/*
			 * LOCK_FAILED indicates that the process tried to lock the file but
			 * failed because it was already locked
			 */
			AssertJUnit.assertEquals("locker process lock operation did not fail",
					Locker.PROGRESS_LOCK_FAILED, currentState);
		} else {
			pause();
			/*
			 * wait a while to see if the locker process proceeded. Note that
			 * there is no way to determine directly if a process is blocked on
			 * an OS call.
			 */
			logger.debug("verify locker process  blocked");
			AssertJUnit.assertEquals("locker process not blocked", Locker.PROGRESS_WAITING,
					currentState);
		}
		try {
			myFileLock.unlockFile();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("unexpected exception unlocking file in main process");
		}

		if (!otherProcessNonblocking) {
			logger.debug("verify locker process unblocked");
			currentState = readStatus(lockerProcessStatus);
			AssertJUnit.assertEquals("locker process did not lock the file",
					Locker.PROGRESS_LOCKED, currentState);
		}
		terminateLockerProcess();
		currentState = readStatus(lockerProcessStatus);
		AssertJUnit.assertNotNull("timeout waiting for semaphore", currentState);
		AssertJUnit.assertEquals("locker process did not terminate",
				Locker.PROGRESS_TERMINATED, currentState);
	}

	/**
	 * Start a second process
	 * 
	 * @param lockType
	 *            blocking/nonblocking; java/native
	 * @param jvmArgs
	 *            command line arguments
	 * @param lockerProcessStatus
	 *            used to synchronize with the main thread.
	 * @return
	 */
	private void startLockerProcess(String lockType, String[] jvmArgs,
			LinkedBlockingQueue<String> lockerProcessStatus) {
		StreamDumper sd = null;

		String[] cmdarray = createCommandLine(lockType, jvmArgs);
		try {
			lockProc = java.lang.Runtime.getRuntime().exec(cmdarray);
			AssertJUnit.assertNotNull("startLockerProcess: locker process not launched",
					lockProc);
			sd = new StreamDumper(lockProc.getErrorStream(),
					lockerProcessStatus);
			sd.start();
		} catch (IOException e) {
			logger.error("could not launch " + LOCKER_CMD + ": " + e.getMessage());
		}
		logger.debug("locker process launched");
	}

	private String[] createCommandLine(String lockType, String[] jvmArgs) {
		String javaHome = System.getProperty("java.home");
		ArrayList<String> cmdElements = new ArrayList<String>();
		cmdElements.add(javaHome + "/bin/java");
		if (null != jvmArgs) {
			cmdElements.addAll(Arrays.asList(jvmArgs));
		}
		String sidecar = System.getProperty("java.sidecar");
		if ((null != sidecar) && (0 != sidecar.length())) {
			String sidecarArgs[] = sidecar.split(" +");
			cmdElements.addAll(Arrays.asList(sidecarArgs));
		}
		cmdElements.add("-classpath");
		cmdElements.add(classpath);
		cmdElements.add(LOCKER_CMD);
		cmdElements.add(lockFileName);
		cmdElements.add(lockType);

		String[] cmdline = new String[cmdElements.size()];
		cmdElements.toArray(cmdline);
		StringBuilder cmdLineBuffer = new StringBuilder();
		String sep = "";
		for (String s : cmdline) {
			cmdLineBuffer.append(sep);
			cmdLineBuffer.append(s);
			sep = " ";
		}
		cmdLineBuffer.append('\n');
		logger.debug(cmdLineBuffer.toString());
		return cmdline;
	}

	private void terminateLockerProcess() {
		AssertJUnit.assertNotNull("terminateLockerProcess: locker process not launched",
				lockProc);
		OutputStream os = lockProc.getOutputStream();
		PrintStream osw = new PrintStream(os);
		osw.println(STOP_CMD);
		osw.flush();
	}

	private String readStatus(LinkedBlockingQueue<String> lockerProcessStatus) {
		String status = null;
		try {
			status = lockerProcessStatus.poll(60, TimeUnit.SECONDS);
		} catch (InterruptedException e) {
			Assert.fail("semaphore interrupted");
		}
		return status;
	}

	private void deleteLockFile(File f) {
		if (f.exists()) {
			logger.debug("Deleting " + f.getAbsolutePath());
			AssertJUnit.assertTrue("error deleting " + f.getAbsolutePath(), f.delete());
			AssertJUnit.assertFalse("File still exists: " + f.getAbsolutePath(), f.exists());
			logger.debug(f.getAbsolutePath() + " lockfile deleted");
		}
	}

	private void emptyLockDirectory(File root) {
		if (null != root) {
			logger.debug("Delete contents of lock directory");
			for (File f : root.listFiles()) {
				if (!f.delete()) {
					Assert.fail("could not delete " + f.getAbsolutePath());
				}
			}
		}
	}

	private void dumpLockDirectory(File root) {
		if (null != root) {
			logger.debug("Check contents of lock directory");
			for (File f : root.listFiles()) {
				logger.debug(f.getAbsolutePath());
			}
		}
	}

	@Test
	public void testNativeFileLocking_sanity() {

		try {
			myFileLock = GenericFileLock.lockerFactory(false, lockFile,
					TARGET_DIRECTORY_PERMISSIONS);
			AssertJUnit.assertTrue(myFileLock.lockFile(true));
			logger.debug("native blocking lock successful");
			myFileLock.unlockFile();
			AssertJUnit.assertTrue(myFileLock.lockFile(true));
			logger.debug("native blocking second lock successful");
			myFileLock.unlockFile();
			AssertJUnit.assertTrue(myFileLock.lockFile(false));
			logger.debug("native nonblocking lock successful");
			myFileLock.unlockFile();

		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("exception in native locking");
		}
	}

	/**
	 * Method naming scheme: C=native lock code, J=Java locks, B=blocking,
	 * N=nonblocking first pair describes the locking thread, second describes
	 * the locking process. The thread locks the file before the process
	 */
	@Test
	public void testCB_CB() {
		boolean threadJavaLocking = false;
		boolean threadNonblocking = false;
		boolean processJavaLocking = false;
		boolean processNonblocking = false;
		contendForLock(threadNonblocking, threadJavaLocking,
				processNonblocking, processJavaLocking);
	}

	@Test
	public void testCB_CN() {
		boolean threadJavaLocking = false;
		boolean threadNonblocking = false;
		boolean processJavaLocking = false;
		boolean processNonblocking = true;
		contendForLock(threadNonblocking, threadJavaLocking,
				processNonblocking, processJavaLocking);
	}

	@Test
	public void testCB_JB() {
		boolean threadJavaLocking = false;
		boolean threadNonblocking = false;
		boolean processJavaLocking = true;
		boolean processNonblocking = false;
		contendForLock(threadNonblocking, threadJavaLocking,
				processNonblocking, processJavaLocking);
	}

	@Test
	public void testCB_JN() {
		boolean threadJavaLocking = false;
		boolean threadNonblocking = false;
		boolean processJavaLocking = true;
		boolean processNonblocking = true;
		contendForLock(threadNonblocking, threadJavaLocking,
				processNonblocking, processJavaLocking);
	}

	@Test
	public void testCN_CB() {
		boolean threadJavaLocking = false;
		boolean threadNonblocking = true;
		boolean processJavaLocking = false;
		boolean processNonblocking = false;
		contendForLock(threadNonblocking, threadJavaLocking,
				processNonblocking, processJavaLocking);
	}

	@Test
	public void testCN_CN() {
		boolean threadJavaLocking = false;
		boolean threadNonblocking = true;
		boolean processJavaLocking = false;
		boolean processNonblocking = true;
		contendForLock(threadNonblocking, threadJavaLocking,
				processNonblocking, processJavaLocking);
	}

	@Test
	public void testCN_JB() {
		boolean threadJavaLocking = false;
		boolean threadNonblocking = true;
		boolean processJavaLocking = true;
		boolean processNonblocking = false;
		contendForLock(threadNonblocking, threadJavaLocking,
				processNonblocking, processJavaLocking);
	}

	@Test
	public void testCN_JN() {
		boolean threadJavaLocking = false;
		boolean threadNonblocking = true;
		boolean processJavaLocking = true;
		boolean processNonblocking = true;
		contendForLock(threadNonblocking, threadJavaLocking,
				processNonblocking, processJavaLocking);
	}

	@Test
	public void testJB_CB() {
		boolean threadJavaLocking = true;
		boolean threadNonblocking = false;
		boolean processJavaLocking = false;
		boolean processNonblocking = false;
		contendForLock(threadNonblocking, threadJavaLocking,
				processNonblocking, processJavaLocking);
	}

	@Test
	public void testJB_CN() {
		boolean threadJavaLocking = true;
		boolean threadNonblocking = false;
		boolean processJavaLocking = false;
		boolean processNonblocking = true;
		contendForLock(threadNonblocking, threadJavaLocking,
				processNonblocking, processJavaLocking);
	}

	@Test
	public void testJB_JB() {
		boolean threadJavaLocking = true;
		boolean threadNonblocking = false;
		boolean processJavaLocking = true;
		boolean processNonblocking = false;
		contendForLock(threadNonblocking, threadJavaLocking,
				processNonblocking, processJavaLocking);
	}

	@Test
	public void testJB_JN() {

		boolean threadJavaLocking = true;
		boolean threadNonblocking = false;
		boolean processJavaLocking = true;
		boolean processNonblocking = true;
		contendForLock(threadNonblocking, threadJavaLocking,
				processNonblocking, processJavaLocking);
	}

	@Test
	public void testJN_CB() {
		boolean threadJavaLocking = true;
		boolean threadNonblocking = true;
		boolean processJavaLocking = false;
		boolean processNonblocking = false;
		contendForLock(threadNonblocking, threadJavaLocking,
				processNonblocking, processJavaLocking);
	}

	@Test
	public void testJN_CN() {
		boolean threadJavaLocking = true;
		boolean threadNonblocking = true;
		boolean processJavaLocking = false;
		boolean processNonblocking = true;
		contendForLock(threadNonblocking, threadJavaLocking,
				processNonblocking, processJavaLocking);
	}

	@Test
	public void testJN_JB() {
		boolean threadJavaLocking = true;
		boolean threadNonblocking = true;
		boolean processJavaLocking = true;
		boolean processNonblocking = false;
		contendForLock(threadNonblocking, threadJavaLocking,
				processNonblocking, processJavaLocking);
	}

	@Test
	public void testJN_JN() {
		boolean threadJavaLocking = true;
		boolean threadNonblocking = true;
		boolean processJavaLocking = true;
		boolean processNonblocking = true;
		contendForLock(threadNonblocking, threadJavaLocking,
				processNonblocking, processJavaLocking);
	}

	void pause() {
		pause(2);
	}

	void pause(int nSeconds) {
		try {
			Thread.sleep(nSeconds * 1000);
		} catch (InterruptedException e) {
			Assert.fail(e.toString());
		}
	}

	/**
	 * Asynchronously print the output from the locker process. Detect and
	 * report state changes using keywords in the text. Post to the semaphore on
	 * state changes.
	 * 
	 */
	private class StreamDumper extends Thread {
		private BufferedReader inputReader;
		private LinkedBlockingQueue<String> statusQueue;

		StreamDumper(InputStream is,
				LinkedBlockingQueue<String> lockerProcessStatus) {
			this.inputReader = new BufferedReader(new InputStreamReader(is));
			statusQueue = lockerProcessStatus;
		}

		@Override
		public void run() {
			String inLine;
			try {
				while (null != (inLine = inputReader.readLine())) {
					logger.error(inLine);
					if (inLine.contains(Locker.PROGRESS_PREFIX)) {
						if (inLine.contains(Locker.PROGRESS_ERROR)) {
							Assert.fail("Error in locker process: "+inLine);
						}
						String currentState = inLine.substring(inLine
								.indexOf(Locker.PROGRESS_PREFIX));
						statusQueue.put(currentState);
					}
				}
			} catch (IOException e) {
				e.printStackTrace();
				Assert.fail("unexpected IOException");
			} catch (InterruptedException e) {
				e.printStackTrace();
				Assert.fail("unexpected InterruptedException");
			}

		}
	}
}
