/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
import java.io.Serializable;
import java.util.concurrent.Callable;

public class TestLambdaAccessibilityRules {

	/**
	 * Helper method used for testing lambdas that call private methods.
	 * This method adds a constant to a parameter so that it is
	 * not inlined by the compiler.
	 */
	private String privateStringMethod(String testString) {
		return "private" + testString;
	}
	
	/**
	 * Helper method used for testing lambdas that call protected methods.
	 * This method adds a constant to a parameter so that it is
	 * not inlined by the compiler.
	 */
	protected String protectedStringMethod(String testString) {
		return "protected" + testString;
	}
	
	/**
	 * Helper method used for testing lambdas that call package methods.
	 * This method adds a constant to a parameter so that it is
	 * not inlined by the compiler.
	 */
	String packageStringMethod(String testString) {
		return "package" + testString;
	}
	
	/**
	 * Helper method called by @ref testProtectedAndPackageLambda() 
	 */
	protected String protectedAndPackageStringMethod(String testString) {
		return packageStringMethod(testString).concat("protected_");
	}
		
	/**
	 * Helper method called by inner class tests.
	 * This method adds a constant to a parameter so that it is
	 * not inlined by the compiler.
	 */
	public String publicStringMethod(String testString) {
		return "public" + testString;
	}
			
	/* Enum used by inner class tests */
	enum LAMBDA_TYPE {PRIVATE_LAMBDA, PROTECTED_LAMBDA, PACKAGE_LAMBDA, PUBLIC_LAMBDA};
	
	/**
	 * Helper method used to get lambdas that call methods with different 
	 * accessibility rules.
	 * @param testString
	 * @param lambdaType
	 * @return
	 */
	private Callable<String> getCallable(String testString, LAMBDA_TYPE lambdaType) {
		Callable<String> callable = null;
		
		switch (lambdaType) {
		case PRIVATE_LAMBDA:
			callable = () -> privateStringMethod(testString);
			break;
		case PROTECTED_LAMBDA:
			callable = () -> protectedStringMethod(testString);
			break;
		case PACKAGE_LAMBDA:
			callable = () -> packageStringMethod(testString);
			break;
		case PUBLIC_LAMBDA:
			callable = () -> publicStringMethod(testString);
			break;
		default:
			throw new RuntimeException("Unexpected lambdaType: " + lambdaType);
		}
		
		return callable;
	}
	
	/**
	 * Helper interface used to test lambdas which are Serializable SAMs
	 */
	interface SerializableCallable<T> extends Callable<T>, Serializable { }
	
	/**
	 * Helper method used to get lambdas that are Serializable SAMs and call 
	 * methods with different accessibility rules.
	 * @param testString
	 * @param lambdaType
	 * @return
	 */
	private SerializableCallable<String> getSerializableCallable(String testString, LAMBDA_TYPE lambdaType) {
		SerializableCallable<String> callable = null;
		
		switch (lambdaType) {
		case PRIVATE_LAMBDA:
			callable = () -> privateStringMethod(testString);
			break;
		case PROTECTED_LAMBDA:
			callable = () -> protectedStringMethod(testString);
			break;
		case PACKAGE_LAMBDA:
			callable = () -> packageStringMethod(testString);
			break;
		case PUBLIC_LAMBDA:
			callable = () -> publicStringMethod(testString);
			break;
		default:
			throw new RuntimeException("Unexpected lambdaType: " + lambdaType);
		}
		
		return callable;
	}
	
	
	/**
	 * Helper method used to get the expected return string when calling
	 * a Callable<String> obtained from @ref getCallable(String, LAMBDA_TYPE)
	 * @param testString
	 * @param lambdaType
	 * @return
	 */
	private String getExpectedString(String testString, LAMBDA_TYPE lambdaType) {
		String expectedString = null;
		
		switch (lambdaType) {
		case PRIVATE_LAMBDA:
			expectedString = privateStringMethod(testString);
			break;
		case PROTECTED_LAMBDA:
			expectedString = protectedStringMethod(testString);
			break;
		case PACKAGE_LAMBDA:
			expectedString = packageStringMethod(testString);
			break;
		case PUBLIC_LAMBDA:
			expectedString = publicStringMethod(testString);
			break;
		default:
			throw new RuntimeException("Unexpected lambdaType: " + lambdaType);
		}
		
		return expectedString;
	}

