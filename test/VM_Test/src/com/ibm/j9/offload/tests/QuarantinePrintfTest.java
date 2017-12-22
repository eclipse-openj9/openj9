package com.ibm.j9.offload.tests;
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


/**
 * This test class simply calls a native that uses j9tty_printf to output
 * 
 * 			**** OUTPUT FROM NATIVE ****
 * 
 * so that we can validate that output is displayed when quarantines are run
 * out of process
 */
public class QuarantinePrintfTest {
	static final String NATIVE_LIBRARY_NAME = "j9offjnitest26";
	public static native int testPrintfOutput(int value);
	
	public static void main(String[] args){
		System.loadLibrary(NATIVE_LIBRARY_NAME);
		testPrintfOutput(0);
		/* provide enough time to ensure output comes out */
		try {
			Thread.sleep(1000);
		} catch (InterruptedException e) {
		}
	}
}


