/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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
package jit.test.tr.newinstance;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity","component.jit" })
public class NewInstanceTests {

	/**
	 *
	 */
	private static Logger logger = Logger.getLogger(NewInstanceTests.class);
	
	private static Object newInstance(String s) throws Throwable {
		return Class.forName(s).newInstance();
	}
	
	/**
	 * Make sure that an IllegalAccessException is thrown when trying to create a 
	 * new instance of a class in some other package with a protected constructor 
	 */
	@Test
	public void test001() {		
		int i = 0;
		try {
			newInstance("jit.test.tr.newinstance.pkg.ProtCtor");			
		} 
		catch (IllegalAccessException e) { i++; }
		catch (Throwable t) { logger.error("Got Exception: " + t); Assert.fail(); }
		AssertJUnit.assertEquals("Expected Exception not thrown", i,1);
	}
	
	/**
	 * Attempt to create a new instance of a Inner class should raise an instantiation
	 * exception, since the default constructor does not exist (all constructors must take an outer
	 * instance)
	 */
	@Test
	public void test002() {		
		int i = 0;
		try {
			newInstance("jit.test.tr.newinstance.NewInstanceTests$Inner");			
		} 
		catch (InstantiationException e) { i++; }
		catch (Throwable t) { logger.error("Got Exception: " + t); Assert.fail(); }
		AssertJUnit.assertEquals("Expected Exception not thrown", i,1);
	}
		
	/**
	 * Attempt to create a new instance of a static inner class should pass 
	 */
	@Test
	public void test003() {		
		try {
			newInstance("jit.test.tr.newinstance.NewInstanceTests$StaticInner");			
		} 		
		catch (Throwable t) { logger.error("Got Exception: " + t); Assert.fail(); }		
	}
		
	/**
	 * Attempt to create an instance of a static class inner class with a protected ctor
	 * in another package should fail with an IllegalAccessException 
	 */
	@Test
	public void test004() {
		int i = 0;
		try {
			newInstance("jit.test.tr.newinstance.pkg.PublicCtor$ProtectedInner");			
		}
		catch (IllegalAccessException e) { i++; }
		catch (Throwable t) { logger.error("Got Exception: " + t); Assert.fail(); }
		AssertJUnit.assertEquals("Expected Exception not thrown",i,1);
	}
	
	/**
	 * Attempt to create a new instance of a static inner class with a public constructor
	 * of a class in another package should fail with an illegal access exception
	 */
	@Test
	public void test005() {		
		int i = 0;		
		try {
			newInstance("jit.test.tr.newinstance.pkg.PublicCtor$PublicInner");				
		} 		
		catch (IllegalAccessException e) { i++; } 		
		catch (Throwable t) { logger.error("Got Exception: " + t); Assert.fail(); }
		AssertJUnit.assertEquals("Expected Exception not thrown",i,1);		
	}
	
	/**
	 * Attempt to create a new instance of a static inner class should pass with a private
	 * constructor by itself should succeed 
	 */
	@Test
	public void test006() {
		Test006.impl006(); Test006.impl006();
	}
	
	static class Test006 {
		private Test006() {}
		static void impl006() {
			try {
				Class.forName("jit.test.tr.newinstance.NewInstanceTests$Test006").newInstance();			
			} 		
			catch (Throwable t) { logger.error("Got Exception: " + t); Assert.fail(); }		
		}
	}
	
	/**
	 * Attempt to create a new instance of an interface should fail with an 
	 * InstantiationException
	 */
	@Test
	public void test007() {		
		int i = 0;		
		try {
			newInstance("jit.test.tr.newinstance.pkg.Interface");		
		} 		
		catch (InstantiationException e) { i++; } 		
		catch (Throwable t) { logger.error("Got Exception: " + t); Assert.fail(); }
		AssertJUnit.assertEquals("Expected Exception not thrown",i,1);		
	}
	
	/**
	 * Attempt to create a new instance of an Abstract class should fail with an
	 * InstantiationException
	 */
	@Test
	public void test008() {		
		int i = 0;		
		try {
			newInstance("jit.test.tr.newinstance.pkg.Abstract");			
		} 		
		catch (InstantiationException e) { i++; } 		
		catch (Throwable t) { logger.error("Got Exception: " + t); Assert.fail(); }
		AssertJUnit.assertEquals("Expected Exception not thrown",i,1);		
	}
	
