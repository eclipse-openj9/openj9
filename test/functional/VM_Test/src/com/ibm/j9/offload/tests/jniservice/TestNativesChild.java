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

public class TestNativesChild extends TestNatives {

	private boolean isChildInvoked;
	
	public TestNativesChild() {
		super();
		isChildInvoked = false;
	}

	public TestNativesChild(int intValue) {
		super(intValue);
		isChildInvoked = false;
	}

	public TestNativesChild(JValue value, JValue staticValue) {
		super(value, staticValue);
		isChildInvoked = false;
	}
	
	public boolean getIsChildInvoked() {
		return isChildInvoked;
	}
	
	public void setIsChildInvoked(boolean newValue) {
		isChildInvoked = newValue;
	}

	/* void methods */
	
	public void voidMethod() {
		isChildInvoked = true;
		super.voidMethod();
	};
	
	public void voidMethod_oneArg(String[] array) {
		isChildInvoked = true;
		super.voidMethod_oneArg(array);		
	};
	
	public void voidMethod_twoArgs(String[] array, String string) {
		isChildInvoked = true;
		super.voidMethod_twoArgs(array, string);
	};
	
	public void voidMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		isChildInvoked = true;	
		super.voidMethod_manyArgs(z, b, c, s, i, j, f, d, l);	
	};
	
	/* object methods */	
	
	public TestNatives objectMethod_oneArg(TestNatives arg) {
		isChildInvoked = true;	
		return super.objectMethod_oneArg(arg);
	};
	
	public TestNatives objectMethod_twoArgs(TestNatives arg, String[] array) {
		isChildInvoked = true;	
		return super.objectMethod_twoArgs(arg, array);
	};
	
	public TestNatives objectMethod_threeArgs(TestNatives arg, String[] array, String string) {
		isChildInvoked = true;
		return super.objectMethod_threeArgs(arg, array, string);
	};
	
	public TestNatives objectMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		isChildInvoked = true;
		return super.objectMethod_manyArgs(z, b, c, s, i, j, f, d, l);
	};
	
	/* boolean methods */	
	
	public boolean booleanMethod_oneArg(boolean arg) {
		isChildInvoked = true;
		return super.booleanMethod_oneArg(arg);
	};
	
	public boolean booleanMethod_twoArgs(boolean arg, String[] array) {
		isChildInvoked = true;	
		return super.booleanMethod_twoArgs(arg, array);
	};
	
	public boolean booleanMethod_threeArgs(boolean arg, String[] array, String string) {
		isChildInvoked = true;
		return super.booleanMethod_threeArgs(arg, array, string);
	};
	
	public boolean booleanMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		isChildInvoked = true;
		return super.booleanMethod_manyArgs(z, b, c, s, i, j, f, d, l);
	};
	
	/* byte methods */	
	
	public byte byteMethod_oneArg(byte arg) {	
		isChildInvoked = true;
		return super.byteMethod_oneArg(arg);
	};
	
	public byte byteMethod_twoArgs(byte arg, String[] array) {
		isChildInvoked = true;
		return super.byteMethod_twoArgs(arg, array);
	};
	
	public byte byteMethod_threeArgs(byte arg, String[] array, String string) {
		isChildInvoked = true;
		return super.byteMethod_threeArgs(arg, array, string);
	};
	
	public byte byteMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		isChildInvoked = true;
		return super.byteMethod_manyArgs(z, b, c, s, i, j, f, d, l);
	};
	
	/* char methods */	
	
	public char charMethod_oneArg(char arg) {	
		isChildInvoked = true;
		return super.charMethod_oneArg(arg);
	};
	
	public char charMethod_twoArgs(char arg, String[] array) {
		isChildInvoked = true;
		return super.charMethod_twoArgs(arg, array);
	};
	
	public char charMethod_threeArgs(char arg, String[] array, String string) {
		isChildInvoked = true;
		return super.charMethod_threeArgs(arg, array, string);
	};
	
	public char charMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		isChildInvoked = true;
		return super.charMethod_manyArgs(z, b, c, s, i, j, f, d, l);
	};
	
	/* short methods */	
	
	public short shortMethod_oneArg(short arg) {		
		isChildInvoked = true;
		return super.shortMethod_oneArg(arg);
	};
	
	public short shortMethod_twoArgs(short arg, String[] array) {
		isChildInvoked = true;;
		return super.shortMethod_twoArgs(arg, array);
	};
	
	public short shortMethod_threeArgs(short arg, String[] array, String string) {
		isChildInvoked = true;
		return super.shortMethod_threeArgs(arg, array, string);
	};
	
	public short shortMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		isChildInvoked = true;
		return super.shortMethod_manyArgs(z, b, c, s, i, j, f, d, l);
	};
	
	/* int methods */	
	
	public int intMethod_oneArg(int arg) {		
		isChildInvoked = true;
		return super.intMethod_oneArg(arg);
	};
	
	public int intMethod_twoArgs(int arg, String[] array) {
		isChildInvoked = true;
		return super.intMethod_twoArgs(arg, array);
	};
	
	public int intMethod_threeArgs(int arg, String[] array, String string) {
		isChildInvoked = true;
		return super.intMethod_threeArgs(arg, array, string);
	};
	
	public int intMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		isChildInvoked = true;
		return super.intMethod_manyArgs(z, b, c, s, i, j, f, d, l);
	};
	
	/* long methods */	
	
	public long longMethod_oneArg(long arg) {
		isChildInvoked = true;
		return super.longMethod_oneArg(arg);
	};
	
	public long longMethod_twoArgs(long arg, String[] array) {
		isChildInvoked = true;
		return super.longMethod_twoArgs(arg, array);
	};
	
	public long longMethod_threeArgs(long arg, String[] array, String string) {
		isChildInvoked = true;
		return super.longMethod_threeArgs(arg, array, string);
	};
	
	public long longMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		isChildInvoked = true;		
		return super.longMethod_manyArgs(z, b, c, s, i, j, f, d, l);
	};
	
	/* float methods */	
	
	public float floatMethod_oneArg(float arg) {		
		isChildInvoked = true;
		return super.floatMethod_oneArg(arg);
	};
	
	public float floatMethod_twoArgs(float arg, String[] array) {
		isChildInvoked = true;
		return super.floatMethod_twoArgs(arg, array);
	};
	
	public float floatMethod_threeArgs(float arg, String[] array, String string) {
		isChildInvoked = true;
		return super.floatMethod_threeArgs(arg, array, string);
	};
	
	public float floatMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		isChildInvoked = true;		
		return super.floatMethod_manyArgs(z, b, c, s, i, j, f, d, l);
	};
	
	/* double methods */	
	
	public double doubleMethod_oneArg(double arg) {		
		isChildInvoked = true;
		return super.doubleMethod_oneArg(arg);
	};
	
	public double doubleMethod_twoArgs(double arg, String[] array) {
		isChildInvoked = true;
		return super.doubleMethod_twoArgs(arg, array);
	};
	
	public double doubleMethod_threeArgs(double arg, String[] array, String string) {
		isChildInvoked = true;
		return super.doubleMethod_threeArgs(arg, array, string);
	};
	
	public double doubleMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		isChildInvoked = true;		
		return super.doubleMethod_manyArgs(z, b, c, s, i, j, f, d, l);
	};
}
