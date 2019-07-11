/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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

import org.testng.annotations.AfterMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeMethod;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.util.Vector;

@Test(groups = { "level.sanity" })
public class Test_ThreadGroup {
	private static final Logger logger = Logger.getLogger(Test_ThreadGroup.class);

	class MyThread extends Thread {
		// this class does not exist in xtrem
		public volatile long heartBeat = 0;
		public Object lock;
		public volatile boolean resumeRequested = false;
		public volatile boolean resumed = false;

		public MyThread(ThreadGroup group, String name) throws SecurityException, IllegalThreadStateException {
			super(group, name);
		}

		public void setLock(Object obj) {
			lock = obj;
		}

		public void resumeRequested() {
			resumeRequested = true;
		}

		public boolean isResumed() {
			return resumed;
		}

		public void run() {
			if (lock == null)
				lock = new Object();
			while (true) {
				try {
					if (resumeRequested) {
						resumed = true;
						synchronized (lock) {
							lock.notify();
							/*We wait here for two reasons. One is to release the lock and
							 * second is to give some break for this Thread which is in a while(true) loop
							 */
							lock.wait(50);
						}

					} else
						Thread.sleep(50);
				} catch (InterruptedException e) {

				}
				heartBeat++;
			}
		}

		public boolean isActivelyRunning() {
			long MAX_WAIT = 100;
			return isActivelyRunning(MAX_WAIT);
		}

		public boolean isActivelyRunning(long maxWait) {
			long beat = heartBeat;
			long start = System.currentTimeMillis();
			do {
				Thread.yield();
				long beat2 = heartBeat;
				if (beat != beat2)
					return true;
			} while (System.currentTimeMillis() - start < maxWait);
			return false;
		}

	}

	private ThreadGroup rootThreadGroup = null;
	private ThreadGroup initialThreadGroup = null;

	/**
	 * @tests java.lang.ThreadGroup#ThreadGroup(java.lang.String)
	 */
	@Test
	public void test_remove() {
		/*
		 * [PR CMVC 114880] ThreadGroup is not notified when all threads
		 * complete
		 */
		ThreadGroup testGroup1 = new ThreadGroup("test_remove ThreadGroup");
		Thread testThread1 = new Thread(testGroup1, "test_remove Thread");
		synchronized (testGroup1) {
			testThread1.start();
			try {
				testGroup1.wait();
			} catch (InterruptedException e) {
				Assert.fail("Unexpected: " + e);
			}
		}
		testGroup1.destroy();

	}

	/**
	 * @tests java.lang.ThreadGroup#ThreadGroup(java.lang.String)
	 */
	@Test
	public void test_Constructor() {
		// Unfortunately we have to use other APIs as well as we test the
		// constructor
		ThreadGroup newGroup = null;
		ThreadGroup initial = getInitialThreadGroup();
		final String name = "Test name";
		newGroup = new ThreadGroup(name);
		AssertJUnit.assertTrue("Has to be possible to create a subgroup of current group using simple constructor",
				newGroup.getParent() == initial);
		AssertJUnit.assertTrue("Name has to be correct", newGroup.getName().equals(name));
		// cleanup
		newGroup.destroy();
	}

	/**
	 * @tests java.lang.ThreadGroup#ThreadGroup(java.lang.ThreadGroup,
	 *        java.lang.String)
	 */
	@Test
	public void test_Constructor2() {
		// Unfortunately we have to use other APIs as well as we test the
		// constructor

		ThreadGroup newGroup = null;
		try {
			newGroup = new ThreadGroup(null, null);
		} catch (NullPointerException e) {
		}
		AssertJUnit.assertTrue("Can't create a ThreadGroup with a null parent", newGroup == null);

		newGroup = new ThreadGroup(getInitialThreadGroup(), null);
		AssertJUnit.assertTrue("Has to be possible to create a subgroup of current group",
				newGroup.getParent() == Thread.currentThread().getThreadGroup());

		// Lets start all over
		newGroup.destroy();

		newGroup = new ThreadGroup(getRootThreadGroup(), "a name here");
		AssertJUnit.assertTrue("Has to be possible to create a subgroup of root group",
				newGroup.getParent() == getRootThreadGroup());

		// Lets start all over
		newGroup.destroy();

		try {
			newGroup = new ThreadGroup(newGroup, "a name here");
		} catch (IllegalThreadStateException e) {
			newGroup = null;
		}
		;
		AssertJUnit.assertTrue("Can't create a subgroup of a destroyed group", newGroup == null);
	}

	/**
	 * @tests java.lang.ThreadGroup#activeCount()
	 */
	@Test
	public void test_activeCount() {
		ThreadGroup tg = new ThreadGroup("activeCount");
		tg.setDaemon(true);
		Thread t1 = new Thread(tg, new Runnable() {
			public void run() {
				try {
					Thread.sleep(5000);
				} catch (InterruptedException e) {
				}
			}
		});
		int count = tg.activeCount();
		/*
		 * [PR 115667, CMVC 93001] activeCount() changed in 1.5, CDC/Foundation
		 * 1.1 to only count active threads
		 */
		AssertJUnit.assertTrue("wrong active count: " + count, count == 0);
		t1.start();
		count = tg.activeCount();
		AssertJUnit.assertTrue("wrong active count: " + count, count == 1);
		t1.interrupt();
		try {
			t1.join();
		} catch (InterruptedException e) {
		}
		AssertJUnit.assertTrue("group not destroyed", tg.isDestroyed());
	}

	/**
	 * @tests java.lang.ThreadGroup#checkAccess()
	 */
	@Test
	public void test_checkAccess() {
		final ThreadGroup originalCurrent = getInitialThreadGroup();
		ThreadGroup testRoot = new ThreadGroup(originalCurrent, "Test group");

		SecurityManager currentManager = System.getSecurityManager();
		boolean passed = true;

		try {
			if (currentManager != null)
				testRoot.checkAccess();
		} catch (SecurityException se) {
			passed = false;
		}
		;

		AssertJUnit.assertTrue("CheckAccess is no-op with no Securitymanager", passed);

		testRoot.destroy();

	}

