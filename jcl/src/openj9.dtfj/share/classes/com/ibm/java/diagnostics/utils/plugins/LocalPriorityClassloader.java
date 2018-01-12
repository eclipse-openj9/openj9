/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2017 IBM Corp. and others
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

import java.net.URL;
import java.net.URLClassLoader;

/**
 * Classloader to search for classes locally first before delegating to the parent classloader.
 * This 'me first' approach is required so that when classes are 'refreshed' and this loader
 * discarded the old versions on the plugin search path are also discarded.
 * 
 * @author adam
 *
 */
public class LocalPriorityClassloader extends URLClassLoader {
	
	public LocalPriorityClassloader(URL[] urls, ClassLoader parent) {
		super(urls, parent);
	}

	public LocalPriorityClassloader(URL[] urls) {
		super(urls);
	}

	/* (non-Javadoc)
	 * @see java.lang.ClassLoader#loadClass(java.lang.String, boolean)
	 * 
	 * Override the delegating model normally provided by this classloader.
	 */
	@Override
	protected synchronized Class<?> loadClass(String name, boolean resolve)	throws ClassNotFoundException {
		Class<?> clazz = findLoadedClass(name);			//see if class loaded already
		if(clazz != null) {
			return clazz;
		}
		try {
			clazz = findClass(name);					//not loaded so check locally first
			return clazz;
		} catch (ClassNotFoundException e) {
			//not found so delegate to parent
			return getParent().loadClass(name);
		}
	}

}
