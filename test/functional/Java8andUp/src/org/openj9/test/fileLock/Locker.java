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
import java.io.InputStreamReader;
import java.io.PrintStream;

@SuppressWarnings("nls")
public class Locker extends Thread {
	public static final String PROGRESS_PREFIX = "PROGRESS_";
	public static final String PROGRESS_WAITING = PROGRESS_PREFIX + "WAITING";
	public static final String PROGRESS_LOCKED = PROGRESS_PREFIX + "LOCKED";
	public static final String PROGRESS_LOCK_FAILED = PROGRESS_PREFIX
			+ "LOCK_FAILED";
	public static final String PROGRESS_TERMINATED = PROGRESS_PREFIX
			+ "TERMINATED";
	public static final String PROGRESS_ERROR = PROGRESS_PREFIX + "ERROR";
	private static PrintStream progressStream;

	public static void main(String[] args) {
		progressStream = System.err;
		if (args.length < 2) {
			sendProgress("error: not enough arguments: need <filename> blocking|nonblocking");
			sendProgress(PROGRESS_ERROR);
			System.exit(1);
		}
		sendProgress("Locker process started");
		boolean blocking = true;
		String modeString = args[1];
		if (modeString.contains(TestFileLocking.LOCK_BLOCKING)) {
			blocking = true;
		} else if (modeString.contains(TestFileLocking.LOCK_NONBLOCKING)) {
			blocking = false;
		} else {
			sendProgress("error: unrecognized option "+ modeString);
			System.exit(1);
		}

		sendProgress("blocking=" + blocking);
		boolean javaLocking = true;
		if (modeString.contains(TestFileLocking.DOMAIN_JAVA)) {
			javaLocking = true;
		} else if (modeString.contains(TestFileLocking.DOMAIN_NATIVE)) {
			javaLocking = false;
		} else {
			sendProgress("error: unrecognized option "
					+ modeString);
			sendProgress(PROGRESS_ERROR);
			System.exit(1);
		}

		File lockFile = new File(TestFileLocking.lockDir, args[0]);
		GenericFileLock myFileLock = GenericFileLock.lockerFactory(javaLocking,
				lockFile, TestFileLocking.TARGET_DIRECTORY_PERMISSIONS);

		String lockfilePath = lockFile.getAbsolutePath();
		if (!lockFile.exists()) {
			sendProgress(lockfilePath + " does not exist");
			sendProgress(PROGRESS_ERROR);
		}

		try {
			boolean result = false;
			if (blocking) {
				/*
				 * this will block: we will go for the lock and stop because the
				 * main process has the file locked
				 */
				sendProgress(PROGRESS_WAITING);
				result = myFileLock.lockFile(blocking);
				sendProgress("Locker process blocking lock on "
						+ lockfilePath + (result ? " succeeded" : " failed"));
			} else {
				/*
				 * this will not block: we will go for the lock and fail because
				 * the main process has the file locked
				 */
				result = myFileLock.lockFile(blocking);
				sendProgress("Locker process non-blocking lock on "
								+ lockfilePath
								+ (result ? " succeeded" : " failed"));
			}
			sendProgress(result ? PROGRESS_LOCKED
					: PROGRESS_LOCK_FAILED);

			/*
			 * use stdin to synchronize with the main process: the main process
			 * will send a "stop" command when this process can proceed
			 */
			BufferedReader ir = new BufferedReader(new InputStreamReader(
					System.in));
			String inline = "";
			do {
				inline = ir.readLine();
				sendProgress("received " + inline);
			} while (!inline.contains(TestFileLocking.STOP_CMD));
			sendProgress("Locker process stopping");

			myFileLock.unlockFile();
			sendProgress("closing the lock file");
			myFileLock.close();
		} catch (Exception e) {
			sendProgress("unexpected exception");
			e.printStackTrace();
			sendProgress(PROGRESS_ERROR);
		} finally {
			sendProgress(PROGRESS_TERMINATED);
		}
	}

	/**
	 * Send a message to the parent process
	 * @param msg message to send
	 */
	private static void sendProgress(String msg) {
		progressStream.println(msg);
	}
}
