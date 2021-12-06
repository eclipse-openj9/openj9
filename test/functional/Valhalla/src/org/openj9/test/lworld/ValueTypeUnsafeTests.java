/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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
package org.openj9.test.lworld;


import jdk.internal.misc.Unsafe;
import java.lang.reflect.Field;
import java.lang.management.ManagementFactory;
import java.lang.management.RuntimeMXBean;
import java.util.List;
import static org.testng.Assert.*;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeClass;
import static org.openj9.test.lworld.ValueTypeUnsafeTestClasses.*;


@Test(groups = { "level.sanity" })
public class ValueTypeUnsafeTests {
	static Unsafe myUnsafe = null;
	static boolean isCompressedRefsEnabled = false;
	static boolean isFlatteningEnabled = false;
	static boolean isArrayFlatteningEnabled = false;

	@BeforeClass
	static public void testSetUp() throws RuntimeException {
		myUnsafe = Unsafe.getUnsafe();

		List<String> arguments = ManagementFactory.getRuntimeMXBean().getInputArguments();
		isFlatteningEnabled = arguments.contains("-XX:ValueTypeFlatteningThreshold=99999");
		isArrayFlatteningEnabled = arguments.contains("-XX:+EnableArrayFlattening");
		isCompressedRefsEnabled = arguments.contains("-Xcompressedrefs");
	}

	@Test
	static public void testFlattenedFieldIsFlattened() throws Throwable {
		ValueTypePoint2D p = new ValueTypePoint2D(new ValueTypeInt(1), new ValueTypeInt(1));
		boolean isFlattened = myUnsafe.isFlattened(p.getClass().getDeclaredField("x"));
		assertEquals(isFlattened, isFlatteningEnabled);
	}

	@Test
	static public void testFlattenedArrayIsFlattened() throws Throwable {
		ValueTypePoint2D[] pointAry = {};
		assertTrue(pointAry.getClass().isArray());
		boolean isArrayFlattened = myUnsafe.isFlattenedArray(pointAry.getClass());
		assertEquals(isArrayFlattened, isArrayFlatteningEnabled);
	}

	@Test
	static public void testRegularArrayIsNotFlattened() throws Throwable {
		Point2D[] ary = {};
		assertTrue(ary.getClass().isArray());
		boolean isArrayFlattened = myUnsafe.isFlattenedArray(ary.getClass());
		assertFalse(isArrayFlattened);
	}

	@Test
	static public void testRegularFieldIsNotFlattened() throws Throwable {
		Point2D p = new Point2D(1, 1);
		boolean isFlattened = myUnsafe.isFlattened(p.getClass().getDeclaredField("x"));
		assertFalse(isFlattened);
	}

	@Test
	static public void testFlattenedObjectIsNotFlattenedArray() throws Throwable {
		ValueTypePoint2D p = new ValueTypePoint2D(new ValueTypeInt(1), new ValueTypeInt(1));
		boolean isArrayFlattened = myUnsafe.isFlattenedArray(p.getClass());
		assertFalse(isArrayFlattened);
	}

	@Test
	static public void testNullIsNotFlattenedArray() throws Throwable {
		boolean isArrayFlattened = myUnsafe.isFlattenedArray(null);
		assertFalse(isArrayFlattened);
	}

	@Test
	static public void testPassingNullToIsFlattenedThrowsException() throws Throwable {
		assertThrows(NullPointerException.class, () -> {
			myUnsafe.isFlattened(null);
		});
	}

	@Test
	static public void testValueHeaderSizeOfValueType() throws Throwable {
		long headerSize = myUnsafe.valueHeaderSize(ValueTypePoint2D.class);
		if (isCompressedRefsEnabled) {
			assertEquals(headerSize, 4);
		} else {
			assertEquals(headerSize, 8);
		}
	}

	@Test
	static public void testValueHeaderSizeOfClassWithLongField() throws Throwable {
		long headerSize = myUnsafe.valueHeaderSize(ValueTypeWithLongField.class);
		if (isCompressedRefsEnabled && !isFlatteningEnabled) {
			assertEquals(headerSize, 4);
		} else {
			// In compressed refs mode, when the class has 8 byte field(s) only, 4 bytes of padding are added to the class. These padding bytes are included in the headerSize.
			assertEquals(headerSize, 8);
		}
	}

	// TODO (#14117): add testing for Unsafe.valueHeaderSize using Unsafe.getObjectSize

	@Test
	static public void testValueHeaderSizeOfNonValueType() throws Throwable {
		assertEquals(myUnsafe.valueHeaderSize(Point2D.class), 0);
	}

	@Test
	static public void testValueHeaderSizeOfNull() throws Throwable {
		assertEquals(myUnsafe.valueHeaderSize(null), 0);
	}

	@Test
	static public void testValueHeaderSizeOfVTArray() throws Throwable {
		ValueTypeInt[] vtAry = {};
		long size = myUnsafe.valueHeaderSize(vtAry.getClass());
		assertEquals(size, 0);
	}

	@Test
	static public void testValueHeaderSizeOfArray() throws Throwable {
		Point2D[] ary = {};
		long size = myUnsafe.valueHeaderSize(ary.getClass());
		assertEquals(size, 0);
	}

	@Test
	static public void testUninitializedDefaultValueOfValueType() throws Throwable {
		// create a new ValueTypePoint2D to ensure that the class has been initialized befoere myUnsafe.uninitializedDefaultValue is called
		ValueTypePoint2D newPoint = new ValueTypePoint2D(new ValueTypeInt(1), new ValueTypeInt(1));
		ValueTypePoint2D p = myUnsafe.uninitializedDefaultValue(newPoint.getClass());
		assertEquals(p.x.i, 0);
		assertEquals(p.y.i, 0);
	}

	@Test
	static public void testUninitializedDefaultValueOfNonValueType() throws Throwable {
		Point2D p = myUnsafe.uninitializedDefaultValue(Point2D.class);
		assertNull(p);
	}

	@Test
	static public void testUninitializedDefaultValueOfNull() throws Throwable {
		Object o = myUnsafe.uninitializedDefaultValue(null);
		assertNull(o);
	}

	@Test
	static public void testUninitializedDefaultValueOfVTArray() throws Throwable {
		ValueTypeInt[] vtAry = {};
		Object o = myUnsafe.uninitializedDefaultValue(vtAry.getClass());
		assertNull(o);
	}

	@Test
	static public void testUninitializedDefaultValueOfArray() throws Throwable {
		Point2D[] ary = {};
		Object o = myUnsafe.uninitializedDefaultValue(ary.getClass());
		assertNull(o);
	}

	@Test
	static public void testuninitializedVTClassHasNullDefaultValue() {
		primitive class NeverInitialized {
			final ValueTypeInt i;
			NeverInitialized(ValueTypeInt i) { this.i = i; }
		}
		assertNull(myUnsafe.uninitializedDefaultValue(NeverInitialized.class));
	}
}