	/**
	 * @tests java.lang.ThreadGroup#destroy()
	 */
	@Test
	public void test_destroy() {
		final ThreadGroup originalCurrent = getInitialThreadGroup();
		ThreadGroup testRoot = new ThreadGroup(originalCurrent, "Test group");
		final int DEPTH = 4;
		final Vector subgroups = buildRandomTreeUnder(testRoot, DEPTH);

		// destroy them all
		testRoot.destroy();

		for (int i = 0; i < subgroups.size(); i++) {
			ThreadGroup child = (ThreadGroup)subgroups.elementAt(i);
			AssertJUnit.assertTrue("Destroyed child can't have children", child.activeCount() == 0);
			boolean passed = false;
			try {
				child.destroy();
			} catch (IllegalThreadStateException e) {
				passed = true;
			}
			;
			AssertJUnit.assertTrue("Destroyed child can't be destroyed again", passed);
		}

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// - - - - - - - - - - -

		testRoot = new ThreadGroup(originalCurrent, "Test group (daemon)");
		testRoot.setDaemon(true);

		ThreadGroup child = new ThreadGroup(testRoot, "daemon child");

		// If we destroy the last daemon's child, the daemon should ge destroyed
		// as well
		child.destroy();

		boolean passed = false;
		try {
			child.destroy();
		} catch (IllegalThreadStateException e) {
			passed = true;
		}
		;
		AssertJUnit.assertTrue("Daemon should have been destroyed already", passed);

		passed = false;
		try {
			testRoot.destroy();
		} catch (IllegalThreadStateException e) {
			passed = true;
		}
		;
		AssertJUnit.assertTrue("Daemon parent should have been destroyed automatically", passed);

		AssertJUnit.assertTrue("Destroyed daemon's child should not be in daemon's list anymore",
				!arrayIncludes(groups(testRoot), child));
		AssertJUnit.assertTrue("Destroyed daemon should not be in parent's list anymore",
				!arrayIncludes(groups(originalCurrent), testRoot));

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// - - - - - - - - - - -

		testRoot = new ThreadGroup(originalCurrent, "Test group (daemon)");
		testRoot.setDaemon(true);
		Thread noOp = new Thread(testRoot, null, "no-op thread") {
			public void run() {
			}
		};
		noOp.start();

		// Wait for the no-op thread to run inside daemon ThreadGroup
		try {
			noOp.join();
		} catch (InterruptedException ie) {
			AssertJUnit.assertTrue("Should not be interrupted", false);
		}
		;

		passed = false;
		try {
			child.destroy();
		} catch (IllegalThreadStateException e) {
			passed = true;
		}
		;
		AssertJUnit.assertTrue("Daemon group should have been destroyed already when last thread died", passed);

		testRoot = new ThreadGroup(originalCurrent, "Test group (daemon)");
		noOp = new Thread(testRoot, null, "no-op thread") {
			public void run() {
				try {
					Thread.sleep(500);
				} catch (InterruptedException ie) {
					// We have our own assert because of 1ES3UGD
					myassertTrue("Should not be interrupted", false);
				}
				;
			}
		};

		// Has to execute the next lines in an interval < the sleep interval of
		// the no-op thread
		noOp.start();
		passed = false;
		try {
			testRoot.destroy();
		} catch (IllegalThreadStateException its) {
			passed = true;
		}
		AssertJUnit.assertTrue("Can't destroy a ThreadGroup that has threads", passed);

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// - - - - - - - - - - -

		// But after the thread dies, we have to be able to destroy the thread
		// group
		try {
			noOp.join();
		} catch (InterruptedException ie) {
			AssertJUnit.assertTrue("Should not be interrupted", false);
		}
		;
		passed = true;
		try {
			testRoot.destroy();
		} catch (IllegalThreadStateException its) {
			passed = false;
		}
		AssertJUnit.assertTrue("Should be able to destroy a ThreadGroup that has no threads", passed);

	}

	/**
	 * @tests java.lang.ThreadGroup#destroy()
	 */
	@Test
	public void test_destroy2() {
		/*
		 * [PR 115667, CMVC 94448] check if ThreadGroup destroyed when creating
		 * Thread
		 */
		ThreadGroup group1 = new ThreadGroup("test_destroy_subtest0");
		group1.destroy();
		try {
			Thread t = new Thread(group1, "test_destroy_subtest0");
			Assert.fail("should throw IllegalThreadStateException");
		} catch (IllegalThreadStateException e) {
		}
	}

	/**
	 * @tests java.lang.ThreadGroup#destroy()
	 */
	@Test
	public void test_destroy3() {
		/* [PR CMVC 137999] Deadlock in threadGroup */
		final Object lock = new Object();
		final boolean[] done = new boolean[1];
		ThreadGroup activeCountGroup = new ThreadGroup("activeCount Group");
		Thread activeThread = new Thread(activeCountGroup, new Runnable() {
			public void run() {
				for (int i = 0; i < 10000; i++) {
					Thread.currentThread().getThreadGroup().activeCount();
				}
				synchronized (lock) {
					done[0] = true;
				}
			}
		});
		activeThread.start();

		for (int i = 0; i < 10000; i++) {
			new ThreadGroup(activeCountGroup, "destroy Group").destroy();
			synchronized (lock) {
				if (done[0])
					break;
			}
		}

		try {
			activeThread.join();
		} catch (InterruptedException e) {
			Assert.fail("unexpected: " + e);
		}
		activeCountGroup.destroy();
	}

	/**
	 * @tests java.lang.ThreadGroup#getMaxPriority()
	 */
	@Test
	public void test_getMaxPriority() {
		final ThreadGroup originalCurrent = getInitialThreadGroup();
		ThreadGroup testRoot = new ThreadGroup(originalCurrent, "Test group");

		boolean passed = true;
		passed = true;
		try {
			testRoot.setMaxPriority(Thread.MIN_PRIORITY);
		} catch (IllegalArgumentException iae) {
			passed = false;
		}
		AssertJUnit.assertTrue("Should be able to set piority", passed);

		AssertJUnit.assertTrue("New value should be the same as we set",
				testRoot.getMaxPriority() == Thread.MIN_PRIORITY);

		testRoot.destroy();

	}

	/**
	 * @tests java.lang.ThreadGroup#getName()
	 */
	@Test
	public void test_getName() {
		final ThreadGroup originalCurrent = getInitialThreadGroup();
		final String name = "Test group";
		final ThreadGroup testRoot = new ThreadGroup(originalCurrent, name);

		AssertJUnit.assertTrue("Setting a name&getting does not work", testRoot.getName().equals(name));

		testRoot.destroy();

	}

	static ThreadGroup checkAccessGroup = null;

