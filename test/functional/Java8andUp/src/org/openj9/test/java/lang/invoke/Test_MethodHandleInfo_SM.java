package org.openj9.test.java.lang.invoke;

/*
 * Copyright IBM Corp. and others 2022
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

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.invoke.*;
import java.lang.reflect.*;
import java.security.AccessControlException;
import org.openj9.test.java.lang.invoke.helpers.*;
import org.openj9.test.util.VersionCheck;
import static java.lang.invoke.MethodHandles.*;

public class Test_MethodHandleInfo_SM {
	public String publicField = "publicField";

	/**
	 * Check that a SecurityException is thrown when ...
	 *
	 * @throws Throwable
	 */
	@Test(groups = { "level.sanity" })
	public void test_RevealDirect_Security() throws Throwable {
		if (VersionCheck.major() >= 15) {
			/* This test doesn't apply to JDK15+ after removing package check for MethodHandles.Lookup() constructors exception three APIs */
			return;
		}

		final String lookup = "org.openj9.test.java.lang.invoke.Helper_MethodHandleInfo";
		final String methodHandle = "org.openj9.test.java.lang.invoke.helpers.Helper_MethodHandleInfoOtherPackagePublic";

		// Get lookup object from class loader L1
		ClassLoader l1 = new ClassLoader(Test_MethodHandleInfo_SM.class.getClassLoader()) {

			@Override
			public Class<?> loadClass(String className) throws ClassNotFoundException {
				if (className.equals(lookup)) {
					return getClass(className);
				}
				if (className.equals(methodHandle)) {
					return getClass(className);
				}
				return super.loadClass(className);
			}

			private Class<?> getClass(String className)throws ClassNotFoundException {
				String classFile = className.replace('.', '/') + ".class";
				try {
					InputStream classStream = getClass().getClassLoader().getResourceAsStream(classFile);
					if (classStream == null) {
						throw new ClassNotFoundException("Error loading class : " + classFile);
					}
			        int size = classStream.available();
			        byte classRep[] = new byte[size];
			        DataInputStream in = new DataInputStream(classStream);
			        in.readFully(classRep);
			        in.close();
			        Class<?> clazz = defineClass(className, classRep, 0, classRep.length);
					return clazz;
				} catch (IOException e) {
					throw new ClassNotFoundException(e.getMessage());
				}
			}

		};
		Class classL1 = l1.loadClass(lookup);
		Method mL1 = classL1.getDeclaredMethod("lookup");
		Lookup lookup1 = (Lookup)mL1.invoke(null);

		// Get MethodHandle from class loader L2
		ClassLoader l2 = new ClassLoader(Test_MethodHandleInfo_SM.class.getClassLoader()) {

			@Override
			public Class<?> loadClass(String className) throws ClassNotFoundException {
				if (className.equals(methodHandle)) {
					return getClass(className);
				}
				return super.loadClass(className);
			}

			private Class<?> getClass(String className)throws ClassNotFoundException {
				String classFile = className.replace('.', '/') + ".class";
				try {
					InputStream classStream = getClass().getClassLoader().getResourceAsStream(classFile);
					if (classStream == null) {
						throw new ClassNotFoundException("Error loading class : " + classFile);
					}
			        int size = classStream.available();
			        byte classRep[] = new byte[size];
			        DataInputStream in = new DataInputStream(classStream);
			        in.readFully(classRep);
			        in.close();
			        Class<?> clazz = defineClass(className, classRep, 0, classRep.length);
					return clazz;
				} catch (IOException e) {
					throw new ClassNotFoundException(e.getMessage());
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
		String s = (String)f.get(new Test_MethodHandleInfo_SM());
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

}
