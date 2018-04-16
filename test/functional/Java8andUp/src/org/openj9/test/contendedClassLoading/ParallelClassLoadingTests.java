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

import org.testng.annotations.Test;
import org.testng.annotations.BeforeMethod;
import org.testng.log4testng.Logger;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.Thread.State;
import java.lang.reflect.Method;

@Test(groups = { "level.extended" })
public class ParallelClassLoadingTests {

	public static final Logger logger = Logger.getLogger(ParallelClassLoadingTests.class);

	public final static String PREREQ_CLASS_NAME = "org.openj9.test.contendedClassLoading.TestClasses.PrerequisiteClass";
	public final static String TARGET_CLASS_NAME = "org.openj9.test.contendedClassLoading.TestClasses.TargetClass";
	final static String myName = "main";

	@Test
	public void testSequentialClassLoading() {
		try {
			Object syncObject = new Object();
			EmptyClassLoader ecl = new EmptyClassLoader(syncObject);
			LoadingThread lt1 = new LoadingThread(EmptyClassLoader.emptyClassName, ecl);
			lt1.setName("lt1");
			LoadingThread lt2 = new LoadingThread(EmptyClassLoader.emptyClassName, ecl);
			lt2.setName("lt2");
			runThread(syncObject, lt1);

			logMessage(myName, "Starting second thread");
			runThread(syncObject, lt2);
		} catch (LinkageError e) {
			e.printStackTrace();
			Assert.fail("Unexpected LinkageError");
		}
	}

	@Test(groups = { "testParallelClassLoading" })
	public void testParallelClassLoading() {
		Object syncObject = new Object();
		EmptyClassLoader ecl = new EmptyClassLoader(syncObject);
		LoadingThread lt1 = new LoadingThread(EmptyClassLoader.emptyClassName, ecl);
		lt1.setName("lt1");
		LoadingThread lt2 = new LoadingThread(EmptyClassLoader.emptyClassName, ecl);
		lt2.setName("lt2");

		runParallelClassLoading(syncObject, lt1, lt2);
	}

	@Test(dependsOnGroups = "testParallelClassLoading")
	public void testParallelCapableClassLoading() {
		Object syncObject = new Object();
		AssertJUnit.assertTrue("could not set parallel capable", EmptyClassLoader.setParallelCapable());
		EmptyClassLoader ecl = new EmptyClassLoader(syncObject);
		LoadingThread lt1 = new LoadingThread(EmptyClassLoader.emptyClassName, ecl);
		lt1.setName("lt1");
		lt1.setExpectLoadFail(false);
		LoadingThread lt2 = new LoadingThread(EmptyClassLoader.emptyClassName, ecl);
		lt2.setName("lt2");
		lt2.setExpectLoadFail(true);
		runParallelClassLoading(syncObject, lt1, lt2);
	}

	@Test
	public void testParallelClassLoadingWithFailure() {
		Object syncObject = new Object();
		EmptyClassLoader ecl = new EmptyClassLoader(syncObject);
		ecl.failFirstLoad = true;
		LoadingThread lt1 = new LoadingThread(EmptyClassLoader.emptyClassName, ecl);
		lt1.setName("lt1");
		lt1.setExpectLoadFail(true);
		LoadingThread lt2 = new LoadingThread(EmptyClassLoader.emptyClassName, ecl);
		lt2.setName("lt2");

		runParallelClassLoading(syncObject, lt1, lt2);
	}

	@Test
	public void testDelegatedClassLoading() {

		/* Set up class loaders */
		Object syncObject = new Object();
		DelegatingLoader parentLdr = new DelegatingLoader();
		parentLdr.setMyName("parentLdr");
		DelegatingLoader childLdr = new DelegatingLoader(parentLdr);
		parentLdr.childLdr = childLdr;
		childLdr.setMyName("childLdr");

		runMultiThreadClassLoading(syncObject, false, parentLdr, childLdr);
	}


	@Test
	public void testDelegatedNonParallelCapableClassLoading() {

		/* Set up class loaders */
		Object syncObject = new Object();
		DelegatingLoader parentLdr = new NonParallelCapableDelegatingLoader();
		parentLdr.setMyName("parentLdr");
		DelegatingLoader childLdr = new NonParallelCapableDelegatingLoader(parentLdr);
		parentLdr.childLdr = childLdr;
		childLdr.setMyName("childLdr");

		runMultiThreadClassLoading(syncObject, false, parentLdr, childLdr);
	}

	@Test
	public void testDelegatedParallelCapableClassLoading() {

		/* Set up class loaders */
		Object syncObject = new Object();
		ParallelCapableDelegatingLoader parentLdr = new ParallelCapableDelegatingLoader();
		AssertJUnit.assertTrue("class loader should be parallel capable", parentLdr.isParallelCapable());
		parentLdr.setMyName("parentLdr");
		ParallelCapableDelegatingLoader childLdr = new ParallelCapableDelegatingLoader(parentLdr);
		parentLdr.childLdr = childLdr;
		childLdr.setMyName("childLdr");

		runMultiThreadClassLoading(syncObject, true, parentLdr, childLdr);
	}

