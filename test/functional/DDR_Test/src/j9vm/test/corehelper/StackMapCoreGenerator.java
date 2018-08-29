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
package j9vm.test.corehelper;

import java.util.HashMap;
import java.util.LinkedList;

/**
 * 
 * @author fkaraman
 * 
 *         This class is used to generate core file in a specific method. This
 *         way, stackmap info of the methods in the stacklist will be known.
 * 
 *         Stack map values are generated depending on the value being primitive
 *         or non-primitive.
 * 
 *         Primitive types are represented by '0' in the stack map.
 *         Non-primitive types are represented by '1' in the stack map.
 * 
 *         Assume that method_1 calls method_2 and method_2 throws exception and
 *         core is generated. See below:
 * 
 *         public void method_1 { method_2(1, new Object()); }
 * 
 *         public void method_2(int i, Object o) { // Throws exception and
 *         causes core generation }
 * 
 *         In this case, stackmap for method_1 will be :
 * 
 *         101 1 is for 'Object o' non-primitive type, 0 is for int primitive
 *         type 1 is for function call
 * 
 *         PS: long and double primitive types use 2 stacks since they are using
 *         64 bit. So double zero (00) printed for them in the stackmap.
 * 
 * 
 * 
 */
public class StackMapCoreGenerator {

	enum WeekDays {
		MON, TUE, WED, THU, FRI, SAT, SUN
	};

	/**
	 * main method of this class to generate core file.
	 * 
	 * @param args
	 */
	public static void main(String[] args) {
		StackMapCoreGenerator stackMapTest = new StackMapCoreGenerator();
		stackMapTest.stackMapTestMethod_1();
	}

	/**
	 * Stack map of this method should be 111
	 */
	public void stackMapTestMethod_1() {
		stackMapTestMethod_2(null, WeekDays.MON);

	}

	/**
	 * Stack map of this method should be 100101
	 * 
	 * @param o
	 *            dummy arg
	 * @param monday
	 *            dummy arg
	 */
	public void stackMapTestMethod_2(Object o, WeekDays monday) {
		char[] charArray = { 't', 'e', 's', 't' };
		stackMapTestMethod_3(1, "test", 9, charArray);

	}

	/**
	 * Stack map of this method should be 1011001
	 * 
	 * @param i
	 *            Dummy arg
	 * @param s
	 *            Dummy arg
	 * @param l
	 *            Dummy arg
	 * @param charArray
	 *            Dummy arg
	 */
	public void stackMapTestMethod_3(int i, String s, long l, char[] charArray) {
		int[] intArray = new int[10];
		stackMapTestMethod_4('0', false, new Integer("1"), new Exception(), 1,
				intArray);

	}

	/**
	 * Stack map of this method should be 01000111
	 * 
	 * @param c
	 *            Dummy arg
	 * @param b
	 *            Dummy arg
	 * @param intObj
	 *            Dummy arg
	 * @param e
	 *            Dummy arg
	 * @param i
	 *            Dummy arg
	 * @param intArray
	 *            Dummy arg
	 */
	public void stackMapTestMethod_4(char c, boolean b, Integer intObj,
			Exception e, int i, int[] intArray) {
		String[] stringArray = { "dummy", "String", "Array" };
		stackMapTestMethod_5(stringArray, "test", 1.5, 1,
				new LinkedList<Object>(), (short) 2);

	}

	/**
	 * Stack map of this method should be 11
	 * 
	 * @param stringArray
	 *            dummy arg
	 * @param s
	 *            Dummy arg
	 * @param d
	 *            Dummy arg
	 * @param f
	 *            Dummy arg
	 * @param o
	 *            Dummy arg
	 * @param sh
	 *            dummy arg
	 */
	public void stackMapTestMethod_5(String[] stringArray, String s, double d,
			float f, Object o, short sh) {
		stackMapTestMethod_6(new HashMap<String, Object>());

	}

	/**
	 * Stack map of this method should be 00100011011
	 * 
	 * @param h
	 *            Dummy arg
	 */
	public void stackMapTestMethod_6(HashMap<String, Object> h) {
		stackMapTestMethod_7(null, false, "stackmap", "test", 0, 1, this, 'c',
				'h');

	}

	/**
	 * Stack map of this method should be 01
	 * 
	 * @param ll
	 *            Dummy arg
	 * @param b
	 *            Dummy arg
	 * @param s1
	 *            Dummy arg
	 * @param s2
	 *            Dummy arg
	 * @param i
	 *            Dummy arg
	 * @param l
	 *            Dummy arg
	 * @param o
	 *            Dummy arg
	 * @param c1
	 *            Dummy arg
	 * @param c2
	 *            Dummy arg
	 */
	public void stackMapTestMethod_7(LinkedList<Integer> ll, boolean b,
			String s1, String s2, int i, long l, Object o, char c1, char c2) {
		stackMapTestMethod_8(5);

	}

	/**
	 * Stack map of this method should be 1
	 * 
	 * @param i
	 *            Dummy arg
	 */
	public void stackMapTestMethod_8(int i) {
		stackMapTestMethod_9();

	}

	/**
	 * Stack map of this method should be 001
	 * 
	 */
	public void stackMapTestMethod_9() {
		stackMapTestMethod_throwException(1);

	}

	/**
	 * This is the method that throws HelperExceptionForCoreGeneration. By using
	 * this exception and Xdump filters for this specific exception, core file
	 * is generated one this method throws this specific exception.
	 * 
	 * @param l
	 *            Dummy arg
	 */
	public void stackMapTestMethod_throwException(long l) {
		try {
			throw new HelperExceptionForCoreGeneration();
		} catch (HelperExceptionForCoreGeneration e) {
			System.out
					.println("HelperExceptionForCoreGeneration is thrown and caught successfuly!");
		}

	}
}
