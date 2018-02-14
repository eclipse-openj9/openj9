/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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

package org.openj9.test.valhalla;

import java.io.FileOutputStream;
import java.lang.reflect.*;
import java.lang.annotation.Annotation;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ClassLoader;
import java.lang.invoke.*;
import org.objectweb.asm.*;
import org.testng.annotations.*;
import org.testng.Assert;
import org.testng.AssertJUnit;
import org.testng.annotations.Test;

@Test(groups = { "level.sanity" })
public class NestAttributeTest {
	private static CustomClassLoader classloader = new CustomClassLoader();
	private static CustomClassLoader classloader2 = new CustomClassLoader();

	@Test
	static public void fieldAccess() throws Exception {
		byte[] bytes = ClassGenerator.fieldAccessDump();
		byte[] bytesInner = ClassGenerator.fieldAccess$InnerDump();
		Class<?> clazz = classloader.getClass("FieldAccess", bytes);
		Class<?> clazzInner = classloader.getClass("FieldAccess$Inner", bytes);
		Object instance = clazz.getDeclaredConstructor().newInstance();
		Method bar = clazz.getDeclaredMethod("foo");
		Assert.assertEquals(3, bar.invoke(instance));
	}
	
	@Test
	static public void methodAccess() throws Exception {
		byte[] bytes = ClassGenerator.methodAccessDump();
		byte[] bytesInner = ClassGenerator.methodAcccess$InnerDump();
		Class<?> clazz = classloader.getClass("MethodAccess", bytes);
		Class<?> clazzInner = classloader.getClass("MethodAccess$Inner", bytes);
		Object instance = clazz.getDeclaredConstructor().newInstance();
		Method bar = clazz.getDeclaredMethod("bar");
		Assert.assertEquals(3, bar.invoke(instance));
	}
	
	@Test(expectedExceptions = java.lang.ClassFormatError.class)
	static public void testMultipleNestMembersAttributes() throws Exception {
		byte[] bytes = ClassGenerator.MultipleNestMembersAttributesdump();
		Class<?> clazz = classloader.getClass("MultipleNestMembersAttributes", bytes);
		Object instance = clazz.getDeclaredConstructor().newInstance();
	}
	
	@Test(expectedExceptions = java.lang.ClassFormatError.class)
	static public void testMultipleMemberOfNestAttributes() throws Exception {
		byte[] bytes = ClassGenerator.MultipleNestHostAttributesdump();
		Class<?> clazz = classloader.getClass("MultipleMemberOfNestAttributes", bytes);
		Object instance = clazz.getDeclaredConstructor().newInstance();
	}
	
	@Test(expectedExceptions = java.lang.ClassFormatError.class)
	static public void testMultipleNestAttributes() throws Exception {
		byte[] bytes = ClassGenerator.MultipleNestAttributesdump();
		Class<?> clazz = classloader.getClass("MultipleNestAttributes", bytes);
		Object instance = clazz.getDeclaredConstructor().newInstance();
	}
	
	@Test(expectedExceptions = java.lang.VerifyError.class)
	static public void testDifferentClassLoaders() throws Exception {
		byte[] bytes = ClassGenerator.fieldAccessDump();
		byte[] bytesInner = ClassGenerator.fieldAccess$InnerDump();
		Class<?> clazz = classloader.getClass("FieldAccess", bytes);
		Class<?> clazzInner = classloader2.getClass("FieldAccess$Inner", bytes);
	}
	
	@Test(expectedExceptions = java.lang.VerifyError.class)
	static public void testDifferentPackages() throws Exception {
		byte[] bytes = ClassGenerator.DifferentPackagedump();
		byte[] bytesInner = ClassGenerator.DifferentPackage$Innerdump();
		Class<?> testClass = classloader.getClass("TestClass", bytes);
		Class<?> testClass$Inner = classloader.getClass("p.TestClass$Inner", bytesInner);		
	}
	
	@Test(expectedExceptions = java.lang.VerifyError.class)
	static public void testNestMemberNotClaimed() throws Exception {
		byte[] bytes = ClassGenerator.nestTopNotClaimedDump();
		byte[] bytesInner = ClassGenerator.nestTopNotClaimed$InnerDump();
		Class<?> nestTopNotClaimed = classloader.getClass("NestTopNotClaimed", bytes);
		Class<?> bestTopNotClaimed = classloader2.getClass("NestTopNotClaimed$Inner", bytesInner);		
		Method bar = nestTopNotClaimed.getDeclaredMethod("bar");
		Object nestTopNotClaimedInstance = nestTopNotClaimed.getDeclaredConstructor().newInstance();
	}
	
	/* If a class claims a nest member, but the member class does not claim
	 * the nest top, both classes load & link without error - an error will
	 * only occur if/when another nest member attempts to access a private
	 * member of the class.
	 */
	@Test
	static public void testNestTopNotClaimedWithoutAccess() throws Exception {
		byte[] bytes = ClassGenerator.nestTopNotClaimedDump();
		byte[] bytesInner = ClassGenerator.nestTopNotClaimed$InnerDump();
		Class<?> nestTopNotClaimed = classloader.getClass("NestTopNotClaimed", bytes);
		Class<?> nestTopNotClaimed$Inner = classloader2.getClass("NestTopNotClaimed$Inner", bytesInner);		
		Method bar = nestTopNotClaimed.getDeclaredMethod("bar");
		Object nestTopNotClaimedInstance = nestTopNotClaimed.getDeclaredConstructor().newInstance();
	}
	
	@Test(expectedExceptions = java.lang.IllegalAccessException.class)
	static public void testNestTopNotClaimedWithAccess() throws Exception {
		byte[] bytes = ClassGenerator.nestTopNotClaimedDump();
		byte[] bytesInner = ClassGenerator.nestTopNotClaimed$InnerDump();
		Class<?> nestTopNotClaimed = classloader.getClass("NestTopNotClaimed", bytes);
		Class<?> nestTopNotClaimed$Inner = classloader2.getClass("NestTopNotClaimed$Inner", bytesInner);		
		Method bar = nestTopNotClaimed.getDeclaredMethod("bar");
		Object nestTopNotClaimedInstance = nestTopNotClaimed.getDeclaredConstructor().newInstance();
		Assert.assertEquals(3, bar.invoke(nestTopNotClaimedInstance));
	}
}
