/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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
package org.openj9.test.jsr335.interfacePrivateMethod;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.reflect.Method;
import java.util.HashSet;
import java.util.Set;

import org.openj9.test.jsr335.interfacePrivateMethod.interfaces.J;
import org.openj9.test.jsr335.interfacePrivateMethod.interfaces.K;

@Test(groups = { "level.sanity" })
public class Test_ValidateReflectionWithExtensionMethods {
	
	final LoadInterfaceContainingPrivateMethod loadClass = new LoadInterfaceContainingPrivateMethod();

	static void printFailResults(Set<String> expectedResults, Method[] actualResults) {
		if (!expectedResults.isEmpty()){
			System.out.println("==========  Expected results still contains: ==========");
			for (String s : expectedResults) {
				System.out.println(s);
			}
			System.out.println("\n==========  Full actual results are: ==========");
			for (Method method : actualResults) {
				System.out.println(method.toGenericString());
			}
		}
	}
	
	/**
	 * Helper method that validates that getMethods() or getDeclaredMethods() returned the correct methods
	 *    
	 * @param expectedMethodsSet	set containing the expected result found by calling the method in question on Oracle JDK 
	 * @param myMethods				The Method[] array containing actual result methods returned by getMethods() or getDeclaredMethods()  
	 */
	private void checkMethods(Set<String> expectedMethodsSet, Method[] myMethods) {
		AssertJUnit.assertTrue("Failing as we found '" + myMethods.length 
				+ "' methods, but the expected number of methods was '" + expectedMethodsSet.size() + "'", 
				myMethods.length == expectedMethodsSet.size());
		
		for (Method method : myMethods) {
			String foundMethodName = method.toGenericString();
			
			String modifiedFoundMethodName = new String();
			if(foundMethodName.contains("native")) {
				modifiedFoundMethodName = foundMethodName.replaceFirst(" native ", " ").replaceFirst("native ", " ");
			} else {
				modifiedFoundMethodName = foundMethodName;
			}
			
			boolean foundMethod = expectedMethodsSet.remove(modifiedFoundMethodName);
			if (!foundMethod) {
				printFailResults(expectedMethodsSet, myMethods);
				Assert.fail("Failing as found unexpected method: " + modifiedFoundMethodName);
			}
		}
	}
	
	/*-------------------ACTUAL TESTS------------------*/
	
	/**
	 * Validates <code>Class.getMethods()</code> by comparing its result to the <code>expectedGetMethodsSet</code> (from Oracle JDK).
	 * For interface <code>J</code>, we expect only the public methods in interface <code>I</code>.
	 */
	@Test
	public void testGetMethodsOnJ() {
		Set<String> expectedGetMethodsSet = new HashSet<String>();
		expectedGetMethodsSet.add("public default void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.I.m()");
		expectedGetMethodsSet.add("public abstract void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.I.n()");
		
		checkMethods(expectedGetMethodsSet, J.class.getMethods());
	}
	
	/**
	 * Validates <code>Class.getDeclaredMethods()</code> by comparing its result to the <code>expectedGetDeclaredMethodsSet</code> 
	 * (from Oracle JDK).
	 * For interface <code>J</code>, we don't expect any methods as it does not declare any methods.
	 */
	@Test
	public void testGetDeclaredMethodsOnJ() {
		Set<String> expectedGetDeclaredMethodsSet = new HashSet<String>();
		
		checkMethods(expectedGetDeclaredMethodsSet, J.class.getDeclaredMethods());
	}
	
	/**
	 * Validates <code>Class.getMethods()</code> by comparing its result to the <code>expectedGetMethodsSet</code> (from Oracle JDK).
	 * For interface <code>K</code>, we expect the public methods in interfaces <code>I</code> and <code>K</code>.
	 */
	@Test
	public void testGetMethodsOnK() {
		Set<String> expectedGetMethodsSet = new HashSet<String>();
		expectedGetMethodsSet.add("public default void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.K.m()");
		expectedGetMethodsSet.add("public abstract void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.K.p()");
		expectedGetMethodsSet.add("public abstract void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.I.n()");
		
		checkMethods(expectedGetMethodsSet, K.class.getMethods());
	}
	
	/**
	 * Validates <code>Class.getDeclaredMethods()</code> by comparing its result to the <code>expectedGetDeclaredMethodsSet</code> 
	 * (from Oracle JDK).
	 * For interface <code>K</code>, we expect only the public methods in interface <code>K</code>.
	 */
	@Test
	public void testGetDeclaredMethodsOnK() {
		Set<String> expectedGetDeclaredMethodsSet = new HashSet<String>();
		expectedGetDeclaredMethodsSet.add("public default void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.K.m()");
		expectedGetDeclaredMethodsSet.add("public abstract void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.K.p()");
		
		checkMethods(expectedGetDeclaredMethodsSet, K.class.getDeclaredMethods());
	}
	
