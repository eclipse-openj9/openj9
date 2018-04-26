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
public class TestUnsafeGetAndOp extends UnsafeTestBase {
	private String[] testList = { GETANDSET, GETANDSETA, GETANDSETR, GETANDADD, GETANDADDA, GETANDADDR, GETANDBITWISEOR,
			GETANDBITWISEORA, GETANDBITWISEORR, GETANDBITWISEAND, GETANDBITWISEANDA, GETANDBITWISEANDR,
			GETANDBITWISEXOR, GETANDBITWISEXORA, GETANDBITWISEXORR };
	private String[] decimalTestList = { GETANDSET, GETANDSETA, GETANDSETR, GETANDADD, GETANDADDA, GETANDADDR };
	private String[] boolTestList = { GETANDSET, GETANDSETA, GETANDSETR, GETANDBITWISEOR, GETANDBITWISEORA,
			GETANDBITWISEORR, GETANDBITWISEAND, GETANDBITWISEANDA, GETANDBITWISEANDR, GETANDBITWISEXOR,
			GETANDBITWISEXORA, GETANDBITWISEXORR };
	private String[] objTestList = { GETANDSET, GETANDSETA, GETANDSETR };

	private static Logger logger = Logger.getLogger(TestUnsafeGetAndOp.class);

	public TestUnsafeGetAndOp(String scenario) {
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
	public void testInstanceGetAndOpByte() throws Exception {
		for (String test : testList) {
			testGetByte(new ByteData(), test);
		}
	}

	public void testInstanceGetAndOpChar() throws Exception {
		for (String test : testList) {
			testGetChar(new CharData(), test);
		}
	}

	public void testInstanceGetAndOpShort() throws Exception {
		for (String test : testList) {
			testGetShort(new ShortData(), test);
		}
	}

	public void testInstanceGetAndOpInt() throws Exception {
		for (String test : testList) {
			testGetInt(new IntData(), test);
		}
	}

	public void testInstanceGetAndOpLong() throws Exception {
		for (String test : testList) {
			testGetLong(new LongData(), test);
		}
	}

	public void testInstanceGetAndOpFloat() throws Exception {
		for (String test : decimalTestList) {
			testGetFloat(new FloatData(), test);
		}
	}

	public void testInstanceGetAndOpDouble() throws Exception {
		for (String test : decimalTestList) {
			testGetDouble(new DoubleData(), test);
		}
	}

	public void testInstanceGetAndOpBoolean() throws Exception {
		for (String test : boolTestList) {
			testGetBoolean(new BooleanData(), test);
		}
	}

	public void testInstanceGetAndOpObject() throws Exception {
		for (String test : objTestList) {
			testGetObject(new ObjectData(), test);
		}
	}

	/* test with array as object */
	public void testArrayGetAndOpByte() throws Exception {
		for (String test : testList) {
			testGetByte(new byte[modelByte.length], test);
		}
	}

	public void testArrayGetAndOpChar() throws Exception {
		for (String test : testList) {
			testGetChar(new char[modelChar.length], test);
		}
	}

	public void testArrayGetAndOpShort() throws Exception {
		for (String test : testList) {
			testGetShort(new short[modelShort.length], test);
		}
	}

	public void testArrayGetAndOpInt() throws Exception {
		for (String test : testList) {
			testGetInt(new int[modelInt.length], test);
		}
	}

	public void testArrayGetAndOpLong() throws Exception {
		for (String test : testList) {
			testGetLong(new long[modelLong.length], test);
		}
	}

	public void testArrayGetAndOpFloat() throws Exception {
		for (String test : decimalTestList) {
			testGetFloat(new float[modelFloat.length], test);
		}
	}

	public void testArrayGetAndOpDouble() throws Exception {
		for (String test : decimalTestList) {
			testGetDouble(new double[modelDouble.length], test);
		}
	}

	public void testArrayGetAndOpBoolean() throws Exception {
		for (String test : boolTestList) {
			testGetBoolean(new boolean[modelBoolean.length], test);
		}
	}

	public void testArrayGetAndOpObject() throws Exception {
		for (String test : objTestList) {
			testGetObject(new Object[models.length], test);
		}
	}

	/* test with static object */
	public void testStaticGetAndOpByte() throws Exception {
		for (String test : testList) {
			testGetByte(ByteData.class, test);
		}
	}

	public void testStaticGetAndOpChar() throws Exception {
		for (String test : testList) {
			testGetChar(CharData.class, test);
		}
	}

	public void testStaticGetAndOpShort() throws Exception {
		for (String test : testList) {
			testGetShort(ShortData.class, test);
		}
	}

	public void testStaticGetAndOpInt() throws Exception {
		for (String test : testList) {
			testGetInt(IntData.class, test);
		}
	}

	public void testStaticGetAndOpLong() throws Exception {
		for (String test : testList) {
			testGetLong(LongData.class, test);
		}
	}

	public void testStaticGetAndOpFloat() throws Exception {
		for (String test : decimalTestList) {
			testGetFloat(FloatData.class, test);
		}
	}

	public void testStaticGetAndOpDouble() throws Exception {
		for (String test : decimalTestList) {
			testGetDouble(DoubleData.class, test);
		}
	}

	public void testStaticGetAndOpBoolean() throws Exception {
		for (String test : boolTestList) {
			testGetBoolean(BooleanData.class, test);
		}
	}

	public void testStaticGetAndOpObject() throws Exception {
		for (String test : objTestList) {
			testGetObject(ObjectData.class, test);
		}
	}

}
