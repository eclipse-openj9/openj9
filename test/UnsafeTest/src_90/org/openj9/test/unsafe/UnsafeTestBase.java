/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package org.openj9.test.unsafe;

import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

import org.testng.AssertJUnit;
import org.testng.ITest;
import org.testng.annotations.AfterMethod;
import org.testng.annotations.BeforeMethod;
import org.testng.log4testng.Logger;
import jdk.internal.misc.Unsafe;

public class UnsafeTestBase implements ITest {
	private static Logger logger = Logger.getLogger(UnsafeTestBase.class);
	protected static Unsafe myUnsafe;
	protected final String VOLATILE = "Volatile";
	protected final String ORDERED = "Ordered";
	protected final String COMPAREANDSWAP = "CompareAndSwap";
	protected final String DEFAULT = "put";
	protected final static String ADDRESS = "address";
	protected long mem = 0;
	
	/* variables required to change test name based on run scenario (compiled first or not) */
	protected String scenario;
	protected String mTestName = "";
	
	/* use scenario string to change TestNG output showing if the run is regular or with compiled classes */
	public UnsafeTestBase (String runScenario) {
		scenario = runScenario;
	}
	
	@BeforeMethod(alwaysRun = true)
	public void setTestName(Method method, Object[] parameters) {
		mTestName = method.getName();
		logger.debug("Test started: " + mTestName);
	}
	
	@Override
	public String getTestName() {
		return mTestName  + " (" + scenario + ")";
	}
	
	/* get logger to use, for child classes to report with their class name instead of UnsafeTestBase*/
	protected Logger getLogger() {
		return logger;
	}

	/* use reflection to bypass the security manager */
	protected static Unsafe getUnsafeInstance() throws IllegalAccessException {
		/* the instance is likely stored in a static field of type Unsafe */
		Field[] staticFields = Unsafe.class.getDeclaredFields();
		for (Field field : staticFields) {
			if (field.getType() == Unsafe.class) {
				field.setAccessible(true);
				return (Unsafe) field.get(Unsafe.class);
			}
		}
		throw new Error("Unable to find an instance of Unsafe");
	}

	protected static Unsafe getUnsafeInstance2() throws IllegalAccessException,
			NoSuchFieldException, SecurityException {
		Field field = jdk.internal.misc.Unsafe.class.getDeclaredField("theUnsafe");
		field.setAccessible(true);
		Unsafe unsafe = (jdk.internal.misc.Unsafe) field.get(null);
		return unsafe;
	}

	protected static final byte[] modelByte = new byte[] { -1, -1, -2, -3, -4,
			1, 2, 3, 4, Byte.MAX_VALUE, Byte.MIN_VALUE, 0 };
	protected static final char[] modelChar = new char[] { (char) -1,
			(char) -1, 1, 2, Character.MAX_VALUE, Character.MAX_VALUE - 1, 0 };
	protected static final short[] modelShort = new short[] { -1, -1, -2, 1, 2,
			Short.MAX_VALUE, Short.MIN_VALUE, 0 };
	protected static final int[] modelInt = new int[] { -1, -1, 1, 2,
			Integer.MAX_VALUE, Integer.MIN_VALUE, 0 };
	protected static final long[] modelLong = new long[] { -1, -1, 1,
			Long.MAX_VALUE, Long.MIN_VALUE, 0 };
	protected static final float[] modelFloat = new float[] { -1f, -1f, 1.0f,
			2.0f, Float.MAX_VALUE, Float.MIN_VALUE, 0.0f };
	protected static final double[] modelDouble = new double[] { -1, -1, 1.0,
			Double.MAX_VALUE, Double.MIN_VALUE, 0.0 };
	protected static final boolean[] modelBoolean = new boolean[] { true, false };
	protected static final Object[] models = new Object[] { modelByte,
			modelChar, modelShort, modelInt, modelLong, modelFloat,
			modelDouble, modelBoolean };

	static abstract class Data {
		abstract void init();

		Object get(int i) {
			throw new Error("Invalid field index " + i);
		}

		Object getStatic(int i) {
			throw new Error("Invalid static field index " + i);
		}
	}

	static class ByteData extends Data {
		byte f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11;
		static byte sf0, sf1, sf2, sf3, sf4, sf5, sf6, sf7, sf8, sf9, sf10,
				sf11;

		void init() {
			f0 = sf0 = modelByte[0];
			f1 = sf1 = modelByte[1];
			f2 = sf2 = modelByte[2];
			f3 = sf3 = modelByte[3];
			f4 = sf4 = modelByte[4];
			f5 = sf5 = modelByte[5];
			f6 = sf6 = modelByte[6];
			f7 = sf7 = modelByte[7];
			f8 = sf8 = modelByte[8];
			f9 = sf9 = modelByte[9];
			f10 = sf10 = modelByte[10];
			f11 = sf11 = modelByte[11];
		}

