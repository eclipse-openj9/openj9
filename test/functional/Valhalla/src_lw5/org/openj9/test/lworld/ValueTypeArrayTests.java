/*
 * Copyright IBM Corp. and others 2022
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

/**
 * Test array element assignment involving arrays of type {@code Object},
 * of an interface type, and of value types, with values of those types,
 * or possibly, a null reference.
 */
@Test(groups = { "level.sanity" })
public class ValueTypeArrayTests {
	private ValueTypeArrayTests() {}

	/**
	 * A simple interface class
	 */
	interface SomeIface {}

	/**
	 * A simple value type class
	 */
	static value class PointV implements SomeIface {
		double x;
		double y;

		PointV(double x, double y) {
			this.x = x;
			this.y = y;
		}
	}

	/**
	 * A simple nullrestricted(flattenable) value type class
	 */
	static value class PointFV implements SomeIface {
		double x;
		double y;

		public implicit PointFV();

		PointFV(double x, double y) {
			this.x = x;
			this.y = y;
		}
	}

	/**
	 * A simple identity class
	 */
	static class Bogus implements SomeIface {};

	static Object nullObj = null;
	static SomeIface bogusIfaceObj = new Bogus();
	static PointV pointVal = new PointV(1.0, 2.0);
	static PointFV! pointFlattenableVal = new PointFV(1.0, 2.0);

	static void assign(Object[] arr, int idx, Object value) {
		arr[idx] = value;
	}

	static void assign(Object[] arr, int idx, SomeIface value) {
		arr[idx] = value;
	}

	static void assign(Object[] arr, int idx, PointV value) {
		arr[idx] = value;
	}

	static void assign(Object[] arr, int idx, PointFV! value) {
		arr[idx] = value;
	}

	static void assign(SomeIface[] arr, int idx, SomeIface value) {
		arr[idx] = value;
	}

	static void assign(SomeIface[] arr, int idx, PointV value) {
		arr[idx] = value;
	}

	static void assign(SomeIface[] arr, int idx, PointFV! value) {
		arr[idx] = value;
	}

	static void assign(PointV[] arr, int idx, PointV value) {
		arr[idx] = value;
	}

	static void assign(PointFV![] arr, int idx, PointFV! value) {
		arr[idx] = value;
	}

	static void assignDispatch(Object[] arr, int idx, Object src, int arrKind, int srcKind) {
		if (arrKind == OBJ_TYPE) {
			if (srcKind == OBJ_TYPE) {
				assign(arr, idx, src);
			} else if (srcKind == IFACE_TYPE) {
				assign(arr, idx, (SomeIface) src);
			} else if (srcKind == VAL_TYPE) {
				assign(arr, idx, (PointV) src);
			} else if (srcKind == FLATTENABLE_TYPE) {
				assign(arr, idx, (PointFV!) src);
			} else {
				fail("Unexpected source type requested "+srcKind);
			}
		} else if (arrKind == IFACE_TYPE) {
			if (srcKind == OBJ_TYPE) {
				// Meaningless combination
			} else if (srcKind == IFACE_TYPE) {
				assign((SomeIface[]) arr, idx, (SomeIface) src);
			} else if (srcKind == VAL_TYPE) {
				assign((SomeIface[]) arr, idx, (PointV) src);
			} else if (srcKind == FLATTENABLE_TYPE) {
				assign((SomeIface[]) arr, idx, (PointFV!) src);
			} else {
				fail("Unexpected source type requested "+srcKind);
			}
		} else if (arrKind == VAL_TYPE) {
			if (srcKind == OBJ_TYPE) {
				// Meaningless combination
			} else if (srcKind == IFACE_TYPE) {
				// Meaningless combination
			} else if (srcKind == VAL_TYPE) {
				assign((PointV[]) arr, idx, (PointV) src);
			} else if (srcKind == FLATTENABLE_TYPE) {
				// Meaningless combination
			} else {
				fail("Unexpected source type requested "+srcKind);
			}
		} else if (arrKind == FLATTENABLE_TYPE) {
			if (srcKind == OBJ_TYPE) {
				// Meaningless combination
			} else if (srcKind == IFACE_TYPE) {
				// Meaningless combination
			} else if (srcKind == VAL_TYPE) {
				// Meaningless combination
			} else if (srcKind == FLATTENABLE_TYPE) {
				assign((PointFV![])arr, idx, (PointFV!) src);
			} else {
				fail("Unexpected source type requested "+srcKind);
			}
		} else {
			fail("Unexpected array type requested "+arrKind);
		}
	}

