/*
 * Copyright IBM Corp. and others 2025
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package org.openj9.test;

/*
 * This test is to make sure that ImageReader is initialized during boot.
 * So in case an invalid java.home is set and an exception is thrown in
 * the code, the exception can be correctly thrown.
 */
public class ImageReaderInitializationTest {
	public static void main(String[] args) {
		System.setProperty("java.home", "/invalid/path/to/java");
		String javaHome = System.getProperty("java.home");
		if (javaHome.equals("/invalid/path/to/java")) {
			throw new IllegalStateException("Invalid Java home set, throwing expected IllegalStateException");
		}
	}
}