	/**
	 * Test a lambda that calls methods with different accessibility rules.
	 */
	@Test(groups = { "level.sanity" })
	public void testSimpleLambdaAccessibility() {
		String testString = "testSimpleLambdaAccessibility";

		for (LAMBDA_TYPE lambdaType : LAMBDA_TYPE.values()) {
			Callable<String> callable = getCallable(testString, lambdaType);

			String resultString = null;
			try {
				resultString = callable.call();
			} catch (Exception e) {
				e.printStackTrace();
				Assert.fail(e.getMessage());
			}		

			String expectedString = getExpectedString(testString, lambdaType);

			AssertJUnit.assertTrue("Lambda did not return expected value. LAMBDA_TYPE: " + lambdaType +
					"\n\tExpected: " + expectedString + "\n\tActual: " + resultString, 
					expectedString.equals(resultString));
		}
	}
		
	
	/**
	 * Test a Serializable lambda that calls methods with different 
	 * accessibility rules.
	 */
	@Test(groups = { "level.sanity" })
	public void testSerializableLambdaAccessibility() {
		String testString = "testSerializableLambdaAccessibility";

		for (LAMBDA_TYPE lambdaType : LAMBDA_TYPE.values()) {
			SerializableCallable<String> callable = getSerializableCallable(testString, lambdaType);

			String resultString = null;
			try {
				resultString = callable.call();
			} catch (Exception e) {
				e.printStackTrace();
				Assert.fail(e.getMessage());
			}		

			String expectedString = getExpectedString(testString, lambdaType);

			AssertJUnit.assertTrue("Serializable lambda did not return expected value. LAMBDA_TYPE: " + lambdaType +
					"\n\tExpected: " + expectedString + "\n\tActual: " + resultString, 
					expectedString.equals(resultString));
		}
	}
	
		
	/**
	 * Test a lambda that calls a private and a protected method.
	 */
	@Test(groups = { "level.sanity" })
	public void testPrivateAndProtectedLambda() {
		String testString = "testPrivateAndProtectedLambda";
		
		Callable<String> callable = () -> {
			String returnString = privateStringMethod(testString) + "_";
			returnString = returnString.concat(protectedStringMethod(testString));
			return returnString;
		};
		
		String resultString = null;
		try {
			resultString = callable.call();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}		
		
		String expectedString = privateStringMethod(testString) + "_";
		expectedString = expectedString.concat(protectedStringMethod(testString));
		
		AssertJUnit.assertTrue("Lambda did not return expected value. " +
			"\n\tExpected: " + expectedString + "\n\tActual: " + resultString, 
			expectedString.equals(resultString));
	}
	
	/**
	 * Test a lambda that calls a protected and a package method.
	 */
	@Test(groups = { "level.sanity" })
	public void testProtectedAndPackageLambda() {
		String testString = "testProtectedAndPackageLambda";
		
		Callable<String> callable = () -> protectedAndPackageStringMethod(testString);
		
		String resultString = null;
		try {
			resultString = callable.call();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}		
		
		String expectedString = protectedAndPackageStringMethod(testString);
		
		AssertJUnit.assertTrue("Lambda did not return expected value. " +
			"\n\tExpected: " + expectedString + "\n\tActual: " + resultString, 
			expectedString.equals(resultString));
	}
	
	
	/**
	 * Test a Serializable lambda that calls a private and a protected method.
	 */
	@Test(groups = { "level.sanity" })
	public void testSerializablePrivateAndProtectedLambda() {
		String testString = "testSerializablePrivateAndProtectedLambda";
		
		SerializableCallable<String> callable = () -> {
			String returnString = privateStringMethod(testString) + "_";
			returnString = returnString.concat(protectedStringMethod(testString));
			return returnString;
		};
		
		String resultString = null;
		try {
			resultString = callable.call();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}		
		
		String expectedString = privateStringMethod(testString) + "_";
		expectedString = expectedString.concat(protectedStringMethod(testString));
		
		AssertJUnit.assertTrue("Serializable lambda did not return expected value. " +
			"\n\tExpected: " + expectedString + "\n\tActual: " + resultString, 
			expectedString.equals(resultString));
	}
	
