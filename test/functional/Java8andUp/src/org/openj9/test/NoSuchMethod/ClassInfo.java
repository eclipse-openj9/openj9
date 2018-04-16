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
package org.openj9.test.NoSuchMethod;

class ClassInfo {
	private Class<?> clazz;

	public ClassInfo(Class<?> clazz) {
		this.clazz = clazz;
	}

	public Class<?> getClazz() {
		return clazz;
	}

	public String getName() {
		return clazz != null ? clazz.getName() : null;
	}

	public String toString() {
		return clazz != null ? clazz.toString() : null;
	}

	public String getClassLoader() {
		if (clazz == null)
			return null;
		ClassLoader classLoader = clazz.getClassLoader();
		if (classLoader != null) {
			return classLoader.toString();
		} else {
			return "<Bootstrap Loader>";
		}
	}

	public String getClassPath() {
		if (clazz == null)
			return null;
		ClassLoader classLoader = clazz.getClassLoader();
		if (classLoader == null) {
			return null;
		}
		try {
			return clazz.getProtectionDomain().getCodeSource().getLocation().toString();
		} catch (Exception e) {
			return "<Unknown>";
		}
	}

}
