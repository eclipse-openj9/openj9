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

package com.ibm.j9.tests.jeptests;

/**
 * @file StaticLinking.java
 * @brief Tests J9 for static linking capability, specifically, JEP178.  Class loads
 * a couple of libraries, which may be statically or dynamically linked, depending up
 * on the test mode.
 */
public class StaticLinking {
	public static void main(String[] args) {
		StaticLinking instance = new StaticLinking();

		// Load the libraries 'testlibA' and 'testlibB'.  These libraries may be
		// statically linked or dynamically.
		System.loadLibrary("testlibA");
		System.loadLibrary("testlibB");

		// Invoke instance and static native methods and see whether these were bound
		// statically or dynamically, depending on the launcher that was used.
		System.out.println("[MSG] Calling native instance method fooImpl.");
		instance.fooImpl();
		System.out.println("[MSG] Calling native static method barImpl.");
		barImpl();
	}
	private native void fooImpl();
	private static native void barImpl();
}
