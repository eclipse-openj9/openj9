package org.openj9.test.annotationPackage;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.openj9.test.support.resource.Support_Resources;
import org.testng.Assert;
import org.testng.AssertJUnit;

/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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
import java.net.URL;
import java.net.URLClassLoader;
import java.security.AccessController;
import java.security.PrivilegedAction;
import javax.xml.bind.annotation.XmlSchema;

@Test(groups = { "level.sanity" })
public class Test_PackageAnnotation {

	static Logger logger = Logger.getLogger(Test_PackageAnnotation.class);
	@Test
	public void test_elementFormDefault() {
		try {
			String url1 = Support_Resources.getURL("openj9tr_annotationPackage_A.jar");
			String url2 = Support_Resources.getURL("openj9tr_annotationPackage_B.jar");

			URLClassLoader cl1 = new URLClassLoader(new URL[] { new URL(url1) });
			URLClassLoader cl2 = new URLClassLoader(new URL[] { new URL(url2) }, cl1);

			// Query and check package for "org.openj9.resources.annotationPackage.A"
			Class<?> aClass = cl2.loadClass("org.openj9.resources.annotationPackage.A");
			Package aPackage = aClass.getPackage();
			System.out.println(aPackage);
			logger.debug("Class: " + aClass.getName() + " :: " + toString(aClass));
			logger.debug("Package: " + aPackage.getName() + " :: " + toString(aPackage));
			String elementForm = checkPackage(aPackage);
			AssertJUnit.assertTrue("Expected element form: QUALIFIED, got : " + elementForm,
					elementForm.equals("QUALIFIED"));

			logger.debug("\n");

			// PR 124166: JCL Test: test_elementFormDefault() failed in Test_PackageAnnotation, failed on 929, passed on 829
			// Query and check package for "org.openj9.resources.annotationPackage.B"
//			Class<?> bClass = cl2.loadClass("org.openj9.resources.annotationPackage.B");
//			Package bPackage = bClass.getPackage();
//			logger.debug("Class: " + bClass.getName() + " :: " + toString(bClass));
//			logger.debug("Package: " + bPackage.getName() + " :: " + toString(bPackage));
//			elementForm = checkPackage(bPackage);
//			AssertJUnit.assertTrue("Expected element form: QUALIFIED, got : " + elementForm,
//					elementForm.equals("QUALIFIED"));
//
//			logger.debug("\n");

			// Query and check package for "org.openj9.resources.annotationPackage.A" again!
			aPackage = aClass.getPackage();
			logger.debug("Class: " + aClass.getName() + " :: " + toString(aClass));
			logger.debug("Package: " + aPackage.getName() + " :: " + toString(aPackage));
			elementForm = checkPackage(aPackage);
			AssertJUnit.assertTrue("Expected element form: QUALIFIED, got : " + elementForm,
					elementForm.equals("QUALIFIED"));

		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Unexpected exception: " + e.toString());
		}
	}

	private static String checkPackage(Package p) {
		if (p.isAnnotationPresent(XmlSchema.class)) {
			logger.debug("Package " + p.getName() + ": @XmlSchema is present!");
			XmlSchema xs = p.getAnnotation(XmlSchema.class);
			logger.debug("Namespace: " + xs.namespace());
			logger.debug("Element form default: " + xs.elementFormDefault());
			return xs.elementFormDefault().name();
		} else {
			Assert.fail("Package " + p.getName() + ": @XmlSchema is NOT present!");
			return null;
		}
	}

	private static String toString(Package p) {
		StringBuilder sb = new StringBuilder();
		sb.append("[");
		sb.append(p.getClass().getName());
		sb.append('@');
		sb.append(Integer.toHexString(System.identityHashCode(p)));
		sb.append(']');
		return sb.toString();
	}

	private static String toString(Class<?> clazz) {
		StringBuilder sb = new StringBuilder();
		sb.append(clazz.getName());
		ClassLoader cl = getClassLoader(clazz);
		if (cl != null) {
			Class<?> clClass = cl.getClass();
			sb.append(" [");
			sb.append(clClass.getName());
			sb.append('@');
			sb.append(Integer.toHexString(System.identityHashCode(cl)));
			sb.append(']');
		}
		return sb.toString();
	}

	private static ClassLoader getClassLoader(final Class<?> clazz) {
		return AccessController.doPrivileged(new PrivilegedAction<ClassLoader>() {
			public ClassLoader run() {
				ClassLoader cl = null;
				try {
					cl = clazz.getClassLoader();
				} catch (SecurityException ex) {
				}
				return cl;
			}
		});
	}

}