		Object get(int i) {
			return new byte[] { f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10,
					f11 }[i];
		}

		Object getStatic(int i) {
			return new byte[] { sf0, sf1, sf2, sf3, sf4, sf5, sf6, sf7, sf8,
					sf9, sf10, f11 }[i];
		}
	}

	static class CharData extends Data {
		char f0, f1, f2, f3, f4, f5, f6;
		static char sf0, sf1, sf2, sf3, sf4, sf5, sf6;

		void init() {
			f0 = sf0 = modelChar[0];
			f1 = sf1 = modelChar[1];
			f2 = sf2 = modelChar[2];
			f3 = sf3 = modelChar[3];
			f4 = sf4 = modelChar[4];
			f5 = sf5 = modelChar[5];
			f6 = sf6 = modelChar[6];
		}

		Object get(int i) {
			return new char[] { f0, f1, f2, f3, f4, f5, f6 }[i];
		}

		Object getStatic(int i) {
			return new char[] { sf0, sf1, sf2, sf3, sf4, sf5, sf6 }[i];
		}
	}

	static class ShortData extends Data {
		short f0, f1, f2, f3, f4, f5, f6, f7;
		static short sf0, sf1, sf2, sf3, sf4, sf5, sf6, sf7;

		void init() {
			f0 = sf0 = modelShort[0];
			f1 = sf1 = modelShort[1];
			f2 = sf2 = modelShort[2];
			f3 = sf3 = modelShort[3];
			f4 = sf4 = modelShort[4];
			f5 = sf5 = modelShort[5];
			f6 = sf6 = modelShort[6];
			f7 = sf7 = modelShort[7];
		}

		Object get(int i) {
			return new short[] { f0, f1, f2, f3, f4, f5, f6, f7 }[i];
		}

		Object getStatic(int i) {
			return new short[] { sf0, sf1, sf2, sf3, sf4, sf5, sf6, sf7 }[i];
		}
	}

	static class IntData extends Data {
		int f0, f1, f2, f3, f4, f5, f6;
		static int sf0, sf1, sf2, sf3, sf4, sf5, sf6;

		void init() {
			f0 = sf0 = modelInt[0];
			f1 = sf1 = modelInt[1];
			f2 = sf2 = modelInt[2];
			f3 = sf3 = modelInt[3];
			f4 = sf4 = modelInt[4];
			f5 = sf5 = modelInt[5];
			f6 = sf6 = modelInt[6];
		}

		Object get(int i) {
			return new int[] { f0, f1, f2, f3, f4, f5, f6 }[i];
		}

		Object getStatic(int i) {
			return new int[] { sf0, sf1, sf2, sf3, sf4, sf5, sf6 }[i];
		}
	}

	static class LongData extends Data {
		long f0, f1, f2, f3, f4, f5;
		static long sf0, sf1, sf2, sf3, sf4, sf5;

		void init() {
			f0 = sf0 = modelLong[0];
			f1 = sf1 = modelLong[1];
			f2 = sf2 = modelLong[2];
			f3 = sf3 = modelLong[3];
			f4 = sf4 = modelLong[4];
			f5 = sf5 = modelLong[5];
		}

		Object get(int i) {
			return new long[] { f0, f1, f2, f3, f4, f5 }[i];
		}

		Object getStatic(int i) {
			return new long[] { sf0, sf1, sf2, sf3, sf4, sf5 }[i];
		}
	}

	static class FloatData extends Data {
		float f0, f1, f2, f3, f4, f5, f6;
		static float sf0, sf1, sf2, sf3, sf4, sf5, sf6;

		void init() {
			f0 = sf0 = modelFloat[0];
			f1 = sf1 = modelFloat[1];
			f2 = sf2 = modelFloat[2];
			f3 = sf3 = modelFloat[3];
			f4 = sf4 = modelFloat[4];
			f5 = sf5 = modelFloat[5];
			f6 = sf6 = modelFloat[6];
		}

		Object get(int i) {
			return new float[] { f0, f1, f2, f3, f4, f5, f6 }[i];
		}

		Object getStatic(int i) {
			return new float[] { sf0, sf1, sf2, sf3, sf4, sf5, sf6 }[i];
		}
	}

	static class DoubleData extends Data {
		double f0, f1, f2, f3, f4, f5;
		static double sf0, sf1, sf2, sf3, sf4, sf5;

		void init() {
			f0 = sf0 = modelDouble[0];
			f1 = sf1 = modelDouble[1];
			f2 = sf2 = modelDouble[2];
			f3 = sf3 = modelDouble[3];
			f4 = sf4 = modelDouble[4];
			f5 = sf5 = modelDouble[5];
		}

