/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package com.ibm.jvmti.tests.modularityTests;

import java.lang.Module;
import java.util.HashSet;
import java.util.Set;

import jdk.internal.module.Modules;

public class mt001 {
	private native static int addModuleReads(Object from, Object to);
	private native static int addModuleExports(Object from, String pkgname, Object to);
	private native static int addModuleOpens(Object from, String pkgname, Object to);
	private native static int addModuleUses(Object module, Object service);
	private native static int addModuleProvides(Object module, Object service, Object implClass);
	
	static final int NULL_POINTER_ERROR = 100;
	static final int NO_ERROR = 0;
	static final int INVALID_MODULE = 26;
	static final int INVALID_CLASS = 21;
	static final int ILLEGAL_ARG = 103;
	
	Module m1 = null;
	Module m2 = null;
	
	static boolean success = true; 
	
	public boolean setup(String args) {
		Set<String> pkgs1 = new HashSet<String>();
		Set<String> pkgs2 = new HashSet<String>();
		
		pkgs1.add("com.j91");
		pkgs1.add("com.j92");
		pkgs2.add("com.j93");
		pkgs2.add("com.j94");
				
		m1 = Modules.defineModule(mt001.class.getClassLoader(), "m1", pkgs1);
		m2 = Modules.defineModule(mt001.class.getClassLoader(), "m2", pkgs2);
		
		return true;
	}
	
	private void assertFalse(boolean condition) {
		if (condition) {
			System.err.print("Expected FALSE instead got TRUE");
			try {
				throw new Exception("Test Failure");
			} catch (Exception e) {
				e.printStackTrace();
			}
			success = false;
		}
	}
	
	private void assertTrue(boolean condition) {
		if (!condition) {
			System.err.print("Expected TRUE instead got FALSE");
			try {
				throw new Exception("Test Failure");
			} catch (Exception e) {
				e.printStackTrace();
			}
			success = false;
		}
	}
	
	private void assertEquals(int expected, int actual) {
		if (expected != actual) {
			System.err.print("Expected " + expected + " instead got " + actual);
			try {
				throw new Exception("Test Failure");
			} catch (Exception e) {
				e.printStackTrace();
			}
			success = false;
		}
	}
	
	public void helpAddExports() {
		System.out.println("============ Add Exports Test ================");
	}
	
	public boolean testAddExports() {		
		success = true;
		
		assertFalse(m1.isExported("com.j91"));
		assertEquals(NO_ERROR, addModuleExports(m1, "com.j91", m2));
		assertTrue(m1.isExported("com.j91", m2));
		
		//NULL test
		assertEquals(NULL_POINTER_ERROR, addModuleExports(m1, null, m2));
		assertEquals(NULL_POINTER_ERROR, addModuleExports(null, null, m2));
		assertEquals(NULL_POINTER_ERROR, addModuleExports(null, null, null));
		assertEquals(NULL_POINTER_ERROR, addModuleExports(null, "com.j91", null));
		
		//invalid module test
		assertEquals(INVALID_MODULE, addModuleExports(m1, "com.j91", new Object()));
		assertEquals(INVALID_MODULE, addModuleExports(new Object(), "com.j91", m2));
		assertEquals(INVALID_MODULE, addModuleExports(new Object(), "com.j91", new Object()));
		
		//invalid package test
		assertEquals(ILLEGAL_ARG, addModuleExports(m1, "com.j9as1", m2));
		assertEquals(ILLEGAL_ARG, addModuleExports(m1, "com/j91", m2));
		assertEquals(ILLEGAL_ARG, addModuleExports(m1, "com.j91.", m2));
		
		//from unnamed module
		Module unnamedModule = mt001.class.getClassLoader().getUnnamedModule();
		assertEquals(INVALID_MODULE, addModuleExports(unnamedModule, "com.j91", new Object()));
		assertEquals(NO_ERROR, addModuleExports(unnamedModule, "com.j9as1", m2));
		assertEquals(NULL_POINTER_ERROR, addModuleExports(unnamedModule, "com.j91", null));
		
		//to Unnamed
		assertFalse(m1.isExported("com.j91"));
		assertEquals(NO_ERROR, addModuleExports(m1, "com.j91", unnamedModule));
		assertTrue(m1.isExported("com.j91", unnamedModule));
		
		return success;
	}
	
	public void helpAddOpens() {
		System.out.println("============ Add Opens Test ================");
	}
	
