/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.jvm.ras.tests;

import static java.lang.Boolean.TRUE;
import static java.lang.Boolean.FALSE;
import junit.framework.TestCase;

public class LogAPISecurityTests extends TestCase {

	@Override
	protected void setUp() throws Exception {
		super.setUp();
		
		// Make sure this is off for the start of every test.
		System.clearProperty("com.ibm.jvm.enableLegacyLogSecurity");
	}

	public void testQueryLogOptions() {
		System.setProperty("com.ibm.jvm.enableLegacyLogSecurity", FALSE.toString());
		com.ibm.jvm.Log.QueryOptions();
		
	}
	
	public void testQueryLogOptionsBlocked() {
		System.clearProperty("com.ibm.jvm.enableLegacyLogSecurity");
		try {
			com.ibm.jvm.Log.QueryOptions();
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e) {
			//Pass
		}

		
		
	}
	
	public void testSetLogOptions() {
		System.setProperty("com.ibm.jvm.enableLegacyLogSecurity", FALSE.toString());
		/* Set to the default options so we don't really change things for other tests. */
		com.ibm.jvm.Log.SetOptions("error,vital");
		
	}

	public void testSetLogOptionsBlocked() {
		System.clearProperty("com.ibm.jvm.enableLegacyLogSecurity");
		try {
			/* Set to the default options so we don't really change things for other tests. */
			com.ibm.jvm.Log.SetOptions("error,vital");
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e) {
			// Pass
		}
	}
	
}
