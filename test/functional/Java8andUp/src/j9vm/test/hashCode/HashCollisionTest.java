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

/**
 * Gather statistics on the distribution of the Java object hash code, specifically how it affects
 * the collision effect in hash tables.
 * Parameters can be set via system properties.
 * 
 * This can be run as a JUnit test or standalone, e.g. java j9vm.test.hashCode.HashCollisionTest
 *
 */
public class HashCollisionTest {
	
	
	/**
	 * Create a bunch of objects and hash them into buckets using the modulus operator.
	 * A collision occurs if two or more objects hash to the same bucket. 
	 * Gather statistics on the distribution of hash values to buckets.
	 * System property hash.collision.buckets overrides the default number of buckets
	 * System property hash.collision.objects overrides the default number of objects
	 */
	@Test
	public void testHashCollisions() {
		int numBuckets = Integer.getInteger("hash.collision.buckets", 98947);
		int objects = Integer.getInteger("hash.collision.objects", 10000000);
		int buckets[] = allocateAndFindCollisions(objects, numBuckets);
		int numEmpty = 0;
		int numUncollided = 0;
		Statistics collisionStats = new Statistics("collisions");
		
		System.out.println("\ntestHashCollisions: hash "+objects+" objects into "+numBuckets+" buckets and count collisions");
		for (int i = 0; i < numBuckets; ++i) {
			int coll = buckets[i] - 1; /* the first entry doesn't count as a collision */
			if (coll > 0) {
				collisionStats.add(coll);
			}
			if (-1 == coll) {
				++numEmpty;
			}
			if (0 == coll) { /* hit this bucket exactly once */
				++numUncollided;
			}
		}
		collisionStats.printStatistics(System.out);
		System.out.println(numEmpty+ " empty buckets "+numUncollided+" uncollided non-empty buckets");
		System.out.println("---------------------------------");
	}

	/**
	 * Create objects and hash them into buckets using the modulus operator until all buckets have at least one entry.
	 * System property hash.fill.buckets overrides the default number of buckets
	 */
	@Test
	public void testTimeToFill() {
		int numBuckets = Integer.getInteger("hash.fill.buckets", 1046527);
		int buckets[] = new int[numBuckets];
		int emptyBuckets = numBuckets;

		for (int i = 0; i < numBuckets; ++i) {
			buckets[i] = 0;
		}
		System.out.println("\ntestTimeToFill: hash objects until we have filled "+numBuckets+" buckets");
		for (long i = 1; i < 32*numBuckets; ++i) {
			Object o = new Object();
			int hash = java.lang.Math.abs(o.hashCode());
			int bucket = hash % numBuckets;
			if (0 == buckets[bucket]) {
				--emptyBuckets;
				++buckets[bucket];
			}
			if (0 == emptyBuckets) {
				System.out.println(numBuckets+" buckets full after "+i+ " objects");
				break;
			} else if ((i % numBuckets) == 0) {
				System.out.println(emptyBuckets+" buckets empty after "+i+ " objects");
			}
		}
		System.out.println("---------------------------------");
	}

	private int[] allocateAndFindCollisions(int objects, int numBuckets) {
		int buckets[] = new int[numBuckets];
		for (int i = 0; i < objects; ++i) {
			Object o = new Object();
			int hash = java.lang.Math.abs(o.hashCode());
			int bucket = hash % numBuckets;
			++buckets[bucket];
		}
		return buckets;
	}
	public static void main(String[] args) {
		HashCollisionTest testObj = new HashCollisionTest();
		testObj.testHashCollisions();
		testObj.testTimeToFill();
	}
}