	/**
	 * Represents a null reference.
	 */
	static final int NULL_REF = 0;

	/**
	 * Represents the type <code>Object</code> or <code>Object[]</code>
	 */
	static final int OBJ_TYPE = 1;

	/**
	 * Represents the types <code>SomeIface</code> or <code>SomeIface[]</code>.
	 * {@link SomeIface} is an interface class defined by this test.
	 */
	static final int IFACE_TYPE = 2;

	/**
	 * Represents the type <code>PointV</code> or <code>PointV[]</code>.
	 * {@link PointV} is a non-flattenable value type class defined by this test.
	 */
	static final int VAL_TYPE = 3;

	/**
	 * Represents the type <code>PointFV</code> or <code>PointFV[]</code>.
	 * {@link PointFV} is a flattenable value type class defined by this test.
	 */
	static final int FLATTENABLE_TYPE = 4;

	/**
	 * Convenient constant reference to the <code>ArrayIndexOutOfBoundsException</code> class
	 */
	static final Class AIOOBE = ArrayIndexOutOfBoundsException.class;

	/**
	 * Convenient constant reference to the <code>ArrayStoreException</code> class
	 */
	static final Class ASE = ArrayStoreException.class;

	/**
	 * Convenient constant reference to the <code>NullPointerException</code> class
	 */
	static final Class NPE = NullPointerException.class;

	/**
	 * The expected kind of exception that will be thrown, if any, for an
	 * assignment to an array whose component type is one of {@link #OBJ_TYPE},
	 * {@link #IFACE_TYPE}, {@link #VAL_TYPE} or {@link #FLATTENABLE_TYPE} with a source
	 * of one of those same types or {@link #NULL_REF}.
	 *
	 * <p><code>expectedAssignmentExceptions[actualArrayKind][actualSourceKind]</code>
	 */
	static Class expectedAssignmentExceptions[][] =
		new Class[][] {
						null,									// NULL_REF for array is not a possibility
						new Class[] {null, null, null, null, null}, // All values can be assigned to Object[]
						new Class[] {null, ASE,  null, null, null}, // ASE for SomeIface[] = Object
						new Class[] {null, ASE,  ASE,  null, ASE},  // ASE for PointV[] = PointFV, SomeIface
						// TODO first ASE for nullrestricted null assignment disabled until OpenJ9 has full support for nullrestricted arrays
						new Class[] {/*ASE*/ null,  ASE,  ASE,  ASE,  null}, // ASE for PointFV[] = null; ASE for PointFV[] = PointV
					};

	/**
	 * Indicates whether a value or an array with component class
	 * that is one of {@link #NULL_REF}, {@link #OBJ_TYPE},
	 * {@link #IFACE_TYPE}, {@link #VAL_TYPE} or {@link #FLATTENABLE_TYPE} can be
	 * cast to another member of that same set of types without triggering a
	 * <code>ClassCastException</code> or <code>NullPointerException</code>
	 *
	 * <p><code>permittedCast[actual][static]</code>
	 */
	static boolean permittedCast[][] =
		new boolean[][] {
						new boolean[] { false, true, true,  true,  false },	// NULL_REF cannot be cast to null-restricted value
						new boolean[] { false, true, false, false, false },	// OBJ_TYPE to Object
						new boolean[] { false, true, true, false,  false },	// IFACE_TYPE to Object, SomeIface
						new boolean[] { false, true, true, true ,  false },	// VAL_TYPE to Object, SomeIface, PointV
						new boolean[] { false, true, true, false,  true }	// FLATTENABLE_TYPE to Object, SomeIface, PointFV
					};

