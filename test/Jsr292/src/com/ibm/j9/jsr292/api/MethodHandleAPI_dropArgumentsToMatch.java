/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.j9.jsr292.api;

import org.testng.annotations.Test;
import org.testng.Assert;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;

public class MethodHandleAPI_dropArgumentsToMatch {
	public static class Hash_Methods {
		public Hash_Methods() {}

		public static String hash_void() {
			return "abc";
		}

		public String hash_boolean(boolean a, boolean b) {
			return String.valueOf(a && b);
		}

		public String hash_byte(byte a, byte b) {
			return new String(new byte[] {a, b});
		}

		public String hash_char(char a, char b) {
			return "" + a + b;
		}

		public String hash_int(int a, int b) {
			return String.valueOf(a) + String.valueOf(b);
		}

		public String hash_double(double a, double b) {
			return String.valueOf(a) + String.valueOf(b);
		}

		public String hash_array(int[] a, int[] b) {
			String res = "";
			for (int i = 0; i < a.length; i++) {
				res += String.valueOf(a[i]);
			}
			for (int j = 0; j < b.length; j++) {
				res += String.valueOf(b[j]);
			}
			return res;
		}

		public String hash_multi_arguments(String a, String b, String c, String d) {
			return a + b + c + d;
		}
	}


	/**
	 * test for null argument
	 * @throws NullPointerException
	 */
	@Test(groups = { "level.extended" }, expectedExceptions = NullPointerException.class)
	public static void test_dropArgumentsToMatch_null_handle() throws Throwable {
		MethodHandle h0 = MethodHandles.constant(boolean.class, true);

		MethodHandle h1 = MethodHandles.dropArgumentsToMatch(null, 0, h0.type().parameterList(), 0);

		Assert.fail("dropArgumentsToMatch method is not supposed to accept null MethodHandle");
	}

	/**
	 * test for null argument
	 * @throws NullPointerException
	 */
	@Test(groups = { "level.extended" }, expectedExceptions = NullPointerException.class)
	public static void test_dropArgumentsToMatch_null_list() throws Throwable {
		MethodHandle h0 = MethodHandles.constant(boolean.class, true);

		MethodHandle h1 = MethodHandles.dropArgumentsToMatch(h0, 0, null, 0);

		Assert.fail("dropArgumentsToMatch method is not supposed to accept null parameter list");
	}

	/**
	 * test for illegal arguments
	 * @throws IllegalArgumentException
	 */
	@Test(groups = { "level.extended" }, expectedExceptions = IllegalArgumentException.class)
	public static void test_dropArgumentsToMatch_negative_skip() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findVirtual(String.class, "concat", MethodType.methodType(String.class, String.class));
		MethodType typeList = h1.type().insertParameterTypes(1, int.class, char.class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, -2, typeList.parameterList(), 0);

