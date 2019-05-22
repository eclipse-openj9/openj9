/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
package org.openj9.test.java.lang;

import org.testng.annotations.Test;
import org.openj9.test.support.Support_ExtendedTestEnvironment;
import org.testng.Assert;
import org.testng.AssertJUnit;

@Test(groups = { "level.sanity" })
public class Test_Object {

	/**
	 * Test objects.
	 */
	Object obj1 = new Object();
	Object obj2 = new Object();
	/**
	 * Generic state indicator.
	 */
	volatile int status = 0;
	volatile int ready = 0;

	/**
	 * The static variables below are used for the testNotifyAndInterrupt()
	 * method
	 */
	public volatile static int errorCount; // number of errors
	public static final int NSLAVES = 20; // number of slaves created
	static int slaveEnterCount; // number of slaves in run method
	static Object MS = new Object(); // sync with master thead
	static Object MM = new Object(); // syn with slave threads
	static int notifyCount; // count of notify() calls
	static int interruptedCount; // count of interrupt() calls
	static int processedNotifyCount; // current processed notify count
	static int processedInterruptedCount; // current processed interrupt count
	static boolean notifyallSent; // master sent notifyAll()
	static volatile boolean slavesCompleted;// slaves completed ok

	/**
	 * The static variables below are used by the testNotify2 method
	 */
	// Tunable input parameters - teste are the defaults
	public static int NSLAVES2 = 2000; // number of slaves created
	public static int MODULO_NOTIFY = 5; // notifyall every n'th time
	public static int MASTER_SLEEP_SECS = 10;
	public static int WAIT_ON_ERROR = 0;
	public volatile static int errorCount2; // number of errors
	static int slaveEnterCount2; // number of slaves in run method
	static int slaveWakeCount; // number of threads awakened
	static Object MS2 = new Object(); // sync with master thead
	static Object MM2 = new Object(); // syn with slave threads
	static int notifyCount2; // count of notify() calls
	static int slaveExitCount; // slaves completed ok

	/**
	 * The static variables below are used by the testNotify3 method
	 */
	// Tunable input parameters - teste are the defaults
	public static int MASTER_SLEEP_SECS3 = 1;
	public static int maxPrio = 38; // <--- These 3 variables
	public static int minPrio = 12; // <--- are also used by
	public static int NSLAVES3 = maxPrio - minPrio + 1; // <--- testNotify4
	public volatile static int errorCount3; // number of errors
	static int slaveEnterCount3; // number of slaves in run method
	static Object MS3 = new Object(); // sync with master thead
	static Object MM3 = new Object(); // syn with slave threads
	static int notifyCount3; // count of notify() calls
	static int slaveExitCount3; // slaves completed ok

	/**
	 * The static variables below are used by the testNotify4 method
	 */
	// Tunable input parameters - teste are the defaults
	public static int MASTER_SLEEP_SECS4 = 1;
	public volatile static int errorCount4; // number of errors
	static int slaveEnterCount4; // number of slaves in run method
	static Object MS4 = new Object(); // sync with master thead
	static Object MM4 = new Object(); // syn with slave threads
	static int notifyCount4; // count of notify() calls
	static int slaveExitCount4; // slaves completed ok

	/**
	 * The static variables below are used by the testNotify5 method
	 */
	public static int MASTER_SLEEP_SECS5 = 1;
	public static int N = 4; // set to how vary the priority of threads in wait
	public volatile static int errorCount5; // number of errors
	static int slaveEnterCount5; // number of slaves in run method
	static Object MS5 = new Object(); // sync with master thead
	static Object MM5 = new Object(); // syn with slave threads
	static int notifyCount5; // count of notify() calls
	static int slaveExitCount5; // slaves completed ok

	/**
	 * @tests java.lang.Object#Object()
	 */
	@Test
	public void test_Constructor() {
		AssertJUnit.assertTrue("Constructor failed !!!", new Object() != null);
	}

	/**
	 * @tests java.lang.Object#equals(java.lang.Object)
	 */
	@Test
	public void test_equals() {
		AssertJUnit.assertTrue("Same object should be equal", obj1.equals(obj1));
		AssertJUnit.assertTrue("Different objects should not be equal", !obj1.equals(obj2));
	}