	/**
	 * Test a Serializable lambda that calls a protected and a package method.
	 */
	@Test(groups = { "level.sanity" })
	public void testSerializableProtectedAndPackageLambda() {
		String testString = "testSerializableProtectedAndPackageLambda";
		
		SerializableCallable<String> callable = () -> protectedAndPackageStringMethod(testString);
		
		String resultString = null;
		try {
			resultString = callable.call();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}		
		
		String expectedString = protectedAndPackageStringMethod(testString);
		
		AssertJUnit.assertTrue("Serializable lambda did not return expected value. " +
			"\n\tExpected: " + expectedString + "\n\tActual: " + resultString, 
			expectedString.equals(resultString));
	}
	
	/**
	 * Helper class with methods that have lambda's which call containing 
	 * class' methods. Used for testing inner class accessibility rules.
	 */
	class ContainingClassCaller {
		
		/**
		 * Creates and executes a lambda which calls a method in the containing
		 * class and returns its result. The method called depends on the specified
		 * lambdaType. The SAM type depends on the value of useSerializable.
		 *  
		 * @param testString		The testString to pass into the containing class' method
		 * @param lambdaType		The accessibility rule of the method to call
		 * @param useSerializable	If true the lambda will be a SAM of the type SerializableCallable<String>,
		 * 							otherwise it will be a Callable<String>
		 * @return	The return value of the containing class' method or null if an error occurred
		 */
		private String privateTestString(String testString, LAMBDA_TYPE lambdaType, boolean useSerializable) {
			Callable<String> callable = null; 
					
			if (useSerializable) {
				callable = getSerializableCallable(testString, lambdaType);
			} else {
				callable = getCallable(testString, lambdaType);
			}
			
			String resultString = null;
			try {
				resultString = callable.call();
			} catch (Exception e) {
				e.printStackTrace();
			}
			
			return resultString;
		}
		
		/**
		 * This is a copy of @ref privateTestString with the only difference
		 * being the method's accessibility/visibility
		 */
		protected String protectedTestString(String testString, LAMBDA_TYPE lambdaType, boolean useSerializable) {
			Callable<String> callable = null; 
					
			if (useSerializable) {
				callable = getSerializableCallable(testString, lambdaType);
			} else {
				callable = getCallable(testString, lambdaType);
			}
			
			String resultString = null;
			try {
				resultString = callable.call();
			} catch (Exception e) {
				e.printStackTrace();
			}
			
			return resultString;
		}
		
		/**
		 * This is a copy of @ref privateTestString with the only difference
		 * being the method's accessibility/visibility
		 */
		String packageTestString(String testString, LAMBDA_TYPE lambdaType, boolean useSerializable) {
			Callable<String> callable = null; 
					
			if (useSerializable) {
				callable = getSerializableCallable(testString, lambdaType);
			} else {
				callable = getCallable(testString, lambdaType);
			}
			
			String resultString = null;
			try {
				resultString = callable.call();
			} catch (Exception e) {
				e.printStackTrace();
			}
			
			return resultString;
		}
		