	/**
	 * @tests java.lang.ThreadGroup#getParent()
	 */
	@Test
	public void test_getParent() {
		final ThreadGroup originalCurrent = getInitialThreadGroup();
		ThreadGroup testRoot = new ThreadGroup(originalCurrent, "Test group");

		AssertJUnit.assertTrue("Parent is wrong", testRoot.getParent() == originalCurrent);

		// Create some groups, nested some levels.
		final int TOTAL_DEPTH = 5;
		ThreadGroup current = testRoot;
		Vector groups = new Vector();
		// To maintain the invariant that a thread in the Vector is parent
		// of the next one in the collection (and child of the previous one)
		groups.addElement(testRoot);

		for (int i = 0; i < TOTAL_DEPTH; i++) {
			current = new ThreadGroup(current, "level " + i);
			groups.addElement(current);
		}

		// Now we walk the levels down, checking if parent is ok
		for (int i = 1; i < groups.size(); i++) {
			current = (ThreadGroup)groups.elementAt(i);
			ThreadGroup previous = (ThreadGroup)groups.elementAt(i - 1);
			AssertJUnit.assertTrue("Parent is wrong", current.getParent() == previous);
		}

		/* [PR 97314] ThreadGroup.getParent() should call parent.checkAccess() */
		SecurityManager m = new SecurityManager() {
			public void checkAccess(ThreadGroup group) {
				checkAccessGroup = group;
			}
		};
		ThreadGroup parent;
		try {
			System.setSecurityManager(m); // To see if it checks Thread creation
			// with our SecurityManager
			parent = testRoot.getParent();
		} finally {
			System.setSecurityManager(null); // restore original, no
			// side-effects
		}
		AssertJUnit.assertTrue("checkAccess with incorrect group", checkAccessGroup == parent);

		testRoot.destroy();
	}

	/**
	 * @tests java.lang.ThreadGroup#isDaemon()
	 */
	@Test
	public void test_isDaemon() {
		daemonTests();
	}

	/**
	 * @tests java.lang.ThreadGroup#list()
	 */
	@Test
	public void test_list() {
		final ThreadGroup originalCurrent = getInitialThreadGroup();
		// wipeSideEffectThreads destory all side effect of threads created in
		// java.lang.thread
		boolean result = wipeSideEffectThreads(originalCurrent);
		if (result == false)
			logger.error("wipe threads in test_list() not successful");
		final ThreadGroup testRoot = new ThreadGroup(originalCurrent, "Test group");

		// First save the original System.out
		java.io.PrintStream originalOut = System.out;

		try {
			java.io.ByteArrayOutputStream contentsStream = new java.io.ByteArrayOutputStream(100);
			java.io.PrintStream newOut = new java.io.PrintStream(contentsStream);

			// We have to "redirect" System.out to test the method 'list'
			System.setOut(newOut);

			originalCurrent.list();
			String contents = contentsStream.toString();

			/*
			 * The output has to look like this
			 *
			 * java.lang.ThreadGroup[name=main,maxpri=10] Thread[main,5,main]
			 * java.lang.ThreadGroup[name=Test group,maxpri=10]
			 */

			boolean passed = verifyThreadList(originalCurrent, testRoot, contents);

			AssertJUnit.assertTrue("Either 'list' is wrong or other tests are leaving side-effects.\n"
					+ "Result from list:\n " + "-----------------\n " + contents + "\n-----------------\n ", passed);

			// Do proper cleanup
			testRoot.destroy();

		} finally {
			// No matter what, we need to restore the original System.out
			System.setOut(originalOut);
		}

	}

	/**
	 * @tests java.lang.ThreadGroup#parentOf(java.lang.ThreadGroup)
	 */
	@Test
	public void test_parentOf() {
		final ThreadGroup originalCurrent = getInitialThreadGroup();
		final ThreadGroup testRoot = new ThreadGroup(originalCurrent, "Test group");
		final int DEPTH = 4;
		buildRandomTreeUnder(testRoot, DEPTH);

		final ThreadGroup[] allChildren = allGroups(testRoot);
		for (int i = 0; i < allChildren.length; i++) {
			AssertJUnit.assertTrue("Have to be parentOf all children", testRoot.parentOf((ThreadGroup)allChildren[i]));
		}

		AssertJUnit.assertTrue("Have to be parentOf itself", testRoot.parentOf(testRoot));

		testRoot.destroy();
		AssertJUnit.assertTrue("Parent can't have test group as subgroup anymore",
				!arrayIncludes(groups(testRoot.getParent()), testRoot));

		/* [PR 97314] ThreadGroup.getParent() should call parent.checkAccess() */
		try {
			System.setSecurityManager(new SecurityManager());
			AssertJUnit.assertTrue("Should not be parent", !testRoot.parentOf(originalCurrent));
		} finally {
			System.setSecurityManager(null);
		}
	}

	/**
	 * @tests java.lang.ThreadGroup#resume()
	 */
	@Test
	public void test_resume() throws OutOfMemoryError {
		final ThreadGroup originalCurrent = getInitialThreadGroup();

		final ThreadGroup testRoot = new ThreadGroup(originalCurrent, "Test group");
		/* [PR 116140] create fewer threads */
		final int DEPTH = 2;
		buildRandomTreeUnder(testRoot, DEPTH);

		final int THREADS_PER_GROUP = 2;
		final Vector threads = populateGroupsWithThreads(testRoot, THREADS_PER_GROUP);

		boolean[] isResumed = null;
		Object[] locks = new Object[threads.size()];

		try {
			try {
				for (int i = 0; i < threads.size(); i++) {
					MyThread t = (MyThread)threads.elementAt(i);
					t.setLock(locks[i] = new Object());
					t.start();
					t.suspend();
				}
			} catch (OutOfMemoryError e) {
				for (int i = 0; i < threads.size(); i++) {
					Thread t = (Thread)threads.elementAt(i);
					t.resume();
					t.stop(); // deprecated but effective
				}
				throw e;
			}

			// Now that they are all suspended, let's resume the ThreadGroup
			testRoot.resume();

			isResumed = new boolean[threads.size()];

			/*This loop goes through each lock for each thread*/
			/*Each lock can be released only if the thread(MyThread) is running or it is stopped or destroyed*/
			/*Timeout thread waits certain amount of time and kills the MyThread if it did not resume. This releases the lock*/
			for (int i = 0; i < threads.size(); i++) {
				MyThread t = (MyThread)threads.elementAt(i);
				t.resumeRequested();
				synchronized (locks[i]) {
					try {
						locks[i].wait(20000);
					} catch (InterruptedException e) {
						Assert.fail("Should not have been interrupted");
					}
					//check whether it resumed or timedout
					if (t.isResumed())
						isResumed[i] = true;
					else
						break;
				}

			}

			for (int i = 0; i < isResumed.length; i++) {
				MyThread t = (MyThread)threads.elementAt(i);
				t.stop(); // deprecated but effective
			}

			// Give them some time to really die
			try {
				Thread.sleep(1000);
			} catch (InterruptedException ie) {
				Assert.fail("Should not have been interrupted");
			}
		} finally {
			// Make sure we do cleanup before returning
			testRoot.destroy();
		}

		for (int i = 0; i < isResumed.length; i++) {
			AssertJUnit.assertTrue("Thread " + threads.elementAt(i) + " was not running when it was resumed",
					isResumed[i]);
		}

		AssertJUnit.assertTrue("Method destroy must have problems", testRoot.activeCount() == 0);

	}

