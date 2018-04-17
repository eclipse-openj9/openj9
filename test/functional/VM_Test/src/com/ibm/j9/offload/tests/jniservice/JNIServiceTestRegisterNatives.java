package com.ibm.j9.offload.tests.jniservice;

/*******************************************************************************
 * Copyright (c) 2008, 2012 IBM Corp. and others
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

import junit.framework.Test;
import junit.framework.TestSuite;
import junit.framework.TestCase;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Constructor;
import java.io.IOException;
import java.io.InputStream;

public class JNIServiceTestRegisterNatives extends TestCase {
	static final String NATIVE_LIBRARY_NAME1 = "j9offjnitest26a";
	static final String NATIVE_LIBRARY_NAME2 = "j9offjnitest26b";
	
	public static void main (String[] args) {
		junit.textui.TestRunner.run(suite());
	}
	
	public static Test suite(){
		return new TestSuite(JNIServiceTestRegisterNatives.class);
	}
	
	public void testRegisterNativesInvalidMethod(){
		
		TestNativesRegisterNatives testNatives = new TestNativesRegisterNatives();
		
		try {
			testNatives.registerWithInvalidMethodName();
			fail("expected exception because we tried to register with an invalid method name");
		} catch (NoSuchMethodError e){
			/* this is what we expect */
		}
	}
	
	public void testRegisterNativesNonNativeMethodName(){
		
		TestNativesRegisterNatives testNatives = new TestNativesRegisterNatives();
		
		try {
			testNatives.registerWithNonNativeMethodName();
			fail("expected exception because we tried to register with non native method name");
		} catch (NoSuchMethodError e){
			/* this is what we expect */
		}
	}
	
	public void testRegisterNativesOneQuarantine(){
		
		TestNativesRegisterNatives testNatives = new TestNativesRegisterNatives();
		
		/* this should be ok because there is a default native registered */
		assertTrue("expected value of 0 from default native", testNatives.getValueRegisteredNativeLibrary1() == 0);

		try {
			testNatives.getValueRegisteredNativeLibrary1NoDefault();
			fail("expected exception because native has not yet been registered");
		} catch (UnsatisfiedLinkError e){
			/* this is what we expect */
		}
		
		testNatives.registerNativesReturn1ValueInLibrary1();
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary1() == 1);
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary1NoDefault() == 1);
		
		testNatives.registerNativesReturn2ValueInLibrary1();
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary1() == 2);
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary1NoDefault() == 2);
		
		testNatives.unregisterNatives();
		
		/* this should be ok because there is a default native registered */
		assertTrue("expected value of 0 from default native", testNatives.getValueRegisteredNativeLibrary1() == 0);
		
		try {
			testNatives.getValueRegisteredNativeLibrary1NoDefault();
			fail("expected exception because native has not yet been registered");
		} catch (UnsatisfiedLinkError e){
			/* this is what we expect */
		}
		
		testNatives.registerNativesReturn1ValueInLibrary1();
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary1() == 1);
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary1NoDefault() == 1);
		
		testNatives.registerNativesReturn2ValueInLibrary1();
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary1() == 2);
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary1NoDefault() == 2);
		
		testNatives.unregisterNatives();
	}
	
	public void testRegisterNativesTwoQuarantines(){
		
		TestNativesRegisterNatives testNatives = new TestNativesRegisterNatives();
		
		/* make sure previous tests have not messed us up */
		testNatives.unregisterNatives();
		
		/* this should be ok because there is a default native registered */
		assertTrue("expected value of 0 from default native", testNatives.getValueRegisteredNativeLibrary1() == 0);

		try {
			testNatives.getValueRegisteredNativeLibrary1NoDefault();
			fail("expected exception because native has not yet been registered");
		} catch (UnsatisfiedLinkError e){
			/* this is what we expect */
		}
		
		/* this should be ok because there is a default native registered */
		assertTrue("expected value of 0 from default native", testNatives.getValueRegisteredNativeLibrary2() == 0);

		try {
			testNatives.getValueRegisteredNativeLibrary2NoDefault();
			fail("expected exception because native has not yet been registered");
		} catch (UnsatisfiedLinkError e){
			/* this is what we expect */
		}
		
		testNatives.registerNativesReturn1ValueInLibrary1();
		testNatives.registerNativesReturn1ValueInLibrary2();
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary1() == 1);
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary1NoDefault() == 1);
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary2() == 1);
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary2NoDefault() == 1);
		
		testNatives.registerNativesReturn2ValueInLibrary1();
		testNatives.registerNativesReturn2ValueInLibrary2();
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary1() == 2);
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary1NoDefault() == 2);
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary2() == 2);
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary2NoDefault() == 2);
	
		testNatives.unregisterNatives();
		
		/* this should be ok because there is a default native registered */
		assertTrue("expected value of 0 from default native", testNatives.getValueRegisteredNativeLibrary1() == 0);
		
		try {
			testNatives.getValueRegisteredNativeLibrary1NoDefault();
			fail("expected exception because native has not yet been registered");
		} catch (UnsatisfiedLinkError e){
			/* this is what we expect */
		}
		
		/* this should be ok because there is a default native registered */
		assertTrue("expected value of 0 from default native", testNatives.getValueRegisteredNativeLibrary2() == 0);
		
		try {
			testNatives.getValueRegisteredNativeLibrary2NoDefault();
			fail("expected exception because native has not yet been registered");
		} catch (UnsatisfiedLinkError e){
			/* this is what we expect */
		}
		
		testNatives.registerNativesReturn1ValueInLibrary1();
		testNatives.registerNativesReturn1ValueInLibrary2();
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary1() == 1);
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary1NoDefault() == 1);
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary2() == 1);
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary2NoDefault() == 1);

		
		testNatives.registerNativesReturn2ValueInLibrary1();
		testNatives.registerNativesReturn2ValueInLibrary2();
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary1() == 2);
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary1NoDefault() == 2);
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary2() == 2);
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary2NoDefault() == 2);

	}
	
	public void testRegisterNativesTwoQuarantinesSeparateThread(){
		
		TestNativesRegisterNatives testNatives = new TestNativesRegisterNatives();
		
		/* make sure previous tests have not messed us up */
		testNatives.unregisterNatives();
		
		/* this should be ok because there is a default native registered */
		assertTrue("expected value of 0 from default native", testNatives.getValueRegisteredNativeLibrary1() == 0);

		try {
			testNatives.getValueRegisteredNativeLibrary1NoDefault();
			fail("expected exception because native has not yet been registered");
		} catch (UnsatisfiedLinkError e){
			/* this is what we expect */
		}
		
		/* this should be ok because there is a default native registered */
		assertTrue("expected value of 0 from default native", testNatives.getValueRegisteredNativeLibrary2() == 0);

		try {
			testNatives.getValueRegisteredNativeLibrary2NoDefault();
			fail("expected exception because native has not yet been registered");
		} catch (UnsatisfiedLinkError e){
			/* this is what we expect */
		}
		
		testNatives.registerNativesReturn1ValueInLibrary1();
		testNatives.registerNativesReturn1ValueInLibrary2SeparateThread();
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary1() == 1);
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary1NoDefault() == 1);
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary2() == 1);
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary2NoDefault() == 1);
		
		testNatives.registerNativesReturn2ValueInLibrary1();
		testNatives.registerNativesReturn2ValueInLibrary2SeparateThread();
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary1() == 2);
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary1NoDefault() == 2);
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary2() == 2);
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary2NoDefault() == 2);
	
		testNatives.unregisterNatives();
		
		/* this should be ok because there is a default native registered */
		assertTrue("expected value of 0 from default native", testNatives.getValueRegisteredNativeLibrary1() == 0);
		
		try {
			testNatives.getValueRegisteredNativeLibrary1NoDefault();
			fail("expected exception because native has not yet been registered");
		} catch (UnsatisfiedLinkError e){
			/* this is what we expect */
		}
		
		/* this should be ok because there is a default native registered */
		assertTrue("expected value of 0 from default native", testNatives.getValueRegisteredNativeLibrary2() == 0);
		
		try {
			testNatives.getValueRegisteredNativeLibrary2NoDefault();
			fail("expected exception because native has not yet been registered");
		} catch (UnsatisfiedLinkError e){
			/* this is what we expect */
		}
		
		testNatives.registerNativesReturn1ValueInLibrary1();
		testNatives.registerNativesReturn1ValueInLibrary2SeparateThread();
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary1() == 1);
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary1NoDefault() == 1);
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary2() == 1);
		assertTrue("expected value of 1 from registered native", testNatives.getValueRegisteredNativeLibrary2NoDefault() == 1);

		
		testNatives.registerNativesReturn2ValueInLibrary1();
		testNatives.registerNativesReturn2ValueInLibrary2SeparateThread();
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary1() == 2);
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary1NoDefault() == 2);
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary2() == 2);
		assertTrue("expected value of 2 from registered native", testNatives.getValueRegisteredNativeLibrary2NoDefault() == 2);

	}
}
