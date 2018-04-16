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
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.concurrent.Callable;

@Test(groups = { "level.sanity" })
public class TestLambda {

	private String instanceString = null;
	private static final String staticString = "StaticBoundString";

	public TestLambda() {
		instanceString = "InstanceString";
	}
	
	/**
	 * Test that a simple unbound (stateless) lambda behaves
	 * as expected.
	 */
	@Test
	public void testUnbound1() {
		Callable<String> callable = () -> "success";
				
		final String expectedString = "success";
		
		String resultString = null;
		try {
			resultString = callable.call();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}
		
		AssertJUnit.assertTrue("resultString is not equal to expected value. " +
				"\n\tExpected: " + expectedString + "\n\tActual: " + resultString, 
				resultString.equals(expectedString));
	}

	
	/**
	 * Test that a simple unbound (stateless) lambda with two
	 * arguments behaves as expected.
	 */
	@Test
	public void testUnbound2() {
		ArrayList<String> list = new ArrayList<String>(5);
		list.add("22");
		list.add("4444");
		list.add("1");
		list.add("55555");
		list.add("333");
		
		/* Sort the list by descending String.length()
		 * i.e. from the longest to the shortest String 
		 */
		Collections.sort(list, (a, b) -> b.length() - a.length());

		/* Verify that the list was correctly sorted */
		int lenghtOfPrevious = list.get(0).length();
		for (int i = 1 ; i < list.size() ; i++) {
			AssertJUnit.assertTrue("List was not correclty sorted in descending String.length() order." +
					"\n\tCurrent element: " + list.get(i) + 
					"\n\tPrevious element: " + list.get(i - 1),
					list.get(i).length() <= lenghtOfPrevious);	
		}
	}
	
	/**
	 * Test a simple instance variable bound lambda.
	 */
	@Test
	public void testInstanceBound1() {
		Callable<String> callable = () -> instanceString;
		
		String resultString = null;
		try {
			resultString = callable.call();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}		
		
		AssertJUnit.assertTrue("Simple instance bound lambda did not return the correct value. " +
				"\n\tExpected: " + instanceString +"\n\tActual: " + resultString, 
				instanceString.equals(resultString));
	}
	
	/**
	 * Test an instance variable and local variable bound lambda.
	 */
	@Test
	public void testInstanceBound2() {
		final String localString = getLocalString();

		/**
		 *  TODO: Uncomment the following two lines and replace them in
		 *  the test once the java compiler bug is fixed 
		 */
//		Callable<String> callable = () -> (localString + " " + instanceString);
		
//		final String expectedString = localString + " " + instanceString;

		Callable<String> callable = () -> instanceString.concat(localString);
		
		final String expectedString = instanceString.concat(localString);

		String resultString = null;
		try {
			resultString = callable.call();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}
		
		AssertJUnit.assertTrue("Local and instance bound lambda did not return the correct value. " +
				"\n\tExpected: " + expectedString +"\n\tActual: " + resultString, 
				expectedString.equals(resultString));
	}
	
	/**
	 * Helper method used by the tests to prevent the compiler 
	 * from inlining the string in the lambda expressions 
	 */
	private String getLocalString() {
		return "localString";
	}
	
	/**
	 * Test a bound lambda which uses one final local variable.
	 */
	@Test
	public void testLocalBound1() {
		final String localString1 = getLocalString() + "1";

		Callable<String> callable = () -> localString1;
		
		final String expectedString = localString1;

		String resultString = null;
		try {
			resultString = callable.call();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}
		
		AssertJUnit.assertTrue("Local variable bound lambda did not return the correct value. " +
				"\n\tExpected: " + expectedString +"\n\tActual: " + resultString, 
				expectedString.equals(resultString));
	}
	
	/**
	 * Test a bound lambda which uses two final local variables.
	 */
	@Test
	public void testLocalBound2() {
		final String localString1 = getLocalString() + "1";
		final String localString2 = getLocalString() + "2";

		/**
		 *  TODO: Uncomment the following two lines and replace them in
		 *  the test once the java compiler bug is fixed 
		 */
//		Callable<String> callable = () -> (localString1 + " " + localString2);
//		
//		final String expectedString = localString1 + " " + localString2;
		
		Callable<String> callable = () -> (localString1.concat(localString2));
		
		final String expectedString = localString1.concat(localString2);

		String resultString = null;
		try {
			resultString = callable.call();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}
		
		AssertJUnit.assertTrue("Two local variable bound lambda did not return the correct value. " +
				"\n\tExpected: " + expectedString +"\n\tActual: " + resultString, 
				expectedString.equals(resultString));
	}
	
	/**
	 * Test a lambda which uses a class static variable.
	 */
	@Test
	public void testStaticBound() {
		Callable<String> callable = () -> staticString;
		
		final String expectedString = staticString;

		String resultString = null;
		try {
			resultString = callable.call();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}
		
		AssertJUnit.assertTrue("Static bound lambda did not return the correct value. " +
				"\n\tExpected: " + expectedString +"\n\tActual: " + resultString, 
				expectedString.equals(resultString));
	}
	
