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

import junit.framework.Test;
import junit.framework.TestSuite;
import junit.framework.TestCase;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Constructor;
import java.io.IOException;
import java.io.InputStream;

public class JNIServiceTest extends TestCase {
	public volatile int criticalValue;
	
	public final int STATIC = 0;
	public final int NONSTATIC = 1;
	public final int VOID = 0;
	public final int OBJECT = 1;
	public final int BOOLEAN = 2;
	public final int BYTE = 3;
	public final int CHAR = 4;
	public final int SHORT = 5;
	public final int INT = 6;
	public final int LONG = 7;
	public final int FLOAT = 8;
	public final int DOUBLE = 9;
	
	/**
	 * method that can be used to run the test by itself
	 * 
	 * @param args no valid arguments
	 */
	public static void main (String[] args) {
		junit.textui.TestRunner.run(suite());
	}
	
	public static Test suite(){
		return new TestSuite(JNIServiceTest.class);
	}
	
	/**
	 * tests AllocObject
	 */
	public void testAllocObject(){
		TestNatives testClass = new TestNatives(1234);
		try {
			Object objectCreated = testClass.testAllocObject(TestNatives.testClass.class);
			assertTrue("AllocObject did not created instance of right class", objectCreated.getClass() == TestNatives.testClass.class);
		} catch (InstantiationException e){
			fail("Unexpected exeption calling AllocObject"); 
		}
		
		try {
			testClass.testAllocObject(TestNatives.testInterface.class);
			fail("Expected Exception calling AllocObject for Interafce");
		} catch (InstantiationException e){
		} 
	}

