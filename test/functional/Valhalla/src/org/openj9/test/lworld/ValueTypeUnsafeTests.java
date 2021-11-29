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
import static org.testng.Assert.*;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeClass;
import static org.openj9.test.lworld.ValueTypeUnsafeTestClasses.*;


@Test(groups = { "level.sanity" })
public class ValueTypeUnsafeTests {
	static Unsafe myUnsafe = null;

	@BeforeClass
	static public void testSetUp() throws RuntimeException {
		myUnsafe = Unsafe.getUnsafe();
	}

	@Test
	static public void testFlattenedFieldIsFlattened() throws Throwable {
		ValueTypePoint2D p = new ValueTypePoint2D(new ValueTypeInt(1), new ValueTypeInt(1));
		boolean isFlattened = myUnsafe.isFlattened(p.getClass().getDeclaredField("x"));
		assertTrue(isFlattened);
	}

	@Test
	static public void testFlattenedArrayIsFlattened() throws Throwable {
		ValueTypePoint2D[] pointAry = {};
		assertTrue(pointAry.getClass().isArray());
		boolean isArrayFlattened = myUnsafe.isFlattenedArray(pointAry.getClass());
		assertTrue(isArrayFlattened);
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
}
