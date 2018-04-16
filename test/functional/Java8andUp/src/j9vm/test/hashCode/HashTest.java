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
package j9vm.test.hashCode;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeMethod;
import org.testng.AssertJUnit;
import j9vm.test.hashCode.HashedObjectFactory.objectClass;
import j9vm.test.hashCode.generator.HashCodeGenerator;

import java.io.PrintStream;
import java.util.Random;

/**
 * Gather statistics on the performance of the hashcode.
 *
 */
public class HashTest {

	private static final Logger logger = Logger.getLogger(HashTest.class);

	public static final String PROPERTY_HASHTEST_PREFIX = "hashtest.";
	public static String hashStrategy = System.getProperty(PROPERTY_HASHTEST_PREFIX + "hashstrategy");

	private static final boolean verboseSeedChange = Boolean.getBoolean(PROPERTY_HASHTEST_PREFIX + "verboseseedchange");
	private static boolean verboseValues = Boolean.getBoolean(PROPERTY_HASHTEST_PREFIX + "verbose");
	private static boolean smallTestsOnly = Boolean.getBoolean(PROPERTY_HASHTEST_PREFIX + "smalltest");

	private long limit = Long.getLong(PROPERTY_HASHTEST_PREFIX + "limit", 256000);
	private boolean verboseCollisions = Boolean.getBoolean(PROPERTY_HASHTEST_PREFIX + "verbosecollisions");

	public final PrintStream statsOutput = System.out;
	long oldCollisionCount = 0;
	private boolean randomPadding;

	@BeforeMethod(alwaysRun = true)
	protected void setUp() throws Exception {
		HashedObject.initialize(hashStrategy);
		randomPadding = false;
	}

	/**
	 * Force all objects to use a hashcode which increases monotonically.
	 * This should be used only for small numbers of objects.
	 */
	@Test(groups = { "level.extended" })
	public void testCollisionsWithAscendingCount() {
		if (smallTestsOnly) {
			return;
		}
		logger.debug("======================================================");
		String testName = printMethodName();
		if (null == hashStrategy) {
			logger.warn("skipping " + testName);
			return;
		}
		printSetupInfo();
		HashedObjectFactory objectFactory = new HashedObjectFactory(objectClass.ascendingCount);
		runAndGetStats(objectFactory);
	}

	/**
	 * Allocate a set of small objects and gather hashcode statistics.
	 */
	@Test(groups = { "level.extended" })
	public void testCollisionsWithSmallObjects() {
		logger.debug("======================================================");
		printMethodName();
		printSetupInfo();
		HashedObjectFactory objectFactory = new HashedObjectFactory(objectClass.hashedObject);
		runAndGetStats(objectFactory);
	}

	/**
	 * Allocate a set of medium sized objects and gather hashcode statistics.
	 */
	@Test(groups = { "level.extended" })
	public void testCollisionsWithBiggerObjects() {
		if (smallTestsOnly) {
			return;
		}
		logger.debug("======================================================");
		printMethodName();
		printSetupInfo();
		HashedObjectFactory objectFactory = new HashedObjectFactory(objectClass.biggerHashedObject);
		runAndGetStats(objectFactory);
	}

	/**
	 * Allocate a set of large objects and gather hashcode statistics.
	 */
	@Test(groups = { "level.extended" })
	public void testCollisionsWithEvenBiggerObjects() {
		if (smallTestsOnly) {
			return;
		}
		logger.debug("======================================================");
		printMethodName();
		printSetupInfo();
		HashedObjectFactory objectFactory = new HashedObjectFactory(objectClass.evenBiggerHashedObject);
		runAndGetStats(objectFactory);
	}

	/**
	 * Allocate a set of objects with random garbage allocations interspersed and gather hashcode statistics.
	 */
	@Test(groups = { "level.extended" })
	public void testCollisionsWithRandomAllocations() {
		if (smallTestsOnly) {
			return;
		}
		logger.debug("======================================================");
		printMethodName();
		randomPadding = true;
		printSetupInfo();
		HashedObjectFactory objectFactory = new HashedObjectFactory(objectClass.hashedObject);
		runAndGetStats(objectFactory);
	}

	public void runAndGetStats(HashedObjectFactory objectFactory) {
		Random rand = null;
		if (randomPadding) {
			rand = new Random(145926535);
		}
		HashedObject root = objectFactory.createHashedObject(0);
		HashStats stats = HashedObject.getStats();
		logger.debug("root=" + root);
		for (long i = 1; i < limit; ++i) {
			HashedObject ho = objectFactory.createHashedObject(i);
			HashedObject collision = root.addNode(ho);
			if (randomPadding) {
				int r = rand.nextInt(128);
				while (r > 0) {
					Integer foo = new Integer(r);
					r--;
				}
			}
			if (verboseSeedChange && ho.seedChanged()) {
				logger.debug("-------------------------------\nInterim results:");
				logger.debug("seed = " + ho.getSeed());
				printResults(stats);
			}
			if (verboseCollisions && (null != collision)) {
				logger.debug("collision: " + ho + "-" + collision);
			}
		}
		HashedObject result = root.verifyHashCode();
		if (null != result) {
			AssertJUnit.assertEquals("Hashcode not consistent", result.myHashCode, result.newHash);
		}
		logger.debug("++++++++++++++++++++++++++++++++++++++++++\nFinal results:");
		printResults(stats);
	}

	public void printResults(HashStats stats) {
		stats.printStats(statsOutput);
		logger.debug("collisions: " + HashedObject.collisionCount + ", "
				+ (HashedObject.collisionCount - oldCollisionCount) + " new collisions");
		oldCollisionCount = HashedObject.collisionCount;
		logger.debug("left placements: " + HashedObject.leftPlacementCount);
		logger.debug("right placements: " + HashedObject.rightPlacementCount);
		logger.debug(
				"imbalance: " + (imbalance(HashedObject.leftPlacementCount, HashedObject.rightPlacementCount)));
		if (verboseValues) {
			stats.printHashValuesAndDifferences(statsOutput);
		}
	}

	private void printSetupInfo() {
		logger.debug("limit: " + limit + " hash strategy: " + hashStrategy + " lfsrcycless: "
				+ HashCodeGenerator.LFSR_CYCLES + " lincon cycles: " + HashCodeGenerator.LINCON_CYCLES + " presquare: "
				+ HashCodeGenerator.HASH_PRESQUARE);
	}

	private String imbalance(long leftPlacementCount, long rightPlacementCount) {
		long difference = leftPlacementCount - rightPlacementCount;
		if (difference < 0) {
			difference = -difference;
		}
		float percentImbalance = 100;
		if ((leftPlacementCount + rightPlacementCount) != 0) {
			percentImbalance = 100 * difference / (leftPlacementCount + rightPlacementCount);
		}
		return Float.toString(percentImbalance) + '%';
	}

	public String printMethodName() {
		StackTraceElement[] myStack = Thread.currentThread().getStackTrace();
		String methodName = myStack[3].getMethodName();
		logger.debug("Starting " + methodName);
		return methodName;
	}

}
