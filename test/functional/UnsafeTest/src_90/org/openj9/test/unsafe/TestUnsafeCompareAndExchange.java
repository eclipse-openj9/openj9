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
public class TestUnsafeCompareAndExchange extends UnsafeTestBase {
	private String[] testList = { COMPAREANDEXCH, COMPAREANDEXCHA, COMPAREANDEXCHR };

	private static Logger logger = Logger.getLogger(TestUnsafeCompareAndExchange.class);

	public TestUnsafeCompareAndExchange(String scenario) {
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
	public void testInstanceCompareAndExchangeByte() throws Exception {
		for (String test : testList) {
			testByte(new ByteData(), test);
		}
	}

	public void testInstanceCompareAndExchangeChar() throws Exception {
		for (String test : testList) {
			testChar(new CharData(), test);
		}
	}

	public void testInstanceCompareAndExchangeShort() throws Exception {
		for (String test : testList) {
			testShort(new ShortData(), test);
		}
	}

	public void testInstanceCompareAndExchangeInt() throws Exception {
		for (String test : testList) {
			testInt(new IntData(), test);
		}
	}

	public void testInstanceCompareAndExchangeLong() throws Exception {
		for (String test : testList) {
			testLong(new LongData(), test);
		}
	}

	public void testInstanceCompareAndExchangeFloat() throws Exception {
		for (String test : testList) {
			testFloat(new FloatData(), test);
		}
	}

	public void testInstanceCompareAndExchangeDouble() throws Exception {
		for (String test : testList) {
			testDouble(new DoubleData(), test);
		}
	}

	public void testInstanceCompareAndExchangeBoolean() throws Exception {
		for (String test : testList) {
			testBoolean(new BooleanData(), test);
		}
	}

	public void testInstanceCompareAndExchangeObject() throws Exception {
		for (String test : testList) {
			testObject(new ObjectData(), test);
		}
	}

	/* test with array as object */
	public void testArrayCompareAndExchangeByte() throws Exception {
		for (String test : testList) {
			testByte(new byte[modelByte.length], test);
		}
	}

	public void testArrayCompareAndExchangeChar() throws Exception {
		for (String test : testList) {
			testChar(new char[modelChar.length], test);
		}
	}

	public void testArrayCompareAndExchangeShort() throws Exception {
		for (String test : testList) {
			testShort(new short[modelShort.length], test);
		}
	}

	public void testArrayCompareAndExchangeInt() throws Exception {
		for (String test : testList) {
			testInt(new int[modelInt.length], test);
		}
	}

	public void testArrayCompareAndExchangeLong() throws Exception {
		for (String test : testList) {
			testLong(new long[modelLong.length], test);
		}
	}

	public void testArrayCompareAndExchangeFloat() throws Exception {
		for (String test : testList) {
			testFloat(new float[modelFloat.length], test);
		}
	}

	public void testArrayCompareAndExchangeDouble() throws Exception {
		for (String test : testList) {
			testDouble(new double[modelDouble.length], test);
		}
	}

	public void testArrayCompareAndExchangeBoolean() throws Exception {
		for (String test : testList) {
			testBoolean(new boolean[modelBoolean.length], test);
		}
	}

	public void testArrayCompareAndExchangeObject() throws Exception {
		for (String test : testList) {
			testObject(new Object[models.length], test);
		}
	}

	/* test with static object */
	public void testStaticCompareAndExchangeByte() throws Exception {
		for (String test : testList) {
			testByte(ByteData.class, test);
		}
	}

	public void testStaticCompareAndExchangeChar() throws Exception {
		for (String test : testList) {
			testChar(CharData.class, test);
		}
	}

	public void testStaticCompareAndExchangeShort() throws Exception {
		for (String test : testList) {
			testShort(ShortData.class, test);
		}
	}

	public void testStaticCompareAndExchangeInt() throws Exception {
		for (String test : testList) {
			testInt(IntData.class, test);
		}
	}

	public void testStaticCompareAndExchangeLong() throws Exception {
		for (String test : testList) {
			testLong(LongData.class, test);
		}
	}

	public void testStaticCompareAndExchangeFloat() throws Exception {
		for (String test : testList) {
			testFloat(FloatData.class, test);
		}
	}

	public void testStaticCompareAndExchangeDouble() throws Exception {
		for (String test : testList) {
			testDouble(DoubleData.class, test);
		}
	}

	public void testStaticCompareAndExchangeBoolean() throws Exception {
		for (String test : testList) {
			testBoolean(BooleanData.class, test);
		}
	}

	public void testStaticCompareAndExchangeObject() throws Exception {
		for (String test : testList) {
			testObject(ObjectData.class, test);
		}
	}

	/* test with null object */
	public void testObjectNullCompareAndExchangeByte() throws Exception {
		memAllocate(100);
		alignment();

		for (String test : testList) {
			testByteNative(test);
		}
	}

	public void testObjectNullCompareAndExchangeChar() throws Exception {
		memAllocate(100);
		alignment();

		for (String test : testList) {
			testCharNative(test);
		}
	}

	public void testObjectNullCompareAndExchangeShort() throws Exception {
		memAllocate(100);
		alignment();

		for (String test : testList) {
			testShortNative(test);
		}
	}

	public void testObjectNullCompareAndExchangeInt() throws Exception {
		memAllocate(100);
		alignment();

		for (String test : testList) {
			testIntNative(test);
		}
	}

	public void testObjectNullCompareAndExchangeLong() throws Exception {
		memAllocate(100);
		alignment();

		for (String test : testList) {
			testLongNative(test);
		}
	}

	public void testObjectNullCompareAndExchangeFloat() throws Exception {
		memAllocate(100);
		alignment();

		for (String test : testList) {
			testFloatNative(test);
		}
	}

	public void testObjectNullCompareAndExchangeDouble() throws Exception {
		memAllocate(100);
		alignment();

		for (String test : testList) {
			testDoubleNative(test);
		}
	}

	public void testObjectNullCompareAndExchangeBoolean() throws Exception {
		memAllocate(100);
		alignment();

		for (String test : testList) {
			testBooleanNative(test);
		}
	}

}
