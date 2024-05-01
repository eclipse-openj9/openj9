package org.openj9.test.constantpool;

/*
 * Copyright IBM Corp. and others 2021
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */

import org.testng.Assert;
import org.testng.annotations.Test;
import org.openj9.test.util.VersionCheck;

import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.util.function.Consumer;

@Test(groups = { "level.sanity" })
public class ClassInitTest {
	static Method getConstantPool;
	static Object javaLangAccess;

	static Method getSize;
	static Method getClassAtIfLoaded;
	static Method getClassAt;
	static Method getMethodAtIfLoaded;
	static Method getMethodAt;
	static Method getFieldAtIfLoaded;
	static Method getFieldAt;
	static boolean clinitInvoked = false;

	public ClassInitTest() throws Throwable {
		int version = VersionCheck.major();

		Class SharedSecrets;
		Class JavaLangAccess;
		Class ConstantPool;

		if (version == 8) {
			SharedSecrets = Class.forName("sun.misc.SharedSecrets");
			JavaLangAccess = Class.forName("sun.misc.JavaLangAccess");
			ConstantPool = Class.forName("sun.reflect.ConstantPool");
		} else if (version == 11) {
			SharedSecrets = Class.forName("jdk.internal.misc.SharedSecrets");
			JavaLangAccess = Class.forName("jdk.internal.misc.JavaLangAccess");
			ConstantPool = Class.forName("jdk.internal.reflect.ConstantPool");
		} else {
			SharedSecrets = Class.forName("jdk.internal.access.SharedSecrets");
			JavaLangAccess = Class.forName("jdk.internal.access.JavaLangAccess");
			ConstantPool = Class.forName("jdk.internal.reflect.ConstantPool");
		}

		Method getJavaLangAccess = SharedSecrets.getDeclaredMethod("getJavaLangAccess");
		getConstantPool = JavaLangAccess.getDeclaredMethod("getConstantPool", new Class[] {Class.class});
		javaLangAccess = getJavaLangAccess.invoke(null);

		getSize = ConstantPool.getDeclaredMethod("getSize");
		getClassAtIfLoaded = ConstantPool.getDeclaredMethod("getClassAtIfLoaded", new Class[] {int.class});
		getClassAt = ConstantPool.getDeclaredMethod("getClassAt", new Class[] {int.class});
		getMethodAtIfLoaded = ConstantPool.getDeclaredMethod("getMethodAtIfLoaded", new Class[] {int.class});
		getMethodAt = ConstantPool.getDeclaredMethod("getMethodAt", new Class[] {int.class});
		getFieldAtIfLoaded = ConstantPool.getDeclaredMethod("getFieldAtIfLoaded", new Class[] {int.class});
		getFieldAt = ConstantPool.getDeclaredMethod("getFieldAt", new Class[] {int.class});
	}

	/**
	 * Tests that ConstantPool.getClassAtIfLoaded returns nonnull if class is loaded (TestClass1a::staticMethod).
	 * Tests that clinit is invoked right before the class's static method.
	 */
	@Test
	public static void test_getClassAtIfLoaded() throws Throwable {
		clinitInvoked = false;
		Consumer<Object> consumer = TestClass1a::staticMethod;
		Object cp = getConstantPool.invoke(javaLangAccess, consumer.getClass());
		int size = (int)getSize.invoke(cp);

		for (int i = 1; i < size; i++) {
			try {
				Class c = (Class)getClassAtIfLoaded.invoke(cp, i);
				Assert.assertNotNull(c);
			} catch (InvocationTargetException e) {
				/* Not a class */
				Assert.assertTrue(e.getCause() instanceof IllegalArgumentException);
				continue;
			}
		}

		Assert.assertFalse(clinitInvoked);
		TestClass1a.staticMethod(null);
	}

	/**
	 * Tests that ConstantPool.getClassAt does not invoke clinit and that clinit is invoked right before the
	 * static method.
	 */
	@Test
	public static void test_getClassAt() throws Throwable {
		clinitInvoked = false;
		Consumer<Object> consumer = TestClass1b::staticMethod;
		Object cp = getConstantPool.invoke(javaLangAccess, consumer.getClass());
		int size = (int)getSize.invoke(cp);

		for (int i = 1; i < size; i++) {
			try {
				Class c = (Class)getClassAt.invoke(cp, i);
				Assert.assertNotNull(c);
			} catch (InvocationTargetException e) {
				/* Not a class */
				Assert.assertTrue(e.getCause() instanceof IllegalArgumentException);
				continue;
			}
		}

		Assert.assertFalse(clinitInvoked);
		TestClass1b.staticMethod(null);
	}

