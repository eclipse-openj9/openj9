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
package org.openj9.test.java.lang;

import org.testng.annotations.Test;
import org.openj9.test.support.resource.Support_Resources;
import org.testng.Assert;
import org.testng.AssertJUnit;

import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.Enumeration;
import java.io.File;
import java.io.IOException;

@Test(groups = { "level.sanity" })
public class Test_ClassLoader {

	static class MyClass {
		static {
			Object o = new MyClass2();
		}
	}

	static class MyClass2 {
	}

	public static class MyURLClassLoader extends URLClassLoader {
		private String classLoaderName;
		private ArrayList<String> visitedFindClassLoaders;
		private ArrayList<String> visitedGetClassLoaders;

		public MyURLClassLoader(URL[] urls, ClassLoader parent, ArrayList<String> visitedFindClassLoaders,
				ArrayList<String> visitedGetClassLoaders) {
			super(urls, parent);
			this.visitedFindClassLoaders = visitedFindClassLoaders;
			this.visitedGetClassLoaders = visitedGetClassLoaders;
		}

		public Enumeration<URL> findResources(String resName) throws IOException {
			visitedFindClassLoaders.add(getName());
			return super.findResources(resName);
		}

		public Enumeration<URL> getResources(String resName) throws IOException {
			visitedGetClassLoaders.add(getName());
			return super.getResources(resName);
		}

		public void setName(String name) {
			classLoaderName = name;
		}

		public String getName() {
			return classLoaderName;
		}

	}

	/**
	 * @tests java.lang.ClassLoader#getResource(java.lang.String)
	 */
	@Test
	public void test_getResource() {
		// Test for method java.net.URL
		// java.lang.ClassLoader.getResource(java.lang.String)
		java.net.URL u = ClassLoader.getSystemClassLoader().getResource("org/openj9/resources/openj9tr_Foo.c");
		AssertJUnit.assertTrue("Unable to find resource", u != null);
		java.io.InputStream is = null;
		try {
			is = u.openStream();
			AssertJUnit.assertTrue("Resource returned is invalid", is != null);
			is.close();
		} catch (java.io.IOException e) {
			AssertJUnit.assertTrue("IOException getting stream for resource", false);
		}
	}

	/**
	 * @tests java.lang.ClassLoader#getResource(java.lang.String)
	 */
	@Test
	public void test_getResource2() {
		try {
			ClassLoader.getSystemClassLoader().getResource(null);
			Assert.fail("should throw NPE");
		} catch (NullPointerException e) {
			// expected
		}
	}

