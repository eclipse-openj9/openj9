package com.ibm.j9.offload.tests.jniservice;

/*******************************************************************************
 * Copyright (c) 2008, 2012 IBM Corp. and others
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

public class TestNatives implements TestNativesInterface {
	static final String NATIVE_LIBRARY_NAME = "j9offjnitest26";
	static boolean libraryLoaded = false;
	
	/* non-static fields that natives look up value for */
	public TestNatives objectField;
	public boolean booleanField;
	public byte byteField;
	public char charField;
	public short shortField;
	public int intField;
	public long longField;
	public float floatField;
	public double doubleField;
	
	/* static fields that natives look up value for */
	static public TestNatives staticObjectField;
	static public boolean staticBooleanField;
	static public byte staticByteField;
	static public char staticCharField;
	static public short staticShortField;
	static public int staticIntField;
	static public long staticLongField;
	static public float staticFloatField;
	static public double staticDoubleField;
	
	/* fields used for sum value perf measurements */
	public int suma, sumb, sumc, sumd, sume, sumf;
	
	/* natives for testing */
	public native Object testAllocObject(Class clazz) throws InstantiationException;
	public native int testCallMethod(int value, JValue arg, JValue ret1, JValue ret2, JValue ret3, JValue ret4);
	public native int testCallMethodA(int value, JValue arg, JValue ret1, JValue ret2, JValue ret3, JValue ret4);
	public native int testCallMethodV(int value, JValue arg, JValue ret1, JValue ret2, JValue ret3, JValue ret4);
	public native int testCallNonvirtualMethod(int value, JValue arg, JValue ret1, JValue ret2, JValue ret3, JValue ret4);	
	public native int testCallNonvirtualMethodA(int value, JValue arg, JValue ret1, JValue ret2, JValue ret3, JValue ret4);
	public native int testCallNonvirtualMethodV(int value, JValue arg, JValue ret1, JValue ret2, JValue ret3, JValue ret4);
	public native int testCallStaticMethod(int value, JValue arg, JValue ret1, JValue ret2, JValue ret3, JValue ret4);
	public native int testCallStaticMethodA(int value, JValue arg, JValue ret1, JValue ret2, JValue ret3, JValue ret4);
	public native int testCallStaticMethodV(int value, JValue arg, JValue ret1, JValue ret2, JValue ret3, JValue ret4);
	public native Class testDefineClass(String name, Object loader, byte[] array);
	public native int testDeleteLocalRef(int value);
	public native int testDeleteGlobalRef(int value);
	public native int testDeleteWeakGlobalRef(int value);
	public native int testEnsureLocalCapacity(int value);
	public native boolean testExceptionCheck(Class clazz);
	public native boolean testExceptionClear(Class clazz) throws InstantiationException;
	public native void testExceptionDescribe();
	public native Throwable testExceptionOccurred(Class clazz);
	public native int testFatalError();
	public native Class testFindClass(int value);
	public native int testFromReflectedField(Object field, int fieldValue, int value);
	public native int testFromReflectedMethod(Object method, int isStatic, int value, int count, JValue arg, JValue ret1, JValue ret2, JValue ret3, JValue ret4);
	public native int testGetArrayLength(Object[] arr);
	public native int testGetArrayElements(int length, boolean[] z, byte[] b, char[] c, short[] s, int[] i, long[] j, float[] f, double[] d);
	public native int testGetArrayRegion(int start, int length, boolean[] z, byte[] b, char[] c, short[] s, int[] i, long[] j, float[] f, double[] d, int value);
	public native Object testGetObjectField();
	public native boolean testGetBooleanField();
	public native byte testGetByteField();
	public native char testGetCharField();
	public native short testGetShortField();
	public native int testGetIntField();
	public native long testGetLongField();
	public native float testGetFloatField();
	public native double testGetDoubleField();
	public native int testDirectBuffer();	
	public native int testGetFieldID(int value) throws NoSuchFieldError;
	public native int testGetMethodID(int value) throws NoSuchMethodError;
	public native Object testGetObjectArrayElement(Object[] arr, int index);
	public native Class testGetObjectClass();
	public native int testGetObjectRefType();		
	public native int testGetPrimitiveArrayCritical(int length, boolean[] arr_z, byte[] arr_b, char[] arr_c, short[] arr_s, int[] arr_i, long[] arr_j, float[] arr_f, double[] arr_d);
	public native int testGetStaticFieldID(int value) throws NoSuchFieldError;
	public native Object testGetStaticObjectField();
	public native boolean testGetStaticBooleanField();
	public native byte testGetStaticByteField();
	public native char testGetStaticCharField();
	public native short testGetStaticShortField();
	public native int testGetStaticIntField();
	public native long testGetStaticLongField();
	public native float testGetStaticFloatField();
	public native double testGetStaticDoubleField();
	public native int testGetStaticMethodID(int value) throws NoSuchMethodError;
	public native int testGetStringChars(String str, int length);
	public native int testGetStringCritical(String str, int length);	
	public native int testGetStringLength(String str);
	public native int testGetStringRegion(String str, int start, int length);
	public native int testGetStringUTFChars(String str, int length);
	public native int testGetStringUTFLength(String string);
	public native int testGetStringUTFRegion(String str, int start, int length);
	public native int testGetStringUTFRegionMultibyteChars(String str);
	public native Class testGetSuperclass(Class clazz);
	public native int testGetVersion();
	public native boolean testIsAssignableFrom(Class clazz1, Class clazz2);
	public native boolean testIsInstanceOf(Object obj, Class clazz);
	public native boolean testIsSameObject(Object ref1, Object ref2);
	public native int testMonitorEnter(Object ref);
	public native int testMonitorExit(Object ref);
	public native int testNewRef();
	public native Object testNewObject(int value);
	public native Object testNewObjectA(int value);
	public native Object testNewObjectV(int value);
	public native Object[] testNewObjectArray(int value, Class elemType, Object iniElem);
	public native boolean[] testNewBooleanArray(int value);	
	public native byte[] testNewByteArray(int value);
	public native char[] testNewCharArray(int value);	
	public native short[] testNewShortArray(int value);
	public native int[] testNewIntArray(int value);
	public native long[] testNewLongArray(int value);
	public native float[] testNewFloatArray(int value);
	public native double[] testNewDoubleArray(int value);
	public native String testNewString();
	public native String testNewStringUTF();
	public native int testPopLocalFrame(Object obj);
	public native int testPushLocalFrame(int capacity);	
	public native int testSetArrayRegion(int start, int length,
										 boolean[] arr1, byte[] arr2, char[] arr3, short[] arr4, int[] arr5, long[] arr6, float[] arr7, double[] arr8,
										 boolean z, byte b, char c, short s, int i, long j, float f, double d);
	public native int testSetObjectField(TestNatives value);
	public native int testSetBooleanField(boolean value);
	public native int testSetByteField(byte value);
	public native int testSetCharField(char value);
	public native int testSetShortField(short value);
	public native int testSetIntField(int value);
	public native int testSetLongField(long value);
	public native int testSetFloatField(float value);
	public native int testSetDoubleField(double value);	
	public native int testSetObjectArrayElement(Object[] arr, int index, Object theObect);
	public native int testSetStaticObjectField(TestNatives value);
	public native int testSetStaticBooleanField(boolean value);
	public native int testSetStaticByteField(byte value);
	public native int testSetStaticCharField(char value);
	public native int testSetStaticShortField(short value);
	public native int testSetStaticIntField(int value);
	public native int testSetStaticLongField(long value);
	public native int testSetStaticFloatField(float value);
	public native int testSetStaticDoubleField(double value);
	public native int testThrow(Throwable throwable) throws Throwable;
	public native int testThrowNew() throws Throwable;
	public native Object testToReflectedField(int value);
	public native Object testToReflectedMethod(int value);
	public native int testAttachAndDetachCurrentThread();
	public native int testGetEnv();
		
	public interface testInterface {
		public void method1();
	}
	
	public class testClass {
		public void method1() {};
	}
	
	/**
	 * make sure the native library required to run the natives is loaded
	 */
	public void ensureLibraryLoaded(){
		if (libraryLoaded == false){
			System.loadLibrary(NATIVE_LIBRARY_NAME);
			libraryLoaded = true;
		}
	}
	
	/**
	 * constructor
	 */	
	public TestNatives() {
		ensureLibraryLoaded();
		objectField = null;
		booleanField = false;
		byteField = 0;
		charField = '\u0000';
		shortField = 0;
		intField = 0;
		longField = 0L;
		floatField = 0.0f;
		doubleField = 0.0d;
		staticObjectField = null;
		staticBooleanField = false;	
		staticByteField = 0;
		staticCharField = '\u0000';
		staticShortField = 0;
		staticIntField = 0;
		staticLongField = 0L;
		staticFloatField = 0.0f;
		staticDoubleField = 0.0d;
	}	

	/**
	 * constructor
	 * @param intValue the intValue to be assigned to the intField that is looked up by the native
	 */
	public TestNatives(int intValue){
		ensureLibraryLoaded();
		intField = intValue;
	}
	
	/**
	 * constructor
	 * @param value the value to be assigned to the non-static fields that are looked up by the native
	 * @param staticValue the value to be assigned to the static fields that are looked up by the native
	 */	
	public TestNatives(JValue value, JValue staticValue) {
		ensureLibraryLoaded();
		objectField = value.getL();
		booleanField = value.getZ();
		byteField = value.getB();
		charField = value.getC();
		shortField = value.getS();
		intField = value.getI();
		longField = value.getJ();
		floatField = value.getF();
		doubleField = value.getD();
		staticObjectField = staticValue.getL();
		staticBooleanField = staticValue.getZ();	
		staticByteField = staticValue.getB();
		staticCharField = staticValue.getC();
		staticShortField = staticValue.getS();
		staticIntField = staticValue.getI();
		staticLongField = staticValue.getJ();
		staticFloatField = staticValue.getF();
		staticDoubleField = staticValue.getD();
	}

	public TestNatives getObjectField() {
		return objectField;
	}	
	
	public boolean getBooleanField() {
		return booleanField;
	}	
	
	public byte getByteField() {
		return byteField;
	}

	public char getCharField() {
		return charField;
	}
	
	public short getShortField() {
		return shortField;
	}
	
	public int getIntField() {
		return intField;
	}	
	
	public long getLongField() {
		return longField;
	}
	
	public float getFloatField() {
		return floatField;
	}
	
	public double getDoubleField() {
		return doubleField;
	}
	
	public String[] strArr1, strArr2, strArr3, strArr4;
	public String str;
	
	public String[] getStrArr1() {
		return strArr1;
	}
	
	public String[] getStrArr2() {
		return strArr2;
	}	
	
	public String[] getStrArr3() {
		return strArr3;
	}
	
	public String[] getStrArr4() {
		return strArr4;
	}	
	
	public String getStr() {
		return str;
	}
	
	public void setStrArr1(String[] strArr) {
		this.strArr1 = strArr;
	}
	
	public void setStrArr2(String[] strArr) {
		this.strArr2 = strArr;
	}	
	
	public void setStrArr3(String[] strArr) {
		this.strArr3 = strArr;
	}
	
	public void setStrArr4(String[] strArr) {
		this.strArr4 = strArr;
	}	
	
	public void setStr(String str) {
		this.str = str;
	}
	
	/* void methods */
	
	public void voidMethod() {
		strArr1 = null;
	};
	
	public void voidMethod_oneArg(String[] array) {
		strArr2 = array;
	};
	
	public void voidMethod_twoArgs(String[] array, String string) {
		strArr3 = array;
		str = string;
	};
	
	public void voidMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		strArr4 = null;
	};
	
	/* object methods */	
	
	public TestNatives objectMethod_oneArg(TestNatives arg) {
		strArr1 = null;
		return arg;
	};
	
	public TestNatives objectMethod_twoArgs(TestNatives arg, String[] array) {
		strArr2 = array;
		return arg;
	};
	
	public TestNatives objectMethod_threeArgs(TestNatives arg, String[] array, String string) {
		strArr3 = array;
		str = string;
		return arg;
	};
	
	public TestNatives objectMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		strArr4 = null;		
		return l;
	};
	
	/* boolean methods */	
	
	public boolean booleanMethod_oneArg(boolean arg) {	
		strArr1 = null;	
		return arg;
	};
	
	public boolean booleanMethod_twoArgs(boolean arg, String[] array) {
		strArr2 = array;
		return arg;
	};
	
	public boolean booleanMethod_threeArgs(boolean arg, String[] array, String string) {
		strArr3 = array;
		str = string;
		return arg;
	};
	
	public boolean booleanMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		strArr4 = null;		
		return z;
	};
	
	/* byte methods */	
	
	public byte byteMethod_oneArg(byte arg) {	
		strArr1 = null;
		return arg;
	};
	
	public byte byteMethod_twoArgs(byte arg, String[] array) {
		strArr2 = array;
		return arg;
	};
	
	public byte byteMethod_threeArgs(byte arg, String[] array, String string) {
		strArr3 = array;
		str = string;
		return arg;
	};
	
	public byte byteMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		strArr4 = null;		
		return b;
	};
	
	/* char methods */	
	
	public char charMethod_oneArg(char arg) {	
		strArr1 = null;
		return arg;
	};
	
	public char charMethod_twoArgs(char arg, String[] array) {
		strArr2 = array;
		return arg;
	};
	
	public char charMethod_threeArgs(char arg, String[] array, String string) {
		strArr3 = array;
		str = string;
		return arg;
	};
	
	public char charMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		strArr4 = null;		
		return c;
	};
	
	/* short methods */	
	
	public short shortMethod_oneArg(short arg) {		
		strArr1 = null;
		return arg;
	};
	
	public short shortMethod_twoArgs(short arg, String[] array) {
		strArr2 = array;
		return arg;
	};
	
	public short shortMethod_threeArgs(short arg, String[] array, String string) {
		strArr3 = array;
		str = string;
		return arg;
	};
	
	public short shortMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		strArr4 = null;		
		return s;
	};
	
	/* int methods */	
	
	public int intMethod_oneArg(int arg) {		
		strArr1 = null;
		return arg;
	};
	
	public int intMethod_twoArgs(int arg, String[] array) {
		strArr2 = array;
		return arg;
	};
	
	public int intMethod_threeArgs(int arg, String[] array, String string) {
		strArr3 = array;
		str = string;
		return arg;
	};
	
	public int intMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		strArr4 = null;		
		return i;
	};
	
	/* long methods */	
	
	public long longMethod_oneArg(long arg) {
		strArr1 = null;
		return arg;
	};
	
	public long longMethod_twoArgs(long arg, String[] array) {
		strArr2 = array;
		return arg;
	};
	
	public long longMethod_threeArgs(long arg, String[] array, String string) {
		strArr3 = array;
		str = string;
		return arg;
	};
	
	public long longMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		strArr4 = null;		
		return j;
	};
	
	/* float methods */	
	
	public float floatMethod_oneArg(float arg) {		
		strArr1 = null;
		return arg;
	};
	
	public float floatMethod_twoArgs(float arg, String[] array) {
		strArr2 = array;
		return arg;
	};
	
	public float floatMethod_threeArgs(float arg, String[] array, String string) {
		strArr3 = array;
		str = string;
		return arg;
	};
	
	public float floatMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		strArr4 = null;		
		return f;
	};
	
	/* double methods */	
	
	public double doubleMethod_oneArg(double arg) {		
		strArr1 = null;
		return arg;
	};
	
	public double doubleMethod_twoArgs(double arg, String[] array) {
		strArr2 = array;
		return arg;
	};
	
	public double doubleMethod_threeArgs(double arg, String[] array, String string) {
		strArr3 = array;
		str = string;
		return arg;
	};
	
	public double doubleMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		strArr4 = null;		
		return d;
	};
	
	static public String[] staticStrArr1, staticStrArr2, staticStrArr3, staticStrArr4;
	static public String staticStr;
	
	/* static void methods */
	
	static public void staticVoidMethod() {
		staticStrArr1 = null;
	};
	
	static public void staticVoidMethod_oneArg(String[] array) {
		staticStrArr2 = array;
	};
	
	static public void staticVoidMethod_twoArgs(String[] array, String string) {
		staticStrArr3 = array;
		staticStr = string;
	};
	
	static public void staticVoidMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		staticStrArr4 = null;
	};
	
	/* static object methods */	
	
	static public TestNatives staticObjectMethod_oneArg(TestNatives arg) {
		staticStrArr1 = null;
		return arg;
	};
	
	static public TestNatives staticObjectMethod_twoArgs(TestNatives arg, String[] array) {
		staticStrArr2 = array;
		return arg;
	};
	
	static public TestNatives staticObjectMethod_threeArgs(TestNatives arg, String[] array, String string) {
		staticStrArr3 = array;
		staticStr = string;
		return arg;
	};
	
	static public TestNatives staticObjectMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		staticStrArr4 = null;		
		return l;
	};
	
	/* static boolean methods */	
	
	static public boolean staticBooleanMethod_oneArg(boolean arg) {	
		staticStrArr1 = null;	
		return arg;
	};
	
	static public boolean staticBooleanMethod_twoArgs(boolean arg, String[] array) {
		staticStrArr2 = array;
		return arg;
	};
	
	static public boolean staticBooleanMethod_threeArgs(boolean arg, String[] array, String string) {
		staticStrArr3 = array;
		staticStr = string;
		return arg;
	};
	
	static public boolean staticBooleanMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		staticStrArr4 = null;		
		return z;
	};
	
	/* static byte methods */	
	
	static public byte staticByteMethod_oneArg(byte arg) {	
		staticStrArr1 = null;
		return arg;
	};
	
	static public byte staticByteMethod_twoArgs(byte arg, String[] array) {
		staticStrArr2 = array;
		return arg;
	};
	
	static public byte staticByteMethod_threeArgs(byte arg, String[] array, String string) {
		staticStrArr3 = array;
		staticStr = string;
		return arg;
	};
	
	static public byte staticByteMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		staticStrArr4 = null;		
		return b;
	};
	
	/* static char methods */	
	
	static public char staticCharMethod_oneArg(char arg) {	
		staticStrArr1 = null;
		return arg;
	};
	
	static public char staticCharMethod_twoArgs(char arg, String[] array) {
		staticStrArr2 = array;
		return arg;
	};
	
	static public char staticCharMethod_threeArgs(char arg, String[] array, String string) {
		staticStrArr3 = array;
		staticStr = string;
		return arg;
	};
	
	static public char staticCharMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		staticStrArr4 = null;		
		return c;
	};
	
	/* static short methods */	
	
	static public short staticShortMethod_oneArg(short arg) {		
		staticStrArr1 = null;
		return arg;
	};
	
	static public short staticShortMethod_twoArgs(short arg, String[] array) {
		staticStrArr2 = array;
		return arg;
	};
	
	static public short staticShortMethod_threeArgs(short arg, String[] array, String string) {
		staticStrArr3 = array;
		staticStr = string;
		return arg;
	};
	
	static public short staticShortMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		staticStrArr4 = null;		
		return s;
	};
	
	/* static int methods */	
	
	static public int staticIntMethod_oneArg(int arg) {		
		staticStrArr1 = null;
		return arg;
	};
	
	static public int staticIntMethod_twoArgs(int arg, String[] array) {
		staticStrArr2 = array;
		return arg;
	};
	
	static public int staticIntMethod_threeArgs(int arg, String[] array, String string) {
		staticStrArr3 = array;
		staticStr = string;
		return arg;
	};
	
	static public int staticIntMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		staticStrArr4 = null;		
		return i;
	};
	
	/* static long methods */	
	
	static public long staticLongMethod_oneArg(long arg) {
		staticStrArr1 = null;
		return arg;
	};
	
	static public long staticLongMethod_twoArgs(long arg, String[] array) {
		staticStrArr2 = array;
		return arg;
	};
	
	static public long staticLongMethod_threeArgs(long arg, String[] array, String string) {
		staticStrArr3 = array;
		staticStr = string;
		return arg;
	};
	
	static public long staticLongMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		staticStrArr4 = null;		
		return j;
	};
	
	/* static float methods */	
	
	static public float staticFloatMethod_oneArg(float arg) {		
		staticStrArr1 = null;
		return arg;
	};
	
	static public float staticFloatMethod_twoArgs(float arg, String[] array) {
		staticStrArr2 = array;
		return arg;
	};
	
	static public float staticFloatMethod_threeArgs(float arg, String[] array, String string) {
		staticStrArr3 = array;
		staticStr = string;
		return arg;
	};
	
	static public float staticFloatMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		staticStrArr4 = null;		
		return f;
	};
	
	/* static double methods */	
	
	static public double staticDoubleMethod_oneArg(double arg) {		
		staticStrArr1 = null;
		return arg;
	};
	
	static public double staticDoubleMethod_twoArgs(double arg, String[] array) {
		staticStrArr2 = array;
		return arg;
	};
	
	static public double staticDoubleMethod_threeArgs(double arg, String[] array, String string) {
		staticStrArr3 = array;
		staticStr = string;
		return arg;
	};
	
	static public double staticDoubleMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		staticStrArr4 = null;
		return d;
	};	
}