	/**
	 * Attempt to create a new instance of an array class should fail with 
	 * an InstantiationException
	 */
	@Test
	public void test009() {		
		int i = 0;
		int[] A = new int[10];
		try {
			A.getClass().newInstance();
			Assert.fail();			
		} 		
		catch (InstantiationException e) { i++; } 		
		catch (Throwable t) { logger.error("Got Exception: " + t); Assert.fail(); }
		AssertJUnit.assertEquals("Expected Exception not thrown",i,1);		
	}
		
	/**
	 * Attempt to create a new instance of a primitive type should result
	 * in an InstantiationException
	 */
	@Test
	public void test010() {
		impl010(); impl010();
	}
	
	private void impl010() {	
		int i = 0;
		int[] A = new int[10];
		try {
			A.getClass().getComponentType().newInstance();
		} 		
		catch (InstantiationException e) { i++; } 		
		catch (Throwable t) { logger.error("Got Exception: " + t); Assert.fail(); }
		AssertJUnit.assertEquals("Expected Exception not thrown",i,1);		
	}	
	
	/**
	 * If the instantiation fails, the correctly catch the exception thrown by the 
	 * initializer 
	 */
	@Test
	public void test011() {		
		int i = 0;		
		try {
			newInstance("jit.test.tr.newinstance.NewInstanceTests$ErrorInInit");			
		} 		
		catch (StupidException e) { i++; }
		catch (Throwable t) { logger.error("Got Exception: " + t); Assert.fail(); }
		AssertJUnit.assertEquals("Expected Exception not thrown",i,1);		
	}
	
	/**
	 * Attempting to create a new instance of a class with a clinit error should result
	 * in an ExceptionInInitializerError the first time its attempted, and a 
	 * NoClassDefFoundError every other time afterwards.
	 */
	@Test
	public void test012() {
		int i = 0;
		try {
			newInstance("jit.test.tr.newinstance.NewInstanceTests$ErrorInClinit");			
		}
		catch (ExceptionInInitializerError e) {
			ArithmeticException ae = (ArithmeticException) e.getException();			
			i++;
		}
		catch (Throwable t) { logger.error("Got Exception: " + t); Assert.fail(); }
		AssertJUnit.assertEquals("Expected Exception not thrown",i,1);
		
		try {
			newInstance("jit.test.tr.newinstance.NewInstanceTests$ErrorInClinit");		
		}
		catch (NoClassDefFoundError f) { i++; }
		catch (Throwable t) { logger.error("Got Exception: " + t); Assert.fail(); }
		AssertJUnit.assertEquals("Expected Exception not thrown",i,2);		
	}
	
	// Anonymous inner class below, will be named $1 by javac
	static Object o = new Object() {int foo = 1;};	
	@Test
	public void test013() {		
		try {
			newInstance("jit.test.tr.newinstance.NewInstanceTests$1");			
		}
		catch (Throwable t) { logger.error("Got Exception: " + t); Assert.fail(); }
	}
	
	@Test
	public void test014() {
		Test014.impl(); Test014.impl();
	}
	static class Test014 {
		static public void impl() {
			try {
				Class.forName("jit.test.tr.newinstance.NewInstanceTests$1").newInstance();
			}
			catch (Throwable t) { logger.error("Got Exception: "+ t); Assert.fail(); }			
		}
	}

    public volatile Object vollyObjecty;
    @Test
    public void test015() {
        vollyObjecty = new Object[0]; // force the load of the class
        // make a escape to prevent escape analysis from happening
        impl015("[Z");
        impl015("[I");
        impl015(vollyObjecty.getClass().getName());
        impl015("[[[[[[[[[[J");
    }

    private void impl015(String name) {
        int i = 0;
        try {
            newInstance(name);
        }
        catch (InstantiationException e) { i++; }
        catch (Throwable t) { logger.error("Got Exception: "+t); Assert.fail(); }
        AssertJUnit.assertEquals("Expected Exception not thrown", i, 1);
    }

	
		
	class Inner { Inner() {} }
	
	static class StaticInner { protected StaticInner() {} }
	
	static class StupidException extends Exception {}
	
	static class ErrorInInit { protected ErrorInInit() throws StupidException { throw new StupidException(); } }
		
	static class ErrorInClinit {
		static boolean TRUE = true;
		static int z = 0;
		static { if (TRUE) z=1/z; }
		public ErrorInClinit () {}
	}
		 
}
