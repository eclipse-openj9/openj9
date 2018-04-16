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

/**
 * Test case for Jazz 60282: Add option to disable classloader locking
 *
 */
public class TestVMClassloaderLocking {

	static public class BogusLoader extends ClassLoader {

		@Override
		public Class<?> loadClass(String className)
				throws ClassNotFoundException {
			return loadClass(className, true);
		}

		@Override
		protected Class<?> loadClass(String className,
				boolean resolveClass) throws ClassNotFoundException {
			try {
				ParallelClassLoadingTests.logMessage("BogusLoader", "loadClass start wait");
				this.wait(1);
				ParallelClassLoadingTests.logMessage("BogusLoader", "loadClass end wait");
			} catch (InterruptedException e) {
				ParallelClassLoadingTests.logMessage("BogusLoader", "Unexpected exception");
				e.printStackTrace();
			}
			return super.loadClass(className, resolveClass);
		}
	}

	/**
	 * This causes an IllegalMonitorStateException if the VM does not implicitly lock the classloader.
	 * Locking the classloader is the legacy behaviour of J9, but was changed for CMVC 193900 to enter the classloader's
	 * monitor during classloads.  In this case, we expect a ClassNotFoundException.
	 * Run with -XX:-VMLockClassLoader to disable the new behaviour
	 * Run with -XX:+VMLockClassLoader to force the new behaviour
	 * @param args Not used
	 */
	public static void main(String[] args) {
		BogusLoader myLoader = new BogusLoader();
		try {
			Class.forName("some.bogus.Classname", true, myLoader);
		} catch (ClassNotFoundException e) {
			ParallelClassLoadingTests.logMessage("main", "ClassNotFoundException");
		} catch (IllegalMonitorStateException e) {
			ParallelClassLoadingTests.logMessage("main", "IllegalMonitorStateException");
		}
	}
}