	/**
	 * Dispatch to a particular test method that will test with parameters cast to a specific pair
	 * of static types.  Those types are specified by the <code>staticArrayKind</code> and
	 * <code>staticSourceKind</code> parameters, each of which has one of the values
	 * {@link #OBJ_TYPE}, {@link #IFACE_TYPE}, {@link #VAL_TYPE} or {@link #FLATTENABLE_TYPE}.
	 *
	 * @param arr Array to which <code>sourceVal</code> will be assigned
	 * @param sourceVal Value that will be assigned to an element of <code>arr</code>
	 * @param staticArrayKind A value indicating the static type of array that is to be tested
	 * @param staticSourceKind A value indicating the static type that is to be tested for the source of the assignmentt
	 */
	static void runTest(Object[] arr, Object sourceVal, int staticArrayKind, int staticSourceKind) throws Throwable {
		boolean caughtThrowable = false;
		int actualArrayKind = arr instanceof PointFV![]
								? FLATTENABLE_TYPE
								: arr instanceof PointV[]
									? VAL_TYPE
									: arr instanceof SomeIface[]
										? IFACE_TYPE
										: OBJ_TYPE;
		int actualSourceKind = sourceVal == null
								? NULL_REF
								: sourceVal instanceof PointFV!
									? FLATTENABLE_TYPE
									: sourceVal instanceof PointV
										? VAL_TYPE
										: sourceVal instanceof SomeIface
											? IFACE_TYPE
											: OBJ_TYPE;

		// Would a cast from the actual type of array to the specified static array type, or the actual type of the
		// source value to the specified static type trigger a ClassCastException or NPE?  If so, skip the test.
		if (!permittedCast[actualArrayKind][staticArrayKind] || !permittedCast[actualSourceKind][staticSourceKind]) {
			return;
		}

		Class expectedExceptionClass = expectedAssignmentExceptions[actualArrayKind][actualSourceKind];

		try {
			assignDispatch(arr, 1, sourceVal, staticArrayKind, staticSourceKind);
		} catch (Throwable t) {
			caughtThrowable = true;
			assertEquals(t.getClass(), expectedExceptionClass, "ActualArrayKind == "+actualArrayKind+"; StaticArrayKind == "
				+staticArrayKind+"; actualSourceKind == "+actualSourceKind+"; staticSourceKind == "+staticSourceKind);
		}

		if (expectedExceptionClass != null) {
			assertTrue(caughtThrowable,
				"Expected exception kind "+expectedExceptionClass.getName()+" to be thrown.  ActualArrayKind == "+actualArrayKind
				+"; StaticArrayKind == "+staticArrayKind+"; actualSourceKind == "+actualSourceKind+"; staticSourceKind == "
				+staticSourceKind);
		}

		// ArrayIndexOutOfBoundsException must be checked before both
		// NullPointerException for null-restricted value type and ArrayStoreException.
		// This call to assignDispatch will attempt an out-of-bounds element assignment,
		// and is always expected to throw an ArrayIndexOutOfBoundsException.
		boolean caughtAIOOBE = false;

		try {
			assignDispatch(arr, 2, sourceVal, staticArrayKind, staticSourceKind);
		} catch(ArrayIndexOutOfBoundsException aioobe) {
			caughtAIOOBE = true;
		} catch (Throwable t) {
			assertTrue(false, "Expected ArrayIndexOutOfBoundsException, but saw "+t.getClass().getName());
		}

		assertTrue(caughtAIOOBE, "Expected ArrayIndexOutOfBoundsException");
	}

	/**
	 * Test various types of arrays and source values along with various statically declared types
	 * for the arrays and source values to ensure required <code>ArrayStoreExceptions</code>,
	 * <code>ArrayIndexOutOfBoundsExceptions</code> and <code>NullPointerExceptions</code> are
	 * detected.
	 * @throws java.lang.Throwable if the attempted array element assignment throws an exception
	 */
	@Test(priority=1,invocationCount=2)
	static public void testValueTypeArrayAssignments() throws Throwable {
		Object[][] testArrays = new Object[][] {new Object[2], new SomeIface[2], new PointV[2], new PointFV![2]};
		int[] kinds = {OBJ_TYPE, IFACE_TYPE, VAL_TYPE, FLATTENABLE_TYPE};
		Object[] vals = new Object[] {null, bogusIfaceObj, new PointV(1.0, 2.0), new PointFV(3.0, 4.0)};

		for (int i = 0; i < testArrays.length; i++) {
			Object[] testArray = testArrays[i];

			for (int j = 0; j < kinds.length; j++) {
				int staticArrayKind = kinds[j];

				for (int k = 0; k < kinds.length; k++) {
					int staticValueKind = kinds[k];
					// runTest's parameters are of type Object[] and Object.  It ultimately dispatches to an assign
					// method whose parameters have more specific static types.  This condition filters out the
					// combinations of static types that aren't permitted from the point of view of the Java language.
					//
					// Cases:
					//  - the two types are the same  (staticArrayKind == staticValueKind)
					//  OR
					//    - the type of the array is Object[] or SomeIface[]  (staticArrayKind < VAL_TYPE)
					//      AND
					//    - the type of the array is less specific than that of the source (staticArrayKind < staticValueKind)
					//
					if (staticArrayKind == staticValueKind
						|| (staticArrayKind < staticValueKind && staticArrayKind < VAL_TYPE)) {
						runTest(testArrays[i], nullObj, staticArrayKind, staticValueKind);
						runTest(testArrays[i], bogusIfaceObj, staticArrayKind, staticValueKind);
						runTest(testArrays[i], pointVal, staticArrayKind, staticValueKind);
						runTest(testArrays[i], pointFlattenableVal, staticArrayKind, staticValueKind);
					}
				}
			}
		}
	}

