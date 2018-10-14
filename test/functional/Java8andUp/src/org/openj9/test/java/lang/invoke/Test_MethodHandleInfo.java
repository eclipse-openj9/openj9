package org.openj9.test.java.lang.invoke;

/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.invoke.*;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.reflect.*;
import java.security.AccessControlException;
import org.openj9.test.java.lang.invoke.helpers.*;
import static java.lang.invoke.MethodHandles.*;
import static java.lang.invoke.MethodType.*;


public class Test_MethodHandleInfo {
	@SuppressWarnings("unused")
	private String privateField = "privateField";
	protected String protectedField = "protectedField";
	String packageField = "packageField";
	public String publicField = "publicField";
	EnumTest enumField = EnumTest.TEST1;

	@SuppressWarnings("unused")
	private String privateMethod() { return "privateMethod"; }
	protected String protectedMethod() { return "protectedMethod"; }
	String packageMethod() { return "packageMethod"; }

	public String publicMethod() { return "publicMethod"; }
	public String publicVarargs(int... i) { return "publicVarargsMethod"; }
	public String publicNotVarargs(Object o) { return "publicVarargsMethod"; }

	/**
	 * Check return values of referenceKindToString
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_ReferenceKindToString() throws Throwable {
		AssertJUnit.assertEquals("getField", MethodHandleInfo.referenceKindToString(MethodHandleInfo.REF_getField));
		AssertJUnit.assertEquals("getStatic", MethodHandleInfo.referenceKindToString(MethodHandleInfo.REF_getStatic));
		AssertJUnit.assertEquals("putField", MethodHandleInfo.referenceKindToString(MethodHandleInfo.REF_putField));
		AssertJUnit.assertEquals("putStatic", MethodHandleInfo.referenceKindToString(MethodHandleInfo.REF_putStatic));
		AssertJUnit.assertEquals("invokeVirtual", MethodHandleInfo.referenceKindToString(MethodHandleInfo.REF_invokeVirtual));
		AssertJUnit.assertEquals("invokeStatic", MethodHandleInfo.referenceKindToString(MethodHandleInfo.REF_invokeStatic));
		AssertJUnit.assertEquals("invokeSpecial", MethodHandleInfo.referenceKindToString(MethodHandleInfo.REF_invokeSpecial));
		AssertJUnit.assertEquals("newInvokeSpecial", MethodHandleInfo.referenceKindToString(MethodHandleInfo.REF_newInvokeSpecial));
		AssertJUnit.assertEquals("invokeInterface", MethodHandleInfo.referenceKindToString(MethodHandleInfo.REF_invokeInterface));
		{
			boolean IAEThrown = false;
			try {
				MethodHandleInfo.referenceKindToString(0);
			} catch (IllegalArgumentException e) {
				IAEThrown = true;
			}
		}
		{
			boolean IAEThrown = false;
			try {
				MethodHandleInfo.referenceKindToString(-1);
			} catch (IllegalArgumentException e) {
				IAEThrown = true;
			}
		}
		{
			boolean IAEThrown = false;
			try {
				MethodHandleInfo.referenceKindToString(100);
			} catch (IllegalArgumentException e) {
				IAEThrown = true;
			}
		}
	}

	/**
	 * Check return values of toString
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_toString() throws Throwable {
		AssertJUnit.assertEquals("invokeVirtual org.openj9.test.java.lang.invoke.Test_MethodHandleInfo.privateMethod:()String", MethodHandleInfo.toString(MethodHandleInfo.REF_invokeVirtual, this.getClass(), "privateMethod", methodType(String.class)));
		AssertJUnit.assertEquals("getField org.openj9.test.java.lang.invoke.Test_MethodHandleInfo.publicField:()String", MethodHandleInfo.toString(MethodHandleInfo.REF_getField, this.getClass(), "publicField", methodType(String.class)));
	}

	/**
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_getName_Method() throws Throwable {
		MethodHandle mh = lookup().findVirtual(Test_MethodHandleInfo.class, "privateMethod", methodType(String.class));
		MethodHandleInfo mhi = lookup().revealDirect(mh);
		AssertJUnit.assertEquals("privateMethod", mhi.getName());
	}

	/**
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_getName_Field() throws Throwable {
		MethodHandle mh = lookup().findGetter(Test_MethodHandleInfo.class, "privateField", String.class);
		MethodHandleInfo mhi = lookup().revealDirect(mh);
		AssertJUnit.assertEquals("privateField", mhi.getName());
	}

	/**
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_getName_Constructor() throws Throwable {
		MethodHandle mh = lookup().findConstructor(Helper_MethodHandleInfoSubClass.class, methodType(void.class));
		MethodHandleInfo mhi = lookup().revealDirect(mh);
		AssertJUnit.assertEquals("<init>", mhi.getName());
	}

	/**
	 *
	 * @throws throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_getModifiers_Method() throws Throwable {
		{
			MethodHandle mh = lookup().findVirtual(Test_MethodHandleInfo.class, "privateMethod", methodType(String.class));
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			AssertJUnit.assertEquals(Modifier.PRIVATE, mhi.getModifiers());
		}
		{
			MethodHandle mh = lookup().findVirtual(Test_MethodHandleInfo.class, "packageMethod", methodType(String.class));
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			AssertJUnit.assertEquals(0, mhi.getModifiers());
		}
		{
			MethodHandle mh = lookup().findVirtual(Test_MethodHandleInfo.class, "publicVarargs", methodType(String.class, int[].class));
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			AssertJUnit.assertEquals((Modifier.PUBLIC | 0x0080), mhi.getModifiers());
		}
	}

	/**
	 *
	 * @throws throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_getModifiers_Field() throws Throwable {
		{
			MethodHandle mh = lookup().findGetter(Test_MethodHandleInfo.class, "privateField", String.class);
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			AssertJUnit.assertEquals(Modifier.PRIVATE, mhi.getModifiers());
		}
		{
			MethodHandle mh = lookup().findGetter(Test_MethodHandleInfo.class, "enumField", EnumTest.class);
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			AssertJUnit.assertEquals(0, mhi.getModifiers());
		}
	}

	/**
	 *
	 * @throws throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_getModifiers_Constructor() throws Throwable {
		{
			MethodHandle mh = lookup().findConstructor(Helper_MethodHandleInfoSubClass.class, methodType(void.class));
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			AssertJUnit.assertEquals(Modifier.PUBLIC, mhi.getModifiers());
		}
	}

	/**
	 * Calls reflectAs on Getter and Setter FieldHandles with different modifiers.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_ReflectAs_Field() throws Throwable {
		/* Private */
		{
			MethodHandle mh = lookup().findGetter(Test_MethodHandleInfo.class, "privateField", String.class);
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			Field f = mhi.reflectAs(Field.class, lookup());
			AssertJUnit.assertEquals("privateField", (String)f.get(new Test_MethodHandleInfo()));
		}
		{
			MethodHandle mh = lookup().findSetter(Test_MethodHandleInfo.class, "privateField", String.class);
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			Field f = mhi.reflectAs(Field.class, lookup());
			Test_MethodHandleInfo tmp = new Test_MethodHandleInfo();
			f.set(tmp, "Test");
			AssertJUnit.assertEquals("Test", (String)f.get(tmp));
			f.set(tmp, "privateField");
		}
		/* Protected */
		{
			MethodHandle mh = lookup().findGetter(Test_MethodHandleInfo.class, "protectedField", String.class);
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			Field f = mhi.reflectAs(Field.class, lookup());
			AssertJUnit.assertEquals("protectedField", (String)f.get(new Test_MethodHandleInfo()));
		}
		{
			MethodHandle mh = lookup().findSetter(Test_MethodHandleInfo.class, "protectedField", String.class);
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			Field f = mhi.reflectAs(Field.class, lookup());
			Test_MethodHandleInfo tmp = new Test_MethodHandleInfo();
			f.set(tmp, "Test");
			AssertJUnit.assertEquals("Test", (String)f.get(tmp));
			f.set(tmp, "protectedField");
		}
		/* Default */
		{
			MethodHandle mh = lookup().findGetter(Test_MethodHandleInfo.class, "packageField", String.class);
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			Field f = mhi.reflectAs(Field.class, lookup());
			AssertJUnit.assertEquals("packageField", (String)f.get(new Test_MethodHandleInfo()));
		}
		{
			MethodHandle mh = lookup().findSetter(Test_MethodHandleInfo.class, "packageField", String.class);
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			Field f = mhi.reflectAs(Field.class, lookup());
			Test_MethodHandleInfo tmp = new Test_MethodHandleInfo();
			f.set(tmp, "Test");
			AssertJUnit.assertEquals("Test", (String)f.get(tmp));
			f.set(tmp, "packageField");
		}
		/* Public */
		{
			MethodHandle mh = lookup().findGetter(Test_MethodHandleInfo.class, "publicField", String.class);
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			Field f = mhi.reflectAs(Field.class, lookup());
			AssertJUnit.assertEquals("publicField", (String)f.get(new Test_MethodHandleInfo()));
		}
		{
			MethodHandle mh = lookup().findSetter(Test_MethodHandleInfo.class, "publicField", String.class);
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			Field f = mhi.reflectAs(Field.class, lookup());
			Test_MethodHandleInfo tmp = new Test_MethodHandleInfo();
			f.set(tmp, "Test");
			AssertJUnit.assertEquals("Test", (String)f.get(tmp));
			f.set(tmp, "publicField");
		}
	}

	/**
	 * Calls reflectAs on MethodHandles with different modifiers.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_ReflectAs_Method() throws Throwable {
		/* Private */
		{
			MethodHandle mh = lookup().findVirtual(Test_MethodHandleInfo.class, "privateMethod", methodType(String.class));
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			Method m = mhi.reflectAs(Method.class, lookup());
			AssertJUnit.assertEquals("privateMethod", (String)m.invoke(new Test_MethodHandleInfo()));
		}
		/* Protected */
		{
			MethodHandle mh = lookup().findVirtual(Test_MethodHandleInfo.class, "protectedMethod", methodType(String.class));
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			Method m = mhi.reflectAs(Method.class, lookup());
			AssertJUnit.assertEquals("protectedMethod", (String)m.invoke(new Test_MethodHandleInfo()));
		}
		/* Default */
		{
			MethodHandle mh = lookup().findVirtual(Test_MethodHandleInfo.class, "packageMethod", methodType(String.class));
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			Method m = mhi.reflectAs(Method.class, lookup());
			AssertJUnit.assertEquals("packageMethod", (String)m.invoke(new Test_MethodHandleInfo()));
		}
		/* Public */
		{
			MethodHandle mh = lookup().findVirtual(Test_MethodHandleInfo.class, "publicMethod", methodType(String.class));
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			Method m = mhi.reflectAs(Method.class, lookup());
			AssertJUnit.assertEquals("publicMethod", (String)m.invoke(new Test_MethodHandleInfo()));
		}
	}

	/**
	 * Calls reflectAs on ConstructorHandles with different modifiers.
	 *
	 * @throws Throwable
	 * @throws InstantiationException
	 */
	@Test(groups = { "level.sanity" })
	public void test_ReflectAs_Constructor() throws Throwable, InstantiationException {
		MethodHandle mh = lookup().findConstructor(Helper_MethodHandleInfoSubClass.class, methodType(void.class));
		MethodHandleInfo mhi = lookup().revealDirect(mh);
		Constructor c = mhi.reflectAs(Constructor.class, lookup());
		AssertJUnit.assertEquals(Helper_MethodHandleInfoSubClass.class, c.newInstance().getClass());
	}

	/**
	 * Calls reflectAs on a MethodHandle with the wrong reference type.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_ReflectAs_WrongRefType() throws Throwable {
		// Method as Field
		{
			boolean CCEThrown = false;
			MethodHandle mh = lookup().findVirtual(this.getClass(), "publicMethod", methodType(String.class));
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			try {
				mhi.reflectAs(Field.class, lookup());
			} catch (ClassCastException e) {
				CCEThrown = true;
			}
			AssertJUnit.assertTrue(CCEThrown);
		}
		// Field as Constructor
		{
			boolean CCEThrown = false;
			MethodHandle mh = lookup().findGetter(this.getClass(), "publicField", String.class);
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			try {
				mhi.reflectAs(Constructor.class, lookup());
			} catch (ClassCastException e) {
				CCEThrown = true;
			}
			AssertJUnit.assertTrue(CCEThrown);
		}
		// Constructor as Method
		{
			boolean CCEThrown = false;
			MethodHandle mh = lookup().findConstructor(Helper_MethodHandleInfoSubClass.class, methodType(void.class));
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			try {
				mhi.reflectAs(Method.class, lookup());
			} catch (ClassCastException e) {
				CCEThrown = true;
			}
			AssertJUnit.assertTrue(CCEThrown);
		}
	}

	/**
	 * Calls reflectAs on a MethodHandle with different arguments set to null.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_ReflectAs_NullArguments() throws Throwable {
		// expected is null
		{
			boolean NPEThrown = false;
			MethodHandle mh = lookup().findVirtual(Test_MethodHandleInfo.class, "publicMethod", methodType(String.class));
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			try {
				mhi.reflectAs(null, lookup());
			} catch (NullPointerException e) {
				NPEThrown = true;
			}
			AssertJUnit.assertTrue(NPEThrown);
		}
		// lookup is null
		{
			boolean NPEThrown = false;
			MethodHandle mh = lookup().findVirtual(Test_MethodHandleInfo.class, "publicMethod", methodType(String.class));
			MethodHandleInfo mhi = lookup().revealDirect(mh);
			try {
				mhi.reflectAs(Method.class, null);
			} catch (NullPointerException e) {
				NPEThrown = true;
			}
			AssertJUnit.assertTrue(NPEThrown);
		}
	}

	/**
	 * Calls reflectAs on a MethodHandle with a lookup object that does not have access to the underlying member.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_ReflectAs_NotAccessible() throws Throwable {
		boolean IAEThrown = false;
		MethodHandle mh = Helper_MethodHandleInfoSubClass.lookup().findVirtual(Helper_MethodHandleInfoSubClass.class, "privateMethod", methodType(String.class));
		MethodHandleInfo mhi = Helper_MethodHandleInfoSubClass.lookup().revealDirect(mh);
		try {
			mhi.reflectAs(Method.class, lookup());
		} catch (IllegalArgumentException e) {
			IAEThrown = true;
		}
		AssertJUnit.assertTrue(IAEThrown);
	}

	/**
	 * Check access to MethodHandleInfo of local methods using local lookup object.
	 * All tests should pass.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_RevealDirect_Local() throws Throwable {
		Lookup lookup = lookup();
		/* Private */
		{
			MethodHandle mh = lookup.findVirtual(Test_MethodHandleInfo.class, "privateMethod", methodType(String.class));
			lookup().revealDirect(mh);
		}
		/* Protected */
		{
			MethodHandle mh = lookup.findVirtual(Test_MethodHandleInfo.class, "protectedMethod", methodType(String.class));
			lookup().revealDirect(mh);
		}
		/* Default */
		{
			MethodHandle mh = lookup.findVirtual(Test_MethodHandleInfo.class, "publicMethod", methodType(String.class));
			lookup().revealDirect(mh);
		}
		/* Public */
		{
			MethodHandle mh = lookup.findVirtual(Test_MethodHandleInfo.class, "packageMethod", methodType(String.class));
			lookup().revealDirect(mh);
		}
	}

	/**
	 * Check access to MethodHandleInfo of methods from the same package using local lookup object.
	 * Private and protected should fail, default and public should pass.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_RevealDirect_SamePackage() throws Throwable {
		Lookup findLookup = Helper_MethodHandleInfoSubClass.lookup();
		Lookup lookup = lookup();
		/* Private */
		{
			boolean IAEThrown = false;
			MethodHandle mh = findLookup.findVirtual(Helper_MethodHandleInfoSubClass.class, "privateMethod", methodType(String.class));
			try {
				lookup.revealDirect(mh);
			} catch (IllegalArgumentException e) {
				IAEThrown = true;
			}
			AssertJUnit.assertTrue(IAEThrown);
		}
		/* Protected */
		{
			MethodHandle mh = findLookup.findVirtual(Helper_MethodHandleInfoSubClass.class, "protectedMethod", methodType(String.class));
			lookup.revealDirect(mh);
		}

		/* Default */
		{
			MethodHandle mh = findLookup.findVirtual(Helper_MethodHandleInfoSubClass.class, "packageMethod", methodType(String.class));
			lookup.revealDirect(mh);
		}
		/* Public */
		{
			MethodHandle mh = findLookup.findVirtual(Helper_MethodHandleInfoSubClass.class, "publicMethod", methodType(String.class));
			lookup.revealDirect(mh);
		}
	}

	/**
	 * Check access to MethodHandleInfo of methods from a public class in another package using local lookup object.
	 * Correct behaviour is confirmed using a lookup object from the same class as the underlying method in the MethodHandle.
	 * Private, protected and default should fail, public should pass.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_RevealDirect_OtherPackagePublicClass() throws Throwable {
		/* Private */
		{
			boolean IAEThrown = false;
			MethodHandle mh = Helper_MethodHandleInfoOtherPackagePublic.lookup().findVirtual(Helper_MethodHandleInfoOtherPackagePublic.class, "privateMethod", methodType(String.class));
			try {
				lookup().revealDirect(mh);
			} catch (IllegalArgumentException e) {
				IAEThrown = true;
			}
			AssertJUnit.assertTrue(IAEThrown);
		}
		/* Protected */
		{
			boolean IAEThrown = false;
			MethodHandle mh = Helper_MethodHandleInfoOtherPackagePublic.lookup().findVirtual(Helper_MethodHandleInfoOtherPackagePublic.class, "protectedMethod", methodType(String.class));
			try {
				lookup().revealDirect(mh);
			} catch (IllegalArgumentException e) {
				IAEThrown = true;
			}
			AssertJUnit.assertTrue(IAEThrown);
		}
		/* Default */
		{
			boolean IAEThrown = false;
			MethodHandle mh = Helper_MethodHandleInfoOtherPackagePublic.lookup().findVirtual(Helper_MethodHandleInfoOtherPackagePublic.class, "packageMethod", methodType(String.class));
			try {
				lookup().revealDirect(mh);
			} catch (IllegalArgumentException e) {
				IAEThrown = true;
			}
			AssertJUnit.assertTrue(IAEThrown);
		}
		/* Private */
		{
			MethodHandle mh = Helper_MethodHandleInfoOtherPackagePublic.lookup().findVirtual(Helper_MethodHandleInfoOtherPackagePublic.class, "privateMethod", methodType(String.class));
			Helper_MethodHandleInfoOtherPackagePublic.lookup().revealDirect(mh);
		}
		/* Protected */
		{
			MethodHandle mh = Helper_MethodHandleInfoOtherPackagePublic.lookup().findVirtual(Helper_MethodHandleInfoOtherPackagePublic.class, "protectedMethod", methodType(String.class));
			Helper_MethodHandleInfoOtherPackagePublic.lookup().revealDirect(mh);
		}
		/* Default */
		{
			MethodHandle mh = Helper_MethodHandleInfoOtherPackagePublic.lookup().findVirtual(Helper_MethodHandleInfoOtherPackagePublic.class, "packageMethod", methodType(String.class));
			Helper_MethodHandleInfoOtherPackagePublic.lookup().revealDirect(mh);
		}
		/* Public */
		{
			MethodHandle mh = Helper_MethodHandleInfoOtherPackagePublic.lookup().findVirtual(Helper_MethodHandleInfoOtherPackagePublic.class, "publicMethod", methodType(String.class));
			lookup().revealDirect(mh);
		}
	}

	/**
	 * Check access to MethodHandleInfo of method from a private class in another package using local lookup object.
	 * Correct behaviour is confirmed using a lookup object from the same package as the class of the underlying method in the MethodHandle.
	 * Public should fail. Other methods will not be tested.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_RevealDirect_OtherPackagePrivateClass() throws Throwable {
		MethodHandle mh = Helper_MethodHandleInfoOtherPackagePublic.privateClassMethodHandle();
		{
			boolean IAEThrown = false;
			try {
				lookup().revealDirect(mh);
			} catch (IllegalArgumentException e) {
				IAEThrown = true;
			}
			AssertJUnit.assertTrue(IAEThrown);
		}
		{
			Helper_MethodHandleInfoOtherPackagePublic.lookup().revealDirect(mh);
		}
	}

	/**
	 * Check access to MethodHandleInfo of method from a super class in another package using subclass lookup object.
	 * Public and protected should pass. Private and default should fail.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_RevealDirect_OtherPackageSubClass() throws Throwable {
		/* Private */
		{
			boolean IAEThrown = false;
			MethodHandle mh = Helper_MethodHandleInfoOtherPackagePublic.lookup().findVirtual(Helper_MethodHandleInfoOtherPackagePublic.class, "privateMethod", methodType(String.class));
			try {
				Helper_MethodHandleInfoSubClass.lookup().revealDirect(mh);
			} catch (IllegalArgumentException e) {
				IAEThrown = true;
			}
			AssertJUnit.assertTrue(IAEThrown);
		}
		/* Protected */
		{
			MethodHandle mh = Helper_MethodHandleInfoOtherPackagePublic.lookup().findVirtual(Helper_MethodHandleInfoOtherPackagePublic.class, "protectedMethod", methodType(String.class));
			Helper_MethodHandleInfoSubClass.lookup().revealDirect(mh);
		}
		/* Default */
		{
			boolean IAEThrown = false;
			MethodHandle mh = Helper_MethodHandleInfoOtherPackagePublic.lookup().findVirtual(Helper_MethodHandleInfoOtherPackagePublic.class, "packageMethod", methodType(String.class));
			try {
				Helper_MethodHandleInfoSubClass.lookup().revealDirect(mh);
			} catch (IllegalArgumentException e) {
				IAEThrown = true;
			}
			AssertJUnit.assertTrue(IAEThrown);
		}
		/* Public */
		{
			MethodHandle mh = Helper_MethodHandleInfoOtherPackagePublic.lookup().findVirtual(Helper_MethodHandleInfoOtherPackagePublic.class, "publicMethod", methodType(String.class));
			Helper_MethodHandleInfoSubClass.lookup().revealDirect(mh);
		}
	}

	/**
	 * Check that a NullPointerException is thrown when revealDirect target is null;
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_RevealDirect_TargetNull() throws Throwable {
		boolean NPEThrown = false;
		try {
			lookup().revealDirect(null);
		} catch (NullPointerException e) {
			NPEThrown = true;
		}
		AssertJUnit.assertTrue(NPEThrown);
	}

	/**
	 * Check that a IllegalArgumentException is thrown when revealDirect target is not a direct handle.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_RevealDirect_TargetNotDirectHandle() throws Throwable {
		boolean IAEThrown = false;
		MethodHandle mh = lookup().findVirtual(Test_MethodHandleInfo.class, "privateMethod", methodType(String.class));
		mh = mh.asSpreader(Object[].class, 0);
		try {
			lookup().revealDirect(mh);
		} catch (IllegalArgumentException e) {
			IAEThrown = true;
		}
		AssertJUnit.assertTrue(IAEThrown);
	}

	/**
	 * Check that a IllegalArgumentException is thrown when revealDirect target is a varargs adaptor.
	 * This is checked because the same MethodHandle derivative is used for direct and indirect vararg method handles.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_RevealDirect_TargetVarargsAdaptor() throws Throwable {
		boolean IAEThrown = false;
		MethodHandle mh = lookup().findVirtual(Test_MethodHandleInfo.class, "publicNotVarargs", methodType(String.class, Object.class));
		mh = mh.asVarargsCollector(Object[].class);
		try {
			lookup().revealDirect(mh);
		} catch (IllegalArgumentException e) {
			IAEThrown = true;
		}
		AssertJUnit.assertTrue(IAEThrown);
	}

	/**
	 * Check that a IllegalArgumentException is thrown when revealDirect target is a direct varargs handle.
	 * This is checked because the same MethodHandle derivative is used for direct and indirect vararg method handles.
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_RevealDirect_TargetVarargsHandle() throws Throwable {
		MethodHandle mh = lookup().findVirtual(Test_MethodHandleInfo.class, "publicVarargs", methodType(String.class, int[].class));
		lookup().revealDirect(mh);
	}

	/**
	 * Check that a SecurityException is thrown when ...
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_RevealDirect_Security() throws Throwable {
		final String lookup = "org.openj9.test.java.lang.invoke.Helper_MethodHandleInfo";
		final String methodHandle = "org.openj9.test.java.lang.invoke.helpers.Helper_MethodHandleInfoOtherPackagePublic";

		// Get lookup object from class loader L1
		ClassLoader l1 = new ClassLoader(Test_MethodHandleInfo.class.getClassLoader()) {

			@Override
			public Class<?> loadClass(String className) throws ClassNotFoundException {
				if (className.equals(lookup)) {
					return getClass( className );
				}
				if (className.equals(methodHandle)) {
					return getClass(className);
				}
				return super.loadClass(className);
			}

			private Class<?> getClass( String className )throws ClassNotFoundException {
				String classFile = className.replace('.', '/') + ".class";
				try {
					InputStream classStream = getClass().getClassLoader().getResourceAsStream( classFile );
					if ( classStream == null ) {
						throw new ClassNotFoundException( "Error loading class : " + classFile );
					}
			        int size = classStream.available();
			        byte classRep[] = new byte[size];
			        DataInputStream in = new DataInputStream( classStream );
			        in.readFully( classRep );
			        in.close();
			        Class<?> clazz = defineClass( className, classRep, 0, classRep.length );
					return clazz;
				} catch ( IOException e ) {
					throw new ClassNotFoundException( e.getMessage() );
				}
			}

		};
		Class classL1 = l1.loadClass(lookup);
		Method mL1 = classL1.getDeclaredMethod("lookup");
		Lookup lookup1 = (Lookup)mL1.invoke(null);

		// Get MethodHandle from class loader L2
		ClassLoader l2 = new ClassLoader(Test_MethodHandleInfo.class.getClassLoader()) {

			@Override
			public Class<?> loadClass(String className) throws ClassNotFoundException {
				if ( className.equals(methodHandle) ) {
					return getClass( className );
				}
				return super.loadClass( className );
			}

			private Class<?> getClass( String className )throws ClassNotFoundException {
				String classFile = className.replace('.', '/') + ".class";
				try {
					InputStream classStream = getClass().getClassLoader().getResourceAsStream( classFile );
					if ( classStream == null ) {
						throw new ClassNotFoundException( "Error loading class : " + classFile );
					}
			        int size = classStream.available();
			        byte classRep[] = new byte[size];
			        DataInputStream in = new DataInputStream( classStream );
			        in.readFully( classRep );
			        in.close();
			        Class<?> clazz = defineClass( className, classRep, 0, classRep.length );
					return clazz;
				} catch ( IOException e ) {
					throw new ClassNotFoundException( e.getMessage() );
				}
			}

		};
		Class classL2 = l2.loadClass(methodHandle);
		Method mL2 = classL2.getDeclaredMethod("publicClassMethodHandle");
		MethodHandle mh = (MethodHandle)mL2.invoke(null);
		Method m2L2 = classL2.getDeclaredMethod("lookup");
		Lookup lookup2 = (Lookup)m2L2.invoke(null);

		helper_turnOwnSecurityManagerOn();

		boolean ACEThrown = false;
		try {
			MethodHandleInfo mhi1 = lookup1.revealDirect(mh);
		} catch (AccessControlException e) {
			ACEThrown = true;
		} catch (IllegalArgumentException e) {
			Assert.fail("IllegalArgumentException thrown, expected AccessControlException.");
		} finally {
			helper_resetSecurityManager();
		}
		AssertJUnit.assertTrue(ACEThrown);
	}

	/**
	 * Caller-sensitive MH can only be cracked by the lookup object used to create it. Test with other lookup object.
	 */
	@Test(groups = { "level.sanity" })
	public void test_RevealDirect_CrackCallerSensitive() throws Throwable {
		// Lookup with local Lookup object
		boolean IAEThrown = false;
		MethodHandle mh = MethodHandles.lookup().findStatic(Class.class, "forName", methodType(Class.class, String.class));
		try {
			// Crack using different Lookup object
			Helper_MethodHandleInfo.lookup().revealDirect(mh);
		} catch (IllegalArgumentException e) {
			IAEThrown = true;
		}
		AssertJUnit.assertTrue(IAEThrown);
	}

	@Test(groups = { "level.sanity" })
	public void test_MHsReflectAs_WithSecMgr() throws Throwable {
		helper_turnSecurityManagerOn();
		boolean ACEThrown = false;
		MethodHandle target = lookup().findGetter(this.getClass(), "publicField", String.class);
		try {
			reflectAs(Field.class, target);
		} catch (SecurityException e) {
			ACEThrown = true;
		} finally {
			helper_resetSecurityManager();
		}
		AssertJUnit.assertTrue(ACEThrown);
	}

	@Test(groups = { "level.sanity" })
	public void test_MHsReflectAs_WithoutSecMgr() throws Throwable {
		helper_turnSecurityManagerOff();
		MethodHandle target = lookup().findGetter(this.getClass(), "publicField", String.class);
		Field f = reflectAs(Field.class, target);
		String s = (String)f.get(new Test_MethodHandleInfo());
		helper_resetSecurityManager();
		AssertJUnit.assertEquals("publicField", s);
	}

	private SecurityManager secmgr;
	private void helper_turnSecurityManagerOn() {
		secmgr = System.getSecurityManager();
		System.setSecurityManager(new SecurityManager());
	}

	private void helper_turnOwnSecurityManagerOn() {
		secmgr = System.getSecurityManager();
		System.setSecurityManager(new SecurityManager() {
			@Override
			public void checkPackageAccess(String arg0) {
				super.checkPackageAccess(arg0);
				if (arg0.equals("org.openj9.test.java.lang.invoke.helpers")) {
					throw new AccessControlException("");
				}
			}
		});
	}

	private void helper_turnSecurityManagerOff() {
		secmgr = System.getSecurityManager();
		System.setSecurityManager(null);
	}

	private void helper_resetSecurityManager() {
		System.setSecurityManager(secmgr);
	}

	enum EnumTest {TEST1, TEST2}
}
