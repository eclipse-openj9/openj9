/*******************************************************************************
 * Copyright (c) 2006, 2018 IBM Corp. and others
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

package jit.test.vich;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import jit.test.vich.utils.Timer;

public class JNICallIn {

	private static Logger logger = Logger.getLogger(JNICallIn.class);
	Timer timer;

	public JNICallIn() {
		timer = new Timer ();
	}

	public static class JNIFailure extends RuntimeException {
		public JNIFailure(){
			super();
		}
		public JNIFailure(String p1){
			super(p1);
		}
	}

	static {
		try {
			System.loadLibrary("j9ben");
		} catch (UnsatisfiedLinkError e) {}
	}
	static final int loopCount = 100000;

public native void callInVirtualVoid(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualByte(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualBoolean(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualShort(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualChar(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualInt(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualLong(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualFloat(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualDouble(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualObject(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualVoidA(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualByteA(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualBooleanA(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualShortA(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualCharA(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualIntA(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualLongA(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualFloatA(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualDoubleA(int loopCount, Object[] objArray, int[] intArray);
public native void callInVirtualObjectA(int loopCount, Object[] objArray, int[] intArray);

public void virtualVoid(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
}
public boolean virtualBoolean(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return z;
}
public byte virtualByte(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return b;
}
public short virtualShort(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return s;
}
public char virtualChar(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return c;
}
public int virtualInt(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return i;
}
public long virtualLong(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return j;
}
public float virtualFloat(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return f;
}
public double virtualDouble(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return d;
}
public Object virtualObject(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return obj;
}

public native void callInNonvirtualVoid(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualByte(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualBoolean(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualShort(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualChar(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualInt(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualLong(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualFloat(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualDouble(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualObject(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualVoidA(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualByteA(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualBooleanA(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualShortA(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualCharA(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualIntA(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualLongA(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualFloatA(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualDoubleA(int loopCount, Object[] objArray, int[] intArray);
public native void callInNonvirtualObjectA(int loopCount, Object[] objArray, int[] intArray);

private void nonvirtualVoid(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
}
private byte nonvirtualByte(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return b;
}
private boolean nonvirtualBoolean(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return z;
}
private short nonvirtualShort(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return s;
}
private char nonvirtualChar(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return c;
}
private int nonvirtualInt(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return i;
}
private long nonvirtualLong(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return j;
}
private float nonvirtualFloat(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return f;
}
private double nonvirtualDouble(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return d;
}
private Object nonvirtualObject(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return obj;
}

public native void callInStaticVoid(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticBoolean(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticByte(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticShort(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticChar(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticInt(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticLong(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticFloat(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticDouble(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticObject(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticVoidA(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticBooleanA(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticByteA(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticShortA(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticCharA(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticIntA(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticLongA(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticFloatA(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticDoubleA(int loopCount, Object[] objArray, int[] intArray);
public native void callInStaticObjectA(int loopCount, Object[] objArray, int[] intArray);

public static void staticVoid(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
}
public static byte staticByte(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return b;
}
public static boolean staticBoolean(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return z;
}
public static short staticShort(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return s;
}
public static char staticChar(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return c;
}
public static int staticInt(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return i;
}
public static long staticLong(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return j;
}
public static float staticFloat(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return f;
}
public static double staticDouble(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return d;
}
public static Object staticObject(Object obj, Object nil, int[] array, Object[] objArray, boolean z, byte b, char c, short s, int i, long j, double d, float f) {
	return obj;
}

@Test(groups = { "level.sanity","component.jit" })
public void testJNICallIn() {
	int i;
	try {
		System.loadLibrary("j9ben");
		callInVirtualVoid(0, null, null);
		callInVirtualBoolean(0, null, null);
		callInVirtualByte(0, null, null);
		callInVirtualShort(0, null, null);
		callInVirtualChar(0, null, null);
		callInVirtualInt(0, null, null);
		callInVirtualLong(0, null, null);
		callInVirtualFloat(0, null, null);
		callInVirtualDouble(0, null, null);
		callInVirtualObject(0, null, null);
		callInVirtualVoidA(0, null, null);
		callInVirtualBooleanA(0, null, null);
		callInVirtualByteA(0, null, null);
		callInVirtualShortA(0, null, null);
		callInVirtualCharA(0, null, null);
		callInVirtualIntA(0, null, null);
		callInVirtualLongA(0, null, null);
		callInVirtualFloatA(0, null, null);
		callInVirtualDoubleA(0, null, null);
		callInVirtualObjectA(0, null, null);
		callInNonvirtualVoid(0, null, null);
		callInNonvirtualByte(0, null, null);
		callInNonvirtualBoolean(0, null, null);
		callInNonvirtualShort(0, null, null);
		callInNonvirtualChar(0, null, null);
		callInNonvirtualInt(0, null, null);
		callInNonvirtualLong(0, null, null);
		callInNonvirtualFloat(0, null, null);
		callInNonvirtualDouble(0, null, null);
		callInNonvirtualObject(0, null, null);
		callInNonvirtualVoidA(0, null, null);
		callInNonvirtualByteA(0, null, null);
		callInNonvirtualBooleanA(0, null, null);
		callInNonvirtualShortA(0, null, null);
		callInNonvirtualCharA(0, null, null);
		callInNonvirtualIntA(0, null, null);
		callInNonvirtualLongA(0, null, null);
		callInNonvirtualFloatA(0, null, null);
		callInNonvirtualDoubleA(0, null, null);
		callInNonvirtualObjectA(0, null, null);
		callInStaticVoid(0, null, null);
		callInStaticByte(0, null, null);
		callInStaticBoolean(0, null, null);
		callInStaticShort(0, null, null);
		callInStaticChar(0, null, null);
		callInStaticInt(0, null, null);
		callInStaticLong(0, null, null);
		callInStaticFloat(0, null, null);
		callInStaticDouble(0, null, null);
		callInStaticObject(0, null, null);
		callInStaticVoidA(0, null, null);
		callInStaticByteA(0, null, null);
		callInStaticBooleanA(0, null, null);
		callInStaticShortA(0, null, null);
		callInStaticCharA(0, null, null);
		callInStaticIntA(0, null, null);
		callInStaticLongA(0, null, null);
		callInStaticFloatA(0, null, null);
		callInStaticDoubleA(0, null, null);
		callInStaticObjectA(0, null, null);
	} catch (UnsatisfiedLinkError e) {
		Assert.fail("No natives for JNI tests");
	}
		
	Object[] objArray = new Object[10];
	int[] intArray = new int[10];
	
/*
#(Virtual Nonvirtual Static) do: [:x |
#(Void Byte Boolean Short Char Int Long Double Float Object) do: [:y |
#('' 'A') do: [:z |
Transcript show: ('
	timer.reset();
	callIn%1%2%3(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callIn%1%2%3(" + loopCount + "); = "+ Long.toString(timer.delta()));
' bindWith: x with: y with: z) ] ] ]
*/
	
	timer.reset();
	callInVirtualVoid(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualVoid(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualVoidA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualVoidA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualByte(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualByte(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualByteA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualByteA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualBoolean(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualBoolean(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualBooleanA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualBooleanA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualShort(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualShort(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualShortA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualShortA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualChar(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualChar(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualCharA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualCharA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualInt(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualInt(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualIntA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualIntA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualLong(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualLong(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualLongA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualLongA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualDouble(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualDouble(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualDoubleA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualDoubleA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualFloat(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualFloat(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualFloatA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualFloatA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualObject(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualObject(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInVirtualObjectA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInVirtualObjectA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualVoid(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualVoid(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualVoidA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualVoidA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualByte(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualByte(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualByteA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualByteA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualBoolean(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualBoolean(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualBooleanA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualBooleanA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualShort(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualShort(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualShortA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualShortA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualChar(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualChar(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualCharA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualCharA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualInt(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualInt(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualIntA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualIntA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualLong(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualLong(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualLongA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualLongA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualDouble(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualDouble(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualDoubleA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualDoubleA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualFloat(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualFloat(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualFloatA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualFloatA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualObject(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualObject(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInNonvirtualObjectA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInNonvirtualObjectA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticVoid(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticVoid(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticVoidA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticVoidA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticByte(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticByte(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticByteA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticByteA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticBoolean(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticBoolean(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticBooleanA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticBooleanA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticShort(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticShort(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticShortA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticShortA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticChar(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticChar(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticCharA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticCharA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticInt(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticInt(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticIntA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticIntA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticLong(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticLong(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticLongA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticLongA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticDouble(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticDouble(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticDoubleA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticDoubleA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticFloat(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticFloat(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticFloatA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticFloatA(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticObject(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticObject(" + loopCount + "); = "+ Long.toString(timer.delta()));

	timer.reset();
	callInStaticObjectA(loopCount, objArray, intArray);
	timer.mark();
	logger.info("callInStaticObjectA(" + loopCount + "); = "+ Long.toString(timer.delta()));

}


}
