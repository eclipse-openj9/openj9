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
 * Measure overall performance of IdentityHashMap.  This is a benchmark supplied by TRL.
 */
public class TrlTest {

	private static final int TABLE_SIZE_INCREMENT = Integer.getInteger("test.identityhashmap.size.increment", 50000);
	private static final int MAX_TABLE_SIZE = Integer.getInteger("test.identityhashmap.size.maximum", 2000000);

	@BeforeMethod
	protected void setUp() throws Exception {
	}

	/**
	 * Create objects and add them to an IdentityHashMap.  Determine the execution time total and mean time per insertion.
	 * Repeat for increasing number of objects.
	 * Use system property test.identityhashmap.size.maximum to set the maximum number of objects.
	 * Use system property test.identityhashmap.size.increment to set the granularity in the number of objects.
	 * 
	 */
	@Test
	public void testHashMapExecutionTime() {
		System.out.println("tableSize,total execution time,mean time per entry");
		for (int tableSize = TABLE_SIZE_INCREMENT; tableSize <= MAX_TABLE_SIZE; tableSize += TABLE_SIZE_INCREMENT) {
			long exTime = timeHashMapExecution(tableSize);
			long meanTime = exTime/tableSize;
			System.out.println(tableSize+","+exTime+","+meanTime);
		}
	}
	private long timeHashMapExecution(int size) {
		IdentityHashMap map =
				new IdentityHashMap();
		System.gc();
		long time = -System.nanoTime();
		for (int i = 0; i <= size; i++) {
			Object o = new Object();
			map.put(o, o);
		}
		time += System.nanoTime();
		return time;
	}
	
	public static void main(String[] args) {
		TrlTest testObj = new TrlTest();
		testObj.testHashMapExecutionTime();
	}
}
