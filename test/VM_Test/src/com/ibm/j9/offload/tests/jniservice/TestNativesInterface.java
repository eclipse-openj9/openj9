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
package com.ibm.j9.offload.tests.jniservice;

public interface TestNativesInterface {
	
	/* void methods */
	public void voidMethod();
	public void voidMethod_oneArg(String[] array);
	public void voidMethod_twoArgs(String[] array, String string);
	public void voidMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l);
	
	/* object methods */
	public TestNatives objectMethod_oneArg(TestNatives arg);
	public TestNatives objectMethod_twoArgs(TestNatives arg, String[] array);
	public TestNatives objectMethod_threeArgs(TestNatives arg, String[] array, String string);
	public TestNatives objectMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l);
	
	/* boolean methods */	
	public boolean booleanMethod_oneArg(boolean arg);
	public boolean booleanMethod_twoArgs(boolean arg, String[] array);
	public boolean booleanMethod_threeArgs(boolean arg, String[] array, String string);
	public boolean booleanMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l);
	
	/* byte methods */	
	public byte byteMethod_oneArg(byte arg);
	public byte byteMethod_twoArgs(byte arg, String[] array);
	public byte byteMethod_threeArgs(byte arg, String[] array, String string);
	public byte byteMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l);
	
	/* char methods */	
	public char charMethod_oneArg(char arg);
	public char charMethod_twoArgs(char arg, String[] array);
	public char charMethod_threeArgs(char arg, String[] array, String string);
	public char charMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l);
	
	/* short methods */	
	public short shortMethod_oneArg(short arg);
	public short shortMethod_twoArgs(short arg, String[] array);
	public short shortMethod_threeArgs(short arg, String[] array, String string);
	public short shortMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l);
	
	/* int methods */	
	public int intMethod_oneArg(int arg);
	public int intMethod_twoArgs(int arg, String[] array);
	public int intMethod_threeArgs(int arg, String[] array, String string);
	public int intMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l);
	
	/* long methods */	
	public long longMethod_oneArg(long arg);
	public long longMethod_twoArgs(long arg, String[] array);
	public long longMethod_threeArgs(long arg, String[] array, String string);
	public long longMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l);
	
	/* float methods */
	public float floatMethod_oneArg(float arg);
	public float floatMethod_twoArgs(float arg, String[] array);
	public float floatMethod_threeArgs(float arg, String[] array, String string);
	public float floatMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l);
	
	/* double methods */
	public double doubleMethod_oneArg(double arg);
	public double doubleMethod_twoArgs(double arg, String[] array);
	public double doubleMethod_threeArgs(double arg, String[] array, String string);
	public double doubleMethod_manyArgs(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l);
}
