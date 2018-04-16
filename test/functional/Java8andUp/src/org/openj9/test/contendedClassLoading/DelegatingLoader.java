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

public class DelegatingLoader extends ClassLoader {
	String myName;
	private Object syncObject = null;
	public DelegatingLoader childLdr = null;
	public volatile boolean loadingPreq = false;
	public volatile int numWaiting = 0;
	static boolean parallelCapable = registerAsParallelCapable();

	public Object getPausingThread() {
		return syncObject;
	}

	public void setSyncObject(Object syncObject) {
		this.syncObject = syncObject;
	}

	public DelegatingLoader(DelegatingLoader parentLdr) {
		super(parentLdr);
	}

	public DelegatingLoader() {
		super();
	}

	public void setMyName(String myName) {
		this.myName = myName;
	}

	@Override
	public Class<?> loadClass(String className) throws ClassNotFoundException {
		try {
			final boolean loadingTarget = className.equals(ParallelClassLoadingTests.TARGET_CLASS_NAME);
			final String threadName = Thread.currentThread().getName();
			final String preamble = threadName + " " + myName;
			ParallelClassLoadingTests.logMessage(myName, " loading " + className);
			ClassLoader parent = getParent();
			if (ParallelClassLoadingTests.PREREQ_CLASS_NAME.equals(className) && null != syncObject) {
				ParallelClassLoadingTests.logMessage(myName, "waiting for other thread");
				synchronized (syncObject) {
					++numWaiting;
					ParallelClassLoadingTests.logMessage(myName, numWaiting + " waiting");
					syncObject.wait();
					--numWaiting;
				}
				ParallelClassLoadingTests.logMessage(myName, "continuing");
			}
			synchronized (this) {
				loadingPreq = true;
				if (loadingTarget && (null != childLdr)) {
					ParallelClassLoadingTests.logger.debug(preamble + " load prereq");
					Class<?> prereq = Class.forName(ParallelClassLoadingTests.PREREQ_CLASS_NAME, true, childLdr);
					ParallelClassLoadingTests.logger.debug(preamble + " loaded prereq");
				}
				ParallelClassLoadingTests.logMessage(myName, " delegating " + className);
				final Class<?> loadedClass = parent.loadClass(className);
				ParallelClassLoadingTests.logMessage(myName, " delegated " + className);
				return loadedClass;
			}
		} catch (Exception e) {
			ParallelClassLoadingTests.logger.error(Thread.currentThread().getName() + "/" + myName + ": Unexpected exception on " + className);
			e.printStackTrace();
			return null;
		}
	}
}
