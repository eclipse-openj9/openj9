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
package org.openj9.test.jsr335;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.util.concurrent.Callable;

@Test(groups = { "level.sanity" })
public class TestMethodReferences {

	/**
	 * Helper interface used by unbound method reference tests 
	 */
	interface MethodReferenceInterface {
		public int evalutateString(String string);
	}
	
	/**
	 * Test an unbound instance method reference
	 */
	public void testUnboundInstanceMethod() {
		String testString = "testUnboundInstanceMethod";
		
		MethodReferenceInterface sam = String::length;
		
		int length = sam.evalutateString(testString);
		
		AssertJUnit.assertTrue("sam.evaluateString() did not return the length of the test string." +
				"\n\tExpected: " + testString.length()
				+"\n\tActual: " + length,
				testString.length() == length);
	}
	
	/**
	 * Test an unbound static method reference
	 */
	public void testUnboundStaticMethod() {
		String oneString = "1";
		final int ONE = 1;
		
		MethodReferenceInterface sam = Integer::valueOf;
		
		int result = sam.evalutateString(oneString);
		
		AssertJUnit.assertTrue("sam.evaluateString(oneString) did not return the expected value." +
				"\n\tExpected: " + ONE
				+ "\n\tActual: " + result,
				ONE == result);
	}
	
	/**
	 * Test that a method reference to a constructor behaves
	 * as expected
	 */
	public void testConstructor() throws Throwable{
		Callable<TestMethodReferences> callable = TestMethodReferences::new;
		
		TestMethodReferences instance = null;
		try {
			instance = callable.call();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getLocalizedMessage());
		}
		
		AssertJUnit.assertTrue("instance is null", null != instance);
		AssertJUnit.assertTrue("instance is not an instance of TestMethodReferences", instance instanceof TestMethodReferences);
		AssertJUnit.assertTrue("instance.getClass() did not return TestMethodReferences.class, instance.getClass() = " + instance.getClass(), instance.getClass().equals(TestMethodReferences.class)); // test that we can call a method on the instance
	}
	
	
	/**
	 * Test a simple instance bound MethodReference
	 */
	public void testBoundInstanceMethod() {
		String testString = "testString";
		
		Callable<Boolean> callable = testString::isEmpty;
		
		Boolean isEmpty = true;
		try {
			isEmpty = callable.call();	
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getLocalizedMessage());
		}
		
		AssertJUnit.assertFalse("s::isEmpty should return false. s = \"" + testString + "\"", isEmpty.booleanValue());
	}
	
	public void testPrivateFinalMethod() {
		Callable<String> gts = this::getToString;
		
		String result = null;
		try {
			result = gts.call();
		} catch(Exception e){
			e.printStackTrace();
			Assert.fail(e.getLocalizedMessage());
		}
		
		AssertJUnit.assertTrue(toString().equals(result));
	}
	
	final private String getToString(){
		return toString();
	}
	
	public void testPublicFinalMethod() {
		Callable<String> gts = this::getPublicToString;
		
		String result = null;
		try {
			result = gts.call();
		} catch(Exception e){
			e.printStackTrace();
			Assert.fail(e.getLocalizedMessage());
		}
		
		AssertJUnit.assertTrue(toString().equals(result));
	}
	
	final private String getPublicToString(){
		return toString();
	}
	
	/**
	 * Test that a method reference to a superclass
	 * method behaves as expected 
	 */
	public void testSuper() {
		Callable<String> callable = org.testng.annotations.Test.class::getName;
		
		String superclassName = null;
		try {
			superclassName = callable.call();	
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getLocalizedMessage());
		}
		
		AssertJUnit.assertTrue("super::getName did not return the expected result" 
				+ "\n\tExpected: " + org.testng.annotations.Test.class.getName()
				+"\n\tActual: " + superclassName
				, org.testng.annotations.Test.class.getName().equals(superclassName));
	}
	
	/**
	 * Helper class used to test inner class method references
	 */
	class InnerMethodReferenceTestClass {}
	
	/**
	 * Test that a method reference to an inner class'
	 * constructor behaves as expected
	 */
	public void testInnerClassReference() {
		Callable<InnerMethodReferenceTestClass> callable = InnerMethodReferenceTestClass::new;
		
		InnerMethodReferenceTestClass instance = null;
		try {
			instance = callable.call();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getLocalizedMessage());
		}
		
		AssertJUnit.assertTrue("instance is null", null != instance);
		AssertJUnit.assertTrue("instance is not an instance of InnerMethodReferenceTestClass", instance instanceof InnerMethodReferenceTestClass);
		AssertJUnit.assertTrue("instance.getClass() did not return InnerMethodReferenceTestClass.class, instance.getClass() = " + instance.getClass(), instance.getClass().equals(InnerMethodReferenceTestClass.class)); // test that we can call a method on the instance
	}
}