	/**
	 * @tests java.lang.ThreadGroup#setDaemon(boolean)
	 */
	@Test
	public void test_setDaemon() {
		daemonTests();
	}

	/**
	 * @tests java.lang.ThreadGroup#setDaemon(boolean)
	 */
	@Test
	public void test_setDaemon2() {
		/*
		 * [PR CMVC 99507] Do not destroy daemon group if threads have been
		 * added but not started
		 */
		ThreadGroup daemonGroup = new ThreadGroup("daemontest1");
		Thread myThread = new Thread(daemonGroup, new Runnable() {
			public void run() {
			}
		});
		ThreadGroup childGroup = new ThreadGroup(daemonGroup, "child");
		daemonGroup.setDaemon(true);
		AssertJUnit.assertTrue("activeCount not zero", daemonGroup.activeCount() == 0);
		childGroup.destroy();
		AssertJUnit.assertTrue("child not destroyed", childGroup.isDestroyed());
		AssertJUnit.assertTrue("daemon is destroyed", !daemonGroup.isDestroyed());
		myThread.start();
		try {
			myThread.join();
		} catch (InterruptedException e) {
		}
		AssertJUnit.assertTrue("daemon not destroyed", daemonGroup.isDestroyed());
	}

	/**
	 * @tests java.lang.ThreadGroup#setMaxPriority(int)
	 */
	@Test
	public void test_setMaxPriority() {
		final ThreadGroup originalCurrent = getInitialThreadGroup();
		ThreadGroup testRoot = new ThreadGroup(originalCurrent, "Test group");

		boolean passed;

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

		/*
		 * [PR 1FJ9S51] If new priority is greater than the current maximum, the
		 * maximum remains unchanged
		 */
		int currentMax = testRoot.getMaxPriority();
		testRoot.setMaxPriority(Thread.MAX_PRIORITY + 1);
		passed = testRoot.getMaxPriority() == currentMax;
		AssertJUnit.assertTrue("setMaxPriority: Any value higher than the current one is ignored. Before: " + currentMax
				+ " , after: " + testRoot.getMaxPriority(), passed);

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

		/*[PR CMVC 177870] Java7:JCK:java_lang/ThreadGroup/setMaxPriority fails in all platform */
		currentMax = testRoot.getMaxPriority();
		testRoot.setMaxPriority(Thread.MIN_PRIORITY - 1);
		passed = testRoot.getMaxPriority() == currentMax;
		AssertJUnit.assertTrue("setMaxPriority: Any value smaller than MIN_PRIORITY is ignored. Before: " + currentMax
				+ " , after: " + testRoot.getMaxPriority(), passed);

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

		/*
		 * [PR 1FJ9S51] The test above now has side-effect, so we destroy and
		 * create a new one
		 */
		testRoot.destroy();
		testRoot = new ThreadGroup(originalCurrent, "Test group");

		// Create some groups, nested some levels. Each level will have maxPrio
		// 1 unit smaller
		// than the parent's. However, there can't be a group with priority <
		// Thread.MIN_PRIORITY
		final int TOTAL_DEPTH = testRoot.getMaxPriority() - Thread.MIN_PRIORITY - 2;
		ThreadGroup current = testRoot;
		for (int i = 0; i < TOTAL_DEPTH; i++) {
			current = new ThreadGroup(current, "level " + i);
		}

		// Now we walk the levels down, changing the maxPrio and later verifying
		// that the value
		// is indeed 1 unit smaller than the parent's maxPrio.
		int maxPrio, parentMaxPrio;
		current = testRoot;

		// To maintain the invariant that when we are to modify a child,
		// its maxPriority is always 1 unit smaller than its parent's.
		// We have to set it for the root manually, and the loop does the rest
		// for all
		// the other sub-levels
		current.setMaxPriority(current.getParent().getMaxPriority() - 1);

		for (int i = 0; i < TOTAL_DEPTH; i++) {
			maxPrio = current.getMaxPriority();
			parentMaxPrio = current.getParent().getMaxPriority();

			ThreadGroup[] children = groups(current);
			AssertJUnit.assertTrue("Can only have 1 subgroup", children.length == 1);
			current = children[0];
			AssertJUnit.assertTrue(
					"Had to be 1 unit smaller than parent's priority in iteration=" + i + " checking->" + current,
					maxPrio == parentMaxPrio - 1);
			current.setMaxPriority(maxPrio - 1);

			// The next test is sort of redundant, since in next iteration it
			// will be the
			// parent tGroup, so the test will be done.
			AssertJUnit.assertTrue("Had to be possible to change max priority",
					current.getMaxPriority() == maxPrio - 1);
		}

		AssertJUnit.assertTrue("Priority of leaf child group has to be much smaller than original root group",
				current.getMaxPriority() == testRoot.getMaxPriority() - TOTAL_DEPTH);

		testRoot.destroy();

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

		passed = true;
		testRoot = new ThreadGroup(originalCurrent, "Test group");
		try {
			testRoot.setMaxPriority(Thread.MAX_PRIORITY);
		} catch (IllegalArgumentException iae) {
			passed = false;
		}
		AssertJUnit.assertTrue(
				"Max Priority = Thread.MAX_PRIORITY should be possible if the test is run with default system ThreadGroup as root",
				passed);
		testRoot.destroy();

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

		passed = true;
		testRoot = new ThreadGroup(originalCurrent, "Test group");
		System.setSecurityManager(new SecurityManager());
		try {
			try {
				testRoot.setMaxPriority(Thread.MIN_PRIORITY);
			} catch (IllegalArgumentException iae) {
				passed = false;
			}
		} finally {
			System.setSecurityManager(null);
		}
		AssertJUnit.assertTrue("Min Priority = Thread.MIN_PRIORITY should be possible, always", passed);
		testRoot.destroy();

		/* [PR 97314] ThreadGroup.getParent() should call parent.checkAccess() */
		try {
			System.setSecurityManager(new SecurityManager());
			/* [PR 97314] Should not cause a SecurityException */
			originalCurrent.setMaxPriority(Thread.MAX_PRIORITY);
		} finally {
			System.setSecurityManager(null);
		}
	}

