/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package org.openj9.test.unsafe;

import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class TestUnsafeCompareAndSet extends UnsafeTestBase {
	private String[] testList = { COMPAREANDEXCH, COMPAREANDEXCHA, COMPAREANDEXCHR };

	private static Logger logger = Logger.getLogger(TestUnsafeCompareAndSet.class);

	public TestUnsafeCompareAndSet(String scenario) {
		super(scenario);
	}

	/*
	 * get logger to use, for child classes to report with their class name instead
	 * of UnsafeTestBase
	 */
	protected Logger getLogger() {
		return logger;
	}

	@Override
	@BeforeMethod
	protected void setUp() throws Exception {
		myUnsafe = getUnsafeInstance2();
	}

	/* test with instance as object */
	public void testInstanceCompareAndSetByte() throws Exception {
		for (String test : testList) {
			testByte(new ByteData(), test);
		}
	}

	public void testInstanceCompareAndSetChar() throws Exception {
		for (String test : testList) {
			testChar(new CharData(), test);
		}
	}

	public void testInstanceCompareAndSetShort() throws Exception {
		for (String test : testList) {
			testShort(new ShortData(), test);
		}
	}

	public void testInstanceCompareAndSetInt() throws Exception {
		for (String test : testList) {
			testInt(new IntData(), test);
		}
	}

	public void testInstanceCompareAndSetLong() throws Exception {
		for (String test : testList) {
			testLong(new LongData(), test);
		}
	}

	public void testInstanceCompareAndSetFloat() throws Exception {
		for (String test : testList) {
			testFloat(new FloatData(), test);
		}
	}

	public void testInstanceCompareAndSetDouble() throws Exception {
		for (String test : testList) {
			testDouble(new DoubleData(), test);
		}
	}

	public void testInstanceCompareAndSetBoolean() throws Exception {
		for (String test : testList) {
			testBoolean(new BooleanData(), test);
		}
	}

	public void testInstanceCompareAndSetObject() throws Exception {
		for (String test : testList) {
			testObject(new ObjectData(), test);
		}
	}

	/* test with array as object */
	public void testArrayCompareAndSetByte() throws Exception {
		for (String test : testList) {
			testByte(new byte[modelByte.length], test);
		}
	}

	public void testArrayCompareAndSetChar() throws Exception {
		for (String test : testList) {
			testChar(new char[modelChar.length], test);
		}
	}

	public void testArrayCompareAndSetShort() throws Exception {
		for (String test : testList) {
			testShort(new short[modelShort.length], test);
		}
	}

	public void testArrayCompareAndSetInt() throws Exception {
		for (String test : testList) {
			testInt(new int[modelInt.length], test);
		}
	}

	public void testArrayCompareAndSetLong() throws Exception {
		for (String test : testList) {
			testLong(new long[modelLong.length], test);
		}
	}

	public void testArrayCompareAndSetFloat() throws Exception {
		for (String test : testList) {
			testFloat(new float[modelFloat.length], test);
		}
	}

	public void testArrayCompareAndSetDouble() throws Exception {
		for (String test : testList) {
			testDouble(new double[modelDouble.length], test);
		}
	}

	public void testArrayCompareAndSetBoolean() throws Exception {
		testBoolean(new boolean[modelBoolean.length], COMPAREANDSET);
	}

	public void testArrayCompareAndSetObject() throws Exception {
		for (String test : testList) {
			testObject(new Object[models.length], test);
		}
	}

	/* test with static object */
	public void testStaticCompareAndSetByte() throws Exception {
		for (String test : testList) {
			testByte(ByteData.class, test);
		}
	}

	public void testStaticCompareAndSetChar() throws Exception {
		for (String test : testList) {
			testChar(CharData.class, test);
		}
	}

	public void testStaticCompareAndSetShort() throws Exception {
		for (String test : testList) {
			testShort(ShortData.class, test);
		}
	}

	public void testStaticCompareAndSetInt() throws Exception {
		for (String test : testList) {
			testInt(IntData.class, test);
		}
	}

	public void testStaticCompareAndSetLong() throws Exception {
		for (String test : testList) {
			testLong(LongData.class, test);
		}
	}

	public void testStaticCompareAndSetFloat() throws Exception {
		for (String test : testList) {
			testFloat(FloatData.class, test);
		}
	}

	public void testStaticCompareAndSetDouble() throws Exception {
		for (String test : testList) {
			testDouble(DoubleData.class, test);
		}
	}

	public void testStaticCompareAndSetBoolean() throws Exception {
		for (String test : testList) {
			testBoolean(BooleanData.class, test);
		}
	}

	public void testStaticCompareAndSetObject() throws Exception {
		for (String test : testList) {
			testObject(ObjectData.class, test);
		}
	}

	/* test with null object */
	public void testObjectNullCompareAndSetByte() throws Exception {
		memAllocate(100);
		alignment();

		for (String test : testList) {
			testByteNative(test);
		}
	}

	public void testObjectNullCompareAndSetChar() throws Exception {
		memAllocate(100);
		alignment();

		for (String test : testList) {
			testCharNative(test);
		}
	}

	public void testObjectNullCompareAndSetShort() throws Exception {
		memAllocate(100);
		alignment();

		for (String test : testList) {
			testShortNative(test);
		}
	}

	public void testObjectNullCompareAndSetInt() throws Exception {
		memAllocate(100);
		alignment();

		for (String test : testList) {
			testIntNative(test);
		}
	}

	public void testObjectNullCompareAndSetLong() throws Exception {
		memAllocate(100);
		alignment();

		for (String test : testList) {
			testLongNative(test);
		}
	}

	public void testObjectNullCompareAndSetFloat() throws Exception {
		memAllocate(100);
		alignment();

		for (String test : testList) {
			testFloatNative(test);
		}
	}

	public void testObjectNullCompareAndSetDouble() throws Exception {
		memAllocate(100);
		alignment();

		for (String test : testList) {
			testDoubleNative(test);
		}
	}

	public void testObjectNullCompareAndSetBoolean() throws Exception {
		memAllocate(100);
		alignment();

		for (String test : testList) {
			testBooleanNative(test);
		}
	}

}