	static value class SomeFlattenableClassWithDoubleField {
		public double d;

		public implicit SomeFlattenableClassWithDoubleField();

		SomeFlattenableClassWithDoubleField(double x) {
			this.d = x;
		}
	}

	static value class SomeFlattenableClassWithFloatField {
		public float f;

		public implicit SomeFlattenableClassWithFloatField();

		SomeFlattenableClassWithFloatField(float x) {
			this.f = x;
		}
	}

	static value class SomeFlattenableClassWithLongField {
		public long l;

		public implicit SomeFlattenableClassWithLongField();

		SomeFlattenableClassWithLongField(long x) {
			this.l = x;
		}
	}

	static class SomeIdentityClassWithDoubleField{
		public double d;

		SomeIdentityClassWithDoubleField(double x) {
			this.d = x;
		}
	}

	static class SomeIdentityClassWithFloatField{
		public float f;

		SomeIdentityClassWithFloatField(float x) {
			this.f = x;
		}
	}

	static class SomeIdentityClassWithLongField{
		public long l;

		SomeIdentityClassWithLongField(long x) {
			this.l = x;
		}
	}

	interface SomeInterface1WithSingleImplementer {}

	interface SomeInterface2WithSingleImplementer {}

	static value class SomeFlattenableClassImplIf implements SomeInterface1WithSingleImplementer {
		public double d;
		public long l;

		public implicit SomeFlattenableClassImplIf();

		SomeFlattenableClassImplIf(double val1, long val2) {
			this.d = val1;
			this.l = val2;
		}
	}

	static class SomeIdentityClassImplIf implements SomeInterface2WithSingleImplementer {
		public double d;
		public long l;

		SomeIdentityClassImplIf(double val1, long val2) {
			this.d = val1;
			this.l = val2;
		}
	}

	static class SomeClassHolder {
		public static int ARRAY_LENGTH = 10;
		SomeInterface1WithSingleImplementer[] data_1;
		SomeInterface1WithSingleImplementer[] data_2;

		SomeInterface2WithSingleImplementer[] data_3;
		SomeInterface2WithSingleImplementer[] data_4;
		SomeInterface2WithSingleImplementer   data_5;

		SomeClassHolder() {
			data_1 = new SomeFlattenableClassImplIf![ARRAY_LENGTH];
			data_2 = new SomeFlattenableClassImplIf![ARRAY_LENGTH];

			data_3 = new SomeIdentityClassImplIf[ARRAY_LENGTH];
			data_4 = new SomeIdentityClassImplIf[ARRAY_LENGTH];

			data_5 = new SomeIdentityClassImplIf((double)(12345), (long)(12345));

			for (int i = 0; i < ARRAY_LENGTH; i++) {
				data_1[i] = new SomeFlattenableClassImplIf((double)i, (long)i);
				data_2[i] = new SomeFlattenableClassImplIf((double)(i+1), (long)(i+1));

				data_3[i] = data_5;
				data_4[i] = new SomeIdentityClassImplIf((double)(i+1), (long)(i+1));
			}
		}
	}

	static void readArrayElementWithDoubleField(SomeFlattenableClassWithDoubleField![] data) throws Throwable {
		for (int i=0; i<data.length; ++i) {
			assertEquals(data[i].d, (double)i);
		}
	}

	static void readArrayElementWithFloatField(SomeFlattenableClassWithFloatField![] data) throws Throwable {
		for (int i=0; i<data.length; ++i) {
			assertEquals(data[i].f, (float)i);
		}
	}

	static void readArrayElementWithLongField(SomeFlattenableClassWithLongField![] data) throws Throwable {
		for (int i=0; i<data.length; ++i) {
			assertEquals(data[i].l, (long)i);
		}
	}