	/**
	 * @tests java.lang.ThreadGroup#stop()
	 */
	@Test
	public void test_stop() throws OutOfMemoryError {
		final ThreadGroup originalCurrent = getInitialThreadGroup();

		final ThreadGroup testRoot = new ThreadGroup(originalCurrent, "Test group");
		/* [PR 116140] create fewer threads */
		final int DEPTH = 2;
		buildRandomTreeUnder(testRoot, DEPTH);

		final int THREADS_PER_GROUP = 2;
		final Vector threads = populateGroupsWithThreads(testRoot, THREADS_PER_GROUP);

		try {
			for (int i = 0; i < threads.size(); i++) {
				Thread t = (Thread)threads.elementAt(i);
				t.start();
			}
		} catch (OutOfMemoryError e) {
			for (int i = 0; i < threads.size(); i++) {
				Thread t = (Thread)threads.elementAt(i);
				t.stop(); // deprecated but effective
			}
			throw e;
		}

		// Now that they are all running, let's stop the ThreadGroup
		testRoot.stop();

		// stop is an async call. The thread may take a while to stop. We have
		// to wait
		// for all of them to stop. However, if stop does not work, we'd have to
		// wait
		// forever. So, we wait with a timeout, and if the Thread is still
		// alive, we assume
		// stop for ThreadGroups does not work. How much we wait (timeout) is
		// very important
		boolean passed = true;
		for (int i = 0; i < threads.size(); i++) {
			Thread t = (Thread)threads.elementAt(i);
			/*
			 * Instead of waiting timeout time once, break it into 100ms.
			 * If the condition is fit earlier, then we do not have to wait whole timeout.
			 */
			long timeout = 10000;
			long start = System.currentTimeMillis();
			long current = System.currentTimeMillis();
			while (current - start < timeout) {
				try {
					/*Do not wait if thread is stopped.*/
					if (t.isAlive()) {
						/*
						 * We wait 10000 ms per Thread, but due to
						 * scheduling it may take a while to run
						 */
						t.join(100);
					} else
						break;
				} catch (InterruptedException ie) {
					Assert.fail("Should not be interrupted");
				}
				current = System.currentTimeMillis();
			}

			if (t.isAlive()) {
				passed = false;
				break;
			}
		}

		// To make sure that even if we fail, we exit in a clean state
		try {
			testRoot.destroy();
		} catch (IllegalThreadStateException e) {
			//If we are here, then ThreadGroup is not empty meaning
			//that there are threads in it which were failed to be stopped
			Assert.fail("Thread should be dead by now");
		}

		AssertJUnit.assertTrue("Thread should be dead by now", passed);

		AssertJUnit.assertTrue("Method destroy (or wipeAllThreads) must have problems", testRoot.activeCount() == 0);

	}

	/**
	 * @tests java.lang.ThreadGroup#suspend()
	 */
	@Test
	public void test_suspend() throws OutOfMemoryError {
		final ThreadGroup originalCurrent = getInitialThreadGroup();

		final ThreadGroup testRoot = new ThreadGroup(originalCurrent, "Test group");
		/* [PR 116140] create fewer threads */
		final int DEPTH = 2;
		buildRandomTreeUnder(testRoot, DEPTH);

		final int THREADS_PER_GROUP = 2;
		final Vector threads = populateGroupsWithThreads(testRoot, THREADS_PER_GROUP);

		boolean passed = false;
		try {
			try {
				for (int i = 0; i < threads.size(); i++) {
					Thread t = (Thread)threads.elementAt(i);
					t.start();
				}
			} catch (OutOfMemoryError e) {
				for (int i = 0; i < threads.size(); i++) {
					Thread t = (Thread)threads.elementAt(i);
					t.stop(); // deprecated but effective
				}
				throw e;
			}

			// Now that they are all running, let's suspend the ThreadGroup
			testRoot.suspend();

			passed = allSuspended(threads);
			AssertJUnit.assertTrue("Should be able to wipe all threads (allSuspended=" + passed + ")",
					wipeAllThreads(testRoot));
		} finally {

			// We can't destroy a ThreadGroup if we do not make sure it has no
			// threads at all

			// Make sure we cleanup before returning from the method
			testRoot.destroy();
		}
		AssertJUnit.assertTrue("All threads should be suspended", passed);

		AssertJUnit.assertTrue("Method destroy (or wipeAllThreads) must have problems", testRoot.activeCount() == 0);

	}

	/**
	 * @tests java.lang.ThreadGroup#toString()
	 */
	@Test
	public void test_toString() {
		final ThreadGroup originalCurrent = getInitialThreadGroup();
		final String tGroupName = "Test group";

		// Our own subclass
		class MyThreadGroup extends ThreadGroup {
			// Have to define a constructor since there's no default one
			public MyThreadGroup(ThreadGroup parent, String name) {
				super(parent, name);
			}
		}
		;

		ThreadGroup testRoot = new MyThreadGroup(originalCurrent, tGroupName);
		final String toString = testRoot.toString();

		StringBuffer expectedResult = new StringBuffer();
		expectedResult.append(testRoot.getClass().getName());
		expectedResult.append("[name=");
		expectedResult.append(tGroupName);
		expectedResult.append(",maxpri=");
		expectedResult.append(testRoot.getMaxPriority());
		expectedResult.append("]");

		String expectedValue = expectedResult.toString();

		AssertJUnit.assertTrue("toString does not follow the Java language spec.", toString.equals(expectedValue));

		testRoot.destroy();
	}

