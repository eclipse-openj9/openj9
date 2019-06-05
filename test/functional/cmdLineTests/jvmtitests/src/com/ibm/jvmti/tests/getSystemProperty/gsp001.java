/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
package com.ibm.jvmti.tests.getSystemProperty;

import java.util.Arrays;

public class gsp001 {

	private static native String getSystemProperty(String propName);
	private static native void cleanup();
	
	/*
	 * As per Java 8 JVMTI spec at https://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html#GetSystemProperties,
	 * following system properties are recommended to be available for JVMTI API before executing Java bytecodes.
	 *   java.vm.vendor
	 *   java.vm.version
	 *   java.vm.name
	 *   java.vm.info
	 *   java.library.path
	 *   java.class.path
	 * Requested by https://github.com/eclipse/openj9/issues/5565, following system property is added as well:
	 *   java.vm.specification.version
	 */
	private static String[] sysPropNames = {
		"java.vm.vendor",
		"java.vm.version",
		"java.vm.name",
		"java.vm.info",
		"java.library.path",
		"java.class.path",
		"java.vm.specification.version"
	};

	/*
	 * Ensure none of system properties returned by getSystemProperty is null.
	 */
	public boolean testGetSystemPropertyAgentOnLoad() {
		return Arrays.stream(sysPropNames).filter(s -> getSystemProperty(s) == null).count() == 0;
	}

	public String helpGetSystemPropertyAgentOnLoad() {
		return "Test that JVMTI GetSystemProperty can retrieve certain system properties at early phase Agent_OnLoad().";
	}
	
	public boolean teardown() {
		cleanup();
		return true;
	}
}