	static void readArrayElementWithDoubleField(SomeIdentityClassWithDoubleField[] data) throws Throwable {
		for (int i=0; i<data.length; ++i) {
			assertEquals(data[i].d, (double)i);
		}
	}

	static void readArrayElementWithFloatField(SomeIdentityClassWithFloatField[] data) throws Throwable {
		for (int i=0; i<data.length; ++i) {
			assertEquals(data[i].f, (float)i);
		}
	}

	static void readArrayElementWithLongField(SomeIdentityClassWithLongField[] data) throws Throwable {
		for (int i=0; i<data.length; ++i) {
			assertEquals(data[i].l, (long)i);
		}
	}

	static void readArrayElementWithSomeFlattenableClassImplIf(SomeClassHolder holder) throws Throwable {
		for (int i=0; i<holder.data_1.length; ++i) {
			assertEquals(holder.data_1[i], new SomeFlattenableClassImplIf((double)i, (long)i));
		}
	}

	static void readArrayElementWithSomeIdentityClassImplIf(SomeClassHolder holder) throws Throwable {
		for (int i=0; i<holder.data_3.length; ++i) {
			assertEquals(holder.data_3[i], holder.data_5);
		}
	}

	static void writeArrayElementWithDoubleField(SomeFlattenableClassWithDoubleField![] srcData, SomeFlattenableClassWithDoubleField![] dstData) throws Throwable {
		for (int i=0; i<dstData.length; ++i) {
			dstData[i] = srcData[i];
		}

		for (int i=0; i<dstData.length; ++i) {
			assertEquals(dstData[i].d, (double)(i+1));
		}
	}

	static void writeArrayElementWithFloatField(SomeFlattenableClassWithFloatField![] srcData, SomeFlattenableClassWithFloatField![] dstData) throws Throwable {
		for (int i=0; i<dstData.length; ++i) {
			dstData[i] = srcData[i];
		}

		for (int i=0; i<dstData.length; ++i) {
			assertEquals(dstData[i].f, (float)(i+1));
		}
	}

	static void writeArrayElementWithLongField(SomeFlattenableClassWithLongField![] srcData, SomeFlattenableClassWithLongField![] dstData) throws Throwable {
		for (int i=0; i<dstData.length; ++i) {
			dstData[i] = srcData[i];
		}

		for (int i=0; i<dstData.length; ++i) {
			assertEquals(dstData[i].l, (long)(i+1));
		}
	}

	static void writeArrayElementWithDoubleField(SomeIdentityClassWithDoubleField[] srcData, SomeIdentityClassWithDoubleField[] dstData) throws Throwable {
		for (int i=0; i<dstData.length; ++i) {
			dstData[i] = srcData[i];
		}

		for (int i=0; i<dstData.length; ++i) {
			assertEquals(dstData[i].d, (double)(i+1));
		}
	}

	static void writeArrayElementWithFloatField(SomeIdentityClassWithFloatField[] srcData, SomeIdentityClassWithFloatField[] dstData) throws Throwable {
		for (int i=0; i<dstData.length; ++i) {
			dstData[i] = srcData[i];
		}

		for (int i=0; i<dstData.length; ++i) {
			assertEquals(dstData[i].f, (float)(i+1));
		}
	}

	static void writeArrayElementWithLongField(SomeIdentityClassWithLongField[] srcData, SomeIdentityClassWithLongField[] dstData) throws Throwable {
		for (int i=0; i<dstData.length; ++i) {
			dstData[i] = srcData[i];
		}

		for (int i=0; i<dstData.length; ++i) {
			assertEquals(dstData[i].l, (long)(i+1));
		}
	}

	static void writeArrayElementWithSomeFlattenableClassImplIf(SomeClassHolder holder) throws Throwable {
		for (int i=0; i<holder.data_1.length; ++i) {
			holder.data_1[i] = holder.data_2[i];
		}

		for (int i=0; i<holder.data_1.length; ++i) {
			assertEquals(holder.data_1[i], new SomeFlattenableClassImplIf((double)(i+1), (long)(i+1)));
		}
	}

	static void writeArrayElementWithSomeIdentityClassImplIf(SomeClassHolder holder) throws Throwable {
		for (int i=0; i<holder.data_3.length; ++i) {
			holder.data_3[i] = holder.data_4[i];
		}

		for (int i=0; i<holder.data_3.length; ++i) {
			assertEquals(holder.data_3[i], holder.data_4[i]);
		}
	}