	/**
	 * Validates <code>Class.getMethods()</code> by comparing its result to the <code>expectedGetMethodsSet</code> (from Oracle JDK).
	 * For Class <code>D</code>, we expect the public methods in interface <code>I</code> and Class <code>Object</code> as 
	 * Class <code>D</code> does not declare any methods.
	 */
	@Test
	public void testGetMethodsOnD() throws Exception, LinkageError {
		Set<String> expectedGetMethodsSet = new HashSet<String>();
		expectedGetMethodsSet.add("public final void java.lang.Object.wait() throws java.lang.InterruptedException");
		expectedGetMethodsSet.add("public final void java.lang.Object.wait(long,int) throws java.lang.InterruptedException");
		expectedGetMethodsSet.add("public final void java.lang.Object.wait(long) throws java.lang.InterruptedException");
		expectedGetMethodsSet.add("public boolean java.lang.Object.equals(java.lang.Object)");
		expectedGetMethodsSet.add("public java.lang.String java.lang.Object.toString()");
		expectedGetMethodsSet.add("public int java.lang.Object.hashCode()");
		expectedGetMethodsSet.add("public final java.lang.Class<?> java.lang.Object.getClass()");
		expectedGetMethodsSet.add("public final void java.lang.Object.notify()");
		expectedGetMethodsSet.add("public final void java.lang.Object.notifyAll()");
		expectedGetMethodsSet.add("public default void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.I.m()");
		expectedGetMethodsSet.add("public abstract void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.I.n()");
		
		Class<?> D = Class.forName("D", true, loadClass);
		checkMethods(expectedGetMethodsSet, D.getMethods());
	}	
	
	/**
	 * Validates <code>Class.getDeclaredMethods()</code> by comparing its result to the <code>expectedGetDeclaredMethodsSet</code> 
	 * (from Oracle JDK).
	 * For Class <code>D</code>, we don't expect any methods as it does not declare any methods.
	 */
	@Test
	public void testGetDeclaredMethodsOnD() throws Exception, LinkageError {
		Set<String> expectedGetDeclaredMethodsSet = new HashSet<String>();
		
		Class<?> D = Class.forName("D", true, loadClass);
		checkMethods(expectedGetDeclaredMethodsSet, D.getDeclaredMethods());
	}
	
	/**
	 * Validates <code>Class.getMethods()</code> by comparing its result to the <code>expectedGetMethodsSet</code> (from Oracle JDK).
	 * For Class <code>E</code>, we expect the public methods in interfaces <code>I</code>, <code>L</code> and Class <code>Object</code> as 
	 * Class <code>E</code> does not declare any methods.
	 */
	/* Currently excluding tests from running in the builds until PR 70385 is resolved. 
	 * Causes assert failure with J9 in conflict case 
	 */
  	
	@Test
	public void testGetMethodsOnE() throws Exception, LinkageError {
		Set<String> expectedGetMethodsSet = new HashSet<String>();
		expectedGetMethodsSet.add("public final void java.lang.Object.wait() throws java.lang.InterruptedException");
		expectedGetMethodsSet.add("public final void java.lang.Object.wait(long,int) throws java.lang.InterruptedException");
		expectedGetMethodsSet.add("public final void java.lang.Object.wait(long) throws java.lang.InterruptedException");
		expectedGetMethodsSet.add("public boolean java.lang.Object.equals(java.lang.Object)");
		expectedGetMethodsSet.add("public java.lang.String java.lang.Object.toString()");
		expectedGetMethodsSet.add("public int java.lang.Object.hashCode()");
		expectedGetMethodsSet.add("public final java.lang.Class<?> java.lang.Object.getClass()");
		expectedGetMethodsSet.add("public final void java.lang.Object.notify()");
		expectedGetMethodsSet.add("public final void java.lang.Object.notifyAll()");
		expectedGetMethodsSet.add("public default void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.I.m()");
		expectedGetMethodsSet.add("public abstract void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.I.n()");
		expectedGetMethodsSet.add("public default void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.L.m()");
		
		Class<?> E = Class.forName("E", true, loadClass);
		checkMethods(expectedGetMethodsSet, E.getMethods());
	}
	
	/**
	 * Validates <code>Class.getDeclaredMethods()</code> by comparing its result to the <code>expectedGetDeclaredMethodsSet</code> 
	 * (from Oracle JDK).
	 * For Class <code>E</code>, we don't expect any methods as it does not declare any methods.
	 */
	@Test
	public void testGetDeclaredMethodsOnE() throws Exception, LinkageError {
		Set<String> expectedGetDeclaredMethodsSet = new HashSet<String>();
		
		Class<?> E = Class.forName("E", true, loadClass);
		checkMethods(expectedGetDeclaredMethodsSet, E.getDeclaredMethods());
	}
	