	/**
	 * @tests java.lang.ThreadGroup#uncaughtException(java.lang.Thread,
	 *        java.lang.Throwable)
	 */
	@Test
	public void test_uncaughtException() {
		final ThreadGroup originalCurrent = getInitialThreadGroup();

		// indices for the array defined below
		final int TEST_DEATH = 0;
		final int TEST_OTHER = 1;
		final int TEST_EXCEPTION_IN_UNCAUGHT = 2;
		final int TEST_OTHER_THEN_DEATH = 3;
		final int TEST_FORCING_THROW_THREAD_DEATH = 4;
		final int TEST_KILLING = 5;
		final int TEST_DEATH_AFTER_UNCAUGHT = 6;

		final boolean[] passed = new boolean[] { false, false, false, false, false, false, false };

		ThreadGroup testRoot;
		Thread thread;

		// Our own exception class
		class TestException extends RuntimeException {
		}
		;

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// - - - - - - -
		/* [PR 1FJA0U0] */
		testRoot = new ThreadGroup(originalCurrent, "Test killing a Thread, forcing it to throw ThreadDeath") {
			public void uncaughtException(Thread t, Throwable e) {
				if (e instanceof ThreadDeath)
					passed[TEST_KILLING] = true;
				// always forward, any exception
				super.uncaughtException(t, e);
			}
		};

		// Test if a Thread tells its ThreadGroup about ThreadDeath
		thread = new Thread(testRoot, null, "victim thread (to be killed)") {
			public void run() {
				while (true) {
					Thread.yield();
				}
			}
		};
		thread.start();
		try {
			Thread.sleep(1000);
		} catch (InterruptedException ie) {
			AssertJUnit.assertTrue("Should not have been interrupted", false);
		}
		thread.stop(); // we know this is deprecated, but we must test this
		// scenario
		// When we stop a thread, it is tagged as not alive even though it is
		// still running code.
		// join would be a no-op, and we might have a race condition. So, to
		// play safe, we wait before joining & testing
		// if the exception was really forwarded to the ThreadGroup
		try {
			Thread.sleep(1000);
		} catch (InterruptedException ie) {
			AssertJUnit.assertTrue("Should not have been interrupted", false);
		}
		try {
			thread.join();
		} catch (InterruptedException ie) {
			AssertJUnit.assertTrue("Should not have been interrupted", false);
		}
		testRoot.destroy();
		AssertJUnit.assertTrue(
				"Any thread should notify its ThreadGroup about its own death, even if killed:" + testRoot,
				passed[TEST_KILLING]);

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// - - - - - - -
		/* [PR 1FJA0U0] */
		testRoot = new ThreadGroup(originalCurrent, "Test Forcing a throw of ThreadDeath") {
			public void uncaughtException(Thread t, Throwable e) {
				if (e instanceof ThreadDeath)
					passed[TEST_FORCING_THROW_THREAD_DEATH] = true;
				// always forward, any exception
				super.uncaughtException(t, e);
			}
		};

		// Test if a Thread tells its ThreadGroup about ThreadDeath
		thread = new Thread(testRoot, null, "suicidal thread") {
			public void run() {
				throw new ThreadDeath();
			}
		};
		thread.start();
		try {
			thread.join();
		} catch (InterruptedException ie) {
			AssertJUnit.assertTrue("Should not have been interrupted", false);
		}
		testRoot.destroy();
		AssertJUnit.assertTrue(
				"Any thread should notify its ThreadGroup about its own death, even if suicide:" + testRoot,
				passed[TEST_FORCING_THROW_THREAD_DEATH]);

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// - - - - - - -

		testRoot = new ThreadGroup(originalCurrent, "Test ThreadDeath") {
			public void uncaughtException(Thread t, Throwable e) {
				passed[TEST_DEATH] = false;
				// always forward, any exception
				super.uncaughtException(t, e);
			}
		};

		// Test if a Thread tells its ThreadGroup about ThreadDeath
		passed[TEST_DEATH] = true;
		thread = new Thread(testRoot, null, "no-op thread");
		thread.start();
		try {
			thread.join();
		} catch (InterruptedException ie) {
			AssertJUnit.assertTrue("Should not have been interrupted", false);
		}
		testRoot.destroy();
		AssertJUnit.assertTrue("A thread should not call uncaughtException when it dies:" + testRoot,
				passed[TEST_DEATH]);

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// - - - - - - -

		testRoot = new ThreadGroup(originalCurrent, "Test other Exception") {
			public void uncaughtException(Thread t, Throwable e) {
				if (e instanceof TestException)
					passed[TEST_OTHER] = true;
				else
					// only forward exceptions other than our test
					super.uncaughtException(t, e);
			}
		};

		// Test if a Thread tells its ThreadGroup about an Exception
		thread = new Thread(testRoot, null, "no-op thread") {
			public void run() {
				throw new TestException();
			}
		};
		thread.start();
		try {
			thread.join();
		} catch (InterruptedException ie) {
			AssertJUnit.assertTrue("Should not have been interrupted", false);
		}
		testRoot.destroy();
		AssertJUnit.assertTrue("Any thread should notify its ThreadGroup about an uncaught exception:" + testRoot,
				passed[TEST_OTHER]);

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// - - - - - - -

		// Our own uncaught exception class
		class UncaughtException extends TestException {
		}
		;

		testRoot = new ThreadGroup(originalCurrent, "Test Exception in uncaught exception") {
			public void uncaughtException(Thread t, Throwable e) {
				if (e instanceof TestException) {
					passed[TEST_EXCEPTION_IN_UNCAUGHT] = true;
					// Let's simulate an error inside our uncaughtException
					// method.
					// This should be no-op according to the spec
					throw new UncaughtException();
				} else
					// only forward exceptions other than our test
					super.uncaughtException(t, e);
			}
		};

		// Test if an Exception in uncaughtException is really a no-op
		thread = new Thread(testRoot, null, "no-op thread") {
			public void run() {
				try {
					throw new TestException();
				} catch (UncaughtException ue) {
					// any exception in my ThreadGroup's uncaughtException must
					// not be propagated
					// If it propagated and we detected that, the test failed
					passed[TEST_EXCEPTION_IN_UNCAUGHT] = false;
				}
			}
		};
		thread.start();
		try {
			thread.join();
		} catch (InterruptedException ie) {
			AssertJUnit.assertTrue("Should not have been interrupted", false);
		}
		testRoot.destroy();
		AssertJUnit.assertTrue("Any uncaughtException in uncaughtException should be no-op:" + testRoot,
				passed[TEST_EXCEPTION_IN_UNCAUGHT]);

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// - - - - - - -

		// This is a mix of 2 of the tests above. It is assumed that ThreadDeath
		// and any random
		// exception do work , tested separately. Now we test if after an
		// uncaughtException is
		// forwarded to the ThreadGroup and the Thread dies, if ThreadDeath is
		// also forwarded
		// It should be (so that a ThreadGroup can know its Thread died)

		testRoot = new ThreadGroup(originalCurrent, "Test Uncaught followed by ThreadDeath") {
			public void uncaughtException(Thread t, Throwable e) {
				if (e instanceof ThreadDeath)
					passed[TEST_DEATH_AFTER_UNCAUGHT] = true;
				if (e instanceof TestException)
					passed[TEST_OTHER_THEN_DEATH] = true;
				else
					// only forward exceptions other than our test
					super.uncaughtException(t, e);
			}
		};

		// Test if a Thread tells its ThreadGroup about an Exception and also
		// ThreadDeath
		thread = new Thread(testRoot, null, "no-op thread") {
			public void run() {
				throw new TestException();
			}
		};
		thread.start();
		try {
			thread.join();
		} catch (InterruptedException ie) {
			AssertJUnit.assertTrue("Should not have been interrupted", false);
		}
		testRoot.destroy();
		// PDS(1/31/00) - test is bogus, uncaughtException() is not called with
		// ThreadDeath
		// assertTrue("Any thread should notify its ThreadGroup about an uncaught exception AND its ThreadDeath as well:"
		// + testRoot , passed[TEST_OTHER_THEN_DEATH] &&
		// passed[TEST_DEATH_AFTER_UNCAUGHT]);

	}

