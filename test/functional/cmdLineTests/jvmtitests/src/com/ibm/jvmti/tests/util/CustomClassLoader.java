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
package com.ibm.jvmti.tests.util;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.InputStream;
import java.lang.reflect.Method;
import java.net.URI;
import java.net.URL;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;

/**
 * A custom class loader that uses Shared Classes, if available, to load the classes.
 * Classes to be loaded by this loader are generally available in the class path.
 * Therefore, if we pass the actual class name to this class loader, it won't be used at all,
 * as the application class loader would load them.
 * To workaround this issue, CustomClassLoader.loadClass() is passed the class name without the version of the class.
 * As a result, application classloader fails to load this class.
 * Version of the class is passed separately to CustomClassLoader's constructor.
 * CustomClassLoader.findClass() then combines the class name with its version to get the actual class name. 
 */
public class CustomClassLoader extends ClassLoader {
	private String classVersion;
	private Object scURLHelper;
	private Method findSharedClassMethod, storeSharedClassMethod;
	
	/**
	 * Determines if we are running under J9VM with -Xshareclasses option. 
	 * 
	 * @param classVersion - version of the class to be loaded. Used by findClass() to create actual class name.
	 */
	public CustomClassLoader(String classVersion) {
		this.classVersion = classVersion;
		try {
			/* If we can locate com.ibm.oti.shared.Shared then we are running with J9VM */
			Class sharedClass = Class.forName("com.ibm.oti.shared.Shared");
			Method isSharingEnabledMethod = sharedClass.getMethod("isSharingEnabled", (Class[])null);
			boolean sharingEnabled = (Boolean)isSharingEnabledMethod.invoke(null, (Object [])null);
			if (sharingEnabled) {
				Method getSCHelperFactorMethod = sharedClass.getMethod("getSharedClassHelperFactory", (Class[])null);
				Object scHelperFactory =  getSCHelperFactorMethod.invoke(null, (Object [])null);
				Method getURLHelperMethod = scHelperFactory.getClass().getMethod("getURLHelper", new Class[] { this.getClass() });
				scURLHelper = getURLHelperMethod.invoke(scHelperFactory, new Object[] { this });
				findSharedClassMethod = scURLHelper.getClass().getMethod("findSharedClass", new Class[] { URL.class, String.class });
				storeSharedClassMethod = scURLHelper.getClass().getMethod("storeSharedClass", new Class[] { URL.class, Class.class });
			} else {
				scURLHelper = null;
				findSharedClassMethod = null;
				storeSharedClassMethod = null;
			}
		} catch (Exception e) {
			scURLHelper = null;
			findSharedClassMethod = null;
			storeSharedClassMethod = null;
		}
	}
	
	/**
	 * @param name - Name of the class to be loaded. Should not include the class version.
	 * @see java.lang.ClassLoader#findClass(java.lang.String)
	 */
	protected Class findClass(String name) {
		byte[] classBytes = null;
		boolean storeClass = false;
		Class customClass;
		try {
			String fullName = name.replace('.', '/') + classVersion + ".class";
			URL url = getClass().getProtectionDomain().getCodeSource().getLocation();
			if (null != scURLHelper) {
				classBytes = (byte[])findSharedClassMethod.invoke(scURLHelper, new Object[] { url, name + classVersion });
			}
			if (null == classBytes) {
				if (url.getFile().toLowerCase().endsWith(".jar")) {
					JarFile jar = new JarFile(new File(new URI(url.toExternalForm())));
					JarEntry entry = jar.getJarEntry(fullName);
					InputStream in = jar.getInputStream(entry);
					ByteArrayOutputStream out = new ByteArrayOutputStream();
					int b;
					while ((b = in.read()) != -1) {
						out.write(b);
					}
					in.close();
					classBytes = out.toByteArray();
				} else {
					classBytes = Util.readFile(url.getPath() + "/" + fullName);
				}
				if (null != scURLHelper) {
					storeClass = true;
				}
			}
			if (classBytes == null) {
				return null;
			}
			customClass = defineClass(name + classVersion, classBytes, 0, classBytes.length);
			if ((true == storeClass) && (null != customClass)) {
				storeSharedClassMethod.invoke(scURLHelper, new Object[] { url, customClass });
			}
			return customClass;
		} catch (Exception e) {
			e.printStackTrace();
			return null;
		}
	}
}