	/**
	 * Tests that ConstantPool.getMethodAtIfLoaded returns nonnull if class is loaded (TestClass2a::staticMethod).
	 * Tests that clinit is invoked right before the class's static method.
	 */
	@Test
	public static void test_getMethodAtIfLoaded() throws Throwable {
		clinitInvoked = false;
		Consumer<Object> consumer = TestClass2a::staticMethod;
		Object cp = getConstantPool.invoke(javaLangAccess, consumer.getClass());
		int size = (int)getSize.invoke(cp);

		for (int i = 1; i < size; i++) {
			try {
				Member m = (Member)getMethodAtIfLoaded.invoke(cp, i);
				Assert.assertNotNull(m);
			} catch (InvocationTargetException e) {
				/* Not a method */
				Assert.assertTrue(e.getCause() instanceof IllegalArgumentException);
				continue;
			}
		}

		Assert.assertFalse(clinitInvoked);
		TestClass2a.staticMethod(null);
	}

	/**
	 * Tests that ConstantPool.getMethodAt does not invoke clinit and that clinit is invoked right before the
	 * static method.
	 */
	@Test
	public static void test_getMethodAt() throws Throwable {
		clinitInvoked = false;
		Consumer<Object> consumer = TestClass2b::staticMethod;
		Object cp = getConstantPool.invoke(javaLangAccess, consumer.getClass());
		int size = (int)getSize.invoke(cp);

		for (int i = 1; i < size; i++) {
			try {
				Member m = (Member)getMethodAt.invoke(cp, i);
				Assert.assertNotNull(m);
			} catch (InvocationTargetException e) {
				/* Not a method */
				Assert.assertTrue(e.getCause() instanceof IllegalArgumentException);
				continue;
			}
		}

		Assert.assertFalse(clinitInvoked);
		TestClass2b.staticMethod(null);
	}

	/**
	 * Tests that ConstantPool.getFieldAtIfLoaded returns nonnull if class is loaded (TestClass3a::staticMethod).
	 * Tests that clinit is invoked right before the TestClass3a.field access.
	 */
	@Test
	public static void test_getFieldAtIfLoaded() throws Throwable {
		clinitInvoked = false;
		Consumer<Object> consumer = TestClass3a::staticMethod;
		Object cp = getConstantPool.invoke(javaLangAccess, TestClass3a.class);
		int size = (int)getSize.invoke(cp);

		for (int i = 1; i < size; i++) {
			try {
				Field f = (Field)getFieldAtIfLoaded.invoke(cp, i);
				Assert.assertNotNull(f);
			} catch (InvocationTargetException e) {
				/* Not a field */
				Assert.assertTrue(e.getCause() instanceof IllegalArgumentException);
				continue;
			}
		}

		Assert.assertFalse(clinitInvoked);
		Object a = TestClass3a.field;
		Assert.assertTrue(clinitInvoked);
	}

	/**
	 * Tests that ConstantPool.getFieldAt does not invoke clinit and that clinit is invoked right before the
	 * TestClass3b.field access.
	 */
	@Test
	public static void test_getFieldAt() throws Throwable {
		clinitInvoked = false;
		Consumer<Object> consumer = TestClass3b::staticMethod;
		Object cp = getConstantPool.invoke(javaLangAccess, TestClass3b.class);
		int size = (int)getSize.invoke(cp);

		for (int i = 1; i < size; i++) {
			try {
				Field f = (Field)getFieldAt.invoke(cp, i);
				Assert.assertNotNull(f);
			} catch (InvocationTargetException e) {
				/* Not a field */
				Assert.assertTrue(e.getCause() instanceof IllegalArgumentException);
				continue;
			}
		}

		Assert.assertFalse(clinitInvoked);
		Object a = TestClass3b.field;
		Assert.assertTrue(clinitInvoked);
	}

	public static class TestClass1a {
		static {
			clinitInvoked = true;
		}

		static class test {}

		public static void staticMethod(Object o) {
			Assert.assertTrue(clinitInvoked);
			test t = new test();
		}
	}

	public static class TestClass1b {
		static {
			clinitInvoked = true;
		}

		static class test {}

		public static void staticMethod(Object o) {
			Assert.assertTrue(clinitInvoked);
			test t = new test();
		}
	}

	public static class TestClass2a {
		static {
			clinitInvoked = true;
		}

		public static void staticMethod(Object o) {
			Assert.assertTrue(clinitInvoked);
		}
	}

	public static class TestClass2b {
		static {
			clinitInvoked = true;
		}

		public static void staticMethod(Object o) {
			Assert.assertTrue(clinitInvoked);
		}
	}

	public static class TestClass3a {
		static {
			clinitInvoked = true;
		}

		public static void staticMethod(Object o) {
			Assert.assertTrue(clinitInvoked);
			field = new Object();
		}

		static Object field;
	}

	public static class TestClass3b {
		static {
			clinitInvoked = true;
		}

		public static void staticMethod(Object o) {
			Assert.assertTrue(clinitInvoked);
			field = new Object();
		}

		static Object field;
	}
}
