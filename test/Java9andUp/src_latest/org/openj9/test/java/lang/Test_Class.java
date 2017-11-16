package org.openj9.test.java.lang;

import java.lang.Class;
import java.lang.ClassNotFoundException;
import java.lang.Module;
import java.security.AllPermission;

import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;
import org.testng.AssertJUnit;

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

@Test(groups = { "level.sanity" })
@SuppressWarnings("nls")
public class Test_Class {
	
	public static AllPermission		ALL_PERMISSION = new AllPermission();

/**
 * @tests java.lang.Class#forName(java.lang.String)
 * @tests java.lang.Class#forName(java.lang.Module, java.lang.String)
 */
@Test
public void test_forName() {
	try {
		// package java.rmi exported by module java.rmi
		Class<?> jrAccessExceptionClz = Class.forName("java.rmi.AccessException");
		AssertJUnit.assertNotNull(jrAccessExceptionClz);
		if (!jrAccessExceptionClz.getProtectionDomain().implies(ALL_PERMISSION)) {
			AssertJUnit.fail("java.rmi.AccessException should have all permission!");
		}

		Module  jrModule = jrAccessExceptionClz.getModule();
		Class<?> jrAlreadyBoundExceptionClz = Class.forName(jrModule, "java.rmi.AlreadyBoundException");
		if ((jrAlreadyBoundExceptionClz == null) || !jrAlreadyBoundExceptionClz.getProtectionDomain().implies(ALL_PERMISSION)) {
			AssertJUnit.fail("java.rmi.AlreadyBoundException should have all permission as well!");
		}
	} catch (ClassNotFoundException e) {
		AssertJUnit.fail("Unexpected ClassNotFoundException: " + e.getMessage());
	}
}

}
