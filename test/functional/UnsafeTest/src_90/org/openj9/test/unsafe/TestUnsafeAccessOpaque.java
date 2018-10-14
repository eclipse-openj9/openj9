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
public class TestUnsafeAccessOpaque extends UnsafeTestBase {
	private static Logger logger = Logger.getLogger(TestUnsafeAccessOpaque.class);

	public TestUnsafeAccessOpaque(String scenario) {
		super(scenario);
	}

	/*
	 * get logger to use, for child classes to report with their class name instead
	 * of UnsafeTestBase
	 */
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
	public void testInstancePutOpaqueByte() throws Exception {
		testByte(new ByteData(), OPAQUE);
	}

	public void testInstancePutOpaqueChar() throws Exception {
		testChar(new CharData(), OPAQUE);
	}

	public void testInstancePutOpaqueShort() throws Exception {
		testShort(new ShortData(), OPAQUE);
	}

	public void testInstancePutOpaqueInt() throws Exception {
		testInt(new IntData(), OPAQUE);
	}

	public void testInstancePutOpaqueLong() throws Exception {
		testLong(new LongData(), OPAQUE);
	}

	public void testInstancePutOpaqueFloat() throws Exception {
		testFloat(new FloatData(), OPAQUE);
	}

	public void testInstancePutOpaqueDouble() throws Exception {
		testDouble(new DoubleData(), OPAQUE);
	}

	public void testInstancePutOpaqueBoolean() throws Exception {
		testBoolean(new BooleanData(), OPAQUE);
	}

	public void testInstancePutOpaqueObject() throws Exception {
		testObject(new ObjectData(), OPAQUE);
	}

	/* put tests with array as object */
	public void testArrayPutOpaqueByte() throws Exception {
		testByte(new byte[modelByte.length], OPAQUE);
	}

	public void testArrayPutOpaqueChar() throws Exception {
		testChar(new char[modelChar.length], OPAQUE);
	}

	public void testArrayPutOpaqueShort() throws Exception {
		testShort(new short[modelShort.length], OPAQUE);
	}

	public void testArrayPutOpaqueInt() throws Exception {
		testInt(new int[modelInt.length], OPAQUE);
	}

	public void testArrayPutOpaqueLong() throws Exception {
		testLong(new long[modelLong.length], OPAQUE);
	}

	public void testArrayPutOpaqueFloat() throws Exception {
		testFloat(new float[modelFloat.length], OPAQUE);
	}

	public void testArrayPutOpaqueDouble() throws Exception {
		testDouble(new double[modelDouble.length], OPAQUE);
	}

	public void testArrayPutOpaqueBoolean() throws Exception {
		testBoolean(new boolean[modelBoolean.length], OPAQUE);
	}

	public void testArrayPutOpaqueObject() throws Exception {
		testObject(new Object[models.length], OPAQUE);
	}

	/* put tests with static object */
	public void testStaticPutOpaqueByte() throws Exception {
		testByte(ByteData.class, OPAQUE);
	}

	public void testStaticPutOpaqueChar() throws Exception {
		testChar(CharData.class, OPAQUE);
	}

	public void testStaticPutOpaqueShort() throws Exception {
		testShort(ShortData.class, OPAQUE);
	}

	public void testStaticPutOpaqueInt() throws Exception {
		testInt(IntData.class, OPAQUE);
	}

	public void testStaticPutOpaqueLong() throws Exception {
		testLong(LongData.class, OPAQUE);
	}

	public void testStaticPutOpaqueFloatRelease() throws Exception {
		testFloat(FloatData.class, OPAQUE);
	}

	public void testStaticPutOrderdDouble() throws Exception {
		testDouble(DoubleData.class, OPAQUE);
	}

	public void testStaticPutOpaqueBoolean() throws Exception {
		testBoolean(BooleanData.class, OPAQUE);
	}

	public void testStaticPutOpaqueObject() throws Exception {
		testObject(ObjectData.class, OPAQUE);
	}

	/* tests with null object */
	public void testObjectNullPutOpaqueByte() throws Exception {
		testByteNative(OPAQUE);
	}

