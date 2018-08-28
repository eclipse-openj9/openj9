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
package com.ibm.j9.jsr292;
import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import static java.lang.invoke.MethodType.methodType;
import static java.lang.invoke.MethodHandles.*;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import java.lang.invoke.WrongMethodTypeException;
import java.util.Arrays;
import java.util.List;

import examples.PackageExamples;

public class AdaptorTests {
	
	public static void voidReturningMethod() { };
	public static int negativeInt(int i) { return -i; }
	public static int subtract(int x, int y) {
		return x - y;
	}
	public static boolean and(boolean a, boolean b) { return a & b; }
	public static byte add(byte a, byte b) { return (byte) (a + b); }
	public static char add(char a, char b) { return (char) (a + b); }
	public static short add(short a, short b) { return (short) (a + b); }
	public static int add(int x, int y) { return x + y; }
	public static float add(float a, float b) { return a + b; }
	public static double add(double a, double b) { return a + b; }
	public static long add(long a, long b) { return a + b; }
	
	public static void doNothing_I(int i) {}
	public static void doNothing_IIII(int i, int j, int k, int l) {}
	
	public static int add_IIII(int i, int j, int k, int l) { return i + j + k + l;}
	
	static class FooException extends Exception {
		private static final long serialVersionUID = 1L;
	}
	
	static class PackageExamplesCrossPackageSubClass extends examples.PackageExamples {
		public static Lookup getLookup() {
			return MethodHandles.lookup();
		}		
	}

	
	@Test(groups = { "level.extended" })
	public void testMethodHandleCachingVisibilityForStaticDefault () throws Throwable {
		String methodname = "defaultMethod";
		boolean exceptionThrown = false;
		try {
			PackageExamplesCrossPackageSubClass.getLookup().findStatic(PackageExamples.class, methodname, methodType(String.class));
		} catch(IllegalAccessException e) {
			exceptionThrown = true;
		}
		AssertJUnit.assertTrue("here here", exceptionThrown);
		
		// Ensure the MH gets cached
		PackageExamples.getLookup().findStatic(PackageExamples.class, methodname, methodType(String.class));

		// Ensure the MH lookup still fails
		exceptionThrown = false;
		try {
			PackageExamplesCrossPackageSubClass.getLookup().findStatic(PackageExamples.class, methodname, methodType(String.class));
			AssertJUnit.assertTrue("MethodHandle lookup succeeded on protected method method", false);
		} catch(IllegalAccessException e) {
			exceptionThrown = true;
		}
		AssertJUnit.assertTrue(exceptionThrown);
		
		exceptionThrown = false;
		try {
			MethodHandles.lookup().findStatic(PackageExamples.class, methodname, methodType(String.class));
			AssertJUnit.assertTrue("MethodHandle lookup succeeded on protected method method", false);
		} catch(IllegalAccessException e) {
			exceptionThrown = true;
		}
		AssertJUnit.assertTrue(exceptionThrown);
	}
	
	
	@Test(groups = { "level.extended" })
	public void testMethodHandleCachingVisibilityForStaticProtected () throws Throwable {
		String methodname = "protectedMethod";
		boolean exceptionThrown = false;
		try {
			MethodHandles.lookup().findStatic(PackageExamples.class, methodname, methodType(String.class));
		} catch(IllegalAccessException e) {
			exceptionThrown = true;
		}
		AssertJUnit.assertTrue("exception didn't occur using MHs.lookup()", exceptionThrown);
		
		// Ensure the MH gets cached
		PackageExamples.getLookup().findStatic(PackageExamples.class, methodname, methodType(String.class));

		// Ensure the MH lookup still fails
		try {
			PackageExamplesCrossPackageSubClass.getLookup().findStatic(PackageExamples.class, methodname, methodType(String.class));
		} catch(IllegalAccessException e) {
			AssertJUnit.assertTrue("Exception thrown when it shouldn't be", false);
		}
		
		exceptionThrown = false;
		try {
			MethodHandles.lookup().findStatic(PackageExamples.class, methodname, methodType(String.class));
			AssertJUnit.assertTrue("MethodHandle lookup succeeded on protected method", false);
		} catch(IllegalAccessException e) {
			exceptionThrown = true;
		}
		AssertJUnit.assertTrue("exception didn't occur using MHs.lookup()", exceptionThrown);
	}
	
	
	@Test(groups = { "level.extended" })
	public void testMethodHandleCachingVisibilityForStaticPrivate () throws Throwable {
		boolean exceptionThrown = false;
		try {
			MethodHandles.lookup().findStatic(PrivateMethod.class, "method", methodType(String.class));
		} catch(IllegalAccessException e) {
			exceptionThrown = true;
		}
		AssertJUnit.assertTrue(exceptionThrown);
		
		// Ensure the MH gets cached
		PrivateMethod.getLookup().findStatic(PrivateMethod.class, "method", methodType(String.class));

		// Ensure the MH lookup still fails
		exceptionThrown = false;
		try {
			MethodHandles.lookup().findStatic(PrivateMethod.class, "method", methodType(String.class));
			AssertJUnit.assertTrue("MethodHandle lookup succeeded on private method", false);
		} catch(IllegalAccessException e) {
			exceptionThrown = true;
		}
		AssertJUnit.assertTrue(exceptionThrown);
	}
	
	@Test(groups = { "level.extended" })
	public void testExactInvoker() throws WrongMethodTypeException, Throwable {
		MethodType type = methodType(void.class); 
		MethodHandle handle = MethodHandles.exactInvoker(type);
		AssertJUnit.assertEquals("exactInvoker: unexpected type", type.insertParameterTypes(0, MethodHandle.class), handle.type());
		
		MethodHandle voidReturningMethod = MethodHandles.lookup().findStatic(AdaptorTests.class, "voidReturningMethod", type);
		handle.invokeExact(voidReturningMethod);
		
		type = methodType(int.class, int.class); 
		handle = MethodHandles.exactInvoker(type);
		AssertJUnit.assertEquals("exactInvoker: unexpected type", type.insertParameterTypes(0, MethodHandle.class), handle.type());
		AssertJUnit.assertEquals("exactInvoker: incorrect return value", 5, (int)handle.invokeExact(MethodHandles.identity(int.class), 5));
	}

	
	@Test(groups = { "level.extended" })
	public void testInvoker() throws WrongMethodTypeException, NullPointerException, IllegalArgumentException, Throwable {
		MethodType type = methodType(void.class); 
		MethodHandle handle = MethodHandles.invoker(type);
		AssertJUnit.assertEquals("invoker: unexpected type", type.insertParameterTypes(0, MethodHandle.class), handle.type());
		
		MethodHandle voidReturningMethod = MethodHandles.lookup().findStatic(AdaptorTests.class, "voidReturningMethod", type);
		handle.invokeExact(voidReturningMethod);
		
		type = methodType(int.class, int.class); 
		handle = MethodHandles.invoker(type);
		AssertJUnit.assertEquals("invoker: unexpected type", type.insertParameterTypes(0, MethodHandle.class), handle.type());
		AssertJUnit.assertEquals("invoker: incorrect return value", 5, (int)handle.invokeExact(MethodHandles.identity(Integer.class), 5));
	}