		Object get(int i) {
			return new double[] { f0, f1, f2, f3, f4, f5 }[i];
		}

		Object getStatic(int i) {
			return new double[] { sf0, sf1, sf2, sf3, sf4, sf5 }[i];
		}
	}

	static class BooleanData extends Data {
		boolean f0, f1;
		static boolean sf0, sf1;

		void init() {
			f0 = sf0 = modelBoolean[0];
			f1 = sf1 = modelBoolean[1];

		}

		Object get(int i) {
			return new boolean[] { f0, f1 }[i];
		}

		Object getStatic(int i) {
			return new boolean[] { sf0, sf1 }[i];
		}
	}

	protected long offset(Object object, int index) throws NoSuchFieldException {
		if (object instanceof Class)
			return myUnsafe.staticFieldOffset(((Class) object)
					.getDeclaredField("sf" + index));
		if (object instanceof Data)
			return myUnsafe.objectFieldOffset(object.getClass()
					.getDeclaredField("f" + index));
		if (object.getClass().isArray())
			return myUnsafe.arrayBaseOffset(object.getClass())
					+ (myUnsafe.arrayIndexScale(object.getClass()) * index);
		throw new Error("Unable to get offset " + index
				+ " from unknown object type " + object.getClass());
	}

	protected Object base(Object object, int index) throws NoSuchFieldException {
		if (object == null)
			return null;
		else if (object instanceof Class)
			return myUnsafe.staticFieldBase(((Class) object)
					.getDeclaredField("sf" + index));
		return object;
	}

	protected static void init(Object object) throws InstantiationException,
			IllegalAccessException {
		if (object instanceof Class)
			((Data) ((Class) object).newInstance()).init();
		if (object instanceof Data)
			((Data) object).init();
		if (object.getClass().isArray()) {
			for (Object model : models) {
				if (model.getClass().equals(object.getClass())) {
					System.arraycopy(model, 0, object, 0,
							Array.getLength(model));
				}
			}
		}
	}

	protected static Object slot(Object object, int index)
			throws InstantiationException, IllegalAccessException {
		if (object instanceof Class)
			return ((Data) ((Class) object).newInstance()).getStatic(index);
		if (object instanceof Data)
			return ((Data) object).get(index);
		if (object.getClass().isArray())
			return Array.get(object, index);
		throw new Error("Can't get slot " + index + " of unknown object type");
	}

	protected void modelCheck(Object model, Object object, int limit)
			throws Exception {
		for (int i = 0; i <= limit; i++) {
			getLogger().debug("Index: " + i + " Expected: " + Array.get(model, i)
					+ " Actual: " + slot(object, i));
			AssertJUnit.assertEquals(Array.get(model, i), slot(object, i));
		}
	}

	protected void modelCheck(Object model, int limit, long[] pointers) {
		for (int i = 0; i <= limit; i++) {
			Byte value = myUnsafe.getByte(null, pointers[i]);
			getLogger().debug("getByte - Index: " + i + ", Expected: "
					+ Array.get(model, i) + ", Actual: " + value);
			AssertJUnit.assertEquals(Array.get(model, i), value);
			value = (byte) myUnsafe.getShort(null, pointers[i]);
			getLogger().debug("getShort - Index: " + i + ", Expected: "
					+ Array.get(model, i) + ", Actual: " + value);
			AssertJUnit.assertEquals(Array.get(model, i), value);
		}
	}

	protected void checkSameAt(int i, Object target, Object value)
			throws InstantiationException, IllegalAccessException {
		getLogger().debug("Index: " + i + " Expected: " + slot(target, i) + " Actual:"
				+ value);
		AssertJUnit.assertEquals(slot(target, i), value);
	}

	@BeforeMethod(groups = { "level.sanity" })
	protected void setUp() throws Exception {
		myUnsafe = getUnsafeInstance();
	}

	
	@AfterMethod(groups = { "level.sanity" })
	protected void tearDown() throws Exception {
		logger.debug("Test finished: " + mTestName);
		if(mem != 0){
			freeMemory();
			mem = 0;
		}
	}

	protected void testByte(Object target, String method) throws Exception {
		for (int i = 0; i < modelByte.length; i++) {
			long offset = offset(target, i);
			getLogger().debug("testByte Method: " + method + "- Object: "
					+ target.getClass().getName() + ", Offset: " + offset
					+ ", Data: " + modelByte[i] + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				myUnsafe.putByteVolatile(base(target, i), offset, modelByte[i]);
			}
			 if (method.equals(ORDERED)) {
				myUnsafe.putObjectRelease(base(target, i), offset, new Byte(
						(byte) -1));
			} else {
				myUnsafe.putByte(base(target, i), offset, modelByte[i]);
			}
			modelCheck(modelByte, target, i);

		}
	}

