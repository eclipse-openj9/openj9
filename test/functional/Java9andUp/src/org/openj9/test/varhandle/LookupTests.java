/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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
package org.openj9.test.varhandle;

import org.testng.annotations.Test;
import org.openj9.test.varhandle.otherpkg.*;
import org.testng.Assert;
import java.lang.reflect.*;
import java.nio.ByteOrder;
import java.lang.invoke.*;

/**
 * Test basic VarHandle.Lookup behaviour.
 * 
 * @author Bjorn Vardal
 */
@Test(groups = { "level.extended" })
public class LookupTests {
	/**
	 * Get a {@link VarHandle} for a package field in {@link OtherPackageHelper}.
	 */
	@Test(expectedExceptions=IllegalAccessException.class)
	public void testVarHandlePackageOtherPackage() throws Throwable {
		MethodHandles.lookup().findVarHandle(OtherPackageHelper.class, "packageInt", int.class);
	}
	
	/**
	 * Get a {@link VarHandle} for a public field in {@link OtherPackageHelper}.
	 */
	@Test
	public void testVarHandlePublicOtherPackage() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(OtherPackageHelper.class, "publicInt", int.class);
		Assert.assertNotNull(vh);
		vh.set(new OtherPackageHelper(), 2);
	}
	
	/**
	 * Get a {@link VarHandle} for a private field in {@link OtherPackageHelper}.
	 */
	@Test(expectedExceptions=IllegalAccessException.class)
	public void testVarHandlePrivateOtherPackage() throws Throwable {
		MethodHandles.lookup().findVarHandle(OtherPackageHelper.class, "privateInt", int.class);
	}

	/**
	 * Get a {@link VarHandle} for a protected field in {@link OtherPackageHelper}.
	 */
	@Test(expectedExceptions=IllegalAccessException.class)
	public void testVarHandleProtectedOtherPackage() throws Throwable {
		MethodHandles.lookup().findVarHandle(OtherPackageHelper.class, "protectedInt", int.class);
	}
	
	/**
	 * Get a {@link VarHandle} for an instance field using the factory method for static fields.
	 */
	@Test(expectedExceptions=IllegalAccessException.class)
	public void testLookupInstanceFieldAsStatic() throws Throwable {
		MethodHandles.lookup().findStaticVarHandle(InstanceHelper.class, "i", int.class);
	}
	
	/**
	 * Get a {@link VarHandle} for an static field using the factory method for instance fields.
	 */
	@Test(expectedExceptions=IllegalAccessException.class)
	public void testLookupStaticFieldAsInstance() throws Throwable {
		MethodHandles.lookup().findVarHandle(StaticHelper.class, "i", int.class);
	}
	
	/**
	 * Look up an instance field using an invalid (unrelated) type ({@link SomeType} for {@link String}).
	 */
	@Test(expectedExceptions=NoSuchFieldException.class)
	public void testLookupIncorrectFieldType() throws Throwable {
		MethodHandles.lookup().findVarHandle(InstanceHelper.class, "l", SomeType.class);
	}
	
	/**
	 * Look up a field that doesn't exist.
	 */
	@Test(expectedExceptions=NoSuchFieldException.class)
	public void testLookupIncorrectFieldName() throws Throwable {
		MethodHandles.lookup().findVarHandle(InstanceHelper.class, "noSuchField", Object.class);
	}
	
	/**
	 * Pass null arguments to VarHandle factory methods. <br />
	 *  - Instance field, declaring class
	 */
	@Test(expectedExceptions = NullPointerException.class)
	public void testNullArgInstanceFieldDeclaringClass() throws Throwable {
		MethodHandles.lookup().findVarHandle(null, "l", String.class);
	}
	
	/**
	 * Pass null arguments to VarHandle factory methods. <br />
	 *  - Instance field, field name
	 */
	@Test(expectedExceptions = NullPointerException.class)
	public void testNullArgInstanceFieldFieldName() throws Throwable {
		MethodHandles.lookup().findVarHandle(InstanceHelper.class, null, String.class);
	}
	
	/**
	 * Pass null arguments to VarHandle factory methods. <br />
	 *  - Instance field, field type
	 */
	@Test(expectedExceptions = NullPointerException.class)
	public void testNullArgInstanceFieldFieldType() throws Throwable {
		MethodHandles.lookup().findVarHandle(InstanceHelper.class, "l", null);
	}
	
	/**
	 * Pass null arguments to VarHandle factory methods. <br />
	 *  - Static field, declaring class
	 */
	@Test(expectedExceptions = NullPointerException.class)
	public void testNullArgStaticFieldDeclaringClass() throws Throwable {
		MethodHandles.lookup().findStaticVarHandle(null, "l", String.class);
	}
	
	/**
	 * Pass null arguments to VarHandle factory methods. <br />
	 *  - Instance field, field name
	 */
	@Test(expectedExceptions = NullPointerException.class)
	public void testNullArgStaticFieldFieldName() throws Throwable {
		MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, null, String.class);
	}
	
	/**
	 * Pass null arguments to VarHandle factory methods. <br />
	 *  - Instance field, field type
	 */
	@Test(expectedExceptions = NullPointerException.class)
	public void testNullArgStaticFieldFieldType() throws Throwable {
		MethodHandles.lookup().findStaticVarHandle(StaticHelper.class, "l", null);
	}
	
	/**
	 * Pass null arguments to VarHandle factory methods. <br />
	 *  - Array element, array type
	 */
	@Test(expectedExceptions = NullPointerException.class)
	public void testNullArgArrayElementArrayType() throws Throwable {
		MethodHandles.arrayElementVarHandle(null);
	}
	
	/**
	 * Pass null arguments to VarHandle factory methods. <br />
	 *  - ByteArrayView, view type
	 */
	@Test(expectedExceptions = NullPointerException.class)
	public void testNullArgByteArrayViewArrayType() throws Throwable {
		MethodHandles.byteArrayViewVarHandle(null, ByteOrder.BIG_ENDIAN);
	}
	
	/**
	 * Pass null arguments to VarHandle factory methods. <br />
	 *  - ByteArrayView, view type
	 */
	@Test(expectedExceptions = NullPointerException.class)
	public void testNullArgByteArrayViewByteOrder() throws Throwable {
		MethodHandles.byteArrayViewVarHandle(int[].class, null);
	}
	
	/**
	 * Pass null arguments to VarHandle factory methods. <br />
	 *  - ByteArrayView, view type
	 */
	@Test(expectedExceptions = NullPointerException.class)
	public void testNullArgByteBufferViewArrayType() throws Throwable {
		MethodHandles.byteBufferViewVarHandle(null, ByteOrder.BIG_ENDIAN);
	}
	
	/**
	 * Pass null arguments to VarHandle factory methods. <br />
	 *  - ByteArrayView, view type
	 */
	@Test(expectedExceptions = NullPointerException.class)
	public void testNullArgByteBufferViewByteOrder() throws Throwable {
		MethodHandles.byteBufferViewVarHandle(int[].class, null);
	}
	
	/**
	 * Look up a field in the declaring class's super class.
	 */
	@Test
	public void testLookupFieldInSuperClass() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(Child.class, "i", int.class);
		Assert.assertNotNull(vh);
	}
	
	/**
	 * Look up a field using the field's super class.
	 */
	@Test(expectedExceptions = NoSuchFieldException.class)
	public void testLookupFieldUsingSuperType() throws Throwable {
		MethodHandles.lookup().findVarHandle(InstanceHelper.class, "l", Object.class);
	}
	
	/**
	 * Look up a field using the field's super interface.
	 */
	@Test(expectedExceptions = NoSuchFieldException.class)
	public void testLookupFieldSuperInterface() throws Throwable {
		MethodHandles.lookup().findVarHandle(InstanceHelper.class, "l", CharSequence.class);
	}
	
	/**
	 * Unreflect an accessible static field and read the value.
	 */
	@Test
	public void testUnreflectAccessibleStaticField() throws Throwable {
		Field f = OtherPackageHelper.class.getField("publicStaticInt");
		VarHandle vh = MethodHandles.lookup().unreflectVarHandle(f);
		Assert.assertNotNull(vh);
		int result = (int)vh.getVolatile();
		Assert.assertEquals(OtherPackageHelper.publicStaticInt, result);
	}
	
	/**
	 * Unreflect an inaccessible static field.
	 */
	@Test(expectedExceptions=IllegalAccessException.class)
	public void testUnreflectInaccessibleStaticField() throws Throwable {
		Field f = OtherPackageHelper.class.getDeclaredField("privateStaticInt");
		f.setAccessible(true);
		MethodHandles.lookup().unreflectVarHandle(f);
	}
	
	/**
	 * Unreflect an accessible instance field and read the value.
	 */
	@Test
	public void testUnreflectAccessibleInstanceField() throws Throwable {
		Field f = OtherPackageHelper.class.getField("publicInt");
		VarHandle vh = MethodHandles.lookup().unreflectVarHandle(f);
		Assert.assertNotNull(vh);
		OtherPackageHelper helper = new OtherPackageHelper();
		int result = (int)vh.getVolatile(helper);
		Assert.assertEquals(helper.publicInt, result);
	}
	
	/**
	 * Unreflect an inaccessible instance field.
	 */
	@Test(expectedExceptions=IllegalAccessException.class)
	public void testUnreflectInaccessibleInstanceField() throws Throwable {
		Field f = OtherPackageHelper.class.getDeclaredField("privateInt");
		f.setAccessible(true);
		MethodHandles.lookup().unreflectVarHandle(f);
	}
	
	/**
	 * Unreflect <code>null</code>.
	 */
	@Test(expectedExceptions=NullPointerException.class)
	public void testUnreflectNullField() throws Throwable {
		MethodHandles.lookup().unreflectVarHandle(null);
	}
	
	/**
	 * Attempt to invoke VarHandle operation using reflection.
	 */
	@Test(expectedExceptions=InvocationTargetException.class)
	public void testReflectJNIMethods() throws Throwable {
		VarHandle vh = MethodHandles.lookup().findVarHandle(InstanceHelper.class, "l", String.class);
		Method m = VarHandle.class.getMethod("get", Object[].class);
		m.invoke(vh, new Object[] {null});
	}
}
