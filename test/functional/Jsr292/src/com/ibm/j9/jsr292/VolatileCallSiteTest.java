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
package com.ibm.j9.jsr292;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.VolatileCallSite;
import java.lang.invoke.WrongMethodTypeException;

/**
 * @author mesbah
 * This class contains tests for VolatileCallSite API
 */
public class VolatileCallSiteTest {
	/**
	 * Test for VolatileCallSite.type()
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testType_VolatileCallSite() throws Throwable {
		MethodType mt = MethodType.methodType(String.class);
		MethodHandle mh = MethodHandles.lookup().findStatic(SamePackageExample.class, "returnOne",mt);
		VolatileCallSite vcs = new VolatileCallSite(mh);
		
		AssertJUnit.assertEquals(mt,vcs.type());
	}
	
	/**
	 * Test for validating default MethodHandle behavior of VolatileCallSite
	 * using VolatileCallSite.dynamicInvoker.invoke()
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testIllegalState_dynamicInvoker_VolatileCallSite() throws Throwable {
		MethodType mt = MethodType.methodType(void.class);
		VolatileCallSite vcs = new VolatileCallSite(mt);
		boolean iseHit = false;
		
		try {
			vcs.dynamicInvoker().invoke();
		}
		catch ( IllegalStateException e ) {
			iseHit = true;
		}
		
		AssertJUnit.assertTrue(iseHit);
	}
	
	/**
	 * Test for validating default MethodHandle behavior of VolatileCallSite
	 * using VolatileCallSite.getTarget().invoke()
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testIllegalState_getTarget_VolatileCallSite() throws Throwable {
		MethodType mt = MethodType.methodType(void.class);
		VolatileCallSite vcs = new VolatileCallSite(mt);
		boolean iseHit = false;
		
		try {
			vcs.getTarget().invoke();
		}
		catch ( IllegalStateException e ) {
			iseHit = true;
		}
		
		AssertJUnit.assertTrue(iseHit);
	}
	
	/**
	 * Basic sanity test for VolatileCallSite
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testBasic_VolatileCallSite() throws Throwable {
		MethodType mt = MethodType.methodType(double.class,String.class);
		VolatileCallSite vcs = new VolatileCallSite(mt);
		MethodHandle mh = MethodHandles.lookup().findStatic(Double.class, "parseDouble",vcs.type());
		vcs.setTarget(mh);
		double d = (double)vcs.dynamicInvoker().invokeExact("1.1");
		
		AssertJUnit.assertEquals(1.1,d);
	}
	
	/**
	 * Basic negative test for VolatileCallSite
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testBasicNegative_VolatileCallSite() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(Math.class, "sin",MethodType.methodType(double.class,double.class));
		VolatileCallSite vcs = new VolatileCallSite(mh);

		boolean wmtThrown = false;
		
		try {
			String s = (String)vcs.dynamicInvoker().invokeExact(0.0);
		}
		catch ( WrongMethodTypeException e ) {
			wmtThrown = true;
		}
		
		AssertJUnit.assertTrue(wmtThrown);
	}
}