	/**
	 * Sets up the fixture, for example, open a network connection. This method
	 * is called before a test is executed.
	 */
	@BeforeMethod
	protected void setUp() {
		initialThreadGroup = Thread.currentThread().getThreadGroup();
		rootThreadGroup = initialThreadGroup;
		while (rootThreadGroup.getParent() != null)
			rootThreadGroup = rootThreadGroup.getParent();
	}

	/**
	 * Tears down the fixture, for example, close a network connection. This
	 * method is called after a test is executed.
	 */
	@AfterMethod
	protected void tearDown() {
		try {
			// Give the threads a chance to die.
			Thread.sleep(50);
		} catch (InterruptedException e) {
		}
	}

	private Thread[] threads(ThreadGroup parent) {
		// this method does not exist in xtrem
		int count = parent.activeCount(); // No API to get the count of
		// immediate children only ?
		Thread[] all = new Thread[count];
		int actualSize = parent.enumerate(all, false);
		Thread[] result;
		if (actualSize == all.length)
			result = all;
		else {
			result = new Thread[actualSize];
			System.arraycopy(all, 0, result, 0, actualSize);
		}

		return result;

	}

	private ThreadGroup getInitialThreadGroup() {
		return initialThreadGroup;
	}

	private ThreadGroup[] allGroups(ThreadGroup parent) {
		int count = parent.activeGroupCount();
		ThreadGroup[] all = new ThreadGroup[count];
		parent.enumerate(all, true);
		return all;
	}

	private void daemonTests() {
		// Test for method void java.lang.ThreadGroup.setDaemon(boolean)

		final ThreadGroup originalCurrent = getInitialThreadGroup();
		final ThreadGroup testRoot = new ThreadGroup(originalCurrent, "Test group");

		testRoot.setDaemon(true);
		AssertJUnit.assertTrue("Setting daemon&getting does not work", testRoot.isDaemon());

		testRoot.setDaemon(false);
		AssertJUnit.assertTrue("Setting daemon&getting does not work", !testRoot.isDaemon());

		testRoot.destroy();

	}

	private boolean wipeAllThreads(final ThreadGroup aGroup) {
		boolean ok = true;
		Thread[] threads = threads(aGroup);
		for (int i = 0; i < threads.length; i++) {
			Thread t = threads[i];
			ok = ok && wipeThread(t);
		}

		// Recursively for subgroups (if any)
		ThreadGroup[] children = groups(aGroup);
		for (int i = 0; i < children.length; i++) {
			ok = ok && wipeAllThreads(children[i]);
		}
		return ok;
	}

	private boolean wipeAllThreads(final Vector threads) {
		boolean ok = true;
		for (int i = 0; i < threads.size(); i++) {
			Thread t = (Thread)threads.elementAt(i);
			ok = ok && wipeThread(t);
		}
		return ok;
	}

	private boolean wipeSideEffectThreads(ThreadGroup aGroup) {
		boolean ok = true;
		Thread[] threads = threads(aGroup);
		for (int i = 0; i < threads.length; i++) {
			Thread t = threads[i];
			if (t.getName().equals("SimpleThread") || t.getName().equals("Bogus Name") || t.getName().equals("Testing")
					|| t.getName().equals("foo") || t.getName().equals("Test Group") || t.getName().equals("Squawk")
					|| t.getName().equals("Thread-1") || t.getName().equals("firstOne")
					|| t.getName().equals("secondOne") || t.getName().equals("Thread-16")
					|| t.getName().equals("Thread-14"))
				ok = ok && wipeThread(t);
		}

		// Recursively for subgroups (if any)
		ThreadGroup[] children = groups(aGroup);

		for (int i = 0; i < children.length; i++) {
			ok = ok && wipeSideEffectThreads(children[i]);
			if (children[i].getName().equals("Test Group") || children[i].getName().equals("foo")
					|| children[i].getName().equals("jp"))
				children[i].destroy();
		}
		try {
			// Give the threads a chance to die.
			Thread.sleep(50);
		} catch (InterruptedException e) {
		}
		return ok;
	}

	private void asyncBuildRandomTreeUnder(final ThreadGroup aGroup, final int depth, final Vector allCreated) {
		if (depth <= 0)
			return;

		final int maxImmediateSubgroups = random(3);
		for (int i = 0; i < maxImmediateSubgroups; i++) {
			final int iClone = i;
			final String name = " Depth = " + depth + ",N = " + iClone + ",Vector size at creation: "
					+ allCreated.size();
			// Use concurrency to maximize chance of exposing concurrency bugs
			// in ThreadGroups
			Thread t = new Thread(aGroup, name) {
				public void run() {
					ThreadGroup newGroup = new ThreadGroup(aGroup, name);
					allCreated.addElement(newGroup);
					asyncBuildRandomTreeUnder(newGroup, depth - 1, allCreated);
				}
			};
			t.start();
		}
	}

