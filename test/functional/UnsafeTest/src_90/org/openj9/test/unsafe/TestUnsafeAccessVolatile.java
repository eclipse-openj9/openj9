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
public class TestUnsafeAccessVolatile extends UnsafeTestBase {
	private static Logger logger = Logger.getLogger(TestUnsafeAccessVolatile.class);
	
	public TestUnsafeAccessVolatile(String scenario) {
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

	// tests for testInstancePutXXXXVolatile
	public void testInstancePutByteVolatile() throws Exception {
		testByte(new ByteData(), VOLATILE);
	}
	
	public void testInstancePutCharVolatile() throws Exception {
		testChar(new CharData(), VOLATILE);
	}
	
	public void testInstancePutShortVolatile() throws Exception {
		testShort(new ShortData(), VOLATILE);
	}
	
	public void testInstancePutIntVolatile() throws Exception {
		testInt(new IntData(), VOLATILE);
	}
	
	public void testInstancePutLongVolatile() throws Exception {
		testLong(new LongData(), VOLATILE);
	}
	
	public void testInstancePutFloatVolatile() throws Exception {
		testFloat(new FloatData(), VOLATILE);
	}
	
	public void testInstancePutDoubleVolatile() throws Exception {
		testDouble(new DoubleData(), VOLATILE);
	}
	
	public void testInstancePutBooleanVolatile() throws Exception {
		testBoolean(new BooleanData(), VOLATILE);
	}

	public void testInstancePutObjectVolatile() throws Exception {
		testObject(new ObjectData(), VOLATILE);
	}

	// tests for testArrayPutXXXXVolatile
	public void testArrayPutByteVolatile() throws Exception {
		testByte(new byte[modelByte.length], VOLATILE);
	}
	
	public void testArrayPutCharVolatile() throws Exception {
		testChar(new char[modelChar.length], VOLATILE);
	}
	
	public void testArrayPutShortVolatile() throws Exception {
		testShort(new short[modelShort.length], VOLATILE);
	}
	
	public void testArrayPutIntVolatile() throws Exception {
		testInt(new int[modelInt.length], VOLATILE);
	}
	
	public void testArrayPutLongVolatile() throws Exception {
		testLong(new long[modelLong.length], VOLATILE);
	}
	
	public void testArrayPutFloatVolatile() throws Exception {
		testFloat(new float[modelFloat.length], VOLATILE);
	}
	
	public void testArrayPutDoubleVolatile() throws Exception {
		testDouble(new double[modelDouble.length], VOLATILE);
	}
	
	public void testArrayPutBooleanVolatile() throws Exception {
		testBoolean(new boolean[modelBoolean.length], VOLATILE);
	}

	public void testArrayPutObjectVolatile() throws Exception {
		testObject(new Object[models.length], VOLATILE);
	}

	// tests for testStaticPutXXXXVolatile
	public void testStaticPutByteVolatile() throws Exception {
		testByte(ByteData.class, VOLATILE);
	}
	
	public void testStaticPutCharVolatile() throws Exception {
		testChar(CharData.class, VOLATILE);
	}
	
	public void testStaticPutShortVolatile() throws Exception {
		testShort(ShortData.class, VOLATILE);
	}
	
	public void testStaticPutIntVolatile() throws Exception {
		testInt(IntData.class, VOLATILE);
	}
	
	public void testStaticPutLongVolatile() throws Exception {
		testLong(LongData.class, VOLATILE);
	}
	
	public void testStaticPutFloatVolatile() throws Exception {
		testFloat(FloatData.class, VOLATILE);
	}
	
	public void testStaticPutDoubleVolatile() throws Exception {
		testDouble(DoubleData.class, VOLATILE);
	}
	
	public void testStaticPutBooleanVolatile() throws Exception {
		testBoolean(BooleanData.class, VOLATILE);
	}

	public void testStaticPutObjectVolatile() throws Exception {
		testObject(ObjectData.class, VOLATILE);
	}

	// tests for testObjectNullPutXXXXVolatile
	public void testObjectNullPutByteVolatile() throws Exception {
		testByteNative(VOLATILE);
	}
	
	public void testObjectNullPutCharVolatile() throws Exception {
		testCharNative(VOLATILE);
	}
	
	public void testObjectNullPutShortVolatile() throws Exception {
		testShortNative(VOLATILE);
	}
	
	public void testObjectNullPutIntVolatile() throws Exception {
		testIntNative(VOLATILE);
	}
	
	public void testObjectNullPutLongVolatile() throws Exception {
		testLongNative(VOLATILE);
	}
	
	public void testObjectNullPutFloatVolatile() throws Exception {
		testFloatNative(VOLATILE);
	}
	
	public void testObjectNullPutDoubleVolatile() throws Exception {
		testDoubleNative(VOLATILE);
	}
	
	public void testObjectNullPutBooleanVolatile() throws Exception {
		testBooleanNative(VOLATILE);
	}
	
	// tests for testInstanceGetXXXXVolatile
	public void testInstanceGetByteVolatile() throws Exception {
		testGetByte(new ByteData(), VOLATILE);
	}
	
	public void testInstanceGetCharVolatile() throws Exception {
		testGetChar(new CharData(), VOLATILE);
	}

	public void testInstanceGetShortVolatile() throws Exception {
		testGetShort(new ShortData(), VOLATILE);
	}

	public void testInstanceGetIntVolatile() throws Exception {
		testGetInt(new IntData(), VOLATILE);
	}
	
	public void testInstanceGetLongVolatile() throws Exception {
		testGetLong(new LongData(), VOLATILE);
	}
	
	public void testInstanceGetFloatVolatile() throws Exception {
		testGetFloat(new FloatData(), VOLATILE);		
	}
	
	public void testInstanceGetDoubleVolatile() throws Exception {
		testGetDouble(new DoubleData(), VOLATILE);
	}
	
	public void testInstanceGetBooleanVolatile() throws Exception {
		testGetBoolean(new BooleanData(), VOLATILE);
	}

	public void testInstanceGetObjectVolatile() throws Exception {
		testGetObject(new ObjectData(), VOLATILE);
	}

	// tests for testArrayGetXXXXVolatile
	public void testArrayGetByteVolatile() throws Exception {
		testGetByte(new byte[modelByte.length], VOLATILE);
	}
	
	public void testArrayGetCharVolatile() throws Exception {
		testGetChar(new char[modelChar.length], VOLATILE);
	}
	
	public void testArrayGetShortVolatile() throws Exception {
		testGetShort(new short[modelShort.length], VOLATILE);
	}
	
	public void testArrayGetIntVolatile() throws Exception {
		testGetInt(new int[modelInt.length], VOLATILE);
	}
	
	public void testArrayGetLongVolatile() throws Exception {
		testGetLong(new long[modelLong.length], VOLATILE);
	}
	
	public void testArrayGetFloatVolatile() throws Exception {
		testGetFloat(new float[modelFloat.length], VOLATILE);
	}
	
	public void testArrayGetDoubleVolatile() throws Exception {
		testGetDouble(new double[modelDouble.length], VOLATILE);
	}
	
	public void testArrayGetBooleanVolatile() throws Exception {
		testGetBoolean(new boolean[modelBoolean.length], VOLATILE);
	}

	public void testArrayGetObjectVolatile() throws Exception {
		testGetObject(new Object[models.length], VOLATILE);
	}

	// tests for testStaticGetXXXXVolatile
	public void testStaticGetByteVolatile() throws Exception {
		testGetByte(ByteData.class, VOLATILE);
	}
	
	public void testStaticGetCharVolatile() throws Exception {
		testGetChar(CharData.class, VOLATILE);
	}
	
	public void testStaticGetShortVolatile() throws Exception {
		testGetShort(ShortData.class, VOLATILE);
	}
	
	public void testStaticGetIntVolatile() throws Exception {
		testGetInt(IntData.class, VOLATILE);
	}
	
	public void testStaticGetLongVolatile() throws Exception {
		testGetLong(LongData.class, VOLATILE);
	}
	
	public void testStaticGetFloatVolatile() throws Exception {
		testGetFloat(FloatData.class, VOLATILE);
	}
	
	public void testStaticGetDoubleVolatile() throws Exception {
		testGetDouble(DoubleData.class, VOLATILE);
	}
	
	public void testStaticGetBooleanVolatile() throws Exception {
		testGetBoolean(BooleanData.class, VOLATILE);
	}

	public void testStaticGetObjectVolatile() throws Exception {
		testGetObject(ObjectData.class, VOLATILE);
	}
}
