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

import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
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
	static Lookup lookup = MethodHandles.lookup();
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
	static int defaultInt = 0x12123434;
	static long defaultLong = 0xFAFBFCFD11223344L;
	static double defaultDouble = Double.MAX_VALUE;
	static float defaultFloat = Float.MAX_VALUE;
	static Object defaultObject = (Object)0xEEFFEEFF;
	static int defaultIntNew = 0x45456767;
	static long defaultLongNew = 0x11551155AAEEAAEEL;
	static double defaultDoubleNew = -123412341.21341234d;
	static float defaultFloatNew = -123423.12341234f;
	static Object defaultObjectNew = (Object)0xFFEEFFEE;
	/* miscellaneous constants */
	static final int genericArraySize = 10;
	static final int objectGCScanningIterationCount = 1000;

	static value class Point2D {
		int x;
		int y;

		public implicit Point2D();

		Point2D(int x, int y) {
			this.x = x;
			this.y = y;
		}

		Point2D(int[] position) {
			this(position[0], position[1]);
		}

		void checkEqualPoint2D(int[] position) throws Throwable {
			assertEquals(this.x, position[0]);
			assertEquals(this.y, position[1]);
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

		FlattenedLine2D(int[][] positions) {
			this(new Point2D(positions[0]), new Point2D(positions[1]));
		}

		void checkEqualFlattenedLine2D(int[][] positions) throws Throwable {
			st.checkEqualPoint2D(positions[0]);
			en.checkEqualPoint2D(positions[1]);
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

		void checkEqualTriangle2D(int[][][] positions) throws Throwable {
			v1.checkEqualFlattenedLine2D(positions[0]);
			v2.checkEqualFlattenedLine2D(positions[1]);
			v3.checkEqualFlattenedLine2D(positions[2]);
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

	static value class AssortedValueWithLongAlignment {
		Point2D! point;
		FlattenedLine2D! line;
		ValueObject! o;
		ValueLong! l;
		ValueDouble! d;
		ValueInt! i;
		Triangle2D! tri;

		public implicit AssortedValueWithLongAlignment();

		AssortedValueWithLongAlignment(Point2D! point, FlattenedLine2D! line, ValueObject! o, ValueLong! l, ValueDouble! d, ValueInt! i, Triangle2D! tri) {
			this.point = point;
			this.line = line;
			this.o = o;
			this.l = l;
			this.d = d;
			this.i = i;
			this.tri = tri;
		}

		static AssortedValueWithLongAlignment createObjectWithDefaults() {
			return new AssortedValueWithLongAlignment(new Point2D(defaultPointPositions1),
				new FlattenedLine2D(defaultLinePositions1), new ValueObject(defaultObject),
				new ValueLong(defaultLong), new ValueDouble(defaultDouble), new ValueInt(defaultInt),
				new Triangle2D(defaultTrianglePositions));
		}

		AssortedValueWithLongAlignment checkFieldsWithDefaults() throws Throwable {
			point.checkEqualPoint2D(defaultPointPositions1);
			line.checkEqualFlattenedLine2D(defaultLinePositions1);
			assertEquals(o.val, defaultObject);
			assertEquals(l.l, defaultLong);
			assertEquals(d.d, defaultDouble);
			assertEquals(i.i, defaultInt);
			tri.checkEqualTriangle2D(defaultTrianglePositions);
			// TODO add putfield tests once withfield is replaced
			return this; // should be updated to new defaults once putfield tests are here
		}
	}

	static class AssortedRefWithLongAlignment {
		Point2D! point;
		FlattenedLine2D! line;
		ValueObject! o;
		ValueLong! l;
		ValueDouble! d;
		ValueInt! i;
		Triangle2D! tri;

		AssortedRefWithLongAlignment(Point2D! point, FlattenedLine2D! line, ValueObject! o, ValueLong! l, ValueDouble! d, ValueInt! i, Triangle2D! tri) {
			this.point = point;
			this.line = line;
			this.o = o;
			this.l = l;
			this.d = d;
			this.i = i;
			this.tri = tri;
		}

		static AssortedRefWithLongAlignment createObjectWithDefaults() {
			return new AssortedRefWithLongAlignment(new Point2D(defaultPointPositions1),
				new FlattenedLine2D(defaultLinePositions1), new ValueObject(defaultObject),
				new ValueLong(defaultLong), new ValueDouble(defaultDouble), new ValueInt(defaultInt),
				new Triangle2D(defaultTrianglePositions));
		}

		void checkFieldsWithDefaults() throws Throwable {
			point.checkEqualPoint2D(defaultPointPositions1);
			point = new Point2D(defaultPointPositionsNew);
			point.checkEqualPoint2D(defaultPointPositionsNew);

			line.checkEqualFlattenedLine2D(defaultLinePositions1);
			line = new FlattenedLine2D(defaultLinePositionsNew);
			line.checkEqualFlattenedLine2D(defaultLinePositionsNew);

			assertEquals(o.val, defaultObject);
			o = new ValueObject(defaultObjectNew);
			assertEquals(o.val, defaultObjectNew);

			assertEquals(l.l, defaultLong);
			l = new ValueLong(defaultLongNew);
			assertEquals(l.l, defaultLongNew);

			assertEquals(d.d, defaultDouble);
			d = new ValueDouble(defaultDoubleNew);
			assertEquals(d.d, defaultDoubleNew);

			assertEquals(i.i, defaultInt);
			i = new ValueInt(defaultIntNew);
			assertEquals(i.i, defaultIntNew);

			tri.checkEqualTriangle2D(defaultTrianglePositions);
			tri = new Triangle2D(defaultTrianglePositionsNew);
			tri.checkEqualTriangle2D(defaultTrianglePositionsNew);
		}
	}

	static value class AssortedValueWithObjectAlignment {
		Triangle2D! tri;
		Point2D! point;
		FlattenedLine2D! line;
		ValueObject! o;
		ValueInt! i;
		ValueFloat! f;
		Triangle2D! tri2;

		public implicit AssortedValueWithObjectAlignment();

		AssortedValueWithObjectAlignment(Triangle2D! tri, Point2D! point, FlattenedLine2D! line, ValueObject! o, ValueInt! i, ValueFloat! f, Triangle2D! tri2) {
			this.tri = tri;
			this.point = point;
			this.line = line;
			this.o = o;
			this.i = i;
			this.f = f;
			this.tri2 = tri2;
		}

		static AssortedValueWithObjectAlignment createObjectWithDefaults() {
			return new AssortedValueWithObjectAlignment(new Triangle2D(defaultTrianglePositions), new Point2D(defaultPointPositions1),
				new FlattenedLine2D(defaultLinePositions1), new ValueObject(defaultObject),
				new ValueInt(defaultInt), new ValueFloat(defaultFloat),
				new Triangle2D(defaultTrianglePositions));
		}

		AssortedValueWithObjectAlignment checkFieldsWithDefaults() throws Throwable {
			tri.checkEqualTriangle2D(defaultTrianglePositions);
			point.checkEqualPoint2D(defaultPointPositions1);
			line.checkEqualFlattenedLine2D( defaultLinePositions1);
			assertEquals(o.val, defaultObject);
			assertEquals(i.i, defaultInt);
			assertEquals(f.f, defaultFloat);
			tri2.checkEqualTriangle2D(defaultTrianglePositions);
			// TODO add putfield tests once withfield is replaced
			return this; // should be updated to new defaults once putfield tests are here
		}
	}

	static class AssortedRefWithObjectAlignment {
		Triangle2D! tri;
		Point2D! point;
		FlattenedLine2D! line;
		ValueObject! o;
		ValueInt! i;
		ValueFloat! f;
		Triangle2D! tri2;

		AssortedRefWithObjectAlignment(Triangle2D! tri, Point2D! point, FlattenedLine2D! line, ValueObject! o, ValueInt! i, ValueFloat! f, Triangle2D! tri2) {
			this.tri = tri;
			this.point = point;
			this.line = line;
			this.o = o;
			this.i = i;
			this.f = f;
			this.tri2 = tri2;
		}

		static AssortedRefWithObjectAlignment createObjectWithDefaults() {
			return new AssortedRefWithObjectAlignment(new Triangle2D(defaultTrianglePositions), new Point2D(defaultPointPositions1),
				new FlattenedLine2D(defaultLinePositions1), new ValueObject(defaultObject),
				new ValueInt(defaultInt), new ValueFloat(defaultFloat),
				new Triangle2D(defaultTrianglePositions));
		}

		void checkFieldsWithDefaults() throws Throwable {
			tri.checkEqualTriangle2D(defaultTrianglePositions);
			tri = new Triangle2D(defaultTrianglePositionsNew);
			tri.checkEqualTriangle2D(defaultTrianglePositionsNew);

			point.checkEqualPoint2D(defaultPointPositions1);
			point = new Point2D(defaultPointPositionsNew);
			point.checkEqualPoint2D(defaultPointPositionsNew);

			line.checkEqualFlattenedLine2D(defaultLinePositions1);
			line = new FlattenedLine2D(defaultLinePositionsNew);
			line.checkEqualFlattenedLine2D(defaultLinePositionsNew);

			assertEquals(o.val, defaultObject);
			o = new ValueObject(defaultObjectNew);
			assertEquals(o.val, defaultObjectNew);

			assertEquals(i.i, defaultInt);
			i = new ValueInt(defaultIntNew);
			assertEquals(i.i, defaultIntNew);

			assertEquals(f.f, defaultFloat);
			f = new ValueFloat(defaultFloatNew);
			assertEquals(f.f, defaultFloatNew);

			tri2.checkEqualTriangle2D(defaultTrianglePositions);
			tri2 = new Triangle2D(defaultTrianglePositionsNew);
			tri2.checkEqualTriangle2D(defaultTrianglePositionsNew);
		}
	}

	static value class AssortedValueWithSingleAlignment {
		Triangle2D! tri;
		Point2D! point;
		FlattenedLine2D! line;
		ValueInt! i;
		ValueFloat! f;
		Triangle2D! tri2;

		public implicit AssortedValueWithSingleAlignment();

		AssortedValueWithSingleAlignment
			(Triangle2D! tri, Point2D! point, FlattenedLine2D! line, ValueInt! i, ValueFloat! f, Triangle2D! tri2)
		{
			this.tri = tri;
			this.point = point;
			this.line = line;
			this.i = i;
			this.f = f;
			this.tri2 = tri2;
		}

		static AssortedValueWithSingleAlignment createObjectWithDefaults() {
			return new AssortedValueWithSingleAlignment(new Triangle2D(defaultTrianglePositions), new Point2D(defaultPointPositions1),
				new FlattenedLine2D(defaultLinePositions1), new ValueInt(defaultInt), new ValueFloat(defaultFloat),
				new Triangle2D(defaultTrianglePositions));
		}

		void checkFieldsWithDefaults() throws Throwable {
			tri.checkEqualTriangle2D(defaultTrianglePositions);
			point.checkEqualPoint2D(defaultPointPositions1);
			line.checkEqualFlattenedLine2D(defaultLinePositions1);
			assertEquals(i.i, defaultInt);
			assertEquals(f.f, defaultFloat);
			tri2.checkEqualTriangle2D(defaultTrianglePositions);
			// TODO add putfield tests once withfield is replaced
		}
	}

	static class AssortedRefWithSingleAlignment {
		Triangle2D! tri;
		Point2D! point;
		FlattenedLine2D! line;
		ValueInt! i;
		ValueFloat! f;
		Triangle2D! tri2;

		AssortedRefWithSingleAlignment(Triangle2D! tri, Point2D! point, FlattenedLine2D! line, ValueInt! i, ValueFloat! f, Triangle2D! tri2) {
			this.tri = tri;
			this.point = point;
			this.line = line;
			this.i = i;
			this.f = f;
			this.tri2 = tri2;
		}

		static AssortedRefWithSingleAlignment createObjectWithDefaults() {
			return new AssortedRefWithSingleAlignment(new Triangle2D(defaultTrianglePositions), new Point2D(defaultPointPositions1),
				new FlattenedLine2D(defaultLinePositions1), new ValueInt(defaultInt), new ValueFloat(defaultFloat),
				new Triangle2D(defaultTrianglePositions));
		}

		void checkFieldsWithDefaults() throws Throwable {
			tri.checkEqualTriangle2D(defaultTrianglePositions);
			tri = new Triangle2D(defaultTrianglePositionsNew);
			tri.checkEqualTriangle2D(defaultTrianglePositionsNew);

			point.checkEqualPoint2D(defaultPointPositions1);
			point = new Point2D(defaultPointPositionsNew);
			point.checkEqualPoint2D(defaultPointPositionsNew);

			line.checkEqualFlattenedLine2D(defaultLinePositions1);
			line = new FlattenedLine2D(defaultLinePositionsNew);
			line.checkEqualFlattenedLine2D(defaultLinePositionsNew);

			assertEquals(i.i, defaultInt);
			i = new ValueInt(defaultIntNew);
			assertEquals(i.i, defaultIntNew);

			assertEquals(f.f, defaultFloat);
			f = new ValueFloat(defaultFloatNew);
			assertEquals(f.f, defaultFloatNew);

			tri2.checkEqualTriangle2D(defaultTrianglePositions);
			tri2 = new Triangle2D(defaultTrianglePositionsNew);
			tri2.checkEqualTriangle2D(defaultTrianglePositionsNew);
		}
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

		LargeValueObject(ValueObject! v1, ValueObject! v2, ValueObject! v3, ValueObject! v4,
			ValueObject! v5, ValueObject! v6, ValueObject! v7, ValueObject! v8,
			ValueObject! v9, ValueObject! v10, ValueObject! v11, ValueObject! v12,
			ValueObject! v13, ValueObject! v14, ValueObject! v15, ValueObject! v16
		) {
			this.v1 = v1;
			this.v2 = v2;
			this.v3 = v3;
			this.v4 = v4;
			this.v5 = v5;
			this.v6 = v6;
			this.v7 = v7;
			this.v8 = v8;
			this.v9 = v9;
			this.v10 = v10;
			this.v11 = v11;
			this.v12 = v12;
			this.v13 = v13;
			this.v14 = v14;
			this.v15 = v15;
			this.v16 = v16;
		}

		static LargeValueObject createObjectWithDefaults() {
			return createWith(defaultObject);
		}

		static LargeValueObject createObjectWithNewDefaults() {
			return createWith(defaultObjectNew);
		}

		private static LargeValueObject createWith(Object obj) {
			return new LargeValueObject(
				new ValueObject(obj), new ValueObject(obj), new ValueObject(obj), new ValueObject(obj),
				new ValueObject(obj), new ValueObject(obj), new ValueObject(obj), new ValueObject(obj),
				new ValueObject(obj), new ValueObject(obj), new ValueObject(obj), new ValueObject(obj),
				new ValueObject(obj), new ValueObject(obj), new ValueObject(obj), new ValueObject(obj)
			);
		}

		void checkFieldsWithDefaults() {
			ValueObject! obj = new ValueObject(defaultObject);
			assertEquals(v1.val, defaultObject);
			assertEquals(v2.val, defaultObject);
			assertEquals(v3.val, defaultObject);
			assertEquals(v4.val, defaultObject);
			assertEquals(v5.val, defaultObject);
			assertEquals(v6.val, defaultObject);
			assertEquals(v7.val, defaultObject);
			assertEquals(v8.val, defaultObject);
			assertEquals(v9.val, defaultObject);
			assertEquals(v10.val, defaultObject);
			assertEquals(v11.val, defaultObject);
			assertEquals(v12.val, defaultObject);
			assertEquals(v13.val, defaultObject);
			assertEquals(v14.val, defaultObject);
			assertEquals(v15.val, defaultObject);
			assertEquals(v16.val, defaultObject);
			// TODO add putfield tests once withfield is replaced
		}
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

		public implicit MegaValueObject();

		MegaValueObject(LargeValueObject! l1, LargeValueObject! l2, LargeValueObject! l3,
			LargeValueObject! l4, LargeValueObject! l5, LargeValueObject! l6, LargeValueObject! l7,
			LargeValueObject! l8, LargeValueObject! l9, LargeValueObject! l10, LargeValueObject! l11,
			LargeValueObject! l12, LargeValueObject! l13, LargeValueObject! l14, LargeValueObject! l15,
			LargeValueObject! l16
		) {
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

			static MegaValueObject createObjectWithDefaults() {
				return new MegaValueObject(LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(),
					LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(),
					LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(),
					LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults()
				);
			}

			void checkFieldsWithDefaults() {
				l1.checkFieldsWithDefaults();
				l2.checkFieldsWithDefaults();
				l3.checkFieldsWithDefaults();
				l4.checkFieldsWithDefaults();
				l5.checkFieldsWithDefaults();
				l6.checkFieldsWithDefaults();
				l7.checkFieldsWithDefaults();
				l8.checkFieldsWithDefaults();
				l9.checkFieldsWithDefaults();
				l10.checkFieldsWithDefaults();
				l11.checkFieldsWithDefaults();
				l12.checkFieldsWithDefaults();
				l13.checkFieldsWithDefaults();
				l14.checkFieldsWithDefaults();
				l15.checkFieldsWithDefaults();
				l16.checkFieldsWithDefaults();
				// TODO add putfield tests once withfield is replaced
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

		LargeRefObject(ValueObject! v1, ValueObject! v2, ValueObject! v3, ValueObject! v4,
			ValueObject! v5, ValueObject! v6, ValueObject! v7, ValueObject! v8,
			ValueObject! v9, ValueObject! v10, ValueObject! v11, ValueObject! v12,
			ValueObject! v13, ValueObject! v14, ValueObject! v15, ValueObject! v16
		){
			this.v1 = v1;
			this.v2 = v2;
			this.v3 = v3;
			this.v4 = v4;
			this.v5 = v5;
			this.v6 = v6;
			this.v7 = v7;
			this.v8 = v8;
			this.v9 = v9;
			this.v10 = v10;
			this.v11 = v11;
			this.v12 = v12;
			this.v13 = v13;
			this.v14 = v14;
			this.v15 = v15;
			this.v16 = v16;
		}

		static LargeRefObject createObjectWithDefaults() {
			return new LargeRefObject(
				new ValueObject(defaultObject), new ValueObject(defaultObject), new ValueObject(defaultObject), new ValueObject(defaultObject),
				new ValueObject(defaultObject), new ValueObject(defaultObject), new ValueObject(defaultObject), new ValueObject(defaultObject),
				new ValueObject(defaultObject), new ValueObject(defaultObject), new ValueObject(defaultObject), new ValueObject(defaultObject),
				new ValueObject(defaultObject), new ValueObject(defaultObject), new ValueObject(defaultObject), new ValueObject(defaultObject)
			);
		}

		void checkFieldsWithDefaults() {
			checkFieldsWithObject(new ValueObject(defaultObject));

			this.v1 = new ValueObject(defaultObjectNew);
			this.v2 = new ValueObject(defaultObjectNew);
			this.v3 = new ValueObject(defaultObjectNew);
			this.v4 = new ValueObject(defaultObjectNew);
			this.v5 = new ValueObject(defaultObjectNew);
			this.v6 = new ValueObject(defaultObjectNew);
			this.v7 = new ValueObject(defaultObjectNew);
			this.v8 = new ValueObject(defaultObjectNew);
			this.v9 = new ValueObject(defaultObjectNew);
			this.v10 = new ValueObject(defaultObjectNew);
			this.v11 = new ValueObject(defaultObjectNew);
			this.v12 = new ValueObject(defaultObjectNew);
			this.v13 = new ValueObject(defaultObjectNew);
			this.v14 = new ValueObject(defaultObjectNew);
			this.v15 = new ValueObject(defaultObjectNew);
			this.v16 = new ValueObject(defaultObjectNew);

			checkFieldsWithObject(new ValueObject(defaultObjectNew));
		}

		private void checkFieldsWithObject(ValueObject! obj) {
			assertEquals(v1, obj);
			assertEquals(v2, obj);
			assertEquals(v3, obj);
			assertEquals(v4, obj);
			assertEquals(v5, obj);
			assertEquals(v6, obj);
			assertEquals(v7, obj);
			assertEquals(v8, obj);
			assertEquals(v9, obj);
			assertEquals(v10, obj);
			assertEquals(v11, obj);
			assertEquals(v12, obj);
			assertEquals(v13, obj);
			assertEquals(v14, obj);
			assertEquals(v15, obj);
			assertEquals(v16, obj);
		}
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

		MegaRefObject(LargeValueObject! l1, LargeValueObject! l2, LargeValueObject! l3,
			LargeValueObject! l4, LargeValueObject! l5, LargeValueObject! l6, LargeValueObject! l7,
			LargeValueObject! l8, LargeValueObject! l9, LargeValueObject! l10, LargeValueObject! l11,
			LargeValueObject! l12, LargeValueObject! l13, LargeValueObject! l14, LargeValueObject! l15,
			LargeValueObject! l16
		) {
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


		static MegaRefObject createObjectWithDefaults() {
			return new MegaRefObject(LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(),
				LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(),
				LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(),
				LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults(), LargeValueObject.createObjectWithDefaults()
			);
		}

		void checkFieldsWithDefaults() {
			checkFieldsWithObject(LargeValueObject.createObjectWithDefaults());
			this.l1 = LargeValueObject.createObjectWithNewDefaults();
			this.l2 = LargeValueObject.createObjectWithNewDefaults();
			this.l3 = LargeValueObject.createObjectWithNewDefaults();
			this.l4 = LargeValueObject.createObjectWithNewDefaults();
			this.l5 = LargeValueObject.createObjectWithNewDefaults();
			this.l6 = LargeValueObject.createObjectWithNewDefaults();
			this.l7 = LargeValueObject.createObjectWithNewDefaults();
			this.l8 = LargeValueObject.createObjectWithNewDefaults();
			this.l9 = LargeValueObject.createObjectWithNewDefaults();
			this.l10 = LargeValueObject.createObjectWithNewDefaults();
			this.l11 = LargeValueObject.createObjectWithNewDefaults();
			this.l12 = LargeValueObject.createObjectWithNewDefaults();
			this.l13 = LargeValueObject.createObjectWithNewDefaults();
			this.l14 = LargeValueObject.createObjectWithNewDefaults();
			this.l15 = LargeValueObject.createObjectWithNewDefaults();
			this.l16 = LargeValueObject.createObjectWithNewDefaults();
			checkFieldsWithObject(LargeValueObject.createObjectWithNewDefaults());
		}

		private void checkFieldsWithObject(LargeValueObject! obj) {
			assertEquals(l1, obj);
			assertEquals(l2, obj);
			assertEquals(l3, obj);
			assertEquals(l4, obj);
			assertEquals(l5, obj);
			assertEquals(l6, obj);
			assertEquals(l7, obj);
			assertEquals(l8, obj);
			assertEquals(l9, obj);
			assertEquals(l10, obj);
			assertEquals(l11, obj);
			assertEquals(l12, obj);
			assertEquals(l13, obj);
			assertEquals(l14, obj);
			assertEquals(l15, obj);
			assertEquals(l16, obj);
		}
	 }

	//*******************************************************************************
	// Create value objects and check getters/setters
	//*******************************************************************************

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
		triangle2D.checkEqualTriangle2D(defaultTrianglePositions);
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

		array[0].checkEqualTriangle2D(defaultTrianglePositions);
		array[1].checkEqualTriangle2D(defaultTrianglePositionsEmpty);
		array[2].checkEqualTriangle2D(defaultTrianglePositionsNew);
		array[3].checkEqualTriangle2D(defaultTrianglePositionsEmpty);
		array[4].checkEqualTriangle2D(defaultTrianglePositions);
		array[5].checkEqualTriangle2D(defaultTrianglePositionsEmpty);
		array[6].checkEqualTriangle2D(defaultTrianglePositionsNew);
		array[7].checkEqualTriangle2D(defaultTrianglePositionsEmpty);
		array[8].checkEqualTriangle2D(defaultTrianglePositions);
		array[9].checkEqualTriangle2D(defaultTrianglePositionsEmpty);
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

	@Test(priority=2)
	static public void testCreateLargeValueObjectAndMegaValueObject() throws Throwable {
		LargeValueObject large = LargeValueObject.createObjectWithDefaults();
		large.checkFieldsWithDefaults();
		MegaValueObject mega = MegaValueObject.createObjectWithDefaults();
		mega.checkFieldsWithDefaults();
	}

	@Test(priority=3)
	static public void testCreateLargeRefObjectAndMegaRefObject() throws Throwable {
		LargeRefObject large = LargeRefObject.createObjectWithDefaults();
		large.checkFieldsWithDefaults();
		MegaRefObject mega = MegaRefObject.createObjectWithDefaults();
		mega.checkFieldsWithDefaults();
	}

	@Test(priority=5, invocationCount=2)
	static public void testAssortedValueWithLongAlignment() throws Throwable {
		AssortedValueWithLongAlignment a = AssortedValueWithLongAlignment.createObjectWithDefaults();
		a.checkFieldsWithDefaults();
	}

	@Test(priority=5, invocationCount=2)
	static public void testAssortedRefWithLongAlignment() throws Throwable {
		AssortedRefWithLongAlignment a = AssortedRefWithLongAlignment.createObjectWithDefaults();
		a.checkFieldsWithDefaults();
	}

	@Test(priority=5, invocationCount=2)
	static public void testAssortedValueWithObjectAlignment() throws Throwable {
		AssortedValueWithObjectAlignment a = AssortedValueWithObjectAlignment.createObjectWithDefaults();
		a.checkFieldsWithDefaults();
	}

	@Test(priority=5, invocationCount=2)
	static public void testAssortedRefWithObjectAlignment() throws Throwable {
		AssortedRefWithObjectAlignment a = AssortedRefWithObjectAlignment.createObjectWithDefaults();
		a.checkFieldsWithDefaults();
	}

	@Test(priority=5, invocationCount=2)
	static public void testAssortedValueWithSingleAlignment() throws Throwable {
		AssortedValueWithSingleAlignment a = AssortedValueWithSingleAlignment.createObjectWithDefaults();
		a.checkFieldsWithDefaults();
	}

	@Test(priority=5, invocationCount=2)
	static public void testAssortedRefWithSingleAlignment() throws Throwable {
		AssortedRefWithSingleAlignment a = AssortedRefWithSingleAlignment.createObjectWithDefaults();
		a.checkFieldsWithDefaults();
	}

	//*******************************************************************************
	// Value type tests for static fields
	//*******************************************************************************

	static value class ClassWithOnlyStaticFieldsWithSingleAlignment {
		static Triangle2D! tri;
		static Point2D! point;
		static FlattenedLine2D! line;
		static ValueInt! i;
		static ValueFloat! f;
		static Triangle2D! tri2;
	}

	@Test(priority=4)
	static public void testStaticFieldsWithSingleAlignment() throws Throwable {
		ClassWithOnlyStaticFieldsWithSingleAlignment c = new ClassWithOnlyStaticFieldsWithSingleAlignment();
		c.tri = new Triangle2D(defaultTrianglePositions);
		c.point = new Point2D(defaultPointPositions1);
		c.line = new FlattenedLine2D(defaultLinePositions1);
		c.i = new ValueInt(defaultInt);
		c.f = new ValueFloat(defaultFloat);
		c.tri2 = new Triangle2D(defaultTrianglePositions);

		c.tri.checkEqualTriangle2D(defaultTrianglePositions);
		c.point.checkEqualPoint2D(defaultPointPositions1);
		c.line.checkEqualFlattenedLine2D(defaultLinePositions1);
		assertEquals(c.i.i, defaultInt);
		assertEquals(c.f.f, defaultFloat);
		c.tri2.checkEqualTriangle2D(defaultTrianglePositions);
	}

	static value class ClassWithOnlyStaticFieldsWithLongAlignment {
		static Point2D! point;
		static FlattenedLine2D! line;
		static ValueObject! o;
		static ValueLong! l;
		static ValueDouble! d;
		static ValueInt! i;
		static Triangle2D! tri;
	}

	@Test(priority=4)
	static public void testStaticFieldsWithLongAlignment() throws Throwable {
		ClassWithOnlyStaticFieldsWithLongAlignment c = new ClassWithOnlyStaticFieldsWithLongAlignment();
		c.point = new Point2D(defaultPointPositions1);
		c.line = new FlattenedLine2D(defaultLinePositions1);
		c.o = new ValueObject(defaultObject);
		c.l = new ValueLong(defaultLong);
		c.d = new ValueDouble(defaultDouble);
		c.i = new ValueInt(defaultInt);
		c.tri = new Triangle2D(defaultTrianglePositions);

		c.point.checkEqualPoint2D(defaultPointPositions1);
		c.line.checkEqualFlattenedLine2D(defaultLinePositions1);
		assertEquals(c.o.val, defaultObject);
		assertEquals(c.l.l, defaultLong);
		assertEquals(c.d.d, defaultDouble);
		assertEquals(c.i.i, defaultInt);
		c.tri.checkEqualTriangle2D(defaultTrianglePositions);
	}

	static value class ClassWithOnlyStaticFieldsWithObjectAlignment {
		static Triangle2D! tri;
		static Point2D! point;
		static FlattenedLine2D! line;
		static ValueObject! o;
		static ValueInt! i;
		static ValueFloat! f;
		static Triangle2D! tri2;
	}

	@Test(priority=4)
	static public void testStaticFieldsWithObjectAlignment() throws Throwable {
		ClassWithOnlyStaticFieldsWithObjectAlignment c = new ClassWithOnlyStaticFieldsWithObjectAlignment();
		c.tri = new Triangle2D(defaultTrianglePositions);
		c.point = new Point2D(defaultPointPositions1);
		c.line = new FlattenedLine2D(defaultLinePositions1);
		c.o = new ValueObject(defaultObject);
		c.i = new ValueInt(defaultInt);
		c.f = new ValueFloat(defaultFloat);
		c.tri2 = new Triangle2D(defaultTrianglePositions);

		c.tri.checkEqualTriangle2D(defaultTrianglePositions);
		c.point.checkEqualPoint2D(defaultPointPositions1);
		c.line.checkEqualFlattenedLine2D(defaultLinePositions1);
		assertEquals(c.o.val, defaultObject);
		assertEquals(c.i.i, defaultInt);
		assertEquals(c.f.f, defaultFloat);
		c.tri2.checkEqualTriangle2D(defaultTrianglePositions);
	}

	//*******************************************************************************
	// GC tests
	//*******************************************************************************

	@Test(priority=5)
	static public void testGCFlattenedPoint2DArray() throws Throwable {
		int x1 = 0xFFEEFFEE;
		int y1 = 0xAABBAABB;
		Point2D! point2D = new Point2D(x1, y1);
		Point2D![] array = new Point2D![8];

		for (int i = 0; i < 8; i++) {
			array[i] = point2D;
		}

		System.gc();

		Point2D! value = array[0];
	}

	@Test(priority=5)
	static public void testGCFlattenedValueArrayWithSingleAlignment() throws Throwable {
		int size = 4;
		AssortedValueWithSingleAlignment![] array = new AssortedValueWithSingleAlignment![size];

		for (int i = 0; i < size; i++) {
			array[i] = AssortedValueWithSingleAlignment.createObjectWithDefaults();
		}

		System.gc();

		for (int i = 0; i < size; i++) {
			array[i].checkFieldsWithDefaults();
		}
	}

	@Test(priority=5)
	static public void testGCFlattenedValueArrayWithObjectAlignment() throws Throwable {
		int size = 4;
		AssortedValueWithObjectAlignment![] array = new AssortedValueWithObjectAlignment![size];

		for (int i = 0; i < size; i++) {
			array[i] = AssortedValueWithObjectAlignment.createObjectWithDefaults();
		}

		System.gc();

		for (int i = 0; i < size; i++) {
			array[i].checkFieldsWithDefaults();
		}
	}

	@Test(priority=5)
	static public void testGCFlattenedValueArrayWithLongAlignment() throws Throwable {
		AssortedValueWithLongAlignment![] array = new AssortedValueWithLongAlignment![genericArraySize];

		for (int i = 0; i < genericArraySize; i++) {
			array[i] = AssortedValueWithLongAlignment.createObjectWithDefaults();
		}

		System.gc();

		for (int i = 0; i < genericArraySize; i++) {
			array[i].checkFieldsWithDefaults();
		}
	}

	@Test(priority=5)
	static public void testGCFlattenedLargeObjectArray() throws Throwable {
		int size = 4;
		LargeValueObject![] array = new LargeValueObject![size];
		LargeValueObject! obj = LargeValueObject.createObjectWithDefaults();

		for (int i = 0; i < size; i++) {
			array[i] = obj;
		}

		System.gc();

		LargeValueObject! value = array[0];
	}

	@Test(priority=5)
	static public void testGCFlattenedMegaObjectArray() throws Throwable {
		int size = 4;
		MegaValueObject![] array = new MegaValueObject![size];
		MegaValueObject! obj = MegaValueObject.createObjectWithDefaults();

		System.gc();

		for (int i = 0; i < 4; i++) {
			array[i] = obj;
		}
		System.gc();

		MegaValueObject! value = array[0];
	}

	//*******************************************************************************
	// ACMP tests
	//*******************************************************************************
	@Test(priority=2, invocationCount=2)
	static public void testBasicACMPTestOnIdentityTypes() throws Throwable {
		Object identityType1 = new String();
		Object identityType2 = new String();
		Object nullPointer = null;

		/* sanity test on identity classes */
		assertTrue((identityType1 == identityType1), "An identity (==) comparison on the same identityType should always return true");
		assertFalse((identityType2 == identityType1), "An identity (==) comparison on different identityTypes should always return false");
		assertFalse((identityType2 == nullPointer), "An identity (==) comparison on different identityTypes should always return false");
		assertTrue((nullPointer == nullPointer), "An identity (==) comparison on the same identityType should always return true");
		assertFalse((identityType1 != identityType1), "An identity (!=) comparison on the same identityType should always return false");
		assertTrue((identityType2 != identityType1), "An identity (!=) comparison on different identityTypes should always return true");
		assertTrue((identityType2 != nullPointer), "An identity (!=) comparison on different identityTypes should always return true");
		assertFalse((nullPointer != nullPointer), "An identity (!=) comparison on the same identityType should always return false");
	}

	@Test(priority=2, invocationCount=2)
	static public void testBasicACMPTestOnValueTypes() throws Throwable {
		Point2D valueType1 = new Point2D(1, 2);
		Point2D valueType2 = new Point2D(1, 2);
		Point2D newValueType = new Point2D(2, 1);
		Object identityType = new String();
		Object nullPointer = null;

		assertTrue((valueType1 == valueType1), "A substitutability (==) test on the same value should always return true");
		assertTrue((valueType1 == valueType2), "A substitutability (==) test on different value the same contents should always return true");
		assertFalse((valueType1 == newValueType), "A substitutability (==) test on different value should always return false");
		assertFalse((valueType1 == identityType), "A substitutability (==) test on different value with identity type should always return false");
		assertFalse((valueType1 == nullPointer), "A substitutability (==) test on different value with null pointer should always return false");
		assertFalse((valueType1 != valueType1), "A substitutability (!=) test on the same value should always return false");
		assertFalse((valueType1 != valueType2), "A substitutability (!=) test on different value the same contents should always return false");
		assertTrue((valueType1 != newValueType), "A substitutability (!=) test on different value should always return true");
		assertTrue((valueType1 != identityType), "A substitutability (!=) test on different value with identity type should always return true");
		assertTrue((valueType1 != nullPointer), "A substitutability (!=) test on different value with null pointer should always return true");
	}

	@Test(priority=4)
	static public void testACMPTestOnFastSubstitutableValueTypes() throws Throwable {
		Triangle2D valueType1 = new Triangle2D(defaultTrianglePositions);
		Triangle2D valueType2 = new Triangle2D(defaultTrianglePositions);
		Triangle2D newValueType = new Triangle2D(defaultTrianglePositionsNew);
		Object identityType = new String();
		Object nullPointer = null;

		assertTrue((valueType1 == valueType1), "A substitutability (==) test on the same value should always return true");
		assertTrue((valueType1 == valueType2), "A substitutability (==) test on different value the same contents should always return true");
		assertFalse((valueType1 == newValueType), "A substitutability (==) test on different value should always return false");
		assertFalse((valueType1 == identityType), "A substitutability (==) test on different value with identity type should always return false");
		assertFalse((valueType1 == nullPointer), "A substitutability (==) test on different value with null pointer should always return false");
		assertFalse((valueType1 != valueType1), "A substitutability (!=) test on the same value should always return false");
		assertFalse((valueType1 != valueType2), "A substitutability (!=) test on different value the same contents should always return false");
		assertTrue((valueType1 != newValueType), "A substitutability (!=) test on different value should always return true");
		assertTrue((valueType1 != identityType), "A substitutability (!=) test on different value with identity type should always return true");
		assertTrue((valueType1 != nullPointer), "A substitutability (!=) test on different value with null pointer should always return true");
	}

	static value class ValueTypeFastSubVT {
		int x,y,z;
		Object[] arr;

		public implicit ValueTypeFastSubVT();

		ValueTypeFastSubVT(int x, int y, int z, Object[] arr) {
			this.x = x;
			this.y = y;
			this.z = z;
			this.arr = arr;
		}
	}

	@Test(priority=3)
	static public void testACMPTestOnFastSubstitutableValueTypesVer2() throws Throwable {
		/* these VTs will have array refs */
		Object[] arr = {"foo", "bar", "baz"};
		Object[] arr2 = {"foozo", "barzo", "bazzo"};
		ValueTypeFastSubVT valueType1 = new ValueTypeFastSubVT(1, 2, 3, arr);
		ValueTypeFastSubVT valueType2 = new ValueTypeFastSubVT(1, 2, 3, arr);
		ValueTypeFastSubVT newValueType = new ValueTypeFastSubVT(3, 2, 1, arr2);
		Object identityType = new String();
		Object nullPointer = null;

		assertTrue((valueType1 == valueType1), "A substitutability (==) test on the same value should always return true");
		assertTrue((valueType1 == valueType2), "A substitutability (==) test on different value the same contents should always return true");
		assertFalse((valueType1 == newValueType), "A substitutability (==) test on different value should always return false");
		assertFalse((valueType1 == identityType), "A substitutability (==) test on different value with identity type should always return false");
		assertFalse((valueType1 == nullPointer), "A substitutability (==) test on different value with null pointer should always return false");
		assertFalse((valueType1 != valueType1), "A substitutability (!=) test on the same value should always return false");
		assertFalse((valueType1 != valueType2), "A substitutability (!=) test on different value the same contents should always return false");
		assertTrue((valueType1 != newValueType), "A substitutability (!=) test on different value should always return true");
		assertTrue((valueType1 != identityType), "A substitutability (!=) test on different value with identity type should always return true");
		assertTrue((valueType1 != nullPointer), "A substitutability (!=) test on different value with null pointer should always return true");
	}

	static value class Node {
		long l;
		Object next;
		int i;

		Node(long l, Object next, int i) {
			this.l = l;
			this.next = next;
			this.i = i;
		}
	}

	@Test(priority=3)
	static public void testACMPTestOnRecursiveValueTypes() throws Throwable {
		Node list1 = new Node(3L, null, 3);
		Node list2 = new Node(3L, null, 3);
		Node list3sameAs1 = new Node(3L, null, 3);
		Node list4 = new Node(3L, null, 3);
		Node list5null = new Node(3L, null, 3);
		Node list6null = new Node(3L, null, 3);
		Node list7obj = new Node(3L, new Object(), 3);
		Node list8str = new Node(3L, "foo", 3);
		Object identityType = new String();
		Object nullPointer = null;

		for (int i = 0; i < 100; i++) {
			list1 = new Node(3L, list1, i);
			list2 = new Node(3L, list2, i + 1);
			list3sameAs1 = new Node(3L, list3sameAs1, i);
		}
		for (int i = 0; i < 50; i++) {
			list4 = new Node(3L, list4, i);
		}

		assertTrue((list1 == list1), "A substitutability (==) test on the same value should always return true");
		assertTrue((list5null == list5null), "A substitutability (==) test on the same value should always return true");
		assertTrue((list1 == list3sameAs1), "A substitutability (==) test on different value the same contents should always return true");
		assertTrue((list5null == list6null), "A substitutability (==) test on different value the same contents should always return true");
		assertFalse((list1 == list2), "A substitutability (==) test on different value should always return false");
		assertFalse((list1 == list4), "A substitutability (==) test on different value should always return false");
		assertFalse((list1 == list5null), "A substitutability (==) test on different value should always return false");
		assertFalse((list1 == list7obj), "A substitutability (==) test on different value should always return false");
		assertFalse((list1 == list8str), "A substitutability (==) test on different value should always return false");
		assertFalse((list7obj == list8str), "A substitutability (==) test on different value should always return false");
		assertFalse((list1 == identityType), "A substitutability (==) test on different value with identity type should always return false");
		assertFalse((list1 == nullPointer), "A substitutability (==) test on different value with null pointer should always return false");
		assertFalse((list1 != list1), "A substitutability (!=) test on the same value should always return false");
		assertFalse((list5null != list5null), "A substitutability (!=) test on the same value should always return false");
		assertFalse((list1 != list3sameAs1), "A substitutability (!=) test on different value the same contents should always return false");
		assertFalse((list5null != list6null), "A substitutability (!=) test on different value the same contents should always return false");
		assertTrue((list1 != list2), "A substitutability (!=) test on different value should always return true");
		assertTrue((list1 != list4), "A substitutability (!=) test on different value should always return true");
		assertTrue((list1 != list5null), "A substitutability (!=) test on different value should always return true");
		assertTrue((list1 != list7obj), "A substitutability (!=) test on different value should always return true");
		assertTrue((list1 != list8str), "A substitutability (!=) test on different value should always return true");
		assertTrue((list7obj != list8str), "A substitutability (!=) test on different value should always return true");
		assertTrue((list1 != identityType), "A substitutability (!=) test on different value with identity type should always return true");
		assertTrue((list1 != nullPointer), "A substitutability (!=) test on different value with null pointer should always return true");
	}

	@Test(priority=3, invocationCount=2)
	static public void testACMPTestOnValueFloat() throws Throwable {
		ValueFloat! float1 = new ValueFloat(1.1f);
		ValueFloat! float2 = new ValueFloat(-1.1f);
		ValueFloat! float3 = new ValueFloat(12341.112341234f);
		ValueFloat! float4sameAs1 = new ValueFloat(1.1f);
		ValueFloat! nan = new ValueFloat(Float.NaN);
		ValueFloat! nan2 = new ValueFloat(Float.NaN);
		ValueFloat! positiveZero = new ValueFloat(0.0f);
		ValueFloat! negativeZero = new ValueFloat(-0.0f);
		ValueFloat! positiveInfinity = new ValueFloat(Float.POSITIVE_INFINITY);
		ValueFloat! negativeInfinity = new ValueFloat(Float.NEGATIVE_INFINITY);

		assertTrue((float1 == float1), "A substitutability (==) test on the same value should always return true");
		assertTrue((float1 == float4sameAs1), "A substitutability (==) test on different value the same contents should always return true");
		assertTrue((nan == nan2), "A substitutability (==) test on different value the same contents should always return true");
		assertFalse((float1 == float2), "A substitutability (==) test on different value should always return false");
		assertFalse((float1 == float3), "A substitutability (==) test on different value should always return false");
		assertFalse((float1 == nan), "A substitutability (==) test on different value should always return false");
		assertFalse((float1 == positiveZero), "A substitutability (==) test on different value should always return false");
		assertFalse((float1 == negativeZero), "A substitutability (==) test on different value should always return false");
		assertFalse((float1 == positiveInfinity), "A substitutability (==) test on different value should always return false");
		assertFalse((float1 == negativeInfinity), "A substitutability (==) test on different value should always return false");
		assertFalse((positiveInfinity == negativeInfinity), "A substitutability (==) test on different value should always return false");
		assertFalse((positiveZero == negativeZero), "A substitutability (==) test on different value should always return false");
		assertFalse((float1 != float1), "A substitutability (!=) test on the same value should always return false");
		assertFalse((float1 != float4sameAs1), "A substitutability (!=) test on different value the same contents should always return false");
		assertFalse((nan != nan2), "A substitutability (!=) test on different value the same contents should always return false");
		assertTrue((float1 != float2), "A substitutability (!=) test on different value should always return true");
		assertTrue((float1 != float3), "A substitutability (!=) test on different value should always return true");
		assertTrue((float1 != nan), "A substitutability (!=) test on different value should always return true");
		assertTrue((float1 != positiveZero), "A substitutability (!=) test on different value should always return true");
		assertTrue((float1 != negativeZero), "A substitutability (!=) test on different value should always return true");
		assertTrue((float1 != positiveInfinity), "A substitutability (!=) test on different value should always return true");
		assertTrue((float1 != negativeInfinity), "A substitutability (!=) test on different value should always return true");
		assertTrue((positiveInfinity != negativeInfinity), "A substitutability (!=) test on different value should always return true");
		assertTrue((positiveZero != negativeZero), "A substitutability (!=) test on different value should always return true");
	}

	@Test(priority=3, invocationCount=2)
	static public void testACMPTestOnValueDouble() throws Throwable {
		ValueDouble! double1 = new ValueDouble(1.1d);
		ValueDouble! double2 = new ValueDouble(-1.1d);
		ValueDouble! double3 = new ValueDouble(12341.112341234d);
		ValueDouble! double4sameAs1 = new ValueDouble(1.1d);
		ValueDouble! nan = new ValueDouble(Double.NaN);
		ValueDouble! nan2 = new ValueDouble(Double.NaN);
		ValueDouble! positiveZero = new ValueDouble(0.0d);
		ValueDouble! negativeZero = new ValueDouble(-0.0d);
		ValueDouble! positiveInfinity = new ValueDouble(Double.POSITIVE_INFINITY);
		ValueDouble! negativeInfinity = new ValueDouble(Double.NEGATIVE_INFINITY);

		assertTrue((double1 == double1), "A substitutability (==) test on the same value should always return true");
		assertTrue((double1 == double4sameAs1), "A substitutability (==) test on different value the same contents should always return true");
		assertTrue((nan == nan2), "A substitutability (==) test on different value the same contents should always return true");
		assertFalse((double1 == double2), "A substitutability (==) test on different value should always return false");
		assertFalse((double1 == double3), "A substitutability (==) test on different value should always return false");
		assertFalse((double1 == nan), "A substitutability (==) test on different value should always return false");
		assertFalse((double1 == positiveZero), "A substitutability (==) test on different value should always return false");
		assertFalse((double1 == negativeZero), "A substitutability (==) test on different value should always return false");
		assertFalse((double1 == positiveInfinity), "A substitutability (==) test on different value should always return false");
		assertFalse((double1 == negativeInfinity), "A substitutability (==) test on different value should always return false");
		assertFalse((positiveInfinity == negativeInfinity), "A substitutability (==) test on different value should always return false");
		assertFalse((positiveZero == negativeZero), "A substitutability (==) test on different value should always return false");
		assertFalse((double1 != double1), "A substitutability (!=) test on the same value should always return false");
		assertFalse((double1 != double4sameAs1), "A substitutability (!=) test on different value the same contents should always return false");
		assertFalse((nan != nan2), "A substitutability (!=) test on different value the same contents should always return false");
		assertTrue((double1 != double2), "A substitutability (!=) test on different value should always return true");
		assertTrue((double1 != double3), "A substitutability (!=) test on different value should always return true");
		assertTrue((double1 != nan), "A substitutability (!=) test on different value should always return true");
		assertTrue((double1 != positiveZero), "A substitutability (!=) test on different value should always return true");
		assertTrue((double1 != negativeZero), "A substitutability (!=) test on different value should always return true");
		assertTrue((double1 != positiveInfinity), "A substitutability (!=) test on different value should always return true");
		assertTrue((double1 != negativeInfinity), "A substitutability (!=) test on different value should always return true");
		assertTrue((positiveInfinity != negativeInfinity), "A substitutability (!=) test on different value should always return true");
		assertTrue((positiveZero != negativeZero), "A substitutability (!=) test on different value should always return true");
	}

	@Test(priority=6)
	static public void testACMPTestOnAssortedValues() throws Throwable {
		Object assortedValueWithLongAlignment = AssortedValueWithLongAlignment.createObjectWithDefaults();
		Object assortedValueWithLongAlignment2 = AssortedValueWithLongAlignment.createObjectWithDefaults();
		Object assortedValueWithLongAlignment3 = AssortedValueWithLongAlignment.createObjectWithDefaults();
		assortedValueWithLongAlignment3 = ((AssortedValueWithLongAlignment)assortedValueWithLongAlignment3).checkFieldsWithDefaults();

		Object assortedValueWithObjectAlignment = AssortedValueWithObjectAlignment.createObjectWithDefaults();
		Object assortedValueWithObjectAlignment2 = AssortedValueWithObjectAlignment.createObjectWithDefaults();
		Object assortedValueWithObjectAlignment3 = AssortedValueWithObjectAlignment.createObjectWithDefaults();
		assortedValueWithObjectAlignment3 = ((AssortedValueWithObjectAlignment)assortedValueWithObjectAlignment3).checkFieldsWithDefaults();

		// TODO add commented out tests once withfield is replaced
		assertTrue((assortedValueWithLongAlignment == assortedValueWithLongAlignment), "A substitutability (==) test on the same value should always return true");
		assertTrue((assortedValueWithObjectAlignment == assortedValueWithObjectAlignment), "A substitutability (==) test on the same value should always return true");
		assertTrue((assortedValueWithLongAlignment == assortedValueWithLongAlignment2), "A substitutability (==) test on different value the same contents should always return true");
		assertTrue((assortedValueWithObjectAlignment == assortedValueWithObjectAlignment2), "A substitutability (==) test on different value the same contents should always return true");
		//assertFalse((assortedValueWithLongAlignment == assortedValueWithLongAlignment3), "A substitutability (==) test on different value should always return false");
		assertFalse((assortedValueWithLongAlignment == assortedValueWithObjectAlignment), "A substitutability (==) test on different value should always return false");
		//assertFalse((assortedValueWithObjectAlignment == assortedValueWithObjectAlignment3), "A substitutability (==) test on different value should always return false");
		assertFalse((assortedValueWithLongAlignment != assortedValueWithLongAlignment), "A substitutability (!=) test on the same value should always return false");
		assertFalse((assortedValueWithObjectAlignment != assortedValueWithObjectAlignment), "A substitutability (!=) test on the same value should always return false");
		assertFalse((assortedValueWithLongAlignment != assortedValueWithLongAlignment2), "A substitutability (!=) test on different value the same contents should always return false");
		assertFalse((assortedValueWithObjectAlignment != assortedValueWithObjectAlignment2), "A substitutability (!=) test on different value the same contents should always return false");
		//assertTrue((assortedValueWithLongAlignment != assortedValueWithLongAlignment3), "A substitutability (!=) test on different value the same contents should always return false");
		assertTrue((assortedValueWithLongAlignment != assortedValueWithObjectAlignment), "A substitutability (!=) test on different value the same contents should always return false");
		//assertTrue((assortedValueWithObjectAlignment != assortedValueWithObjectAlignment3), "A substitutability (!=) test on different value the same contents should always return false");
	}

	// *******************************************************************************
	// Value type synchronization tests
	// *******************************************************************************

	@Test(priority=2)
	static public void testMonitorEnterAndExitOnValueType() throws Throwable {
		int x = 1;
		int y = 1;
		Object valueType = new Point2D(x, y);
		Object[] valueTypeArray = new FlattenedLine2D[genericArraySize];

		Class<?> testMonitorExitOnValueType = ValueTypeGenerator.generateRefClass("TestMonitorEnterAndExitOnValueType");
		MethodHandle monitorEnterAndExitOnValueType = lookup.findStatic(testMonitorExitOnValueType, "testMonitorEnterAndExitWithRefType", MethodType.methodType(void.class, Object.class));
		try {
			monitorEnterAndExitOnValueType.invoke(valueType);
			fail("should throw exception. MonitorExit cannot be used with ValueType");
		} catch (IllegalMonitorStateException e) {}

		try {
			monitorEnterAndExitOnValueType.invoke(valueTypeArray);
		} catch (IllegalMonitorStateException e) {
			fail("Should not throw exception. MonitorExit can be used with ValueType arrays");
		}
	}

	@Test(priority=2)
	static public void testSynchMethodsOnValueTypes() throws Throwable {
		try {
			ValueTypeGenerator.generateIllegalValueClassWithSychMethods("TestSynchMethodsOnValueTypesIllegal");
			fail("should throw exception. Synchronized methods cannot be used with ValueType");
		} catch (ClassFormatError e) {}

		Class<?> clazz = ValueTypeGenerator.generateValueClass("TestSynchMethodsOnValueTypes");
		MethodHandle staticSyncMethod = lookup.findStatic(clazz, "staticSynchronizedMethodReturnInt", MethodType.methodType(int.class));

		try {
			staticSyncMethod.invoke();
		} catch (ClassFormatError e) {
			fail("should not throw exception. Synchronized static methods can be used with ValueType");
		}
	}

	@Test(priority=2)
	static public void testSynchMethodsOnRefTypes() throws Throwable {
		Class<?> refTypeClass = ValueTypeGenerator.generateRefClass("TestSynchMethodsOnRefTypes");
		MethodHandle makeRef = lookup.findStatic(refTypeClass, "makeRef", MethodType.methodType(refTypeClass));
		Object refType = makeRef.invoke();

		MethodHandle syncMethod = lookup.findVirtual(refTypeClass, "synchronizedMethodReturnInt", MethodType.methodType(int.class));
		MethodHandle staticSyncMethod = lookup.findStatic(refTypeClass, "staticSynchronizedMethodReturnInt", MethodType.methodType(int.class));

		try {
			syncMethod.invoke(refType);
		} catch (IllegalMonitorStateException e) {
			fail("should not throw exception. Synchronized static methods can be used with RefType");
		}

		try {
			staticSyncMethod.invoke();
		} catch (IllegalMonitorStateException e) {
			fail("should not throw exception. Synchronized static methods can be used with RefType");
		}
	}

	@Test(priority=1)
	static public void testMonitorExitWithRefType() throws Throwable {
		int x = 1;
		Object refType = (Object) x;

		Class<?> testMonitorExitWithRefType = ValueTypeGenerator.generateRefClass("TestMonitorExitWithRefType");
		MethodHandle monitorExitWithRefType = lookup.findStatic(testMonitorExitWithRefType, "testMonitorExitOnObject", MethodType.methodType(void.class, Object.class));
		try {
			monitorExitWithRefType.invoke(refType);
			fail("should throw exception. MonitorExit doesn't have a matching MonitorEnter");
		} catch (IllegalMonitorStateException e) {}
	}

	@Test(priority=1)
	static public void testMonitorEnterAndExitWithRefType() throws Throwable {
		int x = 2;
		Object refType = (Object) x;

		Class<?> testMonitorEnterAndExitWithRefType = ValueTypeGenerator.generateRefClass("TestMonitorEnterAndExitWithRefType");
		MethodHandle monitorEnterAndExitWithRefType = lookup.findStatic(testMonitorEnterAndExitWithRefType, "testMonitorEnterAndExitWithRefType", MethodType.methodType(void.class, Object.class));
		try {
			monitorEnterAndExitWithRefType.invoke(refType);
		} catch (IllegalMonitorStateException e) {
			fail("shouldn't throw exception. MonitorEnter and MonitorExit should be used with refType");
		}
	}

	static class ClassWithValueTypeVolatile {
		ValueInt! i;
		ValueInt! i2;
		Point2D! point; // 8 bytes, will be flattened.
		volatile Point2D! vpoint; // volatile 8 bytes, will be flattened.
		FlattenedLine2D! line; // 16 bytes, will be flattened.
		volatile FlattenedLine2D! vline; // volatile 16 bytes, will not be flattened.
		ClassWithValueTypeVolatile(ValueInt! i, ValueInt! i2, Point2D! point, Point2D! vpoint,
			FlattenedLine2D! line, FlattenedLine2D! vline) {
				this.i = i;
				this.i2 = i2;
				this.point = point;
				this.vpoint = vpoint;
				this.line = line;
				this.vline = vline;
		}

		static ClassWithValueTypeVolatile createObjectWithDefaults() {
			return new ClassWithValueTypeVolatile(new ValueInt(defaultInt), new ValueInt(defaultInt),
				new Point2D(defaultPointPositions1), new Point2D(defaultPointPositions1),
				new FlattenedLine2D(defaultLinePositions1), new FlattenedLine2D(defaultLinePositions1)
			);
		}

		void checkFieldsWithDefaults() throws Throwable {
			assertEquals(i.i, defaultInt);
			i = new ValueInt(defaultIntNew);
			assertEquals(i.i, defaultIntNew);

			assertEquals(i2.i, defaultInt);
			i2 = new ValueInt(defaultIntNew);
			assertEquals(i2.i, defaultIntNew);

			point.checkEqualPoint2D(defaultPointPositions1);
			point = new Point2D(defaultPointPositionsNew);
			point.checkEqualPoint2D(defaultPointPositionsNew);

			vpoint.checkEqualPoint2D(defaultPointPositions1);
			vpoint = new Point2D(defaultPointPositionsNew);
			vpoint.checkEqualPoint2D(defaultPointPositionsNew);

			line.checkEqualFlattenedLine2D(defaultLinePositions1);
			line = new FlattenedLine2D(defaultLinePositionsNew);
			line.checkEqualFlattenedLine2D(defaultLinePositionsNew);

			vline.checkEqualFlattenedLine2D(defaultLinePositions1);
			vline = new FlattenedLine2D(defaultLinePositionsNew);
			vline.checkEqualFlattenedLine2D(defaultLinePositionsNew);
		}
	}

	@Test(priority=3)
	static public void createClassWithVolatileValueTypeFields() throws Throwable {
		ClassWithValueTypeVolatile c = ClassWithValueTypeVolatile.createObjectWithDefaults();
		c.checkFieldsWithDefaults();
	}
}
