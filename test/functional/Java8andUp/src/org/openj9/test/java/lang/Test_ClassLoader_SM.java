/*******************************************************************************
 * Copyright (c) 2022, 2022 IBM Corp. and others
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
package org.openj9.test.java.lang;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;

@Test(groups = { "level.sanity" })
public class Test_ClassLoader_SM {

	/**
	 * [PR Jazz103 76960]
	 *
	 * @tests java.lang.ClassLoader#getParent() Test the case when in
	 *        getParent() the callerClassLoader is same as child of this
	 *        classloader. getParent() should throw SecurityException in such
	 *        case.
	 */
	@Test
	public void test_getParent1() {
		ClassLoader cl = this.getClass().getClassLoader();
		ClassLoader parentCl = cl.getParent();

		SecurityManager manager = new SecurityManager();
		System.setSecurityManager(manager);
		try {
			parentCl.getParent();
			Assert.fail("expected the above call to getParent() to throw SecurityException");
		} catch (SecurityException e) {
			/* expected exception */
		} finally {
			System.setSecurityManager(null);
		}
	}

	/**
	 * [PR Jazz103 76960]
	 *
	 * @tests java.lang.ClassLoader#getParent() Test the case when in
	 *        getParent() the callerClassLoader is same as this classloader.
	 *        getParent() should throw SecurityException in such case.
	 */
	@Test
	public void test_getParent2() {
		ClassLoader cl = this.getClass().getClassLoader();
		SecurityManager manager = new SecurityManager();

		System.setSecurityManager(manager);
		try {
			cl.getParent();
			Assert.fail("expected the above call to getParent() to throw SecurityException");
		} catch (SecurityException e) {
			/* expected exception */
		} finally {
			System.setSecurityManager(null);
		}
	}

	/**
	 * @tests java.lang.ClassLoader#getParent() Test the case when in
	 *        getParent() the callerClassLoader is same as parent of this
	 *        classloader. This should cause the condition (callerClassLoader !=
	 *        requested) in ClassLoader.needsClassLoaderPermissionCheck() to
	 *        fail, thus preventing any security checks in getParent().
	 */
	@Test
	public void test_getParent3() {
		class ChildClassLoader extends ClassLoader {
			public ChildClassLoader(ClassLoader parent) {
				super(parent);
			}
		}

		ClassLoader cl = this.getClass().getClassLoader();
		ChildClassLoader childCL = new ChildClassLoader(cl);

		SecurityManager manager = new SecurityManager();
		System.setSecurityManager(manager);
		try {
			childCL.getParent();
		} catch (SecurityException e) {
			Assert.fail("unexpected exception: " + e);
		} finally {
			System.setSecurityManager(null);
		}
	}

	/**
	 * @tests java.lang.ClassLoader#getParent() Test the case when in
	 *        getParent() the callerClassLoader is same as grand parent of this
	 *        classloader (that is an ancestor of the parent of this
	 *        classloader) This should cause the condition
	 *        !callerClassLoader.isAncestorOf(requested) in
	 *        ClassLoader.needsClassLoaderPermissionCheck() to fail, thus
	 *        preventing any security checks in getParent().
	 */
	@Test
	public void test_getParent4() {
		class ChildClassLoader extends ClassLoader {
			public ChildClassLoader(ClassLoader parent) {
				super(parent);
			}
		}

		ClassLoader cl = this.getClass().getClassLoader();
		ChildClassLoader child = new ChildClassLoader(cl);
		ChildClassLoader grandChild = new ChildClassLoader(child);

		SecurityManager manager = new SecurityManager();
		System.setSecurityManager(manager);
		try {
			grandChild.getParent();
		} catch (SecurityException e) {
			Assert.fail("unexpected exception: " + e);
		} finally {
			System.setSecurityManager(null);
		}
	}

}
