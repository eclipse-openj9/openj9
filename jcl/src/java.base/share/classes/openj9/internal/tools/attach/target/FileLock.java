/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*
 * Copyright IBM Corp. and others 2010
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */

package openj9.internal.tools.attach.target;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.channels.FileChannel;
import java.util.TimerTask;
import static openj9.internal.tools.attach.target.IPC.LOGGING_DISABLED;
import static openj9.internal.tools.attach.target.IPC.loggingStatus;

public final class FileLock {
	long fileDescriptor;
	String lockFilepath;
	int fileMode;
	private boolean locked;
	private static FilelockTimer fileLockWatchdogTimer;
	static final class syncObject {};
	private static syncObject shutdownSync = new syncObject();
	static boolean terminated;
	private volatile java.nio.channels.FileLock lockObject;
	private  volatile RandomAccessFile lockFileRAF;

	boolean isLocked() {
		return locked;
	}

	public FileLock(String filePath, int mode) {
		locked = false;
		if (null == filePath) {
			throw new NullPointerException("filePath is null"); //$NON-NLS-1$
		}
		lockFilepath= filePath;
		fileMode = mode;
		lockObject = null;
	}

	/**
	 * locks a file using the J9 port library functions.
	 * @param blocking if true, the function waits until the it obtains the file lock or times out.
	 * @param callSite caller info
	 * @param useWatchdog use FileLockWatchdogTask or not
	 * @return true is file lock obtained.
	 * @throws IOException if the file is already locked.
	 * @note locking and unlocking must be done by the same thread.
	 */
	public boolean lockFile(boolean blocking, String callSite, boolean useWatchdog) throws IOException {
		String lockFileMsg = (blocking ? "locking file " : "non-blocking locking file ") //$NON-NLS-1$//$NON-NLS-2$
				+ (useWatchdog ? "using watchdog" : "NOT using watchdog"); //$NON-NLS-1$//$NON-NLS-2$
		IPC.logMessage(callSite + " : " + lockFileMsg, lockFilepath); //$NON-NLS-1$

		final int FILE_LOCK_TIMEOUT = 20 * 1000; /* time in milliseconds */
		if (locked) {
			/*[MSG "K0574", "file already locked"]*/
			throw new IOException(com.ibm.oti.util.Msg.getString("K0574")); //$NON-NLS-1$
		}

		/* lock will usually be uncontended, so don't start a watchdog unless necessary */
		fileDescriptor = lockFileImpl(lockFilepath, fileMode, false);
		locked = (0 <= fileDescriptor); /* negative values indicate error, non-negative values (including 0) are valid FDs */

		if (!locked && blocking) { /* try again, this time with a blocking lock and a timeout */
			FileLockWatchdogTask wdog = null;
			IPC.logMessage("lock failed, trying blocking lock, fileDescriptor = " + fileDescriptor); //$NON-NLS-1$
			if (useWatchdog) {
				wdog = new FileLockWatchdogTask();
				synchronized (shutdownSync) { /* shutdown is called from a different thread */
					/*[PR Jazz 30075] inlined createFileLockWatchdogTimer*/
					if (!terminated && (null == fileLockWatchdogTimer)) {
						fileLockWatchdogTimer = new FilelockTimer("file lock watchdog"); //$NON-NLS-1$
					}
					if (null != fileLockWatchdogTimer) { /* check if the VM is shutting down */
						/*
						 * The file lock timeout is to recover from pathological conditions such as a hung process which is holding the file lock.
						 * Under this condition, the process will break the lock.
						 * This timeout affects only the operation of the attach API.  It may delay the start of the attach API (but will not affect
						 * other aspects of the VM or application launch) or delay an attach attempt.  It will not delay the VM or attach API shutdown.
						 */
						IPC.logMessage("FileLock.lockFile() FILE_LOCK_TIMEOUT = " + FILE_LOCK_TIMEOUT, new Throwable("")); //$NON-NLS-1$ //$NON-NLS-2$
						fileLockWatchdogTimer.schedule(wdog, FILE_LOCK_TIMEOUT);
					}
					else {
						IPC.logMessage("FileLock.lockFile() returns false."); //$NON-NLS-1$
						return false;
					}
				}
			}

			/*[PR 199171] native file locking is not interruptible from Java */
			try {
				IPC.logMessage("FileLock.lockFile() before RandomAccessFile creation"); //$NON-NLS-1$
				lockFileRAF = new RandomAccessFile(lockFilepath, "rw"); //$NON-NLS-1$
				FileChannel lockFileChannel = lockFileRAF.getChannel();
				IPC.logMessage("FileLock.lockFile() before lockFileChannel.lock()"); //$NON-NLS-1$
				lockObject = lockFileChannel.lock();
				IPC.logMessage("FileLock.lockFile() blocking lock succeeded"); //$NON-NLS-1$
				locked = true;
			} catch (IOException e) {
				unlockFile("lockFile_IOException"); //$NON-NLS-1$
				IPC.logMessage("FileLock.lockFile() blocking lock failed with lockFilepath = " + lockFilepath + ", exception message: " + e.getMessage()); //$NON-NLS-1$ //$NON-NLS-2$
			}
			if (useWatchdog) {
				synchronized (shutdownSync) {
					if (null != fileLockWatchdogTimer) {
						wdog.cancel();
					}
				}
			}
		} else {
			IPC.logMessage("FileLock.lockFile() locking file succeeded, locked = " + locked + ", fileDescriptor = " + fileDescriptor); //$NON-NLS-1$ //$NON-NLS-2$
		}

		return locked;
	}