		/**
		 * This is a copy of @ref privateTestString with the only difference
		 * being the method's accessibility/visibility
		 */
		public String publicTestString(String testString, LAMBDA_TYPE lambdaType, boolean useSerializable) {
			Callable<String> callable = null; 
					
			if (useSerializable) {
				callable = getSerializableCallable(testString, lambdaType);
			} else {
				callable = getCallable(testString, lambdaType);
			}
			
			String resultString = null;
			try {
				resultString = callable.call();
			} catch (Exception e) {
				e.printStackTrace();
			}
			
			return resultString;
		}
	}
	
	/**
	 * Helper class with methods that have lambda's which call another inner 
	 * class' methods. Used for testing inner class accessibility rules. 
	 */
	class InnerClassCaller {
		
		private Callable<String> getCallable(String testString, LAMBDA_TYPE outerLambdaType, LAMBDA_TYPE innerLambdaType) {
			Callable<String> callable = null;
			
			switch (outerLambdaType) {
			case PRIVATE_LAMBDA:
				callable = () -> (new ContainingClassCaller()).privateTestString(testString, innerLambdaType, false);
				break;
			case PROTECTED_LAMBDA:
				callable = () -> (new ContainingClassCaller()).protectedTestString(testString, innerLambdaType, false);
				break;
			case PACKAGE_LAMBDA:
				callable = () -> (new ContainingClassCaller()).packageTestString(testString, innerLambdaType, false);
				break;
			case PUBLIC_LAMBDA:
				callable = () -> (new ContainingClassCaller()).publicTestString(testString, innerLambdaType, false);
				break;
			}
			
			return callable;
		}
		
		private SerializableCallable<String> getSerializableCallable(String testString, LAMBDA_TYPE outerLambdaType, LAMBDA_TYPE innerLambdaType) {
			SerializableCallable<String> callable = null;
			
			switch (outerLambdaType) {
			case PRIVATE_LAMBDA:
				callable = () -> (new ContainingClassCaller()).privateTestString(testString, innerLambdaType, true);
				break;
			case PROTECTED_LAMBDA:
				callable = () -> (new ContainingClassCaller()).protectedTestString(testString, innerLambdaType, true);
				break;
			case PACKAGE_LAMBDA:
				callable = () -> (new ContainingClassCaller()).packageTestString(testString, innerLambdaType, true);
				break;
			case PUBLIC_LAMBDA:
				callable = () -> (new ContainingClassCaller()).publicTestString(testString, innerLambdaType, true);
				break;
			}
			
			return callable;
		}
		
		/**
		 * Creates and executes a lambda which calls a method in ContainingClassCaller 
		 * and returns its result. The method called depends on the specified
		 * outerLambdaType. The SAM type depends on the value of useSerializable.
		 *  
		 * @param testString		The testString to pass into the containing class' method
		 * @param outerLambdaType	The accessibility rule of the method to call
		 * @param innerLambdaType	This argument is passed through as the ContainingClassCaller method's lambdaType  
		 * @param useSerializable	If true the lambda will be a SAM of the type SerializableCallable<String>,
		 * 							otherwise it will be a Callable<String>
		 * @return	The return value of the ContainingClassCaller's method or null if an error occurred
		 */
		private String privateTestString(String testString, LAMBDA_TYPE outerLambdaType, LAMBDA_TYPE innerLambdaType, boolean useSerializable) {
			Callable<String> callable = null;
			
			if (useSerializable) {
				callable = getSerializableCallable(testString, outerLambdaType, innerLambdaType);
			} else {
				callable = getCallable(testString, outerLambdaType, innerLambdaType);
			}

			String resultString = null;
			try {
				resultString = callable.call();
			} catch (Exception e) {
				e.printStackTrace();
			}

			return resultString;
		}
		
