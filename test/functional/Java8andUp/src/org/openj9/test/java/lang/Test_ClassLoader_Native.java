/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

import java.net.URL;
import java.net.URLClassLoader;

@Test(groups = { "level.sanity" })
public class Test_ClassLoader_Native {

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
			loader2.loadLibrary("j9ben");
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
}
