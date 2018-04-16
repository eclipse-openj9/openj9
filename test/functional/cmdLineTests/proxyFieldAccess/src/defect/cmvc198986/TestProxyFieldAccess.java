/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package defect.cmvc198986;

import java.io.File;
import java.lang.ClassLoader;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;

public class TestProxyFieldAccess {

	public static void main(String[] args) {
		new TestProxyFieldAccess().test();
	}

	void test() {
		try {
			MyClassLoader mcl3 = new MyClassLoader(getClassPathURLs(getClass().getClassLoader()));

			Class<?> cls = mcl3.loadClass("defect.cmvc198986.ProxyFieldAccess");
			Method m = cls.getDeclaredMethod("main", String[].class);
			m.invoke(cls, new Object[] { new String[0] });

		} catch (Exception e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}

	private static URL[] getClassPathURLs(ClassLoader classLoader) {
		if (classLoader instanceof URLClassLoader) {
			return ((URLClassLoader)classLoader).getURLs();
		}

		/* Parse the java.class.path env var */
		String classpath = System.getProperty("java.class.path");
		String[] paths = classpath.split(File.pathSeparator);
		if (paths.length == 0) {
			return new URL[0];
		}

		ArrayList<URL> urls = new ArrayList<URL>(paths.length);
		for (String path : paths) {
			try {
				urls.add(new File(path).toURI().toURL());
			} catch (MalformedURLException e) {
				e.printStackTrace();
			}
		}
		return urls.toArray(new URL[0]);
	}

}
