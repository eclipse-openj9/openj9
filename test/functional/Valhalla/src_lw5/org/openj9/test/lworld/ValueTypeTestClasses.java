/*
 * Copyright IBM Corp. and others 2021
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

import static org.testng.Assert.*;

public class ValueTypeTestClasses {
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
	static long defaultLongNew2 = 0x22662266BBFFBBFFL;
	static long defaultLongNew3 = 0x33773377CC00CC00L;
	static long defaultLongNew4 = 0x44884488DD11DD11L;
	static long defaultLongNew5 = 0x55995599EE22EE22L;
	static double defaultDoubleNew = -123412341.21341234d;
	static float defaultFloatNew = -123423.12341234f;
	static Object defaultObjectNew = (Object)0xFFEEFFEE;

	static value class ZeroSizeValueType {
		public implicit ZeroSizeValueType();
	}

	static value class ZeroSizeValueTypeWrapper {
		ZeroSizeValueType! z;

		public implicit ZeroSizeValueTypeWrapper();
	}

	static class ClassTypePoint2D {
		final int x, y;

		ClassTypePoint2D(int x, int y) {
			this.x = x;
			this.y = y;
		}
	}

	static value class ValueTypePoint2D {
		final ValueInt! x, y;

		public implicit ValueTypePoint2D();

		ValueTypePoint2D(ValueInt! x, ValueInt! y) {
			this.x = x;
			this.y = y;
		}
	}

	static value class ValueTypeLongPoint2D {
		final ValueLong! x, y;

		public implicit ValueTypeLongPoint2D();

		ValueTypeLongPoint2D(long x, long y) {
			this.x = new ValueLong(x);
			this.y = new ValueLong(y);
		}
	}

	static value class ValueInt2 {
		int i;
		public implicit ValueInt2();
		ValueInt2(int i) { this.i = i; }
	}

	static class IntWrapper {
		IntWrapper(int i) { this.vti = new ValueInt(i); }
		final ValueInt! vti;
	}

	static value class ValueTypeWithLongField {
		ValueLong! l;

		public implicit ValueTypeWithLongField();
	}

	static value class ValueClassInt {
		int i;
		ValueClassInt(int i) { this.i = i; }
	}

	static value class ValueClassPoint2D {
		ValueClassInt x, y;

		ValueClassPoint2D(ValueClassInt x, ValueClassInt y) {
			this.x = x;
			this.y = y;
		}
	}


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

		void checkFieldAccessWithDefaultValues() throws Throwable {
			checkEqualPoint2D(defaultPointPositions1);
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

		void checkFieldAccessWithDefaultValues() throws Throwable {
			checkEqualFlattenedLine2D(defaultLinePositions1);
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


		void checkFieldAccessWithDefaultValues() throws Throwable {
			checkEqualTriangle2D(defaultTrianglePositions);
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

		public long getL() {
			return l;
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

		void checkFieldsWithDefaults() throws Throwable {
			point.checkEqualPoint2D(defaultPointPositions1);
			line.checkEqualFlattenedLine2D(defaultLinePositions1);
			assertEquals(o.val, defaultObject);
			assertEquals(l.l, defaultLong);
			assertEquals(d.d, defaultDouble);
			assertEquals(i.i, defaultInt);
			tri.checkEqualTriangle2D(defaultTrianglePositions);
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

		AssortedRefWithLongAlignment() {}

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

		void checkFieldsWithDefaults() throws Throwable {
			tri.checkEqualTriangle2D(defaultTrianglePositions);
			point.checkEqualPoint2D(defaultPointPositions1);
			line.checkEqualFlattenedLine2D( defaultLinePositions1);
			assertEquals(o.val, defaultObject);
			assertEquals(i.i, defaultInt);
			assertEquals(f.f, defaultFloat);
			tri2.checkEqualTriangle2D(defaultTrianglePositions);
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

		static LargeValueObject createWith(Object obj) {
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

	static value class FlatSingleBackfill {
		ValueLong! l;
		ValueObject! o;
		ValueInt! i;

		public implicit FlatSingleBackfill();

		FlatSingleBackfill(ValueLong! l, ValueObject! o, ValueInt! i) {
			this.l = l;
			this.o = o;
			this.i = i;
		}
	}

	static value class FlatObjectBackfill {
		ValueLong! l;
		ValueObject! o;

		public implicit FlatObjectBackfill();

		FlatObjectBackfill(ValueLong! l, ValueObject! o) {
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

	static value class ValueTypeDoubleLong {
		final ValueLong! l;
		final long l2;
		public implicit ValueTypeDoubleLong();
		ValueTypeDoubleLong(ValueLong l, long l2) {
			this.l = l;
			this.l2 = l2;
		}
		public ValueLong getL() {
			return l;
		}
		public long getL2() {
			return l2;
		}
	}

	static value class ValueTypeQuadLong {
		final ValueTypeDoubleLong! l;
		final ValueLong! l2;
		final long l3;
		public implicit ValueTypeQuadLong();
		ValueTypeQuadLong(ValueTypeDoubleLong l, ValueLong l2, long l3) {
			this.l = l;
			this.l2 = l2;
			this.l3 = l3;
		}
		public ValueTypeDoubleLong getL() {
			return l;
		}
		public ValueLong getL2() {
			return l2;
		}
		public long getL3() {
			return l3;
		}
	}

	static value class ValueTypeDoubleQuadLong {
		final ValueTypeQuadLong! l;
		final ValueTypeDoubleLong! l2;
		final ValueLong! l3;
		final long l4;
		public implicit ValueTypeDoubleQuadLong();
		ValueTypeDoubleQuadLong(ValueTypeQuadLong l, ValueTypeDoubleLong l2, ValueLong l3, long l4) {
			this.l = l;
			this.l2 = l2;
			this.l3 = l3;
			this.l4 = l4;
		}
		public ValueTypeQuadLong getL() {
			return l;
		}
		public ValueTypeDoubleLong getL2() {
			return l2;
		}
		public ValueLong getL3() {
			return l3;
		}
		public long getL4() {
			return l4;
		}
	}

	public static abstract class HeavyAbstractClass {
		/* Abstract class that has fields */
		private int x;
		private int y;
		public int getX(){
			return x;
		}
		public int getY(){
			return y;
		}
	}

	public static abstract class HeavyAbstractClass1 {
		/* Abstract class that has non-empty constructors */
		public HeavyAbstractClass1() {
			System.out.println("This class is HeavyAbstractClass1");
		}
	}

	public static abstract class HeavyAbstractClass2 {
		/* Abstract class that has synchronized methods */
		public synchronized void printClassName() {
			System.out.println("This class is HeavyAbstractClass2");
		}
	}

	public static abstract class HeavyAbstractClass3 {
		/* Abstract class that has initializer */
		{ System.out.println("This class is HeavyAbstractClass3"); }
	}

	public static abstract class LightAbstractClass {

	}

	static abstract class InvisibleLightAbstractClass {

	}

	static value class NestedA {
		int x;
		int y;

		public implicit NestedA();
	}

	static value class NestedB {
		NestedA! a;
		NestedA! b;

		public implicit NestedB();
	}

	static value class ContainerC {
		NestedB! c;
		NestedB! d;

		public implicit ContainerC();
	}

	static class UnresolvedClassDesc {
		public String name;
		public String[] fields;
		public UnresolvedClassDesc(String name, String[] fields) {
			this.name = name;
			this.fields = fields;
		}
	}

	static class IdentityObject {
		long longField;
	}

	static class TestIsValueOnRefArrayClass {
		long longField;
	}

	static value class ValueIntPoint2D {
		final ValueInt x, y;

		ValueIntPoint2D(ValueInt x, ValueInt y) {
			this.x = x;
			this.y = y;
		}
	}

	static value class ClassWithOnlyStaticFieldsWithSingleAlignment {
		static Triangle2D! tri;
		static Point2D! point;
		static FlattenedLine2D! line;
		static ValueInt! i;
		static ValueFloat! f;
		static Triangle2D! tri2;
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

	static value class ClassWithOnlyStaticFieldsWithObjectAlignment {
		static Triangle2D! tri;
		static Point2D! point;
		static FlattenedLine2D! line;
		static ValueObject! o;
		static ValueInt! i;
		static ValueFloat! f;
		static Triangle2D! tri2;
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

	interface TestInterface {}
}
