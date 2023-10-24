/*******************************************************************************
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
 *******************************************************************************/
package org.openj9.test.lworld;

import static org.testng.Assert.*;
import org.testng.annotations.Test;

/*
 * Instructions to run this test:
 *
 * 1)  Build the JDK with the '--enable-inline-types' configure flag
 * 2)  cd [openj9-openjdk-dir]/openj9/test
 * 3)  git clone https://github.com/adoptium/TKG.git
 * 4)  cd TKG
 * 5)  export TEST_JDK_HOME=[openj9-openjdk-dir]/build/linux-x86_64-server-release/images/jdk
 * 6)  export JDK_VERSION=Valhalla
 * 7)  export SPEC=linux_x86-64_cmprssptrs
 * 8)  export BUILD_LIST=functional/Valhalla
 * 9)  export AUTO_DETECT=false
 * 10) export JDK_IMPL=openj9
 * 11) make -f run_configure.mk && make compile && make _sanity
 */

@Test(groups = { "level.sanity" })
public class ValueTypeTests {
	/* default values */
	static int[] defaultPointPositions1 = {0xFFEEFFEE, 0xAABBAABB};
	static int[] defaultPointPositions2 = {0xCCDDCCDD, 0x33443344};
	static int[] defaultPointPositions3 = {0x43211234, 0xABCDDCBA};
	static int[] defaultPointPositionsEmpty = {0, 0};
	static int[][] defaultLinePositions1 = {defaultPointPositions1, defaultPointPositions2};
	static int[][] defaultLinePositions2 = {defaultPointPositions2, defaultPointPositions3};
	static int[][] defaultLinePositions3 = {defaultPointPositions1, defaultPointPositions3};
	static int[][] defaultLinePositionsEmpty = {defaultPointPositionsEmpty, defaultPointPositionsEmpty};
	static int[][][] defaultTrianglePositions = {defaultLinePositions1, defaultLinePositions2, defaultLinePositions3};
	static int[] defaultPointPositionsNew = {0xFF112233, 0xFF332211};
	static int[][] defaultLinePositionsNew = {defaultPointPositionsNew, defaultPointPositions1};
	static int[][][] defaultTrianglePositionsNew = {defaultLinePositionsNew, defaultLinePositions3, defaultLinePositions1};
	static int[][][] defaultTrianglePositionsEmpty = {defaultLinePositionsEmpty, defaultLinePositionsEmpty, defaultLinePositionsEmpty};

	static value class Point2D {
		int x;
		int y;

		public implicit Point2D();

		Point2D(int x, int y) {
			this.x = x;
			this.y = y;
		}
	}

	static value class Point2DComplex {
		double d;
		long l;

		Point2DComplex(double d, long l) {
			this.d = d;
			this.l = l;
		}
	}

	static value class Line2D {
		Point2D st;
		Point2D en;

		Line2D(Point2D st, Point2D en) {
			this.st = st;
			this.en = en;
		}
	}

	static value class FlattenedLine2D {
		Point2D! st;
		Point2D! en;

		public implicit FlattenedLine2D();

		FlattenedLine2D(Point2D! st, Point2D! en) {
			this.st = st;
			this.en = en;
		}
	}

	static value class Triangle2D {
		FlattenedLine2D! v1;
		FlattenedLine2D! v2;
		FlattenedLine2D! v3;

		public implicit Triangle2D();

		Triangle2D(FlattenedLine2D! v1, FlattenedLine2D! v2, FlattenedLine2D! v3) {
			this.v1 = v1;
			this.v2 = v2;
			this.v3 = v3;
		}

		Triangle2D(int[][][] positions) {
			this(new FlattenedLine2D(new Point2D(positions[0][0][0], positions[0][0][1]),
						new Point2D(positions[0][1][0], positions[0][1][1])),
				new FlattenedLine2D(new Point2D(positions[1][0][0], positions[1][0][1]),
						new Point2D(positions[1][1][0], positions[1][1][1])),
				new FlattenedLine2D(new Point2D(positions[2][0][0], positions[2][0][1]),
						new Point2D(positions[2][1][0], positions[2][1][1])));
		}
	}

