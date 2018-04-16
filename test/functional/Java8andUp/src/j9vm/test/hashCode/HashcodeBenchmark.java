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
import java.util.IdentityHashMap;

/**
 * Determine the execution time of java.lang.System.identityHashCode()
 *
 * This can be run as a JUnit test or standalone, e.g. java j9vm.test.hashCode.HashCollisionTest
 */
public class HashcodeBenchmark {

	private static final int MAX_TABLE_SIZE = 1000000;

	@BeforeMethod
	protected void setUp() throws Exception {
	}

	/**
	 * run a timing loop with and without calls to identityHashCode and measure the difference.
	 */
	@Test
	public void testHashMapExecutionTime() {
		long exNoHashTime = 0;
		long exHashTime = 0;
		long sumSize = 0;
		for (int i = 0; i< 100; ++i) {
			exNoHashTime += timeObjectHash(MAX_TABLE_SIZE, false);
			exHashTime += timeObjectHash(MAX_TABLE_SIZE, true);
			sumSize += MAX_TABLE_SIZE;
		}
		long aveHashTime = (exHashTime-exNoHashTime)/sumSize;
		System.out.println("Average hash time: "+aveHashTime);
	}
	
	/**
	 * @param iterations Number of times to call identityHashCode()
	 * @param doHash if true, call identityHashCode(); else skip the call
	 * @return total execution time in ns for the entire loop.
	 */
	private long timeObjectHash(int iterations, boolean doHash) {
		IdentityHashMap map =
				new IdentityHashMap();
		System.gc();
		Object o = new Object();
		long time = -System.nanoTime();
		int Accum = 0;
		for (int i = 0; i < iterations; i++) {
			Accum += (doHash? java.lang.System.identityHashCode(o): 1);
		}
		if (Accum == 0) {
			System.err.println("bogus value");
		}
		time += System.nanoTime();
		return time;
	}
	
	/**
	 * Run the test standalone.
	 * @param args not used
	 */
	public static void main(String[] args) {
		HashcodeBenchmark testObj = new HashcodeBenchmark();
		testObj.testHashMapExecutionTime();
	}
}
