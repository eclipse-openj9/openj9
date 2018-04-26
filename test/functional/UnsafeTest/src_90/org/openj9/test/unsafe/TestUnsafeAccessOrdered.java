/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
public class TestUnsafeAccessOrdered extends UnsafeTestBase {
	private static Logger logger = Logger.getLogger(TestUnsafeAccessOrdered.class);
	
	public TestUnsafeAccessOrdered(String scenario) {
		super(scenario);
	}
	
	/* get logger to use, for child classes to report with their class name instead of UnsafeTestBase*/
	@Override
	protected Logger getLogger() {
		return logger;
	}
	
	@Override
	@BeforeMethod
	protected void setUp() throws Exception {
		myUnsafe = getUnsafeInstance2();
	}

	/* put test with new instance as object */
	public void testInstancePutOrderedByte() throws Exception {
		testByte(new ByteData(), ORDERED);
	}

	public void testInstancePutOrderedChar() throws Exception {
		testChar(new CharData(), ORDERED);
	}

	public void testInstancePutOrderedShort() throws Exception {
		testShort(new ShortData(), ORDERED);
	}

	public void testInstancePutOrderedInt() throws Exception {
		testInt(new IntData(), ORDERED);
	}
	
	public void testInstancePutOrderedLong() throws Exception {
		testLong(new LongData(), ORDERED);
	}

	public void testInstancePutOrderedFloat() throws Exception {
		testFloat(new FloatData(), ORDERED);
	}

	public void testInstancePutOrderedDouble() throws Exception {
		testDouble(new DoubleData(), ORDERED);
	}

	public void testInstancePutOrderedBoolean() throws Exception {
		testBoolean(new BooleanData(), ORDERED);
	}

	public void testInstancePutOrderedObject() throws Exception {
		testObject(new ObjectData(), ORDERED);
	}

	/* put tests with array as object */
	public void testArrayPutOrderedByte() throws Exception {
		testByte(new byte[modelByte.length], ORDERED);
	}

	public void testArrayPutOrderedChar() throws Exception {
		testChar(new char[modelChar.length], ORDERED);
	}

	public void testArrayPutOrderedShort() throws Exception {
		testShort(new short[modelShort.length], ORDERED);
	}

	public void testArrayPutOrderedInt() throws Exception {
		testInt(new int[modelInt.length], ORDERED);
	}
	
	public void testArrayPutOrderedLong() throws Exception {	;
		testLong(new long[modelLong.length], ORDERED);
	}

	public void testArrayPutOrderedFloat() throws Exception {
		testFloat(new float[modelFloat.length], ORDERED);
	}

	public void testArrayPutOrderedDouble() throws Exception {
		testDouble(new double[modelDouble.length], ORDERED);
	}

	public void testArrayPutOrderedBoolean() throws Exception {
		testBoolean(new boolean[modelBoolean.length], ORDERED);
	}

	public void testArrayPutOrderedObject() throws Exception {
		testObject(new Object[models.length], ORDERED);
	}

	/* put tests with static object */
	public void testStaticPutOrderedByte() throws Exception {
		testByte(ByteData.class, ORDERED);
	}

	public void testStaticPutOrderedChar() throws Exception {
		testChar(CharData.class, ORDERED);
	}

	public void testStaticPutOrderedShort() throws Exception {
		testShort(ShortData.class, ORDERED);
	}

	public void testStaticPutOrderedInt() throws Exception {
		testInt(IntData.class, ORDERED);
	}
	
	public void testStaticPutOrderedLong() throws Exception {
		testLong(LongData.class, ORDERED);
	}

	public void testStaticPutOrderedFloatRelease() throws Exception {
		testFloat(FloatData.class, ORDERED);
	}

	public void testStaticPutOrderdDouble() throws Exception {
		testDouble(DoubleData.class, ORDERED);
	}

	public void testStaticPutOrderedBoolean() throws Exception {
		testBoolean(BooleanData.class, ORDERED);
	}

	public void testStaticPutOrderedObject() throws Exception {
		testObject(ObjectData.class, ORDERED);
	}

