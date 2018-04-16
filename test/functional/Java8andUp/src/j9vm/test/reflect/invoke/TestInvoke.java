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
package j9vm.test.reflect.invoke;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeMethod;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.reflect.Method;

@Test(groups = { "level.sanity" })
public class TestInvoke {

	private static final Logger logger = Logger.getLogger(TestInvoke.class);

	MethodsClass methodsClass;
	Method method;

	@BeforeMethod
	public void setUp() {
		methodsClass = new MethodsClass();
		method = null;
	}

	@Test
	public void testInt() {
		logger.debug("Testing for int...");
		int[] expected = { Integer.MAX_VALUE, Integer.MIN_VALUE, 5, Float.MAX_EXPONENT, Float.MIN_EXPONENT };
		int[] actual = new int[expected.length];
		try {
			method = methodsClass.getClass().getMethod("getMethod", new Class[] { Integer.TYPE });
			for (int i = 0; i < expected.length; i++) {
				actual[i] = ((Integer)method.invoke(methodsClass, new Object[] { expected[i] })).intValue();
				AssertJUnit.assertEquals(expected[i], actual[i]);
			}
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Unexpected exception thrown : " + e.toString());
		}
	}

	@Test
	public void testDouble() {
		logger.debug("Testing for double...");
		double[] expected = {Double.MAX_VALUE, Double.MIN_VALUE, 2.01, Double.MAX_EXPONENT, Double.MIN_EXPONENT,
				Double.MIN_NORMAL,Double.NaN,Double.NEGATIVE_INFINITY,Double.POSITIVE_INFINITY };
		double[] actual = new double[expected.length];
		try {
			method = methodsClass.getClass().getMethod("getMethod", new Class[] { Double.TYPE });
			for (int i = 0; i < expected.length; i++) {
				actual[i] = ((Double)method.invoke(methodsClass, new Object[] { expected[i] })).doubleValue();
				AssertJUnit.assertEquals(expected[i], actual[i]);
			}
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Unexpected exception thrown : " + e.toString());
		}
	}

	@Test
	public void testFloat() {
		logger.debug("Testing for float...");
		Float[] expected = {
				Float.MAX_VALUE,
				Float.MIN_VALUE,
				Float.MIN_NORMAL,
				Float.NaN,
				Float.NEGATIVE_INFINITY,
				Float.POSITIVE_INFINITY,
				3.0f };
		Float[] actual = new Float[expected.length];
		try {
			method = methodsClass.getClass().getMethod("getMethod", new Class[] { Float.TYPE });
			for (int i = 0; i < expected.length; i++) {
				actual[i] = ((Float)method.invoke(methodsClass, new Object[] { expected[i] })).floatValue();
				AssertJUnit.assertEquals(expected[i], actual[i]);
			}
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Unexpected exception thrown : " + e.toString());
		}
	}

	@Test
	public void testShort() {
		logger.debug("Testing for short...");
		Short[] expected = { Short.MAX_VALUE, Short.MIN_VALUE, 300 };
		Short[] actual = new Short[expected.length];
		try {
			method = methodsClass.getClass().getMethod("getMethod", new Class[] { Short.TYPE });
			for (int i = 0; i < expected.length; i++) {
				actual[i] = ((Short)method.invoke(methodsClass, new Object[] { expected[i] })).shortValue();
				AssertJUnit.assertEquals(expected[i], actual[i]);
			}
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Unexpected exception thrown : " + e.toString());
		}
	}

	@Test
	public void testLong() {
		logger.debug("Testing for long...");
		Long[] expected = { Long.MAX_VALUE, Long.MIN_VALUE, (long)300 };
		Long[] actual = new Long[expected.length];
		try {
			method = methodsClass.getClass().getMethod("getMethod", new Class[] { Long.TYPE });
			for (int i = 0; i < expected.length; i++) {
				actual[i] = ((Long)method.invoke(methodsClass, new Object[] { expected[i] })).longValue();
				AssertJUnit.assertEquals(expected[i], actual[i]);
			}
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Unexpected exception thrown : " + e.toString());
		}
	}

	@Test
	public void testByte() {
		logger.debug("Testing for byte...");
		Byte[] expected = { Byte.MAX_VALUE, Byte.MIN_VALUE, 64 };
		Byte[] actual = new Byte[expected.length];
		try {
			method = methodsClass.getClass().getMethod("getMethod", new Class[] { Byte.TYPE });
			for (int i = 0; i < expected.length; i++) {
				actual[i] = ((Byte)method.invoke(methodsClass, new Object[] { expected[i] })).byteValue();
				AssertJUnit.assertEquals(expected[i], actual[i]);
			}
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Unexpected exception thrown : " + e.toString());
		}
	}

	@Test
	public void testByteArray() {
		logger.debug("Testing for byte[]...");
		Byte[] expected = { Byte.MAX_VALUE, Byte.MIN_VALUE, 64 };
		Byte[] actual = new Byte[expected.length];
		try {
			method = methodsClass.getClass().getMethod("getMethod", new Class[] { byte[].class });
			for (int i = 0; i < expected.length; i++) {
				actual[i] = ((byte[])method.invoke(methodsClass, new Object[] { new byte[] { expected[i] } }))[0];
				AssertJUnit.assertEquals(expected[i], actual[i]);
			}

		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Unexpected exception thrown : " + e.toString());
		}
	}

	@Test
	public void testChar() {
		logger.debug("Testing for char...");
		Character[] expected = {
				Character.MAX_VALUE,
				Character.MIN_VALUE,
				Character.MAX_HIGH_SURROGATE,
				Character.MAX_LOW_SURROGATE,
				Character.MIN_HIGH_SURROGATE,
				Character.MIN_LOW_SURROGATE,
				't' };
		Character[] actual = new Character[expected.length];
		try {
			method = methodsClass.getClass().getMethod("getMethod", new Class[] { Character.TYPE });
			for (int i = 0; i < expected.length; i++) {
				actual[i] = ((Character)method.invoke(methodsClass, new Object[] { expected[i] })).charValue();
				AssertJUnit.assertEquals(expected[i], actual[i]);
			}
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Unexpected exception thrown : " + e.toString());
		}
	}

	@Test
	public void testBoolean() {
		logger.debug("Testing for boolean...");
		Boolean[] expected = { Boolean.TRUE, Boolean.FALSE };
		Boolean[] actual = new Boolean[expected.length];
		try {
			method = methodsClass.getClass().getMethod("getMethod", new Class[] { Boolean.TYPE });
			for (int i = 0; i < expected.length; i++) {
				actual[i] = ((Boolean)method.invoke(methodsClass, new Object[] { expected[i] })).booleanValue();
				AssertJUnit.assertEquals(expected[i], actual[i]);
			}
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Unexpected exception thrown : " + e.toString());
		}
	}
}
