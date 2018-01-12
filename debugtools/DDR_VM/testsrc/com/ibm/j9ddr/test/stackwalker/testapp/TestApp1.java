/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.test.stackwalker.testapp;

import java.util.LinkedList;
import java.util.List;

/**
 * Application that engineers a complicated call stack, then generates
 * a system dump.
 * 
 * @author andhall
 */
public class TestApp1
{

	/**
	 * Value used for creating conditionals to remove warnings
	 */
	private static boolean falseValue = false;
	
	private static List<Object> list = new LinkedList<Object>();
	
	/**
	 * Designed to stress j9localmap_ArgBitsForPC0.
	 * 
	 * Has 65 arguments - forces three words for signature map
	 */
	private static void manyArgumentsMethod(TestApp1 instance,
											int i1,
											int i2,
											int i3,
											Object o1,
											int i4,
											int i5,
											int i6,
											int i7,
											Object o2,
											int i8,
											int i9,
											int i10,
											Object o3,
											int i11,
											long l1,
											long l2,
											long l3,
											Object o4,
											long l4,
											long l5,
											long l6,
											long l7,
											Object o5,
											long l8,
											long l9,
											long l10,
											Object o6,
											char c1,
											char c2,
											char c3,
											Object o7,
											double d1,
											double d2,
											double d3,
											double d4,
											Object o8,
											double d5,
											double d6,
											double d7,
											Object o9,
											double d8,
											double d9,
											byte b1,
											byte b2,
											Object o10,
											byte b3,
											byte b4,
											byte b5,
											Object o11,
											byte b6,
											byte b7,
											byte b8,
											Object o12,
											byte b9,
											float f1,
											float f2,
											float f3,
											Object o13,
											float f4, 
											float f5, 
											float f6,
											Object o14,
											float f7,
											float f8,
											float f9,
											float f10
											)
	{
		instance.manyLocalsMethod();
	}
	
	/* Has huge numbers of locals that need to be mapped */
	private void manyLocalsMethod()
	{
		int i1;
		int i2;
		int i3;
		int i4;
		int i5;
		int i6;
		int i7;
		int i8;
		int i9;
		Object o1 = new Object();
		int i10;
		int i11;
		int i12;
		int i13;
		int i14;
		int i15;
		int i16;
		int i17;
		int i18;
		long l1;
		long l2;
		long l3;
		long l4;
		Object o2 = new Object();
		long l5;
		long l6;
		long l7;
		long l8;
		long l9;
		long l10;
		Object o3 = new Object();
		long l11;
		long l12;
		long l13;
		long l14;
		long l15;
		long l16;
		long l17;
		double d1;
		double d2;
		Object o4 = new Object();
		double d3;
		double d4;
		double d5;
		double d6;
		double d7;
		double d8;
		double d9;
		double d10;
		double d11;
		Object o5 = new Object();
		double d12;
		double d13;
		double d14;
		double d15;
		
		exerciseOperandStackComplex(o1,o2,o3);
		
		list.add(o1);
		list.add(o2);
		list.add(o3);
		list.add(o4);
		list.add(o5);
	}

	private static Object staticObjVar1 = new Object();
	
	private long longField1 = 10;
	
	
	
	/**
	 * Calls with a few elements on the operand stack.
	 * 
	 * Uses some branches, ifs, conditionals etc. to exercise StackMap more thoroughly.
	 * 
	 * This calls exerciseOperandStackSimple so it's below it (and the stack walker will
	 * try simple before complex, starting from the top)
	 */
	private void exerciseOperandStackComplex(Object o1, Object o2, Object o3)
	{
		try {
			throw new RuntimeException("Oops");
		} catch (Exception ex)
		{
			int a = 10;
			int b = 4;
			int c = 0;
	
			switch (b) {
			case 1:
				c++;
				break;
			case	 2:
				c--;
				break;
			default:
				
			}
			
			while (a > 0) {
				
				switch (b) {
				case 1:
					b = 2;
					break;
				case 4:
					b = 1;
					break;
				case 6:
					break;
				default:
					b = 6;
					continue;
				}
				
				a--;
			}
			
			if (a==0) {
				staticObjVar1 = new Object();
				longField1 = 42;
				long result = staticObjVar1.hashCode() + longField1 + ( new int[45][45][12].length ) + dummyMethod(o1, 42, o2, 17, new Object(), exerciseOperandStackSimple(o1, o2, o3));
				
				result--;
			} else {
				b--;
			}
		}
	}