	protected void testChar(Object target, String method) throws Exception {
		for (int i = 0; i < modelChar.length; i++) {
			long offset = offset(target, i);
			getLogger().debug("testChar Method: " + method + "- Object: "
					+ target.getClass().getName() + ", Offset: " + offset
					+ ", Data: " + modelChar[i] + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				myUnsafe.putCharVolatile(base(target, i), offset, modelChar[i]);
			} else {
				myUnsafe.putChar(base(target, i), offset, modelChar[i]);
			}
			modelCheck(modelChar, target, i);
		}
	}

	protected void testShort(Object target, String method) throws Exception {
		for (int i = 0; i < modelShort.length; i++) {
			long offset = offset(target, i);
			getLogger().debug("testShort Method: " + method + "- Object: "
					+ target.getClass().getName() + ", Offset: " + offset
					+ ", Data: " + modelShort[i] + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				myUnsafe.putShortVolatile(base(target, i), offset,
						modelShort[i]);
			} else {
				myUnsafe.putShort(base(target, i), offset(target, i),
						modelShort[i]);
			}
			modelCheck(modelShort, target, i);
		}
	}

	protected void testInt(Object target, String method) throws Exception {
		for (int i = 0; i < modelInt.length; i++) {
			long offset = offset(target, i);
			getLogger().debug("testInt Method: " + method + "- Object: "
					+ target.getClass().getName() + ", Offset: " + offset
					+ ", Data: " + modelInt[i] + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				myUnsafe.putIntVolatile(base(target, i), offset, modelInt[i]);
			// API is not supported in b181 java9, need to verify if test case is needed
//			} else if (method.equals(COMPAREANDSWAP)) {
//				myUnsafe.putInt(base(target, i), offset, -1);
//				myUnsafe.compareAndSwapInt(base(target, i), offset, -1,
//						modelInt[i]);
			} else if (method.equals(ORDERED)) {
				myUnsafe.putIntRelease(base(target, i), offset, modelInt[i]);
			} else {
				myUnsafe.putInt(base(target, i), offset, modelInt[i]);
			}
			modelCheck(modelInt, target, i);
		}
	}

	protected void testLong(Object target, String method) throws Exception {
		for (int i = 0; i < modelLong.length; i++) {
			long offset = offset(target, i);
			getLogger().debug("testLong Method: " + method + "- Object: "
					+ target.getClass().getName() + ", Offset: " + offset
					+ ", Data: " + modelLong[i] + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				myUnsafe.putLongVolatile(base(target, i), offset, modelLong[i]);
			} else if (method.equals(ORDERED)) {
				myUnsafe.putLongRelease(base(target, i), offset, modelLong[i]);
			// API is not supported in b181 java9, need to verify if test case is needed
//			} else if (method.equals(COMPAREANDSWAP)) {
//				myUnsafe.putLong(base(target, i), offset, -1);
//				myUnsafe.compareAndSwapLong(base(target, i), offset, -1,
//						modelLong[i]);
			} else {
				myUnsafe.putLong(base(target, i), offset, modelLong[i]);
			}
			modelCheck(modelLong, target, i);
		}
	}

	protected void testFloat(Object target, String method) throws Exception {
		for (int i = 0; i < modelFloat.length; i++) {
			long offset = offset(target, i);
			getLogger().debug("testFloat Method: " + method + "- Object: "
					+ target.getClass().getName() + ", Offset: " + offset
					+ ", Data: " + modelFloat[i] + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				myUnsafe.putFloatVolatile(base(target, i), offset,
						modelFloat[i]);
			} else {
				myUnsafe.putFloat(base(target, i), offset, modelFloat[i]);
			}
			modelCheck(modelFloat, target, i);
		}
	}

	protected void testDouble(Object target, String method) throws Exception {
		for (int i = 0; i < modelDouble.length; i++) {
			long offset = offset(target, i);
			getLogger().debug("testDouble Object: " + target.getClass().getName() + ", Offset: "
					+ offset + ", Data: " + modelDouble[i] + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				myUnsafe.putDoubleVolatile(base(target, i), offset,
						modelDouble[i]);
			} else {
				myUnsafe.putDouble(base(target, i), offset, modelDouble[i]);
			}
			modelCheck(modelDouble, target, i);
		}
	}

	protected void testBoolean(Object target, String method) throws Exception {
		for (int i = 0; i < modelBoolean.length; i++) {
			long offset = offset(target, i);
			getLogger().debug("testBoolean Method: " + method + "- Object: "
					+ target.getClass().getName() + ", Offset: " + offset
					+ ", Data: " + modelBoolean[i] + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				myUnsafe.putBooleanVolatile(base(target, i), offset,
						modelBoolean[i]);
			} else {
				myUnsafe.putBoolean(base(target, i), offset, modelBoolean[i]);
			}
			modelCheck(modelBoolean, target, i);
		}
	}

