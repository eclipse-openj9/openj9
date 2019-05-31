/*******************************************************************************
 * Copyright (c) 2014, 2019 IBM Corp. and others
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
import java.lang.reflect.*;
import java.lang.invoke.*;

import examples.A;

public class RestrictReceiverTest {

	private String callMethod(Object testInstance, String method) throws Exception {
		Method m = testInstance.getClass().getMethod(method,(Class<?>[] ) null);
		return (String) m.invoke(testInstance, (Object[]) null);
	}
	/**
	 * This test attempts to perform lookup.unreflect on "protectedMethod" in class A without setting the method
	 * as accessible.
	 *
	 * @param testInstance
	 */
	@Test(enabled = false)
	private void testUnrelfectNoAccess(Object testInstance) { 
		boolean pass = false;
		String result = null;
		
		try {
			result = callMethod(testInstance, "testUnreflectNoAccess");
			confirmResult(testInstance, result);
			pass = true;
		} catch (Exception e) {
			if (testInstance instanceof B) {
				pass = true;
			}
		}
		AssertJUnit.assertTrue(testInstance.getClass().toString() + "failed testUnreflectNoAccess test", pass);
	}
	
	
	/**
	 * This test attempts to perform lookup.unreflect() on a "protectedMethod" in class A and also sets the method
	 * as accessible. 
	 * 
	 * The spec states:
	 * "If the method's accessible flag is not set, access checking is performed immediately on behalf of the 
	 * lookup class. If m is not public, do not share the resulting handle with untrusted parties." 
	 * Based on Oracle behaviour "restrictReceiver()" is also not enforced in this case.
	 *
	 * @param testInstance
	 */
	@Test(enabled = false)
	private void testUnreflect(Object testInstance) {
		boolean pass = false;
		String result = null;
		pass = false;
		try {
			result = callMethod(testInstance, "testUnreflect");
			pass = true;
		} catch (Exception e) {
			e.printStackTrace();
		}
		AssertJUnit.assertTrue(testInstance.getClass().toString() + " was not able to access un-protected member of class A", pass);
		confirmResultWithAccess(testInstance, result);
	}
	
	/**
	 * This test attempts to perform lookup.findVirtual() on a "protectedMethod" in class A.
	 * 
	 * @param testInstance
	 */
	@Test(enabled = false)
	private void testFindVirtual(Object testInstance) {		
		boolean pass = false;
		String result = null;
		try {
			result = callMethod(testInstance, "testFindVirtual");
			confirmResult(testInstance, result);
			pass = true;
		} catch (Exception e) {
			if (testInstance instanceof B) {
				pass = true;
				//class B is not a subclass of A
			} else {
				e.printStackTrace();
			}
		}
		AssertJUnit.assertTrue(testInstance.getClass().toString() + "did not properly access member of class A via findVirtual", pass);
	}
	
	/**
	 * This test attempts to perform lookup.findSpecial() on a "protectedMethod" in class A. This 
	 * test uses the instanceClass as the specialToken
	 * 
	 * @param testInstance
	 */
	@Test(enabled = false)
	private void testFindSpecialSameTokenAsCurrentClass(Object testInstance) {
		boolean pass = false;
		String result = null;
		try {
			result = callMethod(testInstance, "testFindSpecialSameTokenAsClass");
			confirmResultSpecial(testInstance, result);
			pass = true;
		} catch (Exception e) {
			if (testInstance instanceof B) {
				pass = true;
				//class B is not a subclass of A
			} else {
				e.printStackTrace();
			}
		}
		AssertJUnit.assertTrue(testInstance.getClass().toString() + "did not properly access member of class A via findSpecial", pass);
	}
	
	/**
	 * This test attempts to perform lookup.findSpecial() on a "protectedMethod" in class A. This 
	 * test use class A as the specialToken
	 * 
	 * @param testInstance
	 */
	@Test(enabled = false)
	private void testFindSpecial(Object testInstance) {
		boolean pass = false;
		String result = null;
		try {
			result = callMethod(testInstance, "testFindSpecial");
			result = callMethod(testInstance, "testFindSpecial");//twice so we can get the cached value
			confirmResultSpecial(testInstance, result);
		} catch (Exception e) {
			pass = true;// none of the classes have private access for invokespecial in class apackage.A
		}
		AssertJUnit.assertTrue(testInstance.getClass().toString() + "did not properly access member of class A via findSpecial", pass);
	}
	
	/**
	 * This test attempts to perform lookup.bind() on a "protectedMethod" in class A. 
	 * 
	 * @param testInstance
	 */
	@Test(enabled = false)
	private void testBind(Object testInstance) {
		boolean pass = false;
		String result = null;
		try {
			result = callMethod(testInstance, "testBind");
			confirmResultBind(testInstance, result);
			pass = true;
		} catch (Exception e) {
			if (testInstance instanceof B) {
				pass = true;//B has no access
			} else {
				e.printStackTrace();
			}
		}
		AssertJUnit.assertTrue(testInstance.getClass().toString() + "did not properly access member of class A via bind", pass);
	}
	
	/**
	 * Runs all the test methods
	 * 
	 * @param testInstance
	 */
	
	@Test(enabled = false)
	private void testClass(Object testInstance) {
		testUnrelfectNoAccess(testInstance);
		testUnreflect(testInstance);
		testFindVirtual(testInstance);
		testFindSpecialSameTokenAsCurrentClass(testInstance);
		testFindSpecial(testInstance);
		testBind(testInstance);
	}
	
	private void confirmResult(Object testClass, String result) {
		if (testClass instanceof B) {
			AssertJUnit.assertTrue("class B has no access", false);
		} else if (testClass instanceof D) {
			AssertJUnit.assertEquals("class D return the wrong methodType", "(Lcom/ibm/j9/jsr292/C;)V", result);
		} else if (testClass instanceof C) {
			AssertJUnit.assertEquals("class C return the wrong methodType", "(Lcom/ibm/j9/jsr292/C;)V", result);
		} 
	}
	
	private void confirmResultSpecial(Object testClass, String result) {
		if (testClass instanceof B) {
			AssertJUnit.assertTrue("class B has no findSpecial access", false);
		} else if (testClass instanceof D) {
			AssertJUnit.assertEquals("class D return the wrong methodType", "(Lcom/ibm/j9/jsr292/D;)V", result);
		} else if (testClass instanceof C) {
			AssertJUnit.assertEquals("class C return the wrong methodType", "(Lcom/ibm/j9/jsr292/C;)V", result);
		} 
	}
	
	private void confirmResultBind(Object testClass, String result) {
		if (testClass instanceof B) {
			AssertJUnit.assertTrue("class B has no Bind access", false);
		} else if (testClass instanceof D) {
			AssertJUnit.assertEquals("class D return the wrong methodType", "()V", result);
		} else if (testClass instanceof C) {
			AssertJUnit.assertEquals("class C return the wrong methodType", "()V", result);
		} 
	}
	
	private void confirmResultWithAccess(Object testClass, String result) {
		AssertJUnit.assertEquals(testClass.getClass().toString() + " returned the wrong methodType", "(Lexamples/A;)V", result);
	}
	
	/**
	 * Main test case, runs test methods on class B, C, and D
	 * 
	 */
	@Test(groups = { "level.extended" })
	public void testRestrictReceiver() {
		testClass(new B());
		testClass(new C());
		testClass(new D());
	}
}

