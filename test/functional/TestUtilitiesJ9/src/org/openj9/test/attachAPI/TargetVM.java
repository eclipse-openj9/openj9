/*
 * Copyright IBM Corp. and others 2001
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package org.openj9.test.attachAPI;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.Properties;
import org.testng.log4testng.Logger;

public class TargetVM {

	public static final String WAITING_FOR_INITIALIZATION = "STATUS_WAIT_INITIALIZATION"; //$NON-NLS-1$
	private static final String propertyKey = "j9vm.test.attach.testproperty2"; //$NON-NLS-1$
	private static final String propertyValue = "5678def"; //$NON-NLS-1$
	private static final Logger logger = Logger.getLogger(TargetVM.class);

	/**
	 * @param args
	 */
	public static void main(String[] args) {

		/* create a dummy Properties object for testing the heap scanner. */
		Properties p = new Properties();
		p.setProperty(propertyKey, propertyValue);
		/* use the object so the code cannot be reordered */
		String logMsg;
		if (!propertyValue.equals(p.getProperty(propertyKey))) {
			logMsg = "This should never happen";
			System.err.println(logMsg); //$NON-NLS-1$
			logger.error(logMsg);
		}
		BufferedReader inRdr = new BufferedReader(new InputStreamReader(
				System.in));
		logMsg = new java.util.Date() + " : TargetVM starting";
		System.err.println(logMsg);
		logger.info(logMsg);
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
				logger.error("TargetVM initialization failed, exit before sleep 10s");
				Thread.sleep(10000); /*
									 * give the launching process a chance to
									 * connect
									 */
				return;
			}
			logMsg = "before waitForTermination()";
			System.out.println(logMsg);
			logger.info(logMsg);
			String cmd = "";
			cmd = waitForTermination(inRdr, pid, cmd);
			if (null == cmd) {
				logMsg = pid + " stdin closed unexpectedly";
			} else {
				logMsg = new java.util.Date() + " : " + pid + " terminated by \"" + cmd + "\"";
			}
			System.out.println(logMsg);
			logger.info(logMsg);
			/* force a reference to keep the Properties object alive */
			if (!propertyValue.equals(p.getProperty(propertyKey))) {
				logMsg = "This should never happen";
				System.err.println(logMsg); //$NON-NLS-1$
				logger.error(logMsg);
			}
		} catch (Throwable e) {
			logMsg = pid + " failed";
			logger.error(logMsg);
			System.out.println(logMsg);
			System.err.println(logMsg);
			logMsg = "Throwable: " + e.getMessage();
			System.err.println(logMsg);
			logger.error(logMsg);
			e.printStackTrace(System.err);
		} finally {
			System.out.flush();
			System.err.flush();
			logMsg = new java.util.Date() + " : terminated from System.out";
			System.out.println(logMsg);
			logger.info(logMsg);
			System.out.flush();
			System.out.close();
			logMsg = new java.util.Date() + " : terminated from System.err";
			logger.error(logMsg);
			System.err.println(logMsg);
			System.err.flush();
			System.err.close();
		}
	}

	private static String waitForTermination(BufferedReader inRdr, long pid,
			String cmd) throws IOException {
		while ((null != cmd) && !cmd.contains(TargetManager.TARGETVM_STOP)) {
			String logMsg = new java.util.Date() + " : waitForTermination: " + pid + " received \"" + cmd + "\"";
			System.out.println(logMsg);
			System.out.flush();
			logger.info(logMsg);
			cmd = inRdr.readLine();
		}
		return cmd;
	}

}
