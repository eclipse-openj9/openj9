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
package com.ibm.jvmti.tests.redefineClasses;

import java.util.concurrent.Semaphore;

import com.ibm.jvmti.tests.util.Util;

public class rc012 {
	public static native boolean redefineClass(Class name, int classBytesSize, byte[] classBytes);

	public boolean setup(String args) {
		return true;
	}

	public boolean testRedefineRunningMethod() throws InterruptedException {
		final Semaphore startSem = new Semaphore(0);
		final Semaphore goRecursiveSem = new Semaphore(0);
		final rc012_testRedefineRunningMethod_O1 instance = new rc012_testRedefineRunningMethod_O1(startSem, goRecursiveSem);
		final String[] result = new String[1];

		Thread helperThread = new Thread() {
			public void run() {
				try {
					result[0] = instance.doIt(true);
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
			}
		};

		// Start the thread. It will block in doIt() until we raise the semaphore
		System.out.println("Calling helperThread.start()");
		helperThread.start();

		// Wait for doIt() to start.
		System.out.println("Calling startSem.acquire()");
		startSem.acquire();

		// Redefine the class.
		System.out.println("Redefining class");
		boolean redefined = Util.redefineClass(getClass(), rc012_testRedefineRunningMethod_O1.class, rc012_testRedefineRunningMethod_R1.class);
		if (!redefined) {
			return false;
		}

		// Tell the helper thread to call a method in the redefined class
		System.out.println("Calling goRecursiveSem.release");
		goRecursiveSem.release();

		helperThread.join();

		if (!"noyes".equals(result[0])) {
			System.out.println("Expected = noyes");
			System.out.println("Got = " + result[0]);
			return false;
		}

		return true;
	}

	public String helpRedefineRunningMethod() {
		return "Tests that redefining a running method works as stated in the spec.";
	}

	public boolean testRedefineRunningNativeMethod() throws InterruptedException 
	{
		final Semaphore startSem = new Semaphore(0);
		final Semaphore redefineSem = new Semaphore(0);
		final rc012_testRedefineRunningNativeMethod_O1 instance = new rc012_testRedefineRunningNativeMethod_O1(startSem, redefineSem);
		final String[] result = new String[1];

	
		result[0] = instance.meth2();
		if (!"before".equals(result[0])) {
			System.out.println("Expected = before");
			System.out.println("Got = " + result[0]);
			return false;
		}

		Thread helperThread = new Thread() {
			public void run() {
				result[0] = instance.meth1();
			}
		};

		// Start the thread. 
		System.out.println("Calling helperThread.start()");
		helperThread.start();

		// wait for the thread to start and get to a point where we can start redefining. 
		startSem.acquire();
		
		// Redefine the class.
		System.out.println("Redefining class");
		boolean redefined = Util.redefineClass(getClass(), rc012_testRedefineRunningNativeMethod_O1.class, rc012_testRedefineRunningNativeMethod_R1.class);
		if (!redefined) {
			return false;
		}

		// Tell the helper thread to go and call meth2() in the redefined class
		System.out.println("Calling redefineSem.release()");
		redefineSem.release();

		helperThread.join();

		if (!"after".equals(result[0])) {
			System.out.println("Expected = after");
			System.out.println("Got = " + result[0]);
			return false;
		}

		return true;
	}

	public String helpRedefineRunningNativeMethod() {
		return "Tests redefining a running native method.";
	}

	public boolean testRedefineManyRunningMethods() throws InterruptedException {
		final Semaphore sem = new Semaphore(0);
		final rc012_testRedefineManyRunningMethods_O1 instance = new rc012_testRedefineManyRunningMethods_O1(sem);
		final String[] result = new String[1];

		Thread helperThread = new Thread() {
			public void run() {
				result[0] = instance.run();
				sem.release(1);
			}
		};

		// Start the thread. It will block in doIt() until we give a permit to semaphore.
		System.out.println("Calling helperThread.start()");
		helperThread.start();

		// Wait for run() to be called by acquiring on the semaphore.
		System.out.println("Calling sem.acquire(1)");
		sem.acquire(1);

		// Redefine the class.
		System.out.println("Redefining class");
		boolean redefined = Util.redefineClass(getClass(), rc012_testRedefineManyRunningMethods_O1.class, rc012_testRedefineManyRunningMethods_R1.class);
		if (!redefined) {
			return false;
		}

		// Wait for run() to complete by acquiring on the semaphore.
		System.out.println("Calling sem.acquire(1)");
		sem.acquire(1);

		if (!"old".equals(result[0])) {
			System.out.println("Expected = old");
			System.out.println("Got = " + result[0]);
			return false;
		}

		return true;
	}

	public String helpRedefineManyRunningMethods() {
		return "Tests that redefining a running method works as stated in the spec.";
	}
}