		/**
		 * This is a copy of @ref privateTestString() with the only difference
		 * being the method's visibility/accessibility.
		 */
		protected String protectedTestString(String testString, LAMBDA_TYPE outerLambdaType, LAMBDA_TYPE innerLambdaType, boolean useSerializable) {
			Callable<String> callable = null;
			
			if (useSerializable) {
				callable = getSerializableCallable(testString, outerLambdaType, innerLambdaType);
			} else {
				callable = getCallable(testString, outerLambdaType, innerLambdaType);
			}

			String resultString = null;
			try {
				resultString = callable.call();
			} catch (Exception e) {
				e.printStackTrace();
			}

			return resultString;
		}
		
		/**
		 * This is a copy of @ref privateTestString() with the only difference
		 * being the method's visibility/accessibility.
		 */
		String packageTestString(String testString, LAMBDA_TYPE outerLambdaType, LAMBDA_TYPE innerLambdaType, boolean useSerializable) {
			Callable<String> callable = null;
			
			if (useSerializable) {
				callable = getSerializableCallable(testString, outerLambdaType, innerLambdaType);
			} else {
				callable = getCallable(testString, outerLambdaType, innerLambdaType);
			}

			String resultString = null;
			try {
				resultString = callable.call();
			} catch (Exception e) {
				e.printStackTrace();
			}

			return resultString;
		}
		
		/**
		 * This is a copy of @ref privateTestString() with the only difference
		 * being the method's visibility/accessibility.
		 */
		public String publicTestString(String testString, LAMBDA_TYPE outerLambdaType, LAMBDA_TYPE innerLambdaType, boolean useSerializable) {
			Callable<String> callable = null;
			
			if (useSerializable) {
				callable = getSerializableCallable(testString, outerLambdaType, innerLambdaType);
			} else {
				callable = getCallable(testString, outerLambdaType, innerLambdaType);
			}

			String resultString = null;
			try {
				resultString = callable.call();
			} catch (Exception e) {
				e.printStackTrace();
			}

			return resultString;
		}

	}
	
	/**
	 * Helper method used by @ref testInnerToContainingClassCall() and
	 * @ref testSerializableInnerToContainingClassCall()
	 * @param useSerializable If true test a Serializable lambda, if false test a regular Callable
	 */
	private void innerToContainingClassCallHelper(boolean useSerializable) {
		ContainingClassCaller callerClass = new ContainingClassCaller();
		for (LAMBDA_TYPE lambdaType : LAMBDA_TYPE.values()) {
			String testString = "testInnerToContainingClassCall";
			String expectedString = getExpectedString(testString, lambdaType);
			
			/* public inner class method */
			String resultString = callerClass.publicTestString(testString, lambdaType, useSerializable);
			AssertJUnit.assertTrue("Call to ContainingClassCaller.publicTestString(\"" + testString + "\", " + lambdaType + ", " + useSerializable + ") did not return expected string."
					+ "\n\tExpected: " + expectedString
					+ "\n\tReturned: " + resultString,
					resultString.equals(expectedString));
			
			/* package inner class method */
			resultString = callerClass.packageTestString(testString, lambdaType, useSerializable);
			AssertJUnit.assertTrue("Call to ContainingClassCaller.packageTestString(\"" + testString + "\", " + lambdaType + ", " + useSerializable + ") did not return expected string."
					+ "\n\tExpected: " + expectedString
					+ "\n\tReturned: " + resultString,
					resultString.equals(expectedString));
			
			/* protected inner class method */
			resultString = callerClass.packageTestString(testString, lambdaType, useSerializable);
			AssertJUnit.assertTrue("Call to ContainingClassCaller.protectedTestString(\"" + testString + "\", " + lambdaType + ", " + useSerializable + ") did not return expected string."
					+ "\n\tExpected: " + expectedString
					+ "\n\tReturned: " + resultString,
					resultString.equals(expectedString));
			
			/* private inner class method */
			resultString = callerClass.privateTestString(testString, lambdaType, useSerializable);
			AssertJUnit.assertTrue("Call to ContainingClassCaller.privateTestString(\"" + testString + "\", " + lambdaType + ", " + useSerializable + ") did not return expected string."
					+ "\n\tExpected: " + expectedString
					+ "\n\tReturned: " + resultString,
					resultString.equals(expectedString));
		}
	}
	
