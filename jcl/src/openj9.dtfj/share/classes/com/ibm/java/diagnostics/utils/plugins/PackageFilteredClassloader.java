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
 * Classloader to deal with the fact that in order to be able to shim ASM
 * onto the extensions classpath, some DTFJ classes need to be resolved 
 * by this loader rather than the parent loader (which will be the ext 
 * loader). This is because the ext loader cannot see the ASM files.
 * 
 * @author adam
 *
 */
public class PackageFilteredClassloader extends URLClassLoader {
	private static final String PACKAGE_MASK = "com.ibm.java.diagnostics.utils.plugins.impl";
	private ClassLoader parent;
	
	public PackageFilteredClassloader(URL[] urls, ClassLoader parent) {
		super(urls, parent);
		if(getParent() == null) {
			this.parent = getSystemClassLoader();
		} else {
			this.parent = getParent();
		}
	}

	public PackageFilteredClassloader(URL[] urls) {
		super(urls);
		if(getParent() == null) {
			this.parent = getSystemClassLoader();
		} else {
			this.parent = getParent();
		}
	}
	
	/* (non-Javadoc)
	 * @see java.lang.ClassLoader#loadClass(java.lang.String, boolean)
	 * 
	 * Override the delegating model normally provided by this classloader.
	 */
	@Override
	protected synchronized Class<?> loadClass(String name, boolean resolve)	throws ClassNotFoundException {
		int pos = name.lastIndexOf('.');
		if((pos == PACKAGE_MASK.length()) && (name.startsWith(PACKAGE_MASK))) {
			//if this class name matches the mask then check ourselves first
			try {
				return findClass(name);
			} catch (ClassNotFoundException e) {
				return parent.loadClass(name);
			}	
		}
		try {
			return parent.loadClass(name);
		} catch (ClassNotFoundException e) {
			return findClass(name);
		}
	}

	
}