	/**
	 * Validates <code>Class.getMethods()</code> by comparing its result to the <code>expectedGetMethodsSet</code> (from Oracle JDK).
	 * For Class <code>G</code>, we expect the public methods in interfaces <code>O</code> and <code>I</code> and 
	 * Class <code>Object</code>, as Class <code>G</code> does not declare any methods.
	 */
	@Test
	public void testGetMethodsOnG() throws Exception, LinkageError {
		Set<String> expectedGetMethodsSet = new HashSet<String>();
		expectedGetMethodsSet.add("public final void java.lang.Object.wait() throws java.lang.InterruptedException");
		expectedGetMethodsSet.add("public final void java.lang.Object.wait(long,int) throws java.lang.InterruptedException");
		expectedGetMethodsSet.add("public final void java.lang.Object.wait(long) throws java.lang.InterruptedException");
		expectedGetMethodsSet.add("public boolean java.lang.Object.equals(java.lang.Object)");
		expectedGetMethodsSet.add("public java.lang.String java.lang.Object.toString()");
		expectedGetMethodsSet.add("public int java.lang.Object.hashCode()");
		expectedGetMethodsSet.add("public final java.lang.Class<?> java.lang.Object.getClass()");
		expectedGetMethodsSet.add("public final void java.lang.Object.notify()");
		expectedGetMethodsSet.add("public final void java.lang.Object.notifyAll()");
		expectedGetMethodsSet.add("public default void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.O.m()");
		expectedGetMethodsSet.add("public abstract void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.I.n()");
		
		Class<?> G = Class.forName("G", true, loadClass);
		checkMethods(expectedGetMethodsSet, G.getMethods());
	}
	
	/**
	 * Validates <code>Class.getDeclaredMethods()</code> by comparing its result to the <code>expectedGetDeclaredMethodsSet</code> 
	 * (from Oracle JDK).
	 * For Class <code>G</code>, we don't expect any methods as it does not declare any methods.
	 */
	@Test
	public void testGetDeclaredMethodsOnG() throws Exception, LinkageError {
		Set<String> expectedGetDeclaredMethodsSet = new HashSet<String>();
		
		Class<?> G = Class.forName("G", true, loadClass);
		checkMethods(expectedGetDeclaredMethodsSet, G.getDeclaredMethods());
	}
	
	/**
	 * Validates <code>Class.getMethods()</code> by comparing its result to the <code>expectedGetMethodsSet</code> (from Oracle JDK).
	 * For Class <code>F</code>, we expect the public methods in interfaces <code>I</code> and <code>N</code> and 
	 * Class <code>Object</code>, as Class <code>F</code> does not declare any methods.
	 */
	@Test
	public void testGetMethodsOnF() throws Exception, LinkageError {
		Set<String> expectedGetMethodsSet = new HashSet<String>();
		expectedGetMethodsSet.add("public final void java.lang.Object.wait() throws java.lang.InterruptedException");
		expectedGetMethodsSet.add("public final void java.lang.Object.wait(long,int) throws java.lang.InterruptedException");
		expectedGetMethodsSet.add("public final void java.lang.Object.wait(long) throws java.lang.InterruptedException");
		expectedGetMethodsSet.add("public boolean java.lang.Object.equals(java.lang.Object)");
		expectedGetMethodsSet.add("public java.lang.String java.lang.Object.toString()");
		expectedGetMethodsSet.add("public int java.lang.Object.hashCode()");
		expectedGetMethodsSet.add("public final java.lang.Class<?> java.lang.Object.getClass()");
		expectedGetMethodsSet.add("public final void java.lang.Object.notify()");
		expectedGetMethodsSet.add("public final void java.lang.Object.notifyAll()");
		expectedGetMethodsSet.add("public default void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.I.m()");
		expectedGetMethodsSet.add("public abstract void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.I.n()");
		expectedGetMethodsSet.add("public abstract void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.N.m()");
		
		Class<?> F = Class.forName("F", true, loadClass);
		checkMethods(expectedGetMethodsSet, F.getMethods());
	}
	
	/**
	 * Validates <code>Class.getDeclaredMethods()</code> by comparing its result to the <code>expectedGetDeclaredMethodsSet</code> 
	 * (from Oracle JDK).
	 * For Class <code>F</code>, we don't expect any methods as it does not declare any methods.
	 */
	@Test
	public void testGetDeclaredMethodsOnF() throws Exception, LinkageError {
		Set<String> expectedGetDeclaredMethodsSet = new HashSet<String>();
		
		Class<?> F = Class.forName("F", true, loadClass);
		checkMethods(expectedGetDeclaredMethodsSet, F.getDeclaredMethods());
	}

