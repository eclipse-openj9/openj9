package com.ibm.j9.offload.tests.invcost;

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

public class TestNativesInv {
	static final String NATIVE_LIBRARY_NAME = "j9offjnitest26";
	static boolean libraryLoaded = false;
	
	/* fields used for sum value perf measurements */
	public int suma, sumb, sumc, sumd, sume, sumf;

	/* natives for invocation cost tests */
	public native int doNothing();
	public native int doNothing2(Object obj1, Object obj2);
	public native int doNothing3(Object obj1, Object obj2, Object obj3, Object obj4, Object obj5);
	public native int doCallback();
	public native int sumValues(int a, int b, int c, int d, int e, int f);
	public native int sumValues2();
	public static native long getElement(long[] array,int element);
	public static native long getElement2(long[] array,int element);
	
	public int doNothingInJava(){
		return 1;
	}
	
	public int doNothingInJava2(Object obj1, Object obj2){
		return 1;
	}
	
	public int doNothingInJava3(Object obj1, Object obj2, Object obj3, Object obj4, Object obj5){
		return 1;
	}
	
	
	public interface testInterface {
		public void method1();
	}
	
	public class testClass {
		public void method1() {};
	}
	
	/**
	 * make sure the native library required to run the natives is loaded
	 */
	public void ensureLibraryLoaded(){
		if (libraryLoaded == false){
			System.loadLibrary(NATIVE_LIBRARY_NAME);
			libraryLoaded = true;
		}
	}
	
	/**
	 * constructor
	 */	
	public TestNativesInv() {
		ensureLibraryLoaded();
	}	

	/**
	 * constructor
	 * @param intValue the intValue to be assigned to the intField that is looked up by the native
	 */
	public TestNativesInv(int intValue){
		ensureLibraryLoaded();
	}
}
