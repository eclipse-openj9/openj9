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
package com.ibm.jvmti.tests.vmDump;

public class vmd001 
{
	public boolean testVmDump()
	{
		boolean rc = false;
		
		System.out.println("testResetVmDump");
		System.out.println("call tryResetVmDump");
		rc = tryResetVmDump();
		if (rc == true) {
			System.out.println("success [tryResetVmDump]");
		} else {
			System.out.println("error [tryResetVmDump]");
		}
		System.out.println("done testResetVmDump");

		
		System.out.println("testQueryVmDump");
		System.out.println("call tryQueryDumpSmallBuffer");
		rc = tryQueryDumpSmallBuffer();
		if (rc == true) {
			System.out.println("success [tryQueryDumpSmallBuffer]");
		} else {
			System.out.println("error [tryQueryDumpSmallBuffer]");
		}

		System.out.println("call tryQueryDumpBigBuffer");
		if (rc == true) {
			System.out.println("success [tryQueryDumpBigBuffer]");
		} else {
			System.out.println("error [tryQueryDumpBigBuffer]");
		}

		System.out.println("call tryQueryDumpInvalidBufferSize");
		rc = tryQueryDumpInvalidBufferSize();
		if (rc == true) {
			System.out.println("success [tryQueryDumpInvalidBufferSize]");
		} else {
			System.out.println("error [tryQueryDumpInvalidBufferSize]");
		}

		System.out.println("call tryQueryDumpInvalidBuffer");
		rc = tryQueryDumpInvalidBuffer();
		if (rc == true) {
			System.out.println("success [tryQueryDumpInvalidBuffer]");
		} else {
			System.out.println("error [tryQueryDumpInvalidBuffer]");
		}

		System.out.println("call tryQueryDumpInvalidDataSize");
		rc = tryQueryDumpInvalidDataSize();
		if (rc == true) {
			System.out.println("success [tryQueryDumpInvalidDataSize]");
		} else {
			System.out.println("error [tryQueryDumpInvalidDataSize]");
		}

		System.out.println("done testQueryVmDump");

		System.out.println("testSetVmDump");
		System.out.println("call trySetVmDump");
		rc = trySetVmDump();
		if (rc == true) {
			System.out.println("success [trySetVmDump]");
		} else {
			System.out.println("error [trySetVmDump]");
		}
		System.out.println("done testSetVmDump");
		
		System.out.println("testDisableVmDump");
		System.out.println("call tryDisableVmDump");
		rc = tryDisableVmDump();
		if (rc == true) {
			System.out.println("success [tryDisableVmDump]");
		} else {
			System.out.println("error [tryDisableVmDump]");
		}
		System.out.println("done testDisableVmDump");

		return rc;
	}

	public String helpVmDump()
	{
		return "Test the jvmti VmDump extension APIs";
	}

	static public native boolean tryQueryDumpSmallBuffer();
	static public native boolean tryQueryDumpBigBuffer();
	static public native boolean tryQueryDumpInvalidBufferSize();
	static public native boolean tryQueryDumpInvalidBuffer();
	static public native boolean tryQueryDumpInvalidDataSize();

	static public native boolean trySetVmDump();
	static public native boolean tryResetVmDump();
	static public native boolean tryDisableVmDump();
}
