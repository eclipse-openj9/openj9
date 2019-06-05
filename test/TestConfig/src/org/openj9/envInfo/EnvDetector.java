/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package org.openj9.envInfo;

import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.io.IOException;
import java.io.BufferedWriter;

public class EnvDetector {
	static boolean isMachineInfo = false;
	static boolean isJavaInfo = false;

	public static void main(String[] args) {
		parseArgs(args);
	}

	private static void parseArgs(String[] args) {
		for (int i = 0; i < args.length; i++) {
			String option = args[i].toLowerCase();
			if (option.equals("machineinfo")) {
				getMachineInfo();
			} else if (option.equals("javainfo")) {
				getJavaInfo();
			}
		}
	}

	/*
	 * getJavaInfo() is used for AUTO_DETECT
	 */
	private static void getJavaInfo() {
		JavaInfo envDetection = new JavaInfo();
		String SPECInfo = envDetection.getSPEC();
		int javaVersionInfo = envDetection.getJDKVersion();
		String javaImplInfo = envDetection.getJDKImpl();
		if (SPECInfo == null || javaVersionInfo == -1 || javaImplInfo == null) {
			System.exit(1);
		}
		String SPECvalue = "DETECTED_SPEC=" + SPECInfo + "\n";
		String JDKVERSIONvalue = "DETECTED_JDK_VERSION=" + javaVersionInfo + "\n";
		String JDKIMPLvalue = "DETECTED_JDK_IMPL=" + javaImplInfo + "\n";

		/**
		 * autoGenEnv.mk file will be created to store auto detected java info.
		 */
		BufferedWriter output = null;
		try {
			output = new BufferedWriter(new OutputStreamWriter(new FileOutputStream("../autoGenEnv.mk")));
			output.write("########################################################\n");
			output.write("# This is an auto generated file. Please do NOT modify!\n");
			output.write("########################################################\n");
			output.write(SPECvalue);
			output.write(JDKVERSIONvalue);
			output.write(JDKIMPLvalue);
			output.close();
		} catch (IOException e) {
			e.printStackTrace();
		}

	}

	private static void getMachineInfo() {
		MachineInfo machineInfo = new MachineInfo();

		machineInfo.getMachineInfo(MachineInfo.UNAME_CMD);
		machineInfo.getMachineInfo(MachineInfo.SYS_ARCH_CMD);
		machineInfo.getMachineInfo(MachineInfo.PROC_ARCH_CMD);
		machineInfo.getMachineInfo(MachineInfo.SYS_OS_CMD);
		machineInfo.getMachineInfo(MachineInfo.CPU_CORES_CMD);
		machineInfo.getMachineInfo(MachineInfo.ULIMIT_CMD);

		machineInfo.getRuntimeInfo();
		machineInfo.getSpaceInfo("");
		System.out.println(machineInfo);
	}
}