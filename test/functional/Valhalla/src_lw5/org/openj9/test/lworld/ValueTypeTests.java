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

import static org.objectweb.asm.Opcodes.*;
import static org.openj9.test.lworld.ValueTypeTestClasses.*;
import static org.testng.Assert.*;

import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import java.lang.reflect.Array;
import java.util.ArrayList;
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
	/* miscellaneous constants */
	static final int genericArraySize = 10;
	static final int objectGCScanningIterationCount = 1000;

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
	}

	@Test(priority=1)
	static public void testCreatePoint2DComplex() throws Throwable {
		double d = Double.MAX_VALUE;
		long l = Long.MAX_VALUE;

		Point2DComplex p = new Point2DComplex(d, l);

		assertEquals(p.d, d);
		assertEquals(p.l, l);
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
	}

	@Test(priority=1)
	static public void testCreateFlattenedValueLong() throws Throwable {
		long l = Long.MAX_VALUE;
		ValueLong! valueLong = new ValueLong(l);
		assertEquals(valueLong.l, l);
	}

	@Test(priority=1)
	static public void testCreateFlattenedValueDouble() throws Throwable {
		double d = Double.MAX_VALUE;
		ValueDouble! valueDouble = new ValueDouble(d);
		assertEquals(valueDouble.d, d);
	}

	@Test(priority=1)
	static public void testCreateFlattenedValueFloat() throws Throwable {
		float f = Float.MAX_VALUE;
		ValueFloat! valueFloat = new ValueFloat(f);
		assertEquals(valueFloat.f, f);
	}

	@Test(priority=1)
	static public void testCreateFlattenedValueObject() throws Throwable {
		Object val = (Object)0xEEFFEEFF;
		ValueObject! valueObject = new ValueObject(val);
		assertEquals(valueObject.val, val);
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

	@Test(priority=3, invocationCount=2)
	static public void testLayoutsWithNullRestrictedFields() throws Throwable {
		SingleBackfill! singleBackfillInstance = new SingleBackfill(defaultLong, defaultObject, defaultInt);
		assertEquals(singleBackfillInstance.i, defaultInt);
		assertEquals(singleBackfillInstance.o, defaultObject);
		assertEquals(singleBackfillInstance.l, defaultLong);

		ObjectBackfill! objectBackfillInstance = new ObjectBackfill(defaultLong, defaultObject);
		assertEquals(objectBackfillInstance.o, defaultObject);
		assertEquals(objectBackfillInstance.l, defaultLong);
	}

	@Test(priority=5, invocationCount=2)
	static public void testFlatLayoutsWithValueTypes() throws Throwable {
		FlatSingleBackfill! flatSingleBackfillInstance = new FlatSingleBackfill(new ValueLong(defaultLong), new ValueObject(defaultObject), new ValueInt(defaultInt));
		assertEquals(flatSingleBackfillInstance.i.i, defaultInt);
		assertEquals(flatSingleBackfillInstance.o.val, defaultObject);
		assertEquals(flatSingleBackfillInstance.l.l, defaultLong);

		FlatObjectBackfill! objectBackfillInstance = new FlatObjectBackfill(new ValueLong(defaultLong), new ValueObject(defaultObject));
		assertEquals(objectBackfillInstance.o.val, defaultObject);
		assertEquals(objectBackfillInstance.l.l, defaultLong);

		FlatUnAlignedSingleBackfill! flatUnAlignedSingleBackfillInstance =
			new FlatUnAlignedSingleBackfill(new ValueLong(defaultLong), new FlatUnAlignedSingle(new ValueInt(defaultInt), new ValueInt(defaultIntNew)), new ValueObject(defaultObject));
		assertEquals(flatUnAlignedSingleBackfillInstance.l.l, defaultLong);
		assertEquals(flatUnAlignedSingleBackfillInstance.o.val, defaultObject);
		assertEquals(flatUnAlignedSingleBackfillInstance.singles.i.i, defaultInt);
		assertEquals(flatUnAlignedSingleBackfillInstance.singles.i2.i, defaultIntNew);

		FlatUnAlignedSingleBackfill2! flatUnAlignedSingleBackfill2Instance =
			new FlatUnAlignedSingleBackfill2(new ValueLong(defaultLong), new FlatUnAlignedSingle(new ValueInt(defaultInt), new ValueInt(defaultIntNew)), new FlatUnAlignedSingle(new ValueInt(defaultInt), new ValueInt(defaultIntNew)));
		assertEquals(flatUnAlignedSingleBackfill2Instance.l.l, defaultLong);
		assertEquals(flatUnAlignedSingleBackfill2Instance.singles.i.i, defaultInt);
		assertEquals(flatUnAlignedSingleBackfill2Instance.singles.i2.i, defaultIntNew);
		assertEquals(flatUnAlignedSingleBackfill2Instance.singles2.i.i, defaultInt);
		assertEquals(flatUnAlignedSingleBackfill2Instance.singles2.i2.i, defaultIntNew);

		FlatUnAlignedObjectBackfill! flatUnAlignedObjectBackfillInstance =
			new FlatUnAlignedObjectBackfill(new FlatUnAlignedObject(new ValueObject(defaultObject), new ValueObject(defaultObjectNew)),
				new FlatUnAlignedObject(new ValueObject(defaultObject), new ValueObject(defaultObjectNew)), new ValueLong(defaultLong));
		assertEquals(flatUnAlignedObjectBackfillInstance.l.l, defaultLong);
		assertEquals(flatUnAlignedObjectBackfillInstance.objects.o.val, defaultObject);
		assertEquals(flatUnAlignedObjectBackfillInstance.objects.o2.val, defaultObjectNew);
		assertEquals(flatUnAlignedObjectBackfillInstance.objects2.o.val, defaultObject);
		assertEquals(flatUnAlignedObjectBackfillInstance.objects2.o2.val, defaultObjectNew);

		FlatUnAlignedObjectBackfill2! flatUnAlignedObjectBackfill2Instance =
			new FlatUnAlignedObjectBackfill2(new ValueObject(defaultObject), new FlatUnAlignedObject(new ValueObject(defaultObject), new ValueObject(defaultObjectNew)), new ValueLong(defaultLong));
		assertEquals(flatUnAlignedObjectBackfill2Instance.l.l, defaultLong);
		assertEquals(flatUnAlignedObjectBackfill2Instance.objects.o.val, defaultObject);
		assertEquals(flatUnAlignedObjectBackfill2Instance.objects.o2.val, defaultObjectNew);
		assertEquals(flatUnAlignedObjectBackfill2Instance.o.val, defaultObject);
	}

	@Test(priority=4, invocationCount=2)
	static public void testFlatLayoutsWithRecursiveLongs() throws Throwable {
		ValueTypeDoubleLong! doubleLongInstance = new ValueTypeDoubleLong(new ValueLong(defaultLong), defaultLongNew);
		assertEquals(doubleLongInstance.getL().getL(), defaultLong);
		assertEquals(doubleLongInstance.getL2(), defaultLongNew);

		ValueTypeQuadLong! quadLongInstance = new ValueTypeQuadLong(doubleLongInstance, new ValueLong(defaultLongNew2), defaultLongNew3);
		assertEquals(quadLongInstance.getL().getL().getL(), defaultLong);
		assertEquals(quadLongInstance.getL().getL2(), defaultLongNew);
		assertEquals(quadLongInstance.getL2().getL(), defaultLongNew2);
		assertEquals(quadLongInstance.getL3(), defaultLongNew3);

		ValueTypeDoubleQuadLong! doubleQuadLongInstance = new ValueTypeDoubleQuadLong(quadLongInstance, doubleLongInstance, new ValueLong(defaultLongNew4), defaultLongNew5);

		assertEquals(doubleQuadLongInstance.getL().getL().getL().getL(), defaultLong);
		assertEquals(doubleQuadLongInstance.getL().getL().getL2(), defaultLongNew);
		assertEquals(doubleQuadLongInstance.getL().getL2().getL(), defaultLongNew2);
		assertEquals(doubleQuadLongInstance.getL().getL3(), defaultLongNew3);
		assertEquals(doubleQuadLongInstance.getL2().getL().getL(), defaultLong);
		assertEquals(doubleQuadLongInstance.getL2().getL2(), defaultLongNew);
		assertEquals(doubleQuadLongInstance.getL3().getL(), defaultLongNew4);
		assertEquals(doubleQuadLongInstance.getL4(), defaultLongNew5);
	}

	@Test(priority=1)
	static public void testFlattenedFieldInitSequence() throws Throwable {
		ContainerC! c = new ContainerC();
		assertNotNull(c.c);
		assertNotNull(c.c.a);
		assertNotNull(c.c.b);

		assertNotNull(c.d);
		assertNotNull(c.d.a);
		assertNotNull(c.d.b);
	}

	/*
	 * Test use of GETFIELD operations on the fields of a container class, where
	 * the fields are of value type classes that have not been resolved.
	 *
	 * The method is first called so that the GETFIELD will not be executed, and
	 * the class not resolved, and then called so that the GETFIELD and class
	 * resolution is triggered.
	 */
	@Test(priority=1)
	static public void testUnresolvedGetFieldUse() throws Throwable {
		/*
		 * Set up classes that look roughly like this:
		 *
		 * public inline class UnresolvedB1 {
		 *     public final int a;
		 *     public final int b;
		 * }
		 *
		 * public inline class UnresolvedB2 {
		 *     public final int c;
		 *     public final int d;
		 * }
		 *
		 * public inline class UnresolvedB3 {
		 *     public final int e;
		 *     public final int f;
		 * }
		 *
		 * public class ContainerForUnresolvedB {
		 *     public UnresolvedB1! v1;
		 *     public UnresolvedB2! v2;
		 *     public UnresolvedB3! v3;
		 * }
		 *
		 * public class UsingUnresolvedB {
		 *     public Object testUnresolvedValueTypeGetField(int fieldNum, ContainerForUnresolvedB container) {
		 *         // Passing in a value in the range [0..2] triggers execution of a GETFIELD
		 *         // operation on the corresponding field of "container", triggering
		 *         // resolution of the field.  Passing in a value outside that range, delays
		 *         // triggering resolution of the field
		 *         //
		 *         switch (fieldNum) {
		 *         case 0: return container.v1;
		 *         case 1: return container.v2;
		 *         case 2: return container.v3;
		 *         default: return null;
		 *         }
		 *     }
		 * }
		 */
		UnresolvedClassDesc[] uclassDescArr = new UnresolvedClassDesc[] {
								new UnresolvedClassDesc("UnresolvedB1", new String[] {"a:I", "b:I"}),
								new UnresolvedClassDesc("UnresolvedB2", new String[] {"c:I", "d:I"}),
								new UnresolvedClassDesc("UnresolvedB3", new String[] {"e:I", "f:I"})};

		Class[] valueClassArr = new Class[uclassDescArr.length];
		String containerFields[] = new String[uclassDescArr.length];
		MethodHandle[][] valueFieldGetters = new MethodHandle[uclassDescArr.length][];

		for (int i = 0; i < uclassDescArr.length; i++) {
			UnresolvedClassDesc desc = uclassDescArr[i];
			valueClassArr[i] = ValueTypeGenerator.generateValueClass(desc.name, desc.fields);
			valueFieldGetters[i] = new MethodHandle[desc.fields.length];
			containerFields[i] = "v"+(i+1)+":L"+desc.name+";::NR";

			for (int j = 0; j < desc.fields.length; j++) {
				String[] nameAndSig = desc.fields[j].split(":");
				valueFieldGetters[i][j] = generateGenericGetter(valueClassArr[i], nameAndSig[0]);
			}
		}

		Class containerClass = ValueTypeGenerator.generateRefClass("ContainerForUnresolvedB", containerFields);

		String fieldsUsing[] = {};
		Class usingClass = ValueTypeGenerator.generateRefClass("UsingUnresolvedB", fieldsUsing, "ContainerForUnresolvedB", containerFields);

		MethodHandle getFieldUnresolved = lookup.findStatic(usingClass, "testUnresolvedValueTypeGetField",
															MethodType.methodType(Object.class, new Class[] {int.class, containerClass}));

		for (int i = 0; i < 10; i++) {
			/*
			 * Pass -1 to avoid execution of GETFIELD against field that has a value type class
			 * In turn that delays the resolution of the value type class
			 */
			assertNull(getFieldUnresolved.invoke(-1, null));
		}

		Object containerObject = containerClass.newInstance();
		for (int i = 0; i < uclassDescArr.length; i++) {
			/*
			 * Pass 0 or more to trigger execution of GETFIELD against field that has a value type class
			 * In turn that triggers the resolution of the associated value type classes
			 */
			Object fieldVal = getFieldUnresolved.invoke(i, containerObject);
			assertNotNull(fieldVal);

			for (int j = 0; j < valueFieldGetters[i].length; j++) {
				assertEquals(valueFieldGetters[i][j].invoke(fieldVal), Integer.valueOf(0));
			}
		}
	}

	/*
	 * Test use of PUTFIELD operations on the fields of a container class, where
	 * the fields are of value type classes that have not been resolved.
	 *
	 * The method is first called so that the PUTFIELD will not be executed, and
	 * the class not resolved, and then called so that the PUTFIELD and class
	 * resolution is triggered.
	 */
	@Test(priority=1)
	static public void testUnresolvedPutFieldUse() throws Throwable {
		/*
		 * Set up classes that look roughly like this:
		 *
		 * public inline class UnresolvedC1 {
		 *     public final int a;
		 *     public final int b;
		 * }
		 *
		 * public inline class UnresolvedC2 {
		 *     public final int c;
		 *     public final int d;
		 * }
		 *
		 * public inline class UnresolvedC3 {
		 *     public final int e;
		 *     public final int f;
		 * }
		 *
		 * public class ContainerForUnresolvedC {
		 *     public UnresolvedC1 v1;
		 *     public UnresolvedC2 v2;
		 *     public UnresolvedC3 v3;
		 * }
		 *
		 * public class UsingUnresolvedC {
		 *     public Object testUnresolvedValueTypePutField(int fieldNum, ContainerForUnresolvedC container, Object val) {
		 *         // Passing in a value in the range [0..2] triggers execution of a PUTFIELD
		 *         // operation on the corresponding field of "container", triggering
		 *         // resolution of the class and field.  Passing in a value outside that range,
		 *         // delays triggering that resolution
		 *         //
		 *         switch (fieldNum) {
		 *         case 0: container.v1 = (UnresolvedC3) val; break;
		 *         case 1: container.v2 = (UnresolvedC2) val; break;
		 *         case 2: container.v3 = (UnresolvedC3) val; break;
		 *         default: break;
		 *         }
		 *     }
		 * }
		 */
		UnresolvedClassDesc[] uclassDescArr = new UnresolvedClassDesc[] {
								new UnresolvedClassDesc("UnresolvedC1", new String[] {"a:I", "b:I"}),
								new UnresolvedClassDesc("UnresolvedC2", new String[] {"c:I", "d:I"}),
								new UnresolvedClassDesc("UnresolvedC3", new String[] {"e:I", "f:I"})};

		Class[] valueClassArr = new Class[uclassDescArr.length];
		String containerFields[] = new String[uclassDescArr.length];
		MethodHandle[][] valueFieldGetters = new MethodHandle[uclassDescArr.length][];
		MethodHandle[] containerFieldGetters = new MethodHandle[uclassDescArr.length];

		for (int i = 0; i < uclassDescArr.length; i++) {
			UnresolvedClassDesc desc = uclassDescArr[i];
			valueClassArr[i] = ValueTypeGenerator.generateValueClass(desc.name, desc.fields);
			valueFieldGetters[i] = new MethodHandle[desc.fields.length];
			containerFields[i] = "v"+(i+1)+":L"+desc.name+";::NR";

			for (int j = 0; j < desc.fields.length; j++) {
				String[] nameAndSig = desc.fields[j].split(":");
				valueFieldGetters[i][j] = generateGenericGetter(valueClassArr[i], nameAndSig[0]);
			}
		}

		Class containerClass = ValueTypeGenerator.generateRefClass("ContainerForUnresolvedC", containerFields);

		for (int i = 0; i < uclassDescArr.length; i++) {
			String[] nameAndSig = containerFields[i].split(":");
			containerFieldGetters[i] = generateGenericGetter(containerClass, nameAndSig[0]);
		}

		String fieldsUsing[] = {};
		Class usingClass = ValueTypeGenerator.generateRefClass("UsingUnresolvedC", fieldsUsing, "ContainerForUnresolvedC", containerFields);

		MethodHandle putFieldUnresolved = lookup.findStatic(usingClass, "testUnresolvedValueTypePutField",
												MethodType.methodType(void.class, new Class[] {int.class, containerClass, Object.class}));

		for (int i = 0; i < 10; i++) {
			/*
			 * Pass -1 to avoid execution of PUTFIELD into field that has a value type class
			 * In turn that delays the resolution of the value type class
			 */
			putFieldUnresolved.invoke(-1, null, null);
		}

		Object containerObject = containerClass.newInstance();
		for (int i = 0; i < uclassDescArr.length; i++) {
			MethodHandle makeDefaultValue = lookup.findStatic(valueClassArr[i], "makeObject", MethodType.methodType(valueClassArr[i]));
			Object valueObject = makeDefaultValue.invoke();
			/*
			 * Pass 0 or more to trigger execution of PUTFIELD against field that has a value type class
			 * In turn that triggers the resolution of the associated value type classes
			 */
			putFieldUnresolved.invoke(i, containerObject, valueObject);
		}

		for (int i = 0; i < containerFieldGetters.length; i++) {
			Object containerFieldValue = containerFieldGetters[i].invoke(containerObject);

			for (int j = 0; j < valueFieldGetters[i].length; j++) {
				assertEquals(valueFieldGetters[i][j].invoke(containerFieldValue), Integer.valueOf(0));
			}
		}
	}

	static MethodHandle generateGenericGetter(Class<?> clazz, String fieldName) {
		try {
			return lookup.findVirtual(clazz, "getGeneric"+fieldName, MethodType.methodType(Object.class));
		} catch (IllegalAccessException | SecurityException | NullPointerException | NoSuchMethodException e) {
			e.printStackTrace();
		}
		return null;
	}

	@Test(priority = 1)
	static public void testIdentityObjectOnValueType() throws Throwable {
		assertTrue(ValueLong.class.isValue());
		assertFalse(ValueLong.class.isIdentity());
	}

	@Test(priority=1)
	static public void testIdentityObjectOnHeavyAbstract() throws Throwable {
		assertFalse(HeavyAbstractClass.class.isValue());
		assertTrue(HeavyAbstractClass.class.isIdentity());
	}

	static private void valueTypeIdentityObjectTestHelper(String testName, String superClassName, int extraClassFlags) throws Throwable {
		String fields[] = {"longField:J"};
		if (null == superClassName) {
			superClassName = "java/lang/Object";
		}
		Class valueClass = ValueTypeGenerator.generateValueClass(testName, superClassName, fields, extraClassFlags);
		assertTrue(valueClass.isValue());
	}

	@Test(priority=1, expectedExceptions=IncompatibleClassChangeError.class)
	static public void testValueTypeSubClassHeavyAbstract() throws Throwable {
		String superClassName = HeavyAbstractClass.class.getName().replace('.', '/');
		valueTypeIdentityObjectTestHelper("testValueTypeSubClassHeavyAbstract", superClassName, 0);
	}

	@Test(priority=1, expectedExceptions=IncompatibleClassChangeError.class)
	static public void testValueTypeSubClassHeavyAbstract1() throws Throwable {
		String superClassName = HeavyAbstractClass1.class.getName().replace('.', '/');
		valueTypeIdentityObjectTestHelper("testValueTypeSubClassHeavyAbstract1", superClassName, 0);
	}

	@Test(priority=1, expectedExceptions=IncompatibleClassChangeError.class)
	static public void testValueTypeSubClassHeavyAbstract2() throws Throwable {
		String superClassName = HeavyAbstractClass2.class.getName().replace('.', '/');
		valueTypeIdentityObjectTestHelper("testValueTypeSubClassHeavyAbstract2", superClassName, 0);
	}

	@Test(priority=1, expectedExceptions=IncompatibleClassChangeError.class)
	static public void testValueTypeSubClassHeavyAbstract3() throws Throwable {
		String superClassName = HeavyAbstractClass3.class.getName().replace('.', '/');
		valueTypeIdentityObjectTestHelper("testValueTypeSubClassHeavyAbstract3", superClassName, 0);
	}

	@Test(priority=1)
	static public void testValueTypeSubClassLightAbstract() throws Throwable {
		String superClassName = LightAbstractClass.class.getName().replace('.', '/');
		valueTypeIdentityObjectTestHelper("testValueTypeSubClassLightAbstract", superClassName, 0);
	}

	@Test(priority=1, expectedExceptions=ClassFormatError.class)
	/* RI throws ClassFormatError for this case. */
	static public void testInterfaceValueType() throws Throwable {
		valueTypeIdentityObjectTestHelper("testInterfaceValueType", "java/lang/Object", ACC_INTERFACE | ACC_ABSTRACT);
	}

	@Test(priority=1, expectedExceptions=ClassFormatError.class)
	/* RI throws ClassFormatError for this case. */
	static public void testAbstractValueType() throws Throwable {
		valueTypeIdentityObjectTestHelper("testAbstractValueType", "java/lang/Object", ACC_ABSTRACT);
	}

	@Test(priority=1, expectedExceptions=IllegalAccessError.class)
	static public void testValueTypeSubClassInvisibleLightAbstractClass() throws Throwable {
		String superClassName = InvisibleLightAbstractClass.class.getName().replace('.', '/');
		valueTypeIdentityObjectTestHelper("testValueTypeSubClassInvisibleLightAbstractClass", superClassName, 0);
	}

	@Test(priority = 1)
	static public void testIdentityObjectOnJLObject() throws Throwable {
		assertFalse(Object.class.isValue());
		assertFalse(Object.class.isIdentity());
	}

	@Test(priority = 1)
	static public void testIdentityObjectOnRef() throws Throwable {
		assertFalse(IdentityObject.class.isValue());
		assertTrue(IdentityObject.class.isIdentity());
	}

	@Test(priority=1)
	static public void testIsValueClassOnValueType() throws Throwable {
		assertTrue(ValueLong.class.isValue());
		assertFalse(ValueLong.class.isIdentity());
	}

	@Test(priority=1)
	static public void testIsValueClassOnInterface() throws Throwable {
		assertFalse(TestInterface.class.isValue());
		assertFalse(TestInterface.class.isIdentity());
	}

	@Test(priority=1)
	static public void testIsValueClassOnAbstractClass() throws Throwable {
		assertFalse(HeavyAbstractClass.class.isValue());
		assertTrue(HeavyAbstractClass.class.isIdentity());
	}

	@Test(priority=1)
	static public void testIsValueOnValueArrayClass() throws Throwable {
		assertFalse(ValueLong.class.arrayType().isValue());
		assertTrue(ValueLong.class.arrayType().isIdentity());
	}

	@Test(priority=1)
	static public void testIsValueOnRefArrayClass() throws Throwable {
		assertFalse(TestIsValueOnRefArrayClass.class.arrayType().isValue());
		assertTrue(TestIsValueOnRefArrayClass.class.arrayType().isIdentity());
	}

	@Test(priority=1)
	static public void testValueClassHashCode() throws Throwable {
		Object p1 = new ValueIntPoint2D(new ValueInt(1), new ValueInt(2));
		Object p2 = new ValueIntPoint2D(new ValueInt(1), new ValueInt(2));
		Object p3 = new ValueIntPoint2D(new ValueInt(2), new ValueInt(2));
		Object p4 = new ValueIntPoint2D(new ValueInt(2), new ValueInt(1));
		Object p5 = new ValueIntPoint2D(new ValueInt(3), new ValueInt(4));

		int h1 = p1.hashCode();
		int h2 = p2.hashCode();
		int h3 = p3.hashCode();
		int h4 = p4.hashCode();
		int h5 = p5.hashCode();

		assertEquals(h1, h2);
		assertNotEquals(h1, h3);
		assertNotEquals(h1, h4);
		assertNotEquals(h1, h5);
		assertEquals(h1, System.identityHashCode(p1));
	}

	//*******************************************************************************
	// Value type tests for static fields
	//*******************************************************************************

	/* Prioritize before static fields are set by testStaticFieldsWithSingleAlignment */
	@Test(priority=3, invocationCount=2)
	static public void testStaticFieldsWithSingleAlignmenDefaultValues() throws Throwable {
		assertNotNull(ClassWithOnlyStaticFieldsWithSingleAlignment.tri);
		assertNotNull(ClassWithOnlyStaticFieldsWithSingleAlignment.point);
		assertNotNull(ClassWithOnlyStaticFieldsWithSingleAlignment.line);
		assertNotNull(ClassWithOnlyStaticFieldsWithSingleAlignment.i);
		assertNotNull(ClassWithOnlyStaticFieldsWithSingleAlignment.f);
		assertNotNull(ClassWithOnlyStaticFieldsWithSingleAlignment.tri2);
	}

	@Test(priority=4)
	static public void testStaticFieldsWithSingleAlignment() throws Throwable {
		ClassWithOnlyStaticFieldsWithSingleAlignment.tri = new Triangle2D(defaultTrianglePositions);
		ClassWithOnlyStaticFieldsWithSingleAlignment.point = new Point2D(defaultPointPositions1);
		ClassWithOnlyStaticFieldsWithSingleAlignment.line = new FlattenedLine2D(defaultLinePositions1);
		ClassWithOnlyStaticFieldsWithSingleAlignment.i = new ValueInt(defaultInt);
		ClassWithOnlyStaticFieldsWithSingleAlignment.f = new ValueFloat(defaultFloat);
		ClassWithOnlyStaticFieldsWithSingleAlignment.tri2 = new Triangle2D(defaultTrianglePositions);

		ClassWithOnlyStaticFieldsWithSingleAlignment.tri.checkEqualTriangle2D(defaultTrianglePositions);
		ClassWithOnlyStaticFieldsWithSingleAlignment.point.checkEqualPoint2D(defaultPointPositions1);
		ClassWithOnlyStaticFieldsWithSingleAlignment.line.checkEqualFlattenedLine2D(defaultLinePositions1);
		assertEquals(ClassWithOnlyStaticFieldsWithSingleAlignment.i.i, defaultInt);
		assertEquals(ClassWithOnlyStaticFieldsWithSingleAlignment.f.f, defaultFloat);
		ClassWithOnlyStaticFieldsWithSingleAlignment.tri2.checkEqualTriangle2D(defaultTrianglePositions);
	}

	/* Prioritize before static fields are set by testStaticFieldsWithLongAlignment */
	@Test(priority=3, invocationCount=2)
	static public void testStaticFieldsWithLongAlignmenDefaultValues() throws Throwable {
		assertNotNull(ClassWithOnlyStaticFieldsWithLongAlignment.point);
		assertNotNull(ClassWithOnlyStaticFieldsWithLongAlignment.line);
		assertNotNull(ClassWithOnlyStaticFieldsWithLongAlignment.o);
		assertNotNull(ClassWithOnlyStaticFieldsWithLongAlignment.l);
		assertNotNull(ClassWithOnlyStaticFieldsWithLongAlignment.d);
		assertNotNull(ClassWithOnlyStaticFieldsWithLongAlignment.i);
		assertNotNull(ClassWithOnlyStaticFieldsWithLongAlignment.tri);
	}

	@Test(priority=4)
	static public void testStaticFieldsWithLongAlignment() throws Throwable {
		ClassWithOnlyStaticFieldsWithLongAlignment.point = new Point2D(defaultPointPositions1);
		ClassWithOnlyStaticFieldsWithLongAlignment.line = new FlattenedLine2D(defaultLinePositions1);
		ClassWithOnlyStaticFieldsWithLongAlignment.o = new ValueObject(defaultObject);
		ClassWithOnlyStaticFieldsWithLongAlignment.l = new ValueLong(defaultLong);
		ClassWithOnlyStaticFieldsWithLongAlignment.d = new ValueDouble(defaultDouble);
		ClassWithOnlyStaticFieldsWithLongAlignment.i = new ValueInt(defaultInt);
		ClassWithOnlyStaticFieldsWithLongAlignment.tri = new Triangle2D(defaultTrianglePositions);

		ClassWithOnlyStaticFieldsWithLongAlignment.point.checkEqualPoint2D(defaultPointPositions1);
		ClassWithOnlyStaticFieldsWithLongAlignment.line.checkEqualFlattenedLine2D(defaultLinePositions1);
		assertEquals(ClassWithOnlyStaticFieldsWithLongAlignment.o.val, defaultObject);
		assertEquals(ClassWithOnlyStaticFieldsWithLongAlignment.l.l, defaultLong);
		assertEquals(ClassWithOnlyStaticFieldsWithLongAlignment.d.d, defaultDouble);
		assertEquals(ClassWithOnlyStaticFieldsWithLongAlignment.i.i, defaultInt);
		ClassWithOnlyStaticFieldsWithLongAlignment.tri.checkEqualTriangle2D(defaultTrianglePositions);
	}

	/* Prioritize before static fields are set by testStaticFieldsWithObjectAlignment */
	@Test(priority=3, invocationCount=2)
	static public void testStaticFieldsWithObjectAlignmentDefaultValues() throws Throwable {
		assertNotNull(ClassWithOnlyStaticFieldsWithObjectAlignment.tri);
		assertNotNull(ClassWithOnlyStaticFieldsWithObjectAlignment.point);
		assertNotNull(ClassWithOnlyStaticFieldsWithObjectAlignment.line);
		assertNotNull(ClassWithOnlyStaticFieldsWithObjectAlignment.o);
		assertNotNull(ClassWithOnlyStaticFieldsWithObjectAlignment.i);
		assertNotNull(ClassWithOnlyStaticFieldsWithObjectAlignment.f);
		assertNotNull(ClassWithOnlyStaticFieldsWithObjectAlignment.tri2);
	}

	@Test(priority=4)
	static public void testStaticFieldsWithObjectAlignment() throws Throwable {
		ClassWithOnlyStaticFieldsWithObjectAlignment.tri = new Triangle2D(defaultTrianglePositions);
		ClassWithOnlyStaticFieldsWithObjectAlignment.point = new Point2D(defaultPointPositions1);
		ClassWithOnlyStaticFieldsWithObjectAlignment.line = new FlattenedLine2D(defaultLinePositions1);
		ClassWithOnlyStaticFieldsWithObjectAlignment.o = new ValueObject(defaultObject);
		ClassWithOnlyStaticFieldsWithObjectAlignment.i = new ValueInt(defaultInt);
		ClassWithOnlyStaticFieldsWithObjectAlignment.f = new ValueFloat(defaultFloat);
		ClassWithOnlyStaticFieldsWithObjectAlignment.tri2 = new Triangle2D(defaultTrianglePositions);

		ClassWithOnlyStaticFieldsWithObjectAlignment.tri.checkEqualTriangle2D(defaultTrianglePositions);
		ClassWithOnlyStaticFieldsWithObjectAlignment.point.checkEqualPoint2D(defaultPointPositions1);
		ClassWithOnlyStaticFieldsWithObjectAlignment.line.checkEqualFlattenedLine2D(defaultLinePositions1);
		assertEquals(ClassWithOnlyStaticFieldsWithObjectAlignment.o.val, defaultObject);
		assertEquals(ClassWithOnlyStaticFieldsWithObjectAlignment.i.i, defaultInt);
		assertEquals(ClassWithOnlyStaticFieldsWithObjectAlignment.f.f, defaultFloat);
		ClassWithOnlyStaticFieldsWithObjectAlignment.tri2.checkEqualTriangle2D(defaultTrianglePositions);
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

	/*
	 * Create large number of value types and instantiate them
	 *
	 * value Point2D {
	 * 	int x;
	 * 	int y;
	 * }
	 */
	@Test(priority=1)
	static public void testCreateLargeNumberOfPoint2D() throws Throwable {
		String fields[] = {"x:I", "y:I"};
		for (int valueIndex = 0; valueIndex < objectGCScanningIterationCount; valueIndex++) {
			String className = "Point2D" + valueIndex;
			Class point2DXClass = ValueTypeGenerator.generateValueClass(className, fields);
			/* findStatic will trigger class resolution */
			MethodHandle makePoint2DX = lookup.findStatic(point2DXClass, "staticMethod", MethodType.methodType(void.class));
			if (0 == (valueIndex % 100)) {
				System.gc();
			}
		}
	}

	/*
	 * Maintain a buffer of flattened arrays with long-aligned valuetypes while keeping a certain amount of classes alive at any
	 * single time. This forces the GC to unload the classes.
	 */
	@Test(priority=5, invocationCount=2)
	static public void testValueWithLongAlignmentGCScanning() throws Throwable {
		ArrayList<AssortedValueWithLongAlignment![]> longAlignmentArrayList = new ArrayList<AssortedValueWithLongAlignment![]>(objectGCScanningIterationCount);
		for (int i = 0; i < objectGCScanningIterationCount; i++) {
			AssortedValueWithLongAlignment![] array = new AssortedValueWithLongAlignment![genericArraySize];
			for (int j = 0; j < genericArraySize; j++) {
				array[j] = AssortedValueWithLongAlignment.createObjectWithDefaults();
			}
			longAlignmentArrayList.add(array);
		}

		System.gc();

		for (int i = 0; i < objectGCScanningIterationCount; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				longAlignmentArrayList.get(i)[j].checkFieldsWithDefaults();
			}
		}
	}

	/*
	 * Maintain a buffer of flattened arrays with object-aligned valuetypes while keeping a certain amount of classes alive at any
	 * single time. This forces the GC to unload the classes.
	 */
	@Test(priority=5, invocationCount=2)
	static public void testValueWithObjectAlignmentGCScanning() throws Throwable {
		ArrayList<AssortedValueWithObjectAlignment![]> objectAlignmentArrayList = new ArrayList<AssortedValueWithObjectAlignment![]>(objectGCScanningIterationCount);
		for (int i = 0; i < objectGCScanningIterationCount; i++) {
			AssortedValueWithObjectAlignment![] array = new AssortedValueWithObjectAlignment![genericArraySize];
			for (int j = 0; j < genericArraySize; j++) {
				array[j] = AssortedValueWithObjectAlignment.createObjectWithDefaults();
			}
			objectAlignmentArrayList.add(array);
		}

		System.gc();

		for (int i = 0; i < objectGCScanningIterationCount; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				objectAlignmentArrayList.get(i)[j].checkFieldsWithDefaults();
			}
		}
	}

	/*
	 * Maintain a buffer of flattened arrays with single-aligned valuetypes while keeping a certain amount of classes alive at any
	 * single time. This forces the GC to unload the classes.
	 */
	@Test(priority=5, invocationCount=2)
	static public void testValueWithSingleAlignmentGCScanning() throws Throwable {
		ArrayList<AssortedValueWithSingleAlignment![]> singleAlignmentArrayList = new ArrayList<AssortedValueWithSingleAlignment![]>(objectGCScanningIterationCount);
		for (int i = 0; i < objectGCScanningIterationCount; i++) {
			AssortedValueWithSingleAlignment![] array = new AssortedValueWithSingleAlignment![genericArraySize];
			for (int j = 0; j < genericArraySize; j++) {
				array[j] = AssortedValueWithSingleAlignment.createObjectWithDefaults();
			}
			singleAlignmentArrayList.add(array);
		}

		System.gc();

		for (int i = 0; i < objectGCScanningIterationCount; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				singleAlignmentArrayList.get(i)[j].checkFieldsWithDefaults();
			}
		}
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
		Object assortedValueWithLongAlignment3 = new AssortedValueWithLongAlignment(new Point2D(defaultPointPositionsNew),
				new FlattenedLine2D(defaultLinePositionsNew), new ValueObject(defaultObjectNew),
				new ValueLong(defaultLongNew), new ValueDouble(defaultDoubleNew), new ValueInt(defaultIntNew),
				new Triangle2D(defaultTrianglePositionsNew));

		Object assortedValueWithObjectAlignment = AssortedValueWithObjectAlignment.createObjectWithDefaults();
		Object assortedValueWithObjectAlignment2 = AssortedValueWithObjectAlignment.createObjectWithDefaults();
		Object assortedValueWithObjectAlignment3 = new AssortedValueWithObjectAlignment(
				new Triangle2D(defaultTrianglePositionsNew), new Point2D(defaultPointPositionsNew),
				new FlattenedLine2D(defaultLinePositionsNew), new ValueObject(defaultObjectNew),
				new ValueInt(defaultIntNew), new ValueFloat(defaultFloatNew),
				new Triangle2D(defaultTrianglePositionsNew));

		assertTrue((assortedValueWithLongAlignment == assortedValueWithLongAlignment), "A substitutability (==) test on the same value should always return true");
		assertTrue((assortedValueWithObjectAlignment == assortedValueWithObjectAlignment), "A substitutability (==) test on the same value should always return true");
		assertTrue((assortedValueWithLongAlignment == assortedValueWithLongAlignment2), "A substitutability (==) test on different value the same contents should always return true");
		assertTrue((assortedValueWithObjectAlignment == assortedValueWithObjectAlignment2), "A substitutability (==) test on different value the same contents should always return true");
		assertFalse((assortedValueWithLongAlignment == assortedValueWithLongAlignment3), "A substitutability (==) test on different value should always return false");
		assertFalse((assortedValueWithLongAlignment == assortedValueWithObjectAlignment), "A substitutability (==) test on different value should always return false");
		assertFalse((assortedValueWithObjectAlignment == assortedValueWithObjectAlignment3), "A substitutability (==) test on different value should always return false");
		assertFalse((assortedValueWithLongAlignment != assortedValueWithLongAlignment), "A substitutability (!=) test on the same value should always return false");
		assertFalse((assortedValueWithObjectAlignment != assortedValueWithObjectAlignment), "A substitutability (!=) test on the same value should always return false");
		assertFalse((assortedValueWithLongAlignment != assortedValueWithLongAlignment2), "A substitutability (!=) test on different value the same contents should always return false");
		assertFalse((assortedValueWithObjectAlignment != assortedValueWithObjectAlignment2), "A substitutability (!=) test on different value the same contents should always return false");
		assertTrue((assortedValueWithLongAlignment != assortedValueWithLongAlignment3), "A substitutability (!=) test on different value the same contents should always return false");
		assertTrue((assortedValueWithLongAlignment != assortedValueWithObjectAlignment), "A substitutability (!=) test on different value the same contents should always return false");
		assertTrue((assortedValueWithObjectAlignment != assortedValueWithObjectAlignment3), "A substitutability (!=) test on different value the same contents should always return false");
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
			ValueTypeGenerator.generateIllegalValueClassWithSynchMethods("TestSynchMethodsOnValueTypesIllegal", null);
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
		MethodHandle makeRef = lookup.findStatic(refTypeClass, "makeObject", MethodType.methodType(refTypeClass));
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

	@Test(priority=3)
	static public void createClassWithVolatileValueTypeFields() throws Throwable {
		ClassWithValueTypeVolatile c = ClassWithValueTypeVolatile.createObjectWithDefaults();
		c.checkFieldsWithDefaults();
	}

	@Test(priority=1, expectedExceptions=ClassFormatError.class)
	static public void testValueTypeHasSynchMethods() throws Throwable {
		String fields[] = {"longField:J"};
		Class valueClass = ValueTypeGenerator.generateIllegalValueClassWithSynchMethods("TestValueTypeHasSynchMethods", fields);
	}

	//*******************************************************************************
	// default value tests
	//*******************************************************************************

	/*
	 * Create a value type and read the fields before
	 * they are set. The test should Verify that the
	 * flattenable fields are set to the default values.
	 * NULL should never be observed for nullrestricted fields.
	 */
	@Test(priority=4)
	static public void testDefaultValues() throws Throwable {
		AssortedValueWithLongAlignment assortedValueWithLongAlignment = new AssortedValueWithLongAlignment();
		assertNotNull(assortedValueWithLongAlignment.point);
		assertNotNull(assortedValueWithLongAlignment.line);
		assertNotNull(assortedValueWithLongAlignment.o);
		assertNotNull(assortedValueWithLongAlignment.l);
		assertNotNull(assortedValueWithLongAlignment.d);
		assertNotNull(assortedValueWithLongAlignment.i);
		assertNotNull(assortedValueWithLongAlignment.tri);

		/* Test with assorted ref object with long alignment */
		AssortedRefWithLongAlignment assortedRefWithLongAlignment = new AssortedRefWithLongAlignment();
		assertNotNull(assortedRefWithLongAlignment.point);
		assertNotNull(assortedRefWithLongAlignment.line);
		assertNotNull(assortedRefWithLongAlignment.o);
		assertNotNull(assortedRefWithLongAlignment.l);
		assertNotNull(assortedRefWithLongAlignment.d);
		assertNotNull(assortedRefWithLongAlignment.i);
		assertNotNull(assortedRefWithLongAlignment.tri);

		/* Test with flattened line 2D */
		FlattenedLine2D flattenedLine2D = new FlattenedLine2D();
		assertNotNull(flattenedLine2D.st);
		assertNotNull(flattenedLine2D.en);

		/* Test with triangle 2D */
		Triangle2D triangle2D = new Triangle2D();
		assertNotNull(triangle2D.v1);
		assertNotNull(triangle2D.v2);
		assertNotNull(triangle2D.v3);
	}

	/*
	 * Create Array Objects with Point Class without initialization
	 * The array should be set to a Default Value.
	 *
	 * TODO not sure if this array needs to be nullrestricted, currently the ri
	 * is not up to date on this. Revisit this later.
	 */
	@Test(priority=4, invocationCount=2)
	static public void testDefaultValueInPointArray() throws Throwable {
		Point2D[] pointArray = new Point2D[genericArraySize];
		for (int i = 0; i < genericArraySize; i++) {
			assertNotNull(pointArray[i]);
		}
	}

	/**
	 * Create multi dimensional array with Point Class without initialization.
	 * Set each array value to a default value and check field access method handler.
	 */
	@Test(priority=4)
	static public void testDefaultValueInPointInstanceMultiArray() throws Throwable {
		Point2D[][] pointArray = new Point2D[genericArraySize][genericArraySize];
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				 pointArray[i][j] = new Point2D(defaultPointPositions1);
			}
		}
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				pointArray[i][j].checkFieldAccessWithDefaultValues();
			}
		}
	}

	/*
	 * Create Array Objects with Flattened Line without initialization
	 * Check the fields of each element in arrays. No field should be NULL.
	 */
	@Test(priority=4, invocationCount=2)
	static public void testDefaultValueInLineArray() throws Throwable {
		FlattenedLine2D[] flattenedLineArray = new FlattenedLine2D[genericArraySize];
		for (int i = 0; i < genericArraySize; i++) {
			FlattenedLine2D lineObject = flattenedLineArray[i];
			assertNotNull(lineObject);
			assertNotNull(lineObject.st);
			assertNotNull(lineObject.en);
		}
	}

	/**
	 * Create multi dimensional array with Line Class without initialization.
	 * Set each array value to a default value and check field access method handler.
	 */
	@Test(priority=4)
	static public void testDefaultValueInLineInstanceMultiArray() throws Throwable {
		FlattenedLine2D[][] flattenedLineArray = new FlattenedLine2D[genericArraySize][genericArraySize];
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				FlattenedLine2D lineNew = new FlattenedLine2D(defaultLinePositions1);
				flattenedLineArray[i][j] = lineNew;
			}
		}
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				flattenedLineArray[i][j].checkFieldAccessWithDefaultValues();
			}
		}
	}

	/*
	 * Create Array Objects with triangle class without initialization
	 * Check the fields of each element in arrays. No field should be NULL.
	 */
	@Test(priority=4, invocationCount=2)
	static public void testDefaultValueInTriangleArray() throws Throwable {
		Triangle2D[] triangleArray = new Triangle2D[genericArraySize];
		for (int i = 0; i < genericArraySize; i++) {
			Triangle2D triangleObject = triangleArray[i];
			assertNotNull(triangleObject);
			assertNotNull(triangleObject.v1);
			assertNotNull(triangleObject.v2);
			assertNotNull(triangleObject.v3);
		}
	}

	/**
	 * Create multi dimensional array with Triangle Class without initialization.
	 * Set each array value to a default value and check field access method handler.
	 */
	@Test(priority=4)
	static public void testDefaultValueInTriangleInstanceMultiArray() throws Throwable {
		Triangle2D[][] triangleArray = new Triangle2D[genericArraySize][genericArraySize];
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				triangleArray[i][j] = new Triangle2D(defaultTrianglePositions);
			}
		}
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				triangleArray[i][j].checkFieldAccessWithDefaultValues();
			}
		}
	}

	/*
	 * Create an assortedRefWithLongAlignment Array
	 * Since it's ref type, the array should be filled with nullptrs
	 */
	@Test(priority=4, invocationCount=2)
	static public void testDefaultValueInAssortedRefWithLongAlignmentArray() throws Throwable {
		AssortedRefWithLongAlignment[] array = new AssortedRefWithLongAlignment[genericArraySize];
		for (int i = 0; i < genericArraySize; i++) {
			assertNull(array[i]);
		}
	}

	/*
	 * Create an Array  Object with assortedValueWithLongAlignment class without initialization
	 * Check the fields of each element in arrays. No field should be NULL.
	 */
	@Test(priority=4, invocationCount=2)
	static public void testDefaultValueInAssortedValueWithLongAlignmentArray() throws Throwable {
		AssortedValueWithLongAlignment[] array = new AssortedValueWithLongAlignment[genericArraySize];
		for (int i = 0; i < genericArraySize; i++) {
			AssortedValueWithLongAlignment v = array[i];
			assertNotNull(v);
			assertNotNull(v.point);
			assertNotNull(v.line);
			assertNotNull(v.o);
			assertNotNull(v.l);
			assertNotNull(v.d);
			assertNotNull(v.i);
			assertNotNull(v.tri);
		}
	}

	/**
	 * Create multi dimensional array with assortedValueWithLongAlignment without initialization.
	 * Set each array value to a default value and check field access method handler.
	 *
	 * Fails tests with array flattening enabled
	 */
	@Test(priority=5)
	static public void testDefaultValueInAssortedValueWithLongAlignmenInstanceMultiArray() throws Throwable {
		AssortedValueWithLongAlignment[][] array = new AssortedValueWithLongAlignment[genericArraySize][genericArraySize];
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				array[i][j] = AssortedValueWithLongAlignment.createObjectWithDefaults();
			}
		}
		for (int i = 0; i < genericArraySize; i++) {
			for (int j = 0; j < genericArraySize; j++) {
				array[i][j].checkFieldsWithDefaults();
			}
		}
	}

	/*
	 * Create a 2D array of valueTypes, verify that the default elements are null.
	 */
	@Test(priority=5, invocationCount=2)
	static public void testMultiDimentionalArrays() throws Throwable {
		Class assortedValueWithLongAlignment2DClass = Array.newInstance(AssortedValueWithLongAlignment.class, 1).getClass();
		Class assortedValueWithSingleAlignment2DClass = Array.newInstance(AssortedValueWithSingleAlignment.class, 1).getClass();

		Object assortedRefWithLongAlignment2DArray = Array.newInstance(assortedValueWithLongAlignment2DClass, genericArraySize);
		Object assortedRefWithSingleAlignment2DArray = Array.newInstance(assortedValueWithSingleAlignment2DClass, genericArraySize);

		for (int i = 0; i < genericArraySize; i++) {
			Object ref = Array.get(assortedRefWithLongAlignment2DArray, i);
			assertNull(ref);

			ref = Array.get(assortedRefWithSingleAlignment2DArray, i);
			assertNull(ref);
		}
	}

	@Test(priority=1)
	static public void testCyclicalStaticFieldDefaultValues1() throws Throwable {
		String cycleA1[] = { "val1:LCycleB1;:static" };
		String cycleB1[] = { "val1:LCycleA1;:static" };

		Class cycleA1Class = ValueTypeGenerator.generateValueClass("CycleA1", cycleA1);
		Class cycleB1Class = ValueTypeGenerator.generateValueClass("CycleB1", cycleB1);

		MethodHandle makeCycleA1 = lookup.findStatic(cycleA1Class, "makeObject", MethodType.methodType(cycleA1Class));
		MethodHandle makeCycleB1 = lookup.findStatic(cycleB1Class, "makeObject", MethodType.methodType(cycleB1Class));

		makeCycleA1.invoke();
		makeCycleB1.invoke();
	}

	@Test(priority=1)
	static public void testCyclicalStaticFieldDefaultValues2() throws Throwable {
		String cycleA2[] = { "val1:LCycleB2;:static" };
		String cycleB2[] = { "val1:LCycleC2;:static" };
		String cycleC2[] = { "val1:LCycleA2;:static" };

		Class cycleA2Class = ValueTypeGenerator.generateValueClass("CycleA2", cycleA2);
		Class cycleB2Class = ValueTypeGenerator.generateValueClass("CycleB2", cycleB2);
		Class cycleC2Class = ValueTypeGenerator.generateValueClass("CycleC2", cycleC2);

		MethodHandle makeCycleA2 = lookup.findStatic(cycleA2Class, "makeObject", MethodType.methodType(cycleA2Class));
		MethodHandle makeCycleB2 = lookup.findStatic(cycleB2Class, "makeObject", MethodType.methodType(cycleB2Class));
		MethodHandle makeCycleC2 = lookup.findStatic(cycleB2Class, "makeObject", MethodType.methodType(cycleB2Class));

		makeCycleA2.invoke();
		makeCycleB2.invoke();
		makeCycleC2.invoke();
	}

	@Test(priority=1)
	static public void testCyclicalStaticFieldDefaultValues3() throws Throwable {
		String cycleA3[] = { "val1:LCycleA3;:static" };

		Class cycleA3Class = ValueTypeGenerator.generateValueClass("CycleA3", cycleA3);

		MethodHandle makeCycleA3 = lookup.findStatic(cycleA3Class, "makeObject", MethodType.methodType(cycleA3Class));

		makeCycleA3.invoke();
	}

	//*******************************************************************************
	// checkcast tests
	//*******************************************************************************
	@Test(priority=1)
	static public void testCheckCastValueTypeOnInvalidClass() throws Throwable {
		String fields[] = {"longField:J"};
		Class valueClass = ValueTypeGenerator.generateValueClass("TestCheckCastOnInvalidClass", fields);
		MethodHandle checkCastOnInvalidClass = lookup.findStatic(valueClass, "testCheckCastOnInvalidClass", MethodType.methodType(Object.class));
		checkCastOnInvalidClass.invoke();
	}

	/*
	 * Ensure that casting null to a reference type class will pass
	 */
	@Test(priority=1)
	static public void testCheckCastRefClassOnNull() throws Throwable {
		String fields[] = {"longField:J"};
		Class refClass = ValueTypeGenerator.generateRefClass("TestCheckCastRefClassOnNull", fields);
		MethodHandle checkCastRefClassOnNull = lookup.findStatic(refClass, "testCheckCastRefClassOnNull", MethodType.methodType(Object.class));
		checkCastRefClassOnNull.invoke();
	}

	@Test(priority = 1, expectedExceptions = ClassFormatError.class,
		expectedExceptionsMessageRegExp = ".*A non-interface class must have at least one of ACC_FINAL, ACC_IDENTITY, or ACC_ABSTRACT flags set.*")
	static public void testNonInterfaceClassMustHaveFinalIdentityAbstractSet() throws Throwable {
		ValueTypeGenerator.generateNonInterfaceClassWithMissingFlags("testNonInterfaceClassMustHaveFinalIdentityAbstractSet");
	}

	static void checkObject(Object ...objects) throws Throwable {
		com.ibm.jvm.Dump.SystemDump();
	}
}
