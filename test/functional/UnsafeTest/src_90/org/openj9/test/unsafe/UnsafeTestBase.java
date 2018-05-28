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

import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

import org.testng.AssertJUnit;
import org.testng.ITest;
import org.testng.annotations.AfterMethod;
import org.testng.annotations.BeforeMethod;
import org.testng.log4testng.Logger;

import jdk.internal.org.objectweb.asm.ClassWriter;
import jdk.internal.org.objectweb.asm.MethodVisitor;

import static jdk.internal.org.objectweb.asm.Opcodes.ACC_PUBLIC;
import static jdk.internal.org.objectweb.asm.Opcodes.RETURN;

import jdk.internal.misc.Unsafe;

public class UnsafeTestBase implements ITest {
	private static Logger logger = Logger.getLogger(UnsafeTestBase.class);
	protected static jdk.internal.misc.Unsafe myUnsafe;

	protected final String VOLATILE = "Volatile";
	protected final String OPAQUE = "Opaque";
	protected final String ORDERED = "Ordered";
	protected final String UNALIGNED = "Unaligned";
	protected final String DEFAULT = "put";

	/* compareAndSet methods */
	protected final String COMPAREANDSET = "CompareAndSet";
	protected final String WCOMPAREANDSET = "weakCompareAndSet";
	protected final String WCOMPAREANDSETP = "weakCompareAndSetPlain";
	protected final String WCOMPAREANDSETA = "weakCompareAndSetAcquire";
	protected final String WCOMPAREANDSETR = "weakCompareAndSetRelease";

	/* compareAndExchange methods */
	protected final String COMPAREANDEXCH = "CompareAndExchange";
	protected final String COMPAREANDEXCHA = "CompareAndExchangeAcquire";
	protected final String COMPAREANDEXCHR = "CompareAndExchangeRelease";

	/* getAndOp methods */
	protected final String GETANDSET = "GetAndSet";
	protected final String GETANDSETA = "GetAndSetAcquire";
	protected final String GETANDSETR = "GetAndSetRelease";
	protected final String GETANDADD = "GetAndAdd";
	protected final String GETANDADDA = "GetAndAddAcquire";
	protected final String GETANDADDR = "GetAndAddRequire";
	protected final String GETANDBITWISEOR = "GetAndBitwiseOr";
	protected final String GETANDBITWISEORA = "GetAndBitwiseOrAcquire";
	protected final String GETANDBITWISEORR = "GetAndBitwiseOrRelease";
	protected final String GETANDBITWISEAND = "GetAndBitwiseAnd";
	protected final String GETANDBITWISEANDA = "GetAndBitwiseAndAcquire";
	protected final String GETANDBITWISEANDR = "GetAndBitwiseAndRelease";
	protected final String GETANDBITWISEXOR = "GetAndBitwiseXor";
	protected final String GETANDBITWISEXORA = "GetAndBitwiseXorAcquire";
	protected final String GETANDBITWISEXORR = "GetAndBitwiseXorRelease";

	protected final static String ADDRESS = "address";

	private static final int BYTE_MASK = 0xFF;
	private static final long BYTE_MASKL = (long) BYTE_MASK;

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
	protected static final Object[] models = new Object[] { modelByte, modelChar, modelShort, modelInt, modelLong,
			modelFloat, modelDouble, modelBoolean };

	/* compare value for compareAndXXXX tests */
	private static final byte compareValueByte = -5;
	private static final char compareValueChar = (char) -5;
	private static final short compareValueShort = -5;
	private static final int compareValueInt = -5;
	private static final long compareValueLong = -5;
	private static final float compareValueFloat = -5;
	private static final double compareValueDouble = -5;
	private static final Object compareValueObject = new Object();

	/* bitwise check for getAndBitwiseXXX */
	private static final byte bitwiseValueByte = (byte) 0xF;
	private static final char bitwiseValueChar = (char) 0xF;
	private static final short bitwiseValueShort = (short) 0xF;
	private static final int bitwiseValueInt = 0xF;
	private static final long bitwiseValueLong = 0xFL;

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

	static class ObjectData extends Data {
		Object f0, f1, f2, f3, f4, f5, f6, f7;
		static Object sf0, sf1, sf2, sf3, sf4, sf5, sf6, sf7;

		void init() {
			f0 = sf0 = models[0];
			f1 = sf1 = models[1];
			f2 = sf2 = models[2];
			f3 = sf3 = models[3];
			f4 = sf4 = models[4];
			f5 = sf5 = models[5];
			f6 = sf6 = models[6];
			f7 = sf7 = models[7];
		}

		Object get(int i) {
			switch(i) {
			case 0:
				return f0;
			case 1:
				return f1;
			case 2:
				return f2;
			case 3:
				return f3;
			case 4:
				return f4;
			case 5:
				return f5;
			case 6:
				return f6;
			default:
				return f7;
			}
		}

		Object getStatic(int i) {
			switch(i) {
			case 0:
				return sf0;
			case 1:
				return sf1;
			case 2:
				return sf2;
			case 3:
				return sf3;
			case 4:
				return sf4;
			case 5:
				return sf5;
			case 6:
				return sf6;
			default:
				return sf7;
			}
		}