	static value class ValueInt {
		int i;

		public implicit ValueInt();

		ValueInt(int i) {
			this.i = i;
		}
	}

	static value class ValueLong {
		long l;

		public implicit ValueLong();

		ValueLong(long l) {
			this.l = l;
		}
	}

	static value class ValueDouble {
		double d;

		public implicit ValueDouble();

		ValueDouble(double d) {
			this.d = d;
		}
	}

	static value class ValueFloat {
		float f;

		public implicit ValueFloat();

		ValueFloat(float f) {
			this.f = f;
		}
	}

	static value class ValueObject {
		Object val;

		public implicit ValueObject();

		ValueObject(Object val) {
			this.val = val;
		}
	 }

	static value record AssortedValueWithLongAlignment
		(Point2D! point, FlattenedLine2D! line, ValueObject! o, ValueLong! l, ValueDouble! d, ValueInt! i, Triangle2D! tri)
	{
	}

	static record AssortedRefWithLongAlignment
		(Point2D! point, FlattenedLine2D! line, ValueObject! o, ValueLong! l, ValueDouble! d, ValueInt! i, Triangle2D! tri)
	{
	}

	static value record AssortedValueWithObjectAlignment
		(Triangle2D! tri, Point2D! point, FlattenedLine2D! line, ValueObject! o, ValueInt! i, ValueFloat! f, Triangle2D! tri2)
	{
	}

	static record AssortedRefWithObjectAlignment
		(Triangle2D! tri, Point2D! point, FlattenedLine2D! line, ValueObject! o, ValueInt! i, ValueFloat! f, Triangle2D! tri2)
	{
	}

	static value record AssortedValueWithSingleAlignment
		(Triangle2D! tri, Point2D! point, FlattenedLine2D! line, ValueInt! i, ValueFloat! f, Triangle2D! tri2)
	{
	}

	static record AssortedRefWithSingleAlignment
		(Triangle2D! tri, Point2D! point, FlattenedLine2D! line, ValueInt! i, ValueFloat! f, Triangle2D! tri2)
	{
	}

	static value class SingleBackfill {
		long l;
		Object o;
		int i;

		public implicit SingleBackfill();

		SingleBackfill(long l, Object o, int i) {
			this.l = l;
			this.o = o;
			this.i = i;
		}
	}

	static value class ObjectBackfill {
		long l;
		Object o;

		public implicit ObjectBackfill();

		ObjectBackfill(long l, Object o) {
			this.l = l;
			this.o = o;
		}
	}

	static value class FlatUnAlignedSingle {
		ValueInt! i;
		ValueInt! i2;

		public implicit FlatUnAlignedSingle();

		FlatUnAlignedSingle(ValueInt! i, ValueInt! i2) {
			this.i = i;
			this.i2 = i2;
		}
	}

	static value class FlatUnAlignedSingleBackfill {
		ValueLong! l;
		FlatUnAlignedSingle! singles;
		ValueObject! o;

		public implicit FlatUnAlignedSingleBackfill();

		FlatUnAlignedSingleBackfill(ValueLong! l, FlatUnAlignedSingle! singles, ValueObject! o) {
			this.l = l;
			this.singles = singles;
			this.o = o;
		}
	}

	static value class FlatUnAlignedSingleBackfill2 {
		ValueLong! l;
		FlatUnAlignedSingle! singles;
		FlatUnAlignedSingle! singles2;

		public implicit FlatUnAlignedSingleBackfill2();

		FlatUnAlignedSingleBackfill2(ValueLong! l, FlatUnAlignedSingle! singles, FlatUnAlignedSingle! singles2) {
			this.l = l;
			this.singles = singles;
			this.singles2 = singles2;

		}
	}

	static value class FlatUnAlignedObject {
		ValueObject! o;
		ValueObject! o2;

		public implicit FlatUnAlignedObject();

		FlatUnAlignedObject(ValueObject! o, ValueObject! o2) {
			this.o = o;
			this.o2 = o2;
		}
	}

