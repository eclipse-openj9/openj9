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
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.channels.FileChannel;
import java.nio.channels.FileLock;

public class JavaFileLock extends GenericFileLock {

	FileChannel javaLockChannel = null;
	private FileOutputStream lockFileOutputStream;
	private FileLock myLock;

	public JavaFileLock(File lockFile, int mode) throws Exception {
		lockFileOutputStream = new FileOutputStream(lockFile);
		javaLockChannel = lockFileOutputStream.getChannel();
	}

	@Override
	public boolean lockFile(boolean blocking) throws Exception {
		if (blocking) {
			myLock = javaLockChannel.lock();
		} else {
			myLock = javaLockChannel.tryLock();
		}
		return (null != myLock);
	}

	@Override
	public void unlockFile() throws Exception {
		if (null != myLock) {
			myLock.release();
		}
	}

	@Override
	public void close() throws IOException {
		lockFileOutputStream.close();
	}

}
