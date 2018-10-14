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
import java.lang.invoke.CallSite;
import java.lang.invoke.ConstantCallSite;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.MutableCallSite;
import java.lang.invoke.WrongMethodTypeException;
import examples.PackageExamples;

/**
 * @author mesbah
 * This class contains tests for ConstantCallSite API
 */
public class ConstantCallSiteTest {
	
	/**
	 * A basic sanity test for ConstantCallSite
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testBasic_ConstantCallSite() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(SamePackageExample.class, "addPublicStatic", MethodType.methodType(int.class,int.class,int.class));
		ConstantCallSite ccs = new ConstantCallSite(mh);
		int result = (int)ccs.dynamicInvoker().invokeExact(1,2);
		
		AssertJUnit.assertEquals(3,result);
		
		AssertJUnit.assertEquals(mh,ccs.getTarget());
		
		AssertJUnit.assertEquals(mh.type(),ccs.type());
	}
	
	/**
	 * A basic negative test for ConstantCallSite
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testBasicNegative_ConstantCallSite() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(SamePackageExample.class, "addPublicStatic", MethodType.methodType(int.class,int.class,int.class));
		ConstantCallSite ccs = new ConstantCallSite(mh);
		boolean wmtHit = false;
		try {
			int result = (int)ccs.dynamicInvoker().invokeExact(1,2,3);
			AssertJUnit.assertEquals(3,result);
		}
		catch(WrongMethodTypeException e) {
			wmtHit = true;
		}
		
		AssertJUnit.assertTrue(wmtHit);
	}
	
	/**
	 * Negative test - attempts to set a target to a ConstantCallSite instance
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testConstantCallSiteSetTarget() throws Throwable {
		MethodHandle mh1 = MethodHandles.lookup().findStatic(SamePackageExample.class, "addPublicStatic", MethodType.methodType(int.class,int.class,int.class));
		MethodHandle mh2 = MethodHandles.lookup().findStatic(PackageExamples.class, "addPublicStatic", MethodType.methodType(int.class,int.class,int.class));
		ConstantCallSite ccs = new ConstantCallSite(mh1);
		boolean uoHit = false;
		
		try {
			ccs.setTarget(null);
		}
		catch(UnsupportedOperationException e) {
			uoHit = true;
		}
		
		AssertJUnit.assertTrue(uoHit);
		
		uoHit = false;
		
		try {
			ccs.setTarget(mh2);
		}
		catch(UnsupportedOperationException e) {
			uoHit = true;
		}
		
		AssertJUnit.assertTrue(uoHit);
		
		uoHit = false;
		
		try {
			ccs.setTarget(mh1);
		}
		catch(UnsupportedOperationException e) {
			uoHit = true;
		}
		AssertJUnit.assertTrue(uoHit);
	}
	
	/**
	 * Tests protected ConstantCallSite(MethodType targetType,MethodHandle createTargetHook) throws Throwable
	 * A subclass of ConstantCallSite is used to invoke the protected constructor
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testTargetHookConstructor() throws Throwable {
		
		MethodType targetType = MethodType.methodType(int.class);			
		MethodHandle hook = MethodHandles.lookup().findStatic(ConstantCallSiteTest.class, "createTargetHook", MethodType.methodType(MethodHandle.class,ConstantCallSite.class));
		
		MyConstantCallSite myCcs = new MyConstantCallSite(targetType,hook);
		
		int returnVal = (int)myCcs.dynamicInvoker().invokeExact();
		
		AssertJUnit.assertEquals(1,returnVal);
	}
	
	/**
	 * Negative test for protected ConstantCallSite(MethodType targetType,MethodHandle createTargetHook).  
	 * There is a mismatch between the MT passed in and the MH.type() returned by the hook.
	 * A subclass of ConstantCallSite is used to invoke the protected constructor
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testTypeMismatch_TargetHookConstructor() throws Throwable {
		
		MethodType targetType = MethodType.methodType(String.class);			
		MethodHandle hook = MethodHandles.lookup().findStatic(ConstantCallSiteTest.class, "createTargetHook", MethodType.methodType(MethodHandle.class,ConstantCallSite.class));
		boolean wmtHit = false;
		
		try {
			MyConstantCallSite myCcs = new MyConstantCallSite(targetType,hook);
			
		}catch ( WrongMethodTypeException e ) {
			wmtHit = true;
		}
		
		AssertJUnit.assertTrue(wmtHit);
	}
	
	/**
	 * Negative test for protected ConstantCallSite(MethodType targetType,MethodHandle createTargetHook).  
	 * The hook returns null.
	 * A subclass of ConstantCallSite is used to invoke the protected constructor
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testNullHook_TargetHookConstructor() throws Throwable {
		
		MethodType targetType = MethodType.methodType(String.class);			
		MethodHandle hook = MethodHandles.lookup().findStatic(ConstantCallSiteTest.class, "createTargetHook_Null", MethodType.methodType(MethodHandle.class,ConstantCallSite.class));
		boolean npeHit = false;
		
		try {
			MyConstantCallSite myCcs = new MyConstantCallSite(targetType,hook);
			
		}catch ( NullPointerException e ) {
			npeHit = true;
		}
		
		AssertJUnit.assertTrue(npeHit);
	}
	

	/**
	 * Negative test for protected ConstantCallSite(MethodType targetType,MethodHandle createTargetHook).  
	 * The hook does not return a MethodHandle object.
	 * A subclass of ConstantCallSite is used to invoke the protected constructor
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testClassCast_TargetHookConstructor() throws Throwable {
		
		MethodType targetType = MethodType.methodType(String.class);			
		MethodHandle hook = MethodHandles.lookup().findStatic(ConstantCallSiteTest.class, "createTargetHook_NonMH", MethodType.methodType(ConstantCallSiteTest.class,ConstantCallSite.class));
		boolean cceHit = false;
		
		try {
			MyConstantCallSite myCcs = new MyConstantCallSite(targetType,hook);
			
		}catch ( ClassCastException e ) {
			cceHit = true;
		}
		
		AssertJUnit.assertTrue(cceHit);
	}
	
	/**
	 * Test to validate that a partially constructed ConstantCallSite can't be incorrectly used
	 * @throws Throwable
	 */
	CallSite unconstructedCS;
	