	protected void testGetByte(Object target, String method) throws Exception {
		init(target);
		for (int i = 0; i < modelByte.length; i++) {
			getLogger().debug("testGetByte Method: " + method + "- Object: " + target.getClass()
					+ ", Offset: " + offset(target, i) + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				checkSameAt(
						i,
						target,
						myUnsafe.getByteVolatile(base(target, i),
								offset(target, i)));
			} else {
				checkSameAt(i, target,
						myUnsafe.getByte(base(target, i), offset(target, i)));
			}
		}
	}

	protected void testGetChar(Object target, String method) throws Exception {
		init(target);
		for (int i = 0; i < modelChar.length; i++) {
			getLogger().debug("testGetChar Method: " + method + "- Object: " + target.getClass()
					+ ", Offset: " + offset(target, i) + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				checkSameAt(
						i,
						target,
						myUnsafe.getCharVolatile(base(target, i),
								offset(target, i)));
			} else {
				checkSameAt(i, target,
						myUnsafe.getChar(base(target, i), offset(target, i)));
			}
		}
	}

	protected void testGetShort(Object target, String method) throws Exception {
		init(target);
		for (int i = 0; i < modelShort.length; i++) {
			getLogger().debug("testGetShort Method: " + method + "- Object: " + target.getClass()
					+ ", Offset: " + offset(target, i) + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				checkSameAt(
						i,
						target,
						myUnsafe.getShortVolatile(base(target, i),
								offset(target, i)));
			} else {
				checkSameAt(i, target,
						myUnsafe.getShort(base(target, i), offset(target, i)));
			}
		}
	}

	protected void testGetInt(Object target, String method) throws Exception {
		init(target);
		for (int i = 0; i < modelInt.length; i++) {
			getLogger().debug("testGetInt Method: " + method + "- Object: " + target.getClass()
					+ ", Offset: " + offset(target, i) + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				checkSameAt(
						i,
						target,
						myUnsafe.getIntVolatile(base(target, i),
								offset(target, i)));
			} else {
				checkSameAt(i, target,
						myUnsafe.getInt(base(target, i), offset(target, i)));
			}
		}
	}

	protected void testGetLong(Object target, String method) throws Exception {
		init(target);
		for (int i = 0; i < modelLong.length; i++) {
			getLogger().debug("testGetLong Method: " + method + "- Object: " + target.getClass()
					+ ", Offset: " + offset(target, i) + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				checkSameAt(
						i,
						target,
						myUnsafe.getLongVolatile(base(target, i),
								offset(target, i)));
			} else {
				checkSameAt(i, target,
						myUnsafe.getLong(base(target, i), offset(target, i)));
			}
		}
	}

	protected void testGetFloat(Object target, String method) throws Exception {
		init(target);
		for (int i = 0; i < modelFloat.length; i++) {
			getLogger().debug("testGetFloat Method: " + method + "- Object: " + target.getClass()
					+ ", Offset: " + offset(target, i) + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				checkSameAt(
						i,
						target,
						myUnsafe.getFloatVolatile(base(target, i),
								offset(target, i)));
			} else {
				checkSameAt(i, target,
						myUnsafe.getFloat(base(target, i), offset(target, i)));
			}
		}
	}

	protected void testGetDouble(Object target, String method) throws Exception {
		init(target);
		for (int i = 0; i < modelDouble.length; i++) {
			getLogger().debug("testGetDouble Method: " + method + "- Object: " + target.getClass()
					+ ", Offset: " + offset(target, i) + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				checkSameAt(
						i,
						target,
						myUnsafe.getDoubleVolatile(base(target, i),
								offset(target, i)));
			} else {
				checkSameAt(i, target,
						myUnsafe.getDouble(base(target, i), offset(target, i)));
			}
		}
	}

	protected void testGetBoolean(Object target, String method)
			throws Exception {
		init(target);
		for (int i = 0; i < modelBoolean.length; i++) {
			getLogger().debug("testGetBoolean Method: " + method + "- Object: " + target.getClass()
					+ ", Offset: " + offset(target, i) + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				checkSameAt(
						i,
						target,
						myUnsafe.getBooleanVolatile(base(target, i),
								offset(target, i)));
			} else {
				checkSameAt(i, target,
						myUnsafe.getBoolean(base(target, i), offset(target, i)));
			}
		}
	}