	/**
	 * Test that a lambda in an inner class which calls methods
	 * with different accessibility rules in its containing class
	 * behaves as expected.
	 */
	@Test(groups = { "level.sanity" })
	public void testInnerToContainingClassCall() {
		innerToContainingClassCallHelper(false);
	}
	
	/**
	 * Test that a Serializable lambda in an inner class which calls methods
	 * with different accessibility rules in its containing class
	 * behaves as expected.
	 */
	@Test(groups = { "level.sanity" })
	public void testSerializableInnerToContainingClassCall() {
		innerToContainingClassCallHelper(true);
	}

	/**
	 * Helper method used by @ref testInnerToInnerClassCall() and
	 * @ref testSerializableInnerToInnerClassCall()
	 * @param useSerializable If true test a Serializable lambda, if false test a regular Callable
	 */
	private void innerToInnerClassCallHelper(boolean useSerializable) {
		InnerClassCaller callerClass = new InnerClassCaller();
		for (LAMBDA_TYPE outerLambdaType : LAMBDA_TYPE.values()) {
			for (LAMBDA_TYPE innerLambdaType : LAMBDA_TYPE.values()) {
				String testString = "testInnerToInnerClassCall";
				String expectedString = getExpectedString(testString, innerLambdaType);

				/* public inner class method */
				String resultString = callerClass.publicTestString(testString, outerLambdaType, innerLambdaType, useSerializable);
				AssertJUnit.assertTrue("Call to InnerClassCaller.publicTestString(\"" + testString + "\", " + outerLambdaType + ", " + innerLambdaType + ", " + useSerializable + ") did not return expected string."
						+ "\n\tExpected: " + expectedString
						+ "\n\tReturned: " + resultString,
						resultString.equals(expectedString));

				/* package inner class method */
				resultString = callerClass.packageTestString(testString, outerLambdaType, innerLambdaType, useSerializable);
				AssertJUnit.assertTrue("Call to InnerClassCaller.packageTestString(\"" + testString + "\", " + outerLambdaType + ", " + innerLambdaType + ", " + useSerializable + ") did not return expected string."
						+ "\n\tExpected: " + expectedString
						+ "\n\tReturned: " + resultString,
						resultString.equals(expectedString));

				/* protected inner class method */
				resultString = callerClass.packageTestString(testString, outerLambdaType, innerLambdaType, useSerializable);
				AssertJUnit.assertTrue("Call to InnerClassCaller.protectedTestString(\"" + testString + "\", " + outerLambdaType + ", " + innerLambdaType + ", " + useSerializable + ") did not return expected string."
						+ "\n\tExpected: " + expectedString
						+ "\n\tReturned: " + resultString,
						resultString.equals(expectedString));

				/* private inner class method */
				resultString = callerClass.privateTestString(testString, outerLambdaType, innerLambdaType, useSerializable);
				AssertJUnit.assertTrue("Call to InnerClassCaller.privateTestString(\"" + testString + "\", " + outerLambdaType + ", " + innerLambdaType + ", " + useSerializable + ") did not return expected string."
						+ "\n\tExpected: " + expectedString
						+ "\n\tReturned: " + resultString,
						resultString.equals(expectedString));
			}
		}
	}
	/**
	 * Test that a lambda in an inner class which calls methods
	 * with different accessibility rules in another inner class
	 * behaves as expected.
	 */
	@Test(groups = { "level.sanity" })
	public void testInnerToInnerClassCall() {
		innerToInnerClassCallHelper(false);
	}

	/**
	 * Test that a Serializable lambda in an inner class which calls methods
	 * with different accessibility rules in another inner class
	 * behaves as expected.
	 */
	@Test(groups = { "level.sanity" })
	public void testSerializableInnerToInnerClassCall() {
		innerToInnerClassCallHelper(true);
	}
}
