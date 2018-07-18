/*[INCLUDE-IF Sidecar16]*/
/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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

package com.ibm.tools.attach.target;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.channels.FileChannel;
import java.util.TimerTask;
import static com.ibm.tools.attach.target.IPC.LOGGING_DISABLED;
import static com.ibm.tools.attach.target.IPC.loggingStatus;

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
	 * @return true is file lock obtained.
	 * @throws IOException if the file is already locked.
	 * @note locking and unlocking must be done by the same thread.
	 */
	public boolean lockFile(boolean blocking) throws IOException {
		if (LOGGING_DISABLED != loggingStatus) {
			IPC.logMessage(blocking? "locking file ": "non-blocking locking file ", lockFilepath);  //$NON-NLS-1$//$NON-NLS-2$
		}
		
		final int FILE_LOCK_TIMEOUT = 60 * 1000; /* time in milliseconds */
		if (locked) {
			/*[MSG "K0574", "file already locked"]*/
			throw new IOException(com.ibm.oti.util.Msg.getString("K0574")); //$NON-NLS-1$
		}
		
		/* lock will usually be uncontended, so don't start a watchdog unless necessary */
		fileDescriptor = lockFileImpl(lockFilepath, fileMode, false);
		locked = (0 <= fileDescriptor); /* negative values indicate error, non-negative values (including 0) are valid FDs */
		
		if (!locked && blocking) { /* try again, this time with a blocking lock and a timeout */
			FileLockWatchdogTask wdog = new FileLockWatchdogTask();
			IPC.logMessage("lock failed, trying blocking lock"); //$NON-NLS-1$
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
				fileLockWatchdogTimer.schedule(wdog, FILE_LOCK_TIMEOUT);
				}
				else {
					return false;
				}
			}
			
			/*[PR 199171] native file locking is not interruptible from Java */
			try {
				lockFileRAF = new RandomAccessFile(lockFilepath, "rw"); //$NON-NLS-1$
				FileChannel lockFileChannel = lockFileRAF.getChannel();
				lockObject = lockFileChannel.lock();
				IPC.logMessage("Blocking lock succeeded"); //$NON-NLS-1$
				locked = true;
			} catch (IOException e) {
				locked = false;
			}
			synchronized (shutdownSync) { 
				if (null != fileLockWatchdogTimer) { 
					wdog.cancel();
				}
			}
		}

		return locked;
	}
	
	/**
	 * Release the lock on a file.
	 */
	public void unlockFile() {
		if (LOGGING_DISABLED != loggingStatus) {
			IPC.logMessage("unlocking file ", lockFilepath);  //$NON-NLS-1$
		}
		java.nio.channels.FileLock lockObjectCopy = lockObject;
		if (null != lockObjectCopy) {
			IPC.logMessage("closing ", lockFilepath);  //$NON-NLS-1$
			try {
				lockObjectCopy.release();
				lockFileRAF.close();
			} catch (IOException e) {
				IPC.logMessage("IOException unlocking file "+lockFilepath, e); //$NON-NLS-1$
			}
		}
		if (locked && (fileDescriptor >= 0)) {
			unlockFileImpl(fileDescriptor);
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
			unlockFile(); /* unlocks the file if this process, and closes the file descriptor to break the wait */
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