	@Test(groups = { "level.extended" })
	public void testUnconstructedCCS() throws Throwable {
		ConstantCallSiteTest ccs = new ConstantCallSiteTest();
		MethodHandle hookMH = MethodHandles.lookup().bind(ccs, "hook", MethodType.methodType(MethodHandle.class, ConstantCallSite.class));
		
		try {
			ConstantCallSite cs = new MyConstantCallSite(MethodType.methodType(void.class), hookMH);
		} catch(NullPointerException e) {};
		
		boolean illegalState = false;
		
		try {
			ccs.unconstructedCS.dynamicInvoker();
		} catch(IllegalStateException e) {
			illegalState = true;
		}

		AssertJUnit.assertTrue(illegalState);
		
		illegalState = false;
		try {
			ccs.unconstructedCS.getTarget();
		} catch(IllegalStateException e) {
			illegalState = true;
		}
		
		AssertJUnit.assertTrue(illegalState);
		
		illegalState = false;
		try {
			ccs.unconstructedCS.type();
		} catch(IllegalStateException e) {
			illegalState = true;
		}
		
		AssertJUnit.assertFalse(illegalState);
	}
	
	public MethodHandle hook(ConstantCallSite cs) {
		 unconstructedCS = cs;
		 throw new NullPointerException();
	}
	
	/**
	 * Subclass of ConstantCallSite where protected ConstantCallSite(MethodType targetType,MethodHandle createTargetHook) is invoked
	 */
	private class MyConstantCallSite extends ConstantCallSite {
		public MyConstantCallSite(MethodType target, MethodHandle createTargetHook) throws Throwable {
			super(target,createTargetHook);
		}
	}
	
	/**
	 * This method is used to produce the final target method handle that will be hooked to the constant call site
	 * @param c
	 * @return
	 * @throws Throwable
	 */
	public static MethodHandle createTargetHook(ConstantCallSite c) throws Throwable {
		MethodHandle targetHook = MethodHandles.lookup().findStatic(ConstantCallSiteTest.class, "getOne", MethodType.methodType(int.class));
		return targetHook;
	}
	
	/**
	 * This method is used to produce the null target method handle that will be hooked to the constant call site
	 * @param c
	 * @return
	 * @throws Throwable
	 */
	public static MethodHandle createTargetHook_Null(ConstantCallSite c) throws Throwable {
		return null;
	}
	
	/**
	 * This method is used to produce the null target method handle that will be hooked to the constant call site
	 * @param c
	 * @return
	 * @throws Throwable
	 */
	public static ConstantCallSiteTest createTargetHook_NonMH(ConstantCallSite c) throws Throwable {
		return new ConstantCallSiteTest();
	}
	
	
	/**
	 * Target method to be hooked to the call site 
	 * @return always returns 1
	 */
	public static int getOne() {
		return 1;
	}
 
	/**
	 * Test for ConstantCallSite.type()
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testType_ConstantCallSite() throws Throwable {
		MethodType mt =  MethodType.methodType(int.class,int.class,int.class);
		MethodHandle mh = MethodHandles.lookup().findStatic(SamePackageExample.class, "addPublicStatic",mt);
		ConstantCallSite ccs = new ConstantCallSite(mh);
		AssertJUnit.assertEquals(mt,ccs.type());
	}
}