	private Vector asyncBuildRandomTreeUnder(final ThreadGroup aGroup, final int depth) {
		Vector result = new Vector();
		asyncBuildRandomTreeUnder(aGroup, depth, result);
		return result;
	}

	private boolean verifyThreadList(ThreadGroup root, ThreadGroup onlyChildGroup, String listOutput) {
		// We expect that @root has only 1 subgroup, @onlyChildGroup. The
		// output from
		// method 'list' is stored in @listOutput
		if (listOutput.length() == 0) {
			return false;
		}

		// If we got a long output, it means some previous test must have left
		// side-effects
		// (more subgroups and threads);

		//There should be only two ThreadGroup in the list.
		int firstIndex = listOutput.indexOf("ThreadGroup");
		if (firstIndex == -1)
			return false;
		int secondIndex = listOutput.indexOf("ThreadGroup", firstIndex + 1);
		if (secondIndex == -1)
			return false;

		int thirdIndex = listOutput.indexOf("ThreadGroup", secondIndex + 1);
		if (thirdIndex != -1)
			return false;

		//There should be onlyone thread called "Test group" in the list
		int fourthIndex = listOutput.indexOf("Test group");
		if (fourthIndex == -1)
			return false;
		int fifthIndex = listOutput.indexOf("Test group", fourthIndex + 1);
		if (fifthIndex != -1)
			return false;

		return true;
	}

	private boolean allNotSuspended(Vector threads) {
		for (int i = 0; i < threads.size(); i++) {
			MyThread t = (MyThread)threads.elementAt(i);
			if (!t.isActivelyRunning())
				return false;
		}
		return true;
	}

	private boolean allSuspended(Vector threads) {
		for (int i = 0; i < threads.size(); i++) {
			MyThread t = (MyThread)threads.elementAt(i);
			if (t.isActivelyRunning())
				return false;
		}
		return true;
	}

	private boolean sameThreads(Thread[] allThreads, Vector threads) {
		if (allThreads.length != threads.size())
			return false;

		// The complexity of this method is N2, and we do it twice !!

		// First make sure that all threads in @threads are also in @allThreads
		for (int i = 0; i < allThreads.length; i++) {
			Thread t = (Thread)threads.elementAt(i);
			if (!arrayIncludes(allThreads, t))
				return false;
		}

		// Now make sure that all threads in @allThreads are also in @threads
		Thread[] vectorThreads = new Thread[threads.size()];
		threads.copyInto(vectorThreads);
		for (int i = 0; i < vectorThreads.length; i++) {
			Thread t = allThreads[i];
			if (!arrayIncludes(vectorThreads, t))
				return false;
		}
		return true;
	}

	private ThreadGroup[] groups(ThreadGroup parent) {
		int count = parent.activeGroupCount(); // No API to get the count of
		// immediate children only ?
		ThreadGroup[] all = new ThreadGroup[count];
		parent.enumerate(all, false);
		// Now we may have nulls in the array, we must find the actual size
		int actualSize = 0;
		for (; actualSize < all.length; actualSize++) {
			if (all[actualSize] == null)
				break;
		}
		ThreadGroup[] result;
		if (actualSize == all.length)
			result = all;
		else {
			result = new ThreadGroup[actualSize];
			System.arraycopy(all, 0, result, 0, actualSize);
		}
		return result;
	}

	private Vector populateGroupsWithThreads(final ThreadGroup aGroup, final int threadCount) {
		Vector result = new Vector();
		populateGroupsWithThreads(aGroup, threadCount, result);
		return result;
	}

	private void populateGroupsWithThreads(final ThreadGroup aGroup, final int threadCount, final Vector allCreated) {
		for (int i = 0; i < threadCount; i++) {
			final int iClone = i;
			final String name = "(MyThread)N =" + iClone + "/" + threadCount + " ,Vector size at creation: "
					+ allCreated.size();

			MyThread t = new MyThread(aGroup, name);
			allCreated.addElement(t);
		}
		// Recursively for subgroups (if any)
		ThreadGroup[] children = groups(aGroup);
		for (int i = 0; i < children.length; i++) {
			populateGroupsWithThreads(children[i], threadCount, allCreated);
		}
	}

	private int random(int max) {

		return 1 + ((new Object()).hashCode() % max);

	}

	private boolean parentOfAll(ThreadGroup parentCandidate, ThreadGroup[] childrenCandidates) {
		for (int i = 0; i < childrenCandidates.length; i++) {
			if (!parentCandidate.parentOf(childrenCandidates[i]))
				return false;
		}
		return true;
	}

	private boolean wipeThread(Thread t) {
		// this method does not exist in xtrem
		// if (!t.isAlive()) t.start(); // If not started yet, we do so
		// t.resume(); // Just in case, we resume it. Maybe this should be done
		// by Thread.stop ?
		t.stop();
		// Thread.yield(); // Just in case we have too many Threads (timeout
		// below might not be enough)
		// Thread.yield(); // Same
		try {
			t.join(1000);
		} catch (InterruptedException ie) {
			AssertJUnit.assertTrue("Should not have been interrupted", false);
		}
		if (t.isAlive()) // The thread had plenty (subjective) of time to die,
			// so there's a problem
			return false;
		return true;
	}

	private Vector buildRandomTreeUnder(ThreadGroup aGroup, int depth) {
		Vector result = asyncBuildRandomTreeUnder(aGroup, depth);
		while (true) {
			int sizeBefore = result.size();
			try {
				Thread.sleep(1000);
				int sizeAfter = result.size();
				// If no activity for a while, we assume async building may be
				// done
				if (sizeBefore == sizeAfter)
					// It can only be done if no more threads. Unfortunately we
					// are relying on this
					// API to work as well. If it does not, we may loop forever.
					if (aGroup.activeCount() == 0)
					break;
			} catch (InterruptedException e) {
			}
		}
		return result;
	}

	private boolean arrayIncludes(Object[] array, Object toTest) {
		for (int i = 0; i < array.length; i++) {
			if (array[i] == toTest)
				return true;
		}
		return false;
	}

	protected void myassertTrue(String msg, boolean b) {
		// This method is defined here just to solve a visibility problem
		// of protected methods with inner types - see 1ES3UGD
		AssertJUnit.assertTrue(msg, b);
	}

	private ThreadGroup getRootThreadGroup() {
		return rootThreadGroup;

	}

}
