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
package com.ibm.jvmti.tests.eventVMObjectAllocation;

public class evmoa001 
{
	native static boolean didTestRun();
	
	public String helpVMDidBootstrap()
	{
		return "Test that the VM still managed to start even though we tried to follow reference from a freshly allocated yet uninitialized java.lang.Class object.";
	}
	
	public boolean testVMDidBootstrap()
	{
		//if we got this far, the test succeeded (since it is all JVMTI-side and a failure in the test would be a crash before main)
		//just call into the native to ensure that we actually _did_ run the test
		return didTestRun();
	}
}
