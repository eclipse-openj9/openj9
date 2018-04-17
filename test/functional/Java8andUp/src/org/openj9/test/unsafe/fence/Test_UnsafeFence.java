package org.openj9.test.unsafe.fence;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.reflect.*;
import sun.misc.Unsafe;

/*******************************************************************************
 * Copyright (c) 2014, 2018 IBM Corp. and others
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
@Test(groups = { "level.sanity" })
public class Test_UnsafeFence {

	static Unsafe unsafe;

	int intField;
	long longField;

	static Unsafe getUnsafe() {
		if (unsafe == null) {
			try {
				/* Use reflect to get Unsafe so we can avoid the 'must be bootstrap' check */
				Field theUnsafe = Unsafe.class.getDeclaredField("theUnsafe");
				theUnsafe.setAccessible(true);
				unsafe = (Unsafe)theUnsafe.get(null);
			} catch (NoSuchFieldException e) {
			} catch (IllegalAccessException e) {
			}
		}
		return unsafe;
	}

	/**
	 * @tests sun.misc.Unsafe.getAndAddInt()
	 * @tests sun.misc.Unsafe.loadFence()
	 * @tests sun.misc.Unsafe.storeFence()
	 * @tests sun.misc.Unsafe.fullFence()
	 */
	@Test
	public void testIntFence() {
		Unsafe unsafe = getUnsafe();
		Field intFieldReflect;
		try {
			intFieldReflect = getClass().getDeclaredField("intField");
			long intFieldOffset = unsafe.objectFieldOffset(intFieldReflect);
			AssertJUnit.assertEquals(unsafe.getAndAddInt(this, intFieldOffset, 2), 0);
			AssertJUnit.assertEquals(unsafe.getAndAddInt(this, intFieldOffset, 3), 2);
			AssertJUnit.assertEquals(unsafe.getAndSetInt(this, intFieldOffset, 7), 5);
			AssertJUnit.assertEquals(unsafe.getAndAddInt(this, intFieldOffset, -3), 7);
			AssertJUnit.assertEquals(unsafe.getAndSetInt(this, intFieldOffset, -9), 4);
			unsafe.loadFence();
			AssertJUnit.assertEquals(intField, -9);
			unsafe.storeFence();
			AssertJUnit.assertEquals(intField, -9);
			unsafe.fullFence();
			AssertJUnit.assertEquals(intField, -9);
		} catch (NoSuchFieldException e) {
			Assert.fail("Missing field");
		} catch (SecurityException e) {
			Assert.fail("SecurityException: " + e.getMessage());
		}
	}

	/**
	 * @tests sun.misc.Unsafe.getAndAddLong()
	 * @tests sun.misc.Unsafe.loadFence()
	 * @tests sun.misc.Unsafe.storeFence()
	 * @tests sun.misc.Unsafe.fullFence()
	 */
	@Test
	public void testLongFence() {
		Unsafe unsafe = getUnsafe();
		Field intFieldReflect;
		Field longFieldReflect;
		try {
			longFieldReflect = getClass().getDeclaredField("longField");
			long longFieldOffset = unsafe.objectFieldOffset(longFieldReflect);

			AssertJUnit.assertEquals(unsafe.getAndAddLong(this, longFieldOffset, 2), 0);
			AssertJUnit.assertEquals(unsafe.getAndAddLong(this, longFieldOffset, 3), 2);
			AssertJUnit.assertEquals(unsafe.getAndSetLong(this, longFieldOffset, 7), 5);
			AssertJUnit.assertEquals(unsafe.getAndAddLong(this, longFieldOffset, -3), 7);
			AssertJUnit.assertEquals(unsafe.getAndSetLong(this, longFieldOffset, -9), 4);
			AssertJUnit.assertEquals(longField, -9);

			unsafe.loadFence();
			AssertJUnit.assertEquals(longField, -9);
			unsafe.storeFence();
			AssertJUnit.assertEquals(longField, -9);
			unsafe.fullFence();
			AssertJUnit.assertEquals(longField, -9);
		} catch (NoSuchFieldException e) {
			Assert.fail("Missing field");
		} catch (SecurityException e) {
			Assert.fail("SecurityException: " + e.getMessage());
		}
	}
}
