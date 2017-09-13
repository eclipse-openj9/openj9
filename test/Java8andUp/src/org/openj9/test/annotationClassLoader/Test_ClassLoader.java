package org.openj9.test.annotationClassLoader;

/*******************************************************************************
 * Copyright (c) 2010, 2017 IBM Corp. and others
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
import org.testng.annotations.Test;
import org.testng.Assert;
import java.net.URL;
import java.net.URLClassLoader;

@Test(groups = { "level.sanity" })
public class Test_ClassLoader {

	@Test
	public void test_classloaders() {

		URL url = getClass().getProtectionDomain().getCodeSource().getLocation();
		// loads Annotation2 and HelperClass1
		ClassLoader loader0 = new URLClassLoader(new URL[] { url }, null) {
			protected Class loadClass(String className, boolean resolve) throws ClassNotFoundException {
				if (className.equals("org.openj9.test.annotationClassLoader.Annotation1"))
					throw new ClassNotFoundException(className);
				return super.loadClass(className, resolve);
			}
		};
		// loads Annotation1
		ClassLoader loader1 = new URLClassLoader(new URL[] { url }, loader0) {
			protected Class loadClass(String className, boolean resolve) throws ClassNotFoundException {
				if (className.equals("org.openj9.test.annotationClassLoader.AnnotationClass")
						|| className.equals("org.openj9.test.annotationClassLoader.HelperClass2"))
					throw new ClassNotFoundException(className);
				return super.loadClass(className, resolve);
			}
		};
		// loads AnnotationClass and HelperClass2
		ClassLoader loader2 = new URLClassLoader(new URL[] { url }, loader1) {
			protected Class loadClass(String className, boolean resolve) throws ClassNotFoundException {
				if (className.equals("org.openj9.test.annotationClassLoader.Annotation2"))
					throw new ClassNotFoundException(className);
				return super.loadClass(className, resolve);
			}
		};
		try {
			Class testClass = Class.forName("org.openj9.test.annotationClassLoader.AnnotationClass", false, loader2);
			((Runnable)testClass.newInstance()).run();
		} catch (ClassNotFoundException e) {
			Assert.fail("unexpected: " + e);
		} catch (IllegalAccessException e) {
			Assert.fail("unexpected: " + e);
		} catch (InstantiationException e) {
			Assert.fail("unexpected: " + e);
		}
	}

}
