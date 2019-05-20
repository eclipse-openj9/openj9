/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2019 IBM Corp. and others
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
package com.ibm.java.diagnostics.utils.plugins;

import java.io.File;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.java.diagnostics.utils.DTFJContext;

/**
 * Locator class to hide the ASM classloading shimming that occurs when using 
 * the plugin manager, allowing this to be shared by different plugin architectures
 * rather than just the DTFJ based ones.
 * 
 * @author adam
 */
@SuppressWarnings("nls")
public class PluginManagerLocator {

	private static final URLClassLoader asmExtendedClassLoader;
	private static final Logger logger = Logger.getLogger(PluginConstants.LOGGER_NAME);

	static {
		URL[] urls = getASMURLs();

		asmExtendedClassLoader = new PackageFilteredClassloader(urls, DTFJContext.class.getClassLoader());
	}

	public static PluginManager getManager() {
		try {
			// Have to use reflection here so that the DTFJPluginClassloader is loaded using the custom classloader,
			// which will also load its dependencies from asm-3.1.jar, which is not on the application classpath
			Class<?> loaderClass = Class.forName("com.ibm.java.diagnostics.utils.plugins.impl.PluginManagerImpl", true, asmExtendedClassLoader);
			Method method = loaderClass.getDeclaredMethod("getPluginManager", (Class[]) null);
			return (PluginManager) method.invoke(null, (Object[]) null);
		} catch (Exception e) { // many reflection related exceptions
			logger.log(Level.FINE, "Problem initialising DTFJ plugin classloader: " + e.getMessage());
			logger.log(Level.FINEST, "Problem initialising DTFJ plugin classloader: ", e);
			return null;
		}
	}

	private static URL[] getASMURLs() {
		if (System.getProperty(PluginConstants.PACKAGE_FILTER_DISABLE) != null) {
			// DTFJ may already be on the classpath and using the SDK shipped one is not desired as
			// the jar may be a later version. Setting this property to any value will disable the
			// use of the SDK jar.
			return new URL[0];
		}

		List<File> files = new ArrayList<>();
		String javaHome = System.getProperty("java.home", "");
		List<URL> urls = new ArrayList<>();

		files.add(new File(javaHome, "lib/ext/dtfj.jar"));

		for (File file : files) {
			try {
				urls.add(file.toURI().toURL());
			} catch (MalformedURLException e) {
				logger.log(Level.FINE, "Problem locating necessary libraries: " + e.getMessage());
				logger.log(Level.FINEST, "Problem locating necessary libraries: ", e);
			}
		}

		return urls.toArray(new URL[urls.size()]);
	}

}
