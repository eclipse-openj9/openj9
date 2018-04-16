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
import java.lang.invoke.MutableCallSite;
import java.lang.invoke.WrongMethodTypeException;

/**
 * @author mesbah
 * This class contains tests for MutableCallSite API
 */
public class MutableCallSiteTest {
	/**
	 * Basic sanity test for MutableCallSite
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testBasic_MutableCallSite() throws Throwable {
		MethodType mt = MethodType.methodType(double.class,String.class);
		MethodHandle mh = MethodHandles.lookup().findStatic(Double.class, "parseDouble", mt);
		MutableCallSite mcs = new MutableCallSite(mt);
		mcs.setTarget(mh);
		double d = (double)mcs.dynamicInvoker().invokeExact("1.1");
		
		AssertJUnit.assertEquals(1.1,d);
	}
	
	/**
	 * Test for validating default MethodHandle behavior of MutableCallSite
	 * using MutableCallSite.dynamicInvoker().invoke()
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testIllegalState_dynamicInvoker_MutableCallSite() throws Throwable {
		MethodType mt = MethodType.methodType(void.class);
		MutableCallSite mcs = new MutableCallSite(mt);
		boolean iseHit = false;
		
		try {
			mcs.dynamicInvoker().invoke();
		}
		catch ( IllegalStateException e ) {
			iseHit = true;
		}
		
		AssertJUnit.assertTrue(iseHit);
	}
	
	/**
	 * Test for validating default MethodHandle behavior of MutableCallSite
	 * using MutableCallSite.getTarget().invoke()
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testIllegalState_getTarget_MutableCallSite() throws Throwable {
		MethodType mt = MethodType.methodType(void.class);
		MutableCallSite mcs = new MutableCallSite(mt);
		boolean iseHit = false;
		
		try {
			mcs.getTarget().invoke();
		}
		catch ( IllegalStateException e ) {
			iseHit = true;
		}
		
		AssertJUnit.assertTrue(iseHit);
	}
	
	/**
	 * Basic negative test for MutableCallSite
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testBasicNegative_MutableCallSite() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(Math.class, "pow", MethodType.methodType(double.class,double.class,double.class));
		MutableCallSite mcs = new MutableCallSite(mh);
		
		boolean wmtThrown = false;
		
		try {
			String s = (String)mcs.dynamicInvoker().invokeExact(2,3);
		}
		catch ( WrongMethodTypeException e ) {
			wmtThrown = true;
		}
		
		AssertJUnit.assertTrue(wmtThrown);
	}
	
	/**
	 * Test for MutableCallSite.type()
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testType() throws Throwable {
		MethodType mt = MethodType.methodType(String.class);
		MethodHandle mh = MethodHandles.lookup().findStatic(SamePackageExample.class, "returnOne", mt);
		MutableCallSite mcs = new MutableCallSite(mh);
		
		AssertJUnit.assertEquals(mt,mcs.type());
	}
	
	/**
	 * MutableCallSite subclass that overrides setTarget, one where setTarget calls the inherited one.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testMCSChild_CallsParent() throws Throwable {
		MethodType mt = MethodType.methodType(double.class,String.class);
		MethodHandle mh = MethodHandles.lookup().findStatic(Double.class, "parseDouble", mt);
		MCSChild1 mcs = new MCSChild1(mt);
		mcs.setTarget(mh);
		double d = (double)mcs.dynamicInvoker().invokeExact("1.1");
		
		AssertJUnit.assertEquals(1.1,d);
	}
	
	/**
	 * MutableCallSite subclass that overrides setTarget, one where setTarget does not calls the inherited one.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testMCSChild_DoesNotCallParent() throws Throwable {
		MethodType mt = MethodType.methodType(double.class,String.class);
		MethodHandle mh = MethodHandles.lookup().findStatic(Double.class, "parseDouble", mt);
		MCSChild2 mcs = new MCSChild2(mt);
		mcs.setTarget(mh);
		
		AssertJUnit.assertEquals(mcs.ignoredTarget.toString(),mh.toString());
	}
}