	static value class FlatUnAlignedObjectBackfill {
		FlatUnAlignedObject! objects;
		FlatUnAlignedObject! objects2;
		ValueLong! l;

		public implicit FlatUnAlignedObjectBackfill();

		FlatUnAlignedObjectBackfill(FlatUnAlignedObject! objects, FlatUnAlignedObject! objects2, ValueLong! l) {
			this.objects = objects;
			this.objects2 = objects2;
			this.l = l;
		}
	}

	static value class FlatUnAlignedObjectBackfill2 {
		ValueObject! o;
		FlatUnAlignedObject! objects;
		ValueLong! l;

		public implicit FlatUnAlignedObjectBackfill2();

		FlatUnAlignedObjectBackfill2(ValueObject! o, FlatUnAlignedObject! objects, ValueLong! l) {
			this.o = o;
			this.objects = objects;
			this.l = l;
		}
	}

	static value class LargeValueObject {
		/* 16 flattened valuetype members */
		ValueObject! v1;
		ValueObject! v2;
		ValueObject! v3;
		ValueObject! v4;
		ValueObject! v5;
		ValueObject! v6;
		ValueObject! v7;
		ValueObject! v8;
		ValueObject! v9;
		ValueObject! v10;
		ValueObject! v11;
		ValueObject! v12;
		ValueObject! v13;
		ValueObject! v14;
		ValueObject! v15;
		ValueObject! v16;

		public implicit LargeValueObject();
	 }

	 static value class MegaValueObject {
		/* 16 large flattenable valuetypes */
		LargeValueObject! l1;
		LargeValueObject! l2;
		LargeValueObject! l3;
		LargeValueObject! l4;
		LargeValueObject! l5;
		LargeValueObject! l6;
		LargeValueObject! l7;
		LargeValueObject! l8;
		LargeValueObject! l9;
		LargeValueObject! l10;
		LargeValueObject! l11;
		LargeValueObject! l12;
		LargeValueObject! l13;
		LargeValueObject! l14;
		LargeValueObject! l15;
		LargeValueObject! l16;

		MegaValueObject(LargeValueObject! l1, LargeValueObject! l2, LargeValueObject! l3,
			LargeValueObject! l4, LargeValueObject! l5, LargeValueObject! l6, LargeValueObject! l7,
			LargeValueObject! l8, LargeValueObject! l9, LargeValueObject! l10, LargeValueObject! l11,
			LargeValueObject! l12, LargeValueObject! l13, LargeValueObject! l14, LargeValueObject! l15,
			LargeValueObject! l16) {
				this.l1 = l1;
				this.l2 = l2;
				this.l3 = l3;
				this.l4 = l4;
				this.l5 = l5;
				this.l6 = l6;
				this.l7 = l7;
				this.l8 = l8;
				this.l9 = l9;
				this.l10 = l10;
				this.l11 = l11;
				this.l12 = l12;
				this.l13 = l13;
				this.l14 = l14;
				this.l15 = l15;
				this.l16 = l16;
			}
	 }

	static class LargeRefObject {
		/* 16 flattened valuetype members */
		ValueObject! v1;
		ValueObject! v2;
		ValueObject! v3;
		ValueObject! v4;
		ValueObject! v5;
		ValueObject! v6;
		ValueObject! v7;
		ValueObject! v8;
		ValueObject! v9;
		ValueObject! v10;
		ValueObject! v11;
		ValueObject! v12;
		ValueObject! v13;
		ValueObject! v14;
		ValueObject! v15;
		ValueObject! v16;
	 }

	 static class MegaRefObject {
		/* 16 large flattenable valuetypes */
		LargeValueObject! l1;
		LargeValueObject! l2;
		LargeValueObject! l3;
		LargeValueObject! l4;
		LargeValueObject! l5;
		LargeValueObject! l6;
		LargeValueObject! l7;
		LargeValueObject! l8;
		LargeValueObject! l9;
		LargeValueObject! l10;
		LargeValueObject! l11;
		LargeValueObject! l12;
		LargeValueObject! l13;
		LargeValueObject! l14;
		LargeValueObject! l15;
		LargeValueObject! l16;
	 }