		Assert.fail("dropArgumentsToMatch method is not supposed to accept negative skip value");
	}

	/**
	 * test for illegal arguments
	 * @throws IllegalArgumentException
	 */
	@Test(groups = { "level.extended" }, expectedExceptions = IllegalArgumentException.class)
	public static void test_dropArgumentsToMatch_outrange_skip() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findVirtual(String.class, "concat", MethodType.methodType(String.class, String.class));
		MethodType typeList = h1.type().insertParameterTypes(1, int.class, char.class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, 10, typeList.parameterList(), 0);

		Assert.fail("dropArgumentsToMatch method is not supposed to accept outbound skip value");
	}

	/**
	 * test for illegal arguments
	 * @throws IllegalArgumentException
	 */
	@Test(groups = { "level.extended" }, expectedExceptions = IllegalArgumentException.class)
	public static void test_dropArgumentsToMatch_negative_location() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findVirtual(String.class, "concat", MethodType.methodType(String.class, String.class));
		MethodType typeList = h1.type().insertParameterTypes(1, int.class, char.class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, 0, typeList.parameterList(), -1);

		Assert.fail("dropArgumentsToMatch method is not supposed to accept negative location value");
	}

	/**
	 * test for illegal arguments
	 * @throws IllegalArgumentException
	 */
	@Test(groups = { "level.extended" }, expectedExceptions = IllegalArgumentException.class)
	public static void test_dropArgumentsToMatch_outrange_location() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findVirtual(String.class, "concat", MethodType.methodType(String.class, String.class));
		MethodType typeList = h1.type().insertParameterTypes(1, int.class, char.class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, 0, typeList.parameterList(), 10);

		Assert.fail("dropArgumentsToMatch method is not supposed to accept outbound location value");
	}

	/**
	 * test for illegal arguments
	 * @throws IllegalArgumentException
	 */
	@Test(groups = { "level.extended" }, expectedExceptions = IllegalArgumentException.class)
	public static void test_dropArgumentsToMatch_non_mactching_location() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findVirtual(String.class, "concat", MethodType.methodType(String.class, String.class));
		MethodType typeList = h1.type().insertParameterTypes(1, int.class, char.class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, 0, typeList.parameterList(), 1);

		Assert.fail("dropArgumentsToMatch method must match non-skipped parameters with valueTypes list starting at location");
	}

	/**
	 * test for illegal arguments
	 * @throws IllegalArgumentException
	 */
	@Test(groups = { "level.extended" }, expectedExceptions = IllegalArgumentException.class)
	public static void test_dropArgumentsToMatch_void_class() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findVirtual(String.class, "concat", MethodType.methodType(String.class, String.class));
		MethodType typeList = h1.type().insertParameterTypes(1, int.class, void.class, char.class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, 0, typeList.parameterList(), 1);

		Assert.fail("dropArgumentsToMatch method must receive a valueTypes list that doesn't contain void.class");
	}

	/**
	 * test for illegal arguments where match location has passed the end of valueList
	 * @throws IllegalArgumentException
	 */
	@Test(groups = { "level.extended" }, expectedExceptions = IllegalArgumentException.class)
	public static void test_dropArgumentsToMatch_outrange_matching() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findVirtual(String.class, "concat", MethodType.methodType(String.class, String.class));
		MethodType typeList = h1.type().insertParameterTypes(1, String.class, String.class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, 1, typeList.parameterList(), 4);

		Assert.fail("dropArgumentsToMatch method must receive a valueTypes list that doesn't contain void.class");
	}

	/**
	 * test for empty original parameter list
	 */
    @Test(groups = {"level.extended"})
	public static void test_dropArgumentsToMatch_empty_original() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findStatic(Hash_Methods.class, "hash_void", MethodType.methodType(String.class));
		MethodType typeList = h1.type().insertParameterTypes(0, char.class, int.class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, 0, typeList.parameterList(), 0);

		Assert.assertEquals(h1.invoke('c', 2), "abc", "Transformed method handle did not return expected result");
	}

	/**
	 * test for empty valueList
	 */
	@Test(groups = {"level.extended"})
	public static void test_dropArgumentsToMatch_empty_valueList() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findStatic(Hash_Methods.class, "hash_void", MethodType.methodType(String.class));
		MethodType typeList = h1.type();

		h1 = MethodHandles.dropArgumentsToMatch(h1, 0, typeList.parameterList(), 0);

		Assert.assertEquals(h1.invoke(), "abc", "Transformed method handle did not return expected result");
	}

	/**
	 * test for full match, 0 skipped parameters
	 * ie. [M] + [P...M...A] => P...M...A
	 */
	@Test(groups = { "level.extended" })
	public static void test_dropArgumentsToMatch_no_skip() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findVirtual(String.class, "concat", MethodType.methodType(String.class, String.class));
		MethodType typeList = h1.type().insertParameterTypes(1, String.class, String.class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, 0, typeList.parameterList(), 1);

		Assert.assertEquals(h1.invoke("a", "x", "y", "b"), "xy", "Transformed method handle did not return expected result");
	}

	/**
	 * test for full skip, empty matching array
	 * ie. [M] + [P...M...A] => P...M...A
	 */
	@Test(groups = { "level.extended" })
	public static void test_dropArgumentsToMatch_skip_all() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findVirtual(String.class, "concat", MethodType.methodType(String.class, String.class));
		MethodType typeList = h1.type().insertParameterTypes(1, String.class, String.class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, 2, typeList.parameterList(), 0);

		Assert.assertEquals(h1.invoke("x", "y", "a", "b", "c", "d"), "xy", "Transformed method handle did not return expected result");
	}

	/**
	 * test where handle parameter list is a prefix of the valueType list
	 * ie. [S...M] + [M...A] => S...M...A
	 */
	@Test(groups = { "level.extended" })
	public static void test_dropArgumentsToMatch_prefix_list() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findVirtual(Hash_Methods.class, "hash_multi_arguments", MethodType.methodType(String.class, String.class, String.class, String.class, String.class));

		/* Method with rtype = String, ptype = [String, String, boolean, char, int, long, String, String] */
		MethodType typeList = MethodType.methodType(String.class, String.class, String.class, boolean.class, char.class, int.class, long.class, String.class, String.class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, 3, typeList.parameterList(), 0);

		Assert.assertEquals(h1.invoke(new Hash_Methods(), "a", "b", "c", "d", true, 'e', 1, 2, "f", "g"), "abcd", "Method handle did not return expected result");
	}

	/**
	 * test where handle parameter list is a suffix of the valueType list
	 * ie. [S...M] + [P...M] => S...P...M
	 */
	@Test(groups = { "level.extended" })
	public static void test_dropArgumentsToMatch_suffix_list() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findVirtual(Hash_Methods.class, "hash_multi_arguments", MethodType.methodType(String.class, String.class, String.class, String.class, String.class));

		/* Method with rtype = String, ptype = [boolean, char, int, long, String, String] */
		MethodType typeList = MethodType.methodType(String.class, boolean.class, char.class, int.class, long.class, String.class, String.class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, 3, typeList.parameterList(), 4);

		Hash_Methods hm = new Hash_Methods();
		Assert.assertEquals(h1.invoke(hm, "a", "b", true, 'e', 1, 2, "c", "d"), "abcd", "Method handle did not return expected result");
	}

	/**
	 * test where handle parameter list is a sublist of the valueType list
	 * ie. [S...M] + [P...M...A] => S...P...M...A
	 */
	@Test(groups = { "level.extended" })
	public static void test_dropArgumentsToMatch_normal_sublist() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findVirtual(Hash_Methods.class, "hash_multi_arguments", MethodType.methodType(String.class, String.class, String.class, String.class, String.class));

		/* Method with rtype = String, ptype = [boolean, char, int, long, String, String, boolean, char, int, long] */
		MethodType typeList = MethodType.methodType(String.class, boolean.class, char.class, int.class, long.class, String.class, String.class, boolean.class, char.class, int.class, long.class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, 3, typeList.parameterList(), 4);

		Hash_Methods hm = new Hash_Methods();
		Assert.assertEquals(h1.invoke(hm, "a", "b", true, 'e', 1, 2, "c", "d", false, 'f', 3, 4), "abcd", "Method handle did not return expected result");
	}

	/**
	 * test where handle parameter list is of type boolean
	 */
	@Test(groups = { "level.extended" })
	public static void test_dropArgumentsToMatch_normal_sublist_boolean() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findVirtual(Hash_Methods.class, "hash_boolean", MethodType.methodType(String.class, boolean.class, boolean.class));

		/* Method with rtype = String, ptype = [boolean, char, int, long, boolean, String] */
		MethodType typeList = MethodType.methodType(String.class, boolean.class, char.class, int.class, long.class, boolean.class, String.class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, 2, typeList.parameterList(), 4);

		Hash_Methods hm = new Hash_Methods();
		Assert.assertEquals(h1.invoke(hm, true, false, 'a', 1, 2, true, "b"), "true", "Method handle did not return expected result");
	}

	/**
	 * test where handle parameter list is of type byte
	 */
	@Test(groups = { "level.extended" })
	public static void test_dropArgumentsToMatch_normal_sublist_byte() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findVirtual(Hash_Methods.class, "hash_byte", MethodType.methodType(String.class, byte.class, byte.class));

		/* Method with rtype = String, ptype = [boolean, char, int, long, byte, String] */
		MethodType typeList = MethodType.methodType(String.class, boolean.class, char.class, int.class, long.class, byte.class, String.class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, 2, typeList.parameterList(), 4);

		Hash_Methods hm = new Hash_Methods();
		Assert.assertEquals(h1.invoke(hm, (byte)97, true, 'c', 1, 2, (byte)98, "d"), "ab", "Method handle did not return expected result");
	}

	/**
	 * test where handle parameter list is of type char
	 */
	@Test(groups = { "level.extended" })
	public static void test_dropArgumentsToMatch_normal_sublist_char() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findVirtual(Hash_Methods.class, "hash_char", MethodType.methodType(String.class, char.class, char.class));

		/* Method with rtype = String, ptype = [boolean, char, int, long, char, String] */
		MethodType typeList = MethodType.methodType(String.class, boolean.class, char.class, int.class, long.class, char.class, String.class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, 2, typeList.parameterList(), 4);

		Hash_Methods hm = new Hash_Methods();
		Assert.assertEquals(h1.invoke(hm, 'a', true, 'c', 1, 2, 'b', "d"), "ab", "Method handle did not return expected result");
	}

	/**
	 * test where handle parameter list is of type int
	 */
	@Test(groups = { "level.extended" })
	public static void test_dropArgumentsToMatch_normal_sublist_int() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findVirtual(Hash_Methods.class, "hash_int", MethodType.methodType(String.class, int.class, int.class));

		/* Method with rtype = String, ptype = [boolean, char, int, long, int, String] */
		MethodType typeList = MethodType.methodType(String.class, boolean.class, char.class, int.class, long.class, int.class, String.class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, 2, typeList.parameterList(), 4);

		Hash_Methods hm = new Hash_Methods();
		Assert.assertEquals(h1.invoke(hm, 1, true, 'a', 3, 4, 2, "b"), "12", "Method handle did not return expected result");
	}

	/**
	 * test where handle parameter list is of type double
	 */
	@Test(groups = { "level.extended" })
	public static void test_dropArgumentsToMatch_normal_sublist_double() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findVirtual(Hash_Methods.class, "hash_double", MethodType.methodType(String.class, double.class, double.class));

		/* Method with rtype = String, ptype = [boolean, char, int, long, double, String] */
		MethodType typeList = MethodType.methodType(String.class, boolean.class, char.class, int.class, long.class, double.class, String.class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, 2, typeList.parameterList(), 4);

		Hash_Methods hm = new Hash_Methods();
		Assert.assertEquals(h1.invoke(hm, 1, true, 'a', 3, 4, 2, "b"), "1.02.0", "Method handle did not return expected result");
	}

	/**
	 * test where handle parameter list is of type int[]
	 */
	@Test(groups = { "level.extended" })
	public static void test_dropArgumentsToMatch_normal_sublist_Array() throws Throwable {
		MethodHandle h1 = MethodHandles.lookup().findVirtual(Hash_Methods.class, "hash_array", MethodType.methodType(String.class, int[].class, int[].class));

		/* Method with rtype = String, ptype = [boolean, char, int, long, int[], int[]] */
		MethodType typeList = MethodType.methodType(String.class, boolean.class, char.class, int.class, long.class, int[].class, int[].class);

		h1 = MethodHandles.dropArgumentsToMatch(h1, 2, typeList.parameterList(), 4);

		Hash_Methods hm = new Hash_Methods();
		Assert.assertEquals(h1.invoke(hm, new int[] {1, 2}, true, 'a', 5, 6, new int[] {3,4}, null), "1234", "Method handle did not return expected result");
	}
}
