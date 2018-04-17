/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
public class TestUnsafeAccess extends UnsafeTestBase {
	private static Logger logger = Logger.getLogger(TestUnsafeAccess.class);
	
	public TestUnsafeAccess(String scenario) {
		super(scenario);
	}
	
	/* get logger to use, for child classes to report with their class name instead of UnsafeTestBase*/
	@Override
	protected Logger getLogger() {
		return logger;
	}

	public void testInstancePutByte() throws Exception {
		testByte(new ByteData(), DEFAULT);
	}
	
	public void testInstancePutChar() throws Exception {
		testChar(new CharData(), DEFAULT);
	}
	
	public void testInstancePutShort() throws Exception {
		testShort(new ShortData(), DEFAULT);
	}
	
	public void testInstancePutInt() throws Exception {
		testInt(new IntData(), DEFAULT);
	}
	
	public void testInstancePutLong() throws Exception {
		testLong(new LongData(), DEFAULT);
	}
	
	public void testInstancePutFloat() throws Exception {
		testFloat(new FloatData(), DEFAULT);
	}
	
	public void testInstancePutDouble() throws Exception {
		testDouble(new DoubleData(), DEFAULT);
	}
	
	public void testInstancePutBoolean() throws Exception {
		testBoolean(new BooleanData(), DEFAULT);
	}	

	public void testArrayPutByte() throws Exception {
		testByte(new byte[modelByte.length], DEFAULT);
	}
	
	public void testArrayPutChar() throws Exception {
		testChar(new char[modelChar.length], DEFAULT);
	}
	
	public void testArrayPutShort() throws Exception {
		testShort(new short[modelShort.length], DEFAULT);
	}
	
	public void testArrayPutInt() throws Exception {
		testInt(new int[modelInt.length], DEFAULT);
	}
	
	public void testArrayPutLong() throws Exception {
		testLong(new long[modelLong.length], DEFAULT);
	}
	
	public void testArrayPutFloat() throws Exception {
		testFloat(new float[modelFloat.length], DEFAULT);
	}
	
	public void testArrayPutDouble() throws Exception {
		testDouble(new double[modelDouble.length], DEFAULT);
	}
	
	public void testArrayPutBoolean() throws Exception {
		testBoolean(new boolean[modelBoolean.length], DEFAULT);
	}

	public void testStaticPutByte() throws Exception {
		testByte(ByteData.class, DEFAULT);
	}
	
	public void testStaticPutChar() throws Exception {
		testChar(CharData.class, DEFAULT);
	}
	
	public void testStaticPutShort() throws Exception {
		testShort(ShortData.class, DEFAULT);
	}
	
	public void testStaticPutInt() throws Exception {
		testInt(IntData.class, DEFAULT);
	}
	
	public void testStaticPutLong() throws Exception {
		testLong(LongData.class, DEFAULT);
	}
	
	public void testStaticPutFloat() throws Exception {
		testFloat(FloatData.class, DEFAULT);
	}
	
	public void testStaticPutDouble() throws Exception {
		testDouble(DoubleData.class, DEFAULT);
	}
	
	public void testStaticPutBoolean() throws Exception {
		testBoolean(BooleanData.class, DEFAULT);
	}	
	
	public void testObjectNullPutByte() throws Exception {
	
		testByteNative(DEFAULT);
	}
	
	public void testObjectNullPutChar() throws Exception {
		testCharNative(DEFAULT);
	}
	
	public void testObjectNullPutShort() throws Exception {
		testShortNative(DEFAULT);
	}
	
	public void testObjectNullPutInt() throws Exception {
		testIntNative(DEFAULT);
	}
	
	public void testObjectNullPutLong() throws Exception {
		testLongNative(DEFAULT);
	}
	
	public void testObjectNullPutFloat() throws Exception {
		testFloatNative(DEFAULT);
	}
	
	public void testObjectNullPutDouble() throws Exception {
		testDoubleNative(DEFAULT);
	}
	
