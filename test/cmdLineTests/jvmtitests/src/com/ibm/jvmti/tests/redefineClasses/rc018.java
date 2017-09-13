/*******************************************************************************
 * Copyright (c) 2013, 2013 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.jvmti.tests.redefineClasses;

import com.ibm.jvmti.tests.util.Util;

public class rc018 {
	public static native boolean redefineClass(Class name, int classBytesSize,
			byte[] classBytes);

	class RefelctThread extends Thread {
		long patience = 20000000000L; /* 20 seconds in nanoseconds */

		RefelctThread() {
			super("Reflecting Thread");
		}

		public void run() {
			long startTime = System.nanoTime();
			try {
				while (((System.nanoTime() - startTime) < patience)) {
					rc018_testRedefAndReflect_O1.class.getDeclaredFields();
				}
			} catch (SecurityException e) {
				System.out.println("getDeclaredFields hit an security exception");
			}
		}

	}

	class populate extends Thread {
		long patience = 20000000000L; /* 20 seconds in nanoseconds */

		populate() {
			super("Populating java heap");
		}

		public void run() {
			long startTime = System.nanoTime();
			while (((System.nanoTime() - startTime) < patience)) {
				Object array[] = new Object[1000];
			}

		}
	}

	public boolean testReflectRedefineAtSameTime() {
		long patience = 20000000000L; /* 20 seconds in nanoseconds */

		RefelctThread worker1 = new RefelctThread();
		RefelctThread worker2 = new RefelctThread();
		RefelctThread worker3 = new RefelctThread();
		RefelctThread worker4 = new RefelctThread();
		RefelctThread worker5 = new RefelctThread();
		populate garbage = new populate();

		System.out.println("starting reflect worker thread");
		worker1.start();
		worker2.start();
		worker3.start();
		worker4.start();
		worker5.start();

		System.out.println("staring to pupulate java heap");
		garbage.start();

		long startTime = System.nanoTime();

		while (((System.nanoTime() - startTime) < patience)) {
			Util.redefineClass(getClass(), rc018_testRedefAndReflect_O1.class,
					rc018_testRedefAndReflect_O2.class);
		}

		try {
			worker1.join();
			worker2.join();
			worker3.join();
			worker4.join();
			worker5.join();
			garbage.join();
		} catch (InterruptedException e) {
			e.printStackTrace();
		}

		return true;

	}

	public String helpReflectRedefineAtSameTime() {
		return "Tests redefining a class while reflecting its fields";
	}
}