	/* tests with null object */
	public void testObjectNullPutOrderedByte() throws Exception {
		testByteNative(ORDERED);
	}

	public void testObjectNullPutOrderedChar() throws Exception {
		testCharNative(ORDERED);
	}

	public void testObjectNullPutOrderedShort() throws Exception {
		testShortNative(ORDERED);
	}

	public void testObjectNullPutOrderedInt() throws Exception {
		testIntNative(ORDERED);
	}
	
	public void testObjectNullPutOrderedLong() throws Exception {
		testLongNative(ORDERED);
	}

	public void testObjectNullPutOrderedFloat() throws Exception {
		testFloatNative(ORDERED);
	}

	public void testObjectNullPutOrderedDouble() throws Exception {
		testDoubleNative(ORDERED);
	}

	public void testObjectNullPutOrderedBoolean() throws Exception {
		testBooleanNative(ORDERED);
	}

	/* get test with new instance as object */
	public void testInstanceGetOrderedByte() throws Exception {
		testGetByte(new ByteData(), ORDERED);
	}

	public void testInstanceGetOrderedChar() throws Exception {
		testGetChar(new CharData(), ORDERED);
	}

	public void testInstanceGetOrderedShort() throws Exception {
		testGetShort(new ShortData(), ORDERED);
	}

	public void testInstanceGetOrderedInt() throws Exception {
		testGetInt(new IntData(), ORDERED);
	}

	public void testInstanceGetOrderedLong() throws Exception {
		testGetLong(new LongData(), ORDERED);
	}

	public void testInstanceGetOrderedFloat() throws Exception {
		testGetFloat(new FloatData(), ORDERED);
	}

	public void testInstanceGetOrderedDouble() throws Exception {
		testGetDouble(new DoubleData(), ORDERED);
	}

	public void testInstanceGetOrderedBoolean() throws Exception {
		testGetBoolean(new BooleanData(), ORDERED);
	}

	public void testInstanceGetOrderedObject() throws Exception {
		testGetObject(new ObjectData(), ORDERED);
	}

	/* get tests with array as object */
	public void testArrayGetOrderedByte() throws Exception {
		testGetByte(new byte[modelByte.length], ORDERED);
	}

	public void testArrayGetOrderedChar() throws Exception {
		testGetChar(new char[modelChar.length], ORDERED);
	}

	public void testArrayGetOrderedShort() throws Exception {
		testGetShort(new short[modelShort.length], ORDERED);
	}

	public void testArrayGetOrderedInt() throws Exception {
		testGetInt(new int[modelInt.length], ORDERED);
	}

	public void testArrayGetOrderedLong() throws Exception {
		testGetLong(new long[modelLong.length], ORDERED);
	}

	public void testArrayGetOrderedDouble() throws Exception {
		testGetDouble(new double[modelDouble.length], ORDERED);
	}

	public void testArrayGetOrderedBoolean() throws Exception {
		testGetBoolean(new boolean[modelBoolean.length], ORDERED);
	}

	public void testArrayGetOrderedObject() throws Exception {
		testGetObject(new Object[models.length], ORDERED);
	}

	/* get tests with static object */
	public void testStaticGetOrderedByte() throws Exception {
		testGetByte(ByteData.class, ORDERED);
	}

	public void testStaticGetOrderedChar() throws Exception {
		testGetChar(CharData.class, ORDERED);
	}

	public void testStaticGetOrderedShort() throws Exception {
		testGetShort(ShortData.class, ORDERED);
	}

	public void testStaticGetOrderedInt() throws Exception {
		testGetInt(IntData.class, ORDERED);
	}

	public void testStaticGetOrderedLong() throws Exception {
		testGetLong(LongData.class, ORDERED);
	}

	public void testStaticGetOrderedDouble() throws Exception {
		testGetDouble(DoubleData.class, ORDERED);
	}

	public void testStaticGetOrderedBoolean() throws Exception {
		testGetBoolean(BooleanData.class, ORDERED);
	}

	public void testStaticGetOrderedObject() throws Exception {
		testGetObject(ObjectData.class, ORDERED);
	}
}
