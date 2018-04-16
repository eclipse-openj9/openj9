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
package com.ibm.jvmti.tests.util;
import com.sun.tools.attach.VirtualMachine;

class attachingAgent extends Thread {
	VirtualMachine selfVm = null;
	String subtest = null;

	attachingAgent (String subtest) {
		this.subtest = subtest;
	}

	// override run() method in interface
	public void run() {
		try {
			String myVmid = com.ibm.tools.attach.target.AttachHandler.getVmId();
			selfVm = VirtualMachine.attach(myVmid);
			selfVm.loadAgentLibrary("jvmtitest", "test:" + subtest);
		} catch (Exception e){
			System.err.println(Thread.currentThread() + "throws" + e.getMessage());
			e.printStackTrace();
		} finally {
			try{
				if (null != selfVm) {
					selfVm.detach();
				}
			} catch (Exception e){
				System.err.println(Thread.currentThread() + "throws" + e.getMessage());
				e.printStackTrace();
			}
		}
	}
}
