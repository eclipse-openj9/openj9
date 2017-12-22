package com.ibm.j9.offload.tests.jniservice;

/*******************************************************************************
 * Copyright (c) 2009, 2012 IBM Corp. and others
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

public class TestNativesRegisterNatives {
	static final String NATIVE_LIBRARY_NAME1 = "j9offjnitest26";
	static final String NATIVE_LIBRARY_NAME2 = "j9offjnitestb26";
	static boolean librariesLoaded = false;
	
	/**
	 * make sure the native library required to run the natives is loaded
	 */
	public void ensureLibraryLoaded(){
		if (librariesLoaded == false){
			System.loadLibrary(NATIVE_LIBRARY_NAME1);
			System.loadLibrary(NATIVE_LIBRARY_NAME2);
			librariesLoaded = true;
		}
	}
	
	/**
	 * constructor
	 */	
	public TestNativesRegisterNatives() {
		ensureLibraryLoaded();
	}	
	
	
	/** 
	 * non native method use to validate we cannot register a native on top of it
	 */
	public int nonNative(){
		return 10;
	}
	
	
	/**
	 * natives
	 */
	public native int unregisterNatives();
	public native int registerWithInvalidMethodName();
	public native int registerWithNonNativeMethodName();
	
	public native int registerNativesReturn1ValueInLibrary1();
	public native int registerNativesReturn1ValueInLibrary2();
	public native int registerNativesReturn1ValueInLibrary2SeparateThread();
	
	public native int registerNativesReturn2ValueInLibrary1();
	public native int registerNativesReturn2ValueInLibrary2();
	public native int registerNativesReturn2ValueInLibrary2SeparateThread();
	
	public native int getValueRegisteredNativeLibrary1();
	public native int getValueRegisteredNativeLibrary2();
	
	public native int getValueRegisteredNativeLibrary1NoDefault();
	public native int getValueRegisteredNativeLibrary2NoDefault();
	
	
}