	@Test(priority=1,invocationCount=2)
	static public void testValueTypeAaload() throws Throwable {
		int ARRAY_LENGTH = 10;
		SomeFlattenableClassWithDoubleField![] data1  = new SomeFlattenableClassWithDoubleField![ARRAY_LENGTH];
		SomeFlattenableClassWithFloatField![]  data2  = new SomeFlattenableClassWithFloatField![ARRAY_LENGTH];
		SomeFlattenableClassWithLongField![]   data3  = new SomeFlattenableClassWithLongField![ARRAY_LENGTH];

		SomeIdentityClassWithDoubleField[] data4  = new SomeIdentityClassWithDoubleField[ARRAY_LENGTH];
		SomeIdentityClassWithFloatField[]  data5  = new SomeIdentityClassWithFloatField[ARRAY_LENGTH];
		SomeIdentityClassWithLongField[]   data6  = new SomeIdentityClassWithLongField[ARRAY_LENGTH];

		SomeClassHolder holder = new SomeClassHolder();

		for (int i=0; i<ARRAY_LENGTH; ++i) {
			data1[i] = new SomeFlattenableClassWithDoubleField((double)i);
			data2[i] = new SomeFlattenableClassWithFloatField((float)i);
			data3[i] = new SomeFlattenableClassWithLongField((long)i);

			data4[i] = new SomeIdentityClassWithDoubleField((double)i);
			data5[i] = new SomeIdentityClassWithFloatField((float)i);
			data6[i] = new SomeIdentityClassWithLongField((long)i);
		}

		readArrayElementWithDoubleField(data1);
		readArrayElementWithFloatField(data2);
		readArrayElementWithLongField(data3);

		readArrayElementWithDoubleField(data4);
		readArrayElementWithFloatField(data5);
		readArrayElementWithLongField(data6);

		readArrayElementWithSomeFlattenableClassImplIf(holder);
		readArrayElementWithSomeIdentityClassImplIf(holder);
	}

	@Test(priority=1,invocationCount=2)
	static public void testValueTypeAastore() throws Throwable {
		int ARRAY_LENGTH = 10;
		SomeFlattenableClassWithDoubleField![] srcData1 = new SomeFlattenableClassWithDoubleField![ARRAY_LENGTH];
		SomeFlattenableClassWithDoubleField![] dstData1 = new SomeFlattenableClassWithDoubleField![ARRAY_LENGTH];
		SomeFlattenableClassWithFloatField![]  srcData2 = new SomeFlattenableClassWithFloatField![ARRAY_LENGTH];
		SomeFlattenableClassWithFloatField![]  dstData2 = new SomeFlattenableClassWithFloatField![ARRAY_LENGTH];
		SomeFlattenableClassWithLongField![]   srcData3 = new SomeFlattenableClassWithLongField![ARRAY_LENGTH];
		SomeFlattenableClassWithLongField![]   dstData3 = new SomeFlattenableClassWithLongField![ARRAY_LENGTH];

		SomeIdentityClassWithDoubleField[] srcData4  = new SomeIdentityClassWithDoubleField[ARRAY_LENGTH];
		SomeIdentityClassWithDoubleField[] dstData4  = new SomeIdentityClassWithDoubleField[ARRAY_LENGTH];
		SomeIdentityClassWithFloatField[]  srcData5  = new SomeIdentityClassWithFloatField[ARRAY_LENGTH];
		SomeIdentityClassWithFloatField[]  dstData5  = new SomeIdentityClassWithFloatField[ARRAY_LENGTH];
		SomeIdentityClassWithLongField[]   srcData6  = new SomeIdentityClassWithLongField[ARRAY_LENGTH];
		SomeIdentityClassWithLongField[]   dstData6  = new SomeIdentityClassWithLongField[ARRAY_LENGTH];

		SomeClassHolder holder   = new SomeClassHolder();

		for (int i=0; i<ARRAY_LENGTH; ++i) {
			srcData1[i] = new SomeFlattenableClassWithDoubleField((double)(i+1));
			srcData2[i] = new SomeFlattenableClassWithFloatField((float)(i+1));
			srcData3[i] = new SomeFlattenableClassWithLongField((long)(i+1));

			dstData1[i] = new SomeFlattenableClassWithDoubleField((double)i);
			dstData2[i] = new SomeFlattenableClassWithFloatField((float)i);
			dstData3[i] = new SomeFlattenableClassWithLongField((long)i);

			srcData4[i] = new SomeIdentityClassWithDoubleField((double)(i+1));
			srcData5[i] = new SomeIdentityClassWithFloatField((float)(i+1));
			srcData6[i] = new SomeIdentityClassWithLongField((long)(i+1));

			dstData4[i] = new SomeIdentityClassWithDoubleField((double)i);
			dstData5[i] = new SomeIdentityClassWithFloatField((float)i);
			dstData6[i] = new SomeIdentityClassWithLongField((long)i);
		}

		writeArrayElementWithDoubleField(srcData1, dstData1);
		writeArrayElementWithFloatField(srcData2, dstData2);
		writeArrayElementWithLongField(srcData3, dstData3);

		writeArrayElementWithDoubleField(srcData4, dstData4);
		writeArrayElementWithFloatField(srcData5, dstData5);
		writeArrayElementWithLongField(srcData6, dstData6);

		writeArrayElementWithSomeFlattenableClassImplIf(holder);
		writeArrayElementWithSomeIdentityClassImplIf(holder);
	}