	/**
	 * Aims to force a few objects onto the operand stack prior to making a
	 * call. Simplest case for StackMap.
	 */
	private int exerciseOperandStackSimple(Object o1, Object o2, Object o3)
	{
		return dummyMethod(createObject(), 42, lotsOfIfs(), 100, createObject(), null);
	}

	private Object lotsOfIfs()
	{
		Object obj = new Object();
		
		boolean bool = true;
		int a = 100;
		int b = 200;
		int c = 300;
		int d = 400;
		
		if (bool) {
			if (a > 50) {
				if (b < 10) {
					createObject();
				} else {
					if (c < 10) {
						createObject();
					} else if (c == 15) {
						createObject();
					} else if (c == 300) {
						//CALL OUT HERE
						complicatedLoop();
					} else {
						createObject();
					}
				}
			} else {
				createObject();
			}
		}
		
		return obj;
	}

	/**
	 * Loop with some named breaks and continues in it
	 */
	private void complicatedLoop()
	{
		Object o = new Object();
		int a = 91;
		int b = 23;
		int c = 100;
		
OUTER:	for (int i=0;i!=100;i++) {
			createObject();
			
			int j;
			
			if (a == 100) {
				j = 11;
			} else {
				j = 0;
			}
			
INNER:		for (;j < 10;j++) {
				
				if (a < 100) {
					a++;
					continue INNER;
				} else {
					continue OUTER;
				}
			}
			
			//call out here
			createDump();
			break;
		}
	}

	private void createDump()
	{
		com.ibm.jvm.Dump.SystemDump();
	}

	private static Object createObject()
	{
		return new Object();
	}

	/* Method isn't on the path to creating the dump - just used
	 * as part of exerciseOperandStack
	 */
	private int dummyMethod(Object object, int i, Object object3,
			int j, Object object5, Object object6)
	{
		return 27;
	}

	private synchronized void manySwitchesMethod(Object two, int i, int j, long k)
	{
		int a = 50;
		int b = 100;
		int c = 200;
		int d = 300;
		int e = 400;
		//Variable we twiddle in various cases.
		int mutatee = 1;
		
		switch(a) {
		case 10:
			mutatee++;
			break;
		case 20:
			mutatee++;
			mutatee++;
			break;
		case 30:
			mutatee++;
			mutatee++;
			mutatee++;
			break;
		case 40:
		case 50:
			switch(b) {
			case 60:
				mutatee++;
				break;
			case 101:
				mutatee++;
				mutatee--;
				break;
			case 99:
				mutatee++;
				break;
			default:
				switch (c) {
				default:
					switch (d) {
					case 300:
						switch(e) {
						case 1:
							mutatee++;
							break;
						case 2:
							mutatee--;
							break;
						}
						
						//CALL OUT HERE
						manyArgumentsMethod(this,
								1,
								2,
								3,
								new Object(),
								4,
								5,
								6,
								7,
								new Object(),
								8,
								9,
								10,
								new String(),
								11,
								1L,
								2L,
								3L,
								new Thread(),
								4L,
								5L,
								6L,
								7L,
								new Object(),
								8L,
								9L,
								10L,
								new Object(),
								'a',
								'b',
								'c',
								new Character('Z'),
								3.141459,
								43.2,
								23432.234,
								23432.23423,
								new Object(),
								1323.12321,
								123213.123213,
								123123.123213,
								234234.234234,
								243234.234324,
								234324.242342,
								(byte)4,
								(byte)5,
								new Object(),
								(byte)6,
								(byte)7,
								(byte)8,
								new Object(),
								(byte)9,
								(byte)10,
								(byte)11,
								null,
								(byte)12,
								1.1f,
								1.2f,
								1.3f,
								new Object(),
								1.4f,
								1.5f,
								1.6f,
								new Object(),
								1.7f,
								1.8f,
								1.9f,
								1.10f);
						return;
					case 101:
						mutatee = 42;
						break;
					case 1000:
					case 1001:
						return;
					default:
						throw new RuntimeException("Shouldn't get here");
					}
				}
			}
			break;
		}
		
		String str = "Shouldn't get here either";
		throw new RuntimeException(str);
	}
	