class B {
	public String testUnreflectNoAccess() throws Exception {
		Method m = A.getMethod("protectedMethod");
		MethodHandle mh = MethodHandles.lookup().unreflect(m);
		return mh.type().toMethodDescriptorString();
	}
	
	public String testUnreflect() throws Exception {
		Method m = A.getMethod("protectedMethod");
		m.setAccessible(true);
		MethodHandle mh = MethodHandles.lookup().unreflect(m);
		return mh.type().toMethodDescriptorString();
	}
	
	public String testFindVirtual() throws Exception {
		MethodHandle mh = MethodHandles.lookup().findVirtual(A.class, "protectedMethod", MethodType.methodType(void.class));
		return mh.type().toMethodDescriptorString();
	}
	
	public String testFindSpecialSameTokenAsClass() throws Exception {
		MethodHandle mh = MethodHandles.lookup().findSpecial(A.class, "protectedMethod", MethodType.methodType(void.class), B.class);
		return mh.type().toMethodDescriptorString();
	}
	 
	public String testFindSpecial() throws Exception {
		MethodHandle mh = MethodHandles.lookup().findSpecial(A.class, "protectedMethod", MethodType.methodType(void.class), A.class);
		return mh.type().toMethodDescriptorString();
	}
	
	public String testBind() throws Exception {
		MethodHandle mh = MethodHandles.lookup().bind(new B(), "protectedMethod", MethodType.methodType(void.class));
		return mh.type().toMethodDescriptorString();
	}
}

class C extends A {
	public String testUnreflectNoAccess() throws Exception {
		Method m = A.getMethod("protectedMethod");
		MethodHandle mh = MethodHandles.lookup().unreflect(m);
		return mh.type().toMethodDescriptorString();
	}
	
	public String testUnreflect() throws Exception {
		Method m = A.getMethod("protectedMethod");
		m.setAccessible(true);
		MethodHandle mh = MethodHandles.lookup().unreflect(m);
		return mh.type().toMethodDescriptorString();
	}
	
	public String testFindVirtual() throws Exception {
		MethodHandle mh = MethodHandles.lookup().findVirtual(A.class, "protectedMethod", MethodType.methodType(void.class));
		return mh.type().toMethodDescriptorString();
	}
	
	public String testFindSpecialSameTokenAsClass() throws Exception {
		MethodHandle mh = MethodHandles.lookup().findSpecial(A.class, "protectedMethod", MethodType.methodType(void.class), C.class);
		return mh.type().toMethodDescriptorString();
	}
	
	public String testFindSpecial() throws Exception {
		MethodHandle mh = MethodHandles.lookup().findSpecial(A.class, "protectedMethod", MethodType.methodType(void.class), A.class);
		return mh.type().toMethodDescriptorString();
	}

	public String testBind() throws Exception {
		MethodHandle mh = MethodHandles.lookup().bind(new C(), "protectedMethod", MethodType.methodType(void.class));
		return mh.type().toMethodDescriptorString();
	}
}

class D extends C {
	@Override
	public String testFindSpecialSameTokenAsClass() throws Exception {
		MethodHandle mh = MethodHandles.lookup().findSpecial(A.class, "protectedMethod", MethodType.methodType(void.class), D.class);
		return mh.type().toMethodDescriptorString();
	}
	
	@Override
	public String testFindSpecial() throws Exception {
		MethodHandle mh = MethodHandles.lookup().findSpecial(A.class, "protectedMethod", MethodType.methodType(void.class), A.class);
		return mh.type().toMethodDescriptorString();
	}
	
	@Override
	public String testBind() throws Exception {
		MethodHandle mh = MethodHandles.lookup().bind(new D(), "protectedMethod", MethodType.methodType(void.class));
		return mh.type().toMethodDescriptorString();
	}
}
