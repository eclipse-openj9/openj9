/*
 * Copyright IBM Corp. and others 2023
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
package org.openj9.test.lworld;

import org.testng.Assert;
import static org.testng.Assert.*;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeClass;


@Test(groups = { "level.sanity" })
public class NullRestrictedTypeOptTests {

	/* A value class with an implicit constructor is eligible to be NullRestricted. */
	public value static class FlattenablePair {
		public final int x, y;

		public implicit FlattenablePair();

		public FlattenablePair(int x, int y) {
			this.x = x; this.y = y;
		}

		public FlattenablePair(FlattenablePair p) {
			this.x = p.x; this.y = p.y;
		}
	}

	public static class EscapeException extends RuntimeException {
		public Object escapingObject;

		public EscapeException(Object o) {
			escapingObject = o;
		}
	}

	public static int result = 0;
	public static FlattenablePair![] parr = new FlattenablePair![3];
	static {
		/* This syntax is a workaround since initializing the values of a NullRestricted array during
		 * its declaration throws an assertion error in the lw5 compiler. */
		parr[0] = new FlattenablePair(3, 4);
		parr[1] = new FlattenablePair(0, 0);
		parr[2] = new FlattenablePair(0, 0);
	}

	@Test(priority=1)
	static public void testEAOpportunity1() throws Throwable {
		for (int i = 0; i < 10000; i++) {
			result = 0;
			for (int j = 0; j < 100; j++) {
				evalTestEA1a(j);
			}
			assertEquals(result, 500);

			result = 0;
			evalTestEA1b();
			assertEquals(result, 500);
		}
	}

	static private void evalTestEA1a(int iter) {
		// Test situation where EA could apply to value p1,
		// but might have to allocate contiguously
		FlattenablePair! p1 = new FlattenablePair(1, 2);
		FlattenablePair! p2 = parr[0];

		FlattenablePair! p3;
		FlattenablePair! p4;

		if (iter % 2 == 0) {
			p3 = p2;
			p4 = p1;
		} else {
			p3 = p1;
			p4 = p2;
		}
		result += p3.x + p4.y;
	}

	static private void evalTestEA1b() {
		for (int j = 0; j < 100; j++) {
			// Test situation where EA could apply to value p1,
			// but might have to allocate contiguously
			FlattenablePair! p1 = new FlattenablePair(1, 2);
			FlattenablePair! p2 = parr[0];

			FlattenablePair! p3;
			FlattenablePair! p4;

			if (j % 2 == 0) {
				p3 = p2;
				p4 = p1;
			} else {
				p3 = p1;
				p4 = p2;
			}
			result += p3.x + p4.y;
		}
	}

	public static FlattenablePair! testEA2Field = new FlattenablePair(0,0);

	@Test(priority=1)
	static public void testEAOpportunity2() throws Throwable {
		for (int i = 0; i < 10000; i++) {
			result = 0;
			evalTestEA2(-1);  // No escape
			assertEquals(result, 200);
			assertEquals(testEA2Field, new FlattenablePair(0,0));
		}
		evalTestEA2(99);  // Escape for index 99
		assertEquals(testEA2Field, new FlattenablePair(2,1));
		evalTestEA2(98);  // Escape for index 98
		assertEquals(testEA2Field, new FlattenablePair(1,2));
	}

	static private void evalTestEA2(int escapePoint) {
		int x = 1; int y = 2;
		int[] nextVal = {0, 2, 1};

		for (int i = 0; i < 100; i++) {
			FlattenablePair! p1 = new FlattenablePair(x, y);
			int updatex = nextVal[x];
			int updatey = nextVal[y];
			x = updatex;
			y = updatey;
			if (p1.x*p1.y != 2 || escapePoint == i) testEA2Field = p1;  // Value might escape

			result += x*y;
		}
	}

	@Test(priority=1)
	static public void testEAOpportunity3() throws Throwable {
		for (int i = 0; i < 1000; i++) {
			result = 0;
			evalTestEA3();
			assertEquals(result, 1000);
		}
	}

	static private void evalTestEA3() {
		for (int i = 0; i < 100; i++) {
			// Test potential stack allocation of array of value type
			FlattenablePair![] arr = new FlattenablePair![2];
			arr[0] = new FlattenablePair(1, 2);
			arr[1] = new FlattenablePair(3, 4);
			for (int j = 0; j < arr.length; j++) {
				FlattenablePair! val = arr[j%arr.length];
				result += val.x + val.y;
			}
		}
	}

	/* A value class with an implicit constructor (eligible to be null-restricted)
	 * with null-restricted value class fields.
	 */
	public value static class NestedFlattenablePair {
		public final FlattenablePair! p1;
		public final FlattenablePair! p2;

		public implicit NestedFlattenablePair();

		public NestedFlattenablePair(int i, int j, int m, int n) {
			this.p1 = new FlattenablePair(i, j);
			this.p2 = new FlattenablePair(m, n);
		}

		public NestedFlattenablePair(FlattenablePair p1, FlattenablePair p2) {
			this.p1 = new FlattenablePair(p1);
			this.p2 = new FlattenablePair(p2);
		}
	}

	// An array whose component type is a null-restricted value class
	public static NestedFlattenablePair![] nestedflattenablearr = new NestedFlattenablePair![1];
	static {
		nestedflattenablearr[0] = new NestedFlattenablePair(11, 12, 13, 14);
	}

	@Test(priority=1)
	static public void testEAOpportunity4() throws Throwable {
		for (int i = 0; i < 10000; i++) {
			result = 0;
			for (int j = 0; j < 100; j++) {
				evalTestEA4a(j);
			}
			assertEquals(result, 1500);

			result = 0;
			evalTestEA4b();
			assertEquals(result, 1500);
		}
	}

	static private void evalTestEA4a(int iter) {
		// Test situation where EA could apply to value p1,
		// but might have to allocate contiguously
		NestedFlattenablePair! p1 = new NestedFlattenablePair(1, 2, 3, 4);
		NestedFlattenablePair! p2 = nestedflattenablearr[0];

		NestedFlattenablePair! p3;
		NestedFlattenablePair! p4;

		if (iter % 2 == 0) {
			p3 = p2;
			p4 = p1;
		} else {
			p3 = p1;
			p4 = p2;
		}
		result += p3.p1.x + p4.p2.y;
	}

	static private void evalTestEA4b() {
		for (int j = 0; j < 100; j++) {
			// Test situation where EA could apply to value p1,
			// but might have to allocate contiguously.  Also,
			// extra challenges for stack allocation in a loop
			NestedFlattenablePair! p1 = new NestedFlattenablePair(1, 2, 3, 4);
			NestedFlattenablePair! p2 = nestedflattenablearr[0];

			NestedFlattenablePair! p3;
			NestedFlattenablePair! p4;

			if (j % 2 == 0) {
				p3 = p1;
				p4 = p2;
			} else {
				p3 = p2;
				p4 = p1;
			}

			result += p3.p1.x + p4.p2.y;
		}
	}

	public static NestedFlattenablePair! testEA5Field = new NestedFlattenablePair(0,0,0,0);

	@Test(priority=1)
	static public void testEAOpportunity5() throws Throwable {
		for (int i = 0; i < 10000; i++) {
			result = 0;
			evalTestEA5();
			assertEquals(result, 2400);
			assertEquals(testEA5Field, new NestedFlattenablePair(0,0,0,0));
		}
	}

	static private void evalTestEA5() {
		int x = 1; int y = 2; int z = 3; int w = 4;

		for (int i = 0; i < 100; i++) {
			NestedFlattenablePair! p = new NestedFlattenablePair(x, y, z, w);
			int updatex = (x-y)*(x-y);
			int updatey = y*(y-x);
			int updatez = (z-w)*(z-w)*z;
			int updatew = w*(w-z);
			x = updatex;
			y = updatey;
			z = updatez;
			w = updatew;
			if (p.p1.x*p.p1.y+p.p2.x*p.p2.y != 14) testEA5Field = p;  // Looks like value might escape (but never actually does)

			result += x*y*z*w;
		}
	}

	@Test(priority=1)
	static public void testEAOpportunity6() throws Throwable {
		for (int i = 0; i < 1000; i++) {
			result = 0;
			evalTestEA6();
			assertEquals(result, 1800);
		}
	}

	static private void evalTestEA6() {
		for (int i = 0; i < 100; i++) {
			// Test potential stack allocation of array of value type
			NestedFlattenablePair![] arr = new NestedFlattenablePair![2];
			arr[0] = new NestedFlattenablePair(1, 2, 3, 4);
			arr[1] = new NestedFlattenablePair(5, 6, 7, 8);
			for (int j = 0; j < arr.length; j++) {
				NestedFlattenablePair! val = arr[j%arr.length];
				result += val.p1.x + val.p2.y;
			}
		}
	}

	public static int sval7 = 0;
	public static FlattenablePair! escape7 = new FlattenablePair(0,0);

	@Test(priority=1)
	static public void testEAOpportunity7() throws Throwable {
		result = 0;
		for (int i = 0; i < 100000; i++) {
			evalTestEA7(i*sval7);
		}
		assertEquals(result, 500000);
		assertEquals(escape7, new FlattenablePair(0,0));

		evalTestEA7(sval7+99);
		assertEquals(result, 500000);
		assertEquals(escape7, new FlattenablePair(1,2));
	}

	static private void evalTestEA7(int i) {
		// Test cold block escape for nested value object
		FlattenablePair! p = new FlattenablePair(1, 2);

		try {
			result += p.x + parr[i].y;
		} catch (ArrayIndexOutOfBoundsException aioobe) {
			escape7 = p;
		}
	}

	public static int sval8 = 0;
	public static NestedFlattenablePair! escape8 = new NestedFlattenablePair(0,0,0,0);

	@Test(priority=1)
	static public void testEAOpportunity8() throws Throwable {
		result = 0;
		for (int i = 0; i < 100000; i++) {
			evalTestEA8(i*sval8);
		}
		assertEquals(result, 1500000);
		assertEquals(escape8, new NestedFlattenablePair(0,0,0,0));

		evalTestEA8(sval8+99);
		assertEquals(result, 1500000);
		assertEquals(escape8, new NestedFlattenablePair(1,2,3,4));
	}

	static private void evalTestEA8(int i) {
		// Test cold block escape for nested null-restricted value objects
		NestedFlattenablePair! p = new NestedFlattenablePair(1, 2, 3, 4);

		try {
			result += p.p1.x + nestedflattenablearr[i].p2.y;
		} catch (ArrayIndexOutOfBoundsException aioobe) {
			escape8 = p;
		}
	}

	public static class TestStoreToNullRestrictedField {
		public FlattenablePair! nullRestrictedInstanceField;
		public static FlattenablePair! nullRestrictedStaticField;

		public void replaceInstanceField(Object val) {
			nullRestrictedInstanceField = (FlattenablePair) val;
		}

		public static void replaceStaticField(Object val) {
			nullRestrictedStaticField = (FlattenablePair) val;
		}
	}

	public static value class TestWithFieldStoreToNullRestrictedField {
		public FlattenablePair! nullRestrictedInstanceField;

		public TestWithFieldStoreToNullRestrictedField(Object val) {
			this.nullRestrictedInstanceField = (FlattenablePair) val;
		}
	}

	@Test
	static public void testStoreNullValueToNullRestrictedInstanceField() throws Throwable {
		TestStoreToNullRestrictedField storeFieldObj = new TestStoreToNullRestrictedField();
		storeFieldObj.replaceInstanceField(new FlattenablePair(1, 2));

		try {
			storeFieldObj.replaceInstanceField(null);
		} catch (NullPointerException npe) {
			return; /* pass */
		}

		Assert.fail("Expect a NullPointerException. No exception or wrong kind of exception thrown");
	}

	@Test
	static public void testStoreNullValueToNullRestrictedStaticField() throws Throwable {
		TestStoreToNullRestrictedField.replaceStaticField(new FlattenablePair(1, 2));

		try {
			TestStoreToNullRestrictedField.replaceStaticField(null);
		} catch (NullPointerException npe) {
			return; /* pass */
		}

		Assert.fail("Expect a NullPointerException. No exception or wrong kind of exception thrown");
	}

	@Test
	static public void testWithFieldStoreToNullRestrictedField() throws Throwable {
		TestWithFieldStoreToNullRestrictedField withFieldObj = new TestWithFieldStoreToNullRestrictedField(new FlattenablePair(1, 2));

		try {
			withFieldObj = new TestWithFieldStoreToNullRestrictedField(null);
		} catch (NullPointerException npe) {
			return; /* pass */
		}

		Assert.fail("Expect a NullPointerException. No exception or wrong kind of exception thrown");
	}
}