	/**
	 * @tests java.lang.Object#getClass()
	 */
	@Test
	public void test_getClass() {
		String classNames[] = { "java.lang.Object", "java.lang.Throwable", "java.lang.StringBuffer" };
		Class classToTest = null;
		Object instanceToTest = null;

		status = 0;
		for (int i = 0; i < classNames.length; ++i) {
			try {
				classToTest = Class.forName(classNames[i]);
				instanceToTest = classToTest.newInstance();
				AssertJUnit.assertTrue("Instance didn't match creator class.",
						instanceToTest.getClass() == classToTest);
				AssertJUnit.assertTrue("Instance didn't match class with matching name.",
						instanceToTest.getClass() == Class.forName(classNames[i]));
			} catch (Exception ex) {
				AssertJUnit.assertTrue("Unexpected exception (" + ex + ").", false);
			}
		}
	}

	/**
	 * @tests java.lang.Object#hashCode()
	 */
	@Test
	public void test_hashCode() {
		AssertJUnit.assertTrue("Same object should have same hash.", obj1.hashCode() == obj1.hashCode());
		AssertJUnit.assertTrue("Same object should have same hash.", obj2.hashCode() == obj2.hashCode());
	}

	/**
	 * @tests java.lang.Object#notify()
	 */
	@Test
	public void test_notify() {
		// Inner class to run test thread.
		class TestThread implements Runnable {
			public void run() {
				try {
					synchronized (obj1) {
						ready += 1;
						synchronized (obj2) {
							obj2.notify();
						}
						obj1.wait();// Wait for ever.
					}
					// only one thread wakes up at a time so incrementing status doesn't need to be synchronized
					// status is declared volatile so can be accessed from other threads
					status += 1;
					synchronized (obj2) {
						obj2.notify();
					}
				} catch (InterruptedException ex) {
					status = -1000;
				}
			}
		}
		;

		// Start of test code.

		ready = 0;
		status = 0;

		final int threadCount = 20;
		for (int i = 0; i < threadCount; ++i) {
			synchronized (obj2) {
				Support_ExtendedTestEnvironment.getInstance().getThread(new TestThread()).start();
				try {
					obj2.wait(10000);
				} catch (InterruptedException ex) {
					Assert.fail("Unexpectedly got an InterruptedException initializing thread " + i);
				}
			}
		}

		try {

			// after synchronizing on obj1, we are guaranteed all threads are waiting
			synchronized (obj1) {
				// Check pre-conditions of testing notifyAll
				AssertJUnit.assertTrue("Not all launched threads are waiting. (ready = " + ready + ")",
						ready == threadCount);
				AssertJUnit.assertTrue("Thread woke too early. (status = " + status + ")", status == 0);
			}

			for (int i = 1; i <= threadCount; ++i) {
				synchronized (obj2) {
					synchronized (obj1) {
						obj1.notify();
					}
					obj2.wait(10000);
				}
				AssertJUnit.assertTrue("Out of sync. (expected " + i + " but got " + status + ")", status == i);
			}

		} catch (InterruptedException ex) {
			Assert.fail("Unexpectedly got an InterruptedException. (status = " + status + ")");
		}

	}

