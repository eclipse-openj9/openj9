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
import java.lang.invoke.SwitchPoint;

/**
 * @author mesbah
 * This class contains tests for SwitchPoint API
 */
public class SwitchPointTest {

	/**
	 * Tests SwitchPoint invalidation in a single threaded environment
	 */
	@Test(groups = { "level.extended" })
	public void testSwitchPoint_Invalidation_Basic() {
		SwitchPoint spt = new SwitchPoint();
		
		assert(!spt.hasBeenInvalidated());
		
		SwitchPoint.invalidateAll(new SwitchPoint[]{ spt });
		
		assert(spt.hasBeenInvalidated());
	}
	
	/**
	 * Creates a target and a fallback MethodHandle, initializes a SwitchPoint using them and validates invocation of target 
	 * while the SwitchPoint is valid and invocation of the fallback when SwitchPoint has been invalidated.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void testSwitchPoint_guardWithTest_Basic() throws Throwable {
		MethodHandle target = MethodHandles.lookup().findStatic(SamePackageExample.class, "returnOne", MethodType.methodType(String.class));
		MethodHandle fallback = MethodHandles.lookup().findStatic(SamePackageExample.class, "returnTwo", MethodType.methodType(String.class));
		SwitchPoint spt = new SwitchPoint();

		MethodHandle guardedDelegator = spt.guardWithTest(target, fallback);
		
		AssertJUnit.assertEquals("1", (String) guardedDelegator.invokeExact());
		
		SwitchPoint.invalidateAll(new SwitchPoint[]{ spt });
		
		AssertJUnit.assertEquals("2", (String) guardedDelegator.invokeExact());
	}
		
	/**
	 * The test creates and combines 2 guarded pairs of method handles into two guarded delegators and combines the delegators
	 * into a single SwitchPoint. Then assertions are performed based on valid state of the SwitchPoint and invalid state.
	 */
	@Test(groups = { "level.extended" })
	public void testSwitchPoint_guardWithTest_Chained() throws Throwable {
		SwitchPoint spt = new SwitchPoint();
		assert(!spt.hasBeenInvalidated());

		MethodHandle target1 = MethodHandles.lookup().findStatic(SamePackageExample.class, "returnOne", MethodType.methodType(String.class));
		MethodHandle fallback1 = MethodHandles.lookup().findStatic(SamePackageExample.class, "returnTwo", MethodType.methodType(String.class));
		MethodHandle guardedDelegator1 = spt.guardWithTest(target1, fallback1);
		
		MethodHandle target2 = MethodHandles.lookup().findStatic(SamePackageExample.class, "returnThree", MethodType.methodType(String.class));
		MethodHandle guardedDelegator2 = spt.guardWithTest(target2, guardedDelegator1);
		
		AssertJUnit.assertEquals("3", (String) guardedDelegator2.invokeExact());
		
		SwitchPoint.invalidateAll(new SwitchPoint[]{ spt });
		
		assert(spt.hasBeenInvalidated());
		
		AssertJUnit.assertEquals("2", (String) guardedDelegator2.invokeExact());
	}
}
