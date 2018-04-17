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
package j9vm.test.unsafe;

import java.lang.reflect.Field;
import java.lang.reflect.Array;

import sun.misc.Unsafe;

public class UnsafeAccessorTest {
	static Unsafe myUnsafe;

	/* use reflection to bypass the security manager */
	private static Unsafe getUnsafeInstance() throws IllegalAccessException {
		/* the instance is likely stored in a static field of type Unsafe */
		Field[] staticFields = Unsafe.class.getDeclaredFields();
		for (Field field : staticFields) {
			if (field.getType() == Unsafe.class) {
		 		field.setAccessible(true);
		 		return (Unsafe)field.get(Unsafe.class);
			}
		}
		throw new Error("Unable to find an instance of Unsafe");
	}

	private static final byte[] modelByte = new byte[] {1, 2, 3, 4, -1, -2, -3, -4, 0};
	private static final char[] modelChar = new char[] {1, 2, Character.MAX_VALUE, Character.MAX_VALUE - 1, 0};
	private static final short[] modelShort = new short[] {1, 2, -1, -2, 0};
	private static final int[] modelInt = new int[] {1, 2, 0};
	private static final long[] modelLong = new long[] {1, 0};
	private static final float[] modelFloat = new float[] {1.0f, 2.0f, 0.0f};
	private static final double[] modelDouble = new double[] {1.0, 0.0};
	private static final Object[] models = new Object[] {modelByte, modelChar, modelShort, modelInt, modelLong, modelFloat, modelDouble};

	static abstract class Data {
		abstract void init();
		Object get(int i) { throw new Error("Invalid field index " + i); }
		Object getStatic(int i) { throw new Error("Invalid static field index " + i); }
	}