		/* return size in bytes of primitive array being accessed */
		static int getSize(int i) {
			int size;
			Object obj = models[i];

			if ((obj instanceof byte[]) || (obj instanceof boolean[])) {
				size = 1 * ((byte[]) obj).length;
			} else if ((obj instanceof char[]) || (obj instanceof short[])) {
				size = 2 * ((char[]) obj).length;
			} else if ((obj instanceof int[]) || (obj instanceof float[])) {
				size = 4 * ((int[]) obj).length;
			} else {
				size = 8 * ((long[]) obj).length;
			}

			return size;
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

	/**
	 * Intermediate model check for specified unaligned offset
	 */
	protected void modelCheckUnalignedChar(Object model, Object object, int i, long offset) throws Exception {
		char myChar = generateUnalignedChar(object, offset);
		getLogger().debug("Unaligned test: Index: " + i + " Expected: " + Array.get(model, i) + " Actual: " + myChar);
		AssertJUnit.assertEquals(Array.get(model, i), myChar);
	}

	protected void modelCheckUnalignedShort(Object model, Object object, int i, long offset) throws Exception {
		short myShort = generateUnalignedShort(object, offset);
		getLogger().debug("Unaligned test: Index: " + i + " Expected: " + Array.get(model, i) + " Actual: " + myShort);
		AssertJUnit.assertEquals(Array.get(model, i), myShort);
	}

	protected void modelCheckUnalignedInt(Object model, Object object, int i, long offset) throws Exception {
		int myInt = generateUnalignedInt(object, offset);
		getLogger().debug("Unaligned test: Index: " + i + " Expected: " + Array.get(model, i) + " Actual: " + myInt);
		AssertJUnit.assertEquals(Array.get(model, i), myInt);
	}

	protected void modelCheckUnalignedLong(Object model, Object object, int i, long offset) throws Exception {
		long myLong = generateUnalignedLong(object, offset);
		getLogger().debug("Unaligned test: Index: " + i + " Expected: " + Array.get(model, i) + " Actual: " + myLong);
		AssertJUnit.assertEquals(Array.get(model, i), myLong);
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

	protected void checkSameAt(Object target, Object value) throws InstantiationException, IllegalAccessException {
		getLogger().debug("Expected: " + target + " Actual: " + value);
		AssertJUnit.assertEquals(target, value);
	}

	protected void checkTrueAt(boolean value) throws InstantiationException, IllegalAccessException {
		getLogger().debug("true boolean check: " + value);
		AssertJUnit.assertTrue(value);
	}

	@BeforeMethod(groups = { "level.sanity" })
	protected void setUp() throws Exception {
		myUnsafe = getUnsafeInstance();
	}

	
	@AfterMethod(groups = { "level.sanity" })
	protected void tearDown() throws Exception {
		logger.debug("Test finished: " + mTestName);
		if (mem != 0) {
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

			} else if (method.equals(OPAQUE)) {
				myUnsafe.putByteOpaque(base(target, i), offset, modelByte[i]);

			} else if (method.equals(ORDERED)) {
				myUnsafe.putByteRelease(base(target, i), offset, modelByte[i]);

			} else if (method.equals(COMPAREANDSET)) {
				myUnsafe.putByte(base(target, i), offset, compareValueByte);
				checkTrueAt(myUnsafe.compareAndSetByte(base(target, i), offset, compareValueByte, modelByte[i]));

			} else if (method.equals(WCOMPAREANDSET)) {
				myUnsafe.putByte(base(target, i), offset, compareValueByte);
				checkTrueAt(myUnsafe.weakCompareAndSetByte(base(target, i), offset, compareValueByte, modelByte[i]));

			} else if (method.equals(WCOMPAREANDSETP)) {
				myUnsafe.putByte(base(target, i), offset, compareValueByte);
				checkTrueAt(
						myUnsafe.weakCompareAndSetBytePlain(base(target, i), offset, compareValueByte, modelByte[i]));

			} else if (method.equals(WCOMPAREANDSETA)) {
				myUnsafe.putByte(base(target, i), offset, compareValueByte);
				checkTrueAt(
						myUnsafe.weakCompareAndSetByteAcquire(base(target, i), offset, compareValueByte, modelByte[i]));

			} else if (method.equals(WCOMPAREANDSETR)) {
				myUnsafe.putByte(base(target, i), offset, compareValueByte);
				checkTrueAt(
						myUnsafe.weakCompareAndSetByteRelease(base(target, i), offset, compareValueByte, modelByte[i]));

			} else if (method.equals(COMPAREANDEXCH)) {
				myUnsafe.putByte(base(target, i), offset, compareValueByte);
				checkSameAt(compareValueByte,
						myUnsafe.compareAndExchangeByte(base(target, i), offset, compareValueByte, modelByte[i]));

			} else if (method.equals(COMPAREANDEXCHA)) {
				myUnsafe.putByte(base(target, i), offset, compareValueByte);
				checkSameAt(compareValueByte, myUnsafe.compareAndExchangeByteAcquire(base(target, i), offset,
						compareValueByte, modelByte[i]));

			} else if (method.equals(COMPAREANDEXCHR)) {
				myUnsafe.putByte(base(target, i), offset, compareValueByte);
				checkSameAt(compareValueByte, myUnsafe.compareAndExchangeByteRelease(base(target, i), offset,
						compareValueByte, modelByte[i]));

			} else { /* default */
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

			} else if (method.equals(OPAQUE)) {
				myUnsafe.putCharOpaque(base(target, i), offset, modelChar[i]);

			} else if (method.equals(ORDERED)) {
				myUnsafe.putCharRelease(base(target, i), offset, modelChar[i]);

			} else if (method.equals(UNALIGNED)) {
				int sizeOfChar = 2; // bytes

				for (long uOffset = (offset + sizeOfChar); uOffset >= offset; --uOffset) {
					/* putUnaligned */
					myUnsafe.putCharUnaligned(base(target, i), uOffset, modelChar[i]);
					modelCheckUnalignedChar(modelChar, target, i, uOffset);

					getLogger().debug("putUnaligned() endianness test");

					/* putUnaligned - endianness */
					myUnsafe.putCharUnaligned(base(target, i), uOffset, modelChar[i], myUnsafe.isBigEndian());
					modelCheckUnalignedChar(modelChar, target, i, uOffset);
				}

			} else if (method.equals(COMPAREANDSET)) {
				myUnsafe.putChar(base(target, i), offset, compareValueChar);
				checkTrueAt(myUnsafe.compareAndSetChar(base(target, i), offset, compareValueChar, modelChar[i]));

			} else if (method.equals(WCOMPAREANDSET)) {
				myUnsafe.putChar(base(target, i), offset, compareValueChar);
				checkTrueAt(myUnsafe.weakCompareAndSetChar(base(target, i), offset, compareValueChar, modelChar[i]));

			} else if (method.equals(WCOMPAREANDSETP)) {
				myUnsafe.putChar(base(target, i), offset, compareValueChar);
				checkTrueAt(
						myUnsafe.weakCompareAndSetCharPlain(base(target, i), offset, compareValueChar, modelChar[i]));

			} else if (method.equals(WCOMPAREANDSETA)) {
				myUnsafe.putChar(base(target, i), offset, compareValueChar);
				checkTrueAt(
						myUnsafe.weakCompareAndSetCharAcquire(base(target, i), offset, compareValueChar, modelChar[i]));

			} else if (method.equals(WCOMPAREANDSETR)) {
				myUnsafe.putChar(base(target, i), offset, compareValueChar);
				checkTrueAt(
						myUnsafe.weakCompareAndSetCharRelease(base(target, i), offset, compareValueChar, modelChar[i]));

			} else if (method.equals(COMPAREANDEXCH)) {
				myUnsafe.putChar(base(target, i), offset, compareValueChar);
				checkSameAt(compareValueChar,
						myUnsafe.compareAndExchangeChar(base(target, i), offset, compareValueChar, modelChar[i]));

			} else if (method.equals(COMPAREANDEXCHA)) {
				myUnsafe.putChar(base(target, i), offset, compareValueChar);
				checkSameAt(compareValueChar, myUnsafe.compareAndExchangeCharAcquire(base(target, i), offset,
						compareValueChar, modelChar[i]));

			} else if (method.equals(COMPAREANDEXCHR)) {
				myUnsafe.putChar(base(target, i), offset, compareValueChar);
				checkSameAt(compareValueChar, myUnsafe.compareAndExchangeCharRelease(base(target, i), offset,
						compareValueChar, modelChar[i]));

			} else { /* default */
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
				myUnsafe.putShortVolatile(base(target, i), offset, modelShort[i]);

			} else if (method.equals(OPAQUE)) {
				myUnsafe.putShortOpaque(base(target, i), offset, modelShort[i]);

			} else if (method.equals(ORDERED)) {
				myUnsafe.putShortRelease(base(target, i), offset, modelShort[i]);

			} else if (method.equals(UNALIGNED)) {
				int sizeOfShort = 2; // bytes
				for (long uOffset = (offset + sizeOfShort); uOffset >= offset; --uOffset) {
					/* putUnaligned */
					myUnsafe.putShortUnaligned(base(target, i), uOffset, modelShort[i]);

					modelCheckUnalignedShort(modelShort, target, i, uOffset);

					getLogger().debug("putUnaligned() endianness test");

					/* putUnaligned - endianness */
					myUnsafe.putShortUnaligned(base(target, i), uOffset, modelShort[i], myUnsafe.isBigEndian());
					modelCheckUnalignedShort(modelShort, target, i, uOffset);
				}

			} else if (method.equals(COMPAREANDSET)) {
				myUnsafe.putShort(base(target, i), offset, compareValueShort);
				checkTrueAt(myUnsafe.compareAndSetShort(base(target, i), offset, compareValueShort, modelShort[i]));

			} else if (method.equals(WCOMPAREANDSET)) {
				myUnsafe.putShort(base(target, i), offset, compareValueShort);
				checkTrueAt(myUnsafe.weakCompareAndSetShort(base(target, i), offset, compareValueShort, modelShort[i]));

			} else if (method.equals(WCOMPAREANDSETP)) {
				myUnsafe.putShort(base(target, i), offset, compareValueShort);
				checkTrueAt(myUnsafe.weakCompareAndSetShortPlain(base(target, i), offset, compareValueShort,
						modelShort[i]));

			} else if (method.equals(WCOMPAREANDSETA)) {
				myUnsafe.putShort(base(target, i), offset, compareValueShort);
				checkTrueAt(myUnsafe.weakCompareAndSetShortAcquire(base(target, i), offset, compareValueShort,
						modelShort[i]));

			} else if (method.equals(WCOMPAREANDSETR)) {
				myUnsafe.putShort(base(target, i), offset, compareValueShort);
				checkTrueAt(myUnsafe.weakCompareAndSetShortRelease(base(target, i), offset, compareValueShort,
						modelShort[i]));

			} else if (method.equals(COMPAREANDEXCH)) {
				myUnsafe.putShort(base(target, i), offset, compareValueShort);
				checkSameAt(compareValueShort,
						myUnsafe.compareAndExchangeShort(base(target, i), offset, compareValueShort, modelShort[i]));

			} else if (method.equals(COMPAREANDEXCHA)) {
				myUnsafe.putShort(base(target, i), offset, compareValueShort);
				checkSameAt(compareValueShort, myUnsafe.compareAndExchangeShortAcquire(base(target, i), offset,
						compareValueShort, modelShort[i]));

			} else if (method.equals(COMPAREANDEXCHR)) {
				myUnsafe.putShort(base(target, i), offset, compareValueShort);
				checkSameAt(compareValueShort, myUnsafe.compareAndExchangeShortRelease(base(target, i), offset,
						compareValueShort, modelShort[i]));

			} else { /* default */
				myUnsafe.putShort(base(target, i), offset, modelShort[i]);
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

			} else if (method.equals(OPAQUE)) {
				myUnsafe.putIntOpaque(base(target, i), offset, modelInt[i]);

			} else if (method.equals(ORDERED)) {
				myUnsafe.putIntRelease(base(target, i), offset, modelInt[i]);

			} else if (method.equals(UNALIGNED)) {
				int sizeOfInt = 4; // bytes
				for (long uOffset = (offset + sizeOfInt); uOffset >= offset; --uOffset) {
					/* putUnaligned */
					myUnsafe.putIntUnaligned(base(target, i), uOffset, modelInt[i]);
					modelCheckUnalignedInt(modelInt, target, i, uOffset);

					getLogger().debug("putUnaligned() endianness test");

					/* putUnaligned - endianness */
					myUnsafe.putIntUnaligned(base(target, i), uOffset, modelInt[i], myUnsafe.isBigEndian());
					modelCheckUnalignedInt(modelInt, target, i, uOffset);
				}

			} else if (method.equals(COMPAREANDSET)) {
				myUnsafe.putInt(base(target, i), offset, compareValueInt);
				checkTrueAt(myUnsafe.compareAndSetInt(base(target, i), offset, compareValueInt, modelInt[i]));

			} else if (method.equals(WCOMPAREANDSET)) {
				myUnsafe.putInt(base(target, i), offset, compareValueInt);
				checkTrueAt(myUnsafe.weakCompareAndSetInt(base(target, i), offset, compareValueInt, modelInt[i]));

			} else if (method.equals(WCOMPAREANDSETP)) {
				myUnsafe.putInt(base(target, i), offset, compareValueInt);
				checkTrueAt(myUnsafe.weakCompareAndSetIntPlain(base(target, i), offset, compareValueInt, modelInt[i]));

			} else if (method.equals(WCOMPAREANDSETA)) {
				myUnsafe.putInt(base(target, i), offset, compareValueInt);
				checkTrueAt(
						myUnsafe.weakCompareAndSetIntAcquire(base(target, i), offset, compareValueInt, modelInt[i]));

			} else if (method.equals(WCOMPAREANDSETR)) {
				myUnsafe.putInt(base(target, i), offset, compareValueInt);
				checkTrueAt(
						myUnsafe.weakCompareAndSetIntRelease(base(target, i), offset, compareValueInt, modelInt[i]));

			} else if (method.equals(COMPAREANDEXCH)) {
				myUnsafe.putInt(base(target, i), offset, compareValueInt);
				checkSameAt(compareValueInt,
						myUnsafe.compareAndExchangeInt(base(target, i), offset, compareValueInt, modelInt[i]));

			} else if (method.equals(COMPAREANDEXCHA)) {
				myUnsafe.putInt(base(target, i), offset, compareValueInt);
				checkSameAt(compareValueInt,
						myUnsafe.compareAndExchangeIntAcquire(base(target, i), offset, compareValueInt, modelInt[i]));

			} else if (method.equals(COMPAREANDEXCHR)) {
				myUnsafe.putInt(base(target, i), offset, compareValueInt);
				checkSameAt(compareValueInt,
						myUnsafe.compareAndExchangeIntRelease(base(target, i), offset, compareValueInt, modelInt[i]));

			} else { /* default */
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

			} else if (method.equals(OPAQUE)) {
				myUnsafe.putLongOpaque(base(target, i), offset, modelLong[i]);

			} else if (method.equals(ORDERED)) {
				myUnsafe.putLongRelease(base(target, i), offset, modelLong[i]);

			} else if (method.equals(UNALIGNED)) {
				int sizeOfLong = 8; // bytes
				for (long uOffset = (offset + sizeOfLong); uOffset >= offset; --uOffset) {
					/* putUnaligned */
					myUnsafe.putLongUnaligned(base(target, i), uOffset, modelLong[i]);
					modelCheckUnalignedLong(modelLong, target, i, uOffset);

					getLogger().debug("putUnaligned() endianness test");

					/* putUnaligned - endianness */
					myUnsafe.putLongUnaligned(base(target, i), uOffset, modelLong[i], myUnsafe.isBigEndian());
					modelCheckUnalignedLong(modelLong, target, i, uOffset);
				}

			} else if (method.equals(COMPAREANDSET)) {
				myUnsafe.putLong(base(target, i), offset, compareValueLong);
				checkTrueAt(myUnsafe.compareAndSetLong(base(target, i), offset, compareValueLong, modelLong[i]));

			} else if (method.equals(WCOMPAREANDSET)) {
				myUnsafe.putLong(base(target, i), offset, compareValueLong);
				checkTrueAt(myUnsafe.weakCompareAndSetLong(base(target, i), offset, compareValueLong, modelLong[i]));

			} else if (method.equals(WCOMPAREANDSETP)) {
				myUnsafe.putLong(base(target, i), offset, compareValueLong);
				checkTrueAt(
						myUnsafe.weakCompareAndSetLongPlain(base(target, i), offset, compareValueLong, modelLong[i]));

			} else if (method.equals(WCOMPAREANDSETA)) {
				myUnsafe.putLong(base(target, i), offset, compareValueLong);
				checkTrueAt(
						myUnsafe.weakCompareAndSetLongAcquire(base(target, i), offset, compareValueLong, modelLong[i]));

			} else if (method.equals(WCOMPAREANDSETR)) {
				myUnsafe.putLong(base(target, i), offset, compareValueLong);
				checkTrueAt(
						myUnsafe.weakCompareAndSetLongRelease(base(target, i), offset, compareValueLong, modelLong[i]));

			} else if (method.equals(COMPAREANDEXCH)) {
				myUnsafe.putLong(base(target, i), offset, compareValueLong);
				checkSameAt(compareValueLong,
						myUnsafe.compareAndExchangeLong(base(target, i), offset, compareValueLong, modelLong[i]));

			} else if (method.equals(COMPAREANDEXCHA)) {
				myUnsafe.putLong(base(target, i), offset, compareValueLong);
				checkSameAt(compareValueLong, myUnsafe.compareAndExchangeLongAcquire(base(target, i), offset,
						compareValueLong, modelLong[i]));

			} else if (method.equals(COMPAREANDEXCHR)) {
				myUnsafe.putLong(base(target, i), offset, compareValueLong);
				checkSameAt(compareValueLong, myUnsafe.compareAndExchangeLongRelease(base(target, i), offset,
						compareValueLong, modelLong[i]));

			} else { /* default */
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
				myUnsafe.putFloatVolatile(base(target, i), offset, modelFloat[i]);

			} else if (method.equals(OPAQUE)) {
				myUnsafe.putFloatOpaque(base(target, i), offset, modelFloat[i]);

			} else if (method.equals(ORDERED)) {
				myUnsafe.putFloatRelease(base(target, i), offset, modelFloat[i]);

			} else if (method.equals(COMPAREANDSET)) {
				myUnsafe.putFloat(base(target, i), offset, compareValueFloat);
				checkTrueAt(myUnsafe.compareAndSetFloat(base(target, i), offset, compareValueFloat, modelFloat[i]));

			} else if (method.equals(WCOMPAREANDSET)) {
				myUnsafe.putFloat(base(target, i), offset, compareValueFloat);
				checkTrueAt(myUnsafe.weakCompareAndSetFloat(base(target, i), offset, compareValueFloat, modelFloat[i]));

			} else if (method.equals(WCOMPAREANDSETP)) {
				myUnsafe.putFloat(base(target, i), offset, compareValueFloat);
				checkTrueAt(myUnsafe.weakCompareAndSetFloatPlain(base(target, i), offset, compareValueFloat,
						modelFloat[i]));

			} else if (method.equals(WCOMPAREANDSETA)) {
				myUnsafe.putFloat(base(target, i), offset, compareValueFloat);
				checkTrueAt(myUnsafe.weakCompareAndSetFloatAcquire(base(target, i), offset, compareValueFloat,
						modelFloat[i]));

			} else if (method.equals(WCOMPAREANDSETR)) {
				myUnsafe.putFloat(base(target, i), offset, compareValueFloat);
				checkTrueAt(myUnsafe.weakCompareAndSetFloatRelease(base(target, i), offset, compareValueFloat,
						modelFloat[i]));

			} else if (method.equals(COMPAREANDEXCH)) {
				myUnsafe.putFloat(base(target, i), offset, compareValueFloat);
				checkSameAt(compareValueFloat,
						myUnsafe.compareAndExchangeFloat(base(target, i), offset, compareValueFloat, modelFloat[i]));

			} else if (method.equals(COMPAREANDEXCHA)) {
				myUnsafe.putFloat(base(target, i), offset, compareValueFloat);
				checkSameAt(compareValueFloat, myUnsafe.compareAndExchangeFloatAcquire(base(target, i), offset,
						compareValueFloat, modelFloat[i]));

			} else if (method.equals(COMPAREANDEXCHR)) {
				myUnsafe.putFloat(base(target, i), offset, compareValueFloat);
				checkSameAt(compareValueFloat, myUnsafe.compareAndExchangeFloatRelease(base(target, i), offset,
						compareValueFloat, modelFloat[i]));

			} else { /* default */
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
				myUnsafe.putDoubleVolatile(base(target, i), offset, modelDouble[i]);

			} else if (method.equals(OPAQUE)) {
				myUnsafe.putDoubleOpaque(base(target, i), offset, modelDouble[i]);

			} else if (method.equals(ORDERED)) {
				myUnsafe.putDoubleRelease(base(target, i), offset, modelDouble[i]);

			} else if (method.equals(COMPAREANDSET)) {
				myUnsafe.putDouble(base(target, i), offset, compareValueDouble);
				checkTrueAt(myUnsafe.compareAndSetDouble(base(target, i), offset, compareValueDouble, modelDouble[i]));

			} else if (method.equals(WCOMPAREANDSET)) {
				myUnsafe.putDouble(base(target, i), offset, compareValueDouble);
				checkTrueAt(
						myUnsafe.weakCompareAndSetDouble(base(target, i), offset, compareValueDouble, modelDouble[i]));

			} else if (method.equals(WCOMPAREANDSETP)) {
				myUnsafe.putDouble(base(target, i), offset, compareValueDouble);
				checkTrueAt(myUnsafe.weakCompareAndSetDoublePlain(base(target, i), offset, compareValueDouble,
						modelDouble[i]));

			} else if (method.equals(WCOMPAREANDSETA)) {
				myUnsafe.putDouble(base(target, i), offset, compareValueDouble);
				checkTrueAt(myUnsafe.weakCompareAndSetDoubleAcquire(base(target, i), offset, compareValueDouble,
						modelDouble[i]));

			} else if (method.equals(WCOMPAREANDSETR)) {
				myUnsafe.putDouble(base(target, i), offset, compareValueDouble);
				checkTrueAt(myUnsafe.weakCompareAndSetDoubleRelease(base(target, i), offset, compareValueDouble,
						modelDouble[i]));

			} else if (method.equals(COMPAREANDEXCH)) {
				myUnsafe.putDouble(base(target, i), offset, compareValueDouble);
				checkSameAt(compareValueDouble,
						myUnsafe.compareAndExchangeDouble(base(target, i), offset, compareValueDouble, modelDouble[i]));

			} else if (method.equals(COMPAREANDEXCHA)) {
				myUnsafe.putDouble(base(target, i), offset, compareValueDouble);
				checkSameAt(compareValueDouble, myUnsafe.compareAndExchangeDoubleAcquire(base(target, i), offset,
						compareValueDouble, modelDouble[i]));

			} else if (method.equals(COMPAREANDEXCHR)) {
				myUnsafe.putDouble(base(target, i), offset, compareValueDouble);
				checkSameAt(compareValueDouble, myUnsafe.compareAndExchangeDoubleRelease(base(target, i), offset,
						compareValueDouble, modelDouble[i]));

			} else { /* default */
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
				myUnsafe.putBooleanVolatile(base(target, i), offset, modelBoolean[i]);

			} else if (method.equals(OPAQUE)) {
				myUnsafe.putBooleanOpaque(base(target, i), offset, modelBoolean[i]);

			} else if (method.equals(ORDERED)) {
				myUnsafe.putBooleanRelease(base(target, i), offset, modelBoolean[i]);

			} else if (method.equals(COMPAREANDSET)) {
				myUnsafe.putBoolean(base(target, i), offset, !modelBoolean[i]);
				checkTrueAt(myUnsafe.compareAndSetBoolean(base(target, i), offset, !modelBoolean[i], modelBoolean[i]));

			} else if (method.equals(WCOMPAREANDSET)) {
				myUnsafe.putBoolean(base(target, i), offset, !modelBoolean[i]);
				checkTrueAt(
						myUnsafe.weakCompareAndSetBoolean(base(target, i), offset, !modelBoolean[i], modelBoolean[i]));

			} else if (method.equals(WCOMPAREANDSETP)) {
				myUnsafe.putBoolean(base(target, i), offset, !modelBoolean[i]);
				checkTrueAt(myUnsafe.weakCompareAndSetBooleanPlain(base(target, i), offset, !modelBoolean[i],
						modelBoolean[i]));

			} else if (method.equals(WCOMPAREANDSETA)) {
				myUnsafe.putBoolean(base(target, i), offset, !modelBoolean[i]);
				checkTrueAt(myUnsafe.weakCompareAndSetBooleanAcquire(base(target, i), offset, !modelBoolean[i],
						modelBoolean[i]));

			} else if (method.equals(WCOMPAREANDSETR)) {
				myUnsafe.putBoolean(base(target, i), offset, !modelBoolean[i]);
				checkTrueAt(myUnsafe.weakCompareAndSetBooleanRelease(base(target, i), offset, !modelBoolean[i],
						modelBoolean[i]));

			} else if (method.equals(COMPAREANDEXCH)) {
				myUnsafe.putBoolean(base(target, i), offset, !modelBoolean[i]);
				checkSameAt(!modelBoolean[i],
						myUnsafe.compareAndExchangeBoolean(base(target, i), offset, !modelBoolean[i], modelBoolean[i]));

			} else if (method.equals(COMPAREANDEXCHA)) {
				myUnsafe.putBoolean(base(target, i), offset, !modelBoolean[i]);
				checkSameAt(!modelBoolean[i], myUnsafe.compareAndExchangeBooleanAcquire(base(target, i), offset,
						!modelBoolean[i], modelBoolean[i]));

			} else if (method.equals(COMPAREANDEXCHR)) {
				myUnsafe.putBoolean(base(target, i), offset, !modelBoolean[i]);
				checkSameAt(!modelBoolean[i], myUnsafe.compareAndExchangeBooleanRelease(base(target, i), offset,
						!modelBoolean[i], modelBoolean[i]));

			} else { /* default */
				myUnsafe.putBoolean(base(target, i), offset, modelBoolean[i]);
			}
			modelCheck(modelBoolean, target, i);
		}
	}

	protected void testObject(Object target, String method) throws Exception {
		for (int i = 0; i < models.length; i++) {
			long offset = offset(target, i);
			getLogger().debug("testObject Mathod: " + method + "- Object: " + target.getClass().getName() + ", Offset: "
					+ offset + ", Data: " + models[i] + ", Index " + i);
			if (method.equals(VOLATILE)) {
				myUnsafe.putObjectVolatile(base(target, i), offset, models[i]);
			} else if (method.equals(OPAQUE)) {
				myUnsafe.putObjectOpaque(base(target, i), offset, models[i]);

			} else if (method.equals(ORDERED)) {
				myUnsafe.putObjectRelease(base(target, i), offset, models[i]);

			} else if (method.equals(COMPAREANDSET)) {
				myUnsafe.putObject(base(target, i), offset, compareValueObject);
				checkTrueAt(myUnsafe.compareAndSetObject(base(target, i), offset, compareValueObject, models[i]));

			} else if (method.equals(WCOMPAREANDSET)) {
				myUnsafe.putObject(base(target, i), offset, compareValueObject);
				checkTrueAt(myUnsafe.weakCompareAndSetObject(base(target, i), offset, compareValueObject, models[i]));

			} else if (method.equals(WCOMPAREANDSETA)) {
				myUnsafe.putObject(base(target, i), offset, compareValueObject);
				checkTrueAt(myUnsafe.weakCompareAndSetObjectAcquire(base(target, i), offset, compareValueObject,
						models[i]));

			} else if (method.equals(WCOMPAREANDSETR)) {
				myUnsafe.putObject(base(target, i), offset, compareValueObject);
				checkTrueAt(myUnsafe.weakCompareAndSetObjectRelease(base(target, i), offset, compareValueObject,
						models[i]));

			} else if (method.equals(WCOMPAREANDSETP)) {
				myUnsafe.putObject(base(target, i), offset, compareValueObject);
				checkTrueAt(
						myUnsafe.weakCompareAndSetObjectPlain(base(target, i), offset, compareValueObject, models[i]));

			} else if (method.equals(COMPAREANDEXCH)) {
				myUnsafe.putObject(base(target, i), offset, compareValueObject);
				checkSameAt(compareValueObject,
						myUnsafe.compareAndExchangeObject(base(target, i), offset, compareValueObject, models[i]));

			} else if (method.equals(COMPAREANDEXCHA)) {
				myUnsafe.putObject(base(target, i), offset, compareValueObject);
				checkSameAt(compareValueObject, myUnsafe.compareAndExchangeObjectAcquire(base(target, i), offset,
						compareValueObject, models[i]));

			} else if (method.equals(COMPAREANDEXCHR)) {
				myUnsafe.putObject(base(target, i), offset, compareValueObject);
				checkSameAt(compareValueObject, myUnsafe.compareAndExchangeObjectRelease(base(target, i), offset,
						compareValueObject, models[i]));

			} else {
				myUnsafe.putObject(base(target, i), offset, models[i]);
			}
			modelCheck(models, target, i);

		}
	}

	protected void testGetByte(Object target, String method) throws Exception {
		init(target);
		for (int i = 0; i < modelByte.length; i++) {
			long offset = offset(target, i);
			getLogger().debug("testGetByte Method: " + method + "- Object: " + target.getClass() + ", Offset: " + offset
					+ ", Index: " + i);

			if (method.equals(VOLATILE)) {
				checkSameAt(i, target, myUnsafe.getByteVolatile(base(target, i), offset));
			} else if (method.equals(OPAQUE)) {
				checkSameAt(i, target, myUnsafe.getByteOpaque(base(target, i), offset));
			} else if (method.equals(ORDERED)) {
				checkSameAt(i, target, myUnsafe.getByteAcquire(base(target, i), offset));
			} else if (method.equals(GETANDSET)) {
				byte getValue = modelByte[(i + 1) % modelByte.length];
				myUnsafe.putByte(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetByte(base(target, i), offset, modelByte[i]));
				checkSameAt(i, target, modelByte[i]);
			} else if (method.equals(GETANDSETA)) {
				byte getValue = modelByte[(i + 1) % modelByte.length];
				myUnsafe.putByte(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetByteAcquire(base(target, i), offset, modelByte[i]));
				checkSameAt(i, target, modelByte[i]);
			} else if (method.equals(GETANDSETR)) {
				byte getValue = modelByte[(i + 1) % modelByte.length];
				myUnsafe.putByte(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetByteRelease(base(target, i), offset, modelByte[i]));
				checkSameAt(i, target, modelByte[i]);
			} else if (method.equals(GETANDADD)) {
				myUnsafe.putByte(base(target, i), offset, modelByte[i]);

				checkSameAt(modelByte[i], myUnsafe.getAndAddByte(base(target, i), offset, modelByte[i]));
				checkSameAt(i, target, (byte) (modelByte[i] + modelByte[i]));
			} else if (method.equals(GETANDADDA)) {
				myUnsafe.putByte(base(target, i), offset, modelByte[i]);

				checkSameAt(modelByte[i], myUnsafe.getAndAddByteAcquire(base(target, i), offset, modelByte[i]));
				checkSameAt(i, target, (byte) (modelByte[i] + modelByte[i]));
			} else if (method.equals(GETANDADDR)) {
				myUnsafe.putByte(base(target, i), offset, modelByte[i]);

				checkSameAt(modelByte[i], myUnsafe.getAndAddByteRelease(base(target, i), offset, modelByte[i]));
				checkSameAt(i, target, (byte) (modelByte[i] + modelByte[i]));
			} else if (method.equals(GETANDBITWISEOR)) {
				myUnsafe.putByte(base(target, i), offset, modelByte[i]);

				checkSameAt(modelByte[i], myUnsafe.getAndBitwiseOrByte(base(target, i), offset, bitwiseValueByte));
				checkSameAt(myUnsafe.getByte(base(target, i), offset), (byte) (bitwiseValueByte | modelByte[i]));
			} else if (method.equals(GETANDBITWISEORA)) {
				myUnsafe.putByte(base(target, i), offset, modelByte[i]);

				checkSameAt(modelByte[i],
						myUnsafe.getAndBitwiseOrByteAcquire(base(target, i), offset, bitwiseValueByte));
				checkSameAt(myUnsafe.getByte(base(target, i), offset), (byte) (bitwiseValueByte | modelByte[i]));

			} else if (method.equals(GETANDBITWISEORR)) {
				myUnsafe.putByte(base(target, i), offset, modelByte[i]);

				checkSameAt(modelByte[i],
						myUnsafe.getAndBitwiseOrByteRelease(base(target, i), offset, bitwiseValueByte));
				checkSameAt(myUnsafe.getByte(base(target, i), offset), (byte) (bitwiseValueByte | modelByte[i]));
			} else if (method.equals(GETANDBITWISEAND)) {
				myUnsafe.putByte(base(target, i), offset, modelByte[i]);

				checkSameAt(modelByte[i], myUnsafe.getAndBitwiseAndByte(base(target, i), offset, bitwiseValueByte));
				checkSameAt(myUnsafe.getByte(base(target, i), offset), (byte) (bitwiseValueByte & modelByte[i]));
			} else if (method.equals(GETANDBITWISEANDA)) {
				myUnsafe.putByte(base(target, i), offset, modelByte[i]);

				checkSameAt(modelByte[i],
						myUnsafe.getAndBitwiseAndByteAcquire(base(target, i), offset, bitwiseValueByte));
				checkSameAt(myUnsafe.getByte(base(target, i), offset), (byte) (bitwiseValueByte & modelByte[i]));
			} else if (method.equals(GETANDBITWISEANDR)) {
				myUnsafe.putByte(base(target, i), offset, modelByte[i]);

				checkSameAt(modelByte[i],
						myUnsafe.getAndBitwiseAndByteRelease(base(target, i), offset, bitwiseValueByte));
				checkSameAt(myUnsafe.getByte(base(target, i), offset), (byte) (bitwiseValueByte & modelByte[i]));
			} else if (method.equals(GETANDBITWISEXOR)) {
				myUnsafe.putByte(base(target, i), offset, modelByte[i]);

				checkSameAt(modelByte[i], myUnsafe.getAndBitwiseXorByte(base(target, i), offset, bitwiseValueByte));
				checkSameAt(myUnsafe.getByte(base(target, i), offset), (byte) (bitwiseValueByte ^ modelByte[i]));
			} else if (method.equals(GETANDBITWISEXORA)) {
				myUnsafe.putByte(base(target, i), offset, modelByte[i]);

				checkSameAt(modelByte[i],
						myUnsafe.getAndBitwiseXorByteAcquire(base(target, i), offset, bitwiseValueByte));
				checkSameAt(myUnsafe.getByte(base(target, i), offset), (byte) (bitwiseValueByte ^ modelByte[i]));
			} else if (method.equals(GETANDBITWISEXORR)) {
				myUnsafe.putByte(base(target, i), offset, modelByte[i]);

				checkSameAt(modelByte[i],
						myUnsafe.getAndBitwiseXorByteRelease(base(target, i), offset, bitwiseValueByte));
				checkSameAt(myUnsafe.getByte(base(target, i), offset), (byte) (bitwiseValueByte ^ modelByte[i]));

			} else { /* default */
				checkSameAt(i, target, myUnsafe.getByte(base(target, i), offset));
			}
		}
	}

	protected void testGetChar(Object target, String method) throws Exception {
		init(target);
		for (int i = 0; i < modelChar.length; i++) {
			long offset = offset(target, i);

			getLogger().debug("testGetChar Method: " + method + "- Object: " + target.getClass() + ", Offset: " + offset
					+ ", Index: " + i);

			if (method.equals(VOLATILE)) {
				checkSameAt(i, target, myUnsafe.getCharVolatile(base(target, i), offset));
			} else if (method.equals(OPAQUE)) {
				checkSameAt(i, target, myUnsafe.getCharOpaque(base(target, i), offset));
			} else if (method.equals(ORDERED)) {
				checkSameAt(i, target, myUnsafe.getCharAcquire(base(target, i), offset));
			} else if (method.equals(UNALIGNED)) {
				int sizeOfChar = 2; // bytes

				for (long uOffset = (offset + sizeOfChar); uOffset >= offset; --uOffset) {
					myUnsafe.putCharUnaligned(base(target, i), uOffset, modelChar[i]);

					char getValue = generateUnalignedChar(target, uOffset);

					checkSameAt(getValue, myUnsafe.getCharUnaligned(base(target, i), uOffset));

					getLogger().debug("getUnaligned() endianness test");

					checkSameAt(getValue, myUnsafe.getCharUnaligned(base(target, i), uOffset, myUnsafe.isBigEndian()));
				}

			} else if (method.equals(GETANDSET)) {
				char getValue = modelChar[(i + 1) % modelChar.length];
				myUnsafe.putChar(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetChar(base(target, i), offset, modelChar[i]));
				checkSameAt(i, target, modelChar[i]);

			} else if (method.equals(GETANDSETA)) {
				char getValue = modelChar[(i + 1) % modelChar.length];
				myUnsafe.putChar(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetCharAcquire(base(target, i), offset, modelChar[i]));
				checkSameAt(i, target, modelChar[i]);
			} else if (method.equals(GETANDSETR)) {
				char getValue = modelChar[(i + 1) % modelChar.length];
				myUnsafe.putChar(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetCharRelease(base(target, i), offset, modelChar[i]));
				checkSameAt(i, target, modelChar[i]);
			} else if (method.equals(GETANDADD)) {
				myUnsafe.putChar(base(target, i), offset, modelChar[i]);

				checkSameAt(modelChar[i], myUnsafe.getAndAddChar(base(target, i), offset, modelChar[i]));
				checkSameAt(i, target, (char) (modelChar[i] + modelChar[i]));
			} else if (method.equals(GETANDADDA)) {
				myUnsafe.putChar(base(target, i), offset, modelChar[i]);

				checkSameAt(modelChar[i], myUnsafe.getAndAddCharAcquire(base(target, i), offset, modelChar[i]));
				checkSameAt(i, target, (char) (modelChar[i] + modelChar[i]));
			} else if (method.equals(GETANDADDR)) {
				myUnsafe.putChar(base(target, i), offset, modelChar[i]);

				checkSameAt(modelChar[i], myUnsafe.getAndAddCharRelease(base(target, i), offset, modelChar[i]));
				checkSameAt(i, target, (char) (modelChar[i] + modelChar[i]));
			} else if (method.equals(GETANDBITWISEOR)) {
				myUnsafe.putChar(base(target, i), offset, modelChar[i]);

				checkSameAt(modelChar[i], myUnsafe.getAndBitwiseOrChar(base(target, i), offset, bitwiseValueChar));
				checkSameAt(i, target, (char) (bitwiseValueChar | modelChar[i]));
			} else if (method.equals(GETANDBITWISEORA)) {
				myUnsafe.putChar(base(target, i), offset, modelChar[i]);

				checkSameAt(modelChar[i],
						myUnsafe.getAndBitwiseOrCharAcquire(base(target, i), offset, bitwiseValueChar));
				checkSameAt(i, target, (char) (bitwiseValueChar | modelChar[i]));
			} else if (method.equals(GETANDBITWISEORR)) {
				myUnsafe.putChar(base(target, i), offset, modelChar[i]);

				checkSameAt(modelChar[i],
						myUnsafe.getAndBitwiseOrCharRelease(base(target, i), offset, bitwiseValueChar));
				checkSameAt(i, target, (char) (bitwiseValueChar | modelChar[i]));
			} else if (method.equals(GETANDBITWISEAND)) {
				myUnsafe.putChar(base(target, i), offset, modelChar[i]);

				checkSameAt(modelChar[i], myUnsafe.getAndBitwiseAndChar(base(target, i), offset, bitwiseValueChar));
				checkSameAt(i, target, (char) (bitwiseValueChar & modelChar[i]));
			} else if (method.equals(GETANDBITWISEANDA)) {
				myUnsafe.putChar(base(target, i), offset, modelChar[i]);

				checkSameAt(modelChar[i],
						myUnsafe.getAndBitwiseAndCharAcquire(base(target, i), offset, bitwiseValueChar));
				checkSameAt(i, target, (char) (bitwiseValueChar & modelChar[i]));
			} else if (method.equals(GETANDBITWISEANDR)) {
				myUnsafe.putChar(base(target, i), offset, modelChar[i]);

				checkSameAt(modelChar[i],
						myUnsafe.getAndBitwiseAndCharRelease(base(target, i), offset, bitwiseValueChar));
				checkSameAt(i, target, (char) (bitwiseValueChar & modelChar[i]));
			} else if (method.equals(GETANDBITWISEXOR)) {
				myUnsafe.putChar(base(target, i), offset, modelChar[i]);

				checkSameAt(modelChar[i], myUnsafe.getAndBitwiseXorChar(base(target, i), offset, bitwiseValueChar));
				checkSameAt(i, target, (char) (bitwiseValueChar ^ modelChar[i]));
			} else if (method.equals(GETANDBITWISEXORA)) {
				myUnsafe.putChar(base(target, i), offset, modelChar[i]);

				checkSameAt(modelChar[i],
						myUnsafe.getAndBitwiseXorCharAcquire(base(target, i), offset, bitwiseValueChar));
				checkSameAt(i, target, (char) (bitwiseValueChar ^ modelChar[i]));
			} else if (method.equals(GETANDBITWISEXORR)) {
				myUnsafe.putChar(base(target, i), offset, modelChar[i]);

				checkSameAt(modelChar[i],
						myUnsafe.getAndBitwiseXorCharRelease(base(target, i), offset, bitwiseValueChar));
				checkSameAt(i, target, (char) (bitwiseValueChar ^ modelChar[i]));

			} else { /* default */
				checkSameAt(i, target, myUnsafe.getChar(base(target, i), offset));
			}
		}
	}

	protected void testGetShort(Object target, String method) throws Exception {
		init(target);
		for (int i = 0; i < modelShort.length; i++) {
			long offset = offset(target, i);
			getLogger().debug("testGetShort Method: " + method + "- Object: " + target.getClass() + ", Offset: "
					+ offset + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				checkSameAt(i, target, myUnsafe.getShortVolatile(base(target, i), offset));
			} else if (method.equals(OPAQUE)) {
				checkSameAt(i, target, myUnsafe.getShortOpaque(base(target, i), offset));
			} else if (method.equals(ORDERED)) {
				checkSameAt(i, target, myUnsafe.getShortAcquire(base(target, i), offset));
			} else if (method.equals(UNALIGNED)) {
				int sizeOfShort = 2; // bytes

				for (long uOffset = (offset + sizeOfShort); uOffset >= offset; --uOffset) {
					myUnsafe.putShortUnaligned(base(target, i), uOffset, modelShort[i]);

					short getValue = generateUnalignedShort(target, uOffset);

					checkSameAt(getValue, myUnsafe.getShortUnaligned(base(target, i), uOffset));
					getLogger().debug("getUnaligned() endianness test");
					checkSameAt(getValue, myUnsafe.getShortUnaligned(base(target, i), uOffset, myUnsafe.isBigEndian()));
				}

			} else if (method.equals(GETANDSET)) {
				short getValue = modelShort[(i + 1) % modelShort.length];
				myUnsafe.putShort(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetShort(base(target, i), offset, modelShort[i]));
				checkSameAt(i, target, modelShort[i]);

			} else if (method.equals(GETANDSETA)) {
				short getValue = modelShort[(i + 1) % modelShort.length];
				myUnsafe.putShort(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetShortAcquire(base(target, i), offset, modelShort[i]));
				checkSameAt(i, target, modelShort[i]);
			} else if (method.equals(GETANDSETR)) {
				short getValue = modelShort[(i + 1) % modelShort.length];
				myUnsafe.putShort(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetShortRelease(base(target, i), offset, modelShort[i]));
				checkSameAt(i, target, modelShort[i]);
			} else if (method.equals(GETANDADD)) {
				myUnsafe.putShort(base(target, i), offset, modelShort[i]);

				checkSameAt(modelShort[i], myUnsafe.getAndAddShort(base(target, i), offset, modelShort[i]));
				checkSameAt(i, target, (short) (modelShort[i] + modelShort[i]));
			} else if (method.equals(GETANDADDA)) {
				myUnsafe.putShort(base(target, i), offset, modelShort[i]);

				checkSameAt(modelShort[i], myUnsafe.getAndAddShortAcquire(base(target, i), offset, modelShort[i]));
				checkSameAt(i, target, (short) (modelShort[i] + modelShort[i]));
			} else if (method.equals(GETANDADDR)) {
				myUnsafe.putShort(base(target, i), offset, modelShort[i]);

				checkSameAt(modelShort[i], myUnsafe.getAndAddShortRelease(base(target, i), offset, modelShort[i]));
				checkSameAt(i, target, (short) (modelShort[i] + modelShort[i]));
			} else if (method.equals(GETANDBITWISEOR)) {
				myUnsafe.putShort(base(target, i), offset, modelShort[i]);

				checkSameAt(modelShort[i], myUnsafe.getAndBitwiseOrShort(base(target, i), offset, bitwiseValueShort));
				checkSameAt(i, target, (short) (bitwiseValueShort | modelShort[i]));
			} else if (method.equals(GETANDBITWISEORA)) {
				myUnsafe.putShort(base(target, i), offset, modelShort[i]);

				checkSameAt(modelShort[i],
						myUnsafe.getAndBitwiseOrShortAcquire(base(target, i), offset, bitwiseValueShort));
				checkSameAt(i, target, (short) (bitwiseValueShort | modelShort[i]));
			} else if (method.equals(GETANDBITWISEORR)) {
				myUnsafe.putShort(base(target, i), offset, modelShort[i]);

				checkSameAt(modelShort[i],
						myUnsafe.getAndBitwiseOrShortRelease(base(target, i), offset, bitwiseValueShort));
				checkSameAt(i, target, (short) (bitwiseValueShort | modelShort[i]));
			} else if (method.equals(GETANDBITWISEAND)) {
				myUnsafe.putShort(base(target, i), offset, modelShort[i]);

				checkSameAt(modelShort[i], myUnsafe.getAndBitwiseAndShort(base(target, i), offset, bitwiseValueShort));
				checkSameAt(i, target, (short) (bitwiseValueShort & modelShort[i]));
			} else if (method.equals(GETANDBITWISEANDA)) {
				myUnsafe.putShort(base(target, i), offset, modelShort[i]);

				checkSameAt(modelShort[i],
						myUnsafe.getAndBitwiseAndShortAcquire(base(target, i), offset, bitwiseValueShort));
				checkSameAt(i, target, (short) (bitwiseValueShort & modelShort[i]));
			} else if (method.equals(GETANDBITWISEANDR)) {
				myUnsafe.putShort(base(target, i), offset, modelShort[i]);

				checkSameAt(modelShort[i],
						myUnsafe.getAndBitwiseAndShortRelease(base(target, i), offset, bitwiseValueShort));
				checkSameAt(i, target, (short) (bitwiseValueShort & modelShort[i]));
			} else if (method.equals(GETANDBITWISEXOR)) {
				myUnsafe.putShort(base(target, i), offset, modelShort[i]);

				checkSameAt(modelShort[i], myUnsafe.getAndBitwiseXorShort(base(target, i), offset, bitwiseValueShort));
				checkSameAt(i, target, (short) (bitwiseValueShort ^ modelShort[i]));
			} else if (method.equals(GETANDBITWISEXORA)) {
				myUnsafe.putShort(base(target, i), offset, modelShort[i]);

				checkSameAt(modelShort[i],
						myUnsafe.getAndBitwiseXorShortAcquire(base(target, i), offset, bitwiseValueShort));
				checkSameAt(i, target, (short) (bitwiseValueShort ^ modelShort[i]));
			} else if (method.equals(GETANDBITWISEXORR)) {
				myUnsafe.putShort(base(target, i), offset, modelShort[i]);

				checkSameAt(modelShort[i],
						myUnsafe.getAndBitwiseXorShortRelease(base(target, i), offset, bitwiseValueShort));
				checkSameAt(i, target, (short) (bitwiseValueShort ^ modelShort[i]));

			} else { /* default */
				checkSameAt(i, target, myUnsafe.getShort(base(target, i), offset));
			}
		}
	}

	protected void testGetInt(Object target, String method) throws Exception {
		init(target);
		for (int i = 0; i < modelInt.length; i++) {
			long offset = offset(target, i);
			getLogger().debug("testGetInt Method: " + method + "- Object: " + target.getClass() + ", Offset: " + offset
					+ ", Index: " + i);
			if (method.equals(VOLATILE)) {
				checkSameAt(i, target, myUnsafe.getIntVolatile(base(target, i), offset));
			} else if (method.equals(OPAQUE)) {
				checkSameAt(i, target, myUnsafe.getIntOpaque(base(target, i), offset));
			} else if (method.equals(ORDERED)) {
				checkSameAt(i, target, myUnsafe.getIntAcquire(base(target, i), offset));
			} else if (method.equals(UNALIGNED)) {
				int sizeOfInt = 4; // bytes

				for (long uOffset = (offset + sizeOfInt); uOffset >= offset; --uOffset) {
					myUnsafe.putIntUnaligned(base(target, i), uOffset, modelInt[i]);

					int getValue = generateUnalignedInt(target, uOffset);

					checkSameAt(getValue, myUnsafe.getIntUnaligned(base(target, i), uOffset));
					getLogger().debug("getUnaligned() endianness test");
					checkSameAt(getValue, myUnsafe.getIntUnaligned(base(target, i), uOffset, myUnsafe.isBigEndian()));
				}

			} else if (method.equals(GETANDSET)) {
				int getValue = modelInt[i] + 1;
				myUnsafe.putInt(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetInt(base(target, i), offset, modelInt[i]));
				checkSameAt(i, target, modelInt[i]);
			} else if (method.equals(GETANDSETA)) {
				int getValue = modelInt[i] + 2;
				myUnsafe.putInt(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetIntAcquire(base(target, i), offset, modelInt[i]));
				checkSameAt(i, target, modelInt[i]);
			} else if (method.equals(GETANDSETR)) {
				int getValue = modelInt[i] + 3;
				myUnsafe.putInt(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetIntRelease(base(target, i), offset, modelInt[i]));
				checkSameAt(i, target, modelInt[i]);

			} else if (method.equals(GETANDADD)) {
				myUnsafe.putInt(base(target, i), offset, modelInt[i]);

				checkSameAt(modelInt[i], myUnsafe.getAndAddInt(base(target, i), offset, modelInt[i]));
				checkSameAt(i, target, modelInt[i] + modelInt[i]);

			} else if (method.equals(GETANDADDA)) {
				myUnsafe.putInt(base(target, i), offset, modelInt[i]);

				checkSameAt(modelInt[i], myUnsafe.getAndAddIntAcquire(base(target, i), offset, modelInt[i]));
				checkSameAt(i, target, modelInt[i] + modelInt[i]);

			} else if (method.equals(GETANDADDR)) {
				myUnsafe.putInt(base(target, i), offset, modelInt[i]);

				checkSameAt(modelInt[i], myUnsafe.getAndAddIntRelease(base(target, i), offset, modelInt[i]));
				checkSameAt(i, target, modelInt[i] + modelInt[i]);

			} else if (method.equals(GETANDBITWISEOR)) {
				myUnsafe.putInt(base(target, i), offset, modelInt[i]);

				checkSameAt(modelInt[i], myUnsafe.getAndBitwiseOrInt(base(target, i), offset, bitwiseValueInt));
				checkSameAt(i, target, bitwiseValueInt | modelInt[i]);

			} else if (method.equals(GETANDBITWISEORA)) {
				myUnsafe.putInt(base(target, i), offset, modelInt[i]);

				checkSameAt(modelInt[i], myUnsafe.getAndBitwiseOrIntAcquire(base(target, i), offset, bitwiseValueInt));
				checkSameAt(i, target, bitwiseValueInt | modelInt[i]);

			} else if (method.equals(GETANDBITWISEORR)) {
				myUnsafe.putInt(base(target, i), offset, modelInt[i]);

				checkSameAt(modelInt[i], myUnsafe.getAndBitwiseOrIntRelease(base(target, i), offset, bitwiseValueInt));
				checkSameAt(i, target, bitwiseValueInt | modelInt[i]);

			} else if (method.equals(GETANDBITWISEAND)) {
				myUnsafe.putInt(base(target, i), offset, modelInt[i]);

				checkSameAt(modelInt[i], myUnsafe.getAndBitwiseAndInt(base(target, i), offset, bitwiseValueInt));
				checkSameAt(i, target, bitwiseValueInt & modelInt[i]);

			} else if (method.equals(GETANDBITWISEANDA)) {
				myUnsafe.putInt(base(target, i), offset, modelInt[i]);

				checkSameAt(modelInt[i], myUnsafe.getAndBitwiseAndIntAcquire(base(target, i), offset, bitwiseValueInt));
				checkSameAt(i, target, bitwiseValueInt & modelInt[i]);

			} else if (method.equals(GETANDBITWISEANDR)) {
				myUnsafe.putInt(base(target, i), offset, modelInt[i]);

				checkSameAt(modelInt[i], myUnsafe.getAndBitwiseAndIntRelease(base(target, i), offset, bitwiseValueInt));
				checkSameAt(i, target, bitwiseValueInt & modelInt[i]);

			} else if (method.equals(GETANDBITWISEXOR)) {
				myUnsafe.putInt(base(target, i), offset, modelInt[i]);

				checkSameAt(modelInt[i], myUnsafe.getAndBitwiseXorInt(base(target, i), offset, bitwiseValueInt));
				checkSameAt(i, target, bitwiseValueInt ^ modelInt[i]);

			} else if (method.equals(GETANDBITWISEXORA)) {
				myUnsafe.putInt(base(target, i), offset, modelInt[i]);

				checkSameAt(modelInt[i], myUnsafe.getAndBitwiseXorIntAcquire(base(target, i), offset, bitwiseValueInt));
				checkSameAt(i, target, bitwiseValueInt ^ modelInt[i]);

			} else if (method.equals(GETANDBITWISEXORR)) {
				myUnsafe.putInt(base(target, i), offset, modelInt[i]);

				checkSameAt(modelInt[i], myUnsafe.getAndBitwiseXorIntRelease(base(target, i), offset, bitwiseValueInt));
				checkSameAt(i, target, bitwiseValueInt ^ modelInt[i]);

			} else {
				checkSameAt(i, target,
						myUnsafe.getInt(base(target, i), offset(target, i)));
			}
		}
	}

	protected void testGetLong(Object target, String method) throws Exception {
		init(target);
		for (int i = 0; i < modelLong.length; i++) {
			long offset = offset(target, i);
			getLogger().debug("testGetLong Method: " + method + "- Object: " + target.getClass() + ", Offset: " + offset
					+ ", Index: " + i);
			if (method.equals(VOLATILE)) {
				checkSameAt(i, target, myUnsafe.getLongVolatile(base(target, i), offset));
			} else if (method.equals(OPAQUE)) {
				checkSameAt(i, target, myUnsafe.getLongOpaque(base(target, i), offset));
			} else if (method.equals(ORDERED)) {
				checkSameAt(i, target, myUnsafe.getLongAcquire(base(target, i), offset));
			} else if (method.equals(UNALIGNED)) {
				int sizeOfLong = 8; // bytes

				for (long uOffset = (offset + sizeOfLong); uOffset >= offset; --uOffset) {
					myUnsafe.putLongUnaligned(base(target, i), uOffset, modelLong[i]);

					long getValue = generateUnalignedLong(target, uOffset);

					checkSameAt(getValue, myUnsafe.getLongUnaligned(base(target, i), uOffset));
					getLogger().debug("getUnaligned() endianness test");
					checkSameAt(getValue, myUnsafe.getLongUnaligned(base(target, i), uOffset, myUnsafe.isBigEndian()));
				}

			} else if (method.equals(GETANDSET)) {
				long getValue = modelLong[i] + 1;
				myUnsafe.putLong(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetLong(base(target, i), offset, modelLong[i]));
				checkSameAt(i, target, modelLong[i]);
			} else if (method.equals(GETANDSETA)) {
				long getValue = modelLong[i] + 2;
				myUnsafe.putLong(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetLongAcquire(base(target, i), offset, modelLong[i]));
				checkSameAt(i, target, modelLong[i]);
			} else if (method.equals(GETANDSETR)) {
				long getValue = modelLong[i] + 3;
				myUnsafe.putLong(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetLongRelease(base(target, i), offset, modelLong[i]));
				checkSameAt(i, target, modelLong[i]);
			} else if (method.equals(GETANDADD)) {
				myUnsafe.putLong(base(target, i), offset, modelLong[i]);

				checkSameAt(modelLong[i], myUnsafe.getAndAddLong(base(target, i), offset, modelLong[i]));
				checkSameAt(i, target, modelLong[i] + modelLong[i]);
			} else if (method.equals(GETANDADDA)) {
				myUnsafe.putLong(base(target, i), offset, modelLong[i]);

				checkSameAt(modelLong[i], myUnsafe.getAndAddLongAcquire(base(target, i), offset, modelLong[i]));
				checkSameAt(i, target, modelLong[i] + modelLong[i]);
			} else if (method.equals(GETANDADDR)) {
				myUnsafe.putLong(base(target, i), offset, modelLong[i]);

				checkSameAt(modelLong[i], myUnsafe.getAndAddLongRelease(base(target, i), offset, modelLong[i]));
				checkSameAt(i, target, modelLong[i] + modelLong[i]);
			} else if (method.equals(GETANDBITWISEOR)) {
				myUnsafe.putLong(base(target, i), offset, modelLong[i]);

				checkSameAt(modelLong[i], myUnsafe.getAndBitwiseOrLong(base(target, i), offset, bitwiseValueLong));
				checkSameAt(i, target, bitwiseValueLong | modelLong[i]);
			} else if (method.equals(GETANDBITWISEORA)) {
				myUnsafe.putLong(base(target, i), offset, modelLong[i]);

				checkSameAt(modelLong[i],
						myUnsafe.getAndBitwiseOrLongAcquire(base(target, i), offset, bitwiseValueLong));
				checkSameAt(i, target, bitwiseValueLong | modelLong[i]);
			} else if (method.equals(GETANDBITWISEORR)) {
				myUnsafe.putLong(base(target, i), offset, modelLong[i]);

				checkSameAt(modelLong[i],
						myUnsafe.getAndBitwiseOrLongRelease(base(target, i), offset, bitwiseValueLong));
				checkSameAt(i, target, bitwiseValueLong | modelLong[i]);
			} else if (method.equals(GETANDBITWISEAND)) {
				myUnsafe.putLong(base(target, i), offset, modelLong[i]);

				checkSameAt(modelLong[i], myUnsafe.getAndBitwiseAndLong(base(target, i), offset, bitwiseValueLong));
				checkSameAt(i, target, bitwiseValueLong & modelLong[i]);
			} else if (method.equals(GETANDBITWISEANDA)) {
				myUnsafe.putLong(base(target, i), offset, modelLong[i]);

				checkSameAt(modelLong[i],
						myUnsafe.getAndBitwiseAndLongAcquire(base(target, i), offset, bitwiseValueLong));
				checkSameAt(i, target, bitwiseValueLong & modelLong[i]);
			} else if (method.equals(GETANDBITWISEANDR)) {
				myUnsafe.putLong(base(target, i), offset, modelLong[i]);

				checkSameAt(modelLong[i],
						myUnsafe.getAndBitwiseAndLongRelease(base(target, i), offset, bitwiseValueLong));
				checkSameAt(i, target, bitwiseValueLong & modelLong[i]);
			} else if (method.equals(GETANDBITWISEXOR)) {
				myUnsafe.putLong(base(target, i), offset, modelLong[i]);

				checkSameAt(modelLong[i], myUnsafe.getAndBitwiseXorLong(base(target, i), offset, bitwiseValueLong));
				checkSameAt(i, target, bitwiseValueLong ^ modelLong[i]);
			} else if (method.equals(GETANDBITWISEXORA)) {
				myUnsafe.putLong(base(target, i), offset, modelLong[i]);

				checkSameAt(modelLong[i],
						myUnsafe.getAndBitwiseXorLongAcquire(base(target, i), offset, bitwiseValueLong));
				checkSameAt(i, target, bitwiseValueLong ^ modelLong[i]);
			} else if (method.equals(GETANDBITWISEXORR)) {
				myUnsafe.putLong(base(target, i), offset, modelLong[i]);

				checkSameAt(modelLong[i],
						myUnsafe.getAndBitwiseXorLongRelease(base(target, i), offset, bitwiseValueLong));
				checkSameAt(i, target, bitwiseValueLong ^ modelLong[i]);

			} else {
				checkSameAt(i, target, myUnsafe.getLong(base(target, i), offset));
			}
		}
	}

	protected void testGetFloat(Object target, String method) throws Exception {
		init(target);
		for (int i = 0; i < modelFloat.length; i++) {
			long offset = offset(target, i);
			getLogger().debug("testGetFloat Method: " + method + "- Object: " + target.getClass() + ", Offset: "
					+ offset + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				checkSameAt(i, target, myUnsafe.getFloatVolatile(base(target, i), offset));
			} else if (method.equals(OPAQUE)) {
				checkSameAt(i, target, myUnsafe.getFloatOpaque(base(target, i), offset));
			} else if (method.equals(ORDERED)) {
				checkSameAt(i, target, myUnsafe.getFloatAcquire(base(target, i), offset));
			} else if (method.equals(GETANDSET)) {
				float getValue = modelFloat[i] + 1;
				myUnsafe.putFloat(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetFloat(base(target, i), offset, modelFloat[i]));
				checkSameAt(i, target, modelFloat[i]);
			} else if (method.equals(GETANDSETA)) {
				float getValue = modelFloat[i] + 2;
				myUnsafe.putFloat(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetFloatAcquire(base(target, i), offset, modelFloat[i]));
				checkSameAt(i, target, modelFloat[i]);
			} else if (method.equals(GETANDSETR)) {
				float getValue = modelFloat[i] + 3;
				myUnsafe.putFloat(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetFloatRelease(base(target, i), offset, modelFloat[i]));
				checkSameAt(i, target, modelFloat[i]);
			} else if (method.equals(GETANDADD)) {
				myUnsafe.putFloat(base(target, i), offset, modelFloat[i]);

				checkSameAt(modelFloat[i], myUnsafe.getAndAddFloat(base(target, i), offset, modelFloat[i]));
				checkSameAt(i, target, modelFloat[i] + modelFloat[i]);
			} else if (method.equals(GETANDADDA)) {
				myUnsafe.putFloat(base(target, i), offset, modelFloat[i]);

				checkSameAt(modelFloat[i], myUnsafe.getAndAddFloatAcquire(base(target, i), offset, modelFloat[i]));
				checkSameAt(i, target, modelFloat[i] + modelFloat[i]);
			} else if (method.equals(GETANDADDR)) {
				myUnsafe.putFloat(base(target, i), offset, modelFloat[i]);

				checkSameAt(modelFloat[i], myUnsafe.getAndAddFloatRelease(base(target, i), offset, modelFloat[i]));
				checkSameAt(i, target, modelFloat[i] + modelFloat[i]);

			} else {
				checkSameAt(i, target,
						myUnsafe.getFloat(base(target, i), offset(target, i)));
			}
		}
	}

	protected void testGetDouble(Object target, String method) throws Exception {
		init(target);
		for (int i = 0; i < modelDouble.length; i++) {
			long offset = offset(target, i);
			getLogger().debug("testGetDouble Method: " + method + "- Object: " + target.getClass() + ", Offset: "
					+ offset + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				checkSameAt(i, target, myUnsafe.getDoubleVolatile(base(target, i), offset));
			} else if (method.equals(OPAQUE)) {
				checkSameAt(i, target, myUnsafe.getDoubleOpaque(base(target, i), offset));
			} else if (method.equals(ORDERED)) {
				checkSameAt(i, target, myUnsafe.getDoubleAcquire(base(target, i), offset));
			} else if (method.equals(GETANDSET)) {
				double getValue = modelDouble[i] + 1;
				myUnsafe.putDouble(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetDouble(base(target, i), offset, modelDouble[i]));
				checkSameAt(i, target, modelDouble[i]);
			} else if (method.equals(GETANDSETA)) {
				double getValue = modelDouble[i] + 2;
				myUnsafe.putDouble(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetDoubleAcquire(base(target, i), offset, modelDouble[i]));
				checkSameAt(i, target, modelDouble[i]);
			} else if (method.equals(GETANDSETR)) {
				double getValue = modelDouble[i] + 3;
				myUnsafe.putDouble(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetDoubleRelease(base(target, i), offset, modelDouble[i]));
				checkSameAt(i, target, modelDouble[i]);
			} else if (method.equals(GETANDADD)) {
				myUnsafe.putDouble(base(target, i), offset, modelDouble[i]);

				checkSameAt(modelDouble[i], myUnsafe.getAndAddDouble(base(target, i), offset, modelDouble[i]));
				checkSameAt(i, target, modelDouble[i] + modelDouble[i]);
			} else if (method.equals(GETANDADDA)) {
				myUnsafe.putDouble(base(target, i), offset, modelDouble[i]);

				checkSameAt(modelDouble[i], myUnsafe.getAndAddDoubleAcquire(base(target, i), offset, modelDouble[i]));
				checkSameAt(i, target, modelDouble[i] + modelDouble[i]);
			} else if (method.equals(GETANDADDR)) {
				myUnsafe.putDouble(base(target, i), offset, modelDouble[i]);

				checkSameAt(modelDouble[i], myUnsafe.getAndAddDoubleRelease(base(target, i), offset, modelDouble[i]));
				checkSameAt(i, target, modelDouble[i] + modelDouble[i]);

			} else {
				checkSameAt(i, target,
						myUnsafe.getDouble(base(target, i), offset(target, i)));
			}
		}
	}

	protected void testGetBoolean(Object target, String method) throws Exception {
		init(target);
		for (int i = 0; i < modelBoolean.length; i++) {
			long offset = offset(target, i);
			getLogger().debug("testGetBoolean Method: " + method + "- Object: " + target.getClass() + ", Offset: "
					+ offset + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				checkSameAt(i, target, myUnsafe.getBooleanVolatile(base(target, i), offset));
			} else if (method.equals(OPAQUE)) {
				checkSameAt(i, target, myUnsafe.getBooleanOpaque(base(target, i), offset));
			} else if (method.equals(ORDERED)) {
				checkSameAt(i, target, myUnsafe.getBooleanAcquire(base(target, i), offset));
			} else if (method.equals(GETANDSET)) {
				boolean getValue = modelBoolean[(i + 1) % modelBoolean.length];
				myUnsafe.putBoolean(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetBoolean(base(target, i), offset, modelBoolean[i]));
				checkSameAt(i, target, modelBoolean[i]);
			} else if (method.equals(GETANDSETA)) {
				boolean getValue = modelBoolean[(i + 1) % modelBoolean.length];
				myUnsafe.putBoolean(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetBooleanAcquire(base(target, i), offset, modelBoolean[i]));
				checkSameAt(i, target, modelBoolean[i]);
			} else if (method.equals(GETANDSETR)) {
				boolean getValue = modelBoolean[(i + 1) % modelBoolean.length];
				myUnsafe.putBoolean(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetBooleanRelease(base(target, i), offset, modelBoolean[i]));
				checkSameAt(i, target, modelBoolean[i]);
			} else if (method.equals(GETANDBITWISEOR)) {
				myUnsafe.putBoolean(base(target, i), offset, modelBoolean[i]);

				checkSameAt(modelBoolean[i], myUnsafe.getAndBitwiseOrBoolean(base(target, i), offset, modelBoolean[i]));
				checkSameAt(i, target, modelBoolean[i] | modelBoolean[i]);

			} else if (method.equals(GETANDBITWISEORA)) {
				myUnsafe.putBoolean(base(target, i), offset, modelBoolean[i]);

				checkSameAt(modelBoolean[i],
						myUnsafe.getAndBitwiseOrBooleanAcquire(base(target, i), offset, modelBoolean[i]));
				checkSameAt(i, target, modelBoolean[i] | modelBoolean[i]);

			} else if (method.equals(GETANDBITWISEORR)) {
				myUnsafe.putBoolean(base(target, i), offset, modelBoolean[i]);

				checkSameAt(modelBoolean[i],
						myUnsafe.getAndBitwiseOrBooleanRelease(base(target, i), offset, modelBoolean[i]));
				checkSameAt(i, target, modelBoolean[i] | modelBoolean[i]);

			} else if (method.equals(GETANDBITWISEAND)) {
				myUnsafe.putBoolean(base(target, i), offset, modelBoolean[i]);

				checkSameAt(modelBoolean[i],
						myUnsafe.getAndBitwiseAndBoolean(base(target, i), offset, modelBoolean[i]));
				checkSameAt(i, target, modelBoolean[i] & modelBoolean[i]);

			} else if (method.equals(GETANDBITWISEANDA)) {
				myUnsafe.putBoolean(base(target, i), offset, modelBoolean[i]);

				checkSameAt(modelBoolean[i],
						myUnsafe.getAndBitwiseAndBooleanAcquire(base(target, i), offset, modelBoolean[i]));
				checkSameAt(i, target, modelBoolean[i] & modelBoolean[i]);

			} else if (method.equals(GETANDBITWISEANDR)) {
				myUnsafe.putBoolean(base(target, i), offset, modelBoolean[i]);

				checkSameAt(modelBoolean[i],
						myUnsafe.getAndBitwiseAndBooleanRelease(base(target, i), offset, modelBoolean[i]));
				checkSameAt(i, target, modelBoolean[i] & modelBoolean[i]);

			} else if (method.equals(GETANDBITWISEXOR)) {
				myUnsafe.putBoolean(base(target, i), offset, modelBoolean[i]);

				checkSameAt(modelBoolean[i],
						myUnsafe.getAndBitwiseXorBoolean(base(target, i), offset, modelBoolean[i]));
				checkSameAt(i, target, modelBoolean[i] ^ modelBoolean[i]);

			} else if (method.equals(GETANDBITWISEXORA)) {
				myUnsafe.putBoolean(base(target, i), offset, modelBoolean[i]);

				checkSameAt(modelBoolean[i],
						myUnsafe.getAndBitwiseXorBooleanAcquire(base(target, i), offset, modelBoolean[i]));
				checkSameAt(i, target, modelBoolean[i] ^ modelBoolean[i]);

			} else if (method.equals(GETANDBITWISEXORR)) {
				myUnsafe.putBoolean(base(target, i), offset, modelBoolean[i]);

				checkSameAt(modelBoolean[i],
						myUnsafe.getAndBitwiseXorBooleanRelease(base(target, i), offset, modelBoolean[i]));
				checkSameAt(i, target, modelBoolean[i] ^ modelBoolean[i]);

			} else {
				checkSameAt(i, target,
						myUnsafe.getBoolean(base(target, i), offset(target, i)));
			}
		}
	}

	protected void testGetObject(Object target, String method) throws Exception {
		Object getValue = new Object();
		init(target);
		for (int i = 0; i < models.length; i++) {
			long offset = offset(target, i);
			getLogger().debug("testGetObject Method: " + method + "- Object: " + target.getClass() + ", Offset: "
					+ offset + ", Index: " + i);
			if (method.equals(VOLATILE)) {
				checkSameAt(i, target, myUnsafe.getObjectVolatile(base(target, i), offset));

			} else if (method.equals(OPAQUE)) {
				checkSameAt(i, target, myUnsafe.getObjectOpaque(base(target, i), offset));

			} else if (method.equals(ORDERED)) {
				checkSameAt(i, target, myUnsafe.getObjectAcquire(base(target, i), offset));

			} else if (method.equals(GETANDSET)) {
				myUnsafe.putObject(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetObject(base(target, i), offset, models[i]));
				checkSameAt(i, target, models[i]);

			} else if (method.equals(GETANDSETA)) {
				myUnsafe.putObject(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetObjectAcquire(base(target, i), offset, models[i]));
				checkSameAt(i, target, models[i]);

			} else if (method.equals(GETANDSETR)) {
				myUnsafe.putObject(base(target, i), offset, getValue);

				checkSameAt(getValue, myUnsafe.getAndSetObjectRelease(base(target, i), offset, models[i]));
				checkSameAt(i, target, models[i]);

			} else {

				checkSameAt(i, target, myUnsafe.getObject(base(target, i), offset));
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

			} else if (method.equals(OPAQUE)) {
				myUnsafe.putByteOpaque(null, pointers[i], modelByte[i]);

			} else if (method.equals(ORDERED)) {
				myUnsafe.putByteRelease(null, pointers[i], modelByte[i]);

			} else if (method.equals(COMPAREANDSET)) {
				myUnsafe.putByte(null, pointers[i], compareValueByte);
				checkTrueAt(myUnsafe.compareAndSetByte(null, pointers[i], compareValueByte, modelByte[i]));

			} else if (method.equals(WCOMPAREANDSET)) {
				myUnsafe.putByte(null, pointers[i], compareValueByte);
				AssertJUnit
						.assertTrue(myUnsafe.weakCompareAndSetByte(null, pointers[i], compareValueByte, modelByte[i]));
			} else if (method.equals(WCOMPAREANDSETP)) {
				myUnsafe.putByte(null, pointers[i], compareValueByte);
				checkTrueAt(myUnsafe.weakCompareAndSetBytePlain(null, pointers[i], compareValueByte, modelByte[i]));
			} else if (method.equals(WCOMPAREANDSETA)) {
				myUnsafe.putByte(null, pointers[i], compareValueByte);
				checkTrueAt(myUnsafe.weakCompareAndSetByteAcquire(null, pointers[i], compareValueByte, modelByte[i]));
			} else if (method.equals(WCOMPAREANDSETR)) {
				myUnsafe.putByte(null, pointers[i], compareValueByte);
				checkTrueAt(myUnsafe.weakCompareAndSetByteRelease(null, pointers[i], compareValueByte, modelByte[i]));
			} else if (method.equals(COMPAREANDEXCH)) {
				myUnsafe.putByte(null, pointers[i], compareValueByte);
				checkSameAt(compareValueByte,
						myUnsafe.compareAndExchangeByte(null, pointers[i], compareValueByte, modelByte[i]));
			} else if (method.equals(COMPAREANDEXCHA)) {
				myUnsafe.putByte(null, pointers[i], compareValueByte);
				checkSameAt(compareValueByte,
						myUnsafe.compareAndExchangeByteAcquire(null, pointers[i], compareValueByte, modelByte[i]));
			} else if (method.equals(COMPAREANDEXCHR)) {
				myUnsafe.putByte(null, pointers[i], compareValueByte);
				checkSameAt(compareValueByte,
						myUnsafe.compareAndExchangeByteRelease(null, pointers[i], compareValueByte, modelByte[i]));

			} else {
				myUnsafe.putByte(pointers[i], modelByte[i]);
			}
			byteCheck(modelByte, i, pointers, method);
		}
		short value = myUnsafe.getShort(null, pointers[0]);
		checkSameAt((short) -1, value);
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

			} else if (method.equals(OPAQUE)) {
				myUnsafe.putCharOpaque(null, pointers[i], modelChar[i]);

			} else if (method.equals(ORDERED)) {
				myUnsafe.putCharRelease(null, pointers[i], modelChar[i]);

			} else if (method.equals(UNALIGNED)) {
				/* do not run this test at end of array to avoid corrupting memory we don't own */
				if (i == (modelChar.length - 1)) continue;
				
				int sizeOfChar = 2; // bytes

				for (long uOffset = (pointers[i] + sizeOfChar); uOffset >= pointers[i]; --uOffset) {
					/* putUnaligned */
					myUnsafe.putCharUnaligned(null, uOffset, modelChar[i]);
					modelCheckUnalignedChar(modelChar, null, i, uOffset);

					getLogger().debug("putUnaligned() endianness test");

					/* putUnaligned - endianness */
					myUnsafe.putCharUnaligned(null, uOffset, modelChar[i], myUnsafe.isBigEndian());
					modelCheckUnalignedChar(modelChar, null, i, uOffset);
				}

			} else if (method.equals(COMPAREANDSET)) {
				myUnsafe.putChar(null, pointers[i], compareValueChar);
				checkTrueAt(myUnsafe.compareAndSetChar(null, pointers[i], compareValueChar, modelChar[i]));
			} else if (method.equals(WCOMPAREANDSET)) {
				myUnsafe.putChar(null, pointers[i], compareValueChar);
				AssertJUnit
						.assertTrue(myUnsafe.weakCompareAndSetChar(null, pointers[i], compareValueChar, modelChar[i]));
			} else if (method.equals(WCOMPAREANDSETP)) {
				myUnsafe.putChar(null, pointers[i], compareValueChar);
				checkTrueAt(myUnsafe.weakCompareAndSetCharPlain(null, pointers[i], compareValueChar, modelChar[i]));
			} else if (method.equals(WCOMPAREANDSETA)) {
				myUnsafe.putChar(null, pointers[i], compareValueChar);
				checkTrueAt(myUnsafe.weakCompareAndSetCharAcquire(null, pointers[i], compareValueChar, modelChar[i]));
			} else if (method.equals(WCOMPAREANDSETR)) {
				myUnsafe.putChar(null, pointers[i], compareValueChar);
				checkTrueAt(myUnsafe.weakCompareAndSetCharRelease(null, pointers[i], compareValueChar, modelChar[i]));
			} else if (method.equals(COMPAREANDEXCH)) {
				myUnsafe.putChar(null, pointers[i], compareValueChar);
				checkSameAt(compareValueChar,
						myUnsafe.compareAndExchangeChar(null, pointers[i], compareValueChar, modelChar[i]));
			} else if (method.equals(COMPAREANDEXCHA)) {
				myUnsafe.putChar(null, pointers[i], compareValueChar);
				checkSameAt(compareValueChar,
						myUnsafe.compareAndExchangeCharAcquire(null, pointers[i], compareValueChar, modelChar[i]));
			} else if (method.equals(COMPAREANDEXCHR)) {
				myUnsafe.putChar(null, pointers[i], compareValueChar);
				checkSameAt(compareValueChar,
						myUnsafe.compareAndExchangeCharRelease(null, pointers[i], compareValueChar, modelChar[i]));

			} else {
				myUnsafe.putChar(pointers[i], modelChar[i]);
			}
			charCheck(modelChar, i, pointers, method);
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

			} else if (method.equals(OPAQUE)) {
				myUnsafe.putShortOpaque(null, pointers[i], modelShort[i]);

			} else if (method.equals(ORDERED)) {
				myUnsafe.putShortRelease(null, pointers[i], modelShort[i]);

			} else if (method.equals(UNALIGNED)) {
				/* do not run this test at end of array to avoid corrupting memory we don't own */
				if (i == (modelShort.length - 1)) continue;
				
				int sizeOfShort = 2; // bytes

				for (long uOffset = (pointers[i] + sizeOfShort); uOffset >= pointers[i]; --uOffset) {
					/* putUnaligned */
					myUnsafe.putShortUnaligned(null, uOffset, modelShort[i]);
					modelCheckUnalignedShort(modelShort, null, i, uOffset);

					getLogger().debug("putUnaligned() endianness test");

					/* putUnaligned - endianness */
					myUnsafe.putShortUnaligned(null, uOffset, modelShort[i], myUnsafe.isBigEndian());
					modelCheckUnalignedShort(modelShort, null, i, uOffset);
				}

			} else if (method.equals(COMPAREANDSET)) {
				myUnsafe.putShort(null, pointers[i], compareValueShort);
				AssertJUnit
						.assertTrue(myUnsafe.compareAndSetShort(null, pointers[i], compareValueShort, modelShort[i]));
			} else if (method.equals(WCOMPAREANDSET)) {
				myUnsafe.putShort(null, pointers[i], compareValueShort);
				checkTrueAt(myUnsafe.weakCompareAndSetShort(null, pointers[i], compareValueShort, modelShort[i]));
			} else if (method.equals(WCOMPAREANDSETP)) {
				myUnsafe.putShort(null, pointers[i], compareValueShort);
				checkTrueAt(myUnsafe.weakCompareAndSetShortPlain(null, pointers[i], compareValueShort, modelShort[i]));
			} else if (method.equals(WCOMPAREANDSETA)) {
				myUnsafe.putShort(null, pointers[i], compareValueShort);
				checkTrueAt(
						myUnsafe.weakCompareAndSetShortAcquire(null, pointers[i], compareValueShort, modelShort[i]));
			} else if (method.equals(WCOMPAREANDSETR)) {
				myUnsafe.putShort(null, pointers[i], compareValueShort);
				checkTrueAt(
						myUnsafe.weakCompareAndSetShortRelease(null, pointers[i], compareValueShort, modelShort[i]));
			} else if (method.equals(COMPAREANDEXCH)) {
				myUnsafe.putShort(null, pointers[i], compareValueShort);
				checkSameAt(compareValueShort,
						myUnsafe.compareAndExchangeShort(null, pointers[i], compareValueShort, modelShort[i]));
			} else if (method.equals(COMPAREANDEXCHA)) {
				myUnsafe.putShort(null, pointers[i], compareValueShort);
				checkSameAt(compareValueShort,
						myUnsafe.compareAndExchangeShortAcquire(null, pointers[i], compareValueShort, modelShort[i]));
			} else if (method.equals(COMPAREANDEXCHR)) {
				myUnsafe.putShort(null, pointers[i], compareValueShort);
				checkSameAt(compareValueShort,
						myUnsafe.compareAndExchangeShortRelease(null, pointers[i], compareValueShort, modelShort[i]));

			} else {
				myUnsafe.putShort(pointers[i], modelShort[i]);
			}
			shortCheck(modelShort, i, pointers, method);
		}
	}

	protected void testIntNative(String method) throws Exception {
		mem = memAllocate(100);
		long pointers[] = new long[modelInt.length];
		for (int i = 0; i < modelInt.length; i++) {
			pointers[i] = mem + (i * 4); // int size: 4
			getLogger()
					.debug("testIntNative Method: " + method + "- Pointer: " + pointers[i] + ", Data: " + modelInt[i]);
			if (method.equals(VOLATILE)) {
				myUnsafe.putIntVolatile(null, pointers[i], modelInt[i]);

			} else if (method.equals(DEFAULT)) {
				myUnsafe.putInt(null, pointers[i], modelInt[i]);

			} else if (method.equals(OPAQUE)) {
				myUnsafe.putIntOpaque(null, pointers[i], modelInt[i]);

			} else if (method.equals(ORDERED)) {
				myUnsafe.putIntRelease(null, pointers[i], modelInt[i]);

			} else if (method.equals(UNALIGNED)) {
				/* do not run this test at end of array to avoid corrupting memory we don't own */
				if (i == (modelInt.length - 1)) continue;
				
				int sizeOfInt = 4; // bytes

				for (long uOffset = (pointers[i] + sizeOfInt); uOffset >= pointers[i]; --uOffset) {
					/* putUnaligned */
					myUnsafe.putIntUnaligned(null, uOffset, modelInt[i]);
					modelCheckUnalignedInt(modelInt, null, i, uOffset);

					getLogger().debug("putUnaligned() endianness test");

					/* putUnaligned - endianness */
					myUnsafe.putIntUnaligned(null, uOffset, modelInt[i], myUnsafe.isBigEndian());
					modelCheckUnalignedInt(modelInt, null, i, uOffset);
				}

			} else if (method.equals(COMPAREANDSET)) {
				myUnsafe.putInt(null, pointers[i], compareValueInt);
				checkTrueAt(myUnsafe.compareAndSetInt(null, pointers[i], compareValueInt, modelInt[i]));
			} else if (method.equals(WCOMPAREANDSET)) {
				myUnsafe.putInt(null, pointers[i], compareValueInt);
				checkTrueAt(myUnsafe.weakCompareAndSetInt(null, pointers[i], compareValueInt, modelInt[i]));
			} else if (method.equals(WCOMPAREANDSETP)) {
				myUnsafe.putInt(null, pointers[i], compareValueInt);
				checkTrueAt(myUnsafe.weakCompareAndSetIntPlain(null, pointers[i], compareValueInt, modelInt[i]));
			} else if (method.equals(WCOMPAREANDSETA)) {
				myUnsafe.putInt(null, pointers[i], compareValueInt);
				checkTrueAt(myUnsafe.weakCompareAndSetIntAcquire(null, pointers[i], compareValueInt, modelInt[i]));
			} else if (method.equals(WCOMPAREANDSETR)) {
				myUnsafe.putInt(null, pointers[i], compareValueInt);
				checkTrueAt(myUnsafe.weakCompareAndSetIntRelease(null, pointers[i], compareValueInt, modelInt[i]));
			} else if (method.equals(COMPAREANDEXCH)) {
				myUnsafe.putInt(null, pointers[i], compareValueInt);
				checkSameAt(compareValueInt,
						myUnsafe.compareAndExchangeInt(null, pointers[i], compareValueInt, modelInt[i]));
			} else if (method.equals(COMPAREANDEXCHA)) {
				myUnsafe.putInt(null, pointers[i], compareValueInt);
				checkSameAt(compareValueInt,
						myUnsafe.compareAndExchangeIntAcquire(null, pointers[i], compareValueInt, modelInt[i]));
			} else if (method.equals(COMPAREANDEXCHR)) {
				myUnsafe.putInt(null, pointers[i], compareValueInt);
				checkSameAt(compareValueInt,
						myUnsafe.compareAndExchangeIntRelease(null, pointers[i], compareValueInt, modelInt[i]));

			} else if (method.equals(ADDRESS)) {
				myUnsafe.putAddress(pointers[i], modelInt[i]);

			} else {
				myUnsafe.putInt(pointers[i], modelInt[i]);
			}
			intCheck(modelInt, i, pointers, method);
		}
		long value = myUnsafe.getLong(null, pointers[0]);
		checkSameAt((long) -1, value);
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

			} else if (method.equals(OPAQUE)) {
				myUnsafe.putLongOpaque(null, pointers[i], modelLong[i]);

			} else if (method.equals(ORDERED)) {
				myUnsafe.putLongRelease(null, pointers[i], modelLong[i]);

			} else if (method.equals(UNALIGNED)) {
				/* do not run this test at end of array to avoid corrupting memory we don't own */
				if (i == (modelLong.length - 1)) continue;
				
				int sizeOfLong = 8; // bytes

				for (long uOffset = (pointers[i] + sizeOfLong); uOffset >= pointers[i]; --uOffset) {

					/* putUnaligned */
					myUnsafe.putLongUnaligned(null, uOffset, modelLong[i]);
					modelCheckUnalignedLong(modelLong, null, i, uOffset);

					getLogger().debug("putUnaligned() endianness test");

					/* putUnaligned - endianness */
					myUnsafe.putLongUnaligned(null, uOffset, modelLong[i], myUnsafe.isBigEndian());
					modelCheckUnalignedLong(modelLong, null, i, uOffset);
				}

			} else if (method.equals(COMPAREANDSET)) {
				myUnsafe.putLong(null, pointers[i], compareValueLong);
				checkTrueAt(myUnsafe.compareAndSetLong(null, pointers[i], compareValueLong, modelLong[i]));
			} else if (method.equals(WCOMPAREANDSET)) {
				myUnsafe.putLong(null, pointers[i], compareValueLong);
				AssertJUnit
						.assertTrue(myUnsafe.weakCompareAndSetLong(null, pointers[i], compareValueLong, modelLong[i]));
			} else if (method.equals(WCOMPAREANDSETP)) {
				myUnsafe.putLong(null, pointers[i], compareValueLong);
				checkTrueAt(myUnsafe.weakCompareAndSetLongPlain(null, pointers[i], compareValueLong, modelLong[i]));
			} else if (method.equals(WCOMPAREANDSETA)) {
				myUnsafe.putLong(null, pointers[i], compareValueLong);
				checkTrueAt(myUnsafe.weakCompareAndSetLongAcquire(null, pointers[i], compareValueLong, modelLong[i]));
			} else if (method.equals(WCOMPAREANDSETR)) {
				myUnsafe.putLong(null, pointers[i], compareValueLong);
				checkTrueAt(myUnsafe.weakCompareAndSetLongRelease(null, pointers[i], compareValueLong, modelLong[i]));
			} else if (method.equals(COMPAREANDEXCH)) {
				myUnsafe.putLong(null, pointers[i], compareValueLong);
				checkSameAt(compareValueLong,
						myUnsafe.compareAndExchangeLong(null, pointers[i], compareValueLong, modelLong[i]));
			} else if (method.equals(COMPAREANDEXCHA)) {
				myUnsafe.putLong(null, pointers[i], compareValueLong);
				checkSameAt(compareValueLong,
						myUnsafe.compareAndExchangeLongAcquire(null, pointers[i], compareValueLong, modelLong[i]));
			} else if (method.equals(COMPAREANDEXCHR)) {
				myUnsafe.putLong(null, pointers[i], compareValueLong);
				checkSameAt(compareValueLong,
						myUnsafe.compareAndExchangeLongRelease(null, pointers[i], compareValueLong, modelLong[i]));

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

			} else if (method.equals(OPAQUE)) {
				myUnsafe.putFloatOpaque(null, pointers[i], modelFloat[i]);

			} else if (method.equals(ORDERED)) {
				myUnsafe.putFloatRelease(null, pointers[i], modelFloat[i]);

			} else if (method.equals(COMPAREANDSET)) {
				myUnsafe.putFloat(null, pointers[i], compareValueFloat);
				AssertJUnit
						.assertTrue(myUnsafe.compareAndSetFloat(null, pointers[i], compareValueFloat, modelFloat[i]));
			} else if (method.equals(WCOMPAREANDSET)) {
				myUnsafe.putFloat(null, pointers[i], compareValueFloat);
				checkTrueAt(myUnsafe.weakCompareAndSetFloat(null, pointers[i], compareValueFloat, modelFloat[i]));
			} else if (method.equals(WCOMPAREANDSETP)) {
				myUnsafe.putFloat(null, pointers[i], compareValueFloat);
				checkTrueAt(myUnsafe.weakCompareAndSetFloatPlain(null, pointers[i], compareValueFloat, modelFloat[i]));
			} else if (method.equals(WCOMPAREANDSETA)) {
				myUnsafe.putFloat(null, pointers[i], compareValueFloat);
				checkTrueAt(
						myUnsafe.weakCompareAndSetFloatAcquire(null, pointers[i], compareValueFloat, modelFloat[i]));
			} else if (method.equals(WCOMPAREANDSETR)) {
				myUnsafe.putFloat(null, pointers[i], compareValueFloat);
				checkTrueAt(
						myUnsafe.weakCompareAndSetFloatRelease(null, pointers[i], compareValueFloat, modelFloat[i]));
			} else if (method.equals(COMPAREANDEXCH)) {
				myUnsafe.putFloat(null, pointers[i], compareValueFloat);
				checkSameAt(compareValueFloat,
						myUnsafe.compareAndExchangeFloat(null, pointers[i], compareValueFloat, modelFloat[i]));
			} else if (method.equals(COMPAREANDEXCHA)) {
				myUnsafe.putFloat(null, pointers[i], compareValueFloat);
				checkSameAt(compareValueFloat,
						myUnsafe.compareAndExchangeFloatAcquire(null, pointers[i], compareValueFloat, modelFloat[i]));
			} else if (method.equals(COMPAREANDEXCHR)) {
				myUnsafe.putFloat(null, pointers[i], compareValueFloat);
				checkSameAt(compareValueFloat,
						myUnsafe.compareAndExchangeFloatRelease(null, pointers[i], compareValueFloat, modelFloat[i]));

			} else {
				myUnsafe.putFloat(pointers[i], modelFloat[i]);
			}
			floatCheck(modelFloat, i, pointers, method);
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

			} else if (method.equals(OPAQUE)) {
				myUnsafe.putDoubleOpaque(null, pointers[i], modelDouble[i]);

			} else if (method.equals(ORDERED)) {
				myUnsafe.putDoubleRelease(null, pointers[i], modelDouble[i]);

			} else if (method.equals(COMPAREANDSET)) {
				myUnsafe.putDouble(null, pointers[i], compareValueDouble);
				checkTrueAt(myUnsafe.compareAndSetDouble(null, pointers[i], compareValueDouble, modelDouble[i]));
			} else if (method.equals(WCOMPAREANDSET)) {
				myUnsafe.putDouble(null, pointers[i], compareValueDouble);
				checkTrueAt(myUnsafe.weakCompareAndSetDouble(null, pointers[i], compareValueDouble, modelDouble[i]));
			} else if (method.equals(WCOMPAREANDSETP)) {
				myUnsafe.putDouble(null, pointers[i], compareValueDouble);
				checkTrueAt(
						myUnsafe.weakCompareAndSetDoublePlain(null, pointers[i], compareValueDouble, modelDouble[i]));
			} else if (method.equals(WCOMPAREANDSETA)) {
				myUnsafe.putDouble(null, pointers[i], compareValueDouble);
				checkTrueAt(
						myUnsafe.weakCompareAndSetDoubleAcquire(null, pointers[i], compareValueDouble, modelDouble[i]));
			} else if (method.equals(WCOMPAREANDSETR)) {
				myUnsafe.putDouble(null, pointers[i], compareValueDouble);
				checkTrueAt(
						myUnsafe.weakCompareAndSetDoubleRelease(null, pointers[i], compareValueDouble, modelDouble[i]));
			} else if (method.equals(COMPAREANDEXCH)) {
				myUnsafe.putDouble(null, pointers[i], compareValueDouble);
				checkSameAt(compareValueDouble,
						myUnsafe.compareAndExchangeDouble(null, pointers[i], compareValueDouble, modelDouble[i]));
			} else if (method.equals(COMPAREANDEXCHA)) {
				myUnsafe.putDouble(null, pointers[i], compareValueDouble);
				checkSameAt(compareValueDouble, myUnsafe.compareAndExchangeDoubleAcquire(null, pointers[i],
						compareValueDouble, modelDouble[i]));
			} else if (method.equals(COMPAREANDEXCHR)) {
				myUnsafe.putDouble(null, pointers[i], compareValueDouble);
				checkSameAt(compareValueDouble, myUnsafe.compareAndExchangeDoubleRelease(null, pointers[i],
						compareValueDouble, modelDouble[i]));

			} else {
				myUnsafe.putDouble(pointers[i], modelDouble[i]);
			}
			doubleCheck(modelDouble, i, pointers, method);
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

			} else if (method.equals(OPAQUE)) {
				myUnsafe.putBooleanOpaque(null, pointers[i], modelBoolean[i]);

			} else if (method.equals(ORDERED)) {
				myUnsafe.putBooleanRelease(null, pointers[i], modelBoolean[i]);

			} else if (method.equals(COMPAREANDSET)) {
				myUnsafe.putBoolean(null, pointers[i], !modelBoolean[i]);
				checkTrueAt(myUnsafe.compareAndSetBoolean(null, pointers[i], !modelBoolean[i], modelBoolean[i]));
			} else if (method.equals(WCOMPAREANDSET)) {
				myUnsafe.putBoolean(null, pointers[i], !modelBoolean[i]);
				checkTrueAt(myUnsafe.weakCompareAndSetBoolean(null, pointers[i], !modelBoolean[i], modelBoolean[i]));
			} else if (method.equals(WCOMPAREANDSETP)) {
				myUnsafe.putBoolean(null, pointers[i], !modelBoolean[i]);
				checkTrueAt(
						myUnsafe.weakCompareAndSetBooleanPlain(null, pointers[i], !modelBoolean[i], modelBoolean[i]));
			} else if (method.equals(WCOMPAREANDSETA)) {
				myUnsafe.putBoolean(null, pointers[i], !modelBoolean[i]);
				checkTrueAt(
						myUnsafe.weakCompareAndSetBooleanAcquire(null, pointers[i], !modelBoolean[i], modelBoolean[i]));
			} else if (method.equals(WCOMPAREANDSETR)) {
				myUnsafe.putBoolean(null, pointers[i], !modelBoolean[i]);
				checkTrueAt(
						myUnsafe.weakCompareAndSetBooleanRelease(null, pointers[i], !modelBoolean[i], modelBoolean[i]));
			} else if (method.equals(COMPAREANDEXCH)) {
				myUnsafe.putBoolean(null, pointers[i], !modelBoolean[i]);
				checkSameAt(!modelBoolean[i],
						myUnsafe.compareAndExchangeBoolean(null, pointers[i], !modelBoolean[i], modelBoolean[i]));
			} else if (method.equals(COMPAREANDEXCHA)) {
				myUnsafe.putBoolean(null, pointers[i], !modelBoolean[i]);
				checkSameAt(!modelBoolean[i], myUnsafe.compareAndExchangeBooleanAcquire(null, pointers[i],
						!modelBoolean[i], modelBoolean[i]));
			} else if (method.equals(COMPAREANDEXCHR)) {
				myUnsafe.putBoolean(null, pointers[i], !modelBoolean[i]);
				checkSameAt(!modelBoolean[i], myUnsafe.compareAndExchangeBooleanRelease(null, pointers[i],
						!modelBoolean[i], modelBoolean[i]));

			}
			booleanCheck(modelBoolean, i, pointers, method);
		}
	}

	protected void testObjectNative(String method) throws Exception {
		mem = memAllocate(100);
		long pointers[] = new long[models.length];
		for (int i = 0; i < models.length; i++) {
			pointers[i] = mem + (i * ObjectData.getSize(i));
			getLogger()
					.debug("testObjectNative Method: " + method + "- Pointer: " + pointers[i] + ", Data: " + models[i]);
			if (method.equals(VOLATILE)) {
				myUnsafe.putObjectVolatile(null, pointers[i], models[i]);

			} else if (method.equals(OPAQUE)) {
				myUnsafe.putObjectOpaque(null, pointers[i], models[i]);

			} else if (method.equals(ORDERED)) {
				myUnsafe.putObjectRelease(null, pointers[i], models[i]);

			} else if (method.equals(COMPAREANDSET)) {
				myUnsafe.putObject(null, pointers[i], compareValueObject);
				checkTrueAt(myUnsafe.compareAndSetObject(null, pointers[i], compareValueObject, models[i]));
			} else if (method.equals(WCOMPAREANDSET)) {
				myUnsafe.putObject(null, pointers[i], compareValueObject);
				AssertJUnit
						.assertTrue(myUnsafe.weakCompareAndSetObject(null, pointers[i], compareValueObject, models[i]));
			} else if (method.equals(WCOMPAREANDSETA)) {
				myUnsafe.putObject(null, pointers[i], compareValueObject);
				checkTrueAt(myUnsafe.weakCompareAndSetObjectAcquire(null, pointers[i], compareValueObject, models[i]));
			} else if (method.equals(WCOMPAREANDSETR)) {
				myUnsafe.putObject(null, pointers[i], compareValueObject);
				checkTrueAt(myUnsafe.weakCompareAndSetObjectRelease(null, pointers[i], compareValueObject, models[i]));
			} else if (method.equals(WCOMPAREANDSETP)) {
				myUnsafe.putObject(null, pointers[i], compareValueObject);
				checkTrueAt(myUnsafe.weakCompareAndSetObjectPlain(null, pointers[i], compareValueObject, models[i]));
			} else if (method.equals(COMPAREANDEXCH)) {
				myUnsafe.putObject(null, pointers[i], compareValueObject);
				checkSameAt(compareValueObject,
						myUnsafe.compareAndExchangeObject(null, pointers[i], compareValueObject, models[i]));
			} else if (method.equals(COMPAREANDEXCHA)) {
				myUnsafe.putObject(null, pointers[i], compareValueObject);
				checkSameAt(compareValueObject,
						myUnsafe.compareAndExchangeObjectAcquire(null, pointers[i], compareValueObject, models[i]));
			} else if (method.equals(COMPAREANDEXCHR)) {
				myUnsafe.putObject(null, pointers[i], compareValueObject);
				checkSameAt(compareValueObject,
						myUnsafe.compareAndExchangeObjectRelease(null, pointers[i], compareValueObject, models[i]));

			} else {
				myUnsafe.putObject(null, pointers[i], models[i]);
			}
			objectCheck(models, i, pointers, method);
		}
	}

	private void byteCheck(Object model, int limit, long[] pointers, String method) throws Exception {
		for (int i = 0; i <= limit; i++) {
			getLogger().debug(" pointers[" + i +"]: " + pointers[i]);
			byte expected = (Byte) Array.get(model, i);

			if (method.equals(VOLATILE)) {
				byte value = myUnsafe.getByteVolatile(null, pointers[i]);
				getLogger().debug("getByteVolatile(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (method.equals(OPAQUE)) {
				byte value = myUnsafe.getByteOpaque(null, pointers[i]);
				getLogger().debug("getByteOpaque(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (method.equals(ORDERED)) {
				byte value = myUnsafe.getByteAcquire(null, pointers[i]);
				getLogger().debug("getByteAcquire(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else {
				byte value = myUnsafe.getByte(null, pointers[i]);
				getLogger().debug("getByte(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

				value = myUnsafe.getByte(pointers[i]);
				getLogger().debug("getByte(long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);
			}
		}
	}

	private void charCheck(Object model, int limit, long[] pointers, String method) throws Exception {
		for (int i = 0; i <= limit; i++) {
			getLogger().debug(" pointers[" + i +"]: " + pointers[i]);
			char expected = (Character) Array.get(model, i);

			if (method.equals(VOLATILE)) {
				char value = myUnsafe.getCharVolatile(null, pointers[i]);
				getLogger().debug("getCharVolatile(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (method.equals(OPAQUE)) {
				char value = myUnsafe.getCharOpaque(null, pointers[i]);
				getLogger().debug("getCharOpaque(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (method.equals(ORDERED)) {
				char value = myUnsafe.getCharAcquire(null, pointers[i]);
				getLogger().debug("getCharAcquire(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else {
				char value = myUnsafe.getChar(null, pointers[i]);
				getLogger().debug("getChar(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

				value = myUnsafe.getChar(pointers[i]);
				getLogger().debug("getChar(long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);
			}
		}
	}

	private void shortCheck(Object model, int limit, long[] pointers, String method) throws Exception {
		for (int i = 0; i <= limit; i++) {
			getLogger().debug(" pointers[" + i +"]: " + pointers[i]);
			short expected = (Short) Array.get(model, i);

			if (method.equals(VOLATILE)) {
				short value = myUnsafe.getShortVolatile(null, pointers[i]);
				getLogger().debug("getShortVolatile(Object, long)- Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (method.equals(OPAQUE)) {
				short value = myUnsafe.getShortOpaque(null, pointers[i]);
				getLogger().debug("getShortOpaque(Object, long)- Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (method.equals(ORDERED)) {
				short value = myUnsafe.getShortAcquire(null, pointers[i]);
				getLogger().debug("getShortAcquire(Object, long)- Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else {
				short value = myUnsafe.getShort(null, pointers[i]);
				getLogger().debug("getShort(Object, long)- Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

				value = myUnsafe.getShort(pointers[i]);
				getLogger().debug("getShort(long)-  Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);
			}
		}
	}

	private void intCheck(Object model, int limit, long[] pointers, String method) throws Exception {
		for (int i = 0; i <= limit; i++) {
			getLogger().debug(" pointers[" + i +"]: " + pointers[i]);
			int expected = (Integer) Array.get(model, i);

			if (method.equals(ADDRESS)) {
				int value = (int) myUnsafe.getAddress(pointers[i]);
				getLogger().debug("getAddress(long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (method.equals(VOLATILE)) {
				int value = myUnsafe.getIntVolatile(null, pointers[i]);
				getLogger().debug("getIntVolatile(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (method.equals(OPAQUE)) {
				int value = myUnsafe.getIntOpaque(null, pointers[i]);
				getLogger().debug("getIntOpaque(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (method.equals(ORDERED)) {
				int value = myUnsafe.getIntAcquire(null, pointers[i]);
				getLogger().debug("getIntAcquire(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else {
				int value = myUnsafe.getInt(null, pointers[i]);
				getLogger().debug("getInt(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

				value = myUnsafe.getInt(pointers[i]);
				getLogger().debug("getInt(long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);
			}
		}
	}

	private void longCheck(Object model, int limit, long[] pointers, String method) throws Exception {
		for (int i = 0; i <= limit; i++) {
			getLogger().debug(" pointers[" + i +"]: " + pointers[i]);
			long expected = (Long) Array.get(model, i);

			if (method.equals(ADDRESS)) {
				long value = myUnsafe.getAddress(pointers[i]);
				getLogger().debug("getAddress(long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (method.equals(VOLATILE)) {
				long value = myUnsafe.getLongVolatile(null, pointers[i]);
				getLogger().debug("getLongVolatile(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (method.equals(OPAQUE)) {
				long value = myUnsafe.getLongOpaque(null, pointers[i]);
				getLogger().debug("getLongOpaque(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (method.equals(ORDERED)) {
				long value = myUnsafe.getLongAcquire(null, pointers[i]);
				getLogger().debug("getLongAcquire(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else {
				long value = myUnsafe.getLong(null, pointers[i]);
				getLogger().debug("getLong(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

				value = myUnsafe.getLong(pointers[i]);
				getLogger().debug("getLong(long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);
			}
		}
	}

	private void floatCheck(Object model, int limit, long[] pointers, String method) throws Exception {
		for (int i = 0; i <= limit; i++) {
			getLogger().debug(" pointers[" + i +"]: " + pointers[i]);
			float expected = (Float) Array.get(model, i);

			if (method.equals(VOLATILE)) {
				float value = myUnsafe.getFloatVolatile(null, pointers[i]);
				getLogger().debug("getFloatVolatile(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (method.equals(OPAQUE)) {
				float value = myUnsafe.getFloatOpaque(null, pointers[i]);
				getLogger().debug("getFloatOpaque(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (model.equals(ORDERED)) {
				float value = myUnsafe.getFloatAcquire(null, pointers[i]);
				getLogger().debug("getFloatAcquire(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else {
				float value = myUnsafe.getFloat(null, pointers[i]);
				getLogger().debug("getFloat(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

				value = myUnsafe.getFloat(pointers[i]);
				getLogger().debug("getFloat(long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);
			}
		}
	}

	private void doubleCheck(Object model, int limit, long[] pointers, String method) throws Exception {
		for (int i = 0; i <= limit; i++) {
			getLogger().debug(" pointers[" + i +"]: " + pointers[i]);
			double expected = (Double) Array.get(model, i);

			if (model.equals(VOLATILE)) {
				double value = myUnsafe.getDoubleVolatile(null, pointers[i]);
				getLogger().debug("getDoubleVolatile(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (model.equals(OPAQUE)) {
				double value = myUnsafe.getDoubleOpaque(null, pointers[i]);
				getLogger().debug("getDoubleOpaque(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (model.equals(ORDERED)) {
				double value = myUnsafe.getDoubleAcquire(null, pointers[i]);
				getLogger().debug("getDoubleAcquire(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else {
				double value = myUnsafe.getDouble(null, pointers[i]);
				getLogger().debug("getDouble(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

				value = myUnsafe.getDouble(pointers[i]);
				getLogger().debug("getDouble(Object, long) - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);
			}
		}
	}

	private void booleanCheck(Object model, int limit, long[] pointers, String method) throws Exception {
		for (int i = 0; i <= limit; i++) {
			getLogger().debug(" pointers[" + i +"]: " + pointers[i]);
			boolean expected = (Boolean) Array.get(model, i);

			if (method.equals(VOLATILE)) {
				boolean value = myUnsafe.getBooleanVolatile(null, pointers[i]);
				getLogger().debug("getBooleanVolatile - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (method.equals(OPAQUE)) {
				boolean value = myUnsafe.getBooleanOpaque(null, pointers[i]);
				getLogger().debug("getBooleanOpaque - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (model.equals(ORDERED)) {
				boolean value = myUnsafe.getBooleanAcquire(null, pointers[i]);
				getLogger().debug("getBooleanAcquire - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else {
				boolean value = myUnsafe.getBoolean(null, pointers[i]);
				getLogger().debug("getBoolean - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);
			}
		}
	}

	private void objectCheck(Object model, int limit, long[] pointers, String method) throws Exception {
		for (int i = 0; i <= limit; i++) {
			getLogger().debug(" pointers[" + i + "]: " + pointers[i]);
			Object expected = (Object) Array.get(model, i);

			if (method.equals(VOLATILE)) {
				Object value = myUnsafe.getObjectVolatile(null, pointers[i]);
				getLogger().debug("getObjectVolatile - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (method.equals(OPAQUE)) {
				Object value = myUnsafe.getObjectOpaque(null, pointers[i]);
				getLogger().debug("getObjectOpaque - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else if (model.equals(ORDERED)) {
				Object value = myUnsafe.getObjectAcquire(null, pointers[i]);
				getLogger().debug("getObjectAcquire - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);

			} else {
				Object value = myUnsafe.getObject(null, pointers[i]);
				getLogger().debug("getObject - Expected: " + expected + ", Actual: " + value);
				checkSameAt(expected, value);
			}
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

	protected void alignment() {
		int addressSize = myUnsafe.addressSize();
		getLogger().debug("addressSize: " + addressSize);
		long mod = 0;
		if (addressSize > 4) {
			mod = mem % 8;
			getLogger().debug("memory mod 8: " + mod);
		} else {
			mod = mem % 4;
			getLogger().debug("memory mod 4: " +  mod);
		}
		if (mod != 0) {
			mem = mem + mod;
			getLogger().debug("Change pointer to: " + mem);
		}
	}
	
	/* Create a class with the specified package name.
	 * This method is used to verify the correctness of 
	 * jdk.internal.misc.Unsafe.defineAnonymousClass.
	 */
	protected static byte[] createDummyClass(String packageName) {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv = null;
		String className = "DummyClass";
		
		if (packageName != null) {
			className = packageName + "/" + className;
		}
		
		cw.visit(52, ACC_PUBLIC, className, null, "java/lang/Object", null);
		
		{
			mv = cw.visitMethod(ACC_PUBLIC, "bar", "()V", null, null);
			mv.visitCode();
			mv.visitInsn(RETURN);
			mv.visitMaxs(2, 1);
			mv.visitEnd();
		}

		cw.visitEnd();
		
		return cw.toByteArray();
	}

	protected char generateUnalignedChar(Object object, long uOffset) {
		return (char) generateUnalignedShort(object, uOffset);
	}

	protected short generateUnalignedShort(Object object, long uOffset) {
		int first = (int) myUnsafe.getByte(object, uOffset) & BYTE_MASK;
		int second = (int) myUnsafe.getByte(object, 1 + uOffset) & BYTE_MASK;

		if (myUnsafe.isBigEndian()) {
			return (short) (second | (first << 8));
		} else {
			return (short) (first | (second << 8));
		}
	}

	protected int generateUnalignedInt(Object object, long uOffset) {
		int first = (int) myUnsafe.getByte(object, uOffset) & BYTE_MASK;
		int second = (int) myUnsafe.getByte(object, 1 + uOffset) & BYTE_MASK;
		int third = (int) myUnsafe.getByte(object, 2 + uOffset) & BYTE_MASK;
		int fourth = (int) myUnsafe.getByte(object, 3 + uOffset) & BYTE_MASK;

		if (myUnsafe.isBigEndian()) {
			return fourth | (third << 8) | (second << 16) | (first << 24);
		} else {
			return first | (second << 8) | (third << 16) | (fourth << 24);
		}
	}

	protected long generateUnalignedLong(Object object, long uOffset) {

		long first = (long) myUnsafe.getByte(object, uOffset) & BYTE_MASKL;
		long second = (long) myUnsafe.getByte(object, 1 + uOffset) & BYTE_MASKL;
		long third = (long) myUnsafe.getByte(object, 2 + uOffset) & BYTE_MASKL;
		long fourth = (long) myUnsafe.getByte(object, 3 + uOffset) & BYTE_MASKL;
		long fifth = (long) myUnsafe.getByte(object, 4 + uOffset) & BYTE_MASKL;
		long sixth = (long) myUnsafe.getByte(object, 5 + uOffset) & BYTE_MASKL;
		long seventh = (long) myUnsafe.getByte(object, 6 + uOffset) & BYTE_MASKL;
		long eighth = (long) myUnsafe.getByte(object, 7 + uOffset) & BYTE_MASKL;

		if (myUnsafe.isBigEndian()) {
			return eighth | (seventh << 8) | (sixth << 16) | (fifth << 24) | (fourth << 32) | (third << 40)
					| (second << 48) | (first << 56);
		} else {
			return first | (second << 8) | (third << 16) | (fourth << 24) | (fifth << 32) | (sixth << 40)
					| (seventh << 48) | (eighth << 56);
		}
	}
}
