/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

package org.openj9.test.util;

public class JavaVersion {
	private static final String JAVA_VERSION = System.getProperty("java.version");
	
	/* Return true if Java 8 is used; otherwise, return false. */
	public static boolean isJava8() {
		return JAVA_VERSION.startsWith("1.8.0");
	}
	
	/* Return true if Java 9 is used; otherwise, return false. */
	public static boolean isJava9() {
		return JAVA_VERSION.startsWith("1.9.0") || JAVA_VERSION.startsWith("9");
	}
	
	/* Return String value of "java.version" property. */
	public static String getJavaVersion() {
		return JAVA_VERSION;
	}
}
