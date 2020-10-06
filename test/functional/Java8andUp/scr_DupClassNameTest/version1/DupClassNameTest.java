/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

import java.io.File;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import org.testng.annotations.Test;
import org.testng.Assert;

/*
 * Test for this change: https://github.com/eclipse/openj9/pull/10687
 * There are 2 classes with the same name (DupClassNameTest), which are loaded by 2 different class loaders.
 * This file is under directory version1, the other version of DupClassNameTest is under directory version2.
 */
@Test(groups = { "level.sanity" })
public class DupClassNameTest {
	static Class<?> loadClassOfAnotherVersion(String resourceLocation) throws Throwable {
		File dir = new File(resourceLocation);
		Class<?> clazz = null;
		if (!dir.exists()) {
			Assert.fail(resourceLocation + " does not exist !");
		}
		URL[] url = new URL[] {dir.toURI().toURL()};
		URLClassLoader loader = new URLClassLoader(url, null);
		clazz = loader.loadClass("DupClassNameTest");
		return clazz;
		
	}
	/**
	 * Load another version of class DupClassNameTest under version2 directory 
	 * and run its printStackTrace() method.
	 *
	 * @throws Throwable
	 */
	@Test
	public static void runTest() throws Throwable {
		String resourceLocation = DupClassNameTest.class.getProtectionDomain().getCodeSource().getLocation().getPath();
		int index = resourceLocation.lastIndexOf("version1");
		StringBuilder sb = new StringBuilder();
		sb.append(resourceLocation.substring(0, index));
		sb.append("version2");
		resourceLocation = sb.toString();
		Class<?> clazz = loadClassOfAnotherVersion(resourceLocation);
		Method m = clazz.getMethod("getStackTrace", (Class[])null);
		m.invoke(null, (Object[])null);
	}
}