	public static value class EmptyFlat {
		public implicit EmptyFlat();
	}

	public static value class EmptyVal {
	}

	static void copyBetweenEmptyFlatArrays(EmptyFlat![] arr1, EmptyFlat![] arr2) {
		for (int i = 0; i < arr1.length; i++) {
			arr1[i] = arr2[i];
		}
	}

	static void compareEmptyFlatArrays(EmptyFlat![] arr1, EmptyFlat![] arr2) {
		for (int i = 0; i < arr1.length; i++) {
			assertEquals(arr1[i], arr2[i]);
		}
	}

	static void copyBetweenEmptyValArrays(EmptyVal[] arr1, EmptyVal[] arr2) {
		for (int i = 0; i < arr1.length; i++) {
			arr1[i] = arr2[i];
		}
	}

	static void compareEmptyValArrays(EmptyVal[] arr1, EmptyVal[] arr2) {
		for (int i = 0; i < arr1.length; i++) {
			assertEquals(arr1[i], arr2[i]);
		}
	}

	@Test(priority=1,invocationCount=2)
	static public void testEmptyValueArrayElement() {
		EmptyFlat![] flatArr1 = new EmptyFlat![4];
		EmptyFlat![] flatArr2 = new EmptyFlat![4];

		copyBetweenEmptyFlatArrays(flatArr1, flatArr2);
		compareEmptyFlatArrays(flatArr1, flatArr2);

		EmptyVal[] valArr1 = new EmptyVal[4];
		EmptyVal[] valArr2 = new EmptyVal[4];

		copyBetweenEmptyValArrays(valArr1, valArr2);
		compareEmptyValArrays(valArr1, valArr2);
	}

	static void arrayElementStoreNull(Object[] arr, int index) {
		arr[index] = null;
	}

	static void arrayElementStore(Object[] arr, int index, Object obj) {
		arr[index] = obj;
	}

	/* Disabled because OpenJ9 does not fully support flattened arrays for lw5 changes yet. */
	@Test(priority=1,invocationCount=2, enabled=false)
	static public void testStoreNullToNullRestrictedArrayElement1() throws Throwable {
		int ARRAY_LENGTH = 10;
		SomeFlattenableClassWithDoubleField![] dstData = new SomeFlattenableClassWithDoubleField![ARRAY_LENGTH];

		try {
			arrayElementStoreNull(dstData, ARRAY_LENGTH/2);
		} catch (ArrayStoreException npe) {
			return; /* pass */
		}

		Assert.fail("Expect a ArrayStoreException. No exception or wrong kind of exception thrown");
	}

	/* Disabled because OpenJ9 does not fully support flattened arrays for lw5 changes yet. */
	@Test(priority=1,invocationCount=2, enabled=false)
	static public void testStoreNullToNullRestrictedArrayElement2() throws Throwable {
		int ARRAY_LENGTH = 10;
		SomeFlattenableClassWithDoubleField![] dstData = new SomeFlattenableClassWithDoubleField![ARRAY_LENGTH];
		Object obj = null;

		try {
			arrayElementStore(dstData, ARRAY_LENGTH/2, obj);
		} catch (ArrayStoreException npe) {
			return; /* pass */
		}

		Assert.fail("Expect a ArrayStoreException. No exception or wrong kind of exception thrown");
	}
}
