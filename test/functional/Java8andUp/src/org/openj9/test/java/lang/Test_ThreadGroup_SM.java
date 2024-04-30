/*
 * Copyright IBM Corp. and others 2022
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package org.openj9.test.java.lang;

import org.testng.annotations.AfterMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeMethod;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.util.Vector;

@Test(groups = { "level.sanity" })
public class Test_ThreadGroup_SM {

	private ThreadGroup rootThreadGroup = null;
	private ThreadGroup initialThreadGroup = null;

	private ThreadGroup getInitialThreadGroup() {
		return initialThreadGroup;
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

		AssertJUnit.assertTrue("CheckAccess is no-op with no Securitymanager", passed);

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

	private ThreadGroup[] allGroups(ThreadGroup parent) {
		int count = parent.activeGroupCount();
		ThreadGroup[] all = new ThreadGroup[count];
		parent.enumerate(all, true);
		return all;
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

	private int random(int max) {
		return 1 + ((new Object()).hashCode() % max);
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
					if (aGroup.activeCount() == 0) {
						break;
					}
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

}