	public void testObjectNullPutOpaqueChar() throws Exception {
		testCharNative(OPAQUE);
	}

	public void testObjectNullPutOpaqueShort() throws Exception {
		testShortNative(OPAQUE);
	}

	public void testObjectNullPutOpaqueInt() throws Exception {
		testIntNative(OPAQUE);
	}

	public void testObjectNullPutOpaqueLong() throws Exception {
		testLongNative(OPAQUE);
	}

	public void testObjectNullPutOpaqueFloat() throws Exception {
		testFloatNative(OPAQUE);
	}

	public void testObjectNullPutOpaqueDouble() throws Exception {
		testDoubleNative(OPAQUE);
	}

	public void testObjectNullPutOpaqueBoolean() throws Exception {
		testBooleanNative(OPAQUE);
	}

	/* get test with new instance as object */
	public void testInstanceGetOpaqueByte() throws Exception {
		testGetByte(new ByteData(), OPAQUE);
	}

	public void testInstanceGetOpaqueChar() throws Exception {
		testGetChar(new CharData(), OPAQUE);
	}

	public void testInstanceGetOpaqueShort() throws Exception {
		testGetShort(new ShortData(), OPAQUE);
	}

	public void testInstanceGetOpaqueInt() throws Exception {
		testGetInt(new IntData(), OPAQUE);
	}

	public void testInstanceGetOpaqueLong() throws Exception {
		testGetLong(new LongData(), OPAQUE);
	}

	public void testInstanceGetOpaqueFloat() throws Exception {
		testGetFloat(new FloatData(), OPAQUE);
	}

	public void testInstanceGetOpaqueDouble() throws Exception {
		testGetDouble(new DoubleData(), OPAQUE);
	}

	public void testInstanceGetOpaqueBoolean() throws Exception {
		testGetBoolean(new BooleanData(), OPAQUE);
	}

	public void testInstanceGetOpaqueObject() throws Exception {
		testGetObject(new ObjectData(), OPAQUE);
	}

	/* get tests with array as object */
	public void testArrayGetOpaqueByte() throws Exception {
		testGetByte(new byte[modelByte.length], OPAQUE);
	}

	public void testArrayGetOpaqueChar() throws Exception {
		testGetChar(new char[modelChar.length], OPAQUE);
	}

	public void testArrayGetOpaqueShort() throws Exception {
		testGetShort(new short[modelShort.length], OPAQUE);
	}

	public void testArrayGetOpaqueInt() throws Exception {
		testGetInt(new int[modelInt.length], OPAQUE);
	}

	public void testArrayGetOpaqueLong() throws Exception {
		testGetLong(new long[modelLong.length], OPAQUE);
	}

	public void testArrayGetOpaqueDouble() throws Exception {
		testGetDouble(new double[modelDouble.length], OPAQUE);
	}

	public void testArrayGetOpaqueBoolean() throws Exception {
		testGetBoolean(new boolean[modelBoolean.length], OPAQUE);
	}

	public void testArrayGetOpaqueObject() throws Exception {
		testGetObject(new Object[models.length], OPAQUE);
	}

	/* get tests with static object */
	public void testStaticGetOpaqueByte() throws Exception {
		testGetByte(ByteData.class, OPAQUE);
	}

	public void testStaticGetOpaqueChar() throws Exception {
		testGetChar(CharData.class, OPAQUE);
	}

	public void testStaticGetOpaqueShort() throws Exception {
		testGetShort(ShortData.class, OPAQUE);
	}

	public void testStaticGetOpaqueInt() throws Exception {
		testGetInt(IntData.class, OPAQUE);
	}

	public void testStaticGetOpaqueLong() throws Exception {
		testGetLong(LongData.class, OPAQUE);
	}

	public void testStaticGetOpaqueDouble() throws Exception {
		testGetDouble(DoubleData.class, OPAQUE);
	}

	public void testStaticGetOpaqueBoolean() throws Exception {
		testGetBoolean(BooleanData.class, OPAQUE);
	}

	public void testStaticGetOpaqueObject() throws Exception {
		testGetObject(ObjectData.class, OPAQUE);
	}
}
