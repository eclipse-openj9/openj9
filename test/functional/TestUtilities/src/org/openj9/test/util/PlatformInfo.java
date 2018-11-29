/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
package org.openj9.test.util;

public class PlatformInfo {
	private static final String OS_NAME = "os.name";
	private static final String osName = System.getProperty(OS_NAME);

	public static boolean isWindows() {
		return ((null != osName) && osName.startsWith("Windows"));
	}

	public static boolean isMacOS() {
		return ((null != osName) && osName.startsWith("Mac OS"));
	}

	public static String getLibrarySuffix() {
		String librarySuffix = ".so"; // default for Linux, AIX, and z/OS
		if (isWindows()) {
			librarySuffix = ".dll";
		} else if (isMacOS()) {
			librarySuffix = ".dylib";
		}
		return librarySuffix;
	}
}