	/**
	 * Validates <code>Class.getMethods()</code> by comparing its result to the <code>expectedGetMethodsSet</code> (from Oracle JDK).
	 * For Class <code>C</code>, we expect the public methods in interface <code>I</code> and Classes <code>C</code> and <code>Object</code>.
	 */
	@Test
	public void testGetMethodsOnC() throws Exception, LinkageError {
		Set<String> expectedGetMethodsSet = new HashSet<String>();
		expectedGetMethodsSet.add("public void C.m()");
		expectedGetMethodsSet.add("public final void java.lang.Object.wait() throws java.lang.InterruptedException");
		expectedGetMethodsSet.add("public final void java.lang.Object.wait(long,int) throws java.lang.InterruptedException");
		expectedGetMethodsSet.add("public final void java.lang.Object.wait(long) throws java.lang.InterruptedException");
		expectedGetMethodsSet.add("public boolean java.lang.Object.equals(java.lang.Object)");
		expectedGetMethodsSet.add("public java.lang.String java.lang.Object.toString()");
		expectedGetMethodsSet.add("public int java.lang.Object.hashCode()");
		expectedGetMethodsSet.add("public final java.lang.Class<?> java.lang.Object.getClass()");
		expectedGetMethodsSet.add("public final void java.lang.Object.notify()");
		expectedGetMethodsSet.add("public final void java.lang.Object.notifyAll()");
		expectedGetMethodsSet.add("public abstract void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.I.n()");
		
		Class<?> C = Class.forName("C", true, loadClass);
		checkMethods(expectedGetMethodsSet, C.getMethods());
	}
	
	/**
	 * Validates <code>Class.getDeclaredMethods()</code> by comparing its result to the <code>expectedGetDeclaredMethodsSet</code> 
	 * (from Oracle JDK).
	 * For Class <code>C</code>, we expect only the public methods in Class <code>C</code>.
	 */
	@Test
	public void testGetDeclaredMethodsOnC() throws Exception, LinkageError {
		Set<String> expectedGetDeclaredMethodsSet = new HashSet<String>();
		expectedGetDeclaredMethodsSet.add("public void C.m()");
		
		Class<?> C = Class.forName("C", true, loadClass);
		checkMethods(expectedGetDeclaredMethodsSet, C.getDeclaredMethods());
	}

	/**
	 * Validates <code>Class.getMethods()</code> by comparing its result to the <code>expectedGetMethodsSet</code> (from Oracle JDK).
	 * For Class <code>H</code>, we expect the public methods in interfaces <code>I</code> and <code>L</code> and 
	 * Classes <code>H</code> and <code>Object</code>.
	 */
	@Test
	public void testGetMethodsOnH() throws Exception, LinkageError {
		Set<String> expectedGetMethodsSet = new HashSet<String>();
		expectedGetMethodsSet.add("public void H.m()");
		expectedGetMethodsSet.add("public final void java.lang.Object.wait() throws java.lang.InterruptedException");
		expectedGetMethodsSet.add("public final void java.lang.Object.wait(long,int) throws java.lang.InterruptedException");
		expectedGetMethodsSet.add("public final void java.lang.Object.wait(long) throws java.lang.InterruptedException");
		expectedGetMethodsSet.add("public boolean java.lang.Object.equals(java.lang.Object)");
		expectedGetMethodsSet.add("public java.lang.String java.lang.Object.toString()");
		expectedGetMethodsSet.add("public int java.lang.Object.hashCode()");
		expectedGetMethodsSet.add("public final java.lang.Class<?> java.lang.Object.getClass()");
		expectedGetMethodsSet.add("public final void java.lang.Object.notify()");
		expectedGetMethodsSet.add("public final void java.lang.Object.notifyAll()");
		expectedGetMethodsSet.add("public abstract void org.openj9.test.jsr335.interfacePrivateMethod.interfaces.I.n()");
		
		Class<?> H = Class.forName("H", true, loadClass);
		checkMethods(expectedGetMethodsSet, H.getMethods());
	}
	
	/**
	 * Validates <code>Class.getDeclaredMethods()</code> by comparing its result to the <code>expectedGetDeclaredMethodsSet</code> 
	 * (from Oracle JDK).
	 * For Class <code>H</code>, we expect only the public methods in Class <code>H</code>.
	 */
	@Test
	public void testGetDeclaredMethodsOnH() throws Exception, LinkageError {
		Set<String> expectedGetDeclaredMethodsSet = new HashSet<String>();
		expectedGetDeclaredMethodsSet.add("public void H.m()");
		
		Class<?> H = Class.forName("H", true, loadClass);
		checkMethods(expectedGetDeclaredMethodsSet, H.getDeclaredMethods());
	}
}
