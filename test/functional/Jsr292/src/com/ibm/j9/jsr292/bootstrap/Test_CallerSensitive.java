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
package com.ibm.j9.jsr292.bootstrap;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import com.ibm.j9.jsr292.bootstrap.*;
import java.lang.invoke.*;

import static java.lang.invoke.MethodType.*;

public class Test_CallerSensitive {

	private static CallerSensitiveClass callsens = new CallerSensitiveClass();
	
	/**
	 * Lookup with non-bootstrap lookup object, invoke with non-bootstrap
	 */
	@Test(groups = { "level.extended" })
	public void test_NonBootstrapLookup_NonBootstrapInvoke() throws Throwable {
		boolean SEThrown = false;
		MethodHandle mh = MethodHandles.lookup().findVirtual(CallerSensitiveClass.class, "callerSensitiveMethod", methodType(void.class));
		try {
			mh.invoke(callsens);
		} catch (SecurityException e) {
			SEThrown = true;
		}
		AssertJUnit.assertTrue(SEThrown);
	}
	
	/**
	 * Lookup with non-bootstrap lookup object, invoke with bootstrap 
	 */
	@Test(groups = { "level.extended" })
	public void test_NonBootstrapLookup_BootstrapInvoke() throws Throwable {
		boolean SEThrown = false;
		MethodHandle mh = MethodHandles.lookup().findVirtual(CallerSensitiveClass.class, "callerSensitiveMethod", methodType(void.class));
		try {
			Invoker.invokeVirtual(mh, callsens);
		} catch (SecurityException e) {
			SEThrown = true;
		}
		AssertJUnit.assertTrue(SEThrown);
	}
	
	/**
	 * Lookup with bootstrap lookup object, invoke with non-bootstrap
	 */
	@Test(groups = { "level.extended" })
	public void test_BootstrapLookup_NonBootstrapInvoke() throws Throwable {
		MethodHandle mh = BootstrapClassLookup.lookup().findVirtual(CallerSensitiveClass.class, "callerSensitiveMethod", methodType(void.class));
		mh.invoke(callsens);
	}
}