	/**
	 * @tests java.lang.ClassLoader#getResources(java.lang.String)
	 *
	 * @requiredClass org.openj9.test.support.resource.Support_Resources
	 * @requiredResource org/openj9/resources/openj9tr_child.jar
	 * @requiredResource org/openj9/resources/openj9tr_parent.jar
	 * @requiredResource org/openj9/resources/openj9tr_general.jar
	 *
	 */
	@Test
	public void test_getResources() {
		String resName = "URLClassLoader_testResource.txt";

		File resources = Support_Resources.createTempFolder();
		Support_Resources.copyFile(resources, null, "openj9tr_parent.jar");
		Support_Resources.copyFile(resources, null, "openj9tr_child.jar");
		Support_Resources.copyFile(resources, null, "openj9tr_general.jar");

		try {
			ArrayList<String> visitedFindClassLoaders = new ArrayList<String>();
			ArrayList<String> visitedGetClassLoaders = new ArrayList<String>();

			File file = new File(resources.toString() + "/openj9tr_parent.jar");
			java.net.URL parentURL = new java.net.URL("file:" + file.getPath());
			MyURLClassLoader parentCL = new MyURLClassLoader(new java.net.URL[] { parentURL }, null,
					visitedFindClassLoaders, visitedGetClassLoaders);
			parentCL.setName("parent");

			file = new File(resources.toString() + "/openj9tr_general.jar");
			java.net.URL childURL = new java.net.URL("file:" + file.getPath());
			MyURLClassLoader childCL = new MyURLClassLoader(new java.net.URL[] { childURL }, parentCL,
					visitedFindClassLoaders, visitedGetClassLoaders);
			childCL.setName("child");

			file = new File(resources.toString() + "/openj9tr_child.jar");
			java.net.URL grandChildURL = new java.net.URL("file:" + file.getPath());
			MyURLClassLoader grandChildCL = new MyURLClassLoader(new java.net.URL[] { grandChildURL }, childCL,
					visitedFindClassLoaders, visitedGetClassLoaders);
			grandChildCL.setName("grandChild");

			Enumeration<URL> resourcesEnum = grandChildCL.getResources(resName);
			/*
			 * make sure that parent ClassLoader's findResources method is
			 * called first, then the child's one
			 */
			if (3 != visitedFindClassLoaders.size()) {
				Assert.fail("Unexpected number of findResources method calls. Expected 3 calls, "
						+ visitedFindClassLoaders.size()
						+ " actual from parent class loader, child class loader and grandchild class loader.");
			}

			if (3 != visitedGetClassLoaders.size()) {
				Assert.fail("Unexpected number of getResources method calls. Expected 3 calls, "
						+ visitedGetClassLoaders.size()
						+ " actual from parent class loader, child class loader and grandchild class loader.");
			}

			if (!visitedFindClassLoaders.get(0).equals("parent")) {
				Assert.fail("First call to findResources is expected from parent class loader. But it is done from "
						+ visitedFindClassLoaders.get(0));
			}

			if (!visitedFindClassLoaders.get(1).equals("child")) {
				Assert.fail("Second call to findResources is expected from child class loader. But it is done from "
						+ visitedFindClassLoaders.get(1));
			}

			if (!visitedFindClassLoaders.get(2).equals("grandChild")) {
				Assert.fail("Third call to findResources is expected from grandChild class loader. But it is done from "
						+ visitedFindClassLoaders.get(2));
			}

			if (!visitedGetClassLoaders.get(0).equals("grandChild")) {
				Assert.fail("First call to getResources is expected from grandChild class loader. But it is done from "
						+ visitedFindClassLoaders.get(0));
			}

			if (!visitedGetClassLoaders.get(1).equals("child")) {
				Assert.fail("Second call to getResources is expected from child class loader. But it is done from "
						+ visitedFindClassLoaders.get(1));
			}

			if (!visitedGetClassLoaders.get(2).equals("parent")) {
				Assert.fail("Third call to getResources is expected from parent class loader. But it is done from "
						+ visitedFindClassLoaders.get(2));
			}

			/*
			 * make sure there are 2 resource URLs in resourcesEnum and the
			 * parent one is the first one
			 */
			URL currentURL;
			if (!resourcesEnum.hasMoreElements()) {
				Assert.fail(
						"ClassLoader#getResources failed to find any of the existing two resources for resource name : "
								+ resName);
			} else {
				currentURL = resourcesEnum.nextElement();
				if (!currentURL.getPath().equals(parentURL + "!/" + resName)) {
					Assert.fail("Resource \'" + resName + "\' is expected to be found in the parent jar \'" + parentURL
							+ "\' first. But found at " + currentURL.getPath());
				} else {
					if (!resourcesEnum.hasMoreElements()) {
						Assert.fail(
								"ClassLoader#getResources failed to find existing second resource for resource name : "
										+ resName);
					} else {
						currentURL = resourcesEnum.nextElement();
						if (!currentURL.getPath().equals(grandChildURL + "!/" + resName)) {
							Assert.fail("Resource \'" + resName + "\' is expected to be found in the child jar \'"
									+ grandChildURL + "\'. But found at " + currentURL.getPath());
						} else {
							if (resourcesEnum.hasMoreElements()) {
								Assert.fail("Resource \'" + resName
										+ "\' is expected to be found at two different URLs. But it is found at more than two URLs.");
							}
						}
					}
				}
			}
		} catch (IOException e) {
			AssertJUnit.assertTrue("IOException occured while getting resources from class loader.", false);
		}
	}