	@Test(priority=1)
	static public void testCreatePoint2D() throws Throwable {
		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;

		Point2D point2D = new Point2D(x, y);

		assertEquals(point2D.x, x);
		assertEquals(point2D.y, y);

		// TODO add putfield tests once withfield is replaced
	}

	@Test(priority=1)
	static public void testCreatePoint2DComplex() throws Throwable {
		double d = Double.MAX_VALUE;
		long l = Long.MAX_VALUE;

		Point2DComplex p = new Point2DComplex(d, l);

		assertEquals(p.d, d);
		assertEquals(p.l, l);

		// TODO add putfield tests once withfield is replaced
	}

	@Test(priority=2)
	static public void testCreateLine2D() throws Throwable {
		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;
		int x2 = 0xCCDDCCDD;
		int y2 = 0xAAFFAAFF;

		Point2D st = new Point2D(x, y);
		Point2D en = new Point2D(x2, y2);
		Line2D line2D = new Line2D(st, en);

		assertEquals(line2D.st.x, x);
		assertEquals(line2D.st.y, y);
		assertEquals(line2D.en.x, x2);
		assertEquals(line2D.en.y, y2);

		// TODO add putfield tests once withfield is replaced
	}

	@Test(priority=2)
	static public void testCreateFlattenedLine2D() throws Throwable {
		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;
		int x2 = 0xCCDDCCDD;
		int y2 = 0xAAFFAAFF;

		Point2D! st = new Point2D(x, y);
		Point2D! en = new Point2D(x2, y2);
		FlattenedLine2D line2D = new FlattenedLine2D(st, en);

		assertEquals(line2D.st.x, x);
		assertEquals(line2D.st.y, y);
		assertEquals(line2D.en.x, x2);
		assertEquals(line2D.en.y, y2);

		// TODO add putfield tests once withfield is replaced
	}

	@Test(priority=3)
	static public void testCreateTriangle2D() throws Throwable {
		Triangle2D triangle2D = new Triangle2D(defaultTrianglePositions);
		checkEqualTriangle2D(triangle2D, defaultTrianglePositions);
	}

	@Test(priority=2)
	static public void testCreateArrayPoint2D() throws Throwable {
		int x1 = 0xFFEEFFEE;
		int y1 = 0xAABBAABB;
		int x2 = 0x00000011;
		int y2 = 0xABCDEF00;

		Point2D p1 = new Point2D(x1, y1);
		Point2D p2 = new Point2D(x2, y2);

		Point2D[] a = new Point2D[]{p1, p2};

		assertEquals(a[0].x, p1.x);
		assertEquals(a[0].y, p1.y);
		assertEquals(a[1].x, p2.x);
		assertEquals(a[1].y, p2.y);
	}

	@Test(priority=3, invocationCount=2)
	static public void testCreateArrayFlattenedLine2D() throws Throwable {
		int x = 0xFFEEFFEE;
		int y = 0xAABBAABB;
		int x2 = 0xCCDDCCDD;
		int y2 = 0xAAFFAAFF;
		int x3 = 0xFFABFFCD;
		int y3 = 0xBBAABBAA;
		int x4 = 0xCCBBAADD;
		int y4 = 0xAABBAACC;

		Point2D! st1 = new Point2D(x, y);
		Point2D! en1 = new Point2D(x2, y2);
		FlattenedLine2D line2D_1 = new FlattenedLine2D(st1, en1);

		Point2D! st2 = new Point2D(x3, y3);
		Point2D! en2 = new Point2D(x4, y4);
		FlattenedLine2D line2D_2 = new FlattenedLine2D(st2, en2);

		FlattenedLine2D[] array = new FlattenedLine2D[]{line2D_1, line2D_2};

		assertEquals(array[0].st.x, line2D_1.st.x);
		assertEquals(array[1].st.x, line2D_2.st.x);
		assertEquals(array[0].st.y, line2D_1.st.y);
		assertEquals(array[1].st.y, line2D_2.st.y);
		assertEquals(array[0].en.x, line2D_1.en.x);
		assertEquals(array[1].en.x, line2D_2.en.x);
		assertEquals(array[0].en.y, line2D_1.en.y);
		assertEquals(array[1].en.y, line2D_2.en.y);
	}

