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
import com.ibm.jvm.Trace;

public class TraceAPISecurityTests extends TestCase {

	static final String[] TEMPLATES = new String[54];
	static {
		TEMPLATES[0] = Trace.EVENT;
		TEMPLATES[1] = Trace.EVENT + "Tracepoint #1 ";
		TEMPLATES[2] = Trace.EVENT + "Tracepoint #2 insert1=%s";
		TEMPLATES[3] = Trace.EVENT + "Tracepoint #3 insert1=%s insert2=%s";
		TEMPLATES[4] = Trace.EVENT + "Tracepoint #4 insert1=%s insert2=%s insert3=%s";
		TEMPLATES[5] = Trace.EVENT + "Tracepoint #5 insert1=%s insert2=%p";
		TEMPLATES[6] = Trace.EVENT + "Tracepoint #6 insert1=%p insert2=%s";
		TEMPLATES[7] = Trace.EVENT + "Tracepoint #7 insert1=%s insert2=%d";
		TEMPLATES[8] = Trace.EVENT + "Tracepoint #8 insert1=%d insert2=%s";
		TEMPLATES[9] = Trace.EVENT + "Tracepoint #9 insert1=%s insert2=%lld";
		TEMPLATES[10] = Trace.EVENT + "Tracepoint #10 insert1=%lld insert2=%s";     // mkomine@77003
		TEMPLATES[11] = Trace.EVENT + "Tracepoint #11 insert1=%s insert2=%d";
		TEMPLATES[12] = Trace.EVENT + "Tracepoint #12 insert1=%d insert2=%s";
		TEMPLATES[13] = Trace.EVENT + "Tracepoint #13 insert1=%s insert2=%c";
		TEMPLATES[14] = Trace.EVENT + "Tracepoint #14 insert1=%c insert2=%s";
		TEMPLATES[15] = Trace.EVENT + "Tracepoint #15 insert1=%s insert2=%f";
		TEMPLATES[16] = Trace.EVENT + "Tracepoint #16 insert1=%e insert2=%s";
		TEMPLATES[17] = Trace.EVENT + "Tracepoint #17 insert1=%s insert2=%e";
		TEMPLATES[18] = Trace.EVENT + "Tracepoint #18 insert1=%e insert2=%s";
		TEMPLATES[19] = Trace.EVENT + "Tracepoint #19 insert1=%p";
		TEMPLATES[20] = Trace.EVENT + "Tracepoint #20 insert1=%p insert2=%p";
		TEMPLATES[21] = Trace.EVENT + "Tracepoint #21 insert1=%d";
		TEMPLATES[22] = Trace.EVENT + "Tracepoint #22 insert1=%d insert2=%d";
		TEMPLATES[23] = Trace.EVENT + "Tracepoint #23 insert1=%d insert2=%d insert3=%d";
		TEMPLATES[24] = Trace.EVENT + "Tracepoint #24 insert1=%lld";
		TEMPLATES[25] = Trace.EVENT + "Tracepoint #25 insert1=%lld insert2=%lld";
		TEMPLATES[26] = Trace.EVENT + "Tracepoint #26 insert1=%lld insert2=%lld insert3=%lld";
		TEMPLATES[27] = Trace.EVENT + "Tracepoint #27 insert1=%d";
		TEMPLATES[28] = Trace.EVENT + "Tracepoint #28 insert1=%d insert2=%d";
		TEMPLATES[29] = Trace.EVENT + "Tracepoint #29 insert1=%d insert2=%d insert3=%d";
		TEMPLATES[30] = Trace.EVENT + "Tracepoint #30 insert1=%c";
		TEMPLATES[31] = Trace.EVENT + "Tracepoint #31 insert1=%c insert2=%c";
		TEMPLATES[32] = Trace.EVENT + "Tracepoint #32 insert1=%c insert2=%c insert3=%c";
		TEMPLATES[33] = Trace.EVENT + "Tracepoint #33 insert1=%f";
		TEMPLATES[34] = Trace.EVENT + "Tracepoint #34 insert1=%e insert2=%e";
		TEMPLATES[35] = Trace.EVENT + "Tracepoint #35 insert1=%f insert2=%f insert3=%f";
		TEMPLATES[36] = Trace.EVENT + "Tracepoint #36 insert1=%d";
		TEMPLATES[37] = Trace.EVENT + "Tracepoint #37 insert1=%d insert2=%d";
		TEMPLATES[38] = Trace.EVENT + "Tracepoint #38 insert1=%f insert2=%f insert3=%f";
		TEMPLATES[39] = Trace.EVENT + "Tracepoint #39 insert1=%s insert2=%p insert3=%s";
		TEMPLATES[40] = Trace.EVENT + "Tracepoint #40 insert1=%p insert2=%s insert3=%p";
		TEMPLATES[41] = Trace.EVENT + "Tracepoint #41 insert1=%s insert2=%d insert3=%s";
		TEMPLATES[42] = Trace.EVENT + "Tracepoint #42 insert1=%d insert2=%s insert3=%d";
		TEMPLATES[43] = Trace.EVENT + "Tracepoint #43 insert1=%s insert2=%lld insert3=%s";
		TEMPLATES[44] = Trace.EVENT + "Tracepoint #44 insert1=%lld insert2=%s insert3=%lld";
		TEMPLATES[45] = Trace.EVENT + "Tracepoint #45 insert1=%s insert2=%d insert3=%s";
		TEMPLATES[46] = Trace.EVENT + "Tracepoint #46 insert1=%d insert2=%s insert3=%d";
		TEMPLATES[47] = Trace.EVENT + "Tracepoint #47 insert1=%s insert2=%c insert3=%s";
		TEMPLATES[48] = Trace.EVENT + "Tracepoint #48 insert1=%c insert2=%s insert3=%c";
		TEMPLATES[49] = Trace.EVENT + "Tracepoint #49 insert1=%s insert2=%f insert3=%s";
		TEMPLATES[50] = Trace.EVENT + "Tracepoint #50 insert1=%f insert2=%s insert3=%f";
		TEMPLATES[51] = Trace.EVENT + "Tracepoint #51 insert1=%s insert2=%d insert3=%s";
		TEMPLATES[52] = Trace.EVENT + "Tracepoint #52 insert1=%d insert2=%s insert3=%d";
		TEMPLATES[53] = Trace.EVENT + "Tracepoint #53 TEST PASSED";
	}
	
	
	@Override
	protected void setUp() throws Exception {
		super.setUp();
		
		// Make sure this is off for the start of every test.
		System.clearProperty("com.ibm.jvm.enableLegacyTraceSecurity");
	}