	/**
	 * tests CallMethod
	 */
	public void testCallMethod(){
		TestNatives testClass = new TestNatives(0);
		
		JValue arg = new JValue();
		arg.setL(new TestNatives(100));
		arg.setZ(true);
		arg.setB((byte)1);
		arg.setC('a');
		arg.setS((short)2);
		arg.setI(3);
		arg.setJ(4L);
		arg.setF(5.5f);
		arg.setD(6.6d);
		
		JValue ret1 = new JValue();
		JValue ret2 = new JValue();
		JValue ret3 = new JValue();
		JValue ret4 = new JValue();
		
		String[] tempStrArr = {"string1", "string2", "string3"};
		
		try {
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.testCallMethod(VOID, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Void Method1 with no arguments did not execute correctly", testClass.getStrArr1() == null);
			
			assertTrue("Void Method2 with string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Void Method2 with string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Void Method2 with string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));
			
			assertTrue("Void Method3 with string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Void Method3 with string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Void Method3 with string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Void Method3 with string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Void Method3 with string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			
			assertTrue("Void Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);		
			testClass.testCallMethod(OBJECT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Object Method1 with object argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Object Method1 with object argument returned a wrong value", arg.getL().getIntField() == ret1.getL().getIntField());			
			
			assertTrue("Object Method2 with object then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Object Method2 with object then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Object Method2 with object then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Object Method2 with object then string array argument returned a wrong value", arg.getL().getIntField() == ret2.getL().getIntField());			
			
			assertTrue("Object Method3 with object, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Object Method3 with object, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Object Method3 with object, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Object Method3 with object, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Object Method3 with object, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Object Method3 with object, string array then string argument returned a wrong value", arg.getL().getIntField() == ret3.getL().getIntField());			
			
			assertTrue("Object Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Object Method4 with many arguments returned a wrong value", arg.getL().getIntField() == ret4.getL().getIntField());	
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);				
			testClass.testCallMethod(BOOLEAN, arg, ret1, ret2, ret3, ret4);			
			
			assertTrue("Boolean Method1 with boolean argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Boolean Method1 with boolean argument returned a wrong value", arg.getZ() == ret1.getZ());			
			
			assertTrue("Boolean Method2 with boolean then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Boolean Method2 with boolean then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Boolean Method2 with boolean then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Boolean Method2 with boolean then string array argument returned a wrong value", arg.getZ() == ret2.getZ());			
			
			assertTrue("Boolean Method3 with boolean, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Boolean Method3 with boolean, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Boolean Method3 with boolean, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Boolean Method3 with boolean, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Boolean Method3 with boolean, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Boolean Method3 with boolean, string array then string argument returned a wrong value", arg.getZ() == ret3.getZ());			
			
			assertTrue("Boolean Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Boolean Method4 with many arguments returned a wrong value", arg.getZ() == ret4.getZ());
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);			
			testClass.testCallMethod(BYTE, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Byte Method1 with byte argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Byte Method1 with byte argument returned a wrong value", arg.getB() == ret1.getB());			
			
			assertTrue("Byte Method2 with byte then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Byte Method2 with byte then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Byte Method2 with byte then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Byte Method2 with byte then string array argument returned a wrong value", arg.getB() == ret2.getB());			
			
			assertTrue("Byte Method3 with byte, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Byte Method3 with byte, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Byte Method3 with byte, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Byte Method3 with byte, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Byte Method3 with byte, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Byte Method3 with byte, string array then string argument returned a wrong value", arg.getB() == ret3.getB());			
			
			assertTrue("Byte Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Byte Method4 with many arguments returned a wrong value", arg.getB() == ret4.getB());
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);			
			testClass.testCallMethod(CHAR, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Char Method1 with char argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Char Method1 with char argument returned a wrong value", arg.getC() == ret1.getC());			
			
			assertTrue("Char Method2 with char then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Char Method2 with char then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Char Method2 with char then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Char Method2 with char then string array argument returned a wrong value", arg.getC() == ret2.getC());			
			
			assertTrue("Char Method3 with char, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Char Method3 with char, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Char Method3 with char, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Char Method3 with char, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Char Method3 with char, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Char Method3 with char, string array then string argument returned a wrong value", arg.getC() == ret3.getC());			
			
			assertTrue("Char Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Char Method4 with many arguments returned a wrong value", arg.getC() == ret4.getC());	
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);			
			testClass.testCallMethod(SHORT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Short Method1 with short argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Short Method1 with short argument returned a wrong value", arg.getS() == ret1.getS());			
			
			assertTrue("Short Method2 with short then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Short Method2 with short then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Short Method2 with short then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Short Method2 with short then string array argument returned a wrong value", arg.getS() == ret2.getS());			
			
			assertTrue("Short Method3 with short, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Short Method3 with short, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Short Method3 with short, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Short Method3 with short, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Short Method3 with short, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Short Method3 with short, string array then string argument returned a wrong value", arg.getS() == ret3.getS());			
			
			assertTrue("Short Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Short Method4 with many arguments returned a wrong value", arg.getS() == ret4.getS());
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.testCallMethod(INT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Int Method1 with int argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Int Method1 with int argument returned a wrong value", arg.getI() == ret1.getI());			
			
			assertTrue("Int Method2 with int then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Int Method2 with int then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Int Method2 with int then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Int Method2 with int then string array argument returned a wrong value", arg.getI() == ret2.getI());			
			
			assertTrue("Int Method3 with int, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Int Method3 with int, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Int Method3 with int, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Int Method3 with int, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Int Method3 with int, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Int Method3 with int, string array then string argument returned a wrong value", arg.getI() == ret3.getI());			
			
			assertTrue("Int Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Int Method4 with many arguments returned a wrong value", arg.getI() == ret4.getI());
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.testCallMethod(LONG, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Long Method1 with long argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Long Method1 with long argument returned a wrong value", arg.getJ() == ret1.getJ());			
			
			assertTrue("Long Method2 with long then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Long Method2 with long then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Long Method2 with long then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Long Method2 with long then string array argument returned a wrong value", arg.getJ() == ret2.getJ());			
			
			assertTrue("Long Method3 with long, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Long Method3 with long, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Long Method3 with long, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Long Method3 with long, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Long Method3 with long, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Long Method3 with long, string array then string argument returned a wrong value", arg.getJ() == ret3.getJ());			
			
			assertTrue("Long Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Long Method4 with many arguments returned a wrong value", arg.getJ() == ret4.getJ());
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.testCallMethod(FLOAT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Float Method1 with float argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Float Method1 with float argument returned a wrong value", arg.getF() == ret1.getF());			
			
			assertTrue("Float Method2 with float then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Float Method2 with float then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Float Method2 with float then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Float Method2 with float then string array argument returned a wrong value", arg.getF() == ret2.getF());			
			
			assertTrue("Float Method3 with float, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Float Method3 with float, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Float Method3 with float, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Float Method3 with float, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Float Method3 with float, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Float Method3 with float, string array then string argument returned a wrong value", arg.getF() == ret3.getF());			
			
			assertTrue("Float Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Float Method4 with many arguments returned a wrong value", arg.getF() == ret4.getF());
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.testCallMethod(DOUBLE, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Double Method1 with double argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Double Method1 with double argument returned a wrong value", arg.getD() == ret1.getD());			
			
			assertTrue("Double Method2 with double then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Double Method2 with double then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Double Method2 with double then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Double Method2 with double then string array argument returned a wrong value", arg.getD() == ret2.getD());			
			
			assertTrue("Double Method3 with double, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Double Method3 with double, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Double Method3 with double, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Double Method3 with double, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Double Method3 with double, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Double Method3 with double, string array then string argument returned a wrong value", arg.getD() == ret3.getD());			
			
			assertTrue("Double Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Double Method4 with many arguments returned a wrong value", arg.getD() == ret4.getD());
		} catch (Exception e) {
			fail("Exception occurred in testCallMethod:" + e.toString());
		}
	}

	/**
	 * tests CallMethodA
	 */
	public void testCallMethodA() {
		TestNatives testClass = new TestNatives(0);
		
		JValue arg = new JValue();
		arg.setL(new TestNatives(100));
		arg.setZ(true);
		arg.setB((byte)1);
		arg.setC('a');
		arg.setS((short)2);
		arg.setI(3);
		arg.setJ(4L);
		arg.setF(5.5f);
		arg.setD(6.6d);
		
		JValue ret1 = new JValue();
		JValue ret2 = new JValue();
		JValue ret3 = new JValue();
		JValue ret4 = new JValue();
		
		String[] tempStrArr = {"string1", "string2", "string3"};
		
		try {
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.testCallMethodA(VOID, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Void Method1 with no arguments did not execute correctly", testClass.getStrArr1() == null);
			
			assertTrue("Void Method2 with string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Void Method2 with string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Void Method2 with string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));
			
			assertTrue("Void Method3 with string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Void Method3 with string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Void Method3 with string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Void Method3 with string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Void Method3 with string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			
			assertTrue("Void Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);		
			testClass.testCallMethodA(OBJECT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Object Method1 with object argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Object Method1 with object argument returned a wrong value", arg.getL().getIntField() == ret1.getL().getIntField());			
			
			assertTrue("Object Method2 with object then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Object Method2 with object then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Object Method2 with object then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Object Method2 with object then string array argument returned a wrong value", arg.getL().getIntField() == ret2.getL().getIntField());			
			
			assertTrue("Object Method3 with object, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Object Method3 with object, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Object Method3 with object, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Object Method3 with object, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Object Method3 with object, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Object Method3 with object, string array then string argument returned a wrong value", arg.getL().getIntField() == ret3.getL().getIntField());			
			
			assertTrue("Object Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Object Method4 with many arguments returned a wrong value", arg.getL().getIntField() == ret4.getL().getIntField());	
						
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);				
			testClass.testCallMethodA(BOOLEAN, arg, ret1, ret2, ret3, ret4);			
			
			assertTrue("Boolean Method1 with boolean argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Boolean Method1 with boolean argument returned a wrong value", arg.getZ() == ret1.getZ());			
			
			assertTrue("Boolean Method2 with boolean then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Boolean Method2 with boolean then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Boolean Method2 with boolean then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Boolean Method2 with boolean then string array argument returned a wrong value", arg.getZ() == ret2.getZ());			
			
			assertTrue("Boolean Method3 with boolean, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Boolean Method3 with boolean, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Boolean Method3 with boolean, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Boolean Method3 with boolean, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Boolean Method3 with boolean, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Boolean Method3 with boolean, string array then string argument returned a wrong value", arg.getZ() == ret3.getZ());			
			
			assertTrue("Boolean Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Boolean Method4 with many arguments returned a wrong value", arg.getZ() == ret4.getZ());
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);			
			testClass.testCallMethodA(BYTE, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Byte Method1 with byte argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Byte Method1 with byte argument returned a wrong value", arg.getB() == ret1.getB());			
			
			assertTrue("Byte Method2 with byte then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Byte Method2 with byte then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Byte Method2 with byte then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Byte Method2 with byte then string array argument returned a wrong value", arg.getB() == ret2.getB());			
			
			assertTrue("Byte Method3 with byte, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Byte Method3 with byte, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Byte Method3 with byte, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Byte Method3 with byte, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Byte Method3 with byte, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Byte Method3 with byte, string array then string argument returned a wrong value", arg.getB() == ret3.getB());			
			
			assertTrue("Byte Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Byte Method4 with many arguments returned a wrong value", arg.getB() == ret4.getB());
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);			
			testClass.testCallMethodA(CHAR, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Char Method1 with char argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Char Method1 with char argument returned a wrong value", arg.getC() == ret1.getC());			
			
			assertTrue("Char Method2 with char then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Char Method2 with char then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Char Method2 with char then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Char Method2 with char then string array argument returned a wrong value", arg.getC() == ret2.getC());			
			
			assertTrue("Char Method3 with char, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Char Method3 with char, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Char Method3 with char, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Char Method3 with char, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Char Method3 with char, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Char Method3 with char, string array then string argument returned a wrong value", arg.getC() == ret3.getC());			
			
			assertTrue("Char Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Char Method4 with many arguments returned a wrong value", arg.getC() == ret4.getC());	
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);			
			testClass.testCallMethodA(SHORT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Short Method1 with short argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Short Method1 with short argument returned a wrong value", arg.getS() == ret1.getS());			
			
			assertTrue("Short Method2 with short then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Short Method2 with short then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Short Method2 with short then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Short Method2 with short then string array argument returned a wrong value", arg.getS() == ret2.getS());			
			
			assertTrue("Short Method3 with short, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Short Method3 with short, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Short Method3 with short, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Short Method3 with short, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Short Method3 with short, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Short Method3 with short, string array then string argument returned a wrong value", arg.getS() == ret3.getS());			
			
			assertTrue("Short Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Short Method4 with many arguments returned a wrong value", arg.getS() == ret4.getS());
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.testCallMethodA(INT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Int Method1 with int argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Int Method1 with int argument returned a wrong value", arg.getI() == ret1.getI());			
			
			assertTrue("Int Method2 with int then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Int Method2 with int then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Int Method2 with int then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Int Method2 with int then string array argument returned a wrong value", arg.getI() == ret2.getI());			
			
			assertTrue("Int Method3 with int, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Int Method3 with int, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Int Method3 with int, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Int Method3 with int, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Int Method3 with int, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Int Method3 with int, string array then string argument returned a wrong value", arg.getI() == ret3.getI());			
			
			assertTrue("Int Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Int Method4 with many arguments returned a wrong value", arg.getI() == ret4.getI());
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.testCallMethodA(LONG, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Long Method1 with long argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Long Method1 with long argument returned a wrong value", arg.getJ() == ret1.getJ());			
			
			assertTrue("Long Method2 with long then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Long Method2 with long then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Long Method2 with long then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Long Method2 with long then string array argument returned a wrong value", arg.getJ() == ret2.getJ());			
			
			assertTrue("Long Method3 with long, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Long Method3 with long, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Long Method3 with long, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Long Method3 with long, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Long Method3 with long, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Long Method3 with long, string array then string argument returned a wrong value", arg.getJ() == ret3.getJ());			
			
			assertTrue("Long Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Long Method4 with many arguments returned a wrong value", arg.getJ() == ret4.getJ());
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.testCallMethodA(FLOAT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Float Method1 with float argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Float Method1 with float argument returned a wrong value", arg.getF() == ret1.getF());			
			
			assertTrue("Float Method2 with float then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Float Method2 with float then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Float Method2 with float then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Float Method2 with float then string array argument returned a wrong value", arg.getF() == ret2.getF());			
			
			assertTrue("Float Method3 with float, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Float Method3 with float, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Float Method3 with float, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Float Method3 with float, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Float Method3 with float, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Float Method3 with float, string array then string argument returned a wrong value", arg.getF() == ret3.getF());			
			
			assertTrue("Float Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Float Method4 with many arguments returned a wrong value", arg.getF() == ret4.getF());
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.testCallMethodA(DOUBLE, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Double Method1 with double argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Double Method1 with double argument returned a wrong value", arg.getD() == ret1.getD());			
			
			assertTrue("Double Method2 with double then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Double Method2 with double then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Double Method2 with double then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Double Method2 with double then string array argument returned a wrong value", arg.getD() == ret2.getD());			
			
			assertTrue("Double Method3 with double, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Double Method3 with double, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Double Method3 with double, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Double Method3 with double, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Double Method3 with double, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Double Method3 with double, string array then string argument returned a wrong value", arg.getD() == ret3.getD());			
			
			assertTrue("Double Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Double Method4 with many arguments returned a wrong value", arg.getD() == ret4.getD());			
		} catch (Exception e) {
			fail("Exception occurred in testCallMethodA:" + e.toString());
		}
	}
	
	/**
	 * tests CallMethodV
	 */
	public void testCallMethodV(){
		TestNatives testClass = new TestNatives(0);
		
		JValue arg = new JValue();
		arg.setL(new TestNatives(100));
		arg.setZ(true);
		arg.setB((byte)1);
		arg.setC('a');
		arg.setS((short)2);
		arg.setI(3);
		arg.setJ(4L);
		arg.setF(5.5f);
		arg.setD(6.6d);
		
		JValue ret1 = new JValue();
		JValue ret2 = new JValue();
		JValue ret3 = new JValue();
		JValue ret4 = new JValue();
		
		String[] tempStrArr = {"string1", "string2", "string3"};
		
		try {
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.testCallMethodV(VOID, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Void Method1 with no arguments did not execute correctly", testClass.getStrArr1() == null);
			
			assertTrue("Void Method2 with string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Void Method2 with string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Void Method2 with string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));
			
			assertTrue("Void Method3 with string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Void Method3 with string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Void Method3 with string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Void Method3 with string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Void Method3 with string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			
			assertTrue("Void Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);		
			testClass.testCallMethodV(OBJECT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Object Method1 with object argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Object Method1 with object argument returned a wrong value", arg.getL().getIntField() == ret1.getL().getIntField());			
			
			assertTrue("Object Method2 with object then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Object Method2 with object then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Object Method2 with object then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Object Method2 with object then string array argument returned a wrong value", arg.getL().getIntField() == ret2.getL().getIntField());			
			
			assertTrue("Object Method3 with object, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Object Method3 with object, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Object Method3 with object, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Object Method3 with object, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Object Method3 with object, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Object Method3 with object, string array then string argument returned a wrong value", arg.getL().getIntField() == ret3.getL().getIntField());			
			
			assertTrue("Object Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Object Method4 with many arguments returned a wrong value", arg.getL().getIntField() == ret4.getL().getIntField());	
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);				
			testClass.testCallMethodV(BOOLEAN, arg, ret1, ret2, ret3, ret4);			
			
			assertTrue("Boolean Method1 with boolean argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Boolean Method1 with boolean argument returned a wrong value", arg.getZ() == ret1.getZ());			
			
			assertTrue("Boolean Method2 with boolean then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Boolean Method2 with boolean then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Boolean Method2 with boolean then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Boolean Method2 with boolean then string array argument returned a wrong value", arg.getZ() == ret2.getZ());			
			
			assertTrue("Boolean Method3 with boolean, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Boolean Method3 with boolean, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Boolean Method3 with boolean, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Boolean Method3 with boolean, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Boolean Method3 with boolean, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Boolean Method3 with boolean, string array then string argument returned a wrong value", arg.getZ() == ret3.getZ());			
			
			assertTrue("Boolean Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Boolean Method4 with many arguments returned a wrong value", arg.getZ() == ret4.getZ());
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);			
			testClass.testCallMethodV(BYTE, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Byte Method1 with byte argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Byte Method1 with byte argument returned a wrong value", arg.getB() == ret1.getB());			
			
			assertTrue("Byte Method2 with byte then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Byte Method2 with byte then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Byte Method2 with byte then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Byte Method2 with byte then string array argument returned a wrong value", arg.getB() == ret2.getB());			
			
			assertTrue("Byte Method3 with byte, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Byte Method3 with byte, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Byte Method3 with byte, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Byte Method3 with byte, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Byte Method3 with byte, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Byte Method3 with byte, string array then string argument returned a wrong value", arg.getB() == ret3.getB());			
			
			assertTrue("Byte Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Byte Method4 with many arguments returned a wrong value", arg.getB() == ret4.getB());
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);			
			testClass.testCallMethodV(CHAR, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Char Method1 with char argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Char Method1 with char argument returned a wrong value", arg.getC() == ret1.getC());			
			
			assertTrue("Char Method2 with char then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Char Method2 with char then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Char Method2 with char then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Char Method2 with char then string array argument returned a wrong value", arg.getC() == ret2.getC());			
			
			assertTrue("Char Method3 with char, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Char Method3 with char, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Char Method3 with char, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Char Method3 with char, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Char Method3 with char, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Char Method3 with char, string array then string argument returned a wrong value", arg.getC() == ret3.getC());			
			
			assertTrue("Char Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Char Method4 with many arguments returned a wrong value", arg.getC() == ret4.getC());	
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);			
			testClass.testCallMethodV(SHORT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Short Method1 with short argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Short Method1 with short argument returned a wrong value", arg.getS() == ret1.getS());			
			
			assertTrue("Short Method2 with short then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Short Method2 with short then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Short Method2 with short then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Short Method2 with short then string array argument returned a wrong value", arg.getS() == ret2.getS());			
			
			assertTrue("Short Method3 with short, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Short Method3 with short, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Short Method3 with short, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Short Method3 with short, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Short Method3 with short, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Short Method3 with short, string array then string argument returned a wrong value", arg.getS() == ret3.getS());			
			
			assertTrue("Short Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Short Method4 with many arguments returned a wrong value", arg.getS() == ret4.getS());
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.testCallMethodV(INT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Int Method1 with int argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Int Method1 with int argument returned a wrong value", arg.getI() == ret1.getI());			
			
			assertTrue("Int Method2 with int then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Int Method2 with int then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Int Method2 with int then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Int Method2 with int then string array argument returned a wrong value", arg.getI() == ret2.getI());			
			
			assertTrue("Int Method3 with int, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Int Method3 with int, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Int Method3 with int, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Int Method3 with int, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Int Method3 with int, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Int Method3 with int, string array then string argument returned a wrong value", arg.getI() == ret3.getI());			
			
			assertTrue("Int Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Int Method4 with many arguments returned a wrong value", arg.getI() == ret4.getI());
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.testCallMethodV(LONG, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Long Method1 with long argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Long Method1 with long argument returned a wrong value", arg.getJ() == ret1.getJ());			
			
			assertTrue("Long Method2 with long then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Long Method2 with long then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Long Method2 with long then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Long Method2 with long then string array argument returned a wrong value", arg.getJ() == ret2.getJ());			
			
			assertTrue("Long Method3 with long, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Long Method3 with long, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Long Method3 with long, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Long Method3 with long, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Long Method3 with long, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Long Method3 with long, string array then string argument returned a wrong value", arg.getJ() == ret3.getJ());			
			
			assertTrue("Long Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Long Method4 with many arguments returned a wrong value", arg.getJ() == ret4.getJ());
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.testCallMethodV(FLOAT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Float Method1 with float argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Float Method1 with float argument returned a wrong value", arg.getF() == ret1.getF());			
			
			assertTrue("Float Method2 with float then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Float Method2 with float then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Float Method2 with float then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Float Method2 with float then string array argument returned a wrong value", arg.getF() == ret2.getF());			
			
			assertTrue("Float Method3 with float, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Float Method3 with float, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Float Method3 with float, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Float Method3 with float, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Float Method3 with float, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Float Method3 with float, string array then string argument returned a wrong value", arg.getF() == ret3.getF());			
			
			assertTrue("Float Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Float Method4 with many arguments returned a wrong value", arg.getF() == ret4.getF());
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.testCallMethodV(DOUBLE, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Double Method1 with double argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Double Method1 with double argument returned a wrong value", arg.getD() == ret1.getD());			
			
			assertTrue("Double Method2 with double then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Double Method2 with double then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Double Method2 with double then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Double Method2 with double then string array argument returned a wrong value", arg.getD() == ret2.getD());			
			
			assertTrue("Double Method3 with double, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Double Method3 with double, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Double Method3 with double, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Double Method3 with double, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Double Method3 with double, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Double Method3 with double, string array then string argument returned a wrong value", arg.getD() == ret3.getD());			
			
			assertTrue("Double Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Double Method4 with many arguments returned a wrong value", arg.getD() == ret4.getD());
		} catch (Exception e) {
			fail("Exception occurred in testCallMethodV:" + e.toString());
		}
	}
	
	/**
	 * tests CallNonvirtualMethod
	 */
	public void testCallNonvirtualMethod(){
		TestNativesChild testClass = new TestNativesChild(0);
		
		JValue arg = new JValue();
		arg.setL(new TestNatives(100));
		arg.setZ(true);
		arg.setB((byte)1);
		arg.setC('a');
		arg.setS((short)2);
		arg.setI(3);
		arg.setJ(4L);
		arg.setF(5.5f);
		arg.setD(6.6d);
		
		JValue ret1 = new JValue();
		JValue ret2 = new JValue();
		JValue ret3 = new JValue();
		JValue ret4 = new JValue();
		
		String[] tempStrArr = {"string1", "string2", "string3"};
		
		try {
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethod(VOID, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual void Method1 with no arguments did not execute correctly", testClass.getStrArr1() == null);
			
			assertTrue("Nonvirtual void Method2 with string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual void Method2 with string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual void Method2 with string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));
			
			assertTrue("Nonvirtual void Method3 with string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual void Method3 with string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual void Method3 with string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual void Method3 with string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual void Method3 with string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			
			assertTrue("Nonvirtual void Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethod(OBJECT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual object Method1 with object argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual object Method1 with object argument returned a wrong value", arg.getL().getIntField() == ret1.getL().getIntField());			
			
			assertTrue("Nonvirtual object Method2 with object then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual object Method2 with object then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual object Method2 with object then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual object Method2 with object then string array argument returned a wrong value", arg.getL().getIntField() == ret2.getL().getIntField());			
			
			assertTrue("Nonvirtual object Method3 with object, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual object Method3 with object, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual object Method3 with object, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual object Method3 with object, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual object Method3 with object, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual object Method3 with object, string array then string argument returned a wrong value", arg.getL().getIntField() == ret3.getL().getIntField());			
			
			assertTrue("Nonvirtual object Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual object Method4 with many arguments returned a wrong value", arg.getL().getIntField() == ret4.getL().getIntField());	
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethod(BOOLEAN, arg, ret1, ret2, ret3, ret4);			
			
			assertTrue("Nonvirtual boolean Method1 with boolean argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual boolean Method1 with boolean argument returned a wrong value", arg.getZ() == ret1.getZ());			
			
			assertTrue("Nonvirtual boolean Method2 with boolean then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual boolean Method2 with boolean then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual boolean Method2 with boolean then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual boolean Method2 with boolean then string array argument returned a wrong value", arg.getZ() == ret2.getZ());			
			
			assertTrue("Nonvirtual boolean Method3 with boolean, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual boolean Method3 with boolean, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual boolean Method3 with boolean, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual boolean Method3 with boolean, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual boolean Method3 with boolean, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual boolean Method3 with boolean, string array then string argument returned a wrong value", arg.getZ() == ret3.getZ());			
			
			assertTrue("Nonvirtual boolean Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual boolean Method4 with many arguments returned a wrong value", arg.getZ() == ret4.getZ());
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethod(BYTE, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual byte Method1 with byte argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual byte Method1 with byte argument returned a wrong value", arg.getB() == ret1.getB());			
			
			assertTrue("Nonvirtual byte Method2 with byte then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual byte Method2 with byte then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual byte Method2 with byte then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual byte Method2 with byte then string array argument returned a wrong value", arg.getB() == ret2.getB());			
			
			assertTrue("Nonvirtual byte Method3 with byte, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual byte Method3 with byte, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual byte Method3 with byte, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual byte Method3 with byte, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual byte Method3 with byte, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual byte Method3 with byte, string array then string argument returned a wrong value", arg.getB() == ret3.getB());			
			
			assertTrue("Nonvirtual byte Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual byte Method4 with many arguments returned a wrong value", arg.getB() == ret4.getB());
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);	
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethod(CHAR, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual char Method1 with char argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual char Method1 with char argument returned a wrong value", arg.getC() == ret1.getC());			
			
			assertTrue("Nonvirtual char Method2 with char then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual char Method2 with char then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual char Method2 with char then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual char Method2 with char then string array argument returned a wrong value", arg.getC() == ret2.getC());			
			
			assertTrue("Nonvirtual char Method3 with char, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual char Method3 with char, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual char Method3 with char, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual char Method3 with char, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual char Method3 with char, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual char Method3 with char, string array then string argument returned a wrong value", arg.getC() == ret3.getC());			
			
			assertTrue("Nonvirtual char Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual char Method4 with many arguments returned a wrong value", arg.getC() == ret4.getC());
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethod(SHORT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual short Method1 with short argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual short Method1 with short argument returned a wrong value", arg.getS() == ret1.getS());			
			
			assertTrue("Nonvirtual short Method2 with short then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual short Method2 with short then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual short Method2 with short then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual short Method2 with short then string array argument returned a wrong value", arg.getS() == ret2.getS());			
			
			assertTrue("Nonvirtual short Method3 with short, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual short Method3 with short, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual short Method3 with short, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual short Method3 with short, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual short Method3 with short, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual short Method3 with short, string array then string argument returned a wrong value", arg.getS() == ret3.getS());			
			
			assertTrue("Nonvirtual short Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual short Method4 with many arguments returned a wrong value", arg.getS() == ret4.getS());
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethod(INT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual int Method1 with int argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual int Method1 with int argument returned a wrong value", arg.getI() == ret1.getI());			
			
			assertTrue("Nonvirtual int Method2 with int then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual int Method2 with int then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual int Method2 with int then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual int Method2 with int then string array argument returned a wrong value", arg.getI() == ret2.getI());			
			
			assertTrue("Nonvirtual int Method3 with int, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual int Method3 with int, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual int Method3 with int, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual int Method3 with int, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual int Method3 with int, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual int Method3 with int, string array then string argument returned a wrong value", arg.getI() == ret3.getI());			
			
			assertTrue("Nonvirtual int Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual int Method4 with many arguments returned a wrong value", arg.getI() == ret4.getI());
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethod(LONG, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual long Method1 with long argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual long Method1 with long argument returned a wrong value", arg.getJ() == ret1.getJ());			
			
			assertTrue("Nonvirtual long Method2 with long then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual long Method2 with long then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual long Method2 with long then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual long Method2 with long then string array argument returned a wrong value", arg.getJ() == ret2.getJ());			
			
			assertTrue("Nonvirtual long Method3 with long, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual long Method3 with long, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual long Method3 with long, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual long Method3 with long, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual long Method3 with long, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual long Method3 with long, string array then string argument returned a wrong value", arg.getJ() == ret3.getJ());			
			
			assertTrue("Nonvirtual long Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual long Method4 with many arguments returned a wrong value", arg.getJ() == ret4.getJ());
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethod(FLOAT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual float Method1 with float argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual float Method1 with float argument returned a wrong value", arg.getF() == ret1.getF());			
			
			assertTrue("Nonvirtual float Method2 with float then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual float Method2 with float then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual float Method2 with float then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual float Method2 with float then string array argument returned a wrong value", arg.getF() == ret2.getF());			
			
			assertTrue("Nonvirtual float Method3 with float, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual float Method3 with float, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual float Method3 with float, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual float Method3 with float, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual float Method3 with float, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual float Method3 with float, string array then string argument returned a wrong value", arg.getF() == ret3.getF());			
			
			assertTrue("Nonvirtual float Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual float Method4 with many arguments returned a wrong value", arg.getF() == ret4.getF());
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethod(DOUBLE, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual double Method1 with double argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual double Method1 with double argument returned a wrong value", arg.getD() == ret1.getD());			
			
			assertTrue("Nonvirtual double Method2 with double then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual double Method2 with double then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual double Method2 with double then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual double Method2 with double then string array argument returned a wrong value", arg.getD() == ret2.getD());			
			
			assertTrue("Nonvirtual double Method3 with double, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual double Method3 with double, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual double Method3 with double, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual double Method3 with double, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual double Method3 with double, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual double Method3 with double, string array then string argument returned a wrong value", arg.getD() == ret3.getD());			
			
			assertTrue("Nonvirtual double Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual double Method4 with many arguments returned a wrong value", arg.getD() == ret4.getD());
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
		} catch (Exception e) {
			fail("Exception occurred in testCallNonvirtualMethod:" + e.toString());
		}
	}
	
	/**
	 * tests CallNonvirtualMethodA
	 */
	public void testCallNonvirtualMethodA(){
		TestNativesChild testClass = new TestNativesChild(0);
		
		JValue arg = new JValue();
		arg.setL(new TestNatives(100));
		arg.setZ(true);
		arg.setB((byte)1);
		arg.setC('a');
		arg.setS((short)2);
		arg.setI(3);
		arg.setJ(4L);
		arg.setF(5.5f);
		arg.setD(6.6d);
		
		JValue ret1 = new JValue();
		JValue ret2 = new JValue();
		JValue ret3 = new JValue();
		JValue ret4 = new JValue();
		
		String[] tempStrArr = {"string1", "string2", "string3"};
		
		try {
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodA(VOID, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual void Method1 with no arguments did not execute correctly", testClass.getStrArr1() == null);
			
			assertTrue("Nonvirtual void Method2 with string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual void Method2 with string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual void Method2 with string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));
			
			assertTrue("Nonvirtual void Method3 with string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual void Method3 with string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual void Method3 with string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual void Method3 with string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual void Method3 with string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			
			assertTrue("Nonvirtual void Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);	
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodA(OBJECT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual object Method1 with object argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual object Method1 with object argument returned a wrong value", arg.getL().getIntField() == ret1.getL().getIntField());			
			
			assertTrue("Nonvirtual object Method2 with object then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual object Method2 with object then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual object Method2 with object then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual object Method2 with object then string array argument returned a wrong value", arg.getL().getIntField() == ret2.getL().getIntField());			
			
			assertTrue("Nonvirtual object Method3 with object, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual object Method3 with object, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual object Method3 with object, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual object Method3 with object, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual object Method3 with object, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual object Method3 with object, string array then string argument returned a wrong value", arg.getL().getIntField() == ret3.getL().getIntField());			
			
			assertTrue("Nonvirtual object Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual object Method4 with many arguments returned a wrong value", arg.getL().getIntField() == ret4.getL().getIntField());	
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodA(BOOLEAN, arg, ret1, ret2, ret3, ret4);			
			
			assertTrue("Nonvirtual boolean Method1 with boolean argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual boolean Method1 with boolean argument returned a wrong value", arg.getZ() == ret1.getZ());			
			
			assertTrue("Nonvirtual boolean Method2 with boolean then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual boolean Method2 with boolean then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual boolean Method2 with boolean then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual boolean Method2 with boolean then string array argument returned a wrong value", arg.getZ() == ret2.getZ());			
			
			assertTrue("Nonvirtual boolean Method3 with boolean, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual boolean Method3 with boolean, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual boolean Method3 with boolean, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual boolean Method3 with boolean, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual boolean Method3 with boolean, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual boolean Method3 with boolean, string array then string argument returned a wrong value", arg.getZ() == ret3.getZ());			
			
			assertTrue("Nonvirtual boolean Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual boolean Method4 with many arguments returned a wrong value", arg.getZ() == ret4.getZ());
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);	
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodA(BYTE, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual byte Method1 with byte argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual byte Method1 with byte argument returned a wrong value", arg.getB() == ret1.getB());			
			
			assertTrue("Nonvirtual byte Method2 with byte then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual byte Method2 with byte then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual byte Method2 with byte then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual byte Method2 with byte then string array argument returned a wrong value", arg.getB() == ret2.getB());			
			
			assertTrue("Nonvirtual byte Method3 with byte, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual byte Method3 with byte, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual byte Method3 with byte, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual byte Method3 with byte, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual byte Method3 with byte, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual byte Method3 with byte, string array then string argument returned a wrong value", arg.getB() == ret3.getB());			
			
			assertTrue("Nonvirtual byte Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual byte Method4 with many arguments returned a wrong value", arg.getB() == ret4.getB());
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);	
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodA(CHAR, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual char Method1 with char argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual char Method1 with char argument returned a wrong value", arg.getC() == ret1.getC());			
			
			assertTrue("Nonvirtual char Method2 with char then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual char Method2 with char then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual char Method2 with char then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual char Method2 with char then string array argument returned a wrong value", arg.getC() == ret2.getC());			
			
			assertTrue("Nonvirtual char Method3 with char, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual char Method3 with char, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual char Method3 with char, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual char Method3 with char, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual char Method3 with char, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual char Method3 with char, string array then string argument returned a wrong value", arg.getC() == ret3.getC());			
			
			assertTrue("Nonvirtual char Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual char Method4 with many arguments returned a wrong value", arg.getC() == ret4.getC());	
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodA(SHORT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual short Method1 with short argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual short Method1 with short argument returned a wrong value", arg.getS() == ret1.getS());			
			
			assertTrue("Nonvirtual short Method2 with short then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual short Method2 with short then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual short Method2 with short then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual short Method2 with short then string array argument returned a wrong value", arg.getS() == ret2.getS());			
			
			assertTrue("Nonvirtual short Method3 with short, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual short Method3 with short, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual short Method3 with short, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual short Method3 with short, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual short Method3 with short, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual short Method3 with short, string array then string argument returned a wrong value", arg.getS() == ret3.getS());			
			
			assertTrue("Nonvirtual short Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual short Method4 with many arguments returned a wrong value", arg.getS() == ret4.getS());
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodA(INT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual int Method1 with int argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual int Method1 with int argument returned a wrong value", arg.getI() == ret1.getI());			
			
			assertTrue("Nonvirtual int Method2 with int then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual int Method2 with int then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual int Method2 with int then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual int Method2 with int then string array argument returned a wrong value", arg.getI() == ret2.getI());			
			
			assertTrue("Nonvirtual int Method3 with int, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual int Method3 with int, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual int Method3 with int, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual int Method3 with int, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual int Method3 with int, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual int Method3 with int, string array then string argument returned a wrong value", arg.getI() == ret3.getI());			
			
			assertTrue("Nonvirtual int Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual int Method4 with many arguments returned a wrong value", arg.getI() == ret4.getI());
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodA(LONG, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual long Method1 with long argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual long Method1 with long argument returned a wrong value", arg.getJ() == ret1.getJ());			
			
			assertTrue("Nonvirtual long Method2 with long then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual long Method2 with long then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual long Method2 with long then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual long Method2 with long then string array argument returned a wrong value", arg.getJ() == ret2.getJ());			
			
			assertTrue("Nonvirtual long Method3 with long, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual long Method3 with long, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual long Method3 with long, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual long Method3 with long, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual long Method3 with long, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual long Method3 with long, string array then string argument returned a wrong value", arg.getJ() == ret3.getJ());			
			
			assertTrue("Nonvirtual long Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual long Method4 with many arguments returned a wrong value", arg.getJ() == ret4.getJ());
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodA(FLOAT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual float Method1 with float argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual float Method1 with float argument returned a wrong value", arg.getF() == ret1.getF());			
			
			assertTrue("Nonvirtual float Method2 with float then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual float Method2 with float then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual float Method2 with float then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual float Method2 with float then string array argument returned a wrong value", arg.getF() == ret2.getF());			
			
			assertTrue("Nonvirtual float Method3 with float, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual float Method3 with float, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual float Method3 with float, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual float Method3 with float, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual float Method3 with float, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual float Method3 with float, string array then string argument returned a wrong value", arg.getF() == ret3.getF());			
			
			assertTrue("Nonvirtual float Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual float Method4 with many arguments returned a wrong value", arg.getF() == ret4.getF());
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodA(DOUBLE, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual double Method1 with double argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual double Method1 with double argument returned a wrong value", arg.getD() == ret1.getD());			
			
			assertTrue("Nonvirtual double Method2 with double then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual double Method2 with double then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual double Method2 with double then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual double Method2 with double then string array argument returned a wrong value", arg.getD() == ret2.getD());			
			
			assertTrue("Nonvirtual double Method3 with double, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual double Method3 with double, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual double Method3 with double, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual double Method3 with double, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual double Method3 with double, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual double Method3 with double, string array then string argument returned a wrong value", arg.getD() == ret3.getD());			
			
			assertTrue("Nonvirtual double Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual double Method4 with many arguments returned a wrong value", arg.getD() == ret4.getD());
		
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
		} catch (Exception e) {
			fail("Exception occurred in testCallNonvirtualMethodA:" + e.toString());
		}
	}
	
	/**
	 * tests CallNonvirtualMethodV
	 */
	public void testCallNonvirtualMethodV(){
		TestNativesChild testClass = new TestNativesChild(0);
		
		JValue arg = new JValue();
		arg.setL(new TestNatives(100));
		arg.setZ(true);
		arg.setB((byte)1);
		arg.setC('a');
		arg.setS((short)2);
		arg.setI(3);
		arg.setJ(4L);
		arg.setF(5.5f);
		arg.setD(6.6d);
		
		JValue ret1 = new JValue();
		JValue ret2 = new JValue();
		JValue ret3 = new JValue();
		JValue ret4 = new JValue();
		
		String[] tempStrArr = {"string1", "string2", "string3"};
		
		try {
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodV(VOID, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual void Method1 with no arguments did not execute correctly", testClass.getStrArr1() == null);
			
			assertTrue("Nonvirtual void Method2 with string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual void Method2 with string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual void Method2 with string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));
			
			assertTrue("Nonvirtual void Method3 with string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual void Method3 with string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual void Method3 with string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual void Method3 with string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual void Method3 with string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			
			assertTrue("Nonvirtual void Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			
			assertTrue("Virtual method invoked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodV(OBJECT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual object Method1 with object argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual object Method1 with object argument returned a wrong value", arg.getL().getIntField() == ret1.getL().getIntField());			
			
			assertTrue("Nonvirtual object Method2 with object then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual object Method2 with object then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual object Method2 with object then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual object Method2 with object then string array argument returned a wrong value", arg.getL().getIntField() == ret2.getL().getIntField());			
			
			assertTrue("Nonvirtual object Method3 with object, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual object Method3 with object, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual object Method3 with object, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual object Method3 with object, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual object Method3 with object, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual object Method3 with object, string array then string argument returned a wrong value", arg.getL().getIntField() == ret3.getL().getIntField());			
			
			assertTrue("Nonvirtual object Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual object Method4 with many arguments returned a wrong value", arg.getL().getIntField() == ret4.getL().getIntField());	
			
			assertTrue("Virtual method invloked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodV(BOOLEAN, arg, ret1, ret2, ret3, ret4);			
			
			assertTrue("Nonvirtual boolean Method1 with boolean argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual boolean Method1 with boolean argument returned a wrong value", arg.getZ() == ret1.getZ());			
			
			assertTrue("Nonvirtual boolean Method2 with boolean then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual boolean Method2 with boolean then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual boolean Method2 with boolean then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual boolean Method2 with boolean then string array argument returned a wrong value", arg.getZ() == ret2.getZ());			
			
			assertTrue("Nonvirtual boolean Method3 with boolean, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual boolean Method3 with boolean, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual boolean Method3 with boolean, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual boolean Method3 with boolean, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual boolean Method3 with boolean, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual boolean Method3 with boolean, string array then string argument returned a wrong value", arg.getZ() == ret3.getZ());			
			
			assertTrue("Nonvirtual boolean Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual boolean Method4 with many arguments returned a wrong value", arg.getZ() == ret4.getZ());
			
			assertTrue("Virtual method invloked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodV(BYTE, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual byte Method1 with byte argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual byte Method1 with byte argument returned a wrong value", arg.getB() == ret1.getB());			
			
			assertTrue("Nonvirtual byte Method2 with byte then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual byte Method2 with byte then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual byte Method2 with byte then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual byte Method2 with byte then string array argument returned a wrong value", arg.getB() == ret2.getB());			
			
			assertTrue("Nonvirtual byte Method3 with byte, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual byte Method3 with byte, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual byte Method3 with byte, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual byte Method3 with byte, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual byte Method3 with byte, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual byte Method3 with byte, string array then string argument returned a wrong value", arg.getB() == ret3.getB());			
			
			assertTrue("Nonvirtual byte Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual byte Method4 with many arguments returned a wrong value", arg.getB() == ret4.getB());
			
			assertTrue("Virtual method invloked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodV(CHAR, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual char Method1 with char argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual char Method1 with char argument returned a wrong value", arg.getC() == ret1.getC());			
			
			assertTrue("Nonvirtual char Method2 with char then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual char Method2 with char then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual char Method2 with char then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual char Method2 with char then string array argument returned a wrong value", arg.getC() == ret2.getC());			
			
			assertTrue("Nonvirtual char Method3 with char, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual char Method3 with char, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual char Method3 with char, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual char Method3 with char, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual char Method3 with char, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual char Method3 with char, string array then string argument returned a wrong value", arg.getC() == ret3.getC());			
			
			assertTrue("Nonvirtual char Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual char Method4 with many arguments returned a wrong value", arg.getC() == ret4.getC());	
			
			assertTrue("Virtual method invloked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodV(SHORT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual short Method1 with short argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual short Method1 with short argument returned a wrong value", arg.getS() == ret1.getS());			
			
			assertTrue("Nonvirtual short Method2 with short then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual short Method2 with short then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual short Method2 with short then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual short Method2 with short then string array argument returned a wrong value", arg.getS() == ret2.getS());			
			
			assertTrue("Nonvirtual short Method3 with short, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual short Method3 with short, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual short Method3 with short, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual short Method3 with short, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual short Method3 with short, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual short Method3 with short, string array then string argument returned a wrong value", arg.getS() == ret3.getS());			
			
			assertTrue("Nonvirtual short Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual short Method4 with many arguments returned a wrong value", arg.getS() == ret4.getS());
			
			assertTrue("Virtual method invloked", testClass.getIsChildInvoked() == false);
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodV(INT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual int Method1 with int argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual int Method1 with int argument returned a wrong value", arg.getI() == ret1.getI());			
			
			assertTrue("Nonvirtual int Method2 with int then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual int Method2 with int then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual int Method2 with int then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual int Method2 with int then string array argument returned a wrong value", arg.getI() == ret2.getI());			
			
			assertTrue("Nonvirtual int Method3 with int, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual int Method3 with int, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual int Method3 with int, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual int Method3 with int, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual int Method3 with int, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual int Method3 with int, string array then string argument returned a wrong value", arg.getI() == ret3.getI());			
			
			assertTrue("Nonvirtual int Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual int Method4 with many arguments returned a wrong value", arg.getI() == ret4.getI());
			
			assertTrue("Virtual method invloked", testClass.getIsChildInvoked() == false);		
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodV(LONG, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual long Method1 with long argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual long Method1 with long argument returned a wrong value", arg.getJ() == ret1.getJ());			
			
			assertTrue("Nonvirtual long Method2 with long then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual long Method2 with long then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual long Method2 with long then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual long Method2 with long then string array argument returned a wrong value", arg.getJ() == ret2.getJ());			
			
			assertTrue("Nonvirtual long Method3 with long, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual long Method3 with long, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual long Method3 with long, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual long Method3 with long, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual long Method3 with long, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual long Method3 with long, string array then string argument returned a wrong value", arg.getJ() == ret3.getJ());			
			
			assertTrue("Nonvirtual long Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual long Method4 with many arguments returned a wrong value", arg.getJ() == ret4.getJ());
			
			assertTrue("Virtual method invloked", testClass.getIsChildInvoked() == false);				
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodV(FLOAT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual float Method1 with float argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual float Method1 with float argument returned a wrong value", arg.getF() == ret1.getF());			
			
			assertTrue("Nonvirtual float Method2 with float then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual float Method2 with float then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual float Method2 with float then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual float Method2 with float then string array argument returned a wrong value", arg.getF() == ret2.getF());			
			
			assertTrue("Nonvirtual float Method3 with float, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual float Method3 with float, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual float Method3 with float, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual float Method3 with float, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual float Method3 with float, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual float Method3 with float, string array then string argument returned a wrong value", arg.getF() == ret3.getF());			
			
			assertTrue("Nonvirtual float Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual float Method4 with many arguments returned a wrong value", arg.getF() == ret4.getF());
			
			assertTrue("Virtual method invloked", testClass.getIsChildInvoked() == false);	
			
			testClass.setStrArr1(tempStrArr);
			testClass.setStrArr2(null);
			testClass.setStrArr3(null);
			testClass.setStrArr4(tempStrArr);
			testClass.setStr(null);
			testClass.setIsChildInvoked(false);
			testClass.testCallNonvirtualMethodV(DOUBLE, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Nonvirtual double Method1 with double argument did not execute correctly", testClass.getStrArr1() == null);
			assertTrue("Nonvirtual double Method1 with double argument returned a wrong value", arg.getD() == ret1.getD());			
			
			assertTrue("Nonvirtual double Method2 with double then string array argument did not execute correctly", testClass.getStrArr2() != null);
			assertTrue("Nonvirtual double Method2 with double then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
			assertTrue("Nonvirtual double Method2 with double then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
			assertTrue("Nonvirtual double Method2 with double then string array argument returned a wrong value", arg.getD() == ret2.getD());			
			
			assertTrue("Nonvirtual double Method3 with double, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
			assertTrue("Nonvirtual double Method3 with double, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
			assertTrue("Nonvirtual double Method3 with double, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
			assertTrue("Nonvirtual double Method3 with double, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
			assertTrue("Nonvirtual double Method3 with double, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
			assertTrue("Nonvirtual double Method3 with double, string array then string argument returned a wrong value", arg.getD() == ret3.getD());			
			
			assertTrue("Nonvirtual double Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
			assertTrue("Nonvirtual double Method4 with many arguments returned a wrong value", arg.getD() == ret4.getD());
		
			assertTrue("Virtual method invloked", testClass.getIsChildInvoked() == false);
		} catch (Exception e) {
			fail("Exception occurred in testCallNonvirtualMethodV:" + e.toString());
		}
	}
	
	/**
	 * tests CallStaticMethod
	 */
	public void testCallStaticMethod() {
		TestNatives testClass = new TestNatives(0);
		
		JValue arg = new JValue();
		arg.setL(new TestNatives(100));
		arg.setZ(true);
		arg.setB((byte)1);
		arg.setC('a');
		arg.setS((short)2);
		arg.setI(3);
		arg.setJ(4L);
		arg.setF(5.5f);
		arg.setD(6.6d);
		
		JValue ret1 = new JValue();
		JValue ret2 = new JValue();
		JValue ret3 = new JValue();
		JValue ret4 = new JValue();
		
		String[] tempStrArr = {"string1", "string2", "string3"};
		
		try {
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethod(VOID, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Void Method1 with no arguments did not execute correctly", TestNatives.staticStrArr1 == null);
			
			assertTrue("Static Void Method2 with string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Void Method2 with string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Void Method2 with string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));
			
			assertTrue("Static Void Method3 with string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Void Method3 with string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Void Method3 with string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Void Method3 with string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Void Method3 with string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			
			assertTrue("Static Void Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethod(OBJECT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Object Method1 with object argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Object Method1 with object argument returned a wrong value", arg.getL().getIntField() == ret1.getL().getIntField());			
			
			assertTrue("Static Object Method2 with object then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Object Method2 with object then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Object Method2 with object then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Object Method2 with object then string array argument returned a wrong value", arg.getL().getIntField() == ret2.getL().getIntField());			
			
			assertTrue("Static Object Method3 with object, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Object Method3 with object, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Object Method3 with object, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Object Method3 with object, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Object Method3 with object, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Object Method3 with object, string array then string argument returned a wrong value", arg.getL().getIntField() == ret3.getL().getIntField());			
			
			assertTrue("Static Object Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Object Method4 with many arguments returned a wrong value", arg.getL().getIntField() == ret4.getL().getIntField());	
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethod(BOOLEAN, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Boolean Method1 with boolean argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Boolean Method1 with boolean argument returned a wrong value", arg.getZ() == ret1.getZ());			
			
			assertTrue("Static Boolean Method2 with boolean then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Boolean Method2 with boolean then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Boolean Method2 with boolean then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Boolean Method2 with boolean then string array argument returned a wrong value", arg.getZ() == ret2.getZ());			
			
			assertTrue("Static Boolean Method3 with boolean, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Boolean Method3 with boolean, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Boolean Method3 with boolean, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Boolean Method3 with boolean, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Boolean Method3 with boolean, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Boolean Method3 with boolean, string array then string argument returned a wrong value", arg.getZ() == ret3.getZ());			
			
			assertTrue("Static Boolean Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Boolean Method4 with many arguments returned a wrong value", arg.getZ() == ret4.getZ());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethod(BYTE, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Byte Method1 with byte argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Byte Method1 with byte argument returned a wrong value", arg.getB() == ret1.getB());		
			
			assertTrue("Static Byte Method2 with byte then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Byte Method2 with byte then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Byte Method2 with byte then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Byte Method2 with byte then string array argument returned a wrong value", arg.getB() == ret2.getB());			
			
			assertTrue("Static Byte Method3 with byte, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Byte Method3 with byte, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Byte Method3 with byte, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Byte Method3 with byte, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Byte Method3 with byte, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Byte Method3 with byte, string array then string argument returned a wrong value", arg.getB() == ret3.getB());			
			
			assertTrue("Static Byte Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Byte Method4 with many arguments returned a wrong value", arg.getB() == ret4.getB());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethod(CHAR, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Char Method1 with char argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Char Method1 with char argument returned a wrong value", arg.getC() == ret1.getC());	
			
			assertTrue("Static Char Method2 with char then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Char Method2 with char then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Char Method2 with char then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Char Method2 with char then string array argument returned a wrong value", arg.getC() == ret2.getC());			
			
			assertTrue("Static Char Method3 with char, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Char Method3 with char, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Char Method3 with char, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Char Method3 with char, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Char Method3 with char, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Char Method3 with char, string array then string argument returned a wrong value", arg.getC() == ret3.getC());			
			
			assertTrue("Static Char Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Char Method4 with many arguments returned a wrong value", arg.getC() == ret4.getC());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethod(SHORT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Short Method1 with short argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Short Method1 with short argument returned a wrong value", arg.getS() == ret1.getS());	
			
			assertTrue("Static Short Method2 with short then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Short Method2 with short then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Short Method2 with short then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Short Method2 with short then string array argument returned a wrong value", arg.getS() == ret2.getS());			
			
			assertTrue("Static Short Method3 with short, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Short Method3 with short, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Short Method3 with short, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Short Method3 with short, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Short Method3 with short, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Short Method3 with short, string array then string argument returned a wrong value", arg.getS() == ret3.getS());			
			
			assertTrue("Static Short Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Short Method4 with many arguments returned a wrong value", arg.getS() == ret4.getS());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethod(INT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Int Method1 with int argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Int Method1 with int argument returned a wrong value", arg.getI() == ret1.getI());			
			
			assertTrue("Static Int Method2 with int then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Int Method2 with int then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Int Method2 with int then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Int Method2 with int then string array argument returned a wrong value", arg.getI() == ret2.getI());			
			
			assertTrue("Static Int Method3 with int, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Int Method3 with int, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Int Method3 with int, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Int Method3 with int, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Int Method3 with int, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Int Method3 with int, string array then string argument returned a wrong value", arg.getI() == ret3.getI());			
			
			assertTrue("Static Int Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Int Method4 with many arguments returned a wrong value", arg.getI() == ret4.getI());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethod(LONG, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Long Method1 with long argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Long Method1 with long argument returned a wrong value", arg.getJ() == ret1.getJ());			
			
			assertTrue("Static Long Method2 with long then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Long Method2 with long then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Long Method2 with long then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Long Method2 with long then string array argument returned a wrong value", arg.getJ() == ret2.getJ());			
			
			assertTrue("Static Long Method3 with long, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Long Method3 with long, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Long Method3 with long, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Long Method3 with long, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Long Method3 with long, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Long Method3 with long, string array then string argument returned a wrong value", arg.getJ() == ret3.getJ());			
			
			assertTrue("Static Long Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Long Method4 with many arguments returned a wrong value", arg.getJ() == ret4.getJ());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethod(FLOAT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Float Method1 with float argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Float Method1 with float argument returned a wrong value", arg.getF() == ret1.getF());			
			
			assertTrue("Static Float Method2 with float then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Float Method2 with float then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Float Method2 with float then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Float Method2 with float then string array argument returned a wrong value", arg.getF() == ret2.getF());			
			
			assertTrue("Static Float Method3 with float, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Float Method3 with float, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Float Method3 with float, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Float Method3 with float, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Float Method3 with float, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Float Method3 with float, string array then string argument returned a wrong value", arg.getF() == ret3.getF());			
			
			assertTrue("Static Float Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Float Method4 with many arguments returned a wrong value", arg.getF() == ret4.getF());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethod(DOUBLE, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Double Method1 with double argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Double Method1 with double argument returned a wrong value", arg.getD() == ret1.getD());			
			
			assertTrue("Static Double Method2 with double then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Double Method2 with double then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Double Method2 with double then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Double Method2 with double then string array argument returned a wrong value", arg.getD() == ret2.getD());			
			
			assertTrue("Static Double Method3 with double, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Double Method3 with double, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Double Method3 with double, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Double Method3 with double, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Double Method3 with double, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Double Method3 with double, string array then string argument returned a wrong value", arg.getD() == ret3.getD());			
			
			assertTrue("Static Double Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Double Method4 with many arguments returned a wrong value",  arg.getD() == ret4.getD());
		} catch (Exception e) {
			fail("Exception occurred in testCallStaticMethod:" + e.toString());
		}
	}
	
	/**
	 * tests CallStaticMethodA
	 */
	public void testCallStaticMethodA() {
		TestNatives testClass = new TestNatives(0);
		
		JValue arg = new JValue();
		arg.setL(new TestNatives(100));
		arg.setZ(true);
		arg.setB((byte)1);
		arg.setC('a');
		arg.setS((short)2);
		arg.setI(3);
		arg.setJ(4L);
		arg.setF(5.5f);
		arg.setD(6.6d);
		
		JValue ret1 = new JValue();
		JValue ret2 = new JValue();
		JValue ret3 = new JValue();
		JValue ret4 = new JValue();
		
		String[] tempStrArr = {"string1", "string2", "string3"};
		
		try {
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodA(VOID, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Void Method1 with no arguments did not execute correctly", TestNatives.staticStrArr1 == null);
			
			assertTrue("Static Void Method2 with string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Void Method2 with string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Void Method2 with string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));
			
			assertTrue("Static Void Method3 with string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Void Method3 with string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Void Method3 with string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Void Method3 with string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Void Method3 with string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			
			assertTrue("Static Void Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodA(OBJECT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Object Method1 with object argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Object Method1 with object argument returned a wrong value", arg.getL().getIntField() == ret1.getL().getIntField());			
			
			assertTrue("Static Object Method2 with object then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Object Method2 with object then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Object Method2 with object then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Object Method2 with object then string array argument returned a wrong value", arg.getL().getIntField() == ret2.getL().getIntField());			
			
			assertTrue("Static Object Method3 with object, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Object Method3 with object, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Object Method3 with object, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Object Method3 with object, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Object Method3 with object, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Object Method3 with object, string array then string argument returned a wrong value", arg.getL().getIntField() == ret3.getL().getIntField());			
			
			assertTrue("Static Object Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Object Method4 with many arguments returned a wrong value", arg.getL().getIntField() == ret4.getL().getIntField());	
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodA(BOOLEAN, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Boolean Method1 with boolean argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Boolean Method1 with boolean argument returned a wrong value", arg.getZ() == ret1.getZ());			
			
			assertTrue("Static Boolean Method2 with boolean then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Boolean Method2 with boolean then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Boolean Method2 with boolean then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Boolean Method2 with boolean then string array argument returned a wrong value", arg.getZ() == ret2.getZ());			
			
			assertTrue("Static Boolean Method3 with boolean, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Boolean Method3 with boolean, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Boolean Method3 with boolean, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Boolean Method3 with boolean, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Boolean Method3 with boolean, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Boolean Method3 with boolean, string array then string argument returned a wrong value", arg.getZ() == ret3.getZ());			
			
			assertTrue("Static Boolean Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Boolean Method4 with many arguments returned a wrong value", arg.getZ() == ret4.getZ());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodA(BYTE, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Byte Method1 with byte argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Byte Method1 with byte argument returned a wrong value", arg.getB() == ret1.getB());		
			
			assertTrue("Static Byte Method2 with byte then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Byte Method2 with byte then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Byte Method2 with byte then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Byte Method2 with byte then string array argument returned a wrong value", arg.getB() == ret2.getB());			
			
			assertTrue("Static Byte Method3 with byte, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Byte Method3 with byte, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Byte Method3 with byte, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Byte Method3 with byte, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Byte Method3 with byte, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Byte Method3 with byte, string array then string argument returned a wrong value", arg.getB() == ret3.getB());			
			
			assertTrue("Static Byte Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Byte Method4 with many arguments returned a wrong value", arg.getB() == ret4.getB());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodA(CHAR, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Char Method1 with char argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Char Method1 with char argument returned a wrong value", arg.getC() == ret1.getC());	
			
			assertTrue("Static Char Method2 with char then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Char Method2 with char then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Char Method2 with char then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Char Method2 with char then string array argument returned a wrong value", arg.getC() == ret2.getC());			
			
			assertTrue("Static Char Method3 with char, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Char Method3 with char, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Char Method3 with char, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Char Method3 with char, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Char Method3 with char, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Char Method3 with char, string array then string argument returned a wrong value", arg.getC() == ret3.getC());			
			
			assertTrue("Static Char Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Char Method4 with many arguments returned a wrong value", arg.getC() == ret4.getC());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodA(SHORT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Short Method1 with short argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Short Method1 with short argument returned a wrong value", arg.getS() == ret1.getS());	
			
			assertTrue("Static Short Method2 with short then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Short Method2 with short then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Short Method2 with short then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Short Method2 with short then string array argument returned a wrong value", arg.getS() == ret2.getS());			
			
			assertTrue("Static Short Method3 with short, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Short Method3 with short, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Short Method3 with short, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Short Method3 with short, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Short Method3 with short, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Short Method3 with short, string array then string argument returned a wrong value", arg.getS() == ret3.getS());			
			
			assertTrue("Static Short Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Short Method4 with many arguments returned a wrong value", arg.getS() == ret4.getS());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodA(INT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Int Method1 with int argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Int Method1 with int argument returned a wrong value", arg.getI() == ret1.getI());			
			
			assertTrue("Static Int Method2 with int then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Int Method2 with int then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Int Method2 with int then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Int Method2 with int then string array argument returned a wrong value", arg.getI() == ret2.getI());			
			
			assertTrue("Static Int Method3 with int, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Int Method3 with int, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Int Method3 with int, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Int Method3 with int, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Int Method3 with int, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Int Method3 with int, string array then string argument returned a wrong value", arg.getI() == ret3.getI());			
			
			assertTrue("Static Int Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Int Method4 with many arguments returned a wrong value", arg.getI() == ret4.getI());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodA(LONG, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Long Method1 with long argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Long Method1 with long argument returned a wrong value", arg.getJ() == ret1.getJ());			
			
			assertTrue("Static Long Method2 with long then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Long Method2 with long then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Long Method2 with long then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Long Method2 with long then string array argument returned a wrong value", arg.getJ() == ret2.getJ());			
			
			assertTrue("Static Long Method3 with long, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Long Method3 with long, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Long Method3 with long, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Long Method3 with long, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Long Method3 with long, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Long Method3 with long, string array then string argument returned a wrong value", arg.getJ() == ret3.getJ());			
			
			assertTrue("Static Long Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Long Method4 with many arguments returned a wrong value", arg.getJ() == ret3.getJ());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodA(FLOAT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Float Method1 with float argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Float Method1 with float argument returned a wrong value", arg.getF() == ret1.getF());			
			
			assertTrue("Static Float Method2 with float then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Float Method2 with float then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Float Method2 with float then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Float Method2 with float then string array argument returned a wrong value", arg.getF() == ret2.getF());			
			
			assertTrue("Static Float Method3 with float, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Float Method3 with float, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Float Method3 with float, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Float Method3 with float, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Float Method3 with float, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Float Method3 with float, string array then string argument returned a wrong value", arg.getF() == ret3.getF());			
			
			assertTrue("Static Float Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Float Method4 with many arguments returned a wrong value", arg.getF() == ret4.getF());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodA(DOUBLE, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Double Method1 with double argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Double Method1 with double argument returned a wrong value", arg.getD() == ret1.getD());			
			
			assertTrue("Static Double Method2 with double then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Double Method2 with double then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Double Method2 with double then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Double Method2 with double then string array argument returned a wrong value", arg.getD() == ret2.getD());			
			
			assertTrue("Static Double Method3 with double, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Double Method3 with double, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Double Method3 with double, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Double Method3 with double, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Double Method3 with double, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Double Method3 with double, string array then string argument returned a wrong value", arg.getD() == ret3.getD());			
			
			assertTrue("Static Double Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Double Method4 with many arguments returned a wrong value",  arg.getD() == ret4.getD());
		} catch (Exception e) {
			fail("Exception occurred in testCallStaticMethodA:" + e.toString());
		}
	}
		
	/**
	 * tests CallStaticMethodV
	 */
	public void testCallStaticMethodV(){
		TestNatives testClass = new TestNatives(0);
		
		JValue arg = new JValue();
		arg.setL(new TestNatives(100));
		arg.setZ(true);
		arg.setB((byte)1);
		arg.setC('a');
		arg.setS((short)2);
		arg.setI(3);
		arg.setJ(4L);
		arg.setF(5.5f);
		arg.setD(6.6d);
		
		JValue ret1 = new JValue();
		JValue ret2 = new JValue();
		JValue ret3 = new JValue();
		JValue ret4 = new JValue();
		
		String[] tempStrArr = {"string1", "string2", "string3"};
		
		try {
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodV(VOID, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Void Method1 with no arguments did not execute correctly", TestNatives.staticStrArr1 == null);
			
			assertTrue("Static Void Method2 with string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Void Method2 with string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Void Method2 with string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));
			
			assertTrue("Static Void Method3 with string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Void Method3 with string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Void Method3 with string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Void Method3 with string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Void Method3 with string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			
			assertTrue("Static Void Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodV(OBJECT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Object Method1 with object argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Object Method1 with object argument returned a wrong value", arg.getL().getIntField() == ret1.getL().getIntField());			
			
			assertTrue("Static Object Method2 with object then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Object Method2 with object then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Object Method2 with object then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Object Method2 with object then string array argument returned a wrong value", arg.getL().getIntField() == ret2.getL().getIntField());			
			
			assertTrue("Static Object Method3 with object, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Object Method3 with object, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Object Method3 with object, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Object Method3 with object, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Object Method3 with object, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Object Method3 with object, string array then string argument returned a wrong value", arg.getL().getIntField() == ret3.getL().getIntField());			
			
			assertTrue("Static Object Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Object Method4 with many arguments returned a wrong value", arg.getL().getIntField() == ret4.getL().getIntField());	
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodV(BOOLEAN, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Boolean Method1 with boolean argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Boolean Method1 with boolean argument returned a wrong value", arg.getZ() == ret1.getZ());			
			
			assertTrue("Static Boolean Method2 with boolean then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Boolean Method2 with boolean then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Boolean Method2 with boolean then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Boolean Method2 with boolean then string array argument returned a wrong value", arg.getZ() == ret2.getZ());			
			
			assertTrue("Static Boolean Method3 with boolean, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Boolean Method3 with boolean, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Boolean Method3 with boolean, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Boolean Method3 with boolean, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Boolean Method3 with boolean, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Boolean Method3 with boolean, string array then string argument returned a wrong value", arg.getZ() == ret3.getZ());			
			
			assertTrue("Static Boolean Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Boolean Method4 with many arguments returned a wrong value", arg.getZ() == ret4.getZ());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodV(BYTE, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Byte Method1 with byte argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Byte Method1 with byte argument returned a wrong value", arg.getB() == ret1.getB());		
			
			assertTrue("Static Byte Method2 with byte then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Byte Method2 with byte then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Byte Method2 with byte then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Byte Method2 with byte then string array argument returned a wrong value", arg.getB() == ret2.getB());			
			
			assertTrue("Static Byte Method3 with byte, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Byte Method3 with byte, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Byte Method3 with byte, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Byte Method3 with byte, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Byte Method3 with byte, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Byte Method3 with byte, string array then string argument returned a wrong value", arg.getB() == ret3.getB());			
			
			assertTrue("Static Byte Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Byte Method4 with many arguments returned a wrong value", arg.getB() == ret4.getB());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodV(CHAR, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Char Method1 with char argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Char Method1 with char argument returned a wrong value", arg.getC() == ret1.getC());	
			
			assertTrue("Static Char Method2 with char then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Char Method2 with char then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Char Method2 with char then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Char Method2 with char then string array argument returned a wrong value", arg.getC() == ret2.getC());			
			
			assertTrue("Static Char Method3 with char, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Char Method3 with char, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Char Method3 with char, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Char Method3 with char, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Char Method3 with char, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Char Method3 with char, string array then string argument returned a wrong value", arg.getC() == ret3.getC());			
			
			assertTrue("Static Char Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Char Method4 with many arguments returned a wrong value", arg.getC() == ret4.getC());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodV(SHORT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Short Method1 with short argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Short Method1 with short argument returned a wrong value", arg.getS() == ret1.getS());	
			
			assertTrue("Static Short Method2 with short then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Short Method2 with short then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Short Method2 with short then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Short Method2 with short then string array argument returned a wrong value", arg.getS() == ret2.getS());			
			
			assertTrue("Static Short Method3 with short, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Short Method3 with short, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Short Method3 with short, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Short Method3 with short, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Short Method3 with short, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Short Method3 with short, string array then string argument returned a wrong value", arg.getS() == ret3.getS());			
			
			assertTrue("Static Short Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Short Method4 with many arguments returned a wrong value", arg.getS() == ret4.getS());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodV(INT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Int Method1 with int argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Int Method1 with int argument returned a wrong value", arg.getI() == ret1.getI());			
			
			assertTrue("Static Int Method2 with int then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Int Method2 with int then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Int Method2 with int then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Int Method2 with int then string array argument returned a wrong value", arg.getI() == ret2.getI());			
			
			assertTrue("Static Int Method3 with int, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Int Method3 with int, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Int Method3 with int, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Int Method3 with int, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Int Method3 with int, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Int Method3 with int, string array then string argument returned a wrong value", arg.getI() == ret3.getI());			
			
			assertTrue("Static Int Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Int Method4 with many arguments returned a wrong value", arg.getI() == ret4.getI());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodV(LONG, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Long Method1 with long argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Long Method1 with long argument returned a wrong value", arg.getJ() == ret1.getJ());			
			
			assertTrue("Static Long Method2 with long then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Long Method2 with long then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Long Method2 with long then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Long Method2 with long then string array argument returned a wrong value", arg.getJ() == ret2.getJ());			
			
			assertTrue("Static Long Method3 with long, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Long Method3 with long, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Long Method3 with long, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Long Method3 with long, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Long Method3 with long, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Long Method3 with long, string array then string argument returned a wrong value", arg.getJ() == ret3.getJ());			
			
			assertTrue("Static Long Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Long Method4 with many arguments returned a wrong value", arg.getJ() == ret3.getJ());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodV(FLOAT, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Float Method1 with float argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Float Method1 with float argument returned a wrong value", arg.getF() == ret1.getF());			
			
			assertTrue("Static Float Method2 with float then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Float Method2 with float then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Float Method2 with float then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Float Method2 with float then string array argument returned a wrong value", arg.getF() == ret2.getF());			
			
			assertTrue("Static Float Method3 with float, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Float Method3 with float, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Float Method3 with float, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Float Method3 with float, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Float Method3 with float, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Float Method3 with float, string array then string argument returned a wrong value", arg.getF() == ret3.getF());			
			
			assertTrue("Static Float Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Float Method4 with many arguments returned a wrong value", arg.getF() == ret4.getF());
			
			TestNatives.staticStrArr1 = tempStrArr;
			TestNatives.staticStrArr2 = null;
			TestNatives.staticStrArr3 = null;
			TestNatives.staticStrArr4 = tempStrArr;
			TestNatives.staticStr = null;
			testClass.testCallStaticMethodV(DOUBLE, arg, ret1, ret2, ret3, ret4);
			
			assertTrue("Static Double Method1 with double argument did not execute correctly", TestNatives.staticStrArr1 == null);
			assertTrue("Static Double Method1 with double argument returned a wrong value", arg.getD() == ret1.getD());			
			
			assertTrue("Static Double Method2 with double then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
			assertTrue("Static Double Method2 with double then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
			assertTrue("Static Double Method2 with double then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
			assertTrue("Static Double Method2 with double then string array argument returned a wrong value", arg.getD() == ret2.getD());			
			
			assertTrue("Static Double Method3 with double, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
			assertTrue("Static Double Method3 with double, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
			assertTrue("Static Double Method3 with double, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
			assertTrue("Static Double Method3 with double, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
			assertTrue("Static Double Method3 with double, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
			assertTrue("Static Double Method3 with double, string array then string argument returned a wrong value", arg.getD() == ret3.getD());			
			
			assertTrue("Static Double Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
			assertTrue("Static Double Method4 with many arguments returned a wrong value",  arg.getD() == ret4.getD());
		} catch (Exception e) {
			fail("Exception occurred in testCallStaticMethodV:" + e.toString());
		}
	}
		
	/**
	 * tests DefineClass
	 */
	public void testDefineClass(){
		TestNatives testClass = new TestNatives(0);
		try {
			String source = "com/ibm/j9/offload/tests/jniservice/HelloWorld.class";
			ClassLoader loader = this.getClass().getClassLoader();
			InputStream is = loader.getResourceAsStream(source);
	        byte[] bytes = new byte[is.available()];
	        int offset = 0;
	        int numRead = 0;
	        while (offset < bytes.length && (numRead=is.read(bytes, offset, bytes.length-offset)) >= 0) {
	            offset += numRead;
	        }
	        if (offset < bytes.length) {
	            throw new IOException("Could not completely read raw class data: " + source);
	        }
	        is.close();
			Class result = testClass.testDefineClass("com/ibm/j9/offload/tests/jniservice/HelloWorld", loader, bytes);
			assertTrue("Incorrect DefineClass invocation", result != null);
			assertTrue("Class name defined not correct",  result.getName() == "com.ibm.j9.offload.tests.jniservice.HelloWorld");
		} catch (Exception e) {
			fail("Exception occurred in testDefineClass:" + e.toString());
		}
	}
	
	/**
	 * tests DeleteGlobalRef
	 */
	public void testDeleteGlobalRef(){
		TestNatives testClass = new TestNatives(0);
		try {
			testClass.testDeleteGlobalRef(1);
			testClass.testDeleteGlobalRef(2);
		} catch (Exception e){
			fail("Exception occurred in testDeleteGlobalRef:" + e.toString());
		}
	}
	
	/**
	 * tests DeleteLocalRef
	 */
	public void testDeleteLocalRef(){
		TestNatives testClass = new TestNatives(0);
		try {
			testClass.testDeleteLocalRef(1);
			testClass.testDeleteLocalRef(2);
		} catch (Exception e){
			fail("Exception occurred in testDeleteLocalRef:" + e.toString());
		}
	}
	
	/**
	 * tests DeleteWeakGlobalRef
	 */
	public void testDeleteWeakGlobalRef(){
		TestNatives testClass = new TestNatives(0);
		try {
			testClass.testDeleteWeakGlobalRef(1);
			testClass.testDeleteWeakGlobalRef(2);
		} catch (Exception e){
			fail("Exception occurred in testDeleteWeakGlobalRef:" + e.toString());
		}
	}	
	
	/**
	 * tests EnsureCapacity
	 */	
	public void testEnsureLocalCapacity(){
		TestNatives testClass = new TestNatives(0);
		try {
			int result = testClass.testEnsureLocalCapacity(16);
			assertTrue("16 local references cannot be created in the current thread", result == 0);		
		} catch (Exception e) {
			fail("Exception occurred in testEnsureLocalCapacity:" + e.toString());
		}
	}
	
	/**
	 * tests ExceptionCheck
	 */
	public void testExceptionCheck(){
		TestNatives testClass = new TestNatives(1234);
		assertTrue("Exception Check did not indicate an exception pending when expected", 
					testClass.testExceptionCheck(TestNatives.testInterface.class));
		
		assertFalse("Exception Check indicated an exception pending when not expected", 
				testClass.testExceptionCheck(TestNatives.testClass.class));
	}
	
	/**
	 * tests ExceptionClear
	 */
	public void testExceptionClear(){
		TestNatives testClass = new TestNatives(1234);
		try {
			assertFalse("Exception Clear did not clear the exception as expected", 
					testClass.testExceptionClear(TestNatives.testInterface.class));
		} catch (InstantiationException e){
			fail("Exception Clear did not clear the exception as expected");
		}
	}
	
	/**
	 * tests ExceptionDescribe
	 */
	public void testExceptionDescribe(){
		TestNatives testClass = new TestNatives(0);
		try {
			System.out.println("\n***TESTING EXCEPTION DESCRIBE, WE EXPECT CONSOLE OUTPUT HERE showing exception");
			testClass.testExceptionDescribe();
			System.out.println("");
		} catch (Exception e){
			fail("Exception occurred in testExceptionDescribe:" + e.toString());
		}
	}
	
	/**
	 * tests ExceptionOccurred
	 */
	public void testExceptionOccurred(){
		TestNatives testClass = new TestNatives(0);
		Throwable t;
		try {
			t = testClass.testExceptionOccurred(TestNatives.testInterface.class);
			assertFalse("ExceptionOccurred did not throw the exception as expected", t == null);
			t = testClass.testExceptionOccurred(TestNatives.testClass.class);
			assertTrue("ExceptionOccurred threw an exception when not expected", t == null);		
		} catch (Exception e){
			fail("Exception occurred in testExceptionOccurred:" + e.toString());
		}
	}
	
	/**
	 * tests FatalError
	 */
	/*
	public void testFatalError(){
		TestNatives testClass = new TestNatives(0);
		try {
			int result = testClass.testFatalError();
			assertTrue("Incorrect FatalError invocation", result == 0);
		} catch (Exception e){
			fail("Exception occurred in testFatalError:" + e.toString());
		}
	}
	*/
	
	/**
	 * tests FindClass
	 */
	public void testFindClass(){
		// testClass.testFindClass(1)looks up the TestNatives class and returns the 
		// result for it
		// testClass.testFindClass(2) looks up a non-existent class
		TestNatives testClass = new TestNatives(0);
		try {
			assertTrue("Got wrong class", TestNatives.class == testClass.testFindClass(1) );
		} catch (Exception e){
			fail("Exception occurred in testFindClass:" + e.toString());
		}
		
		try {
			assertFalse("Got wrong class", JNIServiceTest.class == testClass.testFindClass(1) );
		} catch (Exception e){
			fail("Exception occurred in testFindClass:" + e.toString());
		}
		
		try {
			testClass.testFindClass(2);
			fail("Did not get exception looking up non-existant class");
		} catch(NoClassDefFoundError e){
			// this is the expected behavior
		} catch (Exception e){
			fail("Exception occurred in testFindClass:" + e.toString());
		}
	}
		
	/**
	 * tests FromReflectedField
	 */	
	public void testFromReflectedField(){
		TestNatives testClass = new TestNatives(0);
		Field field;
		int result;
		try {
			field = testClass.getClass().getField("intField");
			result = testClass.testFromReflectedField(field, 100, 1);
			assertTrue("Incorrect FromReflectedField invocation", result == 0);
			assertTrue("Field value not correct", testClass.getIntField() == 100);
			
			field = testClass.getClass().getField("staticIntField");
			result = testClass.testFromReflectedField(field, 200, 2);
			assertTrue("Incorrect FromReflectedField invocation", result == 0);
			assertTrue("Static field value not correct", TestNatives.staticIntField == 200);
		} catch (Exception e) {
			fail("Exception occurred in testFromReflectedField:" + e.toString());
		}
	}
	
	/**
	 * tests FromReflectedMethod
	 */
	public void testFromReflectedMethod(){
		TestNatives testClass = new TestNatives(0);
		Method[] methodArr;
		Class[] paramArr;
		String name;
		String upperCaseName;
		Method method;
		int isStatic;
		int returnType;
		int numOfMethodsForEachType = 4;
		int count = 0;
		int result;
		try {
			methodArr = testClass.getClass().getMethods();
			for(int i=0; i<methodArr.length; i++){
				if(methodArr[i].toString().toUpperCase().indexOf("STATIC") >= 0){
					isStatic = STATIC;
				} else {
					isStatic = NONSTATIC;
				}
				returnType = -1;
				name = methodArr[i].getName();
				upperCaseName = name.toUpperCase();
				paramArr = methodArr[i].getParameterTypes();
				if(upperCaseName.indexOf("VOIDMETHOD") >= 0) {
					returnType = VOID;
				} else if(upperCaseName.indexOf("OBJECTMETHOD") >= 0) {
					returnType = OBJECT;
				} else if(upperCaseName.indexOf("BOOLEANMETHOD") >= 0) {
					returnType = BOOLEAN;
				} else if(upperCaseName.indexOf("BYTEMETHOD") >= 0) {
					returnType = BYTE;
				} else if(upperCaseName.indexOf("CHARMETHOD") >= 0) {
					returnType = CHAR;
				} else if(upperCaseName.indexOf("SHORTMETHOD") >= 0) {
					returnType = SHORT;
				} else if(upperCaseName.indexOf("INTMETHOD") >= 0) {
					returnType = INT;
				} else if(upperCaseName.indexOf("LONGMETHOD") >= 0) {
					returnType = LONG;
				} else if(upperCaseName.indexOf("FLOATMETHOD") >= 0) {
					returnType = FLOAT;
				} else if(upperCaseName.indexOf("DOUBLEMETHOD") >= 0) {
					returnType = DOUBLE;
				}
				
				if(returnType != -1){
					if(++count > numOfMethodsForEachType){
						count %= numOfMethodsForEachType;
					}
					
					JValue arg = new JValue();
					arg.setL(new TestNatives(100));
					arg.setZ(true);
					arg.setB((byte)1);
					arg.setC('a');
					arg.setS((short)2);
					arg.setI(3);
					arg.setJ(4L);
					arg.setF(5.5f);
					arg.setD(6.6d);
					
					JValue ret1 = new JValue();
					JValue ret2 = new JValue();
					JValue ret3 = new JValue();
					JValue ret4 = new JValue();
					
					String[] tempStrArr = {"string1", "string2", "string3"};				
					
					TestNatives.staticStrArr1 = tempStrArr;
					TestNatives.staticStrArr2 = null;
					TestNatives.staticStrArr3 = null;
					TestNatives.staticStrArr4 = tempStrArr;
					TestNatives.staticStr = null;
					
					testClass.setStrArr1(tempStrArr);
					testClass.setStrArr2(null);
					testClass.setStrArr3(null);
					testClass.setStrArr4(tempStrArr);
					testClass.setStr(null);	
					
					method = testClass.getClass().getMethod(name, paramArr);
					result = testClass.testFromReflectedMethod(method, isStatic, returnType, count, arg, ret1, ret2, ret3, ret4);
					assertTrue("Incorrect testFromReflectedMethod invocation", result == 0);
					
					if(isStatic == STATIC){
						switch(returnType){
							case VOID:
								if(count == 1){
									assertTrue("Static Void Method1 with no arguments did not execute correctly", TestNatives.staticStrArr1 == null);
								} else if(count == 2){
									assertTrue("Static Void Method2 with string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
									assertTrue("Static Void Method2 with string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
									assertTrue("Static Void Method2 with string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));
								} else if(count == 3){
									assertTrue("Static Void Method3 with string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
									assertTrue("Static Void Method3 with string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
									assertTrue("Static Void Method3 with string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
									assertTrue("Static Void Method3 with string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
									assertTrue("Static Void Method3 with string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
								} else if(count == 4){
									assertTrue("Static Void Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
								}
								break;
							case OBJECT:
								if(count == 1){
									assertTrue("Static Object Method1 with object argument did not execute correctly", TestNatives.staticStrArr1 == null);
									assertTrue("Static Object Method1 with object argument returned a wrong value", arg.getL().getIntField() == ret1.getL().getIntField());		
								} else if(count == 2){
									assertTrue("Static Object Method2 with object then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
									assertTrue("Static Object Method2 with object then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
									assertTrue("Static Object Method2 with object then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
									assertTrue("Static Object Method2 with object then string array argument returned a wrong value", arg.getL().getIntField() == ret2.getL().getIntField());	
								} else if(count == 3){
									assertTrue("Static Object Method3 with object, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
									assertTrue("Static Object Method3 with object, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
									assertTrue("Static Object Method3 with object, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
									assertTrue("Static Object Method3 with object, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
									assertTrue("Static Object Method3 with object, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
									assertTrue("Static Object Method3 with object, string array then string argument returned a wrong value", arg.getL().getIntField() == ret3.getL().getIntField());
								} else if(count == 4){
									assertTrue("Static Object Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
									assertTrue("Static Object Method4 with many arguments returned a wrong value", arg.getL().getIntField() == ret4.getL().getIntField());
								}
								break;
							case BOOLEAN:
								if(count == 1){
									assertTrue("Static Boolean Method1 with boolean argument did not execute correctly", TestNatives.staticStrArr1 == null);
									assertTrue("Static Boolean Method1 with boolean argument returned a wrong value", arg.getZ() == ret1.getZ());
								} else if(count == 2){
									assertTrue("Static Boolean Method2 with boolean then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
									assertTrue("Static Boolean Method2 with boolean then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
									assertTrue("Static Boolean Method2 with boolean then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
									assertTrue("Static Boolean Method2 with boolean then string array argument returned a wrong value", arg.getZ() == ret2.getZ());			
								} else if(count == 3){
									assertTrue("Static Boolean Method3 with boolean, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
									assertTrue("Static Boolean Method3 with boolean, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
									assertTrue("Static Boolean Method3 with boolean, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
									assertTrue("Static Boolean Method3 with boolean, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
									assertTrue("Static Boolean Method3 with boolean, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
									assertTrue("Static Boolean Method3 with boolean, string array then string argument returned a wrong value", arg.getZ() == ret3.getZ());
								} else if(count == 4){
									assertTrue("Static Boolean Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
									assertTrue("Static Boolean Method4 with many arguments returned a wrong value", arg.getZ() == ret4.getZ());
								}
								break;
							case BYTE:
								if(count == 1){
									assertTrue("Static Byte Method1 with byte argument did not execute correctly", TestNatives.staticStrArr1 == null);
									assertTrue("Static Byte Method1 with byte argument returned a wrong value", arg.getB() == ret1.getB());		
								} else if(count == 2){
									assertTrue("Static Byte Method2 with byte then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
									assertTrue("Static Byte Method2 with byte then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
									assertTrue("Static Byte Method2 with byte then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
									assertTrue("Static Byte Method2 with byte then string array argument returned a wrong value", arg.getB() == ret2.getB());			
								} else if(count == 3){
									assertTrue("Static Byte Method3 with byte, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
									assertTrue("Static Byte Method3 with byte, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
									assertTrue("Static Byte Method3 with byte, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
									assertTrue("Static Byte Method3 with byte, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
									assertTrue("Static Byte Method3 with byte, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
									assertTrue("Static Byte Method3 with byte, string array then string argument returned a wrong value", arg.getB() == ret3.getB());
								} else if(count == 4){
									assertTrue("Static Byte Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
									assertTrue("Static Byte Method4 with many arguments returned a wrong value", arg.getB() == ret4.getB());
								}
								break;
							case CHAR:
								if(count == 1){
									assertTrue("Static Char Method1 with char argument did not execute correctly", TestNatives.staticStrArr1 == null);
									assertTrue("Static Char Method1 with char argument returned a wrong value", arg.getC() == ret1.getC());	
								} else if(count == 2){
									assertTrue("Static Char Method2 with char then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
									assertTrue("Static Char Method2 with char then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
									assertTrue("Static Char Method2 with char then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
									assertTrue("Static Char Method2 with char then string array argument returned a wrong value", arg.getC() == ret2.getC());			
								} else if(count == 3){
									assertTrue("Static Char Method3 with char, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
									assertTrue("Static Char Method3 with char, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
									assertTrue("Static Char Method3 with char, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
									assertTrue("Static Char Method3 with char, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
									assertTrue("Static Char Method3 with char, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
									assertTrue("Static Char Method3 with char, string array then string argument returned a wrong value", arg.getC() == ret3.getC());
								} else if(count == 4){
									assertTrue("Static Char Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
									assertTrue("Static Char Method4 with many arguments returned a wrong value", arg.getC() == ret4.getC());
								}
								break;							
							case SHORT:
								if(count == 1){
									assertTrue("Static Short Method1 with short argument did not execute correctly", TestNatives.staticStrArr1 == null);
									assertTrue("Static Short Method1 with short argument returned a wrong value", arg.getS() == ret1.getS());		
								} else if(count == 2){
									assertTrue("Static Short Method2 with short then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
									assertTrue("Static Short Method2 with short then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
									assertTrue("Static Short Method2 with short then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
									assertTrue("Static Short Method2 with short then string array argument returned a wrong value", arg.getS() == ret2.getS());			
								} else if(count == 3){
									assertTrue("Static Short Method3 with short, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
									assertTrue("Static Short Method3 with short, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
									assertTrue("Static Short Method3 with short, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
									assertTrue("Static Short Method3 with short, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
									assertTrue("Static Short Method3 with short, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
									assertTrue("Static Short Method3 with short, string array then string argument returned a wrong value", arg.getS() == ret3.getS());
								} else if(count == 4){
									assertTrue("Static Short Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
									assertTrue("Static Short Method4 with many arguments returned a wrong value", arg.getS() == ret4.getS());
								}
								break;
							case INT:
								if(count == 1){
									assertTrue("Static Int Method1 with int argument did not execute correctly", TestNatives.staticStrArr1 == null);
									assertTrue("Static Int Method1 with int argument returned a wrong value", arg.getI() == ret1.getI());		
								} else if(count == 2){
									assertTrue("Static Int Method2 with int then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
									assertTrue("Static Int Method2 with int then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
									assertTrue("Static Int Method2 with int then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
									assertTrue("Static Int Method2 with int then string array argument returned a wrong value", arg.getI() == ret2.getI());			
								} else if(count == 3){
									assertTrue("Static Int Method3 with int, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
									assertTrue("Static Int Method3 with int, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
									assertTrue("Static Int Method3 with int, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
									assertTrue("Static Int Method3 with int, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
									assertTrue("Static Int Method3 with int, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
									assertTrue("Static Int Method3 with int, string array then string argument returned a wrong value", arg.getI() == ret3.getI());
								} else if(count == 4){
									assertTrue("Static Int Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
									assertTrue("Static Int Method4 with many arguments returned a wrong value", arg.getI() == ret4.getI());
								}
								break;
							case LONG:
								if(count == 1){
									assertTrue("Static Long Method1 with long argument did not execute correctly", TestNatives.staticStrArr1 == null);
									assertTrue("Static Long Method1 with long argument returned a wrong value", arg.getJ() == ret1.getJ());		
								} else if(count == 2){
									assertTrue("Static Long Method2 with long then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
									assertTrue("Static Long Method2 with long then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
									assertTrue("Static Long Method2 with long then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
									assertTrue("Static Long Method2 with long then string array argument returned a wrong value", arg.getJ() == ret2.getJ());			
								} else if(count == 3){
									assertTrue("Static Long Method3 with long, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
									assertTrue("Static Long Method3 with long, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
									assertTrue("Static Long Method3 with long, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
									assertTrue("Static Long Method3 with long, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
									assertTrue("Static Long Method3 with long, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
									assertTrue("Static Long Method3 with long, string array then string argument returned a wrong value", arg.getJ() == ret3.getJ());
								} else if(count == 4){
									assertTrue("Static Long Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
									assertTrue("Static Long Method4 with many arguments returned a wrong value", arg.getJ() == ret4.getJ());
								}
								break;
							case FLOAT:
								if(count == 1){
									assertTrue("Static Float Method1 with float argument did not execute correctly", TestNatives.staticStrArr1 == null);
									assertTrue("Static Float Method1 with float argument returned a wrong value", arg.getF() == ret1.getF());			
								} else if(count == 2){
									assertTrue("Static Float Method2 with float then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
									assertTrue("Static Float Method2 with float then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
									assertTrue("Static Float Method2 with float then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
									assertTrue("Static Float Method2 with float then string array argument returned a wrong value", arg.getF() == ret2.getF());			
								} else if(count == 3){
									assertTrue("Static Float Method3 with float, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
									assertTrue("Static Float Method3 with float, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
									assertTrue("Static Float Method3 with float, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
									assertTrue("Static Float Method3 with float, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
									assertTrue("Static Float Method3 with float, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
									assertTrue("Static Float Method3 with float, string array then string argument returned a wrong value", arg.getF() == ret3.getF());
								} else if(count == 4){
									assertTrue("Static Float Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
									assertTrue("Static Float Method4 with many arguments returned a wrong value", arg.getF() == ret4.getF());
								}
								break;
							case DOUBLE:
								if(count == 1){
									assertTrue("Static Double Method1 with double argument did not execute correctly", TestNatives.staticStrArr1 == null);
									assertTrue("Static Double Method1 with double argument returned a wrong value", arg.getD() == ret1.getD());		
								} else if(count == 2){
									assertTrue("Static Double Method2 with double then string array argument did not execute correctly", TestNatives.staticStrArr2 != null);
									assertTrue("Static Double Method2 with double then string array argument, passed array has wrong length", TestNatives.staticStrArr2.length == 1);
									assertTrue("Static Double Method2 with double then string array argument, array element has incorrect value", TestNatives.staticStrArr2[0].equals("string"));			
									assertTrue("Static Double Method2 with double then string array argument returned a wrong value", arg.getD() == ret2.getD());			
								} else if(count == 3){
									assertTrue("Static Double Method3 with double, string array then string argument did not execute correctly", TestNatives.staticStrArr3 != null);
									assertTrue("Static Double Method3 with double, string array then string argument, passed array has wrong length", TestNatives.staticStrArr3.length == 1);
									assertTrue("Static Double Method3 with double, string array then string argument, array element has incorrect value", TestNatives.staticStrArr3[0].equals("string"));
									assertTrue("Static Double Method3 with double, string array then string argument, string arg not passed correctly", TestNatives.staticStr != null);
									assertTrue("Static Double Method3 with double, string array then string argument, string arg incorrect value", TestNatives.staticStr.equals("string"));
									assertTrue("Static Double Method3 with double, string array then string argument returned a wrong value", arg.getD() == ret3.getD());
								} else if(count == 4){
									assertTrue("Static Double Method4 with many arguments did not execute correctly", TestNatives.staticStrArr4 == null);
									assertTrue("Static Double Method4 with many arguments returned a wrong value",  arg.getD() == ret4.getD());
								}
								break;
						}			
					} else {
						switch(returnType){
							case VOID:
								if(count == 1){
									assertTrue("Void Method1 with no arguments did not execute correctly", testClass.getStrArr1() == null);
								} else if(count == 2){
									assertTrue("Void Method2 with string array argument did not execute correctly", testClass.getStrArr2() != null);
									assertTrue("Void Method2 with string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
									assertTrue("Void Method2 with string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));
								} else if(count == 3){
									assertTrue("Void Method3 with string array then string argument did not execute correctly", testClass.getStrArr3() != null);
									assertTrue("Void Method3 with string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
									assertTrue("Void Method3 with string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
									assertTrue("Void Method3 with string array then string argument, string arg not passed correctly", testClass.getStr() != null);
									assertTrue("Void Method3 with string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
								} else if(count == 4){
									assertTrue("Void Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
								}
								break;
							case OBJECT:
								if(count == 1){
									assertTrue("Object Method1 with object argument did not execute correctly", testClass.getStrArr1() == null);
									assertTrue("Object Method1 with object argument returned a wrong value", arg.getL().getIntField() == ret1.getL().getIntField());	
								} else if(count == 2){
									assertTrue("Object Method2 with object then string array argument did not execute correctly", testClass.getStrArr2() != null);
									assertTrue("Object Method2 with object then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
									assertTrue("Object Method2 with object then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
									assertTrue("Object Method2 with object then string array argument returned a wrong value", arg.getL().getIntField() == ret2.getL().getIntField());	
								} else if(count == 3){
									assertTrue("Object Method3 with object, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
									assertTrue("Object Method3 with object, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
									assertTrue("Object Method3 with object, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
									assertTrue("Object Method3 with object, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
									assertTrue("Object Method3 with object, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
									assertTrue("Object Method3 with object, string array then string argument returned a wrong value", arg.getL().getIntField() == ret3.getL().getIntField());
								} else if(count == 4){
									assertTrue("Object Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
									assertTrue("Object Method4 with many arguments returned a wrong value", arg.getL().getIntField() == ret4.getL().getIntField());	
								}
								break;
							case BOOLEAN:
								if(count == 1){
									assertTrue("Boolean Method1 with boolean argument did not execute correctly", testClass.getStrArr1() == null);
									assertTrue("Boolean Method1 with boolean argument returned a wrong value", arg.getZ() == ret1.getZ());
								} else if(count == 2){
									assertTrue("Boolean Method2 with boolean then string array argument did not execute correctly", testClass.getStrArr2() != null);
									assertTrue("Boolean Method2 with boolean then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
									assertTrue("Boolean Method2 with boolean then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
									assertTrue("Boolean Method2 with boolean then string array argument returned a wrong value", arg.getZ() == ret2.getZ());			
								} else if(count == 3){
									assertTrue("Boolean Method3 with boolean, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
									assertTrue("Boolean Method3 with boolean, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
									assertTrue("Boolean Method3 with boolean, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
									assertTrue("Boolean Method3 with boolean, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
									assertTrue("Boolean Method3 with boolean, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
									assertTrue("Boolean Method3 with boolean, string array then string argument returned a wrong value", arg.getZ() == ret3.getZ());
								} else if(count == 4){
									assertTrue("Boolean Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
									assertTrue("Boolean Method4 with many arguments returned a wrong value", arg.getZ() == ret4.getZ());
								}
								break;
							case BYTE:
								if(count == 1){
									assertTrue("Byte Method1 with byte argument did not execute correctly", testClass.getStrArr1() == null);
									assertTrue("Byte Method1 with byte argument returned a wrong value", arg.getB() == ret1.getB());		
								} else if(count == 2){
									assertTrue("Byte Method2 with byte then string array argument did not execute correctly", testClass.getStrArr2() != null);
									assertTrue("Byte Method2 with byte then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
									assertTrue("Byte Method2 with byte then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
									assertTrue("Byte Method2 with byte then string array argument returned a wrong value", arg.getB() == ret2.getB());			
								} else if(count == 3){
									assertTrue("Byte Method3 with byte, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
									assertTrue("Byte Method3 with byte, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
									assertTrue("Byte Method3 with byte, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
									assertTrue("Byte Method3 with byte, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
									assertTrue("Byte Method3 with byte, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
									assertTrue("Byte Method3 with byte, string array then string argument returned a wrong value", arg.getB() == ret3.getB());
								} else if(count == 4){
									assertTrue("Byte Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
									assertTrue("Byte Method4 with many arguments returned a wrong value", arg.getB() == ret4.getB());
								}
								break;
							case CHAR:
								if(count == 1){
									assertTrue("Char Method1 with char argument did not execute correctly", testClass.getStrArr1() == null);
									assertTrue("Char Method1 with char argument returned a wrong value", arg.getC() == ret1.getC());	
								} else if(count == 2){
									assertTrue("Char Method2 with char then string array argument did not execute correctly", testClass.getStrArr2() != null);
									assertTrue("Char Method2 with char then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
									assertTrue("Char Method2 with char then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
									assertTrue("Char Method2 with char then string array argument returned a wrong value", arg.getC() == ret2.getC());			
								} else if(count == 3){
									assertTrue("Char Method3 with char, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
									assertTrue("Char Method3 with char, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
									assertTrue("Char Method3 with char, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
									assertTrue("Char Method3 with char, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
									assertTrue("Char Method3 with char, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
									assertTrue("Char Method3 with char, string array then string argument returned a wrong value", arg.getC() == ret3.getC());
								} else if(count == 4){
									assertTrue("Char Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
									assertTrue("Char Method4 with many arguments returned a wrong value", arg.getC() == ret4.getC());
								}
								break;							
							case SHORT:
								if(count == 1){
									assertTrue("Short Method1 with short argument did not execute correctly", testClass.getStrArr1() == null);
									assertTrue("Short Method1 with short argument returned a wrong value", arg.getS() == ret1.getS());		
								} else if(count == 2){
									assertTrue("Short Method2 with short then string array argument did not execute correctly", testClass.getStrArr2() != null);
									assertTrue("Short Method2 with short then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
									assertTrue("Short Method2 with short then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
									assertTrue("Short Method2 with short then string array argument returned a wrong value", arg.getS() == ret2.getS());			
								} else if(count == 3){
									assertTrue("Short Method3 with short, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
									assertTrue("Short Method3 with short, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
									assertTrue("Short Method3 with short, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
									assertTrue("Short Method3 with short, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
									assertTrue("Short Method3 with short, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
									assertTrue("Short Method3 with short, string array then string argument returned a wrong value", arg.getS() == ret3.getS());
								} else if(count == 4){
									assertTrue("Short Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
									assertTrue("Short Method4 with many arguments returned a wrong value", arg.getS() == ret4.getS());
								}
								break;
							case INT:
								if(count == 1){
									assertTrue("Int Method1 with int argument did not execute correctly", testClass.getStrArr1() == null);
									assertTrue("Int Method1 with int argument returned a wrong value", arg.getI() == ret1.getI());		
								} else if(count == 2){
									assertTrue("Int Method2 with int then string array argument did not execute correctly", testClass.getStrArr2() != null);
									assertTrue("Int Method2 with int then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
									assertTrue("Int Method2 with int then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
									assertTrue("Int Method2 with int then string array argument returned a wrong value", arg.getI() == ret2.getI());			
								} else if(count == 3){
									assertTrue("Int Method3 with int, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
									assertTrue("Int Method3 with int, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
									assertTrue("Int Method3 with int, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
									assertTrue("Int Method3 with int, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
									assertTrue("Int Method3 with int, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
									assertTrue("Int Method3 with int, string array then string argument returned a wrong value", arg.getI() == ret3.getI());
								} else if(count == 4){
									assertTrue("Int Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
									assertTrue("Int Method4 with many arguments returned a wrong value", arg.getI() == ret4.getI());
								}
								break;
							case LONG:
								if(count == 1){
									assertTrue("Long Method1 with long argument did not execute correctly", testClass.getStrArr1() == null);
									assertTrue("Long Method1 with long argument returned a wrong value", arg.getJ() == ret1.getJ());		
								} else if(count == 2){
									assertTrue("Long Method2 with long then string array argument did not execute correctly", testClass.getStrArr2() != null);
									assertTrue("Long Method2 with long then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
									assertTrue("Long Method2 with long then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
									assertTrue("Long Method2 with long then string array argument returned a wrong value", arg.getJ() == ret2.getJ());			
								} else if(count == 3){
									assertTrue("Long Method3 with long, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
									assertTrue("Long Method3 with long, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
									assertTrue("Long Method3 with long, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
									assertTrue("Long Method3 with long, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
									assertTrue("Long Method3 with long, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
									assertTrue("Long Method3 with long, string array then string argument returned a wrong value", arg.getJ() == ret3.getJ());
								} else if(count == 4){
									assertTrue("Long Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
									assertTrue("Long Method4 with many arguments returned a wrong value", arg.getJ() == ret4.getJ());
								}
								break;
							case FLOAT:
								if(count == 1){
									assertTrue("Float Method1 with float argument did not execute correctly", testClass.getStrArr1() == null);
									assertTrue("Float Method1 with float argument returned a wrong value", arg.getF() == ret1.getF());			
								} else if(count == 2){
									assertTrue("Float Method2 with float then string array argument did not execute correctly", testClass.getStrArr2() != null);
									assertTrue("Float Method2 with float then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
									assertTrue("Float Method2 with float then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
									assertTrue("Float Method2 with float then string array argument returned a wrong value", arg.getF() == ret2.getF());			
								} else if(count == 3){
									assertTrue("Float Method3 with float, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
									assertTrue("Float Method3 with float, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
									assertTrue("Float Method3 with float, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
									assertTrue("Float Method3 with float, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
									assertTrue("Float Method3 with float, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
									assertTrue("Float Method3 with float, string array then string argument returned a wrong value", arg.getF() == ret3.getF());
								} else if(count == 4){
									assertTrue("Float Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
									assertTrue("Float Method4 with many arguments returned a wrong value", arg.getF() == ret4.getF());
								}
								break;
							case DOUBLE:
								if(count == 1){
									assertTrue("Double Method1 with double argument did not execute correctly", testClass.getStrArr1() == null);
									assertTrue("Double Method1 with double argument returned a wrong value", arg.getD() == ret1.getD());		
								} else if(count == 2){
									assertTrue("Double Method2 with double then string array argument did not execute correctly", testClass.getStrArr2() != null);
									assertTrue("Double Method2 with double then string array argument, passed array has wrong length", testClass.getStrArr2().length == 1);
									assertTrue("Double Method2 with double then string array argument, array element has incorrect value", testClass.getStrArr2()[0].equals("string"));			
									assertTrue("Double Method2 with double then string array argument returned a wrong value", arg.getD() == ret2.getD());			
								} else if(count == 3){
									assertTrue("Double Method3 with double, string array then string argument did not execute correctly", testClass.getStrArr3() != null);
									assertTrue("Double Method3 with double, string array then string argument, passed array has wrong length", testClass.getStrArr3().length == 1);
									assertTrue("Double Method3 with double, string array then string argument, array element has incorrect value", testClass.getStrArr3()[0].equals("string"));
									assertTrue("Double Method3 with double, string array then string argument, string arg not passed correctly", testClass.getStr() != null);
									assertTrue("Double Method3 with double, string array then string argument, string arg incorrect value", testClass.getStr().equals("string"));
									assertTrue("Double Method3 with double, string array then string argument returned a wrong value", arg.getD() == ret3.getD());
								} else if(count == 4){
									assertTrue("Double Method4 with many arguments did not execute correctly", testClass.getStrArr4() == null);
									assertTrue("Double Method4 with many arguments returned a wrong value", arg.getD() == ret4.getD());
								}
								break;
						}
					}
				} else {
					count = 0;
				}
			}
		} catch (Exception e) {
			fail("Exception occurred in testFromReflectedMethod:" + e.toString());
		}
	}
	
	/**
	 * tests GetArrayLength
	 */
	public void testGetArrayLength(){
		TestNatives testClass = new TestNatives(0);
		try {
			TestNatives[] array1 = new TestNatives[9];
			assertTrue("Incorrect array length:", testClass.testGetArrayLength(array1) == 9);
			
			Object[] array2 = new Object[17];
			assertTrue("Incorrect array length:", testClass.testGetArrayLength(array2) == 17);
			
			Object[] array3 = new Object[0xFFFF];
			assertTrue("Incorrect array length:", testClass.testGetArrayLength(array3) == 0xFFFF);
			
			Object[] array4 = new Object[0];
			assertTrue("Incorrect array length:", testClass.testGetArrayLength(array4) == 0);
		
		} catch (Exception e){
			fail("Exception occurred in testGetArrayLength:" + e.toString());
		}
	}
	
	/**
	 * tests GetArrayElements
	 */
	public void testGetArrayElements(){
		TestNatives testClass = new TestNatives(0);
		try {
			int length = 6;
			boolean[] z = {false, true, true, false, true, true};
			byte[] b = {0, 1, 1, 0, 1, 1};
			char[] c = {'s', 't', 'r', 'i', 'n', 'g'};
			short[] s = {1, 2, 3, 4, 5, 6};
			int[] i = {6, 7, 8, 9, 10, 11};
			long[] j = {11L, 12L, 13L, 14L, 15L, 16L};
			float[] f = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f};
			double[] d = {1.123e1, 1.123e2, 1.123e3, 1.123e4, 1.123e5, 1.123e6};
			assertTrue("Incorrect GetArrayElements invocation:", testClass.testGetArrayElements(length,z,b,c,s,i,j,f,d) == 0);
			for(int k=0; k<length; k++) {
				assertTrue("Incorrect boolean array updates:", z[k] == false);
				assertTrue("Incorrect byte array updates:", b[k] == 0);
				assertTrue("Incorrect char array updates:", c[k] == '\0');
				assertTrue("Incorrect short array updates:", s[k] == 0);
				assertTrue("Incorrect int array updates:", i[k] == 0);
				assertTrue("Incorrect long array updates:", j[k] == 0);
				assertTrue("Incorrect float array updates:", f[k] == 0);
				assertTrue("Incorrect double array updates:", d[k] == 0);
			}
		} catch (Exception e){
			fail("Exception occurred in testGetArrayElements:" + e.toString());
		}
	}
	
	/**
	 * tests GetArrayElements when the arrays have no elements
	 */
	public void testGetArrayElementsEmptyArray(){
		TestNatives testClass = new TestNatives(0);
		try {
			int length = 0;
			boolean[] z = {};
			byte[] b = {};
			char[] c = {};
			short[] s = {};
			int[] i = {};
			long[] j = {};
			float[] f = {};
			double[] d = {};
			assertTrue("Incorrect GetArrayElements invocation when arrays have no elements:", testClass.testGetArrayElements(length,z,b,c,s,i,j,f,d) == 0);
		} catch (Exception e){
			fail("Exception occurred in testGetArrayElements when arrays have no elements::" + e.toString());
		}
	}

	/**
	 * tests Get<type>ArrayRegion
	 */
	public void testGetArrayRegion(){
		TestNatives testClass = new TestNatives(0);
		try {
			boolean[] z = {false, true, true, false, true, true};
			byte[] b = {0, 1, 1, 0, 1, 1};
			char[] c = {'s', 't', 'r', 'i', 'n', 'g'};
			short[] s = {1, 2, 3, 4, 5, 6};
			int[] i = {6, 7, 8, 9, 10, 11};
			long[] j = {11L, 12L, 13L, 14L, 15L, 16L};
			float[] f = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f};
			double[] d = {1.123e1, 1.123e2, 1.123e3, 1.123e4, 1.123e5, 1.123e6};
			assertTrue("Incorrect region:", testClass.testGetArrayRegion(0,6,z,b,c,s,i,j,f,d,0)==0);
			assertTrue("Incorrect region:", testClass.testGetArrayRegion(2,3,z,b,c,s,i,j,f,d,0)==0);
			assertTrue("Incorrect region:", testClass.testGetArrayRegion(5,1,z,b,c,s,i,j,f,d,0)==0);			
			assertTrue("Incorrect region:", testClass.testGetArrayRegion(0,0,z,b,c,s,i,j,f,d,0)==0);
			
			try {
				testClass.testGetArrayRegion(6,1,z,b,c,s,i,j,f,d,1);
				fail("Expected index out of bounds exception");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(6,1,z,b,c,s,i,j,f,d,2);
				fail("Expected index out of bounds exception");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(6,1,z,b,c,s,i,j,f,d,3);
				fail("Expected index out of bounds exception");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(6,1,z,b,c,s,i,j,f,d,4);
				fail("Expected index out of bounds exception");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(6,1,z,b,c,s,i,j,f,d,5);
				fail("Expected index out of bounds exception");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(6,1,z,b,c,s,i,j,f,d,6);
				fail("Expected index out of bounds exception");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(6,1,z,b,c,s,i,j,f,d,7);
				fail("Expected index out of bounds exception");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(6,1,z,b,c,s,i,j,f,d,8);
				fail("Expected index out of bounds exception");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			
			try {
				testClass.testGetArrayRegion(-1,5,z,b,c,s,i,j,f,d,1);
				fail("Expected index out of bounds exception for negative start");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(-1,5,z,b,c,s,i,j,f,d,2);
				fail("Expected index out of bounds exception for negative start");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(-1,5,z,b,c,s,i,j,f,d,3);
				fail("Expected index out of bounds exception for negative start");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(-1,5,z,b,c,s,i,j,f,d,4);
				fail("Expected index out of bounds exception for negative start");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(-1,5,z,b,c,s,i,j,f,d,5);
				fail("Expected index out of bounds exception for negative start");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(-1,5,z,b,c,s,i,j,f,d,6);
				fail("Expected index out of bounds exception for negative start");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(-1,5,z,b,c,s,i,j,f,d,7);
				fail("Expected index out of bounds exception for negative start");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(-1,5,z,b,c,s,i,j,f,d,8);
				fail("Expected index out of bounds exception for negative start");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			
			try {
				testClass.testGetArrayRegion(0,-5,z,b,c,s,i,j,f,d,1);
				fail("Expected index out of bounds exception for negative length");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(0,-5,z,b,c,s,i,j,f,d,2);
				fail("Expected index out of bounds exception for negative length");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(0,-5,z,b,c,s,i,j,f,d,3);
				fail("Expected index out of bounds exception for negative length");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(0,-5,z,b,c,s,i,j,f,d,4);
				fail("Expected index out of bounds exception for negative length");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(0,-5,z,b,c,s,i,j,f,d,5);
				fail("Expected index out of bounds exception for negative length");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(0,-5,z,b,c,s,i,j,f,d,6);
				fail("Expected index out of bounds exception for negative length");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(0,-5,z,b,c,s,i,j,f,d,7);
				fail("Expected index out of bounds exception for negative length");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			try {
				testClass.testGetArrayRegion(0,-5,z,b,c,s,i,j,f,d,8);
				fail("Expected index out of bounds exception for negative length");
			} catch(ArrayIndexOutOfBoundsException e){
			}
		
		} catch (Exception e){
			fail("Exception occurred in testGetArrayRegion:" + e.toString());
		}
	}
	
	/**
	 * tests Get<type>Field and GetStatic<type>Field
	 */	
	public void testGetField(){
		boolean z = true;
		byte b = 1;
		char c = 'a';
		short s = 2;
		int i = 3;
		long j = 4L;
		float f = 5.5f;
		double d = 6.6d;
		TestNatives l = new TestNatives(10);
		JValue value = new JValue(z, b, c, s, i, j, f, d, l);
		
		boolean sz = false;
		byte sb = 0;
		char sc = '\u0041';	 // capital A
		short ss = 032; 	 // in octal
		int si = 0x12345678; // in hexadecimal
		long sj = 1234L; 	 // in decimal
		float sf = 123.4f;	 // in float literal
		double sd = 1.234e2; // in scientific notation
		TestNatives sl = new TestNatives(20);	
		JValue staticValue = new JValue(sz, sb, sc, ss, si, sj, sf, sd, sl);
		
		TestNatives testClass = new TestNatives(value, staticValue);
				
		assertTrue("GetObjectField failed", ((TestNatives)testClass.testGetObjectField()).getIntField() == 10);		
		assertTrue("GetBooleanField failed", testClass.testGetBooleanField() == true);
		assertTrue("GetByteField failed", testClass.testGetByteField() == 1);
		assertTrue("GetCharField failed", testClass.testGetCharField() == 'a');
		assertTrue("GetShortField failed", testClass.testGetShortField() == 2);
		assertTrue("GetIntField failed", testClass.testGetIntField() == 3);
		assertTrue("GetLongField failed", testClass.testGetLongField() == 4L);
		assertTrue("GetFloatField failed", testClass.testGetFloatField() == 5.5f);
		assertTrue("GetDoubleField failed", testClass.testGetDoubleField() == 6.6d);
		
		assertTrue("GetStaticObjectField failed", ((TestNatives)testClass.testGetStaticObjectField()).getIntField() == 20);		
		assertTrue("GetStaticBooleanField failed", testClass.testGetStaticBooleanField() == false);
		assertTrue("GetStaticByteField failed", testClass.testGetStaticByteField() == 0);
		assertTrue("GetStaticCharField failed", testClass.testGetStaticCharField() == 'A');
		assertTrue("GetStaticShortField failed", testClass.testGetStaticShortField() == 032);
		assertTrue("GetStaticIntField failed", testClass.testGetStaticIntField() == 0x12345678);
		assertTrue("GetStaticLongField failed", testClass.testGetStaticLongField() == 1234L);
		assertTrue("GetStaticFloatField failed", testClass.testGetStaticFloatField() == 123.4f);
		assertTrue("GetStaticDoubleField failed", testClass.testGetStaticDoubleField() == 1.234e2);
	}
	
	/**
	 * tests DirectBuffer
	 */
	public void testDirectBuffer(){
		TestNatives testClass = new TestNatives(0);
		try {
			int result = testClass.testDirectBuffer();
			assertTrue("Incorrect testDirectBuffer invocation", result == 0);
		} catch (Exception e) {
			fail("Exception occurred in testDirectBuffer:" + e.toString());
		}
	}	
	
	/**
	 * tests GetFieldID
	 */
	public void testGetFieldID(){
		TestNatives testClass = new TestNatives(0);
		try {
			assertTrue("could not lookup available field", testClass.testGetFieldID(1) == 1);
			
			try {
				testClass.testGetFieldID(2);
				fail("Should not have been able to lookup field with incorrect signature");
			} catch (NoSuchFieldError e){
				// expected;
			}
			
			try {
				testClass.testGetFieldID(3);
				fail("Should not have been able to lookup field with incorrect name");
			} catch (NoSuchFieldError e){
				// expected;
			}
			
		} catch (Exception e){
			fail("Exception occurred in testGetFieldID:" + e.toString());
		}
	}

	/**
	 * tests GetMethodID
	 */
	public void testGetMethodID(){
		TestNatives testClass = new TestNatives(0);
		try {
			assertTrue("could not lookup available method", testClass.testGetMethodID(1) == 1);
			
			try {
				testClass.testGetMethodID(2);
				fail("Should not have been able to lookup method with incorrect signature");
			} catch (NoSuchMethodError e){
				// expected;
			}
			
			try {
				testClass.testGetMethodID(3);
				fail("Should not have been able to lookup method with incorrect name");
			} catch (NoSuchMethodError e){
				// expected;
			}
			
		} catch (Exception e){
			fail("Exception occurred in testGetMethodID:" + e.toString());
		}
	}
	
	/**
	 * tests GetObjectArrayElement
	 */
	public void testGetObjectArrayElement(){
		TestNatives testClass = new TestNatives();
		Object testClassArray[] = new TestNatives[5];
		TestNatives result;
		for(int i=0; i<5; i++){
			testClassArray[i] = new TestNatives(i);
		}
		result = (TestNatives)testClass.testGetObjectArrayElement(testClassArray, 0);
		assertTrue("Object element returned by native did not match object expected", result.getIntField() == 0);
		result = (TestNatives)testClass.testGetObjectArrayElement(testClassArray, 1);
		assertTrue("Object element returned by native did not match object expected", result.getIntField() == 1);
		result = (TestNatives)testClass.testGetObjectArrayElement(testClassArray, 2);
		assertTrue("Object element returned by native did not match object expected", result.getIntField() == 2);
		result = (TestNatives)testClass.testGetObjectArrayElement(testClassArray, 3);
		assertTrue("Object element returned by native did not match object expected", result.getIntField() == 3);
		result = (TestNatives)testClass.testGetObjectArrayElement(testClassArray, 4);
		assertTrue("Object element returned by native did not match object expected", result.getIntField() == 4);		
	}
	
	/**
	 * tests GetObjectClass
	 */
	public void testGetObjectClass(){
		TestNatives testClass = new TestNatives(1234);
		assertTrue("Object class looked up by native did not match object class expected", 
					testClass.getClass() == testClass.testGetObjectClass());
	}
	
	/**
	 * tests GetObjectRefType
	 */
	public void testGetObjectRefType(){
		TestNatives testClass = new TestNatives(0);
		try {
			int result = testClass.testGetObjectRefType();
			assertTrue("Incorrect testGetObjectRefType invocation", result == 0);
		} catch (Exception e) {
			fail("Exception occurred in testGetObjectRefType:" + e.toString());
		}
	}		
	
	/**
	 * tests GetPrimitiveArrayCritical
	 */
	public void testGetPrimitiveArrayCritical(){
		TestNatives testClass = new TestNatives(1234);		
		try {
			int length = 6;
			boolean[] z = {false, true, true, false, true, true};
			byte[] b = {0, 1, 1, 0, 1, 1};
			char[] c = {'s', 't', 'r', 'i', 'n', 'g'};
			short[] s = {1, 2, 3, 4, 5, 6};
			int[] i = {6, 7, 8, 9, 10, 11};
			long[] j = {11L, 12L, 13L, 14L, 15L, 16L};
			float[] f = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f};
			double[] d = {1.123e1, 1.123e2, 1.123e3, 1.123e4, 1.123e5, 1.123e6};
			assertTrue("Incorrect elements:", testClass.testGetPrimitiveArrayCritical(length,z,b,c,s,i,j,f,d) == 0);
			for(int k=0; k<length; k++) {
				assertTrue("Incorrect boolean array updates:", z[k] == false);
				assertTrue("Incorrect byte array updates:", b[k] == 0);
				assertTrue("Incorrect char array updates:", c[k] == '\0');
				assertTrue("Incorrect short array updates:", s[k] == 0);
				assertTrue("Incorrect int array updates:", i[k] == 0);
				assertTrue("Incorrect long array updates:", j[k] == 0);
				assertTrue("Incorrect float array updates:", f[k] == 0);
				assertTrue("Incorrect double array updates:", d[k] == 0);
			}
		} catch (Exception e){
			fail("Exception occurred in testGetPrimitiveArrayCritical:" + e.toString());
		}
	}
	
	/**
	 * tests GetStaticFieldID
	 */
	public void testGetStaticFieldID(){
		TestNatives testClass = new TestNatives(0);
		try {
			assertTrue("could not lookup available field", testClass.testGetStaticFieldID(1) == 1);
			
			try {
				testClass.testGetStaticFieldID(2);
				fail("Should not have been able to lookup field with incorrect signature");
			} catch (NoSuchFieldError e){
				// expected;
			}
			
			try {
				testClass.testGetStaticFieldID(3);
				fail("Should not have been able to lookup field with incorrect name");
			} catch (NoSuchFieldError e){
				// expected;
			}
			
		} catch (Exception e){
			fail("Exception occurred in testGetStaticFieldID:" + e.toString());
		}
	}
	
	/**
	 * tests GetStaticMethodID
	 */
	public void testGetStaticMethodID(){
		TestNatives testClass = new TestNatives(0);
		try {
			assertTrue("could not lookup available method", testClass.testGetStaticMethodID(1) == 1);
			
			try {
				testClass.testGetStaticMethodID(2);
				fail("Should not have been able to lookup method with incorrect signature");
			} catch (NoSuchMethodError e){
				// expected;
			}
			
			try {
				testClass.testGetStaticMethodID(3);
				fail("Should not have been able to lookup method with incorrect signature");
			} catch (NoSuchMethodError e){
				// expected;
			}
			
		} catch (Exception e){
			fail("Exception occurred in testGetStaticMethodID:" + e.toString());
		}
	}
	
	/**
	 * tests GetStringChars
	 */
	public void testGetStringChars(){
		TestNatives testClass = new TestNatives(0);	
		try {
			String str = "test string";
			int result = testClass.testGetStringChars(str, str.length());
			assertTrue("jchar array not returned correctly", result == 0);
		} catch (Exception e){
			fail("Exception occurred in testGetStringChars:" + e.toString());
		}
	}
	
	/**
	 * tests GetStringCritical
	 */
	public void testGetStringCritical(){
		TestNatives testClass = new TestNatives(0);	
		try {
			String str = "test string";
			int result = testClass.testGetStringCritical(str, str.length());
			assertTrue("jchar array not returned correctly", result == 0);
		} catch (Exception e){
			fail("Exception occurred in testGetStringCritical:" + e.toString());
		}
	}		
	
	/**
	 * tests GetStringLength
	 */
	public void testGetStringLength(){
		TestNatives testClass = new TestNatives(0);	
		try {
			int result = testClass.testGetStringLength("My length is 16.");
			assertTrue("The length of the string is not correct", result == 16);
		} catch (Exception e){
			fail("Exception occurred in testGetStringLength:" + e.toString());
		}
	}
	
	/**
	 * tests GetStringRegion
	 */
	public void testGetStringRegion(){
		TestNatives testClass = new TestNatives(0);
		try {
			String str = "test string";
			assertTrue("Incorrect region:", testClass.testGetStringRegion(str,0,11)==0);
			assertTrue("Incorrect region:", testClass.testGetStringRegion(str,2,3)==0);
			assertTrue("Incorrect region:", testClass.testGetStringRegion(str,5,3)==0);
			
			testClass.testGetStringRegion(str,0,0);
			try {
				testClass.testGetStringRegion(str,12,1);
				fail("Expected index out of bounds exception");
			} catch(StringIndexOutOfBoundsException e){
			}
			
			try {
				testClass.testGetStringRegion(str,-1,5);
				fail("Expected index out of bounds exception for negative start");
			} catch(StringIndexOutOfBoundsException e){
			}
			
			try {
				testClass.testGetStringRegion(str,1,-5);
				fail("Expected index out of bounds exception for negative length");
			} catch(StringIndexOutOfBoundsException e){
			}
		
		} catch (Exception e){
			fail("Exception occurred in testGetStringRegion:" + e.toString());
		}
	}
	
	/**
	 * tests GetStringUTFChars
	 */
	public void testGetStringUTFChars(){
		TestNatives testClass = new TestNatives(0);	
		try {
			String str = "test string";
			int result = testClass.testGetStringUTFChars(str, str.length());
			assertTrue("UTF char array not returned correctly", result == 0);
		} catch (Exception e){
			fail("Exception occurred in testGetStringUTFChars:" + e.toString());
		}
	}
	
	/**
	 * tests GetStringUTFLength
	 */
	public void testGetStringUTFLength(){
		TestNatives testClass = new TestNatives(0);
		try {
			int result = testClass.testGetStringUTFLength("My UTF length is 20.");
			assertTrue("The UTF length of the string is not correct", result == 20);
		} catch (Exception e){
			fail("Exception occurred in testGetStringUTFLength:" + e.toString());
		}
	}
	
	/**
	 * tests GetStringUTFRegion
	 */
	public void testGetStringUTFRegion(){
		TestNatives testClass = new TestNatives(0);
		try {
			String str = "test string";
			assertTrue("Incorrect region:", testClass.testGetStringUTFRegion(str,0,11)==0);
			assertTrue("Incorrect region:", testClass.testGetStringUTFRegion(str,2,3)==0);
			assertTrue("Incorrect region:", testClass.testGetStringUTFRegion(str,5,3)==0);	
			assertTrue("Incorrect region:", testClass.testGetStringRegion(str,0,0)==0);
			
			try {
				testClass.testGetStringUTFRegion(str,12,1);
				fail("Expected index out of bounds exception");
			} catch(StringIndexOutOfBoundsException e){
			}
			
			try {
				testClass.testGetStringUTFRegion(str,-1,5);
				fail("Expected index out of bounds exception for negative start");
			} catch(StringIndexOutOfBoundsException e){
			}
			
			try {
				testClass.testGetStringUTFRegion(str,1,-5);
				fail("Expected index out of bounds exception for negative length");
			} catch(StringIndexOutOfBoundsException e){
			}
			
			/* now validate that we handle UTF strings that have characters that get encoded
			 * as multiple bytes
			 */
			char[] longChars = new char[] {'\u0001','\u0081','\u0801', '\u8001'};
			String moreThanOneUTFPerCharString = "";
			moreThanOneUTFPerCharString = moreThanOneUTFPerCharString + longChars[0] + longChars[1] + longChars[2] + longChars[3];
			assertTrue(testClass.testGetStringUTFRegionMultibyteChars(moreThanOneUTFPerCharString)==0);
		
		} catch (Exception e){
			fail("Exception occurred in testGetStringUTFRegion:" + e.toString());
		}
	}
	
	/**
	 * tests GetSuperclass
	 */
	public void testGetSuperclass(){
		TestNatives superClass = new TestNatives(0);
		TestNativesChild childClass = new TestNativesChild(0);
		Class result = superClass.testGetSuperclass(childClass.getClass());
		
		assertTrue("Superclass not returned correctly", result == superClass.getClass());
	}	

	/**
	 * tests GetVersion
	 */	
	public void testGetVersion(){
		TestNatives testClass = new TestNatives(0);
		int result = testClass.testGetVersion();
		
		// java 6: 0x00010006
		assertTrue("GetVersion not returned the correctly", result == 0x00010006);
	}
	
	/**
	 * tests IsAssignableFrom
	 */	
	public void testIsAssignableFrom(){
		TestNatives superClass = new TestNatives(0);
		TestNativesChild childClass = new TestNativesChild(0);
		TestNatives[] superClassArr = new TestNatives[5];
		TestNativesChild[] childClassArr = new TestNativesChild[5];
		boolean result;
		try {
			result = superClass.testIsAssignableFrom(superClass.getClass(), superClass.getClass());
			assertTrue("Incorrect IsAssignableFrom invocation for the same class", result == true);
			
			result = superClass.testIsAssignableFrom(childClass.getClass(), superClass.getClass());
			assertTrue("Incorrect IsAssignableFrom invocation for a subclass", result == true);
			
			result = superClass.testIsAssignableFrom(childClass.getClass(), superClass.getClass());
			assertTrue("Incorrect IsAssignableFrom invocation for a subclass", result == true);
			
			result = superClass.testIsAssignableFrom(childClass.getClass(), ((TestNativesInterface)superClass).getClass());
			assertTrue("Incorrect IsAssignableFrom invocation for an interface", result == true);
			
			result = superClass.testIsAssignableFrom(childClassArr.getClass(), superClassArr.getClass());
			assertTrue("Incorrect IsAssignableFrom invocation for an interface", result == true);			
		} catch (Exception e) {
			fail("Exception occurred in testIsAssignableFrom:" + e.toString());
		}
	}

	/**
	 * tests IsInstanceOf
	 */	
	public void testIsInstanceOf(){
		TestNatives testClass = new TestNatives(0);
		try {
			boolean result = testClass.testIsInstanceOf(testClass, testClass.getClass());
			assertTrue("Incorrect IsInstanceOf invocation", result == true);
		} catch (Exception e) {
			fail("Exception occurred in testIsInstanceOf:" + e.toString());
		}
	}
	
	/**
	 * tests IsSameObject
	 */	
	public void testIsSameObject(){
		TestNatives testClass = new TestNatives(0);
		TestNatives testClassCopy = testClass;
		try {
			boolean result = testClass.testIsSameObject(testClass, testClassCopy);
			assertTrue("Incorrect IsSameObject invocation", result == true);
		} catch (Exception e) {
			fail("Exception occurred in testIsSameObject:" + e.toString());
		}
	}
	
	/**
	 * tests MonitorEnter
	 */
	class TestNativesThread extends Thread {
		TestNatives testClass;
		Object obj;
		int result;
		
		public TestNativesThread(TestNatives testClass, Object obj) {
			super();
			this.testClass = testClass;
			this.obj = obj;
		}
		
		public void run() {
			result = testClass.testMonitorEnter(obj);
			assertTrue("Incorrect MonitorEnter invocation for the child thread", result == 0);
			if (criticalValue == 0) {
				criticalValue = 2;
			}
			result = testClass.testMonitorExit(obj);
			assertTrue("Incorrect MonitorExit invocation for the child thread", result == 0);
		}
	}
	
	public void testMonitorEnter(){
		int result;
		Object obj = new Object();
		TestNatives testClass = new TestNatives(0);
		TestNativesThread thread = new TestNativesThread(testClass, obj);
		
		try {
			criticalValue = 0;
			result = testClass.testMonitorEnter(obj);
			assertTrue("Incorrect MonitorEnter invocation for the main thread", result == 0);
			thread.start();
			Thread.sleep(1000);
			if(criticalValue == 0) {
				criticalValue = 1;
			}
			result = testClass.testMonitorExit(obj);
			assertTrue("Incorrect MonitorExit invocation for the main thread", result == 0);			
		} catch (Exception e) {
			fail("Exception occurred in testMonitorEnter:" + e.toString());
		}
	}
	
	/**
	 * tests MonitorExit
	 */	
	public void testMonitorExit(){
		int result;
		Object obj = new Object();
		TestNatives testClass = new TestNatives(0);
		
		try {
			/* entry count == 1 */
	        result = testClass.testMonitorEnter(obj);
	        assertTrue("Incorrect MonitorEnter invocation", result == 0);
	        
	        result = testClass.testMonitorExit(obj);
	        assertTrue("Incorrect MonitorExit invocation", result == 0);
	        
	        /* entry count == 3 */
	        result = testClass.testMonitorEnter(obj);
	        assertTrue("Incorrect MonitorEnter invocation", result == 0);
	        
	        result = testClass.testMonitorEnter(obj);
	        assertTrue("Incorrect MonitorEnter invocation", result == 0);  
	        
	        result = testClass.testMonitorEnter(obj);
	        assertTrue("Incorrect MonitorEnter invocation", result == 0);  
	        
	        result = testClass.testMonitorExit(obj);
	        assertTrue("Incorrect MonitorExit invocation", result == 0);
	        
	        result = testClass.testMonitorExit(obj);
	        assertTrue("Incorrect MonitorExit invocation", result == 0);
	        
	        result = testClass.testMonitorExit(obj);
	        assertTrue("Incorrect MonitorExit invocation", result == 0);
	        
	        /* entry count == 0 */
	        try {
		        result = testClass.testMonitorExit(obj);
		        fail("Expected IllegalMonitorStateException, the current thread does not own the monitor");
	        } catch (IllegalMonitorStateException e) {
	        	
	        }
		} catch (Exception e) {
			fail("Exception occurred in testMonitorExit:" + e.toString());
		}
	}		
	
	/**
	 * tests NewGlobalRef and NewLocalRef
	 */
	public void testNewRef(){
		TestNatives testClass = new TestNatives(0);
		try {
			int result = testClass.testNewRef();
			assertTrue("Incorrect NewRef invocation", result == 0);
		} catch (Exception e) {
			fail("Exception occurred in testNewRef:" + e.toString());
		}
	}
	
	/**
	 * tests NewObject
	 */
	public void testNewObject(){
		TestNatives testClass = new TestNatives(0);
		try {
			int value = 125;
			TestNatives result = (TestNatives)testClass.testNewObject(value);
			assertTrue("Incorrect NewObject Invocation", result.getIntField() == value);
		} catch (Exception e){
			fail("Exception occurred in testNewObject:" + e.toString());
		}
	}
	
	/**
	 * tests NewObjectA
	 */
	public void testNewObjectA(){
		TestNatives testClass = new TestNatives(0);
		try {
			int value = 126;
			TestNatives result = (TestNatives)testClass.testNewObjectA(value);
			assertTrue("Incorrect NewObjectA Invocation", result.getIntField() == value);
		} catch (Exception e){
			fail("Exception occurred in testNewObjectA:" + e.toString());
		}
	}	
	
	/**
	 * tests NewObjectV
	 */
	public void testNewObjectV(){
		TestNatives testClass = new TestNatives(0);
		try {
			int value = 127;
			TestNatives result = (TestNatives)testClass.testNewObjectV(value);
			assertTrue("Incorrect NewObjectV Invocation", result.getIntField() == value);
		} catch (Exception e){
			fail("Exception occurred in testNewObjectV:" + e.toString());
		}
	}	
	
	/**
	 * tests NewObjectArray
	 */
	public void testNewObjectArray(){
		TestNatives testClass = new TestNatives(0);
		try {
			TestNatives[] result = (TestNatives[])testClass.testNewObjectArray(10, TestNatives.class, testClass);
			for(int i=0;i<10;i++){
				assertTrue("Array entries not initialized to testClass", result[i].equals(testClass));
			}
			try {
				result[10] = testClass;
				fail("Expected ArrayIndexOutOfBoundsException, NewObjectArray not created correctly");
			}catch (ArrayIndexOutOfBoundsException e){
			}
			
			result = (TestNatives[])testClass.testNewObjectArray(20, TestNatives.class, testClass);
			for(int i=0;i<20;i++){
				assertTrue("Array entries not initialized to testClass", result[i].equals(testClass));
			}
			try {
				result[20] = testClass;
				fail("Expected ArrayIndexOutOfBoundsException, NewObjectArray not created correctly");
			}catch (ArrayIndexOutOfBoundsException e){
			}
			
			result = (TestNatives[])testClass.testNewObjectArray(10, TestNatives.class, null);
			result = (TestNatives[])testClass.testNewObjectArray(0, TestNatives.class, null);
		} catch (Exception e){
			fail("Exception occurred in testNewObjectArray:" + e.toString());
		}
	}

	/**
	 * tests New<type>Array
	 */
	public void testNewArray(){
		TestNatives testClass = new TestNatives(0);
		try {
			boolean[] arr_z = testClass.testNewBooleanArray(10);
			byte[] arr_b = testClass.testNewByteArray(10);
			char[] arr_c = testClass.testNewCharArray(10);
			short[] arr_s = testClass.testNewShortArray(10);
			int[] arr_i = testClass.testNewIntArray(10);
			long[] arr_j = testClass.testNewLongArray(10);
			float[] arr_f = testClass.testNewFloatArray(10);
			double[] arr_d = testClass.testNewDoubleArray(10);
			for(int i=0;i<10;i++){
				arr_z[i] = true;
				arr_b[i] = (byte)1;
				arr_c[i] = 'a';
				arr_s[i] = (short)2;
				arr_i[i] = 3;
				arr_j[i] = 4L;
				arr_f[i] = 5f;
				arr_d[i] = 6d;
			}
			try {
				arr_z[10] = true;
				fail("Expected ArrayIndexOutOfBoundsException, NewBooleanArray not created correctly");
			}catch (ArrayIndexOutOfBoundsException e){
			}
			try {
				arr_b[10] = (byte)1;
				fail("Expected ArrayIndexOutOfBoundsException, NewByteArray not created correctly");
			}catch (ArrayIndexOutOfBoundsException e){
			}
			try {
				arr_c[10] = 'a';
				fail("Expected ArrayIndexOutOfBoundsException, NewCharArray not created correctly");
			}catch (ArrayIndexOutOfBoundsException e){
			}
			try {
				arr_s[10] = (short)2;
				fail("Expected ArrayIndexOutOfBoundsException, NewShortArray not created correctly");
			}catch (ArrayIndexOutOfBoundsException e){
			}
			try {
				arr_i[10] = 3;
				fail("Expected ArrayIndexOutOfBoundsException, NewIntArray not created correctly");
			}catch (ArrayIndexOutOfBoundsException e){
			}
			try {
				arr_j[10] = 4L;
				fail("Expected ArrayIndexOutOfBoundsException, NewLongArray not created correctly");
			}catch (ArrayIndexOutOfBoundsException e){
			}
			try {
				arr_f[10] = 5f;
				fail("Expected ArrayIndexOutOfBoundsException, NewFloatArray not created correctly");
			}catch (ArrayIndexOutOfBoundsException e){
			}
			try {
				arr_d[10] = 6d;
				fail("Expected ArrayIndexOutOfBoundsException, NewDoubleArray not created correctly");
			}catch (ArrayIndexOutOfBoundsException e){
			}
			
			arr_z = testClass.testNewBooleanArray(20);
			arr_b = testClass.testNewByteArray(20);
			arr_c = testClass.testNewCharArray(20);
			arr_s = testClass.testNewShortArray(20);
			arr_i = testClass.testNewIntArray(20);
			arr_j = testClass.testNewLongArray(20);
			arr_f = testClass.testNewFloatArray(20);
			arr_d = testClass.testNewDoubleArray(20);
			for(int i=0;i<20;i++){
				arr_z[i] = true;
				arr_b[i] = (byte)1;
				arr_c[i] = 'a';
				arr_s[i] = (short)2;
				arr_i[i] = 3;
				arr_j[i] = 4L;
				arr_f[i] = 5f;
				arr_d[i] = 6d;
			}
			try {
				arr_z[20] = true;
				fail("Expected ArrayIndexOutOfBoundsException, NewBooleanArray not created correctly");
			}catch (ArrayIndexOutOfBoundsException e){
			}
			try {
				arr_b[20] = (byte)1;
				fail("Expected ArrayIndexOutOfBoundsException, NewByteArray not created correctly");
			}catch (ArrayIndexOutOfBoundsException e){
			}
			try {
				arr_c[20] = 'a';
				fail("Expected ArrayIndexOutOfBoundsException, NewCharArray not created correctly");
			}catch (ArrayIndexOutOfBoundsException e){
			}
			try {
				arr_s[20] = (short)2;
				fail("Expected ArrayIndexOutOfBoundsException, NewShortArray not created correctly");
			}catch (ArrayIndexOutOfBoundsException e){
			}
			try {
				arr_i[20] = 3;
				fail("Expected ArrayIndexOutOfBoundsException, NewIntArray not created correctly");
			}catch (ArrayIndexOutOfBoundsException e){
			}
			try {
				arr_j[20] = 4L;
				fail("Expected ArrayIndexOutOfBoundsException, NewLongArray not created correctly");
			}catch (ArrayIndexOutOfBoundsException e){
			}
			try {
				arr_f[20] = 5f;
				fail("Expected ArrayIndexOutOfBoundsException, NewFloatArray not created correctly");
			}catch (ArrayIndexOutOfBoundsException e){
			}
			try {
				arr_d[20] = 6d;
				fail("Expected ArrayIndexOutOfBoundsException, NewDoubleArray not created correctly");
			}catch (ArrayIndexOutOfBoundsException e){
			}
		} catch (Exception e){
			fail("Exception occurred in testNewArray:" + e.toString());
		}
	}
	
	/**
	 * tests NewString
	 */
	public void testNewString(){
		TestNatives testClass = new TestNatives(0);	
		try {
			String result = testClass.testNewString();
			assertTrue("string not created properly", result.equals("test string"));
		} catch (Exception e){
			fail("Exception occurred in testNewString:" + e.toString());
		}
	}
	
	/**
	 * tests NewStringUTF
	 */
	public void testNewStringUTF(){
		TestNatives testClass = new TestNatives(0);
		try {
			String result = testClass.testNewStringUTF();
			assertTrue("string not created properly", result.equals("test string"));
		} catch (Exception e){
			fail("Exception occurred in testNewStringUTF:" + e.toString());
		}
	}
	
	/**
	 * tests PopLocalFrame
	 */
	public void testPopLocalFrame(){
		TestNatives testClass = new TestNatives(0);
		try {
			testClass.testPopLocalFrame(testClass);
		} catch (Exception e){
			fail("Exception occurred in testPopLocalFrame:" + e.toString());
		}
	}
	
	/**
	 * tests PushLocalFrame
	 */
	public void testPushLocalFrame(){
		TestNatives testClass = new TestNatives(0);
		try {
			testClass.testPushLocalFrame(10);
		} catch (Exception e){
			fail("Exception occurred in testPushLocalFrame:" + e.toString());
		}
	}	
	
	/**
	 * tests SetArrayRegion
	 */
	public void testSetArrayRegion(){
		TestNatives testClass = new TestNatives(0);
		try {
			boolean[] arr1 = {true,false,true,false,true,false,true,false,true,false};
			byte[] arr2 = {1,2,3,4,5,6,7,8,9,10};
			char[] arr3 = {'A','B','C','D','E','F','G','H','I','J'};
			short[] arr4 = {1,2,3,4,5,6,7,8,9,10};
			int[] arr5 = {1,2,3,4,5,6,7,8,9,10};
			long[] arr6 = {1L,2L,3L,4L,5L,6L,7L,8L,9L,10L};
			float[] arr7 = {1.1f,2.2f,3.3f,4.4f,5.5f,6.6f,7.7f,8.8f,9.9f,10.0f};
			double[] arr8 = {1.1d,2.2d,3.3d,4.4d,5.5d,6.6d,7.7d,8.8d,9.9d,10.0d};									
			
			testClass.testSetArrayRegion(
					0,10,
					arr1,arr2,arr3,arr4,arr5,arr6,arr7,arr8,
					true,(byte)20,'a',(short)30,40,50L,60f,70d);
			for (int i=0;i<10;i++){
				assertTrue("booleans not set correctly", arr1[i] == true);
				assertTrue("bytes not set correctly", arr2[i] == i + 20);
				assertTrue("chars not set correctly", arr3[i] == i + 'a');
				assertTrue("shorts not set correctly", arr4[i] == i + 30);
				assertTrue("ints not set correctly", arr5[i] == i + 40);
				assertTrue("longs not set correctly", arr6[i] == i + 50L);
				assertTrue("floats not set correctly", arr7[i] == i + 60f);
				assertTrue("doubles not set correctly", arr8[i] == i + 70d);
			}
			
			for (int i=0;i<10;i++){
				arr1[i] = false;
				arr2[i] = (byte)0;
				arr3[i] = '\0';
				arr4[i] = (short)0;
				arr5[i] = 0;
				arr6[i] = 0L;
				arr7[i] = 0f;
				arr8[i] = 0d;
			}
			testClass.testSetArrayRegion(
					3,3,
					arr1,arr2,arr3,arr4,arr5,arr6,arr7,arr8,
					true,(byte)1,'a',(short)2,3,4L,5.5f,6.6d);
			for (int i=0;i<10;i++){
				if (i == 3){
					assertTrue("booleans not set correctly", arr1[i] == true);
					assertTrue("bytes not set correctly", arr2[i] == 1);
					assertTrue("chars not set correctly", arr3[i] == 'a');
					assertTrue("shorts not set correctly", arr4[i] == 2);
					assertTrue("ints not set correctly", arr5[i] == 3);
					assertTrue("longs not set correctly", arr6[i] == 4L);
					assertTrue("floats not set correctly", arr7[i] == 5.5f);
					assertTrue("doubles not set correctly", arr8[i] == 6.6d);
				} else if (i == 4){
					assertTrue("booleans not set correctly", arr1[i] == true);
					assertTrue("bytes not set correctly", arr2[i] == 2);
					assertTrue("chars not set correctly", arr3[i] == 'b');
					assertTrue("shorts not set correctly", arr4[i] == 3);
					assertTrue("ints not set correctly", arr5[i] == 4);
					assertTrue("longs not set correctly", arr6[i] == 5L);
					assertTrue("floats not set correctly", arr7[i] == 6.5f);
					assertTrue("doubles not set correctly", arr8[i] == 7.6d);
				} else if (i == 5){
					assertTrue("booleans not set correctly", arr1[i] == true);
					assertTrue("bytes not set correctly", arr2[i] == 3);
					assertTrue("chars not set correctly", arr3[i] == 'c');
					assertTrue("shorts not set correctly", arr4[i] == 4);
					assertTrue("ints not set correctly", arr5[i] == 5);
					assertTrue("longs not set correctly", arr6[i] == 6L);
					assertTrue("floats not set correctly", arr7[i] == 7.5f);
					assertTrue("doubles not set correctly", arr8[i] == 8.6d);
				} else {
					assertTrue("booleans not set correctly", arr1[i] == false);
					assertTrue("bytes not set correctly", arr2[i] == (byte)0);
					assertTrue("chars not set correctly", arr3[i] == '\0');
					assertTrue("shorts not set correctly", arr4[i] == (short)0);
					assertTrue("ints not set correctly", arr5[i] == 0);
					assertTrue("longs not set correctly", arr6[i] == 0L);
					assertTrue("floats not set correctly", arr7[i] == 0f);
					assertTrue("doubles not set correctly", arr8[i] == 0d);
				}
			}
			
			for (int i=0;i<10;i++){
				arr1[i] = false;
				arr2[i] = (byte)0;
				arr3[i] = '\0';
				arr4[i] = (short)0;
				arr5[i] = 0;
				arr6[i] = 0L;
				arr7[i] = 0f;
				arr8[i] = 0d;
			}
			testClass.testSetArrayRegion(
					0,1,
					arr1,arr2,arr3,arr4,arr5,arr6,arr7,arr8,
					true,(byte)1,'a',(short)2,3,4L,5.5f,6.6d);
			for (int i=0;i<10;i++){
				if (i == 0){
					assertTrue("booleans not set correctly", arr1[i] == true);
					assertTrue("bytes not set correctly", arr2[i] == 1);
					assertTrue("chars not set correctly", arr3[i] == 'a');
					assertTrue("shorts not set correctly", arr4[i] == 2);
					assertTrue("ints not set correctly", arr5[i] == 3);
					assertTrue("longs not set correctly", arr6[i] == 4L);
					assertTrue("floats not set correctly", arr7[i] == 5.5f);
					assertTrue("doubles not set correctly", arr8[i] == 6.6d);
				} else {
					assertTrue("booleans not set correctly", arr1[i] == false);
					assertTrue("bytes not set correctly", arr2[i] == (byte)0);
					assertTrue("chars not set correctly", arr3[i] == '\0');
					assertTrue("shorts not set correctly", arr4[i] == (short)0);
					assertTrue("ints not set correctly", arr5[i] == 0);
					assertTrue("longs not set correctly", arr6[i] == 0L);
					assertTrue("floats not set correctly", arr7[i] == 0f);
					assertTrue("doubles not set correctly", arr8[i] == 0d);
				}
			}
			
			for (int i=0;i<10;i++){
				arr1[i] = false;
				arr2[i] = (byte)0;
				arr3[i] = '\0';
				arr4[i] = (short)0;
				arr5[i] = 0;
				arr6[i] = 0L;
				arr7[i] = 0f;
				arr8[i] = 0d;
			}
			testClass.testSetArrayRegion(
					6,4,
					arr1,arr2,arr3,arr4,arr5,arr6,arr7,arr8,
					true,(byte)1,'a',(short)2,3,4L,5.5f,6.6d);
			for (int i=0;i<10;i++){
				if (i == 6){
					assertTrue("booleans not set correctly", arr1[i] == true);
					assertTrue("bytes not set correctly", arr2[i] == 1);
					assertTrue("chars not set correctly", arr3[i] == 'a');
					assertTrue("shorts not set correctly", arr4[i] == 2);
					assertTrue("ints not set correctly", arr5[i] == 3);
					assertTrue("longs not set correctly", arr6[i] == 4L);
					assertTrue("floats not set correctly", arr7[i] == 5.5f);
					assertTrue("doubles not set correctly", arr8[i] == 6.6d);
				} else if (i == 7){
					assertTrue("booleans not set correctly", arr1[i] == true);
					assertTrue("bytes not set correctly", arr2[i] == 2);
					assertTrue("chars not set correctly", arr3[i] == 'b');
					assertTrue("shorts not set correctly", arr4[i] == 3);
					assertTrue("ints not set correctly", arr5[i] == 4);
					assertTrue("longs not set correctly", arr6[i] == 5L);
					assertTrue("floats not set correctly", arr7[i] == 6.5f);
					assertTrue("doubles not set correctly", arr8[i] == 7.6d);
				} else if (i == 8){
					assertTrue("booleans not set correctly", arr1[i] == true);
					assertTrue("bytes not set correctly", arr2[i] == 3);
					assertTrue("chars not set correctly", arr3[i] == 'c');
					assertTrue("shorts not set correctly", arr4[i] == 4);
					assertTrue("ints not set correctly", arr5[i] == 5);
					assertTrue("longs not set correctly", arr6[i] == 6L);
					assertTrue("floats not set correctly", arr7[i] == 7.5f);
					assertTrue("doubles not set correctly", arr8[i] == 8.6d);
				} else if (i == 9){
					assertTrue("booleans not set correctly", arr1[i] == true);
					assertTrue("bytes not set correctly", arr2[i] == 4);
					assertTrue("chars not set correctly", arr3[i] == 'd');
					assertTrue("shorts not set correctly", arr4[i] == 5);
					assertTrue("ints not set correctly", arr5[i] == 6);
					assertTrue("longs not set correctly", arr6[i] == 7L);
					assertTrue("floats not set correctly", arr7[i] == 8.5f);
					assertTrue("doubles not set correctly", arr8[i] == 9.6d);
				} else {
					assertTrue("booleans not set correctly", arr1[i] == false);
					assertTrue("bytes not set correctly", arr2[i] == (byte)0);
					assertTrue("chars not set correctly", arr3[i] == '\0');
					assertTrue("shorts not set correctly", arr4[i] == (short)0);
					assertTrue("ints not set correctly", arr5[i] == 0);
					assertTrue("longs not set correctly", arr6[i] == 0L);
					assertTrue("floats not set correctly", arr7[i] == 0f);
					assertTrue("doubles not set correctly", arr8[i] == 0d);
				}
			}
			
			for (int i=0;i<10;i++){
				arr1[i] = false;
				arr2[i] = (byte)0;
				arr3[i] = '\0';
				arr4[i] = (short)0;
				arr5[i] = 0;
				arr6[i] = 0L;
				arr7[i] = 0f;
				arr8[i] = 0d;
			}
			testClass.testSetArrayRegion(
					0,0,
					arr1,arr2,arr3,arr4,arr5,arr6,arr7,arr8,
					true,(byte)1,'a',(short)2,3,4L,5.5f,6.6d);
			for (int i=0;i<10;i++){
				assertTrue("booleans not set correctly", arr1[i] == false);
				assertTrue("bytes not set correctly", arr2[i] == (byte)0);
				assertTrue("chars not set correctly", arr3[i] == '\0');
				assertTrue("shorts not set correctly", arr4[i] == (short)0);
				assertTrue("ints not set correctly", arr5[i] == 0);
				assertTrue("longs not set correctly", arr6[i] == 0L);
				assertTrue("floats not set correctly", arr7[i] == 0f);
				assertTrue("doubles not set correctly", arr8[i] == 0d);
			}
			
			try {
				testClass.testSetArrayRegion(
						11,1,
						arr1,arr2,arr3,arr4,arr5,arr6,arr7,arr8,
						true,(byte)1,'a',(short)2,3,4L,5.5f,6.6d);
				fail("Expected index out of bounds exception");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			
			try {
				testClass.testSetArrayRegion(
						-1,5,
						arr1,arr2,arr3,arr4,arr5,arr6,arr7,arr8,
						true,(byte)1,'a',(short)2,3,4L,5.5f,6.6d);
				fail("Expected index out of bounds exception for negative start");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			
			try {
				testClass.testSetArrayRegion(
						0,-5,
						arr1,arr2,arr3,arr4,arr5,arr6,arr7,arr8,
						true,(byte)1,'a',(short)2,3,4L,5.5f,6.6d);
				fail("Expected index out of bounds exception for negative length");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			
		} catch (Exception e){
			fail("Exception occurred in testSetArrayRegion:" + e.toString());
		}
	}
		
	/**
	 * tests Set<type>Field and SetStatic<type>Field
	 */
	public void testSetField(){
		TestNatives testClass = new TestNatives();
		
		testClass.testSetObjectField(new TestNatives(10));
		testClass.testSetBooleanField(true);
		testClass.testSetByteField((byte)1);
		testClass.testSetCharField('a');
		testClass.testSetShortField((short)2);
		testClass.testSetIntField(3);
		testClass.testSetLongField(4L);
		testClass.testSetFloatField(5.5f);
		testClass.testSetDoubleField(6.6d);
		
		testClass.testSetStaticObjectField(new TestNatives(20));
		testClass.testSetStaticBooleanField(false);
		testClass.testSetStaticByteField((byte)0);
		testClass.testSetStaticCharField('\u0041');
		testClass.testSetStaticShortField((short)032);
		testClass.testSetStaticIntField(0x12345678);
		testClass.testSetStaticLongField(1234L);
		testClass.testSetStaticFloatField(123.4f);
		testClass.testSetStaticDoubleField(1.234e2);
		
		assertTrue("SetObjectField failed", ((TestNatives)testClass.testGetObjectField()).getIntField() == 10);		
		assertTrue("SetBooleanField failed", testClass.testGetBooleanField() == true);
		assertTrue("SetByteField failed", testClass.testGetByteField() == 1);
		assertTrue("SetCharField failed", testClass.testGetCharField() == 'a');
		assertTrue("SetShortField failed", testClass.testGetShortField() == 2);
		assertTrue("SetIntField failed", testClass.testGetIntField() == 3);
		assertTrue("SetLongField failed", testClass.testGetLongField() == 4L);
		assertTrue("SetFloatField failed", testClass.testGetFloatField() == 5.5f);
		assertTrue("SetDoubleField failed", testClass.testGetDoubleField() == 6.6d);
		
		assertTrue("SetStaticObjectField failed", ((TestNatives)testClass.testGetStaticObjectField()).getIntField() == 20);		
		assertTrue("SetStaticBooleanField failed", testClass.testGetStaticBooleanField() == false);
		assertTrue("SetStaticByteField failed", testClass.testGetStaticByteField() == 0);
		assertTrue("SetStaticCharField failed", testClass.testGetStaticCharField() == 'A');
		assertTrue("SetStaticShortField failed", testClass.testGetStaticShortField() == 032);
		assertTrue("SetStaticIntField failed", testClass.testGetStaticIntField() == 0x12345678);
		assertTrue("SetStaticLongField failed", testClass.testGetStaticLongField() == 1234L);
		assertTrue("SetStaticFloatField failed", testClass.testGetStaticFloatField() == 123.4f);
		assertTrue("SetStaticDoubleField failed", testClass.testGetStaticDoubleField() == 1.234e2);

		/* additional test to catch case that failed on initial JCK runs */
		testClass.testSetCharField('\uFFFF');
		assertTrue("SetCharField failed", testClass.testGetCharField() == '\uFFFF');
	}
		
	/**
	 * tests SetObjectArrayElement
	 */
	public void testSetObjectArrayElement(){
		TestNatives testClass = new TestNatives(0);
		try {
			Object[] array1 = new Object[10];
			
			testClass.testSetObjectArrayElement(array1,5,this);
			assertTrue("object not set", array1[5].equals(this));
			
			for (int i=0;i<10;i++){
				testClass.testSetObjectArrayElement(array1,i,this);
			}
			
			for (int i=3;i<8;i++){
				testClass.testSetObjectArrayElement(array1,i,testClass);
			}
			
			for (int i=0;i<10;i++){
				if ((i >2)&&(i<8)){
					assertTrue("objects not set correctly", array1[i].equals(testClass));
				} else {
					assertTrue("objects not set correctly", array1[i].equals(this));
				}
			}
			
			try {
				testClass.testSetObjectArrayElement(array1,-5,testClass);
				fail("Expected index out of bounds exception for negative index");
			} catch(ArrayIndexOutOfBoundsException e){
			}
			
		} catch (Exception e){
			fail("Exception occurred in testSetArrayLength:" + e.toString());
		}
	}
	
	/**
	 * tests Throw
	 */	
	public void testThrow(){
		TestNatives testClass = new TestNatives(0);
		Throwable t1 = new Throwable("I've been thrown!");
		try {
			int result = testClass.testThrow(t1);
			assertTrue("Incorrect testThrow invocation", result == 0);
		} catch (Throwable t2) {
			assertEquals("Threw a different Throwable object", t1, t2);
		}
	}
	
	/**
	 * tests ThrowNew
	 */	
	public void testThrowNew(){
		TestNatives testClass = new TestNatives(0);
		try {
			int result = testClass.testThrowNew();
			assertTrue("Incorrect testThrowNew invocation", result == 0);
		} catch (Throwable t) {
			assertTrue("Threw is not an instance of Exception", t instanceof Exception);
		}
	}
	
	/**
	 * tests ToReflectedField
	 */	
	public void testToReflectedField(){
		TestNatives testClass = new TestNatives(100);
		Field result;
		try {
			//intField is set to 100 by the constructor
			result = (Field)testClass.testToReflectedField(1);
			assertFalse("Incorrect testToReflectedField (non-static) invocation", result == null);
			assertTrue("Field name not correct", result.getName() == "intField");
			assertTrue("Field value not correct", result.getInt(testClass) == 100);
			
			TestNatives.staticIntField = 200;
			result = (Field)testClass.testToReflectedField(2);
			assertFalse("Incorrect testToReflectedField (static) invocation", result == null);
			assertTrue("Field name not correct", result.getName() == "staticIntField");
			assertTrue("Field value not correct", result.getInt(testClass) == 200);
		} catch (Exception e) {
			fail("Exception occurred in testToReflectedField:" + e.toString());
		}
	}
	
	/**
	 * tests ToReflectedMethod
	 */	
	public void testToReflectedMethod(){
		TestNatives testClass = new TestNatives(0);
		Method result;
		Constructor result_c;
		try {
			result = (Method)testClass.testToReflectedMethod(1);
			assertFalse("Incorrect testToReflectedMethod (non-static) invocation", result == null);
			assertTrue("Method name not correct", result.getName() == "voidMethod");
			assertTrue("Return type not correct", result.getReturnType().getName() == "void");
			
			result = (Method)testClass.testToReflectedMethod(2);
			assertFalse("Incorrect testToReflectedMethod (static) invocation", result == null);
			assertTrue("Method name not correct", result.getName() == "staticVoidMethod_oneArg");
			assertTrue("Return type not correct", result.getReturnType().getName() == "void");
			
			result_c = (Constructor)testClass.testToReflectedMethod(3);
			assertFalse("Incorrect testToReflectedMethod (constructor) invocation", result_c == null);
			assertTrue("Method name not correct", result_c.getName() == "com.ibm.j9.offload.tests.jniservice.TestNatives");
			assertTrue("First parameter type not correct", result_c.getParameterTypes()[0].getName() == "int");
		} catch (Exception e) {
			fail("Exception occurred in testToReflectedMethod:" + e.toString());
		}
	}
	
	/**
	 * tests GetEnv
	 */
	public void testGetEnv(){
		TestNatives testClass = new TestNatives(0);
		try {
			int result = testClass.testGetEnv();
			assertTrue("Incorrect testGetEnv invocation", result == 0);
		} catch (Exception e) {
			fail("Exception occurred in testGetEnv:" + e.toString());
		}
	}
}