	static class ByteData extends Data {
		byte f0, f1, f2, f3, f4, f5, f6, f7, f8;
		static byte sf0, sf1, sf2, sf3, sf4, sf5, sf6, sf7, sf8;
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
		}
		Object get(int i) { return new byte[] {f0, f1, f2, f3, f4, f5, f6, f7, f8}[i]; }
		Object getStatic(int i) { return new byte[] {sf0, sf1, sf2, sf3, sf4, sf5, sf6, sf7, sf8}[i]; }
	}

	static class CharData extends Data {
		char f0, f1, f2, f3, f4;
		static char sf0, sf1, sf2, sf3, sf4;
		void init() {
			f0 = sf0 = modelChar[0];
			f1 = sf1 = modelChar[1];
			f2 = sf2 = modelChar[2];
			f3 = sf3 = modelChar[3];
			f4 = sf4 = modelChar[4];
		}
		Object get(int i) { return new char[] {f0, f1, f2, f3, f4}[i]; }
		Object getStatic(int i) { return new char[] {sf0, sf1, sf2, sf3, sf4}[i]; }
	}

	static class ShortData extends Data {
		short f0, f1, f2, f3, f4;
		static short sf0, sf1, sf2, sf3, sf4;
		void init() {
			f0 = sf0 = modelShort[0];
			f1 = sf1 = modelShort[1];
			f2 = sf2 = modelShort[2];
			f3 = sf3 = modelShort[3];
			f4 = sf4 = modelShort[4];
		}
		Object get(int i) { return new short[] {f0, f1, f2, f3, f4}[i]; }
		Object getStatic(int i) { return new short[] {sf0, sf1, sf2, sf3, sf4}[i]; }
	}

	static class IntData extends Data {
		int f0, f1, f2;
		static int sf0, sf1, sf2;
		void init() {
			f0 = sf0 = modelInt[0];
			f1 = sf1 = modelInt[1];
			f2 = sf2 = modelInt[2];
		}
		Object get(int i) { return new int[] {f0, f1, f2}[i]; }
		Object getStatic(int i) { return new int[] {sf0, sf1, sf2}[i]; }
	}

	static class LongData extends Data {
		long f0, f1;
		static long sf0, sf1;
		void init() {
			f0 = sf0 = modelLong[0];
			f1 = sf1 = modelLong[1];
		}
		Object get(int i) { return new long[] {f0, f1}[i]; }
		Object getStatic(int i) { return new long[] {sf0, sf1}[i]; }
	}

	static class FloatData extends Data {
		float f0, f1, f2;
		static float sf0, sf1, sf2;
		void init() {
			f0 = sf0 = modelFloat[0];
			f1 = sf1 = modelFloat[1];
			f2 = sf2 = modelFloat[2];
		}
		Object get(int i) { return new float[] {f0, f1, f2}[i]; }
		Object getStatic(int i) { return new float[] {sf0, sf1, sf2}[i]; }
	}

	static class DoubleData extends Data {
		double f0, f1;
		static double sf0, sf1;
		void init() {
			f0 = sf0 = modelDouble[0];
			f1 = sf1 = modelDouble[1];
		}
		Object get(int i) { return new double[] {f0, f1}[i]; }
		Object getStatic(int i) { return new double[] {sf0, sf1}[i]; }
	}

	private long offset(Object object, int index) throws NoSuchFieldException {
		if (object instanceof Class) return myUnsafe.staticFieldOffset(((Class) object).getDeclaredField("sf" + index));
		if (object instanceof Data) return myUnsafe.objectFieldOffset(object.getClass().getDeclaredField("f" + index));
		if (object.getClass().isArray()) return myUnsafe.arrayBaseOffset(object.getClass()) + (myUnsafe.arrayIndexScale(object.getClass()) * index);
		throw new Error("Unable to get offset " + index + " from unknown object type " + object.getClass());
	}
	
	private Object base(Object object, int index) throws NoSuchFieldException {
		if (object instanceof Class) return myUnsafe.staticFieldBase(((Class) object).getDeclaredField("sf" + index));
		return object;
	}

	private static void init(Object object) throws InstantiationException, IllegalAccessException {
		if (object instanceof Class) ((Data) ((Class) object).newInstance()).init();
		if (object instanceof Data) ((Data) object).init();
		if (object.getClass().isArray()) {
			for (Object model : models) {
				if (model.getClass().equals(object.getClass())) {
					System.arraycopy(model, 0, object, 0, Array.getLength(model));
				}
			}
		}
	}

	private static Object slot(Object object, int index) throws InstantiationException, IllegalAccessException {
		if (object instanceof Class) return ((Data) ((Class) object).newInstance()).getStatic(index);
		if (object instanceof Data) return ((Data) object).get(index);
		if (object.getClass().isArray()) return Array.get(object, index);
		throw new Error("Can't get slot " + index + " of unknown object type");
	}

	private static void modelCheck(Object model, Object object, int limit) throws Exception {
		for (int i = 0; i <= limit; i++) {
			if (!Array.get(model, i).equals(slot(object, i))) {
				throw new Error("Expecting to find (" + model.getClass().getComponentType() + ") " + Array.get(model, i) + " at index " + i);
			}
		}
		if (!Array.get(model, Array.getLength(model) - 1).equals(slot(object, limit + 1))) {
			throw new Error("Expecting to find (" + model.getClass().getComponentType() + ") 0 at index " + (limit + 1));
		}
	}
	
	private static void checkSameAt(int i, Object target, Object value) throws InstantiationException, IllegalAccessException {
		if (!slot(target, i).equals(value)) {
			throw new Error("Expecting to find (" + value.getClass() + ") " + slot(target, i) + " at index " + i);
		}
	}
	
	private void testPutByte(Object target) throws Exception {
		for (int i = 0; i < modelByte.length - 1; i++) {
			myUnsafe.putByte(base(target, i), offset(target, i), modelByte[i]);
			modelCheck(modelByte, target, i);
		}
	}
	
	private void testPutChar(Object target) throws Exception {
		for (int i = 0; i < modelChar.length - 1; i++) {
			myUnsafe.putChar(base(target, i), offset(target, i), modelChar[i]);
			modelCheck(modelChar, target, i);
		}
	}
	
	private void testPutShort(Object target) throws Exception {
		for (int i = 0; i < modelShort.length - 1; i++) {
			myUnsafe.putShort(base(target, i), offset(target, i), modelShort[i]);
			modelCheck(modelShort, target, i);
		}
	}
	
	private void testPutInt(Object target) throws Exception {
		for (int i = 0; i < modelInt.length - 1; i++) {
			myUnsafe.putInt(base(target, i), offset(target, i), modelInt[i]);
			modelCheck(modelInt, target, i);
		}
	}
	
	private void testPutLong(Object target) throws Exception {
		for (int i = 0; i < modelLong.length - 1; i++) {
			myUnsafe.putLong(base(target, i), offset(target, i), modelLong[i]);
			modelCheck(modelLong, target, i);
		}
	}
	
	private void testPutFloat(Object target) throws Exception {
		for (int i = 0; i < modelFloat.length - 1; i++) {
			myUnsafe.putFloat(base(target, i), offset(target, i), modelFloat[i]);
			modelCheck(modelFloat, target, i);
		}
	}
	
	private void testPutDouble(Object target) throws Exception {
		for (int i = 0; i < modelDouble.length - 1; i++) {
			myUnsafe.putDouble(base(target, i), offset(target, i), modelDouble[i]);
			modelCheck(modelDouble, target, i);
		}
	}
	
	private void testGetByte(Object target) throws Exception {
		init(target);
		for (int i = 0; i < modelByte.length; i++) {
			checkSameAt(i, target, myUnsafe.getByte(base(target, i), offset(target, i)));
		}
	}
	
	private void testGetChar(Object target) throws Exception {
		init(target);
		for (int i = 0; i < modelChar.length; i++) {
			checkSameAt(i, target, myUnsafe.getChar(base(target, i), offset(target, i)));
		}
	}
	
	private void testGetShort(Object target) throws Exception {
		init(target);
		for (int i = 0; i < modelShort.length; i++) {
			checkSameAt(i, target, myUnsafe.getShort(base(target, i), offset(target, i)));
		}
	}
	
	private void testGetInt(Object target) throws Exception {
		init(target);
		for (int i = 0; i < modelInt.length; i++) {
			checkSameAt(i, target, myUnsafe.getInt(base(target, i), offset(target, i)));
		}
	}
	
	private void testGetLong(Object target) throws Exception {
		init(target);
		for (int i = 0; i < modelLong.length; i++) {
			checkSameAt(i, target, myUnsafe.getLong(base(target, i), offset(target, i)));
		}
	}
	
	private void testGetFloat(Object target) throws Exception {
		init(target);
		for (int i = 0; i < modelFloat.length; i++) {
			checkSameAt(i, target, myUnsafe.getFloat(base(target, i), offset(target, i)));
		}
	}
	
	private void testGetDouble(Object target) throws Exception {
		init(target);
		for (int i = 0; i < modelDouble.length; i++) {
			checkSameAt(i, target, myUnsafe.getDouble(base(target, i), offset(target, i)));
		}
	}
	
	private void testArrayPut() throws Exception {
		testPutByte(new byte[modelByte.length]);
		testPutChar(new char[modelChar.length]);
		testPutShort(new short[modelShort.length]);
		testPutInt(new int[modelInt.length]);
		testPutLong(new long[modelLong.length]);
		testPutFloat(new float[modelFloat.length]);
		testPutDouble(new double[modelDouble.length]);
	}
	
	private void testInstancePut() throws Exception {
		testPutByte(new ByteData());
		testPutChar(new CharData());
		testPutShort(new ShortData());
		testPutInt(new IntData());
		testPutLong(new LongData());
		testPutFloat(new FloatData());
		testPutDouble(new DoubleData());
	}
	
	private void testStaticPut() throws Exception {
		testPutByte(ByteData.class);
		testPutChar(CharData.class);
		testPutShort(ShortData.class);
		testPutInt(IntData.class);
		testPutLong(LongData.class);
		testPutFloat(FloatData.class);
		testPutDouble(DoubleData.class);
	}

	private void testArrayGet() throws Exception {
		testGetByte(new byte[modelByte.length]);
		testGetChar(new char[modelChar.length]);
		testGetShort(new short[modelShort.length]);
		testGetInt(new int[modelInt.length]);
		testGetLong(new long[modelLong.length]);
		testGetFloat(new float[modelFloat.length]);
		testGetDouble(new double[modelDouble.length]);
	}
	
	private void testInstanceGet() throws Exception {
		testGetByte(new ByteData());
		testGetChar(new CharData());
		testGetShort(new ShortData());
		testGetInt(new IntData());
		testGetLong(new LongData());
		testGetFloat(new FloatData());
		testGetDouble(new DoubleData());
	}
	
	private void testStaticGet() throws Exception {
		testGetByte(ByteData.class);
		testGetChar(CharData.class);
		testGetShort(ShortData.class);
		testGetInt(IntData.class);
		testGetLong(LongData.class);
		testGetFloat(FloatData.class);
		testGetDouble(DoubleData.class);
	}

	/**
	 * @param args
	 */
	public static void main(String[] args) throws Exception {
		myUnsafe = getUnsafeInstance();
		UnsafeAccessorTest tester = new UnsafeAccessorTest();
		tester.testArrayPut();
		tester.testInstancePut();
		tester.testStaticPut();
		tester.testArrayGet();
		tester.testInstanceGet();
		tester.testStaticGet();
	}
}