	public void testObjectNullPutBoolean() throws Exception {
		testBooleanNative(DEFAULT);
	}
	
	public void testPutByte() throws Exception {
		String method = mTestName + "(long, value)";
		testByteNative(method);
	}
	
	public void testPutChar() throws Exception {
		String method = mTestName + "(long, value)";
		testCharNative(method);
	}
	
	public void testPutShort() throws Exception {
		String method = mTestName + "(long, value)";
		testShortNative(method);
	}
	
	public void testPutInt() throws Exception {
		String method = mTestName + "(long, value)";
		testIntNative(method);
	}
	
	public void testPutLong() throws Exception {
		String method = mTestName + "(long, value)";
		testLongNative(method);
	}
	
	public void testPutFloat() throws Exception {
		String method = mTestName + "(long, value)";
		testFloatNative(method);
	}
	
	public void testPutDouble() throws Exception {
		String method = mTestName + "(long, value)";
		testDoubleNative(method);
	}
	
	public void testInstanceGetByte() throws Exception {
		testGetByte(new ByteData(), DEFAULT);
	}
	
	public void testInstanceGetChar() throws Exception {
		testGetChar(new CharData(), DEFAULT);
	}

	public void testInstanceGetShort() throws Exception {
		testGetShort(new ShortData(), DEFAULT);
	}

	public void testInstanceGetInt() throws Exception {
		testGetInt(new IntData(), DEFAULT);
	}
	
	public void testInstanceGetLong() throws Exception {
		testGetLong(new LongData(), DEFAULT);
	}
	
	public void testInstanceGetFloat() throws Exception {
		testGetFloat(new FloatData(), DEFAULT);		
	}
	
	public void testInstanceGetDouble() throws Exception {
		testGetDouble(new DoubleData(), DEFAULT);
	}
	
	public void testInstanceGetBoolean() throws Exception {
		testGetBoolean(new BooleanData(), DEFAULT);
	}
	
	public void testArrayGetByte() throws Exception {
		testGetByte(new byte[modelByte.length], DEFAULT);
	}
	
	public void testArrayGetChar() throws Exception {
		testGetChar(new char[modelChar.length], DEFAULT);
	}
	
	public void testArrayGetShort() throws Exception {
		testGetShort(new short[modelShort.length], DEFAULT);
	}
	
	public void testArrayGetInt() throws Exception {
		testGetInt(new int[modelInt.length], DEFAULT);
	}
	
	public void testArrayGetLong() throws Exception {
		testGetLong(new long[modelLong.length], DEFAULT);
	}
	
	public void testArrayGetFloat() throws Exception {
		testGetFloat(new float[modelFloat.length], DEFAULT);
	}
	
	public void testArrayGetDouble() throws Exception {
		testGetDouble(new double[modelDouble.length], DEFAULT);
	}
	
	public void testArrayGetBoolean() throws Exception {
		testGetBoolean(new boolean[modelBoolean.length], DEFAULT);
	}
	
	public void testStaticGetByte() throws Exception {
		testGetByte(ByteData.class, DEFAULT);
	}
	
	public void testStaticGetChar() throws Exception {
		testGetChar(CharData.class, DEFAULT);
	}
	
	public void testStaticGetShort() throws Exception {
		testGetShort(ShortData.class, DEFAULT);
	}
	
	public void testStaticGetInt() throws Exception {
		testGetInt(IntData.class, DEFAULT);
	}
	
	public void testStaticGetLong() throws Exception {
		testGetLong(LongData.class, DEFAULT);
	}
	
	public void testStaticGetFloat() throws Exception {
		testGetFloat(FloatData.class, DEFAULT);
	}
	
	public void testStaticGetDouble() throws Exception {
		testGetDouble(DoubleData.class, DEFAULT);
	}
	
	public void testStaticGetBoolean() throws Exception {
		testGetBoolean(BooleanData.class, DEFAULT);
	}
}