	protected void testByteNative(String method) throws Exception {
		mem = memAllocate(100);
		long pointers[] = new long[modelByte.length];
		for (int i = 0; i < modelByte.length; i++) {
			pointers[i] = mem + i; // byte size: 1
			getLogger().debug("testByteNative Method: " + method + "- Pointer: " + pointers[i]
					+ ", Data: " + modelByte[i]);
			if (method.equals(VOLATILE)) {
				myUnsafe.putByteVolatile(null, pointers[i], modelByte[i]);
			} else if (method.equals(DEFAULT)) {
				myUnsafe.putByte(null, pointers[i], modelByte[i]);
			} else {
				myUnsafe.putByte(pointers[i], modelByte[i]);
			}
			byteCheck(modelByte, i, pointers);
		}
		short value = myUnsafe.getShort(null, pointers[0]);
		AssertJUnit.assertEquals((short)-1, value);		
	}

	protected void testCharNative(String method) throws Exception {
		mem = memAllocate(100);
		long pointers[] = new long[modelChar.length];
		for (int i = 0; i < modelChar.length; i++) {
			pointers[i] = mem + (i * 2); // char size: 2
			getLogger().debug("testCharNative Method: " + method + "- Pointer: " + pointers[i]
					+ ", Data: " + modelChar[i]);
			if (method.equals(VOLATILE)) {
				myUnsafe.putCharVolatile(null, pointers[i], modelChar[i]);
			} else if (method.equals(DEFAULT)) {
				myUnsafe.putChar(null, pointers[i], modelChar[i]);
			} else {
				myUnsafe.putChar(pointers[i], modelChar[i]);
			}
			charCheck(modelChar, i, pointers);
		}
	}

	protected void testShortNative(String method) throws Exception {
		mem = memAllocate(100);
		long pointers[] = new long[modelShort.length];
		for (int i = 0; i < modelShort.length; i++) {
			pointers[i] = mem + (i * 2); // char size: 2
			getLogger().debug("testShortNative Method: " + method + "- Pointer: " + pointers[i]
					+ ", Data: " + modelShort[i]);
			if (method.equals(VOLATILE)) {
				myUnsafe.putShortVolatile(null, pointers[i], modelShort[i]);
			} else if (method.equals(DEFAULT)) {
				myUnsafe.putShort(null, pointers[i], modelShort[i]);
			} else {
				myUnsafe.putShort(pointers[i], modelShort[i]);
			}
			shortCheck(modelShort, i, pointers);
		}
	}

	protected void testIntNative(String method) throws Exception {
		mem = memAllocate(100);
		long pointers[] = new long[modelInt.length];
		for (int i = 0; i < modelInt.length; i++) {
			pointers[i] = mem + (i * 4); // int size: 4
			getLogger().debug("testIntNative Method: " + method + "- Pointer: " + pointers[i]
					+ ", Data: " + modelInt[i]);
			if (method.equals(VOLATILE)) {
				myUnsafe.putIntVolatile(null, pointers[i], modelInt[i]);
			} else if (method.equals(DEFAULT)) {
				myUnsafe.putInt(null, pointers[i], modelInt[i]);
			// API is not supported in b181 java9, need to verify if test case is needed
//			} else if (method.equals(COMPAREANDSWAP)) {
//				myUnsafe.putInt(null, pointers[i], -1);
//				myUnsafe.compareAndSwapInt(null, pointers[i], -1, modelInt[i]);
			} else if (method.equals(ORDERED)) {
				myUnsafe.putIntRelease(null, pointers[i], modelInt[i]);
			} else if (method.equals(ADDRESS)) {
				myUnsafe.putAddress(pointers[i], modelInt[i]);
			} else {
				myUnsafe.putInt(pointers[i], modelInt[i]);
			}
			intCheck(modelInt, i, pointers, method);
		}
		long value = myUnsafe.getLong(null, pointers[0]);
		AssertJUnit.assertEquals((long)-1, value);	
	}

	protected void testLongNative(String method) throws Exception {
		mem = memAllocate(100);
		long pointers[] = new long[modelLong.length];
		for (int i = 0; i < modelLong.length; i++) {
			pointers[i] = mem + (i * 8); // long size: 8
			getLogger().debug("testLongNative Method: " + method + "- Pointer: " + pointers[i]
					+ ", Data: " + modelLong[i]);
			if (method.equals(VOLATILE)) {
				myUnsafe.putLongVolatile(null, pointers[i], modelLong[i]);
			} else if (method.equals(DEFAULT)) {
				myUnsafe.putLong(null, pointers[i], modelLong[i]);
			// API is not supported in b181 java9, need to verify if test case is needed
//			} else if (method.equals(COMPAREANDSWAP)) {
//				myUnsafe.putLong(null, pointers[i], -1);
//				myUnsafe.compareAndSwapLong(null, pointers[i], -1, modelLong[i]);
			} else if (method.equals(ORDERED)) {
				myUnsafe.putLongRelease(null, pointers[i], modelLong[i]);
			} else if (method.equals(ADDRESS)) {
				myUnsafe.putAddress(pointers[i], modelLong[i]);
			} else {
				myUnsafe.putLong(pointers[i], modelLong[i]);
			}
			longCheck(modelLong, i, pointers, method);
		}
	}

