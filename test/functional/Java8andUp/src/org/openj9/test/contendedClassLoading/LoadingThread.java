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
package org.openj9.test.contendedClassLoading;

public class LoadingThread extends Thread {

	String className;
	ClassLoader ldr;
	public volatile boolean loadedCorrectly = false;
	public volatile boolean expectLoadFail = false;

	public void setExpectLoadFail(boolean expectLoadFail) {
		this.expectLoadFail = expectLoadFail;
	}

	public LoadingThread(String className, ClassLoader ldr) {
		super();
		this.className = className;
		this.ldr = ldr;
	}

	@Override
	public void run() {
		try {
			ParallelClassLoadingTests.logMessage("LoadingThread", " starting");
			Class<?> myClass = Class.forName(className, true, ldr);
			ParallelClassLoadingTests.logMessage("LoadingThread", " complete");
			loadedCorrectly = (null != myClass);
		} catch (ClassNotFoundException e) {
			if (expectLoadFail) {
				ParallelClassLoadingTests.logger.debug("expected ClassNotFoundException for " + className);
			} else {
				e.printStackTrace();
			}
		} catch (LinkageError e) {
			e.printStackTrace();
			loadedCorrectly = false;
		}
	}

}