	//Causes crash in Oracle
	@Test(groups = { "level.extended" })
	public void testAsSpreader() throws Throwable {
		MethodType IIII_V_type = methodType(void.class, int.class, int.class, int.class, int.class);

		{
			MethodHandle handle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "doNothing_IIII", IIII_V_type);
			MethodHandle spreader = handle
					.asType(methodType(void.class, Integer.class, Integer.class, Integer.class, Integer.class))
					.asSpreader(Integer[].class, 3);
			spreader.invoke((Integer)4, new Integer[] { 1, 2, 3});
		}
		{
			MethodHandle handle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "and", methodType(boolean.class, boolean.class, boolean.class));
			MethodHandle spreader = handle.asSpreader(boolean[].class, 2);
			AssertJUnit.assertFalse((boolean)spreader.invoke(new boolean[] { false, false}));
		}
		{
			MethodHandle handle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "add", methodType(byte.class, byte.class, byte.class));
			MethodHandle spreader = handle.asSpreader(byte[].class, 2);
			AssertJUnit.assertEquals(3, (byte)spreader.invoke(new byte[] { 1, 2}));
		}
		{
			MethodHandle handle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "add", methodType(char.class, char.class, char.class));
			MethodHandle spreader = handle.asSpreader(char[].class, 2);
			AssertJUnit.assertEquals(3, (char)spreader.invoke(new char[] { 1, 2}));
		}
		{
			MethodHandle handle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "add", methodType(short.class, short.class, short.class));
			MethodHandle spreader = handle.asSpreader(short[].class, 2);
			AssertJUnit.assertEquals(3, (short)spreader.invoke(new short[] { 1, 2}));
		}
		{
			MethodHandle handle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "add", methodType(int.class, int.class, int.class));
			MethodHandle spreader = handle.asSpreader(int[].class, 2);
			AssertJUnit.assertEquals(3, (int)spreader.invoke(new int[] { 1, 2}));
		}
		{
			MethodHandle handle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "add", methodType(float.class, float.class, float.class));
			MethodHandle spreader = handle.asSpreader(float[].class, 2);
			AssertJUnit.assertEquals(3, (float)spreader.invoke(new float[] { 1, 2}), .1);
		}
		{
			MethodHandle handle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "add", methodType(double.class, double.class, double.class));
			MethodHandle spreader = handle.asSpreader(double[].class, 2);
			AssertJUnit.assertEquals(3, (double)spreader.invoke(new double[] { 1, 2}), .1);
		}
		{
			MethodHandle handle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "add", methodType(long.class, long.class, long.class));
			MethodHandle spreader = handle.asSpreader(long[].class, 2);
			AssertJUnit.assertEquals(3, (long)spreader.invoke(new long[] { 1, 2}));
		}
		{
			MethodHandle handle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "add", methodType(long.class, long.class, long.class));
			MethodHandle spreader = handle.asSpreader(long[].class, 1);
			AssertJUnit.assertEquals(3, (long)spreader.invoke(2l, new long[] { 1}));
		}
		{
			MethodHandle handle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "add", methodType(long.class, long.class, long.class));
			MethodHandle spreader = handle.asSpreader(long[].class, 0);
			AssertJUnit.assertEquals(3, (long)spreader.invoke(1l, 2l, new long[] { }));
		}
		{
			/* Arg array has too many elements */
			boolean exceptionOccurred = false;
			try {
				MethodHandle handle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "add", methodType(long.class, long.class, long.class));
				MethodHandle spreader = handle.asSpreader(long[].class, 0);
				AssertJUnit.assertEquals(3, (long)spreader.invoke(1l, 2l, new long[] { 1l }));
			} catch(IllegalArgumentException e) {
				exceptionOccurred = true;
			}
			AssertJUnit.assertTrue(exceptionOccurred);
		}
		{
			/* Arg array has too few elements */
			boolean exceptionOccurred = false;
			try {
				MethodHandle handle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "add", methodType(long.class, long.class, long.class));
				MethodHandle spreader = handle.asSpreader(long[].class, 2);
				AssertJUnit.assertEquals(2, (long)spreader.invoke(new long[] { 1l }));
			} catch(IllegalArgumentException e) {
				exceptionOccurred = true;
			}
			AssertJUnit.assertTrue(exceptionOccurred);
		}
		{
			MethodHandle handle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "add", methodType(long.class, long.class, long.class));
			MethodHandle spreader = handle.asSpreader(long[].class, 0);
			AssertJUnit.assertEquals(3, (long)spreader.invoke(1l, 2l, (long[])null));
		}
		{ // This test makes hotspot crash
			boolean exceptionOccurred = false;
			try {
				MethodHandle handle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "add", methodType(long.class, long.class, long.class));
				MethodHandle spreader = handle.asSpreader(long[].class, 1);
				AssertJUnit.assertEquals(3, (long)spreader.invoke(1l, (long[]) null));
			} catch(IllegalArgumentException e) {
				exceptionOccurred = true;
			}
			AssertJUnit.assertTrue(exceptionOccurred);
		}
		{
			boolean exceptionOccurred = false;
			try {
				MethodHandle handle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "add", methodType(long.class, long.class, long.class));
				MethodHandle spreader = handle.asSpreader(long[].class, 1);
				AssertJUnit.assertEquals(3, (long)spreader.invoke(1l, long.class));
			} catch(ClassCastException e) {
				exceptionOccurred = true;
			}
			AssertJUnit.assertTrue(exceptionOccurred);
		}
		{
			boolean exceptionOccurred = false;
			try {
				MethodHandle handle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "add", methodType(long.class, long.class, long.class));
				MethodHandle spreader = handle.asSpreader(long[].class, 1);
				AssertJUnit.assertEquals(3, (long)spreader.invoke(1l, long.class));
			} catch(ClassCastException e) {
				exceptionOccurred = true;
			}
			AssertJUnit.assertTrue(exceptionOccurred);
		}
		
		MethodHandle handle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "doNothing_IIII", IIII_V_type);
		MethodHandle spreader = handle.asSpreader(int[].class, 3);
		AssertJUnit.assertEquals(methodType(void.class, int.class, int[].class), spreader.type());
		spreader.invoke(4, new int[] { 1, 2, 3});
		
		MethodType IIII_I_type = methodType(int.class, int.class, int.class, int.class, int.class);
		
		MethodHandle addHandle = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "add_IIII", IIII_I_type);
		MethodHandle addSpreader = addHandle.asSpreader(int[].class, 4);
		AssertJUnit.assertEquals(methodType(int.class, int[].class), addSpreader.type());
	}

	@Test(groups = { "level.extended" })
	public void test_MethodHandles_SpreadInvoker() throws Throwable {
		MethodType IIII_I_type = methodType( int.class, int.class, int.class, int.class, int.class );
		MethodHandle handle = MethodHandles.publicLookup().findStatic( AdaptorTests.class, "add_IIII", IIII_I_type );
		
		MethodHandle spreadInvoker = null;
		boolean illegalArgumentExceptionThrown = false; 
		
		try {
			spreadInvoker = MethodHandles.spreadInvoker( IIII_I_type, 7 );
		} catch ( IllegalArgumentException e ) {
			illegalArgumentExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue ( illegalArgumentExceptionThrown );
		
		illegalArgumentExceptionThrown = false;
		
		try {
			spreadInvoker = MethodHandles.spreadInvoker( IIII_I_type, -1 );
		} catch ( IllegalArgumentException e ) {
			illegalArgumentExceptionThrown = true;
		}
		
		AssertJUnit.assertTrue ( illegalArgumentExceptionThrown );
		
		spreadInvoker = MethodHandles.spreadInvoker( IIII_I_type, 2 );
		spreadInvoker = spreadInvoker.bindTo( handle );
		
		AssertJUnit.assertEquals(10, (int)spreadInvoker.invoke( 1, 2, new Object []{3,4} ) );
	}
	
	@Test(groups = { "level.extended" })
	public void testGuardWithTest() throws WrongMethodTypeException, Throwable {
		//System.out.println("testGuardWithTest");
		MethodHandle constant_true = MethodHandles.constant(boolean.class, true);
		MethodHandle constant_false = MethodHandles.constant(boolean.class, false);
		MethodHandle identity = MethodHandles.identity(int.class);
		MethodHandle negativeInt = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "negativeInt", methodType(int.class, int.class));

		// if (true_gwt()) { identity(5) } else { negativeInt(5) } 
		MethodHandle true_gwt = MethodHandles.guardWithTest(constant_true, identity, negativeInt);
		AssertJUnit.assertEquals("should have called identity()", 5, (int)true_gwt.invokeExact((int)5));

		// if (false_gwt()) { identity(5) } else { negativeInt(5) }
		MethodHandle false_gwt = MethodHandles.guardWithTest(constant_false, identity, negativeInt);
		AssertJUnit.assertEquals("should have called identity()", -5, (int)false_gwt.invokeExact((int)5));
		
		// if (true_gwt(int, String, String)) { identity(5, string, String) } else { negativeInt(5, String, String) }
		MethodHandle true_gwt_with_drop = MethodHandles.guardWithTest(
				MethodHandles.dropArguments(constant_true, 0, int.class, String.class, String.class), 
				MethodHandles.dropArguments(identity, 1, String.class, String.class),
				MethodHandles.dropArguments(negativeInt, 1, String.class, String.class));
		AssertJUnit.assertEquals("should have called identity()", 5, (int)true_gwt_with_drop.invokeExact((int)5, "foo", "bar"));
		
	}

	@Test(groups = { "level.extended" })
	public void testCatchException() throws Throwable {
		//System.out.println("testCatchException");
		MethodHandle thrower = MethodHandles.foldArguments(
				MethodHandles.throwException(boolean.class, FooException.class),
				MethodHandles.lookup().findConstructor(FooException.class, methodType(void.class)));
		
		MethodHandle catcher = MethodHandles.dropArguments(MethodHandles.constant(boolean.class, true), 0, FooException.class);
		System.out.println("testCatchException.0");
		// Validate that the 'catch' handle runs for the exception
		MethodHandle runCatcher = MethodHandles.catchException(thrower, FooException.class, catcher);
		AssertJUnit.assertEquals(methodType(boolean.class), runCatcher.type());
		System.out.println("testCatchException.05");
		try {
		boolean b = (boolean)runCatcher.invokeExact();
		System.out.println("boolean was: " + b);
		} catch(Throwable t) {
			System.out.println("caught " + t);
			System.out.println(FooException.class.isInstance(t));
			
		}
		
		AssertJUnit.assertTrue((boolean)runCatcher.invokeExact());
		
		
		System.out.println("testCatchException.1");
		// Validate that the 'catch' handle doesn't run when no exception occurs
		MethodHandle noException = MethodHandles.catchException(MethodHandles.constant(boolean.class, false), FooException.class, catcher);
		AssertJUnit.assertEquals(methodType(boolean.class), runCatcher.type());
		AssertJUnit.assertFalse((boolean)noException.invokeExact());
		System.out.println("testCatchException.2");
		// Validate that catcher doesn't need to accept all the args of the try handle
		runCatcher = MethodHandles.catchException(MethodHandles.dropArguments(thrower, 0, String.class, int.class), FooException.class, catcher);
		AssertJUnit.assertEquals(methodType(boolean.class, String.class, int.class), runCatcher.type());
		AssertJUnit.assertTrue((boolean)runCatcher.invokeExact("Ignored", 5));
		System.out.println("testCatchException.3");
		
		// Validate that catcher can accept some of the args
		runCatcher = MethodHandles.catchException(
				MethodHandles.dropArguments(thrower, 0, String.class, int.class), 
				FooException.class, 
				MethodHandles.dropArguments(catcher, 1, String.class));
		AssertJUnit.assertEquals(methodType(boolean.class, String.class, int.class), runCatcher.type());
		AssertJUnit.assertTrue((boolean)runCatcher.invokeExact("CatcherAcceptsThisArg", 5));
		System.out.println("testCatchException.4");
	}

	
	@Test(groups = { "level.extended" })
	public void testIdentity() throws WrongMethodTypeException, NullPointerException, IllegalArgumentException, Throwable {
		//System.out.println("testIdentity");
		AssertJUnit.assertEquals(5, (int)MethodHandles.identity(int.class).invokeExact(5));
		System.out.println("testIdentity.1");
		AssertJUnit.assertEquals('c', (char)MethodHandles.identity(char.class).invokeExact('c'));
		System.out.println("testIdentity.2");
		AssertJUnit.assertEquals("String", (String)MethodHandles.identity(String.class).invokeExact("String"));
		System.out.println("testIdentity.3");
	}

	
	@Test(groups = { "level.extended" })
	public void testConstant() throws WrongMethodTypeException, NullPointerException, ClassCastException, IllegalArgumentException, Throwable {
		AssertJUnit.assertEquals(5, (int)MethodHandles.constant(int.class, 5).invokeExact());
		AssertJUnit.assertEquals('c', (char)MethodHandles.constant(char.class, 'c').invokeExact());
		AssertJUnit.assertEquals("String", (String)MethodHandles.constant(String.class, "String").invokeExact());
		AssertJUnit.assertEquals(true, (boolean)MethodHandles.constant(boolean.class, true).invokeExact());
		AssertJUnit.assertEquals(false, (boolean)MethodHandles.constant(boolean.class, false).invokeExact());
		AssertJUnit.assertEquals(0, (byte)MethodHandles.constant(byte.class, (byte)0).invokeExact());
		AssertJUnit.assertEquals(Byte.MIN_VALUE, (byte)MethodHandles.constant(byte.class, Byte.MIN_VALUE).invokeExact());
		AssertJUnit.assertEquals(Byte.MAX_VALUE, (byte)MethodHandles.constant(byte.class, Byte.MAX_VALUE).invokeExact());
		AssertJUnit.assertEquals(0, (short)MethodHandles.constant(short.class, (short)0).invokeExact());
		AssertJUnit.assertEquals(Short.MIN_VALUE, (short)MethodHandles.constant(short.class, Short.MIN_VALUE).invokeExact());
		AssertJUnit.assertEquals(Short.MAX_VALUE, (short)MethodHandles.constant(short.class, Short.MAX_VALUE).invokeExact());
		AssertJUnit.assertEquals(0, (char)MethodHandles.constant(char.class, (char)0).invokeExact());
		AssertJUnit.assertEquals(Character.MIN_VALUE, (char)MethodHandles.constant(char.class, Character.MIN_VALUE).invokeExact());
		AssertJUnit.assertEquals(Character.MAX_VALUE, (char)MethodHandles.constant(char.class, Character.MAX_VALUE).invokeExact());
		AssertJUnit.assertEquals((float)0, (float)MethodHandles.constant(float.class, (float)0).invokeExact());
		AssertJUnit.assertEquals(Float.MAX_VALUE, (float)MethodHandles.constant(float.class, Float.MAX_VALUE).invokeExact());
		AssertJUnit.assertEquals(Float.MIN_VALUE, (float)MethodHandles.constant(float.class, Float.MIN_VALUE).invokeExact());
		AssertJUnit.assertEquals(0l, (long)MethodHandles.constant(long.class, (long)0).invokeExact());
		AssertJUnit.assertEquals(Long.MAX_VALUE, (long)MethodHandles.constant(long.class, Long.MAX_VALUE).invokeExact());
		AssertJUnit.assertEquals(Long.MIN_VALUE, (long)MethodHandles.constant(long.class, Long.MIN_VALUE).invokeExact());
		AssertJUnit.assertEquals(0d, (double)MethodHandles.constant(double.class, (double)0).invokeExact());
		AssertJUnit.assertEquals(Double.MAX_VALUE, (double)MethodHandles.constant(double.class, Double.MAX_VALUE).invokeExact());
		AssertJUnit.assertEquals(Double.MIN_VALUE, (double)MethodHandles.constant(double.class, Double.MIN_VALUE).invokeExact());
		AssertJUnit.assertEquals(0, (int)MethodHandles.constant(int.class, 0).invokeExact());
		AssertJUnit.assertEquals(Integer.MAX_VALUE, (int)MethodHandles.constant(int.class, Integer.MAX_VALUE).invokeExact());
		AssertJUnit.assertEquals(Integer.MIN_VALUE, (int)MethodHandles.constant(int.class, Integer.MIN_VALUE).invokeExact());
	}
	
	/* 
	 * This does exactly what testConstant() does except it drops two additional leading arguments.
	 * The purpose of this test is to ensure that the fused constant-drop handle doesn't break anything.
	 * See Jazz103 39342: https://jazz103.hursley.ibm.com:9443/jazz/resource/itemName/com.ibm.team.workitem.WorkItem/39342
	 */
	@Test(groups = { "level.extended" })
	public void testConstantWithDrop() throws WrongMethodTypeException, NullPointerException, ClassCastException, IllegalArgumentException, Throwable {
		AssertJUnit.assertEquals(5, (int)MethodHandles.dropArguments(MethodHandles.constant(int.class, 5), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals('c', (char)MethodHandles.dropArguments(MethodHandles.constant(char.class, 'c'), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals("String", (String)MethodHandles.dropArguments(MethodHandles.constant(String.class, "String"), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(true, (boolean)MethodHandles.dropArguments(MethodHandles.constant(boolean.class, true), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(false, (boolean)MethodHandles.dropArguments(MethodHandles.constant(boolean.class, false), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(0, (byte)MethodHandles.dropArguments(MethodHandles.constant(byte.class, (byte)0), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(Byte.MIN_VALUE, (byte)MethodHandles.dropArguments(MethodHandles.constant(byte.class, Byte.MIN_VALUE), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(Byte.MAX_VALUE, (byte)MethodHandles.dropArguments(MethodHandles.constant(byte.class, Byte.MAX_VALUE), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(0, (short)MethodHandles.dropArguments(MethodHandles.constant(short.class, (short)0), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(Short.MIN_VALUE, (short)MethodHandles.dropArguments(MethodHandles.constant(short.class, Short.MIN_VALUE), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(Short.MAX_VALUE, (short)MethodHandles.dropArguments(MethodHandles.constant(short.class, Short.MAX_VALUE), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(0, (char)MethodHandles.dropArguments(MethodHandles.constant(char.class, (char)0), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(Character.MIN_VALUE, (char)MethodHandles.dropArguments(MethodHandles.constant(char.class, Character.MIN_VALUE), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(Character.MAX_VALUE, (char)MethodHandles.dropArguments(MethodHandles.constant(char.class, Character.MAX_VALUE), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals((float)0, (float)MethodHandles.dropArguments(MethodHandles.constant(float.class, (float)0), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(Float.MAX_VALUE, (float)MethodHandles.dropArguments(MethodHandles.constant(float.class, Float.MAX_VALUE), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(Float.MIN_VALUE, (float)MethodHandles.dropArguments(MethodHandles.constant(float.class, Float.MIN_VALUE), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(0l, (long)MethodHandles.dropArguments(MethodHandles.constant(long.class, (long)0), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(Long.MAX_VALUE, (long)MethodHandles.dropArguments(MethodHandles.constant(long.class, Long.MAX_VALUE), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(Long.MIN_VALUE, (long)MethodHandles.dropArguments(MethodHandles.constant(long.class, Long.MIN_VALUE), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(0d, (double)MethodHandles.dropArguments(MethodHandles.constant(double.class, (double)0), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(Double.MAX_VALUE, (double)MethodHandles.dropArguments(MethodHandles.constant(double.class, Double.MAX_VALUE), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(Double.MIN_VALUE, (double)MethodHandles.dropArguments(MethodHandles.constant(double.class, Double.MIN_VALUE), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(0, (int)MethodHandles.dropArguments(MethodHandles.constant(int.class, 0), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(Integer.MAX_VALUE, (int)MethodHandles.dropArguments(MethodHandles.constant(int.class, Integer.MAX_VALUE), 0, int.class, String.class).invokeExact(1, "a"));
		AssertJUnit.assertEquals(Integer.MIN_VALUE, (int)MethodHandles.dropArguments(MethodHandles.constant(int.class, Integer.MIN_VALUE), 0, int.class, String.class).invokeExact(1, "a"));
	}
	
	/* Test the special behaviour of constantHandles to be able to "throw away" the results of 
	 * insert and drop operations.
	 */
	@Test(groups = { "level.extended" })
	public void testConstantWithDropAndInsert() throws Throwable {
		AssertJUnit.assertEquals(5, (int)wrapWithDropAndInsert(MethodHandles.constant(int.class, 5)).invokeExact());
		AssertJUnit.assertEquals('c', (char)wrapWithDropAndInsert(MethodHandles.constant(char.class, 'c')).invokeExact());
		AssertJUnit.assertEquals("String", (String)wrapWithDropAndInsert(MethodHandles.constant(String.class, "String")).invokeExact());
		AssertJUnit.assertEquals(true, (boolean)wrapWithDropAndInsert(MethodHandles.constant(boolean.class, true)).invokeExact());
		AssertJUnit.assertEquals(false, (boolean)wrapWithDropAndInsert(MethodHandles.constant(boolean.class, false)).invokeExact());
		AssertJUnit.assertEquals(0, (byte)wrapWithDropAndInsert(MethodHandles.constant(byte.class, (byte)0)).invokeExact());
		AssertJUnit.assertEquals(Byte.MIN_VALUE, (byte)wrapWithDropAndInsert(MethodHandles.constant(byte.class, Byte.MIN_VALUE)).invokeExact());
		AssertJUnit.assertEquals(Byte.MAX_VALUE, (byte)wrapWithDropAndInsert(MethodHandles.constant(byte.class, Byte.MAX_VALUE)).invokeExact());
		AssertJUnit.assertEquals(0, (short)wrapWithDropAndInsert(MethodHandles.constant(short.class, (short)0)).invokeExact());
		AssertJUnit.assertEquals(Short.MIN_VALUE, (short)wrapWithDropAndInsert(MethodHandles.constant(short.class, Short.MIN_VALUE)).invokeExact());
		AssertJUnit.assertEquals(Short.MAX_VALUE, (short)wrapWithDropAndInsert(MethodHandles.constant(short.class, Short.MAX_VALUE)).invokeExact());
		AssertJUnit.assertEquals(0, (char)wrapWithDropAndInsert(MethodHandles.constant(char.class, (char)0)).invokeExact());
		AssertJUnit.assertEquals(Character.MIN_VALUE, (char)wrapWithDropAndInsert(MethodHandles.constant(char.class, Character.MIN_VALUE)).invokeExact());
		AssertJUnit.assertEquals(Character.MAX_VALUE, (char)wrapWithDropAndInsert(MethodHandles.constant(char.class, Character.MAX_VALUE)).invokeExact());
		AssertJUnit.assertEquals((float)0, (float)wrapWithDropAndInsert(MethodHandles.constant(float.class, (float)0)).invokeExact());
		AssertJUnit.assertEquals(Float.MAX_VALUE, (float)wrapWithDropAndInsert(MethodHandles.constant(float.class, Float.MAX_VALUE)).invokeExact());
		AssertJUnit.assertEquals(Float.MIN_VALUE, (float)wrapWithDropAndInsert(MethodHandles.constant(float.class, Float.MIN_VALUE)).invokeExact());
		AssertJUnit.assertEquals(0l, (long)wrapWithDropAndInsert(MethodHandles.constant(long.class, (long)0)).invokeExact());
		AssertJUnit.assertEquals(Long.MAX_VALUE, (long)wrapWithDropAndInsert(MethodHandles.constant(long.class, Long.MAX_VALUE)).invokeExact());
		AssertJUnit.assertEquals(Long.MIN_VALUE, (long)wrapWithDropAndInsert(MethodHandles.constant(long.class, Long.MIN_VALUE)).invokeExact());
		AssertJUnit.assertEquals(0d, (double)wrapWithDropAndInsert(MethodHandles.constant(double.class, (double)0)).invokeExact());
		AssertJUnit.assertEquals(Double.MAX_VALUE, (double)wrapWithDropAndInsert(MethodHandles.constant(double.class, Double.MAX_VALUE)).invokeExact());
		AssertJUnit.assertEquals(Double.MIN_VALUE, (double)wrapWithDropAndInsert(MethodHandles.constant(double.class, Double.MIN_VALUE)).invokeExact());
		AssertJUnit.assertEquals(0, (int)wrapWithDropAndInsert(MethodHandles.constant(int.class, 0)).invokeExact());
		AssertJUnit.assertEquals(Integer.MAX_VALUE, (int)wrapWithDropAndInsert(MethodHandles.constant(int.class, Integer.MAX_VALUE)).invokeExact());
		AssertJUnit.assertEquals(Integer.MIN_VALUE, (int)wrapWithDropAndInsert(MethodHandles.constant(int.class, Integer.MIN_VALUE)).invokeExact());
	}
	
	private MethodHandle wrapWithDropAndInsert(MethodHandle mh) {
		return insertArguments(dropArguments(mh, 0, int.class, String.class), 0, 1, "a");
	}

	
	@Test(groups = { "level.extended" })
	public void testArrayElementGetter() throws WrongMethodTypeException, Throwable {
		MethodHandle getter = MethodHandles.arrayElementGetter(String[].class);
		AssertJUnit.assertEquals(methodType(String.class, String[].class, int.class), getter.type());
		String[] strings = new String[] { "A", "B", "C"};
		String s = (String)getter.invokeExact(strings, 2);
		AssertJUnit.assertEquals(strings[2], s);
	}

	
	@Test(groups = { "level.extended" })
	public void testArrayElementSetter() throws WrongMethodTypeException, Throwable {
		MethodHandle setter = MethodHandles.arrayElementSetter(int[].class);
		AssertJUnit.assertEquals(methodType(void.class, int[].class, int.class, int.class), setter.type());
		int[] ints = new int[3];
		setter.invokeExact(ints, 1, 5);
		AssertJUnit.assertEquals(ints[0],0);
		AssertJUnit.assertEquals(ints[1],5);
		AssertJUnit.assertEquals(ints[2],0);
		AssertJUnit.assertEquals(ints.length, 3);
	}
	
	
	@Test(groups = { "level.extended" })
	public void testThrowException() throws WrongMethodTypeException, Throwable {
		boolean flag_V = false;
		MethodHandle thrower_V = MethodHandles.throwException(void.class, FooException.class);
		try { 
			thrower_V.invokeExact(new FooException());
		} catch(FooException e) {
			flag_V = true;
		}
		AssertJUnit.assertTrue(flag_V);
		
		boolean flag_Object = false;
		MethodHandle thrower_Object = MethodHandles.throwException(Object.class, FooException.class);
		try { 
			Object o = (Object)thrower_Object.invokeExact(new FooException());
		} catch(FooException e) {
			flag_Object = true;
		}
		AssertJUnit.assertTrue(flag_Object);
		
		boolean flag_AdaptorTests = false;
		MethodHandle thrower_AdaptorTests = MethodHandles.throwException(AdaptorTests.class, FooException.class);
		try { 
			Object o = (AdaptorTests)thrower_AdaptorTests.invokeExact(new FooException());
		} catch(FooException e) {
			flag_AdaptorTests = true;
		}
		AssertJUnit.assertTrue(flag_AdaptorTests);
	}

	
	@Test(groups = { "level.extended" })
	public void testFilterReturnValue() throws WrongMethodTypeException, Throwable {
		MethodHandle I = MethodHandles.constant(int.class, 5);
		
		MethodHandle identity_filter = MethodHandles.identity(int.class);
		MethodHandle filtered = MethodHandles.filterReturnValue(I, identity_filter);
		AssertJUnit.assertEquals(methodType(int.class), filtered.type());
		AssertJUnit.assertEquals(5, (int)filtered.invokeExact());
	}

	
	@Test(groups = { "level.extended" })
	public void testFilterArguments() throws WrongMethodTypeException, Throwable {
		MethodHandle cat = MethodHandles.lookup().findVirtual(String.class, "concat", methodType(String.class, String.class));
		MethodHandle upcase = MethodHandles.lookup().findVirtual(String.class, "toUpperCase", methodType(String.class));
		AssertJUnit.assertEquals("xy", (String) cat.invokeExact("x", "y"));
		MethodHandle f0 = MethodHandles.filterArguments(cat, 0, upcase);
		AssertJUnit.assertEquals("Xy", (String) f0.invokeExact("x", "y")); // Xy
		MethodHandle f1 = MethodHandles.filterArguments(cat, 1, upcase);
		AssertJUnit.assertEquals("xY", (String) f1.invokeExact("x", "y")); // xY
		MethodHandle f2 = MethodHandles.filterArguments(cat, 0, upcase, upcase);
		AssertJUnit.assertEquals("XY", (String) f2.invokeExact("x", "y")); // XY
	}

	
	@Test(groups = { "level.extended" })
	public void testFoldArguments() throws WrongMethodTypeException, Throwable {
		MethodType II_I_type = methodType(int.class, int.class, int.class);
		MethodType I_I_type = methodType(int.class, int.class);
		
		MethodHandle sub = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "subtract", II_I_type);
		MethodHandle fold = MethodHandles.foldArguments(sub, MethodHandles.identity(int.class));
		AssertJUnit.assertEquals(I_I_type.toMethodDescriptorString(), I_I_type, fold.type());
		AssertJUnit.assertEquals(0, (int)fold.invokeExact(5));
		
		MethodHandle asList = MethodHandles.publicLookup().findStatic(Arrays.class, "asList", methodType(List.class, Object[].class));
		MethodHandle asList_4int = asList.asType(methodType(List.class, int.class, int.class, int.class, int.class));
		MethodHandle combiner = MethodHandles.dropArguments(
				MethodHandles.identity(int.class), 
				1, 
				int.class, 
				int.class);
		MethodHandle fold_list = MethodHandles.foldArguments(asList_4int, combiner);
		AssertJUnit.assertEquals(Arrays.asList(1, 1, 2, 3), (List<?>)fold_list.invokeExact(1, 2, 3));	
	}
	
	@Test(groups = { "level.extended" })
	public void testVoidFoldArguments() throws WrongMethodTypeException, Throwable {
		MethodType II_I_type = methodType(int.class, int.class, int.class);
		MethodType I_V_type = methodType(void.class, int.class);
		MethodType IIII_V_type = methodType(void.class, int.class, int.class, int.class, int.class);
		
		MethodHandle sub = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "subtract", II_I_type);
		MethodHandle voidCombiner = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "doNothing_I", I_V_type);
		MethodHandle fold = MethodHandles.foldArguments(sub, voidCombiner);
		AssertJUnit.assertEquals(II_I_type.toMethodDescriptorString(), II_I_type, fold.type());
		AssertJUnit.assertEquals(0, (int)fold.invokeExact(5, 5));
		
		MethodHandle asList = MethodHandles.publicLookup().findStatic(Arrays.class, "asList", methodType(List.class, Object[].class));
		MethodHandle asList_4int = asList.asType(methodType(List.class, int.class, int.class, int.class, int.class));
		MethodHandle combiner = MethodHandles.publicLookup().findStatic(AdaptorTests.class, "doNothing_IIII", IIII_V_type);
		MethodHandle fold_list = MethodHandles.foldArguments(asList_4int, combiner);
		AssertJUnit.assertEquals(Arrays.asList(1, 2, 3, 4), (List<?>)fold_list.invokeExact(1, 2, 3, 4));	
	}

	
	@Test(groups = { "level.extended" })
	public void testDropArgumentsMethodHandleIntClassOfQArray() throws WrongMethodTypeException, Throwable {
		MethodHandle repFirst = MethodHandles.lookup().findVirtual(String.class, "replaceFirst", methodType(String.class, String.class, String.class));
		AssertJUnit.assertEquals("yx", (String) repFirst.invokeExact("xx", "x", "y"));
		MethodHandle mh0 = MethodHandles.dropArguments(repFirst, 0, String.class);
		AssertJUnit.assertEquals("gala", (String) mh0.invokeExact("late", "lala", "l", "g"));
		
		MethodHandle mh1 = MethodHandles.dropArguments(repFirst, 1, String.class);
		AssertJUnit.assertEquals("gate", (String) mh1.invokeExact("late", "lala", "l", "g"));
		
		MethodHandle mh2 = MethodHandles.dropArguments(repFirst, 1, int.class);
		AssertJUnit.assertEquals("gate", (String) mh2.invokeExact("late", 1, "l", "g"));
		
		MethodHandle mh3 = MethodHandles.dropArguments(repFirst, 1, int.class, boolean.class);
		AssertJUnit.assertEquals("gate", (String) mh3.invokeExact("late", 1, false, "l", "g"));
	}

	
	@Test(groups = { "level.extended" })
	public void testDropArgumentsMethodHandleIntListOfClassOfQ() throws WrongMethodTypeException, Throwable {
		MethodHandle repFirst = MethodHandles.lookup().findVirtual(String.class, "replaceFirst", methodType(String.class, String.class, String.class));
		AssertJUnit.assertEquals("yx", (String) repFirst.invokeExact("xx", "x", "y"));
		MethodHandle mh0 = MethodHandles.dropArguments(repFirst, 0, Arrays.asList(new Class<?>[] {String.class}));
		AssertJUnit.assertEquals("gala", (String) mh0.invokeExact("late", "lala", "l", "g"));
		
		MethodHandle mh1 = MethodHandles.dropArguments(repFirst, 1, Arrays.asList(new Class<?>[] {String.class}));
		AssertJUnit.assertEquals("gate", (String) mh1.invokeExact("late", "lala", "l", "g"));
		
		MethodHandle mh2 = MethodHandles.dropArguments(repFirst, 1, Arrays.asList(new Class<?>[] { int.class}));
		AssertJUnit.assertEquals("gate", (String) mh2.invokeExact("late", 1, "l", "g"));
		
		MethodHandle mh3 = MethodHandles.dropArguments(repFirst, 1, Arrays.asList(new Class<?>[] {int.class, boolean.class}));
		AssertJUnit.assertEquals("gate", (String) mh3.invokeExact("late", 1, false, "l", "g"));
	}

	
	@Test(groups = { "level.extended" })
	public void testInsertArguments() throws WrongMethodTypeException, Throwable {
		MethodHandle cat = MethodHandles.lookup().findVirtual(String.class, "concat", MethodType.methodType(String.class, String.class));
		AssertJUnit.assertEquals("xy", (String) cat.invokeExact("x", "y"));
		Object[] set = {"a"};
		Object[] set2 = {"a", "b"};
		
		MethodHandle d0 = MethodHandles.insertArguments(cat, 0, set);
		AssertJUnit.assertEquals("ax", (String) d0.invokeExact("x"));
		MethodHandle d1 = MethodHandles.insertArguments(cat, 1, set);
		AssertJUnit.assertEquals("xa", (String) d1.invokeExact("x"));
		MethodHandle d2 = MethodHandles.insertArguments(cat, 0, set2);
		AssertJUnit.assertEquals("ab", (String) d2.invokeExact());
	}
	
	static int insertTestMethod_add(String s1, int a, String s2, int b) {
		AssertJUnit.assertTrue(s1 instanceof String);
		AssertJUnit.assertTrue(s2 instanceof String);
		return a + b;
	}
	
	static MethodHandle getAddMethodForInsertionTests() throws Throwable {
		return lookup().findStatic(AdaptorTests.class, "insertTestMethod_add", methodType(int.class, String.class, int.class, String.class, int.class));
	}

	@Test(groups = { "level.extended" })
	public void testInsertArguments_one() throws Throwable {
		MethodHandle handle = getAddMethodForInsertionTests();

		MethodHandle permute = permuteArguments(handle, methodType(int.class, int.class, String.class, String.class, int.class), 1, 0, 2, 3);
		MethodHandle insert = insertArguments(permute, 0, 1);
		AssertJUnit.assertEquals(2, (int)insert.invokeExact("a", "b", 1));
	}
	
	@Test(groups = { "level.extended" })
	public void testInsertArguments_two() throws Throwable {
		boolean cceOccurred = false;
		try {
			MethodHandle handle = getAddMethodForInsertionTests();
			insertArguments(handle, 0, 1);
			Assert.fail("insertArguments should have thrown a CCE");
		} catch(ClassCastException e) {
			cceOccurred = true;
		}
		AssertJUnit.assertTrue(cceOccurred);
	}
	
	@Test(groups = { "level.extended" })
	public void testInsertArguments_three() throws Throwable {
		MethodHandle handle = getAddMethodForInsertionTests();
		Byte b = (byte)1;
		MethodHandle insert = insertArguments(handle, 1, b);
		AssertJUnit.assertEquals(2, (int)insert.invokeExact("a", "b", 1));
	}

	@Test(groups = { "level.extended" })
	public void testAsType_four() throws Throwable {
		MethodHandle handle = getAddMethodForInsertionTests();

		Byte b = (byte)1;
		MethodHandle asType = handle.asType(methodType(int.class, String.class, Byte.class, String.class, int.class));
		AssertJUnit.assertEquals(2, (int)asType.invokeExact("a", b, "b", 1));
	}

	@Test(groups = { "level.extended" })
	public void testInsertArguments_five() throws Throwable {
		boolean cceOccurred = false;
		try {
			MethodHandle handle = getAddMethodForInsertionTests();
			insertArguments(handle, 1, (Long)1l);
			Assert.fail("insertArguments should have thrown a CCE");
		} catch(ClassCastException e) {
			cceOccurred = true;
		}
		AssertJUnit.assertTrue(cceOccurred);
	}

	@Test(groups = { "level.extended" })
	public void testInsertArguments_six() throws Throwable {
		boolean cceOccurred = false;
		try {
			MethodHandle handle = getAddMethodForInsertionTests();
			insertArguments(handle, 0, new Object());
			Assert.fail("insertArguments should have thrown a CCE");
		} catch(ClassCastException e) {
			cceOccurred = true;
		}
		AssertJUnit.assertTrue(cceOccurred);
	}
	
	@Test(groups = { "level.extended" })
	public void testBindToAndInsertArgumentsForNull() throws Throwable {
		MethodHandle intIdentity = identity(int.class);
		try {
			intIdentity.bindTo(null);
			Assert.fail("Should have thrown IllegalArgumentException");
		}catch(IllegalArgumentException e) {
		}
		try {
			insertArguments(intIdentity, 0, new Object[] { null });
			Assert.fail("Should have thrown NullPointerException");
		}catch(NullPointerException e) {
		}
		
		MethodHandle stringIdentity = identity(String.class);
		MethodType type = methodType(String.class);
		AssertJUnit.assertEquals(type, stringIdentity.bindTo(null).type());
		AssertJUnit.assertEquals(type, insertArguments(stringIdentity, 0, new Object[] { null }).type());
	}
	
	public static String[] arrayArgsHelper(String a, String b, String c) {
		return new String[] {a, b, c};
	}
	
	/*
	 * PR54020: Ensure that the permute[] used by PermuteHandle can't be modified after creation.
	 */
	@Test(groups = { "level.extended" })
	public void testPermuteSafety() throws Throwable {
		MethodType type = methodType(String[].class, String.class, String.class, String.class);
		MethodHandle handle = lookup().findStatic(AdaptorTests.class, "arrayArgsHelper", type); 

		int[] reorder = new int[] {2, 1, 0};

		MethodHandle permute = permuteArguments(handle, type, reorder);
		String[] firstResult = (String[])permute.invokeExact("*A*", "*B*", "*C*");
		reorder[0] = 1;
		reorder[1] = 2;
		reorder[2] = 0;
		String[] secondResult = (String[])permute.invokeExact("*A*", "*B*", "*C*");

		AssertJUnit.assertTrue("Change to permute array shouldn't have had any effect", Arrays.equals(firstResult, secondResult));
	}
	
	public static String throwIAEHelper(String a) {
		throw new IllegalArgumentException("Filter deliberately throws IAE");
	}
	
	/*
	 * PR54020: Ensure that the filter[] used by FilterArguments can't be modified after handle creation.
	 */
	@Test(groups = { "level.extended" })
	public void testFilterArgumentsSafety() throws Throwable {
		MethodType type = methodType(String[].class, String.class, String.class, String.class);
		MethodHandle handle = lookup().findStatic(AdaptorTests.class, "arrayArgsHelper", type);
		MethodHandle[] filters = new MethodHandle[]{ identity(String.class) };
		MethodHandle filter = filterArguments(handle, 0, filters);

		try {
			String[] firstResult = (String[])filter.invokeExact("*A*", "*B*", "*C*");
			filters[0] = lookup().findStatic(AdaptorTests.class, "throwIAEHelper", methodType(String.class, String.class));
			String[] secondResult = (String[])filter.invokeExact("*A*", "*B*", "*C*");
			AssertJUnit.assertTrue(Arrays.equals(firstResult, secondResult));
		} catch (IllegalArgumentException e) {
			Assert.fail("Change to filter[] should not have been visible");
		}
	}
	
	public static String[] arrayArgsHelper(String a, String b, String c, String d) {
		return new String[] {a, b, c, d};
	}
	
	/*
	 * PR54020: Ensure that the values[] used by InsertArguments can't be modified after handle creation.
	 */
	@Test(groups = { "level.extended" })
	public void testInsertArgumentsSafety() throws Throwable {
		final MethodType type = methodType(String[].class, String.class, String.class, String.class, String.class);
		final MethodHandle handle = lookup().findStatic(AdaptorTests.class, "arrayArgsHelper", type);
		
		{ // "bare" InsertHandle
			String[] args = new String[]{"*A*", "*B*", "*C*", "*D*", };
			MethodHandle insertHandle = insertArguments(handle, 0, (Object[])args);
			String[] firstResult = (String[])insertHandle.invokeExact();
			args[0] = "*F*";
			String[] secondResult = (String[])insertHandle.invokeExact();
			AssertJUnit.assertTrue("Change to inserted values array shouldn't be visible", Arrays.equals(firstResult, secondResult));
		}
		{ // Insert3Handle
			String[] args = new String[]{"*A*", "*B*", "*C*" };
			MethodHandle insertHandle = insertArguments(handle, 0, (Object[])args);
			String[] firstResult = (String[])insertHandle.invokeExact("*D*");
			args[0] = "*F*";
			String[] secondResult = (String[])insertHandle.invokeExact("*D*");
			AssertJUnit.assertTrue("Change to inserted values array shouldn't be visible", Arrays.equals(firstResult, secondResult));
		}
		{ // Insert2Handle
			String[] args = new String[]{"*A*", "*B*"};
			MethodHandle insertHandle = insertArguments(handle, 0, (Object[])args);
			String[] firstResult = (String[])insertHandle.invokeExact("*C*" ,"*D*");
			args[0] = "*F*";
			String[] secondResult = (String[])insertHandle.invokeExact("*C*" ,"*D*");
			AssertJUnit.assertTrue("Change to inserted values array shouldn't be visible", Arrays.equals(firstResult, secondResult));
		}
		{ // Insert1Handle
			String[] args = new String[]{"*A*"};
			MethodHandle insertHandle = insertArguments(handle, 0, (Object[])args);
			String[] firstResult = (String[])insertHandle.invokeExact("*B*", "*C*" ,"*D*");
			args[0] = "*F*";
			String[] secondResult = (String[])insertHandle.invokeExact("*B*", "*C*" ,"*D*");
			AssertJUnit.assertTrue("Change to inserted values array shouldn't be visible", Arrays.equals(firstResult, secondResult));
		}
	}
}
