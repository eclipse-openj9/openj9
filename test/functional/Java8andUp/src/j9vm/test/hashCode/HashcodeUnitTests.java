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
import org.testng.annotations.BeforeMethod;
import j9vm.test.hashCode.generator.HashCodeGenerator;

import java.io.PrintStream;

public class HashcodeUnitTests {
	private static final int LOWER_THRESHOLD = 12;
	private static final int UPPER_THRESHOLD = 20;
	HashTest harness = new HashTest();
	private PrintStream statsOutput;
	private BitStatistics bitStats;

	@BeforeMethod
	protected void setUp() throws Exception {
		harness.setUp();
		statsOutput = harness.statsOutput;
		bitStats = new BitStatistics();
		System.setProperty(HashTest.PROPERTY_HASHTEST_PREFIX + "lfsrcycles",
				"32");
	}

	@Test
	public void testRandHashingWithAscendingCount() {
		statsOutput
				.println("======================================================");
		harness.printMethodName();
		HashedObject.initialize(HashedObject.STRATEGY_RAND);
		runAndPrintStats();
	}

	@Test
	public void testLinconHashingWithAscendingCount() {
		statsOutput
				.println("======================================================");
		harness.printMethodName();
		HashedObject.initialize(HashedObject.STRATEGY_LINCON);
		runAndPrintStats();
	}

	@Test
	public void testLfsr1HashingWithAscendingCount() {
		statsOutput
				.println("======================================================");
		harness.printMethodName();
		HashedObject.initialize(HashedObject.STRATEGY_LFSR1);
		runAndPrintStats();
	}

	@Test
	public void testLfsrc1HashingWithAscendingCount() {
		statsOutput
				.println("======================================================");
		harness.printMethodName();
		HashedObject.initialize(HashedObject.STRATEGY_LFSRA1);
		runAndPrintStats();
	}

	@Test
	public void testLncLfsrHashingWithAscendingCount() {
		statsOutput
				.println("======================================================");
		harness.printMethodName();
		HashedObject.initialize(HashedObject.STRATEGY_LNCLFSR);
		runAndPrintStats();
	}

	private void runAndPrintStats() {
		HashCodeGenerator hashGen = HashedObject.hashGen;
		HashStats stats = new HashStats();
		hashGen.setSeed(1999);
		int oldhash = 0;
		for (int i = 0; i < 256; ++i) {
			int hash = hashGen.processHashCode(i);
			stats.update(hash);
			int disparity = bitStats.update(hash ^ oldhash);
			if ((disparity > UPPER_THRESHOLD) || (disparity < LOWER_THRESHOLD)) {
				statsOutput.println("disparity: " + disparity + " raw: "
						+ Integer.toHexString(i) + " oldHash: "
						+ Integer.toHexString(oldhash) + " hash: "
						+ Integer.toHexString(hash) + " hash difference: "
						+ Integer.toHexString(hash ^ oldhash));
			}
			oldhash = hash;
		}
		// stats.printHashValuesAndDifferences(statsOutput);
		stats.printStats(statsOutput);
	}

}
