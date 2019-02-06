/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.Properties;


public class TargetVM {

	public static final String WAITING_FOR_INITIALIZATION = "STATUS_WAIT_INITIALIZATION"; //$NON-NLS-1$

	/**
	 * @param args
	 */
	public static void main(String[] args) {

		BufferedReader inRdr = new BufferedReader(new InputStreamReader(
				System.in));
		System.err.println("starting");
		long pid = 0;
		try {
			pid = TargetManager.getProcessId();
			System.out.println(TargetManager.PID_PREAMBLE + pid);
			if (TargetManager.waitForAttachApiInitialization()) {
				System.out.println(TargetManager.VMID_PREAMBLE + TargetManager.getVmId());
				System.out.println(TargetManager.STATUS_PREAMBLE
						+ TargetManager.STATUS_INIT_SUCESS);
				System.out.flush();
			} else {
				System.out.println(TargetManager.STATUS_PREAMBLE
						+ TargetManager.STATUS_INIT_FAIL);
				System.out.flush();
				System.out.println("failed");
				System.err.println("initialization failed");
				Thread.sleep(10000); /*
									 * give the launching process a chance to
									 * connect
									 */
				return;
			}
			Properties p = new Properties();
			p.setProperty("j9vm.test.attach.testproperty2", "5678def");
			String cmd = "";
			cmd = waitForTermination(inRdr, pid, cmd);
			if (null == cmd) {
				System.out.println(pid + " stdin closed unexpectedly");
			} else {
				System.out.println(pid + " terminated by \"" + cmd + "\"");
			}
		} catch (Throwable e) {
			System.out.println(pid + " failed");
			System.err.println(pid + " failed");
			System.err.println("Throwable: " + e.getMessage());
			e.printStackTrace(System.err);
		} finally {
			System.out.flush();
			System.err.flush();
			System.out.println("terminated");
			System.out.close();
			System.err.close();
		}
	}

	private static String waitForTermination(BufferedReader inRdr, long pid,
			String cmd) throws IOException {
		while ((null != cmd) && !cmd.contains(TargetManager.TARGETVM_STOP)) {
			System.out.println(pid + " received \"" + cmd + "\"");
			System.out.flush();
			cmd = inRdr.readLine();
		}
		return cmd;
	}

}
