package org.openj9.test.annotationPackage;

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
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.openj9.test.support.resource.Support_Resources;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.net.URL;
import java.net.URLClassLoader;
import javax.xml.bind.annotation.XmlSchema;

@Test(groups = { "level.sanity" })
public class Test_PackageAnnotation2 {

	static Logger logger = Logger.getLogger(Test_PackageAnnotation2.class);

	@Test
	public void test_isAnnotationPresent() {
		try {
			String url1 = Support_Resources.getURL("openj9tr_annotationPackage_A.jar");
			String url2 = Support_Resources.getURL("openj9tr_annotationPackage_B.jar");

			URLClassLoader cl1 = new URLClassLoader(new URL[] { new URL(url1) });
			URLClassLoader cl2 = new URLClassLoader(new URL[] { new URL(url2) }, cl1);
			Class c = cl2.loadClass("org.openj9.resources.annotationPackage.A");
			Package p = c.getPackage();
			logger.debug("isAnnotationPresent = " + p.isAnnotationPresent(XmlSchema.class));
			AssertJUnit.assertTrue("Annotation is not present in the package " + p.getName(),
					p.isAnnotationPresent(XmlSchema.class));
		} catch (Exception e) {
			Assert.fail("Following exception occured : " + e.getMessage());
		}
	}
}