	/**
	 * @tests java.lang.ClassLoader#getResourceAsStream(java.lang.String)
	 */
	@Test
	public void test_getResourceAsStream() {
		// Test for method java.io.InputStream
		// java.lang.ClassLoader.getResourceAsStream(java.lang.String)
		// BB Need better test

		java.io.InputStream is = null;
		AssertJUnit.assertTrue("Failed to find resource: org/openj9/resources/openj9tr_Foo.c", (is = ClassLoader
				.getSystemClassLoader().getResourceAsStream("org/openj9/resources/openj9tr_Foo.c")) != null);
		try {
			is.close();
		} catch (java.io.IOException e) {
			AssertJUnit.assertTrue("Exception during getResourceAsStream: " + e.toString(), false);
		}
	}

	/**
	 * @tests java.lang.ClassLoader#getResourceAsStream(java.lang.String)
	 */
	@Test
	public void test_getResourceAsStream2() {
		try {
			ClassLoader.getSystemClassLoader().getResourceAsStream(null);
			Assert.fail("should throw NPE");
		} catch (NullPointerException e) {
			// expected
		}
	}

	/**
	 * @tests java.lang.ClassLoader#getSystemClassLoader()
	 */
	@Test
	public void test_getSystemClassLoader() {
		// Test for method java.lang.ClassLoader
		// java.lang.ClassLoader.getSystemClassLoader()
		ClassLoader cl = ClassLoader.getSystemClassLoader();
		AssertJUnit.assertTrue(getClass().getClassLoader() == cl);
		java.io.InputStream is = cl.getResourceAsStream("org/openj9/resources/openj9tr_Foo.c");
		AssertJUnit.assertTrue("Failed to find resource from system classpath", is != null);
		try {
			is.close();
		} catch (java.io.IOException e) {
		}

	}

	/**
	 * @tests java.lang.ClassLoader#getSystemResource(java.lang.String)
	 */
	@Test
	public void test_getSystemResource() {
		// Test for method java.net.URL
		// java.lang.ClassLoader.getSystemResource(java.lang.String)
		// BB Need better test
		AssertJUnit.assertTrue("Failed to find resource: org/openj9/resources/openj9tr_Foo.c",
				ClassLoader.getSystemResource("org/openj9/resources/openj9tr_Foo.c") != null);
	}

	/**
	 * @tests java.lang.ClassLoader#findLoadedClass(java.lang.String)
	 */
	@Test
	public void test_findLoadedClass1() {
		// although loadClass() is used, we are testing the findLoadedClass()
		// behavior
		ClassLoader loader = getClass().getClassLoader();
		String name = getClass().getName().replace('.', '/');
		try {
			// findLoadedClass() should not find this class using '/' instead of
			// '.'
			loader.loadClass(name);
			Assert.fail("load failure expected");
		} catch (NoClassDefFoundError e) {
			// expected, defineClass() throws this because the name is wrong
		} catch (ClassNotFoundException e) {
			Assert.fail("unexpected: " + e);
		}
	}

	/**
	 * @tests java.lang.ClassLoader#findLoadedClass(java.lang.String)
	 */
	@Test
	public void test_findLoadedClass2() {
		// although loadClass() is used, we are testing the findLoadedClass()
		// behavior
		ClassLoader loader = getClass().getClassLoader();
		try {
			// the array class has not been created, and should not be found
			loader.loadClass("[L" + getClass().getName() + ";");
			Assert.fail("array class should not be found");
		} catch (ClassNotFoundException e) {
			// expected
		}

		// create the array class
		Test_ClassLoader[] arrayClass = new Test_ClassLoader[0];
		try {
			loader.loadClass("[L" + getClass().getName() + ";");
			Assert.fail("array class should not be found after creation");
		} catch (ClassNotFoundException e) {
			// expected
		}
	}

	/**
	 * @tests java.lang.ClassLoader#loadClass(java.lang.String)
	 */
	@Test
	public void test_loadClass() {
		try {
			ClassLoader.getSystemClassLoader().loadClass("[Ljava.lang.String;");
			Assert.fail("The array class should not be loaded.");
		} catch (ClassNotFoundException ex) {
			// expected
		}
	}

