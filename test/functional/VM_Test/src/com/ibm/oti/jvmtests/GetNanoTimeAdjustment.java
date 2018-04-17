/*[INCLUDE-IF Sidecar19-SE]*/
package com.ibm.oti.jvmtests;

import junit.framework.TestCase;

/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
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

public class GetNanoTimeAdjustment extends TestCase {
	static final long NANO_TIME_DIGITS = 19L;
	static final long NANO_SECONDS_IN_SECOND = 1000000000L;
	static final long TIME_MAX = 4294967295000000000L;
	static final long FAIL_CODE = -1L;
	
	
	/*
	 * Basic sanity test, can we invoke the function
	 */
	public void test_CallGetNanoTimeAdjustment() {
		SupportJVM.GetNanoTimeAdjustment(0);
	}
	
	
	/*
	 * Make getNanoTime returns a value similar to System.currentTimeMillis()
	 */
	public void test_EnsureWallClockTime() {
		String nanoTime = new Long(SupportJVM.GetNanoTimeAdjustment(0)).toString();
		String milliTime = new Long(System.currentTimeMillis()).toString();
		
		/* accuracy of 1 second */
		milliTime = milliTime.substring(0, milliTime.length() - 3);
		
		assertTrue("nanotime is not similar to millitime", nanoTime.contains(milliTime));
		
	}
	
	/*
	 * Make sure returned value has 19 digits
	 */
	public void test_EnsureResultIsNanoSecondGranularity() {
		long result = SupportJVM.GetNanoTimeAdjustment(0);
		assertEquals("GetNanoTimeAdjustment did not return 19 digits", NANO_TIME_DIGITS, new Long(result).toString().length());
	}
	
	/*
	 * A positive offset subtracts the amount (in seconds) to the current nano time
	 */
	public void test_PositiveOffset() {
		/* pick an offset difference larger than than the amount of time between the two method invocations */
		long time1 = SupportJVM.GetNanoTimeAdjustment(0);
		long time2 = SupportJVM.GetNanoTimeAdjustment(10);
		
		assertTrue("Nano time offset was incorrectly calculated", time1 > time2);
	}
	
	/*
	 * Test the accuracy of the nano time offset to 1 second
	 */
	public void test_PositiveOffset2() {
		long time1 = SupportJVM.GetNanoTimeAdjustment(0);
		long time2 = SupportJVM.GetNanoTimeAdjustment(100);
		
		/* test to an accuracy of 1sec, 
		 * this should take into account the amount it takes to call both methods plus extra 
		 */
		long accuracy = 1 * NANO_SECONDS_IN_SECOND; 
		long offset = 100 * NANO_SECONDS_IN_SECOND;
		long interval = time1 - time2;
		boolean test = ( interval < (offset + accuracy)) && (interval > (offset - accuracy));
		
		assertTrue("Nano time offset was incorrectly calculated",  test);
	}
	
	
	/*
	 * A negative offset adds the amount (in seconds) to the current nano time
	 */
	public void test_NegativeOffset() {
		/* pick an offset difference larger than than the amount of time between the two method invocations */
		long time1 = SupportJVM.GetNanoTimeAdjustment(0);
		long time2 = SupportJVM.GetNanoTimeAdjustment(-10);
		
		assertTrue("Nano time offset was in correctly calculated", time1 < time2);
	}
	
	/*
	 * Test the accuracy of the nano time offset to 1 second
	 */
	public void test_NegativeOffset2() {
		long time1 = SupportJVM.GetNanoTimeAdjustment(0);
		long time2 = SupportJVM.GetNanoTimeAdjustment(-100);
		
		/* test to an accuracy of 1sec, 
		 * this should take into account the amount it takes to call both methods plus extra 
		 */		
		long accuracy = 1 * NANO_SECONDS_IN_SECOND; 
		long offset = 100 * NANO_SECONDS_IN_SECOND;
		long interval = time2 - time1;
		boolean test = (interval < (offset + accuracy)) && (interval > (offset - accuracy));
		
		assertTrue("Nano time offset was in correctly calculated",  test);
	}
	
	/*
	 * Test bounds. Based on Oracle behaviour a valid result must fall within -4294967295000000000 and
	 * +4294967295000000000 otherwise -1 is returned.
	 */
	public void test_UpperBound() {
		long time1 = SupportJVM.GetNanoTimeAdjustment(Long.MIN_VALUE);
		assertEquals("Failed upper bounds test: allowed an invalid offset", FAIL_CODE, time1);
		
		/* Try with the lowest valid offset + 10 */
		time1 = SupportJVM.GetNanoTimeAdjustment(0);
		time1 = SupportJVM.GetNanoTimeAdjustment((-(TIME_MAX - time1)/NANO_SECONDS_IN_SECOND) + 10);
		assertNotSame("Failed upper bounds test: incorrectly rejected a valid offest", FAIL_CODE, time1);
	}
	
	public void test_LowerBound() {
		long time1 = SupportJVM.GetNanoTimeAdjustment(Long.MAX_VALUE);
		assertEquals("Failed lower bounds test: allowed an invalid offset", FAIL_CODE, time1);
		
		/* Try with the highest valid offset - 10 */
		time1 = SupportJVM.GetNanoTimeAdjustment(0);
		time1 = SupportJVM.GetNanoTimeAdjustment(((TIME_MAX + time1)/NANO_SECONDS_IN_SECOND) - 10);
		assertNotSame("Failed lower bounds test: incorrectly rejected a valid offest", FAIL_CODE, time1);
	}
}