	/* TODO this test will have to be updated once b149 is available */
	public boolean testAddOpens() {
		success = true;
		
//		assertFalse(m1.isOpen("com.j91"));
//		assertEquals(NO_ERROR, addModuleOpens(m1, "com.j91", m2));
//		assertTrue(m1.isOpen("com.j91", m2));
		
		//NULL test
		assertEquals(NULL_POINTER_ERROR, addModuleOpens(m1, null, m2));
		assertEquals(NULL_POINTER_ERROR, addModuleOpens(null, null, m2));
		assertEquals(NULL_POINTER_ERROR, addModuleOpens(null, null, null));
		assertEquals(NULL_POINTER_ERROR, addModuleOpens(null, "com.j91", null));
		
		//invalid module test
		assertEquals(INVALID_MODULE, addModuleOpens(m1, "com.j91", new Object()));
		assertEquals(INVALID_MODULE, addModuleOpens(new Object(), "com.j91", m2));
		assertEquals(INVALID_MODULE, addModuleOpens(new Object(), "com.j91", new Object()));
		
		//invalid package test
		assertEquals(ILLEGAL_ARG, addModuleOpens(m1, "com.j9as1", m2));
		assertEquals(ILLEGAL_ARG, addModuleOpens(m1, "com/j91", m2));
		assertEquals(ILLEGAL_ARG, addModuleOpens(m1, "com.j91.", m2));
		
		//from unnamed module
		Module unnamedModule = mt001.class.getClassLoader().getUnnamedModule();
		assertEquals(INVALID_MODULE, addModuleOpens(unnamedModule, "com.j91", new Object()));
//		assertEquals(NO_ERROR, addModuleOpens(unnamedModule, "com.j9as1", m2));
		assertEquals(NULL_POINTER_ERROR, addModuleOpens(unnamedModule, "com.j91", null));
		
		//to Unnamed
//		assertFalse(m1.isOpen("com.j91"));
//		assertEquals(NO_ERROR, addModuleOpens(m1, "com.j91", unnamedModule));
//		assertTrue(m1.isOpen("com.j91", unnamedModule));
		
		return success;
	}
	
	public void helpAddReads() {
		System.out.println("============ Add Reads Test ================");
	}
	
	public boolean testAddReads() {
		success = true;
		
		//addReads
		assertFalse(m1.canRead(m2));
		assertEquals(NO_ERROR, addModuleReads(m1, m2));
		assertTrue(m1.canRead(m2));
				
		//NULL test
		assertEquals(NULL_POINTER_ERROR, addModuleReads(null, m2));
		assertEquals(NULL_POINTER_ERROR, addModuleReads(m1, null));
		assertEquals(NULL_POINTER_ERROR, addModuleReads(null, null));
		
		//invalid module test
		assertEquals(INVALID_MODULE, addModuleReads(m1, new Object()));
		assertEquals(INVALID_MODULE, addModuleReads(new Object(), m2));
		assertEquals(INVALID_MODULE, addModuleReads(new Object(), new Object()));
		
		//from unnamed module
		Module unnamedModule = mt001.class.getClassLoader().getUnnamedModule();
		assertEquals(INVALID_MODULE, addModuleReads(unnamedModule, new Object()));
		assertEquals(NO_ERROR, addModuleReads(unnamedModule, m2));
		assertEquals(NULL_POINTER_ERROR, addModuleReads(unnamedModule, null));
		
		return success;
	}
	
	public void helpAddUses() {
		System.out.println("============ Add Uses Test ================");
	}
	
	/* TODO this test will have to be updated once b149 is available */
	public boolean testAddUses() {
		Class<?> service = TestService.class;
		
		success = true;
		
		//NULL test
		assertEquals(NULL_POINTER_ERROR, addModuleUses(null, service));
		assertEquals(NULL_POINTER_ERROR, addModuleUses(m1, null));
		assertEquals(NULL_POINTER_ERROR, addModuleUses(null, null));
		
		//invalid module and class test
		assertEquals(INVALID_CLASS, addModuleUses(m1, new Object()));
		assertEquals(INVALID_MODULE, addModuleUses(new Object(), service));
		
		return success;
		
	}
	
	public void helpAddProvides() {
		System.out.println("============ Add Provides Test ================");
	}
	
	/* TODO this test will have to be updated once b149 is available */
	public boolean testAddProvides() {
		Class<?> service = TestService.class;
		Class<?> implClass = TestImplService.class;
		success = true;
		
		//NULL test
		assertEquals(NULL_POINTER_ERROR, addModuleProvides(null, service, implClass));
		assertEquals(NULL_POINTER_ERROR, addModuleProvides(m1, null, implClass));
		assertEquals(NULL_POINTER_ERROR, addModuleProvides(null, null, implClass));
		assertEquals(NULL_POINTER_ERROR, addModuleProvides(null, null, null));
		
		//invalid module and class test
		assertEquals(INVALID_CLASS, addModuleProvides(m1, new Object(), implClass));
		assertEquals(INVALID_CLASS, addModuleProvides(m1, service, new Object()));
		assertEquals(INVALID_MODULE, addModuleProvides(new Object(), service, implClass));
		
		return success;
		
	}
	
	interface TestService {};
	
	class TestImplService implements TestService {};
}