	/**
	 * Release the lock on a file.
	 *
	 * @param callSite caller info
	 */
	public void unlockFile(String callSite) {
		IPC.logMessage(callSite + "_unlockFile() ", lockFilepath); //$NON-NLS-1$
		// unlockFileImpl(fileDescriptor) is placed before FileLock.release()/RandomAccessFile.close() to avoid the chance
		// that the portlibrary api might affect OpenJDK release/close.
		// There is only one lock/unlock path, either portlibrary or OpenJDK methods, so technically order is not important.
		if (locked && (fileDescriptor >= 0)) {
			IPC.logMessage(callSite + "_unlockFile : unlockFileImpl fileDescriptor " + fileDescriptor); //$NON-NLS-1$
			unlockFileImpl(fileDescriptor);
			fileDescriptor = -1;
		}
		java.nio.channels.FileLock lockObjectCopy = lockObject;
		if (null != lockObjectCopy) {
			IPC.logMessage("FileLock.unlockFile closing lockObjectCopy ", lockFilepath); //$NON-NLS-1$
			try {
				lockObjectCopy.release();
				lockObject = null;
			} catch (IOException e) {
				IPC.logMessage("IOException at lockObjectCopy.release() with lockFilepath = " + lockFilepath, e); //$NON-NLS-1$
			}
		}

		RandomAccessFile lockFileRAFCopy = lockFileRAF;
		if (lockFileRAFCopy != null) {
			IPC.logMessage("FileLock.unlockFile closing lockFileRAFCopy ", lockFilepath); //$NON-NLS-1$
			try {
				lockFileRAFCopy.close();
				lockFileRAF = null;
			} catch (IOException e) {
				IPC.logMessage("IOException at lockFileRAFCopy.close() with lockFilepath = " + lockFilepath, e); //$NON-NLS-1$
			}
		}
		locked = false;
	}

	static void shutDown() {
		/*
		 * After the last live reference to a Timer object goes away and all
		 * outstanding tasks have completed execution, the timer's task
		 * execution thread terminates
		 */
		synchronized (shutdownSync) {
			terminated = true;
			if (null != fileLockWatchdogTimer) {
				fileLockWatchdogTimer.cancel();
				fileLockWatchdogTimer.purge();
			}
			fileLockWatchdogTimer = null;
		}
	}

	private static native long lockFileImpl(String filePath, int mode, boolean blocking);

	private static native int unlockFileImpl(long fileDesc);

	final class FileLockWatchdogTask extends TimerTask {
		@Override
		/**
		 * Wait for a timeout and delete and recreate the lock file.  Interrupt this thread if the lock is obtained before the timeout.
		 * @return true if the thread was interrupted or the lock file was replaced.
		 */
		public void run() {
			IPC.logMessage("waitAndCheckLock recreating ", lockFilepath); //$NON-NLS-1$
			/* recreate the file */
			File theFile = new File(lockFilepath);
			if (!theFile.renameTo(new File(theFile.getParent(), '.'
					+ "trash_" + IPC.getRandomNumber()))) { //$NON-NLS-1$
				IPC.logMessage("waitAndCheckLock could not rename ", theFile.getName()); //$NON-NLS-1$
				/*[PR Jazz 30075] TODO Handle the case where the rename fails. */
				theFile.renameTo(new File(theFile.getParent(), '.' + "trash_" + IPC.getRandomNumber()));  //$NON-NLS-1$
				/* retry once.  If it fails more than once, it's a systemic problem */
			}
			/* unlocks the file if this process, and closes the file descriptor to break the wait */
			unlockFile("FileLock.FileLockWatchdogTask"); //$NON-NLS-1$
			/* delete the file if it's there */
			if (!theFile.delete()) {
				IPC.logMessage("waitAndCheckLock could not delete ", theFile.getAbsolutePath()); //$NON-NLS-1$
			} else {
				IPC.logMessage("waitAndCheckLock deleted ", theFile.getAbsolutePath()); //$NON-NLS-1$
			}
			IPC.logMessage("waitAndCheckLock normal return "); //$NON-NLS-1$
			return;
		}
	}
}