	/**
	 * Test a lambda which uses a class static variable and
	 * a local final variable.
	 */
	@Test
	public void testStaticAndLocalBound() {
		final String localString1 = getLocalString() + "1";

		/**
		 *  TODO: Uncomment the following two lines and replace them in
		 *  the test once the java compiler bug is fixed 
		 */
//		Callable<String> callable = () -> (localString1 + " " + staticString);
//		
//		final String expectedString = localString1 + " " + staticString;

		Callable<String> callable = () -> (localString1.concat(staticString));
		
		final String expectedString = localString1.concat(staticString);
		
		String resultString = null;
		try {
			resultString = callable.call();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}
		
		AssertJUnit.assertTrue("Local and static bound lambda did not return the correct value. " +
				"\n\tExpected: " + expectedString +"\n\tActual: " + resultString, 
				expectedString.equals(resultString));
	}
	
	/**
	 * Test a lambda which uses "this".
	 */
	@Test
	public void testThis() {
		Callable<String> callable = () -> (this.instanceString);
		
		final String expectedString = instanceString;
		
		String resultString = null;
		try {
			resultString = callable.call();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}

		AssertJUnit.assertTrue("Lambda did not return the correct value. " +
				"\n\tExpected: " + expectedString +"\n\tActual: " + resultString, 
				expectedString.equals(resultString));
	}
	
	/**
	 * Test a lambda which uses "super".
	 */
	@Test
	public void testSuper() {
		Callable<String> callable = () -> (org.testng.annotations.Test.class.getName());

		final String expectedString = org.testng.annotations.Test.class.getName();

		String resultString = null;
		try {
			resultString = callable.call();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}

		AssertJUnit.assertTrue("Lambda did not return the correct value. " +
				"\n\tExpected: " + expectedString +"\n\tActual: " + resultString, 
				expectedString.equals(resultString));
	}
	
	interface BridgeCallableInterface extends Callable<String> {
		public String call() throws Exception;
	}
	
	/**
	 * Test that a simple unbound (stateless) lambda behaves
	 * as expected when used with a bridge method.
	 */
	@Test
	public void testUnboundBridge1() {
		BridgeCallableInterface myBridgeCallableInterface = () -> "success";
				
		final String expectedString = "success";
		
		String resultString = null;
		
		try {
			resultString = myBridgeCallableInterface.call();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}
		
		AssertJUnit.assertTrue("resultString is not equal to expected value. " +
				"\n\tExpected: " + expectedString + "\n\tActual: " + resultString, 
				resultString.equals(expectedString));
		
		
		Callable callable = (Callable) myBridgeCallableInterface;
		try {
			resultString = callable.call().toString();  /* call bridge method */
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}
		
		AssertJUnit.assertTrue("resultString is not equal to expected value. " +
				"\n\tExpected: " + expectedString + "\n\tActual: " + resultString, 
				resultString.equals(expectedString));
	}
		
	/**
	 * Test that a simple bound lambda behaves
	 * as expected when used with a bridge method.
	 */
	@Test
	public void testBoundBridge1() {
		BridgeCallableInterface myBridgeCallableInterface = () -> getLocalString() + "1";
				
		final String expectedString = getLocalString() + "1";
		
		String resultString = null;
		
		try {
			resultString = myBridgeCallableInterface.call();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}
		
		AssertJUnit.assertTrue("resultString is not equal to expected value. " +
				"\n\tExpected: " + expectedString + "\n\tActual: " + resultString, 
				resultString.equals(expectedString));
		
		
		Callable callable = (Callable) myBridgeCallableInterface;
		try {
			resultString = callable.call().toString();  /* call bridge method */
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}
		
		AssertJUnit.assertTrue("resultString is not equal to expected value. " +
				"\n\tExpected: " + expectedString + "\n\tActual: " + resultString, 
				resultString.equals(expectedString));
	}
	
	/**
	 * Test that a local and static bound lambda behaves
	 * as expected when used with a bridge method.
	 */
	@Test
	public void testLocalStaticBridge() {
		BridgeCallableInterface myBridgeCallableInterface = () -> (getLocalString() + "1").concat(staticString);
				
		final String expectedString = (getLocalString() + "1").concat(staticString);
		
		String resultString = null;
		
		try {
			resultString = myBridgeCallableInterface.call();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}
		
		AssertJUnit.assertTrue("resultString is not equal to expected value. " +
				"\n\tExpected: " + expectedString + "\n\tActual: " + resultString, 
				resultString.equals(expectedString));
		
		
		Callable callable = (Callable) myBridgeCallableInterface;
		try {
			resultString = callable.call().toString();  /* call bridge method */
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}
		
		AssertJUnit.assertTrue("resultString is not equal to expected value. " +
				"\n\tExpected: " + expectedString + "\n\tActual: " + resultString, 
				resultString.equals(expectedString));
	}

	/**
	 * Test a functional interface which redeclares an object method. 
	 * In this case Comparator which redeclares boolean equals(Object).
	 */
	@Test
	public void testComparatorEquals(){
		Comparator<Object> comparator = (a,b) -> 1;
		
		AssertJUnit.assertTrue("comparator.equals(comparator) did not return true", 
				comparator.equals(comparator));
		
		AssertJUnit.assertFalse("comparator.equals(this) did not return false",
				comparator.equals(this));
	}

}