	/**
	 * @tests java.lang.Object#notifyAll()
	 */
	@Test
	public void test_notifyAll() {
		// Inner class to run test thread.
		class TestThread implements Runnable {
			Object lock;

			TestThread(Object lock) {
				this.lock = lock;
			}

			public void run() {
				try {
					synchronized (obj1) {
						ready += 1;
						synchronized (obj2) {
							obj2.notify();
						}
						obj1.wait();// Wait for ever.
						// all threads wake up and modify status, so the modification of status must be synchronized
						// status is declared volatile so can be accessed from other threads
						status += 1;
					}
					synchronized (lock) {
						lock.notify();
					}
				} catch (InterruptedException ex) {
					status = -1000;
				}
			}
		}

		// Start of test code.

		status = 0;
		ready = 0;
		final int threadCount = 20;
		Object[] locks = new Object[threadCount];
		for (int i = 0; i < threadCount; ++i) {
			locks[i] = new Object();
			synchronized (obj2) {
				Support_ExtendedTestEnvironment.getInstance().getThread(new TestThread(locks[i])).start();
				try {
					obj2.wait(10000);
				} catch (InterruptedException ex) {
					Assert.fail("Unexpectedly got an InterruptedException initializing thread " + i);
				}
			}
		}

		try {

			// after synchronizing on obj1, we are guaranteed all threads are waiting
			synchronized (obj1) {
				// Check pre-conditions of testing notifyAll
				AssertJUnit.assertTrue("Not all launched threads are waiting. (ready = " + ready + ")",
						ready == threadCount);
				AssertJUnit.assertTrue("At least one thread woke too early. (status = " + status + ")", status == 0);
			}

			synchronized (locks[0]) {
				synchronized (locks[1]) {
					synchronized (locks[2]) {
						synchronized (locks[3]) {
							synchronized (locks[4]) {
								synchronized (locks[5]) {
									synchronized (locks[6]) {
										synchronized (locks[7]) {
											synchronized (locks[8]) {
												synchronized (locks[9]) {
													synchronized (locks[10]) {
														synchronized (locks[11]) {
															synchronized (locks[12]) {
																synchronized (locks[13]) {
																	synchronized (locks[14]) {
																		synchronized (locks[15]) {
																			synchronized (locks[16]) {
																				synchronized (locks[17]) {
																					synchronized (locks[18]) {
																						synchronized (locks[19]) {
																							synchronized (obj1) {
																								obj1.notifyAll();
																							}

																							for (int i = 1; i <= threadCount; ++i) {
																								locks[i - 1]
																										.wait(10000);
																							}
																						}
																					}
																				}
																			}
																		}
																	}
																}
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}

			AssertJUnit.assertTrue("At least one thread did not get notified. (status = " + status + ")",
					status == threadCount);

		} catch (InterruptedException ex) {
			Assert.fail("Unexpectedly got an InterruptedException. (status = " + status + ")");
		}
	}

	/**
	 * @tests java.lang.Object#toString()
	 */
	@Test
	public void test_toString() {
		AssertJUnit.assertTrue("Object toString returned null.", obj1.toString() != null);
	}

	/**
	 * @tests java.lang.Object#wait()
	 */
	@Test
	public void test_wait() {
		// Inner class to run test thread.
		class TestThread implements Runnable {
			public void run() {
				try {
					synchronized (obj1) {
						synchronized (obj2) {
							obj2.notify();
						}
						obj1.wait();// Wait for ever.
						status = 1;
						obj1.notify();
					}
				} catch (InterruptedException ex) {
					status = -1;
				}
			}
		}
		;

		// Start of test code.

		// Warning:
		// This code relies on threads getting serviced within
		// 1 second of when they are notified. Although this
		// seems reasonable, it could lead to false-failures.

		status = 0;
		synchronized (obj2) {
			Support_ExtendedTestEnvironment.getInstance().getThread(new TestThread()).start();
			try {
				obj2.wait(10000);
			} catch (InterruptedException ex) {
				Assert.fail("Unexpectedly got an InterruptedException. (status = " + status + ")");
			}
		}

		synchronized (obj1) {
			try {
				AssertJUnit.assertTrue("Thread woke too early. (status = " + status + ")", status == 0);
				obj1.notifyAll();
				obj1.wait(10000);
				AssertJUnit.assertTrue("Thread did not get notified. (status = " + status + ")", status == 1);
			} catch (InterruptedException ex) {
				Assert.fail("Unexpectedly got an InterruptedException. (status = " + status + ")");
			}
		}
	}

	/**
	 * @tests java.lang.Object#wait(long)
	 */
	@Test
	public void test_waitJ() {
		// Inner class to run test thread.
		class TestThread implements Runnable {
			public void run() {
				try {
					synchronized (obj1) {
						synchronized (obj2) {
							obj2.notify();
						}
						obj1.wait();
						obj1.wait(1); // Don't wait very long.
						status = 1;
						obj1.notify();
						obj1.wait(0); // Wait for ever.
						status = 2;
						obj1.notify();
					}
				} catch (InterruptedException ex) {
					status = -1;
				}
			}
		}
		;

		// Start of test code.

		status = 0;
		synchronized (obj2) {
			Support_ExtendedTestEnvironment.getInstance().getThread(new TestThread()).start();
			try {
				obj2.wait(10000);
			} catch (InterruptedException ex) {
				Assert.fail("Unexpectedly got an InterruptedException. (status = " + status + ")");
			}
		}
		synchronized (obj1) {
			try {
				obj1.notify();
				obj1.wait();
				AssertJUnit.assertTrue("Thread did not wake after 1 ms. (status = " + status + ")", status == 1);
				obj1.notifyAll();
				obj1.wait();
				AssertJUnit.assertTrue("Thread did not get notified. (status = " + status + ")", status == 2);
			} catch (InterruptedException ex) {
				Assert.fail("Unexpectedly got an InterruptedException. (status = " + status + ")");
			}
		}
	}

	/**
	 * @tests java.lang.Object#wait(long, int)
	 */
	@Test
	public void test_wait2() {
		// Inner class to run test thread.
		class TestThread implements Runnable {
			public void run() {
				try {
					synchronized (obj1) {
						synchronized (obj2) {
							obj2.notify();
						}
						obj1.wait();
						obj1.wait(0, 1); // Don't wait very long.
						status = 1;
						obj1.notify();
						obj1.wait(0, 0); // Wait for ever.
						status = 2;
						obj1.notify();
					}
				} catch (InterruptedException ex) {
					status = -1;
				}
			}
		}
		;

		// Start of test code.

		status = 0;
		synchronized (obj2) {
			Support_ExtendedTestEnvironment.getInstance().getThread(new TestThread()).start();
			try {
				obj2.wait(10000);
			} catch (InterruptedException ex) {
				Assert.fail("Unexpectedly got an InterruptedException. (status = " + status + ")");
			}
		}
		synchronized (obj1) {
			try {
				obj1.notify();
				obj1.wait();
				AssertJUnit.assertTrue("Thread did not wake after 1 ns. (status = " + status + ")", status == 1);
				obj1.notifyAll();
				obj1.wait();
				AssertJUnit.assertTrue("Thread did not get notified. (status = " + status + ")", status == 2);
			} catch (InterruptedException ex) {
				Assert.fail("Unexpectedly got an InterruptedException. (status = " + status + ")");
			}
		}
	}

	/**
	 * @tests java.lang.Object#clone()
	 */
	@Test
	public void test_clone() {
		// Test failure case
		try {
			this.clone();
			Assert.fail("Able to clone uncloneable object");
		} catch (CloneNotSupportedException t) {
		} catch (Throwable t) {
			Assert.fail("Unexpected Throwable during failed clone");
		}

		// Test object case
		try {
			class Cloney implements Cloneable {
				public int a;
				public String b;
				public long c;

				public Cloney(int aa, String bb, long cc) {
					a = aa;
					b = bb;
					c = cc;
				}

				public Object cloneMe() throws CloneNotSupportedException {
					return clone();
				}
			}
			;
			Cloney original = new Cloney(47, "argle", 123782173821738787L);
			Cloney copy = (Cloney)original.cloneMe();
			if (original.getClass() != copy.getClass()) {
				Assert.fail("Object class not cloned correctly");
			}
		} catch (Throwable t) {
			Assert.fail("Unable to clone object");
		}

		// Test object array case
		try {
			String[] original = { "a", "b", "c" };
			String[] copy = (String[])original.clone();
			if (original.getClass() != copy.getClass()) {
				Assert.fail("Object array class not cloned correctly");
			}
			for (int i = 0; i < original.length; ++i) {
				if (original[i] != copy[i]) {
					Assert.fail("Object element at " + i + " does not match");
				}
			}
		} catch (Throwable t) {
			Assert.fail("Unable to clone object array");
		}

		// Test primitive array case
		try {
			int[] original = { -1, 42, 2367272 };
			int[] copy = (int[])original.clone();
			if (original.getClass() != copy.getClass()) {
				Assert.fail("Primitive array class not cloned correctly");
			}
			for (int i = 0; i < original.length; ++i) {
				if (original[i] != copy[i]) {
					Assert.fail("Primitive element at " + i + " does not match");
				}
			}
		} catch (Throwable t) {
			Assert.fail("Unable to clone primitive array");
		}
	}

}