	public void testGetMicros() {
		
		System.setProperty("com.ibm.jvm.enableLegacyTraceSecurity", FALSE.toString());
		com.ibm.jvm.Trace.getMicros();
		
	}
	
	public void testQueryGetMicrosNotBlocked() {
		System.setProperty("com.ibm.jvm.enableLegacyLogSecurity", FALSE.toString());
		
		try {
			com.ibm.jvm.Log.QueryOptions();
		} catch (SecurityException e) {
			// Fail, this method shouldn't have security enabled.
			fail("Expected security exception to be thrown.");
		}
	}
	
	public void testSuspendResumeNotBlocked() {
		System.setProperty("com.ibm.jvm.enableLegacyTraceSecurity", FALSE.toString());
		com.ibm.jvm.Trace.suspend();
		com.ibm.jvm.Trace.resume();
	}
	
	public void testSuspendResumeBlocked() {

		System.clearProperty("com.ibm.jvm.enableLegacyTraceSecurity");

		try {
			/* Disable global trace */
			com.ibm.jvm.Trace.suspend();
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e) {
			// Pass
		}
		try {
			/* Enable global trace */
			com.ibm.jvm.Trace.resume();
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e) {
			// Pass
		}
	}
	
	public void testSuspendThisResumeThisNotBlocked() {
		System.setProperty("com.ibm.jvm.enableLegacyTraceSecurity", FALSE.toString());
		com.ibm.jvm.Trace.suspendThis();
		com.ibm.jvm.Trace.resumeThis();
		
	}
	