	/**
	 * @tests java.lang.ClassLoader#defineClass(java.lang.String, byte[], int,
	 *        int, java.security.ProtectionDomain)
	 */
	@Test
	public void test_defineClass() {
		class TestClassLoader extends ClassLoader {
			public Class defineClass1(String s, byte[] ar, int i1, int i2) throws ClassFormatError {
				return super.defineClass(s, ar, i1, i2);
			}
		}

		TestClassLoader cl = new TestClassLoader();
		byte[] array0 = new byte[] { 2, 2, 2, 2, 2, 2, 2 };
		try {
			cl.defineClass1("0", array0, 4, -2);
			Assert.fail("expected exception");
		} catch (ArrayIndexOutOfBoundsException e) {
			// expected
		}
	}

	static class MyClassLoader extends ClassLoader {
		static {
			registerAsParallelCapable();
		}

		public Object myGetClassLoadingLock(String className) {
			return getClassLoadingLock(className);
		}
	}

	/**
	 * @tests java.lang.ClassLoader#loadClass(java.lang.String, boolean)
	 */
	@Test
	public void test_loadClassInParallel() {
		MyClassLoader mcl = new MyClassLoader();
		loadClassInParallelHelper(mcl);
		for (int i = 0; i < 2; i++) {
			System.gc();
		}
		loadClassInParallelHelper(mcl);
	}

	void loadClassInParallelHelper(MyClassLoader mcl) {
		final String CLASSNAME = "classNotExist";
		Object lock1 = mcl.myGetClassLoadingLock(CLASSNAME);
		System.out.println(lock1);
		try {
			mcl.loadClass(CLASSNAME);
		} catch (ClassNotFoundException e) {
		}
		Object lock2 = mcl.myGetClassLoadingLock(CLASSNAME);
		System.out.println(lock2);
		if (!lock1.equals(lock2)) {
			Assert.fail("FAILED: getClassLoadingLock() returns differernt lock objects for same class name!");
		}
	}

	static class LoaderSubclass extends URLClassLoader {
		public LoaderSubclass() {
			super(new URL[0]);
		}

		public Class checkLoadedClass(String className) {
			return findLoadedClass(className);
		}

		public void loadLibrary(String libName) {
			System.loadLibrary(libName);
		}
	}

	/**
	 * /* [PR CMVC 195528] Delayed initialization of native structs for
	 * ClassLoaders
	 * 
	 * @tests java.lang.ClassLoader#findLoadedClass(java.lang.String)
	 * @tests java.lang.System#loadLibrary(java.lang.String)
	 * @tests java.lang.Class#forName(java.lang.String, boolean,
	 *        java.lang.ClassLoader)
	 */
	@Test
	public void test_callNativesOnNewClassLoaders() {
		// Ensure calling these natives on a new ClassLoader() does not crash

		LoaderSubclass loader1 = new LoaderSubclass();
		/*
		 * This native will not allocate any native structures. It should not
		 * find any class since nothing is loaded in this new ClassLoader.
		 */
		Class cl1 = loader1.checkLoadedClass("java.lang.Object");
		AssertJUnit.assertTrue("did not expect to find Object", cl1 == null);

		/* Check for one of the pre-loaded classes. */
		Class cl2 = loader1.checkLoadedClass("java.lang.OutOfMemoryError");
		AssertJUnit.assertTrue("did not expect to find OutOfMemoryError", cl2 == null);

		try {
			/*
			 * It should be fine to re-use a previous loader, as the previous
			 * tests should not have created any native structures, but create a
			 * new loader anyway in case something changes in the future.
			 */
			LoaderSubclass loader2 = new LoaderSubclass();
			/*
			 * This native will allocate native structures. This test is
			 * somewhat fragile. Load an existing library which exists but is
			 * not already loaded by another classloader. If it starts failing,
			 * find another library to load.
			 */
			loader2.loadLibrary("unpack");
		} catch (UnsatisfiedLinkError e) {
			e.printStackTrace();
			Assert.fail("expected to find library");
		}

		LoaderSubclass loader3 = new LoaderSubclass();
		try {
			// this native will allocate native structures
			Class cl = Class.forName("java.lang.Object", false, loader3);
			AssertJUnit.assertTrue("expected to find Object", cl == Object.class);
		} catch (ClassNotFoundException e) {
			e.printStackTrace();
			Assert.fail("expected to find Object, not " + e);
		}
	}

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
