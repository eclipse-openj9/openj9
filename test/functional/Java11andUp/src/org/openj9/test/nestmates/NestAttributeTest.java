/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

package org.openj9.test.nestmates;

import java.lang.reflect.*;
import org.objectweb.asm.*;
import org.testng.Assert;
import org.testng.annotations.Test;

@Test(groups = { "level.sanity" })
public class NestAttributeTest {
	
	/*
	 * A class may have no more than one nest members attribute.
	 */
	@Test(expectedExceptions = java.lang.ClassFormatError.class)
	static public void testMultipleNestMembersAttributes() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] bytes = ClassGenerator.MultipleNestMembersAttributesdump();
		Class<?> clazz = classloader.getClass("MultipleNestMembersAttributes", bytes);
		try {
			Object instance = clazz.getDeclaredConstructor().newInstance();
		} catch (InvocationTargetException e) {
			throw e.getCause();
		}
	}
	
	@Test(expectedExceptions = java.lang.ClassFormatError.class)
	static public void testMultipleNestHostAttributes() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] bytes = ClassGenerator.MultipleNestHostAttributesdump();
		try {
			Class<?> clazz = classloader.getClass("MultipleNestHostAttributes", bytes);
			Object instance = clazz.getDeclaredConstructor().newInstance();
		} catch (InvocationTargetException e) {
			throw e.getCause();
		}
	}
	
	@Test(expectedExceptions = java.lang.ClassFormatError.class)
	static public void testMultipleNestAttributes() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] bytes = ClassGenerator.MultipleNestAttributesdump();
		try {
			Class<?> clazz = classloader.getClass("MultipleNestAttributes", bytes);
			Object instance = clazz.getDeclaredConstructor().newInstance();
		} catch (InvocationTargetException e) {
			throw e.getCause();
		}
	}
	
	/* 
	 * It is valid for a class to say that its nest host is itself or to claim
	 * itself within its own nest members list.
	 */
	@Test
	static public void testClassIsOwnNestHost() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] bytes = ClassGenerator.ClassIsOwnNestHostdump();
		try {
			Class<?> clazz = classloader.getClass("ClassIsOwnNestHost", bytes);
			Object instance = clazz.getDeclaredConstructor().newInstance();
		} catch (InvocationTargetException e) {
			throw e.getCause();
		}
	}
	
	@Test
	static public void testNestMemberIsItself() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] bytes = ClassGenerator.NestMemberIsItselfdump();
		try {
			Class<?> clazz = classloader.getClass("NestMemberIsItself", bytes);
			Object instance = clazz.getDeclaredConstructor().newInstance();
		} catch (InvocationTargetException e) {
			throw e.getCause();
		}
	}
	
	/*
	 * If a nest member and nest host are loaded on a different class loader
	 * or in a different package, they should load and run as normal until
	 * access between private members of the nest member and either the nest
	 * host or another nest member is attempted.
	 */
	@Test
	static public void testDifferentClassLoadersWithoutAccess() throws Throwable {
		CustomClassLoader classloader1 = new CustomClassLoader();
		CustomClassLoader classloader2 = new CustomClassLoader();
		byte[] bytes = ClassGenerator.fieldAccessDump();
		byte[] bytesInner = ClassGenerator.fieldAccess$InnerDump();
		Class<?> clazz = classloader1.getClass("FieldAccess", bytes);
		Class<?> clazzInner = classloader2.getClass("FieldAccess$Inner", bytesInner);
		Object instance = clazz.getDeclaredConstructor().newInstance();
		Method bar = clazz.getDeclaredMethod("foo");
	}
	
	@Test(expectedExceptions = java.lang.NoClassDefFoundError.class)
	static public void testDifferentClassLoadersWithAccess() throws Throwable {
		CustomClassLoader classloader1 = new CustomClassLoader();
		CustomClassLoader classloader2 = new CustomClassLoader();
		byte[] bytes = ClassGenerator.fieldAccessDump();
		byte[] bytesInner = ClassGenerator.fieldAccess$InnerDump();
		Class<?> clazz = classloader1.getClass("FieldAccess", bytes);
		Class<?> clazzInner = classloader2.getClass("FieldAccess$Inner", bytesInner);
		Object instance = clazz.getDeclaredConstructor().newInstance();
		Method bar = clazz.getDeclaredMethod("foo");
		try {
			Assert.assertEquals(3, bar.invoke(instance));
		} catch (InvocationTargetException e) {
			throw e.getCause();
		}
	}
	
	@Test
	static public void testDifferentPackagesWithoutAccess() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] bytes = ClassGenerator.fieldAccessDump_different_package();
		byte[] bytesInner = ClassGenerator.fieldAccess$InnerDump_different_package();
		Class<?> clazz = classloader.getClass("FieldAccess", bytes);
		Class<?> clazzInner = classloader.getClass("pkg.FieldAccess$Inner", bytesInner);
		Object instance = clazz.getDeclaredConstructor().newInstance();
		Method bar = clazz.getDeclaredMethod("foo");
	}

	@Test(expectedExceptions = java.lang.LinkageError.class)
	static public void testDifferentPackagesWithAccess() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] bytes = ClassGenerator.fieldAccessDump_different_package();
		byte[] bytesInner = ClassGenerator.fieldAccess$InnerDump_different_package();
		Class<?> clazz = classloader.getClass("FieldAccess", bytes);
		Class<?> clazzInner = classloader.getClass("pkg.FieldAccess$Inner", bytesInner);
		Object instance = clazz.getDeclaredConstructor().newInstance();
		Method bar = clazz.getDeclaredMethod("foo");
		try {
			Assert.assertEquals(3, bar.invoke(instance));
		} catch (InvocationTargetException e) {
			throw e.getCause();
		}
	}

	/*
	 * If there is incompatibility between two classes' nest definitions (eg. a
	 * nest host does not verify another class which claims it as a nest member,
	 * or a nest host verifies a nest member which does not claim it as a nest
	 * class), both will load and run as normal until private access is attempted
	 * between the two classes.
	 */
	
	@Test
	static public void testNestMemberNotVerifiedWithoutAccess() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] bytes = ClassGenerator.fieldAccessDump_nest_member_not_verified();
		byte[] bytesInner = ClassGenerator.fieldAccess$InnerDump_nest_member_not_verified();
		Class<?> clazz = classloader.getClass("FieldAccess", bytes);
		Class<?> clazzInner = classloader.getClass("FieldAccess$Inner", bytesInner);
		Object instance = clazz.getDeclaredConstructor().newInstance();
		Method bar = clazz.getDeclaredMethod("foo");
	}
	
	@Test(expectedExceptions = java.lang.LinkageError.class)
	static public void testNestMemberNotVerifiedWithAccess() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] bytes = ClassGenerator.fieldAccessDump_nest_member_not_verified();
		byte[] bytesInner = ClassGenerator.fieldAccess$InnerDump_nest_member_not_verified();
		Class<?> clazz = classloader.getClass("FieldAccess", bytes);
		Class<?> clazzInner = classloader.getClass("FieldAccess$Inner", bytesInner);
		Object instance = clazz.getDeclaredConstructor().newInstance();
		Method bar = clazz.getDeclaredMethod("foo");
		try {
			Assert.assertEquals(3, bar.invoke(instance));
		} catch (InvocationTargetException e) {
			throw e.getCause();
		}
	}
	
	@Test
	static public void testNesthostNotClaimedWithoutAccess() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] bytes = ClassGenerator.fieldAccessDump_nest_host_not_claimed();
		byte[] bytesInner = ClassGenerator.fieldAccess$InnerDump_nest_host_not_claimed();
		Class<?> clazz = classloader.getClass("FieldAccess", bytes);
		Class<?> clazzInner = classloader.getClass("FieldAccess$Inner", bytesInner);
		Object instance = clazz.getDeclaredConstructor().newInstance();
		Method bar = clazz.getDeclaredMethod("foo");
	}
	
	@Test(expectedExceptions = java.lang.IllegalAccessError.class)
	static public void testNesthostNotClaimedWithAccess() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] bytes = ClassGenerator.fieldAccessDump_nest_host_not_claimed();
		byte[] bytesInner = ClassGenerator.fieldAccess$InnerDump_nest_host_not_claimed();
		Class<?> clazz = classloader.getClass("FieldAccess", bytes);
		Class<?> clazzInner = classloader.getClass("FieldAccess$Inner", bytesInner);
		Object instance = clazz.getDeclaredConstructor().newInstance();
		Method bar = clazz.getDeclaredMethod("foo");
		try {
			Assert.assertEquals(3, bar.invoke(instance));
		} catch (InvocationTargetException e) {
			throw e.getCause();
		}
	}
	
	/*
	 * If two classes are in the same nest, then they should allow access to
	 * each other's private members - both fields and methods.
	 */
	@Test
	static public void testFieldAccessAllowed() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] bytes = ClassGenerator.fieldAccessDump();
		byte[] bytesInner = ClassGenerator.fieldAccess$InnerDump();
		Class<?> clazz = classloader.getClass("FieldAccess", bytes);
		Class<?> clazzInner = classloader.getClass("FieldAccess$Inner", bytesInner);
		Object instance = clazz.getDeclaredConstructor().newInstance();
		Method bar = clazz.getDeclaredMethod("foo");
		Assert.assertEquals(3, bar.invoke(instance));
	}
	
	@Test
	static public void testMethodAccessAllowed() throws Exception {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] bytes = ClassGenerator.methodAccessDump();
		byte[] bytesInner = ClassGenerator.methodAccess$InnerDump();
		Class<?> clazz = classloader.getClass("MethodAccess", bytes);
		Class<?> clazzInner = classloader.getClass("MethodAccess$Inner", bytesInner);
		Object instance = clazz.getDeclaredConstructor().newInstance();
		Method bar = clazz.getDeclaredMethod("bar");
		Assert.assertEquals(3, bar.invoke(instance));
	}
	
	@Test
	static public void testGetNestHostAPINestHostNotSet() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] aBytes = ClassGenerator.generateClass("test/A", null);
		Class<?> aClazz = classloader.getClass("test.A", aBytes);
		Assert.assertEquals(aClazz, aClazz.getNestHost());
	}
	
	@Test
	static public void testGetNestHostAPINestHostDoesntExist() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] aBytes = ClassGenerator.generateClass("test/A", new Attribute[] {new NestHostAttribute("test/NonExistent")});
		Class<?> aClazz = classloader.getClass("test.A", aBytes);
		Assert.assertEquals(aClazz, aClazz.getNestHost());
	}
	
	@Test
	static public void testGetNestHostAPINestHostSet() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] aBytes = ClassGenerator.generateClass("test/A", new Attribute[] {new NestHostAttribute("test/B")});
		Class<?> aClazz = classloader.getClass("test.A", aBytes);
		byte[] bBytes = ClassGenerator.generateClass("test/B", new Attribute[] {new NestMembersAttribute(new String[] {"test/A"})});
		Class<?> bClazz = classloader.getClass("test.B", bBytes);
		Assert.assertEquals(bClazz, aClazz.getNestHost());
	}
	
	@Test
	static public void testGetNestHostAPINestHostSet2() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] aBytes = ClassGenerator.generateClass("test/A", new Attribute[] {new NestHostAttribute("test/A")});
		Class<?> aClazz = classloader.getClass("test.A", aBytes);
		Assert.assertEquals(aClazz, aClazz.getNestHost());
	}
	
	@Test
	static public void testGetNestMembersAPINoMembers() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] aBytes = ClassGenerator.generateClass("test/A", null);
		Class<?> aClazz = classloader.getClass("test.A", aBytes);
		Class<?> nestMembers[] = aClazz.getNestMembers();
		Assert.assertEquals(1, nestMembers.length);
		Assert.assertEquals(aClazz, nestMembers[0]);
	}
	
	@Test
	static public void testGetNestMembersAPIValidMembers() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] aBytes = ClassGenerator.generateClass("test/A", new Attribute[] {new NestMembersAttribute(new String[] {"test/B", "test/C", "test/D"})});
		Class<?> aClazz = classloader.getClass("test.A", aBytes);
		byte[] bBytes = ClassGenerator.generateClass("test/B", new Attribute[] {new NestHostAttribute("test/A")});
		Class<?> bClazz = classloader.getClass("test.B", bBytes);
		byte[] cBytes = ClassGenerator.generateClass("test/C", new Attribute[] {new NestHostAttribute("test/A")});
		Class<?> cClazz = classloader.getClass("test.C", cBytes);
		byte[] dBytes = ClassGenerator.generateClass("test/D", new Attribute[] {new NestHostAttribute("test/A")});
		Class<?> dClazz = classloader.getClass("test.D", dBytes);
		
		Class<?> clazzList[] = {aClazz, bClazz, cClazz, dClazz};
		for (Class<?> clazz : clazzList) {
			Class<?> nestMembers[] = clazz.getNestMembers();
			
			Assert.assertEquals(4, nestMembers.length);
			Assert.assertEquals(aClazz, nestMembers[0]);
			
			for (int i = 1; i < nestMembers.length; i++)
			{
				Assert.assertTrue((nestMembers[i] == bClazz) || (nestMembers[i] == cClazz) || (nestMembers[i] == dClazz));
			}
		}
	}
	
	@Test
	static public void testGetNestMembersAPIInValidMembers() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] aBytes = ClassGenerator.generateClass("test/A", new Attribute[] {new NestMembersAttribute(new String[] {"test/B", "test/Invalid", "test/D"})});
		Class<?> aClazz = classloader.getClass("test.A", aBytes);
		byte[] bBytes = ClassGenerator.generateClass("test/B", new Attribute[] {new NestHostAttribute("test/A")});
		Class<?> bClazz = classloader.getClass("test.B", bBytes);
		byte[] dBytes = ClassGenerator.generateClass("test/D", new Attribute[] {new NestHostAttribute("test/A")});
		Class<?> dClazz = classloader.getClass("test.D", dBytes);
		
		Class<?> clazzList[] = {aClazz, bClazz, dClazz};
		for (Class<?> clazz : clazzList) {
			try {
				Class<?> nestMembers[] = clazz.getNestMembers();
				Assert.fail("should throw exception on getNestMembers");
			} catch (NoClassDefFoundError e) { }
		}
	}
	
	@Test
	static public void testGetNestMembersAPIInValidHost() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] bBytes = ClassGenerator.generateClass("test/B", new Attribute[] {new NestHostAttribute("test/Invalid")});
		Class<?> bClazz = classloader.getClass("test.B", bBytes);
		byte[] cBytes = ClassGenerator.generateClass("test/C", new Attribute[] {new NestHostAttribute("test/Invalid")});
		Class<?> cClazz = classloader.getClass("test.C", cBytes);
		byte[] dBytes = ClassGenerator.generateClass("test/D", new Attribute[] {new NestHostAttribute("test/InValid")});
		Class<?> dClazz = classloader.getClass("test.D", dBytes);
		
		Class<?> clazzList[] = {bClazz, cClazz, dClazz};
		for (Class<?> clazz : clazzList) {
			try {
				Class<?> nestMembers[] = clazz.getNestMembers();
				Assert.fail("should throw exception on getNestMembers");
			} catch (NoClassDefFoundError e) { }
		}
	}
	
	@Test
	static public void testGetNestMembersAPIInValidHostWrongPackage() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] aBytes = ClassGenerator.generateClass("test2/A", new Attribute[] {new NestMembersAttribute(new String[] {"test/B", "test/Invalid", "test/D"})});
		Class<?> aClazz = classloader.getClass("test2.A", aBytes);
		byte[] bBytes = ClassGenerator.generateClass("test/B", new Attribute[] {new NestHostAttribute("test2/A")});
		Class<?> bClazz = classloader.getClass("test.B", bBytes);
		byte[] cBytes = ClassGenerator.generateClass("test/C", new Attribute[] {new NestHostAttribute("test2/A")});
		Class<?> cClazz = classloader.getClass("test.C", cBytes);
		byte[] dBytes = ClassGenerator.generateClass("test/D", new Attribute[] {new NestHostAttribute("test2/A")});
		Class<?> dClazz = classloader.getClass("test.D", dBytes);
		
		Class<?> clazzList[] = {bClazz, cClazz, dClazz};
		for (Class<?> clazz : clazzList) {
			try {
				Class<?> nestMembers[] = clazz.getNestMembers();
				Assert.fail("should throw exception on getNestMembers");
			} catch (IncompatibleClassChangeError e) { }
		}
	}
	
	@Test
	static public void testGetNestMembersAPINestMemberWrongPackage() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] aBytes = ClassGenerator.generateClass("test/A", new Attribute[] {new NestMembersAttribute(new String[] {"test/B", "test/C", "test2/D"})});
		Class<?> aClazz = classloader.getClass("test.A", aBytes);
		byte[] bBytes = ClassGenerator.generateClass("test/B", new Attribute[] {new NestHostAttribute("test/A")});
		Class<?> bClazz = classloader.getClass("test.B", bBytes);
		byte[] cBytes = ClassGenerator.generateClass("test/C", new Attribute[] {new NestHostAttribute("test/A")});
		Class<?> cClazz = classloader.getClass("test.C", cBytes);
		byte[] dBytes = ClassGenerator.generateClass("test2/D", new Attribute[] {new NestHostAttribute("test/A")});
		Class<?> dClazz = classloader.getClass("test2.D", dBytes);
		
		Class<?> clazzList[] = {aClazz, bClazz, cClazz, dClazz};
		for (Class<?> clazz : clazzList) {
			try {
				Class<?> nestMembers[] = clazz.getNestMembers();
				Assert.fail("should throw exception on getNestMembers");
			} catch (IncompatibleClassChangeError e) { }
		}
	}
	
	@Test
	static public void testGetNestMembersAPINestMemberWrongNestHost() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] aBytes = ClassGenerator.generateClass("test/A", new Attribute[] {new NestMembersAttribute(new String[] {"test/B", "test/C"})});
		Class<?> aClazz = classloader.getClass("test.A", aBytes);
		byte[] bBytes = ClassGenerator.generateClass("test/B", new Attribute[] {new NestHostAttribute("test/A")});
		Class<?> bClazz = classloader.getClass("test.B", bBytes);
		byte[] cBytes = ClassGenerator.generateClass("test/C", new Attribute[] {new NestHostAttribute("test/B")});
		Class<?> cClazz = classloader.getClass("test.C", cBytes);
		
		Class<?> clazzList[] = {aClazz, bClazz, cClazz};
		for (Class<?> clazz : clazzList) {
			try {
				Class<?> nestMembers[] = clazz.getNestMembers();
				Assert.fail("should throw exception on getNestMembers");
			} catch (IncompatibleClassChangeError e) { }
		}	
	}
	
	@Test
	static public void testGetNestMembersAPINestMemberWrongNestHost2() throws Throwable {
		CustomClassLoader classloader = new CustomClassLoader();
		byte[] aBytes = ClassGenerator.generateClass("test/A", new Attribute[] {new NestMembersAttribute(new String[] {"test/C", "test/D"})});
		Class<?> aClazz = classloader.getClass("test.A", aBytes);
		byte[] bBytes = ClassGenerator.generateClass("test/B", new Attribute[] {new NestMembersAttribute(new String[] {"test/C"})});
		Class<?> bClazz = classloader.getClass("test.B", bBytes);
		byte[] cBytes = ClassGenerator.generateClass("test/C", new Attribute[] {new NestHostAttribute("test/B")});
		Class<?> cClazz = classloader.getClass("test.C", cBytes);
		byte[] dBytes = ClassGenerator.generateClass("test/D", new Attribute[] {new NestHostAttribute("test/A")});
		Class<?> dClazz = classloader.getClass("test.D", dBytes);
		
		Class<?> clazzList[] = {aClazz, dClazz};
		for (Class<?> clazz : clazzList) {
			try {
				Class<?> nestMembers[] = clazz.getNestMembers();
				Assert.fail("should throw exception on getNestMembers");
			} catch (IncompatibleClassChangeError e) { }
		}
		
		bClazz.getNestMembers();
		cClazz.getNestMembers();
		
	}
}