	public void testSuspendThisResumeThisBlocked() {

		System.clearProperty("com.ibm.jvm.enableLegacyTraceSecurity");

		try {
			/* Set to the default options so we don't really change things for other tests. */
			com.ibm.jvm.Trace.suspendThis();
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e) {
			// Pass
		}
		try {
			/* Set to the default options so we don't really change things for other tests. */
			com.ibm.jvm.Trace.resumeThis();
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e) {
			// Pass
		}
	}
	
	public void testSetNotBlocked() {
		System.setProperty("com.ibm.jvm.enableLegacyTraceSecurity", FALSE.toString());
		int result = com.ibm.jvm.Trace.set("maximal=j9trc.0");
		assertEquals("Expected to be able to set tracepoint", 0, result);
	}
	
	public void testSetBlocked() {

		System.clearProperty("com.ibm.jvm.enableLegacyTraceSecurity");

		try {
			/* Set the trace init tracepoint, will already be set and won't ever fire after
			 * the trace library comes up anyway.
			 */
			com.ibm.jvm.Trace.set("maximal=j9trc.0");
			fail("Expected security exception to be thrown.");
		} catch (SecurityException e) {
			// Pass
		}
	}
	
	/**
	 * Re-run the trace point taking tests from TestTraceArguments to confirm
	 * that they aren't blocked by default when security is enabled.
	 */
	public void testApplicationTraceNotBlocked() {
		Object anyOldObject = new java.lang.Thread();
		Object anotherOldObject = new java.lang.Thread();
		System.setProperty("com.ibm.jvm.enableLegacyTraceSecurity", FALSE.toString());

		int componentId = Trace.registerApplication("TestTraceArguments",TEMPLATES);

		Trace.set("maximal=TestTraceArguments");

		// Tests for all the different Trace.trace() methods

		// public static void trace(int, int)
		Trace.trace(componentId,0);
		// public static void trace(int, int)
		Trace.trace(componentId,1);
		// public static void trace(int, int, java.lang.String)
		Trace.trace(componentId,2,"hello");
		// public static void trace(int, int, java.lang.String, java.lang.String)
		Trace.trace(componentId,3,"hello1","hello2");
		// public static void trace(int, int, java.lang.String, java.lang.String, java.lang.String)
		Trace.trace(componentId,4,"hello1","hello2","hello3");
		// public static void trace(int, int, java.lang.String, java.lang.Object)
		Trace.trace(componentId,5,"hello",anyOldObject);
		// public static void trace(int, int, java.lang.Object, java.lang.String)
		Trace.trace(componentId,6,anyOldObject,"hello");
		// public static void trace(int, int, java.lang.String, int)
		Trace.trace(componentId,7,"hello",9);
		// public static void trace(int, int, int, java.lang.String)
		Trace.trace(componentId,8,9,"hello");
		// public static void trace(int, int, java.lang.String, long)
		Trace.trace(componentId,9,"hello",9L);
		// public static void trace(int, int, long, java.lang.String)
		Trace.trace(componentId,10,Long.MAX_VALUE,"hello");
		// public static void trace(int, int, java.lang.String, byte)
		Trace.trace(componentId,11,"hello",Byte.MAX_VALUE);
		// public static void trace(int, int, byte, java.lang.String)
		Trace.trace(componentId,12,Byte.MAX_VALUE,"hello");
		// public static void trace(int, int, java.lang.String, char)
		Trace.trace(componentId,13,"hello",'X');
		// public static void trace(int, int, char, java.lang.String)
		Trace.trace(componentId,14,'X',"hello");
		// public static void trace(int, int, java.lang.String, float)
		Trace.trace(componentId,15,"hello",1.0f);
		// public static void trace(int, int, float, java.lang.String)
		Trace.trace(componentId,16,Double.MAX_VALUE,"hello");
		// public static void trace(int, int, java.lang.String, double)
		Trace.trace(componentId,17,"hello",Double.MAX_VALUE);
		// public static void trace(int, int, double, java.lang.String)
		Trace.trace(componentId,18,Double.MAX_VALUE,"hello");
		// public static void trace(int, int, java.lang.Object)
		Trace.trace(componentId,19,anyOldObject);
		// public static void trace(int, int, java.lang.Object, java.lang.Object)
		Trace.trace(componentId,20,anyOldObject,anotherOldObject);
		// public static void trace(int, int, int)
		Trace.trace(componentId,21,9);
		// public static void trace(int, int, int, int)
		Trace.trace(componentId,22,-1,9);
		// public static void trace(int, int, int, int, int)
		Trace.trace(componentId,23,999999,0,9);
		// public static void trace(int, int, long)
		Trace.trace(componentId,24,9L);
		// public static void trace(int, int, long, long)
		Trace.trace(componentId,25,0L,9L);
		// public static void trace(int, int, long, long, long)
		Trace.trace(componentId,26,Long.MAX_VALUE,Long.MIN_VALUE,9L);
		// public static void trace(int, int, byte)
		Trace.trace(componentId,27,9);
		// public static void trace(int, int, byte, byte)
		Trace.trace(componentId,28,-1,0);
		// public static void trace(int, int, byte, byte, byte)
		Trace.trace(componentId,29,Byte.MAX_VALUE,Byte.MIN_VALUE,0);
		// public static void trace(int, int, char)
		Trace.trace(componentId,30,'A');
		// public static void trace(int, int, char, char)
		Trace.trace(componentId,31,'A','B');
		// public static void trace(int, int, char, char, char)
		Trace.trace(componentId,32,'A','B','C');
		// public static void trace(int, int, float)
		Trace.trace(componentId,33,1.0f);
		// public static void trace(int, int, float, float)
		Trace.trace(componentId,34,Float.MAX_VALUE,Float.MIN_VALUE);
		// public static void trace(int, int, float, float, float)
		Trace.trace(componentId,35,Float.POSITIVE_INFINITY,Float.NEGATIVE_INFINITY,Float.NaN);
		// public static void trace(int, int, double)
		Trace.trace(componentId,36,999999);
		// public static void trace(int, int, double, double)
		Trace.trace(componentId,37,0,-1);
		// public static void trace(int, int, double, double, double)
		Trace.trace(componentId,38,Double.MAX_VALUE,Double.MIN_VALUE,0);
		// public static void trace(int, int, java.lang.String, java.lang.Object, java.lang.String)
		Trace.trace(componentId,39,"hello1",anyOldObject,"hello2");
		// public static void trace(int, int, java.lang.Object, java.lang.String, java.lang.Object)
		Trace.trace(componentId,40,anyOldObject,"hello",anotherOldObject);
		// public static void trace(int, int, java.lang.String, int, java.lang.String)
		Trace.trace(componentId,41,"hello1",9,"hello2");
		// public static void trace(int, int, int, java.lang.String, int)
		Trace.trace(componentId,42,9,"hello1",9);
		// public static void trace(int, int, java.lang.String, long, java.lang.String)
		Trace.trace(componentId,43,"hello1",9L,"hello2");
		// public static void trace(int, int, long, java.lang.String, long)
		Trace.trace(componentId,44,9L,"hello",9L);
		// public static void trace(int, int, java.lang.String, byte, java.lang.String)
		Trace.trace(componentId,45,"hello1",0,"hello2");
		// public static void trace(int, int, byte, java.lang.String, byte)
		Trace.trace(componentId,46,1,"hello",1);
		// public static void trace(int, int, java.lang.String, char, java.lang.String)
		Trace.trace(componentId,47,"hello1",'A',"hello2");
		// public static void trace(int, int, char, java.lang.String, char)
		Trace.trace(componentId,48,'A',"hello",'B');
		// public static void trace(int, int, java.lang.String, float, java.lang.String)
		Trace.trace(componentId,49,"hello1",1.0f,"hello2");
		// public static void trace(int, int, float, java.lang.String, float)
		Trace.trace(componentId,50,-1.0f,"hello",1.0f);
		// public static void trace(int, int, java.lang.String, double, java.lang.String)
		Trace.trace(componentId,51,"hello1",1,"hello2");
		// public static void trace(int, int, double, java.lang.String, double)
		Trace.trace(componentId,52,-1,"hello",1);

		Trace.trace(componentId,53); // test passed message, validation in script
	}
	
