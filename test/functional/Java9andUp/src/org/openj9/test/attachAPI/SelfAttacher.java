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
package org.openj9.test.attachAPI;

import java.io.IOException;
import java.util.Properties;

import org.testng.log4testng.Logger;

import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.VirtualMachine;
import com.sun.tools.attach.VirtualMachineDescriptor;

public class SelfAttacher {

	protected static Logger logger = Logger.getLogger(SelfAttacher.class);
	static final int ATTACH_ERROR_CODE = 10;
	static final int ATTACH_NOT_SUPPORTED_CODE = 11;
	static final int ATTACH_SELF_IOEXCEPTION_CODE = 12;
	static final int ATTACH_SELF_API_SUCCEEDED_CODE = 13;
	public static void main(String[] args) {
		/*
		 * This is launched as a child process by the test process.
		 * It uses stderr to communicate its process ID to the test process
		 * and its exit code to indicate the result of the late attach attempt.
		 */
		try {
			long myPid = ProcessHandle.current().pid();
			String myId = Long.toString(myPid);
			System.err.println("myId="+myId);  /* This is required by the parent process */
			boolean found = false;
			for (int i = 0; i <10 && !found; ++i) {
				Thread.sleep(100);
				for (VirtualMachineDescriptor v: VirtualMachine.list()) {
					if (v.id().equals(myId)) {
						found = true;
						break;
					}
				}
			}
			VirtualMachine vm = VirtualMachine.attach(myId);
			Properties props = vm.getSystemProperties();
			props.list(System.out);
		} catch (AttachNotSupportedException e) {
			e.printStackTrace();
			System.exit(ATTACH_NOT_SUPPORTED_CODE);
		} catch (IOException e) {
			e.printStackTrace();
			System.exit(ATTACH_SELF_IOEXCEPTION_CODE);
		} catch (InterruptedException e) {
			e.printStackTrace();
			System.exit(ATTACH_ERROR_CODE);
		}
		System.exit(ATTACH_SELF_API_SUCCEEDED_CODE);
	}

}