	@Test(priority=4, invocationCount=2)
	static public void testCreateArrayTriangle2D() throws Throwable {
		Triangle2D[] array = new Triangle2D[10];
		Triangle2D triangle1 = new Triangle2D(defaultTrianglePositions);
		Triangle2D triangle2 = new Triangle2D(defaultTrianglePositionsNew);
		Triangle2D triangleEmpty = new Triangle2D(defaultTrianglePositionsEmpty);

		array[0] = triangle1;
		array[1] = triangleEmpty;
		array[2] = triangle2;
		array[3] = triangleEmpty;
		array[4] = triangle1;
		array[5] = triangleEmpty;
		array[6] = triangle2;
		array[7]  = triangleEmpty;
		array[8] = triangle1;
		array[9] = triangleEmpty;

		checkEqualTriangle2D(array[0], defaultTrianglePositions);
		checkEqualTriangle2D(array[1], defaultTrianglePositionsEmpty);
		checkEqualTriangle2D(array[2], defaultTrianglePositionsNew);
		checkEqualTriangle2D(array[3], defaultTrianglePositionsEmpty);
		checkEqualTriangle2D(array[4], defaultTrianglePositions);
		checkEqualTriangle2D(array[5], defaultTrianglePositionsEmpty);
		checkEqualTriangle2D(array[6], defaultTrianglePositionsNew);
		checkEqualTriangle2D(array[7], defaultTrianglePositionsEmpty);
		checkEqualTriangle2D(array[8], defaultTrianglePositions);
		checkEqualTriangle2D(array[9], defaultTrianglePositionsEmpty);
	}

	@Test(priority=1)
	static public void testCreateFlattenedValueInt() throws Throwable {
		int i = Integer.MAX_VALUE;
		ValueInt! valueInt = new ValueInt(i);
		assertEquals(valueInt.i, i);
		// TODO add putfield tests once withfield is replaced
	}

	@Test(priority=1)
	static public void testCreateFlattenedValueLong() throws Throwable {
		long l = Long.MAX_VALUE;
		ValueLong! valueLong = new ValueLong(l);
		assertEquals(valueLong.l, l);
		// TODO add putfield tests once withfield is replaced
	}

	@Test(priority=1)
	static public void testCreateFlattenedValueDouble() throws Throwable {
		double d = Double.MAX_VALUE;
		ValueDouble! valueDouble = new ValueDouble(d);
		assertEquals(valueDouble.d, d);
		// TODO add putfield tests once withfield is replaced
	}

	@Test(priority=1)
	static public void testCreateFlattenedValueFloat() throws Throwable {
		float f = Float.MAX_VALUE;
		ValueFloat! valueFloat = new ValueFloat(f);
		assertEquals(valueFloat.f, f);
		// TODO add putfield tests once withfield is replaced
	}

	@Test(priority=1)
	static public void testCreateFlattenedValueObject() throws Throwable {
		Object val = (Object)0xEEFFEEFF;
		ValueObject! valueObject = new ValueObject(val);
		assertEquals(valueObject.val, val);
		// TODO add putfield tests once withfield is replaced
	}

	static void checkEqualPoint2D(Point2D point, int[] positions) throws Throwable {
		assertEquals(point.x, positions[0]);
		assertEquals(point.y, positions[1]);
	}

	static void checkEqualFlattenedLine2D(FlattenedLine2D line, int[][] positions) throws Throwable {
		checkEqualPoint2D(line.st, positions[0]);
		checkEqualPoint2D(line.en, positions[1]);
	}

	static void checkEqualTriangle2D(Triangle2D triangle, int[][][] positions) throws Throwable {
		checkEqualFlattenedLine2D(triangle.v1, positions[0]);
		checkEqualFlattenedLine2D(triangle.v2, positions[1]);
		checkEqualFlattenedLine2D(triangle.v3, positions[2]);
	}
}
