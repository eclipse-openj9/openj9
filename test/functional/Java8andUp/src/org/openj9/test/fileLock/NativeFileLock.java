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
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

@SuppressWarnings("nls")
public class NativeFileLock extends GenericFileLock {
	private Method unlockFileMethod;
	private Method lockFileMethod;
	private Object fileLockObject;

	public NativeFileLock(File lockFile, int mode) throws Exception {
		this.lockFile = lockFile;

		TestFileLocking.logger.debug("enter NativeFileLock");
		Class<?> nativeFileLock = Class
				.forName("com.ibm.tools.attach.target.FileLock");
		Constructor<?> nflConstructor = nativeFileLock.getDeclaredConstructor(
				java.lang.String.class, int.class);
		nflConstructor.setAccessible(true);
		fileLockObject = nflConstructor.newInstance(lockFile.getAbsolutePath(), new Integer(mode));
		lockFileMethod = nativeFileLock.getDeclaredMethod("lockFile",
				boolean.class);
		lockFileMethod.setAccessible(true);
		unlockFileMethod = nativeFileLock.getDeclaredMethod("unlockFile");
		unlockFileMethod.setAccessible(true);
		TestFileLocking.logger.debug("exit NativeFileLock");
	}

	@Override
	public boolean lockFile(boolean blocking) throws Exception {
		Boolean result = new Boolean(true);
		TestFileLocking.logger.debug("lockfile blocking =" + blocking);
		result = (Boolean) lockFileMethod.invoke(fileLockObject, new Boolean(blocking));
		return result.booleanValue();
	}

	@Override
	public void unlockFile() throws Exception {
		unlockFileMethod.invoke(fileLockObject);
	}
}