	private void runThread(Object syncObject, LoadingThread testThread) {
		testThread.start();
		for (int i = 0; i < 100; ++i) {
			State tState = testThread.getState();
			logMessage(myName, testThread.getName() + " State=" + tState);
			if (State.TERMINATED == tState) {
				break;
			}
			synchronized (syncObject) {
				syncObject.notifyAll();
			}
			try {
				testThread.join(100);
			} catch (InterruptedException e) {
				e.printStackTrace();
				Assert.fail("unexpected exception");
			}
		}
		checkState(testThread);
	}

	private void runParallelClassLoading(Object syncObject, LoadingThread lt1, LoadingThread lt2) {
		lt1.start();
		pollForThreadWaiting(lt1);

		lt2.start();
		boolean lt2InForNameImpl = false;
		int count = 0;
		for (int i = 0; (i < 100) && !lt2InForNameImpl; ++i) {
			StackTraceElement[] trc = lt2.getStackTrace();
			logger.debug("checking lt2's current location");
			try {
				Thread.sleep(100);
			} catch (InterruptedException e) {
				Assert.fail("unexpected exception " + e, e);
			}
			count = 0;
			for (StackTraceElement t : trc) {
				if (t.getMethodName().equals("forName")) { /* ensure it's trying to do the class load */
					logger.debug("lt2 is trying to do the class loasing and the count is " + count++);
					lt2InForNameImpl = true;
					break;
				}
			}
		}
		for (int i = 0; i < 100; ++i) {
			State lt1State = lt1.getState();
			State lt2State = lt2.getState();
			if ((State.TERMINATED != lt1State) || (State.TERMINATED != lt2State)) {
				logMessage(myName, "lt1State=" + lt1State + ", lt2State=" + lt2State);
				synchronized (syncObject) {
					syncObject.notify();
				}
				try {
					Thread.sleep(100);
				} catch (InterruptedException e) {
					e.printStackTrace();
					Assert.fail("unexpected exception");
				}
			} else {
				logMessage(myName, "threads terminated");
				break;
			}
		}
		checkClassLoadStatus(lt1);
		checkClassLoadStatus(lt2);
	}

	private void checkClassLoadStatus(LoadingThread testThread) {
		try {
			testThread.join();
		} catch (InterruptedException e) {
			e.printStackTrace();
			Assert.fail("unexpected exception");
		}
		if (testThread.expectLoadFail) {
			AssertJUnit.assertFalse(testThread.getName() + " did not fail to load class", testThread.loadedCorrectly);
		} else {
			AssertJUnit.assertTrue(testThread.getName() + " failed to load class", testThread.loadedCorrectly);
		}
	}

	private void runMultiThreadClassLoading(Object syncObject, boolean bothBlocked, DelegatingLoader parentLdr, DelegatingLoader childLdr) {
		/* set up loading threads */
		LoadingThread t1 = new LoadingThread(PREREQ_CLASS_NAME, childLdr);
		t1.setDaemon(true); /* don't block shutdown */
		t1.setName("t1");
		childLdr.setSyncObject(syncObject);
		LoadingThread t2 = new LoadingThread(TARGET_CLASS_NAME, childLdr);
		t2.setDaemon(true); /* don't block shutdown */
		t2.setName("t2");
		int numWaiting = 0;

		t1.start();
		pollForThreadWaiting(t1);
		t2.start();
		State tState;
		do { /* wait until Thread 2 is blocked */
			try {
				Thread.sleep(100);
			} catch (InterruptedException e) {
				e.printStackTrace();
				Assert.fail("unexpected exception");
			}
			tState = t2.getState();
			numWaiting = childLdr.numWaiting;
			logMessage(myName, "t2 state=" + tState.toString());
			logMessage(myName, "waiting for t2 to try loading prereq");
		} while ((bothBlocked && (2 != numWaiting)) || /* for parallel capable tests we want both threads blocked. */
				((State.BLOCKED != tState) && !parentLdr.loadingPreq));
		synchronized (syncObject) {
			syncObject.notifyAll();
		}
		logMessage(myName, "re-enable t1");

		checkState(t1);
		checkState(t2);
	}

	private void pollForThreadWaiting(LoadingThread lt1) {
		State tState;

		do {
			try {
				Thread.sleep(100);
			} catch (InterruptedException e) {
				e.printStackTrace();
				Assert.fail("unexpected exception");
			}
			tState = lt1.getState();
			logMessage(myName, "thread state=" + tState.toString());
		} while (State.WAITING != tState);
	}

	private void checkState(LoadingThread thr) {
		try {
			logMessage(myName, "waiting for " + thr.getName());
			thr.join(10000);
			if (thr.getState() != Thread.State.TERMINATED) {
				com.ibm.jvm.Dump.JavaDump();
				com.ibm.jvm.Dump.SystemDump();
				Assert.fail("Deadlock detected");
			}
			AssertJUnit.assertTrue(thr.getName() + " failed to load class", thr.loadedCorrectly);
		} catch (InterruptedException e) {
			Assert.fail("unexpected exception");
		}
	}

	static void logMessage(String myName, String msg) {
		final String threadName = Thread.currentThread().getName();
		final String preamble = threadName + "/" + myName + ": ";
		logger.debug(preamble + msg);
	}

	@BeforeMethod
	protected void setUp(Method method) throws Exception {
		logger.debug("\n-------------------------------------------------------------------------------\n");
		logMessage(myName, method.getName());
	}
}