	/**
	 * Calls on from a nested exception handler. Designed to confuse localmap
	 */
	private void complicatedExceptionMethod(Object one) {
		long myLong = 100L;
		Thread t = Thread.currentThread();
		try  {
			Object foo = null;
			
			if (falseValue) {
				foo = new Object();
			}
			
			foo.toString();
			
			Object bar = new Object();
			
			Object baz = new Object();
			
			String frog = bar.toString() + baz.toString();
			frog.length();
		} catch (Exception ex) {
			try {
				String str = "";
				for (int i=0;i!=100;i++) {
					str = str + " rhubarb";
				}
				
				if (falseValue) {
					str = str + "other";
				} else {
					str = str + "something else";
				}
				
			} catch (RuntimeException ex1){
				int i = 0;
				int j = 100;
				Object foo = new Object();
				foo.toString();
				i++;
				j++;
			}
			
			try {
				if (!falseValue) {
					throw new UnsupportedOperationException("Yikes!");
				}
			} catch (Throwable ex2){
				int i = 0;
				int j = 100;
				Object foo = new Object();
				
				i++;
				j--;
				foo.hashCode();
				
				//Call on is here.
				manySwitchesMethod(foo, i, j, myLong);
				
			}
			
			try {
				String str = "";
				for (int i=0;i!=100;i++) {
					str = str + " rhubarb";
				}
				
				if (falseValue) {
					str = str + "other";
				} else {
					str = str + "something else";
				}
				
			} catch (RuntimeException ex1){
				int i = 0;
				int j = 100;
				Object foo = new Object();
				foo.toString();
				i++;
				j++;
			}
		}
		
		//Prevent objects being escaped-analyzed away
		t.toString();
	}
	
	private static void driveSwitchLocalMap()
	{
		Object o = new Object();
		Object b = new Object();
		int a = 10;
		
		new TestApp1().complicatedExceptionMethod(new Object());
		
		switch(a) {
		case 10:
			o = new Object();
		default:
			b = new Object();
		}
	}
	
	
	static Object[] myArray;
	
	/**
	 * Attempts to persuade the JIT to create internal pointers
	 */
	private void driveInternalPointerFixUp()
	{
		Object[] localArray = new Object[100];
		int a = 0;
		int b = 1;
		int c = 4;
		int d = 8;
		
		try {
			d++;
			for (int i=0;i!=localArray.length;i++) {
				localArray[i] = createObject();
				b++;
			}
			Object o = new Object();
			o.toString();
			
			a++;
			b++;
			c = a + b;
			d = c;
			c = a;
		} catch (RuntimeException ex) {
			ex.printStackTrace();
		}finally {
			myArray = localArray;
			Object p = new Object();
			Object q = new Object();
			Object r = new Object();
			
			String s = "" + p + q + r;
			
			s.hashCode();
		}
		
		driveSwitchLocalMap();
		
		return;
	}
	
	/**
	 * Synchronizes on a couple of objects prior to 
	 * calling on
	 */
	private static void makeSynchronizedCall(Object a, int i, int j)
	{
		Object one = new Object();
		Object two = new Object();
		
		synchronized(one) {
			synchronized(two) {
				new TestApp1().driveInternalPointerFixUp();
			}
		}
	}
	
	/**
	 * @param args
	 */
	public static void main(String[] args)
	{
		makeSynchronizedCall(new Object(), 3, 6);
	}

}