	protected void testFloatNative(String method) throws Exception {
		mem = memAllocate(100);
		long pointers[] = new long[modelFloat.length];
		for (int i = 0; i < modelFloat.length; i++) {
			pointers[i] = mem + (i * 4); // float size: 4
			getLogger().debug("testFloatNative Method: " + method + "- Pointer: " + pointers[i]
					+ ", Data: " + modelFloat[i]);
			if (method.equals(VOLATILE)) {
				myUnsafe.putFloatVolatile(null, pointers[i], modelFloat[i]);
			} else if (method.equals(DEFAULT)) {
				myUnsafe.putFloat(null, pointers[i], modelFloat[i]);
			} else {
				myUnsafe.putFloat(pointers[i], modelFloat[i]);
			}
			floatCheck(modelFloat, i, pointers);
		}
	}

	protected void testDoubleNative(String method) throws Exception {
		mem = memAllocate(100);
		long pointers[] = new long[modelDouble.length];
		for (int i = 0; i < modelDouble.length; i++) {
			pointers[i] = mem + (i * 8); // double size: 8
			getLogger().debug("testDoubleNative Method: " + method + "- Pointer: " + pointers[i]
					+ ", Data: " + modelDouble[i]);
			if (method.equals(VOLATILE)) {
				myUnsafe.putDoubleVolatile(null, pointers[i], modelDouble[i]);
			} else if (method.equals(DEFAULT)) {
				myUnsafe.putDouble(null, pointers[i], modelDouble[i]);
			} else {
				myUnsafe.putDouble(pointers[i], modelDouble[i]);
			}
			doubleCheck(modelDouble, i, pointers);
		}
	}

	protected void testBooleanNative(String method) throws Exception {
		mem = memAllocate(100);
		long pointers[] = new long[modelBoolean.length];
		for (int i = 0; i < modelBoolean.length; i++) {
			pointers[i] = mem + i; // boolean size: 1 bit
			getLogger().debug("testBooleanNative Method: " + method + "- Pointer: " + pointers[i]
					+ ", Data: " + modelBoolean[i]);
			if (method.equals(VOLATILE)) {
				myUnsafe.putBooleanVolatile(null, pointers[i], modelBoolean[i]);
			} else if (method.equals(DEFAULT)) {
				myUnsafe.putBoolean(null, pointers[i], modelBoolean[i]);
			}
			booleanCheck(modelBoolean, i, pointers);
		}
	}

	private void byteCheck(Object model, int limit, long[] pointers) {
		for (int i = 0; i <= limit; i++) {
			getLogger().debug(" pointers[" + i +"]: " + pointers[i]);
			byte expected = (Byte) Array.get(model, i);
			byte value = myUnsafe.getByte(null, pointers[i]);
			getLogger().debug("getByte(null, long) - Expected: "
					+ expected + ", Actual: " + value);
			AssertJUnit.assertEquals(expected, value);
			value = myUnsafe.getByte(pointers[i]);
			getLogger().debug("getByte(long) - Expected: " + expected
					+ ", Actual: " + value);
			AssertJUnit.assertEquals(expected, value);
		}
	}

	private void charCheck(Object model, int limit, long[] pointers) {
		for (int i = 0; i <= limit; i++) {
			getLogger().debug(" pointers[" + i +"]: " + pointers[i]);
			char expected = (Character) Array.get(model, i);
			char value = myUnsafe.getChar(null, pointers[i]);
			getLogger().debug("getChar(Object, long) - Expected: "
					+ expected + ", Actual: " + value);
			AssertJUnit.assertEquals(expected, value);
			value = myUnsafe.getChar(pointers[i]);
			getLogger().debug("getChar(long) - Expected: " + expected
					+ ", Actual: " + value);
			AssertJUnit.assertEquals(expected, value);
			value = (char) myUnsafe.getShort(null, pointers[i]);
			getLogger().debug("getShort(Object, long) - Expected: "
					+ expected + ", Actual: " + value);
			AssertJUnit.assertEquals(expected, value);
		}
	}

	private void shortCheck(Object model, int limit, long[] pointers) {
		for (int i = 0; i <= limit; i++) {
			getLogger().debug(" pointers[" + i +"]: " + pointers[i]);
			short expected = (Short) Array.get(model, i);
			short value = myUnsafe.getShort(null, pointers[i]);
			getLogger().debug("getShort(Object, long)- Expected: "
					+ expected + ", Actual: " + value);
			AssertJUnit.assertEquals(expected, value);
			value = myUnsafe.getShort(pointers[i]);
			getLogger().debug("getShort(long)-  Expected: " + expected
					+ ", Actual: " + value);
			AssertJUnit.assertEquals(expected, value);
		}
	}