	/**
	 * Re-run the trace point taking tests from TestTraceArguments to confirm
	 * that they are blocked when security is enabled. A bit long winded but it's
	 * the only way to make sure I didn't miss adding security to one of the
	 * Trace.trace() methods.
	 */
	public void testApplicationTraceBlocked() {

		System.clearProperty("com.ibm.jvm.enableLegacyTraceSecurity");

		Object anyOldObject = new java.lang.Thread();
		Object anotherOldObject = new java.lang.Thread();
		
		int componentId = 100; 
		
		try {
			componentId = Trace.registerApplication("TestTraceArguments",
					TEMPLATES);
		} catch (SecurityException e) {
			/* Pass */
		}

		try {
			Trace.set("maximal=TestTraceArguments");
		} catch (SecurityException e) {
			/* Pass */
		}

		// Tests for all the different Trace.trace() methods

		// public static void trace(int, int)
		try {
			Trace.trace(componentId, 0);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int)
		try {
			Trace.trace(componentId, 1);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.String)
		try {
			Trace.trace(componentId, 2, "hello");
			// java.lang.String)
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.String,
		try {
			Trace.trace(componentId, 3, "hello1", "hello2");
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.String,
		// java.lang.String, java.lang.String)
		try {
			Trace.trace(componentId, 4, "hello1", "hello2", "hello3");
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.String,
		// java.lang.Object)
		try {
			Trace.trace(componentId, 5, "hello", anyOldObject);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.Object,
		// java.lang.String)
		try {
			Trace.trace(componentId, 6, anyOldObject, "hello");
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.String, int)
		try {
			Trace.trace(componentId, 7, "hello", 9);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, int, java.lang.String)
		try {
			Trace.trace(componentId, 8, 9, "hello");
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.String, long)
		try {
			Trace.trace(componentId, 9, "hello", 9L);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, long, java.lang.String)
		try {
			Trace.trace(componentId, 10, Long.MAX_VALUE, "hello");
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.String, byte)
		try {
			Trace.trace(componentId, 11, "hello", Byte.MAX_VALUE);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, byte, java.lang.String)
		try {
			Trace.trace(componentId, 12, Byte.MAX_VALUE, "hello");
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.String, char)
		try {
			Trace.trace(componentId, 13, "hello", 'X');
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, char, java.lang.String)
		try {
			Trace.trace(componentId, 14, 'X', "hello");
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.String, float)
		try {
			Trace.trace(componentId, 15, "hello", 1.0f);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, float, java.lang.String)
		try {
			Trace.trace(componentId, 16, Double.MAX_VALUE, "hello");
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.String, double)
		try {
			Trace.trace(componentId, 17, "hello", Double.MAX_VALUE);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, double, java.lang.String)
		try {
			Trace.trace(componentId, 18, Double.MAX_VALUE, "hello");
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.Object)
		try {
			Trace.trace(componentId, 19, anyOldObject);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.Object,
		// java.lang.Object)
		try {
			Trace.trace(componentId, 20, anyOldObject, anotherOldObject);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, int)
		try {
			Trace.trace(componentId, 21, 9);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, int, int)
		try {
			Trace.trace(componentId, 22, -1, 9);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, int, int, int)
		try {
			Trace.trace(componentId, 23, 999999, 0, 9);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, long)
		try {
			Trace.trace(componentId, 24, 9L);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, long, long)
		try {
			Trace.trace(componentId, 25, 0L, 9L);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, long, long, long)
		try {
			Trace.trace(componentId, 26, Long.MAX_VALUE, Long.MIN_VALUE, 9L);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, byte)
		try {
			Trace.trace(componentId, 27, 9);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, byte, byte)
		try {
			Trace.trace(componentId, 28, -1, 0);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, byte, byte, byte)
		try {
			Trace.trace(componentId, 29, Byte.MAX_VALUE, Byte.MIN_VALUE, 0);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, char)
		try {
			Trace.trace(componentId, 30, 'A');
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, char, char)
		try {
			Trace.trace(componentId, 31, 'A', 'B');
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, char, char, char)
		try {
			Trace.trace(componentId, 32, 'A', 'B', 'C');
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, float)
		try {
			Trace.trace(componentId, 33, 1.0f);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, float, float)
		try {
			Trace.trace(componentId, 34, Float.MAX_VALUE, Float.MIN_VALUE);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, float, float, float)
		try {
			Trace.trace(componentId, 35, Float.POSITIVE_INFINITY,
					Float.NEGATIVE_INFINITY, Float.NaN);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, double)
		try {
			Trace.trace(componentId, 36, 999999);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, double, double)
		try {
			Trace.trace(componentId, 37, 0, -1);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, double, double, double)
		try {
			Trace.trace(componentId, 38, Double.MAX_VALUE, Double.MIN_VALUE, 0);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.String,
		// java.lang.Object, java.lang.String)
		try {
			Trace.trace(componentId, 39, "hello1", anyOldObject, "hello2");
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.Object,
		// java.lang.String, java.lang.Object)
		try {
			Trace.trace(componentId, 40, anyOldObject, "hello",
					anotherOldObject);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.String, int,
		// java.lang.String)
		try {
			Trace.trace(componentId, 41, "hello1", 9, "hello2");
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, int, java.lang.String, int)
		try {
			Trace.trace(componentId, 42, 9, "hello1", 9);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.String, long,
		// java.lang.String)
		try {
			Trace.trace(componentId, 43, "hello1", 9L, "hello2");
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, long, java.lang.String, long)
		try {
			Trace.trace(componentId, 44, 9L, "hello", 9L);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.String, byte,
		// java.lang.String)
		try {
			Trace.trace(componentId, 45, "hello1", 0, "hello2");
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, byte, java.lang.String, byte)
		try {
			Trace.trace(componentId, 46, 1, "hello", 1);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.String, char,
		// java.lang.String)
		try {
			Trace.trace(componentId, 47, "hello1", 'A', "hello2");
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, char, java.lang.String, char)
		try {
			Trace.trace(componentId, 48, 'A', "hello", 'B');
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.String, float,
		// java.lang.String)
		try {
			Trace.trace(componentId, 49, "hello1", 1.0f, "hello2");
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, float, java.lang.String,
		// float)
		try {
			Trace.trace(componentId, 50, -1.0f, "hello", 1.0f);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, java.lang.String, double,
		// java.lang.String)
		try {
			Trace.trace(componentId, 51, "hello1", 1, "hello2");
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		// public static void trace(int, int, double, java.lang.String,
		// double)
		try {
			Trace.trace(componentId, 52, -1, "hello", 1);
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
		try {
			Trace.trace(componentId, 53); // test passed message
			fail("Expected SecurityException to be thrown");
		} catch (SecurityException e) {
			/* Pass */
		}
	}

	
}
