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

import java.io.File;
import java.io.IOException;

/**
 * Generic interface to file locking.
 * 
 */
public abstract class GenericFileLock {
	protected File lockFile;

	/**
	 * Try to lock the file
	 * 
	 * @param blocking
	 *            stop if the file is locked
	 * @return true if the file was locked. Will be false if blocking=false and
	 *         the file is already locked.
	 * @throws Exception
	 *             on error condition
	 */
	public abstract boolean lockFile(boolean blocking) throws Exception;

	public abstract void unlockFile() throws Exception;

	/**
	 * Create a file lock using either Java class library file locks or using J9
	 * VM port library native locks
	 * 
	 * @param useJavaLocking
	 *            select Java or native locking
	 * @param lockFile
	 *            path to the file to be locked.
	 * @param fileMode
	 *            POSIX file permissions for the lock.
	 * @return file lock object.
	 */
	static GenericFileLock lockerFactory(boolean useJavaLocking, File lockFile,
			int fileMode) {
		try {
			if (useJavaLocking) {
				return new JavaFileLock(lockFile, fileMode);
			} else {
				return new NativeFileLock(lockFile, fileMode);
			}
		} catch (Exception e) {
			e.printStackTrace();
			return null;
		}
	}

	@SuppressWarnings("unused")
	public void close() throws IOException {
		return;
	}
}