	private void intCheck(Object model, int limit, long[] pointers, String method) {
		for (int i = 0; i <= limit; i++) {
			getLogger().debug(" pointers[" + i +"]: " + pointers[i]);
			int expected = (Integer) Array.get(model, i);
			int value = myUnsafe.getInt(null, pointers[i]);
			getLogger().debug("getInt(Object, long) - Expected: "
					+ expected + ", Actual: " + value);
			AssertJUnit.assertEquals(expected, value);
			value = myUnsafe.getInt(pointers[i]);
			getLogger().debug("getInt(long) - Expected: " + expected
					+ ", Actual: " + value);
			AssertJUnit.assertEquals(expected, value);
			if(method.equals(ADDRESS)){
				value = (int) myUnsafe.getAddress(pointers[i]);
				getLogger().debug("getAddress(long) - Expected: " + expected
						+ ", Actual: " + value);
				AssertJUnit.assertEquals(expected, value);
			}
		}
	}

	private void longCheck(Object model, int limit, long[] pointers, String method) {
		for (int i = 0; i <= limit; i++) {
			getLogger().debug(" pointers[" + i +"]: " + pointers[i]);
			long expected = (Long) Array.get(model, i);
			long value = myUnsafe.getLong(null, pointers[i]);
			getLogger().debug("getLong(Object, long) - Expected: "
					+ expected + ", Actual: " + value);
			AssertJUnit.assertEquals(expected, value);
			value = myUnsafe.getLong(pointers[i]);
			getLogger().debug("getLong(long) - Expected: " + expected
					+ ", Actual: " + value);
			AssertJUnit.assertEquals(expected, value);
			if(method.equals(ADDRESS)){
				value = myUnsafe.getAddress(pointers[i]);
				getLogger().debug("getAddress(long) - Expected: " + expected
						+ ", Actual: " + value);
				AssertJUnit.assertEquals(expected, value);
			}
		}
	}

	private void floatCheck(Object model, int limit, long[] pointers) {
		for (int i = 0; i <= limit; i++) {
			getLogger().debug(" pointers[" + i +"]: " + pointers[i]);
			float expected = (Float) Array.get(model, i);
			float value = myUnsafe.getFloat(null, pointers[i]);
			getLogger().debug("getFloat(Object, long) - Expected: "
					+ expected + ", Actual: " + value);
			AssertJUnit.assertEquals(expected, value);
			value = myUnsafe.getFloat(pointers[i]);
			getLogger().debug("getFloat(long) - Expected: "
					+ expected + ", Actual: " + value);
			AssertJUnit.assertEquals(expected, value);
		}
	}

	private void doubleCheck(Object model, int limit, long[] pointers) {
		for (int i = 0; i <= limit; i++) {
			getLogger().debug(" pointers[" + i +"]: " + pointers[i]);
			double expected = (Double) Array.get(model, i);
			double value = myUnsafe.getDouble(null, pointers[i]);
			getLogger().debug("getDouble(Object, long) - Expected: "
					+ expected + ", Actual: " + value);
			AssertJUnit.assertEquals(expected, value);
			value = myUnsafe.getDouble(pointers[i]);
			getLogger().debug("getDouble(Object, long) - Expected: "
					+ expected + ", Actual: " + value);
			AssertJUnit.assertEquals(expected, value);
		}
	}

	private void booleanCheck(Object model, int limit, long[] pointers) {
		for (int i = 0; i <= limit; i++) {
			getLogger().debug(" pointers[" + i +"]: " + pointers[i]);
			boolean expected = (Boolean) Array.get(model, i);
			boolean value = myUnsafe.getBoolean(null, pointers[i]);
			getLogger().debug("getBoolean - Expected: " + expected
					+ ", Actual: " + value);
			AssertJUnit.assertEquals(expected, value);
		}
	}

	protected long memAllocate(int size) {
		long address = myUnsafe.allocateMemory(size);
		getLogger().debug("allocateMemory: " + mem);
		return address;
	}
	
	protected void freeMemory() {		
		myUnsafe.freeMemory(mem);
		getLogger().debug("freeMemory: " + mem);
	}
	
	protected void alignment(){
		int addressSize = myUnsafe.addressSize();
		getLogger().debug("addressSize: " + addressSize);
		long mod = 0;
		if(addressSize > 4){
			mod = mem % 8;
			getLogger().debug("memory mod 8: " +  mod);
		}else{
			mod = mem % 4;
			getLogger().debug("memory mod 4: " +  mod);
		}
		if(mod != 0){
			mem = mem + mod;
			getLogger().debug("Change pointer to: " + mem);
		}
	}
}
