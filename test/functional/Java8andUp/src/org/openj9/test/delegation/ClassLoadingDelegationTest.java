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
package org.openj9.test.delegation;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.Thread.State;

import com.ibm.jvm.Dump;

/**
 * CMVC 194091. Cross-delegation of non-explicitly synchronized parallel-incapable classloaders should deadlock (sic).
 * This is because reference implementation implicitly locks the classloader in the VM and, per CMVC 193900,
 * J9 does likewise.
 * registering as parallel capable prevents deadlock.
 */
@Test(groups = { "level.extended" })
public class ClassLoadingDelegationTest {

	public static final Logger logger = Logger.getLogger(ClassLoadingDelegationTest.class);

	private static final int WAITLIMIT = 1000;

	@Test
	public void testParallelIncapableDelegation() {
		TestLoader l1 = new ParallelIncapableClassLoader("l1");
		TestLoader l2 = new ParallelIncapableClassLoader("l2");
		AssertJUnit.assertFalse("classloaders should not be parallel capable", l1.isParallelCapable() || l2.isParallelCapable());
		runDelegationTest(l1, l2, true);
	}

	@Test
	public void testParallelCapableDelegation() {
		TestLoader l1 = new ParallelCapableClassLoader("l1");
		TestLoader l2 = new ParallelCapableClassLoader("l2");
		AssertJUnit.assertTrue("classloaders should not be parallel capable", l1.isParallelCapable() && l2.isParallelCapable());
		runDelegationTest(l1, l2, false);
	}

	/**
	 * Have two threads use classloaders which delegate in a non-hierarchical fashion.
	 * Detect that they deadlock as does reference implementation.
	 */
	private void runDelegationTest(TestLoader l1, TestLoader l2, boolean expectDeadlock) {
		int waitCount = 0;
		TestThread t1 = new TestThread("t1");
		TestThread t2 = new TestThread("t2");
		String classPath = System.getProperty("java.class.path");
		String pathSeparator = System.getProperty("path.separator");
		String classPathDirs[] = classPath.split(pathSeparator);
		TestLoader.setPathDirs(classPathDirs);

		l1.setDelegateTestClassB(true);
		l2.setDelegateTestClassB(false);
		l1.setOtherLoader(l2);
		l2.setOtherLoader(l1);
		t1.setMyLoader(l1);
		t2.setMyLoader(l2);

		t1.setTestClass(l1.getDelegatedClass());
		t2.setTestClass(l2.getDelegatedClass());

		t1.start();
		t2.start();

		try {
			State t1State;
			State t2State;
			State terminationCondition = expectDeadlock ? State.BLOCKED : State.TERMINATED;
			do {
				Thread.sleep(100);
				++waitCount;
				t1State = t1.getState();
				t2State = t2.getState();
				logger.debug("t1 state = " + t1State + " t2 state=" + t2State);
				if (waitCount == WAITLIMIT) {
					Dump.JavaDump();
					Assert.fail("TEST_STATUS: TEST_FAILED TestDelegation\nTest hung");
				} else if (expectDeadlock && ((State.TERMINATED == t1State) || (State.TERMINATED == t2State))) {
					Assert.fail("Threads did not deadlock");
				}
			} while ((terminationCondition != t1State) || (terminationCondition != t2State));
			logger.warn(expectDeadlock ? "Both threads deadlocked (expected behaviour)"
					: "No deadlock (expected behaviour)");
		} catch (InterruptedException e) {
			Assert.fail("TEST_STATUS: TEST_FAILED unexpected exception" + e, e);
		}
	}
}
