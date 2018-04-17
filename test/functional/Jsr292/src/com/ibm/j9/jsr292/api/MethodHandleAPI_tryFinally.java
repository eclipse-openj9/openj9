/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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
package com.ibm.j9.jsr292.api;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles.Lookup;

import static java.lang.invoke.MethodHandles.lookup;
import static java.lang.invoke.MethodHandles.tryFinally;
import static java.lang.invoke.MethodType.methodType;

public class MethodHandleAPI_tryFinally {
	final Lookup lookup = lookup();

	/**
	 * tryFinally test for the try handle with int parameters and a void return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryIntMethod_VoidReturnType() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryIntMethod_VoidReturnType", methodType(void.class, int.class, int.class, int.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyIntMethod", methodType(void.class, Throwable.class, int.class, int.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(void.class, int.class, int.class, int.class), mhResult.type());
		try {
			mhResult.invokeExact(1, 2, 3);
		} catch(Throwable t) {
			/* Intentionally throw out an exception with message from the finally handle so
			 * as to check whether the passed-in arguments are correct for the finally handle.
			 */
			AssertJUnit.assertTrue(t.getMessage().equals("[NO Throwable, 1, 2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with int parameters and a void return type.
	 * Note: it intentionally throws out an exception to be addressed in the finally handle.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryIntMethod_VoidReturnType_Throwable() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryIntMethod_VoidReturnType_Throwable", methodType(void.class, int.class, int.class, int.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyIntMethod", methodType(void.class, Throwable.class, int.class, int.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(void.class, int.class, int.class, int.class), mhResult.type());
		try {
			mhResult.invokeExact(1, 2, 3);
		} catch(Throwable t) {
			/* The exception thrown out from the try handle should be addressed in the finally handle */
			AssertJUnit.assertTrue(t.getMessage().equals("[Throwable, 1, 2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with int parameters and an int return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryIntMethod_IntReturnType() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryIntMethod_IntReturnType", methodType(int.class, int.class, int.class, int.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyIntMethod", methodType(int.class, Throwable.class, int.class, int.class, int.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(int.class, int.class, int.class, int.class), mhResult.type());
		AssertJUnit.assertEquals(9, (int)mhResult.invokeExact(1, 2, 3));
	}

 	/**
 	 * tryFinally test for the try handle with int parameters and an int return type.
 	 * Note: it intentionally throws out an exception to be addressed in the finally handle.
 	 * @throws Throwable
 	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryIntMethod_IntReturnType_Throwable() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryIntMethod_IntReturnType_Throwable", methodType(int.class, int.class, int.class, int.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyIntMethod", methodType(int.class, Throwable.class, int.class, int.class, int.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(int.class, int.class, int.class, int.class), mhResult.type());
		try {
			int result = (int)mhResult.invokeExact(1, 2, 3);
		} catch(Throwable t) {
			/* The exception thrown out from the try handle should be addressed in the finally handle */
			AssertJUnit.assertTrue(t.getMessage().equals("[Throwable, 0, 1, 2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with String parameters and a void return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryStringMethod_VoidReturnType() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryStringMethod_VoidReturnType", methodType(void.class, String.class, String.class, String.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyStringMethod", methodType(void.class, Throwable.class, String.class, String.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(void.class, String.class, String.class, String.class), mhResult.type());
		try {
			mhResult.invokeExact("arg1", "arg2", "arg3");
		} catch(Throwable t) {
			/* Intentionally throw out an exception with message from the finally handle so
			 * as to check whether the passed-in arguments are correct for the finally handle.
			 */
			AssertJUnit.assertTrue(t.getMessage().equals("[NO Throwable, arg1, arg2]"));
		}
	}

 	/**
 	 * tryFinally test for the try handle with String parameters and a void return type.
 	 * Note: it intentionally throws out an exception to be addressed in the finally handle.
 	 * @throws Throwable
 	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryStringMethod_VoidReturnType_Throwable() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryStringMethod_VoidReturnType_Throwable", methodType(void.class, String.class, String.class, String.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyStringMethod", methodType(void.class, Throwable.class, String.class, String.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(void.class, String.class, String.class, String.class), mhResult.type());
		try {
			mhResult.invokeExact("arg1", "arg2", "arg3");
		} catch(Throwable t) {
			/* The exception thrown out from the try handle should be addressed in the finally handle */
			AssertJUnit.assertTrue(t.getMessage().equals("[Throwable, arg1, arg2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with String parameters and a String return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryStringMethod_StringReturnType() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryStringMethod_StringReturnType", methodType(String.class, String.class, String.class, String.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyStringMethod", methodType(String.class, Throwable.class, String.class, String.class, String.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(String.class, String.class, String.class, String.class), mhResult.type());
		AssertJUnit.assertEquals("[[arg1-arg2-arg3], arg1, arg2]", (String)mhResult.invokeExact("arg1", "arg2", "arg3"));
	}

 	/**
 	 * tryFinally test for the try handle with String parameters and a String return type.
 	 * Note: it intentionally throws out an exception to be addressed in the finally handle.
 	 * @throws Throwable
 	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryStringMethod_StringReturnType_Throwable() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryStringMethod_StringReturnType_Throwable", methodType(String.class, String.class, String.class, String.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyStringMethod", methodType(String.class, Throwable.class, String.class, String.class, String.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(String.class, String.class, String.class, String.class), mhResult.type());
		try {
			String result = (String)mhResult.invokeExact("arg1", "arg2", "arg3");
		} catch(Throwable t) {
			/* The exception thrown out from the try handle should be addressed in the finally handle */
			AssertJUnit.assertTrue(t.getMessage().equals("[Throwable, null, arg1, arg2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with boolean parameters and a void return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryBooleanMethod_VoidReturnType() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryBooleanMethod_VoidReturnType", methodType(void.class, boolean.class, boolean.class, boolean.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyBooleanMethod", methodType(void.class, Throwable.class, boolean.class, boolean.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(void.class, boolean.class, boolean.class, boolean.class), mhResult.type());
		try {
			mhResult.invokeExact(true, false, true);
		} catch(Throwable t) {
			/* Intentionally throw out an exception with message from the finally handle so
			 * as to check whether the passed-in arguments are correct for the finally handle.
			 */
			AssertJUnit.assertTrue(t.getMessage().equals("[NO Throwable, true, false]"));
		}
	}

 	/**
 	 * tryFinally test for the try handle with boolean parameters and a void return type.
 	 * Note: it intentionally throws out an exception to be addressed in the finally handle.
 	 * @throws Throwable
 	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryBooleanMethod_VoidReturnType_Throwable() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryBooleanMethod_VoidReturnType_Throwable", methodType(void.class, boolean.class, boolean.class, boolean.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyBooleanMethod", methodType(void.class, Throwable.class, boolean.class, boolean.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(void.class, boolean.class, boolean.class, boolean.class), mhResult.type());
		try {
			mhResult.invokeExact(true, false, true);
		} catch(Throwable t) {
			/* The exception thrown out from the try handle should be addressed in the finally handle */
			AssertJUnit.assertTrue(t.getMessage().equals("[Throwable, true, false]"));
		}
	}

	/**
	 * tryFinally test for the try handle with boolean parameters and a boolean return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryBooleanMethod_BooleanReturnType() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryBooleanMethod_BooleanReturnType", methodType(boolean.class, boolean.class, boolean.class, boolean.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyBooleanMethod", methodType(boolean.class, Throwable.class, boolean.class, boolean.class, boolean.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(boolean.class, boolean.class, boolean.class, boolean.class), mhResult.type());
		AssertJUnit.assertEquals(true, (boolean)mhResult.invokeExact(true, false, true));
	}

 	/**
 	 * tryFinally test for the try handle with boolean parameters and a boolean return type.
 	 * Note: it intentionally throws out an exception to be addressed in the finally handle.
 	 * @throws Throwable
 	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryBooleanMethod_BooleanReturnType_Throwable() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryBooleanMethod_BooleanReturnType_Throwable", methodType(boolean.class, boolean.class, boolean.class, boolean.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyBooleanMethod", methodType(boolean.class, Throwable.class, boolean.class, boolean.class, boolean.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(boolean.class, boolean.class, boolean.class, boolean.class), mhResult.type());
		try {
			boolean result = (boolean)mhResult.invokeExact(true, false, true);
		} catch(Throwable t) {
			/* The exception thrown out from the try handle should be addressed in the finally handle */
			AssertJUnit.assertTrue(t.getMessage().equals("[Throwable, false, true, false]"));
		}
	}

	/**
	 * tryFinally test for the try handle with char parameters and a void return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryCharMethod_VoidReturnType() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryCharMethod_VoidReturnType", methodType(void.class, char.class, char.class, char.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyCharMethod", methodType(void.class, Throwable.class, char.class, char.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(void.class, char.class, char.class, char.class), mhResult.type());
		try {
			mhResult.invokeExact('1', '2', '3');
		} catch(Throwable t) {
			/* Intentionally throw out an exception with message from the finally handle so
			 * as to check whether the passed-in arguments are correct for the finally handle.
			 */
			AssertJUnit.assertTrue(t.getMessage().equals("[NO Throwable, 1, 2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with char parameters and a void return type.
	 * Note: it intentionally throws out an exception to be addressed in the finally handle.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryCharMethod_VoidReturnType_Throwable() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryCharMethod_VoidReturnType_Throwable", methodType(void.class, char.class, char.class, char.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyCharMethod", methodType(void.class, Throwable.class, char.class, char.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(void.class, char.class, char.class, char.class), mhResult.type());
		try {
			mhResult.invokeExact('1', '2', '3');
		} catch(Throwable t) {
			/* The exception thrown out from the try handle should be addressed in the finally handle */
			AssertJUnit.assertTrue(t.getMessage().equals("[Throwable, 1, 2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with char parameters and a char return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryCharMethod_CharReturnType() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryCharMethod_CharReturnType", methodType(char.class, char.class, char.class, char.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyCharMethod", methodType(char.class, Throwable.class, char.class, char.class, char.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(char.class, char.class, char.class, char.class), mhResult.type());
		AssertJUnit.assertEquals('3', (char)mhResult.invokeExact('1', '2', '3'));
	}

 	/**
 	 * tryFinally test for the try handle with char parameters and a char return type.
 	 * Note: it intentionally throws out an exception to be addressed in the finally handle.
 	 * @throws Throwable
 	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryCharMethod_CharReturnType_Throwable() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryCharMethod_CharReturnType_Throwable", methodType(char.class, char.class, char.class, char.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyCharMethod", methodType(char.class, Throwable.class, char.class, char.class, char.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(char.class, char.class, char.class, char.class), mhResult.type());
		try {
			char result = (char)mhResult.invokeExact('1', '2', '3');
		} catch(Throwable t) {
			/* The exception thrown out from the try handle should be addressed in the finally handle */
			AssertJUnit.assertTrue(t.getMessage().equals("[Throwable, 0, 1, 2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with byte parameters and a void return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryByteMethod_VoidReturnType() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryByteMethod_VoidReturnType", methodType(void.class, byte.class, byte.class, byte.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyByteMethod", methodType(void.class, Throwable.class, byte.class, byte.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(void.class, byte.class, byte.class, byte.class), mhResult.type());
		try {
			mhResult.invokeExact((byte)1, (byte)2, (byte)3);
		} catch(Throwable t) {
			/* Intentionally throw out an exception with message from the finally handle so
			 * as to check whether the passed-in arguments are correct for the finally handle.
			 */
			AssertJUnit.assertTrue(t.getMessage().equals("[NO Throwable, 1, 2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with byte parameters and a void return type.
	 * Note: it intentionally throws out an exception to be addressed in the finally handle.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryByteMethod_VoidReturnType_Throwable() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryByteMethod_VoidReturnType_Throwable", methodType(void.class, byte.class, byte.class, byte.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyByteMethod", methodType(void.class, Throwable.class, byte.class, byte.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(void.class, byte.class, byte.class, byte.class), mhResult.type());
		try {
			mhResult.invokeExact((byte)1, (byte)2, (byte)3);
		} catch(Throwable t) {
			/* The exception thrown out from the try handle should be addressed in the finally handle */
			AssertJUnit.assertTrue(t.getMessage().equals("[Throwable, 1, 2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with byte parameters and a byte return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryByteMethod_ByteReturnType() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryByteMethod_ByteReturnType", methodType(byte.class, byte.class, byte.class, byte.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyByteMethod", methodType(byte.class, Throwable.class, byte.class, byte.class, byte.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(byte.class, byte.class, byte.class, byte.class), mhResult.type());
		AssertJUnit.assertEquals(0x3, (byte)mhResult.invokeExact((byte)1, (byte)2, (byte)3));
	}

 	/**
 	 * tryFinally test for the try handle with byte parameters and a byte return type.
 	 * Note: it intentionally throws out an exception to be addressed in the finally handle.
 	 * @throws Throwable
 	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryByteMethod_ByteReturnType_Throwable() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryByteMethod_ByteReturnType_Throwable", methodType(byte.class, byte.class, byte.class, byte.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyByteMethod", methodType(byte.class, Throwable.class, byte.class, byte.class, byte.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(byte.class, byte.class, byte.class, byte.class), mhResult.type());
		try {
			byte result = (byte)mhResult.invokeExact((byte)1, (byte)2, (byte)3);
		} catch(Throwable t) {
			/* The exception thrown out from the try handle should be addressed in the finally handle */
			AssertJUnit.assertTrue(t.getMessage().equals("[Throwable, 0, 1, 2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with short parameters and a void return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryShortMethod_VoidReturnType() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryShortMethod_VoidReturnType", methodType(void.class, short.class, short.class, short.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyShortMethod", methodType(void.class, Throwable.class, short.class, short.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(void.class, short.class, short.class, short.class), mhResult.type());
		try {
			mhResult.invokeExact((short)1, (short)2, (short)3);
		} catch(Throwable t) {
			/* Intentionally throw out an exception with message from the finally handle so
			 * as to check whether the passed-in arguments are correct for the finally handle.
			 */
			AssertJUnit.assertTrue(t.getMessage().equals("[NO Throwable, 1, 2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with short parameters and a void return type.
	 * Note: it intentionally throws out an exception to be addressed in the finally handle.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryShortMethod_VoidReturnType_Throwable() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryShortMethod_VoidReturnType_Throwable", methodType(void.class, short.class, short.class, short.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyShortMethod", methodType(void.class, Throwable.class, short.class, short.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(void.class, short.class, short.class, short.class), mhResult.type());
		try {
			mhResult.invokeExact((short)1, (short)2, (short)3);
		} catch(Throwable t) {
			/* The exception thrown out from the try handle should be addressed in the finally handle */
			AssertJUnit.assertTrue(t.getMessage().equals("[Throwable, 1, 2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with short parameters and a short return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryShortMethod_ShortReturnType() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryShortMethod_ShortReturnType", methodType(short.class, short.class, short.class, short.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyShortMethod", methodType(short.class, Throwable.class, short.class, short.class, short.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(short.class, short.class, short.class, short.class), mhResult.type());
		AssertJUnit.assertEquals((short)3, (short)mhResult.invokeExact((short)1, (short)2, (short)3));
	}

 	/**
 	 * tryFinally test for the try handle with short parameters and a short return type.
 	 * Note: it intentionally throws out an exception to be addressed in the finally handle.
 	 * @throws Throwable
 	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryShortMethod_ShortReturnType_Throwable() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryShortMethod_ShortReturnType_Throwable", methodType(short.class, short.class, short.class, short.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyShortMethod", methodType(short.class, Throwable.class, short.class, short.class, short.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(short.class, short.class, short.class, short.class), mhResult.type());
		try {
			short result = (short)mhResult.invokeExact((short)1, (short)2, (short)3);
		} catch(Throwable t) {
			/* The exception thrown out from the try handle should be addressed in the finally handle */
			AssertJUnit.assertTrue(t.getMessage().equals("[Throwable, 0, 1, 2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with long parameters and a void return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryLongMethod_VoidReturnType() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryLongMethod_VoidReturnType", methodType(void.class, long.class, long.class, long.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyLongMethod", methodType(void.class, Throwable.class, long.class, long.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(void.class, long.class, long.class, long.class), mhResult.type());
		try {
			mhResult.invokeExact(1L, 2L, 3L);
		} catch(Throwable t) {
			/* Intentionally throw out an exception with message from the finally handle so
			 * as to check whether the passed-in arguments are correct for the finally handle.
			 */
			AssertJUnit.assertTrue(t.getMessage().equals("[NO Throwable, 1, 2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with long parameters and a void return type.
	 * Note: it intentionally throws out an exception to be addressed in the finally handle.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryLongMethod_VoidReturnType_Throwable() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryLongMethod_VoidReturnType_Throwable", methodType(void.class, long.class, long.class, long.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyLongMethod", methodType(void.class, Throwable.class, long.class, long.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(void.class, long.class, long.class, long.class), mhResult.type());
		try {
			mhResult.invokeExact(1L, 2L, 3L);
		} catch(Throwable t) {
			/* The exception thrown out from the try handle should be addressed in the finally handle */
			AssertJUnit.assertTrue(t.getMessage().equals("[Throwable, 1, 2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with long parameters and a long return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryLongMethod_LongReturnType() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryLongMethod_LongReturnType", methodType(long.class, long.class, long.class, long.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyLongMethod", methodType(long.class, Throwable.class, long.class, long.class, long.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(long.class, long.class, long.class, long.class), mhResult.type());
		AssertJUnit.assertEquals(3L, (long)mhResult.invokeExact(1L, 2L, 3L));
	}

 	/**
 	 * tryFinally test for the try handle with long parameters and a long return type.
 	 * Note: it intentionally throws out an exception to be addressed in the finally handle.
 	 * @throws Throwable
 	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryLongMethod_LongReturnType_Throwable() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryLongMethod_LongReturnType_Throwable", methodType(long.class, long.class, long.class, long.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyLongMethod", methodType(long.class, Throwable.class, long.class, long.class, long.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(long.class, long.class, long.class, long.class), mhResult.type());
		try {
			long result = (long)mhResult.invokeExact(1L, 2L, 3L);
		} catch(Throwable t) {
			/* The exception thrown out from the try handle should be addressed in the finally handle */
			AssertJUnit.assertTrue(t.getMessage().equals("[Throwable, 0, 1, 2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with float parameters and a void return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryFloatMethod_VoidReturnType() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryFloatMethod_VoidReturnType", methodType(void.class, float.class, float.class, float.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyFloatMethod", methodType(void.class, Throwable.class, float.class, float.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(void.class, float.class, float.class, float.class), mhResult.type());
		try {
			mhResult.invokeExact(1.1F, 2.2F, 3.3F);
		} catch(Throwable t) {
			/* Intentionally throw out an exception with message from the finally handle so
			 * as to check whether the passed-in arguments are correct for the finally handle.
			 */
			AssertJUnit.assertTrue(t.getMessage().equals("[NO Throwable, 1.1, 2.2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with float parameters and a void return type.
	 * Note: it intentionally throws out an exception to be addressed in the finally handle.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryFloatMethod_VoidReturnType_Throwable() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryFloatMethod_VoidReturnType_Throwable", methodType(void.class, float.class, float.class, float.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyFloatMethod", methodType(void.class, Throwable.class, float.class, float.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(void.class, float.class, float.class, float.class), mhResult.type());
		try {
			mhResult.invokeExact(1.1F, 2.2F, 3.3F);
		} catch(Throwable t) {
			/* The exception thrown out from the try handle should be addressed in the finally handle */
			AssertJUnit.assertTrue(t.getMessage().equals("[Throwable, 1.1, 2.2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with float parameters and a float return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryFloatMethod_FloatReturnType() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryFloatMethod_FloatReturnType", methodType(float.class, float.class, float.class, float.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyFloatMethod", methodType(float.class, Throwable.class, float.class, float.class, float.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(float.class, float.class, float.class, float.class), mhResult.type());
		AssertJUnit.assertEquals(3.3F, (float)mhResult.invokeExact(1.1F, 2.2F, 3.3F));
	}

 	/**
 	 * tryFinally test for the try handle with float parameters and a float return type.
 	 * Note: it intentionally throws out an exception to be addressed in the finally handle.
 	 * @throws Throwable
 	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryFloatMethod_FloatReturnType_Throwable() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryFloatMethod_FloatReturnType_Throwable", methodType(float.class, float.class, float.class, float.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyFloatMethod", methodType(float.class, Throwable.class, float.class, float.class, float.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(float.class, float.class, float.class, float.class), mhResult.type());
		try {
			float result = (float)mhResult.invokeExact(1.1F, 2.2F, 3.3F);
		} catch(Throwable t) {
			/* The exception thrown out from the try handle should be addressed in the finally handle */
			AssertJUnit.assertTrue(t.getMessage().equals("[Throwable, 0.0, 1.1, 2.2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with double parameters and a void return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryDoubleMethod_VoidReturnType() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryDoubleMethod_VoidReturnType", methodType(void.class, double.class, double.class, double.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyDoubleMethod", methodType(void.class, Throwable.class, double.class, double.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(void.class, double.class, double.class, double.class), mhResult.type());
		try {
			mhResult.invokeExact(1.1D, 2.2D, 3.3D);
		} catch(Throwable t) {
			/* Intentionally throw out an exception with message from the finally handle so
			 * as to check whether the passed-in arguments are correct for the finally handle.
			 */
			AssertJUnit.assertTrue(t.getMessage().equals("[NO Throwable, 1.1, 2.2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with double parameters and a void return type.
	 * Note: it intentionally throws out an exception to be addressed in the finally handle.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryDoubleMethod_VoidReturnType_Throwable() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryDoubleMethod_VoidReturnType_Throwable", methodType(void.class, double.class, double.class, double.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyDoubleMethod", methodType(void.class, Throwable.class, double.class, double.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(void.class, double.class, double.class, double.class), mhResult.type());
		try {
			mhResult.invokeExact(1.1D, 2.2D, 3.3D);
		} catch(Throwable t) {
			/* The exception thrown out from the try handle should be addressed in the finally handle */
			AssertJUnit.assertTrue(t.getMessage().equals("[Throwable, 1.1, 2.2]"));
		}
	}

	/**
	 * tryFinally test for the try handle with double parameters and a double return type.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryDoubleMethod_DoubleReturnType() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryDoubleMethod_DoubleReturnType", methodType(double.class, double.class, double.class, double.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyDoubleMethod", methodType(double.class, Throwable.class, double.class, double.class, double.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(double.class, double.class, double.class, double.class), mhResult.type());
		AssertJUnit.assertEquals(3.3D, (double)mhResult.invokeExact(1.1D, 2.2D, 3.3D));
	}

 	/**
 	 * tryFinally test for the try handle with double parameters and a double return type.
 	 * Note: it intentionally throws out an exception to be addressed in the finally handle.
 	 * @throws Throwable
 	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_tryFinally_TryDoubleMethod_DoubleReturnType_Throwable() throws Throwable {
		MethodHandle mhTry = lookup.findStatic(SamePackageExample.class, "tryDoubleMethod_DoubleReturnType_Throwable", methodType(double.class, double.class, double.class, double.class));
		MethodHandle mhFinally = lookup.findStatic(SamePackageExample.class, "finallyDoubleMethod", methodType(double.class, Throwable.class, double.class, double.class, double.class));

		MethodHandle mhResult = tryFinally(mhTry, mhFinally);
		AssertJUnit.assertEquals(methodType(double.class, double.class, double.class, double.class), mhResult.type());
		try {
			double result = (double)mhResult.invokeExact(1.1D, 2.2D, 3.3D);
		} catch(Throwable t) {
			/* The exception thrown out from the try handle should be addressed in the finally handle */
			AssertJUnit.assertTrue(t.getMessage().equals("[Throwable, 0.0, 1.1, 2.2]"));
		}
	}
}
